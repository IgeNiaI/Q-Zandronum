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
// Filename: scoreboard.cpp
//
// Description: Contains scoreboard routines and globals
//
//-----------------------------------------------------------------------------

#include "a_pickups.h"
#include "c_dispatch.h"
#include "callvote.h"
#include "chat.h"
#include "cl_demo.h"
#include "cooperative.h"
#include "deathmatch.h"
#include "duel.h"
#include "doomtype.h"
#include "d_player.h"
#include "g_game.h"
#include "gamemode.h"
#include "gi.h"
#include "invasion.h"
#include "joinqueue.h"
#include "lastmanstanding.h"
#include "network.h"
#include "possession.h"
#include "sbar.h"
#include "scoreboard.h"
#include "st_stuff.h"
#include "survival.h"
#include "team.h"
#include "templates.h"
#include "v_text.h"
#include "v_video.h"
#include "w_wad.h"
#include "c_bind.h"	// [RC] To tell user what key to press to vote.
#include "domination.h"
#include "st_hud.h"

//*****************************************************************************
//	VARIABLES

// Player list according to rank.
static	int		g_iSortedPlayers[MAXPLAYERS];

// How many players are currently in the game?
static	ULONG	g_ulNumPlayers = 0;

// What is our current rank?
static	ULONG	g_ulRank = 0;

// What is the spread between us and the person in 1st/2nd?
static	LONG	g_lSpread = 0;

// Is this player tied with another?
static	bool	g_bIsTied = false;

// How many opponents are left standing in LMS?
static	LONG	g_lNumOpponentsLeft = 0;

// [RC] How many allies are alive in Survival, or Team LMS?
static	LONG	g_lNumAlliesLeft = 0;

// Who has the terminator artifact?
static	player_t	*g_pTerminatorArtifactCarrier = NULL;

// Who has the possession artifact?
static	player_t	*g_pPossessionArtifactCarrier = NULL;

// Who is carrying the white flag?
static	player_t	*g_pWhiteCarrier = NULL;

// Centered text that displays in the bottom of the screen.
static	FString		g_BottomString;

// Current position of our "pen".
static	ULONG		g_ulCurYPos;

// Is text scaling enabled?
static	bool		g_bScale;

// What are the virtual dimensions of our screen?
static	UCVarValue	g_ValWidth;
static	UCVarValue	g_ValHeight;

// How much bigger is the virtual screen than the base 320x200 screen?
static	float		g_fXScale;
static	float		g_fYScale;

// How much bigger or smaller is the virtual screen vs. the actual resolution?
static	float		g_rXScale;
static	float		g_rYScale;

// How tall is the smallfont, scaling considered?
static	ULONG		g_ulTextHeight;

// How many columns are we using in our scoreboard display?
static	ULONG		g_ulNumColumnsUsed = 0;

// Array that has the type of each column.
static	ULONG		g_aulColumnType[MAX_COLUMNS];

// X position of each column.
static	ULONG		g_aulColumnX[MAX_COLUMNS];

// What font are the column headers using?
static	FFont		*g_pColumnHeaderFont = NULL;

// This is the header for each column type.
static	const char	*g_pszColumnHeaders[NUM_COLUMN_TYPES] =
{
	"",
	"NAME",
	"TIME",
	"PING",
	"FRAGS",
	"POINTS",
	"DEATHS",
	"WINS",
	"KILLS",
	"SCORE",
	"SECRETS",
	"MEDALS",
};

//*****************************************************************************
//	PROTOTYPES

static	void			scoreboard_SortPlayers( ULONG ulSortType );
static	int	STACK_ARGS	scoreboard_FragCompareFunc( const void *arg1, const void *arg2 );
static	int	STACK_ARGS	scoreboard_PointsCompareFunc( const void *arg1, const void *arg2 );
static	int	STACK_ARGS	scoreboard_KillsCompareFunc( const void *arg1, const void *arg2 );
static	int	STACK_ARGS	scoreboard_WinsCompareFunc( const void *arg1, const void *arg2 );
static	void			scoreboard_RenderIndividualPlayer( ULONG ulDisplayPlayer, ULONG ulPlayer );
static	void			scoreboard_DrawHeader( void );
static	void			scoreboard_DrawLimits( void );
static	void			scoreboard_DrawTeamScores( ULONG ulPlayer );
static	void			scoreboard_DrawMyRank( ULONG ulPlayer );
static	void			scoreboard_ClearColumns( void );
static	void			scoreboard_Prepare5ColumnDisplay( void );
static	void			scoreboard_Prepare4ColumnDisplay( void );
static	void			scoreboard_Prepare3ColumnDisplay( void );
static	void			scoreboard_DoRankingListPass( ULONG ulPlayer, LONG lSpectators, LONG lDead, LONG lNotPlaying, LONG lNoTeam, LONG lWrongTeam, ULONG ulDesiredTeam );
static	void			scoreboard_DrawRankings( ULONG ulPlayer );
static	void			scoreboard_DrawWaiting( void );
static	void			scoreboard_DrawBottomString( void );

//*****************************************************************************
//	CONSOLE VARIABLES

CVAR (Bool, r_drawspectatingstring, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG );
EXTERN_CVAR( Bool, cl_stfullscreenhud );
EXTERN_CVAR( Int, screenblocks );
EXTERN_CVAR( Bool, st_scale );


//*****************************************************************************
//	FUNCTIONS

static void SCOREBOARD_DrawBottomString( void )
{
	// [RC] Draw the centered bottom message (spectating, following, waiting, etc).
	if ( g_BottomString.Len( ) > 0 )
	{
		DHUDMessageFadeOut	*pMsg;
		V_ColorizeString( g_BottomString );

		pMsg = new DHUDMessageFadeOut( SmallFont, g_BottomString,
			1.5f,
			1.0f,
			0,
			0,
			CR_WHITE,
			0.10f,
			0.15f );

		StatusBar->AttachMessage( pMsg, MAKE_ID('W','A','I','T') );
	}
}

//*****************************************************************************
//
static void SCOREBOARD_DrawWaiting( void )
{
	// [RC] Formatting linebreak.
	if ( static_cast<int>( SCOREBOARD_GetViewPlayer() ) != consoleplayer )
		g_BottomString += "\n";
	
	g_BottomString += "\\cgWAITING FOR PLAYERS";
	SCOREBOARD_DrawBottomString();
}

//*****************************************************************************
// Checks if the user wants to see the scoreboard and is allowed to.
//
bool SCOREBOARD_ShouldDrawBoard( ULONG ulDisplayPlayer )
{
	if(
		(( NETWORK_GetState( ) != NETSTATE_SINGLE ) || ( deathmatch || teamgame || invasion ) || CLIENTDEMO_IsPlaying( )) &&
		( Button_ShowScores.bDown || (( players[ulDisplayPlayer].camera && players[ulDisplayPlayer].camera->health <= 0 ) && (( lastmanstanding || teamlms ) && (( LASTMANSTANDING_GetState( ) == LMSS_COUNTDOWN ) || ( LASTMANSTANDING_GetState( ) == LMSS_WAITINGFORPLAYERS )))  && ( teamlms == false ) && ( duel == false || ( DUEL_GetState( ) != DS_WINSEQUENCE ))))
		)
		return true;
	else
		return false;
}

//*****************************************************************************
// Returns either consoleplayer, or (if using F12), the player we're spying.
//
ULONG SCOREBOARD_GetViewPlayer( void )
{
	if (( players[consoleplayer].camera ) && ( players[consoleplayer].camera != players[consoleplayer].mo ) && ( players[consoleplayer].camera->player ))
	{
		return (ULONG) (players[consoleplayer].camera->player - players);
	}
	return consoleplayer;
}

