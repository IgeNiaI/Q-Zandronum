/*
** gl_scene.cpp
** manages the rendering of the player's view
**
**---------------------------------------------------------------------------
** Copyright 2004-2005 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
** 4. When not used as part of GZDoom or a GZDoom derivative, this code will be
**    covered by the terms of the GNU Lesser General Public License as published
**    by the Free Software Foundation; either version 2.1 of the License, or (at
**    your option) any later version.
** 5. Full disclosure of the entire project's source code, except for third
**    party libraries is mandatory. (NOTE: This clause is non-negotiable!)
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include "gl/system/gl_system.h"
#include "gi.h"
#include "m_png.h"
#include "m_random.h"
#include "st_stuff.h"
#include "dobject.h"
#include "doomstat.h"
#include "g_level.h"
#include "r_data/r_interpolate.h"
#include "r_utility.h"
#include "d_player.h"
#include "p_effect.h"
#include "sbar.h"
#include "po_man.h"
#include "r_utility.h"
#include "a_hexenglobal.h"
#include "p_local.h"
#include "gl/gl_functions.h"

#include "gl/system/gl_interface.h"
#include "gl/system/gl_framebuffer.h"
#include "gl/system/gl_cvars.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/data/gl_data.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/dynlights/gl_dynlight.h"
#include "gl/dynlights/gl_lightbuffer.h"
#include "gl/models/gl_models.h"
#include "gl/scene/gl_clipper.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/scene/gl_portal.h"
#include "gl/shaders/gl_shader.h"
#include "gl/textures/gl_material.h"
#include "gl/utility/gl_clock.h"
#include "gl/utility/gl_convert.h"
#include "gl/utility/gl_templates.h"

//==========================================================================
//
// CVARs
//
//==========================================================================
CVAR(Bool, gl_texture, true, 0)
CVAR(Bool, gl_no_skyclear, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
// [BB] Don't allow this in release builds.
CVAR(Float, gl_mask_threshold, 0.5f,CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_DEBUGONLY)
CVAR(Float, gl_mask_sprite_threshold, 0.5f,CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR(Bool, gl_forcemultipass, false, 0)

EXTERN_CVAR (Int, screenblocks)
EXTERN_CVAR (Bool, cl_capfps)
EXTERN_CVAR (Bool, r_deathcamera)

// [BB]
CVAR( Bool, cl_disallowfullpitch, false, CVAR_ARCHIVE )
EXTERN_CVAR (Bool, cl_oldfreelooklimit)

extern int viewpitch;
 
DWORD			gl_fixedcolormap;
area_t			in_area;
TArray<BYTE> currentmapsection;

void gl_ParseDefs();

//-----------------------------------------------------------------------------
//
// R_FrustumAngle
//
//-----------------------------------------------------------------------------
angle_t FGLRenderer::FrustumAngle()
{
	float tilt= fabs(mAngles.Pitch);

	// If the pitch is larger than this you can look all around at a FOV of 90�
	if (tilt>46.0f) return 0xffffffff;

	// ok, this is a gross hack that barely works...
	// but at least it doesn't overestimate too much...
	double floatangle=2.0+(45.0+((tilt/1.9)))*mCurrentFoV*48.0/BaseRatioSizes[WidescreenRatio][3]/90.0;
	angle_t a1 = FLOAT_TO_ANGLE(floatangle);
	if (a1>=ANGLE_180) return 0xffffffff;
	return a1;
}

//-----------------------------------------------------------------------------
//
// Sets the area the camera is in
//
//-----------------------------------------------------------------------------
EXTERN_CVAR(Bool, cl_spectsource)
void FGLRenderer::SetViewArea(AActor *camera)
{
	// The render_sector is better suited to represent the current position in GL
	viewsector = R_PointInSubsector(viewx, viewy)->render_sector;

	if ( !cl_spectsource || camera == NULL || camera->player == NULL || !camera->player->bSpectating )
	{
		// keep the view within the render sector's floor and ceiling
		fixed_t theZ = viewsector->ceilingplane.ZatPoint (viewx, viewy) - 4*FRACUNIT;
		if (viewz > theZ)
		{
			viewz = theZ;
		}

		theZ = viewsector->floorplane.ZatPoint (viewx, viewy) + 4*FRACUNIT;
		if (viewz < theZ)
		{
			viewz = theZ;
		}
	}

	// Get the heightsec state from the render sector, not the current one!
	if (viewsector->heightsec && !(viewsector->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC))
	{
		in_area = viewz<=viewsector->heightsec->floorplane.ZatPoint(viewx,viewy) ? area_below :
				   (viewz>viewsector->heightsec->ceilingplane.ZatPoint(viewx,viewy) &&
				   !(viewsector->heightsec->MoreFlags&SECF_FAKEFLOORONLY)) ? area_above:area_normal;
	}
	else
	{
		in_area=area_default;	// depends on exposed lower sectors
	}
}

//-----------------------------------------------------------------------------
//
// resets the 3D viewport
//
//-----------------------------------------------------------------------------

void FGLRenderer::ResetViewport()
{
	int trueheight = static_cast<OpenGLFrameBuffer*>(screen)->GetTrueHeight();	// ugh...
	glViewport(0, (trueheight-screen->GetHeight())/2, screen->GetWidth(), screen->GetHeight()); 
}

//-----------------------------------------------------------------------------
//
// sets 3D viewport and initial state
//
//-----------------------------------------------------------------------------

void FGLRenderer::SetViewport(GL_IRECT *bounds)
{
	if (!bounds)
	{
		int height, width;

		// Special handling so the view with a visible status bar displays properly

		if (screenblocks >= 10)
		{
			height = SCREENHEIGHT;
			width  = SCREENWIDTH;
		}
		else
		{
			height = (screenblocks*SCREENHEIGHT/10) & ~7;
			width = (screenblocks*SCREENWIDTH/10);
		}

		int trueheight = static_cast<OpenGLFrameBuffer*>(screen)->GetTrueHeight();	// ugh...
		int bars = (trueheight-screen->GetHeight())/2; 

		int vw = viewwidth;
		int vh = viewheight;
		glViewport(viewwindowx, trueheight-bars-(height+viewwindowy-((height-vh)/2)), vw, height);
		glScissor(viewwindowx, trueheight-bars-(vh+viewwindowy), vw, vh);
	}
	else
	{
		glViewport(bounds->left, bounds->top, bounds->width, bounds->height);
		glScissor(bounds->left, bounds->top, bounds->width, bounds->height);
	}
	glEnable(GL_SCISSOR_TEST);
	
	#ifdef _DEBUG
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f); 
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	#else
		glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	#endif

	glEnable(GL_MULTISAMPLE);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_STENCIL_TEST);
	glStencilFunc(GL_ALWAYS,0,~0);	// default stencil
	glStencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);
}

//-----------------------------------------------------------------------------
//
// Setup the camera position
//
//-----------------------------------------------------------------------------

void FGLRenderer::SetCameraPos(fixed_t viewx, fixed_t viewy, fixed_t viewz, angle_t viewangle)
{
	float fviewangle=(float)(viewangle>>ANGLETOFINESHIFT)*360.0f/FINEANGLES;

	mAngles.Yaw = 270.0f-fviewangle;
	mViewVector = FVector2(cos(DEG2RAD(fviewangle)), sin(DEG2RAD(fviewangle)));
	mCameraPos = FVector3(FIXED2FLOAT(viewx), FIXED2FLOAT(viewy), FIXED2FLOAT(viewz));

	R_SetViewAngle();
}
	

//-----------------------------------------------------------------------------
//
// SetProjection
// sets projection matrix
//
//-----------------------------------------------------------------------------

void FGLRenderer::SetProjection(float fov, float ratio, float fovratio, float eyeShift) // [BB] Added eyeShift from GZ3Doom.
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	float fovy = 2 * RAD2DEG(atan(tan(DEG2RAD(fov) / 2) / fovratio));
	// [BB] Added eyeShift from GZ3Doom.
	if ( eyeShift == 0 )
		gluPerspective(fovy, ratio, 5.f, 65536.f);
	else
	{
		const float zNear = 5.0f;
		const float zFar = 65536.0f;
		const float pi = 3.1415926535897932384626433832795;
		double fH = tan( fovy / 360 * pi ) * zNear;
		double fW = fH * ratio;

		float screenZ = 25.0;
		float frustumShift = eyeShift * zNear / screenZ;

		glFrustum( -fW - frustumShift, fW - frustumShift, 
			-fH, fH, 
			zNear, zFar);
		glTranslatef(-eyeShift, 0, 0);
	}

	gl_RenderState.Set2DMode(false);
}

//-----------------------------------------------------------------------------
//
// Setup the modelview matrix
//
//-----------------------------------------------------------------------------

void FGLRenderer::SetViewMatrix(bool mirror, bool planemirror)
{
	if (gl.shadermodel >= 4)
	{
		glActiveTexture(GL_TEXTURE7);
		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();
	}
	glActiveTexture(GL_TEXTURE0);
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	float mult = mirror? -1:1;
	float planemult = planemirror? -1:1;

	glRotatef(GLRenderer->mAngles.Roll,  0.0f, 0.0f, 1.0f);
	glRotatef(GLRenderer->mAngles.Pitch, 1.0f, 0.0f, 0.0f);
	glRotatef(GLRenderer->mAngles.Yaw,   0.0f, mult, 0.0f);
	glTranslatef( GLRenderer->mCameraPos.X * mult, -GLRenderer->mCameraPos.Z*planemult, -GLRenderer->mCameraPos.Y);
	glScalef(-mult, planemult, 1);
}


//-----------------------------------------------------------------------------
//
// SetupView
// Setup the view rotation matrix for the given viewpoint
//
//-----------------------------------------------------------------------------
void FGLRenderer::SetupView(fixed_t viewx, fixed_t viewy, fixed_t viewz, angle_t viewangle, bool mirror, bool planemirror)
{
	SetCameraPos(viewx, viewy, viewz, viewangle);
	SetViewMatrix(mirror, planemirror);
}

//-----------------------------------------------------------------------------
//
// CreateScene
//
// creates the draw lists for the current scene
//
//-----------------------------------------------------------------------------

void FGLRenderer::CreateScene()
{
	// reset the portal manager
	GLPortal::StartFrame();
	PO_LinkToSubsectors();

	ProcessAll.Clock();

	// clip the scene and fill the drawlists
	for(unsigned i=0;i<portals.Size(); i++) portals[i]->glportal = NULL;
	gl_spriteindex=0;
	Bsp.Clock();
	gl_RenderBSPNode (nodes + numnodes - 1);
	Bsp.Unclock();

	// And now the crappy hacks that have to be done to avoid rendering anomalies:

	gl_drawinfo->HandleMissingTextures();	// Missing upper/lower textures
	gl_drawinfo->HandleHackedSubsectors();	// open sector hacks for deep water
	gl_drawinfo->ProcessSectorStacks();		// merge visplanes of sector stacks

	GLRenderer->mVBO->UnmapVBO ();
	ProcessAll.Unclock();

}

//-----------------------------------------------------------------------------
//
// RenderScene
//
// Draws the current draw lists for the non GLSL renderer
//
//-----------------------------------------------------------------------------

void FGLRenderer::RenderScene(int recursion)
{
	RenderAll.Clock();

	glDepthMask(true);
	if (!gl_no_skyclear) GLPortal::RenderFirstSkyPortal(recursion);

	gl_RenderState.SetCameraPos(FIXED2FLOAT(viewx), FIXED2FLOAT(viewy), FIXED2FLOAT(viewz));

	gl_RenderState.EnableFog(true);
	gl_RenderState.BlendFunc(GL_ONE,GL_ZERO);

	// First draw all single-pass stuff

	// Part 1: solid geometry. This is set up so that there are no transparent parts
	glDepthFunc(GL_LESS);


	gl_RenderState.EnableAlphaTest(false);

	glDisable(GL_POLYGON_OFFSET_FILL);	// just in case

	int pass;

	if (mLightCount > 0 && gl_fixedcolormap == CM_DEFAULT && gl_lights && gl_dynlight_shader)
	{
		pass = GLPASS_ALL;
	}
	else if (gl_texture)
	{
		pass = GLPASS_PLAIN;
	}
	else
	{
		pass = GLPASS_BASE;
	}

	gl_RenderState.EnableTexture(gl_texture);
	gl_RenderState.EnableBrightmap(gl_fixedcolormap == CM_DEFAULT);
	gl_drawinfo->drawlists[GLDL_PLAIN].Sort();
	gl_drawinfo->drawlists[GLDL_PLAIN].Draw(pass);
	gl_RenderState.EnableBrightmap(false);
	gl_drawinfo->drawlists[GLDL_FOG].Sort();
	gl_drawinfo->drawlists[GLDL_FOG].Draw(pass);
	gl_drawinfo->drawlists[GLDL_LIGHTFOG].Sort();
	gl_drawinfo->drawlists[GLDL_LIGHTFOG].Draw(pass);


	gl_RenderState.EnableAlphaTest(true);

	// Part 2: masked geometry. This is set up so that only pixels with alpha>0.5 will show
	if (!gl_texture) 
	{
		gl_RenderState.EnableTexture(true);
		gl_RenderState.SetTextureMode(TM_MASK);
	}
	if (pass == GLPASS_BASE) pass = GLPASS_BASE_MASKED;
	gl_RenderState.AlphaFunc(GL_GEQUAL,gl_mask_threshold);
	gl_RenderState.EnableBrightmap(true);
	gl_drawinfo->drawlists[GLDL_MASKED].Sort();
	gl_drawinfo->drawlists[GLDL_MASKED].Draw(pass);
	gl_RenderState.EnableBrightmap(false);
	gl_drawinfo->drawlists[GLDL_FOGMASKED].Sort();
	gl_drawinfo->drawlists[GLDL_FOGMASKED].Draw(pass);
	gl_drawinfo->drawlists[GLDL_LIGHTFOGMASKED].Sort();
	gl_drawinfo->drawlists[GLDL_LIGHTFOGMASKED].Draw(pass);

	// And now the multipass stuff
	if (!gl_dynlight_shader && gl_lights)
	{
		// First pass: empty background with sector light only

		// Part 1: solid geometry. This is set up so that there are no transparent parts

		// remove any remaining texture bindings and shaders whick may get in the way.
		gl_RenderState.EnableTexture(false);
		gl_RenderState.EnableBrightmap(false);
		gl_RenderState.Apply();
		gl_drawinfo->drawlists[GLDL_LIGHT].Draw(GLPASS_BASE);
		gl_RenderState.EnableTexture(true);

		// Part 2: masked geometry. This is set up so that only pixels with alpha>0.5 will show
		// This creates a blank surface that only fills the nontransparent parts of the texture
		gl_RenderState.SetTextureMode(TM_MASK);
		gl_RenderState.EnableBrightmap(true);
		gl_drawinfo->drawlists[GLDL_LIGHTBRIGHT].Draw(GLPASS_BASE_MASKED);
		gl_drawinfo->drawlists[GLDL_LIGHTMASKED].Draw(GLPASS_BASE_MASKED);
		gl_RenderState.EnableBrightmap(false);
		gl_RenderState.SetTextureMode(TM_MODULATE);


		// second pass: draw lights (on fogged surfaces they are added to the textures!)
		glDepthMask(false);
		if (mLightCount && !gl_fixedcolormap)
		{
			if (gl_SetupLightTexture())
			{
				gl_RenderState.BlendFunc(GL_ONE, GL_ONE);
				glDepthFunc(GL_EQUAL);
				if (glset.lightmode == 8) glVertexAttrib1f(VATTR_LIGHTLEVEL, 1.0f); // Korshun.
				for(int i=GLDL_FIRSTLIGHT; i<=GLDL_LASTLIGHT; i++)
				{
					gl_drawinfo->drawlists[i].Draw(GLPASS_LIGHT);
				}
				gl_RenderState.BlendEquation(GL_FUNC_ADD);
			}
			else gl_lights=false;
		}

		// third pass: modulated texture
		glColor3f(1.0f, 1.0f, 1.0f);
		gl_RenderState.BlendFunc(GL_DST_COLOR, GL_ZERO);
		gl_RenderState.EnableFog(false);
		glDepthFunc(GL_LEQUAL);
		if (gl_texture) 
		{
			gl_RenderState.EnableAlphaTest(false);
			gl_drawinfo->drawlists[GLDL_LIGHT].Sort();
			gl_drawinfo->drawlists[GLDL_LIGHT].Draw(GLPASS_TEXTURE);
			gl_RenderState.EnableAlphaTest(true);
			gl_drawinfo->drawlists[GLDL_LIGHTBRIGHT].Sort();
			gl_drawinfo->drawlists[GLDL_LIGHTBRIGHT].Draw(GLPASS_TEXTURE);
			gl_drawinfo->drawlists[GLDL_LIGHTMASKED].Sort();
			gl_drawinfo->drawlists[GLDL_LIGHTMASKED].Draw(GLPASS_TEXTURE);
		}

		// fourth pass: additive lights
		gl_RenderState.EnableFog(true);
		if (gl_lights && mLightCount && !gl_fixedcolormap)
		{
			gl_RenderState.BlendFunc(GL_ONE, GL_ONE);
			glDepthFunc(GL_EQUAL);
			if (gl_SetupLightTexture())
			{
				for(int i=0; i<GLDL_TRANSLUCENT; i++)
				{
					gl_drawinfo->drawlists[i].Draw(GLPASS_LIGHT_ADDITIVE);
				}
				gl_RenderState.BlendEquation(GL_FUNC_ADD);
			}
			else gl_lights=false;
		}
	}

	gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Draw decals (not a real pass)
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(-1.0f, -128.0f);
	glDepthMask(false);

	for(int i=0; i<GLDL_TRANSLUCENT; i++)
	{
		gl_drawinfo->drawlists[i].Draw(GLPASS_DECALS);
	}

	gl_RenderState.SetTextureMode(TM_MODULATE);

	glDepthMask(true);


	// Push bleeding floor/ceiling textures back a little in the z-buffer
	// so they don't interfere with overlapping mid textures.
	glPolygonOffset(1.0f, 128.0f);

	// flood all the gaps with the back sector's flat texture
	// This will always be drawn like GLDL_PLAIN or GLDL_FOG, depending on the fog settings

	// [BB] We may only do this when drawing the final eye.
	GLint drawBuffer;
	glGetIntegerv ( GL_DRAW_BUFFER, &drawBuffer );
	if ( drawBuffer != GL_BACK_LEFT )
	{
		glDepthMask(false);							// don't write to Z-buffer!
		gl_RenderState.EnableFog(true);
		gl_RenderState.EnableAlphaTest(false);
		gl_RenderState.BlendFunc(GL_ONE,GL_ZERO);
		gl_drawinfo->DrawUnhandledMissingTextures();
		gl_RenderState.EnableAlphaTest(true);
	}
	glDepthMask(true);

	glPolygonOffset(0.0f, 0.0f);
	glDisable(GL_POLYGON_OFFSET_FILL);

	RenderAll.Unclock();
}

//-----------------------------------------------------------------------------
//
// RenderTranslucent
//
// Draws the current draw lists for the non GLSL renderer
//
//-----------------------------------------------------------------------------

void FGLRenderer::RenderTranslucent()
{
	RenderAll.Clock();

	glDepthMask(false);
	gl_RenderState.SetCameraPos(FIXED2FLOAT(viewx), FIXED2FLOAT(viewy), FIXED2FLOAT(viewz));

	// final pass: translucent stuff
	gl_RenderState.EnableAlphaTest(true);
	gl_RenderState.AlphaFunc(GL_GEQUAL,gl_mask_sprite_threshold);
	gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	gl_RenderState.EnableBrightmap(true);
	gl_drawinfo->drawlists[GLDL_TRANSLUCENTBORDER].Draw(GLPASS_TRANSLUCENT);
	gl_drawinfo->drawlists[GLDL_TRANSLUCENT].DrawSorted();
	gl_RenderState.EnableBrightmap(false);

	glDepthMask(true);

	gl_RenderState.AlphaFunc(GL_GEQUAL,0.5f);
	RenderAll.Unclock();
}


//-----------------------------------------------------------------------------
//
// gl_drawscene - this function renders the scene from the current
// viewpoint, including mirrors and skyboxes and other portals
// It is assumed that the GLPortal::EndFrame returns with the 
// stencil, z-buffer and the projection matrix intact!
//
//-----------------------------------------------------------------------------
EXTERN_CVAR(Bool, gl_draw_sync)

void FGLRenderer::DrawScene(bool toscreen)
{
	static int recursion=0;

	CreateScene();
	GLRenderer->mCurrentPortal = NULL;	// this must be reset before any portal recursion takes place.

	// Up to this point in the main draw call no rendering is performed so we can wait
	// with swapping the render buffer until now.
	if (!gl_draw_sync && toscreen)
	{
		All.Unclock();
		static_cast<OpenGLFrameBuffer*>(screen)->Swap();
		All.Clock();
	}
	RenderScene(recursion);

	// Handle all portals after rendering the opaque objects but before
	// doing all translucent stuff
	recursion++;
	GLPortal::EndFrame();
	recursion--;
	RenderTranslucent();
}


//==========================================================================
//
// Draws a blend over the entire view
//
//==========================================================================
void FGLRenderer::DrawBlend(sector_t * viewsector)
{
	float blend[4]={0,0,0,0};
	PalEntry blendv=0;
	float extra_red;
	float extra_green;
	float extra_blue;
	player_t *player = NULL;

	if (players[consoleplayer].camera != NULL)
	{
		player=players[consoleplayer].camera->player;
	}

	// don't draw sector based blends when an invulnerability colormap is active
	if (!gl_fixedcolormap)
	{
		if (!viewsector->e->XFloor.ffloors.Size())
		{
			if (viewsector->heightsec && !(viewsector->MoreFlags&SECF_IGNOREHEIGHTSEC))
			{
				switch(in_area)
				{
				default:
				case area_normal: blendv=viewsector->heightsec->midmap; break;
				case area_above: blendv=viewsector->heightsec->topmap; break;
				case area_below: blendv=viewsector->heightsec->bottommap; break;
				}
			}
		}
		else
		{
			TArray<lightlist_t> & lightlist = viewsector->e->XFloor.lightlist;

			for(unsigned int i=0;i<lightlist.Size();i++)
			{
				fixed_t lightbottom;
				if (i<lightlist.Size()-1) 
					lightbottom=lightlist[i+1].plane.ZatPoint(viewx,viewy);
				else 
					lightbottom=viewsector->floorplane.ZatPoint(viewx,viewy);

				if (lightbottom<viewz && (!lightlist[i].caster || !(lightlist[i].caster->flags&FF_FADEWALLS)))
				{
					// 3d floor 'fog' is rendered as a blending value
					blendv=lightlist[i].blend;
					// If this is the same as the sector's it doesn't apply!
					if (blendv == viewsector->ColorMap->Fade) blendv=0;
					// a little hack to make this work for Legacy maps.
					if (blendv.a==0 && blendv!=0) blendv.a=128;
					break;
				}
			}
		}
	}

	if (blendv.a==0)
	{
		blendv = R_BlendForColormap(blendv);
		if (blendv.a==255)
		{
			// The calculated average is too dark so brighten it according to the palettes's overall brightness
			int maxcol = MAX<int>(MAX<int>(framebuffer->palette_brightness, blendv.r), MAX<int>(blendv.g, blendv.b));
			blendv.r = blendv.r * 255 / maxcol;
			blendv.g = blendv.g * 255 / maxcol;
			blendv.b = blendv.b * 255 / maxcol;
		}
	}

	if (blendv.a==255)
	{

		extra_red = blendv.r / 255.0f;
		extra_green = blendv.g / 255.0f;
		extra_blue = blendv.b / 255.0f;

		// If this is a multiplicative blend do it separately and add the additive ones on top of it!
		blendv=0;

		// black multiplicative blends are ignored
		if (extra_red || extra_green || extra_blue)
		{
			gl_RenderState.EnableAlphaTest(false);
			gl_RenderState.EnableTexture(false);
			gl_RenderState.BlendFunc(GL_DST_COLOR,GL_ZERO);
			glColor4f(extra_red, extra_green, extra_blue, 1.0f);
			gl_RenderState.Apply(true);
			glBegin(GL_TRIANGLE_STRIP);
			glVertex2f( 0.0f, 0.0f);
			glVertex2f( 0.0f, (float)SCREENHEIGHT);
			glVertex2f( (float)SCREENWIDTH, 0.0f);
			glVertex2f( (float)SCREENWIDTH, (float)SCREENHEIGHT);
			glEnd();
		}
	}
	else if (blendv.a)
	{
		V_AddBlend (blendv.r / 255.f, blendv.g / 255.f, blendv.b / 255.f, blendv.a/255.0f,blend);
	}

	// This mostly duplicates the code in shared_sbar.cpp
	// When I was writing this the original was called too late so that I
	// couldn't get the blend in time. However, since then I made some changes
	// here that would get lost if I switched back so I won't do it.

	if (player)
	{
		V_AddPlayerBlend(player, blend, 0.5, 175);
	}
	
	if (players[consoleplayer].camera != NULL)
	{
		// except for fadeto effects
		player_t *player = (players[consoleplayer].camera->player != NULL) ? players[consoleplayer].camera->player : &players[consoleplayer];
		V_AddBlend (player->BlendR, player->BlendG, player->BlendB, player->BlendA, blend);
	}

	if (blend[3]>0.0f)
	{
		gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		gl_RenderState.EnableAlphaTest(false);
		gl_RenderState.EnableTexture(false);
		glColor4fv(blend);
		gl_RenderState.Apply(true);
		glBegin(GL_TRIANGLE_STRIP);
		glVertex2f( 0.0f, 0.0f);
		glVertex2f( 0.0f, (float)SCREENHEIGHT);
		glVertex2f( (float)SCREENWIDTH, 0.0f);
		glVertex2f( (float)SCREENWIDTH, (float)SCREENHEIGHT);
		glEnd();
	}
}


//-----------------------------------------------------------------------------
//
// Draws player sprites and color blend
//
//-----------------------------------------------------------------------------


void FGLRenderer::EndDrawScene(sector_t * viewsector)
{
	// [BB] HUD models need to be rendered here. Make sure that
	// DrawPlayerSprites is only called once. Either to draw
	// HUD models or to draw the weapon sprites.
	const bool renderHUDModel = gl_IsHUDModelForPlayerAvailable( players[consoleplayer].camera->player );
	if ( renderHUDModel )
	{
		// [BB] The HUD model should be drawn over everything else already drawn.
		glClear(GL_DEPTH_BUFFER_BIT);
		DrawPlayerSprites (viewsector, true);
	}

	glDisable(GL_STENCIL_TEST);
	glDisable(GL_POLYGON_SMOOTH);

	gl_RenderState.EnableFog(false);
	framebuffer->Begin2D(false);

	ResetViewport();
	// [BB] Only draw the sprites if we didn't render a HUD model before.
	if ( renderHUDModel == false )
	{
		DrawPlayerSprites (viewsector, false);
	}
	DrawTargeterSprites();
	DrawBlend(viewsector);

	// Restore standard rendering state
	gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor3f(1.0f,1.0f,1.0f);
	gl_RenderState.EnableTexture(true);
	gl_RenderState.EnableAlphaTest(true);
	glDisable(GL_SCISSOR_TEST);
}


//-----------------------------------------------------------------------------
//
// R_RenderView - renders one view - either the screen or a camera texture
//
//-----------------------------------------------------------------------------

void FGLRenderer::ProcessScene(bool toscreen)
{
	FDrawInfo::StartDrawInfo();
	iter_dlightf = iter_dlight = draw_dlight = draw_dlightf = 0;
	GLPortal::BeginScene();

	int mapsection = R_PointInSubsector(viewx, viewy)->mapsection;
	memset(&currentmapsection[0], 0, currentmapsection.Size());
	currentmapsection[mapsection>>3] |= 1 << (mapsection & 7);
	DrawScene(toscreen);
	FDrawInfo::EndDrawInfo();

}

//-----------------------------------------------------------------------------
//
// gl_SetFixedColormap
//
//-----------------------------------------------------------------------------

void FGLRenderer::SetFixedColormap (player_t *player)
{
	gl_fixedcolormap=CM_DEFAULT;

	// check for special colormaps
	player_t * cplayer = player->camera->player;
	if (cplayer) 
	{
		if (cplayer->extralight == INT_MIN)
		{
			gl_fixedcolormap=CM_FIRSTSPECIALCOLORMAP + INVERSECOLORMAP;
			extralight=0;
		}
		else if (cplayer->fixedcolormap != NOFIXEDCOLORMAP)
		{
			gl_fixedcolormap = CM_FIRSTSPECIALCOLORMAP + cplayer->fixedcolormap;
		}
		else if (cplayer->fixedlightlevel != -1)
		{
			for(AInventory * in = cplayer->mo->Inventory; in; in = in->Inventory)
			{
				PalEntry color = in->GetBlend ();

				// Need special handling for light amplifiers 
				if (in->IsKindOf(RUNTIME_CLASS(APowerTorch)))
				{
					gl_fixedcolormap = cplayer->fixedlightlevel + CM_TORCH;
				}
				else if (in->IsKindOf(RUNTIME_CLASS(APowerLightAmp)))
				{
					gl_fixedcolormap = CM_LITE;
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
//
// Renders one viewpoint in a scene
//
//-----------------------------------------------------------------------------

sector_t * FGLRenderer::RenderViewpoint (AActor * camera, GL_IRECT * bounds, float fov, float ratio, float fovratio, bool mainview, bool toscreen)
{       
	// [BB] Check if stereo rendering is supported.
	GLboolean supportsStereo = false;
	GLboolean supportsBuffered = false;
	glGetBooleanv(GL_STEREO, &supportsStereo);
	glGetBooleanv(GL_DOUBLEBUFFER, &supportsBuffered);
	const bool renderStereo = (supportsStereo && supportsBuffered && toscreen);

	sector_t * retval;
	R_SetupFrame (camera);
	SetViewArea (camera);
	mAngles.Pitch = clamp<float>((float)((double)(int)(viewpitch))/ANGLE_1, -90, 90);

	// Scroll the sky
	mSky1Pos = (float)fmod(gl_frameMS * level.skyspeed1, 1024.f) * 90.f/256.f;
	mSky2Pos = (float)fmod(gl_frameMS * level.skyspeed2, 1024.f) * 90.f/256.f;



	// [BB] consoleplayer should be able to toggle the chase cam.
	if (camera->player && /*camera->player-players==consoleplayer &&*/
		((/*camera->player->*/players[consoleplayer].cheats & CF_CHASECAM) || (r_deathcamera && camera->health <= 0)) && camera==camera->player->mo)
	{
		mViewActor=NULL;
	}
	else
	{
		mViewActor=camera;
	}

	retval = viewsector;

	SetViewport(bounds);
	mCurrentFoV = fov;
	// [BB] Added stereo rendering based on one of the initial GZ3Doom revisions.
	if ( renderStereo == false )
	{
		SetProjection(fov, ratio, fovratio);	// switch to perspective mode and set up clipper
		SetCameraPos(viewx, viewy, viewz, viewangle);
		SetViewMatrix(false, false);

		clipper.Clear();
		angle_t a1 = FrustumAngle();
		clipper.SafeAddClipRangeRealAngles(viewangle+a1, viewangle-a1);

		ProcessScene(toscreen);
	}
	else
	{
		SetCameraPos(viewx, viewy, viewz, viewangle);
		SetViewMatrix(false, false);
		angle_t a1 = FrustumAngle();

		// Stereo 
		// 1 doom unit = about 3 cm
		float iod = 2.0; // intraocular distance

		// Left eye
		glDrawBuffer(GL_BACK_LEFT);
		SetProjection(fov, ratio, fovratio, -iod/2);	// switch to perspective mode and set up clipper
		clipper.Clear();
		clipper.SafeAddClipRangeRealAngles(viewangle+a1, viewangle-a1);
		ProcessScene(toscreen);

		// Right eye
		SetViewport(bounds);
		glDrawBuffer(GL_BACK_RIGHT);
		SetProjection(fov, ratio, fovratio, +iod/2);	// switch to perspective mode and set up clipper
		clipper.Clear();
		clipper.SafeAddClipRangeRealAngles(viewangle+a1, viewangle-a1);
		ProcessScene(toscreen);

		glDrawBuffer(GL_BACK);
	}

	gl_frameCount++;	// This counter must be increased right before the interpolations are restored.
	interpolator.RestoreInterpolations ();
	return retval;
}


