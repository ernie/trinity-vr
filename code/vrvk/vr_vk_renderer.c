/*
 * vr_vk_renderer.c - Vulkan VR renderer implementation
 *
 * Implements the VR renderer exports for the Vulkan backend.
 * This file handles the OpenXR frame lifecycle and coordinates
 * with the Vulkan command buffer system.
 */

#include "../vrcommon/vr_renderer.h"

#include <math.h>

#include "../client/client.h"

#include "../vrcommon/common/xr_linear.h"
#include "../vrcommon/vr_base.h"
#include "../vrcommon/vr_clientinfo.h"
#include "../vrcommon/vr_events.h"
#include "../vrcommon/vr_gameplay.h"
#include "../vrcommon/vr_input.h"
#include "../vrcommon/vr_macros.h"
#include "../vrcommon/vr_math.h"
#include "../vrcommon/vr_render_loop.h"
#include "../vrcommon/vr_spaces.h"
#include "../vrcommon/vr_swapchains.h"
#include "../vrcommon/vr_types.h"

// Vulkan-specific headers
#include "vr_vk.h"
#include "vr_vk_debug.h"
#include "vr_vk_foveation.h"
#include "vr_vk_swapchains.h"
#include "vr_vk_virtual_screen.h"

extern vr_clientinfo_t vr;
extern cvar_t *vr_heightAdjust;
extern cvar_t *vr_refreshrate;
extern cvar_t *vr_refreshrates;
extern cvar_t *vr_desktopMode;

// This file owns the renderer's VR state; views/viewCount stay external because vr_vk_virtual_screen.c reads them.
static const float hudScale = M_PI * 15.0f / 180.0f;

static XrBool32 stageSupported = XR_FALSE;
static XrTime lastPredictedDisplayTime = 0;
static qboolean needRecenter = qtrue;

// Per-frame data held between BeginFrame and EndFrame
static XrFovf fov = { 0 };
XrView views[2];
uint32_t viewCount = 2;
static uint32_t swapchainColorIndex = 0;

// Forward declarations
static void VR_Renderer_BeginFrame(VR_Engine* engine, XrBool32 needsRecenter);
static void VR_Renderer_EndFrame(VR_Engine* engine);
static void VR_Recenter(VR_Engine* engine, XrTime predictedDisplayTime);
static void VR_ClearFrameBuffer(int width, int height);
static void VR_UpdatePerFrameState(void);

/*
==================
ConvertToReversedDepth

Quake3e/renderervk uses reversed depth (near=1.0, far=0.0) for better depth precision.
OpenXR's XrMatrix4x4f_CreateProjectionFov produces standard Vulkan depth (near=0.0, far=1.0).
This function converts a standard projection matrix to reversed depth.

For an infinite far plane (which OpenXR uses when farZ <= nearZ):
  Standard:  m[10] = -1,     m[14] = -near
  Reversed:  m[10] =  0,     m[14] =  near
==================
*/
static void ConvertToReversedDepth(XrMatrix4x4f* matrix)
{
	// For infinite projection (standard Vulkan):
	//   m[10] = -1.0, m[14] = -nearZ
	// For reversed depth infinite projection:
	//   m[10] = 0.0,  m[14] = nearZ
	//
	// The conversion: m[10] = m[10] + 1.0, m[14] = -m[14]
	matrix->m[10] = matrix->m[10] + 1.0f;  // -1 -> 0
	matrix->m[14] = -matrix->m[14];         // -near -> near
}
static XrDesktopViewConfiguration VR_GetDesktopViewConfiguration(void);

// A frame is begun and not yet ended; RestoreState re-begins it after a mid-frame restart
static qboolean frameStarted = qfalse;


void VR_GetResolution(VR_Engine* engine, int *pWidth, int *pHeight)
{
	VR_GetSupersampledResolution(engine->appState.Instance, engine->appState.SystemId, pWidth, pHeight);
}


// Rate asked of the runtime but not yet confirmed by its change event
static float s_requestedRefreshRate = 0.0f;

