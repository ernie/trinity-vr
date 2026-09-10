include_guard(GLOBAL)

# Find OpenXR - required for all VR code
find_package(OpenXR CONFIG REQUIRED)
list(APPEND VR_LIBRARIES OpenXR::openxr_loader OpenXR::headers)

# vr_types.h includes vulkan.h because OpenXR's Vulkan platform types need it
find_package(Vulkan REQUIRED)
list(APPEND VR_INCLUDE_DIRS ${Vulkan_INCLUDE_DIRS})

# vrcommon - The OpenXR layer; Vulkan-specific parts live in vrvk
set(VR_COMMON_SOURCES
    ${SOURCE_DIR}/vrcommon/vr_cvars.c
    ${SOURCE_DIR}/vrcommon/vr_debug.c
    ${SOURCE_DIR}/vrcommon/vr_events.c
    ${SOURCE_DIR}/vrcommon/vr_gameplay.c
    ${SOURCE_DIR}/vrcommon/vr_haptics.c
    ${SOURCE_DIR}/vrcommon/vr_bhaptics.c
    ${SOURCE_DIR}/vrcommon/vr_input.c
    ${SOURCE_DIR}/vrcommon/vr_math.c
    ${SOURCE_DIR}/vrcommon/vr_spaces.c
    ${SOURCE_DIR}/vrcommon/vr_swapchains.c
    ${SOURCE_DIR}/vrcommon/vr_base.c
    ${SOURCE_DIR}/vrcommon/vr_shared_sync.c
    ${SOURCE_DIR}/vrcommon/vr_instance.c
    ${SOURCE_DIR}/vrcommon/vr_render_loop.c
    ${SOURCE_DIR}/vrcommon/vr_session.c
    ${SOURCE_DIR}/vrcommon/vr_virtual_screen.c
)

# vrvk - Vulkan-specific VR sources (XR_KHR_vulkan_enable2 integration)
set(VR_VK_SOURCES
    ${SOURCE_DIR}/vrvk/vr_vk.c
    ${SOURCE_DIR}/vrvk/vr_vk_debug.c
    ${SOURCE_DIR}/vrvk/vr_vk_renderer.c
    ${SOURCE_DIR}/vrvk/vr_vk_session.c
    ${SOURCE_DIR}/vrvk/vr_vk_swapchains.c
    ${SOURCE_DIR}/vrvk/vr_vk_virtual_screen.c
)

# The client links the VR layer (vrcommon + vrvk); the renderer DLL is loaded
# through cl_renderer.
set(VR_SOURCES ${VR_COMMON_SOURCES} ${VR_VK_SOURCES})

# The client makes Vulkan calls itself (cl_main.c needs vkGetInstanceProcAddr).
list(APPEND VR_LIBRARIES Vulkan::Vulkan)

list(APPEND VR_INCLUDE_DIRS
    ${SOURCE_DIR}/vrcommon
    ${SOURCE_DIR}/vrvk)

list(APPEND RENDERER_INCLUDE_DIRS ${VR_INCLUDE_DIRS})
