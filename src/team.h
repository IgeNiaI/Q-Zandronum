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
// Filename: team.h
//
// Description: Contains team structures and prototypes
//
//-----------------------------------------------------------------------------

#ifndef __TEAM_H__
#define __TEAM_H__

#include "a_doomglobal.h"
#include "a_pickups.h"
#include "c_cvars.h"
//#include "keycard.h"
#include "d_player.h"
#include "teaminfo.h"

//*****************************************************************************
//	DEFINES

// [CW] Add to this when bumping 'MAX_TEAMS'.
enum
{
	TEAM1_STARTMENUPOS = 2,
	TEAM2_STARTMENUPOS = 10,
	TEAM3_STARTMENUPOS = 18,
	TEAM4_STARTMENUPOS = 26,
};

// [RC] Global constant for	team messages.
#define TEAM_MESSAGE_Y_AXIS		0.135f
#define TEAM_MESSAGE_Y_AXIS_SUB 0.195f

//*****************************************************************************
//	PROTOTYPES

void		TEAM_Construct( void );
void		TEAM_Tick( void );
void		TEAM_Reset( void );
ULONG		TEAM_CountPlayers( ULONG ulTeamIdx );
ULONG		TEAM_CountLivingAndRespawnablePlayers( ULONG ulTeamIdx );
ULONG		TEAM_TeamsWithPlayersOn( void );
void		TEAM_ExecuteReturnRoutine( ULONG ulTeamIdx, AActor *pReturner );
ULONG		TEAM_ChooseBestTeamForPlayer( const bool bIgnoreTeamStartsAvailability = false );
void		TEAM_ScoreSkulltagPoint( player_t *pPlayer, ULONG ulNumPoints, AActor *pPillar );
void		TEAM_DisplayNeedToReturnSkullMessage( player_t *pPlayer );
void		TEAM_FlagDropped( player_t *pPlayer, ULONG ulTeamIdx );
WORD		TEAM_GetReturnScriptOffset( ULONG ulTeamIdx );
void		TEAM_DoWinSequence( ULONG ulTeamIdx );
void		TEAM_TimeExpired( void );
bool		TEAM_SpawningTemporaryFlag( void );

// Access functions.
bool		TEAM_CheckIfValid( ULONG ulTeamIdx );

const char	*TEAM_GetName( ULONG ulTeamIdx );
void		TEAM_SetName( ULONG ulTeamIdx, const char *pszName );

ULONG		TEAM_GetTeamNumberByName( const char *pszName );

int			TEAM_GetColor( ULONG ulTeamIdx );
void		TEAM_SetColor( ULONG ulTeamIdx, int r, int g, int b );
bool		TEAM_IsCustomPlayerColorAllowed( ULONG ulTeamIdx );

ULONG		TEAM_GetTextColor( ULONG ulTeamIdx );
void		TEAM_SetTextColor( ULONG ulTeamIdx, USHORT usColor );

LONG		TEAM_GetRailgunColor( ULONG ulTeamIdx );
void		TEAM_SetRailgunColor( ULONG ulTeamIdx, LONG lColor );

LONG		TEAM_GetScore( ULONG ulTeamIdx );
void		TEAM_SetScore( ULONG ulTeamIdx, LONG lScore, bool bAnnouncer );

const char	*TEAM_GetSmallHUDIcon( ULONG ulTeamIdx );
void		TEAM_SetSmallHUDIcon( ULONG ulTeamIdx, const char *pszName, bool bFlag );

const char	*TEAM_GetLargeHUDIcon( ULONG ulTeamIdx );
void		TEAM_SetLargeHUDIcon( ULONG ulTeamIdx, const char *pszName, bool bFlag );

bool		TEAM_HasCustomString( ULONG ulTeamIdx, const FString TEAMINFO::*stringPointer );
const char	*TEAM_GetCustomString( ULONG ulTeamIdx, const FString TEAMINFO::*stringPointer );
const char	*TEAM_SelectCustomStringForPlayer( player_t *pPlayer, const FString TEAMINFO::*stringPointer, const char *pszDefaultString );

const PClass	*TEAM_GetItem( ULONG ulTeamIdx );
void		TEAM_SetItem( ULONG ulTeamIdx, const PClass *pType, bool bFlag );

AInventory	*TEAM_FindOpposingTeamsItemInPlayersInventory( player_t *pPlayer );

player_t	*TEAM_GetCarrier( ULONG ulTeamIdx );
void		TEAM_SetCarrier( ULONG ulTeamIdx, player_t *player );

bool		TEAM_GetAnnouncedLeadState( ULONG ulTeamIdx );
void		TEAM_SetAnnouncedLeadState( ULONG ulTeamIdx, bool bAnnouncedLeadState );

