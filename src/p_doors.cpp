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
// DESCRIPTION: Door animation code (opening/closing)
//		[RH] Removed sliding door code and simplified for Hexen-ish specials
//
//-----------------------------------------------------------------------------



#include "doomdef.h"
#include "p_local.h"
#include "s_sound.h"
#include "s_sndseq.h"
#include "doomstat.h"
#include "r_state.h"
#include "c_console.h"
#include "gi.h"
#include "a_keys.h"
#include "i_system.h"
#include "sc_man.h"
#include "cmdlib.h"
#include "farchive.h"
// [BB] New #includes.
#include "cl_demo.h"
#include "network.h"
#include "sv_commands.h"
#include "cl_main.h"

//============================================================================
//
// VERTICAL DOORS
//
//============================================================================

IMPLEMENT_CLASS (DDoor)

inline FArchive &operator<< (FArchive &arc, DDoor::EVlDoor &type)
{
	BYTE val = (BYTE)type;
	arc << val;
	type = (DDoor::EVlDoor)val;
	return arc;
}

DDoor::DDoor ()
{
	m_LastInstigator = NULL;
}

void DDoor::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Type
		<< m_TopDist
		<< m_BotSpot << m_BotDist << m_OldFloorDist
		<< m_Speed
		<< m_Direction
		<< m_TopWait
		<< m_TopCountdown
		<< m_LightTag;
}

//============================================================================
//
// T_VerticalDoor
//
//============================================================================

void DDoor::Tick ()
{
	EResult res;

	if (m_Sector->floorplane.d != m_OldFloorDist)
	{
		if (!m_Sector->floordata || !m_Sector->floordata->IsKindOf(RUNTIME_CLASS(DPlat)) ||
			!(barrier_cast<DPlat*>(m_Sector->floordata))->IsLift())
		{
			m_OldFloorDist = m_Sector->floorplane.d;
			m_BotDist = m_Sector->ceilingplane.PointToDist (m_BotSpot,
				m_Sector->floorplane.ZatPoint (m_BotSpot));
		}
	}

	switch (m_Direction)
	{
	case 0:
		// WAITING
		if (!--m_TopCountdown)
		{
			switch (m_Type)
			{
			case doorRaise:
				m_Direction = -1; // time to go back down
				DoorSound (false);

				// [geNia] If we are the server, update the clients
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_DoDoor( this );

				break;
				
			case doorCloseWaitOpen:
				m_Direction = 1;
				DoorSound (true);

				// [geNia] If we are the server, update the clients
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_DoDoor( this );

				break;
				
			default:
				break;
			}
		}
		break;
		
	case 2:
		//	INITIAL WAIT
		if (!--m_TopCountdown)
		{
			switch (m_Type)
			{
			case doorRaiseIn5Mins:
				m_Direction = 1;
				m_Type = doorRaise;
				DoorSound (true);

				// [geNia] If we are the server, update the clients
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_DoDoor( this );

				break;
				
			default:
				break;
			}
		}
		break;
		
	case -1:
		// DOWN
		res = MoveCeiling (m_Speed, m_BotDist, -1, m_Direction, false);

		// killough 10/98: implement gradual lighting effects
		if (m_LightTag != 0 && m_TopDist != -m_Sector->floorplane.d)
		{
			EV_LightTurnOnPartway (m_LightTag, FixedDiv (m_Sector->ceilingplane.d + m_Sector->floorplane.d,
				m_TopDist + m_Sector->floorplane.d));
		}

		if (res == pastdest)
		{
			SN_StopSequence (m_Sector, CHAN_CEILING);
			switch (m_Type)
			{
			case doorRaise:
			case doorClose:
				Destroy ();						// unlink and free
				break;
				
			case doorCloseWaitOpen:
				m_Direction = 0;
				m_TopCountdown = m_TopWait;

				// [geNia] If we are the server, update the clients
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_DoDoor( this );

				break;
				
			default:
				break;
			}
		}
		else if (res == crushed)
		{
			switch (m_Type)
			{
			case doorClose:				// DO NOT GO BACK UP!
				break;
				
			default:
				m_Direction = 1;
				DoorSound (true);

				// [geNia] If we are the server, update the clients
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_DoDoor( this );

				break;
			}
		}
		break;
		
	case 1:
		// UP
		res = MoveCeiling (m_Speed, m_TopDist, -1, m_Direction, false);
		
		// killough 10/98: implement gradual lighting effects
		if (m_LightTag != 0 && m_TopDist != -m_Sector->floorplane.d)
		{
			EV_LightTurnOnPartway (m_LightTag, FixedDiv (m_Sector->ceilingplane.d + m_Sector->floorplane.d,
				m_TopDist + m_Sector->floorplane.d));
		}

		if (res == pastdest)
		{
			SN_StopSequence (m_Sector, CHAN_CEILING);
			switch (m_Type)
			{
			case doorRaise:
				m_Direction = 0; // wait at top
				m_TopCountdown = m_TopWait;

				// [geNia] If we are the server, update the clients
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_DoDoor( this );

				break;
				
			case doorCloseWaitOpen:
			case doorOpen:
				Destroy ();						// unlink and free
				break;
				
			default:
				break;
			}
		}
		else if (res == crushed)
		{
			switch (m_Type)
			{
			case doorRaise:
			case doorRaiseIn5Mins:
				m_Direction = -1;
				DoorSound(false);
				break;

			default:
				break;
			}
		}
		break;
	}
}

