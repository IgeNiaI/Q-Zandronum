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
//		Floor animation: raising stairs.
//
//-----------------------------------------------------------------------------

#include "doomdef.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "s_sound.h"
#include "s_sndseq.h"
#include "doomstat.h"
#include "r_state.h"
#include "tables.h"
#include "farchive.h"
#include "p_3dmidtex.h"
#include "r_data/r_interpolate.h"
// [BB] New #includes.
#include "version.h"
#include "network.h"
#include "sv_commands.h"
#include "cl_main.h"

//==========================================================================
//
//
//
//==========================================================================

inline FArchive &operator<< (FArchive &arc, DFloor::EFloor &type)
{
	BYTE val = (BYTE)type;
	arc << val;
	type = (DFloor::EFloor)val;
	return arc;
}

//==========================================================================
//
//
//
//==========================================================================

static void StartFloorSound (sector_t *sec)
{
	if (sec->seqType >= 0)
	{
		SN_StartSequence (sec, CHAN_FLOOR, sec->seqType, SEQ_PLATFORM, 0);
	}
	else if (sec->SeqName != NAME_None)
	{
		SN_StartSequence (sec, CHAN_FLOOR, sec->SeqName, 0);
	}
	else
	{
		SN_StartSequence (sec, CHAN_FLOOR, "Floor", 0);
	}
}


//==========================================================================
//
// FLOORS
//
//==========================================================================

IMPLEMENT_CLASS (DFloor)

DFloor::DFloor ()
{
}

void DFloor::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Type
		<< m_Crush
		<< m_Direction
		<< m_NewSpecial
		<< m_Texture
		<< m_FloorDestDist
		<< m_Speed
		<< m_ResetCount
		<< m_OrgDist
		<< m_Delay
		<< m_PauseTime
		<< m_StepTime
		<< m_PerStepTime
		<< m_Hexencrush;
}

//
// MOVE A FLOOR TO ITS DESTINATION (UP OR DOWN)
//
//==========================================================================

