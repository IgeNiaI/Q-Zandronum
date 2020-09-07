//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2003-2005 Brad Carney
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
// Date created:  9/20/05
//
//
// Filename: invasion.cpp
//
// Description: 
//
//	Quick note on how all the spawns work:
//
//		arg[0]: This is the number of items to spawn in the first wave. The amount per wave increases on each successive wave.
//		arg[1]: This is the number of ticks between spawnings of this item (35 = one second).
//		arg[2]: This delays the spawn of this item from the start of the round this number of ticks. (NOTE: If this is set to 255, the monster will spawn after all other monsters are killed).
//		arg[3]: This is the first wave this item should spawn in (this can be left at 0).
//		arg[4]: This is the maximum number of items that can be spawned from this spawner in a round.
//
//-----------------------------------------------------------------------------

#include "actor.h"
#include "announcer.h"
#include "cl_demo.h"
#include "cooperative.h"
#include "doomstat.h"
#include "g_game.h"
#include "gi.h"
#include "g_level.h"
#include "i_system.h"
#include "invasion.h"
#include "m_png.h"
#include "m_random.h"
#include "network.h"
#include "p_local.h"
#include "sbar.h"
#include "sv_commands.h"
#include "thingdef/thingdef.h"
#include "v_video.h"
#include "survival.h"
#include "gamemode.h"
#include "farchive.h"

void	SERVERCONSOLE_UpdateScoreboard( );

//*****************************************************************************
//	PROTOTYPES

static	ULONG		invasion_GetNumThingsThisWave( ULONG ulNumOnFirstWave, ULONG ulWave, bool bMonster );

//*****************************************************************************
//	VARIABLES

// [EP] TODO: remove the 'ul' mark from the name of the variables which aren't ULONG anymore
static	unsigned int		g_ulCurrentWave = 0;
static	unsigned int		g_ulNumMonstersLeft = 0;
static	unsigned int		g_ulNumArchVilesLeft = 0;
static	unsigned int		g_ulInvasionCountdownTicks = 0;
static	INVASIONSTATE_e		g_InvasionState;
static	unsigned int		g_ulNumBossMonsters = 0;
static	bool				g_bIncreaseNumMonstersOnSpawn = true;
static	std::vector<AActor*> g_MonsterCorpsesFromPreviousWave;

//*****************************************************************************
//	STRUCTURES

FName ABaseMonsterInvasionSpot::GetSpawnName( void )
{
	return NAME_None;
}

//*****************************************************************************
//
void ABaseMonsterInvasionSpot::Activate( AActor *pActivator )
{
	flags2 &= ~MF2_DORMANT;
}

//*****************************************************************************
//
void ABaseMonsterInvasionSpot::Deactivate( AActor *pActivator )
{
	flags2 |= MF2_DORMANT;
}

//*****************************************************************************
//
void ABaseMonsterInvasionSpot::Tick( void )
{
	AActor	*pActor;
	AActor	*pFog;

	// Don't do anything unless the round is in progress.
	if (( g_InvasionState != IS_INPROGRESS ) && ( g_InvasionState != IS_BOSSFIGHT ))
		return;

	// This isn't handled client-side.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	if (( bIsBossMonster ) &&
		( g_InvasionState != IS_BOSSFIGHT ))
	{
		// Not time to spawn this yet.
		if ( g_ulNumMonstersLeft > g_ulNumBossMonsters )
			return;

		// Since this a boss monster, potentially set the invasion state to fighting a boss.
		INVASION_SetState( IS_BOSSFIGHT );
	}

	// Are we ticking down to the next spawn?
	if ( lNextSpawnTick > 0 )
		lNextSpawnTick--;

	// Next spawn tick is 0! Time to spawn something.
	if ( lNextSpawnTick == 0 )
	{
		// Spawn the item.
		g_bIncreaseNumMonstersOnSpawn = false;
		pActor = Spawn( GetSpawnName( ), x, y, z, ALLOW_REPLACE );
		g_bIncreaseNumMonstersOnSpawn = true;

		// If the item spawned in a "not so good" location, delete it and try again next tick.
		if ( P_TestMobjLocation( pActor ) == false )
		{
			// If this is a monster, subtract it from the total monster count, because it
			// was added when it spawned.
			if ( pActor->flags & MF_COUNTKILL )
				level.total_monsters--;

			// Same for items.
			if ( pActor->flags & MF_COUNTITEM )
				level.total_items--;

			pActor->Destroy( );
			return;
		}

		// [BB] The just spawned thing doesn't count as kill. Since the invasion code assumes
		// that each successful spawn of a monster spawner is a hostile monster, we need to
		// adapt the monster count here.
		if ( !pActor->CountsAsKill() )
			INVASION_UpdateMonsterCount( pActor, true );

		// Since the actor successfully spawned, tell clients.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SpawnThing( pActor );

		// The item successfully spawned. Now, there's one less items that have to spawn.
		lNumLeftThisWave--;

		// If there's still something left to spawn set the number of ticks left to spawn the next one.
		if ( lNumLeftThisWave > 0 )
			lNextSpawnTick = this->args[1] * TICRATE;
		// Otherwise, we're done spawning things this round.
		else
			lNextSpawnTick = -1;

		// Set the proper angle, and spawn the teleport fog.
		pActor->angle = angle;
		if (( this->flags & MF_AMBUSH ) == false )
		{
			pFog = Spawn<ATeleportFog>( x, y, z + TELEFOGHEIGHT, ALLOW_REPLACE );

			// If we're the server, tell clients to spawn the fog.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SpawnThing( pFog );
		}
		pActor->pMonsterSpot = this;
		pActor->ulInvasionWave = g_ulCurrentWave;
	}
}

//*****************************************************************************
//
void ABaseMonsterInvasionSpot::Serialize( FArchive &arc )
{
	Super::Serialize( arc );
	arc << lNextSpawnTick << lNumLeftThisWave;
}

//*****************************************************************************
IMPLEMENT_CLASS( ABaseMonsterInvasionSpot )