void DDoor::UpdateToClient( ULONG ulClient )
{
	SERVERCOMMANDS_DoDoor( this, ulClient, SVCF_ONLYTHISCLIENT );
}

fixed_t DDoor::GetPosition ()
{
	return ( m_Sector->ceilingplane.d );
}

int DDoor::GetDirection ()
{
	return ( m_Direction );
}

void DDoor::SetDirection ( int direction )
{
	if (m_Direction != direction)
	{
		if (direction == 1)
			DoorSound(true);
		else if (direction == -1)
			DoorSound(false);

		m_Direction = direction;
	}
}

LONG DDoor::GetSectorNum( void )
{
	return ( m_Sector->sectornum );
}

DDoor::EVlDoor DDoor::GetType( void )
{
	return ( m_Type );
}

void DDoor::SetType( DDoor::EVlDoor Type )
{
	m_Type = Type;
}

fixed_t DDoor::GetSpeed( void )
{
	return ( m_Speed );
}

void DDoor::SetSpeed( fixed_t Speed )
{
	m_Speed = Speed;
}

int DDoor::GetTopWait( void )
{
	return ( m_TopWait );
}

void DDoor::SetTopWait( int TopWait )
{
	m_TopWait = TopWait;
}

int DDoor::GetCountdown( void )
{
	return ( m_TopCountdown );
}

void DDoor::SetCountdown( int Countdown)
{
	m_TopCountdown = Countdown;
}

int DDoor::GetLightTag( void )
{
	return ( m_LightTag );
}

void DDoor::SetLightTag( int Tag )
{
	m_LightTag = Tag;
}

//============================================================================
//
// [RH] DoorSound: Plays door sound depending on direction and speed
//
// If curseq is non-NULL, then it will check if the desired sound sequence
// will result in a different command stream than the current one. If not,
// then it does nothing.
//
//============================================================================

void DDoor::DoorSound(bool raise, DSeqNode *curseq) const
{
	int choice;

	// For multiple-selection sound sequences, the following choices are used:
	//  0  Opening
	//  1  Closing
	//  2  Opening fast
	//  3  Closing fast

	choice = !raise;

	if (m_Speed >= FRACUNIT*8)
	{
		choice += 2;
	}

	if (m_Sector->seqType >= 0)
	{
		if (curseq == NULL || !SN_AreModesSame(m_Sector->seqType, SEQ_DOOR, choice, curseq->GetModeNum()))
		{
			SN_StartSequence(m_Sector, CHAN_CEILING, m_Sector->seqType, SEQ_DOOR, choice);
		}
	}
	else if (m_Sector->SeqName != NAME_None)
	{
		if (curseq == NULL || !SN_AreModesSame(m_Sector->SeqName, choice, curseq->GetModeNum()))
		{
			SN_StartSequence(m_Sector, CHAN_CEILING, m_Sector->SeqName, choice);
		}
	}
	else
	{
		const char *snd;

		switch (gameinfo.gametype)
		{
		default:	/* Doom and Hexen */
			snd = "DoorNormal";
			break;
			
		case GAME_Heretic:
			snd = "HereticDoor";
			break;

		case GAME_Strife:
			snd = "DoorSmallMetal";

			// Search the front top textures of 2-sided lines on the door sector
			// for a door sound to use.
			for (int i = 0; i < m_Sector->linecount; ++i)
			{
				const char *texname;
				line_t *line = m_Sector->lines[i];

				if (line->backsector == NULL)
					continue;

				FTexture *tex = TexMan[line->sidedef[0]->GetTexture(side_t::top)];
				texname = tex? tex->Name : NULL;
				if (texname != NULL && texname[0] == 'D' && texname[1] == 'O' && texname[2] == 'R')
				{
					switch (texname[3])
					{
					case 'S':
						snd = "DoorStone";
						break;

					case 'M':
						if (texname[4] == 'L')
						{
							snd = "DoorLargeMetal";
						}
						break;

					case 'W':
						if (texname[4] == 'L')
						{
							snd = "DoorLargeWood";
						}
						else
						{
							snd = "DoorSmallWood";
						}
						break;
					}
				}
			}
			break;
		}
		if (curseq == NULL || !SN_AreModesSame(snd, choice, curseq->GetModeNum()))
		{
			SN_StartSequence(m_Sector, CHAN_CEILING, snd, choice);
		}
	}
}