void DFloor::Tick ()
{
	EResult res;

	// [RH] Handle resetting stairs
	if (m_Type == buildStair || m_Type == waitStair)
	{
		if (m_ResetCount)
		{
			if (--m_ResetCount == 0)
			{
				m_Type = resetStair;
				m_Direction = (m_Direction > 0) ? -1 : 1;
				m_FloorDestDist = m_OrgDist;
			}
		}
		if (m_PauseTime)
		{
			m_PauseTime--;
			return;
		}
		else if (m_StepTime)
		{
			if (--m_StepTime == 0)
			{
				m_PauseTime = m_Delay;
				m_StepTime = m_PerStepTime;
			}
		}
	}

	if (m_Type == waitStair)
		return;

	res = MoveFloor (m_Speed, m_FloorDestDist, m_Crush, m_Direction, m_Hexencrush);

	if (res == pastdest)
	{
		SN_StopSequence (m_Sector, CHAN_FLOOR);

		if (m_Type == buildStair)
		{
			m_Type = waitStair;
		}

		if (m_Type != waitStair || m_ResetCount == 0)
		{
			if (m_Direction == 1)
			{
				switch (m_Type)
				{
				case donutRaise:
				case genFloorChgT:
				case genFloorChg0:
					m_Sector->special = (m_Sector->special & SECRET_MASK) | m_NewSpecial;
					//fall thru
				case genFloorChg:
					m_Sector->SetTexture(sector_t::floor, m_Texture);

					// [BC] Update clients about this flat change.
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					{
						if ( m_LastInstigator )
							SERVERCOMMANDS_SetSectorFlat( ULONG( m_Sector - sectors ), ULONG(m_LastInstigator - players), SVCF_SKIPTHISCLIENT);
						else
							SERVERCOMMANDS_SetSectorFlat( ULONG( m_Sector - sectors ));
					}

					// [BC] Also, mark this sector as having its flat changed.
					m_Sector->bFlatChange = true;
					break;
				default:
					break;
				}
			}
			else if (m_Direction == -1)
			{
				switch (m_Type)
				{
				case floorLowerAndChange:
				case genFloorChgT:
				case genFloorChg0:
					m_Sector->special = (m_Sector->special & SECRET_MASK) | m_NewSpecial;
					//fall thru
				case genFloorChg:
					m_Sector->SetTexture(sector_t::floor, m_Texture);

					// [BC] Update clients about this flat change.
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					{
						if ( m_LastInstigator )
							SERVERCOMMANDS_SetSectorFlat( ULONG( m_Sector - sectors ), ULONG(m_LastInstigator - players), SVCF_SKIPTHISCLIENT);
						else
							SERVERCOMMANDS_SetSectorFlat( ULONG( m_Sector - sectors ));
					}

					// [BC] Also, mark this sector as having its flat changed.
					m_Sector->bFlatChange = true;
					break;
				default:
					break;
				}
			}

			StopInterpolation();

			//jff 2/26/98 implement stair retrigger lockout while still building
			// note this only applies to the retriggerable generalized stairs

			if (m_Sector->stairlock == -2)		// if this sector is stairlocked
			{
				sector_t *sec = m_Sector;
				sec->stairlock = -1;				// thinker done, promote lock to -1

				while (sec->prevsec != -1 && sectors[sec->prevsec].stairlock != -2)
					sec = &sectors[sec->prevsec];	// search for a non-done thinker
				if (sec->prevsec == -1)				// if all thinkers previous are done
				{
					sec = m_Sector;			// search forward
					while (sec->nextsec != -1 && sectors[sec->nextsec].stairlock != -2) 
						sec = &sectors[sec->nextsec];
					if (sec->nextsec == -1)			// if all thinkers ahead are done too
					{
						while (sec->prevsec != -1)	// clear all locks
						{
							sec->stairlock = 0;
							sec = &sectors[sec->prevsec];
						}
						sec->stairlock = 0;
					}
				}
			}

			Destroy();
		}

		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			if ( (m_Type != buildStair) && (m_Type != waitStair) && (m_Type != resetStair) )
				SERVERCOMMANDS_DoFloor( this );
			else
				SERVERCOMMANDS_BuildStair( this );
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void DFloor::UpdateToClient( ULONG ulClient )
{
	if ( ( m_Type != buildStair ) && ( m_Type != waitStair ) && ( m_Type != resetStair ) )
		SERVERCOMMANDS_DoFloor( this, ulClient, SVCF_ONLYTHISCLIENT );
	else
		SERVERCOMMANDS_BuildStair( this, ulClient, SVCF_ONLYTHISCLIENT );
}

void DFloor::SetFloorChangeType (sector_t *sec, int change)
{
	m_Texture = sec->GetTexture(sector_t::floor);

	switch (change & 3)
	{
	case 1:
		m_NewSpecial = 0;
		m_Type = DFloor::genFloorChg0;
		break;
	case 2:
		m_Type = DFloor::genFloorChg;
		break;
	case 3:
		m_NewSpecial = sec->special & ~SECRET_MASK;
		m_Type = DFloor::genFloorChgT;
		break;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void DFloor::StartFloorSound ()
{
	::StartFloorSound (m_Sector);
}

//==========================================================================
//
//
//
//==========================================================================

DFloor::DFloor (sector_t *sec)
	: DMovingFloor (sec)
{
	m_LastInstigator = NULL;
}

DFloor::EFloor DFloor::GetType( void )
{
	return ( m_Type );
}

void DFloor::SetType( DFloor::EFloor Type )
{
	m_Type = Type;
}

LONG DFloor::GetCrush( void )
{
	return ( m_Crush );
}

void DFloor::SetCrush( LONG lCrush )
{
	m_Crush = lCrush;
}

bool DFloor::GetHexencrush( void ) 
{
	return ( m_Hexencrush );
}

void DFloor::SetHexencrush( bool Hexencrush )
{
	m_Hexencrush = Hexencrush;
}

fixed_t DFloor::GetSpeed( void )
{
	return ( m_Speed );
}

void DFloor::SetSpeed( fixed_t Speed )
{
	m_Speed = Speed;
}

LONG DFloor::GetResetCount( void )
{
	return ( m_ResetCount );
}

void DFloor::SetResetCount( LONG lResetCount )
{
	m_ResetCount = lResetCount;
}

fixed_t DFloor::GetOrgDist( void )
{
	return ( m_OrgDist );
}

void DFloor::SetOrgDist( fixed_t OrgDist )
{
	m_OrgDist = OrgDist;
}

fixed_t DFloor::GetPosition( void )
{
	return ( m_Sector->floorplane.d );
}

LONG DFloor::GetDirection( void )
{
	return ( m_Direction );
}

void DFloor::SetDirection( LONG lDirection )
{
	if (m_Direction != lDirection)
	{
		if (lDirection != 0)
			StartFloorSound();
		else
			SN_StopSequence(m_Sector, CHAN_FLOOR);

		m_Direction = lDirection;
	}
}

fixed_t DFloor::GetFloorDestDist( void ) 
{
	return ( m_FloorDestDist );
}

void DFloor::SetFloorDestDist( fixed_t FloorDestDist )
{
	m_FloorDestDist = FloorDestDist;
}

int DFloor::GetNewSpecial( void ) 
{
	return ( m_NewSpecial );
}

void DFloor::SetNewSpecial( int NewSpecial )
{
	m_NewSpecial = NewSpecial;
}

int DFloor::GetDelay( void )
{
	return m_Delay;
}

void DFloor::SetDelay( int Delay )
{
	m_Delay = Delay;
}

int DFloor::GetPauseTime( void )
{
	return m_PauseTime;
}

void DFloor::SetPauseTime( int PauseTime )
{
	m_PauseTime = PauseTime;
}

int DFloor::GetStepTime( void )
{
	return m_StepTime;
}

void DFloor::SetStepTime( int StepTime )
{
	m_StepTime = StepTime;
}

int DFloor::GetPerStepTime( void )
{
	return m_PerStepTime;
}

void DFloor::SetPerStepTime( int PerStepTime )
{
	m_PerStepTime = PerStepTime;
}

FTextureID DFloor::GetTexture( void )
{
	return ( m_Texture );
}

void DFloor::SetTexture(FTextureID Texture )
{
	m_Texture = Texture;
}

//==========================================================================
//
// HANDLE FLOOR TYPES
// [RH] Added tag, speed, height, crush, change params.
// This functions starts too many different things.
//
//==========================================================================

bool EV_DoFloor (DFloor::EFloor floortype, line_t *line, int tag,
				 fixed_t speed, fixed_t height, int crush, int change, bool hexencrush, bool hereticlower)
{
	return EV_DoFloor(NULL, floortype, line, tag,
		speed, height, crush, change, hexencrush, hereticlower);
}

bool EV_DoFloor (player_t *instigator, DFloor::EFloor floortype, line_t *line, int tag,
				 fixed_t speed, fixed_t height, int crush, int change, bool hexencrush, bool hereticlower)
{
	if (CLIENT_PREDICT_IsPredicting())
		return false;

	int 		secnum;
	bool 		rtn;
	sector_t*	sec;
	DFloor*		floor;
	fixed_t		ceilingheight;
	fixed_t		newheight;
	vertex_t	*spot, *spot2;

	rtn = false;

	// check if a manual trigger; if so do just the sector on the backside
	if (tag == 0)
	{
		if (!line || !(sec = line->backsector))
			return rtn;
		secnum = (int)(sec-sectors);
		goto manual_floor;
	}

	secnum = -1;
	while (tag && (secnum = P_FindSectorFromTag (tag, secnum)) >= 0)
	{
		sec = &sectors[secnum];

manual_floor:
		// ALREADY MOVING?	IF SO, KEEP GOING...
		if (sec->PlaneMoving(sector_t::floor))
		{
			// There was a test for 0/non-0 here, supposed to prevent 0-tags from executing "continue" and searching for unrelated sectors
			// Unfortunately, the condition had been reversed, so that searches for tag-0 would continue,
			// while numbered tags would abort (return false, even if some floors have been successfully triggered)

			// All occurences of the condition (faulty or not) have been replaced by a looping condition: Looping only occurs if we're looking for a non-0 tag.
			continue;
		}
		

		// new floor thinker
		rtn = true;
		floor = new DFloor (sec);
		floor->m_Type = floortype;
		floor->m_Crush = -1;
		floor->m_Hexencrush = hexencrush;
		floor->m_Speed = speed;
		floor->m_ResetCount = 0;				// [RH]
		floor->m_OrgDist = sec->floorplane.d;	// [RH]
		floor->m_LastInstigator = instigator;

		floor->StartFloorSound ();

		switch (floortype)
		{
		case DFloor::floorLowerToHighest:
			floor->m_Direction = -1;
			newheight = sec->FindHighestFloorSurrounding (&spot);
			floor->m_FloorDestDist = sec->floorplane.PointToDist (spot, newheight);
			// [RH] DOOM's turboLower type did this. I've just extended it
			//		to be applicable to all LowerToHighest types.
			if (hereticlower || floor->m_FloorDestDist != sec->floorplane.d)
				floor->m_FloorDestDist = sec->floorplane.PointToDist (spot, newheight+height);
			break;

		case DFloor::floorLowerToLowest:
			floor->m_Direction = -1;
			newheight = sec->FindLowestFloorSurrounding (&spot);
			floor->m_FloorDestDist = sec->floorplane.PointToDist (spot, newheight);
			break;

		case DFloor::floorLowerToNearest:
			//jff 02/03/30 support lowering floor to next lowest floor
			floor->m_Direction = -1;
			newheight = sec->FindNextLowestFloor (&spot);
			floor->m_FloorDestDist = sec->floorplane.PointToDist (spot, newheight);
			break;

		case DFloor::floorLowerInstant:
			floor->m_Speed = height;
			// fall through
		case DFloor::floorLowerByValue:
			floor->m_Direction = -1;
			newheight = sec->floorplane.ZatPoint (0, 0) - height;
			floor->m_FloorDestDist = sec->floorplane.PointToDist (0, 0, newheight);
			break;

		case DFloor::floorRaiseInstant:
			floor->m_Speed = height;
			// fall through
		case DFloor::floorRaiseByValue:
			floor->m_Direction = 1;
			newheight = sec->floorplane.ZatPoint (0, 0) + height;
			floor->m_FloorDestDist = sec->floorplane.PointToDist (0, 0, newheight);
			break;

		case DFloor::floorMoveToValue:
			sec->FindHighestFloorPoint (&spot);
			floor->m_FloorDestDist = sec->floorplane.PointToDist (spot, height);
			floor->m_Direction = (floor->m_FloorDestDist > sec->floorplane.d) ? -1 : 1;
			break;

		case DFloor::floorRaiseAndCrushDoom:
			floor->m_Crush = crush;
			// fall through
		case DFloor::floorRaiseToLowestCeiling:
			floor->m_Direction = 1;
			newheight = sec->FindLowestCeilingSurrounding (&spot);
			if (floortype == DFloor::floorRaiseAndCrushDoom)
				newheight -= 8 * FRACUNIT;
			ceilingheight = sec->FindLowestCeilingPoint (&spot2);
			floor->m_FloorDestDist = sec->floorplane.PointToDist (spot, newheight);
			if (sec->floorplane.ZatPointDist (spot2, floor->m_FloorDestDist) > ceilingheight)
				floor->m_FloorDestDist = sec->floorplane.PointToDist (spot2,
					floortype == DFloor::floorRaiseAndCrushDoom ? ceilingheight - 8*FRACUNIT : ceilingheight);
			break;

		case DFloor::floorRaiseToHighest:
			floor->m_Direction = 1;
			newheight = sec->FindHighestFloorSurrounding (&spot);
			floor->m_FloorDestDist = sec->floorplane.PointToDist (spot, newheight);
			break;

		case DFloor::floorRaiseToNearest:
			floor->m_Direction = 1;
			newheight = sec->FindNextHighestFloor (&spot);
			floor->m_FloorDestDist = sec->floorplane.PointToDist (spot, newheight);
			break;

		case DFloor::floorRaiseToLowest:
			floor->m_Direction = 1;
			newheight = sec->FindLowestFloorSurrounding (&spot);
			floor->m_FloorDestDist = sec->floorplane.PointToDist (spot, newheight);
			break;

		case DFloor::floorRaiseAndCrush:
			floor->m_Crush = crush;
			floor->m_Direction = 1;
			newheight = sec->FindLowestCeilingPoint (&spot) - 8*FRACUNIT;
			floor->m_FloorDestDist = sec->floorplane.PointToDist (spot, newheight);
			break;

		case DFloor::floorRaiseToCeiling:
			floor->m_Direction = 1;
			newheight = sec->FindLowestCeilingPoint (&spot);
			floor->m_FloorDestDist = sec->floorplane.PointToDist (spot, newheight);
			break;

		case DFloor::floorLowerToLowestCeiling:
			floor->m_Direction = -1;
			newheight = sec->FindLowestCeilingSurrounding (&spot);
			floor->m_FloorDestDist = sec->floorplane.PointToDist (spot, newheight);
			break;

		case DFloor::floorLowerByTexture:
			floor->m_Direction = -1;
			newheight = sec->floorplane.ZatPoint (0, 0) - sec->FindShortestTextureAround ();
			floor->m_FloorDestDist = sec->floorplane.PointToDist (0, 0, newheight);
			break;

		case DFloor::floorLowerToCeiling:
			// [RH] Essentially instantly raises the floor to the ceiling
			floor->m_Direction = -1;
			newheight = sec->FindLowestCeilingPoint (&spot);
			floor->m_FloorDestDist = sec->floorplane.PointToDist (spot, newheight);
			break;

		case DFloor::floorRaiseByTexture:
			floor->m_Direction = 1;
			// [RH] Use P_FindShortestTextureAround from BOOM to do this
			//		since the code is identical to what was here. (Oddly
			//		enough, BOOM preserved the code here even though it
			//		also had this function.)
			newheight = sec->floorplane.ZatPoint (0, 0) + sec->FindShortestTextureAround ();
			floor->m_FloorDestDist = sec->floorplane.PointToDist (0, 0, newheight);
			break;

		case DFloor::floorRaiseAndChange:
			floor->m_Direction = 1;
			newheight = sec->floorplane.ZatPoint (0, 0) + height;
			floor->m_FloorDestDist = sec->floorplane.PointToDist (0, 0, newheight);
			if (line != NULL)
			{
				FTextureID oldpic = sec->GetTexture(sector_t::floor);
				sec->SetTexture(sector_t::floor, line->frontsector->GetTexture(sector_t::floor));

				// [BC] Update clients about this flat change.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					if ( instigator )
						SERVERCOMMANDS_SetSectorFlat( ULONG( sec - sectors ), ULONG(instigator - players), SVCF_SKIPTHISCLIENT);
					else
						SERVERCOMMANDS_SetSectorFlat( ULONG( sec - sectors ));
				}

				// [BC] Also, mark this sector as having its flat changed.
				sec->bFlatChange = true;

				sec->special = (sec->special & SECRET_MASK) | (line->frontsector->special & ~SECRET_MASK);
			}
			else
			{
				sec->special &= SECRET_MASK;
			}
			break;
		  
		case DFloor::floorLowerAndChange:
			floor->m_Direction = -1;
			newheight = sec->FindLowestFloorSurrounding (&spot);
			floor->m_FloorDestDist = sec->floorplane.PointToDist (spot, newheight);
			floor->m_Texture = sec->GetTexture(sector_t::floor);
			// jff 1/24/98 make sure floor->m_NewSpecial gets initialized
			// in case no surrounding sector is at floordestheight
			// --> should not affect compatibility <--
			floor->m_NewSpecial = sec->special & ~SECRET_MASK; 

			//jff 5/23/98 use model subroutine to unify fixes and handling
			sector_t *modelsec;
			modelsec = sec->FindModelFloorSector (newheight);
			if (modelsec != NULL)
			{
				floor->m_Texture = modelsec->GetTexture(sector_t::floor);
				floor->m_NewSpecial = modelsec->special & ~SECRET_MASK;

				// [BC] Update clients about this flat change.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					if ( instigator )
						SERVERCOMMANDS_SetSectorFlat( ULONG( sec - sectors ), ULONG(instigator - players), SVCF_SKIPTHISCLIENT);
					else
						SERVERCOMMANDS_SetSectorFlat( ULONG( sec - sectors ));
				}

				// [BC] Also, mark this sector as having its flat changed.
				sec->bFlatChange = true;
			}
			break;

		  default:
			break;
		}

		// [BC] If we're the server, tell clients to create the floor.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_DoFloor( floor );

		// Do not interpolate instant movement floors.
		bool silent = false;

		if ((floor->m_Direction>0 && floor->m_FloorDestDist>sec->floorplane.d) ||	// moving up but going down
			(floor->m_Direction<0 && floor->m_FloorDestDist<sec->floorplane.d) ||	// moving down but going up
			(floor->m_Speed >= abs(sec->floorplane.d - floor->m_FloorDestDist)))	// moving in one step
		{
			floor->StopInterpolation();

			// [Graf Zahl]
			// Don't make sounds for instant movement hacks but make an exception for
			// switches that activate their own back side. 
			if (!(i_compatflags & COMPATF_SILENT_INSTANT_FLOORS))
			{
				if (!line || !(line->activation & (SPAC_Use|SPAC_Push)) || line->backsector!=sec)
					silent = true;
			}
		}
		if (!silent) floor->StartFloorSound ();

		if (change & 3)
		{
			// [RH] Need to do some transferring
			if (change & 4)
			{
				// Numeric model change
				sector_t *modelsec;

				modelsec = (floortype == DFloor::floorRaiseToLowestCeiling ||
					   floortype == DFloor::floorLowerToLowestCeiling ||
					   floortype == DFloor::floorRaiseToCeiling ||
					   floortype == DFloor::floorLowerToCeiling) ?
					  sec->FindModelCeilingSector (-floor->m_FloorDestDist) :
					  sec->FindModelFloorSector (-floor->m_FloorDestDist);

				if (modelsec != NULL)
				{
					floor->SetFloorChangeType (modelsec, change);

					// [BC] Update clients about this flat change.
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
						SERVERCOMMANDS_SetSectorFlat( ULONG( modelsec - sectors ));
				}
			}
			else if (line)
			{
				// Trigger model change
				floor->SetFloorChangeType (line->frontsector, change);

				// [BC] Update clients about this flat change.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SetSectorFlat( ULONG( sec - sectors ));
			}
		}
	}
	return rtn;
}

//==========================================================================
//
// [RH]
// EV_FloorCrushStop
// Stop a floor from crushing!
//
//==========================================================================

bool EV_FloorCrushStop (player_t *instigator, int tag)
{
	if (CLIENT_PREDICT_IsPredicting())
		return false;

	int secnum = -1;

	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0)
	{
		sector_t *sec = sectors + secnum;

		if (sec->floordata && sec->floordata->IsKindOf (RUNTIME_CLASS(DFloor)) &&
			barrier_cast<DFloor *>(sec->floordata)->m_Type == DFloor::floorRaiseAndCrush)
		{
			SN_StopSequence (sec, CHAN_FLOOR);
			sec->floordata->Destroy ();

			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_DoFloor( barrier_cast<DFloor *>( sec->floordata ) );
		}
	}
	return true;
}

//============================================================================
//
// EV_FloorStop
// Stop a floor
//
//============================================================================

bool EV_FloorStop (int tag)
{
	int secnum = -1;

	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0)
	{
		sector_t *sec = sectors + secnum;

		if (sec->floordata && sec->floordata->IsKindOf (RUNTIME_CLASS(DFloor)))
		{
			SN_StopSequence (sec, CHAN_FLOOR);
			sec->floordata->Destroy ();
			sec->floordata = NULL;

			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_DoFloor( barrier_cast<DFloor *>( sec->floordata ) );
		}
	}
	return true;
}

