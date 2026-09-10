/*
 * VR Vulkan Integration
 *
 * Implements XR_KHR_vulkan_enable2 workflow for OpenXR/Vulkan integration.
 * This module creates and manages Vulkan instance and device that OpenXR requires,
 * which are then passed to renderervk for use.
 */

// IMPORTANT: Vulkan headers and XR_USE_GRAPHICS_API_VULKAN must be defined
// BEFORE any OpenXR includes (including through vr_types.h via vr_vk.h)
#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan.h>
#define XR_USE_GRAPHICS_API_VULKAN

#include "vr_vk.h"
#include "../vrcommon/vr_base.h"
#include "../vrcommon/vr_graphics.h"
#include "../vrcommon/vr_macros.h"
#include "vr_vk_debug.h"

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Global VR Vulkan state
VR_VulkanState vr_vk;

static VR_Bool vr_vk_initialized = VR_FALSE;

// Function pointers for XR_KHR_vulkan_enable2
// Note: enable2 does NOT provide xrGetVulkanInstanceExtensionsKHR or xrGetVulkanDeviceExtensionsKHR
// Those are from the older XR_KHR_vulkan_enable extension. With enable2, the runtime adds
// required extensions via xrCreateVulkanInstanceKHR and xrCreateVulkanDeviceKHR.
static PFN_xrGetVulkanGraphicsRequirements2KHR xrGetVulkanGraphicsRequirements2KHR = NULL;
static PFN_xrGetVulkanGraphicsDevice2KHR xrGetVulkanGraphicsDevice2KHR = NULL;
static PFN_xrCreateVulkanInstanceKHR xrCreateVulkanInstanceKHR = NULL;
static PFN_xrCreateVulkanDeviceKHR xrCreateVulkanDeviceKHR = NULL;

// Forward declarations
static VR_Bool LoadXrVulkanFunctions(XrInstance xrInstance);

// Get the Vulkan graphics extension name for OpenXR instance creation
const char* VR_VK_GetGraphicsExtensionName(void)
{
	return XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME;
}

// Get Vulkan graphics requirements from OpenXR
// This is a wrapper that populates VR_VK_GraphicsRequirements
XrResult VR_VK_GetGraphicsRequirements(XrInstance instance, XrSystemId systemId,
                                        VR_VK_GraphicsRequirements* requirements)
{
	// First ensure we have the function pointer
	if (!xrGetVulkanGraphicsRequirements2KHR) {
		if (!LoadXrVulkanFunctions(instance)) {
			return XR_ERROR_FUNCTION_UNSUPPORTED;
		}
	}

	requirements->requirements.type = XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR;
	requirements->requirements.next = NULL;

	return xrGetVulkanGraphicsRequirements2KHR(instance, systemId, &requirements->requirements);
}

// Print graphics requirements debug info
void VR_VK_PrintGraphicsRequirements(const VR_VK_GraphicsRequirements* requirements)
{
	// Use XR version macros: XrVersion is encoded differently than VkVersion
	fprintf(stderr, "[OpenXR] Vulkan version requirements: [%d.%d.%d, %d.%d.%d]\n",
		XR_VERSION_MAJOR(requirements->requirements.minApiVersionSupported),
		XR_VERSION_MINOR(requirements->requirements.minApiVersionSupported),
		(int)XR_VERSION_PATCH(requirements->requirements.minApiVersionSupported),
		XR_VERSION_MAJOR(requirements->requirements.maxApiVersionSupported),
		XR_VERSION_MINOR(requirements->requirements.maxApiVersionSupported),
		(int)XR_VERSION_PATCH(requirements->requirements.maxApiVersionSupported));
}

