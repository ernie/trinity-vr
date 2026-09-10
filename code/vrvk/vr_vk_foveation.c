/*
 * vr_vk_foveation.c - Foveation driver
 *
 * See vr_vk_foveation.h.
 */

#include "vr_vk_foveation.h"
#include "vr_vk.h"

#include "../vrcommon/vr_base.h"
#include "../vrcommon/vr_clientinfo.h"
#include "../vrcommon/vr_cvars.h"
#include "../vrcommon/vr_input.h"
#include "../vrcommon/common/xr_linear.h"

#include "../client/client.h"
#include "../qcommon/qcommon.h"

#include <math.h>

extern vr_clientinfo_t vr;

// The sharp island's center per eye, in NDC. Fixed mode sits on the optical
// axis; eye-tracked mode holds the last gaze that located.
static float s_foveationCenter[2][2] = { { 0.0f, 0.0f }, { 0.0f, 0.0f } };

// Whether s_foveationCenter came from a gaze rather than from the optical axis.
// It picks the renderer's tighter eye-tracked falloff, and it deliberately
// stays set across a frame that fails to sample: the renderer rebuilds the
// whole map whenever this flips, so clearing it on a blink would pulse the
// island's size twice per blink.
static qboolean s_gazeHeld = qfalse;

// The display time the held centers were sampled at, in the same clock the
// frame hands in, so the hold below can time itself out with no second clock.
static XrTime s_gazeHeldTime = 0;

// How long the centers are held with no valid sample before gaze is given up on
// and the island goes back to the optical axis. A blink invalidates the pose for
// ten to fifteen frames, so a second is ninety frames at 90 Hz: far past any
// blink or squint, and still quick enough that gaze genuinely going away -- eye
// tracking switched off, the headset lifted off -- reads as the effect settling
// rather than as an island stuck where the player last looked.
#define VR_GAZE_HOLD_TIMEOUT_NS 1000000000LL

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
VR_VK_Foveation_SampleGaze

This frame's gaze direction, in the view space. The sample time rides along on the
location's next chain: a streaming runtime can hand back a pose measured several
frames ago, and the gap against the predicted display time is the only way to see it.
==================
*/
static qboolean VR_VK_Foveation_SampleGaze(VR_Engine* engine, XrTime displayTime, XrPosef* pose, XrTime* sampleTime)
{
	const XrSpaceLocationFlags required =
		XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
	XrEyeGazeSampleTimeEXT gazeSample;
	XrSpaceLocation loc;
	const XrSpace gazeSpace = VR_GetEyeGazeSpace();

	// The one gate for every way gaze can be missing: the extension absent, the
	// system reporting no eye tracking, or the action, its binding or its space
	// having failed to come up
	if (gazeSpace == XR_NULL_HANDLE)
	{
		return qfalse;
	}

	memset(&gazeSample, 0, sizeof(gazeSample));
	gazeSample.type = XR_TYPE_EYE_GAZE_SAMPLE_TIME_EXT;

	memset(&loc, 0, sizeof(loc));
	loc.type = XR_TYPE_SPACE_LOCATION;
	loc.next = &gazeSample;

	// HeadSpace, not CurrentSpace: it is XR_REFERENCE_SPACE_TYPE_VIEW (vr_base.c),
	// so the gaze comes back already in view space and the projection below needs
	// no view rotation and cannot drift with head turns
	if (xrLocateSpace(gazeSpace, engine->appState.HeadSpace, displayTime, &loc) != XR_SUCCESS)
	{
		return qfalse;
	}

	// Orientation alone: a gaze is a direction, and the position bit can be clear
	// on a runtime that reports the eye as a direction from the head. Tracked as
	// well as valid, because valid alone can cover an inferred or stale pose, and
	// taking one of those for this frame's gaze would make the age log below read
	// fresh when it is not
	if ((loc.locationFlags & required) != required)
	{
		return qfalse;
	}

	*pose = loc.pose;
	*sampleTime = gazeSample.time;
	return qtrue;
}