//*****************************************************************************
//*****************************************************************************
class ACustomMonsterInvasionSpot : public ABaseMonsterInvasionSpot
{
	DECLARE_CLASS( ACustomMonsterInvasionSpot, ABaseMonsterInvasionSpot )
public:

	virtual FName	GetSpawnName( void );
};

//*****************************************************************************
//
FName ACustomMonsterInvasionSpot::GetSpawnName( void )
{
	ULONG		ulNumDropItems;
	FDropItem	*pDropItem;

	ulNumDropItems = 0;

	pDropItem = this->GetDropItems();
	while ( pDropItem )
	{
		if ( pDropItem->Name != NAME_None )
			ulNumDropItems++;

		pDropItem = pDropItem->Next;
	}

	if ( ulNumDropItems == 0 )
		I_Error( "Custom monster invasion spot has no defined monsters!" );

	ulNumDropItems = M_Random( ) % ( ulNumDropItems );
	
	pDropItem = this->GetDropItems();
	while ( pDropItem )
	{
		if ( pDropItem->Name != NAME_None )
		{
			if ( ulNumDropItems == 0 )
				return pDropItem->Name;
			else
				ulNumDropItems--;
		}

		pDropItem = pDropItem->Next;
	}

	return NAME_None;
}

//*****************************************************************************
IMPLEMENT_CLASS( ACustomMonsterInvasionSpot )

//*****************************************************************************
//*****************************************************************************
//
FName ABasePickupInvasionSpot::GetSpawnName( void )
{
	return NAME_None;
}

//*****************************************************************************
//
void ABasePickupInvasionSpot::Activate( AActor *pActivator )
{
	flags2 &= ~MF2_DORMANT;
}

//*****************************************************************************
//
void ABasePickupInvasionSpot::Deactivate( AActor *pActivator )
{
	flags2 |= MF2_DORMANT;
}

//*****************************************************************************
//
void ABasePickupInvasionSpot::Destroy( )
{
	AActor						*pActor;
	TThinkerIterator<AActor>	Iterator;

	// [BB] The pickups spawned by this spot may no longer retain a pointer to the
	// destroyed spot.
	while (( pActor = Iterator.Next( )))
	{
	if ( pActor->pPickupSpot == this)
		pActor->pPickupSpot = NULL;
	}

	Super::Destroy ();
}

//*****************************************************************************
//
void ABasePickupInvasionSpot::Tick( void )
{
	AActor			*pActor;
	AActor			*pFog;
	const PClass	*pClass;

	// Don't do anything unless the round is in progress.
	if (( g_InvasionState != IS_INPROGRESS ) &&
		( g_InvasionState != IS_BOSSFIGHT ) &&
		( g_InvasionState != IS_WAVECOMPLETE ) &&
		( g_InvasionState != IS_COUNTDOWN ))
	{
		return;
	}

	// This isn't handled client-side.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	// Are we ticking down to the next spawn?
	if ( lNextSpawnTick > 0 )
		lNextSpawnTick--;

	// Next spawn tick is 0! Time to spawn something.
	if ( lNextSpawnTick == 0 )
	{
		// Spawn the item.
		pActor = Spawn( GetSpawnName( ), x, y, z, ALLOW_REPLACE );

		// Since the actor successfully spawned, tell clients.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SpawnThing( pActor );

		// The item successfully spawned. Now, there's one less items that have to spawn.
		// In server mode, don't decrement how much of this item is left if it's a type of
		// ammunition, since we want ammo to spawn forever.
		pClass = PClass::FindClass( GetSpawnName( ));
		if (( NETWORK_GetState( ) != NETSTATE_SERVER ) ||
			( pClass == NULL ) ||
			( pClass->IsDescendantOf( RUNTIME_CLASS( AAmmo )) == false ))
		{
			lNumLeftThisWave--;
		}

		// Don't spawn anything until this item has been picked up.
		lNextSpawnTick = -1;

		// Set the proper angle, and spawn the teleport fog.
		pActor->angle = angle;
		if (( this->flags & MF_AMBUSH ) == false )
		{
			pFog = Spawn<ATeleportFog>( x, y, z + TELEFOGHEIGHT, ALLOW_REPLACE );

			// If we're the server, tell clients to spawn the fog.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SpawnThing( pFog );
		}
		pActor->pPickupSpot = this;
	}
}

//*****************************************************************************
//
void ABasePickupInvasionSpot::Serialize( FArchive &arc )
{
	Super::Serialize( arc );
	arc << lNextSpawnTick << lNumLeftThisWave;
}

//*****************************************************************************
//
void ABasePickupInvasionSpot::PickedUp( void )
{
	const PClass	*pClass;

	// If there's still something left to spawn, or if we're the server and this is a type
	// of ammo, set the number of ticks left to spawn the next one.
	pClass = PClass::FindClass( GetSpawnName( ));
	if (( lNumLeftThisWave > 0 ) ||
		(( NETWORK_GetState( ) == NETSTATE_SERVER ) &&
		 ( pClass ) &&
		 ( pClass->IsDescendantOf( RUNTIME_CLASS( AAmmo )))))
	{
		if ( this->args[1] == 0 )
			lNextSpawnTick = 2 * TICRATE;
		else
			lNextSpawnTick = this->args[1] * TICRATE;
	}
	// Otherwise, we're done spawning things this round.
	else
		lNextSpawnTick = -1;
}

//*****************************************************************************
IMPLEMENT_CLASS( ABasePickupInvasionSpot )

//*****************************************************************************
//*****************************************************************************
class ACustomPickupInvasionSpot : public ABasePickupInvasionSpot
{
	DECLARE_CLASS( ACustomPickupInvasionSpot, ABasePickupInvasionSpot )
public:

	virtual FName	GetSpawnName( void );
};

