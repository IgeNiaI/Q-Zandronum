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
// Filename: cooperative.cpp
//
// Description: 
//
//-----------------------------------------------------------------------------

#include "a_keys.h"
#include "cooperative.h"
#include "deathmatch.h"
#include "doomstat.h"
#include "gamemode.h"
#include "g_level.h"
#include "p_acs.h"
#include "team.h"

//*****************************************************************************
//	CONSOLE VARIABLES

CVAR( Bool, sv_coopspawnvoodoodolls, true, CVAR_SERVERINFO | CVAR_LATCH );
CVAR( Bool, sv_coopunassignedvoodoodolls, true, CVAR_SERVERINFO | CVAR_LATCH );
CVAR( Int, sv_coopunassignedvoodoodollsfornplayers, MAXPLAYERS, CVAR_SERVERINFO | CVAR_LATCH );

//*****************************************************************************
//	PRIVATE DATA DEFINITIONS

int dummyplayer = MAXPLAYERS;
TMap<FName, int> UVDpickupMap;

//*****************************************************************************
//	PROTOTYPES

void COOP_DestroyVoodooDollsOfPlayer ( const ULONG ulPlayer )
{
	// [BB] The current game mode doesn't need voodoo dolls.
	if ( COOP_VoodooDollsSelectedByGameMode() == false )
		return;

	// [BB] The map doesn't have any voodoo doll starts for this player.
	if ( AllStartsOfPlayer[ulPlayer].Size() <= 1 )
		return;

	TThinkerIterator<AActor>	Iterator;
	AActor						*pActor;
	while (( pActor = Iterator.Next( )))
	{
		// [BB] This actor doesn't belong to a player, so it can't be a voodoo doll.
		if ( pActor->player == NULL )
			continue;

		// [BB] Belongs to a different player.
		if ( static_cast<ULONG>(pActor->player - players) != ulPlayer )
			continue;

		// [BB] If we come here, we found a body belonging to the player.

		if (
			// [BB] The current player doesn't have a body assigned, so we found a voodoo doll.
			( players[ulPlayer].mo == NULL )
			// [BB] A different body is assigned to the player, so we found a voodoo doll.
			|| ( players[ulPlayer].mo != pActor )
			)
		{
			// [BB] Now that we found a voodoo doll, finally destroy it.
			pActor->Destroy();
		}
	}
}

//*****************************************************************************
//
bool COOP_PlayersVoodooDollsNeedToBeSpawned ( const ULONG ulPlayer )
{
	// [BB] The current game mode doesn't need voodoo dolls.
	if ( COOP_VoodooDollsSelectedByGameMode() == false )
		return false;

	// [BB] The map doesn't have any voodoo doll starts for this player.
	if ( AllStartsOfPlayer[ulPlayer].Size() <= 1 )
		return false;

	// [BB] When we are using unassigned voodoo dolls, all dolls for all players
	// (even those who are not in the game) are already spawned in P_SpawnThings.
	// Therefore, we don't need to spawn any dolls afterwards.
	if ( sv_coopunassignedvoodoodolls == true )
		return false;

	TThinkerIterator<AActor>	Iterator;
	AActor						*pActor;
	while (( pActor = Iterator.Next( )))
	{
		// [BB] This actor doesn't belong to a player, so it can't be a voodoo doll.
		if ( pActor->player == NULL )
			continue;

		// [BB] Belongs to a different player.
		if ( static_cast<ULONG>(pActor->player - players) !=  ulPlayer )
			continue;

		// [BB] If we come here, we found a body belonging to the player.

		if (
			// [BB] The current player doesn't have a body assigned, so we found a voodoo doll.
			( players[ulPlayer].mo == NULL )
			// [BB] A different body is assigned to the player, so we found a voodoo doll.
			|| ( players[ulPlayer].mo != pActor )
			)
		{
			// [BB] There already is a voodoo doll for the player, so we don't need to spawn it.
			return false;
		}

	}

	return true;
}

