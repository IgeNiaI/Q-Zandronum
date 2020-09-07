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
// Filename: bots.h
//
// Description: Contains bot structures and prototypes
//
//-----------------------------------------------------------------------------

#ifndef __BOTS_H__
#define __BOTS_H__

#include "astar.h"
#include "c_cvars.h"
#include "d_main.h"
#include "d_player.h"
#include "d_ticcmd.h"
#include "doomdef.h"
#include "info.h"
#include "m_argv.h"
#include "r_defs.h"
#include "tables.h"
#include "w_wad.h"

//*****************************************************************************
//  DEFINES

// Maximum number of variables on the bot evaluation stack.
#define	BOTSCRIPT_STACK_SIZE		8

// Maximum number of states that can appear in a script.
#define	MAX_NUM_STATES				256

// Maximum number of bot events that can be defined.
#define	MAX_NUM_EVENTS				32

// Maximum number of global bot events that can be defined.
#define	MAX_NUM_GLOBAL_EVENTS		32

// Maximum number of global variables that can be defined in a script.
#define	MAX_SCRIPT_VARIABLES		128

// Maximum number of global arrays that can be defined in a script.
#define	MAX_SCRIPT_ARRAYS			16

// Maximum number of global arrays that can be defined in a script.
#define	MAX_SCRIPTARRAY_SIZE		65536

// Maxmimum number of state (local) variables that can appear in a script.
#define	MAX_STATE_VARIABLES			16

// Maximum number of strings that can appear in a script stringlist.
#define	MAX_LIST_STRINGS			128

// Maximum length of those strings in the stringlist.
#define	MAX_STRING_LENGTH			256

// Maximum reaction time for a bot.
#define	MAX_REACTION_TIME			52

// Maximum number of events the bots can store up that it's waiting to react to.
#define	MAX_STORED_EVENTS			64

//*****************************************************************************
typedef enum
{
	// Bot skill ratings.
	BOTSKILL_VERYPOOR,
	BOTSKILL_POOR,
	BOTSKILL_LOW,
	BOTSKILL_MEDIUM,
	BOTSKILL_HIGH,
	BOTSKILL_EXCELLENT,
	BOTSKILL_SUPREME,
	BOTSKILL_GODLIKE,
	BOTSKILL_PERFECT,

	NUM_BOT_SKILLS

} BOTSKILL_e;

//*****************************************************************************
enum
{
	BOTPATHTYPE_NONE,
	BOTPATHTYPE_ROAM,
	BOTPATHTYPE_ITEM,
	BOTPATHTYPE_ENEMYPOSITION,

	NUM_BOTPATHTYPES
};

//*****************************************************************************
//  STRUCTURES
//
typedef struct
{
	// Name of bot about to be spawned.
	char	szName[64];

	// Name of the team the bot will be on.
	char	szTeam[32];

	// How much longer the bot has left before it spawns.
	ULONG	ulTick;

} BOTSPAWN_t;

