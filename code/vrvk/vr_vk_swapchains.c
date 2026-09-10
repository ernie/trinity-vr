/*
 * vr_vk_swapchains.c - Vulkan XR swapchain management
 *
 * VR layer swapchain operations for Vulkan. Creates and manages XrSwapchains
 * and provides access to VkImage handles. VkImageViews and VkFramebuffers
 * are created by the renderer (see VkXrResources in renderervk/vk.h).
 */

#include "vr_vk_swapchains.h"
#include "vr_vk.h"

#include "../client/client.h"

#include "../vrcommon/vr_base.h"
#include "../vrcommon/vr_debug.h"
#include "../vrcommon/vr_macros.h"
#include "../vrcommon/vr_swapchains.h"

#include <stdlib.h>

//
// Internal helpers
//

// Helper to get the UNORM equivalent of an sRGB format
static VkFormat vk_get_unorm_format(VkFormat srgbFormat)
{
	switch (srgbFormat) {
		case VK_FORMAT_R8G8B8A8_SRGB:  return VK_FORMAT_R8G8B8A8_UNORM;
		case VK_FORMAT_B8G8R8A8_SRGB:  return VK_FORMAT_B8G8R8A8_UNORM;
		case VK_FORMAT_A8B8G8R8_SRGB_PACK32:  return VK_FORMAT_A8B8G8R8_UNORM_PACK32;
		default: return srgbFormat;  // Return as-is if not sRGB
	}
}

static void VR_VK_CreateSwapchain(
	XrSession session,
	XrBool32 isColor,
	XrBool32 mutableFormat,  // Allow creating UNORM views for gamma pass
	VkFormat format,
	uint32_t width,
	uint32_t height,
	uint32_t arraySize,
	VR_VK_SwapchainInfo* info)
{
	// TRANSFER_SRC for virtual screen blit, TRANSFER_DST for runtime compatibility
	XrSwapchainUsageFlags usage = isColor
		? (XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT)
		: (XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

	// Add mutable format flag for color swapchains that need UNORM views
	// This maps to VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT, allowing us to create
	// UNORM image views for the gamma pass (to avoid automatic sRGB conversion)
	if (mutableFormat) {
		usage |= XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT;
	}

	XrResult result;

	// For color swapchains with mutable format, use the format list extension
	// This tells the runtime which formats we'll use for image views, helping it
	// avoid adding unnecessary usage flags like VK_IMAGE_USAGE_STORAGE_BIT
	if (isColor && mutableFormat) {
		VkFormat viewFormats[2] = {
			format,                        // sRGB for normal rendering
			vk_get_unorm_format(format)    // UNORM for gamma pass (bypass sRGB conversion)
		};
		const VR_Bool formatListEnabled = VR_HasEnabledInstanceExtension("XR_KHR_vulkan_swapchain_format_list");

		// Validation reports UNORM views on sRGB images the runtime created
		// without VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT. These lines are the
		// evidence for the next capture to say whether the runtime ignored
		// this request or never saw it.
		// The runtime's identity belongs with them, because there is a third
		// outcome the other lines cannot show: vr_base.c records that a runtime
		// reading XR_API_VERSION_1_0's patch level as the app's SDK version
		// hands back a legacy profile that ignores the XrSwapchainCreateInfo
		// next chain, which is where the format list is attached. Under that
		// profile every line below reads positively and the request still never
		// arrives.
		Com_Printf("[VRVK] Color swapchain: runtime %s, declared API %s\n",
			VR_GetRuntimeDescription(), VR_GetDeclaredApiVersion());
		Com_Printf("[VRVK] Color swapchain: XR_KHR_vulkan_swapchain_format_list %s on the instance\n",
			formatListEnabled ? "enabled" : "NOT enabled");
		Com_Printf("[VRVK] Color swapchain: usage flags 0x%llx, mutable format bit %s\n",
			(unsigned long long)usage, (usage & XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT) ? "set" : "NOT set");
		Com_Printf("[VRVK] Color swapchain: format list chained, sRGB=0x%x UNORM=0x%x%s\n",
			(unsigned int)viewFormats[0], (unsigned int)viewFormats[1],
			(viewFormats[0] == viewFormats[1]) ? " (equal - request is a no-op)" : "");

		result = VR_Vulkan_CreateSwapchainWithFormatList(session, format, width, height,
			arraySize, usage, viewFormats, 2, &info->swapchain);

		Com_Printf("[VRVK] Color swapchain: xrCreateSwapchain result %d (%s)\n",
			(int)result, GetXRErrorString(result));
	} else {
		result = VR_Vulkan_CreateSwapchain(session, format, width, height,
			arraySize, usage, &info->swapchain);
	}

	CHECK(!XR_FAILED(result), isColor ? "Failed to create color swapchain" : "Failed to create depth swapchain");

	info->format = format;
	info->width = width;
	info->height = height;
	info->arraySize = arraySize;

	// Get VkImage handles from OpenXR
	result = VR_Vulkan_GetSwapchainImages(info->swapchain, &info->images, &info->imageCount);
	CHECK(!XR_FAILED(result), "Failed to get swapchain images");
}

static void VR_VK_DestroySwapchain(VR_VK_SwapchainInfo* info)
{
	if (!info) {
		return;
	}

	// OpenXR requires every acquired image released before the swapchain is
	// destroyed, and a restart mid-frame arrives with one held
	if (info->swapchain != XR_NULL_HANDLE && info->acquired) {
		XrSwapchainImageReleaseInfo releaseInfo = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO, NULL};
		xrReleaseSwapchainImage(info->swapchain, &releaseInfo);
		info->acquired = XR_FALSE;
		// everReleased is deliberately not latched here: info is freed right
		// after, and its replacement is calloc'd fresh (see everReleased in
		// vr_vk_types.h).
	}

	// Free images array (we don't own the VkImages themselves: OpenXR does)
	if (info->images) {
		free(info->images);
		info->images = NULL;
	}
	info->imageCount = 0;

	// Destroy the XR swapchain
	if (info->swapchain != XR_NULL_HANDLE) {
		XR_CHECK(
			xrDestroySwapchain(info->swapchain),
			"Failed to destroy XR swapchain");
		info->swapchain = XR_NULL_HANDLE;
	}
}

