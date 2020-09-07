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
// Filename: medal.cpp
//
// Description: Contains medal routines and globals
//
//-----------------------------------------------------------------------------

#include "a_sharedglobal.h"
#include "announcer.h"
#include "chat.h"
#include "cl_demo.h"
#include "deathmatch.h"
#include "doomstat.h"
#include "duel.h"
#include "gi.h"
#include "gamemode.h"
#include "g_level.h"
#include "info.h"
#include "medal.h"
#include "p_local.h"
#include "d_player.h"
#include "network.h"
#include "s_sound.h"
#include "st_stuff.h"
#include "sv_commands.h"
#include "team.h"
#include "templates.h"
#include "v_text.h"
#include "v_video.h"
#include "w_wad.h"
#include "scoreboard.h"
#include "p_acs.h"

//*****************************************************************************
//	VARIABLES

static	MEDAL_t	g_Medals[NUM_MEDALS] =
{
	{ "EXCLA0", S_EXCELLENT, "Excellent!", CR_GRAY, "Excellent", NUM_MEDALS, "",	},
	{ "INCRA0", S_INCREDIBLE, "Incredible!", CR_RED, "Incredible", MEDAL_EXCELLENT, "", },

	{ "IMPRA0", S_IMPRESSIVE, "Impressive!", CR_GRAY, "Impressive", NUM_MEDALS, "", },
	{ "MIMPA0", S_MOST_IMPRESSIVE, "Most impressive!", CR_RED, "MostImpressive", MEDAL_IMPRESSIVE, "", },

	{ "DOMNA0", S_DOMINATION, "Domination!", CR_GRAY, "Domination", NUM_MEDALS, "", },
	{ "TDOMA0", S_TOTAL_DOMINATION, "Total domination!", CR_RED, "TotalDomination", MEDAL_DOMINATION, "", },

	{ "ACCUA0", S_ACCURACY, "Accuracy!", CR_GRAY, "Accuracy", NUM_MEDALS, "", },
	{ "PRECA0", S_PRECISION, "Precision!", CR_RED, "Precision", MEDAL_ACCURACY, "", },

	{ "FAILA0", S_YOUFAILIT, "You fail it!", CR_GREEN, "YouFailIt", NUM_MEDALS, "", },
	{ "SKILA0", S_YOURSKILLISNOTENOUGH, "Your skill is not enough!", CR_ORANGE, "YourSkillIsNotEnough", MEDAL_YOUFAILIT, "", },

	{ "LLAMA0", S_LLAMA, "Llama!", CR_GREEN, "Llama", NUM_MEDALS, "misc/llama", },
	{ "SPAMA0", S_SPAM, "Spam!", CR_GREEN, "Spam", MEDAL_LLAMA, "misc/spam", },

	{ "VICTA0", S_VICTORY, "Victory!", CR_GRAY, "Victory", NUM_MEDALS, "", },
	{ "PFCTA0", S_PERFECT, "Perfect!", CR_RED, "Perfect", MEDAL_VICTORY, "", },

	{ "TRMAA0", S_TERMINATION, "Termination!", CR_GRAY, "Termination", NUM_MEDALS, "", },
	{ "FFRGA0", S_FIRSTFRAG, "First frag!", CR_GRAY, "FirstFrag", NUM_MEDALS, "", },
	{ "CAPTA0", S_CAPTURE, "Capture!", CR_GRAY, "Capture", NUM_MEDALS, "", },
	{ "STAGA0", S_TAG, "Tag!", CR_GRAY, "Tag", NUM_MEDALS, "", },
	{ "ASSTA0", S_ASSIST, "Assist!", CR_GRAY, "Assist", NUM_MEDALS, "", },
	{ "DFNSA0", S_DEFENSE, "Defense!", CR_GRAY, "Defense", NUM_MEDALS, "", },
	{ "FISTA0", S_FISTING, "Fisting!", CR_GRAY, "Fisting", NUM_MEDALS, "", },
};

enum
{
	SPRITE_CHAT,
	SPRITE_INCONSOLE,
	SPRITE_ALLY,
	SPRITE_LAG,
	SPRITE_WHITEFLAG,
	SPRITE_TERMINATORARTIFACT,
	SPRITE_POSSESSIONARTIFACT,
	SPRITE_TEAMITEM,
	NUM_SPRITES
};

static	MEDALQUEUE_t	g_MedalQueue[MAXPLAYERS][MEDALQUEUE_DEPTH];

// Has the first frag medal been awarded this round?
static	bool			g_bFirstFragAwarded;

// [Dusk] Need this from p_interaction.cpp for spawn telefrag checking
extern FName MeansOfDeath;

//*****************************************************************************
//	CONSOLE VARIABLES

CVAR( Bool, cl_medals, true, CVAR_ARCHIVE )
CVAR( Bool, cl_icons, true, CVAR_ARCHIVE )

//*****************************************************************************
//	PROTOTYPES

ULONG	medal_AddToQueue( ULONG ulPlayer, ULONG ulMedal );
void	medal_PopQueue( ULONG ulPlayer );
ULONG	medal_GetDesiredIcon( player_t *pPlayer, AInventory *&pTeamItem );
void	medal_TriggerMedal( ULONG ulPlayer, ULONG ulMedal );
void	medal_SelectIcon( ULONG ulPlayer );
void	medal_GiveMedal( ULONG ulPlayer, ULONG ulMedal );
void	medal_CheckForFirstFrag( ULONG ulPlayer );
void	medal_CheckForDomination( ULONG ulPlayer );
void	medal_CheckForFisting( ULONG ulPlayer );
void	medal_CheckForExcellent( ULONG ulPlayer );
void	medal_CheckForTermination( ULONG ulDeadPlayer, ULONG ulPlayer );
void	medal_CheckForLlama( ULONG ulDeadPlayer, ULONG ulPlayer );
void	medal_CheckForYouFailIt( ULONG ulPlayer );
bool	medal_PlayerHasCarrierIcon( ULONG ulPlayer );

//*****************************************************************************
//	FUNCTIONS

void MEDAL_Construct( void )
{
	ULONG	ulIdx;
	ULONG	ulQueueIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		for ( ulQueueIdx = 0; ulQueueIdx < MEDALQUEUE_DEPTH; ulQueueIdx++ )
		{
			g_MedalQueue[ulIdx][ulQueueIdx].ulMedal = 0;
			g_MedalQueue[ulIdx][ulQueueIdx].ulTick = 0;
		}
	}

	g_bFirstFragAwarded = false;
}

