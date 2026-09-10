/*
 * vr_vk_foveation.h - Fixed foveation driver
 *
 * Where the sharp island belongs, once a frame. The renderer authors the
 * shading rate map itself (renderervk/vk.c) and is told only strength, mode
 * and one center an eye; nothing here talks to Vulkan.
 */

#ifndef __VR_VK_FOVEATION
#define __VR_VK_FOVEATION

#include "../vrcommon/vr_types.h"

// Per-frame upkeep: clamp the cvars against what the device can do, then hand
// the renderer the strength and the centers
void VR_VK_Foveation_Frame(VR_Engine* engine);

#endif // __VR_VK_FOVEATION