DDoor::DDoor (sector_t *sector)
	: DMovingCeiling (sector)
{
}

//============================================================================
//
// [RH] SpawnDoor: Helper function for EV_DoDoor
//
//============================================================================

// [BC] Added bNoSound. When creating doors when connecting to a server, we don't want a sound to be played.
DDoor::DDoor (sector_t *sec, EVlDoor type, fixed_t speed, int delay, int lightTag, bool bNoSound)
	: DMovingCeiling (sec),
  	  m_Type (type), m_Speed (speed), m_TopWait (delay), m_LightTag (lightTag)
{
	vertex_t *spot;
	fixed_t height;

	if (i_compatflags & COMPATF_NODOORLIGHT)
	{
		m_LightTag = 0;
	}

	switch (type)
	{
	case doorClose:
		m_Direction = -1;
		height = sec->FindLowestCeilingSurrounding (&spot);
		m_TopDist = sec->ceilingplane.PointToDist (spot, height - 4*FRACUNIT);
		// [BC] Added option to create the door soundlessly.
		if ( bNoSound == false )
			DoorSound (false);
		break;

	case doorOpen:
	case doorRaise:
		m_Direction = 1;
		height = sec->FindLowestCeilingSurrounding (&spot);
		m_TopDist = sec->ceilingplane.PointToDist (spot, height - 4*FRACUNIT);
		// [BC] Added option to create the door soundlessly.
		if ((m_TopDist != sec->ceilingplane.d) && ( bNoSound == false ))
			DoorSound (true);
		break;

	case doorCloseWaitOpen:
		m_TopDist = sec->ceilingplane.d;
		m_Direction = -1;
		// [BC] Added option to create the door soundlessly.
		if ( bNoSound == false )
			DoorSound (false);
		break;

	case doorRaiseIn5Mins:
		m_Direction = 2;
		height = sec->FindLowestCeilingSurrounding (&spot);
		m_TopDist = sec->ceilingplane.PointToDist (spot, height - 4*FRACUNIT);
		m_TopCountdown = 5 * 60 * TICRATE;
		break;
	}

	if (!m_Sector->floordata || !m_Sector->floordata->IsKindOf(RUNTIME_CLASS(DPlat)) ||
		!(barrier_cast<DPlat*>(m_Sector->floordata))->IsLift())
	{
		height = sec->FindHighestFloorPoint (&m_BotSpot);
		m_BotDist = sec->ceilingplane.PointToDist (m_BotSpot, height);
	}
	else
	{
		height = sec->FindLowestCeilingPoint(&m_BotSpot);
		m_BotDist = sec->ceilingplane.PointToDist (m_BotSpot, height);
	}
	m_OldFloorDist = sec->floorplane.d;
}

//============================================================================
//
// [RH] Merged EV_VerticalDoor and EV_DoLockedDoor into EV_DoDoor
//		and made them more general to support the new specials.
//
//============================================================================

bool EV_DoDoor (DDoor::EVlDoor type, line_t *line, AActor *thing,
				int tag, int speed, int delay, int lock, int lightTag, bool boomgen)
{
	return EV_DoDoor(NULL, type, line, thing, tag, speed, delay, lock, lightTag, boomgen);
}