//*****************************************************************************
//
void MEDAL_Tick( void )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		// No need to do anything.
		if ( playeringame[ulIdx] == false )
			continue;

		// Tick down the duration of the medal on the top of the queue.
		if ( g_MedalQueue[ulIdx][0].ulTick )
		{
			// If time has expired on this medal, pop it and potentially trigger a new one.
			if ( --g_MedalQueue[ulIdx][0].ulTick == 0 )
				medal_PopQueue( ulIdx );
		}

		// [BB] We don't need to know what medal_GetDesiredIcon puts into pTeamItem, but we still need to supply it as argument.
		AInventory *pTeamItem;
		const ULONG ulDesiredSprite = medal_GetDesiredIcon( &players[ulIdx], pTeamItem );

		// If we're not currently displaying a medal for the player, potentially display
		// some other type of icon.
		// [BB] Also let carrier icons override medals.
		if ( ( g_MedalQueue[ulIdx][0].ulTick == 0 ) || ( ( ulDesiredSprite >= SPRITE_WHITEFLAG ) && ( ulDesiredSprite <= SPRITE_TEAMITEM ) ) )
			medal_SelectIcon( ulIdx );

		// [BB] If the player is being awarded a medal at the moment but has no icon, restore the medal.
		// This happens when the player respawns while being awarded a medal.
		if ( ( g_MedalQueue[ulIdx][0].ulTick > 0 ) && ( players[ulIdx].pIcon == NULL ) )
			medal_TriggerMedal( ulIdx, g_MedalQueue[ulIdx][0].ulMedal );

		// [BB] Remove any old carrier icons.
		medal_PlayerHasCarrierIcon ( ulIdx );

		// Don't render icons floating above our own heads.
		if ( players[ulIdx].pIcon )
		{
			if (( players[ulIdx].mo->CheckLocalView( consoleplayer )) && (( players[ulIdx].cheats & CF_CHASECAM ) == false ))
				players[ulIdx].pIcon->renderflags |= RF_INVISIBLE;
			else
				players[ulIdx].pIcon->renderflags &= ~RF_INVISIBLE;
		}
	}
}

//*****************************************************************************
//
void MEDAL_Render( void )
{
	player_t	*pPlayer;
	ULONG		ulPlayer;
	ULONG		ulMedal;
	ULONG		ulTick;
	FString		string;
	FString		patchName;
	long		lAlpha = OPAQUE;

	if ( players[consoleplayer].camera == NULL )
		return;

	pPlayer = players[consoleplayer].camera->player;
	if ( pPlayer == NULL )
		return;

	ulPlayer = ULONG( pPlayer - players );

	// [TP] Sanity check
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	// If the player doesn't have a medal to be drawn, don't do anything.
	if ( g_MedalQueue[ulPlayer][0].ulTick == 0 )
		return;

	ulMedal	= g_MedalQueue[ulPlayer][0].ulMedal;
	ulTick	= g_MedalQueue[ulPlayer][0].ulTick;

	if ( ulTick > ( 1 * TICRATE ))
		lAlpha = static_cast<long> ( OPAQUE * ((float) ulTick / TICRATE ) );

	// Get the graphic from the global array.
	patchName = g_Medals[ulMedal].szLumpName;
	if ( patchName.IsNotEmpty() )
	{
		ULONG	ulCurXPos;
		ULONG	ulCurYPos;
		ULONG	ulNumMedals = pPlayer->ulMedalCount[ulMedal];
		ULONG	ulLength;

		// Determine how much actual screen space it will take to render the amount of
		// medals the player has received up until this point.
		ulLength = ulNumMedals * TexMan[patchName]->GetWidth( );

		if ( viewheight <= ST_Y )
			ulCurYPos = ((int)( ST_Y - 11 * CleanYfac ));
		else
			ulCurYPos = ((int)( SCREENHEIGHT - 11 * CleanYfac ));

		// If that length is greater then the screen width, display the medals as "<icon> <name> X <num>"
		if ( ulLength >= 320 )
		{
			ulCurXPos = SCREENWIDTH / 2;

			const char* szColor1 = TEXTCOLOR_WHITE,
				*szColor2 = TEXTCOLOR_RED;

			if ( g_Medals[ulMedal].ulTextColor == CR_RED )
			{
				szColor1 = TEXTCOLOR_RED;
				szColor2 = TEXTCOLOR_WHITE;
			}
			else if ( g_Medals[ulMedal].ulTextColor == CR_GREEN )
				szColor1 = TEXTCOLOR_GREEN;

			string.Format( "%s%s%s X %lu", szColor1, g_Medals[ulMedal].szStr, szColor2, ulNumMedals );

			screen->DrawTexture( TexMan[patchName], ulCurXPos, ulCurYPos,
				DTA_CleanNoMove, true,
				DTA_Alpha, lAlpha,
				TAG_DONE );

			ulCurXPos = SCREENWIDTH / 2 - CleanXfac * ( SmallFont->StringWidth( string ) / 2 );

			screen->DrawText( SmallFont, g_Medals[ulMedal].ulTextColor, ulCurXPos, ulCurYPos, string,
				DTA_CleanNoMove, true,
				DTA_Alpha, lAlpha,
				TAG_DONE );
		}
		// Display the medal icon <usNumMedals> times centered on the screen.
		else
		{
			ULONG	ulIdx;

			ulCurXPos = SCREENWIDTH / 2 - ( CleanXfac * ulLength ) / 2;
			string = g_Medals[ulMedal].szStr;

			for ( ulIdx = 0; ulIdx < ulNumMedals; ulIdx++ )
			{
				screen->DrawTexture( TexMan[patchName],
					ulCurXPos + CleanXfac * ( TexMan[patchName]->GetWidth( ) / 2 ),
					ulCurYPos,
					DTA_CleanNoMove, true,
					DTA_Alpha, lAlpha,
					TAG_DONE );

				ulCurXPos += CleanXfac * TexMan[patchName]->GetWidth( );
			}

			ulCurXPos = SCREENWIDTH / 2 - CleanXfac * ( SmallFont->StringWidth( string ) / 2 );

			screen->DrawText( SmallFont, g_Medals[ulMedal].ulTextColor, ulCurXPos, ulCurYPos, string,
				DTA_CleanNoMove, true,
				DTA_Alpha, lAlpha,
				TAG_DONE );
		}
	}
}

