// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//		Teleportation.
//
//-----------------------------------------------------------------------------


#include "templates.h"
#include "doomtype.h"
#include "doomdef.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_terrain.h"
#include "r_state.h"
#include "gi.h"
#include "a_sharedglobal.h"
#include "m_random.h"
#include "i_system.h"
#include "doomstat.h"
// [BB] New #includes.
#include "sv_commands.h"
#include "cl_main.h"
#include "network.h"

#define FUDGEFACTOR		10

static FRandom pr_teleport ("Teleport");

extern void P_CalcHeight (player_t *player);

CVAR (Bool, telezoom, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);

IMPLEMENT_CLASS (ATeleportFog)

void ATeleportFog::PostBeginPlay ()
{
	Super::PostBeginPlay ();
	S_Sound (this, CHAN_BODY, "misc/teleport", 1, ATTN_NORM);
	switch (gameinfo.gametype)
	{
	case GAME_Hexen:
	case GAME_Heretic:
		SetState(FindState(NAME_Raven));
		break;

	case GAME_Strife:
		SetState(FindState(NAME_Strife));
		break;
		
	default:
		break;
	}
}

//==========================================================================
//
// P_SpawnTeleportFog
//
// The beginning of customizable teleport fog
// (not active yet)
//
//==========================================================================

void P_SpawnTeleportFog(fixed_t x, fixed_t y, fixed_t z, int spawnid)
{
	const PClass *fog = P_GetSpawnableType(spawnid);

	if (fog == NULL)
	{
		AActor *mo = Spawn ("TeleportFog", x, y, z + TELEFOGHEIGHT, ALLOW_REPLACE);
	}
	else
	{
		AActor *mo = Spawn (fog, x, y, z, ALLOW_REPLACE);
		if (mo != NULL) S_Sound(mo, CHAN_BODY, mo->SeeSound, 1.f, ATTN_NORM);
	}
}

//
// TELEPORTATION
//

bool P_Teleport(AActor *thing, fixed_t x, fixed_t y, fixed_t z, angle_t angle,
	bool useFog, bool sourceFog, bool keepOrientation, bool bHaltVelocity, bool keepHeight)
{
	return P_Teleport(NULL, thing, x, y, z, angle,
		useFog, sourceFog, keepOrientation, bHaltVelocity, keepHeight);
}

