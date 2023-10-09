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
//
//
// Filename: deathmatch.cpp
//
// Description: Contains deathmatch routines
//
//-----------------------------------------------------------------------------

#include "announcer.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "cl_commands.h"
#include "cl_demo.h"
#include "cl_main.h"
#include "cooperative.h"
#include "deathmatch.h"
#include "doomstat.h"
#include "g_game.h"
#include "gamemode.h"
#include "network.h"
#include "p_effect.h"
#include "r_state.h"
#include "s_sound.h"
#include "sbar.h"
#include "sv_commands.h"
#include "team.h"
#include "v_video.h"
#include "g_level.h"
#include "scoreboard.h"

//*****************************************************************************
//	MISC CRAP THAT SHOULDN'T BE HERE BUT HAS TO BE BECAUSE OF SLOPPY CODING

void	SERVERCONSOLE_UpdateScoreboard( );

EXTERN_CVAR( Int, sv_endleveldelay )

//*****************************************************************************
//	VARIABLES

static	ULONG		g_ulDeathmatchCountdownTicks = 0;
static	DEATHMATCHSTATE_e	g_DeathmatchState;

//*****************************************************************************
//	FUNCTIONS

void DEATHMATCH_Construct( void )
{
	g_DeathmatchState = DEATHMATCHS_WAITINGFORPLAYERS;
}

//*****************************************************************************
//
void DEATHMATCH_Tick( void )
{
	// Not in any special mode
	if ( !deathmatch || duel || lastmanstanding || teamlms || possession || teampossession 
		|| survival || invasion || domination )
		return;
	
	switch ( g_DeathmatchState )
	{
	case DEATHMATCHS_WAITINGFORPLAYERS:

		if ( NETWORK_InClientMode() )
		{
			break;
		}

		if ( teamplay )
		{
			// Two players are here now, begin the countdown or warmup state
			if ( TEAM_TeamsWithPlayersOn( ) > 1 && GAMEMODE_IsNewMapStartMatchDelayOver() )
			{
				// Warmup only in non lobby maps
				if ( sv_deathmatchwarmup && !GAMEMODE_IsLobbyMap( ) && BOTSPAWN_AllBotsSpawned( ) )
				{			
					DEATHMATCH_SetState( DEATHMATCHS_WARMUP );
					break;
				}

				if ( BOTSPAWN_AllBotsSpawned() )
				{
					// [BB] Skip countdown and map reset if the map is supposed to be a lobby.
					if ( sv_deathmatchcountdowntime > 0 )
						DEATHMATCH_StartCountdown(( sv_deathmatchcountdowntime * TICRATE ) - 1 );
					else
						DEATHMATCH_StartCountdown(( 10 * TICRATE ) - 1 );
				}
			}
		}
		else
		{
			// Two players are here now, begin the countdown or warmup state
			if ( GAME_CountActivePlayers( ) >= 2 && GAMEMODE_IsNewMapStartMatchDelayOver() )
			{
				// Warmup only in non lobby maps
				if ( sv_deathmatchwarmup && !GAMEMODE_IsLobbyMap( ) && BOTSPAWN_AllBotsSpawned( ) )
				{			
					DEATHMATCH_SetState( DEATHMATCHS_WARMUP );
					break;
				}
				
				if ( BOTSPAWN_AllBotsSpawned() )
				{
					if ( sv_deathmatchcountdowntime > 0 )
						DEATHMATCH_StartCountdown(( sv_deathmatchcountdowntime * TICRATE ) - 1 );
					else
						DEATHMATCH_StartCountdown(( 10 * TICRATE ) - 1 );
				}
			}
		}
		break;
	case DEATHMATCHS_WARMUP:
		if ( NETWORK_InClientMode() )
		{
			break;
		}
		
		if ( teamplay )
		{
			// Two players are here now, begin the countdown or warmup state
			if ( TEAM_TeamsWithPlayersOn( ) <= 1 )
			{
				DEATHMATCH_SetState( DEATHMATCHS_WAITINGFORPLAYERS );
				break;
			}
		}
		else
		{
			// Two players are here now, begin the countdown or warmup state
			if ( GAME_CountActivePlayers( ) < 2 )
			{
				DEATHMATCH_SetState( DEATHMATCHS_WAITINGFORPLAYERS );
				break;
			}
		}

		break;
	case DEATHMATCHS_COUNTDOWN:

		if ( g_ulDeathmatchCountdownTicks )
		{
			g_ulDeathmatchCountdownTicks--;

			// FIGHT!
			if (( g_ulDeathmatchCountdownTicks == 0 ) &&
				( NETWORK_InClientMode() == false ))
			{
				DEATHMATCH_DoFight( );
			}
			// Play "3... 2... 1..." sounds.
			else if ( g_ulDeathmatchCountdownTicks == ( 3 * TICRATE ))
				ANNOUNCER_PlayEntry( cl_announcer, "Three" );
			else if ( g_ulDeathmatchCountdownTicks == ( 2 * TICRATE ))
				ANNOUNCER_PlayEntry( cl_announcer, "Two" );
			else if ( g_ulDeathmatchCountdownTicks == ( 1 * TICRATE ))
				ANNOUNCER_PlayEntry( cl_announcer, "One" );
		}
		break;
	default: //Satisfy GCC
		break;
	}
}