/*
==================
VR_EnumerateRefreshRates

Publishes the runtime's rates in vr_refreshrates so the UI offers only those.
==================
*/
static void VR_EnumerateRefreshRates(VR_Engine* engine)
{
	PFN_xrEnumerateDisplayRefreshRatesFB xrEnumerateDisplayRefreshRatesFB = NULL;
	VR_Renderer* renderer = &engine->appState.Renderer;
	char list[256];
	uint32_t count = 0;
	uint32_t i;

	renderer->NumSupportedRefreshRates = 0;
	list[0] = '\0';

	xrGetInstanceProcAddr(engine->appState.Instance, "xrEnumerateDisplayRefreshRatesFB", (PFN_xrVoidFunction*)(&xrEnumerateDisplayRefreshRatesFB));
	if (xrEnumerateDisplayRefreshRatesFB &&
		XR_SUCCEEDED(xrEnumerateDisplayRefreshRatesFB(engine->appState.Session, 0, &count, NULL)) && count > 0)
	{
		if (count > VR_MAX_REFRESH_RATES)
		{
			count = VR_MAX_REFRESH_RATES;
		}
		if (XR_SUCCEEDED(xrEnumerateDisplayRefreshRatesFB(engine->appState.Session, count, &count, renderer->SupportedRefreshRates)))
		{
			renderer->NumSupportedRefreshRates = count;
		}
	}

	for (i = 0; i < renderer->NumSupportedRefreshRates; i++)
	{
		Q_strcat(list, sizeof(list), va("%s%g", i ? " " : "", renderer->SupportedRefreshRates[i]));
	}
	Cvar_Set2("vr_refreshrates", list, qtrue);
	Com_Printf("Supported display refresh rates: %s\n", list[0] ? list : "(not reported)");
}

// Nearest supported rate, or the rate itself when the runtime reported none
static float VR_SnapRefreshRate(const VR_Engine* engine, float rate)
{
	const VR_Renderer* renderer = &engine->appState.Renderer;
	float best = rate;
	float bestDiff = -1.0f;
	uint32_t i;

	for (i = 0; i < renderer->NumSupportedRefreshRates; i++)
	{
		const float diff = fabsf(renderer->SupportedRefreshRates[i] - rate);
		if (bestDiff < 0.0f || diff < bestDiff)
		{
			bestDiff = diff;
			best = renderer->SupportedRefreshRates[i];
		}
	}
	return best;
}

static void VR_RequestRefreshRate(VR_Engine* engine, float rate)
{
	PFN_xrRequestDisplayRefreshRateFB xrRequestDisplayRefreshRateFB;
	XrResult requestResult;

	XR_CHECK(
		xrGetInstanceProcAddr(engine->appState.Instance, "xrRequestDisplayRefreshRateFB", (PFN_xrVoidFunction*)(&xrRequestDisplayRefreshRateFB)),
		"failed to get xrRequestDisplayRefreshRateFB func proc");

	Com_Printf("Requesting display refresh rate: %g\n", rate);
	requestResult = xrRequestDisplayRefreshRateFB(engine->appState.Session, rate);
	if (requestResult == XR_SUCCESS)
	{
		// Confirmed later by XR_TYPE_EVENT_DATA_DISPLAY_REFRESH_RATE_CHANGED_FB
		s_requestedRefreshRate = rate;
	}
	else
	{
		Com_Printf("Refresh rate %g refused by the runtime (%d), staying at %g\n", rate, (int)requestResult, engine->appState.Renderer.RefreshRate);
		Cvar_SetValue("vr_refreshrate", engine->appState.Renderer.RefreshRate);
		vr_refreshrate->modified = qfalse;
		s_requestedRefreshRate = 0.0f;
	}
}