//*****************************************************************************
//	These are the botscript data headers that it writes out.
typedef enum
{
	DH_COMMAND,
	DH_STATEIDX,
	DH_STATENAME,
	DH_ONENTER,
	DH_MAINLOOP,
	DH_ONEXIT,
	DH_EVENT,
	DH_ENDONENTER,
	DH_ENDMAINLOOP,
	DH_ENDONEXIT,
	DH_ENDEVENT,
	DH_IFGOTO,
	DH_IFNOTGOTO,
	DH_GOTO,
	DH_ORLOGICAL,
	DH_ANDLOGICAL,
	DH_ORBITWISE,
	DH_EORBITWISE,
	DH_ANDBITWISE,
	DH_EQUALS,
	DH_NOTEQUALS,
	DH_LESSTHAN,
	DH_LESSTHANEQUALS,
	DH_GREATERTHAN,
	DH_GREATERTHANEQUALS,
	DH_NEGATELOGICAL,
	DH_LSHIFT,
	DH_RSHIFT,
	DH_ADD,
	DH_SUBTRACT,
	DH_UNARYMINUS,
	DH_MULTIPLY,
	DH_DIVIDE,
	DH_MODULUS,
	DH_PUSHNUMBER,
	DH_PUSHSTRINGINDEX,
	DH_PUSHGLOBALVAR,
	DH_PUSHLOCALVAR,
	DH_DROPSTACKPOSITION,
	DH_SCRIPTVARLIST,
	DH_STRINGLIST,
	DH_INCGLOBALVAR,
	DH_DECGLOBALVAR,
	DH_ASSIGNGLOBALVAR,
	DH_ADDGLOBALVAR,
	DH_SUBGLOBALVAR,
	DH_MULGLOBALVAR,
	DH_DIVGLOBALVAR,
	DH_MODGLOBALVAR,
	DH_INCLOCALVAR,
	DH_DECLOCALVAR,
	DH_ASSIGNLOCALVAR,
	DH_ADDLOCALVAR,
	DH_SUBLOCALVAR,
	DH_MULLOCALVAR,
	DH_DIVLOCALVAR,
	DH_MODLOCALVAR,
	DH_CASEGOTO,
	DH_DROP,
	DH_INCGLOBALARRAY,
	DH_DECGLOBALARRAY,
	DH_ASSIGNGLOBALARRAY,
	DH_ADDGLOBALARRAY,
	DH_SUBGLOBALARRAY,
	DH_MULGLOBALARRAY,
	DH_DIVGLOBALARRAY,
	DH_MODGLOBALARRAY,
	DH_PUSHGLOBALARRAY,
	DH_SWAP,
	DH_DUP,
	DH_ARRAYSET,

	NUM_DATAHEADERS

} DATAHEADERS_e;

//*****************************************************************************
//	These are the different bot events that can be posted to a bot's state.
typedef enum
{
	BOTEVENT_KILLED_BYENEMY,
	BOTEVENT_KILLED_BYPLAYER,
	BOTEVENT_KILLED_BYSELF,
	BOTEVENT_KILLED_BYENVIORNMENT,
	BOTEVENT_REACHED_GOAL,
	BOTEVENT_GOAL_REMOVED,
	BOTEVENT_DAMAGEDBY_PLAYER,
	BOTEVENT_PLAYER_SAY,
	BOTEVENT_ENEMY_KILLED,
	BOTEVENT_RESPAWNED,
	BOTEVENT_INTERMISSION,
	BOTEVENT_NEWMAP,
	BOTEVENT_ENEMY_USEDFIST,
	BOTEVENT_ENEMY_USEDCHAINSAW,
	BOTEVENT_ENEMY_FIREDPISTOL,
	BOTEVENT_ENEMY_FIREDSHOTGUN,
	BOTEVENT_ENEMY_FIREDSSG,
	BOTEVENT_ENEMY_FIREDCHAINGUN,
	BOTEVENT_ENEMY_FIREDMINIGUN,
	BOTEVENT_ENEMY_FIREDROCKET,
	BOTEVENT_ENEMY_FIREDGRENADE,
	BOTEVENT_ENEMY_FIREDRAILGUN,
	BOTEVENT_ENEMY_FIREDPLASMA,
	BOTEVENT_ENEMY_FIREDBFG,
	BOTEVENT_ENEMY_FIREDBFG10K,
	BOTEVENT_PLAYER_USEDFIST,
	BOTEVENT_PLAYER_USEDCHAINSAW,
	BOTEVENT_PLAYER_FIREDPISTOL,
	BOTEVENT_PLAYER_FIREDSHOTGUN,
	BOTEVENT_PLAYER_FIREDSSG,
	BOTEVENT_PLAYER_FIREDCHAINGUN,
	BOTEVENT_PLAYER_FIREDMINIGUN,
	BOTEVENT_PLAYER_FIREDROCKET,
	BOTEVENT_PLAYER_FIREDGRENADE,
	BOTEVENT_PLAYER_FIREDRAILGUN,
	BOTEVENT_PLAYER_FIREDPLASMA,
	BOTEVENT_PLAYER_FIREDBFG,
	BOTEVENT_PLAYER_FIREDBFG10K,
	BOTEVENT_USEDFIST,
	BOTEVENT_USEDCHAINSAW,
	BOTEVENT_FIREDPISTOL,
	BOTEVENT_FIREDSHOTGUN,
	BOTEVENT_FIREDSSG,
	BOTEVENT_FIREDCHAINGUN,
	BOTEVENT_FIREDMINIGUN,
	BOTEVENT_FIREDROCKET,
	BOTEVENT_FIREDGRENADE,
	BOTEVENT_FIREDRAILGUN,
	BOTEVENT_FIREDPLASMA,
	BOTEVENT_FIREDBFG,
	BOTEVENT_FIREDBFG10K,
	BOTEVENT_PLAYER_JOINEDGAME,
	BOTEVENT_JOINEDGAME,
	BOTEVENT_DUEL_STARTINGCOUNTDOWN,
	BOTEVENT_DUEL_FIGHT,
	BOTEVENT_DUEL_WINSEQUENCE,
	BOTEVENT_SPECTATING,
	BOTEVENT_LMS_STARTINGCOUNTDOWN,
	BOTEVENT_LMS_FIGHT,
	BOTEVENT_LMS_WINSEQUENCE,
	BOTEVENT_WEAPONCHANGE,
	BOTEVENT_ENEMY_BFGEXPLODE,
	BOTEVENT_PLAYER_BFGEXPLODE,
	BOTEVENT_BFGEXPLODE,
	BOTEVENT_RECEIVEDMEDAL,

	NUM_BOTEVENTS

} BOTEVENT_e;

