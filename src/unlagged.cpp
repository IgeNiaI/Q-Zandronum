//-----------------------------------------------------------------------------
//
// Unlagged Source
// Copyright (C) 2010 "Spleen"
// Copyright (C) 2010-2012 Skulltag Development Team
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
// Filename: unlagged.cpp
//
// Description: Contains variables and routines related to the backwards
// reconciliation.
//
//-----------------------------------------------------------------------------

#include "unlagged.h"
#include "doomstat.h"
#include "doomdef.h"
#include "actor.h"
#include "d_player.h"
#include "r_defs.h"
#include "network.h"
#include "r_state.h"
#include "p_local.h"
#include "i_system.h"
#include "sv_commands.h"
#include "cl_commands.h"
#include "templates.h"
#include "d_netinf.h"


IMPLEMENT_CLASS(ADebugUnlaggedHitbox)

void ADebugUnlaggedHitbox::PostBeginPlay()
{
	Super::PostBeginPlay();
	drawHitbox = true;
	if (target)
	{
		radius = target->radius;
		height = target->height;
	}
}

IMPLEMENT_CLASS(AUnlaggedActor)

AUnlaggedActor* firstUnlaggedActor = NULL;
AUnlaggedActor* lastUnlaggedActor = NULL;

void AUnlaggedActor::BeginPlay()
{
	Super::BeginPlay();

	if (NETWORK_GetState() == NETSTATE_SERVER) {
		if ( firstUnlaggedActor == NULL ) {
			firstUnlaggedActor = this;
		}

		previousUnlaggedActor = lastUnlaggedActor;
		nextUnlaggedActor = NULL;
		if ( lastUnlaggedActor != NULL ) {
			lastUnlaggedActor->nextUnlaggedActor = this;
		}

		lastUnlaggedActor = this;

		UNLAGGED_ResetActor( this );
		isCurrentlyBeingUnlagged = false;
		isMissile = !!(flags & MF_MISSILE);
	}
}

void AUnlaggedActor::Destroy()
{
	if (NETWORK_GetState() == NETSTATE_SERVER) {
		if ( previousUnlaggedActor != NULL )
			previousUnlaggedActor->nextUnlaggedActor = nextUnlaggedActor;
		else
			firstUnlaggedActor = nextUnlaggedActor;

		if ( nextUnlaggedActor != NULL )
			nextUnlaggedActor->previousUnlaggedActor = previousUnlaggedActor;
		else
			lastUnlaggedActor = previousUnlaggedActor;
	}

	Super::Destroy();
}


CVAR(Flag, sv_nounlagged, zadmflags, ZADF_NOUNLAGGED);
CUSTOM_CVAR( Bool, sv_unlagged_debugactors, false, CVAR_SERVERINFO )
{
	if ( NETWORK_GetState() == NETSTATE_SERVER )
		SERVERCOMMANDS_SetCVar( sv_unlagged_debugactors );
	else if (self)
		self = false;
}

bool reconciledGame = false;
bool unlagInProgress = false;
int reconciliationBlockers = 0;
player_t* reconciledPlayer = NULL;
int delayTics = 0;

void UNLAGGED_Tick( void )
{
	// [BB] Only the server has to do anything here.
	if ( NETWORK_GetState() != NETSTATE_SERVER )
		return;

	//find the index
	const int unlaggedIndex = gametic % UNLAGGEDTICS;

	// [Spleen] Record sectors soon before they are reconciled/restored
	UNLAGGED_RecordSectors( unlaggedIndex );

	// [geNia] Record polyobjects
	UNLAGGED_RecordPolyobj( unlaggedIndex );

	// [Spleen] Record players
	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ++ulIdx )
	{
		if ( PLAYER_IsValidPlayerWithMo( ulIdx ) )
			UNLAGGED_RecordPlayer( &players[ulIdx], unlaggedIndex );
	}

	// [geNia] Record unlagged actors
	AUnlaggedActor* unlaggedActor = firstUnlaggedActor;
	while ( unlaggedActor ) {
		UNLAGGED_RecordActor( unlaggedActor, unlaggedIndex );
		unlaggedActor = unlaggedActor->nextUnlaggedActor;
	}
}

//Figure out which tic to use for reconciliation
int UNLAGGED_Gametic( player_t *player )
{
	// [BB] Sanity check.
	if ( player == NULL )
		return gametic;

	const ULONG playerNum = static_cast<ULONG> ( player - players );

	// Sanity check
	if ( playerNum >= MAXPLAYERS )
		return gametic;

	// [CK] We do tick-based ping now.
	const CLIENT_s *pClient = SERVER_GetClient(playerNum);
	if ( pClient == NULL )
		return gametic;

	// The logic here with + 1 is that you will be behind the server by one
	// gametic, since the loop is: 1) poll input 2) update world 3) render,
	// which makes your client old by one gametic.
	// [CK] If the client wants ping unlagged, use that.
	int unlaggedGametic = ( players[playerNum].userinfo.GetClientFlags() & CLIENTFLAGS_PING_UNLAGGED ) ?
							gametic - ( player->ulPing * TICRATE / 1000 ) :
							pClient->lLastServerGametic + 1;

	// Do not let the client send us invalid gametics ahead of the server's
	// gametic. This should be guarded against, but just in case...
	if ( unlaggedGametic > gametic )
		unlaggedGametic = gametic;

	//don't look up tics that are too old
	if ( (gametic - unlaggedGametic) >= UNLAGGEDTICS)
		unlaggedGametic = gametic - UNLAGGEDTICS + 1;

	//don't look up negative tics
	if (unlaggedGametic < 0)
		unlaggedGametic = 0;

	return unlaggedGametic;
}

