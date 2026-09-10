#pragma once

// Disable MSVC warning for anonymous structs/unions (standard extension)
#ifdef _MSC_VER
#pragma warning(disable: 4201)
#endif

#include <vulkan/vulkan.h>
#include "tr_common.h"

#define MAX_SWAPCHAIN_IMAGES 8
#define MIN_SWAPCHAIN_IMAGES_IMM 3
#define MIN_SWAPCHAIN_IMAGES_FIFO   3
#define MIN_SWAPCHAIN_IMAGES_FIFO_0 4
#define MIN_SWAPCHAIN_IMAGES_MAILBOX 3

#define MAX_VK_SAMPLERS 32
#define MAX_VK_PIPELINES ((1024 + 128)*2)

#define VERTEX_BUFFER_SIZE     (4 * 1024 * 1024)  /* by default */
#define VERTEX_BUFFER_SIZE_HI  (8 * 1024 * 1024)

#define STAGING_BUFFER_SIZE    (2 * 1024 * 1024)  /* by default */
#define STAGING_BUFFER_SIZE_HI (384 * 1024 * 1024) /* enough for max.texture size upload with all mip levels at once */

#define IMAGE_CHUNK_SIZE (256 * 1024 * 1024)
#define MAX_IMAGE_CHUNKS 56

#define NUM_COMMAND_BUFFERS 2	// number of command buffers / render semaphores / framebuffer sets

#define USE_REVERSED_DEPTH

//#define USE_UPLOAD_QUEUE

#define VK_NUM_BLOOM_PASSES 4

// HUD buffer dimensions for HUD mode 1 (in-world sprite)
#define HUD_BUFFER_WIDTH  1280
#define HUD_BUFFER_HEIGHT 960

// Mirrors VR_MAX_SHADING_RATES and VR_ShadingRate from vrvk/vr_vk.h. Copied
// rather than included because that header pulls the OpenXR headers in behind
// it, and every translation unit that takes vk.h would then carry them. The
// copy into vk.shadingRates is clamped to this length where it is made.
#define VK_MAX_SHADING_RATES 16

typedef struct {
	uint32_t width, height;
	VkSampleCountFlags sampleCounts;
} vk_shading_rate_t;

#ifndef _DEBUG
#define USE_DEDICATED_ALLOCATION
#endif
//#define MIN_IMAGE_ALIGN (128*1024)
#define MAX_ATTACHMENTS_IN_POOL (10+VK_NUM_BLOOM_PASSES*2) // depth + msaa + msaa-resolve + depth-resolve + screenmap.msaa + screenmap.resolve + screenmap.depth + bloom_extract + emissive-resolve + emissive-msaa + blur pairs

#define VK_DESC_STORAGE      0
#define VK_DESC_UNIFORM      0
#define VK_DESC_TEXTURE0     1
#define VK_DESC_TEXTURE1     2
#define VK_DESC_TEXTURE2     3
#define VK_DESC_FOG_COLLAPSE 4
#define VK_DESC_COUNT        5

#define VK_DESC_TEXTURE_BASE VK_DESC_TEXTURE0
#define VK_DESC_FOG_ONLY     VK_DESC_TEXTURE1
#define VK_DESC_FOG_DLIGHT   VK_DESC_TEXTURE1

// Multiview descriptor bindings (VR matrices at binding 0)
#define VK_DESC_MV_VRMATRICES  0
#define VK_DESC_MV_UNIFORM     1
#define VK_DESC_MV_TEXTURE0    2
#define VK_DESC_MV_TEXTURE1    3
#define VK_DESC_MV_TEXTURE2    4
#define VK_DESC_MV_FOG_COLLAPSE 5
#define VK_DESC_MV_COUNT       6

typedef enum {
	TYPE_COLOR_BLACK,
	TYPE_COLOR_WHITE,
	TYPE_COLOR_GREEN,
	TYPE_COLOR_RED,
	TYPE_FOG_ONLY,
	TYPE_DOT,

	TYPE_SINGLE_TEXTURE_LIGHTING,
	TYPE_SINGLE_TEXTURE_LIGHTING_LINEAR,
	TYPE_SINGLE_TEXTURE_LIGHTING_OVERBRIGHT,

	TYPE_SINGLE_TEXTURE_DF,

	TYPE_GENERIC_BEGIN, // start of non-env/env shader pairs
	TYPE_SINGLE_TEXTURE = TYPE_GENERIC_BEGIN,
	TYPE_SINGLE_TEXTURE_ENV,

	TYPE_SINGLE_TEXTURE_IDENTITY,
	TYPE_SINGLE_TEXTURE_IDENTITY_ENV,

	TYPE_SINGLE_TEXTURE_FIXED_COLOR,
	TYPE_SINGLE_TEXTURE_FIXED_COLOR_ENV,

	TYPE_SINGLE_TEXTURE_ENT_COLOR,
	TYPE_SINGLE_TEXTURE_ENT_COLOR_ENV,

	TYPE_MULTI_TEXTURE_ADD2_IDENTITY,
	TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV,
	TYPE_MULTI_TEXTURE_MUL2_IDENTITY,
	TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV,

	TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR,
	TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV,
	TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR,
	TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV,

	TYPE_MULTI_TEXTURE_MUL2,
	TYPE_MULTI_TEXTURE_MUL2_ENV,
	TYPE_MULTI_TEXTURE_ADD2_1_1,
	TYPE_MULTI_TEXTURE_ADD2_1_1_ENV,
	TYPE_MULTI_TEXTURE_ADD2,
	TYPE_MULTI_TEXTURE_ADD2_ENV,

	TYPE_MULTI_TEXTURE_MUL3,
	TYPE_MULTI_TEXTURE_MUL3_ENV,
	TYPE_MULTI_TEXTURE_ADD3_1_1,
	TYPE_MULTI_TEXTURE_ADD3_1_1_ENV,
	TYPE_MULTI_TEXTURE_ADD3,
	TYPE_MULTI_TEXTURE_ADD3_ENV,

	TYPE_BLEND2_ADD,
	TYPE_BLEND2_ADD_ENV,
	TYPE_BLEND2_MUL,
	TYPE_BLEND2_MUL_ENV,
	TYPE_BLEND2_ALPHA,
	TYPE_BLEND2_ALPHA_ENV,
	TYPE_BLEND2_ONE_MINUS_ALPHA,
	TYPE_BLEND2_ONE_MINUS_ALPHA_ENV,
	TYPE_BLEND2_MIX_ALPHA,
	TYPE_BLEND2_MIX_ALPHA_ENV,

	TYPE_BLEND2_MIX_ONE_MINUS_ALPHA,
	TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV,

	TYPE_BLEND2_DST_COLOR_SRC_ALPHA,
	TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV,

	TYPE_BLEND3_ADD,
	TYPE_BLEND3_ADD_ENV,
	TYPE_BLEND3_MUL,
	TYPE_BLEND3_MUL_ENV,
	TYPE_BLEND3_ALPHA,
	TYPE_BLEND3_ALPHA_ENV,
	TYPE_BLEND3_ONE_MINUS_ALPHA,
	TYPE_BLEND3_ONE_MINUS_ALPHA_ENV,
	TYPE_BLEND3_MIX_ALPHA,
	TYPE_BLEND3_MIX_ALPHA_ENV,
	TYPE_BLEND3_MIX_ONE_MINUS_ALPHA,
	TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV,

	TYPE_BLEND3_DST_COLOR_SRC_ALPHA,
	TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV,

	TYPE_GENERIC_END = TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV

} Vk_Shader_Type;

