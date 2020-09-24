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
		<< m_Hexencrush
		// [BC]
		<< m_lCeilingID;
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
		
		// [BC] Don't need to do anything more here if we're a client.
		if ( NETWORK_InClientMode() )
			break;

		if (res == pastdest)
		{
			// [BC] If the sector has reached its destination, this is probably a good time to verify all the clients
			// have the correct floor/ceiling height for this sector.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
				if ( m_Sector->floorOrCeiling == 0 )
					SERVERCOMMANDS_SetSectorFloorPlane( ULONG( m_Sector - sectors ));
				else
					SERVERCOMMANDS_SetSectorCeilingPlane( ULONG( m_Sector - sectors ));
			}

			switch (m_Type)
			{
			case ceilCrushAndRaise:
			case ceilCrushAndRaiseDist:
				m_Direction = -1;
				m_Speed = m_Speed1;
				if (!SN_IsMakingLoopingSound (m_Sector))
					PlayCeilingSound ();

				// [BC] If we're the server, send out a bunch of updates to clients.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					// Tell clients to change the direction of the ceiling.
					SERVERCOMMANDS_ChangeCeilingDirection( m_lCeilingID, m_Direction );

					// Tell clients to change the speed of the ceiling.
					SERVERCOMMANDS_ChangeCeilingSpeed( m_lCeilingID, m_Speed );

					// Potentially tell clients to stop playing a ceiling sound.
					if ( SN_IsMakingLoopingSound( m_Sector ) == false )
						SERVERCOMMANDS_PlayCeilingSound( m_lCeilingID );
				}
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
					SERVERCOMMANDS_SetSectorFlat( m_Sector - sectors );

				// [BB] Also, mark this sector as having its flat changed.
				m_Sector->bFlatChange = true;

				// fall through
			default:

				// [BC] If we're the server, tell the client to destroy this ceiling.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					SERVERCOMMANDS_StopSectorSequence( m_Sector );
					SERVERCOMMANDS_DestroyCeiling( m_lCeilingID );
				}

				SN_StopSequence (m_Sector, CHAN_CEILING);
				Destroy ();
				break;
			}
		}
		break;
		
	case -1:
		// DOWN
		res = MoveCeiling (m_Speed, m_BottomHeight, m_Crush, m_Direction, m_Hexencrush);
		
		// [BC] Don't need to do anything more here if we're a client.
		if ( NETWORK_InClientMode() )
			break;

		if (res == pastdest)
		{
			// [BC] If the sector has reached its destination, this is probably a good time to verify all the clients
			// have the correct floor/ceiling height for this sector.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
				if ( m_Sector->floorOrCeiling == 0 )
					SERVERCOMMANDS_SetSectorFloorPlane( ULONG( m_Sector - sectors ));
				else
					SERVERCOMMANDS_SetSectorCeilingPlane( ULONG( m_Sector - sectors ));
			}

			switch (m_Type)
			{
			case ceilCrushAndRaise:
			case ceilCrushAndRaiseDist:
			case ceilCrushRaiseAndStay:
				m_Speed = m_Speed2;
				m_Direction = 1;
				if (!SN_IsMakingLoopingSound (m_Sector))
					PlayCeilingSound ();

				// [BC] If we're the server, send out a bunch of updates to clients.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					// Tell clients to change the direction of the ceiling.
					SERVERCOMMANDS_ChangeCeilingDirection( m_lCeilingID, m_Direction );

					// Tell clients to change the speed of the ceiling.
					SERVERCOMMANDS_ChangeCeilingSpeed( m_lCeilingID, m_Speed );

					// Potentially tell clients to stop playing a ceiling sound.
					if ( SN_IsMakingLoopingSound( m_Sector ) == false )
						SERVERCOMMANDS_PlayCeilingSound( m_lCeilingID );
				}
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
					SERVERCOMMANDS_SetSectorFlat( m_Sector - sectors );

				// [BB] Also, mark this sector as having its flat changed.
				m_Sector->bFlatChange = true;

				// fall through
			default:

				// [BC] If we're the server, tell the client to destroy this ceiling.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					SERVERCOMMANDS_StopSectorSequence( m_Sector );
					SERVERCOMMANDS_DestroyCeiling( m_lCeilingID );
				}

				SN_StopSequence (m_Sector, CHAN_CEILING);
				Destroy ();
				break;
			}
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

						// [BC] If we're the server, tell clients to change the ceiling's speed.
						if ( NETWORK_GetState( ) == NETSTATE_SERVER )
							SERVERCOMMANDS_ChangeCeilingSpeed( m_lCeilingID, m_Speed );
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
	SERVERCOMMANDS_DoCeiling( m_Type, m_Sector, m_Direction, m_BottomHeight, m_TopHeight, m_Speed, m_Crush, m_Hexencrush, m_Silent, m_lCeilingID, ulClient, SVCF_ONLYTHISCLIENT );
}

//============================================================================
//
// 
//
//============================================================================

DCeiling::DCeiling (sector_t *sec)
	: DMovingCeiling (sec)
{
	// [BB]
	m_lCeilingID = -1;
}

DCeiling::DCeiling (sector_t *sec, fixed_t speed1, fixed_t speed2, int silent)
	: DMovingCeiling (sec)
{
	m_Crush = -1;
	m_Hexencrush = false;
	m_Speed = m_Speed1 = speed1;
	m_Speed2 = speed2;
	m_Silent = silent;
	// [BB]
	m_lCeilingID = -1;
}