// Shift stuff back in time before doing hitscan calculations
// Call UNLAGGED_Restore afterwards to restore everything
int UNLAGGED_Reconcile( AActor *actor )
{
	if ( sv_unlagged_debugactors && actor->player && NETWORK_GetState() == NETSTATE_CLIENT )
		UNLAGGED_SpawnDebugActors( actor->player, false );

	//Only do anything if the actor to be reconciled is a player,
	//it's on a server with unlagged on, and reconciliation is not being blocked
	if ( !actor || !actor->player || (NETWORK_GetState() != NETSTATE_SERVER) || !NETWORK_IsUnlaggedEnabled( actor->player ) || ( reconciliationBlockers > 0 ) || actor->player->bIsBot )
		return 0;

	//Something went wrong, reconciliation was attempted when the gamestate
	//was already reconciled!
	if (reconciledGame)
	{
		// [BB] I_Error terminates the current game, so we need to reset the value of reconciledGame,
		// otherwise UNLAGGED_Reconcile will always trigger this error from now on.
		reconciledGame = false;
		I_Error("UNLAGGED_Reconcile called while reconciledGame is true");
	}

	const int unlaggedGametic = UNLAGGED_Gametic( actor->player );

	UNLAGGED_ReconcileTick( actor, unlaggedGametic );

	if ( sv_unlagged_debugactors )
		UNLAGGED_SpawnDebugActors( actor->player, true );

	return gametic - unlaggedGametic;
}