//==========================================================================
//
// Linear tag search to emulate stair building from Doom.exe
//
//==========================================================================

static int P_FindSectorFromTagLinear (int tag, int start)
{
    for (int i=start+1;i<numsectors;i++)
	{
		if (sectors[i].tag == tag) return i;
	}
    return -1;
}

//==========================================================================
//
// BUILD A STAIRCASE!
// [RH] Added stairsize, srcspeed, delay, reset, igntxt, usespecials parameters
//		If usespecials is non-zero, then each sector in a stair is determined
//		by its special. If usespecials is 2, each sector stays in "sync" with
//		the others.
//
//==========================================================================

bool EV_BuildStairs (player_t *instigator, int tag, DFloor::EStair type, line_t *line,
					 fixed_t stairsize, fixed_t speed, int delay, int reset, int igntxt,
					 int usespecials)
{
	if (CLIENT_PREDICT_IsPredicting())
		return false;

	int 				secnum;
	int					osecnum;	//jff 3/4/98 save old loop index
	int 				height;
	fixed_t				stairstep;
	int 				i;
	int 				newsecnum = -1;
	FTextureID			texture;
	int 				ok;
	int					persteptime;
	bool 				rtn = false;
	
	sector_t*			sec;
	sector_t*			tsec = NULL;
	sector_t*			prev = NULL;

	DFloor*				floor;
	bool				manual = false;

	if (speed == 0)
		return false;

	persteptime = FixedDiv (stairsize, speed) >> FRACBITS;

	int (* FindSector) (int tag, int start)  =
		(i_compatflags & COMPATF_STAIRINDEX)? P_FindSectorFromTagLinear : P_FindSectorFromTag;

	// check if a manual trigger, if so do just the sector on the backside
	if (tag == 0)
	{
		if (!line || !(sec = line->backsector))
			return rtn;
		secnum = (int)(sec-sectors);
		manual = true;
		goto manual_stair;
	}

	// The compatibility mode doesn't work with a hashing algorithm.
	// It needs the original linear search method. This was broken in Boom.

	secnum = -1;
	while ((secnum = FindSector (tag, secnum)) >= 0)
	{
		sec = &sectors[secnum];

manual_stair:
		// ALREADY MOVING?	IF SO, KEEP GOING...
		//jff 2/26/98 add special lockout condition to wait for entire
		//staircase to build before retriggering
		if (sec->PlaneMoving(sector_t::floor) || sec->stairlock)
		{
			if (!manual)
				continue;
			else
				return rtn;
		}
		
		// new floor thinker
		rtn = true;
		floor = new DFloor (sec);
		floor->m_Direction = (type == DFloor::buildUp) ? 1 : -1;
		stairstep = stairsize * floor->m_Direction;
		floor->m_Type = DFloor::buildStair;	//jff 3/31/98 do not leave uninited
		floor->m_ResetCount = reset;	// [RH] Tics until reset (0 if never)
		floor->m_OrgDist = sec->floorplane.d;	// [RH] Height to reset to
		// [RH] Set up delay values
		floor->m_Delay = delay;
		floor->m_PauseTime = 0;
		floor->m_StepTime = floor->m_PerStepTime = persteptime;
		floor->m_LastInstigator = instigator;

		floor->m_Crush = (!usespecials && speed == 4*FRACUNIT) ? 10 : -1; //jff 2/27/98 fix uninitialized crush field
		floor->m_Hexencrush = false;

		floor->m_Speed = speed;
		height = sec->floorplane.ZatPoint (0, 0) + stairstep;
		floor->m_FloorDestDist = sec->floorplane.PointToDist (0, 0, height);

		texture = sec->GetTexture(sector_t::floor);
		osecnum = secnum;				//jff 3/4/98 preserve loop index

		// [BC] If we're the server, tell clients to create the floor.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_BuildStair( floor );

		// Find next sector to raise
		// 1. Find 2-sided line with same sector side[0] (lowest numbered)
		// 2. Other side is the next sector to raise
		// 3. Unless already moving, or different texture, then stop building
		do
		{
			ok = 0;

			if (usespecials)
			{
				// [RH] Find the next sector by scanning for Stairs_Special?
				tsec = sec->NextSpecialSector (
						(sec->special & 0xff) == Stairs_Special1 ?
							Stairs_Special2 : Stairs_Special1, prev);

				if ( (ok = (tsec != NULL)) )
				{
					height += stairstep;

					// if sector's floor already moving, look for another
					//jff 2/26/98 special lockout condition for retriggering
					if (tsec->PlaneMoving(sector_t::floor) || tsec->stairlock)
					{
						prev = sec;
						sec = tsec;
						continue;
					}
					
				}
				newsecnum = (int)(tsec - sectors);
			}
			else
			{
				for (i = 0; i < sec->linecount; i++)
				{
					if ( !((sec->lines[i])->flags & ML_TWOSIDED) )
						continue;

					tsec = (sec->lines[i])->frontsector;
					newsecnum = (int)(tsec-sectors);

					if (secnum != newsecnum)
						continue;

					tsec = (sec->lines[i])->backsector;
					if (!tsec) continue;	//jff 5/7/98 if no backside, continue
					newsecnum = (int)(tsec - sectors);

					if (!igntxt && tsec->GetTexture(sector_t::floor) != texture)
						continue;

					// Doom bug: Height was changed before discarding the sector as part of the stairs.
					// Needs to be compatibility optioned because some maps (Eternall MAP25) depend on it.
					if (i_compatflags & COMPATF_STAIRINDEX) height += stairstep;

					// if sector's floor already moving, look for another
					//jff 2/26/98 special lockout condition for retriggering
					if (tsec->PlaneMoving(sector_t::floor) || tsec->stairlock)
						continue;

					if (!(i_compatflags & COMPATF_STAIRINDEX)) height += stairstep;

					ok = true;
					break;
				}
			}

			if (ok)
			{
				// jff 2/26/98
				// link the stair chain in both directions
				// lock the stair sector until building complete
				sec->nextsec = newsecnum; // link step to next
				tsec->prevsec = secnum;   // link next back
				tsec->nextsec = -1;       // set next forward link as end
				tsec->stairlock = -2;     // lock the step

				prev = sec;
				sec = tsec;
				secnum = newsecnum;

				// create and initialize a thinker for the next step
				floor = new DFloor (sec);
				floor->StartFloorSound ();
				floor->m_Direction = (type == DFloor::buildUp) ? 1 : -1;
				floor->m_FloorDestDist = sec->floorplane.PointToDist (0, 0, height);
				// [RH] Set up delay values
				floor->m_Delay = delay;
				floor->m_PauseTime = 0;
				floor->m_StepTime = floor->m_PerStepTime = persteptime;
				floor->m_LastInstigator = instigator;

				if (usespecials == 2)
				{
					// [RH]
					fixed_t rise = height - sec->CenterFloor();
					floor->m_Speed = Scale (speed, rise, stairstep);
				}
				else
				{
					floor->m_Speed = speed;
				}
				floor->m_Type = DFloor::buildStair;	//jff 3/31/98 do not leave uninited
				//jff 2/27/98 fix uninitialized crush field
				floor->m_Crush = (!usespecials && speed == 4*FRACUNIT) ? 10 : -1;
				floor->m_Hexencrush = false;
				floor->m_ResetCount = reset;	// [RH] Tics until reset (0 if never)
				floor->m_OrgDist = sec->floorplane.d;	// [RH] Height to reset to

				// [BC] If we're the server, tell clients to create the floor.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_BuildStair( floor );
			}
		} while (ok);
		// [RH] make sure the first sector doesn't point to a previous one, otherwise
		// it can infinite loop when the first sector stops moving.
		sectors[osecnum].prevsec = -1;	
		if (manual)
		{
			return rtn;
		}
		if (!(i_compatflags & COMPATF_STAIRINDEX))
		{
			secnum = osecnum;	//jff 3/4/98 restore loop index
		}
	}
	return rtn;
}