//*****************************************************************************
//
FName ACustomPickupInvasionSpot::GetSpawnName( void )
{
	ULONG		ulNumDropItems;
	FDropItem	*pDropItem;

	ulNumDropItems = 0;

	pDropItem = this->GetDropItems();
	while ( pDropItem )
	{
		if ( pDropItem->Name != NAME_None )
			ulNumDropItems++;

		pDropItem = pDropItem->Next;
	}

	if ( ulNumDropItems == 0 )
		I_Error( "Custom monster invasion spot has no defined monsters!" );

	ulNumDropItems = M_Random( ) % ( ulNumDropItems );
	
	pDropItem = this->GetDropItems();
	while ( pDropItem )
	{
		if ( pDropItem->Name != NAME_None )
		{
			if ( ulNumDropItems == 0 )
				return pDropItem->Name;
			else
				ulNumDropItems--;
		}

		pDropItem = pDropItem->Next;
	}

	return NAME_None;
}

//*****************************************************************************
IMPLEMENT_CLASS( ACustomPickupInvasionSpot )

//*****************************************************************************
//*****************************************************************************
//
FName ABaseWeaponInvasionSpot::GetSpawnName( void )
{
	return NAME_None;
}

//*****************************************************************************
//
void ABaseWeaponInvasionSpot::Activate( AActor *pActivator )
{
	flags2 &= ~MF2_DORMANT;
}

//*****************************************************************************
//
void ABaseWeaponInvasionSpot::Deactivate( AActor *pActivator )
{
	flags2 |= MF2_DORMANT;
}

//*****************************************************************************
//
void ABaseWeaponInvasionSpot::Tick( void )
{
	AActor	*pActor;
	AActor	*pFog;

	// Don't do anything unless the round is in progress.
	if (( g_InvasionState != IS_INPROGRESS ) &&
		( g_InvasionState != IS_BOSSFIGHT ) &&
		( g_InvasionState != IS_WAVECOMPLETE ) &&
		( g_InvasionState != IS_COUNTDOWN ))
	{
		return;
	}

	// This isn't handled client-side.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	// Are we ticking down to the next spawn?
	if ( lNextSpawnTick > 0 )
		lNextSpawnTick--;

	// Next spawn tick is 0! Time to spawn something.
	if ( lNextSpawnTick == 0 )
	{
		// Spawn the item.
		pActor = Spawn( GetSpawnName( ), x, y, z, ALLOW_REPLACE );

		// Make this item stick around in multiplayer games.
		pActor->flags &= ~MF_DROPPED;

		// Since the actor successfully spawned, tell clients.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SpawnThing( pActor );

		// Don't spawn anything until this item has been picked up.
		lNextSpawnTick = -1;

		// Set the proper angle, and spawn the teleport fog.
		pActor->angle = angle;
		if (( this->flags & MF_AMBUSH ) == false )
		{
			pFog = Spawn<ATeleportFog>( x, y, z + TELEFOGHEIGHT, ALLOW_REPLACE );

			// If we're the server, tell clients to spawn the fog.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SpawnThing( pFog );
		}

		// Destroy the weapon spot since it doesn't need to persist.
//		this->Destroy( );
	}
}

//*****************************************************************************
//
void ABaseWeaponInvasionSpot::Serialize( FArchive &arc )
{
	Super::Serialize( arc );
	arc << lNextSpawnTick;
}

//*****************************************************************************
IMPLEMENT_CLASS( ABaseWeaponInvasionSpot )

//*****************************************************************************
class ACustomWeaponInvasionSpot : public ABaseWeaponInvasionSpot
{
	DECLARE_CLASS( ACustomWeaponInvasionSpot, ABaseWeaponInvasionSpot )
public:

	virtual FName	GetSpawnName( void );
};

//*****************************************************************************
//
FName ACustomWeaponInvasionSpot::GetSpawnName( void )
{
	ULONG		ulNumDropItems;
	FDropItem	*pDropItem;

	ulNumDropItems = 0;

	pDropItem = this->GetDropItems();
	while ( pDropItem )
	{
		if ( pDropItem->Name != NAME_None )
			ulNumDropItems++;

		pDropItem = pDropItem->Next;
	}

	if ( ulNumDropItems == 0 )
		I_Error( "Custom monster invasion spot has no defined monsters!" );

	ulNumDropItems = M_Random( ) % ( ulNumDropItems );
	
	pDropItem = this->GetDropItems();
	while ( pDropItem )
	{
		if ( pDropItem->Name != NAME_None )
		{
			if ( ulNumDropItems == 0 )
				return pDropItem->Name;
			else
				ulNumDropItems--;
		}

		pDropItem = pDropItem->Next;
	}

	return NAME_None;
}

//*****************************************************************************
IMPLEMENT_CLASS( ACustomWeaponInvasionSpot )

//*****************************************************************************
//	FUNCTIONS

void INVASION_Construct( void )
{
	g_InvasionState = IS_WAITINGFORPLAYERS;
}

