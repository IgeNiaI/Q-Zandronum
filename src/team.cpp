//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2002 Brad Carney
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
// Filename: team.cpp
//
// Description: Contains team routines
//
//-----------------------------------------------------------------------------

#include "a_doomglobal.h"
#include "a_pickups.h"
#include "announcer.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "campaign.h"
#include "cl_commands.h"
#include "cl_demo.h"
#include "cl_main.h"
#include "cooperative.h"
#include "deathmatch.h"
#include "doomstat.h"
#include "g_game.h"
#include "g_level.h"
#include "info.h"
#include "joinqueue.h"
#include "m_random.h"
#include "network.h"
#include "p_acs.h"
#include "p_effect.h"
#include "p_local.h"
#include "s_sound.h"
#include "sbar.h"
#include "scoreboard.h"
#include "sv_commands.h"
#include "sv_main.h"
#include "team.h"
#include "v_palette.h"
#include "v_text.h"
#include "v_video.h"
#include "templates.h"
#include "d_event.h"

//*****************************************************************************
//	MISC CRAP THAT SHOULDN'T BE HERE BUT HAS TO BE BECAUSE OF SLOPPY CODING

void	SERVERCONSOLE_UpdateScoreboard( void );

//*****************************************************************************
//	VARIABLES

static	bool	g_bSimpleCTFSTMode;
static	bool	g_bWhiteFlagTaken;
static	POS_t	g_WhiteFlagOrigin;
static	ULONG	g_ulWhiteFlagReturnTicks;

// Are we spawning a temporary flag? If so, ignore return zones.
static	bool	g_bSpawningTemporaryFlag = false;

static FRandom	g_JoinTeamSeed( "JoinTeamSeed" );

//*****************************************************************************
//	CONSOLE VARIABLES

CVAR( Bool, sv_forcerandomclass, false, 0 )

//*****************************************************************************
//	FUNCTIONS

void TEAM_Construct( void )
{
	for ( ULONG i = 0; i < teams.Size( ); i++ )
	{
		TEAM_SetScore( i, 0, false );
		TEAM_SetReturnTicks( i, 0 );
		TEAM_SetFragCount( i, 0, false );
		TEAM_SetDeathCount( i, 0 );
		TEAM_SetWinCount( i, 0, false );
		TEAM_SetCarrier( i, NULL );
		TEAM_SetItemTaken( i, false );
		TEAM_SetAnnouncedLeadState( i, false );
		TEAM_SetAssistPlayer( i, MAXPLAYERS );

		teams[i].g_Origin.x = 0;
		teams[i].g_Origin.y = 0;
		teams[i].g_Origin.z = 0;

		switch ( i )
		{
		case 0:
			teams[i].ulReturnScriptOffset = SCRIPT_BlueReturn;
			break;
		case 1:
			teams[i].ulReturnScriptOffset = SCRIPT_RedReturn;
			break;
		default:
			break;
		}
	}

	// Start off in regular CTF/ST mode.
	TEAM_SetSimpleCTFSTMode( false );

	TEAM_SetWhiteFlagTaken( false );
	g_ulWhiteFlagReturnTicks = 0;
}

//*****************************************************************************
//
void TEAM_Tick( void )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < teams.Size( ); ulIdx++ )
	{
		if ( teams[ulIdx].ulReturnTicks )
		{
			teams[ulIdx].ulReturnTicks--;
			if ( teams[ulIdx].ulReturnTicks == 0 )
				TEAM_ExecuteReturnRoutine( ulIdx, NULL );
		}
	}

	if ( g_ulWhiteFlagReturnTicks )
	{
		g_ulWhiteFlagReturnTicks--;
		if ( g_ulWhiteFlagReturnTicks == 0 )
			TEAM_ExecuteReturnRoutine( ulIdx, NULL );
	}
}

//*****************************************************************************
//
void TEAM_Reset( void )
{
	for ( ULONG i = 0; i < teams.Size( ); i++ )
	{
		TEAM_SetScore( i, 0, false );
		TEAM_SetReturnTicks( i, 0 );
		TEAM_SetFragCount( i, 0, false );
		TEAM_SetDeathCount( i, 0 );
		TEAM_SetWinCount( i, 0, false );
		TEAM_SetCarrier( i, NULL );
		TEAM_SetItemTaken( i, false );
		TEAM_SetAnnouncedLeadState( i, false );
		TEAM_SetAssistPlayer( i, MAXPLAYERS );

		teams[i].g_Origin.x = 0;
		teams[i].g_Origin.y = 0;
		teams[i].g_Origin.z = 0;

		switch ( i )
		{
		case 0:
			teams[i].ulReturnScriptOffset = SCRIPT_BlueReturn;
			break;
		case 1:
			teams[i].ulReturnScriptOffset = SCRIPT_RedReturn;
			break;
		default:
			break;
		}
	}

	TEAM_SetWhiteFlagTaken( false );
	g_ulWhiteFlagReturnTicks = 0;
}

//*****************************************************************************
//
ULONG TEAM_CountPlayers( ULONG ulTeamIdx )
{
	ULONG	ulIdx;
	ULONG	ulCount;

	ulCount = 0;
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		// Don't count players not in the game or spectators.
		if (( playeringame[ulIdx] == false ) || ( PLAYER_IsTrueSpectator( &players[ulIdx] )))
			continue;

		if ( players[ulIdx].bOnTeam && ( players[ulIdx].ulTeam == ulTeamIdx ))
			ulCount++;
	}

	return ( ulCount );
}

//*****************************************************************************
//
ULONG TEAM_CountLivingAndRespawnablePlayers( ULONG ulTeamIdx )
{
	ULONG	ulIdx;
	ULONG	ulCount;

	ulCount = 0;
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		// Don't count players not in the game or spectators.
		if (( playeringame[ulIdx] == false ) || ( PLAYER_IsTrueSpectator( &players[ulIdx] )) || ( players[ulIdx].bDeadSpectator == true ))
			continue;

		// Don't count dead players. [BB] Only if they can't respawn.
		if ( PLAYER_IsAliveOrCanRespawn ( &players[ulIdx] ) == false )
			continue;

		if ( players[ulIdx].bOnTeam && ( players[ulIdx].ulTeam == ulTeamIdx ))
			ulCount++;
	}

	return ( ulCount );
}

//*****************************************************************************
//
ULONG TEAM_TeamsWithPlayersOn( void )
{
	ULONG ulTeamsWithPlayersOn = 0;

	for ( ULONG i = 0; i < teams.Size( ); i++ )
	{
		if (TEAM_CountPlayers (i) > 0)
			ulTeamsWithPlayersOn ++;
	}

	return ulTeamsWithPlayersOn;
}


//*****************************************************************************
//
void TEAM_ExecuteReturnRoutine( ULONG ulTeamIdx, AActor *pReturner )
{
	AActor						*pTeamItem;
	const PClass				*pClass;
	TThinkerIterator<AActor>	Iterator;

	// [BB] Allow teams.Size( ) here, this handles the white flag.
	if ( ( TEAM_CheckIfValid( ulTeamIdx ) == false ) && ( ulTeamIdx != teams.Size( ) ) )
		return;

	// Execute the return scripts.
	if ( NETWORK_InClientMode() == false )
	{
		if ( ulTeamIdx == teams.Size( ) )
			FBehavior::StaticStartTypedScripts( SCRIPT_WhiteReturn, NULL, true );
		else
			FBehavior::StaticStartTypedScripts( TEAM_GetReturnScriptOffset( ulTeamIdx ), NULL, true );
	}

	if ( ulTeamIdx == teams.Size( ) )
		pClass = PClass::FindClass( "WhiteFlag" );
	else
		pClass = TEAM_GetItem( ulTeamIdx );

	g_bSpawningTemporaryFlag = true;
	pTeamItem = Spawn( pClass, 0, 0, 0, NO_REPLACE );
	g_bSpawningTemporaryFlag = false;
	if ( pTeamItem->IsKindOf( PClass::FindClass( "TeamItem" )) == false )
	{
		pTeamItem->Destroy( );
		return;
	}

	// In non-simple CTF mode, scripts take care of the returning and displaying messages.
	if ( TEAM_GetSimpleCTFSTMode( ))
	{
		if ( NETWORK_InClientMode() == false )
			static_cast<ATeamItem *>( pTeamItem )->ReturnFlag( pReturner );
		static_cast<ATeamItem *>( pTeamItem )->DisplayFlagReturn( );
	}

	static_cast<ATeamItem *>( pTeamItem )->ResetReturnTicks( );
	static_cast<ATeamItem *>( pTeamItem )->AnnounceFlagReturn( );

	// Destroy the temporarily created flag.
	pTeamItem->Destroy( );
	pTeamItem = NULL;

	// Destroy any sitting flags that being returned from the return ticks running out,
	// or whatever reason.
	if ( NETWORK_InClientMode() == false )
	{
		while (( pTeamItem = Iterator.Next( )))
		{
			if (( pTeamItem->GetClass( ) != pClass ) || (( pTeamItem->flags & MF_DROPPED ) == false ))
				continue;

			// If we're the server, tell clients to destroy the flag.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_DestroyThing( pTeamItem );

			pTeamItem->Destroy( );
		}
	}

	// Tell clients that the flag has been returned.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_TeamFlagReturned( ulTeamIdx );
	else
		SCOREBOARD_RefreshHUD( );
}