/*
==================
VR_ApplyRefreshRate

Runs at init and on every cvar change, so the setting applies live without a video restart.
==================
*/
static void VR_ApplyRefreshRate(VR_Engine* engine)
{
	const float current = engine->appState.Renderer.RefreshRate;
	float desired, snapped;

	vr_refreshrate->modified = qfalse;

	desired = vr_refreshrate->value;
	if (desired <= 0.0f)
	{
		Cvar_SetValue("vr_refreshrate", current);
		vr_refreshrate->modified = qfalse;
		return;
	}

	snapped = VR_SnapRefreshRate(engine, desired);
	if (snapped != desired)
	{
		Com_Printf("Refresh rate %g is not supported, using %g\n", desired, snapped);
		Cvar_SetValue("vr_refreshrate", snapped);
		vr_refreshrate->modified = qfalse;
	}

	if (snapped == current)
	{
		s_requestedRefreshRate = 0.0f;
		return;
	}
	if (snapped == s_requestedRefreshRate)
	{
		return; // already asked; waiting for the runtime to confirm
	}
	VR_RequestRefreshRate(engine, snapped);
}

void VR_InitRenderer(VR_Engine* engine)
{
	VR_VK_RegisterDebugCallbackIfEnabled();

	// Read the current rate, publish the runtime's list and apply the archived pick
	{
		PFN_xrGetDisplayRefreshRateFB xrGetDisplayRefreshRateFB;
		XR_CHECK(
			xrGetInstanceProcAddr(engine->appState.Instance, "xrGetDisplayRefreshRateFB", (PFN_xrVoidFunction*)(&xrGetDisplayRefreshRateFB)),
			"failed to get xrGetDisplayRefreshRateFB func proc");

		engine->appState.Renderer.RefreshRate = 0.0f;
		XR_CHECK(
			xrGetDisplayRefreshRateFB(engine->appState.Session, &engine->appState.Renderer.RefreshRate),
			"failed to get current display refresh rate");
		printf("Current System Display Refresh Rate: %f\n", engine->appState.Renderer.RefreshRate);

		VR_EnumerateRefreshRates(engine);
		s_requestedRefreshRate = 0.0f;
		VR_ApplyRefreshRate(engine);
	}

	stageSupported = VR_IsStageSpaceSupported(engine->appState.Session);

	if (engine->appState.CurrentSpace == XR_NULL_HANDLE)
	{
		XrTime nullTime = 0; // won't be used anyway
		VR_Recenter(engine, nullTime);
	}

	// Create VIEW reference space for head-locked quad layers
	{
		XrReferenceSpaceCreateInfo viewSpaceCI = {0};
		viewSpaceCI.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
		viewSpaceCI.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
		viewSpaceCI.poseInReferenceSpace.orientation.w = 1.0f;  // Identity
		XR_CHECK(
			xrCreateReferenceSpace(engine->appState.Session, &viewSpaceCI, &engine->appState.ViewSpace),
			"Failed to create VIEW reference space for quad layer");
		printf("Created VIEW reference space for head-locked quad layers\n");
	}

	// Create Vulkan XR swapchains
	engine->appState.Renderer.Swapchains = VR_VK_CreateSwapchains(
		engine->appState.Instance,
		engine->appState.SystemId,
		engine->appState.Session);

	// Initialize renderer XR resources (VkImageViews, VkFramebuffers)
	// Renderer pulls swapchain info via ri.VR_Vulkan_GetSwapchainInfo()
	if (!re.InitXRResources()) {
		printf("[VR Vulkan] Warning: Failed to initialize XR resources\n");
	}

	VRVK_VirtualScreen_Init();
	VR_VirtualScreen_ResetPosition();
}


void VR_DestroyRenderer(VR_Engine* engine)
{
	VRVK_VirtualScreen_Destroy();
	VR_VK_DestroySwapchains(&engine->appState.Renderer.Swapchains);

	// Destroy VIEW reference space
	if (engine->appState.ViewSpace != XR_NULL_HANDLE)
	{
		xrDestroySpace(engine->appState.ViewSpace);
		engine->appState.ViewSpace = XR_NULL_HANDLE;
	}
}


