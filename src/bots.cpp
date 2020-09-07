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
// Filename: bots.cpp
//
// Description: Contains bot functions
//
//-----------------------------------------------------------------------------

#include "astar.h"
#include "bots.h"
#include "botcommands.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "campaign.h"
#include "cl_demo.h"
#include "cmdlib.h"
#include "configfile.h"
#include "deathmatch.h"
#include "doomdef.h"
#include "doomstat.h"
#include "d_event.h"
#include "d_netinf.h"
#include "g_game.h"
#include "g_level.h"
#include "gi.h"
#include "i_system.h"
#include "joinqueue.h"
#include "m_random.h"
#include "network.h"
#include "p_acs.h"
#include "p_lnspec.h"
#include "p_local.h"
#include "r_main.h"
#include "r_sky.h"
#include "s_sound.h"
#include "sc_man.h"
#include "scoreboard.h"
#include "st_stuff.h"
#include "stats.h"
#include "sv_commands.h"
#include "sv_rcon.h"
#include "team.h"
#include "vectors.h"
#include "version.h"
#include "v_video.h"
#include "w_wad.h"
#include "p_trace.h"
#include "sbar.h"
#include "doomerrors.h"

//*****************************************************************************
//	VARIABLES

static	FRandom		BotSpawn( "BotSpawn" );
static	FRandom		BotRemove( "BotRemove" );
static	FRandom		g_RandomBotAimSeed( "RandomBotAimSeed" );
static	BOTSPAWN_t	g_BotSpawn[MAXPLAYERS];
static	TArray<BOTINFO_s>	g_BotInfo;
static	cycle_t		g_BotCycles;
static	bool		g_bBotIsInitialized[MAXPLAYERS];
static	LONG		g_lLastHeader;
static	bool		g_bBlockClearTable = false;
static	const char	*g_pszDataHeaders[NUM_DATAHEADERS] =
{
	"DH_COMMAND",
	"DH_STATEIDX",
	"DH_STATENAME",
	"DH_ONENTER",
	"DH_MAINLOOP",
	"DH_ONEXIT",
	"DH_EVENT",
	"DH_ENDONENTER",
	"DH_ENDMAINLOOP",
	"DH_ENDONEXIT",
	"DH_ENDEVENT",
	"DH_IFGOTO",
	"DH_IFNOTGOTO",
	"DH_GOTO",
	"DH_ORLOGICAL",
	"DH_ANDLOGICAL",
	"DH_ORBITWISE",
	"DH_EORBITWISE",
	"DH_ANDBITWISE",
	"DH_EQUALS",
	"DH_NOTEQUALS",
	"DH_LESSTHAN",
	"DH_LESSTHANEQUALS",
	"DH_GREATERTHAN",
	"DH_GREATERTHANEQUALS",
	"DH_NEGATELOGICAL",
	"DH_LSHIFT",
	"DH_RSHIFT",
	"DH_ADD",
	"DH_SUBTRACT",
	"DH_UNARYMINUS",
	"DH_MULTIPLY",
	"DH_DIVIDE",
	"DH_MODULUS",
	"DH_PUSHNUMBER",
	"DH_PUSHSTRINGINDEX",
	"DH_PUSHGLOBALVAR",
	"DH_PUSHLOCALVAR",
	"DH_DROPSTACKPOSITION",
	"DH_SCRIPTVARLIST",
	"DH_STRINGLIST",
	"DH_INCGLOBALVAR",
	"DH_DECGLOBALVAR",
	"DH_ASSIGNGLOBALVAR",
	"DH_ADDGLOBALVAR",
	"DH_SUBGLOBALVAR",
	"DH_MULGLOBALVAR",
	"DH_DIVGLOBALVAR",
	"DH_MODGLOBALVAR",
	"DH_INCLOCALVAR",
	"DH_DECLOCALVAR",
	"DH_ASSIGNLOCALVAR",
	"DH_ADDLOCALVAR",
	"DH_SUBLOCALVAR",
	"DH_MULLOCALVAR",
	"DH_DIVLOCALVAR",
	"DH_MODLOCALVAR",
	"DH_CASEGOTO",
	"DH_DROP",
	"DH_INCGLOBALARRAY",
	"DH_DECGLOBALARRAY",
	"DH_ASSIGNGLOBALARRAY",
	"DH_ADDGLOBALARRAY",
	"DH_SUBGLOBALARRAY",
	"DH_MULGLOBALARRAY",
	"DH_DIVGLOBALARRAY",
	"DH_MODGLOBALARRAY",
	"DH_PUSHGLOBALARRAY",
	"DH_SWAP",
	"DH_DUP",
	"DH_ARRAYSET",
};

//*****************************************************************************
static	LONG	g_lExpectedStackChange[NUM_DATAHEADERS] =
{
	0,		// DH_COMMAND
	0,		// DH_STATEIDX
	0,		// DH_STATENAME
	0,		// DH_ONENTER
	0,		// DH_MAINLOOP
	0,		// DH_ONEXIT
	0,		// DH_EVENT
	0,		// DH_ENDONENTER
	0,		// DH_ENDMAINLOOP
	0,		// DH_ENDONEXIT
	0,		// DH_ENDEVENT
	-1,		// DH_IFGOTO
	-1,		// DH_IFNOTGOTO
	0,		// DH_GOTO
	-1,		// DH_ORLOGICAL
	-1,		// DH_ANDLOGICAL
	-1,		// DH_ORBITWISE
	-1,		// DH_EORBITWISE
	-1,		// DH_ANDBITWISE
	-1,		// DH_EQUALS
	-1,		// DH_NOTEQUALS
	-1,		// DH_LESSTHAN
	-1,		// DH_LESSTHANEQUALS
	-1,		// DH_GREATERTHAN
	-1,		// DH_GREATERTHANEQUALS
	0,		// DH_NEGATELOGICAL
	-1,		// DH_LSHIFT
	-1,		// DH_RSHIFT
	-1,		// DH_ADD
	-1,		// DH_SUBTRACT
	0,		// DH_UNARYMINUS
	-1,		// DH_MULTIPLY
	-1,		// DH_DIVIDE
	-1,		// DH_MODULUS
	1,		// DH_PUSHNUMBER
	1,		// DH_PUSHSTRINGINDEX
	1,		// DH_PUSHGLOBALVAR
	1,		// DH_PUSHLOCALVAR
	-1,		// DH_DROPSTACKPOSITION
	0,		// DH_SCRIPTVARLIST
	0,		// DH_STRINGLIST
	0,		// DH_INCGLOBALVAR
	0,		// DH_DECGLOBALVAR
	-1,		// DH_ASSIGNGLOBALVAR
	-1,		// DH_ADDGLOBALVAR
	-1,		// DH_SUBGLOBALVAR
	-1,		// DH_MULGLOBALVAR
	-1,		// DH_DIVGLOBALVAR
	-1,		// DH_MODGLOBALVAR
	0,		// DH_INCLOCALVAR
	0,		// DH_DECLOCALVAR
	-1,		// DH_ASSIGNLOCALVAR
	-1,		// DH_ADDLOCALVAR
	-1,		// DH_SUBLOCALVAR
	-1,		// DH_MULLOCALVAR
	-1,		// DH_DIVLOCALVAR
	-1,		// DH_MODLOCALVAR
	0,		// DH_CASEGOTO
	-1,		// DH_DROP
	-1,		// DH_INCGLOBALARRAY
	-1,		// DH_DECGLOBALARRAY
	-2,		// DH_ASSIGNGLOBALARRAY
	-2,		// DH_ADDGLOBALARRAY
	-2,		// DH_SUBGLOBALARRAY
	-2,		// DH_MULGLOBALARRAY
	-2,		// DH_DIVGLOBALARRAY
	-2,		// DH_MODGLOBALARRAY
	0,		// DH_PUSHGLOBALARRAY
	0,		// DH_SWAP
	1,		// DH_DUP
	-3,		// DH_ARRAYSET
};

//*****************************************************************************
static	const char	*g_pszEventNames[NUM_BOTEVENTS] =
{
	"BOTEVENT_KILLED_BYENEMY",
	"BOTEVENT_KILLED_BYPLAYER",
	"BOTEVENT_KILLED_BYSELF",
	"BOTEVENT_KILLED_BYENVIORNMENT",
	"BOTEVENT_REACHED_GOAL",
	"BOTEVENT_GOAL_REMOVED",
	"BOTEVENT_DAMAGEDBY_PLAYER",
	"BOTEVENT_PLAYER_SAY",
	"BOTEVENT_ENEMY_KILLED",
	"BOTEVENT_RESPAWNED",
	"BOTEVENT_INTERMISSION",
	"BOTEVENT_NEWMAP",
	"BOTEVENT_ENEMY_USEDFIST",
	"BOTEVENT_ENEMY_USEDCHAINSAW",
	"BOTEVENT_ENEMY_FIREDPISTOL",
	"BOTEVENT_ENEMY_FIREDSHOTGUN",
	"BOTEVENT_ENEMY_FIREDSSG",
	"BOTEVENT_ENEMY_FIREDCHAINGUN",
	"BOTEVENT_ENEMY_FIREDMINIGUN",
	"BOTEVENT_ENEMY_FIREDROCKET",
	"BOTEVENT_ENEMY_FIREDGRENADE",
	"BOTEVENT_ENEMY_FIREDRAILGUN",
	"BOTEVENT_ENEMY_FIREDPLASMA",
	"BOTEVENT_ENEMY_FIREDBFG",
	"BOTEVENT_ENEMY_FIREDBFG10K",
	"BOTEVENT_PLAYER_USEDFIST",
	"BOTEVENT_PLAYER_USEDCHAINSAW",
	"BOTEVENT_PLAYER_FIREDPISTOL",
	"BOTEVENT_PLAYER_FIREDSHOTGUN",
	"BOTEVENT_PLAYER_FIREDSSG",
	"BOTEVENT_PLAYER_FIREDCHAINGUN",
	"BOTEVENT_PLAYER_FIREDMINIGUN",
	"BOTEVENT_PLAYER_FIREDROCKET",
	"BOTEVENT_PLAYER_FIREDGRENADE",
	"BOTEVENT_PLAYER_FIREDRAILGUN",
	"BOTEVENT_PLAYER_FIREDPLASMA",
	"BOTEVENT_PLAYER_FIREDBFG",
	"BOTEVENT_PLAYER_FIREDBFG10K",
	"BOTEVENT_USEDFIST",
	"BOTEVENT_USEDCHAINSAW",
	"BOTEVENT_FIREDPISTOL",
	"BOTEVENT_FIREDSHOTGUN",
	"BOTEVENT_FIREDSSG",
	"BOTEVENT_FIREDCHAINGUN",
	"BOTEVENT_FIREDMINIGUN",
	"BOTEVENT_FIREDROCKET",
	"BOTEVENT_FIREDGRENADE",
	"BOTEVENT_FIREDRAILGUN",
	"BOTEVENT_FIREDPLASMA",
	"BOTEVENT_FIREDBFG",
	"BOTEVENT_FIREDBFG10K",
	"BOTEVENT_PLAYER_JOINEDGAME",
	"BOTEVENT_JOINEDGAME",
	"BOTEVENT_DUEL_STARTINGCOUNTDOWN",
	"BOTEVENT_DUEL_FIGHT",
	"BOTEVENT_DUEL_WINSEQUENCE",
	"BOTEVENT_SPECTATING",
	"BOTEVENT_LMS_STARTINGCOUNTDOWN",
	"BOTEVENT_LMS_FIGHT",
	"BOTEVENT_LMS_WINSEQUENCE",
	"BOTEVENT_WEAPONCHANGE",
	"BOTEVENT_ENEMY_BFGEXPLODE",
	"BOTEVENT_PLAYER_BFGEXPLODE",
	"BOTEVENT_BFGEXPLODE",
	"BOTEVENT_RECEIVEDMEDAL",
};

//*****************************************************************************
//	CONSOLE VARIABLES

CVAR( Bool, bot_allowchat, true, CVAR_ARCHIVE );
CVAR( Int, botdebug_statechanges, 0, CVAR_ARCHIVE );
CVAR( Int, botdebug_states, 0, CVAR_ARCHIVE );
CVAR( Int, botdebug_commands, 0, CVAR_ARCHIVE );
CVAR( Int, botdebug_obstructiontest, 0, CVAR_ARCHIVE );
CVAR( Int, botdebug_walktest, 0, CVAR_ARCHIVE );
CVAR( Int, botdebug_dataheaders, 0, CVAR_ARCHIVE );
CVAR( Int, botdebug_showstackpushes, 0, CVAR_ARCHIVE );
CVAR( Int, botdebug_showgoal, 0, CVAR_ARCHIVE );
CVAR( Int, botdebug_showcosts, 0, CVAR_ARCHIVE );
CVAR( Int, botdebug_showevents, 0, CVAR_ARCHIVE );
CVAR( Float, botdebug_maxsearchnodes, 1024.0, CVAR_ARCHIVE );
CVAR( Float, botdebug_maxgiveupnodes, 512.0, CVAR_ARCHIVE );
CVAR( Float, botdebug_maxroamgiveupnodes, 4096.0, CVAR_ARCHIVE );

//*****************************************************************************
//
CUSTOM_CVAR( Int, botskill, 2, CVAR_SERVERINFO | CVAR_LATCH )
{
	if ( self < 0 )
		self = 0;
	if ( self > 4 )
		self = 4;
}

//*****************************************************************************
//
CUSTOM_CVAR( Int, botdebug_shownodes, 0, CVAR_ARCHIVE )
{
	if ( self < 0 )
		self = 0;

	if ( self == 0 )
		ASTAR_ClearVisualizations( );
}

//*****************************************************************************
//	PROTOTYPES

static	void					bots_ParseBotInfoLump( FScanner &sc );

void	SERVERCONSOLE_ReListPlayers( void );

// Ugh.
EXTERN_CVAR( Bool, sv_cheats );

//*****************************************************************************
//	FUNCTIONS

void BOTS_Construct( void )
{
	ULONG	ulIdx;

	// Set up all the hardcoded bot info.

	/**************** EXTRA BOTS *****************/

	// Initialize all botinfo pointers.
	g_BotInfo.Clear();

	// Initialize the botspawn table.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		g_BotSpawn[ulIdx].szName[0] = 0;
		g_BotSpawn[ulIdx].szTeam[0] = 0;
		g_BotSpawn[ulIdx].ulTick = 0;
	}

	// Initialize the "is initialized" table.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		g_bBotIsInitialized[ulIdx] = false;

	// Call BOTS_Destruct() when Skulltag closes.
	atterm( BOTS_Destruct );
}

//*****************************************************************************
//
void BOTS_Tick( void )
{
	ULONG	ulIdx;

	// First, handle any bots waiting to be spawned in skirmish games.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( g_BotSpawn[ulIdx].ulTick )
		{
			// It's time to spawn our bot.
			if ( --g_BotSpawn[ulIdx].ulTick == 0 )
			{
				ULONG		ulPlayerIdx = BOTS_FindFreePlayerSlot( );
				CSkullBot	*pBot = NULL;

				if ( ulPlayerIdx == MAXPLAYERS )
				{
					Printf( "The maximum number of players/bots has been reached.\n" );
					break;
				}

				// Verify that the bot's name matches one of the existing bots.
				if ( BOTS_IsValidName( g_BotSpawn[ulIdx].szName ))
					pBot = new CSkullBot( g_BotSpawn[ulIdx].szName, g_BotSpawn[ulIdx].szTeam, BOTS_FindFreePlayerSlot( ));
				else
				{
					Printf( PRINT_HIGH, "Invalid bot name: %s\n", g_BotSpawn[ulIdx].szName );
					break;
				}

				g_BotSpawn[ulIdx].szName[0] = 0;
				g_BotSpawn[ulIdx].szTeam[0] = 0;
			}
		}
	}
}

//*****************************************************************************
//
void BOTS_Destruct( void )
{
	ULONG	ulIdx;

	// First, go through and free all additional botinfo's.
	g_BotInfo.Clear();

	// Clear out any player's bot data.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( players[ulIdx].pSkullBot )
		{
			if ( g_bBotIsInitialized[ulIdx] )
				players[ulIdx].pSkullBot->PreDelete( );

			delete ( players[ulIdx].pSkullBot );
			players[ulIdx].pSkullBot = NULL;
			players[ulIdx].bIsBot = false;
		}
	}
}