//*****************************************************************************
//
ULONG TEAM_ChooseBestTeamForPlayer( const bool bIgnoreTeamStartsAvailability )
{
	// Possible teams to select.
	bool bPossibleTeams[MAX_TEAMS];

	// The score used for each team.
	LONG lGotScore[MAX_TEAMS];

	// The lowest amount of players on any team.
	ULONG ulLowestPlayerCount = ULONG_MAX;

	// The amount of possible teams to choose from. [BB] Properly set in the loop below.
	ULONG ulPossibleTeamCount = 0;

	// The lowest score on any team.
	LONG lLowestScoreCount = LONG_MAX;

	for ( ULONG i = 0; i < MAX_TEAMS; i++)
	{
		bPossibleTeams[i] = false;

		if ( TEAM_CheckIfValid( i ) == false )
			continue;

		if ( ( bIgnoreTeamStartsAvailability == false ) && ( TEAM_ShouldUseTeam( i ) == false ) )
			continue;

		bPossibleTeams[i] = true;
		++ulPossibleTeamCount;
		lGotScore[i] = LONG_MIN;
	}

	// [BB] No possible team found (shouldn't happen), so we can't recommend any team.
	if ( ulPossibleTeamCount == 0 )
		return teams.Size( );

	for ( ULONG i = 0; i < teams.Size( ); i++ )
	{
		if ( bPossibleTeams[i] == false )
			continue;

		if ( TEAM_CountPlayers( i ) < ulLowestPlayerCount )
			ulLowestPlayerCount = TEAM_CountPlayers( i );
	}

	for ( ULONG i = 0; i < teams.Size( ); i++ )
	{
		if ( bPossibleTeams[i] == false )
			continue;

		if ( TEAM_CountPlayers( i ) != ulLowestPlayerCount )
		{
			bPossibleTeams[i] = false;
			ulPossibleTeamCount--;
		}
	}

	// [BB] A single team has the least amount of players, recommend this team for the player.
	if ( ulPossibleTeamCount == 1 )
	{
		for ( ULONG i = 0; i < teams.Size( ); i++ )
		{
			if ( bPossibleTeams[i] )
				return ( i );
		}
	}

	for ( ULONG i = 0; i < teams.Size( ); i++ )
	{
		if ( bPossibleTeams[i] == false )
			continue;

		if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNFRAGS )
		{
			if ( lLowestScoreCount > TEAM_GetFragCount( i ))
				lLowestScoreCount = TEAM_GetFragCount( i );
		}
		else if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNWINS )
		{
			if ( lLowestScoreCount > TEAM_GetWinCount( i ))
				lLowestScoreCount = TEAM_GetWinCount( i );
		}
		else
		{
			if ( lLowestScoreCount > TEAM_GetScore( i ))
				lLowestScoreCount = TEAM_GetScore( i );
		}
	}

	for ( ULONG i = 0; i < teams.Size( ); i++ )
	{
		if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNFRAGS )
			lGotScore[i] = TEAM_GetFragCount( i );
		else if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNWINS )
			lGotScore[i] = TEAM_GetWinCount( i );
		else
			lGotScore[i] = TEAM_GetScore( i );
	}

	for ( ULONG i = 0; i < teams.Size( ); i++ )
	{
		if ( bPossibleTeams[i] == false )
			continue;

		if ( lGotScore[i] != lLowestScoreCount )
		{
			bPossibleTeams[i] = false;
			ulPossibleTeamCount--;
		}
	}

	while( true )
	{
		ULONG ulRandomTeam;

		ulRandomTeam = g_JoinTeamSeed.Random( ) % teams.Size( );

		if ( bPossibleTeams[ulRandomTeam] == false )
			continue;

		return ( ulRandomTeam );
	}
}

//*****************************************************************************
//
void TEAM_ScoreSkulltagPoint( player_t *pPlayer, ULONG ulNumPoints, AActor *pPillar )
{
	char				szString[256];
	POS_t				SkullOrigin;
	DHUDMessageFadeOut	*pMsg;
	AActor				*pActor;
	AInventory			*pInventory = NULL;
	bool				bAssisted;
	bool				bSelfAssisted = false;
	ULONG				ulTeamIdx = 0;

	// Determine who assisted.
	bAssisted = ( TEAM_GetAssistPlayer( pPlayer->ulTeam ) != MAXPLAYERS );

	if ( bAssisted )
	{
		// Self assist?
		bSelfAssisted = false;
		for( ULONG i = 0; i < MAXPLAYERS; i++ )
		{
			if( (&players[i] == pPlayer) && (TEAM_GetAssistPlayer( pPlayer->ulTeam ) == i) )
			{
				bSelfAssisted = true;
				break;
			}
		}
	}

	// Create the console message.
	if( ( bAssisted ) && ( ! bSelfAssisted ) )
		sprintf(szString, "%s \\c-and %s\\c- scored for the \\c%c%s \\c-team!\n", pPlayer->userinfo.GetName(), players[TEAM_GetAssistPlayer( pPlayer->ulTeam )].userinfo.GetName(), V_GetColorChar( TEAM_GetTextColor( pPlayer->ulTeam )), TEAM_GetName( pPlayer->ulTeam ));
	else
		sprintf(szString, "%s \\c-scored for the \\c%c%s \\c-team!\n", pPlayer->userinfo.GetName(), V_GetColorChar( TEAM_GetTextColor( pPlayer->ulTeam )), TEAM_GetName( pPlayer->ulTeam ));

	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_Print( szString, PRINT_HIGH );

	// Create the fullscreen message.
	FString coloredTeamName = V_GetColorChar( TEAM_GetTextColor( pPlayer->ulTeam ) );
	coloredTeamName += " ";
	coloredTeamName += TEAM_GetName( pPlayer->ulTeam );
	switch ( ulNumPoints )
	{
	case 0:

		return;
	case 1:

		sprintf( szString, "\\c%s team scores!", coloredTeamName.GetChars() );
		break;
	case 2:

		sprintf( szString, "\\c%s scores two points!", coloredTeamName.GetChars() );
		break;
	case 3:

		sprintf( szString, "\\c%s scores three points!", coloredTeamName.GetChars() );
		break;
	case 4:

		sprintf( szString, "\\c%s scores four points!", coloredTeamName.GetChars() );
		break;
	case 5:

		sprintf( szString, "\\c%s scores five points!", coloredTeamName.GetChars() );
		break;
	default:

		sprintf( szString, "\\c%s scores %d points!", coloredTeamName.GetChars(), static_cast<unsigned int> (ulNumPoints) );
		break;
	}

	V_ColorizeString( szString );

	// Now, print it.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		pMsg = new DHUDMessageFadeOut( BigFont, szString,
			1.5,
			TEAM_MESSAGE_Y_AXIS,
			0,
			0,
			CR_BLUE,
			3.0f,
			0.5f );
		StatusBar->AttachMessage( pMsg, MAKE_ID('C','N','T','R') );
	}
	// If necessary, send it to clients.
	else
	{
		SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS, 0, 0, CR_BLUE, 3.0f, 0.5f, "BigFont", false, MAKE_ID('C','N','T','R') );
	}

	// Create the "scored by / assisted by" message.
	sprintf( szString, "\\c%cScored by: %s", V_GetColorChar( TEAM_GetTextColor( pPlayer->ulTeam ) ), pPlayer->userinfo.GetName());

	if ( bAssisted )
	{
		if ( bSelfAssisted )
			sprintf( szString + strlen ( szString ), "\\n\\c%c( Self-Assisted )", V_GetColorChar( TEAM_GetTextColor( pPlayer->ulTeam ) ) );
		else
			sprintf( szString + strlen ( szString ), "\\n\\c%cAssisted by: %s", V_GetColorChar( TEAM_GetTextColor( pPlayer->ulTeam ) ), players[TEAM_GetAssistPlayer( pPlayer->ulTeam )].userinfo.GetName());
	}
	
	V_ColorizeString( szString );

	// Now, print it.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		pMsg = new DHUDMessageFadeOut( SmallFont, szString,
			1.5f,
			TEAM_MESSAGE_Y_AXIS_SUB,
			0,
			0,
			(EColorRange)(TEAM_GetTextColor (pPlayer->ulTeam)),
			3.0f,
			0.5f );
		StatusBar->AttachMessage( pMsg, MAKE_ID('S','U','B','S') );
	}
	// If necessary, send it to clients.
	else
		SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS_SUB, 0, 0, CR_BLUE, 3.0f, 0.5f, "SmallFont", false, MAKE_ID('S','U','B','S') );

	// Give his team a point.
	TEAM_SetScore( pPlayer->ulTeam, TEAM_GetScore( pPlayer->ulTeam ) + ulNumPoints, true );
	PLAYER_SetPoints ( pPlayer, pPlayer->lPointCount + ulNumPoints );

	// Take the skull away.
	for ( ULONG i = 0; i < teams.Size( ); i++ )
	{
		pInventory = pPlayer->mo->FindInventory( TEAM_GetItem( i ));

		if ( pInventory )
		{
			ulTeamIdx = i;
			break;
		}
	}

	if ( pInventory )
		pPlayer->mo->RemoveInventory( pInventory );

	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_TakeInventory( ULONG( pPlayer - players ), TEAM_GetItem( ulTeamIdx ), 0 );
	else
		SCOREBOARD_RefreshHUD( );

	// Respawn the skull.
	SkullOrigin = TEAM_GetItemOrigin( ulTeamIdx );

	pActor = Spawn( TEAM_GetItem( ulTeamIdx ), SkullOrigin.x, SkullOrigin.y, SkullOrigin.z, NO_REPLACE );

	// Since all inventory spawns with the MF_DROPPED flag, we need to unset it.
	if ( pActor )
		pActor->flags &= ~MF_DROPPED;

	// If we're the server, tell clients to spawn the new skull.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( pActor ))
		SERVERCOMMANDS_SpawnThing( pActor );

	// Mark the skull as no longer being taken.
	TEAM_SetItemTaken( ulTeamIdx, false );

	// Award the scorer with a "Tag!" medal.
	MEDAL_GiveMedal( ULONG( pPlayer - players ), MEDAL_TAG );

	// Tell clients about the medal that been given.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_GivePlayerMedal( ULONG( pPlayer - players ), MEDAL_TAG );

	// If someone just recently returned the skull, award him with an "Assist!" medal.
	if ( TEAM_GetAssistPlayer( pPlayer->ulTeam ) != MAXPLAYERS )
	{
		MEDAL_GiveMedal( TEAM_GetAssistPlayer( pPlayer->ulTeam ), MEDAL_ASSIST );

		// Tell clients about the medal that been given.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_GivePlayerMedal( TEAM_GetAssistPlayer( pPlayer->ulTeam ), MEDAL_ASSIST );

		TEAM_SetAssistPlayer( pPlayer->ulTeam, MAXPLAYERS );
	}

	FString Name;
	
	Name = "Tag";
	Name += TEAM_GetName( ulTeamIdx );
	Name += "Skull";

	FState *SkulltagScoreState = pPillar->FindState( (FName)Name.GetChars( ));

	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetThingFrame( pPillar, SkulltagScoreState );

	pPillar->SetState( SkulltagScoreState );
}

