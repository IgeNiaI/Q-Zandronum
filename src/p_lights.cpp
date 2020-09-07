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
//		Handle Sector base lighting effects.
//
//-----------------------------------------------------------------------------


#include "templates.h"
#include "m_random.h"

#include "doomdef.h"
#include "p_local.h"
// [BB] New #includes.
#include "network.h"
#include "sv_commands.h"
#include "g_level.h"

#include "p_lnspec.h"

// State.
#include "r_state.h"
#include "statnums.h"
#include "farchive.h"

static FRandom pr_flicker ("Flicker");
static FRandom pr_lightflash ("LightFlash");
static FRandom pr_strobeflash ("StrobeFlash");
static FRandom pr_fireflicker ("FireFlicker");

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

IMPLEMENT_CLASS (DLighting)

DLighting::DLighting ()
{
	// [BB] Workaround to save whether this lighting was created by the map.
	bNotMapSpawned = ( level.time > 0 );
}

DLighting::DLighting (sector_t *sector)
	: DSectorEffect (sector)
{
	ChangeStatNum (STAT_LIGHT);

	// [BB] Workaround to save whether this lighting was created by the map.
	bNotMapSpawned = ( level.time > 0 );
}

//-----------------------------------------------------------------------------
//
// FIRELIGHT FLICKER
//
//-----------------------------------------------------------------------------

IMPLEMENT_CLASS (DFireFlicker)

DFireFlicker::DFireFlicker ()
{
}

void DFireFlicker::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Count << m_MaxLight << m_MinLight;
}


//-----------------------------------------------------------------------------
//
// T_FireFlicker
//
//-----------------------------------------------------------------------------

void DFireFlicker::Tick ()
{
	int amount;

	if (--m_Count == 0)
	{
		amount = (pr_fireflicker() & 3) << 4;

		// [RH] Shouldn't this be (m_MaxLight - amount < m_MinLight)?
		if (m_Sector->lightlevel - amount < m_MinLight)
			m_Sector->SetLightLevel(m_MinLight);
		else
			m_Sector->SetLightLevel(m_MaxLight - amount);

		m_Count = 4;
	}
}

// [BC]
void DFireFlicker::UpdateToClient( ULONG ulClient )
{
	SERVERCOMMANDS_DoSectorLightFireFlicker( ULONG( m_Sector - sectors ), m_MaxLight, m_MinLight, ulClient, SVCF_ONLYTHISCLIENT );
}

//-----------------------------------------------------------------------------
//
// P_SpawnFireFlicker
//
//-----------------------------------------------------------------------------

DFireFlicker::DFireFlicker (sector_t *sector)
	: DLighting (sector)
{
	m_MaxLight = sector->lightlevel;
	m_MinLight = sector_t::ClampLight(sector->FindMinSurroundingLight(sector->lightlevel) + 16);
	m_Count = 4;

	// [BC] If we're the server, tell clients to create the fire flicker.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DoSectorLightFireFlicker( ULONG( sector - sectors ), m_MaxLight, m_MinLight );
}

DFireFlicker::DFireFlicker (sector_t *sector, int upper, int lower)
	: DLighting (sector)
{
	m_MaxLight = sector_t::ClampLight(upper);
	m_MinLight = sector_t::ClampLight(lower);
	m_Count = 4;

	// [BC] If we're the server, tell clients to create the fire flicker.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DoSectorLightFireFlicker( ULONG( sector - sectors ), m_MaxLight, m_MinLight );
}

//-----------------------------------------------------------------------------
//
// [RH] flickering light like Hexen's
//
//-----------------------------------------------------------------------------

IMPLEMENT_CLASS (DFlicker)

DFlicker::DFlicker ()
{
}

void DFlicker::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Count << m_MaxLight << m_MinLight;
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void DFlicker::Tick ()
{
	if (m_Count)
	{
		m_Count--;	
	}
	else if (m_Sector->lightlevel == m_MaxLight)
	{
		m_Sector->SetLightLevel(m_MinLight);
		m_Count = (pr_flicker()&7)+1;
	}
	else
	{
		m_Sector->SetLightLevel(m_MaxLight);
		m_Count = (pr_flicker()&31)+1;
	}
}

