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
//		Plats (i.e. elevator platforms) code, raising/lowering.
//
//-----------------------------------------------------------------------------

#include "i_system.h"
#include "m_random.h"
#include "doomdef.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "s_sndseq.h"
#include "doomstat.h"
#include "r_state.h"
#include "gi.h"
#include "farchive.h"
// [BB] New #includes.
#include "network.h"
#include "sv_commands.h"
#include "cl_demo.h"
#include "cl_main.h"

static FRandom pr_doplat ("DoPlat");

IMPLEMENT_CLASS (DPlat)

inline FArchive &operator<< (FArchive &arc, DPlat::EPlatType &type)
{
	BYTE val = (BYTE)type;
	arc << val;
	type = (DPlat::EPlatType)val;
	return arc;
}
inline FArchive &operator<< (FArchive &arc, DPlat::EPlatState &state)
{
	BYTE val = (BYTE)state;
	arc << val;
	state = (DPlat::EPlatState)val;
	return arc;
}

DPlat::DPlat ()
{
}

void DPlat::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Speed
		<< m_Low
		<< m_High
		<< m_Wait
		<< m_Count
		<< m_Status
		<< m_OldStatus
		<< m_Crush
		<< m_Tag
		<< m_Type;
}

void DPlat::PlayPlatSound (const char *sound)
{
	if (m_Sector->seqType >= 0)
	{
		SN_StartSequence (m_Sector, CHAN_FLOOR, m_Sector->seqType, SEQ_PLATFORM, 0);
	}
	else if (m_Sector->SeqName != NAME_None)
	{
		SN_StartSequence (m_Sector, CHAN_FLOOR, m_Sector->SeqName, 0);
	}
	else
	{
		SN_StartSequence (m_Sector, CHAN_FLOOR, sound, 0);
	}
}

//
// Move a plat up and down
//
void DPlat::Tick ()
{
	EResult res;
		
	switch (m_Status)
	{
	case up:
		res = MoveFloor (m_Speed, m_High, m_Crush, 1, false);
		
		if (res == crushed && (m_Crush == -1))
		{
			m_Count = m_Wait;
			m_Status = down;
			PlayPlatSound ("Platform");

			// [BC] Tell clients that this plat is changing directions.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_DoPlat( this );
		}
		else if (res == pastdest)
		{
			SN_StopSequence (m_Sector, CHAN_FLOOR);

			if (m_Type != platToggle)
			{
				m_Count = m_Wait;
				m_Status = waiting;

				switch (m_Type)
				{
					case platRaiseAndStayLockout:
						// Instead of keeping the dead thinker like Heretic did let's 
						// better use a flag to avoid problems elsewhere. For example,
						// keeping the thinker would make tagwait wait indefinitely.
						m_Sector->planes[sector_t::floor].Flags |= PLANEF_BLOCKED; 
						// fall through
					case platRaiseAndStay:
					case platDownByValue:
					case platDownWaitUpStay:
					case platDownWaitUpStayStone:
					case platUpByValueStay:
					case platDownToNearestFloor:
					case platDownToLowestCeiling:
						Destroy ();
						break;
					default:
						break;
				}
			}
			else
			{
				m_OldStatus = m_Status;		//jff 3/14/98 after action wait  
				m_Status = in_stasis;		//for reactivation of toggle
			}

			// [BC] If the sector has reached its destination, this is probably a good time to verify all the clients
			// have the correct floor/ceiling height for this sector.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_DoPlat( this );
		}
		break;
	case down:
		res = MoveFloor (m_Speed, m_Low, -1, -1, false);

		if (res == pastdest)
		{
			SN_StopSequence (m_Sector, CHAN_FLOOR);

			// if not an instant toggle, start waiting
			if (m_Type != platToggle)		//jff 3/14/98 toggle up down
			{								// is silent, instant, no waiting
				m_Count = m_Wait;
				m_Status = waiting;

				switch (m_Type)
				{
					case platUpWaitDownStay:
					case platUpNearestWaitDownStay:
					case platUpByValue:
						Destroy ();
						break;
				}
			}
			else
			{	// instant toggles go into stasis awaiting next activation
				m_OldStatus = m_Status;		//jff 3/14/98 after action wait  
				m_Status = in_stasis;		//for reactivation of toggle
			}

			// [BC] If the sector has reached its destination, this is probably a good time to verify all the clients
			// have the correct floor/ceiling height for this sector.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_DoPlat( this );

		}
		else if (res == crushed && m_Crush < 0 && m_Type != platToggle)
		{
			m_Status = up;
			m_Count = m_Wait;
			PlayPlatSound ("Platform");
		}

		//jff 1/26/98 remove the plat if it bounced so it can be tried again
		//only affects plats that raise and bounce

		// remove the plat if it's a pure raise type
		switch (m_Type)
		{
			case platUpByValueStay:
			case platRaiseAndStay:
			case platRaiseAndStayLockout:
				Destroy ();
			default:
				break;
		}

		break;
		
	case waiting:

		if (m_Count > 0 && !--m_Count)
		{
			if (m_Sector->floorplane.d == m_Low)
				m_Status = up;
			else
				m_Status = down;

			if (m_Type == platToggle)
			{
				SN_StartSequence (m_Sector, CHAN_FLOOR, "Silence", 0);
			}
			else
			{
				PlayPlatSound ("Platform");
			}
			
			// [BC] If we're the server, tell clients that that status is changing.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_DoPlat( this );
		}
		break;

	case in_stasis:
		break;
	}
}

