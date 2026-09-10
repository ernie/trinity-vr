#include "vr_render_loop.h"

#include <string.h>
#include <math.h>

#include "vr_macros.h"
#include "vr_clientinfo.h"
#include "../vrvk/vr_vk_types.h"
#include "common/xr_linear.h"

extern cvar_t *vr_frameTimingLog;
extern cvar_t *vr_layerSourceAlpha;

// Diagnostic only: logs XR frame pacing (shouldRender transitions and
// predictedDisplayTime deltas) to the console when vr_frameTimingLog is
// nonzero. Never touches frame submission. No-op (aside from re-arming
// its own priming state) when disabled.
static void VR_LogFrameTiming( const XrFrameState *fs, qboolean enabled )
{
	static qboolean primed = qfalse;
	static XrBool32 lastShouldRender = 0;
	static XrTime lastDisplayTime = 0;
	static XrTime windowAccumTime = 0;
	static XrTime windowMaxDelta = 0;
	static XrDuration windowPeriod = 0;
	static int windowFrameCount = 0;
	static int windowLongCount = 0;
	XrTime delta;

	if ( !enabled )
	{
		// Re-arm so the next enable primes cleanly instead of logging a
		// spurious delta spanning the disabled interval.
		primed = qfalse;
		return;
	}

	if ( !primed )
	{
		primed = qtrue;
		lastShouldRender = fs->shouldRender;
		lastDisplayTime = fs->predictedDisplayTime;
		windowPeriod = fs->predictedDisplayPeriod;
		windowAccumTime = 0;
		windowMaxDelta = 0;
		windowFrameCount = 0;
		windowLongCount = 0;
		return;
	}

	if ( fs->shouldRender != lastShouldRender )
	{
		Com_Printf( "VR timing: shouldRender -> %d\n", (int)fs->shouldRender );
		lastShouldRender = fs->shouldRender;
	}

	delta = fs->predictedDisplayTime - lastDisplayTime;
	lastDisplayTime = fs->predictedDisplayTime;
	windowPeriod = fs->predictedDisplayPeriod;

	windowAccumTime += delta;
	windowFrameCount++;
	if ( delta > windowMaxDelta )
	{
		windowMaxDelta = delta;
	}
	if ( windowPeriod > 0 && delta > ( windowPeriod + windowPeriod / 2 ) )
	{
		windowLongCount++;
	}

	if ( windowAccumTime >= 1000000000LL )
	{
		Com_Printf( "VR timing: %d frames, period %.2fms, avg %.2fms, max %.2fms, long(>1.5x) %d\n",
			windowFrameCount,
			(double)windowPeriod / 1000000.0,
			( (double)windowAccumTime / (double)windowFrameCount ) / 1000000.0,
			(double)windowMaxDelta / 1000000.0,
			windowLongCount );

		windowAccumTime = 0;
		windowMaxDelta = 0;
		windowFrameCount = 0;
		windowLongCount = 0;
	}
}

XrFrameState VR_WaitFrame(XrSession session)
{
	XrFrameWaitInfo waitFrameInfo = {};
	waitFrameInfo.type = XR_TYPE_FRAME_WAIT_INFO;
	waitFrameInfo.next = NULL;

	XrFrameState frameState = {};
	frameState.type = XR_TYPE_FRAME_STATE;
	frameState.next = NULL;

	XR_CHECK(
		xrWaitFrame(session, &waitFrameInfo, &frameState),
		"Failed to wait for XR frame");

	VR_LogFrameTiming( &frameState, vr_frameTimingLog->integer != 0 );

	return frameState;
}

void VR_BeginFrame(XrSession session)
{
	XrFrameBeginInfo beginFrameDesc = {};
	beginFrameDesc.type = XR_TYPE_FRAME_BEGIN_INFO;
	beginFrameDesc.next = NULL;
	XR_CHECK(
		xrBeginFrame(session, &beginFrameDesc),
		"Failed to begin XR frame");
}