//
// Public API
//

VR_SwapchainInfos* VR_VK_CreateSwapchains(XrInstance instance, XrSystemId systemId, XrSession session)
{
	// Get best view configuration
	const XrViewConfigurationType viewConfigurationType = VR_GetBestViewConfiguration(instance, systemId);
	CHECK(
		viewConfigurationType != XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM,
		"No required view configuration type supported");

	// Get view configuration views
	XrViewConfigurationView* views = NULL;
	const uint32_t viewCount = VR_GetViewConfigurationViews(instance, systemId, viewConfigurationType, &views);

	// Sanity check: all views must have same resolution for multiview rendering
	for (uint32_t idx = 0; idx < viewCount; ++idx) {
		CHECK(
			views[0].recommendedImageRectWidth == views[idx].recommendedImageRectWidth,
			"Failed sanity check for same image sizes in Vulkan Multiview rendering");
		CHECK(
			views[0].recommendedImageRectHeight == views[idx].recommendedImageRectHeight,
			"Failed sanity check for same image sizes in Vulkan Multiview rendering");
	}

	// Get available swapchain formats
	int64_t* formats = NULL;
	const uint32_t formatCount = VR_GetSwapchainFormats(session, &formats);

	// Select best color format for Vulkan
	const VkFormat colorFormat = VR_Vulkan_SelectColorFormat(formats, formatCount);
	free(formats);

	Com_Printf("[VRVK] Chosen VK color format: 0x%x\n", (unsigned int)colorFormat);

	// Calculate supersampled resolution
	int supersampledWidth = views[0].recommendedImageRectWidth;
	int supersampledHeight = views[0].recommendedImageRectHeight;
	VR_GetSupersampledResolution(instance, systemId, &supersampledWidth, &supersampledHeight);

	// Allocate swapchain info structure
	VR_SwapchainInfos* swapchains = calloc(1, sizeof(VR_SwapchainInfos));
	if (!swapchains) {
		free(views);
		return NULL;
	}
	swapchains->viewCount = viewCount;

	// Create color swapchain (multiview - 2 layers for stereo)
	// Use mutable format to allow UNORM views for gamma pass (avoids sRGB auto-conversion)
	VR_VK_CreateSwapchain(
		session,
		XR_TRUE,  // isColor
		XR_TRUE,  // mutableFormat - needed for gamma pass UNORM views
		colorFormat,
		supersampledWidth,
		supersampledHeight,
		viewCount,  // arraySize = 2 for stereo
		&swapchains->color);

	Com_Printf("[VRVK] Created color swapchain: %dx%d, %u images, %u layers\n",
		swapchains->color.width, swapchains->color.height,
		swapchains->color.imageCount, swapchains->color.arraySize);

	free(views);
	return swapchains;
}

void VR_VK_DestroySwapchains(VR_SwapchainInfos** swapchainsPtr)
{
	if (!swapchainsPtr || !*swapchainsPtr) {
		return;
	}

	VR_SwapchainInfos* swapchains = *swapchainsPtr;

	// Destroy swapchains (VR layer only owns XrSwapchain handles)
	// VkImageViews and VkFramebuffers are destroyed by the renderer
	VR_VK_DestroySwapchain(&swapchains->color);

	free(swapchains);
	*swapchainsPtr = NULL;
}

void VR_VK_Swapchains_Acquire(VR_SwapchainInfos* swapchains, uint32_t* colorIndex)
{
	if (!swapchains) {
		return;
	}

	XrSwapchainImageAcquireInfo acquireInfo = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO, NULL};

	XR_CHECK(
		xrAcquireSwapchainImage(swapchains->color.swapchain, &acquireInfo, colorIndex),
		"Failed to acquire color swapchain image");

	XrSwapchainImageWaitInfo waitInfo = {
		.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO,
		.next = NULL,
		.timeout = XR_INFINITE_DURATION
	};

	CHECK(
		!XR_FAILED(xrWaitSwapchainImage(swapchains->color.swapchain, &waitInfo)),
		"Failed to wait for color swapchain image");

	swapchains->color.acquired = XR_TRUE;
}

void VR_VK_Swapchains_Release(VR_SwapchainInfos* swapchains)
{
	if (!swapchains) {
		return;
	}

	XrSwapchainImageReleaseInfo releaseInfo = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO, NULL};
	XR_CHECK(
		xrReleaseSwapchainImage(swapchains->color.swapchain, &releaseInfo),
		"Failed to release color swapchain image");

	swapchains->color.acquired = XR_FALSE;
	swapchains->color.everReleased = XR_TRUE;
}

//
// Accessors for renderer to get swapchain info
//

const VR_VK_SwapchainInfo* VR_VK_GetColorSwapchain(const VR_SwapchainInfos* swapchains)
{
	return swapchains ? &swapchains->color : NULL;
}