//*****************************************************************************
typedef struct
{
	// Position of the beginning of the state.
	LONG	lPos;

	// Position of the onenter section of the state.
	LONG	lOnEnterPos;

	// Position of the onmainloop section of the state.
	LONG	lMainloopPos;

	// Position of the onexit section of the state.
	LONG	lOnExitPos;

} STATEPOSITION_t;

//*****************************************************************************
typedef struct
{
	// Position of this event in the script.
	LONG		lPos;

	// Type of event this is.
	BOTEVENT_e	Event;

} EVENTPOSITION_t;

//*****************************************************************************
typedef struct
{
	// Raw data in the script.
	FWadLump	RawData;

	// Position in the script.
	LONG		lScriptPos;

	// Saved position in the script (used in events).
	LONG		lSavedScriptPos;

	// Bot is exiting his current state to change to a different one.
	bool		bExitingState;

	// Are we in the onexit section of our state?
	bool		bInOnExit;

	// Are we in the onexit section of our state?
	bool		bInEvent;

	// State the bot is switching to.
	LONG		lNextState;

	// Number of events in each states.
	LONG		lNumEvents[MAX_NUM_STATES];

	// Number of global events in each script.
	LONG		lNumGlobalEvents;

	// The positions of all the state and state sections in the bot's script.
	STATEPOSITION_t	StatePositions[MAX_NUM_STATES];

	// The positions of all the state events in the bot's script.
	EVENTPOSITION_t	EventPositions[MAX_NUM_STATES][MAX_NUM_EVENTS];

	// The positions of all the global event positions in the bot's script.
	EVENTPOSITION_t	GlobalEventPositions[MAX_NUM_GLOBAL_EVENTS];

	// Current state the script is in.
	LONG		lCurrentStateIdx;

	// Number of ticks this bot's script is paused for.
	ULONG		ulScriptDelay;

	// Number of ticks this bot's script is paused for in an event.
	ULONG		ulScriptEventDelay;

	char		szStateName[MAX_NUM_STATES][64];

	LONG		alStack[BOTSCRIPT_STACK_SIZE];
	
	LONG		lStackPosition;

	char		aszStringStack[BOTSCRIPT_STACK_SIZE][MAX_STRING_LENGTH];

	LONG		lStringStackPosition;

	LONG		alScriptVariables[MAX_SCRIPT_VARIABLES];

	LONG		alScriptArrays[MAX_SCRIPT_ARRAYS][MAX_SCRIPTARRAY_SIZE];

	LONG		alStateVariables[MAX_NUM_STATES][MAX_STATE_VARIABLES];

	char		szStringList[MAX_LIST_STRINGS][MAX_STRING_LENGTH];

} SCRIPTDATA_t;

