/*
 * vr_virtual_screen.c - renderer-agnostic virtual-screen anchor ladder
 *
 * The virtual screen shows menus, the console, and first-person follow
 * mode. This module owns the anchor pose state and the model/view matrix
 * math; the renderer layer (vrvk) owns its graphics resources and
 * draw paths and pull poses from here.
 */

#include "vr_virtual_screen.h"

#include "vr_clientinfo.h"
#include "vr_graphics.h"
#include "vr_input.h"

extern vr_clientinfo_t vr;
extern cvar_t *vr_virtualScreenMode;

// Current virtual screen orientation (for tracking yaw)
static XrQuaternionf lastKnownVirtualScreenOrientation = { 0.0f, 0.0f, 0.0f, 1.0f };

// Position tracking state
static int positionsInitialized = 0;
static int updateTarget = 1;
static XrVector3f targetPosition;
static XrVector3f currentPosition;
static XrQuaternionf currentRotation;

// Screen anchor distance from the head, meters
static const float targetDistance = 3.0f;


/*
==================
GetPositionInFront

Head forward projected onto the horizontal plane, at the given distance and
the head's own height: the screen anchors upright at eye level regardless
of head pitch.
==================
*/
static XrVector3f GetPositionInFront(const XrPosef* pose, float distance)
{
	XrVector3f forward = { 0.0f, 0.0f, -1.0f };
	XrVector3f rotatedForward;
	XrQuaternionf_RotateVector3f(&rotatedForward, &pose->orientation, &forward);

	// Project to XZ plane (remove Y component to keep same height)
	rotatedForward.y = 0.0f;
	XrVector3f_Normalize(&rotatedForward);

	XrVector3f result = {
		pose->position.x + rotatedForward.x * distance,
		pose->position.y,
		pose->position.z + rotatedForward.z * distance
	};
	return result;
}


/*
==================
YawFacingQuaternion

Yaw-only rotation turning the screen toward the head: keeps the screen
upright instead of inheriting the anchoring head pose's pitch/roll.
==================
*/
static XrQuaternionf YawFacingQuaternion(const XrVector3f* from, const XrVector3f* to)
{
	float dx = to->x - from->x;
	float dz = to->z - from->z;

	float len = sqrtf(dx * dx + dz * dz);
	if (len < 1e-6f)
	{
		return (XrQuaternionf){ 0.0f, 0.0f, 0.0f, 1.0f };
	}
	dx /= len;
	dz /= len;

	float yaw = atan2f(dx, dz);
	float half = yaw * 0.5f;
	return (XrQuaternionf){ 0.0f, sinf(half), 0.0f, cosf(half) };
}


/*
==================
EnsureNewPositionInExpectedDistance

Clamp a drifting screen position back onto the targetDistance sphere around
the head so follow-mode drift never changes the apparent screen size.
==================
*/
static void EnsureNewPositionInExpectedDistance(const XrVector3f* hmdPosition, XrVector3f* vsPosition)
{
	XrVector3f virtualScreenToHmd;
	XrVector3f_Sub(&virtualScreenToHmd, vsPosition, hmdPosition);
	const float distance = XrVector3f_Length(&virtualScreenToHmd);

	const float delta = (distance > targetDistance ? (distance - targetDistance) : (targetDistance - distance));
	if (delta > 0.001f)
	{
		XrVector3f scaledVirtualScreenToHmd;
		XrVector3f_Normalize(&virtualScreenToHmd);
		XrVector3f_Scale(&scaledVirtualScreenToHmd, &virtualScreenToHmd, targetDistance);
		XrVector3f_Add(vsPosition, hmdPosition, &scaledVirtualScreenToHmd);
	}
}


/*
==================
GetCurrentVirtualScreenPositionAndRotation

Anchor ladder. Fixed mode (vr_virtualScreenMode 0): anchor once per reset.
Follow mode (1): hysteresis re-targeting; settle when the in-front point is
within 0.04*d of the target, track once it drifts past 0.60*d, teleport past
1.20*d, drift at 0.01/frame with the head distance held constant.
==================
*/
static void GetCurrentVirtualScreenPositionAndRotation(const XrPosef* headPose,
	XrVector3f* translation, XrQuaternionf* rotation)
{
	XrVector3f positionInFront = GetPositionInFront(headPose, targetDistance);

	if (!positionsInitialized)
	{
		currentPosition = positionInFront;
		targetPosition = positionInFront;
		currentRotation = YawFacingQuaternion(&currentPosition, &headPose->position);
		positionsInitialized = 1;
	}
	else if (vr_virtualScreenMode && vr_virtualScreenMode->integer == 1)
	{
		XrVector3f lastToCurrentVec;
		XrVector3f_Sub(&lastToCurrentVec, &targetPosition, &positionInFront);

		// The hysteresis was tuned at a 2.5 m anchor distance; scale the meter
		// thresholds with targetDistance so the angular feel stays the same
		// (re-target at ~35 deg of head turn, settle at ~2.3 deg) instead of
		// tightening as the distance grows
		const float settleDist   = 0.04f * targetDistance;
		const float retargetDist = 0.60f * targetDistance;
		const float teleportDist = 1.20f * targetDistance;

		const float updateTargetDist = XrVector3f_Length(&lastToCurrentVec);
		if (updateTargetDist < settleDist)
		{
			updateTarget = 0;
		}
		else if (updateTargetDist > retargetDist || updateTarget)
		{
			targetPosition = positionInFront;
			updateTarget = 1;

			if (updateTargetDist > teleportDist)
			{
				// Too far - teleport; we probably just started or switched into the virtual screen
				currentPosition = targetPosition;
			}
		}

		XrVector3f lerped;
		XrVector3f_Lerp(&lerped, &currentPosition, &targetPosition, 0.01f);
		EnsureNewPositionInExpectedDistance(&headPose->position, &lerped);
		currentPosition = lerped;
		currentRotation = YawFacingQuaternion(&currentPosition, &headPose->position);
	}

	*translation = currentPosition;
	*rotation = currentRotation;
}