//==========================================================================
//
// [RH] Added pillarspeed and slimespeed parameters
//
//==========================================================================

bool EV_DoDonut (player_t *instigator, int tag, line_t *line, fixed_t pillarspeed, fixed_t slimespeed)
{
	if (CLIENT_PREDICT_IsPredicting())
		return false;

	sector_t*			s1;
	sector_t*			s2;
	sector_t*			s3;
	int 				secnum;
	bool 				rtn;
	int 				i;
	DFloor*				floor;
	vertex_t*			spot;
	fixed_t				height;
		
	secnum = -1;
	rtn = false;

	if (tag == 0)
	{
		if (!line || !(s1 = line->backsector))
			return rtn;
		goto manual_donut;
	}

	while (tag && (secnum = P_FindSectorFromTag(tag,secnum)) >= 0)
	{
		s1 = &sectors[secnum];					// s1 is pillar's sector

manual_donut:
		// ALREADY MOVING?	IF SO, KEEP GOING...
		if (s1->PlaneMoving(sector_t::floor))
			continue; // safe now, because we check that tag is non-0 in the looping condition [fdari]

		rtn = true;
		s2 = getNextSector (s1->lines[0], s1);	// s2 is pool's sector
		if (!s2)								// note lowest numbered line around
			continue;							// pillar must be two-sided

		if (s2->PlaneMoving(sector_t::floor))
			continue;

		for (i = 0; i < s2->linecount; i++)
		{
			if (!(s2->lines[i]->flags & ML_TWOSIDED) ||
				(s2->lines[i]->backsector == s1))
				continue;
			s3 = s2->lines[i]->backsector;

			//	Spawn rising slime
			floor = new DFloor (s2);
			floor->m_Type = DFloor::donutRaise;
			floor->m_Crush = -1;
			floor->m_Hexencrush = false;
			floor->m_Direction = 1;
			floor->m_Sector = s2;
			floor->m_Speed = slimespeed;
			floor->m_Texture = s3->GetTexture(sector_t::floor);
			floor->m_NewSpecial = 0;
			height = s3->FindHighestFloorPoint (&spot);
			floor->m_FloorDestDist = s2->floorplane.PointToDist (spot, height);
			floor->m_LastInstigator = instigator;
			floor->StartFloorSound ();
			
			// [BC] If we're the server, tell clients to create the floor.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_DoFloor( floor );

			//	Spawn lowering donut-hole
			floor = new DFloor (s1);
			floor->m_Type = DFloor::floorLowerToNearest;
			floor->m_Crush = -1;
			floor->m_Hexencrush = false;
			floor->m_Direction = -1;
			floor->m_Sector = s1;
			floor->m_Speed = pillarspeed;
			height = s3->FindHighestFloorPoint (&spot);
			floor->m_FloorDestDist = s1->floorplane.PointToDist (spot, height);
			floor->m_LastInstigator = instigator;
			floor->StartFloorSound ();

			// [BC] If we're the server, tell clients to create the floor.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_DoFloor( floor );

			break;
		}
	}
	return rtn;
}

