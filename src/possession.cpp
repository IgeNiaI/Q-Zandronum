//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2004-2006 Brad Carney
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
// Date created:  10/6/06
//
//
// Filename: possession.cpp
//
// Description: Contains possession routines
//
//-----------------------------------------------------------------------------

#include "announcer.h"
#include "chat.h"
#include "cl_demo.h"
#include "cl_main.h"
#include "deathmatch.h"
#include "doomstat.h"
#include "g_game.h"
#include "g_level.h"
#include "gamemode.h"
#include "gstrings.h"
#include "p_effect.h"
#include "possession.h"
#include "sv_commands.h"
#include "team.h"
#include "sbar.h"
#include "scoreboard.h"
#include "v_video.h"

//*****************************************************************************
//	PROTOTYPES

static	void			possession_DisplayScoreInfo( ULONG ulPlayer );

//*****************************************************************************
//	VARIABLES

static	player_t		*g_pPossessionArtifactCarrier = NULL;
static	ULONG			g_ulPSNCountdownTicks = 0;
static	ULONG			g_ulPSNArtifactHoldTicks = 0;
static	PSNSTATE_e		g_PSNState;

//*****************************************************************************
//	FUNCTIONS

void POSSESSION_Construct( void )
{
	g_PSNState = PSNS_WAITINGFORPLAYERS;
}

//*****************************************************************************
//
void POSSESSION_Tick( void )
{
	// Not in possession mode.
	if (( possession == false ) && ( teampossession == false ))
		return;

	switch ( g_PSNState )
	{
	case PSNS_WAITINGFORPLAYERS:

		// No need to do anything here for clients.
		if ( NETWORK_InClientMode() )
		{
			break;
		}

		if ( possession )
		{
			// Two players are here now, being the initial countdown!
			if ( SERVER_CalcNumNonSpectatingPlayers( MAXPLAYERS ) >= 2 )
			{
				if ( sv_possessioncountdowntime > 0 )
					POSSESSION_StartCountdown(( sv_possessioncountdowntime * TICRATE ) - 1 );
				else
					POSSESSION_StartCountdown(( 10 * TICRATE ) - 1 );
			}
		}

		if ( teampossession )
		{
			if ( TEAM_TeamsWithPlayersOn( ) > 1 )
			{
				if ( sv_possessioncountdowntime > 0 )
					POSSESSION_StartCountdown(( sv_possessioncountdowntime * TICRATE ) - 1 );
				else
					POSSESSION_StartCountdown(( 10 * TICRATE ) - 1 );
			}
		}
		break;
	case PSNS_COUNTDOWN:

		if ( g_ulPSNCountdownTicks )
		{
			g_ulPSNCountdownTicks--;

			// FIGHT!
			if (( g_ulPSNCountdownTicks == 0 ) &&
				( NETWORK_InClientMode() == false ))
			{
				POSSESSION_DoFight( );
			}
			// Play "3... 2... 1..." sounds.
			else if ( g_ulPSNCountdownTicks == ( 3 * TICRATE ))
				ANNOUNCER_PlayEntry( cl_announcer, "Three" );
			else if ( g_ulPSNCountdownTicks == ( 2 * TICRATE ))
				ANNOUNCER_PlayEntry( cl_announcer, "Two" );
			else if ( g_ulPSNCountdownTicks == ( 1 * TICRATE ))
				ANNOUNCER_PlayEntry( cl_announcer, "One" );
		}
		break;
	case PSNS_INPROGRESS:

		// Nothing to do while the match is in progress. Just wait until someone picks up
		// the possession artifact.
		// Although... don't we need to check to see if people have left the game so we can
		// revert back to PSNS_WAITINGFORPLAYERS?
		break;
	case PSNS_ARTIFACTHELD:

		if ( g_ulPSNArtifactHoldTicks )
		{
			g_ulPSNArtifactHoldTicks--;

			// The holder has held the artifact for the required time! Give the holder a point!
			if (( g_ulPSNArtifactHoldTicks == 0 ) &&
				( NETWORK_InClientMode() == false ))
			{
				POSSESSION_ScorePossessionPoint( g_pPossessionArtifactCarrier );
			}
			// Play "3... 2... 1..." sounds.
			else if ( g_ulPSNArtifactHoldTicks == ( 3 * TICRATE ))
				ANNOUNCER_PlayEntry( cl_announcer, "Three" );
			else if ( g_ulPSNArtifactHoldTicks == ( 2 * TICRATE ))
				ANNOUNCER_PlayEntry( cl_announcer, "Two" );
			else if ( g_ulPSNArtifactHoldTicks == ( 1 * TICRATE ))
				ANNOUNCER_PlayEntry( cl_announcer, "One" );
		}
		break;
	case PSNS_HOLDERSCORED:

		break;
	case PSNS_PRENEXTROUNDCOUNTDOWN:

		// No need to do anything here for clients.
		if ( NETWORK_InClientMode() )
		{
			break;
		}

		if ( possession )
		{
			// Two players are here now, being the initial countdown!
			if ( SERVER_CalcNumNonSpectatingPlayers( MAXPLAYERS ) >= 2 )
			{
				if ( sv_possessioncountdowntime > 0 )
					POSSESSION_StartNextRoundCountdown(( sv_possessioncountdowntime * TICRATE ) - 1 );
				else
					POSSESSION_StartNextRoundCountdown(( 10 * TICRATE ) - 1 );
			}
			else
				POSSESSION_SetState( PSNS_WAITINGFORPLAYERS );
		}

		if ( teampossession )
		{
			if ( TEAM_TeamsWithPlayersOn( ) > 1 )
			{
				if ( sv_possessioncountdowntime > 0 )
					POSSESSION_StartNextRoundCountdown(( sv_possessioncountdowntime * TICRATE ) - 1 );
				else
					POSSESSION_StartNextRoundCountdown(( 10 * TICRATE ) - 1 );
			}
			else
				POSSESSION_SetState( PSNS_WAITINGFORPLAYERS );
		}
		break;
	case PSNS_NEXTROUNDCOUNTDOWN:

		if ( g_ulPSNCountdownTicks )
		{
			g_ulPSNCountdownTicks--;

			// FIGHT!
			if (( g_ulPSNCountdownTicks == 0 ) &&
				( NETWORK_InClientMode() == false ))
			{
				POSSESSION_DoFight( );
			}
			// Play "3... 2... 1..." sounds.
			else if ( g_ulPSNCountdownTicks == ( 3 * TICRATE ))
				ANNOUNCER_PlayEntry( cl_announcer, "Three" );
			else if ( g_ulPSNCountdownTicks == ( 2 * TICRATE ))
				ANNOUNCER_PlayEntry( cl_announcer, "Two" );
			else if ( g_ulPSNCountdownTicks == ( 1 * TICRATE ))
				ANNOUNCER_PlayEntry( cl_announcer, "One" );
		}
		break;
	}
}

