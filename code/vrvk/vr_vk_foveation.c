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

// The sharp island's center per eye, in NDC. Fixed mode sits on the angular
// bisector of the vertical field; eye-tracked mode holds the last gaze that located.
static float s_foveationCenter[2][2] = { { 0.0f, 0.0f }, { 0.0f, 0.0f } };

// Whether s_foveationCenter came from a gaze rather than from fixed centers.
// It picks the renderer's tighter eye-tracked falloff, and it deliberately
// stays set across a frame that fails to sample: the renderer rebuilds the
// whole map whenever this flips, so clearing it on a blink would pulse the
// island's size twice per blink.
static qboolean s_gazeHeld = qfalse;

// The display time the held centers were sampled at, in the same clock the
// frame hands in, so the hold below can time itself out with no second clock.
static XrTime s_gazeHeldTime = 0;

// How long the centers are held with no valid sample before gaze is given up on
// and the island goes back to fixed centers. A blink invalidates the pose for
// ten to fifteen frames, so a second is ninety frames at 90 Hz: far past any
// blink or squint, and still quick enough that gaze genuinely going away -- eye
// tracking switched off, the headset lifted off -- reads as the effect settling
// rather than as an island stuck where the player last looked.
#define VR_GAZE_HOLD_TIMEOUT_NS 1000000000LL

/*
==================
VR_VK_Foveation_ProjectToEyes

A head-local direction as an NDC point per eye. Two things move the answer away from
the middle of the buffer, and both must be applied: the frustum is asymmetric, so the
optical axis is already off center, and a canted display carries that axis outward
again, so head-forward does not land on it either. Rotating into the eye's frame first
handles both, and is identity on a headset reporting no cant.

Returns qfalse if any eye fell back rather than projecting, so a caller that must know
whether it got a real answer can ask. Either way both eyes are always written, fallback
included: two call sites and a local-plus-copy in the gaze arm all read both unconditionally.
==================
*/
static qboolean VR_VK_Foveation_ProjectToEyes(const XrVector3f* dirHead, float centers[2][2])
{
	const float tanUp = tanf(vr.fov_angle_up);
	const float tanDown = tanf(vr.fov_angle_down);
	const float spanY = tanUp - tanDown;
	qboolean projected = qtrue;
	int eye;

	for (eye = 0; eye < 2; ++eye)
	{
		const float tanLeft = tanf(vr.eye_fov_angle_left[eye]);
		const float tanRight = tanf(vr.eye_fov_angle_right[eye]);
		const float spanX = tanRight - tanLeft;
		// Copied, not cast: vrQuaternionf_t matches XrQuaternionf's layout, but
		// matching layouts are not permission to alias through a pointer
		const XrQuaternionf eyeLocal = {
			vr.eyeLocalRotation[eye].x, vr.eyeLocalRotation[eye].y,
			vr.eyeLocalRotation[eye].z, vr.eyeLocalRotation[eye].w };
		XrQuaternionf eyeInv;
		XrVector3f dir;
		float u, v;

		// eyeLocalRotation takes eye-local axes to head-local, so its inverse is
		// what brings a head-local direction into the eye's frame. Rotating by it
		// uninverted doubles the cant instead of removing it, and no runtime here
		// reports a cant to catch that with.
		XrQuaternionf_Invert(&eyeInv, &eyeLocal);
		XrQuaternionf_RotateVector3f(&dir, &eyeInv, dirHead);

		// Behind the eye, or edge-on: keep the optical axis rather than
		// projecting through a near-zero divisor
		if (dir.z >= -0.01f || fabsf(spanX) < 1e-6f || fabsf(spanY) < 1e-6f)
		{
			centers[eye][0] = (fabsf(spanX) > 1e-6f) ? -(tanLeft + tanRight) / spanX : 0.0f;
			centers[eye][1] = (fabsf(spanY) > 1e-6f) ? (tanUp + tanDown) / spanY : 0.0f;
			projected = qfalse;
			continue;
		}

		u = dir.x / -dir.z;
		v = dir.y / -dir.z;

		// y runs down the image
		centers[eye][0] = 2.0f * (u - tanLeft) / spanX - 1.0f;
		centers[eye][1] = 1.0f - 2.0f * (v - tanDown) / spanY;
	}

	return projected;
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
	// -Z is both the head's own forward and the axis an OpenXR pose faces along
	const XrVector3f forward = { 0.0f, 0.0f, -1.0f };
	// Fixed centers sit on the angular bisector of the vertical field, not the optical
	// axis: a headset that gives more field one way than the other is stating where
	// its designer expects the eye to go, so the island follows that lean. Shared by
	// both places fixed centers get written -- the mode chosen outright, and gaze
	// lost for long enough to give up on -- so those two can't drift into meaning
	// different things by "fixed". On this hardware (44 up, 55 down) that is 5.5
	// degrees below the axis.
	const float bisectorMid = 0.5f * (vr.fov_angle_up + vr.fov_angle_down);
	const XrVector3f bisector = { 0.0f, sinf(bisectorMid), -cosf(bisectorMid) };
	int mode;
	int strength;
	int eye;
	float fovTan[2][4];

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
	// the map's center never lines up with that panel, so foveating it only
	// softens text where nobody is looking past anything
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
		float centers[2][2];
		qboolean projected = qfalse;

		if (VR_VK_Foveation_SampleGaze(engine, displayTime, &gazePose, &sampleTime))
		{
			XrVector3f gazeDir;

			// -Z rotated by the gaze orientation, which is the direction OpenXR poses face
			XrQuaternionf_RotateVector3f(&gazeDir, &gazePose.orientation, &forward);

			// An eye that fell back holds the optical axis, not a gaze -- no longer
			// what fixed mode uses either, now that it sits on the bisector. Committing
			// it would put the tighter eye-tracked falloff on a center with no gaze
			// behind it, so let it fail the sample and leave the held centers alone
			// for the ladder below to judge.
			projected = VR_VK_Foveation_ProjectToEyes(&gazeDir, centers);
		}

		if (projected)
		{
			memcpy(s_foveationCenter, centers, sizeof(s_foveationCenter));
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
			// island belongs back on fixed centers, with the fixed falloff, rather
			// than frozen where the player last looked
			VR_VK_Foveation_ProjectToEyes(&bisector, s_foveationCenter);
			s_gazeHeld = qfalse;
		}
		// else: write nothing, holding the last good gaze centers. A blink
		// invalidates the pose for a few frames, and snapping the island back to
		// fixed centers and out again each time is far more visible than leaving
		// it where the player was last looking.
	}
	else
	{
		VR_VK_Foveation_ProjectToEyes(&bisector, s_foveationCenter);
		s_gazeHeld = qfalse;
	}

	// vr.fov_angle_up/down are already averaged across both eyes by
	// IN_VRUpdateHMD (vr_input.c), so the vertical pair below is shared rather
	// than read per eye. Exact on both runtimes here, since vertical field of
	// view does not differ between eyes the way the horizontal asymmetry does.
	for (eye = 0; eye < 2; ++eye)
	{
		fovTan[eye][0] = tanf(vr.eye_fov_angle_left[eye]);
		fovTan[eye][1] = tanf(vr.eye_fov_angle_right[eye]);
		fovTan[eye][2] = tanf(vr.fov_angle_up);
		fovTan[eye][3] = tanf(vr.fov_angle_down);
	}

	re.SetFoveation(strength, s_gazeHeld, (const float (*)[2])s_foveationCenter, (const float (*)[4])fovTan);
}
