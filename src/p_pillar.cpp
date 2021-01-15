/*
** p_pillar.cpp
** Handles pillars
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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
*/

#include "doomdef.h"
#include "p_local.h"
#include "p_spec.h"
#include "g_level.h"
#include "s_sndseq.h"
#include "farchive.h"
#include "r_data/r_interpolate.h"
// [BC] New #includes.
#include "cl_demo.h"
#include "network.h"
#include "sv_commands.h"
#include "cl_main.h"

IMPLEMENT_POINTY_CLASS (DPillar)
	DECLARE_POINTER(m_Interp_Floor)
	DECLARE_POINTER(m_Interp_Ceiling)
END_POINTERS

inline FArchive &operator<< (FArchive &arc, DPillar::EPillar &type)
{
	BYTE val = (BYTE)type;
	arc << val;
	type = (DPillar::EPillar)val;
	return arc;
}

DPillar::DPillar ()
{
}

// [BC]
DPillar::DPillar(sector_t *sector)
	: Super (sector)
{
	sector->floordata = this;
	sector->ceilingdata = this;
	m_Interp_Floor = sector->SetInterpolation(sector_t::FloorMove, true);
	m_Interp_Ceiling = sector->SetInterpolation(sector_t::CeilingMove, true);
	m_LastInstigator = NULL;
}

void DPillar::Destroy()
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

void DPillar::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Type
		<< m_Speed
		<< m_FloorSpeed
		<< m_CeilingSpeed
		<< m_FloorTarget
		<< m_CeilingTarget
		<< m_Crush
		<< m_Hexencrush
		<< m_Interp_Floor
		<< m_Interp_Ceiling;
}

// [BC]
void DPillar::UpdateToClient( ULONG ulClient )
{
	SERVERCOMMANDS_DoPillar( this, ulClient, SVCF_ONLYTHISCLIENT );
}

// [BC]
void DPillar::SetType( EPillar Type )
{
	m_Type = Type;
}

// [BB]
DPillar::EPillar DPillar::GetType( )
{
	return m_Type;
}

// [geNia]
fixed_t DPillar::GetFloorPosition()
{
	return ( m_Sector->floorplane.d );
}

// [geNia]
fixed_t DPillar::GetCeilingPosition()
{
	return ( m_Sector->ceilingplane.d );
}

// [BC]
void DPillar::SetFloorSpeed( fixed_t Speed )
{
	m_FloorSpeed = Speed;
}

// [BB]
fixed_t DPillar::GetFloorSpeed( )
{
	return m_FloorSpeed;
}

// [BC]
void DPillar::SetCeilingSpeed( fixed_t Speed )
{
	m_CeilingSpeed = Speed;
}

// [BB]
fixed_t DPillar::GetCeilingSpeed( )
{
	return m_CeilingSpeed;
}

// [BC]
void DPillar::SetFloorTarget( fixed_t Target )
{
	m_FloorTarget = Target;
}

// [BB]
fixed_t DPillar::GetFloorTarget( )
{
	return m_FloorTarget;
}

// [BC]
void DPillar::SetCeilingTarget( fixed_t Target )
{
	m_CeilingTarget = Target;
}

// [BB]
fixed_t DPillar::GetCeilingTarget( )
{
	return m_CeilingTarget;
}

void DPillar::SetCrush( LONG lCrush )
{
	m_Crush = lCrush;
}

int DPillar::GetCrush( void )
{
	return ( m_Crush );
}

void DPillar::SetHexencrush( bool Hexencrush )
{
	m_Hexencrush = Hexencrush;
}

bool DPillar::GetHexencrush( void )
{
	return ( m_Hexencrush );
}

void DPillar::Tick ()
{
	int r, s;
	fixed_t oldfloor, oldceiling;

	oldfloor = m_Sector->floorplane.d;
	oldceiling = m_Sector->ceilingplane.d;

	if (m_Type == pillarBuild)
	{
		r = MoveFloor (m_FloorSpeed, m_FloorTarget, m_Crush, 1, m_Hexencrush);
		s = MoveCeiling (m_CeilingSpeed, m_CeilingTarget, m_Crush, -1, m_Hexencrush);
	}
	else
	{
		r = MoveFloor (m_FloorSpeed, m_FloorTarget, m_Crush, -1, m_Hexencrush);
		s = MoveCeiling (m_CeilingSpeed, m_CeilingTarget, m_Crush, 1, m_Hexencrush);
	}

	if (r == pastdest && s == pastdest)
	{
		SN_StopSequence (m_Sector, CHAN_FLOOR);
		Destroy();

		// [BC] If we're the server, tell clients to destroy the pillar, and to stop
		// the sector sound sequence.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_DoPillar( this );
	}
	else
	{
		if (r == crushed)
		{
			MoveFloor (m_FloorSpeed, oldfloor, -1, -1, m_Hexencrush);
		}
		if (s == crushed)
		{
			MoveCeiling (m_CeilingSpeed, oldceiling, -1, 1, m_Hexencrush);
		}
	}
}