//*****************************************************************************
//
void INVASION_Tick( void )
{
	// Not in invasion mode.
	if ( invasion == false )
		return;

	// Don't tick if the game is paused.
	if ( paused )
		return;

	switch ( g_InvasionState )
	{
	case IS_WAITINGFORPLAYERS:

		if ( NETWORK_InClientMode() )
		{
			break;
		}

		// A player is here! Begin the countdown!
		if ( SERVER_CalcNumNonSpectatingPlayers( MAXPLAYERS ) >= 1 )
		{
			if ( sv_invasioncountdowntime > 0 )
				INVASION_StartFirstCountdown(( sv_invasioncountdowntime * TICRATE ) - 1 );
			else
				INVASION_StartFirstCountdown(( 10 * TICRATE ) - 1 );
		}
		break;
	case IS_FIRSTCOUNTDOWN:

		if ( g_ulInvasionCountdownTicks )
		{
			g_ulInvasionCountdownTicks--;

			// FIGHT!
			if (( g_ulInvasionCountdownTicks == 0 ) &&
				( NETWORK_InClientMode() == false ))
			{
				INVASION_BeginWave( 1 );
			}
			// Play "3... 2... 1..." sounds.
			else if ( g_ulInvasionCountdownTicks == ( 3 * TICRATE ))
				ANNOUNCER_PlayEntry( cl_announcer, "Three" );
			else if ( g_ulInvasionCountdownTicks == ( 2 * TICRATE ))
				ANNOUNCER_PlayEntry( cl_announcer, "Two" );
			else if ( g_ulInvasionCountdownTicks == ( 1 * TICRATE ))
				ANNOUNCER_PlayEntry( cl_announcer, "One" );
		}
		break;
	case IS_WAVECOMPLETE:

		if ( g_ulInvasionCountdownTicks )
		{
			g_ulInvasionCountdownTicks--;

			if (( g_ulInvasionCountdownTicks == 0 ) &&
				( NETWORK_InClientMode() == false ))
			{
				if ( (LONG)g_ulCurrentWave == wavelimit )
					G_ExitLevel( 0, false );
				else
				{
					// FIGHT!
					if ( sv_invasioncountdowntime > 0 )
						INVASION_StartCountdown(( sv_invasioncountdowntime * TICRATE ) - 1 );
					else
						INVASION_StartCountdown(( 10 * TICRATE ) - 1 );
				}
			}
		}
		break;
	case IS_COUNTDOWN:

		if ( g_ulInvasionCountdownTicks )
		{
			g_ulInvasionCountdownTicks--;

			// FIGHT!
			if (( g_ulInvasionCountdownTicks == 0 ) &&
				( NETWORK_InClientMode() == false ))
			{
				INVASION_BeginWave( g_ulCurrentWave + 1 );
			}
			// Play "3... 2... 1..." sounds.
			else if ( g_ulInvasionCountdownTicks == ( 3 * TICRATE ))
				ANNOUNCER_PlayEntry( cl_announcer, "Three" );
			else if ( g_ulInvasionCountdownTicks == ( 2 * TICRATE ))
				ANNOUNCER_PlayEntry( cl_announcer, "Two" );
			else if ( g_ulInvasionCountdownTicks == ( 1 * TICRATE ))
				ANNOUNCER_PlayEntry( cl_announcer, "One" );
		}
		break;
	case IS_INPROGRESS:
	case IS_BOSSFIGHT:
		if ( NETWORK_InClientMode() )
		{
			break;
		}

		// [BB] If everyone is dead, the mission has failed!
		// [BB] Invasion with (sv_maxlives == 0) allows unlimited respawns.
		if ( ( sv_maxlives > 0 ) && ( GAME_CountLivingAndRespawnablePlayers( ) == 0 ) )
		{
			// Put the game state in the mission failed state.
			INVASION_SetState( IS_MISSIONFAILED );

			// Pause for five seconds for the failed sequence.
			GAME_SetEndLevelDelay( 5 * TICRATE );
		}
	default:
		break;	
	}
}

//*****************************************************************************
//
void INVASION_StartFirstCountdown( ULONG ulTicks )
{
	// Put the invasion state into the first countdown.
	if ( NETWORK_InClientMode() == false )
	{
		INVASION_SetState( IS_FIRSTCOUNTDOWN );
	}

	// Set the invasion countdown ticks.
	INVASION_SetCountdownTicks( ulTicks );

	// Announce that the fight will soon start.
	ANNOUNCER_PlayEntry( cl_announcer, "PrepareToFight" );

	// [EP] Clear all the HUD messages.
	if ( StatusBar )
		StatusBar->DetachAllMessages();

	// Tell clients to start the countdown.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DoGameModeCountdown( ulTicks );

	// Reset the map.
	GAME_ResetMap( false );
	// [BB] Since the map reset possibly alters floor heights, players may get
	// stuck if we don't respawn them now.
	GAMEMODE_RespawnAllPlayers();
	GAMEMODE_ResetPlayersKillCount( true );

	// [BB] To properly handle respawning of the consoleplayer in single player
	// we need to put the game into a "fake multiplayer" mode.
	if ( (NETWORK_GetState( ) == NETSTATE_SINGLE) && sv_maxlives > 0 )
		NETWORK_SetState( NETSTATE_SINGLE_MULTIPLAYER );
}

//*****************************************************************************
//
void INVASION_StartCountdown( ULONG ulTicks )
{
	AActor						*pActor;
	TThinkerIterator<AActor>	ActorIterator;

	// Put the invasion state into the first countdown.
	if ( NETWORK_InClientMode() == false )
	{
		INVASION_SetState( IS_COUNTDOWN );
	}

	// Set the invasion countdown ticks.
	INVASION_SetCountdownTicks( ulTicks );

	// Also, clear out dead bodies from two rounds ago.
//	if ( g_ulCurrentWave > 1 )
	{
		// [BB] The monster corpses from two waves ago will be removed below anyway,
		// so we can clear this vector.
		if ( NETWORK_InClientMode() == false )
		{
			g_MonsterCorpsesFromPreviousWave.clear();
		}

		while (( pActor = ActorIterator.Next( )))
		{
			// Skip friendly or dormant actors.
			if ((( pActor->flags & MF_COUNTKILL ) == false ) ||
				( pActor->flags & MF_FRIENDLY ) ||
				( pActor->flags2 & MF2_DORMANT ))
			{
				continue;
			}

			// [BB] Get rid of any hostile monsters that are still alive. This should only
			// happen on buggy maps, nevertheless we have to catch this case to prevent
			// sync problems between client and server.
			if ( pActor->health > 0 )
			{
				// [BB] Let the server handle this buggy case.
				if ( NETWORK_InClientMode() == false )
				{
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
						SERVERCOMMANDS_DestroyThing( pActor );

					pActor->Destroy( );
				}
				continue;
			}

			// Get rid of any bodies that didn't come from a spawner.
			// [BB] Clients don't know by which monster spot something was spawned.
			if (( pActor->pMonsterSpot == NULL ) &&
				( NETWORK_InClientMode() == false ))
			{
				pActor->Destroy( );
				continue;
			}

			// Also, get rid of any bodies from previous waves.
			// [BB] Clients just slap the ulInvasionWave value to everything spawned in CLIENT_SpawnThing,
			// therefore they know what to do here.
			if (( g_ulCurrentWave > 1 ) &&
				( pActor->ulInvasionWave == ( g_ulCurrentWave - 1 )))
			{
				pActor->Destroy( );
				continue;
			}

			// [BB] Build a vector containing all pointers to corpses from the wave one round ago.
			if (( pActor->ulInvasionWave == g_ulCurrentWave ) &&
				( NETWORK_InClientMode() == false ))
			{
				g_MonsterCorpsesFromPreviousWave.push_back( pActor );
				continue;
			}
		}
	}

	// Announce that the fight will soon start.
	ANNOUNCER_PlayEntry( cl_announcer, "PrepareToFight" );

	// Tell clients to start the countdown.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DoGameModeCountdown( ulTicks );
}

