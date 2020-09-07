//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2003 Brad Carney
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
// Filename: sv_commands.h
//
// Description: Contains prototypes for a set of functions that correspond to each
// message a server can send out. Each functions handles the send out of each message.
//
//-----------------------------------------------------------------------------

#ifndef __SV_COMMANDS_H__
#define __SV_COMMANDS_H__

#include <set>
#include "doomtype.h"
#include "network.h"
#include "p_local.h"
#include "p_spec.h"
#include "vectors.h"
#include "a_weaponpiece.h"

#include "tflags.h"

//*****************************************************************************
//	DEFINES

enum ServerCommandFlag
{
	SVCF_SKIPTHISCLIENT			= ( 1 << 0 ),
	SVCF_ONLYTHISCLIENT			= ( 1 << 1 ),
	SVCF_ONLY_CONNECTIONTYPE_0	= ( 1 << 2 ),
	SVCF_ONLY_CONNECTIONTYPE_1	= ( 1 << 3 )
};

/*
 * [TP] For SERVERCOMMANDS_MoveThingIfChanged
 */
struct MoveThingData
{
	MoveThingData( AActor *actor ) :
	    x ( actor->x ),
	    y ( actor->y ),
	    z ( actor->z ),
	    velx ( actor->velx ),
	    vely ( actor->vely ),
	    velz ( actor->velz ),
	    pitch ( actor->pitch ),
	    angle ( actor->angle ),
	    movedir ( actor->movedir ) {}

	fixed_t x, y, z;
	fixed_t velx, vely, velz;
	fixed_t pitch;
	angle_t angle;
	BYTE movedir;
};

typedef TFlags<ServerCommandFlag, unsigned int> ServerCommandFlags;
DEFINE_TFLAGS_OPERATORS (ServerCommandFlags)

//*****************************************************************************
//	PROTOTYPES

// General protocol commands. These handle connecting to and being part of the server.
void	SERVERCOMMANDS_Ping( ULONG ulTime );
void	SERVERCOMMANDS_Nothing( ULONG ulPlayer, bool bReliable = false );
void	SERVERCOMMANDS_BeginSnapshot( ULONG ulPlayer );
void	SERVERCOMMANDS_EndSnapshot( ULONG ulPlayer );

