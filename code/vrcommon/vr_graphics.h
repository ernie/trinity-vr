/*
 * vr_graphics.h - Graphics API abstraction interface
 *
 * This header provides an abstraction layer for graphics-specific VR operations.
 * The implementations are in vrvk/vr_vk.c.
 */

#ifndef __VR_GRAPHICS_H
#define __VR_GRAPHICS_H

#include <openxr/openxr.h>

// Get the graphics API extension name for OpenXR instance creation
// Returns XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME for Vulkan
const char* VR_Graphics_GetExtensionName(void);

// Get graphics requirements from OpenXR runtime
// This must be called after XR instance and system are created
// Returns XR_SUCCESS on success, error code on failure
XrResult VR_Graphics_GetRequirements(XrInstance instance, XrSystemId systemId);

// Print graphics requirements debug info
void VR_Graphics_PrintRequirements(void);

// Initialize graphics-specific VR subsystems
// Called after OpenXR instance is created
// Vulkan: creates VkInstance and VkDevice via xrCreateVulkanInstanceKHR/xrCreateVulkanDeviceKHR
void VR_Graphics_Init(XrInstance instance, XrSystemId systemId);

// Shutdown graphics-specific VR subsystems
void VR_Graphics_Shutdown(void);

// Invalidate XR function pointers before XrInstance is destroyed
// This is called from VR_Destroy() before xrDestroyInstance() to ensure
// function pointers obtained via xrGetInstanceProcAddr are cleared.
// Note: This does NOT destroy Vulkan resources; the renderer owns those.
void VR_Graphics_InvalidateFunctionPointers(void);

// Create XR session with graphics-specific binding
// Returns XR_SUCCESS on success
XrResult VR_Graphics_CreateSession(XrInstance instance, XrSystemId systemId, XrSession* session);

// Virtual screen interface: implemented in vrcommon (vr_virtual_screen.c),
// graphics-agnostic and not one of the roles above.
void VR_VirtualScreen_ResetPosition(void);
float VR_VirtualScreen_GetCurrentYaw(void);

#endif // __VR_GRAPHICS_H