// [BC]
void DFlicker::UpdateToClient( ULONG ulClient )
{
	SERVERCOMMANDS_DoSectorLightFlicker( ULONG( m_Sector - sectors ), m_MaxLight, m_MinLight, ulClient, SVCF_ONLYTHISCLIENT );
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

DFlicker::DFlicker (sector_t *sector, int upper, int lower)
	: DLighting (sector)
{
	m_MaxLight = sector_t::ClampLight(upper);
	m_MinLight = sector_t::ClampLight(lower);
	sector->lightlevel = m_MaxLight;
	m_Count = (pr_flicker()&64)+1;

	// [BC] If we're the server, tell clients to create the flicker.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DoSectorLightFlicker( ULONG( sector - sectors ), m_MaxLight, m_MinLight );
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void EV_StartLightFlickering (int tag, int upper, int lower)
{
	int secnum;
		
	secnum = -1;
	while ((secnum = P_FindSectorFromTag (tag,secnum)) >= 0)
	{
		new DFlicker (&sectors[secnum], upper, lower);
	}
}


//-----------------------------------------------------------------------------
//
// BROKEN LIGHT FLASHING
//
//-----------------------------------------------------------------------------

IMPLEMENT_CLASS (DLightFlash)

DLightFlash::DLightFlash ()
{
}

void DLightFlash::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Count << m_MaxLight << m_MaxTime << m_MinLight << m_MinTime;
}

//-----------------------------------------------------------------------------
//
// T_LightFlash
// Do flashing lights.
//
//-----------------------------------------------------------------------------

void DLightFlash::Tick ()
{
	if (--m_Count == 0)
	{
		if (m_Sector->lightlevel == m_MaxLight)
		{
			m_Sector->SetLightLevel(m_MinLight);
			m_Count = (pr_lightflash() & m_MinTime) + 1;
		}
		else
		{
			m_Sector->SetLightLevel(m_MaxLight);
			m_Count = (pr_lightflash() & m_MaxTime) + 1;
		}
	}
}

// [BC]
void DLightFlash::UpdateToClient( ULONG ulClient )
{
	SERVERCOMMANDS_DoSectorLightLightFlash( ULONG( m_Sector - sectors ), m_MaxLight, m_MinLight, ulClient, SVCF_ONLYTHISCLIENT ); 
}

//-----------------------------------------------------------------------------
//
// P_SpawnLightFlash
//
//-----------------------------------------------------------------------------

DLightFlash::DLightFlash (sector_t *sector)
	: DLighting (sector)
{
	// Find light levels like Doom.
	m_MaxLight = sector->lightlevel;
	m_MinLight = sector->FindMinSurroundingLight (sector->lightlevel);
	m_MaxTime = 64;
	m_MinTime = 7;
	m_Count = (pr_lightflash() & m_MaxTime) + 1;

	// [BC] If we're the server, tell clients to create the light flash.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DoSectorLightLightFlash( ULONG( sector - sectors ), m_MaxLight, m_MinLight );
}
	
DLightFlash::DLightFlash (sector_t *sector, int min, int max)
	: DLighting (sector)
{
	// Use specified light levels.
	m_MaxLight = sector_t::ClampLight(max);
	m_MinLight = sector_t::ClampLight(min);
	m_MaxTime = 64;
	m_MinTime = 7;
	m_Count = (pr_lightflash() & m_MaxTime) + 1;

	// [BC] If we're the server, tell clients to create the light flash.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DoSectorLightLightFlash( ULONG( sector - sectors ), m_MaxLight, m_MinLight );
}


//-----------------------------------------------------------------------------
//
// STROBE LIGHT FLASHING
//
//-----------------------------------------------------------------------------

IMPLEMENT_CLASS (DStrobe)

DStrobe::DStrobe ()
{
}

void DStrobe::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Count << m_MaxLight << m_MinLight << m_DarkTime << m_BrightTime;
}

//-----------------------------------------------------------------------------
//
// T_StrobeFlash
//
//-----------------------------------------------------------------------------

