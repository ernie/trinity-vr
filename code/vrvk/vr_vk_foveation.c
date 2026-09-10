/*
 * vr_vk_foveation.c - Fixed foveation driver
 *
 * See vr_vk_foveation.h.
 */

#include "vr_vk_foveation.h"
#include "vr_vk.h"

#include "../vrcommon/vr_clientinfo.h"
#include "../vrcommon/vr_cvars.h"

#include "../client/client.h"
#include "../qcommon/qcommon.h"

#include <math.h>

extern vr_clientinfo_t vr;

// The sharp island's center per eye, in NDC. Fixed mode sits on the optical
// axis; Plan B replaces this with the last valid gaze.
static float s_foveationCenter[2][2] = { { 0.0f, 0.0f }, { 0.0f, 0.0f } };

/*
==================
VR_VK_Foveation_OpticalCenter

Straight ahead in the gaze's NDC. The FOV is asymmetric, so the optical axis sits off
the middle of the eye buffer, by a different amount on each headset.
==================
*/
static void VR_VK_Foveation_OpticalCenter(float centers[2][2])
{
	const float tanUp = tanf(vr.fov_angle_up);
	const float tanDown = tanf(vr.fov_angle_down);
	const float spanY = tanUp - tanDown;
	int eye;

	for (eye = 0; eye < 2; ++eye)
	{
		const float tanLeft = tanf(vr.eye_fov_angle_left[eye]);
		const float tanRight = tanf(vr.eye_fov_angle_right[eye]);
		const float spanX = tanRight - tanLeft;

		centers[eye][0] = (fabsf(spanX) > 1e-6f) ? -(tanLeft + tanRight) / spanX : 0.0f;
		// y runs down the image, matching the convention the gaze uses
		centers[eye][1] = (fabsf(spanY) > 1e-6f) ? (tanUp + tanDown) / spanY : 0.0f;
	}
}

/*
==================
VR_VK_Foveation_Caps

What this device can actually follow. Gaze needs a runtime extension nothing in
Plan A requests, so the answer never rises above fixed yet.
==================
*/
static VR_FoveationCaps VR_VK_Foveation_Caps(void)
{
	const VR_VulkanDeviceInfo* info = VR_Vulkan_GetDeviceInfo();

	return (info && info->shadingRateSupported) ? VR_FOVEATION_CAPS_FIXED : VR_FOVEATION_CAPS_NONE;
}

void VR_VK_Foveation_Frame(VR_Engine* engine)
{
	const VR_FoveationCaps caps = VR_VK_Foveation_Caps();
	int mode;
	int strength;

	(void)engine;

	if (!vr_foveation || !vr_foveationStrength || !re.SetFoveation)
	{
		return;
	}

	mode = vr_foveation->integer;
	if (caps == VR_FOVEATION_CAPS_NONE)
	{
		// Write the forced-off state back so the menu row does not claim a mode
		// this device has no shading-rate attachment to actually perform
		mode = VR_FOVEATION_OFF;
		Cvar_Set("vr_foveation", "0");
	}
	else if (mode == VR_FOVEATION_EYE_TRACKED && caps < VR_FOVEATION_CAPS_EYE_TRACKED)
	{
		// Write the fallback back so the menu row shows what is in force rather
		// than what was asked for
		mode = VR_FOVEATION_FIXED;
		Cvar_Set("vr_foveation", "1");
	}
	vr_foveation->modified = qfalse;
	vr_foveationStrength->modified = qfalse;

	// The virtual screen fills the eye buffer with a panel the player reads, and
	// the map is centered on the optical axis rather than on that panel, so
	// foveating it only softens text where nobody is looking past anything
	strength = (mode == VR_FOVEATION_OFF || vr.virtual_screen) ? 0 : vr_foveationStrength->integer;

	if (vr.weapon_zoomed)
	{
		// The scope aims at the middle of a symmetric render, where neither an
		// eye's optical axis nor a gaze projected through that eye lands
		s_foveationCenter[0][0] = s_foveationCenter[0][1] = 0.0f;
		s_foveationCenter[1][0] = s_foveationCenter[1][1] = 0.0f;
	}
	else
	{
		VR_VK_Foveation_OpticalCenter(s_foveationCenter);
	}

	re.SetFoveation(strength, qfalse, (const float (*)[2])s_foveationCenter);
}
