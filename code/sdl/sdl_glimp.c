/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#ifdef USE_INTERNAL_SDL_HEADERS
#	include "SDL.h"
#else
#	include <SDL.h>
#endif

#include <vulkan/vulkan.h>
#include <SDL_vulkan.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "../renderercommon/tr_common.h"
#include "../sys/sys_local.h"
#include "../client/client.h"
#include "sdl_icon.h"

#include "../vrcommon/vr_base.h"
#include "../vrcommon/vr_input.h"
#include "../vrcommon/vr_renderer.h"

typedef enum
{
	RSERR_OK,

	RSERR_INVALID_FULLSCREEN,
	RSERR_INVALID_MODE,

	RSERR_UNKNOWN
} rserr_t;

SDL_Window *SDL_window = NULL;

// The renderer owns glconfig_t (frozen ABI); the client-side glimp fills the
// caller's instance through this pointer, set at the top of VKimp_Init.
glconfig_t *glimp_config = NULL;

// Used only by client-side mode selection; renderers never read it.
float displayAspect = 0.0f;

cvar_t *r_allowResize; // make window resizable
cvar_t *r_centerWindow;
cvar_t *r_sdlDriver;

/*
===============
GLimp_Minimize

Minimize the game so that user is back at the desktop
===============
*/
void GLimp_Minimize( void )
{
	SDL_MinimizeWindow( SDL_window );
}


/*
===============
GLimp_CompareModes
===============
*/
static int GLimp_CompareModes( const void *a, const void *b )
{
	const float ASPECT_EPSILON = 0.001f;
	SDL_Rect *modeA = (SDL_Rect *)a;
	SDL_Rect *modeB = (SDL_Rect *)b;
	float aspectA = (float)modeA->w / (float)modeA->h;
	float aspectB = (float)modeB->w / (float)modeB->h;
	int areaA = modeA->w * modeA->h;
	int areaB = modeB->w * modeB->h;
	float aspectDiffA = fabs( aspectA - displayAspect );
	float aspectDiffB = fabs( aspectB - displayAspect );
	float aspectDiffsDiff = aspectDiffA - aspectDiffB;

	if( aspectDiffsDiff > ASPECT_EPSILON )
		return 1;
	else if( aspectDiffsDiff < -ASPECT_EPSILON )
		return -1;
	else
		return areaA - areaB;
}


/*
===============
GLimp_DetectAvailableModes
===============
*/
static void GLimp_DetectAvailableModes(void)
{
	int i, j;
	char buf[ MAX_STRING_CHARS ] = { 0 };
	int numSDLModes;
	SDL_Rect *modes;
	int numModes = 0;

	SDL_DisplayMode windowMode;
	int display = SDL_GetWindowDisplayIndex( SDL_window );
	if( display < 0 )
	{
		Com_Printf( "Couldn't get window display index, no resolutions detected: %s\n", SDL_GetError() );
		return;
	}
	numSDLModes = SDL_GetNumDisplayModes( display );

	if( SDL_GetWindowDisplayMode( SDL_window, &windowMode ) < 0 || numSDLModes <= 0 )
	{
		Com_Printf( "Couldn't get window display mode, no resolutions detected: %s\n", SDL_GetError() );
		return;
	}

	modes = SDL_calloc( (size_t)numSDLModes, sizeof( SDL_Rect ) );
	if ( !modes )
	{
		Com_Error( ERR_FATAL, "Out of memory" );
	}

	for( i = 0; i < numSDLModes; i++ )
	{
		SDL_DisplayMode mode;

		if( SDL_GetDisplayMode( display, i, &mode ) < 0 )
			continue;

		if( !mode.w || !mode.h )
		{
			Com_Printf( "Display supports any resolution\n" );
			SDL_free( modes );
			return;
		}

		if( windowMode.format != mode.format )
			continue;

		// SDL can give the same resolution with different refresh rates.
		// Only list resolution once.
		for( j = 0; j < numModes; j++ )
		{
			if( mode.w == modes[ j ].w && mode.h == modes[ j ].h )
				break;
		}

		if( j != numModes )
			continue;

		modes[ numModes ].w = mode.w;
		modes[ numModes ].h = mode.h;
		numModes++;
	}

	if( numModes > 1 )
		qsort( modes, numModes, sizeof( SDL_Rect ), GLimp_CompareModes );

	for( i = 0; i < numModes; i++ )
	{
		const char *newModeString = va( "%ux%u ", modes[ i ].w, modes[ i ].h );

		if( strlen( newModeString ) < (int)sizeof( buf ) - strlen( buf ) )
			Q_strcat( buf, sizeof( buf ), newModeString );
		else
			Com_Printf( "Skipping mode %ux%u, buffer too small\n", modes[ i ].w, modes[ i ].h );
	}

	if( *buf )
	{
		buf[ strlen( buf ) - 1 ] = 0;
		Com_Printf( "Available modes: '%s'\n", buf );
		Cvar_Set( "r_availableModes", buf );
	}
	SDL_free( modes );
}

