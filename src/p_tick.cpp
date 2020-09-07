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
// $Log:$
//
// DESCRIPTION:
//		Ticker.
//
//-----------------------------------------------------------------------------


#include "p_local.h"
#include "p_effect.h"
#include "c_console.h"
#include "s_sound.h"
#include "doomstat.h"
#include "sbar.h"
#include "r_data/r_interpolate.h"
#include "i_sound.h"
#include "g_level.h"
// [BB] New #includes.
#include "g_game.h"
#include "team.h"
#include "network.h"
#include "sv_commands.h"
#include "cl_demo.h"
#include "cl_main.h"
#include "astar.h"
#include "botpath.h"

extern gamestate_t wipegamestate;

//==========================================================================
//
// P_CheckTickerPaused
//
// Returns true if the ticker should be paused. In that cause, it also
// pauses sound effects and possibly music. If the ticker should not be
// paused, then it returns false but does not unpause anything.
//
//==========================================================================

bool P_CheckTickerPaused ()
{
	// [BB] A paused demo always pauses the ticker.
	if ( CLIENTDEMO_IsPaused( ) )
		return true;

	// pause if in menu or console and at least one tic has been run
	if (( NETWORK_GetState( ) != NETSTATE_CLIENT )
		 && gamestate != GS_TITLELEVEL
		 && ((menuactive != MENU_Off && menuactive != MENU_OnNoPause) ||
			 ConsoleState == c_down || ConsoleState == c_falling)
		 && !demoplayback
		 && !demorecording
		 && CLIENTDEMO_IsPlaying( ) == false
		 && CLIENTDEMO_IsRecording( ) == false
		 && players[consoleplayer].viewz != 1
		 && wipegamestate == gamestate)
	{
		S_PauseSound (!(level.flags2 & LEVEL2_PAUSE_MUSIC_IN_MENUS), false);
		return true;
	}
	return false;
}

//
// P_Ticker
//
void SERVERCONSOLE_UpdatePlayerInfo( LONG lPlayer, ULONG ulUpdateFlags );
void P_Ticker (void)
{
	int i;
	ULONG	ulIdx;

	// [BC] Don't run this if the server is lagging.
	if ( NETWORK_InClientMode() )
	{
		if (( CLIENT_GetServerLagging( ) == true ) ||
			( players[consoleplayer].mo == NULL ))
		{
			return;
		}
	}

	// [BC] Server doesn't need any of this.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		interpolator.UpdateInterpolations ();
		r_NoInterpolate = true;

		if (!demoplayback)
		{
			// This is a separate slot from the wipe in D_Display(), because this
			// is delayed slightly due to latency. (Even on a singleplayer game!)
	//		GSnd->SetSfxPaused(!!playerswiping, 2);
		}

		// [BB] Allow the free spectate player to move even if the demo is paused.
		if ( CLIENTDEMO_IsPaused() && CLIENTDEMO_IsInFreeSpectateMode() )
			CLIENTDEMO_FreeSpectatorPlayerThink( true );

		// run the tic
		if (paused || P_CheckTickerPaused())
			return;
	}

	P_NewPspriteTick();

	// [BC] Server doesn't need any of this.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