void VR_ProcessFrame(VR_Engine* engine)
{
	const XrBool32 needsRecenter = VR_ProcessXrEvents(&engine->appState);
	if (engine->appState.SessionActive == VR_FALSE)
	{
		// If we haven't called Com_Frame() then let's at least process input
		// (specifically SDL events) so that app won't appear as stuck/deadlocked
		IN_Frame();
		return;
	}

	// A menu pick applies live, no video restart
	if (vr_refreshrate->modified)
	{
		VR_ApplyRefreshRate(engine);
	}

	VR_Renderer_BeginFrame(engine, needsRecenter);
	Com_Frame();
	VR_Renderer_EndFrame(engine);

	if (needRecenter)
	{
		VR_Recenter(engine, lastPredictedDisplayTime);
		needRecenter = qfalse;
	}
}


void VR_Renderer_RestoreState(VR_Engine* engine)
{
	if (!frameStarted)
	{
		// Frame hasn't started, no need to restore anything here
		return;
	}

	VR_UpdatePerFrameState();

	// If we need to re-start frame until `Com_Frame()` call, we need session to
	// be active to proceed
	XrBool32 needsRecenter = XR_FALSE;
	while (!engine->appState.SessionActive)
	{
		needsRecenter |= VR_ProcessXrEvents(&engine->appState);
	}

	VR_Renderer_BeginFrame(engine, needsRecenter);
}