//*****************************************************************************
//
void POSSESSION_Render( void )
{
	char		szString[16];
	ULONG		ulColor;
	bool		bScale;
	UCVarValue	ValWidth;
	UCVarValue	ValHeight;
	float		fXScale;
	float		fYScale = 0.0f;

	// If the artifact isn't being held by anyone, just break out since we
	// don't have a timer to draw.
	if ( g_PSNState != PSNS_ARTIFACTHELD )
		return;

	// No need to do anything if the automap is active.
	if ( automapactive )
		return;

	// Initialization.
	if (( con_scaletext ) && ( con_virtualwidth > 0 ) && ( con_virtualheight > 0 ))
	{
		ValWidth = con_virtualwidth.GetGenericRep( CVAR_Int );
		ValHeight = con_virtualheight.GetGenericRep( CVAR_Int );

		fXScale =  (float)ValWidth.Int / 320.0f;
		fYScale =  (float)ValHeight.Int / 200.0f;
		bScale = true;
	}
	else
		bScale = false;

	ulColor = ( g_ulPSNArtifactHoldTicks > ( 3 * TICRATE )) ? CR_GRAY : CR_RED;
	sprintf( szString , "%02d:%02d", static_cast<unsigned int> (( g_ulPSNArtifactHoldTicks + TICRATE ) / ( TICRATE * 60 )), static_cast<unsigned int> ((( g_ulPSNArtifactHoldTicks + TICRATE ) - ((( g_ulPSNArtifactHoldTicks + TICRATE ) / ( TICRATE * 60 )) * ( TICRATE * 60 ))) / TICRATE) );
	V_ColorizeString( szString );

	// [RC] Hide this when the scoreboard is up to prevent overlapping.
	if( SCOREBOARD_ShouldDrawBoard( consoleplayer ) )
		return;

	bool bUseBigFont = false;
	// [RC] Use a bolder display, at resolutions large enough.
	if ( bScale )
	{
		if ( con_virtualwidth.GetGenericRep( CVAR_Int ).Int >= 640 )
			bUseBigFont = true;
	}
	else
	{
		if ( SCREENWIDTH >= 640 )
			bUseBigFont = true;
	}

	if ( bScale )
	{
		screen->DrawText( bUseBigFont ? BigFont : SmallFont, ulColor,
			(LONG)(( con_virtualwidth / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
			(LONG)( 25 * fYScale ),
			szString,
			DTA_VirtualWidth, ValWidth.Int,
			DTA_VirtualHeight, ValHeight.Int,
			TAG_DONE );
	}
	else
	{
		screen->DrawText( bUseBigFont ? BigFont : SmallFont, ulColor,
			( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
			25,
			szString,
			TAG_DONE );
	}
}

//*****************************************************************************
//
void POSSESSION_StartCountdown( ULONG ulTicks )
{
/*
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( playeringame[ulIdx] ) && ( players[ulIdx].pSkullBot ))
			players[ulIdx].pSkullBot->PostEvent( BOTEVENT_LMS_STARTINGCOUNTDOWN );
	}
*/
	// Put the game in a countdown state.
	if ( NETWORK_InClientMode() == false )
	{
		POSSESSION_SetState( PSNS_COUNTDOWN );
	}

	// Set the possession countdown ticks.
	POSSESSION_SetCountdownTicks( ulTicks );

	// Announce that the fight will soon start.
	// [BB] The server makes the clients call POSSESSION_StartCountdown even when
	// the next round countdown begins. So they need to select the proper announcer here.
	if ( NETWORK_InClientMode() && ( POSSESSION_GetState() == PSNS_NEXTROUNDCOUNTDOWN ) )
		ANNOUNCER_PlayEntry( cl_announcer, "NextRoundIn" );
	else
		ANNOUNCER_PlayEntry( cl_announcer, "PrepareToFight" );

	// Tell clients to start the countdown.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DoGameModeCountdown( ulTicks );
}

//*****************************************************************************
//
void POSSESSION_StartNextRoundCountdown( ULONG ulTicks )
{
/*
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( playeringame[ulIdx] ) && ( players[ulIdx].pSkullBot ))
			players[ulIdx].pSkullBot->PostEvent( BOTEVENT_LMS_STARTINGCOUNTDOWN );
	}
*/
	// Put the game in a countdown state.
	if ( NETWORK_InClientMode() == false )
	{
		POSSESSION_SetState( PSNS_NEXTROUNDCOUNTDOWN );
	}

	// Set the possession countdown ticks.
	POSSESSION_SetCountdownTicks( ulTicks );

	// Announce that the fight will soon start.
	ANNOUNCER_PlayEntry( cl_announcer, "NextRoundIn" );

	// Tell clients to start the countdown.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DoGameModeCountdown( ulTicks );
}

//*****************************************************************************
//
void POSSESSION_DoFight( void )
{
	DHUDMessageFadeOut	*pMsg;
	AActor				*pActor;

	// The match is now in progress.
	if ( NETWORK_InClientMode() == false )
	{
		POSSESSION_SetState( PSNS_INPROGRESS );
	}

	// Make sure this is 0. Can be non-zero in network games if they're slightly out of sync.
	g_ulPSNCountdownTicks = 0;
	g_pPossessionArtifactCarrier = NULL;

	// Reset level time to 0.
	level.time = 0;

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

	if ( NETWORK_InClientMode() == false )
	{
		// Reload the items on this level.
		TThinkerIterator<AActor> iterator;
		while (( pActor = iterator.Next( )) != NULL )
		{
			// Kill any stray missiles that have been fired during the countdown.
			if ( pActor->flags & MF_MISSILE )
			{
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_DestroyThing( pActor );

				pActor->Destroy( );
				continue;
			}

			// If the possession artifact is present on the map, delete it so that
			// when we spawn a new one, there aren't multiples.
			if (( pActor->GetClass( )->IsDescendantOf( RUNTIME_CLASS( APowerupGiver ))) &&
				( static_cast<APowerupGiver *>( pActor )->PowerupType == RUNTIME_CLASS( APowerPossessionArtifact )))
			{
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_DestroyThing( pActor );

				pActor->Destroy( );
			}

			if (( pActor->state == RUNTIME_CLASS ( AInventory )->ActorInfo->FindState("HideDoomish") ) ||	// S_HIDEDOOMISH
				( pActor->state == RUNTIME_CLASS ( AInventory )->ActorInfo->FindState("HideSpecial") ) ||	// S_HIDESPECIAL
				( pActor->state == RUNTIME_CLASS ( AInventory )->ActorInfo->FindState("HideIndefinitely") ))	// S_HIDEINDEFINITELY
			{
				CLIENT_RestoreSpecialPosition( pActor );
				CLIENT_RestoreSpecialDoomThing( pActor, false );

				// Tell clients to respawn this item (without fog).
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_RespawnDoomThing( pActor, false );
			}
		}
	}

	// Normally, we set the playerstate to PST_ENTER so that enter scripts
	// are executed. However, we don't actually reset the map in possession, so
	// that is not necessary.
	// [BB] For some reason Skulltag always sent BOTEVENT_LMS_FIGHT on this occasion.
	GAMEMODE_RespawnAllPlayers ( BOTEVENT_LMS_FIGHT, PST_REBORNNOINVENTORY );

	// Also, spawn the possession artifact so that we can actually play!
	if ( NETWORK_InClientMode() == false )
	{
		GAME_SpawnPossessionArtifact( );
	}

	SCOREBOARD_RefreshHUD( );
}

//*****************************************************************************
//
void POSSESSION_ScorePossessionPoint( player_t *pPlayer )
{
	bool				bPointLimitReached;

	if ( pPlayer == NULL )
		return;

	// Don't do anything unless we're actually in possession mode.
	if (( possession == false ) && ( teampossession == false ))
		return;

	// Change the game state to the score sequence.
	POSSESSION_SetState( PSNS_HOLDERSCORED );

	// Give the player holding the artifact a point.
	PLAYER_SetPoints ( pPlayer, pPlayer->lPointCount + 1 );

	// If the player's on a team in team possession mode, give the player's point a team.
	if ( teampossession && pPlayer->bOnTeam )
		TEAM_SetScore( pPlayer->ulTeam, TEAM_GetScore( pPlayer->ulTeam ) + 1, true );

	// Refresh the HUD since there's bound to be changes.
	SCOREBOARD_RefreshHUD( );

	// Determine if the pointlimit has been reached.
	if (( teampossession ) && ( pPlayer->bOnTeam ))
		bPointLimitReached = ( pointlimit && ( TEAM_GetScore( pPlayer->ulTeam ) >= pointlimit ));
	else
		bPointLimitReached = ( pointlimit && ( pPlayer->lPointCount >= pointlimit ));

	// If the pointlimit has been reached, then display it in the console.
	if ( bPointLimitReached )
		NETWORK_Printf( "Pointlimit hit.\n" );

	// Display the score info.
	possession_DisplayScoreInfo( ULONG( pPlayer - players ));

	// End the round, or level after seconds (depending on whether or not the pointlimit
	// has been reached).
	GAME_SetEndLevelDelay( 5 * TICRATE );
}

//*****************************************************************************
//
void POSSESSION_ArtifactPickedUp( player_t *pPlayer, ULONG ulTicks )
{
	DHUDMessageFadeOut	*pMsg;

	// If we're just waiting for players, or counting down, don't bother doing
	// anything.
	if (( g_PSNState == PSNS_WAITINGFORPLAYERS ) ||
		( g_PSNState == PSNS_COUNTDOWN ) ||
		( g_PSNState == PSNS_PRENEXTROUNDCOUNTDOWN ) ||
		( g_PSNState == PSNS_NEXTROUNDCOUNTDOWN ))
	{
		return;
	}

	// Nothing to do if the player is invalid.
	if ( pPlayer == NULL )
		return;

	g_pPossessionArtifactCarrier = pPlayer;
	g_ulPSNArtifactHoldTicks = ulTicks;

	// Change the game state to the artifact being held, and begin the countdown.
	if ( NETWORK_InClientMode() == false )
	{
		POSSESSION_SetState( PSNS_ARTIFACTHELD );
	}

	// Print out a HUD message indicating that the possession artifact has been picked
	// up.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		pMsg = new DHUDMessageFadeOut( BigFont, GStrings( "POSSESSIONARTIFACT_PICKEDUP" ),
			1.5f,
			TEAM_MESSAGE_Y_AXIS,
			0,
			0,
			CR_RED,
			3.0f,
			0.25f );
		StatusBar->AttachMessage( pMsg, MAKE_ID('C','N','T','R') );
	}

	// Also, announce that the artifact has been picked up.
	ANNOUNCER_PlayEntry( cl_announcer, "PossessionArtifactPickedUp" );

	// Finally, if we're the server, inform the clients that the artifact has been picked
	// up, so that they can run this function.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DoPossessionArtifactPickedUp( ULONG( pPlayer - players ), g_ulPSNArtifactHoldTicks );
}

//*****************************************************************************
//
void POSSESSION_ArtifactDropped( void )
{
	DHUDMessageFadeOut	*pMsg;

	// If we're just waiting for players, or counting down, don't bother doing
	// anything.
	if (( g_PSNState == PSNS_WAITINGFORPLAYERS ) ||
		( g_PSNState == PSNS_COUNTDOWN ) ||
		( g_PSNState == PSNS_PRENEXTROUNDCOUNTDOWN ) ||
		( g_PSNState == PSNS_NEXTROUNDCOUNTDOWN ))
	{
		return;
	}

	g_pPossessionArtifactCarrier = NULL;
	g_ulPSNArtifactHoldTicks = 0;

	// Change the game state to being in progress.
	POSSESSION_SetState( PSNS_INPROGRESS );

	// Print out a HUD message indicating that the possession artifact has been picked
	// up.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		pMsg = new DHUDMessageFadeOut( BigFont, GStrings( "POSSESSIONARTIFACT_DROPPED" ),
			1.5f,
			TEAM_MESSAGE_Y_AXIS,
			0,
			0,
			CR_RED,
			3.0f,
			0.25f );
		StatusBar->AttachMessage( pMsg, MAKE_ID('C','N','T','R') );
	}

	// Also, announce that the artifact has been dropped.
	ANNOUNCER_PlayEntry( cl_announcer, "PossessionArtifactDropped" );

	// Finally, if we're the server, inform the clients that the artifact has been dropped,
	// so that they can run this function.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DoPossessionArtifactDropped( );
}