//*****************************************************************************
// Renders some HUD strings, and the main board if the player is pushing the keys.
//
void SCOREBOARD_Render( ULONG ulDisplayPlayer )
{
	LONG				lPosition;

	// Make sure the display player is valid.
	if ( ulDisplayPlayer >= MAXPLAYERS )
		return;

	// Initialization for text scaling.
	g_ValWidth	= con_virtualwidth.GetGenericRep( CVAR_Int );
	g_ValHeight	= con_virtualheight.GetGenericRep( CVAR_Int );

	if (( con_scaletext ) && ( con_virtualwidth > 0 ) && ( con_virtualheight > 0 ))
	{
		g_bScale		= true;
		g_fXScale		= (float)g_ValWidth.Int / 320.0f;
		g_fYScale		= (float)g_ValHeight.Int / 200.0f;
		g_rXScale		= (float)g_ValWidth.Int / SCREENWIDTH;
		g_rYScale		= (float)g_ValHeight.Int / SCREENHEIGHT;
		g_ulTextHeight	= Scale( SCREENHEIGHT, SmallFont->GetHeight( ) + 1, con_virtualheight );
	}
	else
	{
		g_bScale		= false;
		g_rXScale		= 1;
		g_rYScale		= 1;
		g_ulTextHeight	= SmallFont->GetHeight( ) + 1;
	}

	// Draw the main scoreboard.
	if (SCOREBOARD_ShouldDrawBoard( ulDisplayPlayer ))
		SCOREBOARD_RenderBoard( ulDisplayPlayer );

	g_BottomString = "";

	int viewplayer = static_cast<int>( SCOREBOARD_GetViewPlayer() );
	// [BB] Draw a message to show that the free spectate mode is active.
	if ( CLIENTDEMO_IsInFreeSpectateMode() )
		g_BottomString.AppendFormat( "FREE SPECTATE MODE" );
	// If the console player is looking through someone else's eyes, draw the following message.
	else if ( viewplayer != consoleplayer )
	{
		char cColor = V_GetColorChar( CR_RED );

		// [RC] Or draw this in their team's color.
		if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
			 cColor = V_GetColorChar( TEAM_GetTextColor( players[viewplayer].ulTeam ) );

		g_BottomString.AppendFormat( "\\c%cFOLLOWING - %s\\c%c", cColor, players[viewplayer].userinfo.GetName(), cColor );
	}

	// Print the totals for living and dead allies/enemies.
	if (( players[ulDisplayPlayer].bSpectating == false ) && ( GAMEMODE_GetCurrentFlags() & GMF_DEADSPECTATORS ) && ( GAMEMODE_GetState() == GAMESTATE_INPROGRESS ))
	{
		// Survival, Survival Invasion, etc
		if ( GAMEMODE_GetCurrentFlags() & GMF_COOPERATIVE )
		{
			if ( viewplayer != consoleplayer )
				g_BottomString.AppendFormat(" - ");

			if(g_lNumAlliesLeft < 1)
				g_BottomString += "\\cgLAST PLAYER ALIVE"; // Uh-oh.
			else {
				g_BottomString.AppendFormat( "\\cc%d ", static_cast<int> (g_lNumAlliesLeft) );
				g_BottomString.AppendFormat( "\\cGALL%s LEFT", ( g_lNumAlliesLeft != 1 ) ? "IES" : "Y" );
			}
		}

		// Last Man Standing, TLMS, etc
		if ( GAMEMODE_GetCurrentFlags() & GMF_DEATHMATCH )
		{
			if ( viewplayer != consoleplayer )
				g_BottomString.AppendFormat(" - ");

			if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
			{
				g_BottomString += "\\cC";
				g_BottomString.AppendFormat( "%d ", static_cast<int> (g_lNumOpponentsLeft) );
				g_BottomString.AppendFormat( "\\cGOPPONENT%s", ( g_lNumOpponentsLeft != 1 ) ? "s" : "" );
				g_BottomString += "\\cC";					
				if(g_lNumAlliesLeft > 0)
				{
					g_BottomString.AppendFormat( ", %d ", static_cast<int> (g_lNumAlliesLeft) );
					g_BottomString.AppendFormat( "\\cGALL%s LEFT ", ( g_lNumAlliesLeft != 1 ) ? "IES" : "Y" );
				}
				else
					g_BottomString += "\\cG LEFT - ALLIES DEAD";
			}
			else
			{
				g_BottomString += "\\cC";
				g_BottomString.AppendFormat( "%d ", static_cast<int> (g_lNumOpponentsLeft) );
				g_BottomString.AppendFormat( "\\cGOPPONENT%s LEFT", ( g_lNumOpponentsLeft != 1 ) ? "S" : "" );
			}
		}
	}

	// If the console player is spectating, draw the spectator message.
	// [BB] Only when not in free spectate mode.
	if (( players[consoleplayer].bSpectating ) && r_drawspectatingstring && !CLIENTDEMO_IsInFreeSpectateMode())
	{
		g_BottomString += "\n";
		lPosition = JOINQUEUE_GetPositionInLine( consoleplayer );
		if ( players[consoleplayer].bDeadSpectator )
			g_BottomString += "\\cdSPECTATING - WAITING TO RESPAWN";
		else if ( lPosition != -1 )
		{
			g_BottomString += "\\cdWAITING TO PLAY - ";
			FString ordinal = SCOREBOARD_SpellOrdinal( lPosition );
			ordinal.ToUpper();
			g_BottomString += ordinal;
			g_BottomString += " IN LINE";
		}
		else
		{
			int key1 = 0;
			int key2 = 0;
			Bindings.GetKeysForCommand( "menu_join", &key1, &key2 );
			g_BottomString += "\\cdSPECTATING - PRESS \'";

			if ( key2 )
				g_BottomString = g_BottomString + KeyNames[key1] + "\' OR \'" + KeyNames[key2];
			else if ( key1 )
				g_BottomString += KeyNames[key1];
			else
				g_BottomString += G_DescribeJoinMenuKey();

			g_BottomString += "\' TO JOIN";
		}
	}

	if ( CALLVOTE_ShouldShowVoteScreen( ))
	{		
		// [RC] Display either the fullscreen or minimized vote screen.
		if ( cl_showfullscreenvote )
			SCOREBOARD_RenderInVoteClassic( );
		else
			SCOREBOARD_RenderInVote( );
	}

	if ( duel )
	{
		// Determine what to draw based on the duel state.
		switch ( DUEL_GetState( ))
		{
		case DS_COUNTDOWN:

			// Render "x vs. x" text.
			SCOREBOARD_RenderDuelCountdown( DUEL_GetCountdownTicks( ) + TICRATE );
			break;
		case DS_WAITINGFORPLAYERS:

			if ( players[ulDisplayPlayer].bSpectating == false )
			{
				SCOREBOARD_DrawWaiting();
				// Nothing more to do if we're just waiting for players.
				return;
			}
			break;
		default:
			break;
		}
	}

	if ( lastmanstanding || teamlms )
	{
		// Determine what to draw based on the duel state.
		switch ( LASTMANSTANDING_GetState( ))
		{
		case LMSS_COUNTDOWN:

			// Render title text.
			SCOREBOARD_RenderLMSCountdown( LASTMANSTANDING_GetCountdownTicks( ) + TICRATE );
			break;
		case LMSS_WAITINGFORPLAYERS:

			if ( players[ulDisplayPlayer].bSpectating == false )
			{
				SCOREBOARD_DrawWaiting();				
				// Nothing more to do if we're just waiting for players.
				return;
			}
			break;
		default:
			break;
		}
	}

	if ( possession || teampossession )
	{
		// Determine what to draw based on the duel state.
		switch ( POSSESSION_GetState( ))
		{
		case PSNS_COUNTDOWN:
		case PSNS_NEXTROUNDCOUNTDOWN:

			// Render title text.
			SCOREBOARD_RenderPossessionCountdown(( POSSESSION_GetState( ) == PSNS_COUNTDOWN ) ? (( possession ) ? "POSSESSION" : "TEAM POSSESSION" ) : "NEXT ROUND IN...", POSSESSION_GetCountdownTicks( ) + TICRATE );
			break;
		case PSNS_WAITINGFORPLAYERS:

			if ( players[ulDisplayPlayer].bSpectating == false )
			{
				g_BottomString += "\\cgWAITING FOR PLAYERS";
			}
			break;
		default:
			break;
		}
	}

	if ( survival )
	{
		// Determine what to draw based on the survival state.
		switch ( SURVIVAL_GetState( ))
		{
		case SURVS_COUNTDOWN:

			// Render title text.
			SCOREBOARD_RenderSurvivalCountdown( SURVIVAL_GetCountdownTicks( ) + TICRATE );
			break;
		case SURVS_WAITINGFORPLAYERS:

			if ( players[ulDisplayPlayer].bSpectating == false )
			{
				SCOREBOARD_DrawWaiting();
				// Nothing more to do if we're just waiting for players.
				return;
			}
			break;
		default:
			break;
		}
	}

	if ( invasion )
	{
		// Determine what to draw based on the invasion state.
		switch ( INVASION_GetState( ))
		{
		case IS_FIRSTCOUNTDOWN:

			// Render title text.
			SCOREBOARD_RenderInvasionFirstCountdown( INVASION_GetCountdownTicks( ) + TICRATE );
			break;
		case IS_COUNTDOWN:

			// Render title text.
			SCOREBOARD_RenderInvasionCountdown( INVASION_GetCountdownTicks( ) + TICRATE );
			break;
		case IS_INPROGRESS:
		case IS_BOSSFIGHT:

			// Render the number of monsters left, etc.
			SCOREBOARD_RenderInvasionStats( );
			break;
		default:
			break;
		}
	}
	
	if( SCOREBOARD_IsHudVisible() )
	{
		// Draw the item holders (hellstone, flags, skulls, etc).
		SCOREBOARD_RenderStats_Holders( );

		if( ( SCOREBOARD_IsUsingNewHud() && SCOREBOARD_IsHudFullscreen() ) == false )
		{
			// Are we in a team game? Draw scores.
			if( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
				SCOREBOARD_RenderStats_TeamScores( );

			if ( !players[ulDisplayPlayer].bSpectating )
			{
				// Draw the player's rank and spread in FFA modes.
				if( !(GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS ))
					if( (GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNFRAGS ))
						SCOREBOARD_RenderStats_RankSpread( );

				// [BB] Draw number of lives left.
				if ( GAMEMODE_AreLivesLimited ( ) )
				{
					char szString[64];
					sprintf( szString, "Lives: %d / %d", static_cast<unsigned int> (players[ulDisplayPlayer].ulLivesLeft+1), GAMEMODE_GetMaxLives() );
					HUD_DrawText ( SmallFont, CR_RED, 0, static_cast<int> ( g_rYScale * ( ST_Y - g_ulTextHeight + 1 ) ), szString );
				}
			}

		}
	}
	
	// Display the bottom message.
	SCOREBOARD_DrawBottomString();
}

//*****************************************************************************
//
void SCOREBOARD_RenderBoard( ULONG ulDisplayPlayer )
{
	ULONG	ulNumIdealColumns;

	// Make sure the display player is valid.
	if ( ulDisplayPlayer >= MAXPLAYERS )
		return;

	// Draw the "RANKINGS" text at the top.
	scoreboard_DrawHeader( );

	// Draw the time, frags, points, or kills we have left until the level ends.
	if ( gamestate == GS_LEVEL )
		scoreboard_DrawLimits( );

	// Draw the team scores and their relation (tied, red leads, etc).
	scoreboard_DrawTeamScores( ulDisplayPlayer );

	// Draw my rank and my frags, points, etc.
	if ( gamestate == GS_LEVEL )
		scoreboard_DrawMyRank( ulDisplayPlayer );

	// Draw the player list and its data.
	// First, determine how many columns we can use, based on our screen resolution.
	ulNumIdealColumns = 3;
	if ( g_bScale )
	{
		if ( g_ValWidth.Int >= 480 )
			ulNumIdealColumns = 4;
		if ( g_ValWidth.Int >= 600 )
			ulNumIdealColumns = 5;
	}
	else
	{
		if ( SCREENWIDTH >= 480 )
			ulNumIdealColumns = 4;
		if ( SCREENWIDTH >= 600 )
			ulNumIdealColumns = 5;
	}

	// The 5 column display is only availible for modes that support it.
	if (( ulNumIdealColumns == 5 ) && !( GAMEMODE_GetCurrentFlags() & (GMF_PLAYERSEARNPOINTS|GMF_PLAYERSEARNWINS) ))
		ulNumIdealColumns = 4;

	if ( ulNumIdealColumns == 5 )
		scoreboard_Prepare5ColumnDisplay( );
	else if( ulNumIdealColumns == 4 )
		scoreboard_Prepare4ColumnDisplay( );
	else
		scoreboard_Prepare3ColumnDisplay( );

	// Draw the headers, list, entries, everything.
	scoreboard_DrawRankings( ulDisplayPlayer );
}

//*****************************************************************************
//

bool SCOREBOARD_IsUsingNewHud( void )
{
	if( cl_stfullscreenhud && ( gameinfo.gametype & GAME_DoomChex ) )
		return true;
	else
		return false;
}

bool SCOREBOARD_IsHudVisible( void )
{
	if (screenblocks < 12)
		return true;
	else
		return false;
}

bool SCOREBOARD_IsHudFullscreen( void )
{
	if (viewheight == SCREENHEIGHT)
		return true;
	else
		return false;
}
//*****************************************************************************
//
void SCOREBOARD_RenderStats_Holders( void )
{
	ULONG		ulYPos;
	char		szString[160];
	char		szPatchName[9];
	char		szName[MAXPLAYERNAME+1];

	// Draw the carrier information for ONE object (POS, TERM, OFCTF).
	if ( oneflagctf || terminator || possession || teampossession)
	{
		// Decide what text and object needs to be drawn.
		if ( possession || teampossession )
		{
			sprintf( szPatchName, "HELLSTON" );
			if ( g_pPossessionArtifactCarrier )
			{
				sprintf( szName, "%s", g_pPossessionArtifactCarrier->userinfo.GetName() );
				if ( teampossession )
				{
					V_RemoveColorCodes( szName );
					if ( TEAM_CheckIfValid ( g_pPossessionArtifactCarrier->ulTeam ) )
						sprintf( szString, "\\c%c%s :", V_GetColorChar( TEAM_GetTextColor( g_pPossessionArtifactCarrier->ulTeam )), szName );
				}
				else
					sprintf( szString, "%s \\cG:", szName );
			}
			else
				sprintf( szString, "\\cC- \\cG:" );
		}
		else if ( terminator )
		{
			sprintf( szPatchName, "TERMINAT" );
			sprintf( szString, "\\cC%s \\cG:", g_pTerminatorArtifactCarrier ? g_pTerminatorArtifactCarrier->userinfo.GetName() : "-" );
		}
		else if ( oneflagctf )
		{
			sprintf( szPatchName, "STFLA3" );
			if ( g_pWhiteCarrier )
			{
				sprintf( szName, "%s", g_pWhiteCarrier->userinfo.GetName() );
				V_RemoveColorCodes( szName );
				if ( TEAM_CheckIfValid ( g_pWhiteCarrier->ulTeam ) )
					sprintf( szString, "\\cC%s \\cC:", szName );
			}
			else
				sprintf( szString, "\\cC%s \\cC:", TEAM_GetReturnTicks( teams.Size( ) ) ? "?" : "-" );
		}

		// Now, draw it.
		ulYPos = ST_Y - g_ulTextHeight + 1;
		V_ColorizeString( szString );
		if ( g_bScale )
		{
			screen->DrawTexture( TexMan[szPatchName],
				( g_ValWidth.Int ) - ( TexMan[szPatchName]->GetWidth( )),
				(LONG)( ulYPos * g_rYScale ),
				DTA_VirtualWidth, g_ValWidth.Int,
				DTA_VirtualHeight, g_ValHeight.Int,
				TAG_DONE );

			screen->DrawText( SmallFont, CR_GRAY,
				( con_virtualwidth ) - ( TexMan[szPatchName]->GetWidth( )) - ( SmallFont->StringWidth( szString )),
				(LONG)( ulYPos * g_rYScale ),
				szString,
				DTA_VirtualWidth, g_ValWidth.Int,
				DTA_VirtualHeight, g_ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawTexture( TexMan[szPatchName],
				( SCREENWIDTH ) - ( TexMan[szPatchName]->GetWidth( )),
				(LONG)ulYPos,
				TAG_DONE );

			screen->DrawText( SmallFont, CR_GRAY,
				( SCREENWIDTH ) - ( TexMan[szPatchName]->GetWidth( )) - ( SmallFont->StringWidth( szString )),
				(LONG)ulYPos,
				szString,
				TAG_DONE );
		}
	}

	// Draw the carrier information for TWO objects (ST, CTF).
	else if ( ctf || skulltag )
	{
		ulYPos = ST_Y - g_ulTextHeight * 3 + 1;

		for ( LONG i = teams.Size( ) - 1; i >= 0; i-- )
		{
			if ( TEAM_ShouldUseTeam( i ) == false )
				continue;

			sprintf( szPatchName, "%s", TEAM_GetSmallHUDIcon( i ));
		
			if ( g_bScale )
			{
				screen->DrawTexture( TexMan[szPatchName],
					( g_ValWidth.Int ) - ( TexMan[szPatchName]->GetWidth( )),
					(LONG)( ulYPos * g_rYScale ),
					DTA_VirtualWidth, g_ValWidth.Int,
					DTA_VirtualHeight, g_ValHeight.Int,
					TAG_DONE );

				sprintf( szString, "\\cC%s \\c%c:", TEAM_GetCarrier( i ) ? TEAM_GetCarrier( i )->userinfo.GetName() : TEAM_GetReturnTicks( i ) ? "?" : "-", V_GetColorChar( TEAM_GetTextColor( i )));
				V_ColorizeString( szString );

				screen->DrawText( SmallFont, CR_GRAY,
					( g_ValWidth.Int ) - ( TexMan[szPatchName]->GetWidth( )) - ( SmallFont->StringWidth( szString )),
					(LONG)( ulYPos * g_rYScale ),
					szString,
					DTA_VirtualWidth, g_ValWidth.Int,
					DTA_VirtualHeight, g_ValHeight.Int,
					TAG_DONE );
			}
			else
			{
				screen->DrawTexture( TexMan[szPatchName],
					( SCREENWIDTH ) - ( TexMan[szPatchName]->GetWidth( )),
					ulYPos,
					TAG_DONE );

				sprintf( szString, "\\cC%s \\c%c:", TEAM_GetCarrier( i ) ? TEAM_GetCarrier( i )->userinfo.GetName() : TEAM_GetReturnTicks( i ) ? "?" : "-", V_GetColorChar( TEAM_GetTextColor( i )));
				V_ColorizeString( szString );

				screen->DrawText( SmallFont, CR_GRAY,
					( SCREENWIDTH ) - ( TexMan[szPatchName]->GetWidth( )) - ( SmallFont->StringWidth( szString )),
					ulYPos,
					szString,
					TAG_DONE );
			}

			ulYPos -= g_ulTextHeight;
		}
	}
	//Domination can have an indefinite amount
	else if ( domination )
	{
		DOMINATION_DrawHUD(g_bScale);
	}
}

//*****************************************************************************
//

void SCOREBOARD_RenderStats_TeamScores( void )
{	
	ULONG		ulYPos;
	char		szString[160];
	LONG		lTeamScore[MAX_TEAMS];
	
	// The classic sbar HUD for Doom, Heretic, and Hexen has its own display for CTF and Skulltag scores.
	if( (gameinfo.gametype == GAME_Doom) || ( gameinfo.gametype == GAME_Heretic) || ( gameinfo.gametype == GAME_Hexen) )
		if( SCOREBOARD_IsHudFullscreen() )
			if( ctf || skulltag || oneflagctf)
				return;

	ulYPos = ST_Y - ( g_ulTextHeight * 2 ) + 1;

	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
	{
		for ( ULONG i = 0; i < teams.Size( ); i++ )
		{
			if ( TEAM_ShouldUseTeam( i ) == false )
				continue;

			if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNWINS )
				lTeamScore[i] = TEAM_GetWinCount( i );
			else if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNPOINTS )
				lTeamScore[i] = TEAM_GetScore( i );
			else if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNFRAGS )
				lTeamScore[i] = TEAM_GetFragCount( i );
			else
				return;
		}

		for ( LONG i = teams.Size( ) - 1; i >= 0; i-- )
		{
			if ( TEAM_CountPlayers( i ) < 1 )
				continue;

			sprintf( szString , "\\c%c%s: \\cC%d", V_GetColorChar( TEAM_GetTextColor ( i )), TEAM_GetName( i ), static_cast<int>( lTeamScore[i] ));
			V_ColorizeString ( szString );

			ulYPos -= g_ulTextHeight;

			if ( g_bScale )
			{
				screen->DrawText( SmallFont, CR_GRAY,
					0,
					(LONG)( ulYPos * g_rYScale ),
					szString,
					DTA_VirtualWidth, g_ValWidth.Int,
					DTA_VirtualHeight, g_ValHeight.Int,
					TAG_DONE );
			}
			else
			{
				screen->DrawText( SmallFont, CR_GRAY,
					0,
					ulYPos,
					szString,
					TAG_DONE );
			}
		}
	}
}
//*****************************************************************************
//
void SCOREBOARD_RenderStats_RankSpread( void )
{
	ULONG		ulYPos;
	char		szString[160];

	ulYPos = ST_Y - ( g_ulTextHeight * 2 ) + 1;

	// [RC] Move this up to make room for armor on the fullscreen, classic display.
	if ( !st_scale && screenblocks > 10 )
		ulYPos -= ( g_ulTextHeight * 2 );

	// [RC] Don't draw this if there aren't any competitors.
	if ( g_ulNumPlayers <= 1 )
		return;

	sprintf( szString, "\\cGRANK: \\cC%d/%s%d", static_cast<unsigned int> (g_ulRank + 1), g_bIsTied ? "\\cG" : "", static_cast<unsigned int> (g_ulNumPlayers) );
	V_ColorizeString( szString );

	HUD_DrawText( SmallFont, CR_GRAY,
		0,
		(LONG)( ulYPos * g_rYScale ),
		szString );

	ulYPos += g_ulTextHeight;

	sprintf( szString, "\\cGSPREAD: \\cC%s%d", g_lSpread > 0 ? "+" : "", static_cast<int> (g_lSpread) );
	V_ColorizeString( szString );

	HUD_DrawText( SmallFont, CR_GRAY,
		0,
		(LONG)( ulYPos * g_rYScale ),
		szString );

	// 'Wins' isn't an entry on the statusbar, so we have to draw this here.
	// [BB] Note: SCOREBOARD_RenderStats_RankSpread is not called in LMS, so the LMS check can be removed...
	unsigned int viewplayerwins = static_cast<unsigned int>( players[ SCOREBOARD_GetViewPlayer() ].ulWins );
	if (( duel || lastmanstanding ) && ( viewplayerwins > 0 ))
	{
		sprintf( szString, "\\cGWINS: \\cC%d", viewplayerwins );
		V_ColorizeString( szString );

		HUD_DrawText( SmallFont, CR_GRAY,
			HUD_GetWidth() - SmallFont->StringWidth( szString ),
			(LONG)( ulYPos * g_rYScale ),
			szString );
	}
}

