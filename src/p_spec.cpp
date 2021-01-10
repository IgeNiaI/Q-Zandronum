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
//		Implements special effects:
//		Texture animation, height or lighting changes
//		 according to adjacent sectors, respective
//		 utility functions, etc.
//		Line Tag handling. Line and Sector triggers.
//		Implements donut linedef triggers
//		Initializes and implements BOOM linedef triggers for
//			Scrollers/Conveyors
//			Friction
//			Wind/Current
//
//-----------------------------------------------------------------------------


#include <stdlib.h>

#include "templates.h"
#include "doomdef.h"
#include "doomstat.h"
#include "d_event.h"
#include "gstrings.h"

#include "i_system.h"
#include "m_argv.h"
#include "m_random.h"
#include "m_bbox.h"
#include "w_wad.h"

#include "p_local.h"
#include "p_lnspec.h"
#include "p_terrain.h"
#include "p_acs.h"
#include "p_3dmidtex.h"

#include "g_game.h"

#include "s_sound.h"
#include "sc_man.h"
#include "gi.h"
#include "statnums.h"
#include "g_level.h"
#include "v_font.h"
#include "a_sharedglobal.h"
#include "farchive.h"
#include "a_keys.h"

// State.
#include "r_state.h"

#include "c_console.h"

#include "r_data/r_interpolate.h"
// [BB] New #includes.
#include "announcer.h"
#include "deathmatch.h"
#include "duel.h"
#include "network.h"
#include "team.h"
#include "lastmanstanding.h"
#include "sbar.h"
#include "sv_commands.h"
#include "cl_demo.h"
#include "cl_main.h"
#include "possession.h"
#include "cooperative.h"
#include "survival.h"
#include "gamemode.h"
#include "doomdata.h"
#include "invasion.h"
#include "unlagged.h"
#include "network_enums.h"

static FRandom pr_playerinspecialsector ("PlayerInSpecialSector");
void P_SetupPortals();


// [GrafZahl] Make this message changable by the user! ;)
CVAR(String, secretmessage, "A Secret is revealed!", CVAR_ARCHIVE)

IMPLEMENT_POINTY_CLASS (DScroller)
 DECLARE_POINTER (m_Interpolations[0])
 DECLARE_POINTER (m_Interpolations[1])
 DECLARE_POINTER (m_Interpolations[2])
END_POINTERS

IMPLEMENT_POINTY_CLASS (DPusher)
 DECLARE_POINTER (m_Source)
END_POINTERS

// [BB]
void DPusher::UpdateToClient( ULONG ulClient )
{
	SERVERCOMMANDS_DoPusher( m_Type, m_pLine, m_Magnitude, m_Angle, m_Source, m_Affectee, ulClient, SVCF_ONLYTHISCLIENT );
}

inline FArchive &operator<< (FArchive &arc, DScroller::EScrollType &type)
{
	BYTE val = (BYTE)type;
	arc << val;
	type = (DScroller::EScrollType)val;
	return arc;
}

DScroller::DScroller ()
{
}

void DScroller::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Type
		<< m_dx << m_dy
		<< m_Affectee
		<< m_Control
		<< m_LastHeight
		<< m_vdx << m_vdy
		<< m_Accel
		<< m_Parts
		<< m_Interpolations[0]
		<< m_Interpolations[1]
		<< m_Interpolations[2];
}

DPusher::DPusher ()
{
}

inline FArchive &operator<< (FArchive &arc, DPusher::EPusher &type)
{
	BYTE val = (BYTE)type;
	arc << val;
	type = (DPusher::EPusher)val;
	return arc;
}

void DPusher::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Type
		<< m_Source
		<< m_Xmag
		<< m_Ymag
		<< m_Magnitude
		<< m_Radius
		<< m_X
		<< m_Y
		<< m_Affectee;
}

// killough 3/7/98: Initialize generalized scrolling
static void P_SpawnScrollers();
static void P_SpawnFriction ();		// phares 3/16/98
static void P_SpawnPushers ();		// phares 3/20/98

EXTERN_CVAR( Int, sv_endleveldelay )

CUSTOM_CVAR ( Int, sv_killallmonsters_percentage, 100, CVAR_SERVERINFO )
{
	if ( self > 100 )
		self = 100;
	else if ( self < 0 )
		self = 0;
}

FPlayerStart *SelectRandomCooperativeSpot( ULONG ulPlayer );

// [RH] Check dmflags for noexit and respond accordingly
bool CheckIfExitIsGood (AActor *self, level_info_t *info)
{
	cluster_info_t *cluster;

	// The world can always exit itself.
	if (self == NULL)
		return true;

	// We must kill all monsters to exit the level.
	if ((dmflags2 & DF2_KILL_MONSTERS) && level.killed_monsters != level.total_monsters)
	{
		// [BB] Refine this: Instead of needing to kill all monsters, only sv_killallmonsters_percentage percent have to be killed.
		float fPercentKilled = 100 * ( static_cast<float>(level.killed_monsters) / static_cast<float>(level.total_monsters) );
		// [BB] Use the flag only in cooperative game modes, doesn't make much sense otherwise.
		if ( ( fPercentKilled < sv_killallmonsters_percentage ) && ( GAMEMODE_GetCurrentFlags() & GMF_COOPERATIVE ) )
		{
			// [BB] We have to do something when a player passes a line that should exit the level, so we just
			// teleport the player back to one of the player starts.
			if ( self->player && self->player->mo )
			{
				ULONG ulPlayer = static_cast<ULONG>( self->player - players );

				// [BB] SelectRandomCooperativeSpot calls G_CheckSpot which removes the MF_SOLID flag, we need to work around this.
				bool bSolidFlag = !!( players[ulPlayer].mo->flags & MF_SOLID );
				FPlayerStart *pSpot = SelectRandomCooperativeSpot( ulPlayer );
				if ( bSolidFlag )
					players[ulPlayer].mo->flags |=  MF_SOLID;
				P_Teleport (self, pSpot->x, pSpot->y, ONFLOORZ, ANG45 * (pSpot->angle/45), false, true, true, false);
				NETWORK_Printf( "You need to kill %d percent of the monsters before exiting the level.\n", *sv_killallmonsters_percentage );

			}
			return false;
		}
	}

	// Is this a deathmatch game and we're not allowed to exit?
	// [BC] Teamgame, too.
	// [BB] Ignore DF_NO_EXIT in lobby maps.
	if ( ((deathmatch || teamgame || alwaysapplydmflags) && (dmflags & DF_NO_EXIT) && !( GAMEMODE_IsLobbyMap( ) ) )
	     // [BB] Don't allow anybody to exit during the survival countdown.
	     || (( survival ) && ( SURVIVAL_GetState( ) == SURVS_COUNTDOWN ))
	   )
	{
		P_DamageMobj (self, self, self, TELEFRAG_DAMAGE, NAME_Exit);
		return false;
	}
	// Is this a singleplayer game and the next map is part of the same hub and we're dead?
	if (self->health <= 0 &&
		( NETWORK_GetState( ) == NETSTATE_SINGLE ) &&
		info != NULL &&
		info->cluster == level.cluster &&
		(cluster = FindClusterInfo(level.cluster)) != NULL &&
		cluster->flags & CLUSTER_HUB)
	{
		return false;
	}
	// [BC] Instead of displaying this message in deathmatch only, display it any
	// time we're not in single player mode (it can be annoying when people exit
	// the map in cooperative, and it's nice to know who's doing it).
	if ( ( NETWORK_GetState( ) != NETSTATE_SINGLE ) && gameaction != ga_completed)
	{
		// [BB] It's possible, that a monster exits the level, so self->player can be 0.
		// [TP] A voodoo doll may also exit.
		if (( self->player != NULL ) && ( self->player->mo == self ))
		{
			// [K6/BB] The server should let the clients know who exited the level.
			NETWORK_Printf ("%s \\c-exited the level.\n", self->player->userinfo.GetName());
		}
	}
	return true;
}


//
// UTILITIES
//



//
// RETURN NEXT SECTOR # THAT LINE TAG REFERS TO
//

// Find the next sector with a specified tag.
// Rewritten by Lee Killough to use chained hashing to improve speed

int P_FindSectorFromTag (int tag, int start)
{
	start = start >= 0 ? sectors[start].nexttag :
		sectors[(unsigned) tag % (unsigned) numsectors].firsttag;
	while (start >= 0 && sectors[start].tag != tag)
		start = sectors[start].nexttag;
	return start;
}

// killough 4/16/98: Same thing, only for linedefs

int P_FindLineFromID (int id, int start)
{
	start = start >= 0 ? lines[start].nextid :
		lines[(unsigned) id % (unsigned) numlines].firstid;
	while (start >= 0 && lines[start].id != id)
		start = lines[start].nextid;
	return start;
}




//============================================================================
//
// P_ActivateLine
//
//============================================================================

bool P_ActivateLine (line_t *line, AActor *mo, int side, int activationType)
{
	int lineActivation;
	INTBOOL repeat;
	INTBOOL buttonSuccess;
	BYTE special;

	// Special Zandronum checks
	// TODO [geNia] replace ML_BLOCKUSE with a new flag dedicated for client acs execution
	if ( GAMEMODE_IsHandledSpecial (mo, line->special) == false )
		return false;

	if (!P_TestActivateLine (line, mo, side, activationType))
	{
		return false;
	}
	bool remote = (line->special != 7 && line->special != 8 && (line->special < 11 || line->special > 14));
	if (line->locknumber > 0 && !P_CheckKeys (mo, line->locknumber, remote))
		return false;
	lineActivation = line->activation;
	repeat = line->flags & ML_REPEAT_SPECIAL;
	buttonSuccess = false;

	// [BB] Activating the line may trigger a sector movement. For this it's crucial
	// for the sectors to be in their actual (and not their unlagged) position.
	UNLAGGED_SwapSectorUnlaggedStatus ( );

	buttonSuccess = P_ExecuteSpecial(line->special,
					line, mo, side == 1, false,
					line->args[0], line->args[1], line->args[2],
					line->args[3], line->args[4]);

	// [BB] If the sectors were reconciled revert their actual to their reconciled positions now.
	UNLAGGED_SwapSectorUnlaggedStatus ( );

	special = line->special;
	if (!repeat && buttonSuccess)
	{ // clear the special on non-retriggerable lines
		line->special = 0;
	}

	if (buttonSuccess)
	{
		if (activationType == SPAC_Use || activationType == SPAC_Impact)
		{
			P_ChangeSwitchTexture (line->sidedef[0], repeat, special);

			// [BC] Tell the clients of the switch texture change.
//			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
//				SERVERCOMMANDS_ToggleLine( line - lines, !!repeat );
		}
	}
	// some old WADs use this method to create walls that change the texture when shot.
	else if (activationType == SPAC_Impact &&					// only for shootable triggers
		(level.flags2 & LEVEL2_DUMMYSWITCHES) &&					// this is only a compatibility setting for an old hack!
		!repeat &&												// only non-repeatable triggers
		(special<Generic_Floor || special>Generic_Crusher) &&	// not for Boom's generalized linedefs
		special &&												// not for lines without a special
		line->args[0] == line->id &&							// Safety check: exclude edited UDMF linedefs or ones that don't map the tag to args[0]
		line->args[0] &&										// only if there's a tag (which is stored in the first arg)
		P_FindSectorFromTag (line->args[0], -1) == -1)			// only if no sector is tagged to this linedef
	{
		P_ChangeSwitchTexture (line->sidedef[0], repeat, special);
		line->special = 0;

		// [BC] Tell the clients of the switch texture change.
//		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
//			SERVERCOMMANDS_ToggleLine( line - lines, !!repeat );
	}
// end of changed code
	if (developer && buttonSuccess)
	{
		Printf ("Line special %d activated on line %i\n", special, int(line - lines));
	}
	return true;
}