bool EV_DoDoor(player_t *instigator, DDoor::EVlDoor type, line_t *line, AActor *thing,
	int tag, int speed, int delay, int lock, int lightTag, bool boomgen)
{
	if (CLIENT_PREDICT_IsPredicting())
		return false;

	bool		rtn = false;
	int 		secnum;
	sector_t*	sec;
	DDoor		*pDoor;

	if (lock != 0 && !P_CheckKeys (thing, lock, tag != 0))
		return false;

	if (tag == 0)
	{		// [RH] manual door
		if (!line)
			return false;

		// if the wrong side of door is pushed, give oof sound
		if (line->sidedef[1] == NULL)			// killough
		{
			S_Sound (thing, CHAN_VOICE, "*usefail", 1, ATTN_NORM);

			// [BC] Tell clients of the "oof" sound.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
				if ( thing && thing->player )
					SERVERCOMMANDS_SoundActor( thing, CHAN_VOICE, "*usefail", 1, ATTN_NORM, ULONG( thing->player - players ), SVCF_SKIPTHISCLIENT );
				else
					SERVERCOMMANDS_SoundActor( thing, CHAN_VOICE, "*usefail", 1, ATTN_NORM );
			}

			return false;
		}

		// get the sector on the second side of activating linedef
		sec = line->sidedef[1]->sector;
		secnum = int(sec-sectors);

		// if door already has a thinker, use it
		if (sec->PlaneMoving(sector_t::ceiling))
		{
			// Boom used remote door logic for generalized doors, even if they are manual
			if (boomgen)
				return false;
			if (sec->ceilingdata->IsKindOf (RUNTIME_CLASS(DDoor)))
			{
				DDoor *door = barrier_cast<DDoor *>(sec->ceilingdata);
				door->m_LastInstigator = instigator;

				// ONLY FOR "RAISE" DOORS, NOT "OPEN"s
				if (door->m_Type == DDoor::doorRaise && type == DDoor::doorRaise)
				{
					if (door->m_Direction == -1)
					{
						door->m_Direction = 1;	// go back up
						door->DoorSound (true);	// [RH] Make noise

						// [geNia] If we are the server, update the clients
						if ( NETWORK_GetState( ) == NETSTATE_SERVER )
							SERVERCOMMANDS_DoDoor( door );
					}
					else if (!(line->activation & (SPAC_Push|SPAC_MPush)))
						// [RH] activate push doors don't go back down when you
						//		run into them (otherwise opening them would be
						//		a real pain).
					{
						// [BC] Added !thing, because sometimes thing can be NULL when this function
						// is called by client_DoDoor.
						if (!thing || !thing->player)
							return false;	// JDC: bad guys never close doors

						door->m_Direction = -1;	// start going down immediately

						// Start the door close sequence.
						door->DoorSound(false, SN_CheckSequence(sec, CHAN_CEILING));

						// [geNia] If we are the server, update the clients
						if ( NETWORK_GetState( ) == NETSTATE_SERVER )
							SERVERCOMMANDS_DoDoor( door );

						return true;
					}
					else
					{
						return false;
					}
				}
			}
			return false;
		}
		if ( (pDoor = new DDoor (sec, type, speed, delay, lightTag)))
		{
			// Don't update for the instigator client
			pDoor->m_LastInstigator = instigator;

			// [geNia] If we are the server, update the clients
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_DoDoor( pDoor );

			rtn = true;
		}
	}
	else
	{	// [RH] Remote door

		secnum = -1;
		while ((secnum = P_FindSectorFromTag (tag,secnum)) >= 0)
		{
			sec = &sectors[secnum];
			// if the ceiling is already moving, don't start the door action
			if (sec->PlaneMoving(sector_t::ceiling))
				continue;

			if ( (pDoor = new DDoor (sec, type, speed, delay, lightTag)))
			{
				pDoor->SetLastInstigator( instigator );

				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_DoDoor( pDoor );

				rtn = true;
			}
		}

	}
	return rtn;
}

//============================================================================
//
// Spawn a door that closes after 30 seconds
//
//============================================================================

void P_SpawnDoorCloseIn30 (sector_t *sec)
{
	fixed_t height;
	DDoor *door = new DDoor (sec);

	sec->special = 0;

	door->m_Sector = sec;
	door->m_Direction = 0;
	door->m_Type = DDoor::doorRaise;
	door->m_Speed = FRACUNIT*2;
	door->m_TopCountdown = 30 * TICRATE;
	height = sec->FindHighestFloorPoint (&door->m_BotSpot);
	door->m_BotDist = sec->ceilingplane.PointToDist (door->m_BotSpot, height);
	door->m_OldFloorDist = sec->floorplane.d;
	door->m_TopDist = sec->ceilingplane.d;
	door->m_LightTag = 0;
}