//*****************************************************************************
//*****************************************************************************
//
void MEDAL_GiveMedal( ULONG ulPlayer, ULONG ulMedal )
{
	player_t	*pPlayer;
	ULONG		ulWhereToInsertMedal = UINT_MAX;

	// [CK] Do not award if it's a countdown sequence
	if ( GAMEMODE_IsGameInCountdown() )
		return;

	// Make sure all inputs are valid first.
	if (( ulPlayer >= MAXPLAYERS ) ||
		(( deathmatch || teamgame ) == false ) ||
		( players[ulPlayer].mo == NULL ) ||
		( cl_medals == false ) ||
		( zadmflags & ZADF_NO_MEDALS ) ||
		( ulMedal >= NUM_MEDALS ))
	{
		return;
	}

	pPlayer = &players[ulPlayer];

	// [CK] Trigger events if a medal is received
	GAMEMODE_HandleEvent ( GAMEEVENT_MEDALS, pPlayer->mo, ACS_PushAndReturnDynamicString ( g_Medals[ulMedal].szAnnouncerEntry, NULL, 0 ) );

	// Increase the player's count of this type of medal.
	pPlayer->ulMedalCount[ulMedal]++;

	// The queue is empty, so put this medal first.
	if ( !g_MedalQueue[ulPlayer][0].ulTick )
		ulWhereToInsertMedal = 0;
	else
	{
		// Go through the queue.
		ULONG ulQueueIdx = 0;
		while ( ulQueueIdx < MEDALQUEUE_DEPTH - 1 && g_MedalQueue[ulPlayer][ulQueueIdx].ulTick )
		{
			// Is this a duplicate/suboordinate of the new medal?
			if ( g_MedalQueue[ulPlayer][ulQueueIdx].ulMedal == g_Medals[ulMedal].ulLowerMedal ||
				 g_MedalQueue[ulPlayer][ulQueueIdx].ulMedal == ulMedal )
			{
				// Commandeer its slot!
				if ( ulWhereToInsertMedal == UINT_MAX )
				{
					ulWhereToInsertMedal = ulQueueIdx;
					ulQueueIdx++;
				}

				// Or remove it, as it's a duplicate.
				else
				{
					// [BB] This is not the most optimal way to remove the medal because also empty slots are copied.
					for ( ULONG ulIdx = ulQueueIdx; ulIdx < MEDALQUEUE_DEPTH - 1; ++ulIdx ) {
						g_MedalQueue[ulPlayer][ulIdx].ulMedal		= g_MedalQueue[ulPlayer][ulIdx + 1].ulMedal;
						g_MedalQueue[ulPlayer][ulIdx].ulTick		= g_MedalQueue[ulPlayer][ulIdx + 1].ulTick;
					}
					g_MedalQueue[ulPlayer][MEDALQUEUE_DEPTH - 1].ulMedal = 0;
					g_MedalQueue[ulPlayer][MEDALQUEUE_DEPTH - 1].ulTick = 0;
				}
			}
			else
				ulQueueIdx++;
		}
	}

	// [RC] No special place for the medal, so just queue it.
	if ( ulWhereToInsertMedal == UINT_MAX )
		medal_AddToQueue( ulPlayer, ulMedal );
	else
	{
		// Update the slot in line.
		g_MedalQueue[ulPlayer][ulWhereToInsertMedal].ulTick	= MEDAL_ICON_DURATION;
		g_MedalQueue[ulPlayer][ulWhereToInsertMedal].ulMedal	= ulMedal;

		// If it's replacing/is the medal on top, play the sound.
		if ( ulWhereToInsertMedal == 0 )
		{
			medal_TriggerMedal( ulPlayer, ulMedal );
		}
	}

	// If this player is a bot, tell it that it received a medal.
	if ( players[ulPlayer].pSkullBot )
	{
		players[ulPlayer].pSkullBot->m_ulLastMedalReceived = ulMedal;
		players[ulPlayer].pSkullBot->PostEvent( BOTEVENT_RECEIVEDMEDAL );
	}
}

//*****************************************************************************
//
void MEDAL_RenderAllMedals( LONG lYOffset )
{
	player_t	*pPlayer;
	ULONG		ulPlayer;
	ULONG		ulCurXPos;
	ULONG		ulCurYPos;
	ULONG		ulLength;
	ULONG		ulIdx;
	FString		patchName;

	if ( players[consoleplayer].camera == NULL )
		return;

	pPlayer = players[consoleplayer].camera->player;
	if ( pPlayer == NULL )
		return;

	ulPlayer = ULONG( pPlayer - players );

	{
		int y0 = ( viewheight <= ST_Y ) ? ST_Y : SCREENHEIGHT;
		ulCurYPos = (LONG)((( y0 - 11 * CleanYfac ) + lYOffset ) / CleanYfac );
	}

	// Determine length of all medals strung together.
	ulLength = 0;
	for ( ulIdx = 0; ulIdx < NUM_MEDALS; ulIdx++ )
	{
		if ( pPlayer->ulMedalCount[ulIdx] )
		{
			patchName = g_Medals[ulIdx].szLumpName;
			if ( patchName.IsNotEmpty() )
				ulLength += ( TexMan[patchName]->GetWidth( ) * pPlayer->ulMedalCount[ulIdx] );
		}
	}

	// Can't fit all the medals on the screen.
	if ( ulLength >= 320 )
	{
		bool	bScale;
		FString	string;

		// Recalculate the length of all the medals strung together.
		ulLength = 0;
		for ( ulIdx = 0; ulIdx < NUM_MEDALS; ulIdx++ )
		{
			if ( pPlayer->ulMedalCount[ulIdx] == 0 )
				continue;

			patchName = g_Medals[ulIdx].szLumpName;
			if ( patchName.IsNotEmpty() )
				ulLength += TexMan[patchName]->GetWidth( );
		}

		// If the length of all our medals goes beyond 320, we cannot scale them.
		if ( ulLength >= 320 )
		{
			bScale = false;
			ulCurYPos = (LONG)( ulCurYPos * CleanYfac );
		}
		else
			bScale = true;

		ulCurXPos = ( bScale ? 160 : ( SCREENWIDTH / 2 )) - ( ulLength / 2 );
		for ( ulIdx = 0; ulIdx < NUM_MEDALS; ulIdx++ )
		{
			if ( pPlayer->ulMedalCount[ulIdx] == 0 )
				continue;

			patchName = g_Medals[ulIdx].szLumpName;
			if ( patchName.IsNotEmpty() )
			{
				screen->DrawTexture( TexMan[patchName],
					ulCurXPos + ( TexMan[patchName]->GetWidth( ) / 2 ),
					ulCurYPos,
					DTA_Clean, bScale,
					TAG_DONE );

				string.Format( "%lu", pPlayer->ulMedalCount[ulIdx] );
				screen->DrawText( SmallFont, CR_RED,
					ulCurXPos - ( SmallFont->StringWidth( string ) / 2 ) + TexMan[patchName]->GetWidth( ) / 2,
					ulCurYPos,
					string,
					DTA_Clean, bScale,
					TAG_DONE );
				ulCurXPos += TexMan[patchName]->GetWidth( );
			}
		}
	}
	else
	{
		ulCurXPos = 160 - ( ulLength / 2 );
		for ( ulIdx = 0; ulIdx < NUM_MEDALS; ulIdx++ )
		{
			patchName = g_Medals[ulIdx].szLumpName;
			if ( patchName.IsNotEmpty() )
			{
				ULONG	ulMedalIdx;

				for ( ulMedalIdx = 0; ulMedalIdx < pPlayer->ulMedalCount[ulIdx]; ulMedalIdx++ )
				{
					screen->DrawTexture( TexMan[patchName],
						ulCurXPos + ( TexMan[patchName]->GetWidth( ) / 2 ),
						ulCurYPos,
						DTA_Clean, true,
						TAG_DONE );
					ulCurXPos += TexMan[patchName]->GetWidth( );
				}
			}
		}
	}
}

