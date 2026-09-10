/*
 * vr_vk_session.c - Vulkan-specific XR session creation
 *
 * Creates XrSession with Vulkan graphics binding (XR_KHR_vulkan_enable2).
 * Uses VR-created Vulkan objects from vr_vk.c.
 */

// IMPORTANT: Vulkan headers and XR_USE_GRAPHICS_API_VULKAN must be defined
// BEFORE any OpenXR includes (including through vr_types.h via vr_vk.h)
#include <vulkan/vulkan.h>
#define XR_USE_GRAPHICS_API_VULKAN

#include "vr_vk.h"
#include "../qcommon/q_shared.h"
#include "../vrcommon/vr_macros.h"
#include "../vrcommon/vr_graphics.h"

#include <string.h>
#include <stdio.h>

// Create XR session with Vulkan graphics binding
XrResult VR_VK_CreateSession(XrInstance instance, XrSystemId systemId, XrSession* session)
{
	CHECK(VR_Vulkan_IsInitialized(), "[OpenXR] VR Vulkan not initialized before session creation");

	XrGraphicsBindingVulkanKHR graphicsBinding = {};
	memset(&graphicsBinding, 0, sizeof(graphicsBinding));
	graphicsBinding.type = XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR;
	graphicsBinding.next = NULL;
	graphicsBinding.instance = vr_vk.instance;
	graphicsBinding.physicalDevice = vr_vk.physicalDevice;
	graphicsBinding.device = vr_vk.device;
	graphicsBinding.queueFamilyIndex = vr_vk.queueFamilyIndex;
	graphicsBinding.queueIndex = 0;

	fprintf(stdout, "[OpenXR] Creating session with Vulkan binding\n");
	fprintf(stdout, "  VkInstance: %p\n", (void*)vr_vk.instance);
	fprintf(stdout, "  VkPhysicalDevice: %p\n", (void*)vr_vk.physicalDevice);
	fprintf(stdout, "  VkDevice: %p\n", (void*)vr_vk.device);
	fprintf(stdout, "  Queue Family: %d\n", vr_vk.queueFamilyIndex);

	XrSessionCreateInfo sessionCreateInfo = {};
	memset(&sessionCreateInfo, 0, sizeof(sessionCreateInfo));
	sessionCreateInfo.type = XR_TYPE_SESSION_CREATE_INFO;
	sessionCreateInfo.next = &graphicsBinding;
	sessionCreateInfo.createFlags = 0;
	sessionCreateInfo.systemId = systemId;

	XrResult result = xrCreateSession(instance, &sessionCreateInfo, session);
	if (XR_SUCCEEDED(result)) {
		// Declare Rec.709; prevents wide-gamut panels (e.g. Quest Pro QD-OLED) from
		// over-saturating sRGB content as P3. No-op where XR_FB_color_space is absent.
		PFN_xrEnumerateColorSpacesFB pfnEnumerate = NULL;
		PFN_xrSetColorSpaceFB pfnSet = NULL;
		xrGetInstanceProcAddr(instance, "xrEnumerateColorSpacesFB", (PFN_xrVoidFunction*)&pfnEnumerate);
		xrGetInstanceProcAddr(instance, "xrSetColorSpaceFB", (PFN_xrVoidFunction*)&pfnSet);
		if (pfnEnumerate && pfnSet) {
			uint32_t count = 0;
			if (XR_SUCCEEDED(pfnEnumerate(*session, 0, &count, NULL)) && count > 0) {
				XrColorSpaceFB spaces[16];
				if (count > 16) count = 16;
				if (XR_SUCCEEDED(pfnEnumerate(*session, count, &count, spaces))) {
					uint32_t i;
					qboolean have709 = qfalse;
					for (i = 0; i < count; i++) {
						if (spaces[i] == XR_COLOR_SPACE_REC709_FB) { have709 = qtrue; break; }
					}
					if (have709 && XR_SUCCEEDED(pfnSet(*session, XR_COLOR_SPACE_REC709_FB))) {
						vr_vk.xrColorManaged = VR_TRUE;
						fprintf(stdout, "[OpenXR] headset color space: Rec709 (color-managed)\n");
					}
				}
			}
		}
		if (!vr_vk.xrColorManaged) {
			fprintf(stdout, "[OpenXR] XR_FB_color_space unavailable; using runtime default color space\n");
		}
	}
	return result;
}

XrResult VR_Graphics_CreateSession(XrInstance instance, XrSystemId systemId, XrSession* session)
{
	return VR_VK_CreateSession(instance, systemId, session);
}
