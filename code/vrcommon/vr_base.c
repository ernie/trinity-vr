#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include "vr_base.h"
#include "vr_clientinfo.h"
#include "vr_shared.h"
#include "vr_debug.h"

#include "vr_bhaptics.h"
#include "vr_debug.h"
#include "vr_instance.h"
#include "vr_macros.h"
#include "vr_session.h"
#include "../vrcommon/vr_graphics.h"

#if __ANDROID__
#include <assert.h>
#include <unistd.h>
#endif

static VR_Engine vr_engine;
vr_clientinfo_t vr;

qboolean vr_initialized = qfalse;
qboolean vr_shutdown = qfalse;

// Required extensions first, optional extensions only if the runtime advertises them.
#define MAX_REQUIRED_EXTENSIONS 8
static const char* requiredExtensionNames[MAX_REQUIRED_EXTENSIONS];
static uint32_t numRequiredExtensions = 0;

VR_Bool VR_HasInstanceExtension(const char* name)
{
	uint32_t count = 0;
	if (xrEnumerateInstanceExtensionProperties(NULL, 0, &count, NULL) != XR_SUCCESS || count == 0) {
		return VR_FALSE;
	}

	XrExtensionProperties* props = (XrExtensionProperties*)malloc(sizeof(XrExtensionProperties) * count);
	if (!props) {
		return VR_FALSE;
	}
	for (uint32_t i = 0; i < count; ++i) {
		props[i].type = XR_TYPE_EXTENSION_PROPERTIES;
		props[i].next = NULL;
	}

	VR_Bool found = VR_FALSE;
	if (xrEnumerateInstanceExtensionProperties(NULL, count, &count, props) == XR_SUCCESS) {
		for (uint32_t i = 0; i < count; ++i) {
			if (strcmp(props[i].extensionName, name) == 0) {
				found = VR_TRUE;
				break;
			}
		}
	}

	free(props);
	return found;
}

static void VR_BuildExtensionList(void)
{
	numRequiredExtensions = 0;

	if ( VR_HasInstanceExtension( XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME ) )
		requiredExtensionNames[numRequiredExtensions++] = XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME;

	requiredExtensionNames[numRequiredExtensions++] = XR_EXT_DEBUG_UTILS_EXTENSION_NAME;
	requiredExtensionNames[numRequiredExtensions++] = XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME;

	// Optional extensions, instance-level, so requested here before the renderer loads
	if ( numRequiredExtensions < MAX_REQUIRED_EXTENSIONS &&
		VR_HasInstanceExtension( "XR_KHR_vulkan_swapchain_format_list" ) )
		requiredExtensionNames[numRequiredExtensions++] = "XR_KHR_vulkan_swapchain_format_list";
	if ( numRequiredExtensions < MAX_REQUIRED_EXTENSIONS &&
		VR_HasInstanceExtension( "XR_FB_color_space" ) )
		requiredExtensionNames[numRequiredExtensions++] = "XR_FB_color_space";
}

// Part of init
void VR_InitInstanceInput( VR_Engine* );

static qboolean vr_graphicsInitialized = qfalse;

// Deferred until the renderer DLL's GetRefAPI pulls the Vulkan device;
// idempotent because VR_EnterVR also calls it.
void VR_EnsureGraphicsInitialized( void )
{
	if ( vr_graphicsInitialized || vr_engine.appState.Instance == XR_NULL_HANDLE )
		return;

	XR_CHECK(
		VR_Graphics_GetRequirements( vr_engine.appState.Instance, vr_engine.appState.SystemId ),
		"Failed to get graphics requirements" );

	VR_Graphics_PrintRequirements();

	// Creates the VkInstance and VkDevice through xrCreateVulkanInstanceKHR and xrCreateVulkanDeviceKHR
	VR_Graphics_Init( vr_engine.appState.Instance, vr_engine.appState.SystemId );

	vr_graphicsInitialized = qtrue;
}