// Player commands. These involve manipulating a player in some way.
void	SERVERCOMMANDS_SpawnPlayer( ULONG ulPlayer, LONG lPlayerState, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0, bool bMorph = false );
void	SERVERCOMMANDS_MovePlayer( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_DamagePlayer( ULONG ulPlayer );
void	SERVERCOMMANDS_KillPlayer( ULONG ulPlayer, AActor *pSource, AActor *pInflictor, FName MOD );
void	SERVERCOMMANDS_SetPlayerHealth( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetPlayerArmor( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetPlayerHealthAndMaxHealthBonus( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetPlayerArmorAndMaxArmorBonus( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetPlayerState( ULONG ulPlayer, PLAYERSTATE_e ulState, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetAllPlayerUserInfo( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetPlayerUserInfo(ULONG ulPlayer, const std::set<FName> &names, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetPlayerFrags( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetPlayerPoints( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetPlayerWins( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetPlayerKillCount( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetPlayerChatStatus( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetPlayerConsoleStatus( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetPlayerLaggingStatus( ULONG ulPlayer );
void	SERVERCOMMANDS_SetPlayerReadyToGoOnStatus( ULONG ulPlayer );
void	SERVERCOMMANDS_SetPlayerTeam( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetPlayerCamera( ULONG ulPlayer, AActor *camera, bool bRevertPlease );
void	SERVERCOMMANDS_SetPlayerPoisonCount( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetPlayerAmmoCapacity( ULONG ulPlayer, AInventory *pAmmo, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetPlayerCheats( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetPlayerPendingWeapon( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
// [BB] Does not work with the latest ZDoom changes. Check if it's still necessary.
//void	SERVERCOMMANDS_SetPlayerPieces( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetPlayerPSprite( ULONG ulPlayer, FState *pState, LONG lPosition, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetPlayerBlend( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetPlayerMaxHealth( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetPlayerLivesLeft( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetPlayerViewHeight( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SyncPlayerAmmoAmount( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_UpdatePlayerPing( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_UpdatePlayerExtraData( ULONG ulPlayer, ULONG ulDisplayPlayer );
void	SERVERCOMMANDS_UpdatePlayerTime( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_MoveLocalPlayer( ULONG ulPlayer );
void	SERVERCOMMANDS_DisconnectPlayer( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetConsolePlayer( ULONG ulPlayer );
void	SERVERCOMMANDS_ConsolePlayerKicked( ULONG ulPlayer );
void	SERVERCOMMANDS_GivePlayerMedal( ULONG ulPlayer, ULONG ulMedal, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_ResetAllPlayersFragcount( ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_PlayerIsSpectator( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_PlayerSay( ULONG ulPlayer, const char *pszString, ULONG ulMode, bool bForbidChatToPlayers, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_PlayerTaunt( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_PlayerRespawnInvulnerability( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_PlayerUseInventory( ULONG ulPlayer, AInventory *pItem, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_PlayerDropInventory( ULONG ulPlayer, AInventory *pItem, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_PotentiallyIgnorePlayer( ULONG ulPlayer );
void	SERVERCOMMANDS_SetPlayerHazardCount ( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetMugShotState ( const char *statename );
void	SERVERCOMMANDS_SetPlayerAccountName( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );

// Thing commands. This involve handling of actors.
void	SERVERCOMMANDS_SpawnThing( AActor *pActor, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SpawnThingNoNetID( AActor *pActor, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SpawnThingExact( AActor *pActor, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SpawnThingExactNoNetID( AActor *pActor, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_LevelSpawnThing( AActor *mobj, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_LevelSpawnThingNoNetID( AActor *mobj, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_MoveThing( AActor *pActor, ULONG ulBits, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_MoveThingIfChanged( AActor *pActor, const MoveThingData &oldData, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_MoveThingExact( AActor *pActor, ULONG ulBits, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_KillThing( AActor *pActor, AActor *pSource, AActor *pInflictor );
void	SERVERCOMMANDS_SetThingState( AActor *pActor, NetworkActorState state, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetThingTarget( AActor *pActor );
void	SERVERCOMMANDS_DestroyThing( AActor *pActor );
void	SERVERCOMMANDS_SetThingAngle( AActor *pActor, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetThingAngleExact( AActor *pActor, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetThingWaterLevel( AActor *pActor, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetThingFlags( AActor *pActor, FlagSet flagset, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetThingArguments( AActor *pActor, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetThingTranslation( AActor *pActor, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetThingProperty( AActor *pActor, ULONG ulProperty, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetThingSound( AActor *pActor, ULONG ulSound, const char *pszSound, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetThingSpawnPoint( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags );
void	SERVERCOMMANDS_SetThingSpecial( AActor *pActor, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetThingSpecial1( AActor *pActor, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetThingSpecial2( AActor *pActor, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetThingTics( AActor *pActor, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetThingReactionTime( AActor *pActor, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetThingTID( AActor *pActor, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetThingGravity( AActor *pActor, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetThingFrame( AActor *pActor, FState *pState, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0, bool bCallStateFunction = true );
void	SERVERCOMMANDS_SetWeaponAmmoGive( AActor *pActor, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_ThingIsCorpse( AActor *pActor, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_HideThing( AActor *pActor, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_TeleportThing( AActor *pActor, bool bSourceFog, bool bDestFog, bool bTeleZoom, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_ThingActivate( AActor *pActor, AActor *pActivator, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_ThingDeactivate( AActor *pActor, AActor *pActivator, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_RespawnDoomThing( AActor *pActor, bool bFog, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_RespawnRavenThing( AActor *pActor, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SpawnBlood( fixed_t x, fixed_t y, fixed_t z, angle_t dir, int damage, AActor *originator, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SpawnBloodSplatter( fixed_t x, fixed_t y, fixed_t z, AActor *originator, bool isBloodSplatter2, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SpawnPuff( AActor *pActor, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SpawnPuffNoNetID( AActor *pActor, ULONG ulState, bool bSendTranslation, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_UpdateThingFlagsNotAtDefaults( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags );
void	SERVERCOMMANDS_SetFastChaseStrafeCount( AActor *mobj, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetThingHealth( AActor* mobj, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetThingScale( AActor* mobj, unsigned int scaleFlags, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_UpdateThingScaleNotAtDefault( AActor* pActor, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_FlashStealthMonster( AActor* pActor, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );

// Print commands. These print some sort of message to the screen.
void	SERVERCOMMANDS_Print( const char *pszString, ULONG ulPrintLevel, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_PrintMid( const char *pszString, bool bBold, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_PrintMOTD( const char *pszString, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_PrintHUDMessage( const char *pszString, float fX, float fY, LONG lHUDWidth, LONG lHUDHeight, LONG lColor, float fHoldTime, const char *pszFont, bool bLog, LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_PrintHUDMessageFadeOut( const char *pszString, float fX, float fY, LONG lHUDWidth, LONG lHUDHeight, LONG lColor, float fHoldTime, float fFadeOutTime, const char *pszFont, bool bLog, LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_PrintHUDMessageFadeInOut( const char *pszString, float fX, float fY, LONG lHUDWidth, LONG lHUDHeight, LONG lColor, float fHoldTime, float fFadeInTime, float fFadeOutTime, const char *pszFont, bool bLog, LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_PrintHUDMessageTypeOnFadeOut( const char *pszString, float fX, float fY, LONG lHUDWidth, LONG lHUDHeight, LONG lColor, float fTypeTime, float fHoldTime, float fFadeOutTime, const char *pszFont, bool bLog, LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );

// Game commands. These manipulate some aspect of the gameplay.
void	SERVERCOMMANDS_SetGameMode( ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetGameSkill( ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetGameDMFlags( ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetGameModeLimits( ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetGameEndLevelDelay( ULONG ulEndLevelDelay, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetGameModeState( ULONG ulState, ULONG ulCountdownTicks, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetDuelNumDuels( ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetLMSSpectatorSettings( ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetLMSAllowedWeapons( ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetInvasionNumMonstersLeft( ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetInvasionWave( ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetSimpleCTFSTMode( ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_DoPossessionArtifactPickedUp( ULONG ulPlayer, ULONG ulTicks, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_DoPossessionArtifactDropped( ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_DoGameModeFight( ULONG ulCurrentWave, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_DoGameModeCountdown( ULONG ulTicks, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_DoGameModeWinSequence( ULONG ulWinner, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetDominationState( ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetDominationPointOwnership( ULONG ulPoint, ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );

// Team commands. These involve one of the teams in teamgame mode.
void	SERVERCOMMANDS_SetTeamFrags( ULONG ulTeam, LONG lFrags, bool bAnnounce, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetTeamScore( ULONG ulTeam, LONG lScore, bool bAnnounce, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetTeamWins( ULONG ulTeam, LONG lWins, bool bAnnounce, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetTeamReturnTicks( ULONG ulTeam, ULONG ulReturnTicks, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_TeamFlagReturned( ULONG ulTeam, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_TeamFlagDropped( ULONG ulPlayer, ULONG ulTeam, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );

// Missile commands. These handle missiles in some way.
void	SERVERCOMMANDS_SpawnMissile( AActor *pMissile, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SpawnMissileExact( AActor *pMissile, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_MissileExplode( AActor *pMissile, line_t *pLine, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );

// Weapon commands. These handle firing weapons, weapon changes, etc.
void	SERVERCOMMANDS_WeaponSound( ULONG ulPlayer, const char *pszSound, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_WeaponChange( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_WeaponRailgun( AActor *source, const FVector3 &start, const FVector3 &end, LONG color1, LONG color2, float maxdiff, int railflags, angle_t angleoffset, const PClass* spawnclass, int duration, float sparsity, float drift, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );

// Sector commands. These manipulate some property of sectors.
void	SERVERCOMMANDS_SetSectorFloorPlane( ULONG ulSector, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetSectorCeilingPlane( ULONG ulSector, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetSectorFloorPlaneSlope( ULONG ulSector, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetSectorCeilingPlaneSlope( ULONG ulSector, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetSectorLightLevel( ULONG ulSector, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetSectorColor( ULONG ulSector, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetSectorColorByTag( ULONG ulTag, ULONG ulRed, ULONG ulGreen, ULONG ulBlue, ULONG ulDesaturate, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetSectorFade( ULONG ulSector, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetSectorFadeByTag( ULONG ulTag, ULONG ulRed, ULONG ulGreen, ULONG ulBlue, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetSectorFlat( ULONG ulSector, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetSectorPanning( ULONG ulSector, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetSectorRotation( ULONG ulSector, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetSectorRotationByTag( ULONG ulTag, LONG lFloorRot, LONG lCeilingRot, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetSectorScale( ULONG ulSector, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetSectorSpecial( ULONG ulLine, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetSectorFriction( ULONG ulSector, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetSectorAngleYOffset( ULONG ulSector, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetSectorGravity( ULONG ulSector, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetSectorReflection( ULONG ulSector, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_StopSectorLightEffect( ULONG ulSector, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_DestroyAllSectorMovers( ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );

// Sector light commands. These start a sector light effect, such as strobe effects.
void	SERVERCOMMANDS_DoSectorLightFireFlicker( ULONG ulSector, LONG lMaxLight, LONG lMinLight, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_DoSectorLightFlicker( ULONG ulSector, LONG lMaxLight, LONG lMinLight, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_DoSectorLightLightFlash( ULONG ulSector, LONG lMaxLight, LONG lMinLight, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_DoSectorLightStrobe( ULONG ulSector, LONG lDarkTime, LONG lBrightTime, LONG lMaxLight, LONG lMinLight, LONG lCount, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_DoSectorLightGlow( ULONG ulSector, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_DoSectorLightGlow2( ULONG ulSector, LONG lStart, LONG lEnd, LONG lTics, LONG lMaxTics, bool bOneShot, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_DoSectorLightPhased( ULONG ulSector, LONG lBaseLevel, LONG lPhase, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );

// Line commands. These have something to do with lines.
void	SERVERCOMMANDS_SetLineAlpha( ULONG ulLine, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetLineTexture( ULONG ulLine, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetLineTextureByID( ULONG ulLineID, ULONG ulSide, ULONG ulPosition, const char *pszTexName, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetSomeLineFlags( ULONG ulLine, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );

// Side commands. These have something to do with lines.
void	SERVERCOMMANDS_SetSideFlags( ULONG ulSide, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );

// ACS commands. These have something to do with ACS scripts.
void	SERVERCOMMANDS_ACSScriptExecute( int ScriptNum, AActor *pActivator, LONG lLineIdx, int levelnum, bool bBackSide, const int *args, int argcount, bool bAlways, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );

// Sound commands. These play a sound.
void	SERVERCOMMANDS_Sound( LONG lChannel, const char *pszSound, float fVolume, float fAttenuation, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SoundActor( AActor *pActor, LONG lChannel, const char *pszSound, float fVolume, float fAttenuation, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0, bool bRespectActorPlayingSomething = false );
void	SERVERCOMMANDS_SoundSector( sector_t *sector, int channel, const char *sound, float volume, float attenuation, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SoundPoint( LONG lX, LONG lY, LONG lZ, LONG lChannel, const char *pszSound, float fVolume, float fAttenuation, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_AnnouncerSound( const char *pszSound, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );

// Sector sequence commands. These handle sector sound sequences.
void	SERVERCOMMANDS_StartSectorSequence( sector_t *pSector, const int Channel, const char *pszSequence, const int Modenum, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_StopSectorSequence( sector_t *pSector, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );

// Voting commands. These handle the voting.
void	SERVERCOMMANDS_CallVote( ULONG ulPlayer, FString Command, FString Parameters, FString Reason, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_PlayerVote( ULONG ulPlayer, bool bVoteYes, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_VoteEnded( bool bVotePassed, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );

// Map commands. These load maps, exit maps, or manipulate some property of the current map.
void	SERVERCOMMANDS_MapLoad( ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_MapNew( const char *pszMapName, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_MapExit( LONG lPosition, const char *pszNextMap, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_MapAuthenticate( const char *pszMapName, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SecretFound( AActor *actor, BYTE secretFlags, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SecretMarkSectorFound( sector_t *sector, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0);
void	SERVERCOMMANDS_SetMapTime( ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetMapNumKilledMonsters( ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetMapNumFoundItems( ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetMapNumFoundSecrets( ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetMapNumTotalMonsters( ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetMapNumTotalItems( ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetMapNumTotalSecrets( ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetMapMusic( const char *pszMusic, int track, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetMapSky( ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );

// Inventory commands. These give the player inventory items, takes them away, etc.
void	SERVERCOMMANDS_GiveInventory( ULONG ulPlayer, AInventory *pInventory, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_GiveInventoryNotOverwritingAmount( AActor *pReceiver, AInventory *pItem );
void	SERVERCOMMANDS_TakeInventory( ULONG ulPlayer, const PClass *inventoryClass, ULONG ulAmount, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_GivePowerup( ULONG ulPlayer, APowerup *pPowerup, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetPowerupBlendColor( ULONG ulPlayer, APowerup *pPowerup, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_DoInventoryPickup( ULONG ulPlayer, const char *pszClassName, const char *pszPickupMessage, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_DestroyAllInventory( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetInventoryIcon( ULONG ulPlayer, AInventory *pInventory, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_GiveWeaponHolder( ULONG ulPlayer, AWeaponHolder *pHolder, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetHexenArmorSlots( ULONG ulPlayer, AHexenArmor *aHXArmor, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SyncHexenArmorSlots( ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );

// Door commands. These create, destroy, and manipulate doors.
void	SERVERCOMMANDS_DoDoor( sector_t *pSector, DDoor::EVlDoor type, LONG lSpeed, LONG lDirection, LONG lLightTag, LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_DestroyDoor( LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_ChangeDoorDirection( LONG lID, LONG lDirection, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );

// Floor commands. Ditto.
void	SERVERCOMMANDS_DoFloor( DFloor::EFloor Type, sector_t *pSector, LONG lDirection, LONG lSpeed, LONG lFloorDestDist, LONG Crush, bool Hexencrush, LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_DestroyFloor( LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_ChangeFloorDirection( LONG lID, LONG lDirection, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_ChangeFloorType( LONG lID, DFloor::EFloor Type, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_ChangeFloorDestDist( LONG lID, LONG lFloorDestDist, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_StartFloorSound( LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_BuildStair( DFloor::EFloor Type, sector_t *pSector, int Direction, fixed_t Speed, fixed_t FloorDestDist, int Crush, bool Hexencrush, int ResetCount, int Delay, int PauseTime, int StepTime, int PerStepTime, int ID, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );

// Ceiling commands. Ditto.
void	SERVERCOMMANDS_DoCeiling( DCeiling::ECeiling Type, sector_t *pSector, LONG lDirection, LONG lBottomHeight, LONG lTopHeight, LONG lSpeed, LONG lCrush, bool Hexencrush, LONG lSilent, LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_DestroyCeiling( LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_ChangeCeilingDirection( LONG lID, LONG lDirection, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_ChangeCeilingSpeed( LONG lID, LONG lSpeed, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_PlayCeilingSound( LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );

// Plat commands. Ditto.
void	SERVERCOMMANDS_DoPlat( DPlat::EPlatType Type, sector_t *pSector, DPlat::EPlatState Status, LONG lHigh, LONG lLow, LONG lSpeed, LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_DestroyPlat( LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_ChangePlatStatus( LONG lID, DPlat::EPlatState Status, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_PlayPlatSound( LONG lID, LONG lSound, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );

// Elevator commands. Ditto.
void	SERVERCOMMANDS_DoElevator( DElevator::EElevator Type, sector_t *pSector, LONG lSpeed, LONG lDirection, LONG lFloorDestDist, LONG lCeilingDestDist, LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_DestroyElevator( LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_StartElevatorSound( LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );

// Pillar commands. Ditto.
void	SERVERCOMMANDS_DoPillar( DPillar::EPillar Type, sector_t *pSector, LONG lFloorSpeed, LONG lCeilingSpeed, LONG lFloorTarget, LONG lCeilingTarget, LONG Crush, bool Hexencrush, LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_DestroyPillar( LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );

// Waggle commands. Ditto.
void	SERVERCOMMANDS_DoWaggle( bool bCeiling, sector_t *pSector, LONG lOriginalDistance, LONG lAccumulator, LONG lAccelerationDelta, LONG lTargetScale, LONG lScale, LONG lScaleDelta, LONG lTicker, LONG lState, LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_DestroyWaggle( LONG lID, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_UpdateWaggle( LONG lID, LONG lAccumulator, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );

// Rotate poly commands. Ditto.
void	SERVERCOMMANDS_DoRotatePoly( LONG lSpeed, LONG lPolyNum, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_DestroyRotatePoly( LONG lPolyNum, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );

// Move poly commands. Ditto.
void	SERVERCOMMANDS_DoMovePoly( LONG lXSpeed, LONG lYSpeed, LONG lPolyNum, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_DestroyMovePoly( LONG lPolyNum, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );

// Poly door commands. Ditto.
void	SERVERCOMMANDS_DoPolyDoor( LONG lType, LONG lXSpeed, LONG lYSpeed, LONG lSpeed, LONG lPolyNum, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_DestroyPolyDoor( LONG lPolyNum, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetPolyDoorSpeedPosition( LONG lPolyNum, LONG lXSpeed, LONG lYSpeed, LONG lX, LONG lY, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetPolyDoorSpeedRotation( LONG lPolyNum, LONG lSpeed, LONG lAngle, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );

// Generic polyobject commands. These work with polyobjects of any type.
void	SERVERCOMMANDS_PlayPolyobjSound( LONG lPolyNum, bool PolyMode, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_StopPolyobjSound( LONG lPolyNum, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetPolyobjPosition( LONG lPolyNum, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetPolyobjRotation( LONG lPolyNum, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );

// Miscellaneous commands. These are commands that don't fit into any other group.
void	SERVERCOMMANDS_Earthquake( AActor *pCenter, LONG lIntensity, LONG lDuration, LONG lTemorRadius, FSoundID Quakesound, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SyncJoinQueue( ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_PushToJoinQueue( ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_RemoveFromJoinQueue( unsigned int index, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_DoScroller( LONG lType, LONG lXSpeed, LONG lYSpeed, LONG lAffectee, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetScroller( LONG lType, LONG lXSpeed, LONG lYSpeed, LONG lTag, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetWallScroller( LONG lId, LONG lSidechoice, LONG lXSpeed, LONG lYSpeed, LONG lWhere, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_DoFlashFader( float fR1, float fG1, float fB1, float fA1, float fR2, float fG2, float fB2, float fA2, float fTime, ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_GenericCheat( ULONG ulPlayer, ULONG ulCheat, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetCameraToTexture( AActor *pCamera, char *pszTexture, LONG lFOV, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_CreateTranslation( ULONG ulTranslation, ULONG ulStart, ULONG ulEnd, ULONG ulPal1, ULONG ulPal2, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_CreateTranslation( ULONG ulTranslation, ULONG ulStart, ULONG ulEnd, ULONG ulR1, ULONG ulG1, ULONG ulB1, ULONG ulR2, ULONG ulG2, ULONG ulB2, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_ReplaceTextures( const char *Fromname, const char *Toname, int iTexFlags, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetSectorLink( ULONG ulSector, int iArg1, int iArg2, int iArg3, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_DoPusher( ULONG ulType, line_t *pLine, int iMagnitude, int iAngle, AActor *pSource, int iAffectee, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_AdjustPusher( int iTag, int iMagnitude, int iAngle, ULONG ulType, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_FullUpdateCompleted( ULONG ulClient );
void	SERVERCOMMANDS_SetIgnoreWeaponSelect( ULONG ulClient, const bool bIgnoreWeaponSelect );
void	SERVERCOMMANDS_ClearConsoleplayerWeapon( ULONG ulClient );
void	SERVERCOMMANDS_Lightning( ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_CancelFade( const ULONG ulPlayer, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_PlayBounceSound( const AActor *pActor, const bool bOnfloor, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_ResetMap( ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_Scroll3dMidtexture ( sector_t* sector, fixed_t move, bool ceiling, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetPlayerLogNumber ( const ULONG ulPlayer, const int Arg0, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetCVar( const FBaseCVar &CVar, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SetDefaultSkybox ( ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
void	SERVERCOMMANDS_SRPUserStartAuthentication ( const ULONG ulClient );
void	SERVERCOMMANDS_SRPUserProcessChallenge ( const ULONG ulClient );
void	SERVERCOMMANDS_SRPUserVerifySession ( const ULONG ulClient );
void	SERVERCOMMANDS_ShootDecal ( const FDecalTemplate* tpl, AActor* actor, fixed_t z, angle_t angle, fixed_t tracedist, bool permanent, ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );

#endif	// __SV_COMMANDS_H__
