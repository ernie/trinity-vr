#ifndef __VR_VK_TYPES
#define __VR_VK_TYPES

// vr_types.h sets up the Vulkan OpenXR platform types
#include "../vrcommon/vr_types.h"

// Vulkan graphics requirements from OpenXR (XR_KHR_vulkan_enable2)
typedef struct {
	XrGraphicsRequirementsVulkan2KHR requirements;
} VR_VK_GraphicsRequirements;

// Vulkan-specific swapchain info
// VR layer only stores XrSwapchain handles and VkImage references.
// VkImageViews and VkFramebuffers are created by the renderer (see VkXrResources in vk.h)
typedef struct VR_VK_SwapchainInfo_s {
	XrSwapchain swapchain;       // XR swapchain handle (owned by VR layer)
	VkFormat format;             // Chosen format
	uint32_t width;
	uint32_t height;
	uint32_t arraySize;          // 2 for stereo multiview
	uint32_t imageCount;         // Number of swapchain images
	VkImage* images;             // VkImage handles from XR (NOT owned: from OpenXR)
	XrBool32 acquired;           // An image is held; a frame that draws nothing holds none
} VR_VK_SwapchainInfo;

// Concrete implementation of VR_SwapchainInfos for Vulkan
// VR layer only owns XrSwapchain handles. All Vulkan resources (VkImageViews,
// VkFramebuffers, virtual screen, etc.) are owned by the renderer (see VkXrResources in vk.h)
struct VR_SwapchainInfos_s {
	uint32_t viewCount;
	VR_VK_SwapchainInfo color;             // 2-layer multiview for stereo
};

#endif