ULONG		TEAM_GetReturnTicks( ULONG ulTeamIdx );
void		TEAM_SetReturnTicks( ULONG ulTeamIdx, ULONG ulTicks );

LONG		TEAM_GetFragCount( ULONG ulTeamIdx );
void		TEAM_SetFragCount( ULONG ulTeamIdx, LONG lFragCount, bool bAnnounce );

LONG		TEAM_GetDeathCount( ULONG ulTeamIdx );
void		TEAM_SetDeathCount( ULONG ulTeamIdx, LONG lDeathCount );

LONG		TEAM_GetWinCount( ULONG ulTeamIdx );
void		TEAM_SetWinCount( ULONG ulTeamIdx, LONG lWinCount, bool bAnnounce );

bool		TEAM_GetSimpleCTFSTMode( void );
void		TEAM_SetSimpleCTFSTMode( bool bSimple );

bool		TEAM_GetItemTaken( ULONG ulTeamIdx );
void		TEAM_SetItemTaken( ULONG ulTeamIdx, bool bTaken );

bool		TEAM_GetWhiteFlagTaken( void );
void		TEAM_SetWhiteFlagTaken( bool bTaken );

POS_t		TEAM_GetItemOrigin( ULONG ulTeamIdx );
void		TEAM_SetTeamItemOrigin( ULONG ulTeamIdx, POS_t Origin );

POS_t		TEAM_GetWhiteFlagOrigin( void );
void		TEAM_SetWhiteFlagOrigin( POS_t Origin );

ULONG		TEAM_GetAssistPlayer( ULONG ulTeamIdx );
void		TEAM_SetAssistPlayer( ULONG ulTeamIdx, ULONG ulPlayer );
void		TEAM_CancelAssistsOfPlayer( ULONG ulPlayer );

bool		TEAM_CheckAllTeamsHaveEqualFrags( void );
bool		TEAM_CheckAllTeamsHaveEqualWins( void );
bool		TEAM_CheckAllTeamsHaveEqualScores( void );

unsigned int	TEAM_GetNumAvailableTeams( void );
unsigned int	TEAM_GetNumTeamsWithStarts( void );
bool		TEAM_ShouldUseTeam( ULONG ulTeam );

LONG		TEAM_GetHighestFragCount( void );
LONG		TEAM_GetHighestWinCount( void );
LONG		TEAM_GetHighestScoreCount( void );

LONG		TEAM_GetSpread ( ULONG ulTeam, LONG (*GetCount) ( ULONG ulTeam ) );
LONG		TEAM_GetFragCountSpread ( ULONG ulTeam );
LONG		TEAM_GetWinCountSpread ( ULONG ulTeam );
LONG		TEAM_GetScoreCountSpread ( ULONG ulTeam );

ULONG		TEAM_CountFlags( void );
ULONG		TEAM_CountSkulls( void );

ULONG		TEAM_GetTeamFromItem( AActor *pActor );
ULONG		TEAM_GetNextTeam( ULONG ulTeamIdx );

bool		TEAM_ShouldJoinTeam();
bool		TEAM_IsActorAllowedForPlayer( AActor *pActor, player_t *pPlayer );
bool		TEAM_IsClassAllowedForPlayer( ULONG ulClass, player_t *pPlayer );
bool		TEAM_CheckTeamRestriction( ULONG ulTeam, ULONG ulTeamRestriction );
bool		TEAM_IsActorAllowedForTeam( AActor *pActor, ULONG ulTeam );
bool		TEAM_IsClassAllowedForTeam( ULONG ulClass, ULONG ulTeam );
ULONG		TEAM_FindValidClassForPlayer( player_t *pPlayer );
void		TEAM_EnsurePlayerHasValidClass( player_t *pPlayer );
LONG		TEAM_SelectRandomValidPlayerClass( ULONG ulTeam );
bool		TEAM_IsActorVisibleToPlayer( const AActor *pActor, player_t *pPlayer );
int		TEAM_GetPlayerStartThingNum( ULONG ulTeam );
const char*	TEAM_GetTeamItemName( ULONG ulTeam );
const char*	TEAM_GetIntermissionTheme( ULONG ulTeam, bool bWin );

//*****************************************************************************
//  EXTERNAL CONSOLE VARIABLES

EXTERN_CVAR( Bool, teamgame )
EXTERN_CVAR( Bool, ctf )
EXTERN_CVAR( Bool, oneflagctf )
EXTERN_CVAR( Bool, skulltag )
EXTERN_CVAR( Bool, domination )

EXTERN_CVAR( Int, pointlimit )
EXTERN_CVAR( Int, sv_flagreturntime )

EXTERN_CVAR( Int, menu_team )

EXTERN_CVAR( Int, sv_maxteams )

#endif	// __TEAM_H__
