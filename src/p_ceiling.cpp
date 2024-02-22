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
		<< m_Speed1
		<< m_Speed2
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
				m_Speed = m_Speed1;
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

				// [TP] If we're the server, tell the client to change the ceiling texture.
				if ( NETWORK_GetState() == NETSTATE_SERVER )
				{
					if ( m_LastInstigator )
						SERVERCOMMANDS_SetSectorFlat( ULONG( m_Sector - sectors ), ULONG(m_LastInstigator - players), SVCF_SKIPTHISCLIENT);
					else
						SERVERCOMMANDS_SetSectorFlat( ULONG( m_Sector - sectors ));
				}

				// [BB] Also, mark this sector as having its flat changed.
				m_Sector->bFlatChange = true;

				// fall through
			default:
				SN_StopSequence (m_Sector, CHAN_CEILING);
				Destroy ();
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
				m_Speed = m_Speed2;
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

				// [TP] If we're the server, tell the client to change the ceiling texture.
				if ( NETWORK_GetState() == NETSTATE_SERVER )
				{
					if ( m_LastInstigator )
						SERVERCOMMANDS_SetSectorFlat( ULONG( m_Sector - sectors ), ULONG(m_LastInstigator - players), SVCF_SKIPTHISCLIENT);
					else
						SERVERCOMMANDS_SetSectorFlat( ULONG( m_Sector - sectors ));
				}

				// [BB] Also, mark this sector as having its flat changed.
				m_Sector->bFlatChange = true;

				// fall through
			default:

				SN_StopSequence (m_Sector, CHAN_CEILING);
				Destroy ();
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
					if (m_Speed1 == FRACUNIT && m_Speed2 == FRACUNIT)
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

//============================================================================
//
// 
//
//============================================================================

DCeiling::DCeiling (sector_t *sec)
	: DMovingCeiling (sec)
{
}