//*****************************************************************************
//
void TEAM_DisplayNeedToReturnSkullMessage( player_t *pPlayer )
{
	char						szString[256];
	DHUDMessageFadeOut			*pMsg;

	if (( pPlayer == NULL ) || ( pPlayer->bOnTeam == false ))
		return;

	// Create the message.
	sprintf( szString, "\\c%cThe %s skull must be returned first!", V_GetColorChar( TEAM_GetTextColor( pPlayer->ulTeam )), TEAM_GetName( pPlayer->ulTeam ));

	V_ColorizeString( szString );

	// Now, print it.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		pMsg = new DHUDMessageFadeOut( SmallFont, szString,
			1.5f,
			TEAM_MESSAGE_Y_AXIS,
			0,
			0,
			CR_UNTRANSLATED,
			1.0f,
			0.25f );
		StatusBar->AttachMessage( pMsg, MAKE_ID('C','N','T','R') );
	}
	// If necessary, send it to clients.
	else
	{
		SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS, 0, 0, CR_RED, 1.0f, 0.25f, "SmallFont", false, MAKE_ID('C','N','T','R'), ULONG( pPlayer - players ), SVCF_ONLYTHISCLIENT );
	}
}

//*****************************************************************************
//
void TEAM_FlagDropped( player_t *pPlayer, ULONG ulTeamIdx )
{
	DHUDMessageFadeOut	*pMsg;
	char				szString[64];

	// First, make sure the player is valid, and on a valid team.
	if (( pPlayer == NULL ) ||
		(( pPlayer - players ) >= MAXPLAYERS ) ||
		(( pPlayer - players ) < 0 ) ||
		( pPlayer->bOnTeam == false ) ||
		( TEAM_CheckIfValid( pPlayer->ulTeam ) == false ))
	{
		return;
	}

	// If we're the server, just tell clients to do this.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVERCOMMANDS_TeamFlagDropped( ULONG( pPlayer - players ), ulTeamIdx );
		SERVER_Printf( PRINT_MEDIUM, "%s \\c-lost the \\c%c%s \\c-%s.\n", pPlayer->userinfo.GetName(), V_GetColorChar( TEAM_GetTextColor( ulTeamIdx)), TEAM_GetName( ulTeamIdx), ( skulltag ) ? "skull" : "flag" );
		return;
	}

	// Add the console message.
	Printf( "%s %s dropped!\n", TEAM_GetName( ulTeamIdx ), ( skulltag ) ? "skull" : "flag" );

	// Next, build the dropped message.
	sprintf( szString, "\\c%c%s %s dropped!", V_GetColorChar( TEAM_GetTextColor( ulTeamIdx )), TEAM_GetName( ulTeamIdx ), ( skulltag ) ? "skull" : "flag" );

	// Colorize it.
	V_ColorizeString( szString );

	// Now, print it.
	pMsg = new DHUDMessageFadeOut( BigFont, szString,
		1.5f,
		TEAM_MESSAGE_Y_AXIS,
		0,
		0,
		CR_WHITE,
		3.0f,
		0.25f );
	StatusBar->AttachMessage( pMsg, MAKE_ID('C','N','T','R') );

	// Finally, play the announcer entry associated with this event.
	sprintf( szString, "%s%sDropped", TEAM_GetName( ulTeamIdx ), ( skulltag ) ? "skull" : "flag" );
	ANNOUNCER_PlayEntry( cl_announcer, szString );
}

//*****************************************************************************
//
WORD TEAM_GetReturnScriptOffset( ULONG ulTeamIdx )
{
	if ( TEAM_CheckIfValid( ulTeamIdx ))
		return ( (WORD)teams[ulTeamIdx].ulReturnScriptOffset );
	else
		return ( -1 );
}

//*****************************************************************************
//
void TEAM_DoWinSequence( ULONG ulTeamIdx )
{
	char				szString[32];
	DHUDMessageFadeOut	*pMsg;

	// Display "%s WINS!" HUD message.
	if ( ulTeamIdx < teams.Size( ) )
		sprintf( szString, "\\c%c%s WINS!", V_GetColorChar( TEAM_GetTextColor( ulTeamIdx ) ), TEAM_GetName( ulTeamIdx ) );
	else
		sprintf( szString, "DRAW GAME!\n" );

	V_ColorizeString( szString );

	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		pMsg = new DHUDMessageFadeOut( BigFont, szString,
			160.4f,
			75.0f,
			320,
			200,
			CR_UNTRANSLATED,
			3.0f,
			2.0f );

		StatusBar->AttachMessage( pMsg, MAKE_ID('C','N','T','R') );
	}
	else
	{
		SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 160.4f, 75.0f, 320, 200, CR_RED, 3.0f, 0.25f, "BigFont", false, MAKE_ID('C','N','T','R') );
	}
}

//*****************************************************************************
//
void TEAM_TimeExpired( void )
{
	LONG				lWinner = 0;
	DHUDMessageFadeOut	*pMsg;
	char				szString[64];
	ULONG				lHighestScore;
	ULONG				ulLeadingTeamsCount = 0;

	// If there are players on the map, either declare a winner or sudden death. If
	// there aren't any, then just end the map.
	if ( SERVER_CalcNumPlayers( ))
	{
		if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNPOINTS )
			lHighestScore = TEAM_GetHighestScoreCount( );
		else
			lHighestScore = TEAM_GetHighestFragCount( );

		for ( ULONG i = 0; i < teams.Size( ); i++ )
		{
			if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNPOINTS )
			{
				if ( lHighestScore == static_cast<unsigned> (TEAM_GetScore( i )))
				{
					ulLeadingTeamsCount++;
					lWinner = i;
				}
			}
			else
			{
				if ( lHighestScore == static_cast<unsigned> (TEAM_GetFragCount( i )))
				{
					ulLeadingTeamsCount++;
					lWinner = i;
				}
			}
		}

		if ( ulLeadingTeamsCount > 1 )
			lWinner = teams.Size( );

		// If there was a tie, then go into sudden death!
		if ( sv_suddendeath && ( (ULONG)lWinner == teams.Size( ) ) )
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
						CR_UNTRANSLATED,
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

		// Also, do the win sequence for the player.
		TEAM_DoWinSequence( lWinner );
	}

	NETWORK_Printf( "%s\n", GStrings( "TXT_TIMELIMIT" ));
	GAME_SetEndLevelDelay( 5 * TICRATE );
}

//*****************************************************************************
//
bool TEAM_SpawningTemporaryFlag( void )
{
	return ( g_bSpawningTemporaryFlag );
}

//*****************************************************************************
//*****************************************************************************
//
bool TEAM_CheckIfValid( ULONG ulTeamIdx )
{
	if ( ulTeamIdx >= TEAM_GetNumAvailableTeams( ))
		return ( false );

	return ( true );
}

