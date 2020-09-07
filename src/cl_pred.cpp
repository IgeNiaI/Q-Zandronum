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
// Filename: cl_pred.cpp
//
// Description: Contains variables and routines related to the client prediction
// portion of the program.
//
//-----------------------------------------------------------------------------

#include "doomtype.h"
#include "doomstat.h"
#include "d_player.h"
#include "g_game.h"
#include "d_net.h"
#include "p_local.h"
#include "gi.h"
#include "i_system.h"
#include "c_dispatch.h"
#include "st_stuff.h"
#include "m_argv.h"
#include "p_effect.h"
#include "c_console.h"
#include "cl_demo.h"
#include "cl_main.h"

void P_MovePlayer (player_t *player, ticcmd_t *cmd);
void P_CalcHeight (player_t *player);
void P_DeathThink (player_t *player);
bool	P_AdjustFloorCeil (AActor *thing);

//*****************************************************************************
//	VARIABLES

// Are we predicting?
static	bool		g_bPredicting = false;

// Version of gametic for normal games, as well as demos.
static	ULONG		g_ulGameTick;

// Store crucial player attributes for prediction.
static	ticcmd_t	g_SavedTiccmd[CLIENT_PREDICTION_TICS];
static	angle_t		g_SavedAngle[CLIENT_PREDICTION_TICS];
static	fixed_t		g_SavedPitch[CLIENT_PREDICTION_TICS];
static	fixed_t		g_SavedCrouchfactor[CLIENT_PREDICTION_TICS];
static	LONG		g_lSavedJumpTicks[CLIENT_PREDICTION_TICS];
static	BYTE		g_SavedTurnTicks[CLIENT_PREDICTION_TICS];
static	LONG		g_lSavedReactionTime[CLIENT_PREDICTION_TICS];
static	LONG		g_lSavedWaterLevel[CLIENT_PREDICTION_TICS];
static	bool		g_bSavedOnFloor[CLIENT_PREDICTION_TICS];
static	bool		g_bSavedOnMobj[CLIENT_PREDICTION_TICS];
static	fixed_t		g_SavedFloorZ[CLIENT_PREDICTION_TICS];

#ifdef	_DEBUG
CVAR( Bool, cl_showpredictionsuccess, false, 0 );
CVAR( Bool, cl_showonetickpredictionerrors, false, 0 );
#endif

//*****************************************************************************
//	PROTOTYPES

static	void	client_predict_BeginPrediction( player_t *pPlayer );
static	void	client_predict_DoPrediction( player_t *pPlayer, ULONG ulTicks );
static	void	client_predict_EndPrediction( player_t *pPlayer );
static	void	client_predict_SaveOnGroundStatus( const player_t *pPlayer, const ULONG Tick );

//*****************************************************************************
//	FUNCTIONS

void CLIENT_PREDICT_Construct( void )
{
	for ( int i = 0; i < CLIENT_PREDICTION_TICS; ++i )
		g_SavedCrouchfactor[i] = FRACUNIT;
}