/*		// [BB] ST doesn't do this.
		// [RH] Frozen mode is only changed every 4 tics, to make it work with A_Tracer().
		if ((level.time & 3) == 0)
		{
			if (bglobal.changefreeze)
			{
				bglobal.freeze ^= 1;
				bglobal.changefreeze = 0;
			}
		}
*/
		// [BC] Do a quick check to see if anyone has the freeze time power. If they do,
		// then don't resume the sound, since one of the effects of that power is to shut
		// off the music.
		for (i = 0; i < MAXPLAYERS; i++ )
		{
			if (playeringame[i] && players[i].timefreezer != 0)
				break;
		}

		// [BB] If the freeze command was executed from the console, the sound needs to
		// be resumed. In this case, the music isn't paused. The other check is only meant
		// not to resume the music.
		// [BB] Don't resume the sound while we are skipping. This is important when skipping
		// while the demo is paused.
		if ( ( ( i == MAXPLAYERS ) || ( S_IsMusicPaused () == false ) ) && ( CLIENTDEMO_IsSkipping() == false ) )
			S_ResumeSound (false);
		P_ResetSightCounters (false);

		// Since things will be moving, it's okay to interpolate them in the renderer.
		r_NoInterpolate = false;

		P_ResetSpawnCounters( );

		// Since things will be moving, it's okay to interpolate them in the renderer.
		r_NoInterpolate = false;

		// Don't run particles while in freeze mode.
		if ( !(level.flags2 & LEVEL2_FROZEN) )
		{
			P_ThinkParticles ();	// [RH] make the particles think
		}
	}

	// Predict the console player's position.
	if ( NETWORK_InClientMode() )
	{
		if (( CLIENT_GetServerLagging( ) == false ) && ( CLIENT_GetClientLagging( ) == false ))
			CLIENT_PREDICT_PlayerPredict( );
	}

	if (( botdebug_showcosts ) && ( players[consoleplayer].camera ))
	{
		POS_t	Position;

		Position.x = players[consoleplayer].camera->x;
		Position.y = players[consoleplayer].camera->y;
		ASTAR_ShowCosts( Position );
	}

	if (( NETWORK_GetState( ) != NETSTATE_SERVER ) && ( players[consoleplayer].camera ))
	{
		if ( botdebug_walktest > 0 )
		{
			char				szString[256];
			ULONG				ulTextColor;
			fixed_t				DestX;
			fixed_t				DestY;
			ULONG				ulFlags;
			DHUDMessageFadeOut	*pMsg;

			DestX = players[consoleplayer].camera->x + ( botdebug_walktest * finecosine[players[consoleplayer].camera->angle >> ANGLETOFINESHIFT] );
			DestY = players[consoleplayer].camera->y + ( botdebug_walktest * finesine[players[consoleplayer].camera->angle >> ANGLETOFINESHIFT] );

			szString[0] = 0;
			ulFlags = BOTPATH_TryWalk( players[consoleplayer].camera, players[consoleplayer].camera->x, players[consoleplayer].camera->y, players[consoleplayer].camera->z, DestX, DestY );
			if ( ulFlags > 0 )
			{
				bool	bNeedMark;

				bNeedMark = false;
				if ( ulFlags & BOTPATH_OBSTRUCTED )
				{
					if ( bNeedMark )
						sprintf( szString + strlen ( szString ), " " );
					
					sprintf( szString + strlen ( szString ), "OBSTRUCTED" );
					bNeedMark = true;
				}
				if ( ulFlags & BOTPATH_STAIRS )
				{
					if ( bNeedMark )
						sprintf( szString + strlen ( szString ), " " );
					
					sprintf( szString + strlen ( szString ), "STAIRS" );
					bNeedMark = true;
				}
				if ( ulFlags & BOTPATH_JUMPABLELEDGE )
				{
					if ( bNeedMark )
						sprintf( szString + strlen ( szString ), " " );
					
					sprintf( szString + strlen ( szString ), "JUMPABLELEDGE" );
					bNeedMark = true;
				}
				if ( ulFlags & BOTPATH_DAMAGINGSECTOR )
				{
					if ( bNeedMark )
						sprintf( szString + strlen ( szString ), " " );
					
					sprintf( szString + strlen ( szString ), "DAMAGINGSECTOR" );
					bNeedMark = true;
				}
				if ( ulFlags & BOTPATH_DROPOFF )
				{
					if ( bNeedMark )
						sprintf( szString + strlen ( szString ), " " );
					
					sprintf( szString + strlen ( szString ), "DROPOFF" );
					bNeedMark = true;
				}
				if ( ulFlags & BOTPATH_TELEPORT )
				{
					if ( bNeedMark )
						sprintf( szString + strlen ( szString ), " " );
					
					sprintf( szString + strlen ( szString ), "TELEPORT" );
					bNeedMark = true;
				}
				if ( ulFlags & BOTPATH_DOOR )
				{
					if ( bNeedMark )
						sprintf( szString + strlen ( szString ), " " );
					
					sprintf( szString + strlen ( szString ), "DOOR" );
					bNeedMark = true;
				}

				ulTextColor = CR_RED;
			}
			else
			{
				ulTextColor = CR_GREEN;
				sprintf( szString, "ALL CLEAR!" );
			}

			pMsg = new DHUDMessageFadeOut( SmallFont, szString,
				1.5f,
				0.9f,
				0,
				0,
				(EColorRange)ulTextColor,
				0.f,
				0.35f );

			StatusBar->AttachMessage( pMsg, MAKE_ID('O','B','S','T') );
		}

		if ( botdebug_obstructiontest > 0 )
		{
			char				szString[64];
			ULONG				ulTextColor;
			DHUDMessageFadeOut	*pMsg;

			if ( BOTS_IsPathObstructed( botdebug_obstructiontest, players[consoleplayer].camera ))
			{
				ulTextColor = CR_RED;
				sprintf( szString, "PATH OBSTRUCTED!" );
			}
			else
			{
				ulTextColor = CR_GREEN;
				sprintf( szString, "ALL CLEAR!" );
			}

			pMsg = new DHUDMessageFadeOut( SmallFont, szString,
				1.5f,
				0.9f,
				0,
				0,
				(EColorRange)ulTextColor,
				0.f,
				0.35f );

			StatusBar->AttachMessage( pMsg, MAKE_ID('O','B','S','T') );
		}
	}

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		// Increment individual player time.
		if ( NETWORK_InClientMode() == false )
		{
			if ( playeringame[ulIdx] )
			{
				players[ulIdx].ulTime++;

				// Potentially update the scoreboard or send out an update.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					if (( players[ulIdx].ulTime % ( TICRATE * 60 )) == 0 )
					{
						// Send out the updated time field to all clients.
						SERVERCOMMANDS_UpdatePlayerTime( ulIdx );

						// Update the console as well.
						SERVERCONSOLE_UpdatePlayerInfo( ulIdx, UDF_TIME );
					}
				}
			}
		}

		// Client's "think" every time we get a cmd.
		// [BB] The server has to think for lagging clients, otherwise they aren't affected by things like sector damage.
		if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( players[ulIdx].bIsBot == false ) && ( players[ulIdx].bLagging == false ) )
			continue;

		// [BB] Assume lagging players are not pressing any buttons.
		if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( players[ulIdx].bIsBot == false ) && ( players[ulIdx].bLagging ) )
			memset( &(players[ulIdx].cmd), 0, sizeof( ticcmd_t ));

		// Console player thinking is handled by player prediction.
		if (( static_cast<signed> (ulIdx) == consoleplayer ) &&
			NETWORK_InClientMode() )
		{
			continue;
		}

		if ( playeringame[ulIdx] )
			P_PlayerThink( &players[ulIdx] );
	}

	// [BB] If we are playing a demo in free spectate mode, we also need to let the special free
	// spectator player think. That's necessary to move this player and thus to move the camera.
	if ( CLIENTDEMO_IsInFreeSpectateMode() )
		CLIENTDEMO_FreeSpectatorPlayerThink();

	// [BB] The server has no status bar.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		StatusBar->Tick ();		// [RH] moved this here
	level.Tick ();			// [RH] let the level tick

	// [BB] Some things like AMovingCamera rely on the AActor tid in the PostBeginPlay functions,
	// which are called by DThinker::RunThinkers (). The client only knows these tids once the
	// server send him a full update, i.e. CLIENT_GetConnectionState() == CTS_ACTIVE.
	// I have no idea if this has unwanted side effects. Has to be checked.
	if(( NETWORK_GetState( ) != NETSTATE_CLIENT ) || (CLIENT_GetConnectionState() == CTS_ACTIVE))
		DThinker::RunThinkers ();

	// Don't do this stuff while in freeze mode.
	if ( !(level.flags2 & LEVEL2_FROZEN) )
	{
		for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			if (( playeringame[ulIdx] ) && ( players[ulIdx].pSkullBot ))
			{
				// Also, if they have an enemy, and can see it, update their known enemy position.
				if (( players[ulIdx].pSkullBot->m_ulPlayerEnemy != MAXPLAYERS ) && ( P_CheckSight( players[ulIdx].mo, players[players[ulIdx].pSkullBot->m_ulPlayerEnemy].mo, SF_SEEPASTBLOCKEVERYTHING )))
					players[ulIdx].pSkullBot->SetEnemyPosition( players[players[ulIdx].pSkullBot->m_ulPlayerEnemy].mo->x, players[players[ulIdx].pSkullBot->m_ulPlayerEnemy].mo->y, players[players[ulIdx].pSkullBot->m_ulPlayerEnemy].mo->z );

				// Now that all the players have moved to their final location for this tick,
				// we can properly aim at them.
				players[ulIdx].pSkullBot->HandleAiming( );
			}
		}

		P_UpdateSpecials ();

		if ( NETWORK_GetState( ) != NETSTATE_SERVER )
			P_RunEffects ();	// [RH] Run particle effects
	}

	// for par times
	level.time++;
	level.maptime++;
	level.totaltime++;

	// Tick the team module. The handles returning dropped flags/skulls.
	if ( teamgame )
	{
		if ( NETWORK_InClientMode() == false )
		{
			TEAM_Tick( );
		}
	}
}