//*****************************************************************************
//
const char *TEAM_GetName( ULONG ulTeamIdx )
{
	if ( TEAM_CheckIfValid( ulTeamIdx ))
		return ( teams[ulTeamIdx].Name.GetChars( ) );
	else
		return ( "" );
}

//*****************************************************************************
//
void TEAM_SetName( ULONG ulTeamIdx, const char *pszName )
{
	teams[ulTeamIdx].Name = pszName;
}

//*****************************************************************************
//
ULONG TEAM_GetTeamNumberByName( const char *pszName )
{
	for ( ULONG i = 0; i < teams.Size( ); i++ )
	{
		if ( !stricmp( pszName, TEAM_GetName( i ) ) )
			return i;
	}

	return teams.Size( );
}

//*****************************************************************************
//
int TEAM_GetColor( ULONG ulTeamIdx )
{
	if ( TEAM_CheckIfValid( ulTeamIdx ))
		return ( teams[ulTeamIdx].lPlayerColor );
	else
		return ( 0 );
}

//*****************************************************************************
//
void TEAM_SetColor( ULONG ulTeamIdx, int r, int g, int b )
{
	if ( TEAM_CheckIfValid( ulTeamIdx ))
		teams[ulTeamIdx].lPlayerColor = MAKERGB (r, g, b);
}

//*****************************************************************************
//
bool TEAM_IsCustomPlayerColorAllowed( ULONG ulTeamIdx )
{
	if ( TEAM_CheckIfValid( ulTeamIdx ))
		return ( teams[ulTeamIdx].bCustomPlayerColorAllowed );
	// [BB] Players on an invalid team (or not on a team) are free to choose their color
	else
		return true;
}

//*****************************************************************************
//
ULONG TEAM_GetTextColor( ULONG ulTeamIdx )
{
	if ( TEAM_CheckIfValid( ulTeamIdx ))
	{
		if ( teams[ulTeamIdx].TextColor.IsEmpty( ))
		{
			return ( CR_UNTRANSLATED );
		}
		const BYTE *cp = (const BYTE *)teams[ulTeamIdx].TextColor.GetChars( );
		LONG lColor = V_ParseFontColor( cp, 0, 0 );
		if ( lColor == CR_UNDEFINED )
		{
			lColor = CR_UNTRANSLATED;
		}
		return ( lColor );
	}

	return ( CR_UNTRANSLATED );
}

//*****************************************************************************
//
void TEAM_SetTextColor( ULONG ulTeamIdx, ULONG ulColor )
{
	if ( TEAM_CheckIfValid( ulTeamIdx ))
		teams[ulTeamIdx].ulTextColor = ulColor;
}

//*****************************************************************************
//
LONG TEAM_GetRailgunColor( ULONG ulTeamIdx )
{
	if ( TEAM_CheckIfValid( ulTeamIdx ))
		return ( teams[ulTeamIdx].lRailColor );
	else
		return ( 0 );
}

//*****************************************************************************
//
void TEAM_SetRailgunColor( ULONG ulTeamIdx, LONG lColor )
{
	if ( TEAM_CheckIfValid( ulTeamIdx ))
		teams[ulTeamIdx].lRailColor = lColor;
}

//*****************************************************************************
//
LONG TEAM_GetScore( ULONG ulTeamIdx )
{
	if ( TEAM_CheckIfValid( ulTeamIdx ))
		return ( teams[ulTeamIdx].lScore );
	else
		return ( 0 );
}

//*****************************************************************************
//
void TEAM_SetScore( ULONG ulTeamIdx, LONG lScore, bool bAnnouncer )
{
	LONG				lOldScore;

	if ( TEAM_CheckIfValid( ulTeamIdx ) == false )
		return;

	lOldScore = TEAM_GetScore( ulTeamIdx );
	teams[ulTeamIdx].lScore = lScore;
	if ( bAnnouncer && ( TEAM_GetScore( ulTeamIdx ) > lOldScore ))
	{
		// Build the message.
		// Whatever the team's name is, is the first part of the message. For example:
		// if the "blue" team has been defined then the message will be "BlueScores".
		// This way we don't have to change every announcer to use a new system. 
		FString name;
		name += TEAM_GetName( ulTeamIdx );
		name += "Scores";

		// Play the announcer sound for scoring.
		ANNOUNCER_PlayEntry( cl_announcer, name.GetChars( ));
	}

	// If we're the server, tell clients about the team score update.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVERCOMMANDS_SetTeamScore( ulTeamIdx, lScore, bAnnouncer );

		// Also, update the scoreboard.
		SERVERCONSOLE_UpdateScoreboard( );
	}

	// Implement the pointlimit.
	if ( pointlimit <= 0 || NETWORK_InClientMode() )
		return;

	if ( TEAM_GetScore( ulTeamIdx ) >= (LONG)pointlimit )
	{
		NETWORK_Printf( "\\c%c%s \\c-has won the game!\n", V_GetColorChar( TEAM_GetTextColor( ulTeamIdx )), TEAM_GetName( ulTeamIdx ));

		// Do the win sequence for the winner.
		TEAM_DoWinSequence( ulTeamIdx );

		// End the level after five seconds.
		GAME_SetEndLevelDelay( 5 * TICRATE );
	}
}

//*****************************************************************************
//
const char *TEAM_GetSmallHUDIcon( ULONG ulTeamIdx )
{
	if ( TEAM_CheckIfValid( ulTeamIdx ))
	{
		if ( GAMEMODE_GetCurrentFlags() & GMF_USEFLAGASTEAMITEM )
			return ( teams[ulTeamIdx].SmallFlagHUDIcon.GetChars( ));
		else
			return ( teams[ulTeamIdx].SmallSkullHUDIcon.GetChars( ) );
	}

	return ( "" );
}

//*****************************************************************************
//
void TEAM_SetSmallHUDIcon( ULONG ulTeamIdx, const char *pszName, bool bFlag )
{
	if ( TEAM_CheckIfValid( ulTeamIdx ))
	{
		if ( bFlag )
			teams[ulTeamIdx].SmallFlagHUDIcon = pszName;
		else
			teams[ulTeamIdx].SmallSkullHUDIcon = pszName;
	}
}

//*****************************************************************************
//
const char *TEAM_GetLargeHUDIcon( ULONG ulTeamIdx )
{
	if ( ulTeamIdx < teams.Size( ))
	{
		if ( GAMEMODE_GetCurrentFlags() & GMF_USEFLAGASTEAMITEM )
			return ( teams[ulTeamIdx].LargeFlagHUDIcon.GetChars( ));
		else
			return ( teams[ulTeamIdx].LargeSkullHUDIcon.GetChars( ) );
	}

	return ( "" );
}

//*****************************************************************************
//
void TEAM_SetLargeHUDIcon( ULONG ulTeamIdx, const char *pszName, bool bFlag )
{
	if ( TEAM_CheckIfValid( ulTeamIdx ))
	{
		if ( bFlag )
			teams[ulTeamIdx].LargeFlagHUDIcon = pszName;
		else
			teams[ulTeamIdx].LargeSkullHUDIcon = pszName;
	}
}

//*****************************************************************************
//
bool TEAM_HasCustomString( ULONG ulTeamIdx, const FString TEAMINFO::*stringPointer )
{
	if ( TEAM_CheckIfValid( ulTeamIdx ))
		return ( (teams[ulTeamIdx].*stringPointer).IsNotEmpty() );
	else
		return false;
}

//*****************************************************************************
//
const char *TEAM_GetCustomString( ULONG ulTeamIdx, const FString TEAMINFO::*stringPointer )
{
	if ( TEAM_CheckIfValid( ulTeamIdx ))
		return ( (teams[ulTeamIdx].*stringPointer).GetChars() );
	else
		return ( "" );
}

//*****************************************************************************
//
const char *TEAM_SelectCustomStringForPlayer( player_t *pPlayer, const FString TEAMINFO::*stringPointer, const char *pszDefaultString )
{
	if ( pPlayer == NULL )
		return pszDefaultString;

	if ( pPlayer->bOnTeam && TEAM_HasCustomString ( pPlayer->ulTeam, stringPointer ) )
		return TEAM_GetCustomString ( pPlayer->ulTeam, stringPointer );
	else
		return pszDefaultString;
}

//*****************************************************************************
//
const PClass *TEAM_GetItem( ULONG ulTeamIdx )
{
	if ( TEAM_CheckIfValid( ulTeamIdx ))
	{
		if ( GAMEMODE_GetCurrentFlags() & GMF_USEFLAGASTEAMITEM )
			return ( PClass::FindClass( teams[ulTeamIdx].FlagItem.GetChars( )));
		else
			return ( PClass::FindClass( teams[ulTeamIdx].SkullItem.GetChars( )));
	}

	return ( NULL );
}

//*****************************************************************************
//
void TEAM_SetItem( ULONG ulTeamIdx, const PClass *pType, bool bFlag )
{
	if ( TEAM_CheckIfValid( ulTeamIdx ))
	{
		if ( bFlag )
			teams[ulTeamIdx].FlagItem = pType->TypeName.GetChars( );
		else
			teams[ulTeamIdx].SkullItem = pType->TypeName.GetChars( );
	}
}