//============================================================================
//
// Spawn a door that opens after 5 minutes
//
//============================================================================

void P_SpawnDoorRaiseIn5Mins (sector_t *sec)
{
	sec->special = 0;
	new DDoor (sec, DDoor::doorRaiseIn5Mins, 2*FRACUNIT, TICRATE*30/7, 0);
}

//============================================================================
DDoor *P_GetDoorBySectorNum( LONG sectorNum )
{
	DDoor	*pDoor;

	TThinkerIterator<DDoor>		Iterator;

	while (( pDoor = Iterator.Next( )))
	{
		if ( pDoor->GetSectorNum( ) == sectorNum )
			return ( pDoor );
	}

	return ( NULL );
}

//*****************************************************************************
//
//
// animated doors
//
//============================================================================

IMPLEMENT_CLASS (DAnimatedDoor)

DAnimatedDoor::DAnimatedDoor ()
{
}

DAnimatedDoor::DAnimatedDoor (sector_t *sec)
	: DMovingCeiling (sec)
{
}

void DAnimatedDoor::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	
	arc << m_Line1 << m_Line2
		<< m_Frame
		<< m_Timer
		<< m_BotDist
		<< m_Status
		<< m_Speed
		<< m_Delay
		<< m_DoorAnim
		<< m_SetBlocking1 << m_SetBlocking2;
}

//============================================================================
//
// Starts a closing action on an animated door
//
//============================================================================

bool DAnimatedDoor::StartClosing ()
{
	// CAN DOOR CLOSE?
	if (m_Sector->touching_thinglist != NULL)
	{
		return false;
	}

	fixed_t topdist = m_Sector->ceilingplane.d;
	if (MoveCeiling (2048*FRACUNIT, m_BotDist, 0, -1, false) == crushed)
	{
		return false;
	}

	MoveCeiling (2048*FRACUNIT, topdist, 1);

	m_Line1->flags |= ML_BLOCKING;
	m_Line2->flags |= ML_BLOCKING;

	// [BC] If we're the server, tell clients to alter this line's blocking status.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		if ( m_LastInstigator )
		{
			SERVERCOMMANDS_SetSomeLineFlags( ULONG( m_Line1 - lines ), ULONG(m_LastInstigator - players), SVCF_SKIPTHISCLIENT );
			SERVERCOMMANDS_SetSomeLineFlags( ULONG( m_Line2 - lines ), ULONG(m_LastInstigator - players), SVCF_SKIPTHISCLIENT );

			// Also, tell clients to move the ceiling.
			SERVERCOMMANDS_SetSectorCeilingPlane( ULONG( m_Sector - sectors ), ULONG(m_LastInstigator - players), SVCF_SKIPTHISCLIENT );
		}
		else
		{
			SERVERCOMMANDS_SetSomeLineFlags( ULONG( m_Line1 - lines ));
			SERVERCOMMANDS_SetSomeLineFlags( ULONG( m_Line2 - lines ));

			// Also, tell clients to move the ceiling.
			SERVERCOMMANDS_SetSectorCeilingPlane( ULONG( m_Sector - sectors ));
		}
	}

	if (m_DoorAnim->CloseSound != NAME_None)
	{
		SN_StartSequence (m_Sector, CHAN_CEILING, m_DoorAnim->CloseSound, 1);

		// [BB] Tell the clients to play the sound.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			if ( m_LastInstigator )
				SERVERCOMMANDS_StartSectorSequence( m_Sector, CHAN_CEILING, m_DoorAnim->CloseSound.GetChars(), 1, ULONG(m_LastInstigator - players), SVCF_SKIPTHISCLIENT );
			else
				SERVERCOMMANDS_StartSectorSequence( m_Sector, CHAN_CEILING, m_DoorAnim->CloseSound.GetChars(), 1 );
		}
	}

	m_Status = Closing;
	m_Timer = m_Speed;
	return true;
}

//============================================================================
//
//
//
//============================================================================

