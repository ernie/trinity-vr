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
// tr_flares.c

#include "tr_local.h"

/*
=============================================================================

LIGHT FLARES

A light flare is an effect that takes place inside the eye when bright light
sources are visible.  The size of the flare relative to the screen is nearly
constant, irrespective of distance, but the intensity should be proportional to the
projected area of the light source.

A surface that has been flagged as having a light flare will calculate the depth
buffer value that its midpoint should have when the surface is added.

After all opaque surfaces have been rendered, the depth buffer is read back for
each flare in view.  If the point has not been obscured by a closer surface, the
flare should be drawn.

Surfaces that have a repeated texture should never be flagged as flaring, because
there will only be a single flare added at the midpoint of the polygon.

To prevent abrupt popping, the intensity of the flare is interpolated up and
down as it changes visibility.  This involves scene to scene state, unlike almost
all other aspects of the renderer, and is complicated by the fact that a single
frame may have multiple scenes.

RB_RenderFlares() will be called once per view (twice in a mirrored scene, potentially
up to five or more times in a frame with 3D status bar icons).

=============================================================================
*/


// flare states maintain visibility over multiple frames for fading
// layers: view, mirror, menu
typedef struct flare_s {
	struct		flare_s	*next;		// for active chain

	int			addedFrame;
	uint32_t	testCount;

	portalView_t portalView;
	int			frameSceneNum;
	void		*surface;
	int			fogNum;

	int			fadeTime;

	qboolean	visible;			// state of last test
	float		drawIntensity;		// may be non 0 even if !visible due to fading
	float		deferredIntensity;	// drawIntensity captured at own-view test; later PV_NONE views (HUD icons) zero drawIntensity before the deferred draw

	int			windowX, windowY;
	float		eyeZ;
	float		drawZ;

	// Owning view's basis/metrics captured in RB_AddFlare so the deferred draw
	// sizes/orients the world-space billboard from the flare's own view instead
	// of whatever (HUD-icon/2D) view backEnd.viewParms holds at the 2D boundary.
	int			viewportWidth;		// owning view's viewport width, for corona sizing
	float		projScaleX;			// owning view's projectionMatrix[0] (1/tanHalfFovX)
	vec3_t		viewOrigin;			// owning view's eye position
	vec3_t		viewLeft;			// owning view's or.axis[1]
	vec3_t		viewUp;				// owning view's or.axis[2]

	vec3_t		origin;
	vec3_t		color;
} flare_t;

static flare_t	r_flareStructs[ MAX_FLARES ];
static flare_t	*r_activeFlares, *r_inactiveFlares;

// Main (PV_NONE) view stereo projection + mono modelview, captured in
// RB_RenderFlares so RB_RenderDeferredFlares can re-establish the exact main
// view camera after later views have overwritten the eyeProj UBO / viewParms.
static float	deferredEyeProj[2][16];
static float	deferredModelMatrix[16];

// In-world VR HUD sprite deferral: the HUD RT_SPRITE is intercepted in
// RB_SurfaceSprite during the main view and replayed in post_bloom AFTER the corona
// (RB_DrawDeferredHud) so opaque HUD pixels composite over the additive corona. All
// state is captured in the back end at sprite-draw time so the replay uses the exact
// main-view camera the sprite would have drawn with, independent of r_flares.
static float		deferredHudEyeProj[2][16];	// main view's per-eye projections
static float		deferredHudModelMatrix[16];	// main view's mono world modelview
static vec3_t		deferredHudOrigin;			// world-space sprite center
static vec3_t		deferredHudLeft, deferredHudUp;	// resolved (aspect/rotation-applied) billboard axes
static color4ub_t	deferredHudColor;			// e.shaderRGBA
static int			deferredHudFogNum;


/*
==================
R_ClearFlares
==================
*/
void R_ClearFlares( void ) {
	int		i;

	if ( !vk.fragmentStores )
		return;

	Com_Memset( r_flareStructs, 0, sizeof( r_flareStructs ) );
	r_activeFlares = NULL;
	r_inactiveFlares = NULL;

	for ( i = 0 ; i < MAX_FLARES ; i++ ) {
		r_flareStructs[i].next = r_inactiveFlares;
		r_inactiveFlares = &r_flareStructs[i];
	}
}