void UNLAGGED_ReconcileTick( AActor *actor, int Tic )
{
	//Only do anything if the actor to be reconciled is a player,
	//it's on a server with unlagged on, and reconciliation is not being blocked
	if ( !actor || !actor->player || (NETWORK_GetState() != NETSTATE_SERVER) || ( zadmflags & ZADF_NOUNLAGGED ) || ( reconciliationBlockers > 0 ) )
		return;

	//Something went wrong, reconciliation was attempted when the gamestate
	//was already reconciled!
	if (reconciledGame)
	{
		// [BB] I_Error terminates the current game, so we need to reset the value of reconciledGame,
		// otherwise UNLAGGED_Reconcile will always trigger this error from now on.
		reconciledGame = false;
		I_Error("UNLAGGED_ReconcileTick called while reconciledGame is true");
	}

	//Don't reconcile if the unlagged gametic is the same as the current
	//because unlagged data for this tic may not be completely recorded yet
	if (Tic >= gametic)
		return;

	reconciledGame = true;
	reconciledPlayer = actor->player;

	//find the index
	const int unlaggedIndex = Tic % UNLAGGEDTICS;

	//reconcile the sectors
	for (int i = 0; i < numsectors; ++i)
	{
		if ( sectors[i].floordata && sectors[i].floordata->GetLastInstigator() != actor->player )
		{
			sectors[i].floorplane.restoreD = sectors[i].floorplane.d;
			sectors[i].floorplane.d = sectors[i].floorplane.unlaggedD[unlaggedIndex];
		}

		if ( sectors[i].ceilingdata && sectors[i].ceilingdata->GetLastInstigator() != actor->player )
		{
			sectors[i].ceilingplane.restoreD = sectors[i].ceilingplane.d;
			sectors[i].ceilingplane.d = sectors[i].ceilingplane.unlaggedD[unlaggedIndex];
		}
	}

	//reconcile the PolyActions
	TThinkerIterator<DPolyAction> polyActionIt;
	DPolyAction *polyAction;

	polyActionIt.Reinit();
	while ((polyAction = polyActionIt.Next()))
		if ( polyAction->GetLastInstigator() != actor->player )
			polyAction->ReconcileUnlagged(unlaggedIndex);

	//reconcile the players
	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i] && players[i].mo && !players[i].bSpectating)
		{
			players[i].restoreX = players[i].mo->x;
			players[i].restoreY = players[i].mo->y;
			players[i].restoreZ = players[i].mo->z;

			//Work around limitations of SetOrigin to prevent players
			//from getting stuck in ledges
			players[i].restoreFloorZ = players[i].mo->floorz;
			// [BB] ... and from jumping up through solid 3D floors.
			players[i].restoreCeilingZ = players[i].mo->ceilingz;

			//Also, don't reconcile the shooter because the client is supposed
			//to predict him
			if (players + i != actor->player)
			{
				players[i].mo->SetOrigin(
					players[i].unlaggedX[unlaggedIndex],
					players[i].unlaggedY[unlaggedIndex],
					players[i].unlaggedZ[unlaggedIndex]
				);
			}
			else
				//However, the client sometimes mispredicts itself if it's on a moving sector.
				//We need to correct for that.
			{
				//current server floorz/ceilingz before reconciliation
				fixed_t serverFloorZ = actor->floorz;
				fixed_t serverCeilingZ = actor->ceilingz;

				// [BB] Try to reset floorz/ceilingz to account for the fact that the sector the actor is in was possibly reconciled.
				actor->floorz = actor->Sector->floorplane.ZatPoint (actor->x, actor->y);
				actor->ceilingz = actor->Sector->ceilingplane.ZatPoint (actor->x, actor->y);
				P_FindFloorCeiling(actor, false);

				//force the shooter out of the floor/ceiling - a client has to mispredict in this case,
				//because not mispredicting would mean the client would think he's inside the floor/ceiling
				if (actor->z + actor->height > actor->ceilingz)
					actor->z = actor->ceilingz - actor->height;

				if (actor->z < actor->floorz)
					actor->z = actor->floorz;

				//floor moved up - a client might have mispredicted himself too low due to gravity
				//and the client thinking the floor is lower than it actually is
				// [BB] But only do this if the sector actually moved. Note: This adjustment seems to break on some kind of non-moving 3D floors.
				if ( (serverFloorZ > actor->floorz) && (( actor->Sector->floorplane.restoreD != actor->Sector->floorplane.d ) || ( actor->Sector->ceilingplane.restoreD != actor->Sector->ceilingplane.d )) )
				{
					//shooter was standing on the floor, let's pull him down to his floor if
					//he wasn't falling
					if ( (actor->z == serverFloorZ) && (actor->velz >= 0) )
						actor->z = actor->floorz;

					//todo: more correction for floor moving up
				}

				//todo: more correction for client misprediction
			}
		}
	}

	// [geNia] Reconcile unlagged actors
	AUnlaggedActor* unlaggedActor = firstUnlaggedActor;
	while ( unlaggedActor ) {
		if ( !unlaggedActor->isCurrentlyBeingUnlagged ) {
			unlaggedActor->restoreFlags[0] = unlaggedActor->mvFlags;
			unlaggedActor->restoreFlags[1] = unlaggedActor->flags;
			unlaggedActor->restoreFlags[2] = unlaggedActor->flags2;
			unlaggedActor->restoreFlags[3] = unlaggedActor->flags3;
			unlaggedActor->restoreFlags[4] = unlaggedActor->flags4;
			unlaggedActor->restoreFlags[5] = unlaggedActor->flags5;
			unlaggedActor->restoreFlags[6] = unlaggedActor->flags6;
			unlaggedActor->restoreFlags[7] = unlaggedActor->flags7;
			unlaggedActor->restoreFlags[8] = unlaggedActor->flags8;
			unlaggedActor->restoreX = unlaggedActor->x;
			unlaggedActor->restoreY = unlaggedActor->y;
			unlaggedActor->restoreZ = unlaggedActor->z;
			unlaggedActor->restoreFloorZ = unlaggedActor->floorz;
			unlaggedActor->restoreCeilingZ = unlaggedActor->ceilingz;

			unlaggedActor->UnlinkFromWorld();

			unlaggedActor->mvFlags = unlaggedActor->unlaggedFlags[unlaggedIndex][0];
			unlaggedActor->flags = unlaggedActor->unlaggedFlags[unlaggedIndex][1];
			unlaggedActor->flags2 = unlaggedActor->unlaggedFlags[unlaggedIndex][2];
			unlaggedActor->flags3 = unlaggedActor->unlaggedFlags[unlaggedIndex][3];
			unlaggedActor->flags4 = unlaggedActor->unlaggedFlags[unlaggedIndex][4];
			unlaggedActor->flags5 = unlaggedActor->unlaggedFlags[unlaggedIndex][5];
			unlaggedActor->flags6 = unlaggedActor->unlaggedFlags[unlaggedIndex][6];
			unlaggedActor->flags7 = unlaggedActor->unlaggedFlags[unlaggedIndex][7];
			unlaggedActor->flags8 = unlaggedActor->unlaggedFlags[unlaggedIndex][8];
			unlaggedActor->x = unlaggedActor->unlaggedX[unlaggedIndex];
			unlaggedActor->y = unlaggedActor->unlaggedY[unlaggedIndex];
			unlaggedActor->z = unlaggedActor->unlaggedZ[unlaggedIndex];

			unlaggedActor->LinkToWorld();

			unlaggedActor->floorz = unlaggedActor->Sector->floorplane.ZatPoint(unlaggedActor->restoreX, unlaggedActor->restoreY);
			unlaggedActor->ceilingz = unlaggedActor->Sector->ceilingplane.ZatPoint(unlaggedActor->restoreX, unlaggedActor->restoreY);

		}

		unlaggedActor = unlaggedActor->nextUnlaggedActor;
	}
}

int	UNLAGGED_GetDelayTics( )
{
	return delayTics;
}