//*****************************************************************************
//
bool BOTS_AddBotInfo( BOTINFO_s *pBotInfo )
{
	BOTINFO_s botInfo;

	// Now copy all the data we passed in into this block.
	botInfo.bRevealed						= pBotInfo->bRevealed;
	botInfo.bRevealedByDefault			= pBotInfo->bRevealedByDefault;
	botInfo.Accuracy						= pBotInfo->Accuracy;
	botInfo.Anticipation					= pBotInfo->Anticipation;
	botInfo.Evade							= pBotInfo->Evade;
	botInfo.Intellect						= pBotInfo->Intellect;
	botInfo.ulRailgunColor				= pBotInfo->ulRailgunColor;
	botInfo.ulChatFrequency				= pBotInfo->ulChatFrequency;
	botInfo.ReactionTime					= pBotInfo->ReactionTime;
	botInfo.Perception					= pBotInfo->Perception;
	sprintf( botInfo.szFavoriteWeapon,	"%s", pBotInfo->szFavoriteWeapon );
	sprintf( botInfo.szClassName,			"%s", pBotInfo->szClassName );
	sprintf( botInfo.szColor,				"%s", pBotInfo->szColor );
	sprintf( botInfo.szGender,			"%s", pBotInfo->szGender );
	sprintf( botInfo.szSkinName,			"%s", pBotInfo->szSkinName );
	sprintf( botInfo.szName,				"%s", pBotInfo->szName );
	sprintf( botInfo.szScriptName,		"%s", pBotInfo->szScriptName );
	sprintf( botInfo.szChatFile,			"%s", pBotInfo->szChatFile );
	sprintf( botInfo.szChatLump,			"%s", pBotInfo->szChatLump );
	g_BotInfo.Push ( botInfo );

	return ( true );
}

//*****************************************************************************
//
void BOTS_ParseBotInfo( void )
{
	int			iCurLump;
	int			iLastLump = 0; // [BL] We can't use LONG here since 64-bit won't like it.

	// Search through all loaded wads for a lump called "BOTINFO".
	while (( iCurLump = Wads.FindLump( "BOTINFO", &iLastLump )) != -1 )
	{
		// Make pszBotInfo point to the raw data (which should be a text file) in the BOTINFO lump.
		FScanner sc( iCurLump );

		// Parse the lump.
		bots_ParseBotInfoLump( sc );
	}
}

//*****************************************************************************
//
bool BOTS_IsValidName( char *pszName )
{
	if ( pszName == NULL )
		return ( false );
	else
	{
		ULONG	ulIdx;
		char	szName[64];

		// Search through the botinfo. If the given name matches one of the names, return true.
		for ( ulIdx = 0; ulIdx < g_BotInfo.Size(); ulIdx++ )
		{
			sprintf( szName, "%s", g_BotInfo[ulIdx].szName );
			V_ColorizeString( szName );
			V_RemoveColorCodes( szName );
			if ( stricmp( szName, pszName ) == 0 )
				return ( true );
		}

		// Didn't find anything.
		return ( false );
	}
}

//*****************************************************************************
//
ULONG BOTS_FindFreePlayerSlot( void )
{
	ULONG	ulIdx;

	// Don't allow us to add bots past the max. clients limit.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) &&
		( static_cast<LONG>(SERVER_CalcNumPlayers( )) >= sv_maxclients ))
	{
		return ( MAXPLAYERS );
	}

	// Loop through all the player slots. If it's an inactive slot, break.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( playeringame[ulIdx] )
			continue;

		// Maybe a player is busy trying to connect on this slot?
		if (( NETWORK_GetState( ) == NETSTATE_SERVER ) &&
			( SERVER_GetClient( ulIdx )->State != CLS_FREE ))
		{
			continue;
		}

		return ( ulIdx );
	}

	return ( MAXPLAYERS );
}

//*****************************************************************************
//
void BOTS_RemoveBot( ULONG ulPlayerIdx, bool bExitMsg )
{
	ULONG	ulIdx;

	if (( ulPlayerIdx >= MAXPLAYERS ) ||
		( players[ulPlayerIdx].pSkullBot == NULL ))
	{
		return;
	}

	if ( bExitMsg )
	{
		if ( NETWORK_GetState( ) != NETSTATE_SERVER )
			Printf( PRINT_HIGH, "%s \\c-left the game.\n", players[ulPlayerIdx].userinfo.GetName() );
		else
			SERVER_Printf( "%s \\c-left the game.\n", players[ulPlayerIdx].userinfo.GetName() );
	}

	// [BB] Morphed bots need to be unmorphed before disconnecting.
	if (players[ulPlayerIdx].morphTics)
		P_UndoPlayerMorph (&players[ulPlayerIdx], &players[ulPlayerIdx]);

	// Remove the bot from the game.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DisconnectPlayer( ulPlayerIdx, ulPlayerIdx, SVCF_SKIPTHISCLIENT );

	// If he's carrying an important item like a flag, etc., make sure he, 
	// drops it before he leaves.
	if ( players[ulPlayerIdx].mo )
		players[ulPlayerIdx].mo->DropImportantItems( true );

	// If this bot was eligible to get an assist, cancel that.
	TEAM_CancelAssistsOfPlayer ( ulPlayerIdx );

	playeringame[ulPlayerIdx] = false;
	
	// [BB] Run the disconnect scripts now that the bot is leaving the game.
	if (( players[ulPlayerIdx].bSpectating == false ) ||
		( players[ulPlayerIdx].bDeadSpectator ))
	{
		PLAYER_LeavesGame( ulPlayerIdx );
	}

	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		// Redo the scoreboard.
		SERVERCONSOLE_ReListPlayers( );
	
		// [RC] Update clients using the RCON utility.
		SERVER_RCON_UpdateInfo( SVRCU_PLAYERDATA );
	}

	if ( g_bBotIsInitialized[ulPlayerIdx] )
		players[ulPlayerIdx].pSkullBot->PreDelete( );
/*
	else
	{
		if ( players[ulPlayerIdx].pSkullBot->m_bHasScript )
		{
			players[ulPlayerIdx].pSkullBot->m_bHasScript = false;
//			players[ulPlayerIdx].pSkullBot->m_ScriptData.RawData.CloseOnDestruct = false;
		}
	}
*/
	if ( players[ulPlayerIdx].pSkullBot )
	{
		delete ( players[ulPlayerIdx].pSkullBot );
		players[ulPlayerIdx].pSkullBot = NULL;
	}
	players[ulPlayerIdx].bIsBot = false;

	// Tell the join queue module that a player has left the game.
	if ( gameaction != ga_worlddone )
		JOINQUEUE_PlayerLeftGame( ulPlayerIdx, true );

	// If this bot was the enemy of another bot, tell the bot.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( playeringame[ulIdx] == false ) || ( players[ulIdx].pSkullBot == NULL ) || ( g_bBotIsInitialized[ulIdx] == false ))
			continue;

		if ( players[ulIdx].pSkullBot->m_ulPlayerEnemy == ulPlayerIdx )
			players[ulIdx].pSkullBot->m_ulPlayerEnemy = MAXPLAYERS;
	}

	// Refresh the HUD since the number of players in the game is potentially changing.
	SCOREBOARD_RefreshHUD( );

	// [K6] If there are no more bots left, clear the bot nodes.
	if ( BOTS_CountBots( ) == 0 && ASTAR_IsInitialized( ) )
		ASTAR_ClearNodes( );
}

//*****************************************************************************
//
void BOTS_RemoveAllBots( bool bExitMsg )
{
	ULONG	ulIdx;

	// Loop through all the players and delete all the bots.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( playeringame[ulIdx] && players[ulIdx].pSkullBot && g_bBotIsInitialized[ulIdx] )
			BOTS_RemoveBot( ulIdx, bExitMsg );
	}
}

//*****************************************************************************
//
void BOTS_ResetCyclesCounter( void )
{
	g_BotCycles.Reset();
}

//*****************************************************************************
//
// TEEEEEEEEEEEEEMP
bool PTR_AimTraverse (intercept_t* in);

bool BOTS_IsPathObstructed( fixed_t Distance, AActor *pSource )
{
	angle_t			Angle;
	angle_t			Pitch;
	angle_t			Meat;
	FTraceResults	TraceResults;
	fixed_t			vx, vy, vz, sz;

	Meat = pSource->angle;
	Angle = /*pSource->angle*/Meat >> ANGLETOFINESHIFT;
	Pitch = 0;//pSource->pitch >> ANGLETOFINESHIFT;

	vx = FixedMul( finecosine[Pitch], finecosine[Angle] );
	vy = FixedMul( finecosine[Pitch], finesine[Angle] );
//	vx = FixedMul( pSource->x, Distance );
//	vy = FixedMul( pSource->y, Distance );
	vz = finesine[Pitch];

	sz = pSource->z - pSource->floorclip + pSource->height;// + (fixed_t)(chase_height * FRACUNIT);

//	if ( P_PathTraverse( CurPos.x, CurPos.y, DestPos.x, DestPos.y, PT_ADDLINES|PT_ADDTHINGS, PTR_AimTraverse ) == false )
//	return ( P_PathTraverse( pSource->x, pSource->y, vx, vy, PT_ADDLINES|PT_ADDTHINGS, PTR_AimTraverse ) == false );
	if ( Trace( pSource->x,	// Source X
				pSource->y,		// Source Y
				// [BB] gameinfo.StepHeight seems to be gone from ZDoom, but even before the removal it was always 0.
				pSource->z /*+ gameinfo.StepHeight*/,//sz,				// Source Z
				pSource->Sector,// Source sector
				vx,
				vy,
				vz,
				Distance * FRACUNIT,	// Maximum search distance
				MF_SOLID|MF_SHOOTABLE,		// Actor mask (ignore actors without this flag)
				0,				// Wall mask
				pSource,		// Actor to ignore
				TraceResults	// Callback
				))
	{
		/*
		if ( TraceResults.Line )
		{
			Printf( "Line\n---Flags: " );
			if ( TraceResults.Line->flags & ML_BLOCKING )
				Printf( "BLOCKING " );
			if ( TraceResults.Line->flags & ML_BLOCKMONSTERS )
				Printf( "BLOCKMONSTERS " );
			if ( TraceResults.Line->flags & ML_TWOSIDED )
				Printf( "TWOSIDED " );
			if ( TraceResults.Line->flags & ML_DONTPEGTOP )
				Printf( "UPPERUNPEG " );
			if ( TraceResults.Line->flags & ML_DONTPEGBOTTOM )
				Printf( "LOWERUNPEG " );
			if ( TraceResults.Line->flags & ML_SECRET )
				Printf( "SECRET " );
			if ( TraceResults.Line->flags & ML_SOUNDBLOCK )
				Printf( "SOUNDBLOCK " );
			if ( TraceResults.Line->flags & ML_DONTDRAW )
				Printf( "INVIS " );
			if ( TraceResults.Line->flags & ML_MAPPED )
				Printf( "ONMAP " );
			if ( TraceResults.Line->flags & ML_REPEAT_SPECIAL )
				Printf( "REPEATABLE " );
			if ( TraceResults.Line->flags & ML_MONSTERSCANACTIVATE )
				Printf( "MONSTERACTIVATE " );
			if ( TraceResults.Line->flags & ML_PASSUSE_BOOM )
				Printf( "PASSUSE " );
			if ( TraceResults.Line->flags & ML_BLOCKEVERYTHING )
				Printf( "BLOCKALL " );
			Printf( "\n" );
		}
		*/
		return ( true );
	}
	
	return ( false );
}

//*****************************************************************************
//
bool BOTS_IsVisible( AActor *pActor1, AActor *pActor2 )
{
	angle_t	Angle;

	// Check if we have a line of sight to this object.
	if ( P_CheckSight( pActor1, pActor2, SF_SEEPASTBLOCKEVERYTHING ) == false )
		return ( false );

	Angle = R_PointToAngle2( pActor1->x,
					  pActor1->y, 
					  pActor2->x,
					  pActor2->y );

	Angle -= pActor1->angle;

	// If the object within our view range, tell the bot.
	return (( Angle <= ANG45 ) || ( Angle >= ((ULONG)ANGLE_1 * 315 )));
}

//*****************************************************************************
//
void BOTS_ArchiveRevealedBotsAndSkins( FConfigFile *f )
{
	ULONG	ulIdx;
	FString string;

	for ( ulIdx = 0; ulIdx < g_BotInfo.Size(); ulIdx++ )
	{
		// If this bot isn't revealed, or isn't revealed by default, don't archive it.
		if (( g_BotInfo[ulIdx].bRevealed == false ) || ( g_BotInfo[ulIdx].bRevealedByDefault ))
			continue;

		string.Format( "\"%s\"", g_BotInfo[ulIdx].szName );

		V_ColorizeString( string );
		V_RemoveColorCodes( string );
		f->SetValueForKey( string, "1" );
	}

	for ( ulIdx = 0; ulIdx < (ULONG)skins.Size(); ulIdx++ )
	{
		// If this bot isn't revealed, or isn't revealed by default, don't archive it.
		if (( skins[ulIdx].bRevealed == false ) || ( skins[ulIdx].bRevealedByDefault ))
			continue;

		string.Format( "\"%s\"", skins[ulIdx].name );

		V_ColorizeString( string );
		V_RemoveColorCodes( string );
		f->SetValueForKey( string, "1" );
	}
}

//*****************************************************************************
//
void BOTS_RestoreRevealedBotsAndSkins( FConfigFile &config )
{
	char		szBuffer[64];
	ULONG		ulIdx;
	const char	*pszKey;
	const char	*pszValue;

	while ( config.NextInSection ( pszKey, pszValue ))
	{
		for ( ulIdx = 0; ulIdx < g_BotInfo.Size(); ulIdx++ )
		{
			if ( g_BotInfo[ulIdx].bRevealed )
				continue;

			sprintf( szBuffer, "%s", g_BotInfo[ulIdx].szName );
			V_ColorizeString( szBuffer );
			V_RemoveColorCodes( szBuffer );

			if ( strnicmp( pszKey + 1, szBuffer, strlen( szBuffer )) != 0 )
				continue;

			g_BotInfo[ulIdx].bRevealed = true;
		}

		for ( ulIdx = 0; ulIdx < (ULONG)skins.Size(); ulIdx++ )
		{
			if ( skins[ulIdx].bRevealed )
				continue;

			sprintf( szBuffer, "%s", skins[ulIdx].name );
			V_ColorizeString( szBuffer );
			V_RemoveColorCodes( szBuffer );

			if ( strnicmp( pszKey + 1, szBuffer, strlen( szBuffer )) != 0 )
				continue;

			skins[ulIdx].bRevealed = true;
		}
	}
}

//*****************************************************************************
//
bool BOTS_IsBotInitialized( ULONG ulBot )
{
	return ( g_bBotIsInitialized[ulBot] );
}

//*****************************************************************************
//
BOTSKILL_e BOTS_AdjustSkill( CSkullBot *pBot, BOTSKILL_e Skill )
{
	LONG	lSkill;

	lSkill = (LONG)Skill;
	switch ( botskill )
	{
	// In easy mode, take two points off the skill level.
	case 0:

		lSkill -= 2;
		break;
	// In somewhat easy, take off one point.
	case 1:

		lSkill -= 1;
		break;
	// In somewhat hard, add one point.
	case 3:

		lSkill += 1;
		break;
	// In nightmare, add two points.
	case 4:

		lSkill += 2;
		break;
	}

	if ( pBot->m_bSkillIncrease )
		lSkill++;
	if ( pBot->m_bSkillDecrease )
		lSkill--;

	if ( lSkill < BOTSKILL_VERYPOOR )
		lSkill = (LONG)BOTSKILL_VERYPOOR;
	else if ( lSkill >= NUM_BOT_SKILLS )
		lSkill = NUM_BOT_SKILLS - 1;

	return ( (BOTSKILL_e)lSkill );
}

//*****************************************************************************
//
void BOTS_PostWeaponFiredEvent( ULONG ulPlayer, BOTEVENT_e EventIfSelf, BOTEVENT_e EventIfEnemy, BOTEVENT_e EventIfPlayer )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( playeringame[ulIdx] == false ) || ( players[ulIdx].pSkullBot == NULL ))
			continue;

		if ( players[ulIdx].pSkullBot->m_ulPlayerEnemy == ulPlayer )
			players[ulIdx].pSkullBot->PostEvent( EventIfEnemy );
		else if ( ulPlayer == ulIdx )
			players[ulIdx].pSkullBot->PostEvent( EventIfSelf );
		else
			players[ulIdx].pSkullBot->PostEvent( EventIfPlayer );
	}
}

