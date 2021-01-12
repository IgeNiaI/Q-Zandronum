/*
** p_3dfloor.cpp
**
** 3D-floor handling
**
**---------------------------------------------------------------------------
** Copyright 2005-2008 Christoph Oelckers
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
** 4. Full disclosure of the entire project's source code, except for third
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

#include "templates.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "w_wad.h"
#include "sc_man.h"
#include "g_level.h"
#include "r_data/colormaps.h"

#ifdef _3DFLOORS
EXTERN_CVAR(Int, vid_renderer)

//==========================================================================
//
//  3D Floors
//
//==========================================================================

//==========================================================================
//
// Wrappers to access the colormap information
//
//==========================================================================

FDynamicColormap *F3DFloor::GetColormap()
{
	// If there's no fog in either model or target sector this is easy and fast.
	if ((target->ColorMap->Fade == 0 && model->ColorMap->Fade == 0) || (flags & (FF_FADEWALLS|FF_FOG)))
	{
		return model->ColorMap;
	}
	else
	{
		// We must create a new colormap combining the properties we need
		return GetSpecialLights(model->ColorMap->Color, target->ColorMap->Fade, model->ColorMap->Desaturate);
	}
}

PalEntry F3DFloor::GetBlend()
{
	// The model sector's fog is used as blend unless FF_FADEWALLS is set.
	if (!(flags & FF_FADEWALLS) && target->ColorMap->Fade != model->ColorMap->Fade)
	{
		return model->ColorMap->Fade;
	}
	else
	{
		return 0;
	}
}

void F3DFloor::UpdateColormap(FDynamicColormap *&map)
{
	// If there's no fog in either model or target sector (or both have the same fog) this is easy and fast.
	if ((target->ColorMap->Fade == 0 && model->ColorMap->Fade == 0) || (flags & FF_FADEWALLS) ||
		target->ColorMap->Fade == model->ColorMap->Fade)
	{
		map = model->ColorMap;
	}
	else
	{
		// since rebuilding the map is not a cheap operation let's only do it if something really changed.
		if (map->Color != model->ColorMap->Color || map->Fade != target->ColorMap->Fade ||
			map->Desaturate != model->ColorMap->Desaturate)
		{
			map = GetSpecialLights(model->ColorMap->Color, target->ColorMap->Fade, model->ColorMap->Desaturate);
		}
	}
}

//==========================================================================
//
// Add one 3D floor to the sector
//
//==========================================================================
static void P_Add3DFloor(sector_t* sec, sector_t* sec2, line_t* master, int flags, int alpha)
{
	F3DFloor*      ffloor;
	unsigned  i;

	if(!(flags & FF_THISINSIDE)) {
		for(i = 0; i < sec2->e->XFloor.attached.Size(); i++) if(sec2->e->XFloor.attached[i] == sec) return;
		sec2->e->XFloor.attached.Push(sec);
	}
	
	//Add the floor
	ffloor = new F3DFloor;
	ffloor->top.model = ffloor->bottom.model = ffloor->model = sec2;
	ffloor->target = sec;
	ffloor->ceilingclip = ffloor->floorclip = NULL;
	ffloor->validcount = 0;

	if (!(flags&FF_THINFLOOR))
	{
		ffloor->bottom.plane = &sec2->floorplane;
		ffloor->bottom.texture = &sec2->planes[sector_t::floor].Texture;
		ffloor->bottom.texheight = &sec2->planes[sector_t::floor].TexZ;
		ffloor->bottom.isceiling = sector_t::floor;
	}
	else 
	{
		ffloor->bottom.plane = &sec2->ceilingplane;
		ffloor->bottom.texture = &sec2->planes[sector_t::ceiling].Texture;
		ffloor->bottom.texheight = &sec2->planes[sector_t::ceiling].TexZ;
		ffloor->bottom.isceiling = sector_t::ceiling;
	}
	
	if (!(flags&FF_FIX))
	{
		ffloor->top.plane = &sec2->ceilingplane;
		ffloor->top.texture = &sec2->planes[sector_t::ceiling].Texture;
		ffloor->top.texheight = &sec2->planes[sector_t::ceiling].TexZ;
		ffloor->toplightlevel = &sec2->lightlevel;
		ffloor->top.isceiling = sector_t::ceiling;
	}
	else	// FF_FIX is a special case to patch rendering holes
	{
		ffloor->top.plane = &sec->floorplane;
		ffloor->top.texture = &sec2->planes[sector_t::floor].Texture;
		ffloor->top.texheight = &sec2->planes[sector_t::floor].TexZ;
		ffloor->toplightlevel = &sec->lightlevel;
		ffloor->top.isceiling = sector_t::floor;
		ffloor->top.model = sec;
	}

	// Hacks for Vavoom's idiotic implementation
	if (flags&FF_INVERTSECTOR)
	{
		// switch the planes
		F3DFloor::planeref sp = ffloor->top;

		ffloor->top=ffloor->bottom;
		ffloor->bottom=sp;

		if (flags&FF_SWIMMABLE)
		{
			// Vavoom floods the lower part if it is swimmable.
			// fortunately this plane won't be rendered - otherwise this wouldn't work...
			ffloor->bottom.plane=&sec->floorplane;
			ffloor->bottom.model=sec;
			ffloor->bottom.isceiling = sector_t::floor;
		}
	}

	ffloor->flags  = flags;
	ffloor->master = master;
	ffloor->alpha  = alpha;
	ffloor->top.vindex = ffloor->bottom.vindex = -1;

	// The engine cannot handle sloped translucent floors. Sorry
	if (ffloor->top.plane->a || ffloor->top.plane->b || ffloor->bottom.plane->a || ffloor->bottom.plane->b)
	{
		ffloor->alpha = FRACUNIT;
		ffloor->flags &= ~FF_ADDITIVETRANS;
	}

	if(flags & FF_THISINSIDE) {
		// switch the planes
		F3DFloor::planeref sp = ffloor->top;
		ffloor->top=ffloor->bottom;
		ffloor->bottom=sp;
	}

	sec->e->XFloor.ffloors.Push(ffloor);

	// kg3D - software renderer only hack
	// this is really required because of ceilingclip and floorclip
	if((vid_renderer == 0) && (flags & FF_BOTHPLANES))
	{
		P_Add3DFloor(sec, sec2, master, FF_EXISTS | FF_THISINSIDE | FF_RENDERPLANES | FF_NOSHADE | FF_SEETHROUGH | FF_SHOOTTHROUGH |
			(flags & (FF_INVERTSECTOR | FF_TRANSLUCENT | FF_ADDITIVETRANS)), alpha);
	}
}

//==========================================================================
//
// Creates all 3D floors defined by one linedef
//
//==========================================================================
static int P_Set3DFloor(line_t * line, int param, int param2, int alpha)
{
	int s,i;
	int flags;
	int tag=line->args[0];
    sector_t * sec = line->frontsector, * ss;

    for (s=-1; (s = P_FindSectorFromTag(tag,s)) >= 0;)
	{
		ss=&sectors[s];

		if (param==0)
		{
			flags=FF_EXISTS|FF_RENDERALL|FF_SOLID|FF_INVERTSECTOR;
			for (i=0;i<sec->linecount;i++)
			{
				line_t * l=sec->lines[i];

				alpha=255;
				if (l->special==Sector_SetContents && l->frontsector==sec)
				{
					alpha=clamp<int>(l->args[1], 0, 100);
					if (l->args[2] & 1) flags &= ~FF_SOLID;
					if (l->args[2] & 2) flags |= FF_SEETHROUGH;
					if (l->args[2] & 4) flags |= FF_SHOOTTHROUGH;
					if (l->args[2] & 8) flags |= FF_ADDITIVETRANS;
					if (alpha!=100) flags|=FF_TRANSLUCENT;//|FF_BOTHPLANES|FF_ALLSIDES;
					if (l->args[0]) 
					{
						// Yes, Vavoom's 3D-floor definitions suck!
						// The content list changed in r1783 of Vavoom to be unified
						// among all its supported games, so it has now ten different
						// values instead of just five.
						static DWORD vavoomcolors[]={VC_EMPTY, 
							VC_WATER, VC_LAVA, VC_NUKAGE, VC_SLIME, VC_HELLSLIME,
							VC_BLOOD, VC_SLUDGE, VC_HAZARD, VC_BOOMWATER};
						flags|=FF_SWIMMABLE|FF_BOTHPLANES|FF_ALLSIDES|FF_FLOOD;

						l->frontsector->ColorMap = 
							GetSpecialLights (l->frontsector->ColorMap->Color, 
											  (unsigned int)(vavoomcolors[l->args[0]]&VC_COLORMASK), 
											  (unsigned int)(vavoomcolors[l->args[0]]&VC_ALPHAMASK)>>24);
										//	  l->frontsector->ColorMap->Desaturate);
					}
					alpha=(alpha*255)/100;
					break;
				}
			}
		}
		else if (param==4)
		{
			flags=FF_EXISTS|FF_RENDERPLANES|FF_INVERTPLANES|FF_NOSHADE|FF_FIX;
			alpha=255;
		}
		else 
		{
			static const int defflags[]= {0, 
										  FF_SOLID, 
										  FF_SWIMMABLE|FF_BOTHPLANES|FF_ALLSIDES|FF_SHOOTTHROUGH|FF_SEETHROUGH, 
										  FF_SHOOTTHROUGH|FF_SEETHROUGH, 
			};

			flags = defflags[param&3] | FF_EXISTS|FF_RENDERALL;

			if (param&4) flags |= FF_ALLSIDES|FF_BOTHPLANES;
			if (param&16) flags ^= FF_SEETHROUGH;
			if (param&32) flags ^= FF_SHOOTTHROUGH;

			if (param2&1) flags |= FF_NOSHADE;
			if (param2&2) flags |= FF_DOUBLESHADOW;
			if (param2&4) flags |= FF_FOG;
			if (param2&8) flags |= FF_THINFLOOR;
			if (param2&16) flags |= FF_UPPERTEXTURE;
			if (param2&32) flags |= FF_LOWERTEXTURE;
			if (param2&64) flags |= FF_ADDITIVETRANS|FF_TRANSLUCENT;
			// if flooding is used the floor must be non-solid and is automatically made shootthrough and seethrough
			if ((param2&128) && !(flags & FF_SOLID)) flags |= FF_FLOOD|FF_SEETHROUGH|FF_SHOOTTHROUGH;
			if (param2&512) flags |= FF_FADEWALLS;
			FTextureID tex = line->sidedef[0]->GetTexture(side_t::top);
			if (!tex.Exists() && alpha<255)
			{
				alpha = -tex.GetIndex();
			}
			alpha = clamp(alpha, 0, 255);
			if (alpha==0) flags&=~(FF_RENDERALL|FF_BOTHPLANES|FF_ALLSIDES);
			else if (alpha!=255) flags|=FF_TRANSLUCENT;
										 
		}
		P_Add3DFloor(ss, sec, line, flags, alpha);
	}
	// To be 100% safe this should be done even if the alpha by texture value isn't used.
	if (!line->sidedef[0]->GetTexture(side_t::top).isValid()) 
		line->sidedef[0]->SetTexture(side_t::top, FNullTextureID());
	return 1;
}

//==========================================================================
//
// P_PlayerOnSpecial3DFloor
// Checks to see if a player is standing on or is inside a 3D floor (water)
// and applies any specials..
//
//==========================================================================

void P_PlayerOnSpecial3DFloor(player_t* player)
{
	sector_t * sector = player->mo->Sector;

	for(unsigned i=0;i<sector->e->XFloor.ffloors.Size();i++)
	{
		F3DFloor* rover=sector->e->XFloor.ffloors[i];

		if (!(rover->flags & FF_EXISTS)) continue;
		if (rover->flags & FF_FIX) continue;

		// Check the 3D floor's type...
		if(rover->flags & FF_SOLID)
		{
			// Player must be on top of the floor to be affected...
			if(player->mo->z != rover->top.plane->ZatPoint(player->mo->x, player->mo->y)) continue;
		}
		else
		{
			//Water and DEATH FOG!!! heh
			if (player->mo->z > rover->top.plane->ZatPoint(player->mo->x, player->mo->y) || 
				(player->mo->z + player->mo->height) < rover->bottom.plane->ZatPoint(player->mo->x, player->mo->y))
				continue;
		}
		
		if (rover->model->special || rover->model->damage) P_PlayerInSpecialSector(player, rover->model);
		break;
	}
}

//==========================================================================
//
// P_CheckFor3DFloorHit
// Checks whether the player's feet touch a solid 3D floor in the sector
//
//==========================================================================
bool P_CheckFor3DFloorHit(AActor * mo)
{
	sector_t * sector = mo->Sector;

	//if ((mo->player && (mo->player->cheats & CF_PREDICTING))) return false;

	for(unsigned i=0;i<sector->e->XFloor.ffloors.Size();i++)
	{
		F3DFloor* rover=sector->e->XFloor.ffloors[i];

		if (!(rover->flags & FF_EXISTS)) continue;

		if(rover->flags & FF_SOLID && rover->model->SecActTarget)
		{
			if(mo->floorz == rover->top.plane->ZatPoint(mo->x, mo->y)) 
			{
				rover->model->SecActTarget->TriggerAction (mo, SECSPAC_HitFloor);
				return true;
			}
		}
	}
	return false;
}

//==========================================================================
//
// P_CheckFor3DCeilingHit
// Checks whether the player's head touches a solid 3D floor in the sector
//
//==========================================================================
bool P_CheckFor3DCeilingHit(AActor * mo)
{
	sector_t * sector = mo->Sector;

	//if ((mo->player && (mo->player->cheats & CF_PREDICTING))) return false;

	for(unsigned i=0;i<sector->e->XFloor.ffloors.Size();i++)
	{
		F3DFloor* rover=sector->e->XFloor.ffloors[i];

		if (!(rover->flags & FF_EXISTS)) continue;

		if(rover->flags & FF_SOLID && rover->model->SecActTarget)
		{
			if(mo->ceilingz == rover->bottom.plane->ZatPoint(mo->x, mo->y)) 
			{
				rover->model->SecActTarget->TriggerAction (mo, SECSPAC_HitCeiling);
				return true;
			}
		}
	}
	return false;
}

//==========================================================================
//
// P_Recalculate3DFloors
//
// This function sorts the ffloors by height and creates the lightlists 
// that the given sector uses to light floors/ceilings/walls according to the 3D floors.
//
//==========================================================================

void P_Recalculate3DFloors(sector_t * sector)
{
	F3DFloor *		rover;
	F3DFloor *		pick;
	unsigned		pickindex;
	F3DFloor *		clipped=NULL;
	fixed_t			clipped_top;
	fixed_t			clipped_bottom=0;
	fixed_t			maxheight, minheight;
	unsigned		i, j;
	lightlist_t newlight;

	TArray<F3DFloor*> & ffloors=sector->e->XFloor.ffloors;
	TArray<lightlist_t> & lightlist = sector->e->XFloor.lightlist;

	// Sort the floors top to bottom for quicker access here and later
	// Translucent and swimmable floors are split if they overlap with solid ones.
	if (ffloors.Size()>1)
	{
		TArray<F3DFloor*> oldlist;
		
		oldlist = ffloors;
		ffloors.Clear();

		// first delete the old dynamic stuff
		for(i=0;i<oldlist.Size();i++)
		{
			F3DFloor * rover=oldlist[i];

			if (rover->flags&FF_DYNAMIC)
			{
				delete rover;
				oldlist.Delete(i);
				i--;
				continue;
			}
			if (rover->flags&FF_CLIPPED)
			{
				rover->flags&=~FF_CLIPPED;
				rover->flags|=FF_EXISTS;
			}
		}

		while (oldlist.Size())
		{
			pick=oldlist[0];
			fixed_t height=pick->top.plane->ZatPoint(CenterSpot(sector));

			// find highest starting ffloor - intersections are not supported!
			pickindex=0;
			for (j=1;j<oldlist.Size();j++)
			{
				fixed_t h2=oldlist[j]->top.plane->ZatPoint(CenterSpot(sector));

				if (h2>height)
				{
					pick=oldlist[j];
					pickindex=j;
					height=h2;
				}
			}

			oldlist.Delete(pickindex);

			if (pick->flags & FF_THISINSIDE)
			{
				// These have the floor higher than the ceiling and cannot be processed
				// by the clipping code below.
				ffloors.Push(pick);
			}
			else if (pick->flags&(FF_SWIMMABLE|FF_TRANSLUCENT) && pick->flags&FF_EXISTS)
			{
				clipped=pick;
				clipped_top=height;
				clipped_bottom=pick->bottom.plane->ZatPoint(CenterSpot(sector));
				ffloors.Push(pick);
			}
			else if (clipped && clipped_bottom<height)
			{
				// translucent floor above must be clipped to this one!
				F3DFloor * dyn=new F3DFloor;
				*dyn=*clipped;
				clipped->flags|=FF_CLIPPED;
				clipped->flags&=~FF_EXISTS;
				dyn->flags|=FF_DYNAMIC;
				dyn->bottom=pick->top;
				ffloors.Push(dyn);
				ffloors.Push(pick);

				fixed_t pick_bottom=pick->bottom.plane->ZatPoint(CenterSpot(sector));

				if (pick_bottom<=clipped_bottom)
				{
					clipped=NULL;
				}
				else
				{
					// the translucent part extends below the clipper
					dyn=new F3DFloor;
					*dyn=*clipped;
					dyn->flags|=FF_DYNAMIC|FF_EXISTS;
					dyn->top=pick->bottom;
					ffloors.Push(dyn);
				}
			}
			else
			{
				clipped=NULL;
				ffloors.Push(pick);
			}

		}
	}

	// having the floors sorted makes this routine significantly simpler
	// Only some overlapping cases with FF_DOUBLESHADOW might create anomalies
	// but these are clearly undefined.
	if(ffloors.Size())
	{
		lightlist.Resize(1);
		lightlist[0].plane = sector->ceilingplane;
		lightlist[0].p_lightlevel = &sector->lightlevel;
		lightlist[0].caster = NULL;
		lightlist[0].lightsource = NULL;
		lightlist[0].extra_colormap = sector->ColorMap;
		lightlist[0].blend = 0;
		lightlist[0].flags = 0;
		
		maxheight = sector->CenterCeiling();
		minheight = sector->CenterFloor();
		for(i = 0; i < ffloors.Size(); i++)
		{
			rover=ffloors[i];

			if ( !(rover->flags & FF_EXISTS) || rover->flags & FF_NOSHADE )
				continue;
				
			fixed_t ff_top=rover->top.plane->ZatPoint(CenterSpot(sector));
			if (ff_top < minheight) break;	// reached the floor
			if (ff_top < maxheight)
			{
				newlight.plane = *rover->top.plane;
				newlight.p_lightlevel = rover->toplightlevel;
				newlight.caster = rover;
				newlight.lightsource = rover;
				newlight.extra_colormap = rover->GetColormap();
				newlight.blend = rover->GetBlend();
				newlight.flags = rover->flags;
				lightlist.Push(newlight);
			}
			else if (i==0)
			{
				fixed_t ff_bottom=rover->bottom.plane->ZatPoint(CenterSpot(sector));
				if (ff_bottom<maxheight)
				{
					// this segment begins over the ceiling and extends beyond it
					lightlist[0].p_lightlevel = rover->toplightlevel;
					lightlist[0].caster = rover;
					lightlist[0].lightsource = rover;
					lightlist[0].extra_colormap = rover->GetColormap();
					lightlist[0].blend = rover->GetBlend();
					lightlist[0].flags = rover->flags;
				}
			}
			if (rover->flags&FF_DOUBLESHADOW)
			{
				fixed_t ff_bottom=rover->bottom.plane->ZatPoint(CenterSpot(sector));
				if(ff_bottom < maxheight && ff_bottom>minheight)
				{
					newlight.caster = rover;
					newlight.plane = *rover->bottom.plane;
					if (lightlist.Size()>1)
					{
						newlight.lightsource = lightlist[lightlist.Size()-2].lightsource;
						newlight.p_lightlevel = lightlist[lightlist.Size()-2].p_lightlevel;
						newlight.extra_colormap = lightlist[lightlist.Size()-2].extra_colormap;
						newlight.blend = lightlist[lightlist.Size()-2].blend;
					}
					else
					{
						newlight.lightsource = NULL;
						newlight.p_lightlevel = &sector->lightlevel;
						newlight.extra_colormap = sector->ColorMap;
						newlight.blend = 0;
					}
					newlight.flags = rover->flags;
					lightlist.Push(newlight);
				}
			}
		}
	}
}

//==========================================================================
//
// recalculates 3D floors for all attached sectors
//
//==========================================================================

void P_RecalculateAttached3DFloors(sector_t * sec)
{
	extsector_t::xfloor &x = sec->e->XFloor;

	for(unsigned int i=0; i<x.attached.Size(); i++)
	{
		P_Recalculate3DFloors(x.attached[i]);
	}
	P_Recalculate3DFloors(sec);
}

//==========================================================================
//
// recalculates light lists for this sector
//
//==========================================================================

void P_RecalculateLights(sector_t *sector)
{
	TArray<lightlist_t> &lightlist = sector->e->XFloor.lightlist;

	for(unsigned i = 0; i < lightlist.Size(); i++)
	{
		lightlist_t *ll = &lightlist[i];
		if (ll->lightsource != NULL)
		{
			ll->lightsource->UpdateColormap(ll->extra_colormap);
			ll->blend = ll->lightsource->GetBlend();
		}
		else
		{
			ll->extra_colormap = sector->ColorMap;
			ll->blend = 0;
		}
	}
}

//==========================================================================
//
// recalculates light lists for all attached sectors
//
//==========================================================================

void P_RecalculateAttachedLights(sector_t *sector)
{
	extsector_t::xfloor &x = sector->e->XFloor;

	for(unsigned int i=0; i<x.attached.Size(); i++)
	{
		P_RecalculateLights(x.attached[i]);
	}
	P_RecalculateLights(sector);
}

//==========================================================================
//
//
//
//==========================================================================

lightlist_t * P_GetPlaneLight(sector_t * sector, secplane_t * plane, bool underside)
{
	unsigned   i;
	TArray<lightlist_t> &lightlist = sector->e->XFloor.lightlist;

	fixed_t planeheight=plane->ZatPoint(CenterSpot(sector));
	if(underside) planeheight--;
	
	for(i = 1; i < lightlist.Size(); i++)
		if (lightlist[i].plane.ZatPoint(CenterSpot(sector)) <= planeheight) 
			return &lightlist[i - 1];
		
	return &lightlist[lightlist.Size() - 1];
}

//==========================================================================
//
// Extended P_LineOpening
//
//==========================================================================

void P_LineOpening_XFloors (FLineOpening &open, AActor * thing, const line_t *linedef, 
							fixed_t x, fixed_t y, fixed_t refx, fixed_t refy, bool restrict)
{
    if(thing)
    {
		fixed_t thingbot, thingtop;
		
		thingbot = thing->z;
		thingtop = thingbot + (thing->height==0? 1:thing->height);

		extsector_t::xfloor *xf[2] = {&linedef->frontsector->e->XFloor, &linedef->backsector->e->XFloor};

		// Check for 3D-floors in the sector (mostly identical to what Legacy does here)
		if(xf[0]->ffloors.Size() || xf[1]->ffloors.Size())
		{
			fixed_t    lowestceiling = open.top;
			fixed_t    highestfloor = open.bottom;
			fixed_t    lowestfloor[2] = {
				linedef->frontsector->floorplane.ZatPoint(x, y), 
				linedef->backsector->floorplane.ZatPoint(x, y) };
			FTextureID highestfloorpic;
			FTextureID lowestceilingpic;
			
			highestfloorpic.SetInvalid();
			lowestceilingpic.SetInvalid();
			
			for(int j=0;j<2;j++)
			{
				for(unsigned i=0;i<xf[j]->ffloors.Size();i++)
				{
					F3DFloor *rover = xf[j]->ffloors[i];

					if (!(rover->flags & FF_EXISTS)) continue;
					if (!(rover->flags & FF_SOLID)) continue;
					if ((thing->flags & MF_MISSILE) && (rover->flags & FF_SHOOTTHROUGH)) continue;
					
					fixed_t ff_bottom=rover->bottom.plane->ZatPoint(x, y);
					fixed_t ff_top=rover->top.plane->ZatPoint(x, y);
					
					fixed_t delta1 = abs(thingbot - ((ff_bottom + ff_top) / 2));
					fixed_t delta2 = abs(thingtop - ((ff_bottom + ff_top) / 2));
					
					if(ff_bottom < lowestceiling && delta1 >= delta2) 
					{
						lowestceiling = ff_bottom;
						lowestceilingpic = *rover->bottom.texture;
					}
					
					if(ff_top > highestfloor && delta1 < delta2 && (!restrict || thing->z >= ff_top))
					{
						highestfloor = ff_top;
						highestfloorpic = *rover->top.texture;
					}
					if(ff_top > lowestfloor[j] && ff_top <= thing->z + thing->MaxStepHeight) lowestfloor[j] = ff_top;
				}
			}
			
			if(highestfloor > open.bottom)
			{
				open.bottom = highestfloor;
				open.floorpic = highestfloorpic;
			}
			
			if(lowestceiling < open.top) 
			{
				open.top = lowestceiling;
				open.ceilingpic = lowestceilingpic;
			}
			
			open.lowfloor = MIN(lowestfloor[0], lowestfloor[1]);
		}
    }
}


//==========================================================================
//
// Spawns 3D floors
//
//==========================================================================
void P_Spawn3DFloors (void)
{
	static int flagvals[] = {128+512, 2+512, 512};
	int i;
	line_t * line;

	for (i=0,line=lines;i<numlines;i++,line++)
	{
		switch(line->special)
		{
		case ExtraFloor_LightOnly:
			// Note: I am spawning both this and ZDoom's ExtraLight data
			// I don't want to mess with both at the same time during rendering
			// so inserting this into the 3D-floor table as well seemed to be
			// the best option.
			//
			// This does not yet handle case 0 properly!
			if (line->args[1] < 0 || line->args[1] > 2) line->args[1] = 0;
			P_Set3DFloor(line, 3, flagvals[line->args[1]], 0);
			break;

		case Sector_Set3DFloor:
			// The flag high-byte/line id is only needed in Hexen format.
			// UDMF can set both of these parameters without any restriction of the usable values.
			// In Doom format the translators can take full integers for the tag and the line ID always is the same as the tag.
			if (level.maptype == MAPTYPE_HEXEN)	
			{
				if (line->args[1]&8)
				{
					line->id = line->args[4];
				}
				else
				{
					line->args[0]+=256*line->args[4];
					line->args[4]=0;
				}
			}
			P_Set3DFloor(line, line->args[1]&~8, line->args[2], line->args[3]);
			break;

		default:
			continue;
		}
		line->special=0;
		line->args[0] = line->args[1] = line->args[2] = line->args[3] = line->args[4] = 0;
	}
	// kg3D - do it in software
	if(vid_renderer == 0)
	for (i = 0; i < numsectors; i++)
	{
		P_Recalculate3DFloors(&sectors[i]);
	}
}


//==========================================================================
//
// Returns a 3D floorplane appropriate for the given coordinates
//
//==========================================================================

secplane_t P_FindFloorPlane(sector_t * sector, fixed_t x, fixed_t y, fixed_t z)
{
	secplane_t retplane = sector->floorplane;
	if (sector->e)	// apparently this can be called when the data is already gone
	{
		for(unsigned int i=0;i<sector->e->XFloor.ffloors.Size();i++)
		{
			F3DFloor * rover= sector->e->XFloor.ffloors[i];
			if(!(rover->flags & FF_SOLID) || !(rover->flags & FF_EXISTS)) continue;

			if (rover->top.plane->ZatPoint(x, y) == z)
			{
				retplane = *rover->top.plane;
				if (retplane.c<0) retplane.FlipVert();
				break;
			}
		}
	}
	return retplane;
}

//==========================================================================
//
// Gives the index to an extra floor above or below the given location.
// -1 means normal floor or ceiling
//
//==========================================================================

int	P_Find3DFloor(sector_t * sec, fixed_t x, fixed_t y, fixed_t z, bool above, bool floor, fixed_t &cmpz)
{
	// If no sector given, find the one appropriate
	if (sec == NULL)
		sec = R_PointInSubsector(x, y)->sector;

	// Above normal ceiling
	cmpz = sec->ceilingplane.ZatPoint(x, y);
	if (z >= cmpz)
		return -1;

	// Below normal floor
	cmpz = sec->floorplane.ZatPoint(x, y);
	if (z <= cmpz)
		return -1;

	// Looking through planes from top to bottom
	for (int i = 0; i < (signed)sec->e->XFloor.ffloors.Size(); ++i)
	{
		F3DFloor *rover = sec->e->XFloor.ffloors[i];

		// We are only interested in solid 3D floors here
		if(!(rover->flags & FF_SOLID) || !(rover->flags & FF_EXISTS)) continue;

		if (above)
		{
			// z is above that floor
			if (floor && (z >= (cmpz = rover->top.plane->ZatPoint(x, y))))
				return i - 1;
			// z is above that ceiling
			if (z >= (cmpz = rover->bottom.plane->ZatPoint(x, y)))
				return i - 1;
		}
		else // below
		{
			// z is below that ceiling
			if (!floor && (z <= (cmpz = rover->bottom.plane->ZatPoint(x, y))))
				return i;
			// z is below that floor
			if (z <= (cmpz = rover->top.plane->ZatPoint(x, y)))
				return i;
		}
	}

	// Failsafe
	return -1;
}

#endif