//*****************************************************************************
//
LONG DEATHMATCH_GetLastManStanding( void )
{
	ULONG	ulIdx;

	// Not in lastmanstanding mode.
	if ( lastmanstanding == false )
		return ( -1 );

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( playeringame[ulIdx] && ( players[ulIdx].bSpectating == false ) && PLAYER_IsAliveOrCanRespawn ( &players[ulIdx] ) )
			return ( ulIdx );
	}

	return ( -1 );
}

//*****************************************************************************
//
void DEATHMATCH_StartCountdown( ULONG ulTicks )
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
					players[ulIdx].pSkullBot->PostEvent( BOTEVENT_DEATHMATCH_STARTINGCOUNTDOWN );
			}
		}

		// Put the game in a countdown state.
		DEATHMATCH_SetState( DEATHMATCHS_COUNTDOWN );
	}

	// Set the game countdown ticks.
	DEATHMATCH_SetCountdownTicks( ulTicks );

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
void DEATHMATCH_DoFight( void )
{
	DHUDMessageFadeOut	*pMsg;

	// No longer waiting to play.
	if ( NETWORK_InClientMode() == false )
	{
		DEATHMATCH_SetState( DEATHMATCHS_INPROGRESS );
	}

	// Make sure this is 0. Can be non-zero in network games if they're slightly out of sync.
	g_ulDeathmatchCountdownTicks = 0;

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
	GAMEMODE_RespawnAllPlayers( BOTEVENT_DEATHMATCH_FIGHT );

	SCOREBOARD_RefreshHUD( );
}

//*****************************************************************************
//
void DEATHMATCH_DoWinSequence( player_t *player )
{
	DEATHMATCH_DetermineAndPrintWinner();

	GAME_SetEndLevelDelay( sv_endleveldelay * TICRATE );
}

//*****************************************************************************
//
void DEATHMATCH_TimeExpired( void )
{
	NETWORK_Printf( "%s\n", GStrings( "TXT_TIMELIMIT" ));

	DEATHMATCH_DetermineAndPrintWinner();

	GAME_SetEndLevelDelay( sv_endleveldelay * TICRATE );
}

