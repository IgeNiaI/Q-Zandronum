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
//		Base class for effects on sectors.
//		[RH] Created this class hierarchy.
//
//-----------------------------------------------------------------------------

#include "dsectoreffect.h"
#include "gi.h"
#include "p_local.h"
#include "p_3dmidtex.h"
#include "r_data/r_interpolate.h"
#include "statnums.h"
#include "farchive.h"
#include "cl_demo.h"
#include "cl_main.h"

IMPLEMENT_CLASS (DSectorEffect)

DSectorEffect::DSectorEffect ()
: DThinker(STAT_SECTOREFFECT)
{
	m_Sector = NULL;
}

void DSectorEffect::Destroy()
{
	if (m_Sector)
	{
		if (m_Sector->floordata == this)
		{
			m_Sector->floordata = NULL;
		}
		if (m_Sector->ceilingdata == this)
		{
			m_Sector->ceilingdata = NULL;
		}
		if (m_Sector->lightingdata == this)
		{
			m_Sector->lightingdata = NULL;
		}
	}
	Super::Destroy();
}

DSectorEffect::DSectorEffect (sector_t *sector)
{
	m_Sector = sector;
}

void DSectorEffect::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Sector;
}

player_t* DSectorEffect::GetLastInstigator()
{
	return m_LastInstigator;
}

void DSectorEffect::SetLastInstigator( player_t* player )
{
	m_LastInstigator = player;
}

IMPLEMENT_POINTY_CLASS (DMover)
	DECLARE_POINTER(interpolation)
END_POINTERS

DMover::DMover ()
{
}

DMover::DMover (sector_t *sector)
	: DSectorEffect (sector)
{
	interpolation = NULL;
}

void DMover::Predict()
{
	// Use a version of gametic that's appropriate for both the current game and demos.
	ULONG TicsToPredict = gametic - CLIENTDEMO_GetGameticOffset( );

	// [geNia] This would mean that a negative amount of prediction tics is needed, so something is wrong.
	// So far it looks like the "lagging at connect / map start" prevented this from happening before.
	if ( CLIENT_GetLastConsolePlayerUpdateTick() > TicsToPredict)
		return;

	// How many ticks of prediction do we need?
	TicsToPredict = TicsToPredict - CLIENT_GetLastConsolePlayerUpdateTick( );

	while (TicsToPredict && !(ObjectFlags & OF_EuthanizeMe))
	{
		Tick();
		TicsToPredict--;
	}
}

void DMover::Destroy()
{
	StopInterpolation();
	Super::Destroy();
}

void DMover::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << interpolation;
}

void DMover::StopInterpolation()
{
	if (interpolation != NULL)
	{
		interpolation->DelRef();
		interpolation = NULL;
	}
}



IMPLEMENT_CLASS (DMovingFloor)

DMovingFloor::DMovingFloor ()
{
}

DMovingFloor::DMovingFloor (sector_t *sector)
	: DMover (sector)
{
	sector->floordata = this;
	interpolation = sector->SetInterpolation(sector_t::FloorMove, true);
}

IMPLEMENT_CLASS (DMovingCeiling)

DMovingCeiling::DMovingCeiling ()
{
}

DMovingCeiling::DMovingCeiling (sector_t *sector)
	: DMover (sector)
{
	sector->ceilingdata = this;
	interpolation = sector->SetInterpolation(sector_t::CeilingMove, true);
}

bool DMover::MoveAttached(int crush, fixed_t move, int floorOrCeiling, bool resetfailed)
{
	if (!P_Scroll3dMidtex(m_Sector, crush, move, !!floorOrCeiling) && resetfailed)
	{
		P_Scroll3dMidtex(m_Sector, crush, -move, !!floorOrCeiling);
		return false;
	}
	if (!P_MoveLinkedSectors(m_Sector, crush, move, !!floorOrCeiling) && resetfailed)
	{
		P_MoveLinkedSectors(m_Sector, crush, -move, !!floorOrCeiling);
		P_Scroll3dMidtex(m_Sector, crush, -move, !!floorOrCeiling);
		return false;
	}
	return true;
}