// Restore everything that has been shifted
// back in time by UNLAGGED_Reconcile
void UNLAGGED_Restore( AActor *actor )
{
	//Only do anything if the game is currently reconciled
	// [BB] Since reconciledGame can only be true if all necessary checks in UNLAGGED_Reconcile were passed,
	// we don't need to check anything but reconciledGame here.
	// [Spleen] Also need to check for any reconciliationBlockers
	if ( !reconciledGame || ( reconciliationBlockers > 0 ) )
		return;

	//restore the sectors
	for (int i = 0; i < numsectors; ++i)
	{
		if ( sectors[i].floordata && sectors[i].floordata->GetLastInstigator() != actor->player )
			sectors[i].floorplane.d = sectors[i].floorplane.restoreD;
		if ( sectors[i].ceilingdata && sectors[i].ceilingdata->GetLastInstigator() != actor->player )
			sectors[i].ceilingplane.d = sectors[i].ceilingplane.restoreD;
	}

	//reconcile the PolyActions
	TThinkerIterator<DPolyAction> polyActionIt;
	DPolyAction *polyAction;

	polyActionIt.Reinit();
	while ((polyAction = polyActionIt.Next()))
		if ( polyAction->GetLastInstigator() != actor->player )
			polyAction->RestoreUnlagged();

	//restore the players
	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i] && players[i].mo && !players[i].bSpectating && players + i != actor->player)
		{
			players[i].mo->SetOrigin( players[i].restoreX, players[i].restoreY, players[i].restoreZ );
			players[i].mo->floorz = players[i].restoreFloorZ;
			players[i].mo->ceilingz = players[i].restoreCeilingZ;
		}
	}

	// we need to set this to false earlier, otherwise SERVERCOMMANDS below won't be sent
	reconciledGame = false;
	reconciledPlayer = NULL;

	// [geNia] Restore unlagged actor
	AUnlaggedActor* unlaggedActor = firstUnlaggedActor;
	while ( unlaggedActor ) {
		if ( !unlaggedActor->isCurrentlyBeingUnlagged )
		{
			// Only restore if the actor wasn't destroyed or killed
			if ( ( unlaggedActor->health <= 0 ) || ( unlaggedActor->isMissile && !(unlaggedActor->flags & MF_MISSILE) ) )
			{
				if ( unlaggedActor->isMissile )
					SERVERCOMMANDS_MissileExplode( unlaggedActor, unlaggedActor->BlockingLine );
				else
					SERVERCOMMANDS_MoveThing( unlaggedActor, CM_XY|CM_Z|CM_VELXY|CM_VELZ|CM_NOSMOOTH );
				SERVERCOMMANDS_SetThingFrame( unlaggedActor, unlaggedActor->state );
				SERVERCOMMANDS_SetThingTics( unlaggedActor );
				SERVERCOMMANDS_UpdateThingFlagsNotAtDefaults( unlaggedActor );
			}
			else
			{
				unlaggedActor->UnlinkFromWorld();

				unlaggedActor->mvFlags = unlaggedActor->restoreFlags[0];
				unlaggedActor->flags = unlaggedActor->restoreFlags[1];
				unlaggedActor->flags2 = unlaggedActor->restoreFlags[2];
				unlaggedActor->flags3 = unlaggedActor->restoreFlags[3];
				unlaggedActor->flags4 = unlaggedActor->restoreFlags[4];
				unlaggedActor->flags5 = unlaggedActor->restoreFlags[5];
				unlaggedActor->flags6 = unlaggedActor->restoreFlags[6];
				unlaggedActor->flags7 = unlaggedActor->restoreFlags[7];
				unlaggedActor->flags8 = unlaggedActor->restoreFlags[8];
				unlaggedActor->x = unlaggedActor->restoreX;
				unlaggedActor->y = unlaggedActor->restoreY;
				unlaggedActor->z = unlaggedActor->restoreZ;

				unlaggedActor->LinkToWorld();

				unlaggedActor->floorz = unlaggedActor->restoreFloorZ;
				unlaggedActor->ceilingz = unlaggedActor->restoreCeilingZ;
			}
		}

		unlaggedActor = unlaggedActor->nextUnlaggedActor;
	}
}


// Record the positions of just one player
// in order to be able to reconcile them later
void UNLAGGED_RecordPlayer( player_t *player, int unlaggedIndex )
{
	// [BB] Sanity check.
	if ( player == NULL )
		return;

	//Only do anything if it's on a server
	if (NETWORK_GetState() != NETSTATE_SERVER)
		return;

	//record the player
	player->unlaggedX[unlaggedIndex] = player->mo->x;
	player->unlaggedY[unlaggedIndex] = player->mo->y;
	player->unlaggedZ[unlaggedIndex] = player->mo->z;
}


// Reset the reconciliation buffers of a player
// Should be called when a player is spawned
void UNLAGGED_ResetPlayer( player_t *player )
{
	// [BB] Sanity check.
	if ( player == NULL )
		return;

	//Only do anything if it's on a server
	if (NETWORK_GetState() != NETSTATE_SERVER)
		return;

	for (int unlaggedIndex = 0; unlaggedIndex < UNLAGGEDTICS; ++unlaggedIndex)
	{
		player->unlaggedX[unlaggedIndex] = player->mo->x;
		player->unlaggedY[unlaggedIndex] = player->mo->y;
		player->unlaggedZ[unlaggedIndex] = player->mo->z;
	}
}