// [OpenXR] (Re)create session and renderer if needed
void GLimp_InitVR(void)
{
	VR_Engine* engine = VR_GetEngine();
	if (engine->appState.Session == XR_NULL_HANDLE) {
		VR_EnterVR(engine);
		VR_InitRenderer(engine);
    VR_InitSessionInput(engine);
		VR_Renderer_RestoreState(engine);
	}
}


/*
===============
GLimp_EndFrame

Applies an r_fullscreen change to the window. The Vulkan swapchain presents on its own.
===============
*/
void GLimp_EndFrame( void )
{
	// Renderer-owned cvar; cached after first lookup (runs once per frame).
	static cvar_t *cvFullscreen = NULL;

	if ( !cvFullscreen )
		cvFullscreen = Cvar_Get( "r_fullscreen", "1", 0 );

	if( cvFullscreen->modified )
	{
		int         fullscreen;
		qboolean    needToToggle;
		qboolean    sdlToggled = qfalse;

		// Find out the current state
		fullscreen = !!( SDL_GetWindowFlags( SDL_window ) & SDL_WINDOW_FULLSCREEN );

		if( cvFullscreen->integer && Cvar_VariableIntegerValue( "in_nograb" ) )
		{
			Com_Printf( "Fullscreen not allowed with in_nograb 1\n");
			Cvar_Set( "r_fullscreen", "0" );
			cvFullscreen->modified = qfalse;
		}

		// Is the state we want different from the current state?
		needToToggle = !!cvFullscreen->integer != fullscreen;

		if( needToToggle )
		{
			sdlToggled = SDL_SetWindowFullscreen( SDL_window, cvFullscreen->integer ) >= 0;

			// SDL_WM_ToggleFullScreen didn't work, so do it the slow way
			if( !sdlToggled )
				Cbuf_ExecuteText(EXEC_APPEND, "vid_restart\n");

			IN_Restart( );
		}

		cvFullscreen->modified = qfalse;
	}
}