bool P_Teleport (player_t *instigator, AActor *thing, fixed_t x, fixed_t y, fixed_t z, angle_t angle,
				 bool useFog, bool sourceFog, bool keepOrientation, bool bHaltVelocity, bool keepHeight)
{
	fixed_t oldx;
	fixed_t oldy;
	fixed_t oldz;
	fixed_t aboveFloor;
	player_t *player;
	angle_t an;
	sector_t *destsect;
	bool resetpitch = false;
	fixed_t floorheight, ceilingheight;
	fixed_t missilespeed;

	oldx = thing->x;
	oldy = thing->y;
	oldz = thing->z;
	aboveFloor = thing->z - thing->floorz;
	destsect = P_PointInSector (x, y);
	// killough 5/12/98: exclude voodoo dolls:
	player = thing->player;
	if (player && player->mo != thing)
		player = NULL;
	floorheight = destsect->floorplane.ZatPoint (x, y);
	ceilingheight = destsect->ceilingplane.ZatPoint (x, y);
	if (thing->flags & MF_MISSILE)
	{ // We don't measure z velocity, because it doesn't change.
		missilespeed = xs_CRoundToInt(TVector2<double>(thing->velx, thing->vely).Length());
	}
	if (keepHeight)
	{
		z = floorheight + aboveFloor;
	}
	else if (z == ONFLOORZ)
	{
		if (player)
		{
			if (thing->flags & MF_NOGRAVITY && aboveFloor)
			{
				z = floorheight + aboveFloor;
				if (z + thing->height > ceilingheight)
				{
					z = ceilingheight - thing->height;
				}
			}
			else
			{
				z = floorheight;
				if (!keepOrientation)
				{
					resetpitch = false;
				}
			}
		}
		else if (thing->flags & MF_MISSILE)
		{
			z = floorheight + aboveFloor;
			if (z + thing->height > ceilingheight)
			{
				z = ceilingheight - thing->height;
			}
		}
		else
		{
			z = floorheight;
		}
	}
	if (!P_TeleportMove (thing, x, y, z, false))
	{
		return false;
	}
	if (player)
	{
		player->viewz = thing->z + player->viewheight;
		if (resetpitch)
		{
			player->mo->pitch = 0;
		}
	}
	if (!keepOrientation)
	{
		thing->angle = angle;
	}
	else
	{
		angle = thing->angle;
	}

	// [BC] Spectators and predicting clients do not create fog.
	if ( ( thing && thing->player && thing->player->bSpectating )
		|| CLIENT_PREDICT_IsPredicting() )
	{
		useFog = false;
		sourceFog = false;
	}

	// Spawn teleport fog at source and destination
	if (sourceFog)
	{
		fixed_t fogDelta = thing->flags & MF_MISSILE ? 0 : TELEFOGHEIGHT;
		AActor *fog = Spawn<ATeleportFog> (oldx, oldy, oldz + fogDelta, ALLOW_REPLACE);
		fog->target = thing;
	}
	if (useFog)
	{
		fixed_t fogDelta = thing->flags & MF_MISSILE ? 0 : TELEFOGHEIGHT;
		an = angle >> ANGLETOFINESHIFT;
		AActor *fog = Spawn<ATeleportFog> (x + 20*finecosine[an],
			y + 20*finesine[an], thing->z + fogDelta, ALLOW_REPLACE);
		fog->target = thing;
		if (thing->player)
		{
			// [RH] Zoom player's field of vision
			// [BC] && bHaltVelocity.
			if (telezoom && thing->player->mo == thing && bHaltVelocity)
				thing->player->FOV = MIN (175.f, thing->player->DesiredFOV + 45.f);
		}
	}
	// [BC] && bHaltVelocity.
	if (thing->player && (useFog || !keepOrientation) && bHaltVelocity)
	{
		// Freeze player for about .5 sec
		if ( thing->Inventory == NULL || thing->Inventory->GetSpeedFactor() <= FRACUNIT )
		{
			if ( NETWORK_GetState() == NETSTATE_SERVER )
			{
				// [BB] The clients start their reactiontime later on their end. Try to adjust for this.
				thing->reactiontime = 18 - ( thing->player->ulPing * TICRATE / 1000 );
				if ( thing->reactiontime < 0 )
					thing->reactiontime = 0;
			}
			else if ( !CLIENT_PREDICT_IsPredicting() )
				thing->reactiontime = 18;
		}
	}
	if (thing->flags & MF_MISSILE)
	{
		angle >>= ANGLETOFINESHIFT;
		thing->velx = FixedMul (missilespeed, finecosine[angle]);
		thing->vely = FixedMul (missilespeed, finesine[angle]);
	}
	// [BC] && bHaltVelocity.
	else if (!keepOrientation && bHaltVelocity) // no fog doesn't alter the player's momentum
	{
		thing->velx = thing->vely = thing->velz = 0;
		// killough 10/98: kill all bobbing velocity too
		if (player)
			player->velx = player->vely = 0;
	}

	// [BC] If we're the server, update clients about this teleport.
	if ( NETWORK_GetState() == NETSTATE_SERVER )
	{
		player_t *player = thing->player;
		if (player && player->mo != thing)
			player = NULL;

		if (instigator && instigator == player)
			SERVERCOMMANDS_TeleportThing( thing, sourceFog, useFog, ( useFog && bHaltVelocity ), ULONG( instigator - players ), SVCF_SKIPTHISCLIENT );
		else
			SERVERCOMMANDS_TeleportThing( thing, sourceFog, useFog, (useFog && bHaltVelocity) );
	}

	return true;
}