static void VR_Renderer_BeginFrame(VR_Engine* engine, XrBool32 needsRecenter)
{
	frameStarted = qtrue;
	lastPredictedDisplayTime = VR_WaitFrame(engine->appState.Session).predictedDisplayTime;

	if (needsRecenter)
	{
		VR_Recenter(engine, lastPredictedDisplayTime);
	}

	VR_BeginFrame(engine->appState.Session);

	VR_LocateViews(
		engine->appState.Session,
		lastPredictedDisplayTime,
		engine->appState.CurrentSpace,
		views,
		&viewCount);

	// Update HMD position/views
	IN_VRUpdateHMD(views, viewCount, &fov);

	// SP intermission state tracking: must be set before rendering
	// so UI code sees the correct state for scaling/offsets
	qboolean isSPIntermission = VR_IsSPIntermission();
	if (isSPIntermission && !vr.sp_intermission_active)
	{
		// First frame of SP intermission: capture anchor position
		vr.sp_intermission_active = qtrue;
		// Store yaw for HUD positioning (in degrees)
		XrQuaternionf q = views[0].pose.orientation;
		float siny_cosp = 2.0f * (q.w * q.y + q.z * q.x);
		float cosy_cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
		vr.sp_intermission_yaw = atan2f(siny_cosp, cosy_cosp) * 180.0f / (float)M_PI;
	}
	else if (!isSPIntermission && vr.sp_intermission_active)
	{
		// Exiting SP intermission: reset state
		vr.sp_intermission_active = qfalse;
	}

	// [Input] poll actions, update controller state, issue action commands
	IN_VRSyncActions(engine);
	IN_VRUpdateControllers(engine, lastPredictedDisplayTime);

	// Update zoom level after input processing so weapon_zoomLevel
	// matches weapon_zoomed (set during IN_VRUpdateControllers)
	VR_UpdatePerFrameState();

	// Needs this frame's FOV (from IN_VRUpdateHMD above), this frame's synced
	// action state for the gaze space to be locatable (IN_VRSyncActions), and
	// this frame's zoom state (IN_VRUpdateControllers, applied by
	// VR_UpdatePerFrameState just above). lastPredictedDisplayTime is fixed
	// for the whole function, so it doesn't constrain where this call sits.
	VR_VK_Foveation_Frame(engine, lastPredictedDisplayTime);

	VR_SwapchainInfos* swapchains = engine->appState.Renderer.Swapchains;

	// Images are acquired only when something is about to be drawn
	// (VR_Renderer_BeginRender): a frame that draws nothing holds none, and
	// the compositor keeps showing the last released image, not a held one

	// Set renderer params
	// Near plane must be in Quake units to match our view matrices
	float nearPlane = 4.0f;  // Match r_znear default in Quake units

	// Use Vulkan conventions for projection matrix (Y-down, Z in [0,1])
	const GraphicsAPI graphicsApi = GRAPHICS_VULKAN;

	XrMatrix4x4f vrMatrixMono, vrMatrixProjection;
	const XrFovf monoFov = { -hudScale, hudScale, hudScale, -hudScale };
	XrFovf projectionFov;
	if (vr.weapon_zoomed)
	{
		// Scope: the view the quad in VR_EndFrame shows, from the same frustum.
		// Zoom narrows the angle; the height keeps the buffer aspect
		float halfTanH, halfTanV;
		VR_ScopeFrustum(&halfTanH, &halfTanV, swapchains->color.width, swapchains->color.height);
		float tanH = tanf(atanf(halfTanH) / vr.weapon_zoomLevel);
		float tanV = tanH * (float)swapchains->color.height / (float)swapchains->color.width;
		projectionFov.angleLeft = -atanf(tanH);
		projectionFov.angleRight = atanf(tanH);
		projectionFov.angleUp = atanf(tanV);
		projectionFov.angleDown = -atanf(tanV);
	}
	else
	{
		projectionFov.angleLeft = fov.angleLeft / vr.weapon_zoomLevel;
		projectionFov.angleRight = fov.angleRight / vr.weapon_zoomLevel;
		projectionFov.angleUp = fov.angleUp / vr.weapon_zoomLevel;
		projectionFov.angleDown = fov.angleDown / vr.weapon_zoomLevel;
	}
	XrMatrix4x4f_CreateProjectionFov(&vrMatrixMono, graphicsApi, monoFov, nearPlane, 0.0f);
	XrMatrix4x4f_CreateProjectionFov(&vrMatrixProjection, graphicsApi, projectionFov, nearPlane, 0.0f);

	// Create per-eye projection matrices from actual OpenXR FOVs
	XrMatrix4x4f vrMatrixEye[2];
	for (int eye = 0; eye < 2 && eye < (int)viewCount; eye++)
	{
		XrFovf eyeFov = {
			views[eye].fov.angleLeft / vr.weapon_zoomLevel,
			views[eye].fov.angleRight / vr.weapon_zoomLevel,
			views[eye].fov.angleUp / vr.weapon_zoomLevel,
			views[eye].fov.angleDown / vr.weapon_zoomLevel,
		};
		XrMatrix4x4f_CreateProjectionFov(&vrMatrixEye[eye], graphicsApi, eyeFov, nearPlane, 0.0f);
	}

	// Convert to reversed depth (near=1.0, far=0.0) for Quake3e's depth precision
	ConvertToReversedDepth(&vrMatrixMono);
	ConvertToReversedDepth(&vrMatrixProjection);
	ConvertToReversedDepth(&vrMatrixEye[0]);
	ConvertToReversedDepth(&vrMatrixEye[1]);

	// Compute combined stereo horizontal FOV for culling
	float combinedAngleLeft = views[0].fov.angleLeft / vr.weapon_zoomLevel;
	float combinedAngleRight = views[1].fov.angleRight / vr.weapon_zoomLevel;
	float combinedFovX = (fabsf(combinedAngleLeft) + fabsf(combinedAngleRight)) * 180.0f / M_PI;
	// Canted displays: each eye's FOV is centered on its own yawed axis
	combinedFovX += (fabsf(vr.eyeCantYaw[0]) + fabsf(vr.eyeCantYaw[1])) * 180.0f / M_PI;

	if (vr.weapon_zoomed)
	{
		// Cull to whichever is wider, the eyes or the scope's fixed frustum
		float scopeFovX = 2.0f * projectionFov.angleRight * 180.0f / M_PI;
		if (scopeFovX > combinedFovX)
			combinedFovX = scopeFovX;
	}

	// Calculate half-IPD in meters for frustum plane offset
	float halfIpdMeters = 0.0f;
	if (viewCount >= 2) {
		float dx = views[1].pose.position.x - views[0].pose.position.x;
		float dy = views[1].pose.position.y - views[0].pose.position.y;
		float dz = views[1].pose.position.z - views[0].pose.position.z;
		halfIpdMeters = sqrtf(dx*dx + dy*dy + dz*dz) * 0.5f;
	}

	re.SetVRHeadsetParms(vrMatrixProjection.m, vrMatrixMono.m, 0, // renderBuffer not used for VK
						 vrMatrixEye[0].m, vrMatrixEye[1].m, combinedFovX, halfIpdMeters);
}