//*****************************************************************************
//
bool POSSESSION_ShouldRespawnArtifact( void )
{
	return (( g_PSNState == PSNS_WAITINGFORPLAYERS ) ||
			( g_PSNState == PSNS_COUNTDOWN ) ||
			( g_PSNState == PSNS_PRENEXTROUNDCOUNTDOWN ) ||
			( g_PSNState == PSNS_NEXTROUNDCOUNTDOWN ));
}

//*****************************************************************************
//
void POSSESSION_TimeExpired( void )
{
	char				szString[64];

	// Don't end the level if we're not playing.
	if (( POSSESSION_GetState( ) == PSNS_WAITINGFORPLAYERS ) ||
		( POSSESSION_GetState( ) == PSNS_COUNTDOWN ) ||
		( POSSESSION_GetState( ) == PSNS_PRENEXTROUNDCOUNTDOWN ) ||
		( POSSESSION_GetState( ) == PSNS_NEXTROUNDCOUNTDOWN ))
	{
		return;
	}

	// If the timelimit is reached, and no one is holding the stone,
	// sudden death is reached!
	if ( g_pPossessionArtifactCarrier == NULL )
	{
		// Only print the message the instant we reach sudden death.
		if ( level.time == (int)( timelimit * TICRATE * 60 ))
		{
			sprintf( szString, "\\cdSUDDEN DEATH!" );
			V_ColorizeString( szString );
			GAMEMODE_DisplayStandardMessage ( szString, true );
		}

		return;
	}

	// Change the game state to the score sequence.
	POSSESSION_SetState( PSNS_HOLDERSCORED );

	// Give the player holding the artifact a point.
	PLAYER_SetPoints ( g_pPossessionArtifactCarrier, g_pPossessionArtifactCarrier->lPointCount + 1 );

	// Also, display the score info for the player.
	possession_DisplayScoreInfo( ULONG( g_pPossessionArtifactCarrier - players ));

	// If the player's on a team in team possession mode, give the player's point a team.
	if ( teampossession && g_pPossessionArtifactCarrier->bOnTeam )
		TEAM_SetScore( g_pPossessionArtifactCarrier->ulTeam, TEAM_GetScore( g_pPossessionArtifactCarrier->ulTeam ) + 1, true );

	NETWORK_Printf( "%s\n", GStrings( "TXT_TIMELIMIT" ));
	GAME_SetEndLevelDelay( 5 * TICRATE );
}

