//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2007 Brad Carney
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
// Date created:  7/12/07
//
//
// Filename: gamemode.cpp
//
// Description: 
//
//-----------------------------------------------------------------------------

#include "cooperative.h"
#include "deathmatch.h"
#include "domination.h"
#include "doomstat.h"
#include "d_event.h"
#include "gamemode.h"
#include "team.h"
#include "network.h"
#include "sv_commands.h"
#include "g_game.h"
#include "joinqueue.h"
#include "cl_demo.h"
#include "survival.h"
#include "duel.h"
#include "invasion.h"
#include "lastmanstanding.h"
#include "possession.h"
#include "p_lnspec.h"
#include "p_acs.h"
#include "c_dispatch.h"
#include "cl_commands.h"
#include "c_bind.h"	// [geNia] To tell user what key to press to ready.
// [BB] The next includes are only needed for GAMEMODE_DisplayStandardMessage
#include "sbar.h"
#include "v_video.h"
#include "i_system.h"

//*****************************************************************************
//	CONSOLE VARIABLES

// [AK] Added CVAR_GAMEMODESETTING.
CVAR( Bool, instagib, false, CVAR_SERVERINFO | CVAR_LATCH | CVAR_CAMPAIGNLOCK | CVAR_GAMEMODESETTING );
CVAR( Bool, buckshot, false, CVAR_SERVERINFO | CVAR_LATCH | CVAR_CAMPAIGNLOCK | CVAR_GAMEMODESETTING );

CVAR( Bool, sv_suddendeath, true, CVAR_SERVERINFO | CVAR_LATCH | CVAR_GAMEMODESETTING );

EXTERN_CVAR( Bool, sv_respawnonnewwave );

//*****************************************************************************
//	VARIABLES

// Data for all our game modes.
static	GAMEMODE_s				g_GameModes[NUM_GAMEMODES];

// Our current game mode.
static	GAMEMODE_e				g_CurrentGameMode;

// [BB] Implement the string table and the conversion functions for the GMF and GAMEMODE enums.
#define GENERATE_ENUM_STRINGS  // Start string generation
#include "gamemode_enums.h"
#undef GENERATE_ENUM_STRINGS   // Stop string generation

static	bool		g_bReadyPlayers[MAXPLAYERS];

//*****************************************************************************
//	FUNCTIONS

GAMESETTING_s::~GAMESETTING_s ( void )
{
	DeleteStrings( );
}

//*****************************************************************************
//
void GAMESETTING_s::DeleteStrings ( bool bDeleteDefault )
{
	// [AK] Nothing to do if this is not a string CVar.
	if ( Type != CVAR_String )
		return;

	if ( Val.String != NULL )
	{
		delete[] Val.String;
		Val.String = NULL;
	}

	if (( bDeleteDefault ) && ( DefaultVal.String != NULL ))
	{
		delete[] DefaultVal.String;
		DefaultVal.String = NULL;
	}
}

//*****************************************************************************
//
void GAMEMODE_Tick( void )
{
	static GAMESTATE_e oldState = GAMESTATE_UNSPECIFIED;
	const GAMESTATE_e state = GAMEMODE_GetState();

	if ( GAMEMODE_IsGameInWarmup() )
	{
		if ( !NETWORK_InClientMode() )
		{
			// If one of the players is a bot, skip the warmup
			for (ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++)
			{
				if ( playeringame[ulIdx] && !players[ulIdx].bSpectating )
				{
					if ( players[ulIdx].pSkullBot && !g_bReadyPlayers[ulIdx] )
					{
						GAMEMODE_AddPlayerReady(ulIdx);
					}
				}
				else
				{
					g_bReadyPlayers[ulIdx] = false;
				}
			}

			if ( GAMEMODE_AreEnoughPlayersReady() )
			{
				GAMEMODE_StartMatch();
			}
		}
		
		GAMEMODE_UpdateWarmupHUDMessage();
	}

	// [BB] If the state change, potentially trigger an event and update the saved state.
	if ( oldState != state )
	{
		// [BB] Apparently the round just ended.
		if ( ( oldState == GAMESTATE_INPROGRESS ) && ( state == GAMESTATE_INRESULTSEQUENCE ) )
			GAMEMODE_HandleEvent ( GAMEEVENT_ROUND_ENDS );
		// [BB] Changing from GAMESTATE_INPROGRESS to anything but GAMESTATE_INRESULTSEQUENCE means the roudn was aborted.
		else if ( oldState == GAMESTATE_INPROGRESS )
			GAMEMODE_HandleEvent ( GAMEEVENT_ROUND_ABORTED );
		// [BB] Changing from anything to GAMESTATE_INPROGRESS means the round started.
		else if ( state == GAMESTATE_INPROGRESS )
			GAMEMODE_HandleEvent ( GAMEEVENT_ROUND_STARTS );
		
		if ( oldState == GAMESTATE_WARMUP)
		{
			GAMEMODE_ResetReadyPlayers();
			if (NETWORK_GetState() != NETSTATE_SERVER)
				StatusBar->DetachAllMessages();
		}

		oldState = state;
	}
}

//*****************************************************************************
//
int GAMEMODE_ParserMustGetEnumName ( FScanner &sc, const char *EnumName, const char *FlagPrefix, int (*GetValueFromName) ( const char *Name ), const bool StringAlreadyParse = false )
{
	if ( StringAlreadyParse == false )
		sc.MustGetString ();
	FString flagname = FlagPrefix;
	flagname += sc.String;
	flagname.ToUpper();
	const int flagNum = GetValueFromName ( flagname.GetChars() );
	if ( flagNum == -1 )
		sc.ScriptError ( "Unknown %s '%s', on line %d in GAMEMODE.", EnumName, sc.String, sc.Line );
	return flagNum;
}

//*****************************************************************************
//
void GAMEMODE_ParseGamemodeInfoLump ( FScanner &sc, const GAMEMODE_e GameMode )
{
	sc.MustGetStringName("{");
	while (!sc.CheckString("}"))
	{
		sc.MustGetString();

		if (0 == stricmp (sc.String, "removeflag"))
		{
			g_GameModes[GameMode].ulFlags &= ~GAMEMODE_ParserMustGetEnumName( sc, "flag", "GMF_", GetValueGMF );
		}
		else if (0 == stricmp (sc.String, "addflag"))
		{
			g_GameModes[GameMode].ulFlags |= GAMEMODE_ParserMustGetEnumName( sc, "flag", "GMF_", GetValueGMF );
		}
		else if (0 == stricmp (sc.String, "name"))
		{
			sc.MustGetString();
			strncpy( g_GameModes[GameMode].szName, sc.String, 31 );
			g_GameModes[GameMode].szName[31] = 0;
		}
		else if (0 == stricmp (sc.String, "shortname"))
		{
			sc.MustGetString();
			strncpy( g_GameModes[GameMode].szShortName, sc.String, 8 );
			g_GameModes[GameMode].szShortName[8] = 0;
		}
		else if (0 == stricmp (sc.String, "f1texture"))
		{
			sc.MustGetString();
			strncpy( g_GameModes[GameMode].szF1Texture, sc.String, 8 );
			g_GameModes[GameMode].szF1Texture[8] = 0;
		}
		else if ((0 == stricmp (sc.String, "gamesettings")) || (0 == stricmp (sc.String, "lockedgamesettings")))
 		{
			GAMEMODE_ParseGameSettingBlock( sc, GameMode, !stricmp( sc.String, "lockedgamesettings" ));
		}
		else if (0 == stricmp (sc.String, "removegamesetting"))
		{
			sc.MustGetString();
			FBaseCVar *cvar = FindCVar( sc.String, NULL );

			if ( cvar != NULL )
			{
				// [AK] If this CVar is in the unlocked game settings list, remove it.
				for ( unsigned int i = 0; i < g_GameModes[GameMode].GameSettings.Size( ); i++ )
				{
					if ( g_GameModes[GameMode].GameSettings[i].pCVar == cvar )
					{
						g_GameModes[GameMode].GameSettings.Delete( i );
						break;
					}
				}

				// [AK] If this CVar is also in the locked game settings list, remove it too.
				for ( unsigned int i = 0; i < g_GameModes[GameMode].LockedGameSettings.Size( ); i++ )
				{
					if ( g_GameModes[GameMode].LockedGameSettings[i].pCVar == cvar )
					{
						g_GameModes[GameMode].LockedGameSettings.Delete( i );
						break;
					}
				}
			}
		}
		else
			sc.ScriptError ( "Unknown option '%s', on line %d in GAMEMODE.", sc.String, sc.Line );
	}

	// [AK] Get the game mode type (cooperative, deathmatch, or team game). There shouldn't be more than one enabled or none at all.
	ULONG ulFlags = g_GameModes[GameMode].ulFlags & ( GMF_COOPERATIVE | GMF_DEATHMATCH | GMF_TEAMGAME );
	if (( ulFlags == 0 ) || (( ulFlags & ( ulFlags - 1 )) != 0 ))
		sc.ScriptError( "Can't determine if '%s' is cooperative, deathmatch, or team-based.", g_GameModes[GameMode].szName );

	// [AK] Get the type of "players earn" flag this game mode is currently using.
	ulFlags = g_GameModes[GameMode].ulFlags & ( GMF_PLAYERSEARNKILLS | GMF_PLAYERSEARNFRAGS | GMF_PLAYERSEARNPOINTS | GMF_PLAYERSEARNWINS );

	// [AK] If all of these flags were removed or if more than one was added, then throw an error.
	if ( ulFlags == 0 )
		sc.ScriptError( "Players have no way of earning kills, frags, points, or wins in '%s'.", g_GameModes[GameMode].szName );
	else if (( ulFlags & ( ulFlags - 1 )) != 0 )
		sc.ScriptError( "There is more than one PLAYERSEARN flag enabled in '%s'.", g_GameModes[GameMode].szName );
}

