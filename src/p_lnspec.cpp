/*
** p_lnspec.cpp
** Handles line specials
**
**---------------------------------------------------------------------------
** Copyright 1998-2007 Randy Heit
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
** Each function returns true if it caused something to happen
** or false if it could not perform the desired action.
*/

#include "doomstat.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "p_enemy.h"
#include "g_level.h"
#include "v_palette.h"
#include "tables.h"
#include "i_system.h"
#include "a_sharedglobal.h"
#include "a_lightning.h"
#include "statnums.h"
#include "s_sound.h"
#include "templates.h"
#include "a_keys.h"
#include "gi.h"
#include "m_random.h"
#include "p_conversation.h"
#include "a_strifeglobal.h"
#include "r_data/r_translate.h"
#include "p_3dmidtex.h"
#include "d_net.h"
#include "d_event.h"
#include "r_data/colormaps.h"
// [BC] New #includes.
#include "cooperative.h"
#include "deathmatch.h"
#include "team.h"
#include "announcer.h"
#include "sv_commands.h"
#include "network.h"
#include "invasion.h"
#include "cl_demo.h"
#include "p_acs.h"

#define FUNC(a) static int a (line_t *ln, AActor *it, bool backSide, bool isFromAcs, \
	int arg0, int arg1, int arg2, int arg3, int arg4)

#define SPEED(a)		((a)*(FRACUNIT/8))
#define TICS(a)			(((a)*TICRATE)/35)
#define OCTICS(a)		(((a)*TICRATE)/8)
#define	BYTEANGLE(a)	((angle_t)((a)<<24))
#define CRUSHTYPE(a)	((a)==1? false : (a)==2? true : gameinfo.gametype == GAME_Hexen)

static FRandom pr_glass ("GlassBreak");

// There are aliases for the ACS specials that take names instead of numbers.
// This table maps them onto the real number-based specials.
BYTE NamedACSToNormalACS[7] =
{
	ACS_Execute,
	ACS_Suspend,
	ACS_Terminate,
	ACS_LockedExecute,
	ACS_LockedExecuteDoor,
	ACS_ExecuteWithResult,
	ACS_ExecuteAlways,
};

FName MODtoDamageType (int mod)
{
	switch (mod)
	{
	default:	return NAME_None;			break;
	case 9:		return NAME_BFGSplash;		break;
	case 12:	return NAME_Drowning;		break;
	case 13:	return NAME_Slime;			break;
	case 14:	return NAME_Fire;			break;
	case 15:	return NAME_Crush;			break;
	case 16:	return NAME_Telefrag;		break;
	case 17:	return NAME_Falling;		break;
	case 18:	return NAME_Suicide;		break;
	case 20:	return NAME_Exit;			break;
	case 22:	return NAME_Melee;			break;
	case 23:	return NAME_Railgun;		break;
	case 24:	return NAME_Ice;			break;
	case 25:	return NAME_Disintegrate;	break;
	case 26:	return NAME_Poison;			break;
	case 27:	return NAME_Electric;		break;
	case 1000:	return NAME_Massacre;		break;
	}
}

player_t* GetInstigator(AActor *it, bool isFromAcs)
{
	if (isFromAcs && (zacompatflags & ZACOMPATF_NO_PREDICTION_ACS))
		return NULL;

	return it ? it->player : NULL;
}

FUNC(LS_NOP)
{
	return false;
}

FUNC(LS_Polyobj_RotateLeft)
// Polyobj_RotateLeft (po, speed, angle)
{
	return EV_RotatePoly (GetInstigator(it, isFromAcs), ln, arg0, arg1, arg2, 1, false);
}

FUNC(LS_Polyobj_RotateRight)
// Polyobj_rotateRight (po, speed, angle)
{
	return EV_RotatePoly (GetInstigator(it, isFromAcs), ln, arg0, arg1, arg2, -1, false);
}

FUNC(LS_Polyobj_Move)
// Polyobj_Move (po, speed, angle, distance)
{
	return EV_MovePoly (GetInstigator(it, isFromAcs), ln, arg0, SPEED(arg1), BYTEANGLE(arg2), arg3 * FRACUNIT, false);
}

FUNC(LS_Polyobj_MoveTimes8)
// Polyobj_MoveTimes8 (po, speed, angle, distance)
{
	return EV_MovePoly (GetInstigator(it, isFromAcs), ln, arg0, SPEED(arg1), BYTEANGLE(arg2), arg3 * FRACUNIT * 8, false);
}

FUNC(LS_Polyobj_MoveTo)
// Polyobj_MoveTo (po, speed, x, y)
{
	return EV_MovePolyTo (GetInstigator(it, isFromAcs), ln, arg0, SPEED(arg1), arg2 << FRACBITS, arg3 << FRACBITS, false);
}

FUNC(LS_Polyobj_MoveToSpot)
// Polyobj_MoveToSpot (po, speed, tid)
{
	FActorIterator iterator (arg2);
	AActor *spot = iterator.Next();
	if (spot == NULL) return false;
	return EV_MovePolyTo (GetInstigator(it, isFromAcs), ln, arg0, SPEED(arg1), spot->x, spot->y, false);
}

FUNC(LS_Polyobj_DoorSwing)
// Polyobj_DoorSwing (po, speed, angle, delay)
{
	return EV_OpenPolyDoor (GetInstigator(it, isFromAcs), ln, arg0, arg1, BYTEANGLE(arg2), arg3, 0, PODOOR_SWING);
}

FUNC(LS_Polyobj_DoorSlide)
// Polyobj_DoorSlide (po, speed, angle, distance, delay)
{
	return EV_OpenPolyDoor (GetInstigator(it, isFromAcs), ln, arg0, SPEED(arg1), BYTEANGLE(arg2), arg4, arg3*FRACUNIT, PODOOR_SLIDE);
}

FUNC(LS_Polyobj_OR_RotateLeft)
// Polyobj_OR_RotateLeft (po, speed, angle)
{
	return EV_RotatePoly (GetInstigator(it, isFromAcs), ln, arg0, arg1, arg2, 1, true);
}

FUNC(LS_Polyobj_OR_RotateRight)
// Polyobj_OR_RotateRight (po, speed, angle)
{
	return EV_RotatePoly (GetInstigator(it, isFromAcs), ln, arg0, arg1, arg2, -1, true);
}

FUNC(LS_Polyobj_OR_Move)
// Polyobj_OR_Move (po, speed, angle, distance)
{
	return EV_MovePoly (GetInstigator(it, isFromAcs), ln, arg0, SPEED(arg1), BYTEANGLE(arg2), arg3 * FRACUNIT, true);
}

FUNC(LS_Polyobj_OR_MoveTimes8)
// Polyobj_OR_MoveTimes8 (po, speed, angle, distance)
{
	return EV_MovePoly (GetInstigator(it, isFromAcs), ln, arg0, SPEED(arg1), BYTEANGLE(arg2), arg3 * FRACUNIT * 8, true);
}

FUNC(LS_Polyobj_OR_MoveTo)
// Polyobj_OR_MoveTo (po, speed, x, y)
{
	return EV_MovePolyTo (GetInstigator(it, isFromAcs), ln, arg0, SPEED(arg1), arg2 << FRACBITS, arg3 << FRACBITS, true);
}

FUNC(LS_Polyobj_OR_MoveToSpot)
// Polyobj_OR_MoveToSpot (po, speed, tid)
{
	FActorIterator iterator (arg2);
	AActor *spot = iterator.Next();
	if (spot == NULL) return false;
	return EV_MovePolyTo (GetInstigator(it, isFromAcs), ln, arg0, SPEED(arg1), spot->x, spot->y, true);
}

FUNC(LS_Polyobj_Stop)
// Polyobj_Stop (po)
{
	return EV_StopPoly (GetInstigator(it, isFromAcs), arg0);
}

FUNC(LS_Door_Close)
// Door_Close (tag, speed, lighttag)
{
	return EV_DoDoor (GetInstigator(it, isFromAcs), DDoor::doorClose, ln, it, arg0, SPEED(arg1), 0, 0, arg2);
}

FUNC(LS_Door_Open)
// Door_Open (tag, speed, lighttag)
{
	return EV_DoDoor (GetInstigator(it, isFromAcs), DDoor::doorOpen, ln, it, arg0, SPEED(arg1), 0, 0, arg2);
}

FUNC(LS_Door_Raise)
// Door_Raise (tag, speed, delay, lighttag)
{
	return EV_DoDoor (GetInstigator(it, isFromAcs), DDoor::doorRaise, ln, it, arg0, SPEED(arg1), TICS(arg2), 0, arg3);
}

FUNC(LS_Door_LockedRaise)
// Door_LockedRaise (tag, speed, delay, lock, lighttag)
{
	return EV_DoDoor (GetInstigator(it, isFromAcs), arg2 ? DDoor::doorRaise : DDoor::doorOpen, ln, it,
					  arg0, SPEED(arg1), TICS(arg2), arg3, arg4);
}

FUNC(LS_Door_CloseWaitOpen)
// Door_CloseWaitOpen (tag, speed, delay, lighttag)
{
	return EV_DoDoor (GetInstigator(it, isFromAcs), DDoor::doorCloseWaitOpen, ln, it, arg0, SPEED(arg1), OCTICS(arg2), 0, arg3);
}

FUNC(LS_Door_Animated)
// Door_Animated (tag, speed, delay, lock)
{
	if (arg3 != 0 && !P_CheckKeys (it, arg3, arg0 != 0))
		return false;

	return EV_SlidingDoor (GetInstigator(it, isFromAcs), ln, it, arg0, arg1, arg2);
}

FUNC(LS_Generic_Door)
// Generic_Door (tag, speed, kind, delay, lock)
{
	int tag, lightTag;
	DDoor::EVlDoor type;
	bool boomgen = false;

	switch (arg2 & 63)
	{
		case 0: type = DDoor::doorRaise;			break;
		case 1: type = DDoor::doorOpen;				break;
		case 2: type = DDoor::doorCloseWaitOpen;	break;
		case 3: type = DDoor::doorClose;			break;
		default: return false;
	}
	// Boom doesn't allow manual generalized doors to be activated while they move
	if (arg2 & 64) boomgen = true;
	if (arg2 & 128)
	{
		// New for 2.0.58: Finally support BOOM's local door light effect
		tag = 0;
		lightTag = arg0;
	}
	else
	{
		tag = arg0;
		lightTag = 0;
	}
	return EV_DoDoor (GetInstigator(it, isFromAcs), type, ln, it, tag, SPEED(arg1), OCTICS(arg3), arg4, lightTag, boomgen);
}

FUNC(LS_Floor_LowerByValue)
// Floor_LowerByValue (tag, speed, height)
{
	return EV_DoFloor (GetInstigator(it, isFromAcs), DFloor::floorLowerByValue, ln, arg0, SPEED(arg1), FRACUNIT*arg2, 0, 0, false);
}

FUNC(LS_Floor_LowerToLowest)
// Floor_LowerToLowest (tag, speed)
{
	return EV_DoFloor (GetInstigator(it, isFromAcs), DFloor::floorLowerToLowest, ln, arg0, SPEED(arg1), 0, 0, 0, false);
}

FUNC(LS_Floor_LowerToHighest)
// Floor_LowerToHighest (tag, speed, adjust, hereticlower)
{
	return EV_DoFloor (GetInstigator(it, isFromAcs), DFloor::floorLowerToHighest, ln, arg0, SPEED(arg1), (arg2-128)*FRACUNIT, 0, 0, false, arg3==1);
}

FUNC(LS_Floor_LowerToNearest)
// Floor_LowerToNearest (tag, speed)
{
	return EV_DoFloor (GetInstigator(it, isFromAcs), DFloor::floorLowerToNearest, ln, arg0, SPEED(arg1), 0, 0, 0, false);
}

FUNC(LS_Floor_RaiseByValue)
// Floor_RaiseByValue (tag, speed, height)
{
	return EV_DoFloor (GetInstigator(it, isFromAcs), DFloor::floorRaiseByValue, ln, arg0, SPEED(arg1), FRACUNIT*arg2, 0, 0, false);
}

FUNC(LS_Floor_RaiseToHighest)
// Floor_RaiseToHighest (tag, speed)
{
	return EV_DoFloor (GetInstigator(it, isFromAcs), DFloor::floorRaiseToHighest, ln, arg0, SPEED(arg1), 0, 0, 0, false);
}

FUNC(LS_Floor_RaiseToNearest)
// Floor_RaiseToNearest (tag, speed)
{
	return EV_DoFloor (GetInstigator(it, isFromAcs), DFloor::floorRaiseToNearest, ln, arg0, SPEED(arg1), 0, 0, 0, false);
}

FUNC(LS_Floor_RaiseAndCrush)
// Floor_RaiseAndCrush (tag, speed, crush, crushmode)
{
	return EV_DoFloor (GetInstigator(it, isFromAcs), DFloor::floorRaiseAndCrush, ln, arg0, SPEED(arg1), 0, arg2, 0, CRUSHTYPE(arg3));
}

FUNC(LS_Floor_RaiseAndCrushDoom)
// Floor_RaiseAndCrushDoom (tag, speed, crush, crushmode)
{
	return EV_DoFloor (GetInstigator(it, isFromAcs), DFloor::floorRaiseAndCrushDoom, ln, arg0, SPEED(arg1), 0, arg2, 0, CRUSHTYPE(arg3));
}

FUNC(LS_Floor_RaiseByValueTimes8)
// FLoor_RaiseByValueTimes8 (tag, speed, height)
{
	return EV_DoFloor (GetInstigator(it, isFromAcs), DFloor::floorRaiseByValue, ln, arg0, SPEED(arg1), FRACUNIT*arg2*8, 0, 0, false);
}

FUNC(LS_Floor_LowerByValueTimes8)
// Floor_LowerByValueTimes8 (tag, speed, height)
{
	return EV_DoFloor (GetInstigator(it, isFromAcs), DFloor::floorLowerByValue, ln, arg0, SPEED(arg1), FRACUNIT*arg2*8, 0, 0, false);
}

FUNC(LS_Floor_CrushStop)
// Floor_CrushStop (tag)
{
	return EV_FloorCrushStop (GetInstigator(it, isFromAcs), arg0);
}

FUNC(LS_Floor_LowerInstant)
// Floor_LowerInstant (tag, unused, height)
{
	return EV_DoFloor (GetInstigator(it, isFromAcs), DFloor::floorLowerInstant, ln, arg0, 0, arg2*FRACUNIT*8, 0, 0, false);
}

FUNC(LS_Floor_RaiseInstant)
// Floor_RaiseInstant (tag, unused, height)
{
	return EV_DoFloor (GetInstigator(it, isFromAcs), DFloor::floorRaiseInstant, ln, arg0, 0, arg2*FRACUNIT*8, 0, 0, false);
}

FUNC(LS_Floor_MoveToValueTimes8)
// Floor_MoveToValueTimes8 (tag, speed, height, negative)
{
	return EV_DoFloor (GetInstigator(it, isFromAcs), DFloor::floorMoveToValue, ln, arg0, SPEED(arg1),
					   arg2*FRACUNIT*8*(arg3?-1:1), 0, 0, false);
}

FUNC(LS_Floor_MoveToValue)
// Floor_MoveToValue (tag, speed, height, negative)
{
	return EV_DoFloor (GetInstigator(it, isFromAcs), DFloor::floorMoveToValue, ln, arg0, SPEED(arg1),
					   arg2*FRACUNIT*(arg3?-1:1), 0, 0, false);
}

FUNC(LS_Floor_RaiseToLowestCeiling)
// Floor_RaiseToLowestCeiling (tag, speed)
{
	return EV_DoFloor (GetInstigator(it, isFromAcs), DFloor::floorRaiseToLowestCeiling, ln, arg0, SPEED(arg1), 0, 0, 0, false);
}

FUNC(LS_Floor_RaiseByTexture)
// Floor_RaiseByTexture (tag, speed)
{
	return EV_DoFloor (GetInstigator(it, isFromAcs), DFloor::floorRaiseByTexture, ln, arg0, SPEED(arg1), 0, 0, 0, false);
}

FUNC(LS_Floor_RaiseByValueTxTy)
// Floor_RaiseByValueTxTy (tag, speed, height)
{
	return EV_DoFloor (GetInstigator(it, isFromAcs), DFloor::floorRaiseAndChange, ln, arg0, SPEED(arg1), arg2*FRACUNIT, 0, 0, false);
}

FUNC(LS_Floor_LowerToLowestTxTy)
// Floor_LowerToLowestTxTy (tag, speed)
{
	return EV_DoFloor (GetInstigator(it, isFromAcs), DFloor::floorLowerAndChange, ln, arg0, SPEED(arg1), arg2*FRACUNIT, 0, 0, false);
}

