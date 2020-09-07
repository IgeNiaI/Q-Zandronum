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
// Filename: botcommands.cpp
//
// Description: Contains bot functions
//
//-----------------------------------------------------------------------------

#include "astar.h"
#include "botcommands.h"
#include "botpath.h"
#include "bots.h"
#include "c_console.h"
#include "chat.h"
#include "cl_main.h"
#include "cmdlib.h"
#include "cooperative.h"
#include "deathmatch.h"
#include "doomstat.h"
#include "duel.h"
#include "g_game.h"
#include "g_level.h"
#include "gamemode.h"
#include "gi.h"
#include "joinqueue.h"
#include "m_random.h"
#include "network.h"
#include "p_local.h"
#include "p_trace.h"
#include "i_system.h"
#include "sc_man.h"
#include "scoreboard.h"
#include "sv_commands.h"
#include "sv_main.h"
#include "team.h"

//*****************************************************************************
//	PROTOTYPES

static	void	botcmd_ChangeState( CSkullBot *pBot );
static	void	botcmd_Delay( CSkullBot *pBot );
static	void	botcmd_Random( CSkullBot *pBot );
static	void	botcmd_StringsAreEqual( CSkullBot *pBot );
static	void	botcmd_LookForPowerups( CSkullBot *pBot );
static	void	botcmd_LookForWeapons( CSkullBot *pBot );
static	void	botcmd_LookForAmmo( CSkullBot *pBot );
static	void	botcmd_LookForBaseHealth( CSkullBot *pBot );
static	void	botcmd_LookForBaseArmor( CSkullBot *pBot );
static	void	botcmd_LookForSuperHealth( CSkullBot *pBot );
// 10
static	void	botcmd_LookForSuperArmor( CSkullBot *pBot );
static	void	botcmd_LookForPlayerEnemies( CSkullBot *pBot );
static	void	botcmd_GetClosestPlayerEnemy( CSkullBot *pBot );
static	void	botcmd_MoveLeft( CSkullBot *pBot );
static	void	botcmd_MoveRight( CSkullBot *pBot );
static	void	botcmd_MoveForward( CSkullBot *pBot );
static	void	botcmd_MoveBackwards( CSkullBot *pBot );
static	void	botcmd_StopMovement( CSkullBot *pBot );
static	void	botcmd_StopForwardMovement( CSkullBot *pBot );
static	void	botcmd_StopSidewaysMovement( CSkullBot *pBot );
// 20
static	void	botcmd_CheckTerrain( CSkullBot *pBot );
static	void	botcmd_PathToGoal( CSkullBot *pBot );
static	void	botcmd_PathToLastKnownEnemyPosition( CSkullBot *pBot );
static	void	botcmd_PathToLastHeardSound( CSkullBot *pBot );
static	void	botcmd_Roam( CSkullBot *pBot );
static	void	botcmd_GetPathingCostToItem( CSkullBot *pBot );
static	void	botcmd_GetDistanceToItem( CSkullBot *pBot );
static	void	botcmd_GetItemName( CSkullBot *pBot );
static	void	botcmd_IsItemVisible( CSkullBot *pBot );
static	void	botcmd_SetGoal( CSkullBot *pBot );
// 30
static	void	botcmd_BeginAimingAtEnemy( CSkullBot *pBot );
static	void	botcmd_StopAimingAtEnemy( CSkullBot *pBot );
static	void	botcmd_Turn( CSkullBot *pBot );
static	void	botcmd_GetCurrentAngle( CSkullBot *pBot );
static	void	botcmd_SetEnemy( CSkullBot *pBot );
static	void	botcmd_ClearEnemy( CSkullBot *pBot );
static	void	botcmd_IsEnemyAlive( CSkullBot *pBot );
static	void	botcmd_IsEnemyVisible( CSkullBot *pBot );
static	void	botcmd_GetDistanceToEnemy( CSkullBot *pBot );
static	void	botcmd_GetPlayerDamagedBy( CSkullBot *pBot );
// 40
static	void	botcmd_GetEnemyInvulnerabilityTicks( CSkullBot *pBot );
static	void	botcmd_FireWeapon( CSkullBot *pBot );
static	void	botcmd_BeginFiringWeapon( CSkullBot *pBot );
static	void	botcmd_StopFiringWeapon( CSkullBot *pBot );
static	void	botcmd_GetCurrentWeapon( CSkullBot *pBot );
static	void	botcmd_ChangeWeapon( CSkullBot *pBot );
static	void	botcmd_GetWeaponFromItem( CSkullBot *pBot );
static	void	botcmd_IsWeaponOwned( CSkullBot *pBot );
static	void	botcmd_IsFavoriteWeapon( CSkullBot *pBot );
static	void	botcmd_Say( CSkullBot *pBot );
// 50
static	void	botcmd_SayFromFile( CSkullBot *pBot );
static	void	botcmd_SayFromChatFile( CSkullBot *pBot );
static	void	botcmd_BeginChatting( CSkullBot *pBot );
static	void	botcmd_StopChatting( CSkullBot *pBot );
static	void	botcmd_ChatSectionExists( CSkullBot *pBot );
static	void	botcmd_ChatSectionExistsInFile( CSkullBot *pBot );
static	void	botcmd_GetLastChatString( CSkullBot *pBot );
static	void	botcmd_GetChatFrequency( CSkullBot *pBot );
static	void	botcmd_GetLastChatPlayer( CSkullBot *pBot );
static	void	botcmd_Jump( CSkullBot *pBot );
// 60
static	void	botcmd_BeginJumping( CSkullBot *pBot );
static	void	botcmd_StopJumping( CSkullBot *pBot );
static	void	botcmd_Taunt( CSkullBot *pBot );
static	void	botcmd_Respawn( CSkullBot *pBot );
static	void	botcmd_TryToJoinGame( CSkullBot *pBot );
static	void	botcmd_IsDead( CSkullBot *pBot );
static	void	botcmd_IsSpectating( CSkullBot *pBot );
static	void	botcmd_GetHealth( CSkullBot *pBot );
static	void	botcmd_GetArmor( CSkullBot *pBot );
static	void	botcmd_GetBaseHealth( CSkullBot *pBot );
// 70 
static	void	botcmd_GetBaseArmor( CSkullBot *pBot );
static	void	botcmd_GetBotskill( CSkullBot *pBot );
static	void	botcmd_GetAccuracy( CSkullBot *pBot );
static	void	botcmd_GetIntellect( CSkullBot *pBot );
static	void	botcmd_GetAnticipation( CSkullBot *pBot );
static	void	botcmd_GetEvade( CSkullBot *pBot );
static	void	botcmd_GetReactionTime( CSkullBot *pBot );
static	void	botcmd_GetPerception( CSkullBot *pBot );
static	void	botcmd_SetSkillIncrease( CSkullBot *pBot );
static	void	botcmd_IsSkillIncreased( CSkullBot *pBot );
// 80
static	void	botcmd_SetSkillDecrease( CSkullBot *pBot );
static	void	botcmd_IsSkillDecreased( CSkullBot *pBot );
static	void	botcmd_GetGameMode( CSkullBot *pBot );
static	void	botcmd_GetSpread( CSkullBot *pBot );
static	void	botcmd_GetLastJoinedPlayer( CSkullBot *pBot );
static	void	botcmd_GetPlayerName( CSkullBot *pBot );
static	void	botcmd_Print( CSkullBot *pBot );
static	void	botcmd_GetReceivedMedal( CSkullBot *pBot );
static	void	botcmd_ACS_Execute( CSkullBot *pBot );
static	void	botcmd_GetFavoriteWeapon( CSkullBot *pBot );
static	void	botcmd_SayFromLump( CSkullBot *pBot );
// 90
static	void	botcmd_SayFromChatLump( CSkullBot *pBot );
static	void	botcmd_ChatSectionExistsInLump( CSkullBot *pBot );
static	void	botcmd_ChatSectionExistsInChatLump( CSkullBot *pBot );

//*****************************************************************************
//	VARIABLES

static	BOTCMD_s	g_BotCommands[NUM_BOTCMDS] = 
{
	{ "changestate", botcmd_ChangeState, 1, 0, RETURNVAL_VOID },
	{ "delay", botcmd_Delay, 1, 0, RETURNVAL_VOID },
	{ "Random", botcmd_Random, 2, 0, RETURNVAL_INT },
	{ "StringsAreEqual", botcmd_StringsAreEqual, 0, 2, RETURNVAL_BOOLEAN },
	{ "LookForPowerups", botcmd_LookForPowerups, 2, 0, RETURNVAL_INT },
	{ "LookForWeapons", botcmd_LookForWeapons, 2, 0, RETURNVAL_INT },
	{ "LookForAmmo", botcmd_LookForAmmo, 2, 0, RETURNVAL_INT },
	{ "LookForBaseHealth", botcmd_LookForBaseHealth, 2, 0, RETURNVAL_INT },
	{ "LookForBaseArmor", botcmd_LookForBaseArmor, 2, 0, RETURNVAL_INT },
	{ "LookForSuperHealth", botcmd_LookForSuperHealth, 2, 0, RETURNVAL_INT },
	// 10
	{ "LookForSuperArmor", botcmd_LookForSuperArmor, 2, 0, RETURNVAL_INT },
	{ "LookForPlayerEnemies", botcmd_LookForPlayerEnemies, 1, 0, RETURNVAL_INT },
	{ "GetClosestPlayerEnemy", botcmd_GetClosestPlayerEnemy, 0, 0, RETURNVAL_INT },
	{ "MoveLeft", botcmd_MoveLeft, 1, 0, RETURNVAL_VOID },
	{ "MoveRight", botcmd_MoveRight, 1, 0, RETURNVAL_VOID },
	{ "MoveForward", botcmd_MoveForward, 1, 0, RETURNVAL_VOID },
	{ "MoveBackwards", botcmd_MoveBackwards, 1, 0, RETURNVAL_VOID },
	{ "StopMovement", botcmd_StopMovement, 0, 0, RETURNVAL_VOID },
	{ "StopForwardMovement", botcmd_StopForwardMovement, 0, 0, RETURNVAL_VOID },
	{ "StopSidewaysMovement", botcmd_StopSidewaysMovement, 0, 0, RETURNVAL_VOID },
	// 20
	{ "CheckTerrain", botcmd_CheckTerrain, 2, 0, RETURNVAL_INT },
	{ "PathToGoal", botcmd_PathToGoal, 1, 0, RETURNVAL_INT },
	{ "PathToLastKnownEnemyPosition", botcmd_PathToLastKnownEnemyPosition, 1, 0, RETURNVAL_INT },
	{ "PathToLastHeardSound", botcmd_PathToLastHeardSound, 1, 0, RETURNVAL_INT },
	{ "Roam", botcmd_Roam, 1, 0, RETURNVAL_INT },
	{ "GetPathingCostToItem", botcmd_GetPathingCostToItem, 1, 0, RETURNVAL_INT },
	{ "GetDistanceToItem", botcmd_GetDistanceToItem, 1, 0, RETURNVAL_INT },
	{ "GetItemName", botcmd_GetItemName, 1, 0, RETURNVAL_STRING },
	{ "IsItemVisible", botcmd_IsItemVisible, 1, 0, RETURNVAL_BOOLEAN },
	{ "SetGoal", botcmd_SetGoal, 1, 0, RETURNVAL_VOID },
	// 30
	{ "BeginAimingAtEnemy", botcmd_BeginAimingAtEnemy, 0, 0, RETURNVAL_VOID },
	{ "StopAimingAtEnemy", botcmd_StopAimingAtEnemy, 0, 0, RETURNVAL_VOID },
	{ "Turn", botcmd_Turn, 1, 0, RETURNVAL_VOID },
	{ "GetCurrentAngle", botcmd_GetCurrentAngle, 0, 0, RETURNVAL_INT },
	{ "SetEnemy", botcmd_SetEnemy, 1, 0, RETURNVAL_VOID },
	{ "ClearEnemy", botcmd_ClearEnemy, 0, 0, RETURNVAL_VOID },
	{ "IsEnemyAlive", botcmd_IsEnemyAlive, 0, 0, RETURNVAL_BOOLEAN },
	{ "IsEnemyVisible", botcmd_IsEnemyVisible, 0, 0, RETURNVAL_BOOLEAN },
	{ "GetDistanceToEnemy", botcmd_GetDistanceToEnemy, 0, 0, RETURNVAL_INT },
	{ "GetPlayerDamagedBy", botcmd_GetPlayerDamagedBy, 0, 0, RETURNVAL_INT },
	// 40
	{ "GetEnemyInvulnerabilityTicks", botcmd_GetEnemyInvulnerabilityTicks, 0, 0, RETURNVAL_INT },
	{ "FireWeapon", botcmd_FireWeapon, 0, 0, RETURNVAL_VOID },
	{ "BeginFiringWeapon", botcmd_BeginFiringWeapon, 0, 0, RETURNVAL_VOID },
	{ "StopFiringWeapon", botcmd_StopFiringWeapon, 0, 0, RETURNVAL_VOID },
	{ "GetCurrentWeapon", botcmd_GetCurrentWeapon, 0, 0, RETURNVAL_STRING },
	{ "ChangeWeapon", botcmd_ChangeWeapon, 0, 1, RETURNVAL_VOID },
	{ "GetWeaponFromItem", botcmd_GetWeaponFromItem, 1, 0, RETURNVAL_STRING },
	{ "IsWeaponOwned", botcmd_IsWeaponOwned, 1, 0, RETURNVAL_BOOLEAN },
	{ "IsFavoriteWeapon", botcmd_IsFavoriteWeapon, 0, 1, RETURNVAL_BOOLEAN },
	{ "Say", botcmd_Say, 0, 1, RETURNVAL_VOID },
	// 50
	{ "SayFromFile", botcmd_SayFromFile, 0, 2, RETURNVAL_VOID },
	{ "SayFromChatFile", botcmd_SayFromChatFile, 0, 1, RETURNVAL_VOID },
	{ "BeginChatting", botcmd_BeginChatting, 0, 0, RETURNVAL_VOID },
	{ "StopChatting", botcmd_StopChatting, 0, 0, RETURNVAL_VOID },
	{ "ChatSectionExists", botcmd_ChatSectionExists, 0, 1, RETURNVAL_BOOLEAN },
	{ "ChatSectionExistsInFile", botcmd_ChatSectionExistsInFile, 0, 2, RETURNVAL_BOOLEAN },
	{ "GetLastChatString", botcmd_GetLastChatString, 0, 0, RETURNVAL_STRING },
	{ "GetLastChatPlayer", botcmd_GetLastChatPlayer, 0, 0, RETURNVAL_STRING },
	{ "GetChatFrequency", botcmd_GetChatFrequency, 0, 0, RETURNVAL_INT },
	{ "Jump", botcmd_Jump, 0, 0, RETURNVAL_VOID },
	// 60
	{ "BeginJumping", botcmd_BeginJumping, 0, 0, RETURNVAL_VOID },
	{ "StopJumping", botcmd_StopJumping, 0, 0, RETURNVAL_VOID },
	{ "Taunt", botcmd_Taunt, 0, 0, RETURNVAL_VOID },
	{ "Respawn", botcmd_Respawn, 0, 0, RETURNVAL_VOID },
	{ "TryToJoinGame", botcmd_TryToJoinGame, 0, 0, RETURNVAL_VOID },
	{ "IsDead", botcmd_IsDead, 0, 0, RETURNVAL_BOOLEAN },
	{ "IsSpectating", botcmd_IsSpectating, 0, 0, RETURNVAL_BOOLEAN },
	{ "GetHealth", botcmd_GetHealth, 0, 0, RETURNVAL_INT },
	{ "GetArmor", botcmd_GetArmor, 0, 0, RETURNVAL_INT },
	{ "GetBaseHealth", botcmd_GetBaseHealth, 0, 0, RETURNVAL_INT },
	// 70
	{ "GetBaseArmor", botcmd_GetBaseArmor, 0, 0, RETURNVAL_INT },
	{ "GetBotskill", botcmd_GetBotskill, 0, 0, RETURNVAL_INT },
	{ "GetAccuracy", botcmd_GetAccuracy, 0, 0, RETURNVAL_INT },
	{ "GetIntellect", botcmd_GetIntellect, 0, 0, RETURNVAL_INT },
	{ "GetAnticipation", botcmd_GetAnticipation, 0, 0, RETURNVAL_INT },
	{ "GetEvade", botcmd_GetEvade, 0, 0, RETURNVAL_INT },
	{ "GetReactionTime", botcmd_GetReactionTime, 0, 0, RETURNVAL_INT },
	{ "GetPerception", botcmd_GetPerception, 0, 0, RETURNVAL_INT },
	{ "SetSkillIncrease", botcmd_SetSkillIncrease, 1, 0, RETURNVAL_VOID },
	{ "IsSkillIncreased", botcmd_IsSkillIncreased, 0, 0, RETURNVAL_BOOLEAN },
	// 80
	{ "SetSkillDecrease", botcmd_SetSkillDecrease, 1, 0, RETURNVAL_VOID },
	{ "IsSkillDecreased", botcmd_IsSkillDecreased, 0, 0, RETURNVAL_BOOLEAN },
	{ "GetGameMode", botcmd_GetGameMode, 0, 0, RETURNVAL_INT },
	{ "GetSpread", botcmd_GetSpread, 0, 0, RETURNVAL_INT },
	{ "GetLastJoinedPlayer", botcmd_GetLastJoinedPlayer, 0, 0, RETURNVAL_STRING },
	{ "GetPlayerName", botcmd_GetPlayerName, 1, 0, RETURNVAL_STRING },
	{ "GetReceivedMedal", botcmd_GetReceivedMedal, 0, 0, RETURNVAL_INT },
	{ "ACS_Execute", botcmd_ACS_Execute, 5, 0, RETURNVAL_VOID },
	{ "GetFavoriteWeapon", botcmd_GetFavoriteWeapon, 0, 0, RETURNVAL_STRING },
	{ "SayFromLump", botcmd_SayFromLump, 0, 2, RETURNVAL_VOID },
	// 90
	{ "SayFromChatLump", botcmd_SayFromChatLump, 0, 1, RETURNVAL_VOID },
	{ "ChatSectionExistsInLump", botcmd_ChatSectionExistsInLump, 0, 2, RETURNVAL_BOOLEAN },
	{ "ChatSectionExistsInChatLump", botcmd_ChatSectionExistsInChatLump, 0, 1, RETURNVAL_BOOLEAN },
};