//*****************************************************************************
//
void GAMEMODE_ParseGameSettingBlock ( FScanner &sc, const GAMEMODE_e GameMode, bool bLockFlags, bool bResetFlags )
{
	sc.MustGetStringName( "{" );

	// [AK] If this is the start of a "defaultgamesettings" or "defaultlockedgamesettings" block, empty the CVar
	// lists for all game modes. We don't want to do this more than once in a single GAMEMODE lump in case both
	// blocks are declared in the same lump.
	if (( GameMode == NUM_GAMEMODES ) && ( bResetFlags ))
	{
		for ( unsigned int mode = GAMEMODE_COOPERATIVE; mode < NUM_GAMEMODES; mode++ )
		{
			g_GameModes[mode].GameSettings.Clear( );
			g_GameModes[mode].LockedGameSettings.Clear( );
		}
	}

	while ( !sc.CheckString( "}" ))
	{
		GAMESETTING_s gameSetting;

		sc.MustGetString( );
		FBaseCVar *cvar = FindCVar( sc.String, NULL );

		// [AK] Make sure that this CVar exists.
		if ( cvar == NULL )
			sc.ScriptError( "'%s' is not a CVar.", sc.String );

		// [AK] Only CVars with the CVAR_GAMEMODESETTING flag are acceptable. If it's a flag CVar, then only
		// its parent CVar requires the flag. Also, in order to keep the implementation of this featue as
		// simple as possible, mask CVars are not accepted.
		if ( cvar->IsFlagCVar( ) == false )
		{
			if (( cvar->GetFlags( ) & CVAR_GAMEMODESETTING ) == false )
				sc.ScriptError( "'%s' cannot be used in a game settings block.", cvar->GetName( ));
		}
		else if (( static_cast<FFlagCVar *>( cvar )->GetValueVar( )->GetFlags( ) & CVAR_GAMEMODESETTING ) == false )
		{
			sc.ScriptError( "'%s' is a flag of a CVar that cannot be used in a game settings block.", cvar->GetName( ));
		}

		gameSetting.pCVar = cvar;

		// [AK] There must be an equal sign and value after the name of the CVar.
		sc.MustGetStringName( "=" );
		sc.MustGetString( );

		switch ( cvar->GetRealType( ))
		{
			case CVAR_Bool:
			case CVAR_Dummy:
			{
				if ( stricmp( sc.String, "true" ) == 0 )
					gameSetting.Val.Int = true;
				else if ( stricmp( sc.String, "false" ) == 0 )
					gameSetting.Val.Int = false;
				else
					gameSetting.Val.Int = !!atoi( sc.String );

				gameSetting.Type = CVAR_Int;
				break;
			}

			case CVAR_String:
			{
				gameSetting.Val.String = ncopystring( sc.String );
				gameSetting.Type = CVAR_String;
				break;
			}

			case CVAR_Float:
			{
				gameSetting.Val.Float = static_cast<float>( atof( sc.String ));
				gameSetting.Type = CVAR_Float;
				break;
			}

			default:
			{
				gameSetting.Val.Int = atoi( sc.String );
				gameSetting.Type = CVAR_Int;
				break;
			}
		}

		gameSetting.DefaultVal = gameSetting.Val;

		for ( unsigned int mode = GAMEMODE_COOPERATIVE; mode < NUM_GAMEMODES; mode++ )
		{
			// [AK] If this flag was added inside a "defaultgamesettings" or "defaultlockedgamesettings" block, apply
			// it to all the game modes. Otherwise, just apply it to the one we specified.
			if (( GameMode == NUM_GAMEMODES ) || ( GameMode == static_cast<GAMEMODE_e>( mode )))
			{
				// [AK] Do we want to add this CVar to the locked or unlocked game settings list?
				TArray<GAMESETTING_s> *pSettingList = bLockFlags ? &g_GameModes[mode].LockedGameSettings : &g_GameModes[mode].GameSettings;
				bool bAlreadyInList = false;

				// [AK] Check if this CVar's already in the list. If it is, just update the value.
				for ( unsigned int i = 0; i < pSettingList->Size( ); i++ )
				{
					GAMESETTING_s *pOtherSetting = &( *pSettingList )[i];

					if ( pOtherSetting->pCVar == gameSetting.pCVar )
					{
						pOtherSetting->DeleteStrings( );
						pOtherSetting->Val = gameSetting.Val;
						pOtherSetting->DefaultVal = gameSetting.DefaultVal;
						bAlreadyInList = true;
						break;
					}
				}

				// [AK] If it's not in the list, add it then.
				if ( bAlreadyInList == false )
					pSettingList->Push( gameSetting );
			}
		}
	}
}

//*****************************************************************************
//
void GAMEMODE_CondenseGameSettingBlock ( TArray<GAMESETTING_s> *pList )
{
	GAMESETTING_s *pSetting;
	GAMESETTING_s *pOtherSetting;

	for ( unsigned int i = 0; i < pList->Size( ); i++ )
	{
		pSetting = &( *pList )[i];

		// [AK] If this is a flag CVar, check if its "master" comes before it in this list.
		if ( pSetting->pCVar->IsFlagCVar( ))
		{
			FFlagCVar *flagCVar = static_cast<FFlagCVar *>( pSetting->pCVar );

			for ( int j = i - 1; j >= 0; j-- )
			{
				pOtherSetting = &( *pList )[j];

				// [AK] We found the flagset, so update its default value according to the value of this flag.
				if ( pOtherSetting->pCVar == flagCVar->GetValueVar( ))
				{
					if ( pSetting->Val.Int != 0 )
						pOtherSetting->Val.Int |= flagCVar->GetBitVal( );
					else
						pOtherSetting->Val.Int &= ~flagCVar->GetBitVal( );

					pOtherSetting->DefaultVal = pOtherSetting->Val;

					// [AK] We don't need the flag anymore, remove it!
					pList->Delete( i-- );
					break;
				}
			}
		}
		// [AK] If this is possibly a flagset, check if any of its flags come before it in this list.
		// If we find any, remove them because the flagset overrides their values in this case.
		else if ( pSetting->pCVar->GetRealType( ) == CVAR_Int )
		{
			for ( int j = i - 1; j >= 0; j-- )
			{
				pOtherSetting = &( *pList )[j];

				if (( pOtherSetting->pCVar->IsFlagCVar( )) && ( static_cast<FFlagCVar *>( pOtherSetting->pCVar )->GetValueVar( ) == pSetting->pCVar ))
					pList->Delete( j );
			}
		}
	}
}