VR_Engine* VR_Init( void )
{
	if (vr_initialized || vr_shutdown)
	{
		return &vr_engine;
	}

	memset(&vr_engine, 0, sizeof(vr_engine));
	memset(&vr, 0, sizeof(vr));

	vr.follow_mode = VRFM_THIRDPERSON_1;

	// Build extension list with appropriate graphics API extension
	VR_BuildExtensionList();

	fprintf(stderr, "[OpenXR] Initializing OpenXR instance and system...\n");

	const qboolean listApLayersExtensions = qfalse;
	if (listApLayersExtensions)
	{
		VR_ListAPILayers();
		VR_ListExtensions();
	}

	// Create the OpenXR instance.
	const char* appName = "Quake 3 Arena";
	const XrVersion apiVersion = XR_MAKE_VERSION(1, 0, 0);
	XR_CHECK(
		VR_CreateInstance(appName, apiVersion, numRequiredExtensions, requiredExtensionNames, &vr_engine.appState.Instance), 
		"Failed to create OpenXR instance");

	XrInstanceProperties instanceInfo;
	instanceInfo.type = XR_TYPE_INSTANCE_PROPERTIES;
	instanceInfo.next = NULL;
	XR_CHECK(xrGetInstanceProperties(vr_engine.appState.Instance, &instanceInfo), "Failed to query OpenXR instance properties");
	fprintf(stdout, "[OpenXR] Runtime: %s | Version: %u.%u.%u\n",
		instanceInfo.runtimeName,
		XR_VERSION_MAJOR(instanceInfo.runtimeVersion),
		XR_VERSION_MINOR(instanceInfo.runtimeVersion),
		XR_VERSION_PATCH(instanceInfo.runtimeVersion));

	VR_CreateDebugUtilsMessenger(vr_engine.appState.Instance, &vr_engine.appState.DebugUtilsMessenger);

	XR_CHECK(
		VR_GetHMDSystem(vr_engine.appState.Instance, &vr_engine.appState.SystemId), 
		"Failed to get OpenXR system ID");


	VR_GetSystemProperties(vr_engine.appState.Instance, vr_engine.appState.SystemId, &vr_engine.systemProperties);

	fprintf(stderr,
		"[OpenXR] system properties:\n"
		"  System name: %s\n"
		"  Tracking: {position: %s, orientation: %s}\n"
		"  Graphics: {maxLayerCount: %d, maxSwapchainResolution: %dx%d}\n",
		vr_engine.systemProperties.SystemProperties.systemName,
		vr_engine.systemProperties.SystemProperties.trackingProperties.positionTracking ? "yes" : "no",
		vr_engine.systemProperties.SystemProperties.trackingProperties.orientationTracking ? "yes" : "no",
		vr_engine.systemProperties.SystemProperties.graphicsProperties.maxLayerCount,
		vr_engine.systemProperties.SystemProperties.graphicsProperties.maxSwapchainImageWidth,
		vr_engine.systemProperties.SystemProperties.graphicsProperties.maxSwapchainImageHeight);

	// We're done
	fprintf(stderr, 
		"[OpenXR] Instance and system succesfully initialized:\n"
		"  - Instance: %p\n"
		"  - System: %llu\n\n",
		vr_engine.appState.Instance,
		vr_engine.appState.SystemId);

	vr_initialized = qtrue;
	VR_InitInstanceInput(&vr_engine);

	return &vr_engine;
}

VR_Engine* VR_GetEngine( void )
{
	return &vr_engine;
}

void VR_Destroy( VR_Engine* engine )
{
	if (engine == &vr_engine)
	{
#ifdef USE_BHAPTICS
		VR_Bhaptics_Shutdown();
#endif
		// Invalidate XR function pointers before destroying XrInstance: they were
		// obtained via xrGetInstanceProcAddr and become invalid after xrDestroyInstance.
		// Note: We do NOT call VR_Graphics_Shutdown() here because the renderer still
		// needs the VkDevice/VkInstance. The renderer will destroy them in vk_shutdown().
		VR_Graphics_InvalidateFunctionPointers();

		VR_DestroyDebugUtilsMessenger(engine->appState.Instance, &engine->appState.DebugUtilsMessenger);
		xrDestroyInstance(engine->appState.Instance);
		memset(&vr_engine, 0, sizeof(vr_engine));
		vr_graphicsInitialized = qfalse;
	}
	vr_initialized = qfalse;
}

void VR_PrepareForShutdown( void )
{
	vr_shutdown = qtrue;
}

void VR_EnterVR( VR_Engine* engine )
{
	VR_EnsureGraphicsInitialized();

	if (engine->appState.Session)
	{
		fprintf(stderr, "VR_EnterVR called with existing session");
		return;
	}

	fprintf(stderr, "[OpenXR] Creating XR session and reference space\n");

	// Create the OpenXR Session.
	XR_CHECK(
		VR_CreateSession(engine->appState.Instance, engine->appState.SystemId, &engine->appState.Session),
		"Failed to create XR session");

	// Create a space to the first path
	XrReferenceSpaceCreateInfo spaceCreateInfo = {};
	spaceCreateInfo.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
	spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
	spaceCreateInfo.poseInReferenceSpace.orientation.w = 1.0f;
	XR_CHECK(
		xrCreateReferenceSpace(engine->appState.Session, &spaceCreateInfo, &engine->appState.HeadSpace),
		"Failed to create reference space (HEAD/VIEW)");

	fprintf(stderr, "[OpenXR] XR session and reference space created\n\n");
}

void VR_LeaveVR( VR_Engine* engine )
{
	if (engine->appState.Session) 
	{
		fprintf(stderr, "[OpenXR] Destroying XR session and reference spaces\n");

		XR_CHECK(
			xrDestroySpace(engine->appState.HeadSpace),
			"Failed to destroy reference space (HEAD/VIEW)");
		engine->appState.HeadSpace = XR_NULL_HANDLE;

		// StageSpace is optional.
		if (engine->appState.StageSpace != XR_NULL_HANDLE)
		{
			XR_CHECK(
				xrDestroySpace(engine->appState.StageSpace),
				"Failed to destroy reference space (STAGE)");
			engine->appState.StageSpace = XR_NULL_HANDLE;
		}
		XR_CHECK(
			xrDestroySpace(engine->appState.FakeStageSpace),
			"Failed to destroy reference space (FAKE STAGE)");
		engine->appState.FakeStageSpace = XR_NULL_HANDLE;
		engine->appState.CurrentSpace = XR_NULL_HANDLE;

		XR_CHECK(
			xrDestroySession(engine->appState.Session),
			"Failed to destroy XR session");
		engine->appState.Session = NULL;

		engine->appState.SessionActive = VR_FALSE;
		engine->appState.Visible = VR_FALSE;
		engine->appState.Focused = VR_FALSE;

		fprintf(stderr, "[OpenXR] XR session and reference spaces destroyed\n");
	}
}

//#endif