//==========================================================================
//
// Elevators
//
//==========================================================================

//==========================================================================
// [BC]
void DElevator::UpdateToClient( ULONG ulClient )
{
	SERVERCOMMANDS_DoElevator( this, ulClient, SVCF_ONLYTHISCLIENT );
}

// [BC]
DElevator::EElevator DElevator::GetType( void )
{
	return ( m_Type );
}

// [BC]
void DElevator::SetType( EElevator Type )
{
	m_Type = Type;
}

// [BC]
fixed_t DElevator::GetSpeed( void )
{
	return ( m_Speed );
}

// [BC]
void DElevator::SetSpeed( fixed_t Speed )
{
	m_Speed = Speed;
}

// [BC]
LONG DElevator::GetDirection( void )
{
	return ( m_Direction );
}

// [BC]
void DElevator::SetDirection( LONG lDirection )
{
	if (m_Direction == 0 && lDirection != 0)
		StartFloorSound();
	else if (lDirection == 0)
		SN_StopSequence (m_Sector, CHAN_FLOOR);

	m_Direction = lDirection;
}

// [geNia]
fixed_t	DElevator::GetFloorPosition( void )
{
	return ( m_Sector->floorplane.d );
}

// [BC]
fixed_t DElevator::GetFloorDestDist( void )
{
	return ( m_FloorDestDist );
}