//*****************************************************************************
//
void GAMEMODE_ParseGamemodeInfo( void )
{
	int lastlump = 0, lump;

	while ((lump = Wads.FindLump ("GAMEMODE", &lastlump)) != -1)
	{
		FScanner sc(lump);
		bool bParsedDefGameSettings = false;
		bool bParsedDefLockedSettings = false;

		while (sc.GetString ())
		{
			if (stricmp(sc.String, "defaultgamesettings") == 0)
			{
				// [AK] Don't allow more than one "defaultgamesettings" block in the same lump.
				if ( bParsedDefGameSettings )
					sc.ScriptError( "There is already a \"DefaultGameSettings\" block defined in this lump." );

				GAMEMODE_ParseGameSettingBlock( sc, NUM_GAMEMODES, false, !( bParsedDefGameSettings || bParsedDefLockedSettings ) );
				bParsedDefGameSettings = true;
			}
			else if (stricmp(sc.String, "defaultlockedgamesettings") == 0)
			{
				// [AK] Don't allow more than one "defaultlockedgamesettings" block in the same lump.
				if ( bParsedDefLockedSettings )
					sc.ScriptError( "There is already a \"DefaultLockedGameSettings\" block defined in this lump." );

				GAMEMODE_ParseGameSettingBlock( sc, NUM_GAMEMODES, true, !( bParsedDefGameSettings || bParsedDefLockedSettings ) );
				bParsedDefLockedSettings = true;
			}
			else
			{
				GAMEMODE_e GameMode = static_cast<GAMEMODE_e>( GAMEMODE_ParserMustGetEnumName( sc, "gamemode", "GAMEMODE_", GetValueGAMEMODE_e, true ) );
				GAMEMODE_ParseGamemodeInfoLump ( sc, GameMode );
			}
		}
	}

	const ULONG ulPrefixLen = strlen( "GAMEMODE_" );

	// [AK] Check if all game mode are acceptable.
	for ( unsigned int i = GAMEMODE_COOPERATIVE; i < NUM_GAMEMODES; i++ )
	{
		FString name = ( GetStringGAMEMODE_e( static_cast<GAMEMODE_e>( i )) + ulPrefixLen );
		name.ToLower( );

		// [AK] Make sure the game mode has a (short) name.
		if ( strlen( g_GameModes[i].szName ) == 0 )
			I_Error( "\"%s\" has no name.", name.GetChars( ));
		if ( strlen( g_GameModes[i].szShortName ) == 0 )
			I_Error( "\"%s\" has no short name.", name.GetChars( ));

		// [AK] Get the game mode type (cooperative, deathmatch, or team game). There shouldn't be more than one enabled or none at all.
		ULONG ulFlags = g_GameModes[i].ulFlags & ( GMF_COOPERATIVE | GMF_DEATHMATCH | GMF_TEAMGAME );
		if (( ulFlags == 0 ) || (( ulFlags & ( ulFlags - 1 )) != 0 ))
			I_Error( "Can't determine if \"%s\" is cooperative, deathmatch, or team-based.", name.GetChars( ));

		// [AK] Get the type of "players earn" flag this game mode is currently using.
		ulFlags = g_GameModes[i].ulFlags & ( GMF_PLAYERSEARNKILLS | GMF_PLAYERSEARNFRAGS | GMF_PLAYERSEARNPOINTS | GMF_PLAYERSEARNWINS );

		// [AK] If all of these flags were removed or if more than one was added, then throw an error.
		if ( ulFlags == 0 )
			I_Error( "Players have no way of earning kills, frags, points, or wins in \"%s\".", name.GetChars( ));
		else if (( ulFlags & ( ulFlags - 1 )) != 0 )
			I_Error( "There is more than one PLAYERSEARN flag enabled in \"%s\".", name.GetChars( ));

		// [AK] If the same CVar is used in the "GameSettings" and "LockedGameSettings" lists, only keep the locked one.
		for ( unsigned int j = 0; j < g_GameModes[i].LockedGameSettings.Size( ); j++ )
		{
			for ( unsigned int k = 0; k < g_GameModes[i].GameSettings.Size( ); k++ )
			{
				if ( g_GameModes[i].LockedGameSettings[j].pCVar == g_GameModes[i].GameSettings[k].pCVar )
				{
					g_GameModes[i].GameSettings.Delete( k );
					break;
				}
			}
		}

		// [AK] We must also check if there's two or more CVars in each list that can interact with each other (e.g. a flagset
		// and a flag belonging to that flagset). To keep the list as minimalistic as possible, always keep the flagset entry
		// and remove all flags associated with it.
		GAMEMODE_CondenseGameSettingBlock( &g_GameModes[i].GameSettings );
		GAMEMODE_CondenseGameSettingBlock( &g_GameModes[i].LockedGameSettings );
	}
	// Our default game mode is co-op.
	g_CurrentGameMode = GAMEMODE_COOPERATIVE;
}

//*****************************************************************************
//
ULONG GAMEMODE_GetFlags( GAMEMODE_e GameMode )
{
	if ( GameMode >= NUM_GAMEMODES )
		return ( 0 );

	return ( g_GameModes[GameMode].ulFlags );
}

//*****************************************************************************
//
ULONG GAMEMODE_GetCurrentFlags( void )
{
	return ( g_GameModes[g_CurrentGameMode].ulFlags );
}

//*****************************************************************************
//
bool GAMEMODE_IsNewMapStartMatchDelayOver( void )
{
	return ( NETWORK_GetState() != NETSTATE_SERVER ) || ( level.time >= TICRATE * 5 );
}

//*****************************************************************************
//
char *GAMEMODE_GetShortName( GAMEMODE_e GameMode )
{
	if ( GameMode >= NUM_GAMEMODES )
		return ( NULL );

	return ( g_GameModes[GameMode].szShortName );
}

//*****************************************************************************
//
char *GAMEMODE_GetName( GAMEMODE_e GameMode )
{
	if ( GameMode >= NUM_GAMEMODES )
		return ( NULL );

	return ( g_GameModes[GameMode].szName );
}

//*****************************************************************************
//
char *GAMEMODE_GetF1Texture( GAMEMODE_e GameMode )
{
	if ( GameMode >= NUM_GAMEMODES )
		return ( NULL );

	return ( g_GameModes[GameMode].szF1Texture );
}

//*****************************************************************************
//
void GAMEMODE_DetermineGameMode( void )
{
	g_CurrentGameMode = GAMEMODE_COOPERATIVE;
	if ( survival )
		g_CurrentGameMode = GAMEMODE_SURVIVAL;
	if ( invasion )
		g_CurrentGameMode = GAMEMODE_INVASION;
	if ( deathmatch )
		g_CurrentGameMode = GAMEMODE_DEATHMATCH;
	if ( teamplay )
		g_CurrentGameMode = GAMEMODE_TEAMPLAY;
	if ( duel )
		g_CurrentGameMode = GAMEMODE_DUEL;
	if ( terminator )
		g_CurrentGameMode = GAMEMODE_TERMINATOR;
	if ( lastmanstanding )
		g_CurrentGameMode = GAMEMODE_LASTMANSTANDING;
	if ( teamlms )
		g_CurrentGameMode = GAMEMODE_TEAMLMS;
	if ( possession )
		g_CurrentGameMode = GAMEMODE_POSSESSION;
	if ( teampossession )
		g_CurrentGameMode = GAMEMODE_TEAMPOSSESSION;
	if ( teamgame )
		g_CurrentGameMode = GAMEMODE_TEAMGAME;
	if ( ctf )
		g_CurrentGameMode = GAMEMODE_CTF;
	if ( oneflagctf )
		g_CurrentGameMode = GAMEMODE_ONEFLAGCTF;
	if ( skulltag )
		g_CurrentGameMode = GAMEMODE_SKULLTAG;
	if ( domination )
		g_CurrentGameMode = GAMEMODE_DOMINATION;
}

//*****************************************************************************
//
bool GAMEMODE_IsGameWaitingForPlayers( void )
{
	if ( survival )
		return ( SURVIVAL_GetState( ) == SURVS_WAITINGFORPLAYERS );
	else if ( invasion )
		return ( INVASION_GetState( ) == IS_WAITINGFORPLAYERS );
	else if ( duel )
		return ( DUEL_GetState( ) == DS_WAITINGFORPLAYERS );
	else if ( teamlms || lastmanstanding )
		return ( LASTMANSTANDING_GetState( ) == LMSS_WAITINGFORPLAYERS );
	else if ( possession || teampossession )
		return ( POSSESSION_GetState( ) == PSNS_WAITINGFORPLAYERS );
	else if ( teamgame )
		return ( TEAM_GetState() == TEAMS_WAITINGFORPLAYERS );
	// [BB] Non-coop game modes need two or more players.
	else if ( deathmatch )
		return ( DEATHMATCH_GetState( ) == DEATHMATCHS_WAITINGFORPLAYERS );
	// [BB] For coop games one player is enough.
	else
		return ( GAME_CountActivePlayers( ) < 1 );
}

//*****************************************************************************
//
bool GAMEMODE_IsGameInWarmup( void )
{
	if ( duel )
		return ( DUEL_GetState( ) == DS_WARMUP );
	else if ( teamlms || lastmanstanding )
		return ( LASTMANSTANDING_GetState( ) == LMSS_WARMUP );
	else if ( teamgame )
		return ( TEAM_GetState() == TEAMS_WARMUP );
	else if ( deathmatch && !possession && !teampossession && !survival && !invasion )
		return ( DEATHMATCH_GetState( ) == DEATHMATCHS_WARMUP );
	else
		return ( false );
}

