#ifndef __VR_INPUT_H
#define __VR_INPUT_H

#include "../qcommon/q_shared.h"
#include "vr_types.h"

// Init
void VR_InitInstanceInput( VR_Engine* engine );
void VR_InitSessionInput( VR_Engine* engine );
void VR_DestroySessionInput( VR_Engine* engine );

// Eye gaze pose space, XR_NULL_HANDLE whenever gaze is unavailable
XrSpace VR_GetEyeGazeSpace( void );
const char* VR_EyeGazeBindingState( void );
// Whether an eye is actually being tracked this frame. The space location flags
// are not required to say so and at least one runtime leaves them permanently set
qboolean VR_EyeGazeIsActive( void );

// Render loop
void VR_RefreshDerivedModeState( void );
void VR_ProcessInputActions( void );
void IN_VRUpdateHMD( XrView* views, uint32_t viewCount, XrFovf* fov );
void IN_VRSyncActions( VR_Engine* engine );
void IN_VRUpdateControllers( VR_Engine* engine, XrTime predictedDisplayTime );

void VR_HapticEvent(const char* event, int position, int flags, int intensity, float angle, float yHeight );

const char* VR_GetMenuSkipButtonName( void );
const char* VR_GetMenuCancelButtonName( void );

void QuatToYawPitchRoll(XrQuaternionf q, vec3_t rotation, vec3_t out);

#endif