FUNC(LS_Floor_Waggle)
// Floor_Waggle (tag, amplitude, frequency, delay, time)
{
	return EV_StartWaggle (GetInstigator(it, isFromAcs), arg0, ln, arg1, arg2, arg3, arg4, false);
}

FUNC(LS_Ceiling_Waggle)
// Ceiling_Waggle (tag, amplitude, frequency, delay, time)
{
	return EV_StartWaggle (GetInstigator(it, isFromAcs), arg0, ln, arg1, arg2, arg3, arg4, true);
}

FUNC(LS_Floor_TransferTrigger)
// Floor_TransferTrigger (tag)
{
	return EV_DoChange (GetInstigator(it, isFromAcs), ln, trigChangeOnly, arg0);
}

FUNC(LS_Floor_TransferNumeric)
// Floor_TransferNumeric (tag)
{
	return EV_DoChange (GetInstigator(it, isFromAcs), ln, numChangeOnly, arg0);
}

FUNC(LS_Floor_Donut)
// Floor_Donut (pillartag, pillarspeed, slimespeed)
{
	return EV_DoDonut (GetInstigator(it, isFromAcs), arg0, ln, SPEED(arg1), SPEED(arg2));
}

FUNC(LS_Generic_Floor)
// Generic_Floor (tag, speed, height, target, change/model/direct/crush)
{
	DFloor::EFloor type;

	if (arg4 & 8)
	{
		switch (arg3)
		{
			case 1: type = DFloor::floorRaiseToHighest;			break;
			case 2: type = DFloor::floorRaiseToLowest;			break;
			case 3: type = DFloor::floorRaiseToNearest;			break;
			case 4: type = DFloor::floorRaiseToLowestCeiling;	break;
			case 5: type = DFloor::floorRaiseToCeiling;			break;
			case 6: type = DFloor::floorRaiseByTexture;			break;
			default:type = DFloor::floorRaiseByValue;			break;
		}
	}
	else
	{
		switch (arg3)
		{
			case 1: type = DFloor::floorLowerToHighest;			break;
			case 2: type = DFloor::floorLowerToLowest;			break;
			case 3: type = DFloor::floorLowerToNearest;			break;
			case 4: type = DFloor::floorLowerToLowestCeiling;	break;
			case 5: type = DFloor::floorLowerToCeiling;			break;
			case 6: type = DFloor::floorLowerByTexture;			break;
			default:type = DFloor::floorLowerByValue;			break;
		}
	}

	return EV_DoFloor (GetInstigator(it, isFromAcs), type, ln, arg0, SPEED(arg1), arg2*FRACUNIT,
					   (arg4 & 16) ? 20 : -1, arg4 & 7, false);
					   
}

FUNC(LS_Stairs_BuildDown)
// Stair_BuildDown (tag, speed, height, delay, reset)
{
	return EV_BuildStairs (GetInstigator(it, isFromAcs), arg0, DFloor::buildDown, ln,
						   arg2 * FRACUNIT, SPEED(arg1), TICS(arg3), arg4, 0, 1);
}

FUNC(LS_Stairs_BuildUp)
// Stairs_BuildUp (tag, speed, height, delay, reset)
{
	return EV_BuildStairs (GetInstigator(it, isFromAcs), arg0, DFloor::buildUp, ln,
						   arg2 * FRACUNIT, SPEED(arg1), TICS(arg3), arg4, 0, 1);
}

FUNC(LS_Stairs_BuildDownSync)
// Stairs_BuildDownSync (tag, speed, height, reset)
{
	return EV_BuildStairs (GetInstigator(it, isFromAcs), arg0, DFloor::buildDown, ln,
						   arg2 * FRACUNIT, SPEED(arg1), 0, arg3, 0, 2);
}

FUNC(LS_Stairs_BuildUpSync)
// Stairs_BuildUpSync (tag, speed, height, reset)
{
	return EV_BuildStairs (GetInstigator(it, isFromAcs), arg0, DFloor::buildUp, ln,
						   arg2 * FRACUNIT, SPEED(arg1), 0, arg3, 0, 2);
}

FUNC(LS_Stairs_BuildUpDoom)
// Stairs_BuildUpDoom (tag, speed, height, delay, reset)
{
	return EV_BuildStairs (GetInstigator(it, isFromAcs), arg0, DFloor::buildUp, ln,
						   arg2 * FRACUNIT, SPEED(arg1), TICS(arg3), arg4, 0, 0);
}

FUNC(LS_Generic_Stairs)
// Generic_Stairs (tag, speed, step, dir/igntxt, reset)
{
	DFloor::EStair type = (arg3 & 1) ? DFloor::buildUp : DFloor::buildDown;
	bool res = EV_BuildStairs (GetInstigator(it, isFromAcs), arg0, type, ln,
							   arg2 * FRACUNIT, SPEED(arg1), 0, arg4, arg3 & 2, 0);

	if (res && ln && (ln->flags & ML_REPEAT_SPECIAL) && ln->special == Generic_Stairs)
		// Toggle direction of next activation of repeatable stairs
		ln->args[3] ^= 1;

	return res;
}

FUNC(LS_Pillar_Build)
// Pillar_Build (tag, speed, height)
{
	return EV_DoPillar (GetInstigator(it, isFromAcs), DPillar::pillarBuild, arg0, SPEED(arg1), arg2*FRACUNIT, 0, -1, false);
}

FUNC(LS_Pillar_BuildAndCrush)
// Pillar_BuildAndCrush (tag, speed, height, crush, crushtype)
{
	return EV_DoPillar (GetInstigator(it, isFromAcs), DPillar::pillarBuild, arg0, SPEED(arg1), arg2*FRACUNIT, 0, arg3, CRUSHTYPE(arg4));
}

FUNC(LS_Pillar_Open)
// Pillar_Open (tag, speed, f_height, c_height)
{
	return EV_DoPillar (GetInstigator(it, isFromAcs), DPillar::pillarOpen, arg0, SPEED(arg1), arg2*FRACUNIT, arg3*FRACUNIT, -1, false);
}

FUNC(LS_Ceiling_LowerByValue)
// Ceiling_LowerByValue (tag, speed, height)
{
	return EV_DoCeiling (GetInstigator(it, isFromAcs), DCeiling::ceilLowerByValue, ln, arg0, SPEED(arg1), SPEED(arg1), arg2*FRACUNIT, -1, 0, 0, false);
}

FUNC(LS_Ceiling_RaiseByValue)
// Ceiling_RaiseByValue (tag, speed, height)
{
	return EV_DoCeiling (GetInstigator(it, isFromAcs), DCeiling::ceilRaiseByValue, ln, arg0, SPEED(arg1), SPEED(arg1), arg2*FRACUNIT, -1, 0, 0, false);
}

FUNC(LS_Ceiling_LowerByValueTimes8)
// Ceiling_LowerByValueTimes8 (tag, speed, height)
{
	return EV_DoCeiling (GetInstigator(it, isFromAcs), DCeiling::ceilLowerByValue, ln, arg0, SPEED(arg1), SPEED(arg1), arg2*FRACUNIT*8, -1, 0, 0, false);
}

FUNC(LS_Ceiling_RaiseByValueTimes8)
// Ceiling_RaiseByValueTimes8 (tag, speed, height)
{
	return EV_DoCeiling (GetInstigator(it, isFromAcs), DCeiling::ceilRaiseByValue, ln, arg0, SPEED(arg1), SPEED(arg1), arg2*FRACUNIT*8, -1, 0, 0, false);
}

FUNC(LS_Ceiling_CrushAndRaise)
// Ceiling_CrushAndRaise (tag, speed, crush, crushtype)
{
	return EV_DoCeiling (GetInstigator(it, isFromAcs), DCeiling::ceilCrushAndRaise, ln, arg0, SPEED(arg1), SPEED(arg1)/2, 0, arg2, 0, 0, CRUSHTYPE(arg3));
}

FUNC(LS_Ceiling_LowerAndCrush)
// Ceiling_LowerAndCrush (tag, speed, crush, crushtype)
{
	return EV_DoCeiling (GetInstigator(it, isFromAcs), DCeiling::ceilLowerAndCrush, ln, arg0, SPEED(arg1), SPEED(arg1), 0, arg2, 0, 0, CRUSHTYPE(arg3));
}

FUNC(LS_Ceiling_LowerAndCrushDist)
// Ceiling_LowerAndCrush (tag, speed, crush, dist, crushtype)
{
	return EV_DoCeiling (GetInstigator(it, isFromAcs), DCeiling::ceilLowerAndCrushDist, ln, arg0, SPEED(arg1), SPEED(arg1), arg3*FRACUNIT, arg2, 0, 0, CRUSHTYPE(arg4));
}

FUNC(LS_Ceiling_CrushStop)
// Ceiling_CrushStop (tag)
{
	return EV_CeilingCrushStop (GetInstigator(it, isFromAcs), arg0);
}

FUNC(LS_Ceiling_CrushRaiseAndStay)
// Ceiling_CrushRaiseAndStay (tag, speed, crush, crushtype)
{
	return EV_DoCeiling (GetInstigator(it, isFromAcs), DCeiling::ceilCrushRaiseAndStay, ln, arg0, SPEED(arg1), SPEED(arg1)/2, 0, arg2, 0, 0, CRUSHTYPE(arg3));
}

FUNC(LS_Ceiling_MoveToValueTimes8)
// Ceiling_MoveToValueTimes8 (tag, speed, height, negative)
{
	return EV_DoCeiling (GetInstigator(it, isFromAcs), DCeiling::ceilMoveToValue, ln, arg0, SPEED(arg1), 0,
						 arg2*FRACUNIT*8*((arg3) ? -1 : 1), -1, 0, 0, false);
}

FUNC(LS_Ceiling_MoveToValue)
// Ceiling_MoveToValue (tag, speed, height, negative)
{
	return EV_DoCeiling (GetInstigator(it, isFromAcs), DCeiling::ceilMoveToValue, ln, arg0, SPEED(arg1), 0,
						 arg2*FRACUNIT*((arg3) ? -1 : 1), -1, 0, 0, false);
}

FUNC(LS_Ceiling_LowerToHighestFloor)
// Ceiling_LowerToHighestFloor (tag, speed)
{
	return EV_DoCeiling (GetInstigator(it, isFromAcs), DCeiling::ceilLowerToHighestFloor, ln, arg0, SPEED(arg1), 0, 0, -1, 0, 0, false);
}

FUNC(LS_Ceiling_LowerInstant)
// Ceiling_LowerInstant (tag, unused, height)
{
	return EV_DoCeiling (GetInstigator(it, isFromAcs), DCeiling::ceilLowerInstant, ln, arg0, 0, 0, arg2*FRACUNIT*8, -1, 0, 0, false);
}

FUNC(LS_Ceiling_RaiseInstant)
// Ceiling_RaiseInstant (tag, unused, height)
{
	return EV_DoCeiling (GetInstigator(it, isFromAcs), DCeiling::ceilRaiseInstant, ln, arg0, 0, 0, arg2*FRACUNIT*8, -1, 0, 0, false);
}

FUNC(LS_Ceiling_CrushRaiseAndStayA)
// Ceiling_CrushRaiseAndStayA (tag, dnspeed, upspeed, damage, crushtype)
{
	return EV_DoCeiling (GetInstigator(it, isFromAcs), DCeiling::ceilCrushRaiseAndStay, ln, arg0, SPEED(arg1), SPEED(arg2), 0, arg3, 0, 0, CRUSHTYPE(arg4));
}

FUNC(LS_Ceiling_CrushRaiseAndStaySilA)
// Ceiling_CrushRaiseAndStaySilA (tag, dnspeed, upspeed, damage, crushtype)
{
	return EV_DoCeiling (GetInstigator(it, isFromAcs), DCeiling::ceilCrushRaiseAndStay, ln, arg0, SPEED(arg1), SPEED(arg2), 0, arg3, 1, 0, CRUSHTYPE(arg4));
}

FUNC(LS_Ceiling_CrushAndRaiseA)
// Ceiling_CrushAndRaiseA (tag, dnspeed, upspeed, damage, crushtype)
{
	return EV_DoCeiling (GetInstigator(it, isFromAcs), DCeiling::ceilCrushAndRaise, ln, arg0, SPEED(arg1), SPEED(arg2), 0, arg3, 0, 0, CRUSHTYPE(arg4));
}

FUNC(LS_Ceiling_CrushAndRaiseDist)
// Ceiling_CrushAndRaiseDist (tag, dist, speed, damage, crushtype)
{
	return EV_DoCeiling (GetInstigator(it, isFromAcs), DCeiling::ceilCrushAndRaiseDist, ln, arg0, SPEED(arg2), SPEED(arg2), arg1*FRACUNIT, arg3, 0, 0, CRUSHTYPE(arg4));
}

FUNC(LS_Ceiling_CrushAndRaiseSilentA)
// Ceiling_CrushAndRaiseSilentA (tag, dnspeed, upspeed, damage, crushtype)
{
	return EV_DoCeiling (GetInstigator(it, isFromAcs), DCeiling::ceilCrushAndRaise, ln, arg0, SPEED(arg1), SPEED(arg2), 0, arg3, 1, 0, CRUSHTYPE(arg4));
}

FUNC(LS_Ceiling_RaiseToNearest)
// Ceiling_RaiseToNearest (tag, speed)
{
	return EV_DoCeiling (GetInstigator(it, isFromAcs), DCeiling::ceilRaiseToNearest, ln, arg0, SPEED(arg1), 0, 0, -1, 0, 0, false);
}

FUNC(LS_Ceiling_LowerToLowest)
// Ceiling_LowerToLowest (tag, speed)
{
	return EV_DoCeiling (GetInstigator(it, isFromAcs), DCeiling::ceilLowerToLowest, ln, arg0, SPEED(arg1), 0, 0, -1, 0, 0, false);
}

FUNC(LS_Ceiling_LowerToFloor)
// Ceiling_LowerToFloor (tag, speed)
{
	return EV_DoCeiling (GetInstigator(it, isFromAcs), DCeiling::ceilLowerToFloor, ln, arg0, SPEED(arg1), 0, 0, -1, 0, 0, false);
}

FUNC(LS_Generic_Ceiling)
// Generic_Ceiling (tag, speed, height, target, change/model/direct/crush)
{
	DCeiling::ECeiling type;

	if (arg4 & 8) {
		switch (arg3) {
			case 1:  type = DCeiling::ceilRaiseToHighest;		break;
			case 2:  type = DCeiling::ceilRaiseToLowest;		break;
			case 3:  type = DCeiling::ceilRaiseToNearest;		break;
			case 4:  type = DCeiling::ceilRaiseToHighestFloor;	break;
			case 5:  type = DCeiling::ceilRaiseToFloor;			break;
			case 6:  type = DCeiling::ceilRaiseByTexture;		break;
			default: type = DCeiling::ceilRaiseByValue;			break;
		}
	} else {
		switch (arg3) {
			case 1:  type = DCeiling::ceilLowerToHighest;		break;
			case 2:  type = DCeiling::ceilLowerToLowest;		break;
			case 3:  type = DCeiling::ceilLowerToNearest;		break;
			case 4:  type = DCeiling::ceilLowerToHighestFloor;	break;
			case 5:  type = DCeiling::ceilLowerToFloor;			break;
			case 6:  type = DCeiling::ceilLowerByTexture;		break;
			default: type = DCeiling::ceilLowerByValue;			break;
		}
	}

	return EV_DoCeiling (GetInstigator(it, isFromAcs), type, ln, arg0, SPEED(arg1), SPEED(arg1), arg2*FRACUNIT,
						 (arg4 & 16) ? 20 : -1, 0, arg4 & 7, false);
}

FUNC(LS_Generic_Crusher)
// Generic_Crusher (tag, dnspeed, upspeed, silent, damage)
{
	return EV_DoCeiling (GetInstigator(it, isFromAcs), DCeiling::ceilCrushAndRaise, ln, arg0, SPEED(arg1),
						 SPEED(arg2), 0, arg4, arg3 ? 2 : 0, 0, false);
}

FUNC(LS_Generic_Crusher2)
// Generic_Crusher2 (tag, dnspeed, upspeed, silent, damage)
{
	// same as above but uses Hexen's crushing method.
	return EV_DoCeiling (GetInstigator(it, isFromAcs), DCeiling::ceilCrushAndRaise, ln, arg0, SPEED(arg1),
						 SPEED(arg2), 0, arg4, arg3 ? 2 : 0, 0, true);
}