//============================================================================
//
// P_TestActivateLine
//
//============================================================================

bool P_TestActivateLine (line_t *line, AActor *mo, int side, int activationType)
{
 	int lineActivation = line->activation;

	if (line->flags & ML_FIRSTSIDEONLY && side == 1)
	{
		return false;
	}

	if (lineActivation & SPAC_UseThrough)
	{
		lineActivation |= SPAC_Use;
	}
	else if (line->special == Teleport &&
		(lineActivation & SPAC_Cross) &&
		activationType == SPAC_PCross &&
		mo != NULL &&
		mo->flags & MF_MISSILE)
	{ // Let missiles use regular player teleports
		lineActivation |= SPAC_PCross;
	}
	// BOOM's generalized line types that allow monster use can actually be
	// activated by anything except projectiles.
	if (lineActivation & SPAC_AnyCross)
	{
		lineActivation |= SPAC_Cross|SPAC_MCross;
	}
	if (activationType == SPAC_Use || activationType == SPAC_UseBack)
	{
		if (!P_CheckSwitchRange(mo, line, side))
		{
			return false;
		}
	}

	if ((lineActivation & activationType) == 0)
	{
		if (activationType == SPAC_Use && (lineActivation & SPAC_MUse) && !mo->player && mo->flags4 & MF4_CANUSEWALLS)
		{
			return true;
		}
		if (activationType == SPAC_Push && (lineActivation & SPAC_MPush) && !mo->player && mo->flags2 & MF2_PUSHWALL)
		{
			return true;
		}
		if (activationType != SPAC_MCross || lineActivation != SPAC_Cross)
		{ 
			return false;
		}
	}
	if (activationType == SPAC_AnyCross && (lineActivation & activationType))
	{
		return true;
	}
	if (mo && !mo->player &&
		!(mo->flags & MF_MISSILE) &&
		!(line->flags & ML_MONSTERSCANACTIVATE) &&
		(activationType != SPAC_MCross || (!(lineActivation & SPAC_MCross))))
	{ 
		// [RH] monsters' ability to activate this line depends on its type
		// In Hexen, only MCROSS lines could be activated by monsters. With
		// lax activation checks, monsters can also activate certain lines
		// even without them being marked as monster activate-able. This is
		// the default for non-Hexen maps in Hexen format.
		if (!(level.flags2 & LEVEL2_LAXMONSTERACTIVATION))
		{
			return false;
		}
		if ((activationType == SPAC_Use || activationType == SPAC_Push)
			&& (line->flags & ML_SECRET))
			return false;		// never open secret doors

		bool noway = true;

		switch (activationType)
		{
		case SPAC_Use:
		case SPAC_Push:
			switch (line->special)
			{
			case Door_Raise:
				if (line->args[0] == 0 && line->args[1] < 64)
					noway = false;
				break;
			case Teleport:
			case Teleport_NoFog:
				noway = false;
			}
			break;

		case SPAC_MCross:
			if (!(lineActivation & SPAC_MCross))
			{
				switch (line->special)
				{
				case Door_Raise:
					if (line->args[1] >= 64)
					{
						break;
					}
				// fall through
				case Teleport:
				case Teleport_NoFog:
				case Teleport_Line:
				case Plat_DownWaitUpStayLip:
				case Plat_DownWaitUpStay:
					noway = false;
				}
			}
			else noway = false;
			break;

		default:
			noway = false;
		}
		return !noway;
	}
	if (activationType == SPAC_MCross && !(lineActivation & SPAC_MCross) &&
		!(line->flags & ML_MONSTERSCANACTIVATE))
	{
		return false;
	}
	return true;
}

//
// P_PlayerInSpecialSector
// Called every tic frame
//	that the player origin is in a special sector
//
void P_PlayerInSpecialSector (player_t *player, sector_t * sector)
{
	// [BB] Some restructuring compared to the ZDoom code to handle the client case.
	bool bSectorWasNull = false;
	if (sector == NULL)
	{
		sector = player->mo->Sector;
		bSectorWasNull = true;
	}
	int special = sector->special & ~SECRET_MASK;

	// [BC] Sector specials are server-side.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	// [BB] Replaced check.
	if ( bSectorWasNull )
	{
		// Falling, not all the way down yet?
		if (player->mo->z != sector->floorplane.ZatPoint (player->mo->x, player->mo->y)
			&& !player->mo->waterlevel)
		{
			return;
		}
	}

	// Has hit ground.
	AInventory *ironfeet;

	// Allow subclasses. Better would be to implement it as armor and let that reduce
	// the damage as part of the normal damage procedure. Unfortunately, I don't have
	// different damage types yet, so that's not happening for now.
	for (ironfeet = player->mo->Inventory; ironfeet != NULL; ironfeet = ironfeet->Inventory)
	{
		if (ironfeet->IsKindOf (RUNTIME_CLASS(APowerIronFeet)))
			break;
	}

	// [RH] Normal DOOM special or BOOM specialized?
	if (special >= dLight_Flicker && special <= 255)
	{
		switch (special)
		{
		case Sector_Heal:
			// CoD's healing sector
			if (!(level.time & 0x1f))
				P_GiveBody (player->mo, 1);
			break;

		case Damage_InstantDeath:
			// Strife's instant death sector
			P_DamageMobj (player->mo, NULL, NULL, 999, NAME_InstantDeath);
			break;

		case dDamage_Hellslime:
			// HELLSLIME DAMAGE
			if (ironfeet == NULL && !(level.time&0x1f))
				P_DamageMobj (player->mo, NULL, NULL, 10, NAME_Slime);
			break;

		case dDamage_Nukage:
			// NUKAGE DAMAGE
		case sLight_Strobe_Hurt:
			if (ironfeet == NULL && !(level.time&0x1f))
				P_DamageMobj (player->mo, NULL, NULL, 5, NAME_Slime);
			break;

		case hDamage_Sludge:
			if (ironfeet == NULL && !(level.time&0x1f))
				P_DamageMobj (player->mo, NULL, NULL, 4, NAME_Slime);
			break;

		case dDamage_SuperHellslime:
			// SUPER HELLSLIME DAMAGE
		case dLight_Strobe_Hurt:
			// STROBE HURT
			if (ironfeet == NULL || pr_playerinspecialsector() < 5)
			{
				if (!(level.time&0x1f))
					P_DamageMobj (player->mo, NULL, NULL, 20, NAME_Slime);
			}
			break;

		case sDamage_Hellslime:
			// [Dusk] Don't touch the hazard count as the client
			if (ironfeet == NULL && ( NETWORK_InClientMode() == false ))
			{
				player->hazardcount += 2;

				// [Dusk] Update the hazard count
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SetPlayerHazardCount ( static_cast<ULONG>(player - players) );
			}
			break;

		case sDamage_SuperHellslime:
			// [Dusk] Don't touch the hazard count as the client
			if (ironfeet == NULL && ( NETWORK_InClientMode() == false ))
			{
				player->hazardcount += 4;

				// [Dusk] Update the hazard count
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SetPlayerHazardCount ( static_cast<ULONG>(player - players) );
			}
			break;

		case dDamage_End:
			// EXIT SUPER DAMAGE! (for E1M8 finale)
			player->cheats &= ~CF_GODMODE;

			if (!(level.time & 0x1f))
				P_DamageMobj (player->mo, NULL, NULL, 20, NAME_None);

			// [BB] Also do a CheckIfExitIsGood good check to properly handle DF_NO_EXIT.
			// This check kills the player in case exiting is forbidden.
			if (player->health <= 10 && CheckIfExitIsGood ( player->mo, NULL ) )
				G_ExitLevel(0, false);
			break;

		case dDamage_LavaWimpy:
		case dScroll_EastLavaDamage:
			if (!(level.time & 15))
			{
				P_DamageMobj(player->mo, NULL, NULL, 5, NAME_Fire);
				P_HitFloor(player->mo);
			}
			break;

		case dDamage_LavaHefty:
			if(!(level.time & 15))
			{
				P_DamageMobj(player->mo, NULL, NULL, 8, NAME_Fire);
				P_HitFloor(player->mo);
			}
			break;

		default:
			// [RH] Ignore unknown specials
			break;
		}
	}
	else
	{
		//jff 3/14/98 handle extended sector types for secrets and damage
		switch (special & DAMAGE_MASK)
		{
		case 0x000: // no damage
			break;
		case 0x100: // 2/5 damage per 31 ticks
			if (ironfeet == NULL && !(level.time&0x1f))
				P_DamageMobj (player->mo, NULL, NULL, 5, NAME_Fire);
			break;
		case 0x200: // 5/10 damage per 31 ticks
			if (ironfeet == NULL && !(level.time&0x1f))
				P_DamageMobj (player->mo, NULL, NULL, 10, NAME_Slime);
			break;
		case 0x300: // 10/20 damage per 31 ticks
			if (ironfeet == NULL
				|| pr_playerinspecialsector() < 5)	// take damage even with suit
			{
				if (!(level.time&0x1f))
					P_DamageMobj (player->mo, NULL, NULL, 20, NAME_Slime);
			}
			break;
		}
	}

	// [RH] Apply any customizable damage
	if (sector->damage)
	{
		if (sector->damage < 20)
		{
			if (ironfeet == NULL && !(level.time&0x1f))
				P_DamageMobj (player->mo, NULL, NULL, sector->damage, MODtoDamageType (sector->mod));
		}
		else if (sector->damage < 50)
		{
			if ((ironfeet == NULL || (pr_playerinspecialsector()<5))
				 && !(level.time&0x1f))
			{
				P_DamageMobj (player->mo, NULL, NULL, sector->damage, MODtoDamageType (sector->mod));
			}
		}
		else
		{
			P_DamageMobj (player->mo, NULL, NULL, sector->damage, MODtoDamageType (sector->mod));
		}
	}

	if (sector->special & SECRET_MASK)
	{
		sector->special &= ~SECRET_MASK;
		P_GiveSecret(player->mo, true, true);
	}
}

//============================================================================
//
// P_SectorDamage
//
//============================================================================

static void DoSectorDamage(AActor *actor, sector_t *sec, int amount, FName type, const PClass *protectClass, int flags)
{
	if (!(actor->flags & MF_SHOOTABLE))
		return;

	if (!(flags & DAMAGE_NONPLAYERS) && actor->player == NULL)
		return;

	if (!(flags & DAMAGE_PLAYERS) && actor->player != NULL)
		return;

	if (!(flags & DAMAGE_IN_AIR) && actor->z != sec->floorplane.ZatPoint(actor->x, actor->y) && !actor->waterlevel)
		return;

	if (protectClass != NULL)
	{
		if (actor->FindInventory(protectClass, !!(flags & DAMAGE_SUBCLASSES_PROTECT)))
			return;
	}

	P_DamageMobj (actor, NULL, NULL, amount, type);
}