// Load XR Vulkan extension function pointers for XR_KHR_vulkan_enable2
static VR_Bool LoadXrVulkanFunctions(XrInstance xrInstance)
{
    XrResult result;

    result = xrGetInstanceProcAddr(xrInstance, "xrGetVulkanGraphicsRequirements2KHR",
                                    (PFN_xrVoidFunction*)&xrGetVulkanGraphicsRequirements2KHR);
    if (XR_FAILED(result)) {
        fprintf(stderr, "[VRVK] Failed to get xrGetVulkanGraphicsRequirements2KHR\n");
        return VR_FALSE;
    }

    result = xrGetInstanceProcAddr(xrInstance, "xrGetVulkanGraphicsDevice2KHR",
                                    (PFN_xrVoidFunction*)&xrGetVulkanGraphicsDevice2KHR);
    if (XR_FAILED(result)) {
        fprintf(stderr, "[VRVK] Failed to get xrGetVulkanGraphicsDevice2KHR\n");
        return VR_FALSE;
    }

    result = xrGetInstanceProcAddr(xrInstance, "xrCreateVulkanInstanceKHR",
                                    (PFN_xrVoidFunction*)&xrCreateVulkanInstanceKHR);
    if (XR_FAILED(result)) {
        fprintf(stderr, "[VRVK] Failed to get xrCreateVulkanInstanceKHR\n");
        return VR_FALSE;
    }

    result = xrGetInstanceProcAddr(xrInstance, "xrCreateVulkanDeviceKHR",
                                    (PFN_xrVoidFunction*)&xrCreateVulkanDeviceKHR);
    if (XR_FAILED(result)) {
        fprintf(stderr, "[VRVK] Failed to get xrCreateVulkanDeviceKHR\n");
        return VR_FALSE;
    }

    return VR_TRUE;
}

VR_Bool VR_Vulkan_IsInitialized(void)
{
    return vr_vk_initialized;
}

const VR_VulkanDeviceInfo* VR_Vulkan_GetDeviceInfo(void)
{
    if (!vr_vk_initialized) {
        return NULL;
    }

    // Return pointer to static info populated from vr_vk globals
    static VR_VulkanDeviceInfo info;
    info.instance = vr_vk.instance;
    info.physicalDevice = vr_vk.physicalDevice;
    info.device = vr_vk.device;
    info.queue = vr_vk.queue;
    info.queueFamilyIndex = vr_vk.queueFamilyIndex;
    info.swapchainColorspaceEnabled = vr_vk.swapchainColorspaceEnabled;
    info.debugUtilsEnabled = vr_vk.debugUtilsEnabled;
    return &info;
}

const VR_VulkanSwapchainInfo* VR_Vulkan_GetSwapchainInfo(void)
{
    VR_Engine* engine = VR_GetEngine();
    if (!engine || !engine->appState.Renderer.Swapchains) {
        return NULL;
    }

    VR_SwapchainInfos* swapchains = engine->appState.Renderer.Swapchains;

    // Return pointer to static info populated from swapchain data
    static VR_VulkanSwapchainInfo info;

    // Color swapchain only - depth is renderer-managed
    info.colorFormat = swapchains->color.format;
    info.colorWidth = swapchains->color.width;
    info.colorHeight = swapchains->color.height;
    info.colorArraySize = swapchains->color.arraySize;
    info.colorImageCount = swapchains->color.imageCount;
    info.colorImages = swapchains->color.images;

    return &info;
}