static	int			g_iReturnInt = -1;
static	bool		g_bReturnBool = false;
static	char		g_szReturnString[BOTCMD_RETURNSTRING_SIZE] = { 0 };
static	char		g_szLastChatString[256] = { 0 };
static	char		g_szLastChatPlayer[MAXPLAYERNAME+1] = { 0 };
static	char		g_szLastJoinedPlayer[MAXPLAYERNAME+1] = { 0 };
static	FRandom		g_RandomBotCmdSeed( "RandomBotCmdSeed" );
static	FRandom		g_RandomBotChatSeed( "RandomBotChatSeed" );

//*****************************************************************************
//	FUNCTIONS

CChatFile::CChatFile( )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < 64; ulIdx++ )
	{
		Sections[ulIdx].szName[0] = 0;
		Sections[ulIdx].lNumEntries = 0;
	}
}

//*****************************************************************************
//
CChatFile::~CChatFile( )
{
}

//*****************************************************************************
//
bool CChatFile::LoadChatFile( char *pszFileName )
{
	FILE	*pFile;
	char	szFullFileName[1024];

	sprintf( szFullFileName, "%s%s", progdir.GetChars(), pszFileName );

	pFile = fopen( szFullFileName, "r" );
	if ( pFile == NULL )
		return ( false );

	ParseChatFile( pFile );
	fclose( pFile );

	return ( true );
}

//*****************************************************************************
//
void CChatFile::ParseChatFile( void *pvFile )
{
	char				szReadBuffer[1024];
	CHATFILESECTION_t	*pSection = NULL;

	while ( ReadLine( szReadBuffer, sizeof( szReadBuffer ), pvFile ) != NULL )
	{
		char	*pszStart = szReadBuffer;
		char	*pszEndPoint;

		// Remove white space at start of line.
		while (( *pszStart ) && ( *pszStart <= ' ' ))
			pszStart++;

		// Remove comment lines.
		if (( *pszStart == '#' ) || (( pszStart[0] == '/' ) && ( pszStart[1] == '/' )))
			continue;

		// Remove white space at end of line.
		pszEndPoint = pszStart + strlen( pszStart ) - 1;
		while (( pszEndPoint > pszStart ) && ( *pszEndPoint <= ' ' ))
			pszEndPoint--;

		pszEndPoint[1] = 0;
		if ( pszEndPoint <= pszStart )
			continue;	// Nothing here

		// A '[' denotes a new section header.
		if ( *pszStart == '[' )
		{
			if ( *pszEndPoint == ']' )
				*pszEndPoint = 0;

			pSection = AddSection( pszStart + 1 );
		}
		else if ( pSection == NULL )
			return;
		// A '"' denotes a new entry.
		else if ( *pszStart == '\"' )
		{
			if ( *pszEndPoint == '\"' )
				*pszEndPoint = 0;

			if ( pSection )
				AddEntry( pSection, pszStart + 1 );
		}
	}

	return;
}

//*****************************************************************************
//
bool CChatFile::LoadChatLump( char *pszLumpName )
{
//	LONG		lLastLump;

	// If we can't find any lumps that match the given name, return false.
	if ( Wads.CheckNumForName( pszLumpName ) == -1 )
	{
		if ( Wads.CheckNumForFullName( pszLumpName ) == -1 )
			return ( false );
	}

//	lLastLump = 0;
//	if ( Wads.FindLump( pszLumpName, (int *)&lLastLump ) == -1 )
//		return ( false );

	ParseChatLump( pszLumpName );
	return ( true );
}

//*****************************************************************************
//
void CChatFile::ParseChatLump( char *pszLumpName )
{
	LONG		lLump;
//	char		szKey[16];
//	char		szValue[32];
//	char				szReadBuffer[1024];
	CHATFILESECTION_t	*pSection = NULL;

	// Find the last instance of the given lumpname in all the wads.
	lLump = Wads.CheckNumForName( pszLumpName );
	if ( lLump == -1 )
		lLump = Wads.GetNumForFullName( pszLumpName );

	FScanner sc( lLump );

	while ( sc.GetString( ))
	{
		char	*pszStart = sc.String;
		char	*pszEndPoint;

		// Remove white space at start of line.
		while (( *pszStart ) && ( *pszStart <= ' ' ))
			pszStart++;

		// Remove comment lines.
		if (( *pszStart == '#' ) || (( pszStart[0] == '/' ) && ( pszStart[1] == '/' )))
			continue;

		// Remove white space at end of line.
		pszEndPoint = pszStart + strlen( pszStart ) - 1;
		while (( pszEndPoint > pszStart ) && ( *pszEndPoint <= ' ' ))
			pszEndPoint--;

		pszEndPoint[1] = 0;
		if ( pszEndPoint <= pszStart )
			continue;	// Nothing here

		// A '[' denotes a new section header.
		if ( *pszStart == '[' )
		{
			if ( *pszEndPoint == ']' )
				*pszEndPoint = 0;

			pSection = AddSection( pszStart + 1 );
		}
		else if ( pSection == NULL )
			return;
		else
			AddEntry( pSection, pszStart );
	}

	return;
}

//*****************************************************************************
//
CHATFILESECTION_t *CChatFile::AddSection( char *pszName )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < 64; ulIdx++ )
	{
		if ( Sections[ulIdx].szName[0] == 0 )
			break;
	}

	// Maxed out.
	if ( ulIdx == 64 )
		return ( NULL );

	sprintf( Sections[ulIdx].szName, "%s", pszName );
	return ( &Sections[ulIdx] );
}

//*****************************************************************************
//
void CChatFile::AddEntry( CHATFILESECTION_t *pSection, char *pszName )
{
	// Maxed out.
	if ( pSection->lNumEntries == 64 )
		return;

	sprintf( pSection->szEntry[pSection->lNumEntries++], "%s", pszName );
}

//*****************************************************************************
//
LONG CChatFile::FindSection( char *pszName )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < 64; ulIdx++ )
	{
		if ( stricmp( Sections[ulIdx].szName, pszName ) == 0 )
			return ( ulIdx );
	}

	return ( -1 );
}

//*****************************************************************************
//
char *CChatFile::ChooseRandomEntry( char *pszSection )
{
	LONG	lIdx;
	LONG	lIdx2;

	lIdx = FindSection( pszSection );
	if (( lIdx == -1 ) || ( Sections[lIdx].lNumEntries <= 0 ))
	{
		static char str_null[5] = "NULL";
		return ( str_null );
	}

	lIdx2 = g_RandomBotChatSeed.Random( ) % Sections[lIdx].lNumEntries;
	return ( Sections[lIdx].szEntry[lIdx2] );
}

//*****************************************************************************
//
char *CChatFile::ReadLine( char *pszString, LONG lSize, void *pvFile )
{
	return ( fgets( pszString, lSize, (FILE *)pvFile ));
}

//*****************************************************************************
//*****************************************************************************
//
void BOTCMD_Construct( void )
{
}

//*****************************************************************************
//
void BOTCMD_RunCommand( BOTCMD_e Command, CSkullBot *pBot )
{
	SDWORD	sdwNumArgs;
//	LONG	lExpectedStackPosition;

	pBot->GetRawScriptData( )->Read( &sdwNumArgs, sizeof( SDWORD ));
	pBot->IncrementScriptPosition( sizeof( SDWORD ));

//	if ( sdwNumArgs != g_BotCommands[Command].lNumArgs )
	if ( pBot->m_ScriptData.lStackPosition < g_BotCommands[Command].lNumArgs )
		I_Error( "Wrong number of arguments (%d) in bot command, %s!", static_cast<int> (pBot->m_ScriptData.lStackPosition), g_BotCommands[Command].pszName );
	if ( pBot->m_ScriptData.lStringStackPosition < g_BotCommands[Command].lNumStringArgs )
		I_Error( "Wrong number of arguments (%d) in bot command, %s!", static_cast<int> (pBot->m_ScriptData.lStringStackPosition), g_BotCommands[Command].pszName );

	// This is the expected stack position after the function is called.
//	lExpectedStackPosition = pBot->m_ScriptData.lStackPosition - g_BotCommands[Command].lNumArgs;

	if ( botdebug_commands )
		Printf( "bot %s command: %s\n", pBot->GetPlayer( )->userinfo.GetName(), g_BotCommands[Command].pszName );
	g_BotCommands[Command].pvFunction( pBot );

//	if ( pBot->m_ScriptData.lStackPosition != lExpectedStackPosition )
//		I_Error( "BOTCMD_RunCommand: Wrong number of arguments used in bot command %s!", g_BotCommands[Command].pszName );

	// If this function has a return value, push it to the stack.
	switch ( g_BotCommands[Command].ReturnType )
	{
	case RETURNVAL_INT:

		pBot->PushToStack( g_iReturnInt );
		break;
	case RETURNVAL_BOOLEAN:

		pBot->PushToStack( g_bReturnBool );
		break;
	case RETURNVAL_STRING:

		pBot->PushToStringStack( g_szReturnString );
		break;
//	case RETURNVAL_VOID:

//		if ( pBot->m_ScriptData.lStackPosition != 0 )
//			I_Error( "BOTCMD_RunCommand: Stack position not 0 after \"void\" function, %s!", g_BotCommands[Command].pszName );
//		break;
	default:

		break;
	}
}