//*****************************************************************************
//
void MEDAL_RenderAllMedalsFullscreen( player_t *pPlayer )
{
	bool		bScale;
	ULONG		ulCurXPos;
	ULONG		ulCurYPos;
	ULONG		ulIdx;
	FString		string;
	UCVarValue	ValWidth;
	UCVarValue	ValHeight;
	float		fXScale = 0.0f;
	float		fYScale;
	ULONG		ulMaxMedalHeight = 0;
	ULONG		ulNumMedal;
	ULONG		ulLastMedal = 0;

	ValWidth = con_virtualwidth.GetGenericRep( CVAR_Int );
	ValHeight = con_virtualheight.GetGenericRep( CVAR_Int );

	if (( con_scaletext ) && ( con_virtualwidth > 0 ) && ( con_virtualheight > 0 ))
	{
		fXScale =  (float)ValWidth.Int / 320.0f;
		fYScale =  (float)ValHeight.Int / 200.0f;
		bScale = true;
	}
	else
		bScale = false;

	// Start by drawing "MEDALS" 4 pixels from the top.
	ulCurYPos = 4;

	if ( bScale )
	{
		screen->DrawText( BigFont, gameinfo.gametype == GAME_Doom ? CR_RED : CR_UNTRANSLATED,
			(LONG)(( ValWidth.Int / 2 ) - ( BigFont->StringWidth( "MEDALS" ) / 2 )),
			ulCurYPos,
			"MEDALS",
			DTA_VirtualWidth, ValWidth.Int,
			DTA_VirtualHeight, ValHeight.Int,
			TAG_DONE );
	}
	else
	{
		screen->DrawText( BigFont, gameinfo.gametype == GAME_Doom ? CR_RED : CR_UNTRANSLATED,
			( SCREENWIDTH / 2 ) - ( BigFont->StringWidth( "MEDALS" ) / 2 ),
			ulCurYPos,
			"MEDALS",
			TAG_DONE );
	}

	ulCurYPos += 42;

	ulNumMedal = 0;
	for ( ulIdx = 0; ulIdx < NUM_MEDALS; ulIdx++ )
	{
		if ( pPlayer->ulMedalCount[ulIdx] == 0 )
			continue;

		if (( ulNumMedal % 2 ) == 0 )
		{
			ulCurXPos = 40;
			if ( bScale )
				ulCurXPos = (ULONG)( (float)ulCurXPos * fXScale );
			else
				ulCurXPos = (ULONG)( (float)ulCurXPos * CleanXfac );

			ulLastMedal = ulIdx;
		}
		else
		{
			if ( bScale )
				ulCurXPos = (ULONG)(( ValWidth.Int / 2 ) + ( 40 * fXScale ));
			else
				ulCurXPos = (ULONG)(( SCREENWIDTH / 2 ) + ( 40 * CleanXfac ));

			ulMaxMedalHeight = MAX( TexMan[g_Medals[ulIdx].szLumpName]->GetHeight( ), TexMan[g_Medals[ulLastMedal].szLumpName]->GetHeight( ));
		}

		if ( bScale )
		{
			screen->DrawTexture( TexMan[g_Medals[ulIdx].szLumpName],
				ulCurXPos + ( TexMan[g_Medals[ulIdx].szLumpName]->GetWidth( ) / 2 ),
				ulCurYPos + ( TexMan[g_Medals[ulIdx].szLumpName]->GetHeight( )),
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawTexture( TexMan[g_Medals[ulIdx].szLumpName],
				ulCurXPos + ( TexMan[g_Medals[ulIdx].szLumpName]->GetWidth( ) / 2 ),
				ulCurYPos + ( TexMan[g_Medals[ulIdx].szLumpName]->GetHeight( )),
				TAG_DONE );
		}

		if ( bScale )
		{
			screen->DrawText( SmallFont, CR_RED,
				ulCurXPos + 48,
				ulCurYPos + ( TexMan[g_Medals[ulIdx].szLumpName]->GetHeight( )) / 2 - SmallFont->GetHeight( ) / 2,
				"X",
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( SmallFont, CR_RED,
				ulCurXPos + 48,
				ulCurYPos + ( TexMan[g_Medals[ulIdx].szLumpName]->GetHeight( )) / 2 - SmallFont->GetHeight( ) / 2,
				"X",
				TAG_DONE );
		}

		string.Format( "%lu", pPlayer->ulMedalCount[ulIdx] );
		if ( bScale )
		{
			screen->DrawText( BigFont, CR_RED,
				ulCurXPos + 64,
				ulCurYPos + ( TexMan[g_Medals[ulIdx].szLumpName]->GetHeight( )) / 2 - BigFont->GetHeight( ) / 2,
				string,
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( BigFont, CR_RED,
				ulCurXPos + 64,
				ulCurYPos + ( TexMan[g_Medals[ulIdx].szLumpName]->GetHeight( )) / 2 - BigFont->GetHeight( ) / 2,
				string,
				TAG_DONE );
		}

		if ( ulNumMedal % 2 )
			ulCurYPos += ulMaxMedalHeight;

		ulNumMedal++;
	}

	// [CK] Update the names as well
	std::string medalStatusString = "";
	bool isConsolePlayer = (pPlayer - &players[consoleplayer] == 0);

	// The player has not earned any medals, so nothing was drawn.
	if ( ulNumMedal == 0 )
	{
		if ( isConsolePlayer )
			medalStatusString += "YOU HAVE NOT YET EARNED ANY MEDALS.";
		else
		{
			medalStatusString += pPlayer->userinfo.GetName();
			medalStatusString += " HAS NOT YET EARNED ANY MEDALS.";
		}

		if ( bScale )
		{
			screen->DrawText( SmallFont, CR_WHITE,
				(LONG)(( ValWidth.Int / 2 ) - ( SmallFont->StringWidth( medalStatusString.c_str() ) / 2 )),
				26,
				medalStatusString.c_str(),
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( SmallFont, CR_WHITE,
				( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( medalStatusString.c_str() ) / 2 ),
				26,
				medalStatusString.c_str(),
				TAG_DONE );
		}
	}
	else
	{
		if ( isConsolePlayer )
			medalStatusString += "YOU HAVE EARNED THE FOLLOWING MEDALS:";
		else
		{
			medalStatusString += pPlayer->userinfo.GetName();
			medalStatusString += " HAS EARNED THE FOLLOWING MEDALS:";
		}

		if ( bScale )
		{
			screen->DrawText( SmallFont, CR_WHITE,
				(LONG)(( ValWidth.Int / 2 ) - ( SmallFont->StringWidth( medalStatusString.c_str() ) / 2 )),
				26,
				medalStatusString.c_str(),
				DTA_VirtualWidth, ValWidth.Int,
				DTA_VirtualHeight, ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( SmallFont, CR_WHITE,
				( SCREENWIDTH / 2 ) - ( SmallFont->StringWidth( medalStatusString.c_str() ) / 2 ),
				26,
				medalStatusString.c_str(),
				TAG_DONE );
		}
	}
}

//*****************************************************************************
//
ULONG MEDAL_GetDisplayedMedal( ULONG ulPlayer )
{
	if( ulPlayer < MAXPLAYERS )
	{
		if ( g_MedalQueue[ulPlayer][0].ulTick )
			return ( g_MedalQueue[ulPlayer][0].ulMedal );
	}

	return ( NUM_MEDALS );
}

//*****************************************************************************
//
void MEDAL_ClearMedalQueue( ULONG ulPlayer )
{
	ULONG	ulQueueIdx;

	for ( ulQueueIdx = 0; ulQueueIdx < MEDALQUEUE_DEPTH; ulQueueIdx++ )
		g_MedalQueue[ulPlayer][ulQueueIdx].ulTick = 0;
}

//*****************************************************************************
//
void MEDAL_PlayerDied( ULONG ulPlayer, ULONG ulSourcePlayer )
{
	if ( PLAYER_IsValidPlayerWithMo ( ulPlayer ) == false )
		return;

	// Check for domination and first frag medals.
	if ( PLAYER_IsValidPlayerWithMo ( ulSourcePlayer ) &&
		( players[ulSourcePlayer].mo->IsTeammate( players[ulPlayer].mo ) == false ) &&
		// [Dusk] As players do not get frags for spawn telefrags, they shouldn't get medals for that either
		( MeansOfDeath != NAME_SpawnTelefrag ))
	{
		players[ulSourcePlayer].ulFragsWithoutDeath++;
		players[ulSourcePlayer].ulDeathsWithoutFrag = 0;

		medal_CheckForFirstFrag( ulSourcePlayer );
		medal_CheckForDomination( ulSourcePlayer );
		medal_CheckForFisting( ulSourcePlayer );
		medal_CheckForExcellent( ulSourcePlayer );
		medal_CheckForTermination( ulPlayer, ulSourcePlayer );
		medal_CheckForLlama( ulPlayer, ulSourcePlayer );

		players[ulSourcePlayer].ulLastFragTick = level.time;
	}

	players[ulPlayer].ulDeathCount++;
	players[ulPlayer].ulFragsWithoutDeath = 0;

	// [BB] Don't punish being killed by a teammate (except if a player kills himself).
	if ( ( ulPlayer == ulSourcePlayer )
		|| ( PLAYER_IsValidPlayerWithMo ( ulSourcePlayer ) == false )
		|| ( players[ulSourcePlayer].mo->IsTeammate( players[ulPlayer].mo ) == false ) )
	{
		players[ulPlayer].ulDeathsWithoutFrag++;
		medal_CheckForYouFailIt( ulPlayer );
	}
}

//*****************************************************************************
//
void MEDAL_ResetFirstFragAwarded( void )
{
	g_bFirstFragAwarded = false;
}

//*****************************************************************************
//*****************************************************************************
//
ULONG medal_AddToQueue( ULONG ulPlayer, ULONG ulMedal )
{
	ULONG	ulQueueIdx;

	// Search for a free slot in this player's medal queue.
	for ( ulQueueIdx = 0; ulQueueIdx < MEDALQUEUE_DEPTH; ulQueueIdx++ )
	{
		// Once we've found a free slot, update its info and break.
		if ( g_MedalQueue[ulPlayer][ulQueueIdx].ulTick == 0 )
		{
			g_MedalQueue[ulPlayer][ulQueueIdx].ulTick = MEDAL_ICON_DURATION;
			g_MedalQueue[ulPlayer][ulQueueIdx].ulMedal = ulMedal;

			return ( ulQueueIdx );
		}
		// If this isn't a free slot, maybe it's a medal of the same type that we're trying to add.
		// If so, there's no need to do anything.
		else if ( g_MedalQueue[ulPlayer][ulQueueIdx].ulMedal == ulMedal )
			return ( ulQueueIdx );
	}

	return ( ulQueueIdx );
}

//*****************************************************************************
//
void medal_PopQueue( ULONG ulPlayer )
{
	ULONG	ulQueueIdx;

	// Shift all items in the queue up one notch.
	for ( ulQueueIdx = 0; ulQueueIdx < ( MEDALQUEUE_DEPTH - 1 ); ulQueueIdx++ )
	{
		g_MedalQueue[ulPlayer][ulQueueIdx].ulMedal	= g_MedalQueue[ulPlayer][ulQueueIdx + 1].ulMedal;
		g_MedalQueue[ulPlayer][ulQueueIdx].ulTick	= g_MedalQueue[ulPlayer][ulQueueIdx + 1].ulTick;
	}

	// Also, erase the info in the last slot.
	g_MedalQueue[ulPlayer][MEDALQUEUE_DEPTH - 1].ulMedal	= 0;
	g_MedalQueue[ulPlayer][MEDALQUEUE_DEPTH - 1].ulTick		= 0;

	// If a new medal is now at the top of the queue, trigger it.
	if ( g_MedalQueue[ulPlayer][0].ulTick )
		medal_TriggerMedal( ulPlayer, g_MedalQueue[ulPlayer][0].ulMedal );
	// If there isn't, just delete the medal that has been displaying.
	else if ( players[ulPlayer].pIcon )
	{
		players[ulPlayer].pIcon->Destroy( );
		players[ulPlayer].pIcon = NULL;
	}
}

//*****************************************************************************
// [BB, RC] Returns whether the player wears a carrier icon (flag/skull/hellstone/etc) and removes any invalid ones.
// 
bool medal_PlayerHasCarrierIcon ( ULONG ulPlayer )
{
	player_t *pPlayer = &players[ulPlayer];
	AInventory	*pInventory = NULL;
	bool bInvalid = false;
	bool bHasIcon = true;

	// [BB] If the player has no icon, he obviously doesn't have a carrier icon.
	if ( pPlayer->pIcon == NULL )
		return false;

	// Verify that our current icon is valid.
	if ( pPlayer->pIcon && pPlayer->pIcon->bTeamItemFloatyIcon == false )
	{
		switch ( (ULONG)( pPlayer->pIcon->state - pPlayer->pIcon->SpawnState ))
		{
		// Flag/skull icon. Delete it if the player no longer has it.
		case S_WHITEFLAG:
		case ( S_WHITEFLAG + 1 ):
		case ( S_WHITEFLAG + 2 ):
		case ( S_WHITEFLAG + 3 ):
		case ( S_WHITEFLAG + 4 ):
		case ( S_WHITEFLAG + 5 ):

			{
				// Delete the icon if teamgame has been turned off, or if the player
				// is not on a team.
				if (( teamgame == false ) ||
					( pPlayer->bOnTeam == false ))
				{
					bInvalid = true;
					break;
				}

				// Delete the white flag if the player no longer has it.
				pInventory = pPlayer->mo->FindInventory( PClass::FindClass( "WhiteFlag" ));
				if ( pInventory == NULL )
				{
					bInvalid = true;
					break;
				}

			}

			break;
		// Terminator artifact icon. Delete it if the player no longer has it.
		case S_TERMINATORARTIFACT:
		case ( S_TERMINATORARTIFACT + 1 ):
		case ( S_TERMINATORARTIFACT + 2 ):
		case ( S_TERMINATORARTIFACT + 3 ):

			if (( terminator == false ) || (( pPlayer->cheats2 & CF2_TERMINATORARTIFACT ) == false ))
				bInvalid = true;
			break;
		// Possession artifact icon. Delete it if the player no longer has it.
		case S_POSSESSIONARTIFACT:
		case ( S_POSSESSIONARTIFACT + 1 ):
		case ( S_POSSESSIONARTIFACT + 2 ):
		case ( S_POSSESSIONARTIFACT + 3 ):

			if ((( possession == false ) && ( teampossession == false )) || (( pPlayer->cheats2 & CF2_POSSESSIONARTIFACT ) == false ))
				bInvalid = true;
			break;
		default:
			bHasIcon = false;
			break;
		}
	}

	if ( pPlayer->pIcon && pPlayer->pIcon->bTeamItemFloatyIcon )
	{
		if ( GAMEMODE_GetCurrentFlags() & GMF_USETEAMITEM )
			bInvalid = ( TEAM_FindOpposingTeamsItemInPlayersInventory ( pPlayer ) == NULL );
	}

	// Remove it.
	if ( bInvalid && bHasIcon )
	{
		players[ulPlayer].pIcon->Destroy( );
		players[ulPlayer].pIcon = NULL;

		medal_TriggerMedal( ulPlayer, g_MedalQueue[ulPlayer][0].ulMedal );
	}

	return bHasIcon && !bInvalid;
}

//*****************************************************************************
//
void medal_TriggerMedal( ULONG ulPlayer, ULONG ulMedal )
{
	player_t	*pPlayer;

	pPlayer = &players[ulPlayer];

	// Servers don't actually spawn medals.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	// Shouldn't happen...
	if ( pPlayer->mo == NULL )
		return;

	// Make sure this medal is valid.
	if ( ulMedal >= NUM_MEDALS || !g_MedalQueue[ulPlayer][0].ulTick )
		return;

	// Medals don't override carrier symbols.
	if ( !medal_PlayerHasCarrierIcon( ulPlayer) )
	{
		if ( pPlayer->pIcon )
			pPlayer->pIcon->Destroy( );

		// Spawn the medal as an icon above the player and set its properties.
		pPlayer->pIcon = Spawn<AFloatyIcon>( pPlayer->mo->x, pPlayer->mo->y, pPlayer->mo->z, NO_REPLACE );
		if ( pPlayer->pIcon )
		{
			pPlayer->pIcon->SetState( pPlayer->pIcon->SpawnState + g_Medals[ulMedal].usFrame );
			// [BB] Instead of MEDAL_ICON_DURATION only use the remaining ticks of the medal as ticks for the icon.
			// It is possible that the medal is just restored because the player respawned or that the medal was
			// suppressed by a carrier icon.
			pPlayer->pIcon->lTick = g_MedalQueue[ulPlayer][0].ulTick;
			pPlayer->pIcon->SetTracer( pPlayer->mo );
		}
	}

	// [BB] Only announce the medal when it reaches the top of the queue. Otherwise it could be
	// announced multiple times (for instance when a carrier dies).
	if ( g_MedalQueue[ulPlayer][0].ulTick == MEDAL_ICON_DURATION )
	{
		// Also, locally play the announcer sound associated with this medal.
		// [Dusk] Check coop spy too
		if ( pPlayer->mo->CheckLocalView( consoleplayer ) )
		{
			if ( g_Medals[ulMedal].szAnnouncerEntry[0] )
				ANNOUNCER_PlayEntry( cl_announcer, g_Medals[ulMedal].szAnnouncerEntry );
		}
		// If a player besides the console player got the medal, play the remote sound.
		else
		{
			// Play the sound effect associated with this medal type.
			if ( g_Medals[ulMedal].szSoundName[0] != '\0' )
				S_Sound( pPlayer->mo, CHAN_AUTO, g_Medals[ulMedal].szSoundName, 1, ATTN_NORM );
		}
	}
}

//*****************************************************************************
//
ULONG medal_GetDesiredIcon( player_t *pPlayer, AInventory *&pTeamItem )
{
	ULONG ulDesiredSprite = NUM_SPRITES;

	// [BB] Invalid players certainly don't need any icon.
	if ( ( pPlayer == NULL ) || ( pPlayer->mo == NULL ) )
		return NUM_SPRITES;

	// Draw an ally icon if this person is on our team. Would this be useful for co-op, too?
	// [BB] In free spectate mode, we don't have allies (and SCOREBOARD_GetViewPlayer doesn't return a useful value).
	if ( ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS ) && ( CLIENTDEMO_IsInFreeSpectateMode() == false ) )
	{
		// [BB] Dead spectators shall see the icon for their teammates.
		if ( pPlayer->mo->IsTeammate( players[SCOREBOARD_GetViewPlayer()].mo ) && !PLAYER_IsTrueSpectator ( &players[SCOREBOARD_GetViewPlayer()] ) )
			ulDesiredSprite = SPRITE_ALLY;
	}

	// Draw a chat icon over the player if they're typing.
	if ( pPlayer->bChatting )
		ulDesiredSprite = SPRITE_CHAT;

	// Draw a console icon over the player if they're in the console.
	if ( pPlayer->bInConsole )
		ulDesiredSprite = SPRITE_INCONSOLE;

	// Draw a lag icon over their head if they're lagging.
	if ( pPlayer->bLagging )
		ulDesiredSprite = SPRITE_LAG;

	// Draw a flag/skull above this player if he's carrying one.
	if ( GAMEMODE_GetCurrentFlags() & GMF_USETEAMITEM )
	{
		if ( pPlayer->bOnTeam )
		{
			if ( oneflagctf )
			{
				AInventory *pInventory = pPlayer->mo->FindInventory( PClass::FindClass( "WhiteFlag" ));
				if ( pInventory )
					ulDesiredSprite = SPRITE_WHITEFLAG;
			}

			else
			{
				pTeamItem = TEAM_FindOpposingTeamsItemInPlayersInventory ( pPlayer );
				if ( pTeamItem )
					ulDesiredSprite = SPRITE_TEAMITEM;
			}
		}
	}

	// Draw the terminator artifact over the terminator.
	if ( terminator && ( pPlayer->cheats2 & CF2_TERMINATORARTIFACT ))
		ulDesiredSprite = SPRITE_TERMINATORARTIFACT;

	// Draw the possession artifact over the player.
	if (( possession || teampossession ) && ( pPlayer->cheats2 & CF2_POSSESSIONARTIFACT ))
		ulDesiredSprite = SPRITE_POSSESSIONARTIFACT;

	return ulDesiredSprite;
}

//*****************************************************************************
//
void medal_SelectIcon( ULONG ulPlayer )
{
	AInventory	*pInventory;
	player_t	*pPlayer;
	ULONG		ulActualSprite = NUM_SPRITES;
	// [BB] If ulPlayer carries a TeamItem, e.g. flag or skull, we store a pointer
	// to it in pTeamItem and set the floaty icon to the carry (or spawn) state of
	// the TeamItem. We also need to copy the Translation of the TeamItem to the
	// FloatyIcon.
	AInventory	*pTeamItem = NULL;

	if ( ulPlayer >= MAXPLAYERS )
		return;

	pPlayer = &players[ulPlayer];
	if ( pPlayer->mo == NULL )
		return;

	// Allow the user to disable icons.
	if (( cl_icons == false ) || ( NETWORK_GetState( ) == NETSTATE_SERVER ) || pPlayer->bSpectating )
	{
		if ( pPlayer->pIcon )
		{
			pPlayer->pIcon->Destroy( );
			pPlayer->pIcon = NULL;
		}

		return;
	}

	// Verify that our current icon is valid. (i.e. We may have had a chat bubble, then
	// stopped talking, so we need to delete it).
	if ( pPlayer->pIcon && pPlayer->pIcon->bTeamItemFloatyIcon == false )
	{
		switch ( (ULONG)( pPlayer->pIcon->state - pPlayer->pIcon->SpawnState ))
		{
		// Chat icon. Delete it if the player is no longer talking.
		case S_CHAT:

			if ( pPlayer->bChatting == false )
			{
				pPlayer->pIcon->Destroy( );
				pPlayer->pIcon = NULL;
			}
			else
				ulActualSprite = SPRITE_CHAT;
			break;
		// In console icon. Delete it if the player is no longer in the console.
		case S_INCONSOLE:
		case ( S_INCONSOLE + 1):

			if ( pPlayer->bInConsole == false )
			{
				pPlayer->pIcon->Destroy( );
				pPlayer->pIcon = NULL;
			}
			else
				ulActualSprite = SPRITE_INCONSOLE;
			break;

		// Ally icon. Delete it if the player is now our enemy or if we're spectating.
		// [BB] Dead spectators shall keep the icon for their teammates.
		case S_ALLY:

			if ( PLAYER_IsTrueSpectator ( &players[ SCOREBOARD_GetViewPlayer() ] ) || ( !pPlayer->mo->IsTeammate( players[ SCOREBOARD_GetViewPlayer() ].mo ) ) )
			{
				pPlayer->pIcon->Destroy( );
				pPlayer->pIcon = NULL;
			}
			else
				ulActualSprite = SPRITE_ALLY;
			break;
		// Flag/skull icon. Delete it if the player no longer has it.
		case S_WHITEFLAG:
		case ( S_WHITEFLAG + 1 ):
		case ( S_WHITEFLAG + 2 ):
		case ( S_WHITEFLAG + 3 ):
		case ( S_WHITEFLAG + 4 ):
		case ( S_WHITEFLAG + 5 ):

			{
				bool	bDelete = false;

				// Delete the icon if teamgame has been turned off, or if the player
				// is not on a team.
				if (( teamgame == false ) ||
					( pPlayer->bOnTeam == false ))
				{
					bDelete = true;
				}

				// Delete the white flag if the player no longer has it.
				pInventory = pPlayer->mo->FindInventory( PClass::FindClass( "WhiteFlag" ));
				if (( oneflagctf ) && ( pInventory == NULL ))
					bDelete = true;

				// We wish to delete the icon, so do that now.
				if ( bDelete )
				{
					pPlayer->pIcon->Destroy( );
					pPlayer->pIcon = NULL;
				}
				else
					ulActualSprite = SPRITE_WHITEFLAG;
			}

			break;
		// Terminator artifact icon. Delete it if the player no longer has it.
		case S_TERMINATORARTIFACT:
		case ( S_TERMINATORARTIFACT + 1 ):
		case ( S_TERMINATORARTIFACT + 2 ):
		case ( S_TERMINATORARTIFACT + 3 ):

			if (( terminator == false ) || (( pPlayer->cheats2 & CF2_TERMINATORARTIFACT ) == false ))
			{
				pPlayer->pIcon->Destroy( );
				pPlayer->pIcon = NULL;
			}
			else
				ulActualSprite = SPRITE_TERMINATORARTIFACT;
			break;
		// Lag icon. Delete it if the player is no longer lagging.
		case S_LAG:

			if (( NETWORK_InClientMode() == false ) ||
				( pPlayer->bLagging == false ))
			{
				pPlayer->pIcon->Destroy( );
				pPlayer->pIcon = NULL;
			}
			else
				ulActualSprite = SPRITE_LAG;
			break;
		// Possession artifact icon. Delete it if the player no longer has it.
		case S_POSSESSIONARTIFACT:
		case ( S_POSSESSIONARTIFACT + 1 ):
		case ( S_POSSESSIONARTIFACT + 2 ):
		case ( S_POSSESSIONARTIFACT + 3 ):

			if ((( possession == false ) && ( teampossession == false )) || (( pPlayer->cheats2 & CF2_POSSESSIONARTIFACT ) == false ))
			{
				pPlayer->pIcon->Destroy( );
				pPlayer->pIcon = NULL;
			}
			else
				ulActualSprite = SPRITE_POSSESSIONARTIFACT;
			break;
		}
	}

	// Check if we need to have an icon above us, or change the current icon.
	{
		if ( pPlayer->pIcon && pPlayer->pIcon->bTeamItemFloatyIcon )
		{
			if ( !( GAMEMODE_GetCurrentFlags() & GMF_USETEAMITEM ) || ( pPlayer->bOnTeam == false )
			     || ( TEAM_FindOpposingTeamsItemInPlayersInventory ( pPlayer ) == NULL ) )
			{
				pPlayer->pIcon->Destroy( );
				pPlayer->pIcon = NULL;
			}
			else
			{
				ulActualSprite = SPRITE_TEAMITEM;
			}
		}

		ULONG	ulFrame = UINT_MAX;
		const ULONG ulDesiredSprite = medal_GetDesiredIcon ( pPlayer, pTeamItem );

		// [BB] Determine the frame based on the desired sprite.
		switch ( ulDesiredSprite )
		{
		case SPRITE_ALLY:
			ulFrame = S_ALLY;
			break;

		case SPRITE_CHAT:
			ulFrame = S_CHAT;
			break;

		case SPRITE_INCONSOLE:
			ulFrame = S_INCONSOLE;
			break;

		case SPRITE_LAG:
			ulFrame = S_LAG;
			break;

		case SPRITE_WHITEFLAG:
			ulFrame = S_WHITEFLAG;
			break;

		case SPRITE_TEAMITEM:
			ulFrame = 0;
			break;

		case SPRITE_TERMINATORARTIFACT:
			ulFrame = S_TERMINATORARTIFACT;
			break;

		case SPRITE_POSSESSIONARTIFACT:
			ulFrame = S_POSSESSIONARTIFACT;
			break;

		default:
			break;
		}

		// We have an icon that needs to be spawned.
		if ((( ulFrame != UINT_MAX ) && ( ulDesiredSprite != NUM_SPRITES )))
		{
			// [BB] If a TeamItem icon replaces an existing non-team icon, we have to delete the old icon first.
			if ( pPlayer->pIcon && ( pPlayer->pIcon->bTeamItemFloatyIcon == false ) && pTeamItem )
			{
				pPlayer->pIcon->Destroy( );
				pPlayer->pIcon = NULL;
			}

			if (( pPlayer->pIcon == NULL ) || ( ulDesiredSprite != ulActualSprite ))
			{
				if ( pPlayer->pIcon == NULL )
				{
					pPlayer->pIcon = Spawn<AFloatyIcon>( pPlayer->mo->x, pPlayer->mo->y, pPlayer->mo->z + pPlayer->mo->height + ( 4 * FRACUNIT ), NO_REPLACE );
					if ( pTeamItem )
					{
						pPlayer->pIcon->bTeamItemFloatyIcon = true;

						FName Name = "Carry";

						FState *CarryState = pTeamItem->FindState( Name );

						// [BB] If the TeamItem has a Carry state (like the built in flags), use it.
						// Otherwise use the spawn state (the built in skulls don't have a carry state).
						if ( CarryState )
							pPlayer->pIcon->SetState( CarryState );
						else
							pPlayer->pIcon->SetState( pTeamItem->SpawnState );
						pPlayer->pIcon->Translation = pTeamItem->Translation;
					}
					else
					{
						pPlayer->pIcon->bTeamItemFloatyIcon = false;
					}
				}

				if ( pPlayer->pIcon )
				{
					// [BB] Potentially the new icon overrides an existing medal, so make sure that it doesn't fade out.
					pPlayer->pIcon->lTick = 0;
					pPlayer->pIcon->SetTracer( pPlayer->mo );

					if ( pPlayer->pIcon->bTeamItemFloatyIcon == false )
						pPlayer->pIcon->SetState( pPlayer->pIcon->SpawnState + ulFrame );
				}
			}
		}
	}
}

//*****************************************************************************
//
void medal_GiveMedal( ULONG ulPlayer, ULONG ulMedal )
{
	// [CK] Clients do not need to know if they got a medal during countdown
	if ( GAMEMODE_IsGameInCountdown() )
		return;

	// Give the player the medal, and if we're the server, tell clients.
	MEDAL_GiveMedal( ulPlayer, ulMedal );
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_GivePlayerMedal( ulPlayer, ulMedal );
}

//*****************************************************************************
//
void medal_CheckForFirstFrag( ULONG ulPlayer )
{
	// Only award it once.
	if ( g_bFirstFragAwarded )
		return;

	if (( deathmatch ) &&
		( lastmanstanding == false ) &&
		( teamlms == false ) &&
		( possession == false ) &&
		( teampossession == false ) &&
		(( duel == false ) || ( DUEL_GetState( ) == DS_INDUEL )))
	{
		medal_GiveMedal( ulPlayer, MEDAL_FIRSTFRAG );

		// It's been given.
		g_bFirstFragAwarded = true;
	}
}

//*****************************************************************************
//
void medal_CheckForDomination( ULONG ulPlayer )
{
	// If the player has gotten 5 straight frags without dying, award a medal.
	if (( players[ulPlayer].ulFragsWithoutDeath % 5 ) == 0 )
	{
		// If the player gets 10+ straight frags without dying, award a "Total Domination" medal.
		if ( players[ulPlayer].ulFragsWithoutDeath >= 10 )
			medal_GiveMedal( ulPlayer, MEDAL_TOTALDOMINATION );
		// Otherwise, award a "Domination" medal.
		else
			medal_GiveMedal( ulPlayer, MEDAL_DOMINATION );
	}
}

//*****************************************************************************
//
void medal_CheckForFisting( ULONG ulPlayer )
{
	if ( players[ulPlayer].ReadyWeapon == NULL )
		return;

	// [BB/K6] Neither Fist nor BFG9000 will cause this MeansOfDeath.
	if ( MeansOfDeath == NAME_Telefrag )
		return;

	// If the player killed him with this fist, award him a "Fisting!" medal.
	if ( players[ulPlayer].ReadyWeapon->GetClass( ) == PClass::FindClass( "Fist" ))
		medal_GiveMedal( ulPlayer, MEDAL_FISTING );

	// If this is the second frag this player has gotten THIS TICK with the
	// BFG9000, award him a "SPAM!" medal.
	if ( players[ulPlayer].ReadyWeapon->GetClass( ) == PClass::FindClass( "BFG9000" ))
	{
		if ( players[ulPlayer].ulLastBFGFragTick == static_cast<unsigned> (level.time) )
		{
			// Award the medal.
			medal_GiveMedal( ulPlayer, MEDAL_SPAM );

			// Also, cancel out the possibility of getting an Excellent/Incredible medal.
			players[ulPlayer].ulLastExcellentTick = 0;
			players[ulPlayer].ulLastFragTick = 0;
		}
		else
			players[ulPlayer].ulLastBFGFragTick = level.time;
	}
}

//*****************************************************************************
//
void medal_CheckForExcellent( ULONG ulPlayer )
{
	// If the player has gotten two Excelents within two seconds, award an "Incredible" medal.
	// [BB] Check that the player actually got an excellent medal.
	if ( ( ( players[ulPlayer].ulLastExcellentTick + ( 2 * TICRATE )) > (ULONG)level.time ) && players[ulPlayer].ulLastExcellentTick )
	{
		// Award the incredible.
		medal_GiveMedal( ulPlayer, MEDAL_INCREDIBLE );

		players[ulPlayer].ulLastExcellentTick = level.time;
		players[ulPlayer].ulLastFragTick = level.time;
	}
	// If this player has gotten two frags within two seconds, award an "Excellent" medal.
	// [BB] Check that the player actually got a frag.
	else if ( ( ( players[ulPlayer].ulLastFragTick + ( 2 * TICRATE )) > (ULONG)level.time ) && players[ulPlayer].ulLastFragTick )
	{
		// Award the excellent.
		medal_GiveMedal( ulPlayer, MEDAL_EXCELLENT );

		players[ulPlayer].ulLastExcellentTick = level.time;
		players[ulPlayer].ulLastFragTick = level.time;
	}
}

//*****************************************************************************
//
void medal_CheckForTermination( ULONG ulDeadPlayer, ULONG ulPlayer )
{
	// If the target player is the terminatior, award a "termination" medal.
	if ( players[ulDeadPlayer].cheats2 & CF2_TERMINATORARTIFACT )
		medal_GiveMedal( ulPlayer, MEDAL_TERMINATION );
}

//*****************************************************************************
//
void medal_CheckForLlama( ULONG ulDeadPlayer, ULONG ulPlayer )
{
	// Award a "llama" medal if the victim had been typing, lagging, or in the console.
	if ( players[ulDeadPlayer].bChatting ||	players[ulDeadPlayer].bLagging || players[ulDeadPlayer].bInConsole )
		medal_GiveMedal( ulPlayer, MEDAL_LLAMA );
}

//*****************************************************************************
//
void medal_CheckForYouFailIt( ULONG ulPlayer )
{
	// If the player dies TEN times without getting a frag, award a "Your skill is not enough" medal.
	if (( players[ulPlayer].ulDeathsWithoutFrag % 10 ) == 0 )
		medal_GiveMedal( ulPlayer, MEDAL_YOURSKILLISNOTENOUGH );
	// If the player dies five times without getting a frag, award a "You fail it" medal.
	else if (( players[ulPlayer].ulDeathsWithoutFrag % 5 ) == 0 )
		medal_GiveMedal( ulPlayer, MEDAL_YOUFAILIT );
}

#ifdef	_DEBUG
#include "c_dispatch.h"
CCMD( testgivemedal )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < NUM_MEDALS; ulIdx++ )
		MEDAL_GiveMedal( consoleplayer, ulIdx );
}
#endif	// _DEBUG