//*****************************************************************************
//
void INVASION_BeginWave( ULONG ulWave )
{
	DHUDMessageFadeOut							*pMsg;
	AActor										*pActor;
	AActor										*pFog;
	ABaseMonsterInvasionSpot					*pMonsterSpot;
	ABasePickupInvasionSpot						*pPickupSpot;
	ABaseWeaponInvasionSpot						*pWeaponSpot;
	ULONG										ulNumMonstersLeft;
	LONG										lWaveForItem;
	bool										bContinue;
	TThinkerIterator<ABaseMonsterInvasionSpot>	MonsterIterator;
	TThinkerIterator<ABasePickupInvasionSpot>	PickupIterator;
	TThinkerIterator<ABaseWeaponInvasionSpot>	WeaponIterator;
	TThinkerIterator<AActor>					ActorIterator;

	// We're now in the middle of the invasion!
	if ( NETWORK_InClientMode() == false )
	{
		INVASION_SetState( IS_INPROGRESS );
	}

	// Make sure this is 0. Can be non-zero in network games if they're slightly out of sync.
	g_ulInvasionCountdownTicks = 0;

	// Tell clients to "fight!".
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DoGameModeFight( ulWave );

	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		// Play fight sound.
		ANNOUNCER_PlayEntry( cl_announcer, "Fight" );

		// Display "BEGIN!" HUD message.
		pMsg = new DHUDMessageFadeOut( BigFont, "BEGIN!",
			160.4f,
			75.0f,
			320,
			200,
			CR_RED,
			2.0f,
			1.0f );

		StatusBar->AttachMessage( pMsg, MAKE_ID('C','N','T','R') );
	}
	// Display a little thing in the server window so servers can know when waves begin.
	else
		Printf( "BEGIN!\n" );

	g_ulCurrentWave = ulWave;
	g_ulNumMonstersLeft = 0;
	g_ulNumArchVilesLeft = 0;
	g_ulNumBossMonsters = 0;

	// Clients don't need to do any more.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	while (( pMonsterSpot = MonsterIterator.Next( )))
	{
		// Spot is dormant. Don't do anything with it this round.
		if ( pMonsterSpot->flags2 & MF2_DORMANT )
		{
			pMonsterSpot->lNextSpawnTick = -1;
			continue;
		}

		// Not time to spawn this item yet!
		if ( g_ulCurrentWave < (ULONG)pMonsterSpot->args[3] )
		{
			pMonsterSpot->lNextSpawnTick = -1;
			continue;
		}

		lWaveForItem = g_ulCurrentWave - (( pMonsterSpot->args[3] <= 1 ) ? 0 : ( pMonsterSpot->args[3] - 1 ));

		// This item will be used this wave. Calculate the number of things to spawn.
		pMonsterSpot->lNumLeftThisWave = invasion_GetNumThingsThisWave( pMonsterSpot->args[0], lWaveForItem, true );
		if (( pMonsterSpot->args[4] > 0 ) && ( pMonsterSpot->lNumLeftThisWave > pMonsterSpot->args[4] ))
			pMonsterSpot->lNumLeftThisWave = pMonsterSpot->args[4];

		g_ulNumMonstersLeft += pMonsterSpot->lNumLeftThisWave;
		if ( stricmp( pMonsterSpot->GetSpawnName( ), "Archvile" ) == 0 )
			g_ulNumArchVilesLeft += pMonsterSpot->lNumLeftThisWave;

		// Nothing to spawn!
		if ( pMonsterSpot->lNumLeftThisWave <= 0 )
		{
			pMonsterSpot->lNextSpawnTick = -1;
			continue;
		}

		// If this item has a delayed spawn set, just set the ticks until it spawns, and don't
		// spawn it.
		if ( pMonsterSpot->args[2] > 0 )
		{
			// Special exception: if this is set to 255, don't spawn it until everything else is dead.
			if ( pMonsterSpot->args[2] == 255 )
			{
				pMonsterSpot->bIsBossMonster = true;
				pMonsterSpot->lNextSpawnTick = 0;

				g_ulNumBossMonsters += pMonsterSpot->lNumLeftThisWave;
			}
			else
				pMonsterSpot->lNextSpawnTick = pMonsterSpot->args[2] * TICRATE;
			continue;
		}

		// Spawn the item.
		ulNumMonstersLeft = g_ulNumMonstersLeft;
		pActor = Spawn( pMonsterSpot->GetSpawnName( ), pMonsterSpot->x, pMonsterSpot->y, pMonsterSpot->z, ALLOW_REPLACE );
		g_ulNumMonstersLeft = ulNumMonstersLeft;

		// If the item spawned in a "not so good" location, delete it and try again next tick.
		if ( P_TestMobjLocation( pActor ) == false )
		{
			// If this is a monster, subtract it from the total monster count, because it
			// was added when it spawned.
			if ( pActor->flags & MF_COUNTKILL )
				level.total_monsters--;

			// Same for items.
			if ( pActor->flags & MF_COUNTITEM )
				level.total_items--;

			pActor->Destroy( );
			pMonsterSpot->lNextSpawnTick = 0;
			continue;
		}

		// [BB] The just spawned thing doesn't count as kill. Since the invasion code assumes
		// that each successful spawn of a monster spawner is a hostile monster, we need to
		// adapt the monster count here.
		if ( !pActor->CountsAsKill() )
			INVASION_UpdateMonsterCount( pActor, true );

		// Since the actor successfully spawned, tell clients.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SpawnThing( pActor );

		// The item successfully spawned. Now, there's one less items that have to spawn.
		pMonsterSpot->lNumLeftThisWave--;

		// If there's still something left to spawn set the number of ticks left to spawn the next one.
		if ( pMonsterSpot->lNumLeftThisWave > 0 )
			pMonsterSpot->lNextSpawnTick = pMonsterSpot->args[1] * TICRATE;
		// Otherwise, we're done spawning things this round.
		else
			pMonsterSpot->lNextSpawnTick = -1;

		// Set the proper angle, and spawn the teleport fog.
		pActor->angle = pMonsterSpot->angle;
		if (( pMonsterSpot->flags & MF_AMBUSH ) == false )
		{
			pFog = Spawn<ATeleportFog>( pMonsterSpot->x, pMonsterSpot->y, pMonsterSpot->z + TELEFOGHEIGHT, ALLOW_REPLACE );

			// If we're the server, tell clients to spawn the fog.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SpawnThing( pFog );
		}
		pActor->pMonsterSpot = pMonsterSpot;
		pActor->ulInvasionWave = g_ulCurrentWave;
	}

	while (( pPickupSpot = PickupIterator.Next( )))
	{
		// Spot is dormant. Don't do anything with it this round.
		if ( pPickupSpot->flags2 & MF2_DORMANT )
		{
			pPickupSpot->lNextSpawnTick = -1;
			continue;
		}

		// Not time to spawn this item yet!
		if ( g_ulCurrentWave < (ULONG)pPickupSpot->args[3] )
		{
			pPickupSpot->lNextSpawnTick = -1;
			continue;
		}

		lWaveForItem = g_ulCurrentWave - (( pPickupSpot->args[3] <= 1 ) ? 0 : ( pPickupSpot->args[3] - 1 ));

		// This item will be used this wave. Calculate the number of things to spawn.
		pPickupSpot->lNumLeftThisWave = invasion_GetNumThingsThisWave( pPickupSpot->args[0], lWaveForItem, false );
		if (( pPickupSpot->args[4] > 0 ) && ( pPickupSpot->lNumLeftThisWave > pPickupSpot->args[4] ))
			pPickupSpot->lNumLeftThisWave = pPickupSpot->args[4];

		// Nothing to spawn!
		if ( pPickupSpot->lNumLeftThisWave <= 0 )
		{
			pPickupSpot->lNextSpawnTick = -1;
			continue;
		}

		// If this item has a delayed spawn set, just set the ticks until it spawns, and don't
		// spawn it.
		if ( pPickupSpot->args[2] > 2 )
		{
			pPickupSpot->lNextSpawnTick = pPickupSpot->args[2] * TICRATE;
			continue;
		}

		// If there's still an item resting in this spot, then there's no need to spawn something.
		bContinue = false;
		ActorIterator.Reinit( );
		while (( pActor = ActorIterator.Next( )))
		{
			if (( pActor->flags & MF_SPECIAL ) &&
				( pActor->pPickupSpot == pPickupSpot ))
			{
				bContinue = true;
				pPickupSpot->lNextSpawnTick = -1;
				break;
			}
		}

		if ( bContinue )
			continue;

		// Spawn the item.
		pActor = Spawn( pPickupSpot->GetSpawnName( ), pPickupSpot->x, pPickupSpot->y, pPickupSpot->z, ALLOW_REPLACE );

		// Since the actor successfully spawned, tell clients.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SpawnThing( pActor );

		// The item successfully spawned. Now, there's one less items that have to spawn.
		pPickupSpot->lNumLeftThisWave--;

		pPickupSpot->lNextSpawnTick = -1;

		// Set the proper angle, and spawn the teleport fog.
		pActor->angle = pPickupSpot->angle;
		if (( pPickupSpot->flags & MF_AMBUSH ) == false )
		{
			pFog = Spawn<ATeleportFog>( pPickupSpot->x, pPickupSpot->y, pPickupSpot->z + TELEFOGHEIGHT, ALLOW_REPLACE );

			// If we're the server, tell clients to spawn the fog.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SpawnThing( pFog );
		}
		pActor->pPickupSpot = pPickupSpot;
	}

	while (( pWeaponSpot = WeaponIterator.Next( )))
	{
		// Spot is dormant. Don't do anything with it this round.
		if ( pWeaponSpot->flags2 & MF2_DORMANT )
		{
			pWeaponSpot->lNextSpawnTick = -1;
			continue;
		}

		// Not time to spawn this item yet, or we already spawned it.
		if (( (ULONG)pWeaponSpot->args[3] != 0 ) && ( g_ulCurrentWave != (ULONG)pWeaponSpot->args[3] ))
		{
			pWeaponSpot->lNextSpawnTick = -1;
			continue;
		}

		// If this item has a delayed spawn set, just set the ticks until it spawns, and don't
		// spawn it.
		if ( pWeaponSpot->args[2] > 2 )
		{
			pWeaponSpot->lNextSpawnTick = pWeaponSpot->args[2] * TICRATE;
			continue;
		}

		// Spawn the item.
		pActor = Spawn( pWeaponSpot->GetSpawnName( ), pWeaponSpot->x, pWeaponSpot->y, pWeaponSpot->z, ALLOW_REPLACE );

		// Make this item stick around in multiplayer games.
		pActor->flags &= ~MF_DROPPED;

		// Since the actor successfully spawned, tell clients.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SpawnThing( pActor );

		pWeaponSpot->lNextSpawnTick = -1;

		// Set the proper angle, and spawn the teleport fog.
		pActor->angle = pWeaponSpot->angle;
		if (( pWeaponSpot->flags & MF_AMBUSH ) == false )
		{
			pFog = Spawn<ATeleportFog>( pWeaponSpot->x, pWeaponSpot->y, pWeaponSpot->z + TELEFOGHEIGHT, ALLOW_REPLACE );

			// If we're the server, tell clients to spawn the fog.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SpawnThing( pFog );
		}

		// Destroy the weapon spot since it doesn't need to persist.
//		pWeaponSpot->Destroy( );
	}

	// If we're the server, tell the client how many monsters are left.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetInvasionNumMonstersLeft( );
}