void P_SectorDamage(int tag, int amount, FName type, const PClass *protectClass, int flags)
{
	int secnum = -1;

	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0)
	{
		AActor *actor, *next;
		sector_t *sec = &sectors[secnum];

		// Do for actors in this sector.
		for (actor = sec->thinglist; actor != NULL; actor = next)
		{
			next = actor->snext;
			DoSectorDamage(actor, sec, amount, type, protectClass, flags);
		}
		// If this is a 3D floor control sector, also do for anything in/on
		// those 3D floors.
		for (unsigned i = 0; i < sec->e->XFloor.attached.Size(); ++i)
		{
			sector_t *sec2 = sec->e->XFloor.attached[i];

			for (actor = sec2->thinglist; actor != NULL; actor = next)
			{
				next = actor->snext;
				// Only affect actors touching the 3D floor
				fixed_t z1 = sec->floorplane.ZatPoint(actor->x, actor->y);
				fixed_t z2 = sec->ceilingplane.ZatPoint(actor->x, actor->y);
				if (z2 < z1)
				{
					// Account for Vavoom-style 3D floors
					fixed_t zz = z1;
					z1 = z2;
					z2 = zz;
				}
				if (actor->z + actor->height > z1)
				{
					// If DAMAGE_IN_AIR is used, anything not beneath the 3D floor will be
					// damaged (so, anything touching it or above it). Other 3D floors between
					// the actor and this one will not stop this effect.
					if ((flags & DAMAGE_IN_AIR) || actor->z <= z2)
					{
						// Here we pass the DAMAGE_IN_AIR flag to disable the floor check, since it
						// only works with the real sector's floor. We did the appropriate height checks
						// for 3D floors already.
						DoSectorDamage(actor, NULL, amount, type, protectClass, flags | DAMAGE_IN_AIR);
					}
				}
			}
		}
	}
}

//============================================================================
//
// P_GiveSecret
//
//============================================================================

// [Zandronum] `allowclient` is Zandronum extension to prevent accidental execution
// by clients unless explicitly allowed to do so.
void P_GiveSecret(AActor *actor, bool printmessage, bool playsound, bool allowclient)
{
	// [Zandronum] client must bail out if not allowed to give secret.
	if ( !allowclient && NETWORK_InClientMode() )
		return;

	if (actor != NULL)
	{
		if (actor->player != NULL)
		{
			actor->player->secretcount++;
		}
		// [Zandronum] The server needs to inform all clients (even foes) - ZDoom does so.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			BYTE secretFlags = 0;
			if ( printmessage )
				secretFlags |= SECRETFOUND_MESSAGE;
			if ( playsound )
				secretFlags |= SECRETFOUND_SOUND;
			// [Zandronum] Check if sector was secret but is not anymore.
			// We can send this even if player triggers P_GiveSecret
			// by picking up +COUNTSECRET actors in a secret sector
			// as the client-side reaction is a no-op if client
			// already knows that the sector was discovered.
			if ( actor->Sector != NULL && (actor->Sector->special & SECRET_MASK) == 0 && actor->Sector->secretsector)
				SERVERCOMMANDS_SecretMarkSectorFound( actor->Sector );
			SERVERCOMMANDS_SecretFound( actor, secretFlags );
		}
		else if (actor->CheckLocalView (consoleplayer))
		{
			if (printmessage) C_MidPrint (SmallFont, secretmessage);
			if (playsound) S_Sound (CHAN_AUTO | CHAN_UI, "misc/secret", 1, ATTN_NORM);
		}
	}
	level.found_secrets++;

	// [BB] Inform clients.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetMapNumFoundSecrets();
}

//============================================================================
//
// P_PlayerOnSpecialFlat
//
//============================================================================

void P_PlayerOnSpecialFlat (player_t *player, int floorType)
{
	if (player->mo->z > player->mo->Sector->floorplane.ZatPoint (
		player->mo->x, player->mo->y) &&
		!player->mo->waterlevel)
	{ // Player is not touching the floor
		return;
	}
	if (Terrains[floorType].DamageAmount &&
		!(level.time & Terrains[floorType].DamageTimeMask))
	{
		AInventory *ironfeet = NULL;

		if (Terrains[floorType].AllowProtection)
		{
			for (ironfeet = player->mo->Inventory; ironfeet != NULL; ironfeet = ironfeet->Inventory)
			{
				if (ironfeet->IsKindOf (RUNTIME_CLASS(APowerIronFeet)))
					break;
			}
		}

		// [Dusk] Don't apply TERRAIN damage on our own if we're the
		// client. The server will tell us when we get hurt.
		if (ironfeet == NULL && ( NETWORK_InClientMode() == false ) )
		{
			P_DamageMobj (player->mo, NULL, NULL, Terrains[floorType].DamageAmount,
				Terrains[floorType].DamageMOD);
		}
		if (Terrains[floorType].Splash != -1)
		{
			S_Sound (player->mo, CHAN_AUTO,
				Splashes[Terrains[floorType].Splash].NormalSplashSound, 1,
				ATTN_IDLE);
		}
	}
}



//
// P_UpdateSpecials
// Animate planes, scroll walls, etc.
//
void P_UpdateSpecials ()
{
	// LEVEL TIMER
	// [BB] The gamemode decides whether the timelimit is used.
	if ( GAMEMODE_IsTimelimitActive() )
	{
		// [RC] Play the five minute warning.
		if ( level.time == (int)( ( timelimit - 5 ) * TICRATE * 60 ) ) // I'm amazed this works so well without a flag.
		{
			Printf("Five minutes remain!\n");
			ANNOUNCER_PlayEntry( cl_announcer, "FiveMinuteWarning" );
		}

		// [RC] Play the one minute warning.
		else if ( level.time == (int)( ( timelimit - 1 ) * TICRATE * 60 ) )
		{
			Printf("One minute remains!\n");
			ANNOUNCER_PlayEntry( cl_announcer, "OneMinuteWarning" );
		}

		if ( NETWORK_InClientMode() == false )
		{
			if (( level.time >= (int)( timelimit * TICRATE * 60 )) && ( GAME_GetEndLevelDelay( ) == 0 ))
			{
				// Special game modes handle this differently.
				if ( duel )
					DUEL_TimeExpired( );
				else if ( lastmanstanding || teamlms )
					LASTMANSTANDING_TimeExpired( );
				else if ( possession || teampossession )
					POSSESSION_TimeExpired( );
				else if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
					TEAM_TimeExpired( );
				else if ( cooperative )
				{
					NETWORK_Printf( "%s\n", GStrings( "TXT_TIMELIMIT" ));
					GAME_SetEndLevelDelay( 1 * TICRATE );
				}
				// End the level after one second.
				else
				{
					ULONG				ulIdx;
					LONG				lWinner;
					LONG				lHighestFrags;
					bool				bTied;
					char				szString[64];

					NETWORK_Printf( "%s\n", GStrings( "TXT_TIMELIMIT" ));
					GAME_SetEndLevelDelay( 1 * TICRATE );

					// Determine the winner.
					lWinner = -1;
					lHighestFrags = INT_MIN;
					bTied = false;
					for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
					{
						// [BB] Spectators can't win.
						if ( ( playeringame[ulIdx] == false ) || players[ulIdx].bSpectating )
							continue;

						if ( players[ulIdx].fragcount > lHighestFrags )
						{
							lWinner = ulIdx;
							lHighestFrags = players[ulIdx].fragcount;
							bTied = false;
						}
						else if ( players[ulIdx].fragcount == lHighestFrags )
							bTied = true;
					}

					// [BB] In case there are no active players (only spectators), lWinner is -1.
					if ( bTied || ( lWinner == -1 ) )
						sprintf( szString, "\\cdDRAW GAME!" );
					else
					{
						if (( NETWORK_GetState( ) == NETSTATE_SINGLE_MULTIPLAYER ) && ( players[consoleplayer].mo->CheckLocalView( lWinner )))
							sprintf( szString, "YOU WIN!" );
						else
							sprintf( szString, "%s \\c-WINS!", players[lWinner].userinfo.GetName() );
					}
					V_ColorizeString( szString );

					// Display "%s WINS!" HUD message.
					GAMEMODE_DisplayStandardMessage ( szString, true );

					GAME_SetEndLevelDelay( sv_endleveldelay * TICRATE );
				}
			}
		}
	}
}



//
// SPECIAL SPAWNING
//

CUSTOM_CVAR (Bool, forcewater, false, CVAR_ARCHIVE|CVAR_SERVERINFO)
{
	if (gamestate == GS_LEVEL)
	{
		int i;

		for (i = 0; i < numsectors; i++)
		{
			sector_t *hsec = sectors[i].GetHeightSec();
			if (hsec &&
				!(sectors[i].heightsec->MoreFlags & SECF_UNDERWATER))
			{
				if (self)
				{
					hsec->MoreFlags |= SECF_FORCEDUNDERWATER;
				}
				else
				{
					hsec->MoreFlags &= ~SECF_FORCEDUNDERWATER;
				}
			}
		}
	}
}

class DLightTransfer : public DThinker
{
	DECLARE_CLASS (DLightTransfer, DThinker)

	DLightTransfer() {}
public:
	DLightTransfer (sector_t *srcSec, int target, bool copyFloor);
	void Serialize (FArchive &arc);
	void Tick ();

protected:
	static void DoTransfer (int level, int target, bool floor);

	sector_t *Source;
	int TargetTag;
	bool CopyFloor;
	short LastLight;
};

IMPLEMENT_CLASS (DLightTransfer)

void DLightTransfer::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	if (SaveVersion < 3223)
	{
		BYTE bytelight;
		arc << bytelight;
		LastLight = bytelight;
	}
	else
	{
		arc << LastLight;
	}
	arc << Source << TargetTag << CopyFloor;
}

DLightTransfer::DLightTransfer (sector_t *srcSec, int target, bool copyFloor)
{
	int secnum;

	Source = srcSec;
	TargetTag = target;
	CopyFloor = copyFloor;
	DoTransfer (LastLight = srcSec->lightlevel, target, copyFloor);

	if (copyFloor)
	{
		for (secnum = -1; (secnum = P_FindSectorFromTag (target, secnum)) >= 0; )
			sectors[secnum].ChangeFlags(sector_t::floor, 0, PLANEF_ABSLIGHTING);
	}
	else
	{
		for (secnum = -1; (secnum = P_FindSectorFromTag (target, secnum)) >= 0; )
			sectors[secnum].ChangeFlags(sector_t::ceiling, 0, PLANEF_ABSLIGHTING);
	}
	ChangeStatNum (STAT_LIGHTTRANSFER);
}

void DLightTransfer::Tick ()
{
	int light = Source->lightlevel;

	if (light != LastLight)
	{
		LastLight = light;
		DoTransfer (light, TargetTag, CopyFloor);
	}
}

void DLightTransfer::DoTransfer (int level, int target, bool floor)
{
	int secnum;

	if (floor)
	{
		for (secnum = -1; (secnum = P_FindSectorFromTag (target, secnum)) >= 0; )
			sectors[secnum].SetPlaneLight(sector_t::floor, level);
	}
	else
	{
		for (secnum = -1; (secnum = P_FindSectorFromTag (target, secnum)) >= 0; )
			sectors[secnum].SetPlaneLight(sector_t::ceiling, level);
	}
}


class DWallLightTransfer : public DThinker
{
	enum
	{
		WLF_SIDE1=1,
		WLF_SIDE2=2,
		WLF_NOFAKECONTRAST=4
	};

	DECLARE_CLASS (DWallLightTransfer, DThinker)
	DWallLightTransfer() {}
public:
	DWallLightTransfer (sector_t *srcSec, int target, BYTE flags);
	void Serialize (FArchive &arc);
	void Tick ();

protected:
	static void DoTransfer (short level, int target, BYTE flags);

	sector_t *Source;
	int TargetID;
	short LastLight;
	BYTE Flags;
};

IMPLEMENT_CLASS (DWallLightTransfer)