XrResult VR_Vulkan_CheckRequirements(XrInstance xrInstance, XrSystemId systemId)
{
    if (!LoadXrVulkanFunctions(xrInstance)) {
        return XR_ERROR_FUNCTION_UNSUPPORTED;
    }

    XrGraphicsRequirementsVulkan2KHR requirements = {
        .type = XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR,
        .next = NULL,
    };

    XrResult result = xrGetVulkanGraphicsRequirements2KHR(xrInstance, systemId, &requirements);
    if (XR_FAILED(result)) {
        fprintf(stderr, "[VRVK] Failed to get graphics requirements: %s\n",
                GetXRErrorString(result));
        return result;
    }

    vr_vk.minApiVersionSupported = requirements.minApiVersionSupported;
    vr_vk.maxApiVersionSupported = requirements.maxApiVersionSupported;

    fprintf(stdout, "[VRVK] Graphics Requirements:\n");
    fprintf(stdout, "  Min Vulkan API: %d.%d.%d\n",
            XR_VERSION_MAJOR(requirements.minApiVersionSupported),
            XR_VERSION_MINOR(requirements.minApiVersionSupported),
            XR_VERSION_PATCH(requirements.minApiVersionSupported));
    fprintf(stdout, "  Max Vulkan API: %d.%d.%d\n",
            XR_VERSION_MAJOR(requirements.maxApiVersionSupported),
            XR_VERSION_MINOR(requirements.maxApiVersionSupported),
            XR_VERSION_PATCH(requirements.maxApiVersionSupported));

    // We require Vulkan 1.1 for multiview: use XR version encoding for comparison
    XrVersion ourVersion = XR_MAKE_VERSION(1, 1, 0);

    // If runtime reports 0 for min/max, it means any version is acceptable
    if (requirements.minApiVersionSupported != 0 && ourVersion < requirements.minApiVersionSupported) {
        fprintf(stderr, "[VRVK] Vulkan 1.1 required but runtime requires higher version\n");
        return XR_ERROR_GRAPHICS_REQUIREMENTS_CALL_MISSING;
    }
    if (requirements.maxApiVersionSupported != 0 && ourVersion > requirements.maxApiVersionSupported) {
        fprintf(stderr, "[VRVK] Warning: Our Vulkan 1.1 exceeds runtime's max version\n");
        // Continue anyway, usually works
    }

    return XR_SUCCESS;
}

// Note: VR_Vulkan_GetInstanceExtensions() removed: not needed with XR_KHR_vulkan_enable2
// The enable2 extension uses xrCreateVulkanInstanceKHR which automatically adds required extensions