//*****************************************************************************
//
void BOTS_RemoveGoal( AActor* pGoal )
{
	// If this is a bot's goal object, tell the bot it's been removed.
	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( playeringame[ulIdx] == false ) || ( players[ulIdx].pSkullBot == NULL ) || ( g_bBotIsInitialized[ulIdx] == false ))
			continue;

		if ( pGoal == players[ulIdx].pSkullBot->m_pGoalActor )
		{
			players[ulIdx].pSkullBot->m_pGoalActor = NULL;
			players[ulIdx].pSkullBot->PostEvent( BOTEVENT_GOAL_REMOVED );
			players[ulIdx].pSkullBot->m_ulPathType = BOTPATHTYPE_NONE;
//			ASTAR_ClearPath( ulIdx );
		}
	}
}

//*****************************************************************************
//
ULONG BOTS_CountBots( void )
{
	ULONG	ulNumBots = 0;

	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ++ulIdx )
	{
		if ( playeringame[ulIdx] && players[ulIdx].bIsBot )
			++ulNumBots;
	}

	return ( ulNumBots );
}

//*****************************************************************************
//*****************************************************************************
//
void bots_ParseBotInfoLump( FScanner &sc )
{
	char		szKey[64];
	char		szValue[128];
	BOTINFO_s	BotInfo;

	// Begin parsing that text. COM_Parse will create a token (com_token), and
	// pszBotInfo will skip past the token.
	while ( sc.GetString( ))
	{
		// Initialize our botinfo variable.
		BotInfo.bRevealed					= true;
		BotInfo.bRevealedByDefault			= true;
		BotInfo.Accuracy					= BOTSKILL_MEDIUM;
		BotInfo.Anticipation				= BOTSKILL_MEDIUM;
		BotInfo.Evade						= BOTSKILL_MEDIUM;
		BotInfo.Intellect					= BOTSKILL_MEDIUM;
		BotInfo.ulRailgunColor				= 0;
		BotInfo.ulChatFrequency				= 50;
		BotInfo.ReactionTime				= BOTSKILL_MEDIUM;
		BotInfo.Perception					= BOTSKILL_MEDIUM;
		sprintf( BotInfo.szFavoriteWeapon, 	"pistol" );
		sprintf( BotInfo.szClassName,		"random" );
		sprintf( BotInfo.szColor,			"00 00 00" );
		sprintf( BotInfo.szGender,			"male" );
		sprintf( BotInfo.szName,			"UNNAMED BOT" );
		BotInfo.szScriptName[0]				= 0;
		sprintf( BotInfo.szSkinName,		"base" );
		BotInfo.szChatFile[0]				= 0;
		BotInfo.szChatLump[0]				= 0;

		while ( sc.String[0] != '{' )
		{
			// [BB] We can clear all previously defined bots by just calling BOTS_Destruct().
			if (sc.Compare("CLEARBOTS"))
				BOTS_Destruct();
			sc.GetString( );
		}

		// We've encountered a starting bracket. Now continue to parse until we hit an end bracket.
		while ( sc.String[0] != '}' )
		{
			// The current token should be our key. (key = value) If it's an end bracket, break.
			sc.GetString( );
			strncpy( szKey, sc.String, 63 );
			szKey[63] = 0;
			if ( sc.String[0] == '}' )
				break;

			// The following key must be an = sign. If not, the user made an error!
			sc.GetString( );
			if ( stricmp( sc.String, "=" ) != 0 )
				I_Error( "BOTS_ParseBotInfo: Missing \"=\" in BOTINFO lump for field \"%s\"!", szKey );

			// The last token should be our value.
			sc.GetString( );
			strncpy( szValue, sc.String, 127 );
			szValue[127] = 0;

			// Now try to match our key with a valid bot info field.
			if ( stricmp( szKey, "name" ) == 0 )
			{
				strncpy( BotInfo.szName, szValue, 63 );
				BotInfo.szName[63] = 0;
			}
			else if ( stricmp( szKey, "accuracy" ) == 0 )
			{
				switch ( atoi( szValue ))
				{
				case -2:
				case -1:
				case 0:
				case 1:
				case 2:
				case 3:
				case 4:
				case 5:
				case 6:

					BotInfo.Accuracy = (BOTSKILL_e)( atoi( szValue ) + (LONG)BOTSKILL_LOW );
					break;
				default:

					I_Error( "BOTS_ParseBotInfo: Expected value from -2-6 for field \"accuracy\"!" );
					break;
				}
			}
			else if (( stricmp( szKey, "intelect" ) == 0 ) || ( stricmp( szKey, "intellect" ) == 0 ))
			{
				switch ( atoi( szValue ))
				{
				case -2:
				case -1:
				case 0:
				case 1:
				case 2:
				case 3:
				case 4:
				case 5:
				case 6:

					BotInfo.Intellect = (BOTSKILL_e)( atoi( szValue ) + (LONG)BOTSKILL_LOW );
					break;
				default:

					I_Error( "BOTS_ParseBotInfo: Expected value from -2-6 for field \"intellect\"!" );
					break;
				}
			}
			else if ( stricmp( szKey, "evade" ) == 0 )
			{
				switch ( atoi( szValue ))
				{
				case -2:
				case -1:
				case 0:
				case 1:
				case 2:
				case 3:
				case 4:
				case 5:
				case 6:

					BotInfo.Evade = (BOTSKILL_e)( atoi( szValue ) + (LONG)BOTSKILL_LOW );
					break;
				default:

					I_Error( "BOTS_ParseBotInfo: Expected value from -2-6 for field \"evade\"!" );
					break;
				}
			}
			else if ( stricmp( szKey, "anticipation" ) == 0 )
			{
				switch ( atoi( szValue ))
				{
				case -2:
				case -1:
				case 0:
				case 1:
				case 2:
				case 3:
				case 4:
				case 5:
				case 6:

					BotInfo.Anticipation = (BOTSKILL_e)( atoi( szValue ) + (LONG)BOTSKILL_LOW );
					break;
				default:

					I_Error( "BOTS_ParseBotInfo: Expected value from -2-6 for field \"anticipation\"!" );
					break;
				}
			}
			else if ( stricmp( szKey, "reactiontime" ) == 0 )
			{
				switch ( atoi( szValue ))
				{
				case -2:
				case -1:
				case 0:
				case 1:
				case 2:
				case 3:
				case 4:
				case 5:
				case 6:

					BotInfo.ReactionTime = (BOTSKILL_e)( atoi( szValue ) + (LONG)BOTSKILL_LOW );
					break;
				default:

					I_Error( "BOTS_ParseBotInfo: Expected value from -2-6 for field \"reactiontime\"!" );
					break;
				}
			}
			else if ( stricmp( szKey, "perception" ) == 0 )
			{
				switch ( atoi( szValue ))
				{
				case -2:
				case -1:
				case 0:
				case 1:
				case 2:
				case 3:
				case 4:
				case 5:
				case 6:

					BotInfo.Perception = (BOTSKILL_e)( atoi( szValue ) + (LONG)BOTSKILL_LOW );
					break;
				default:

					I_Error( "BOTS_ParseBotInfo: Expected value from -2-6 for field \"perception\"!" );
					break;
				}
			}
			else if ( stricmp( szKey, "favoriteweapon" ) == 0 )
			{
				strncpy( BotInfo.szFavoriteWeapon, szValue, 31 );
				BotInfo.szFavoriteWeapon[31] = 0;
			}
			else if ( stricmp( szKey, "class" ) == 0 )
			{
				strncpy( BotInfo.szClassName, szValue, 31 );
				BotInfo.szClassName[31] = 0;
			}
			else if ( stricmp( szKey, "color" ) == 0 )
			{
				strncpy( BotInfo.szColor, szValue, 15 );
				BotInfo.szColor[15] = 0;
			}
			else if ( stricmp( szKey, "gender" ) == 0 )
			{
				strncpy( BotInfo.szGender, szValue, 15 );
				BotInfo.szGender[15] = 0;
			}
			else if ( stricmp( szKey, "skin" ) == 0 )
			{
				strncpy( BotInfo.szSkinName, szValue, 31 );
				BotInfo.szSkinName[31] = 0;
			}
			else if ( stricmp( szKey, "railcolor" ) == 0 )
			{
				if ( stricmp( szValue, "blue" ) == 0 )
					BotInfo.ulRailgunColor = RAILCOLOR_BLUE;
				else if ( stricmp( szValue, "red" ) == 0 )
					BotInfo.ulRailgunColor = RAILCOLOR_RED;
				else if ( stricmp( szValue, "yellow" ) == 0 )
					BotInfo.ulRailgunColor = RAILCOLOR_YELLOW;
				else if ( stricmp( szValue, "black" ) == 0 )
					BotInfo.ulRailgunColor = RAILCOLOR_BLACK;
				else if ( stricmp( szValue, "silver" ) == 0 )
					BotInfo.ulRailgunColor = RAILCOLOR_SILVER;
				else if ( stricmp( szValue, "gold" ) == 0 )
					BotInfo.ulRailgunColor = RAILCOLOR_GOLD;
				else if ( stricmp( szValue, "green" ) == 0 )
					BotInfo.ulRailgunColor = RAILCOLOR_GREEN;
				else if ( stricmp( szValue, "white" ) == 0 )
					BotInfo.ulRailgunColor = RAILCOLOR_WHITE;
				else if ( stricmp( szValue, "purple" ) == 0 )
					BotInfo.ulRailgunColor = RAILCOLOR_PURPLE;
				else if ( stricmp( szValue, "orange" ) == 0 )
					BotInfo.ulRailgunColor = RAILCOLOR_ORANGE;
				else if ( stricmp( szValue, "rainbow" ) == 0 )
					BotInfo.ulRailgunColor = RAILCOLOR_RAINBOW;
				else
					BotInfo.ulRailgunColor = atoi( szValue );
			}
			else if ( stricmp( szKey, "chatfrequency" ) == 0 )
			{
				BotInfo.ulChatFrequency = atoi( szValue );
				if ( BotInfo.ulChatFrequency > 100 )
					I_Error( "BOTS_ParseBotInfo: Expected value from 0-100 for field \"chatfrequency\"!" );
			}
			else if ( stricmp( szKey, "revealed" ) == 0 )
			{
				if ( stricmp( szValue, "true" ) == 0 )
					BotInfo.bRevealed = true;
				else if ( stricmp( szValue, "false" ) == 0 )
					BotInfo.bRevealed = false;
				else
					BotInfo.bRevealed = !!atoi( szValue );

				BotInfo.bRevealedByDefault = BotInfo.bRevealed;
			}
			else if ( stricmp( szKey, "script" ) == 0 )
			{
				if ( strlen( szValue ) > 8 )
					I_Error( "BOTS_ParseBotInfo: Value for BOTINFO property \"script\" (\"%s\") cannot exceed 8 characters!", szValue );

				strncpy( BotInfo.szScriptName, szValue, 64 );
				BotInfo.szScriptName[64] = 0;
			}
			else if ( stricmp( szKey, "chatfile" ) == 0 )
			{
				strncpy( BotInfo.szChatFile, szValue, 127 );
				BotInfo.szChatFile[127] = 0;
			}
			else if ( stricmp( szKey, "chatlump" ) == 0 )
			{
				if ( strlen( szValue ) > 31 )
					I_Error( "BOTS_ParseBotInfo: Value for BOTINFO property \"chatlump\" (\"%s\") cannot exceed 31 characters!", szValue );

				strncpy( BotInfo.szChatLump, szValue, 31 );
				BotInfo.szChatLump[31] = 0;
			}
			else
				I_Error( "BOTS_ParseBotInfo: Unknown BOTINFO property, \"%s\"!", szKey );
		}

		// Finally, add our completed botinfo.
		BOTS_AddBotInfo( &BotInfo );
	}
}

//*****************************************************************************
//*****************************************************************************
//
ULONG BOTINFO_GetNumBotInfos( void )
{
	return ( g_BotInfo.Size() );
}

//*****************************************************************************
//
char *BOTINFO_GetName( ULONG ulIdx )
{
	if ( ulIdx >= g_BotInfo.Size() )
		return ( NULL );

	return ( g_BotInfo[ulIdx].szName );
}

//*****************************************************************************
//
BOTSKILL_e BOTINFO_GetAccuracy( ULONG ulIdx )
{
	if ( ulIdx >= g_BotInfo.Size() )
		return ( BOTSKILL_VERYPOOR );

	return ( g_BotInfo[ulIdx].Accuracy );
}

//*****************************************************************************
//
BOTSKILL_e BOTINFO_GetIntellect( ULONG ulIdx )
{
	if ( ulIdx >= g_BotInfo.Size() )
		return ( BOTSKILL_VERYPOOR );

	return ( g_BotInfo[ulIdx].Intellect );
}

//*****************************************************************************
//
BOTSKILL_e BOTINFO_GetEvade( ULONG ulIdx )
{
	if ( ulIdx >= g_BotInfo.Size() )
		return ( BOTSKILL_VERYPOOR );

	return ( g_BotInfo[ulIdx].Evade );
}

//*****************************************************************************
//
BOTSKILL_e BOTINFO_GetAnticipation( ULONG ulIdx )
{
	if ( ulIdx >= g_BotInfo.Size() )
		return ( BOTSKILL_VERYPOOR );

	return ( g_BotInfo[ulIdx].Anticipation );
}

//*****************************************************************************
//
BOTSKILL_e BOTINFO_GetReactionTime( ULONG ulIdx )
{
	if ( ulIdx >= g_BotInfo.Size() )
		return ( BOTSKILL_VERYPOOR );

	return ( g_BotInfo[ulIdx].ReactionTime );
}

//*****************************************************************************
//
BOTSKILL_e BOTINFO_GetPerception( ULONG ulIdx )
{
	if ( ulIdx >= g_BotInfo.Size() )
		return ( BOTSKILL_VERYPOOR );

	return ( g_BotInfo[ulIdx].Perception );
}

//*****************************************************************************
//
char *BOTINFO_GetFavoriteWeapon( ULONG ulIdx )
{
	if ( ulIdx >= g_BotInfo.Size() )
		return ( NULL );

	return ( g_BotInfo[ulIdx].szFavoriteWeapon );
}

//*****************************************************************************
//
char *BOTINFO_GetColor( ULONG ulIdx )
{
	if ( ulIdx >= g_BotInfo.Size() )
		return ( NULL );

	return ( g_BotInfo[ulIdx].szColor );
}

//*****************************************************************************
//
char *BOTINFO_GetGender( ULONG ulIdx )
{
	if ( ulIdx >= g_BotInfo.Size() )
		return ( NULL );

	return ( g_BotInfo[ulIdx].szGender );
}

//*****************************************************************************
//
char *BOTINFO_GetClass( ULONG ulIdx )
{
	if ( ulIdx >= g_BotInfo.Size() )
		return ( NULL );

	return ( g_BotInfo[ulIdx].szClassName );
}

//*****************************************************************************
//
char *BOTINFO_GetSkin( ULONG ulIdx )
{
	if ( ulIdx >= g_BotInfo.Size() )
		return ( NULL );

	return ( g_BotInfo[ulIdx].szSkinName );
}

//*****************************************************************************
//
ULONG BOTINFO_GetRailgunColor( ULONG ulIdx )
{
	if ( ulIdx >= g_BotInfo.Size() )
		return ( -1 );

	return ( g_BotInfo[ulIdx].ulRailgunColor );
}

//*****************************************************************************
//
ULONG BOTINFO_GetChatFrequency( ULONG ulIdx )
{
	if ( ulIdx >= g_BotInfo.Size() )
		return ( -1 );

	return ( g_BotInfo[ulIdx].ulChatFrequency );
}

//*****************************************************************************
//
bool BOTINFO_GetRevealed( ULONG ulIdx )
{
	if ( ulIdx >= g_BotInfo.Size() )
		return ( false );

	return ( g_BotInfo[ulIdx].bRevealed );
}

//*****************************************************************************
//
char *BOTINFO_GetChatFile( ULONG ulIdx )
{
	if ( ulIdx >= g_BotInfo.Size() )
		return ( NULL );

	return ( g_BotInfo[ulIdx].szChatFile );
}

//*****************************************************************************
//
char *BOTINFO_GetChatLump( ULONG ulIdx )
{
	if ( ulIdx >= g_BotInfo.Size() )
		return ( NULL );

	return ( g_BotInfo[ulIdx].szChatLump );
}

//*****************************************************************************
//*****************************************************************************
//
void BOTSPAWN_AddToTable( const char *pszBotName, const char *pszBotTeam )
{
	ULONG	ulIdx;
	ULONG	ulTick;

	if (( pszBotName == NULL ) || ( pszBotName[0] == 0 ))
		return;

	ulTick = TICRATE;
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( g_BotSpawn[ulIdx].szName[0] )
		{
			ulTick += TICRATE;
			continue;
		}

		sprintf( g_BotSpawn[ulIdx].szName, "%s", pszBotName );
		if (( pszBotTeam == NULL ) || ( pszBotTeam[0] == 0 ))
			g_BotSpawn[ulIdx].szTeam[0] = 0;
		else
			sprintf( g_BotSpawn[ulIdx].szTeam, "%s", pszBotTeam );
		g_BotSpawn[ulIdx].ulTick = ulTick;
		break;
	}
}