// [BC]
void DPlat::UpdateToClient( ULONG ulClient )
{
	SERVERCOMMANDS_DoPlat( this, ulClient, SVCF_ONLYTHISCLIENT );
}

DPlat::DPlat (sector_t *sector)
	: DMovingFloor (sector)
{
	m_LastInstigator = NULL;
}

// [BC]
fixed_t DPlat::GetLow( void )
{
	return ( m_Low );
}

// [BC]
void DPlat::SetLow( fixed_t Low )
{
	m_Low = Low;
}

// [BC]
fixed_t DPlat::GetHigh( void )
{
	return ( m_High );
}

// [BC]
void DPlat::SetHigh( fixed_t High )
{
	m_High = High;
}

// [BC]
fixed_t DPlat::GetPosition( void )
{
	return ( m_Sector->floorplane.d );
}

// [BC]
DPlat::EPlatState DPlat::GetStatus( void )
{
	return ( m_Status );
}

// [BC]
void DPlat::SetStatus( LONG lStatus )
{
	if ( ( m_Status != (DPlat::EPlatState) lStatus ) && ( m_Type != platToggle ) )
	{
		PlayPlatSound("Platform");
	}

	m_Status = (DPlat::EPlatState) lStatus;
}

// [BC]
DPlat::EPlatState DPlat::GetOldStatus( void )
{
	return ( m_OldStatus );
}

// [BC]
void DPlat::SetOldStatus( LONG lOldStatus )
{
	m_OldStatus = (DPlat::EPlatState)lOldStatus;
}

// [BC]
DPlat::EPlatType DPlat::GetType( void )
{
	return ( m_Type );
}

// [BC]
void DPlat::SetType( EPlatType Type )
{
	m_Type = Type;
}

// [BC]
LONG DPlat::GetSpeed( void )
{
	return ( m_Speed );
}

// [BC]
void DPlat::SetSpeed( LONG lSpeed )
{
	m_Speed = lSpeed;
}

// [BC]
LONG DPlat::GetWait( void )
{
	return ( m_Wait );
}

// [BC]
void DPlat::SetWait( LONG lWait )
{
	m_Wait = lWait;
}

// [BC]
LONG DPlat::GetCrush( void )
{
	return ( m_Crush );
}

// [BC]
void DPlat::SetCrush( LONG lCrush )
{
	m_Crush = lCrush;
}

// [BC]
LONG DPlat::GetCount( void )
{
	return ( m_Count );
}

// [BC]
void DPlat::SetCount( LONG lCount )
{
	m_Count = lCount;
}

// [BC]
LONG DPlat::GetTag( void )
{
	return ( m_Tag );
}

// [BC]
void DPlat::SetTag( LONG lTag )
{
	m_Tag = lTag;
}