//*****************************************************************************
//
void DEATHMATCH_DetermineAndPrintWinner()
{
	ULONG				ulIdx;
	ULONG				ulWinner;
	LONG				lHighestFrags;
	bool				bTied;
	char				szString[64];
	DHUDMessageFadeOut	*pMsg;

	// Determine the winner.
	ulWinner = MAXPLAYERS;
	lHighestFrags = INT_MIN;
	bTied = false;
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		// [BB] Spectators can't win.
		if ( ( playeringame[ulIdx] == false ) || players[ulIdx].bSpectating )
			continue;

		if ( players[ulIdx].fragcount > lHighestFrags )
		{
			ulWinner = ulIdx;
			lHighestFrags = players[ulIdx].fragcount;
			bTied = false;
		}
		else if ( players[ulIdx].fragcount == lHighestFrags )
			bTied = true;
	}

	// [BB] In case there are no active players (only spectators), lWinner is -1.
	if ( bTied || ulWinner >= MAXPLAYERS || ulWinner < 0 )
		sprintf( szString, "\\cdDRAW GAME!" );
	else
		sprintf( szString, "%s \\c-WINS!", players[ulWinner].userinfo.GetName() );
	V_ColorizeString( szString );
	
	// Put the deathmatch state in the win sequence state.
	if ( NETWORK_InClientMode() == false )
	{
		DEATHMATCH_SetState( DEATHMATCHS_WINSEQUENCE );
	}

	// Tell clients to do the win sequence.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVERCOMMANDS_DoGameModeWinSequence( ulWinner );
	}
	else if ( playeringame[consoleplayer] )
	{
		if ( bTied || ulWinner >= MAXPLAYERS || ulWinner < 0 )
			ANNOUNCER_PlayEntry( cl_announcer, "DrawGame" );
		else if ( ulWinner == static_cast<LONG>(consoleplayer) )
			ANNOUNCER_PlayEntry( cl_announcer, "YouWin" );
		else
			ANNOUNCER_PlayEntry( cl_announcer, "YouLose" );
	}

	// Display "%s WINS!" HUD message.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		pMsg = new DHUDMessageFadeOut( BigFont, szString,
			160.4f,
			75.0f,
			320,
			200,
			CR_WHITE,
			3.0f,
			2.0f );

		StatusBar->AttachMessage( pMsg, MAKE_ID('C','N','T','R') );

		szString[0] = 0;
		pMsg = new DHUDMessageFadeOut( SmallFont, szString,
			0.0f,
			0.0f,
			0,
			0,
			CR_RED,
			3.0f,
			2.0f );

		StatusBar->AttachMessage( pMsg, MAKE_ID('F','R','A','G') );

		pMsg = new DHUDMessageFadeOut( SmallFont, szString,
			0.0f,
			0.0f,
			0,
			0,
			CR_RED,
			3.0f,
			2.0f );

		StatusBar->AttachMessage( pMsg, MAKE_ID('P','L','A','C') );
	}
	else
	{
		SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 160.4f, 75.0f, 320, 200, CR_WHITE, 3.0f, 2.0f, "BigFont", false, MAKE_ID('C','N','T','R') );
		szString[0] = 0;
		SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 0.0f, 0.0f, 0, 0, CR_WHITE, 3.0f, 2.0f, "BigFont", false, MAKE_ID('F','R','A','G') );
		SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 0.0f, 0.0f, 0, 0, CR_WHITE, 3.0f, 2.0f, "BigFont", false, MAKE_ID('P','L','A','C') );
	}

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( playeringame[ulIdx] ) && ( players[ulIdx].pSkullBot ))
			players[ulIdx].pSkullBot->PostEvent( BOTEVENT_DEATHMATCH_WINSEQUENCE );
	}
}

//*****************************************************************************
//*****************************************************************************
//
ULONG DEATHMATCH_GetCountdownTicks( void )
{
	return ( g_ulDeathmatchCountdownTicks );
}

//*****************************************************************************
//
void DEATHMATCH_SetCountdownTicks( ULONG ulTicks )
{
	g_ulDeathmatchCountdownTicks = ulTicks;
}

//*****************************************************************************
//
DEATHMATCHSTATE_e DEATHMATCH_GetState( void )
{
	return ( g_DeathmatchState );
}

//*****************************************************************************
//
void DEATHMATCH_SetState( DEATHMATCHSTATE_e State )
{
	g_DeathmatchState = State;

	// Tell clients about the state change.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetGameModeState( State, g_ulDeathmatchCountdownTicks );

	switch ( State )
	{
	case DEATHMATCHS_WAITINGFORPLAYERS:

		// Zero out the countdown ticker.
		DEATHMATCH_SetCountdownTicks( 0 );

		break;
	default:
		break;
	}
}

//*****************************************************************************
//	CONSOLE COMMANDS/VARIABLES