//*****************************************************************************
typedef struct
{
	// The name of this bot.
	char			szName[64];

	// This is the bot's ability to hit opponents with "instant" weapons.
	BOTSKILL_e		Accuracy;

	// This is the bots ability to hit opponents with "missile" weapons (rocket launcher, etc.).
	BOTSKILL_e		Intellect;

	// This is the bots ability to dodge incoming missiles.
	BOTSKILL_e		Evade;

	// This is the bots ability to anticipate "instant" shots.
	BOTSKILL_e		Anticipation;

	// Overall reaction time.
	BOTSKILL_e		ReactionTime;

	// This is the bot's ability to perceive where their enemy is.
	BOTSKILL_e		Perception;

	// The name of this bot's favorite weapon. It will switch to it whenever it has it and ammo.
	char			szFavoriteWeapon[32];

	// Bot's player color (in form of a string).
	char			szColor[16];

	// Male/female/it :)
	char			szGender[16];

	// Name of the skin that this bot will use.
	char			szSkinName[32];

	// Class this bot will be in Hexen.
	char			szClassName[32];

	// Railgun trail color.
	ULONG			ulRailgunColor;

	// How often should this bot talk?
	ULONG			ulChatFrequency;

	// Is this bot selectable at the skirmish menu?
	bool			bRevealed;

	// Is this bot revealed by default?
	bool			bRevealedByDefault;

	// Name of the lump that contains this bot's script.
	char			szScriptName[65];

	// The default chatfile used by this bot.
	char			szChatFile[128];

	// The default chatlump used by this bot.
	char			szChatLump[32];

} BOTINFO_s;

//*****************************************************************************
typedef struct
{
	// The event that will be posted to the bot's script.
	BOTEVENT_e	Event;

	// Game tick this event should be activated on.
	LONG		lTick;

} BOTEVENT_s;

//*****************************************************************************
//	This is the definition of a bot. This is attached to the player structure.
class CSkullBot
{

public:
	//*************************************************************************
	CSkullBot( char *pszName, char *pszTeamName, ULONG ulPlayerNum );
	~CSkullBot( );

	// Used for saving the bot for savegames.
	void		Serialize( FArchive &arc );

	// Tick's the bot's logic. This single function handles everything!
	void		Tick( void );

	// Apply things like movement flags, etc.
	void		EndTick( void );

	// Post an event to this bot's current state.
	void		PostEvent( BOTEVENT_e Event );

	// Parse the bot's script.
	void		ParseScript( void );

	// Parse a section of the bot's script.
	void		GetStatePositions( void );

	// Pause a bot's script.
	ULONG		GetScriptDelay( void );
	void		SetScriptDelay( ULONG ulDelay );
	ULONG		GetScriptEventDelay( void );
	void		SetScriptEventDelay( ULONG ulDelay );

	player_t	*GetPlayer( void );

	FWadLump	*GetRawScriptData( void );

	LONG		GetScriptPosition( void );
	void		SetScriptPosition( LONG lPosition );
	void		IncrementScriptPosition( LONG lUnits );

	void		SetExitingState( bool bExit );

	void		SetNextState( LONG lState );

	void		PushToStack( LONG lValue );