DPillar::DPillar (sector_t *sector, EPillar type, fixed_t speed,
				  fixed_t floordist, fixed_t ceilingdist, int crush, bool hexencrush)
	: DMover (sector)
{
	fixed_t newheight;
	vertex_t *spot;

	sector->floordata = sector->ceilingdata = this;
	m_Interp_Floor = sector->SetInterpolation(sector_t::FloorMove, true);
	m_Interp_Ceiling = sector->SetInterpolation(sector_t::CeilingMove, true);

	m_Type = type;
	m_Crush = crush;
	m_Hexencrush = hexencrush;

	if (type == pillarBuild)
	{
		// If the pillar height is 0, have the floor and ceiling meet halfway
		if (floordist == 0)
		{
			newheight = (sector->CenterFloor () + sector->CenterCeiling ()) / 2;
			m_FloorTarget = sector->floorplane.PointToDist (sector->soundorg[0], sector->soundorg[1], newheight);
			m_CeilingTarget = sector->ceilingplane.PointToDist (sector->soundorg[0], sector->soundorg[1], newheight);
			floordist = newheight - sector->CenterFloor ();
		}
		else
		{
			newheight = sector->CenterFloor () + floordist;
			m_FloorTarget = sector->floorplane.PointToDist (sector->soundorg[0], sector->soundorg[1], newheight);
			m_CeilingTarget = sector->ceilingplane.PointToDist (sector->soundorg[0], sector->soundorg[1], newheight);
		}
		ceilingdist = sector->CenterCeiling () - newheight;
	}
	else
	{
		// If one of the heights is 0, figure it out based on the
		// surrounding sectors
		if (floordist == 0)
		{
			newheight = sector->FindLowestFloorSurrounding (&spot);
			m_FloorTarget = sector->floorplane.PointToDist (spot, newheight);
			floordist = sector->floorplane.ZatPoint (spot) - newheight;
		}
		else
		{
			newheight = sector->floorplane.ZatPoint (0, 0) - floordist;
			m_FloorTarget = sector->floorplane.PointToDist (0, 0, newheight);
		}
		if (ceilingdist == 0)
		{
			newheight = sector->FindHighestCeilingSurrounding (&spot);
			m_CeilingTarget = sector->ceilingplane.PointToDist (spot, newheight);
			ceilingdist = newheight - sector->ceilingplane.ZatPoint (spot);
		}
		else
		{
			newheight = sector->ceilingplane.ZatPoint (0, 0) + ceilingdist;
			m_CeilingTarget = sector->ceilingplane.PointToDist (0, 0, newheight);
		}
	}

	// The speed parameter applies to whichever part of the pillar
	// travels the farthest. The other part's speed is then set so
	// that it arrives at its destination at the same time.
	if (floordist > ceilingdist)
	{
		m_FloorSpeed = speed;
		m_CeilingSpeed = Scale (speed, ceilingdist, floordist);
	}
	else
	{
		m_CeilingSpeed = speed;
		m_FloorSpeed = Scale (speed, floordist, ceilingdist);
	}

	if (sector->seqType >= 0)
	{
		SN_StartSequence (sector, CHAN_FLOOR, sector->seqType, SEQ_PLATFORM, 0);
	}
	else if (sector->SeqName != NAME_None)
	{
		SN_StartSequence (sector, CHAN_FLOOR, sector->SeqName, 0);
	}
	else
	{
		SN_StartSequence (sector, CHAN_FLOOR, "Floor", 0);
	}
}

bool EV_DoPillar (player_t *instigator, DPillar::EPillar type, int tag, fixed_t speed, fixed_t height,
				  fixed_t height2, int crush, bool hexencrush)
{
	bool rtn = false;
	int secnum = -1;
	// [BC]
	DPillar		*pPillar;

	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0)
	{
		sector_t *sec = &sectors[secnum];

		if (sec->PlaneMoving(sector_t::floor) || sec->PlaneMoving(sector_t::ceiling))
			continue;

		fixed_t flor, ceil;

		flor = sec->CenterFloor ();
		ceil = sec->CenterCeiling ();

		if (type == DPillar::pillarBuild && flor == ceil)
			continue;

		if (type == DPillar::pillarOpen && flor != ceil)
			continue;

		rtn = true;
		pPillar = new DPillar (sec, type, speed, height, height2, crush, hexencrush);

		if ( pPillar )
		{
			pPillar->m_LastInstigator = instigator;

			// [BC] If we're the server, tell clients to create the pillar.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_DoPillar( pPillar );
		}
	}
	return rtn;
}

//*****************************************************************************
//
DPillar *P_GetPillarBySectorNum( LONG sectorNum )
{
	DPillar	*pPillar;

	TThinkerIterator<DPillar>		Iterator;

	while (( pPillar = Iterator.Next( )))
	{
		if ( pPillar->GetSector()->sectornum == sectorNum )
			return ( pPillar );
	}

	return ( NULL );
}