void DAnimatedDoor::Tick ()
{
	if (m_DoorAnim == NULL)
	{
		// can only happen when a bad savegame is loaded.
		Destroy();
		return;
	}

	switch (m_Status)
	{
	case Dead:
		m_Sector->ceilingdata = NULL;
		Destroy ();
		break;

	case Opening:
		if (!m_Timer--)
		{
			if (++m_Frame >= m_DoorAnim->NumTextureFrames)
			{
				// IF DOOR IS DONE OPENING...
				m_Line1->flags &= ~ML_BLOCKING;
				m_Line2->flags &= ~ML_BLOCKING;

				// [BC] If we're the server, tell clients to alter this line's blocking status.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					if ( m_LastInstigator )
					{
						SERVERCOMMANDS_SetSomeLineFlags( ULONG( m_Line1 - lines ), ULONG(m_LastInstigator - players), SVCF_SKIPTHISCLIENT );
						SERVERCOMMANDS_SetSomeLineFlags( ULONG( m_Line2 - lines ), ULONG(m_LastInstigator - players), SVCF_SKIPTHISCLIENT );
					}
					else
					{
						SERVERCOMMANDS_SetSomeLineFlags( ULONG( m_Line1 - lines ));
						SERVERCOMMANDS_SetSomeLineFlags( ULONG( m_Line2 - lines ));
					}
				}

				if (m_Delay == 0)
				{
					m_Sector->ceilingdata = NULL;
					Destroy ();
					break;
				}

				m_Timer = m_Delay;
				m_Status = Waiting;
			}
			else
			{
				// IF DOOR NEEDS TO ANIMATE TO NEXT FRAME...
				m_Timer = m_Speed;

				m_Line1->sidedef[0]->SetTexture(side_t::mid, m_DoorAnim->TextureFrames[m_Frame]);
				m_Line1->sidedef[1]->SetTexture(side_t::mid, m_DoorAnim->TextureFrames[m_Frame]);
				m_Line2->sidedef[0]->SetTexture(side_t::mid, m_DoorAnim->TextureFrames[m_Frame]);
				m_Line2->sidedef[1]->SetTexture(side_t::mid, m_DoorAnim->TextureFrames[m_Frame]);

				// [BC] Mark this line's textures as having been changed.
				m_Line1->ulTexChangeFlags |= TEXCHANGE_FRONTMEDIUM|TEXCHANGE_BACKMEDIUM;
				m_Line2->ulTexChangeFlags |= TEXCHANGE_FRONTMEDIUM|TEXCHANGE_BACKMEDIUM;

				// [BC] If we're the server, tell clients that these lines' texture
				// has changed.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					if ( m_LastInstigator )
					{
						SERVERCOMMANDS_SetLineTexture( ULONG( m_Line1 - lines ), ULONG(m_LastInstigator - players), SVCF_SKIPTHISCLIENT );
						SERVERCOMMANDS_SetLineTexture( ULONG( m_Line2 - lines ), ULONG(m_LastInstigator - players), SVCF_SKIPTHISCLIENT );
					}
					else
					{
						SERVERCOMMANDS_SetLineTexture( ULONG( m_Line1 - lines ));
						SERVERCOMMANDS_SetLineTexture( ULONG( m_Line2 - lines ));
					}
				}
			}
		}
		break;

	case Waiting:
		// IF DOOR IS DONE WAITING...
		if (!m_Timer--)
		{
			if (!StartClosing())
			{
				m_Timer = m_Delay;
			}
		}
		break;

	case Closing:
		if (!m_Timer--)
		{
			if (--m_Frame < 0)
			{
				// IF DOOR IS DONE CLOSING...
				MoveCeiling (2048*FRACUNIT, m_BotDist, -1);

				// [BC] If we're the server, tell clients to move the ceiling.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					if ( m_LastInstigator )
						SERVERCOMMANDS_SetSectorCeilingPlane( ULONG( m_Sector - sectors ), ULONG(m_LastInstigator - players), SVCF_SKIPTHISCLIENT );
					else
						SERVERCOMMANDS_SetSectorCeilingPlane( ULONG( m_Sector - sectors ));
				}

				m_Sector->ceilingdata = NULL;
				Destroy ();
				// Unset blocking flags on lines that didn't start with them. Since the
				// ceiling is down now, we shouldn't need this flag anymore to keep things
				// from getting through.
				if (!m_SetBlocking1)
				{
					m_Line1->flags &= ~ML_BLOCKING;
				}
				if (!m_SetBlocking2)
				{
					m_Line2->flags &= ~ML_BLOCKING;
				}
				break;
			}
			else
			{
				// IF DOOR NEEDS TO ANIMATE TO NEXT FRAME...
				m_Timer = m_Speed;

				m_Line1->sidedef[0]->SetTexture(side_t::mid, m_DoorAnim->TextureFrames[m_Frame]);
				m_Line1->sidedef[1]->SetTexture(side_t::mid, m_DoorAnim->TextureFrames[m_Frame]);
				m_Line2->sidedef[0]->SetTexture(side_t::mid, m_DoorAnim->TextureFrames[m_Frame]);
				m_Line2->sidedef[1]->SetTexture(side_t::mid, m_DoorAnim->TextureFrames[m_Frame]);

				// [BC] Mark this line's textures as having been changed.
				m_Line1->ulTexChangeFlags |= TEXCHANGE_FRONTMEDIUM|TEXCHANGE_BACKMEDIUM;
				m_Line2->ulTexChangeFlags |= TEXCHANGE_FRONTMEDIUM|TEXCHANGE_BACKMEDIUM;

				// [BC] If we're the server, tell clients that these lines' texture
				// has changed.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					if ( m_LastInstigator )
					{
						SERVERCOMMANDS_SetLineTexture( ULONG( m_Line1 - lines ), ULONG(m_LastInstigator - players), SVCF_SKIPTHISCLIENT );
						SERVERCOMMANDS_SetLineTexture( ULONG( m_Line2 - lines ), ULONG(m_LastInstigator - players), SVCF_SKIPTHISCLIENT );
					}
					else
					{
						SERVERCOMMANDS_SetLineTexture( ULONG( m_Line1 - lines ));
						SERVERCOMMANDS_SetLineTexture( ULONG( m_Line2 - lines ));
					}
				}
			}
		}
		break;
	}
}