//*****************************************************************************
//
void BOTCMD_SetLastChatString( const char *pszString )
{
	strncpy( g_szLastChatString, pszString, 255 );
	g_szLastChatString[255] = 0;
}

//*****************************************************************************
//
void BOTCMD_SetLastChatPlayer( const char *pszString )
{
	strncpy( g_szLastChatPlayer, pszString, MAXPLAYERNAME );
	g_szLastChatPlayer[MAXPLAYERNAME] = 0;
}

//*****************************************************************************
//
void BOTCMD_SetLastJoinedPlayer( const char *pszString )
{
	strncpy( g_szLastJoinedPlayer, pszString, MAXPLAYERNAME );
	g_szLastJoinedPlayer[MAXPLAYERNAME] = 0;
}

//*****************************************************************************
//
void BOTCMD_DoChatStringSubstitutions( CSkullBot *pBot, const char *pszInString, char *pszOutString )
{
	do
	{
		// Continue to copy the instring to the outstring until we hit a '$'.
		if ( *pszInString != '$' )
			*pszOutString++ = *pszInString;
		else
		{

			if (( strnicmp( pszInString + 1, "player_damagedby", strlen( "player_damagedby" )) == 0 ) && ( pBot->m_ulLastPlayerDamagedBy != MAXPLAYERS ))
			{
				sprintf( pszOutString, "%s\\c-", players[pBot->m_ulLastPlayerDamagedBy].userinfo.GetName() );
				pszOutString += strlen ( players[pBot->m_ulLastPlayerDamagedBy].userinfo.GetName() );
				pszOutString += strlen( "\\c-" );

				pszInString += strlen( "player_damagedby" );
			}
			else if (( strnicmp( pszInString + 1, "player_enemy", strlen( "player_enemy" )) == 0 ) && ( pBot->m_ulPlayerEnemy != MAXPLAYERS ))
			{
				sprintf( pszOutString, "%s\\c-", players[pBot->m_ulPlayerEnemy].userinfo.GetName() );
				pszOutString += strlen( players[pBot->m_ulPlayerEnemy].userinfo.GetName() );
				pszOutString += strlen( "\\c-" );

				pszInString += strlen( "player_enemy" );
			}
			else if (( strnicmp( pszInString + 1, "player_killedby", strlen( "player_killedby" )) == 0 ) && ( pBot->m_ulPlayerKilledBy != MAXPLAYERS ))
			{
				sprintf( pszOutString, "%s\\c-", players[pBot->m_ulPlayerKilledBy].userinfo.GetName() );
				pszOutString += strlen( players[pBot->m_ulPlayerKilledBy].userinfo.GetName() );
				pszOutString += strlen( "\\c-" );

				pszInString += strlen( "player_killedby" );
			}
			else if (( strnicmp( pszInString + 1, "player_killed", strlen( "player_killed" )) == 0 ) && ( pBot->m_ulPlayerKilled != MAXPLAYERS ))
			{
				sprintf( pszOutString, "%s\\c-", players[pBot->m_ulPlayerKilled].userinfo.GetName() );
				pszOutString += strlen( players[pBot->m_ulPlayerKilled].userinfo.GetName() );
				pszOutString += strlen( "\\c-" );

				pszInString += strlen( "player_killed" );
			}
			else if ( strnicmp( pszInString + 1, "player_inlead", strlen( "player_inlead" )) == 0 )
			{
				ULONG	ulBestPlayer = 0;
				ULONG	ulIdx;

				for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
				{
					if (( playeringame[ulIdx] == false ) || ( PLAYER_IsTrueSpectator( &players[ulIdx] )))
						continue;

					if ( players[ulIdx].fragcount > players[ulBestPlayer].fragcount )
						ulBestPlayer = ulIdx;
				}

				sprintf( pszOutString, "%s\\c-", players[ulBestPlayer].userinfo.GetName() );
				pszOutString += strlen( players[ulBestPlayer].userinfo.GetName() );
				pszOutString += strlen( "\\c-" );

				pszInString += strlen( "player_inlead" );
			}
			else if ( strnicmp( pszInString + 1, "player_lastplace", strlen( "player_lastplace" )) == 0 )
			{
				ULONG	ulBestPlayer = 0;
				ULONG	ulIdx;

				for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
				{
					if (( playeringame[ulIdx] == false ) || ( PLAYER_IsTrueSpectator( &players[ulIdx] )))
						continue;

					if ( players[ulIdx].fragcount < players[ulBestPlayer].fragcount )
						ulBestPlayer = ulIdx;
				}

				sprintf( pszOutString, "%s\\c-", players[ulBestPlayer].userinfo.GetName() );
				pszOutString += strlen( players[ulBestPlayer].userinfo.GetName() );
				pszOutString += strlen( "\\c-" );

				pszInString += strlen( "player_lastplace" );
			}
			else if ( strnicmp( pszInString + 1, "player_random_notself", strlen( "player_random_notself" )) == 0 )
			{
				ULONG	ulNumPlayers;
				ULONG	ulIdx;
				ULONG	ulPlayer;

				ulNumPlayers = 0;
				for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
				{
					if ( playeringame[ulIdx] )
						ulNumPlayers++;
				}

				if ( ulNumPlayers <= 1 )
					ulPlayer = pBot->GetPlayer( ) - players;
				else
				{
					do
					{
						ulPlayer = g_RandomBotCmdSeed.Random( ) % MAXPLAYERS;
					}
					while (( ulPlayer == static_cast<unsigned> ( pBot->GetPlayer( ) - players )) || ( playeringame[ulPlayer] == false ));
				}
				
				sprintf( pszOutString, "%s\\c-", players[ulPlayer].userinfo.GetName() );
				pszOutString += strlen( players[ulPlayer].userinfo.GetName() );
				pszOutString += strlen( "\\c-" );

				pszInString += strlen( "player_random_notself" );
			}
			else if ( strnicmp( pszInString + 1, "player_random", strlen( "player_random" )) == 0 )
			{
				ULONG	ulPlayer;

				do
				{
					ulPlayer = g_RandomBotCmdSeed.Random( ) % MAXPLAYERS;
				}
				while ( playeringame[ulPlayer] == false );
				
				sprintf( pszOutString, "%s\\c-", players[ulPlayer].userinfo.GetName() );
				pszOutString += strlen( players[ulPlayer].userinfo.GetName() );
				pszOutString += strlen( "\\c-" );

				pszInString += strlen( "player_random" );
			}
			else if (( strnicmp( pszInString + 1, "player_lastchat", strlen( "player_lastchat" )) == 0 ) && ( g_szLastChatPlayer[0] != 0 ))
			{				
				sprintf( pszOutString, "%s\\c-", g_szLastChatPlayer );
				pszOutString += strlen( g_szLastChatPlayer );
				pszOutString += strlen( "\\c-" );

				pszInString += strlen( "player_lastchat" );
			}
			else if ( strnicmp( pszInString + 1, "level_name", strlen( "level_name" )) == 0 )
			{				
				sprintf( pszOutString, "%s\\c-", level.LevelName.GetChars() );
				pszOutString += strlen( level.LevelName.GetChars() );
				pszOutString += strlen( "\\c-" );

				pszInString += strlen( "level_name" );
			}
			else if ( strnicmp( pszInString + 1, "map_name", strlen( "map_name" )) == 0 )
			{				
				sprintf( pszOutString, "%s\\c-", level.mapname );
				pszOutString += strlen( level.mapname );
				pszOutString += strlen( "\\c-" );

				pszInString += strlen( "map_name" );
			}
			else
				*pszOutString++ = '$';
		}

	} while ( *pszInString++ );
}

//*****************************************************************************
//
bool BOTCMD_IgnoreItem( CSkullBot *pBot, LONG lIdx, bool bVisibilityCheck )
{
	AActor *pActor = g_NetIDList.findPointerByID ( lIdx );
	if (( pActor == NULL ) ||
		(( pActor->flags & MF_SPECIAL ) == false ) ||
		( bVisibilityCheck && ( BOTS_IsVisible( pBot->GetPlayer( )->mo, pActor ) == false )))
	{
		return ( true );
	}

	return ( false );
}

//*****************************************************************************
//*****************************************************************************
//
static void botcmd_ChangeState( CSkullBot *pBot )
{
	LONG	lBuffer;

	lBuffer = pBot->m_ScriptData.alStack[pBot->m_ScriptData.lStackPosition - 1];
	pBot->PopStack( );

	if (( lBuffer < 0 ) || ( lBuffer >= MAX_NUM_STATES ))
		I_Error( "botcmd_ChangeState: Illegal state index, %d!", static_cast<int> (lBuffer) );

	// Don't change to another state while we're in the middle of changing to a state.
	if ( pBot->m_ScriptData.bExitingState )
		return;

	pBot->SetExitingState( true );
	pBot->SetNextState( lBuffer );
}

//*****************************************************************************
//
static void botcmd_Delay( CSkullBot *pBot )
{
	LONG	lBuffer;

	lBuffer = pBot->m_ScriptData.alStack[pBot->m_ScriptData.lStackPosition - 1];
	pBot->PopStack( );

	if ( lBuffer < 0 )
		I_Error( "botcmd_Delay: Illegal delay value, %d", static_cast<int> (lBuffer) );

	// Set the script delay.
	if ( pBot->m_ScriptData.bInEvent )
		pBot->SetScriptEventDelay( lBuffer );
	else
		pBot->SetScriptDelay( lBuffer );
}

//*****************************************************************************
//
static void botcmd_Random( CSkullBot *pBot )
{
	LONG	lMin;
	LONG	lMax;

	lMax = pBot->m_ScriptData.alStack[pBot->m_ScriptData.lStackPosition - 1];
	pBot->PopStack( );

	lMin = pBot->m_ScriptData.alStack[pBot->m_ScriptData.lStackPosition - 1];
	pBot->PopStack( );

	if ( lMax < lMin )
		std::swap ( lMax, lMin );

	if ( lMax == lMin )
	{
		g_iReturnInt = lMax;
		return;
	}

	g_iReturnInt = g_RandomBotCmdSeed.Random( ) % ( lMax - lMin + 1 ) + lMin;
}

//*****************************************************************************
//
static void botcmd_StringsAreEqual( CSkullBot *pBot )
{
	char	szString1[256];
	char	szString2[256];

	// Read in the string index of our first argument.
	sprintf( szString1, "%s", pBot->m_ScriptData.aszStringStack[pBot->m_ScriptData.lStringStackPosition - 1] );
	pBot->PopStringStack( );

	// Read in the string index of our second argument.
	sprintf( szString2, "%s", pBot->m_ScriptData.aszStringStack[pBot->m_ScriptData.lStringStackPosition - 1] );
	pBot->PopStringStack( );

	if (( strlen( szString1 ) >= 256 ) || ( strlen( szString2 ) >= 256 ))
		Printf( "CRAP\n" );

	g_bReturnBool = ( stricmp( szString1, szString2 ) == 0 );
}

//*****************************************************************************
// [BB] Helperfunction to reduce code duplication.
template <typename T>
int botcmd_LookForItemType( CSkullBot *pBot, const char *FunctionName )
{
	LONG		lIdx;
	bool		bVisibilityCheck;

	bVisibilityCheck = !!pBot->m_ScriptData.alStack[pBot->m_ScriptData.lStackPosition - 1];
	pBot->PopStack( );

	lIdx = pBot->m_ScriptData.alStack[pBot->m_ScriptData.lStackPosition - 1];
	pBot->PopStack( );

	if (( lIdx < 0 ) || ( lIdx >= IDList<AActor>::MAX_NETID ))
		I_Error( "%s: Illegal item index, %d!", FunctionName, static_cast<int> (lIdx) );

	while (( BOTCMD_IgnoreItem( pBot, lIdx, bVisibilityCheck )) ||
		( g_NetIDList.findPointerByID ( lIdx )->GetClass( )->IsDescendantOf( RUNTIME_CLASS( T )) == false ))
	{
		if ( ++lIdx == IDList<AActor>::MAX_NETID )
			break;
	}

	if ( lIdx == IDList<AActor>::MAX_NETID )
		return g_iReturnInt = -1;
	else
		return g_iReturnInt = lIdx;
}

//*****************************************************************************
//
static void botcmd_LookForPowerups( CSkullBot *pBot )
{
	g_iReturnInt = botcmd_LookForItemType<APowerupGiver> ( pBot, "botcmd_LookForPowerups" );
}

//*****************************************************************************
//
static void botcmd_LookForWeapons( CSkullBot *pBot )
{
	g_iReturnInt = botcmd_LookForItemType<AWeapon> ( pBot, "botcmd_LookForWeapons" );
}