//*****************************************************************************
//
void INVASION_DoWaveComplete( void )
{
	// Put the invasion state in the win sequence state.
	if ( NETWORK_InClientMode() == false )
	{
		INVASION_SetState( IS_WAVECOMPLETE );
	}

	// Tell clients to do the win sequence.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DoGameModeWinSequence( 0 );

	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		char				szString[32];
		DHUDMessageFadeOut	*pMsg;

		if ( (LONG)g_ulCurrentWave == wavelimit )
			sprintf( szString, "VICTORY!" );
		else
			sprintf( szString, "WAVE %d COMPLETE!", static_cast<unsigned int> (g_ulCurrentWave) );
		V_ColorizeString( szString );

		// Display "VICTORY!"/"WAVE %d COMPLETE!" HUD message.
		pMsg = new DHUDMessageFadeOut( BigFont, szString,
			160.4f,
			75.0f,
			320,
			200,
			CR_RED,
			3.0f,
			2.0f );

		StatusBar->AttachMessage( pMsg, MAKE_ID('C','N','T','R') );
	}

	if ( NETWORK_InClientMode() == false )
	{
		INVASION_SetCountdownTicks( 5 * TICRATE );
	}
}

//*****************************************************************************
//
ULONG INVASION_GetCountdownTicks( void )
{
	return ( g_ulInvasionCountdownTicks );
}