//============================================================================
//
//
//
//============================================================================

DAnimatedDoor::DAnimatedDoor (player_t *instigator, sector_t *sec, line_t *line, int speed, int delay, FDoorAnimation *anim)
	: DMovingCeiling (sec)
{
	fixed_t topdist;
	FTextureID picnum;

	// The DMovingCeiling constructor automatically sets up an interpolation for us.
	// Stop it, since the ceiling is moving instantly here.
	StopInterpolation();
	m_DoorAnim = anim;
	m_LastInstigator = instigator;

	m_Line1 = line;
	m_Line2 = line;

	for (int i = 0; i < sec->linecount; ++i)
	{
		if (sec->lines[i] == line)
			continue;

		if (sec->lines[i]->sidedef[0]->GetTexture(side_t::top) == line->sidedef[0]->GetTexture(side_t::top))
		{
			m_Line2 = sec->lines[i];
			break;
		}
	}


	picnum = m_Line1->sidedef[0]->GetTexture(side_t::top);
	m_Line1->sidedef[0]->SetTexture(side_t::mid, picnum);
	m_Line2->sidedef[0]->SetTexture(side_t::mid, picnum);

	// [BC] Mark this line's textures as having been changed.
	m_Line1->ulTexChangeFlags |= TEXCHANGE_FRONTMEDIUM|TEXCHANGE_BACKMEDIUM;
	m_Line2->ulTexChangeFlags |= TEXCHANGE_FRONTMEDIUM|TEXCHANGE_BACKMEDIUM;

	// [BC] If we're the server, tell clients that these lines' texture
	// has changed.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		if ( m_LastInstigator )
		{
			SERVERCOMMANDS_SetLineTexture( ULONG( m_Line1 - lines ), ULONG(m_LastInstigator - players), SVCF_SKIPTHISCLIENT );
			SERVERCOMMANDS_SetLineTexture( ULONG( m_Line2 - lines ), ULONG(m_LastInstigator - players), SVCF_SKIPTHISCLIENT );
		}
		else
		{
			SERVERCOMMANDS_SetLineTexture( ULONG( m_Line1 - lines ));
			SERVERCOMMANDS_SetLineTexture( ULONG( m_Line2 - lines ));
		}
	}

	// don't forget texture scaling here!
	FTexture *tex = TexMan[picnum];
	topdist = tex ? tex->GetScaledHeight() : 64;

	topdist = m_Sector->ceilingplane.d - topdist * m_Sector->ceilingplane.c;

	m_Status = Opening;
	m_Speed = speed;
	m_Delay = delay;
	m_Timer = m_Speed;
	m_Frame = 0;
	m_SetBlocking1 = !!(m_Line1->flags & ML_BLOCKING);
	m_SetBlocking2 = !!(m_Line2->flags & ML_BLOCKING);
	m_Line1->flags |= ML_BLOCKING;
	m_Line2->flags |= ML_BLOCKING;

	// [BC] If we're the server, tell clients that these lines' blocking status
	// has changed.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		if ( m_LastInstigator )
		{
			SERVERCOMMANDS_SetSomeLineFlags( ULONG( m_Line1 - lines ), ULONG(m_LastInstigator - players), SVCF_SKIPTHISCLIENT );
			SERVERCOMMANDS_SetSomeLineFlags( ULONG( m_Line2 - lines ), ULONG(m_LastInstigator - players), SVCF_SKIPTHISCLIENT );
		}
		else
		{
			SERVERCOMMANDS_SetSomeLineFlags( ULONG( m_Line1 - lines ));
			SERVERCOMMANDS_SetSomeLineFlags( ULONG( m_Line2 - lines ));
		}
	}

	m_BotDist = m_Sector->ceilingplane.d;
	MoveCeiling (2048*FRACUNIT, topdist, 1);
	if (m_DoorAnim->OpenSound != NAME_None)
	{
		SN_StartSequence (m_Sector, CHAN_INTERIOR, m_DoorAnim->OpenSound, 1);

		// [BB] Tell the clients to play the sound.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			if ( m_LastInstigator )
				SERVERCOMMANDS_StartSectorSequence( m_Sector, CHAN_INTERIOR, m_DoorAnim->OpenSound.GetChars(), 1, ULONG(m_LastInstigator - players), SVCF_SKIPTHISCLIENT );
			else
				SERVERCOMMANDS_StartSectorSequence( m_Sector, CHAN_INTERIOR, m_DoorAnim->OpenSound.GetChars(), 1 );
		}
	}

	// [BC] If we're the server, tell clients to move the ceiling.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		if ( m_LastInstigator )
			SERVERCOMMANDS_SetSectorCeilingPlane( ULONG( m_Sector - sectors ), ULONG(m_LastInstigator - players), SVCF_SKIPTHISCLIENT );
		else
			SERVERCOMMANDS_SetSectorCeilingPlane( ULONG( m_Sector - sectors ));
	}
}