//*****************************************************************************
//
static void botcmd_LookForAmmo( CSkullBot *pBot )
{
	g_iReturnInt = botcmd_LookForItemType<AAmmo> ( pBot, "botcmd_LookForAmmo" );
}

//*****************************************************************************
// [BB] Helperfunction to reduce code duplication.
int botcmd_LookForItemWithFlag( CSkullBot *pBot, const int Flag, const char *FunctionName )
{
	LONG		lIdx;
	bool		bVisibilityCheck;

	bVisibilityCheck = !!pBot->m_ScriptData.alStack[pBot->m_ScriptData.lStackPosition - 1];
	pBot->PopStack( );

	lIdx = pBot->m_ScriptData.alStack[pBot->m_ScriptData.lStackPosition - 1];
	pBot->PopStack( );

	if (( lIdx < 0 ) || ( lIdx >= IDList<AActor>::MAX_NETID ))
		I_Error( "%s: Illegal item index, %d!", FunctionName, static_cast<int> (lIdx) );

	while (( BOTCMD_IgnoreItem( pBot, lIdx, bVisibilityCheck )) ||
		(( g_NetIDList.findPointerByID ( lIdx )->ulSTFlags & Flag ) == false ))
	{
		if ( ++lIdx == IDList<AActor>::MAX_NETID )
			break;
	}

	if ( lIdx == IDList<AActor>::MAX_NETID )
		return -1;
	else
		return lIdx;
}

//*****************************************************************************
//
static void botcmd_LookForBaseHealth( CSkullBot *pBot )
{
	g_iReturnInt = botcmd_LookForItemWithFlag ( pBot, STFL_BASEHEALTH, "botcmd_LookForBaseHealth" );
}

//*****************************************************************************
//
static void botcmd_LookForBaseArmor( CSkullBot *pBot )
{
	g_iReturnInt = botcmd_LookForItemWithFlag ( pBot, STFL_BASEARMOR, "botcmd_LookForBaseArmor" );
}

//*****************************************************************************
//
static void botcmd_LookForSuperHealth( CSkullBot *pBot )
{
	g_iReturnInt = botcmd_LookForItemWithFlag ( pBot, STFL_SUPERHEALTH, "botcmd_LookForSuperHealth" );
}

//*****************************************************************************
//
static void botcmd_LookForSuperArmor( CSkullBot *pBot )
{
	g_iReturnInt = botcmd_LookForItemWithFlag ( pBot, STFL_SUPERARMOR, "botcmd_LookForSuperArmor" );
}

//*****************************************************************************
//
static void botcmd_LookForPlayerEnemies( CSkullBot *pBot )
{
	ULONG	ulIdx;
	LONG	lStartPlayer;

	lStartPlayer = pBot->m_ScriptData.alStack[pBot->m_ScriptData.lStackPosition - 1];
	pBot->PopStack( );

	if (( lStartPlayer < 0 ) || ( lStartPlayer >= MAXPLAYERS ))
		I_Error( "botcmd_LookForPlayerEnemies: Illegal player start index, %d!", static_cast<int> (lStartPlayer) );

	if ( pBot->GetPlayer( )->health <= 0 )
		g_iReturnInt = -1;

	for ( ulIdx = lStartPlayer; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( playeringame[ulIdx] == false ) ||
			( ulIdx == (ULONG)( pBot->GetPlayer( ) - players )) ||
			( players[ulIdx].mo == NULL ) ||
			( players[ulIdx].health <= 0 ) ||
			( players[ulIdx].mo->IsTeammate( pBot->GetPlayer( )->mo )))
		{
			continue;
		}

		// Check if we have a line of sight to this player.
		if ( P_CheckSight( pBot->GetPlayer( )->mo, players[ulIdx].mo, SF_SEEPASTBLOCKEVERYTHING ))
		{
			angle_t	Angle;

			Angle = R_PointToAngle2( pBot->GetPlayer( )->mo->x,
								  pBot->GetPlayer( )->mo->y, 
								  players[ulIdx].mo->x,
								  players[ulIdx].mo->y );

			Angle -= pBot->GetPlayer( )->mo->angle;

			// If he's within our view range, tell the bot.
			if (( Angle <= ANG45 ) || ( Angle >= ((ULONG)ANGLE_1 * 315 )))
			{
				pBot->m_ulLastSeenPlayer = ulIdx;
				g_iReturnInt = ulIdx;
				return;
			}
		}
	}

	g_iReturnInt = -1;
}

//*****************************************************************************
//
static void botcmd_GetClosestPlayerEnemy( CSkullBot *pBot )
{
	ULONG	ulIdx;
	LONG	lDistance;
	LONG	lClosestDistance = 0;
	LONG	lClosestPlayer;
	angle_t	Angle;

	if ( pBot->GetPlayer( )->health <= 0 )
		g_iReturnInt = -1;

	lClosestPlayer = -1;
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( playeringame[ulIdx] == false ) ||
			( ulIdx == (ULONG)( pBot->GetPlayer( ) - players )) ||
			( players[ulIdx].mo == NULL ) ||
			( players[ulIdx].health <= 0 ) ||
			( players[ulIdx].bSpectating ) ||
			( players[ulIdx].mo->IsTeammate( pBot->GetPlayer( )->mo )))
		{
			continue;
		}

		// Check if we have a line of sight to this player.
		if ( P_CheckSight( pBot->GetPlayer( )->mo, players[ulIdx].mo, SF_SEEPASTBLOCKEVERYTHING ) == false )
			continue;

		Angle = R_PointToAngle2( pBot->GetPlayer( )->mo->x,
							  pBot->GetPlayer( )->mo->y, 
							  players[ulIdx].mo->x,
							  players[ulIdx].mo->y );

		Angle -= pBot->GetPlayer( )->mo->angle;

		// If this player can't be seen, skip him.
		if (( Angle > ANG45 ) && ( Angle < ((ULONG)ANGLE_1 * 315 )))
			continue;

		// If at this point, we haven't found a valid player... then now this guy's it!
		if ( lClosestPlayer == -1 )
		{
			lClosestPlayer = ulIdx;
			lClosestDistance = P_AproxDistance( pBot->GetPlayer( )->mo->x - players[ulIdx].mo->x, 
												pBot->GetPlayer( )->mo->y - players[ulIdx].mo->y );
			continue;
		}

		lDistance = P_AproxDistance( pBot->GetPlayer( )->mo->x - players[ulIdx].mo->x, 
									 pBot->GetPlayer( )->mo->y - players[ulIdx].mo->y );
		if ( lDistance < lClosestDistance )
		{
			lClosestPlayer = ulIdx;
			lClosestDistance = lDistance;
		}
	}

	g_iReturnInt = lClosestPlayer;
}

//*****************************************************************************
//
static void botcmd_MoveLeft( CSkullBot *pBot )
{
	float	fSpeed;

	fSpeed = (float)pBot->m_ScriptData.alStack[pBot->m_ScriptData.lStackPosition - 1];
	pBot->PopStack( );

	if ( fSpeed < 0 )
		fSpeed = 0;
	if ( fSpeed > 100 )
		fSpeed = 100;

	pBot->m_lSideMove = static_cast<LONG>( -0x32 * ( fSpeed / 100.0f ));
	pBot->m_bSideMovePersist = true;
}

//*****************************************************************************
//
static void botcmd_MoveRight( CSkullBot *pBot )
{
	float	fSpeed;

	fSpeed = (float)pBot->m_ScriptData.alStack[pBot->m_ScriptData.lStackPosition - 1];
	pBot->PopStack( );

	if ( fSpeed < 0 )
		fSpeed = 0;
	if ( fSpeed > 100 )
		fSpeed = 100;

	pBot->m_lSideMove = static_cast<LONG>( 0x32 * ( fSpeed / 100.0f ));
	pBot->m_bSideMovePersist = true;
}

//*****************************************************************************
//
static void botcmd_MoveForward( CSkullBot *pBot )
{
	float	fSpeed;

	fSpeed = (float)pBot->m_ScriptData.alStack[pBot->m_ScriptData.lStackPosition - 1];
	pBot->PopStack( );

	if ( fSpeed < 0 )
		fSpeed = 0;
	if ( fSpeed > 100 )
		fSpeed = 100;

	pBot->m_lForwardMove = static_cast<LONG>( 0x32 * ( fSpeed / 100.0f ));
	pBot->m_bForwardMovePersist = true;
}

//*****************************************************************************
//
static void botcmd_MoveBackwards( CSkullBot *pBot )
{
	float	fSpeed;

	fSpeed = (float)pBot->m_ScriptData.alStack[pBot->m_ScriptData.lStackPosition - 1];
	pBot->PopStack( );

	if ( fSpeed < 0 )
		fSpeed = 0;
	if ( fSpeed > 100 )
		fSpeed = 100;

	pBot->m_lForwardMove = static_cast<LONG>( -0x32 * ( fSpeed / 100.0f ));
	pBot->m_bForwardMovePersist = true;
}

//*****************************************************************************
//
static void botcmd_StopMovement( CSkullBot *pBot )
{
	pBot->m_lForwardMove = 0;
	pBot->m_lSideMove = 0;

	pBot->m_bForwardMovePersist = false;
	pBot->m_bSideMovePersist = false;
}

//*****************************************************************************
//
static void botcmd_StopForwardMovement( CSkullBot *pBot )
{
	pBot->m_lForwardMove = 0;
	pBot->m_bForwardMovePersist = false;
}

//*****************************************************************************
//
static void botcmd_StopSidewaysMovement( CSkullBot *pBot )
{
	pBot->m_lSideMove = 0;
	pBot->m_bSideMovePersist = false;
}

//*****************************************************************************
//
static void botcmd_CheckTerrain( CSkullBot *pBot )
{
	LONG		lAngle;
	LONG		lDistance;
	fixed_t		DestX;
	fixed_t		DestY;
	fixed_t		Angle;

	lDistance = pBot->m_ScriptData.alStack[pBot->m_ScriptData.lStackPosition - 1];
	pBot->PopStack( );

	lAngle = pBot->m_ScriptData.alStack[pBot->m_ScriptData.lStackPosition - 1];
	pBot->PopStack( );

	lAngle *= ANGLE_1;
	if ( lAngle < 0 )
		lAngle = ANGLE_MAX - labs( lAngle );

	Angle = pBot->GetPlayer( )->mo->angle;
	Angle += lAngle;

	Angle >>= ANGLETOFINESHIFT;

	DestX = pBot->GetPlayer( )->mo->x * finecosine[Angle];
	DestY = pBot->GetPlayer( )->mo->y * finesine[Angle];

	g_iReturnInt = BOTPATH_TryWalk( pBot->GetPlayer( )->mo, pBot->GetPlayer( )->mo->x, pBot->GetPlayer( )->mo->y, pBot->GetPlayer( )->mo->z, DestX, DestY );
}

//*****************************************************************************
//
static void botcmd_PathToGoal( CSkullBot *pBot )
{
	POS_t				GoalPos;
	angle_t				Angle;
	ULONG				ulFlags;
	float				fSpeed;
	ASTARRETURNSTRUCT_t	ReturnVal;

	fSpeed = (float)pBot->m_ScriptData.alStack[pBot->m_ScriptData.lStackPosition - 1];
	pBot->PopStack( );

	if ( fSpeed < 0 )
		fSpeed = 0;
	if ( fSpeed > 100 )
		fSpeed = 100;

	if ( pBot->m_pGoalActor == NULL )
	{
		// This will print after a bot picks up an item due to reaction time :(
//		Printf( "PathToGoal: WARNING! Attempting to path to goal without a goal set!\n" );
		return;
	}

	if (( botdebug_showgoal ) && ( pBot->GetPlayer( )->mo->CheckLocalView( consoleplayer )) && ( NETWORK_GetState( ) != NETSTATE_SERVER ))
		Printf( "%s (%d, %d)\n", pBot->m_pGoalActor->GetClass( )->TypeName.GetChars( ), pBot->m_pGoalActor->x / FRACUNIT, pBot->m_pGoalActor->y / FRACUNIT );

	// First, if the bot has a roam position, clear out his path.
	if ( pBot->m_ulPathType != BOTPATHTYPE_ITEM )
	{
		ASTAR_ClearPath( pBot->GetPlayer( ) - players );
		pBot->m_ulPathType = BOTPATHTYPE_ITEM;

		pBot->m_PathGoalPos.x = pBot->m_pGoalActor->x;
		pBot->m_PathGoalPos.y = pBot->m_pGoalActor->y;
		pBot->m_PathGoalPos.z = pBot->m_pGoalActor->z;
	}

	ReturnVal = ASTAR_Path( pBot->GetPlayer( ) - players, pBot->m_PathGoalPos, botdebug_maxsearchnodes, 0 );//1 );//MAX_NODES_TO_SEARCH );

	if ( ReturnVal.ulFlags & PF_COMPLETE )
	{
		if (( ReturnVal.pNode == NULL ) || (( ReturnVal.ulFlags & PF_SUCCESS ) == false ))
		{
			g_iReturnInt = PATH_UNREACHABLE;

			// No need to clear this. It'll get cleared next time the bot tries to make a path.
//			ASTAR_ClearPath( pBot->GetPlayer( ) - players );
			pBot->m_ulPathType = BOTPATHTYPE_NONE;
			return;
		}

		if ( ReturnVal.bIsGoal )
		{
			GoalPos.x = pBot->m_PathGoalPos.x;
			GoalPos.y = pBot->m_PathGoalPos.y;
			GoalPos.z = 0;
		}
		else
			GoalPos = ASTAR_GetPosition( ReturnVal.pNode );
	}
	else
	{
		g_iReturnInt = PATH_INCOMPLETE;
		return;
	}

	Angle = R_PointToAngle2( pBot->GetPlayer( )->mo->x,
							  pBot->GetPlayer( )->mo->y, 
							  GoalPos.x,
							  GoalPos.y );

	pBot->GetPlayer( )->mo->angle = Angle;
	pBot->GetPlayer( )->cmd.ucmd.forwardmove = static_cast<short> ( ( 0x32 << 8 ) * ( fSpeed / 100.0f ) );

	// We don't need GoalPos anymore, so we can corrupt it! KEKE!
	Angle = Angle >> ANGLETOFINESHIFT;
	GoalPos.x = pBot->GetPlayer( )->mo->x + ( USERANGE >> FRACBITS ) * finecosine[Angle];
	GoalPos.y = pBot->GetPlayer( )->mo->y + ( USERANGE >> FRACBITS ) * finesine[Angle];

	ulFlags = BOTPATH_TryWalk( pBot->GetPlayer( )->mo, pBot->GetPlayer( )->mo->x, pBot->GetPlayer( )->mo->y, pBot->GetPlayer( )->mo->z, GoalPos.x, GoalPos.y );
	if ( ulFlags & BOTPATH_JUMPABLELEDGE )
		pBot->GetPlayer( )->cmd.ucmd.buttons |= BT_JUMP;
	if ( ulFlags & BOTPATH_DOOR )
	{
		sector_t	*pSector;
		DDoor		*pDoor;

		pSector = BOTPATH_GetDoorSector( );
		if ( pSector != NULL )
		{
			TThinkerIterator<DDoor>		Iterator;
			while (( pDoor = Iterator.Next( )))
			{
				if ( pSector == pDoor->GetSector( ))
				{
					// Door is already rising.
					if ( pDoor->GetDirection( ) == 1 )
					{
						g_iReturnInt = PATH_COMPLETE;
						return;
					}

					break;
				}
			}
		}

		// At this point, either the door has not been created yet, or is closing or at a height
		// that still blocks us. Therefore, try to open the door.
		pBot->GetPlayer( )->cmd.ucmd.buttons |= BT_USE;
		pBot->GetPlayer( )->oldbuttons &= ~BT_USE;
	}

	g_iReturnInt = PATH_COMPLETE;
}