XrResult VR_Vulkan_CreateInstance(XrInstance xrInstance, XrSystemId systemId)
{
    // With XR_KHR_vulkan_enable2, we only need to provide our own extensions.
    // The runtime will add any additional required extensions during xrCreateVulkanInstanceKHR.

    uint32_t availExtCount = 0;
    VkBool32 colorspaceSupported = VK_FALSE;
    VkBool32 debugUtilsSupported = VK_FALSE;
    VkBool32 scanned = VK_FALSE;
    VkResult scanResult = vkEnumerateInstanceExtensionProperties(NULL, &availExtCount, NULL);
    if (availExtCount > 0) {
        VkExtensionProperties* availExts = (VkExtensionProperties*)malloc(
            sizeof(VkExtensionProperties) * availExtCount);
        if (availExts) {
            scanResult = vkEnumerateInstanceExtensionProperties(NULL, &availExtCount, availExts);
            if (scanResult == VK_SUCCESS) {
                for (uint32_t i = 0; i < availExtCount; i++) {
                    if (strcmp(availExts[i].extensionName,
                               VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME) == 0) {
                        colorspaceSupported = VK_TRUE;
                    } else if (strcmp(availExts[i].extensionName,
                                      VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
                        debugUtilsSupported = VK_TRUE;
                    }
                }
                scanned = VK_TRUE;
            }
            free(availExts);
        }
    }
    // A scan that never ran leaves every optional extension below looking absent,
    // and downstream reports that as the runtime not offering it. Only this call
    // can tell a refused query from a genuinely missing name, so it says so when
    // it fails and stays quiet when it succeeds.
    if (!scanned) {
        Com_Printf("[VRVK] Instance extension scan failed: %u listed, result %d\n",
                   availExtCount, (int)scanResult);
    }

    // Our extensions:
    // - VK_KHR_surface + platform surface for desktop mirror window
    // - VK_EXT_swapchain_colorspace when available (enables HDR on desktop mirror)
    // - VK_EXT_debug_utils when available (names Vulkan objects for a capture)
    const char* extensions[8];
    uint32_t extensionCount = 0;
    extensions[extensionCount++] = VK_KHR_SURFACE_EXTENSION_NAME;
#ifdef _WIN32
    extensions[extensionCount++] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
#endif
    if (colorspaceSupported) {
        extensions[extensionCount++] = VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME;
        Com_Printf("[VRVK] VK_EXT_swapchain_colorspace available; requesting it\n");
    }
    if (debugUtilsSupported) {
        extensions[extensionCount++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    }

    vr_vk.swapchainColorspaceEnabled = colorspaceSupported;
    vr_vk.debugUtilsEnabled = debugUtilsSupported;

    // Application info
    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = NULL,
        .pApplicationName = "Quake 3 VR",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "ioquake3",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_1,
    };

    // Instance create info
    VkInstanceCreateInfo instanceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = NULL,
        .enabledExtensionCount = extensionCount,
        .ppEnabledExtensionNames = extensions,
    };

#ifdef _DEBUG
    // Enable validation layer in debug builds
    const char* validationLayers[] = {
        "VK_LAYER_KHRONOS_validation",
    };
    instanceCreateInfo.enabledLayerCount = 1;
    instanceCreateInfo.ppEnabledLayerNames = validationLayers;
#endif

    // Use xrCreateVulkanInstanceKHR to create instance with XR-required extensions
    // The runtime adds any extensions it needs automatically
    XrVulkanInstanceCreateInfoKHR xrCreateInfo = {
        .type = XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR,
        .next = NULL,
        .systemId = systemId,
        .createFlags = 0,
        .pfnGetInstanceProcAddr = vkGetInstanceProcAddr,
        .vulkanCreateInfo = &instanceCreateInfo,
        .vulkanAllocator = NULL,
    };

    VkResult vkResult;
    XrResult result = xrCreateVulkanInstanceKHR(xrInstance, &xrCreateInfo, &vr_vk.instance, &vkResult);

    if (XR_FAILED(result)) {
        fprintf(stderr, "[VRVK] xrCreateVulkanInstanceKHR failed: %s\n",
                GetXRErrorString(result));
        return result;
    }

    if (vkResult != VK_SUCCESS) {
        fprintf(stderr, "[VRVK] vkCreateInstance failed: %d\n", vkResult);
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    fprintf(stdout, "[VRVK] VkInstance created successfully\n");
    return XR_SUCCESS;
}

XrResult VR_Vulkan_GetPhysicalDevice(XrInstance xrInstance, XrSystemId systemId)
{
    XrVulkanGraphicsDeviceGetInfoKHR deviceGetInfo = {
        .type = XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR,
        .next = NULL,
        .systemId = systemId,
        .vulkanInstance = vr_vk.instance,
    };

    XrResult result = xrGetVulkanGraphicsDevice2KHR(xrInstance, &deviceGetInfo,
                                                     &vr_vk.physicalDevice);
    if (XR_FAILED(result)) {
        fprintf(stderr, "[VRVK] Failed to get XR physical device: %s\n",
                GetXRErrorString(result));
        return result;
    }

    // Cache device properties
    vkGetPhysicalDeviceProperties(vr_vk.physicalDevice, &vr_vk.deviceProperties);
    vkGetPhysicalDeviceMemoryProperties(vr_vk.physicalDevice, &vr_vk.memoryProperties);
    vkGetPhysicalDeviceFeatures(vr_vk.physicalDevice, &vr_vk.deviceFeatures);

    fprintf(stdout, "[VRVK] XR Physical Device: %s\n", vr_vk.deviceProperties.deviceName);
    fprintf(stdout, "  Driver Version: %d.%d.%d\n",
            VK_API_VERSION_MAJOR(vr_vk.deviceProperties.driverVersion),
            VK_API_VERSION_MINOR(vr_vk.deviceProperties.driverVersion),
            VK_API_VERSION_PATCH(vr_vk.deviceProperties.driverVersion));
    fprintf(stdout, "  API Version: %d.%d.%d\n",
            VK_API_VERSION_MAJOR(vr_vk.deviceProperties.apiVersion),
            VK_API_VERSION_MINOR(vr_vk.deviceProperties.apiVersion),
            VK_API_VERSION_PATCH(vr_vk.deviceProperties.apiVersion));

    // Check multiview support
    VkPhysicalDeviceMultiviewFeatures multiviewFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES,
        .pNext = NULL,
    };
    VkPhysicalDeviceFeatures2 features2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &multiviewFeatures,
    };
    vkGetPhysicalDeviceFeatures2(vr_vk.physicalDevice, &features2);

    vr_vk.multiviewSupported = multiviewFeatures.multiview ? VR_TRUE : VR_FALSE;

    if (!vr_vk.multiviewSupported) {
        fprintf(stderr, "[VRVK] ERROR: Device does not support multiview!\n");
        return XR_ERROR_GRAPHICS_DEVICE_INVALID;
    }

    // Get max multiview view count
    VkPhysicalDeviceMultiviewProperties multiviewProps = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES,
        .pNext = NULL,
    };
    VkPhysicalDeviceProperties2 props2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &multiviewProps,
    };
    vkGetPhysicalDeviceProperties2(vr_vk.physicalDevice, &props2);

    vr_vk.maxMultiviewViewCount = multiviewProps.maxMultiviewViewCount;
    fprintf(stdout, "  Multiview: supported (max views: %d)\n", vr_vk.maxMultiviewViewCount);

    return XR_SUCCESS;
}

