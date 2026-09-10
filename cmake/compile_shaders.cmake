# Shader compilation for Vulkan renderer
# Compiles GLSL shaders to SPIR-V and generates shader_data.c
# Only rebuilds when shader sources change
#
# Q3VR: All shaders use multiview (gl_ViewIndex) for stereo VR rendering.
# Non-multiview shaders have been removed.

if(NOT BUILD_CLIENT OR NOT BUILD_RENDERER_VK)
    return()
endif()

# Find glslangValidator from Vulkan SDK
find_program(GLSLANG_VALIDATOR glslangValidator
    HINTS "$ENV{VULKAN_SDK}/Bin"
    REQUIRED
)

set(SHADER_DIR ${SOURCE_DIR}/renderervk/shaders)
set(SPIRV_DIR ${CMAKE_BINARY_DIR}/spirv)
set(SHADER_DATA_OUTPUT ${SPIRV_DIR}/shader_data.c)
set(SHADER_DATA_FINAL ${SHADER_DIR}/spirv/shader_data.c)

# Build bin2hex utility
add_executable(bin2hex ${SHADER_DIR}/bin2hex.c)
set_target_properties(bin2hex PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}
)

# Collect all shader source files
file(GLOB SHADER_SOURCES
    ${SHADER_DIR}/*.vert
    ${SHADER_DIR}/*.frag
    ${SHADER_DIR}/*.tmpl
)

# Use a single custom command that compiles ALL shaders sequentially
# This ensures proper ordering and atomic update of shader_data.c
#
# All 3D shaders use multiview with --target-env vulkan1.1 for gl_ViewIndex.
# Post-processing shaders (gamma, bloom, blur, blend) don't need multiview.
add_custom_command(
    OUTPUT ${SHADER_DATA_FINAL}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SPIRV_DIR}
    COMMAND ${CMAKE_COMMAND} -E remove -f ${SHADER_DATA_OUTPUT}

    # ========================================================
    # POST-PROCESSING SHADERS (multiview for XR r_fbo pipeline)
    # These render fullscreen quads to 2-layer multiview targets
    # Requires --target-env vulkan1.1 for gl_ViewIndex
    # ========================================================
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gamma.vert
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} gamma_vert_spv
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gamma.frag
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} gamma_frag_spv
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/foveationdebug.frag
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} foveationdebug_frag_spv
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/blend.frag
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} blend_frag_spv
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/bloom.frag
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} bloom_frag_spv
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/blur.frag
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} blur_frag_spv

    # Dot shader (debug visualization)
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/dot.vert
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} dot_vert_spv
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/dot.frag
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} dot_frag_spv

    # ========================================================
    # MULTIVIEW 3D SHADERS (VR stereo rendering)
    # Requires --target-env vulkan1.1 for gl_ViewIndex
    # ========================================================

    # Standalone vertex shaders
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/color.vert
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} color_vert_spv
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/fog.vert
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} fog_vert_spv

    # Standalone fragment shaders
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/color.frag
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} color_frag_spv
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/fog.frag
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} fog_frag_spv

    # Lighting shaders
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/light_vert.tmpl
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_light
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/light_vert.tmpl -DUSE_FOG
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_light_fog

    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/light_frag.tmpl -DUSE_EMISSIVE
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_light
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/light_frag.tmpl -DUSE_FOG -DUSE_EMISSIVE
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_light_fog
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/light_frag.tmpl -DUSE_LINE -DUSE_EMISSIVE
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_light_line
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/light_frag.tmpl -DUSE_LINE -DUSE_FOG -DUSE_EMISSIVE
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_light_line_fog

    # Generic vertex shaders - single texture
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx0
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_OVERBRIGHT
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx0_overbright
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_OVERBRIGHT -DUSE_FOG
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx0_overbright_fog
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_FOG
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx0_fog
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_ENV
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx0_env
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_FOG -DUSE_ENV
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx0_env_fog

    # Single-texture vertex, identity colors
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_CLX_IDENT
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx0_ident1
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_CLX_IDENT -DUSE_FOG
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx0_ident1_fog
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_CLX_IDENT -DUSE_ENV
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx0_ident1_env
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_CLX_IDENT -DUSE_FOG -DUSE_ENV
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx0_ident1_env_fog

    # Single-texture vertex, fixed colors
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_FIXED_COLOR
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx0_fixed
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_FIXED_COLOR -DUSE_FOG
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx0_fixed_fog
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_FIXED_COLOR -DUSE_ENV
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx0_fixed_env
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_FIXED_COLOR -DUSE_FOG -DUSE_ENV
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx0_fixed_env_fog

    # Double-texture vertex shaders
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_TX1
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx1
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_TX1 -DUSE_FOG
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx1_fog
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_TX1 -DUSE_ENV
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx1_env
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_TX1 -DUSE_FOG -DUSE_ENV
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx1_env_fog

    # Double-texture vertex, identity colors
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_CLX_IDENT -DUSE_TX1
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx1_ident1
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_CLX_IDENT -DUSE_TX1 -DUSE_FOG
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx1_ident1_fog
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_CLX_IDENT -DUSE_TX1 -DUSE_ENV
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx1_ident1_env
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_CLX_IDENT -DUSE_TX1 -DUSE_FOG -DUSE_ENV
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx1_ident1_env_fog

    # Double-texture vertex, fixed colors
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_FIXED_COLOR -DUSE_TX1
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx1_fixed
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_FIXED_COLOR -DUSE_TX1 -DUSE_FOG
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx1_fixed_fog
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_FIXED_COLOR -DUSE_TX1 -DUSE_ENV
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx1_fixed_env
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_FIXED_COLOR -DUSE_TX1 -DUSE_FOG -DUSE_ENV
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx1_fixed_env_fog

    # Double-texture vertex, non-identical colors
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_CL1 -DUSE_TX1
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx1_cl
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_CL1 -DUSE_TX1 -DUSE_FOG
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx1_cl_fog
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_CL1 -DUSE_TX1 -DUSE_ENV
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx1_cl_env
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_CL1 -DUSE_TX1 -DUSE_ENV -DUSE_FOG
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx1_cl_env_fog

    # Triple-texture vertex shaders
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_TX2
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx2
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_TX2 -DUSE_FOG
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx2_fog
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_TX2 -DUSE_ENV
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx2_env
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_TX2 -DUSE_ENV -DUSE_FOG
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx2_env_fog

    # Triple-texture vertex, non-identical colors
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_CL2 -DUSE_TX2
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx2_cl
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_CL2 -DUSE_TX2 -DUSE_FOG
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx2_cl_fog
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_CL2 -DUSE_TX2 -DUSE_ENV
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx2_cl_env
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_CL2 -DUSE_TX2 -DUSE_ENV -DUSE_FOG
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx2_cl_env_fog

    # Generic fragment shaders - single-texture
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_ATEST -DUSE_EMISSIVE
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx0
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_ATEST -DUSE_OVERBRIGHT -DUSE_EMISSIVE
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx0_overbright
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_ATEST -DUSE_OVERBRIGHT -DUSE_FOG -DUSE_EMISSIVE
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx0_overbright_fog
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_ATEST -DUSE_FOG -DUSE_EMISSIVE
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx0_fog

    # Single-texture fragment, identity color
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_CLX_IDENT -DUSE_ATEST -DUSE_EMISSIVE
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx0_ident1
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_CLX_IDENT -DUSE_ATEST -DUSE_FOG -DUSE_EMISSIVE
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx0_ident1_fog

    # Single-texture fragment, fixed color
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_FIXED_COLOR -DUSE_ATEST -DUSE_EMISSIVE
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx0_fixed
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_FIXED_COLOR -DUSE_ATEST -DUSE_FOG -DUSE_EMISSIVE
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx0_fixed_fog

    # Single-texture fragment, entity color
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_ENT_COLOR -DUSE_ATEST -DUSE_EMISSIVE
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx0_ent
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_ENT_COLOR -DUSE_ATEST -DUSE_FOG -DUSE_EMISSIVE
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx0_ent_fog

    # Single-texture fragment, depth-fragment
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_CLX_IDENT -DUSE_ATEST -DUSE_DF
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx0_df

    # Double-texture fragment
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_TX1 -DUSE_EMISSIVE
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx1
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_TX1 -DUSE_FOG -DUSE_EMISSIVE
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx1_fog

    # Double-texture fragment, identity colors
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_CLX_IDENT -DUSE_TX1 -DUSE_EMISSIVE
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx1_ident1
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_CLX_IDENT -DUSE_TX1 -DUSE_FOG -DUSE_EMISSIVE
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx1_ident1_fog

    # Double-texture fragment, fixed colors
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_FIXED_COLOR -DUSE_TX1 -DUSE_EMISSIVE
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx1_fixed
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_FIXED_COLOR -DUSE_TX1 -DUSE_FOG -DUSE_EMISSIVE
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx1_fixed_fog

    # Double-texture fragment, non-identical colors
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_CL1 -DUSE_TX1 -DUSE_EMISSIVE
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx1_cl
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_CL1 -DUSE_TX1 -DUSE_FOG -DUSE_EMISSIVE
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx1_cl_fog

    # Triple-texture fragment
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_TX2 -DUSE_EMISSIVE
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx2
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_TX2 -DUSE_FOG -DUSE_EMISSIVE
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx2_fog

    # Triple-texture fragment, non-identical colors
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_CL2 -DUSE_TX2 -DUSE_EMISSIVE
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx2_cl
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_CL2 -DUSE_TX2 -DUSE_FOG -DUSE_EMISSIVE
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx2_cl_fog

    # ========================================================
    # NO-EMISSIVE FRAGMENT VARIANTS
    # Same template matrix minus USE_EMISSIVE, for passes without
    # the emissive attachment (screenmap, HUD, SDR).
    # ========================================================
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/light_frag.tmpl
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_light_ne
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/light_frag.tmpl -DUSE_FOG
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_light_fog_ne
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/light_frag.tmpl -DUSE_LINE
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_light_line_ne
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/light_frag.tmpl -DUSE_LINE -DUSE_FOG
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_light_line_fog_ne

    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_ATEST
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx0_ne
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_ATEST -DUSE_OVERBRIGHT
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx0_overbright_ne
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_ATEST -DUSE_OVERBRIGHT -DUSE_FOG
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx0_overbright_fog_ne
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_ATEST -DUSE_FOG
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx0_fog_ne
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_CLX_IDENT -DUSE_ATEST
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx0_ident1_ne
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_CLX_IDENT -DUSE_ATEST -DUSE_FOG
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx0_ident1_fog_ne
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_FIXED_COLOR -DUSE_ATEST
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx0_fixed_ne
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_FIXED_COLOR -DUSE_ATEST -DUSE_FOG
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx0_fixed_fog_ne
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_ENT_COLOR -DUSE_ATEST
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx0_ent_ne
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_ENT_COLOR -DUSE_ATEST -DUSE_FOG
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx0_ent_fog_ne

    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_TX1
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx1_ne
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_TX1 -DUSE_FOG
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx1_fog_ne
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_CLX_IDENT -DUSE_TX1
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx1_ident1_ne
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_CLX_IDENT -DUSE_TX1 -DUSE_FOG
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx1_ident1_fog_ne
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_FIXED_COLOR -DUSE_TX1
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx1_fixed_ne
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_FIXED_COLOR -DUSE_TX1 -DUSE_FOG
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx1_fixed_fog_ne
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_CL1 -DUSE_TX1
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx1_cl_ne
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_CL1 -DUSE_TX1 -DUSE_FOG
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx1_cl_fog_ne

    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_TX2
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx2_ne
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_TX2 -DUSE_FOG
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx2_fog_ne
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_CL2 -DUSE_TX2
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx2_cl_ne
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_CL2 -DUSE_TX2 -DUSE_FOG
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx2_cl_fog_ne

    # ========================================================
    # VIRTUAL SCREEN SHADERS (multiview for menu/follow mode)
    # Requires --target-env vulkan1.1 for gl_ViewIndex
    # ========================================================
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/virtualscreen.vert
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} virtualscreen_vert_spv
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/virtualscreen.frag
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} virtualscreen_frag_spv

    # Floor grid shaders (multiview)
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/floor_grid.vert
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} floor_grid_vert_spv
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/floor_grid.frag
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} floor_grid_frag_spv

    # Desktop mirror shaders (NOT multiview - renders to single-layer desktop swapchain)
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/desktopmirror.vert
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} desktopmirror_vert_spv
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/desktopmirror.frag
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} desktopmirror_frag_spv

    # Overlay zoom shaders (NOT multiview - gamma-correct weapon zoom to overlay swapchain)
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/overlayzoom.vert
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} overlayzoom_vert_spv
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/overlayzoom.frag
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}" bin2hex ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} overlayzoom_frag_spv

    # Cleanup temp file and copy to source tree atomically
    COMMAND ${CMAKE_COMMAND} -E remove -f ${SPIRV_DIR}/temp.spv
    COMMAND ${CMAKE_COMMAND} -E copy ${SHADER_DATA_OUTPUT} ${SHADER_DATA_FINAL}

    DEPENDS bin2hex ${SHADER_SOURCES}
    COMMENT "Compiling SPIR-V shaders into ${SHADER_DATA_FINAL}"
    VERBATIM
)

# Create target for shader compilation
add_custom_target(compile_shaders
    DEPENDS ${SHADER_DATA_FINAL}
    COMMENT "All shaders compiled"
)