//-----------------------------------------------------------------------------
//
// renders the view
//
//-----------------------------------------------------------------------------

#ifdef _WIN32 // [BB] Detect some kinds of glBegin hooking.
extern char myGlBeginCharArray[4];
int crashoutTic = 0;
#endif

void FGLRenderer::RenderView (player_t* player)
{
#ifdef _WIN32 // [BB] Detect some kinds of glBegin hooking.
	// [BB] Continuously make this check, otherwise a hack could bypass the check by activating
	// and deactivating itself at the right time interval.
	{
		if ( strncmp(reinterpret_cast<char *>(glBegin), myGlBeginCharArray, 4) )
		{
			I_FatalError ( "OpenGL malfunction encountered.\n" );
		}
		else
		{
			// [BB] Most GL wallhacks deactivate GL_DEPTH_TEST by manipulating glBegin.
			// Here we try check if this is done.
			GLboolean oldValue;
			glGetBooleanv ( GL_DEPTH_TEST, &oldValue );
			glEnable ( GL_DEPTH_TEST );
			glBegin( GL_TRIANGLE_STRIP );
			glEnd();
			GLboolean valueTrue;
			glGetBooleanv ( GL_DEPTH_TEST, &valueTrue );

			// [BB] Also check if glGetBooleanv simply always returns true.
			glDisable ( GL_DEPTH_TEST );
			GLboolean valueFalse;
			glGetBooleanv ( GL_DEPTH_TEST, &valueFalse );

			if ( ( valueTrue == GL_FALSE ) || ( valueFalse == GL_TRUE ) )
			{
				I_FatalError ( "OpenGL malfunction encountered.\n" );
			}

			if ( oldValue )
				glEnable ( GL_DEPTH_TEST );
			else
				glDisable ( GL_DEPTH_TEST );
		}
	}
#endif

	OpenGLFrameBuffer* GLTarget = static_cast<OpenGLFrameBuffer*>(screen);
	AActor *&LastCamera = GLTarget->LastCamera;

	if (player->camera != LastCamera)
	{
		// If the camera changed don't interpolate
		// Otherwise there will be some not so nice effects.
		R_ResetViewInterpolation();
		LastCamera=player->camera;
	}

	mVBO->BindVBO();

	// reset statistics counters
	ResetProfilingData();

	// Get this before everything else
	if (cl_capfps || r_NoInterpolate) r_TicFrac = FRACUNIT;
	else r_TicFrac = I_GetTimeFrac (&r_FrameTime);
	gl_frameMS = I_MSTime();

	P_FindParticleSubsectors ();

	// prepare all camera textures that have been used in the last frame
	FCanvasTextureInfo::UpdateAll();


	// I stopped using BaseRatioSizes here because the information there wasn't well presented.
	#define RMUL (1.6f/1.333333f)
	//							4:3				16:9		16:10		17:10		5:4			17:10		21:9
	static float ratios[]={RMUL*1.333333f, RMUL*1.777777f, RMUL*1.6f, RMUL*1.7f, RMUL*1.25f, RMUL*1.7f, RMUL*2.333333f};

	// now render the main view
	float fovratio;
	float ratio = ratios[WidescreenRatio];
	if (!(WidescreenRatio&4))
	{
		fovratio = 1.6f;
	}
	else
	{
		fovratio = ratio;
	}

	SetFixedColormap (player);

	// Check if there's some lights. If not some code can be skipped.
	TThinkerIterator<ADynamicLight> it(STAT_DLIGHT);
	GLRenderer->mLightCount = ((it.Next()) != NULL);

	sector_t * viewsector = RenderViewpoint(player->camera, NULL, FieldOfView * 360.0f / FINEANGLES, ratio, fovratio, true, true);
	EndDrawScene(viewsector);

	All.Unclock();
}

