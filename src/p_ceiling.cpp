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
// DESCRIPTION:  Ceiling animation (lowering, crushing, raising)
//
//-----------------------------------------------------------------------------



#include "doomdef.h"
#include "p_local.h"
#include "s_sound.h"
#include "s_sndseq.h"
#include "doomstat.h"
#include "r_state.h"
#include "gi.h"
#include "farchive.h"
// [BB] New #includes.
#include "cl_demo.h"
#include "network.h"
#include "sv_commands.h"
#include "cl_main.h"

//============================================================================
//
// 
//
//============================================================================

inline FArchive &operator<< (FArchive &arc, DCeiling::ECeiling &type)
{
	BYTE val = (BYTE)type;
	arc << val;
	type = (DCeiling::ECeiling)val;
	return arc;
}

//============================================================================
//
// CEILINGS
//
//============================================================================

IMPLEMENT_CLASS (DCeiling)

DCeiling::DCeiling ()
{
}

//============================================================================
//
// 
//
//============================================================================

void DCeiling::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Type
		<< m_BottomHeight
		<< m_TopHeight
		<< m_Speed
		<< m_SpeedDown
		<< m_SpeedUp
		<< m_Crush
		<< m_Silent
		<< m_Direction
		<< m_Texture
		<< m_NewSpecial
		<< m_Tag
		<< m_OldDirection
		<< m_Hexencrush;
}

//============================================================================
//
// 
//
//============================================================================

void DCeiling::PlayCeilingSound ()
{
	if (m_Sector->seqType >= 0)
	{
		SN_StartSequence (m_Sector, CHAN_CEILING, m_Sector->seqType, SEQ_PLATFORM, 0, false);
	}
	else if (m_Sector->SeqName != NAME_None)
	{
		SN_StartSequence (m_Sector, CHAN_CEILING, m_Sector->SeqName, 0);
	}
	else
	{
		if (m_Silent == 2)
			SN_StartSequence (m_Sector, CHAN_CEILING, "Silence", 0);
		else if (m_Silent == 1)
			SN_StartSequence (m_Sector, CHAN_CEILING, "CeilingSemiSilent", 0);
		else
			SN_StartSequence (m_Sector, CHAN_CEILING, "CeilingNormal", 0);
	}
}

//============================================================================
//
// DCeiling :: Tick
//
//============================================================================

void DCeiling::Tick ()
{
	EResult res;
		
	switch (m_Direction)
	{
	case 0:
		// IN STASIS
		break;
	case 1:
		// UP
		res = MoveCeiling (m_Speed, m_TopHeight, m_Direction);
		
		if (res == pastdest)
		{
			switch (m_Type)
			{
			case ceilCrushAndRaise:
			case ceilCrushAndRaiseDist:
				m_Direction = -1;
				m_Speed = m_SpeedDown;

				if (!SN_IsMakingLoopingSound (m_Sector))
					PlayCeilingSound ();

				break;
				
			// movers with texture change, change the texture then get removed
			case genCeilingChgT:
			case genCeilingChg0:
				m_Sector->special = m_NewSpecial;
				// fall through
			case genCeilingChg:
				m_Sector->SetTexture(sector_t::ceiling, m_Texture);

				// [BB] Also, mark this sector as having its flat changed.
				m_Sector->bFlatChange = true;

				// fall through
			default:
				SN_StopSequence (m_Sector, CHAN_CEILING);
				m_Direction = 0;
				break;
			}

			// [geNia] If we are the server, update the clients
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_DoCeiling( this );
		}
		break;
		
	case -1:
		// DOWN
		res = MoveCeiling (m_Speed, m_BottomHeight, m_Crush, m_Direction, m_Hexencrush);
		
		if (res == pastdest)
		{
			switch (m_Type)
			{
			case ceilCrushAndRaise:
			case ceilCrushAndRaiseDist:
			case ceilCrushRaiseAndStay:
				m_Speed = m_SpeedUp;
				m_Direction = 1;

				if (!SN_IsMakingLoopingSound (m_Sector))
					PlayCeilingSound ();

				break;

			// in the case of ceiling mover/changer, change the texture
			// then remove the active ceiling
			case genCeilingChgT:
			case genCeilingChg0:
				m_Sector->special = m_NewSpecial;
				// fall through
			case genCeilingChg:
				m_Sector->SetTexture(sector_t::ceiling, m_Texture);

				// [BB] Also, mark this sector as having its flat changed.
				m_Sector->bFlatChange = true;

				// fall through
			default:

				SN_StopSequence (m_Sector, CHAN_CEILING);
				m_Direction = 0;
				break;
			}

			// [geNia] If we are the server, update the clients
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_DoCeiling( this );
		}
		else // ( res != pastdest )
		{
			if (res == crushed)
			{
				switch (m_Type)
				{
				case ceilCrushAndRaise:
				case ceilCrushAndRaiseDist:
				case ceilLowerAndCrush:
				case ceilLowerAndCrushDist:
					if (m_SpeedDown == FRACUNIT && m_SpeedUp == FRACUNIT)
					{
						m_Speed = FRACUNIT / 8;

						// [geNia] If we are the server, update the clients
						if ( NETWORK_GetState( ) == NETSTATE_SERVER )
							SERVERCOMMANDS_DoCeiling( this );
					}
					break;

				default:
					break;
				}
			}
		}
		break;
	}
}