// used with cg_shadows == 2
typedef enum {
	SHADOW_DISABLED,
	SHADOW_EDGES,
	SHADOW_FS_QUAD,
} Vk_Shadow_Phase;

typedef enum {
	TRIANGLE_LIST = 0,
	TRIANGLE_STRIP,
	LINE_LIST,
	POINT_LIST
} Vk_Primitive_Topology;

typedef enum {
	DEPTH_RANGE_NORMAL,		// [0..1]
	DEPTH_RANGE_ZERO,		// [0..0]
	DEPTH_RANGE_ONE,		// [1..1]
	DEPTH_RANGE_WEAPON,		// [0..0.3]
	DEPTH_RANGE_COUNT
}  Vk_Depth_Range;

typedef struct {
	VkSamplerAddressMode address_mode; // clamp/repeat texture addressing mode
	int gl_mag_filter;		// GL_XXX mag filter
	int gl_min_filter;		// GL_XXX min filter
	qboolean max_lod_1_0;	// fixed 1.0 lod
	qboolean noAnisotropy;
} Vk_Sampler_Def;

typedef enum {
	RENDER_PASS_MAIN = 0,
	RENDER_PASS_SCREENMAP,
	RENDER_PASS_POST_BLOOM,
	RENDER_PASS_HUD,            // HUD buffer (1280x960) for HUD mode 1 sprite
	RENDER_PASS_MAIN_2D,        // Main pass, 2D draws: same pass object, rate combiner KEEP
	RENDER_PASS_COUNT
} renderPass_t;

typedef struct {
	Vk_Shader_Type shader_type;
	unsigned int state_bits; // GLS_XXX flags
	cullType_t face_culling;
	qboolean polygon_offset;
	qboolean mirror;
	Vk_Shadow_Phase shadow_phase;
	Vk_Primitive_Topology primitives;
	int line_width;
	int fog_stage; // off, fog-in / fog-out
	int abs_light;
	int allow_discard;
	int acff; // none, rgb, rgba, alpha
	int stencil_mark; // mark pixels with stencil bit 0x80 (for shadow exclusion)
	struct {
		byte rgb;
		byte alpha;
	} color;
} Vk_Pipeline_Def;

typedef struct VK_Pipeline {
	Vk_Pipeline_Def def;
	VkPipeline handle[ RENDER_PASS_COUNT ];
} VK_Pipeline_t;

// this structure must be in sync with shader uniforms!
typedef struct vkUniform_s {
	// light/env parameters:
	vec4_t eyePos;				// vertex
	union {
		struct {
			vec4_t pos;			// vertex: light origin
			vec4_t color;		// fragment: rgb + 1/(r*r)
			vec4_t vector;		// fragment: linear dynamic light
		} light;
		struct {
			vec4_t color[3];	// ent.color[3]
		} ent;
	};
	// fog parameters:
	vec4_t fogDistanceVector;	// vertex
	vec4_t fogDepthVector;		// vertex
	vec4_t fogEyeT;				// vertex
	vec4_t fogColor;			// fragment
} vkUniform_t;

#define TESS_XYZ   (1)
#define TESS_RGBA0 (2)
#define TESS_RGBA1 (4)
#define TESS_RGBA2 (8)
#define TESS_ST0   (16)
#define TESS_ST1   (32)
#define TESS_ST2   (64)
#define TESS_NNN   (128)
#define TESS_VPOS  (256)  // uniform with eyePos
#define TESS_ENV   (512)  // mark shader stage with environment mapping
#define TESS_ENT0  (1024) // uniform with ent.color[0]
#define TESS_ENT1  (2048) // uniform with ent.color[1]
#define TESS_ENT2  (4096) // uniform with ent.color[2]
#define TESS_OVERBRIGHT (8192) // upload tess.svars.overbright to binding 5 (lightingDiffuse HDR)
//
// Initialization.
//