void DWallLightTransfer::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	if (SaveVersion < 3223)
	{
		BYTE bytelight;
		arc << bytelight;
		LastLight = bytelight;
	}
	else
	{
		arc << LastLight;
	}
	arc << Source << TargetID << Flags;
}

DWallLightTransfer::DWallLightTransfer (sector_t *srcSec, int target, BYTE flags)
{
	int linenum;
	int wallflags;

	Source = srcSec;
	TargetID = target;
	Flags = flags;
	DoTransfer (LastLight = srcSec->GetLightLevel(), target, Flags);

	if (!(flags & WLF_NOFAKECONTRAST))
	{
		wallflags = WALLF_ABSLIGHTING;
	}
	else
	{
		wallflags = WALLF_ABSLIGHTING | WALLF_NOFAKECONTRAST;
	}

	for (linenum = -1; (linenum = P_FindLineFromID (target, linenum)) >= 0; )
	{
		if (flags & WLF_SIDE1 && lines[linenum].sidedef[0] != NULL)
		{
			lines[linenum].sidedef[0]->Flags |= wallflags;
		}

		if (flags & WLF_SIDE2 && lines[linenum].sidedef[1] != NULL)
		{
			lines[linenum].sidedef[1]->Flags |= wallflags;
		}
	}
	ChangeStatNum(STAT_LIGHTTRANSFER);
}

void DWallLightTransfer::Tick ()
{
	short light = sector_t::ClampLight(Source->lightlevel);

	if (light != LastLight)
	{
		LastLight = light;
		DoTransfer (light, TargetID, Flags);
	}
}

void DWallLightTransfer::DoTransfer (short lightlevel, int target, BYTE flags)
{
	int linenum;

	for (linenum = -1; (linenum = P_FindLineFromID (target, linenum)) >= 0; )
	{
		line_t *line = &lines[linenum];

		if (flags & WLF_SIDE1 && line->sidedef[0] != NULL)
		{
			line->sidedef[0]->SetLight(lightlevel);
		}

		if (flags & WLF_SIDE2 && line->sidedef[1] != NULL)
		{
			line->sidedef[1]->SetLight(lightlevel);
		}
	}
}

//-----------------------------------------------------------------------------
//
// Portals
//
//-----------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Upper stacks go in the top sector. Lower stacks go in the bottom sector.

static void SetupFloorPortal (AStackPoint *point)
{
	NActorIterator it (NAME_LowerStackLookOnly, point->tid);
	sector_t *Sector = point->Sector;
	Sector->FloorSkyBox = static_cast<ASkyViewpoint*>(it.Next());
	if (Sector->FloorSkyBox != NULL && Sector->FloorSkyBox->bAlways)
	{
		Sector->FloorSkyBox->Mate = point;
		if (Sector->GetAlpha(sector_t::floor) == OPAQUE)
			Sector->SetAlpha(sector_t::floor, Scale (point->args[0], OPAQUE, 255));
	}
}

static void SetupCeilingPortal (AStackPoint *point)
{
	NActorIterator it (NAME_UpperStackLookOnly, point->tid);
	sector_t *Sector = point->Sector;
	Sector->CeilingSkyBox = static_cast<ASkyViewpoint*>(it.Next());
	if (Sector->CeilingSkyBox != NULL && Sector->CeilingSkyBox->bAlways)
	{
		Sector->CeilingSkyBox->Mate = point;
		if (Sector->GetAlpha(sector_t::ceiling) == OPAQUE)
			Sector->SetAlpha(sector_t::ceiling, Scale (point->args[0], OPAQUE, 255));
	}
}

static bool SpreadCeilingPortal(AStackPoint *pt, fixed_t alpha, sector_t *sector)
{
	bool fail = false;
	sector->validcount = validcount;
	for(int i=0; i<sector->linecount; i++)
	{
		line_t *line = sector->lines[i];
		sector_t *backsector = sector == line->frontsector? line->backsector : line->frontsector;
		if (line->backsector == line->frontsector) continue;
		if (backsector == NULL) { fail = true; continue; }
		if (backsector->validcount == validcount) continue;
		if (backsector->CeilingSkyBox == pt) continue;

		// Check if the backside would map to the same visplane
		if (backsector->CeilingSkyBox != NULL) { fail = true; continue; }
		if (backsector->ceilingplane != sector->ceilingplane) { fail = true; continue; }
		if (backsector->lightlevel != sector->lightlevel) { fail = true; continue; }
		if (backsector->GetTexture(sector_t::ceiling)		!= sector->GetTexture(sector_t::ceiling)) { fail = true; continue; }
		if (backsector->GetXOffset(sector_t::ceiling)		!= sector->GetXOffset(sector_t::ceiling)) { fail = true; continue; }
		if (backsector->GetYOffset(sector_t::ceiling)		!= sector->GetYOffset(sector_t::ceiling)) { fail = true; continue; }
		if (backsector->GetXScale(sector_t::ceiling)		!= sector->GetXScale(sector_t::ceiling)) { fail = true; continue; }
		if (backsector->GetYScale(sector_t::ceiling)		!= sector->GetYScale(sector_t::ceiling)) { fail = true; continue; }
		if (backsector->GetAngle(sector_t::ceiling)		!= sector->GetAngle(sector_t::ceiling)) { fail = true; continue; }
		if (SpreadCeilingPortal(pt, alpha, backsector)) { fail = true; continue; }
	}
	if (!fail) 
	{
		sector->CeilingSkyBox = pt;
		sector->SetAlpha(sector_t::ceiling, alpha);
	}
	return fail;
}

void P_SetupPortals()
{
	TThinkerIterator<AStackPoint> it;
	AStackPoint *pt;
	TArray<AStackPoint *> points;

	while ((pt = it.Next()))
	{
		FName nm = pt->GetClass()->TypeName;
		if (nm == NAME_UpperStackLookOnly)
		{
			SetupFloorPortal(pt);
		}
		else if (nm == NAME_LowerStackLookOnly)
		{
			SetupCeilingPortal(pt);
		}
		pt->special1 = 0;
		points.Push(pt);
	}
}

inline void SetPortal(sector_t *sector, int plane, AStackPoint *portal, fixed_t alpha)
{
	// plane: 0=floor, 1=ceiling, 2=both
	if (plane > 0)
	{
		if (sector->CeilingSkyBox == NULL || !sector->CeilingSkyBox->bAlways) 
		{
			sector->CeilingSkyBox = portal;
			if (sector->GetAlpha(sector_t::ceiling) == OPAQUE)
				sector->SetAlpha(sector_t::ceiling, alpha);
		}
	}
	if (plane == 2 || plane == 0)
	{
		if (sector->FloorSkyBox == NULL || !sector->FloorSkyBox->bAlways) 
		{
			sector->FloorSkyBox = portal;
		}
		if (sector->GetAlpha(sector_t::floor) == OPAQUE)
			sector->SetAlpha(sector_t::floor, alpha);
	}
}

void P_SpawnPortal(line_t *line, int sectortag, int plane, int alpha)
{
	for (int i=0;i<numlines;i++)
	{
		// We must look for the reference line with a linear search unless we want to waste the line ID for it
		// which is not a good idea.
		if (lines[i].special == Sector_SetPortal &&
			lines[i].args[0] == sectortag &&
			lines[i].args[1] == 0 &&
			lines[i].args[2] == plane &&
			lines[i].args[3] == 1)
		{
			fixed_t x1 = (line->v1->x + line->v2->x) >> 1;
			fixed_t y1 = (line->v1->y + line->v2->y) >> 1;
			fixed_t x2 = (lines[i].v1->x + lines[i].v2->x) >> 1;
			fixed_t y2 = (lines[i].v1->y + lines[i].v2->y) >> 1;
			fixed_t alpha = Scale (lines[i].args[4], OPAQUE, 255);

			AStackPoint *anchor = Spawn<AStackPoint>(x1, y1, 0, NO_REPLACE);
			AStackPoint *reference = Spawn<AStackPoint>(x2, y2, 0, NO_REPLACE);

			reference->Mate = anchor;
			anchor->Mate = reference;

			// This is so that the renderer can distinguish these portals from
			// the ones spawned with the '*StackLookOnly' things.
			reference->flags |= MF_JUSTATTACKED;
			anchor->flags |= MF_JUSTATTACKED;

		    for (int s=-1; (s = P_FindSectorFromTag(sectortag,s)) >= 0;)
			{
				SetPortal(&sectors[s], plane, reference, alpha);
			}

			for (int j=0;j<numlines;j++)
			{
				// Check if this portal needs to be copied to other sectors
				// This must be done here to ensure that it gets done only after the portal is set up
				if (lines[j].special == Sector_SetPortal &&
					lines[j].args[1] == 1 &&
					lines[j].args[2] == plane &&
					lines[j].args[3] == sectortag)
				{
					if (lines[j].args[0] == 0)
					{
						SetPortal(lines[j].frontsector, plane, reference, alpha);
					}
					else
					{
						for (int s=-1; (s = P_FindSectorFromTag(lines[j].args[0],s)) >= 0;)
						{
							SetPortal(&sectors[s], plane, reference, alpha);
						}
					}
				}
			}

			return;
		}
	}
}


//
// P_SpawnSpecials
//
// After the map has been loaded, scan for specials that spawn thinkers
//