//*****************************************************************************
//
bool GAMEMODE_IsGameInCountdown( void )
{
	if ( survival )
		return ( SURVIVAL_GetState( ) == SURVS_COUNTDOWN );
	// [BB] What about IS_COUNTDOWN?
	else if ( invasion )
		return ( INVASION_GetState( ) == IS_FIRSTCOUNTDOWN );
	else if ( duel )
		return ( DUEL_GetState( ) == DS_COUNTDOWN );
	else if ( teamlms || lastmanstanding )
		return ( LASTMANSTANDING_GetState( ) == LMSS_COUNTDOWN );
	// [BB] What about PSNS_PRENEXTROUNDCOUNTDOWN?
	else if ( possession || teampossession )
		return ( ( POSSESSION_GetState( ) == PSNS_COUNTDOWN ) || ( POSSESSION_GetState( ) == PSNS_NEXTROUNDCOUNTDOWN ) );
	else if ( teamgame )
		return ( TEAM_GetState() == TEAMS_COUNTDOWN );
	else if ( deathmatch )
		return ( DEATHMATCH_GetState() == DEATHMATCHS_COUNTDOWN );
	// [BB] The other game modes don't have a countdown.
	else
		return ( false );
}

//*****************************************************************************
//
bool GAMEMODE_IsGameInProgress( void )
{
	// [BB] Since there is currently no way unified way to check the state of
	// the active game mode, we have to do it manually.
	if ( survival )
		return ( SURVIVAL_GetState( ) == SURVS_INPROGRESS );
	else if ( invasion )
		return ( ( INVASION_GetState( ) == IS_INPROGRESS ) || ( INVASION_GetState( ) == IS_BOSSFIGHT )
			|| ( INVASION_GetState( ) == IS_WAVECOMPLETE ) || ( !sv_respawnonnewwave && INVASION_GetState( ) == IS_COUNTDOWN ) );
	else if ( duel )
		return ( DUEL_GetState( ) == DS_INDUEL );
	else if ( teamlms || lastmanstanding )
		return ( LASTMANSTANDING_GetState( ) == LMSS_INPROGRESS );
	else if ( possession || teampossession )
		return ( ( POSSESSION_GetState( ) == PSNS_INPROGRESS ) || ( POSSESSION_GetState( ) == PSNS_ARTIFACTHELD ) );
	else if ( teamgame )
		return ( TEAM_GetState() == TEAMS_INPROGRESS );
	// [BB] In non-coop game modes without warmup phase, we just say the game is
	// in progress when there are two or more players and the game is not frozen
	// due to the end level delay.
	else if ( deathmatch )
		return ( DEATHMATCH_GetState() == DEATHMATCHS_INPROGRESS );
	// [BB] For coop games one player is enough.
	else
		return ( ( GAME_CountActivePlayers( ) >= 1 ) && ( GAME_GetEndLevelDelay () == 0 ) );
}

//*****************************************************************************
//
bool GAMEMODE_IsGameInResultSequence( void )
{
	if ( survival )
		return ( SURVIVAL_GetState( ) == SURVS_MISSIONFAILED );
	else if ( invasion )
		return ( INVASION_GetState( ) == IS_MISSIONFAILED );
	else if ( duel )
		return ( DUEL_GetState( ) == DS_WINSEQUENCE );
	else if ( teamlms || lastmanstanding )
		return ( LASTMANSTANDING_GetState( ) == LMSS_WINSEQUENCE );
	else if ( teamgame )
		return ( TEAM_GetState() == TEAMS_WINSEQUENCE );
	else if ( deathmatch && !possession && !teampossession && !survival)
		return ( DEATHMATCH_GetState() == DEATHMATCHS_WINSEQUENCE );
	// [BB] The other game modes don't have such a sequnce. Arguably, possession
	// with PSNS_HOLDERSCORED could also be considered for this.
	// As substitute for such a sequence we consider whether the game is
	// frozen because of the end level delay.
	else
		return ( GAME_GetEndLevelDelay () > 0 );
}

//*****************************************************************************
//
bool GAMEMODE_IsGameInProgressOrResultSequence( void )
{
	return ( GAMEMODE_IsGameInProgress() || GAMEMODE_IsGameInResultSequence() );
}

//*****************************************************************************
//
bool GAMEMODE_IsLobbyMap( void )
{
	return level.flagsZA & LEVEL_ZA_ISLOBBY || stricmp(level.mapname, lobby) == 0;
}

//*****************************************************************************
//
bool GAMEMODE_IsLobbyMap( const char* mapname )
{
	// [BB] The level is not loaded yet, so we can't use level.flags2 directly.
	const level_info_t *levelinfo = FindLevelInfo( mapname, false );

	if (levelinfo == NULL)
	{
		return false;
	}

	return levelinfo->flagsZA & LEVEL_ZA_ISLOBBY || stricmp( levelinfo->mapname, lobby ) == 0;
}

//*****************************************************************************
//
bool GAMEMODE_IsNextMapCvarLobby( void )
{
	// If we're using a CVAR lobby and we're not on the lobby map, the next map
	// should always be the lobby.
	return strcmp(lobby, "") != 0 && stricmp(lobby, level.mapname) != 0;
}

//*****************************************************************************
//
bool GAMEMODE_IsTimelimitActive( void )
{
	// [AM] If the map is a lobby, ignore the timelimit.
	if ( GAMEMODE_IsLobbyMap( ) )
		return false;

	// [BB] In gamemodes that reset the time during a map reset, the timelimit doesn't make sense when the game is not in progress.
	if ( ( GAMEMODE_GetCurrentFlags() & GMF_MAPRESET_RESETS_MAPTIME ) && ( GAMEMODE_IsGameInProgress( ) == false ) )
		return false;

	// [BB] Teamlms doesn't support timelimit, so just turn it off in this mode.
	if ( teamlms )
		return false;

	// [BB] SuperGod insisted to have timelimit in coop, e.g. for jumpmaze, but its implementation conceptually doesn't work in invasion or survival.
	return (/*( deathmatch || teamgame ) &&*/ ( invasion == false ) && ( survival == false ) && timelimit );
}

//*****************************************************************************
//
void GAMEMODE_GetTimeLeftString( FString &TimeLeftString )
{
	LONG	lTimeLeft = (LONG)( timelimit * ( TICRATE * 60 )) - level.time;
	ULONG	ulHours, ulMinutes, ulSeconds;

	if ( lTimeLeft <= 0 )
		ulHours = ulMinutes = ulSeconds = 0;
	else
	{
		ulHours = lTimeLeft / ( TICRATE * 3600 );
		lTimeLeft -= ulHours * TICRATE * 3600;
		ulMinutes = lTimeLeft / ( TICRATE * 60 );
		lTimeLeft -= ulMinutes * TICRATE * 60;
		ulSeconds = lTimeLeft / TICRATE;
	}

	if ( ulHours )
		TimeLeftString.Format ( "%02d:%02d:%02d", static_cast<unsigned int> (ulHours), static_cast<unsigned int> (ulMinutes), static_cast<unsigned int> (ulSeconds) );
	else
		TimeLeftString.Format ( "%02d:%02d", static_cast<unsigned int> (ulMinutes), static_cast<unsigned int> (ulSeconds) );
}