//*****************************************************************************
//
void BOTSPAWN_BlockClearTable( void )
{
	g_bBlockClearTable = true;
}

//*****************************************************************************
//
void BOTSPAWN_ClearTable( void )
{
	if ( g_bBlockClearTable )
	{
		g_bBlockClearTable = false;
		return;
	}

	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		g_BotSpawn[ulIdx].szName[0] = 0;
		g_BotSpawn[ulIdx].szTeam[0] = 0;
		g_BotSpawn[ulIdx].ulTick = 0;
	}
}

//*****************************************************************************
//
char *BOTSPAWN_GetName( ULONG ulIdx )
{
	if ( ulIdx >= MAXPLAYERS )
		return ( NULL );

	return ( g_BotSpawn[ulIdx].szName );
}

//*****************************************************************************
//
void BOTSPAWN_SetName( ULONG ulIdx, char *pszName )
{
	if ( ulIdx >= MAXPLAYERS )
		return;

	if ( pszName == NULL )
		g_BotSpawn[ulIdx].szName[0] = 0;
	else
		sprintf( g_BotSpawn[ulIdx].szName, "%s", pszName );
}

//*****************************************************************************
//
char *BOTSPAWN_GetTeam( ULONG ulIdx )
{
	if ( ulIdx >= MAXPLAYERS )
		return ( NULL );

	return ( g_BotSpawn[ulIdx].szTeam );
}

//*****************************************************************************
//
void BOTSPAWN_SetTeam( ULONG ulIdx, char *pszTeam )
{
	if ( ulIdx >= MAXPLAYERS )
		return;

	sprintf( g_BotSpawn[ulIdx].szTeam, "%s", pszTeam );
}

//*****************************************************************************
//
ULONG BOTSPAWN_GetTicks( ULONG ulIdx )
{
	if ( ulIdx >= MAXPLAYERS )
		return ( 0 );

	return ( g_BotSpawn[ulIdx].ulTick );
}

//*****************************************************************************
//
void BOTSPAWN_SetTicks( ULONG ulIdx, ULONG ulTicks )
{
	if ( ulIdx >= MAXPLAYERS )
		return;

	g_BotSpawn[ulIdx].ulTick = ulTicks;
}

//*****************************************************************************
//*****************************************************************************
//
CSkullBot::CSkullBot( char *pszName, char *pszTeamName, ULONG ulPlayerNum )
{
	ULONG	ulIdx;

	g_bBotIsInitialized[ulPlayerNum] = false;

	// [BB/K6] Make sure that the bot nodes are initialized.
	if ( ASTAR_IsInitialized( ) == false )
		ASTAR_BuildNodes( );

	// First, initialize all variables.
	m_posTarget.x = 0;
	m_posTarget.y = 0;
	m_posTarget.z = 0;
	m_pPlayer = NULL;
	m_ulBotInfoIdx = g_BotInfo.Size();
	m_bHasScript = false;
	m_lForwardMove = 0;
	m_lSideMove = 0;
	m_bForwardMovePersist = false;
	m_bSideMovePersist = false;
	m_ulLastSeenPlayer = MAXPLAYERS;
	m_ulLastPlayerDamagedBy = MAXPLAYERS;
	m_ulPlayerKilledBy = MAXPLAYERS;
	m_ulPlayerKilled = MAXPLAYERS;
	m_lButtons = 0;
	m_bAimAtEnemy = false;
	m_ulAimAtEnemyDelay = 0;
	m_AngleDelta = 0;
	m_AngleOffBy = 0;
	m_AngleDesired = 0;
	m_bTurnLeft = false;
	m_pGoalActor = NULL;
	m_ulPlayerEnemy = MAXPLAYERS;
	m_ulPathType = BOTPATHTYPE_NONE;
	m_PathGoalPos.x = 0;
	m_PathGoalPos.y = 0;
	m_PathGoalPos.z = 0;
	for ( ulIdx = 0; ulIdx < MAX_REACTION_TIME; ulIdx++ )
	{
		m_EnemyPosition[ulIdx].x = 0;
		m_EnemyPosition[ulIdx].y = 0;
		m_EnemyPosition[ulIdx].z = 0;
	}
	m_ulLastEnemyPositionTick = 0;
	m_bSkillIncrease = false;
	m_bSkillDecrease = false;
	m_ulLastMedalReceived = NUM_MEDALS;
	m_lQueueHead = 0;
	m_lQueueTail = 0;
	for ( ulIdx = 0; ulIdx < MAX_STORED_EVENTS; ulIdx++ )
	{
		m_StoredEventQueue[ulIdx].Event = NUM_BOTEVENTS;
		m_StoredEventQueue[ulIdx].lTick = -1;
	}

	// We've asked to spawn a certain bot by name. Search through the bot database and see
	// if there's a bot with the name we've asked for.
	if ( pszName )
	{
		char	szName[64];

		for ( ulIdx = 0; ulIdx < g_BotInfo.Size(); ulIdx++ )
		{
			sprintf( szName, "%s", g_BotInfo[ulIdx].szName );
			V_ColorizeString( szName );
			V_RemoveColorCodes( szName );
			if ( stricmp( szName, pszName ) == 0 )
			{
				// If the bot was hidden, reveal it!
				if ( g_BotInfo[ulIdx].bRevealed == false )
				{
					Printf( "Hidden bot \"%s\\c-\" has now been revealed!\n", g_BotInfo[ulIdx].szName );
					g_BotInfo[ulIdx].bRevealed = true;
				}

				m_ulBotInfoIdx = ulIdx;
				break;
			}
		}
		// We've already handled the "what if there's no match" exception.
	}
	// If the user doesn't input a name, randomly select an available bot.
	else
	{
		ULONG	ulRandom;

		while ( m_ulBotInfoIdx == g_BotInfo.Size() )
		{
			ulRandom = ( BotSpawn( ) % g_BotInfo.Size() );
			if ( g_BotInfo[ulRandom].bRevealed )
				m_ulBotInfoIdx = ulRandom;
		}
	}

	// Link the bot to the player.
	m_pPlayer = &players[ulPlayerNum];
	m_pPlayer->pSkullBot = this;
	m_pPlayer->bIsBot = true;
	m_pPlayer->bSpectating = false;
	m_pPlayer->bDeadSpectator = false;

	// Update the playeringame slot.
	playeringame[ulPlayerNum] = true;

	// Setup the player's userinfo based on the bot's botinfo.
	// [BB] First clear the userinfo.
	m_pPlayer->userinfo.Reset();
	FString botname = g_BotInfo[m_ulBotInfoIdx].szName;
	V_ColorizeString( botname );
	m_pPlayer->userinfo.NameChanged ( botname );

	m_pPlayer->userinfo.ColorChanged ( V_GetColorFromString( NULL, g_BotInfo[m_ulBotInfoIdx].szColor ) );

	// Store the name of the skin the client gave us, so others can view the skin
	// even if the server doesn't have the skin loaded.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		strncpy( SERVER_GetClient( ulPlayerNum )->szSkin, g_BotInfo[m_ulBotInfoIdx].szSkinName, MAX_SKIN_NAME + 1 );

	LONG lSkin = R_FindSkin( g_BotInfo[m_ulBotInfoIdx].szSkinName, 0 );
	m_pPlayer->userinfo.SkinNumChanged ( lSkin );

	// If the skin was hidden, reveal it!
	if ( skins[lSkin].bRevealed == false )
	{
		Printf( "Hidden skin \"%s\\c-\" has now been revealed!\n", skins[lSkin].name );
		skins[lSkin].bRevealed = true;
	}

	// See if the given class name matches one in the global list.
	for ( ulIdx = 0; ulIdx < PlayerClasses.Size( ); ulIdx++ )
	{
		if ( stricmp( g_BotInfo[m_ulBotInfoIdx].szClassName, PlayerClasses[ulIdx].Type->Meta.GetMetaString (APMETA_DisplayName)) == 0 )
		{
			m_pPlayer->userinfo.PlayerClassNumChanged ( ulIdx );
			break;
		}
	}

	m_pPlayer->userinfo.RailColorChanged ( g_BotInfo[m_ulBotInfoIdx].ulRailgunColor );
	m_pPlayer->userinfo.GenderNumChanged ( D_GenderToInt( g_BotInfo[m_ulBotInfoIdx].szGender ) );
	if ( pszTeamName )
	{
		// If we're in teamgame mode, put the bot on a defined team.
		if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
		{
			ULONG ulTeam = TEAM_GetTeamNumberByName ( pszTeamName );
			if ( TEAM_CheckIfValid ( ulTeam ) )
				PLAYER_SetTeam(m_pPlayer, ulTeam, true);
			else
				PLAYER_SetTeam( m_pPlayer, TEAM_ChooseBestTeamForPlayer( ), true );
		}
	}
	else
	{
		// In certain modes, the bot NEEDS to be placed on a team, or else he will constantly
		// respawn.
		if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
			PLAYER_SetTeam( m_pPlayer, TEAM_ChooseBestTeamForPlayer( ), true );
	}

	// For now, bots always switch weapons on pickup.
	m_pPlayer->userinfo.SwitchOnPickupChanged ( 2 );
	*static_cast<FIntCVar *>(m_pPlayer->userinfo[NAME_StillBob]) = 0;
	*static_cast<FIntCVar *>(m_pPlayer->userinfo[NAME_MoveBob]) = static_cast<fixed_t>(65536.f * 0.25);

	// If we've added the bot to a single player game, enable "fake multiplayer" mode.
	if ( NETWORK_GetState( ) == NETSTATE_SINGLE )
		NETWORK_SetState( NETSTATE_SINGLE_MULTIPLAYER );

	m_pPlayer->playerstate = PST_ENTER;
	m_pPlayer->fragcount = 0;
	m_pPlayer->killcount = 0;	
	m_pPlayer->ulDeathCount = 0;
	m_pPlayer->ulTime = 0;
	PLAYER_ResetSpecialCounters ( m_pPlayer );

	// Load the bot's script. If he doesn't have a script, inform the user.
	if ( g_BotInfo[m_ulBotInfoIdx].szScriptName[0] )
	{
		ULONG		ulIdx;
		ULONG		ulIdx2;

		// This bot now has a script. Now, initialize all the script data.
		m_bHasScript = true;

		// Open the lump name specified.
		m_ScriptData.RawData = Wads.OpenLumpName( g_BotInfo[m_ulBotInfoIdx].szScriptName );
		
		m_ScriptData.lScriptPos = 0;
		m_ScriptData.bExitingState = false;
		m_ScriptData.bInOnExit = false;
		m_ScriptData.bInEvent = false;
		m_ScriptData.lNextState = -1;
		for ( ulIdx = 0; ulIdx < MAX_NUM_STATES; ulIdx++ )
		{
			m_ScriptData.StatePositions[ulIdx].lPos = -1;
			m_ScriptData.StatePositions[ulIdx].lOnEnterPos = -1;
			m_ScriptData.StatePositions[ulIdx].lMainloopPos = -1;
			m_ScriptData.StatePositions[ulIdx].lOnExitPos = -1;
		}

		m_ScriptData.lNumGlobalEvents = 0;
		for ( ulIdx = 0; ulIdx < MAX_NUM_GLOBAL_EVENTS; ulIdx++ )
		{
			m_ScriptData.GlobalEventPositions[ulIdx].lPos = -1;
			m_ScriptData.GlobalEventPositions[ulIdx].Event = (BOTEVENT_e)-1;
		}

		for ( ulIdx = 0; ulIdx < MAX_NUM_STATES; ulIdx++ )
		{
			m_ScriptData.lNumEvents[ulIdx] = 0;

			for ( ulIdx2 = 0; ulIdx2 < MAX_NUM_EVENTS; ulIdx2++ )
			{
				m_ScriptData.EventPositions[ulIdx][ulIdx2].lPos = -1;
				m_ScriptData.EventPositions[ulIdx][ulIdx2].Event = (BOTEVENT_e)-1;
			}
		}

		for ( ulIdx = 0; ulIdx < MAX_SCRIPT_VARIABLES; ulIdx++ )
		{
			m_ScriptData.alScriptVariables[ulIdx] = 0;
		}

		for ( ulIdx = 0; ulIdx < MAX_NUM_STATES; ulIdx++ )
		{
			for ( ulIdx2 = 0; ulIdx2 < MAX_STATE_VARIABLES; ulIdx2++ )
				m_ScriptData.alStateVariables[ulIdx][ulIdx2] = 0;
		}

		m_ScriptData.lCurrentStateIdx = -1;
		m_ScriptData.ulScriptDelay = 0;
		m_ScriptData.ulScriptEventDelay = 0;

		// Parse through the script to determine where the various states and events are located.
		this->GetStatePositions( );

		// Now that we've gotten the state positions, we can set the script position to either
		// the onenter or mainloop position of the spawn state.
		for ( ulIdx = 0; ulIdx < MAX_NUM_STATES; ulIdx++ )
		{
			if ( stricmp( m_ScriptData.szStateName[ulIdx], "statespawn" ) == 0 )
			{
				if ( m_ScriptData.StatePositions[ulIdx].lOnEnterPos != -1 )
					m_ScriptData.lScriptPos = m_ScriptData.StatePositions[ulIdx].lOnEnterPos;
				else
					m_ScriptData.lScriptPos = m_ScriptData.StatePositions[ulIdx].lMainloopPos;

				m_ScriptData.lCurrentStateIdx = ulIdx;
				break;
			}
		}

		if ( ulIdx == MAX_NUM_STATES )
			I_Error( "Could not find spawn state in bot %s's script!", m_pPlayer->userinfo.GetName() );

		m_ScriptData.lStackPosition = 0;
		m_ScriptData.lStringStackPosition = 0;

		for ( ulIdx = 0; ulIdx < BOTSCRIPT_STACK_SIZE; ulIdx++ )
		{
			m_ScriptData.alStack[ulIdx] = 0;
			m_ScriptData.aszStringStack[ulIdx][0] = 0;
		}
	}
	else
	{
		Printf( "%s does not have a script specified. %s will not do anything.\n", players[ulPlayerNum].userinfo.GetName(), players[ulPlayerNum].userinfo.GetGender() == GENDER_MALE ? "He" : players[ulPlayerNum].userinfo.GetGender() == GENDER_FEMALE ? "She" : "It" );
	}

	// Check and see if this bot should spawn as a spectator.
	m_pPlayer->bSpectating = PLAYER_ShouldSpawnAsSpectator( m_pPlayer );

	// [BB] If the bot is forced to spectate, make sure he is not on a team.
	if ( m_pPlayer->bSpectating )
		PLAYER_SetTeam( m_pPlayer, teams.Size( ), true );

	// [BB] If any of the spawn functions throw an exception, we
	// have to catch it here. Otherwise the constructor is not properly
	// finished and trying to delete the pointer generated by it leads
	// to a crash.
	try
	{
		// [BB] Spawn the bot at its appropriate start.
		GAMEMODE_SpawnPlayer( ulPlayerNum, true );
	}
	catch (CRecoverableError &/*error*/)
	{
		Printf("Unable to spawn bot %s.\n", players[ulPlayerNum].userinfo.GetName());
		g_bBotIsInitialized[ulPlayerNum] = true;
		m_pPlayer->bSpectating = true;
		return;
	}

	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		Printf( "%s \\c-entered the game.\n", players[ulPlayerNum].userinfo.GetName() );
	else
	{
		// Let the other players know that this bot has entered the game.
		SERVER_Printf( "%s \\c-entered the game.\n", players[ulPlayerNum].userinfo.GetName() );

		// Redo the scoreboard.
		SERVERCONSOLE_ReListPlayers( );

		// [RC] Update clients using the RCON utility.
		SERVER_RCON_UpdateInfo( SVRCU_PLAYERDATA );

		// Now send out the player's userinfo out to other players.
		SERVERCOMMANDS_SetAllPlayerUserInfo( ulPlayerNum );

		// If this player is on a team, tell all the other clients that a team has
		// been selected for him.
		if ( m_pPlayer->bOnTeam )
			SERVERCOMMANDS_SetPlayerTeam( ulPlayerNum );
	}

	g_bBotIsInitialized[ulPlayerNum] = true;

	BOTCMD_SetLastJoinedPlayer( GetPlayer( )->userinfo.GetName() );

	// Tell the bots that a new players has joined the game!
	{
		ULONG	ulIdx;

		for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			if ( playeringame[ulIdx] == false )
				continue;

			// Don't tell the bot that joined that it joined the game.
			if ( ulIdx == ulPlayerNum )
				continue;

			if ( players[ulIdx].pSkullBot )
				players[ulIdx].pSkullBot->PostEvent( BOTEVENT_PLAYER_JOINEDGAME );
		}
	}

	// If this bot spawned as a spectator, let him know.
	if ( m_pPlayer->bSpectating )
		PostEvent( BOTEVENT_SPECTATING );

	// Refresh the HUD since a new player is now here (this affects the number of players in the game).
	SCOREBOARD_RefreshHUD( );
}

