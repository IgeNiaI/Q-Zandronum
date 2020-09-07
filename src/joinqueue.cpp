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
// Date created:  3/25/04
//
//
// Filename: joinqueue.cpp
//
// Description: Contains join queue routines
//
//-----------------------------------------------------------------------------

#include "announcer.h"
#include "c_dispatch.h"
#include "c_cvars.h"
#include "cl_demo.h"
#include "cooperative.h"
#include "deathmatch.h"
#include "doomstat.h"
#include "duel.h"
#include "g_game.h"
#include "g_level.h"
#include "gamemode.h"
#include "invasion.h"
#include "lastmanstanding.h"
#include "joinqueue.h"
#include "network.h"
#include "possession.h"
#include "survival.h"
#include "sv_commands.h"
#include "team.h"

//*****************************************************************************
//	VARIABLES

static TArray<JoinSlot> g_JoinQueue;
int g_ConsolePlayerPosition = -1;

//*****************************************************************************
//	FUNCTIONS

void JOINQUEUE_Construct( void )
{
	// Initialize the join queue.
	JOINQUEUE_ClearList( );
}

//*****************************************************************************
//
static void JOINQUEUE_QueueChanged()
{
	// [TP] Display a message when the consoleplayer's position in line changes. However, if we're removed from it, it
	// will either be obvious that we were removed from it (i.e. we joined the game), or the server will tell us why.
	if ( NETWORK_GetState() != NETSTATE_SERVER )
	{
		int position = JOINQUEUE_GetPositionInLine( consoleplayer );

		if (( position != g_ConsolePlayerPosition ) && ( position != -1 ))
			Printf( "Your position in line is: %d\n", position + 1 );

		g_ConsolePlayerPosition = position;
	}
}

//*****************************************************************************
//
void JOINQUEUE_RemovePlayerAtPosition ( unsigned int position )
{
	if ( position < g_JoinQueue.Size() )
	{
		g_JoinQueue.Delete( position );

		if ( NETWORK_GetState() == NETSTATE_SERVER )
			SERVERCOMMANDS_RemoveFromJoinQueue( position );

		JOINQUEUE_QueueChanged();
	}
}

//*****************************************************************************
//
void JOINQUEUE_RemovePlayerFromQueue ( unsigned int player )
{
	// [BB] Invalid player.
	if ( player >= MAXPLAYERS )
		return;

	int position = JOINQUEUE_GetPositionInLine ( player );

	if ( position != -1 )
		JOINQUEUE_RemovePlayerAtPosition ( position );
}

//*****************************************************************************
//
static void JOINQUEUE_CheckSanity()
{
	// [TP] This routine should never have to clean up anything but it's here to preserve data sanity in case.
	for ( unsigned int i = 0; i < g_JoinQueue.Size(); )
	{
		const JoinSlot& slot = g_JoinQueue[i];
		if ( slot.player >= MAXPLAYERS || playeringame[slot.player] == false )
		{
			Printf( "Invalid join queue entry detected at position %d. Player idx: %d\n", i, slot.player );
			g_JoinQueue.Delete( i );
		}
		else
			++i;
	}
}