// Record the positions of just one actor
// in order to be able to reconcile it later
void UNLAGGED_RecordActor( AUnlaggedActor* unlaggedActor, int unlaggedIndex )
{
	// [geNia] Sanity check.
	if ( unlaggedActor == NULL )
		return;

	// Only do anything if it's on a server
	if ( NETWORK_GetState() != NETSTATE_SERVER )
		return;

	// record the projectile
	unlaggedActor->unlaggedFlags[unlaggedIndex][0] = unlaggedActor->mvFlags;
	unlaggedActor->unlaggedFlags[unlaggedIndex][1] = unlaggedActor->flags;
	unlaggedActor->unlaggedFlags[unlaggedIndex][2] = unlaggedActor->flags2;
	unlaggedActor->unlaggedFlags[unlaggedIndex][3] = unlaggedActor->flags3;
	unlaggedActor->unlaggedFlags[unlaggedIndex][4] = unlaggedActor->flags4;
	unlaggedActor->unlaggedFlags[unlaggedIndex][5] = unlaggedActor->flags5;
	unlaggedActor->unlaggedFlags[unlaggedIndex][6] = unlaggedActor->flags6;
	unlaggedActor->unlaggedFlags[unlaggedIndex][7] = unlaggedActor->flags7;
	unlaggedActor->unlaggedFlags[unlaggedIndex][8] = unlaggedActor->flags8;
	unlaggedActor->unlaggedX[unlaggedIndex] = unlaggedActor->x;
	unlaggedActor->unlaggedY[unlaggedIndex] = unlaggedActor->y;
	unlaggedActor->unlaggedZ[unlaggedIndex] = unlaggedActor->z;
}


// Reset the reconciliation buffers of an actor
// Should be called when an actor is spawned
void UNLAGGED_ResetActor( AUnlaggedActor* unlaggedActor )
{
	// [geNia] Sanity check.
	if ( unlaggedActor == NULL )
		return;

	// Only do anything if it's on a server
	if ( NETWORK_GetState() != NETSTATE_SERVER )
		return;

	for (int unlaggedIndex = 0; unlaggedIndex < UNLAGGEDTICS; ++unlaggedIndex)
	{
		unlaggedActor->unlaggedFlags[unlaggedIndex][0] = unlaggedActor->mvFlags;
		unlaggedActor->unlaggedFlags[unlaggedIndex][1] = unlaggedActor->flags;
		unlaggedActor->unlaggedFlags[unlaggedIndex][2] = unlaggedActor->flags2;
		unlaggedActor->unlaggedFlags[unlaggedIndex][3] = unlaggedActor->flags3;
		unlaggedActor->unlaggedFlags[unlaggedIndex][4] = unlaggedActor->flags4;
		unlaggedActor->unlaggedFlags[unlaggedIndex][5] = unlaggedActor->flags5;
		unlaggedActor->unlaggedFlags[unlaggedIndex][6] = unlaggedActor->flags6;
		unlaggedActor->unlaggedFlags[unlaggedIndex][7] = unlaggedActor->flags7;
		unlaggedActor->unlaggedFlags[unlaggedIndex][8] = unlaggedActor->flags8;
		unlaggedActor->unlaggedX[unlaggedIndex] = unlaggedActor->x;
		unlaggedActor->unlaggedY[unlaggedIndex] = unlaggedActor->y;
		unlaggedActor->unlaggedZ[unlaggedIndex] = unlaggedActor->z;
	}
}


// Record the positions of the sectors
void UNLAGGED_RecordSectors( int unlaggedIndex )
{
	//Only do anything if it's on a server
	if (NETWORK_GetState() != NETSTATE_SERVER)
		return;

	//record the sectors
	for (int i = 0; i < numsectors; ++i)
	{
		sectors[i].floorplane.unlaggedD[unlaggedIndex] = sectors[i].floorplane.d;
		sectors[i].ceilingplane.unlaggedD[unlaggedIndex] = sectors[i].ceilingplane.d;
	}
}

// Record the positions of the polyobjects
void UNLAGGED_RecordPolyobj( int unlaggedIndex )
{
	//Only do anything if it's on a server
	if (NETWORK_GetState() != NETSTATE_SERVER)
		return;

	TThinkerIterator<DPolyAction> polyActionIt;
	DPolyAction *polyAction;

	polyActionIt.Reinit();
	while ((polyAction = polyActionIt.Next()))
		polyAction->RecordUnlagged(unlaggedIndex);
}

bool UNLAGGED_DrawRailClientside ( AActor *attacker )
{
	if ( ( attacker == NULL ) || ( attacker->player == NULL ) )
		return false;

	// [BB] Rails are only client side when unlagged is on.
	if ( !NETWORK_IsUnlaggedEnabled( attacker->player ) )
		return false;

	// [BB] A client should only draw rails for its own player.
	if ( ( NETWORK_GetState() != NETSTATE_SERVER ) && ( ( attacker->player - players ) != consoleplayer ) )
		return false;

	return true;
}