FUNC(LS_Plat_PerpetualRaise)
// Plat_PerpetualRaise (tag, speed, delay)
{
	return EV_DoPlat (GetInstigator(it, isFromAcs), arg0, ln, DPlat::platPerpetualRaise, 0, SPEED(arg1), TICS(arg2), 8, 0);
}

FUNC(LS_Plat_PerpetualRaiseLip)
// Plat_PerpetualRaiseLip (tag, speed, delay, lip)
{
	return EV_DoPlat (GetInstigator(it, isFromAcs), arg0, ln, DPlat::platPerpetualRaise, 0, SPEED(arg1), TICS(arg2), arg3, 0);
}

FUNC(LS_Plat_Stop)
// Plat_Stop (tag)
{
	EV_StopPlat (GetInstigator(it, isFromAcs), arg0);
	return true;
}

FUNC(LS_Plat_DownWaitUpStay)
// Plat_DownWaitUpStay (tag, speed, delay)
{
	return EV_DoPlat (GetInstigator(it, isFromAcs), arg0, ln, DPlat::platDownWaitUpStay, 0, SPEED(arg1), TICS(arg2), 8, 0);
}

FUNC(LS_Plat_DownWaitUpStayLip)
// Plat_DownWaitUpStayLip (tag, speed, delay, lip, floor-sound?)
{
	return EV_DoPlat (GetInstigator(it, isFromAcs), arg0, ln,
		arg4 ? DPlat::platDownWaitUpStayStone : DPlat::platDownWaitUpStay,
		0, SPEED(arg1), TICS(arg2), arg3, 0);
}

FUNC(LS_Plat_DownByValue)
// Plat_DownByValue (tag, speed, delay, height)
{
	return EV_DoPlat (GetInstigator(it, isFromAcs), arg0, ln, DPlat::platDownByValue, FRACUNIT*arg3*8, SPEED(arg1), TICS(arg2), 0, 0);
}

FUNC(LS_Plat_UpByValue)
// Plat_UpByValue (tag, speed, delay, height)
{
	return EV_DoPlat (GetInstigator(it, isFromAcs), arg0, ln, DPlat::platUpByValue, FRACUNIT*arg3*8, SPEED(arg1), TICS(arg2), 0, 0);
}

FUNC(LS_Plat_UpWaitDownStay)
// Plat_UpWaitDownStay (tag, speed, delay)
{
	return EV_DoPlat (GetInstigator(it, isFromAcs), arg0, ln, DPlat::platUpWaitDownStay, 0, SPEED(arg1), TICS(arg2), 0, 0);
}

FUNC(LS_Plat_UpNearestWaitDownStay)
// Plat_UpNearestWaitDownStay (tag, speed, delay)
{
	return EV_DoPlat (GetInstigator(it, isFromAcs), arg0, ln, DPlat::platUpNearestWaitDownStay, 0, SPEED(arg1), TICS(arg2), 0, 0);
}

FUNC(LS_Plat_RaiseAndStayTx0)
// Plat_RaiseAndStayTx0 (tag, speed, lockout)
{
	DPlat::EPlatType type;

	switch (arg3)
	{
		case 1:
			type = DPlat::platRaiseAndStay;
			break;
		case 2:
			type = DPlat::platRaiseAndStayLockout;
			break;
		default:
			type = gameinfo.gametype == GAME_Heretic? DPlat::platRaiseAndStayLockout : DPlat::platRaiseAndStay;
			break;
	}


	return EV_DoPlat (GetInstigator(it, isFromAcs), arg0, ln, type, 0, SPEED(arg1), 0, 0, 1);
}

FUNC(LS_Plat_UpByValueStayTx)
// Plat_UpByValueStayTx (tag, speed, height)
{
	return EV_DoPlat (GetInstigator(it, isFromAcs), arg0, ln, DPlat::platUpByValueStay, FRACUNIT*arg2*8, SPEED(arg1), 0, 0, 2);
}

FUNC(LS_Plat_ToggleCeiling)
// Plat_ToggleCeiling (tag)
{
	return EV_DoPlat (GetInstigator(it, isFromAcs), arg0, ln, DPlat::platToggle, 0, 0, 0, 0, 0);
}

FUNC(LS_Generic_Lift)
// Generic_Lift (tag, speed, delay, target, height)
{
	DPlat::EPlatType type;

	switch (arg3)
	{
		case 1:
			type = DPlat::platDownWaitUpStay;
			break;
		case 2:
			type = DPlat::platDownToNearestFloor;
			break;
		case 3:
			type = DPlat::platDownToLowestCeiling;
			break;
		case 4:
			type = DPlat::platPerpetualRaise;
			break;
		default:
			type = DPlat::platUpByValue;
			break;
	}

	return EV_DoPlat (GetInstigator(it, isFromAcs), arg0, ln, type, arg4*8*FRACUNIT, SPEED(arg1), OCTICS(arg2), 0, 0);
}

FUNC(LS_Exit_Normal)
// Exit_Normal (position)
{
	if (CheckIfExitIsGood (it, FindLevelInfo(G_GetExitMap())))
	{
		G_ExitLevel (arg0, false);
		return true;
	}
	return false;
}

FUNC(LS_Exit_Secret)
// Exit_Secret (position)
{
	if (CheckIfExitIsGood (it, FindLevelInfo(G_GetSecretExitMap())))
	{
		G_SecretExitLevel (arg0);
		return true;
	}
	return false;
}

FUNC(LS_Teleport_NewMap)
// Teleport_NewMap (map, position, keepFacing?)
{
	if (backSide == 0 || gameinfo.gametype == GAME_Strife)
	{
		level_info_t *info = FindLevelByNum (arg0);

		if (info && CheckIfExitIsGood (it, info))
		{
			G_ChangeLevel(info->mapname, arg1, arg2 ? CHANGELEVEL_KEEPFACING : 0);
			return true;
		}
	}
	return false;
}

FUNC(LS_Teleport)
// Teleport (tid, sectortag, bNoSourceFog)
{
	return EV_Teleport (GetInstigator(it, isFromAcs), arg0, arg1, ln, backSide, it, true, !arg2, false);
}

FUNC( LS_Teleport_NoStop )
// Teleport_NoStop (tid, sectortag, bNoSourceFog)
{
	return EV_Teleport(GetInstigator(it, isFromAcs), arg0, arg1, ln, backSide, it, true, !arg2, false, false);
}

FUNC(LS_Teleport_NoFog)
// Teleport_NoFog (tid, useang, sectortag, keepheight)
{
	return EV_Teleport (GetInstigator(it, isFromAcs), arg0, arg2, ln, backSide, it, false, false, !arg1, true, !!arg3);
}

FUNC(LS_Teleport_ZombieChanger)
// Teleport_ZombieChanger (tid, sectortag)
{
	// This is practically useless outside of Strife, but oh well.
	if (it != NULL)
	{
		EV_Teleport (GetInstigator(it, isFromAcs), arg0, arg1, ln, backSide, it, NULL, false, false, false);

		// [BC] If we're the server, tell clients to put this thing in its pain state.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingState( it, STATE_PAIN );

		if (it->health >= 0) it->SetState (it->FindState(NAME_Pain));
		return true;
	}
	return false;
}

FUNC(LS_TeleportOther)
// TeleportOther (other_tid, dest_tid, fog?)
{
	return EV_TeleportOther (arg0, arg1, arg2?true:false);
}

FUNC(LS_TeleportGroup)
// TeleportGroup (group_tid, source_tid, dest_tid, move_source?, fog?)
{
	return EV_TeleportGroup (arg0, it, arg1, arg2, arg3?true:false, arg4?true:false);
}

FUNC(LS_TeleportInSector)
// TeleportInSector (tag, source_tid, dest_tid, bFog, group_tid)
{
	return EV_TeleportSector (arg0, arg1, arg2, arg3?true:false, arg4);
}

FUNC(LS_Teleport_EndGame)
// Teleport_EndGame ()
{
	if (!backSide && CheckIfExitIsGood (it, NULL))
	{
		G_ChangeLevel(NULL, 0, 0);
		return true;
	}
	return false;
}

FUNC(LS_Teleport_Line)
// Teleport_Line (thisid, destid, reversed)
{
	return EV_SilentLineTeleport (ln, backSide, it, arg1, arg2);
}

static void ThrustThingHelper (AActor *it, angle_t angle, int force, INTBOOL nolimit);
FUNC(LS_ThrustThing)
// ThrustThing (angle, force, nolimit, tid)
{
	if (arg3 != 0)
	{
		FActorIterator iterator (arg3);
		while ((it = iterator.Next()) != NULL)
		{
			ThrustThingHelper (it, BYTEANGLE(arg0), arg1, arg2);
		}
		return true;
	}
	else if (it)
	{
		ThrustThingHelper (it, BYTEANGLE(arg0), arg1, arg2);
		return true;
	}
	return false;
}

static void ThrustThingHelper (AActor *it, angle_t angle, int force, INTBOOL nolimit)
{
	angle >>= ANGLETOFINESHIFT;
	it->velx += force * finecosine[angle];
	it->vely += force * finesine[angle];
	if (!nolimit)
	{
		it->velx = clamp<fixed_t> (it->velx, -MAXMOVE, MAXMOVE);
		it->vely = clamp<fixed_t> (it->vely, -MAXMOVE, MAXMOVE);
	}

	// [BC] If we're the server, update the thing's velocity.
	// [Dusk] Use SERVER_UpdateThingVelocity
	if (NETWORK_GetState() == NETSTATE_SERVER)
		SERVER_UpdateThingVelocity( it, false );
}

FUNC(LS_ThrustThingZ)	// [BC]
// ThrustThingZ (tid, zthrust, down/up, set)
{
	AActor *victim;
	fixed_t thrust = arg1*FRACUNIT/4;

	// [BC] Up is default
	if (arg2)
		thrust = -thrust;

	if (arg0 != 0)
	{
		FActorIterator iterator (arg0);

		while ( (victim = iterator.Next ()) )
		{
			if (!arg3)
				victim->velz = thrust;
			else
				victim->velz += thrust;

			if (victim->player && victim->velz > 0)
			{
				victim->player->mo->wasJustThrustedZ = true;
			}

			// [BC] If we're the server, update the thing's velocity.
			// [BB] Unfortunately there are sync issues, if we don't also update the actual position.
			// Is there a way to fix this without sending the position?
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_MoveThing( victim, CM_Z|CM_VELZ );
		}
		return true;
	}
	else if (it)
	{
		if (!arg3)
			it->velz = thrust;
		else
			it->velz += thrust;

		if (it->player && it->velz > 0)
		{
			it->player->mo->wasJustThrustedZ = true;
		}

		// [BC] If we're the server, update the thing's velocity.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVER_UpdateThingVelocity ( it, true, false );

		return true;
	}
	return false;
}

FUNC(LS_Thing_SetSpecial)	// [BC]
// Thing_SetSpecial (tid, special, arg1, arg2, arg3)
// [RH] Use the SetThingSpecial ACS command instead.
// It can set all args and not just the first three.
{
	if (arg0 == 0)
	{
		if (it != NULL)
		{
			it->special = arg1;
			it->args[0] = arg2;
			it->args[1] = arg3;
			it->args[2] = arg4;
		}
	}
	else
	{
		AActor *actor;
		FActorIterator iterator (arg0);

		while ( (actor = iterator.Next ()) )
		{
			actor->special = arg1;
			actor->args[0] = arg2;
			actor->args[1] = arg3;
			actor->args[2] = arg4;
		}
	}
	return true;
}

FUNC(LS_Thing_ChangeTID)
// Thing_ChangeTID (oldtid, newtid)
{
	if (arg0 == 0)
	{
		if (it != NULL && !(it->ObjectFlags & OF_EuthanizeMe))
		{
			it->RemoveFromHash ();
			it->tid = arg1;
			it->AddToHash ();

			// [BB] Notify the clients about the TID change.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetThingTID( it );
		}
	}
	else
	{
		FActorIterator iterator (arg0);
		AActor *actor, *next;

		next = iterator.Next ();
		while (next != NULL)
		{
			actor = next;
			next = iterator.Next ();

			if (!(actor->ObjectFlags & OF_EuthanizeMe))
			{
				actor->RemoveFromHash ();
				actor->tid = arg1;
				actor->AddToHash ();

				// [BB] Notify the clients about the TID change.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SetThingTID( actor );
			}
		}
	}
	return true;
}

FUNC(LS_DamageThing)
// DamageThing (damage, mod)
{
	if (it)
	{
		if (arg0 < 0)
		{ // Negative damages mean healing
			if (it->player)
			{
				P_GiveBody (it, -arg0);
			}
			else
			{
				it->health -= arg0;
				if (it->SpawnHealth() < it->health)
					it->health = it->SpawnHealth();
			}
		}
		else if (arg0 > 0)
		{
			P_DamageMobj (it, NULL, NULL, arg0, MODtoDamageType (arg1));
		}
		else
		{ // If zero damage, guarantee a kill
			P_DamageMobj (it, NULL, NULL, TELEFRAG_DAMAGE, MODtoDamageType (arg1));
		}
	}

	return it ? true : false;
}

FUNC(LS_HealThing)
// HealThing (amount, max)
{
	if (it)
	{
		int max = arg1;

		if (max == 0 || it->player == NULL)
		{
			// [BC] If it's a regular player, we can still apply the player's max. health
			// bonus to his maximum allowed health.
			if ( it->player )
			{
				// [BC] Apply the prosperity power.
				if ( it->player->cheats & CF_PROSPERITY )
					max = deh.MaxSoulsphere + 50;
				else
					max = it->GetDefault()->health + it->player->lMaxHealthBonus;
			}
			else
			{
				// [BB] Check if P_GiveBody is correct here (ported from ZDoom).
				P_GiveBody(it, arg0);
				return true;
			}
		}
		else if (max == 1)
		{
			// [BC] Include the player's max. health bonus in the max. allowed health.
			max = deh.MaxSoulsphere + it->player->lMaxHealthBonus;
		}

		// If health is already above max, do nothing
		if (it->health < max)
		{
			it->health += arg0;
			if (it->health > max && max > 0)
			{
				it->health = max;
			}
			if (it->player)
			{
				it->player->health = it->health;

				// [BC] If we're the server, send out the health change.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SetPlayerHealth( ULONG( it->player - players ));
			}
		}
	}

	return it ? true : false;
}

// So that things activated/deactivated by ACS or DECORATE *and* by 
// the BUMPSPECIAL or USESPECIAL flags work correctly both ways.
void DoActivateThing(AActor * thing, AActor * activator)
{
	if (thing->activationtype & THINGSPEC_Activate)
	{
		thing->activationtype &= ~THINGSPEC_Activate; // Clear flag
		if (thing->activationtype & THINGSPEC_Switch) // Set other flag if switching
			thing->activationtype |= THINGSPEC_Deactivate;
	}
	thing->Activate (activator);
}

void DoDeactivateThing(AActor * thing, AActor * activator)
{
	if (thing->activationtype & THINGSPEC_Deactivate)
	{
		thing->activationtype &= ~THINGSPEC_Deactivate; // Clear flag
		if (thing->activationtype & THINGSPEC_Switch) // Set other flag if switching
			thing->activationtype |= THINGSPEC_Activate;
	}
	thing->Deactivate (activator);
}

FUNC(LS_Thing_Activate)
// Thing_Activate (tid)
{
	if (arg0 != 0)
	{
		AActor *actor;
		FActorIterator iterator (arg0);
		int count = 0;

		actor = iterator.Next ();
		while (actor)
		{
			// Actor might remove itself as part of activation, so get next
			// one before activating it.
			AActor *temp = iterator.Next ();

			// [BC] Tell clients to activate the thing.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_ThingActivate( actor, it );

			DoActivateThing(actor, it);
			actor = temp;
			count++;
		}

		return count != 0;
	}
	else if (it != NULL)
	{
		// [BC] Tell clients to activate the thing.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_ThingActivate( it, it );

		DoActivateThing(it, it);
		return true;
	}
	return false;
}

FUNC(LS_Thing_Deactivate)
// Thing_Deactivate (tid)
{
	if (arg0 != 0)
	{
		AActor *actor;
		FActorIterator iterator (arg0);
		int count = 0;
	
		actor = iterator.Next ();
		while (actor)
		{
			// Actor might removes itself as part of deactivation, so get next
			// one before we activate it.
			AActor *temp = iterator.Next ();

			// [BC] Tell clients to deactivate the thing.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_ThingDeactivate( actor, it );

			DoDeactivateThing(actor, it);
			actor = temp;
			count++;
		}
	
		return count != 0;
	}
	else if (it != NULL)
	{
		// [BC] Tell clients to deactivate the thing.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_ThingDeactivate( it, it );

		DoDeactivateThing(it, it);
		return true;
	}
	return false;
}