	void		PopStack( void );

	void		PushToStringStack( char *pszString );

	void		PopStringStack( void );

	void		PreDelete( void );

	void		HandleAiming( void );

	// Get the current enemy's position. How accurate this will be depends on the bots perception.
	POS_t		GetEnemyPosition( void );
	void		SetEnemyPosition( fixed_t X, fixed_t Y, fixed_t Z );

	// Get the last known enemy position.
	POS_t		GetLastEnemyPosition( void );

	//*************************
	// SHOULD BE PRIVATE!!!!!!!

	LONG			m_lForwardMove;
	LONG			m_lSideMove;

	bool			m_bForwardMovePersist;
	bool			m_bSideMovePersist;

	bool			m_bHasScript;

	AActor			*m_pGoalActor;

	ULONG			m_ulPlayerEnemy;

	ULONG			m_ulLastSeenPlayer;
	ULONG			m_ulLastPlayerDamagedBy;
	ULONG			m_ulPlayerKilledBy;
	ULONG			m_ulPlayerKilled;

	LONG			m_lButtons;

	bool			m_bAimAtEnemy;

	ULONG			m_ulAimAtEnemyDelay;

	angle_t			m_AngleDelta;

	angle_t			m_AngleOffBy;

	angle_t			m_AngleDesired;

	bool			m_bTurnLeft;

	// The last player that attacked this bot.
	ULONG			m_ulLastPlayerAttacker;

	// Data for the bot's script.
	SCRIPTDATA_t	m_ScriptData;

	// What is the bot current using his path for?
	ULONG			m_ulPathType;

	// What is the goal location of the path?
	POS_t			m_PathGoalPos;

	// Set of "botinfo" this bot uses.
	ULONG			m_ulBotInfoIdx;

	// Is the bot's skill temporarily increased?
	bool			m_bSkillIncrease;

	// Is the bot's skill temporarily decreased?
	bool			m_bSkillDecrease;

	// What's the last medal we received?
	ULONG			m_ulLastMedalReceived;

private:
	//*************************************************************************

	// Points back to the reference player.
	player_t		*m_pPlayer;

	// XYZ position of the bot's target.
	POS_t			m_posTarget;

	// Known position of the enemy.
	POS_t			m_EnemyPosition[MAX_REACTION_TIME];

	// Last tick the enemy position was updated.
	ULONG			m_ulLastEnemyPositionTick;

	// List of events that the bot is waiting to react to.
	BOTEVENT_s		m_StoredEventQueue[MAX_STORED_EVENTS];

	LONG			m_lQueueHead;
	LONG			m_lQueueTail;

	void			AddEventToQueue( BOTEVENT_e Event, LONG lTick );
	void			DeleteEventFromQueue( void );
};

//*****************************************************************************
//  PROTOTYPES

// Standard API
void		BOTS_Construct( void );
void		BOTS_Tick( void );
void		BOTS_Destruct( void );

bool		BOTS_AddBotInfo( BOTINFO_s *pBotInfo );
void		BOTS_ParseBotInfo( void );
bool		BOTS_IsValidName( char *pszName );
ULONG		BOTS_FindFreePlayerSlot( void );
void		BOTS_RemoveBot( ULONG usPlayerIdx, bool bExitMsg );
void		BOTS_RemoveAllBots( bool bExitMsg );
void		BOTS_ResetCyclesCounter( void );
bool		BOTS_IsPathObstructed( fixed_t Distance, AActor *pSource );
bool		BOTS_IsVisible( AActor *pActor1, AActor *pActor2 );
void		BOTS_ArchiveRevealedBotsAndSkins( FConfigFile *f );
void		BOTS_RestoreRevealedBotsAndSkins( FConfigFile &config );
bool		BOTS_IsBotInitialized( ULONG ulBot );
BOTSKILL_e	BOTS_AdjustSkill( CSkullBot *pBot, BOTSKILL_e Skill );
void		BOTS_PostWeaponFiredEvent( ULONG ulPlayer, BOTEVENT_e EventIfSelf, BOTEVENT_e EventIfEnemy, BOTEVENT_e EventIfPlayer );
void		BOTS_RemoveGoal( AActor* pGoal );
ULONG		BOTS_CountBots( void );