/*
 * Whether the XR physical device lists a device extension. The renderer builds
 * its main render pass through entry points two of these provide, and a missing
 * one should name itself rather than surface as a number out of vkCreateDevice.
 */
static VkBool32 VR_Vulkan_DeviceExtensionSupported(const char* name)
{
    uint32_t count = 0;
    VkExtensionProperties* props;
    VkBool32 found = VK_FALSE;
    VkResult result;

    // Every failure below answers "not supported", so the result and the count
    // travel with the answer; otherwise a refused query reads as an absent name
    result = vkEnumerateDeviceExtensionProperties(vr_vk.physicalDevice, NULL, &count, NULL);
    if (result != VK_SUCCESS || count == 0) {
        Com_Printf("[VRVK] Device extension %s: nothing to scan, %u listed, result %d\n",
                   name, count, (int)result);
        return VK_FALSE;
    }

    props = (VkExtensionProperties*)malloc(sizeof(VkExtensionProperties) * count);
    if (props == NULL) {
        Com_Printf("[VRVK] Device extension %s: no memory for %u properties\n", name, count);
        return VK_FALSE;
    }

    result = vkEnumerateDeviceExtensionProperties(vr_vk.physicalDevice, NULL, &count, props);
    if (result == VK_SUCCESS) {
        for (uint32_t i = 0; i < count; i++) {
            if (strcmp(props[i].extensionName, name) == 0) {
                found = VK_TRUE;
                break;
            }
        }
    } else {
        Com_Printf("[VRVK] Device extension %s: second scan failed, %u listed, result %d\n",
                   name, count, (int)result);
    }

    free(props);
    return found;
}