// [BB] If reconciliation moved the actor we hit, this function calculates the offset.
void UNLAGGED_GetHitOffset ( const AActor *attacker, const FTraceResults &trace, TVector3<fixed_t> &hitOffset )
{
	hitOffset.Zero();

	// [BB] The game is not reconciled, so no offset. If the attacker is unknown, we can't calculate the offset.
	if ( ( reconciledGame == false ) || ( attacker == NULL ) )
		return;

	const int unlaggedGametic = UNLAGGED_Gametic( attacker->player );

	// [BB] No reconciliation no offset.
	if ( unlaggedGametic == gametic )
		return;

	if ( ( trace.HitType == TRACE_HitActor ) && trace.Actor && trace.Actor->player )
	{
		const int unlaggedIndex = unlaggedGametic % UNLAGGEDTICS;

		const player_t *hitPlayer = trace.Actor->player;

		hitOffset[0] = hitPlayer->restoreX - hitPlayer->unlaggedX[unlaggedIndex];
		hitOffset[1] = hitPlayer->restoreY - hitPlayer->unlaggedY[unlaggedIndex];
		hitOffset[2] = hitPlayer->restoreZ - hitPlayer->unlaggedZ[unlaggedIndex];
	}
}

bool UNLAGGED_IsReconciled ( )
{
  return reconciledGame;
}

player_t *UNLAGGED_GetReconciledPlayer ( )
{
  return reconciledPlayer;
}

void UNLAGGED_AddReconciliationBlocker ( )
{
	reconciliationBlockers++;
}

void UNLAGGED_RemoveReconciliationBlocker ( )
{
	if ( reconciliationBlockers == 0 )
		I_Error("UNLAGGED_RemoveReconciliationBlocker called when reconciliationBlockers == 0");
	else
		reconciliationBlockers--;
}

void UNLAGGED_SpawnDebugActor ( player_t *player, AActor *actor, bool server )
{
	AActor *pActor = Spawn( "DebugUnlaggedHitbox", actor->x, actor->y, actor->z, NO_REPLACE );
	pActor->projectilepassheight = actor->projectilepassheight;
	pActor->target = actor;

	if ( server )
	{
		SERVERCOMMANDS_SpawnThing( pActor, player - players, SVCF_ONLYTHISCLIENT );
		SERVERCOMMANDS_SetThingTarget( pActor );
	}
	else
	{
		pActor->hitboxColor = 255 * 256; // green color
	}
}

void UNLAGGED_SpawnDebugActors ( player_t *player, bool server )
{
	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ++ulIdx )
	{
		if ( PLAYER_IsValidPlayerWithMo( ulIdx ) )
		{
			UNLAGGED_SpawnDebugActor( player, players[ulIdx].mo, server );
		}
	}

	AUnlaggedActor* unlaggedActor = firstUnlaggedActor;
	while ( unlaggedActor) {
		UNLAGGED_SpawnDebugActor( player, unlaggedActor, server );
		unlaggedActor = unlaggedActor->nextUnlaggedActor;
	}
}

void UNLAGGED_SendThingUpdate( AActor *source, AActor *thing, int lNetID, bool skipOwner )
{
	player_t* player = NETWORK_GetActorsOwnerPlayer( source );

	bool wasReconciled = reconciledGame;
	reconciledGame = false;
	if ( thing->state != NULL )
	{
		SERVERCOMMANDS_UpdateClientNetID( player - players );
		// The thing didn't stop during unlagged, so tell clients about it
		if ( skipOwner )
		{
			SERVERCOMMANDS_MoveThing( thing, CM_XY|CM_Z|CM_VELXY|CM_VELZ|CM_NOSMOOTH, player - players, SVCF_SKIPTHISCLIENT );
			SERVERCOMMANDS_SetThingFrame( thing, thing->state, player - players, SVCF_SKIPTHISCLIENT );
			SERVERCOMMANDS_SetThingTics( thing, player - players, SVCF_SKIPTHISCLIENT );
			SERVERCOMMANDS_UpdateThingFlagsNotAtDefaults( thing, player - players, SVCF_SKIPTHISCLIENT );
		}
		else
		{
			SERVERCOMMANDS_SpawnThing( thing, player - players, SVCF_ONLYTHISCLIENT );
			SERVERCOMMANDS_MoveThing( thing, CM_XY|CM_Z|CM_VELXY|CM_VELZ|CM_NOSMOOTH );
			SERVERCOMMANDS_SetThingFrame( thing, thing->state );
			SERVERCOMMANDS_SetThingTics( thing );
			SERVERCOMMANDS_UpdateThingFlagsNotAtDefaults( thing );
		}
	}
	else
	{
		// The thing stopped during unlagged, tell clients about it
		SERVERCOMMANDS_UpdateClientNetID( player - players );
		thing->lNetID = lNetID; // The thing lost it's NetID so we need to restore it for this command only
		SERVERCOMMANDS_DestroyThing( thing, source->player - players, SVCF_SKIPTHISCLIENT );
		thing->lNetID = -1;
	}
	reconciledGame = wasReconciled;
}