//
// Move a plane (floor or ceiling) and check for crushing
// [RH] Crush specifies the actual amount of crushing damage inflictable.
//		(Use -1 to prevent it from trying to crush)
//		dest is the desired d value for the plane
//
DMover::EResult DMover::MovePlane (fixed_t speed, fixed_t dest, int crush,
								   int floorOrCeiling, int direction, bool hexencrush)
{
	bool	 	flag;
	fixed_t 	lastpos;
	fixed_t		movedest;
	fixed_t		move;
	//fixed_t		destheight;	//jff 02/04/98 used to keep floors/ceilings
							// from moving thru each other

	// [BC] Flag this sector's height as changed, so we can tell new clients that connect the
	// new height.
	m_Sector->floorOrCeiling = floorOrCeiling;
	if ( floorOrCeiling == 0 )
		m_Sector->bFloorHeightChange = true;
	else
		m_Sector->bCeilingHeightChange = true;

	switch (floorOrCeiling)
	{
	case 0:
		// FLOOR
		lastpos = m_Sector->floorplane.d;
		switch (direction)
		{
		case -1:
			// DOWN
			movedest = m_Sector->floorplane.GetChangedHeight (-speed);
			if (movedest >= dest)
			{
				move = m_Sector->floorplane.HeightDiff (lastpos, dest);

				if (!MoveAttached(crush, move, 0, true)) return crushed;

				m_Sector->floorplane.d = dest;
				flag = P_ChangeSector (m_Sector, crush, move, 0, false);
				if (flag)
				{
					m_Sector->floorplane.d = lastpos;
					P_ChangeSector (m_Sector, crush, -move, 0, true);
					MoveAttached(crush, -move, 0, false);
				}
				else
				{
					m_Sector->ChangePlaneTexZ(sector_t::floor, move);
					m_Sector->AdjustFloorClip ();
				}
				return pastdest;
			}
			else
			{
				if (!MoveAttached(crush, -speed, 0, true)) return crushed;

				m_Sector->floorplane.d = movedest;

				flag = P_ChangeSector (m_Sector, crush, -speed, 0, false);
				if (flag)
				{
					m_Sector->floorplane.d = lastpos;
					P_ChangeSector (m_Sector, crush, speed, 0, true);
					MoveAttached(crush, speed, 0, false);
					return crushed;
				}
				else
				{
					m_Sector->ChangePlaneTexZ(sector_t::floor, m_Sector->floorplane.HeightDiff (lastpos));
					m_Sector->AdjustFloorClip ();
				}
			}
			break;
												
		case 1:
			// UP
			// jff 02/04/98 keep floor from moving thru ceilings
			// [RH] not so easy with arbitrary planes
			//destheight = (dest < m_Sector->ceilingheight) ? dest : m_Sector->ceilingheight;
			if ((m_Sector->ceilingplane.a | m_Sector->ceilingplane.b |
				 m_Sector->floorplane.a | m_Sector->floorplane.b) == 0 &&
				(!(i_compatflags2 & COMPATF2_FLOORMOVE) && -dest > m_Sector->ceilingplane.d))
			{
				dest = -m_Sector->ceilingplane.d;
			}

			movedest = m_Sector->floorplane.GetChangedHeight (speed);

			if (movedest <= dest)
			{
				move = m_Sector->floorplane.HeightDiff (lastpos, dest);

				if (!MoveAttached(crush, move, 0, true)) return crushed;

				m_Sector->floorplane.d = dest;

				flag = P_ChangeSector (m_Sector, crush, move, 0, false);
				if (flag)
				{
					m_Sector->floorplane.d = lastpos;
					P_ChangeSector (m_Sector, crush, -move, 0, true);
					MoveAttached(crush, -move, 0, false);
				}
				else
				{
					m_Sector->ChangePlaneTexZ(sector_t::floor, move);
					m_Sector->AdjustFloorClip ();
				}
				return pastdest;
			}
			else
			{
				if (!MoveAttached(crush, speed, 0, true)) return crushed;

				m_Sector->floorplane.d = movedest;

				// COULD GET CRUSHED
				flag = P_ChangeSector (m_Sector, crush, speed, 0, false);
				if (flag)
				{
					if (crush >= 0 && !hexencrush)
					{
						m_Sector->ChangePlaneTexZ(sector_t::floor, m_Sector->floorplane.HeightDiff (lastpos));
						m_Sector->AdjustFloorClip ();
						return crushed;
					}
					m_Sector->floorplane.d = lastpos;
					P_ChangeSector (m_Sector, crush, -speed, 0, true);
					MoveAttached(crush, -speed, 0, false);
					return crushed;
				}
				m_Sector->ChangePlaneTexZ(sector_t::floor, m_Sector->floorplane.HeightDiff (lastpos));
				m_Sector->AdjustFloorClip ();
			}
			break;
		}
		break;
																		
	  case 1:
		// CEILING
		lastpos = m_Sector->ceilingplane.d;
		switch (direction)
		{
		case -1:
			// DOWN
			// jff 02/04/98 keep ceiling from moving thru floors
			// [RH] not so easy with arbitrary planes
			//destheight = (dest > m_Sector->floorheight) ? dest : m_Sector->floorheight;
			if ((m_Sector->ceilingplane.a | m_Sector->ceilingplane.b |
				 m_Sector->floorplane.a | m_Sector->floorplane.b) == 0 &&
				(!(i_compatflags2 & COMPATF2_FLOORMOVE) && dest < -m_Sector->floorplane.d))
			{
				dest = -m_Sector->floorplane.d;
			}
			movedest = m_Sector->ceilingplane.GetChangedHeight (-speed);
			if (movedest <= dest)
			{
				move = m_Sector->ceilingplane.HeightDiff (lastpos, dest);

				if (!MoveAttached(crush, move, 1, true)) return crushed;

				m_Sector->ceilingplane.d = dest;
				flag = P_ChangeSector (m_Sector, crush, move, 1, false);

				if (flag)
				{
					m_Sector->ceilingplane.d = lastpos;
					P_ChangeSector (m_Sector, crush, -move, 1, true);
					MoveAttached(crush, -move, 1, false);
				}
				else
				{
					m_Sector->ChangePlaneTexZ(sector_t::ceiling, move);
				}
				return pastdest;
			}
			else
			{
				if (!MoveAttached(crush, -speed, 1, true)) return crushed;

				m_Sector->ceilingplane.d = movedest;

				// COULD GET CRUSHED
				flag = P_ChangeSector (m_Sector, crush, -speed, 1, false);
				if (flag)
				{
					if (crush >= 0 && !hexencrush)
					{
						m_Sector->ChangePlaneTexZ(sector_t::ceiling, m_Sector->ceilingplane.HeightDiff (lastpos));
						return crushed;
					}
					m_Sector->ceilingplane.d = lastpos;
					P_ChangeSector (m_Sector, crush, speed, 1, true);
					MoveAttached(crush, speed, 1, false);
					return crushed;
				}
				m_Sector->ChangePlaneTexZ(sector_t::ceiling, m_Sector->ceilingplane.HeightDiff (lastpos));
			}
			break;
												
		case 1:
			// UP
			movedest = m_Sector->ceilingplane.GetChangedHeight (speed);
			if (movedest >= dest)
			{
				move = m_Sector->ceilingplane.HeightDiff (lastpos, dest);

				if (!MoveAttached(crush, move, 1, true)) return crushed;

				m_Sector->ceilingplane.d = dest;

				flag = P_ChangeSector (m_Sector, crush, move, 1, false);
				if (flag)
				{
					m_Sector->ceilingplane.d = lastpos;
					P_ChangeSector (m_Sector, crush, move, 1, true);
					MoveAttached(crush, move, 1, false);
				}
				else
				{
					m_Sector->ChangePlaneTexZ(sector_t::ceiling, move);
				}
				return pastdest;
			}
			else
			{
				if (!MoveAttached(crush, speed, 1, true)) return crushed;

				m_Sector->ceilingplane.d = movedest;

				flag = P_ChangeSector (m_Sector, crush, speed, 1, false);
				if (flag)
				{
					m_Sector->ceilingplane.d = lastpos;
					P_ChangeSector (m_Sector, crush, -speed, 1, true);
					MoveAttached(crush, -speed, 1, false);
					return crushed;
				}
				m_Sector->ChangePlaneTexZ(sector_t::ceiling, m_Sector->ceilingplane.HeightDiff (lastpos));
			}
			break;
		}
		break;
				
	}
	return ok;
}