XrResult VR_Vulkan_CreateDevice(XrInstance xrInstance, XrSystemId systemId)
{
    // Our required extensions: runtime will add any additional ones via xrCreateVulkanDeviceKHR.
    // Fixed capacity with an explicit count, so that a conditional request below
    // cannot run off the end of an initializer-sized array without a diagnostic.
    const char* extensions[8];
    uint32_t extensionCount = 0;

    extensions[extensionCount++] = VK_KHR_MULTIVIEW_EXTENSION_NAME;               // For stereo rendering
    extensions[extensionCount++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;               // For desktop mirror window
    extensions[extensionCount++] = VK_KHR_MAINTENANCE_4_EXTENSION_NAME;           // Relaxes push constant validation
    extensions[extensionCount++] = VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME;     // Render pass form the depth resolve needs
    extensions[extensionCount++] = VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME;   // Main pass resolves depth for the post pass

    // Both are Vulkan 1.2 core and present on every part that runs a PCVR
    // runtime, and the renderer has no path without them
    if (!VR_Vulkan_DeviceExtensionSupported(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME)) {
        fprintf(stderr, "[VRVK] ERROR: device does not support %s\n", VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);
        return XR_ERROR_GRAPHICS_DEVICE_INVALID;
    }
    if (!VR_Vulkan_DeviceExtensionSupported(VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME)) {
        fprintf(stderr, "[VRVK] ERROR: device does not support %s\n", VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME);
        return XR_ERROR_GRAPHICS_DEVICE_INVALID;
    }

    // Find graphics queue family
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(vr_vk.physicalDevice, &queueFamilyCount, NULL);

    VkQueueFamilyProperties* queueFamilies = (VkQueueFamilyProperties*)malloc(
        sizeof(VkQueueFamilyProperties) * queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(vr_vk.physicalDevice, &queueFamilyCount, queueFamilies);

    vr_vk.queueFamilyIndex = UINT32_MAX;
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            vr_vk.queueFamilyIndex = i;
            break;
        }
    }
    free(queueFamilies);

    if (vr_vk.queueFamilyIndex == UINT32_MAX) {
        fprintf(stderr, "[VRVK] No graphics queue family found\n");
        return XR_ERROR_GRAPHICS_DEVICE_INVALID;
    }

    fprintf(stdout, "[VRVK] Using queue family %d for graphics\n", vr_vk.queueFamilyIndex);

    // Enable maintenance4 feature (relaxes push constant validation)
    VkPhysicalDeviceMaintenance4Features maintenance4Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES,
        .pNext = NULL,
        .maintenance4 = VK_TRUE,
    };

    // Enable multiview feature
    VkPhysicalDeviceMultiviewFeatures multiviewFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES,
        .pNext = &maintenance4Features,  // Chain maintenance4
        .multiview = VK_TRUE,
        .multiviewGeometryShader = VK_FALSE,
        .multiviewTessellationShader = VK_FALSE,
    };

    VkPhysicalDeviceFeatures2 features2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &multiviewFeatures,
    };
    // Get all supported features
    vkGetPhysicalDeviceFeatures2(vr_vk.physicalDevice, &features2);
    // Ensure required features are enabled after querying
    multiviewFeatures.multiview = VK_TRUE;
    maintenance4Features.maintenance4 = VK_TRUE;

    // Queue create info
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .queueFamilyIndex = vr_vk.queueFamilyIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority,
    };

    // Device create info
    VkDeviceCreateInfo deviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &features2,
        .flags = 0,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = NULL,
        .enabledExtensionCount = extensionCount,
        .ppEnabledExtensionNames = extensions,
        .pEnabledFeatures = NULL, // Using features2 instead
    };

    // Use xrCreateVulkanDeviceKHR to create device with XR-required extensions
    XrVulkanDeviceCreateInfoKHR xrDeviceCreateInfo = {
        .type = XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR,
        .next = NULL,
        .systemId = systemId,
        .createFlags = 0,
        .pfnGetInstanceProcAddr = vkGetInstanceProcAddr,
        .vulkanPhysicalDevice = vr_vk.physicalDevice,
        .vulkanCreateInfo = &deviceCreateInfo,
        .vulkanAllocator = NULL,
    };

    VkResult vkResult;
    XrResult result = xrCreateVulkanDeviceKHR(xrInstance, &xrDeviceCreateInfo, &vr_vk.device, &vkResult);

    if (XR_FAILED(result)) {
        fprintf(stderr, "[VRVK] xrCreateVulkanDeviceKHR failed: %s\n",
                GetXRErrorString(result));
        return result;
    }

    if (vkResult != VK_SUCCESS) {
        fprintf(stderr, "[VRVK] vkCreateDevice failed: %d\n", vkResult);
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    // Get the graphics queue
    vkGetDeviceQueue(vr_vk.device, vr_vk.queueFamilyIndex, 0, &vr_vk.queue);

    vr_vk_initialized = VR_TRUE;

    fprintf(stdout, "[VRVK] VkDevice created successfully\n");
    fprintf(stdout, "[VRVK] Vulkan initialization complete\n");

    return XR_SUCCESS;
}

void VR_Vulkan_Shutdown(void)
{
    if (!vr_vk_initialized) {
        return;
    }

    fprintf(stdout, "[VRVK] Shutting down Vulkan...\n");

    // NOTE: We do NOT destroy vr_vk.device or vr_vk.instance here.
    // The renderer (renderervk) pulls these handles via VR_Vulkan_GetDeviceInfo()
    // and is responsible for destroying them in vk_shutdown().
    // We only clear our state to mark ourselves as uninitialized.

    memset(&vr_vk, 0, sizeof(vr_vk));
    vr_vk_initialized = VR_FALSE;

    // Clear XR function pointers: they become invalid when XrInstance is destroyed
    xrGetVulkanGraphicsRequirements2KHR = NULL;
    xrGetVulkanGraphicsDevice2KHR = NULL;
    xrCreateVulkanInstanceKHR = NULL;
    xrCreateVulkanDeviceKHR = NULL;

    fprintf(stdout, "[VRVK] Vulkan shutdown complete\n");
}