FUNC(LS_Thing_Remove)
// Thing_Remove (tid)
{
	if (arg0 != 0)
	{
		FActorIterator iterator (arg0);
		AActor *actor;

		actor = iterator.Next ();
		while (actor)
		{
			AActor *temp = iterator.Next ();

			P_RemoveThing(actor);
			actor = temp;
		}
	}
	else if (it != NULL)
	{
		P_RemoveThing(it);
	}

	return true;
}

FUNC(LS_Thing_Destroy)
// Thing_Destroy (tid, extreme, tag)
{
	AActor *actor;

	if (arg0 == 0 && arg2 == 0)
	{
		P_Massacre ();
	}
	else if (arg0 == 0)
	{
		TThinkerIterator<AActor> iterator;
		
		actor = iterator.Next ();
		while (actor)
		{
			AActor *temp = iterator.Next ();
			if (actor->flags & MF_SHOOTABLE && actor->Sector->tag == arg2)
				P_DamageMobj (actor, NULL, it, arg1 ? TELEFRAG_DAMAGE : actor->health, NAME_None);
			actor = temp;
		}
	}
	else
	{
		FActorIterator iterator (arg0);

		actor = iterator.Next ();
		while (actor)
		{
			AActor *temp = iterator.Next ();
			if (actor->flags & MF_SHOOTABLE && (arg2 == 0 || actor->Sector->tag == arg2))
				P_DamageMobj (actor, NULL, it, arg1 ? TELEFRAG_DAMAGE : actor->health, NAME_None);
			actor = temp;
		}
	}
	return true;
}

FUNC(LS_Thing_Damage)
// Thing_Damage (tid, amount, MOD)
{
	P_Thing_Damage (arg0, it, arg1, MODtoDamageType (arg2));
	return true;
}

FUNC(LS_Thing_Projectile)
// Thing_Projectile (tid, type, angle, speed, vspeed)
{
	return P_Thing_Projectile (arg0, it, arg1, NULL, BYTEANGLE(arg2), arg3<<(FRACBITS-3),
		arg4<<(FRACBITS-3), 0, NULL, 0, 0, false);
}

FUNC(LS_Thing_ProjectileGravity)
// Thing_ProjectileGravity (tid, type, angle, speed, vspeed)
{
	return P_Thing_Projectile (arg0, it, arg1, NULL, BYTEANGLE(arg2), arg3<<(FRACBITS-3),
		arg4<<(FRACBITS-3), 0, NULL, 1, 0, false);
}

FUNC(LS_Thing_Hate)
// Thing_Hate (hater, hatee, group/"xray"?)
{
	FActorIterator haterIt (arg0);
	AActor *hater, *hatee = NULL;
	FActorIterator hateeIt (arg1);
	bool nothingToHate = false;

	if (arg1 != 0)
	{
		while ((hatee = hateeIt.Next ()))
		{
			if (hatee->flags & MF_SHOOTABLE	&&	// can't hate nonshootable things
				hatee->health > 0 &&			// can't hate dead things
				!(hatee->flags2 & MF2_DORMANT))	// can't target dormant things
			{
				break;
			}
		}
		if (hatee == NULL)
		{ // Nothing to hate
			nothingToHate = true;
		}
	}

	if (arg0 == 0)
	{
		if (it != NULL && it->player != NULL)
		{
			// Players cannot have their attitudes set
			return false;
		}
		else
		{
			hater = it;
		}
	}
	else
	{
		while ((hater = haterIt.Next ()))
		{
			if (hater->health > 0 && hater->flags & MF_SHOOTABLE)
			{
				break;
			}
		}
	}
	while (hater != NULL)
	{
		// Can't hate if can't attack.
		if (hater->SeeState != NULL)
		{
			// If hating a group of things, record the TID and NULL
			// the target (if its TID doesn't match). A_Look will
			// find an appropriate thing to go chase after.
			if (arg2 != 0)
			{
				hater->TIDtoHate = arg1;
				hater->LastLookActor = NULL;

				// If the TID to hate is 0, then don't forget the target and
				// lastenemy fields.
				if (arg1 != 0)
				{
					if (hater->target != NULL && hater->target->tid != arg1)
					{
						hater->target = NULL;
					}
					if (hater->lastenemy != NULL && hater->lastenemy->tid != arg1)
					{
						hater->lastenemy = NULL;
					}
				}
			}
			// Hate types for arg2:
			//
			// 0 - Just hate one specific actor
			// 1 - Hate actors with given TID and attack players when shot
			// 2 - Same as 1, but will go after enemies without seeing them first
			// 3 - Hunt actors with given TID and also players
			// 4 - Same as 3, but will go after monsters without seeing them first
			// 5 - Hate actors with given TID and ignore player attacks
			// 6 - Same as 5, but will go after enemies without seeing them first

			// Note here: If you use Thing_Hate (tid, 0, 2), you can make
			// a monster go after a player without seeing him first.
			if (arg2 == 2 || arg2 == 4 || arg2 == 6)
			{
				hater->flags3 |= MF3_NOSIGHTCHECK;
			}
			else
			{
				hater->flags3 &= ~MF3_NOSIGHTCHECK;
			}
			if (arg2 == 3 || arg2 == 4)
			{
				hater->flags3 |= MF3_HUNTPLAYERS;
			}
			else
			{
				hater->flags3 &= ~MF3_HUNTPLAYERS;
			}
			if (arg2 == 5 || arg2 == 6)
			{
				hater->flags4 |= MF4_NOHATEPLAYERS;
			}
			else
			{
				hater->flags4 &= ~MF4_NOHATEPLAYERS;
			}

			if (arg1 == 0)
			{
				hatee = it;
			}
			else if (nothingToHate)
			{
				hatee = NULL;
			}
			else if (arg2 != 0)
			{
				do
				{
					hatee = hateeIt.Next ();
				}
				while ( hatee == NULL ||
						hatee == hater ||					// can't hate self
						!(hatee->flags & MF_SHOOTABLE) ||	// can't hate nonshootable things
						hatee->health <= 0 ||				// can't hate dead things
						(hatee->flags2 & MF2_DORMANT));	
			}

			if (hatee != NULL && hatee != hater && (arg2 == 0 || (hater->goal != NULL && hater->target != hater->goal)))
			{
				if (hater->target)
				{
					hater->lastenemy = hater->target;
				}
				hater->target = hatee;
				if (!(hater->flags2 & MF2_DORMANT))
				{
					if (hater->health > 0)
					{
						// [BB] If we're the server, tell clients to put the thing into its see state.
						if ( NETWORK_GetState( ) == NETSTATE_SERVER )
							SERVERCOMMANDS_SetThingState( hater, STATE_SEE );

						hater->SetState (hater->SeeState);
					}
				}
			}
		}
		if (arg0 != 0)
		{
			while ((hater = haterIt.Next ()))
			{
				if (hater->health > 0 && hater->flags & MF_SHOOTABLE)
				{
					break;
				}
			}
		}
		else
		{
			hater = NULL;
		}
	}
	return true;
}

FUNC(LS_Thing_ProjectileAimed)
// Thing_ProjectileAimed (tid, type, speed, target, newtid)
{
	return P_Thing_Projectile (arg0, it, arg1, NULL, 0, arg2<<(FRACBITS-3), 0, arg3, it, 0, arg4, false);
}

FUNC(LS_Thing_ProjectileIntercept)
// Thing_ProjectileIntercept (tid, type, speed, target, newtid)
{
	return P_Thing_Projectile (arg0, it, arg1, NULL, 0, arg2<<(FRACBITS-3), 0, arg3, it, 0, arg4, true);
}

// [BC] added newtid for next two
FUNC(LS_Thing_Spawn)
// Thing_Spawn (tid, type, angle, newtid)
{
	return P_Thing_Spawn (arg0, it, arg1, BYTEANGLE(arg2), true, arg3);
}

FUNC(LS_Thing_SpawnNoFog)
// Thing_SpawnNoFog (tid, type, angle, newtid)
{
	return P_Thing_Spawn (arg0, it, arg1, BYTEANGLE(arg2), false, arg3);
}

FUNC(LS_Thing_SpawnFacing)
// Thing_SpawnFacing (tid, type, nofog, newtid)
{
	return P_Thing_Spawn (arg0, it, arg1, ANGLE_MAX, arg2 ? false : true, arg3);
}

FUNC(LS_Thing_Raise)
// Thing_Raise(tid)
{
	AActor * target;
	bool ok = false;

	if (arg0==0)
	{
		ok = P_Thing_Raise (it);
	}
	else
	{
		TActorIterator<AActor> iterator (arg0);

		while ( (target = iterator.Next ()) )
		{
			ok |= P_Thing_Raise(target);
		}
	}
	return ok;
}

FUNC(LS_Thing_Stop)
// Thing_Stop(tid)
{
	AActor * target;
	bool ok = false;

	if (arg0==0)
	{
		if (it != NULL)
		{
			it->velx = it->vely = it->velz = 0;
			if (it->player != NULL) it->player->velx = it->player->vely = 0;

			// [Dusk] tell the clients about this
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_MoveThing( it, ( ( it->player == NULL ) ? (CM_X|CM_Y|CM_Z) : 0 )|CM_VELX|CM_VELY|CM_VELZ );

			ok = true;
		}
	}
	else
	{
		TActorIterator<AActor> iterator (arg0);

		while ( (target = iterator.Next ()) )
		{
			target->velx = target->vely = target->velz = 0;
			if (target->player != NULL) target->player->velx = target->player->vely = 0;

			// [Dusk] tell the clients about this
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_MoveThing( target, ( ( target->player == NULL ) ? (CM_X|CM_Y|CM_Z) : 0 )|CM_VELX|CM_VELY|CM_VELZ );

			ok = true;
		}
	}
	return ok;
}


FUNC(LS_Thing_SetGoal)
// Thing_SetGoal (tid, goal, delay, chasegoal)
{
	TActorIterator<AActor> selfiterator (arg0);
	NActorIterator goaliterator (NAME_PatrolPoint, arg1);
	AActor *self;
	AActor *goal = goaliterator.Next ();
	bool ok = false;

	while ( (self = selfiterator.Next ()) )
	{
		ok = true;
		if (self->flags & MF_SHOOTABLE)
		{
			if (self->target == self->goal)
			{ // Targeting a goal already? -> don't target it anymore.
			  // A_Look will set it to the goal, presuming no real targets
			  // come into view by then.
				self->target = NULL;
			}
			self->goal = goal;
			if (arg3 == 0)
			{
				self->flags5 &= ~MF5_CHASEGOAL;
			}
			else
			{
				self->flags5 |= MF5_CHASEGOAL;
			}
			if (self->target == NULL)
			{
				self->reactiontime = arg2 * TICRATE;
			}
		}
	}

	return ok;
}

FUNC(LS_Thing_Move)		// [BC]
// Thing_Move (tid, mapspot, nofog)
{
	return P_Thing_Move (arg0, it, arg1, arg2 ? false : true);
}

FUNC(LS_Thing_SetTranslation)
// Thing_SetTranslation (tid, range)
{
	TActorIterator<AActor> iterator (arg0);
	int range;
	AActor *target;
	bool ok = false;

	if (arg1 == -1 && it != NULL)
	{
		range = it->Translation;
	}
	else if (arg1 >= 1 && arg1 < MAX_ACS_TRANSLATIONS)
	{
		range = TRANSLATION(TRANSLATION_LevelScripted, (arg1-1));
	}
	else
	{
		range = 0;
	}

	if (arg0 == 0)
	{
		if (it != NULL)
		{
			ok = true;
			it->Translation = range==0? it->GetDefault()->Translation : range;

			// [BC] If we're the server, tell clients to set this thing's translation.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetThingTranslation( it );

			}
	}
	else
	{
		while ( (target = iterator.Next ()) )
		{
			ok = true;
			target->Translation = range==0? target->GetDefault()->Translation : range;

			// [BC] If we're the server, tell clients to set this thing's translation.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetThingTranslation( target );
			}

	}

	return ok;
}

FUNC(LS_ACS_Execute)
// ACS_Execute (script, map, s_arg1, s_arg2, s_arg3)
{
	level_info_t *info;
	const char *mapname = NULL;
	int args[3] = { arg2, arg3, arg4 };
	int flags = (backSide ? ACS_BACKSIDE : 0);

	// [BC] If this script is client side, just let clients execute it themselves.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) &&
		( ACS_IsScriptClientSide( arg0 )))
	{
		SERVERCOMMANDS_ACSScriptExecute( arg0, it, LONG( ln - lines ), arg1, backSide, args, 3, false );
		return ( true );
	}

	if (arg1 == 0)
	{
		mapname = level.mapname;
	}
	else if ((info = FindLevelByNum(arg1)) != NULL)
	{
		mapname = info->mapname;
	}
	else
	{
		return false;
	}
	return P_StartScript(it, ln, arg0, mapname, args, 3, flags);
}

FUNC(LS_ACS_ExecuteAlways)
// ACS_ExecuteAlways (script, map, s_arg1, s_arg2, s_arg3)
{
	level_info_t *info;
	const char *mapname = NULL;
	int args[3] = { arg2, arg3, arg4 };
	int flags = (backSide ? ACS_BACKSIDE : 0) | ACS_ALWAYS;

	// [BC] If this script is client side, just let clients execute it themselves.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) &&
		( ACS_IsScriptClientSide( arg0 )))
	{
		SERVERCOMMANDS_ACSScriptExecute( arg0, it, LONG( ln - lines ), arg1, backSide, args, 3, true );
		return ( true );
	}

	if (arg1 == 0)
	{
		mapname = level.mapname;
	}
	else if ((info = FindLevelByNum(arg1)) != NULL)
	{
		mapname = info->mapname;
	}
	else
	{
		return false;
	}
	return P_StartScript(it, ln, arg0, mapname, args, 3, flags);
}

FUNC(LS_ACS_LockedExecute)
// ACS_LockedExecute (script, map, s_arg1, s_arg2, lock)
{
	if (arg4 && !P_CheckKeys (it, arg4, true))
		return false;
	else
		return LS_ACS_Execute (ln, it, backSide, true, arg0, arg1, arg2, arg3, 0);
}

FUNC(LS_ACS_LockedExecuteDoor)
// ACS_LockedExecuteDoor (script, map, s_arg1, s_arg2, lock)
{
	if (arg4 && !P_CheckKeys (it, arg4, false))
		return false;
	else
		return LS_ACS_Execute (ln, it, backSide, true, arg0, arg1, arg2, arg3, 0);
}

FUNC(LS_ACS_ExecuteWithResult)
// ACS_ExecuteWithResult (script, s_arg1, s_arg2, s_arg3, s_arg4)
{
	// This is like ACS_ExecuteAlways, except the script is always run on
	// the current map, and the return value is whatever the script sets
	// with SetResultValue.
	int args[4] = { arg1, arg2, arg3, arg4 };
	int flags = (backSide ? ACS_BACKSIDE : 0) | ACS_ALWAYS | ACS_WANTRESULT;

	// [BC] If this script is client side, just let clients execute it themselves.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) &&
		( ACS_IsScriptClientSide( arg0 )))
	{
		SERVERCOMMANDS_ACSScriptExecute( arg0, it, LONG( ln - lines ), 0, backSide, args, 4, true );
		return ( false );
	}

	return P_StartScript (it, ln, arg0, level.mapname, args, 4, flags);
}

FUNC(LS_ACS_Suspend)
// ACS_Suspend (script, map)
{
	level_info_t *info;

	if (arg1 == 0)
		P_SuspendScript (arg0, level.mapname);
	else if ((info = FindLevelByNum (arg1)) )
		P_SuspendScript (arg0, info->mapname);

	return true;
}

FUNC(LS_ACS_Terminate)
// ACS_Terminate (script, map)
{
	level_info_t *info;

	if (arg1 == 0)
		P_TerminateScript (arg0, level.mapname);
	else if ((info = FindLevelByNum (arg1)) )
		P_TerminateScript (arg0, info->mapname);

	return true;
}

FUNC(LS_FloorAndCeiling_LowerByValue)
// FloorAndCeiling_LowerByValue (tag, speed, height)
{
	return EV_DoElevator (GetInstigator(it, isFromAcs), ln, DElevator::elevateLower, SPEED(arg1), arg2*FRACUNIT, arg0);
}

FUNC(LS_FloorAndCeiling_RaiseByValue)
// FloorAndCeiling_RaiseByValue (tag, speed, height)
{
	return EV_DoElevator (GetInstigator(it, isFromAcs), ln, DElevator::elevateRaise, SPEED(arg1), arg2*FRACUNIT, arg0);
}