//*****************************************************************************
//
AInventory *TEAM_FindOpposingTeamsItemInPlayersInventory( player_t *pPlayer )
{
	if ( pPlayer == NULL || pPlayer->mo == NULL )
		return NULL;

	// [BB] A player can't pickup the team item of his team or more than one
	// team item (cf. ATeamItem::AllowFlagPickup),so we don't have to take this
	// into account here.
	for ( ULONG i = 0; i < teams.Size( ); i++ )
	{
		if ( TEAM_ShouldUseTeam( i ) == false )
			continue;

		AInventory* pInventory = pPlayer->mo->FindInventory( TEAM_GetItem( i ) );

		if ( pInventory )
			return pInventory;
	}

	return NULL;
}

//*****************************************************************************
//
player_t *TEAM_GetCarrier( ULONG ulTeamIdx )
{
	if ( TEAM_CheckIfValid( ulTeamIdx ))
		return ( teams[ulTeamIdx].g_pCarrier );

	return ( NULL );
}

//*****************************************************************************
//
void TEAM_SetCarrier( ULONG ulTeamIdx, player_t *player )
{
	if ( TEAM_CheckIfValid( ulTeamIdx ))
		teams[ulTeamIdx].g_pCarrier = player;
}

//*****************************************************************************
//
bool TEAM_GetAnnouncedLeadState( ULONG ulTeamIdx )
{
	if ( TEAM_CheckIfValid( ulTeamIdx ))
		return teams[ulTeamIdx].bAnnouncedLeadState;

	return false;
}

//*****************************************************************************
//
void TEAM_SetAnnouncedLeadState( ULONG ulTeamIdx, bool bAnnouncedLeadState )
{
	if ( TEAM_CheckIfValid( ulTeamIdx ))
		teams[ulTeamIdx].bAnnouncedLeadState = bAnnouncedLeadState;
}

//*****************************************************************************
//
ULONG TEAM_GetReturnTicks( ULONG ulTeamIdx )
{
	if ( TEAM_CheckIfValid( ulTeamIdx ))
		return ( teams[ulTeamIdx].ulReturnTicks );
	else if ( ulTeamIdx == teams.Size( ) )
		return ( g_ulWhiteFlagReturnTicks );
	else
		return ( 0 );
}

//*****************************************************************************
//
void TEAM_SetReturnTicks( ULONG ulTeamIdx, ULONG ulTicks )
{
	if ( TEAM_CheckIfValid( ulTeamIdx ) )
		teams[ulTeamIdx].ulReturnTicks = ulTicks;
	else
		g_ulWhiteFlagReturnTicks = ulTicks;

	// If we're the server, tell clients to update their team return ticks.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetTeamReturnTicks( ulTeamIdx, ( ulTeamIdx == teams.Size( ) ) ? g_ulWhiteFlagReturnTicks : teams[ulTeamIdx].ulReturnTicks );
}

//*****************************************************************************
//
LONG TEAM_GetFragCount( ULONG ulTeamIdx )
{
	if ( TEAM_CheckIfValid( ulTeamIdx ))
		return ( teams[ulTeamIdx].lFragCount );
	else
		return ( 0 );
}

//*****************************************************************************
//
void TEAM_SetFragCount( ULONG ulTeamIdx, LONG lFragCount, bool bAnnounce )
{
	// Invalid team.
	if ( TEAM_CheckIfValid( ulTeamIdx ) == false )
		return;

	// Potentially play some announcer sounds resulting from this frag ("Teams are tied!"),
	// etc.
	if (( bAnnounce ) &&
		(GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNFRAGS ))
	{
		ANNOUNCER_PlayTeamFragSounds( ulTeamIdx, teams[ulTeamIdx].lFragCount, lFragCount );
	}

	// Set the fragcount.
	teams[ulTeamIdx].lFragCount = lFragCount;

	// If we're the server, let clients know that the score has changed.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVERCOMMANDS_SetTeamFrags( ulTeamIdx, lFragCount, bAnnounce );

		// Also, update the scoreboard.
		SERVERCONSOLE_UpdateScoreboard( );
	}
}

//*****************************************************************************
//
LONG TEAM_GetDeathCount( ULONG ulTeamIdx )
{
	if ( TEAM_CheckIfValid( ulTeamIdx ))
		return ( teams[ulTeamIdx].lDeathCount );
	else
		return ( 0 );
}

//*****************************************************************************
//
void TEAM_SetDeathCount( ULONG ulTeamIdx, LONG lDeathCount )
{
	// Invalid team.
	if ( TEAM_CheckIfValid( ulTeamIdx ) == false )
		return;
	else
		teams[ulTeamIdx].lDeathCount = lDeathCount;
}

//*****************************************************************************
//
LONG TEAM_GetWinCount( ULONG ulTeamIdx )
{
	if ( TEAM_CheckIfValid( ulTeamIdx ))
		return ( teams[ulTeamIdx].lWinCount );
	else
		return ( 0 );
}

//*****************************************************************************
//
void TEAM_SetWinCount( ULONG ulTeamIdx, LONG lWinCount, bool bAnnounce )
{
	LONG	lOldWinCount;

	if ( TEAM_CheckIfValid( ulTeamIdx ) == false )
		return;

	lOldWinCount = teams[ulTeamIdx].lWinCount;
	teams[ulTeamIdx].lWinCount = lWinCount;
	if (( bAnnounce ) && ( teams[ulTeamIdx].lWinCount > lOldWinCount ))
	{
		// Build the message.
		// Whatever the team's name is, is the first part of the message. For example:
		// if the "blue" team has been defined then the message will be "BlueScores".
		// This way we don't have to change every announcer to use a new system. 
		FString name;
		name += TEAM_GetName( ulTeamIdx );
		name += "Scores";

		// Play the announcer sound for scoring.
		ANNOUNCER_PlayEntry( cl_announcer, name.GetChars( ));
	}

	// If we're the server, tell clients about the team score update.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVERCOMMANDS_SetTeamWins( ulTeamIdx, lWinCount, bAnnounce );

		// Also, update the scoreboard.
		SERVERCONSOLE_UpdateScoreboard( );
	}
}

//*****************************************************************************
//
bool TEAM_GetSimpleCTFSTMode( void )
{
	return ( g_bSimpleCTFSTMode );
}

//*****************************************************************************
//
void TEAM_SetSimpleCTFSTMode( bool bSimple )
{
	g_bSimpleCTFSTMode = bSimple;
}

//*****************************************************************************
//
bool TEAM_GetItemTaken( ULONG ulTeamIdx )
{
	return ( teams[ulTeamIdx].g_bTaken );
}

//*****************************************************************************
//
void TEAM_SetItemTaken( ULONG ulTeamIdx, bool bTaken )
{
	teams[ulTeamIdx].g_bTaken = bTaken;
}

//*****************************************************************************
//
bool TEAM_GetWhiteFlagTaken( void )
{
	return ( g_bWhiteFlagTaken );
}

//*****************************************************************************
//
void TEAM_SetWhiteFlagTaken( bool bTaken )
{
	g_bWhiteFlagTaken = bTaken;
}

//*****************************************************************************
//
POS_t TEAM_GetItemOrigin( ULONG ulTeamIdx )
{
	return ( teams[ulTeamIdx].g_Origin );
}

//*****************************************************************************
//
void TEAM_SetTeamItemOrigin( ULONG ulTeamIdx, POS_t Origin )
{
	teams[ulTeamIdx].g_Origin = Origin;
}

//*****************************************************************************
//
POS_t TEAM_GetWhiteFlagOrigin( void )
{
	return ( g_WhiteFlagOrigin );
}

//*****************************************************************************
//
void TEAM_SetWhiteFlagOrigin( POS_t Origin )
{
	g_WhiteFlagOrigin = Origin;
}

//*****************************************************************************
//
ULONG TEAM_GetAssistPlayer( ULONG ulTeamIdx )
{
	if ( TEAM_CheckIfValid( ulTeamIdx ) == false )
		return ( MAXPLAYERS );

	return ( teams[ulTeamIdx].g_ulAssistPlayer );
}

//*****************************************************************************
//
void TEAM_SetAssistPlayer( ULONG ulTeamIdx, ULONG ulPlayer )
{
	if (( TEAM_CheckIfValid( ulTeamIdx ) == false ) || ( ulPlayer > MAXPLAYERS ))
		return;

	teams[ulTeamIdx].g_ulAssistPlayer = ulPlayer;
}

//*****************************************************************************
//
void TEAM_CancelAssistsOfPlayer( ULONG ulPlayer )
{
	if ( ulPlayer >= MAXPLAYERS )
		return;

	for ( ULONG i = 0; i < teams.Size( ); i++ )
	{
		if ( TEAM_GetAssistPlayer( i ) != ulPlayer )
			continue;

		TEAM_SetAssistPlayer( i, MAXPLAYERS );
		break;
	}
}