// XR Swapchain format selection
VkFormat VR_Vulkan_SelectColorFormat(const int64_t* formats, uint32_t count)
{
    // Preference order for color (sRGB preferred for gamma-correct rendering)
    const VkFormat preferred[] = {
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_B8G8R8A8_SRGB,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_B8G8R8A8_UNORM,
    };

    for (uint32_t p = 0; p < sizeof(preferred) / sizeof(preferred[0]); p++) {
        for (uint32_t f = 0; f < count; f++) {
            if (formats[f] == (int64_t)preferred[p]) {
                return preferred[p];
            }
        }
    }

    // Use first available as last resort
    return (VkFormat)formats[0];
}

VkFormat VR_Vulkan_SelectDepthFormat(const int64_t* formats, uint32_t count)
{
    // Preference order for depth (32-bit float preferred for reversed depth precision)
    const VkFormat preferred[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
    };

    for (uint32_t p = 0; p < sizeof(preferred) / sizeof(preferred[0]); p++) {
        for (uint32_t f = 0; f < count; f++) {
            if (formats[f] == (int64_t)preferred[p]) {
                return preferred[p];
            }
        }
    }

    return (VkFormat)formats[0];
}

XrResult VR_Vulkan_CreateSwapchain(XrSession session, VkFormat format,
                                    uint32_t width, uint32_t height,
                                    uint32_t arraySize, XrSwapchainUsageFlags usage,
                                    XrSwapchain* swapchain)
{
    // Simple version without format list
    return VR_Vulkan_CreateSwapchainWithFormatList(session, format, width, height,
                                                    arraySize, usage, NULL, 0, swapchain);
}

XrResult VR_Vulkan_CreateSwapchainWithFormatList(XrSession session, VkFormat format,
                                                  uint32_t width, uint32_t height,
                                                  uint32_t arraySize, XrSwapchainUsageFlags usage,
                                                  const VkFormat* viewFormats, uint32_t viewFormatCount,
                                                  XrSwapchain* swapchain)
{
    // Set up format list if provided (XR_KHR_vulkan_swapchain_format_list extension)
    // This tells the runtime which formats we'll use for image views, helping it
    // avoid adding unnecessary usage flags like VK_IMAGE_USAGE_STORAGE_BIT
    XrVulkanSwapchainFormatListCreateInfoKHR formatListInfo = {
        .type = XR_TYPE_VULKAN_SWAPCHAIN_FORMAT_LIST_CREATE_INFO_KHR,
        .next = NULL,
        .viewFormatCount = viewFormatCount,
        .viewFormats = viewFormats,
    };

    XrSwapchainCreateInfo createInfo = {
        .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
        .next = (viewFormats && viewFormatCount > 0) ? &formatListInfo : NULL,
        .createFlags = 0,
        .usageFlags = usage,
        .format = (int64_t)format,
        .sampleCount = 1,
        .width = width,
        .height = height,
        .faceCount = 1,
        .arraySize = arraySize,
        .mipCount = 1,
    };

    return xrCreateSwapchain(session, &createInfo, swapchain);
}

XrResult VR_Vulkan_GetSwapchainImages(XrSwapchain swapchain,
                                       VkImage** images, uint32_t* imageCount)
{
    // Get count
    XrResult result = xrEnumerateSwapchainImages(swapchain, 0, imageCount, NULL);
    if (XR_FAILED(result)) {
        return result;
    }

    // Allocate XrSwapchainImageVulkanKHR array
    XrSwapchainImageVulkanKHR* xrImages = (XrSwapchainImageVulkanKHR*)malloc(
        sizeof(XrSwapchainImageVulkanKHR) * (*imageCount));
    if (!xrImages) {
        return XR_ERROR_OUT_OF_MEMORY;
    }

    for (uint32_t i = 0; i < *imageCount; i++) {
        xrImages[i].type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
        xrImages[i].next = NULL;
    }

    result = xrEnumerateSwapchainImages(swapchain, *imageCount, imageCount,
                                         (XrSwapchainImageBaseHeader*)xrImages);

    if (XR_SUCCEEDED(result)) {
        // Extract VkImage handles
        *images = (VkImage*)malloc(sizeof(VkImage) * (*imageCount));
        if (*images) {
            for (uint32_t i = 0; i < *imageCount; i++) {
                (*images)[i] = xrImages[i].image;
            }
        } else {
            result = XR_ERROR_OUT_OF_MEMORY;
        }
    }

    free(xrImages);
    return result;
}