static void VR_Renderer_EndFrame(VR_Engine* engine)
{
	VR_SwapchainInfos* swapchains = engine->appState.Renderer.Swapchains;

	// Draw Virtual Screen if needed
	const int use_virtual_screen = VR_Gameplay_ShouldRenderInVirtualScreen();
	if (use_virtual_screen)
	{
		// Re-anchor when the client state changes mid-window (e.g. a map
		// load beginning) so the screen appears where the player is facing
		if ( VR_Gameplay_VirtualScreenContextChanged() ) {
			VR_VirtualScreen_ResetPosition();
		}
		if ( !vr.menuYawLocked ) {
			vr.menuYaw = VR_VirtualScreen_GetCurrentYaw();
		}
	}
	else
	{
		VR_VirtualScreen_ResetPosition();
		if ( !vr.menuYawLocked ) {
			vr.menuYaw = vr.hmdorientation[YAW];
		}
	}

	// Only a frame that drew holds an image. The mirror blits from it before
	// the release; after the release it belongs to the runtime
	if (swapchains->color.acquired)
	{
		re.SwapDesktopWindow();
		VR_VK_Swapchains_Release(swapchains);
	}

	VR_EndFrame(
		engine->appState.Session,
		swapchains,
		views,
		viewCount,
		engine->appState.CurrentSpace,
		engine->appState.ViewSpace,
		lastPredictedDisplayTime);

	frameStarted = qfalse;
}


static void VR_Recenter(VR_Engine* engine, XrTime predictedDisplayTime)
{
	// Calculate recenter reference
	XrReferenceSpaceCreateInfo spaceCreateInfo = {0};
	spaceCreateInfo.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
	spaceCreateInfo.poseInReferenceSpace.orientation.w = 1.0f;
	if (engine->appState.CurrentSpace != XR_NULL_HANDLE)
	{
		vec3_t rotation = {0, 0, 0};
		XrSpaceLocation loc = {0};
		loc.type = XR_TYPE_SPACE_LOCATION;
		XR_CHECK(
			xrLocateSpace(engine->appState.HeadSpace, engine->appState.CurrentSpace, predictedDisplayTime, &loc),
			"Failed to locate space");
		QuatToYawPitchRoll(loc.pose.orientation, rotation, vr.hmdorientation);

		vr.recenterYaw += DegreesToRadians(vr.hmdorientation[YAW]);
		spaceCreateInfo.poseInReferenceSpace.orientation.x = 0;
		spaceCreateInfo.poseInReferenceSpace.orientation.y = sin(vr.recenterYaw / 2);
		spaceCreateInfo.poseInReferenceSpace.orientation.z = 0;
		spaceCreateInfo.poseInReferenceSpace.orientation.w = cos(vr.recenterYaw / 2);
	}

	// Delete previous space instances
	if (engine->appState.StageSpace != XR_NULL_HANDLE)
	{
		XR_CHECK(
			xrDestroySpace(engine->appState.StageSpace),
			"Failed to destroy stage space");
	}
	if (engine->appState.FakeStageSpace != XR_NULL_HANDLE)
	{
		XR_CHECK(
			xrDestroySpace(engine->appState.FakeStageSpace),
			"Failed to destroy fake stage space");
	}

	// Create a default stage space to use if SPACE_TYPE_STAGE is not
	// supported, or calls to xrGetReferenceSpaceBoundsRect fail.
	spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
	spaceCreateInfo.poseInReferenceSpace.position.y = -1.6750f;
	XR_CHECK(
		xrCreateReferenceSpace(engine->appState.Session, &spaceCreateInfo, &engine->appState.FakeStageSpace),
		"Failed to create reference space (fake stage)");
	printf("Created fake stage space from local space with offset\n");
	engine->appState.CurrentSpace = engine->appState.FakeStageSpace;

	if (stageSupported)
	{
		spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
		spaceCreateInfo.poseInReferenceSpace.position.y = 0.0f;
		XR_CHECK(
			xrCreateReferenceSpace(engine->appState.Session, &spaceCreateInfo, &engine->appState.StageSpace),
			"Failed to create reference space (stage)");
		printf("Created stage space\n");
		engine->appState.CurrentSpace = engine->appState.StageSpace;
	}

	// Update menu orientation
	vr.menuYaw = 0;

	// Reset VirtualScreen's position
	VR_VirtualScreen_ResetPosition();
}