//*****************************************************************************
//
CSkullBot::~CSkullBot( )
{
}

//*****************************************************************************
//
void CSkullBot::Tick( void )
{
	ticcmd_t	*cmd = &m_pPlayer->cmd;

	g_BotCycles.Clock();

	// Don't execute bot logic during demos, or if the console player is a client.
	if ( NETWORK_InClientMode() ||
		( demoplayback ))
	{
		return;
	}

	// Reset the bots keypresses.
	memset( cmd, 0, sizeof( ticcmd_t ));

	// Bot does not have a script. Nothing to do.
	if ( m_bHasScript == false )
		return;

	// Don't run their script if the game is frozen.
	if ( level.flags2 & LEVEL2_FROZEN )
		return;

	// [BB] Don't run their script if they are frozen either.
	if ( ( m_pPlayer->cheats & CF_TOTALLYFROZEN ) || ( m_pPlayer->cheats & CF_FROZEN ) )
	{
		// [BB] Don't freeze dead bots. Otherwise they can't respawn.
		if ( m_pPlayer->mo && m_pPlayer->mo->health > 0 )
			return;
	}

	// Check to see if there's any events that need to be executed.
	while ( m_lQueueHead != m_lQueueTail )
	{
		if ( gametic >= ( m_StoredEventQueue[m_lQueueHead].lTick ))
			DeleteEventFromQueue( );
		else
			break;
	}

	// Is the bot's script is currently paused?
	if ( m_ScriptData.bInEvent )
	{
		if ( m_ScriptData.ulScriptEventDelay )
		{
			m_ScriptData.ulScriptEventDelay--;

			this->EndTick( );
			return;
		}
	}
	else if ( m_ScriptData.ulScriptDelay )
	{
		m_ScriptData.ulScriptDelay--;

		this->EndTick( );
		return;
	}

	// Parse the bots's script. This executes the bot's logic.
	this->ParseScript( );

	this->EndTick( );
}

//*****************************************************************************
//
void CSkullBot::EndTick( void )
{
	if ( m_bForwardMovePersist )
		m_pPlayer->cmd.ucmd.forwardmove = static_cast<short> ( m_lForwardMove << 8 );
	if ( m_bSideMovePersist )
		m_pPlayer->cmd.ucmd.sidemove = static_cast<short> ( m_lSideMove << 8 );
	m_pPlayer->cmd.ucmd.buttons |= m_lButtons;

	g_BotCycles.Unclock();

	if ( botdebug_states && ( m_pPlayer->mo->CheckLocalView( consoleplayer )) && ( NETWORK_GetState( ) != NETSTATE_SERVER ))
		Printf( "%s: %s\n", m_pPlayer->userinfo.GetName(), m_ScriptData.szStateName[m_ScriptData.lCurrentStateIdx] );
}

//*****************************************************************************
//
void CSkullBot::PostEvent( BOTEVENT_e Event )
{
	LONG		lIdx;
	BOTSKILL_e	Skill;

	// No script to post the event to.
	if ( m_bHasScript == false )
		return;

//	if ( botdebug_showevents )
//		Printf( "%s: %s\n", GetPlayer( )->userinfo.GetName(), g_pszEventNames[Event] );

	Skill = BOTS_AdjustSkill( this, BOTINFO_GetReactionTime( m_ulBotInfoIdx ));
	switch ( Skill )
	{
	case BOTSKILL_VERYPOOR:

		AddEventToQueue( Event, gametic + 25 );
		return;
	case BOTSKILL_POOR:

		AddEventToQueue( Event, gametic + 19 );
		return;
	case BOTSKILL_LOW:

		AddEventToQueue( Event, gametic + 14 );
		return;
	case BOTSKILL_MEDIUM:

		AddEventToQueue( Event, gametic + 10 );
		return;
	case BOTSKILL_HIGH:

		AddEventToQueue( Event, gametic + 7 );
		return;
	case BOTSKILL_EXCELLENT:

		AddEventToQueue( Event, gametic + 5 );
		return;
	case BOTSKILL_SUPREME:

		AddEventToQueue( Event, gametic + 3 );
		return;
	case BOTSKILL_GODLIKE:

		AddEventToQueue( Event, gametic + 1 );
		return;
	// A bot with perfect reaction time can instantly react to events.
	case BOTSKILL_PERFECT:

		break;
	default:

		Printf( "CSkullBot::PostEvent: Unhandled bot skill\n." );
		break;
	}

	// First, scan global events.
	for ( lIdx = 0; lIdx < m_ScriptData.lNumGlobalEvents; lIdx++ )
	{
		// Found a matching event.
		if ( m_ScriptData.GlobalEventPositions[lIdx].Event == Event )
		{
			if ( m_ScriptData.bInEvent == false )
			{
				// Save the current position in the script, since we will go back to it if we do not change
				// states.
				m_ScriptData.lSavedScriptPos = m_ScriptData.lScriptPos;

				m_ScriptData.bInEvent = true;
			}

			m_ScriptData.lScriptPos = m_ScriptData.GlobalEventPositions[lIdx].lPos;
			m_ScriptData.RawData.Seek( m_ScriptData.lScriptPos, SEEK_SET );

			// Parse after the event.
			m_ScriptData.ulScriptEventDelay = 0;
			ParseScript( );
			return;
		}
	}

	// If the event wasn't found in the global events, scan state events.
	for ( lIdx = 0; lIdx < MAX_NUM_EVENTS; lIdx++ )
	{
		// Found a matching event.
		if ( m_ScriptData.EventPositions[m_ScriptData.lCurrentStateIdx][lIdx].Event == Event )
		{
			if ( m_ScriptData.bInEvent == false )
			{
				// Save the current position in the script, since we will go back to it if we do not change
				// states.
				m_ScriptData.lSavedScriptPos = m_ScriptData.lScriptPos;

				m_ScriptData.bInEvent = true;
			}

			m_ScriptData.lScriptPos = m_ScriptData.EventPositions[m_ScriptData.lCurrentStateIdx][lIdx].lPos;
			m_ScriptData.RawData.Seek( m_ScriptData.lScriptPos, SEEK_SET );

			// Parse after the event.
			m_ScriptData.ulScriptEventDelay = 0;
			ParseScript( );
			return;
		}
	}
}