void UNLAGGED_SendMissileUpdate( AActor *source, AActor *missile, int lNetID, bool skipOwner )
{
	player_t* player = NETWORK_GetActorsOwnerPlayer( source );

	bool wasReconciled = reconciledGame;
	reconciledGame = false;
	if ( missile->state != NULL )
	{
		SERVERCOMMANDS_UpdateClientNetID( player - players );
		// The missile didn't stop during unlagged, so tell clients about it
		if ( skipOwner )
		{
			if ( missile->flags & MF_MISSILE )
				SERVERCOMMANDS_MoveThing( missile, CM_XY|CM_Z|CM_VELXY|CM_VELZ|CM_NOSMOOTH, player - players, SVCF_SKIPTHISCLIENT );
			else
				SERVERCOMMANDS_MissileExplode( missile, missile->BlockingLine, player - players, SVCF_SKIPTHISCLIENT );
			SERVERCOMMANDS_SetThingFrame( missile, missile->state, player - players, SVCF_SKIPTHISCLIENT );
			SERVERCOMMANDS_SetThingTics( missile, player - players, SVCF_SKIPTHISCLIENT );
			SERVERCOMMANDS_UpdateThingFlagsNotAtDefaults( missile, player - players, SVCF_SKIPTHISCLIENT );
		}
		else
		{
			if ( missile->flags & MF_MISSILE )
			{
				SERVERCOMMANDS_SpawnMissile( missile, player - players, SVCF_ONLYTHISCLIENT );
				SERVERCOMMANDS_MoveThing( missile, CM_XY|CM_Z|CM_VELXY|CM_VELZ|CM_NOSMOOTH, player - players, SVCF_SKIPTHISCLIENT );
				SERVERCOMMANDS_SetThingFrame( missile, missile->state );
				SERVERCOMMANDS_SetThingTics( missile );
				SERVERCOMMANDS_UpdateThingFlagsNotAtDefaults( missile );
			}
			else
			{
				SERVERCOMMANDS_MissileExplode( missile, missile->BlockingLine, player - players, SVCF_SKIPTHISCLIENT );
				SERVERCOMMANDS_SetThingFrame( missile, missile->state, player - players, SVCF_SKIPTHISCLIENT );
				SERVERCOMMANDS_SetThingTics( missile, player - players, SVCF_SKIPTHISCLIENT );
				SERVERCOMMANDS_UpdateThingFlagsNotAtDefaults( missile, player - players, SVCF_SKIPTHISCLIENT );
			}
		}
	}
	else
	{
		// The missile stopped during unlagged, tell clients about it
		SERVERCOMMANDS_UpdateClientNetID( player - players );
		missile->lNetID = lNetID; // The missile lost it's NetID so we need to restore it for this command only
		SERVERCOMMANDS_DestroyThing( missile, source->player - players, SVCF_SKIPTHISCLIENT );
		missile->lNetID = -1;
	}
	reconciledGame = wasReconciled;
}

struct currentlyUnlaggingActor
{
	AActor *actor;
	bool isMissile;
	int lNetID;
	bool skipOwner;
	bool unlagDeath;
	bool isFirstIteration;
	bool finishedUnlagging;
};
std::vector<currentlyUnlaggingActor*> currentlyUnlaggingActors;

void UNLAGGED_DoUnlagActors( AActor *source, int StartingTick )
{
	if (StartingTick < gametic || currentlyUnlaggingActors.size() > 0)
	{
		unlagInProgress = true;
		bool allActorsFinished = false;
		for (int Tick = StartingTick; Tick <= gametic; Tick++)
		{
			if (!reconciledGame) // it is possible that an actor to unlag spawned while the game is reconciled. We can just skip reconciliation.
			{
				UNLAGGED_ReconcileTick( source, Tick );
				if ( sv_unlagged_debugactors )
					UNLAGGED_SpawnDebugActors( source->player, true );
			}
			UNLAGGED_AddReconciliationBlocker();
			delayTics = Tick - StartingTick;

			allActorsFinished = currentlyUnlaggingActors[0]->finishedUnlagging;

			for (unsigned int i = 0; i < currentlyUnlaggingActors.size(); i++)
			{
				currentlyUnlaggingActor *cua = currentlyUnlaggingActors[i];
				if (cua->finishedUnlagging)
					continue;

				AActor *actor = cua->actor;

				bool isMissile = !!( actor->flags & MF_MISSILE );
				AUnlaggedActor* unlaggedActor = NULL;
				if (actor->IsKindOf(RUNTIME_CLASS(AUnlaggedActor)))
					unlaggedActor = static_cast<AUnlaggedActor *>(actor);

				if (cua->isFirstIteration)
				{
					actor->PostBeginPlay();
					cua->isFirstIteration = false;
				}
				actor->Tick();

				UNLAGGED_RecordActor( unlaggedActor, Tick % UNLAGGEDTICS );

				bool actorDied = isMissile ? !(actor->flags & MF_MISSILE) : !!(actor->flags6 & MF6_KILLED);
				if ( ( actorDied && !cua->unlagDeath ) || actor->state == NULL )
				{
					if (unlaggedActor != NULL)
					{
						// We don't want to unlag dead actors so complete remaining unlagged records
						for (int Tick2 = Tick + 1; Tick2 <= gametic; Tick2++)
							UNLAGGED_RecordActor( unlaggedActor, Tick2 % UNLAGGEDTICS );
						unlaggedActor->isCurrentlyBeingUnlagged = false;
					}

					// Skip this actor in future
					cua->finishedUnlagging = true;
					cua->actor->wasJustUnlagged = true;

					if ( cua->isMissile )
						UNLAGGED_SendMissileUpdate( source, cua->actor, cua->lNetID, cua->skipOwner );
					else
						UNLAGGED_SendThingUpdate( source, cua->actor, cua->lNetID, cua->skipOwner );
				}

				allActorsFinished &= cua->finishedUnlagging;
			}

			UNLAGGED_RemoveReconciliationBlocker();
			UNLAGGED_Restore( source );

			if (allActorsFinished)
				break;
		}
		unlagInProgress = false;
	}

	for (unsigned int i = 0; i < currentlyUnlaggingActors.size(); i++)
	{
		currentlyUnlaggingActor *cua = currentlyUnlaggingActors[i];

		cua->actor->wasJustUnlagged = true;

		if ( cua->actor->IsKindOf( RUNTIME_CLASS(AUnlaggedActor) ) )
			static_cast<AUnlaggedActor *>(cua->actor)->isCurrentlyBeingUnlagged = false;

		if ( !cua->finishedUnlagging )
			if ( cua->isMissile )
				UNLAGGED_SendMissileUpdate( source, cua->actor, cua->lNetID, cua->skipOwner );
			else
				UNLAGGED_SendThingUpdate( source, cua->actor, cua->lNetID, cua->skipOwner );

		free(cua);
	}
	currentlyUnlaggingActors.clear();
}