// [BC]
void DElevator::SetFloorDestDist( fixed_t DestDist )
{
	m_FloorDestDist = DestDist;
}

// [geNia]
fixed_t	DElevator::GetCeilingPosition( void )
{
	return ( m_Sector->ceilingplane.d );
}

// [BC]
fixed_t DElevator::GetCeilingDestDist( void )
{
	return ( m_CeilingDestDist );
}

// [BC]
void DElevator::SetCeilingDestDist( fixed_t DestDist )
{
	m_CeilingDestDist = DestDist;
}

IMPLEMENT_POINTY_CLASS (DElevator)
	DECLARE_POINTER(m_Interp_Floor)
	DECLARE_POINTER(m_Interp_Ceiling)
END_POINTERS

inline FArchive &operator<< (FArchive &arc, DElevator::EElevator &type)
{
	BYTE val = (BYTE)type;
	arc << val;
	type = (DElevator::EElevator)val;
	return arc;
}

DElevator::DElevator ()
{
}

DElevator::DElevator (sector_t *sec)
	: Super (sec)
{
	sec->floordata = this;
	sec->ceilingdata = this;
	m_Interp_Floor = sec->SetInterpolation(sector_t::FloorMove, true);
	m_Interp_Ceiling = sec->SetInterpolation(sector_t::CeilingMove, true);
	m_Direction = 0;
	m_LastInstigator = NULL;
}

void DElevator::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Type
		<< m_Direction
		<< m_FloorDestDist
		<< m_CeilingDestDist
		<< m_Speed
		<< m_Interp_Floor
		<< m_Interp_Ceiling;
}

//==========================================================================
//
//
//
//==========================================================================

void DElevator::Destroy()
{
	if (m_Interp_Ceiling != NULL)
	{
		m_Interp_Ceiling->DelRef();
		m_Interp_Ceiling = NULL;
	}
	if (m_Interp_Floor != NULL)
	{
		m_Interp_Floor->DelRef();
		m_Interp_Floor = NULL;
	}
	Super::Destroy();
}

//==========================================================================
//
// T_MoveElevator()
//
// Move an elevator to it's destination (up or down)
// Called once per tick for each moving floor.
//
// Passed an elevator_t structure that contains all pertinent info about the
// move. See P_SPEC.H for fields.
// No return.
//
// jff 02/22/98 added to support parallel floor/ceiling motion
//
//==========================================================================

void DElevator::Tick ()
{
	EResult res;

	fixed_t oldfloor, oldceiling;

	oldfloor = m_Sector->floorplane.d;
	oldceiling = m_Sector->ceilingplane.d;

	if (m_Direction < 0)	// moving down
	{
		res = MoveFloor (m_Speed, m_FloorDestDist, m_Direction);
		if (res == ok || res == pastdest)
		{
			res = MoveCeiling (m_Speed, m_CeilingDestDist, m_Direction);
			if (res == crushed)
			{
				MoveFloor (m_Speed, oldfloor, -m_Direction);
			}
		}
	}
	else // up
	{
		res = MoveCeiling (m_Speed, m_CeilingDestDist, m_Direction);
		if (res == ok || res == pastdest)
		{
			res = MoveFloor (m_Speed, m_FloorDestDist, m_Direction);
			if (res == crushed)
			{
				MoveCeiling (m_Speed, oldceiling, -m_Direction);
			}
		}
	}

	if (res == pastdest)	// if destination height acheived
	{
		SN_StopSequence(m_Sector, CHAN_FLOOR);
		Destroy();

		// [BC] If the sector has reached its destination, this is probably a good time to verify all the clients
		// have the correct floor/ceiling height for this sector.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_DoElevator( this );
	}
}

//==========================================================================
//
//
//
//==========================================================================

void DElevator::StartFloorSound ()
{
	::StartFloorSound (m_Sector);
}

//==========================================================================
//
// EV_DoElevator
//
// Handle elevator linedef types
//
// Passed the linedef that triggered the elevator and the elevator action
//
// jff 2/22/98 new type to move floor and ceiling in parallel
// [RH] Added speed, tag, and height parameters and new types.
//
//==========================================================================

bool EV_DoElevator (player_t *instigator, line_t *line, DElevator::EElevator elevtype,
					fixed_t speed, fixed_t height, int tag)
{
	if (CLIENT_PREDICT_IsPredicting())
		return false;

	int			secnum;
	bool		rtn;
	sector_t*	sec;
	DElevator*	elevator;
	fixed_t		floorheight, ceilingheight;
	fixed_t		newheight;
	vertex_t*	spot;

	if (!line && (elevtype == DElevator::elevateCurrent))
		return false;

	secnum = -1;
	rtn = false;

	if (tag == 0)
	{
		if (!line || !(sec = line->backsector))
			return rtn;
		goto manual_elevator;
	}


	// act on all sectors with the same tag as the triggering linedef
	while (tag && (secnum = P_FindSectorFromTag (tag, secnum)) >= 0) // never loop for a non-0 tag (condition moved to beginning of loop) [FDARI]
	{
		sec = &sectors[secnum];
manual_elevator:
		// If either floor or ceiling is already activated, skip it
		if (sec->PlaneMoving(sector_t::floor) || sec->ceilingdata) //jff 2/22/98
			continue; // the loop used to break at the end if tag were 0, but would miss that step if "continue" occured [FDARI]

		// create and initialize new elevator thinker
		rtn = true;
		elevator = new DElevator (sec);
		elevator->m_Type = elevtype;
		elevator->m_Speed = speed;
		elevator->StartFloorSound ();
		elevator->m_LastInstigator = instigator;

		floorheight = sec->CenterFloor ();
		ceilingheight = sec->CenterCeiling ();

		// set up the fields according to the type of elevator action
		switch (elevtype)
		{
		// elevator down to next floor
		case DElevator::elevateDown:
			elevator->m_Direction = -1;
			newheight = sec->FindNextLowestFloor (&spot);
			elevator->m_FloorDestDist = sec->floorplane.PointToDist (spot, newheight);
			newheight += sec->ceilingplane.ZatPoint (spot) - sec->floorplane.ZatPoint (spot);
			elevator->m_CeilingDestDist = sec->ceilingplane.PointToDist (spot, newheight);
			break;

		// elevator up to next floor
		case DElevator::elevateUp:
			elevator->m_Direction = 1;
			newheight = sec->FindNextHighestFloor (&spot);
			elevator->m_FloorDestDist = sec->floorplane.PointToDist (spot, newheight);
			newheight += sec->ceilingplane.ZatPoint (spot) - sec->floorplane.ZatPoint (spot);
			elevator->m_CeilingDestDist = sec->ceilingplane.PointToDist (spot, newheight);
			break;

		// elevator to floor height of activating switch's front sector
		case DElevator::elevateCurrent:
			newheight = line->frontsector->floorplane.ZatPoint (line->v1);
			elevator->m_FloorDestDist = sec->floorplane.PointToDist (line->v1, newheight);
			newheight += sec->ceilingplane.ZatPoint (line->v1) - sec->floorplane.ZatPoint (line->v1);
			elevator->m_CeilingDestDist = sec->ceilingplane.PointToDist (line->v1, newheight);

			elevator->m_Direction =
				elevator->m_FloorDestDist > sec->floorplane.d ? -1 : 1;
			break;

		// [RH] elevate up by a specific amount
		case DElevator::elevateRaise:
			elevator->m_Direction = 1;
			elevator->m_FloorDestDist = sec->floorplane.PointToDist (sec->soundorg[0], sec->soundorg[1], floorheight + height);
			elevator->m_CeilingDestDist = sec->ceilingplane.PointToDist (sec->soundorg[0], sec->soundorg[1], ceilingheight + height);
			break;

		// [RH] elevate down by a specific amount
		case DElevator::elevateLower:
			elevator->m_Direction = -1;
			elevator->m_FloorDestDist = sec->floorplane.PointToDist (sec->soundorg[0], sec->soundorg[1], floorheight - height);
			elevator->m_CeilingDestDist = sec->ceilingplane.PointToDist (sec->soundorg[0], sec->soundorg[1], ceilingheight - height);
			break;
		}

		// [BC] If we're the server, tell clients to create the elevator.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_DoElevator( elevator );

	}
	return rtn;
}