void DStrobe::Tick ()
{
	if (--m_Count == 0)
	{
		if (m_Sector->lightlevel == m_MinLight)
		{
			m_Sector->SetLightLevel(m_MaxLight);
			m_Count = m_BrightTime;
		}
		else
		{
			m_Sector->SetLightLevel(m_MinLight);
			m_Count = m_DarkTime;
		}
	}
}

// [BC]
void DStrobe::UpdateToClient( ULONG ulClient )
{
	SERVERCOMMANDS_DoSectorLightStrobe( ULONG( m_Sector - sectors ), m_DarkTime, m_BrightTime, m_MaxLight, m_MinLight, m_Count, ulClient, SVCF_ONLYTHISCLIENT ); 
}

// [BC]
void DStrobe::SetCount( LONG lCount )
{
	m_Count = lCount;
}

//-----------------------------------------------------------------------------
//
// Hexen-style constructor
//
//-----------------------------------------------------------------------------

DStrobe::DStrobe (sector_t *sector, int upper, int lower, int utics, int ltics)
	: DLighting (sector)
{
	m_DarkTime = ltics;
	m_BrightTime = utics;
	m_MaxLight = sector_t::ClampLight(upper);
	m_MinLight = sector_t::ClampLight(lower);
	m_Count = 1;	// Hexen-style is always in sync

	// [BC] If we're the server, tell clients to create the strobe light.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DoSectorLightStrobe( ULONG( sector - sectors ), m_DarkTime, m_BrightTime, m_MaxLight, m_MinLight, m_Count );
}

//-----------------------------------------------------------------------------
//
// Doom-style constructor
//
//-----------------------------------------------------------------------------

DStrobe::DStrobe (sector_t *sector, int utics, int ltics, bool inSync)
	: DLighting (sector)
{
	m_DarkTime = ltics;
	m_BrightTime = utics;

	m_MaxLight = sector->lightlevel;
	m_MinLight = sector->FindMinSurroundingLight (sector->lightlevel);

	if (m_MinLight == m_MaxLight)
		m_MinLight = 0;

	m_Count = inSync ? 1 : (pr_strobeflash() & 7) + 1;

	// [BC] If we're the server, tell clients to create the strobe light.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DoSectorLightStrobe( ULONG( sector - sectors ), m_DarkTime, m_BrightTime, m_MaxLight, m_MinLight, m_Count );
}



//-----------------------------------------------------------------------------
//
// Start strobing lights (usually from a trigger)
// [RH] Made it more configurable.
//
//-----------------------------------------------------------------------------

void EV_StartLightStrobing (int tag, int upper, int lower, int utics, int ltics)
{
	int secnum;
		
	secnum = -1;
	while ((secnum = P_FindSectorFromTag (tag,secnum)) >= 0)
	{
		sector_t *sec = &sectors[secnum];
		if (sec->lightingdata)
			continue;
		
		new DStrobe (sec, upper, lower, utics, ltics);
	}
}

void EV_StartLightStrobing (int tag, int utics, int ltics)
{
	int secnum;
		
	secnum = -1;
	while ((secnum = P_FindSectorFromTag (tag,secnum)) >= 0)
	{
		sector_t *sec = &sectors[secnum];
		if (sec->lightingdata)
			continue;
		
		new DStrobe (sec, utics, ltics, false);
	}
}


//-----------------------------------------------------------------------------
//
// TURN LINE'S TAG LIGHTS OFF
// [RH] Takes a tag instead of a line
//
//-----------------------------------------------------------------------------

void EV_TurnTagLightsOff (int tag)
{
	int i;
	int secnum;

	// [RH] Don't do a linear search
	for (secnum = -1; (secnum = P_FindSectorFromTag (tag, secnum)) >= 0; ) 
	{
		sector_t *sector = sectors + secnum;
		int min = sector->lightlevel;

		for (i = 0; i < sector->linecount; i++)
		{
			sector_t *tsec = getNextSector (sector->lines[i],sector);
			if (!tsec)
				continue;
			if (tsec->lightlevel < min)
				min = tsec->lightlevel;
		}
		sector->SetLightLevel(min);

		// [BC] Flag the sector as having its light level altered. That way, when clients
		// connect, we can tell them about the updated light level.
		sector->bLightChange = true;

		// [BC] If we're the server, tell clients about the light level change.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetSectorLightLevel( secnum );
	}
}