// Initializes VK_Instance structure.
// After calling this function we get fully functional vulkan subsystem.
// Q3VR is XR-only: pulls Vulkan device from VR layer via ri.VR_Vulkan_GetDeviceInfo()
void vk_initialize( void );

// Called after initialization or renderer restart
void vk_init_descriptors( void );

// Shutdown vulkan subsystem by releasing resources acquired by Vk_Instance.
void vk_shutdown( refShutdownCode_t code );

// Releases vulkan resources allocated during program execution.
// This effectively puts vulkan subsystem into initial state (the state we have after vk_initialize call).
void vk_release_resources( void );

void vk_wait_idle( void );
void vk_queue_wait_idle( void );

//
// Resources allocation.
//
void vk_create_image( image_t *image, int width, int height, int mip_levels );
void vk_upload_image_data( image_t *image, int x, int y, int width, int height, int miplevels, byte *pixels, int size, qboolean update );
void vk_update_descriptor_set( image_t *image, qboolean mipmap );
void vk_destroy_image_resources( VkImage *image, VkImageView *imageView );
void vk_update_attachment_descriptors( void );
void vk_destroy_samplers( void );

uint32_t vk_find_pipeline_ext( uint32_t base, const Vk_Pipeline_Def *def, qboolean use );
void vk_get_pipeline_def( uint32_t pipeline, Vk_Pipeline_Def *def );

void vk_create_post_process_pipelines( void );
void vk_create_pipelines( void );

//
// Rendering setup.
//

void vk_clear_color( const vec4_t color );
void vk_clear_depth( qboolean clear_stencil );

// Frame functions (Q3VR is XR-only)
void vk_begin_frame( uint32_t colorIndex );
void vk_end_frame( void );
void vk_finish_frame( void );  // Force-end an interrupted frame (for shutdown)
void vk_discard_frame( void ); // End an interrupted frame without submitting it (shutdown)
void vk_present_desktop_mirror( void );  // Desktop mirror: acquire, blit, present

void vk_end_render_pass( void );
void vk_begin_main_render_pass( void );
void vk_update_shading_rate( void );  // records the rate map upload; one image, so no index
// Records what the map should hold. Called from outside the frame's command
// buffer, so it touches no Vulkan object.
void vk_set_foveation( int level, qboolean eyeTracked, const float centers[2][2] );
void vk_begin_hud_render_pass( qboolean clear );
void vk_end_hud_render_pass( void );
qboolean vk_create_hud_buffer( void );
qboolean vk_create_virtual_screen_buffer( void );  // Virtual screen texture for menu/follow mode
qboolean vk_create_virtual_screen_meshes( void );  // Cylinder and quad geometry
qboolean vk_create_virtual_screen_pipelines( void );  // Pipelines for virtual screen rendering
qboolean vk_init_xr_resources( void );  // Initialize XR-related Vulkan resources
void vk_shutdown_xr_resources( void );  // Cleanup XR-related resources

void vk_bind_pipeline( uint32_t pipeline );
void vk_bind_index( void );
void vk_bind_index_ext( const int numIndexes, const uint32_t*indexes );
void vk_bind_geometry( uint32_t flags );
void vk_bind_lighting( int stage, int bundle );
void vk_draw_geometry( Vk_Depth_Range depth_range, qboolean indexed );
void vk_draw_dot( uint32_t storage_offset );

void vk_read_pixels( byte* buffer, uint32_t width, uint32_t height ); // screenshots
qboolean vk_bloom( void );

// r_gpuTimeLog: what each interval of the frame is, named by the stamp that closes it
typedef enum {
	GPU_TIME_START,		// the frame's first stamp; closes nothing
	GPU_TIME_SCENE,		// 3D draws up to the 3D-to-2D boundary
	GPU_TIME_MAIN_END,	// the scene pass's store and resolve
	GPU_TIME_BLOOM,		// bloom extract and the blur chain
	GPU_TIME_HUD_END,	// the HUD buffer pass, in its own command buffer
	GPU_TIME_EYE_END,	// the eye buffer's last pass, 2D and its store
	GPU_TIME_GAMMA,		// the gamma pass
	GPU_TIME_END,		// virtual screen and whatever else precedes the end
	GPU_TIME_LABELS
} gpuTimeLabel_t;

#define GPU_TIME_MAX_STAMPS 16
#define GPU_TIME_HUD_STAMPS 6	// the HUD command buffer's own run: its start and up to five passes

void vk_gpu_time_stamp( gpuTimeLabel_t label );
void vk_gpu_time_mark_scene( void );  // the 3D-to-2D boundary, once per frame

qboolean vk_alloc_vbo( const byte *vbo_data, int vbo_size );
void vk_update_mvp( const float *m );
extern float vk_view_eyeproj[2][16];
void vk_set_view_eyeproj( void );
uint32_t VK_PushEyeProj( void );

uint32_t vk_tess_index( uint32_t numIndexes, const void *src );
void vk_bind_index_buffer( VkBuffer buffer, uint32_t offset );
#ifdef USE_VBO
void vk_draw_indexed( uint32_t indexCount, uint32_t firstIndex );
#endif
void vk_reset_descriptor( int index );
void vk_update_descriptor( int index, VkDescriptorSet descriptor );
void vk_update_descriptor_offset( int index, uint32_t offset );

void vk_update_post_process_pipelines( void );

const char *vk_format_string( VkFormat format );

void VBO_PrepareQueues( void );
void VBO_RenderIBOItems( void );
void VBO_ClearQueue( void );