//*****************************************************************************
//
void JOINQUEUE_PlayerLeftGame( int player, bool pop )
{
	if ( player >= MAXPLAYERS )
		return;

	JOINQUEUE_RemovePlayerFromQueue( player );

	// If we're in a duel, revert to the "waiting for players" state.
	// [BB] But only do so if there are less then two duelers left (probably JOINQUEUE_PlayerLeftGame was called mistakenly).
	if ( duel && ( DUEL_CountActiveDuelers( ) < 2 ) )
		DUEL_SetState( DS_WAITINGFORPLAYERS );

	// If only one (or zero) person is left, go back to "waiting for players".
	if ( lastmanstanding )
	{
		// Someone just won by default!
		if (( GAME_CountLivingAndRespawnablePlayers( ) == 1 ) && ( LASTMANSTANDING_GetState( ) == LMSS_INPROGRESS ))
		{
			LONG	lWinner;

			lWinner = LASTMANSTANDING_GetLastManStanding( );
			if ( lWinner != -1 )
			{
				NETWORK_Printf( "%s \\c-wins!\n", players[lWinner].userinfo.GetName() );

				if (( NETWORK_GetState( ) != NETSTATE_SERVER ) && ( lWinner == consoleplayer ))
					ANNOUNCER_PlayEntry( cl_announcer, "YouWin" );

				// Give the winner a win.
				PLAYER_SetWins( &players[lWinner], players[lWinner].ulWins + 1 );

				// Pause for five seconds for the win sequence.
				LASTMANSTANDING_DoWinSequence( lWinner );
			}

			// Join queue will be popped upon state change.
			pop = false;

			GAME_SetEndLevelDelay( 5 * TICRATE );
		}
		else if ( SERVER_CalcNumNonSpectatingPlayers( MAXPLAYERS ) <= 1 )
			LASTMANSTANDING_SetState( LMSS_WAITINGFORPLAYERS );
	}

	if ( teamlms )
	{
		// Someone just won by default!
		if (( LASTMANSTANDING_GetState( ) == LMSS_INPROGRESS ) && LASTMANSTANDING_TeamsWithAlivePlayersOn( ) <= 1)
		{
			LONG	lWinner;

			lWinner = LASTMANSTANDING_TeamGetLastManStanding( );
			if ( lWinner != -1 )
			{
				NETWORK_Printf( "%s \\c-wins!\n", TEAM_GetName( lWinner ));

				if ( NETWORK_GetState() != NETSTATE_SERVER
					&& players[consoleplayer].bOnTeam
					&& lWinner == (LONG)players[consoleplayer].ulTeam )
				{
					ANNOUNCER_PlayEntry( cl_announcer, "YouWin" );
				}

				// Give the team a win.
				TEAM_SetWinCount( lWinner, TEAM_GetWinCount( lWinner ) + 1, false );

				// Pause for five seconds for the win sequence.
				LASTMANSTANDING_DoWinSequence( lWinner );
			}

			// Join queue will be popped upon state change.
			pop = false;

			GAME_SetEndLevelDelay( 5 * TICRATE );
		}
		else if ( TEAM_TeamsWithPlayersOn( ) <= 1 )
			LASTMANSTANDING_SetState( LMSS_WAITINGFORPLAYERS );
	}

	// If we're in possession mode, revert to the "waiting for players" state
	// [BB] when there are less than two players now.
	if ( possession && ( SERVER_CalcNumNonSpectatingPlayers( MAXPLAYERS ) < 2 ))
		POSSESSION_SetState( PSNS_WAITINGFORPLAYERS );

	if ( teampossession && ( TEAM_TeamsWithPlayersOn( ) <= 1 ) )
		POSSESSION_SetState( PSNS_WAITINGFORPLAYERS );

	// If we're in invasion mode, revert to the "waiting for players" state.
	if ( invasion && ( SERVER_CalcNumNonSpectatingPlayers( MAXPLAYERS ) < 1 ))
		INVASION_SetState( IS_WAITINGFORPLAYERS );

	// If we're in survival co-op mode ...
	if ( survival && (SERVER_CalcNumNonSpectatingPlayers( MAXPLAYERS ) < 1) )
	{
		if ( JOINQUEUE_GetSize( ) > 0 )
		{
			// ... if there are players waiting in queue allow them to continue
			// the mission.
			if ( !NETWORK_InClientMode( ) )
			{
				// Reset the map or not in accordance to the "no reset on death" dmflag.
				SURVIVAL_FailMission( );
			}
		}
		else
		{
			// ... if there are no more players left, revert to the "waiting for players" state.
			SURVIVAL_SetState( SURVS_WAITINGFORPLAYERS );
		}
	}

	// Potentially let one person join the game.
	if ( pop )
		JOINQUEUE_PopQueue( 1 );
}