//*****************************************************************************
//
void CSkullBot::ParseScript( void )
{
	bool	bStopParsing;
	SDWORD	sdwCommandHeader;
	SDWORD	sdwBuffer;
	SDWORD	sdwVariable;
	LONG	lNumOperations = 0;
//	LONG	lExpectedStackPosition;

	bStopParsing = false;
	while ( bStopParsing == false )
	{
		m_ScriptData.RawData.Seek( m_ScriptData.lScriptPos, SEEK_SET );
		m_ScriptData.RawData.Read( &sdwCommandHeader, sizeof( SDWORD ));
		m_ScriptData.lScriptPos += sizeof( SDWORD );

		if ( botdebug_dataheaders )
			Printf( "%s\n", g_pszDataHeaders[sdwCommandHeader] );

		if ( lNumOperations++ >= 8192 )
			I_Error( "ParseScript: Infinite loop detected in bot %s's script!", GetPlayer( )->userinfo.GetName() );

//		lExpectedStackPosition = m_ScriptData.lStackPosition + g_lExpectedStackChange[sdwCommandHeader];
//		g_lLastHeader = sdwCommandHeader;
		switch ( sdwCommandHeader )
		{
		// Looped back around to the beginning of the main loop.
		case DH_MAINLOOP:

			break;
		// We encountered a command.
		case DH_COMMAND:

			// Read in the command.
			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );

			if (( sdwBuffer < 0 ) || ( sdwBuffer >= NUM_BOTCMDS ))
				I_Error( "ParseScript: Unknown command %d, in state %s!", sdwBuffer, m_ScriptData.szStateName[m_ScriptData.lCurrentStateIdx] );
			else
				BOTCMD_RunCommand( (BOTCMD_e)sdwBuffer, this );

			if ( m_ScriptData.bExitingState && ( m_ScriptData.bInOnExit == false ))
			{
				// If this state has an onexit section, go to that.
				if ( m_ScriptData.StatePositions[m_ScriptData.lCurrentStateIdx].lOnExitPos != -1 )
				{
					m_ScriptData.RawData.Seek( m_ScriptData.StatePositions[m_ScriptData.lCurrentStateIdx].lOnExitPos, SEEK_SET );
					m_ScriptData.lScriptPos = m_ScriptData.StatePositions[m_ScriptData.lCurrentStateIdx].lOnExitPos;

					m_ScriptData.bInOnExit = true;
				}
				// Otherwise, move on to the next state.
				else if ( m_ScriptData.StatePositions[m_ScriptData.lNextState].lOnEnterPos != -1 )
				{
					m_ScriptData.RawData.Seek( m_ScriptData.StatePositions[m_ScriptData.lNextState].lOnEnterPos, SEEK_SET );
					m_ScriptData.lScriptPos = m_ScriptData.StatePositions[m_ScriptData.lNextState].lOnEnterPos;

					if ( botdebug_statechanges )
						Printf( "leaving %s, entering %s\n", m_ScriptData.szStateName[m_ScriptData.lCurrentStateIdx], m_ScriptData.szStateName[m_ScriptData.lNextState] );

					m_ScriptData.lCurrentStateIdx = m_ScriptData.lNextState;
					m_ScriptData.bExitingState = false;
					bStopParsing = true;
				}
				else if ( m_ScriptData.StatePositions[m_ScriptData.lNextState].lMainloopPos != -1 )
				{
					m_ScriptData.RawData.Seek( m_ScriptData.StatePositions[m_ScriptData.lNextState].lMainloopPos, SEEK_SET );
					m_ScriptData.lScriptPos = m_ScriptData.StatePositions[m_ScriptData.lNextState].lMainloopPos;

					if ( botdebug_statechanges )
						Printf( "leaving %s, entering %s\n", m_ScriptData.szStateName[m_ScriptData.lCurrentStateIdx], m_ScriptData.szStateName[m_ScriptData.lNextState] );

					m_ScriptData.lCurrentStateIdx = m_ScriptData.lNextState;
					m_ScriptData.bExitingState = false;
					bStopParsing = true;
				}
				else
					I_Error( "ParseScript: Couldn't find onenter or mainloop section in state %s!\n", m_ScriptData.szStateName[m_ScriptData.lNextState] );

				// If we're changing states, kill the script delay.
				m_ScriptData.ulScriptDelay = 0;
				m_ScriptData.ulScriptEventDelay = 0;
				m_ScriptData.bInEvent = false;
			}

			// If delay has been called, delay the script.
			if ( m_ScriptData.bInEvent )
			{
				if ( m_ScriptData.ulScriptEventDelay )
					bStopParsing = true;
			}
			else if ( m_ScriptData.ulScriptDelay )
				bStopParsing = true;

			break;
		case DH_ENDONENTER:

			// Read the next longword. It must be the start of the main loop.
			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );

			if ( sdwBuffer != DH_MAINLOOP )
				I_Error( "ParseSecton: Missing mainloop in state %s!", m_ScriptData.szStateName[m_ScriptData.lCurrentStateIdx] );

//			if ( m_ScriptData.lStackPosition != 0 )
//				I_Error( "ParseScript: Stack position not 0 at DH_ENDONENTER!" );
			break;
		case DH_ENDMAINLOOP:

			m_ScriptData.lScriptPos = m_ScriptData.StatePositions[m_ScriptData.lCurrentStateIdx].lMainloopPos;
			m_ScriptData.RawData.Seek( m_ScriptData.lScriptPos, SEEK_SET );
			bStopParsing = true;

//			if ( m_ScriptData.lStackPosition != 0 )
//				I_Error( "ParseScript: Stack position not 0 at DH_ENDMAINLOOP!" );
			break;
		case DH_ONEXIT:

			bStopParsing = true;
			break;
		case DH_ENDONEXIT:

			// Otherwise, move on to the next state.
			if ( m_ScriptData.StatePositions[m_ScriptData.lNextState].lOnEnterPos != -1 )
			{
				m_ScriptData.RawData.Seek( m_ScriptData.StatePositions[m_ScriptData.lNextState].lOnEnterPos, SEEK_SET );
				m_ScriptData.lScriptPos = m_ScriptData.StatePositions[m_ScriptData.lNextState].lOnEnterPos;
			}
			else if ( m_ScriptData.StatePositions[m_ScriptData.lNextState].lMainloopPos != -1 )
			{
				m_ScriptData.RawData.Seek( m_ScriptData.StatePositions[m_ScriptData.lNextState].lMainloopPos, SEEK_SET );
				m_ScriptData.lScriptPos = m_ScriptData.StatePositions[m_ScriptData.lNextState].lMainloopPos;
			}
			else
				I_Error( "ParseScript: Couldn't find onenter or mainloop section in state %s!\n", m_ScriptData.szStateName[m_ScriptData.lNextState] );

			if ( botdebug_statechanges )
				Printf( "leaving %s, entering %s\n", m_ScriptData.szStateName[m_ScriptData.lCurrentStateIdx], m_ScriptData.szStateName[m_ScriptData.lNextState] );

			m_ScriptData.lCurrentStateIdx = m_ScriptData.lNextState;
			m_ScriptData.bInOnExit = false;
			m_ScriptData.bExitingState = false;
			bStopParsing = true;

//			if ( m_ScriptData.lStackPosition != 0 )
//				I_Error( "ParseScript: Stack position not 0 at DH_ENDONEXIT!" );
			break;
		case DH_ENDEVENT:

			// If a changestate command wasn't called, simply go back to the main loop.
			m_ScriptData.lScriptPos = m_ScriptData.lSavedScriptPos;
			m_ScriptData.RawData.Seek( m_ScriptData.lScriptPos, SEEK_SET );

			bStopParsing = true;
			m_ScriptData.bInEvent = false;

//			if ( m_ScriptData.lStackPosition != 0 )
//				I_Error( "ParseScript: Stack position not 0 at DH_ENDEVENT!" );

			break;
		case DH_PUSHNUMBER:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );

			PushToStack( sdwBuffer );
			break;
		case DH_PUSHSTRINGINDEX:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );

			PushToStringStack( m_ScriptData.szStringList[sdwBuffer] );
			break;
		case DH_PUSHGLOBALVAR:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );

			PushToStack( m_ScriptData.alScriptVariables[sdwBuffer] );
			break;
		case DH_PUSHLOCALVAR:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );

			PushToStack( m_ScriptData.alStateVariables[m_ScriptData.lCurrentStateIdx][sdwBuffer] );
			break;
		case DH_IFGOTO:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );

			if ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] )
			{
				m_ScriptData.lScriptPos = sdwBuffer;
				m_ScriptData.RawData.Seek( m_ScriptData.lScriptPos, SEEK_SET );
			}
			PopStack( );
			break;
		case DH_IFNOTGOTO:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );

			if ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] == 0 )
			{
				m_ScriptData.lScriptPos = sdwBuffer;
				m_ScriptData.RawData.Seek( m_ScriptData.lScriptPos, SEEK_SET );
			}
			PopStack( );
			break;
		case DH_GOTO:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );

			m_ScriptData.lScriptPos = sdwBuffer;
			m_ScriptData.RawData.Seek( m_ScriptData.lScriptPos, SEEK_SET );
			break;
		case DH_DROPSTACKPOSITION:

			PopStack( );
			break;
		case DH_ORLOGICAL:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] || m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] );
			PopStack( );
			break;
		case DH_ANDLOGICAL:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] && m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] );
			PopStack( );
			break;
		case DH_ORBITWISE:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] | m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] );
			PopStack( );
			break;
		case DH_EORBITWISE:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] ^ m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] );
			PopStack( );
			break;
		case DH_ANDBITWISE:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] & m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] );
			PopStack( );
			break;
		case DH_EQUALS:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] == m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] );
			PopStack( );
			break;
		case DH_NOTEQUALS:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] != m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] );
			PopStack( );
			break;
		case DH_LESSTHAN:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] < m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] );
			PopStack( );
			break;
		case DH_LESSTHANEQUALS:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] <= m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] );
			PopStack( );
			break;
		case DH_GREATERTHAN:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] > m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] );
			PopStack( );
			break;
		case DH_GREATERTHANEQUALS:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] >= m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] );
			PopStack( );
			break;
		case DH_NEGATELOGICAL:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] = !m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			break;
		case DH_LSHIFT:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] << m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] );
			PopStack( );
			break;
		case DH_RSHIFT:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] >> m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] );
			PopStack( );
			break;
		case DH_ADD:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] + m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] );
			PopStack( );
			break;
		case DH_SUBTRACT:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] - m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] );
			PopStack( );
			break;
		case DH_UNARYMINUS:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] = -m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			break;
		case DH_MULTIPLY:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] * m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] );
			PopStack( );
			break;
		case DH_DIVIDE:

			if ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] == 0 )
				I_Error( "ParseScript: Illegal divide by 0 in bot %s's script!", m_pPlayer->userinfo.GetName() );

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] / m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] );
			PopStack( );
			break;
		case DH_MODULUS:

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] % m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] );
			PopStack( );
			break;
		case DH_SCRIPTVARLIST:

			// This doesn't do much now.
			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );
			break;
		case DH_INCGLOBALVAR:

			m_ScriptData.RawData.Read( &sdwVariable, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );

			m_ScriptData.alScriptVariables[sdwVariable]++;
			break;
		case DH_DECGLOBALVAR:

			m_ScriptData.RawData.Read( &sdwVariable, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );

			m_ScriptData.alScriptVariables[sdwVariable]--;
			break;
		case DH_ASSIGNGLOBALVAR:

			m_ScriptData.RawData.Read( &sdwVariable, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );

			m_ScriptData.alScriptVariables[sdwVariable] = m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			PopStack( );
			break;
		case DH_ADDGLOBALVAR:

			m_ScriptData.RawData.Read( &sdwVariable, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );

			m_ScriptData.alScriptVariables[sdwVariable] += m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			PopStack( );
			break;
		case DH_SUBGLOBALVAR:

			m_ScriptData.RawData.Read( &sdwVariable, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );

			m_ScriptData.alScriptVariables[sdwVariable] -= m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			PopStack( );
			break;
		case DH_MULGLOBALVAR:

			m_ScriptData.RawData.Read( &sdwVariable, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );

			m_ScriptData.alScriptVariables[sdwVariable] *= m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			PopStack( );
			break;
		case DH_DIVGLOBALVAR:

			m_ScriptData.RawData.Read( &sdwVariable, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );

			if ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] == 0 )
				I_Error( "ParseScript: Illegal divide by 0 occured while trying to divide global variable in bot %s's script!!", m_pPlayer->userinfo.GetName() );

			m_ScriptData.alScriptVariables[sdwVariable] /= m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			PopStack( );
			break;
		case DH_MODGLOBALVAR:

			m_ScriptData.RawData.Read( &sdwVariable, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );

			m_ScriptData.alScriptVariables[sdwVariable] %= m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			PopStack( );
			break;
		case DH_INCLOCALVAR:

			m_ScriptData.RawData.Read( &sdwVariable, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );

			m_ScriptData.alStateVariables[m_ScriptData.lCurrentStateIdx][sdwVariable]++;
			break;
		case DH_DECLOCALVAR:

			m_ScriptData.RawData.Read( &sdwVariable, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );

			m_ScriptData.alStateVariables[m_ScriptData.lCurrentStateIdx][sdwVariable]--;
			break;
		case DH_ASSIGNLOCALVAR:

			m_ScriptData.RawData.Read( &sdwVariable, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );

			m_ScriptData.alStateVariables[m_ScriptData.lCurrentStateIdx][sdwVariable] = m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			PopStack( );
			break;
		case DH_ADDLOCALVAR:

			m_ScriptData.RawData.Read( &sdwVariable, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );

			m_ScriptData.alStateVariables[m_ScriptData.lCurrentStateIdx][sdwVariable] += m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			PopStack( );
			break;
		case DH_SUBLOCALVAR:

			m_ScriptData.RawData.Read( &sdwVariable, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );

			m_ScriptData.alStateVariables[m_ScriptData.lCurrentStateIdx][sdwVariable] -= m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			PopStack( );
			break;
		case DH_MULLOCALVAR:

			m_ScriptData.RawData.Read( &sdwVariable, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );

			m_ScriptData.alStateVariables[m_ScriptData.lCurrentStateIdx][sdwVariable] *= m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			PopStack( );
			break;
		case DH_DIVLOCALVAR:

			m_ScriptData.RawData.Read( &sdwVariable, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );

			if ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] == 0 )
				I_Error( "ParseScript: Illegal divide by 0 occured while trying to divide local variable in bot %s's script!", m_pPlayer->userinfo.GetName() );

			m_ScriptData.alStateVariables[m_ScriptData.lCurrentStateIdx][sdwVariable] /= m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			PopStack( );
			break;
		case DH_MODLOCALVAR:

			m_ScriptData.RawData.Read( &sdwVariable, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );

			m_ScriptData.alStateVariables[m_ScriptData.lCurrentStateIdx][sdwVariable] %= m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			PopStack( );
			break;
		case DH_CASEGOTO:

			m_ScriptData.RawData.Read( &sdwVariable, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );

			if ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] == sdwVariable )
			{
				m_ScriptData.lScriptPos = sdwBuffer;
				m_ScriptData.RawData.Seek( m_ScriptData.lScriptPos, SEEK_SET );

				PopStack( );
			}
			break;
		case DH_DROP:

			PopStack( );
			break;
		case DH_INCGLOBALARRAY:

			{
				LONG	lIdx;

				lIdx = m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];

				m_ScriptData.RawData.Read( &sdwVariable, sizeof( SDWORD ));
				m_ScriptData.lScriptPos += sizeof( SDWORD );

				m_ScriptData.alScriptArrays[sdwVariable][lIdx]++;
				PopStack( );
			}
			break;
		case DH_DECGLOBALARRAY:

			{
				LONG	lIdx;

				lIdx = m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];

				m_ScriptData.RawData.Read( &sdwVariable, sizeof( SDWORD ));
				m_ScriptData.lScriptPos += sizeof( SDWORD );

				m_ScriptData.alScriptArrays[sdwVariable][lIdx]--;
				PopStack( );
			}
			break;
		case DH_ASSIGNGLOBALARRAY:

			m_ScriptData.RawData.Read( &sdwVariable, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );

			m_ScriptData.alScriptArrays[sdwVariable][m_ScriptData.alStack[m_ScriptData.lStackPosition - 2]] = m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			PopStack( );
			PopStack( );
			break;
		case DH_ADDGLOBALARRAY:

			m_ScriptData.RawData.Read( &sdwVariable, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );

			m_ScriptData.alScriptArrays[sdwVariable][m_ScriptData.alStack[m_ScriptData.lStackPosition - 2]] += m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			PopStack( );
			PopStack( );
			break;
		case DH_SUBGLOBALARRAY:

			m_ScriptData.RawData.Read( &sdwVariable, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );

			m_ScriptData.alScriptArrays[sdwVariable][m_ScriptData.alStack[m_ScriptData.lStackPosition - 2]] -= m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			PopStack( );
			PopStack( );
			break;
		case DH_MULGLOBALARRAY:

			m_ScriptData.RawData.Read( &sdwVariable, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );

			m_ScriptData.alScriptArrays[sdwVariable][m_ScriptData.alStack[m_ScriptData.lStackPosition - 2]] *= m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			PopStack( );
			PopStack( );
			break;
		case DH_DIVGLOBALARRAY:

			m_ScriptData.RawData.Read( &sdwVariable, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );

			if ( m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] == 0 )
				I_Error( "ParseScript: Illegal divide by 0 occured while trying to divide array in bot %s's script!", m_pPlayer->userinfo.GetName() );

			m_ScriptData.alScriptArrays[sdwVariable][m_ScriptData.alStack[m_ScriptData.lStackPosition - 2]] /= m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			PopStack( );
			PopStack( );
			break;
		case DH_MODGLOBALARRAY:

			m_ScriptData.RawData.Read( &sdwVariable, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );

			m_ScriptData.alScriptArrays[sdwVariable][m_ScriptData.alStack[m_ScriptData.lStackPosition - 2]] %= m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			PopStack( );
			PopStack( );
			break;
		case DH_PUSHGLOBALARRAY:

			m_ScriptData.RawData.Read( &sdwVariable, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );

			m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] = m_ScriptData.alScriptArrays[sdwVariable][m_ScriptData.alStack[m_ScriptData.lStackPosition - 1]];
			break;
		case DH_SWAP:

			{
				LONG	lTemp;

				lTemp = m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
				m_ScriptData.alStack[m_ScriptData.lStackPosition - 1] = m_ScriptData.alStack[m_ScriptData.lStackPosition - 2];
				m_ScriptData.alStack[m_ScriptData.lStackPosition - 2] = lTemp;
			}
			break;
		case DH_DUP:

			m_ScriptData.alStack[m_ScriptData.lStackPosition] = m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];
			m_ScriptData.lStackPosition++;

			if ( botdebug_showstackpushes )
				Printf( "PushToStack: DH_DUP\n" );

			break;
		case DH_ARRAYSET:
			
			{
				LONG	lArray;
				LONG	lVal;
				LONG	lHighestVal;
//				LONG	lIdx;

				lArray = m_ScriptData.alStack[m_ScriptData.lStackPosition - 3];
				lVal = m_ScriptData.alStack[m_ScriptData.lStackPosition - 2];
				lHighestVal = m_ScriptData.alStack[m_ScriptData.lStackPosition - 1];

				if (( lArray < 0 ) || ( lArray >= MAX_SCRIPT_ARRAYS ))
					I_Error( "ParseScript: Invalid array index, %d, in command \"arrayset\"!", static_cast<int> (lArray) );
				if (( lHighestVal < 0 ) || ( lHighestVal >= MAX_SCRIPTARRAY_SIZE ))
					I_Error( "ParseScript: Invalid array maximum index, %d, in command \"arrayset\"!", static_cast<int> (lHighestVal) );

				memset( m_ScriptData.alScriptArrays[lArray], lVal, lHighestVal * sizeof( LONG ));
				PopStack( );
				PopStack( );
				PopStack( );
			}
			break;
		default:

			{
				char	szCommandHeader[32];

				itoa( sdwCommandHeader, szCommandHeader, 10 );
				I_Error( "ParseScript: Invalid command, %s, in state %s!", sdwCommandHeader < NUM_DATAHEADERS ? g_pszDataHeaders[sdwCommandHeader] : szCommandHeader, m_ScriptData.szStateName[m_ScriptData.lCurrentStateIdx] );
			}
			break;
		}
/*
		if (( lExpectedStackPosition != m_ScriptData.lStackPosition ) &&
			( sdwCommandHeader != DH_COMMAND ) &&
			( sdwCommandHeader != DH_CASEGOTO ))
		{
			I_Error( "ParseScript: Something's screwey about %s!", g_pszDataHeaders[sdwCommandHeader] );
		}
*/
	}
}

//*****************************************************************************
//
void CSkullBot::GetStatePositions( void )
{
	bool		bStopParsing;
	SDWORD		sdwCommandHeader;
	SDWORD		sdwLastCommandHeader;
	SDWORD		sdwBuffer;

	bStopParsing = false;
	sdwCommandHeader = -1;
	while ( bStopParsing == false )
	{
		sdwLastCommandHeader = sdwCommandHeader;

		// Hit the end of the file.
		m_ScriptData.RawData.Seek( m_ScriptData.lScriptPos, SEEK_SET );
		if ( m_ScriptData.RawData.Read( &sdwCommandHeader, sizeof( SDWORD )) < static_cast<LONG>(sizeof( SDWORD )))
			return;
		m_ScriptData.lScriptPos += sizeof( SDWORD );

		switch ( sdwCommandHeader )
		{
		case DH_COMMAND:

			// Read in the command.
			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );

			if (( sdwBuffer < 0 ) || ( sdwBuffer >= NUM_BOTCMDS ))
				I_Error( "GetStatePositions: Unknown command %d, in state %s!", sdwBuffer, m_ScriptData.szStateName[m_ScriptData.lCurrentStateIdx] );

			// Read in the argument list.
			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );
			break;
		case DH_ONENTER:

			if ( m_ScriptData.lCurrentStateIdx == -1 )
				I_Error( "GetStatePositions: Found onenter without state definition!" );

			m_ScriptData.StatePositions[m_ScriptData.lCurrentStateIdx].lOnEnterPos = m_ScriptData.lScriptPos;
			break;
		case DH_MAINLOOP:

			if ( m_ScriptData.lCurrentStateIdx == -1 )
				I_Error( "GetStatePositions: Found mainloop without state definition!" );

			m_ScriptData.StatePositions[m_ScriptData.lCurrentStateIdx].lMainloopPos = m_ScriptData.lScriptPos;
			break;
		case DH_ONEXIT:

			if ( m_ScriptData.lCurrentStateIdx == -1 )
				I_Error( "GetStatePositions: Found onexit without state definition!" );

			m_ScriptData.StatePositions[m_ScriptData.lCurrentStateIdx].lOnExitPos = m_ScriptData.lScriptPos;
			break;
		case DH_EVENT:

			// Read in the event.
			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );

			if ( m_ScriptData.lCurrentStateIdx == -1 )
			{
				if ( m_ScriptData.lNumGlobalEvents == MAX_NUM_GLOBAL_EVENTS )
					I_Error( "GetStatePositions: Too many global events in bot %s's script!", GetPlayer( )->userinfo.GetName() );

				m_ScriptData.GlobalEventPositions[m_ScriptData.lNumGlobalEvents].lPos = m_ScriptData.lScriptPos;
				m_ScriptData.GlobalEventPositions[m_ScriptData.lNumGlobalEvents].Event = (BOTEVENT_e)sdwBuffer;
				m_ScriptData.lNumGlobalEvents++;
			}
			else
			{
				if ( m_ScriptData.lNumEvents[m_ScriptData.lCurrentStateIdx] == MAX_NUM_EVENTS )
					I_Error( "GetStatePositions: Too many events in bot %s's state, %s!", GetPlayer( )->userinfo.GetName(), m_ScriptData.szStateName[m_ScriptData.lCurrentStateIdx] );

				m_ScriptData.EventPositions[m_ScriptData.lCurrentStateIdx][m_ScriptData.lNumEvents[m_ScriptData.lCurrentStateIdx]].lPos = m_ScriptData.lScriptPos;
				m_ScriptData.EventPositions[m_ScriptData.lCurrentStateIdx][m_ScriptData.lNumEvents[m_ScriptData.lCurrentStateIdx]].Event = (BOTEVENT_e)sdwBuffer;
				m_ScriptData.lNumEvents[m_ScriptData.lCurrentStateIdx]++;
			}
			break;
		case DH_ENDONENTER:

			break;
		case DH_ENDMAINLOOP:

			break;
		case DH_ENDONEXIT:

			break;
		case DH_ENDEVENT:

			break;
		case DH_STATENAME:

			{
				SDWORD	sdwNameLength;
				char	szStateName[256];

				// Read in the string length of the script name.
				m_ScriptData.RawData.Read( &sdwNameLength, sizeof( SDWORD ));
				m_ScriptData.lScriptPos += sizeof( SDWORD );

				// Now, read in the string name.
				m_ScriptData.RawData.Read( szStateName, sdwNameLength );
				m_ScriptData.lScriptPos += sdwNameLength;

				szStateName[sdwNameLength] = 0;

				// Read in the string length of the script name.
				m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
				m_ScriptData.lScriptPos += sizeof( SDWORD );

				if ( sdwBuffer != DH_STATEIDX )
					I_Error( "GetStatePositions: Expected state index after state, %s!", szStateName );

				SDWORD sdwTemp;
				m_ScriptData.RawData.Read( &sdwTemp, sizeof( SDWORD ));
				m_ScriptData.lCurrentStateIdx = sdwTemp;
				m_ScriptData.lScriptPos += sizeof( SDWORD );

				m_ScriptData.StatePositions[m_ScriptData.lCurrentStateIdx].lPos = m_ScriptData.lScriptPos;
				sprintf( m_ScriptData.szStateName[m_ScriptData.lCurrentStateIdx], "%s", szStateName );
			}

			// All done.
			break;
		case DH_STATEIDX:

			{
				SDWORD sdwTemp;
				m_ScriptData.RawData.Read( &sdwTemp, sizeof( SDWORD ));
				m_ScriptData.lCurrentStateIdx = sdwTemp;
				m_ScriptData.lScriptPos += sizeof( SDWORD );

				m_ScriptData.StatePositions[m_ScriptData.lCurrentStateIdx].lPos = m_ScriptData.lScriptPos;
			}

			break;
		case DH_PUSHNUMBER:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );
			break;
		case DH_PUSHSTRINGINDEX:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );
			break;
		case DH_PUSHGLOBALVAR:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );
			break;
		case DH_PUSHLOCALVAR:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );
			break;
		case DH_IFGOTO:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );
			break;
		case DH_IFNOTGOTO:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );
			break;
		case DH_GOTO:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );
			break;
		case DH_ORLOGICAL:
		case DH_ANDLOGICAL:
		case DH_ORBITWISE:
		case DH_EORBITWISE:
		case DH_ANDBITWISE:
		case DH_EQUALS:
		case DH_NOTEQUALS:
		case DH_DROPSTACKPOSITION:
		case DH_LESSTHAN:
		case DH_LESSTHANEQUALS:
		case DH_GREATERTHAN:
		case DH_GREATERTHANEQUALS:
		case DH_NEGATELOGICAL:
		case DH_LSHIFT:
		case DH_RSHIFT:
		case DH_ADD:
		case DH_SUBTRACT:
		case DH_UNARYMINUS:
		case DH_MULTIPLY:
		case DH_DIVIDE:
		case DH_MODULUS:

			break;
		case DH_SCRIPTVARLIST:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );
			break;
		case DH_STRINGLIST:

			{
				char	szString[MAX_STRING_LENGTH];
				SDWORD	sdwNumStrings;
				ULONG	ulIdx;

				m_ScriptData.RawData.Read( &sdwNumStrings, sizeof( SDWORD ));
				m_ScriptData.lScriptPos += sizeof( SDWORD );

				for ( ulIdx = 0; ulIdx < (ULONG)sdwNumStrings; ulIdx++ )
				{
					// This is the string length.
					m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
					m_ScriptData.lScriptPos += sizeof( SDWORD );

					m_ScriptData.RawData.Read( &szString, sdwBuffer );
					m_ScriptData.lScriptPos += sdwBuffer;

					szString[sdwBuffer] = 0;
					sprintf( m_ScriptData.szStringList[ulIdx], "%s", szString );
				}
			}
			break;
		case DH_INCGLOBALVAR:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );
			break;
		case DH_DECGLOBALVAR:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );
			break;
		case DH_ASSIGNGLOBALVAR:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );
			break;
		case DH_ADDGLOBALVAR:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );
			break;
		case DH_SUBGLOBALVAR:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );
			break;
		case DH_MULGLOBALVAR:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );
			break;
		case DH_DIVGLOBALVAR:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );
			break;
		case DH_MODGLOBALVAR:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );
			break;
		case DH_INCLOCALVAR:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );
			break;
		case DH_DECLOCALVAR:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );
			break;
		case DH_ASSIGNLOCALVAR:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );
			break;
		case DH_ADDLOCALVAR:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );
			break;
		case DH_SUBLOCALVAR:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );
			break;
		case DH_MULLOCALVAR:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );
			break;
		case DH_DIVLOCALVAR:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );
			break;
		case DH_MODLOCALVAR:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );
			break;
		case DH_CASEGOTO:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );
			break;
		case DH_DROP:

			break;
		case DH_INCGLOBALARRAY:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );
			break;
		case DH_DECGLOBALARRAY:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );
			break;
		case DH_ASSIGNGLOBALARRAY:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );
			break;
		case DH_ADDGLOBALARRAY:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );
			break;
		case DH_SUBGLOBALARRAY:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );
			break;
		case DH_MULGLOBALARRAY:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );
			break;
		case DH_DIVGLOBALARRAY:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );
			break;
		case DH_MODGLOBALARRAY:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );
			break;
		case DH_PUSHGLOBALARRAY:

			m_ScriptData.RawData.Read( &sdwBuffer, sizeof( SDWORD ));
			m_ScriptData.lScriptPos += sizeof( SDWORD );
			break;
		case DH_SWAP:

			break;
		case DH_ARRAYSET:

			break;
		default:

			I_Error( "GetStatePositions: Unknown header, %d in bot %s's script at position, %d! (Last known header: %d)", sdwCommandHeader, m_pPlayer->userinfo.GetName(), static_cast<int> (m_ScriptData.lScriptPos - sizeof( SDWORD )), sdwLastCommandHeader );
			break;
		}
	}
}