//*****************************************************************************
//

void SCOREBOARD_RenderInvasionStats( void )
{
	if( SCOREBOARD_IsUsingNewHud() && SCOREBOARD_IsHudFullscreen())
		return;

	if ( SCOREBOARD_IsHudVisible() )
	{
		char			szString[128];
		DHUDMessage		*pMsg;

		sprintf( szString, "WAVE: %d  MONSTERS: %d  ARCH-VILES: %d", static_cast<unsigned int> (INVASION_GetCurrentWave( )), static_cast<unsigned int> (INVASION_GetNumMonstersLeft( )), static_cast<unsigned int> (INVASION_GetNumArchVilesLeft( )));
		pMsg = new DHUDMessage( SmallFont, szString, 0.5f, 0.075f, 0, 0, CR_RED, 0.1f );

		StatusBar->AttachMessage( pMsg, MAKE_ID('I','N','V','S') );
	}
}

//*****************************************************************************
//
void SCOREBOARD_RenderInVoteClassic( void )
{
	char				szString[128];
	ULONG				ulCurYPos = 0;
	ULONG				ulIdx;
	ULONG				ulNumYes;
	ULONG				ulNumNo;
	ULONG				*pulPlayersWhoVotedYes;
	ULONG				*pulPlayersWhoVotedNo;

	// Start with the "VOTE NOW!" title.
	ulCurYPos = 16;

	// Render the title.
	sprintf( szString, "VOTE NOW!" );
	screen->DrawText( BigFont, gameinfo.gametype == GAME_Doom ? CR_RED : CR_UNTRANSLATED,
		160 - ( BigFont->StringWidth( szString ) / 2 ),
		ulCurYPos,
		szString,
		DTA_Clean, true, TAG_DONE );

	// Render who called the vote.
	ulCurYPos += 24;
	sprintf( szString, "Vote called by: %s", players[CALLVOTE_GetVoteCaller( )].userinfo.GetName() );
	screen->DrawText( SmallFont, CR_UNTRANSLATED,
		160 - ( SmallFont->StringWidth( szString ) / 2 ),
		ulCurYPos,
		szString,
		DTA_Clean, true, TAG_DONE );

	// Render the command being voted on.
	ulCurYPos += 16;
	sprintf( szString, "%s", CALLVOTE_GetCommand( ));
	screen->DrawText( SmallFont, CR_WHITE,
		160 - ( SmallFont->StringWidth( szString ) / 2 ),
		ulCurYPos,
		szString,
		DTA_Clean, true, TAG_DONE );

	// Render the reason for the vote being voted on.
	if ( strlen( CALLVOTE_GetReason( )) > 0 )
	{
		ulCurYPos += 16;
		sprintf( szString, "Reason: %s", CALLVOTE_GetReason( ));
		screen->DrawText( SmallFont, CR_ORANGE,
			160 - ( SmallFont->StringWidth( szString ) / 2 ),
			ulCurYPos,
			szString,
			DTA_Clean, true, TAG_DONE );
	}

	// Render how much time is left to vote.
	ulCurYPos += 16;
	sprintf( szString, "Vote ends in: %d", static_cast<unsigned int> (( CALLVOTE_GetCountdownTicks( ) + TICRATE ) / TICRATE) );
	screen->DrawText( SmallFont, CR_RED,
		160 - ( SmallFont->StringWidth( szString ) / 2 ),
		ulCurYPos,
		szString,
		DTA_Clean, true, TAG_DONE );

	pulPlayersWhoVotedYes = CALLVOTE_GetPlayersWhoVotedYes( );
	pulPlayersWhoVotedNo = CALLVOTE_GetPlayersWhoVotedNo( );
	ulNumYes = 0;
	ulNumNo = 0;

	// Count how many players voted for what.
	for ( ulIdx = 0; ulIdx < ( MAXPLAYERS / 2 ) + 1; ulIdx++ )
	{
		if ( pulPlayersWhoVotedYes[ulIdx] != MAXPLAYERS && SERVER_IsValidClient( pulPlayersWhoVotedYes[ulIdx] ))
			ulNumYes++;

		if ( pulPlayersWhoVotedNo[ulIdx] != MAXPLAYERS && SERVER_IsValidClient( pulPlayersWhoVotedNo[ulIdx] ))
			ulNumNo++;
	}

	// Display how many have voted for "Yes" and "No".
	ulCurYPos += 16;
	sprintf( szString, "YES: %d", static_cast<unsigned int> (ulNumYes) );
	screen->DrawText( SmallFont, CR_UNTRANSLATED,
		32,
		ulCurYPos,
		szString,
		DTA_Clean, true, TAG_DONE );

	sprintf( szString, "NO: %d", static_cast<unsigned int> (ulNumNo) );
	screen->DrawText( SmallFont, CR_UNTRANSLATED,
		320 - 32 - SmallFont->StringWidth( szString ),
		ulCurYPos,
		szString,
		DTA_Clean, true, TAG_DONE );

	// Show all the players who have voted, and what they voted for.
	ulCurYPos += 8;
	for ( ulIdx = 0; ulIdx < MAX( ulNumYes, ulNumNo ); ulIdx++ )
	{
		ulCurYPos += 8;
		if ( pulPlayersWhoVotedYes[ulIdx] != MAXPLAYERS && SERVER_IsValidClient( pulPlayersWhoVotedYes[ulIdx] ))
		{
			sprintf( szString, "%s", players[pulPlayersWhoVotedYes[ulIdx]].userinfo.GetName() );
			screen->DrawText( SmallFont, CR_UNTRANSLATED,
				32,
				ulCurYPos,
				szString,
				DTA_Clean, true, TAG_DONE );
		}

		if ( pulPlayersWhoVotedNo[ulIdx] != MAXPLAYERS && SERVER_IsValidClient( pulPlayersWhoVotedNo[ulIdx] ))
		{
			sprintf( szString, "%s", players[pulPlayersWhoVotedNo[ulIdx]].userinfo.GetName() );
			screen->DrawText( SmallFont, CR_UNTRANSLATED,
				320 - 32 - SmallFont->StringWidth( szString ),
				ulCurYPos,
				szString,
				DTA_Clean, true, TAG_DONE );
		}
	}
}