//*****************************************************************************
//
void COOP_SpawnVoodooDollsForPlayerIfNecessary ( const ULONG ulPlayer, const bool bSpawnEvenIfPlayerIsNotIngame )
{
	// [BB] Only the server spawns voodoo dolls.
	if ( NETWORK_GetState() != NETSTATE_SERVER )
		return;

	// [BB] The current game mode doesn't need voodoo dolls.
	if ( COOP_VoodooDollsSelectedByGameMode() == false )
		return;

	// [BB] The map doesn't have any voodoo doll starts for this player.
	if ( AllStartsOfPlayer[ulPlayer].Size() <= 1 )
		return;

	// [BB] In case of unassigned voodoo dolls, on some maps it may be better only
	// to spawn the voodoo dolls for the first N players.
	if ( ( sv_coopunassignedvoodoodolls == true ) && ( static_cast<LONG>(ulPlayer) >= sv_coopunassignedvoodoodollsfornplayers ) )
		return;

	// [BB] To enforce the spawning, we have to set playeringame to true.
	const bool bPlayerInGame = playeringame[ulPlayer];
	if ( bSpawnEvenIfPlayerIsNotIngame )
		playeringame[ulPlayer] = true;

	// [BB] Every start except for the last, has to spawn a voodoo doll.
	for ( ULONG ulIdx = 0; ulIdx < AllStartsOfPlayer[ulPlayer].Size() - 1; ulIdx++ )
	{
		APlayerPawn *pDoll = P_SpawnPlayer ( &(AllStartsOfPlayer[ulPlayer][ulIdx]), ulPlayer );
		// [BB] Mark the voodoo doll as spawned by the map.
		// P_SpawnPlayer won't spawn anything for a player not in game, therefore we need to check if pDoll is NULL.
		if ( pDoll )
		{
			pDoll->ulSTFlags |= STFL_LEVELSPAWNED;

			// [BB] The clients will not spawn the doll, so mark it accordingly and free it's network ID.
			pDoll->ulNetworkFlags |= NETFL_SERVERSIDEONLY;
			g_NetIDList.freeID ( pDoll->lNetID );
			pDoll->lNetID = -1;

			// [BB] If we would just set the player pointer to NULL, a lot of things wouldn't work
			// at all for the voodoo dolls (e.g. floor scrollers), so we set it do a pointer to a
			// dummy player to get past all the ( player != NULL ) checks. This will require special
			// handling wherever the code assumes that non-NULL player pointers have a valid mo.
			if ( sv_coopunassignedvoodoodolls )
				pDoll->player = &players[dummyplayer];
		}
	}

	// [BB] Now that the spawning is done, we have to restore the proper playeringame value.
	playeringame[ulPlayer] = bPlayerInGame;
	// [BB] If we have spawned the doll for a player not in the game, we have to clear a few pointers.
	// Otherwise we'll run into nasty problems after a map change.
	if ( bPlayerInGame == false )
	{
		players[ulPlayer].mo = NULL;
		players[ulPlayer].ReadyWeapon = NULL;
		players[ulPlayer].PendingWeapon = NULL;
	}
}

//*****************************************************************************
//
bool COOP_VoodooDollsSelectedByGameMode ( void )
{
	// [BB] Voodoo dolls are only used in coop.
	if ( !( GAMEMODE_GetCurrentFlags() & GMF_COOPERATIVE ) )
		return false;

	// [BB] Only use them if the compat flag tells us to.
	if ( sv_coopspawnvoodoodolls == false )
		return false;

	return true;
}

//*****************************************************************************
//
void COOP_InitVoodooDollDummyPlayer ( void )
{
	players[dummyplayer].userinfo.Reset();
}

//*****************************************************************************
//
const player_t* COOP_GetVoodooDollDummyPlayer ( void )
{
	return &players[dummyplayer];
}