void P_SpawnSpecials (void)
{
	sector_t *sector;
	int i;

	P_SetupPortals();

	//	Init special SECTORs.
	sector = sectors;
	for (i = 0; i < numsectors; i++, sector++)
	{
		if (sector->special == 0)
			continue;

		// [RH] All secret sectors are marked with a BOOM-ish bitfield
		if (sector->special & SECRET_MASK)
			level.total_secrets++;

		switch (sector->special & 0xff)
		{
			// [RH] Normal DOOM/Hexen specials. We clear off the special for lights
			//	  here instead of inside the spawners.

		case dLight_Flicker:
			// FLICKERING LIGHTS
			// [BC] In client mode, light specials may have been shut off by the server.
			// Therefore, we can't spawn them on our end.
			if ( NETWORK_InClientMode() == false )
			{
				new DLightFlash (sector);
			}
			sector->special &= 0xff00;
			break;

		case dLight_StrobeFast:
			// STROBE FAST
			// [BC] In client mode, light specials may have been shut off by the server.
			// Therefore, we can't spawn them on our end.
			if ( NETWORK_InClientMode() == false )
			{
				new DStrobe (sector, STROBEBRIGHT, FASTDARK, false);
			}
			sector->special &= 0xff00;
			break;
			
		case dLight_StrobeSlow:
			// STROBE SLOW
			// [BC] In client mode, light specials may have been shut off by the server.
			// Therefore, we can't spawn them on our end.
			if ( NETWORK_InClientMode() == false )
			{
				new DStrobe (sector, STROBEBRIGHT, SLOWDARK, false);
			}
			sector->special &= 0xff00;
			break;

		case dLight_Strobe_Hurt:
		case sLight_Strobe_Hurt:
			// STROBE FAST/DEATH SLIME
			// [BC] In client mode, light specials may have been shut off by the server.
			// Therefore, we can't spawn them on our end.
			if ( NETWORK_InClientMode() == false )
			{
				new DStrobe (sector, STROBEBRIGHT, FASTDARK, false);
			}
			break;

		case dLight_Glow:
			// GLOWING LIGHT
			// [BC] In client mode, light specials may have been shut off by the server.
			// Therefore, we can't spawn them on our end.
			if ( NETWORK_InClientMode() == false )
			{
				new DGlow (sector);
			}
			sector->special &= 0xff00;
			break;
			
		case dSector_DoorCloseIn30:
			// DOOR CLOSE IN 30 SECONDS
			P_SpawnDoorCloseIn30 (sector);
			break;
			
		case dLight_StrobeSlowSync:
			// SYNC STROBE SLOW
			// [BC] In client mode, light specials may have been shut off by the server.
			// Therefore, we can't spawn them on our end.
			if ( NETWORK_InClientMode() == false )
			{
				new DStrobe (sector, STROBEBRIGHT, SLOWDARK, true);
			}
			sector->special &= 0xff00;
			break;

		case dLight_StrobeFastSync:
			// SYNC STROBE FAST
			// [BC] In client mode, light specials may have been shut off by the server.
			// Therefore, we can't spawn them on our end.
			if ( NETWORK_InClientMode() == false )
				new DStrobe (sector, STROBEBRIGHT, FASTDARK, true);
			sector->special &= 0xff00;
			break;

		case dSector_DoorRaiseIn5Mins:
			// DOOR RAISE IN 5 MINUTES
			P_SpawnDoorRaiseIn5Mins (sector);
			break;
			
		case dLight_FireFlicker:
			// fire flickering
			// [BC] In client mode, light specials may have been shut off by the server.
			// Therefore, we can't spawn them on our end.
			if ( NETWORK_InClientMode() == false )
			{
				new DFireFlicker (sector);
			}
			sector->special &= 0xff00;
			break;

		case dFriction_Low:
			// [BC] In client mode, let the server tell us about sectors' friction level.
			if ( NETWORK_InClientMode() == false )
			{
				sector->friction = FRICTION_LOW;
				sector->movefactor = 0x269;
			}
			sector->special &= 0xff00;
			// [BC] In client mode, let the server tell us about sectors' friction level.
			if ( NETWORK_InClientMode() == false )
			{
				sector->special |= FRICTION_MASK;
			}
			break;

		  // [RH] Hexen-like phased lighting
		case LightSequenceStart:

			// [BC] In client mode, light specials may have been shut off by the server.
			// Therefore, we can't spawn them on our end.
			if ( NETWORK_InClientMode() == false )
			{
				new DPhased (sector);
			}
			break;

		case Light_Phased:

			// [BC] In client mode, light specials may have been shut off by the server.
			// Therefore, we can't spawn them on our end.
			if ( NETWORK_InClientMode() == false )
			{
				new DPhased (sector, 48, 63 - (sector->lightlevel & 63));
			}
			break;

		case Sky2:
			sector->sky = PL_SKYFLAT;
			break;

		case dScroll_EastLavaDamage:

			// [BC] Damage is server-side.
			if ( NETWORK_InClientMode() )
			{
				break;
			}

			new DStrobe (sector, STROBEBRIGHT, FASTDARK, false);
			new DScroller (DScroller::sc_floor, (-FRACUNIT/2)<<3,
				0, -1, int(sector-sectors), 0);
			break;

		case Sector_Hidden:
			sector->MoreFlags |= SECF_HIDDEN;
			sector->special &= 0xff00;
			break;

		default:

			// [BC] Don't run any other specials in client mode.
			if ( NETWORK_InClientMode() )
			{
				break;
			}

			if ((sector->special & 0xff) >= Scroll_North_Slow &&
				(sector->special & 0xff) <= Scroll_SouthWest_Fast)
			{ // Hexen scroll special
				static const SBYTE hexenScrollies[24][2] =
				{
					{  0,  1 }, {  0,  2 }, {  0,  4 },
					{ -1,  0 }, { -2,  0 }, { -4,  0 },
					{  0, -1 }, {  0, -2 }, {  0, -4 },
					{  1,  0 }, {  2,  0 }, {  4,  0 },
					{  1,  1 }, {  2,  2 }, {  4,  4 },
					{ -1,  1 }, { -2,  2 }, { -4,  4 },
					{ -1, -1 }, { -2, -2 }, { -4, -4 },
					{  1, -1 }, {  2, -2 }, {  4, -4 }
				};

				int i = (sector->special & 0xff) - Scroll_North_Slow;
				fixed_t dx = hexenScrollies[i][0] * (FRACUNIT/2);
				fixed_t dy = hexenScrollies[i][1] * (FRACUNIT/2);
				new DScroller (DScroller::sc_floor, dx, dy, -1, int(sector-sectors), 0);
			}
			else if ((sector->special & 0xff) >= Carry_East5 &&
					 (sector->special & 0xff) <= Carry_East35)
			{ // Heretic scroll special
			  // Only east scrollers also scroll the texture
				new DScroller (DScroller::sc_floor,
					(-FRACUNIT/2)<<((sector->special & 0xff) - Carry_East5),
					0, -1, int(sector-sectors), 0);
			}
			break;
		}
	}
	
	// Init other misc stuff

	P_SpawnScrollers(); // killough 3/7/98: Add generalized scrollers
	P_SpawnFriction();	// phares 3/12/98: New friction model using linedefs
	P_SpawnPushers();	// phares 3/20/98: New pusher model using linedefs

	for (i = 0; i < numlines; i++)
	{
		switch (lines[i].special)
		{
			int s;
			sector_t *sec;

		// killough 3/7/98:
		// support for drawn heights coming from different sector
		case Transfer_Heights:
			sec = lines[i].frontsector;
			if (lines[i].args[1] & 2)
			{
				sec->MoreFlags |= SECF_FAKEFLOORONLY;
			}
			if (lines[i].args[1] & 4)
			{
				sec->MoreFlags |= SECF_CLIPFAKEPLANES;
			}
			if (lines[i].args[1] & 8)
			{
				sec->MoreFlags |= SECF_UNDERWATER;
			}
			else if (forcewater)
			{
				sec->MoreFlags |= SECF_FORCEDUNDERWATER;
			}
			if (lines[i].args[1] & 16)
			{
				sec->MoreFlags |= SECF_IGNOREHEIGHTSEC;
			}
			if (lines[i].args[1] & 32)
			{
				sec->MoreFlags |= SECF_NOFAKELIGHT;
			}
			for (s = -1; (s = P_FindSectorFromTag(lines[i].args[0],s)) >= 0;)
			{
				sectors[s].heightsec = sec;
				sec->e->FakeFloor.Sectors.Push(&sectors[s]);
				sectors[s].AdjustFloorClip();
			}
			break;

		// killough 3/16/98: Add support for setting
		// floor lighting independently (e.g. lava)
		case Transfer_FloorLight:
			new DLightTransfer (lines[i].frontsector, lines[i].args[0], true);
			break;

		// killough 4/11/98: Add support for setting
		// ceiling lighting independently
		case Transfer_CeilingLight:
			new DLightTransfer (lines[i].frontsector, lines[i].args[0], false);
			break;

		// [Graf Zahl] Add support for setting lighting
		// per wall independently
		case Transfer_WallLight:
			new DWallLightTransfer (lines[i].frontsector, lines[i].args[0], lines[i].args[1]);
			break;

		case Sector_Attach3dMidtex:
			P_Attach3dMidtexLinesToSector(lines[i].frontsector, lines[i].args[0], lines[i].args[1], !!lines[i].args[2]);
			break;

		case Sector_SetLink:
			if (lines[i].args[0] == 0)
			{
				P_AddSectorLinks(lines[i].frontsector, lines[i].args[1], lines[i].args[2], lines[i].args[3]);
			}
			break;

		case Sector_SetPortal:
			// arg 0 = sector tag
			// arg 1 = type
			//	- 0: normal (handled here)
			//	- 1: copy (handled by the portal they copy)
			//	- 2: EE-style skybox (handled by the camera object)
			//	other values reserved for later use
			// arg 2 = 0:floor, 1:ceiling, 2:both
			// arg 3 = 0: anchor, 1: reference line
			// arg 4 = for the anchor only: alpha
			if (lines[i].args[1] == 0 && lines[i].args[3] == 0)
			{
				P_SpawnPortal(&lines[i], lines[i].args[0], lines[i].args[2], lines[i].args[4]);
			}
			break;

		// [RH] ZDoom Static_Init settings
		case Static_Init:
			switch (lines[i].args[1])
			{
			case Init_Gravity:

				// [BC] The server will give us gravity updates.
				if ( NETWORK_InClientMode() )
				{
					break;
				}

				{
				float grav = ((float)P_AproxDistance (lines[i].dx, lines[i].dy)) / (FRACUNIT * 100.0f);
				for (s = -1; (s = P_FindSectorFromTag(lines[i].args[0],s)) >= 0;)
					sectors[s].gravity = grav;
				}
				break;

			//case Init_Color:
			// handled in P_LoadSideDefs2()

			case Init_Damage:

				// [BC] Damage is server-side.
				if ( NETWORK_InClientMode() )
				{
					break;
				}

				{
					int damage = P_AproxDistance (lines[i].dx, lines[i].dy) >> FRACBITS;
					for (s = -1; (s = P_FindSectorFromTag(lines[i].args[0],s)) >= 0;)
					{
						sectors[s].damage = damage;
						sectors[s].mod = 0;//MOD_UNKNOWN;
					}
				}
				break;

			case Init_SectorLink:
				if (lines[i].args[3] == 0)
					P_AddSectorLinksByID(lines[i].frontsector, lines[i].args[0], lines[i].args[2]);
				break;

			// killough 10/98:
			//
			// Support for sky textures being transferred from sidedefs.
			// Allows scrolling and other effects (but if scrolling is
			// used, then the same sector tag needs to be used for the
			// sky sector, the sky-transfer linedef, and the scroll-effect
			// linedef). Still requires user to use F_SKY1 for the floor
			// or ceiling texture, to distinguish floor and ceiling sky.

			case Init_TransferSky:
				for (s = -1; (s = P_FindSectorFromTag(lines[i].args[0],s)) >= 0;)
					sectors[s].sky = (i+1) | PL_SKYFLAT;
				break;
			}
			break;
		}
	}

	// [BC] Save these values. If they change, and a client connects, send
	// him the new values.
	for ( i = 0; i < numsectors; i++ )
	{
		sectors[i].SavedLightLevel = sectors[i].lightlevel;
		sectors[i].SavedCeilingPic = sectors[i].GetTexture(sector_t::ceiling);
		sectors[i].SavedFloorPic = sectors[i].GetTexture(sector_t::floor);
		sectors[i].SavedCeilingPlane = sectors[i].ceilingplane;
		sectors[i].SavedFloorPlane = sectors[i].floorplane;
		sectors[i].SavedCeilingTexZ = sectors[i].GetPlaneTexZ(sector_t::ceiling);
		sectors[i].SavedFloorTexZ = sectors[i].GetPlaneTexZ(sector_t::floor);
		sectors[i].SavedColorMap = sectors[i].ColorMap;
		sectors[i].SavedFloorXOffset = sectors[i].GetXOffset(sector_t::floor);
		sectors[i].SavedFloorYOffset = sectors[i].GetYOffset(sector_t::floor,false);
		sectors[i].SavedCeilingXOffset = sectors[i].GetXOffset(sector_t::ceiling);
		sectors[i].SavedCeilingYOffset = sectors[i].GetYOffset(sector_t::ceiling,false);
		sectors[i].SavedFloorXScale = sectors[i].GetXScale(sector_t::floor);
		sectors[i].SavedFloorYScale = sectors[i].GetYScale(sector_t::floor);
		sectors[i].SavedCeilingXScale = sectors[i].GetXScale(sector_t::ceiling);
		sectors[i].SavedCeilingYScale = sectors[i].GetYScale(sector_t::ceiling);
		sectors[i].SavedFloorAngle = sectors[i].GetAngle(sector_t::floor,false);
		sectors[i].SavedCeilingAngle = sectors[i].GetAngle(sector_t::ceiling,false);
		sectors[i].SavedBaseFloorAngle = sectors[i].planes[sector_t::floor].xform.base_angle;
		sectors[i].SavedBaseFloorYOffset = sectors[i].planes[sector_t::floor].xform.base_yoffs;
		sectors[i].SavedBaseCeilingAngle = sectors[i].planes[sector_t::ceiling].xform.base_angle;
		sectors[i].SavedBaseCeilingYOffset = sectors[i].planes[sector_t::ceiling].xform.base_yoffs;
		sectors[i].SavedFriction = sectors[i].friction;
		sectors[i].SavedMoveFactor = sectors[i].movefactor;
		sectors[i].SavedSpecial = sectors[i].special;
		sectors[i].SavedDamage = sectors[i].damage;
		sectors[i].SavedMOD = sectors[i].mod;
		sectors[i].SavedCeilingReflect = sectors[i].reflect[sector_t::ceiling];
		sectors[i].SavedFloorReflect = sectors[i].reflect[sector_t::floor];
	}

	// [RH] Start running any open scripts on this map
	// [BC] Clients don't run scripts.
	// [BB] Clients only run the client side open scripts.
	if ( NETWORK_InClientMode() == false )
	{
		FBehavior::StaticStartTypedScripts (SCRIPT_Open, NULL, false);
	}
	else
	{
		FBehavior::StaticStartTypedScripts (SCRIPT_Open, NULL, false, 0, false, true);
	}
}