//-----------------------------------------------------------------------------
//
// TURN LINE'S TAG LIGHTS ON
// [RH] Takes a tag instead of a line
//
//-----------------------------------------------------------------------------

void EV_LightTurnOn (int tag, int bright)
{
	int secnum = -1;

	// [RH] Don't do a linear search
	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0) 
	{
		sector_t *sector = sectors + secnum;
		int tbright = bright; //jff 5/17/98 search for maximum PER sector

		// bright = -1 means to search ([RH] Not 0)
		// for highest light level
		// surrounding sector
		if (bright < 0)
		{
			int j;

			for (j = 0; j < sector->linecount; j++)
			{
				sector_t *temp = getNextSector (sector->lines[j], sector);

				if (!temp)
					continue;

				if (temp->lightlevel > tbright)
					tbright = temp->lightlevel;
			}
		}
		sector->SetLightLevel(tbright);

		//jff 5/17/98 unless compatibility optioned
		//then maximum near ANY tagged sector
		if (i_compatflags & COMPATF_LIGHT)
		{
			bright = tbright;
		}

		// [BC] Flag the sector as having its light level altered. That way, when clients
		// connect, we can tell them about the updated light level.
		sector->bLightChange = true;

		// [BC] If we're the server, tell clients about the light level change.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetSectorLightLevel( secnum );
	}
}

//-----------------------------------------------------------------------------
//
// killough 10/98
//
// EV_LightTurnOnPartway
//
// Turn sectors tagged to line lights on to specified or max neighbor level
//
// Passed the tag of sector(s) to light and a light level fraction between 0 and 1.
// Sets the light to min on 0, max on 1, and interpolates in-between.
// Used for doors with gradual lighting effects.
//
//-----------------------------------------------------------------------------

void EV_LightTurnOnPartway (int tag, fixed_t frac)
{
	int i;

	frac = clamp<fixed_t> (frac, 0, FRACUNIT);

	// Search all sectors for ones with same tag as activating line
	i = -1;
	while ((i = P_FindSectorFromTag (tag, i)) >= 0)
	{
		sector_t *temp, *sector = sectors + i;
		int j, bright = 0, min = sector->lightlevel;

		for (j = 0; j < sector->linecount; ++j)
		{
			if ((temp = getNextSector (sector->lines[j], sector)) != NULL)
			{
				if (temp->lightlevel > bright)
				{
					bright = temp->lightlevel;
				}
				if (temp->lightlevel < min)
				{
					min = temp->lightlevel;
				}
			}
		}
		sector->SetLightLevel(DMulScale16 (frac, bright, FRACUNIT-frac, min));
	}
}


//-----------------------------------------------------------------------------
//
// [RH] New function to adjust tagged sectors' light levels
//		by a relative amount. Light levels are clipped to
//		be within range for sector_t::lightlevel.
//
//-----------------------------------------------------------------------------

void EV_LightChange (int tag, int value)
{
	int secnum = -1;

	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0)
	{
		sectors[secnum].SetLightLevel(sectors[secnum].lightlevel + value);

		// [BC] Flag the sector as having its light level altered. That way, when clients
		// connect, we can tell them about the updated light level.
		sectors[secnum].bLightChange = true;

		// [BC] If we're the server, tell clients about the light level change.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetSectorLightLevel( secnum );
	}
}

	
//-----------------------------------------------------------------------------
//
// Spawn glowing light
//
//-----------------------------------------------------------------------------

IMPLEMENT_CLASS (DGlow)

DGlow::DGlow ()
{
}

void DGlow::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Direction << m_MaxLight << m_MinLight;
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void DGlow::Tick ()
{
	int newlight = m_Sector->lightlevel;

	switch (m_Direction)
	{
	case -1:
		// DOWN
		newlight -= GLOWSPEED;
		if (newlight <= m_MinLight)
		{
			newlight += GLOWSPEED;
			m_Direction = 1;
		}
		break;
		
	case 1:
		// UP
		newlight += GLOWSPEED;
		if (newlight >= m_MaxLight)
		{
			newlight -= GLOWSPEED;
			m_Direction = -1;
		}
		break;
	}
	m_Sector->SetLightLevel(newlight);
}