//
// Do Platforms
//	[RH] Changed amount to height and added delay,
//		 lip, change, tag, and speed parameters.
//
bool EV_DoPlat (player_t *instigator, int tag, line_t *line, DPlat::EPlatType type, int height,
				int speed, int delay, int lip, int change)
{
	if (CLIENT_PREDICT_IsPredicting())
		return false;

	DPlat *plat;
	int secnum;
	sector_t *sec;
	bool rtn = false;
	bool manual = false;
	fixed_t newheight = 0;
	vertex_t *spot;

	// [RH] If tag is zero, use the sector on the back side
	//		of the activating line (if any).
	if (!tag)
	{
		if (!line || !(sec = line->backsector))
			return false;
		secnum = (int)(sec - sectors);
		manual = true;
		goto manual_plat;
	}

	//	Activate all <type> plats that are in_stasis
	switch (type)
	{
	case DPlat::platToggle:
		rtn = true;
		// fall through
	case DPlat::platPerpetualRaise:
		P_ActivateInStasis (instigator, tag);
		break;

	default:
		break;
	}

	secnum = -1;
	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0)
	{
		sec = &sectors[secnum];

manual_plat:
		if (sec->PlaneMoving(sector_t::floor))
		{
			if (!manual)
				continue;
			else
				return false;
		}

		// Find lowest & highest floors around sector
		rtn = true;
		plat = new DPlat (sec);

		plat->m_Type = type;
		plat->m_Crush = -1;
		plat->m_Tag = tag;
		plat->m_Speed = speed;
		plat->m_Wait = delay;
		plat->m_LastInstigator = instigator;

		//jff 1/26/98 Avoid raise plat bouncing a head off a ceiling and then
		//going down forever -- default lower to plat height when triggered
		plat->m_Low = sec->floorplane.d;

		if (change)
		{
			if (line)
			{
				sec->SetTexture(sector_t::floor, line->sidedef[0]->sector->GetTexture(sector_t::floor));

				// [BC] Update clients about this flat change.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SetSectorFlat( ULONG( sec - sectors ));

				// [BC] Also, mark this sector as having its flat changed.
				sec->bFlatChange = true;
			}
			if (change == 1)
				sec->special &= SECRET_MASK;	// Stop damage and other stuff, if any
		}

		switch (type)
		{
		case DPlat::platRaiseAndStay:
		case DPlat::platRaiseAndStayLockout:
			newheight = sec->FindNextHighestFloor (&spot);
			plat->m_High = sec->floorplane.PointToDist (spot, newheight);
			plat->m_Low = sec->floorplane.d;
			plat->m_Status = DPlat::up;
			plat->PlayPlatSound ("Floor");
			sec->special &= SECRET_MASK;		// NO MORE DAMAGE, IF APPLICABLE
			break;

		case DPlat::platUpByValue:
		case DPlat::platUpByValueStay:
			newheight = sec->floorplane.ZatPoint (0, 0) + height;
			plat->m_High = sec->floorplane.PointToDist (0, 0, newheight);
			plat->m_Low = sec->floorplane.d;
			plat->m_Status = DPlat::up;
			plat->PlayPlatSound ("Floor");
			break;
		
		case DPlat::platDownByValue:
			newheight = sec->floorplane.ZatPoint (0, 0) - height;
			plat->m_Low = sec->floorplane.PointToDist (0, 0, newheight);
			plat->m_High = sec->floorplane.d;
			plat->m_Status = DPlat::down;

			// [BC] I think this should be the platform sound, but I could be wrong.
			plat->PlayPlatSound ("Platform");
			break;

		case DPlat::platDownWaitUpStay:
		case DPlat::platDownWaitUpStayStone:
			newheight = sec->FindLowestFloorSurrounding (&spot) + lip*FRACUNIT;
			plat->m_Low = sec->floorplane.PointToDist (spot, newheight);

			if (plat->m_Low < sec->floorplane.d)
				plat->m_Low = sec->floorplane.d;

			plat->m_High = sec->floorplane.d;
			plat->m_Status = DPlat::down;
			plat->PlayPlatSound (type == DPlat::platDownWaitUpStay ? "Platform" : "Floor");
			break;
		
		case DPlat::platUpNearestWaitDownStay:
			newheight = sec->FindNextHighestFloor (&spot);
			// Intentional fall-through

		case DPlat::platUpWaitDownStay:
			if (type == DPlat::platUpWaitDownStay)
			{
				newheight = sec->FindHighestFloorSurrounding (&spot);
			}
			plat->m_High = sec->floorplane.PointToDist (spot, newheight);
			plat->m_Low = sec->floorplane.d;

			if (plat->m_High > sec->floorplane.d)
				plat->m_High = sec->floorplane.d;

			plat->m_Status = DPlat::up;
			plat->PlayPlatSound ("Platform");
			break;

		case DPlat::platPerpetualRaise:
			newheight = sec->FindLowestFloorSurrounding (&spot) + lip*FRACUNIT;
			plat->m_Low =  sec->floorplane.PointToDist (spot, newheight);

			if (plat->m_Low < sec->floorplane.d)
				plat->m_Low = sec->floorplane.d;

			newheight = sec->FindHighestFloorSurrounding (&spot);
			plat->m_High =  sec->floorplane.PointToDist (spot, newheight);

			if (plat->m_High > sec->floorplane.d)
				plat->m_High = sec->floorplane.d;

			plat->m_Status = pr_doplat() & 1 ? DPlat::up : DPlat::down;
			plat->PlayPlatSound ("Platform");
			break;

		case DPlat::platToggle:	//jff 3/14/98 add new type to support instant toggle
			plat->m_Crush = 10;	//jff 3/14/98 crush anything in the way

			// set up toggling between ceiling, floor inclusive
			newheight = sec->FindLowestCeilingPoint (&spot);
			plat->m_Low = sec->floorplane.PointToDist (spot, newheight);
			plat->m_High = sec->floorplane.d;
			plat->m_Status = DPlat::down;
			SN_StartSequence (sec, CHAN_FLOOR, "Silence", 0);
			break;

		case DPlat::platDownToNearestFloor:
			newheight = sec->FindNextLowestFloor (&spot) + lip*FRACUNIT;
			plat->m_Low = sec->floorplane.PointToDist (spot, newheight);
			plat->m_Status = DPlat::down;
			plat->m_High = sec->floorplane.d;
			plat->PlayPlatSound ("Platform");
			break;

		case DPlat::platDownToLowestCeiling:
			newheight = sec->FindLowestCeilingSurrounding (&spot);
		    plat->m_Low = sec->floorplane.PointToDist (spot, newheight);
			plat->m_High = sec->floorplane.d;

			if (plat->m_Low < sec->floorplane.d)
				plat->m_Low = sec->floorplane.d;

			plat->m_Status = DPlat::down;
			plat->PlayPlatSound ("Platform");
			break;

		default:
			break;
		}

		// [BC] If we're the server, tell clients to create the plat.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_DoPlat( plat );

		if (manual)
			return rtn;
	}
	return rtn;
}