FUNC(LS_FloorAndCeiling_LowerRaise)
// FloorAndCeiling_LowerRaise (tag, fspeed, cspeed, boomemu)
{
	bool res = EV_DoCeiling (GetInstigator(it, isFromAcs), DCeiling::ceilRaiseToHighest, ln, arg0, SPEED(arg2), 0, 0, 0, 0, 0, false);
	// The switch based Boom equivalents of FloorandCeiling_LowerRaise do incorrect checks
	// which cause the floor only to move when the ceiling fails to do so.
	// To avoid problems with maps that have incorrect args this only uses a 
	// more or less unintuitive value for the fourth arg to trigger Boom's broken behavior
	if (arg3 != 1998 || !res)	// (1998 for the year in which Boom was released... :P)
	{
		res |= EV_DoFloor (GetInstigator(it, isFromAcs), DFloor::floorLowerToLowest, ln, arg0, SPEED(arg1), 0, 0, 0, false);
	}
	return res;
}

FUNC(LS_Elevator_MoveToFloor)
// Elevator_MoveToFloor (tag, speed)
{
	return EV_DoElevator (GetInstigator(it, isFromAcs), ln, DElevator::elevateCurrent, SPEED(arg1), 0, arg0);
}

FUNC(LS_Elevator_RaiseToNearest)
// Elevator_RaiseToNearest (tag, speed)
{
	return EV_DoElevator (GetInstigator(it, isFromAcs), ln, DElevator::elevateUp, SPEED(arg1), 0, arg0);
}

FUNC(LS_Elevator_LowerToNearest)
// Elevator_LowerToNearest (tag, speed)
{
	return EV_DoElevator (GetInstigator(it, isFromAcs), ln, DElevator::elevateDown, SPEED(arg1), 0, arg0);
}

FUNC(LS_Light_ForceLightning)
// Light_ForceLightning (mode)
{
	P_ForceLightning (arg0);
	return true;
}

FUNC(LS_Light_RaiseByValue)
// Light_RaiseByValue (tag, value)
{
	EV_LightChange (arg0, arg1);
	return true;
}

FUNC(LS_Light_LowerByValue)
// Light_LowerByValue (tag, value)
{
	EV_LightChange (arg0, -arg1);
	return true;
}

FUNC(LS_Light_ChangeToValue)
// Light_ChangeToValue (tag, value)
{
	EV_LightTurnOn (arg0, arg1);
	return true;
}

FUNC(LS_Light_Fade)
// Light_Fade (tag, value, tics);
{
	EV_StartLightFading (arg0, arg1, TICS(arg2));
	return true;
}

FUNC(LS_Light_Glow)
// Light_Glow (tag, upper, lower, tics)
{
	EV_StartLightGlowing (arg0, arg1, arg2, TICS(arg3));
	return true;
}

FUNC(LS_Light_Flicker)
// Light_Flicker (tag, upper, lower)
{
	EV_StartLightFlickering (arg0, arg1, arg2);
	return true;
}

FUNC(LS_Light_Strobe)
// Light_Strobe (tag, upper, lower, u-tics, l-tics)
{
	EV_StartLightStrobing (arg0, arg1, arg2, TICS(arg3), TICS(arg4));
	return true;
}

FUNC(LS_Light_StrobeDoom)
// Light_StrobeDoom (tag, u-tics, l-tics)
{
	EV_StartLightStrobing (arg0, TICS(arg1), TICS(arg2));
	return true;
}

FUNC(LS_Light_MinNeighbor)
// Light_MinNeighbor (tag)
{
	EV_TurnTagLightsOff (arg0);
	return true;
}

FUNC(LS_Light_MaxNeighbor)
// Light_MaxNeighbor (tag)
{
	EV_LightTurnOn (arg0, -1);
	return true;
}

FUNC(LS_Light_Stop)
// Light_Stop (tag)
{
	EV_StopLightEffect (arg0);
	return true;
}

FUNC(LS_Radius_Quake)
// Radius_Quake (intensity, duration, damrad, tremrad, tid)
{
	return P_StartQuake (it, arg4, arg0, arg1, arg2*64, arg3*64, "world/quake");
}

FUNC(LS_UsePuzzleItem)
// UsePuzzleItem (item, script)
{
	AInventory *item;

	if (!it) return false;

	// Check player's inventory for puzzle item
	for (item = it->Inventory; item != NULL; item = item->Inventory)
	{
		if (item->IsKindOf (RUNTIME_CLASS(APuzzleItem)))
		{
			if (static_cast<APuzzleItem*>(item)->PuzzleItemNumber == arg0)
			{
				if (it->UseInventory (item))
				{
					return true;
				}
				break;
			}
		}
	}

	// [RH] Say "hmm" if you don't have the puzzle item
	S_Sound (it, CHAN_VOICE, "*puzzfail", 1, ATTN_IDLE);
	return false;
}

FUNC(LS_Sector_ChangeSound)
// Sector_ChangeSound (tag, sound)
{
	int secNum;
	bool rtn;

	if (!arg0)
		return false;

	secNum = -1;
	rtn = false;
	while ((secNum = P_FindSectorFromTag (arg0,	secNum)) >= 0)
	{
		sectors[secNum].seqType = arg1;
		rtn = true;
	}
	return rtn;
}

FUNC(LS_Sector_ChangeFlags)
// Sector_ChangeFlags (tag, set, clear)
{
	int secNum;
	bool rtn;

	if (!arg0)
		return false;

	secNum = -1;
	rtn = false;
	while ((secNum = P_FindSectorFromTag (arg0,	secNum)) >= 0)
	{
		sectors[secNum].Flags = (sectors[secNum].Flags | arg1) & ~arg2;
		rtn = true;
	}
	return rtn;
}

// [BC] Start of new Skulltag linespecials.

FUNC( LS_Player_SetTeam )
// Player_SetTeam( team id, bool nobroadcast )
{
	// Don't set teams on the client end.
	if ( NETWORK_InClientMode() )
		return ( false );

	// Break if we don't have a player.
	if ( !it || !it->player )
		return ( false );

	// Verify that we're setting a valid team.
	if ( TEAM_CheckIfValid( arg0 ) == false )
		I_Error( "Tried to set player to bad team, %d\n", arg0 );

	// Set the player's team.
	PLAYER_SetTeam( it->player, arg0, !!arg1 );
	return ( true );
}

void SERVERCONSOLE_UpdatePlayerInfo( LONG lPlayer, ULONG ulUpdateFlags );
void SERVERCONSOLE_UpdateScoreboard( void );
FUNC( LS_Team_Score )
// Team_Score (int howmuch, bool nogrin)
{
	// Scoring is not client side.
	if ( NETWORK_InClientMode() )
		return ( false );

	// Nothing to do if we're not in teamgame mode.
	if ( teamgame == false )
		return ( false );

	// Make sure a valid player is doing the scoring.
	if ( !it || !it->player || it->player->bOnTeam == false )
		return ( false );

	TEAM_SetScore( it->player->ulTeam, TEAM_GetScore( it->player->ulTeam ) + arg0, true );
	PLAYER_SetPoints ( it->player, it->player->lPointCount + arg0 );

	return ( false );
}

FUNC( LS_Team_GivePoints )
// Team_GivePoints( int iTeam, int iHowMuch, bool bAnnounce )
{
	// Scoring is not client side.
	if ( NETWORK_InClientMode() )
		return ( false );

	// Nothing to do if we're not in teamgame mode.
	if ( teamgame == false )
		return ( false );

	// Make sure this is a valid team.
	if ( TEAM_CheckIfValid( arg0 ) == false )
		return ( false );

	// Give the point(s) to the team.
	TEAM_SetScore( arg0, TEAM_GetScore( arg0 ) + arg1, !!arg2 );

	if ( it && it->player && it->player->bOnTeam )
	{
		PLAYER_SetPoints ( it->player, it->player->lPointCount + arg1 );
	}

	return ( false );
}

// [BC] End of new Skulltag linespecials.

struct FThinkerCollection
{
	int RefNum;
	DThinker *Obj;
};

static TArray<FThinkerCollection> Collection;

void AdjustPusher (int tag, int magnitude, int angle, DPusher::EPusher type)
{
	// Find pushers already attached to the sector, and change their parameters.
	{
		TThinkerIterator<DPusher> iterator;
		FThinkerCollection collect;

		while ( (collect.Obj = iterator.Next ()) )
		{
			if ((collect.RefNum = ((DPusher *)collect.Obj)->CheckForSectorMatch (type, tag)) >= 0)
			{
				((DPusher *)collect.Obj)->ChangeValues (magnitude, angle);
				Collection.Push (collect);
			}
		}
	}

	size_t numcollected = Collection.Size ();
	int secnum = -1;

	// Now create pushers for any sectors that don't already have them.
	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0)
	{
		unsigned int i;
		for (i = 0; i < numcollected; i++)
		{
			if (Collection[i].RefNum == sectors[secnum].sectornum)
				break;
		}
		if (i == numcollected)
		{
			new DPusher (type, NULL, magnitude, angle, NULL, secnum);
		}
	}
	Collection.Clear ();

	// [BB] The server needs to tell the clients to do the same.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_AdjustPusher( tag, magnitude, angle, type );
}

FUNC(LS_Sector_SetWind)
// Sector_SetWind (tag, amount, angle)
{
	if (arg3)
		return false;

	AdjustPusher (arg0, arg1, arg2, DPusher::p_wind);
	return true;
}

FUNC(LS_Sector_SetCurrent)
// Sector_SetCurrent (tag, amount, angle)
{
	if (arg3)
		return false;

	AdjustPusher (arg0, arg1, arg2, DPusher::p_current);
	return true;
}

FUNC(LS_Sector_SetFriction)
// Sector_SetFriction (tag, amount)
{
	P_SetSectorFriction (arg0, arg1, true);
	return true;
}

FUNC(LS_Sector_SetTranslucent)
// Sector_SetTranslucent (tag, plane, amount, type)
{
	if (arg0 != 0)
	{
		int secnum = -1;

		while ((secnum = P_FindSectorFromTag (arg0, secnum)) >= 0)
		{
			sectors[secnum].SetAlpha(arg1, Scale(arg2, OPAQUE, 255));
			sectors[secnum].ChangeFlags(arg1, ~PLANEF_ADDITIVE, arg3? PLANEF_ADDITIVE:0);
		}
		return true;
	}
	return false;
}

FUNC(LS_Sector_SetLink)
// Sector_SetLink (controltag, linktag, floor/ceiling, movetype)
{
	if (arg0 != 0)	// control tag == 0 is for static initialization and must not be handled here
	{
		int control = P_FindSectorFromTag(arg0, -1);
		if (control >= 0)
		{
			// [BB] Inform the clients and remember the link to inform future clients.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
				SERVER_AddSectorLink( control, arg1, arg2, arg3 );
				SERVERCOMMANDS_SetSectorLink( control, arg1, arg2, arg3 );
			}

			return P_AddSectorLinks(&sectors[control], arg1, arg2, arg3);
		}
	}
	return false;
}


/*static*/ void SetWallScroller (int id, int sidechoice, fixed_t dx, fixed_t dy, int Where)
{
	// [BB] The server has to tell the clients to call SetWallScroller.
	if( NETWORK_GetState() == NETSTATE_SERVER )
		SERVERCOMMANDS_SetWallScroller( id, sidechoice, dx, dy, Where );

	Where &=7;
	if (Where == 0) return;

	if ((dx | dy) == 0)
	{
		// Special case: Remove the scroller, because the deltas are both 0.
		TThinkerIterator<DScroller> iterator (STAT_SCROLLER);
		DScroller *scroller;

		while ( (scroller = iterator.Next ()) )
		{
			int wallnum = scroller->GetWallNum ();

			if (wallnum >= 0 && sides[wallnum].linedef->id == id &&
				int(sides[wallnum].linedef->sidedef[sidechoice] - sides) == wallnum &&
				Where == scroller->GetScrollParts())
			{
				scroller->Destroy ();
			}
		}
	}
	else
	{
		// Find scrollers already attached to the matching walls, and change
		// their rates.
		{
			TThinkerIterator<DScroller> iterator (STAT_SCROLLER);
			FThinkerCollection collect;

			while ( (collect.Obj = iterator.Next ()) )
			{
				if ((collect.RefNum = ((DScroller *)collect.Obj)->GetWallNum ()) != -1 &&
					sides[collect.RefNum].linedef->id == id &&
					int(sides[collect.RefNum].linedef->sidedef[sidechoice] - sides) == collect.RefNum &&
					Where == ((DScroller *)collect.Obj)->GetScrollParts())
				{
					((DScroller *)collect.Obj)->SetRate (dx, dy);
					Collection.Push (collect);
				}
			}
		}

		size_t numcollected = Collection.Size ();
		int linenum = -1;

		// Now create scrollers for any walls that don't already have them.
		while ((linenum = P_FindLineFromID (id, linenum)) >= 0)
		{
			if (lines[linenum].sidedef[sidechoice] != NULL)
			{
				int sidenum = int(lines[linenum].sidedef[sidechoice] - sides);
				unsigned int i;
				for (i = 0; i < numcollected; i++)
				{
					if (Collection[i].RefNum == sidenum)
						break;
				}
				if (i == numcollected)
				{
					new DScroller (DScroller::sc_side, dx, dy, -1, sidenum, 0, Where);
				}
			}
		}
		Collection.Clear ();
	}
}

FUNC(LS_Scroll_Texture_Both)
// Scroll_Texture_Both (id, left, right, up, down)
{
	if (arg0 == 0)
		return false;

	fixed_t dx = (arg1 - arg2) * (FRACUNIT/64);
	fixed_t dy = (arg4 - arg3) * (FRACUNIT/64);
	int sidechoice;

	if (arg0 < 0)
	{
		sidechoice = 1;
		arg0 = -arg0;
	}
	else
	{
		sidechoice = 0;
	}

	SetWallScroller (arg0, sidechoice, dx, dy, 7);

	return true;
}

FUNC(LS_Scroll_Wall)
// Scroll_Wall (id, x, y, side, flags)
{
	if (arg0 == 0)
		return false;

	SetWallScroller (arg0, !!arg3, arg1, arg2, arg4);
	return true;
}

/*static*/ void SetScroller (int tag, DScroller::EScrollType type, fixed_t dx, fixed_t dy)
{
	// [BB] The server has to tell the clients to call SetScroller.
	if( NETWORK_GetState() == NETSTATE_SERVER )
		SERVERCOMMANDS_SetScroller(type, dx, dy, tag);

	TThinkerIterator<DScroller> iterator (STAT_SCROLLER);
	DScroller *scroller;
	int i;

	// Check if there is already a scroller for this tag
	// If at least one sector with this tag is scrolling, then they all are.
	// If the deltas are both 0, we don't remove the scroller, because a
	// displacement/accelerative scroller might have been set up, and there's
	// no way to create one after the level is fully loaded.
	i = 0;
	while ( (scroller = iterator.Next ()) )
	{
		if (scroller->IsType (type))
		{
			if (sectors[scroller->GetAffectee ()].tag == tag)
			{
				i++;
				scroller->SetRate (dx, dy);
			}
		}
	}

	if (i > 0 || (dx|dy) == 0)
	{
		return;
	}

	// Need to create scrollers for the sector(s)
	for (i = -1; (i = P_FindSectorFromTag (tag, i)) >= 0; )
	{
		new DScroller (type, dx, dy, -1, i, 0);
	}
}

// NOTE: For the next two functions, x-move and y-move are
// 0-based, not 128-based as they are if they appear on lines.
// Note also that parameter ordering is different.

FUNC(LS_Scroll_Floor)
// Scroll_Floor (tag, x-move, y-move, s/c)
{
	fixed_t dx = arg1 * FRACUNIT/32;
	fixed_t dy = arg2 * FRACUNIT/32;

	if (arg3 == 0 || arg3 == 2)
	{
		SetScroller (arg0, DScroller::sc_floor, -dx, dy);
	}
	else
	{
		SetScroller (arg0, DScroller::sc_floor, 0, 0);
	}
	if (arg3 > 0)
	{
		SetScroller (arg0, DScroller::sc_carry, dx, dy);
	}
	else
	{
		SetScroller (arg0, DScroller::sc_carry, 0, 0);
	}
	return true;
}

FUNC(LS_Scroll_Ceiling)
// Scroll_Ceiling (tag, x-move, y-move, 0)
{
	fixed_t dx = arg1 * FRACUNIT/32;
	fixed_t dy = arg2 * FRACUNIT/32;

	SetScroller (arg0, DScroller::sc_ceiling, -dx, dy);
	return true;
}