/*
==================
VR_VK_Foveation_GazeCenters

The gaze direction as an NDC point per eye. Same convention as the optical center:
y runs down the image, and the frustum is asymmetric, so the middle of the buffer
is not straight ahead.
==================
*/
static void VR_VK_Foveation_GazeCenters(const XrPosef* gaze, float centers[2][2])
{
	const float tanUp = tanf(vr.fov_angle_up);
	const float tanDown = tanf(vr.fov_angle_down);
	const float spanY = tanUp - tanDown;
	XrVector3f forward = { 0.0f, 0.0f, -1.0f };
	XrVector3f dir;
	int eye;

	// -Z rotated by the gaze orientation, which is the direction OpenXR poses face
	XrQuaternionf_RotateVector3f(&dir, &gaze->orientation, &forward);

	for (eye = 0; eye < 2; ++eye)
	{
		const float tanLeft = tanf(vr.eye_fov_angle_left[eye]);
		const float tanRight = tanf(vr.eye_fov_angle_right[eye]);
		const float spanX = tanRight - tanLeft;
		float u, v;

		// Behind the eye, or edge-on: keep the optical axis rather than
		// projecting through a near-zero divisor
		if (dir.z >= -0.01f || fabsf(spanX) < 1e-6f || fabsf(spanY) < 1e-6f)
		{
			centers[eye][0] = (fabsf(spanX) > 1e-6f) ? -(tanLeft + tanRight) / spanX : 0.0f;
			centers[eye][1] = (fabsf(spanY) > 1e-6f) ? (tanUp + tanDown) / spanY : 0.0f;
			continue;
		}

		u = dir.x / -dir.z;
		v = dir.y / -dir.z;

		centers[eye][0] = 2.0f * (u - tanLeft) / spanX - 1.0f;
		centers[eye][1] = 1.0f - 2.0f * (v - tanDown) / spanY;
	}
}

/*
==================
VR_VK_Foveation_Caps

What this device can actually follow.
==================
*/
static VR_FoveationCaps VR_VK_Foveation_Caps(void)
{
	const VR_VulkanDeviceInfo* info = VR_Vulkan_GetDeviceInfo();

	if (!info || !info->shadingRateSupported)
		return VR_FOVEATION_CAPS_NONE;

	// Gaze without a shading-rate attachment is still NONE above -- there is
	// nothing to steer.
	return VR_HasEyeGazeSupport() ? VR_FOVEATION_CAPS_EYE_TRACKED : VR_FOVEATION_CAPS_FIXED;
}

void VR_VK_Foveation_Frame(VR_Engine* engine, XrTime displayTime)
{
	const VR_FoveationCaps caps = VR_VK_Foveation_Caps();
	int mode;
	int strength;

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
		s_gazeHeld = qfalse;
	}
	else if (mode == VR_FOVEATION_EYE_TRACKED && !vr.virtual_screen)
	{
		XrPosef gazePose;
		XrTime sampleTime = 0;

		if (VR_VK_Foveation_SampleGaze(engine, displayTime, &gazePose, &sampleTime))
		{
			VR_VK_Foveation_GazeCenters(&gazePose, s_foveationCenter);
			s_gazeHeld = qtrue;
			s_gazeHeldTime = displayTime;

			// TEMPORARY, remove before this branch ships.
			//
			// A streaming runtime can hand back a pose measured several frames
			// ago. At 90 Hz a frame is 11 ms, so an age in the tens of
			// milliseconds means the island trails a saccade badly enough to be
			// worse than fixed centers. A runtime that ignores the sample time
			// leaves it at 0, where the subtraction would print nanoseconds
			// since the runtime's epoch, so no line at all means the age is
			// unknown rather than zero.
			if (sampleTime != 0)
			{
				static int windowStart = 0;
				const int now = Sys_Milliseconds();

				if (windowStart == 0 || now - windowStart >= 1000)
				{
					Com_Printf("Foveation: gaze age %.1f ms\n",
						(double)(displayTime - sampleTime) / 1000000.0);
					windowStart = now;
				}
			}
		}
		else if (!s_gazeHeld || displayTime - s_gazeHeldTime > VR_GAZE_HOLD_TIMEOUT_NS)
		{
			// Nothing good to hold yet, or nothing good for long enough that this
			// has stopped being a blink and become gaze going away: either way the
			// island belongs back on the optical axis with the fixed falloff,
			// rather than frozen where the player last looked
			VR_VK_Foveation_OpticalCenter(s_foveationCenter);
			s_gazeHeld = qfalse;
		}
		// else: write nothing, holding the last good gaze centers. A blink
		// invalidates the pose for a few frames, and snapping the island back to
		// the optical axis and out again each time is far more visible than
		// leaving it where the player was last looking.
	}
	else
	{
		VR_VK_Foveation_OpticalCenter(s_foveationCenter);
		s_gazeHeld = qfalse;
	}

	re.SetFoveation(strength, s_gazeHeld, (const float (*)[2])s_foveationCenter);
}