//*****************************************************************************
//
void CLIENT_PREDICT_PlayerPredict( void )
{
	player_t	*pPlayer;
	ULONG		ulPredictionTicks;
#ifdef	_DEBUG
	fixed_t		SavedX;
	fixed_t		SavedY;
	fixed_t		SavedZ;
#endif

	// Always predict only the console player.
	pPlayer = &players[consoleplayer];
	if ( pPlayer->mo == NULL )
		return;

	// For spectators, we don't care about prediction. Just think and leave.
	if (( pPlayer->bSpectating ) ||
		( pPlayer->playerstate == PST_DEAD ))
	{
		P_PlayerThink( pPlayer );
		pPlayer->mo->Tick( );
		return;
	}

	// Just came out of a teleport.
	if ( pPlayer->mo->reactiontime )
	{
		pPlayer->cmd.ucmd.forwardmove = 0;
		pPlayer->cmd.ucmd.sidemove = 0;
	
		P_PlayerThink( pPlayer );
		pPlayer->mo->Tick( );
		return;
	}

	// Back up the player's current position to see if we predicted correctly.
#ifdef	_DEBUG
	SavedX	= pPlayer->mo->x;
	SavedY	= pPlayer->mo->y;
	SavedZ	= pPlayer->mo->z;
#endif

	// Use a version of gametic that's appropriate for both the current game and demos.
	g_ulGameTick = gametic - CLIENTDEMO_GetGameticOffset( );

	// [BB] This would mean that a negative amount of prediction tics is needed, so something is wrong.
	// So far it looks like the "lagging at connect / map start" prevented this from happening before.
	if ( CLIENT_GetLastConsolePlayerUpdateTick() > g_ulGameTick )
		return;

	// How many ticks of prediction do we need?
	ulPredictionTicks = g_ulGameTick - CLIENT_GetLastConsolePlayerUpdateTick( );
	// [BB] We can't predict more tics than we store.
	if ( ulPredictionTicks > CLIENT_PREDICTION_TICS )
		ulPredictionTicks = CLIENT_PREDICTION_TICS;
	if ( ulPredictionTicks )
		ulPredictionTicks--;

#ifdef	_DEBUG
	if (( cl_showonetickpredictionerrors ) && ( ulPredictionTicks == 0 ))
	{
		if (( pPlayer->ServerXYZ[0] != pPlayer->mo->x ) ||
			( pPlayer->ServerXYZ[1] != pPlayer->mo->y ) ||
			( pPlayer->ServerXYZ[2] != pPlayer->mo->z ))
		{
			Printf( "(%d) WARNING! ServerXYZ does not match local origin after 1 tick!\n", static_cast<unsigned int> (g_ulGameTick) );
			Printf( "     X: %d, %d\n", pPlayer->ServerXYZ[0], pPlayer->mo->x );
			Printf( "     Y: %d, %d\n", pPlayer->ServerXYZ[1], pPlayer->mo->y );
			Printf( "     Z: %d, %d\n", pPlayer->ServerXYZ[2], pPlayer->mo->z );
		}

		if (( pPlayer->ServerXYZVel[0] != pPlayer->mo->velx ) ||
			( pPlayer->ServerXYZVel[1] != pPlayer->mo->vely ) ||
			( pPlayer->ServerXYZVel[2] != pPlayer->mo->velz ))
		{
			Printf( "(%d) WARNING! ServerXYZVel does not match local origin after 1 tick!\n", static_cast<unsigned int> (g_ulGameTick) );
			Printf( "     X: %d, %d\n", pPlayer->ServerXYZVel[0], pPlayer->mo->velx );
			Printf( "     Y: %d, %d\n", pPlayer->ServerXYZVel[1], pPlayer->mo->vely );
			Printf( "     Z: %d, %d\n", pPlayer->ServerXYZVel[2], pPlayer->mo->velz );
		}
	}
#endif

	// [BB] Save the "on ground" status. Necessary to keep movement on moving floors
	// and on actors like bridge things smooth.
	client_predict_SaveOnGroundStatus ( pPlayer, g_ulGameTick );

	// Set the player's position as told to him by the server.
	CLIENT_MoveThing( pPlayer->mo,
		pPlayer->ServerXYZ[0],
		pPlayer->ServerXYZ[1],
		pPlayer->ServerXYZ[2] );

	// Set the player's velocity as told to him by the server.
	pPlayer->mo->velx = pPlayer->ServerXYZVel[0];
	pPlayer->mo->vely = pPlayer->ServerXYZVel[1];
	pPlayer->mo->velz = pPlayer->ServerXYZVel[2];

	// If we don't want to do any prediction, just tick the player and get out.
	if ( cl_predict_players == false )
	{
		P_PlayerThink( pPlayer );
		pPlayer->mo->Tick( );
		return;
	}

	// Save a bunch of crucial attributes of the player that are necessary for prediction.
	client_predict_BeginPrediction( pPlayer );

	// Predict however many ticks are necessary.
	g_bPredicting = true;
	client_predict_DoPrediction( pPlayer, ulPredictionTicks );
	g_bPredicting = false;

	// Restore crucial attributes for this tick.
	client_predict_EndPrediction( pPlayer );

#ifdef	_DEBUG
	if ( cl_showpredictionsuccess )
	{
		if (( SavedX == pPlayer->mo->x ) &&
			( SavedY == pPlayer->mo->y ) &&
			( SavedZ == pPlayer->mo->z ))
		{
			Printf( "SUCCESSFULLY predicted %d ticks!\n", static_cast<unsigned int> (ulPredictionTicks) );
		}
		else
		{
			Printf( "FAILED to predict %d ticks.\n", static_cast<unsigned int> (ulPredictionTicks) );
		}
	}
#endif

	// Now that all of the prediction has been done, do our movement for this tick.
	P_PlayerThink( pPlayer );
	// [BB] Due to recent ZDoom changes (ported in revision 2029), we need to save the old buttons.
	pPlayer->oldbuttons = pPlayer->cmd.ucmd.buttons;
	pPlayer->mo->Tick( );
}