//*****************************************************************************
//
bool TEAM_CheckAllTeamsHaveEqualFrags( void )
{
	LONG lPointsToCompare = teams[0].lFragCount;

	for ( ULONG i = 0; i < teams.Size( ); i++ )
	{
		if ( TEAM_ShouldUseTeam( i ) == false )
			continue;

		if ( TEAM_GetFragCount( i ) != lPointsToCompare )
			return false;
	}

	return ( true );
}

//*****************************************************************************
//
bool TEAM_CheckAllTeamsHaveEqualWins( void )
{
	LONG lPointsToCompare = teams[0].lWinCount;

	for ( ULONG i = 0; i < teams.Size( ); i++ )
	{
		if ( TEAM_ShouldUseTeam( i ) == false )
			continue;

		if ( TEAM_GetWinCount( i ) != lPointsToCompare )
			return false;
	}

	return ( true );
}

//*****************************************************************************
//
bool TEAM_CheckAllTeamsHaveEqualScores( void )
{
	LONG lPointsToCompare = teams[0].lScore;

	for ( ULONG i = 0; i < teams.Size( ); i++ )
	{
		if ( TEAM_ShouldUseTeam( i ) == false )
			continue;

		if ( TEAM_GetScore( i ) != lPointsToCompare )
			return false;
	}

	return ( true );
}

//*****************************************************************************
//
unsigned int TEAM_GetNumAvailableTeams( void )
{
	return MIN<unsigned int>(teams.Size(), sv_maxteams);
}

//*****************************************************************************
//
unsigned int TEAM_GetNumTeamsWithStarts( void )
{
	ULONG ulTeamsWithStarts = 0;

	for ( ULONG i = 0; i < teams.Size( ); ++i )
	{
		if ( teams[i].TeamStarts.Size( ) > 0 )
			++ulTeamsWithStarts;
	}

	return ulTeamsWithStarts;
}

//*****************************************************************************
//
bool TEAM_ShouldUseTeam( ULONG ulTeam )
{
	if ( TEAM_CheckIfValid( ulTeam ) == false )
		return ( false );

	if ( ( GAMEMODE_GetCurrentFlags() & GMF_TEAMGAME ) && teams[ulTeam].TeamStarts.Size( ) < 1 )
		return ( false );

	return ( true );
}

//*****************************************************************************
//
LONG TEAM_GetHighestFragCount( void )
{
	LONG lFragCount = LONG_MIN;

	for ( ULONG i = 0; i < teams.Size( ); i++ )
	{
		if ( TEAM_CountPlayers( i ) < 1 )
			continue;

		if ( lFragCount < TEAM_GetFragCount( i ))
			lFragCount = TEAM_GetFragCount( i );
	}

	return lFragCount;
}

//*****************************************************************************
//
LONG TEAM_GetHighestWinCount( void )
{
	LONG lWinCount = LONG_MIN;

	for ( ULONG i = 0; i < teams.Size( ); i++ )
	{
		if ( TEAM_CountPlayers( i ) < 1 )
			continue;

		if ( lWinCount < TEAM_GetWinCount( i ))
			lWinCount = TEAM_GetWinCount( i );
	}

	return lWinCount;
}

//*****************************************************************************
//
LONG TEAM_GetHighestScoreCount( void )
{
	LONG lScoreCount = LONG_MIN;

	for ( ULONG i = 0; i < teams.Size( ); i++ )
	{
		if ( teamgame == false && TEAM_CountPlayers( i ) < 1 )
			continue;

		if ( lScoreCount < TEAM_GetScore( i ))
			lScoreCount = TEAM_GetScore( i );
	}

	return lScoreCount;
}

//*****************************************************************************
//
LONG TEAM_GetSpread ( ULONG ulTeam, LONG (*GetCount) ( ULONG ulTeam ) )
{
	bool	bInit = true;
	LONG	lHighestCount = 0;

	if ( TEAM_CheckIfValid( ulTeam ) == false )
		return 0;

	// First, find the highest count that isn't ours.
	for ( ULONG ulIdx = 0; ulIdx < teams.Size( ); ulIdx++ )
	{
		if ( ulTeam == ulIdx )
			continue;

		if ( bInit )
		{
			lHighestCount = (*GetCount) ( ulIdx );
			bInit = false;
		}

		if ( (*GetCount) ( ulIdx ) > lHighestCount )
			lHighestCount = (*GetCount) ( ulIdx );
	}

	// If we're the only team in the game...
	if ( bInit )
		lHighestCount = (*GetCount) ( ulTeam );

	// Finally, simply return the difference.
	return ( (*GetCount) ( ulTeam ) - lHighestCount );
}

//*****************************************************************************
//
LONG TEAM_GetFragCountSpread ( ULONG ulTeam )
{
	return TEAM_GetSpread ( ulTeam, &TEAM_GetFragCount );
}

//*****************************************************************************
//
LONG TEAM_GetWinCountSpread ( ULONG ulTeam )
{
	return TEAM_GetSpread ( ulTeam, &TEAM_GetWinCount );
}

//*****************************************************************************
//
LONG TEAM_GetScoreCountSpread ( ULONG ulTeam )
{
	return TEAM_GetSpread ( ulTeam, &TEAM_GetScore );
}

//*****************************************************************************
//
ULONG TEAM_CountFlags( void )
{
	AFlag *pItem;
	TThinkerIterator<AFlag>	iterator;
	ULONG ulCounted = 0;

	while ( (pItem = iterator.Next( )))
		ulCounted++;

	return ulCounted;
}

//*****************************************************************************
//
ULONG TEAM_CountSkulls( void )
{
	ASkull *pItem;
	TThinkerIterator<ASkull>	iterator;
	ULONG ulCounted = 0;

	while ( (pItem = iterator.Next( )))
		ulCounted++;

	return ulCounted;
}

//*****************************************************************************
//
ULONG TEAM_GetTeamFromItem( AActor *pActor )
{
	for ( ULONG i = 0; i < teams.Size( ); i++ )
	{
		if ( pActor->GetClass( ) == TEAM_GetItem( i ))
			return i;
	}

	return teams.Size( );
}

//*****************************************************************************
//
ULONG TEAM_GetNextTeam( ULONG ulTeamIdx )
{
	ULONG ulNewTeamIdx;

	ulNewTeamIdx = ulTeamIdx + 1;

	if ( TEAM_CheckIfValid( ulNewTeamIdx ) == false )
		ulNewTeamIdx = 0;

	return ( ulNewTeamIdx );
}

//*****************************************************************************
//
bool TEAM_ShouldJoinTeam()
{
	return ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
		&& ((( GAMEMODE_GetCurrentFlags() & GMF_TEAMGAME ) == 0 )
			|| ( TemporaryTeamStarts.Size() == 0 ))
		&& (( dmflags2 & DF2_NO_TEAM_SELECT ) == 0 );
}

//****************************************************************************
//
bool TEAM_IsActorAllowedForPlayer( AActor *pActor, player_t *pPlayer )
{
	// [BB] Safety checks.
	if ( (pActor == NULL) || (pPlayer == NULL) )
		return false;

	// [BB] Allow all actors to players not on a team.
	if ( pPlayer->bOnTeam == false )
		return true;

	// [BB] Check if the actor is allowed for the team the player is on.
	return TEAM_IsActorAllowedForTeam( pActor, pPlayer->ulTeam );
}

//****************************************************************************
//
bool TEAM_IsClassAllowedForPlayer( ULONG ulClass, player_t *pPlayer )
{
	// [BB] Safety checks.
	if ( ulClass >= PlayerClasses.Size() )
		return false;

	return TEAM_IsActorAllowedForPlayer ( GetDefaultByType(PlayerClasses[ulClass].Type), pPlayer );
}

//****************************************************************************
//
bool TEAM_CheckTeamRestriction( ULONG ulTeam, ULONG ulTeamRestriction )
{
	// [BB] No teamgame, so no team restrictions apply.
	if ( !( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS ) )
		return true;

	// [BB] Not restricted to a certain team.
	if ( ulTeamRestriction == 0 )
		return true;

	// [BB] Allow everything to "no team".
	if ( ulTeam == teams.Size( ) )
		return true;

	// [BB] The team is the one specified by ulTeamRestriction.
	if ( ulTeam == (ulTeamRestriction - 1) )
		return true;
	
	return false;
}

//****************************************************************************
//
bool TEAM_IsActorAllowedForTeam( AActor *pActor, ULONG ulTeam )
{
	// [BB] Safety checks.
	if ( pActor == NULL )
		return false;

	return TEAM_CheckTeamRestriction( ulTeam, pActor->ulLimitedToTeam );
}

//****************************************************************************
//
bool TEAM_IsClassAllowedForTeam( ULONG ulClass, ULONG ulTeam )
{
	// [BB] Safety checks.
	if ( ulClass >= PlayerClasses.Size() )
		return false;

	return TEAM_IsActorAllowedForTeam ( GetDefaultByType(PlayerClasses[ulClass].Type), ulTeam );
}

//****************************************************************************
//
ULONG TEAM_FindValidClassForPlayer( player_t *pPlayer )
{
	// [BB] Safety checks.
	if ( pPlayer == NULL )
		return 0;

	for ( ULONG ulIdx = 0; ulIdx < PlayerClasses.Size(); ++ulIdx )
	{
		if ( TEAM_IsClassAllowedForPlayer ( ulIdx, pPlayer ) )
		{
			return ulIdx;
		}
	}

	return 0;
}