// killough 2/28/98:
//
// This function, with the help of r_plane.c and r_bsp.c, supports generalized
// scrolling floors and walls, with optional mobj-carrying properties, e.g.
// conveyor belts, rivers, etc. A linedef with a special type affects all
// tagged sectors the same way, by creating scrolling and/or object-carrying
// properties. Multiple linedefs may be used on the same sector and are
// cumulative, although the special case of scrolling a floor and carrying
// things on it, requires only one linedef. The linedef's direction determines
// the scrolling direction, and the linedef's length determines the scrolling
// speed. This was designed so that an edge around the sector could be used to
// control the direction of the sector's scrolling, which is usually what is
// desired.
//
// Process the active scrollers.
//
// This is the main scrolling code
// killough 3/7/98

// [RH] Compensate for rotated sector textures by rotating the scrolling
// in the opposite direction.
static void RotationComp(const sector_t *sec, int which, fixed_t dx, fixed_t dy, fixed_t &tdx, fixed_t &tdy)
{
	angle_t an = sec->GetAngle(which);
	if (an == 0)
	{
		tdx = dx;
		tdy = dy;
	}
	else
	{
		an = an >> ANGLETOFINESHIFT;
		fixed_t ca = -finecosine[an];
		fixed_t sa = -finesine[an];
		tdx = DMulScale16(dx, ca, -dy, sa);
		tdy = DMulScale16(dy, ca,  dx, sa);
	}
}

void DScroller::Tick ()
{
	fixed_t dx = m_dx, dy = m_dy, tdx, tdy;

	if (m_Control != -1)
	{	// compute scroll amounts based on a sector's height changes
		fixed_t height = sectors[m_Control].CenterFloor () +
						 sectors[m_Control].CenterCeiling ();
		fixed_t delta = height - m_LastHeight;
		m_LastHeight = height;
		dx = FixedMul(dx, delta);
		dy = FixedMul(dy, delta);
	}

	// killough 3/14/98: Add acceleration
	if (m_Accel)
	{
		m_vdx = dx += m_vdx;
		m_vdy = dy += m_vdy;
	}

	if (!(dx | dy))			// no-op if both (x,y) offsets are 0
		return;

	switch (m_Type)
	{
		case sc_side:					// killough 3/7/98: Scroll wall texture
			if (m_Parts & scw_top)
			{
				sides[m_Affectee].AddTextureXOffset(side_t::top, dx);
				sides[m_Affectee].AddTextureYOffset(side_t::top, dy);
			}
			if (m_Parts & scw_mid && (sides[m_Affectee].linedef->backsector == NULL ||
				!(sides[m_Affectee].linedef->flags&ML_3DMIDTEX)))
			{
				sides[m_Affectee].AddTextureXOffset(side_t::mid, dx);
				sides[m_Affectee].AddTextureYOffset(side_t::mid, dy);
			}
			if (m_Parts & scw_bottom)
			{
				sides[m_Affectee].AddTextureXOffset(side_t::bottom, dx);
				sides[m_Affectee].AddTextureYOffset(side_t::bottom, dy);
			}
			break;

		case sc_floor:						// killough 3/7/98: Scroll floor texture
			RotationComp(&sectors[m_Affectee], sector_t::floor, dx, dy, tdx, tdy);
			sectors[m_Affectee].AddXOffset(sector_t::floor, tdx);
			sectors[m_Affectee].AddYOffset(sector_t::floor, tdy);
			break;

		case sc_ceiling:					// killough 3/7/98: Scroll ceiling texture
			RotationComp(&sectors[m_Affectee], sector_t::ceiling, dx, dy, tdx, tdy);
			sectors[m_Affectee].AddXOffset(sector_t::ceiling, tdx);
			sectors[m_Affectee].AddYOffset(sector_t::ceiling, tdy);
			break;

		// [RH] Don't actually carry anything here. That happens later.
		case sc_carry:
			level.Scrolls[m_Affectee].ScrollX += dx;
			level.Scrolls[m_Affectee].ScrollY += dy;
			break;

		case sc_carry_ceiling:       // to be added later
			break;
	}
}

//*****************************************************************************
//
void DScroller::UpdateToClient( ULONG ulClient )
{
	switch ( m_Type )
	{
	case sc_side:
	case sc_floor:
	case sc_carry:
	case sc_ceiling:
	case sc_carry_ceiling:

		SERVERCOMMANDS_DoScroller( m_Type, m_dx, m_dy, m_Affectee, ulClient, SVCF_ONLYTHISCLIENT );
		break;
	}
}

//
// Add_Scroller()
//
// Add a generalized scroller to the thinker list.
//
// type: the enumerated type of scrolling: floor, ceiling, floor carrier,
//   wall, floor carrier & scroller
//
// (dx,dy): the direction and speed of the scrolling or its acceleration
//
// control: the sector whose heights control this scroller's effect
//   remotely, or -1 if no control sector
//
// affectee: the index of the affected object (sector or sidedef)
//
// accel: non-zero if this is an accelerative effect
//

DScroller::DScroller (EScrollType type, fixed_t dx, fixed_t dy,
					  int control, int affectee, int accel, int scrollpos)
	: DThinker (STAT_SCROLLER)
{
	m_Type = type;
	m_dx = dx;
	m_dy = dy;
	m_Accel = accel;
	m_Parts = scrollpos;
	m_vdx = m_vdy = 0;
	if ((m_Control = control) != -1)
		m_LastHeight =
			sectors[control].CenterFloor () + sectors[control].CenterCeiling ();
	m_Affectee = affectee;
	m_Interpolations[0] = m_Interpolations[1] = m_Interpolations[2] = NULL;

	switch (type)
	{
	case sc_carry:
		level.AddScroller (this, affectee);
		break;

	case sc_side:
		sides[affectee].Flags |= WALLF_NOAUTODECALS;
		if (m_Parts & scw_top)
		{
			m_Interpolations[0] = sides[m_Affectee].SetInterpolation(side_t::top);
		}
		if (m_Parts & scw_mid && (sides[m_Affectee].linedef->backsector == NULL ||
			!(sides[m_Affectee].linedef->flags&ML_3DMIDTEX)))
		{
			m_Interpolations[1] = sides[m_Affectee].SetInterpolation(side_t::mid);
		}
		if (m_Parts & scw_bottom)
		{
			m_Interpolations[2] = sides[m_Affectee].SetInterpolation(side_t::bottom);
		}
		break;

	case sc_floor:
		m_Interpolations[0] = sectors[affectee].SetInterpolation(sector_t::FloorScroll, false);
		break;

	case sc_ceiling:
		m_Interpolations[0] = sectors[affectee].SetInterpolation(sector_t::CeilingScroll, false);
		break;

	default:
		break;
	}
}

void DScroller::Destroy ()
{
	for(int i=0;i<3;i++)
	{
		if (m_Interpolations[i] != NULL)
		{
			m_Interpolations[i]->DelRef();
			m_Interpolations[i] = NULL;
		}
	}
	Super::Destroy();
}

// Adds wall scroller. Scroll amount is rotated with respect to wall's
// linedef first, so that scrolling towards the wall in a perpendicular
// direction is translated into vertical motion, while scrolling along
// the wall in a parallel direction is translated into horizontal motion.
//
// killough 5/25/98: cleaned up arithmetic to avoid drift due to roundoff

DScroller::DScroller (fixed_t dx, fixed_t dy, const line_t *l,
					 int control, int accel, int scrollpos)
	: DThinker (STAT_SCROLLER)
{
	fixed_t x = abs(l->dx), y = abs(l->dy), d;
	if (y > x)
		d = x, x = y, y = d;
	d = FixedDiv (x, finesine[(tantoangle[FixedDiv(y,x) >> DBITS] + ANG90)
						  >> ANGLETOFINESHIFT]);
	x = -FixedDiv (FixedMul(dy, l->dy) + FixedMul(dx, l->dx), d);
	y = -FixedDiv (FixedMul(dx, l->dy) - FixedMul(dy, l->dx), d);

	m_Type = sc_side;
	m_dx = x;
	m_dy = y;
	m_vdx = m_vdy = 0;
	m_Accel = accel;
	m_Parts = scrollpos;
	if ((m_Control = control) != -1)
		m_LastHeight = sectors[control].CenterFloor() + sectors[control].CenterCeiling();
	m_Affectee = int(l->sidedef[0] - sides);
	sides[m_Affectee].Flags |= WALLF_NOAUTODECALS;
	m_Interpolations[0] = m_Interpolations[1] = m_Interpolations[2] = NULL;

	if (m_Parts & scw_top)
	{
		m_Interpolations[0] = sides[m_Affectee].SetInterpolation(side_t::top);
	}
	if (m_Parts & scw_mid && (sides[m_Affectee].linedef->backsector == NULL ||
		!(sides[m_Affectee].linedef->flags&ML_3DMIDTEX)))
	{
		m_Interpolations[1] = sides[m_Affectee].SetInterpolation(side_t::mid);
	}
	if (m_Parts & scw_bottom)
	{
		m_Interpolations[2] = sides[m_Affectee].SetInterpolation(side_t::bottom);
	}
}

// Amount (dx,dy) vector linedef is shifted right to get scroll amount
#define SCROLL_SHIFT 5
#define SCROLLTYPE(i) (((i) <= 0) || ((i) & ~7) ? 7 : (i))