static flare_t *R_SearchFlare( void *surface )
{
	flare_t *f;

	// see if a flare with a matching surface, scene, and view exists
	for ( f = r_activeFlares ; f ; f = f->next ) {
		if ( f->surface == surface && f->frameSceneNum == backEnd.viewParms.frameSceneNum && f->portalView == backEnd.viewParms.portalView ) {
			return f;
		}
	}

	return NULL;
}


/*
==================
RB_AddFlare

This is called at surface tesselation time
==================
*/
void RB_AddFlare( void *surface, int fogNum, vec3_t point, vec3_t color, vec3_t normal ) {
	int				i;
	flare_t			*f;
	vec3_t			local;
	float			d = 1;
	vec4_t			eye, clip, normalized, window;

	backEnd.pc.c_flareAdds++;

	if ( normal && (normal[0] || normal[1] || normal[2] ) )	{
		VectorSubtract( backEnd.viewParms.or.origin, point, local );
		VectorNormalizeFast( local );
		d = DotProduct( local, normal );
		// If the viewer is behind the flare don't add it.
		if ( d < 0 ) {
			return;
		}
	}

	// if the point is off the screen, don't bother adding it
	// calculate screen coordinates and depth
	R_TransformModelToClip( point, backEnd.or.modelMatrix, backEnd.viewParms.projectionMatrix, eye, clip );

	// check to see if the point is completely off screen
	for ( i = 0 ; i < 3 ; i++ ) {
		if ( clip[i] >= clip[3] || clip[i] <= -clip[3] ) {
			return;
		}
	}

	R_TransformClipToWindow( clip, &backEnd.viewParms, normalized, window );

	if ( window[0] < 0 || window[0] >= backEnd.viewParms.viewportWidth || window[1] < 0 || window[1] >= backEnd.viewParms.viewportHeight ) {
		return;	// shouldn't happen, since we check the clip[] above, except for FP rounding
	}

	f = R_SearchFlare( surface );

	// allocate a new one
	if ( !f ) {
		if ( !r_inactiveFlares ) {
			// the list is completely full
			return;
		}
		f = r_inactiveFlares;
		r_inactiveFlares = r_inactiveFlares->next;
		f->next = r_activeFlares;
		r_activeFlares = f;

		f->surface = surface;
		f->frameSceneNum = backEnd.viewParms.frameSceneNum;
		f->portalView = backEnd.viewParms.portalView;
		f->visible = qfalse;
		f->fadeTime = backEnd.refdef.time - 2000;
		f->testCount = 0;
	} else {
		++f->testCount;
	}

	f->addedFrame = backEnd.viewParms.frameCount;
	f->fogNum = fogNum;

	VectorCopy( point, f->origin );
	VectorCopy( color, f->color );

	// fade the intensity of the flare down as the
	// light surface turns away from the viewer
	VectorScale( f->color, d, f->color );

	// save info needed to test
	f->windowX = backEnd.viewParms.viewportX + window[0];
	f->windowY = backEnd.viewParms.viewportY + window[1];

	// captured now (own view) so the deferred draw doesn't size/orient off
	// whatever view is last at the 2D boundary
	f->viewportWidth = backEnd.viewParms.viewportWidth;
	f->projScaleX = backEnd.viewParms.projectionMatrix[0];
	VectorCopy( backEnd.viewParms.or.origin, f->viewOrigin );
	VectorCopy( backEnd.viewParms.or.axis[1], f->viewLeft );
	VectorCopy( backEnd.viewParms.or.axis[2], f->viewUp );

	f->eyeZ = eye[2];

#ifdef USE_REVERSED_DEPTH
	f->drawZ = (clip[2]+0.20) / clip[3];
#else
	f->drawZ = (clip[2]-0.20) / clip[3];
#endif

}