//*****************************************************************************
//
// [RC] New compact version; RenderInVoteClassic is the fullscreen version
//
void SCOREBOARD_RenderInVote( void )
{
	char				szString[128];
	char				szKeyYes[16];
	char				szKeyNo[16];
	ULONG				ulCurYPos = 0;
	ULONG				ulIdx;
	ULONG				ulNumYes = 0;
	ULONG				ulNumNo = 0;
	ULONG				*pulPlayersWhoVotedYes;
	ULONG				*pulPlayersWhoVotedNo;
	bool				bWeVotedYes = false;
	bool				bWeVotedNo = false;

	// Get how many players voted for what.
	pulPlayersWhoVotedYes = CALLVOTE_GetPlayersWhoVotedYes( );
	pulPlayersWhoVotedNo = CALLVOTE_GetPlayersWhoVotedNo( );
	for ( ulIdx = 0; ulIdx < ( MAXPLAYERS / 2 ) + 1; ulIdx++ )
	{
		if ( pulPlayersWhoVotedYes[ulIdx] != MAXPLAYERS && SERVER_IsValidClient( pulPlayersWhoVotedYes[ulIdx] ))
		{
			ulNumYes++;
			if( static_cast<signed> (pulPlayersWhoVotedYes[ulIdx]) == consoleplayer )
				bWeVotedYes = true;
		}

		if ( pulPlayersWhoVotedNo[ulIdx] != MAXPLAYERS && SERVER_IsValidClient( pulPlayersWhoVotedNo[ulIdx] ))
		{
			ulNumNo++;
			if( static_cast<signed> (pulPlayersWhoVotedNo[ulIdx]) == consoleplayer)
				bWeVotedNo = true;
		}
	}

	// Start at the top of the screen
	ulCurYPos = 8;	

	// Render the title and time left.
	sprintf( szString, "VOTE NOW! ( %d )", static_cast<unsigned int> (( CALLVOTE_GetCountdownTicks( ) + TICRATE ) / TICRATE) );

	if(g_bScale)
	{
		screen->DrawText( BigFont, gameinfo.gametype == GAME_Doom ? CR_RED : CR_UNTRANSLATED,
			(LONG)(160 * g_fXScale) - (BigFont->StringWidth( szString ) / 2),
			ulCurYPos,	szString, 
			DTA_VirtualWidth, g_ValWidth.Int,
			DTA_VirtualHeight, g_ValHeight.Int,
			TAG_DONE );
	}
	else
	{
		screen->DrawText( BigFont, gameinfo.gametype == GAME_Doom ? CR_RED : CR_UNTRANSLATED,
			SCREENWIDTH/2 - ( BigFont->StringWidth( szString ) / 2 ),
			ulCurYPos,
			szString,
			DTA_Clean,	g_bScale,	TAG_DONE );
	}						
	// Render the command being voted on.
	ulCurYPos += 14;
	sprintf( szString, "%s", CALLVOTE_GetCommand( ));
	if(g_bScale)
	{
		screen->DrawText( SmallFont, CR_WHITE,
			(LONG)(160 * g_fXScale) - ( SmallFont->StringWidth( szString ) / 2 ),
			ulCurYPos,	szString,
			DTA_VirtualWidth, g_ValWidth.Int,
			DTA_VirtualHeight, g_ValHeight.Int,
			TAG_DONE );
	}
	else
	{
		screen->DrawText( SmallFont, CR_WHITE,
			SCREENWIDTH/2 - ( SmallFont->StringWidth( szString ) / 2 ),
			ulCurYPos,
			szString,
			DTA_Clean,	g_bScale,	TAG_DONE );
	}

	ulCurYPos += 4;

	// Render the reason of the vote being voted on.
	if ( strlen( CALLVOTE_GetReason( )) > 0 )
	{
		ulCurYPos += 8;
		sprintf( szString, "Reason: %s", CALLVOTE_GetReason( ));
		if ( g_bScale )
		{
			screen->DrawText( SmallFont, CR_ORANGE,
				(LONG)(160 * g_fXScale) - ( SmallFont->StringWidth( szString ) / 2 ),
				ulCurYPos,	szString,
				DTA_VirtualWidth, g_ValWidth.Int,
				DTA_VirtualHeight, g_ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( SmallFont, CR_ORANGE,
				SCREENWIDTH/2 - ( SmallFont->StringWidth( szString ) / 2 ),
				ulCurYPos,
				szString,
				DTA_Clean,	g_bScale,	TAG_DONE );
		}
	}

	// Render the number of votes.
	ulCurYPos += 8;
	sprintf( szString, "\\c%sYes: %d, \\c%sNo: %d",  bWeVotedYes ? "k" : "s",  static_cast<unsigned int> (ulNumYes), bWeVotedNo ? "k" : "s", static_cast<unsigned int> (ulNumNo) );
	V_ColorizeString( szString );
	if(g_bScale)
	{
		screen->DrawText( SmallFont, CR_DARKBROWN,
			(LONG)(160 * g_fXScale) - ( SmallFont->StringWidth( szString ) / 2 ),
			ulCurYPos,	szString,
			DTA_VirtualWidth, g_ValWidth.Int,
			DTA_VirtualHeight, g_ValHeight.Int,
			TAG_DONE );
	}
	else
	{
		screen->DrawText( SmallFont, CR_DARKBROWN,
			SCREENWIDTH/2 - ( SmallFont->StringWidth( szString ) / 2 ),
			ulCurYPos,
			szString,
			DTA_Clean,	g_bScale,	TAG_DONE );
	}
	if( !bWeVotedYes && !bWeVotedNo )
	{
		// Render the explanation of keys.
		ulCurYPos += 8;

		static char vote_yes[] = "vote_yes";
		static char vote_no[] = "vote no";
		C_FindBind( vote_yes, szKeyYes );
		C_FindBind( vote_no, szKeyNo );
		sprintf( szString, "%s | %s", szKeyYes, szKeyNo);
		if(g_bScale)
		{
			screen->DrawText( SmallFont, CR_BLACK,
				(LONG)(160 * g_fXScale) - ( SmallFont->StringWidth( szString ) / 2 ),
				ulCurYPos,	szString,
				DTA_VirtualWidth, g_ValWidth.Int,
				DTA_VirtualHeight, g_ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( SmallFont, CR_BLACK,
				SCREENWIDTH/2 - ( SmallFont->StringWidth( szString ) / 2 ),
				ulCurYPos,
				szString,
				DTA_Clean,	g_bScale,	TAG_DONE );
		}

	}
}

//*****************************************************************************
//
void SCOREBOARD_RenderDuelCountdown( ULONG ulTimeLeft )
{
	char				szString[128];
	LONG				lDueler1 = -1;
	LONG				lDueler2 = -1;
	ULONG				ulCurYPos = 0;
	ULONG				ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( playeringame[ulIdx] && ( players[ulIdx].bSpectating == false ))
		{
			if ( lDueler1 == -1 )
				lDueler1 = ulIdx;
			else if ( lDueler2 == -1 )
				lDueler2 = ulIdx;
		}
	}

	// This really should not happen, because if we can't find two duelers, we're shouldn't be
	// in the countdown phase.
	if (( lDueler1 == -1 ) || ( lDueler2 == -1 ))
		return;

	// Start by drawing the "RANKINGS" patch 4 pixels from the top. Don't draw it if we're in intermission.
	ulCurYPos = 16;
	if ( gamestate == GS_LEVEL )
	{
		sprintf( szString, "%s", players[lDueler1].userinfo.GetName() );
		screen->DrawText( BigFont, gameinfo.gametype == GAME_Doom ? CR_RED : CR_UNTRANSLATED,
			160 - ( BigFont->StringWidth( szString ) / 2 ),
			ulCurYPos,
			szString,
			DTA_Clean, true, TAG_DONE );

		ulCurYPos += 16;
		sprintf( szString, "vs." );
		screen->DrawText( BigFont, CR_UNTRANSLATED,
			160 - ( BigFont->StringWidth( szString ) / 2 ),
			ulCurYPos,
			szString,
			DTA_Clean, true, TAG_DONE );

		ulCurYPos += 16;
		sprintf( szString, "%s", players[lDueler2].userinfo.GetName() );
		screen->DrawText( BigFont, gameinfo.gametype == GAME_Doom ? CR_RED : CR_UNTRANSLATED,
			160 - ( BigFont->StringWidth( szString ) / 2 ),
			ulCurYPos,
			szString,
			DTA_Clean, true, TAG_DONE );
	}

	ulCurYPos += 24;
	sprintf( szString, "Match begins in: %d", static_cast<unsigned int> (ulTimeLeft / TICRATE) );
	screen->DrawText( SmallFont, CR_UNTRANSLATED,
		160 - ( SmallFont->StringWidth( szString ) / 2 ),
		ulCurYPos,
		szString,
		DTA_Clean, true, TAG_DONE );
}

//*****************************************************************************
//
void SCOREBOARD_RenderLMSCountdown( ULONG ulTimeLeft )
{
	char				szString[128];
	ULONG				ulCurYPos = 0;

	if ( lastmanstanding )
	{
		// Start the "LAST MAN STANDING" title.
		ulCurYPos = 32;
		if ( gamestate == GS_LEVEL )
		{
			sprintf( szString, "LAST MAN STANDING" );
			screen->DrawText( BigFont, gameinfo.gametype == GAME_Doom ? CR_RED : CR_UNTRANSLATED,
				160 - ( BigFont->StringWidth( szString ) / 2 ),
				ulCurYPos,
				szString,
				DTA_Clean, true, TAG_DONE );
		}
	}
	else if ( teamlms )
	{
		ulCurYPos = 32;
		if ( gamestate == GS_LEVEL )
		{
			sprintf( szString, "TEAM LAST MAN STANDING" );
			screen->DrawText( BigFont, gameinfo.gametype == GAME_Doom ? CR_RED : CR_UNTRANSLATED,
				160 - ( BigFont->StringWidth( szString ) / 2 ),
				ulCurYPos,
				szString,
				DTA_Clean, true, TAG_DONE );
		}
	}

	ulCurYPos += 24;
	sprintf( szString, "Match begins in: %d", static_cast<unsigned int> (ulTimeLeft / TICRATE) );
	screen->DrawText( SmallFont, CR_UNTRANSLATED,
		160 - ( SmallFont->StringWidth( szString ) / 2 ),
		ulCurYPos,
		szString,
		DTA_Clean, true, TAG_DONE );
}

//*****************************************************************************
//
void SCOREBOARD_RenderPossessionCountdown( const char *pszString, ULONG ulTimeLeft )
{
	char				szString[128];
	ULONG				ulCurYPos = 0;

	if ( possession || teampossession )
	{
		// Start the "POSSESSION" title.
		ulCurYPos = 32;
		if ( gamestate == GS_LEVEL )
		{
			sprintf( szString, "%s", pszString );
			screen->DrawText( BigFont, gameinfo.gametype == GAME_Doom ? CR_RED : CR_UNTRANSLATED,
				160 - ( BigFont->StringWidth( szString ) / 2 ),
				ulCurYPos,
				szString,
				DTA_Clean, true, TAG_DONE );
		}
	}

	ulCurYPos += 24;
	sprintf( szString, "Match begins in: %d", static_cast<unsigned int> (ulTimeLeft / TICRATE) );
	screen->DrawText( SmallFont, CR_UNTRANSLATED,
		160 - ( SmallFont->StringWidth( szString ) / 2 ),
		ulCurYPos,
		szString,
		DTA_Clean, true, TAG_DONE );
}

//*****************************************************************************
//
void SCOREBOARD_RenderSurvivalCountdown( ULONG ulTimeLeft )
{
	char				szString[128];
	ULONG				ulCurYPos = 0;

	// Start the "SURVIVAL CO-OP" title.
	ulCurYPos = 32;
	if ( gamestate == GS_LEVEL )
	{
		sprintf( szString, "SURVIVAL CO-OP" );
		screen->DrawText( BigFont, gameinfo.gametype == GAME_Doom ? CR_RED : CR_UNTRANSLATED,
			160 - ( BigFont->StringWidth( szString ) / 2 ),
			ulCurYPos,
			szString,
			DTA_Clean, true, TAG_DONE );
	}

	ulCurYPos += 24;
	sprintf( szString, "Match begins in: %d", static_cast<unsigned int> (ulTimeLeft / TICRATE) );
	screen->DrawText( SmallFont, CR_UNTRANSLATED,
		160 - ( SmallFont->StringWidth( szString ) / 2 ),
		ulCurYPos,
		szString,
		DTA_Clean, true, TAG_DONE );
}

//*****************************************************************************
//
void SCOREBOARD_RenderInvasionFirstCountdown( ULONG ulTimeLeft )
{
	char				szString[128];
	ULONG				ulCurYPos = 0;

	// Start the "PREPARE FOR INVASION!" title.
	ulCurYPos = 32;
	if ( gamestate == GS_LEVEL )
	{
		if ( sv_maxlives > 0 )
			sprintf( szString, "SURVIVAL INVASION" );
		else
			sprintf( szString, "PREPARE FOR INVASION!" );
		screen->DrawText( BigFont, gameinfo.gametype == GAME_Doom ? CR_RED : CR_UNTRANSLATED,
			160 - ( BigFont->StringWidth( szString ) / 2 ),
			ulCurYPos,
			szString,
			DTA_Clean, true, TAG_DONE );
	}

	ulCurYPos += 24;
	sprintf( szString, "First wave begins in: %d", static_cast<unsigned int> (ulTimeLeft / TICRATE) );
	screen->DrawText( SmallFont, CR_UNTRANSLATED,
		160 - ( SmallFont->StringWidth( szString ) / 2 ),
		ulCurYPos,
		szString,
		DTA_Clean, true, TAG_DONE );
}

//*****************************************************************************
//
void SCOREBOARD_RenderInvasionCountdown( ULONG ulTimeLeft )
{
	char				szString[128];
	ULONG				ulCurYPos = 0;

	// Start the title.
	ulCurYPos = 32;
	if ( gamestate == GS_LEVEL )
	{
		// Build the string to use.
		if ((LONG)( INVASION_GetCurrentWave( ) + 1 ) == wavelimit )
			sprintf( szString, "FINAL WAVE!" );
		else
		{
			switch ( INVASION_GetCurrentWave( ) + 1 )
			{
			case 2:

				sprintf( szString, "Second wave" );
				break;
			case 3:

				sprintf( szString, "Third wave" );
				break;
			case 4:

				sprintf( szString, "Fourth wave" );
				break;
			case 5:

				sprintf( szString, "Fifth wave" );
				break;
			case 6:

				sprintf( szString, "Sixth wave" );
				break;
			case 7:

				sprintf( szString, "Seventh wave" );
				break;
			case 8:

				sprintf( szString, "Eighth wave" );
				break;
			case 9:

				sprintf( szString, "Ninth wave" );
				break;
			case 10:

				sprintf( szString, "Tenth wave" );
				break;
			default:

				{
					ULONG	ulWave;
					char	szSuffix[3];

					ulWave = INVASION_GetCurrentWave( ) + 1;

					// xx11-13 are exceptions; they're always "th".
					if ((( ulWave % 100 ) >= 11 ) && (( ulWave % 100 ) <= 13 ))
					{
						sprintf( szSuffix, "th" );
					}
					else
					{
						if (( ulWave % 10 ) == 1 )
							sprintf( szSuffix, "st" );
						else if (( ulWave % 10 ) == 2 )
							sprintf( szSuffix, "nd" );
						else if (( ulWave % 10 ) == 3 )
							sprintf( szSuffix, "rd" );
						else
							sprintf( szSuffix, "th" );
					}

					sprintf( szString, "%d%s wave", static_cast<unsigned int> (ulWave), szSuffix );
				}
				break;
			}
		}

		screen->DrawText( BigFont, gameinfo.gametype == GAME_Doom ? CR_RED : CR_UNTRANSLATED,
			160 - ( BigFont->StringWidth( szString ) / 2 ),
			ulCurYPos,
			szString,
			DTA_Clean, true, TAG_DONE );
	}

	ulCurYPos += 24;
	sprintf( szString, "begins in: %d", static_cast<unsigned int> (ulTimeLeft / TICRATE) );
	screen->DrawText( SmallFont, CR_UNTRANSLATED,
		160 - ( SmallFont->StringWidth( szString ) / 2 ),
		ulCurYPos,
		szString,
		DTA_Clean, true, TAG_DONE );
}

//*****************************************************************************
//
LONG SCOREBOARD_CalcSpread( ULONG ulPlayerNum )
{
	bool	bInit = true;
	ULONG	ulIdx;
	LONG	lHighestFrags = 0;

	// First, find the highest fragcount that isn't ours.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNWINS )
		{
			if (( ulPlayerNum == ulIdx ) || ( playeringame[ulIdx] == false ) || ( PLAYER_IsTrueSpectator( &players[ulIdx] )))
				continue;

			if ( bInit )
			{
				lHighestFrags = players[ulIdx].ulWins;
				bInit = false;
			}

			if ( players[ulIdx].ulWins > (ULONG)lHighestFrags )
				lHighestFrags = players[ulIdx].ulWins;
		}
		else if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNPOINTS )
		{
			if (( ulPlayerNum == ulIdx ) || ( playeringame[ulIdx] == false ) || ( players[ulIdx].bSpectating ))
				continue;

			if ( bInit )
			{
				lHighestFrags = players[ulIdx].lPointCount;
				bInit = false;
			}

			if ( players[ulIdx].lPointCount > lHighestFrags )
				lHighestFrags = players[ulIdx].lPointCount;
		}
		else
		{
			if ( ulPlayerNum == ulIdx || ( playeringame[ulIdx] == false ) || ( players[ulIdx].bSpectating ))
				continue;

			if ( bInit )
			{
				lHighestFrags = players[ulIdx].fragcount;
				bInit = false;
			}

			if ( players[ulIdx].fragcount > lHighestFrags )
				lHighestFrags = players[ulIdx].fragcount;
		}
	}

	// If we're the only person in the game...
	if ( bInit )
	{
		if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNWINS )
			lHighestFrags = players[ulPlayerNum].ulWins;
		else if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNPOINTS )
			lHighestFrags = players[ulPlayerNum].lPointCount;
		else
			lHighestFrags = players[ulPlayerNum].fragcount;
	}

	// Finally, simply return the difference.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNWINS )
		return ( players[ulPlayerNum].ulWins - lHighestFrags );
	else if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNPOINTS )
		return ( players[ulPlayerNum].lPointCount - lHighestFrags );
	else
		return ( players[ulPlayerNum].fragcount - lHighestFrags );
}

//*****************************************************************************
//
ULONG SCOREBOARD_CalcRank( ULONG ulPlayerNum )
{
	ULONG	ulIdx;
	ULONG	ulRank;

	ulRank = 0;
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( ulIdx == ulPlayerNum || ( playeringame[ulIdx] == false ) || ( PLAYER_IsTrueSpectator( &players[ulIdx] )))
			continue;

		if ( lastmanstanding )
		{
			if ( players[ulIdx].ulWins > players[ulPlayerNum].ulWins )
				ulRank++;
		}
		else if ( possession )
		{
			if ( players[ulIdx].lPointCount > players[ulPlayerNum].lPointCount )
				ulRank++;
		}
		else
		{
			if ( players[ulIdx].fragcount > players[ulPlayerNum].fragcount )
				ulRank++;
		}
	}

	return ( ulRank );
}