typedef struct vk_tess_s {
	VkCommandBuffer command_buffer;
	VkCommandBuffer hud_command_buffer;  // the HUD buffer's own, submitted ahead of command_buffer
	qboolean		hud_begun;

	VkSemaphore image_acquired;
	uint32_t	swapchain_image_index;
	qboolean	swapchain_image_acquired;
#ifdef USE_UPLOAD_QUEUE
	VkSemaphore rendering_finished2;
#endif
	VkFence rendering_finished_fence;
	qboolean waitForFence;

	// r_gpuTimeLog: this slot's timestamps, written this frame and read back after its fence
	qboolean gpu_time_armed;
	qboolean gpu_time_scene_marked;	// the 3D-to-2D boundary stamp was written
	qboolean gpu_time_pending;
	uint32_t gpu_time_count;		// stamps written this frame, the start included
	byte gpu_time_label[GPU_TIME_MAX_STAMPS];
	// the HUD command buffer runs before command_buffer resets the slot's stamps, so it keeps its own
	uint32_t gpu_time_hud_count;
	byte gpu_time_hud_label[GPU_TIME_HUD_STAMPS];

	VkBuffer vertex_buffer;
	byte *vertex_buffer_ptr; // pointer to mapped vertex buffer
	uint32_t vertex_buffer_offset; // VkDeviceSize

	VkDescriptorSet uniform_descriptor;
	uint32_t		uniform_read_offset;
	uint32_t		eyeproj_offset;	// dynamic offset for set 0 binding 1 (per-view eyeProj)
	float			eyeproj_cache[32];		// ring content at eyeproj_offset, to skip redundant pushes
	qboolean		eyeproj_cache_valid;	// per-frame: reset at begin_frame and on ring resize
	VkDeviceSize	buf_offset[8];
	VkDeviceSize	vbo_offset[8];

	VkBuffer		curr_index_buffer;
	uint32_t		curr_index_offset;

	struct {
		uint32_t		start, end;
		VkDescriptorSet	current[5]; // 0:uniform, 1:color0, 2:color1, 3:color2, 4:fog
		uint32_t		offset[1]; // 0 (uniform)
	} descriptor_set;

	Vk_Depth_Range		depth_range;
	VkPipeline			last_pipeline;

	uint32_t num_indexes; // value from most recent vk_bind_index() call

	float		emissive_factor;	// fragment push constant: +1 additive, -1 alpha-additive, 0 otherwise

	VkRect2D scissor_rect;
} vk_tess_t;


// Forward declaration for VR layer swapchain info
struct VR_VK_SwapchainInfo_s;
typedef struct VR_VK_SwapchainInfo_s VR_VK_SwapchainInfo;