//*****************************************************************************
//
void GAMEMODE_RespawnDeadSpectators( BYTE Playerstate )
{
	// [BB] This is server side.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	// [BB] Any player spawning in this game state would fail.
	// [BB] The same is true when we are starting a new game at the moment.
	if ( ( gamestate == GS_STARTUP ) || ( gamestate == GS_FULLCONSOLE ) || ( gameaction == ga_newgame )  || ( gameaction == ga_newgame2 ) )
	{
		return;
	}

	// Respawn any players who were downed during the previous round.
	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( playeringame[ulIdx] == false ) ||
			( PLAYER_IsTrueSpectator( &players[ulIdx] )))
		{
			continue;
		}

		// We don't want to respawn players as soon as the map starts; we only
		// [BB] respawn all dead players and dead spectators.
		if ((( players[ulIdx].mo == NULL ) ||
			( players[ulIdx].mo->health > 0 )) &&
			( players[ulIdx].bDeadSpectator == false ))
		{
			continue;
		}

		players[ulIdx].bSpectating = false;
		players[ulIdx].bDeadSpectator = false;
		if ( GAMEMODE_GetCurrentFlags() & GMF_USEMAXLIVES )
		{
			PLAYER_SetLivesLeft ( &players[ulIdx], GAMEMODE_GetMaxLives() - 1 );
		}
		players[ulIdx].playerstate = Playerstate;

		APlayerPawn *oldactor = players[ulIdx].mo;

		GAMEMODE_SpawnPlayer( ulIdx );

		// [CK] Ice corpses that are persistent between rounds must not affect
		// the client post-death in any gamemode with a countdown.
		if (( oldactor ) && ( oldactor->health > 0 || oldactor->flags & MF_ICECORPSE ))
		{
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_DestroyThing( oldactor );

			oldactor->Destroy( );
		}


		// [BB] If he's a bot, tell him that he successfully joined.
		if ( players[ulIdx].bIsBot && players[ulIdx].pSkullBot )
			players[ulIdx].pSkullBot->PostEvent( BOTEVENT_JOINEDGAME );
	}

	// [BB] Dead spectators were allowed to use chasecam, but are not necessarily allowed to use it
	// when alive again. Re-applying dmflags2 takes care of this.
	UCVarValue Val;
	Val.Int = dmflags2;
	dmflags2.ForceSet( Val, CVAR_Int );
}

void GAMEMODE_RespawnDeadSpectatorsAndPopQueue( BYTE Playerstate )
{
	GAMEMODE_RespawnDeadSpectators( Playerstate );
	// Let anyone who's been waiting in line join now.
	JOINQUEUE_PopQueue( -1 );
}

//*****************************************************************************
//
void GAMEMODE_RespawnAllPlayers( BOTEVENT_e BotEvent, playerstate_t PlayerState )
{
	// [BB] This is server side.
	if ( NETWORK_InClientMode() == false )
	{
		// Respawn the players.
		for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			if (( playeringame[ulIdx] == false ) ||
				( PLAYER_IsTrueSpectator( &players[ulIdx] )))
			{
				continue;
			}

			// [BB] Disassociate the player body, but don't delete it right now. The clients
			// still need the old body while the player is respawned so that they can properly
			// transfer their camera if they are spying through the eyes of the respawned player.
			APlayerPawn* pOldPlayerBody = players[ulIdx].mo;
			players[ulIdx].mo = NULL;

			players[ulIdx].playerstate = PlayerState;
			GAMEMODE_SpawnPlayer( ulIdx );

			if ( pOldPlayerBody )
			{
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_DestroyThing( pOldPlayerBody );

				pOldPlayerBody->Destroy( );
			}

			if ( players[ulIdx].pSkullBot && ( BotEvent < NUM_BOTEVENTS ) )
				players[ulIdx].pSkullBot->PostEvent( BotEvent );
		}
	}
}

//*****************************************************************************
//
void GAMEMODE_SpawnPlayer( const ULONG ulPlayer, bool bClientUpdate )
{
	// Spawn the player at their appropriate team start.
	if ( GAMEMODE_GetCurrentFlags() & GMF_TEAMGAME )
	{
		if ( players[ulPlayer].bOnTeam )
			G_TeamgameSpawnPlayer( ulPlayer, players[ulPlayer].ulTeam, bClientUpdate );
		else
			G_TemporaryTeamSpawnPlayer( ulPlayer, bClientUpdate );
	}
	// If deathmatch, just spawn at a random spot.
	else if ( GAMEMODE_GetCurrentFlags() & GMF_DEATHMATCH )
		G_DeathMatchSpawnPlayer( ulPlayer, bClientUpdate );
	// Otherwise, just spawn at their normal player start.
	else
		G_CooperativeSpawnPlayer( ulPlayer, bClientUpdate );
}

//*****************************************************************************
//
void GAMEMODE_ResetPlayersKillCount( const bool bInformClients )
{
	// Reset everyone's kill count.
	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		players[ulIdx].killcount = 0;
		players[ulIdx].ulRailgunShots = 0;
		// [BB] Also reset the things for ZADF_AWARD_DAMAGE_INSTEAD_KILLS.
		players[ulIdx].lPointCount = 0;
		players[ulIdx].ulUnrewardedDamageDealt = 0;

		// [BB] Notify the clients about the killcount change.
		if ( playeringame[ulIdx] && bInformClients && (NETWORK_GetState() == NETSTATE_SERVER) )
		{
			SERVERCOMMANDS_SetPlayerKillCount ( ulIdx );
			SERVERCOMMANDS_SetPlayerPoints ( ulIdx );
		}
	}
}

//*****************************************************************************
//
bool GAMEMODE_AreSpectatorsFordiddenToChatToPlayers( void )
{
	if ( ( lmsspectatorsettings & LMS_SPF_CHAT ) == false )
	{
		if (( teamlms || lastmanstanding ) && ( LASTMANSTANDING_GetState( ) == LMSS_INPROGRESS ))
			return true;

		if ( ( zadmflags & ZADF_ALWAYS_APPLY_LMS_SPECTATORSETTINGS ) && GAMEMODE_IsGameInProgress() )
			return true;
	}

	return false;
}

//*****************************************************************************
//
bool GAMEMODE_IsClientFordiddenToChatToPlayers( const ULONG ulClient )
{
	// [BB] If it's not a valid client, there are no restrictions. Note:
	// ulClient == MAXPLAYERS means the server wants to say something.
	if ( ulClient >= MAXPLAYERS )
		return false;

	// [BB] Ingame players are allowed to chat to other players.
	if ( players[ulClient].bSpectating == false )
		return false;

	return GAMEMODE_AreSpectatorsFordiddenToChatToPlayers();
}

