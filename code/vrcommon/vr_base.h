#ifndef __VR_BASE
#define __VR_BASE

#include "vr_types.h"

// Whether the OpenXR runtime advertises a given instance extension.
VR_Bool VR_HasInstanceExtension( const char *name );

VR_Engine* VR_Init( void );
VR_Engine* VR_GetEngine( void );
void VR_Destroy( VR_Engine* engine );
void VR_PrepareForShutdown( void );

// Deferred until the renderer DLL's GetRefAPI pulls the Vulkan device;
// idempotent because VR_EnterVR also calls it.
void VR_EnsureGraphicsInitialized( void );

void VR_EnterVR( VR_Engine* engine );
void VR_LeaveVR( VR_Engine* engine );

#endif