// Initialize the scrollers
static void P_SpawnScrollers(void)
{
	int i;
	line_t *l = lines;
	TArray<int> copyscrollers;

	for (i = 0; i < numlines; i++)
	{
		if (lines[i].special == Sector_CopyScroller)
		{
			// don't allow copying the scroller if the sector has the same tag as it would just duplicate it.
			if (lines[i].args[0] != lines[i].frontsector->tag)
			{
				copyscrollers.Push(i);
			}
			lines[i].special = 0;
		}
	}

	for (i = 0; i < numlines; i++, l++)
	{
		fixed_t dx;	// direction and speed of scrolling
		fixed_t dy;
		int control = -1, accel = 0;		// no control sector or acceleration
		int special = l->special;

		// Check for undefined parameters that are non-zero and output messages for them.
		// We don't report for specials we don't understand.
		if (special != 0)
		{
			int max = LineSpecialsInfo[special] != NULL ? LineSpecialsInfo[special]->map_args : countof(l->args);
			for (unsigned arg = max; arg < countof(l->args); ++arg)
			{
				if (l->args[arg] != 0)
				{
					Printf("Line %d (type %d:%s), arg %u is %d (should be 0)\n",
						i, special, LineSpecialsInfo[special]->name, arg+1, l->args[arg]);
				}
			}
		}

		// killough 3/7/98: Types 245-249 are same as 250-254 except that the
		// first side's sector's heights cause scrolling when they change, and
		// this linedef controls the direction and speed of the scrolling. The
		// most complicated linedef since donuts, but powerful :)
		//
		// killough 3/15/98: Add acceleration. Types 214-218 are the same but
		// are accelerative.

		// [RH] Assume that it's a scroller and zero the line's special.
		l->special = 0;

		dx = dy = 0;	// Shut up, GCC

		if (special == Scroll_Ceiling ||
			special == Scroll_Floor ||
			special == Scroll_Texture_Model)
		{
			if (l->args[1] & 3)
			{
				// if 1, then displacement
				// if 2, then accelerative (also if 3)
				control = int(l->sidedef[0]->sector - sectors);
				if (l->args[1] & 2)
					accel = 1;
			}
			if (special == Scroll_Texture_Model ||
				l->args[1] & 4)
			{
				// The line housing the special controls the
				// direction and speed of scrolling.
				dx = l->dx >> SCROLL_SHIFT;
				dy = l->dy >> SCROLL_SHIFT;
			}
			else
			{
				// The speed and direction are parameters to the special.
				dx = (l->args[3] - 128) * (FRACUNIT / 32);
				dy = (l->args[4] - 128) * (FRACUNIT / 32);
			}
		}

		switch (special)
		{
			register int s;

		case Scroll_Ceiling:

			// [BC] The server will update these for us.
			if ( NETWORK_InClientMode() )
				break;

			for (s=-1; (s = P_FindSectorFromTag (l->args[0],s)) >= 0;)
			{
				new DScroller (DScroller::sc_ceiling, -dx, dy, control, s, accel);
			}
			for(unsigned j = 0;j < copyscrollers.Size(); j++)
			{
				line_t *line = &lines[copyscrollers[j]];

				if (line->args[0] == l->args[0] && (line->args[1] & 1))
				{
					new DScroller (DScroller::sc_ceiling, -dx, dy, control, int(line->frontsector-sectors), accel);
				}
			}
			break;

		case Scroll_Floor:

			// [BC] The server will update these for us.
			if ( NETWORK_InClientMode() )
				break;

			if (l->args[2] != 1)
			{ // scroll the floor texture
				for (s=-1; (s = P_FindSectorFromTag (l->args[0],s)) >= 0;)
				{
					new DScroller (DScroller::sc_floor, -dx, dy, control, s, accel);
				}
				for(unsigned j = 0;j < copyscrollers.Size(); j++)
				{
					line_t *line = &lines[copyscrollers[j]];

					if (line->args[0] == l->args[0] && (line->args[1] & 2))
					{
						new DScroller (DScroller::sc_floor, -dx, dy, control, int(line->frontsector-sectors), accel);
					}
				}
			}

			if (l->args[2] > 0)
			{ // carry objects on the floor
				for (s=-1; (s = P_FindSectorFromTag (l->args[0],s)) >= 0;)
				{
					new DScroller (DScroller::sc_carry, dx, dy, control, s, accel);
				}
				for(unsigned j = 0;j < copyscrollers.Size(); j++)
				{
					line_t *line = &lines[copyscrollers[j]];

					if (line->args[0] == l->args[0] && (line->args[1] & 4))
					{
						new DScroller (DScroller::sc_carry, dx, dy, control, int(line->frontsector-sectors), accel);
					}
				}
			}
			break;

		// killough 3/1/98: scroll wall according to linedef
		// (same direction and speed as scrolling floors)
		case Scroll_Texture_Model:

			// [WS] The server will update these for us.
			if ( NETWORK_InClientMode() )
				break;

			for (s=-1; (s = P_FindLineFromID (l->args[0],s)) >= 0;)
				if (s != i)
					new DScroller (dx, dy, lines+s, control, accel);
			break;

		case Scroll_Texture_Offsets:

			// [WS] The server will update these for us.
			if ( NETWORK_InClientMode() )
				break;

			// killough 3/2/98: scroll according to sidedef offsets
			s = int(lines[i].sidedef[0] - sides);
			new DScroller (DScroller::sc_side, -sides[s].GetTextureXOffset(side_t::mid),
				sides[s].GetTextureYOffset(side_t::mid), -1, s, accel, SCROLLTYPE(l->args[0]));
			break;

		case Scroll_Texture_Left:

			// [WS] The server will update these for us.
			if ( NETWORK_InClientMode() )
				break;

			l->special = special;	// Restore the special, for compat_useblocking's benefit.
			s = int(lines[i].sidedef[0] - sides);
			new DScroller (DScroller::sc_side, l->args[0] * (FRACUNIT/64), 0,
						   -1, s, accel, SCROLLTYPE(l->args[1]));
			break;

		case Scroll_Texture_Right:

			// [WS] The server will update these for us.
			if ( NETWORK_InClientMode() )
				break;

			l->special = special;
			s = int(lines[i].sidedef[0] - sides);
			new DScroller (DScroller::sc_side, l->args[0] * (-FRACUNIT/64), 0,
						   -1, s, accel, SCROLLTYPE(l->args[1]));
			break;

		case Scroll_Texture_Up:

			// [WS] The server will update these for us.
			if ( NETWORK_InClientMode() )
				break;

			l->special = special;
			s = int(lines[i].sidedef[0] - sides);
			new DScroller (DScroller::sc_side, 0, l->args[0] * (FRACUNIT/64),
						   -1, s, accel, SCROLLTYPE(l->args[1]));
			break;

		case Scroll_Texture_Down:

			// [WS] The server will update these for us.
			if ( NETWORK_InClientMode() )
				break;

			l->special = special;
			s = int(lines[i].sidedef[0] - sides);
			new DScroller (DScroller::sc_side, 0, l->args[0] * (-FRACUNIT/64),
						   -1, s, accel, SCROLLTYPE(l->args[1]));
			break;

		case Scroll_Texture_Both:

			// [WS] The server will update these for us.
			if ( NETWORK_InClientMode() )
				break;

			s = int(lines[i].sidedef[0] - sides);
			if (l->args[0] == 0) {
				dx = (l->args[1] - l->args[2]) * (FRACUNIT/64);
				dy = (l->args[4] - l->args[3]) * (FRACUNIT/64);
				new DScroller (DScroller::sc_side, dx, dy, -1, s, accel);
			}
			break;

		default:
			// [RH] It wasn't a scroller after all, so restore the special.
			l->special = special;
			break;
		}
	}
}

// killough 3/7/98 -- end generalized scroll effects

////////////////////////////////////////////////////////////////////////////
//
// FRICTION EFFECTS
//
// phares 3/12/98: Start of friction effects

// As the player moves, friction is applied by decreasing the x and y
// velocity values on each tic. By varying the percentage of decrease,
// we can simulate muddy or icy conditions. In mud, the player slows
// down faster. In ice, the player slows down more slowly.
//
// The amount of friction change is controlled by the length of a linedef
// with type 223. A length < 100 gives you mud. A length > 100 gives you ice.
//
// Also, each sector where these effects are to take place is given a
// new special type _______. Changing the type value at runtime allows
// these effects to be turned on or off.
//
// Sector boundaries present problems. The player should experience these
// friction changes only when his feet are touching the sector floor. At
// sector boundaries where floor height changes, the player can find
// himself still 'in' one sector, but with his feet at the floor level
// of the next sector (steps up or down). To handle this, Thinkers are used
// in icy/muddy sectors. These thinkers examine each object that is touching
// their sectors, looking for players whose feet are at the same level as
// their floors. Players satisfying this condition are given new friction
// values that are applied by the player movement code later.

//
// killough 8/28/98:
//
// Completely redid code, which did not need thinkers, and which put a heavy
// drag on CPU. Friction is now a property of sectors, NOT objects inside
// them. All objects, not just players, are affected by it, if they touch
// the sector's floor. Code simpler and faster, only calling on friction
// calculations when an object needs friction considered, instead of doing
// friction calculations on every sector during every tic.
//
// Although this -might- ruin Boom demo sync involving friction, it's the only
// way, short of code explosion, to fix the original design bug. Fixing the
// design bug in Boom's original friction code, while maintaining demo sync
// under every conceivable circumstance, would double or triple code size, and
// would require maintenance of buggy legacy code which is only useful for old
// demos. Doom demos, which are more important IMO, are not affected by this
// change.
//
// [RH] On the other hand, since I've given up on trying to maintain demo
//		sync between versions, these considerations aren't a big deal to me.
//
/////////////////////////////
//
// Initialize the sectors where friction is increased or decreased

static void P_SpawnFriction(void)
{
	int i;
	line_t *l = lines;

	// [BC] Don't do this in client mode, because the friction for the sector could
	// have changed at some point on the server end.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	for (i = 0 ; i < numlines ; i++,l++)
	{
		if (l->special == Sector_SetFriction)
		{
			int length;

			if (l->args[1])
			{	// [RH] Allow setting friction amount from parameter
				length = l->args[1] <= 200 ? l->args[1] : 200;
			}
			else
			{
				length = P_AproxDistance(l->dx,l->dy)>>FRACBITS;
			}

			P_SetSectorFriction (l->args[0], length, false);
			l->special = 0;
		}
	}
}

void P_SetSectorFriction (int tag, int amount, bool alterFlag)
{
	int s;
	fixed_t friction, movefactor;

	// An amount of 100 should result in a friction of
	// ORIG_FRICTION (0xE800)
	friction = (0x1EB8*amount)/0x80 + 0xD001;

	// killough 8/28/98: prevent odd situations
	if (friction > FRACUNIT)
		friction = FRACUNIT;
	if (friction < 0)
		friction = 0;

	// The following check might seem odd. At the time of movement,
	// the move distance is multiplied by 'friction/0x10000', so a
	// higher friction value actually means 'less friction'.

	// [RH] Twiddled these values so that velocity on ice (with
	//		friction 0xf900) is the same as in Heretic/Hexen.
	if (friction >= ORIG_FRICTION)	// ice
//		movefactor = ((0x10092 - friction)*(0x70))/0x158;
		movefactor = ((0x10092 - friction) * 1024) / 4352 + 568;
	else
		movefactor = ((friction - 0xDB34)*(0xA))/0x80;

	// killough 8/28/98: prevent odd situations
	if (movefactor < 32)
		movefactor = 32;

	for (s = -1; (s = P_FindSectorFromTag (tag,s)) >= 0; )
	{
		// killough 8/28/98:
		//
		// Instead of spawning thinkers, which are slow and expensive,
		// modify the sector's own friction values. Friction should be
		// a property of sectors, not objects which reside inside them.
		// Original code scanned every object in every friction sector
		// on every tic, adjusting its friction, putting unnecessary
		// drag on CPU. New code adjusts friction of sector only once
		// at level startup, and then uses this friction value.

		sectors[s].friction = friction;
		sectors[s].movefactor = movefactor;
		if (alterFlag)
		{
			// When used inside a script, the sectors' friction flags
			// can be enabled and disabled at will.
			if (friction == ORIG_FRICTION)
			{
				sectors[s].special &= ~FRICTION_MASK;
			}
			else
			{
				sectors[s].special |= FRICTION_MASK;
			}

			// [BC] If we're the server, update clients about this friction change.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetSectorFriction( s );
		}
	}
}