/*
==================
RB_AddDlightFlares
==================
*/
void RB_AddDlightFlares( void ) {
	dlight_t		*l;
	int				i, j, k;
	fog_t			*fog = NULL;

	if ( !r_flares->integer ) {
		return;
	}

	l = backEnd.refdef.dlights;

	if ( tr.world )
		fog = tr.world->fogs;

	for ( i = 0 ; i < backEnd.refdef.num_dlights; i++, l++ ) {

		if ( fog )
		{
			// find which fog volume the light is in
			for ( j = 1 ; j < tr.world->numfogs ; j++ ) {
				fog = &tr.world->fogs[j];
				for ( k = 0 ; k < 3 ; k++ ) {
					if ( l->origin[k] < fog->bounds[0][k] || l->origin[k] > fog->bounds[1][k] ) {
						break;
					}
				}
				if ( k == 3 ) {
					break;
				}
			}
			if ( j == tr.world->numfogs ) {
				j = 0;
			}
		}
		else
			j = 0;

		RB_AddFlare( (void *)l, j, l->origin, l->color, NULL );
	}
}

/*
===============================================================================

FLARE BACK END

===============================================================================
*/


/*
==================
RB_TestFlare
==================
*/
static void RB_TestFlare( flare_t *f ) {
	qboolean		visible;
	float			fade;
	float			clipPos[16];
	vec4_t			eye, clip;
	uint32_t		offset;
	int				i;

	backEnd.pc.c_flareTests++;

/*
	We don't have equivalent of glReadPixels() in vulkan
	and explicit depth buffer reading may be very slow and require surface conversion.

	So we will use storage buffer and exploit early depth tests by
	rendering a test dot at the flare's projected position, biased slightly
	toward the viewer: if the dot is not covered by any world geometry it
	invokes the fragment shader, which fills the storage buffer at the
	desired location, then discards the fragment.
	In next frame we read storage buffer: if there is a non-zero value
	then our flare WAS visible (as we're working with 1-frame delay),
	multisampled image will cause multiple fragment shader invocations.

	VR: the probe position must be computed PER EYE with the same eyeProj
	matrices the scene rendered with: each view's depth buffer is shifted
	by asymmetric-FOV offset + IPD parallax relative to the mono projection,
	so a mono-positioned dot samples pixels several degrees away from where
	the flare actually is in either eye. The two clip-space positions go in
	the vertex push range; dot.vert selects by gl_ViewIndex.
*/

	// we neeed only single uint32_t but take care of alignment
	offset = (f - r_flareStructs) * vk.storage_alignment;

	if ( f->testCount ) {
		uint32_t *cnt = (uint32_t*)(vk.storage.buffer_ptr + offset);
		if ( *cnt )
			visible = qtrue;
		else
			visible = qfalse;

		f->testCount = 1;
	} else {
		visible = qfalse;
	}

	// reset the test result; this frame's probe fragment sets it again if it
	// survives the depth test in either view. The reset must live here on the
	// CPU rather than in dot.vert: multiview gives no cross-view ordering
	// between vertex and fragment invocations, so a vertex-stage reset in one
	// view could clobber the other view's pass.
	*((uint32_t*)(vk.storage.buffer_ptr + offset)) = 0x00;

	// per-eye probe positions: clip = eyeProj[e] * (worldModelView * origin),
	// biased toward the viewer: exactly the transform the scene drew with
	Com_Memset( clipPos, 0, sizeof( clipPos ) );
	for ( i = 0; i < 2; i++ ) {
		R_TransformModelToClip( f->origin, backEnd.viewParms.world.modelMatrix,
			vk_view_eyeproj[i], eye, clip );
#ifdef USE_REVERSED_DEPTH
		clip[2] += 0.20f;
#else
		clip[2] -= 0.20f;
#endif
		Com_Memcpy( clipPos + i * 4, clip, sizeof( vec4_t ) );
	}
	// params slot: clip-space extent of ~2 pixels (per unit w) so dot.vert
	// can expand the probe into a sub-pixel triangle around the pixel center
	clipPos[8] = 4.0f / (float)vk.renderWidth;
	clipPos[9] = 4.0f / (float)vk.renderHeight;
	// dot.vert reads the first 48 bytes of the 64-byte vertex push range as
	// vec4 clipPos[2] + vec4 params; reuse the matrix push plumbing
	vk_update_mvp( clipPos );

	// three dummy vertices for the probe triangle: the pipeline's vertex
	// input still binds location 0, but the probe position comes from the
	// push constants (expanded per-vertex via gl_VertexIndex)
	Com_Memset( tess.xyz, 0, 3 * sizeof( tess.xyz[0] ) );
	tess.numVertexes = 3;

#ifdef USE_VBO
	tess.vboIndex = 0;
#endif
	// invalidate descriptors
	for ( i = 0; i < VK_DESC_COUNT; i++ ) {
		vk_reset_descriptor( i );
	}
	// render test dot
	vk_bind_pipeline( vk.dot_pipeline );
	vk_bind_geometry( TESS_XYZ );
	vk_draw_dot( offset );

	//Com_Memcpy( vk_world.modelview_transform, modelMatrix_original, sizeof( modelMatrix_original ) );
	//vk_update_mvp( NULL );

	if ( visible ) {
		if ( !f->visible ) {
			f->visible = qtrue;
			f->fadeTime = backEnd.refdef.time - 1;
		}
		fade = ( ( backEnd.refdef.time - f->fadeTime ) /1000.0f ) * r_flareFade->value;
	} else {
		if ( f->visible ) {
			f->visible = qfalse;
			f->fadeTime = backEnd.refdef.time - 1;
		}
		fade = 1.0f - ( ( backEnd.refdef.time - f->fadeTime ) / 1000.0f ) * r_flareFade->value;
	}

	if ( fade < 0 ) {
		fade = 0;
	} else if ( fade > 1 ) {
		fade = 1;
	}

	f->drawIntensity = fade;
}