/*
===============
VKimp_SetMode

Creates a Vulkan-compatible SDL window (no OpenGL context).
For Q3VR: Vulkan device is created by VR layer, we just need the window for desktop mirror.
===============
*/
static rserr_t VKimp_SetMode(int mode, qboolean fullscreen, qboolean noborder)
{
	int colorBits, depthBits, stencilBits;
	int samples;
	int x = SDL_WINDOWPOS_UNDEFINED, y = SDL_WINDOWPOS_UNDEFINED;
	Uint32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN;
	SDL_DisplayMode desktopMode;
	int display = 0;
	SDL_Surface *icon = NULL;
	// Renderer-owned cvars (renderervk/tr_init.c registration: default+flags matched)
	cvar_t *cvColorbits = Cvar_Get( "r_colorbits", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	cvar_t *cvDepthbits = Cvar_Get( "r_depthbits", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	cvar_t *cvStencilbits = Cvar_Get( "r_stencilbits", "8", CVAR_ARCHIVE_ND | CVAR_LATCH );

	Com_Printf( "Initializing Vulkan display\n");

	if ( r_allowResize->integer )
		flags |= SDL_WINDOW_RESIZABLE;

#ifdef USE_ICON
	icon = SDL_CreateRGBSurfaceFrom(
			(void *)CLIENT_WINDOW_ICON.pixel_data,
			CLIENT_WINDOW_ICON.width,
			CLIENT_WINDOW_ICON.height,
			CLIENT_WINDOW_ICON.bytes_per_pixel * 8,
			CLIENT_WINDOW_ICON.bytes_per_pixel * CLIENT_WINDOW_ICON.width,
#ifdef Q3_LITTLE_ENDIAN
			0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000
#else
			0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF
#endif
			);
#endif

	if( SDL_window != NULL )
	{
		display = SDL_GetWindowDisplayIndex( SDL_window );
		if( display < 0 )
		{
			Com_DPrintf( "SDL_GetWindowDisplayIndex() failed: %s\n", SDL_GetError() );
			display = 0;
		}
	}

	if( SDL_GetDesktopDisplayMode( display, &desktopMode ) == 0 )
	{
		displayAspect = (float)desktopMode.w / (float)desktopMode.h;
		Com_Printf( "Display aspect: %.3f\n", displayAspect );
	}
	else
	{
		Com_Memset( &desktopMode, 0, sizeof( SDL_DisplayMode ) );
		Com_Printf( "Cannot determine display aspect, assuming 1.333\n" );
	}

	Com_Printf( "...setting mode %d:", mode );

	VR_Engine* engine = VR_GetEngine();
	VR_GetResolution(engine, &glimp_config->vidWidth, &glimp_config->vidHeight);
	glimp_config->windowAspect = (float)glimp_config->vidWidth / (float)glimp_config->vidHeight;

	int windowWidth, windowHeight;

	const int desktopWidth = Cvar_VariableIntegerValue("r_customdesktopwidth");
	const int desktopHeight = Cvar_VariableIntegerValue("r_customdesktopheight");
	if ( desktopWidth <= 0 || desktopHeight <= 0 )
	{
		Cvar_SetValue("r_customdesktopwidth", desktopMode.w);
		Cvar_SetValue("r_customdesktopheight", desktopMode.h);
	}
	else
	{
		desktopMode.w = desktopWidth;
		desktopMode.h = desktopHeight;
	}
	windowWidth = desktopMode.w;
	windowHeight = desktopMode.h;

	Com_Printf( " %d %d\n", glimp_config->vidWidth, glimp_config->vidHeight);

	if( SDL_window != NULL )
	{
		SDL_GetWindowPosition( SDL_window, &x, &y );
		Com_DPrintf( "Existing window at %dx%d before being destroyed\n", x, y );
		SDL_DestroyWindow( SDL_window );
		SDL_window = NULL;
	}

	if( fullscreen )
	{
		flags |= SDL_WINDOW_FULLSCREEN;
		glimp_config->isFullscreen = qtrue;
	}
	else
	{
		if( noborder )
			flags |= SDL_WINDOW_BORDERLESS;

		glimp_config->isFullscreen = qfalse;
	}

	colorBits = cvColorbits->value;
	if (colorBits == 0 || colorBits > 32)
		colorBits = 32;

	if (!cvDepthbits->value)
		depthBits = 24;
	else
		depthBits = cvDepthbits->value;

	stencilBits = cvStencilbits->value;
	samples = 0;

	if( r_centerWindow->integer && !fullscreen )
	{
		x = ( desktopMode.w / 2 ) - ( windowWidth / 2 );
		y = ( desktopMode.h / 2 ) - ( windowHeight / 2 );
	}

	SDL_window = SDL_CreateWindow(CLIENT_WINDOW_TITLE, x, y,
			windowWidth, windowHeight, flags);

	if (!SDL_window)
	{
		Com_Printf( "Couldn't create Vulkan window: %s\n", SDL_GetError() );
		SDL_FreeSurface( icon );
		return RSERR_INVALID_MODE;
	}

	SDL_SetWindowIcon( SDL_window, icon );
	SDL_FreeSurface( icon );

	glimp_config->colorBits = colorBits;
	glimp_config->depthBits = depthBits;
	glimp_config->stencilBits = stencilBits;

	GLimp_DetectAvailableModes();

	engine->window.width = windowWidth;
	engine->window.height = windowHeight;

	if (Cvar_VariableIntegerValue("vr_desktopMode") == 0)
	{
		SDL_HideWindow(SDL_window);
	}

	Com_Printf( "Created Vulkan window %dx%d (XR render: %dx%d)\n",
		windowWidth, windowHeight, glimp_config->vidWidth, glimp_config->vidHeight);

	return RSERR_OK;
}


#define R_MODE_FALLBACK 3 // 640 * 480

/*
===============
VKimp_Init

Initialize Vulkan for Q3VR.
This creates the SDL window for desktop mirror.
Vulkan instance/device is already created by the VR layer.
===============
*/
void VKimp_Init(glconfig_t *config)
{
	glimp_config = config;

	Com_DPrintf( "VKimp_Init()\n" );

	r_sdlDriver = Cvar_Get( "r_sdlDriver", "", CVAR_ROM );
	r_allowResize = Cvar_Get( "r_allowResize", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_centerWindow = Cvar_Get( "r_centerWindow", "0", CVAR_ARCHIVE | CVAR_LATCH );

	// Renderer-owned cvars (renderervk/tr_init.c registration: default+flags matched)
	cvar_t *cvMode = Cvar_Get( "r_mode", "-2", CVAR_ARCHIVE_ND | CVAR_LATCH );
	cvar_t *cvFullscreen = Cvar_Get( "r_fullscreen", "1", CVAR_ARCHIVE_ND );
	cvar_t *cvNoborder = Cvar_Get( "r_noborder", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );

	if( Cvar_VariableIntegerValue( "com_abnormalExit" ) )
	{
		Cvar_Set( "r_mode", va( "%d", R_MODE_FALLBACK ) );
		Cvar_Set( "r_fullscreen", "0" );
		Cvar_Set( "r_centerWindow", "0" );
		Cvar_Set( "com_abnormalExit", "0" );
	}

	Sys_GLimpInit();

	if (!SDL_WasInit(SDL_INIT_VIDEO))
	{
		if (SDL_Init(SDL_INIT_VIDEO) != 0)
		{
			Com_Error( ERR_FATAL, "SDL_Init( SDL_INIT_VIDEO ) FAILED (%s)", SDL_GetError());
			return;
		}

		Com_Printf( "SDL using driver \"%s\"\n", SDL_GetCurrentVideoDriver() );
	}

	if (VKimp_SetMode(cvMode->integer, cvFullscreen->integer, cvNoborder->integer) != RSERR_OK)
	{
		Sys_GLimpSafeInit();

		if (VKimp_SetMode(cvMode->integer, cvFullscreen->integer, qfalse) != RSERR_OK)
		{
			if( cvMode->integer != R_MODE_FALLBACK )
			{
				Com_Printf( "Setting r_mode %d failed, falling back on r_mode %d\n",
						cvMode->integer, R_MODE_FALLBACK );

				if (VKimp_SetMode(R_MODE_FALLBACK, qfalse, qfalse) != RSERR_OK)
				{
					Com_Error( ERR_FATAL, "VKimp_Init() - could not create Vulkan window" );
					return;
				}
			}
		}
	}

	// Fill in glConfig
	glimp_config->driverType = GLDRV_ICD;
	glimp_config->hardwareType = GLHW_GENERIC;
	glimp_config->deviceSupportsGamma = qfalse;  // VR headsets handle gamma

	// Vulkan doesn't use these GL strings, but fill in something useful
	Q_strncpyz(glimp_config->vendor_string, "Vulkan VR", sizeof(glimp_config->vendor_string));
	Q_strncpyz(glimp_config->renderer_string, "Q3VR Vulkan Renderer", sizeof(glimp_config->renderer_string));
	Q_strncpyz(glimp_config->version_string, "Vulkan 1.1", sizeof(glimp_config->version_string));
	glimp_config->extensions_string[0] = '\0';

	Cvar_Get( "r_availableModes", "", CVAR_ROM );

	// This depends on SDL_INIT_VIDEO, hence having it here
	IN_Init( SDL_window );
}


/*
===============
VKimp_Shutdown
===============
*/
void VKimp_Shutdown(qboolean unloadDLL)
{
	(void)unloadDLL;  // Not used: DLL management handled elsewhere

	IN_Shutdown();

	// [OpenXR] Destroy renderer and current XR session due to loss of Vulkan context,
	// will recreate it on next renderer init.
	VR_Engine* engine = VR_GetEngine();
	VR_DestroySessionInput(engine);
	VR_DestroyRenderer(engine);
	VR_LeaveVR(engine);

	// Reinitialize OpenXR instance and system, otherwise there might be visual
	// glitches/problems with swapchains when new session is (re)created.
	VR_Destroy(engine);
	VR_Init();

	if (SDL_window)
	{
		SDL_DestroyWindow(SDL_window);
		SDL_window = NULL;
	}

	SDL_QuitSubSystem(SDL_INIT_VIDEO);

	glimp_config = NULL;
}


/*
===============
VK_CreateSurface

Create a VkSurfaceKHR for the SDL window (for desktop mirror).
===============
*/
qboolean VK_CreateSurface(void *instance, void *pSurface)
{
	VkSurfaceKHR *surface = (VkSurfaceKHR *)pSurface;

	if (!SDL_window)
	{
		Com_Printf( "VK_CreateSurface: No SDL window\n");
		return qfalse;
	}

	if (!SDL_Vulkan_CreateSurface(SDL_window, (VkInstance)instance, surface))
	{
		Com_Printf( "VK_CreateSurface: SDL_Vulkan_CreateSurface failed: %s\n", SDL_GetError());
		return qfalse;
	}

	return qtrue;
}