FUNC(LS_PointPush_SetForce)
// PointPush_SetForce ()
{
	return false;
}

FUNC(LS_Sector_SetDamage)
// Sector_SetDamage (tag, amount, mod)
{
	// The sector still stores the mod in its old format because
	// adding an FName to the sector_t structure might cause
	// problems by adding an unwanted constructor.
	// Since it doesn't really matter whether the type is translated
	// here or in P_PlayerInSpecialSector I think it's the best solution.
	int secnum = -1;
	while ((secnum = P_FindSectorFromTag (arg0, secnum)) >= 0) {
		sectors[secnum].damage = arg1;
		sectors[secnum].mod = arg2;
	}
	return true;
}

FUNC(LS_Sector_SetGravity)
// Sector_SetGravity (tag, intpart, fracpart)
{
	int secnum = -1;
	float gravity;

	if (arg2 > 99)
		arg2 = 99;
	gravity = (float)arg1 + (float)arg2 * 0.01f;

	while ((secnum = P_FindSectorFromTag (arg0, secnum)) >= 0)
	{
		sectors[secnum].gravity = gravity;

		// [BC] If we're the server, tell clients that this sector's gravity is being altered.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetSectorGravity( secnum );
	}

	return true;
}

FUNC(LS_Sector_SetColor)
// Sector_SetColor (tag, r, g, b, desaturate)
{
	int secnum = -1;
	
	while ((secnum = P_FindSectorFromTag (arg0, secnum)) >= 0)
	{
		// [BB] Don't update the clients for each sector separately.
		sectors[secnum].SetColor(arg1, arg2, arg3, arg4, false);
	}
	// [BB] Tell clients to set the color for all sectors with tag arg0.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetSectorColorByTag( arg0, arg1, arg2, arg3, arg4 );

	return true;
}

FUNC(LS_Sector_SetFade)
// Sector_SetFade (tag, r, g, b)
{
	int secnum = -1;

	while ((secnum = P_FindSectorFromTag (arg0, secnum)) >= 0)
	{
		// [BB] Don't update the clients for each sector separately.
		sectors[secnum].SetFade(arg1, arg2, arg3, false);
	}
	// [BB] Tell clients to set the fade for all sectors with tag arg0.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetSectorFadeByTag( arg0, arg1, arg2, arg3 );

	return true;
}

FUNC(LS_Sector_SetCeilingPanning)
// Sector_SetCeilingPanning (tag, x-int, x-frac, y-int, y-frac)
{
	int secnum = -1;
	fixed_t xofs = arg1 * FRACUNIT + arg2 * (FRACUNIT/100);
	fixed_t yofs = arg3 * FRACUNIT + arg4 * (FRACUNIT/100);

	while ((secnum = P_FindSectorFromTag (arg0, secnum)) >= 0)
	{
		sectors[secnum].SetXOffset(sector_t::ceiling, xofs);
		sectors[secnum].SetYOffset(sector_t::ceiling, yofs);

		// Tell clients about the floor panning update.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetSectorPanning( secnum );
	}
	return true;
}

FUNC(LS_Sector_SetFloorPanning)
// Sector_SetFloorPanning (tag, x-int, x-frac, y-int, y-frac)
{
	int secnum = -1;
	fixed_t xofs = arg1 * FRACUNIT + arg2 * (FRACUNIT/100);
	fixed_t yofs = arg3 * FRACUNIT + arg4 * (FRACUNIT/100);

	while ((secnum = P_FindSectorFromTag (arg0, secnum)) >= 0)
	{
		sectors[secnum].SetXOffset(sector_t::floor, xofs);
		sectors[secnum].SetYOffset(sector_t::floor, yofs);

		// Tell clients about the floor panning update.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetSectorPanning( secnum );
	}
	return true;
}

FUNC(LS_Sector_SetFloorScale)
// Sector_SetFloorScale (tag, x-int, x-frac, y-int, y-frac)
{
	int secnum = -1;
	fixed_t xscale = arg1 * FRACUNIT + arg2 * (FRACUNIT/100);
	fixed_t yscale = arg3 * FRACUNIT + arg4 * (FRACUNIT/100);

	if (xscale)
		xscale = FixedDiv (FRACUNIT, xscale);
	if (yscale)
		yscale = FixedDiv (FRACUNIT, yscale);

	while ((secnum = P_FindSectorFromTag (arg0, secnum)) >= 0)
	{
		if (xscale)
			sectors[secnum].SetXScale(sector_t::floor, xscale);
		if (yscale)
			sectors[secnum].SetYScale(sector_t::floor, yscale);

		// [BC] Tell clients about the scale update.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetSectorScale( secnum );
	}
	return true;
}

FUNC(LS_Sector_SetCeilingScale)
// Sector_SetCeilingScale (tag, x-int, x-frac, y-int, y-frac)
{
	int secnum = -1;
	fixed_t xscale = arg1 * FRACUNIT + arg2 * (FRACUNIT/100);
	fixed_t yscale = arg3 * FRACUNIT + arg4 * (FRACUNIT/100);

	if (xscale)
		xscale = FixedDiv (FRACUNIT, xscale);
	if (yscale)
		yscale = FixedDiv (FRACUNIT, yscale);

	while ((secnum = P_FindSectorFromTag (arg0, secnum)) >= 0)
	{
		if (xscale)
			sectors[secnum].SetXScale(sector_t::ceiling, xscale);
		if (yscale)
			sectors[secnum].SetYScale(sector_t::ceiling, yscale);

		// [BC] Tell clients about the scale update.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetSectorScale( secnum );
	}
	return true;
}

FUNC(LS_Sector_SetFloorScale2)
// Sector_SetFloorScale2 (tag, x-factor, y-factor)
{
	int secnum = -1;

	if (arg1)
		arg1 = FixedDiv (FRACUNIT, arg1);
	if (arg2)
		arg2 = FixedDiv (FRACUNIT, arg2);

	while ((secnum = P_FindSectorFromTag (arg0, secnum)) >= 0)
	{
		if (arg1)
			sectors[secnum].SetXScale(sector_t::floor, arg1);
		if (arg2)
			sectors[secnum].SetYScale(sector_t::floor, arg2);

		// [BC] Tell clients about the scale update.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetSectorScale( secnum );
	}
	return true;
}

FUNC(LS_Sector_SetCeilingScale2)
// Sector_SetFloorScale2 (tag, x-factor, y-factor)
{
	int secnum = -1;

	if (arg1)
		arg1 = FixedDiv (FRACUNIT, arg1);
	if (arg2)
		arg2 = FixedDiv (FRACUNIT, arg2);

	while ((secnum = P_FindSectorFromTag (arg0, secnum)) >= 0)
	{
		if (arg1)
			sectors[secnum].SetXScale(sector_t::ceiling, arg1);
		if (arg2)
			sectors[secnum].SetYScale(sector_t::ceiling, arg2);

		// [BC] Tell clients about the scale update.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetSectorScale( secnum );
	}
	return true;
}

FUNC(LS_Sector_SetRotation)
// Sector_SetRotation (tag, floor-angle, ceiling-angle)
{
	int secnum = -1;
	angle_t ceiling = arg2 * ANGLE_1;
	angle_t floor = arg1 * ANGLE_1;

	while ((secnum = P_FindSectorFromTag (arg0, secnum)) >= 0)
	{
		sectors[secnum].SetAngle(sector_t::floor, floor);
		sectors[secnum].SetAngle(sector_t::ceiling, ceiling);
	}

	// [BB] Tell clients to set the the rotation for all sectors with tag arg0.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetSectorRotationByTag( arg0, arg1, arg2 );

	return true;
}

FUNC(LS_Line_AlignCeiling)
// Line_AlignCeiling (lineid, side)
{
	int line = P_FindLineFromID (arg0, -1);
	bool ret = 0;

	if (line < 0)
		I_Error ("Sector_AlignCeiling: Lineid %d is undefined", arg0);
	do
	{
		ret |= P_AlignFlat (line, !!arg1, 1);
	} while ( (line = P_FindLineFromID (arg0, line)) >= 0);
	return ret;
}

FUNC(LS_Line_AlignFloor)
// Line_AlignFloor (lineid, side)
{
	int line = P_FindLineFromID (arg0, -1);
	bool ret = 0;

	if (line < 0)
		I_Error ("Sector_AlignFloor: Lineid %d is undefined", arg0);
	do
	{
		ret |= P_AlignFlat (line, !!arg1, 0);
	} while ( (line = P_FindLineFromID (arg0, line)) >= 0);
	return ret;
}

FUNC(LS_Line_SetTextureOffset)
// Line_SetTextureOffset (id, x, y, side, flags)
{
	const fixed_t NO_CHANGE = 32767<<FRACBITS;

	if (arg0 == 0 || arg3 < 0 || arg3 > 1)
		return false;

	for(int line = -1; (line = P_FindLineFromID (arg0, line)) >= 0; )
	{
		side_t *side = lines[line].sidedef[arg3];
		if (side != NULL)
		{

			if ((arg4&8)==0)
			{
				// set
				if (arg1 != NO_CHANGE)
				{
					if (arg4&1) side->SetTextureXOffset(side_t::top, arg1);
					if (arg4&2) side->SetTextureXOffset(side_t::mid, arg1);
					if (arg4&4) side->SetTextureXOffset(side_t::bottom, arg1);
				}
				if (arg2 != NO_CHANGE)
				{
					if (arg4&1) side->SetTextureYOffset(side_t::top, arg2);
					if (arg4&2) side->SetTextureYOffset(side_t::mid, arg2);
					if (arg4&4) side->SetTextureYOffset(side_t::bottom, arg2);
				}
			}
			else
			{
				// add
				if (arg1 != NO_CHANGE)
				{
					if (arg4&1) side->AddTextureXOffset(side_t::top, arg1);
					if (arg4&2) side->AddTextureXOffset(side_t::mid, arg1);
					if (arg4&4) side->AddTextureXOffset(side_t::bottom, arg1);
				}
				if (arg2 != NO_CHANGE)
				{
					if (arg4&1) side->AddTextureYOffset(side_t::top, arg2);
					if (arg4&2) side->AddTextureYOffset(side_t::mid, arg2);
					if (arg4&4) side->AddTextureYOffset(side_t::bottom, arg2);
				}
			}
		}
	}
	return true;
}

FUNC(LS_Line_SetTextureScale)
// Line_SetTextureScale (id, x, y, side, flags)
{
	const fixed_t NO_CHANGE = 32767<<FRACBITS;

	if (arg0 == 0 || arg3 < 0 || arg3 > 1)
		return false;

	for(int line = -1; (line = P_FindLineFromID (arg0, line)) >= 0; )
	{
		side_t *side = lines[line].sidedef[arg3];
		if (side != NULL)
		{
			if ((arg4&8)==0)
			{
				// set
				if (arg1 != NO_CHANGE)
				{
					if (arg4&1) side->SetTextureXScale(side_t::top, arg1);
					if (arg4&2) side->SetTextureXScale(side_t::mid, arg1);
					if (arg4&4) side->SetTextureXScale(side_t::bottom, arg1);
				}
				if (arg2 != NO_CHANGE)
				{
					if (arg4&1) side->SetTextureYScale(side_t::top, arg2);
					if (arg4&2) side->SetTextureYScale(side_t::mid, arg2);
					if (arg4&4) side->SetTextureYScale(side_t::bottom, arg2);
				}
			}
			else
			{
				// add
				if (arg1 != NO_CHANGE)
				{
					if (arg4&1) side->MultiplyTextureXScale(side_t::top, arg1);
					if (arg4&2) side->MultiplyTextureXScale(side_t::mid, arg1);
					if (arg4&4) side->MultiplyTextureXScale(side_t::bottom, arg1);
				}
				if (arg2 != NO_CHANGE)
				{
					if (arg4&1) side->MultiplyTextureYScale(side_t::top, arg2);
					if (arg4&2) side->MultiplyTextureYScale(side_t::mid, arg2);
					if (arg4&4) side->MultiplyTextureYScale(side_t::bottom, arg2);
				}
			}
		}
	}
	return true;
}

FUNC(LS_Line_SetBlocking)
// Line_SetBlocking (id, setflags, clearflags)
{
	static const int flagtrans[] =
	{
		ML_BLOCKING,
		ML_BLOCKMONSTERS,
		ML_BLOCK_PLAYERS,
		ML_BLOCK_FLOATERS,
		ML_BLOCKPROJECTILE,
		ML_BLOCKEVERYTHING,
		ML_RAILING,
		ML_BLOCKUSE,
		ML_BLOCKSIGHT,
		ML_BLOCKHITSCAN,
		-1
	};

	if (arg0 == 0) return false;

	int setflags = 0;
	int clearflags = 0;

	for(int i = 0; flagtrans[i] != -1; i++, arg1 >>= 1, arg2 >>= 1)
	{
		if (arg1 & 1) setflags |= flagtrans[i];
		if (arg2 & 1) clearflags |= flagtrans[i];
	}

	for(int line = -1; (line = P_FindLineFromID (arg0, line)) >= 0; )
	{
		lines[line].flags = (lines[line].flags & ~clearflags) | setflags;

		// [Dusk] Update clients on the line flags
		if ( NETWORK_GetState() == NETSTATE_SERVER )
			SERVERCOMMANDS_SetSomeLineFlags( line );
	}
	return true;
}



FUNC(LS_ChangeCamera)
// ChangeCamera (tid, who, revert?)
{
	AActor *camera;
	if (arg0 != 0)
	{
		FActorIterator iterator (arg0);
		camera = iterator.Next ();
	}
	else
	{
		camera = NULL;
	}

	if (!it || !it->player || arg1)
	{
		int i;

		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (!playeringame[i])
				continue;

			AActor *oldcamera = players[i].camera;
			if (camera)
			{
				players[i].camera = camera;
				if (arg2)
					players[i].cheats |= CF_REVERTPLEASE;

				// [BC] If we're the server, tell this player to change his camera.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SetPlayerCamera( i, camera, ( arg2 ) ? true : false );
			}
			else
			{
				players[i].camera = players[i].mo;
				players[i].cheats &= ~CF_REVERTPLEASE;

				// [BC] If we're the server, tell this player to change his camera.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SetPlayerCamera( i, players[i].mo, false );
			}
			if (oldcamera != players[i].camera)
			{
				R_ClearPastViewer (players[i].camera);
			}
		}
	}
	else
	{
		AActor *oldcamera = it->player->camera;
		if (camera)
		{
			it->player->camera = camera;
			if (arg2)
				it->player->cheats |= CF_REVERTPLEASE;

			// [BC] If we're the server, tell this player to change his camera.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetPlayerCamera( ULONG( it->player - players ), camera, ( arg2 ) ? true : false );
		}
		else
		{
			it->player->camera = it;
			it->player->cheats &= ~CF_REVERTPLEASE;

			// [BC] If we're the server, tell this player to change his camera.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetPlayerCamera( ULONG( it->player - players ), it, false );
		}
		if (oldcamera != it->player->camera)
		{
			R_ClearPastViewer (it->player->camera);
		}
	}

	return true;
}

enum
{
	PROP_FROZEN,
	PROP_NOTARGET,
	PROP_INSTANTWEAPONSWITCH,
	PROP_FLY,
	PROP_TOTALLYFROZEN,

	PROP_INVULNERABILITY,
	PROP_STRENGTH,
	PROP_INVISIBILITY,
	PROP_RADIATIONSUIT,
	PROP_ALLMAP,
	PROP_INFRARED,
	PROP_WEAPONLEVEL2,
	PROP_FLIGHT,
	PROP_UNUSED1,
	PROP_UNUSED2,
	PROP_SPEED,
	PROP_BUDDHA,
};