//****************************************************************************
//
void TEAM_EnsurePlayerHasValidClass( player_t *pPlayer )
{
	// [BB] This is server side.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	// [BB] Safety checks.
	if ( pPlayer == NULL )
		return;

	// [BB] The random class is available to all players, P_SpawnPlayer needs to take 
	// care of only selecting valid random classes.
	if ( pPlayer->userinfo.GetPlayerClassNum() == -1 )
		return;

	// [BB] The additional checks prevent this from being done when there is only one class and while the map is loaded.
	const bool forcerandom = ( sv_forcerandomclass && (PlayerClasses.Size () > 1) && ( gameaction == ga_nothing ) && PLAYER_IsValidPlayer ( static_cast<ULONG> ( pPlayer - players ) ) );

	// [BB] The class is valid, nothing to do.
	if ( TEAM_IsClassAllowedForPlayer ( pPlayer->userinfo.GetPlayerClassNum(), pPlayer ) && ( ( forcerandom == false ) || ( pPlayer->userinfo.GetPlayerClassNum() == -1 ) ) )
		return;

	// [BB] The current class is invalid, select a valid one.
	pPlayer->userinfo.PlayerClassNumChanged ( forcerandom ? -1 : TEAM_FindValidClassForPlayer ( pPlayer ) );
	// [BB] This should respawn the player at the appropriate spot. Set the player state to
	// PST_REBORNNOINVENTORY so everything (weapons, etc.) is cleared.
	pPlayer->playerstate = PST_REBORNNOINVENTORY;
}

//****************************************************************************
//
LONG TEAM_SelectRandomValidPlayerClass( ULONG ulTeam )
{
	if ( TEAM_CheckIfValid ( ulTeam ) == false )
		return ( M_Random() % PlayerClasses.Size () );

	// [BB] It's pretty inefficient to rebuild the list of available classes
	// every time when calling this function, but since it's not called to
	// often and the number of player classes is usually rather small, I
	// don't care about this for now.
	TArray<ULONG> _availablePlayerClasses;
	for ( ULONG ulIdx = 0; ulIdx < PlayerClasses.Size(); ++ulIdx )
	{
		if ( TEAM_IsClassAllowedForTeam( ulIdx, ulTeam ) )
			_availablePlayerClasses.Push ( ulIdx );
	}

	if ( _availablePlayerClasses.Size () == 0 )
		return 0;

	return _availablePlayerClasses[ M_Random() % _availablePlayerClasses.Size () ];
}

//****************************************************************************
//
bool TEAM_IsActorVisibleToPlayer( const AActor *pActor, player_t *pPlayer )
{
	// [BB] Safety checks.
	if ( pActor == NULL )
		return false;

	// [BB] Non-player actors are allowed to see everything. 
	if ( pPlayer == NULL )
		return true;

	// [BB] Allow all actors to be seen by players not on a team.
	if ( pPlayer->bOnTeam == false )
		return true;

	// [BB] Finally check the team restricion.
	return TEAM_CheckTeamRestriction( pPlayer->ulTeam, pActor->VisibleToTeam );
}

//*****************************************************************************
// [Dusk]
int TEAM_GetPlayerStartThingNum( ULONG ulTeam ) {
	if ( !TEAM_CheckIfValid( ulTeam ))
		return 0;
	return static_cast<int>( teams[ulTeam].ulPlayerStartThingNumber );
}

//*****************************************************************************
// [Dusk]
const char* TEAM_GetTeamItemName( ULONG ulTeam ) {
	if ( !TEAM_CheckIfValid( ulTeam ) )
		return NULL;
	if ( GAMEMODE_GetCurrentFlags( ) & GMF_USEFLAGASTEAMITEM )
		return teams[ulTeam].FlagItem;
	else
		return teams[ulTeam].SkullItem;
}

//*****************************************************************************
// [Dusk]
const char* TEAM_GetIntermissionTheme( ULONG ulTeam, bool bWin ) {
	if ( !TEAM_CheckIfValid( ulTeam ) )
		return NULL;
	if ( bWin )
		return teams[ulTeam].WinnerTheme;
	else
		return teams[ulTeam].LoserTheme;
}

//*****************************************************************************
//	CONSOLE COMMANDS/VARIABLES