//
// phares 3/12/98: End of friction effects
//
////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
//
// PUSH/PULL EFFECT
//
// phares 3/20/98: Start of push/pull effects
//
// This is where push/pull effects are applied to objects in the sectors.
//
// There are four kinds of push effects
//
// 1) Pushing Away
//
//    Pushes you away from a point source defined by the location of an
//    MT_PUSH Thing. The force decreases linearly with distance from the
//    source. This force crosses sector boundaries and is felt w/in a circle
//    whose center is at the MT_PUSH. The force is felt only if the point
//    MT_PUSH can see the target object.
//
// 2) Pulling toward
//
//    Same as Pushing Away except you're pulled toward an MT_PULL point
//    source. This force crosses sector boundaries and is felt w/in a circle
//    whose center is at the MT_PULL. The force is felt only if the point
//    MT_PULL can see the target object.
//
// 3) Wind
//
//    Pushes you in a constant direction. Full force above ground, half
//    force on the ground, nothing if you're below it (water).
//
// 4) Current
//
//    Pushes you in a constant direction. No force above ground, full
//    force if on the ground or below it (water).
//
// The magnitude of the force is controlled by the length of a controlling
// linedef. The force vector for types 3 & 4 is determined by the angle
// of the linedef, and is constant.
//
// For each sector where these effects occur, the sector special type has
// to have the PUSH_MASK bit set. If this bit is turned off by a switch
// at run-time, the effect will not occur. The controlling sector for
// types 1 & 2 is the sector containing the MT_PUSH/MT_PULL Thing.


#define PUSH_FACTOR 7

/////////////////////////////
//
// Add a push thinker to the thinker list

DPusher::DPusher (DPusher::EPusher type, line_t *l, int magnitude, int angle,
				  AActor *source, int affectee)
{
	// [BB] Save angle and line. This makes it easier to inform the clients about this pusher.
	m_pLine = l;
	m_Angle = angle;

	m_Source = source;
	m_Type = type;
	if (l)
	{
		m_Xmag = l->dx>>FRACBITS;
		m_Ymag = l->dy>>FRACBITS;
		m_Magnitude = P_AproxDistance (m_Xmag, m_Ymag);
	}
	else
	{ // [RH] Allow setting magnitude and angle with parameters
		ChangeValues (magnitude, angle);
	}
	if (source) // point source exist?
	{
		m_Radius = (m_Magnitude) << (FRACBITS+1); // where force goes to zero
		m_X = m_Source->x;
		m_Y = m_Source->y;
	}
	m_Affectee = affectee;
}

int DPusher::CheckForSectorMatch (EPusher type, int tag)
{
	if (m_Type == type && sectors[m_Affectee].tag == tag)
		return m_Affectee;
	else
		return -1;
}


/////////////////////////////
//
// T_Pusher looks for all objects that are inside the radius of
// the effect.
//
void DPusher::Tick ()
{
	sector_t *sec;
	AActor *thing;
	msecnode_t *node;
	int xspeed,yspeed;
	int ht;

	if (!var_pushers)
		return;

	sec = sectors + m_Affectee;

	// Be sure the special sector type is still turned on. If so, proceed.
	// Else, bail out; the sector type has been changed on us.

	if (!(sec->special & PUSH_MASK))
		return;

	// For constant pushers (wind/current) there are 3 situations:
	//
	// 1) Affected Thing is above the floor.
	//
	//    Apply the full force if wind, no force if current.
	//
	// 2) Affected Thing is on the ground.
	//
	//    Apply half force if wind, full force if current.
	//
	// 3) Affected Thing is below the ground (underwater effect).
	//
	//    Apply no force if wind, full force if current.
	//
	// Apply the effect to clipped players only for now.
	//
	// In Phase II, you can apply these effects to Things other than players.
	// [RH] No Phase II, but it works with anything having MF2_WINDTHRUST now.

	if (m_Type == p_push)
	{
		// Seek out all pushable things within the force radius of this
		// point pusher. Crosses sectors, so use blockmap.

		FBlockThingsIterator it(FBoundingBox(m_X, m_Y, m_Radius));
		AActor *thing;

		while ((thing = it.Next()))
		{
			// [BB] While predicting, only handle the body of the predicted player.
			if ( CLIENT_PREDICT_IsPredicting() && ( ( thing->player == NULL ) || ( static_cast<int>( thing->player - players ) != consoleplayer ) ) )
				continue;

			// [BB] Don't affect spectators.
			if ( thing->player && thing->player->bSpectating )
				continue;

			// Normal ZDoom is based only on the WINDTHRUST flag, with the noclip cheat as an exemption.
			bool pusharound = ((thing->flags2 & MF2_WINDTHRUST) && !(thing->flags & MF_NOCLIP));
					
			// MBF allows any sentient or shootable thing to be affected, but players with a fly cheat aren't.
			if (compatflags & COMPATF_MBFMONSTERMOVE)
			{
				pusharound = ((pusharound || (thing->IsSentient()) || (thing->flags & MF_SHOOTABLE)) // Add categories here
					&& (!(thing->player && (thing->flags & (MF_NOGRAVITY))))); // Exclude flying players here
			}

			if ((pusharound) )
			{
				int sx = m_X;
				int sy = m_Y;
				int dist = P_AproxDistance (thing->x - sx,thing->y - sy);
				int speed = (m_Magnitude - ((dist>>FRACBITS)>>1))<<(FRACBITS-PUSH_FACTOR-1);

				// If speed <= 0, you're outside the effective radius. You also have
				// to be able to see the push/pull source point.

				if ((speed > 0) && (P_CheckSight (thing, m_Source, SF_IGNOREVISIBILITY)))
				{
					angle_t pushangle = R_PointToAngle2 (thing->x, thing->y, sx, sy);
					if (m_Source->GetClass()->TypeName == NAME_PointPusher)
						pushangle += ANG180;    // away
					pushangle >>= ANGLETOFINESHIFT;
					thing->velx += FixedMul (speed, finecosine[pushangle]);
					thing->vely += FixedMul (speed, finesine[pushangle]);
				}
			}
		}
		return;
	}

	// constant pushers p_wind and p_current

	node = sec->touching_thinglist; // things touching this sector
	for ( ; node ; node = node->m_snext)
	{
		thing = node->m_thing;
		if (!(thing->flags2 & MF2_WINDTHRUST) || (thing->flags & MF_NOCLIP))
			continue;

		// [BB] While predicting, only handle the body of the predicted player.
		if ( CLIENT_PREDICT_IsPredicting() && ( ( thing->player == NULL ) || ( static_cast<int>( thing->player - players ) != consoleplayer ) ) )
			continue;

		// [BB] Don't affect spectators.
		if ( thing->player && thing->player->bSpectating )
			continue;

		sector_t *hsec = sec->GetHeightSec();
		if (m_Type == p_wind)
		{
			if (hsec == NULL)
			{ // NOT special water sector
				if (thing->z > thing->floorz) // above ground
				{
					xspeed = m_Xmag; // full force
					yspeed = m_Ymag;
				}
				else // on ground
				{
					xspeed = (m_Xmag)>>1; // half force
					yspeed = (m_Ymag)>>1;
				}
			}
			else // special water sector
			{
				ht = hsec->floorplane.ZatPoint (thing->x, thing->y);
				if (thing->z > ht) // above ground
				{
					xspeed = m_Xmag; // full force
					yspeed = m_Ymag;
				}
				else if (thing->player->viewz < ht) // underwater
				{
					xspeed = yspeed = 0; // no force
				}
				else // wading in water
				{
					xspeed = (m_Xmag)>>1; // half force
					yspeed = (m_Ymag)>>1;
				}
			}
		}
		else // p_current
		{
			const secplane_t *floor;

			if (hsec == NULL)
			{ // NOT special water sector
				floor = &sec->floorplane;
			}
			else
			{ // special water sector
				floor = &hsec->floorplane;
			}
			if (thing->z > floor->ZatPoint (thing->x, thing->y))
			{ // above ground
				xspeed = yspeed = 0; // no force
			}
			else
			{ // on ground/underwater
				xspeed = m_Xmag; // full force
				yspeed = m_Ymag;
			}
		}
		thing->velx += xspeed<<(FRACBITS-PUSH_FACTOR);
		thing->vely += yspeed<<(FRACBITS-PUSH_FACTOR);
	}
}

/////////////////////////////
//
// P_GetPushThing() returns a pointer to an MT_PUSH or MT_PULL thing,
// NULL otherwise.

AActor *P_GetPushThing (int s)
{
	AActor* thing;
	sector_t* sec;

	sec = sectors + s;
	thing = sec->thinglist;

	while (thing &&
		thing->GetClass()->TypeName != NAME_PointPusher &&
		thing->GetClass()->TypeName != NAME_PointPuller)
	{
		thing = thing->snext;
	}
	return thing;
}

/////////////////////////////
//
// Initialize the sectors where pushers are present
//

static void P_SpawnPushers ()
{
	int i;
	line_t *l = lines;
	register int s;

	for (i = 0; i < numlines; i++, l++)
	{
		switch (l->special)
		{
		case Sector_SetWind: // wind
			for (s = -1; (s = P_FindSectorFromTag (l->args[0],s)) >= 0 ; )
				new DPusher (DPusher::p_wind, l->args[3] ? l : NULL, l->args[1], l->args[2], NULL, s);
			l->special = 0;
			break;

		case Sector_SetCurrent: // current
			for (s = -1; (s = P_FindSectorFromTag (l->args[0],s)) >= 0 ; )
				new DPusher (DPusher::p_current, l->args[3] ? l : NULL, l->args[1], l->args[2], NULL, s);
			l->special = 0;
			break;

		case PointPush_SetForce: // push/pull
			if (l->args[0]) {	// [RH] Find thing by sector
				for (s = -1; (s = P_FindSectorFromTag (l->args[0], s)) >= 0 ; )
				{
					AActor *thing = P_GetPushThing (s);
					if (thing) {	// No MT_P* means no effect
						// [RH] Allow narrowing it down by tid
						if (!l->args[1] || l->args[1] == thing->tid)
							new DPusher (DPusher::p_push, l->args[3] ? l : NULL, l->args[2],
										 0, thing, s);
					}
				}
			} else {	// [RH] Find thing by tid
				AActor *thing;
				FActorIterator iterator (l->args[1]);

				while ( (thing = iterator.Next ()) )
				{
					if (thing->GetClass()->TypeName == NAME_PointPusher ||
						thing->GetClass()->TypeName == NAME_PointPuller)
					{
						new DPusher (DPusher::p_push, l->args[3] ? l : NULL, l->args[2],
									 0, thing, int(thing->Sector - sectors));
					}
				}
			}
			l->special = 0;
			break;
		}
	}
}

//
// phares 3/20/98: End of Pusher effects
//
////////////////////////////////////////////////////////////////////////////

void sector_t::AdjustFloorClip () const
{
	msecnode_t *node;

	for (node = touching_thinglist; node; node = node->m_snext)
	{
		if (node->m_thing->flags2 & MF2_FLOORCLIP)
		{
			node->m_thing->AdjustFloorClip();
		}
	}
}