//*****************************************************************************
//
static void botcmd_PathToLastKnownEnemyPosition( CSkullBot *pBot )
{
	POS_t				GoalPos;
	angle_t				Angle;
	ULONG				ulFlags;
	float				fSpeed;
	ASTARRETURNSTRUCT_t	ReturnVal;
	fixed_t				Distance;

	fSpeed = (float)pBot->m_ScriptData.alStack[pBot->m_ScriptData.lStackPosition - 1];
	pBot->PopStack( );

	if ( fSpeed < 0 )
		fSpeed = 0;
	if ( fSpeed > 100 )
		fSpeed = 100;

	// First, if the bot has a roam position, clear out his path.
	if ( pBot->m_ulPathType != BOTPATHTYPE_ENEMYPOSITION )
	{
		ASTAR_ClearPath( pBot->GetPlayer( ) - players );
		pBot->m_ulPathType = BOTPATHTYPE_ENEMYPOSITION;

		pBot->m_PathGoalPos = pBot->GetLastEnemyPosition( );
	}

	ReturnVal = ASTAR_Path( pBot->GetPlayer( ) - players, pBot->m_PathGoalPos, botdebug_maxsearchnodes, 0 );//1 );//MAX_NODES_TO_SEARCH );

	if ( ReturnVal.ulFlags & PF_COMPLETE )
	{
		if (( ReturnVal.pNode == NULL ) || (( ReturnVal.ulFlags & PF_SUCCESS ) == false ))
		{
			g_iReturnInt = PATH_UNREACHABLE;

			// No need to clear this. It'll get cleared next time the bot tries to make a path.
//			ASTAR_ClearPath( pBot->GetPlayer( ) - players );
			pBot->m_ulPathType = BOTPATHTYPE_NONE;
			return;
		}

		if ( ReturnVal.bIsGoal )
		{
			GoalPos.x = pBot->m_PathGoalPos.x;
			GoalPos.y = pBot->m_PathGoalPos.y;
			GoalPos.z = 0;
		}
		else
			GoalPos = ASTAR_GetPosition( ReturnVal.pNode );
	}
	else
	{
		g_iReturnInt = PATH_INCOMPLETE;
		return;
	}

	Angle = R_PointToAngle2( pBot->GetPlayer( )->mo->x,
							  pBot->GetPlayer( )->mo->y, 
							  GoalPos.x,
							  GoalPos.y );

	pBot->GetPlayer( )->mo->angle = Angle;
	pBot->GetPlayer( )->cmd.ucmd.forwardmove = static_cast<short> ( ( 0x32 << 8 ) * ( fSpeed / 100.0f ) );

	Distance = pBot->GetPlayer( )->mo->radius;
	if (( abs( pBot->GetPlayer( )->mo->x - GoalPos.x ) >= Distance ) || ( abs( pBot->GetPlayer( )->mo->y - GoalPos.y ) >= Distance ))
		g_iReturnInt = PATH_COMPLETE;
	else
	{
		g_iReturnInt = PATH_REACHEDGOAL;
		pBot->m_ulPathType = BOTPATHTYPE_NONE;
	}

	// We don't need GoalPos anymore, so we can corrupt it! KEKE!
	Angle = Angle >> ANGLETOFINESHIFT;
	GoalPos.x = pBot->GetPlayer( )->mo->x + ( USERANGE >> FRACBITS ) * finecosine[Angle];
	GoalPos.y = pBot->GetPlayer( )->mo->y + ( USERANGE >> FRACBITS ) * finesine[Angle];

	ulFlags = BOTPATH_TryWalk( pBot->GetPlayer( )->mo, pBot->GetPlayer( )->mo->x, pBot->GetPlayer( )->mo->y, pBot->GetPlayer( )->mo->z, GoalPos.x, GoalPos.y );
	if ( ulFlags & BOTPATH_JUMPABLELEDGE )
		pBot->GetPlayer( )->cmd.ucmd.buttons |= BT_JUMP;
	if ( ulFlags & BOTPATH_DOOR )
	{
		sector_t	*pSector;
		DDoor		*pDoor;

		pSector = BOTPATH_GetDoorSector( );
		if ( pSector != NULL )
		{
			TThinkerIterator<DDoor>		Iterator;
			while (( pDoor = Iterator.Next( )))
			{
				if ( pSector == pDoor->GetSector( ))
				{
					// Door is already rising.
					if ( pDoor->GetDirection( ) == 1 )
						return;

					break;
				}
			}
		}

		// At this point, either the door has not been created yet, or is closing or at a height
		// that still blocks us. Therefore, try to open the door.
		pBot->GetPlayer( )->cmd.ucmd.buttons |= BT_USE;
		pBot->GetPlayer( )->oldbuttons &= ~BT_USE;
	}
}

//*****************************************************************************
//
static void botcmd_PathToLastHeardSound( CSkullBot *pBot )
{
	float	fSpeed;

	fSpeed = (float)pBot->m_ScriptData.alStack[pBot->m_ScriptData.lStackPosition - 1];
	pBot->PopStack( );

	if ( fSpeed < 0 )
		fSpeed = 0;
	if ( fSpeed > 100 )
		fSpeed = 100;

	g_iReturnInt = PATH_UNREACHABLE;
}

//*****************************************************************************
//
static void botcmd_Roam( CSkullBot *pBot )
{
	POS_t				GoalPos;
	angle_t				Angle;
	ULONG				ulFlags;
	float				fSpeed;
	ASTARRETURNSTRUCT_t	ReturnVal;
	fixed_t				Distance;

	fSpeed = (float)pBot->m_ScriptData.alStack[pBot->m_ScriptData.lStackPosition - 1];
	pBot->PopStack( );

	if ( fSpeed < 0 )
		fSpeed = 0;
	if ( fSpeed > 100 )
		fSpeed = 100;

	// If the bot doesn't have a roam goal, randomly select a place on the map for him to roam to.
	if ( pBot->m_ulPathType != BOTPATHTYPE_ROAM )
	{
		ASTAR_ClearPath( pBot->GetPlayer( ) - players );
		pBot->m_ulPathType = BOTPATHTYPE_ROAM;

		// Get rid of the goal actor.
		pBot->m_pGoalActor = NULL;

		// Select a random map location to roam to.
		ASTAR_SelectRandomMapLocation( &pBot->m_PathGoalPos, pBot->GetPlayer( )->mo->x, pBot->GetPlayer( )->mo->y );
	}

	ReturnVal = ASTAR_Path( pBot->GetPlayer( ) - players, pBot->m_PathGoalPos, botdebug_maxsearchnodes, static_cast<LONG> ( botdebug_maxroamgiveupnodes ) );
	if ( ReturnVal.ulFlags & PF_COMPLETE )
	{
		// If it wasn't possible to create a path to the goal, try again next tick.
		if (( ReturnVal.pNode == NULL ) || (( ReturnVal.ulFlags & PF_SUCCESS ) == false ))
		{
			// No need to clear this. It'll get cleared next time the bot tries to make a path.
//			ASTAR_ClearPath( pBot->GetPlayer( ) - players );
			pBot->m_ulPathType = BOTPATHTYPE_NONE;
			g_iReturnInt = PATH_INCOMPLETE;
			return;
		}

		// If we're in the goal node, we've reached our roaming destination. Time to choose
		// another one and roam there!
		if ( ReturnVal.bIsGoal )
		{
			// No need to clear this. It'll get cleared next time the bot tries to make a path.
//			ASTAR_ClearPath( pBot->GetPlayer( ) - players );
//			pBot->m_ulPathType = BOTPATHTYPE_NONE;

			GoalPos.x = pBot->m_PathGoalPos.x;
			GoalPos.y = pBot->m_PathGoalPos.y;
			GoalPos.z = 0;
		}
		else
			GoalPos = ASTAR_GetPosition( ReturnVal.pNode );
	}
	else
	{
		g_iReturnInt = PATH_INCOMPLETE;
		return;
	}

	Angle = R_PointToAngle2( pBot->GetPlayer( )->mo->x,
							  pBot->GetPlayer( )->mo->y, 
							  GoalPos.x,
							  GoalPos.y );

	pBot->GetPlayer( )->mo->angle = Angle;
	pBot->GetPlayer( )->cmd.ucmd.forwardmove = static_cast<short> ( ( 0x32 << 8 ) * ( fSpeed / 100.0f ) );

	Distance = pBot->GetPlayer( )->mo->radius;
	if (( abs( pBot->GetPlayer( )->mo->x - GoalPos.x ) < Distance ) && ( abs( pBot->GetPlayer( )->mo->y - GoalPos.y ) < Distance ))
		pBot->m_ulPathType = BOTPATHTYPE_NONE;

	// We don't need GoalPos anymore, so we can corrupt it! KEKE!
	Angle = Angle >> ANGLETOFINESHIFT;
	GoalPos.x = pBot->GetPlayer( )->mo->x + ( USERANGE >> FRACBITS ) * finecosine[Angle];
	GoalPos.y = pBot->GetPlayer( )->mo->y + ( USERANGE >> FRACBITS ) * finesine[Angle];

	ulFlags = BOTPATH_TryWalk( pBot->GetPlayer( )->mo, pBot->GetPlayer( )->mo->x, pBot->GetPlayer( )->mo->y, pBot->GetPlayer( )->mo->z, GoalPos.x, GoalPos.y );
	if ( ulFlags & BOTPATH_JUMPABLELEDGE )
		pBot->GetPlayer( )->cmd.ucmd.buttons |= BT_JUMP;
	if ( ulFlags & BOTPATH_DOOR )
	{
		sector_t	*pSector;
		DDoor		*pDoor;

		pSector = BOTPATH_GetDoorSector( );
		if ( pSector != NULL )
		{
			TThinkerIterator<DDoor>		Iterator;
			while (( pDoor = Iterator.Next( )))
			{
				if ( pSector == pDoor->GetSector( ))
				{
					// Door is already rising.
					if ( pDoor->GetDirection( ) == 1 )
					{
						g_iReturnInt = PATH_COMPLETE;
						return;
					}

					break;
				}
			}
		}

		// At this point, either the door has not been created yet, or is closing or at a height
		// that still blocks us. Therefore, try to open the door.
		pBot->GetPlayer( )->cmd.ucmd.buttons |= BT_USE;
		pBot->GetPlayer( )->oldbuttons &= ~BT_USE;
	}

	g_iReturnInt = PATH_COMPLETE;
}