//===========================================================================
//
// Render the view to a savegame picture
//
//===========================================================================

void FGLRenderer::WriteSavePic (player_t *player, FILE *file, int width, int height)
{
	GL_IRECT bounds;

	bounds.left=0;
	bounds.top=0;
	bounds.width=width;
	bounds.height=height;
	glFlush();
	SetFixedColormap(player);

	// Check if there's some lights. If not some code can be skipped.
	TThinkerIterator<ADynamicLight> it(STAT_DLIGHT);
	GLRenderer->mLightCount = ((it.Next()) != NULL);

	sector_t *viewsector = RenderViewpoint(players[consoleplayer].camera, &bounds, 
								FieldOfView * 360.0f / FINEANGLES, 1.6f, 1.6f, true, false);
	glDisable(GL_STENCIL_TEST);
	screen->Begin2D(false);
	DrawBlend(viewsector);
	glFlush();

	byte * scr = (byte *)M_Malloc(width * height * 3);
	glReadPixels(0,0,width, height,GL_RGB,GL_UNSIGNED_BYTE,scr);
	M_CreatePNG (file, scr + ((height-1) * width * 3), NULL, SS_RGB, width, height, -width*3);
	M_Free(scr);

	// [BC] In GZDoom, this is called every frame, regardless of whether or not
	// the view is active. In Skulltag, we don't so we have to call this here
	// to reset everything, such as the viewport, after rendering our view to
	// a canvas.
	FGLRenderer::EndDrawScene( viewsector );
}