/*
==================
RB_RenderFlare
==================
*/
static void RB_RenderFlare( flare_t *f ) {
	float			size;
	vec3_t			color;
	float distance, intensity, factor;
	float radius;
	vec3_t			dir, left, up;
	byte fogFactors[3] = {255, 255, 255};
	color4ub_t		c;

	// RB_RenderFlares culls drawIntensity == 0 before calling here
	//if ( f->drawIntensity == 0.0 )
	//	return;

	backEnd.pc.c_flareRenders++;

	// We don't want too big values anyways when dividing by distance.
	if ( f->eyeZ > -1.0f )
		distance = 1.0f;
	else
		distance = -f->eyeZ;

	// calculate the flare size.. use the flare's own captured viewport width so a
	// deferred draw isn't mis-sized by the last (HUD-icon) view's viewParms
	size = f->viewportWidth * ( r_flareSize->value/640.0f + 8 / distance );

/*
 * This is an alternative to intensity scaling. It changes the size of the flare on screen instead
 * with growing distance. See in the description at the top why this is not the way to go.
	// size will change ~ 1/r.
	size = f->viewportWidth * (r_flareSize->value / (distance * -2.0f));
*/

/*
 * As flare sizes stay nearly constant with increasing distance we must decrease the intensity
 * to achieve a reasonable visual result. The intensity is ~ (size^2 / distance^2) which can be
 * got by considering the ratio of
 * (flaresurface on screen) : (Surface of sphere defined by flare origin and distance from flare)
 * An important requirement is:
 * intensity <= 1 for all distances.
 *
 * The formula used here to compute the intensity is as follows:
 * intensity = flareCoeff * size^2 / (distance + size*sqrt(flareCoeff))^2
 * As you can see, the intensity will have a max. of 1 when the distance is 0.
 * The coefficient flareCoeff will determine the falloff speed with increasing distance.
 */

	factor = distance + size * sqrt( r_flareCoeff->value );

	intensity = r_flareCoeff->value * size * size / ( factor * factor );

	VectorScale( f->color, f->drawIntensity * intensity, color );

	// Calculations for fogging
	if ( tr.world && f->fogNum > 0 && f->fogNum < tr.world->numfogs )
	{
		tess.numVertexes = 1;
		VectorCopy( f->origin, tess.xyz[0] );
		tess.fogNum = f->fogNum;

		RB_CalcModulateColorsByFog( fogFactors );

		// We don't need to render the flare if colors are 0 anyways.
		if ( !(fogFactors[0] || fogFactors[1] || fogFactors[2]) )
			return;
	}

	c.rgba[0] = color[0] * fogFactors[0];
	c.rgba[1] = color[1] * fogFactors[1];
	c.rgba[2] = color[2] * fogFactors[2];
	c.rgba[3] = 255;

	// falloff/fog can quantize the color to black; an additive black quad
	// contributes nothing but still pays full depth-test-disabled fill
	if ( !( c.rgba[0] | c.rgba[1] | c.rgba[2] ) )
		return;

	// Depth handling: the flare stage pipelines have the depth test disabled
	// (rebaked in CreateExternalShaders), so the billboard is not swallowed
	// by the light-fixture surface it sits on. Occlusion is the probe's job.
	RB_BeginSurface( tr.flareShader, f->fogNum );

	// World-space billboard at the flare origin, sized to subtend the same
	// screen fraction as the classic window-space quad (`size` pixels of
	// viewportWidth at `distance`): half-width = 2 * size/W * distance * tanHalfFovX.
	// The stereo projection is applied per eye by the pipeline, so the corona
	// gets correct parallax and asymmetric-FOV placement in VR. All view metrics
	// come from the flare's own captured view (f->) so the deferred draw is not
	// mis-sized/oriented by the last (HUD-icon/2D) view's viewParms.
	radius = 2.0f * distance * ( size / f->viewportWidth ) / f->projScaleX;

	// Viewer-facing basis: the quad faces the viewer's POSITION (view-plane
	// alignment tracks head orientation instead and foreshortens at
	// peripheral gaze angles); in-plane spin follows the view's up vector.
	VectorSubtract( f->origin, f->viewOrigin, dir );
	VectorNormalizeFast( dir );
	CrossProduct( f->viewUp, dir, left );
	if ( DotProduct( left, left ) < 0.0001f ) {
		// sight line parallel to view up: fall back to view left
		VectorCopy( f->viewLeft, left );
	} else {
		VectorNormalizeFast( left );
	}
	CrossProduct( dir, left, up );

	VectorScale( left, radius, left );
	VectorScale( up, radius, up );

	if ( f->portalView == PV_MIRROR ) {
		VectorSubtract( vec3_origin, left, left );
	}

	RB_AddQuadStamp( f->origin, left, up, c );

	RB_EndSurface();
}