DCeiling::DCeiling (player_t *instigator, sector_t *sec, fixed_t speed1, fixed_t speed2, int silent)
	: DMovingCeiling (sec)
{
	m_Crush = -1;
	m_Hexencrush = false;
	m_Speed = m_Speed1 = speed1;
	m_Speed2 = speed2;
	m_Silent = silent;
	m_BottomHeight = 0;
	m_TopHeight = 0;
	m_OldDirection = 0;
	m_LastInstigator = instigator;
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

void DCeiling::SetSpeed( fixed_t Speed )
{
	m_Speed = Speed;
}

fixed_t DCeiling::GetSpeed1( void )
{
	return ( m_Speed1 );
}

void DCeiling::SetSpeed1( fixed_t Speed )
{
	m_Speed1 = Speed;
}

fixed_t DCeiling::GetSpeed2( void )
{
	return ( m_Speed2 );
}

void DCeiling::SetSpeed2( fixed_t Speed )
{
	m_Speed2 = Speed;
}

fixed_t DCeiling::GetPosition( void )
{
	return ( m_Sector->ceilingplane.d );
}

int DCeiling::GetDirection( void )
{
	return ( m_Direction );
}

void DCeiling::SetDirection( int Direction )
{
	if (m_Direction != Direction)
	{
		if (Direction != 0)
			PlayCeilingSound();
		else
			SN_StopSequence(m_Sector, CHAN_CEILING);

		m_Direction = Direction;
	}
}

int DCeiling::GetOldDirection( void )
{
	return ( m_OldDirection );
}

void DCeiling::SetOldDirection( int OldDirection )
{
	m_OldDirection = OldDirection;
}

DCeiling::ECeiling DCeiling::GetType( void )
{
	return ( m_Type );
}

void DCeiling::SetType( ECeiling Type )
{
	m_Type = Type;
}

int DCeiling::GetTag( void )
{
	return ( m_Tag );
}

void DCeiling::SetTag( int Tag )
{
	m_Tag = Tag;
}

int DCeiling::GetCrush( void )
{
	return ( m_Crush );
}

void DCeiling::SetCrush( int Crush )
{
	m_Crush = Crush;
}

bool DCeiling::GetHexencrush( void )
{
	return ( m_Hexencrush );
}

void DCeiling::SetHexencrush( bool Hexencrush )
{
	m_Hexencrush = Hexencrush;
}

int DCeiling::GetSilent( void )
{
	return ( m_Silent );
}

void DCeiling::SetSilent( int Silent )
{
	m_Silent = Silent;
}

FTextureID DCeiling::GetTexture( void )
{
	return ( m_Texture );
}

void DCeiling::SetTexture(FTextureID Texture )
{
	m_Texture = Texture;
}

//============================================================================
//
// 
//
//============================================================================

DCeiling *DCeiling::Create(player_t *instigator, sector_t *sec, DCeiling::ECeiling type, line_t *line, int tag,
				   fixed_t speed, fixed_t speed2, fixed_t height,
				   int crush, int silent, int change, bool hexencrush)
{
	fixed_t		targheight = 0;	// Silence, GCC

	// if ceiling already moving, don't start a second function on it
	if (sec->PlaneMoving(sector_t::ceiling))
	{
		return NULL;
	}

	// new door thinker
	DCeiling *ceiling = new DCeiling (instigator, sec, speed, speed2, silent);
	vertex_t *spot = sec->lines[0]->v1;

	switch (type)
	{
	case ceilCrushAndRaise:
	case ceilCrushAndRaiseDist:
	case ceilCrushRaiseAndStay:
		ceiling->m_TopHeight = sec->ceilingplane.d;
		// fall through
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

	// [geNia] If we are the server, update the clients
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DoCeiling( ceiling );

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

bool EV_DoCeiling (player_t *instigator, DCeiling::ECeiling type, line_t *line,
				   int tag, fixed_t speed, fixed_t speed2, fixed_t height,
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
		P_ActivateInStasisCeiling (instigator, tag);
		return !!DCeiling::Create(instigator, sec, type, line, tag, speed, speed2, height, crush, silent, change, hexencrush);
	}

	//	Reactivate in-stasis ceilings...for certain types.
	// This restarts a crusher after it has been stopped
	if (type == DCeiling::ceilCrushAndRaise || type == DCeiling::ceilCrushAndRaiseDist)
	{
		P_ActivateInStasisCeiling (instigator, tag);
	}

	secnum = -1;
	// affects all sectors with the same tag as the linedef
	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0)
	{
		rtn |= !!DCeiling::Create(instigator, &sectors[secnum], type, line, tag, speed, speed2, height, crush, silent, change, hexencrush);
	}
	return rtn;
}


//============================================================================
//
// Restart a ceiling that's in-stasis
// [RH] Passed a tag instead of a line and rewritten to use a list
//
//============================================================================

void P_ActivateInStasisCeiling (player_t *instigator, int tag)
{
	DCeiling *scan;
	TThinkerIterator<DCeiling> iterator;

	while ( (scan = iterator.Next ()) )
	{
		if (scan->m_Tag == tag && scan->m_Direction == 0)
		{
			scan->m_LastInstigator = instigator;
			scan->m_Direction = scan->m_OldDirection;
			scan->PlayCeilingSound ();

			// [BC] If we're the server, tell clients to change the ceiling direction, and
			// play the ceiling sound.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
				SERVERCOMMANDS_DoCeiling ( scan );
			}
		}
	}
}

//============================================================================
//
// EV_CeilingCrushStop
// Stop a ceiling from crushing!
// [RH] Passed a tag instead of a line and rewritten to use a list
//
//============================================================================

bool EV_CeilingCrushStop (player_t *instigator, int tag)
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

//============================================================================
//
// EV_CeilingStop
// Stop a ceiling
//
//============================================================================

bool EV_CeilingStop (int tag)
{
	DCeiling *scan;
	TThinkerIterator<DCeiling> iterator;
	while ( (scan = iterator.Next ()) )
	{
		if (scan->m_Tag == tag)
		{
			SN_StopSequence (scan->m_Sector, CHAN_CEILING);
			scan->m_Sector->ceilingdata = NULL;
			scan->Destroy ();

			// [geNia] If we are the server, update the clients
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_DoCeiling( scan );
		}
	}
	return true;
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