// XR-specific resources that don't have Quake3e/renderervk equivalents
// VR layer provides XrSwapchain handles and VkImages via VR_VK_SwapchainInfo.
// Renderer creates and owns VkImageViews, VkFramebuffers for XR swapchains.
// Note: FBO intermediate buffers (color_image, bloom_image[], etc.) use
// the existing Quake3e fields in Vk_Instance, made multiview-compatible.
// Depth buffer is created natively by the renderer, not from OpenXR,
// to avoid compatibility issues with some XR runtimes that fail depth swapchain creation.
typedef struct {
	// Pointer to VR layer color swapchain info (for accessing VkImages)
	const VR_VK_SwapchainInfo* colorInfo;

	// VkImageViews for XR color swapchain images (created by renderer)
	VkImageView colorViews[MAX_SWAPCHAIN_IMAGES];    // Multiview array views (sRGB format)
	VkImageView gammaViews[MAX_SWAPCHAIN_IMAGES];    // UNORM views for gamma pass

	// Native depth buffer for XR rendering (replaces OpenXR depth swapchain)
	// Single 2-layer multiview depth buffer, shared across all frames
	VkImage xrDepthImage;
	VkImageView xrDepthView;
	VkDeviceMemory xrDepthMemory;

	// Per-eye views for sampling XR swapchain (for desktop mirror menuStyle=1)
	VkImageView colorEyeViews[2][MAX_SWAPCHAIN_IMAGES];  // [eye][swapchain_index] - per-eye 2D views
	VkDescriptorSet colorEyeDescriptors[2][MAX_SWAPCHAIN_IMAGES];  // [eye][swapchain_index] - pre-bound descriptors

	// 2D_ARRAY descriptors for XR swapchain (for desktop mirror shader with sampler2DArray)
	VkDescriptorSet colorArrayDescriptors[MAX_SWAPCHAIN_IMAGES];

	// Direct XR swapchain framebuffers (used when FBO is NOT active)
	VkFramebuffer framebuffers[MAX_SWAPCHAIN_IMAGES];

	// Direct mode: the main pass renders into the swapchain image itself.
	// Separate from framebuffers[], which is the virtual screen's and has a
	// different attachment count under MSAA.
	VkFramebuffer directFramebuffers[MAX_SWAPCHAIN_IMAGES];

	// Fixed foveation: the main pass's shading rate attachment. One image, not
	// one per frame slot or swapchain index, because the barrier that precedes
	// the upload orders the write against every rate read already submitted to
	// the queue. That is also what leaves both framebuffer builders their
	// shape: the FBO path has one shared main framebuffer, direct mode one per
	// swapchain image, and a per-slot map would force an array on one and a
	// cross product on the other.
	VkImage shadingRateImage;
	VkDeviceMemory shadingRateMemory;
	VkImageView shadingRateView;
	// The staging buffer, unlike the image, is per frame slot. A pipeline
	// barrier orders the GPU against the GPU; nothing it can express orders a
	// CPU write into host-mapped memory against a copy the GPU has not reached
	// yet, and vk_begin_frame waits only on the slot it is about to reuse.
	// Indexing by vk.cmd_index is what makes that fence wait cover this buffer.
	VkBuffer shadingRateStaging[NUM_COMMAND_BUFFERS];
	VkDeviceMemory shadingRateStagingMemory[NUM_COMMAND_BUFFERS];
	void *shadingRateStagingMapped[NUM_COMMAND_BUFFERS];
	uint32_t shadingRateWidth;      // in rate texels, not pixels
	uint32_t shadingRateHeight;
	uint32_t shadingRateLayers;     // one per eye where the device allows it, else 1
	qboolean foveationActive;       // every consumer is guarded on this

	// What the VR layer asked for, recorded by vk_set_foveation outside the
	// frame and read by vk_update_shading_rate when the frame's main pass opens
	int foveationLevel;             // VR_FOVEATION_STRENGTH_*, 0 for off
	qboolean foveationEyeTracked;
	float foveationCenter[2][2];    // per eye, in NDC, y running down the image

	// The falloff drawn once at twice the map's size, so moving an eye's window
	// over it is a row copy rather than a rebuild. One byte a texel, the
	// finished rate: the density-to-rate conversion depends on strength and
	// sample count but never on position, so it belongs in the template.
	byte *shadingRateTemplate;
	int shadingRateTemplateLevel;   // -1 until built
	qboolean shadingRateTemplateEyeTracked;
	int shadingRateTemplateSamples; // the legal rate set changes with samples

	// What the image already holds. One image, so one copy of this, not one a
	// frame slot; vk_begin_main_render_pass runs twice in a frame that draws a
	// screenmap, and this is what makes the second call a no-op.
	qboolean shadingRateUploaded;
	int shadingRateAppliedLevel;
	qboolean shadingRateAppliedEyeTracked;
	int shadingRateAppliedSamples;
	uint32_t shadingRateAppliedOffset[2][2];  // [eye][x,y], in map texels
	int shadingRateRebuilds;        // TEMPORARY, remove before this branch ships

	// HUD buffer (1280x960, single layer) for HUD mode 1 sprite
	VkImage hudImage;
	VkImageView hudView;           // 2D_ARRAY view for framebuffer
	VkImageView hudSamplerView;    // 2D view for descriptor/sampling
	VkFramebuffer hudFramebuffer;
	VkDescriptorSet hudDescriptor;
	VkDeviceMemory hudMemory;

	// HUD depth buffer for proper 3D model rendering (e.g., character heads)
	VkImage hudDepthImage;
	VkImageView hudDepthView;
	VkDeviceMemory hudDepthMemory;

	// Virtual screen texture (for menu/follow mode display)
	VkImage virtualScreenImage;
	VkImageView virtualScreenView;
	VkImageView virtualScreenArrayView;      // 2D_ARRAY view for desktop mirror sampling
	VkFramebuffer virtualScreenFramebuffer;
	VkDescriptorSet virtualScreenDescriptor;
	VkDescriptorSet virtualScreenMirrorDescriptor;  // For desktop mirror shader (uses array view)
	VkDeviceMemory virtualScreenMemory;
	VkSampler virtualScreenSampler;

	// Virtual screen mesh geometry (cylinder and floor grid quad)
	struct {
		// Cylinder mesh (for curved virtual screen)
		VkBuffer cylinderVertexBuffer;
		VkBuffer cylinderIndexBuffer;
		VkDeviceMemory cylinderMemory;       // Vertex buffer memory
		VkDeviceMemory cylinderIndexMemory;  // Index buffer memory
		uint32_t cylinderVertexCount;
		uint32_t cylinderIndexCount;

		// Floor grid quad
		VkBuffer quadVertexBuffer;
		VkBuffer quadIndexBuffer;
		VkDeviceMemory quadMemory;           // Vertex buffer memory
		VkDeviceMemory quadIndexMemory;      // Index buffer memory
		uint32_t quadVertexCount;
		uint32_t quadIndexCount;
	} mesh;

	// Virtual screen pipelines
	VkPipeline virtualScreenPipeline;
	VkPipeline floorGridPipeline;

	// Current XR swapchain state
	uint32_t colorIndex;

	// XR resolution
	uint32_t width;
	uint32_t height;

	// Initialization state
	qboolean initialized;
} VkXrResources;