//*****************************************************************************
//
bool GAMEMODE_PreventPlayersFromJoining( ULONG ulExcludePlayer )
{
	// [BB] No free player slots.
	if ( ( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( SERVER_CalcNumNonSpectatingPlayers( ulExcludePlayer ) >= static_cast<unsigned> (sv_maxplayers) ) )
		return true;

	// [BB] Don't let players join during intermission. They will be put in line and the queue is popped once intermission ends.
	if ( gamestate == GS_INTERMISSION )
		return true;

	// [BB] Duel in progress.
	if ( duel && ( DUEL_CountActiveDuelers( ) >= 2 ) )
		return true;

	// [BB] If lives are limited, players are not allowed to join most of the time.
	// [BB] The ga_worlddone check makes sure that in game players (at least in survival and survival invasion) aren't forced to spectate after a "changemap" map change.
	// [BB] The ga_newgame check fixes some problem when starting a survival invasion skirmish with bots while already in a survival invasion game with bots
	// (the consoleplayer is spawned as spectator in this case and leaves a ghost player upon joining)
	if ( ( gameaction != ga_worlddone ) && ( gameaction != ga_newgame ) && GAMEMODE_AreLivesLimited() && GAMEMODE_IsGameInProgressOrResultSequence() )
			return true;

	return false;
}

//*****************************************************************************
//
bool GAMEMODE_AreLivesLimited( void )
{
	// [BB] Invasion is a special case: If sv_maxlives == 0 in invasion, players have infinite lives.
	return ( ( ( sv_maxlives > 0 ) || ( invasion == false ) ) && ( GAMEMODE_GetCurrentFlags() & GMF_USEMAXLIVES ) );
}

//*****************************************************************************
//
unsigned int GAMEMODE_GetMaxLives( void )
{
	return ( ( sv_maxlives > 0 ) ? static_cast<unsigned int> ( sv_maxlives ) : 1 );
}

//*****************************************************************************
//
void GAMEMODE_AdjustActorSpawnFlags ( AActor *pActor )
{
	if ( pActor == NULL )
		return;

	// [BB] Since several Skulltag versions added NOGRAVITY to some spheres on default, allow the user to restore this behavior.
	if ( zacompatflags & ZACOMPATF_NOGRAVITY_SPHERES )
	{
		if ( ( stricmp ( pActor->GetClass()->TypeName.GetChars(), "InvulnerabilitySphere" ) == 0 )
			|| ( stricmp ( pActor->GetClass()->TypeName.GetChars(), "Soulsphere" ) == 0 )
			|| ( stricmp ( pActor->GetClass()->TypeName.GetChars(), "Megasphere" ) == 0 ) 
			|| ( stricmp ( pActor->GetClass()->TypeName.GetChars(), "BlurSphere" ) == 0 ) )
			pActor->flags |= MF_NOGRAVITY;
	}
}

//*****************************************************************************
//
void GAMEMODE_SpawnSpecialGamemodeThings ( void )
{
	// [BB] The server will let the clients know of any necessary spawns.
	if ( NETWORK_InClientMode() == false )
	{
		// Spawn the terminator artifact in terminator mode.
		if ( terminator )
			GAME_SpawnTerminatorArtifact( );

		// Spawn the possession artifact in possession/team possession mode.
		if ( possession || teampossession )
			GAME_SpawnPossessionArtifact( );
	}
}

//*****************************************************************************
//
void GAMEMODE_ResetSpecalGamemodeStates ( void )
{
	// [BB] If playing Domination reset ownership, even the clients can do this.
	if ( domination )
		DOMINATION_Reset();

	// [BB] If playing possession make sure to end the held countdown, even the clients can do this.
	if ( possession || teampossession )
	{
		POSSESSION_SetArtifactHoldTicks ( 0 );
		if ( POSSESSION_GetState() == PSNS_ARTIFACTHELD )
			POSSESSION_SetState( PSNS_PRENEXTROUNDCOUNTDOWN );
	}
}

//*****************************************************************************
//
bool GAMEMODE_IsSpectatorAllowedSpecial ( const int Special )
{
	return ( ( Special == Teleport ) || ( Special == Teleport_NoFog ) || ( Special == Teleport_NoStop ) || ( Special == Teleport_Line ) );
}

//*****************************************************************************
//
bool GAMEMODE_IsHandledSpecial ( AActor *Activator, int Special )
{
	// [BB] Non-player activated specials are never handled by the client.
	if ( Activator == NULL || Activator->player == NULL )
		return ( NETWORK_InClientMode() == false );

	// [EP/BB] Spectators activate a very limited amount of specials and ignore all others.
	if ( Activator->player->bSpectating )
		return ( GAMEMODE_IsSpectatorAllowedSpecial( Special ) );

	// [BB] Clients predict a very limited amount of specials for the local player and ignore all others (spectators were already handled)
	if ( NETWORK_InClientMode() )
		return ( NETWORK_IsConsolePlayer ( Activator ) && NETWORK_IsClientPredictedSpecial( Special ) );

	// [BB] Neither spectator, nor client.
	return true;
}

//*****************************************************************************
//
GAMESTATE_e GAMEMODE_GetState( void )
{
	if ( GAMEMODE_IsGameWaitingForPlayers() )
		return GAMESTATE_WAITFORPLAYERS;
	else if ( GAMEMODE_IsGameInWarmup() )
		return GAMESTATE_WARMUP;
	else if ( GAMEMODE_IsGameInCountdown() )
		return GAMESTATE_COUNTDOWN;
	else if ( GAMEMODE_IsGameInProgress() )
		return GAMESTATE_INPROGRESS;
	else if ( GAMEMODE_IsGameInResultSequence() )
		return GAMESTATE_INRESULTSEQUENCE;

	// [BB] Some of the above should apply, but this function always has to return something.
	return GAMESTATE_UNSPECIFIED;
}

//*****************************************************************************
//
void GAMEMODE_SetState( GAMESTATE_e GameState )
{
	if( GameState == GAMESTATE_WAITFORPLAYERS )
	{
		if ( survival )
			SURVIVAL_SetState( SURVS_WAITINGFORPLAYERS );
		else if ( invasion )
			INVASION_SetState( IS_WAITINGFORPLAYERS );
		else if ( duel )
			DUEL_SetState( DS_WAITINGFORPLAYERS );
		else if ( teamlms || lastmanstanding )
			LASTMANSTANDING_SetState( LMSS_WAITINGFORPLAYERS );
		else if ( possession || teampossession )
			POSSESSION_SetState( PSNS_WAITINGFORPLAYERS );
		else if ( teamgame )
			TEAM_SetState( TEAMS_WAITINGFORPLAYERS );
		else if ( deathmatch )
			DEATHMATCH_SetState( DEATHMATCHS_WAITINGFORPLAYERS );
	}
	else if ( GameState == GAMESTATE_WARMUP )
	{
		if ( duel )
			DUEL_SetState( DS_WARMUP );
		else if ( teamlms || lastmanstanding )
			LASTMANSTANDING_SetState( LMSS_WARMUP );
		else if ( teamgame )
			TEAM_SetState( TEAMS_WARMUP );
		else if ( deathmatch )
			DEATHMATCH_SetState( DEATHMATCHS_WARMUP );

		GAMEMODE_ResetReadyPlayers();
	}
	else if( GameState == GAMESTATE_COUNTDOWN )
	{
		if ( survival )
			SURVIVAL_SetState( SURVS_COUNTDOWN );
		else if ( invasion )
			INVASION_SetState( IS_FIRSTCOUNTDOWN );
		else if ( duel )
			DUEL_SetState( DS_COUNTDOWN );
		else if ( teamlms || lastmanstanding )
			LASTMANSTANDING_SetState( LMSS_COUNTDOWN );
		else if ( possession || teampossession )
			POSSESSION_SetState( PSNS_COUNTDOWN );
		else if ( teamgame )
			TEAM_SetState( TEAMS_COUNTDOWN );
		else if ( deathmatch )
			DEATHMATCH_SetState( DEATHMATCHS_COUNTDOWN );
	}
	else if( GameState == GAMESTATE_INPROGRESS )
	{
		if ( survival )
			SURVIVAL_SetState( SURVS_INPROGRESS );
		else if ( invasion )
			INVASION_SetState( IS_INPROGRESS );
		else if ( duel )
			DUEL_SetState( DS_INDUEL );
		else if ( teamlms || lastmanstanding )
			LASTMANSTANDING_SetState( LMSS_INPROGRESS );
		else if ( possession || teampossession )
			POSSESSION_SetState( PSNS_INPROGRESS );
		else if ( teamgame )
			TEAM_SetState( TEAMS_INPROGRESS );
		else if ( deathmatch )
			DEATHMATCH_SetState( DEATHMATCHS_INPROGRESS );
	}
	else if( GameState == GAMESTATE_INRESULTSEQUENCE )
	{
		if ( survival )
			SURVIVAL_SetState( SURVS_MISSIONFAILED );
		else if ( invasion )
			INVASION_SetState( IS_MISSIONFAILED );
		else if ( duel )
			DUEL_SetState( DS_WINSEQUENCE );
		else if ( teamlms || lastmanstanding )
			LASTMANSTANDING_SetState( LMSS_WINSEQUENCE );
		else if ( teamgame )
			TEAM_SetState( TEAMS_WINSEQUENCE );
		else if ( deathmatch )
			DEATHMATCH_SetState( DEATHMATCHS_WINSEQUENCE );
	}
}

//*****************************************************************************
//
void GAMEMODE_HandleEvent ( const GAMEEVENT_e Event, AActor *pActivator, const int DataOne, const int DataTwo )
{
	// [BB] Clients don't start scripts.
	if ( NETWORK_InClientMode() )
		return;

	// [BB] The activator of the event activates the event script.
	// The first argument is the type, e.g. GAMEEVENT_PLAYERFRAGS,
	// the second and third are specific to the event, e.g. the second is the number of the fragged player.
	// The third argument will be zero if it isn't used in the script.
	FBehavior::StaticStartTypedScripts( SCRIPT_Event, pActivator, true, Event, false, false, DataOne, DataTwo );
}

//*****************************************************************************
//
void GAMEMODE_DisplayStandardMessage( const char *pszMessage, const bool bInformClients )
{
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		DHUDMessageFadeOut	*pMsg;

		// Display the HUD message.
		pMsg = new DHUDMessageFadeOut( BigFont, pszMessage,
			160.4f,
			75.0f,
			320,
			200,
			CR_RED,
			3.0f,
			2.0f );

		StatusBar->AttachMessage( pMsg, MAKE_ID('C','N','T','R') );
	}
	// If necessary, send it to clients.
	else if ( bInformClients )
	{
		SERVERCOMMANDS_PrintHUDMessageFadeOut( pszMessage, 160.4f, 75.0f, 320, 200, CR_RED, 3.0f, 2.0f, "BigFont", false, MAKE_ID('C','N','T','R') );
	}
}

//*****************************************************************************
// [BB] Expects pszMessage already to be colorized with V_ColorizeString.
void GAMEMODE_DisplayCNTRMessage( const char *pszMessage, const bool bInformClients, const ULONG ulPlayerExtra, const ULONG ulFlags )
{
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		DHUDMessageFadeOut *pMsg = new DHUDMessageFadeOut( BigFont, pszMessage,
			1.5f,
			TEAM_MESSAGE_Y_AXIS,
			0,
			0,
			CR_UNTRANSLATED,
			3.0f,
			0.25f );
		StatusBar->AttachMessage( pMsg, MAKE_ID( 'C','N','T','R' ));
	}
	// If necessary, send it to clients.
	else if ( bInformClients )
	{
		SERVERCOMMANDS_PrintHUDMessageFadeOut( pszMessage, 1.5f, TEAM_MESSAGE_Y_AXIS, 0, 0, CR_UNTRANSLATED, 3.0f, 0.25f, "BigFont", false, MAKE_ID('C','N','T','R'), 0, ulPlayerExtra, ServerCommandFlags::FromInt( ulFlags ) );
	}
}