//*****************************************************************************
//
ULONG CSkullBot::GetScriptDelay( void )
{
	return ( m_ScriptData.ulScriptDelay );
}

//*****************************************************************************
//
void CSkullBot::SetScriptDelay( ULONG ulDelay )
{
	if ( ulDelay > 0 )
		m_ScriptData.ulScriptDelay = ulDelay;
}

//*****************************************************************************
//
ULONG CSkullBot::GetScriptEventDelay( void )
{
	return ( m_ScriptData.ulScriptEventDelay );
}

//*****************************************************************************
//
void CSkullBot::SetScriptEventDelay( ULONG ulDelay )
{
	if ( ulDelay > 0 )
		m_ScriptData.ulScriptEventDelay = ulDelay;
}

//*****************************************************************************
//
player_t *CSkullBot::GetPlayer( void )
{
	return ( m_pPlayer );
}

//*****************************************************************************
//
FWadLump *CSkullBot::GetRawScriptData( void )
{
	return ( &m_ScriptData.RawData );
}

//*****************************************************************************
//
LONG CSkullBot::GetScriptPosition( void )
{
	return ( m_ScriptData.lScriptPos );
}

//*****************************************************************************
//
void CSkullBot::SetScriptPosition( LONG lPosition )
{
	m_ScriptData.lScriptPos = lPosition;
}

//*****************************************************************************
//
void CSkullBot::IncrementScriptPosition( LONG lUnits )
{
	m_ScriptData.lScriptPos += lUnits;
}

//*****************************************************************************
//
void CSkullBot::SetExitingState( bool bExit )
{
	m_ScriptData.bExitingState = bExit;
}

//*****************************************************************************
//
void CSkullBot::SetNextState( LONG lNextState )
{
	m_ScriptData.lNextState = lNextState;
}

//*****************************************************************************
//
void CSkullBot::PushToStack( LONG lValue )
{
	m_ScriptData.alStack[m_ScriptData.lStackPosition++] = lValue;
	
	if ( botdebug_showstackpushes )
		Printf( "%s: PushToStack: pos %d val %d\n", m_pPlayer->userinfo.GetName(), static_cast<int> (m_ScriptData.lStackPosition), static_cast<int> (lValue) );

	if ( m_ScriptData.lStackPosition >= BOTSCRIPT_STACK_SIZE )
		I_Error( "PushToStack: Stack size exceeded in bot %s's script!", m_pPlayer->userinfo.GetName() );
}

//*****************************************************************************
//
void CSkullBot::PopStack( void )
{
	m_ScriptData.lStackPosition--;
	
	if ( botdebug_showstackpushes )
		Printf( "%s: PopStack: %d\n", m_pPlayer->userinfo.GetName(), static_cast<int> (m_ScriptData.lStackPosition) );

	if ( m_ScriptData.lStackPosition < 0 )
		I_Error( "PopStack: Bot stack position went below 0 in bot %s's script!", m_pPlayer->userinfo.GetName() );
}

//*****************************************************************************
//
void CSkullBot::PushToStringStack( char *pszString )
{
	sprintf( m_ScriptData.aszStringStack[m_ScriptData.lStringStackPosition++], "%s", pszString );
	
	if ( botdebug_showstackpushes )
		Printf( "PushToStringStack: %d\n", static_cast<int> (m_ScriptData.lStringStackPosition) );

	if ( m_ScriptData.lStringStackPosition >= BOTSCRIPT_STACK_SIZE )
		I_Error( "PushToStringStack: Stack size exceeded!" );
}

//*****************************************************************************
//
void CSkullBot::PopStringStack( void )
{
	m_ScriptData.lStringStackPosition--;
	
	if ( botdebug_showstackpushes )
		Printf( "PopStringStack: %d\n", static_cast<int> (m_ScriptData.lStackPosition) );

	if ( m_ScriptData.lStringStackPosition < 0 )
		I_Error( "PopStringStack: Bot stack position went below 0!" );
}

//*****************************************************************************
//
void CSkullBot::PreDelete( void )
{
	// If this player is the displayplayer, revert the camera back to the console player's eyes.
	if ( m_pPlayer->mo && ( m_pPlayer->mo->CheckLocalView( consoleplayer )) && ( NETWORK_GetState( ) != NETSTATE_SERVER ))
	{
		players[consoleplayer].camera = players[consoleplayer].mo;
		S_UpdateSounds( players[consoleplayer].camera );
		StatusBar->AttachToPlayer( &players[consoleplayer] );
	}

	// Remove the bot from the game.
	playeringame[( m_pPlayer - players )] = false;

	// Delete the actor attached to the player.
	if ( m_pPlayer->mo )
		m_pPlayer->mo->Destroy( );

	// Finally, fix some pointers.
	// [BB] We have to delete the CSkullBot pointer before setting it to NULL.
	//m_pPlayer->pSkullBot = NULL;
	m_pPlayer->mo = NULL;
	m_pPlayer = NULL;
}

//*****************************************************************************
//
void CSkullBot::HandleAiming( void )
{
	if (( m_bAimAtEnemy ) && ( m_ulPlayerEnemy != MAXPLAYERS ) && ( players[m_ulPlayerEnemy].mo ))
	{
		fixed_t	Distance;
		fixed_t	ShootZ;
		SDWORD	lTopPitch;
		SDWORD	lBottomPitch;
		POS_t	EnemyPos;

		// Get the current enemy position.
		EnemyPos = this->GetEnemyPosition( );
//		Printf( "Position delta:\nX: %d\nY: %d\nZ: %d\n",
//			( EnemyPos.x - players[m_ulPlayerEnemy].mo->x ) / FRACUNIT, 
//			( EnemyPos.y - players[m_ulPlayerEnemy].mo->y ) / FRACUNIT, 
//			( EnemyPos.z - players[m_ulPlayerEnemy].mo->z ) / FRACUNIT );

		m_pPlayer->mo->angle = R_PointToAngle2( m_pPlayer->mo->x,
											m_pPlayer->mo->y, 
											EnemyPos.x,
											EnemyPos.y );

		m_pPlayer->mo->angle += m_AngleDesired;
		m_AngleOffBy -= m_AngleDelta;
		if ( m_bTurnLeft )
			m_pPlayer->mo->angle -= m_AngleOffBy;
		else
			m_pPlayer->mo->angle += m_AngleOffBy;

		ShootZ = m_pPlayer->mo->z - m_pPlayer->mo->floorclip + ( m_pPlayer->mo->height >> 1 ) + ( 8 * FRACUNIT );
		Distance = P_AproxDistance( m_pPlayer->mo->x - EnemyPos.x, m_pPlayer->mo->y - EnemyPos.y );
//		m_pPlayer->mo->pitch = R_PointToAngle( Distance, ( EnemyPos.z + ( players[m_ulPlayerEnemy].mo->height / 2 )) - m_pPlayer->mo->z );
		lTopPitch = -(SDWORD)R_PointToAngle2( 0, ShootZ, Distance, EnemyPos.z + players[m_ulPlayerEnemy].mo->height );
		lBottomPitch = -(SDWORD)R_PointToAngle2( 0, ShootZ, Distance, EnemyPos.z );

		m_pPlayer->mo->pitch = ( lTopPitch / 2 ) + ( lBottomPitch / 2 );
/*
		{
			angle_t	Angle;
			angle_t	AngleDifference;

			Angle = R_PointToAngle2( m_pPlayer->mo->x,
												m_pPlayer->mo->y, 
												EnemyPos.x,
												EnemyPos.y );

			if ( Angle > m_pPlayer->mo->angle )
				AngleDifference = Angle - m_pPlayer->mo->angle;
			else
				AngleDifference = m_pPlayer->mo->angle - Angle;

			Printf( "Now offby: " );
			// AngleDistance is the absolute value of the difference in player's angle, and
			// the angle pointint to his enemy. WE DO NOT KNOW IF THE DIFFERENCE IS TO THE
			// LEFT OR RIGHT!
			if ( AngleDifference > ANGLE_180 )
			{
				Printf( "-" );
				AngleDifference = ( ANGLE_MAX - AngleDifference ) + 1;	// Add 1 because ANGLE_MAX is 1 short of 360
			}

			Printf( "%2.2f\n", (float)AngleDifference / ANGLE_1 );
			Printf( "Player's angle: %2.2f\n", (float)m_pPlayer->mo->angle / ANGLE_1 );
		}
*/
//		if (( m_ulPlayerEnemy != MAXPLAYERS ) && players[m_ulPlayerEnemy].mo )
		{
			// Still waiting to reaim at the enemy.
			if ( m_ulAimAtEnemyDelay == 0 )
			{
				angle_t		Angle;
				angle_t		AngleDifference;
				angle_t		AngleDesired;
				angle_t		AngleRange = 0;
				angle_t		AngleRandom;
				angle_t		AngleFinal;

				// Get the exact angle between us and the enemy.
				Angle = R_PointToAngle2( m_pPlayer->mo->x,
										  m_pPlayer->mo->y, 
										  EnemyPos.x,
										  EnemyPos.y );

				// The greater the difference between the angle between ourselves and the enemy, and our
				// current angle, the more inaccurate it is likely to be.
				if ( Angle > m_pPlayer->mo->angle )
					AngleDifference = Angle - m_pPlayer->mo->angle;
				else
					AngleDifference = m_pPlayer->mo->angle - Angle;

				// AngleDistance is the absolute value of the difference in player's angle, and
				// the angle pointint to his enemy. WE DO NOT KNOW IF THE DIFFERENCE IS TO THE
				// LEFT OR RIGHT!
				if ( AngleDifference > ANGLE_180 )
					AngleDifference = ( ANGLE_MAX - AngleDifference ) + 1;	// Add 1 because ANGLE_MAX is 1 short of 360
/*
				Printf( "Angle to enemy: %2.2f\n", (float)( (float)Angle / ANGLE_1 ));
				Printf( "Current angle: %2.2f\n", (float)( (float)m_pPlayer->mo->angle / ANGLE_1 ));
				Printf( "Initially off by (absolute value): %2.2f\n", (float)( (float)AngleDifference / ANGLE_1 ));
*/
				// Select some random angle between 0 and 360 degrees. 
				AngleRandom = ANGLE_1 * g_RandomBotAimSeed( 360 );

				// Select the range of possible angles and how long it's going to be before
				// reaiming, based on the bot's accuracy skill level.
				switch ( BOTS_AdjustSkill( this, BOTINFO_GetAccuracy( m_ulBotInfoIdx )))
				{
				case BOTSKILL_VERYPOOR:

					AngleRange = ANGLE_1 * 90;
					m_ulAimAtEnemyDelay = 17;
					break;
				case BOTSKILL_POOR:

					AngleRange = ANGLE_1 * 75;
					m_ulAimAtEnemyDelay = 12;
					break;
				case BOTSKILL_LOW:

					AngleRange = ANGLE_1 * 60;
					m_ulAimAtEnemyDelay = 8;
					break;
				case BOTSKILL_MEDIUM:

					AngleRange = ANGLE_1 * 45;
					m_ulAimAtEnemyDelay = 6;
					break;
				case BOTSKILL_HIGH:

					AngleRange = ANGLE_1 * 35;
					m_ulAimAtEnemyDelay = 4;
					break;
				case BOTSKILL_EXCELLENT:

					AngleRange = ANGLE_1 * 25;
					m_ulAimAtEnemyDelay = 3;
					break;
				case BOTSKILL_SUPREME:

					AngleRange = ANGLE_1 * 15;
					m_ulAimAtEnemyDelay = 2;
					break;
				case BOTSKILL_GODLIKE:

					AngleRange = ANGLE_1 * 10;
					m_ulAimAtEnemyDelay = 1;
					break;
				case BOTSKILL_PERFECT:

					// Accuracy is perfect.
					m_pPlayer->mo->angle = Angle;
					m_ulAimAtEnemyDelay = 0;

					// Just return: nothing else to do.
					return;
				default:

					I_Error( "botcmd_BeginAimingAtEnemy: Unknown botskill level, %d!", BOTINFO_GetAccuracy( m_ulBotInfoIdx ));
					break;
				}

				// Now, get the desired angle based on the random angle and the range of angles.
				// If the difference in angles is more than the angle range, select an angle
				// within that range instead.
				if (( AngleDifference/* / 2*/ ) > AngleRange )
				{
//					Printf( "Using AngleDifference as maximum angle range\n" );

					// Disallow % 0.
					if ( AngleDifference == 0 )
						AngleDifference++;

					AngleDesired = ( AngleRandom % ( AngleDifference/* / 2*/ ));
				}
				else
				{
					AngleDesired = ( AngleRandom % AngleRange );
				}

//				Printf( "Desired angle offset: %2.2f\n", (float)( (float)AngleDesired / ANGLE_1 ));

				if ( g_RandomBotAimSeed( ) % 2 )
				{
//					Printf( "Randomly picked angle to the left...\n" );

					AngleFinal = Angle + AngleDesired;
					m_AngleDesired = AngleDesired;
				}
				// Otherwise, turn counter-clockwise.
				else
				{
//					Printf( "Randomly picked angle to the right...\n" );

					AngleFinal = Angle - AngleDesired;
					m_AngleDesired = ANGLE_MAX - AngleDesired;
				}

//				Printf( "AngleFinal: %2.2f\n", (float)AngleFinal / ANGLE_1 );
//				Printf( "Player's angle: %2.2f\n", (float)m_pPlayer->mo->angle / ANGLE_1 );

				// Now AngleDifference is going to be the differnce between our current angle
				// and the final angle!
				if ( AngleFinal > m_pPlayer->mo->angle )
				{
//					Printf( "We'll need to turn LEFT\n" );
					m_bTurnLeft = true;
					AngleDifference = AngleFinal - m_pPlayer->mo->angle;
				}
				else
				{
//					Printf( "We'll need to turn RIGHT\n" );
					m_bTurnLeft = false;
					AngleDifference = m_pPlayer->mo->angle - AngleFinal;
				}

//				Printf( "AngleDifference: %2.2f\n", (float)AngleDifference / ANGLE_1 );
				if ( AngleDifference > ANGLE_180 )
				{
					m_bTurnLeft = !m_bTurnLeft;
					AngleDifference = ( ANGLE_MAX - AngleDifference ) + 1;	// Add 1 because ANGLE_MAX is 1 short of 360
//					Printf( "Angle too large! New value: %2.2f (changing direction)\n", (float)Angle / ANGLE_1 );
				}

				// Our delta angle is the angles we turn each tick, so that at the end of our
				// turning, we've reached our desired angle.
				m_AngleOffBy = AngleDifference;
				m_AngleDelta = ( AngleDifference / ( m_ulAimAtEnemyDelay + 1 ));
//				Printf( "AngleDelta (%d / %d): %2.2f\n", AngleDifference / ANGLE_1, m_ulAimAtEnemyDelay + 1, (float)( (float)m_AngleDelta / ANGLE_1 ));
//				Printf( "All done!\n" );
			}
			else
			{
				m_ulAimAtEnemyDelay--;
			}
		}
	}
	else
		m_pPlayer->mo->pitch = 0;
}