static AActor *SelectTeleDest (int tid, int tag)
{
	AActor *searcher;

	// If tid is non-zero, select a destination from a matching actor at random.
	// If tag is also non-zero, the selection is restricted to actors in sectors
	// with a matching tag. If tid is zero and tag is non-zero, then the old Doom
	// behavior is used instead (return the first teleport dest found in a tagged
	// sector).

	// Compatibility hack for some maps that fell victim to a bug in the teleport code in 2.0.9x
	if (ib_compatflags & BCOMPATF_BADTELEPORTERS) tag = 0;

	if (tid != 0)
	{
		NActorIterator iterator (NAME_TeleportDest, tid);
		int count = 0;
		while ( (searcher = iterator.Next ()) )
		{
			if (tag == 0 || searcher->Sector->tag == tag)
			{
				count++;
			}
		}

		// If teleport dests were not found, the sector tag is ignored for the
		// following compatibility searches.
		// Do this only when tag is 0 because this is the only case that was defined in Hexen.
		if (count == 0)
		{
			if (tag == 0)
			{
				// Try to find a matching map spot (fixes Hexen MAP10)
				NActorIterator it2 (NAME_MapSpot, tid);
				searcher = it2.Next ();
				if (searcher == NULL)
				{
					// Try to find a matching non-blocking spot of any type (fixes Caldera MAP13)
					FActorIterator it3 (tid);
					searcher = it3.Next ();
					while (searcher != NULL && (searcher->flags & MF_SOLID))
					{
						searcher = it3.Next ();
					}
					return searcher;
				}
			}
		}
		else
		{
			if (count != 1)
			{
				count = 1 + (pr_teleport() % count);
			}
			searcher = NULL;
			while (count > 0)
			{
				searcher = iterator.Next ();
				if (tag == 0 || searcher->Sector->tag == tag)
				{
					count--;
				}
			}
		}
		return searcher;
	}

	if (tag != 0)
	{
		int secnum = -1;

		while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0)
		{
			// Scanning the snext links of things in the sector will not work, because
			// TeleportDests have MF_NOSECTOR set. So you have to search *everything*.
			// If there is more than one sector with a matching tag, then the destination
			// in the lowest-numbered sector is returned, rather than the earliest placed
			// teleport destination. This means if 50 sectors have a matching tag and
			// only the last one has a destination, *every* actor is scanned at least 49
			// times. Yuck.
			TThinkerIterator<AActor> it2(NAME_TeleportDest);
			while ((searcher = it2.Next()) != NULL)
			{
				if (searcher->Sector == sectors + secnum)
				{
					return searcher;
				}
			}
		}
	}

	return NULL;
}

bool EV_Teleport (player_t *instigator, int tid, int tag, line_t *line, int side, AActor *thing, bool fog,
				  bool sourceFog, bool keepOrientation, bool haltVelocity, bool keepHeight)
{
	AActor *searcher;
	fixed_t z;
	angle_t angle = 0;
	fixed_t s = 0, c = 0;
	fixed_t velx = 0, vely = 0;
	angle_t badangle = 0;
	angle_t	OldAngle;

	if (thing == NULL)
	{ // Teleport function called with an invalid actor
		return false;
	}
	if (thing->flags2 & MF2_NOTELEPORT)
	{
		return false;
	}
	if (side != 0)
	{ // Don't teleport if hit back of line, so you can get out of teleporter.
		return 0;
	}
	searcher = SelectTeleDest (tid, tag);
	if (searcher == NULL)
	{
		return false;
	}

	// [BC]
	OldAngle = thing->angle;

	// [RH] Lee Killough's changes for silent teleporters from BOOM
	if (keepOrientation && line)
	{
		// Get the angle between the exit thing and source linedef.
		// Rotate 90 degrees, so that walking perpendicularly across
		// teleporter linedef causes thing to exit in the direction
		// indicated by the exit thing.
		angle = R_PointToAngle2 (0, 0, line->dx, line->dy) - searcher->angle + ANG90;

		// Sine, cosine of angle adjustment
		s = finesine[angle>>ANGLETOFINESHIFT];
		c = finecosine[angle>>ANGLETOFINESHIFT];

		// Velocity of thing crossing teleporter linedef
		velx = thing->velx;
		vely = thing->vely;

		z = searcher->z;
	}
	else if (searcher->IsKindOf (PClass::FindClass(NAME_TeleportDest2)))
	{
		z = searcher->z;
	}
	else
	{
		z = ONFLOORZ;
	}
	if ((i_compatflags2 & COMPATF2_BADANGLES) && (thing->player != NULL))
	{
		badangle = 1 << ANGLETOFINESHIFT;
	}
	if (P_Teleport (instigator, thing, searcher->x, searcher->y, z, searcher->angle + badangle, fog, sourceFog, keepOrientation, haltVelocity, keepHeight))
	{
		// [RH] Lee Killough's changes for silent teleporters from BOOM
		if (!fog && line && keepOrientation)
		{
			// Rotate thing according to difference in angles
			thing->angle += angle;

			// Rotate thing's velocity to come out of exit just like it entered
			thing->velx = FixedMul(velx, c) - FixedMul(vely, s);
			thing->vely = FixedMul(vely, c) + FixedMul(velx, s);
		}
		if ((velx | vely) == 0 && thing->player != NULL && thing->player->mo == thing)
		{
			thing->player->mo->PlayIdle ();
		}

		// [BC] Adjust the thing's momentum if we didn't halt it.
		if ( haltVelocity == false )
		{
			// Get the angle between the exit thing and source linedef.
			// Rotate 90 degrees, so that walking perpendicularly across
			// teleporter linedef causes thing to exit in the direction
			// indicated by the exit thing.
//			angle = R_PointToAngle2 (0, 0, line->dx, line->dy) - searcher->angle + ANG90;
			angle = thing->angle - OldAngle;

			// Sine, cosine of angle adjustment
			s = finesine[angle>>ANGLETOFINESHIFT];
			c = finecosine[angle>>ANGLETOFINESHIFT];

			// Momentum of thing crossing teleporter linedef
			velx = thing->velx;
			vely = thing->vely;

			// Rotate thing's momentum to come out of exit just like it entered
			thing->velx = FixedMul(velx, c) - FixedMul(vely, s);
			thing->vely = FixedMul(vely, c) + FixedMul(velx, s);
		}
		return true;
	}
	return false;
}