//*****************************************************************************
//
static void botcmd_GetPathingCostToItem( CSkullBot *pBot )
{
	LONG				lItem;
	POS_t				GoalPos;
	ASTARRETURNSTRUCT_t	ReturnVal;

	lItem = pBot->m_ScriptData.alStack[pBot->m_ScriptData.lStackPosition - 1];
	pBot->PopStack( );

	if (( lItem < 0 ) || ( lItem >= IDList<AActor>::MAX_NETID ))
		I_Error( "botcmd_GetPathingCostToItem: Illegal item index, %d", static_cast<int> (lItem) );

	AActor *pActor = g_NetIDList.findPointerByID ( lItem );
	if ( pActor == NULL )
	{
		g_iReturnInt = -1;
		return;
	}

	GoalPos.x = pActor->x;
	GoalPos.y = pActor->y;

	ASTAR_ClearPath(( pBot->GetPlayer( ) - players ) + MAXPLAYERS );

	ReturnVal = ASTAR_Path(( pBot->GetPlayer( ) - players ) + MAXPLAYERS, GoalPos, 0, static_cast<LONG> ( botdebug_maxgiveupnodes ) );
	if ( ReturnVal.ulFlags & PF_COMPLETE )
	{
		// If it wasn't possible to create a path to the goal, try again next tick.
		if (( ReturnVal.pNode == NULL ) || (( ReturnVal.ulFlags & PF_SUCCESS ) == false ))
			g_iReturnInt = -1;
		else
			g_iReturnInt = ReturnVal.lTotalCost;
	}
	else
		g_iReturnInt = -1;
}

//*****************************************************************************
//
static void botcmd_GetDistanceToItem( CSkullBot *pBot )
{
	LONG	lItem;

	lItem = pBot->m_ScriptData.alStack[pBot->m_ScriptData.lStackPosition - 1];
	pBot->PopStack( );

	if (( lItem < 0 ) || ( lItem >= IDList<AActor>::MAX_NETID ))
		I_Error( "botcmd_GetDistanceToItem: Illegal item index, %d", static_cast<int> (lItem) );

	AActor *pActor = g_NetIDList.findPointerByID ( lItem );
	if ( pActor )
	{
		g_iReturnInt = abs( P_AproxDistance( pActor->x - pBot->GetPlayer( )->mo->x, 
											 pActor->y - pBot->GetPlayer( )->mo->y ) / FRACUNIT );
	}
	else
		g_iReturnInt = -1;
}

//*****************************************************************************
//
static void botcmd_GetItemName( CSkullBot *pBot )
{
	LONG	lItem;

	lItem = pBot->m_ScriptData.alStack[pBot->m_ScriptData.lStackPosition - 1];
	pBot->PopStack( );

	if (( lItem < 0 ) || ( lItem >= IDList<AActor>::MAX_NETID ))
		I_Error( "botcmd_GetItemName: Illegal item index, %d", static_cast<int> (lItem) );

	AActor *pActor = g_NetIDList.findPointerByID ( lItem );
	if ( pActor )
	{
		if ( strlen( pActor->GetClass( )->TypeName.GetChars( )) < BOTCMD_RETURNSTRING_SIZE )
			sprintf( g_szReturnString, "%s", pActor->GetClass( )->TypeName.GetChars( ));
		else
			g_szReturnString[0] = 0;
	}
	else
		g_szReturnString[0] = 0;
}

//*****************************************************************************
//
static void botcmd_IsItemVisible( CSkullBot *pBot )
{
	LONG		lIdx;

	lIdx = pBot->m_ScriptData.alStack[pBot->m_ScriptData.lStackPosition - 1];
	pBot->PopStack( );

	if (( lIdx < 0 ) || ( lIdx >= IDList<AActor>::MAX_NETID ))
		I_Error( "botcmd_IsItemVisible: Illegal item index, %d", static_cast<int> (lIdx) );

	AActor *pActor = g_NetIDList.findPointerByID ( lIdx );
	if ( pActor )
	{

		// Check if we have a line of sight to this object.
		if ( P_CheckSight( pBot->GetPlayer( )->mo, pActor, SF_SEEPASTBLOCKEVERYTHING ))
		{
			angle_t	Angle;

			Angle = R_PointToAngle2( pBot->GetPlayer( )->mo->x,
								  pBot->GetPlayer( )->mo->y, 
								  pActor->x,
								  pActor->y );

			Angle -= pBot->GetPlayer( )->mo->angle;

			// If the object within our view range, tell the bot.
			if (( Angle <= ANG45 ) || ( Angle >= ((ULONG)ANGLE_1 * 315 )))
			{
				g_bReturnBool = true;
				return;
			}
		}

		g_bReturnBool = false;
		return;
	}
	else
	{
		g_bReturnBool = false;
		return;
	}
}

//*****************************************************************************
//
static void botcmd_SetGoal( CSkullBot *pBot )
{
	LONG	lIdx;

	lIdx = pBot->m_ScriptData.alStack[pBot->m_ScriptData.lStackPosition - 1];
	pBot->PopStack( );

	if (( lIdx < 0 ) || ( lIdx >= IDList<AActor>::MAX_NETID ))
		I_Error( "botcmd_SetGoal: Illegal item index, %d", static_cast<int> (lIdx) );

	AActor *pActor = g_NetIDList.findPointerByID ( lIdx );
	if ( pActor )
	{
		pBot->m_pGoalActor = pActor;
		pBot->m_ulPathType = BOTPATHTYPE_NONE;
	}
	else
		Printf( "botcmd_SetGoal: WARNING! Tried to set goal to bad item ID, %d!\n", static_cast<int> (lIdx) );
}

//*****************************************************************************
//
static void botcmd_BeginAimingAtEnemy( CSkullBot *pBot )
{
	pBot->m_bAimAtEnemy = true;
}

//*****************************************************************************
//
static void botcmd_StopAimingAtEnemy( CSkullBot *pBot )
{
	pBot->m_bAimAtEnemy = false;
	pBot->m_AngleDelta = 0;
	pBot->m_AngleOffBy = 0;
	pBot->m_AngleDesired = 0;
}

//*****************************************************************************
//
static void botcmd_Turn( CSkullBot *pBot )
{
	LONG	lBuffer;

	lBuffer = pBot->m_ScriptData.alStack[pBot->m_ScriptData.lStackPosition - 1];
	pBot->PopStack( );

	// Adjust the bot's angle.
	if ( lBuffer < 0 )
		pBot->GetPlayer( )->mo->angle -= ANGLE_1 * labs( lBuffer );
	else
		pBot->GetPlayer( )->mo->angle += ANGLE_1 * lBuffer;
}

//*****************************************************************************
//
static void botcmd_GetCurrentAngle( CSkullBot *pBot )
{
	g_iReturnInt = ( pBot->GetPlayer( )->mo->angle / ANGLE_1 );
}

//*****************************************************************************
//
static void botcmd_SetEnemy( CSkullBot *pBot )
{
	LONG	lPlayer;

	lPlayer = pBot->m_ScriptData.alStack[pBot->m_ScriptData.lStackPosition - 1];
	pBot->PopStack( );

	if (( lPlayer < 0 ) || ( lPlayer >= MAXPLAYERS ))
		I_Error( "botcmd_SetEnemy: Illegal player index, %d", static_cast<int> (lPlayer) );

	pBot->m_ulPlayerEnemy = lPlayer;
}

//*****************************************************************************
//
static void botcmd_ClearEnemy( CSkullBot *pBot )
{
	pBot->m_ulPlayerEnemy = MAXPLAYERS;
}

//*****************************************************************************
//
static void botcmd_IsEnemyAlive( CSkullBot *pBot )
{
	if (( pBot->m_ulPlayerEnemy != MAXPLAYERS ) && ( players[pBot->m_ulPlayerEnemy].health > 0 ))
		g_bReturnBool = true;
	else
		g_bReturnBool = false;
}

//*****************************************************************************
//
static void botcmd_IsEnemyVisible( CSkullBot *pBot )
{
	if (( pBot->m_ulPlayerEnemy != MAXPLAYERS ) && ( players[pBot->m_ulPlayerEnemy].mo ))
	{
		g_bReturnBool = ( P_CheckSight( pBot->GetPlayer( )->mo, players[pBot->m_ulPlayerEnemy].mo, SF_SEEPASTBLOCKEVERYTHING ));
	}
	else
		g_bReturnBool = false;
}

//*****************************************************************************
//
static void botcmd_GetDistanceToEnemy( CSkullBot *pBot )
{
	if (( pBot->m_ulPlayerEnemy != MAXPLAYERS ) && ( players[pBot->m_ulPlayerEnemy].mo != nullptr ))
	{
		POS_t	EnemyPos;

		EnemyPos = pBot->GetEnemyPosition( );
		g_iReturnInt = P_AproxDistance( pBot->GetPlayer( )->mo->x - EnemyPos.x, 
										pBot->GetPlayer( )->mo->y - EnemyPos.y ) / FRACUNIT;
	}
	else
		g_iReturnInt = -1;
}

//*****************************************************************************
//
static void botcmd_GetPlayerDamagedBy( CSkullBot *pBot )
{
	g_iReturnInt = pBot->m_ulLastPlayerDamagedBy;
}

//*****************************************************************************
//
static void botcmd_GetEnemyInvulnerabilityTicks( CSkullBot *pBot )
{
	APowerInvulnerable		*pInvulnerability;

	if ( PLAYER_IsValidPlayer ( pBot->m_ulPlayerEnemy ) == false )
		g_iReturnInt = -1;
	else
	{
		pInvulnerability = players[pBot->m_ulPlayerEnemy].mo->FindInventory<APowerInvulnerable>( );
		if ( pInvulnerability )
			g_iReturnInt = pInvulnerability->EffectTics;
		else
			g_iReturnInt = 0;
	}
}

//*****************************************************************************
//
static void botcmd_FireWeapon( CSkullBot *pBot )
{
	pBot->GetPlayer( )->cmd.ucmd.buttons |= BT_ATTACK;
}

//*****************************************************************************
//
static void botcmd_BeginFiringWeapon( CSkullBot *pBot )
{
	pBot->m_lButtons |= BT_ATTACK;
}

//*****************************************************************************
//
static void botcmd_StopFiringWeapon( CSkullBot *pBot )
{
	pBot->m_lButtons &= ~BT_ATTACK;
}

//*****************************************************************************
//
static void botcmd_GetCurrentWeapon( CSkullBot *pBot )
{
	if (( pBot->GetPlayer( )->PendingWeapon == WP_NOCHANGE ) ||
		( pBot->GetPlayer( )->PendingWeapon == NULL ))
	{
		if ( pBot->GetPlayer( )->ReadyWeapon )
		{
			if ( strlen( pBot->GetPlayer( )->ReadyWeapon->GetClass( )->TypeName.GetChars( )) < BOTCMD_RETURNSTRING_SIZE )
				sprintf( g_szReturnString, "%s", pBot->GetPlayer( )->ReadyWeapon->GetClass( )->TypeName.GetChars( ));
			else
				g_szReturnString[0] = 0;
		}
		else
			g_szReturnString[0] = 0;
	}
	else
	{
		if ( strlen( pBot->GetPlayer( )->PendingWeapon->GetClass( )->TypeName.GetChars( )) < BOTCMD_RETURNSTRING_SIZE )
			sprintf( g_szReturnString, "%s", pBot->GetPlayer( )->PendingWeapon->GetClass( )->TypeName.GetChars( ));
		else
			g_szReturnString[0] = 0;
	}
}

//*****************************************************************************
//
static void botcmd_ChangeWeapon( CSkullBot *pBot )
{
	const PClass	*pType;
	AWeapon			*pWeapon;
	char			szWeapon[1024];

	sprintf( szWeapon, "%s", pBot->m_ScriptData.aszStringStack[pBot->m_ScriptData.lStringStackPosition - 1] );
	pBot->PopStringStack( );

	pType = PClass::FindClass( szWeapon );

	// Invalid class.
	if ( pType == NULL )
		return;

	// Not a weapon.
	if ( pType->IsDescendantOf( RUNTIME_CLASS( AWeapon )) == false )
		return;

	pWeapon = static_cast<AWeapon *>( pBot->GetPlayer( )->mo->FindInventory( pType ));

	// The bot doesn't own the weapon.
	if ( pWeapon == NULL )
		return;

	if ( pBot->GetPlayer( )->ReadyWeapon != pWeapon )
	{
		pBot->GetPlayer( )->PendingWeapon = pWeapon;

		// [BB] Inform the clients about the weapon change.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetPlayerPendingWeapon( static_cast<ULONG> ( pBot->GetPlayer( ) - players ) );
	}
}

//*****************************************************************************
//
static void botcmd_GetWeaponFromItem( CSkullBot *pBot )
{
	LONG	lItem;

	lItem = pBot->m_ScriptData.alStack[pBot->m_ScriptData.lStackPosition - 1];
	pBot->PopStack( );

	if (( lItem < 0 ) || ( lItem >= IDList<AActor>::MAX_NETID ))
		I_Error( "botcmd_GetWeaponFromItem: Illegal item index, %d", static_cast<int> (lItem) );

	AActor *pActor = g_NetIDList.findPointerByID ( lItem );
	if ( pActor )
	{
		if ( pActor->GetClass( )->IsDescendantOf( RUNTIME_CLASS( AWeapon )) == false )
			g_szReturnString[0] = 0;
		else
		{
			if ( strlen( pActor->GetClass( )->TypeName.GetChars( )) < BOTCMD_RETURNSTRING_SIZE )
				sprintf( g_szReturnString, "%s", pActor->GetClass( )->TypeName.GetChars( ));
			else
				g_szReturnString[0] = 0;
		}
	}
	else
		g_szReturnString[0] = 0;
}