FUNC(LS_SetPlayerProperty)
// SetPlayerProperty (who, value, which)
// who == 0: set activator's property
// who == 1: set every player's property
{
	int mask = 0;

	if ((!it || !it->player) && !arg0)
		return false;

	// Add or remove a power
	if (arg2 >= PROP_INVULNERABILITY && arg2 <= PROP_SPEED)
	{
		static const PClass *powers[11] =
		{
			RUNTIME_CLASS(APowerInvulnerable),
			RUNTIME_CLASS(APowerStrength),
			RUNTIME_CLASS(APowerInvisibility),
			RUNTIME_CLASS(APowerIronFeet),
			NULL, // MapRevealer
			RUNTIME_CLASS(APowerLightAmp),
			RUNTIME_CLASS(APowerWeaponLevel2),
			RUNTIME_CLASS(APowerFlight),
			NULL,
			NULL,
			RUNTIME_CLASS(APowerSpeed)
		};
		int power = arg2 - PROP_INVULNERABILITY;

		if (power > 4 && powers[power] == NULL)
		{
			return false;
		}

		if (arg0 == 0)
		{
			// [WS] If the player is spectating, do nothing. The reason this check is put here and
			// not at the beginning is because we may want the spectator to be able to puke a command
			// that gets activated on all players that are currently spawned/(not spectating).
			if ( it->player && it->player->bSpectating )
				return false;

			if (arg1)
			{ // Give power to activator
				if (power != 4)
				{
					APowerup *item = static_cast<APowerup*>(it->GiveInventoryType (powers[power]));
					if (item != NULL && power == 0 && arg1 == 1) 
					{
						item->BlendColor = MakeSpecialColormap(INVERSECOLORMAP);
					}

					// [WS] Inform clients of the powerup and blend color.
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					{
						SERVERCOMMANDS_GivePowerup( ULONG( it->player - players ), item );
						// [WS] If it is an invulnerability powerup, we need to inform the clients of the blend color.
						if ( power == 0 )
							SERVERCOMMANDS_SetPowerupBlendColor( ULONG( it->player - players ), item );
					}
				}
				else if (it->player - players == consoleplayer)
				{
					level.flags2 |= LEVEL2_ALLMAP;
				}
			}
			else
			{ // Take power from activator
				if (power != 4)
				{
					AInventory *item = it->FindInventory (powers[power], true);
					if (item != NULL)
					{
						// [WS] Destroy the powerup.
						if ( NETWORK_GetState( ) == NETSTATE_SERVER )
							SERVERCOMMANDS_TakeInventory( ULONG( it->player - players ), item->GetClass(), 0 );

						item->Destroy ();
					}
				}
				else if (it->player - players == consoleplayer)
				{
					level.flags2 &= ~LEVEL2_ALLMAP;
				}
			}
		}
		else
		{
			int i;

			for (i = 0; i < MAXPLAYERS; i++)
			{
				// [WS] Do not do this for spectators.
				if (!playeringame[i] || players[i].mo == NULL || players[i].bSpectating)
					continue;

				if (arg1)
				{ // Give power
					if (power != 4)
					{
						APowerup *item = static_cast<APowerup*>(players[i].mo->GiveInventoryType (powers[power]));
						if (item != NULL && power == 0 && arg1 == 1) 
						{
							item->BlendColor = MakeSpecialColormap(INVERSECOLORMAP);
						}

						// [WS] Inform clients of powerup.
						if ( NETWORK_GetState( ) == NETSTATE_SERVER )
							SERVERCOMMANDS_GivePowerup( i, static_cast<APowerup *>( item ) );
					}
					else if (i == consoleplayer)
					{
						level.flags2 |= LEVEL2_ALLMAP;
					}
				}
				else
				{ // Take power
					if (power != 4)
					{
						AInventory *item = players[i].mo->FindInventory (powers[power]);
						if (item != NULL)
						{
							// [WS] Destroy the powerup.
							if ( NETWORK_GetState( ) == NETSTATE_SERVER )
								SERVERCOMMANDS_TakeInventory( i, item->GetClass(), 0 );

							item->Destroy ();
						}
					}
					else if (i == consoleplayer)
					{
						level.flags2 &= ~LEVEL2_ALLMAP;
					}
				}
			}
		}
		return true;
	}

	// Set or clear a flag
	switch (arg2)
	{
	case PROP_BUDDHA:
		mask = CF_BUDDHA;
		break;
	case PROP_FROZEN:
		mask = CF_FROZEN;
		break;
	case PROP_NOTARGET:
		mask = CF_NOTARGET;
		break;
	case PROP_INSTANTWEAPONSWITCH:
		mask = CF_INSTANTWEAPSWITCH;
		break;
	case PROP_FLY:
		mask = CF_FLY;
		break;
	case PROP_TOTALLYFROZEN:
		mask = CF_TOTALLYFROZEN;
		break;
	}

	if (arg0 == 0)
	{
		// [Leo] Not done for spectators.
		if ( it->player->bSpectating )
			return false;

		int oldCheats = it->player->cheats;

		if (arg1)
		{
			it->player->cheats |= mask;
			if (arg2 == PROP_FLY)
			{
				it->flags2 |= MF2_FLY;
				it->flags |= MF_NOGRAVITY;
			}
		}
		else
		{
			it->player->cheats &= ~mask;
			if (arg2 == PROP_FLY)
			{
				it->flags2 &= ~MF2_FLY;
				it->flags &= ~MF_NOGRAVITY;
			}
		}

		// [BB] Tell the client that his cheats have changed.
		if ( NETWORK_GetState() == NETSTATE_SERVER && oldCheats != it->player->cheats )
		{
			SERVERCOMMANDS_SetPlayerCheats( ULONG( it->player - players ), ULONG( it->player - players ), SVCF_ONLYTHISCLIENT );
			SERVERCOMMANDS_SetThingFlags( it, FLAGSET_FLAGS );
			SERVERCOMMANDS_SetThingFlags( it, FLAGSET_FLAGS2 );
		}
	}
	else
	{
		int i;

		if ((ib_compatflags & BCOMPATF_LINKFROZENPROPS) && (mask & (CF_FROZEN | CF_TOTALLYFROZEN)))
		{ // Clearing one of these properties clears both of them (if the compat flag is set.)
			mask = CF_FROZEN | CF_TOTALLYFROZEN;
		}

		for (i = 0; i < MAXPLAYERS; i++)
		{
			// [Leo] Skip spectators.
			if (!playeringame[i] || players[i].bSpectating)
				continue;

			int oldCheats = players[i].cheats;

			if (arg1)
			{
				players[i].cheats |= mask;
				if (arg2 == PROP_FLY)
				{
					players[i].mo->flags2 |= MF2_FLY;
					players[i].mo->flags |= MF_NOGRAVITY;
				}
			}
			else
			{
				players[i].cheats &= ~mask;
				if (arg2 == PROP_FLY)
				{
					players[i].mo->flags2 &= ~MF2_FLY;
					players[i].mo->flags &= ~MF_NOGRAVITY;
				}
			}

			// [BB] Tell the client that his cheats have changed.
			if ( NETWORK_GetState() == NETSTATE_SERVER && oldCheats != players[i].cheats )
			{
				SERVERCOMMANDS_SetPlayerCheats( i, i, SVCF_ONLYTHISCLIENT );
				SERVERCOMMANDS_SetThingFlags( players[i].mo, FLAGSET_FLAGS );
				SERVERCOMMANDS_SetThingFlags( players[i].mo, FLAGSET_FLAGS2 );
			}
		}
	}

	return !!mask;
}

FUNC(LS_TranslucentLine)
// TranslucentLine (id, amount, type)
{
	int linenum = -1;
	while ((linenum = P_FindLineFromID (arg0, linenum)) >= 0)
	{
		lines[linenum].Alpha = Scale(clamp(arg1, 0, 255), FRACUNIT, 255);

		// [BC] If we're the server, tell clients to adjust this line's alpha.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetLineAlpha( linenum );

		if (arg2 == 0)
		{
			lines[linenum].flags &= ~ML_ADDTRANS;
		}
		else if (arg2 == 1)
		{
			lines[linenum].flags |= ML_ADDTRANS;
		}
		else
		{
			Printf ("Unknown translucency type used with TranslucentLine\n");
		}

		// [BB] If we're the server, tell clients the new ML_ADDTRANS setting.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetSomeLineFlags( linenum );
	}

	return true;
}

FUNC(LS_Autosave)
{
	if (gameaction != ga_savegame)
	{
		level.flags2 &= ~LEVEL2_NOAUTOSAVEHINT;
		Net_WriteByte (DEM_CHECKAUTOSAVE);
	}
	return true;
}

FUNC(LS_ChangeSkill)
{
	if ((unsigned)arg0 >= AllSkills.Size())
	{
		NextSkill = -1;
	}
	else
	{
		NextSkill = arg0;
	}
	return true;
}

FUNC(LS_NoiseAlert)
// NoiseAlert (TID of target, TID of emitter)
{
	AActor *target, *emitter;

	if (arg0 == 0)
	{
		target = it;
	}
	else
	{
		FActorIterator iter (arg0);
		target = iter.Next();
	}

	if (arg1 == 0)
	{
		emitter = it;
	}
	else if (arg1 == arg0)
	{
		emitter = target;
	}
	else
	{
		FActorIterator iter (arg1);
		emitter = iter.Next();
	}

	P_NoiseAlert (target, emitter);
	return true;
}

FUNC(LS_SendToCommunicator)
// SendToCommunicator (voc #, front-only, identify, nolog)
{
	// This obviously isn't going to work for co-op.
	if (arg1 && backSide)
		return false;

	if (it != NULL && it->player != NULL && it->FindInventory(NAME_Communicator))
	{
		char name[32];									   
		mysnprintf (name, countof(name), "svox/voc%d", arg0);

		if (!arg3)
		{
			it->player->SetLogNumber (arg0);

			// [BB] Set the log number on the clients.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetPlayerLogNumber ( static_cast<ULONG> ( it->player - players ), arg0 );
		}

		if (it->CheckLocalView (consoleplayer))
		{
			S_StopSound (CHAN_VOICE);
			S_Sound (CHAN_VOICE, name, 1, ATTN_NORM);

			// [BB] Play the sound on the client.
			const ULONG ulPlayer = static_cast<ULONG> ( it->player - players );
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_Sound( CHAN_VOICE, name, 1, ATTN_NORM, ulPlayer, SVCF_ONLYTHISCLIENT );

			if (arg2 == 0)
			{
				Printf (PRINT_CHAT, "Incoming Message\n");

				// [BB] Print the message on the client.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVER_PrintfPlayer( PRINT_CHAT, ulPlayer, "Incoming Message\n" );
			}
			else if (arg2 == 1)
			{
				Printf (PRINT_CHAT, "Incoming Message from BlackBird\n");

				// [BB] Print the message on the client.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVER_PrintfPlayer( PRINT_CHAT, ulPlayer, "Incoming Message from BlackBird\n" );
			}
		}
		return true;
	}
	return false;
}

FUNC(LS_ForceField)
// ForceField ()
{
	if (it != NULL)
	{
		P_DamageMobj (it, NULL, NULL, 16, NAME_None);
		P_ThrustMobj (it, it->angle + ANGLE_180, 0x7D000);
	}
	return true;
}

FUNC(LS_ClearForceField)
// ClearForceField (tag)
{
	int secnum = -1;
	bool rtn = false;

	while ((secnum = P_FindSectorFromTag (arg0, secnum)) >= 0)
	{
		sector_t *sec = &sectors[secnum];
		rtn = true;

		for (int i = 0; i < sec->linecount; ++i)
		{
			line_t *line = sec->lines[i];
			if (line->backsector != NULL && line->special == ForceField)
			{
				line->flags &= ~(ML_BLOCKING|ML_BLOCKEVERYTHING);
				line->special = 0;
				line->sidedef[0]->SetTexture(side_t::mid, FNullTextureID());
				line->sidedef[1]->SetTexture(side_t::mid, FNullTextureID());

				// [BC] Mark this line's texture change flags.
				line->ulTexChangeFlags |= TEXCHANGE_FRONTMEDIUM|TEXCHANGE_BACKMEDIUM;

				// [BC] If we're the server, tell clients that this line's textures and
				// blocking status have been altered.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					SERVERCOMMANDS_SetLineTexture( ULONG( line - lines ));
					SERVERCOMMANDS_SetSomeLineFlags( ULONG( line - lines ));
				}
			}
		}
	}
	return rtn;
}

FUNC(LS_GlassBreak)
// GlassBreak (bNoJunk)
{
	bool switched;
	bool quest1, quest2;

	ln->flags &= ~(ML_BLOCKING|ML_BLOCKEVERYTHING);

	// [BC] If we're the server, update this line's blocking.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetSomeLineFlags( ULONG( ln - lines ));

	switched = P_ChangeSwitchTexture (ln->sidedef[0], false, 0, &quest1);
	ln->special = 0;
	if (ln->sidedef[1] != NULL)
	{
		switched |= P_ChangeSwitchTexture (ln->sidedef[1], false, 0, &quest2);
	}
	else
	{
		quest2 = quest1;
	}
	if (switched)
	{
		if (!arg0)
		{ // Break some glass
			fixed_t x, y;
			AActor *glass;
			angle_t an;
			int speed;

			x = ln->v1->x + ln->dx/2;
			y = ln->v1->y + ln->dy/2;
			x += (ln->frontsector->soundorg[0] - x) / 5;
			y += (ln->frontsector->soundorg[1] - y) / 5;

			for (int i = 0; i < 7; ++i)
			{
				glass = Spawn("GlassJunk", x, y, ONFLOORZ, ALLOW_REPLACE);

				glass->z += 24 * FRACUNIT;
				glass->SetState (glass->SpawnState + (pr_glass() % glass->health));
				an = pr_glass() << (32-8);
				glass->angle = an;
				an >>= ANGLETOFINESHIFT;
				speed = pr_glass() & 3;
				glass->velx = finecosine[an] * speed;
				glass->vely = finesine[an] * speed;
				glass->velz = (pr_glass() & 7) << FRACBITS;
				// [RH] Let the shards stick around longer than they did in Strife.
				glass->tics += pr_glass();
			}
		}
		if (quest1 || quest2)
		{ // Up stats and signal this mission is complete
			if (it == NULL)
			{
				for (int i = 0; i < MAXPLAYERS; ++i)
				{
					if (playeringame[i])
					{
						it = players[i].mo;
						break;
					}
				}
			}
			if (it != NULL)
			{
				it->GiveInventoryType (QuestItemClasses[28]);
				it->GiveInventoryType (RUNTIME_CLASS(AUpgradeAccuracy));
				it->GiveInventoryType (RUNTIME_CLASS(AUpgradeStamina));
			}
		}
	}
	// We already changed the switch texture, so don't make the main code switch it back.
	return false;
}

FUNC(LS_StartConversation)
// StartConversation (tid, facetalker)
{
	FActorIterator iterator (arg0);

	AActor *target = iterator.Next();

	// Nothing to talk to
	if (target == NULL)
	{
		return false;
	}
	
	// Only living players are allowed to start conversations
	if (it == NULL || it->player == NULL || it->player->mo != it || it->health<=0)
	{
		return false;
	}

	// Dead things can't talk.
	if (target->health <= 0)
	{
		return false;
	}
	// Fighting things don't talk either.
	if (target->flags4 & MF4_INCOMBAT)
	{
		return false;
	}
	if (target->Conversation != NULL)
	{
		// Give the NPC a chance to play a brief animation
		target->ConversationAnimation (0);
		P_StartConversation (target, it, !!arg1, true);
		return true;
	}
	return false;
}

FUNC(LS_Thing_SetConversation)
// Thing_SetConversation (tid, dlg_id)
{
	int dlg_index = -1;
	FStrifeDialogueNode *node = NULL;

	if (arg1 != 0)
	{
		dlg_index = GetConversation(arg1);	
		if (dlg_index == -1) return false;
		node = StrifeDialogues[dlg_index];
	}

	if (arg0 != 0)
	{
		FActorIterator iterator (arg0);
		while ((it = iterator.Next()) != NULL)
		{
			it->ConversationRoot = dlg_index;
			it->Conversation = node;
		}
	}
	else if (it)
	{
		it->ConversationRoot = dlg_index;
		it->Conversation = node;
	}
	return true;
}

// [BC] From GZDoom. Now in p_lnspec.cpp where it belongs.
// Normally this would be better placed in p_lnspec.cpp.
// But I have accidentally overwritten that file several times
// so I'd rather place it here.
FUNC(LS_Sector_SetPlaneReflection)
// Sector_SetPlaneReflection (tag, floor, ceiling)
{
	int secnum = -1;

	while ((secnum = P_FindSectorFromTag (arg0, secnum)) >= 0)
	{
		sector_t * s = &sectors[secnum];
		if (s->floorplane.a==0 && s->floorplane.b==0) s->reflect[sector_t::floor] = arg1/255.f;
		if (s->ceilingplane.a==0 && s->ceilingplane.b==0) sectors[secnum].reflect[sector_t::ceiling] = arg2/255.f;

		// [BC] If we're the server, tell clients that this sector's reflection is being altered.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetSectorReflection( secnum );
	}

	return true;
}