/*
==================
VR_VirtualScreen_ComputeCenteredHeadPose

Eye-midpoint position with the left eye's orientation (both eyes'
orientations are nearly identical).
==================
*/
void VR_VirtualScreen_ComputeCenteredHeadPose(XrPosef* out, const XrView* views, uint32_t viewCount)
{
	const XrPosef* left = &views[0].pose;
	const XrPosef* right = &views[viewCount > 1 ? viewCount - 1 : 0].pose;

	out->position.x = (left->position.x + right->position.x) * 0.5f;
	out->position.y = (left->position.y + right->position.y) * 0.5f;
	out->position.z = (left->position.z + right->position.z) * 0.5f;
	out->orientation = left->orientation;
}


/*
==================
VR_VirtualScreen_GetModelMatrix

Create model matrix for the virtual screen mesh.
==================
*/
void VR_VirtualScreen_GetModelMatrix(XrMatrix4x4f* model, const XrPosef* centeredHead)
{
	// Base 4:3 aspect ratio for the virtual screen content
	float aspectRatioCoeff = 3.0f / 4.0f;

	// Compensate for non-square framebuffer aspect ratio
	if (vr.fov_x > 0.0f && vr.fov_y > 0.0f)
	{
		float framebufferAspect = vr.fov_y / vr.fov_x;
		aspectRatioCoeff *= framebufferAspect;
	}

	XrVector3f translation;
	XrQuaternionf rotation;
	GetCurrentVirtualScreenPositionAndRotation(centeredHead, &translation, &rotation);
	XrVector3f scale = { 3.0f, 3.0f * aspectRatioCoeff, 3.0f };

	lastKnownVirtualScreenOrientation = rotation;

	// Lower the screen slightly
	translation.y -= 0.5f;

	XrMatrix4x4f_CreateTranslationRotationScale(model, &translation, &rotation, &scale);
}


/*
==================
VR_VirtualScreen_GetFloorModelMatrix

Create model matrix for the floor grid quad.
==================
*/
void VR_VirtualScreen_GetFloorModelMatrix(XrMatrix4x4f* model)
{
	XrVector3f translation = { 0.0f, 0.0f, 0.0f };
	XrQuaternionf rotation = { 0.0f, 0.0f, 0.0f, 1.0f };
	XrVector3f scale = { 30.0f, 30.0f, -30.0f };

	// Align the floor with actual stage space instead of initial HMD rotation
	XrVector3f upAxis = { 0.0f, 1.0f, 0.0f };
	XrQuaternionf_CreateFromAxisAngle(&rotation, &upAxis, -vr.recenterYaw);

	XrMatrix4x4f_CreateTranslationRotationScale(model, &translation, &rotation, &scale);
}


/*
==================
VR_VirtualScreen_GetViewMatrix

Create view matrix from eye position and orientation.
==================
*/
void VR_VirtualScreen_GetViewMatrix(XrMatrix4x4f* result,
	const XrVector3f* translation, const XrQuaternionf* rotation)
{
	XrMatrix4x4f rotationMatrix, translationMatrix, viewMatrix;
	XrMatrix4x4f_CreateFromQuaternion(&rotationMatrix, rotation);
	XrMatrix4x4f_CreateTranslation(&translationMatrix, translation->x, translation->y, translation->z);
	XrMatrix4x4f_Multiply(&viewMatrix, &translationMatrix, &rotationMatrix);
	XrMatrix4x4f_Invert(result, &viewMatrix);
}


void VR_VirtualScreen_ResetPosition(void)
{
	positionsInitialized = 0;
	updateTarget = 1;
}


float VR_VirtualScreen_GetCurrentYaw(void)
{
	vec3_t rotation = { 0, 0, 0 };
	vec3_t yawPitchRoll = { 0, 0, 0 };
	QuatToYawPitchRoll(lastKnownVirtualScreenOrientation, rotation, yawPitchRoll);
	return yawPitchRoll[YAW];
}