CUSTOM_CVAR( Bool, deathmatch, false, CVAR_SERVERINFO | CVAR_LATCH | CVAR_CAMPAIGNLOCK )
{
	UCVarValue	Val;

	if ( self == true )
	{
		Val.Bool = false;

		// Disable teamgame if we're playing deathmatch.
		teamgame.ForceSet( Val, CVAR_Bool );

		// Disable cooperative if we're playing deathmatch.
		cooperative.ForceSet( Val, CVAR_Bool );
	}
	else
	{
		Val.Bool = false;
		// Deathmatch has been disabled, so disable all related modes.
		teamplay.ForceSet( Val, CVAR_Bool );
		duel.ForceSet( Val, CVAR_Bool );
		terminator.ForceSet( Val, CVAR_Bool );
		lastmanstanding.ForceSet( Val, CVAR_Bool );
		teamlms.ForceSet( Val, CVAR_Bool );
		possession.ForceSet( Val, CVAR_Bool );
		teampossession.ForceSet( Val, CVAR_Bool );

		// If teamgame is also disabled, enable cooperative mode.
		if ( teamgame == false )
		{
			Val.Bool = true;

			if ( cooperative != true )
				cooperative.ForceSet( Val, CVAR_Bool );
		}
	}

	// Reset what the current game mode is.
	GAMEMODE_DetermineGameMode( );
}

//*****************************************************************************
//
CUSTOM_CVAR( Bool, teamplay, false, CVAR_SERVERINFO | CVAR_LATCH | CVAR_CAMPAIGNLOCK )
{
	UCVarValue	Val;

	if ( self == true )
	{
		Val.Bool = true;
		// Enable deathmatch.
		deathmatch.ForceSet( Val, CVAR_Bool );

		Val.Bool = false;
		// Disable other modes.
		duel.ForceSet( Val, CVAR_Bool );
		terminator.ForceSet( Val, CVAR_Bool );
		lastmanstanding.ForceSet( Val, CVAR_Bool );
		teamlms.ForceSet( Val, CVAR_Bool );
		possession.ForceSet( Val, CVAR_Bool );
		teampossession.ForceSet( Val, CVAR_Bool );
	}

	// Reset what the current game mode is.
	GAMEMODE_DetermineGameMode( );
}

//*****************************************************************************
//
CUSTOM_CVAR( Bool, duel, false, CVAR_SERVERINFO | CVAR_LATCH | CVAR_CAMPAIGNLOCK )
{
	UCVarValue	Val;

	if ( self == true )
	{
		Val.Bool = true;
		// Enable deathmatch.
		deathmatch.ForceSet( Val, CVAR_Bool );

		Val.Bool = false;
		// Disable other modes.
		teamplay.ForceSet( Val, CVAR_Bool );
		terminator.ForceSet( Val, CVAR_Bool );
		lastmanstanding.ForceSet( Val, CVAR_Bool );
		teamlms.ForceSet( Val, CVAR_Bool );
		possession.ForceSet( Val, CVAR_Bool );
		teampossession.ForceSet( Val, CVAR_Bool );
	}

	// Reset what the current game mode is.
	GAMEMODE_DetermineGameMode( );
}

//*****************************************************************************
//
CUSTOM_CVAR( Bool, terminator, false, CVAR_SERVERINFO | CVAR_LATCH | CVAR_CAMPAIGNLOCK )
{
	UCVarValue	Val;

	if ( self == true )
	{
		Val.Bool = true;
		// Enable deathmatch.
		deathmatch.ForceSet( Val, CVAR_Bool );

		Val.Bool = false;
		// Disable other modes.
		teamplay.ForceSet( Val, CVAR_Bool );
		duel.ForceSet( Val, CVAR_Bool );
		lastmanstanding.ForceSet( Val, CVAR_Bool );
		teamlms.ForceSet( Val, CVAR_Bool );
		possession.ForceSet( Val, CVAR_Bool );
		teampossession.ForceSet( Val, CVAR_Bool );
	}

	// Reset what the current game mode is.
	GAMEMODE_DetermineGameMode( );
}