void DPlat::Reactivate ()
{
	if (m_Type == platToggle)	//jff 3/14/98 reactivate toggle type
		m_Status = m_OldStatus == up ? down : up;
	else
		m_Status = m_OldStatus;
}

void P_ActivateInStasis (player_t *instigator, int tag)
{
	DPlat *scan;
	TThinkerIterator<DPlat> iterator;

	while ( (scan = iterator.Next ()) )
	{
		if (scan->m_Tag == tag && scan->m_Status == DPlat::in_stasis)
		{
			scan->m_LastInstigator = instigator;
			scan->Reactivate ();

			// [BC] If we're the server, tell clients that that status is changing.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_DoPlat( scan );
		}
	}
}

void DPlat::Stop ()
{
	m_OldStatus = m_Status;
	m_Status = in_stasis;
}

void EV_StopPlat (player_t *instigator, int tag)
{
	if (CLIENT_PREDICT_IsPredicting())
		return;

	DPlat *scan;
	TThinkerIterator<DPlat> iterator;

	while ( (scan = iterator.Next ()) )
	{
		scan->m_LastInstigator = instigator;
		if (scan->m_Status != DPlat::in_stasis && scan->m_Tag == tag)
		{
			scan->Stop ();

			// [BC] If we're the server, tell clients that that status is changing.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_DoPlat( scan );
		}
	}
}

//*****************************************************************************
//
DPlat *P_GetPlatBySectorNum( LONG sectornum )
{
	DPlat	*pPlat;

	TThinkerIterator<DPlat>		Iterator;

	while (( pPlat = Iterator.Next( )))
	{
		if ( pPlat->GetSector()->sectornum == sectornum )
			return ( pPlat );
	}

	return ( NULL );
}