XrViewState VR_LocateViews(XrSession session, XrTime predictedDisplayTime, XrSpace space, XrView* views, uint32_t* viewCount)
{
	XrViewLocateInfo projectionInfo = {};
	projectionInfo.type = XR_TYPE_VIEW_LOCATE_INFO;
	projectionInfo.next = NULL;
	projectionInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
	projectionInfo.displayTime = predictedDisplayTime;
	projectionInfo.space = space;

	XrViewState viewState = {0};
	viewState.type = XR_TYPE_VIEW_STATE;
	viewState.next = NULL;

	views[0].type = views[1].type = XR_TYPE_VIEW;
	views[0].next = views[1].next = NULL;

	XR_CHECK(
		xrLocateViews(
			session,
			&projectionInfo,
			&viewState,
			*viewCount,
			viewCount,
			views),
		"Failed to locate XR views");
	
	return viewState;
}

void VR_EndFrame(XrSession session, VR_SwapchainInfos* swapchains, XrView* views, uint32_t viewCount, XrSpace worldSpace, XrSpace viewSpace, XrTime predictedDisplayTime)
{
	extern vr_clientinfo_t vr;

	const XrSwapchain colorSwapchain = swapchains->color.swapchain;
	const int colorWidth = (int)swapchains->color.width;
	const int colorHeight = (int)swapchains->color.height;

	// Scoped: submit only a head-locked quad sampling the cyclopean texture.
	// Quad layers carry a single pose (no per-view geometry for SteamVR to
	// override per-eye) so the crosshair lands on the same world ray for both
	// eyes even when system overlays force the compositor into reprojection.
	if (vr.weapon_zoomed)
	{
		XrCompositionLayerQuad quad_layer = {};
		quad_layer.type = XR_TYPE_COMPOSITION_LAYER_QUAD;
		quad_layer.layerFlags = XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT;
		quad_layer.space = viewSpace;
		quad_layer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;

		quad_layer.subImage.swapchain = colorSwapchain;
		quad_layer.subImage.imageRect.extent.width = colorWidth;
		quad_layer.subImage.imageRect.extent.height = colorHeight;
		quad_layer.subImage.imageArrayIndex = 0;  // both array layers carry identical cyclopean pixels

		quad_layer.pose.orientation.w = 1.0f;
		quad_layer.pose.position.z = -1.0f;

		// Aspect-match to texture so reticle stays circular.
		quad_layer.size.height = 2.0f;
		quad_layer.size.width = 2.0f * (float)colorWidth / (float)colorHeight;

		const XrCompositionLayerBaseHeader* layers[1] = {
			(const XrCompositionLayerBaseHeader*)&quad_layer,
		};

		XrFrameEndInfo endFrameInfo = {};
		endFrameInfo.type = XR_TYPE_FRAME_END_INFO;
		endFrameInfo.displayTime = predictedDisplayTime;
		endFrameInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
		endFrameInfo.layerCount = 1;
		endFrameInfo.layers = layers;

		XR_CHECK(xrEndFrame(session, &endFrameInfo), "Failed to end XR frame");
		return;
	}

	XrCompositionLayerProjectionView projection_layer_elements[2] = {};
	for (uint32_t view = 0; view < viewCount; view++)
	{
		projection_layer_elements[view].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
		projection_layer_elements[view].pose = views[view].pose;
		projection_layer_elements[view].fov = views[view].fov;
		projection_layer_elements[view].subImage.swapchain = colorSwapchain;
		projection_layer_elements[view].subImage.imageRect.extent.width = colorWidth;
		projection_layer_elements[view].subImage.imageRect.extent.height = colorHeight;
		projection_layer_elements[view].subImage.imageArrayIndex = view;
	}

	XrCompositionLayerProjection projection_layer = {};
	projection_layer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
	projection_layer.layerFlags = XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT;
	if ( vr_layerSourceAlpha->integer )
		projection_layer.layerFlags |= XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
	projection_layer.space = worldSpace;
	projection_layer.viewCount = viewCount;
	projection_layer.views = projection_layer_elements;

	const XrCompositionLayerBaseHeader* layers[1];
	int layerCount = 0;
	if (viewCount > 0)
	{
		layers[layerCount++] = (const XrCompositionLayerBaseHeader*)&projection_layer;
	}

	XrFrameEndInfo endFrameInfo = {};
	endFrameInfo.type = XR_TYPE_FRAME_END_INFO;
	endFrameInfo.displayTime = predictedDisplayTime;
	endFrameInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
	endFrameInfo.layerCount = layerCount;
	endFrameInfo.layers = layerCount > 0 ? layers : NULL;

	XR_CHECK(xrEndFrame(session, &endFrameInfo), "Failed to end XR frame");
}