/*
==================
RB_RenderFlares

Because flares are simulating an occular effect, they should be drawn after
everything (all views) in the entire frame has been drawn.

Because of the way portals use the depth buffer to mark off areas, the
needed information would be lost after each view, so we are forced to draw
flares after each view.

The resulting artifact is that flares in mirrors or portals don't dim properly
when occluded by something in the main view, and portal flares that should
extend past the portal edge will be overwritten.
==================
*/
void RB_RenderFlares( void ) {
	flare_t		*f;
	flare_t		**prev;
	qboolean	draw;

	if ( !r_flares->integer ) {
		return;
	}

	if ( vk.renderPassIndex == RENDER_PASS_SCREENMAP ) {
		return;
	}

	if ( backEnd.isHyperspace ) {
		return;
	}

	// Reset currentEntity to world so that any previously referenced entities
	// don't have influence on the rendering of these flares (i.e. RF_ renderer flags).
	backEnd.currentEntity = &tr.worldEntity;
	backEnd.or = backEnd.viewParms.world;

	//RB_AddDlightFlares();

	// perform z buffer readback on each flare in this view
	draw = qfalse;
	prev = &r_activeFlares;
	while ( ( f = *prev ) != NULL ) {
		// throw out any flares that weren't added last frame
		if ( backEnd.viewParms.frameCount - f->addedFrame > 0 && f->portalView == backEnd.viewParms.portalView ) {
			*prev = f->next;
			f->next = r_inactiveFlares;
			r_inactiveFlares = f;
			continue;
		}

		// don't draw any here that aren't from this scene / portal
		f->drawIntensity = 0;
		if ( f->frameSceneNum == backEnd.viewParms.frameSceneNum && f->portalView == backEnd.viewParms.portalView ) {
			RB_TestFlare( f );
			// deferred draw runs after later PV_NONE views (3D HUD icons) zero
			// drawIntensity for this flare; preserve the real intensity here
			f->deferredIntensity = f->drawIntensity;
			if ( f->testCount == 0 ) {
				// recently added, wait 1 frame for test result
			} else if ( f->drawIntensity ) {
				draw = qtrue;
			} else {
				// this flare has completely faded out, so remove it from the chain
				*prev = f->next;
				f->next = r_inactiveFlares;
				r_inactiveFlares = f;
				continue;
			}
		}

		prev = &f->next;
	}

	if ( !draw ) {
		return;		// none visible
	}

	// Main-view coronas draw later via RB_RenderDeferredFlares (after bloom's
	// bright-pass, so they aren't re-bloomed). portal/mirror views keep the
	// classic per-view timing here, or a deferred draw would leak past the
	// portal edge / lose occlusion by the main view.
	if ( backEnd.viewParms.portalView == PV_NONE ) {
		// Capture this (main) view's per-eye projections and mono modelview so
		// the deferred draw can re-establish the exact camera: by 2D-boundary
		// time backEnd.viewParms / the eyeProj UBO belong to a later view.
		Com_Memcpy( deferredEyeProj, vk_view_eyeproj, sizeof( deferredEyeProj ) );
		Com_Memcpy( deferredModelMatrix, backEnd.viewParms.world.modelMatrix, sizeof( deferredModelMatrix ) );
		return;
	}

	// Flare quads are world-space billboards: push the mono world modelview
	// (same matrix world surfaces draw with) and let the per-view eyeProj UBO,
	// still holding this view's per-eye projections, provide the stereo
	// projection. Window-space ortho must NOT be used here: the generic
	// pipelines multiply by eyeProj, which garbles pre-projected vertices.
	vk_update_mvp( backEnd.viewParms.world.modelMatrix );

	for ( f = r_activeFlares ; f ; f = f->next ) {
		if ( f->frameSceneNum == backEnd.viewParms.frameSceneNum && f->portalView == backEnd.viewParms.portalView && f->drawIntensity ) {
			RB_RenderFlare( f );
		}
	}

	//Com_Memcpy( vk_world.modelview_transform, modelMatrix_original, sizeof( modelMatrix_original ) );
	//vk_update_mvp( NULL );
}