void UNLAGGED_UnlagActor( AActor *source, AActor *actor, bool isMissile, bool skipOwner, bool unlagDeath )
{
	struct currentlyUnlaggingActor *cua = (struct currentlyUnlaggingActor*) malloc(sizeof(struct currentlyUnlaggingActor));
	cua->actor = actor;
	cua->isFirstIteration = true;
	cua->isMissile = isMissile;
	cua->skipOwner = skipOwner;
	cua->lNetID = actor->lNetID;
	cua->unlagDeath = unlagDeath;
	cua->finishedUnlagging = false;
	currentlyUnlaggingActors.emplace_back( cua );

	if (actor->IsKindOf(RUNTIME_CLASS(AUnlaggedActor)))
		static_cast<AUnlaggedActor *>(actor)->isCurrentlyBeingUnlagged = true;

	// If the game is already being unlagged, just add the actor to the list and leave
	if ( unlagInProgress )
		return;

	UNLAGGED_DoUnlagActors( source, UNLAGGED_Gametic( source->player ) );
}

void UNLAGGED_UnlagAndReplicateThing( AActor *source, AActor *thing, bool bSkipOwner, bool bNoUnlagged, bool bUnlagDeath )
{
	if ( ( (!source || !source->player) && !UNLAGGED_IsReconciled() ) || ( zadmflags & ZADF_NOUNLAGGED ) )
		bNoUnlagged = true;
	
	player_t* player = source->player;

	// We have to spawn the thing on clients every time even if it stops during unlagged
	// Otherwise some Decorate functions will be replicated before the client spawns the thing and not work as intended
	SERVERCOMMANDS_SpawnThing(thing, player - players, SVCF_SKIPTHISCLIENT);

	if ( !source || !NETWORK_ClientsideFunctionsAllowed( player ) || bNoUnlagged || UNLAGGED_Gametic( source->player ) >= gametic || player->bIsBot ) {
		// Unlagged is disabled, so exit here
		if ( bSkipOwner )
			SERVERCOMMANDS_UpdateClientNetID( player - players );
		else
			SERVERCOMMANDS_SpawnThing( thing, player - players, SVCF_ONLYTHISCLIENT );
		SERVER_SetThingNonZeroAngleAndVelocity( thing );
		return;
	}

	UNLAGGED_UnlagActor( source, thing, false, bSkipOwner, bUnlagDeath );
}

void UNLAGGED_UnlagAndReplicateMissile( AActor *source, AActor *missile, bool bSkipOwner, bool bNoUnlagged, bool bUnlagDeath )
{
	if ( ( (!source || !source->player) && !UNLAGGED_IsReconciled() ) || ( zadmflags & ZADF_NOUNLAGGED ) )
		bNoUnlagged = true;

	player_t* player = source->player;

	// We have to spawn the missile on clients every time even if it stops during unlagged
	// Otherwise some Decorate functions will be replicated before the client spawns the missile and not work as intended
	SERVERCOMMANDS_SpawnMissile( missile, player - players, SVCF_SKIPTHISCLIENT );

	if ( !source || !NETWORK_ClientsideFunctionsAllowed( player ) || bNoUnlagged || UNLAGGED_Gametic( source->player ) >= gametic || player->bIsBot ) {
		// Unlagged is disabled, so exit here
		if ( bSkipOwner )
			SERVERCOMMANDS_UpdateClientNetID( player - players );
		else
			SERVERCOMMANDS_SpawnMissile( missile, player - players, SVCF_ONLYTHISCLIENT );
		return;
	}

	UNLAGGED_UnlagActor( source, missile, true, bSkipOwner, bUnlagDeath );
}
