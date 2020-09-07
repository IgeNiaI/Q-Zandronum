//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2003 Brad Carney
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
// Date created:  12/2/03
//
//
// Filename: duel.cpp
//
// Description: Contains duel routines
//
//-----------------------------------------------------------------------------

#include "announcer.h"
#include "c_cvars.h"
#include "cl_demo.h"
#include "cl_main.h"
#include "deathmatch.h"
#include "doomstat.h"
#include "duel.h"
#include "g_game.h"
#include "g_level.h"
#include "gamemode.h"
#include "gstrings.h"
#include "network.h"
#include "p_effect.h"
#include "sbar.h"
#include "scoreboard.h"
#include "sv_commands.h"
#include "team.h"
#include "v_video.h"

//*****************************************************************************
//	MISC CRAP THAT SHOULDN'T BE HERE BUT HAS TO BE BECAUSE OF SLOPPY CODING

void	SERVERCONSOLE_UpdateScoreboard( );

EXTERN_CVAR( Int,  cl_respawninvuleffect )

//*****************************************************************************
//	VARIABLES

static	ULONG		g_ulDuelCountdownTicks = 0;
static	ULONG		g_ulDuelLoser = 0;
static	ULONG		g_ulNumDuels = 0;
static	bool		g_bStartNextDuelOnLevelLoad = false;
static	DUELSTATE_e	g_DuelState;

//*****************************************************************************
//	FUNCTIONS

void DUEL_Construct( void )
{
	g_DuelState = DS_WAITINGFORPLAYERS;
}

//*****************************************************************************
//
void DUEL_Tick( void )
{
	// Not in duel mode.
	if ( duel == false )
		return;

	switch ( g_DuelState )
	{
	case DS_WAITINGFORPLAYERS:

		if ( NETWORK_InClientMode() )
		{
			break;
		}

		// Two players are here now, begin the countdown!
		if ( DUEL_CountActiveDuelers( ) == 2 )
		{
			// [BB] Skip countdown and map reset if the map is supposed to be a lobby.
			if ( GAMEMODE_IsLobbyMap( ) )
				DUEL_SetState( DS_INDUEL );
			else if ( sv_duelcountdowntime > 0 )
				DUEL_StartCountdown(( sv_duelcountdowntime * TICRATE ) - 1 );
			else
				DUEL_StartCountdown(( 10 * TICRATE ) - 1 );
		}
		break;
	case DS_COUNTDOWN:

		if ( g_ulDuelCountdownTicks )
		{
			g_ulDuelCountdownTicks--;

			// FIGHT!
			if (( g_ulDuelCountdownTicks == 0 ) &&
				( NETWORK_InClientMode() == false ))
			{
				DUEL_DoFight( );
			}
			// Play "3... 2... 1..." sounds.
			else if ( g_ulDuelCountdownTicks == ( 3 * TICRATE ))
				ANNOUNCER_PlayEntry( cl_announcer, "Three" );
			else if ( g_ulDuelCountdownTicks == ( 2 * TICRATE ))
				ANNOUNCER_PlayEntry( cl_announcer, "Two" );
			else if ( g_ulDuelCountdownTicks == ( 1 * TICRATE ))
				ANNOUNCER_PlayEntry( cl_announcer, "One" );
		}
		break;
	default: //Satisfy GCC
		break;
	}
}

//*****************************************************************************
//
ULONG DUEL_CountActiveDuelers( void )
{
	ULONG	ulIdx;
	ULONG	ulDuelers;

	ulDuelers = 0;
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( playeringame[ulIdx] ) && ( players[ulIdx].bSpectating == false ))
			ulDuelers++;
	}

	return ( ulDuelers );
}

//*****************************************************************************
//
void DUEL_StartCountdown( ULONG ulTicks )
{
	ULONG	ulIdx;

	if ( NETWORK_InClientMode() == false )
	{
		// First, reset everyone's fragcount.
		PLAYER_ResetAllPlayersFragcount( );

		// If we're the server, tell clients to reset everyone's fragcount.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_ResetAllPlayersFragcount( );

		// Also, tell bots that a duel countdown is starting.
		for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			if ( playeringame[ulIdx] )
			{
				if ( players[ulIdx].pSkullBot )
					players[ulIdx].pSkullBot->PostEvent( BOTEVENT_DUEL_STARTINGCOUNTDOWN );
			}
		}

		// Put the duel in a countdown state.
		DUEL_SetState( DS_COUNTDOWN );
	}

	// Set the duel countdown ticks.
	DUEL_SetCountdownTicks( ulTicks );

	// Announce that the fight will soon start.
	ANNOUNCER_PlayEntry( cl_announcer, "PrepareToFight" );

	// Reset announcer "frags left" variables.
	ANNOUNCER_AllowNumFragsAndPointsLeftSounds( );

	// Reset the first frag awarded flag.
	MEDAL_ResetFirstFragAwarded( );

	// Tell clients to start the countdown.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DoGameModeCountdown( ulTicks );
}