// VkImageView/VkFramebuffer creation moved to renderer (see vk_create_xr_image_views in vk.c)
// VR layer only provides XrSwapchain handles and VkImage handles

// ============================================================================
// VR_Graphics interface implementation for Vulkan
// These functions are called from vrcommon code and provide the graphics-specific
// behavior for Vulkan builds.
// ============================================================================

// Cached graphics requirements
static VR_VK_GraphicsRequirements s_vkRequirements;
static VR_Bool s_requirementsFetched = VR_FALSE;

const char* VR_Graphics_GetExtensionName(void)
{
	return VR_VK_GetGraphicsExtensionName();
}

XrResult VR_Graphics_GetRequirements(XrInstance instance, XrSystemId systemId)
{
	XrResult result = VR_VK_GetGraphicsRequirements(instance, systemId, &s_vkRequirements);
	if (XR_SUCCEEDED(result)) {
		s_requirementsFetched = VR_TRUE;
	}
	return result;
}

void VR_Graphics_PrintRequirements(void)
{
	if (s_requirementsFetched) {
		VR_VK_PrintGraphicsRequirements(&s_vkRequirements);
	}
}

void VR_Graphics_Init(XrInstance instance, XrSystemId systemId)
{
	fprintf(stdout, "[VRVK] Initializing Vulkan via XR_KHR_vulkan_enable2...\n");

	// Load XR Vulkan function pointers
	if (!LoadXrVulkanFunctions(instance)) {
		fprintf(stderr, "[VRVK] Failed to load XR Vulkan functions\n");
		return;
	}

	// Check requirements
	XrResult result = VR_Vulkan_CheckRequirements(instance, systemId);
	if (XR_FAILED(result)) {
		fprintf(stderr, "[VRVK] Failed to check Vulkan requirements\n");
		return;
	}

	// Create VkInstance via xrCreateVulkanInstanceKHR
	result = VR_Vulkan_CreateInstance(instance, systemId);
	if (XR_FAILED(result)) {
		fprintf(stderr, "[VRVK] Failed to create VkInstance\n");
		return;
	}

	// Get the physical device that OpenXR requires
	result = VR_Vulkan_GetPhysicalDevice(instance, systemId);
	if (XR_FAILED(result)) {
		fprintf(stderr, "[VRVK] Failed to get physical device\n");
		return;
	}

	// Create VkDevice via xrCreateVulkanDeviceKHR
	result = VR_Vulkan_CreateDevice(instance, systemId);
	if (XR_FAILED(result)) {
		fprintf(stderr, "[VRVK] Failed to create VkDevice\n");
		return;
	}

	fprintf(stdout, "[VRVK] Vulkan initialization complete\n");
}

void VR_Graphics_Shutdown(void)
{
	VR_Vulkan_Shutdown();
	s_requirementsFetched = VR_FALSE;
}

void VR_Graphics_InvalidateFunctionPointers(void)
{
	// Clear XR function pointers before XrInstance is destroyed.
	// These were obtained via xrGetInstanceProcAddr and become invalid
	// after xrDestroyInstance. This prevents use-after-free crashes
	// when VR_Graphics_GetRequirements is called after vid_restart.
	xrGetVulkanGraphicsRequirements2KHR = NULL;
	xrGetVulkanGraphicsDevice2KHR = NULL;
	xrCreateVulkanInstanceKHR = NULL;
	xrCreateVulkanDeviceKHR = NULL;
	s_requirementsFetched = VR_FALSE;
}
