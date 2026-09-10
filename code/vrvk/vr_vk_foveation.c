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

// Whether s_foveationCenter came from a gaze, which picks the tighter falloff.
// Stays set across a frame that fails to sample: the renderer rebuilds the whole
// map when it flips, so clearing it on a blink pulses the island's size twice.
static qboolean s_gazeHeld = qfalse;

// The display time the held centers were sampled at, in the same clock the
// frame hands in, so the hold below can time itself out with no second clock.
static XrTime s_gazeHeldTime = 0;

// How long centers are held with no valid sample before falling back to fixed.
// A blink costs ten to fifteen frames, so a second clears any blink or squint
// while still settling quickly when gaze goes away for real.
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

This frame's gaze direction, in the view space.

The extension's XrEyeGazeSampleTimeEXT is deliberately not read. It was, to log how
far a streamed pose trailed the display time, and it cannot answer that: the spec
requires only "the clamped, predicted or interpolated time" and states the field may
be in the future, so a sample time equal to or ahead of the display time is
conformant and measures nothing. That is a property of the extension, not of one
runtime, so no runtime is expected to answer it either.
==================
*/
static qboolean VR_VK_Foveation_SampleGaze(VR_Engine* engine, XrTime displayTime, XrPosef* pose)
{
	const XrSpaceLocationFlags required =
		XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
	XrSpaceLocation loc;
	const XrSpace gazeSpace = VR_GetEyeGazeSpace();

	// The one gate for every way gaze can be missing: the extension absent, the
	// system reporting no eye tracking, or the action, its binding or its space
	// having failed to come up
	if (gazeSpace == XR_NULL_HANDLE)
	{
		return qfalse;
	}

	// Whether an eye is tracked right now is a different question from whether the
	// space can be located, and lives in the action state. A runtime that has lost
	// the eye can still hand back a location flagged valid and tracked while
	// holding a default forward pose.
	if (!VR_EyeGazeIsActive())
	{
		return qfalse;
	}

	memset(&loc, 0, sizeof(loc));
	loc.type = XR_TYPE_SPACE_LOCATION;

	// HeadSpace, not CurrentSpace: it is XR_REFERENCE_SPACE_TYPE_VIEW (vr_base.c),
	// so the gaze comes back already in view space and the projection below needs
	// no view rotation and cannot drift with head turns
	if (xrLocateSpace(gazeSpace, engine->appState.HeadSpace, displayTime, &loc) != XR_SUCCESS)
	{
		return qfalse;
	}

	// Orientation alone: a gaze is a direction, and the position bit can be clear
	// on a runtime that reports the eye as a direction from the head. Tracked as
	// well as valid, since valid alone can cover an inferred or stale pose and an
	// inferred pose is not a gaze
	if ((loc.locationFlags & required) != required)
	{
		return qfalse;
	}

	*pose = loc.pose;
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
	// Fixed centers sit on the vertical field's angular bisector, not the optical
	// axis: an asymmetric field states where its designer expects the eye to go.
	// Hoisted so both places that write fixed centers mean the same thing by it.
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
		float centers[2][2];
		qboolean projected = qfalse;

		if (VR_VK_Foveation_SampleGaze(engine, displayTime, &gazePose))
		{
			XrVector3f gazeDir;

			// -Z rotated by the gaze orientation, which is the direction OpenXR poses face
			XrQuaternionf_RotateVector3f(&gazeDir, &gazePose.orientation, &forward);

			// An eye that fell back holds its optical axis, which is neither a gaze
			// nor what fixed mode uses. Failing the sample leaves the held centers
			// for the ladder below rather than putting the tight falloff on it.
			projected = VR_VK_Foveation_ProjectToEyes(&gazeDir, centers);
		}

		if (projected)
		{
			memcpy(s_foveationCenter, centers, sizeof(s_foveationCenter));
			s_gazeHeld = qtrue;
			s_gazeHeldTime = displayTime;
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