//
// Silent linedef-based TELEPORTATION, by Lee Killough
// Primarily for rooms-over-rooms etc.
// This is the complete player-preserving kind of teleporter.
// It has advantages over the teleporter with thing exits.
//

// [RH] Modified to support different source and destination ids.
// [RH] Modified some more to be accurate.
bool EV_SilentLineTeleport (line_t *line, int side, AActor *thing, int id, INTBOOL reverse)
{
	int i;
	line_t *l;

	if (side || thing->flags2 & MF2_NOTELEPORT || !line || line->sidedef[1] == NULL)
		return false;

	for (i = -1; (i = P_FindLineFromID (id, i)) >= 0; )
	{
		if (line-lines == i)
			continue;

		if ((l=lines+i) != line && l->backsector)
		{
			// Get the thing's position along the source linedef
			SDWORD pos;				// 30.2 fixed
			fixed_t nposx, nposy;	// offsets from line
			{
				SQWORD den;

				den = (SQWORD)line->dx*line->dx + (SQWORD)line->dy*line->dy;
				if (den == 0)
				{
					pos = 0;
					nposx = 0;
					nposy = 0;
				}
				else
				{
					SQWORD num = (SQWORD)(thing->x-line->v1->x)*line->dx + 
								 (SQWORD)(thing->y-line->v1->y)*line->dy;
					if (num <= 0)
					{
						pos = 0;
					}
					else if (num >= den)
					{
						pos = 1<<30;
					}
					else
					{
						pos = (SDWORD)(num / (den>>30));
					}
					nposx = thing->x - line->v1->x - MulScale30 (line->dx, pos);
					nposy = thing->y - line->v1->y - MulScale30 (line->dy, pos);
				}
			}

			// Get the angle between the two linedefs, for rotating
			// orientation and velocity. Rotate 180 degrees, and flip
			// the position across the exit linedef, if reversed.
			angle_t angle =
				R_PointToAngle2(0, 0, l->dx, l->dy) -
				R_PointToAngle2(0, 0, line->dx, line->dy);

			if (!reverse)
			{
				angle += ANGLE_180;
				pos = (1<<30) - pos;
			}

			// Sine, cosine of angle adjustment
			fixed_t s = finesine[angle>>ANGLETOFINESHIFT];
			fixed_t c = finecosine[angle>>ANGLETOFINESHIFT];

			fixed_t x, y;

			// Rotate position along normal to match exit linedef
			x = DMulScale16 (nposx, c, -nposy, s);
			y = DMulScale16 (nposy, c,  nposx, s);

			// Interpolate position across the exit linedef
			x += l->v1->x + MulScale30 (pos, l->dx);
			y += l->v1->y + MulScale30 (pos, l->dy);

			// Whether this is a player, and if so, a pointer to its player_t.
			// Voodoo dolls are excluded by making sure thing->player->mo==thing.
			player_t *player = thing->player && thing->player->mo == thing ?
				thing->player : NULL;

			// Whether walking towards first side of exit linedef steps down
			bool stepdown = l->frontsector->floorplane.ZatPoint(x, y) < l->backsector->floorplane.ZatPoint(x, y);

			// Height of thing above ground
			fixed_t z = thing->z - thing->floorz;

			// Side to exit the linedef on positionally.
			//
			// Notes:
			//
			// This flag concerns exit position, not momentum. Due to
			// roundoff error, the thing can land on either the left or
			// the right side of the exit linedef, and steps must be
			// taken to make sure it does not end up on the wrong side.
			//
			// Exit momentum is always towards side 1 in a reversed
			// teleporter, and always towards side 0 otherwise.
			//
			// Exiting positionally on side 1 is always safe, as far
			// as avoiding oscillations and stuck-in-wall problems,
			// but may not be optimum for non-reversed teleporters.
			//
			// Exiting on side 0 can cause oscillations if momentum
			// is towards side 1, as it is with reversed teleporters.
			//
			// Exiting on side 1 slightly improves player viewing
			// when going down a step on a non-reversed teleporter.

			int side = reverse || (player && stepdown);
			int fudge = FUDGEFACTOR;

			// Make sure we are on correct side of exit linedef.
			while (P_PointOnLineSide(x, y, l) != side && --fudge >= 0)
			{
				if (abs(l->dx) > abs(l->dy))
					y -= (l->dx < 0) != side ? -1 : 1;
				else
					x += (l->dy < 0) != side ? -1 : 1;
			}

			// Adjust z position to be same height above ground as before.
			// Ground level at the exit is measured as the higher of the
			// two floor heights at the exit linedef.
			z = z + l->sidedef[stepdown]->sector->floorplane.ZatPoint(x, y);

			// Attempt to teleport, aborting if blocked
			if (!P_TeleportMove (thing, x, y, z, false))
			{
				return false;
			}

			if (thing == players[consoleplayer].camera)
			{
				R_ResetViewInterpolation ();
			}

			// Rotate thing's orientation according to difference in linedef angles
			thing->angle += angle;

			// Velocity of thing crossing teleporter linedef
			x = thing->velx;
			y = thing->vely;

			// Rotate thing's velocity to come out of exit just like it entered
			thing->velx = DMulScale16 (x, c, -y, s);
			thing->vely = DMulScale16 (y, c,  x, s);

			// Adjust a player's view, in case there has been a height change
			if (player && player->mo == thing)
			{
				// Adjust player's local copy of velocity
				x = player->velx;
				y = player->vely;
				player->velx = DMulScale16 (x, c, -y, s);
				player->vely = DMulScale16 (y, c,  x, s);

				// Save the current deltaviewheight, used in stepping
				fixed_t deltaviewheight = player->deltaviewheight;

				// Clear deltaviewheight, since we don't want any changes now
				player->deltaviewheight = 0;

				// Set player's view according to the newly set parameters
				P_CalcHeight(player);

				// Reset the delta to have the same dynamics as before
				player->deltaviewheight = deltaviewheight;
			}

			// [BC] If we're the server, send the message that this thing has been tele-
			// ported.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
				SERVERCOMMANDS_TeleportThing( thing, false, false, false );

				// [BB] The clients start their reactiontime later on their end. Try to adjust for this.
				if ( thing->player && thing->reactiontime )
					SERVER_AdjustPlayersReactiontime ( static_cast<ULONG> ( thing->player - players ) );
			}

			return true;
		}
	}
	return false;
}

