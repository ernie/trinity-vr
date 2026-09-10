#ifndef __VR_BASE
#define __VR_BASE

#include "vr_types.h"

// Whether the OpenXR runtime advertises a given instance extension.
VR_Bool VR_HasInstanceExtension( const char *name );

// Whether a given instance extension was in the list handed to
// xrCreateInstance, and so enabled on the instance.
VR_Bool VR_HasEnabledInstanceExtension( const char *name );

// The runtime's name and version, as VR_Init already queried them. Empty
// string before VR_Init has created the instance.
const char *VR_GetRuntimeDescription( void );

// The XrVersion VR_Init declared to xrCreateInstance, as "major.minor.patch".
// Empty string before VR_Init has created the instance.
const char *VR_GetDeclaredApiVersion( void );

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