LONG DCeiling::GetID( void )
{
	return ( m_lCeilingID );
}

void DCeiling::SetID( LONG lID )
{
	m_lCeilingID = lID;
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

LONG DCeiling::GetDirection( void )
{
	return ( m_Direction );
}

void DCeiling::SetDirection( LONG lDirection )
{
	m_Direction = lDirection;
}

DCeiling::ECeiling DCeiling::GetType( void )
{
	return ( m_Type );
}

void DCeiling::SetType( ECeiling Type )
{
	m_Type = Type;
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

//============================================================================
//
// 
//
//============================================================================

DCeiling *DCeiling::Create(sector_t *sec, DCeiling::ECeiling type, line_t *line, int tag, 
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
	DCeiling *ceiling = new DCeiling (sec, speed, speed2, silent);
	vertex_t *spot = sec->lines[0]->v1;

	// [BC] If we're not a client, assign a network ID to the ceiling.
	if ( NETWORK_InClientMode() == false ) 
		ceiling->m_lCeilingID = P_GetFirstFreeCeilingID( );

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

	// [BC] If we're the server, tell clients to create a ceiling.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DoCeiling( type, sec, ceiling->m_Direction, ceiling->m_BottomHeight, ceiling->m_TopHeight, ceiling->m_Speed, ceiling->m_Crush, ceiling->m_Hexencrush, ceiling->m_Silent, ceiling->m_lCeilingID );

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

	// [BC] If we're the server, tell clients to play the ceiling sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_PlayCeilingSound( ceiling->m_lCeilingID );

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

bool EV_DoCeiling (DCeiling::ECeiling type, line_t *line,
				   int tag, fixed_t speed, fixed_t speed2, fixed_t height,
				   int crush, int silent, int change, bool hexencrush)
{
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
		P_ActivateInStasisCeiling (tag);
		return !!DCeiling::Create(sec, type, line, tag, speed, speed2, height, crush, silent, change, hexencrush);
	}
	
	//	Reactivate in-stasis ceilings...for certain types.
	// This restarts a crusher after it has been stopped
	if (type == DCeiling::ceilCrushAndRaise || type == DCeiling::ceilCrushAndRaiseDist)
	{
		P_ActivateInStasisCeiling (tag);
	}

	secnum = -1;
	// affects all sectors with the same tag as the linedef
	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0)
	{
		rtn |= !!DCeiling::Create(&sectors[secnum], type, line, tag, speed, speed2, height, crush, silent, change, hexencrush);
	}
	return rtn;
}


//============================================================================
//
// Restart a ceiling that's in-stasis
// [RH] Passed a tag instead of a line and rewritten to use a list
//
//============================================================================

void P_ActivateInStasisCeiling (int tag)
{
	DCeiling *scan;
	TThinkerIterator<DCeiling> iterator;

	while ( (scan = iterator.Next ()) )
	{
		if (scan->m_Tag == tag && scan->m_Direction == 0)
		{
			scan->m_Direction = scan->m_OldDirection;
			scan->PlayCeilingSound ();

			// [BC] If we're the server, tell clients to change the ceiling direction, and
			// play the ceiling sound.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
				SERVERCOMMANDS_ChangeCeilingDirection( scan->GetID( ), scan->GetDirection( ));
				SERVERCOMMANDS_PlayCeilingSound( scan->GetID( ));
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

bool EV_CeilingCrushStop (int tag)
{
	bool rtn = false;
	DCeiling *scan;
	TThinkerIterator<DCeiling> iterator;

	while ( (scan = iterator.Next ()) )
	{
		if (scan->m_Tag == tag && scan->m_Direction != 0)
		{
			// [BC] If we're stopping, this is probably a good time to verify all the clients
			// have the correct floor/ceiling height for this sector.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
				if ( scan->m_Sector->floorOrCeiling == 0 )
					SERVERCOMMANDS_SetSectorFloorPlane( ULONG( scan->m_Sector - sectors ));
				else
					SERVERCOMMANDS_SetSectorCeilingPlane( ULONG( scan->m_Sector - sectors ));

				// Tell clients to stop the floor's sound sequence.
				SERVERCOMMANDS_StopSectorSequence( scan->m_Sector );
			}

			SN_StopSequence (scan->m_Sector, CHAN_CEILING);
			scan->m_OldDirection = scan->m_Direction;
			scan->m_Direction = 0;		// in-stasis;
			rtn = true;

			// [BB] Also tell the updated direction to the clients.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_ChangeCeilingDirection( scan->GetID( ), scan->GetDirection( ));
		}
	}

	return rtn;
}

//*****************************************************************************
//
DCeiling *P_GetCeilingByID( LONG lID )
{
	DCeiling	*pCeiling;

	TThinkerIterator<DCeiling>		Iterator;

	while (( pCeiling = Iterator.Next( )))
	{
		if ( pCeiling->GetID( ) == lID )
			return ( pCeiling );
	}

	return ( NULL );
}

//*****************************************************************************
//
LONG P_GetFirstFreeCeilingID( void )
{
	return NETWORK_GetFirstFreeID<DCeiling>();
}