//*****************************************************************************
//*****************************************************************************
//
ULONG POSSESSION_GetCountdownTicks( void )
{
	return ( g_ulPSNCountdownTicks );
}

//*****************************************************************************
//
void POSSESSION_SetCountdownTicks( ULONG ulTicks )
{
	g_ulPSNCountdownTicks = ulTicks;
}

//*****************************************************************************
//
PSNSTATE_e POSSESSION_GetState( void )
{
	return ( g_PSNState );
}

//*****************************************************************************
//
void POSSESSION_SetState( PSNSTATE_e State )
{
	g_PSNState = State;

	// Tell clients about the state change.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		if ( State == PSNS_ARTIFACTHELD )
			SERVERCOMMANDS_SetGameModeState( State, g_ulPSNArtifactHoldTicks );
		else
			SERVERCOMMANDS_SetGameModeState( State, g_ulPSNCountdownTicks );
	}
}

//*****************************************************************************
//
ULONG POSSESSION_GetArtifactHoldTicks( void )
{
	return ( g_ulPSNArtifactHoldTicks );
}

//*****************************************************************************
//
void POSSESSION_SetArtifactHoldTicks( ULONG ulTicks )
{
	g_ulPSNArtifactHoldTicks = ulTicks;
}

//*****************************************************************************
//*****************************************************************************
//
void possession_DisplayScoreInfo( ULONG ulPlayer )
{
	char				szString[64];
	char				szScorer[64];
	bool				bPointLimitReached;
	DHUDMessageFadeOut	*pMsg;

	if ( ulPlayer >= MAXPLAYERS )
		return;

	// First, determine if the pointlimit has been reached.
	if (( teampossession ) && ( players[ulPlayer].bOnTeam ))
		bPointLimitReached = ( pointlimit && ( TEAM_GetScore( players[ulPlayer].ulTeam ) >= pointlimit ));
	else
		bPointLimitReached = ( pointlimit && ( players[ulPlayer].lPointCount >= pointlimit ));

	// Build the string that's displayed in big letters in the center of the screen.
	// [RC] On team possession, state who scored.
	if ( teampossession && ( players[ulPlayer].bOnTeam ))
	{
		sprintf( szString, "\\c%c%s %s!", V_GetColorChar( TEAM_GetTextColor( players[ulPlayer].ulTeam )), TEAM_GetName( players[ulPlayer].ulTeam ) ,bPointLimitReached ? "WINS" : "SCORES" );
		sprintf( szScorer, "\\c%cScored by: %s", V_GetColorChar( TEAM_GetTextColor( players[ulPlayer].ulTeam )), players[ulPlayer].userinfo.GetName() );

		// [BB] I don't see why we should remove the player name's color codes here. It's not done in CTF either
		// and the player's team is apparent from the rest of the message.
		//V_RemoveColorCodes( szScorer );
		V_ColorizeString( szScorer );
	}
	else
		sprintf( szString, "%s \\c-%s!", players[ulPlayer].userinfo.GetName(), bPointLimitReached ? "WINS" : "SCORES" );
	V_ColorizeString( szString );

	// Print out the HUD message that displays who scored/won. If we're the server, just
	// send the parameters to the client.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
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

		// [RC] Display small HUD message for the scorer
		if ( teampossession && ( players[ulPlayer].bOnTeam ))
		{
			pMsg = new DHUDMessageFadeOut( SmallFont, szScorer,
					160.4f,
					90.0f,
					320,
					200,
					CR_RED,
					3.0f,
					2.0f );
			StatusBar->AttachMessage( pMsg, MAKE_ID('S','U','B','S') );
		}
	}
	else
	{
		SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 160.4f, 75.0f, 320, 200, CR_RED, 3.0f, 2.0f, "BigFont", false, MAKE_ID('C','N','T','R') );
		if ( teampossession && ( players[ulPlayer].bOnTeam ))
			SERVERCOMMANDS_PrintHUDMessageFadeOut( szScorer, 160.4f, 90.0f, 320, 200, CR_RED, 3.0f, 2.0f, "SmallFont", false, MAKE_ID('S','U','B','S') );
	}
}

//*****************************************************************************
//	CONSOLE COMMANDS/VARIABLES

CVAR( Int, sv_possessioncountdowntime, 10, CVAR_ARCHIVE );
CVAR( Int, sv_possessionholdtime, 30, CVAR_ARCHIVE );
CVAR( Bool, sv_usemapsettingspossessionholdtime, true, CVAR_ARCHIVE );