//*****************************************************************************
//
void DUEL_DoFight( void )
{
	DHUDMessageFadeOut	*pMsg;

	// No longer waiting to duel.
	if ( NETWORK_InClientMode() == false )
	{
		DUEL_SetState( DS_INDUEL );
	}

	// Make sure this is 0. Can be non-zero in network games if they're slightly out of sync.
	g_ulDuelCountdownTicks = 0;

	// Since the level time is being reset, also reset the last frag/excellent time for
	// each player.
	PLAYER_ResetAllPlayersSpecialCounters();

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

	// Reset the map.
	GAME_ResetMap( );
	GAMEMODE_RespawnAllPlayers( BOTEVENT_DUEL_FIGHT );

	SCOREBOARD_RefreshHUD( );
}

//*****************************************************************************
//
void DUEL_DoWinSequence( ULONG ulPlayer )
{
	ULONG	ulIdx;

	// Put the duel state in the win sequence state.
	if ( NETWORK_InClientMode() == false )
	{
		DUEL_SetState( DS_WINSEQUENCE );
	}

	// Tell clients to do the win sequence.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DoGameModeWinSequence( ulPlayer );

	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		char				szString[64];
		DHUDMessageFadeOut	*pMsg;

		sprintf( szString, "%s \\c-WINS!", players[ulPlayer].userinfo.GetName() );
		V_ColorizeString( szString );

		// Display "%s WINS!" HUD message.
		pMsg = new DHUDMessageFadeOut( BigFont, szString,
			160.4f,
			75.0f,
			320,
			200,
			CR_RED,
			3.0f,
			2.0f );

		StatusBar->AttachMessage( pMsg, MAKE_ID('C','N','T','R') );
	}

	// Award a victory or perfect medal to the winner.
	if ( NETWORK_InClientMode() == false )
	{
		LONG	lMedal;

		// If the duel loser doesn't have any frags, give the winner a "Perfect!".
		if ( players[g_ulDuelLoser].fragcount <= 0 )
			lMedal = MEDAL_PERFECT;
		else
			lMedal = MEDAL_VICTORY;

		// Give the player the medal.
		MEDAL_GiveMedal( ulPlayer, lMedal );
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_GivePlayerMedal( ulPlayer, lMedal );
	}

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( playeringame[ulIdx] ) && ( players[ulIdx].pSkullBot ))
			players[ulIdx].pSkullBot->PostEvent( BOTEVENT_DUEL_WINSEQUENCE );
	}
}

//*****************************************************************************
//
void DUEL_SendLoserToSpectators( void )
{
	// Losing dueler must have left.
	if ( playeringame[g_ulDuelLoser] == false )
		return;

	// Make this player a spectator.
	PLAYER_SetSpectator( &players[g_ulDuelLoser], true, false );

	// Tell the other players to mark this player as a spectator.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_PlayerIsSpectator( g_ulDuelLoser );
}

//*****************************************************************************
//
bool DUEL_IsDueler( ULONG ulPlayer )
{
	if ( ulPlayer >= MAXPLAYERS )
		return ( false );

	return (( playeringame[ulPlayer] ) && ( players[ulPlayer].bSpectating == false ));
}

