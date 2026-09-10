#ifndef __VR_CVARS
#define __VR_CVARS

#include "../qcommon/q_shared.h"

void VR_InitCvars( void );

// vr_foveation, vr_foveationStrength: see VR_FOVEATION_* / VR_FOVEATION_STRENGTH_* in vr_types.h
extern cvar_t *vr_foveation;
extern cvar_t *vr_foveationStrength;
extern cvar_t *vr_foveationCaps;  // CVAR_ROM, set from VR_FoveationCapsString()

#endif