// [RH] Teleport anything matching other_tid to dest_tid
bool EV_TeleportOther (int other_tid, int dest_tid, bool fog)
{
	bool didSomething = false;

	if (other_tid != 0 && dest_tid != 0)
	{
		AActor *victim;
		FActorIterator iterator (other_tid);

		while ( (victim = iterator.Next ()) )
		{
			didSomething |= EV_Teleport (NULL, dest_tid, 0, NULL, 0, victim, NULL, fog, fog, !fog);
		}
	}

	return didSomething;
}

static bool DoGroupForOne (AActor *victim, AActor *source, AActor *dest, bool floorz, bool fog)
{
	int an = (dest->angle - source->angle) >> ANGLETOFINESHIFT;
	fixed_t offX = victim->x - source->x;
	fixed_t offY = victim->y - source->y;
	angle_t offAngle = victim->angle - source->angle;
	fixed_t newX = DMulScale16 (offX, finecosine[an], -offY, finesine[an]);
	fixed_t newY = DMulScale16 (offX, finesine[an], offY, finecosine[an]);

	bool res =
		P_Teleport (NULL, victim, dest->x + newX,
							dest->y + newY,
							floorz ? ONFLOORZ : dest->z + victim->z - source->z,
							0, fog, fog, !fog);
	// P_Teleport only changes angle if fog is true
	victim->angle = dest->angle + offAngle;
	// [BB] If we change the angle of a player, we have to inform the client.
	// For proper demo recording, we inform all clients about the angle of
	// all players.
	if( victim->player && NETWORK_GetState() == NETSTATE_SERVER )
		SERVERCOMMANDS_SetThingAngle( victim );
	return res;
}