// Botinfo access functions.
ULONG			BOTINFO_GetNumBotInfos( void );
char			*BOTINFO_GetName( ULONG ulIdx );
BOTSKILL_e		BOTINFO_GetAccuracy( ULONG ulIdx );
BOTSKILL_e		BOTINFO_GetIntellect( ULONG ulIdx );
BOTSKILL_e		BOTINFO_GetEvade( ULONG ulIdx );
BOTSKILL_e		BOTINFO_GetAnticipation( ULONG ulIdx );
BOTSKILL_e		BOTINFO_GetReactionTime( ULONG ulIdx );
BOTSKILL_e		BOTINFO_GetPerception( ULONG ulIdx );
char			*BOTINFO_GetFavoriteWeapon( ULONG ulIdx );
char			*BOTINFO_GetClass( ULONG ulIdx );
char			*BOTINFO_GetColor( ULONG ulIdx );
char			*BOTINFO_GetGender( ULONG ulIdx );
char			*BOTINFO_GetSkin( ULONG ulIdx );
ULONG			BOTINFO_GetRailgunColor( ULONG ulIdx );
ULONG			BOTINFO_GetChatFrequency( ULONG ulIdx );
bool			BOTINFO_GetRevealed( ULONG ulIdx );
char			*BOTINFO_GetChatFile( ULONG ulIdx );
char			*BOTINFO_GetChatLump( ULONG ulIdx );

void		BOTSPAWN_AddToTable( const char *pszBotName, const char *pszBotTeam );
void		BOTSPAWN_BlockClearTable( void );
void		BOTSPAWN_ClearTable( void );

// Botspawn access functions.
char		*BOTSPAWN_GetName( ULONG ulIdx );
void		BOTSPAWN_SetName( ULONG ulIdx, char *pszName );

char		*BOTSPAWN_GetTeam( ULONG ulIdx );
void		BOTSPAWN_SetTeam( ULONG ulIdx, char *pszTeam );

ULONG		BOTSPAWN_GetTicks( ULONG ulIdx );
void		BOTSPAWN_SetTicks( ULONG ulIdx, ULONG usTicks );

FArchive &operator<< ( FArchive &arc, POS_t *pPos );
FArchive &operator>> ( FArchive &arc, POS_t *pPos );

//FArchive &operator<< ( FArchive &arc, CSkullBot *bot );
//FArchive &operator>> ( FArchive &arc, CSkullBot *bot );

//*****************************************************************************
//	EXTERNAL CONSOLE VARIABLES

EXTERN_CVAR( Int, botskill )
EXTERN_CVAR( Bool, bot_allowchat )
EXTERN_CVAR( Int, botdebug_statechanges )
EXTERN_CVAR( Int, botdebug_states )
EXTERN_CVAR( Int, botdebug_commands )
EXTERN_CVAR( Int, botdebug_obstructiontest )
EXTERN_CVAR( Int, botdebug_walktest )
EXTERN_CVAR( Int, botdebug_dataheaders )
EXTERN_CVAR( Int, botdebug_showstackpushes );
EXTERN_CVAR( Int, botdebug_showgoal )
EXTERN_CVAR( Int, botdebug_showcosts )
EXTERN_CVAR( Int, botdebug_showcosts )
EXTERN_CVAR( Float, botdebug_maxsearchnodes )
EXTERN_CVAR( Float, botdebug_maxgiveupnodes )
EXTERN_CVAR( Float, botdebug_maxroamgiveupnodes )
EXTERN_CVAR( Int, botdebug_shownodes )

#endif	// __BOTS_H__