//===========================================================================
//
//
//
//===========================================================================

struct FGLInterface : public FRenderer
{
	bool UsesColormap() const;
	void PrecacheTexture(FTexture *tex, int cache);
	void RenderView(player_t *player);
	void WriteSavePic (player_t *player, FILE *file, int width, int height);
	void StateChanged(AActor *actor);
	void StartSerialize(FArchive &arc);
	void EndSerialize(FArchive &arc);
	void RenderTextureView (FCanvasTexture *self, AActor *viewpoint, int fov);
	sector_t *FakeFlat(sector_t *sec, sector_t *tempsec, int *floorlightlevel, int *ceilinglightlevel, bool back);
	void SetFogParams(int _fogdensity, PalEntry _outsidefogcolor, int _outsidefogdensity, int _skyfog);
	void PreprocessLevel();
	void CleanLevelData();
	bool RequireGLNodes();

	int GetMaxViewPitch(bool down);
	void ClearBuffer(int color);
	void Init();
};

//===========================================================================
//
// The GL renderer has no use for colormaps so let's
// not create them and save us some time.
//
//===========================================================================

bool FGLInterface::UsesColormap() const
{
	return false;
}

//==========================================================================
//
// DFrameBuffer :: PrecacheTexture
//
//==========================================================================

void FGLInterface::PrecacheTexture(FTexture *tex, int cache)
{
	if (tex != NULL)
	{
		if (cache)
		{
			tex->PrecacheGL();
		}
		else
		{
			tex->UncacheGL();
		}
	}
}