//*****************************************************************************
//
void INVASION_SetCountdownTicks( ULONG ulTicks )
{
	g_ulInvasionCountdownTicks = ulTicks;
}

//*****************************************************************************
//
INVASIONSTATE_e INVASION_GetState( void )
{
	return ( g_InvasionState );
}

//*****************************************************************************
//
void INVASION_SetState( INVASIONSTATE_e State )
{
	AActor						*pActor;
	TThinkerIterator<AActor>	Iterator;

	g_InvasionState = State;

	switch ( State )
	{
	case IS_WAITINGFORPLAYERS:

		g_ulCurrentWave = 0;
		g_ulNumMonstersLeft = 0;
		g_ulNumArchVilesLeft = 0;
		g_ulNumBossMonsters = 0;
		g_ulInvasionCountdownTicks = 0;

		// [BB] Respawn any players who were downed during the previous round.
		// [BB] INVASION_SetState ( IS_WAITINGFORPLAYERS ) may also be called if invasion is false.
		if ( invasion )
			GAMEMODE_RespawnDeadSpectatorsAndPopQueue();

		// Nothing else to do here if we're not in invasion mode.
		if ( 1 )//( invasion == false )
			break;

		while (( pActor = Iterator.Next( )) != NULL )
		{
			// Go through and delete all the current monsters to calm things down.
			if ( pActor->flags & MF_COUNTKILL )
			{
				pActor->Destroy( );
				continue;
			}
		}

		break;
	case IS_MISSIONFAILED:
		GAMEMODE_DisplayStandardMessage ( "MISSION FAILED!" );
		break;
	default:
		break;
	}

	// Tell clients about the state change.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetGameModeState( State, g_ulInvasionCountdownTicks );
}

//*****************************************************************************
//
ULONG INVASION_GetNumMonstersLeft( void )
{
	return ( g_ulNumMonstersLeft );
}

//*****************************************************************************
//
void INVASION_SetNumMonstersLeft( ULONG ulLeft )
{
	g_ulNumMonstersLeft = ulLeft;
	if (( g_ulNumMonstersLeft == 0 ) &&
		( NETWORK_InClientMode() == false ))
	{
		INVASION_DoWaveComplete( );
	}
}

//*****************************************************************************
//
ULONG INVASION_GetNumArchVilesLeft( void )
{
	return ( g_ulNumArchVilesLeft );
}

//*****************************************************************************
//
void INVASION_SetNumArchVilesLeft( ULONG ulLeft )
{
	g_ulNumArchVilesLeft = ulLeft;
}

//*****************************************************************************
//
ULONG INVASION_GetCurrentWave( void )
{
	return ( g_ulCurrentWave );
}

//*****************************************************************************
//
void INVASION_SetCurrentWave( ULONG ulWave )
{
	g_ulCurrentWave = ulWave;
}

//*****************************************************************************
//
bool INVASION_GetIncreaseNumMonstersOnSpawn( void )
{
	return ( g_bIncreaseNumMonstersOnSpawn );
}

//*****************************************************************************
//
void INVASION_SetIncreaseNumMonstersOnSpawn( bool bIncrease )
{
	g_bIncreaseNumMonstersOnSpawn = bIncrease;
}

//*****************************************************************************
//
void INVASION_WriteSaveInfo( FILE *pFile )
{
	unsigned int				ulInvasionState;
	FPNGChunkArchive	arc( pFile, MAKE_ID( 'i','n','V','s' ));

	ulInvasionState = g_InvasionState;
	arc << g_ulNumMonstersLeft << g_ulInvasionCountdownTicks << g_ulCurrentWave << ulInvasionState << g_ulNumBossMonsters << g_ulNumArchVilesLeft;
}

//*****************************************************************************
//
void INVASION_ReadSaveInfo( PNGHandle *pPng )
{
	size_t	Length;

	Length = M_FindPNGChunk( pPng, MAKE_ID( 'i','n','V','s' ));
	if ( Length != 0 )
	{
		unsigned int		ulInvasionState;
		FPNGChunkArchive	arc( pPng->File->GetFile( ), MAKE_ID( 'i','n','V','s' ), Length );

		arc << g_ulNumMonstersLeft << g_ulInvasionCountdownTicks << g_ulCurrentWave << ulInvasionState << g_ulNumBossMonsters << g_ulNumArchVilesLeft;
		g_InvasionState = (INVASIONSTATE_e)ulInvasionState;
	}
}

//*****************************************************************************
//
// [BB] Remove one monster corpse from the previous wave.
void INVASION_RemoveMonsterCorpse( )
{
	if (( NETWORK_InClientMode() == false ) &&
		( g_MonsterCorpsesFromPreviousWave.size() > 0 ))
	{
		AActor *pCorpse = g_MonsterCorpsesFromPreviousWave.back();
		g_MonsterCorpsesFromPreviousWave.pop_back();
		// [BB] Check if the actor is still in its death state.
		// This won't be the case for example if it has been
		// raises by an Archvile in the mean time.
		// We also have to check if the pointer is still
		// valid.
		if ( pCorpse && pCorpse->InDeathState( ) )
		{
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_DestroyThing( pCorpse );
			pCorpse->Destroy();
		}
	}
}

