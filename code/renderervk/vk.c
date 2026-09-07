#ifdef _WIN32
// DisplayConfig OS-HDR query needs Win7 headers; set before tr_local.h, which pins
// _WIN32_WINNT to 0x0501 via q_platform.h and would hide these types on MSVC/old MinGW.
#undef _WIN32_WINNT
#undef WINVER
#define _WIN32_WINNT 0x0601
#define WINVER 0x0601
#endif

#include "tr_local.h"
#include "vk.h"
// Types-only: VR_VulkanDeviceInfo, pulled via ri.VR_Vulkan_GetDeviceInfo().
#include "../vrvk/vr_vk.h"

#ifdef _WIN32
#include <windows.h>
#endif

// Virtual screen shape cvar (0 = curved, 1 = flat)
extern cvar_t *vr_virtualScreenShape;

#if defined (_DEBUG)
#if defined (_WIN32)
#define USE_VK_VALIDATION
#include <windows.h> // for win32 debug callback
#endif
#endif

static int vkSamples = VK_SAMPLE_COUNT_1_BIT;
static int vkMaxSamples = VK_SAMPLE_COUNT_1_BIT;

static VkInstance vk_instance = VK_NULL_HANDLE;
static VkSurfaceKHR vk_surface = VK_NULL_HANDLE;

#ifndef NDEBUG
VkDebugReportCallbackEXT vk_debug_callback = VK_NULL_HANDLE;
#endif

//
// Vulkan API functions used by the renderer.
//
static PFN_vkCreateInstance								qvkCreateInstance;
static PFN_vkEnumerateInstanceExtensionProperties		qvkEnumerateInstanceExtensionProperties;

static PFN_vkCreateDevice								qvkCreateDevice;
static PFN_vkDestroyInstance							qvkDestroyInstance;
static PFN_vkEnumerateDeviceExtensionProperties			qvkEnumerateDeviceExtensionProperties;
static PFN_vkEnumeratePhysicalDevices					qvkEnumeratePhysicalDevices;
static PFN_vkGetDeviceProcAddr							qvkGetDeviceProcAddr;
static PFN_vkGetPhysicalDeviceFeatures					qvkGetPhysicalDeviceFeatures;
static PFN_vkGetPhysicalDeviceFeatures2					qvkGetPhysicalDeviceFeatures2;
static PFN_vkGetPhysicalDeviceFormatProperties			qvkGetPhysicalDeviceFormatProperties;
static PFN_vkGetPhysicalDeviceMemoryProperties			qvkGetPhysicalDeviceMemoryProperties;
static PFN_vkGetPhysicalDeviceProperties				qvkGetPhysicalDeviceProperties;
static PFN_vkGetPhysicalDeviceQueueFamilyProperties		qvkGetPhysicalDeviceQueueFamilyProperties;
static PFN_vkDestroySurfaceKHR							qvkDestroySurfaceKHR;
static PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR	qvkGetPhysicalDeviceSurfaceCapabilitiesKHR;
static PFN_vkGetPhysicalDeviceSurfaceFormatsKHR			qvkGetPhysicalDeviceSurfaceFormatsKHR;
static PFN_vkGetPhysicalDeviceSurfacePresentModesKHR	qvkGetPhysicalDeviceSurfacePresentModesKHR;
static PFN_vkGetPhysicalDeviceSurfaceSupportKHR			qvkGetPhysicalDeviceSurfaceSupportKHR;
#ifdef USE_VK_VALIDATION
static PFN_vkCreateDebugReportCallbackEXT				qvkCreateDebugReportCallbackEXT;
static PFN_vkDestroyDebugReportCallbackEXT				qvkDestroyDebugReportCallbackEXT;
#endif
static PFN_vkAllocateCommandBuffers						qvkAllocateCommandBuffers;
static PFN_vkAllocateDescriptorSets						qvkAllocateDescriptorSets;
static PFN_vkAllocateMemory								qvkAllocateMemory;
static PFN_vkBeginCommandBuffer							qvkBeginCommandBuffer;
static PFN_vkBindBufferMemory							qvkBindBufferMemory;
static PFN_vkBindImageMemory							qvkBindImageMemory;
static PFN_vkCmdBeginRenderPass							qvkCmdBeginRenderPass;
static PFN_vkCmdBindDescriptorSets						qvkCmdBindDescriptorSets;
static PFN_vkCmdBindIndexBuffer							qvkCmdBindIndexBuffer;
static PFN_vkCmdBindPipeline							qvkCmdBindPipeline;
static PFN_vkCmdBindVertexBuffers						qvkCmdBindVertexBuffers;
static PFN_vkCmdBlitImage								qvkCmdBlitImage;
static PFN_vkCmdClearAttachments						qvkCmdClearAttachments;
static PFN_vkCmdClearColorImage							qvkCmdClearColorImage;
static PFN_vkCmdClearDepthStencilImage					qvkCmdClearDepthStencilImage;
static PFN_vkCmdCopyBuffer								qvkCmdCopyBuffer;
static PFN_vkCmdCopyBufferToImage						qvkCmdCopyBufferToImage;
static PFN_vkCmdCopyImage								qvkCmdCopyImage;
static PFN_vkCmdDraw									qvkCmdDraw;
static PFN_vkCmdDrawIndexed								qvkCmdDrawIndexed;
static PFN_vkCmdEndRenderPass							qvkCmdEndRenderPass;
static PFN_vkCmdNextSubpass								qvkCmdNextSubpass;
static PFN_vkCmdPipelineBarrier							qvkCmdPipelineBarrier;
static PFN_vkCmdPushConstants							qvkCmdPushConstants;
static PFN_vkCmdResetQueryPool							qvkCmdResetQueryPool;
static PFN_vkCmdSetDepthBias							qvkCmdSetDepthBias;
static PFN_vkCmdSetScissor								qvkCmdSetScissor;
static PFN_vkCmdSetViewport								qvkCmdSetViewport;
static PFN_vkCmdWriteTimestamp							qvkCmdWriteTimestamp;
static PFN_vkCreateBuffer								qvkCreateBuffer;
static PFN_vkCreateCommandPool							qvkCreateCommandPool;
static PFN_vkCreateDescriptorPool						qvkCreateDescriptorPool;
static PFN_vkCreateDescriptorSetLayout					qvkCreateDescriptorSetLayout;
static PFN_vkCreateFence								qvkCreateFence;
static PFN_vkCreateFramebuffer							qvkCreateFramebuffer;
static PFN_vkCreateGraphicsPipelines					qvkCreateGraphicsPipelines;
static PFN_vkCreateImage								qvkCreateImage;
static PFN_vkCreateImageView							qvkCreateImageView;
static PFN_vkCreatePipelineLayout						qvkCreatePipelineLayout;
static PFN_vkCreatePipelineCache						qvkCreatePipelineCache;
static PFN_vkCreateQueryPool							qvkCreateQueryPool;
static PFN_vkCreateRenderPass							qvkCreateRenderPass;
static PFN_vkCreateSampler								qvkCreateSampler;
static PFN_vkCreateSemaphore							qvkCreateSemaphore;
static PFN_vkCreateShaderModule							qvkCreateShaderModule;
static PFN_vkDestroyBuffer								qvkDestroyBuffer;
static PFN_vkDestroyCommandPool							qvkDestroyCommandPool;
static PFN_vkDestroyDescriptorPool						qvkDestroyDescriptorPool;
static PFN_vkDestroyDescriptorSetLayout					qvkDestroyDescriptorSetLayout;
static PFN_vkDestroyDevice								qvkDestroyDevice;
static PFN_vkDestroyFence								qvkDestroyFence;
static PFN_vkDestroyFramebuffer							qvkDestroyFramebuffer;
static PFN_vkDestroyImage								qvkDestroyImage;
static PFN_vkDestroyImageView							qvkDestroyImageView;
static PFN_vkDestroyPipeline							qvkDestroyPipeline;
static PFN_vkDestroyPipelineCache						qvkDestroyPipelineCache;
static PFN_vkDestroyPipelineLayout						qvkDestroyPipelineLayout;
static PFN_vkDestroyQueryPool							qvkDestroyQueryPool;
static PFN_vkDestroyRenderPass							qvkDestroyRenderPass;
static PFN_vkDestroySampler								qvkDestroySampler;
static PFN_vkDestroySemaphore							qvkDestroySemaphore;
static PFN_vkDestroyShaderModule						qvkDestroyShaderModule;
static PFN_vkDeviceWaitIdle								qvkDeviceWaitIdle;
static PFN_vkEndCommandBuffer							qvkEndCommandBuffer;
static PFN_vkFlushMappedMemoryRanges					qvkFlushMappedMemoryRanges;
static PFN_vkFreeCommandBuffers							qvkFreeCommandBuffers;
static PFN_vkFreeDescriptorSets							qvkFreeDescriptorSets;
static PFN_vkFreeMemory									qvkFreeMemory;
static PFN_vkGetBufferMemoryRequirements				qvkGetBufferMemoryRequirements;
static PFN_vkGetDeviceQueue								qvkGetDeviceQueue;
static PFN_vkGetFenceStatus								qvkGetFenceStatus;
static PFN_vkGetImageMemoryRequirements					qvkGetImageMemoryRequirements;
static PFN_vkGetImageSubresourceLayout					qvkGetImageSubresourceLayout;
static PFN_vkGetQueryPoolResults						qvkGetQueryPoolResults;
static PFN_vkInvalidateMappedMemoryRanges				qvkInvalidateMappedMemoryRanges;
static PFN_vkMapMemory									qvkMapMemory;
static PFN_vkQueueSubmit								qvkQueueSubmit;
static PFN_vkQueueWaitIdle								qvkQueueWaitIdle;
static PFN_vkResetCommandBuffer							qvkResetCommandBuffer;
static PFN_vkResetDescriptorPool						qvkResetDescriptorPool;
static PFN_vkResetFences								qvkResetFences;
static PFN_vkUnmapMemory								qvkUnmapMemory;
static PFN_vkUpdateDescriptorSets						qvkUpdateDescriptorSets;
static PFN_vkWaitForFences								qvkWaitForFences;
static PFN_vkAcquireNextImageKHR						qvkAcquireNextImageKHR;
static PFN_vkCreateSwapchainKHR							qvkCreateSwapchainKHR;
static PFN_vkDestroySwapchainKHR						qvkDestroySwapchainKHR;
static PFN_vkGetSwapchainImagesKHR						qvkGetSwapchainImagesKHR;
static PFN_vkQueuePresentKHR							qvkQueuePresentKHR;

static PFN_vkGetBufferMemoryRequirements2KHR			qvkGetBufferMemoryRequirements2KHR;
static PFN_vkGetImageMemoryRequirements2KHR				qvkGetImageMemoryRequirements2KHR;

static PFN_vkDebugMarkerSetObjectNameEXT				qvkDebugMarkerSetObjectNameEXT;

////////////////////////////////////////////////////////////////////////////

// forward declarations
VkPipeline create_pipeline( const Vk_Pipeline_Def *def, renderPass_t renderPassIndex, uint32_t def_index );
static qboolean vk_is_window_minimized( void );

static uint32_t find_memory_type( uint32_t memory_type_bits, VkMemoryPropertyFlags properties ) {
	VkPhysicalDeviceMemoryProperties memory_properties;
	uint32_t i;

	qvkGetPhysicalDeviceMemoryProperties( vk.physical_device, &memory_properties );

	for ( i = 0; i < memory_properties.memoryTypeCount; i++ ) {
		if ((memory_type_bits & (1 << i)) != 0 &&
			(memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}
	ri.Error( ERR_FATAL, "Vulkan: failed to find matching memory type with requested properties" );
	return ~0U;
}


static uint32_t find_memory_type2( uint32_t memory_type_bits, VkMemoryPropertyFlags properties, VkMemoryPropertyFlags *outprops ) {
	VkPhysicalDeviceMemoryProperties memory_properties;
	uint32_t i;

	qvkGetPhysicalDeviceMemoryProperties( vk.physical_device, &memory_properties );

	for ( i = 0; i < memory_properties.memoryTypeCount; i++ ) {
		if ( (memory_type_bits & (1 << i)) != 0 && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties ) {
			if ( outprops ) {
				*outprops = memory_properties.memoryTypes[i].propertyFlags;
			}
			return i;
		}
	}

	return ~0U;
}


static const char *pmode_to_str( VkPresentModeKHR mode )
{
	static char buf[32];

	switch ( mode ) {
		case VK_PRESENT_MODE_IMMEDIATE_KHR: return "IMMEDIATE";
		case VK_PRESENT_MODE_MAILBOX_KHR: return "MAILBOX";
		case VK_PRESENT_MODE_FIFO_KHR: return "FIFO";
		case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return "FIFO_RELAXED";
		case VK_PRESENT_MODE_FIFO_LATEST_READY_EXT: return "FIFO_LATEST_READY";
		default: sprintf( buf, "mode#%x", mode ); return buf;
	};
}


#define CASE_STR(x) case (x): return #x

const char *vk_format_string( VkFormat format )
{
	static char buf[16];

	switch ( format ) {
		// color formats
		CASE_STR( VK_FORMAT_R5G5B5A1_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_B5G5R5A1_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_R5G6B5_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_B5G6R5_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_B8G8R8A8_SRGB );
		CASE_STR( VK_FORMAT_R8G8B8A8_SRGB );
		CASE_STR( VK_FORMAT_B8G8R8A8_SNORM );
		CASE_STR( VK_FORMAT_R8G8B8A8_SNORM );
		CASE_STR( VK_FORMAT_B8G8R8A8_UNORM );
		CASE_STR( VK_FORMAT_R8G8B8A8_UNORM );
		CASE_STR( VK_FORMAT_B4G4R4A4_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_R4G4B4A4_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_R16G16B16A16_UNORM );
		CASE_STR( VK_FORMAT_A2B10G10R10_UNORM_PACK32 );
		CASE_STR( VK_FORMAT_A2R10G10B10_UNORM_PACK32 );
		CASE_STR( VK_FORMAT_B10G11R11_UFLOAT_PACK32 );
		// depth formats
		CASE_STR( VK_FORMAT_D16_UNORM );
		CASE_STR( VK_FORMAT_D16_UNORM_S8_UINT );
		CASE_STR( VK_FORMAT_X8_D24_UNORM_PACK32 );
		CASE_STR( VK_FORMAT_D24_UNORM_S8_UINT );
		CASE_STR( VK_FORMAT_D32_SFLOAT );
		CASE_STR( VK_FORMAT_D32_SFLOAT_S8_UINT );
	default:
		Com_sprintf( buf, sizeof( buf ), "#%i", format );
		return buf;
	}
}


static const char *vk_result_string( VkResult code ) {
	static char buffer[32];

	switch ( code ) {
		CASE_STR( VK_SUCCESS );
		CASE_STR( VK_NOT_READY );
		CASE_STR( VK_TIMEOUT );
		CASE_STR( VK_EVENT_SET );
		CASE_STR( VK_EVENT_RESET );
		CASE_STR( VK_INCOMPLETE );
		CASE_STR( VK_ERROR_OUT_OF_HOST_MEMORY );
		CASE_STR( VK_ERROR_OUT_OF_DEVICE_MEMORY );
		CASE_STR( VK_ERROR_INITIALIZATION_FAILED );
		CASE_STR( VK_ERROR_DEVICE_LOST );
		CASE_STR( VK_ERROR_MEMORY_MAP_FAILED );
		CASE_STR( VK_ERROR_LAYER_NOT_PRESENT );
		CASE_STR( VK_ERROR_EXTENSION_NOT_PRESENT );
		CASE_STR( VK_ERROR_FEATURE_NOT_PRESENT );
		CASE_STR( VK_ERROR_INCOMPATIBLE_DRIVER );
		CASE_STR( VK_ERROR_TOO_MANY_OBJECTS );
		CASE_STR( VK_ERROR_FORMAT_NOT_SUPPORTED );
		CASE_STR( VK_ERROR_FRAGMENTED_POOL );
		CASE_STR( VK_ERROR_UNKNOWN );
		CASE_STR( VK_ERROR_OUT_OF_POOL_MEMORY );
		CASE_STR( VK_ERROR_INVALID_EXTERNAL_HANDLE );
		CASE_STR( VK_ERROR_FRAGMENTATION );
		CASE_STR( VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS );
		CASE_STR( VK_ERROR_SURFACE_LOST_KHR );
		CASE_STR( VK_ERROR_NATIVE_WINDOW_IN_USE_KHR );
		CASE_STR( VK_SUBOPTIMAL_KHR );
		CASE_STR( VK_ERROR_OUT_OF_DATE_KHR );
		CASE_STR( VK_ERROR_INCOMPATIBLE_DISPLAY_KHR );
		CASE_STR( VK_ERROR_VALIDATION_FAILED_EXT );
		CASE_STR( VK_ERROR_INVALID_SHADER_NV );
		CASE_STR( VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT );
		CASE_STR( VK_ERROR_NOT_PERMITTED_EXT );
		CASE_STR( VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT );
		CASE_STR( VK_THREAD_IDLE_KHR );
		CASE_STR( VK_THREAD_DONE_KHR );
		CASE_STR( VK_OPERATION_DEFERRED_KHR );
		CASE_STR( VK_OPERATION_NOT_DEFERRED_KHR );
		CASE_STR( VK_PIPELINE_COMPILE_REQUIRED_EXT );
	default:
		sprintf( buffer, "code %i", code );
		return buffer;
	}
}
#undef CASE_STR

#define VK_CHECK( function_call ) { \
	VkResult res = function_call; \
	if ( res < 0 ) { \
		ri.Error( ERR_FATAL, "Vulkan: %s returned %s", #function_call, vk_result_string( res ) ); \
	} \
}

// Forward declarations
static VkFormat vk_get_unorm_format( VkFormat format );


/*
static VkFlags get_composite_alpha( VkCompositeAlphaFlagsKHR flags )
{
	const VkCompositeAlphaFlagBitsKHR compositeFlags[] = {
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
	};
	int i;

	for ( i = 1; i < ARRAY_LEN( compositeFlags ); i++ ) {
		if ( flags & compositeFlags[i] ) {
			return compositeFlags[i];
		}
	}

	return compositeFlags[0];
}
*/


static VkCommandBuffer begin_command_buffer( void )
{
	VkCommandBufferBeginInfo begin_info;
	VkCommandBufferAllocateInfo alloc_info;
	VkCommandBuffer command_buffer;

	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.commandPool = vk.command_pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = 1;
	VK_CHECK( qvkAllocateCommandBuffers( vk.device, &alloc_info, &command_buffer ) );

	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.pNext = NULL;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	begin_info.pInheritanceInfo = NULL;

	VK_CHECK( qvkBeginCommandBuffer( command_buffer, &begin_info ) );

	return command_buffer;
}


static void end_command_buffer( VkCommandBuffer command_buffer, const char *location )
{
#ifdef USE_UPLOAD_QUEUE
	const VkPipelineStageFlags wait_dst_stage_mask = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	VkSemaphore waits;
#endif
	VkSubmitInfo submit_info;
	VkCommandBuffer cmdbuf[1];

	cmdbuf[0] = command_buffer;

	VK_CHECK( qvkEndCommandBuffer( command_buffer ) );

	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = NULL;
#ifdef USE_UPLOAD_QUEUE
	if ( vk.rendering_finished != VK_NULL_HANDLE ) {
		waits = vk.rendering_finished;
		vk.rendering_finished = VK_NULL_HANDLE;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &waits;
		submit_info.pWaitDstStageMask = &wait_dst_stage_mask;
	} else 
#endif
	{
		submit_info.waitSemaphoreCount = 0;
		submit_info.pWaitSemaphores = NULL;
		submit_info.pWaitDstStageMask = NULL;
	}

	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = cmdbuf;
	submit_info.signalSemaphoreCount = 0;
	submit_info.pSignalSemaphores = NULL;

	VK_CHECK( qvkQueueSubmit( vk.queue, 1, &submit_info, VK_NULL_HANDLE ) );

	vk_queue_wait_idle();

	qvkFreeCommandBuffers( vk.device, vk.command_pool, 1, cmdbuf );
}


// Forward declaration for virtual screen rendering (defined later in file)
static void vk_render_virtual_screen( float eyeProj[2][16], float screenMV[16], float floorMV[16] );

// Forward declaration for record_image_layout_transition
static void record_image_layout_transition( VkCommandBuffer command_buffer, VkImage image, VkImageAspectFlags image_aspect_flags,
	VkImageLayout old_layout, VkImageLayout new_layout, uint32_t src_stage_override, uint32_t dst_stage_override );


static void record_image_layout_transition( VkCommandBuffer command_buffer, VkImage image, VkImageAspectFlags image_aspect_flags,
	VkImageLayout old_layout, VkImageLayout new_layout, uint32_t src_stage_override, uint32_t dst_stage_override ) {
	VkImageMemoryBarrier barrier;
	uint32_t src_stage, dst_stage;

	switch ( old_layout ) {
		case VK_IMAGE_LAYOUT_UNDEFINED:
			if ( src_stage_override != 0 )
				src_stage = src_stage_override;
			else
				src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			barrier.srcAccessMask = VK_ACCESS_NONE;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			src_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.srcAccessMask = VK_ACCESS_NONE;
			break;
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			src_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			break;
		default:
			ri.Error( ERR_DROP, "unsupported old layout %i", old_layout );
			src_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			barrier.srcAccessMask = VK_ACCESS_NONE;
			break;
	}

	switch ( new_layout ) {
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.dstAccessMask = VK_ACCESS_NONE;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
			break;
		default:
			ri.Error( ERR_DROP, "unsupported new layout %i", new_layout);
			dst_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			barrier.dstAccessMask = VK_ACCESS_NONE;
			break;
	}


	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.pNext = NULL;
	//barrier.srcAccessMask = src_access_flags;
	//barrier.dstAccessMask = dst_access_flags;
	barrier.oldLayout = old_layout;
	barrier.newLayout = new_layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = image_aspect_flags;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	qvkCmdPipelineBarrier( command_buffer, src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1, &barrier );
}


// debug markers
#define SET_OBJECT_NAME(obj,objName,objType) vk_set_object_name( (uint64_t)(obj), (objName), (objType) )

static void vk_set_object_name( uint64_t obj, const char *objName, VkDebugReportObjectTypeEXT objType )
{
	if ( qvkDebugMarkerSetObjectNameEXT && obj )
	{
		VkDebugMarkerObjectNameInfoEXT info;
		info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
		info.pNext = NULL;
		info.objectType = objType;
		info.object = obj;
		info.pObjectName = objName;
		qvkDebugMarkerSetObjectNameEXT( vk.device, &info );
	}
}


static void vk_create_swapchain( VkPhysicalDevice physical_device, VkDevice device, VkSurfaceKHR surface, VkSurfaceFormatKHR surface_format, VkSwapchainKHR *swapchain, qboolean verbose ) {
	VkSurfaceCapabilitiesKHR surface_caps;
	VkExtent2D image_extent;
	uint32_t present_mode_count, i;
	VkPresentModeKHR present_mode;
	VkPresentModeKHR *present_modes;
	uint32_t image_count;
	VkSwapchainCreateInfoKHR desc;
	qboolean mailbox_supported = qfalse;
	qboolean immediate_supported = qfalse;
	qboolean fifo_relaxed_supported = qfalse;
	int v;

	VK_CHECK( qvkGetPhysicalDeviceSurfaceCapabilitiesKHR( physical_device, surface, &surface_caps ) );

	image_extent = surface_caps.currentExtent;
	if ( image_extent.width == 0xffffffff && image_extent.height == 0xffffffff ) {
		image_extent.width = MIN( surface_caps.maxImageExtent.width, MAX( surface_caps.minImageExtent.width, (uint32_t) glConfig.vidWidth ) );
		image_extent.height = MIN( surface_caps.maxImageExtent.height, MAX( surface_caps.minImageExtent.height, (uint32_t) glConfig.vidHeight ) );
	}

	// Validate extent - can be 0x0 when window is minimized or in invalid state
	if ( image_extent.width == 0 || image_extent.height == 0 ) {
		if ( verbose ) {
			ri.Printf( PRINT_WARNING, "Invalid swapchain extent %ux%u, deferring creation\n",
				image_extent.width, image_extent.height );
		}
		*swapchain = VK_NULL_HANDLE;
		return;
	}

	// Store actual desktop swapchain dimensions for mirror blitting
	vk.desktopWidth = image_extent.width;
	vk.desktopHeight = image_extent.height;

	vk.clearAttachment = qtrue;
	// Note: fboActive is always true in VR, so no need to check swapchain usage flags here

	// determine present mode and swapchain image count
	VK_CHECK(qvkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, NULL));

	present_modes = (VkPresentModeKHR *) ri.Malloc( present_mode_count * sizeof( VkPresentModeKHR ) );
	VK_CHECK(qvkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, present_modes));

	if ( verbose ) {
		ri.Printf( PRINT_ALL, "...presentation modes:" );
	}
	for ( i = 0; i < present_mode_count; i++ ) {
		if ( verbose ) {
			ri.Printf( PRINT_ALL, " %s", pmode_to_str( present_modes[i] ) );
		}
		if ( present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR )
			mailbox_supported = qtrue;
		else if ( present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR )
			immediate_supported = qtrue;
		else if ( present_modes[i] == VK_PRESENT_MODE_FIFO_RELAXED_KHR )
			fifo_relaxed_supported = qtrue;
	}
	if ( verbose ) {
		ri.Printf( PRINT_ALL, "\n" );
	}

	ri.Free( present_modes );

	if ( ( v = ri.Cvar_VariableIntegerValue( "r_swapInterval" ) ) != 0 ) {
		if ( v == 2 && mailbox_supported )
			present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
		else if ( fifo_relaxed_supported )
			present_mode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
		else
			present_mode = VK_PRESENT_MODE_FIFO_KHR;
		image_count = MAX( MIN_SWAPCHAIN_IMAGES_FIFO, surface_caps.minImageCount );
	} else {
		if ( immediate_supported ) {
			present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
			image_count = MAX( MIN_SWAPCHAIN_IMAGES_IMM, surface_caps.minImageCount );
		} else if ( mailbox_supported ) {
			present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
			image_count = MAX( MIN_SWAPCHAIN_IMAGES_MAILBOX, surface_caps.minImageCount );
		} else if ( fifo_relaxed_supported ) {
			present_mode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
			image_count = MAX( MIN_SWAPCHAIN_IMAGES_FIFO, surface_caps.minImageCount );
		} else {
			present_mode = VK_PRESENT_MODE_FIFO_KHR;
			image_count = MAX( MIN_SWAPCHAIN_IMAGES_FIFO, surface_caps.minImageCount );
		}
	}

	if ( image_count < 2 ) {
		image_count = 2;
	}

	if ( surface_caps.maxImageCount == 0 && present_mode == VK_PRESENT_MODE_FIFO_KHR ) {
		image_count = MAX( MIN_SWAPCHAIN_IMAGES_FIFO_0, surface_caps.minImageCount );
	} else if ( surface_caps.maxImageCount > 0 ) {
		image_count = MIN( MIN( image_count, surface_caps.maxImageCount ), MAX_SWAPCHAIN_IMAGES );
	}

	if ( verbose ) {
		ri.Printf( PRINT_ALL, "...selected presentation mode: %s, image count: %i\n", pmode_to_str( present_mode ), image_count );
	}

	// create swap chain
	desc.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.surface = surface;
	desc.minImageCount = image_count;
	desc.imageFormat = surface_format.format;
	desc.imageColorSpace = surface_format.colorSpace;
	desc.imageExtent = image_extent;
	desc.imageArrayLayers = 1;
	desc.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	// Always add transfer bits, needed for:
	// - TRANSFER_DST_BIT: vkCmdClearColorImage (r_clear) and blit destination (VR mirror)
	// - TRANSFER_SRC_BIT: screenshots
	desc.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	desc.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;
	desc.preTransform = surface_caps.currentTransform;
	//desc.compositeAlpha = get_composite_alpha( surface_caps.supportedCompositeAlpha );
	desc.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	desc.presentMode = present_mode;
	desc.clipped = VK_TRUE;
	desc.oldSwapchain = VK_NULL_HANDLE;

	ri.Printf( PRINT_ALL, "...creating swapchain: %dx%d, format %d\n", image_extent.width, image_extent.height, surface_format.format );
	ri.Printf( PRINT_ALL, "   surface=%p, device=%p, queueFamily=%d\n", (void*)surface, (void*)device, vk.queue_family_index );
	fflush( stdout );
	fflush( stderr );
	if ( qvkCreateSwapchainKHR == NULL ) {
		ri.Error( ERR_FATAL, "qvkCreateSwapchainKHR is NULL!" );
		return;
	}
	{
		VkResult swapResult = qvkCreateSwapchainKHR( device, &desc, NULL, swapchain );
		ri.Printf( PRINT_ALL, "...swapchain creation returned: %d\n", swapResult );
		fflush( stdout );
		if ( swapResult != VK_SUCCESS ) {
			ri.Error( ERR_FATAL, "vkCreateSwapchainKHR failed: %s", vk_result_string( swapResult ) );
			return;
		}
	}

	ri.Printf( PRINT_ALL, "...getting swapchain images\n" );
	fflush( stdout );
	VK_CHECK( qvkGetSwapchainImagesKHR( vk.device, vk.swapchain, &vk.swapchain_image_count, NULL ) );
	vk.swapchain_image_count = MIN( vk.swapchain_image_count, MAX_SWAPCHAIN_IMAGES );
	VK_CHECK( qvkGetSwapchainImagesKHR( vk.device, vk.swapchain, &vk.swapchain_image_count, vk.swapchain_images ) );
	ri.Printf( PRINT_ALL, "...got %d swapchain images\n", vk.swapchain_image_count );
	fflush( stdout );

	for ( i = 0; i < vk.swapchain_image_count; i++ ) {
		SET_OBJECT_NAME( vk.swapchain_images[i], va( "swapchain image %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	}

	ri.Printf( PRINT_ALL, "...creating swapchain semaphores\n" );
	fflush( stdout );
	for ( i = 0; i < vk.swapchain_image_count; i++ ) {
		VkSemaphoreCreateInfo s;
		s.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		s.pNext = NULL;
		s.flags = 0;
		VK_CHECK( qvkCreateSemaphore( vk.device, &s, NULL, &vk.swapchain_rendering_finished[i] ) );
		SET_OBJECT_NAME( vk.swapchain_rendering_finished[i], va( "swapchain_rendering_finished semaphore %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT );
	}
	ri.Printf( PRINT_ALL, "...swapchain semaphores created\n" );
	fflush( stdout );

	if ( vk.initSwapchainLayout != VK_IMAGE_LAYOUT_UNDEFINED ) {
		VkCommandBuffer command_buffer = begin_command_buffer();

		for ( i = 0; i < vk.swapchain_image_count; i++ ) {
			record_image_layout_transition( command_buffer, vk.swapchain_images[i],
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED, vk.initSwapchainLayout, 0, 0 );
		}

		end_command_buffer( command_buffer, __func__ );
	}
	ri.Printf( PRINT_ALL, "...swapchain creation complete\n" );
	fflush( stdout );
}


static void vk_create_render_passes( void )
{
	VkAttachmentDescription attachments[5]; // color | depth | msaa color | emissive resolve | emissive msaa
	VkAttachmentReference colorResolveRef = { 0 };
	VkAttachmentReference colorResolveRefs[2];
	VkAttachmentReference colorRef0;
	VkAttachmentReference colorRefs[2];
	VkAttachmentReference depthRef0;
	VkSubpassDescription subpass;
	VkSubpassDependency deps[3];
	VkRenderPassCreateInfo desc;
	VkFormat depth_format;
	VkDevice device;
	uint32_t i;

	depth_format = vk.depth_format;
	device = vk.device;

	// Common subpass dependencies: used by all render passes
	Com_Memset( &deps, 0, sizeof( deps ) );

	// deps[0]: External -> subpass 0 (wait for previous operations before color/depth output)
	// Includes depth stages for mainResume which uses LOAD_OP_LOAD on depth after HUD pass
	deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	deps[0].dstSubpass = 0;
	deps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
	                        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
	                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT |
	                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
	                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
	                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
	                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	deps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	// deps[1]: Subpass 0 -> external (wait for color/depth writes before shader reads)
	deps[1].srcSubpass = 0;
	deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
	                        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
	                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	deps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	// deps[2]: External -> subpass 0 (color output ordering, used by gamma/bloom passes)
	deps[2].srcSubpass = VK_SUBPASS_EXTERNAL;
	deps[2].dstSubpass = 0;
	deps[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	deps[2].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	deps[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
	deps[2].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	deps[2].dependencyFlags = 0;

	// Screenmap render pass: only needed for FBO mode (r_fbo=1)
	// In non-FBO mode, we skip directly to XR multiview passes
	if ( r_fbo->integer )
	{
		desc.dependencyCount = 2;
		desc.pDependencies = &deps[0];

		// screenmap resolve/color buffer
		attachments[0].flags = 0;
		attachments[0].format = vk.color_format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
#ifdef USE_BUFFER_CLEAR
		if ( vk.screenMapSamples > VK_SAMPLE_COUNT_1_BIT )
			attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		else
			attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
#else
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // Assuming this will be completely overwritten
#endif
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;   // needed for next render pass
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		// screenmap depth buffer
		attachments[1].flags = 0;
		attachments[1].format = depth_format;
		attachments[1].samples = vk.screenMapSamples;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Need empty depth buffer before use
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		colorRef0.attachment = 0;
		colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		depthRef0.attachment = 1;
		depthRef0.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		Com_Memset( &subpass, 0, sizeof( subpass ) );
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef0;
		subpass.pDepthStencilAttachment = &depthRef0;

		Com_Memset( &desc, 0, sizeof( desc ) );
		desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.pAttachments = attachments;
		desc.pSubpasses = &subpass;
		desc.subpassCount = 1;
		desc.attachmentCount = 2;
		desc.dependencyCount = 2;
		desc.pDependencies = deps;

		if ( vk.screenMapSamples > VK_SAMPLE_COUNT_1_BIT ) {

			attachments[2].flags = 0;
			attachments[2].format = vk.color_format;
			attachments[2].samples = vk.screenMapSamples;
#ifdef USE_BUFFER_CLEAR
			attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
#else
			attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
#endif
			attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[2].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			attachments[2].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			desc.attachmentCount = 3;

			colorRef0.attachment = 2; // screenmap msaa image attachment
			colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			colorResolveRef.attachment = 0; // screenmap resolve image attachment
			colorResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			subpass.pResolveAttachments = &colorResolveRef;
		}

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.screenmap ) );

		SET_OBJECT_NAME( vk.render_pass.screenmap, "render pass - screenmap", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
	}
	/*
	 * XR Multiview Render Passes (VK_KHR_multiview)
	 *
	 * Main pass attachments:
	 *   [0] Color resolve target (1x samples) - STORE for gamma pass
	 *   [1] Depth (multisampled if MSAA) - STORE for mainResume after HUD
	 *   [2] MSAA color (only if MSAA active) - STORE for mainResume
	 *
	 * Uses reversed depth (near=1.0, far=0.0) for precision.
	 */
	ri.Printf( PRINT_ALL, "Checking multiview support: %d\n", vk.multiviewSupported );
	if ( vk.multiviewSupported )
	{
		ri.Printf( PRINT_ALL, "Creating XR multiview render passes (MSAA: %s)...\n",
			vk.msaaActive ? "yes" : "no" );
		VkRenderPassMultiviewCreateInfo multiviewInfo;
		uint32_t viewMask = 0b11;        // Both eyes (views 0 and 1)
		uint32_t correlationMask = 0b11; // Optimization: views are correlated

		Com_Memset( &multiviewInfo, 0, sizeof( multiviewInfo ) );
		multiviewInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO;
		multiviewInfo.subpassCount = 1;
		multiviewInfo.pViewMasks = &viewMask;
		multiviewInfo.correlationMaskCount = 1;
		multiviewInfo.pCorrelationMasks = &correlationMask;

		// Attachment 0: Color resolve target
		attachments[0].flags = 0;
		attachments[0].format = vk.color_format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		if ( vk.msaaActive ) {
			attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // Written by resolve
		} else {
#ifdef USE_BUFFER_CLEAR
			attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
#else
			attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
#endif
		}
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// Attachment 1: Depth
		attachments[1].flags = 0;
		attachments[1].format = depth_format;
		attachments[1].samples = vk.msaaActive ? vkSamples : VK_SAMPLE_COUNT_1_BIT;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		// Must STORE at every sample count: post_bloom and mainResume LOAD depth
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[1].stencilLoadOp = glConfig.stencilBits ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		colorRef0.attachment = 0;
		colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		depthRef0.attachment = 1;
		depthRef0.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		Com_Memset( &subpass, 0, sizeof( subpass ) );
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef0;
		subpass.pDepthStencilAttachment = &depthRef0;

		Com_Memset( &desc, 0, sizeof( desc ) );
		desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		desc.pNext = &multiviewInfo;
		desc.flags = 0;
		desc.pAttachments = attachments;
		desc.attachmentCount = 2;
		desc.pSubpasses = &subpass;
		desc.subpassCount = 1;
		desc.dependencyCount = 2;
		desc.pDependencies = deps;

		if ( vk.msaaActive )
		{
			// Attachment 2: MSAA color (render target, resolves to [0])
			attachments[2].flags = 0;
			attachments[2].format = vk.color_format;
			attachments[2].samples = vkSamples;
#ifdef USE_BUFFER_CLEAR
			attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
#else
			attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
#endif
			attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE; // Needed for mainResume
			attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachments[2].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			desc.attachmentCount = 3;
			colorRef0.attachment = 2;         // Render to MSAA
			colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			colorResolveRef.attachment = 0;   // Resolve to non-MSAA
			colorResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			subpass.pResolveAttachments = &colorResolveRef;

			if ( vk.hdrActive )
			{
				// emissive resolve target (sampled by the gamma/mirror pass)
				attachments[3].flags = 0;
				attachments[3].format = VK_FORMAT_R16G16B16A16_SFLOAT;
				attachments[3].samples = VK_SAMPLE_COUNT_1_BIT;
				attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				attachments[3].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				attachments[3].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				attachments[3].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				attachments[3].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				attachments[3].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				// msaa emissive render target
				attachments[4].flags = 0;
				attachments[4].format = VK_FORMAT_R16G16B16A16_SFLOAT;
				attachments[4].samples = vkSamples;
				attachments[4].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				attachments[4].storeOp = VK_ATTACHMENT_STORE_OP_STORE; // Needed for mainResume
				attachments[4].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				attachments[4].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				attachments[4].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				attachments[4].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

				colorRefs[0] = colorRef0;       // attachment 2 (msaa color)
				colorRefs[1].attachment = 4;    // msaa emissive render target
				colorRefs[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				subpass.colorAttachmentCount = 2;
				subpass.pColorAttachments = colorRefs;

				colorResolveRefs[0] = colorResolveRef; // attachment 0 (color resolve)
				colorResolveRefs[1].attachment = 3;    // emissive resolve
				colorResolveRefs[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				subpass.pResolveAttachments = colorResolveRefs;

				desc.attachmentCount = 5;
			}
		}

		if ( vk.hdrActive && !vk.msaaActive )
		{
			// emissive resolve: rendered directly as 2nd color attachment (no MSAA)
			attachments[2].flags = 0;
			attachments[2].format = VK_FORMAT_R16G16B16A16_SFLOAT;
			attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			// mirror the color attachment so the shared post-bloom pass can LOAD it
			attachments[2].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			attachments[2].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			colorRefs[0] = colorRef0;
			colorRefs[1].attachment = 2;
			colorRefs[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			subpass.colorAttachmentCount = 2;
			subpass.pColorAttachments = colorRefs;

			desc.attachmentCount = 3;
		}

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.main ) );
		SET_OBJECT_NAME( vk.render_pass.main, "render pass - XR main (multiview)", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
		ri.Printf( PRINT_ALL, "Created main render pass: %p (attachments: %d)\n", (void*)vk.render_pass.main, desc.attachmentCount );

		// mainResume: same as main but LOAD_OP_LOAD to preserve content after HUD pass
		// initialLayout must NOT be UNDEFINED with LOAD_OP_LOAD
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		if ( vk.msaaActive ) {
			// MSAA: main pass stored depth, so we can LOAD it
			attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			attachments[1].stencilLoadOp = glConfig.stencilBits ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attachments[2].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		} else {
			// Non-MSAA: main pass stored depth, so we can LOAD it
			attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			attachments[1].stencilLoadOp = glConfig.stencilBits ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		}

		if ( vk.hdrActive )
		{
			// Preserve emissive energy accumulated in the main pass
			if ( vk.msaaActive )
			{
				attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // emissive resolve
				attachments[4].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // msaa emissive
			}
			else
			{
				attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // emissive resolve
			}
		}

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.mainResume ) );
		SET_OBJECT_NAME( vk.render_pass.mainResume, "render pass - XR main resume (multiview)", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
		ri.Printf( PRINT_ALL, "Created mainResume render pass: %p\n", (void*)vk.render_pass.mainResume );

		// Reset for non-MSAA passes
		subpass.pResolveAttachments = NULL;
		colorRef0.attachment = 0;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef0;

		// Gamma pass: outputs to XR swapchain (uses UNORM format to bypass automatic sRGB conversion)
		// The gamma shader outputs sRGB-encoded values directly
		attachments[0].flags = 0;
		attachments[0].format = vk_get_unorm_format( vk.color_format );
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		desc.attachmentCount = 1;
		subpass.pDepthStencilAttachment = NULL;
		desc.dependencyCount = 1;
		desc.pDependencies = &deps[2];

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.gamma ) );
		SET_OBJECT_NAME( vk.render_pass.gamma, "render pass - XR gamma (multiview)", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

		// Bloom passes (multiview)
		attachments[0].format = vk.bloom_format;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.bloom_extract ) );
		SET_OBJECT_NAME( vk.render_pass.bloom_extract, "render pass - XR bloom extract (multiview)", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

		for ( i = 0; i < ARRAY_LEN( vk.render_pass.blur ); i++ ) {
			VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.blur[i] ) );
			SET_OBJECT_NAME( vk.render_pass.blur[i], va( "render pass - XR blur %i (multiview)", i ), VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
		}

		// Post-bloom: blends bloom back into FBO color
		if ( vk.msaaActive ) {
			// MSAA: [0]=resolve, [1]=depth, [2]=MSAA color
			attachments[0].format = vk.color_format;
			attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			attachments[1].format = depth_format;
			attachments[1].samples = vkSamples;
			attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;  // Store for mainResume after HUD
			attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;  // Store for mainResume after HUD
			attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			attachments[2].format = vk.color_format;
			attachments[2].samples = vkSamples;
			attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;  // Store for mainResume after HUD
			attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[2].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			attachments[2].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			desc.attachmentCount = 3;
			desc.dependencyCount = 2;
			desc.pDependencies = deps;
			colorRef0.attachment = 2;
			colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			colorResolveRef.attachment = 0;
			colorResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			subpass.pDepthStencilAttachment = &depthRef0;
			subpass.pResolveAttachments = &colorResolveRef;
		} else {
			// Non-MSAA: color + depth (for pipeline compatibility with RENDER_PASS_MAIN)
			// RENDER_PASS_MAIN has 2 attachments in non-MSAA, so post_bloom must match
			attachments[0].format = vk.color_format;
			attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			attachments[1].format = depth_format;
			attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;  // Store for mainResume after HUD
			attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			desc.attachmentCount = 2;
			desc.dependencyCount = 2;
			desc.pDependencies = deps;
			subpass.pDepthStencilAttachment = &depthRef0;
		}

		if ( vk.hdrActive )
		{
			if ( vk.msaaActive )
			{
				// Keep the emitter energy the main pass accumulated
				attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;  // emissive resolve
				attachments[3].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				attachments[4].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;  // msaa emissive
				attachments[4].storeOp = VK_ATTACHMENT_STORE_OP_STORE;

				colorRefs[0] = colorRef0;       // attachment 2 (msaa color)
				colorRefs[1].attachment = 4;    // msaa emissive render target
				colorRefs[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				subpass.colorAttachmentCount = 2;
				subpass.pColorAttachments = colorRefs;

				colorResolveRefs[0] = colorResolveRef; // attachment 0 (color resolve)
				colorResolveRefs[1].attachment = 3;    // emissive resolve
				colorResolveRefs[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				subpass.pResolveAttachments = colorResolveRefs;

				desc.attachmentCount = 5;
			}
			else
			{
				// Emissive resolve: LOAD what main wrote, STORE for gamma/mirror pass
				attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
				attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;

				colorRefs[0] = colorRef0;
				colorRefs[1].attachment = 2;
				colorRefs[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				subpass.colorAttachmentCount = 2;
				subpass.pColorAttachments = colorRefs;

				desc.attachmentCount = 3;
			}
		}

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.post_bloom ) );
		SET_OBJECT_NAME( vk.render_pass.post_bloom, "render pass - XR post bloom (multiview)", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

		ri.Printf( PRINT_ALL, "...XR multiview render passes created\n" );
	}

	/*
	 * HUD Buffer Render Pass (1280x960, single layer, no multiview)
	 * Used for HUD mode 1 (in-world sprite). Depth needed for 3D models.
	 * [0] Color: LOAD preserves content (1-frame latency)
	 * [1] Depth: CLEAR each frame, STORE ensures clear completes
	 */
	{
		VkSubpassDependency hudDeps[2];
		VkAttachmentReference hudDepthRef;

		ri.Printf( PRINT_ALL, "Creating HUD buffer render pass (color_format=0x%x, depth_format=0x%x)...\n",
			vk.color_format, depth_format );

		attachments[0].flags = 0;
		attachments[0].format = vk.color_format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		attachments[1].flags = 0;
		attachments[1].format = depth_format;
		attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		colorRef0.attachment = 0;
		colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		hudDepthRef.attachment = 1;
		hudDepthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		Com_Memset( &subpass, 0, sizeof( subpass ) );
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef0;
		subpass.pDepthStencilAttachment = &hudDepthRef;

		// Dependencies: wait for sampling before load, complete writes before sampling
		hudDeps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		hudDeps[0].dstSubpass = 0;
		hudDeps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		hudDeps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		hudDeps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		hudDeps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		hudDeps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		hudDeps[1].srcSubpass = 0;
		hudDeps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		hudDeps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
		                          VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
		                          VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		hudDeps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		hudDeps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
		                           VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		hudDeps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		hudDeps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		Com_Memset( &desc, 0, sizeof( desc ) );
		desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.pAttachments = attachments;
		desc.attachmentCount = 2;
		desc.pSubpasses = &subpass;
		desc.subpassCount = 1;
		desc.dependencyCount = 2;
		desc.pDependencies = hudDeps;

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.hudBuffer ) );
		SET_OBJECT_NAME( vk.render_pass.hudBuffer, "render pass - HUD buffer", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

		ri.Printf( PRINT_ALL, "...HUD buffer render pass created\n" );
	}
}


static void allocate_and_bind_image_memory(VkImage image) {
	VkMemoryRequirements memory_requirements;
	VkDeviceSize alignment;
	ImageChunk *chunk;
	int i;

	qvkGetImageMemoryRequirements(vk.device, image, &memory_requirements);

	if ( memory_requirements.size > vk.image_chunk_size ) {
		ri.Error( ERR_FATAL, "Vulkan: could not allocate memory, image is too large (%ikbytes).",
			(int)(memory_requirements.size/1024) );
	}

	chunk = NULL;

	// Try to find an existing chunk of sufficient capacity.
	alignment = memory_requirements.alignment;
	for ( i = 0; i < vk_world.num_image_chunks; i++ ) {
		// ensure that memory region has proper alignment
		VkDeviceSize offset = PAD( vk_world.image_chunks[i].used, alignment );

		if ( offset + memory_requirements.size <= vk.image_chunk_size ) {
			chunk = &vk_world.image_chunks[i];
			chunk->used = offset + memory_requirements.size;
			break;
		}
	}

	// Allocate a new chunk in case we couldn't find suitable existing chunk.
	if (chunk == NULL) {
		VkMemoryAllocateInfo alloc_info;
		VkDeviceMemory memory;

		if (vk_world.num_image_chunks >= MAX_IMAGE_CHUNKS) {
			ri.Error(ERR_FATAL, "Vulkan: image chunk limit has been reached" );
		}

		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.pNext = NULL;
		alloc_info.allocationSize = vk.image_chunk_size;
		alloc_info.memoryTypeIndex = find_memory_type( memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

		VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &memory ) );

		chunk = &vk_world.image_chunks[vk_world.num_image_chunks];
		chunk->memory = memory;
		chunk->used = memory_requirements.size;

		SET_OBJECT_NAME( memory, va( "image memory chunk %i", vk_world.num_image_chunks ), VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT );

		vk_world.num_image_chunks++;
	}

	VK_CHECK(qvkBindImageMemory(vk.device, image, chunk->memory, chunk->used - memory_requirements.size));
}


static void vk_clean_staging_buffer( void )
{
	if ( vk.staging_buffer.handle != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.staging_buffer.handle, NULL );
		vk.staging_buffer.handle = VK_NULL_HANDLE;
	}

	//if ( vk.staging_buffer.ptr != NULL ) 
	//	qvkUnmapMemory( vk.device, vk.staging_buffer.memory ) {
	//	vk.staging_buffer.ptr = NULL;
	//}

	if ( vk.staging_buffer.memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.staging_buffer.memory, NULL );
		vk.staging_buffer.memory = VK_NULL_HANDLE;
	}

	vk.staging_buffer.ptr = NULL;
	vk.staging_buffer.size = 0;
#ifdef USE_UPLOAD_QUEUE
	vk.staging_buffer.offset = 0;
#endif
}


#ifdef USE_UPLOAD_QUEUE
static qboolean vk_wait_staging_buffer( void )
{
	if ( vk.aux_fence_wait ) {
		VkResult res = qvkWaitForFences( vk.device, 1, &vk.aux_fence, VK_TRUE, 5 * 1000000000ULL );
		if ( res != VK_SUCCESS ) {
			ri.Error( ERR_FATAL, "vkWaitForFences() failed with %s at %s", vk_result_string( res ), __func__ );
		}
		qvkResetFences( vk.device, 1, &vk.aux_fence );
		VK_CHECK( qvkResetCommandBuffer( vk.staging_command_buffer, 0 ) );
		vk.staging_buffer.offset = 0; // Reset for reuse after fence confirms upload complete
		vk.aux_fence_wait = qfalse;
		return qtrue;
	} else {
		return qfalse;
	}
}


static void vk_flush_staging_buffer( qboolean final )
{
	const VkPipelineStageFlags wait_dst_stage_mask = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	VkSemaphore waits;
	VkSubmitInfo submit_info;
	VkResult res;

	if ( vk.staging_buffer.offset == 0 ) {
		return;
	}

	//ri.Printf( PRINT_WARNING, S_COLOR_CYAN ">>> flush %i bytes (final=%i)<<<\n", (int)vk_world.staging_buffer_offset, final );

	vk.staging_buffer.offset = 0;

	VK_CHECK( qvkEndCommandBuffer( vk.staging_command_buffer ) );

	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = NULL;

	if ( vk.rendering_finished != VK_NULL_HANDLE ) {
		// first call after previous queue submission?
		waits = vk.rendering_finished;
		vk.rendering_finished = VK_NULL_HANDLE;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &waits;
		submit_info.pWaitDstStageMask = &wait_dst_stage_mask;
	} else {
		submit_info.waitSemaphoreCount = 0;
		submit_info.pWaitSemaphores = NULL;
		submit_info.pWaitDstStageMask = NULL;
	}

	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &vk.staging_command_buffer;

	if ( vk.image_uploaded != VK_NULL_HANDLE ) {
		ri.Error( ERR_FATAL, "Vulkan: incorrect state during image upload" );
	}
	if ( final ) {
		// final submission before recording
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &vk.image_uploaded2;
		vk.image_uploaded = vk.image_uploaded2;
		VK_CHECK( qvkQueueSubmit( vk.queue, 1, &submit_info, vk.aux_fence ) );
		vk.aux_fence_wait = qtrue;
	} else {
		// if submission before another upload then do explicit wait
		submit_info.signalSemaphoreCount = 0;
		submit_info.pSignalSemaphores = NULL;
		VK_CHECK( qvkQueueSubmit( vk.queue, 1, &submit_info, vk.aux_fence ) );
		res = qvkWaitForFences( vk.device, 1, &vk.aux_fence, VK_TRUE, 5 * 1000000000ULL );
		if ( res != VK_SUCCESS ) {
			ri.Error( ERR_FATAL, "vkWaitForFences() failed with %s at %s", vk_result_string( res ), __func__ );
		}
		qvkResetFences( vk.device, 1, &vk.aux_fence );
		VK_CHECK( qvkResetCommandBuffer( vk.staging_command_buffer, 0 ) );
	}
}
#endif // USE_UPLOAD_QUEUE


static void vk_alloc_staging_buffer( VkDeviceSize size )
{
	VkBufferCreateInfo buffer_desc;
	VkMemoryRequirements memory_requirements;
	VkMemoryAllocateInfo alloc_info;
	uint32_t memory_type;
	void *data;

	vk_clean_staging_buffer();

	vk.staging_buffer.size = MAX( size, STAGING_BUFFER_SIZE );
	vk.staging_buffer.size = PAD( vk.staging_buffer.size, 1024 * 1024 );

	buffer_desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_desc.pNext = NULL;
	buffer_desc.flags = 0;
	buffer_desc.size = vk.staging_buffer.size;
	buffer_desc.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	buffer_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	buffer_desc.queueFamilyIndexCount = 0;
	buffer_desc.pQueueFamilyIndices = NULL;
	VK_CHECK(qvkCreateBuffer(vk.device, &buffer_desc, NULL, &vk.staging_buffer.handle));

	qvkGetBufferMemoryRequirements( vk.device, vk.staging_buffer.handle, &memory_requirements );

	memory_type = find_memory_type( memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = memory_requirements.size;
	alloc_info.memoryTypeIndex = memory_type;

	VK_CHECK(qvkAllocateMemory(vk.device, &alloc_info, NULL, &vk.staging_buffer.memory));
	VK_CHECK(qvkBindBufferMemory(vk.device, vk.staging_buffer.handle, vk.staging_buffer.memory, 0));

	VK_CHECK(qvkMapMemory(vk.device, vk.staging_buffer.memory, 0, VK_WHOLE_SIZE, 0, &data));
	vk.staging_buffer.ptr = (byte*)data;
#ifdef USE_UPLOAD_QUEUE
	vk.staging_buffer.offset = 0;
#endif
	SET_OBJECT_NAME( vk.staging_buffer.handle, "staging buffer", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	SET_OBJECT_NAME( vk.staging_buffer.memory, "staging buffer memory", VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT );
}


#ifdef USE_VK_VALIDATION
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT object_type, uint64_t object, size_t location,
	int32_t message_code, const char* layer_prefix, const char* message, void* user_data) {
#ifdef _WIN32
	MessageBoxA( 0, message, layer_prefix, MB_ICONWARNING );
	OutputDebugString(message);
	OutputDebugString("\n");
	DebugBreak();
#endif
	return VK_FALSE;
}
#endif


static qboolean used_instance_extension( const char *ext )
{
	const char *u;

	// allow all VK_*_surface extensions
	u = strrchr( ext, '_' );
	if ( u && Q_stricmp( u + 1, "surface" ) == 0 )
		return qtrue;

	if ( Q_stricmp( ext, VK_KHR_DISPLAY_EXTENSION_NAME ) == 0 )
		return qtrue; // needed for KMSDRM instances/devices?

	if ( Q_stricmp( ext, VK_KHR_SWAPCHAIN_EXTENSION_NAME ) == 0 )
		return qtrue;

#ifdef USE_VK_VALIDATION
	if ( Q_stricmp( ext, VK_EXT_DEBUG_REPORT_EXTENSION_NAME ) == 0 )
		return qtrue;
#endif

	if ( Q_stricmp( ext, VK_EXT_DEBUG_UTILS_EXTENSION_NAME ) == 0 )
		return qtrue;

	if ( Q_stricmp( ext, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME ) == 0 )
		return qtrue;

	if ( Q_stricmp( ext, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME ) == 0 )
		return qtrue;

	if ( Q_stricmp( ext, VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME ) == 0 )
		return qtrue;

	return qfalse;
}


static void create_instance( void )
{
#ifdef USE_VK_VALIDATION
	const char* validation_layer_name = "VK_LAYER_LUNARG_standard_validation";
	const char* validation_layer_name2 = "VK_LAYER_KHRONOS_validation";
#endif
	VkInstanceCreateInfo desc;
	VkInstanceCreateFlags flags;
	VkExtensionProperties *extension_properties;
	VkResult res;
	const char **extension_names;
	uint32_t i, n, count, extension_count;
	VkApplicationInfo appInfo;

	flags = 0;
	count = 0;
	extension_count = 0;
	VK_CHECK(qvkEnumerateInstanceExtensionProperties(NULL, &count, NULL));

	extension_properties = (VkExtensionProperties *)ri.Malloc(sizeof(VkExtensionProperties) * count);
	extension_names = (const char**)ri.Malloc(sizeof(char *) * count);

	VK_CHECK( qvkEnumerateInstanceExtensionProperties( NULL, &count, extension_properties ) );
	for ( i = 0; i < count; i++ ) {
		const char *ext = extension_properties[i].extensionName;

		if ( !used_instance_extension( ext ) ) {
			continue;
		}

		// search for duplicates
		for ( n = 0; n < extension_count; n++ ) {
			if ( Q_stricmp( ext, extension_names[ n ] ) == 0 ) {
				break;
			}
		}
		if ( n != extension_count ) {
			continue;
		}

		extension_names[ extension_count++ ] = ext;

		if ( Q_stricmp( ext, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME ) == 0 ) {
			flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
		}

		if ( Q_stricmp( ext, VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME ) == 0 ) {
			vk.hdrColorspaceExt = qtrue;
		}

		ri.Printf(PRINT_DEVELOPER, "instance extension: %s\n", ext);
	}

	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pNext = NULL;
	appInfo.pApplicationName = NULL; // Q3_VERSION;
	appInfo.applicationVersion = 0x0;
	appInfo.pEngineName = NULL;
	appInfo.engineVersion = 0x0;
#ifdef _DEBUG
	appInfo.apiVersion = VK_API_VERSION_1_1;
#else
	appInfo.apiVersion = VK_API_VERSION_1_0;
#endif

	// create instance
	desc.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = flags;
	desc.pApplicationInfo = &appInfo;
	desc.enabledExtensionCount = extension_count;
	desc.ppEnabledExtensionNames = extension_names;

#ifdef USE_VK_VALIDATION
	desc.enabledLayerCount = 1;
	desc.ppEnabledLayerNames = &validation_layer_name;

	res = qvkCreateInstance( &desc, NULL, &vk_instance );

	if ( res == VK_ERROR_LAYER_NOT_PRESENT ) {

		desc.enabledLayerCount = 1;
		desc.ppEnabledLayerNames = &validation_layer_name2;

		res = qvkCreateInstance( &desc, NULL, &vk_instance );

		if ( res == VK_ERROR_LAYER_NOT_PRESENT ) {

			ri.Printf( PRINT_WARNING, "...validation layer is not available\n" );

			// try without validation layer
			desc.enabledLayerCount = 0;
			desc.ppEnabledLayerNames = NULL;

			res = qvkCreateInstance( &desc, NULL, &vk_instance );
		}
	}
#else
	desc.enabledLayerCount = 0;
	desc.ppEnabledLayerNames = NULL;

	res = qvkCreateInstance( &desc, NULL, &vk_instance );
#endif

	ri.Free( (void*)extension_names );
	ri.Free( extension_properties );

	if ( res != VK_SUCCESS ) {
		ri.Error( ERR_FATAL, "Vulkan: instance creation failed with %s", vk_result_string( res ) );
	}
}


static VkFormat get_depth_format( VkPhysicalDevice physical_device ) {
	VkFormatProperties props;
	VkFormat formats[2];
	int i;

	if ( glConfig.stencilBits > 0 ) {
		formats[0] = glConfig.depthBits == 16 ? VK_FORMAT_D16_UNORM_S8_UINT : VK_FORMAT_D24_UNORM_S8_UINT;
		formats[1] = VK_FORMAT_D32_SFLOAT_S8_UINT;
	} else {
		formats[0] = glConfig.depthBits == 16 ? VK_FORMAT_D16_UNORM : VK_FORMAT_X8_D24_UNORM_PACK32;
		formats[1] = VK_FORMAT_D32_SFLOAT;
	}

	for ( i = 0; i < ARRAY_LEN( formats ); i++ ) {
		qvkGetPhysicalDeviceFormatProperties( physical_device, formats[i], &props );
		if ( ( props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT ) != 0 ) {
			return formats[i];
		}
	}

	ri.Error( ERR_FATAL, "get_depth_format: failed to find depth attachment format" );
	return VK_FORMAT_UNDEFINED; // never get here
}


// Check if we can use vkCmdBlitImage for the given source and destination image formats.
static qboolean vk_blit_enabled( VkPhysicalDevice physical_device, const VkFormat srcFormat, const VkFormat dstFormat )
{
	VkFormatProperties formatProps;

	qvkGetPhysicalDeviceFormatProperties( physical_device, srcFormat, &formatProps );
	if ( ( formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT ) == 0 ) {
		return qfalse;
	}

	qvkGetPhysicalDeviceFormatProperties( physical_device, dstFormat, &formatProps );
	if ( ( formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT ) == 0 ) {
		return qfalse;
	}

	return qtrue;
}


static VkFormat get_hdr_format( VkFormat base_format )
{
	if ( r_fbo->integer == 0 ) {
		return base_format;
	}

	if ( vk.hdrActive ) {
		// UNORM keeps the framebuffer clamped to [0,1], which Q3's
		// destination-dependent blends (e.g. GL_ONE_MINUS_DST_COLOR) require.
		// HDR >1.0 headroom is reconstructed in the mirror encode via overbright.
		return VK_FORMAT_R16G16B16A16_UNORM;
	}

	switch ( r_hdr->integer ) {
		case -1: return VK_FORMAT_B4G4R4A4_UNORM_PACK16;
		case 1: return VK_FORMAT_R16G16B16A16_UNORM;
		default: return base_format;
	}
}


/*
 * vk_get_unorm_format - Convert sRGB format to corresponding UNORM format
 *
 * When writing to an sRGB framebuffer attachment, Vulkan automatically applies
 * linear-to-sRGB conversion. For the gamma pass, we want to write sRGB-encoded
 * values directly (as Quake3e does), so we use UNORM views to disable this
 * automatic conversion.
 */
static VkFormat vk_get_unorm_format( VkFormat format )
{
	switch ( format ) {
		case VK_FORMAT_R8G8B8A8_SRGB:  return VK_FORMAT_R8G8B8A8_UNORM;
		case VK_FORMAT_B8G8R8A8_SRGB:  return VK_FORMAT_B8G8R8A8_UNORM;
		case VK_FORMAT_A8B8G8R8_SRGB_PACK32:  return VK_FORMAT_A8B8G8R8_UNORM_PACK32;
		default: return format;  // Already UNORM or other format
	}
}

/*
 * vk_get_srgb_format - Convert UNORM format to corresponding sRGB format
 *
 * Used for creating sRGB views of swapchain images so we can have consistent
 * gamma handling behavior regardless of the swapchain's native format.
 */
static VkFormat vk_get_srgb_format( VkFormat format )
{
	switch ( format ) {
		case VK_FORMAT_R8G8B8A8_UNORM:  return VK_FORMAT_R8G8B8A8_SRGB;
		case VK_FORMAT_B8G8R8A8_UNORM:  return VK_FORMAT_B8G8R8A8_SRGB;
		case VK_FORMAT_A8B8G8R8_UNORM_PACK32:  return VK_FORMAT_A8B8G8R8_SRGB_PACK32;
		case VK_FORMAT_R8G8B8A8_SRGB:
		case VK_FORMAT_B8G8R8A8_SRGB:
		case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
			return format;  // Already sRGB
		default: return format;  // Unknown format, return as-is
	}
}


/*
 * vk_format_has_stencil - Check if depth format includes stencil component
 *
 * Used to determine the correct aspect mask for image barriers and views
 * when working with depth/stencil images. Without separateDepthStencilLayouts
 * feature, barriers must include both aspects.
 */
static qboolean vk_format_has_stencil( VkFormat format )
{
	switch ( format ) {
		case VK_FORMAT_D16_UNORM_S8_UINT:
		case VK_FORMAT_D24_UNORM_S8_UINT:
		case VK_FORMAT_D32_SFLOAT_S8_UINT:
			return qtrue;
		default:
			return qfalse;
	}
}


typedef struct {
	int bits;
	VkFormat rgb;
	VkFormat bgr;
} present_format_t;

static const present_format_t present_formats[] = {
	//{12, VK_FORMAT_B4G4R4A4_UNORM_PACK16, VK_FORMAT_R4G4B4A4_UNORM_PACK16},
	//{15, VK_FORMAT_B5G5R5A1_UNORM_PACK16, VK_FORMAT_R5G5B5A1_UNORM_PACK16},
	{16, VK_FORMAT_B5G6R5_UNORM_PACK16, VK_FORMAT_R5G6B5_UNORM_PACK16},
	{24, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM},
	{30, VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_A2R10G10B10_UNORM_PACK32},
	//{32, VK_FORMAT_B10G11R11_UFLOAT_PACK32, VK_FORMAT_B10G11R11_UFLOAT_PACK32}
};

// sRGB formats for desktop swapchain: matches XR swapchain color space
static const present_format_t present_formats_srgb[] = {
	{24, VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB},
};

static void get_present_format( int present_bits, VkFormat *bgr, VkFormat *rgb ) {
	const present_format_t *pf, *sel;
	int i;

	sel = NULL;
	pf = present_formats;
	for ( i = 0; i < ARRAY_LEN( present_formats ); i++, pf++ ) {
		if ( pf->bits <= present_bits  ) {
			sel = pf;
		}
	}
	if ( !sel ) {
		*bgr = VK_FORMAT_B8G8R8A8_UNORM;
		*rgb = VK_FORMAT_R8G8B8A8_UNORM;
	} else {
		*bgr = sel->bgr;
		*rgb = sel->rgb;
	}
}


typedef enum {
	OSHDR_UNKNOWN = 0,	// could not determine (non-Windows, old OS, or query failed)
	OSHDR_ON,		// an HDR-capable output has the OS HDR switch on
	OSHDR_OFF,		// HDR-capable output present but the OS HDR switch is off
	OSHDR_UNSUPPORTED	// no HDR-capable output found
} osHdrState_t;

#ifdef _WIN32
// These info-types are enum values (not macros), absent from some MinGW headers.
// Private structs match the documented Windows layouts.
//
// Type 9 "advancedColorEnabled" fires on wide-gamut SDR too (false positive when
// the HDR switch is off). Type 13 adds activeColorMode (SDR=0, WCG=1, HDR=2);
// prefer it, fall back to type 9 on systems without it.
#define TR_DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO	9
#define TR_DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO_2	15
#define TR_ADVANCED_COLOR_MODE_HDR			2
typedef struct {
	DISPLAYCONFIG_DEVICE_INFO_HEADER	header;
	UINT32					value;	// bit 0 supported, bit 1 enabled
	UINT32					colorEncoding;
	UINT32					bitsPerColorChannel;
} trAdvancedColorInfo_t;

typedef struct {
	DISPLAYCONFIG_DEVICE_INFO_HEADER	header;
	UINT32					value;	// bit 4 HDR supported, bit 5 HDR user-enabled
	UINT32					colorEncoding;
	UINT32					bitsPerColorChannel;
	UINT32					activeColorMode;
} trAdvancedColorInfo2_t;

static osHdrState_t vk_query_os_hdr_state( void )
{
	UINT32 numPath = 0, numMode = 0;
	DISPLAYCONFIG_PATH_INFO *paths;
	DISPLAYCONFIG_MODE_INFO *modes;
	osHdrState_t result = OSHDR_UNSUPPORTED;
	UINT32 i;

	if ( GetDisplayConfigBufferSizes( QDC_ONLY_ACTIVE_PATHS, &numPath, &numMode ) != ERROR_SUCCESS || numPath == 0 )
		return OSHDR_UNKNOWN;

	paths = (DISPLAYCONFIG_PATH_INFO*)ri.Malloc( numPath * sizeof( *paths ) );
	modes = (DISPLAYCONFIG_MODE_INFO*)ri.Malloc( ( numMode ? numMode : 1 ) * sizeof( *modes ) );

	if ( QueryDisplayConfig( QDC_ONLY_ACTIVE_PATHS, &numPath, paths, &numMode, modes, NULL ) != ERROR_SUCCESS ) {
		ri.Free( paths );
		ri.Free( modes );
		return OSHDR_UNKNOWN;
	}

	for ( i = 0; i < numPath; i++ ) {
		trAdvancedColorInfo2_t info2;
		trAdvancedColorInfo_t info;

		// Preferred: type 13 separates HDR from wide-gamut SDR.
		Com_Memset( &info2, 0, sizeof( info2 ) );
		info2.header.type = (DISPLAYCONFIG_DEVICE_INFO_TYPE)TR_DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO_2;
		info2.header.size = sizeof( info2 );
		info2.header.adapterId = paths[i].targetInfo.adapterId;
		info2.header.id = paths[i].targetInfo.id;
		if ( DisplayConfigGetDeviceInfo( &info2.header ) == ERROR_SUCCESS ) {
			ri.Printf( PRINT_DEVELOPER, "...OS HDR query (path %u): type-13 value 0x%02x, activeColorMode %u\n",
				(unsigned)i, (unsigned)info2.value, (unsigned)info2.activeColorMode );
			if ( info2.value & 0x10 ) {	// HDR supported
				if ( info2.activeColorMode == TR_ADVANCED_COLOR_MODE_HDR ) {
					result = OSHDR_ON;
					break;
				}
				result = OSHDR_OFF;
			}
			continue;
		}

		// Fallback for systems without type 13.
		Com_Memset( &info, 0, sizeof( info ) );
		info.header.type = (DISPLAYCONFIG_DEVICE_INFO_TYPE)TR_DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO;
		info.header.size = sizeof( info );
		info.header.adapterId = paths[i].targetInfo.adapterId;
		info.header.id = paths[i].targetInfo.id;
		if ( DisplayConfigGetDeviceInfo( &info.header ) == ERROR_SUCCESS && ( info.value & 0x1 ) ) {
			ri.Printf( PRINT_DEVELOPER, "...OS HDR query (path %u): type-9 value 0x%02x\n",
				(unsigned)i, (unsigned)info.value );
			if ( info.value & 0x2 ) {
				result = OSHDR_ON;
				break;
			}
			result = OSHDR_OFF;
		}
	}

	ri.Free( paths );
	ri.Free( modes );
	return result;
}
#else
static osHdrState_t vk_query_os_hdr_state( void )
{
	return OSHDR_UNKNOWN;
}
#endif


static qboolean vk_select_surface_format( VkPhysicalDevice physical_device, VkSurfaceKHR surface )
{
	VkFormat base_bgr, base_rgb;
	VkFormat ext_bgr, ext_rgb;
	VkSurfaceFormatKHR *candidates;
	uint32_t format_count;
	VkResult res;

	res = qvkGetPhysicalDeviceSurfaceFormatsKHR( physical_device, surface, &format_count, NULL );
	if ( res < 0 ) {
		ri.Printf( PRINT_ERROR, "vkGetPhysicalDeviceSurfaceFormatsKHR returned %s\n", vk_result_string( res ) );
		return qfalse;
	}

	if ( format_count == 0 ) {
		ri.Printf( PRINT_ERROR, "...no surface formats found\n" );
		return qfalse;
	}

	candidates = (VkSurfaceFormatKHR*)ri.Malloc( format_count * sizeof(VkSurfaceFormatKHR) );

	VK_CHECK( qvkGetPhysicalDeviceSurfaceFormatsKHR( physical_device, surface, &format_count, candidates ) );

	get_present_format( 24, &base_bgr, &base_rgb );

	if ( r_fbo->integer ) {
		get_present_format( r_presentBits->integer, &ext_bgr, &ext_rgb );
	} else {
		ext_bgr = base_bgr;
		ext_rgb = base_rgb;
	}

	vk.hdrActive = qfalse;

	if ( format_count == 1 && candidates[0].format == VK_FORMAT_UNDEFINED ) {
		// special case that means we can choose any format
		vk.base_format.format = base_bgr;
		vk.base_format.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
		vk.present_format.format = ext_bgr;
		vk.present_format.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	}
	else {
		uint32_t i;
		for ( i = 0; i < format_count; i++ ) {
			if ( ( candidates[i].format == base_bgr || candidates[i].format == base_rgb ) && candidates[i].colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR ) {
				vk.base_format = candidates[i];
				break;
			}
		}
		if ( i == format_count ) {
			vk.base_format = candidates[0];
		}

		// For desktop swapchain, prefer sRGB format so display correctly interprets
		// the sRGB-encoded values from the gamma pass (which writes through UNORM view)
		vk.present_format = vk.base_format; // fallback
		for ( i = 0; i < format_count; i++ ) {
			if ( ( candidates[i].format == VK_FORMAT_B8G8R8A8_SRGB || candidates[i].format == VK_FORMAT_R8G8B8A8_SRGB ) && candidates[i].colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR ) {
				vk.present_format = candidates[i];
				ri.Printf( PRINT_ALL, "...using sRGB format for desktop swapchain\n" );
				break;
			}
		}
		// Fallback to UNORM if sRGB not available
		if ( i == format_count ) {
			for ( i = 0; i < format_count; i++ ) {
				if ( ( candidates[i].format == ext_bgr || candidates[i].format == ext_rgb ) && candidates[i].colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR ) {
					vk.present_format = candidates[i];
					break;
				}
			}
		}

		if ( r_hdrDisplay->integer && r_fbo->integer ) {
			if ( !vk.hdrColorspaceExt ) {
				ri.Printf( PRINT_ALL, "...HDR requested but VK_EXT_swapchain_colorspace is unavailable; using SDR\n" );
			} else {
				vk.hdrOsState = vk_query_os_hdr_state();
				if ( vk.hdrOsState != OSHDR_OFF && vk.hdrOsState != OSHDR_UNSUPPORTED ) {
					uint32_t h;
					for ( h = 0; h < format_count; h++ ) {
						if ( candidates[h].format == VK_FORMAT_R16G16B16A16_SFLOAT &&
							candidates[h].colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT ) {
							vk.present_format = candidates[h];
							vk.hdrActive = qtrue;
							ri.Printf( PRINT_ALL, "...HDR output: scRGB linear FP16 (EXTENDED_SRGB_LINEAR)\n" );
							break;
						}
					}
					if ( !vk.hdrActive ) {
						ri.Printf( PRINT_ALL, "...HDR requested but no scRGB FP16 surface; using SDR\n" );
					}
				} else if ( vk.hdrOsState == OSHDR_OFF ) {
					ri.Printf( PRINT_ALL, "...HDR requested but the OS HDR switch is off; using SDR\n" );
				} else {
					ri.Printf( PRINT_ALL, "...HDR requested but no HDR-capable display found; using SDR\n" );
				}
			}
		}
	}

	ri.Cvar_Set( "r_hdrActive", vk.hdrActive ? "1" : "0" );

	if ( !r_fbo->integer ) {
		vk.present_format = vk.base_format;
	}

	ri.Free( candidates );

	return qtrue;
}


static void setup_surface_formats( VkPhysicalDevice physical_device )
{
	vk.depth_format = get_depth_format( physical_device );

	vk.color_format = get_hdr_format( vk.base_format.format );

	vk.capture_format = VK_FORMAT_R8G8B8A8_UNORM;

	vk.bloom_format = vk.base_format.format;

	vk.blitEnabled = vk_blit_enabled( physical_device, vk.color_format, vk.capture_format );

	if ( !vk.blitEnabled )
	{
		vk.capture_format = vk.color_format;
	}
}


static const char *renderer_name( const VkPhysicalDeviceProperties *props ) {
	static char buf[sizeof( props->deviceName ) + 64];
	const char *device_type;

	switch ( props->deviceType ) {
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: device_type = "Integrated"; break;
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: device_type = "Discrete"; break;
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: device_type = "Virtual"; break;
		case VK_PHYSICAL_DEVICE_TYPE_CPU: device_type = "CPU"; break;
		default: device_type = "OTHER"; break;
	}

	Com_sprintf( buf, sizeof( buf ), "%s %s, 0x%04x",
		device_type, props->deviceName, props->deviceID );

	return buf;
}


#define INIT_INSTANCE_FUNCTION(func) \
	q##func = /*(PFN_ ## func)*/ ri.VK_GetInstanceProcAddr(vk_instance, #func); \
	if (q##func == NULL) {											\
		ri.Error(ERR_FATAL, "Failed to find entrypoint %s", #func);	\
	}

#define INIT_INSTANCE_FUNCTION_EXT(func) \
	q##func = /*(PFN_ ## func)*/ ri.VK_GetInstanceProcAddr(vk_instance, #func);


#define INIT_DEVICE_FUNCTION(func) \
	q##func = (PFN_ ## func) qvkGetDeviceProcAddr(vk.device, #func);\
	if (q##func == NULL) {											\
		ri.Error(ERR_FATAL, "Failed to find entrypoint %s", #func);	\
	}

#define INIT_DEVICE_FUNCTION_EXT(func) \
	q##func = (PFN_ ## func) qvkGetDeviceProcAddr(vk.device, #func);


static void vk_destroy_instance( void ) {
	// Destroy the desktop mirror surface (we created this)
	if ( vk_surface != VK_NULL_HANDLE ) {
		if ( qvkDestroySurfaceKHR != NULL ) {
			qvkDestroySurfaceKHR( vk_instance, vk_surface, NULL );
		}
		vk_surface = VK_NULL_HANDLE;
	}

	// DO NOT destroy vk_instance: it's owned by the VR layer (XR_KHR_vulkan_enable2)
	// The VR layer's VR_Vulkan_Shutdown() handles instance/device destruction
	vk_instance = VK_NULL_HANDLE;
}


static void init_vulkan_library( void )
{
	// Q3VR is XR-only: pull Vulkan device from VR layer (XR_KHR_vulkan_enable2)
	const VR_VulkanDeviceInfo* xrDevice =
		(const VR_VulkanDeviceInfo*)ri.VR_Vulkan_GetDeviceInfo();

	if ( !xrDevice || xrDevice->device == VK_NULL_HANDLE ) {
		ri.Error( ERR_FATAL, "[VK] VR Vulkan device not available" );
		return;
	}

	Com_Memset( &vk, 0, sizeof( vk ) );

	// Set up from XR-provided objects
	vk.xrMode = qtrue;
	vk.xrInstance = xrDevice->instance;
	vk.physical_device = xrDevice->physicalDevice;
	vk.device = xrDevice->device;
	vk.queue_family_index = xrDevice->queueFamilyIndex;
	vk_instance = xrDevice->instance;
	vk.hdrColorspaceExt = xrDevice->swapchainColorspaceEnabled ? qtrue : qfalse;

	ri.Printf( PRINT_ALL, "[VK] Using VR-provided Vulkan device\n" );

	// Load instance-level function pointers from the XR-provided instance
	INIT_INSTANCE_FUNCTION( vkCreateDevice )
	INIT_INSTANCE_FUNCTION( vkDestroyInstance )
	INIT_INSTANCE_FUNCTION( vkEnumerateDeviceExtensionProperties )
	INIT_INSTANCE_FUNCTION( vkEnumeratePhysicalDevices )
	INIT_INSTANCE_FUNCTION( vkGetDeviceProcAddr )
	INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceFeatures )
	INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceFeatures2 )
	INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceFormatProperties )
	INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceMemoryProperties )
	INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceProperties )
	INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceQueueFamilyProperties )
	INIT_INSTANCE_FUNCTION( vkDestroySurfaceKHR )
	INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceSurfaceCapabilitiesKHR )
	INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceSurfaceFormatsKHR )
	INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceSurfacePresentModesKHR )
	INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceSurfaceSupportKHR )

	// Create desktop mirror surface for the window
	// This is separate from XR swapchains and used for spectator view
	if ( !ri.VK_CreateSurface( vk_instance, &vk_surface ) ) {
		ri.Error( ERR_FATAL, "Error creating Vulkan surface for desktop mirror" );
		return;
	}

	// Verify XR queue family supports presentation to desktop surface
	{
		VkBool32 presentSupported = VK_FALSE;
		VkResult res = qvkGetPhysicalDeviceSurfaceSupportKHR( vk.physical_device, vk.queue_family_index, vk_surface, &presentSupported );
		if ( res != VK_SUCCESS ) {
			ri.Printf( PRINT_WARNING, "[VK] Failed to query presentation support: %d\n", res );
		}
		if ( !presentSupported ) {
			// Try to find a queue family that supports presentation
			uint32_t queueFamilyCount = 0;
			qvkGetPhysicalDeviceQueueFamilyProperties( vk.physical_device, &queueFamilyCount, NULL );
			ri.Printf( PRINT_WARNING, "[VK] XR queue family %d does not support presentation, checking %d families...\n",
				vk.queue_family_index, queueFamilyCount );
			// For now, continue anyway: the presentation might work or we might need separate queues
		} else {
			ri.Printf( PRINT_ALL, "[VK] XR queue family %d supports presentation\n", vk.queue_family_index );
		}
	}

	// Set up surface formats for desktop swapchain
	vk.hdrOsState = OSHDR_UNKNOWN;
	if ( !vk_select_surface_format( vk.physical_device, vk_surface ) ) {
		ri.Error( ERR_FATAL, "Error selecting surface format for desktop mirror" );
		return;
	}
	setup_surface_formats( vk.physical_device );

	//
	// Get device level functions.
	//
	INIT_DEVICE_FUNCTION(vkAllocateCommandBuffers)
	INIT_DEVICE_FUNCTION(vkAllocateDescriptorSets)
	INIT_DEVICE_FUNCTION(vkAllocateMemory)
	INIT_DEVICE_FUNCTION(vkBeginCommandBuffer)
	INIT_DEVICE_FUNCTION(vkBindBufferMemory)
	INIT_DEVICE_FUNCTION(vkBindImageMemory)
	INIT_DEVICE_FUNCTION(vkCmdBeginRenderPass)
	INIT_DEVICE_FUNCTION(vkCmdBindDescriptorSets)
	INIT_DEVICE_FUNCTION(vkCmdBindIndexBuffer)
	INIT_DEVICE_FUNCTION(vkCmdBindPipeline)
	INIT_DEVICE_FUNCTION(vkCmdBindVertexBuffers)
	INIT_DEVICE_FUNCTION(vkCmdBlitImage)
	INIT_DEVICE_FUNCTION(vkCmdClearAttachments)
	INIT_DEVICE_FUNCTION(vkCmdClearColorImage)
	INIT_DEVICE_FUNCTION(vkCmdClearDepthStencilImage)
	INIT_DEVICE_FUNCTION(vkCmdCopyBuffer)
	INIT_DEVICE_FUNCTION(vkCmdCopyBufferToImage)
	INIT_DEVICE_FUNCTION(vkCmdCopyImage)
	INIT_DEVICE_FUNCTION(vkCmdDraw)
	INIT_DEVICE_FUNCTION(vkCmdDrawIndexed)
	INIT_DEVICE_FUNCTION(vkCmdEndRenderPass)
	INIT_DEVICE_FUNCTION(vkCmdNextSubpass)
	INIT_DEVICE_FUNCTION(vkCmdPipelineBarrier)
	INIT_DEVICE_FUNCTION(vkCmdPushConstants)
	INIT_DEVICE_FUNCTION(vkCmdResetQueryPool)
	INIT_DEVICE_FUNCTION(vkCmdSetDepthBias)
	INIT_DEVICE_FUNCTION(vkCmdSetScissor)
	INIT_DEVICE_FUNCTION(vkCmdSetViewport)
	INIT_DEVICE_FUNCTION(vkCmdWriteTimestamp)
	INIT_DEVICE_FUNCTION(vkCreateBuffer)
	INIT_DEVICE_FUNCTION(vkCreateCommandPool)
	INIT_DEVICE_FUNCTION(vkCreateDescriptorPool)
	INIT_DEVICE_FUNCTION(vkCreateDescriptorSetLayout)
	INIT_DEVICE_FUNCTION(vkCreateFence)
	INIT_DEVICE_FUNCTION(vkCreateFramebuffer)
	INIT_DEVICE_FUNCTION(vkCreateGraphicsPipelines)
	INIT_DEVICE_FUNCTION(vkCreateImage)
	INIT_DEVICE_FUNCTION(vkCreateImageView)
	INIT_DEVICE_FUNCTION(vkCreatePipelineCache)
	INIT_DEVICE_FUNCTION(vkCreatePipelineLayout)
	INIT_DEVICE_FUNCTION(vkCreateQueryPool)
	INIT_DEVICE_FUNCTION(vkCreateRenderPass)
	INIT_DEVICE_FUNCTION(vkCreateSampler)
	INIT_DEVICE_FUNCTION(vkCreateSemaphore)
	INIT_DEVICE_FUNCTION(vkCreateShaderModule)
	INIT_DEVICE_FUNCTION(vkDestroyBuffer)
	INIT_DEVICE_FUNCTION(vkDestroyCommandPool)
	INIT_DEVICE_FUNCTION(vkDestroyDescriptorPool)
	INIT_DEVICE_FUNCTION(vkDestroyDescriptorSetLayout)
	INIT_DEVICE_FUNCTION(vkDestroyDevice)
	INIT_DEVICE_FUNCTION(vkDestroyFence)
	INIT_DEVICE_FUNCTION(vkDestroyFramebuffer)
	INIT_DEVICE_FUNCTION(vkDestroyImage)
	INIT_DEVICE_FUNCTION(vkDestroyImageView)
	INIT_DEVICE_FUNCTION(vkDestroyPipeline)
	INIT_DEVICE_FUNCTION(vkDestroyPipelineCache)
	INIT_DEVICE_FUNCTION(vkDestroyPipelineLayout)
	INIT_DEVICE_FUNCTION(vkDestroyQueryPool)
	INIT_DEVICE_FUNCTION(vkDestroyRenderPass)
	INIT_DEVICE_FUNCTION(vkDestroySampler)
	INIT_DEVICE_FUNCTION(vkDestroySemaphore)
	INIT_DEVICE_FUNCTION(vkDestroyShaderModule)
	INIT_DEVICE_FUNCTION(vkDeviceWaitIdle)
	INIT_DEVICE_FUNCTION(vkEndCommandBuffer)
	INIT_DEVICE_FUNCTION(vkFlushMappedMemoryRanges)
	INIT_DEVICE_FUNCTION(vkFreeCommandBuffers)
	INIT_DEVICE_FUNCTION(vkFreeDescriptorSets)
	INIT_DEVICE_FUNCTION(vkFreeMemory)
	INIT_DEVICE_FUNCTION(vkGetBufferMemoryRequirements)
	INIT_DEVICE_FUNCTION(vkGetDeviceQueue)
	INIT_DEVICE_FUNCTION(vkGetFenceStatus)
	INIT_DEVICE_FUNCTION(vkGetImageMemoryRequirements)
	INIT_DEVICE_FUNCTION(vkGetQueryPoolResults)
	INIT_DEVICE_FUNCTION(vkGetImageSubresourceLayout)
	INIT_DEVICE_FUNCTION(vkInvalidateMappedMemoryRanges)
	INIT_DEVICE_FUNCTION(vkMapMemory)
	INIT_DEVICE_FUNCTION(vkQueueSubmit)
	INIT_DEVICE_FUNCTION(vkQueueWaitIdle)
	INIT_DEVICE_FUNCTION(vkResetCommandBuffer)
	INIT_DEVICE_FUNCTION(vkResetDescriptorPool)
	INIT_DEVICE_FUNCTION(vkResetFences)
	INIT_DEVICE_FUNCTION(vkUnmapMemory)
	INIT_DEVICE_FUNCTION(vkUpdateDescriptorSets)
	INIT_DEVICE_FUNCTION(vkWaitForFences)
	INIT_DEVICE_FUNCTION(vkAcquireNextImageKHR)
	INIT_DEVICE_FUNCTION(vkCreateSwapchainKHR)
	INIT_DEVICE_FUNCTION(vkDestroySwapchainKHR)
	INIT_DEVICE_FUNCTION(vkGetSwapchainImagesKHR)
	INIT_DEVICE_FUNCTION(vkQueuePresentKHR)

	if ( vk.dedicatedAllocation ) {
		INIT_DEVICE_FUNCTION_EXT(vkGetBufferMemoryRequirements2KHR);
		INIT_DEVICE_FUNCTION_EXT(vkGetImageMemoryRequirements2KHR);
		if ( !qvkGetBufferMemoryRequirements2KHR || !qvkGetImageMemoryRequirements2KHR ) {
			vk.dedicatedAllocation = qfalse;
		}
	}

	if ( vk.debugMarkers ) {
		INIT_DEVICE_FUNCTION_EXT(vkDebugMarkerSetObjectNameEXT)
	}

	// Check multiview support for VR single-pass stereo rendering
	if ( qvkGetPhysicalDeviceFeatures2 ) {
		VkPhysicalDeviceMultiviewFeatures multiviewFeatures = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES,
			.pNext = NULL,
		};
		VkPhysicalDeviceFeatures2 features2 = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
			.pNext = &multiviewFeatures,
		};
		qvkGetPhysicalDeviceFeatures2( vk.physical_device, &features2 );
		vk.multiviewSupported = multiviewFeatures.multiview ? qtrue : qfalse;
		vk.samplerAnisotropy = features2.features.samplerAnisotropy ? qtrue : qfalse;
		// depth clamp is enabled at device creation (vrvk passes the full queried
		// features2); record support here for the z-fail shadow pipeline.
		vk.depthClamp = features2.features.depthClamp ? qtrue : qfalse;
		// storage-buffer writes from vertex+fragment stages are likewise already
		// enabled at device creation; the flare visibility probe (dot.vert/dot.frag)
		// needs both. Without this flag R_ClearFlares never initializes the flare
		// pool and no flare can ever be allocated.
		vk.fragmentStores = ( features2.features.fragmentStoresAndAtomics &&
			features2.features.vertexPipelineStoresAndAtomics ) ? qtrue : qfalse;
		ri.Printf( PRINT_ALL, "...VK_KHR_multiview: %s\n",
			vk.multiviewSupported ? "supported" : "not supported" );
		ri.Printf( PRINT_ALL, "...samplerAnisotropy: %s\n",
			vk.samplerAnisotropy ? "supported" : "not supported" );
		ri.Printf( PRINT_ALL, "...fragmentStores: %s\n",
			vk.fragmentStores ? "supported" : "not supported" );
	} else {
		VkPhysicalDeviceFeatures features;
		qvkGetPhysicalDeviceFeatures( vk.physical_device, &features );
		vk.samplerAnisotropy = features.samplerAnisotropy ? qtrue : qfalse;
		vk.depthClamp = features.depthClamp ? qtrue : qfalse;
		vk.fragmentStores = ( features.fragmentStoresAndAtomics &&
			features.vertexPipelineStoresAndAtomics ) ? qtrue : qfalse;
		vk.multiviewSupported = qfalse;
		ri.Printf( PRINT_WARNING, "...vkGetPhysicalDeviceFeatures2 not available, multiview disabled\n" );
		ri.Printf( PRINT_ALL, "...samplerAnisotropy: %s\n",
			vk.samplerAnisotropy ? "supported" : "not supported" );
	}
}

#undef INIT_INSTANCE_FUNCTION
#undef INIT_DEVICE_FUNCTION
#undef INIT_DEVICE_FUNCTION_EXT

static void deinit_instance_functions( void )
{
	qvkCreateInstance = NULL;
	qvkEnumerateInstanceExtensionProperties = NULL;

	// instance functions:
	qvkCreateDevice = NULL;
	qvkDestroyInstance = NULL;
	qvkEnumerateDeviceExtensionProperties = NULL;
	qvkEnumeratePhysicalDevices = NULL;
	qvkGetDeviceProcAddr = NULL;
	qvkGetPhysicalDeviceFeatures = NULL;
	qvkGetPhysicalDeviceFeatures2 = NULL;
	qvkGetPhysicalDeviceFormatProperties = NULL;
	qvkGetPhysicalDeviceMemoryProperties = NULL;
	qvkGetPhysicalDeviceProperties = NULL;
	qvkGetPhysicalDeviceQueueFamilyProperties = NULL;
	qvkDestroySurfaceKHR = NULL;
	qvkGetPhysicalDeviceSurfaceCapabilitiesKHR = NULL;
	qvkGetPhysicalDeviceSurfaceFormatsKHR = NULL;
	qvkGetPhysicalDeviceSurfacePresentModesKHR = NULL;
	qvkGetPhysicalDeviceSurfaceSupportKHR = NULL;
#ifdef USE_VK_VALIDATION
	qvkCreateDebugReportCallbackEXT = NULL;
	qvkDestroyDebugReportCallbackEXT = NULL;
#endif
}


static void deinit_device_functions( void )
{
	// device functions:
	qvkAllocateCommandBuffers					= NULL;
	qvkAllocateDescriptorSets					= NULL;
	qvkAllocateMemory							= NULL;
	qvkBeginCommandBuffer						= NULL;
	qvkBindBufferMemory							= NULL;
	qvkBindImageMemory							= NULL;
	qvkCmdBeginRenderPass						= NULL;
	qvkCmdBindDescriptorSets					= NULL;
	qvkCmdBindIndexBuffer						= NULL;
	qvkCmdBindPipeline							= NULL;
	qvkCmdBindVertexBuffers						= NULL;
	qvkCmdBlitImage								= NULL;
	qvkCmdClearAttachments						= NULL;
	qvkCmdClearColorImage						= NULL;
	qvkCmdClearDepthStencilImage				= NULL;
	qvkCmdCopyBuffer							= NULL;
	qvkCmdCopyBufferToImage						= NULL;
	qvkCmdCopyImage								= NULL;
	qvkCmdDraw									= NULL;
	qvkCmdDrawIndexed							= NULL;
	qvkCmdEndRenderPass							= NULL;
	qvkCmdNextSubpass							= NULL;
	qvkCmdPipelineBarrier						= NULL;
	qvkCmdPushConstants							= NULL;
	qvkCmdResetQueryPool						= NULL;
	qvkCmdSetDepthBias							= NULL;
	qvkCmdSetScissor							= NULL;
	qvkCmdSetViewport							= NULL;
	qvkCmdWriteTimestamp						= NULL;
	qvkCreateBuffer								= NULL;
	qvkCreateCommandPool						= NULL;
	qvkCreateDescriptorPool						= NULL;
	qvkCreateDescriptorSetLayout				= NULL;
	qvkCreateFence								= NULL;
	qvkCreateFramebuffer						= NULL;
	qvkCreateGraphicsPipelines					= NULL;
	qvkCreateImage								= NULL;
	qvkCreateImageView							= NULL;
	qvkCreatePipelineCache						= NULL;
	qvkCreatePipelineLayout						= NULL;
	qvkCreateQueryPool							= NULL;
	qvkCreateRenderPass							= NULL;
	qvkCreateSampler							= NULL;
	qvkCreateSemaphore							= NULL;
	qvkCreateShaderModule						= NULL;
	qvkDestroyBuffer							= NULL;
	qvkDestroyCommandPool						= NULL;
	qvkDestroyDescriptorPool					= NULL;
	qvkDestroyDescriptorSetLayout				= NULL;
	qvkDestroyDevice							= NULL;
	qvkDestroyFence								= NULL;
	qvkDestroyFramebuffer						= NULL;
	qvkDestroyImage								= NULL;
	qvkDestroyImageView							= NULL;
	qvkDestroyPipeline							= NULL;
	qvkDestroyPipelineCache						= NULL;
	qvkDestroyPipelineLayout					= NULL;
	qvkDestroyQueryPool							= NULL;
	qvkDestroyRenderPass						= NULL;
	qvkDestroySampler							= NULL;
	qvkDestroySemaphore							= NULL;
	qvkDestroyShaderModule						= NULL;
	qvkDeviceWaitIdle							= NULL;
	qvkEndCommandBuffer							= NULL;
	qvkFlushMappedMemoryRanges					= NULL;
	qvkFreeCommandBuffers						= NULL;
	qvkFreeDescriptorSets						= NULL;
	qvkFreeMemory								= NULL;
	qvkGetBufferMemoryRequirements				= NULL;
	qvkGetQueryPoolResults						= NULL;
	qvkGetDeviceQueue							= NULL;
	qvkGetFenceStatus							= NULL;
	qvkGetImageMemoryRequirements				= NULL;
	qvkGetImageSubresourceLayout				= NULL;
	qvkInvalidateMappedMemoryRanges				= NULL;
	qvkMapMemory								= NULL;
	qvkQueueSubmit								= NULL;
	qvkQueueWaitIdle							= NULL;
	qvkResetCommandBuffer						= NULL;
	qvkResetDescriptorPool						= NULL;
	qvkResetFences								= NULL;
	qvkUnmapMemory								= NULL;
	qvkUpdateDescriptorSets						= NULL;
	qvkWaitForFences							= NULL;
	qvkAcquireNextImageKHR						= NULL;
	qvkCreateSwapchainKHR						= NULL;
	qvkDestroySwapchainKHR						= NULL;
	qvkGetSwapchainImagesKHR					= NULL;
	qvkQueuePresentKHR							= NULL;

	qvkGetBufferMemoryRequirements2KHR			= NULL;
	qvkGetImageMemoryRequirements2KHR			= NULL;

	qvkDebugMarkerSetObjectNameEXT				= NULL;
}


static VkShaderModule SHADER_MODULE(const uint8_t *bytes, const int count) {
	VkShaderModuleCreateInfo desc;
	VkShaderModule module;

	if ( count % 4 != 0 ) {
		ri.Error( ERR_FATAL, "Vulkan: SPIR-V binary buffer size is not a multiple of 4" );
	}

	desc.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.codeSize = count;
	desc.pCode = (const uint32_t*)bytes;

	VK_CHECK(qvkCreateShaderModule(vk.device, &desc, NULL, &module));

	return module;
}


static void vk_create_layout_binding( int binding, VkDescriptorType type, VkShaderStageFlags flags, VkDescriptorSetLayout *layout )
{
	VkDescriptorSetLayoutBinding bind;
	VkDescriptorSetLayoutCreateInfo desc;

	bind.binding = binding;
	bind.descriptorType = type;
	bind.descriptorCount = 1;
	bind.stageFlags = flags;
	bind.pImmutableSamplers = NULL;

	desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.bindingCount = 1;
	desc.pBindings = &bind;

	VK_CHECK( qvkCreateDescriptorSetLayout(vk.device, &desc, NULL, layout ) );
}


void vk_update_uniform_descriptor( VkDescriptorSet descriptor, VkBuffer buffer )
{
	VkDescriptorBufferInfo info[2];
	VkWriteDescriptorSet desc[2];
	int i;

	info[0].buffer = buffer;
	info[0].offset = 0;
	info[0].range = sizeof( vkUniform_t );

	info[1].buffer = buffer;
	info[1].offset = 0;
	info[1].range = sizeof( float ) * 32; // eyeProj[2]

	for ( i = 0; i < 2; i++ ) {
		desc[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		desc[i].pNext = NULL;
		desc[i].dstSet = descriptor;
		desc[i].dstBinding = i;
		desc[i].dstArrayElement = 0;
		desc[i].descriptorCount = 1;
		desc[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		desc[i].pImageInfo = NULL;
		desc[i].pBufferInfo = &info[i];
		desc[i].pTexelBufferView = NULL;
	}

	qvkUpdateDescriptorSets( vk.device, 2, desc, 0, NULL );
}


static VkSampler vk_find_sampler( const Vk_Sampler_Def *def ) {
	VkSamplerAddressMode address_mode;
	VkSamplerCreateInfo desc;
	VkSampler sampler;
	VkFilter mag_filter;
	VkFilter min_filter;
	VkSamplerMipmapMode mipmap_mode;
	float maxLod;
	int i;

	// Look for sampler among existing samplers.
	for ( i = 0; i < vk.samplers.count; i++ ) {
		const Vk_Sampler_Def *cur_def = &vk.samplers.def[i];
		if ( memcmp( cur_def, def, sizeof( *def ) ) == 0 ) {
			return vk.samplers.handle[i];
		}
	}

	// Create new sampler.
	if ( vk.samplers.count >= MAX_VK_SAMPLERS ) {
		ri.Error( ERR_DROP, "vk_find_sampler: MAX_VK_SAMPLERS hit\n" );
		// return VK_NULL_HANDLE;
	}

	address_mode = def->address_mode;

	if (def->gl_mag_filter == GL_NEAREST) {
		mag_filter = VK_FILTER_NEAREST;
	} else if (def->gl_mag_filter == GL_LINEAR) {
		mag_filter = VK_FILTER_LINEAR;
	} else {
		ri.Error(ERR_FATAL, "vk_find_sampler: invalid gl_mag_filter");
		return VK_NULL_HANDLE;
	}

	maxLod = vk.maxLod;

	if (def->gl_min_filter == GL_NEAREST) {
		min_filter = VK_FILTER_NEAREST;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		maxLod = 0.25f; // used to emulate OpenGL's GL_LINEAR/GL_NEAREST minification filter
	} else if (def->gl_min_filter == GL_LINEAR) {
		min_filter = VK_FILTER_LINEAR;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		maxLod = 0.25f; // used to emulate OpenGL's GL_LINEAR/GL_NEAREST minification filter
	} else if (def->gl_min_filter == GL_NEAREST_MIPMAP_NEAREST) {
		min_filter = VK_FILTER_NEAREST;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	} else if (def->gl_min_filter == GL_LINEAR_MIPMAP_NEAREST) {
		min_filter = VK_FILTER_LINEAR;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	} else if (def->gl_min_filter == GL_NEAREST_MIPMAP_LINEAR) {
		min_filter = VK_FILTER_NEAREST;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	} else if (def->gl_min_filter == GL_LINEAR_MIPMAP_LINEAR) {
		min_filter = VK_FILTER_LINEAR;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	} else {
		ri.Error(ERR_FATAL, "vk_find_sampler: invalid gl_min_filter");
		return VK_NULL_HANDLE;
	}

	if ( def->max_lod_1_0 ) {
		maxLod = 1.0f;
	}

	desc.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.magFilter = mag_filter;
	desc.minFilter = min_filter;
	desc.mipmapMode = mipmap_mode;
	desc.addressModeU = address_mode;
	desc.addressModeV = address_mode;
	desc.addressModeW = address_mode;
	desc.mipLodBias = 0.0f;

	// Match OpenGL behavior: only enable anisotropic filtering for textures with mipmaps
	// maxLod < 1.0f means no mipmaps (GL_NEAREST or GL_LINEAR min filter, or max_lod_1_0 flag)
	if ( def->noAnisotropy || mag_filter == VK_FILTER_NEAREST || maxLod < 1.0f ) {
		desc.anisotropyEnable = VK_FALSE;
		desc.maxAnisotropy = 1.0f;
	} else {
		desc.anisotropyEnable = (r_ext_texture_filter_anisotropic->integer && vk.samplerAnisotropy) ? VK_TRUE : VK_FALSE;
		if ( desc.anisotropyEnable ) {
			desc.maxAnisotropy = MIN( r_ext_max_anisotropy->integer, vk.maxAnisotropy );
		}
	}

	desc.compareEnable = VK_FALSE;
	desc.compareOp = VK_COMPARE_OP_ALWAYS;
	desc.minLod = 0.0f;
	desc.maxLod = (maxLod == vk.maxLod) ? VK_LOD_CLAMP_NONE : maxLod;
	desc.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	desc.unnormalizedCoordinates = VK_FALSE;

	VK_CHECK( qvkCreateSampler( vk.device, &desc, NULL, &sampler ) );

	SET_OBJECT_NAME( sampler, va( "image sampler %i", vk.samplers.count ), VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT );

	vk.samplers.def[ vk.samplers.count ] = *def;
	vk.samplers.handle[ vk.samplers.count ] = sampler;
	vk.samplers.count++;

	return sampler;
}


void vk_destroy_samplers( void )
{
	int i;

	for ( i = 0; i < vk.samplers.count; i++ ) {
		qvkDestroySampler( vk.device, vk.samplers.handle[i], NULL );
		memset( &vk.samplers.def[i], 0x0, sizeof( vk.samplers.def[i] ) );
		vk.samplers.handle[i] = VK_NULL_HANDLE;
	}

	vk.samplers.count = 0;
}


void vk_update_attachment_descriptors( void ) {

	if ( vk.color_image_view )
	{
		VkDescriptorImageInfo info;
		VkWriteDescriptorSet desc;
		Vk_Sampler_Def sd;

		Com_Memset( &sd, 0, sizeof( sd ) );
		sd.gl_mag_filter = sd.gl_min_filter = vk.blitFilter;
		sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sd.max_lod_1_0 = qtrue;
		sd.noAnisotropy = qtrue;

		info.sampler = vk_find_sampler( &sd );
		info.imageView = vk.color_image_view;
		info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		desc.dstSet = vk.color_descriptor;
		desc.dstBinding = 0;
		desc.dstArrayElement = 0;
		desc.descriptorCount = 1;
		desc.pNext = NULL;
		desc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		desc.pImageInfo = &info;
		desc.pBufferInfo = NULL;
		desc.pTexelBufferView = NULL;

		qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

		// emissive: placeholder (color_image_view) when inactive; gamma_fs always declares set 1
		info.imageView = vk.hdrActive ? vk.emissive_image_view : vk.color_image_view;
		desc.dstSet = vk.emissive_descriptor;
		desc.dstBinding = 0;
		qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

		// screenmap
		sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
		sd.max_lod_1_0 = qfalse;
		sd.noAnisotropy = qtrue;

		info.sampler = vk_find_sampler( &sd );

		info.imageView = vk.screenMap.color_image_view;
		desc.dstSet = vk.screenMap.color_descriptor;

		qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

		// bloom images
		if ( r_bloom->integer )
		{
			uint32_t i;
			for ( i = 0; i < ARRAY_LEN( vk.bloom_image_descriptor ); i++ )
			{
				info.imageView = vk.bloom_image_view[i];
				desc.dstSet = vk.bloom_image_descriptor[i];

				qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
			}
		}
	}
}


void vk_init_descriptors( void )
{
	VkDescriptorSetAllocateInfo alloc;
	VkDescriptorBufferInfo info;
	VkWriteDescriptorSet desc;
	uint32_t i;

	alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc.pNext = NULL;
	alloc.descriptorPool = vk.descriptor_pool;
	alloc.descriptorSetCount = 1;
	alloc.pSetLayouts = &vk.set_layout_storage;

	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.storage.descriptor ) );

	info.buffer = vk.storage.buffer;
	info.offset = 0;
	info.range = sizeof( uint32_t );

	desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	desc.dstSet = vk.storage.descriptor;
	desc.dstBinding = 0;
	desc.dstArrayElement = 0;
	desc.descriptorCount = 1;
	desc.pNext = NULL;
	desc.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
	desc.pImageInfo = NULL;
	desc.pBufferInfo = &info;
	desc.pTexelBufferView = NULL;

	qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

	// allocated and update descriptor set
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ )
	{
		alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc.pNext = NULL;
		alloc.descriptorPool = vk.descriptor_pool;
		alloc.descriptorSetCount = 1;
		alloc.pSetLayouts = &vk.set_layout_uniform;

		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.tess[i].uniform_descriptor ) );

		vk_update_uniform_descriptor( vk.tess[ i ].uniform_descriptor, vk.tess[ i ].vertex_buffer );

		SET_OBJECT_NAME( vk.tess[ i ].uniform_descriptor, va( "uniform descriptor %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
	}

	if ( vk.color_image_view )
	{
		alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc.pNext = NULL;
		alloc.descriptorPool = vk.descriptor_pool;
		alloc.descriptorSetCount = 1;
		alloc.pSetLayouts = &vk.set_layout_sampler;

		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.color_descriptor ) );
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.emissive_descriptor ) );

		if ( r_bloom->integer )
		{
			for ( i = 0; i < ARRAY_LEN( vk.bloom_image_descriptor ); i++ )
			{
				VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.bloom_image_descriptor[i] ) );
			}
		}

		alloc.descriptorSetCount = 1;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.screenMap.color_descriptor ) ); // screenmap

		vk_update_attachment_descriptors();
	}

	vk.descriptorsReady = qtrue;
}


static void vk_release_geometry_buffers( void )
{
	int i;

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		qvkDestroyBuffer( vk.device, vk.tess[i].vertex_buffer, NULL );
		vk.tess[i].vertex_buffer = VK_NULL_HANDLE;
	}

	qvkFreeMemory( vk.device, vk.geometry_buffer_memory, NULL );
	vk.geometry_buffer_memory = VK_NULL_HANDLE;
}


static void vk_create_geometry_buffers( VkDeviceSize size )
{
	VkMemoryRequirements vb_memory_requirements;
	VkMemoryAllocateInfo alloc_info;
	VkBufferCreateInfo desc;
	VkDeviceSize vertex_buffer_offset;
	uint32_t memory_type_bits;
	uint32_t memory_type;
	void *data;
	int i;

	desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;

	Com_Memset( &vb_memory_requirements, 0, sizeof( vb_memory_requirements ) );

	for ( i = 0 ; i < NUM_COMMAND_BUFFERS; i++ ) {
		desc.size = size;
		desc.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		VK_CHECK( qvkCreateBuffer( vk.device, &desc, NULL, &vk.tess[i].vertex_buffer ) );

		qvkGetBufferMemoryRequirements( vk.device, vk.tess[i].vertex_buffer, &vb_memory_requirements );
	}

	memory_type_bits = vb_memory_requirements.memoryTypeBits;
	memory_type = find_memory_type( memory_type_bits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = vb_memory_requirements.size * NUM_COMMAND_BUFFERS;
	alloc_info.memoryTypeIndex = memory_type;

	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.geometry_buffer_memory ) );
	VK_CHECK( qvkMapMemory( vk.device, vk.geometry_buffer_memory, 0, VK_WHOLE_SIZE, 0, &data ) );

	vertex_buffer_offset = 0;

	for ( i = 0 ; i < NUM_COMMAND_BUFFERS; i++ ) {
		qvkBindBufferMemory( vk.device, vk.tess[i].vertex_buffer, vk.geometry_buffer_memory, vertex_buffer_offset );
		vk.tess[i].vertex_buffer_ptr = (byte*)data + vertex_buffer_offset;
		vk.tess[i].vertex_buffer_offset = 0;
		vertex_buffer_offset += vb_memory_requirements.size;

		SET_OBJECT_NAME( vk.tess[i].vertex_buffer, va( "geometry buffer %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	}

	SET_OBJECT_NAME( vk.geometry_buffer_memory, "geometry buffer memory", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );

	vk.geometry_buffer_size = vb_memory_requirements.size;

	Com_Memset( &vk.stats, 0, sizeof( vk.stats ) );
}


static void vk_create_storage_buffer( uint32_t size )
{
	VkMemoryRequirements memory_requirements;
	VkMemoryAllocateInfo alloc_info;
	VkBufferCreateInfo desc;
	uint32_t memory_type_bits;
	uint32_t memory_type;

	desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;

	Com_Memset( &memory_requirements, 0, sizeof( memory_requirements ) );

	desc.size = size;
	desc.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	VK_CHECK( qvkCreateBuffer( vk.device, &desc, NULL, &vk.storage.buffer ) );

	qvkGetBufferMemoryRequirements( vk.device, vk.storage.buffer, &memory_requirements );

	memory_type_bits = memory_requirements.memoryTypeBits;
	memory_type = find_memory_type( memory_type_bits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = memory_requirements.size;
	alloc_info.memoryTypeIndex = memory_type;

	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.storage.memory ) );
	VK_CHECK( qvkMapMemory( vk.device, vk.storage.memory, 0, VK_WHOLE_SIZE, 0, (void**)&vk.storage.buffer_ptr ) );

	Com_Memset( vk.storage.buffer_ptr, 0, memory_requirements.size );

	qvkBindBufferMemory( vk.device, vk.storage.buffer, vk.storage.memory, 0 );

	SET_OBJECT_NAME( vk.storage.buffer, "storage buffer", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	SET_OBJECT_NAME( vk.storage.descriptor, "storage buffer", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
	SET_OBJECT_NAME( vk.storage.memory, "storage buffer memory", VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT );
}


#ifdef USE_VBO
void vk_release_vbo( void )
{
	if ( vk.vbo.vertex_buffer )
		qvkDestroyBuffer( vk.device, vk.vbo.vertex_buffer, NULL );
	vk.vbo.vertex_buffer = VK_NULL_HANDLE;

	if ( vk.vbo.buffer_memory )
		qvkFreeMemory( vk.device, vk.vbo.buffer_memory, NULL );
	vk.vbo.buffer_memory = VK_NULL_HANDLE;
}


qboolean vk_alloc_vbo( const byte *vbo_data, int vbo_size )
{
	VkMemoryRequirements vb_mem_reqs;
	VkMemoryAllocateInfo alloc_info;
	VkBufferCreateInfo desc;
	VkDeviceSize vertex_buffer_offset;
	VkDeviceSize allocationSize;
	uint32_t memory_type_bits;
	VkCommandBuffer command_buffer;
	VkBufferCopy copyRegion[1];
	VkDeviceSize uploadDone;

	vk_release_vbo();

	desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;

	// device-local buffer
	desc.size = vbo_size;
	desc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	VK_CHECK( qvkCreateBuffer( vk.device, &desc, NULL, &vk.vbo.vertex_buffer ) );

	// memory requirements
	qvkGetBufferMemoryRequirements( vk.device, vk.vbo.vertex_buffer, &vb_mem_reqs );
	vertex_buffer_offset = 0;
	allocationSize = vertex_buffer_offset + vb_mem_reqs.size;
	memory_type_bits = vb_mem_reqs.memoryTypeBits;

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = allocationSize;
	alloc_info.memoryTypeIndex = find_memory_type( memory_type_bits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.vbo.buffer_memory ) );
	qvkBindBufferMemory( vk.device, vk.vbo.vertex_buffer, vk.vbo.buffer_memory, vertex_buffer_offset );

	// staging buffers

#ifdef USE_UPLOAD_QUEUE
	vk_flush_staging_buffer( qfalse );
#endif
	// utilize existing staging buffer
	uploadDone = 0;
	while ( uploadDone < vbo_size ) {
		VkDeviceSize uploadSize = vk.staging_buffer.size;
		if ( uploadDone + uploadSize > vbo_size ) {
			uploadSize = vbo_size - uploadDone;
		}
		memcpy(vk.staging_buffer.ptr + 0, vbo_data + uploadDone, uploadSize);
		command_buffer = begin_command_buffer();
		copyRegion[0].srcOffset = 0;
		copyRegion[0].dstOffset = uploadDone;
		copyRegion[0].size = uploadSize;
		qvkCmdCopyBuffer( command_buffer, vk.staging_buffer.handle, vk.vbo.vertex_buffer, 1, &copyRegion[0] );
		end_command_buffer( command_buffer, __func__ );
		uploadDone += uploadSize;
	}

	SET_OBJECT_NAME( vk.vbo.vertex_buffer, "static VBO", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	SET_OBJECT_NAME( vk.vbo.buffer_memory, "static VBO memory", VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT );

	return qtrue;
}
#endif

#include "shaders/spirv/shader_data.c"
#define SHADER_MODULE(name) SHADER_MODULE(name,sizeof(name))

static void vk_create_shader_modules( void )
{
	int i, j, k, l;

	vk.modules.vert.gen[0][0][0][0] = SHADER_MODULE( vert_tx0 );
	vk.modules.vert.gen[0][0][0][1] = SHADER_MODULE( vert_tx0_fog );
	vk.modules.vert.gen[0][0][1][0] = SHADER_MODULE( vert_tx0_env );
	vk.modules.vert.gen[0][0][1][1] = SHADER_MODULE( vert_tx0_env_fog );

	vk.modules.vert.gen[1][0][0][0] = SHADER_MODULE( vert_tx1 );
	vk.modules.vert.gen[1][0][0][1] = SHADER_MODULE( vert_tx1_fog );
	vk.modules.vert.gen[1][0][1][0] = SHADER_MODULE( vert_tx1_env );
	vk.modules.vert.gen[1][0][1][1] = SHADER_MODULE( vert_tx1_env_fog );

	vk.modules.vert.gen[1][1][0][0] = SHADER_MODULE( vert_tx1_cl );
	vk.modules.vert.gen[1][1][0][1] = SHADER_MODULE( vert_tx1_cl_fog );
	vk.modules.vert.gen[1][1][1][0] = SHADER_MODULE( vert_tx1_cl_env );
	vk.modules.vert.gen[1][1][1][1] = SHADER_MODULE( vert_tx1_cl_env_fog );

	vk.modules.vert.gen[2][0][0][0] = SHADER_MODULE( vert_tx2 );
	vk.modules.vert.gen[2][0][0][1] = SHADER_MODULE( vert_tx2_fog );
	vk.modules.vert.gen[2][0][1][0] = SHADER_MODULE( vert_tx2_env );
	vk.modules.vert.gen[2][0][1][1] = SHADER_MODULE( vert_tx2_env_fog );

	vk.modules.vert.gen[2][1][0][0] = SHADER_MODULE( vert_tx2_cl );
	vk.modules.vert.gen[2][1][0][1] = SHADER_MODULE( vert_tx2_cl_fog );
	vk.modules.vert.gen[2][1][1][0] = SHADER_MODULE( vert_tx2_cl_env );
	vk.modules.vert.gen[2][1][1][1] = SHADER_MODULE( vert_tx2_cl_env_fog );

	for ( i = 0; i < 3; i++ ) {
		const char *tx[] = { "single", "double", "triple" };
		const char *cl[] = { "", "+cl" };
		const char *env[] = { "", "+env" };
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				for ( l = 0; l < 2; l++ ) {
					const char *s = va( "%s-texture%s%s%s vertex module", tx[i], cl[j], env[k], fog[l] );
					SET_OBJECT_NAME( vk.modules.vert.gen[i][j][k][l], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
				}
			}
		}
	}

	// specialized depth-fragment shader
	vk.modules.frag.gen0_df = SHADER_MODULE( frag_tx0_df );
	SET_OBJECT_NAME( vk.modules.frag.gen0_df, "single-texture df fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	// fixed-color (1.0) shader modules
	vk.modules.vert.ident1[0][0][0] = SHADER_MODULE( vert_tx0_ident1 );
	vk.modules.vert.ident1[0][0][1] = SHADER_MODULE( vert_tx0_ident1_fog );
	vk.modules.vert.ident1[0][1][0] = SHADER_MODULE( vert_tx0_ident1_env );
	vk.modules.vert.ident1[0][1][1] = SHADER_MODULE( vert_tx0_ident1_env_fog );
	vk.modules.vert.ident1[1][0][0] = SHADER_MODULE( vert_tx1_ident1 );
	vk.modules.vert.ident1[1][0][1] = SHADER_MODULE( vert_tx1_ident1_fog );
	vk.modules.vert.ident1[1][1][0] = SHADER_MODULE( vert_tx1_ident1_env );
	vk.modules.vert.ident1[1][1][1] = SHADER_MODULE( vert_tx1_ident1_env_fog );
	for ( i = 0; i < 2; i++ ) {
		const char *tx[] = { "single", "double" };
		const char *env[] = { "", "+env" };
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				const char *s = va( "%s-texture identity%s%s vertex module", tx[i], env[j], fog[k] );
				SET_OBJECT_NAME( vk.modules.vert.ident1[i][j][k], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
			}
		}
	}

	vk.modules.frag.ident1[0][0][0] = SHADER_MODULE( frag_tx0_ident1 );
	vk.modules.frag.ident1[0][0][1] = SHADER_MODULE( frag_tx0_ident1_fog );
	vk.modules.frag.ident1[0][1][0] = SHADER_MODULE( frag_tx0_ident1_ne );
	vk.modules.frag.ident1[0][1][1] = SHADER_MODULE( frag_tx0_ident1_fog_ne );
	vk.modules.frag.ident1[1][0][0] = SHADER_MODULE( frag_tx1_ident1 );
	vk.modules.frag.ident1[1][0][1] = SHADER_MODULE( frag_tx1_ident1_fog );
	vk.modules.frag.ident1[1][1][0] = SHADER_MODULE( frag_tx1_ident1_ne );
	vk.modules.frag.ident1[1][1][1] = SHADER_MODULE( frag_tx1_ident1_fog_ne );
	for ( i = 0; i < 2; i++ ) {
		const char *tx[] = { "single", "double" };
		const char *em[] = { "", " no-emissive" };
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				const char *s = va( "%s-texture identity%s%s fragment module", tx[i], fog[k], em[j] );
				SET_OBJECT_NAME( vk.modules.frag.ident1[i][j][k], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
			}
		}
	}

	vk.modules.vert.fixed[0][0][0] = SHADER_MODULE( vert_tx0_fixed );
	vk.modules.vert.fixed[0][0][1] = SHADER_MODULE( vert_tx0_fixed_fog );
	vk.modules.vert.fixed[0][1][0] = SHADER_MODULE( vert_tx0_fixed_env );
	vk.modules.vert.fixed[0][1][1] = SHADER_MODULE( vert_tx0_fixed_env_fog );
	vk.modules.vert.fixed[1][0][0] = SHADER_MODULE( vert_tx1_fixed );
	vk.modules.vert.fixed[1][0][1] = SHADER_MODULE( vert_tx1_fixed_fog );
	vk.modules.vert.fixed[1][1][0] = SHADER_MODULE( vert_tx1_fixed_env );
	vk.modules.vert.fixed[1][1][1] = SHADER_MODULE( vert_tx1_fixed_env_fog );
	for ( i = 0; i < 2; i++ ) {
		const char *tx[] = { "single", "double" };
		const char *env[] = { "", "+env" };
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				const char *s = va( "%s-texture fixed-color%s%s vertex module", tx[i], env[j], fog[k] );
				SET_OBJECT_NAME( vk.modules.vert.fixed[i][j][k], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
			}
		}
	}

	vk.modules.frag.fixed[0][0][0] = SHADER_MODULE( frag_tx0_fixed );
	vk.modules.frag.fixed[0][0][1] = SHADER_MODULE( frag_tx0_fixed_fog );
	vk.modules.frag.fixed[0][1][0] = SHADER_MODULE( frag_tx0_fixed_ne );
	vk.modules.frag.fixed[0][1][1] = SHADER_MODULE( frag_tx0_fixed_fog_ne );
	vk.modules.frag.fixed[1][0][0] = SHADER_MODULE( frag_tx1_fixed );
	vk.modules.frag.fixed[1][0][1] = SHADER_MODULE( frag_tx1_fixed_fog );
	vk.modules.frag.fixed[1][1][0] = SHADER_MODULE( frag_tx1_fixed_ne );
	vk.modules.frag.fixed[1][1][1] = SHADER_MODULE( frag_tx1_fixed_fog_ne );
	for ( i = 0; i < 2; i++ ) {
		const char *tx[] = { "single", "double" };
		const char *em[] = { "", " no-emissive" };
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				const char *s = va( "%s-texture fixed-color%s%s fragment module", tx[i], fog[k], em[j] );
				SET_OBJECT_NAME( vk.modules.frag.fixed[i][j][k], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
			}
		}
	}

	vk.modules.frag.ent[0][0][0] = SHADER_MODULE( frag_tx0_ent );
	vk.modules.frag.ent[0][0][1] = SHADER_MODULE( frag_tx0_ent_fog );
	vk.modules.frag.ent[0][1][0] = SHADER_MODULE( frag_tx0_ent_ne );
	vk.modules.frag.ent[0][1][1] = SHADER_MODULE( frag_tx0_ent_fog_ne );
	for ( i = 0; i < 1; i++ ) {
		const char *tx[] = { "single" /*, "double" */};
		const char *em[] = { "", " no-emissive" };
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				const char *s = va( "%s-texture entity-color%s%s fragment module", tx[i], fog[k], em[j] );
				SET_OBJECT_NAME( vk.modules.frag.ent[i][j][k], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
			}
		}
	}

	vk.modules.frag.gen[0][0][0][0] = SHADER_MODULE( frag_tx0 );
	vk.modules.frag.gen[0][0][0][1] = SHADER_MODULE( frag_tx0_fog );
	vk.modules.frag.gen[0][0][1][0] = SHADER_MODULE( frag_tx0_ne );
	vk.modules.frag.gen[0][0][1][1] = SHADER_MODULE( frag_tx0_fog_ne );

	vk.modules.frag.gen[1][0][0][0] = SHADER_MODULE( frag_tx1 );
	vk.modules.frag.gen[1][0][0][1] = SHADER_MODULE( frag_tx1_fog );
	vk.modules.frag.gen[1][0][1][0] = SHADER_MODULE( frag_tx1_ne );
	vk.modules.frag.gen[1][0][1][1] = SHADER_MODULE( frag_tx1_fog_ne );

	vk.modules.frag.gen[1][1][0][0] = SHADER_MODULE( frag_tx1_cl );
	vk.modules.frag.gen[1][1][0][1] = SHADER_MODULE( frag_tx1_cl_fog );
	vk.modules.frag.gen[1][1][1][0] = SHADER_MODULE( frag_tx1_cl_ne );
	vk.modules.frag.gen[1][1][1][1] = SHADER_MODULE( frag_tx1_cl_fog_ne );

	vk.modules.frag.gen[2][0][0][0] = SHADER_MODULE( frag_tx2 );
	vk.modules.frag.gen[2][0][0][1] = SHADER_MODULE( frag_tx2_fog );
	vk.modules.frag.gen[2][0][1][0] = SHADER_MODULE( frag_tx2_ne );
	vk.modules.frag.gen[2][0][1][1] = SHADER_MODULE( frag_tx2_fog_ne );

	vk.modules.frag.gen[2][1][0][0] = SHADER_MODULE( frag_tx2_cl );
	vk.modules.frag.gen[2][1][0][1] = SHADER_MODULE( frag_tx2_cl_fog );
	vk.modules.frag.gen[2][1][1][0] = SHADER_MODULE( frag_tx2_cl_ne );
	vk.modules.frag.gen[2][1][1][1] = SHADER_MODULE( frag_tx2_cl_fog_ne );

	for ( i = 0; i < 3; i++ ) {
		const char *tx[] = { "single", "double", "triple" };
		const char *cl[] = { "", "+cl" };
		const char *em[] = { "", " no-emissive" };
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				for ( l = 0; l < 2; l++ ) {
					const char *s = va( "%s-texture%s%s%s fragment module", tx[i], cl[j], fog[l], em[k] );
					SET_OBJECT_NAME( vk.modules.frag.gen[i][j][k][l], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
				}
			}
		}
	}


	vk.modules.vert.light[0] = SHADER_MODULE( vert_light );
	vk.modules.vert.light[1] = SHADER_MODULE( vert_light_fog );
	SET_OBJECT_NAME( vk.modules.vert.light[0], "light vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.vert.light[1], "light fog vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.frag.light[0][0][0] = SHADER_MODULE( frag_light );
	vk.modules.frag.light[0][0][1] = SHADER_MODULE( frag_light_fog );
	vk.modules.frag.light[0][1][0] = SHADER_MODULE( frag_light_ne );
	vk.modules.frag.light[0][1][1] = SHADER_MODULE( frag_light_fog_ne );
	vk.modules.frag.light[1][0][0] = SHADER_MODULE( frag_light_line );
	vk.modules.frag.light[1][0][1] = SHADER_MODULE( frag_light_line_fog );
	vk.modules.frag.light[1][1][0] = SHADER_MODULE( frag_light_line_ne );
	vk.modules.frag.light[1][1][1] = SHADER_MODULE( frag_light_line_fog_ne );
	for ( i = 0; i < 2; i++ ) {
		const char *line[] = { "", "linear " };
		const char *em[] = { "", " no-emissive" };
		const char *fog[] = { "", " fog" };
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				const char *s = va( "%slight%s%s fragment module", line[i], fog[k], em[j] );
				SET_OBJECT_NAME( vk.modules.frag.light[i][j][k], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
			}
		}
	}

	vk.modules.vert.overbright_vert[0] = SHADER_MODULE( vert_tx0_overbright );
	vk.modules.vert.overbright_vert[1] = SHADER_MODULE( vert_tx0_overbright_fog );
	vk.modules.frag.overbright_frag[0][0] = SHADER_MODULE( frag_tx0_overbright );
	vk.modules.frag.overbright_frag[0][1] = SHADER_MODULE( frag_tx0_overbright_fog );
	vk.modules.frag.overbright_frag[1][0] = SHADER_MODULE( frag_tx0_overbright_ne );
	vk.modules.frag.overbright_frag[1][1] = SHADER_MODULE( frag_tx0_overbright_fog_ne );

	vk.modules.color_fs = SHADER_MODULE( color_frag_spv );
	vk.modules.color_vs = SHADER_MODULE( color_vert_spv );

	SET_OBJECT_NAME( vk.modules.color_vs, "single-color vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.color_fs, "single-color fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.fog_vs = SHADER_MODULE( fog_vert_spv );
	vk.modules.fog_fs = SHADER_MODULE( fog_frag_spv );

	SET_OBJECT_NAME( vk.modules.fog_vs, "fog-only vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.fog_fs, "fog-only fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.dot_vs = SHADER_MODULE( dot_vert_spv );
	vk.modules.dot_fs = SHADER_MODULE( dot_frag_spv );

	SET_OBJECT_NAME( vk.modules.dot_vs, "dot vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.dot_fs, "dot fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.bloom_fs = SHADER_MODULE( bloom_frag_spv );
	vk.modules.blur_fs = SHADER_MODULE( blur_frag_spv );
	vk.modules.blend_fs = SHADER_MODULE( blend_frag_spv );

	SET_OBJECT_NAME( vk.modules.bloom_fs, "bloom extraction fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.blur_fs, "gaussian blur fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.blend_fs, "final bloom blend fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.gamma_fs = SHADER_MODULE( gamma_frag_spv );
	vk.modules.gamma_vs = SHADER_MODULE( gamma_vert_spv );

	SET_OBJECT_NAME( vk.modules.gamma_fs, "gamma post-processing fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.gamma_vs, "gamma post-processing vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.virtualscreen_vs = SHADER_MODULE( virtualscreen_vert_spv );
	vk.modules.virtualscreen_fs = SHADER_MODULE( virtualscreen_frag_spv );

	SET_OBJECT_NAME( vk.modules.virtualscreen_vs, "virtual screen vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.virtualscreen_fs, "virtual screen fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.floor_grid_vs = SHADER_MODULE( floor_grid_vert_spv );
	vk.modules.floor_grid_fs = SHADER_MODULE( floor_grid_frag_spv );

	SET_OBJECT_NAME( vk.modules.floor_grid_vs, "floor grid vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.floor_grid_fs, "floor grid fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.desktopmirror_vs = SHADER_MODULE( desktopmirror_vert_spv );
	vk.modules.desktopmirror_fs = SHADER_MODULE( desktopmirror_frag_spv );

	SET_OBJECT_NAME( vk.modules.desktopmirror_vs, "desktop mirror vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.desktopmirror_fs, "desktop mirror fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
}


static void vk_alloc_persistent_pipelines( void )
{
	unsigned int state_bits;
	Vk_Pipeline_Def def;

	// Ensure we're creating pipelines for the main render pass
	vk.renderPassIndex = RENDER_PASS_MAIN;

	// skybox
	{
		Com_Memset(&def, 0, sizeof(def));
		def.shader_type = TYPE_SINGLE_TEXTURE_FIXED_COLOR;
		def.color.rgb = tr.identityLightByte;
		def.color.alpha = tr.identityLightByte;
		def.face_culling = CT_FRONT_SIDED;
		def.polygon_offset = qfalse;
		def.mirror = qfalse;
		vk.skybox_pipeline = vk_find_pipeline_ext( 0, &def, qtrue );
	}

	// stencil shadows
	{
		cullType_t cull_types[2] = { CT_FRONT_SIDED, CT_BACK_SIDED };
		qboolean mirror_flags[2] = { qfalse, qtrue };
		int i, j;

		Com_Memset(&def, 0, sizeof(def));
		def.polygon_offset = qfalse;
		def.state_bits = 0;
		def.shader_type = TYPE_SINGLE_TEXTURE;
		def.shadow_phase = SHADOW_EDGES;

		for (i = 0; i < 2; i++) {
			def.face_culling = cull_types[i];
			for (j = 0; j < 2; j++) {
				def.mirror = mirror_flags[j];
				vk.shadow_volume_pipelines[i][j] = vk_find_pipeline_ext( 0, &def, r_shadows->integer ? qtrue: qfalse );
			}
		}
	}
	{
		Com_Memset( &def, 0, sizeof( def ) );
		def.face_culling = CT_FRONT_SIDED;
		def.polygon_offset = qfalse;
		def.state_bits = GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO | GLS_DEPTHTEST_DISABLE;
		def.shader_type = TYPE_SINGLE_TEXTURE;
		def.mirror = qfalse;
		def.shadow_phase = SHADOW_FS_QUAD;
		def.primitives = TRIANGLE_STRIP;
		vk.shadow_finish_pipeline = vk_find_pipeline_ext( 0, &def, r_shadows->integer ? qtrue: qfalse );
	}

	// fog and dlights
	{
		unsigned int fog_state_bits[2] = {
			GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_EQUAL, // fogPass == FP_EQUAL
			GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA // fogPass == FP_LE
		};
		unsigned int dlight_state_bits[2] = {
			GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL,	// modulated
			GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL			// additive
		};
		qboolean polygon_offset[2] = { qfalse, qtrue };
		int i, j, k;
#ifdef USE_PMLIGHT
		int l;
#endif

		Com_Memset(&def, 0, sizeof(def));
		def.shader_type = TYPE_SINGLE_TEXTURE;
		def.mirror = qfalse;

		for ( i = 0; i < 2; i++ ) {
			unsigned fog_state = fog_state_bits[ i ];
			unsigned dlight_state = dlight_state_bits[ i ];

			for ( j = 0; j < 3; j++ ) {
				def.face_culling = j; // cullType_t value

				for ( k = 0; k < 2; k++ ) {
					def.polygon_offset = polygon_offset[ k ];
#ifdef USE_FOG_ONLY
					def.shader_type = TYPE_FOG_ONLY;
#else
					def.shader_type = TYPE_SINGLE_TEXTURE;
#endif
					def.state_bits = fog_state;
					vk.fog_pipelines[ i ][ j ][ k ] = vk_find_pipeline_ext( 0, &def, qtrue );

					def.shader_type = TYPE_SINGLE_TEXTURE;
					def.state_bits = dlight_state;
#ifdef USE_LEGACY_DLIGHTS
#ifdef USE_PMLIGHT
					vk.dlight_pipelines[ i ][ j ][ k ] = vk_find_pipeline_ext( 0, &def, r_dlightMode->integer == 0 ? qtrue : qfalse );
#else
					vk.dlight_pipelines[ i ][ j ][ k ] = vk_find_pipeline_ext( 0, &def, qtrue );
#endif
#endif
				}
			}
		}

#ifdef USE_PMLIGHT
		def.state_bits = GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL;
		//def.shader_type = TYPE_SINGLE_TEXTURE_LIGHTING;
		for (i = 0; i < 3; i++) { // cullType
			def.face_culling = i;
			for ( j = 0; j < 2; j++ ) { // polygonOffset
				def.polygon_offset = polygon_offset[j];
				for ( k = 0; k < 2; k++ ) {
					def.fog_stage = k; // fogStage
					for ( l = 0; l < 2; l++ ) {
						def.abs_light = l;
						def.shader_type = TYPE_SINGLE_TEXTURE_LIGHTING;
						vk.dlight_pipelines_x[i][j][k][l] = vk_find_pipeline_ext( 0, &def, qfalse );
						def.shader_type = TYPE_SINGLE_TEXTURE_LIGHTING_LINEAR;
						vk.dlight1_pipelines_x[i][j][k][l] = vk_find_pipeline_ext( 0, &def, qfalse );
					}
				}
			}
		}
#endif // USE_PMLIGHT
	}

	// RT_BEAM surface
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
		def.face_culling = CT_FRONT_SIDED;
		def.primitives = TRIANGLE_STRIP;
		vk.surface_beam_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// axis for missing models
	{
		Com_Memset( &def, 0, sizeof( def ) );
		def.state_bits = GLS_DEFAULT;
		def.shader_type = TYPE_SINGLE_TEXTURE;
		def.face_culling = CT_TWO_SIDED;
		def.primitives = LINE_LIST;
		if ( vk.wideLines )
			def.line_width = 3;
		vk.surface_axis_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// flare visibility test probe: a sub-pixel triangle (an NVIDIA driver
	// bug hangs the GPU on multiview POINT_LIST + gl_ViewIndex +
	// gl_PointSize, see dot.vert)
	if ( vk.fragmentStores )
	{
		Com_Memset( &def, 0, sizeof( def ) );
		//def.state_bits = GLS_DEFAULT;
		def.face_culling = CT_TWO_SIDED;
		def.shader_type = TYPE_DOT;
		def.primitives = TRIANGLE_LIST;
		vk.dot_pipeline = vk_find_pipeline_ext( 0, &def, qtrue );
	}

	// DrawTris()
	state_bits = GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE;
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_WHITE;
		def.face_culling = CT_FRONT_SIDED;
		vk.tris_debug_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_WHITE;
		def.face_culling = CT_BACK_SIDED;
		vk.tris_mirror_debug_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_GREEN;
		def.face_culling = CT_FRONT_SIDED;
		vk.tris_debug_green_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_GREEN;
		def.face_culling = CT_BACK_SIDED;
		vk.tris_mirror_debug_green_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_RED;
		def.face_culling = CT_FRONT_SIDED;
		vk.tris_debug_red_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_RED;
		def.face_culling = CT_BACK_SIDED;
		vk.tris_mirror_debug_red_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// DrawNormals()
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = GLS_DEPTHMASK_TRUE;
		def.shader_type = TYPE_SINGLE_TEXTURE;
		def.primitives = LINE_LIST;
		vk.normals_debug_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// RB_DebugPolygon()
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
		def.shader_type = TYPE_SINGLE_TEXTURE;
		vk.surface_debug_pipeline_solid = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
		def.shader_type = TYPE_SINGLE_TEXTURE;
		def.primitives = LINE_LIST;
		vk.surface_debug_pipeline_outline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// RB_ShowImages
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
		def.shader_type = TYPE_SINGLE_TEXTURE;
		def.primitives = TRIANGLE_STRIP;
		vk.images_debug_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );

		def.state_bits = GLS_DEPTHTEST_DISABLE;
		def.shader_type = TYPE_COLOR_BLACK;
		def.primitives = TRIANGLE_STRIP;
		vk.images_debug_pipeline2 = vk_find_pipeline_ext( 0, &def, qfalse );
	}
}

void vk_update_post_process_pipelines( void )
{
	if ( vk.xr.initialized ) {
		vk_create_post_process_pipelines();
	}
}


typedef struct vk_attach_desc_s  {
	VkImage descriptor;
	VkImageView *image_view;
	VkImageUsageFlags usage;
	VkMemoryRequirements reqs;
	uint32_t memoryTypeIndex;
	VkDeviceSize  memory_offset;
	// for layout transition:
	VkImageAspectFlags aspect_flags;
	VkImageLayout image_layout;
	VkFormat image_format;
} vk_attach_desc_t;

static vk_attach_desc_t attachments[ MAX_ATTACHMENTS_IN_POOL ];
static uint32_t num_attachments = 0;


static void vk_clear_attachment_pool( void )
{
	num_attachments = 0;
}


static void vk_alloc_attachments( void )
{
	VkImageViewCreateInfo view_desc;
	VkMemoryDedicatedAllocateInfoKHR alloc_info2;
	VkMemoryAllocateInfo alloc_info;
	VkCommandBuffer command_buffer;
	VkDeviceMemory memory;
	VkDeviceSize offset;
	uint32_t memoryTypeBits;
	uint32_t memoryTypeIndex;
	uint32_t i;

	if ( num_attachments == 0 ) {
		return;
	}

	if ( vk.image_memory_count >= ARRAY_LEN( vk.image_memory ) ) {
		ri.Error( ERR_DROP, "vk.image_memory_count == %i", (int)ARRAY_LEN( vk.image_memory ) );
	}

	memoryTypeBits = ~0U;
	offset = 0;

	for ( i = 0; i < num_attachments; i++ ) {
#ifdef MIN_IMAGE_ALIGN
		VkDeviceSize alignment = MAX( attachments[ i ].reqs.alignment, MIN_IMAGE_ALIGN );
#else
		VkDeviceSize alignment = attachments[ i ].reqs.alignment;
#endif
		memoryTypeBits &= attachments[ i ].reqs.memoryTypeBits;
		offset = PAD( offset, alignment );
		attachments[ i ].memory_offset = offset;
		offset += attachments[ i ].reqs.size;
#ifdef _DEBUG
		ri.Printf( PRINT_ALL, S_COLOR_CYAN "[%i] type %i, size %i, align %i\n", i,
			attachments[ i ].reqs.memoryTypeBits,
			(int)attachments[ i ].reqs.size,
			(int)attachments[ i ].reqs.alignment );
#endif
	}

	if ( num_attachments == 1 && attachments[ 0 ].usage & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT ) {
		// try lazy memory
		memoryTypeIndex = find_memory_type2( memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT, NULL );
		if ( memoryTypeIndex == ~0U ) {
			memoryTypeIndex = find_memory_type( memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		}
	} else {
		memoryTypeIndex = find_memory_type( memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	}

#ifdef _DEBUG
	ri.Printf( PRINT_ALL, "memory type bits: %04x\n", memoryTypeBits );
	ri.Printf( PRINT_ALL, "memory type index: %04x\n", memoryTypeIndex );
	ri.Printf( PRINT_ALL, "total size: %i\n", (int)offset );
#endif

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = offset;
	alloc_info.memoryTypeIndex = memoryTypeIndex;

	if ( num_attachments == 1 ) {
		if ( vk.dedicatedAllocation ) {
			Com_Memset( &alloc_info2, 0, sizeof( alloc_info2 ) );
			alloc_info2.sType =  VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR;
			alloc_info2.image = attachments[ 0 ].descriptor;
			alloc_info.pNext = &alloc_info2;
		}
	}

	// allocate and bind memory
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &memory ) );

	vk.image_memory[ vk.image_memory_count++ ] = memory;

	for ( i = 0; i < num_attachments; i++ ) {

		VK_CHECK( qvkBindImageMemory( vk.device, attachments[i].descriptor, memory, attachments[i].memory_offset ) );

		// create image view (2D array for stereo multiview)
		view_desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_desc.pNext = NULL;
		view_desc.flags = 0;
		view_desc.image = attachments[ i ].descriptor;
		view_desc.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;  // Multiview stereo
		view_desc.format = attachments[ i ].image_format;
		view_desc.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_desc.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_desc.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_desc.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_desc.subresourceRange.aspectMask = attachments[ i ].aspect_flags;
		view_desc.subresourceRange.baseMipLevel = 0;
		view_desc.subresourceRange.levelCount = 1;
		view_desc.subresourceRange.baseArrayLayer = 0;
		view_desc.subresourceRange.layerCount = 2;  // Stereo (left + right)

		VK_CHECK( qvkCreateImageView( vk.device, &view_desc, NULL, attachments[ i ].image_view ) );
	}

	// perform layout transition
	command_buffer = begin_command_buffer();
	for ( i = 0; i < num_attachments; i++ ) {
		record_image_layout_transition( command_buffer,
			attachments[i].descriptor,
			attachments[i].aspect_flags,
			VK_IMAGE_LAYOUT_UNDEFINED, // old_layout
			attachments[i].image_layout,
			0, 0 );
	}
	end_command_buffer( command_buffer, __func__ );

	num_attachments = 0;
}


static void vk_add_attachment_desc( VkImage desc, VkImageView *image_view, VkImageUsageFlags usage, VkMemoryRequirements *reqs, VkFormat image_format, VkImageAspectFlags aspect_flags, VkImageLayout image_layout )
{
	if ( num_attachments >= ARRAY_LEN( attachments ) ) {
		ri.Error( ERR_FATAL, "Attachments array overflow" );
	} else {
		attachments[ num_attachments ].descriptor = desc;
		attachments[ num_attachments ].image_view = image_view;
		attachments[ num_attachments ].usage = usage;
		attachments[ num_attachments ].reqs = *reqs;
		attachments[ num_attachments ].aspect_flags = aspect_flags;
		attachments[ num_attachments ].image_layout = image_layout;
		attachments[ num_attachments ].image_format = image_format;
		attachments[ num_attachments ].memory_offset = 0;
		num_attachments++;
	}
}


static void vk_get_image_memory_erquirements( VkImage image, VkMemoryRequirements *memory_requirements )
{
	if ( vk.dedicatedAllocation ) {
		VkMemoryRequirements2KHR memory_requirements2;
		VkImageMemoryRequirementsInfo2KHR image_requirements2;
		VkMemoryDedicatedRequirementsKHR mem_req2;

		Com_Memset( &mem_req2, 0, sizeof( mem_req2 ) );
		mem_req2.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS_KHR;

		image_requirements2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2_KHR;
		image_requirements2.image = image;
		image_requirements2.pNext = NULL;

		Com_Memset( &memory_requirements2, 0, sizeof( memory_requirements2 ) );
		memory_requirements2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR;
		memory_requirements2.pNext = &mem_req2;

		qvkGetImageMemoryRequirements2KHR( vk.device, &image_requirements2, &memory_requirements2 );

		*memory_requirements = memory_requirements2.memoryRequirements;
	} else {
		qvkGetImageMemoryRequirements( vk.device, image, memory_requirements );
	}
}


static void create_color_attachment( uint32_t width, uint32_t height, VkSampleCountFlagBits samples, VkFormat format,
	VkImageUsageFlags usage, VkImage *image, VkImageView *image_view, VkImageLayout image_layout, qboolean multisample )
{
	VkImageCreateInfo create_desc;
	VkMemoryRequirements memory_requirements;

	if ( multisample && !( usage & VK_IMAGE_USAGE_SAMPLED_BIT ) )
		usage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;

	// create color image
	// For XR multiview rendering, we always need 2 array layers (stereo)
	create_desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	create_desc.pNext = NULL;
	create_desc.flags = 0;
	create_desc.imageType = VK_IMAGE_TYPE_2D;
	create_desc.format = format;
	create_desc.extent.width = width;
	create_desc.extent.height = height;
	create_desc.extent.depth = 1;
	create_desc.mipLevels = 1;
	create_desc.arrayLayers = 2;  // Stereo multiview
	create_desc.samples = samples;
	create_desc.tiling = VK_IMAGE_TILING_OPTIMAL;
	create_desc.usage = usage;
	create_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	create_desc.queueFamilyIndexCount = 0;
	create_desc.pQueueFamilyIndices = NULL;
	create_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VK_CHECK( qvkCreateImage( vk.device, &create_desc, NULL, image ) );

	vk_get_image_memory_erquirements( *image, &memory_requirements );

	vk_add_attachment_desc( *image, image_view, usage, &memory_requirements, format, VK_IMAGE_ASPECT_COLOR_BIT, image_layout );
}


static void create_depth_attachment( uint32_t width, uint32_t height, VkSampleCountFlagBits samples, VkImage *image, VkImageView *image_view, qboolean allowTransient )
{
	VkImageCreateInfo create_desc;
	VkMemoryRequirements memory_requirements;
	VkImageAspectFlags image_aspect_flags;

	// create depth image
	// For XR multiview rendering, we always need 2 array layers (stereo)
	create_desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	create_desc.pNext = NULL;
	create_desc.flags = 0;
	create_desc.imageType = VK_IMAGE_TYPE_2D;
	create_desc.format = vk.depth_format;
	create_desc.extent.width = width;
	create_desc.extent.height = height;
	create_desc.extent.depth = 1;
	create_desc.mipLevels = 1;
	create_desc.arrayLayers = 2;  // Stereo multiview
	create_desc.samples = samples;
	create_desc.tiling = VK_IMAGE_TILING_OPTIMAL;
	create_desc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	if ( allowTransient ) {
		create_desc.usage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
	}
	create_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	create_desc.queueFamilyIndexCount = 0;
	create_desc.pQueueFamilyIndices = NULL;
	create_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	image_aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT;
	if ( glConfig.stencilBits > 0 )
		image_aspect_flags |= VK_IMAGE_ASPECT_STENCIL_BIT;

	VK_CHECK( qvkCreateImage( vk.device, &create_desc, NULL, image ) );

	vk_get_image_memory_erquirements( *image, &memory_requirements );

	vk_add_attachment_desc( *image, image_view, create_desc.usage, &memory_requirements, vk.depth_format, image_aspect_flags, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL );
}


static void vk_create_attachments( void )
{
	uint32_t i;

	vk_clear_attachment_pool();

	// It looks like resulting performance depends from order you're creating/allocating
	// memory for attachments in vulkan i.e. similar images grouped together will provide best results
	// so [resolve0][resolve1][msaa0][msaa1][depth0][depth1] is most optimal
	// while cases like [resolve0][depth0][color0][...] is the worst

	// TODO: preallocate first image chunk in attachment' memory pool?
	{
		VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

		// bloom images (multiview-compatible 2-layer arrays)
		if ( r_bloom->integer ) {
			uint32_t width = gls.captureWidth;
			uint32_t height = gls.captureHeight;

			create_color_attachment( width, height, VK_SAMPLE_COUNT_1_BIT, vk.bloom_format,
				usage, &vk.bloom_image[0], &vk.bloom_image_view[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );

			for ( i = 1; i < ARRAY_LEN( vk.bloom_image ); i += 2 ) {
				width /= 2;
				height /= 2;
				create_color_attachment( width, height, VK_SAMPLE_COUNT_1_BIT, vk.bloom_format,
					usage, &vk.bloom_image[i+0], &vk.bloom_image_view[i+0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );

				create_color_attachment( width, height, VK_SAMPLE_COUNT_1_BIT, vk.bloom_format,
					usage, &vk.bloom_image[i+1], &vk.bloom_image_view[i+1], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );
			}
		}

		// post-processing/msaa-resolve (multiview-compatible 2-layer array)
		create_color_attachment( glConfig.vidWidth, glConfig.vidHeight, VK_SAMPLE_COUNT_1_BIT, vk.color_format,
			usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, &vk.color_image, &vk.color_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );

		if ( vk.hdrActive ) {
			// resolved 2-layer multiview emissive layer, sampled by the gamma/mirror pass
			create_color_attachment( glConfig.vidWidth, glConfig.vidHeight, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R16G16B16A16_SFLOAT,
				usage, &vk.emissive_image, &vk.emissive_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );

			if ( vk.msaaActive ) {
				create_color_attachment( glConfig.vidWidth, glConfig.vidHeight, vkSamples, VK_FORMAT_R16G16B16A16_SFLOAT,
					VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &vk.emissive_image_msaa, &vk.emissive_image_view_msaa,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, qtrue );
			}
		}

		// screenmap-msaa
		if ( vk.screenMapSamples > VK_SAMPLE_COUNT_1_BIT ) {
			create_color_attachment( vk.screenMapWidth, vk.screenMapHeight, vk.screenMapSamples, vk.color_format,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &vk.screenMap.color_image_msaa, &vk.screenMap.color_image_view_msaa, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, qtrue );
		}

		// screenmap/msaa-resolve
		create_color_attachment( vk.screenMapWidth, vk.screenMapHeight, VK_SAMPLE_COUNT_1_BIT, vk.color_format,
			usage, &vk.screenMap.color_image, &vk.screenMap.color_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );

		// screenmap depth
		create_depth_attachment( vk.screenMapWidth, vk.screenMapHeight, vk.screenMapSamples, &vk.screenMap.depth_image, &vk.screenMap.depth_image_view, qtrue );

		// MSAA color attachment (multiview-compatible 2-layer array)
		if ( vk.msaaActive ) {
			create_color_attachment( glConfig.vidWidth, glConfig.vidHeight, vkSamples, vk.color_format,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &vk.msaa_image, &vk.msaa_image_view, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, qtrue );
		}

	}

	//vk_alloc_attachments();

	create_depth_attachment( glConfig.vidWidth, glConfig.vidHeight, vkSamples, &vk.depth_image, &vk.depth_image_view,
		r_bloom->integer ? qfalse : qtrue );

	vk_alloc_attachments();

	for ( i = 0; i < vk.image_memory_count; i++ )
	{
		SET_OBJECT_NAME( vk.image_memory[i], va( "framebuffer memory chunk %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT );
	}

	SET_OBJECT_NAME( vk.depth_image, "depth attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	SET_OBJECT_NAME( vk.depth_image_view, "depth attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );

	SET_OBJECT_NAME( vk.color_image, "color attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	SET_OBJECT_NAME( vk.color_image_view, "color attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );

	SET_OBJECT_NAME( vk.capture.image, "capture image", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	SET_OBJECT_NAME( vk.capture.image_view, "capture image view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );

	for ( i = 0; i < ARRAY_LEN( vk.bloom_image ); i++ )
	{
		SET_OBJECT_NAME( vk.bloom_image[i], va( "bloom attachment %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.bloom_image_view[i], va( "bloom attachment %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	}
}


static void vk_create_framebuffers( void )
{
	VkImageView attachments[5]; // color | depth | msaa color | emissive resolve | emissive msaa
	VkFramebufferCreateInfo desc;
	uint32_t n;

	desc.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.pAttachments = attachments;
	desc.layers = 1;  // Multiview handles stereo layers via view mask

	for ( n = 0; n < vk.swapchain_image_count; n++ )
	{
		desc.renderPass = vk.render_pass.main;
		desc.attachmentCount = 2;
		if ( r_fbo->integer == 0 )
		{
			// VR-only: When FBO is NOT active, main rendering goes directly to XR swapchain.
			// XR framebuffers are created later in vk_create_xr_framebuffers() once
			// XR swapchains are available. Don't create desktop framebuffers here.
			continue;
		}
		else
		{
			// FBO mode: create main framebuffer (only once, shared for all swapchain indices)
			if ( n == 0 )
			{
				desc.width = glConfig.vidWidth;
				desc.height = glConfig.vidHeight;
				attachments[0] = vk.color_image_view;
				attachments[1] = vk.depth_image_view;
				if ( vk.msaaActive )
				{
					desc.attachmentCount = 3;
					attachments[2] = vk.msaa_image_view;
				}
				if ( vk.hdrActive )
				{
					// same attachment order the main/post-bloom render passes declare
					if ( vk.msaaActive )
					{
						attachments[3] = vk.emissive_image_view;      // resolve
						attachments[4] = vk.emissive_image_view_msaa; // msaa render target
						desc.attachmentCount = 5;
					}
					else
					{
						attachments[2] = vk.emissive_image_view;      // resolve
						desc.attachmentCount = 3;
					}
				}
				VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.main ) );
				SET_OBJECT_NAME( vk.framebuffers.main, "framebuffer - main (multiview)", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );

				// Post-bloom framebuffer - same attachments as main for pipeline compatibility
				// Post_bloom render pass must match RENDER_PASS_MAIN attachment structure
				desc.renderPass = vk.render_pass.post_bloom;
				// match main's attachmentCount
				VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.post_bloom ) );
				SET_OBJECT_NAME( vk.framebuffers.post_bloom, "framebuffer - post_bloom (multiview)", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
			}

			// Gamma framebuffers output to XR swapchain UNORM views
			// These are created later in vk_create_xr_gamma_framebuffers() once XR swapchains are ready
		}
	}

	{
		// screenmap - used for portal/mirror rendering
		desc.renderPass = vk.render_pass.screenmap;
		desc.attachmentCount = 2;
		desc.width = vk.screenMapWidth;
		desc.height = vk.screenMapHeight;
		attachments[0] = vk.screenMap.color_image_view;
		attachments[1] = vk.screenMap.depth_image_view;
		if ( vk.screenMapSamples > VK_SAMPLE_COUNT_1_BIT )
		{
			desc.attachmentCount = 3;
			attachments[2] = vk.screenMap.color_image_view_msaa;
		}
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.screenmap ) );
		SET_OBJECT_NAME( vk.framebuffers.screenmap, "framebuffer - screenmap", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );

		if ( r_bloom->integer )
		{
			uint32_t width = gls.captureWidth;
			uint32_t height = gls.captureHeight;

			// bloom color extraction
			desc.renderPass = vk.render_pass.bloom_extract;
			desc.width = width;
			desc.height = height;
			desc.attachmentCount = 1;
			attachments[0] = vk.bloom_image_view[0];

			VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.bloom_extract ) );
			SET_OBJECT_NAME( vk.framebuffers.bloom_extract, "framebuffer - bloom extraction", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );

			for ( n = 0; n < ARRAY_LEN( vk.framebuffers.blur ); n += 2 )
			{
				width /= 2;
				height /= 2;

				desc.renderPass = vk.render_pass.blur[n];
				desc.width = width;
				desc.height = height;
				desc.attachmentCount = 1;

				attachments[0] = vk.bloom_image_view[n+0+1];
				VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.blur[n+0] ) );

				attachments[0] = vk.bloom_image_view[n+1+1];
				VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.blur[n+1] ) );

				SET_OBJECT_NAME( vk.framebuffers.blur[n+0], va( "framebuffer - blur %i", n+0 ), VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
				SET_OBJECT_NAME( vk.framebuffers.blur[n+1], va( "framebuffer - blur %i", n+1 ), VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
			}
		}
	}
}


static void vk_create_sync_primitives( void ) {
	VkSemaphoreCreateInfo desc;
	VkFenceCreateInfo fence_desc;
	uint32_t i;

	desc.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;

#ifdef USE_UPLOAD_QUEUE
	VK_CHECK( qvkCreateSemaphore( vk.device, &desc, NULL, &vk.image_uploaded2 ) );
#endif

	// all commands submitted
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ )
	{
		desc.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;

		// swapchain image acquired
		VK_CHECK( qvkCreateSemaphore( vk.device, &desc, NULL, &vk.tess[i].image_acquired ) );

#ifdef USE_UPLOAD_QUEUE
		// second semaphore to synchronize additional tasks (e.g. image upload)
		VK_CHECK( qvkCreateSemaphore( vk.device, &desc, NULL, &vk.tess[i].rendering_finished2 ) );
#endif
		fence_desc.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fence_desc.pNext = NULL;
		//fence_desc.flags = VK_FENCE_CREATE_SIGNALED_BIT; // so it can be used to start rendering
		fence_desc.flags = 0; // non-signalled state

		VK_CHECK( qvkCreateFence( vk.device, &fence_desc, NULL, &vk.tess[i].rendering_finished_fence ) );
		vk.tess[i].waitForFence = qfalse;

		SET_OBJECT_NAME( vk.tess[i].image_acquired, va( "image_acquired semaphore %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT );
#ifdef USE_UPLOAD_QUEUE
		SET_OBJECT_NAME( vk.tess[i].rendering_finished2, va( "rendering_finished2 semaphore %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT );
#endif
		SET_OBJECT_NAME( vk.tess[i].rendering_finished_fence, va( "rendering_finished fence %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT );
	}

	fence_desc.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_desc.pNext = NULL;
	fence_desc.flags = 0;

#ifdef USE_UPLOAD_QUEUE
	VK_CHECK( qvkCreateFence( vk.device, &fence_desc, NULL, &vk.aux_fence ) );
	SET_OBJECT_NAME( vk.aux_fence, "aux fence", VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT );

	vk.rendering_finished = VK_NULL_HANDLE;
	vk.image_uploaded = VK_NULL_HANDLE;
	vk.aux_fence_wait = qfalse;
#endif

	// Desktop mirror semaphores for proper swapchain synchronization
	// Acquire semaphore: signaled by vkAcquireNextImageKHR, waited by command buffer submit
	VK_CHECK( qvkCreateSemaphore( vk.device, &desc, NULL, &vk.desktopAcquireSem ) );
	SET_OBJECT_NAME( vk.desktopAcquireSem, "desktop acquire semaphore", VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT );

	// Single fence - polled at the start of vk_present_desktop_mirror; the mirror
	// update is skipped (never waited on) while the previous blit is in flight
	// before reusing command buffer, acquire semaphore, etc.
	fence_desc.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_desc.pNext = NULL;
	fence_desc.flags = VK_FENCE_CREATE_SIGNALED_BIT; // start signaled for first use
	VK_CHECK( qvkCreateFence( vk.device, &fence_desc, NULL, &vk.desktopBlitFence ) );
	SET_OBJECT_NAME( vk.desktopBlitFence, "desktop blit fence", VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT );

	// Per-image semaphores - each signaled when blit for that image is done, waited by present
	for ( i = 0; i < MAX_SWAPCHAIN_IMAGES; i++ ) {
		VK_CHECK( qvkCreateSemaphore( vk.device, &desc, NULL, &vk.desktopBlitComplete[i] ) );
		SET_OBJECT_NAME( vk.desktopBlitComplete[i], va( "desktop blit complete %d", i ), VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT );
	}

	// Rendering complete semaphore: signaled by vk_end_frame, waited by desktop blit
	VK_CHECK( qvkCreateSemaphore( vk.device, &desc, NULL, &vk.renderingCompleteSem ) );
	SET_OBJECT_NAME( vk.renderingCompleteSem, "rendering complete semaphore", VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT );

	// GPU frame time: three timestamps per frame slot (r_gpuTimeLog)
	vk.gpuTimePool = VK_NULL_HANDLE;
	vk.gpuTime.count = 0;
	if ( vk.timestampPeriod > 0.0f ) {
		VkQueryPoolCreateInfo query_desc;

		Com_Memset( &query_desc, 0, sizeof( query_desc ) );
		query_desc.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		query_desc.queryType = VK_QUERY_TYPE_TIMESTAMP;
		query_desc.queryCount = NUM_COMMAND_BUFFERS * 3;
		VK_CHECK( qvkCreateQueryPool( vk.device, &query_desc, NULL, &vk.gpuTimePool ) );
		SET_OBJECT_NAME( vk.gpuTimePool, "gpu time query pool", VK_DEBUG_REPORT_OBJECT_TYPE_QUERY_POOL_EXT );
	}
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		vk.tess[i].gpu_time_armed = qfalse;
		vk.tess[i].gpu_time_scene_marked = qfalse;
		vk.tess[i].gpu_time_pending = qfalse;
	}
}


static void vk_destroy_sync_primitives( void  ) {
	uint32_t i;

#ifdef USE_UPLOAD_QUEUE
	qvkDestroySemaphore( vk.device, vk.image_uploaded2, NULL );
#endif

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		qvkDestroySemaphore( vk.device, vk.tess[i].image_acquired, NULL );
#ifdef USE_UPLOAD_QUEUE
		qvkDestroySemaphore( vk.device, vk.tess[i].rendering_finished2, NULL );
#endif
		qvkDestroyFence( vk.device, vk.tess[i].rendering_finished_fence, NULL );
		vk.tess[i].waitForFence = qfalse;
		vk.tess[i].swapchain_image_acquired = qfalse;
	}

#ifdef USE_UPLOAD_QUEUE
	qvkDestroyFence( vk.device, vk.aux_fence, NULL );

	vk.rendering_finished = VK_NULL_HANDLE;
	vk.image_uploaded = VK_NULL_HANDLE;
#endif

	// Desktop mirror blit resources
	if ( vk.desktopAcquireSem != VK_NULL_HANDLE ) {
		qvkDestroySemaphore( vk.device, vk.desktopAcquireSem, NULL );
		vk.desktopAcquireSem = VK_NULL_HANDLE;
	}
	if ( vk.desktopBlitFence != VK_NULL_HANDLE ) {
		qvkDestroyFence( vk.device, vk.desktopBlitFence, NULL );
		vk.desktopBlitFence = VK_NULL_HANDLE;
	}
	for ( i = 0; i < MAX_SWAPCHAIN_IMAGES; i++ ) {
		if ( vk.desktopBlitComplete[i] != VK_NULL_HANDLE ) {
			qvkDestroySemaphore( vk.device, vk.desktopBlitComplete[i], NULL );
			vk.desktopBlitComplete[i] = VK_NULL_HANDLE;
		}
	}
	if ( vk.renderingCompleteSem != VK_NULL_HANDLE ) {
		qvkDestroySemaphore( vk.device, vk.renderingCompleteSem, NULL );
		vk.renderingCompleteSem = VK_NULL_HANDLE;
	}
	if ( vk.gpuTimePool != VK_NULL_HANDLE ) {
		qvkDestroyQueryPool( vk.device, vk.gpuTimePool, NULL );
		vk.gpuTimePool = VK_NULL_HANDLE;
	}
	// Command buffer is freed when command pool is destroyed, no explicit free needed
	vk.desktopBlitCmd = VK_NULL_HANDLE;
}


static void vk_destroy_framebuffers( void ) {
	uint32_t n;

	if ( vk.framebuffers.main != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.main, NULL );
		vk.framebuffers.main = VK_NULL_HANDLE;
	}

	for ( n = 0; n < vk.swapchain_image_count; n++ ) {
		if ( vk.framebuffers.gamma[n] != VK_NULL_HANDLE ) {
			qvkDestroyFramebuffer( vk.device, vk.framebuffers.gamma[n], NULL );
			vk.framebuffers.gamma[n] = VK_NULL_HANDLE;
		}
	}

	if ( vk.framebuffers.bloom_extract != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.bloom_extract, NULL );
		vk.framebuffers.bloom_extract = VK_NULL_HANDLE;
	}

	if ( vk.framebuffers.screenmap != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.screenmap, NULL );
		vk.framebuffers.screenmap = VK_NULL_HANDLE;
	}

	for ( n = 0; n < ARRAY_LEN( vk.framebuffers.blur ); n++ ) {
		if ( vk.framebuffers.blur[n] != VK_NULL_HANDLE ) {
			qvkDestroyFramebuffer( vk.device, vk.framebuffers.blur[n], NULL );
			vk.framebuffers.blur[n] = VK_NULL_HANDLE;
		}
	}

	if ( vk.framebuffers.post_bloom != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.post_bloom, NULL );
		vk.framebuffers.post_bloom = VK_NULL_HANDLE;
	}
}


static void vk_destroy_swapchain( void ) {
	uint32_t i;

	for ( i = 0; i < vk.swapchain_image_count; i++ ) {
		if ( vk.swapchain_rendering_finished[i] != VK_NULL_HANDLE ) {
			qvkDestroySemaphore( vk.device, vk.swapchain_rendering_finished[i], NULL );
			vk.swapchain_rendering_finished[i] = VK_NULL_HANDLE;
		}
	}

	qvkDestroySwapchainKHR( vk.device, vk.swapchain, NULL );
}

static void vk_destroy_attachments( void );
static void vk_destroy_render_passes( void );
static void vk_destroy_pipelines( qboolean resetCount );
static qboolean vk_reallocate_xr_fbo_descriptors( void );
static void vk_destroy_hud_buffer( void );



static void vk_set_render_scale( void )
{
	if ( gls.windowWidth != glConfig.vidWidth || gls.windowHeight != glConfig.vidHeight )
	{
		if ( r_renderScale->integer > 0 )
		{
			int scaleMode = r_renderScale->integer - 1;
			if ( scaleMode & 1 )
			{
				// preserve aspect ratio (black bars on sides)
				float windowAspect = (float) gls.windowWidth / (float) gls.windowHeight;
				float renderAspect = (float) glConfig.vidWidth / (float) glConfig.vidHeight;
				if ( windowAspect >= renderAspect )
				{
					float scale = (float)gls.windowHeight / ( float ) glConfig.vidHeight;
					int bias = ( gls.windowWidth - scale * (float) glConfig.vidWidth ) / 2;
					vk.blitX0 += bias;
				}
				else
				{
					float scale = (float)gls.windowWidth / ( float ) glConfig.vidWidth;
					int bias = ( gls.windowHeight - scale * (float) glConfig.vidHeight ) / 2;
					vk.blitY0 += bias;
				}
			}
			// linear filtering
			if ( scaleMode & 2 )
				vk.blitFilter = GL_LINEAR;
			else
				vk.blitFilter = GL_NEAREST;
		}

		vk.windowAdjusted = qtrue;
	}

}



void vk_initialize( void )
{
	char buf[64], driver_version[64];
	const char *vendor_name;
	VkPhysicalDeviceProperties props;
	uint32_t major;
	uint32_t minor;
	uint32_t patch;
	uint32_t maxSize;
	uint32_t i;

	init_vulkan_library();

	qvkGetDeviceQueue( vk.device, vk.queue_family_index, 0, &vk.queue );

	qvkGetPhysicalDeviceProperties( vk.physical_device, &props );

	vk.cmd = vk.tess + 0;

	// identity until the first RB_BeginDrawingView() computes real per-view matrices
	Com_Memset( vk_view_eyeproj, 0, sizeof( vk_view_eyeproj ) );
	vk_view_eyeproj[0][0] = vk_view_eyeproj[0][5] = vk_view_eyeproj[0][10] = vk_view_eyeproj[0][15] = 1.0f;
	vk_view_eyeproj[1][0] = vk_view_eyeproj[1][5] = vk_view_eyeproj[1][10] = vk_view_eyeproj[1][15] = 1.0f;

	vk.uniform_alignment = props.limits.minUniformBufferOffsetAlignment;
	vk.uniform_item_size = PAD( (uint32_t)sizeof( vkUniform_t ), vk.uniform_alignment );

	// for flare visibility tests
	vk.storage_alignment = MAX( props.limits.minStorageBufferOffsetAlignment, sizeof( uint32_t ) );

	vk.maxAnisotropy = props.limits.maxSamplerAnisotropy;
	ri.Printf( PRINT_ALL, "...max anisotropy: %.0f\n", vk.maxAnisotropy );

	// r_gpuTimeLog: ticks to nanoseconds, zero where the graphics queue cannot timestamp
	vk.timestampPeriod = props.limits.timestampComputeAndGraphics ? props.limits.timestampPeriod : 0.0f;

	vk.blitFilter = GL_NEAREST;
	vk.windowAdjusted = qfalse;
	vk.blitX0 = vk.blitY0 = 0;

	vk_set_render_scale();

	// FBO is always active in VR for post-processing (r_fbo clamped to 1)
	vk.fboActive = qtrue;
	if ( r_ext_multisample->integer ) {
		vk.msaaActive = qtrue;
	}

	// multisampling
	// Use framebuffer sample count limits, not sampled image limits
	// These determine what sample counts are supported for color/depth attachments
	vkMaxSamples = MIN( props.limits.framebufferColorSampleCounts, props.limits.framebufferDepthSampleCounts );
	if ( glConfig.stencilBits > 0 ) {
		vkMaxSamples = MIN( vkMaxSamples, props.limits.framebufferStencilSampleCounts );
	}

	if ( vk.msaaActive ) {
		VkSampleCountFlags mask = vkMaxSamples;
		vkSamples = MAX( log2pad( r_ext_multisample->integer, 1 ), VK_SAMPLE_COUNT_2_BIT );
		while ( vkSamples > mask )
				vkSamples >>= 1;
		ri.Printf( PRINT_ALL, "...using %ix MSAA\n", vkSamples );
	} else {
		vkSamples = VK_SAMPLE_COUNT_1_BIT;
	}

	vk.screenMapSamples = MIN( vkMaxSamples, VK_SAMPLE_COUNT_4_BIT );

	vk.screenMapWidth = (float) glConfig.vidWidth / 16.0;
	if ( vk.screenMapWidth < 4 )
		vk.screenMapWidth = 4;

	vk.screenMapHeight = (float) glConfig.vidHeight / 16.0;
	if ( vk.screenMapHeight < 4 )
		vk.screenMapHeight = 4;

	vk.defaults.geometry_size = VERTEX_BUFFER_SIZE;
	vk.defaults.staging_size = STAGING_BUFFER_SIZE;

	// get memory size & defaults
	{
		VkPhysicalDeviceMemoryProperties props;
		VkDeviceSize maxDedicatedSize = 0;
		VkDeviceSize maxBARSize = 0;
		qvkGetPhysicalDeviceMemoryProperties( vk.physical_device, &props );
		for ( i = 0; i < props.memoryTypeCount; i++ ) {
			if ( props.memoryTypes[i].propertyFlags == VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ) {
				maxDedicatedSize = props.memoryHeaps[props.memoryTypes[i].heapIndex].size;
			}
			else if ( props.memoryTypes[i].propertyFlags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ) {
				if ( maxDedicatedSize == 0 || props.memoryHeaps[props.memoryTypes[i].heapIndex].size > maxDedicatedSize ) {
					maxDedicatedSize = props.memoryHeaps[props.memoryTypes[i].heapIndex].size;
				}
			}
			if ( props.memoryTypes[i].propertyFlags == (VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ) {
				maxBARSize = props.memoryHeaps[props.memoryTypes[i].heapIndex].size;
			}
			else if ( (props.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) == (VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ) {
				if ( maxBARSize == 0 ) {
					maxBARSize = props.memoryHeaps[props.memoryTypes[i].heapIndex].size;
				}
			}
		}

		if ( maxDedicatedSize != 0 ) {
			ri.Printf( PRINT_ALL, "...device memory size: %iMB\n", (int)((maxDedicatedSize + (1024 * 1024) - 1) / (1024 * 1024)) );
		}
		if ( maxBARSize != 0 ) {
			if ( maxBARSize >= 128 * 1024 * 1024 ) {
				// user larger buffers to avoid potential reallocations
				vk.defaults.geometry_size = VERTEX_BUFFER_SIZE_HI;
				vk.defaults.staging_size = STAGING_BUFFER_SIZE_HI;
			}
#ifdef _DEBUG
			ri.Printf( PRINT_ALL, "...BAR memory size: %iMB\n", (int)((maxBARSize + (1024 * 1024) - 1) / (1024 * 1024)) );
#endif
		}
	}

	// fill glConfig information

	// maxTextureSize must not exceed IMAGE_CHUNK_SIZE
	maxSize = sqrtf( IMAGE_CHUNK_SIZE / 4 );
	// round down to next power of 2
	glConfig.maxTextureSize = MIN( props.limits.maxImageDimension2D, log2pad( maxSize, 0 ) );

	if ( glConfig.maxTextureSize > MAX_TEXTURE_SIZE )
		glConfig.maxTextureSize = MAX_TEXTURE_SIZE; // ResampleTexture() relies on that maximum

	// default chunk size, may be doubled on demand
	vk.image_chunk_size = IMAGE_CHUNK_SIZE;

	vk.maxLod = 1 + Q_log2( glConfig.maxTextureSize );

	if ( props.limits.maxPerStageDescriptorSamplers != 0xFFFFFFFF )
		glConfig.numTextureUnits = props.limits.maxPerStageDescriptorSamplers;
	else
		glConfig.numTextureUnits = props.limits.maxBoundDescriptorSets;
	if ( glConfig.numTextureUnits > MAX_TEXTURE_UNITS )
		glConfig.numTextureUnits = MAX_TEXTURE_UNITS;

	vk.maxBoundDescriptorSets = props.limits.maxBoundDescriptorSets;

	if ( r_ext_texture_env_add->integer != 0 )
		glConfig.textureEnvAddAvailable = qtrue;
	else
		glConfig.textureEnvAddAvailable = qfalse;

	glConfig.textureCompression = TC_NONE;

	major = VK_VERSION_MAJOR(props.apiVersion);
	minor = VK_VERSION_MINOR(props.apiVersion);
	patch = VK_VERSION_PATCH(props.apiVersion);

	// decode driver version
	switch ( props.vendorID ) {
		case 0x10DE: // NVidia
			Com_sprintf( driver_version, sizeof( driver_version ), "%i.%i.%i.%i",
				(props.driverVersion >> 22) & 0x3FF,
				(props.driverVersion >> 14) & 0x0FF,
				(props.driverVersion >> 6) & 0x0FF,
				(props.driverVersion >> 0) & 0x03F );
			break;
#ifdef _WIN32
		case 0x8086: // Intel
			Com_sprintf( driver_version, sizeof( driver_version ), "%i.%i",
				(props.driverVersion >> 14),
				(props.driverVersion >> 0) & 0x3FFF );
			break;
#endif
		default:
			Com_sprintf( driver_version, sizeof( driver_version ), "%i.%i.%i",
				(props.driverVersion >> 22),
				(props.driverVersion >> 12) & 0x3FF,
				(props.driverVersion >> 0) & 0xFFF );
	}

	Com_sprintf( glConfig.version_string, sizeof( glConfig.version_string ), "API: %i.%i.%i, Driver: %s",
		major, minor, patch, driver_version );

#ifdef _WIN32
	// Intel iGPU drivers from 101.5333 to 101.6737 have a known bug that causes
	// VK_ERROR_DEVICE_LOST during vkQueueSubmit, see https://github.com/ec-/Quake3e/issues/312
	if ( props.vendorID == 0x8086 ) {
		uint32_t drvMajor = props.driverVersion >> 14;
		uint32_t drvMinor = props.driverVersion & 0x3FFF;
		if ( drvMajor == 101 && drvMinor >= 5333 && drvMinor <= 6737 ) {
			Com_sprintf( vk.driverNote, sizeof( vk.driverNote ), S_COLOR_WARNING
				"\nWARNING: Intel driver %i.%i is known to cause Vulkan crashes.\n"
				"Consider updating to driver >= 101.6790 or downgrading to <= 101.5186.\n",
				drvMajor, drvMinor );
		}
	}
#endif

	vk.offscreenRender = qtrue;

	if ( props.vendorID == 0x1002 ) {
		vendor_name = "Advanced Micro Devices, Inc.";
	} else if ( props.vendorID == 0x106B ) {
		vendor_name = "Apple Inc.";
	} else if ( props.vendorID == 0x10DE ) {
		// https://github.com/SaschaWillems/Vulkan/issues/493
		// we can't render to offscreen presentation surfaces on nvidia
		vk.offscreenRender = qfalse;
		vendor_name = "NVIDIA";
	} else if ( props.vendorID == 0x14E4 ) {
		vendor_name = "Broadcom Inc.";
	} else if ( props.vendorID == 0x1AE0 ) {
		vendor_name = "Google Inc.";
	} else if ( props.vendorID == 0x8086 ) {
		vendor_name = "Intel Corporation";
	} else if ( props.vendorID == VK_VENDOR_ID_MESA ) {
		vendor_name = "MESA";
	} else {
		Com_sprintf( buf, sizeof( buf ), "VendorID: %04x", props.vendorID );
		vendor_name = buf;
	}

	Q_strncpyz( glConfig.vendor_string, vendor_name, sizeof( glConfig.vendor_string ) );
	Q_strncpyz( glConfig.renderer_string, renderer_name( &props ), sizeof( glConfig.renderer_string ) );

	SET_OBJECT_NAME( (intptr_t)vk.device, glConfig.renderer_string, VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_EXT );

	// do early texture mode setup to avoid redundant descriptor updates in GL_SetDefaultState()
	vk.samplers.filter_min = -1;
	vk.samplers.filter_max = -1;
	GL_TextureMode( r_textureMode->string );
	r_textureMode->modified = qfalse;

	//
	// Sync primitives.
	//
	vk_create_sync_primitives();

	//
	// Command pool.
	//
	{
		VkCommandPoolCreateInfo desc;

		desc.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		desc.queueFamilyIndex = vk.queue_family_index;

		VK_CHECK( qvkCreateCommandPool( vk.device, &desc, NULL, &vk.command_pool ) );

		SET_OBJECT_NAME( vk.command_pool, "command pool", VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_POOL_EXT );
	}

#ifdef USE_UPLOAD_QUEUE
	{
		VkCommandBufferAllocateInfo alloc_info;

		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.pNext = NULL;
		alloc_info.commandPool = vk.command_pool;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = 1;

		VK_CHECK( qvkAllocateCommandBuffers( vk.device, &alloc_info, &vk.staging_command_buffer ) );
		SET_OBJECT_NAME( vk.staging_command_buffer, "staging cmd", VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT );
	}
#endif

	//
	// Command buffers and color attachments.
	//
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ )
	{
		VkCommandBufferAllocateInfo alloc_info;

		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.pNext = NULL;
		alloc_info.commandPool = vk.command_pool;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = 1;

		VK_CHECK( qvkAllocateCommandBuffers( vk.device, &alloc_info, &vk.tess[i].command_buffer ) );
		SET_OBJECT_NAME( vk.tess[i].command_buffer, va( "tess cmd %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT );
	}

	// Desktop mirror blit command buffer
	{
		VkCommandBufferAllocateInfo alloc_info;
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.pNext = NULL;
		alloc_info.commandPool = vk.command_pool;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = 1;
		VK_CHECK( qvkAllocateCommandBuffers( vk.device, &alloc_info, &vk.desktopBlitCmd ) );
		SET_OBJECT_NAME( vk.desktopBlitCmd, "desktop blit cmd", VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT );
	}

#ifdef USE_UPLOAD_QUEUE
	ri.Printf( PRINT_ALL, "Command buffers: staging=%p tess0=%p tess1=%p desktopBlit=%p\n",
		(void *)vk.staging_command_buffer, (void *)vk.tess[0].command_buffer,
		(void *)vk.tess[1].command_buffer, (void *)vk.desktopBlitCmd );
#else
	ri.Printf( PRINT_ALL, "Command buffers: tess0=%p tess1=%p desktopBlit=%p\n",
		(void *)vk.tess[0].command_buffer, (void *)vk.tess[1].command_buffer,
		(void *)vk.desktopBlitCmd );
#endif

	//
	// Descriptor pool.
	//
	{
		VkDescriptorPoolSize pool_size[3];
		VkDescriptorPoolCreateInfo desc;
		uint32_t i, maxSets;

		pool_size[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pool_size[0].descriptorCount = MAX_DRAWIMAGES + 1 + 1 + 1 + 1 + VK_NUM_BLOOM_PASSES * 2; // color, emissive, screenmap, bloom descriptors

		pool_size[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		pool_size[1].descriptorCount = NUM_COMMAND_BUFFERS * 2; // binding 0 (fog/dlight) + binding 1 (per-view eyeProj)

		//pool_size[2].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		//pool_size[2].descriptorCount = NUM_COMMAND_BUFFERS;

		pool_size[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
		pool_size[2].descriptorCount = 1;

		for ( i = 0, maxSets = 0; i < ARRAY_LEN( pool_size ); i++ ) {
			maxSets += pool_size[i].descriptorCount;
		}

		desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.maxSets = maxSets;
		desc.poolSizeCount = ARRAY_LEN( pool_size );
		desc.pPoolSizes = pool_size;

		VK_CHECK( qvkCreateDescriptorPool( vk.device, &desc, NULL, &vk.descriptor_pool ) );
	}

	//
	// Descriptor set layout.
	//
	vk_create_layout_binding( 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &vk.set_layout_sampler );
	{
		VkDescriptorSetLayoutBinding b[2];
		VkDescriptorSetLayoutCreateInfo ci;

		b[0].binding = 0;
		b[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		b[0].descriptorCount = 1;
		b[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
		b[0].pImmutableSamplers = NULL;

		// per-view eyeProj[2] (P_eye * E'_eye), written once per view
		b[1].binding = 1;
		b[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		b[1].descriptorCount = 1;
		b[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		b[1].pImmutableSamplers = NULL;

		ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		ci.pNext = NULL;
		ci.flags = 0;
		ci.bindingCount = 2;
		ci.pBindings = b;
		VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &ci, NULL, &vk.set_layout_uniform ) );
	}
	vk_create_layout_binding( 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, &vk.set_layout_storage );
	//vk_create_layout_binding( 0, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT, &vk.set_layout_input );

	//
	// Pipeline layouts.
	//
	{
		VkDescriptorSetLayout set_layouts[6];
		VkPipelineLayoutCreateInfo desc;
		VkPushConstantRange push_ranges[2];

		// vertex stage: mono modelview; per-eye projection lives in set 0 binding 1
		push_ranges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		push_ranges[0].offset = 0;
		push_ranges[0].size = 64;

		// fragment stage: runtime emissiveFactor selecting HDR emissive-layer output per draw
		push_ranges[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		push_ranges[1].offset = 64;
		push_ranges[1].size = 4;    // float emissiveFactor

		// standard pipelines
		set_layouts[0] = vk.set_layout_uniform; // fog/dlight parameters
		set_layouts[1] = vk.set_layout_sampler; // diffuse
		set_layouts[2] = vk.set_layout_sampler; // lightmap / fog-only
		set_layouts[3] = vk.set_layout_sampler; // blend
		set_layouts[4] = vk.set_layout_sampler; // collapsed fog texture

		desc.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.setLayoutCount = (vk.maxBoundDescriptorSets >= VK_DESC_COUNT) ? VK_DESC_COUNT : 4;
		desc.pSetLayouts = set_layouts;
		desc.pushConstantRangeCount = 2;
		desc.pPushConstantRanges = push_ranges;

		VK_CHECK(qvkCreatePipelineLayout(vk.device, &desc, NULL, &vk.pipeline_layout));

		// flare test pipeline
		set_layouts[0] = vk.set_layout_storage; // dynamic storage buffer

		desc.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.setLayoutCount = 1;
		desc.pSetLayouts = set_layouts;
		desc.pushConstantRangeCount = 2;
		desc.pPushConstantRanges = push_ranges;

		VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.pipeline_layout_storage ) );

		// post-processing pipeline
		set_layouts[0] = vk.set_layout_sampler; // sampler
		set_layouts[1] = vk.set_layout_sampler; // sampler
		set_layouts[2] = vk.set_layout_sampler; // sampler
		set_layouts[3] = vk.set_layout_sampler; // sampler

		desc.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.setLayoutCount = 1;
		desc.pSetLayouts = set_layouts;
		desc.pushConstantRangeCount = 0;
		desc.pPushConstantRanges = NULL;

		VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.pipeline_layout_post_process ) );

		desc.setLayoutCount = VK_NUM_BLOOM_PASSES;

		VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.pipeline_layout_blend ) );

		// virtual screen pipeline layout: same split-transform shape as the
		// standard pipelines (set 0 uniform + eyeProj, set 1 sampler, 64-byte
		// mono modelview push); the eyeProj content is the virtual screen's
		// own XR-space pair, pushed at end-frame
		set_layouts[0] = vk.set_layout_uniform;
		set_layouts[1] = vk.set_layout_sampler;

		desc.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.setLayoutCount = 2;
		desc.pSetLayouts = set_layouts;
		desc.pushConstantRangeCount = 1;
		desc.pPushConstantRanges = &push_ranges[0];

		VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.pipeline_layout_virtual_screen ) );

		SET_OBJECT_NAME( vk.pipeline_layout, "pipeline layout - main", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );
		SET_OBJECT_NAME( vk.pipeline_layout_post_process, "pipeline layout - post-processing", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );
		SET_OBJECT_NAME( vk.pipeline_layout_blend, "pipeline layout - blend", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );
		SET_OBJECT_NAME( vk.pipeline_layout_virtual_screen, "pipeline layout - virtual screen", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );
	}

	// Create general-purpose linear sampler (used by desktop mirror, virtual screen, etc.)
	{
		VkSamplerCreateInfo samplerCI;

		Com_Memset( &samplerCI, 0, sizeof( samplerCI ) );
		samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCI.magFilter = VK_FILTER_LINEAR;
		samplerCI.minFilter = VK_FILTER_LINEAR;
		samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

		VK_CHECK( qvkCreateSampler( vk.device, &samplerCI, NULL, &vk.linearSampler ) );
		SET_OBJECT_NAME( vk.linearSampler, "Linear sampler", VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT );
	}

	vk.geometry_buffer_size_new = vk.defaults.geometry_size;
	vk_create_geometry_buffers( vk.geometry_buffer_size_new );
	vk.geometry_buffer_size_new = 0;

	vk_create_storage_buffer( MAX_FLARES * vk.storage_alignment );

	vk_create_shader_modules();

	{
		VkPipelineCacheCreateInfo ci;
		Com_Memset( &ci, 0, sizeof( ci ) );
		ci.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		VK_CHECK( qvkCreatePipelineCache( vk.device, &ci, NULL, &vk.pipelineCache ) );
	}

	vk.renderPassIndex = RENDER_PASS_MAIN; // default render pass

	// swapchain
	ri.Printf( PRINT_ALL, "Creating desktop swapchain...\n" );
	fflush( stdout );
	vk.initSwapchainLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	//vk.initSwapchainLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	vk_create_swapchain( vk.physical_device, vk.device, vk_surface, vk.present_format, &vk.swapchain, qtrue );
	ri.Printf( PRINT_ALL, "Desktop swapchain created\n" );
	fflush( stdout );

	// color/depth attachments
	ri.Printf( PRINT_ALL, "Creating attachments...\n" );
	fflush( stdout );
	vk_create_attachments();
	ri.Printf( PRINT_ALL, "Attachments created\n" );
	fflush( stdout );

	// renderpasses
	ri.Printf( PRINT_ALL, "Creating render passes...\n" );
	fflush( stdout );
	vk_create_render_passes();
	ri.Printf( PRINT_ALL, "Render passes created\n" );
	fflush( stdout );

	// framebuffers for each swapchain image
	ri.Printf( PRINT_ALL, "Creating framebuffers...\n" );
	fflush( stdout );
	vk_create_framebuffers();
	ri.Printf( PRINT_ALL, "Framebuffers created\n" );
	fflush( stdout );

	// preallocate staging buffer
	if ( vk.defaults.staging_size == STAGING_BUFFER_SIZE_HI ) {
		vk_alloc_staging_buffer( vk.defaults.staging_size );
	}

	ri.Printf( PRINT_ALL, "Vulkan initialization complete\n" );
	fflush( stdout );
	vk.active = qtrue;
}


void vk_create_pipelines( void )
{
	vk_alloc_persistent_pipelines();

	vk.pipelines_world_base = vk.pipelines_count;
}


static void vk_destroy_attachments( void )
{
	uint32_t i;

	if ( vk.bloom_image[0] ) {
		for ( i = 0; i < ARRAY_LEN( vk.bloom_image ); i++ ) {
			qvkDestroyImage( vk.device, vk.bloom_image[i], NULL );
			qvkDestroyImageView( vk.device, vk.bloom_image_view[i], NULL );
			vk.bloom_image[i] = VK_NULL_HANDLE;
			vk.bloom_image_view[i] = VK_NULL_HANDLE;
		}
	}

	if ( vk.color_image ) {
		qvkDestroyImage( vk.device, vk.color_image, NULL );
		qvkDestroyImageView( vk.device, vk.color_image_view, NULL );
		vk.color_image = VK_NULL_HANDLE;
		vk.color_image_view = VK_NULL_HANDLE;
	}

	if ( vk.emissive_image ) {
		qvkDestroyImage( vk.device, vk.emissive_image, NULL );
		qvkDestroyImageView( vk.device, vk.emissive_image_view, NULL );
		vk.emissive_image = VK_NULL_HANDLE;
		vk.emissive_image_view = VK_NULL_HANDLE;
	}

	if ( vk.emissive_image_msaa ) {
		qvkDestroyImage( vk.device, vk.emissive_image_msaa, NULL );
		qvkDestroyImageView( vk.device, vk.emissive_image_view_msaa, NULL );
		vk.emissive_image_msaa = VK_NULL_HANDLE;
		vk.emissive_image_view_msaa = VK_NULL_HANDLE;
	}

	if ( vk.msaa_image ) {
		qvkDestroyImage( vk.device, vk.msaa_image, NULL );
		qvkDestroyImageView( vk.device, vk.msaa_image_view, NULL );
		vk.msaa_image = VK_NULL_HANDLE;
		vk.msaa_image_view = VK_NULL_HANDLE;
	}

	qvkDestroyImage( vk.device, vk.depth_image, NULL );
	qvkDestroyImageView( vk.device, vk.depth_image_view, NULL );
	vk.depth_image = VK_NULL_HANDLE;
	vk.depth_image_view = VK_NULL_HANDLE;

	if ( vk.screenMap.color_image ) {
		qvkDestroyImage( vk.device, vk.screenMap.color_image, NULL );
		qvkDestroyImageView( vk.device, vk.screenMap.color_image_view, NULL );
		vk.screenMap.color_image = VK_NULL_HANDLE;
		vk.screenMap.color_image_view = VK_NULL_HANDLE;
	}

	if ( vk.screenMap.color_image_msaa ) {
		qvkDestroyImage( vk.device, vk.screenMap.color_image_msaa, NULL );
		qvkDestroyImageView( vk.device, vk.screenMap.color_image_view_msaa, NULL );
		vk.screenMap.color_image_msaa = VK_NULL_HANDLE;
		vk.screenMap.color_image_view_msaa = VK_NULL_HANDLE;
	}

	if ( vk.screenMap.depth_image ) {
		qvkDestroyImage( vk.device, vk.screenMap.depth_image, NULL );
		qvkDestroyImageView( vk.device, vk.screenMap.depth_image_view, NULL );
		vk.screenMap.depth_image = VK_NULL_HANDLE;
		vk.screenMap.depth_image_view = VK_NULL_HANDLE;
	}

	if ( vk.capture.image ) {
		qvkDestroyImage( vk.device, vk.capture.image, NULL );
		qvkDestroyImageView( vk.device, vk.capture.image_view, NULL );
		vk.capture.image = VK_NULL_HANDLE;
		vk.capture.image_view = VK_NULL_HANDLE;
	}

	for ( i = 0; i < vk.image_memory_count; i++ ) {
		qvkFreeMemory( vk.device, vk.image_memory[i], NULL );
	}

	vk.image_memory_count = 0;
}


static void vk_destroy_render_passes( void )
{
	uint32_t i;

	// Main multiview render passes
	if ( vk.render_pass.main != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.main, NULL );
		vk.render_pass.main = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.mainResume != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.mainResume, NULL );
		vk.render_pass.mainResume = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.screenmap != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.screenmap, NULL );
		vk.render_pass.screenmap = VK_NULL_HANDLE;
	}

	// Post-processing render passes (multiview)
	if ( vk.render_pass.gamma != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.gamma, NULL );
		vk.render_pass.gamma = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.bloom_extract != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.bloom_extract, NULL );
		vk.render_pass.bloom_extract = VK_NULL_HANDLE;
	}

	for ( i = 0; i < ARRAY_LEN( vk.render_pass.blur ); i++ ) {
		if ( vk.render_pass.blur[i] != VK_NULL_HANDLE ) {
			qvkDestroyRenderPass( vk.device, vk.render_pass.blur[i], NULL );
			vk.render_pass.blur[i] = VK_NULL_HANDLE;
		}
	}

	if ( vk.render_pass.post_bloom != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.post_bloom, NULL );
		vk.render_pass.post_bloom = VK_NULL_HANDLE;
	}

	// HUD buffer render pass
	if ( vk.render_pass.hudBuffer != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.hudBuffer, NULL );
		vk.render_pass.hudBuffer = VK_NULL_HANDLE;
	}

	// Virtual screen render pass
	if ( vk.render_pass.virtualScreen != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.virtualScreen, NULL );
		vk.render_pass.virtualScreen = VK_NULL_HANDLE;
	}
}


static void vk_destroy_pipelines( qboolean resetCounter )
{
	uint32_t i, j;

	for ( i = 0; i < vk.pipelines_count; i++ ) {
		for ( j = 0; j < RENDER_PASS_COUNT; j++ ) {
			if ( vk.pipelines[i].handle[j] != VK_NULL_HANDLE ) {
				qvkDestroyPipeline( vk.device, vk.pipelines[i].handle[j], NULL );
				vk.pipelines[i].handle[j] = VK_NULL_HANDLE;
				vk.pipeline_create_count--;
			}
		}
	}

	if ( resetCounter ) {
		Com_Memset( &vk.pipelines, 0, sizeof( vk.pipelines ) );
		vk.pipelines_count = 0;
	}

	if ( vk.gamma_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.gamma_pipeline, NULL );
		vk.gamma_pipeline = VK_NULL_HANDLE;
	}

	if ( vk.bloom_extract_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.bloom_extract_pipeline, NULL );
		vk.bloom_extract_pipeline = VK_NULL_HANDLE;
	}

	if ( vk.bloom_blend_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.bloom_blend_pipeline, NULL );
		vk.bloom_blend_pipeline = VK_NULL_HANDLE;
	}

	for ( i = 0; i < ARRAY_LEN( vk.blur_pipeline ); i++ ) {
		if ( vk.blur_pipeline[i] != VK_NULL_HANDLE ) {
			qvkDestroyPipeline( vk.device, vk.blur_pipeline[i], NULL );
			vk.blur_pipeline[i] = VK_NULL_HANDLE;
		}
	}
}


void vk_shutdown( refShutdownCode_t code )
{
	int i, j, k, l;

	if ( qvkQueuePresentKHR == NULL ) { // not fully initialized
		goto __cleanup;
	}

	// Note: vk_shutdown_xr_resources() is called earlier in RE_Shutdown(),
	// before VKimp_Shutdown() destroys XR swapchains. This ensures VkImageViews
	// are destroyed while XR swapchain images are still valid.

	vk_destroy_framebuffers();

	vk_destroy_pipelines( qtrue ); // reset counter

	vk_destroy_render_passes();

	vk_destroy_attachments();

	vk_destroy_swapchain();

	if ( vk.pipelineCache != VK_NULL_HANDLE ) {
		qvkDestroyPipelineCache( vk.device, vk.pipelineCache, NULL );
		vk.pipelineCache = VK_NULL_HANDLE;
	}

	qvkDestroyCommandPool( vk.device, vk.command_pool, NULL );

	qvkDestroyDescriptorPool(vk.device, vk.descriptor_pool, NULL);

	qvkDestroyDescriptorSetLayout(vk.device, vk.set_layout_sampler, NULL);
	qvkDestroyDescriptorSetLayout(vk.device, vk.set_layout_uniform, NULL);
	qvkDestroyDescriptorSetLayout(vk.device, vk.set_layout_storage, NULL);

	qvkDestroyPipelineLayout(vk.device, vk.pipeline_layout, NULL);
	qvkDestroyPipelineLayout(vk.device, vk.pipeline_layout_storage, NULL);
	qvkDestroyPipelineLayout(vk.device, vk.pipeline_layout_post_process, NULL);
	qvkDestroyPipelineLayout(vk.device, vk.pipeline_layout_blend, NULL);
	qvkDestroyPipelineLayout(vk.device, vk.pipeline_layout_virtual_screen, NULL);

#ifdef USE_VBO
	vk_release_vbo();
#endif

	vk_clean_staging_buffer();

	vk_release_geometry_buffers();

	vk_destroy_samplers();

	// Destroy general-purpose linear sampler (created in vk_initialize)
	if ( vk.linearSampler != VK_NULL_HANDLE ) {
		qvkDestroySampler( vk.device, vk.linearSampler, NULL );
		vk.linearSampler = VK_NULL_HANDLE;
	}

	vk_destroy_sync_primitives();

	qvkDestroyBuffer( vk.device, vk.storage.buffer, NULL );
	qvkFreeMemory( vk.device, vk.storage.memory, NULL );

	for ( i = 0; i < 3; i++ ) {
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				for ( l = 0; l < 2; l++ ) {
					if ( vk.modules.vert.gen[i][j][k][l] != VK_NULL_HANDLE ) {
						qvkDestroyShaderModule( vk.device, vk.modules.vert.gen[i][j][k][l], NULL );
						vk.modules.vert.gen[i][j][k][l] = VK_NULL_HANDLE;
					}
				}
			}
		}
	}
	for ( i = 0; i < 3; i++ ) {
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				for ( l = 0; l < 2; l++ ) {
					if ( vk.modules.frag.gen[i][j][k][l] != VK_NULL_HANDLE ) {
						qvkDestroyShaderModule( vk.device, vk.modules.frag.gen[i][j][k][l], NULL );
						vk.modules.frag.gen[i][j][k][l] = VK_NULL_HANDLE;
					}
				}
			}
		}
	}
	for ( i = 0; i < 2; i++ ) {
		if ( vk.modules.vert.light[i] != VK_NULL_HANDLE ) {
			qvkDestroyShaderModule( vk.device, vk.modules.vert.light[i], NULL );
			vk.modules.vert.light[i] = VK_NULL_HANDLE;
		}
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				if ( vk.modules.frag.light[i][j][k] != VK_NULL_HANDLE ) {
					qvkDestroyShaderModule( vk.device, vk.modules.frag.light[i][j][k], NULL );
					vk.modules.frag.light[i][j][k] = VK_NULL_HANDLE;
				}
			}
		}
	}

	for ( i = 0; i < 2; i++ ) {
		if ( vk.modules.vert.overbright_vert[i] != VK_NULL_HANDLE ) {
			qvkDestroyShaderModule( vk.device, vk.modules.vert.overbright_vert[i], NULL );
			vk.modules.vert.overbright_vert[i] = VK_NULL_HANDLE;
		}
		for ( j = 0; j < 2; j++ ) {
			if ( vk.modules.frag.overbright_frag[i][j] != VK_NULL_HANDLE ) {
				qvkDestroyShaderModule( vk.device, vk.modules.frag.overbright_frag[i][j], NULL );
				vk.modules.frag.overbright_frag[i][j] = VK_NULL_HANDLE;
			}
		}
	}

	for ( i = 0; i < 2; i++ ) {
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				qvkDestroyShaderModule( vk.device, vk.modules.vert.ident1[i][j][k], NULL );
				vk.modules.vert.ident1[i][j][k] = VK_NULL_HANDLE;
				qvkDestroyShaderModule( vk.device, vk.modules.frag.ident1[i][j][k], NULL );
				vk.modules.frag.ident1[i][j][k] = VK_NULL_HANDLE;
			}
		}
	}

	for ( i = 0; i < 2; i++ ) {
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				qvkDestroyShaderModule( vk.device, vk.modules.vert.fixed[i][j][k], NULL );
				vk.modules.vert.fixed[i][j][k] = VK_NULL_HANDLE;
				qvkDestroyShaderModule( vk.device, vk.modules.frag.fixed[i][j][k], NULL );
				vk.modules.frag.fixed[i][j][k] = VK_NULL_HANDLE;
			}
		}
	}

	for ( i = 0; i < 1; i++ ) {
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				qvkDestroyShaderModule( vk.device, vk.modules.frag.ent[i][j][k], NULL );
				vk.modules.frag.ent[i][j][k] = VK_NULL_HANDLE;
			}
		}
	}

	qvkDestroyShaderModule( vk.device, vk.modules.frag.gen0_df, NULL );

	qvkDestroyShaderModule( vk.device, vk.modules.color_fs, NULL );
	qvkDestroyShaderModule( vk.device, vk.modules.color_vs, NULL );

	qvkDestroyShaderModule(vk.device, vk.modules.fog_vs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.fog_fs, NULL);

	qvkDestroyShaderModule(vk.device, vk.modules.dot_vs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.dot_fs, NULL);

	qvkDestroyShaderModule(vk.device, vk.modules.bloom_fs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.blur_fs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.blend_fs, NULL);

	qvkDestroyShaderModule(vk.device, vk.modules.gamma_vs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.gamma_fs, NULL);

	qvkDestroyShaderModule(vk.device, vk.modules.virtualscreen_vs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.virtualscreen_fs, NULL);

	qvkDestroyShaderModule(vk.device, vk.modules.floor_grid_vs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.floor_grid_fs, NULL);

	qvkDestroyShaderModule(vk.device, vk.modules.desktopmirror_vs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.desktopmirror_fs, NULL);

__cleanup:
	if ( vk.device != VK_NULL_HANDLE ) {
		qvkDestroyDevice( vk.device, NULL );
	}

	deinit_device_functions();

	Com_Memset( &vk, 0, sizeof( vk ) );
	Com_Memset( &vk_world, 0, sizeof( vk_world ) );
	
	if ( code != REF_KEEP_CONTEXT ) {
		vk_destroy_instance();
		deinit_instance_functions();
	}
}


void vk_wait_idle( void )
{
	VK_CHECK( qvkDeviceWaitIdle( vk.device ) );
}


void vk_queue_wait_idle( void )
{
	VK_CHECK( qvkQueueWaitIdle( vk.queue ) );
}


// Precondition: callers must end (discard or finish) any in-flight frame
// first: the pool reset below frees every set, and descriptorsReady only
// guards frames that haven't started yet.
void vk_release_resources( void ) {
	int i, j;

	vk_wait_idle();

	for (i = 0; i < vk_world.num_image_chunks; i++)
		qvkFreeMemory(vk.device, vk_world.image_chunks[i].memory, NULL);

	vk_clean_staging_buffer();

	// vk_destroy_samplers();

	for ( i = vk.pipelines_world_base; i < vk.pipelines_count; i++ ) {
		for ( j = 0; j < RENDER_PASS_COUNT; j++ ) {
			if ( vk.pipelines[i].handle[j] != VK_NULL_HANDLE ) {
				qvkDestroyPipeline( vk.device, vk.pipelines[i].handle[j], NULL );
				vk.pipelines[i].handle[j] = VK_NULL_HANDLE;
				vk.pipeline_create_count--;
			}
		}
		Com_Memset( &vk.pipelines[i], 0, sizeof( vk.pipelines[0] ) );
	}
	vk.pipelines_count = vk.pipelines_world_base;

	VK_CHECK( qvkResetDescriptorPool( vk.device, vk.descriptor_pool, 0 ) );

	// Pool reset freed every set. vk_reallocate_xr_fbo_descriptors() below
	// restores only the sampler-layout sets; the rest stay dead until R_Init
	// reruns vk_init_descriptors(). NULL them so a gate bypass binds NULL
	// rather than a freed handle.
	vk.descriptorsReady = qfalse;
	vk.storage.descriptor = VK_NULL_HANDLE;
	vk.emissive_descriptor = VK_NULL_HANDLE;
	vk.screenMap.color_descriptor = VK_NULL_HANDLE;
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		vk.tess[i].uniform_descriptor = VK_NULL_HANDLE;
	}

	// Reallocate XR descriptor sets that were invalidated by pool reset
	// (includes FBO color, bloom, desktop mirror, and virtual screen mirror descriptors)
	vk_reallocate_xr_fbo_descriptors();

	if ( vk_world.num_image_chunks > 1 ) {
		// if we allocated more than 2 image chunks - use doubled default size
		vk.image_chunk_size = (IMAGE_CHUNK_SIZE * 2);
	}

	Com_Memset( &vk_world, 0, sizeof( vk_world ) );

	// Reset geometry buffers offsets
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		vk.tess[i].uniform_read_offset = 0;
		vk.tess[i].vertex_buffer_offset = 0;
	}

	Com_Memset( vk.cmd->buf_offset, 0, sizeof( vk.cmd->buf_offset ) );
	Com_Memset( vk.cmd->vbo_offset, 0, sizeof( vk.cmd->vbo_offset ) );

	Com_Memset( &vk.stats, 0, sizeof( vk.stats ) );
}


void vk_create_image( image_t *image, int width, int height, int mip_levels ) {

	VkFormat format = image->internalFormat;

	if ( image->handle ) {
		qvkDestroyImage( vk.device, image->handle, NULL );
		image->handle = VK_NULL_HANDLE;
	}

	if ( image->view ) {
		qvkDestroyImageView( vk.device, image->view, NULL );
		image->view = VK_NULL_HANDLE;
	}

	// create image
	{
		VkImageCreateInfo desc;

		desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.imageType = VK_IMAGE_TYPE_2D;
		desc.format = format;
		desc.extent.width = width;
		desc.extent.height = height;
		desc.extent.depth = 1;
		desc.mipLevels = mip_levels;
		desc.arrayLayers = 1;
		desc.samples = VK_SAMPLE_COUNT_1_BIT;
		desc.tiling = VK_IMAGE_TILING_OPTIMAL;
		desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		desc.queueFamilyIndexCount = 0;
		desc.pQueueFamilyIndices = NULL;
		desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VK_CHECK( qvkCreateImage( vk.device, &desc, NULL, &image->handle ) );

		allocate_and_bind_image_memory( image->handle );
	}

	// create image view
	{
		VkImageViewCreateInfo desc;

		desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.image = image->handle;
		desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
		desc.format = format;
		desc.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		desc.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		desc.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		desc.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		desc.subresourceRange.baseMipLevel = 0;
		desc.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		desc.subresourceRange.baseArrayLayer = 0;
		desc.subresourceRange.layerCount = 1;

		VK_CHECK( qvkCreateImageView( vk.device, &desc, NULL, &image->view ) );
	}

	// create associated descriptor set
	if ( image->descriptor == VK_NULL_HANDLE ) {
		VkDescriptorSetAllocateInfo desc;

		desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		desc.pNext = NULL;
		desc.descriptorPool = vk.descriptor_pool;
		desc.descriptorSetCount = 1;
		desc.pSetLayouts = &vk.set_layout_sampler;

		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &desc, &image->descriptor ) );
	}

	vk_update_descriptor_set( image, mip_levels > 1 ? qtrue : qfalse );

	SET_OBJECT_NAME( image->handle, image->imgName, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	SET_OBJECT_NAME( image->view, image->imgName, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	SET_OBJECT_NAME( image->descriptor, image->imgName, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
}


static byte *resample_image_data( const int target_format, byte *data, const int data_size, int *bytes_per_pixel )
{
	byte* buffer;
	uint16_t* p;
	int i, n;

	switch ( target_format ) {
	case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
		buffer = (byte*)ri.Hunk_AllocateTempMemory( data_size / 2 );
		p = (uint16_t*)buffer;
		for ( i = 0; i < data_size; i += 4, p++ ) {
			byte r = data[i + 0];
			byte g = data[i + 1];
			byte b = data[i + 2];
			byte a = data[i + 3];
			*p = (uint32_t)((a / 255.0) * 15.0 + 0.5) |
				((uint32_t)((r / 255.0) * 15.0 + 0.5) << 4) |
				((uint32_t)((g / 255.0) * 15.0 + 0.5) << 8) |
				((uint32_t)((b / 255.0) * 15.0 + 0.5) << 12);
		}
		*bytes_per_pixel = 2;
		return buffer; // must be freed after upload!

	case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
		buffer = (byte*)ri.Hunk_AllocateTempMemory( data_size / 2 );
		p = (uint16_t*)buffer;
		for ( i = 0; i < data_size; i += 4, p++ ) {
			byte r = data[i + 0];
			byte g = data[i + 1];
			byte b = data[i + 2];
			*p = (uint32_t)((b / 255.0) * 31.0 + 0.5) |
				((uint32_t)((g / 255.0) * 31.0 + 0.5) << 5) |
				((uint32_t)((r / 255.0) * 31.0 + 0.5) << 10) |
				(1 << 15);
		}
		*bytes_per_pixel = 2;
		return buffer; // must be freed after upload!

	case VK_FORMAT_B8G8R8A8_UNORM:
		buffer = (byte*)ri.Hunk_AllocateTempMemory( data_size );
		for ( i = 0; i < data_size; i += 4 ) {
			buffer[i + 0] = data[i + 2];
			buffer[i + 1] = data[i + 1];
			buffer[i + 2] = data[i + 0];
			buffer[i + 3] = data[i + 3];
		}
		*bytes_per_pixel = 4;
		return buffer;

	case VK_FORMAT_R8G8B8_UNORM: {
		buffer = (byte*)ri.Hunk_AllocateTempMemory( (data_size * 3) / 4 );
		for ( i = 0, n = 0; i < data_size; i += 4, n += 3 ) {
			buffer[n + 0] = data[i + 0];
			buffer[n + 1] = data[i + 1];
			buffer[n + 2] = data[i + 2];
		}
		*bytes_per_pixel = 3;
		return buffer;
	}

	default:
		*bytes_per_pixel = 4;
		return data;
	}
}


void vk_upload_image_data( image_t *image, int x, int y, int width, int height, int mipmaps, byte *pixels, int size, qboolean update ) {

	VkCommandBuffer   command_buffer;
	VkBufferImageCopy regions[16];
	VkBufferImageCopy region;
	byte *buf;
	int n;

	int num_regions = 0;
	int buffer_size = 0;

	buf = resample_image_data( image->internalFormat, pixels, size, &n /*bpp*/ );

	while (qtrue) {
		Com_Memset(&region, 0, sizeof(region));
		region.bufferOffset = buffer_size;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = num_regions;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset.x = x;
		region.imageOffset.y = y;
		region.imageOffset.z = 0;
		region.imageExtent.width = width;
		region.imageExtent.height = height;
		region.imageExtent.depth = 1;

		regions[num_regions] = region;
		num_regions++;

		buffer_size += width * height * n;

		if ( num_regions >= mipmaps || (width == 1 && height == 1) || num_regions >= ARRAY_LEN( regions ) )
			break;

		x >>= 1;
		y >>= 1;

		width >>= 1;
		if (width < 1) width = 1;

		height >>= 1;
		if (height < 1) height = 1;
	}

#ifdef USE_UPLOAD_QUEUE
	if ( vk_wait_staging_buffer() ) {
		// wait for vkQueueSubmit() completion before new upload
	}

	if ( vk.staging_buffer.size - vk.staging_buffer.offset < buffer_size ) {
		// try to flush staging buffer and reset offset
		vk_flush_staging_buffer( qfalse );
	}

	if ( vk.staging_buffer.size /* - vk_world.staging_buffer_offset */ < buffer_size ) {
		// if still not enough - reallocate staging buffer
		vk_alloc_staging_buffer( buffer_size );
	}

	for ( n = 0; n < num_regions; n++ ) {
		regions[n].bufferOffset += vk.staging_buffer.offset;
	}

	Com_Memcpy( vk.staging_buffer.ptr + vk.staging_buffer.offset, buf, buffer_size );

	if ( vk.staging_buffer.offset == 0 ) {
		VkCommandBufferBeginInfo begin_info;
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.pNext = NULL;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		begin_info.pInheritanceInfo = NULL;
		VK_CHECK( qvkBeginCommandBuffer( vk.staging_command_buffer, &begin_info ) );
	}

	//ri.Printf( PRINT_WARNING, "batch @%6i + %i %s \n", (int)vk_world.staging_buffer_offset, (int)buffer_size, image->imgName );
	vk.staging_buffer.offset += buffer_size;

	command_buffer = vk.staging_command_buffer;

	if ( update ) {
		record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );
	} else {
		record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT, 0 );
	}

	qvkCmdCopyBufferToImage( command_buffer, vk.staging_buffer.handle, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, num_regions, regions );

	// final transition after upload comleted
	record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
#else
	if ( vk.staging_buffer.size < buffer_size ) {
		vk_alloc_staging_buffer( buffer_size );
	}

	Com_Memcpy( vk.staging_buffer.ptr, buf, buffer_size );

	command_buffer = begin_command_buffer();
	// record_buffer_memory_barrier( command_buffer, vk_world.staging_buffer, VK_WHOLE_SIZE, 0, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT );
	if ( update ) {
		record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );
	} else {
		record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT, 0 );
	}
	qvkCmdCopyBufferToImage( command_buffer, vk.staging_buffer.handle, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, num_regions, regions );
	record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
	end_command_buffer( command_buffer, __func__ );
#endif

	if ( buf != pixels ) {
		ri.Hunk_FreeTempMemory( buf );
	}
}


void vk_update_descriptor_set( image_t *image, qboolean mipmap ) {
	Vk_Sampler_Def sampler_def;
	VkDescriptorImageInfo image_info;
	VkWriteDescriptorSet descriptor_write;

	Com_Memset( &sampler_def, 0, sizeof( sampler_def ) );

	sampler_def.address_mode = image->wrapClampMode;

	if ( mipmap ) {
		sampler_def.gl_mag_filter = gl_filter_max;
		sampler_def.gl_min_filter = gl_filter_min;
	} else {
		sampler_def.gl_mag_filter = GL_LINEAR;
		sampler_def.gl_min_filter = GL_LINEAR;
		// no anisotropy without mipmaps
		sampler_def.noAnisotropy = qtrue;
	}

	image_info.sampler = vk_find_sampler( &sampler_def );
	image_info.imageView = image->view;
	image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptor_write.dstSet = image->descriptor;
	descriptor_write.dstBinding = 0;
	descriptor_write.dstArrayElement = 0;
	descriptor_write.descriptorCount = 1;
	descriptor_write.pNext = NULL;
	descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptor_write.pImageInfo = &image_info;
	descriptor_write.pBufferInfo = NULL;
	descriptor_write.pTexelBufferView = NULL;

	qvkUpdateDescriptorSets( vk.device, 1, &descriptor_write, 0, NULL );
}


void vk_destroy_image_resources( VkImage *image, VkImageView *imageView )
{
	if ( image != NULL ) {
		if ( *image != VK_NULL_HANDLE ) {
			qvkDestroyImage( vk.device, *image, NULL );
			*image = VK_NULL_HANDLE;
		}
	}
	if ( imageView != NULL ) {
		if ( *imageView != VK_NULL_HANDLE ) {
			qvkDestroyImageView( vk.device, *imageView, NULL );
			*imageView = VK_NULL_HANDLE;
		}
	}
}


static void set_shader_stage_desc(VkPipelineShaderStageCreateInfo *desc, VkShaderStageFlagBits stage, VkShaderModule shader_module, const char *entry) {
	desc->sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	desc->pNext = NULL;
	desc->flags = 0;
	desc->stage = stage;
	desc->module = shader_module;
	desc->pName = entry;
	desc->pSpecializationInfo = NULL;
}


#define FORMAT_DEPTH(format, r_bits, g_bits, b_bits) case(VK_FORMAT_##format): *r = r_bits; *b = b_bits; *g = g_bits; return qtrue;
static qboolean vk_surface_format_color_depth( VkFormat format, int *r, int *g, int *b ) {
	switch (format) {
		// Common formats from https://vulkan.gpuinfo.org/listsurfaceformats.php
		FORMAT_DEPTH(B8G8R8A8_UNORM, 255, 255, 255)
			FORMAT_DEPTH(B8G8R8A8_SRGB, 255, 255, 255)
			FORMAT_DEPTH(A2B10G10R10_UNORM_PACK32, 1023, 1023, 1023)
			FORMAT_DEPTH(R8G8B8A8_UNORM, 255, 255, 255)
			FORMAT_DEPTH(R8G8B8A8_SRGB, 255, 255, 255)
			FORMAT_DEPTH(A2R10G10B10_UNORM_PACK32, 1023, 1023, 1023)
			FORMAT_DEPTH(R5G6B5_UNORM_PACK16, 31, 63, 31)
			FORMAT_DEPTH(R8G8B8A8_SNORM, 255, 255, 255)
			FORMAT_DEPTH(A8B8G8R8_UNORM_PACK32, 255, 255, 255)
			FORMAT_DEPTH(A8B8G8R8_SNORM_PACK32, 255, 255, 255)
			FORMAT_DEPTH(A8B8G8R8_SRGB_PACK32, 255, 255, 255)
			FORMAT_DEPTH(R16G16B16A16_UNORM, 65535, 65535, 65535)
			FORMAT_DEPTH(R16G16B16A16_SNORM, 65535, 65535, 65535)
			FORMAT_DEPTH(B5G6R5_UNORM_PACK16, 31, 63, 31)
			FORMAT_DEPTH(B8G8R8A8_SNORM, 255, 255, 255)
			FORMAT_DEPTH(R4G4B4A4_UNORM_PACK16, 15, 15, 15)
			FORMAT_DEPTH(B4G4R4A4_UNORM_PACK16, 15, 15, 15)
			FORMAT_DEPTH(A1R5G5B5_UNORM_PACK16, 31, 31, 31)
			FORMAT_DEPTH(R5G5B5A1_UNORM_PACK16, 31, 31, 31)
			FORMAT_DEPTH(B5G5R5A1_UNORM_PACK16, 31, 31, 31)
	default:
		*r = 255; *g = 255; *b = 255; return qfalse;
	}
}


/*
 * vk_create_post_process_pipelines - Create post-processing pipelines for multiview
 *
 * Creates gamma, bloom extract, blur, and blend pipelines using multiview render passes.
 * Called when r_fbo is active.
 */
void vk_create_post_process_pipelines( void )
{
	VkPipelineShaderStageCreateInfo shader_stages[2];
	VkPipelineVertexInputStateCreateInfo vertex_input_state;
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
	VkPipelineRasterizationStateCreateInfo rasterization_state;
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state;
	VkPipelineViewportStateCreateInfo viewport_state;
	VkPipelineMultisampleStateCreateInfo multisample_state;
	VkPipelineColorBlendStateCreateInfo blend_state;
	VkPipelineColorBlendAttachmentState attachment_blend_state;
	VkPipelineColorBlendAttachmentState blend_attachments[2];
	VkGraphicsPipelineCreateInfo create_info;
	VkViewport viewport;
	VkRect2D scissor;
	VkSpecializationMapEntry spec_entries[11];
	VkSpecializationInfo frag_spec_info;
	uint32_t i, width, height;

	struct FragSpecData {
		float gamma;
		float overbright;
		float greyscale;
		float bloom_threshold;
		float bloom_intensity;
		int bloom_threshold_mode;
		int bloom_modulate;
		int dither;
		int depth_r;
		int depth_g;
		int depth_b;
	} frag_spec_data;

	// Note: This is called during vk_init_xr_resources() before vk.xr.initialized is set
	// So we check the prerequisites directly instead of vk.xr.initialized
	if ( vk.xr.width == 0 || vk.xr.height == 0 ) {
		ri.Printf( PRINT_WARNING, "vk_create_post_process_pipelines: XR dimensions not set\n" );
		return;
	}

	if ( vk.render_pass.gamma == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_WARNING, "vk_create_post_process_pipelines: gamma render pass not created\n" );
		return;
	}

	width = vk.xr.width;
	height = vk.xr.height;

	// Common vertex input state (no vertex input for fullscreen quad)
	vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_state.pNext = NULL;
	vertex_input_state.flags = 0;
	vertex_input_state.vertexBindingDescriptionCount = 0;
	vertex_input_state.pVertexBindingDescriptions = NULL;
	vertex_input_state.vertexAttributeDescriptionCount = 0;
	vertex_input_state.pVertexAttributeDescriptions = NULL;

	// Common input assembly state
	input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_state.pNext = NULL;
	input_assembly_state.flags = 0;
	input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	input_assembly_state.primitiveRestartEnable = VK_FALSE;

	// Common rasterization state
	rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization_state.pNext = NULL;
	rasterization_state.flags = 0;
	rasterization_state.depthClampEnable = VK_FALSE;
	rasterization_state.rasterizerDiscardEnable = VK_FALSE;
	rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
	rasterization_state.cullMode = VK_CULL_MODE_NONE;
	rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterization_state.depthBiasEnable = VK_FALSE;
	rasterization_state.depthBiasConstantFactor = 0.0f;
	rasterization_state.depthBiasClamp = 0.0f;
	rasterization_state.depthBiasSlopeFactor = 0.0f;
	rasterization_state.lineWidth = 1.0f;

	// Common multisample state
	multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state.pNext = NULL;
	multisample_state.flags = 0;
	multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisample_state.sampleShadingEnable = VK_FALSE;
	multisample_state.minSampleShading = 1.0f;
	multisample_state.pSampleMask = NULL;
	multisample_state.alphaToCoverageEnable = VK_FALSE;
	multisample_state.alphaToOneEnable = VK_FALSE;

	// Common depth/stencil state (disabled)
	Com_Memset( &depth_stencil_state, 0, sizeof( depth_stencil_state ) );
	depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil_state.depthTestEnable = VK_FALSE;
	depth_stencil_state.depthWriteEnable = VK_FALSE;
	depth_stencil_state.depthCompareOp = VK_COMPARE_OP_NEVER;

	// Setup specialization constants for gamma/bloom
	frag_spec_data.gamma = 1.0 / (r_gamma->value);
	frag_spec_data.overbright = (float)(1 << tr.overbrightBits);
	frag_spec_data.greyscale = r_greyscale->value;
	frag_spec_data.bloom_threshold = r_bloom_threshold->value;
	// Compensate for multiview doubling effect in bloom blend pass
	frag_spec_data.bloom_intensity = r_bloom_intensity->value * 0.5f;
	frag_spec_data.bloom_threshold_mode = r_bloom_threshold_mode->integer;
	frag_spec_data.bloom_modulate = r_bloom_modulate->integer;
	frag_spec_data.dither = r_dither->integer;

	// Get color depth from XR swapchain format
	if ( vk.xr.colorInfo && !vk_surface_format_color_depth( vk.xr.colorInfo->format,
			&frag_spec_data.depth_r, &frag_spec_data.depth_g, &frag_spec_data.depth_b ) ) {
		frag_spec_data.depth_r = frag_spec_data.depth_g = frag_spec_data.depth_b = 255;
	}

	spec_entries[0].constantID = 0;
	spec_entries[0].offset = offsetof( struct FragSpecData, gamma );
	spec_entries[0].size = sizeof( frag_spec_data.gamma );

	spec_entries[1].constantID = 1;
	spec_entries[1].offset = offsetof( struct FragSpecData, overbright );
	spec_entries[1].size = sizeof( frag_spec_data.overbright );

	spec_entries[2].constantID = 2;
	spec_entries[2].offset = offsetof( struct FragSpecData, greyscale );
	spec_entries[2].size = sizeof( frag_spec_data.greyscale );

	spec_entries[3].constantID = 3;
	spec_entries[3].offset = offsetof( struct FragSpecData, bloom_threshold );
	spec_entries[3].size = sizeof( frag_spec_data.bloom_threshold );

	spec_entries[4].constantID = 4;
	spec_entries[4].offset = offsetof( struct FragSpecData, bloom_intensity );
	spec_entries[4].size = sizeof( frag_spec_data.bloom_intensity );

	spec_entries[5].constantID = 5;
	spec_entries[5].offset = offsetof( struct FragSpecData, bloom_threshold_mode );
	spec_entries[5].size = sizeof( frag_spec_data.bloom_threshold_mode );

	spec_entries[6].constantID = 6;
	spec_entries[6].offset = offsetof( struct FragSpecData, bloom_modulate );
	spec_entries[6].size = sizeof( frag_spec_data.bloom_modulate );

	spec_entries[7].constantID = 7;
	spec_entries[7].offset = offsetof( struct FragSpecData, dither );
	spec_entries[7].size = sizeof( frag_spec_data.dither );

	spec_entries[8].constantID = 8;
	spec_entries[8].offset = offsetof( struct FragSpecData, depth_r );
	spec_entries[8].size = sizeof( frag_spec_data.depth_r );

	spec_entries[9].constantID = 9;
	spec_entries[9].offset = offsetof( struct FragSpecData, depth_g );
	spec_entries[9].size = sizeof( frag_spec_data.depth_g );

	spec_entries[10].constantID = 10;
	spec_entries[10].offset = offsetof( struct FragSpecData, depth_b );
	spec_entries[10].size = sizeof( frag_spec_data.depth_b );

	frag_spec_info.mapEntryCount = 11;
	frag_spec_info.pMapEntries = spec_entries;
	frag_spec_info.dataSize = sizeof( frag_spec_data );
	frag_spec_info.pData = &frag_spec_data;

	// Common pipeline create info template
	Com_Memset( &create_info, 0, sizeof( create_info ) );
	create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	create_info.stageCount = 2;
	create_info.pStages = shader_stages;
	create_info.pVertexInputState = &vertex_input_state;
	create_info.pInputAssemblyState = &input_assembly_state;
	create_info.pRasterizationState = &rasterization_state;
	create_info.pMultisampleState = &multisample_state;
	create_info.pDepthStencilState = &depth_stencil_state;

	// =====================================================================
	// 1. XR Gamma Pipeline: outputs from FBO to XR swapchain
	// =====================================================================
	if ( vk.gamma_pipeline != VK_NULL_HANDLE ) {
		vk_wait_idle();
		qvkDestroyPipeline( vk.device, vk.gamma_pipeline, NULL );
		vk.gamma_pipeline = VK_NULL_HANDLE;
	}

	set_shader_stage_desc( shader_stages+0, VK_SHADER_STAGE_VERTEX_BIT, vk.modules.gamma_vs, "main" );
	set_shader_stage_desc( shader_stages+1, VK_SHADER_STAGE_FRAGMENT_BIT, vk.modules.gamma_fs, "main" );
	shader_stages[1].pSpecializationInfo = &frag_spec_info;

	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)width;
	viewport.height = (float)height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = width;
	scissor.extent.height = height;

	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.pNext = NULL;
	viewport_state.flags = 0;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &scissor;

	Com_Memset( &attachment_blend_state, 0, sizeof( attachment_blend_state ) );
	attachment_blend_state.blendEnable = VK_FALSE;
	attachment_blend_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
											VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.pNext = NULL;
	blend_state.flags = 0;
	blend_state.logicOpEnable = VK_FALSE;
	blend_state.logicOp = VK_LOGIC_OP_COPY;
	blend_state.attachmentCount = 1;
	blend_state.pAttachments = &attachment_blend_state;
	Com_Memset( blend_state.blendConstants, 0, sizeof( blend_state.blendConstants ) );

	create_info.pViewportState = &viewport_state;
	create_info.pColorBlendState = &blend_state;
	create_info.layout = vk.pipeline_layout_post_process;
	create_info.renderPass = vk.render_pass.gamma;

	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &create_info, NULL, &vk.gamma_pipeline ) );
	SET_OBJECT_NAME( vk.gamma_pipeline, "XR gamma pipeline", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );

	// =====================================================================
	// 2. XR Bloom Extract Pipeline
	// =====================================================================
	if ( vk.render_pass.bloom_extract != VK_NULL_HANDLE && r_bloom->integer ) {
		if ( vk.bloom_extract_pipeline != VK_NULL_HANDLE ) {
			vk_wait_idle();
			qvkDestroyPipeline( vk.device, vk.bloom_extract_pipeline, NULL );
			vk.bloom_extract_pipeline = VK_NULL_HANDLE;
		}

		set_shader_stage_desc( shader_stages+1, VK_SHADER_STAGE_FRAGMENT_BIT, vk.modules.bloom_fs, "main" );
		shader_stages[1].pSpecializationInfo = &frag_spec_info;

		create_info.renderPass = vk.render_pass.bloom_extract;

		VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &create_info, NULL, &vk.bloom_extract_pipeline ) );
		SET_OBJECT_NAME( vk.bloom_extract_pipeline, "XR bloom extract pipeline", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );

		// =================================================================
		// 3. XR Blur Pipelines
		// =================================================================
		for ( i = 0; i < VK_NUM_BLOOM_PASSES * 2; i++ ) {
			float blur_spec_data[3];
			VkSpecializationMapEntry blur_spec_entries[3];
			VkSpecializationInfo blur_spec_info;
			uint32_t blur_width, blur_height;
			qboolean horizontal = (i % 2) == 0;

			if ( vk.blur_pipeline[i] != VK_NULL_HANDLE ) {
				vk_wait_idle();
				qvkDestroyPipeline( vk.device, vk.blur_pipeline[i], NULL );
				vk.blur_pipeline[i] = VK_NULL_HANDLE;
			}

			// Calculate blur pass dimensions
			blur_width = width / ( 2 << ( i / 2 ) );
			blur_height = height / ( 2 << ( i / 2 ) );

			blur_spec_data[0] = 1.2f / (float)blur_width;   // x offset
			blur_spec_data[1] = 1.2f / (float)blur_height;  // y offset
			blur_spec_data[2] = 1.0f;                        // intensity

			if ( horizontal ) {
				blur_spec_data[1] = 0.0f;
			} else {
				blur_spec_data[0] = 0.0f;
			}

			blur_spec_entries[0].constantID = 0;
			blur_spec_entries[0].offset = 0;
			blur_spec_entries[0].size = sizeof( float );

			blur_spec_entries[1].constantID = 1;
			blur_spec_entries[1].offset = sizeof( float );
			blur_spec_entries[1].size = sizeof( float );

			blur_spec_entries[2].constantID = 2;
			blur_spec_entries[2].offset = 2 * sizeof( float );
			blur_spec_entries[2].size = sizeof( float );

			blur_spec_info.mapEntryCount = 3;
			blur_spec_info.pMapEntries = blur_spec_entries;
			blur_spec_info.dataSize = sizeof( blur_spec_data );
			blur_spec_info.pData = blur_spec_data;

			set_shader_stage_desc( shader_stages+1, VK_SHADER_STAGE_FRAGMENT_BIT, vk.modules.blur_fs, "main" );
			shader_stages[1].pSpecializationInfo = &blur_spec_info;

			viewport.width = (float)blur_width;
			viewport.height = (float)blur_height;
			scissor.extent.width = blur_width;
			scissor.extent.height = blur_height;

			create_info.renderPass = vk.render_pass.blur[i];

			VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &create_info, NULL, &vk.blur_pipeline[i] ) );
			SET_OBJECT_NAME( vk.blur_pipeline[i], va( "XR %s blur pipeline %i", horizontal ? "horizontal" : "vertical", i/2 + 1 ), VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
		}

		// =================================================================
		// 4. XR Bloom Blend Pipeline
		// =================================================================
		if ( vk.bloom_blend_pipeline != VK_NULL_HANDLE ) {
			vk_wait_idle();
			qvkDestroyPipeline( vk.device, vk.bloom_blend_pipeline, NULL );
			vk.bloom_blend_pipeline = VK_NULL_HANDLE;
		}

		set_shader_stage_desc( shader_stages+1, VK_SHADER_STAGE_FRAGMENT_BIT, vk.modules.blend_fs, "main" );
		shader_stages[1].pSpecializationInfo = &frag_spec_info;

		viewport.width = (float)width;
		viewport.height = (float)height;
		scissor.extent.width = width;
		scissor.extent.height = height;

		// Bloom blend uses additive blending
		attachment_blend_state.blendEnable = VK_TRUE;
		attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;

		// post_bloom carries the emissive attachment when HDR is active; blend.frag must not write it
		blend_attachments[0] = attachment_blend_state;
		if ( vk.hdrActive ) {
			Com_Memset( &blend_attachments[1], 0, sizeof( blend_attachments[1] ) );
			blend_attachments[1].colorWriteMask = 0;
			blend_state.attachmentCount = 2;
		} else {
			blend_state.attachmentCount = 1;
		}
		blend_state.pAttachments = blend_attachments;

		create_info.layout = vk.pipeline_layout_blend;
		create_info.renderPass = vk.render_pass.post_bloom;

		// post_bloom uses MSAA when active, so bloom blend pipeline must match
		if ( vk.msaaActive ) {
			multisample_state.rasterizationSamples = vkSamples;
		}

		VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &create_info, NULL, &vk.bloom_blend_pipeline ) );
		SET_OBJECT_NAME( vk.bloom_blend_pipeline, "XR bloom blend pipeline", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
	}
}


/*
 * vk_destroy_post_process_pipelines - Clean up post-processing pipelines
 */
static void vk_destroy_post_process_pipelines( void )
{
	uint32_t i;

	if ( vk.gamma_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.gamma_pipeline, NULL );
		vk.gamma_pipeline = VK_NULL_HANDLE;
	}

	if ( vk.bloom_extract_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.bloom_extract_pipeline, NULL );
		vk.bloom_extract_pipeline = VK_NULL_HANDLE;
	}

	for ( i = 0; i < VK_NUM_BLOOM_PASSES * 2; i++ ) {
		if ( vk.blur_pipeline[i] != VK_NULL_HANDLE ) {
			qvkDestroyPipeline( vk.device, vk.blur_pipeline[i], NULL );
			vk.blur_pipeline[i] = VK_NULL_HANDLE;
		}
	}

	if ( vk.bloom_blend_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.bloom_blend_pipeline, NULL );
		vk.bloom_blend_pipeline = VK_NULL_HANDLE;
	}
}


static VkVertexInputBindingDescription bindings[8];
static VkVertexInputAttributeDescription attribs[8];
static uint32_t num_binds;
static uint32_t num_attrs;

static void push_bind( uint32_t binding, uint32_t stride )
{
	bindings[ num_binds ].binding = binding;
	bindings[ num_binds ].stride = stride;
	bindings[ num_binds ].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	num_binds++;
}

static void push_attr( uint32_t location, uint32_t binding, VkFormat format )
{
	attribs[ num_attrs ].location = location;
	attribs[ num_attrs ].binding = binding;
	attribs[ num_attrs ].format = format;
	attribs[ num_attrs ].offset = 0;
	num_attrs++;
}


VkPipeline create_pipeline( const Vk_Pipeline_Def *def, renderPass_t renderPassIndex, uint32_t def_index ) {
	VkShaderModule *vs_module = NULL;
	VkShaderModule *fs_module = NULL;
	//int32_t vert_spec_data[1]; // clippping
	floatint_t frag_spec_data[12]; // 0:alpha-test-func, 1:alpha-test-value, 2:depth-fragment, 3:alpha-to-coverage, 4:color_mode, 5:abs_light, 6:multitexture mode, 7:discard mode, 8: ident.color, 9 - ident.alpha, 10 - acff, 11 - force_opaque_alpha (HUD 3D)
	VkSpecializationMapEntry spec_entries[13];
	//VkSpecializationInfo vert_spec_info;
	VkSpecializationInfo frag_spec_info;
	VkPipelineVertexInputStateCreateInfo vertex_input_state;
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
	VkPipelineRasterizationStateCreateInfo rasterization_state;
	VkPipelineViewportStateCreateInfo viewport_state;
	VkPipelineMultisampleStateCreateInfo multisample_state;
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state;
	VkPipelineColorBlendStateCreateInfo blend_state;
	VkPipelineColorBlendAttachmentState attachment_blend_state;
	VkPipelineColorBlendAttachmentState blend_attachments[2];
	VkPipelineDynamicStateCreateInfo dynamic_state;
	VkDynamicState dynamic_state_array[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkGraphicsPipelineCreateInfo create_info;
	VkPipeline pipeline;
	VkPipelineShaderStageCreateInfo shader_stages[2];
	VkBool32 alphaToCoverage = VK_FALSE;
	unsigned int atest_bits;
	unsigned int state_bits = def->state_bits;

	// Q3VR: All shaders use multiview for VR stereo rendering.
	// gl_ViewIndex is 0 for single-layer targets (HUD buffer).

	// the emissive attachment only exists in these passes; fragment-module
	// choice below must agree with the blend-state attachmentCount decision
	// or the pipeline gets an unconsumed location-1 output
	const qboolean emissiveActive = ( renderPassIndex == RENDER_PASS_MAIN || renderPassIndex == RENDER_PASS_POST_BLOOM ) && vk.hdrActive;
	const int fs_em = emissiveActive ? 0 : 1;

	switch ( def->shader_type ) {

		case TYPE_SINGLE_TEXTURE_LIGHTING:
			vs_module = &vk.modules.vert.light[0];
			fs_module = &vk.modules.frag.light[0][fs_em][0];
			break;

		case TYPE_SINGLE_TEXTURE_LIGHTING_LINEAR:
			vs_module = &vk.modules.vert.light[0];
			fs_module = &vk.modules.frag.light[1][fs_em][0];
			break;

		case TYPE_SINGLE_TEXTURE_LIGHTING_OVERBRIGHT:
			vs_module = &vk.modules.vert.overbright_vert[0];
			fs_module = &vk.modules.frag.overbright_frag[fs_em][0];
			break;

		case TYPE_SINGLE_TEXTURE_DF:
			state_bits |= GLS_DEPTHMASK_TRUE;
			vs_module = &vk.modules.vert.ident1[0][0][0];
			fs_module = &vk.modules.frag.gen0_df;
			break;

		case TYPE_SINGLE_TEXTURE_FIXED_COLOR:
			vs_module = &vk.modules.vert.fixed[0][0][0];
			fs_module = &vk.modules.frag.fixed[0][fs_em][0];
			break;

		case TYPE_SINGLE_TEXTURE_FIXED_COLOR_ENV:
			vs_module = &vk.modules.vert.fixed[0][1][0];
			fs_module = &vk.modules.frag.fixed[0][fs_em][0];
			break;

		case TYPE_SINGLE_TEXTURE_ENT_COLOR:
			vs_module = &vk.modules.vert.fixed[0][0][0];
			fs_module = &vk.modules.frag.ent[0][fs_em][0];
			break;

		case TYPE_SINGLE_TEXTURE_ENT_COLOR_ENV:
			vs_module = &vk.modules.vert.fixed[0][1][0];
			fs_module = &vk.modules.frag.ent[0][fs_em][0];
			break;

		case TYPE_SINGLE_TEXTURE:
			vs_module = &vk.modules.vert.gen[0][0][0][0];
			fs_module = &vk.modules.frag.gen[0][0][fs_em][0];
			break;

		case TYPE_SINGLE_TEXTURE_ENV:
			vs_module = &vk.modules.vert.gen[0][0][1][0];
			fs_module = &vk.modules.frag.gen[0][0][fs_em][0];
			break;

		case TYPE_SINGLE_TEXTURE_IDENTITY:
			vs_module = &vk.modules.vert.ident1[0][0][0];
			fs_module = &vk.modules.frag.ident1[0][fs_em][0];
			break;

		case TYPE_SINGLE_TEXTURE_IDENTITY_ENV:
			vs_module = &vk.modules.vert.ident1[0][1][0];
			fs_module = &vk.modules.frag.ident1[0][fs_em][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY:
		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY:
			vs_module = &vk.modules.vert.ident1[1][0][0];
			fs_module = &vk.modules.frag.ident1[1][fs_em][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV:
			vs_module = &vk.modules.vert.ident1[1][1][0];
			fs_module = &vk.modules.frag.ident1[1][fs_em][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR:
			vs_module = &vk.modules.vert.fixed[1][0][0];
			fs_module = &vk.modules.frag.fixed[1][fs_em][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV:
			vs_module = &vk.modules.vert.fixed[1][1][0];
			fs_module = &vk.modules.frag.fixed[1][fs_em][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL2:
		case TYPE_MULTI_TEXTURE_ADD2_1_1:
		case TYPE_MULTI_TEXTURE_ADD2:
			vs_module = &vk.modules.vert.gen[1][0][0][0];
			fs_module = &vk.modules.frag.gen[1][0][fs_em][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL2_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_ENV:
			vs_module = &vk.modules.vert.gen[1][0][1][0];
			fs_module = &vk.modules.frag.gen[1][0][fs_em][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL3:
		case TYPE_MULTI_TEXTURE_ADD3_1_1:
		case TYPE_MULTI_TEXTURE_ADD3:
			vs_module = &vk.modules.vert.gen[2][0][0][0];
			fs_module = &vk.modules.frag.gen[2][0][fs_em][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL3_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_ENV:
			vs_module = &vk.modules.vert.gen[2][0][1][0];
			fs_module = &vk.modules.frag.gen[2][0][fs_em][0];
			break;

		case TYPE_BLEND2_ADD:
		case TYPE_BLEND2_MUL:
		case TYPE_BLEND2_ALPHA:
		case TYPE_BLEND2_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_MIX_ALPHA:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA:
			vs_module = &vk.modules.vert.gen[1][1][0][0];
			fs_module = &vk.modules.frag.gen[1][1][fs_em][0];
			break;

		case TYPE_BLEND2_ADD_ENV:
		case TYPE_BLEND2_MUL_ENV:
		case TYPE_BLEND2_ALPHA_ENV:
		case TYPE_BLEND2_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND2_MIX_ALPHA_ENV:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV:
			vs_module = &vk.modules.vert.gen[1][1][1][0];
			fs_module = &vk.modules.frag.gen[1][1][fs_em][0];
			break;

		case TYPE_BLEND3_ADD:
		case TYPE_BLEND3_MUL:
		case TYPE_BLEND3_ALPHA:
		case TYPE_BLEND3_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_MIX_ALPHA:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA:
			vs_module = &vk.modules.vert.gen[2][1][0][0];
			fs_module = &vk.modules.frag.gen[2][1][fs_em][0];
			break;

		case TYPE_BLEND3_ADD_ENV:
		case TYPE_BLEND3_MUL_ENV:
		case TYPE_BLEND3_ALPHA_ENV:
		case TYPE_BLEND3_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV:
			vs_module = &vk.modules.vert.gen[2][1][1][0];
			fs_module = &vk.modules.frag.gen[2][1][fs_em][0];
			break;

		case TYPE_COLOR_BLACK:
		case TYPE_COLOR_WHITE:
		case TYPE_COLOR_GREEN:
		case TYPE_COLOR_RED:
			vs_module = &vk.modules.color_vs;
			fs_module = &vk.modules.color_fs;
			break;

		case TYPE_FOG_ONLY:
			vs_module = &vk.modules.fog_vs;
			fs_module = &vk.modules.fog_fs;
			break;

		case TYPE_DOT:
			vs_module = &vk.modules.dot_vs;
			fs_module = &vk.modules.dot_fs;
			break;

		default:
			ri.Error(ERR_DROP, "create_pipeline: unknown shader type %i\n", def->shader_type);
			return 0;
	}

	if ( def->fog_stage ) {
		switch ( def->shader_type ) {
			case TYPE_FOG_ONLY:
			case TYPE_DOT:
			case TYPE_SINGLE_TEXTURE_DF:
			case TYPE_COLOR_BLACK:
			case TYPE_COLOR_WHITE:
			case TYPE_COLOR_GREEN:
			case TYPE_COLOR_RED:
				break;
			default:
				// switch to fogged modules
				vs_module++;
				fs_module++;
				break;
		}
	}

	set_shader_stage_desc(shader_stages+0, VK_SHADER_STAGE_VERTEX_BIT, *vs_module, "main");
	set_shader_stage_desc(shader_stages+1, VK_SHADER_STAGE_FRAGMENT_BIT, *fs_module, "main");

	//Com_Memset( vert_spec_data, 0, sizeof( vert_spec_data ) );
	Com_Memset( frag_spec_data, 0, sizeof( frag_spec_data ) );

	//vert_spec_data[0] = def->clipping_plane ? 1 : 0;

	// fragment shader specialization data
	atest_bits = state_bits & GLS_ATEST_BITS;
	switch ( atest_bits ) {
		case GLS_ATEST_GT_0:
			frag_spec_data[0].i = 1; // not equal
			frag_spec_data[1].f = 0.0f;
			break;
		case GLS_ATEST_LT_80:
			frag_spec_data[0].i = 2; // less than
			frag_spec_data[1].f = 0.5f;
			break;
		case GLS_ATEST_GE_80:
			frag_spec_data[0].i = 3; // greater or equal
			frag_spec_data[1].f = 0.5f;
			break;
		default:
			frag_spec_data[0].i = 0;
			frag_spec_data[1].f = 0.0f;
			break;
	};

	// depth fragment threshold
	frag_spec_data[2].f = 0.85f;

	// constant color
	switch ( def->shader_type ) {
		default: frag_spec_data[4].i = 0; break;
		case TYPE_COLOR_WHITE: frag_spec_data[4].i = 1; break;
		case TYPE_COLOR_GREEN: frag_spec_data[4].i = 2; break;
		case TYPE_COLOR_RED:   frag_spec_data[4].i = 3; break;
	}

	// abs lighting
	switch ( def->shader_type ) {
		case TYPE_SINGLE_TEXTURE_LIGHTING:
		case TYPE_SINGLE_TEXTURE_LIGHTING_LINEAR:
			frag_spec_data[5].i = def->abs_light ? 1 : 0;
		default:
			break;
	}

	// multutexture mode
	switch ( def->shader_type ) {
		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY:
		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV:
		case TYPE_MULTI_TEXTURE_MUL2:
		case TYPE_MULTI_TEXTURE_MUL2_ENV:
		case TYPE_MULTI_TEXTURE_MUL3:
		case TYPE_MULTI_TEXTURE_MUL3_ENV:
		case TYPE_BLEND2_MUL:
		case TYPE_BLEND2_MUL_ENV:
		case TYPE_BLEND3_MUL:
		case TYPE_BLEND3_MUL_ENV:
			frag_spec_data[6].i = 0;
			break;

		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY:
		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR:
		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_1_1:
		case TYPE_MULTI_TEXTURE_ADD2_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_1_1:
		case TYPE_MULTI_TEXTURE_ADD3_1_1_ENV:
			frag_spec_data[6].i = 1;
			break;

		case TYPE_MULTI_TEXTURE_ADD2:
		case TYPE_MULTI_TEXTURE_ADD2_ENV:
		case TYPE_MULTI_TEXTURE_ADD3:
		case TYPE_MULTI_TEXTURE_ADD3_ENV:
		case TYPE_BLEND2_ADD:
		case TYPE_BLEND2_ADD_ENV:
		case TYPE_BLEND3_ADD:
		case TYPE_BLEND3_ADD_ENV:
			frag_spec_data[6].i = 2;
			break;

		case TYPE_BLEND2_ALPHA:
		case TYPE_BLEND2_ALPHA_ENV:
		case TYPE_BLEND3_ALPHA:
		case TYPE_BLEND3_ALPHA_ENV:
			frag_spec_data[6].i = 3;
			break;

		case TYPE_BLEND2_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_ONE_MINUS_ALPHA_ENV:
			frag_spec_data[6].i = 4;
			break;

		case TYPE_BLEND2_MIX_ALPHA:
		case TYPE_BLEND2_MIX_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ALPHA:
		case TYPE_BLEND3_MIX_ALPHA_ENV:
			frag_spec_data[6].i = 5;
			break;

		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV:
			frag_spec_data[6].i = 6;
			break;

		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV:
			frag_spec_data[6].i = 7;
			break;

		default:
			break;
	}

	frag_spec_data[8].f = ((float)def->color.rgb) / 255.0;
	frag_spec_data[9].f = ((float)def->color.alpha) / 255.0;

	if ( def->fog_stage ) {
		frag_spec_data[10].i = def->acff;
	} else {
		frag_spec_data[10].i = 0;
	}

	//
	// vertex module specialization data
	//
	shader_stages[0].pSpecializationInfo = NULL;

	//
	// fragment module specialization data
	//

	spec_entries[1].constantID = 0;  // alpha-test-function
	spec_entries[1].offset = 0 * sizeof( int32_t );
	spec_entries[1].size = sizeof( int32_t );

	spec_entries[2].constantID = 1; // alpha-test-value
	spec_entries[2].offset = 1 * sizeof( int32_t );
	spec_entries[2].size = sizeof( float );

	spec_entries[3].constantID = 2; // depth-fragment
	spec_entries[3].offset = 2 * sizeof( int32_t );
	spec_entries[3].size = sizeof( float );

	spec_entries[4].constantID = 3; // alpha-to-coverage
	spec_entries[4].offset = 3 * sizeof( int32_t );
	spec_entries[4].size = sizeof( int32_t );

	spec_entries[5].constantID = 4; // color_mode
	spec_entries[5].offset = 4 * sizeof( int32_t );
	spec_entries[5].size = sizeof( int32_t );

	spec_entries[6].constantID = 5; // abs_light
	spec_entries[6].offset = 5 * sizeof( int32_t );
	spec_entries[6].size = sizeof( int32_t );

	spec_entries[7].constantID = 6; // multitexture mode
	spec_entries[7].offset = 6 * sizeof( int32_t );
	spec_entries[7].size = sizeof( int32_t );

	spec_entries[8].constantID = 7; // discard mode
	spec_entries[8].offset = 7 * sizeof( int32_t );
	spec_entries[8].size = sizeof( int32_t );

	spec_entries[9].constantID = 8; // fixed color
	spec_entries[9].offset = 8 * sizeof( int32_t );
	spec_entries[9].size = sizeof( float );

	spec_entries[10].constantID = 9; // fixed alpha
	spec_entries[10].offset = 9 * sizeof( int32_t );
	spec_entries[10].size = sizeof( float );

	spec_entries[11].constantID = 10; // acff
	spec_entries[11].offset = 10 * sizeof( int32_t );
	spec_entries[11].size = sizeof( int32_t );

	spec_entries[12].constantID = 11; // force_opaque_alpha (HUD 3D)
	spec_entries[12].offset = 11 * sizeof( int32_t );
	spec_entries[12].size = sizeof( int32_t );

	// For non-blended HUD render pass stages, force alpha=1.0
	// This ensures 3D models (player heads, weapon icons) are fully opaque
	if (renderPassIndex == RENDER_PASS_HUD && !(state_bits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS))) {
		frag_spec_data[11].i = 1;
	} else {
		frag_spec_data[11].i = 0;
	}

	frag_spec_info.mapEntryCount = 12;
	frag_spec_info.pMapEntries = spec_entries + 1;
	frag_spec_info.dataSize = sizeof( int32_t ) * 12;
	frag_spec_info.pData = &frag_spec_data[0];
	shader_stages[1].pSpecializationInfo = &frag_spec_info;

	//
	// Vertex input
	//
	num_binds = num_attrs = 0;
	switch ( def->shader_type ) {

		case TYPE_FOG_ONLY:
		case TYPE_DOT:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_COLOR_BLACK:
		case TYPE_COLOR_WHITE:
		case TYPE_COLOR_GREEN:
		case TYPE_COLOR_RED:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_SINGLE_TEXTURE_DF:
		case TYPE_SINGLE_TEXTURE_IDENTITY:
		case TYPE_SINGLE_TEXTURE_FIXED_COLOR:
		case TYPE_SINGLE_TEXTURE_ENT_COLOR:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			break;

		case TYPE_SINGLE_TEXTURE:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			break;

		case TYPE_SINGLE_TEXTURE_LIGHTING_OVERBRIGHT:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 5, sizeof( float ) );					// overbright array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 8, 5, VK_FORMAT_R32_SFLOAT );			// GLSL location 8, binding 5
			break;

		case TYPE_SINGLE_TEXTURE_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			//push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			//push_attr( 2, 2, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_SINGLE_TEXTURE_IDENTITY_ENV:
		case TYPE_SINGLE_TEXTURE_FIXED_COLOR_ENV:
		case TYPE_SINGLE_TEXTURE_ENT_COLOR_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_SINGLE_TEXTURE_LIGHTING:
		case TYPE_SINGLE_TEXTURE_LIGHTING_LINEAR:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( vec2_t ) );					// st0 array
			push_bind( 2, sizeof( vec4_t ) );					// normals array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 2, 2, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY:
		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR:
		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL2:
		case TYPE_MULTI_TEXTURE_ADD2_1_1:
		case TYPE_MULTI_TEXTURE_ADD2:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL2_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			//push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			//push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL3:
		case TYPE_MULTI_TEXTURE_ADD3_1_1:
		case TYPE_MULTI_TEXTURE_ADD3:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 4, sizeof( vec2_t ) );					// st2 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 4, 4, VK_FORMAT_R32G32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL3_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			//push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 4, sizeof( vec2_t ) );					// st2 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			//push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 4, 4, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_BLEND2_ADD:
		case TYPE_BLEND2_MUL:
		case TYPE_BLEND2_ALPHA:
		case TYPE_BLEND2_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_MIX_ALPHA:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color0 array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 6, sizeof( color4ub_t ) );				// color1 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 6, 6, VK_FORMAT_R8G8B8A8_UNORM );
			break;

		case TYPE_BLEND2_ADD_ENV:
		case TYPE_BLEND2_MUL_ENV:
		case TYPE_BLEND2_ALPHA_ENV:
		case TYPE_BLEND2_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND2_MIX_ALPHA_ENV:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color0 array
			//push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_bind( 6, sizeof( color4ub_t ) );				// color1 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			//push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 6, 6, VK_FORMAT_R8G8B8A8_UNORM );
			break;

		case TYPE_BLEND3_ADD:
		case TYPE_BLEND3_MUL:
		case TYPE_BLEND3_ALPHA:
		case TYPE_BLEND3_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_MIX_ALPHA:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color0 array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 4, sizeof( vec2_t ) );					// st2 array
			push_bind( 6, sizeof( color4ub_t ) );				// color1 array
			push_bind( 7, sizeof( color4ub_t ) );				// color2 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 4, 4, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 6, 6, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 7, 7, VK_FORMAT_R8G8B8A8_UNORM );
			break;

		case TYPE_BLEND3_ADD_ENV:
		case TYPE_BLEND3_MUL_ENV:
		case TYPE_BLEND3_ALPHA_ENV:
		case TYPE_BLEND3_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color0 array
			//push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 4, sizeof( vec2_t ) );					// st2 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_bind( 6, sizeof( color4ub_t ) );				// color1 array
			push_bind( 7, sizeof( color4ub_t ) );				// color2 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			//push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 4, 4, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 6, 6, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 7, 7, VK_FORMAT_R8G8B8A8_UNORM );
			break;

		default:
			ri.Error( ERR_DROP, "%s: invalid shader type - %i", __func__, def->shader_type );
			break;
	}

	vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_state.pNext = NULL;
	vertex_input_state.flags = 0;
	vertex_input_state.pVertexBindingDescriptions = bindings;
	vertex_input_state.pVertexAttributeDescriptions = attribs;
	vertex_input_state.vertexBindingDescriptionCount = num_binds;
	vertex_input_state.vertexAttributeDescriptionCount = num_attrs;

	//
	// Primitive assembly.
	//
	input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_state.pNext = NULL;
	input_assembly_state.flags = 0;
	input_assembly_state.primitiveRestartEnable = VK_FALSE;

	switch ( def->primitives ) {
		case LINE_LIST: input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST; break;
		case POINT_LIST: input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; break;
		case TRIANGLE_STRIP: input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; break;
		default: input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;
	}

	//
	// Viewport.
	//
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.pNext = NULL;
	viewport_state.flags = 0;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = NULL; // dynamic viewport state
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = NULL; // dynamic scissor state

	//
	// Rasterization.
	//
	rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization_state.pNext = NULL;
	rasterization_state.flags = 0;
	rasterization_state.depthClampEnable = ((def->shadow_phase == SHADOW_FS_QUAD || def->shadow_phase == SHADOW_EDGES) && vk.depthClamp) ? VK_TRUE : VK_FALSE;
	rasterization_state.rasterizerDiscardEnable = VK_FALSE;
	rasterization_state.polygonMode = (state_bits & GLS_POLYMODE_LINE) ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;

	switch ( def->face_culling ) {
		case CT_TWO_SIDED:
			rasterization_state.cullMode = VK_CULL_MODE_NONE;
			break;
		case CT_FRONT_SIDED:
			rasterization_state.cullMode = (def->mirror ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_BACK_BIT);
			break;
		case CT_BACK_SIDED:
			rasterization_state.cullMode = (def->mirror ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_FRONT_BIT);
			break;
		default:
			ri.Error( ERR_DROP, "create_pipeline: invalid face culling mode %i\n", def->face_culling );
			break;
	}

	rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE; // Q3 defaults to clockwise vertex order

	 // depth bias state
	if ( def->polygon_offset ) {
		rasterization_state.depthBiasEnable = VK_TRUE;
		rasterization_state.depthBiasClamp = 0.0f;
#ifdef USE_REVERSED_DEPTH
		rasterization_state.depthBiasConstantFactor = -r_offsetUnits->value;
		rasterization_state.depthBiasSlopeFactor = -r_offsetFactor->value;
#else
		rasterization_state.depthBiasConstantFactor = r_offsetUnits->value;
		rasterization_state.depthBiasSlopeFactor = r_offsetFactor->value;
#endif
	} else {
		rasterization_state.depthBiasEnable = VK_FALSE;
		rasterization_state.depthBiasClamp = 0.0f;
		rasterization_state.depthBiasConstantFactor = 0.0f;
		rasterization_state.depthBiasSlopeFactor = 0.0f;
	}

	if ( def->line_width )
		rasterization_state.lineWidth = (float)def->line_width;
	else
		rasterization_state.lineWidth = 1.0f;

	multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state.pNext = NULL;
	multisample_state.flags = 0;

	// Set sample count based on render pass:
	// - SCREENMAP uses its own sample count
	// - MAIN, POST_BLOOM, and default use vkSamples (main has MSAA when active)
	// - HUD always uses 1 sample (its render pass is non-MSAA)
	if ( renderPassIndex == RENDER_PASS_SCREENMAP ) {
		multisample_state.rasterizationSamples = vk.screenMapSamples;
	} else if ( renderPassIndex == RENDER_PASS_HUD ) {
		// HUD render pass is always 1 sample
		multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	} else {
		// MAIN, POST_BLOOM, and any unexpected value use vkSamples
		// (they all use main or post_bloom which have MSAA when active)
		multisample_state.rasterizationSamples = vkSamples;
	}

	multisample_state.sampleShadingEnable = VK_FALSE;
	multisample_state.minSampleShading = 1.0f;
	multisample_state.pSampleMask = NULL;
	multisample_state.alphaToCoverageEnable = alphaToCoverage;
	multisample_state.alphaToOneEnable = VK_FALSE;

	Com_Memset( &depth_stencil_state, 0, sizeof( depth_stencil_state ) );

	depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil_state.pNext = NULL;
	depth_stencil_state.flags = 0;
	depth_stencil_state.depthTestEnable = (state_bits & GLS_DEPTHTEST_DISABLE) ? VK_FALSE : VK_TRUE;
	depth_stencil_state.depthWriteEnable = (state_bits & GLS_DEPTHMASK_TRUE) ? VK_TRUE : VK_FALSE;
#ifdef USE_REVERSED_DEPTH
	depth_stencil_state.depthCompareOp = (state_bits & GLS_DEPTHFUNC_EQUAL) ? VK_COMPARE_OP_EQUAL : VK_COMPARE_OP_GREATER_OR_EQUAL;
#else
	depth_stencil_state.depthCompareOp = (state_bits & GLS_DEPTHFUNC_EQUAL) ? VK_COMPARE_OP_EQUAL : VK_COMPARE_OP_LESS_OR_EQUAL;
#endif
	depth_stencil_state.depthBoundsTestEnable = VK_FALSE;
	depth_stencil_state.stencilTestEnable = (def->shadow_phase != SHADOW_DISABLED || def->stencil_mark) ? VK_TRUE : VK_FALSE;

	if (def->stencil_mark) {
		// mark entity pixels with stencil bit 0x80 so shadow finish skips them
		depth_stencil_state.front.failOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.passOp = VK_STENCIL_OP_REPLACE;
		depth_stencil_state.front.depthFailOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.compareOp = VK_COMPARE_OP_ALWAYS;
		depth_stencil_state.front.compareMask = 0xFF;
		depth_stencil_state.front.writeMask = 0x80;
		depth_stencil_state.front.reference = 0x80;

		depth_stencil_state.back = depth_stencil_state.front;

	} else if (def->shadow_phase == SHADOW_EDGES) {
		// z-fail (Carmack's reverse): count fragments behind scene geometry.
		// WRAP keeps the count modular across nested volumes and draw order;
		// writeMask 0x7F preserves the 0x80 entity-mark bit. Needs a closed
		// volume (near + far caps) and depth clamp.
		depth_stencil_state.front.failOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.passOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.depthFailOp = (def->face_culling == CT_FRONT_SIDED) ? VK_STENCIL_OP_DECREMENT_AND_WRAP : VK_STENCIL_OP_INCREMENT_AND_WRAP;
		depth_stencil_state.front.compareOp = VK_COMPARE_OP_EQUAL;
		depth_stencil_state.front.compareMask = 0x80;  // skip entity-marked pixels
		depth_stencil_state.front.writeMask = 0x7F;    // only write shadow count to bits 0-6
		depth_stencil_state.front.reference = 0;        // pass where bit 7 == 0 (not entity)

		depth_stencil_state.back = depth_stencil_state.front;

	} else if (def->shadow_phase == SHADOW_FS_QUAD) {
		depth_stencil_state.front.failOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.passOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.depthFailOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.compareOp = VK_COMPARE_OP_NOT_EQUAL;
		depth_stencil_state.front.compareMask = 0x7F;  // check shadow bits 0-6 only
		depth_stencil_state.front.writeMask = 0x7F;
		depth_stencil_state.front.reference = 0;

		depth_stencil_state.back = depth_stencil_state.front;
	}

	depth_stencil_state.minDepthBounds = 0.0f;
	depth_stencil_state.maxDepthBounds = 1.0f;

	Com_Memset(&attachment_blend_state, 0, sizeof(attachment_blend_state));
	attachment_blend_state.blendEnable = (state_bits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)) ? VK_TRUE : VK_FALSE;

	if (def->shadow_phase == SHADOW_EDGES || def->shader_type == TYPE_SINGLE_TEXTURE_DF)
		attachment_blend_state.colorWriteMask = 0;
	else
		attachment_blend_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	if (attachment_blend_state.blendEnable) {
		switch (state_bits & GLS_SRCBLEND_BITS) {
			case GLS_SRCBLEND_ZERO:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
				break;
			case GLS_SRCBLEND_ONE:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
				break;
			case GLS_SRCBLEND_DST_COLOR:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
				break;
			case GLS_SRCBLEND_SRC_ALPHA:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_DST_ALPHA:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ALPHA_SATURATE:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
				break;
			default:
				ri.Error( ERR_DROP, "create_pipeline: invalid src blend state bits\n" );
				break;
		}
		switch (state_bits & GLS_DSTBLEND_BITS) {
			case GLS_DSTBLEND_ZERO:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
				break;
			case GLS_DSTBLEND_ONE:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
				break;
			case GLS_DSTBLEND_SRC_COLOR:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
				break;
			case GLS_DSTBLEND_SRC_ALPHA:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_DST_ALPHA:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
				break;
			default:
				ri.Error( ERR_DROP, "create_pipeline: invalid dst blend state bits\n" );
				break;
		}

		// HUD render pass: use separate alpha blend to accumulate toward opaque
		// This matches glBlendFuncSeparate(srcFactor, dstFactor, GL_ONE, GL_ONE) from renderergl2
		// RGB blends normally so specular shows through texture alpha
		// Alpha accumulates: src=ONE, dst=ONE adds alphas together, saturating toward 1.0
		if (renderPassIndex == RENDER_PASS_HUD) {
			attachment_blend_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			attachment_blend_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		} else {
			attachment_blend_state.srcAlphaBlendFactor = attachment_blend_state.srcColorBlendFactor;
			attachment_blend_state.dstAlphaBlendFactor = attachment_blend_state.dstColorBlendFactor;
		}
		attachment_blend_state.colorBlendOp = VK_BLEND_OP_ADD;
		attachment_blend_state.alphaBlendOp = VK_BLEND_OP_ADD;

		if ( def->allow_discard && vkSamples != VK_SAMPLE_COUNT_1_BIT ) {
			// try to reduce pixel fillrate for transparent surfaces, this yields 1..10% fps increase when multisampling in enabled
			if ( attachment_blend_state.srcColorBlendFactor == VK_BLEND_FACTOR_SRC_ALPHA && attachment_blend_state.dstColorBlendFactor == VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA ) {
				frag_spec_data[7].i = 1;
			} else if ( attachment_blend_state.srcColorBlendFactor == VK_BLEND_FACTOR_ONE && attachment_blend_state.dstColorBlendFactor == VK_BLEND_FACTOR_ONE ) {
				frag_spec_data[7].i = 2;
			}
		}
	}

	blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.pNext = NULL;
	blend_state.flags = 0;
	blend_state.logicOpEnable = VK_FALSE;
	blend_state.logicOp = VK_LOGIC_OP_COPY;

	blend_attachments[0] = attachment_blend_state;

	if ( emissiveActive ) {
		// the emissive layer mirrors the color blend, so it only needs writes from
		// stages that add light (emitters, overbright lit models) or that cover an
		// emitter without depth occlusion (blends attenuate/accumulate, depth-test-
		// disabled 2D replaces). Opaque depth-tested geometry is masked off: the
		// layer clears each frame and depth already excludes hidden emitters. The
		// lighting and dynamic-light shaders always write out_emissive (dlight
		// glow) and keep the mask unconditionally.
		VkPipelineColorBlendAttachmentState em = attachment_blend_state;
		if ( ( def->shader_type >= TYPE_GENERIC_BEGIN
				&& ( ( def->state_bits & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) )
					|| ( def->state_bits & GLS_DEPTHTEST_DISABLE ) ) )
			|| def->shader_type == TYPE_SINGLE_TEXTURE_LIGHTING
			|| def->shader_type == TYPE_SINGLE_TEXTURE_LIGHTING_LINEAR
			|| def->shader_type == TYPE_SINGLE_TEXTURE_LIGHTING_OVERBRIGHT )
			em.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		else
			em.colorWriteMask = 0;
		blend_attachments[1] = em;
		blend_state.attachmentCount = 2;
	} else {
		blend_state.attachmentCount = 1;
	}
	blend_state.pAttachments = blend_attachments;
	blend_state.blendConstants[0] = 0.0f;
	blend_state.blendConstants[1] = 0.0f;
	blend_state.blendConstants[2] = 0.0f;
	blend_state.blendConstants[3] = 0.0f;

	dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.pNext = NULL;
	dynamic_state.flags = 0;
	dynamic_state.dynamicStateCount = ARRAY_LEN( dynamic_state_array );
	dynamic_state.pDynamicStates = dynamic_state_array;

	create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	create_info.pNext = NULL;
	create_info.flags = 0;
	create_info.stageCount = ARRAY_LEN(shader_stages);
	create_info.pStages = shader_stages;
	create_info.pVertexInputState = &vertex_input_state;
	create_info.pInputAssemblyState = &input_assembly_state;
	create_info.pTessellationState = NULL;
	create_info.pViewportState = &viewport_state;
	create_info.pRasterizationState = &rasterization_state;
	create_info.pMultisampleState = &multisample_state;
	create_info.pDepthStencilState = &depth_stencil_state;
	create_info.pColorBlendState = &blend_state;
	create_info.pDynamicState = &dynamic_state;

	if ( def->shader_type == TYPE_DOT )
		create_info.layout = vk.pipeline_layout_storage;
	else
		create_info.layout = vk.pipeline_layout;

	if ( renderPassIndex == RENDER_PASS_SCREENMAP ) {
		create_info.renderPass = vk.render_pass.screenmap;
	} else if ( renderPassIndex == RENDER_PASS_HUD ) {
		create_info.renderPass = vk.render_pass.hudBuffer;
	} else if ( renderPassIndex == RENDER_PASS_POST_BLOOM ) {
		// Post-bloom uses post_bloom (handles MSAA properly)
		create_info.renderPass = vk.render_pass.post_bloom;
	} else {
		// Q3VR: Use main for main render pass (XR-only, no desktop rendering)
		// RENDER_PASS_MAIN or any other value defaults to main
		create_info.renderPass = vk.render_pass.main;
		// Sanity check: if not RENDER_PASS_MAIN, sample count must match
		if ( renderPassIndex != RENDER_PASS_MAIN ) {
			ri.Printf( PRINT_WARNING, "create_pipeline: unexpected renderPassIndex %d, defaulting to main\n", renderPassIndex );
		}
	}

	create_info.subpass = 0;
	create_info.basePipelineHandle = VK_NULL_HANDLE;
	create_info.basePipelineIndex = -1;

	{
		VkResult res = qvkCreateGraphicsPipelines( vk.device, vk.pipelineCache, 1, &create_info, NULL, &pipeline );
		if ( res < 0 ) {
			// deviceWaitIdle tells a bad create apart from an earlier device loss
			const VkResult idle = qvkDeviceWaitIdle( vk.device );
			ri.Error( ERR_FATAL, "Vulkan: vkCreateGraphicsPipelines returned %s for def#%i "
				"(type=%i state=0x%08X pass=%i fog=%i mirror=%i shadow=%i cull=%i "
				"pofs=%i prim=%i acff=%i stencil=%i color=%02X/%02X), deviceWaitIdle=%s",
				vk_result_string( res ), def_index,
				def->shader_type, def->state_bits, renderPassIndex,
				def->fog_stage, def->mirror, def->shadow_phase, def->face_culling,
				def->polygon_offset, def->primitives, def->acff, def->stencil_mark,
				def->color.rgb, def->color.alpha,
				vk_result_string( idle ) );
		}
	}

	SET_OBJECT_NAME( pipeline, va( "pipeline def#%i, pass#%i", def_index, renderPassIndex ), VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );

	vk.pipeline_create_count++;

	return pipeline;
}


static uint32_t vk_alloc_pipeline( const Vk_Pipeline_Def *def ) {
	VK_Pipeline_t *pipeline;
	if ( vk.pipelines_count >= MAX_VK_PIPELINES ) {
		ri.Error( ERR_DROP, "alloc_pipeline: MAX_VK_PIPELINES reached" );
		return 0;
	} else {
		int j;
		pipeline = &vk.pipelines[ vk.pipelines_count ];
		pipeline->def = *def;
		for ( j = 0; j < RENDER_PASS_COUNT; j++ ) {
			pipeline->handle[j] = VK_NULL_HANDLE;
		}
		return vk.pipelines_count++;
	}
}


VkPipeline vk_gen_pipeline( uint32_t index ) {
	if ( index < vk.pipelines_count ) {
		VK_Pipeline_t *pipeline = vk.pipelines + index;
		const renderPass_t pass = vk.renderPassIndex;
		if ( pipeline->handle[ pass ] == VK_NULL_HANDLE ) {
			pipeline->handle[ pass ] = create_pipeline( &pipeline->def, pass, index );
		}
		return pipeline->handle[ pass ];
	} else {
		ri.Error( ERR_FATAL, "%s(%i): NULL pipeline", __func__, index );
		return VK_NULL_HANDLE;
	}
}


uint32_t vk_find_pipeline_ext( uint32_t base, const Vk_Pipeline_Def *def, qboolean use ) {
	const Vk_Pipeline_Def *cur_def;
	uint32_t index;

	for ( index = base; index < vk.pipelines_count; index++ ) {
		cur_def = &vk.pipelines[ index ].def;
		if ( memcmp( cur_def, def, sizeof( *def ) ) == 0 ) {
			goto found;
		}
	}

	index = vk_alloc_pipeline( def );
found:

	if ( use )
		vk_gen_pipeline( index );

	return index;
}


void vk_get_pipeline_def( uint32_t pipeline, Vk_Pipeline_Def *def ) {
	if ( pipeline >= vk.pipelines_count ) {
		Com_Memset( def, 0, sizeof( *def ) );
	} else {
		Com_Memcpy( def, &vk.pipelines[ pipeline ].def, sizeof( *def ) );
	}
}


static void get_viewport_rect(VkRect2D *r)
{
	if ( backEnd.projection2D )
	{
		r->offset.x = 0;
		r->offset.y = 0;
		r->extent.width = vk.renderWidth;
		r->extent.height = vk.renderHeight;
	}
	else
	{
		r->offset.x = backEnd.viewParms.viewportX * vk.renderScaleX;
		r->offset.y = vk.renderHeight - (backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight) * vk.renderScaleY;
		r->extent.width = (float)backEnd.viewParms.viewportWidth * vk.renderScaleX;
		r->extent.height = (float)backEnd.viewParms.viewportHeight * vk.renderScaleY;
	}
}

static void get_viewport(VkViewport *viewport, Vk_Depth_Range depth_range) {
	VkRect2D r;

	get_viewport_rect( &r );

	viewport->x = (float)r.offset.x;
	viewport->y = (float)r.offset.y;
	viewport->width = (float)r.extent.width;
	viewport->height = (float)r.extent.height;

	switch ( depth_range ) {
		default:
#ifdef USE_REVERSED_DEPTH
		//case DEPTH_RANGE_NORMAL:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 1.0f;
			break;
		case DEPTH_RANGE_ZERO:
			viewport->minDepth = 1.0f;
			viewport->maxDepth = 1.0f;
			break;
		case DEPTH_RANGE_ONE:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 0.0f;
			break;
		case DEPTH_RANGE_WEAPON:
			viewport->minDepth = 0.6f;
			viewport->maxDepth = 1.0f;
			break;
#else
		//case DEPTH_RANGE_NORMAL:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 1.0f;
			break;
		case DEPTH_RANGE_ZERO:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 0.0f;
			break;
		case DEPTH_RANGE_ONE:
			viewport->minDepth = 1.0f;
			viewport->maxDepth = 1.0f;
			break;
		case DEPTH_RANGE_WEAPON:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 0.3f;
			break;
#endif
	}
}

static void get_scissor_rect(VkRect2D *r) {

	if ( backEnd.viewParms.portalView != PV_NONE )
	{
		r->offset.x = backEnd.viewParms.scissorX;
		r->offset.y = glConfig.vidHeight - backEnd.viewParms.scissorY - backEnd.viewParms.scissorHeight;
		r->extent.width = backEnd.viewParms.scissorWidth;
		r->extent.height = backEnd.viewParms.scissorHeight;
	}
	else
	{
		get_viewport_rect(r);

		if (r->offset.x < 0)
			r->offset.x = 0;
		if (r->offset.y < 0)
			r->offset.y = 0;

		if (r->offset.x + r->extent.width > glConfig.vidWidth)
			r->extent.width = glConfig.vidWidth - r->offset.x;
		if (r->offset.y + r->extent.height > glConfig.vidHeight)
			r->extent.height = glConfig.vidHeight - r->offset.y;
	}
}


static void get_mvp_transform( float *mvp )
{
	if ( backEnd.projection2D )
	{
		// Use vk.renderWidth/Height which are set to HUD buffer dimensions
		// when rendering to the HUD buffer, otherwise they're glConfig dimensions
		float mvp0 = 2.0f / vk.renderWidth;
		float mvp5 = 2.0f / vk.renderHeight;

		mvp[0]  =  mvp0; mvp[1]  =  0.0f; mvp[2]  = 0.0f; mvp[3]  = 0.0f;
		mvp[4]  =  0.0f; mvp[5]  =  mvp5; mvp[6]  = 0.0f; mvp[7]  = 0.0f;
#ifdef USE_REVERSED_DEPTH
		mvp[8]  =  0.0f; mvp[9]  =  0.0f; mvp[10] = 0.0f; mvp[11] = 0.0f;
		mvp[12] = -1.0f; mvp[13] = -1.0f; mvp[14] = 1.0f; mvp[15] = 1.0f;
#else
		mvp[8]  =  0.0f; mvp[9]  =  0.0f; mvp[10] = 1.0f; mvp[11] = 0.0f;
		mvp[12] = -1.0f; mvp[13] = -1.0f; mvp[14] = 0.0f; mvp[15] = 1.0f;
#endif
	}
	else
	{
		const float *p = backEnd.viewParms.projectionMatrix;
		float proj[16];
		Com_Memcpy( proj, p, 64 );

		// update q3's proj matrix (opengl) to vulkan conventions: z - [0, 1] instead of [-1, 1] and invert y direction
		proj[5] = -p[5];
		//proj[10] = ( p[10] - 1.0f ) / 2.0f;
		//proj[14] = p[14] / 2.0f;
		myGlMultMatrix( vk_world.modelview_transform, proj, mvp );
	}
}


void vk_clear_color( const vec4_t color ) {

	VkClearAttachment attachment;
	VkClearRect clear_rect;

	if ( !vk.active )
		return;

	attachment.colorAttachment = 0;
	attachment.clearValue.color.float32[0] = color[0];
	attachment.clearValue.color.float32[1] = color[1];
	attachment.clearValue.color.float32[2] = color[2];
	attachment.clearValue.color.float32[3] = color[3];
	attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	get_scissor_rect( &clear_rect.rect );
	clear_rect.baseArrayLayer = 0;
	// Q3VR: In multiview render passes, layerCount must be 1 - the view mask
	// automatically broadcasts the clear to all active views (both eyes)
	clear_rect.layerCount = 1;

	qvkCmdClearAttachments( vk.cmd->command_buffer, 1, &attachment, 1, &clear_rect );
}


void vk_clear_depth( qboolean clear_stencil ) {

	VkClearAttachment attachment;
	VkClearRect clear_rect[1];

	if ( !vk.active )
		return;

	if ( vk_world.dirty_depth_attachment == 0 )
		return;

	attachment.colorAttachment = 0;
#ifdef USE_REVERSED_DEPTH
	attachment.clearValue.depthStencil.depth = 0.0f;
#else
	attachment.clearValue.depthStencil.depth = 1.0f;
#endif
	attachment.clearValue.depthStencil.stencil = 0;
	if ( clear_stencil && glConfig.stencilBits > 0 ) {
		attachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	} else {
		attachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	get_scissor_rect( &clear_rect[0].rect );
	clear_rect[0].baseArrayLayer = 0;
	// Q3VR: In multiview render passes, layerCount must be 1 - the view mask
	// automatically broadcasts the clear to all active views (both eyes)
	clear_rect[0].layerCount = 1;

	qvkCmdClearAttachments( vk.cmd->command_buffer, 1, &attachment, 1, clear_rect );
}


// Inverse of a rigid/orthonormal affine GL matrix (rotation|reflection + translation).
// Valid for view matrices incl. s_flipMatrix axis permutations.
static void Matrix4x4_OrthonormalInvert( const float in[16], float out[16] )
{
	out[0] = in[0];  out[4] = in[1];  out[8]  = in[2];
	out[1] = in[4];  out[5] = in[5];  out[9]  = in[6];
	out[2] = in[8];  out[6] = in[9];  out[10] = in[10];
	out[3] = 0.0f;   out[7] = 0.0f;   out[11] = 0.0f;
	out[12] = -( in[12]*out[0] + in[13]*out[4] + in[14]*out[8]  );
	out[13] = -( in[12]*out[1] + in[13]*out[5] + in[14]*out[9]  );
	out[14] = -( in[12]*out[2] + in[13]*out[6] + in[14]*out[10] );
	out[15] = 1.0f;
}


float vk_view_eyeproj[2][16];

void vk_set_view_eyeproj( void )
{
	int e;

	if ( tr.vrParms.valid && !backEnd.projection2D &&
		!( backEnd.isDrawingHUD || backEnd.refdef.isHUD ) &&
		!( vr.virtual_screen || vr.weapon_zoomed ) ) {
		// Stereo (normal or portal): eyeProj = E'_e then P_e, where
		// E'_e = inverse(world monoView) * world eyeView_e. Exact because,
		// for any entity's local-to-world transform L, L * E'_e equals what
		// L folded against world.eyeViewMatrix[e] would produce (both share
		// the same L against world.modelMatrix / world.eyeViewMatrix[e]).
		float invMono[16], eprime[16];
		const float *proj[2];

		if ( backEnd.viewParms.portalView != PV_NONE ) {
			proj[0] = tr.vrParms.mirrorProjectionEye[0];
			proj[1] = tr.vrParms.mirrorProjectionEye[1];
		} else {
			proj[0] = tr.vrParms.projectionEye[0];
			proj[1] = tr.vrParms.projectionEye[1];
		}

		Matrix4x4_OrthonormalInvert( backEnd.viewParms.world.modelMatrix, invMono );
		for ( e = 0; e < 2; e++ ) {
			myGlMultMatrix( invMono, backEnd.viewParms.world.eyeViewMatrix[e], eprime );
			myGlMultMatrix( eprime, proj[e], vk_view_eyeproj[e] );
		}
		return;
	}

	// All cyclopean/mono flavors: both slots get the same projection the old
	// code multiplied per draw. Guards mirror vk_update_mvp's ladder exactly.
	{
		float proj[16];

		if ( tr.vrParms.valid && ( backEnd.isDrawingHUD || backEnd.refdef.isHUD ) ) {
			Com_Memcpy( proj, tr.vrParms.monoVRProjection, sizeof( proj ) );
		} else if ( tr.vrParms.valid && backEnd.viewParms.portalView != PV_NONE &&
				( vr.virtual_screen || vr.weapon_zoomed ) ) {
			// Portal cyclopean: plain aspect scale, no refdef-FOV override
			Com_Memcpy( proj, backEnd.viewParms.projectionMatrix, sizeof( proj ) );
			if ( vr.weapon_zoomed ) {
				proj[5] *= (float)glConfig.vidWidth / (float)glConfig.vidHeight;
			} else {
				proj[5] *= (float)glConfig.vidHeight / (float)glConfig.vidWidth;
			}
			proj[8] = 0.0f;
			proj[9] = 0.0f;
		} else if ( tr.vrParms.valid && ( vr.virtual_screen || vr.weapon_zoomed ) ) {
			Com_Memcpy( proj, tr.vrParms.projection, sizeof( proj ) );
			if ( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) {
				// UI model scenes: refdef FOV scaled by the 4:3 crop factor
				float cropHeight = (float)( glConfig.vidWidth * 3 ) / 4.0f;
				float cropFactor = (float)glConfig.vidHeight / cropHeight;
				proj[0] = ( 1.0f / tan( DEG2RAD( backEnd.viewParms.fovX ) * 0.5f ) ) / cropFactor;
				proj[5] = ( -1.0f / tan( DEG2RAD( backEnd.viewParms.fovY ) * 0.5f ) ) / cropFactor;
			} else if ( vr.weapon_zoomed ) {
				proj[5] *= (float)glConfig.vidWidth / (float)glConfig.vidHeight;
			} else {
				proj[5] *= (float)glConfig.vidHeight / (float)glConfig.vidWidth;
			}
			proj[8] = 0.0f;
			proj[9] = 0.0f;
		} else {
			Com_Memcpy( proj, backEnd.viewParms.projectionMatrix, sizeof( proj ) );
		}

		Com_Memcpy( vk_view_eyeproj[0], proj, sizeof( proj ) );
		Com_Memcpy( vk_view_eyeproj[1], proj, sizeof( proj ) );
	}
}


void vk_update_mvp( const float *m ) {
	float push_constants[16]; // mono modelview

	if ( !vk.recordingCommands ) {
		return;
	}

	if ( backEnd.projection2D ) {
		// 2D ortho: common scale/translate in the push; per-eye asymmetry and
		// HUD parallax become clip-space translations in eyeProj (view slot).
		int hudStatus = vr_currentHudDrawStatus ? vr_currentHudDrawStatus->integer : -1;
		qboolean isVirtualScreen = ri.VR_InVirtualScreen();

		float hudScale = 1.0f;
		if ( backEnd.isDrawingHUD && hudStatus == 2 && !isVirtualScreen ) {
			hudScale = ( vr_hudScale ? vr_hudScale->value : 1.0f ) * ( 2.0f / 3.0f );
		}

		float mvp0 = 2.0f * hudScale / vk.renderWidth;
		float mvp5 = 2.0f * hudScale / vk.renderHeight;

		float asymmetryOffsetX[2] = { 0.0f, 0.0f };
		float asymmetryOffsetY = 0.0f;
		qboolean isHudMode1 = ( backEnd.isDrawingHUD && hudStatus == 1 );
		if ( tr.vrParms.valid && !isVirtualScreen && !isHudMode1 && !vr.weapon_zoomed ) {
			asymmetryOffsetX[0] = tr.vrParms.projectionEye[0][8];
			asymmetryOffsetX[1] = tr.vrParms.projectionEye[1][8];
			asymmetryOffsetY = tr.vrParms.projectionEye[0][9];
		}

		float depthOffset = 0.0f;
		if ( backEnd.isDrawingHUD && hudStatus == 2 && !vr.first_person_following && !vr.weapon_zoomed ) {
			float hudDepth = vr_currentHudDepth ? vr_currentHudDepth->value : 3.0f;
			float heightFraction = 0.05f / ( hudDepth + 1.0f );
			depthOffset = heightFraction * (float)vk.renderHeight * mvp0;
		}

		float yOffset = 0.0f;
		if ( backEnd.isDrawingHUD && hudStatus == 2 && !isVirtualScreen ) {
			yOffset = -asymmetryOffsetY * 0.5f;
			float userOffset = vr_hudYOffset ? vr_hudYOffset->value : 0.0f;
			yOffset += -userOffset * mvp5 * 0.5f;
		}

		// Common part -> push
		Com_Memset( push_constants, 0, sizeof( push_constants ) );
		push_constants[0]  = mvp0;
		push_constants[5]  = mvp5;
		push_constants[12] = -hudScale;
		push_constants[13] = -hudScale + yOffset;
#ifdef USE_REVERSED_DEPTH
		push_constants[14] = 1.0f;
#else
		push_constants[10] = 1.0f;
#endif
		push_constants[15] = 1.0f;

		// Per-eye part -> view slot (pure clip-space translation)
		Com_Memset( vk_view_eyeproj, 0, sizeof( vk_view_eyeproj ) );
		for ( int e = 0; e < 2; e++ ) {
			vk_view_eyeproj[e][0] = vk_view_eyeproj[e][5] =
			vk_view_eyeproj[e][10] = vk_view_eyeproj[e][15] = 1.0f;
		}
		vk_view_eyeproj[0][12] = -asymmetryOffsetX[0] + depthOffset;
		vk_view_eyeproj[1][12] = -asymmetryOffsetX[1] - depthOffset;
		VK_PushEyeProj();
	} else if ( m ) {
		// Explicit modelview (shadows, flares): eyeProj already set per view.
		Com_Memcpy( push_constants, m, sizeof( push_constants ) );
	} else {
		Com_Memcpy( push_constants, vk_world.modelview_transform, sizeof( push_constants ) );
	}

	qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout,
		VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof( push_constants ), push_constants );
	vk.stats.push_size += sizeof( push_constants );
}


static VkBuffer shade_bufs[8];
static int bind_base;
static int bind_count;

static void vk_bind_index_attr( int index )
{
	if ( bind_base == -1 ) {
		bind_base = index;
		bind_count = 1;
	} else {
		bind_count = index - bind_base + 1;
	}
}


static void vk_bind_attr( int index, unsigned int item_size, const void *src ) {
	const uint32_t offset = PAD( vk.cmd->vertex_buffer_offset, 32 );
	const uint32_t size = tess.numVertexes * item_size;

	if ( offset + size > vk.geometry_buffer_size ) {
		// schedule geometry buffer resize
		vk.geometry_buffer_size_new = log2pad( offset + size, 1 );
	} else {
		vk.cmd->buf_offset[ index ] = offset;
		Com_Memcpy( vk.cmd->vertex_buffer_ptr + offset, src, size );
		vk.cmd->vertex_buffer_offset = (VkDeviceSize)offset + size;
	}

	vk_bind_index_attr( index );
}


uint32_t vk_tess_index( uint32_t numIndexes, const void *src ) {
	const uint32_t offset = vk.cmd->vertex_buffer_offset;
	const uint32_t size = numIndexes * sizeof( tess.indexes[0] );

	if ( offset + size > vk.geometry_buffer_size ) {
		// schedule geometry buffer resize
		vk.geometry_buffer_size_new = log2pad( offset + size, 1 );
		return ~0U;
	} else {
		Com_Memcpy( vk.cmd->vertex_buffer_ptr + offset, src, size );
		vk.cmd->vertex_buffer_offset = (VkDeviceSize)offset + size;
		return offset;
	}
}


void vk_bind_index_buffer( VkBuffer buffer, uint32_t offset )
{
	// Don't issue commands if we're not recording (e.g., during RE_Shutdown transition)
	if ( !vk.recordingCommands ) {
		return;
	}

	if ( vk.cmd->curr_index_buffer != buffer || vk.cmd->curr_index_offset != offset )
		qvkCmdBindIndexBuffer( vk.cmd->command_buffer, buffer, offset, VK_INDEX_TYPE_UINT32 );

	vk.cmd->curr_index_buffer = buffer;
	vk.cmd->curr_index_offset = offset;
}


#ifdef USE_VBO
void vk_draw_indexed( uint32_t indexCount, uint32_t firstIndex )
{
	qvkCmdDrawIndexed( vk.cmd->command_buffer, indexCount, 1, firstIndex, 0, 0 );
}
#endif


void vk_bind_index( void )
{
#ifdef USE_VBO
	if ( tess.vboIndex ) {
		vk.cmd->num_indexes = 0;
		//qvkCmdBindIndexBuffer( vk.cmd->command_buffer, vk.vbo.index_buffer, tess.shader->iboOffset, VK_INDEX_TYPE_UINT32 );
		return;
	}
#endif

	vk_bind_index_ext( tess.numIndexes, tess.indexes );
}


void vk_bind_index_ext( const int numIndexes, const uint32_t *indexes )
{
	uint32_t offset	= vk_tess_index( numIndexes, indexes );
	if ( offset != ~0U ) {
		vk_bind_index_buffer( vk.cmd->vertex_buffer, offset );
		vk.cmd->num_indexes = numIndexes;
	} else {
		// overflowed
		vk.cmd->num_indexes = 0;
	}
}


void vk_bind_geometry( uint32_t flags )
{
	//unsigned int size;
	bind_base = -1;
	bind_count = 0;

	// Don't issue commands if we're not recording (e.g., during RE_Shutdown transition)
	if ( !vk.recordingCommands ) {
		return;
	}

	if ( ( flags & ( TESS_XYZ | TESS_RGBA0 | TESS_ST0 | TESS_ST1 | TESS_ST2 | TESS_NNN | TESS_RGBA1 | TESS_RGBA2 ) ) == 0 )
		return;

#ifdef USE_VBO
	if ( tess.vboIndex ) {

		shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = shade_bufs[3] = shade_bufs[4] = shade_bufs[5] = shade_bufs[6] = shade_bufs[7] = vk.vbo.vertex_buffer;

		if ( flags & TESS_XYZ ) {  // 0
			vk.cmd->vbo_offset[0] = tess.shader->vboOffset + 0;
			vk_bind_index_attr( 0 );
		}

		if ( flags & TESS_RGBA0 ) { // 1
			vk.cmd->vbo_offset[1] = tess.shader->stages[ tess.vboStage ]->rgb_offset[0];
			vk_bind_index_attr( 1 );
		}

		if ( flags & TESS_ST0 ) {  // 2
			vk.cmd->vbo_offset[2] = tess.shader->stages[ tess.vboStage ]->tex_offset[0];
			vk_bind_index_attr( 2 );
		}

		if ( flags & TESS_ST1 ) {  // 3
			vk.cmd->vbo_offset[3] = tess.shader->stages[ tess.vboStage ]->tex_offset[1];
			vk_bind_index_attr( 3 );
		}

		if ( flags & TESS_ST2 ) {  // 4
			vk.cmd->vbo_offset[4] = tess.shader->stages[ tess.vboStage ]->tex_offset[2];
			vk_bind_index_attr( 4 );
		}

		if ( flags & TESS_NNN ) { // 5
			vk.cmd->vbo_offset[5] = tess.shader->normalOffset;
			vk_bind_index_attr( 5 );
		}

		if ( flags & TESS_RGBA1 ) { // 6
			vk.cmd->vbo_offset[6] = tess.shader->stages[ tess.vboStage ]->rgb_offset[1];
			vk_bind_index_attr( 6 );
		}

		if ( flags & TESS_RGBA2 ) { // 7
			vk.cmd->vbo_offset[7] = tess.shader->stages[ tess.vboStage ]->rgb_offset[2];
			vk_bind_index_attr( 7 );
		}

		qvkCmdBindVertexBuffers( vk.cmd->command_buffer, bind_base, bind_count, shade_bufs, vk.cmd->vbo_offset + bind_base );

	} else
#endif // USE_VBO
	{
		shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = shade_bufs[3] = shade_bufs[4] = shade_bufs[5] = shade_bufs[6] = shade_bufs[7] = vk.cmd->vertex_buffer;

		if ( flags & TESS_XYZ ) {
			vk_bind_attr(0, sizeof(tess.xyz[0]), &tess.xyz[0]);
		}

		if ( flags & TESS_RGBA0 ) {
			vk_bind_attr(1, sizeof( color4ub_t ), tess.svars.colors[0][0].rgba);
		}

		if ( flags & TESS_ST0 ) {
			vk_bind_attr(2, sizeof( vec2_t ), tess.svars.texcoordPtr[0]);
		}

		if ( flags & TESS_ST1 ) {
			vk_bind_attr(3, sizeof( vec2_t ), tess.svars.texcoordPtr[1]);
		}

		if ( flags & TESS_ST2 ) {
			vk_bind_attr(4, sizeof( vec2_t ), tess.svars.texcoordPtr[2]);
		}

		// binding 5; must follow the lower-numbered binds because vk_bind_attr sets
		// bind_count from the last (highest) index it sees, not the max of all of them
		if ( flags & TESS_OVERBRIGHT ) {
			vk_bind_attr( 5, sizeof( float ), tess.svars.overbright );
		}

		if ( flags & TESS_NNN ) {
			vk_bind_attr(5, sizeof(tess.normal[0]), tess.normal);
		}

		if ( flags & TESS_RGBA1 ) {
			vk_bind_attr(6, sizeof( color4ub_t ), tess.svars.colors[1][0].rgba);
		}

		if ( flags & TESS_RGBA2 ) {
			vk_bind_attr(7, sizeof( color4ub_t ), tess.svars.colors[2][0].rgba);
		}

		qvkCmdBindVertexBuffers( vk.cmd->command_buffer, bind_base, bind_count, shade_bufs, vk.cmd->buf_offset + bind_base );
	}
}


void vk_bind_lighting( int stage, int bundle )
{
	bind_base = -1;
	bind_count = 0;

#ifdef USE_VBO
	if ( tess.vboIndex ) {

		shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = vk.vbo.vertex_buffer;

		vk.cmd->vbo_offset[0] = tess.shader->vboOffset + 0;
		vk.cmd->vbo_offset[1] = tess.shader->stages[ stage ]->tex_offset[ bundle ];
		vk.cmd->vbo_offset[2] = tess.shader->normalOffset;

		qvkCmdBindVertexBuffers( vk.cmd->command_buffer, 0, 3, shade_bufs, vk.cmd->vbo_offset + 0 );

	}
	else
#endif // USE_VBO
	{
		shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = vk.cmd->vertex_buffer;

		vk_bind_attr( 0, sizeof( tess.xyz[0] ), &tess.xyz[0] );
		vk_bind_attr( 1, sizeof( vec2_t ), tess.svars.texcoordPtr[ bundle ] );
		vk_bind_attr( 2, sizeof( tess.normal[0] ), tess.normal );

		qvkCmdBindVertexBuffers( vk.cmd->command_buffer, bind_base, bind_count, shade_bufs, vk.cmd->buf_offset + bind_base );
	}
}


void vk_reset_descriptor( int index )
{
	vk.cmd->descriptor_set.current[ index ] = VK_NULL_HANDLE;
}


void vk_update_descriptor( int index, VkDescriptorSet descriptor )
{
	if ( vk.cmd->descriptor_set.current[ index ] != descriptor ) {
		vk.cmd->descriptor_set.start = ( index < vk.cmd->descriptor_set.start ) ? index : vk.cmd->descriptor_set.start;
		vk.cmd->descriptor_set.end = ( index > vk.cmd->descriptor_set.end ) ? index : vk.cmd->descriptor_set.end;
	}
	vk.cmd->descriptor_set.current[ index ] = descriptor;
}


void vk_update_descriptor_offset( int index, uint32_t offset )
{
	vk.cmd->descriptor_set.offset[ index ] = offset;
}


void vk_bind_descriptor_sets( void )
{
	uint32_t offsets[2], offset_count;
	uint32_t start, end, count, i;

	start = vk.cmd->descriptor_set.start;
	if ( start == ~0U )
		return;

	// set 0 (uniform + eyeProj) is statically used by every standard pipeline
	// since the matrix split; if its tracking was wiped by a render-pass
	// transition, fold it back into this bind so no draw runs without it.
	if ( start != VK_DESC_UNIFORM && vk.cmd->descriptor_set.current[ VK_DESC_UNIFORM ] == VK_NULL_HANDLE ) {
		vk.cmd->descriptor_set.current[ VK_DESC_UNIFORM ] = vk.cmd->uniform_descriptor;
		start = VK_DESC_UNIFORM;
	}

	end = vk.cmd->descriptor_set.end;

	offset_count = 0;
	if ( start == VK_DESC_UNIFORM ) { // uniform + eyeproj dynamic offsets, binding order
		offsets[ offset_count++ ] = vk.cmd->descriptor_set.offset[ start ];
		offsets[ offset_count++ ] = vk.cmd->eyeproj_offset;
	}

	// Ensure we always bind at least up to VK_DESC_TEXTURE1 (set 2) when starting from set 0,
	// since multi-texture shaders expect texture1 to be bound even if only texture0 is explicitly set.
	// This prevents validation errors when a pipeline statically uses set 2 but we only updated sets 0-1.
	if ( start == VK_DESC_UNIFORM && end < VK_DESC_TEXTURE1 ) {
		end = VK_DESC_TEXTURE1;
	}

	count = end - start + 1;

	// fill NULL descriptor gaps (including end, since we bind from start to end inclusive)
	for ( i = start + 1; i <= end; i++ ) {
		if ( vk.cmd->descriptor_set.current[i] == VK_NULL_HANDLE ) {
			vk.cmd->descriptor_set.current[i] = tr.whiteImage->descriptor;
		}
	}

	// Q3VR: Always use multiview layout; mono modelview passed via 64-byte push
	// constants, per-eye projection via the ViewTransform UBO (set 0, binding 1)
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.pipeline_layout, start, count, vk.cmd->descriptor_set.current + start, offset_count, offsets );

	vk.cmd->descriptor_set.end = 0;
	vk.cmd->descriptor_set.start = ~0U;
}


void vk_bind_pipeline( uint32_t pipeline ) {
	VkPipeline vkpipe;

	// Don't issue commands if we're not recording (e.g., during RE_Shutdown transition)
	if ( !vk.recordingCommands ) {
		return;
	}

	vkpipe = vk_gen_pipeline( pipeline );

	if ( vkpipe != vk.cmd->last_pipeline ) {
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkpipe );
		vk.cmd->last_pipeline = vkpipe;
	}

	vk_world.dirty_depth_attachment |= ( vk.pipelines[ pipeline ].def.state_bits & GLS_DEPTHMASK_TRUE );
}

static void vk_update_depth_range( Vk_Depth_Range depth_range )
{
	if ( vk.cmd->depth_range != depth_range ) {
		VkRect2D scissor_rect;
		VkViewport viewport;

		vk.cmd->depth_range = depth_range;

		get_scissor_rect( &scissor_rect );

		if ( memcmp( &vk.cmd->scissor_rect, &scissor_rect, sizeof( scissor_rect ) ) != 0 ) {
			qvkCmdSetScissor( vk.cmd->command_buffer, 0, 1, &scissor_rect );
			vk.cmd->scissor_rect = scissor_rect;
		}

		get_viewport( &viewport, depth_range );
		qvkCmdSetViewport( vk.cmd->command_buffer, 0, 1, &viewport );
	}
}


void vk_draw_geometry( Vk_Depth_Range depth_range, qboolean indexed ) {

	// Don't issue commands if we're not recording (e.g., during RE_Shutdown transition)
	if ( !vk.recordingCommands ) {
		return;
	}

	if ( vk.geometry_buffer_size_new ) {
		// geometry buffer overflow happened this frame
		return;
	}

	vk_bind_descriptor_sets();

	// configure pipeline's dynamic state
	vk_update_depth_range( depth_range );

	qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT,
		64, sizeof( float ), &vk.cmd->emissive_factor );
	vk.stats.push_size += sizeof( float );

	// issue draw call(s)
#ifdef USE_VBO
	if ( tess.vboIndex )
		VBO_RenderIBOItems();
	else
#endif
	if ( indexed ) {
		qvkCmdDrawIndexed( vk.cmd->command_buffer, vk.cmd->num_indexes, 1, 0, 0, 0 );
	} else {
		qvkCmdDraw( vk.cmd->command_buffer, tess.numVertexes, 1, 0, 0 );
	}
}


void vk_draw_dot( uint32_t storage_offset )
{
	if ( vk.geometry_buffer_size_new ) {
		// geometry buffer overflow happened this frame
		return;
	}

	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_storage, VK_DESC_STORAGE, 1, &vk.storage.descriptor, 1, &storage_offset );

	// configure pipeline's dynamic state
	vk_update_depth_range( DEPTH_RANGE_NORMAL );

	qvkCmdDraw( vk.cmd->command_buffer, tess.numVertexes, 1, 0, 0 );

	// vk.storage.descriptor was just bound at set 0 via vk.pipeline_layout_storage, which is
	// NOT compatible-for-set-0 with vk.pipeline_layout (2 UNIFORM_BUFFER_DYNAMIC bindings vs.
	// 1 STORAGE_BUFFER_DYNAMIC binding). Every gen/color/fog/light vertex shader statically
	// reads set 0 binding 1 (eyeProj), so re-dirty set 0 here, the same way VK_PushEyeProj's
	// tail does, to force the next main-layout draw to rebind set 0 with both dynamic offsets.
	// The offsets themselves are still valid from earlier pushes; nothing new needs pushing.
	vk_reset_descriptor( VK_DESC_UNIFORM );
	vk_update_descriptor( VK_DESC_UNIFORM, vk.cmd->uniform_descriptor );
}


static void vk_begin_render_pass( VkRenderPass renderPass, VkFramebuffer frameBuffer, qboolean clearValues, uint32_t width, uint32_t height )
{
	VkRenderPassBeginInfo render_pass_begin_info;
	VkClearValue clear_values[3];

	// Begin render pass.

	render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_begin_info.pNext = NULL;
	render_pass_begin_info.renderPass = renderPass;
	render_pass_begin_info.framebuffer = frameBuffer;
	render_pass_begin_info.renderArea.offset.x = 0;
	render_pass_begin_info.renderArea.offset.y = 0;
	render_pass_begin_info.renderArea.extent.width = width;
	render_pass_begin_info.renderArea.extent.height = height;

	if ( clearValues ) {
		// attachments layout:
		// [0] - resolve/color/presentation
		// [1] - depth/stencil
		// [2] - multisampled color, optional
		Com_Memset( clear_values, 0, sizeof( clear_values ) );
#ifndef USE_REVERSED_DEPTH
		clear_values[1].depthStencil.depth = 1.0;
#endif
		render_pass_begin_info.clearValueCount = vk.msaaActive ? 3 : 2;
		render_pass_begin_info.pClearValues = clear_values;

		vk_world.dirty_depth_attachment = 0;
	} else {
		render_pass_begin_info.clearValueCount = 0;
		render_pass_begin_info.pClearValues = NULL;
	}

	qvkCmdBeginRenderPass( vk.cmd->command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE );
	vk.inRenderPass = qtrue;

	vk.cmd->last_pipeline = VK_NULL_HANDLE;
	vk.cmd->depth_range = DEPTH_RANGE_COUNT;
}


void vk_begin_main_render_pass( qboolean clear )
{
	VkRenderPassBeginInfo render_pass_begin_info;
	VkClearValue clear_values[5];  // [0]=resolve/color, [1]=depth, [2]=MSAA color, [3]=emissive resolve, [4]=emissive msaa
	VkRenderPass renderPass;
	VkFramebuffer frameBuffer;

	// End current render pass if active
	if ( vk.inRenderPass ) {
		qvkCmdEndRenderPass( vk.cmd->command_buffer );
		vk.inRenderPass = qfalse;
	}

	// Validate XR resources are initialized
	if ( !vk.xr.colorInfo || vk.xr.colorIndex >= vk.xr.colorInfo->imageCount ) {
		ri.Printf( PRINT_WARNING, "vk_begin_main_render_pass: XR resources not ready\n" );
		return;
	}

	// Q3VR is XR-only: use FBO framebuffer (fboActive always true in VR)
	frameBuffer = vk.framebuffers.main;

	// Choose render pass based on whether we want to clear or preserve content
	// main uses LOAD_OP_CLEAR, mainResume uses LOAD_OP_LOAD
	renderPass = clear ? vk.render_pass.main : vk.render_pass.mainResume;

	// Validate render pass exists
	if ( renderPass == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_WARNING, "vk_begin_main_render_pass: render pass not ready (clear=%d)\n", clear );
		return;
	}

	vk.renderPassIndex = RENDER_PASS_MAIN;

	// Use FBO dimensions (fboActive always true in VR)
	// Note: glConfig.vidWidth/Height = vk.xr.width/height in VR
	vk.renderWidth = glConfig.vidWidth;
	vk.renderHeight = glConfig.vidHeight;

	vk.renderScaleX = vk.renderScaleY = 1.0f;

	// XR render pass: 2 attachments normally, 3 with MSAA (resolve target, depth, MSAA color)
	render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_begin_info.pNext = NULL;
	render_pass_begin_info.renderPass = renderPass;
	render_pass_begin_info.framebuffer = frameBuffer;
	render_pass_begin_info.renderArea.offset.x = 0;
	render_pass_begin_info.renderArea.offset.y = 0;
	render_pass_begin_info.renderArea.extent.width = vk.renderWidth;
	render_pass_begin_info.renderArea.extent.height = vk.renderHeight;

	if ( clear ) {
		// Clear values: [0] = color/resolve (black), [1] = depth (0.0 for reversed depth)
		// With MSAA: [2] = MSAA color (black)
		Com_Memset( clear_values, 0, sizeof( clear_values ) );
#ifndef USE_REVERSED_DEPTH
		clear_values[1].depthStencil.depth = 1.0f;
#endif
		if ( vk.msaaActive ) {
			// MSAA: [0]=resolve, [1]=depth, [2]=MSAA color; with HDR: +[3]=emissive resolve, [4]=emissive msaa
			render_pass_begin_info.clearValueCount = vk.hdrActive ? 5 : 3;
		} else {
			// Non-MSAA: [0]=color, [1]=depth; with HDR: +[2]=emissive resolve
			render_pass_begin_info.clearValueCount = vk.hdrActive ? 3 : 2;
		}
		render_pass_begin_info.pClearValues = clear_values;
		vk_world.dirty_depth_attachment = 0;
	} else {
		// Resuming after HUD pass: don't clear, preserve existing content
		render_pass_begin_info.clearValueCount = 0;
		render_pass_begin_info.pClearValues = NULL;
	}

	qvkCmdBeginRenderPass( vk.cmd->command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE );
	vk.inRenderPass = qtrue;

	if ( clear ) {
		// Safety net: guarantee the eye color target has DEFINED contents across
		// its FULL extent on the first 3D pass of the frame. The render-pass
		// LOAD_OP for the color/resolve target is config-dependent (DONT_CARE when
		// MSAA resolves it, CLEAR otherwise) and only ever covers what the subpass
		// actually rasterizes. A game module that submits a refdef smaller than the
		// per-eye render target would leave the surrounding region undefined; since
		// the color image is double-buffered, that region alternates between two
		// stale/undefined images each frame and the gamma pass composites the WHOLE
		// FBO to the XR swapchain -> white/black strobing outside the view rect.
		//
		// vkCmdClearAttachments clears the rects passed here directly and ignores
		// the dynamic scissor, so this covers the whole extent regardless of the
		// view rectangle set later by RB_BeginDrawingView. Only the clear==qtrue
		// (first) pass does this; the mainResume/HUD LOAD_OP_LOAD passes that
		// preserve intra-frame content are untouched. For the in-tree cgame (which
		// fills the eye every frame) this is a negligible, visually invisible clear.
		VkClearAttachment clearAttachment;
		VkClearRect clearRect;

		clearAttachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		clearAttachment.colorAttachment = 0;
		// Opaque black - matches RE_ClearVRFramebuffer (no spectator tint in the
		// Vulkan renderer; it is a stub that always clears black).
		clearAttachment.clearValue.color.float32[0] = 0.0f;
		clearAttachment.clearValue.color.float32[1] = 0.0f;
		clearAttachment.clearValue.color.float32[2] = 0.0f;
		clearAttachment.clearValue.color.float32[3] = 1.0f;

		clearRect.rect.offset.x = 0;
		clearRect.rect.offset.y = 0;
		clearRect.rect.extent.width = vk.renderWidth;
		clearRect.rect.extent.height = vk.renderHeight;
		clearRect.baseArrayLayer = 0;
		// Multiview: layerCount must be 1; the view mask broadcasts to both eyes.
		clearRect.layerCount = 1;

		qvkCmdClearAttachments( vk.cmd->command_buffer, 1, &clearAttachment, 1, &clearRect );
	}

	// Note: mono modelview is passed via push constants (64 bytes); per-eye
	// projection lives in the ViewTransform UBO (set 0, binding 1)

	vk.cmd->last_pipeline = VK_NULL_HANDLE;
	vk.cmd->depth_range = DEPTH_RANGE_COUNT;

	// Conservative reset of binding tracking across the render-pass transition;
	// vk_bind_descriptor_sets self-heals set 0 (see there) so the next draw is
	// never left without the uniform/eyeProj descriptor.
	Com_Memset( vk.cmd->descriptor_set.current, 0, sizeof( vk.cmd->descriptor_set.current ) );
	vk.cmd->descriptor_set.start = ~0U;
	vk.cmd->descriptor_set.end = 0;
}


void vk_begin_blur_render_pass( uint32_t index )
{
	VkFramebuffer frameBuffer = vk.framebuffers.blur[ index ];

	//vk.renderPassIndex = RENDER_PASS_BLOOM_EXTRACT; // doesn't matter, we will use dedicated pipelines

	vk.renderWidth = gls.captureWidth / ( 2 << ( index / 2 ) );
	vk.renderHeight = gls.captureHeight / ( 2 << ( index / 2 ) );

	//vk.renderScaleX = (float)vk.renderWidth / (float)glConfig.vidWidth;
	//vk.renderScaleY = (float)vk.renderHeight / (float)glConfig.vidHeight;
	vk.renderScaleX = vk.renderScaleY = 1.0f;

	vk_begin_render_pass( vk.render_pass.blur[ index ], frameBuffer, qfalse, vk.renderWidth, vk.renderHeight );
}


static void vk_begin_screenmap_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.screenmap;

	vk.renderPassIndex = RENDER_PASS_SCREENMAP;

	vk.renderWidth = vk.screenMapWidth;
	vk.renderHeight = vk.screenMapHeight;

	vk.renderScaleX = (float)vk.renderWidth / (float)glConfig.vidWidth;
	vk.renderScaleY = (float)vk.renderHeight / (float)glConfig.vidHeight;

	vk_begin_render_pass( vk.render_pass.screenmap, frameBuffer, qtrue, vk.renderWidth, vk.renderHeight );
}


void vk_end_render_pass( void )
{
	if ( !vk.inRenderPass ) {
		return; // Not in a render pass, nothing to end
	}
	qvkCmdEndRenderPass( vk.cmd->command_buffer );
	vk.inRenderPass = qfalse;
}


/*
 * vk_begin_hud_render_pass - Begin rendering to the HUD buffer
 *
 * Switches to the HUD framebuffer (1280x960) for rendering HUD elements.
 * After HUD rendering is complete, call vk_end_hud_render_pass() to resume
 * the main render pass. The HUD texture can then be sampled via vk.xr.hudDescriptor.
 */
void vk_begin_hud_render_pass( qboolean clear )
{
	VkRenderPassBeginInfo rpBI;
	VkViewport viewport;
	VkRect2D scissor;
	VkClearValue clearValues[2];

	if ( !vk.recordingCommands ) {
		return;
	}

	if ( vk.xr.hudFramebuffer == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_WARNING, "vk_begin_hud_render_pass: HUD buffer not initialized\n" );
		return;
	}

	// End current render pass if active
	if ( vk.inRenderPass ) {
		// Track if we're ending post_bloom: its finalLayout transitions color to SHADER_READ_ONLY
		// Only post_bloom does this; main keeps color in COLOR_ATTACHMENT
		if ( vk.renderPassIndex == RENDER_PASS_POST_BLOOM ) {
			vk.colorNeedsTransitionToAttachment = qtrue;
		}
		qvkCmdEndRenderPass( vk.cmd->command_buffer );
		vk.inRenderPass = qfalse;
	}

	// Set up clear values
	// Color: not cleared here (LOAD_OP_LOAD), but value must be provided for array indexing
	clearValues[0].color.float32[0] = 0.0f;
	clearValues[0].color.float32[1] = 0.0f;
	clearValues[0].color.float32[2] = 0.0f;
	clearValues[0].color.float32[3] = 0.0f;
	// Depth: cleared each frame (LOAD_OP_CLEAR)
	// USE_REVERSED_DEPTH: clear to 0.0 (farthest), depth test is GREATER_OR_EQUAL
	clearValues[1].depthStencil.depth = 0.0f;
	clearValues[1].depthStencil.stencil = 0;

	// Set up render pass begin info
	// Note: We use LOAD_OP_LOAD for color to preserve previous frame's content
	// (like OpenGL FBO behavior). Depth is cleared each frame for 3D model rendering.
	Com_Memset( &rpBI, 0, sizeof( rpBI ) );
	rpBI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rpBI.renderPass = vk.render_pass.hudBuffer;
	rpBI.framebuffer = vk.xr.hudFramebuffer;
	rpBI.renderArea.offset.x = 0;
	rpBI.renderArea.offset.y = 0;
	rpBI.renderArea.extent.width = HUD_BUFFER_WIDTH;
	rpBI.renderArea.extent.height = HUD_BUFFER_HEIGHT;
	rpBI.clearValueCount = 2;  // Color + depth (depth uses LOAD_OP_CLEAR)
	rpBI.pClearValues = clearValues;

	qvkCmdBeginRenderPass( vk.cmd->command_buffer, &rpBI, VK_SUBPASS_CONTENTS_INLINE );

	// Update state
	vk.renderPassIndex = RENDER_PASS_HUD;
	vk.inRenderPass = qtrue;
	vk.renderWidth = HUD_BUFFER_WIDTH;
	vk.renderHeight = HUD_BUFFER_HEIGHT;
	vk.renderScaleX = 1.0f;
	vk.renderScaleY = 1.0f;

	// Set viewport (Y-flipped for Vulkan coordinate space)
	viewport.x = 0.0f;
	viewport.y = (float)HUD_BUFFER_HEIGHT;  // Start at bottom
	viewport.width = (float)HUD_BUFFER_WIDTH;
	viewport.height = -(float)HUD_BUFFER_HEIGHT;  // Negative height for Y-flip
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	qvkCmdSetViewport( vk.cmd->command_buffer, 0, 1, &viewport );

	// Set scissor BEFORE any drawing commands (including vkCmdClearAttachments)
	// vkCmdClearAttachments uses current scissor state, so it must be set first
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = HUD_BUFFER_WIDTH;
	scissor.extent.height = HUD_BUFFER_HEIGHT;
	qvkCmdSetScissor( vk.cmd->command_buffer, 0, 1, &scissor );

	// Manual clear if requested (since we use LOAD_OP_LOAD for color)
	if ( clear ) {
		VkClearAttachment clearAttachment;
		VkClearRect clearRect;

		// Color: transparent black
		clearAttachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		clearAttachment.colorAttachment = 0;
		clearAttachment.clearValue.color.float32[0] = 0.0f;
		clearAttachment.clearValue.color.float32[1] = 0.0f;
		clearAttachment.clearValue.color.float32[2] = 0.0f;
		clearAttachment.clearValue.color.float32[3] = 0.0f;

		clearRect.rect.offset.x = 0;
		clearRect.rect.offset.y = 0;
		clearRect.rect.extent.width = HUD_BUFFER_WIDTH;
		clearRect.rect.extent.height = HUD_BUFFER_HEIGHT;
		clearRect.baseArrayLayer = 0;
		clearRect.layerCount = 1;

		qvkCmdClearAttachments( vk.cmd->command_buffer, 1, &clearAttachment, 1, &clearRect );
	}

	vk.cmd->last_pipeline = VK_NULL_HANDLE;

	// Conservative reset of binding tracking across the render-pass transition;
	// vk_bind_descriptor_sets self-heals set 0 (see there) so the next draw is
	// never left without the uniform/eyeProj descriptor.
	Com_Memset( vk.cmd->descriptor_set.current, 0, sizeof( vk.cmd->descriptor_set.current ) );
	vk.cmd->descriptor_set.start = ~0U;
	vk.cmd->descriptor_set.end = 0;
}


/*
 * vk_end_hud_render_pass - End rendering to the HUD buffer
 *
 * Ends the HUD render pass and transitions the HUD image to SHADER_READ_ONLY_OPTIMAL
 * layout (via render pass finalLayout). Then resumes the main XR render pass.
 */
void vk_end_hud_render_pass( void )
{
	if ( vk.renderPassIndex != RENDER_PASS_HUD ) {
		return;
	}

	// End the HUD render pass
	qvkCmdEndRenderPass( vk.cmd->command_buffer );
	vk.inRenderPass = qfalse;

	// HUD image is now in SHADER_READ_ONLY_OPTIMAL layout (from render pass finalLayout)

	// If we ended post_bloom when starting HUD, the FBO color was transitioned to SHADER_READ_ONLY
	// (post_bloom's finalLayout). We need to transition it back to COLOR_ATTACHMENT before
	// resuming the main render pass. Only do this if we actually ended post_bloom, not main.
	if ( vk.colorNeedsTransitionToAttachment ) {
		record_image_layout_transition( vk.cmd->command_buffer,
			vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, 0 );
		vk.colorNeedsTransitionToAttachment = qfalse;
	}

	// Resume main XR render pass without clearing (preserve existing content)
	vk_begin_main_render_pass( qfalse );
}


static qboolean vk_find_screenmap_drawsurfs( void )
{
	const void *curCmd = &backEndData->commands.cmds;
	const drawBufferCommand_t *db_cmd;
	const drawSurfsCommand_t *ds_cmd;

	for ( ;; ) {
		curCmd = PADP( curCmd, sizeof(void *) );
		switch ( *(const int *)curCmd ) {
			case RC_DRAW_BUFFER:
				db_cmd = (const drawBufferCommand_t *)curCmd;
				curCmd = (const void *)(db_cmd + 1);
				break;
			case RC_DRAW_SURFS:
				ds_cmd = (const drawSurfsCommand_t *)curCmd;
				return ds_cmd->refdef.needScreenMap;
			default:
				return qfalse;
		}
	}
}


#ifndef UINT64_MAX
#define UINT64_MAX 0xFFFFFFFFFFFFFFFFULL
#endif

static void vk_resize_geometry_buffer( void )
{
	int i;

	vk_end_render_pass();

	VK_CHECK( qvkEndCommandBuffer( vk.cmd->command_buffer ) );

	qvkResetCommandBuffer( vk.cmd->command_buffer, 0 );

	// the buffer is back in the initial state; without this, the next
	// vk_begin_frame would route it into vk_finish_frame and end/submit a
	// never-begun command buffer
	vk.recordingCommands = qfalse;
	vk.cmd->gpu_time_armed = qfalse;

	vk_wait_idle();

	vk_release_geometry_buffers();

	vk_create_geometry_buffers( vk.geometry_buffer_size_new );
	vk.geometry_buffer_size_new = 0;

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		vk_update_uniform_descriptor( vk.tess[ i ].uniform_descriptor, vk.tess[ i ].vertex_buffer );
		// fresh buffers: every cached eyeProj slot is gone
		vk.tess[ i ].eyeproj_cache_valid = qfalse;
	}

	ri.Printf( PRINT_DEVELOPER, "...geometry buffer resized to %iK\n", (int)( vk.geometry_buffer_size / 1024 ) );
}

/*
==============================================================================

XR FRAME FUNCTIONS

These functions handle OpenXR frame lifecycle, separate from desktop swapchain.
XR swapchain images are owned by OpenXR, not Vulkan.

==============================================================================
*/

static int vk_gpu_time_compare( const void *a, const void *b )
{
	const float fa = *(const float *)a, fb = *(const float *)b;
	return ( fa > fb ) - ( fa < fb );
}


/*
==================
vk_gpu_time_collect

This slot's stamps from its last frame, readable now that its fence was
waited on, into the r_gpuTimeLog window; one report per window. The desktop
mirror blit is a separate submission and is not in the span.
==================
*/
static void vk_gpu_time_collect( void )
{
	uint64_t results[6];  // start, scene end, frame end, each followed by its availability
	const uint32_t first = vk.cmd_index * 3;
	int window, n;

	if ( !vk.cmd->gpu_time_pending ) {
		return;
	}
	vk.cmd->gpu_time_pending = qfalse;

	if ( qvkGetQueryPoolResults( vk.device, vk.gpuTimePool, first, 3, sizeof( results ), results,
			2 * sizeof( uint64_t ), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT ) != VK_SUCCESS ) {
		return;
	}
	if ( !results[1] || !results[3] || !results[5] || results[2] < results[0] || results[4] < results[2] ) {
		return;
	}

	window = r_gpuTimeLog->integer;
	if ( window <= 0 ) {
		vk.gpuTime.count = 0;
		return;
	}
	if ( window > ARRAY_LEN( vk.gpuTime.ms ) ) {
		window = ARRAY_LEN( vk.gpuTime.ms );
	}

	vk.gpuTime.sceneMs[vk.gpuTime.count] = (float)( (double)( results[2] - results[0] ) * vk.timestampPeriod * 1e-6 );
	vk.gpuTime.ms[vk.gpuTime.count++] = (float)( (double)( results[4] - results[0] ) * vk.timestampPeriod * 1e-6 );
	if ( vk.gpuTime.count < window ) {
		return;
	}

	n = vk.gpuTime.count;
	qsort( vk.gpuTime.ms, n, sizeof( float ), vk_gpu_time_compare );
	qsort( vk.gpuTime.sceneMs, n, sizeof( float ), vk_gpu_time_compare );
	ri.Printf( PRINT_ALL, "GPU time over %i frames: frame median %.2f ms, p99 %.2f, max %.2f; scene median %.2f ms, p99 %.2f, max %.2f (%.0f%% of the frame)\n",
		n, vk.gpuTime.ms[n / 2], vk.gpuTime.ms[( n * 99 ) / 100], vk.gpuTime.ms[n - 1],
		vk.gpuTime.sceneMs[n / 2], vk.gpuTime.sceneMs[( n * 99 ) / 100], vk.gpuTime.sceneMs[n - 1],
		vk.gpuTime.ms[n / 2] > 0.0f ? 100.0f * vk.gpuTime.sceneMs[n / 2] / vk.gpuTime.ms[n / 2] : 0.0f );
	vk.gpuTime.count = 0;
}


// Top of the slot's command buffer: the frame's first timestamp
static void vk_gpu_time_begin( void )
{
	const uint32_t first = vk.cmd_index * 3;

	if ( vk.gpuTimePool == VK_NULL_HANDLE || r_gpuTimeLog->integer <= 0 ) {
		return;
	}
	qvkCmdResetQueryPool( vk.cmd->command_buffer, vk.gpuTimePool, first, 3 );
	qvkCmdWriteTimestamp( vk.cmd->command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk.gpuTimePool, first );
	vk.cmd->gpu_time_armed = qtrue;
	vk.cmd->gpu_time_scene_marked = qfalse;
}


/*
==================
vk_gpu_time_mark_scene

The 3D-to-2D boundary, where the backend finishes the scene and bloom
starts; a frame without one (menus) counts everything as scene.
==================
*/
void vk_gpu_time_mark_scene( void )
{
	if ( !vk.cmd || !vk.cmd->gpu_time_armed || vk.cmd->gpu_time_scene_marked || !vk.recordingCommands ) {
		return;
	}
	qvkCmdWriteTimestamp( vk.cmd->command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, vk.gpuTimePool, vk.cmd_index * 3 + 1 );
	vk.cmd->gpu_time_scene_marked = qtrue;
}


// Bottom of the frame's command buffer, before it ends and submits
static void vk_gpu_time_end( void )
{
	if ( !vk.cmd->gpu_time_armed ) {
		return;
	}
	vk_gpu_time_mark_scene();
	qvkCmdWriteTimestamp( vk.cmd->command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, vk.gpuTimePool, vk.cmd_index * 3 + 2 );
	vk.cmd->gpu_time_armed = qfalse;
	vk.cmd->gpu_time_pending = qtrue;
}


void vk_begin_frame( uint32_t colorIndex )
{
	VkCommandBufferBeginInfo begin_info;

	// Descriptor sets are dead during the map-load window (between
	// vk_release_resources() and vk_init_descriptors()); refuse to start a
	// frame that would bind freed or dangling descriptors.
	if ( !vk.descriptorsReady ) {
		ri.Printf( PRINT_DEVELOPER, "vk_begin_frame: skipped, descriptor sets not initialized\n" );
		// Guard: abandon a frame left open across the pool reset rather than
		// let vk_end_frame submit dead-set binds.
		if ( vk.recordingCommands || vk.inRenderPass ) {
			vk_discard_frame();
		}
		vk.frame_count = 0;
		return;
	}

	// If a frame is already in progress, we need to properly finish it first
	// This can happen during loading when VR_Renderer_SubmitLoadingFrame starts new frames
	// but the renderer doesn't always render to them (e.g., during reinit when uivm is NULL)
	if ( vk.frame_count > 0 || vk.recordingCommands ) {
		vk_finish_frame();
	}

	// Always start fresh: increment frame count
	vk.frame_count++;

#ifdef USE_UPLOAD_QUEUE
	vk_flush_staging_buffer( qtrue );
#endif

	// Rotate to next command buffer
	vk.cmd_index = (vk.cmd_index + 1) % NUM_COMMAND_BUFFERS;
	vk.cmd = &vk.tess[ vk.cmd_index ];

	// Wait for this command buffer's previous work to complete
	if ( vk.cmd->waitForFence ) {
		VkResult res = qvkWaitForFences( vk.device, 1, &vk.cmd->rendering_finished_fence, VK_FALSE, 1e10 );
		if ( res != VK_SUCCESS ) {
			if ( res == VK_ERROR_DEVICE_LOST ) {
				ri.Printf( PRINT_WARNING, "vk_begin_frame: vkWaitForFences returned %s\n", vk_result_string( res ) );
			} else {
				ri.Error( ERR_FATAL, "vk_begin_frame: vkWaitForFences returned %s", vk_result_string( res ) );
			}
		}
		VK_CHECK( qvkResetFences( vk.device, 1, &vk.cmd->rendering_finished_fence ) );
		vk.cmd->waitForFence = qfalse;
	}

	vk_gpu_time_collect();

	// Validate XR resources are initialized BEFORE accessing arrays
	if ( !vk.xr.initialized || !vk.xr.colorInfo ) {
		ri.Printf( PRINT_WARNING, "vk_begin_frame: XR resources not initialized (init=%d, colorInfo=%p)\n",
			vk.xr.initialized, (void*)vk.xr.colorInfo );
		vk.frame_count = 0;  // Reset so vk_end_frame won't try to submit
		return;
	}

	// Validate indices are in bounds
	if ( colorIndex >= vk.xr.colorInfo->imageCount ) {
		ri.Printf( PRINT_WARNING, "vk_begin_frame: colorIndex %u >= imageCount %u\n",
			colorIndex, vk.xr.colorInfo->imageCount );
		vk.frame_count = 0;
		return;
	}

	// Store current XR swapchain index (after validation)
	vk.xr.colorIndex = colorIndex;

	// Validate framebuffer exists based on rendering mode
	// When FBO is active: use vk.framebuffers.main for main rendering
	// When FBO is NOT active: use xr->framebuffers[] (direct to XR swapchain)
	if ( vk.framebuffers.main == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_WARNING, "vk_begin_frame: main framebuffer is NULL\n" );
		vk.frame_count = 0;
		return;
	}

	// Begin command buffer
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.pNext = NULL;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	begin_info.pInheritanceInfo = NULL;

	VK_CHECK( qvkBeginCommandBuffer( vk.cmd->command_buffer, &begin_info ) );
	vk.recordingCommands = qtrue;

	vk_gpu_time_begin();

	// Batch FBO layout transitions into a single pipeline barrier
	{
		VkImageMemoryBarrier barriers[3];
		uint32_t barrierCount = 0;

		if ( vk.color_image != VK_NULL_HANDLE ) {
			barriers[barrierCount].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barriers[barrierCount].pNext = NULL;
			barriers[barrierCount].srcAccessMask = VK_ACCESS_NONE;
			barriers[barrierCount].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			barriers[barrierCount].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barriers[barrierCount].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			barriers[barrierCount].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barriers[barrierCount].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barriers[barrierCount].image = vk.color_image;
			barriers[barrierCount].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barriers[barrierCount].subresourceRange.baseMipLevel = 0;
			barriers[barrierCount].subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
			barriers[barrierCount].subresourceRange.baseArrayLayer = 0;
			barriers[barrierCount].subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
			barrierCount++;
		}

		if ( vk.depth_image != VK_NULL_HANDLE ) {
			VkImageAspectFlags depthAspects = VK_IMAGE_ASPECT_DEPTH_BIT;
			if ( vk_format_has_stencil( vk.depth_format ) ) {
				depthAspects |= VK_IMAGE_ASPECT_STENCIL_BIT;
			}
			barriers[barrierCount].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barriers[barrierCount].pNext = NULL;
			barriers[barrierCount].srcAccessMask = VK_ACCESS_NONE;
			barriers[barrierCount].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			barriers[barrierCount].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barriers[barrierCount].newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			barriers[barrierCount].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barriers[barrierCount].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barriers[barrierCount].image = vk.depth_image;
			barriers[barrierCount].subresourceRange.aspectMask = depthAspects;
			barriers[barrierCount].subresourceRange.baseMipLevel = 0;
			barriers[barrierCount].subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
			barriers[barrierCount].subresourceRange.baseArrayLayer = 0;
			barriers[barrierCount].subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
			barrierCount++;
		}

		if ( vk.msaaActive && vk.msaa_image != VK_NULL_HANDLE ) {
			barriers[barrierCount].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barriers[barrierCount].pNext = NULL;
			barriers[barrierCount].srcAccessMask = VK_ACCESS_NONE;
			barriers[barrierCount].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			barriers[barrierCount].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barriers[barrierCount].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			barriers[barrierCount].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barriers[barrierCount].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barriers[barrierCount].image = vk.msaa_image;
			barriers[barrierCount].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barriers[barrierCount].subresourceRange.baseMipLevel = 0;
			barriers[barrierCount].subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
			barriers[barrierCount].subresourceRange.baseArrayLayer = 0;
			barriers[barrierCount].subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
			barrierCount++;
		}

		if ( barrierCount > 0 ) {
			qvkCmdPipelineBarrier( vk.cmd->command_buffer,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
				0, 0, NULL, 0, NULL, barrierCount, barriers );
		}
	}

	// Track stats
	if ( vk.cmd->vertex_buffer_offset > vk.stats.vertex_buffer_max ) {
		vk.stats.vertex_buffer_max = vk.cmd->vertex_buffer_offset;
	}
	if ( vk.stats.push_size > vk.stats.push_size_max ) {
		vk.stats.push_size_max = vk.stats.push_size;
	}

	vk.cmd->last_pipeline = VK_NULL_HANDLE;
	backEnd.screenMapDone = qfalse;

	// XR always uses the main multiview render pass: no screenmap in VR
	vk_begin_main_render_pass( qtrue );  // Clear framebuffer at start of frame

	// Reset dynamic buffers for new frame
	vk.cmd->uniform_read_offset = 0;
	vk.cmd->vertex_buffer_offset = 0;
	Com_Memset( vk.cmd->buf_offset, 0, sizeof( vk.cmd->buf_offset ) );
	Com_Memset( vk.cmd->vbo_offset, 0, sizeof( vk.cmd->vbo_offset ) );
	vk.cmd->curr_index_buffer = VK_NULL_HANDLE;
	vk.cmd->curr_index_offset = 0;
	vk.cmd->num_indexes = 0;
	vk.cmd->emissive_factor = 0.0f;

	Com_Memset( &vk.cmd->descriptor_set, 0, sizeof( vk.cmd->descriptor_set ) );
	vk.cmd->descriptor_set.start = ~0U;

	Com_Memset( &vk.cmd->scissor_rect, 0, sizeof( vk.cmd->scissor_rect ) );

	// the ring restarted at offset 0: last frame's cached eyeProj slot is gone
	vk.cmd->eyeproj_cache_valid = qfalse;

	// prime set 0 binding 1 so every dynamic-offset bind this frame has a
	// valid eyeproj_offset even before the first RB_BeginDrawingView() (e.g. 2D/menu draws)
	VK_PushEyeProj();

	vk.stats.push_size = 0;
}


void vk_end_frame( void )
{
	VkSubmitInfo submit_info;
	uint32_t colorIndex;
	float vsEyeProj[2][16], screenMV[16], floorMV[16];
	qboolean useVirtualScreen = qfalse;
	qboolean mirrorEnabled;
	extern cvar_t *vr_desktopMode;

	if ( vk.frame_count == 0 && !vk.recordingCommands )
		return;

	vk.frame_count = 0;

	// Handle geometry buffer resize if needed
	if ( vk.geometry_buffer_size_new ) {
		vk_resize_geometry_buffer();
		return;
	}

	colorIndex = vk.xr.colorIndex;
	mirrorEnabled = ( vr_desktopMode && vr_desktopMode->integer != 0 );

	// Query virtual screen state from VR layer (pull model)
	if ( ri.VR_GetVirtualScreenState != NULL ) {
		useVirtualScreen = ri.VR_GetVirtualScreenState( vsEyeProj, screenMV, floorMV );
	}

	// Post-processing for XR (bloom, gamma): fboActive always true in VR
	if ( vk.xr.initialized && vk.color_image != VK_NULL_HANDLE ) {
		vk.cmd->last_pipeline = VK_NULL_HANDLE; // do not restore clobbered descriptors in vk_bloom()

		// Apply bloom if enabled
		if ( r_bloom->integer ) {
			vk_bloom();
		}

		// Fallback deferred-corona site for frames without an RC_FINISHBLOOM
		// command; doneFlares makes this a no-op once the tr_backend hook ran.
		RB_RenderDeferredFlares();
		// Fallback replay of the in-world HUD sprite over the corona; hudDeferred
		// makes this a no-op once the tr_backend hook already replayed it.
		RB_DrawDeferredHud();

		// End current render pass before gamma/virtual screen operations
		vk_end_render_pass();

		// Transition FBO color to shader read for gamma pass
		// If we just ended post_bloom: FBO is already in SHADER_READ_ONLY_OPTIMAL (from finalLayout)
		// Otherwise (main, mainResume, or HUD after bloom): FBO is in COLOR_ATTACHMENT_OPTIMAL, needs transition
		if ( vk.renderPassIndex != RENDER_PASS_POST_BLOOM ) {
			record_image_layout_transition( vk.cmd->command_buffer,
				vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
		}

		vk.renderWidth = vk.xr.width;
		vk.renderHeight = vk.xr.height;
		vk.renderScaleX = vk.renderScaleY = 1.0f;

		// Gamma pass renders directly to XR swapchain (finalLayout = COLOR_ATTACHMENT_OPTIMAL)
		vk_begin_render_pass( vk.render_pass.gamma, vk.framebuffers.gamma[colorIndex],
			qfalse, vk.renderWidth, vk.renderHeight );
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			vk.gamma_pipeline );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			vk.pipeline_layout_post_process, 0, 1, &vk.color_descriptor, 0, NULL );
		qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
		vk_end_render_pass();

		if ( useVirtualScreen ) {
			vk_render_virtual_screen( vsEyeProj, screenMV, floorMV );
		}
	}

	// Only proceed if we're actually recording commands
	if ( !vk.recordingCommands ) {
		vk.renderPassIndex = RENDER_PASS_MAIN;
		return;
	}

	vk_gpu_time_end();

	VK_CHECK( qvkEndCommandBuffer( vk.cmd->command_buffer ) );
	vk.recordingCommands = qfalse;

	// Submit command buffer with fence
	// Only signal renderingCompleteSem if desktop mirror is enabled: it must be consumed each frame.
	// If we signal it but desktop mirror doesn't run (disabled, minimized, etc), the semaphore
	// remains signaled and the next vk_end_frame will cause a double-signal validation error.
	{
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.pNext = NULL;
		submit_info.waitSemaphoreCount = 0;
		submit_info.pWaitSemaphores = NULL;
		submit_info.pWaitDstStageMask = NULL;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &vk.cmd->command_buffer;

		if ( mirrorEnabled && !vk_is_window_minimized() && vk.swapchain != VK_NULL_HANDLE ) {
			submit_info.signalSemaphoreCount = 1;
			submit_info.pSignalSemaphores = &vk.renderingCompleteSem;
			vk.renderingCompleteSemSignaled = qtrue;
		} else {
			submit_info.signalSemaphoreCount = 0;
			submit_info.pSignalSemaphores = NULL;
			vk.renderingCompleteSemSignaled = qfalse;
		}

		VK_CHECK( qvkQueueSubmit( vk.queue, 1, &submit_info, vk.cmd->rendering_finished_fence ) );
		vk.cmd->waitForFence = qtrue;
	}

	backEnd.pc.msec = ri.Milliseconds() - backEnd.pc.msec;
	vk.renderPassIndex = RENDER_PASS_MAIN;
}


/*
Shared by vk_finish_frame / vk_discard_frame: end an interrupted render
pass and report whether a command buffer is open: the finish path submits
it, the discard path drops it.
*/
static qboolean vk_end_interrupted_pass( void )
{
	if ( vk.inRenderPass ) {
		qvkCmdEndRenderPass( vk.cmd->command_buffer );
		vk.inRenderPass = qfalse;
	}
	return vk.recordingCommands;
}


/*
==============================================================================

vk_finish_frame - Force-finish an interrupted frame

Called when we need to cleanly end an in-progress frame, e.g., during shutdown
or when starting a new frame before the previous one was properly ended.
This ensures render passes are ended and command buffers are properly submitted.

==============================================================================
*/
void vk_finish_frame( void )
{
	VkSubmitInfo submit_info;

	if ( vk_end_interrupted_pass() ) {
		vk_gpu_time_end();

		VK_CHECK( qvkEndCommandBuffer( vk.cmd->command_buffer ) );
		vk.recordingCommands = qfalse;

		// Submit with fence so we can wait for completion
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.pNext = NULL;
		submit_info.waitSemaphoreCount = 0;
		submit_info.pWaitSemaphores = NULL;
		submit_info.pWaitDstStageMask = NULL;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &vk.cmd->command_buffer;
		submit_info.signalSemaphoreCount = 0;
		submit_info.pSignalSemaphores = NULL;

		VK_CHECK( qvkQueueSubmit( vk.queue, 1, &submit_info, vk.cmd->rendering_finished_fence ) );
		vk.cmd->waitForFence = qtrue;
	}

	// Reset frame tracking
	vk.frame_count = 0;
	vk.renderPassIndex = RENDER_PASS_MAIN;
}


/*
==============================================================================

vk_discard_frame - Abandon an interrupted frame without submitting it

The frame is incomplete and its output is irrelevant at teardown; submitting
it would hand the queue draws referencing resources RE_Shutdown is about to
destroy, with attachments possibly left mid-pass (VUID-vkCmdDraw-None-09600).

==============================================================================
*/
void vk_discard_frame( void )
{
	if ( vk_end_interrupted_pass() ) {
		VK_CHECK( qvkEndCommandBuffer( vk.cmd->command_buffer ) );
		vk.recordingCommands = qfalse;
	}
	// never submitted, so there is nothing to read back
	vk.cmd->gpu_time_armed = qfalse;

	// Do not touch vk.cmd->waitForFence: it may still track this slot's
	// previous real submission, which vk_begin_frame must still wait on.
	vk.frame_count = 0;
	vk.renderPassIndex = RENDER_PASS_MAIN;
}


// Forward declarations for vk_recreate_desktop_swapchain
static void vk_destroy_desktop_mirror_resources( void );


/*
===============
vk_is_window_minimized

Check if the desktop window is currently minimized.
===============
*/
static qboolean vk_is_window_minimized( void )
{
	// com_minimized lives in the client; cached after first ri.Cvar_Get lookup.
	static cvar_t *cvMinimized = NULL;

	if ( !cvMinimized )
		cvMinimized = ri.Cvar_Get( "com_minimized", "0", 0 );

	return cvMinimized->integer != 0;
}


/*
===============
vk_recreate_desktop_swapchain

Recreate the desktop swapchain after minimize/restore or resize.
This only affects desktop mirror resources, not XR resources.
===============
*/
static void vk_recreate_desktop_swapchain( void )
{
	uint32_t i;

	ri.Printf( PRINT_DEVELOPER, "Recreating desktop swapchain...\n" );

	// Wait for all GPU work to complete
	qvkDeviceWaitIdle( vk.device );

	// 1. Destroy desktop mirror resources (framebuffers, pipelines, descriptors)
	vk_destroy_desktop_mirror_resources();

	// 2. Destroy desktop-specific sync primitives
	// Note: Can't use vk_destroy_sync_primitives() as it destroys XR sync too
	if ( vk.desktopAcquireSem != VK_NULL_HANDLE ) {
		qvkDestroySemaphore( vk.device, vk.desktopAcquireSem, NULL );
		vk.desktopAcquireSem = VK_NULL_HANDLE;
	}
	if ( vk.desktopBlitFence != VK_NULL_HANDLE ) {
		qvkDestroyFence( vk.device, vk.desktopBlitFence, NULL );
		vk.desktopBlitFence = VK_NULL_HANDLE;
	}
	for ( i = 0; i < MAX_SWAPCHAIN_IMAGES; i++ ) {
		if ( vk.desktopBlitComplete[i] != VK_NULL_HANDLE ) {
			qvkDestroySemaphore( vk.device, vk.desktopBlitComplete[i], NULL );
			vk.desktopBlitComplete[i] = VK_NULL_HANDLE;
		}
	}

	// 3. Destroy old swapchain
	vk_destroy_swapchain();

	// 4. Recreate swapchain (updates vk.desktopWidth/Height)
	vk_create_swapchain( vk.physical_device, vk.device, vk_surface,
						 vk.present_format, &vk.swapchain, qfalse );

	// 5. Recreate desktop sync primitives
	{
		VkFenceCreateInfo fence_desc;
		VkSemaphoreCreateInfo sem_desc;

		fence_desc.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fence_desc.pNext = NULL;
		fence_desc.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		sem_desc.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		sem_desc.pNext = NULL;
		sem_desc.flags = 0;
		VK_CHECK( qvkCreateSemaphore( vk.device, &sem_desc, NULL, &vk.desktopAcquireSem ) );
		VK_CHECK( qvkCreateFence( vk.device, &fence_desc, NULL, &vk.desktopBlitFence ) );

		for ( i = 0; i < MAX_SWAPCHAIN_IMAGES; i++ ) {
			VK_CHECK( qvkCreateSemaphore( vk.device, &sem_desc, NULL, &vk.desktopBlitComplete[i] ) );
		}
	}

	// 6. Desktop mirror resources will be lazy-initialized on next present
	//    via vk_create_desktop_mirror_resources()
}


/*
===============
vk_destroy_desktop_mirror_resources

Destroy desktop mirror shader-based rendering resources.
===============
*/
static void vk_destroy_desktop_mirror_resources( void )
{
	uint32_t i;

	if ( vk.desktopMirrorPipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.desktopMirrorPipeline, NULL );
		vk.desktopMirrorPipeline = VK_NULL_HANDLE;
	}
	if ( vk.desktopMirrorPipelineLayout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.desktopMirrorPipelineLayout, NULL );
		vk.desktopMirrorPipelineLayout = VK_NULL_HANDLE;
	}
	if ( vk.desktopMirrorFramebuffers ) {
		for ( i = 0; i < vk.swapchain_image_count; i++ ) {
			if ( vk.desktopMirrorFramebuffers[i] != VK_NULL_HANDLE ) {
				qvkDestroyFramebuffer( vk.device, vk.desktopMirrorFramebuffers[i], NULL );
			}
		}
		ri.Free( vk.desktopMirrorFramebuffers );
		vk.desktopMirrorFramebuffers = NULL;
	}
	if ( vk.desktopMirrorSwapViews ) {
		for ( i = 0; i < vk.swapchain_image_count; i++ ) {
			if ( vk.desktopMirrorSwapViews[i] != VK_NULL_HANDLE ) {
				qvkDestroyImageView( vk.device, vk.desktopMirrorSwapViews[i], NULL );
			}
		}
		ri.Free( vk.desktopMirrorSwapViews );
		vk.desktopMirrorSwapViews = NULL;
	}
	if ( vk.desktopMirrorRenderPass != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.desktopMirrorRenderPass, NULL );
		vk.desktopMirrorRenderPass = VK_NULL_HANDLE;
	}
	if ( vk.desktopMirrorView != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.desktopMirrorView, NULL );
		vk.desktopMirrorView = VK_NULL_HANDLE;
	}
	if ( vk.desktopMirrorImage != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, vk.desktopMirrorImage, NULL );
		vk.desktopMirrorImage = VK_NULL_HANDLE;
	}
	if ( vk.desktopMirrorMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.desktopMirrorMemory, NULL );
		vk.desktopMirrorMemory = VK_NULL_HANDLE;
	}
	// Note: vk.linearSampler is NOT destroyed here; it's a shared resource
	// created early in vk_initialize() and destroyed in vk_shutdown()
	// Descriptors are freed when pool is reset
	vk.desktopMirrorDescriptor = VK_NULL_HANDLE;
	for ( int eye = 0; eye < 2; eye++ ) {
		for ( int i = 0; i < MAX_SWAPCHAIN_IMAGES; i++ ) {
			vk.xr.colorEyeDescriptors[eye][i] = VK_NULL_HANDLE;
		}
	}
	for ( int i = 0; i < MAX_SWAPCHAIN_IMAGES; i++ ) {
		vk.xr.colorArrayDescriptors[i] = VK_NULL_HANDLE;
	}
}


/*
===============
vk_create_desktop_mirror_resources

Create desktop mirror shader-based rendering resources for gamma-correct output.
Creates intermediate texture (2-layer for stereo), render pass, framebuffers, and pipeline.
Called lazily on first use after XR resources are available.
===============
*/
static qboolean vk_create_desktop_mirror_resources( void )
{
	VkImageCreateInfo imageCI;
	VkMemoryRequirements memReqs;
	VkMemoryAllocateInfo allocInfo;
	VkImageViewCreateInfo viewCI;
	VkAttachmentDescription attachment;
	VkAttachmentReference colorRef;
	VkSubpassDescription subpass;
	VkRenderPassCreateInfo rpInfo;
	VkFramebufferCreateInfo fbInfo;
	VkDescriptorSetAllocateInfo descAllocInfo;
	VkDescriptorImageInfo imageInfo;
	VkWriteDescriptorSet descriptorWrite;
	VkPushConstantRange pushRange;
	VkPipelineLayoutCreateInfo layoutInfo;
	VkPipelineShaderStageCreateInfo shaderStages[2];
	VkPipelineVertexInputStateCreateInfo vertexInput;
	VkPipelineInputAssemblyStateCreateInfo inputAssembly;
	VkPipelineViewportStateCreateInfo viewportState;
	VkPipelineRasterizationStateCreateInfo rasterization;
	VkPipelineMultisampleStateCreateInfo multisample;
	VkPipelineColorBlendAttachmentState blendAttachment;
	VkPipelineColorBlendStateCreateInfo colorBlend;
	VkPipelineDynamicStateCreateInfo dynamicState;
	VkDynamicState dynamicStates[2];
	VkViewport viewport;
	VkRect2D scissor;
	VkGraphicsPipelineCreateInfo pipelineInfo;
	VkSpecializationMapEntry specEntries[2];
	VkSpecializationInfo specInfo;
	struct { float gamma; float obScale; } specData;
	uint32_t memoryType;
	uint32_t i;

	// Clean up any existing resources first
	vk_destroy_desktop_mirror_resources();

	// Need XR dimensions and desktop swapchain
	if ( vk.xr.width == 0 || vk.xr.height == 0 ) {
		return qfalse;
	}
	if ( vk.swapchain_image_count == 0 ) {
		return qfalse;
	}

	// 1. Create intermediate image (2-layer for stereo)
	Com_Memset( &imageCI, 0, sizeof( imageCI ) );
	imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCI.imageType = VK_IMAGE_TYPE_2D;
	imageCI.format = vk.color_format;
	imageCI.extent.width = vk.xr.width;
	imageCI.extent.height = vk.xr.height;
	imageCI.extent.depth = 1;
	imageCI.mipLevels = 1;
	imageCI.arrayLayers = 2;  // Stereo
	imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCI.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VK_CHECK( qvkCreateImage( vk.device, &imageCI, NULL, &vk.desktopMirrorImage ) );
	SET_OBJECT_NAME( vk.desktopMirrorImage, "Desktop mirror image", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );

	// 2. Allocate memory
	qvkGetImageMemoryRequirements( vk.device, vk.desktopMirrorImage, &memReqs );
	memoryType = find_memory_type( memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

	Com_Memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReqs.size;
	allocInfo.memoryTypeIndex = memoryType;

	VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.desktopMirrorMemory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, vk.desktopMirrorImage, vk.desktopMirrorMemory, 0 ) );

	// 3. Create UNORM image view (2D array, prevents sRGB conversion during sampling)
	Com_Memset( &viewCI, 0, sizeof( viewCI ) );
	viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCI.image = vk.desktopMirrorImage;
	viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	viewCI.format = vk_get_unorm_format( vk.color_format );
	viewCI.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCI.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCI.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCI.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewCI.subresourceRange.baseMipLevel = 0;
	viewCI.subresourceRange.levelCount = 1;
	viewCI.subresourceRange.baseArrayLayer = 0;
	viewCI.subresourceRange.layerCount = 2;

	VK_CHECK( qvkCreateImageView( vk.device, &viewCI, NULL, &vk.desktopMirrorView ) );
	SET_OBJECT_NAME( vk.desktopMirrorView, "Desktop mirror view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );

	// 4. Create descriptor set (sampler is created early in vk_initialize)
	Com_Memset( &descAllocInfo, 0, sizeof( descAllocInfo ) );
	descAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descAllocInfo.descriptorPool = vk.descriptor_pool;
	descAllocInfo.descriptorSetCount = 1;
	descAllocInfo.pSetLayouts = &vk.set_layout_sampler;

	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &descAllocInfo, &vk.desktopMirrorDescriptor ) );

	Com_Memset( &imageInfo, 0, sizeof( imageInfo ) );
	imageInfo.sampler = vk.linearSampler;
	imageInfo.imageView = vk.desktopMirrorView;
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	Com_Memset( &descriptorWrite, 0, sizeof( descriptorWrite ) );
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = vk.desktopMirrorDescriptor;
	descriptorWrite.dstBinding = 0;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pImageInfo = &imageInfo;

	qvkUpdateDescriptorSets( vk.device, 1, &descriptorWrite, 0, NULL );

	// Note: Per-eye descriptors for XR swapchain sampling (menuStyle=1 "VR view")
	// are allocated in vk_create_xr_image_views() when XR swapchains are available,
	// because we need the image views to pre-bind the descriptors.

	// 5. Create render pass (non-multiview, for desktop swapchain)
	// In HDR the swapchain is scRGB FP16: use it raw (no sRGB view, no auto-encode).
	// In SDR keep the sRGB view so the gamma-corrected output is encoded on write.
	VkFormat desktopMirrorFormat = vk.hdrActive
		? vk.present_format.format
		: vk_get_srgb_format( vk.present_format.format );

	Com_Memset( &attachment, 0, sizeof( attachment ) );
	attachment.format = desktopMirrorFormat;
	attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	Com_Memset( &colorRef, 0, sizeof( colorRef ) );
	colorRef.attachment = 0;
	colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	Com_Memset( &subpass, 0, sizeof( subpass ) );
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef;

	Com_Memset( &rpInfo, 0, sizeof( rpInfo ) );
	rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	rpInfo.attachmentCount = 1;
	rpInfo.pAttachments = &attachment;
	rpInfo.subpassCount = 1;
	rpInfo.pSubpasses = &subpass;

	VK_CHECK( qvkCreateRenderPass( vk.device, &rpInfo, NULL, &vk.desktopMirrorRenderPass ) );
	SET_OBJECT_NAME( vk.desktopMirrorRenderPass, "Desktop mirror render pass", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

	// 7. Create image views and framebuffers for each desktop swapchain image
	vk.desktopMirrorSwapViews = ri.Malloc( vk.swapchain_image_count * sizeof( VkImageView ) );
	vk.desktopMirrorFramebuffers = ri.Malloc( vk.swapchain_image_count * sizeof( VkFramebuffer ) );
	Com_Memset( vk.desktopMirrorSwapViews, 0, vk.swapchain_image_count * sizeof( VkImageView ) );
	Com_Memset( vk.desktopMirrorFramebuffers, 0, vk.swapchain_image_count * sizeof( VkFramebuffer ) );

	for ( i = 0; i < vk.swapchain_image_count; i++ ) {
		Com_Memset( &viewCI, 0, sizeof( viewCI ) );
		viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCI.image = vk.swapchain_images[i];
		viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewCI.format = desktopMirrorFormat;  // FP16 raw when HDR active, sRGB view otherwise
		viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewCI.subresourceRange.baseMipLevel = 0;
		viewCI.subresourceRange.levelCount = 1;
		viewCI.subresourceRange.baseArrayLayer = 0;
		viewCI.subresourceRange.layerCount = 1;

		VK_CHECK( qvkCreateImageView( vk.device, &viewCI, NULL, &vk.desktopMirrorSwapViews[i] ) );

		Com_Memset( &fbInfo, 0, sizeof( fbInfo ) );
		fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fbInfo.renderPass = vk.desktopMirrorRenderPass;
		fbInfo.attachmentCount = 1;
		fbInfo.pAttachments = &vk.desktopMirrorSwapViews[i];
		fbInfo.width = vk.desktopWidth;
		fbInfo.height = vk.desktopHeight;
		fbInfo.layers = 1;

		VK_CHECK( qvkCreateFramebuffer( vk.device, &fbInfo, NULL, &vk.desktopMirrorFramebuffers[i] ) );
	}

	// 8. Create pipeline layout
	Com_Memset( &pushRange, 0, sizeof( pushRange ) );
	pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	pushRange.offset = 0;
	pushRange.size = 17 * sizeof(float);  // offset(8) + scale(8) + texCrop(8) + eyeLayer(4) + hdr fields(40)

	// set 0 = SDR source (eye swapchain / virtual screen); set 1 = scene base
	// (color_descriptor); set 2 = emissive (emissive_descriptor) for HDR reconstruction.
	VkDescriptorSetLayout mirrorSetLayouts[3] = {
		vk.set_layout_sampler,
		vk.set_layout_sampler,
		vk.set_layout_sampler,
	};

	Com_Memset( &layoutInfo, 0, sizeof( layoutInfo ) );
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.setLayoutCount = 3;
	layoutInfo.pSetLayouts = mirrorSetLayouts;
	layoutInfo.pushConstantRangeCount = 1;
	layoutInfo.pPushConstantRanges = &pushRange;

	VK_CHECK( qvkCreatePipelineLayout( vk.device, &layoutInfo, NULL, &vk.desktopMirrorPipelineLayout ) );
	SET_OBJECT_NAME( vk.desktopMirrorPipelineLayout, "Desktop mirror pipeline layout", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );

	// 9. Create pipeline: sRGB view auto-decodes on read, sRGB framebuffer auto-encodes on write
	specData.gamma = 1.0f;
	specData.obScale = 1.0f;

	specEntries[0].constantID = 0;
	specEntries[0].offset = 0;
	specEntries[0].size = sizeof( float );
	specEntries[1].constantID = 1;
	specEntries[1].offset = sizeof( float );
	specEntries[1].size = sizeof( float );

	Com_Memset( &specInfo, 0, sizeof( specInfo ) );
	specInfo.mapEntryCount = 2;
	specInfo.pMapEntries = specEntries;
	specInfo.dataSize = sizeof( specData );
	specInfo.pData = &specData;

	// Shader stages
	Com_Memset( shaderStages, 0, sizeof( shaderStages ) );
	shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaderStages[0].module = vk.modules.desktopmirror_vs;
	shaderStages[0].pName = "main";

	shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderStages[1].module = vk.modules.desktopmirror_fs;
	shaderStages[1].pName = "main";
	shaderStages[1].pSpecializationInfo = &specInfo;

	// Vertex input (no vertex attributes: fullscreen triangle from gl_VertexIndex)
	Com_Memset( &vertexInput, 0, sizeof( vertexInput ) );
	vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	// Input assembly
	Com_Memset( &inputAssembly, 0, sizeof( inputAssembly ) );
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	// Viewport and scissor (dynamic)
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = (float)vk.desktopWidth;
	viewport.height = (float)vk.desktopHeight;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = vk.desktopWidth;
	scissor.extent.height = vk.desktopHeight;

	Com_Memset( &viewportState, 0, sizeof( viewportState ) );
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	// Rasterization
	Com_Memset( &rasterization, 0, sizeof( rasterization ) );
	rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization.polygonMode = VK_POLYGON_MODE_FILL;
	rasterization.cullMode = VK_CULL_MODE_NONE;
	rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterization.lineWidth = 1.0f;

	// Multisample
	Com_Memset( &multisample, 0, sizeof( multisample ) );
	multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	// Color blend
	Com_Memset( &blendAttachment, 0, sizeof( blendAttachment ) );
	blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	Com_Memset( &colorBlend, 0, sizeof( colorBlend ) );
	colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlend.attachmentCount = 1;
	colorBlend.pAttachments = &blendAttachment;

	// Dynamic state
	dynamicStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
	dynamicStates[1] = VK_DYNAMIC_STATE_SCISSOR;

	Com_Memset( &dynamicState, 0, sizeof( dynamicState ) );
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates = dynamicStates;

	// Create pipeline
	Com_Memset( &pipelineInfo, 0, sizeof( pipelineInfo ) );
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInput;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterization;
	pipelineInfo.pMultisampleState = &multisample;
	pipelineInfo.pColorBlendState = &colorBlend;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = vk.desktopMirrorPipelineLayout;
	pipelineInfo.renderPass = vk.desktopMirrorRenderPass;
	pipelineInfo.subpass = 0;

	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, vk.pipelineCache, 1, &pipelineInfo, NULL, &vk.desktopMirrorPipeline ) );
	SET_OBJECT_NAME( vk.desktopMirrorPipeline, "Desktop mirror pipeline", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );

	// 10. Initialize image layout
	{
		VkCommandBuffer cmdBuf = begin_command_buffer();
		record_image_layout_transition( cmdBuf, vk.desktopMirrorImage, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
		end_command_buffer( cmdBuf, "desktop mirror layout init" );
	}

	return qtrue;
}


/*
===============
vk_present_desktop_mirror

Acquires desktop swapchain image, blits XR content to it, and presents.
This is called from RE_SwapDesktopWindow in tr_init.c.
===============
*/
/*
 * Drain the renderingCompleteSem if it was signaled but not consumed.
 * Vulkan semaphores must be consumed before they can be signaled again,
 * so we submit an empty command buffer that waits on it to prevent a
 * double-signal validation error on the next frame.
 */
static void vk_drain_rendering_semaphore( void )
{
	if ( vk.renderingCompleteSemSignaled ) {
		VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		VkSubmitInfo submit_info;

		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.pNext = NULL;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &vk.renderingCompleteSem;
		submit_info.pWaitDstStageMask = &waitStage;
		submit_info.commandBufferCount = 0;
		submit_info.pCommandBuffers = NULL;
		submit_info.signalSemaphoreCount = 0;
		submit_info.pSignalSemaphores = NULL;

		qvkQueueSubmit( vk.queue, 1, &submit_info, VK_NULL_HANDLE );
		vk.renderingCompleteSemSignaled = qfalse;
	}
}


static float vk_hdr_paper_white( void )
{
	if ( r_hdrPaperWhite->value > 0.0f )
		return r_hdrPaperWhite->value;
	// auto: BT.2408 HLG reference white at the panel peak (r_hdrPeak)
	double sysgamma = 1.2 + 0.42 * log10( r_hdrPeak->value / 1000.0 );
	return (float)( r_hdrPeak->value * pow( 0.264964, sysgamma ) );
}


void vk_present_desktop_mirror( void )
{
	extern cvar_t *vr_desktopMode;
	extern cvar_t *vr_desktopContentType;
	extern cvar_t *vr_desktopContentFit;
	extern cvar_t *vr_desktopMenuStyle;
	static qboolean desktopMirrorWasActive = qfalse;
	VkCommandBufferBeginInfo begin_info;
	VkSubmitInfo submit_info;
	VkPresentInfoKHR presentInfo;
	VkResult result;
	uint32_t desktopImageIndex;
	qboolean mirrorActive;

	// Track mirror state to handle semaphore synchronization when toggling
	mirrorActive = ( vr_desktopMode && vr_desktopMode->integer != 0 );

	desktopMirrorWasActive = mirrorActive;

	// Early out if desktop mirroring is disabled
	if ( !mirrorActive ) {
		return;
	}

	// Skip when window is minimized: no point trying to present
	if ( vk_is_window_minimized() ) {
		return;
	}

	// Check if Vulkan is ready
	if ( !vk.active || vk.swapchain == VK_NULL_HANDLE ) {
		return;
	}

	// Check if XR resources are ready
	if ( !vk.xr.initialized || !vk.xr.colorInfo || !vk.xr.colorInfo->images ) {
		return;
	}

	// Second command recorder outside vk_begin_frame, so its descriptorsReady
	// gate doesn't cover this path. Binds vk.emissive_descriptor (NULL during
	// the dead-descriptor window) too, so gate here; spectator holds the last
	// image meanwhile.
	if ( !vk.descriptorsReady ) {
		return;
	}

	// Never block the VR frame on the previous blit: if it hasn't drained yet,
	// skip this update and let the spectator hold the previous frame. The fence
	// is created signaled, so the first call passes through.
	if ( qvkGetFenceStatus( vk.device, vk.desktopBlitFence ) != VK_SUCCESS ) {
		vk_drain_rendering_semaphore();
		return;
	}

	// Non-blocking acquire so the VR frame is never stalled by desktop vsync.
	// If no image is available, we skip: spectator sees the previous frame held.
	result = qvkAcquireNextImageKHR( vk.device, vk.swapchain, 0,
		vk.desktopAcquireSem, VK_NULL_HANDLE, &desktopImageIndex );

	if ( result == VK_ERROR_OUT_OF_DATE_KHR ) {
		// Swapchain needs recreation (window resized, minimized then restored, etc.)
		vk_recreate_desktop_swapchain();
		vk_drain_rendering_semaphore();
		return;
	}

	if ( result == VK_NOT_READY || result == VK_TIMEOUT ) {
		// No desktop swapchain image available: skip this frame's mirror
		vk_drain_rendering_semaphore();
		return;
	}

	if ( result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR ) {
		ri.Printf( PRINT_WARNING, "vk_present_desktop_mirror: Failed to acquire desktop image: %s\n", vk_result_string( result ) );
		vk_drain_rendering_semaphore();
		return;
	}

	qvkResetFences( vk.device, 1, &vk.desktopBlitFence );

	// Begin command buffer
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.pNext = NULL;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	begin_info.pInheritanceInfo = NULL;

	qvkResetCommandBuffer( vk.desktopBlitCmd, 0 );
	VK_CHECK( qvkBeginCommandBuffer( vk.desktopBlitCmd, &begin_info ) );

	// Get destination image (desktop swapchain)
	VkImage dstImage = vk.swapchain_images[desktopImageIndex];

	// Get content type (0=left eye, 1=right eye, 2=both eyes)
	int contentType = vr_desktopContentType ? vr_desktopContentType->integer : 0;
	// Get content fit (0=fit/contain with letterbox, 1=fill/crop)
	int contentFit = vr_desktopContentFit ? vr_desktopContentFit->integer : 1;
	// Get menu style (0=desktop/flat view, 1=VR view)
	int menuStyle = vr_desktopMenuStyle ? vr_desktopMenuStyle->integer : 0;
	// The HDR calibration box only renders on the Desktop-view mirror branch (the
	// box is a present-time scRGB overlay, and the VR-view branch samples the SDR
	// XR swapchain). Force Desktop view while the calibration screen is up so the
	// pattern shows regardless of the user's mirror preference; reverts on close.
	if ( r_hdrCalibrate->integer ) {
		menuStyle = 0;
	}

	// Source dimensions (XR resolution)
	int srcWidth = vk.xr.width;
	int srcHeight = vk.xr.height;

	// Destination dimensions (desktop window)
	int dstWidth = vk.desktopWidth;
	int dstHeight = vk.desktopHeight;

	// Track destination image layout for final transition
	// Start with UNDEFINED since after vkAcquireNextImageKHR the layout is undefined
	// and we're about to overwrite the contents anyway
	VkImageLayout dstLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	// Transition destination to TRANSFER_DST_OPTIMAL
	record_image_layout_transition( vk.desktopBlitCmd, dstImage,
		VK_IMAGE_ASPECT_COLOR_BIT,
		dstLayout,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );
	dstLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

	// Clear destination to black (for letterbox bars)
	{
		VkClearColorValue clearColor = { { 0.0f, 0.0f, 0.0f, 1.0f } };
		VkImageSubresourceRange range;
		range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		range.baseMipLevel = 0;
		range.levelCount = 1;
		range.baseArrayLayer = 0;
		range.layerCount = 1;
		qvkCmdClearColorImage( vk.desktopBlitCmd, dstImage,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range );
	}

	// Virtual screen mode with "VR view" (menuStyle=1): sample from XR swapchain
	// This shows what the headset sees: floor grid + floating virtual screen
	if ( vr.virtual_screen && menuStyle == 1 ) {
		uint32_t colorIndex = vk.xr.colorIndex;

		// Lazy-init desktop mirror resources
		if ( vk.desktopMirrorPipeline == VK_NULL_HANDLE ) {
			vk_create_desktop_mirror_resources();
			if ( vk.desktopMirrorPipeline == VK_NULL_HANDLE ) {
				goto finish_desktop_mirror;
			}
		}

		// Check if we have the per-eye views for this swapchain image
		if ( colorIndex >= MAX_SWAPCHAIN_IMAGES ||
			 vk.xr.colorEyeViews[0][colorIndex] == VK_NULL_HANDLE ) {
			goto finish_desktop_mirror;
		}

		// Lazy-init descriptors if they weren't created during vk_create_xr_image_views
		// (can happen if XR resources were created before vk_initialize)
		if ( vk.xr.colorArrayDescriptors[colorIndex] == VK_NULL_HANDLE ) {
			if ( vk.linearSampler == VK_NULL_HANDLE || vk.descriptor_pool == VK_NULL_HANDLE ||
			     vk.set_layout_sampler == VK_NULL_HANDLE ) {
				goto finish_desktop_mirror;
			}

			VkDescriptorSetAllocateInfo descAllocInfo;
			VkDescriptorImageInfo imageInfo;
			VkWriteDescriptorSet descriptorWrite;

			Com_Memset( &descAllocInfo, 0, sizeof( descAllocInfo ) );
			descAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			descAllocInfo.descriptorPool = vk.descriptor_pool;
			descAllocInfo.descriptorSetCount = 1;
			descAllocInfo.pSetLayouts = &vk.set_layout_sampler;

			Com_Memset( &imageInfo, 0, sizeof( imageInfo ) );
			imageInfo.sampler = vk.linearSampler;
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			Com_Memset( &descriptorWrite, 0, sizeof( descriptorWrite ) );
			descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrite.dstBinding = 0;
			descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrite.descriptorCount = 1;
			descriptorWrite.pImageInfo = &imageInfo;

			// Create all per-eye descriptors for all swapchain images
			for ( uint32_t i = 0; i < vk.xr.colorInfo->imageCount && i < MAX_SWAPCHAIN_IMAGES; i++ ) {
				for ( int eye = 0; eye < 2; eye++ ) {
					if ( vk.xr.colorEyeDescriptors[eye][i] == VK_NULL_HANDLE &&
					     vk.xr.colorEyeViews[eye][i] != VK_NULL_HANDLE ) {
						VK_CHECK( qvkAllocateDescriptorSets( vk.device, &descAllocInfo, &vk.xr.colorEyeDescriptors[eye][i] ) );
						imageInfo.imageView = vk.xr.colorEyeViews[eye][i];
						descriptorWrite.dstSet = vk.xr.colorEyeDescriptors[eye][i];
						qvkUpdateDescriptorSets( vk.device, 1, &descriptorWrite, 0, NULL );
					}
				}
				// Create 2D_ARRAY descriptor for desktop mirror shader (sampler2DArray)
				if ( vk.xr.colorArrayDescriptors[i] == VK_NULL_HANDLE &&
				     vk.xr.colorViews[i] != VK_NULL_HANDLE ) {
					VK_CHECK( qvkAllocateDescriptorSets( vk.device, &descAllocInfo, &vk.xr.colorArrayDescriptors[i] ) );
					imageInfo.imageView = vk.xr.colorViews[i];  // 2D_ARRAY view
					descriptorWrite.dstSet = vk.xr.colorArrayDescriptors[i];
					qvkUpdateDescriptorSets( vk.device, 1, &descriptorWrite, 0, NULL );
				}
			}

			ri.Printf( PRINT_ALL, "XR per-eye descriptors lazy-initialized\n" );
		}

		// Transition XR swapchain to SHADER_READ_ONLY for sampling
		// The XR swapchain is in COLOR_ATTACHMENT_OPTIMAL after vk_render_virtual_screen
		record_image_layout_transition( vk.desktopBlitCmd, vk.xr.colorInfo->images[colorIndex],
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );

		// Transition desktop swapchain to COLOR_ATTACHMENT_OPTIMAL for rendering
		record_image_layout_transition( vk.desktopBlitCmd, dstImage,
			VK_IMAGE_ASPECT_COLOR_BIT,
			dstLayout,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, 0 );
		dstLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// Begin render pass
		{
			VkRenderPassBeginInfo rpBegin;
			VkClearValue clearValue;

			clearValue.color.float32[0] = 0.0f;
			clearValue.color.float32[1] = 0.0f;
			clearValue.color.float32[2] = 0.0f;
			clearValue.color.float32[3] = 1.0f;

			rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			rpBegin.pNext = NULL;
			rpBegin.renderPass = vk.desktopMirrorRenderPass;
			rpBegin.framebuffer = vk.desktopMirrorFramebuffers[desktopImageIndex];
			rpBegin.renderArea.offset.x = 0;
			rpBegin.renderArea.offset.y = 0;
			rpBegin.renderArea.extent.width = dstWidth;
			rpBegin.renderArea.extent.height = dstHeight;
			rpBegin.clearValueCount = 1;
			rpBegin.pClearValues = &clearValue;

			qvkCmdBeginRenderPass( vk.desktopBlitCmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE );
		}

		qvkCmdBindPipeline( vk.desktopBlitCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.desktopMirrorPipeline );

		// Set viewport/scissor
		{
			VkViewport viewport;
			VkRect2D scissor;

			viewport.x = 0;
			viewport.y = 0;
			viewport.width = (float)dstWidth;
			viewport.height = (float)dstHeight;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			qvkCmdSetViewport( vk.desktopBlitCmd, 0, 1, &viewport );

			scissor.offset.x = 0;
			scissor.offset.y = 0;
			scissor.extent.width = dstWidth;
			scissor.extent.height = dstHeight;
			qvkCmdSetScissor( vk.desktopBlitCmd, 0, 1, &scissor );
		}

		// Render based on content type and content fit settings
		// Similar logic to the normal mode else block below
		{
			struct {
				float offsetX, offsetY;
				float scaleX, scaleY;
				float texCropX, texCropY;
				int32_t eyeLayer;
				int32_t hdrMode;
				float hdrGamma;
				float hdrObScale;
				float paperWhite;
				float hdrPeak;
				float hdrHighlight;
				float hdrSaturation;
				float hdrSaturationFull;
				float hdrSoftKnee;
				int32_t hdrCalibrate;
			} pushConstants;

			float srcAspect = (float)srcWidth / (float)srcHeight;

			// Calculate per-part dimensions for both eyes mode
			int parts = ( contentType == 2 ) ? 2 : 1;
			float partWidth = (float)dstWidth / parts;
			float partAspect = partWidth / (float)dstHeight;

			// quadScale: controls the rendered quad size (for letterboxing)
			// texCrop: controls texture sampling region (for fill/crop)
			float quadScaleX = 1.0f, quadScaleY = 1.0f;
			float texCropX = 1.0f, texCropY = 1.0f;

			if ( contentFit == 1 ) {
				// Fill/crop mode: quad fills viewport, texture is cropped
				quadScaleX = 1.0f;
				quadScaleY = 1.0f;
				if ( srcAspect > partAspect ) {
					// Source wider - crop left/right
					texCropX = partAspect / srcAspect;
					texCropY = 1.0f;
				} else {
					// Source taller - crop top/bottom
					texCropX = 1.0f;
					texCropY = srcAspect / partAspect;
				}
			} else {
				// Fit/contain mode: texture is full, quad may be letterboxed
				texCropX = 1.0f;
				texCropY = 1.0f;
				if ( srcAspect > partAspect ) {
					// Source wider - letterbox top/bottom
					quadScaleX = 1.0f;
					quadScaleY = partAspect / srcAspect;
				} else {
					// Source taller - pillarbox left/right
					quadScaleX = srcAspect / partAspect;
					quadScaleY = 1.0f;
				}
			}

			if ( contentType == 2 ) {
				// Both eyes side by side
				for ( int eye = 0; eye < 2; eye++ ) {
					VkViewport partViewport;
					partViewport.x = eye * partWidth;
					partViewport.y = 0;
					partViewport.width = partWidth;
					partViewport.height = (float)dstHeight;
					partViewport.minDepth = 0.0f;
					partViewport.maxDepth = 1.0f;
					qvkCmdSetViewport( vk.desktopBlitCmd, 0, 1, &partViewport );

					VkRect2D partScissor;
					partScissor.offset.x = (int32_t)(eye * partWidth);
					partScissor.offset.y = 0;
					partScissor.extent.width = (uint32_t)partWidth;
					partScissor.extent.height = dstHeight;
					qvkCmdSetScissor( vk.desktopBlitCmd, 0, 1, &partScissor );

					// sets 1/2 bound to satisfy the layout; unused on this SDR/flat path.
					{
						VkDescriptorSet mirrorSets[3] = {
							vk.xr.colorArrayDescriptors[colorIndex],
							vk.color_descriptor,
							vk.emissive_descriptor,
						};
						qvkCmdBindDescriptorSets( vk.desktopBlitCmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
							vk.desktopMirrorPipelineLayout, 0, 3, mirrorSets, 0, NULL );
					}

					pushConstants.offsetX = 0.0f;
					pushConstants.offsetY = 0.0f;
					pushConstants.scaleX = quadScaleX;
					pushConstants.scaleY = quadScaleY;
					pushConstants.texCropX = texCropX;
					pushConstants.texCropY = texCropY;
					pushConstants.eyeLayer = eye;  // Select layer in 2D_ARRAY
					pushConstants.hdrMode = vk.hdrActive ? 2 : 0;
					pushConstants.hdrGamma = 1.0f;
					pushConstants.hdrObScale = 1.0f;
					pushConstants.paperWhite = vk.hdrActive ? vk_hdr_paper_white() : 200.0f;
					pushConstants.hdrPeak = 1000.0f;
					pushConstants.hdrHighlight = 1.0f;
					pushConstants.hdrCalibrate = 0;  // calibration pattern is drawn only on the desktop-view mirror

					qvkCmdPushConstants( vk.desktopBlitCmd, vk.desktopMirrorPipelineLayout,
						VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( pushConstants ), &pushConstants );
					qvkCmdDraw( vk.desktopBlitCmd, 3, 1, 0, 0 );
				}
			} else {
				// Single eye (left=0, right=1)
				int eye = ( contentType == 1 ) ? 1 : 0;

				// sets 1/2 bound to satisfy the layout; unused on this SDR/flat path.
				{
					VkDescriptorSet mirrorSets[3] = {
						vk.xr.colorArrayDescriptors[colorIndex],
						vk.color_descriptor,
						vk.emissive_descriptor,
					};
					qvkCmdBindDescriptorSets( vk.desktopBlitCmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
						vk.desktopMirrorPipelineLayout, 0, 3, mirrorSets, 0, NULL );
				}

				pushConstants.offsetX = 0.0f;
				pushConstants.offsetY = 0.0f;
				pushConstants.scaleX = quadScaleX;
				pushConstants.scaleY = quadScaleY;
				pushConstants.texCropX = texCropX;
				pushConstants.texCropY = texCropY;
				pushConstants.eyeLayer = eye;  // Select layer in 2D_ARRAY
				pushConstants.hdrMode = vk.hdrActive ? 2 : 0;
				pushConstants.hdrGamma = 1.0f;
				pushConstants.hdrObScale = 1.0f;
				pushConstants.paperWhite = vk.hdrActive ? vk_hdr_paper_white() : 200.0f;
				pushConstants.hdrPeak = 1000.0f;
				pushConstants.hdrHighlight = 1.0f;
				pushConstants.hdrCalibrate = 0;  // calibration pattern is drawn only on the desktop-view mirror

				qvkCmdPushConstants( vk.desktopBlitCmd, vk.desktopMirrorPipelineLayout,
					VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( pushConstants ), &pushConstants );
				qvkCmdDraw( vk.desktopBlitCmd, 3, 1, 0, 0 );
			}
		}

		qvkCmdEndRenderPass( vk.desktopBlitCmd );
		dstLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		// Transition XR swapchain back to COLOR_ATTACHMENT_OPTIMAL for next frame
		record_image_layout_transition( vk.desktopBlitCmd, vk.xr.colorInfo->images[colorIndex],
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, 0 );

	} else if ( vr.virtual_screen && vk.xr.virtualScreenImage != VK_NULL_HANDLE && menuStyle == 0 ) {
		// Virtual screen mode with "Desktop view" (menuStyle=0): shader-based rendering with 4:3 aspect ratio
		// Lazy-init desktop mirror resources (needed for render pass, framebuffers, pipeline)
		if ( vk.desktopMirrorPipeline == VK_NULL_HANDLE ) {
			vk_create_desktop_mirror_resources();
			if ( vk.desktopMirrorPipeline == VK_NULL_HANDLE ) {
				goto finish_desktop_mirror;
			}
		}

		// Check if we have the virtual screen mirror descriptor
		if ( vk.xr.virtualScreenMirrorDescriptor == VK_NULL_HANDLE ) {
			goto finish_desktop_mirror;
		}

		// Transition desktop swapchain to COLOR_ATTACHMENT_OPTIMAL for rendering
		record_image_layout_transition( vk.desktopBlitCmd, dstImage,
			VK_IMAGE_ASPECT_COLOR_BIT,
			dstLayout,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, 0 );
		dstLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// Begin render pass
		{
			VkRenderPassBeginInfo rpBegin;
			VkClearValue clearValue;

			clearValue.color.float32[0] = 0.0f;
			clearValue.color.float32[1] = 0.0f;
			clearValue.color.float32[2] = 0.0f;
			clearValue.color.float32[3] = 1.0f;

			rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			rpBegin.pNext = NULL;
			rpBegin.renderPass = vk.desktopMirrorRenderPass;
			rpBegin.framebuffer = vk.desktopMirrorFramebuffers[desktopImageIndex];
			rpBegin.renderArea.offset.x = 0;
			rpBegin.renderArea.offset.y = 0;
			rpBegin.renderArea.extent.width = dstWidth;
			rpBegin.renderArea.extent.height = dstHeight;
			rpBegin.clearValueCount = 1;
			rpBegin.pClearValues = &clearValue;

			qvkCmdBeginRenderPass( vk.desktopBlitCmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE );
		}

		// sets 1/2 bound to satisfy the layout; unused on this SDR/flat path.
		qvkCmdBindPipeline( vk.desktopBlitCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.desktopMirrorPipeline );
		{
			VkDescriptorSet mirrorSets[3] = {
				vk.xr.virtualScreenMirrorDescriptor,
				vk.color_descriptor,
				vk.emissive_descriptor,
			};
			qvkCmdBindDescriptorSets( vk.desktopBlitCmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
				vk.desktopMirrorPipelineLayout, 0, 3, mirrorSets, 0, NULL );
		}

		// Set viewport/scissor
		{
			VkViewport viewport;
			VkRect2D scissor;

			viewport.x = 0;
			viewport.y = 0;
			viewport.width = (float)dstWidth;
			viewport.height = (float)dstHeight;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			qvkCmdSetViewport( vk.desktopBlitCmd, 0, 1, &viewport );

			scissor.offset.x = 0;
			scissor.offset.y = 0;
			scissor.extent.width = dstWidth;
			scissor.extent.height = dstHeight;
			qvkCmdSetScissor( vk.desktopBlitCmd, 0, 1, &scissor );
		}

		// Calculate 4:3 letterbox/pillarbox parameters
		{
			struct {
				float offsetX, offsetY;
				float scaleX, scaleY;
				float texCropX, texCropY;
				int32_t eyeLayer;
				int32_t hdrMode;
				float hdrGamma;
				float hdrObScale;
				float paperWhite;
				float hdrPeak;
				float hdrHighlight;
				float hdrSaturation;
				float hdrSaturationFull;
				float hdrSoftKnee;
				int32_t hdrCalibrate;
			} pushConstants;

			const float targetAspect = 4.0f / 3.0f;
			const float windowAspect = (float)dstWidth / (float)dstHeight;
			float fitScaleX, fitScaleY;

			if ( windowAspect > targetAspect ) {
				// Window wider than 4:3 - pillarbox
				fitScaleX = targetAspect / windowAspect;
				fitScaleY = 1.0f;
			} else {
				// Window taller than 4:3 - letterbox
				fitScaleX = 1.0f;
				fitScaleY = windowAspect / targetAspect;
			}

			pushConstants.offsetX = 0.0f;
			pushConstants.offsetY = 0.0f;
			pushConstants.scaleX = fitScaleX;
			pushConstants.scaleY = fitScaleY;
			pushConstants.texCropX = 1.0f;  // Virtual screen always shows full texture
			pushConstants.texCropY = 1.0f;
			pushConstants.eyeLayer = 0;  // Virtual screen is single layer, sample layer 0
			pushConstants.hdrMode = vk.hdrActive ? 2 : 0;
			pushConstants.hdrGamma = 1.0f;
			pushConstants.hdrObScale = 1.0f;
			pushConstants.paperWhite = vk.hdrActive ? vk_hdr_paper_white() : 200.0f;
			// Mode 2 does not use hdrPeak, but the calibration inner box does: feed it
			// the live slider so "raise Peak until the inner edge vanishes" actually works.
			pushConstants.hdrPeak = vk.hdrActive ? r_hdrPeak->value : 1000.0f;
			pushConstants.hdrHighlight = 1.0f;
			pushConstants.hdrCalibrate = ( vk.hdrActive && r_hdrCalibrate->integer ) ? 1 : 0;

			qvkCmdPushConstants( vk.desktopBlitCmd, vk.desktopMirrorPipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( pushConstants ), &pushConstants );
			qvkCmdDraw( vk.desktopBlitCmd, 3, 1, 0, 0 );
		}

		qvkCmdEndRenderPass( vk.desktopBlitCmd );
		// Render pass finalLayout transitions to PRESENT_SRC_KHR
		dstLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	} else {
		// Normal mode: sample from XR swapchain (still ours, runs before xrReleaseSwapchainImage)
		uint32_t xrColorIndex = vk.xr.colorIndex;

		// Lazy-init desktop mirror resources
		if ( vk.desktopMirrorPipeline == VK_NULL_HANDLE ) {
			vk_create_desktop_mirror_resources();
			if ( vk.desktopMirrorPipeline == VK_NULL_HANDLE ) {
				// Failed to create resources, fall back to simple clear
				goto finish_desktop_mirror;
			}
		}

		// Check if we have descriptors for this swapchain image (HDR skips this; uses scene FBO instead)
		if ( !vk.hdrActive && ( xrColorIndex >= MAX_SWAPCHAIN_IMAGES ||
		     vk.xr.colorArrayDescriptors[xrColorIndex] == VK_NULL_HANDLE ) ) {
			goto finish_desktop_mirror;
		}

		if ( !vk.hdrActive ) {
			// SDR: source is the XR swapchain image; transition it for sampling.
			record_image_layout_transition( vk.desktopBlitCmd, vk.xr.colorInfo->images[xrColorIndex],
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
		}

		// Transition desktop swapchain to COLOR_ATTACHMENT_OPTIMAL for rendering
		record_image_layout_transition( vk.desktopBlitCmd, dstImage,
			VK_IMAGE_ASPECT_COLOR_BIT,
			dstLayout,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, 0 );
		dstLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// Begin render pass
		{
			VkRenderPassBeginInfo rpBegin;
			VkClearValue clearValue;

			clearValue.color.float32[0] = 0.0f;
			clearValue.color.float32[1] = 0.0f;
			clearValue.color.float32[2] = 0.0f;
			clearValue.color.float32[3] = 1.0f;

			rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			rpBegin.pNext = NULL;
			rpBegin.renderPass = vk.desktopMirrorRenderPass;
			rpBegin.framebuffer = vk.desktopMirrorFramebuffers[desktopImageIndex];
			rpBegin.renderArea.offset.x = 0;
			rpBegin.renderArea.offset.y = 0;
			rpBegin.renderArea.extent.width = dstWidth;
			rpBegin.renderArea.extent.height = dstHeight;
			rpBegin.clearValueCount = 1;
			rpBegin.pClearValues = &clearValue;

			qvkCmdBeginRenderPass( vk.desktopBlitCmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE );
		}

		qvkCmdBindPipeline( vk.desktopBlitCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.desktopMirrorPipeline );
		// set 0 = SDR source (eye swapchain), unused in HDR but kept valid; sets 1/2 =
		// scene base + emissive, sampled by the HDR (hdrMode==1) reconstruction.
		{
			VkDescriptorSet mirrorSets[3] = {
				vk.hdrActive ? vk.color_descriptor : vk.xr.colorArrayDescriptors[xrColorIndex],
				vk.color_descriptor,
				vk.emissive_descriptor,
			};
			qvkCmdBindDescriptorSets( vk.desktopBlitCmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
				vk.desktopMirrorPipelineLayout, 0, 3, mirrorSets, 0, NULL );
		}

		// Calculate viewport/scissor for the destination
		{
			VkViewport viewport;
			VkRect2D scissor;

			viewport.x = 0;
			viewport.y = 0;
			viewport.width = (float)dstWidth;
			viewport.height = (float)dstHeight;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			qvkCmdSetViewport( vk.desktopBlitCmd, 0, 1, &viewport );

			scissor.offset.x = 0;
			scissor.offset.y = 0;
			scissor.extent.width = dstWidth;
			scissor.extent.height = dstHeight;
			qvkCmdSetScissor( vk.desktopBlitCmd, 0, 1, &scissor );
		}

		// Push constants structure: offset(8) + scale(8) + texCrop(8) + eyeLayer(4) + hdr fields(32)
		struct {
			float offsetX, offsetY;
			float scaleX, scaleY;
			float texCropX, texCropY;
			int32_t eyeLayer;
			int32_t hdrMode;
			float hdrGamma;
			float hdrObScale;
			float paperWhite;
			float hdrPeak;
			float hdrHighlight;
			float hdrSaturation;
			float hdrSaturationFull;
			float hdrSoftKnee;
			int32_t hdrCalibrate;
		} pushConstants;

		// Calculate letterbox/pillarbox (fit) or crop (fill) parameters
		float srcAspect = (float)srcWidth / (float)srcHeight;

		if ( contentType == 2 ) {
			// Both eyes - side by side
			int halfDstWidth = dstWidth / 2;
			float dstAspect = (float)halfDstWidth / (float)dstHeight;

			// quadScale: controls the rendered quad size (for letterboxing)
			// texCrop: controls texture sampling region (for fill/crop)
			float quadScaleX, quadScaleY;
			float texCropX = 1.0f, texCropY = 1.0f;

			if ( contentFit == 1 ) {
				// Fill/crop mode: quad fills half the screen, texture is cropped
				quadScaleX = 0.5f;  // Half screen width for each eye
				quadScaleY = 1.0f;  // Full height
				if ( srcAspect > dstAspect ) {
					// Source wider - crop left/right
					texCropX = dstAspect / srcAspect;
					texCropY = 1.0f;
				} else {
					// Source taller - crop top/bottom
					texCropX = 1.0f;
					texCropY = srcAspect / dstAspect;
				}
			} else {
				// Fit/contain mode: texture is full, quad may be letterboxed
				texCropX = 1.0f;
				texCropY = 1.0f;
				if ( srcAspect > dstAspect ) {
					// Source wider - letterbox top/bottom
					quadScaleX = 0.5f;  // Each eye takes half width
					quadScaleY = dstAspect / srcAspect;
				} else {
					// Source taller - pillarbox left/right
					quadScaleX = 0.5f * (srcAspect / dstAspect);
					quadScaleY = 1.0f;
				}
			}

			// Draw left eye (offset left, layer 0)
			pushConstants.offsetX = -0.5f;  // Center of left half in NDC
			pushConstants.offsetY = 0.0f;
			pushConstants.scaleX = quadScaleX;
			pushConstants.scaleY = quadScaleY;
			pushConstants.texCropX = texCropX;
			pushConstants.texCropY = texCropY;
			pushConstants.eyeLayer = 0;
			pushConstants.hdrMode = 0;
			pushConstants.hdrGamma = 1.0f;
			pushConstants.hdrObScale = 1.0f;
			pushConstants.paperWhite = 200.0f;
			pushConstants.hdrPeak = 1000.0f;
			pushConstants.hdrHighlight = 1.0f;
			pushConstants.hdrCalibrate = 0;  // calibration pattern is drawn only on the desktop-view mirror
			if ( vk.hdrActive ) {
				float peakNits = r_hdrPeak->value;
				pushConstants.eyeLayer = 0;
				pushConstants.hdrMode = 1;
				pushConstants.hdrGamma = 1.0f / r_gamma->value;
				pushConstants.hdrObScale = (float)( 1 << tr.overbrightBits );
				pushConstants.paperWhite = vk_hdr_paper_white();
				pushConstants.hdrPeak = peakNits;
				pushConstants.hdrHighlight = r_hdrHighlight->value;
				pushConstants.hdrSaturation = r_hdrSaturation->value;
				pushConstants.hdrSaturationFull = r_hdrSaturationFull->value;
				pushConstants.hdrSoftKnee = r_hdrSoftKnee->value;
			}
			qvkCmdPushConstants( vk.desktopBlitCmd, vk.desktopMirrorPipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( pushConstants ), &pushConstants );
			qvkCmdDraw( vk.desktopBlitCmd, 3, 1, 0, 0 );

			// Draw right eye (offset right, layer 1)
			pushConstants.offsetX = 0.5f;  // Center of right half in NDC
			pushConstants.eyeLayer = 1;
			qvkCmdPushConstants( vk.desktopBlitCmd, vk.desktopMirrorPipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( pushConstants ), &pushConstants );
			qvkCmdDraw( vk.desktopBlitCmd, 3, 1, 0, 0 );
		} else {
			// Single eye (0=left, 1=right)
			float dstAspect = (float)dstWidth / (float)dstHeight;

			float quadScaleX, quadScaleY;
			float texCropX = 1.0f, texCropY = 1.0f;

			if ( contentFit == 1 ) {
				// Fill/crop mode: quad fills screen, texture is cropped
				quadScaleX = 1.0f;
				quadScaleY = 1.0f;
				if ( srcAspect > dstAspect ) {
					// Source wider - crop left/right
					texCropX = dstAspect / srcAspect;
					texCropY = 1.0f;
				} else {
					// Source taller - crop top/bottom
					texCropX = 1.0f;
					texCropY = srcAspect / dstAspect;
				}
			} else {
				// Fit/contain mode: texture is full, quad may be letterboxed
				texCropX = 1.0f;
				texCropY = 1.0f;
				if ( srcAspect > dstAspect ) {
					// Source wider - letterbox top/bottom
					quadScaleX = 1.0f;
					quadScaleY = dstAspect / srcAspect;
				} else {
					// Source taller - pillarbox left/right
					quadScaleX = srcAspect / dstAspect;
					quadScaleY = 1.0f;
				}
			}

			pushConstants.offsetX = 0.0f;
			pushConstants.offsetY = 0.0f;
			pushConstants.scaleX = quadScaleX;
			pushConstants.scaleY = quadScaleY;
			pushConstants.texCropX = texCropX;
			pushConstants.texCropY = texCropY;
			pushConstants.eyeLayer = ( contentType == 1 ) ? 1 : 0;
			pushConstants.hdrMode = 0;
			pushConstants.hdrGamma = 1.0f;
			pushConstants.hdrObScale = 1.0f;
			pushConstants.paperWhite = 200.0f;
			pushConstants.hdrPeak = 1000.0f;
			pushConstants.hdrHighlight = 1.0f;
			pushConstants.hdrCalibrate = 0;  // calibration pattern is drawn only on the desktop-view mirror
			if ( vk.hdrActive ) {
				float peakNits = r_hdrPeak->value;
				pushConstants.hdrMode = 1;
				pushConstants.hdrGamma = 1.0f / r_gamma->value;
				pushConstants.hdrObScale = (float)( 1 << tr.overbrightBits );
				pushConstants.paperWhite = vk_hdr_paper_white();
				pushConstants.hdrPeak = peakNits;
				pushConstants.hdrHighlight = r_hdrHighlight->value;
				pushConstants.hdrSaturation = r_hdrSaturation->value;
				pushConstants.hdrSaturationFull = r_hdrSaturationFull->value;
				pushConstants.hdrSoftKnee = r_hdrSoftKnee->value;
			}
			qvkCmdPushConstants( vk.desktopBlitCmd, vk.desktopMirrorPipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( pushConstants ), &pushConstants );
			qvkCmdDraw( vk.desktopBlitCmd, 3, 1, 0, 0 );
		}

		qvkCmdEndRenderPass( vk.desktopBlitCmd );
		// Render pass finalLayout transitions to PRESENT_SRC_KHR
		dstLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		if ( !vk.hdrActive ) {
			// SDR: transition XR swapchain back to COLOR_ATTACHMENT_OPTIMAL for OpenXR
			record_image_layout_transition( vk.desktopBlitCmd, vk.xr.colorInfo->images[xrColorIndex],
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, 0 );
		}
	}

finish_desktop_mirror:
	// Transition destination to PRESENT_SRC_KHR for presentation
	record_image_layout_transition( vk.desktopBlitCmd, dstImage,
		VK_IMAGE_ASPECT_COLOR_BIT,
		dstLayout,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 0, 0 );

	// End and submit command buffer
	VK_CHECK( qvkEndCommandBuffer( vk.desktopBlitCmd ) );

	// Submit blit command buffer
	// - Wait on acquire semaphore: ensures presentation engine is done reading before we write
	// - Wait on renderingCompleteSem: ensures XR rendering is done before we read virtualScreenImage
	//   (only if vk_end_frame submitted this frame: renderingCompleteSignaled indicates successful submit)
	// - Signal per-image semaphore: indexed by acquired image, presentation waits on this
	{
		VkSemaphore waitSemaphores[2];
		VkPipelineStageFlags waitStages[2];
		uint32_t waitCount;

		waitSemaphores[0] = vk.desktopAcquireSem;
		waitStages[0] = VK_PIPELINE_STAGE_TRANSFER_BIT;
		waitCount = 1;

		// Only wait on renderingCompleteSem if vk_end_frame actually signaled it this frame
		if ( vk.renderingCompleteSemSignaled ) {
			waitSemaphores[1] = vk.renderingCompleteSem;
			waitStages[1] = VK_PIPELINE_STAGE_TRANSFER_BIT;
			waitCount = 2;
			vk.renderingCompleteSemSignaled = qfalse;  // Consumed
		}

		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.pNext = NULL;
		submit_info.waitSemaphoreCount = waitCount;
		submit_info.pWaitSemaphores = waitSemaphores;
		submit_info.pWaitDstStageMask = waitStages;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &vk.desktopBlitCmd;
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &vk.desktopBlitComplete[desktopImageIndex];

		VK_CHECK( qvkQueueSubmit( vk.queue, 1, &submit_info, vk.desktopBlitFence ) );
	}

	// Present desktop swapchain - wait for blit to complete using per-image semaphore
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = NULL;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &vk.desktopBlitComplete[desktopImageIndex];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &vk.swapchain;
	presentInfo.pImageIndices = &desktopImageIndex;
	presentInfo.pResults = NULL;

	result = qvkQueuePresentKHR( vk.queue, &presentInfo );
	if ( result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR && result != VK_ERROR_OUT_OF_DATE_KHR ) {
		ri.Printf( PRINT_WARNING, "vk_present_desktop_mirror: Present failed: %s\n", vk_result_string( result ) );
	}
}


static qboolean is_bgr( VkFormat format ) {
	switch ( format ) {
		case VK_FORMAT_B8G8R8A8_UNORM:
		case VK_FORMAT_B8G8R8A8_SNORM:
		case VK_FORMAT_B8G8R8A8_UINT:
		case VK_FORMAT_B8G8R8A8_SINT:
		case VK_FORMAT_B8G8R8A8_SRGB:
		case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
			return qtrue;
		default:
			return qfalse;
	}
}


void vk_read_pixels( byte *buffer, uint32_t width, uint32_t height )
{
	VkCommandBuffer command_buffer;
	VkDeviceMemory memory;
	VkMemoryRequirements memory_requirements;
	VkMemoryPropertyFlags memory_reqs;
	VkMemoryPropertyFlags memory_flags;
	VkMemoryAllocateInfo alloc_info;
	VkImageSubresource subresource;
	VkSubresourceLayout layout;
	VkImageCreateInfo desc;
	VkImage srcImage;
	VkImageLayout srcImageLayout;
	VkImage dstImage;
	byte *buffer_ptr;
	byte *data;
	uint32_t pixel_width;
	uint32_t i, n;
	qboolean invalidate_ptr;

	VK_CHECK( qvkWaitForFences( vk.device, 1, &vk.cmd->rendering_finished_fence, VK_FALSE, 1e12 ) );

	if ( vk.capture.image ) {
		// dedicated capture buffer
		srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		srcImage = vk.capture.image;
	} else {
		srcImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		srcImage = vk.color_image;
	}

	Com_Memset( &desc, 0, sizeof( desc ) );

	// Create image in host visible memory to serve as a destination for framebuffer pixels.
	desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.imageType = VK_IMAGE_TYPE_2D;
	desc.format = vk.capture_format;
	desc.extent.width = width;
	desc.extent.height = height;
	desc.extent.depth = 1;
	desc.mipLevels = 1;
	desc.arrayLayers = 1;
	desc.samples = VK_SAMPLE_COUNT_1_BIT;
	desc.tiling = VK_IMAGE_TILING_LINEAR;
	desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;
	desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VK_CHECK( qvkCreateImage( vk.device, &desc, NULL, &dstImage ) );

	qvkGetImageMemoryRequirements( vk.device, dstImage, &memory_requirements );

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = memory_requirements.size;

	// host_cached bit is desirable for fast reads
	memory_reqs = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
	alloc_info.memoryTypeIndex = find_memory_type2( memory_requirements.memoryTypeBits, memory_reqs, &memory_flags );
	if ( alloc_info.memoryTypeIndex == ~0 ) {
		// try less explicit flags, without host_coherent
		memory_reqs = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
		alloc_info.memoryTypeIndex = find_memory_type2( memory_requirements.memoryTypeBits, memory_reqs, &memory_flags );
		if ( alloc_info.memoryTypeIndex == ~0U ) {
			// slowest case
			memory_reqs = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			alloc_info.memoryTypeIndex = find_memory_type2( memory_requirements.memoryTypeBits, memory_reqs, &memory_flags );
			if ( alloc_info.memoryTypeIndex == ~0U ) {
				ri.Error( ERR_FATAL, "%s(): failed to find matching memory type for image capture", __func__ );
			}
		}
	}

	if ( memory_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT ) {
		invalidate_ptr = qfalse;
	} else {
		 // according to specification - must be performed if host_coherent is not set
		invalidate_ptr = qtrue;
	}

	VK_CHECK(qvkAllocateMemory(vk.device, &alloc_info, NULL, &memory));
	VK_CHECK(qvkBindImageMemory(vk.device, dstImage, memory, 0));

	command_buffer = begin_command_buffer();

	if ( srcImageLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL ) {
		record_image_layout_transition( command_buffer, srcImage,
			VK_IMAGE_ASPECT_COLOR_BIT,
			srcImageLayout,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			0, 0);
	}

	record_image_layout_transition( command_buffer, dstImage,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );

	// end_command_buffer( command_buffer );

	// command_buffer = begin_command_buffer();

	if ( vk.blitEnabled ) {
		VkImageBlit region;

		region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.srcSubresource.mipLevel = 0;
		region.srcSubresource.baseArrayLayer = 0;
		region.srcSubresource.layerCount = 1;
		region.srcOffsets[0].x = 0;
		region.srcOffsets[0].y = 0;
		region.srcOffsets[0].z = 0;
		region.srcOffsets[1].x = width;
		region.srcOffsets[1].y = height;
		region.srcOffsets[1].z = 1;
		region.dstSubresource = region.srcSubresource;
		region.dstOffsets[0] = region.srcOffsets[0];
		region.dstOffsets[1] = region.srcOffsets[1];

		qvkCmdBlitImage( command_buffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_NEAREST );

	} else {
		VkImageCopy region;

		region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.srcSubresource.mipLevel = 0;
		region.srcSubresource.baseArrayLayer = 0;
		region.srcSubresource.layerCount = 1;
		region.srcOffset.x = 0;
		region.srcOffset.y = 0;
		region.srcOffset.z = 0;
		region.dstSubresource = region.srcSubresource;
		region.dstOffset = region.srcOffset;
		region.extent.width = width;
		region.extent.height = height;
		region.extent.depth = 1;

		qvkCmdCopyImage( command_buffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );
	}

	end_command_buffer( command_buffer, __func__ );

	// Copy data from destination image to memory buffer.
	subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresource.mipLevel = 0;
	subresource.arrayLayer = 0;

	qvkGetImageSubresourceLayout( vk.device, dstImage, &subresource, &layout );

	VK_CHECK( qvkMapMemory( vk.device, memory, 0, VK_WHOLE_SIZE, 0, (void**)&data ) );

	if ( invalidate_ptr )
	{
		VkMappedMemoryRange range;
		range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.pNext = NULL;
		range.memory = memory;
		range.size = VK_WHOLE_SIZE;
		range.offset = 0;
		qvkInvalidateMappedMemoryRanges( vk.device, 1, &range );
	}

	data += layout.offset;

	switch ( vk.capture_format ) {
		case VK_FORMAT_B4G4R4A4_UNORM_PACK16: pixel_width = 2; break;
		case VK_FORMAT_R16G16B16A16_UNORM: pixel_width = 8; break;
		default: pixel_width = 4; break;
	}

	buffer_ptr = buffer + width * (height - 1) * 3;
	for ( i = 0; i < height; i++ ) {
		switch ( pixel_width ) {
			case 2: {
				uint16_t *src = (uint16_t*)data;
				for ( n = 0; n < width; n++ ) {
					buffer_ptr[n*3+0] = ((src[n]>>12)&0xF)<<4;
					buffer_ptr[n*3+1] = ((src[n]>>8)&0xF)<<4;
					buffer_ptr[n*3+2] = ((src[n]>>4)&0xF)<<4;
				}
			} break;

			case 4: {
				for ( n = 0; n < width; n++ ) {
					Com_Memcpy( &buffer_ptr[n*3], &data[n*4], 3 );
					//buffer_ptr[n*3+0] = data[n*4+0];
					//buffer_ptr[n*3+1] = data[n*4+1];
					//buffer_ptr[n*3+2] = data[n*4+2];
				}
			} break;

			case 8: {
				const uint16_t *src = (uint16_t*)data;
				for ( n = 0; n < width; n++ ) {
					buffer_ptr[n*3+0] = src[n*4+0]>>8;
					buffer_ptr[n*3+1] = src[n*4+1]>>8;
					buffer_ptr[n*3+2] = src[n*4+2]>>8;
				}
			} break;
		}
		buffer_ptr -= width * 3;
		data += layout.rowPitch;
	}

	if ( is_bgr( vk.capture_format ) ) {
		buffer_ptr = buffer;
		for ( i = 0; i < width * height; i++ ) {
			byte tmp = buffer_ptr[0];
			buffer_ptr[0] = buffer_ptr[2];
			buffer_ptr[2] = tmp;
			buffer_ptr += 3;
		}
	}

	qvkDestroyImage( vk.device, dstImage, NULL );
	qvkFreeMemory( vk.device, memory, NULL );

	// restore previous layout
	if ( srcImageLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL ) {
		command_buffer = begin_command_buffer();

		record_image_layout_transition( command_buffer, srcImage,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			srcImageLayout, 0, 0 );

		end_command_buffer( command_buffer, "restore layout" );
	}
}


qboolean vk_bloom( void )
{
	uint32_t i;
	uint32_t width, height;

	if ( vk.renderPassIndex == RENDER_PASS_SCREENMAP )
	{
		return qfalse;
	}

	if ( backEnd.doneBloom || !backEnd.doneSurfaces )
	{
		return qfalse;
	}

	// Q3VR is VR-only: use FBO resources
	if ( !vk.xr.initialized || vk.color_image == VK_NULL_HANDLE )
	{
		return qfalse;
	}

	if ( vk.bloom_extract_pipeline == VK_NULL_HANDLE )
	{
		return qfalse;
	}

	width = vk.xr.width;
	height = vk.xr.height;

	vk_end_render_pass(); // end main FBO render pass

	// Transition FBO color to shader read for bloom extraction
	record_image_layout_transition( vk.cmd->command_buffer,
		vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );

	// Bloom extraction - sample FBO color, output to bloom_image[0]
	vk.renderWidth = width;
	vk.renderHeight = height;
	vk.renderScaleX = vk.renderScaleY = 1.0f;

	vk_begin_render_pass( vk.render_pass.bloom_extract, vk.framebuffers.bloom_extract,
		qfalse, width, height );
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.bloom_extract_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.pipeline_layout_post_process, 0, 1, &vk.color_descriptor, 0, NULL );
	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
	vk_end_render_pass();

	// Blur passes - alternating horizontal/vertical at decreasing sizes
	for ( i = 0; i < VK_NUM_BLOOM_PASSES * 2; i += 2 ) {
		uint32_t blur_width = width / ( 2 << ( i / 2 ) );
		uint32_t blur_height = height / ( 2 << ( i / 2 ) );

		vk.renderWidth = blur_width;
		vk.renderHeight = blur_height;

		// Horizontal blur
		vk_begin_render_pass( vk.render_pass.blur[i+0], vk.framebuffers.blur[i+0],
			qfalse, blur_width, blur_height );
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			vk.blur_pipeline[i+0] );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			vk.pipeline_layout_post_process, 0, 1, &vk.bloom_image_descriptor[i+0], 0, NULL );
		qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
		vk_end_render_pass();

		// Vertical blur
		vk_begin_render_pass( vk.render_pass.blur[i+1], vk.framebuffers.blur[i+1],
			qfalse, blur_width, blur_height );
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			vk.blur_pipeline[i+1] );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			vk.pipeline_layout_post_process, 0, 1, &vk.bloom_image_descriptor[i+1], 0, NULL );
		qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
		vk_end_render_pass();
	}

	// Post-bloom blend - blend blurred images back to FBO
	{
		VkDescriptorSet dset[VK_NUM_BLOOM_PASSES];

		for ( i = 0; i < VK_NUM_BLOOM_PASSES; i++ )
		{
			dset[i] = vk.bloom_image_descriptor[(i+1)*2];
		}

		// Transition FBO color back to attachment for blending
		record_image_layout_transition( vk.cmd->command_buffer,
			vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, 0 );

		// Post-bloom blends back to FBO with LOAD_OP_LOAD (preserves base image)
		// Post_bloom render pass must match RENDER_PASS_MAIN attachment structure
		// for 2D pipeline compatibility (2D pipelines created for RENDER_PASS_MAIN)
		vk.renderWidth = width;
		vk.renderHeight = height;
		vk.renderScaleX = vk.renderScaleY = 1.0f;

		vk_begin_render_pass( vk.render_pass.post_bloom, vk.framebuffers.post_bloom,
			qfalse, width, height );
		vk.renderPassIndex = RENDER_PASS_POST_BLOOM;

		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			vk.bloom_blend_pipeline );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			vk.pipeline_layout_blend, 0, ARRAY_LEN(dset), dset, 0, NULL );
		qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
		// NOTE: Do NOT end render pass here; keep post_bloom open for 2D rendering
		// The render pass will be ended by vk_end_frame() or next vk_end_render_pass() call
	}

	// The bloom passes bound foreign layouts at set index 0, invalidating it for
	// the main layout. Re-arm the tracking unconditionally (2D draws never push
	// uniforms/eyeProj themselves) so the next main-layout draw rebinds set 0
	// with both dynamic offsets.
	vk_reset_descriptor( VK_DESC_UNIFORM );
	vk_update_descriptor( VK_DESC_UNIFORM, vk.cmd->uniform_descriptor );

	// Restore pipeline state for continued 2D rendering (Quake3e pattern)
	if ( vk.cmd->last_pipeline != VK_NULL_HANDLE )
	{
		// restore last pipeline
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.cmd->last_pipeline );

		vk_update_mvp( NULL );

		// force depth range and viewport/scissor updates
		vk.cmd->depth_range = DEPTH_RANGE_COUNT;

		// restore clobbered descriptor sets
		for ( i = 0; i < VK_NUM_BLOOM_PASSES; i++ ) {
			if ( vk.cmd->descriptor_set.current[i] != VK_NULL_HANDLE ) {
				if ( i == VK_DESC_UNIFORM ) {
					// uniform + eyeproj dynamic offsets, binding order
					uint32_t restore_offsets[2];
					restore_offsets[0] = vk.cmd->descriptor_set.offset[i];
					restore_offsets[1] = vk.cmd->eyeproj_offset;
					qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout, i, 1, &vk.cmd->descriptor_set.current[i], 2, restore_offsets );
				} else
					qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout, i, 1, &vk.cmd->descriptor_set.current[i], 0, NULL );
			}
		}
	}

	backEnd.doneBloom = qtrue;

	return qtrue;
}

// ============================================================================
// XR Swapchain Resource Management (Phase 2)
// These functions create VkImageViews and VkFramebuffers for XR swapchain images.
// Full implementation requires render passes (Phase 3).
// ============================================================================

#ifdef USE_VULKAN
#include "../vrvk/vr_vk_types.h"

/*
 * vk_create_xr_native_depth - Create native Vulkan depth buffer for XR framebuffers
 *
 * Creates a single 2-layer multiview depth buffer that replaces the
 * OpenXR-provided depth swapchain. This fixes compatibility issues
 * with some XR runtimes that fail to create valid depth swapchains.
 */
static qboolean vk_create_xr_native_depth( void )
{
	VkXrResources *xr = &vk.xr;
	VkImageCreateInfo imageCI;
	VkMemoryRequirements memReqs;
	VkMemoryAllocateInfo allocInfo;
	VkImageViewCreateInfo viewCI;
	uint32_t memoryType;

	if ( xr->width == 0 || xr->height == 0 ) {
		ri.Printf( PRINT_WARNING, "vk_create_xr_native_depth: XR resolution not set\n" );
		return qfalse;
	}

	ri.Printf( PRINT_ALL, "Creating native XR depth buffer (%ux%u, format=0x%x)...\n",
		xr->width, xr->height, vk.depth_format );

	// Create depth image (2-layer for stereo multiview)
	Com_Memset( &imageCI, 0, sizeof( imageCI ) );
	imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCI.imageType = VK_IMAGE_TYPE_2D;
	imageCI.format = vk.depth_format;
	imageCI.extent.width = xr->width;
	imageCI.extent.height = xr->height;
	imageCI.extent.depth = 1;
	imageCI.mipLevels = 1;
	imageCI.arrayLayers = 2;  // Stereo multiview
	imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VK_CHECK( qvkCreateImage( vk.device, &imageCI, NULL, &xr->xrDepthImage ) );
	SET_OBJECT_NAME( xr->xrDepthImage, "XR native depth image", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );

	// Allocate device-local memory
	qvkGetImageMemoryRequirements( vk.device, xr->xrDepthImage, &memReqs );

	memoryType = find_memory_type(
		memReqs.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

	Com_Memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReqs.size;
	allocInfo.memoryTypeIndex = memoryType;

	VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &xr->xrDepthMemory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, xr->xrDepthImage, xr->xrDepthMemory, 0 ) );

	// Create image view (2D_ARRAY for multiview)
	Com_Memset( &viewCI, 0, sizeof( viewCI ) );
	viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCI.image = xr->xrDepthImage;
	viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	viewCI.format = vk.depth_format;
	viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	viewCI.subresourceRange.baseMipLevel = 0;
	viewCI.subresourceRange.levelCount = 1;
	viewCI.subresourceRange.baseArrayLayer = 0;
	viewCI.subresourceRange.layerCount = 2;  // Stereo

	VK_CHECK( qvkCreateImageView( vk.device, &viewCI, NULL, &xr->xrDepthView ) );
	SET_OBJECT_NAME( xr->xrDepthView, "XR native depth view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );

	ri.Printf( PRINT_ALL, "Native XR depth buffer created successfully\n" );
	return qtrue;
}

/*
 * vk_destroy_xr_native_depth - Destroy native XR depth buffer resources
 */
static void vk_destroy_xr_native_depth( void )
{
	VkXrResources *xr = &vk.xr;

	if ( xr->xrDepthView != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, xr->xrDepthView, NULL );
		xr->xrDepthView = VK_NULL_HANDLE;
	}
	if ( xr->xrDepthImage != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, xr->xrDepthImage, NULL );
		xr->xrDepthImage = VK_NULL_HANDLE;
	}
	if ( xr->xrDepthMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, xr->xrDepthMemory, NULL );
		xr->xrDepthMemory = VK_NULL_HANDLE;
	}
}

/*
 * vk_create_xr_image_views - Create VkImageViews for XR swapchain images
 *
 * Creates multiview array views for the color swapchain,
 * and per-eye views for desktop mirror blitting.
 *
 * This function is called after the VR layer creates XR swapchains
 * and the renderer creates render passes.
 */
qboolean vk_create_xr_image_views( void )
{
	VkXrResources *xr = &vk.xr;

	if ( !xr->colorInfo ) {
		ri.Printf( PRINT_WARNING, "vk_create_xr_image_views: XR color swapchain info not set\n" );
		return qfalse;
	}

	ri.Printf( PRINT_ALL, "Creating XR image views...\n" );

	// Store XR resolution
	xr->width = xr->colorInfo->width;
	xr->height = xr->colorInfo->height;

	// Create color views (multiview array)
	for ( uint32_t i = 0; i < xr->colorInfo->imageCount && i < MAX_SWAPCHAIN_IMAGES; i++ ) {
		VkImageViewCreateInfo viewInfo = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext = NULL,
			.flags = 0,
			.image = xr->colorInfo->images[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
			.format = xr->colorInfo->format,
			.components = {
				.r = VK_COMPONENT_SWIZZLE_IDENTITY,
				.g = VK_COMPONENT_SWIZZLE_IDENTITY,
				.b = VK_COMPONENT_SWIZZLE_IDENTITY,
				.a = VK_COMPONENT_SWIZZLE_IDENTITY,
			},
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = xr->colorInfo->arraySize,
			},
		};

		VK_CHECK( qvkCreateImageView( vk.device, &viewInfo, NULL, &xr->colorViews[i] ) );

		// Create UNORM view for gamma pass (no automatic sRGB conversion)
		// The gamma shader outputs sRGB-encoded values directly, so we need to
		// bypass Vulkan's automatic linear-to-sRGB conversion on write
		{
			VkImageViewCreateInfo gammaViewInfo = viewInfo;
			gammaViewInfo.format = vk_get_unorm_format( xr->colorInfo->format );
			VK_CHECK( qvkCreateImageView( vk.device, &gammaViewInfo, NULL, &xr->gammaViews[i] ) );
		}

		// Create per-eye views for desktop mirror sampling (menuStyle=1 "VR view")
		// These are 2D views into each layer of the multiview array, using UNORM format
		// to match the gamma-corrected output (no automatic sRGB decode on read)
		for ( uint32_t eye = 0; eye < 2 && eye < xr->colorInfo->arraySize; eye++ ) {
			VkImageViewCreateInfo eyeViewInfo = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.pNext = NULL,
				.flags = 0,
				.image = xr->colorInfo->images[i],
				.viewType = VK_IMAGE_VIEW_TYPE_2D,  // Single layer, not array
				.format = vk_get_unorm_format( xr->colorInfo->format ),  // UNORM for gamma-correct sampling
				.components = {
					.r = VK_COMPONENT_SWIZZLE_IDENTITY,
					.g = VK_COMPONENT_SWIZZLE_IDENTITY,
					.b = VK_COMPONENT_SWIZZLE_IDENTITY,
					.a = VK_COMPONENT_SWIZZLE_IDENTITY,
				},
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = eye,  // Select specific layer
					.layerCount = 1,        // Single layer
				},
			};
			VK_CHECK( qvkCreateImageView( vk.device, &eyeViewInfo, NULL, &xr->colorEyeViews[eye][i] ) );
		}
	}

	ri.Printf( PRINT_ALL, "XR color image views created: %u\n", xr->colorInfo->imageCount );

	// Allocate and initialize per-eye descriptors for desktop mirror VR view mode
	// These are pre-bound to specific swapchain images so we don't need to update them at runtime
	// Note: This requires vk.linearSampler and vk.descriptor_pool to be created first (in vk_initialize)
	if ( vk.linearSampler != VK_NULL_HANDLE && vk.descriptor_pool != VK_NULL_HANDLE &&
	     vk.set_layout_sampler != VK_NULL_HANDLE ) {
		VkDescriptorSetAllocateInfo descAllocInfo;
		VkDescriptorImageInfo imageInfo;
		VkWriteDescriptorSet descriptorWrite;

		Com_Memset( &descAllocInfo, 0, sizeof( descAllocInfo ) );
		descAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descAllocInfo.descriptorPool = vk.descriptor_pool;
		descAllocInfo.descriptorSetCount = 1;
		descAllocInfo.pSetLayouts = &vk.set_layout_sampler;

		Com_Memset( &imageInfo, 0, sizeof( imageInfo ) );
		imageInfo.sampler = vk.linearSampler;
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		Com_Memset( &descriptorWrite, 0, sizeof( descriptorWrite ) );
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstBinding = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pImageInfo = &imageInfo;

		for ( uint32_t i = 0; i < xr->colorInfo->imageCount && i < MAX_SWAPCHAIN_IMAGES; i++ ) {
			for ( int eye = 0; eye < 2; eye++ ) {
				VK_CHECK( qvkAllocateDescriptorSets( vk.device, &descAllocInfo, &xr->colorEyeDescriptors[eye][i] ) );
				imageInfo.imageView = xr->colorEyeViews[eye][i];
				descriptorWrite.dstSet = xr->colorEyeDescriptors[eye][i];
				qvkUpdateDescriptorSets( vk.device, 1, &descriptorWrite, 0, NULL );
			}

			// Create 2D_ARRAY descriptor for desktop mirror shader (sampler2DArray)
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &descAllocInfo, &xr->colorArrayDescriptors[i] ) );
			imageInfo.imageView = xr->colorViews[i];  // 2D_ARRAY view
			descriptorWrite.dstSet = xr->colorArrayDescriptors[i];
			qvkUpdateDescriptorSets( vk.device, 1, &descriptorWrite, 0, NULL );
		}

		ri.Printf( PRINT_ALL, "XR per-eye descriptors created: %u per eye\n", xr->colorInfo->imageCount );
		ri.Printf( PRINT_ALL, "XR 2D_ARRAY descriptors created: %u\n", xr->colorInfo->imageCount );
	} else {
		ri.Printf( PRINT_WARNING, "XR per-eye descriptors deferred: waiting for vk_initialize\n" );
	}

	return qtrue;
}

/*
 * vk_destroy_xr_image_views - Destroy VkImageViews for XR swapchain images
 */
void vk_destroy_xr_image_views( void )
{
	VkXrResources *xr = &vk.xr;

	// Destroy color views
	for ( uint32_t i = 0; i < MAX_SWAPCHAIN_IMAGES; i++ ) {
		if ( xr->colorViews[i] != VK_NULL_HANDLE ) {
			qvkDestroyImageView( vk.device, xr->colorViews[i], NULL );
			xr->colorViews[i] = VK_NULL_HANDLE;
		}
		if ( xr->gammaViews[i] != VK_NULL_HANDLE ) {
			qvkDestroyImageView( vk.device, xr->gammaViews[i], NULL );
			xr->gammaViews[i] = VK_NULL_HANDLE;
		}
		// Destroy per-eye views and reset descriptors for desktop mirror sampling
		for ( uint32_t eye = 0; eye < 2; eye++ ) {
			if ( xr->colorEyeViews[eye][i] != VK_NULL_HANDLE ) {
				qvkDestroyImageView( vk.device, xr->colorEyeViews[eye][i], NULL );
				xr->colorEyeViews[eye][i] = VK_NULL_HANDLE;
			}
			// Descriptors are freed when descriptor pool is reset, just clear handles
			xr->colorEyeDescriptors[eye][i] = VK_NULL_HANDLE;
		}
		// Reset 2D_ARRAY descriptors
		xr->colorArrayDescriptors[i] = VK_NULL_HANDLE;
	}
}

/*
 * vk_create_xr_framebuffers - Create VkFramebuffers for XR swapchain images
 *
 * When r_fbo is active:
 *   - Main rendering goes to xr->fboFramebuffer (created separately)
 *   - Gamma pass outputs to xr->gammaFramebuffers[] (created separately)
 *   - We don't create xr->framebuffers[] here since they're not used
 *
 * When r_fbo is NOT active:
 *   - Main rendering goes directly to XR swapchain via xr->framebuffers[]
 *   - We create multiview framebuffers for each swapchain image
 *
 * NOTE: Per-eye framebuffers for desktop mirror are not created since
 * desktop mirror is not implemented in this VR-only application.
 */
qboolean vk_create_xr_framebuffers( void )
{
	VkXrResources *xr = &vk.xr;
	VkFramebufferCreateInfo fbInfo;
	VkImageView attachments[2];
	uint32_t i;

	// Validate prerequisites
	if ( !vk.multiviewSupported ) {
		ri.Printf( PRINT_WARNING, "vk_create_xr_framebuffers: multiview not supported\n" );
		return qfalse;
	}

	if ( vk.render_pass.main == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_WARNING, "vk_create_xr_framebuffers: main render pass not created (ptr=%p, multiview=%d)\n",
			(void*)vk.render_pass.main, vk.multiviewSupported );
		return qfalse;
	}

	if ( xr->colorInfo == NULL ) {
		ri.Printf( PRINT_WARNING, "vk_create_xr_framebuffers: color swapchain info not set\n" );
		return qfalse;
	}

	if ( xr->xrDepthView == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_WARNING, "vk_create_xr_framebuffers: native depth buffer not created\n" );
		return qfalse;
	}

	// Create XR swapchain framebuffers (direct render when !r_fbo, virtual screen otherwise)
	for ( i = 0; i < xr->colorInfo->imageCount && i < MAX_SWAPCHAIN_IMAGES; i++ ) {
		// Color image views should already be created
		if ( xr->colorViews[i] == VK_NULL_HANDLE ) {
			ri.Printf( PRINT_WARNING, "vk_create_xr_framebuffers: color view %d not created\n", i );
			return qfalse;
		}

		// Use UNORM views to bypass automatic sRGB conversion (shader handles gamma)
		if ( xr->gammaViews[i] != VK_NULL_HANDLE ) {
			attachments[0] = xr->gammaViews[i];  // UNORM view: no auto sRGB conversion
		} else {
			attachments[0] = xr->colorViews[i];  // Fallback to sRGB view
		}
		attachments[1] = xr->xrDepthView;  // Single shared native depth buffer

		Com_Memset( &fbInfo, 0, sizeof( fbInfo ) );
		fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		// Use virtualScreen render pass - compatible with 2 attachments (color+depth), no MSAA
		fbInfo.renderPass = vk.render_pass.virtualScreen;
		fbInfo.attachmentCount = 2;
		fbInfo.pAttachments = attachments;
		fbInfo.width = xr->width;
		fbInfo.height = xr->height;
		// Multiview render pass: layers must be 1 (view mask handles stereo)
		fbInfo.layers = 1;

		VK_CHECK( qvkCreateFramebuffer( vk.device, &fbInfo, NULL, &xr->framebuffers[i] ) );
		SET_OBJECT_NAME( xr->framebuffers[i], va( "XR framebuffer %d (virtual screen)", i ),
			VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
	}

	ri.Printf( PRINT_ALL, "...XR swapchain framebuffers created (%d for virtual screen)\n",
		xr->colorInfo->imageCount );

	// Per-eye framebuffers for desktop mirror not implemented (VR-only app)

	return qtrue;
}

/*
 * vk_destroy_xr_framebuffers - Destroy VkFramebuffers for XR swapchain images
 */
void vk_destroy_xr_framebuffers( void )
{
	VkXrResources *xr = &vk.xr;

	// Destroy multiview framebuffers
	for ( uint32_t i = 0; i < MAX_SWAPCHAIN_IMAGES; i++ ) {
		if ( xr->framebuffers[i] != VK_NULL_HANDLE ) {
			qvkDestroyFramebuffer( vk.device, xr->framebuffers[i], NULL );
			xr->framebuffers[i] = VK_NULL_HANDLE;
		}
	}
}

/*
 * vk_create_hud_buffer - Create HUD buffer resources (1280x960 color + depth)
 *
 * Creates the VkImage, VkImageView, VkFramebuffer, VkDescriptorSet, and
 * tr.hudImage for the HUD buffer used in HUD mode 1 (in-world sprite rendering).
 * Called from R_InitImages() before CreateExternalShaders() sets up tr.hudShader.
 * Includes depth buffer for proper 3D model rendering (e.g., character heads with ponytails).
 */
qboolean vk_create_hud_buffer( void )
{
	VkXrResources *xr = &vk.xr;
	VkImageCreateInfo imageCI;
	VkMemoryRequirements colorMemReqs, depthMemReqs;
	VkMemoryAllocateInfo allocInfo;
	VkImageViewCreateInfo viewCI;
	VkFramebufferCreateInfo fbCI;
	VkImageView attachments[2];  // Color + depth
	uint32_t memoryType;

	// Clean up any existing HUD buffer resources first (happens on map reload with REF_KEEP_CONTEXT)
	vk_destroy_hud_buffer();

	ri.Printf( PRINT_ALL, "Creating HUD buffer (%dx%d, color_format=0x%x, depth_format=0x%x)...\n",
		HUD_BUFFER_WIDTH, HUD_BUFFER_HEIGHT, vk.color_format, vk.depth_format );

	// 1. Create HUD color image (1280x960, RGBA8, sampled + color attachment + transfer dest for clears)
	Com_Memset( &imageCI, 0, sizeof( imageCI ) );
	imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCI.imageType = VK_IMAGE_TYPE_2D;
	imageCI.format = vk.color_format;
	imageCI.extent.width = HUD_BUFFER_WIDTH;
	imageCI.extent.height = HUD_BUFFER_HEIGHT;
	imageCI.extent.depth = 1;
	imageCI.mipLevels = 1;
	imageCI.arrayLayers = 1;
	imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VK_CHECK( qvkCreateImage( vk.device, &imageCI, NULL, &xr->hudImage ) );
	SET_OBJECT_NAME( xr->hudImage, "HUD color image", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );

	// 2. Allocate memory for color image
	qvkGetImageMemoryRequirements( vk.device, xr->hudImage, &colorMemReqs );

	memoryType = find_memory_type(
		colorMemReqs.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

	Com_Memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = colorMemReqs.size;
	allocInfo.memoryTypeIndex = memoryType;

	VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &xr->hudMemory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, xr->hudImage, xr->hudMemory, 0 ) );

	// 3. Create color image views (2D_ARRAY for framebuffer, 2D for shader sampling)
	Com_Memset( &viewCI, 0, sizeof( viewCI ) );
	viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCI.image = xr->hudImage;
	viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	viewCI.format = vk.color_format;
	viewCI.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCI.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCI.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCI.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewCI.subresourceRange.baseMipLevel = 0;
	viewCI.subresourceRange.levelCount = 1;
	viewCI.subresourceRange.baseArrayLayer = 0;
	viewCI.subresourceRange.layerCount = 1;

	VK_CHECK( qvkCreateImageView( vk.device, &viewCI, NULL, &xr->hudView ) );
	SET_OBJECT_NAME( xr->hudView, "HUD color image view (2D_ARRAY for framebuffer)", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );

	// Create 2D view for shader sampling
	viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
	VK_CHECK( qvkCreateImageView( vk.device, &viewCI, NULL, &xr->hudSamplerView ) );
	SET_OBJECT_NAME( xr->hudSamplerView, "HUD color image view (2D for sampling)", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );

	// 4. Create HUD depth image for proper 3D model rendering
	Com_Memset( &imageCI, 0, sizeof( imageCI ) );
	imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCI.imageType = VK_IMAGE_TYPE_2D;
	imageCI.format = vk.depth_format;
	imageCI.extent.width = HUD_BUFFER_WIDTH;
	imageCI.extent.height = HUD_BUFFER_HEIGHT;
	imageCI.extent.depth = 1;
	imageCI.mipLevels = 1;
	imageCI.arrayLayers = 1;
	imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VK_CHECK( qvkCreateImage( vk.device, &imageCI, NULL, &xr->hudDepthImage ) );
	SET_OBJECT_NAME( xr->hudDepthImage, "HUD depth image", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );

	// 5. Allocate memory for depth image
	qvkGetImageMemoryRequirements( vk.device, xr->hudDepthImage, &depthMemReqs );

	memoryType = find_memory_type(
		depthMemReqs.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

	Com_Memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = depthMemReqs.size;
	allocInfo.memoryTypeIndex = memoryType;

	VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &xr->hudDepthMemory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, xr->hudDepthImage, xr->hudDepthMemory, 0 ) );

	// 6. Create depth image view (2D_ARRAY for multiview render pass compatibility)
	Com_Memset( &viewCI, 0, sizeof( viewCI ) );
	viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCI.image = xr->hudDepthImage;
	viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	viewCI.format = vk.depth_format;
	viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	viewCI.subresourceRange.baseMipLevel = 0;
	viewCI.subresourceRange.levelCount = 1;
	viewCI.subresourceRange.baseArrayLayer = 0;
	viewCI.subresourceRange.layerCount = 1;

	VK_CHECK( qvkCreateImageView( vk.device, &viewCI, NULL, &xr->hudDepthView ) );
	SET_OBJECT_NAME( xr->hudDepthView, "HUD depth image view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );

	// 7. Create framebuffer with color + depth attachments
	attachments[0] = xr->hudView;
	attachments[1] = xr->hudDepthView;

	Com_Memset( &fbCI, 0, sizeof( fbCI ) );
	fbCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fbCI.renderPass = vk.render_pass.hudBuffer;
	fbCI.attachmentCount = 2;  // Color + depth
	fbCI.pAttachments = attachments;
	fbCI.width = HUD_BUFFER_WIDTH;
	fbCI.height = HUD_BUFFER_HEIGHT;
	fbCI.layers = 1;

	VK_CHECK( qvkCreateFramebuffer( vk.device, &fbCI, NULL, &xr->hudFramebuffer ) );
	SET_OBJECT_NAME( xr->hudFramebuffer, "HUD framebuffer", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );

	// 7. Create descriptor set for sampling HUD texture
	{
		VkDescriptorSetAllocateInfo descAllocInfo;
		VkDescriptorImageInfo imageInfo;
		VkWriteDescriptorSet descriptorWrite;
		Vk_Sampler_Def samplerDef;

		Com_Memset( &descAllocInfo, 0, sizeof( descAllocInfo ) );
		descAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descAllocInfo.descriptorPool = vk.descriptor_pool;
		descAllocInfo.descriptorSetCount = 1;
		descAllocInfo.pSetLayouts = &vk.set_layout_sampler;

		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &descAllocInfo, &xr->hudDescriptor ) );

		// Set up sampler for HUD texture: linear filtering, clamp to edge, no mipmaps
		Com_Memset( &samplerDef, 0, sizeof( samplerDef ) );
		samplerDef.gl_mag_filter = GL_LINEAR;
		samplerDef.gl_min_filter = GL_LINEAR;
		samplerDef.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerDef.max_lod_1_0 = qtrue;  // No mipmaps
		samplerDef.noAnisotropy = qtrue;

		// Update descriptor to point to HUD color image (use 2D view for shader sampling)
		Com_Memset( &imageInfo, 0, sizeof( imageInfo ) );
		imageInfo.sampler = vk_find_sampler( &samplerDef );
		imageInfo.imageView = xr->hudSamplerView;  // Use 2D view, not 2D_ARRAY
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		Com_Memset( &descriptorWrite, 0, sizeof( descriptorWrite ) );
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = xr->hudDescriptor;
		descriptorWrite.dstBinding = 0;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pImageInfo = &imageInfo;

		ri.Printf( PRINT_ALL, "HUD descriptor update: descriptor=%p view=%p sampler=%p\n",
			(void*)(uintptr_t)xr->hudDescriptor, (void*)(uintptr_t)xr->hudSamplerView,
			(void*)(uintptr_t)imageInfo.sampler );

		qvkUpdateDescriptorSets( vk.device, 1, &descriptorWrite, 0, NULL );
		ri.Printf( PRINT_ALL, "HUD descriptor updated successfully\n" );
	}

	// 8. Initialize HUD color and depth images
	// Color: clear to transparent black and transition to SHADER_READ_ONLY_OPTIMAL
	// Depth: clear to 0.0 (USE_REVERSED_DEPTH); render pass uses UNDEFINED initialLayout with LOAD_OP_CLEAR
	{
		VkCommandBuffer cmdBuf;
		VkCommandBufferAllocateInfo cmdAllocInfo;
		VkCommandBufferBeginInfo beginInfo;
		VkSubmitInfo submitInfo;
		VkClearColorValue clearColor;
		VkClearDepthStencilValue clearDepth;
		VkImageSubresourceRange range;

		Com_Memset( &cmdAllocInfo, 0, sizeof( cmdAllocInfo ) );
		cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdAllocInfo.commandPool = vk.command_pool;
		cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmdAllocInfo.commandBufferCount = 1;

		VK_CHECK( qvkAllocateCommandBuffers( vk.device, &cmdAllocInfo, &cmdBuf ) );

		Com_Memset( &beginInfo, 0, sizeof( beginInfo ) );
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		VK_CHECK( qvkBeginCommandBuffer( cmdBuf, &beginInfo ) );

		// Initialize color image: transition to TRANSFER_DST and clear
		record_image_layout_transition( cmdBuf, xr->hudImage, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );

		clearColor.float32[0] = 0.0f;
		clearColor.float32[1] = 0.0f;
		clearColor.float32[2] = 0.0f;
		clearColor.float32[3] = 0.0f;

		range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		range.baseMipLevel = 0;
		range.levelCount = 1;
		range.baseArrayLayer = 0;
		range.layerCount = 1;

		qvkCmdClearColorImage( cmdBuf, xr->hudImage,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range );

		// Transition color to SHADER_READ_ONLY_OPTIMAL for sampling
		record_image_layout_transition( cmdBuf, xr->hudImage, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );

		// Initialize depth image: transition to TRANSFER_DST and clear to 0.0 (reversed depth)
		// For depth/stencil formats, barriers must include both aspects (Vulkan spec requirement
		// when separateDepthStencilLayouts is not enabled)
		VkImageAspectFlags depthAspects = VK_IMAGE_ASPECT_DEPTH_BIT;
		if ( vk_format_has_stencil( vk.depth_format ) ) {
			depthAspects |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}

		record_image_layout_transition( cmdBuf, xr->hudDepthImage, depthAspects,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );

		clearDepth.depth = 0.0f;  // USE_REVERSED_DEPTH: 0.0 = farthest
		clearDepth.stencil = 0;

		range.aspectMask = depthAspects;

		qvkCmdClearDepthStencilImage( cmdBuf, xr->hudDepthImage,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearDepth, 1, &range );

		// Transition depth to DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		// The render pass requires initialLayout=DEPTH_STENCIL_ATTACHMENT_OPTIMAL because
		// the HUD pass may run multiple times per frame (cgame + console notify), and
		// subsequent passes need the image in the correct layout
		record_image_layout_transition( cmdBuf, xr->hudDepthImage, depthAspects,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0, 0 );

		VK_CHECK( qvkEndCommandBuffer( cmdBuf ) );

		Com_Memset( &submitInfo, 0, sizeof( submitInfo ) );
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmdBuf;

		VK_CHECK( qvkQueueSubmit( vk.queue, 1, &submitInfo, VK_NULL_HANDLE ) );
		VK_CHECK( qvkQueueWaitIdle( vk.queue ) );

		qvkFreeCommandBuffers( vk.device, vk.command_pool, 1, &cmdBuf );
	}

	// 9. Create tr.hudImage wrapper for shader system
	// Pass the 2D sampler view since that's what shaders will sample
	tr.hudImage = R_CreateHUDImage( xr->hudImage, xr->hudSamplerView, xr->hudDescriptor,
	                                HUD_BUFFER_WIDTH, HUD_BUFFER_HEIGHT );
	ri.Printf( PRINT_ALL, "Created tr.hudImage wrapper: %p (descriptor=%p)\n",
		(void*)tr.hudImage, (void*)(uintptr_t)tr.hudImage->descriptor );

	ri.Printf( PRINT_ALL, "...HUD buffer created\n" );
	return qtrue;
}

/*
 * vk_destroy_hud_buffer - Destroy HUD buffer resources
 */
static void vk_destroy_hud_buffer( void )
{
	VkXrResources *xr = &vk.xr;

	if ( xr->hudFramebuffer != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, xr->hudFramebuffer, NULL );
		xr->hudFramebuffer = VK_NULL_HANDLE;
	}
	if ( xr->hudView != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, xr->hudView, NULL );
		xr->hudView = VK_NULL_HANDLE;
	}
	if ( xr->hudSamplerView != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, xr->hudSamplerView, NULL );
		xr->hudSamplerView = VK_NULL_HANDLE;
	}
	if ( xr->hudImage != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, xr->hudImage, NULL );
		xr->hudImage = VK_NULL_HANDLE;
	}
	if ( xr->hudMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, xr->hudMemory, NULL );
		xr->hudMemory = VK_NULL_HANDLE;
	}
	// Depth buffer resources
	if ( xr->hudDepthView != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, xr->hudDepthView, NULL );
		xr->hudDepthView = VK_NULL_HANDLE;
	}
	if ( xr->hudDepthImage != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, xr->hudDepthImage, NULL );
		xr->hudDepthImage = VK_NULL_HANDLE;
	}
	if ( xr->hudDepthMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, xr->hudDepthMemory, NULL );
		xr->hudDepthMemory = VK_NULL_HANDLE;
	}
	// Descriptor is freed when pool is reset
	xr->hudDescriptor = VK_NULL_HANDLE;
}


/*
 * vk_destroy_virtual_screen_buffer - Destroy virtual screen buffer resources
 */
static void vk_destroy_virtual_screen_buffer( void )
{
	VkXrResources *xr = &vk.xr;

	if ( xr->virtualScreenFramebuffer != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, xr->virtualScreenFramebuffer, NULL );
		xr->virtualScreenFramebuffer = VK_NULL_HANDLE;
	}
	if ( xr->virtualScreenView != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, xr->virtualScreenView, NULL );
		xr->virtualScreenView = VK_NULL_HANDLE;
	}
	if ( xr->virtualScreenArrayView != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, xr->virtualScreenArrayView, NULL );
		xr->virtualScreenArrayView = VK_NULL_HANDLE;
	}
	if ( xr->virtualScreenImage != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, xr->virtualScreenImage, NULL );
		xr->virtualScreenImage = VK_NULL_HANDLE;
	}
	if ( xr->virtualScreenMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, xr->virtualScreenMemory, NULL );
		xr->virtualScreenMemory = VK_NULL_HANDLE;
	}
	if ( xr->virtualScreenSampler != VK_NULL_HANDLE ) {
		qvkDestroySampler( vk.device, xr->virtualScreenSampler, NULL );
		xr->virtualScreenSampler = VK_NULL_HANDLE;
	}
	// Descriptors are freed when pool is reset
	xr->virtualScreenDescriptor = VK_NULL_HANDLE;
	xr->virtualScreenMirrorDescriptor = VK_NULL_HANDLE;
}


/*
 * vk_create_virtual_screen_buffer - Create virtual screen buffer resources
 *
 * Creates the VkImage, VkImageView, VkSampler, and VkDescriptorSet for the
 * virtual screen texture. This texture is used to display menus, console,
 * and first-person follow mode as a 3D curved quad in VR.
 *
 * The virtual screen texture is:
 * - Single layer (not multiview) - it's a 2D texture
 * - Same resolution as XR swapchain (for quality)
 * - Transfer destination (for blit from scene)
 * - Sampled (for textured mesh rendering)
 *
 * Note: We don't need a framebuffer for this - we blit to it, not render to it.
 */
qboolean vk_create_virtual_screen_buffer( void )
{
	VkXrResources *xr = &vk.xr;
	VkImageCreateInfo imageCI;
	VkMemoryRequirements memReqs;
	VkMemoryAllocateInfo allocInfo;
	VkImageViewCreateInfo viewCI;
	VkSamplerCreateInfo samplerCI;
	uint32_t memoryType;

	// Clean up any existing virtual screen buffer resources first
	vk_destroy_virtual_screen_buffer();

	// Need XR dimensions to create the buffer
	if ( xr->width == 0 || xr->height == 0 ) {
		ri.Printf( PRINT_WARNING, "vk_create_virtual_screen_buffer: XR dimensions not set\n" );
		return qfalse;
	}

	ri.Printf( PRINT_ALL, "Creating virtual screen buffer (%dx%d, format=0x%x)...\n",
		xr->width, xr->height, vk.color_format );

	// 1. Create virtual screen image
	// - Transfer destination: for blitting scene content
	// - Sampled: for texture sampling when rendering the 3D mesh
	Com_Memset( &imageCI, 0, sizeof( imageCI ) );
	imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCI.imageType = VK_IMAGE_TYPE_2D;
	imageCI.format = vk.color_format;  // Same as FBO/swapchain for consistency
	imageCI.extent.width = xr->width;
	imageCI.extent.height = xr->height;
	imageCI.extent.depth = 1;
	imageCI.mipLevels = 1;
	imageCI.arrayLayers = 1;
	imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCI.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VK_CHECK( qvkCreateImage( vk.device, &imageCI, NULL, &xr->virtualScreenImage ) );
	SET_OBJECT_NAME( xr->virtualScreenImage, "Virtual screen image", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );

	// 2. Allocate memory
	qvkGetImageMemoryRequirements( vk.device, xr->virtualScreenImage, &memReqs );

	memoryType = find_memory_type(
		memReqs.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

	Com_Memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReqs.size;
	allocInfo.memoryTypeIndex = memoryType;

	VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &xr->virtualScreenMemory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, xr->virtualScreenImage, xr->virtualScreenMemory, 0 ) );

	// 3. Create image view (2D for shader sampling)
	// Use UNORM format to prevent automatic sRGB→linear conversion when sampling.
	// For r_fbo=0: source is sRGB, we want passthrough (no conversion)
	// For r_fbo=1: source is linear HDR, UNORM sampling is fine (shader handles gamma)
	Com_Memset( &viewCI, 0, sizeof( viewCI ) );
	viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCI.image = xr->virtualScreenImage;
	viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewCI.format = vk_get_unorm_format( vk.color_format );
	viewCI.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCI.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCI.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCI.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewCI.subresourceRange.baseMipLevel = 0;
	viewCI.subresourceRange.levelCount = 1;
	viewCI.subresourceRange.baseArrayLayer = 0;
	viewCI.subresourceRange.layerCount = 1;

	VK_CHECK( qvkCreateImageView( vk.device, &viewCI, NULL, &xr->virtualScreenView ) );
	SET_OBJECT_NAME( xr->virtualScreenView, "Virtual screen image view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );

	// 4. Create sampler (linear filtering, clamp to edge)
	Com_Memset( &samplerCI, 0, sizeof( samplerCI ) );
	samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCI.magFilter = VK_FILTER_LINEAR;
	samplerCI.minFilter = VK_FILTER_LINEAR;
	samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCI.mipLodBias = 0.0f;
	samplerCI.anisotropyEnable = VK_FALSE;
	samplerCI.maxAnisotropy = 1.0f;
	samplerCI.compareEnable = VK_FALSE;
	samplerCI.minLod = 0.0f;
	samplerCI.maxLod = 0.0f;
	samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
	samplerCI.unnormalizedCoordinates = VK_FALSE;

	VK_CHECK( qvkCreateSampler( vk.device, &samplerCI, NULL, &xr->virtualScreenSampler ) );
	SET_OBJECT_NAME( xr->virtualScreenSampler, "Virtual screen sampler", VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT );

	// 5. Create descriptor set for sampling virtual screen texture
	{
		VkDescriptorSetAllocateInfo descAllocInfo;
		VkDescriptorImageInfo imageInfo;
		VkWriteDescriptorSet descriptorWrite;

		Com_Memset( &descAllocInfo, 0, sizeof( descAllocInfo ) );
		descAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descAllocInfo.descriptorPool = vk.descriptor_pool;
		descAllocInfo.descriptorSetCount = 1;
		descAllocInfo.pSetLayouts = &vk.set_layout_sampler;

		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &descAllocInfo, &xr->virtualScreenDescriptor ) );

		// Update descriptor to point to virtual screen image
		Com_Memset( &imageInfo, 0, sizeof( imageInfo ) );
		imageInfo.sampler = xr->virtualScreenSampler;
		imageInfo.imageView = xr->virtualScreenView;
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		Com_Memset( &descriptorWrite, 0, sizeof( descriptorWrite ) );
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = xr->virtualScreenDescriptor;
		descriptorWrite.dstBinding = 0;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pImageInfo = &imageInfo;

		qvkUpdateDescriptorSets( vk.device, 1, &descriptorWrite, 0, NULL );
	}

	// 5b. Create 2D_ARRAY view for desktop mirror shader (samples layer 0)
	{
		VkImageViewCreateInfo arrayViewCI;
		Com_Memset( &arrayViewCI, 0, sizeof( arrayViewCI ) );
		arrayViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		arrayViewCI.image = xr->virtualScreenImage;
		arrayViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		arrayViewCI.format = vk_get_unorm_format( vk.color_format );
		arrayViewCI.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		arrayViewCI.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		arrayViewCI.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		arrayViewCI.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		arrayViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		arrayViewCI.subresourceRange.baseMipLevel = 0;
		arrayViewCI.subresourceRange.levelCount = 1;
		arrayViewCI.subresourceRange.baseArrayLayer = 0;
		arrayViewCI.subresourceRange.layerCount = 1;

		VK_CHECK( qvkCreateImageView( vk.device, &arrayViewCI, NULL, &xr->virtualScreenArrayView ) );
		SET_OBJECT_NAME( xr->virtualScreenArrayView, "Virtual screen array view (for mirror)", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	}

	// 5c. Create descriptor set for desktop mirror shader (uses array view)
	{
		VkDescriptorSetAllocateInfo descAllocInfo;
		VkDescriptorImageInfo imageInfo;
		VkWriteDescriptorSet descriptorWrite;

		Com_Memset( &descAllocInfo, 0, sizeof( descAllocInfo ) );
		descAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descAllocInfo.descriptorPool = vk.descriptor_pool;
		descAllocInfo.descriptorSetCount = 1;
		descAllocInfo.pSetLayouts = &vk.set_layout_sampler;

		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &descAllocInfo, &xr->virtualScreenMirrorDescriptor ) );

		Com_Memset( &imageInfo, 0, sizeof( imageInfo ) );
		imageInfo.sampler = xr->virtualScreenSampler;  // Reuse same sampler
		imageInfo.imageView = xr->virtualScreenArrayView;
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		Com_Memset( &descriptorWrite, 0, sizeof( descriptorWrite ) );
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = xr->virtualScreenMirrorDescriptor;
		descriptorWrite.dstBinding = 0;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pImageInfo = &imageInfo;

		qvkUpdateDescriptorSets( vk.device, 1, &descriptorWrite, 0, NULL );
	}

	// 6. Initialize virtual screen image (transition to SHADER_READ_ONLY and clear)
	{
		VkCommandBuffer cmdBuf;
		VkCommandBufferAllocateInfo cmdAllocInfo;
		VkCommandBufferBeginInfo beginInfo;
		VkSubmitInfo submitInfo;
		VkClearColorValue clearColor;
		VkImageSubresourceRange range;

		Com_Memset( &cmdAllocInfo, 0, sizeof( cmdAllocInfo ) );
		cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdAllocInfo.commandPool = vk.command_pool;
		cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmdAllocInfo.commandBufferCount = 1;

		VK_CHECK( qvkAllocateCommandBuffers( vk.device, &cmdAllocInfo, &cmdBuf ) );

		Com_Memset( &beginInfo, 0, sizeof( beginInfo ) );
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		VK_CHECK( qvkBeginCommandBuffer( cmdBuf, &beginInfo ) );

		// Transition to TRANSFER_DST for clearing
		record_image_layout_transition( cmdBuf, xr->virtualScreenImage, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );

		// Clear to black
		clearColor.float32[0] = 0.0f;
		clearColor.float32[1] = 0.0f;
		clearColor.float32[2] = 0.0f;
		clearColor.float32[3] = 1.0f;

		range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		range.baseMipLevel = 0;
		range.levelCount = 1;
		range.baseArrayLayer = 0;
		range.layerCount = 1;

		qvkCmdClearColorImage( cmdBuf, xr->virtualScreenImage,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range );

		// Transition to SHADER_READ_ONLY_OPTIMAL for sampling
		record_image_layout_transition( cmdBuf, xr->virtualScreenImage, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );

		VK_CHECK( qvkEndCommandBuffer( cmdBuf ) );

		Com_Memset( &submitInfo, 0, sizeof( submitInfo ) );
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmdBuf;

		VK_CHECK( qvkQueueSubmit( vk.queue, 1, &submitInfo, VK_NULL_HANDLE ) );
		VK_CHECK( qvkQueueWaitIdle( vk.queue ) );

		qvkFreeCommandBuffers( vk.device, vk.command_pool, 1, &cmdBuf );
	}

	ri.Printf( PRINT_ALL, "...virtual screen buffer created\n" );
	return qtrue;
}


/*
 * vk_destroy_virtual_screen_meshes - Destroy virtual screen mesh resources
 */
static void vk_destroy_virtual_screen_meshes( void )
{
	VkXrResources *xr = &vk.xr;

	// Cylinder mesh
	if ( xr->mesh.cylinderVertexBuffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, xr->mesh.cylinderVertexBuffer, NULL );
		xr->mesh.cylinderVertexBuffer = VK_NULL_HANDLE;
	}
	if ( xr->mesh.cylinderIndexBuffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, xr->mesh.cylinderIndexBuffer, NULL );
		xr->mesh.cylinderIndexBuffer = VK_NULL_HANDLE;
	}
	if ( xr->mesh.cylinderMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, xr->mesh.cylinderMemory, NULL );
		xr->mesh.cylinderMemory = VK_NULL_HANDLE;
	}
	if ( xr->mesh.cylinderIndexMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, xr->mesh.cylinderIndexMemory, NULL );
		xr->mesh.cylinderIndexMemory = VK_NULL_HANDLE;
	}
	xr->mesh.cylinderVertexCount = 0;
	xr->mesh.cylinderIndexCount = 0;

	// Quad mesh
	if ( xr->mesh.quadVertexBuffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, xr->mesh.quadVertexBuffer, NULL );
		xr->mesh.quadVertexBuffer = VK_NULL_HANDLE;
	}
	if ( xr->mesh.quadIndexBuffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, xr->mesh.quadIndexBuffer, NULL );
		xr->mesh.quadIndexBuffer = VK_NULL_HANDLE;
	}
	if ( xr->mesh.quadMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, xr->mesh.quadMemory, NULL );
		xr->mesh.quadMemory = VK_NULL_HANDLE;
	}
	if ( xr->mesh.quadIndexMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, xr->mesh.quadIndexMemory, NULL );
		xr->mesh.quadIndexMemory = VK_NULL_HANDLE;
	}
	xr->mesh.quadVertexCount = 0;
	xr->mesh.quadIndexCount = 0;
}


/*
 * Generate cylinder section mesh for virtual screen
 *
 * Creates a curved cylinder section centered at origin, curving toward -Z.
 * After model transform (placed in front of user), this creates a screen
 * that wraps around the user (center furthest, edges closer).
 *
 * Vertex format: vec3 position, vec2 texcoord (5 floats per vertex)
 */
static void vk_generate_cylinder_mesh(
	float radius, float height, float arcAngle, int segments,
	float **outVertices, uint32_t *outVertexCount,
	uint16_t **outIndices, uint32_t *outIndexCount )
{
	int vertsPerRow = segments + 1;
	int totalVerts = vertsPerRow * 2;
	int floatsPerVert = 5;
	int totalIndices = segments * 6;
	int i;
	float *vptr;
	uint16_t *iptr;
	float startAngle, dTheta;

	*outVertexCount = totalVerts;
	*outVertices = (float *)ri.Malloc( totalVerts * floatsPerVert * sizeof( float ) );

	*outIndexCount = totalIndices;
	*outIndices = (uint16_t *)ri.Malloc( totalIndices * sizeof( uint16_t ) );

	vptr = *outVertices;
	iptr = *outIndices;

	startAngle = -arcAngle * 0.5f;
	dTheta = arcAngle / (float)segments;

	// Generate vertices (two rows: top and bottom)
	for ( i = 0; i <= segments; i++ ) {
		float theta = startAngle + i * dTheta;
		float x = radius * sinf( theta );
		float z = -radius * cosf( theta );  // Negative Z - center is furthest from origin
		float u = (float)i / (float)segments;

		// Top vertex
		*vptr++ = x;
		*vptr++ = height * 0.5f;
		*vptr++ = z;
		*vptr++ = u;
		*vptr++ = 1.0f;

		// Bottom vertex
		*vptr++ = x;
		*vptr++ = -height * 0.5f;
		*vptr++ = z;
		*vptr++ = u;
		*vptr++ = 0.0f;
	}

	// Generate indices (two triangles per segment)
	for ( i = 0; i < segments; i++ ) {
		int top0 = i * 2;
		int bot0 = top0 + 1;
		int top1 = top0 + 2;
		int bot1 = bot0 + 2;

		// First triangle
		*iptr++ = (uint16_t)top0;
		*iptr++ = (uint16_t)top1;
		*iptr++ = (uint16_t)bot0;

		// Second triangle
		*iptr++ = (uint16_t)bot0;
		*iptr++ = (uint16_t)top1;
		*iptr++ = (uint16_t)bot1;
	}
}


/*
 * Create a Vulkan buffer with device-local memory
 */
static qboolean vk_create_device_local_buffer(
	VkBuffer *buffer, VkDeviceMemory *memory,
	const void *data, VkDeviceSize size, VkBufferUsageFlags usage,
	const char *debugName )
{
	VkBufferCreateInfo bufferCI;
	VkMemoryRequirements memReqs;
	VkMemoryAllocateInfo allocInfo;
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;
	VkCommandBuffer cmdBuf;
	VkCommandBufferAllocateInfo cmdAllocInfo;
	VkCommandBufferBeginInfo beginInfo;
	VkBufferCopy copyRegion;
	VkSubmitInfo submitInfo;
	uint32_t memoryType;
	void *mapped;

	// Create staging buffer (host-visible)
	Com_Memset( &bufferCI, 0, sizeof( bufferCI ) );
	bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCI.size = size;
	bufferCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VK_CHECK( qvkCreateBuffer( vk.device, &bufferCI, NULL, &stagingBuffer ) );

	qvkGetBufferMemoryRequirements( vk.device, stagingBuffer, &memReqs );
	memoryType = find_memory_type( memReqs.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

	Com_Memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReqs.size;
	allocInfo.memoryTypeIndex = memoryType;

	VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &stagingMemory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, stagingBuffer, stagingMemory, 0 ) );

	// Copy data to staging buffer
	VK_CHECK( qvkMapMemory( vk.device, stagingMemory, 0, size, 0, &mapped ) );
	Com_Memcpy( mapped, data, size );
	qvkUnmapMemory( vk.device, stagingMemory );

	// Create device-local buffer
	bufferCI.usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	VK_CHECK( qvkCreateBuffer( vk.device, &bufferCI, NULL, buffer ) );
	SET_OBJECT_NAME( *buffer, debugName, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );

	qvkGetBufferMemoryRequirements( vk.device, *buffer, &memReqs );
	memoryType = find_memory_type( memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

	allocInfo.allocationSize = memReqs.size;
	allocInfo.memoryTypeIndex = memoryType;

	VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, memory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, *buffer, *memory, 0 ) );

	// Copy from staging to device-local
	Com_Memset( &cmdAllocInfo, 0, sizeof( cmdAllocInfo ) );
	cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdAllocInfo.commandPool = vk.command_pool;
	cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdAllocInfo.commandBufferCount = 1;

	VK_CHECK( qvkAllocateCommandBuffers( vk.device, &cmdAllocInfo, &cmdBuf ) );

	Com_Memset( &beginInfo, 0, sizeof( beginInfo ) );
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_CHECK( qvkBeginCommandBuffer( cmdBuf, &beginInfo ) );

	Com_Memset( &copyRegion, 0, sizeof( copyRegion ) );
	copyRegion.size = size;
	qvkCmdCopyBuffer( cmdBuf, stagingBuffer, *buffer, 1, &copyRegion );

	VK_CHECK( qvkEndCommandBuffer( cmdBuf ) );

	Com_Memset( &submitInfo, 0, sizeof( submitInfo ) );
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmdBuf;

	VK_CHECK( qvkQueueSubmit( vk.queue, 1, &submitInfo, VK_NULL_HANDLE ) );
	VK_CHECK( qvkQueueWaitIdle( vk.queue ) );

	qvkFreeCommandBuffers( vk.device, vk.command_pool, 1, &cmdBuf );

	// Cleanup staging resources
	qvkDestroyBuffer( vk.device, stagingBuffer, NULL );
	qvkFreeMemory( vk.device, stagingMemory, NULL );

	return qtrue;
}


/*
 * vk_create_virtual_screen_meshes - Create mesh geometry for virtual screen
 *
 * Creates:
 * 1. Cylinder mesh for curved virtual screen (64 segments, 90° arc)
 * 2. Quad mesh for floor grid
 */
qboolean vk_create_virtual_screen_meshes( void )
{
	VkXrResources *xr = &vk.xr;
	float *cylinderVertices = NULL;
	uint16_t *cylinderIndices = NULL;
	VkDeviceSize cylinderVertexSize, cylinderIndexSize;
	VkDeviceSize quadVertexSize, quadIndexSize;

	// Floor grid quad (unit quad, scaled by shader)
	// Position (3) + texcoord (2) = 5 floats per vertex
	static const float quadVertices[] = {
		// position            uv
		 0.5f,  0.5f, 0.0f,    1.0f, 1.0f,  // top right
		 0.5f, -0.5f, 0.0f,    1.0f, 0.0f,  // bottom right
		-0.5f, -0.5f, 0.0f,    0.0f, 0.0f,  // bottom left
		-0.5f,  0.5f, 0.0f,    0.0f, 1.0f,  // top left
	};
	static const uint16_t quadIndices[] = {
		0, 1, 3,   // first triangle
		1, 2, 3,   // second triangle
	};

	// Clean up any existing mesh resources
	vk_destroy_virtual_screen_meshes();

	ri.Printf( PRINT_ALL, "Creating virtual screen meshes...\n" );

	// Generate cylinder mesh (radius=2.5m, height=3.5m, 90° arc, 64 segments)
	// These values match the OpenGL implementation
	vk_generate_cylinder_mesh( 2.5f, 3.5f, M_PI / 2.0f, 64,
		&cylinderVertices, &xr->mesh.cylinderVertexCount,
		&cylinderIndices, &xr->mesh.cylinderIndexCount );

	cylinderVertexSize = xr->mesh.cylinderVertexCount * 5 * sizeof( float );
	cylinderIndexSize = xr->mesh.cylinderIndexCount * sizeof( uint16_t );

	ri.Printf( PRINT_ALL, "  Cylinder mesh: %d vertices, %d indices\n",
		xr->mesh.cylinderVertexCount, xr->mesh.cylinderIndexCount );

	// Create cylinder vertex buffer
	if ( !vk_create_device_local_buffer(
		&xr->mesh.cylinderVertexBuffer, &xr->mesh.cylinderMemory,
		cylinderVertices, cylinderVertexSize,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		"Virtual screen cylinder vertex buffer" ) )
	{
		ri.Printf( PRINT_ERROR, "Failed to create cylinder vertex buffer\n" );
		ri.Free( cylinderVertices );
		ri.Free( cylinderIndices );
		return qfalse;
	}

	// Create cylinder index buffer (shares memory allocation: append after vertex data)
	// For simplicity, we use a separate allocation
	{
		VkBufferCreateInfo bufferCI;
		VkMemoryRequirements memReqs;
		VkMemoryAllocateInfo allocInfo;
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingMemory;
		VkCommandBuffer cmdBuf;
		VkCommandBufferAllocateInfo cmdAllocInfo;
		VkCommandBufferBeginInfo beginInfo;
		VkBufferCopy copyRegion;
		VkSubmitInfo submitInfo;
		uint32_t memoryType;
		void *mapped;

		// Create staging buffer
		Com_Memset( &bufferCI, 0, sizeof( bufferCI ) );
		bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferCI.size = cylinderIndexSize;
		bufferCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VK_CHECK( qvkCreateBuffer( vk.device, &bufferCI, NULL, &stagingBuffer ) );

		qvkGetBufferMemoryRequirements( vk.device, stagingBuffer, &memReqs );
		memoryType = find_memory_type( memReqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

		Com_Memset( &allocInfo, 0, sizeof( allocInfo ) );
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memReqs.size;
		allocInfo.memoryTypeIndex = memoryType;

		VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &stagingMemory ) );
		VK_CHECK( qvkBindBufferMemory( vk.device, stagingBuffer, stagingMemory, 0 ) );

		VK_CHECK( qvkMapMemory( vk.device, stagingMemory, 0, cylinderIndexSize, 0, &mapped ) );
		Com_Memcpy( mapped, cylinderIndices, cylinderIndexSize );
		qvkUnmapMemory( vk.device, stagingMemory );

		// Create device-local index buffer
		bufferCI.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		VK_CHECK( qvkCreateBuffer( vk.device, &bufferCI, NULL, &xr->mesh.cylinderIndexBuffer ) );
		SET_OBJECT_NAME( xr->mesh.cylinderIndexBuffer, "Virtual screen cylinder index buffer", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );

		qvkGetBufferMemoryRequirements( vk.device, xr->mesh.cylinderIndexBuffer, &memReqs );
		memoryType = find_memory_type( memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

		allocInfo.allocationSize = memReqs.size;
		allocInfo.memoryTypeIndex = memoryType;

		VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &xr->mesh.cylinderIndexMemory ) );
		VK_CHECK( qvkBindBufferMemory( vk.device, xr->mesh.cylinderIndexBuffer, xr->mesh.cylinderIndexMemory, 0 ) );

		// Copy from staging to device-local
		Com_Memset( &cmdAllocInfo, 0, sizeof( cmdAllocInfo ) );
		cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdAllocInfo.commandPool = vk.command_pool;
		cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmdAllocInfo.commandBufferCount = 1;

		VK_CHECK( qvkAllocateCommandBuffers( vk.device, &cmdAllocInfo, &cmdBuf ) );

		Com_Memset( &beginInfo, 0, sizeof( beginInfo ) );
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		VK_CHECK( qvkBeginCommandBuffer( cmdBuf, &beginInfo ) );

		Com_Memset( &copyRegion, 0, sizeof( copyRegion ) );
		copyRegion.size = cylinderIndexSize;
		qvkCmdCopyBuffer( cmdBuf, stagingBuffer, xr->mesh.cylinderIndexBuffer, 1, &copyRegion );

		VK_CHECK( qvkEndCommandBuffer( cmdBuf ) );

		Com_Memset( &submitInfo, 0, sizeof( submitInfo ) );
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmdBuf;

		VK_CHECK( qvkQueueSubmit( vk.queue, 1, &submitInfo, VK_NULL_HANDLE ) );
		VK_CHECK( qvkQueueWaitIdle( vk.queue ) );

		qvkFreeCommandBuffers( vk.device, vk.command_pool, 1, &cmdBuf );
		qvkDestroyBuffer( vk.device, stagingBuffer, NULL );
		qvkFreeMemory( vk.device, stagingMemory, NULL );
	}

	ri.Free( cylinderVertices );
	ri.Free( cylinderIndices );

	// Create quad mesh for floor grid
	xr->mesh.quadVertexCount = 4;
	xr->mesh.quadIndexCount = 6;
	quadVertexSize = xr->mesh.quadVertexCount * 5 * sizeof( float );
	quadIndexSize = xr->mesh.quadIndexCount * sizeof( uint16_t );

	ri.Printf( PRINT_ALL, "  Floor quad mesh: %d vertices, %d indices\n",
		xr->mesh.quadVertexCount, xr->mesh.quadIndexCount );

	// Create quad vertex buffer
	if ( !vk_create_device_local_buffer(
		&xr->mesh.quadVertexBuffer, &xr->mesh.quadMemory,
		quadVertices, quadVertexSize,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		"Floor grid quad vertex buffer" ) )
	{
		ri.Printf( PRINT_ERROR, "Failed to create quad vertex buffer\n" );
		return qfalse;
	}

	// Create quad index buffer
	{
		VkBufferCreateInfo bufferCI;
		VkMemoryRequirements memReqs;
		VkMemoryAllocateInfo allocInfo;
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingMemory;
		VkCommandBuffer cmdBuf;
		VkCommandBufferAllocateInfo cmdAllocInfo;
		VkCommandBufferBeginInfo beginInfo;
		VkBufferCopy copyRegion;
		VkSubmitInfo submitInfo;
		uint32_t memoryType;
		void *mapped;

		Com_Memset( &bufferCI, 0, sizeof( bufferCI ) );
		bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferCI.size = quadIndexSize;
		bufferCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VK_CHECK( qvkCreateBuffer( vk.device, &bufferCI, NULL, &stagingBuffer ) );

		qvkGetBufferMemoryRequirements( vk.device, stagingBuffer, &memReqs );
		memoryType = find_memory_type( memReqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

		Com_Memset( &allocInfo, 0, sizeof( allocInfo ) );
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memReqs.size;
		allocInfo.memoryTypeIndex = memoryType;

		VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &stagingMemory ) );
		VK_CHECK( qvkBindBufferMemory( vk.device, stagingBuffer, stagingMemory, 0 ) );

		VK_CHECK( qvkMapMemory( vk.device, stagingMemory, 0, quadIndexSize, 0, &mapped ) );
		Com_Memcpy( mapped, quadIndices, quadIndexSize );
		qvkUnmapMemory( vk.device, stagingMemory );

		bufferCI.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		VK_CHECK( qvkCreateBuffer( vk.device, &bufferCI, NULL, &xr->mesh.quadIndexBuffer ) );
		SET_OBJECT_NAME( xr->mesh.quadIndexBuffer, "Floor grid quad index buffer", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );

		qvkGetBufferMemoryRequirements( vk.device, xr->mesh.quadIndexBuffer, &memReqs );
		memoryType = find_memory_type( memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

		allocInfo.allocationSize = memReqs.size;
		allocInfo.memoryTypeIndex = memoryType;

		VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &xr->mesh.quadIndexMemory ) );
		VK_CHECK( qvkBindBufferMemory( vk.device, xr->mesh.quadIndexBuffer, xr->mesh.quadIndexMemory, 0 ) );

		Com_Memset( &cmdAllocInfo, 0, sizeof( cmdAllocInfo ) );
		cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdAllocInfo.commandPool = vk.command_pool;
		cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmdAllocInfo.commandBufferCount = 1;

		VK_CHECK( qvkAllocateCommandBuffers( vk.device, &cmdAllocInfo, &cmdBuf ) );

		Com_Memset( &beginInfo, 0, sizeof( beginInfo ) );
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		VK_CHECK( qvkBeginCommandBuffer( cmdBuf, &beginInfo ) );

		Com_Memset( &copyRegion, 0, sizeof( copyRegion ) );
		copyRegion.size = quadIndexSize;
		qvkCmdCopyBuffer( cmdBuf, stagingBuffer, xr->mesh.quadIndexBuffer, 1, &copyRegion );

		VK_CHECK( qvkEndCommandBuffer( cmdBuf ) );

		Com_Memset( &submitInfo, 0, sizeof( submitInfo ) );
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmdBuf;

		VK_CHECK( qvkQueueSubmit( vk.queue, 1, &submitInfo, VK_NULL_HANDLE ) );
		VK_CHECK( qvkQueueWaitIdle( vk.queue ) );

		qvkFreeCommandBuffers( vk.device, vk.command_pool, 1, &cmdBuf );
		qvkDestroyBuffer( vk.device, stagingBuffer, NULL );
		qvkFreeMemory( vk.device, stagingMemory, NULL );
	}

	ri.Printf( PRINT_ALL, "...virtual screen meshes created\n" );
	return qtrue;
}


/*
 * vk_destroy_virtual_screen_pipelines - Destroy virtual screen pipelines
 */
static void vk_destroy_virtual_screen_pipelines( void )
{
	VkXrResources *xr = &vk.xr;

	if ( xr->virtualScreenPipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, xr->virtualScreenPipeline, NULL );
		xr->virtualScreenPipeline = VK_NULL_HANDLE;
	}
	if ( xr->floorGridPipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, xr->floorGridPipeline, NULL );
		xr->floorGridPipeline = VK_NULL_HANDLE;
	}
}


/*
 * vk_create_virtual_screen_pipelines - Create pipelines for virtual screen rendering
 *
 * Creates:
 * 1. Virtual screen pipeline - textured cylinder with alpha blending
 * 2. Floor grid pipeline - procedural grid with alpha blending
 *
 * Both use multiview for stereo rendering via the split transform (64-byte
 * mono modelview push + per-view eyeProj UBO at set 0 binding 1).
 */
qboolean vk_create_virtual_screen_pipelines( void )
{
	VkXrResources *xr = &vk.xr;
	VkPipelineShaderStageCreateInfo shader_stages[2];
	VkPipelineVertexInputStateCreateInfo vertex_input_state;
	VkVertexInputBindingDescription vertex_binding;
	VkVertexInputAttributeDescription vertex_attrs[2];
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
	VkPipelineRasterizationStateCreateInfo rasterization_state;
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state;
	VkPipelineViewportStateCreateInfo viewport_state;
	VkPipelineMultisampleStateCreateInfo multisample_state;
	VkPipelineColorBlendStateCreateInfo blend_state;
	VkPipelineColorBlendAttachmentState attachment_blend_state;
	VkGraphicsPipelineCreateInfo create_info;
	VkViewport viewport;
	VkRect2D scissor;
	uint32_t width, height;

	// Clean up any existing pipelines
	vk_destroy_virtual_screen_pipelines();

	// Prerequisites check
	if ( xr->width == 0 || xr->height == 0 ) {
		ri.Printf( PRINT_WARNING, "vk_create_virtual_screen_pipelines: XR dimensions not set\n" );
		return qfalse;
	}

	if ( vk.render_pass.main == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_WARNING, "vk_create_virtual_screen_pipelines: Main render pass not created\n" );
		return qfalse;
	}

	width = xr->width;
	height = xr->height;

	ri.Printf( PRINT_ALL, "Creating virtual screen pipelines (%dx%d)...\n", width, height );

	// Common vertex input state: position (vec3) + texcoord (vec2)
	vertex_binding.binding = 0;
	vertex_binding.stride = 5 * sizeof( float );  // 3 floats pos + 2 floats uv
	vertex_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	vertex_attrs[0].location = 0;  // position
	vertex_attrs[0].binding = 0;
	vertex_attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	vertex_attrs[0].offset = 0;

	vertex_attrs[1].location = 1;  // texcoord
	vertex_attrs[1].binding = 0;
	vertex_attrs[1].format = VK_FORMAT_R32G32_SFLOAT;
	vertex_attrs[1].offset = 3 * sizeof( float );

	Com_Memset( &vertex_input_state, 0, sizeof( vertex_input_state ) );
	vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_state.vertexBindingDescriptionCount = 1;
	vertex_input_state.pVertexBindingDescriptions = &vertex_binding;
	vertex_input_state.vertexAttributeDescriptionCount = 2;
	vertex_input_state.pVertexAttributeDescriptions = vertex_attrs;

	// Common input assembly state
	Com_Memset( &input_assembly_state, 0, sizeof( input_assembly_state ) );
	input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_assembly_state.primitiveRestartEnable = VK_FALSE;

	// Common rasterization state
	Com_Memset( &rasterization_state, 0, sizeof( rasterization_state ) );
	rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization_state.depthClampEnable = VK_FALSE;
	rasterization_state.rasterizerDiscardEnable = VK_FALSE;
	rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
	rasterization_state.cullMode = VK_CULL_MODE_NONE;  // No culling for virtual screen
	rasterization_state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterization_state.depthBiasEnable = VK_FALSE;
	rasterization_state.lineWidth = 1.0f;

	// Common multisample state - virtual screen render pass uses no MSAA
	Com_Memset( &multisample_state, 0, sizeof( multisample_state ) );
	multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;  // Virtual screen: no MSAA
	multisample_state.sampleShadingEnable = VK_FALSE;
	multisample_state.minSampleShading = 1.0f;

	// Depth/stencil state - depth test enabled, write disabled
	Com_Memset( &depth_stencil_state, 0, sizeof( depth_stencil_state ) );
	depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
#ifdef USE_REVERSED_DEPTH
	depth_stencil_state.depthTestEnable = VK_TRUE;
	depth_stencil_state.depthWriteEnable = VK_FALSE;
	depth_stencil_state.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
#else
	depth_stencil_state.depthTestEnable = VK_TRUE;
	depth_stencil_state.depthWriteEnable = VK_FALSE;
	depth_stencil_state.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
#endif

	// Blend state - alpha blending
	Com_Memset( &attachment_blend_state, 0, sizeof( attachment_blend_state ) );
	attachment_blend_state.blendEnable = VK_TRUE;
	attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	attachment_blend_state.colorBlendOp = VK_BLEND_OP_ADD;
	attachment_blend_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	attachment_blend_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	attachment_blend_state.alphaBlendOp = VK_BLEND_OP_ADD;
	attachment_blend_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
	                                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	Com_Memset( &blend_state, 0, sizeof( blend_state ) );
	blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.logicOpEnable = VK_FALSE;
	blend_state.attachmentCount = 1;
	blend_state.pAttachments = &attachment_blend_state;

	// Viewport state
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)width;
	viewport.height = (float)height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = width;
	scissor.extent.height = height;

	Com_Memset( &viewport_state, 0, sizeof( viewport_state ) );
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &scissor;

	// Common pipeline create info template
	Com_Memset( &create_info, 0, sizeof( create_info ) );
	create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	create_info.stageCount = 2;
	create_info.pStages = shader_stages;
	create_info.pVertexInputState = &vertex_input_state;
	create_info.pInputAssemblyState = &input_assembly_state;
	create_info.pViewportState = &viewport_state;
	create_info.pRasterizationState = &rasterization_state;
	create_info.pMultisampleState = &multisample_state;
	create_info.pDepthStencilState = &depth_stencil_state;
	create_info.pColorBlendState = &blend_state;
	create_info.pDynamicState = NULL;
	create_info.layout = vk.pipeline_layout_virtual_screen;  // set 0 uniform+eyeProj, set 1 sampler, 64-byte push
	create_info.renderPass = vk.render_pass.virtualScreen;  // Dedicated render pass (no MSAA)
	create_info.subpass = 0;
	create_info.basePipelineHandle = VK_NULL_HANDLE;
	create_info.basePipelineIndex = -1;

	// =====================================================================
	// 1. Virtual Screen Pipeline (textured cylinder)
	// =====================================================================
	{
		// Inverse gamma (1/2.2) counteracts the sRGB encode on the output attachment
		struct {
			float gamma;
			float overbright;
		} vs_spec_data;
		VkSpecializationMapEntry vs_spec_entries[2];
		VkSpecializationInfo vs_spec_info;

		vs_spec_data.gamma = 1.0f / 2.2f;
		vs_spec_data.overbright = 1.0f;

		vs_spec_entries[0].constantID = 0;
		vs_spec_entries[0].offset = 0;  // gamma is first member
		vs_spec_entries[0].size = sizeof( float );

		vs_spec_entries[1].constantID = 1;
		vs_spec_entries[1].offset = sizeof( float );  // overbright is after gamma
		vs_spec_entries[1].size = sizeof( float );

		vs_spec_info.mapEntryCount = 2;
		vs_spec_info.pMapEntries = vs_spec_entries;
		vs_spec_info.dataSize = sizeof( vs_spec_data );
		vs_spec_info.pData = &vs_spec_data;

		set_shader_stage_desc( &shader_stages[0], VK_SHADER_STAGE_VERTEX_BIT,
			vk.modules.virtualscreen_vs, "main" );
		set_shader_stage_desc( &shader_stages[1], VK_SHADER_STAGE_FRAGMENT_BIT,
			vk.modules.virtualscreen_fs, "main" );
		shader_stages[1].pSpecializationInfo = &vs_spec_info;

		VK_CHECK( qvkCreateGraphicsPipelines( vk.device, vk.pipelineCache, 1,
			&create_info, NULL, &xr->virtualScreenPipeline ) );
		SET_OBJECT_NAME( xr->virtualScreenPipeline, "Virtual screen pipeline",
			VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
	}

	// =====================================================================
	// 2. Floor Grid Pipeline (procedural grid)
	// =====================================================================
	set_shader_stage_desc( &shader_stages[0], VK_SHADER_STAGE_VERTEX_BIT,
		vk.modules.floor_grid_vs, "main" );
	set_shader_stage_desc( &shader_stages[1], VK_SHADER_STAGE_FRAGMENT_BIT,
		vk.modules.floor_grid_fs, "main" );

	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, vk.pipelineCache, 1,
		&create_info, NULL, &xr->floorGridPipeline ) );
	SET_OBJECT_NAME( xr->floorGridPipeline, "Floor grid pipeline",
		VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );

	ri.Printf( PRINT_ALL, "...virtual screen pipelines created\n" );
	return qtrue;
}


/*
 * vk_render_virtual_screen - Draw the virtual screen (cylinder + floor grid)
 *
 * Called from vk_end_frame when virtual screen should be displayed.
 * Blits FBO content to virtual screen texture, then draws it as a cylinder.
 *
 * Parameters:
 *   eyeProj  - Per-eye projection pair (P_eye * eyeFromHead, XR meter space),
 *              shared by both meshes via set 0 binding 1
 *   screenMV - Mono model-view for the virtual screen cylinder/quad
 *   floorMV  - Mono model-view for the floor grid
 */
static void vk_render_virtual_screen( float eyeProj[2][16], float screenMV[16], float floorMV[16] )
{
	VkXrResources *xr = &vk.xr;
	uint32_t colorIndex = xr->colorIndex;
	VkImage srcImage;
	VkImageLayout srcLayout;
	VkImageBlit blit_region;
	VkDeviceSize zero_offset = 0;
	uint32_t eyeproj_offset;

	// Safety checks
	if ( !xr->initialized || xr->virtualScreenImage == VK_NULL_HANDLE ) {
		return;
	}
	if ( xr->virtualScreenPipeline == VK_NULL_HANDLE ||
		 xr->floorGridPipeline == VK_NULL_HANDLE ) {
		return;
	}
	if ( xr->mesh.cylinderVertexBuffer == VK_NULL_HANDLE ||
		 xr->mesh.quadVertexBuffer == VK_NULL_HANDLE ) {
		return;
	}

	// XR swapchain is in COLOR_ATTACHMENT_OPTIMAL (gamma render pass finalLayout)
	if ( !xr->colorInfo ) {
		return;
	}
	srcImage = xr->colorInfo->images[colorIndex];
	srcLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// 2. Transition source to TRANSFER_SRC
	record_image_layout_transition( vk.cmd->command_buffer,
		srcImage, VK_IMAGE_ASPECT_COLOR_BIT,
		srcLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0 );

	// 4. Transition virtual screen texture to TRANSFER_DST
	record_image_layout_transition( vk.cmd->command_buffer,
		xr->virtualScreenImage, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );

	// 5. Blit with 4:3 crop (center crop from source aspect to 4:3)
	// Calculate 4:3 region from source, accounting for optical center offset
	// Note: glConfig.vidWidth/Height = vk.xr.width/height in VR (set by VR_GetSupersampledResolution)
	int fbWidth = glConfig.vidWidth;
	int fbHeight = glConfig.vidHeight;
	int dstWidth = xr->width;
	int dstHeight = xr->height;

	int cropWidth, cropHeight, cropX, cropY;
	int heightFromWidth = (fbWidth * 3) / 4;   // 4:3 height if we use full width
	int widthFromHeight = (fbHeight * 4) / 3;  // 4:3 width if we use full height

	if ( heightFromWidth <= fbHeight ) {
		// Normal case: width-limited, full width fits with 4:3 height
		cropWidth = fbWidth;
		cropHeight = heightFromWidth;
		cropX = 0;
	} else {
		// Ultra-wide case: height-limited, constrain width to fit 4:3
		cropHeight = fbHeight;
		cropWidth = widthFromHeight;
		cropX = (fbWidth - cropWidth) / 2;  // Center horizontally
	}

	// Calculate vertical offset to center the crop on the optical center rather than
	// the geometric center. VR headsets have asymmetric FOV (more down-look than up-look).
	float geometricCenterY = fbHeight / 2.0f;
	float opticalOffset = 0.0f;

	// Get FOV angles from VR layer (stored in vr_clientinfo, accessible via extern)
	// tanUp and tanDown encode the asymmetric vertical FOV
	float tanUp = tanf( vr.fov_angle_up );
	float tanDown = tanf( vr.fov_angle_down );
	float tanHeight = tanUp - tanDown;

	if ( fabsf( tanHeight ) > 0.001f ) {
		float m9 = (tanUp + tanDown) / tanHeight;
		// Offset in virtual 480 coords: 240 * m9 (negative when optical center is above geometric)
		// Scale to framebuffer pixels: multiply by (cropHeight / 480)
		// Note: Vulkan Y=0 is at TOP (opposite of OpenGL), so we DON'T negate
		opticalOffset = 240.0f * m9 * (cropHeight / 480.0f);
	}

	// Center the crop around the optical center, not the geometric center
	cropY = (int)(geometricCenterY - cropHeight / 2.0f + opticalOffset);

	// Clamp to valid framebuffer bounds
	if ( cropY < 0 ) cropY = 0;
	if ( cropY + cropHeight > fbHeight ) {
		cropY = fbHeight - cropHeight;
	}

	Com_Memset( &blit_region, 0, sizeof( blit_region ) );
	blit_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blit_region.srcSubresource.layerCount = 1;
	blit_region.srcOffsets[0].x = cropX;
	blit_region.srcOffsets[0].y = cropY;
	blit_region.srcOffsets[1].x = cropX + cropWidth;
	blit_region.srcOffsets[1].y = cropY + cropHeight;
	blit_region.srcOffsets[1].z = 1;
	blit_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blit_region.dstSubresource.layerCount = 1;
	blit_region.dstOffsets[0].x = 0;
	blit_region.dstOffsets[0].y = 0;
	blit_region.dstOffsets[1].x = dstWidth;
	blit_region.dstOffsets[1].y = dstHeight;
	blit_region.dstOffsets[1].z = 1;

	qvkCmdBlitImage( vk.cmd->command_buffer,
		srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		xr->virtualScreenImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &blit_region, VK_FILTER_LINEAR );

	// 6. Transition virtual screen to SHADER_READ_ONLY for sampling
	record_image_layout_transition( vk.cmd->command_buffer,
		xr->virtualScreenImage, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );

	// 7. XR swapchain left in TRANSFER_SRC: virtual screen render pass uses
	// initialLayout=UNDEFINED and will clear+overwrite entirely

	// Route the virtual screen's own eyeProj pair through the standard
	// set 0 binding 1 machinery; save/restore the view-global so nothing
	// downstream observes virtual-screen matrices
	{
		float saved_eyeproj[2][16];

		Com_Memcpy( saved_eyeproj, vk_view_eyeproj, sizeof( saved_eyeproj ) );
		Com_Memcpy( vk_view_eyeproj, eyeProj, sizeof( saved_eyeproj ) );
		eyeproj_offset = VK_PushEyeProj();
		Com_Memcpy( vk_view_eyeproj, saved_eyeproj, sizeof( saved_eyeproj ) );
	}

	// 8. Clear XR swapchain and begin render pass for virtual screen drawing
	// Use virtual screen render pass to draw to XR swapchain with clear
	vk.renderWidth = xr->width;
	vk.renderHeight = xr->height;
	vk.renderScaleX = vk.renderScaleY = 1.0f;

	vk_begin_render_pass( vk.render_pass.virtualScreen, xr->framebuffers[colorIndex],
		qtrue, xr->width, xr->height );

	// 9. Draw floor grid and screen mesh; on ring overflow keep the cleared
	// pass but skip the draws: the resize is scheduled and the ring
	// self-heals next frame
	if ( eyeproj_offset != ~0U ) {
		VkDescriptorSet sets[2];
		uint32_t offsets[2];
		VkBuffer vertexBuffer;
		VkBuffer indexBuffer;
		uint32_t indexCount;

		// binding 0 (fog/dlight uniform) is statically unused by these
		// shaders; offset 0 is always in-bounds. binding 1 selects this
		// frame's virtual-screen eyeProj pair.
		sets[0] = vk.cmd->uniform_descriptor;
		sets[1] = tr.whiteImage->descriptor;	// floor grid samples nothing, layout still wants set 1
		offsets[0] = 0;
		offsets[1] = eyeproj_offset;

		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			xr->floorGridPipeline );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			vk.pipeline_layout_virtual_screen, 0, 2, sets, 2, offsets );
		qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout_virtual_screen,
			VK_SHADER_STAGE_VERTEX_BIT, 0, 64, floorMV );
		qvkCmdBindVertexBuffers( vk.cmd->command_buffer, 0, 1,
			&xr->mesh.quadVertexBuffer, &zero_offset );
		qvkCmdBindIndexBuffer( vk.cmd->command_buffer,
			xr->mesh.quadIndexBuffer, 0, VK_INDEX_TYPE_UINT16 );
		qvkCmdDrawIndexed( vk.cmd->command_buffer, xr->mesh.quadIndexCount, 1, 0, 0, 0 );

		// 10. Draw virtual screen (cylinder or flat quad based on vr_virtualScreenShape)

		// Select mesh based on shape setting (0 = curved/cylinder, 1 = flat/quad)
		if ( vr_virtualScreenShape && vr_virtualScreenShape->integer == 1 ) {
			// Flat quad
			vertexBuffer = xr->mesh.quadVertexBuffer;
			indexBuffer = xr->mesh.quadIndexBuffer;
			indexCount = xr->mesh.quadIndexCount;
		} else {
			// Curved cylinder (default)
			vertexBuffer = xr->mesh.cylinderVertexBuffer;
			indexBuffer = xr->mesh.cylinderIndexBuffer;
			indexCount = xr->mesh.cylinderIndexCount;
		}

		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			xr->virtualScreenPipeline );
		// set 0 (and its dynamic offsets) is still bound; swap only the sampler set
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			vk.pipeline_layout_virtual_screen, 1, 1, &xr->virtualScreenDescriptor, 0, NULL );
		qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout_virtual_screen,
			VK_SHADER_STAGE_VERTEX_BIT, 0, 64, screenMV );
		qvkCmdBindVertexBuffers( vk.cmd->command_buffer, 0, 1,
			&vertexBuffer, &zero_offset );
		qvkCmdBindIndexBuffer( vk.cmd->command_buffer,
			indexBuffer, 0, VK_INDEX_TYPE_UINT16 );
		qvkCmdDrawIndexed( vk.cmd->command_buffer, indexCount, 1, 0, 0, 0 );
	}

	// End render pass - virtual screen drawing complete
	vk_end_render_pass();
}


/*
 * vk_destroy_gamma_framebuffers - Destroy gamma framebuffers (XR swapchain outputs)
 */
static void vk_destroy_gamma_framebuffers( void )
{
	uint32_t i;
	for ( i = 0; i < MAX_SWAPCHAIN_IMAGES; i++ ) {
		if ( vk.framebuffers.gamma[i] != VK_NULL_HANDLE ) {
			qvkDestroyFramebuffer( vk.device, vk.framebuffers.gamma[i], NULL );
			vk.framebuffers.gamma[i] = VK_NULL_HANDLE;
		}
	}
}

/*
 * vk_create_xr_gamma_framebuffers - Create framebuffers for gamma pass output to XR swapchain
 *
 * These framebuffers output to XR swapchain images via UNORM views (bypassing sRGB conversion).
 * They must be created after XR image views are ready.
 */
static qboolean vk_create_xr_gamma_framebuffers( void )
{
	VkXrResources *xr = &vk.xr;
	VkFramebufferCreateInfo fbCI;
	uint32_t i;

	if ( !vk.multiviewSupported ) {
		return qtrue;
	}

	if ( xr->colorInfo == NULL || vk.render_pass.gamma == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_WARNING, "vk_create_xr_gamma_framebuffers: Prerequisites not met\n" );
		return qfalse;
	}

	ri.Printf( PRINT_ALL, "Creating gamma framebuffers for XR swapchain output...\n" );

	for ( i = 0; i < xr->colorInfo->imageCount && i < MAX_SWAPCHAIN_IMAGES; i++ ) {
		// Use UNORM views for gamma framebuffer: gamma shader outputs sRGB-encoded
		// values directly, so we need to bypass Vulkan's automatic linear-to-sRGB
		// conversion that would occur with sRGB attachment format
		if ( xr->gammaViews[i] == VK_NULL_HANDLE ) {
			continue;
		}

		Com_Memset( &fbCI, 0, sizeof( fbCI ) );
		fbCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fbCI.renderPass = vk.render_pass.gamma;
		fbCI.attachmentCount = 1;
		fbCI.pAttachments = &xr->gammaViews[i];  // UNORM view - no sRGB conversion
		fbCI.width = xr->width;
		fbCI.height = xr->height;
		// Multiview render pass: layers must be 1 (view mask handles stereo)
		fbCI.layers = 1;

		VK_CHECK( qvkCreateFramebuffer( vk.device, &fbCI, NULL, &vk.framebuffers.gamma[i] ) );
		SET_OBJECT_NAME( vk.framebuffers.gamma[i], va( "gamma framebuffer %d (XR swapchain)", i ),
			VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
	}

	ri.Printf( PRINT_ALL, "...gamma framebuffers created (%d)\n", xr->colorInfo->imageCount );
	return qtrue;
}


/*
 * vk_create_xr_fbo_descriptors - Create descriptors for FBO resources
 *
 * Creates descriptors for color (gamma pass input) and bloom chain.
 * Uses Quake3e fields: vk.color_descriptor, vk.bloom_image_descriptor[]
 */
static qboolean vk_create_xr_fbo_descriptors( void )
{
	VkDescriptorSetAllocateInfo allocInfo;
	VkDescriptorImageInfo imageInfo;
	VkWriteDescriptorSet writeSet;
	Vk_Sampler_Def samplerDef;
	uint32_t i;

	if ( !vk.multiviewSupported ) {
		return qtrue;
	}

	ri.Printf( PRINT_ALL, "Creating FBO descriptors...\n" );

	// Set up sampler for post-processing: linear filtering, clamp to edge, no mipmaps
	Com_Memset( &samplerDef, 0, sizeof( samplerDef ) );
	samplerDef.gl_mag_filter = GL_LINEAR;
	samplerDef.gl_min_filter = GL_LINEAR;
	samplerDef.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerDef.max_lod_1_0 = qtrue;  // No mipmaps
	samplerDef.noAnisotropy = qtrue;

	// Allocate descriptor for FBO color (for gamma pass input)
	Com_Memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = vk.descriptor_pool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &vk.set_layout_sampler;

	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &allocInfo, &vk.color_descriptor ) );

	// Write FBO color descriptor
	Com_Memset( &imageInfo, 0, sizeof( imageInfo ) );
	imageInfo.sampler = vk_find_sampler( &samplerDef );
	imageInfo.imageView = vk.color_image_view;
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	ri.Printf( PRINT_ALL, "FBO descriptor: view=%p, image=%p, descriptor=%p\n",
		(void*)vk.color_image_view, (void*)vk.color_image, (void*)vk.color_descriptor );

	Com_Memset( &writeSet, 0, sizeof( writeSet ) );
	writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeSet.dstSet = vk.color_descriptor;
	writeSet.dstBinding = 0;
	writeSet.descriptorCount = 1;
	writeSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeSet.pImageInfo = &imageInfo;

	qvkUpdateDescriptorSets( vk.device, 1, &writeSet, 0, NULL );

	// Create descriptors for bloom chain
	if ( r_bloom->integer ) {
		for ( i = 0; i < ARRAY_LEN( vk.bloom_image_descriptor ); i++ ) {
			if ( vk.bloom_image_view[i] == VK_NULL_HANDLE ) {
				continue;
			}

			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &allocInfo, &vk.bloom_image_descriptor[i] ) );

			imageInfo.imageView = vk.bloom_image_view[i];
			writeSet.dstSet = vk.bloom_image_descriptor[i];

			qvkUpdateDescriptorSets( vk.device, 1, &writeSet, 0, NULL );
		}
	}

	ri.Printf( PRINT_ALL, "...FBO descriptors created\n" );
	return qtrue;
}

/*
 * vk_reallocate_xr_fbo_descriptors - Reallocate FBO descriptors after pool reset
 *
 * Called from vk_release_resources() after resetting the descriptor pool.
 * The images, views, and samplers are still valid; only descriptor sets need reallocation.
 */
static qboolean vk_reallocate_xr_fbo_descriptors( void )
{
	VkDescriptorSetAllocateInfo allocInfo;
	VkDescriptorImageInfo imageInfo;
	VkWriteDescriptorSet writeSet;
	Vk_Sampler_Def samplerDef;
	uint32_t i;

	// Set up common allocation info
	Com_Memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = vk.descriptor_pool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &vk.set_layout_sampler;

	Com_Memset( &imageInfo, 0, sizeof( imageInfo ) );
	Com_Memset( &writeSet, 0, sizeof( writeSet ) );
	writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeSet.dstBinding = 0;
	writeSet.descriptorCount = 1;
	writeSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeSet.pImageInfo = &imageInfo;

	// Reallocate FBO descriptors (fboActive always true in VR)
	if ( vk.multiviewSupported && vk.color_image_view != VK_NULL_HANDLE ) {
		// Set up sampler for post-processing
		Com_Memset( &samplerDef, 0, sizeof( samplerDef ) );
		samplerDef.gl_mag_filter = GL_LINEAR;
		samplerDef.gl_min_filter = GL_LINEAR;
		samplerDef.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerDef.max_lod_1_0 = qtrue;
		samplerDef.noAnisotropy = qtrue;

		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &allocInfo, &vk.color_descriptor ) );

		// Update FBO color descriptor
		imageInfo.sampler = vk_find_sampler( &samplerDef );
		imageInfo.imageView = vk.color_image_view;
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		writeSet.dstSet = vk.color_descriptor;

		qvkUpdateDescriptorSets( vk.device, 1, &writeSet, 0, NULL );

		// Reallocate bloom descriptors if bloom is active
		if ( r_bloom->integer ) {
			for ( i = 0; i < ARRAY_LEN( vk.bloom_image_descriptor ); i++ ) {
				if ( vk.bloom_image_view[i] == VK_NULL_HANDLE ) {
					continue;
				}

				VK_CHECK( qvkAllocateDescriptorSets( vk.device, &allocInfo, &vk.bloom_image_descriptor[i] ) );

				imageInfo.imageView = vk.bloom_image_view[i];
				writeSet.dstSet = vk.bloom_image_descriptor[i];

				qvkUpdateDescriptorSets( vk.device, 1, &writeSet, 0, NULL );
			}
		}
	}

	// Reallocate desktop mirror descriptor if resources exist
	// Note: This happens regardless of FBO state since desktop mirror has its own resources
	if ( vk.desktopMirrorView != VK_NULL_HANDLE && vk.linearSampler != VK_NULL_HANDLE ) {
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &allocInfo, &vk.desktopMirrorDescriptor ) );

		imageInfo.sampler = vk.linearSampler;
		imageInfo.imageView = vk.desktopMirrorView;
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		writeSet.dstSet = vk.desktopMirrorDescriptor;

		qvkUpdateDescriptorSets( vk.device, 1, &writeSet, 0, NULL );
	}

	// Reallocate virtual screen mirror descriptor if resources exist
	if ( vk.xr.virtualScreenArrayView != VK_NULL_HANDLE && vk.linearSampler != VK_NULL_HANDLE ) {
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &allocInfo, &vk.xr.virtualScreenMirrorDescriptor ) );

		imageInfo.sampler = vk.linearSampler;
		imageInfo.imageView = vk.xr.virtualScreenArrayView;
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		writeSet.dstSet = vk.xr.virtualScreenMirrorDescriptor;

		qvkUpdateDescriptorSets( vk.device, 1, &writeSet, 0, NULL );
	}

	// Reallocate per-eye XR swapchain descriptors for desktop mirror VR view mode
	if ( vk.xr.colorInfo && vk.linearSampler != VK_NULL_HANDLE ) {
		imageInfo.sampler = vk.linearSampler;
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		for ( i = 0; i < vk.xr.colorInfo->imageCount && i < MAX_SWAPCHAIN_IMAGES; i++ ) {
			for ( int eye = 0; eye < 2; eye++ ) {
				if ( vk.xr.colorEyeViews[eye][i] != VK_NULL_HANDLE ) {
					VK_CHECK( qvkAllocateDescriptorSets( vk.device, &allocInfo, &vk.xr.colorEyeDescriptors[eye][i] ) );

					imageInfo.imageView = vk.xr.colorEyeViews[eye][i];
					writeSet.dstSet = vk.xr.colorEyeDescriptors[eye][i];

					qvkUpdateDescriptorSets( vk.device, 1, &writeSet, 0, NULL );
				}
			}

			// Also reallocate 2D_ARRAY descriptors for desktop mirror shader
			if ( vk.xr.colorViews[i] != VK_NULL_HANDLE ) {
				VK_CHECK( qvkAllocateDescriptorSets( vk.device, &allocInfo, &vk.xr.colorArrayDescriptors[i] ) );

				imageInfo.imageView = vk.xr.colorViews[i];  // 2D_ARRAY view
				writeSet.dstSet = vk.xr.colorArrayDescriptors[i];

				qvkUpdateDescriptorSets( vk.device, 1, &writeSet, 0, NULL );
			}
		}
	}

	return qtrue;
}

// Static color swapchain info storage: populated from VR layer pull
static VR_VK_SwapchainInfo s_colorSwapchainInfo;

/*
 * vk_recreate_xr_render_pass - Recreate main render pass with correct XR formats
 *
 * The main render pass is initially created with vk.color_format (desktop format).
 * This function recreates it with the actual XR swapchain formats.
 */
static qboolean vk_recreate_xr_render_pass( VkFormat colorFormat, VkFormat depthFormat )
{
	VkAttachmentDescription attachments[5];  // [0] = resolve/color, [1] = depth, [2] = MSAA color, [3] = emissive resolve, [4] = emissive msaa (HDR)
	VkAttachmentReference colorRef, depthRef, colorResolveRef;
	VkAttachmentReference colorRefs[2];        // [0] = color, [1] = emissive (HDR 2nd color attachment)
	VkAttachmentReference colorResolveRefs[2]; // [0] = color resolve, [1] = emissive resolve (HDR MSAA)
	VkSubpassDescription subpass;
	VkSubpassDependency deps[2];
	VkRenderPassCreateInfo desc;
	VkRenderPassMultiviewCreateInfo multiviewInfo;
	uint32_t viewMask = 0b11;
	uint32_t correlationMask = 0b11;
	uint32_t attachmentCount;

	if ( !vk.multiviewSupported ) {
		ri.Printf( PRINT_WARNING, "vk_recreate_xr_render_pass: multiview not supported\n" );
		return qfalse;
	}

	// Destroy old render pass if it exists
	if ( vk.render_pass.main != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.main, NULL );
		vk.render_pass.main = VK_NULL_HANDLE;
	}

	// Multiview info
	Com_Memset( &multiviewInfo, 0, sizeof( multiviewInfo ) );
	multiviewInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO;
	multiviewInfo.subpassCount = 1;
	multiviewInfo.pViewMasks = &viewMask;
	multiviewInfo.correlationMaskCount = 1;
	multiviewInfo.pCorrelationMasks = &correlationMask;

	Com_Memset( attachments, 0, sizeof( attachments ) );

	if ( vk.msaaActive ) {
		// MSAA mode: 3 attachments
		// [0] = resolve target (1x samples, where MSAA is resolved to)
		// [1] = depth (multisampled)
		// [2] = MSAA color (render target)

		// Attachment 0: Resolve target (non-MSAA)
		attachments[0].format = vk.color_format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;  // Will be resolved into
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// Attachment 1: Depth (multisampled)
		attachments[1].format = vk.depth_format;
		attachments[1].samples = vkSamples;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		// Must STORE because mainResume uses LOAD_OP_LOAD on all attachments
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[1].stencilLoadOp = glConfig.stencilBits ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		// Attachment 2: MSAA color (render target)
		attachments[2].format = vk.color_format;
		attachments[2].samples = vkSamples;
#ifdef USE_BUFFER_CLEAR
		attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
#else
		attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
#endif
		// Must STORE because mainResume uses LOAD_OP_LOAD on all attachments
		attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[2].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// Subpass renders to MSAA color, resolves to attachment 0
		colorRef.attachment = 2;  // Render to MSAA
		colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		colorResolveRef.attachment = 0;  // Resolve to non-MSAA
		colorResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		depthRef.attachment = 1;
		depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		Com_Memset( &subpass, 0, sizeof( subpass ) );
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef;
		subpass.pResolveAttachments = &colorResolveRef;  // Automatic MSAA resolve
		subpass.pDepthStencilAttachment = &depthRef;

		attachmentCount = 3;

		if ( vk.hdrActive ) {
			// Mirror vk_create_render_passes' emissive-aware MSAA main pass:
			// [3] = emissive resolve (single-sample), [4] = emissive MSAA render target.
			attachments[3].format = VK_FORMAT_R16G16B16A16_SFLOAT;
			attachments[3].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachments[3].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[3].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[3].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[3].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			attachments[3].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			attachments[4].format = VK_FORMAT_R16G16B16A16_SFLOAT;
			attachments[4].samples = vkSamples;
			attachments[4].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachments[4].storeOp = VK_ATTACHMENT_STORE_OP_STORE; // Needed for mainResume
			attachments[4].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[4].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[4].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			attachments[4].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			colorRefs[0] = colorRef;        // attachment 2 (msaa color)
			colorRefs[1].attachment = 4;    // msaa emissive render target
			colorRefs[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			subpass.colorAttachmentCount = 2;
			subpass.pColorAttachments = colorRefs;

			colorResolveRefs[0] = colorResolveRef; // attachment 0 (color resolve)
			colorResolveRefs[1].attachment = 3;    // emissive resolve
			colorResolveRefs[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			subpass.pResolveAttachments = colorResolveRefs;

			attachmentCount = 5;
		}

		ri.Printf( PRINT_ALL, "Recreating main render pass with MSAA (%d samples)\n", vkSamples );
	} else {
		// Non-MSAA mode: 2 attachments
		// [0] = color
		// [1] = depth

		// Color attachment (2-layer array)
		attachments[0].format = vk.color_format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
#ifdef USE_BUFFER_CLEAR
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
#else
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
#endif
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// Depth attachment (2-layer array)
		attachments[1].format = vk.depth_format;
		attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		// Must STORE because post_bloom and mainResume use LOAD_OP_LOAD on depth
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[1].stencilLoadOp = glConfig.stencilBits ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		colorRef.attachment = 0;
		colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		depthRef.attachment = 1;
		depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		Com_Memset( &subpass, 0, sizeof( subpass ) );
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef;
		subpass.pDepthStencilAttachment = &depthRef;

		attachmentCount = 2;

		if ( vk.hdrActive ) {
			// Mirror vk_create_render_passes' emissive-aware non-MSAA main pass:
			// [2] = emissive resolve rendered directly as the 2nd color attachment.
			attachments[2].format = VK_FORMAT_R16G16B16A16_SFLOAT;
			attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[2].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			attachments[2].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			colorRefs[0] = colorRef;        // attachment 0 (color)
			colorRefs[1].attachment = 2;    // emissive
			colorRefs[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			subpass.colorAttachmentCount = 2;
			subpass.pColorAttachments = colorRefs;

			attachmentCount = 3;
		}
	}

	// Subpass dependencies: includes depth stages for mainResume which uses
	// LOAD_OP_LOAD on depth after HUD pass (FBO depth must be synchronized)
	deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	deps[0].dstSubpass = 0;
	deps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
	                        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
	                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT |
	                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
	                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
	                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
	                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	deps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	deps[1].srcSubpass = 0;
	deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
	                        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
	                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	deps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	desc.pNext = &multiviewInfo;
	desc.attachmentCount = attachmentCount;
	desc.pAttachments = attachments;
	desc.subpassCount = 1;
	desc.pSubpasses = &subpass;
	desc.dependencyCount = 2;
	desc.pDependencies = deps;

	VK_CHECK( qvkCreateRenderPass( vk.device, &desc, NULL, &vk.render_pass.main ) );
	SET_OBJECT_NAME( vk.render_pass.main, "render pass - XR main (multiview, recreated)", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
	ri.Printf( PRINT_ALL, "Recreated main render pass: %p (attachments: %d)\n", (void*)vk.render_pass.main, attachmentCount );

	// Also destroy and recreate mainResume render pass
	if ( vk.render_pass.mainResume != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.mainResume, NULL );
		vk.render_pass.mainResume = VK_NULL_HANDLE;
	}

	// mainResume: same as main but LOAD_OP_LOAD to preserve content after HUD pass
	// initialLayout must NOT be UNDEFINED with LOAD_OP_LOAD
	if ( vk.msaaActive ) {
		// MSAA mode: load all 3 attachments
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachments[1].stencilLoadOp = glConfig.stencilBits ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[2].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	} else {
		// Non-MSAA mode: load color and depth (main pass stored depth)
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachments[1].stencilLoadOp = glConfig.stencilBits ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	}

	if ( vk.hdrActive ) {
		// Preserve emissive energy accumulated in the main pass; initialLayouts set above
		// in the main branch are non-UNDEFINED, as LOAD_OP_LOAD requires.
		if ( vk.msaaActive ) {
			attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // emissive resolve
			attachments[4].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // msaa emissive
		} else {
			attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // emissive resolve
		}
	}

	// mainResume uses same deps as main (both now include depth synchronization)
	VK_CHECK( qvkCreateRenderPass( vk.device, &desc, NULL, &vk.render_pass.mainResume ) );
	SET_OBJECT_NAME( vk.render_pass.mainResume, "render pass - XR main resume (multiview, recreated)", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
	ri.Printf( PRINT_ALL, "Recreated mainResume render pass: %p\n", (void*)vk.render_pass.mainResume );

	// Recreate gamma render pass with UNORM format (outputs to XR swapchain via UNORM view)
	// The gamma shader outputs sRGB-encoded values directly, so we use UNORM format
	// to bypass Vulkan's automatic linear-to-sRGB conversion on write.
	// OpenXR will read the sRGB data correctly because the underlying image is sRGB format.
	{
		VkSubpassDependency gammaDep;
		VkFormat gammaFormat = vk_get_unorm_format( colorFormat );

		if ( vk.render_pass.gamma != VK_NULL_HANDLE ) {
			qvkDestroyRenderPass( vk.device, vk.render_pass.gamma, NULL );
			vk.render_pass.gamma = VK_NULL_HANDLE;
		}

		// gamma has only color attachment (no depth): outputs to XR swapchain
		// Uses UNORM format because gamma shader outputs sRGB values directly
		attachments[0].format = gammaFormat;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		// Use UNDEFINED since we don't care about previous content (gamma overwrites everything)
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// Reset colorRef and subpass for gamma pass (no MSAA, single attachment)
		colorRef.attachment = 0;
		colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		Com_Memset( &subpass, 0, sizeof( subpass ) );
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef;
		subpass.pDepthStencilAttachment = NULL;
		// No resolve attachments for gamma pass

		// Simpler dependency for gamma pass
		gammaDep.srcSubpass = VK_SUBPASS_EXTERNAL;
		gammaDep.dstSubpass = 0;
		gammaDep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		gammaDep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		gammaDep.srcAccessMask = 0;
		gammaDep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		gammaDep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		desc.attachmentCount = 1;
		desc.dependencyCount = 1;
		desc.pDependencies = &gammaDep;

		VK_CHECK( qvkCreateRenderPass( vk.device, &desc, NULL, &vk.render_pass.gamma ) );
		SET_OBJECT_NAME( vk.render_pass.gamma, "render pass - XR gamma (multiview, recreated)", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
		ri.Printf( PRINT_ALL, "Recreated gamma render pass: %p (UNORM format 0x%x)\n",
			(void*)vk.render_pass.gamma, (unsigned int)gammaFormat );
	}

	// Create virtual screen render pass (multiview, color+depth, no MSAA)
	// This is used for rendering floor grid and virtual screen cylinder to XR swapchain
	{
		VkRenderPassMultiviewCreateInfo vsMultiviewInfo;
		VkSubpassDependency vsDeps[2];
		uint32_t vsViewMask = 0b11;  // Both eyes
		uint32_t vsCorrelationMask = 0b11;

		if ( vk.render_pass.virtualScreen != VK_NULL_HANDLE ) {
			qvkDestroyRenderPass( vk.device, vk.render_pass.virtualScreen, NULL );
			vk.render_pass.virtualScreen = VK_NULL_HANDLE;
		}

		// Color attachment (UNORM format to match gammaViews used by framebuffers)
		attachments[0].flags = 0;
		attachments[0].format = vk_get_unorm_format( colorFormat );  // Must match gammaViews format
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;  // No MSAA
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// Depth attachment (XR swapchain format)
		attachments[1].flags = 0;
		attachments[1].format = depthFormat;  // XR swapchain depth format
		attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;  // No MSAA
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		colorRef.attachment = 0;
		colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		depthRef.attachment = 1;
		depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		Com_Memset( &subpass, 0, sizeof( subpass ) );
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef;
		subpass.pDepthStencilAttachment = &depthRef;

		// Dependencies
		vsDeps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		vsDeps[0].dstSubpass = 0;
		vsDeps[0].srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
		vsDeps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		vsDeps[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		vsDeps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		vsDeps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		vsDeps[1].srcSubpass = 0;
		vsDeps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		vsDeps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		vsDeps[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		vsDeps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		vsDeps[1].dstAccessMask = 0;
		vsDeps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		// Multiview info
		Com_Memset( &vsMultiviewInfo, 0, sizeof( vsMultiviewInfo ) );
		vsMultiviewInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO;
		vsMultiviewInfo.subpassCount = 1;
		vsMultiviewInfo.pViewMasks = &vsViewMask;
		vsMultiviewInfo.correlationMaskCount = 1;
		vsMultiviewInfo.pCorrelationMasks = &vsCorrelationMask;

		Com_Memset( &desc, 0, sizeof( desc ) );
		desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		desc.pNext = &vsMultiviewInfo;
		desc.attachmentCount = 2;
		desc.pAttachments = attachments;
		desc.subpassCount = 1;
		desc.pSubpasses = &subpass;
		desc.dependencyCount = 2;
		desc.pDependencies = vsDeps;

		VK_CHECK( qvkCreateRenderPass( vk.device, &desc, NULL, &vk.render_pass.virtualScreen ) );
		SET_OBJECT_NAME( vk.render_pass.virtualScreen, "render pass - virtual screen", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
		ri.Printf( PRINT_ALL, "Created virtual screen render pass\n" );
	}

	// Invalidate all pipelines created for RENDER_PASS_MAIN since render pass was recreated
	// They will be lazily recreated with the new render pass on next use
	{
		uint32_t i, invalidated = 0;
		for ( i = 0; i < vk.pipelines_count; i++ ) {
			if ( vk.pipelines[i].handle[RENDER_PASS_MAIN] != VK_NULL_HANDLE ) {
				qvkDestroyPipeline( vk.device, vk.pipelines[i].handle[RENDER_PASS_MAIN], NULL );
				vk.pipelines[i].handle[RENDER_PASS_MAIN] = VK_NULL_HANDLE;
				invalidated++;
			}
		}
		ri.Printf( PRINT_ALL, "Invalidated %u pipelines for RENDER_PASS_MAIN\n", invalidated );
	}

	// Recreate FBO framebuffers with the new render passes
	// These were created with the initial render pass in vk_create_framebuffers()
	// but must be recreated now that render passes have changed
	{
		VkImageView attachments[5]; // color | depth | msaa color | emissive resolve | emissive msaa
		VkFramebufferCreateInfo desc;

		// Destroy old framebuffers
		if ( vk.framebuffers.main != VK_NULL_HANDLE ) {
			qvkDestroyFramebuffer( vk.device, vk.framebuffers.main, NULL );
			vk.framebuffers.main = VK_NULL_HANDLE;
		}
		if ( vk.framebuffers.post_bloom != VK_NULL_HANDLE ) {
			qvkDestroyFramebuffer( vk.device, vk.framebuffers.post_bloom, NULL );
			vk.framebuffers.post_bloom = VK_NULL_HANDLE;
		}

		// Recreate main framebuffer with new render pass
		Com_Memset( &desc, 0, sizeof( desc ) );
		desc.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		desc.renderPass = vk.render_pass.main;
		desc.width = glConfig.vidWidth;
		desc.height = glConfig.vidHeight;
		desc.layers = 1;
		attachments[0] = vk.color_image_view;
		attachments[1] = vk.depth_image_view;
		desc.attachmentCount = 2;
		desc.pAttachments = attachments;

		if ( vk.msaaActive ) {
			attachments[2] = vk.msaa_image_view;
			desc.attachmentCount = 3;
		}

		if ( vk.hdrActive ) {
			// Same attachment order the recreated main/post_bloom render passes declare
			if ( vk.msaaActive ) {
				attachments[3] = vk.emissive_image_view;      // resolve
				attachments[4] = vk.emissive_image_view_msaa; // msaa render target
				desc.attachmentCount = 5;
			} else {
				attachments[2] = vk.emissive_image_view;      // resolve
				desc.attachmentCount = 3;
			}
		}

		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.main ) );
		SET_OBJECT_NAME( vk.framebuffers.main, "framebuffer - main (recreated)", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );

		// Recreate post-bloom framebuffer with new render pass
		// Keep same attachmentCount as main for pipeline compatibility
		desc.renderPass = vk.render_pass.post_bloom;
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.post_bloom ) );
		SET_OBJECT_NAME( vk.framebuffers.post_bloom, "framebuffer - post_bloom (recreated)", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );

		ri.Printf( PRINT_ALL, "Recreated FBO framebuffers with new render passes\n" );
	}

	{
		VkFormat gammaFormat = vk_get_unorm_format( colorFormat );
		ri.Printf( PRINT_ALL, "Recreated XR render passes: main uses FBO formats (color=0x%x, depth=0x%x), gamma outputs to UNORM (0x%x)\n",
			vk.color_format, vk.depth_format, gammaFormat );
	}

	return qtrue;
}

/*
 * vk_init_xr_resources - Initialize all XR-related Vulkan resources
 *
 * Called after VR layer creates XR swapchains and renderer creates render passes.
 * Pulls swapchain info from VR layer via ri.VR_Vulkan_GetSwapchainInfo().
 */
qboolean vk_init_xr_resources( void )
{
	if ( vk.xr.initialized ) {
		ri.Printf( PRINT_WARNING, "vk_init_xr_resources: Already initialized\n" );
		return qtrue;
	}

	// Pull swapchain info from VR layer
	const VR_VulkanSwapchainInfo* xrInfo =
		(const VR_VulkanSwapchainInfo*)ri.VR_Vulkan_GetSwapchainInfo();

	if ( !xrInfo || !xrInfo->colorImages ) {
		ri.Printf( PRINT_WARNING, "vk_init_xr_resources: XR color swapchain not available\n" );
		return qfalse;
	}

	// Populate static color swapchain info from pulled data
	Com_Memset( &s_colorSwapchainInfo, 0, sizeof( s_colorSwapchainInfo ) );
	s_colorSwapchainInfo.format = xrInfo->colorFormat;
	s_colorSwapchainInfo.width = xrInfo->colorWidth;
	s_colorSwapchainInfo.height = xrInfo->colorHeight;
	s_colorSwapchainInfo.arraySize = xrInfo->colorArraySize;
	s_colorSwapchainInfo.imageCount = xrInfo->colorImageCount;
	s_colorSwapchainInfo.images = xrInfo->colorImages;

	// Point vk.xr to the static color info
	vk.xr.colorInfo = &s_colorSwapchainInfo;

	// Set XR render dimensions from color swapchain
	vk.xr.width = xrInfo->colorWidth;
	vk.xr.height = xrInfo->colorHeight;

	ri.Printf( PRINT_ALL, "XR color swapchain info: %ux%u (%u images)\n",
		xrInfo->colorWidth, xrInfo->colorHeight, xrInfo->colorImageCount );

	// Recreate main render pass with correct XR swapchain formats
	// The initial render pass was created with desktop swapchain format which may differ
	// Use vk.depth_format for depth (native buffer) instead of XR-provided format
	if ( !vk_recreate_xr_render_pass( xrInfo->colorFormat, vk.depth_format ) ) {
		ri.Printf( PRINT_WARNING, "vk_init_xr_resources: Failed to recreate XR render pass\n" );
		return qfalse;
	}

	if ( !vk_create_xr_image_views() ) {
		return qfalse;
	}

	// Create native depth buffer (replaces OpenXR depth swapchain)
	if ( !vk_create_xr_native_depth() ) {
		vk_destroy_xr_image_views();
		return qfalse;
	}

	if ( !vk_create_xr_framebuffers() ) {
		vk_destroy_xr_native_depth();
		vk_destroy_xr_image_views();
		return qfalse;
	}

	// Create gamma framebuffers (output to XR swapchain)
	// Main FBO, bloom images, and bloom framebuffers are created in vk_create_attachments/vk_create_framebuffers
	if ( !vk_create_xr_gamma_framebuffers() ) {
		vk_destroy_xr_framebuffers();
		vk_destroy_xr_native_depth();
		vk_destroy_xr_image_views();
		return qfalse;
	}

	if ( !vk_create_xr_fbo_descriptors() ) {
		vk_destroy_gamma_framebuffers();
		vk_destroy_xr_framebuffers();
		vk_destroy_xr_native_depth();
		vk_destroy_xr_image_views();
		return qfalse;
	}

	// Create XR post-processing pipelines (gamma, bloom, blur, blend)
	vk_create_post_process_pipelines();

	// Note: HUD buffer is created earlier in R_InitImages(), not here
	// This ensures tr.hudImage exists before CreateExternalShaders() runs

	vk.xr.initialized = qtrue;
	ri.Printf( PRINT_ALL, "XR resources initialized successfully\n" );

	return qtrue;
}

/*
 * vk_shutdown_xr_resources - Shutdown all XR-related Vulkan resources
 */
void vk_shutdown_xr_resources( void )
{
	if ( !vk.xr.initialized ) {
		return;
	}

	// Wait for GPU to finish all operations before destroying resources
	if ( vk.device != VK_NULL_HANDLE ) {
		qvkDeviceWaitIdle( vk.device );
	}

	// Destroy post-processing pipelines first
	vk_destroy_post_process_pipelines();

	// Destroy desktop mirror resources (shader-based gamma correction)
	vk_destroy_desktop_mirror_resources();

	// Destroy virtual screen pipelines
	vk_destroy_virtual_screen_pipelines();

	// Destroy gamma framebuffers (XR swapchain outputs)
	vk_destroy_gamma_framebuffers();

	vk_destroy_virtual_screen_meshes();
	vk_destroy_virtual_screen_buffer();
	vk_destroy_hud_buffer();
	vk_destroy_xr_framebuffers();
	vk_destroy_xr_native_depth();  // Native depth buffer (replaces XR depth swapchain)
	vk_destroy_xr_image_views();

	// Clear swapchain info pointer
	vk.xr.colorInfo = NULL;

	vk.xr.initialized = qfalse;
	ri.Printf( PRINT_ALL, "XR resources shutdown complete\n" );
}

#endif // USE_VULKAN