void DCeiling::UpdateToClient( ULONG ulClient )
{
	SERVERCOMMANDS_DoCeiling( this, ulClient, SVCF_ONLYTHISCLIENT );
}

void DCeiling::Predict()
{
	// Use a version of gametic that's appropriate for both the current game and demos.
	ULONG TicsToPredict = gametic - CLIENTDEMO_GetGameticOffset( );

	// [geNia] This would mean that a negative amount of prediction tics is needed, so something is wrong.
	// So far it looks like the "lagging at connect / map start" prevented this from happening before.
	if ( CLIENT_GetLastConsolePlayerUpdateTick() > TicsToPredict)
		return;

	// How many ticks of prediction do we need?
	TicsToPredict = TicsToPredict - CLIENT_GetLastConsolePlayerUpdateTick( );

	while (TicsToPredict)
	{
		Tick();
		TicsToPredict--;
	}
}

//============================================================================
//
// 
//
//============================================================================

DCeiling::DCeiling (sector_t *sec)
	: DMovingCeiling (sec)
{
}

DCeiling::DCeiling (sector_t *sec, fixed_t speedDown, fixed_t speedUp, int silent)
	: DMovingCeiling (sec)
{
	m_Crush = -1;
	m_Hexencrush = false;
	m_Speed = m_SpeedDown = speedDown;
	m_SpeedUp = speedUp;
	m_Silent = silent;
	m_Direction = m_OldDirection = 0;
	m_LastInstigator = NULL;
}

player_t* DCeiling::GetLastInstigator()
{
	return m_LastInstigator;
}

void DCeiling::SetLastInstigator( player_t* player )
{
	m_LastInstigator = player;
}

fixed_t DCeiling::GetTopHeight( void )
{
	return ( m_TopHeight );
}

void DCeiling::SetTopHeight( fixed_t TopHeight )
{
	m_TopHeight = TopHeight;
}

fixed_t DCeiling::GetBottomHeight( void )
{
	return ( m_BottomHeight );
}

void DCeiling::SetBottomHeight( fixed_t BottomHeight )
{
	m_BottomHeight = BottomHeight;
}

fixed_t DCeiling::GetSpeed( void )
{
	return ( m_Speed );
}

fixed_t DCeiling::GetSpeedDown( void )
{
	return ( m_SpeedDown );
}

fixed_t DCeiling::GetSpeedUp( void )
{
	return ( m_SpeedUp );
}

void DCeiling::SetSpeed( fixed_t Speed )
{
	m_Speed = Speed;
}

fixed_t DCeiling::GetPosition( void )
{
	return ( m_Sector->ceilingplane.d );
}

LONG DCeiling::GetDirection( void )
{
	return ( m_Direction );
}

void DCeiling::SetPositionAndDirection( LONG lPosition, LONG lDirection )
{
	fixed_t diff = m_Sector->ceilingplane.d - lPosition;
	if (diff > 0)
	{
		MoveCeiling(diff, m_BottomHeight, -1, -1, false);
	}
	else if (diff < 0)
	{
		MoveCeiling(-diff, m_TopHeight, -1, 1, false);
	}

	if (m_Direction != lDirection)
	{
		SN_StopSequence(m_Sector, CHAN_CEILING);
		if (lDirection != 0)
			PlayCeilingSound();

		m_Direction = lDirection;
	}
}

LONG DCeiling::GetOldDirection( void )
{
	return ( m_OldDirection );
}

void DCeiling::SetOldDirection( LONG lOldDirection )
{
	m_OldDirection = lOldDirection;
}

DCeiling::ECeiling DCeiling::GetType( void )
{
	return ( m_Type );
}

void DCeiling::SetType( ECeiling Type )
{
	m_Type = Type;
}