// [BC]
void DGlow::UpdateToClient( ULONG ulClient )
{
	SERVERCOMMANDS_DoSectorLightGlow( ULONG( m_Sector - sectors ), ulClient, SVCF_ONLYTHISCLIENT );
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

DGlow::DGlow (sector_t *sector)
	: DLighting (sector)
{
	m_MinLight = sector->FindMinSurroundingLight (sector->lightlevel);
	m_MaxLight = sector->lightlevel;
	m_Direction = -1;

	// [BC] If we're the server, tell clients to create the glow light.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DoSectorLightGlow( ULONG( sector - sectors ));
}

//-----------------------------------------------------------------------------
//
// [RH] More glowing light, this time appropriate for Hexen-ish uses.
//
//-----------------------------------------------------------------------------

IMPLEMENT_CLASS (DGlow2)

DGlow2::DGlow2 ()
{
}

void DGlow2::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_End << m_MaxTics << m_OneShot << m_Start << m_Tics;
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void DGlow2::Tick ()
{
	if (m_Tics++ >= m_MaxTics)
	{
		if (m_OneShot)
		{
			m_Sector->SetLightLevel(m_End);

			// [BC] Flag the sector as having its light level altered. That way, when clients
			// connect, we can tell them about the updated light level.
			m_Sector->bLightChange = true;

			// [BC] If we're the server, tell clients about the light level change.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetSectorLightLevel( ULONG( m_Sector - sectors ));

			Destroy ();
			return;
		}
		else
		{
			int temp = m_Start;
			m_Start = m_End;
			m_End = temp;
			m_Tics -= m_MaxTics;
		}
	}

	m_Sector->SetLightLevel(((m_End - m_Start) * m_Tics) / m_MaxTics + m_Start);
}

// [BC]
void DGlow2::UpdateToClient( ULONG ulClient )
{
	SERVERCOMMANDS_DoSectorLightGlow2( ULONG( m_Sector - sectors ), m_Start, m_End, m_Tics, m_MaxTics, m_OneShot );
}

// [BC]
void DGlow2::SetTics( LONG lTics )
{
	m_Tics = lTics;
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

DGlow2::DGlow2 (sector_t *sector, int start, int end, int tics, bool oneshot)
	: DLighting (sector)
{
	m_Start = sector_t::ClampLight(start);
	m_End = sector_t::ClampLight(end);
	m_MaxTics = tics;
	m_Tics = -1;
	m_OneShot = oneshot;

	// [BC] If we're the server, tell clients to create the glow light.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DoSectorLightGlow2( ULONG( sector - sectors ), m_Start, m_End, m_Tics, m_MaxTics, m_OneShot );
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void EV_StartLightGlowing (int tag, int upper, int lower, int tics)
{
	int secnum;

	// If tics is non-positive, then we can't really do anything.
	if (tics <= 0)
	{
		return;
	}

	if (upper < lower)
	{
		int temp = upper;
		upper = lower;
		lower = temp;
	}

	secnum = -1;
	while ((secnum = P_FindSectorFromTag (tag,secnum)) >= 0)
	{
		sector_t *sec = &sectors[secnum];
		if (sec->lightingdata)
			continue;
		
		new DGlow2 (sec, upper, lower, tics, false);
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void EV_StartLightFading (int tag, int value, int tics)
{
	int secnum;

	secnum = -1;
	while ((secnum = P_FindSectorFromTag (tag,secnum)) >= 0)
	{
		sector_t *sec = &sectors[secnum];
		if (sec->lightingdata)
			continue;

		if (tics <= 0)
		{
			// [CK] Only update if the values are not the same, since this is
			// an instant function with zero tics.
			if ( sec->GetLightLevel() != value )
			{
				sec->SetLightLevel(value);
				sec->bLightChange = true;

				// [CK] Since we know it's instant, this is a simple light level
				// change and not a fade.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SetSectorLightLevel( secnum );
			}
		}
		else
		{
			// No need to fade if lightlevel is already at desired value.
			if (sec->lightlevel == value)
				continue;

			new DGlow2 (sec, sec->lightlevel, value, tics, true);
		}
	}
}


//-----------------------------------------------------------------------------
//
// [RH] Phased lighting ala Hexen, but implemented without the help of the Hexen source
// The effect is a little different, but close enough, I feel.
//
//-----------------------------------------------------------------------------

IMPLEMENT_CLASS (DPhased)

DPhased::DPhased ()
{
}

void DPhased::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_BaseLevel << m_Phase;
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void DPhased::Tick ()
{
	const int steps = 12;

	if (m_Phase < steps)
		m_Sector->SetLightLevel( ((255 - m_BaseLevel) * m_Phase) / steps + m_BaseLevel);
	else if (m_Phase < 2*steps)
		m_Sector->SetLightLevel( ((255 - m_BaseLevel) * (2*steps - m_Phase - 1) / steps
								+ m_BaseLevel));
	else
		m_Sector->SetLightLevel(m_BaseLevel);

	if (m_Phase == 0)
		m_Phase = 63;
	else
		m_Phase--;
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

int DPhased::PhaseHelper (sector_t *sector, int index, int light, sector_t *prev)
{
	if (!sector)
	{
		return index;
	}
	else
	{
		DPhased *l;
		int baselevel = sector->lightlevel ? sector->lightlevel : light;

		if (index == 0)
		{
			l = this;
			m_BaseLevel = baselevel;
		}
		else
			l = new DPhased (sector, baselevel);

		int numsteps = PhaseHelper (sector->NextSpecialSector (
				(sector->special & 0x00ff) == LightSequenceSpecial1 ?
					LightSequenceSpecial2 : LightSequenceSpecial1, prev),
				index + 1, l->m_BaseLevel, sector);
		l->m_Phase = ((numsteps - index - 1) * 64) / numsteps;

		sector->special &= 0xff00;

		// [BC] If we're the server, tell clients to create the phased light.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_DoSectorLightPhased( ULONG( sector - sectors ), m_BaseLevel, m_Phase );

		return numsteps;
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

// [BC]
void DPhased::UpdateToClient( ULONG ulClient )
{
	SERVERCOMMANDS_DoSectorLightPhased( ULONG( m_Sector - sectors ), m_BaseLevel, m_Phase, ulClient, SVCF_ONLYTHISCLIENT );
}

DPhased::DPhased (sector_t *sector, int baselevel)
	: DLighting (sector)
{
	m_BaseLevel = baselevel;
}

DPhased::DPhased (sector_t *sector)
	: DLighting (sector)
{
	PhaseHelper (sector, 0, 0, NULL);
}

DPhased::DPhased (sector_t *sector, int baselevel, int phase)
	: DLighting (sector)
{
	m_BaseLevel = baselevel;
	m_Phase = phase;
	sector->special &= 0xff00;

	// [BC] If we're the server, tell clients to create the phased light.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DoSectorLightPhased( ULONG( sector - sectors ), m_BaseLevel, m_Phase );
}

//============================================================================
//
// EV_StopLightEffect
//
// Stops a lighting effect that is currently running in a sector.
//
//============================================================================

void EV_StopLightEffect (int tag)
{
	TThinkerIterator<DLighting> iterator;
	DLighting *effect;

	while ((effect = iterator.Next()) != NULL)
	{
		if (effect->GetSector()->tag == tag)
		{
			effect->Destroy();

			// [BC] Since this sector's light level most likely changed, mark it as such so
			// that we can tell clients when they come in.
			effect->GetSector( )->bLightChange = true;

			// [BC] If we're the server, tell clients to stop this light effect.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
				SERVERCOMMANDS_StopSectorLightEffect( ULONG( effect->GetSector( ) - sectors ));
				SERVERCOMMANDS_SetSectorLightLevel( ULONG( effect->GetSector( ) - sectors ));
			}
		}
	}
}