//*****************************************************************************
//
CUSTOM_CVAR( Bool, lastmanstanding, false, CVAR_SERVERINFO | CVAR_LATCH | CVAR_CAMPAIGNLOCK )
{
	UCVarValue	Val;

	if ( self == true )
	{
		Val.Bool = true;
		// Enable deathmatch.
		deathmatch.ForceSet( Val, CVAR_Bool );

		Val.Bool = false;
		// Disable other modes.
		teamplay.ForceSet( Val, CVAR_Bool );
		duel.ForceSet( Val, CVAR_Bool );
		terminator.ForceSet( Val, CVAR_Bool );
		teamlms.ForceSet( Val, CVAR_Bool );
		possession.ForceSet( Val, CVAR_Bool );
		teampossession.ForceSet( Val, CVAR_Bool );
	}

	// Reset what the current game mode is.
	GAMEMODE_DetermineGameMode( );
}

//*****************************************************************************
//
CUSTOM_CVAR( Bool, teamlms, false, CVAR_SERVERINFO | CVAR_LATCH | CVAR_CAMPAIGNLOCK )
{
	UCVarValue	Val;

	if ( self == true )
	{
		Val.Bool = true;
		// Enable deathmatch.
		deathmatch.ForceSet( Val, CVAR_Bool );

		Val.Bool = false;
		// Disable other modes.
		teamplay.ForceSet( Val, CVAR_Bool );
		duel.ForceSet( Val, CVAR_Bool );
		terminator.ForceSet( Val, CVAR_Bool );
		lastmanstanding.ForceSet( Val, CVAR_Bool );
		possession.ForceSet( Val, CVAR_Bool );
		teampossession.ForceSet( Val, CVAR_Bool );
	}

	// Reset what the current game mode is.
	GAMEMODE_DetermineGameMode( );
}

//*****************************************************************************
//
CUSTOM_CVAR( Bool, possession, false, CVAR_SERVERINFO | CVAR_LATCH | CVAR_CAMPAIGNLOCK )
{
	UCVarValue	Val;

	if ( self == true )
	{
		Val.Bool = true;
		// Enable deathmatch.
		deathmatch.ForceSet( Val, CVAR_Bool );

		Val.Bool = false;
		// Disable other modes.
		teamplay.ForceSet( Val, CVAR_Bool );
		duel.ForceSet( Val, CVAR_Bool );
		terminator.ForceSet( Val, CVAR_Bool );
		lastmanstanding.ForceSet( Val, CVAR_Bool );
		teamlms.ForceSet( Val, CVAR_Bool );
		teampossession.ForceSet( Val, CVAR_Bool );
	}

	// Reset what the current game mode is.
	GAMEMODE_DetermineGameMode( );
}

//*****************************************************************************
//
CUSTOM_CVAR( Bool, teampossession, false, CVAR_SERVERINFO | CVAR_LATCH | CVAR_CAMPAIGNLOCK )
{
	UCVarValue	Val;

	if ( self == true )
	{
		Val.Bool = true;
		// Enable deathmatch.
		deathmatch.ForceSet( Val, CVAR_Bool );

		Val.Bool = false;
		// Disable other modes.
		teamplay.ForceSet( Val, CVAR_Bool );
		duel.ForceSet( Val, CVAR_Bool );
		terminator.ForceSet( Val, CVAR_Bool );
		lastmanstanding.ForceSet( Val, CVAR_Bool );
		teamlms.ForceSet( Val, CVAR_Bool );
		possession.ForceSet( Val, CVAR_Bool );
	}

	// Reset what the current game mode is.
	GAMEMODE_DetermineGameMode( );
}

//*****************************************************************************
//
CCMD( spectate )
{
	// [BB] When playing a demo enter free spectate mode.
	if ( CLIENTDEMO_IsPlaying( ) == true )
	{
		C_DoCommand( "demo_spectatefreely" );
		return;
	}

	// [BB] The server can't use this.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		Printf ( "CCMD spectate can't be used on the server\n" );
		return;
	}

	// If we're a client, inform the server that we wish to spectate.
	// [BB] This also serves as way to leave the join queue.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		CLIENTCOMMANDS_Spectate( );
		return;
	}

	// Already a spectator!
	if ( PLAYER_IsTrueSpectator( &players[consoleplayer] ))
		return;

	// Make the player a spectator.
	PLAYER_SetSpectator( &players[consoleplayer], true, false );
}