//*****************************************************************************
//
void CLIENT_PREDICT_SaveCmd( void )
{
	memcpy( &g_SavedTiccmd[gametic % CLIENT_PREDICTION_TICS], &players[consoleplayer].cmd, sizeof( ticcmd_t ));
}

//*****************************************************************************
//
void CLIENT_PREDICT_PlayerTeleported( void )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < CLIENT_PREDICTION_TICS; ulIdx++ )
	{
		memset( &g_SavedTiccmd[ulIdx], 0, sizeof( ticcmd_t ));
		g_SavedFloorZ[ulIdx] = players[consoleplayer].mo->z;
		g_bSavedOnFloor[ulIdx] = false;
	}
}

//*****************************************************************************
//*****************************************************************************
//
bool CLIENT_PREDICT_IsPredicting( void )
{
	return ( g_bPredicting );
}

//*****************************************************************************
//*****************************************************************************
//
static fixed_t client_predict_GetPredictedFloorZ( player_t *pPlayer, const ULONG Tick )
{
	// [BB] Using g_SavedFloorZ when the player is on a lowering floor seems to make things very laggy,
	// this does not happen when using mo->floorz.
	return g_bSavedOnMobj[Tick % CLIENT_PREDICTION_TICS] ? g_SavedFloorZ[Tick % CLIENT_PREDICTION_TICS] : pPlayer->mo->floorz;
}

//*****************************************************************************
//
static void client_predict_SaveOnGroundStatus( const player_t *pPlayer, const ULONG Tick )
{
	const ULONG tickIndex = Tick % CLIENT_PREDICTION_TICS;
	// Save the player's "on the floor" status. If the player was on the floor prior to moving
	// back to the player's saved position, move him back onto the floor after moving him.
	// [BB] Standing on an actor (like a bridge) needs special treatment.
	const AActor *pActor = (pPlayer->mo->flags2 & MF2_ONMOBJ) ? P_CheckOnmobj ( pPlayer->mo ) : NULL;
	if ( pActor == NULL ) {
		g_bSavedOnFloor[tickIndex] = pPlayer->mo->z == pPlayer->mo->floorz;
		g_SavedFloorZ[tickIndex] = pPlayer->mo->floorz;
	}
	else
	{
		g_bSavedOnFloor[tickIndex] = ( pPlayer->mo->z == pActor->z + pActor->height );
		g_SavedFloorZ[tickIndex] = pActor->z + pActor->height;
	}

	// [BB] Remember whether the player was standing on another actor.
	g_bSavedOnMobj[Tick % CLIENT_PREDICTION_TICS] = !!(pPlayer->mo->flags2 & MF2_ONMOBJ);
}

//*****************************************************************************
//
static void client_predict_BeginPrediction( player_t *pPlayer )
{
	g_SavedAngle[g_ulGameTick % CLIENT_PREDICTION_TICS] = pPlayer->mo->angle;
	g_SavedPitch[g_ulGameTick % CLIENT_PREDICTION_TICS] = pPlayer->mo->pitch;
	g_SavedCrouchfactor[g_ulGameTick % CLIENT_PREDICTION_TICS] = pPlayer->crouchfactor;
	g_lSavedJumpTicks[g_ulGameTick % CLIENT_PREDICTION_TICS] = pPlayer->jumpTics;
	g_SavedTurnTicks[g_ulGameTick % CLIENT_PREDICTION_TICS] = pPlayer->turnticks;
	g_lSavedReactionTime[g_ulGameTick % CLIENT_PREDICTION_TICS] = pPlayer->mo->reactiontime;
	g_lSavedWaterLevel[g_ulGameTick % CLIENT_PREDICTION_TICS] = pPlayer->mo->waterlevel;
	memcpy( &g_SavedTiccmd[g_ulGameTick % CLIENT_PREDICTION_TICS], &pPlayer->cmd, sizeof( ticcmd_t ));
}