//*****************************************************************************
// [BB] Expects pszMessage already to be colorized with V_ColorizeString.
void GAMEMODE_DisplaySUBSMessage( const char *pszMessage, const bool bInformClients, const ULONG ulPlayerExtra, const ULONG ulFlags )
{
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		DHUDMessageFadeOut *pMsg = new DHUDMessageFadeOut( SmallFont, pszMessage,
			1.5f,
			TEAM_MESSAGE_Y_AXIS_SUB,
			0,
			0,
			CR_UNTRANSLATED,
			3.0f,
			0.25f );
		StatusBar->AttachMessage( pMsg, MAKE_ID( 'S','U','B','S' ));
	}
	// If necessary, send it to clients.
	else if ( bInformClients )
	{
		SERVERCOMMANDS_PrintHUDMessageFadeOut( pszMessage, 1.5f, TEAM_MESSAGE_Y_AXIS_SUB, 0, 0, CR_UNTRANSLATED, 3.0f, 0.25f, "SmallFont", false, MAKE_ID( 'S','U','B','S' ), 0, ulPlayerExtra, ServerCommandFlags::FromInt( ulFlags ) );
	}
}

//*****************************************************************************
//
GAMEMODE_e GAMEMODE_GetCurrentMode( void )
{
	return ( g_CurrentGameMode );
}

//*****************************************************************************
//
void GAMEMODE_SetCurrentMode( GAMEMODE_e GameMode )
{
	UCVarValue	 Val;
	g_CurrentGameMode = GameMode;	
	
	// [AK] Set any locked flags to what they're supposed to be in the new game mode.
	GAMEMODE_ReconfigureGameSettings();

	// [RC] Set all the CVars. We can't just use "= true;" because of the latched cvars.
	// (Hopefully Blzut's update will save us from this garbage.)

	Val.Bool = false;
	// [BB] Even though setting deathmatch and teamgame to false will set cooperative to true,
	// we need to set cooperative to false here first to clear survival and invasion.
	cooperative.ForceSet( Val, CVAR_Bool );
	deathmatch.ForceSet( Val, CVAR_Bool );
	teamgame.ForceSet( Val, CVAR_Bool );
	instagib.ForceSet( Val, CVAR_Bool );
	buckshot.ForceSet( Val, CVAR_Bool );

	Val.Bool = true;
	switch ( GameMode )
	{
	case GAMEMODE_COOPERATIVE:

		cooperative.ForceSet( Val, CVAR_Bool );
		break;
	case GAMEMODE_SURVIVAL:

		survival.ForceSet( Val, CVAR_Bool );
		break;
	case GAMEMODE_INVASION:

		invasion.ForceSet( Val, CVAR_Bool );
		break;
	case GAMEMODE_DEATHMATCH:

		deathmatch.ForceSet( Val, CVAR_Bool );
		break;
	case GAMEMODE_TEAMPLAY:

		teamplay.ForceSet( Val, CVAR_Bool );
		break;
	case GAMEMODE_DUEL:

		duel.ForceSet( Val, CVAR_Bool );
		break;
	case GAMEMODE_TERMINATOR:

		terminator.ForceSet( Val, CVAR_Bool );
		break;
	case GAMEMODE_LASTMANSTANDING:

		lastmanstanding.ForceSet( Val, CVAR_Bool );
		break;
	case GAMEMODE_TEAMLMS:

		teamlms.ForceSet( Val, CVAR_Bool );
		break;
	case GAMEMODE_POSSESSION:

		possession.ForceSet( Val, CVAR_Bool );
		break;
	case GAMEMODE_TEAMPOSSESSION:

		teampossession.ForceSet( Val, CVAR_Bool );
		break;
	case GAMEMODE_TEAMGAME:

		teamgame.ForceSet( Val, CVAR_Bool );
		break;
	case GAMEMODE_CTF:

		ctf.ForceSet( Val, CVAR_Bool );
		break;
	case GAMEMODE_ONEFLAGCTF:

		oneflagctf.ForceSet( Val, CVAR_Bool );
		break;
	case GAMEMODE_SKULLTAG:

		skulltag.ForceSet( Val, CVAR_Bool );
		break;
	case GAMEMODE_DOMINATION:

		domination.ForceSet( Val, CVAR_Bool );
		break;
	default:
		break;
	}
}

//*****************************************************************************
//
MODIFIER_e GAMEMODE_GetModifier( void )
{
	if ( instagib )
		return MODIFIER_INSTAGIB;
	else if ( buckshot )
		return MODIFIER_BUCKSHOT;
	else
		return MODIFIER_NONE;
}

//*****************************************************************************
//
void GAMEMODE_SetModifier( MODIFIER_e Modifier )
{
	UCVarValue	 Val;

	// Turn them all off.
	Val.Bool = false;
	instagib.ForceSet( Val, CVAR_Bool );
	buckshot.ForceSet( Val, CVAR_Bool );

	// Turn the selected one on.
	Val.Bool = true;
	switch ( Modifier )
	{
	case MODIFIER_INSTAGIB:
		instagib.ForceSet( Val, CVAR_Bool );
		break;
	case MODIFIER_BUCKSHOT:
		buckshot.ForceSet( Val, CVAR_Bool );
		break;
	default:
		break;
	}
}

//*****************************************************************************
//
ULONG GAMEMODE_GetCountdownTicks( void )
{
	if ( survival )
		return ( SURVIVAL_GetCountdownTicks() );
	else if ( invasion )
		return ( INVASION_GetCountdownTicks() );
	else if ( duel )
		return ( DUEL_GetCountdownTicks() );
	else if ( teamlms || lastmanstanding )
		return ( LASTMANSTANDING_GetCountdownTicks() );
	else if ( possession || teampossession )
		return ( POSSESSION_GetCountdownTicks() );
	else if ( teamgame )
		return ( TEAM_GetCountdownTicks() );
	else if ( deathmatch )
		return ( DEATHMATCH_GetCountdownTicks() );

	// [AK] The other gamemodes don't have a countdown, so just return zero.
	return 0;
}

//*****************************************************************************
//
void GAMEMODE_SetCountdownTicks( const ULONG Ticks )
{
	if ( survival )
		SURVIVAL_SetCountdownTicks( Ticks );
	else if ( invasion )
		INVASION_SetCountdownTicks( Ticks );
	else if ( duel )
		DUEL_SetCountdownTicks( Ticks );
	else if ( teamlms || lastmanstanding )
		LASTMANSTANDING_SetCountdownTicks( Ticks );
	else if ( possession || teampossession )
		POSSESSION_SetCountdownTicks( Ticks );
	else if ( teamgame )
		TEAM_SetCountdownTicks( Ticks );
	else if ( deathmatch )
		DEATHMATCH_SetCountdownTicks( Ticks );
}

//*****************************************************************************
//
void GAMEMODE_SetLimit( GAMELIMIT_e GameLimit, int value )
{
	UCVarValue Val;
	Val.Int = value;

	switch ( GameLimit )
	{
		case GAMELIMIT_FRAGS:
			fraglimit.ForceSet( Val, CVAR_Int );
			break;

		case GAMELIMIT_POINTS:
			pointlimit.ForceSet( Val, CVAR_Int );
			break;

		case GAMELIMIT_DUELS:
			duellimit.ForceSet( Val, CVAR_Int );
			break;

		case GAMELIMIT_WINS:
			winlimit.ForceSet( Val, CVAR_Int );
			break;

		case GAMELIMIT_WAVES:
			wavelimit.ForceSet( Val, CVAR_Int );
			break;
	}
}

//*****************************************************************************
//
bool GAMEMODE_IsCVarLocked( FBaseCVar *cvar )
{
	// [AK] Check if this CVar is supposed to be locked for the current game mode.
	for ( unsigned int i = 0; i < g_GameModes[g_CurrentGameMode].LockedGameSettings.Size( ); i++ )
	{
		if ( g_GameModes[g_CurrentGameMode].LockedGameSettings[i].pCVar == cvar )
			return true;
	}

	return false;
}