//*****************************************************************************
//
bool SCOREBOARD_IsTied( ULONG ulPlayerNum )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( ulIdx == ulPlayerNum || ( playeringame[ulIdx] == false ) || ( PLAYER_IsTrueSpectator( &players[ulIdx] )))
			continue;

		if ( lastmanstanding )
		{
			if ( players[ulIdx].ulWins == players[ulPlayerNum].ulWins )
				return ( true );
		}
		else if ( possession )
		{
			if ( players[ulIdx].lPointCount == players[ulPlayerNum].lPointCount )
				return ( true );
		}
		else
		{
			if ( players[ulIdx].fragcount == players[ulPlayerNum].fragcount )
				return ( true );
		}
	}

	return ( false );
}

//*****************************************************************************
//
void SCOREBOARD_BuildPointString( char *pszString, const char *pszPointName, bool (*CheckAllEqual) ( void ), LONG (*GetHighestCount) ( void ), LONG (*GetCount) ( ULONG ulTeam ) )
{
	// Build the score message.
	if ( CheckAllEqual( ))
	{
		sprintf( pszString, "Teams are tied at %d", static_cast<int>( GetCount( 0 )));
	}
	else
	{
		LONG lHighestCount = GetHighestCount( );
		LONG lFirstTeamCount = LONG_MIN;
		ULONG ulNumberOfTeams = 0;
		FString OutString;

		for ( ULONG i = 0; i < teams.Size( ); i++ )
		{
			if ( TEAM_ShouldUseTeam( i ) == false )
				continue;

			if ( lHighestCount == GetCount( i ) )
			{
				ulNumberOfTeams++;

				if ( lFirstTeamCount == LONG_MIN )
				{
					OutString = "\\c";
					OutString += V_GetColorChar( TEAM_GetTextColor( i ));
					OutString += TEAM_GetName( i );

					lFirstTeamCount = GetCount( i );
				}
				else
				{
					OutString += "\\c-, ";
					OutString += "\\c";
					OutString += V_GetColorChar( TEAM_GetTextColor( i ));
					OutString += TEAM_GetName( i );
				}
			}
		}

		if ( TEAM_GetNumAvailableTeams( ) > 2 )
		{
			if ( gamestate == GS_LEVEL )
			{
				if ( ulNumberOfTeams == 1 )
					sprintf( pszString, "%s\\c- leads with %d %s%s", OutString.GetChars( ), static_cast<int>( lHighestCount ), pszPointName, static_cast<int>( lHighestCount ) != 1 ? "s" : "" );
				else
					sprintf( pszString, "Teams leading with %d %s%s: %s", static_cast<int>( lHighestCount ), pszPointName, static_cast<int>( lHighestCount ) != 1 ? "s" : "", OutString.GetChars( ));
			}
			else
			{
				if (ulNumberOfTeams != 1)
					sprintf( pszString, "Teams that won with %d %s%s: %s", static_cast<int>( lHighestCount ), pszPointName, static_cast<int>( lHighestCount ) != 1 ? "s" : "", OutString.GetChars( ) );
				else
					sprintf( pszString, "%s\\c- has won with %d %s%s", OutString.GetChars( ), static_cast<int>( lHighestCount ), pszPointName, static_cast<int>( lHighestCount ) != 1 ? "s" : "" );
			}
		}
		else
		{
			if ( gamestate == GS_LEVEL )
			{
				if ( GetCount( 1 ) > GetCount( 0 ))
					sprintf( pszString, "\\c%c%s\\c- leads %d to %d", V_GetColorChar( TEAM_GetTextColor( 1 )), TEAM_GetName( 1 ), static_cast<int>( GetCount( 1 )), static_cast<int>( GetCount( 0 )));
				else
					sprintf( pszString, "\\c%c%s\\c- leads %d to %d", V_GetColorChar( TEAM_GetTextColor( 0 )), TEAM_GetName( 0 ), static_cast<int>( GetCount( 0 )), static_cast<int>( GetCount( 1 )));
			}
			else
			{
				if ( GetCount( 1 ) > GetCount( 0 ))
					sprintf( pszString, "\\c%c%s\\c- has won %d to %d", V_GetColorChar( TEAM_GetTextColor( 1 )), TEAM_GetName( 1 ), static_cast<int>( GetCount( 1 )), static_cast<int>( GetCount( 0 )));
				else
					sprintf( pszString, "\\c%c%s\\c- has won %d to %d", V_GetColorChar( TEAM_GetTextColor( 0 )), TEAM_GetName( 0 ), static_cast<int>( GetCount( 0 )), static_cast<int>( GetCount( 1 )));
			}
		}
	}
}

//*****************************************************************************
//
// [TP] Now in a function
//
FString SCOREBOARD_SpellOrdinal( int ranknum )
{
	FString result;
	result.AppendFormat( "%d", ranknum + 1 );

	//[ES] This way all ordinals are correctly written.
	if ( ranknum % 100 / 10 != 1 )
	{
		switch ( ranknum % 10 )
		{
		case 0:
			result += "st";
			break;

		case 1:
			result += "nd";
			break;

		case 2:
			result += "rd";
			break;

		default:
			result += "th";
			break;
		}
	}
	else
		result += "th";

	return result;
}

//*****************************************************************************
//
// [TP]
//

FString SCOREBOARD_SpellOrdinalColored( int ranknum )
{
	FString result;

	// Determine  what color and number to print for their rank.
	switch ( g_ulRank )
	{
	case 0: result = "\\cH"; break;
	case 1: result = "\\cG"; break;
	case 2: result = "\\cD"; break;
	}

	result += SCOREBOARD_SpellOrdinal( ranknum );
	return result;
}

//*****************************************************************************
//
void SCOREBOARD_BuildPlaceString ( char* pszString )
{
	if ( ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS ) && ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNFRAGS ) )
	{
		// Build the score message.
		SCOREBOARD_BuildPointString( pszString, "frag", &TEAM_CheckAllTeamsHaveEqualFrags, &TEAM_GetHighestFragCount, &TEAM_GetFragCount );
	}
	else if ( ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS ) && ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNPOINTS ) )
	{
		// Build the score message.
		SCOREBOARD_BuildPointString( pszString, "score", &TEAM_CheckAllTeamsHaveEqualScores, &TEAM_GetHighestScoreCount, &TEAM_GetScore );
	}
	else if ( !( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS ) && ( GAMEMODE_GetCurrentFlags() & (GMF_PLAYERSEARNFRAGS|GMF_PLAYERSEARNPOINTS) ) )
	{
		// If the player is tied with someone else, add a "tied for" to their string.
		if ( SCOREBOARD_IsTied( consoleplayer ) )
			sprintf( pszString, "Tied for " );
		else
			pszString[0] = 0;

		strcpy( pszString + strlen ( pszString ), SCOREBOARD_SpellOrdinalColored( g_ulRank ));

		// Tack on the rest of the string.
		if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNPOINTS )
			sprintf( pszString + strlen ( pszString ), "\\c- place with %d point%s", static_cast<int> (players[consoleplayer].lPointCount), players[consoleplayer].lPointCount == 1 ? "" : "s" );
		else
			sprintf( pszString + strlen ( pszString ), "\\c- place with %d frag%s", players[consoleplayer].fragcount, players[consoleplayer].fragcount == 1 ? "" : "s" );
	}
}

//*****************************************************************************
//
void SCOREBOARD_DisplayFragMessage( player_t *pFraggedPlayer )
{
	char	szString[128];
	DHUDMessageFadeOut	*pMsg;

	sprintf( szString, "You fragged %s\\c-!\n", pFraggedPlayer->userinfo.GetName() );

	// Print the frag message out in the console.
	Printf( "%s", szString );

	V_ColorizeString( szString );

	pMsg = new DHUDMessageFadeOut( BigFont, szString,
		1.5f,
		0.325f,
		0,
		0,
		CR_RED,
		2.5f,
		0.5f );

	StatusBar->AttachMessage( pMsg, MAKE_ID('F','R','A','G') );

	szString[0] = 0;

	if ( lastmanstanding )
	{
		LONG	lMenLeftStanding;

		lMenLeftStanding = GAME_CountLivingAndRespawnablePlayers( ) - 1;
		sprintf( szString, "%d opponent%s left standing", static_cast<int> (lMenLeftStanding), ( lMenLeftStanding != 1 ) ? "s" : "" );
	}
	else if (( teamlms ) && ( players[consoleplayer].bOnTeam ))
	{
		LONG	lMenLeftStanding = 0;

		for ( ULONG i = 0; i < teams.Size( ); i++ )
		{
			if ( TEAM_ShouldUseTeam( i ) == false )
				continue;

			if ( i == players[consoleplayer].ulTeam )
				continue;

			lMenLeftStanding += TEAM_CountLivingAndRespawnablePlayers( i );
		}

		sprintf( szString, "%d opponent%s left standing", static_cast<int> (lMenLeftStanding), ( lMenLeftStanding != 1 ) ? "s" : "" );
	}
	else
		SCOREBOARD_BuildPlaceString ( szString );

	if ( szString[0] != 0 )
	{
		V_ColorizeString( szString );
		pMsg = new DHUDMessageFadeOut( SmallFont, szString,
			1.5f,
			0.375f,
			0,
			0,
			CR_RED,
			2.5f,
			0.5f );

		StatusBar->AttachMessage( pMsg, MAKE_ID('P','L','A','C') );
	}
}

//*****************************************************************************
//
void SCOREBOARD_DisplayFraggedMessage( player_t *pFraggingPlayer )
{
	char	szString[128];
	DHUDMessageFadeOut	*pMsg;

	sprintf( szString, "You were fragged by %s\\c-.\n", pFraggingPlayer->userinfo.GetName() );

	// Print the frag message out in the console.
	Printf( "%s", szString );

	V_ColorizeString( szString );

	pMsg = new DHUDMessageFadeOut( BigFont, szString,
		1.5f,
		0.325f,
		0,
		0,
		CR_RED,
		2.5f,
		0.5f );

	StatusBar->AttachMessage( pMsg, MAKE_ID('F','R','A','G') );

	szString[0] = 0;

	SCOREBOARD_BuildPlaceString ( szString );

	if ( szString[0] != 0 )
	{
		V_ColorizeString( szString );
		pMsg = new DHUDMessageFadeOut( SmallFont, szString,
			1.5f,
			0.375f,
			0,
			0,
			CR_RED,
			2.5f,
			0.5f );

		StatusBar->AttachMessage( pMsg, MAKE_ID('P','L','A','C') );
	}
}

//*****************************************************************************
//
void SCOREBOARD_RefreshHUD( void )
{
	ULONG	ulIdx;

	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	g_ulNumPlayers = 0;
	g_pTerminatorArtifactCarrier = NULL;
	g_pPossessionArtifactCarrier = NULL;

	for ( ULONG i = 0; i < teams.Size( ); i++ )
		TEAM_SetCarrier ( i, NULL );

	g_pWhiteCarrier = NULL;
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( lastmanstanding || teamlms )
		{
			if ( playeringame[ulIdx] == false )
				continue;

			if ( PLAYER_IsTrueSpectator( &players[ulIdx] ) == false )
				g_ulNumPlayers++;
		}
		else
		{
			if ( playeringame[ulIdx] && ( players[ulIdx].bSpectating == false ))
			{
				g_ulNumPlayers++;

				if ( players[ulIdx].cheats2 & CF2_TERMINATORARTIFACT )
					g_pTerminatorArtifactCarrier = &players[ulIdx];
				else if ( players[ulIdx].cheats2 & CF2_POSSESSIONARTIFACT )
					g_pPossessionArtifactCarrier = &players[ulIdx];
				else if ( players[ulIdx].mo )
				{
					for ( ULONG i = 0; i < teams.Size( ); i++ )
					{
						if ( players[ulIdx].mo->FindInventory( TEAM_GetItem( i )))
							TEAM_SetCarrier( i, &players[ulIdx] );
					}

					if ( players[ulIdx].mo->FindInventory( PClass::FindClass( "WhiteFlag" )))
						g_pWhiteCarrier = &players[ulIdx];
				}
			}
		}
	}

	player_t *player = &players[ SCOREBOARD_GetViewPlayer() ];

	g_ulRank = SCOREBOARD_CalcRank( player - players );
	g_lSpread = SCOREBOARD_CalcSpread( player - players );
	g_bIsTied = SCOREBOARD_IsTied( player - players );

	// "x opponents left", "x allies alive", etc
	if ( GAMEMODE_GetCurrentFlags() & GMF_DEADSPECTATORS )
	{
		// Survival, Survival Invasion, etc
		if ( GAMEMODE_GetCurrentFlags() & GMF_COOPERATIVE )
			g_lNumAlliesLeft = GAME_CountLivingAndRespawnablePlayers() - PLAYER_IsAliveOrCanRespawn( player );

		// Last Man Standing, TLMS, etc
		if ( GAMEMODE_GetCurrentFlags() & GMF_DEATHMATCH )
		{
			if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
			{
				unsigned livingAndRespawnableTeammates = TEAM_CountLivingAndRespawnablePlayers( player->ulTeam );
				g_lNumOpponentsLeft = GAME_CountLivingAndRespawnablePlayers() - livingAndRespawnableTeammates;
				g_lNumAlliesLeft = livingAndRespawnableTeammates - PLAYER_IsAliveOrCanRespawn( player );
			}
			else
			{
				g_lNumOpponentsLeft = GAME_CountLivingAndRespawnablePlayers() - PLAYER_IsAliveOrCanRespawn( player );
			}
		}
	}
}

//*****************************************************************************
//
ULONG SCOREBOARD_GetNumPlayers( void )
{
	return ( g_ulNumPlayers );
}

//*****************************************************************************
//
ULONG SCOREBOARD_GetRank( void )
{
	return ( g_ulRank );
}

//*****************************************************************************
//
LONG SCOREBOARD_GetSpread( void )
{
	return ( g_lSpread );
}