//*****************************************************************************
//
static void client_predict_DoPrediction( player_t *pPlayer, ULONG ulTicks )
{
	TThinkerIterator<DPusher> pusherIt;
	DPusher *pusher = NULL;

	LONG lTick = g_ulGameTick - ulTicks;

	// [BB] The server moved us to a postion above the floor and into a sector without a moving floor,
	// so don't glue us to the floor for this tic.
	if ( ( g_bSavedOnMobj[lTick % CLIENT_PREDICTION_TICS] == false ) && ( pPlayer->mo->z > pPlayer->mo->floorz )
		&& pPlayer->mo->Sector && !pPlayer->mo->Sector->floordata )
		g_bSavedOnFloor[lTick % CLIENT_PREDICTION_TICS] = false;

	// [BB] The server gave us z-velocity, so don't glue us to a floor or an actor for this tic.
	if ( pPlayer->mo->velz > 0 )
	{
		g_bSavedOnFloor[lTick % CLIENT_PREDICTION_TICS] = false;
		g_bSavedOnMobj[lTick % CLIENT_PREDICTION_TICS] = false;
	}

	// [BB] Restore the saved "on ground" status.
	if ( g_bSavedOnMobj[lTick % CLIENT_PREDICTION_TICS] )
		pPlayer->mo->flags2 |= MF2_ONMOBJ;
	else
		pPlayer->mo->flags2 &= ~MF2_ONMOBJ;
	pPlayer->onground = g_bSavedOnFloor[lTick % CLIENT_PREDICTION_TICS];
	if ( g_bSavedOnFloor[lTick % CLIENT_PREDICTION_TICS] )
		pPlayer->mo->z = client_predict_GetPredictedFloorZ ( pPlayer, lTick );

	while ( ulTicks )
	{
		// Disable bobbing, sounds, etc.
		g_bPredicting = true;

		// Use backed up values for prediction.
		pPlayer->mo->angle = g_SavedAngle[lTick % CLIENT_PREDICTION_TICS];
		pPlayer->mo->pitch = g_SavedPitch[lTick % CLIENT_PREDICTION_TICS];
		// [BB] Crouch prediction seems to be very tricky. While predicting, we don't recalculate
		// crouchfactor, but just use the value we already calculated before.
		pPlayer->crouchfactor = g_SavedCrouchfactor[( lTick + 1 )% CLIENT_PREDICTION_TICS];
		pPlayer->jumpTics = g_lSavedJumpTicks[lTick % CLIENT_PREDICTION_TICS];
		pPlayer->turnticks = g_SavedTurnTicks[lTick % CLIENT_PREDICTION_TICS];
		pPlayer->mo->reactiontime = g_lSavedReactionTime[lTick % CLIENT_PREDICTION_TICS];
		pPlayer->mo->waterlevel = g_lSavedWaterLevel[lTick % CLIENT_PREDICTION_TICS];

		// Tick the player.
		P_PlayerThink( pPlayer, &g_SavedTiccmd[lTick % CLIENT_PREDICTION_TICS] );
		pPlayer->mo->Tick( );

		// [BB] The effect of all DPushers needs to be manually predicted.
		pusherIt.Reinit();
		while (( pusher = pusherIt.Next() ))
			pusher->Tick();

		// [BB] Save the new "on ground" status (which correspond to the start of the next tic).
		// It is based on the latest position the server sent us, so it's more accurate than
		// the older predicted values.
		client_predict_SaveOnGroundStatus ( pPlayer, lTick+1 );

		ulTicks--;
		lTick++;
	}
}

//*****************************************************************************
//
static void client_predict_EndPrediction( player_t *pPlayer )
{
	pPlayer->mo->angle = g_SavedAngle[g_ulGameTick % CLIENT_PREDICTION_TICS];
	pPlayer->mo->pitch = g_SavedPitch[g_ulGameTick % CLIENT_PREDICTION_TICS];
	pPlayer->crouchfactor = g_SavedCrouchfactor[g_ulGameTick % CLIENT_PREDICTION_TICS];
	pPlayer->jumpTics = g_lSavedJumpTicks[g_ulGameTick % CLIENT_PREDICTION_TICS];
	pPlayer->turnticks = g_SavedTurnTicks[g_ulGameTick % CLIENT_PREDICTION_TICS];
	pPlayer->mo->reactiontime = g_lSavedReactionTime[g_ulGameTick % CLIENT_PREDICTION_TICS];
	pPlayer->mo->waterlevel = g_lSavedWaterLevel[g_ulGameTick % CLIENT_PREDICTION_TICS];
}