//==========================================================================
//
// DFrameBuffer :: StateChanged
//
//==========================================================================

void FGLInterface::StateChanged(AActor *actor)
{
	gl_SetActorLights(actor);
}

//===========================================================================
//
// notify the renderer that serialization of the curent level is about to start/end
//
//===========================================================================

void FGLInterface::StartSerialize(FArchive &arc)
{
	gl_DeleteAllAttachedLights();
	arc << fogdensity << outsidefogdensity << skyfog;
}

void FGLInterface::EndSerialize(FArchive &arc)
{
	gl_RecreateAllAttachedLights();
	if (arc.IsLoading()) gl_InitPortals();
}

//===========================================================================
//
// Get max. view angle (renderer specific information so it goes here now)
//
//===========================================================================

EXTERN_CVAR(Float, maxviewpitch)

int FGLInterface::GetMaxViewPitch(bool down)
{
	// [BB] Zandronum clamps the pitch differently
	if (cl_disallowfullpitch) return down? 56 : ( cl_oldfreelooklimit ? 32 : 56 );
	else return maxviewpitch;
}

//===========================================================================
//
// 
//
//===========================================================================

void FGLInterface::ClearBuffer(int color)
{
	PalEntry pe = GPalette.BaseColors[color];
	glClearColor(pe.r/255.f, pe.g/255.f, pe.b/255.f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT);
}