//*****************************************************************************
//
void JOINQUEUE_PopQueue( int slotCount )
{
	// [BB] Players are not allowed to join.
	// [TP] The client does not decide when players join.
	if ( GAMEMODE_PreventPlayersFromJoining() || NETWORK_InClientMode() )
		return;

	JOINQUEUE_CheckSanity();

	// Try to find the next person in line.
	for ( unsigned int i = 0; i < g_JoinQueue.Size(); )
	{
		if ( slotCount == 0 )
			break;

		// [BB] Since we possibly just let somebody waiting in line join, check if more persons are allowed to join now.
		if ( GAMEMODE_PreventPlayersFromJoining() )
			break;

		const JoinSlot& slot = g_JoinQueue[i];

		// Found a player waiting in line. They will now join the game!
		if ( playeringame[slot.player] )
		{
			// [K6] Reset their AFK timer now - they may have been waiting in the queue silently and we don't want to kick them.
			player_t* player = &players[slot.player];
			CLIENT_s* client = SERVER_GetClient( slot.player );
			client->lLastActionTic = gametic;
			PLAYER_SpectatorJoinsGame ( player );

			// [BB/Spleen] The "lag interval" is only half of the "spectate info send" interval. Account for this here.
			if (( gametic - client->ulLastCommandTic ) <= 2*TICRATE )
				client->ulClientGameTic += ( gametic - client->ulLastCommandTic );

			if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
			{
				if ( TEAM_CheckIfValid ( slot.team ) )
					PLAYER_SetTeam( player, slot.team, true );
				else
					PLAYER_SetTeam( player, TEAM_ChooseBestTeamForPlayer( ), true );
			}

			// Begin the duel countdown.
			if ( duel )
			{
				// [BB] Skip countdown and map reset if the map is supposed to be a lobby.
				if ( GAMEMODE_IsLobbyMap( ) )
					DUEL_SetState( DS_INDUEL );
				else if ( sv_duelcountdowntime > 0 )
					DUEL_StartCountdown(( sv_duelcountdowntime * TICRATE ) - 1 );
				else
					DUEL_StartCountdown(( 10 * TICRATE ) - 1 );
			}
			// Begin the LMS countdown.
			else if ( lastmanstanding )
			{
				if ( sv_lmscountdowntime > 0 )
					LASTMANSTANDING_StartCountdown(( sv_lmscountdowntime * TICRATE ) - 1 );
				else
					LASTMANSTANDING_StartCountdown(( 10 * TICRATE ) - 1 );
			}
			else
			{
				NETWORK_Printf( "%s \\c-joined the game.\n", player->userinfo.GetName() );
			}

			JOINQUEUE_RemovePlayerAtPosition ( i );

			if ( slotCount > 0 )
				slotCount--;
		}
		else
			++i;
	}
}

//*****************************************************************************
//
unsigned int JOINQUEUE_AddPlayer( unsigned int player, unsigned int team )
{
	int idx = JOINQUEUE_GetPositionInLine( player );

	if ( idx >= 0 )
	{
		// Player is already in the queue!
		return idx;
	}

	JoinSlot slot;
	slot.player = player;
	slot.team = team;
	unsigned int result = g_JoinQueue.Push( slot );

	// [TP] Tell clients of the join queue's most recent addition
	if ( NETWORK_GetState() == NETSTATE_SERVER )
		SERVERCOMMANDS_PushToJoinQueue();

	JOINQUEUE_QueueChanged();
	return result;
}

//*****************************************************************************
//
void JOINQUEUE_ClearList( void )
{
	// [TP] Reset the client's memory of its position in the join queue because the queue is only really cleared in
	// situations where it's obvious the client's out of the queue anyway and printing a message would be redundant.
	g_ConsolePlayerPosition = -1;
	g_JoinQueue.Clear();
	JOINQUEUE_QueueChanged();
}

//*****************************************************************************
//
int JOINQUEUE_GetPositionInLine( unsigned int player )
{
	for ( unsigned int i = 0; i < g_JoinQueue.Size(); i++ )
	{
		if ( g_JoinQueue[i].player == player )
			return ( i );
	}

	return ( -1 );
}

//*****************************************************************************
//
void JOINQUEUE_AddConsolePlayer( unsigned int desiredTeam )
{
	JOINQUEUE_AddPlayer( consoleplayer, desiredTeam );
}

//*****************************************************************************
//
const JoinSlot& JOINQUEUE_GetSlotAt( unsigned int index )
{
	if ( index < g_JoinQueue.Size() )
	{
		return g_JoinQueue[index];
	}
	else
	{
		Printf( "Attempted to access the join queue at invalid index %u\n", index );
		static JoinSlot defvalue;
		defvalue.player = MAXPLAYERS;
		defvalue.team = 0;
		return defvalue;
	}
}

//*****************************************************************************
//
unsigned int JOINQUEUE_GetSize()
{
	return g_JoinQueue.Size();
}

//*****************************************************************************
//
void JOINQUEUE_PrintQueue( void )
{
	JOINQUEUE_CheckSanity();

	if ( g_JoinQueue.Size() == 0 )
	{
		Printf ( "The join queue is empty\n" );
	}
	else
	{
		for ( unsigned int ulIdx = 0; ulIdx < g_JoinQueue.Size(); ulIdx++ )
		{
			const JoinSlot& slot = g_JoinQueue[ulIdx];
			Printf ( "%02d - %s", ulIdx + 1, players[slot.player].userinfo.GetName() );
			if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
				Printf ( " - %s", TEAM_CheckIfValid ( slot.team ) ? TEAM_GetName ( slot.team ) : "auto team selection" );
			Printf ( "\n" );
		}
	}
}

//*****************************************************************************
//
CCMD( printjoinqueue )
{
	JOINQUEUE_PrintQueue();
}