static void VR_ClearFrameBuffer(int width, int height)
{
	// Delegate to renderer: avoids direct graphics API calls in VR layer
	qboolean isThirdPersonSpectator = Cvar_VariableIntegerValue("vr_thirdPersonSpectator") ? qtrue : qfalse;
	re.ClearVRFramebuffer(width, height, isThirdPersonSpectator);
}


static void VR_UpdatePerFrameState(void)
{
	if (vr.weapon_zoomed)
	{
		vr.weapon_zoomLevel += 0.05f;
		if (vr.weapon_zoomLevel > 2.5f)
			vr.weapon_zoomLevel = 2.5f;
	}
	else
	{
		// Zoom back out quicker
		vr.weapon_zoomLevel -= 0.25f;
		if (vr.weapon_zoomLevel < 1.0f)
			vr.weapon_zoomLevel = 1.0f;
	}
}


static XrDesktopViewConfiguration VR_GetDesktopViewConfiguration(void)
{
	switch (vr_desktopMode->integer)
	{
		case 0:
			return LEFT_EYE;
		case 1:
			return RIGHT_EYE;
		case 2:
			return BOTH_EYES;
	}
	return LEFT_EYE;
}


/*
==================
VR_Renderer_BeginRender

About to draw: acquire this frame's image and begin the renderer's XR frame.
Called from the screen update rather than the frame begin so a frame that
draws nothing holds no image; the compositor then keeps showing the image
last released instead of a held one with the ring's stale contents.
==================
*/
void VR_Renderer_BeginRender(VR_Engine* engine)
{
	VR_SwapchainInfos* swapchains = engine ? engine->appState.Renderer.Swapchains : NULL;

	if (!frameStarted || !swapchains || swapchains->color.acquired)
	{
		return;
	}

	// Acquire XR color swapchain (depth is native Vulkan buffer, not from OpenXR)
	VR_VK_Swapchains_Acquire(swapchains, &swapchainColorIndex);

	// Begin XR rendering: sets up Vulkan command buffer and binds XR framebuffers
	re.BeginXRFrame(swapchainColorIndex);

	// Clear framebuffer
	VR_ClearFrameBuffer(swapchains->color.width, swapchains->color.height);
}


qboolean VR_Renderer_SubmitLoadingFrame(VR_Engine* engine)
{
	// Only submit frames during loading states when a frame has been started.
	// engine is unguarded past this point (VR_Renderer_EndFrame/BeginFrame both
	// dereference it directly), so check it here the way VR_Renderer_BeginRender
	// checks it before its own dereference.
	if (!engine || (clc.state != CA_LOADING && clc.state != CA_PRIMED) || !frameStarted)
	{
		return qfalse;
	}

	// End the current VR frame (this will blit to virtual screen and submit to XR)
	VR_Renderer_EndFrame(engine);

	// Start a new VR frame for the next screen update
	// This is needed because SCR_UpdateScreen will be called again during loading,
	// and it needs a valid XR frame to render into
	VR_Renderer_BeginFrame(engine, XR_FALSE);

	return qtrue;
}