//===========================================================================
//
// Render the view to a savegame picture
//
//===========================================================================

void FGLInterface::WriteSavePic (player_t *player, FILE *file, int width, int height)
{
	GLRenderer->WriteSavePic(player, file, width, height);
}

//===========================================================================
//
// 
//
//===========================================================================

void FGLInterface::RenderView(player_t *player)
{
	GLRenderer->RenderView(player);
}

//===========================================================================
//
// 
//
//===========================================================================

void FGLInterface::Init()
{
	gl_ParseDefs();
	gl_InitData();
}

//===========================================================================
//
// Camera texture rendering
//
//===========================================================================
CVAR(Bool, gl_usefb, false , CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
extern TexFilter_s TexFilter[];

void FGLInterface::RenderTextureView (FCanvasTexture *tex, AActor *Viewpoint, int FOV)
{
	FMaterial * gltex = FMaterial::ValidateTexture(tex);

	int width = gltex->TextureWidth(GLUSE_TEXTURE);
	int height = gltex->TextureHeight(GLUSE_TEXTURE);

	gl_fixedcolormap=CM_DEFAULT;

	bool usefb;

	if (gl.flags & RFL_FRAMEBUFFER)
	{
		usefb = gl_usefb || width > screen->GetWidth() || height > screen->GetHeight();
	}
	else usefb = false;


	if (!usefb)
	{
		glFlush();
	}
	else
	{
#if defined(_WIN32) && (defined(_MSC_VER) || defined(__INTEL_COMPILER))
		__try
#endif
		{
			GLRenderer->StartOffscreen();
			gltex->BindToFrameBuffer();
		}
#if defined(_WIN32) && (defined(_MSC_VER) || defined(__INTEL_COMPILER))
		__except(1)
		{
			usefb = false;
			gl_usefb = false;
			GLRenderer->EndOffscreen();
			glFlush();
		}
#endif
	}

	GL_IRECT bounds;
	bounds.left=bounds.top=0;
	bounds.width=FHardwareTexture::GetTexDimension(gltex->GetWidth(GLUSE_TEXTURE));
	bounds.height=FHardwareTexture::GetTexDimension(gltex->GetHeight(GLUSE_TEXTURE));

	GLRenderer->RenderViewpoint(Viewpoint, &bounds, FOV, (float)width/height, (float)width/height, false, false);

	if (!usefb)
	{
		glFlush();
		gltex->Bind(CM_DEFAULT, 0, 0);
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, bounds.width, bounds.height);
	}
	else
	{
		GLRenderer->EndOffscreen();
	}

	gltex->Bind(CM_DEFAULT, 0, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, TexFilter[gl_texture_filter].magfilter);
	tex->SetUpdated();
}

//==========================================================================
//
//
//
//==========================================================================

sector_t *FGLInterface::FakeFlat(sector_t *sec, sector_t *tempsec, int *floorlightlevel, int *ceilinglightlevel, bool back)
{
	if (floorlightlevel != NULL)
	{
		*floorlightlevel = sec->GetFloorLight ();
	}
	if (ceilinglightlevel != NULL)
	{
		*ceilinglightlevel = sec->GetCeilingLight ();
	}
	return gl_FakeFlat(sec, tempsec, back);
}

//===========================================================================
//
// 
//
//===========================================================================

void FGLInterface::SetFogParams(int _fogdensity, PalEntry _outsidefogcolor, int _outsidefogdensity, int _skyfog)
{
	gl_SetFogParams(_fogdensity, _outsidefogcolor, _outsidefogdensity, _skyfog);
}

void FGLInterface::PreprocessLevel() 
{
	gl_PreprocessLevel();
}

void FGLInterface::CleanLevelData() 
{
	gl_CleanLevelData();
}

bool FGLInterface::RequireGLNodes() 
{ 
	return true; 
}

//===========================================================================
//
// 
//
//===========================================================================

FRenderer *gl_CreateInterface()
{
	return new FGLInterface;
}