#if 0
static void MoveTheDecal (DBaseDecal *decal, fixed_t z, AActor *source, AActor *dest)
{
	int an = (dest->angle - source->angle) >> ANGLETOFINESHIFT;
	fixed_t offX = decal->x - source->x;
	fixed_t offY = decal->y - source->y;
	fixed_t newX = DMulScale16 (offX, finecosine[an], -offY, finesine[an]);
	fixed_t newY = DMulScale16 (offX, finesine[an], offY, finecosine[an]);

	decal->Relocate (dest->x + newX, dest->y + newY, dest->z + z - source->z);
}
#endif

// [RH] Teleport a group of actors centered around source_tid so
// that they become centered around dest_tid instead.
bool EV_TeleportGroup (int group_tid, AActor *victim, int source_tid, int dest_tid, bool moveSource, bool fog)
{
	AActor *sourceOrigin, *destOrigin;
	{
		FActorIterator iterator (source_tid);
		sourceOrigin = iterator.Next ();
	}
	if (sourceOrigin == NULL)
	{ // If there is no source origin, behave like TeleportOther
		return EV_TeleportOther (group_tid, dest_tid, fog);
	}

	{
		NActorIterator iterator (NAME_TeleportDest, dest_tid);
		destOrigin = iterator.Next ();
	}
	if (destOrigin == NULL)
	{
		return false;
	}

	bool didSomething = false;
	bool floorz = !destOrigin->IsKindOf (PClass::FindClass("TeleportDest2"));

	// Use the passed victim if group_tid is 0
	if (group_tid == 0 && victim != NULL)
	{
		didSomething = DoGroupForOne (victim, sourceOrigin, destOrigin, floorz, fog);
	}
	else
	{
		FActorIterator iterator (group_tid);

		// For each actor with tid matching arg0, move it to the same
		// position relative to destOrigin as it is relative to sourceOrigin
		// before the teleport.
		while ( (victim = iterator.Next ()) )
		{
			didSomething |= DoGroupForOne (victim, sourceOrigin, destOrigin, floorz, fog);
		}
	}

	if (moveSource && didSomething)
	{
		didSomething |=
			P_Teleport (NULL, sourceOrigin, destOrigin->x, destOrigin->y,
				floorz ? ONFLOORZ : destOrigin->z, 0, false, false, true);
		sourceOrigin->angle = destOrigin->angle;
	}

	return didSomething;
}

// [RH] Teleport a group of actors in a sector. Source_tid is used as a
// reference point so that they end up in the same position relative to
// dest_tid. Group_tid can be used to not teleport all actors in the sector.
bool EV_TeleportSector (int tag, int source_tid, int dest_tid, bool fog, int group_tid)
{
	AActor *sourceOrigin, *destOrigin;
	{
		FActorIterator iterator (source_tid);
		sourceOrigin = iterator.Next ();
	}
	if (sourceOrigin == NULL)
	{
		return false;
	}
	{
		NActorIterator iterator (NAME_TeleportDest, dest_tid);
		destOrigin = iterator.Next ();
	}
	if (destOrigin == NULL)
	{
		return false;
	}

	bool didSomething = false;
	bool floorz = !destOrigin->IsKindOf (PClass::FindClass("TeleportDest2"));
	int secnum;

	secnum = -1;
	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0)
	{
		msecnode_t *node;
		const sector_t * const sec = &sectors[secnum];

		for (node = sec->touching_thinglist; node; )
		{
			AActor *actor = node->m_thing;
			msecnode_t *next = node->m_snext;

			// possibly limit actors by group
			if (actor != NULL && (group_tid == 0 || actor->tid == group_tid))
			{
				didSomething |= DoGroupForOne (actor, sourceOrigin, destOrigin, floorz, fog);
			}
			node = next;
		}

#if 0
		if (group_tid == 0 && !fog)
		{
			int lineindex;
			for (lineindex = sec->linecount-1; lineindex >= 0; --lineindex)
			{
				line_t *line = sec->lines[lineindex];
				int wallnum;

				wallnum = line->sidenum[(line->backsector == sec)];
				if (wallnum != -1)
				{
					side_t *wall = &sides[wallnum];
					ADecal *decal = wall->BoundActors;

					while (decal != NULL)
					{
						ADecal *next = (ADecal *)decal->snext;
						MoveTheDecal (decal, decal->GetRealZ (wall), sourceOrigin, destOrigin);	
						decal = next;
					}
				}
			}
		}
#endif
	}
	return didSomething;
}