// Vk_Instance contains engine-specific vulkan resources that persist entire renderer lifetime.
// This structure is initialized/deinitialized by vk_initialize/vk_shutdown functions correspondingly.
typedef struct {
	VkPhysicalDevice physical_device;
	VkSurfaceFormatKHR base_format;
	VkSurfaceFormatKHR present_format;

	uint32_t queue_family_index;
	VkDevice device;
	VkQueue queue;

	VkSwapchainKHR swapchain;
	uint32_t swapchain_image_count;
	VkImage swapchain_images[MAX_SWAPCHAIN_IMAGES];
	VkSemaphore swapchain_rendering_finished[MAX_SWAPCHAIN_IMAGES];
	//uint32_t swapchain_image_index;

	// Desktop mirror tracking
	int desktopWidth;
	int desktopHeight;
	uint32_t desktopImageIndex;
	VkCommandBuffer desktopBlitCmd;
	VkSemaphore desktopAcquireSem;      // Signaled by acquire, waited by command buffer submit
	VkFence desktopBlitFence;           // Single fence - wait at start to ensure previous blit complete
	// Per-image semaphores to avoid reuse before presentation consumes them
	VkSemaphore desktopBlitComplete[MAX_SWAPCHAIN_IMAGES];     // Signaled when blit is done, waited by present
	VkSemaphore renderingCompleteSem;   // Signals when XR rendering is done, waited by desktop blit
	qboolean renderingCompleteSemSignaled;  // renderingCompleteSem holds an outstanding signal; it can outlive the frame that raised it

	// HDR display output (desktop mirror scRGB FP16) + headset wide-gamut
	qboolean hdrColorspaceExt;   // VK_EXT_swapchain_colorspace instance ext present
	qboolean hdrActive;          // mirror negotiated scRGB FP16 this run
	int      hdrOsState;         // osHdrState_t: OS HDR switch state (Windows)

	VkSampler linearSampler;  // Linear filtering, clamp to edge

	// Desktop mirror shader-based rendering (gamma-correct path)
	VkImage desktopMirrorImage;           // Intermediate texture (2-layer for stereo)
	VkDeviceMemory desktopMirrorMemory;
	VkImageView desktopMirrorView;        // UNORM view for sampling (prevents sRGB conversion)
	VkDescriptorSet desktopMirrorDescriptor;
	VkRenderPass desktopMirrorRenderPass;
	VkFramebuffer *desktopMirrorFramebuffers;  // [swapchain_image_count]
	VkImageView *desktopMirrorSwapViews;       // [swapchain_image_count] views into desktop swapchain
	VkPipeline desktopMirrorPipeline;
	VkPipelineLayout desktopMirrorPipelineLayout;

	VkCommandPool command_pool;
#ifdef USE_UPLOAD_QUEUE
	VkCommandBuffer staging_command_buffer;
#endif

	VkDeviceMemory image_memory[ MAX_ATTACHMENTS_IN_POOL ];
	uint32_t image_memory_count;

	struct {
		VkRenderPass main;        // Multiview main rendering (clears framebuffer)
		VkRenderPass screenmap;
		VkRenderPass gamma;       // Multiview gamma correction; built in both modes, entered only under the FBO
		VkRenderPass bloom_extract; // Multiview bloom extraction
		VkRenderPass blur[VK_NUM_BLOOM_PASSES*2]; // Multiview blur passes
		VkRenderPass post_bloom;  // Multiview post-bloom blend
		VkRenderPass hudBuffer;       // HUD buffer (1280x960, single layer, color+depth), color loaded
		VkRenderPass hudBufferClear;  // same, color cleared on load: the first HUD pass of a frame
		VkRenderPass virtualScreen; // Virtual screen (multiview, color+depth, no MSAA)
	} render_pass;

	VkDescriptorPool descriptor_pool;
	VkDescriptorSetLayout set_layout_sampler;	// combined image sampler
	VkDescriptorSetLayout set_layout_uniform;	// dynamic uniform buffer
	VkDescriptorSetLayout set_layout_storage;	// feedback buffer

	VkPipelineLayout pipeline_layout;			// main shaders (64-byte mono modelview push; per-eye projection in set 0 binding 1)
	VkPipelineLayout pipeline_layout_storage;	// flare test shader layout
	VkPipelineLayout pipeline_layout_post_process;	// post-processing
	VkPipelineLayout pipeline_layout_blend;		// post-processing
	VkPipelineLayout pipeline_layout_virtual_screen; // virtual screen (set 0 uniform+eyeProj, set 1 sampler, 64-byte mono-MV push)

	VkDescriptorSet color_descriptor;
	VkDescriptorSet emissive_descriptor;

	VkImage color_image;
	VkImageView color_image_view;

	VkImage emissive_image;
	VkImageView emissive_image_view;
	VkImage emissive_image_msaa;
	VkImageView emissive_image_view_msaa;

	VkImage bloom_image[1+VK_NUM_BLOOM_PASSES*2];
	VkImageView bloom_image_view[1+VK_NUM_BLOOM_PASSES*2];

	VkDescriptorSet bloom_image_descriptor[1+VK_NUM_BLOOM_PASSES*2];

	VkImage depth_image;
	VkImageView depth_image_view;

	// Single-sample depth the main pass resolves into, loaded by the post pass
	VkImage depth_resolve_image;
	VkImageView depth_resolve_image_view;

	VkImage msaa_image;
	VkImageView msaa_image_view;

	// Direct mode with MSAA: the scene draws into these and the store resolves
	// color into the swapchain image, so nothing reads either afterwards. They
	// own their memory instead of taking a slice of the batched attachment pool,
	// because the color's format is the XR swapchain's and is not known until
	// long after the pool is packed.
	VkImage transient_color_image;
	VkImageView transient_color_image_view;
	VkDeviceMemory transient_color_memory;
	VkImage transient_depth_image;
	VkImageView transient_depth_image_view;
	VkDeviceMemory transient_depth_memory;

	// screenMap
	struct {
		VkDescriptorSet color_descriptor;
		VkImage color_image;
		VkImageView color_image_view;

		VkImage color_image_msaa;
		VkImageView color_image_view_msaa;

		VkImage depth_image;
		VkImageView depth_image_view;

	} screenMap;

	struct {
		VkImage image;
		VkImageView image_view;
	} capture;

	struct {
		VkFramebuffer blur[VK_NUM_BLOOM_PASSES*2];
		VkFramebuffer bloom_extract;
		VkFramebuffer post_bloom;   // Post-bloom blend pass: the single-sample resolves, color and depth
		VkFramebuffer main;         // FBO mode: single framebuffer for main rendering
		VkFramebuffer gamma[MAX_SWAPCHAIN_IMAGES];
		VkFramebuffer screenmap;
	} framebuffers;

#ifdef USE_UPLOAD_QUEUE
	VkSemaphore rendering_finished;	// reference to vk.cmd->rendering_finished2
	VkSemaphore image_uploaded2;
	VkSemaphore image_uploaded;		// reference to vk.image_uploaded2
#endif

	vk_tess_t tess[ NUM_COMMAND_BUFFERS ], *cmd;
	int cmd_index;

	struct {
		VkBuffer		buffer;
		byte			*buffer_ptr;
		VkDeviceMemory	memory;
		VkDescriptorSet	descriptor;
	} storage;

	uint32_t uniform_item_size;
	uint32_t uniform_alignment;
	uint32_t storage_alignment;

	struct {
		VkBuffer vertex_buffer;
		VkDeviceMemory	buffer_memory;
	} vbo;

	// host visible memory that holds vertex, index and uniform data
	VkDeviceMemory geometry_buffer_memory;
	VkDeviceSize geometry_buffer_size;
	VkDeviceSize geometry_buffer_size_new;

	// statistics
	struct {
		VkDeviceSize vertex_buffer_max;
		uint32_t push_size;
		uint32_t push_size_max;
	} stats;

	// GPU time of the eye buffer frame (r_gpuTimeLog): a stamp at the frame's start and
	// after each pass; every interval is named by the stamp that closes it
	VkQueryPool gpuTimePool;
	float timestampPeriod;		// nanoseconds per tick, zero when the queue cannot timestamp
	struct {
		float ms[1024];							// whole frame
		float label[GPU_TIME_LABELS][1024];		// per-frame sum of the intervals closed by that label
		int count;
	} gpuTime;

	//
	// Shader modules.
	// All 3D shaders use multiview for VR stereo rendering.
	//
	struct {
		struct {
			VkShaderModule gen[3][2][2][2]; // tx[0,1,2], cl[0,1] env0[0,1] fog[0,1]
			VkShaderModule ident1[2][2][2]; // tx[0,1], env0[0,1] fog[0,1]
			VkShaderModule fixed[2][2][2];  // tx[0,1], env0[0,1] fog[0,1]
			VkShaderModule light[2];        // fog[0,1]
			VkShaderModule overbright_vert[2]; // fog[0,1]
		} vert;
		struct {
			// em index: [0] writes the location-1 emissive output, [1] omits it
			// (passes without the emissive attachment). fog stays the last
			// dimension: create_pipeline selects fog variants by fs_module++.
			VkShaderModule gen0_df;
			VkShaderModule gen[3][2][2][2]; // tx[0,1,2] cl[0,1] em[0,1] fog[0,1]
			VkShaderModule ident1[2][2][2]; // tx[0,1], em[0,1] fog[0,1]
			VkShaderModule fixed[2][2][2];  // tx[0,1], em[0,1] fog[0,1]
			VkShaderModule ent[1][2][2];    // tx[0], em[0,1] fog[0,1]
			VkShaderModule light[2][2][2];  // linear[0,1] em[0,1] fog[0,1]
			VkShaderModule overbright_frag[2][2]; // em[0,1] fog[0,1]
		} frag;

		VkShaderModule color_fs;
		VkShaderModule color_vs;  // multiview

		VkShaderModule bloom_fs;
		VkShaderModule blur_fs;
		VkShaderModule blend_fs;

		VkShaderModule gamma_fs;
		VkShaderModule gamma_vs;

		VkShaderModule foveationdebug_fs;  // reuses gamma_vs for its fullscreen quad

		VkShaderModule fog_fs;  // multiview
		VkShaderModule fog_vs;  // multiview

		VkShaderModule dot_fs;
		VkShaderModule dot_vs;

		VkShaderModule virtualscreen_fs;
		VkShaderModule virtualscreen_vs;  // multiview
		VkShaderModule floor_grid_fs;
		VkShaderModule floor_grid_vs;  // multiview

		VkShaderModule desktopmirror_fs;
		VkShaderModule desktopmirror_vs;  // NOT multiview
	} modules;

	VkPipelineCache pipelineCache;

	VK_Pipeline_t pipelines[ MAX_VK_PIPELINES ];
	uint32_t pipelines_count;
	uint32_t pipelines_world_base;

	// pipeline statistics
	int32_t pipeline_create_count;

	//
	// Standard pipelines.
	//
	uint32_t skybox_pipeline;

	// dim 0: 0 - front side, 1 - back size
	// dim 1: 0 - normal view, 1 - mirror view
	uint32_t shadow_volume_pipelines[2][2];
	uint32_t shadow_finish_pipeline;

	// dim 0 is based on fogPass_t: 0 - corresponds to FP_EQUAL, 1 - corresponds to FP_LE.
	// dim 1 is directly a cullType_t enum value.
	// dim 2 is a polygon offset value (0 - off, 1 - on).
	uint32_t fog_pipelines[2][3][2];

	// dim 0 is based on dlight additive flag: 0 - not additive, 1 - additive
	// dim 1 is directly a cullType_t enum value.
	// dim 2 is a polygon offset value (0 - off, 1 - on).
#ifdef USE_LEGACY_DLIGHTS
	uint32_t dlight_pipelines[2][3][2];
#endif

	// cullType[3], polygonOffset[2], fogStage[2], absLight[2]
#ifdef USE_PMLIGHT
	uint32_t dlight_pipelines_x[3][2][2][2];
	uint32_t dlight1_pipelines_x[3][2][2][2];
#endif

	// debug visualization pipelines
	uint32_t tris_debug_pipeline;
	uint32_t tris_mirror_debug_pipeline;
	uint32_t tris_debug_green_pipeline;
	uint32_t tris_mirror_debug_green_pipeline;
	uint32_t tris_debug_red_pipeline;
	uint32_t tris_mirror_debug_red_pipeline;

	uint32_t normals_debug_pipeline;
	uint32_t surface_debug_pipeline_solid;
	uint32_t surface_debug_pipeline_outline;
	uint32_t images_debug_pipeline;
	uint32_t images_debug_pipeline2;
	uint32_t surface_beam_pipeline;
	uint32_t surface_axis_pipeline;
	uint32_t dot_pipeline;

	// Post-processing pipelines (multiview)
	VkPipeline gamma_pipeline;
	VkPipeline bloom_extract_pipeline;
	VkPipeline blur_pipeline[VK_NUM_BLOOM_PASSES*2];
	VkPipeline bloom_blend_pipeline;

	// Built lazily on first r_foveationDebug enable, not alongside the pipelines
	// above: a player who never types the cvar should not pay for it.
	VkPipeline foveation_debug_pipeline;

	uint32_t frame_count;
	qboolean active;
	qboolean wideLines;
	qboolean samplerAnisotropy;
	qboolean fragmentStores;
	qboolean dedicatedAllocation;
	qboolean debugNames;
	qboolean multiviewSupported;   // VK_KHR_multiview available
	qboolean depthClamp;           // depth clamp for z-fail shadow volumes

	// VK_KHR_fragment_shading_rate, cached from VR_VulkanDeviceInfo at device
	// pickup so nothing in the renderer reaches back into the VR layer's struct
	// mid-frame; the rate list is read on every pipeline template build
	qboolean shadingRateSupported;
	uint32_t shadingRateTexelWidth;   // the attachment's texel grid
	uint32_t shadingRateTexelHeight;
	uint32_t shadingRateMaxWidth;     // coarsest fragment the device will produce
	uint32_t shadingRateMaxHeight;
	qboolean shadingRateLayered;      // layeredShadingRateAttachments: one layer per eye
	vk_shading_rate_t shadingRates[VK_MAX_SHADING_RATES];
	uint32_t shadingRateCount;

	VkResolveModeFlagBits depthResolveMode;   // main pass depth resolve, MAX under reversed depth
	VkResolveModeFlagBits stencilResolveMode; // NONE unless the device couples it to depth

	float maxAnisotropy;
	float maxLod;

	VkFormat color_format;
	// What the main pass declares for its color attachment: the FBO's format,
	// or in direct mode the swapchain view's, which is known only once the XR
	// swapchain exists and the pass is rebuilt
	VkFormat mainColorFormat;
	VkFormat capture_format;
	VkFormat depth_format;
	VkFormat bloom_format;

	VkImageLayout initSwapchainLayout;

	qboolean clearAttachment;		// requires VK_IMAGE_USAGE_TRANSFER_DST_BIT for swapchains
	qboolean fboActive;
	qboolean blitEnabled;
	qboolean msaaActive;
	qboolean depthResolveActive;	// main pass resolves depth for a single-sample post pass

	qboolean offscreenRender;

	qboolean windowAdjusted;
	int		blitX0;
	int		blitY0;
	int		blitFilter;

	uint32_t renderWidth;
	uint32_t renderHeight;

	float renderScaleX;
	float renderScaleY;

	renderPass_t renderPassIndex;
	qboolean rateExempt;		// a 3D draw the player reads rather than looks past
	qboolean inRenderPass;		// true when actually inside a render pass
	qboolean recordingCommands;	// true when command buffer is recording (between Begin/End)
	qboolean descriptorsReady;	// qfalse between vk_release_resources() and vk_init_descriptors(): pool contents are dead

	// HUD brackets record into vk.cmd->hud_command_buffer, submitted ahead of
	// the frame's, so an open pass never has to be resumed; frame state parks here
	qboolean inHudCommandBuffer;
	struct {
		VkCommandBuffer commandBuffer;
		qboolean inRenderPass;
		renderPass_t renderPassIndex;
		uint32_t renderWidth, renderHeight;
		float renderScaleX, renderScaleY;
	} hudSaved;

	uint32_t screenMapWidth;
	uint32_t screenMapHeight;
	uint32_t screenMapSamples;

	uint32_t image_chunk_size;

	uint32_t maxBoundDescriptorSets;

#ifdef USE_UPLOAD_QUEUE
	VkFence aux_fence;
	qboolean aux_fence_wait;
#endif

	struct staging_buffer_s {
		VkBuffer handle;
		VkDeviceMemory memory;
		VkDeviceSize size;
		byte *ptr; // pointer to mapped staging buffer
#ifdef USE_UPLOAD_QUEUE
		VkDeviceSize offset;
#endif
	} staging_buffer;

	struct samplers_s {
		int count;
		Vk_Sampler_Def def[MAX_VK_SAMPLERS];
		VkSampler handle[MAX_VK_SAMPLERS];
		int filter_min;
		int filter_max;
	} samplers;

	struct defaults_t {
		VkDeviceSize staging_size;
		VkDeviceSize geometry_size;
	} defaults;

	char driverNote[200];

	// Q3VR is XR-only: Vulkan instance/device are provided by VR layer
	qboolean xrMode;
	VkInstance xrInstance;

	// XR swapchain resources (renderer-owned views/framebuffers for XR images)
	VkXrResources xr;

} Vk_Instance;

typedef struct {
	VkDeviceMemory memory;
	VkDeviceSize used;
} ImageChunk;

// Vk_World contains vulkan resources/state requested by the game code.
// It is reinitialized on a map change.
typedef struct {
	//
	// Memory allocations.
	//
	int num_image_chunks;
	ImageChunk image_chunks[MAX_IMAGE_CHUNKS];

	//
	// State.
	//

	// Descriptor sets corresponding to bound texture images.
	//VkDescriptorSet current_descriptor_sets[ MAX_TEXTURE_UNITS ];

	// This flag is used to decide whether framebuffer's depth attachment should be cleared
	// with vmCmdClearAttachment (dirty_depth_attachment != 0), or it have just been
	// cleared by render pass instance clear op (dirty_depth_attachment == 0).
	int dirty_depth_attachment;

	float modelview_transform[16];

	// Per-eye projection matrices from OpenXR (set via RE_SetVRHeadsetParms)
	float projectionEye[2][16];
} Vk_World;

extern Vk_Instance	vk;				// shouldn't be cleared during ref re-init
extern Vk_World		vk_world;		// this data is cleared during ref re-init
