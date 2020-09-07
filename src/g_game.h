// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//	 Duh.
// 
//-----------------------------------------------------------------------------


#ifndef __G_GAME__
#define __G_GAME__

struct event_t;
struct PNGHandle;

struct FMapThing;
struct FPlayerStart;
struct line_t;
//
// GAME
//
void G_DeathMatchSpawnPlayer( int playernum, bool bClientUpdate );
void G_TemporaryTeamSpawnPlayer( ULONG ulPlayer, bool bClientUpdate );
void G_TeamgameSpawnPlayer( ULONG ulPlayer, ULONG ulTeam, bool bClientUpdate );
FPlayerStart *SelectRandomCooperativeSpot( ULONG ulPlayer );
void G_CooperativeSpawnPlayer( ULONG ulPlayer, bool bClientUpdate, bool bTempPlayer = false );

// [BB] Added bGiveInventory and moved the declaration to g_game.h.
void G_PlayerReborn (int player, bool bGiveInventory = true);

// [BC] Determines the game type by map spots and other items placed on the level.
void	GAME_CheckMode( void );

// [TP] Sets default dmflags
void	GAME_SetDefaultDMFlags();

// [BB] Backup certain initial properties of the line necessary for a map reset.
void	GAME_BackupLineProperties ( line_t *li );

// [BC] Function that reverts the map into its original state when it first loaded, without
// actually reloading the map.
void	GAME_ResetMap( bool bRunEnterScripts = false );

// [BB] Allows to request a map reset at a time when GAME_ResetMap can't be called,
// e.g. while executing a ACS function.
void	GAME_RequestMapReset( void );
bool	GAME_IsMapRestRequested( void );

// [BC] Spawn the terminator artifact at a random deathmatch spot for terminator games.
void	GAME_SpawnTerminatorArtifact( void );

// [BC] Spawn the possession artifact at a random deathmatch spot for possession/team possession games.
void	GAME_SpawnPossessionArtifact( void );

// [BC] Access functions.
void	GAME_SetEndLevelDelay( ULONG ulTicks );
ULONG	GAME_GetEndLevelDelay( void );

void	GAME_SetLevelIntroTicks( USHORT usTicks );
USHORT	GAME_GetLevelIntroTicks( void );

ULONG	GAME_CountLivingAndRespawnablePlayers( void );
ULONG	GAME_CountActivePlayers( void );

// [BC] End changes.

struct FPlayerStart *G_PickPlayerStart (int playernum, int flags = 0);
enum
{
	PPS_FORCERANDOM			= 1,
	PPS_NOBLOCKINGCHECK		= 2,
};

void G_DeferedPlayDemo (const char* demo);

// Can be called by the startup code or M_Responder,
// calls P_SetupLevel or W_EnterWorld.
void G_LoadGame (const char* name, bool hidecon=false);

void G_DoLoadGame (void);

// Called by M_Responder.
void G_SaveGame (const char *filename, const char *description);

// Only called by startup code.
void G_RecordDemo (const char* name);

void G_BeginRecording (const char *startmap);

void G_PlayDemo (char* name);
void G_TimeDemo (const char* name);
bool G_CheckDemoStatus (void);

void G_WorldDone (void);

void G_Ticker (void);
bool G_Responder (event_t*	ev);

void G_ScreenShot (char *filename);

FString G_BuildSaveName (const char *prefix, int slot);

struct PNGHandle;
bool G_CheckSaveGameWads (PNGHandle *png, bool printwarn);

enum EFinishLevelType
{
	FINISH_SameHub,
	FINISH_NextHub,
	FINISH_NoHub
};

void G_PlayerFinishLevel (int player, EFinishLevelType mode, int flags);

void G_DoReborn (int playernum, bool freshbot);

// Adds pitch to consoleplayer's viewpitch and clamps it
void G_AddViewPitch (int look);

// Adds to consoleplayer's viewangle if allowed
void G_AddViewAngle (int yaw);

#define BODYQUESIZE 	32
class AActor;
extern AActor *bodyque[BODYQUESIZE]; 
extern int bodyqueslot; 
class AInventory;
extern const AInventory *SendItemUse, *SendItemDrop;

// [BB] Exported G_QueueBody.
void G_QueueBody (AActor *body);

// [TP]
const char* G_DescribeJoinMenuKey();
int G_GetJoinMenuKey();

#endif