//*****************************************************************************
//
static void botcmd_IsWeaponOwned( CSkullBot *pBot )
{
	LONG	lItem;

	lItem = pBot->m_ScriptData.alStack[pBot->m_ScriptData.lStackPosition - 1];
	pBot->PopStack( );

	if (( lItem < 0 ) || ( lItem >= IDList<AActor>::MAX_NETID ))
		I_Error( "botcmd_IsWeaponOwned: Illegal item index, %d", static_cast<int> (lItem) );

	AActor *pActor = g_NetIDList.findPointerByID ( lItem );
	if ( pActor )
	{
		if ( pActor->GetClass( )->IsDescendantOf( RUNTIME_CLASS( AWeapon )) == false )
			g_bReturnBool = false;
		else
			g_bReturnBool = ( pBot->GetPlayer( )->mo->FindInventory( pActor->GetClass( )) != NULL );
	}
	else
		g_bReturnBool = false;
}

//*****************************************************************************
//
static void botcmd_IsFavoriteWeapon( CSkullBot *pBot )
{
	char	szWeapon[1024];

	sprintf( szWeapon, "%s", pBot->m_ScriptData.aszStringStack[pBot->m_ScriptData.lStringStackPosition - 1] );
	pBot->PopStringStack( );

	g_bReturnBool = ( stricmp( szWeapon, BOTINFO_GetFavoriteWeapon( pBot->m_ulBotInfoIdx )) == 0 );
}

//*****************************************************************************
//
static void botcmd_Say( CSkullBot *pBot )
{
	char	szInString[1024];
	char	szOutString[1024];

	sprintf( szInString, "%s", pBot->m_ScriptData.aszStringStack[pBot->m_ScriptData.lStringStackPosition - 1] );
	pBot->PopStringStack( );

	if ( bot_allowchat )
	{
		// Perform any chat string substitutions that need to be done.
		BOTCMD_DoChatStringSubstitutions( pBot, szInString, szOutString );

		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVER_SendChatMessage( pBot->GetPlayer( ) - players, CHATMODE_GLOBAL, szOutString );
		else
			CHAT_PrintChatString( pBot->GetPlayer( ) - players, CHATMODE_GLOBAL, szOutString );
	}

	// We can now get rid of the chat bubble above the bot's head.
	pBot->GetPlayer( )->bChatting = false;
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetPlayerChatStatus( pBot->GetPlayer( ) - players );
}

//*****************************************************************************
//
static void botcmd_SayFromFile( CSkullBot *pBot )
{
	char		szFilename[1024];
	char		szSection[1024];
	char		szInString[1024];
	char		szOutString[1024];
	CChatFile	*pFile;

	// We can now get rid of the chat bubble above the bot's head.
	pBot->GetPlayer( )->bChatting = false;
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetPlayerChatStatus( pBot->GetPlayer( ) - players );

	sprintf( szSection, "%s", pBot->m_ScriptData.aszStringStack[pBot->m_ScriptData.lStringStackPosition - 1] );
	pBot->PopStringStack( );

	sprintf( szFilename, "%s", pBot->m_ScriptData.aszStringStack[pBot->m_ScriptData.lStringStackPosition - 1] );
	pBot->PopStringStack( );

	pFile = new CChatFile;
	if ( pFile->LoadChatFile( szFilename ) == false )
	{
		// Temporarily disable the use of color codes.
		CONSOLE_SetAllowColorCodes( false );

		Printf( "botcmd_SayFromFile: Couldn't open file %s!\n", szFilename );

		// Re-enable the use of color codes.
		CONSOLE_SetAllowColorCodes( true );

		// Free the file before leaving.
		delete( pFile );
		return;
	}

	sprintf( szInString, "%s", pFile->ChooseRandomEntry( szSection ));
	if ( stricmp( szInString, "NULL" ) == 0 )
	{
		// Temporarily disable the use of color codes.
		CONSOLE_SetAllowColorCodes( false );

		Printf( "botcmd_SayFromFile: Couldn't find section %s in file %s!\n", szSection, szFilename );

		// Re-enable the use of color codes.
		CONSOLE_SetAllowColorCodes( true );

		// Free the file before leaving.
		delete( pFile );
		return;
	}

	if ( bot_allowchat )
	{
		// Perform any chat string substitutions that need to be done.
		BOTCMD_DoChatStringSubstitutions( pBot, szInString, szOutString );

		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVER_SendChatMessage( pBot->GetPlayer( ) - players, CHATMODE_GLOBAL, szOutString );
		else
			CHAT_PrintChatString( pBot->GetPlayer( ) - players, CHATMODE_GLOBAL, szOutString );
	}

	// Free the file before leaving.
	delete( pFile );
}

//*****************************************************************************
//
static void botcmd_SayFromChatFile( CSkullBot *pBot )
{
	char		szFilename[1024];
	char		szSection[1024];
	char		szInString[1024];
	char		szOutString[1024];
	CChatFile	*pFile;

	// We can now get rid of the chat bubble above the bot's head.
	pBot->GetPlayer( )->bChatting = false;
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetPlayerChatStatus( pBot->GetPlayer( ) - players );

	sprintf( szSection, "%s", pBot->m_ScriptData.aszStringStack[pBot->m_ScriptData.lStringStackPosition - 1] );
	pBot->PopStringStack( );

	sprintf( szFilename, "%s", BOTINFO_GetChatFile( pBot->m_ulBotInfoIdx ));

	pFile = new CChatFile;
	if ( pFile->LoadChatFile( szFilename ) == false )
	{
		// Temporarily disable the use of color codes.
		CONSOLE_SetAllowColorCodes( false );

		Printf( "botcmd_SayFromFile: Couldn't open file %s!\n", szFilename );

		// Re-enable the use of color codes.
		CONSOLE_SetAllowColorCodes( true );

		// Free the file before leaving.
		delete( pFile );
		return;
	}

	sprintf( szInString, "%s", pFile->ChooseRandomEntry( szSection ));
	if ( stricmp( szInString, "NULL" ) == 0 )
	{
		// Temporarily disable the use of color codes.
		CONSOLE_SetAllowColorCodes( false );

		Printf( "botcmd_SayFromChatFile: Couldn't find section %s in file %s!\n", szSection, szFilename );

		// Re-enable the use of color codes.
		CONSOLE_SetAllowColorCodes( true );

		// Free the file before leaving.
		delete( pFile );
		return;
	}

	if ( bot_allowchat )
	{
		// Perform any chat string substitutions that need to be done.
		BOTCMD_DoChatStringSubstitutions( pBot, szInString, szOutString );

		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVER_SendChatMessage( pBot->GetPlayer( ) - players, CHATMODE_GLOBAL, szOutString );
		else
			CHAT_PrintChatString( pBot->GetPlayer( ) - players, CHATMODE_GLOBAL, szOutString );
	}

	// Free the file before leaving.
	delete( pFile );
}

//*****************************************************************************
//
static void botcmd_BeginChatting( CSkullBot *pBot )
{
	pBot->GetPlayer( )->bChatting = true;
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetPlayerChatStatus( pBot->GetPlayer( ) - players );
}

//*****************************************************************************
//
static void botcmd_StopChatting( CSkullBot *pBot )
{
	pBot->GetPlayer( )->bChatting = false;
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetPlayerChatStatus( pBot->GetPlayer( ) - players );
}

//*****************************************************************************
//
static void botcmd_ChatSectionExists( CSkullBot *pBot )
{
	char		szFilename[1024];
	char		szSection[1024];
	CChatFile	*pFile;

	// Read in the string index of our first argument.
	sprintf( szSection, "%s", pBot->m_ScriptData.aszStringStack[pBot->m_ScriptData.lStringStackPosition - 1] );
	pBot->PopStringStack( );

	// Set the filename.
	sprintf( szFilename, "%s", BOTINFO_GetChatFile( pBot->m_ulBotInfoIdx ));

	pFile = new CChatFile;
	if ( pFile->LoadChatFile( szFilename ) == false )
	{
		// It's not there!
		g_bReturnBool = false;

		// Free the file before leaving.
		delete( pFile );
		return;
	}

	if ( pFile->FindSection( szSection ) != -1 )
		g_bReturnBool = true;
	else
		g_bReturnBool = false;

	// Free the file before leaving.
	delete( pFile );
}

//*****************************************************************************
//
static void botcmd_ChatSectionExistsInFile( CSkullBot *pBot )
{
	char		szFilename[1024];
	char		szSection[1024];
	CChatFile	*pFile;

	// Read in the string index of our first argument.
	sprintf( szSection, "%s", pBot->m_ScriptData.aszStringStack[pBot->m_ScriptData.lStringStackPosition - 1] );
	pBot->PopStringStack( );

	// Read in the string index of our second argument.
	sprintf( szFilename, "%s", pBot->m_ScriptData.aszStringStack[pBot->m_ScriptData.lStringStackPosition - 1] );
	pBot->PopStringStack( );

	pFile = new CChatFile;
	if ( pFile->LoadChatFile( szFilename ) == false )
	{
		// It's not there!
		g_bReturnBool = false;

		// Free the file before leaving.
		delete( pFile );
		return;
	}

	if ( pFile->FindSection( szSection ) != -1 )
		g_bReturnBool = true;
	else
		g_bReturnBool = false;

	// Free the file before leaving.
	delete( pFile );
}

//*****************************************************************************
//
static void botcmd_GetLastChatString( CSkullBot *pBot )
{
	if ( strlen( g_szLastChatString ) < BOTCMD_RETURNSTRING_SIZE )
		sprintf( g_szReturnString, "%s", g_szLastChatString );
	else
		g_szReturnString[0] = 0;
}

//*****************************************************************************
//
static void botcmd_GetLastChatPlayer( CSkullBot *pBot )
{
	if ( strlen( g_szLastChatPlayer ) < BOTCMD_RETURNSTRING_SIZE )
		sprintf( g_szReturnString, "%s", g_szLastChatPlayer );
	else
		g_szReturnString[0] = 0;
}

//*****************************************************************************
//
static void botcmd_GetChatFrequency( CSkullBot *pBot )
{
	g_iReturnInt = BOTINFO_GetChatFrequency( pBot->m_ulBotInfoIdx );
}

//*****************************************************************************
//
static void botcmd_Jump( CSkullBot *pBot )
{
	pBot->GetPlayer( )->cmd.ucmd.buttons |= BT_JUMP;
}

//*****************************************************************************
//
static void botcmd_BeginJumping( CSkullBot *pBot )
{
	pBot->m_lButtons |= BT_JUMP;
}

//*****************************************************************************
//
static void botcmd_StopJumping( CSkullBot *pBot )
{
	pBot->m_lButtons &= ~BT_JUMP;
}

//*****************************************************************************
//
static void botcmd_Taunt( CSkullBot *pBot )
{
	if ( PLAYER_Taunt( pBot->GetPlayer( )))
	{
		// If we're the server, tell clients.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_PlayerTaunt( pBot->GetPlayer( ) - players );
	}
}

//*****************************************************************************
//
static void botcmd_Respawn( CSkullBot *pBot )
{
	pBot->GetPlayer( )->cmd.ucmd.buttons |= BT_USE;
}

//*****************************************************************************
//
static void botcmd_TryToJoinGame( CSkullBot *pBot )
{
	// Can't rejoin game if he's not spectating!
	if ( pBot->GetPlayer( )->bSpectating == false )
		return;

	// Player can't rejoin their LMS/survival game if they are dead.
	if (( GAMEMODE_GetCurrentFlags() & GMF_DEADSPECTATORS ) && ( pBot->GetPlayer( )->bDeadSpectator ))
		return;

	// [BB] If players aren't allowed to join at the moment, just put the bot in line.
	if ( GAMEMODE_PreventPlayersFromJoining() )
	{
		// [BB] Don't chose the team before the bot actually joins.
		JOINQUEUE_AddPlayer( pBot->GetPlayer( ) - players, teams.Size() );
		return;
	}

	// Everything's okay! Go ahead and join!
	PLAYER_SpectatorJoinsGame ( pBot->GetPlayer( ) );

	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
		PLAYER_SetTeam( pBot->GetPlayer( ), TEAM_ChooseBestTeamForPlayer( ), true );

	Printf( "%s \\c-joined the game.\n", pBot->GetPlayer( )->userinfo.GetName() );
}

//*****************************************************************************
//
static void botcmd_IsDead( CSkullBot *pBot )
{
	g_bReturnBool = ( pBot->GetPlayer( )->health <= 0 );
}

//*****************************************************************************
//
static void botcmd_IsSpectating( CSkullBot *pBot )
{
	g_bReturnBool = PLAYER_IsTrueSpectator( pBot->GetPlayer( ));
}

//*****************************************************************************
//
static void botcmd_GetHealth( CSkullBot *pBot )
{
	g_iReturnInt = pBot->GetPlayer( )->health;
}

//*****************************************************************************
//
static void botcmd_GetArmor( CSkullBot *pBot )
{
	ABasicArmor	*pArmor;

	pArmor = pBot->GetPlayer( )->mo->FindInventory<ABasicArmor>( );
	if ( pArmor )
		g_iReturnInt = pArmor->Amount;
	else
		g_iReturnInt = 0;
}

//*****************************************************************************
//
static void botcmd_GetBaseHealth( CSkullBot *pBot )
{
	g_iReturnInt = deh.StartHealth + pBot->GetPlayer( )->lMaxHealthBonus;
}

//*****************************************************************************
//
static void botcmd_GetBaseArmor( CSkullBot *pBot )
{
	ABasicArmor *pArmor = NULL;
	if ( pBot->GetPlayer()->mo )
		pArmor = pBot->GetPlayer()->mo->FindInventory<ABasicArmor>( );
	g_iReturnInt = ( deh.GreenAC * 100 + (pArmor ? pArmor->BonusCount : 0));
}