//*****************************************************************************
//
void DUEL_TimeExpired( void )
{
	LONG				lDueler1 = -1;
	LONG				lDueler2 = -1;
	LONG				lWinner = -1;
	LONG				lLoser = -1;
	ULONG				ulIdx;
	DHUDMessageFadeOut	*pMsg;
	char				szString[64];

	// Don't end the level if we're not in a duel.
	if ( DUEL_GetState( ) != DS_INDUEL )
		return;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( DUEL_IsDueler( ulIdx ))
		{
			if ( lDueler1 == -1 )
				lDueler1 = ulIdx;
			else if ( lDueler2 == -1 )
				lDueler2 = ulIdx;
		}
	}

	// If for some reason we don't have two duelers, just end the map like normal.
	if (( lDueler1 == -1 ) || ( lDueler2 == -1 ))
	{
		NETWORK_Printf( "%s\n", GStrings( "TXT_TIMELIMIT" ));
		GAME_SetEndLevelDelay( 5 * TICRATE );
		return;
	}

	// If the timelimit is reached, and both duelers have the same fragcount, 
	// sudden death is reached!
	if (( players[lDueler1].fragcount ) == ( players[lDueler2].fragcount ))
	{
		// Only print the message the instant we reach sudden death.
		if ( level.time == (int)( timelimit * TICRATE * 60 ))
		{
			sprintf( szString, "\\cdSUDDEN DEATH!" );
			V_ColorizeString( szString );

			if ( NETWORK_GetState( ) != NETSTATE_SERVER )
			{
				// Display the HUD message.
				pMsg = new DHUDMessageFadeOut( BigFont, szString,
					160.4f,
					75.0f,
					320,
					200,
					CR_RED,
					3.0f,
					2.0f );

				StatusBar->AttachMessage( pMsg, MAKE_ID('C','N','T','R') );
			}
			else
			{
				SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 160.4f, 75.0f, 320, 200, CR_RED, 3.0f, 2.0f, "BigFont", false, MAKE_ID('C','N','T','R') );
			}
		}

		return;
	}

	if (( players[lDueler1].fragcount ) > ( players[lDueler2].fragcount ))
	{
		lWinner = lDueler1;
		lLoser = lDueler2;
	}
	else
	{
		lWinner = lDueler2;
		lLoser = lDueler1;
	}

	// Also, do the win sequence for the player.
	DUEL_SetLoser( lLoser );
	DUEL_DoWinSequence( lWinner );

	// Give the winner a win.
	PLAYER_SetWins( &players[lWinner], players[lWinner].ulWins + 1 );
	NETWORK_Printf( "%s\n", GStrings( "TXT_TIMELIMIT" ));
	GAME_SetEndLevelDelay( 5 * TICRATE );
}

//*****************************************************************************
//*****************************************************************************
//
ULONG DUEL_GetCountdownTicks( void )
{
	return ( g_ulDuelCountdownTicks );
}

//*****************************************************************************
//
void DUEL_SetCountdownTicks( ULONG ulTicks )
{
	g_ulDuelCountdownTicks = ulTicks;
}

//*****************************************************************************
//
void DUEL_SetLoser( ULONG ulPlayer )
{
	g_ulDuelLoser = ulPlayer;
}

//*****************************************************************************
//
ULONG DUEL_GetLoser( void )
{
	return ( g_ulDuelLoser );
}

//*****************************************************************************
//
DUELSTATE_e DUEL_GetState( void )
{
	return ( g_DuelState );
}

//*****************************************************************************
//
void DUEL_SetState( DUELSTATE_e State )
{
	if ( g_DuelState == State )
		return;

	g_DuelState = State;

	// Tell clients about the state change.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetGameModeState( State, g_ulDuelCountdownTicks );

	switch ( State )
	{
	case DS_WINSEQUENCE:

		// If we've gotten to a win sequence, we've completed a duel.
		if ( NETWORK_InClientMode() == false )
		{
			DUEL_SetNumDuels( g_ulNumDuels + 1 );
		}
		break;
	case DS_WAITINGFORPLAYERS:

		// Zero out the countdown ticker.
		DUEL_SetCountdownTicks( 0 );
		break;
	default: //Satisfy GCC
		break;
	}
}

//*****************************************************************************
//
ULONG DUEL_GetNumDuels( void )
{
	return ( g_ulNumDuels );
}

//*****************************************************************************
//
void DUEL_SetNumDuels( ULONG ulNumDuels )
{
	g_ulNumDuels = ulNumDuels;

	// If we're the server, tell the clients that the number of duels has changed.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetDuelNumDuels( );
}

//*****************************************************************************
//
bool DUEL_GetStartNextDuelOnLevelLoad( void )
{
	return ( g_bStartNextDuelOnLevelLoad );
}

//*****************************************************************************
//
void DUEL_SetStartNextDuelOnLevelLoad( bool bStart )
{
	g_bStartNextDuelOnLevelLoad = bStart;
}

//*****************************************************************************
//	CONSOLE COMMANDS/VARIABLES

CVAR( Int, sv_duelcountdowntime, 10, CVAR_ARCHIVE );
CUSTOM_CVAR( Int, duellimit, 0, CVAR_CAMPAIGNLOCK )
{
	if ( self >= 256 )
		self = 255;
	if ( self < 0 )
		self = 0;

	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( gamestate != GS_STARTUP ))
	{
		SERVER_Printf( "%s changed to: %d\n", self.GetName( ), (int)self );
		SERVERCOMMANDS_SetGameModeLimits( );

		// Update the scoreboard.
		SERVERCONSOLE_UpdateScoreboard( );
	}
}
