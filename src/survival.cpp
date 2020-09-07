//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2004 Brad Carney
// Copyright (C) 2007-2012 Skulltag Development Team
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// 3. Neither the name of the Skulltag Development Team nor the names of its
//    contributors may be used to endorse or promote products derived from this
//    software without specific prior written permission.
// 4. Redistributions in any form must be accompanied by information on how to
//    obtain complete source code for the software and any accompanying
//    software that uses the software. The source code must either be included
//    in the distribution or be available for no more than the cost of
//    distribution plus a nominal fee, and must be freely redistributable
//    under reasonable conditions. For an executable file, complete source
//    code means the source code for all modules it contains. It does not
//    include source code for modules or files that typically accompany the
//    major components of the operating system on which the executable file
//    runs.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Date created:  1/14/06
//
//
// Filename: survival.cpp
//
// Description: Contains survival co-op routines
//
//-----------------------------------------------------------------------------

#include "announcer.h"
#include "cl_demo.h"
#include "cl_main.h"
#include "cooperative.h"
#include "doomstat.h"
#include "g_game.h"
#include "g_level.h"
#include "gamemode.h"
#include "i_system.h"
#include "joinqueue.h"
#include "network.h"
#include "p_acs.h"
#include "p_local.h"
#include "p_setup.h"
#include "sbar.h"
#include "scoreboard.h"
#include "survival.h"
#include "sv_commands.h"
#include "v_video.h"

CUSTOM_CVAR( Int, sv_maxlives, 0, CVAR_SERVERINFO | CVAR_LATCH )
{
	if ( self >= 256 )
		self = 255;
	if ( self < 0 )
		self = 0;

	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( gamestate != GS_STARTUP ))
	{
		SERVER_Printf( "%s changed to: %d\n", self.GetName( ), (int)self );
		SERVERCOMMANDS_SetGameModeLimits( );
	}
}

//*****************************************************************************
//	VARIABLES

static	ULONG			g_ulSurvivalCountdownTicks = 0;
static	SURVIVALSTATE_e	g_SurvivalState;
static	bool			g_SurvivalResetMap = true;

//*****************************************************************************
//	FUNCTIONS

void SURVIVAL_Construct( void )
{
	g_SurvivalState = SURVS_WAITINGFORPLAYERS;
}

//*****************************************************************************
//
void SURVIVAL_Tick( void )
{
	// Not in survival mode.
	if ( survival == false )
		return;

	switch ( g_SurvivalState )
	{
	case SURVS_WAITINGFORPLAYERS:

		if ( NETWORK_InClientMode() )
		{
			break;
		}

		// Someone is here! Begin the countdown.
		if ( GAME_CountActivePlayers( ) > 0 )
		{
			if ( sv_survivalcountdowntime > 0 )
				SURVIVAL_StartCountdown(( sv_survivalcountdowntime * TICRATE ) - 1 );
			else
				SURVIVAL_StartCountdown(( 20 * TICRATE ) - 1 );
		}
		break;
	case SURVS_COUNTDOWN:

		if ( g_ulSurvivalCountdownTicks )
		{
			g_ulSurvivalCountdownTicks--;

			// FIGHT!
			if (( g_ulSurvivalCountdownTicks == 0 ) &&
				( NETWORK_InClientMode() == false ))
			{
				SURVIVAL_DoFight( );
			}
			// Play "3... 2... 1..." sounds.
			else if ( g_ulSurvivalCountdownTicks == ( 3 * TICRATE ))
				ANNOUNCER_PlayEntry( cl_announcer, "Three" );
			else if ( g_ulSurvivalCountdownTicks == ( 2 * TICRATE ))
				ANNOUNCER_PlayEntry( cl_announcer, "Two" );
			else if ( g_ulSurvivalCountdownTicks == ( 1 * TICRATE ))
				ANNOUNCER_PlayEntry( cl_announcer, "One" );
		}
		break;
	case SURVS_INPROGRESS:

		if ( NETWORK_InClientMode() )
		{
			break;
		}

		// If everyone is dead, the mission has failed!
		if ( GAME_CountLivingAndRespawnablePlayers( ) == 0 )
		{
			SURVIVAL_FailMission( );
		}
	default:
		break;
	}
}