//==========================================================================
//
// EV_DoChange()
//
// Handle pure change types. These change floor texture and sector type
// by trigger or numeric model without moving the floor.
//
// The linedef causing the change and the type of change is passed
// Returns true if any sector changes
//
// jff 3/15/98 added to better support generalized sector types
// [RH] Added tag parameter.
//
//==========================================================================

bool EV_DoChange (player_t *instigator, line_t *line, EChange changetype, int tag)
{
	if (CLIENT_PREDICT_IsPredicting())
		return false;

	int			secnum;
	bool		rtn;
	sector_t	*sec;
	sector_t	*secm;

	secnum = -1;
	rtn = false;
	// change all sectors with the same tag as the linedef
	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0)
	{
		sec = &sectors[secnum];
              
		rtn = true;

		// handle trigger or numeric change type
		FTextureID oldpic = sec->GetTexture(sector_t::floor);

		switch(changetype)
		{
		case trigChangeOnly:
			if (line)
			{ // [RH] if no line, no change
				sec->SetTexture(sector_t::floor, line->frontsector->GetTexture(sector_t::floor));
				sec->special = (sec->special & SECRET_MASK) | (line->frontsector->special & ~SECRET_MASK);

				// [BC] Update clients about this flat change.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					if (instigator)
						SERVERCOMMANDS_SetSectorFlat( ULONG( sec - sectors ), ULONG(instigator - players), SVCF_SKIPTHISCLIENT);
					else
						SERVERCOMMANDS_SetSectorFlat( ULONG( sec - sectors ));
				}

				// [BC] Also, mark this sector as having its flat changed.
				sec->bFlatChange = true;
			}
			break;
		case numChangeOnly:
			secm = sec->FindModelFloorSector (sec->CenterFloor());
			if (secm)
			{ // if no model, no change
				sec->SetTexture(sector_t::floor, secm->GetTexture(sector_t::floor));
				sec->special = secm->special;

				// [BC] Update clients about this flat change.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					if (instigator)
						SERVERCOMMANDS_SetSectorFlat( ULONG( sec - sectors ), ULONG(instigator - players), SVCF_SKIPTHISCLIENT);
					else
						SERVERCOMMANDS_SetSectorFlat( ULONG( sec - sectors ));
				}

				// [BC] Also, mark this sector as having its flat changed.
				sec->bFlatChange = true;
			}
			break;
		default:
			break;
		}
	}
	return rtn;
}


//==========================================================================
//
//
//
//==========================================================================

IMPLEMENT_POINTY_CLASS (DWaggleBase)
	DECLARE_POINTER(m_Interpolation)
END_POINTERS

IMPLEMENT_CLASS (DFloorWaggle)
IMPLEMENT_CLASS (DCeilingWaggle)

DWaggleBase::DWaggleBase ()
{
}

void DWaggleBase::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_OriginalDist
		<< m_Accumulator
		<< m_AccDelta
		<< m_TargetScale
		<< m_Scale
		<< m_ScaleDelta
		<< m_Ticker
		<< m_State
		<< m_Interpolation;
}

//==========================================================================
//
// WaggleBase
//
//==========================================================================

#define WGLSTATE_EXPAND 1
#define WGLSTATE_STABLE 2
#define WGLSTATE_REDUCE 3

DWaggleBase::DWaggleBase (sector_t *sec)
	: Super (sec)
{
	m_LastInstigator = NULL;
}

void DWaggleBase::Destroy()
{
	if (m_Interpolation != NULL)
	{
		m_Interpolation->DelRef();
		m_Interpolation = NULL;
	}
	Super::Destroy();
}

// [BC]
void DWaggleBase::UpdateToClient( ULONG ulClient )
{
	SERVERCOMMANDS_DoWaggle( this, ulClient, SVCF_ONLYTHISCLIENT );
}

// [geNia]
fixed_t DWaggleBase::GetPosition( )
{
	Printf("WARNING: DWaggleBase::GetPosition was called. This should never happen! Please report this at the %s bug tracker!\n", GAMENAME);
	return 0;
}

// [geNia]
fixed_t DWaggleBase::GetOriginalDistance( )
{
	return m_OriginalDist;
}

// [BC]
void DWaggleBase::SetOriginalDistance( fixed_t OriginalDistance )
{
	m_OriginalDist = OriginalDistance;
}

// [geNia]
fixed_t DWaggleBase::GetAccumulator( )
{
	return m_Accumulator;
}

// [BC]
void DWaggleBase::SetAccumulator( fixed_t Accumulator )
{
	m_Accumulator = Accumulator;
}

// [geNia]
fixed_t DWaggleBase::GetAccelerationDelta( )
{
	return m_AccDelta;
}

// [BC]
void DWaggleBase::SetAccelerationDelta( fixed_t AccelerationDelta )
{
	m_AccDelta = AccelerationDelta;
}

// [geNia]
fixed_t DWaggleBase::GetTargetScale( )
{
	return m_TargetScale;
}

// [BC]
void DWaggleBase::SetTargetScale( fixed_t Scale )
{
	m_TargetScale = Scale;
}

// [geNia]
fixed_t DWaggleBase::GetScale( )
{
	return m_Scale;
}

// [BC]
void DWaggleBase::SetScale( fixed_t Scale )
{
	m_Scale = Scale;
}

// [geNia]
fixed_t DWaggleBase::GetScaleDelta( )
{
	return m_ScaleDelta;
}

// [BC]
void DWaggleBase::SetScaleDelta( fixed_t ScaleDelta )
{
	m_ScaleDelta = ScaleDelta;
}

// [geNia]
int DWaggleBase::GetTicker( )
{
	return m_Ticker;
}

// [BC]
void DWaggleBase::SetTicker( int Ticker )
{
	m_Ticker = Ticker;
}

// [geNia]
int DWaggleBase::GetState( )
{
	return m_State;
}

// [BC]
void DWaggleBase::SetState( int State )
{
	m_State = State;
}

