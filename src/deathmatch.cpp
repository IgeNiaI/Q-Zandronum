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

//*****************************************************************************
//	MISC CRAP THAT SHOULDN'T BE HERE BUT HAS TO BE BECAUSE OF SLOPPY CODING

void	SERVERCONSOLE_UpdateScoreboard( );

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