//*****************************************************************************
//*****************************************************************************
//
CUSTOM_CVAR( Int, fraglimit, 0, CVAR_SERVERINFO | CVAR_CAMPAIGNLOCK )
{
/* [BB] Do we need this in ST?
	// Check for the fraglimit being hit because the fraglimit is being
	// lowered below somebody's current frag count.
	if (deathmatch && self > 0)
	{
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i] && self <= D_GetFragCount(&players[i]))
			{
				Printf ("%s\n", GStrings("TXT_FRAGLIMIT"));
				G_ExitLevel (0, false);
				break;
			}
		}
	}
*/
	if ( self > SHRT_MAX ) {
		self = SHRT_MAX;
		return;
	}
	else if ( self < 0 ) {
		self = 0;
		return;
	}

	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( gamestate != GS_STARTUP ))
	{
		SERVER_Printf( "%s changed to: %d\n", self.GetName( ), (int)self );
		SERVERCOMMANDS_SetGameModeLimits( );

		// Update the scoreboard.
		SERVERCONSOLE_UpdateScoreboard( );
	}
}

//*****************************************************************************
//
CUSTOM_CVAR( Float, timelimit, 0.0f, CVAR_SERVERINFO | CVAR_CAMPAIGNLOCK )
{
	// [BB] SHRT_MAX is a pretty arbitrary limit considering that timelimit is a float,
	// Nevertheless, we should put some limit here. SHRT_MAX allows for a limit of
	// a bit more than three weeks, that's more than enough.
	if ( self > SHRT_MAX ) {
		self = SHRT_MAX;
		return;
	}
	else if ( self < 0 ) {
		self = 0;
		return;
	}

	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( gamestate != GS_STARTUP ))
	{
		SERVER_Printf( "%s changed to: %.2f\n", self.GetName( ), static_cast<float>(self) );
		SERVERCOMMANDS_SetGameModeLimits( );

		// Update the scoreboard.
		SERVERCONSOLE_UpdateScoreboard( );
	}
}

// [AM] Set or unset a map as being a "lobby" map.
CUSTOM_CVAR(String, lobby, "", CVAR_SERVERINFO)
{
	if (strcmp(*self, "") == 0)
	{
		// Lobby map is empty.  Tell the client that if necessary.
		if ((NETWORK_GetState() == NETSTATE_SERVER) && (gamestate != GS_STARTUP))
		{
			SERVER_Printf(PRINT_HIGH, "%s unset\n", self.GetName());
			SERVERCOMMANDS_SetGameModeLimits();
		}
	}
	else
	{
		// Prevent setting a lobby map that doesn't exist.
		level_info_t *map = FindLevelByName(*self);
		if (map == NULL)
		{
			Printf("map %s doesn't exist.\n", *self);
			self = "";
			return;
		}

		// Update the client about the lobby map if necessary.
		if ((NETWORK_GetState() == NETSTATE_SERVER) && (gamestate != GS_STARTUP))
		{
			SERVER_Printf(PRINT_HIGH, "%s changed to: %s\n", self.GetName(), *self);
			SERVERCOMMANDS_SetGameModeLimits();
		}
	}
}

// Is this a good place for these variables?
CVAR( Bool, cl_noammoswitch, true, CVAR_ARCHIVE );
CVAR( Bool, cl_useoriginalweaponorder, false, CVAR_ARCHIVE );

// Allow the display of large frag messages.
CVAR( Bool, cl_showlargefragmessages, true, CVAR_ARCHIVE );

CVAR( Int, sv_deathmatchcountdowntime, 10, CVAR_ARCHIVE | CVAR_GAMEMODESETTING );
CVAR( Bool, sv_deathmatchwarmup, false, CVAR_ARCHIVE | CVAR_GAMEMODESETTING );
/*
CCMD( showweaponstates )
{
	ULONG			ulIdx;
	const PClass	*pClass;

	if ( players[consoleplayer].ReadyWeapon == NULL )
		return;

	pClass = players[consoleplayer].ReadyWeapon->GetClass( );

	// Begin searching through the actor's state labels to find the state that corresponds
	// to the given state.
	for ( ulIdx = 0; ulIdx < (ULONG)pClass->ActorInfo->StateList->NumLabels; ulIdx++ )
		Printf( pClass->ActorInfo->StateList->Labels[ulIdx].Label.GetChars( ));
}
*/
