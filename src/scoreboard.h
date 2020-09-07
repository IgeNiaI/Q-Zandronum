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
// Filename: scoreboard.h
//
// Description: Contains scoreboard structures and prototypes
//
//-----------------------------------------------------------------------------

#ifndef __SCOREBOARD_H__
#define __SCOREBOARD_H__

#include "d_player.h"

//*****************************************************************************
//	DEFINES

#define	COLUMN1_XPOS		24
#define	COLUMN2_XPOS		72
#define	COLUMN3_XPOS		192
#define	COLUMN4_XPOS		256

// Maximum number of columns.
#define MAX_COLUMNS			8

//*****************************************************************************
enum
{
	COLUMN_EMPTY,
	COLUMN_NAME,
	COLUMN_TIME,
	COLUMN_PING,
	COLUMN_FRAGS,
	COLUMN_POINTS,
	COLUMN_DEATHS,
	COLUMN_WINS,
	COLUMN_KILLS,
	COLUMN_POINTSASSISTS,
	COLUMN_SECRETS,
	COLUMN_MEDALS,

	NUM_COLUMN_TYPES
};

//*****************************************************************************
enum
{
	ST_FRAGCOUNT,
	ST_POINTCOUNT,
	ST_KILLCOUNT,
	ST_WINCOUNT,

	NUM_SORT_TYPES
};

//*****************************************************************************
//	PROTOTYPES

void	SCOREBOARD_Render( ULONG ulDisplayPlayer );
void	SCOREBOARD_RenderBoard( ULONG ulDisplayPlayer );
bool	SCOREBOARD_IsUsingNewHud( void );
bool	SCOREBOARD_IsHudVisible( void );
bool	SCOREBOARD_IsHudFullscreen( void );
void	SCOREBOARD_RenderStats_Holders( void );
void	SCOREBOARD_RenderStats_TeamScores( void );
void	SCOREBOARD_RenderStats_RankSpread( void );
void	SCOREBOARD_RenderInvasionStats( void );
void	SCOREBOARD_BuildLimitStrings( std::list<FString> &lines, bool bAcceptColors );
void	SCOREBOARD_RenderInVote( void );
void	SCOREBOARD_RenderInVoteClassic( void );
void	SCOREBOARD_RenderDuelCountdown( ULONG ulTimeLeft );
void	SCOREBOARD_RenderLMSCountdown( ULONG ulTimeLeft );
void	SCOREBOARD_RenderPossessionCountdown( const char *pszTitleString, ULONG ulTimeLeft );
void	SCOREBOARD_RenderSurvivalCountdown( ULONG ulTimeLeft );
void	SCOREBOARD_RenderInvasionFirstCountdown( ULONG ulTimeLeft );
void	SCOREBOARD_RenderInvasionCountdown( ULONG ulTimeLeft );
bool	SCOREBOARD_ShouldDrawBoard( ULONG ulDisplayPlayer );
bool	SCOREBOARD_ShouldDrawRank( ULONG player );
ULONG	SCOREBOARD_GetViewPlayer( void );
LONG	SCOREBOARD_CalcSpread( ULONG ulPlayerNum );
ULONG	SCOREBOARD_CalcRank( ULONG ulPlayerNum );
bool	SCOREBOARD_IsTied( ULONG ulPlayerNum );
void	SCOREBOARD_DisplayFragMessage( player_t *pFraggedPlayer );
void	SCOREBOARD_DisplayFraggedMessage( player_t *pFraggingPlayer );
void	SCOREBOARD_RefreshHUD( void );
ULONG	SCOREBOARD_GetNumPlayers( void );
ULONG	SCOREBOARD_GetRank( void );
LONG	SCOREBOARD_GetSpread( void );
LONG	SCOREBOARD_GetLeftToLimit( void );
bool	SCOREBOARD_IsTied( void );
FString	SCOREBOARD_SpellOrdinal( int ranknum );
FString	SCOREBOARD_SpellOrdinalColored( int ranknum );

//*****************************************************************************
//	EXTERNAL CONSOLE VARIABLES

EXTERN_CVAR( Bool, cl_alwaysdrawdmstats )
EXTERN_CVAR( Bool, cl_alwaysdrawteamstats )

#endif // __SCOREBOARD_H__