//============================================================================
//
// EV_SlidingDoor : slide a door horizontally
// (animate midtexture, then set noblocking line)
//
//============================================================================

bool EV_SlidingDoor (player_t *instigator, line_t *line, AActor *actor, int tag, int speed, int delay)
{
	sector_t *sec;
	int secnum;
	bool rtn;

	secnum = -1;
	rtn = false;

	if (tag == 0)
	{
		// Manual sliding door
		sec = line->backsector;

		// Make sure door isn't already being animated
		if (sec->ceilingdata != NULL)
		{
			if (actor->player == NULL)
				return false;

			if (sec->ceilingdata->IsA (RUNTIME_CLASS(DAnimatedDoor)))
			{
				DAnimatedDoor *door = barrier_cast<DAnimatedDoor *>(sec->ceilingdata);
				if (door->m_Status == DAnimatedDoor::Waiting)
				{
					door->m_LastInstigator = instigator;
					return door->StartClosing();
				}
			}
			return false;
		}
		FDoorAnimation *anim = TexMan.FindAnimatedDoor (line->sidedef[0]->GetTexture(side_t::top));
		if (anim != NULL)
		{
			new DAnimatedDoor (instigator, sec, line, speed, delay, anim);
			return true;
		}
		return false;
	}

	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0)
	{
		sec = &sectors[secnum];
		if (sec->ceilingdata != NULL)
		{
			continue;
		}

		for (int i = 0; tag != 0 && i < sec->linecount; ++i)
		{
			line = sec->lines[i];
			if (line->backsector == NULL)
			{
				continue;
			}
			FDoorAnimation *anim = TexMan.FindAnimatedDoor (line->sidedef[0]->GetTexture(side_t::top));
			if (anim != NULL)
			{
				rtn = true;
				new DAnimatedDoor (instigator, sec, line, speed, delay, anim);
				break;
			}
		}
	}
	return rtn;
}

