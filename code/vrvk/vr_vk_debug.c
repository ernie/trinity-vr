#include "vr_vk_debug.h"

#include <stdio.h>
#include <vulkan/vulkan.h>

//
// Vulkan Debug
//

// Note: Vulkan validation layer debugging is typically handled by the renderer
// itself (renderervk) via VK_EXT_debug_utils or VK_EXT_debug_report.
// This file provides VR-layer specific debug utilities if needed.

void VR_VK_RegisterDebugCallbackIfEnabled(void)
{
#if VR_ENABLE_VK_DEBUG
	// Vulkan debug callbacks are typically set up during VkInstance creation
	// in vr_vk.c as part of the XR_KHR_vulkan_enable2 workflow.
	//
	// If additional VR-layer-specific Vulkan debugging is needed, it can be
	// added here.
	fprintf(stderr, "[VR_VK] Vulkan debug enabled\n");
#endif
}