//*****************************************************************************
//
void COOP_PotentiallyStoreUVDPickup ( const PClass *pType )
{
	// [BB] The current game mode doesn't need voodoo dolls, so no need to store any pickups.
	if ( COOP_VoodooDollsSelectedByGameMode() == false )
		return;

	// [BB] There is no ingame joining in such gamemodes, so no need to store any pickups.
	if ( GAMEMODE_GetCurrentFlags() & GMF_MAPRESETS )
		return;

	// [BB] Nothing to store.
	if ( pType == NULL )
		return;

	// [BB] We only store weapons and keys since they might be crucial to finish a map.
	if ( ( pType->IsDescendantOf( RUNTIME_CLASS( AWeapon )) == false )
		&& ( pType->IsDescendantOf( RUNTIME_CLASS( AKey )) == false ) )
		return;

	const FName pickupName = pType->TypeName.GetChars();
	if ( UVDpickupMap.CheckKey( pickupName ) == NULL )
		UVDpickupMap.Insert( pickupName, 1 );
}

//*****************************************************************************
//
void COOP_ClearStoredUVDPickups ( )
{
	UVDpickupMap.Clear();
}

//*****************************************************************************
//
void COOP_GiveStoredUVDPickupsToPlayer ( const ULONG ulPlayer )
{
	const TMap<FName, int>::Pair *pair;
	TMap<FName, int>::ConstIterator mapit ( UVDpickupMap );
	while ( mapit.NextPair (pair) )
	{
		const PClass *pType = PClass::FindClass ( pair->Key.GetChars() );
		if ( pType )
			DoGiveInv ( players[ulPlayer].mo, pType, 1 );
	}
}

//*****************************************************************************
//	CONSOLE COMMANDS/VARIABLES

// [BB] Since cooperative defaults to true and its default value sets deathmatch
// and teamgame to false, we have to initialize it last.
CUSTOM_CVAR( Bool, cooperative, true, CVAR_SERVERINFO | CVAR_LATCH | CVAR_CAMPAIGNLOCK | CVAR_NOINITCALL )
{
	UCVarValue	Val;

	if ( self == true )
	{
		Val.Bool = false;

		// Disable deathmatch and teamgame if we're playing cooperative.
		if ( deathmatch != false )
			deathmatch.ForceSet( Val, CVAR_Bool );
		if ( teamgame != false )
			teamgame.ForceSet( Val, CVAR_Bool );
	}
	else
	{
		Val.Bool = false;

		// Cooperative, so disable all related modes.
		survival.ForceSet( Val, CVAR_Bool );
		invasion.ForceSet( Val, CVAR_Bool );
	}

	// Reset what the current game mode is.
	GAMEMODE_DetermineGameMode( );
}

//*****************************************************************************
//
CUSTOM_CVAR( Bool, survival, false, CVAR_SERVERINFO | CVAR_LATCH | CVAR_CAMPAIGNLOCK )
{
	UCVarValue	Val;

	if ( self == true )
	{
		Val.Bool = true;

		// Enable cooperative.
		cooperative.ForceSet( Val, CVAR_Bool );

		Val.Bool = false;

		// Disable other modes.
		invasion.ForceSet( Val, CVAR_Bool );
	}

	// Reset what the current game mode is.
	GAMEMODE_DetermineGameMode( );
}

//*****************************************************************************
//
CUSTOM_CVAR( Bool, invasion, false, CVAR_SERVERINFO | CVAR_LATCH | CVAR_CAMPAIGNLOCK )
{
	UCVarValue	Val;

	if ( self == true )
	{
		Val.Bool = true;

		// Enable cooperative.
		cooperative.ForceSet( Val, CVAR_Bool );

		Val.Bool = false;

		// Disable other modes.
		survival.ForceSet( Val, CVAR_Bool );
	}

	// Reset what the current game mode is.
	GAMEMODE_DetermineGameMode( );
}

//*****************************************************************************
//*****************************************************************************
//	CONSOLE VARIABLES