//*****************************************************************************
//
// [BB] If a monster corpse in g_MonsterCorpsesFromPreviousWave is destroyed by
// anything besides INVASION_RemoveMonsterCorpse, we have to NULL out the
// corresponding pointer to it.
void INVASION_ClearMonsterCorpsePointer( AActor *pActor )
{
	if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
	{
		for( unsigned int i = 0; i < g_MonsterCorpsesFromPreviousWave.size(); i++ )
		{
			if ( g_MonsterCorpsesFromPreviousWave[i] == pActor )
			{
				g_MonsterCorpsesFromPreviousWave[i] = NULL;
			}
		}
	}
}

//*****************************************************************************
//
// [BB] If a monster is spawned or removed, we need to adjust the number of
// monsters (and perhaps Archviles).
// removeMonster == false  - pActor was removed
// removeMonster == true   - pActor was added
void INVASION_UpdateMonsterCount( AActor* pActor, bool removeMonster )
{
	if (( invasion ) &&
		( INVASION_GetIncreaseNumMonstersOnSpawn( )) &&
		( NETWORK_InClientMode() == false ))
	{
		INVASION_SetNumMonstersLeft( INVASION_GetNumMonstersLeft( ) + (removeMonster ? -1 : 1) );

		if ( pActor->GetClass( ) == PClass::FindClass("Archvile") )
			INVASION_SetNumArchVilesLeft( INVASION_GetNumArchVilesLeft( ) + (removeMonster ? -1 : 1) );

		// [BB] If we're the server, tell the client how many monsters are left.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetInvasionNumMonstersLeft( );
	}
}

//*****************************************************************************
//
bool INVASION_PreventPlayersFromJoining( void )
{
	// [BB] Invasion with (sv_maxlives == 0) allows unlimited respawns.
	return ( invasion && ( sv_maxlives > 0 ) && ( INVASION_GetState() != IS_WAITINGFORPLAYERS ) && ( INVASION_GetState() != IS_FIRSTCOUNTDOWN ) );
}

//*****************************************************************************
// [BB]
bool INVASION_IsMapThingInvasionSpot( const FMapThing *mthing )
{
	if ( mthing == NULL )
		return false;

	const PClass *pType = DoomEdMap.FindType( mthing->type );
	if ( pType )
	{
		pType = pType->ActorInfo->GetReplacement( )->Class;

		if (( pType->IsDescendantOf( PClass::FindClass( "BaseMonsterInvasionSpot" ))) ||
			( pType->IsDescendantOf( PClass::FindClass( "BasePickupInvasionSpot" ))) ||
			( pType->IsDescendantOf( PClass::FindClass( "BaseWeaponInvasionSpot" ))))
		{
			return true;
		}
	}
	return false;
}

//*****************************************************************************
//*****************************************************************************
//
static ULONG invasion_GetNumThingsThisWave( ULONG ulNumOnFirstWave, ULONG ulWave, bool bMonster )
{
	UCVarValue	Val;

	Val = gameskill.GetGenericRep( CVAR_Int );
	switch ( Val.Int )
	{
	case 0:
	case 1:

		if ( bMonster )
			return ( (ULONG)(( pow( 1.25, (double)(( ulWave - 1 ))) * ulNumOnFirstWave )));
		else
			return ( (ULONG)(( pow( 2, (double)(( ulWave - 1 ))) * ulNumOnFirstWave )));
		break;
	case 2:
	default:

		if ( bMonster )
			return ( (ULONG)(( pow( 1.5, (double)(( ulWave - 1 ))) * ulNumOnFirstWave )));
		else
			return ( (ULONG)(( pow( 1.75, (double)(( ulWave - 1 ))) * ulNumOnFirstWave )));
		break;
	case 3:
	case 4:

		return ( (ULONG)(( pow( 1.6225, (double)(( ulWave - 1 ))) * ulNumOnFirstWave )));
		break;
	}
/*
	ULONG	ulIdx;
	ULONG	ulNumThingsThisWave;
	ULONG	ulNumTwoWavesAgo;
	ULONG	ulNumOneWaveAgo;
	ULONG	ulAddedAmount;

	ulNumThingsThisWave = 0;
	for ( ulIdx = 1; ulIdx <= ulWave; ulIdx++ )
	{
		switch ( ulIdx )
		{
		case 1:

			ulAddedAmount = ulNumOnFirstWave;
			ulNumThingsThisWave += ulAddedAmount;
			ulNumTwoWavesAgo = ulAddedAmount;
			break;
		case 2:

			ulAddedAmount = (ULONG)( ulNumOnFirstWave * 0.5f );
			ulNumThingsThisWave += (ULONG)( ulNumOnFirstWave * 0.5f );
			ulNumOneWaveAgo = ulAddedAmount;
			break;
		default:

			ulAddedAmount = ( ulNumOneWaveAgo + ulNumTwoWavesAgo );
			ulNumThingsThisWave += ulAddedAmount;
			ulNumTwoWavesAgo = ulNumOneWaveAgo;
			ulNumOneWaveAgo = ulAddedAmount;
			break;
		}
	}

	return ( ulNumThingsThisWave );
*/
}

//*****************************************************************************
//	CONSOLE COMMANDS/VARIABLES

CVAR( Int, sv_invasioncountdowntime, 10, CVAR_ARCHIVE );
CUSTOM_CVAR( Int, wavelimit, 0, CVAR_CAMPAIGNLOCK | CVAR_SERVERINFO )
{
	if ( self >= 256 )
		self = 255;
	if ( self < 0 )
		self = 0;

	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( gamestate != GS_STARTUP ))
	{
		SERVER_Printf( "%s changed to: %d\n", self.GetName( ), (int)self );
		SERVERCOMMANDS_SetGameModeLimits( );

		// Update the scoreboard.
		SERVERCONSOLE_UpdateScoreboard( );
	}
}
CVAR( Bool, sv_usemapsettingswavelimit, true, CVAR_ARCHIVE );