//*****************************************************************************
//
LONG SCOREBOARD_GetLeftToLimit( void )
{
	ULONG	ulIdx;

	// If we're not in a level, then clearly there's no need for this.
	if ( gamestate != GS_LEVEL )
		return ( 0 );

	// KILL-based mode. [BB] This works indepently of any players in game.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNKILLS )
	{
		if ( invasion )
			return (LONG) INVASION_GetNumMonstersLeft( );
		else if ( dmflags2 & DF2_KILL_MONSTERS )
		{
			if ( level.total_monsters > 0 )
				return ( 100 * ( level.total_monsters - level.killed_monsters ) / level.total_monsters );
			else
				return 0;
		}
		else
			return ( level.total_monsters - level.killed_monsters );
	}

	// [BB] In a team game with only empty teams or if there are no players at all, just return the appropriate limit.
	if ( ( ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
	     && ( TEAM_TeamsWithPlayersOn() == 0 ) )
		 || ( SERVER_CalcNumNonSpectatingPlayers( MAXPLAYERS ) == 0 ) )
	{
		if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNWINS )
			return winlimit;
		else if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNPOINTS )
			return pointlimit;
		else if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNFRAGS )
			return fraglimit;
		else
			return 0;
	}

	// FRAG-based mode.
	if ( fraglimit && GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNFRAGS )
	{
		LONG	lHighestFragcount;
				
		if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
			lHighestFragcount = TEAM_GetHighestFragCount( );		
		else
		{
			lHighestFragcount = INT_MIN;
			for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
			{
				if ( playeringame[ulIdx] && !players[ulIdx].bSpectating && players[ulIdx].fragcount > lHighestFragcount )
					lHighestFragcount = players[ulIdx].fragcount;
			}
		}

		return ( fraglimit - lHighestFragcount );
	}

	// POINT-based mode.
	else if ( pointlimit && GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNPOINTS )
	{
		if ( teamgame || teampossession )
			return ( pointlimit - TEAM_GetHighestScoreCount( ));		
		else // Must be possession mode.
		{
			LONG lHighestPointCount = INT_MIN;
			for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
			{
				if ( playeringame[ulIdx] && !players[ulIdx].bSpectating && players[ulIdx].lPointCount > lHighestPointCount )
					lHighestPointCount = players[ulIdx].lPointCount;
			}

			return pointlimit - (ULONG) lHighestPointCount;
		}
	}

	// WIN-based mode (LMS).
	else if ( winlimit && GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNWINS )
	{
		bool	bFoundPlayer = false;
		LONG	lHighestWincount = 0;

		if ( teamlms )
			lHighestWincount = TEAM_GetHighestWinCount( );
		else
		{
			for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
			{
				if ( playeringame[ulIdx] == false || players[ulIdx].bSpectating )
					continue;

				if ( bFoundPlayer == false )
				{
					lHighestWincount = players[ulIdx].ulWins;
					bFoundPlayer = true;
					continue;
				}
				else if ( players[ulIdx].ulWins > (ULONG)lHighestWincount )
					lHighestWincount = players[ulIdx].ulWins;
			}
		}

		return ( winlimit - lHighestWincount );
	}

	// None of the above.
	return ( -1 );
}

//*****************************************************************************
//
bool SCOREBOARD_IsTied( void )
{
	return ( g_bIsTied );
}

//*****************************************************************************
//*****************************************************************************
//
static void scoreboard_SortPlayers( ULONG ulSortType )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		g_iSortedPlayers[ulIdx] = ulIdx;

	if ( ulSortType == ST_FRAGCOUNT )
		qsort( g_iSortedPlayers, MAXPLAYERS, sizeof( int ), scoreboard_FragCompareFunc );
	else if ( ulSortType == ST_POINTCOUNT )
		qsort( g_iSortedPlayers, MAXPLAYERS, sizeof( int ), scoreboard_PointsCompareFunc );
	else if ( ulSortType == ST_WINCOUNT )
		qsort( g_iSortedPlayers, MAXPLAYERS, sizeof( int ), scoreboard_WinsCompareFunc );
	else
		qsort( g_iSortedPlayers, MAXPLAYERS, sizeof( int ), scoreboard_KillsCompareFunc );
}

//*****************************************************************************
//
static int STACK_ARGS scoreboard_FragCompareFunc( const void *arg1, const void *arg2 )
{
	return ( players[*(int *)arg2].fragcount - players[*(int *)arg1].fragcount );
}

//*****************************************************************************
//
static int STACK_ARGS scoreboard_PointsCompareFunc( const void *arg1, const void *arg2 )
{
	return ( players[*(int *)arg2].lPointCount - players[*(int *)arg1].lPointCount );
}

//*****************************************************************************
//
static int STACK_ARGS scoreboard_KillsCompareFunc( const void *arg1, const void *arg2 )
{
	return ( players[*(int *)arg2].killcount - players[*(int *)arg1].killcount );
}

//*****************************************************************************
//
static int STACK_ARGS scoreboard_WinsCompareFunc( const void *arg1, const void *arg2 )
{
	return ( players[*(int *)arg2].ulWins - players[*(int *)arg1].ulWins );
}