LONG DCeiling::GetTag( void )
{
	return ( m_Tag );
}

void DCeiling::SetTag( LONG lTag )
{
	m_Tag = lTag;
}

LONG DCeiling::GetCrush( void )
{
	return ( m_Crush );
}

void DCeiling::SetCrush( LONG lCrush )
{
	m_Crush = lCrush;
}

bool DCeiling::GetHexencrush( void )
{
	return ( m_Hexencrush );
}

void DCeiling::SetHexencrush( bool Hexencrush )
{
	m_Hexencrush = Hexencrush;
}

LONG DCeiling::GetSilent( void )
{
	return ( m_Silent );
}

void DCeiling::SetSilent( LONG lSilent )
{
	m_Silent = lSilent;
}

//============================================================================
//
// 
//
//============================================================================

DCeiling *DCeiling::Create(sector_t *sec, DCeiling::ECeiling type, line_t *line, int tag, player_t *instigator, 
				   fixed_t speedDown, fixed_t speedUp, fixed_t height,
				   int crush, int silent, int change, bool hexencrush)
{
	fixed_t		targheight = 0;	// Silence, GCC
	DCeiling	*ceiling = NULL;

	// if ceiling already moving, don't start a second function on it
	if (sec->PlaneMoving(sector_t::ceiling))
	{
		if (sec->ceilingdata->IsKindOf(RUNTIME_CLASS(DCeiling)))
		{
			ceiling = barrier_cast<DCeiling*>(sec->ceilingdata);

			//	Reactivate in-stasis ceilings...for certain types.
			// This restarts a crusher after it has been stopped
			if ((type == DCeiling::ceilCrushAndRaise || type == DCeiling::ceilCrushAndRaiseDist) && ceiling->m_Direction == 0)
			{
				ceiling->m_Direction = ceiling->m_OldDirection;
				ceiling->m_LastInstigator = instigator;
				ceiling->PlayCeilingSound ();

				// [geNia] If we are the server, update the clients
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_DoCeiling( ceiling );

				return ceiling;
			}
		}
	}

	// new ceiling thinker
	if (ceiling == NULL)
		ceiling = new DCeiling (sec, speedDown, speedUp, silent);

	if (ceiling->m_Direction != 0)
		return NULL;

	ceiling->m_LastInstigator = instigator;
	vertex_t *spot = sec->lines[0]->v1;

	switch (type)
	{
	case ceilCrushAndRaise:
	case ceilCrushAndRaiseDist:
	case ceilCrushRaiseAndStay:
		ceiling->m_TopHeight = sec->ceilingplane.d;
	case ceilLowerAndCrush:
	case ceilLowerAndCrushDist:
		targheight = sec->FindHighestFloorPoint (&spot);
		if (type == ceilLowerAndCrush)
		{
			targheight += 8*FRACUNIT;
		}
		else if (type == ceilLowerAndCrushDist || type == ceilCrushAndRaiseDist)
		{
			targheight += height;
		}
		ceiling->m_BottomHeight = sec->ceilingplane.PointToDist (spot, targheight);
		ceiling->m_Direction = -1;
		break;

	case ceilRaiseToHighest:
		targheight = sec->FindHighestCeilingSurrounding (&spot);
		ceiling->m_TopHeight = sec->ceilingplane.PointToDist (spot, targheight);
		ceiling->m_Direction = 1;
		break;

	case ceilLowerByValue:
		targheight = sec->ceilingplane.ZatPoint (spot) - height;
		ceiling->m_BottomHeight = sec->ceilingplane.PointToDist (spot, targheight);
		ceiling->m_Direction = -1;
		break;

	case ceilRaiseByValue:
		targheight = sec->ceilingplane.ZatPoint (spot) + height;
		ceiling->m_TopHeight = sec->ceilingplane.PointToDist (spot, targheight);
		ceiling->m_Direction = 1;
		break;

	case ceilMoveToValue:
		{
			int diff = height - sec->ceilingplane.ZatPoint (spot);

			targheight = height;
			if (diff < 0)
			{
				ceiling->m_BottomHeight = sec->ceilingplane.PointToDist (spot, height);
				ceiling->m_Direction = -1;
			}
			else
			{
				ceiling->m_TopHeight = sec->ceilingplane.PointToDist (spot, height);
				ceiling->m_Direction = 1;
			}
		}
		break;

	case ceilLowerToHighestFloor:
		targheight = sec->FindHighestFloorSurrounding (&spot);
		ceiling->m_BottomHeight = sec->ceilingplane.PointToDist (spot, targheight);
		ceiling->m_Direction = -1;
		break;

	case ceilRaiseToHighestFloor:
		targheight = sec->FindHighestFloorSurrounding (&spot);
		ceiling->m_TopHeight = sec->ceilingplane.PointToDist (spot, targheight);
		ceiling->m_Direction = 1;
		break;

	case ceilLowerInstant:
		targheight = sec->ceilingplane.ZatPoint (spot) - height;
		ceiling->m_BottomHeight = sec->ceilingplane.PointToDist (spot, targheight);
		ceiling->m_Direction = -1;
		ceiling->m_Speed = height;
		break;

	case ceilRaiseInstant:
		targheight = sec->ceilingplane.ZatPoint (spot) + height;
		ceiling->m_TopHeight = sec->ceilingplane.PointToDist (spot, targheight);
		ceiling->m_Direction = 1;
		ceiling->m_Speed = height;
		break;

	case ceilLowerToNearest:
		targheight = sec->FindNextLowestCeiling (&spot);
		ceiling->m_BottomHeight = sec->ceilingplane.PointToDist (spot, targheight);
		ceiling->m_Direction = -1;
		break;

	case ceilRaiseToNearest:
		targheight = sec->FindNextHighestCeiling (&spot);
		ceiling->m_TopHeight = sec->ceilingplane.PointToDist (spot, targheight);
		ceiling->m_Direction = 1;
		break;

	case ceilLowerToLowest:
		targheight = sec->FindLowestCeilingSurrounding (&spot);
		ceiling->m_BottomHeight = sec->ceilingplane.PointToDist (spot, targheight);
		ceiling->m_Direction = -1;
		break;

	case ceilRaiseToLowest:
		targheight = sec->FindLowestCeilingSurrounding (&spot);
		ceiling->m_TopHeight = sec->ceilingplane.PointToDist (spot, targheight);
		ceiling->m_Direction = 1;
		break;

	case ceilLowerToFloor:
		targheight = sec->FindHighestFloorPoint (&spot);
		ceiling->m_BottomHeight = sec->ceilingplane.PointToDist (spot, targheight);
		ceiling->m_Direction = -1;
		break;

	case ceilRaiseToFloor:	// [RH] What's this for?
		targheight = sec->FindHighestFloorPoint (&spot);
		ceiling->m_TopHeight = sec->ceilingplane.PointToDist (spot, targheight);
		ceiling->m_Direction = 1;
		break;

	case ceilLowerToHighest:
		targheight = sec->FindHighestCeilingSurrounding (&spot);
		ceiling->m_BottomHeight = sec->ceilingplane.PointToDist (spot, targheight);
		ceiling->m_Direction = -1;
		break;

	case ceilLowerByTexture:
		targheight = sec->ceilingplane.ZatPoint (spot) - sec->FindShortestUpperAround ();
		ceiling->m_BottomHeight = sec->ceilingplane.PointToDist (spot, targheight);
		ceiling->m_Direction = -1;
		break;

	case ceilRaiseByTexture:
		targheight = sec->ceilingplane.ZatPoint (spot) + sec->FindShortestUpperAround ();
		ceiling->m_TopHeight = sec->ceilingplane.PointToDist (spot, targheight);
		ceiling->m_Direction = 1;
		break;

	default:
		break;	// Silence GCC
	}
			
	ceiling->m_Tag = tag;
	ceiling->m_Type = type;
	ceiling->m_Crush = crush;
	ceiling->m_Hexencrush = hexencrush;

	// Do not interpolate instant movement ceilings.
	// Note for ZDoomGL: Check to make sure that you update the sector
	// after the ceiling moves, because it hasn't actually moved yet.
	fixed_t movedist;

	if (ceiling->m_Direction < 0)
	{
		movedist = sec->ceilingplane.d - ceiling->m_BottomHeight;
	}
	else
	{
		movedist = ceiling->m_TopHeight - sec->ceilingplane.d;
	}
	if (ceiling->m_Speed >= movedist)
	{
		ceiling->StopInterpolation();
	}

	// set texture/type change properties
	if (change & 3)		// if a texture change is indicated
	{
		if (change & 4)	// if a numeric model change
		{
			sector_t *modelsec;

			//jff 5/23/98 find model with floor at target height if target
			//is a floor type
			modelsec = (/*type == ceilRaiseToHighest ||*/
				   type == ceilRaiseToFloor ||
				   /*type == ceilLowerToHighest ||*/
				   type == ceilLowerToFloor) ?
				sec->FindModelFloorSector (targheight) :
				sec->FindModelCeilingSector (targheight);
			if (modelsec != NULL)
			{
				ceiling->m_Texture = modelsec->GetTexture(sector_t::ceiling);
				switch (change & 3)
				{
					case 1:		// type is zeroed
						ceiling->m_NewSpecial = 0;
						ceiling->m_Type = genCeilingChg0;
						break;
					case 2:		// type is copied
						ceiling->m_NewSpecial = sec->special;
						ceiling->m_Type = genCeilingChgT;
						break;
					case 3:		// type is left alone
						ceiling->m_Type = genCeilingChg;
						break;
				}
			}
		}
		else if (line)	// else if a trigger model change
		{
			ceiling->m_Texture = line->frontsector->GetTexture(sector_t::ceiling);
			switch (change & 3)
			{
				case 1:		// type is zeroed
					ceiling->m_NewSpecial = 0;
					ceiling->m_Type = genCeilingChg0;
					break;
				case 2:		// type is copied
					ceiling->m_NewSpecial = line->frontsector->special;
					ceiling->m_Type = genCeilingChgT;
					break;
				case 3:		// type is left alone
					ceiling->m_Type = genCeilingChg;
					break;
			}
		}
	}

	ceiling->PlayCeilingSound ();

	// [geNia] If we are the server, update the clients
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DoCeiling( ceiling );

	return ceiling;
}