//==========================================================================
//
//
//
//==========================================================================
void DWaggleBase::DoWaggle (bool ceiling)
{
	secplane_t *plane;
	int pos;
	fixed_t dist;

	if (ceiling)
	{
		plane = &m_Sector->ceilingplane;
		pos = sector_t::ceiling;
		// [BB] The ceiling is going to be moved in here. Is this the best place to put this?
		// Why do we need these bools anyway? Wouldn't it be better to check the current
		// ceiling/floor values agains the saved intial values?
		m_Sector->bCeilingHeightChange = true;
	}
	else
	{
		plane = &m_Sector->floorplane;
		pos = sector_t::floor;
		// [BB] The floor is going to be moved in here. Is this the best place to put this?
		m_Sector->bFloorHeightChange = true;
	}

	switch (m_State)
	{
	case WGLSTATE_EXPAND:
		if ((m_Scale += m_ScaleDelta) >= m_TargetScale)
		{
			m_Scale = m_TargetScale;
			m_State = WGLSTATE_STABLE;
		}
		break;

	case WGLSTATE_REDUCE:
		if ((m_Scale -= m_ScaleDelta) <= 0)
		{ // Remove
			dist = FixedMul (m_OriginalDist - plane->d, plane->ic);
			m_Sector->ChangePlaneTexZ(pos, -plane->HeightDiff (m_OriginalDist));
			plane->d = m_OriginalDist;
			P_ChangeSector (m_Sector, true, dist, ceiling, false);
			if (ceiling)
			{
				m_Sector->ceilingdata = NULL;
			}
			else
			{
				m_Sector->floordata = NULL;
			}

			Destroy ();

			// [BC] If we're the server, tell clients to delete the waggle.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_DoWaggle( this );

			return;
		}
		break;

	case WGLSTATE_STABLE:
		if (m_Ticker != -1)
		{
			if (!--m_Ticker)
			{
				m_State = WGLSTATE_REDUCE;
			}
		}
		break;
	}
	m_Accumulator += m_AccDelta;


	fixed_t mag = finesine[(m_Accumulator>>9)&8191]*8;

	dist = plane->d;
	plane->d = m_OriginalDist + plane->PointToDist (0, 0, FixedMul (mag, m_Scale));
	m_Sector->ChangePlaneTexZ(pos, plane->HeightDiff (dist));
	dist = plane->HeightDiff (dist);

	// Interesting: Hexen passes 'true' for the crunch parameter which really is crushing damage here...
	// Also, this does not reset the move if it blocks.
	P_Scroll3dMidtex(m_Sector, 1, dist, ceiling);
	P_MoveLinkedSectors(m_Sector, 1, dist, ceiling);
	P_ChangeSector (m_Sector, 1, dist, ceiling, false);

	// [BC] At the peak and troughs of the waggle, update the clients with the current
	// position of the floor/ceiling.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DoWaggle( this );
}

//==========================================================================
//
// FloorWaggle
//
//==========================================================================

DFloorWaggle::DFloorWaggle ()
{
}

DFloorWaggle::DFloorWaggle (sector_t *sec)
	: Super (sec)
{
	sec->floordata = this;
	m_Interpolation = sec->SetInterpolation(sector_t::FloorMove, true);
}

void DFloorWaggle::Tick ()
{
	DoWaggle (false);
}

// [geNia]
fixed_t DFloorWaggle::GetPosition( )
{
	return ( m_Sector->floorplane.d );
}

//==========================================================================
//
// CeilingWaggle
//
//==========================================================================

DCeilingWaggle::DCeilingWaggle ()
{
}

DCeilingWaggle::DCeilingWaggle (sector_t *sec)
	: Super (sec)
{
	sec->ceilingdata = this;
	m_Interpolation = sec->SetInterpolation(sector_t::CeilingMove, true);
}

void DCeilingWaggle::Tick ()
{
	DoWaggle (true);
}

// [geNia]
fixed_t DCeilingWaggle::GetPosition( )
{
	return ( m_Sector->ceilingplane.d );
}

//==========================================================================
//
// EV_StartWaggle
//
//==========================================================================

bool EV_StartWaggle (player_t *instigator, int tag, line_t *line, int height, int speed, int offset,
	int timer, bool ceiling)
{
	if (CLIENT_PREDICT_IsPredicting())
		return false;

	int sectorIndex;
	sector_t *sector;
	DWaggleBase *waggle;
	bool retCode;

	retCode = false;
	sectorIndex = -1;

	if (tag == 0)
	{
		if (!line || !(sector = line->backsector))
			return retCode;
		goto manual_waggle;
	}


	while (tag && (sectorIndex = P_FindSectorFromTag(tag, sectorIndex)) >= 0)
	{
		sector = &sectors[sectorIndex];
manual_waggle:
		if ((!ceiling && sector->PlaneMoving(sector_t::floor)) ||
			(ceiling && sector->PlaneMoving(sector_t::ceiling)))
		{ // Already busy with another thinker
			continue;
		}
		retCode = true;
		if (ceiling)
		{
			waggle = new DCeilingWaggle (sector);
			waggle->m_OriginalDist = sector->ceilingplane.d;
		}
		else
		{
			waggle = new DFloorWaggle (sector);
			waggle->m_OriginalDist = sector->floorplane.d;
		}

		waggle->m_LastInstigator = instigator;
		waggle->m_Accumulator = offset*FRACUNIT;
		waggle->m_AccDelta = speed << (FRACBITS-6);
		waggle->m_Scale = 0;
		waggle->m_TargetScale = height << (FRACBITS-6);
		waggle->m_ScaleDelta = waggle->m_TargetScale
			/(TICRATE+((3*TICRATE)*height)/255);
		waggle->m_Ticker = timer ? timer*TICRATE : -1;
		waggle->m_State = WGLSTATE_EXPAND;

		// [BC] If we're the server, tell clients to do the waggle.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_DoWaggle( waggle );

	}
	return retCode;
}

//*****************************************************************************
//
DFloor *P_GetFloorBySectorNum( LONG sectorNum )
{
	DFloor	*pFloor;

	TThinkerIterator<DFloor>		Iterator;

	while (( pFloor = Iterator.Next( )))
	{
		if ( pFloor->GetSector()->sectornum == sectorNum )
			return ( pFloor );
	}

	return ( NULL );
}

//*****************************************************************************
//
DElevator *P_GetElevatorBySectorNum( LONG sectorNum )
{
	DElevator	*pElevator;

	TThinkerIterator<DElevator>		Iterator;

	while (( pElevator = Iterator.Next( )))
	{
		if ( pElevator->GetSector()->sectornum == sectorNum )
			return ( pElevator );
	}

	return ( NULL );
}

//*****************************************************************************
//
DWaggleBase *P_GetWaggleBySectorNum( LONG sectorNum, bool bCeiling )
{
	if ( bCeiling )
	{
		DCeilingWaggle	*pCeilingWaggle;
	
		TThinkerIterator<DCeilingWaggle>		Iterator;

		while (( pCeilingWaggle = Iterator.Next( )))
		{
			if ( pCeilingWaggle->GetSector()->sectornum == sectorNum )
				return ( pCeilingWaggle );
		}
	}
	else
	{
		DFloorWaggle	*pFloorWaggle;
	
		TThinkerIterator<DFloorWaggle>		Iterator;

		while (( pFloorWaggle = Iterator.Next( )))
		{
			if ( pFloorWaggle->GetSector()->sectornum == sectorNum )
				return ( pFloorWaggle );
		}
	}

	return ( NULL );
}