//*****************************************************************************
//
static void scoreboard_RenderIndividualPlayer( ULONG ulDisplayPlayer, ULONG ulPlayer )
{
	ULONG	ulIdx;
	ULONG	ulColor;
	LONG	lXPosOffset;
	char	szPatchName[9];
	char	szString[64];

	// Draw the data for each column.
	for( ulIdx = 0; ulIdx < g_ulNumColumnsUsed; ulIdx++ )
	{
		szString[0] = 0;

		ulColor = CR_GRAY;
		if (( terminator ) && ( players[ulPlayer].cheats2 & CF2_TERMINATORARTIFACT ))
			ulColor = CR_RED;
		else if ( players[ulPlayer].bOnTeam == true )
			ulColor = TEAM_GetTextColor( players[ulPlayer].ulTeam );
		else if ( ulDisplayPlayer == ulPlayer )
			ulColor = demoplayback ? CR_GOLD : CR_GREEN;

		// Determine what needs to be displayed in this column.
		switch ( g_aulColumnType[ulIdx] )
		{
			case COLUMN_NAME:

				sprintf( szString, "%s", players[ulPlayer].userinfo.GetName() );

				// Track where we are to draw multiple icons.
				lXPosOffset = -SmallFont->StringWidth( "  " );

				// [TP] If this player is in the join queue, display the position.
				{
					int position = JOINQUEUE_GetPositionInLine( ulPlayer );
					if ( position != -1 )
					{
						FString text;
						text.Format( "%d.", position + 1 );
						lXPosOffset -= SmallFont->StringWidth ( text );
						if ( g_bScale )
						{
							screen->DrawText( SmallFont, ( position == 0 ) ? CR_RED : CR_GOLD,
								(LONG)( g_aulColumnX[ulIdx] * g_fXScale ) + lXPosOffset,
								g_ulCurYPos,
								text.GetChars(),
								DTA_VirtualWidth, g_ValWidth.Int,
								DTA_VirtualHeight, g_ValHeight.Int,
								TAG_DONE );
						}
						else
						{
							screen->DrawText( SmallFont, ( position == 0 ) ? CR_RED : CR_GOLD,
								(LONG)( g_aulColumnX[ulIdx] / 320.0f * SCREENWIDTH ) + lXPosOffset,
								g_ulCurYPos,
								text.GetChars(),
								DTA_Clean,
								g_bScale,
								TAG_DONE );
						}
						lXPosOffset -= 4;
					}
				}

				// Draw the user's handicap, if any.
				if ( players[ulPlayer].userinfo.GetHandicap() > 0 )
				{
					char	szHandicapString[8];

					if ( lastmanstanding || teamlms )
					{
						if (( deh.MaxSoulsphere - (LONG)players[ulPlayer].userinfo.GetHandicap() ) < 1 )
							sprintf( szHandicapString, "(1)" );
						else
							sprintf( szHandicapString, "(%d)", static_cast<int> (deh.MaxArmor - (LONG)players[ulPlayer].userinfo.GetHandicap()) );
					}
					else
					{
						if (( deh.StartHealth - (LONG)players[ulPlayer].userinfo.GetHandicap() ) < 1 )
							sprintf( szHandicapString, "(1)" );
						else
							sprintf( szHandicapString, "(%d)", static_cast<int> (deh.StartHealth - (LONG)players[ulPlayer].userinfo.GetHandicap()) );
					}
					
					lXPosOffset -= SmallFont->StringWidth ( szHandicapString );
					if ( g_bScale )
					{
						screen->DrawText( SmallFont, ulColor,
							(LONG)( g_aulColumnX[ulIdx] * g_fXScale ) + lXPosOffset,
							g_ulCurYPos,
							szHandicapString,
							DTA_VirtualWidth, g_ValWidth.Int,
							DTA_VirtualHeight, g_ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawText( SmallFont, ulColor,
							(LONG)( g_aulColumnX[ulIdx] / 320.0f * SCREENWIDTH ) + lXPosOffset,
							g_ulCurYPos,
							szHandicapString,
							DTA_Clean,
							g_bScale,
							TAG_DONE );
					}
					lXPosOffset -= 4;
				}

				// Draw an icon if this player is a ready to go on.
				if ( players[ulPlayer].bReadyToGoOn )
				{
					sprintf( szPatchName, "RDYTOGO" );
					lXPosOffset -= TexMan[szPatchName]->GetWidth();
					if ( g_bScale )
					{
						screen->DrawTexture( TexMan[szPatchName],
							(LONG)( g_aulColumnX[ulIdx] * g_fXScale ) + lXPosOffset,
							g_ulCurYPos - (( TexMan[szPatchName]->GetHeight( ) - SmallFont->GetHeight( )) / 2 ),
							DTA_VirtualWidth, g_ValWidth.Int,
							DTA_VirtualHeight, g_ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawTexture( TexMan[szPatchName],
							(LONG)( g_aulColumnX[ulIdx] / 320.0f * SCREENWIDTH ) + lXPosOffset,
							g_ulCurYPos - (( TexMan[szPatchName]->GetHeight( ) - SmallFont->GetHeight( )) / 2 ),
							TAG_DONE );
					}
					lXPosOffset -= 4;
				}

				// Draw a bot icon if this player is a bot.
				if ( players[ulPlayer].bIsBot )
				{
					sprintf( szPatchName, "BOTSKIL%d", botskill.GetGenericRep( CVAR_Int ).Int);

					lXPosOffset -= TexMan[szPatchName]->GetWidth();
					if ( g_bScale )
					{
						screen->DrawTexture( TexMan[szPatchName],
							(LONG)( g_aulColumnX[ulIdx] * g_fXScale ) + lXPosOffset,
							g_ulCurYPos,
							DTA_VirtualWidth, g_ValWidth.Int,
							DTA_VirtualHeight, g_ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawTexture( TexMan[szPatchName],
							(LONG)( g_aulColumnX[ulIdx] / 320.0f * SCREENWIDTH ) + lXPosOffset,
							g_ulCurYPos,
							DTA_Clean,
							g_bScale,
							TAG_DONE );
					}
					lXPosOffset -= 4;
				}

				// Draw a chat icon if this player is chatting.
				// [Cata] Also shows who's in the console.
				if (( players[ulPlayer].bChatting ) || ( players[ulPlayer].bInConsole ))
				{
					if ( players[ulPlayer].bInConsole )
						sprintf( szPatchName, "CONSMINI" );
					else
						sprintf( szPatchName, "TLKMINI" );

					lXPosOffset -= TexMan[szPatchName]->GetWidth();
					if ( g_bScale )
					{
						screen->DrawTexture( TexMan[szPatchName],
							(LONG)( g_aulColumnX[ulIdx] * g_fXScale ) + lXPosOffset,
							g_ulCurYPos - 1,
							DTA_VirtualWidth, g_ValWidth.Int,
							DTA_VirtualHeight, g_ValHeight.Int,
							TAG_DONE );
					}
					else
					{
						screen->DrawTexture( TexMan[szPatchName],
							(LONG)( g_aulColumnX[ulIdx] / 320.0f * SCREENWIDTH ) + lXPosOffset,
							g_ulCurYPos - 1,
							DTA_Clean,
							g_bScale,
							TAG_DONE );
					}
					lXPosOffset -= 4;
				}

				// Draw text if there's a vote on and this player voted.
				if ( CALLVOTE_GetVoteState() == VOTESTATE_INVOTE )
				{
					ULONG				*pulPlayersWhoVotedYes;
					ULONG				*pulPlayersWhoVotedNo;
					ULONG				ulNumYes = 0;
					ULONG				ulNumNo = 0;
					bool				bWeVotedYes = false;
					bool				bWeVotedNo = false;

					// Get how many players voted for what.
					pulPlayersWhoVotedYes = CALLVOTE_GetPlayersWhoVotedYes( );
					pulPlayersWhoVotedNo = CALLVOTE_GetPlayersWhoVotedNo( );

					for ( ULONG ulidx2 = 0; ulidx2 < ( MAXPLAYERS / 2 ) + 1; ulidx2++ )
					{
						
						if ( pulPlayersWhoVotedYes[ulidx2] != MAXPLAYERS )
						{
							ulNumYes++;
							if( pulPlayersWhoVotedYes[ulidx2] == ulPlayer )
								bWeVotedYes = true;
						}

						if ( pulPlayersWhoVotedNo[ulidx2] != MAXPLAYERS )
						{
							ulNumNo++;
							if( pulPlayersWhoVotedNo[ulidx2] == ulPlayer)
								bWeVotedNo = true;
						}
						
					}

					if( bWeVotedYes || bWeVotedNo )
					{
						char	szVoteString[8];

						sprintf( szVoteString, "(%s)", bWeVotedYes ? "yes" : "no" );
						lXPosOffset -= SmallFont->StringWidth ( szVoteString );
						if ( g_bScale )
						{
							screen->DrawText( SmallFont, ( CALLVOTE_GetVoteCaller() == ulPlayer ) ? CR_RED : CR_GOLD,
								(LONG)( g_aulColumnX[ulIdx] * g_fXScale ) + lXPosOffset,
								g_ulCurYPos,
								szVoteString,
								DTA_VirtualWidth, g_ValWidth.Int,
								DTA_VirtualHeight, g_ValHeight.Int,
								TAG_DONE );
						}
						else
						{
							screen->DrawText( SmallFont, ( static_cast<signed> (CALLVOTE_GetVoteCaller()) == consoleplayer ) ? CR_RED : CR_GOLD,
								(LONG)( g_aulColumnX[ulIdx] / 320.0f * SCREENWIDTH ) + lXPosOffset,
								g_ulCurYPos,
								szVoteString,
								DTA_Clean,
								g_bScale,
								TAG_DONE );
						}
						lXPosOffset -= 4;
					}
				}

				break;
			case COLUMN_TIME:	

				sprintf( szString, "%d", static_cast<unsigned int> (players[ulPlayer].ulTime / ( TICRATE * 60 )));
				break;
			case COLUMN_PING:

				sprintf( szString, "%d", static_cast<unsigned int> (players[ulPlayer].ulPing) );
				break;
			case COLUMN_FRAGS:

				sprintf( szString, "%d", players[ulPlayer].fragcount );

				// If the player isn't really playing, change this.
				if (( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS ) &&
					( players[ulPlayer].bOnTeam == false ))
				{
					sprintf( szString, "NO TEAM" );
				}
				if(PLAYER_IsTrueSpectator( &players[ulPlayer] ))
					sprintf(szString, "SPECT");

				if (( GAMEMODE_GetCurrentFlags() & GMF_DEADSPECTATORS ) &&
					(( players[ulPlayer].health <= 0 ) || ( players[ulPlayer].bDeadSpectator )) &&
					( gamestate != GS_INTERMISSION ))
				{
					sprintf( szString, "DEAD" );
				}
				break;
			case COLUMN_POINTS:

				sprintf( szString, "%d", static_cast<int> (players[ulPlayer].lPointCount) );
				
				// If the player isn't really playing, change this.
				if (( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS ) &&
					( players[ulPlayer].bOnTeam == false ))
				{
					sprintf( szString, "NO TEAM" );
				}
				if(PLAYER_IsTrueSpectator( &players[ulPlayer] ))
					sprintf(szString, "SPECT");

				if (( GAMEMODE_GetCurrentFlags() & GMF_DEADSPECTATORS ) &&
					(( players[ulPlayer].health <= 0 ) || ( players[ulPlayer].bDeadSpectator )) &&
					( gamestate != GS_INTERMISSION ))
				{
					sprintf( szString, "DEAD" );
				}
				break;

			case COLUMN_POINTSASSISTS:
				sprintf(szString, "%d / %d", static_cast<int> (players[ulPlayer].lPointCount), static_cast<unsigned int> (players[ulPlayer].ulMedalCount[14]));

				// If the player isn't really playing, change this.
				if (( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS ) &&
					( players[ulPlayer].bOnTeam == false ))
				{
					sprintf( szString, "NO TEAM" );
				}
				if(PLAYER_IsTrueSpectator( &players[ulPlayer] ))
					sprintf(szString, "SPECT");

				if (( GAMEMODE_GetCurrentFlags() & GMF_DEADSPECTATORS ) &&
					(( players[ulPlayer].health <= 0 ) || ( players[ulPlayer].bDeadSpectator )) &&
					( gamestate != GS_INTERMISSION ))
				{
					sprintf( szString, "DEAD" );
				}
				break;

			case COLUMN_DEATHS:
				sprintf(szString, "%d", static_cast<unsigned int> (players[ulPlayer].ulDeathCount));
				break;

			case COLUMN_WINS:
				sprintf(szString, "%d", static_cast<unsigned int> (players[ulPlayer].ulWins));

				// If the player isn't really playing, change this.
				if (( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS ) &&
					( players[ulPlayer].bOnTeam == false ))
				{
					sprintf( szString, "NO TEAM" );
				}
				if(PLAYER_IsTrueSpectator( &players[ulPlayer] ))
					sprintf(szString, "SPECT");

				if (( GAMEMODE_GetCurrentFlags() & GMF_DEADSPECTATORS ) &&
					(( players[ulPlayer].health <= 0 ) || ( players[ulPlayer].bDeadSpectator )) &&
					( gamestate != GS_INTERMISSION ))
				{
					sprintf( szString, "DEAD" );
				}
				break;

			case COLUMN_KILLS:
				sprintf(szString, "%d", players[ulPlayer].killcount);

				// If the player isn't really playing, change this.
				if(PLAYER_IsTrueSpectator( &players[ulPlayer] ))
					sprintf(szString, "SPECT");

				if (( GAMEMODE_GetCurrentFlags() & GMF_DEADSPECTATORS ) &&
					(( players[ulPlayer].health <= 0 ) || ( players[ulPlayer].bDeadSpectator )) &&
					( gamestate != GS_INTERMISSION ))
				{
					sprintf( szString, "DEAD" );
				}
				break;
			case COLUMN_SECRETS:
				sprintf(szString, "%d", players[ulPlayer].secretcount);
				// If the player isn't really playing, change this.
				if(PLAYER_IsTrueSpectator( &players[ulPlayer] ))
					sprintf(szString, "SPECT");
				if(((players[ulPlayer].health <= 0) || ( players[ulPlayer].bDeadSpectator ))
					&& (gamestate != GS_INTERMISSION))
				{
					sprintf(szString, "DEAD");
				}

				if (( GAMEMODE_GetCurrentFlags() & GMF_DEADSPECTATORS ) &&
					(( players[ulPlayer].health <= 0 ) || ( players[ulPlayer].bDeadSpectator )) &&
					( gamestate != GS_INTERMISSION ))
				{
					sprintf( szString, "DEAD" );
				}
				break;
				
		}

		if ( szString[0] != 0 )
		{
			if ( g_bScale )
			{
				screen->DrawText( SmallFont, ulColor,
						(LONG)( g_aulColumnX[ulIdx] * g_fXScale ),
						g_ulCurYPos,
						szString,
						DTA_VirtualWidth, g_ValWidth.Int,
						DTA_VirtualHeight, g_ValHeight.Int,
						TAG_DONE );
			}
			else
			{
				screen->DrawText( SmallFont, ulColor,
						(LONG)( g_aulColumnX[ulIdx] / 320.0f * SCREENWIDTH ),
						g_ulCurYPos,
						szString,
						TAG_DONE );
			}
		}
	}
}

//*****************************************************************************
//
static void scoreboard_DrawHeader( void )
{
	g_ulCurYPos = 4;

	// Don't draw it if we're in intermission.
	if ( gamestate == GS_LEVEL )
	{
		if ( g_bScale )
		{
			screen->DrawText( BigFont, gameinfo.gametype == GAME_Doom ? CR_RED : CR_UNTRANSLATED,
				(LONG)(( g_ValWidth.Int / 2 ) - ( BigFont->StringWidth( "RANKINGS" ) / 2 )),
				g_ulCurYPos,
				"RANKINGS",
				DTA_VirtualWidth, g_ValWidth.Int,
				DTA_VirtualHeight, g_ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( BigFont, gameinfo.gametype == GAME_Doom ? CR_RED : CR_UNTRANSLATED,
				( SCREENWIDTH / 2 ) - ( BigFont->StringWidth( "RANKINGS" ) / 2 ),
				g_ulCurYPos,
				"RANKINGS",
				TAG_DONE );
		}
	}

	g_ulCurYPos += 22;
}

//*****************************************************************************
// [RC] Helper method for SCOREBOARD_BuildLimitStrings. Creates a "x things remaining" message.
//
void scoreboard_AddSingleLimit( std::list<FString> &lines, bool condition, int remaining, const char *pszUnitName )
{
	if ( condition && remaining > 0 )
	{
		char	szString[128];
		sprintf( szString, "%d %s%s remain%s", static_cast<int> (remaining), pszUnitName, ( remaining == 1 ) ? "" : "s", ( remaining == 1 ) ? "s" : "" );
		lines.push_back( szString );
	}
}

//*****************************************************************************
//
// [RC] Builds the series of "x frags left / 3rd match between the two / 15:10 remain" strings. Used here and in serverconsole.cpp
//
void SCOREBOARD_BuildLimitStrings( std::list<FString> &lines, bool bAcceptColors )
{
	char	szString[128];

	if ( gamestate != GS_LEVEL )
		return;

	LONG remaining = SCOREBOARD_GetLeftToLimit( );

	// Build the fraglimit and/or duellimit strings.
	scoreboard_AddSingleLimit( lines, ( fraglimit && GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNFRAGS ), remaining, "frag" );
	// [TL] The number of duels left is the maximum number of duels less the number of duels fought.
	scoreboard_AddSingleLimit( lines, ( duellimit && duel ), duellimit - DUEL_GetNumDuels( ), "duel" );

	// Build the "wins" string.
	if ( duel && duellimit )
	{
		LONG lWinner = -1;
		for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			if ( playeringame[ulIdx] && players[ulIdx].ulWins )
			{
				lWinner = ulIdx;
				break;
			}
		}

		bool bDraw = true;
		if ( lWinner == -1 )
		{
			if ( DUEL_CountActiveDuelers( ) == 2 )
				sprintf( szString, "First match between the two" );
			else
				bDraw = false;
		}
		else
			sprintf( szString, "Champion is %s \\c-with %d win%s", players[lWinner].userinfo.GetName(), static_cast<unsigned int> (players[lWinner].ulWins), players[lWinner].ulWins == 1 ? "" : "s" );

		if ( bDraw )
		{
			V_ColorizeString( szString );
			if ( !bAcceptColors )
				V_StripColors( szString );
			lines.push_back( szString );
		}
	}

	// Build the pointlimit, winlimit, and/or wavelimit strings.
	scoreboard_AddSingleLimit( lines, ( pointlimit && GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNPOINTS ), remaining, "point" );
	scoreboard_AddSingleLimit( lines, ( winlimit && GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNWINS ), remaining, "win" );
	scoreboard_AddSingleLimit( lines, ( invasion && wavelimit ), wavelimit - INVASION_GetCurrentWave( ), "wave" );

	// Render the timelimit string. - [BB] if the gamemode uses it.
	if ( GAMEMODE_IsTimelimitActive() )
	{
		FString TimeLeftString;
		GAMEMODE_GetTimeLeftString ( TimeLeftString );
		const char *szRound = ( lastmanstanding || teamlms ) ? "Round" : "Level";
		sprintf( szString, "%s ends in %s", szRound, TimeLeftString.GetChars() );
		lines.push_back( szString );
	}

	// Render the number of monsters left in coop.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNKILLS )
	{
		if ( dmflags2 & DF2_KILL_MONSTERS )
			sprintf( szString, "%d%% remaining", static_cast<int> (remaining) );		
		else
			sprintf( szString, "%d monster%s remaining", static_cast<int> (remaining), remaining == 1 ? "" : "s" );
		lines.push_back( szString );

		// [WS] Show the damage factor.
		if ( sv_coop_damagefactor != 1.0f )
		{
			sprintf( szString, "damage factor %.2f", static_cast<float> (sv_coop_damagefactor) );
			lines.push_back( szString );
		}
	}
}

//*****************************************************************************
//
static void scoreboard_DrawLimits( void )
{
	// Generate the strings.
	std::list<FString> lines;
	SCOREBOARD_BuildLimitStrings( lines, true );

	// Now, draw them.
	for ( std::list<FString>::iterator i = lines.begin(); i != lines.end(); ++i )
	{
		if ( g_bScale )
		{
			screen->DrawText( SmallFont, CR_GREY, (LONG)(( g_ValWidth.Int / 2 ) - ( SmallFont->StringWidth( *i ) / 2 )),
				g_ulCurYPos, *i, DTA_VirtualWidth, g_ValWidth.Int, DTA_VirtualHeight, g_ValHeight.Int, TAG_DONE );
		}
		else
			screen->DrawText( SmallFont, CR_GREY, ( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( *i ) / 2 ), g_ulCurYPos, *i, TAG_DONE );

		g_ulCurYPos += 10;
	}
}

//*****************************************************************************
//
static void scoreboard_DrawTeamScores( ULONG ulPlayer )
{
	char	szString[128];

	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
	{
		if ( gamestate != GS_LEVEL )
			g_ulCurYPos += 10;

		if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNFRAGS )
			SCOREBOARD_BuildPointString( szString, "frag", &TEAM_CheckAllTeamsHaveEqualFrags, &TEAM_GetHighestFragCount, &TEAM_GetFragCount );
		else if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNWINS )
			SCOREBOARD_BuildPointString( szString, "win", &TEAM_CheckAllTeamsHaveEqualWins, &TEAM_GetHighestWinCount, &TEAM_GetWinCount );
		else
			SCOREBOARD_BuildPointString( szString, "score", &TEAM_CheckAllTeamsHaveEqualScores, &TEAM_GetHighestScoreCount, &TEAM_GetScore );

		V_ColorizeString ( szString );

		if ( g_bScale )
		{
			screen->DrawText( SmallFont, CR_GREY,
				(LONG)(( g_ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
				g_ulCurYPos,
				szString,
				DTA_VirtualWidth, g_ValWidth.Int,
				DTA_VirtualHeight, g_ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( SmallFont, CR_GREY,
				( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
				g_ulCurYPos,
				szString,
				TAG_DONE );
		}

		g_ulCurYPos += 10;
	}
}

//*****************************************************************************
//
// [TP]
//
bool SCOREBOARD_ShouldDrawRank( ULONG player )
{
	return deathmatch
		&& ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS ) == 0
		&& ( PLAYER_IsTrueSpectator( &players[player] ) == false );
}

//*****************************************************************************
//
static void scoreboard_DrawMyRank( ULONG ulPlayer )
{
	char	szString[128];
	bool	bIsTied;

	// Render the current ranking string.
	if ( deathmatch && !( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS ) && ( PLAYER_IsTrueSpectator( &players[ulPlayer] ) == false ))
	{
		bIsTied	= SCOREBOARD_IsTied( ulPlayer );

		// If the player is tied with someone else, add a "tied for" to their string.
		if ( bIsTied )
			sprintf( szString, "Tied for " );
		else
			szString[0] = 0;

		// Determine  what color and number to print for their rank.
		strcpy( szString + strlen ( szString ), SCOREBOARD_SpellOrdinalColored( g_ulRank ));

		// Tack on the rest of the string.
		if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNWINS )
			sprintf( szString + strlen ( szString ), "\\c- place with %d win%s",  static_cast<unsigned int> (players[ulPlayer].ulWins), players[ulPlayer].ulWins == 1 ? "" : "s" );
		else if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNPOINTS )
			sprintf( szString + strlen ( szString ), "\\c- place with %d point%s", static_cast<int> (players[ulPlayer].lPointCount), players[ulPlayer].lPointCount == 1 ? "" : "s" );
		else
			sprintf( szString + strlen ( szString ), "\\c- place with %d frag%s", players[ulPlayer].fragcount, players[ulPlayer].fragcount == 1 ? "" : "s" );
		V_ColorizeString( szString );

		if ( g_bScale )
		{
			screen->DrawText( SmallFont, CR_GREY,
				(LONG)(( g_ValWidth.Int / 2 ) - ( SmallFont->StringWidth( szString ) / 2 )),
				g_ulCurYPos,
				szString,
				DTA_VirtualWidth, g_ValWidth.Int,
				DTA_VirtualHeight, g_ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( SmallFont, CR_GREY,
				( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( szString ) / 2 ),
				g_ulCurYPos,
				szString,
				TAG_DONE );
		}

		g_ulCurYPos += 10;
	}
}

//*****************************************************************************
//
static void scoreboard_ClearColumns( void )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAX_COLUMNS; ulIdx++ )
		g_aulColumnType[ulIdx] = COLUMN_EMPTY;

	g_ulNumColumnsUsed = 0;
}

//*****************************************************************************
//
static void scoreboard_Prepare5ColumnDisplay( void )
{
	// Set all to empty.
	scoreboard_ClearColumns( );

	g_ulNumColumnsUsed = 5;
	g_pColumnHeaderFont = BigFont;

	// Set up the location of each column.
	g_aulColumnX[0] = 8;
	g_aulColumnX[1] = 56;
	g_aulColumnX[2] = 106;
	g_aulColumnX[3] = 222;
	g_aulColumnX[4] = 286;

	// Build columns for modes in which players try to earn points.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNPOINTS )
	{
		g_aulColumnType[0] = COLUMN_POINTS;
		// [BC] Doesn't look like this is being used right now (at least not properly).
/*
		// Can have assists.
		if ( ctf || skulltag )
			g_aulColumnType[0] = COL_POINTSASSISTS;
*/
		g_aulColumnType[1] = COLUMN_FRAGS;
		g_aulColumnType[2] = COLUMN_NAME;
		g_aulColumnType[3] = COLUMN_DEATHS;
		if ( NETWORK_InClientMode() )
			g_aulColumnType[3] = COLUMN_PING;
		g_aulColumnType[4] = COLUMN_TIME;

		// Sort players based on their pointcount.
		scoreboard_SortPlayers( ST_POINTCOUNT );
	}

	// Build columns for modes in which players try to earn wins.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNWINS )
	{
		g_aulColumnType[0] = COLUMN_WINS;
		g_aulColumnType[1] = COLUMN_FRAGS;
		g_aulColumnType[2] = COLUMN_NAME;
		g_aulColumnType[3] = COLUMN_EMPTY;
		if ( NETWORK_InClientMode() )
			g_aulColumnType[3] = COLUMN_PING;
		g_aulColumnType[4] = COLUMN_TIME;

		// Sort players based on their pointcount.
		scoreboard_SortPlayers( ST_WINCOUNT );
	}
}

//*****************************************************************************
//
static void scoreboard_SetColumnZeroToKillsAndSortPlayers( void )
{
	if ( zadmflags & ZADF_AWARD_DAMAGE_INSTEAD_KILLS )
	{
		g_aulColumnType[0] = COLUMN_POINTS;

		// Sort players based on their points.
		scoreboard_SortPlayers( ST_POINTCOUNT );
	}
	else
	{
		g_aulColumnType[0] = COLUMN_KILLS;

		// Sort players based on their killcount.
		scoreboard_SortPlayers( ST_KILLCOUNT );
	}
}
//*****************************************************************************
//
static void scoreboard_Prepare4ColumnDisplay( void )
{
	// Set all to empty.
	scoreboard_ClearColumns( );

	g_ulNumColumnsUsed = 4;
	g_pColumnHeaderFont = BigFont;

	// Set up the location of each column.
	g_aulColumnX[0] = 24;
	g_aulColumnX[1] = 84;
	g_aulColumnX[2] = 192;
	g_aulColumnX[3] = 256;
	
	// Build columns for modes in which players try to earn kills.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNKILLS )
	{
		scoreboard_SetColumnZeroToKillsAndSortPlayers();
		g_aulColumnType[1] = COLUMN_NAME;
		g_aulColumnType[2] = COLUMN_DEATHS;
		if ( NETWORK_InClientMode() )
			g_aulColumnType[2] = COLUMN_PING;
		g_aulColumnType[3] = COLUMN_TIME;
	}

	// Build columns for modes in which players try to earn frags.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNFRAGS )
	{
		g_aulColumnType[0] = COLUMN_FRAGS;
		g_aulColumnType[1] = COLUMN_NAME;
		g_aulColumnType[2] = COLUMN_DEATHS;
		if ( NETWORK_InClientMode() )
			g_aulColumnType[2] = COLUMN_PING;
		g_aulColumnType[3] = COLUMN_TIME;

		// Sort players based on their fragcount.
		scoreboard_SortPlayers( ST_FRAGCOUNT );
	}
	
	// Build columns for modes in which players try to earn points.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNPOINTS )
	{
//		if ( ctf || skulltag ) // Can have assists
//			g_aulColumnType[0] = COL_POINTSASSISTS;

		g_aulColumnType[0] = COLUMN_POINTS;
		g_aulColumnType[1] = COLUMN_NAME;
		g_aulColumnType[2] = COLUMN_DEATHS;
		if ( NETWORK_InClientMode() )
			g_aulColumnType[2] = COLUMN_PING;
		g_aulColumnType[3] = COLUMN_TIME;

		// Sort players based on their pointcount.
		scoreboard_SortPlayers( ST_POINTCOUNT );
	}

	// Build columns for modes in which players try to earn wins.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNWINS )
	{
		g_aulColumnType[0] = COLUMN_WINS;
		g_aulColumnType[1] = COLUMN_NAME;
		g_aulColumnType[2] = COLUMN_FRAGS;
		if ( NETWORK_InClientMode() )
			g_aulColumnType[2] = COLUMN_PING;
		g_aulColumnType[3] = COLUMN_TIME;

		// Sort players based on their wincount.
		scoreboard_SortPlayers( ST_WINCOUNT );
	}

}

//*****************************************************************************
//
static void scoreboard_Prepare3ColumnDisplay( void )
{
	// Set all to empty.
	scoreboard_ClearColumns( );

	g_ulNumColumnsUsed = 3;
	g_pColumnHeaderFont = SmallFont;

	// Set up the location of each column.
	g_aulColumnX[0] = 16;
	g_aulColumnX[1] = 96;
	g_aulColumnX[2] = 272;

	// All boards share these two columns. However, you can still deviant on these columns if you want.
	g_aulColumnType[1] = COLUMN_NAME;
	g_aulColumnType[2] = COLUMN_TIME;
	if ( NETWORK_InClientMode() )
		g_aulColumnType[2] = COLUMN_PING;

	// Build columns for modes in which players try to earn kills.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNKILLS )
	{
		scoreboard_SetColumnZeroToKillsAndSortPlayers();
	}

	// Build columns for modes in which players try to earn frags.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNFRAGS )
	{
		g_aulColumnType[0] = COLUMN_FRAGS;

		// Sort players based on their fragcount.
		scoreboard_SortPlayers( ST_FRAGCOUNT );
	}
	
	// Build columns for modes in which players try to earn points.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNPOINTS )
	{
//		if ( ctf || skulltag ) // Can have assists
//			g_aulColumnType[0] = COL_POINTSASSISTS;

		g_aulColumnType[0] = COLUMN_POINTS;

		// Sort players based on their pointcount.
		scoreboard_SortPlayers( ST_POINTCOUNT );
	}

	// Build columns for modes in which players try to earn wins.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNWINS )
	{
		g_aulColumnType[0] = COLUMN_WINS;

		// Sort players based on their wincount.
		scoreboard_SortPlayers( ST_WINCOUNT );
	}

}

//*****************************************************************************
//	These parameters are filters.
//	If 1, players with this trait will be skipped.
//	If 2, players *without* this trait will be skipped.
static void scoreboard_DoRankingListPass( ULONG ulPlayer, LONG lSpectators, LONG lDead, LONG lNotPlaying, LONG lNoTeam, LONG lWrongTeam, ULONG ulDesiredTeam )
{
	ULONG	ulIdx;
	ULONG	ulNumPlayers;

	ulNumPlayers = 0;
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		// Skip or require players not in the game.
		if (((lNotPlaying == 1) && (playeringame[g_iSortedPlayers[ulIdx]] == false )) ||
			((lNotPlaying == 2) && (!playeringame[g_iSortedPlayers[ulIdx]] == false )))
			continue;

		// Skip or require players not on a team.
		 if(((lNoTeam == 1) && (!players[g_iSortedPlayers[ulIdx]].bOnTeam)) ||
			 ((lNoTeam == 2) && (players[g_iSortedPlayers[ulIdx]].bOnTeam)))
			continue;

		// Skip or require spectators.
		if (((lSpectators == 1) && PLAYER_IsTrueSpectator( &players[g_iSortedPlayers[ulIdx]])) ||
			((lSpectators == 2) && !PLAYER_IsTrueSpectator( &players[g_iSortedPlayers[ulIdx]])))
			continue;

		// In LMS, skip or require dead players.
		if( gamestate != GS_INTERMISSION ){
			/*(( lastmanstanding ) && (( LASTMANSTANDING_GetState( ) == LMSS_INPROGRESS ) || ( LASTMANSTANDING_GetState( ) == LMSS_WINSEQUENCE ))) ||
			(( survival ) && (( SURVIVAL_GetState( ) == SURVS_INPROGRESS ) || ( SURVIVAL_GetState( ) == SURVS_MISSIONFAILED )))*/
			
			// If we don't want to draw dead players, and this player is dead, skip this player.
			if (( lDead == 1 ) &&
				(( players[g_iSortedPlayers[ulIdx]].health <= 0 ) || ( players[g_iSortedPlayers[ulIdx]].bDeadSpectator )))
			{
				continue;
			}

			// If we don't want to draw living players, and this player is alive, skip this player.
			if (( lDead == 2 ) &&
				( players[g_iSortedPlayers[ulIdx]].health > 0 ) &&
				( players[g_iSortedPlayers[ulIdx]].bDeadSpectator == false ))
			{
				continue;
			}
		}

		// Skip or require players that aren't on this team.
		if (((lWrongTeam == 1) && (players[g_iSortedPlayers[ulIdx]].ulTeam != ulDesiredTeam)) ||
			((lWrongTeam == 2) && (players[g_iSortedPlayers[ulIdx]].ulTeam == ulDesiredTeam)))
			continue;

		scoreboard_RenderIndividualPlayer( ulPlayer, g_iSortedPlayers[ulIdx] );
		g_ulCurYPos += 10;
		ulNumPlayers++;
	}

	if ( ulNumPlayers )
		g_ulCurYPos += 10;
}