lnSpecFunc LineSpecials[256] =
{
	/*   0 */ LS_NOP,
	/*   1 */ LS_NOP,		// Polyobj_StartLine,
	/*   2 */ LS_Polyobj_RotateLeft,
	/*   3 */ LS_Polyobj_RotateRight,
	/*   4 */ LS_Polyobj_Move,
	/*   5 */ LS_NOP,		// Polyobj_ExplicitLine
	/*   6 */ LS_Polyobj_MoveTimes8,
	/*   7 */ LS_Polyobj_DoorSwing,
	/*   8 */ LS_Polyobj_DoorSlide,
	/*   9 */ LS_NOP,		// Line_Horizon
	/*  10 */ LS_Door_Close,
	/*  11 */ LS_Door_Open,
	/*  12 */ LS_Door_Raise,
	/*  13 */ LS_Door_LockedRaise,
	/*  14 */ LS_Door_Animated,
	/*  15 */ LS_Autosave,
	/*  16 */ LS_NOP,		// Transfer_WallLight
	/*  17 */ LS_Thing_Raise,
	/*  18 */ LS_StartConversation,
	/*  19 */ LS_Thing_Stop,
	/*  20 */ LS_Floor_LowerByValue,
	/*  21 */ LS_Floor_LowerToLowest,
	/*  22 */ LS_Floor_LowerToNearest,
	/*  23 */ LS_Floor_RaiseByValue,
	/*  24 */ LS_Floor_RaiseToHighest,
	/*  25 */ LS_Floor_RaiseToNearest,
	/*  26 */ LS_Stairs_BuildDown,
	/*  27 */ LS_Stairs_BuildUp,
	/*  28 */ LS_Floor_RaiseAndCrush,
	/*  29 */ LS_Pillar_Build,
	/*  30 */ LS_Pillar_Open,
	/*  31 */ LS_Stairs_BuildDownSync,
	/*  32 */ LS_Stairs_BuildUpSync,
	/*  33 */ LS_ForceField,
	/*  34 */ LS_ClearForceField,
	/*  35 */ LS_Floor_RaiseByValueTimes8,
	/*  36 */ LS_Floor_LowerByValueTimes8,
	/*  37 */ LS_Floor_MoveToValue,
	/*  38 */ LS_Ceiling_Waggle,
	/*  39 */ LS_Teleport_ZombieChanger,
	/*  40 */ LS_Ceiling_LowerByValue,
	/*  41 */ LS_Ceiling_RaiseByValue,
	/*  42 */ LS_Ceiling_CrushAndRaise,
	/*  43 */ LS_Ceiling_LowerAndCrush,
	/*  44 */ LS_Ceiling_CrushStop,
	/*  45 */ LS_Ceiling_CrushRaiseAndStay,
	/*  46 */ LS_Floor_CrushStop,
	/*  47 */ LS_Ceiling_MoveToValue,
	/*  48 */ LS_NOP,		// Sector_Attach3dMidtex
	/*  49 */ LS_GlassBreak,
	/*  50 */ LS_NOP,		// ExtraFloor_LightOnly
	/*  51 */ LS_Sector_SetLink,
	/*  52 */ LS_Scroll_Wall,
	/*  53 */ LS_Line_SetTextureOffset,
	/*  54 */ LS_Sector_ChangeFlags,
	/*  55 */ LS_Line_SetBlocking,
	/*  56 */ LS_Line_SetTextureScale,
	/*  57 */ LS_NOP,		// Sector_SetPortal
	/*  58 */ LS_NOP,		// Sector_CopyScroller
	/*  59 */ LS_Polyobj_OR_MoveToSpot,
	/*  60 */ LS_Plat_PerpetualRaise,
	/*  61 */ LS_Plat_Stop,
	/*  62 */ LS_Plat_DownWaitUpStay,
	/*  63 */ LS_Plat_DownByValue,
	/*  64 */ LS_Plat_UpWaitDownStay,
	/*  65 */ LS_Plat_UpByValue,
	/*  66 */ LS_Floor_LowerInstant,
	/*  67 */ LS_Floor_RaiseInstant,
	/*  68 */ LS_Floor_MoveToValueTimes8,
	/*  69 */ LS_Ceiling_MoveToValueTimes8,
	/*  70 */ LS_Teleport,
	/*  71 */ LS_Teleport_NoFog,
	/*  72 */ LS_ThrustThing,
	/*  73 */ LS_DamageThing,
	/*  74 */ LS_Teleport_NewMap,
	/*  75 */ LS_Teleport_EndGame,
	/*  76 */ LS_TeleportOther,
	/*  77 */ LS_TeleportGroup,
	/*  78 */ LS_TeleportInSector,
	/*  79 */ LS_Thing_SetConversation,
	/*  80 */ LS_ACS_Execute,
	/*  81 */ LS_ACS_Suspend,
	/*  82 */ LS_ACS_Terminate,
	/*  83 */ LS_ACS_LockedExecute,
	/*  84 */ LS_ACS_ExecuteWithResult,
	/*  85 */ LS_ACS_LockedExecuteDoor,
	/*  86 */ LS_Polyobj_MoveToSpot,
	/*  87 */ LS_Polyobj_Stop,
	/*  88 */ LS_Polyobj_MoveTo,
	/*  89 */ LS_Polyobj_OR_MoveTo,
	/*  90 */ LS_Polyobj_OR_RotateLeft,
	/*  91 */ LS_Polyobj_OR_RotateRight,
	/*  92 */ LS_Polyobj_OR_Move,
	/*  93 */ LS_Polyobj_OR_MoveTimes8,
	/*  94 */ LS_Pillar_BuildAndCrush,
	/*  95 */ LS_FloorAndCeiling_LowerByValue,
	/*  96 */ LS_FloorAndCeiling_RaiseByValue,
	/*  97 */ LS_Ceiling_LowerAndCrushDist,
	/*  98 */ LS_Sector_SetTranslucent,
	/*  99 */ LS_Floor_RaiseAndCrushDoom,
	/* 100 */ LS_NOP,		// Scroll_Texture_Left
	/* 101 */ LS_NOP,		// Scroll_Texture_Right
	/* 102 */ LS_NOP,		// Scroll_Texture_Up
	/* 103 */ LS_NOP,		// Scroll_Texture_Down
	/* 104 */ LS_NOP,
	/* 105 */ LS_NOP,
	/* 106 */ LS_NOP,
	/* 107 */ LS_NOP,
	/* 108 */ LS_NOP,
	/* 109 */ LS_Light_ForceLightning,
	/* 110 */ LS_Light_RaiseByValue,
	/* 111 */ LS_Light_LowerByValue,
	/* 112 */ LS_Light_ChangeToValue,
	/* 113 */ LS_Light_Fade,
	/* 114 */ LS_Light_Glow,
	/* 115 */ LS_Light_Flicker,
	/* 116 */ LS_Light_Strobe,
	/* 117 */ LS_Light_Stop,
	/* 118 */ LS_NOP,		// Plane_Copy
	/* 119 */ LS_Thing_Damage,
	/* 120 */ LS_Radius_Quake,
	/* 121 */ LS_NOP,		// Line_SetIdentification
	/* 122 */ LS_NOP,
	/* 123 */ LS_NOP,
	/* 124 */ LS_NOP,
	/* 125 */ LS_Thing_Move,
	/* 126 */ LS_NOP,
	/* 127 */ LS_Thing_SetSpecial,
	/* 128 */ LS_ThrustThingZ,
	/* 129 */ LS_UsePuzzleItem,
	/* 130 */ LS_Thing_Activate,
	/* 131 */ LS_Thing_Deactivate,
	/* 132 */ LS_Thing_Remove,
	/* 133 */ LS_Thing_Destroy,
	/* 134 */ LS_Thing_Projectile,
	/* 135 */ LS_Thing_Spawn,
	/* 136 */ LS_Thing_ProjectileGravity,
	/* 137 */ LS_Thing_SpawnNoFog,
	/* 138 */ LS_Floor_Waggle,
	/* 139 */ LS_Thing_SpawnFacing,
	/* 140 */ LS_Sector_ChangeSound,
	/* 141 */ LS_NOP,
	/* 142 */ LS_NOP,
	/* 143 */ LS_NOP,
	/* 144 */ LS_NOP,
	/* 145 */ LS_Player_SetTeam,
	/* 146 */ LS_NOP,
	/* 147 */ LS_NOP,
	/* 148 */ LS_NOP,
	/* 149 */ LS_NOP,
	/* 150 */ LS_NOP,
	/* 151 */ LS_NOP,
	/* 152 */ LS_Team_Score,
	/* 153 */ LS_Team_GivePoints,
	/* 154 */ LS_Teleport_NoStop,
	/* 155 */ LS_NOP,
	/* 156 */ LS_NOP,
	/* 157 */ LS_NOP,		// SetGlobalFogParameter // in GZDoom
	/* 158 */ LS_NOP,		// FS_Execute
	/* 159 */ LS_Sector_SetPlaneReflection,
	/* 160 */ LS_NOP,		// Sector_Set3DFloor
	/* 161 */ LS_NOP,		// Sector_SetContents
	/* 162 */ LS_NOP,		// Reserved Doom64 branch
	/* 163 */ LS_NOP,		// Reserved Doom64 branch
	/* 164 */ LS_NOP,		// Reserved Doom64 branch
	/* 165 */ LS_NOP,		// Reserved Doom64 branch
	/* 166 */ LS_NOP,		// Reserved Doom64 branch
	/* 167 */ LS_NOP,		// Reserved Doom64 branch
	/* 168 */ LS_Ceiling_CrushAndRaiseDist,
	/* 169 */ LS_Generic_Crusher2,
	/* 170 */ LS_Sector_SetCeilingScale2,
	/* 171 */ LS_Sector_SetFloorScale2,
	/* 172 */ LS_Plat_UpNearestWaitDownStay,
	/* 173 */ LS_NoiseAlert,
	/* 174 */ LS_SendToCommunicator,
	/* 175 */ LS_Thing_ProjectileIntercept,
	/* 176 */ LS_Thing_ChangeTID,
	/* 177 */ LS_Thing_Hate,
	/* 178 */ LS_Thing_ProjectileAimed,
	/* 179 */ LS_ChangeSkill,
	/* 180 */ LS_Thing_SetTranslation,
	/* 181 */ LS_NOP,		// Plane_Align
	/* 182 */ LS_NOP,		// Line_Mirror
	/* 183 */ LS_Line_AlignCeiling,
	/* 184 */ LS_Line_AlignFloor,
	/* 185 */ LS_Sector_SetRotation,
	/* 186 */ LS_Sector_SetCeilingPanning,
	/* 187 */ LS_Sector_SetFloorPanning,
	/* 188 */ LS_Sector_SetCeilingScale,
	/* 189 */ LS_Sector_SetFloorScale,
	/* 190 */ LS_NOP,		// Static_Init
	/* 191 */ LS_SetPlayerProperty,
	/* 192 */ LS_Ceiling_LowerToHighestFloor,
	/* 193 */ LS_Ceiling_LowerInstant,
	/* 194 */ LS_Ceiling_RaiseInstant,
	/* 195 */ LS_Ceiling_CrushRaiseAndStayA,
	/* 196 */ LS_Ceiling_CrushAndRaiseA,
	/* 197 */ LS_Ceiling_CrushAndRaiseSilentA,
	/* 198 */ LS_Ceiling_RaiseByValueTimes8,
	/* 199 */ LS_Ceiling_LowerByValueTimes8,
	/* 200 */ LS_Generic_Floor,
	/* 201 */ LS_Generic_Ceiling,
	/* 202 */ LS_Generic_Door,
	/* 203 */ LS_Generic_Lift,
	/* 204 */ LS_Generic_Stairs,
	/* 205 */ LS_Generic_Crusher,
	/* 206 */ LS_Plat_DownWaitUpStayLip,
	/* 207 */ LS_Plat_PerpetualRaiseLip,
	/* 208 */ LS_TranslucentLine,
	/* 209 */ LS_NOP,		// Transfer_Heights
	/* 210 */ LS_NOP,		// Transfer_FloorLight
	/* 211 */ LS_NOP,		// Transfer_CeilingLight
	/* 212 */ LS_Sector_SetColor,
	/* 213 */ LS_Sector_SetFade,
	/* 214 */ LS_Sector_SetDamage,
	/* 215 */ LS_Teleport_Line,
	/* 216 */ LS_Sector_SetGravity,
	/* 217 */ LS_Stairs_BuildUpDoom,
	/* 218 */ LS_Sector_SetWind,
	/* 219 */ LS_Sector_SetFriction,
	/* 220 */ LS_Sector_SetCurrent,
	/* 221 */ LS_Scroll_Texture_Both,
	/* 222 */ LS_NOP,		// Scroll_Texture_Model
	/* 223 */ LS_Scroll_Floor,
	/* 224 */ LS_Scroll_Ceiling,
	/* 225 */ LS_NOP,		// Scroll_Texture_Offsets
	/* 226 */ LS_ACS_ExecuteAlways,
	/* 227 */ LS_PointPush_SetForce,
	/* 228 */ LS_Plat_RaiseAndStayTx0,
	/* 229 */ LS_Thing_SetGoal,
	/* 230 */ LS_Plat_UpByValueStayTx,
	/* 231 */ LS_Plat_ToggleCeiling,
	/* 232 */ LS_Light_StrobeDoom,
	/* 233 */ LS_Light_MinNeighbor,
	/* 234 */ LS_Light_MaxNeighbor,
	/* 235 */ LS_Floor_TransferTrigger,
	/* 236 */ LS_Floor_TransferNumeric,
	/* 237 */ LS_ChangeCamera,
	/* 238 */ LS_Floor_RaiseToLowestCeiling,
	/* 239 */ LS_Floor_RaiseByValueTxTy,
	/* 240 */ LS_Floor_RaiseByTexture,
	/* 241 */ LS_Floor_LowerToLowestTxTy,
	/* 242 */ LS_Floor_LowerToHighest,
	/* 243 */ LS_Exit_Normal,
	/* 244 */ LS_Exit_Secret,
	/* 245 */ LS_Elevator_RaiseToNearest,
	/* 246 */ LS_Elevator_MoveToFloor,
	/* 247 */ LS_Elevator_LowerToNearest,
	/* 248 */ LS_HealThing,
	/* 249 */ LS_Door_CloseWaitOpen,
	/* 250 */ LS_Floor_Donut,
	/* 251 */ LS_FloorAndCeiling_LowerRaise,
	/* 252 */ LS_Ceiling_RaiseToNearest,
	/* 253 */ LS_Ceiling_LowerToLowest,
	/* 254 */ LS_Ceiling_LowerToFloor,
	/* 255 */ LS_Ceiling_CrushRaiseAndStaySilA
};

#define DEFINE_SPECIAL(name, num, min, max, mmax) {#name, num, min, max, mmax},
static FLineSpecial LineSpecialNames[] = {
#include "actionspecials.h"
};
const FLineSpecial *LineSpecialsInfo[256];

static int STACK_ARGS lscmp (const void * a, const void * b)
{
	return stricmp( ((FLineSpecial*)a)->name, ((FLineSpecial*)b)->name);
}

static struct InitLineSpecials
{
	InitLineSpecials()
	{
		qsort(LineSpecialNames, countof(LineSpecialNames), sizeof(FLineSpecial), lscmp);
		for (size_t i = 0; i < countof(LineSpecialNames); ++i)
		{
			assert(LineSpecialsInfo[LineSpecialNames[i].number] == NULL);
			LineSpecialsInfo[LineSpecialNames[i].number] = &LineSpecialNames[i];
		}
	}
} DoInit;

//==========================================================================
//
// P_FindLineSpecial
//
// Finds a line special and also returns the min and max argument count.
//
//==========================================================================

int P_FindLineSpecial (const char *string, int *min_args, int *max_args)
{
	int min = 0, max = countof(LineSpecialNames) - 1;

	while (min <= max)
	{
		int mid = (min + max) / 2;
		int lexval = stricmp (string, LineSpecialNames[mid].name);
		if (lexval == 0)
		{
			if (min_args != NULL) *min_args = LineSpecialNames[mid].min_args;
			if (max_args != NULL) *max_args = LineSpecialNames[mid].max_args;
			return LineSpecialNames[mid].number;
		}
		else if (lexval > 0)
		{
			min = mid + 1;
		}
		else
		{
			max = mid - 1;
		}
	}
	return 0;
}


//==========================================================================
//
// P_ExecuteSpecial
//
//==========================================================================

int P_ExecuteSpecial(int			num,
					 struct line_t	*line,
					 class AActor	*activator,
					 bool			backSide,
					 bool			isFromAcs,
					 int			arg1,
					 int			arg2,
					 int			arg3,
					 int			arg4,
					 int			arg5)
{
	if (num >= 0 && num <= 255)
	{
		return LineSpecials[num](line, activator, backSide, isFromAcs, arg1, arg2, arg3, arg4, arg5);
	}
	return 0;
}