//*****************************************************************************
//
void SURVIVAL_StartCountdown( ULONG ulTicks )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( playeringame[ulIdx] ) && ( players[ulIdx].pSkullBot ))
			players[ulIdx].pSkullBot->PostEvent( BOTEVENT_LMS_STARTINGCOUNTDOWN );
	}
/*
	// First, reset everyone's fragcount. This must be done before setting the state to LMSS_COUNTDOWN
	// otherwise PLAYER_SetFragcount will ignore our request.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( playeringame[ulIdx] )
			PLAYER_SetFragcount( &players[ulIdx], 0, false, false );
	}
*/
	// Put the game in a countdown state.
	if ( NETWORK_InClientMode() == false )
	{
		SURVIVAL_SetState( SURVS_COUNTDOWN );
	}

	// Set the survival countdown ticks.
	SURVIVAL_SetCountdownTicks( ulTicks );

	// Announce that the fight will soon start.
	ANNOUNCER_PlayEntry( cl_announcer, "PrepareToFight" );

	// Tell clients to start the countdown.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DoGameModeCountdown( ulTicks );
}

//*****************************************************************************
//
void SURVIVAL_DoFight( void )
{
	DHUDMessageFadeOut	*pMsg;

	// The battle is now in progress.
	if ( NETWORK_InClientMode() == false )
	{
		SURVIVAL_SetState( SURVS_INPROGRESS );
	}

	// Make sure this is 0. Can be non-zero in network games if they're slightly out of sync.
	g_ulSurvivalCountdownTicks = 0;

	// Reset everyone's kill count.
	GAMEMODE_ResetPlayersKillCount ( false );

	// Tell clients to "fight!".
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DoGameModeFight( 0 );

	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		// Play fight sound.
		ANNOUNCER_PlayEntry( cl_announcer, "Fight" );

		// [EP] Clear all the HUD messages.
		StatusBar->DetachAllMessages();

		// Display "FIGHT!" HUD message.
		pMsg = new DHUDMessageFadeOut( BigFont, "FIGHT!",
			160.4f,
			75.0f,
			320,
			200,
			CR_RED,
			2.0f,
			1.0f );

		StatusBar->AttachMessage( pMsg, MAKE_ID('C','N','T','R') );
	}
	// Display a little thing in the server window so servers can know when matches begin.
	else
		Printf( "FIGHT!\n" );

	// Revert the map to how it was in its original state.
	GAME_ResetMap( );
	GAMEMODE_RespawnAllPlayers( );

	// [BB] To properly handle respawning of the consoleplayer in single player
	// we need to put the game into a "fake multiplayer" mode.
	if ( (NETWORK_GetState( ) == NETSTATE_SINGLE) && sv_maxlives > 0 )
		NETWORK_SetState( NETSTATE_SINGLE_MULTIPLAYER );

	SCOREBOARD_RefreshHUD( );
}

//*****************************************************************************
//
void SURVIVAL_FailMission( void )
{
	// Put the game state in the mission failed state.
	SURVIVAL_SetState( SURVS_MISSIONFAILED );

	// Pause for five seconds for the failed sequence.
	GAME_SetEndLevelDelay( 5 * TICRATE );
}