//*****************************************************************************
//
POS_t CSkullBot::GetEnemyPosition( void )
{
	LONG		lTicks;
	POS_t		Pos1;
	POS_t		Pos2;
	POS_t		PosFinal;
	BOTSKILL_e	Skill;

	Skill = BOTS_AdjustSkill( this, BOTINFO_GetPerception( m_ulBotInfoIdx ));
	switch ( Skill )
	{
	case BOTSKILL_VERYPOOR:

		lTicks = 25;
		Pos1 = m_EnemyPosition[( gametic - lTicks ) % MAX_REACTION_TIME];
		Pos2 = m_EnemyPosition[( gametic - ( lTicks + 1 )) % MAX_REACTION_TIME];
		break;
	case BOTSKILL_POOR:

		lTicks = 19;
		Pos1 = m_EnemyPosition[( gametic - lTicks ) % MAX_REACTION_TIME];
		Pos2 = m_EnemyPosition[( gametic - ( lTicks + 1 )) % MAX_REACTION_TIME];
		break;
	case BOTSKILL_LOW:

		lTicks = 14;
		Pos1 = m_EnemyPosition[( gametic - lTicks ) % MAX_REACTION_TIME];
		Pos2 = m_EnemyPosition[( gametic - ( lTicks + 1 )) % MAX_REACTION_TIME];
		break;
	case BOTSKILL_MEDIUM:

		lTicks = 10;
		Pos1 = m_EnemyPosition[( gametic - lTicks ) % MAX_REACTION_TIME];
		Pos2 = m_EnemyPosition[( gametic - ( lTicks + 1 )) % MAX_REACTION_TIME];
		break;
	case BOTSKILL_HIGH:

		lTicks = 7;
		Pos1 = m_EnemyPosition[( gametic - lTicks ) % MAX_REACTION_TIME];
		Pos2 = m_EnemyPosition[( gametic - ( lTicks + 1 )) % MAX_REACTION_TIME];
		break;
	case BOTSKILL_EXCELLENT:

		lTicks = 5;
		Pos1 = m_EnemyPosition[( gametic - lTicks ) % MAX_REACTION_TIME];
		Pos2 = m_EnemyPosition[( gametic - ( lTicks + 1 )) % MAX_REACTION_TIME];
		break;
	case BOTSKILL_SUPREME:

		lTicks = 3;
		Pos1 = m_EnemyPosition[( gametic - lTicks ) % MAX_REACTION_TIME];
		Pos2 = m_EnemyPosition[( gametic - ( lTicks + 1 )) % MAX_REACTION_TIME];
		break;
	case BOTSKILL_GODLIKE:

		lTicks = 1;
		Pos1 = m_EnemyPosition[( gametic - lTicks ) % MAX_REACTION_TIME];
		Pos2 = m_EnemyPosition[( gametic - ( lTicks + 1 )) % MAX_REACTION_TIME];
		break;
	case BOTSKILL_PERFECT:

		return ( m_EnemyPosition[gametic % MAX_REACTION_TIME] );
	default:

		{
			POS_t	ZeroPos;
		
			I_Error( "GetEnemyPosition: Unknown bot skill level, %d!", Skill );

			// To shut the compiler up...
			ZeroPos.x = ZeroPos.y = ZeroPos.z = 0;
			return ( ZeroPos );
		}
		break;
	}

	PosFinal.x = Pos1.x + (( Pos1.x - Pos2.x ) * lTicks );
	PosFinal.y = Pos1.y + (( Pos1.y - Pos2.y ) * lTicks );
	PosFinal.z = Pos1.z + (( Pos1.z - Pos2.z ) * lTicks );
	return ( PosFinal );
}

//*****************************************************************************
//
void CSkullBot::SetEnemyPosition( fixed_t X, fixed_t Y, fixed_t Z )
{
	m_EnemyPosition[gametic % MAX_REACTION_TIME].x = X;
	m_EnemyPosition[gametic % MAX_REACTION_TIME].y = Y;
	m_EnemyPosition[gametic % MAX_REACTION_TIME].z = Z;

	m_ulLastEnemyPositionTick = gametic;
	if ( m_ulPathType == BOTPATHTYPE_ENEMYPOSITION )
		m_ulPathType = BOTPATHTYPE_NONE;
}

//*****************************************************************************
//
POS_t CSkullBot::GetLastEnemyPosition( void )
{
	return ( m_EnemyPosition[m_ulLastEnemyPositionTick % MAX_REACTION_TIME] );
}

//*****************************************************************************
//
void CSkullBot::AddEventToQueue( BOTEVENT_e Event, LONG lTick )
{
	// [BB]: Just ignore additional events, if the queue is full, since the queue fills easily in timefreeze mode.
	if( ((m_lQueueTail+1) % MAX_STORED_EVENTS)  != m_lQueueHead ){
		m_StoredEventQueue[m_lQueueTail].Event = Event;
		m_StoredEventQueue[m_lQueueTail++].lTick = lTick;
		m_lQueueTail = m_lQueueTail % MAX_STORED_EVENTS;

		if ( m_lQueueTail == m_lQueueHead )
			I_Error( "AddEventToQueue: Event queue size exceeded!" );
	}
}

//*****************************************************************************
//
void CSkullBot::DeleteEventFromQueue( void )
{
	LONG		lIdx;
	BOTEVENT_e	Event;

	Event = m_StoredEventQueue[m_lQueueHead++].Event;
	m_lQueueHead = m_lQueueHead % MAX_STORED_EVENTS;

	if ( botdebug_showevents )
		Printf( "%s: %s\n", GetPlayer( )->userinfo.GetName(), g_pszEventNames[Event] );

	// First, scan global events.
	for ( lIdx = 0; lIdx < m_ScriptData.lNumGlobalEvents; lIdx++ )
	{
		// Found a matching event.
		if ( m_ScriptData.GlobalEventPositions[lIdx].Event == Event )
		{
			if ( m_ScriptData.bInEvent == false )
			{
				// Save the current position in the script, since we will go back to it if we do not change
				// states.
				m_ScriptData.lSavedScriptPos = m_ScriptData.lScriptPos;

				m_ScriptData.bInEvent = true;
			}

			m_ScriptData.lScriptPos = m_ScriptData.GlobalEventPositions[lIdx].lPos;
			m_ScriptData.RawData.Seek( m_ScriptData.lScriptPos, SEEK_SET );

			// Parse after the event.
			m_ScriptData.ulScriptEventDelay = 0;
			ParseScript( );
			return;
		}
	}

	// If the event wasn't found in the global events, scan state events.
	for ( lIdx = 0; lIdx < MAX_NUM_EVENTS; lIdx++ )
	{
		// Found a matching event.
		if ( m_ScriptData.EventPositions[m_ScriptData.lCurrentStateIdx][lIdx].Event == Event )
		{
			if ( m_ScriptData.bInEvent == false )
			{
				// Save the current position in the script, since we will go back to it if we do not change
				// states.
				m_ScriptData.lSavedScriptPos = m_ScriptData.lScriptPos;

				m_ScriptData.bInEvent = true;
			}

			m_ScriptData.lScriptPos = m_ScriptData.EventPositions[m_ScriptData.lCurrentStateIdx][lIdx].lPos;
			m_ScriptData.RawData.Seek( m_ScriptData.lScriptPos, SEEK_SET );

			// Parse after the event.
			m_ScriptData.ulScriptEventDelay = 0;
			ParseScript( );
			return;
		}
	}
}

//*****************************************************************************
//	CONSOLE COMMANDS

// Add a secret bot to the skirmish menu
CCMD( reveal )
{
	char	szBuffer[256];
	ULONG	ulIdx;

	if ( argv.argc( ) < 2 )
	{
		Printf( PRINT_HIGH, "reveal [botname]: reveals a secret bot\n" );
		return;
	}

	for ( ulIdx = 0; ulIdx < g_BotInfo.Size(); ulIdx++ )
	{
		if ( g_BotInfo[ulIdx].bRevealed )
			continue;

		sprintf( szBuffer, "%s", g_BotInfo[ulIdx].szName );
		V_ColorizeString( szBuffer );
		V_RemoveColorCodes( szBuffer );

		if ( stricmp( argv[1], szBuffer ) == 0 )
		{
			Printf( "Hidden bot \"%s\\c-\" has now been revealed!\n", g_BotInfo[ulIdx].szName );
			g_BotInfo[ulIdx].bRevealed = true;
		}
	}

	for ( ulIdx = 0; ulIdx < (ULONG)skins.Size(); ulIdx++ )
	{
		if ( skins[ulIdx].bRevealed )
			continue;

		sprintf( szBuffer, "%s", skins[ulIdx].name );
		V_ColorizeString( szBuffer );
		V_RemoveColorCodes( szBuffer );

		if ( stricmp( argv[1], skins[ulIdx].name ) == 0 )
		{
			Printf( "Hidden skin \"%s\\c-\" has now been revealed!\n", skins[ulIdx].name );
			skins[ulIdx].bRevealed = true;
		}
	}
}

//*****************************************************************************
//
CCMD( addbot )
{
	CSkullBot	*pBot;
	ULONG		ulPlayerIdx;

	if ( gamestate != GS_LEVEL )
		return;

	// Don't allow adding of bots in campaign mode.
	if (( CAMPAIGN_InCampaign( )) && ( sv_cheats == false ))
	{
		Printf( "Bots cannot be added manually during a campaign!\n" );
		return;
	}

	// Don't allow bots in network mode, unless we're the host.
	if ( NETWORK_InClientMode() )
	{
		Printf( "Only the host can add bots!\n" );
		return;
	}

	if ( level.flagsZA & LEVEL_ZA_NOBOTNODES )
	{
		Printf( "The bot pathing nodes have not been set up. This level has disabled the ability to do so.\n" );
		// Don't allow the bot to spawn.
		return;
	}

	// Break if we have the wrong amount of arguments.
	if ( argv.argc( ) > 3 )
	{
		Printf( "addbot [botname] [team name]: add a bot to the game\n" );
		return;
	}

	ulPlayerIdx = BOTS_FindFreePlayerSlot( );
	if ( ulPlayerIdx == MAXPLAYERS )
	{
		Printf( "The maximum number of players/bots has been reached.\n" );
		return;
	}

	switch ( argv.argc( ) )
	{
	case 1:

		// Allocate a new bot.
		pBot = new CSkullBot( NULL, NULL, BOTS_FindFreePlayerSlot( ));
		break;
	case 2:

		// Verify that the bot's name matches one of the existing bots.
		if ( BOTS_IsValidName( argv[1] ))
			pBot = new CSkullBot( argv[1], NULL, BOTS_FindFreePlayerSlot( ));
		else
		{
			Printf( "Invalid bot name: %s\n", argv[1] );
			return;
		}
		break;
	case 3:

		// Verify that the bot's name matches one of the existing bots.
		if ( BOTS_IsValidName( argv[1] ))
			pBot = new CSkullBot( argv[1], argv[2], BOTS_FindFreePlayerSlot( ));
		else
		{
			Printf( "Invalid bot name: %s\n", argv[1] );
			return;
		}
		break;
	}
}

//*****************************************************************************
//
CCMD( removebot )
{
	ULONG	ulIdx;
	char	szName[64];

	// Don't allow removing of bots in campaign mode.
	if (( CAMPAIGN_InCampaign( )) && ( sv_cheats == false ))
	{
		Printf( "Bots cannot be removed manually during a campaign!\n" );
		return;
	}

	// If we didn't input which bot to remove, remove a random one.
	if ( argv.argc( ) < 2 )
	{
		ULONG	ulRandom;
		bool	bBotInGame = false;

		// First, verify that there's a bot in the game.
		for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			if (( playeringame[ulIdx] ) && ( players[ulIdx].pSkullBot ))
			{
				bBotInGame = true;
				break;
			}
		}

		// If there isn't, break.
		if ( bBotInGame == false )
		{
			Printf( "No bots found.\n" );
			return;
		}

		// Now randomly select a bot to remove.
		do
		{
			ulRandom = ( BotRemove( ) % MAXPLAYERS );
		} while (( playeringame[ulRandom] == false ) || ( players[ulRandom].pSkullBot == NULL ));

		// Now that we've found a valid bot, remove it.
		BOTS_RemoveBot( ulRandom, true );
	}
	else
	{
		for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			if (( playeringame[ulIdx] == false ) || ( players[ulIdx].pSkullBot == NULL ))
				continue;

			sprintf( szName, "%s", players[ulIdx].userinfo.GetName() );
			V_RemoveColorCodes( szName );
			if ( stricmp( szName, argv[1] ) == 0 )
			{
				// Now that we've found a valid bot, remove it.
				BOTS_RemoveBot( ulIdx, true );

				// Nothing more to do.
				return;
			}
		}
	}
}

//*****************************************************************************
//
CCMD( removebots )
{
	// Don't allow removing of bots in campaign mode.
	if (( CAMPAIGN_InCampaign( )) && ( sv_cheats == false ))
	{
		Printf( "Bots cannot be removed manually during a campaign!\n" );
		return;
	}

	// Remove all the bots from the level. If less than 3 seconds have passed,
	// the scripts are probably clearing out all the bots from the level, so don't
	// display exit messages.
	if ( level.time > ( 3 * TICRATE ))
		BOTS_RemoveAllBots( true );
	else
		BOTS_RemoveAllBots( false );
}

//*****************************************************************************
//
CCMD( listbots )
{
	ULONG	ulIdx;
	ULONG	ulNumBots = 0;

	Printf( "Active bots:\n\n" );

	// Loop through all the players and count up the bots.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( playeringame[ulIdx] && players[ulIdx].pSkullBot )
		{
			Printf( "%s\n", players[ulIdx].userinfo.GetName() );
			ulNumBots++;
		}
	}

	Printf( "\n%u bot%s.\n", static_cast<unsigned int> (ulNumBots), ulNumBots == 1 ? "" : "s" );
}

//*****************************************************************************
ADD_STAT( bots )
{
	FString	Out;

	Out.Format( "Bot cycles = %04.1f ms",
		g_BotCycles.TimeMS()
		);

	return ( Out );
}