//============================================================================
//
// EV_DoCeiling
// Move a ceiling up/down and all around!
//
// [RH] Added tag, speed, speed2, height, crush, silent, change params
//
//============================================================================

bool EV_DoCeiling(DCeiling::ECeiling type, line_t *line,
	int tag, fixed_t speed, fixed_t speed2, fixed_t height,
	int crush, int silent, int change, bool hexencrush)
{
	return EV_DoCeiling(type, line,
		tag, NULL, speed, speed2, height,
		crush, silent, change, hexencrush);
}

bool EV_DoCeiling (DCeiling::ECeiling type, line_t *line,
				   int tag, player_t *instigator, fixed_t speed, fixed_t speed2, fixed_t height,
				   int crush, int silent, int change, bool hexencrush)
{
	if (CLIENT_PREDICT_IsPredicting())
		return false;

	int 		secnum;
	bool 		rtn;
	sector_t*	sec;
		
	rtn = false;

	// check if a manual trigger, if so do just the sector on the backside
	if (tag == 0)
	{
		if (!line || !(sec = line->backsector))
			return rtn;
		secnum = (int)(sec-sectors);
		// [RH] Hack to let manual crushers be retriggerable, too
		tag ^= secnum | 0x1000000;
		return !!DCeiling::Create(sec, type, line, tag, instigator, speed, speed2, height, crush, silent, change, hexencrush);
	}
	
	secnum = -1;
	// affects all sectors with the same tag as the linedef
	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0)
	{
		rtn |= !!DCeiling::Create(&sectors[secnum], type, line, tag, instigator, speed, speed2, height, crush, silent, change, hexencrush);
	}
	return rtn;
}