//*****************************************************************************
//
static void SURVIVAL_RefreshLives( void )
{
	// We want to allow players who were not in game
	// but are in queue to join the game. We also want players who died
	// to keep their inventory in accordance to the "keep inventory"
	// dmflags. Game state must be altered to drive conditions
	// necessary for this to work properly.
	// It's just another gross hack.

	// First let all players who died to respawn and keep their inventory.
	// This will not pop awaiting players from the join queue.
	GAMEMODE_RespawnDeadSpectators( zadmflags & ZADF_DEAD_PLAYERS_CAN_KEEP_INVENTORY ? PST_REBORN : PST_REBORNNOINVENTORY );

	// Next let all players in queue to join the game.
	// JOINQUEUE_PopQueue will not join players unless the game
	// state is appropriate.
	g_SurvivalState = SURVS_WAITINGFORPLAYERS;
	JOINQUEUE_PopQueue( -1 );

	if ( GAMEMODE_GetCurrentFlags() & GMF_USEMAXLIVES )
	{
		// Make sure that all lives are restored.
		for ( int playerIdx = 0; playerIdx < MAXPLAYERS; ++playerIdx )
		{
			if ( playeringame[playerIdx] )
				PLAYER_SetLivesLeft( &players[playerIdx], GAMEMODE_GetMaxLives( ) - 1 );
		}
	}

	// Now set the game state to the proper one.
	SURVIVAL_SetState( SURVS_INPROGRESS );

	// Share keys so that resurrected dead spectators can get them.
	SERVER_SyncSharedKeys( MAXPLAYERS, false );
}

void SURVIVAL_RestartMission( void )
{
	if ( g_SurvivalResetMap )
	{
		SURVIVAL_SetState( SURVS_WAITINGFORPLAYERS );
	}
	else
	{
		SURVIVAL_RefreshLives( );
	}
}
//*****************************************************************************
//*****************************************************************************
//
ULONG SURVIVAL_GetCountdownTicks( void )
{
	return ( g_ulSurvivalCountdownTicks );
}

//*****************************************************************************
//
void SURVIVAL_SetCountdownTicks( ULONG ulTicks )
{
	g_ulSurvivalCountdownTicks = ulTicks;
}

//*****************************************************************************
//
SURVIVALSTATE_e SURVIVAL_GetState( void )
{
	return ( g_SurvivalState );
}

//*****************************************************************************
//
void SERVERCONSOLE_UpdatePlayerInfo( LONG lPlayer, ULONG ulUpdateFlags );
void SURVIVAL_SetState( SURVIVALSTATE_e State )
{
	ULONG	ulIdx;

	g_SurvivalState = State;

	switch ( State )
	{
	case SURVS_WAITINGFORPLAYERS:

		// Zero out the countdown ticker.
		SURVIVAL_SetCountdownTicks( 0 );

		if ( survival )
		{
			GAMEMODE_RespawnDeadSpectatorsAndPopQueue( );
		}
		break;
	case SURVS_MISSIONFAILED:
		g_SurvivalResetMap = !(zadmflags & ZADF_SURVIVAL_NO_MAP_RESET_ON_DEATH); // yay for double-negation

		if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		{
			DHUDMessageFadeOut	*pMsg;

			const char *text = "";
			if ( g_SurvivalResetMap )
				text = "MISSION FAILED!";
			else
				text = "ALL PLAYERS DIED.\nRESPAWNING.";
			pMsg = new DHUDMessageFadeOut( BigFont, text,
				160.4f,
				75.0f,
				320,
				200,
				CR_RED,
				3.0f,
				2.0f );

			StatusBar->AttachMessage( pMsg, MAKE_ID('C','N','T','R') );
		}
		break;
	default:
		break;
	}

	// Since some players might have respawned, update the server console window.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			if ( playeringame[ulIdx] )
				SERVERCONSOLE_UpdatePlayerInfo( ulIdx, UDF_FRAGS );
		}
	}

	// Tell clients about the state change.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetGameModeState( State, g_ulSurvivalCountdownTicks );
}

//*****************************************************************************
//	CONSOLE COMMANDS/VARIABLES

CVAR( Int, sv_survivalcountdowntime, 10, CVAR_ARCHIVE );