/*
==================
RB_RenderDeferredFlares

Draws main-view (PV_NONE) coronas once per frame at the 3D->2D boundary, after
vk_bloom()'s bright-pass, so coronas aren't re-bloomed. doneFlares guards the
once-per-frame; the draw is idempotent across the two bloom hook sites.

The coronas are world-space billboards, so unlike the engine's window-space
deferred draw this must re-establish the MAIN view's camera: by the time we run,
backEnd.viewParms and the per-view eyeProj UBO belong to a later HUD-icon or 2D
pass. We restore the main view's per-eye projections (captured in RB_RenderFlares)
into the eyeProj UBO and push the main view's mono modelview; per-flare sizing/
orientation comes from state captured in RB_AddFlare.
==================
*/
void RB_RenderDeferredFlares( void ) {
	flare_t				*f;
	const trRefEntity_t	*savedEntity;

	if ( !r_flares->integer || backEnd.doneFlares )
		return;

	// Skip pure-2D frames (menu/disconnect): frameCount is frozen on the last 3D
	// frame there, so stale flares would still match and paint coronas over the UI.
	if ( !backEnd.doneSurfaces )
		return;

	// checked before marking done, so a screenmap pass can't suppress the real deferred draw
	if ( vk.renderPassIndex == RENDER_PASS_SCREENMAP )
		return;

	// Deferred coronas push a world modelview; if we somehow reach here in a 2D
	// projection (e.g. the end-of-frame fallback after 2D began) vk_update_mvp
	// would ignore it and mis-transform the quad. Only draw from the 3D->2D
	// boundary where projection2D is still false.
	if ( backEnd.projection2D )
		return;

	backEnd.doneFlares = qtrue;

	// save/restore currentEntity so the following 2D batch flushes with the entity the caller expects
	savedEntity = backEnd.currentEntity;
	backEnd.currentEntity = &tr.worldEntity;

	// Re-establish the main view's per-eye projections in the eyeProj UBO. The
	// UBO now holds a later (HUD-icon/2D) view's data, so without this the
	// world-space corona would be stereo-mismatched / mis-projected in-headset.
	Com_Memcpy( vk_view_eyeproj, deferredEyeProj, sizeof( vk_view_eyeproj ) );
	VK_PushEyeProj();

	// World-space billboards: push the captured main-view mono modelview; the
	// restored per-eye eyeProj UBO supplies the stereo projection.
	vk_update_mvp( deferredModelMatrix );

	for ( f = r_activeFlares ; f ; f = f->next ) {
		if ( f->portalView == PV_NONE && f->addedFrame == backEnd.viewParms.frameCount ) {
			// restore intensity zeroed by later PV_NONE (3D HUD icon) views since this flare's own test
			f->drawIntensity = f->deferredIntensity;
			if ( f->drawIntensity )
				RB_RenderFlare( f );
		}
	}

	// Restore MVP state so the flare camera can't leak into subsequent 2D: when
	// projection2D is already set (end-of-frame hook site) this re-pushes the 2D
	// ortho + eyeProj; otherwise it just restores the mono modelview push.
	vk_update_mvp( NULL );
	backEnd.currentEntity = savedEntity;
}