//============================================================================
//
// EV_CeilingCrushStop
// Stop a ceiling from crushing!
// [RH] Passed a tag instead of a line and rewritten to use a list
//
//============================================================================

bool EV_CeilingCrushStop (int tag)
{
	return EV_CeilingCrushStop(tag, NULL);
}

bool EV_CeilingCrushStop (int tag, player_t *instigator)
{
	if (CLIENT_PREDICT_IsPredicting())
		return false;

	bool rtn = false;
	DCeiling *scan;
	TThinkerIterator<DCeiling> iterator;

	while ( (scan = iterator.Next ()) )
	{
		if (scan->m_Tag == tag && scan->m_Direction != 0)
		{
			scan->m_LastInstigator = instigator;

			SN_StopSequence (scan->m_Sector, CHAN_CEILING);
			scan->m_OldDirection = scan->m_Direction;
			scan->m_Direction = 0;		// in-stasis;
			rtn = true;

			// [geNia] If we are the server, update the clients
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_DoCeiling( scan );
		}
	}

	return rtn;
}

//*****************************************************************************
//
DCeiling *P_GetCeilingBySectorNum( LONG sectorNum )
{
	DCeiling	*pCeiling;

	TThinkerIterator<DCeiling>		Iterator;

	while (( pCeiling = Iterator.Next( )))
	{
		if ( pCeiling->GetSector()->sectornum == sectorNum )
			return ( pCeiling );
	}

	return ( NULL );
}