void P_SetFloorPlane (sector_t *sector, fixed_t dest)
{
	// Calculate the change in floor height.
	fixed_t delta = dest - sector->floorplane.d;

	// Store the original height position.
	fixed_t lastPos = sector->floorplane.d;

	// Change the height.
	sector->floorplane.ChangeHeight(-delta);

	// Call this to update various actor's within the sector.
	P_ChangeSector(sector, false, -delta, 0, false);

	// Finally, adjust textures.
	sector->SetPlaneTexZ(sector_t::floor, sector->GetPlaneTexZ(sector_t::floor) + sector->floorplane.HeightDiff(lastPos));

	// [BB] We also need to move any linked sectors.
	P_MoveLinkedSectors(sector, false, -delta, false);
}

void P_SetCeilingPlane(sector_t *sector, fixed_t dest)
{
	// Calculate the change in ceiling height.
	fixed_t delta = dest - sector->ceilingplane.d;

	// Store the original height position.
	fixed_t lastPos = sector->ceilingplane.d;

	// Change the height.
	sector->ceilingplane.ChangeHeight(delta);

	// Finally, adjust textures.
	sector->SetPlaneTexZ(sector_t::ceiling, sector->GetPlaneTexZ(sector_t::ceiling) + sector->ceilingplane.HeightDiff(lastPos));

	// [BB] We also need to move any linked sectors.
	P_MoveLinkedSectors(sector, false, delta, true);
}