//*****************************************************************************
//
static void scoreboard_DrawRankings( ULONG ulPlayer )
{
	ULONG	ulIdx;
	ULONG	ulTeamIdx;
	char	szString[16];

	// Nothing to do.
	if ( g_ulNumColumnsUsed < 1 )
		return;

	g_ulCurYPos += 8;

	// Center this a little better in intermission
	if ( gamestate != GS_LEVEL )
		g_ulCurYPos = ( g_bScale == true ) ? (LONG)( 48 * g_fYScale ) : (LONG)( 48 * CleanYfac );

	// Draw the titles for the columns.
	for ( ulIdx = 0; ulIdx < g_ulNumColumnsUsed; ulIdx++ )
	{
		sprintf( szString, "%s", g_pszColumnHeaders[g_aulColumnType[ulIdx]] );
		if ( g_bScale )
		{
			screen->DrawText( g_pColumnHeaderFont, CR_RED,
				(LONG)( g_aulColumnX[ulIdx] * g_fXScale ),
				g_ulCurYPos,
				szString,
				DTA_VirtualWidth, g_ValWidth.Int,
				DTA_VirtualHeight, g_ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( g_pColumnHeaderFont, CR_RED,
				(LONG)( g_aulColumnX[ulIdx] / 320.0f * SCREENWIDTH ),
				g_ulCurYPos,
				szString,
				TAG_DONE );
		}
	}

	// Draw the player list.
	g_ulCurYPos += 24;

	// Team-based games: Divide up the teams.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
	{
		// Draw players on teams.
		for ( ulTeamIdx = 0; ulTeamIdx < teams.Size( ); ulTeamIdx++ )
		{
			// In team LMS, separate the dead players from the living.
			if (( teamlms ) && ( gamestate != GS_INTERMISSION ) && ( LASTMANSTANDING_GetState( ) != LMSS_COUNTDOWN ) && ( LASTMANSTANDING_GetState( ) != LMSS_WAITINGFORPLAYERS ))
			{
				scoreboard_DoRankingListPass( ulPlayer, 1, 1, 1, 1, 1, ulTeamIdx ); // Living in this team
				scoreboard_DoRankingListPass( ulPlayer, 1, 2, 1, 1, 1, ulTeamIdx ); // Dead in this team
			}
			// Otherwise, draw all players all in one group.
			else
				scoreboard_DoRankingListPass( ulPlayer, 1, 0, 1, 1, 1, ulTeamIdx ); 

		}

		// Players that aren't on a team.
		scoreboard_DoRankingListPass( ulPlayer, 1, 1, 1, 2, 0, 0 ); 

		// Spectators are last.
		scoreboard_DoRankingListPass( ulPlayer, 2, 0, 1, 0, 0, 0 );
	}
	// Other modes: Just players and spectators.
	else
	{
		// [WS] Does the gamemode we are in use lives?
		// If so, dead players are drawn after living ones.
		if (( gamestate != GS_INTERMISSION ) && GAMEMODE_AreLivesLimited( ) && GAMEMODE_IsGameInProgress( ) )
		{
			scoreboard_DoRankingListPass( ulPlayer, 1, 1, 1, 0, 0, 0 ); // Living
			scoreboard_DoRankingListPass( ulPlayer, 1, 2, 1, 0, 0, 0 ); // Dead
		}
		// Othrwise, draw all active players in the game together.
		else
			scoreboard_DoRankingListPass( ulPlayer, 1, 0, 1, 0, 0, 0 );

		// Spectators are last.
		scoreboard_DoRankingListPass( ulPlayer, 2, 0, 1, 0, 0, 0 );
	}

	V_SetBorderNeedRefresh();
}