//*****************************************************************************
//
void GAMEMODE_ReconfigureGameSettings( bool bLockedOnly, bool bResetToDefault )
{
	GAMEMODE_s *const pGameMode = &g_GameModes[g_CurrentGameMode];
	GAMESETTING_s *pGameSetting;

	FBaseCVar::DisableGameModeLock( true );

	// [AK] Only reset unlocked CVars if we need to.
	if ( bLockedOnly == false )
	{
		for ( unsigned int i = 0; i < pGameMode->GameSettings.Size( ); i++ )
		{
			pGameSetting = &pGameMode->GameSettings[i];

			// [AK] Do we also want to reset this CVar to its default value?
			if ( bResetToDefault )
			{
				pGameSetting->DeleteStrings( false );
				pGameSetting->Val = pGameSetting->DefaultVal;
			}

			pGameSetting->pCVar->ForceSet( pGameSetting->Val, pGameSetting->Type );
		}
	}

	// [AK] All locked CVars should be reset every time the current game mode is reconfigured.
	for ( unsigned int i = 0; i < pGameMode->LockedGameSettings.Size( ); i++ )
	{
		pGameSetting = &pGameMode->LockedGameSettings[i];

		// [AK] Do we also want to reset this CVar to its default value?
		if ( bResetToDefault )
		{
			pGameSetting->DeleteStrings( false );
			pGameSetting->Val = pGameSetting->DefaultVal;
		}

		pGameSetting->pCVar->ForceSet( pGameSetting->Val, pGameSetting->Type );
	}

	FBaseCVar::DisableGameModeLock( false );
}

//*****************************************************************************
//
void GAMEMODE_AddPlayerReady(ULONG ulPlayer)
{
	if (NETWORK_InClientMode())
		return;

	if (NETWORK_GetState() != NETSTATE_SERVER)
		Printf("* %s \\cfis ready to fight!\n", players[ulPlayer].userinfo.GetName());
	else
		SERVER_Printf("* %s \\cfis ready to fight!\n", players[ulPlayer].userinfo.GetName());

	g_bReadyPlayers[ulPlayer] = true;
}

//*****************************************************************************
//
void GAMEMODE_RemovePlayerReady(ULONG ulPlayer)
{
	if (NETWORK_InClientMode())
		return;

	if (NETWORK_GetState() != NETSTATE_SERVER)
		Printf("* %s \\cfis not ready to fight anymore.\n", players[ulPlayer].userinfo.GetName());
	else
		SERVER_Printf("* %s \\cfis not ready to fight anymore.\n", players[ulPlayer].userinfo.GetName());

	g_bReadyPlayers[ulPlayer] = false;
}

//*****************************************************************************
//
void GAMEMODE_TogglePlayerReady(ULONG ulPlayer)
{
	if (NETWORK_InClientMode())
		return;

	if (g_bReadyPlayers[ulPlayer])
	{
		GAMEMODE_RemovePlayerReady(ulPlayer);
	}
	else
	{
		GAMEMODE_AddPlayerReady(ulPlayer);
	}
}

//*****************************************************************************
//
void GAMEMODE_ResetReadyPlayers(void)
{
	if (NETWORK_InClientMode())
		return;
	
	// If one of the players is a bot, skip the warmup
	for (ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++)
	{
		g_bReadyPlayers[ulIdx] = false;
	}
}

//*****************************************************************************
//
bool GAMEMODE_AreEnoughPlayersReady(void)
{
	if (NETWORK_InClientMode())
		return true;

	for (ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++)
	{
		if ( playeringame[ulIdx] && !players[ulIdx].bSpectating ) {
			if ( !g_bReadyPlayers[ulIdx] )
				return false;
		}
	}

	return true;
}

//*****************************************************************************
//
void GAMEMODE_UpdateWarmupHUDMessage(void)
{
	if ( NETWORK_GetState() != NETSTATE_SERVER )
	{
		FString readyString = "\\cdWarmup mode is enabled.";
		if ( !players[consoleplayer].bSpectating )
		{
			int key1 = 0;
			int key2 = 0;
			Bindings.GetKeysForCommand("ready", &key1, &key2);

			if (key2)
				readyString = readyString + " Press \\cj\'" + KeyNames[key1] + "\'\\cd or \\cj\'" + KeyNames[key2] + "\'\\cd";
			else if (key1)
				readyString = readyString + " Press \\cj\'" + KeyNames[key1] + "\'\\cd";
			else
				readyString = readyString + " Please bind the \\cj\'ready\'\\cd key in options";

			readyString += " to ready up";
		}

		V_ColorizeString(readyString);

		// Create the message.
		DHUDMessage *hudMessage = new DHUDMessage(V_GetFont("SmallFont"), readyString,
			1.5f, 0.3f,
			0, 0,
			CR_WHITE,
			0.0f);

		// Now attach the message.
		StatusBar->AttachMessage(hudMessage, MAKE_ID('W', 'A', 'R', 'M'));
	}

	if (NETWORK_InClientMode())
		return;

	char warmupString[90];
	int readyPlayers = 0, requiredPlayers = 0;

	for (ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++)
	{
		if ( playeringame[ulIdx] && !players[ulIdx].bSpectating ) {
			requiredPlayers++;
			if ( g_bReadyPlayers[ulIdx] )
				readyPlayers++;
		}
	}

	sprintf(warmupString, "Players ready: %d / %d", readyPlayers, requiredPlayers);
	if (NETWORK_GetState() != NETSTATE_SERVER)
	{
		// Create the message.
		DHUDMessage *hudMessage = new DHUDMessage(V_GetFont("SmallFont"), warmupString,
			1.5f, 0.33f,
			0, 0,
			CR_GREEN,
			0.0f);

		// Now attach the message.
		StatusBar->AttachMessage(hudMessage, MAKE_ID('W', 'A', 'R', 'S'));
	}
	else
		SERVERCOMMANDS_PrintHUDMessage(warmupString, 1.5f, 0.33f, 0, 0, CR_GREEN, 0.0f, "SmallFont", false, MAKE_ID('W', 'A', 'R', 'S'));
}

//*****************************************************************************
//
bool GAMEMODE_StartMatch()
{
	if ( !GAMEMODE_IsGameInWarmup() )
	{
		Printf("The game is not in warmup state\n");
		return false;
	}

	if ( ( duel && DUEL_CountActiveDuelers() != 2 )
		|| ( lastmanstanding && GAME_CountLivingAndRespawnablePlayers() < 2 )
		|| ( teamlms && LASTMANSTANDING_TeamsWithAlivePlayersOn( ) < 2 ) )
	{
		Printf("Not enough players\n");
		return false;
	}
	
	if ( duel )
	{
		if (GAMEMODE_IsLobbyMap())
			DUEL_SetState(DS_INDUEL);
		else if (sv_duelcountdowntime > 0)
			DUEL_StartCountdown((sv_duelcountdowntime * TICRATE) - 1);
		else
			DUEL_StartCountdown((10 * TICRATE) - 1);

		return true;
	}
	else if ( teamlms || lastmanstanding )
	{
		if ( sv_lmscountdowntime > 0 )
			LASTMANSTANDING_StartCountdown(( sv_lmscountdowntime * TICRATE ) - 1 );
		else
			LASTMANSTANDING_StartCountdown(( 10 * TICRATE ) - 1 );

		return true;
	}
	else if ( teamgame )
	{
		if ( sv_teamgamecountdowntime > 0 )
			TEAM_StartCountdown(( sv_teamgamecountdowntime * TICRATE ) - 1 );
		else
			TEAM_StartCountdown(( 10 * TICRATE ) - 1 );

		return true;
	}
	else if ( deathmatch && !possession && !teampossession && !survival && !invasion )
	{
		if ( sv_deathmatchcountdowntime > 0 )
			DEATHMATCH_StartCountdown(( sv_deathmatchcountdowntime * TICRATE ) - 1 );
		else
			DEATHMATCH_StartCountdown(( 10 * TICRATE ) - 1 );

		return true;
	}

	return false;
}

CCMD(start_match)
{
	if ( NETWORK_InClientMode() )
	{
		Printf("Only the server can start the duel.\n");
		return;
	}

	if ( GAMEMODE_StartMatch() && NETWORK_GetState() == NETSTATE_SERVER )
		SERVER_Printf("\\cf* Server administrator started the match!");
}

CCMD(ready)
{
	if (NETWORK_InClientMode())
	{
		CLIENTCOMMANDS_Ready();
	}
	else
	{
		if (GAMEMODE_IsGameInWarmup() && playeringame[consoleplayer] && !players[consoleplayer].bSpectating )
			GAMEMODE_TogglePlayerReady(consoleplayer);
	}
}