CUSTOM_CVAR( Bool, teamgame, false, CVAR_SERVERINFO | CVAR_LATCH | CVAR_CAMPAIGNLOCK )
{
	UCVarValue	Val;

	if ( self == true )
	{
		Val.Bool = false;

		// Disable deathmatch if we're setting a teamgame.
		deathmatch.ForceSet( Val, CVAR_Bool );

		// Disable cooperative if we're setting a teamgame.
		cooperative.ForceSet( Val, CVAR_Bool );
	}
	else
	{
		Val.Bool = false;
		// Teamgame has been disabled, so disable all related modes.
		ctf.ForceSet( Val, CVAR_Bool );
		oneflagctf.ForceSet( Val, CVAR_Bool );
		skulltag.ForceSet( Val, CVAR_Bool );
		domination.ForceSet( Val, CVAR_Bool );

		// If deathmatch is also disabled, enable cooperative mode.
		if ( deathmatch == false )
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
CUSTOM_CVAR( Bool, ctf, false, CVAR_SERVERINFO | CVAR_LATCH | CVAR_CAMPAIGNLOCK )
{
	UCVarValue	Val;

	if ( self == true )
	{
		Val.Bool = true;
		// Enable teamgame if we're playing CTF.
		teamgame.ForceSet( Val, CVAR_Bool );

		Val.Bool = false;
		// Disable other modes.
		oneflagctf.ForceSet( Val, CVAR_Bool );
		skulltag.ForceSet( Val, CVAR_Bool );
		domination.ForceSet( Val, CVAR_Bool );
	}

	// Reset what the current game mode is.
	GAMEMODE_DetermineGameMode( );
}

//*****************************************************************************
//
CUSTOM_CVAR( Bool, oneflagctf, false, CVAR_SERVERINFO | CVAR_LATCH | CVAR_CAMPAIGNLOCK )
{
	UCVarValue	Val;

	if ( self == true )
	{
		Val.Bool = true;
		// Enable teamgame if we're playing CTF.
		teamgame.ForceSet( Val, CVAR_Bool );

		Val.Bool = false;
		// Disable other modes.
		ctf.ForceSet( Val, CVAR_Bool );
		skulltag.ForceSet( Val, CVAR_Bool );
		domination.ForceSet( Val, CVAR_Bool );
	}

	// Reset what the current game mode is.
	GAMEMODE_DetermineGameMode( );
}

//*****************************************************************************
//
CUSTOM_CVAR( Bool, skulltag, false, CVAR_SERVERINFO | CVAR_LATCH | CVAR_CAMPAIGNLOCK )
{
	UCVarValue	Val;

	if ( self == true )
	{
		Val.Bool = true;
		// Enable teamgame if we're playing CTF.
		teamgame.ForceSet( Val, CVAR_Bool );

		Val.Bool = false;
		// Disable other modes.
		ctf.ForceSet( Val, CVAR_Bool );
		oneflagctf.ForceSet( Val, CVAR_Bool );
		domination.ForceSet( Val, CVAR_Bool );
	}

	// Reset what the current game mode is.
	GAMEMODE_DetermineGameMode( );
}

//*****************************************************************************
//
CUSTOM_CVAR( Bool, domination, false, CVAR_SERVERINFO | CVAR_LATCH | CVAR_CAMPAIGNLOCK )
{
	UCVarValue	Val;

	if ( self == true )
	{
		Val.Bool = true;
		// Enable teamgame if we're playing CTF.
		teamgame.ForceSet( Val, CVAR_Bool );

		Val.Bool = false;
		// Disable other modes.
		ctf.ForceSet( Val, CVAR_Bool );
		oneflagctf.ForceSet( Val, CVAR_Bool );
		skulltag.ForceSet( Val, CVAR_Bool );
	}

	// Reset what the current game mode is.
	GAMEMODE_DetermineGameMode( );
}

//*****************************************************************************
//
CCMD( team )
{
	// Not a valid team mode. Ignore.
	if ( !( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS ) )
		return;

	// If the played inputted a team they'd like to join (such as, "team red"), handle that
	// with the changeteam command.
	if ( argv.argc( ) > 1 )
	{
		char	szCommand[64];

		sprintf( szCommand, "changeteam \"%s\"", argv[1] );
		AddCommandString( szCommand );
	}
	// If they didn't, just display which team they're on.
	else
	{
		if ( players[consoleplayer].bOnTeam )
		{
			Printf( "You are on the \\c%c%s \\c-team.\n", V_GetColorChar( TEAM_GetTextColor( players[consoleplayer].ulTeam ) ), TEAM_GetName( players[consoleplayer].ulTeam ) );
		}
		else
			Printf( "You are not currently on a team.\n" );
	}
}

//*****************************************************************************
//
CCMD( changeteam )
{
	LONG	lDesiredTeam;

	// [BB] No joining while playing a demo.
	if ( CLIENTDEMO_IsPlaying( ) == true )
	{
		Printf ( "You can't join or change your team during demo playback.\n" );
		return;
	}

	// The server can't do this!
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	if ( playeringame[consoleplayer] == false )
		return;

	// Not in a level.
	// [BB] Still allow spectators to join the queue (that's what happens if you try to join during intermission).
	if ( ( gamestate != GS_LEVEL ) && ( ( players[consoleplayer].bSpectating == false ) || ( gamestate != GS_INTERMISSION ) ) )
	{
		Printf( "You can only change your team while in a level.\n" );
		return;
	}

	// Not a team mode.
	if (( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS ) == false )
	{
		Printf( "You can only change your team in a team game.\n" );
		return;
	}

	// [WS] "No team select" dmflag is set. Ignore this.
	if ( players[consoleplayer].bSpectating && ( dmflags2 & DF2_NO_TEAM_SELECT ) )
	{
		Printf( "You are not allowed to choose your team!\n" );
		return;
	}

	// "No team change" dmflag is set. Ignore this.
	if (( players[consoleplayer].bOnTeam ) &&
		( dmflags2 & DF2_NO_TEAM_SWITCH ))
	{
		Printf( "You are not allowed to change your team!\n" );
		return;
	}

	// Can't change teams in a campaign!
	if ( CAMPAIGN_InCampaign( ))
	{
		CAMPAIGNINFO_s *pInfo = CAMPAIGN_GetCampaignInfo( level.mapname );

		// [BB] Allow the player to join the team, he is supposed to have.
		if ( players[consoleplayer].bOnTeam
		     || ( pInfo == NULL )
		     || ( stricmp( pInfo->PlayerTeamName.GetChars(), argv[1] ) != 0 ) )
		{
			Printf( "You cannot change teams in the middle of a campaign!\n" );
			return;
		}
	}

	lDesiredTeam = teams.Size( );

	// If we're a client, just send out the "change team" message.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		UCVarValue	Val;

		if ( argv.argc( ) > 1 )
		{
			if ( stricmp( argv[1], "random" ) == 0 )
			{
				while( true )
				{
					lDesiredTeam = g_JoinTeamSeed.Random( ) % teams.Size( );

					if ( TEAM_CheckIfValid( lDesiredTeam ) && TEAM_ShouldUseTeam( lDesiredTeam ))
						break;
				}
			}
			else if ( stricmp( argv[1], "autoselect" ) == 0 )
				lDesiredTeam = teams.Size( );
			else
			{
				ULONG	ulIdx;

				// If the arg we passed in matches the team index or the team name, set that
				// team as the desired team.
				for ( ulIdx = 0; ulIdx < teams.Size( ); ulIdx++ )
				{
					if (( (ULONG)atoi( argv[1] ) == ulIdx ) || ( stricmp( argv[1], TEAM_GetName( ulIdx )) == 0 ))
						lDesiredTeam = ulIdx;
				}

				if ( !TEAM_ShouldUseTeam(lDesiredTeam) )
					return;
			}
		}
		// We did not pass in a team, so we must want to toggle our team.
		else if ( (ULONG)lDesiredTeam == teams.Size( ) )
		{
			// Can't toggle our teams if we're not on a team!
			if ( players[consoleplayer].bOnTeam == false )
			{
				Printf( "You can only toggle your team if you are on a team.\n" );
				return;
			}

			lDesiredTeam = TEAM_GetNextTeam( players[consoleplayer].ulTeam );
		}

		Val = cl_joinpassword.GetGenericRep( CVAR_String );
		CLIENTCOMMANDS_ChangeTeam( Val.String, lDesiredTeam );
		return;
	}

	if ( argv.argc( ) > 1 )
	{
		if ( stricmp( argv[1], "random" ) == 0 )
		{
			while( true )
			{
				lDesiredTeam = g_JoinTeamSeed.Random( ) % teams.Size( );

				if ( TEAM_CheckIfValid( lDesiredTeam ) && TEAM_ShouldUseTeam( lDesiredTeam ))
					break;
			}
		}
		else if ( stricmp( argv[1], "autoselect" ) == 0 )
			lDesiredTeam = TEAM_ChooseBestTeamForPlayer( );
		else
		{
			ULONG	ulIdx;

			// If the arg we passed in matches the team index or the team name, set that
			// team as the desired team.
			for ( ulIdx = 0; ulIdx < teams.Size( ); ulIdx++ )
			{
				if (( (ULONG)atoi( argv[1] ) == ulIdx ) || ( stricmp( argv[1], TEAM_GetName( ulIdx )) == 0 ))
					lDesiredTeam = ulIdx;
			}

			if ( !TEAM_ShouldUseTeam(lDesiredTeam) )
				return;
		}
	}
	// We did not pass in a team, so we must want to toggle our team.
	else
	{
		// Can't toggle our teams if we're not on a team!
		if ( players[consoleplayer].bOnTeam == false )
			return;

		lDesiredTeam = TEAM_GetNextTeam( players[consoleplayer].ulTeam );
	}

	// If the desired team matches our current team, break out.
	// [BB] Don't break out if we are a spectator. In this case we don't change our team, but we want to join.
	if (( players[consoleplayer].bOnTeam ) && ( lDesiredTeam == static_cast<signed> (players[consoleplayer].ulTeam) )
		&& !(players[consoleplayer].bSpectating) )
	{
		return;
	}

	// [BB] If players aren't allowed to join at the moment, just put the consoleplayer in line.
	if ( GAMEMODE_PreventPlayersFromJoining( consoleplayer ) )
	{
		JOINQUEUE_AddConsolePlayer ( lDesiredTeam );
		return;
	}

	{
		bool	bOnTeam;

		// Don't allow him to bring flags, skulls, terminators, etc. with him.
		if ( players[consoleplayer].mo )
			players[consoleplayer].mo->DropImportantItems( false );

		// [BB] Morphed players need to be unmorphed before changing teams.
		if ( players[consoleplayer].morphTics )
			P_UndoPlayerMorph ( &players[consoleplayer], &players[consoleplayer] );

		// Save this. This will determine our message.
		bOnTeam = players[consoleplayer].bOnTeam;

		// Set the new team.
		PLAYER_SetTeam( &players[consoleplayer], lDesiredTeam, true );

		// Player was on a team, so tell everyone that he's changing teams.
		if ( bOnTeam )
		{
			Printf( "%s \\c-defected to the \\c%c%s \\c-team.\n", players[consoleplayer].userinfo.GetName(), V_GetColorChar( TEAM_GetTextColor( players[consoleplayer].ulTeam ) ), TEAM_GetName( players[consoleplayer].ulTeam ));
		}
		// Otherwise, tell everyone he's joining a team.
		else
		{
			Printf( "%s \\c-joined the \\c%c%s \\c-team.\n", players[consoleplayer].userinfo.GetName(), V_GetColorChar( TEAM_GetTextColor( players[consoleplayer].ulTeam ) ), TEAM_GetName( players[consoleplayer].ulTeam ));
		}

		if ( players[consoleplayer].mo )
		{
			players[consoleplayer].mo->Destroy( );
			players[consoleplayer].mo = NULL;
		}

		// Now respawn the player at the appropriate spot. Set the player state to
		// PST_REBORNNOINVENTORY so everything (weapons, etc.) is cleared.
		// [BB/WS] If the player was a spectator, we have to set the state to
		// PST_ENTERNOINVENTORY. Otherwise the enter scripts are not executed.
		if ( players[consoleplayer].bSpectating )
			players[consoleplayer].playerstate = PST_ENTERNOINVENTORY;
		else
			players[consoleplayer].playerstate = PST_REBORNNOINVENTORY;

		// Also, take away spectator status.
		players[consoleplayer].bSpectating = false;
		players[consoleplayer].bDeadSpectator = false;

		if ( GAMEMODE_GetCurrentFlags() & GMF_TEAMGAME )
			G_TeamgameSpawnPlayer( consoleplayer, players[consoleplayer].ulTeam, true );
		else
			G_DeathMatchSpawnPlayer( consoleplayer, true );
	}
}

//*****************************************************************************
//*****************************************************************************
//
CUSTOM_CVAR( Int, pointlimit, 0, CVAR_SERVERINFO | CVAR_CAMPAIGNLOCK )
{
	if ( self >= 65536 )
		self = 65535;
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

// Allow the server to set the return time for flags/skulls.
CVAR( Int, sv_flagreturntime, 15, CVAR_CAMPAIGNLOCK );

CUSTOM_CVAR( Int, sv_maxteams, 2, CVAR_SERVERINFO | CVAR_CAMPAIGNLOCK | CVAR_LATCH )
{
	// [BB] We didn't initialize TEAMINFO yet, so we can't use teams.Size() to clamp sv_maxteams right now.
	// In order to ensure that sv_maxteams stays in its limits, we have to clamp it in TEAMINFO_Init.
	if ( teams.Size() == 0 )
		return;

	int value = clamp<int>(self, 2, teams.Size());
	if(value != self)
		self = value;
}