/*
==================
RB_CaptureDeferredHud

Called from RB_SurfaceSprite (back end, main view) with the HUD sprite's already-
resolved world-space billboard. Stashes the geometry plus the exact main-view camera
(per-eye projections + mono modelview) so the deferred replay is independent of
r_flares and of whatever view later owns the eyeProj UBO / MVP push.
==================
*/
void RB_CaptureDeferredHud( const vec3_t origin, const vec3_t left, const vec3_t up, color4ub_t color ) {
	VectorCopy( origin, deferredHudOrigin );
	VectorCopy( left, deferredHudLeft );
	VectorCopy( up, deferredHudUp );
	deferredHudColor = color;
	deferredHudFogNum = tess.fogNum;

	Com_Memcpy( deferredHudEyeProj, vk_view_eyeproj, sizeof( deferredHudEyeProj ) );
	Com_Memcpy( deferredHudModelMatrix, backEnd.viewParms.world.modelMatrix, sizeof( deferredHudModelMatrix ) );

	backEnd.hudDeferred = qtrue;
}


/*
==================
RB_DrawDeferredHud

Replays the captured in-world HUD sprite in the post_bloom pass, AFTER
RB_RenderDeferredFlares has drawn the corona. Draws once per frame even
though two post_bloom hook sites call it. Runs unconditionally of r_flares so the HUD
still appears when flares are disabled. RF_DEPTHHACK is honored via DEPTH_RANGE_WEAPON
against the scene depth the post_bloom pass LOADs. That depth is the main pass's
resolved single sample, taken with MAX where the device supports it, so at a
silhouette the sprite tests against the nearest of the covering samples and
occludes conservatively - the accepted trade for a single-sample post pass.
The HUD alpha-blends over the corona.
==================
*/
void RB_DrawDeferredHud( void ) {
	const trRefEntity_t	*savedEntity;

	if ( !backEnd.hudDeferred )
		return;

	// checked before consuming so a screenmap pass can't suppress the real replay
	if ( vk.renderPassIndex == RENDER_PASS_SCREENMAP )
		return;

	// World-space quad needs the 3D camera; never draw once the 2D ortho is active
	// (mirrors the deferred corona's guard for the end-of-frame fallback site).
	if ( backEnd.projection2D )
		return;

	// consume: draw exactly once even though both post_bloom hook sites call us
	backEnd.hudDeferred = qfalse;

	savedEntity = backEnd.currentEntity;
	backEnd.currentEntity = &tr.worldEntity;
	backEnd.or = backEnd.viewParms.world;

	// Re-establish the captured main-view per-eye projections + mono modelview: by
	// now a later HUD-icon/2D view owns the eyeProj UBO and the MVP push.
	Com_Memcpy( vk_view_eyeproj, deferredHudEyeProj, sizeof( vk_view_eyeproj ) );
	VK_PushEyeProj();
	vk_update_mvp( deferredHudModelMatrix );

	RB_BeginSurface( tr.hudShader, deferredHudFogNum );
	// RF_DEPTHHACK: same weapon depth range the sprite used in the main pass.
	tess.depthRange = DEPTH_RANGE_WEAPON;
	RB_AddQuadStamp( deferredHudOrigin, deferredHudLeft, deferredHudUp, deferredHudColor );
	// The sprite carries the HUD, so it is read rather than looked past; without
	// this its corners coarsen wherever the falloff puts them.
	vk.rateExempt = qtrue;
	RB_EndSurface();
	vk.rateExempt = qfalse;
	tess.depthRange = DEPTH_RANGE_NORMAL; // reset; nothing after should inherit the weapon range

	// Restore MVP so the HUD camera can't leak into subsequent 2D drawing.
	vk_update_mvp( NULL );
	backEnd.currentEntity = savedEntity;
}