//*****************************************************************************
//
static void botcmd_GetBotskill( CSkullBot *pBot )
{
	g_iReturnInt = botskill;
}

//*****************************************************************************
//
static void botcmd_GetAccuracy( CSkullBot *pBot )
{
	g_iReturnInt = (int)BOTS_AdjustSkill( pBot, BOTINFO_GetAccuracy( pBot->m_ulBotInfoIdx ));
}

//*****************************************************************************
//
static void botcmd_GetIntellect( CSkullBot *pBot )
{
	g_iReturnInt = (int)BOTS_AdjustSkill( pBot, BOTINFO_GetIntellect( pBot->m_ulBotInfoIdx ));
}

//*****************************************************************************
//
static void botcmd_GetEvade( CSkullBot *pBot )
{
	g_iReturnInt = (int)BOTS_AdjustSkill( pBot, BOTINFO_GetEvade( pBot->m_ulBotInfoIdx ));
}

//*****************************************************************************
//
static void botcmd_GetAnticipation( CSkullBot *pBot )
{
	g_iReturnInt = (int)BOTS_AdjustSkill( pBot, BOTINFO_GetAnticipation( pBot->m_ulBotInfoIdx ));
}

//*****************************************************************************
//
static void botcmd_GetReactionTime( CSkullBot *pBot )
{
	g_iReturnInt = (int)BOTS_AdjustSkill( pBot, BOTINFO_GetReactionTime( pBot->m_ulBotInfoIdx ));
}

//*****************************************************************************
//
static void botcmd_GetPerception( CSkullBot *pBot )
{
	g_iReturnInt = (int)BOTS_AdjustSkill( pBot, BOTINFO_GetPerception( pBot->m_ulBotInfoIdx ));
}

//*****************************************************************************
//
static void botcmd_SetSkillIncrease( CSkullBot *pBot )
{
	LONG	lSkillIncrease;

	lSkillIncrease = pBot->m_ScriptData.alStack[pBot->m_ScriptData.lStackPosition - 1];
	pBot->PopStack( );

	pBot->m_bSkillIncrease = !!lSkillIncrease;
}

//*****************************************************************************
//
static void botcmd_IsSkillIncreased( CSkullBot *pBot )
{
	g_bReturnBool = pBot->m_bSkillIncrease;
}

//*****************************************************************************
//
static void botcmd_SetSkillDecrease( CSkullBot *pBot )
{
	LONG	lSkillDecrease;

	lSkillDecrease = pBot->m_ScriptData.alStack[pBot->m_ScriptData.lStackPosition - 1];
	pBot->PopStack( );

	pBot->m_bSkillDecrease = !!lSkillDecrease;
}

//*****************************************************************************
//
static void botcmd_IsSkillDecreased( CSkullBot *pBot )
{
	g_bReturnBool = pBot->m_bSkillDecrease;
}

//*****************************************************************************
//
static void botcmd_GetGameMode( CSkullBot *pBot )
{
	g_iReturnInt = GAMEMODE_GetCurrentMode( );
}

//*****************************************************************************
//
static void botcmd_GetSpread( CSkullBot *pBot )
{
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
	{
		if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNPOINTS )
			g_iReturnInt = TEAM_GetScoreCountSpread( pBot->GetPlayer( )->ulTeam );
		else if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNFRAGS )
			g_iReturnInt = TEAM_GetFragCountSpread( pBot->GetPlayer( )->ulTeam );
		else if ( teamlms )
			g_iReturnInt = TEAM_GetWinCountSpread( pBot->GetPlayer( )->ulTeam );
	}
	else
		g_iReturnInt = SCOREBOARD_CalcSpread( pBot->GetPlayer( ) - players );
}

//*****************************************************************************
//
static void botcmd_GetLastJoinedPlayer( CSkullBot *pBot )
{
	if ( strlen( g_szLastJoinedPlayer ) < BOTCMD_RETURNSTRING_SIZE )
		sprintf( g_szReturnString, "%s", g_szLastJoinedPlayer );
	else
		g_szReturnString[0] = 0;
}

//*****************************************************************************
//
static void botcmd_GetPlayerName( CSkullBot *pBot )
{
	LONG	lPlayer;

	lPlayer = pBot->m_ScriptData.alStack[pBot->m_ScriptData.lStackPosition - 1];
	pBot->PopStack( );

	if (( lPlayer < 0 ) || ( lPlayer >= MAXPLAYERS ))
		I_Error( "botcmd_GetPlayerName: Invalid player index, %d", static_cast<int> (lPlayer) );

	if ( strlen( players[lPlayer].userinfo.GetName() ) < BOTCMD_RETURNSTRING_SIZE )
		sprintf( g_szReturnString, "%s", players[lPlayer].userinfo.GetName() );
	else
		g_szReturnString[0] = 0;
}

//*****************************************************************************
//
static void botcmd_GetReceivedMedal( CSkullBot *pBot )
{
	if ( MEDAL_GetDisplayedMedal( pBot->GetPlayer( ) - players ) != NUM_MEDALS )
		g_iReturnInt = pBot->m_ulLastMedalReceived;
	else
		g_iReturnInt = NUM_MEDALS;
}

//*****************************************************************************
//
static void botcmd_ACS_Execute( CSkullBot *pBot )
{
	LONG			lScript;
	LONG			lMap;
	int				lArg1;
	int				lArg2;
	int				lArg3;
	level_info_t	*pLevelInfo;

	lArg3 = pBot->m_ScriptData.alStack[pBot->m_ScriptData.lStackPosition - 1];
	pBot->PopStack( );

	lArg2 = pBot->m_ScriptData.alStack[pBot->m_ScriptData.lStackPosition - 1];
	pBot->PopStack( );
	
	lArg1 = pBot->m_ScriptData.alStack[pBot->m_ScriptData.lStackPosition - 1];
	pBot->PopStack( );
	
	lMap = pBot->m_ScriptData.alStack[pBot->m_ScriptData.lStackPosition - 1];
	pBot->PopStack( );
	
	lScript = pBot->m_ScriptData.alStack[pBot->m_ScriptData.lStackPosition - 1];
	pBot->PopStack( );

	int arg[3] = { lArg1, lArg2, lArg3 };
	if (( lMap == 0 ) || (( pLevelInfo = FindLevelByNum( lMap )) == NULL ))
		P_StartScript( pBot->GetPlayer( )->mo, NULL, lScript, level.mapname, arg, 3, 0 );
	else
		P_StartScript( pBot->GetPlayer( )->mo, NULL, lScript, pLevelInfo->mapname, arg, 3, 0 );
}

//*****************************************************************************
//
static void botcmd_GetFavoriteWeapon( CSkullBot *pBot )
{
	if ( strlen( BOTINFO_GetFavoriteWeapon( pBot->m_ulBotInfoIdx )) < BOTCMD_RETURNSTRING_SIZE )
		sprintf( g_szReturnString, "%s", BOTINFO_GetFavoriteWeapon( pBot->m_ulBotInfoIdx ));
	else
		g_szReturnString[0] = 0;
}

//*****************************************************************************
//
static void botcmd_SayFromLump( CSkullBot *pBot )
{
	char		szLumpname[1024];
	char		szSection[1024];
	char		szInString[1024];
	char		szOutString[1024];
	CChatFile	*pFile;

	// We can now get rid of the chat bubble above the bot's head.
	pBot->GetPlayer( )->bChatting = false;
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetPlayerChatStatus( pBot->GetPlayer( ) - players );

	sprintf( szSection, "%s", pBot->m_ScriptData.aszStringStack[pBot->m_ScriptData.lStringStackPosition - 1] );
	pBot->PopStringStack( );

	sprintf( szLumpname, "%s", pBot->m_ScriptData.aszStringStack[pBot->m_ScriptData.lStringStackPosition - 1] );
	pBot->PopStringStack( );

	pFile = new CChatFile;
	if ( pFile->LoadChatLump( szLumpname ) == false )
	{
		// Temporarily disable the use of color codes.
		CONSOLE_SetAllowColorCodes( false );

		Printf( "botcmd_SayFromLump: Couldn't open lump %s!\n", szLumpname );

		// Re-enable the use of color codes.
		CONSOLE_SetAllowColorCodes( true );

		// Free the file before leaving.
		delete( pFile );
		return;
	}

	sprintf( szInString, "%s", pFile->ChooseRandomEntry( szSection ));
	if ( stricmp( szInString, "NULL" ) == 0 )
	{
		// Temporarily disable the use of color codes.
		CONSOLE_SetAllowColorCodes( false );

		Printf( "botcmd_SayFromLump: Couldn't find section %s in lump %s!\n", szSection, szLumpname );

		// Re-enable the use of color codes.
		CONSOLE_SetAllowColorCodes( true );

		// Free the file before leaving.
		delete( pFile );
		return;
	}

	if ( bot_allowchat )
	{
		// Perform any chat string substitutions that need to be done.
		BOTCMD_DoChatStringSubstitutions( pBot, szInString, szOutString );

		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVER_SendChatMessage( pBot->GetPlayer( ) - players, CHATMODE_GLOBAL, szOutString );
		else
			CHAT_PrintChatString( pBot->GetPlayer( ) - players, CHATMODE_GLOBAL, szOutString );
	}

	// Free the file before leaving.
	delete( pFile );
}

//*****************************************************************************
//
static void botcmd_SayFromChatLump( CSkullBot *pBot )
{
	char		szLumpname[1024];
	char		szSection[1024];
	char		szInString[1024];
	char		szOutString[1024];
	CChatFile	*pFile;

	// We can now get rid of the chat bubble above the bot's head.
	pBot->GetPlayer( )->bChatting = false;
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetPlayerChatStatus( pBot->GetPlayer( ) - players );

	sprintf( szSection, "%s", pBot->m_ScriptData.aszStringStack[pBot->m_ScriptData.lStringStackPosition - 1] );
	pBot->PopStringStack( );

	sprintf( szLumpname, "%s", BOTINFO_GetChatLump( pBot->m_ulBotInfoIdx ));

	pFile = new CChatFile;
	if ( pFile->LoadChatLump( szLumpname ) == false )
	{
		// Temporarily disable the use of color codes.
		CONSOLE_SetAllowColorCodes( false );

		Printf( "botcmd_SayFromChatLump: Couldn't open lump %s!\n", szLumpname );

		// Re-enable the use of color codes.
		CONSOLE_SetAllowColorCodes( true );

		// Free the file before leaving.
		delete pFile;
		return;
	}

	sprintf( szInString, "%s", pFile->ChooseRandomEntry( szSection ));
	if ( stricmp( szInString, "NULL" ) == 0 )
	{
		// Temporarily disable the use of color codes.
		CONSOLE_SetAllowColorCodes( false );

		Printf( "botcmd_SayFromChatLump: Couldn't find section %s in lump %s!\n", szSection, szLumpname );

		// Re-enable the use of color codes.
		CONSOLE_SetAllowColorCodes( true );

		// Free the file before leaving.
		delete pFile;
		return;
	}

	if ( bot_allowchat )
	{
		// Perform any chat string substitutions that need to be done.
		BOTCMD_DoChatStringSubstitutions( pBot, szInString, szOutString );

		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVER_SendChatMessage( pBot->GetPlayer( ) - players, CHATMODE_GLOBAL, szOutString );
		else
			CHAT_PrintChatString( pBot->GetPlayer( ) - players, CHATMODE_GLOBAL, szOutString );
	}

	// Free the file before leaving.
	delete pFile;
}

//*****************************************************************************
//
static void botcmd_ChatSectionExistsInLump( CSkullBot *pBot )
{
	char		szLumpname[1024];
	char		szSection[1024];
	CChatFile	*pFile;

	// Read in the string index of our first argument.
	sprintf( szSection, "%s", pBot->m_ScriptData.aszStringStack[pBot->m_ScriptData.lStringStackPosition - 1] );
	pBot->PopStringStack( );

	// Read in the string index of our second argument.
	sprintf( szLumpname, "%s", pBot->m_ScriptData.aszStringStack[pBot->m_ScriptData.lStringStackPosition - 1] );
	pBot->PopStringStack( );

	pFile = new CChatFile;
	if ( pFile->LoadChatLump( szLumpname ) == false )
	{
		// It's not there!
		g_bReturnBool = false;

		// Free the file before leaving.
		delete( pFile );
		return;
	}

	if ( pFile->FindSection( szSection ) != -1 )
		g_bReturnBool = true;
	else
		g_bReturnBool = false;

	// Free the file before leaving.
	delete( pFile );
}

//*****************************************************************************
//
static void botcmd_ChatSectionExistsInChatLump( CSkullBot *pBot )
{
	char		szLumpname[1024];
	char		szSection[1024];
	CChatFile	*pFile;

	// Read in the string index of our first argument.
	sprintf( szSection, "%s", pBot->m_ScriptData.aszStringStack[pBot->m_ScriptData.lStringStackPosition - 1] );
	pBot->PopStringStack( );

	// Set the lumpname.
	sprintf( szLumpname, "%s", BOTINFO_GetChatLump( pBot->m_ulBotInfoIdx ));

	pFile = new CChatFile;
	if ( pFile->LoadChatLump( szLumpname ) == false )
	{
		// It's not there!
		g_bReturnBool = false;

		// Free the file before leaving.
		delete pFile;
		return;
	}

	if ( pFile->FindSection( szSection ) != -1 )
		g_bReturnBool = true;
	else
		g_bReturnBool = false;

	// Free the file before leaving.
	delete pFile;
}
