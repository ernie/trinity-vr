#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include "vr_base.h"
#include "vr_clientinfo.h"
#include "vr_shared.h"
#include "vr_debug.h"
#include "../vrvk/vr_vk.h"

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

// Instance extensions the runtime advertises, enumerated once per VR_Init
static XrExtensionProperties* s_instanceExtensions = NULL;
static uint32_t s_numInstanceExtensions = 0;

// Name and version from VR_Init's xrGetInstanceProperties, kept so callers can
// name the runtime without asking a second time. Empty before VR_Init runs.
static char s_runtimeDescription[XR_MAX_RUNTIME_NAME_SIZE + 32] = "";

// The XrVersion VR_Init declares to xrCreateInstance, kept so callers can
// print the same value instance creation used. Empty before VR_Init runs.
static char s_declaredApiVersion[32] = "";

static void VR_LogLine(const char* line)
{
	fprintf(stderr, "[OpenXR] %s\n", line);
}

// Logged as one block so a runtime's capabilities can be read off a single capture
static void VR_EnumerateInstanceExtensions(void)
{
	uint32_t count = 0;
	uint32_t i;
	char line[256];

	free(s_instanceExtensions);
	s_instanceExtensions = NULL;
	s_numInstanceExtensions = 0;

	if (xrEnumerateInstanceExtensionProperties(NULL, 0, &count, NULL) != XR_SUCCESS || count == 0) {
		VR_LogLine("instance extensions: none enumerated");
		return;
	}

	s_instanceExtensions = (XrExtensionProperties*)malloc(sizeof(XrExtensionProperties) * count);
	if (!s_instanceExtensions) {
		VR_LogLine("instance extensions: allocation failed");
		return;
	}
	for (i = 0; i < count; ++i) {
		s_instanceExtensions[i].type = XR_TYPE_EXTENSION_PROPERTIES;
		s_instanceExtensions[i].next = NULL;
	}
	if (xrEnumerateInstanceExtensionProperties(NULL, count, &count, s_instanceExtensions) != XR_SUCCESS) {
		free(s_instanceExtensions);
		s_instanceExtensions = NULL;
		VR_LogLine("instance extensions: enumeration failed");
		return;
	}
	s_numInstanceExtensions = count;

	Com_sprintf(line, sizeof(line), "instance extensions (%u):", count);
	VR_LogLine(line);
	for (i = 0; i < count; ++i) {
		Com_sprintf(line, sizeof(line), "  %s (v%u)",
			s_instanceExtensions[i].extensionName, s_instanceExtensions[i].extensionVersion);
		VR_LogLine(line);
	}
}

VR_Bool VR_HasInstanceExtension(const char* name)
{
	uint32_t i;
	for (i = 0; i < s_numInstanceExtensions; ++i) {
		if (strcmp(s_instanceExtensions[i].extensionName, name) == 0) {
			return VR_TRUE;
		}
	}
	return VR_FALSE;
}

// VR_HasInstanceExtension answers what the runtime advertises; this answers
// what was enabled. The list goes to xrCreateInstance unaltered, under a check
// with no retry and no fallback, so a running process is proof that every name
// still in it was accepted.
VR_Bool VR_HasEnabledInstanceExtension(const char* name)
{
	uint32_t i;
	for (i = 0; i < numRequiredExtensions; ++i) {
		if (strcmp(requiredExtensionNames[i], name) == 0) {
			return VR_TRUE;
		}
	}
	return VR_FALSE;
}

const char* VR_GetRuntimeDescription(void)
{
	return s_runtimeDescription;
}

const char* VR_GetDeclaredApiVersion(void)
{
	return s_declaredApiVersion;
}

const char* VR_FoveationCapsString(void)
{
	const VR_VulkanDeviceInfo* info = VR_Vulkan_GetDeviceInfo();
	if (info && info->shadingRateSupported) {
		return "fixed";
	}
	return "none";
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

	// PICO's native controller profiles; a strict runtime rejects their paths unless enabled
	if ( numRequiredExtensions < MAX_REQUIRED_EXTENSIONS &&
		VR_HasInstanceExtension( "XR_BD_controller_interaction" ) )
		requiredExtensionNames[numRequiredExtensions++] = "XR_BD_controller_interaction";
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

	// vr_foveationCaps is registered CVAR_ROM "none" in VR_InitCvars, which
	// runs from CL_Init well before this function's first successful call
	// (from CL_InitRef) creates the Vulkan device -- VR_FoveationCapsString
	// has nothing to read that early. Refresh it here instead of folding it
	// back into VR_InitCvars: VR_FoveationCapsString reports "none" on its
	// own if VR_Graphics_Init just failed, so this is correct either way.
	Cvar_Set2( "vr_foveationCaps", VR_FoveationCapsString(), qtrue );

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

	VR_EnumerateInstanceExtensions();
	VR_BuildExtensionList();

	fprintf(stderr, "[OpenXR] Initializing OpenXR instance and system...\n");

	const qboolean listApiLayers = qfalse;
	if (listApiLayers)
	{
		VR_ListAPILayers();
	}

	// Create the OpenXR instance.
	// Meta's runtime reads the patch version as the app's SDK and gives 1.0.0 a
	// legacy profile that ignores the XrSwapchainCreateInfo next chain
	const char* appName = "Quake 3 Arena";
	const XrVersion apiVersion = XR_API_VERSION_1_0;
	Com_sprintf(s_declaredApiVersion, sizeof(s_declaredApiVersion), "%u.%u.%u",
		XR_VERSION_MAJOR(apiVersion), XR_VERSION_MINOR(apiVersion), XR_VERSION_PATCH(apiVersion));
	XR_CHECK(
		VR_CreateInstance(appName, apiVersion, numRequiredExtensions, requiredExtensionNames, &vr_engine.appState.Instance),
		"Failed to create OpenXR instance");

	XrInstanceProperties instanceInfo;
	instanceInfo.type = XR_TYPE_INSTANCE_PROPERTIES;
	instanceInfo.next = NULL;
	XR_CHECK(xrGetInstanceProperties(vr_engine.appState.Instance, &instanceInfo), "Failed to query OpenXR instance properties");
	Com_sprintf(s_runtimeDescription, sizeof(s_runtimeDescription), "%s %u.%u.%u",
		instanceInfo.runtimeName,
		XR_VERSION_MAJOR(instanceInfo.runtimeVersion),
		XR_VERSION_MINOR(instanceInfo.runtimeVersion),
		XR_VERSION_PATCH(instanceInfo.runtimeVersion));
	fprintf(stdout, "[OpenXR] Runtime: %s\n", s_runtimeDescription);

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
		free(s_instanceExtensions);
		s_instanceExtensions = NULL;
		s_numInstanceExtensions = 0;
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
