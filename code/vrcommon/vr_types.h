#ifndef __VR_TYPES
#define __VR_TYPES

#include <stdint.h>

// Platform-specific defines for OpenXR
#if defined(WIN32)
#include "unknwn.h"
#define XR_USE_PLATFORM_WIN32
#elif defined(__ANDROID__)
#define XR_USE_PLATFORM_ANDROID
#endif
// vulkan.h must precede openxr_platform.h so OpenXR can use Vulkan types
#include <vulkan/vulkan.h>
#define XR_USE_GRAPHICS_API_VULKAN

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include "vr_safe_types.h"

#define OXR(func) func;

// Renderer-agnostic boolean type
// Uses XrBool32 which is always available from OpenXR
typedef XrBool32 VR_Bool;
#define VR_TRUE  XR_TRUE
#define VR_FALSE XR_FALSE

// The struct is defined in vrvk/vr_vk_types.h
typedef struct VR_SwapchainInfos_s VR_SwapchainInfos;

#define VR_MAX_REFRESH_RATES 16

// vr_foveation: what the pattern follows; strength is separate, either pattern can be gentle or aggressive
#define VR_FOVEATION_OFF          0
#define VR_FOVEATION_FIXED        1
#define VR_FOVEATION_EYE_TRACKED  2

// vr_foveationStrength: how far detail drops off toward the edges
#define VR_FOVEATION_STRENGTH_LOW     1
#define VR_FOVEATION_STRENGTH_MEDIUM  2
#define VR_FOVEATION_STRENGTH_HIGH    3

typedef struct
{
	VR_SwapchainInfos* Swapchains;  // Pointer to graphics-specific swapchain info
	float RefreshRate;
	float SupportedRefreshRates[VR_MAX_REFRESH_RATES]; // as enumerated from the runtime
	uint32_t NumSupportedRefreshRates;
} VR_Renderer;

// What this Vulkan device (and, for Plan B, the runtime) can do for foveated
// rendering, decided once at device creation.
typedef enum
{
	VR_FOVEATION_CAPS_NONE,        // no usable shading-rate attachment
	VR_FOVEATION_CAPS_FIXED,       // VK_KHR_fragment_shading_rate attachment feature
	VR_FOVEATION_CAPS_EYE_TRACKED  // fixed plus the system reporting gaze tracking
} VR_FoveationCaps;

typedef struct
{
	VR_Bool Active;
	XrPosef Pose;
} VR_TrackedController;

typedef struct
{
	XrInstance Instance;
	XrSystemId SystemId;
	XrSession Session;

	VR_Bool SessionActive;
	VR_Bool Focused;
	VR_Bool Visible;

	XrDebugUtilsMessengerEXT DebugUtilsMessenger;

	XrSpace HeadSpace;
	XrSpace StageSpace;
	XrSpace FakeStageSpace;
	XrSpace CurrentSpace;
	XrSpace ViewSpace;          // VIEW reference space for head-locked quad layers

	VR_Renderer Renderer;
	VR_TrackedController TrackedController[2];
} VR_App;

typedef struct
{
	XrSystemProperties SystemProperties;
	// Graphics requirements are stored in graphics-specific code
} VR_SystemProperties;

typedef struct
{
	int width;
	int height;
} VR_Window;

typedef struct
{
	VR_Window window;
	VR_SystemProperties systemProperties;
	VR_App appState;
} VR_Engine;

#endif
