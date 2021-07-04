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
// Filename: sv_commands.cpp
//
// Description: Contains a set of functions that correspond to each message a server
// can send out. Each functions handles the send out of each message.
//
//-----------------------------------------------------------------------------

#include "chat.h"
#include "cooperative.h"
#include "deathmatch.h"
#include "doomtype.h"
#include "duel.h"
#include "g_game.h"
#include "gamemode.h"
#include "gi.h"
#include "invasion.h"
#include "joinqueue.h"
#include "lastmanstanding.h"
#include "network.h"
#include "p_local.h"
#include "p_spec.h"
#include "r_state.h"
#include "sbar.h"
#include "sv_commands.h"
#include "sv_main.h"
#include "team.h"
#include "survival.h"
#include "vectors.h"
#include "v_palette.h"
#include "r_data/r_translate.h"
#include "domination.h"
#include "p_acs.h"
#include "templates.h"
#include "a_movingcamera.h"
#include "po_man.h"
#include "i_system.h"
#include "r_data/colormaps.h"
#include "network_enums.h"
#include "decallib.h"
#include "network/netcommand.h"
#include "network/servercommands.h"

CVAR (Bool, sv_showwarnings, false, CVAR_GLOBALCONFIG|CVAR_ARCHIVE)

EXTERN_CVAR( Float, sv_aircontrol )

//*****************************************************************************
//	FUNCTIONS

// [BB] Check if the actor has a valid net ID. Returns true if it does, returns false and prints a warning if not.
bool EnsureActorHasNetID( const AActor *pActor )
{
	if ( pActor == NULL )
		return false;

	if ( pActor->lNetID == -1 )
	{
		if ( sv_showwarnings && !( pActor->ulNetworkFlags & NETFL_SERVERSIDEONLY ) )
			Printf ( "Warning: Actor %s doesn't have a netID and therefore can't be manipulated online!\n", pActor->GetClass()->TypeName.GetChars() );
		return false;
	}
	else
		return true;
}

//*****************************************************************************
//
// [BB] Check if the actor already was updated during this tic, and alters ulBits accorindly.
void RemoveUnnecessaryPositionUpdateFlags( AActor *pActor, ULONG &ulBits )
{
	if ( (pActor->lastNetXUpdateTic == gametic) && (pActor->lastX == pActor->x) )
		ulBits  &= ~CM_X;
	if ( (pActor->lastNetYUpdateTic == gametic) && (pActor->lastY == pActor->y) )
		ulBits  &= ~CM_Y;
	if ( (pActor->lastNetZUpdateTic == gametic) && (pActor->lastZ == pActor->z) )
		ulBits  &= ~CM_Z;
	if ( (pActor->lastNetVelXUpdateTic == gametic) && (pActor->lastVelx == pActor->velx) )
		ulBits  &= ~CM_VELX;
	if ( (pActor->lastNetVelYUpdateTic == gametic) && (pActor->lastVely == pActor->vely) )
		ulBits  &= ~CM_VELY;
	if ( (pActor->lastNetVelZUpdateTic == gametic) && (pActor->lastVelz == pActor->velz) )
		ulBits  &= ~CM_VELZ;
	if ( (pActor->lastNetMovedirUpdateTic == gametic) && (pActor->lastMovedir == pActor->movedir) )
		ulBits  &= ~CM_MOVEDIR;
}

//*****************************************************************************
//
// [BB] Mark the actor as updated according to ulBits.
void ActorNetPositionUpdated( AActor *pActor, ULONG &ulBits )
{
	if ( ulBits & CM_X )
	{
		pActor->lastNetXUpdateTic = gametic;
		pActor->lastX = pActor->x;
	}
	if ( ulBits & CM_Y )
	{
		pActor->lastNetYUpdateTic = gametic;
		pActor->lastY = pActor->y;
	}
	if ( ulBits & CM_Z )
	{
		pActor->lastNetZUpdateTic = gametic;
		pActor->lastZ = pActor->z;
	}
	if ( ulBits & CM_VELX )
	{
		pActor->lastNetVelXUpdateTic = gametic;
		pActor->lastVelx = pActor->velx;
	}
	if ( ulBits & CM_VELY )
	{
		pActor->lastNetVelYUpdateTic = gametic;
		pActor->lastVely = pActor->vely;
	}
	if ( ulBits & CM_VELZ )
	{
		pActor->lastNetVelZUpdateTic = gametic;
		pActor->lastVelz = pActor->velz;
	}
	if ( ulBits & CM_MOVEDIR )
	{
		pActor->lastNetMovedirUpdateTic = gametic;
		pActor->lastMovedir = pActor->movedir;
	}
}
//*****************************************************************************
//
// [BB/WS] Tell clients to re-use their last updated position. This saves a lot of bandwidth.
void CheckPositionReuse( AActor *pActor, ULONG &ulBits )
{
	if ( ulBits & CM_X && pActor->lastX == pActor->x )
	{
		ulBits &= ~CM_X;
		ulBits |= CM_REUSE_X;
	}
	if ( ulBits & CM_Y && pActor->lastY == pActor->y )
	{
		ulBits &= ~CM_Y;
		ulBits |= CM_REUSE_Y;
	}
	if ( ulBits & CM_Z && pActor->lastZ == pActor->z )
	{
		ulBits &= ~CM_Z;
		ulBits |= CM_REUSE_Z;
	}
}
//*****************************************************************************
//
void SERVERCOMMANDS_Ping( ULONG ulTime )
{
	ServerCommands::Ping command;
	command.SetTime( ulTime );
	command.sendCommandToClients();
}

//*****************************************************************************
//
void SERVERCOMMANDS_Nothing( ULONG ulPlayer, bool bReliable )
{
	if ( SERVER_IsValidClient( ulPlayer ) == false )
		return;

	ServerCommands::Nothing().sendCommandToClients( ulPlayer, SVCF_ONLYTHISCLIENT );
}

//*****************************************************************************
//
void SERVERCOMMANDS_BeginSnapshot( ULONG ulPlayer )
{
	if ( SERVER_IsValidClient( ulPlayer ) == false )
		return;

	ServerCommands::BeginSnapshot().sendCommandToClients( ulPlayer, SVCF_ONLYTHISCLIENT );
}

//*****************************************************************************
//
void SERVERCOMMANDS_EndSnapshot( ULONG ulPlayer )
{
	if ( SERVER_IsValidClient( ulPlayer ) == false )
		return;

	ServerCommands::EndSnapshot().sendCommandToClients( ulPlayer, SVCF_ONLYTHISCLIENT );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_SpawnPlayer( ULONG ulPlayer, LONG lPlayerState, ULONG ulPlayerExtra, ServerCommandFlags flags, bool bMorph )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::SpawnPlayer command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetPriorState( lPlayerState );
	command.SetIsBot( players[ulPlayer].bIsBot );
	// Do we really need to send this? Shouldn't it always be PST_LIVE?
	command.SetPlayerState( players[ulPlayer].playerstate );
	command.SetIsSpectating( players[ulPlayer].bSpectating );
	command.SetIsDeadSpectator( players[ulPlayer].bDeadSpectator );
	command.SetIsMorphed( bMorph );
	command.SetNetid( players[ulPlayer].mo->lNetID );
	command.SetAngle( players[ulPlayer].mo->angle );
	command.SetX( players[ulPlayer].mo->x );
	command.SetY( players[ulPlayer].mo->y );
	command.SetZ( players[ulPlayer].mo->z );
	command.SetPlayerClass( players[ulPlayer].CurrentPlayerClass );
	// command.addByte( players[ulPlayer].userinfo.GetPlayerClassNum() );
	command.SetMorphedClass( players[ulPlayer].mo ? players[ulPlayer].mo->GetClass() : NULL );
	command.SetMorphStyle( players[ulPlayer].MorphStyle );
	command.sendCommandToClients( ulPlayerExtra, flags );

	// [BB]: If the player still has any cheats activated from the last level, tell
	// him about it. Not doing this leads for example to jerky movement on client side
	// in case of NOCLIP.
	// We don't have to tell about CF_NOTARGET, since it's only used on the server.
	if( players[ulPlayer].cheats && players[ulPlayer].cheats != CF_NOTARGET )
		SERVERCOMMANDS_SetPlayerCheats( ulPlayer, ulPlayer, SVCF_ONLYTHISCLIENT );
}

//*****************************************************************************
//
void SERVERCOMMANDS_MovePlayer( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ULONG ulPlayerFlags = 0;

	if ( PLAYER_IsValidPlayerWithMo( ulPlayer ) == false )
		return;

	ServerCommands::MovePlayer fullCommand;
	fullCommand.SetPlayer ( &players[ulPlayer] );
	fullCommand.SetClientTicOnServerEnd ( SERVER_GetClient( ulPlayer )->ulClientGameTic );
	fullCommand.SetFlags( ulPlayerFlags | PLAYER_VISIBLE );
	fullCommand.SetX( players[ulPlayer].mo->ClientX );
	fullCommand.SetY( players[ulPlayer].mo->ClientY );
	fullCommand.SetZ( players[ulPlayer].mo->ClientZ );
	fullCommand.SetWaterlevel( players[ulPlayer].mo->waterlevel );
	fullCommand.SetAngle( players[ulPlayer].mo->ClientAngle );
	fullCommand.SetPitch( players[ulPlayer].mo->ClientPitch );
	fullCommand.SetVelx( players[ulPlayer].mo->ClientVelX );
	fullCommand.SetVely( players[ulPlayer].mo->ClientVelY );
	fullCommand.SetVelz( players[ulPlayer].mo->ClientVelZ );
	fullCommand.SetUcmd_forwardmove( players[ulPlayer].cmd.ucmd.forwardmove );
	fullCommand.SetUcmd_sidemove( players[ulPlayer].cmd.ucmd.sidemove );
	fullCommand.SetUcmd_upmove( players[ulPlayer].cmd.ucmd.upmove );
	fullCommand.SetUcmd_yaw( players[ulPlayer].cmd.ucmd.yaw );
	fullCommand.SetUcmd_pitch( players[ulPlayer].cmd.ucmd.pitch );
	// fullCommand.SetUcmd_roll( players[ulPlayer].cmd.ucmd.roll );
	fullCommand.SetUcmd_buttons( players[ulPlayer].cmd.ucmd.buttons );

	ServerCommands::MovePlayer stubCommand = fullCommand;
	stubCommand.SetFlags( ulPlayerFlags );

	for ( ClientIterator it ( ulPlayerExtra, flags ); it.notAtEnd(); ++it )
	{
		if ( SERVER_IsPlayerVisible( *it, ulPlayer ))
			fullCommand.sendCommandToClients( *it, SVCF_ONLYTHISCLIENT );
		else
			stubCommand.sendCommandToClients( *it, SVCF_ONLYTHISCLIENT );
	}

	players[ulPlayer].mo->ClientX = players[ulPlayer].mo->x;
	players[ulPlayer].mo->ClientY = players[ulPlayer].mo->y;
	players[ulPlayer].mo->ClientZ = players[ulPlayer].mo->z;
	players[ulPlayer].mo->ClientVelX = players[ulPlayer].mo->velx;
	players[ulPlayer].mo->ClientVelY = players[ulPlayer].mo->vely;
	players[ulPlayer].mo->ClientVelZ = players[ulPlayer].mo->velz;
	players[ulPlayer].mo->ClientAngle = players[ulPlayer].mo->angle;
	players[ulPlayer].mo->ClientPitch = players[ulPlayer].mo->pitch;
}

//*****************************************************************************
//
void SERVERCOMMANDS_DamagePlayer( ULONG ulPlayer )
{
	ULONG		ulArmorPoints;
	AInventory	*pArmor;

	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	// Determine how much armor the damaged player now has, so we can send that information.
	if ( players[ulPlayer].mo == NULL )
		ulArmorPoints = 0;
	else
	{
		// Search the player's inventory for armor.
		pArmor = players[ulPlayer].mo->FindInventory< ABasicArmor >( );

		// If the player doesn't possess any armor, then his armor points are 0.
		ulArmorPoints = ( pArmor != NULL ) ? pArmor->Amount : 0;
	}

	ServerCommands::DamagePlayer fullCommand;
	fullCommand.SetPlayer( &players[ulPlayer] );
	fullCommand.SetHealth( players[ulPlayer].health );
	fullCommand.SetArmor( ulArmorPoints );
	fullCommand.SetAttacker( players[ulPlayer].attacker );

	for ( ClientIterator it; it.notAtEnd(); ++it )
	{
		// [EP] Send the updated health and armor of the player who's being damaged to this client
		// only if this client is allowed to know.
		if ( SERVER_IsPlayerAllowedToKnowHealth( *it, ulPlayer ))
			fullCommand.sendCommandToClients( *it, SVCF_ONLYTHISCLIENT );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_KillPlayer( ULONG ulPlayer, AActor *pSource, AActor *pInflictor, FName MOD )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	const PClass *weaponType = NULL;

	// [TP] FIXME: Wouldn't it be miles easier to find find the killing player with pSource->player?
	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( playeringame[ulIdx] == false ) ||
			( players[ulIdx].mo == NULL ))
		{
			continue;
		}

		if ( players[ulIdx].mo == pSource )
		{
			if ( players[ulIdx].ReadyWeapon != NULL )
				weaponType = players[ulIdx].ReadyWeapon->GetClass();
			break;
		}
	}

	ServerCommands::KillPlayer command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetSource( pSource );
	command.SetInflictor( pInflictor );
	// [BB] We only send the health as short, so make sure that it doesn't exceed the corresponding range.
	command.SetHealth( clamp ( players[ulPlayer].mo->health, SHRT_MIN, SHRT_MAX ));
	command.SetMOD( MOD.GetChars() );
	command.SetDamageType( players[ulPlayer].mo->DamageType.GetChars() );
	command.SetWeaponType( weaponType );
	command.sendCommandToClients();
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerHealth( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::SetPlayerHealth command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetHealth( players[ulPlayer].health );

	for ( ClientIterator it ( ulPlayerExtra, flags ); it.notAtEnd(); ++it )
	{
		if ( SERVER_IsPlayerAllowedToKnowHealth( *it, ulPlayer ))
			command.sendCommandToClients( *it, SVCF_ONLYTHISCLIENT );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerArmor( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayerWithMo( ulPlayer ) == false )
		return;

	AInventory *pArmor = players[ulPlayer].mo->FindInventory< ABasicArmor >( );

	if ( pArmor == NULL )
		return;

	ServerCommands::SetPlayerArmor command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetArmorAmount( pArmor->Amount );
	command.SetArmorIcon( pArmor->Icon.isValid() ? TexMan( pArmor->Icon )->Name : "" );

	for ( ClientIterator it ( ulPlayerExtra, flags ); it.notAtEnd(); ++it )
	{
		if ( SERVER_IsPlayerAllowedToKnowHealth( *it, ulPlayer ))
			command.sendCommandToClients( *it, SVCF_ONLYTHISCLIENT );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerHealthAndMaxHealthBonus( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayerWithMo( ulPlayer ) == false )
		return;

	// [BB] Workaround to set max health bonus for the player on the client(s).
	if ( players[ulPlayer].lMaxHealthBonus > 0 ) {
		AInventory *pInventory = Spawn<AMaxHealth>(0,0,0, NO_REPLACE);
		if ( pInventory )
		{
			pInventory->Amount = players[ulPlayer].lMaxHealthBonus;
			SERVERCOMMANDS_GiveInventory( ulPlayer, pInventory, ulPlayerExtra, flags );
			pInventory->Destroy ();
			pInventory = NULL;
		}
	}
	SERVERCOMMANDS_SetPlayerHealth( ulPlayer, ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerArmorAndMaxArmorBonus( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayerWithMo( ulPlayer ) == false )
		return;

	// [BB] Workaround to set max armor bonus for the player on the client(s).
	ABasicArmor *pArmor = players[ulPlayer].mo->FindInventory<ABasicArmor>( );

	if ( pArmor && ( pArmor->BonusCount > 0 ) )
	{
		AInventory *pInventory = static_cast<AInventory*> ( Spawn( PClass::FindClass( "MaxArmorBonus" ), 0,0,0, NO_REPLACE) );
		if ( pInventory )
		{
			pInventory->Amount = pArmor->BonusCount;
			SERVERCOMMANDS_GiveInventory( ulPlayer, pInventory, ulPlayerExtra, flags );
			pInventory->Destroy ();
			pInventory = NULL;
		}
	}
	SERVERCOMMANDS_SetPlayerArmor( ulPlayer, ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerState( ULONG ulPlayer, PLAYERSTATE_e ulState, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::SetPlayerState command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetState( ulState );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetAllPlayerUserInfo( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	userinfo_t &userinfo = players[ulPlayer].userinfo;
	userinfo_t::Iterator iterator ( userinfo );
	std::set<FName> cvarNames;

	for ( userinfo_t::Pair *pair; iterator.NextPair( pair ); )
		cvarNames.insert( pair->Key );

	SERVERCOMMANDS_SetPlayerUserInfo( ulPlayer, cvarNames, ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerUserInfo( ULONG ulPlayer, const std::set<FName> &names, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if (( PLAYER_IsValidPlayer( ulPlayer ) == false ) || ( names.size() == 0 ))
		return;

	ServerCommands::SetPlayerUserInfo command;
	command.SetPlayer( &players[ulPlayer] );

	for ( FName name : names )
	{
		FBaseCVar **cvarPointer = players[ulPlayer].userinfo.CheckKey( name );
		FBaseCVar *cvar = cvarPointer ? *cvarPointer : nullptr;

		if ( cvar )
		{
			ServerCommands::CVar element;
			element.name = name;
			// [BB] Skin needs special treatment, so that the clients can use skins the server doesn't have.
			if ( name == NAME_Skin )
				element.value = SERVER_GetClient( ulPlayer )->szSkin;
			else
				element.value = cvar->GetGenericRep( CVAR_String ).String;

			command.PushToCvars( element );

			// [BB] The new element won't fit in a single packet. Send what we have so far
			// and put the new command into the next packet.
			if ( static_cast<ULONG>( command.BuildNetCommand().calcSize() ) + PACKET_HEADER_SIZE >= SERVER_GetMaxPacketSize( ) )
			{
				command.PopFromCvars( element );
				command.sendCommandToClients( ulPlayerExtra, flags );
				command.ClearCvars();
				command.PushToCvars( element );
			}
		}
	}

	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerAccountName( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetPlayerAccountName command;
	command.SetPlayer( &players[ulPlayer] );
	// [TP] Redact the account name if the player so wishes.
	if ( SERVER_GetClient( ulPlayer )->WantHideAccount )
		command.SetAccountName( "" );
	else
		command.SetAccountName( SERVER_GetClient( ulPlayer )->GetAccountName() );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerFrags( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::SetPlayerFrags command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetFragCount( players[ulPlayer].fragcount );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerPoints( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::SetPlayerPoints command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetPointCount( players[ulPlayer].lPointCount );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerWins( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::SetPlayerWins command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetWins( players[ulPlayer].ulWins );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerDeaths( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::SetPlayerDeaths command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetDeathCount( players[ulPlayer].ulDeathCount );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerKillCount( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::SetPlayerKillCount command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetKillCount( players[ulPlayer].killcount );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerChatStatus( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetPlayerChatStatus command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetChatting( players[ulPlayer].bChatting );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerLaggingStatus( ULONG ulPlayer )
{
	ServerCommands::SetPlayerLaggingStatus command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetLagging( players[ulPlayer].bLagging );
	command.sendCommandToClients();
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerConsoleStatus( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetPlayerConsoleStatus command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetInConsole( players[ulPlayer].bInConsole );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerMenuStatus(ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags)
{
	ServerCommands::SetPlayerMenuStatus command;
	command.SetPlayer(&players[ulPlayer]);
	command.SetInMenu(players[ulPlayer].bInMenu);
	command.sendCommandToClients(ulPlayerExtra, flags);
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerReadyToGoOnStatus( ULONG ulPlayer )
{
	ServerCommands::SetPlayerReadyToGoOnStatus command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetReadyToGoOn( players[ulPlayer].bReadyToGoOn );
	command.sendCommandToClients();
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerTeam( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::SetPlayerTeam command;
	command.SetPlayer( &players[ulPlayer] );

	if ( players[ulPlayer].bOnTeam == false )
		command.SetTeam( teams.Size( ));
	else
		command.SetTeam( players[ulPlayer].ulTeam );

	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerCamera( ULONG ulPlayer, AActor *camera, bool bRevertPlease )
{
	if ( SERVER_IsValidClient( ulPlayer ) == false )
		return;

	ServerCommands::SetPlayerCamera command;
	command.SetCamera( camera );
	command.SetRevertPlease( bRevertPlease );
	command.sendCommandToClients( ulPlayer, SVCF_ONLYTHISCLIENT );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerPoisonCount( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::SetPlayerPoisonCount command;
	command.SetPlayer ( &players[ulPlayer] );
	command.SetPoisonCount( players[ulPlayer].poisoncount );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerAmmoCapacity( ULONG ulPlayer, AInventory *pAmmo, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if (( PLAYER_IsValidPlayer( ulPlayer ) == false ) ||
		( pAmmo == NULL ) ||
		( pAmmo->GetClass()->IsDescendantOf (RUNTIME_CLASS(AAmmo)) == false ))
	{
		return;
	}

	ServerCommands::SetPlayerAmmoCapacity command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetAmmoType( pAmmo->GetClass( ) );
	command.SetMaxAmount( pAmmo->MaxAmount );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerCheats( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::SetPlayerCheats command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetCheats( players[ulPlayer].cheats );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerPendingWeapon( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	if (( players[ulPlayer].PendingWeapon == WP_NOCHANGE ) ||
		( players[ulPlayer].PendingWeapon == NULL ))
	{
		return;
	}

	// Only send this info to spectators.
	// [BB] Or if this is a COOP game.
	// [BB] Everybody needs to know this. Otherwise the Railgun sound is broken and spying in demos doesn't work properly.
	ServerCommands::SetPlayerPendingWeapon command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetWeaponType( players[ulPlayer].PendingWeapon->GetClass() );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerPSprite( ULONG ulPlayer, FState *pState, LONG lPosition, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	if ( players[ulPlayer].ReadyWeapon == NULL )
		return;

	const PClass *stateOwner = FState::StaticFindStateOwner( pState );

	if ( stateOwner && stateOwner->ActorInfo && stateOwner->ActorInfo->OwnsState( pState ))
	{
		ServerCommands::SetPlayerPSprite command;
		command.SetPlayer( &players[ulPlayer] );
		command.SetStateOwner( stateOwner );
		command.SetOffset( pState - stateOwner->ActorInfo->OwnedStates );
		command.SetPosition( lPosition );
		command.sendCommandToClients( ulPlayerExtra, flags );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerBlend( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::SetPlayerBlend command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetBlendR( players[ulPlayer].BlendR );
	command.SetBlendG( players[ulPlayer].BlendG );
	command.SetBlendB( players[ulPlayer].BlendB );
	command.SetBlendA( players[ulPlayer].BlendA );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerMaxHealth( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false || players[ulPlayer].mo == NULL )
		return;

	ServerCommands::SetPlayerMaxHealth command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetMaxHealth( players[ulPlayer].mo->MaxHealth );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerLivesLeft( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::SetPlayerLivesLeft command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetLivesLeft( players[ulPlayer].ulLivesLeft );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetPlayerViewHeight( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayerWithMo( ulPlayer ) == false )
		return;

	ServerCommands::SetPlayerViewHeight command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetViewHeight( players[ulPlayer].mo->ViewHeight );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SyncPlayerAmmoAmount( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayerWithMo( ulPlayer ) == false )
		return;

	AInventory *pInventory = NULL;

	for ( pInventory = players[ulPlayer].mo->Inventory; pInventory != NULL; pInventory = pInventory->Inventory )
	{
		if ( pInventory->IsKindOf( RUNTIME_CLASS( AAmmo )) == false )
			continue;

		SERVERCOMMANDS_GiveInventory( ulPlayer, pInventory, ulPlayerExtra, flags );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_UpdatePlayerPing( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::UpdatePlayerPing command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetPing( players[ulPlayer].ulPing );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_UpdatePlayerTime( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::UpdatePlayerTime command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetTime(( players[ulPlayer].ulTime / ( TICRATE * 60 )));
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_MoveLocalPlayer( ULONG ulPlayer )
{
	if ( SERVER_IsValidClient( ulPlayer ) == false || players[ulPlayer].mo == NULL )
		return;

	ServerCommands::MoveLocalPlayer command;
	command.SetClientTicOnServerEnd( SERVER_GetClient( ulPlayer )->ulClientGameTic );
	command.SetLatestServerGametic( gametic );
	command.SetX( players[ulPlayer].mo->x );
	command.SetY( players[ulPlayer].mo->y );
	command.SetZ( players[ulPlayer].mo->z );
	command.SetVelx( players[ulPlayer].mo->velx );
	command.SetVely( players[ulPlayer].mo->vely );
	command.SetVelz( players[ulPlayer].mo->velz );
	command.sendCommandToClients( ulPlayer, SVCF_ONLYTHISCLIENT );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DisconnectPlayer( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::DisconnectPlayer command;
	command.SetPlayer( &players[ulPlayer] );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetConsolePlayer( ULONG ulPlayer )
{
	if ( SERVER_IsValidClient( ulPlayer ) == false )
		return;

	ServerCommands::SetConsolePlayer command;
	command.SetPlayerNumber( ulPlayer );
	command.sendCommandToClients( ulPlayer, SVCF_ONLYTHISCLIENT );
}

//*****************************************************************************
//
void SERVERCOMMANDS_ConsolePlayerKicked( ULONG ulPlayer )
{
	if ( SERVER_IsValidClient( ulPlayer ) == false )
		return;

	ServerCommands::ConsolePlayerKicked command;
	command.sendCommandToClients( ulPlayer, SVCF_ONLYTHISCLIENT );
}

//*****************************************************************************
//
void SERVERCOMMANDS_GivePlayerMedal( ULONG ulPlayer, ULONG ulMedal, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::GivePlayerMedal command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetMedal( ulMedal );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_ResetAllPlayersFragcount( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::ResetAllPlayersFragcount().sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_PlayerIsSpectator( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::PlayerIsSpectator command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetDeadSpectator( players[ulPlayer].bDeadSpectator );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_PlayerSay( ULONG ulPlayer, const char *pszString, ULONG ulMode, bool bForbidChatToPlayers, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulPlayer != MAXPLAYERS && PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::PlayerSay command;
	command.SetPlayerNumber( ulPlayer );
	command.SetMode( ulMode );
	command.SetMessage( pszString );

	for ( ClientIterator it ( ulPlayerExtra, flags ); it.notAtEnd(); ++it )
	{
		// Don't allow the chat message to be broadcasted to this player.
		if (( bForbidChatToPlayers ) && ( players[*it].bSpectating == false ))
			continue;

		// The player is sending a message to his teammates.
		if ( ulMode == CHATMODE_TEAM )
		{
			if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
			{
				// If either player is not on a team, don't send the message.
				if ( (( players[*it].bOnTeam == false ) || ( players[ulPlayer].bOnTeam == false ))
					&& (( PLAYER_IsTrueSpectator ( &players[*it] ) == false ) || ( PLAYER_IsTrueSpectator ( &players[ulPlayer] ) == false )) )
					continue;

				// If the players are not on the same team, don't send the message.
				if ( ( players[*it].ulTeam != players[ulPlayer].ulTeam ) && ( ( PLAYER_IsTrueSpectator ( &players[*it] ) != PLAYER_IsTrueSpectator ( &players[ulPlayer] ) ) || ( PLAYER_IsTrueSpectator ( &players[*it] ) == false ) ) )
					continue;
			}
			// Not in a team mode.
			else
				continue;
		}

		command.sendCommandToClients( *it, SVCF_ONLYTHISCLIENT );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_PlayerTaunt( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::PlayerTaunt command;
	command.SetPlayer( &players[ulPlayer] );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_PlayerRespawnInvulnerability( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::PlayerRespawnInvulnerability command;
	command.SetPlayer( &players[ulPlayer] );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_PlayerUseInventory( ULONG ulPlayer, AInventory *pItem, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if (( PLAYER_IsValidPlayer( ulPlayer ) == false ) ||
		( pItem == NULL ))
	{
		return;
	}

	ServerCommands::PlayerUseInventory command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetItemType( pItem->GetClass() );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_PlayerDropInventory( ULONG ulPlayer, AInventory *pItem, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if (( PLAYER_IsValidPlayer( ulPlayer ) == false ) ||
		( pItem == NULL ))
	{
		return;
	}

	ServerCommands::PlayerDropInventory command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetItemType( pItem->GetClass() );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_PotentiallyIgnorePlayer( ULONG ulPlayer )
{
	for ( ClientIterator it; it.notAtEnd(); ++it )
	{
		// Check whether this player is ignoring the newcomer's address.
		LONG lTicks = SERVER_GetPlayerIgnoreTic( *it, SERVER_GetClient( ulPlayer )->Address );

		if ( lTicks == 0 )
			continue;

		ServerCommands::IgnorePlayer command;
		command.SetPlayer( &players[ulPlayer] );
		command.SetIgnoreChatTicks( lTicks );
		command.sendCommandToClients( ulPlayer, SVCF_ONLYTHISCLIENT );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SpawnThing( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( pActor == NULL )
		return;

	// If the actor doesn't have a network ID, it's better to send it ID-less.
	if ( pActor->lNetID == -1 )
	{
		SERVERCOMMANDS_SpawnThingNoNetID( pActor, ulPlayerExtra, flags );
		return;
	}

	ServerCommands::SpawnThing command;
	command.SetType( pActor->GetClass() );
	command.SetX( pActor->x );
	command.SetY( pActor->y );
	command.SetZ( pActor->z );
	command.SetId( pActor->lNetID );
	command.sendCommandToClients( ulPlayerExtra, flags );

	if ( pActor->ulSTFlags & STFL_LEVELSPAWNED )
	{
		SERVERCOMMANDS_SetThingFlags( pActor, FLAGSET_FLAGS, ulPlayerExtra, flags );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SpawnThingNoNetID( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( pActor == NULL )
		return;

	if ( pActor->ulNetworkFlags & NETFL_SERVERSIDEONLY )
		return;

	ServerCommands::SpawnThingNoNetID command;
	command.SetType( pActor->GetClass() );
	command.SetX( pActor->x );
	command.SetY( pActor->y );
	command.SetZ( pActor->z );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_LevelSpawnThing( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( pActor == NULL )
		return;

	// If the actor doesn't have a network ID, it's better to send it ID-less.
	if ( pActor->lNetID == -1 )
	{
		SERVERCOMMANDS_LevelSpawnThingNoNetID( pActor, ulPlayerExtra, flags );
		return;
	}

	ServerCommands::LevelSpawnThing command;
	command.SetType( pActor->GetClass() );
	command.SetX( pActor->x );
	command.SetY( pActor->y );
	command.SetZ( pActor->z );
	command.SetId( pActor->lNetID );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_LevelSpawnThingNoNetID( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( pActor == NULL )
		return;

	if ( pActor->ulNetworkFlags & NETFL_SERVERSIDEONLY )
		return;

	ServerCommands::LevelSpawnThingNoNetID command;
	command.SetType( pActor->GetClass() );
	command.SetX( pActor->x );
	command.SetY( pActor->y );
	command.SetZ( pActor->z );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

/*
 * [TP] Compares actor position data to a previous state and calls SERVERCOMMANDS_MoveThing to send appropriate updates.
 */
void SERVERCOMMANDS_MoveThingIfChanged( AActor *actor, const MoveThingData &oldData, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( EnsureActorHasNetID( actor ) )
	{
		ULONG bits = 0;

		if ( actor->x != oldData.x )
			bits |= CM_X;

		if ( actor->y != oldData.y )
			bits |= CM_Y;

		if ( actor->z != oldData.z )
			bits |= CM_Z;

		if ( actor->velx != oldData.velx )
			bits |= CM_VELX;

		if ( actor->vely != oldData.vely )
			bits |= CM_VELY;

		if ( actor->velz != oldData.velz )
			bits |= CM_VELZ;

		if ( actor->angle != oldData.angle )
			bits |= CM_ANGLE;

		if ( actor->pitch != oldData.pitch )
			bits |= CM_PITCH;

		if ( actor->movedir != oldData.movedir )
			bits |= CM_MOVEDIR;

		if ( bits != 0 )
			SERVERCOMMANDS_MoveThing( actor, bits, ulPlayerExtra, flags );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_MoveThing( AActor *actor, ULONG bits, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (actor) )
		return;

	// [BB] Only skip updates, if sent to all players.
	if ( flags == 0 )
		RemoveUnnecessaryPositionUpdateFlags ( actor, bits );
	else // [WS] This will inform clients not to set their lastX/Y/Z with the new position.
		bits |= CM_NOLAST;

	// [WS] Check to see if the position can be re-used by the client.
	CheckPositionReuse( actor, bits );

	// Nothing to update.
	if ( bits == 0 )
		return;

	ServerCommands::MoveThing command;
	command.SetActor( actor );
	command.SetBits( bits );
	command.SetNewX( actor->x );
	command.SetNewY( actor->y );
	command.SetNewZ( actor->z );
	command.SetLastX( actor->lastX );
	command.SetLastY( actor->lastY );
	command.SetLastZ( actor->lastZ );
	command.SetAngle( actor->angle );
	command.SetVelX( actor->velx );
	command.SetVelY( actor->vely );
	command.SetVelZ( actor->velz );
	command.SetPitch( actor->pitch );
	command.SetMovedir( actor->movedir );
	command.sendCommandToClients( ulPlayerExtra, flags );

	// [BB] Only mark something as updated, if it the update was sent to all players.
	if ( flags == 0 )
		ActorNetPositionUpdated ( actor, bits );
}

//*****************************************************************************
//
void SERVERCOMMANDS_KillThing( AActor *pActor, AActor *pSource, AActor *pInflictor )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::KillThing command;
	command.SetVictim( pActor );
	command.SetSource( pSource );
	command.SetInflictor( pInflictor );
	command.SetDamageType( pActor->DamageType.GetChars() );
	command.SetHealth( pActor->health );
	command.sendCommandToClients();
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingState( AActor *pActor, NetworkActorState state, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::SetThingState command;
	command.SetActor( pActor );
	command.SetState( state );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingTarget( AActor *pActor )
{
	if ( !EnsureActorHasNetID (pActor) || !EnsureActorHasNetID(pActor->target) )
		return;

	ServerCommands::SetThingTarget command;
	command.SetActor( pActor );
	command.SetTarget( pActor->target );
	command.sendCommandToClients();
}

//*****************************************************************************
//
void SERVERCOMMANDS_DestroyThing( AActor *pActor )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::DestroyThing command;
	command.SetActor( pActor );
	command.sendCommandToClients();
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingAngle( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::SetThingAngle command;
	command.SetActor( pActor );
	command.SetAngle( pActor->angle );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingWaterLevel( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::SetThingWaterLevel command;
	command.SetActor( pActor );
	command.SetWaterlevel( pActor->waterlevel );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingFlags( AActor *pActor, FlagSet flagset, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	int actorFlags;

	switch ( flagset )
	{
		case FLAGSET_FLAGS:		actorFlags = pActor->flags; break;
		case FLAGSET_FLAGS2:	actorFlags = pActor->flags2; break;
		case FLAGSET_FLAGS3:	actorFlags = pActor->flags3; break;
		case FLAGSET_FLAGS4:	actorFlags = pActor->flags4; break;
		case FLAGSET_FLAGS5:	actorFlags = pActor->flags5; break;
		case FLAGSET_FLAGS6:	actorFlags = pActor->flags6; break;
		case FLAGSET_FLAGS7:	actorFlags = pActor->flags7; break;
		case FLAGSET_FLAGS8:	actorFlags = pActor->flags8; break;
		case FLAGSET_FLAGSST:	actorFlags = pActor->ulSTFlags; break;
		case FLAGSET_MVFLAGS:	actorFlags = pActor->mvFlags; break;
		default: return;
	}

	ServerCommands::SetThingFlags command;
	command.SetActor( pActor );
	command.SetFlagset( flagset );
	command.SetFlags( actorFlags );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_UpdateThingFlagsNotAtDefaults( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( pActor->flags != pActor->GetDefault( )->flags )
	{
		SERVERCOMMANDS_SetThingFlags( pActor, FLAGSET_FLAGS, ulPlayerExtra, flags );
	}
	if ( pActor->flags2 != pActor->GetDefault( )->flags2 )
	{
		SERVERCOMMANDS_SetThingFlags( pActor, FLAGSET_FLAGS2, ulPlayerExtra, flags );
	}
	if ( pActor->flags3 != pActor->GetDefault( )->flags3 )
	{
		SERVERCOMMANDS_SetThingFlags( pActor, FLAGSET_FLAGS3, ulPlayerExtra, flags );
	}
	if ( pActor->flags4 != pActor->GetDefault( )->flags4 )
	{
		SERVERCOMMANDS_SetThingFlags( pActor, FLAGSET_FLAGS4, ulPlayerExtra, flags );
	}
	if ( pActor->flags5 != pActor->GetDefault( )->flags5 )
	{
		SERVERCOMMANDS_SetThingFlags( pActor, FLAGSET_FLAGS5, ulPlayerExtra, flags );
	}
	if ( pActor->flags6 != pActor->GetDefault( )->flags6 )
	{
		SERVERCOMMANDS_SetThingFlags( pActor, FLAGSET_FLAGS6, ulPlayerExtra, flags );
	}
	if ( pActor->flags7 != pActor->GetDefault( )->flags7 )
	{
		SERVERCOMMANDS_SetThingFlags( pActor, FLAGSET_FLAGS7, ulPlayerExtra, flags );
	}
	// [BB] ulSTFlags is intentionally left out here.
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingArguments( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::SetThingArguments command;
	command.SetActor( pActor );
	command.SetArg0( pActor->args[0] );
	command.SetArg1( pActor->args[1] );
	command.SetArg2( pActor->args[2] );
	command.SetArg3( pActor->args[3] );
	command.SetArg4( pActor->args[4] );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingTranslation( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::SetThingTranslation command;
	command.SetActor( pActor );
	command.SetTranslation( pActor->Translation );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingSound( AActor *pActor, ULONG ulSound, const char *pszSound, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::SetThingSound command;
	command.SetActor( pActor );
	command.SetSoundType( ulSound );
	command.SetSound( pszSound );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingSpawnPoint( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::SetThingSpawnPoint command;
	command.SetActor( pActor );
	command.SetSpawnPointX( pActor->SpawnPoint[0] );
	command.SetSpawnPointY( pActor->SpawnPoint[1] );
	command.SetSpawnPointZ( pActor->SpawnPoint[2] );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingSpecial( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::SetThingSpecial command;
	command.SetActor( pActor );
	command.SetSpecial( pActor->special );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingSpecial1( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::SetThingSpecial1 command;
	command.SetActor( pActor );
	command.SetSpecial1( pActor->special1 );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingSpecial2( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::SetThingSpecial2 command;
	command.SetActor( pActor );
	command.SetSpecial2( pActor->special2 );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingWeaveIndex( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::SetThingWeaveIndex command;
	command.SetActor( pActor );
	command.SetIndexXY( pActor->WeaveIndexXY );
	command.SetIndexZ( pActor->WeaveIndexZ );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingTics( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::SetThingTics command;
	command.SetActor( pActor );
	command.SetTics( pActor->tics );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingReactionTime( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::SetThingReactionTime command;
	command.SetActor( pActor );
	command.SetReactiontime( pActor->reactiontime );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingTID( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::SetThingTID command;
	command.SetActor( pActor );
	command.SetTid( pActor->tid );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingGravity( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::SetThingGravity command;
	command.SetActor( pActor );
	command.SetGravity( pActor->gravity );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingFrame( AActor *pActor, FState *pState, ULONG ulPlayerExtra, ServerCommandFlags flags, bool bCallStateFunction )
{
	if ( !EnsureActorHasNetID (pActor) || (pState == NULL) )
		return;

	// [BB] Special handling of certain states. This perhaps is not necessary
	// but at least saves a little net traffic.
	if ( bCallStateFunction
		 && (ulPlayerExtra == MAXPLAYERS)
		 && (flags == 0)
		)
	{
		if ( pState == pActor->MeleeState )
		{
			SERVERCOMMANDS_SetThingState( pActor, STATE_MELEE );
			return;
		}
		else if ( pState == pActor->MissileState )
		{
			SERVERCOMMANDS_SetThingState( pActor, STATE_MISSILE );
			return;
		}
		else if ( pState == pActor->FindState( NAME_Wound ))
		{
			SERVERCOMMANDS_SetThingState( pActor, STATE_WOUND );
			return;
		}
		else if ( pState == pActor->FindState( NAME_Pain ))
		{
			SERVERCOMMANDS_SetThingState( pActor, STATE_PAIN );
			return;
		}
	}

	const PClass *stateOwner = FState::StaticFindStateOwner( pState );

	if ( stateOwner && stateOwner->ActorInfo && stateOwner->ActorInfo->OwnsState( pState ))
	{
		int offset = pState - stateOwner->ActorInfo->OwnedStates;

		if ( bCallStateFunction )
		{
			ServerCommands::SetThingFrame command;
			command.SetActor( pActor );
			command.SetStateOwner( stateOwner );
			command.SetOffset( offset );
			command.sendCommandToClients( ulPlayerExtra, flags );
		}
		else
		{
			ServerCommands::SetThingFrameNF command;
			command.SetActor( pActor );
			command.SetStateOwner( stateOwner );
			command.SetOffset( offset );
			command.sendCommandToClients( ulPlayerExtra, flags );
		}
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetActorProperty( AActor *pActor, int property, int value, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( pActor == NULL || pActor->lNetID == -1 )
		return;

	ServerCommands::SetActorProperty command;
	command.SetActor( pActor );
	command.SetProperty( property );
	command.SetValue( value );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetWeaponAmmoGive( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	if ( pActor->IsKindOf( RUNTIME_CLASS( AWeapon )) == false )
		return;

	ServerCommands::SetWeaponAmmoGive command;
	command.SetWeapon( static_cast<AWeapon *>( pActor ) );
	command.SetAmmoGive1( static_cast<AWeapon *>( pActor )->AmmoGive1 );
	command.SetAmmoGive2( static_cast<AWeapon *>( pActor )->AmmoGive2 );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_ThingIsCorpse( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::ThingIsCorpse command;
	command.SetActor( pActor );
	command.SetIsMonster( pActor->CountsAsKill() );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_HideThing( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	// [BB] The client will call HideIndefinitely on the actor, which only is possible on AInventory and descendants.
	if ( !EnsureActorHasNetID (pActor) || !(pActor->IsKindOf( RUNTIME_CLASS( AInventory ))) )
		return;

	ServerCommands::HideThing command;
	command.SetItem( static_cast<AInventory*>( pActor ) );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_TeleportThing( AActor *pActor, bool bSourceFog, bool bDestFog, bool bTeleZoom, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::TeleportThing command;
	command.SetActor( pActor );
	command.SetX( pActor->x );
	command.SetY( pActor->y );
	command.SetZ( pActor->z );
	command.SetMomx( pActor->velx );
	command.SetMomy( pActor->vely );
	command.SetMomz( pActor->velz );
	command.SetReactiontime( pActor->reactiontime );
	command.SetAngle( pActor->angle );
	command.SetSourcefog( bSourceFog );
	command.SetDestfog( bDestFog );
	command.SetTeleportzoom( bTeleZoom );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_ThingActivate( AActor *pActor, AActor *pActivator, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::ThingActivate command;
	command.SetActor( pActor );
	command.SetActivator( pActivator );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_ThingDeactivate( AActor *pActor, AActor *pActivator, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::ThingDeactivate command;
	command.SetActor( pActor );
	command.SetActivator( pActivator );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_RespawnDoomThing( AActor *pActor, bool bFog, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::RespawnDoomThing command;
	command.SetActor( pActor );
	command.SetFog( bFog );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_RespawnRavenThing( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::RespawnRavenThing command;
	command.SetActor( pActor );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_SpawnBlood( fixed_t x, fixed_t y, fixed_t z, angle_t dir, int damage, AActor *originator, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( originator == NULL )
		return;

	ServerCommands::SpawnBlood command;
	command.SetX( x );
	command.SetY( y );
	command.SetZ( z );
	command.SetDir( dir );
	command.SetDamage( damage );
	command.SetOriginator( originator );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SpawnBloodSplatter( fixed_t x, fixed_t y, fixed_t z, AActor *originator, bool isBloodSplatter2, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( originator == NULL )
		return;

	if ( isBloodSplatter2 == false )
	{
		ServerCommands::SpawnBloodSplatter command;
		command.SetX( x );
		command.SetY( y );
		command.SetZ( z );
		command.SetOriginator( originator );
		command.sendCommandToClients( ulPlayerExtra, flags );
	}
	else
	{
		ServerCommands::SpawnBloodSplatter2 command;
		command.SetX( x );
		command.SetY( y );
		command.SetZ( z );
		command.SetOriginator( originator );
		command.sendCommandToClients( ulPlayerExtra, flags );
	}
}

//*****************************************************************************
// SpawnPuff is essentially SpawnThing that is treated a bit differently
// by the client. It differs a lot from SpawnPuffNoNetID.
void SERVERCOMMANDS_SpawnPuff( AActor *pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( pActor == NULL )
		return;

	// If the actor doesn't have a network ID, it's better to send it ID-less.
	if ( pActor->lNetID == -1 )
	{
		ULONG ulState = STATE_SPAWN;
		if ( pActor->state == pActor->MeleeState )
			ulState = STATE_MELEE;
		else if ( pActor->state == pActor->FindState( NAME_Crash ) )
			ulState = STATE_CRASH;
		bool bSendTranslation = pActor->Translation != 0;
		SERVERCOMMANDS_SpawnPuffNoNetID( pActor, ulState, bSendTranslation, ulPlayerExtra, flags );
		return;
	}

	ServerCommands::SpawnPuff command;
	command.SetX( pActor->x );
	command.SetY( pActor->y );
	command.SetZ( pActor->z );
	command.SetPufftype( pActor->GetClass( ) );
	command.SetId( pActor->lNetID );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SpawnPuffNoNetID( AActor *pActor, ULONG ulState, bool bSendTranslation, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( pActor == NULL )
		return;

	ServerCommands::SpawnPuffNoNetID command;
	command.SetX( pActor->x );
	command.SetY( pActor->y );
	command.SetZ( pActor->z );
	command.SetPufftype( pActor->GetClass() );
	command.SetStateid( ulState );
	command.SetReceiveTranslation( !!bSendTranslation );
	command.SetTranslation( pActor->Translation );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_Print( const char *pszString, ULONG ulPrintLevel, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::Print command;
	command.SetPrintlevel( ulPrintLevel );
	command.SetMessage( pszString );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_PrintMid( const char *pszString, bool bBold, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::PrintMid command;
	command.SetMessage( pszString );
	command.SetBold( bBold );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_PrintMOTD( const char *pszString, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::PrintMOTD command;
	command.SetMotd( pszString );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_PrintHUDMessage( const char *pszString, float fX, float fY, LONG lHUDWidth, LONG lHUDHeight, LONG lColor, float fHoldTime, const char *pszFont, bool bLog, LONG lID, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::PrintHUDMessage command;
	command.SetMessage( pszString );
	command.SetX( fX );
	command.SetY( fY );
	command.SetHudWidth( lHUDWidth );
	command.SetHudHeight( lHUDHeight );
	command.SetColor( lColor );
	command.SetHoldTime( fHoldTime );
	command.SetFontName( pszFont );
	command.SetLog( bLog );
	command.SetId( lID );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_PrintHUDMessageFadeOut( const char *pszString, float fX, float fY, LONG lHUDWidth, LONG lHUDHeight, LONG lColor, float fHoldTime, float fFadeOutTime, const char *pszFont, bool bLog, LONG lID, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::PrintHUDMessageFadeOut command;
	command.SetMessage( pszString );
	command.SetX( fX );
	command.SetY( fY );
	command.SetHudWidth( lHUDWidth );
	command.SetHudHeight( lHUDHeight );
	command.SetColor( lColor );
	command.SetHoldTime( fHoldTime );
	command.SetFadeOutTime( fFadeOutTime );
	command.SetFontName( pszFont );
	command.SetLog( bLog );
	command.SetId( lID );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_PrintHUDMessageFadeInOut( const char *pszString, float fX, float fY, LONG lHUDWidth, LONG lHUDHeight, LONG lColor, float fHoldTime, float fFadeInTime, float fFadeOutTime, const char *pszFont, bool bLog, LONG lID, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::PrintHUDMessageFadeInOut command;
	command.SetMessage( pszString );
	command.SetX( fX );
	command.SetY( fY );
	command.SetHudWidth( lHUDWidth );
	command.SetHudHeight( lHUDHeight );
	command.SetColor( lColor );
	command.SetHoldTime( fHoldTime );
	command.SetFadeInTime( fFadeInTime );
	command.SetFadeOutTime( fFadeOutTime );
	command.SetFontName( pszFont );
	command.SetLog( bLog );
	command.SetId( lID );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_PrintHUDMessageTypeOnFadeOut( const char *pszString, float fX, float fY, LONG lHUDWidth, LONG lHUDHeight, LONG lColor, float fTypeTime, float fHoldTime, float fFadeOutTime, const char *pszFont, bool bLog, LONG lID, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::PrintHUDMessageTypeOnFadeOut command;
	command.SetMessage( pszString );
	command.SetX( fX );
	command.SetY( fY );
	command.SetHudWidth( lHUDWidth );
	command.SetHudHeight( lHUDHeight );
	command.SetColor( lColor );
	command.SetTypeOnTime( fTypeTime );
	command.SetHoldTime( fHoldTime );
	command.SetFadeOutTime( fFadeOutTime );
	command.SetFontName( pszFont );
	command.SetLog( bLog );
	command.SetId( lID );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_SetGameMode( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetGameMode command;
	command.SetTheCurrentMode( GAMEMODE_GetCurrentMode( ));
	command.SetTheInstaGib( instagib );
	command.SetTheBuckShot( buckshot );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetGameSkill( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetGameSkill command;
	command.SetTheGameSkill( gameskill );
	command.SetTheBotSkill( botskill );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetGameDMFlags( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetGameDmFlags command;
	command.SetTheDmFlags ( dmflags );
	command.SetTheDmFlags2 ( dmflags2 );
	command.SetTheCompatFlags ( compatflags );
	command.SetTheCompatFlags2 ( compatflags2 );
	command.SetTheZacompatFlags ( zacompatflags );
	command.SetTheZadmFlags ( zadmflags );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetGameModeLimits( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetGameModeLimits command;
	command.SetTheFragLimit( fraglimit );
	command.SetTheTimeLimit( timelimit );
	command.SetThePointLimit( pointlimit );
	command.SetTheDuelLimit( duellimit );
	command.SetTheWinLimit( winlimit );
	command.SetTheWaveLimit( wavelimit );
	command.SetTheCheats( sv_cheats );
	command.SetTheFastWeapons( sv_fastweapons );
	command.SetTheMaxLives( sv_maxlives );
	command.SetTheMaxTeams( sv_maxteams );
	// [BB] The value of sv_gravity is irrelevant when sv_gravity hasn't been changed since the map start
	// and the map has a custom gravity value in MAPINFO. level.gravity always contains the used gravity value,
	// so we just send this one instead of sv_gravity.
	command.SetTheLevelGravity( level.gravity );
	// [BB] sv_aircontrol needs to be handled similarly to sv_gravity.
	command.SetTheLevelAirControl( static_cast<float>(level.aircontrol) / 65536.f );
	// [WS] Send in sv_coop_damagefactor.
	command.SetTheCoopDamageFactor( sv_coop_damagefactor );
	// [WS] Send in alwaysapplydmflags.
	command.SetTheAlwaysApplyDmFlags( alwaysapplydmflags );
	// [AM] Send lobby map.
	command.SetTheLobby( static_cast<FString>(lobby) );
	// [TP] Send sv_limitcommands
	command.SetTheLimitCommands( sv_limitcommands );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetGameEndLevelDelay( ULONG ulEndLevelDelay, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetGameEndLevelDelay command;
	command.SetTheEndLevelDelay( ulEndLevelDelay );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetGameModeState( ULONG ulState, ULONG ulCountdownTicks, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetGameModeState command;
	command.SetTheState( ulState );
	command.SetTheCountdownTicks( ulCountdownTicks );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetDuelNumDuels( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetDuelNumDuels command;
	command.SetTheNumDuels( DUEL_GetNumDuels( ));
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetLMSSpectatorSettings( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetLMSSpectatorSettings command;
	command.SetTheLMSSpectatorSettings( lmsspectatorsettings );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetLMSAllowedWeapons( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetLMSAllowedWeapons command;
	command.SetTheLMSAllowedWeapons( lmsallowedweapons );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetInvasionNumMonstersLeft( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetInvasionNumMonstersLeft command;
	command.SetTheNumMonstersLeft( INVASION_GetNumMonstersLeft( ));
	command.SetTheNumArchVilesLeft( INVASION_GetNumArchVilesLeft( ));
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetInvasionWave( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetInvasionWave command;
	command.SetTheInvasionWave( INVASION_GetCurrentWave( ));
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSimpleCTFSTMode( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetSimpleCTFSTMode command;
	command.SetTheSimpleCTFSTMode( !!TEAM_GetSimpleCTFSTMode( ));
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoPossessionArtifactPickedUp( ULONG ulPlayer, ULONG ulTicks, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::DoPossessionArtifactPickedUp command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetTicks( ulTicks );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoPossessionArtifactDropped( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::DoPossessionArtifactDropped command;
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoGameModeFight( ULONG ulCurrentWave, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::DoGameModeFight command;
	command.SetCurrentWave( ulCurrentWave );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoGameModeCountdown( ULONG ulTicks, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::DoGameModeCountdown command;
	command.SetTicks( ulTicks );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoGameModeWinSequence( ULONG ulWinner, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::DoGameModeWinSequence command;
	command.SetWinner( &players[ulWinner] );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetDominationState( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	unsigned int *PointOwners = DOMINATION_PointOwners();

	ServerCommands::SetDominationState command;

	TArray<int> pointOwners;
	for ( unsigned int i = 0; i < DOMINATION_NumPoints(); ++i )
	{
		pointOwners[i] = PointOwners[i];
	}
	command.SetPointOwners( pointOwners );

	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetDominationPointOwnership( ULONG ulPoint, ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetDominationPointOwner command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetPoint( ulPoint );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetTeamFrags( ULONG ulTeam, LONG lFrags, bool bAnnounce, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( TEAM_CheckIfValid( ulTeam ) == false )
		return;

	ServerCommands::SetTeamFrags command;
	command.SetTeam( ulTeam );
	command.SetFrags( lFrags );
	command.SetAnnounce( bAnnounce );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetTeamScore( ULONG ulTeam, LONG lScore, bool bAnnounce, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( TEAM_CheckIfValid( ulTeam ) == false )
		return;

	ServerCommands::SetTeamScore command;
	command.SetTeam( ulTeam );
	command.SetScore( lScore );
	command.SetAnnounce( bAnnounce );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetTeamWins( ULONG ulTeam, LONG lWins, bool bAnnounce, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( TEAM_CheckIfValid( ulTeam ) == false )
		return;

	ServerCommands::SetTeamWins command;
	command.SetTeam( ulTeam );
	command.SetWins( lWins );
	command.SetAnnounce( bAnnounce );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetTeamReturnTicks( ULONG ulTeam, ULONG ulReturnTicks, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	// [BB] Allow teams.Size( ) here, this handles the white flag.
	if (( TEAM_CheckIfValid ( ulTeam ) == false ) && ( ulTeam != teams.Size() ))
		return;

	ServerCommands::SetTeamReturnTicks command;
	command.SetTeam( ulTeam );
	command.SetReturnTicks( ulReturnTicks );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_TeamFlagReturned( ULONG ulTeam, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	// [BB] Allow teams.Size( ) here, this handles the white flag.
	if (( TEAM_CheckIfValid ( ulTeam ) == false ) && ( ulTeam != teams.Size() ))
		return;

	ServerCommands::TeamFlagReturned command;
	command.SetTeam( ulTeam );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_TeamFlagDropped( ULONG ulPlayer, ULONG ulTeam, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::TeamFlagDropped command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetTeam( ulTeam );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SpawnMissile( AActor *pMissile, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( pMissile == NULL )
		return;

	ServerCommands::SpawnMissile command;
	command.SetX( pMissile->x );
	command.SetY( pMissile->y );
	command.SetZ( pMissile->z );
	command.SetVelX( pMissile->velx );
	command.SetVelY( pMissile->vely );
	command.SetVelZ( pMissile->velz );
	command.SetMissileType( pMissile->GetClass() );
	command.SetNetID( pMissile->lNetID );

	if ( pMissile->target )
		command.SetTargetNetID( pMissile->target->lNetID );
	else
		command.SetTargetNetID( -1 );

	command.sendCommandToClients( ulPlayerExtra, flags );

	// [BB] It's possible that the angle can't be derived from the velocity
	// of the missle. In this case the correct angle has to be told to the clients.
 	if( pMissile->angle != R_PointToAngle2( 0, 0, pMissile->velx, pMissile->vely ))
		SERVERCOMMANDS_SetThingAngle( pMissile, ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_MissileExplode( AActor *pMissile, line_t *pLine, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pMissile) )
		return;

	ServerCommands::MissileExplode command;
	command.SetMissile( pMissile );

	if ( pLine != NULL )
		command.SetLineId( pLine - lines );
	else
		command.SetLineId( -1 );

	command.SetX( pMissile->x );
	command.SetY( pMissile->y );
	command.SetZ( pMissile->z );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_WeaponSound( ULONG ulPlayer, const char *pszSound, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::WeaponSound command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetSound( pszSound );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_WeaponChange( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false || players[ulPlayer].ReadyWeapon == NULL )
		return;

	ServerCommands::WeaponChange command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetWeaponType( players[ulPlayer].ReadyWeapon->GetClass() );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_WeaponRailgun( AActor *source, const FVector3 &start, const FVector3 &end, LONG color1, LONG color2, float maxdiff, int railflags, angle_t angleoffset, const PClass* spawnclass, int duration, float sparsity, float drift, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	// Evidently, to draw a railgun trail, there must be a source actor.
	if ( !EnsureActorHasNetID (source) )
		return;

	ServerCommands::WeaponRailgun command;
	command.SetSource( source );
	command.SetStart( start );
	command.SetEnd( end );
	command.SetColor1( color1 );
	command.SetColor2( color2 );
	command.SetMaxdiff( maxdiff );
	command.SetAngleoffset( angleoffset );
	command.SetSpawnclass( spawnclass );
	command.SetDuration( duration );
	command.SetSparsity( sparsity );
	command.SetDrift( drift );
	command.SetFlags( railflags );

	// [TP] Recent ZDoom versions have added more railgun parameters. Add these parameters to the command
	// only if they're not at defaults.
	command.SetExtended( angleoffset != 0
		|| spawnclass != NULL
		|| duration != 0
		|| fabs( sparsity - 1.0f ) > 1e-8
		|| fabs( drift - 1.0f ) > 1e-8 );

	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorFloorPlane( ULONG ulSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::SetSectorFloorPlane command;
	command.SetSector( &sectors[ulSector] );
	command.SetHeight( sectors[ulSector].floorplane.d );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorCeilingPlane( ULONG ulSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::SetSectorCeilingPlane command;
	command.SetSector( &sectors[ulSector] );
	command.SetHeight( sectors[ulSector].ceilingplane.d );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorFloorPlaneSlope( ULONG ulSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::SetSectorFloorPlaneSlope command;
	command.SetSector( &sectors[ulSector] );
	command.SetA( sectors[ulSector].floorplane.a );
	command.SetB( sectors[ulSector].floorplane.b );
	command.SetC( sectors[ulSector].floorplane.c );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorCeilingPlaneSlope( ULONG ulSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::SetSectorCeilingPlaneSlope command;
	command.SetSector( &sectors[ulSector] );
	command.SetA( sectors[ulSector].ceilingplane.a );
	command.SetB( sectors[ulSector].ceilingplane.b );
	command.SetC( sectors[ulSector].ceilingplane.c );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorLightLevel( ULONG ulSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::SetSectorLightLevel command;
	command.SetSector( &sectors[ulSector] );
	command.SetLightLevel( sectors[ulSector].lightlevel );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorColor( ULONG ulSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::SetSectorColor command;
	command.SetSector( &sectors[ulSector] );
	command.SetRed( sectors[ulSector].ColorMap->Color.r );
	command.SetGreen( sectors[ulSector].ColorMap->Color.g );
	command.SetBlue( sectors[ulSector].ColorMap->Color.b );
	command.SetDesaturate( sectors[ulSector].ColorMap->Desaturate );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorColorByTag( ULONG ulTag, ULONG ulRed, ULONG ulGreen, ULONG ulBlue, ULONG ulDesaturate, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetSectorColorByTag command;
	command.SetTag( ulTag );
	command.SetRed( ulRed );
	command.SetGreen( ulGreen );
	command.SetBlue( ulBlue );
	command.SetDesaturate( ulDesaturate );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorFade( ULONG ulSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::SetSectorFade command;
	command.SetSector( &sectors[ulSector] );
	command.SetRed( sectors[ulSector].ColorMap->Fade.r );
	command.SetGreen( sectors[ulSector].ColorMap->Fade.g );
	command.SetBlue( sectors[ulSector].ColorMap->Fade.b );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorFadeByTag( ULONG ulTag, ULONG ulRed, ULONG ulGreen, ULONG ulBlue, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetSectorFadeByTag command;
	command.SetTag( ulTag );
	command.SetRed( ulRed );
	command.SetGreen( ulGreen );
	command.SetBlue( ulBlue );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorFlat( ULONG ulSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::SetSectorFlat command;
	command.SetSector( &sectors[ulSector] );
	command.SetCeilingFlatName( TexMan( sectors[ulSector].GetTexture( sector_t::ceiling ))->Name );
	command.SetFloorFlatName( TexMan( sectors[ulSector].GetTexture( sector_t::floor ))->Name );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorPanning( ULONG ulSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::SetSectorPanning command;
	command.SetSector( &sectors[ulSector] );
	command.SetCeilingXOffset( sectors[ulSector].GetXOffset( sector_t::ceiling ));
	command.SetCeilingYOffset( sectors[ulSector].GetYOffset( sector_t::ceiling, false ));
	command.SetFloorXOffset( sectors[ulSector].GetXOffset( sector_t::floor ));
	command.SetFloorYOffset( sectors[ulSector].GetYOffset( sector_t::floor, false ));
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorRotation( ULONG ulSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::SetSectorRotation command;
	command.SetSector( &sectors[ulSector] );
	command.SetCeilingRotation( sectors[ulSector].GetAngle( sector_t::ceiling, false ) / ANGLE_1 );
	command.SetFloorRotation( sectors[ulSector].GetAngle( sector_t::floor, false ) / ANGLE_1 );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorRotationByTag( ULONG ulTag, LONG lFloorRot, LONG lCeilingRot, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetSectorRotationByTag command;
	command.SetTag( ulTag );
	command.SetCeilingRotation( lCeilingRot );
	command.SetFloorRotation( lFloorRot );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorScale( ULONG ulSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::SetSectorScale command;
	command.SetSector( &sectors[ulSector] );
	command.SetCeilingXScale( sectors[ulSector].GetXScale( sector_t::ceiling ) );
	command.SetCeilingYScale( sectors[ulSector].GetYScale( sector_t::ceiling ) );
	command.SetFloorXScale( sectors[ulSector].GetXScale( sector_t::floor ) );
	command.SetFloorYScale( sectors[ulSector].GetYScale( sector_t::floor ) );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorSpecial( ULONG ulSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::SetSectorSpecial command;
	command.SetSector( &sectors[ulSector] );
	command.SetSpecial( sectors[ulSector].special );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorFriction( ULONG ulSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::SetSectorFriction command;
	command.SetSector( &sectors[ulSector] );
	command.SetFriction( sectors[ulSector].friction );
	command.SetMoveFactor( sectors[ulSector].movefactor );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorAngleYOffset( ULONG ulSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::SetSectorAngleYOffset command;
	command.SetSector( &sectors[ulSector] );
	command.SetCeilingBaseAngle( sectors[ulSector].planes[sector_t::ceiling].xform.base_angle );
	command.SetCeilingBaseYOffset( sectors[ulSector].planes[sector_t::ceiling].xform.base_yoffs );
	command.SetFloorBaseAngle( sectors[ulSector].planes[sector_t::floor].xform.base_angle );
	command.SetFloorBaseYOffset( sectors[ulSector].planes[sector_t::floor].xform.base_yoffs );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorGravity( ULONG ulSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::SetSectorGravity command;
	command.SetSector( &sectors[ulSector] );
	command.SetGravity( sectors[ulSector].gravity );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorReflection( ULONG ulSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::SetSectorReflection command;
	command.SetSector( &sectors[ulSector] );
	command.SetCeilingReflection( sectors[ulSector].reflect[sector_t::ceiling] );
	command.SetFloorReflection( sectors[ulSector].reflect[sector_t::floor] );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_StopSectorLightEffect( ULONG ulSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::StopSectorLightEffect command;
	command.SetSector( &sectors[ulSector] );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DestroyAllSectorMovers( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::DestroyAllSectorMovers().sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_DoSectorLightFireFlicker( ULONG ulSector, LONG lMaxLight, LONG lMinLight, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::DoSectorLightFireFlicker command;
	command.SetSector( &sectors[ulSector] );
	command.SetMaxLight( lMaxLight );
	command.SetMinLight( lMinLight );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoSectorLightFlicker( ULONG ulSector, LONG lMaxLight, LONG lMinLight, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::DoSectorLightFireFlicker command;
	command.SetSector( &sectors[ulSector] );
	command.SetMaxLight( lMaxLight );
	command.SetMinLight( lMinLight );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoSectorLightLightFlash( ULONG ulSector, LONG lMaxLight, LONG lMinLight, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::DoSectorLightLightFlash command;
	command.SetSector( &sectors[ulSector] );
	command.SetMaxLight( lMaxLight );
	command.SetMinLight( lMinLight );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoSectorLightStrobe( ULONG ulSector, LONG lDarkTime, LONG lBrightTime, LONG lMaxLight, LONG lMinLight, LONG lCount, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::DoSectorLightStrobe command;
	command.SetSector( &sectors[ulSector] );
	command.SetDarkTime( lDarkTime );
	command.SetBrightTime( lBrightTime );
	command.SetMaxLight( lMaxLight );
	command.SetMinLight( lMinLight );
	command.SetCount( lCount );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoSectorLightGlow( ULONG ulSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::DoSectorLightGlow command;
	command.SetSector( &sectors[ulSector] );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoSectorLightGlow2( ULONG ulSector, LONG lStart, LONG lEnd, LONG lTics, LONG lMaxTics, bool bOneShot, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::DoSectorLightGlow2 command;
	command.SetSector( &sectors[ulSector] );
	command.SetStartLight( lStart );
	command.SetEndLight( lEnd );
	command.SetTics( lTics );
	command.SetMaxTics( lMaxTics );
	command.SetOneShot( bOneShot );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoSectorLightPhased( ULONG ulSector, LONG lBaseLevel, LONG lPhase, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSector >= (ULONG)numsectors )
		return;

	ServerCommands::DoSectorLightPhased command;
	command.SetSector( &sectors[ulSector] );
	command.SetBaseLevel( lBaseLevel );
	command.SetPhase( lPhase );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_SetLineAlpha( ULONG ulLine, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulLine >= (ULONG)numlines )
		return;

	ServerCommands::SetLineAlpha command;
	command.SetLine( &lines[ulLine] );
	command.SetAlpha( lines[ulLine].Alpha );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetLineTexture( ULONG ulLine, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulLine >= (ULONG)numlines )
		return;

	// No changed textures to update.
	if ( lines[ulLine].ulTexChangeFlags == 0 )
		return;

	ServerCommands::SetLineTexture command;
	command.SetLine( &lines[ulLine] );

	if ( lines[ulLine].ulTexChangeFlags & TEXCHANGE_FRONTTOP )
	{
		command.SetTextureName( lines[ulLine].sidedef[0]->GetTexture(side_t::top).isValid() ? TexMan[lines[ulLine].sidedef[0]->GetTexture(side_t::top)]->Name : "-" );
		command.SetSide( 0 );
		command.SetPosition( 0 );
		command.sendCommandToClients( ulPlayerExtra, flags );
	}

	if ( lines[ulLine].ulTexChangeFlags & TEXCHANGE_FRONTMEDIUM )
	{
		command.SetTextureName( lines[ulLine].sidedef[0]->GetTexture(side_t::mid).isValid() ? TexMan[lines[ulLine].sidedef[0]->GetTexture(side_t::mid)]->Name : "-" );
		command.SetSide( 0 );
		command.SetPosition( 1 );
		command.sendCommandToClients( ulPlayerExtra, flags );
	}

	if ( lines[ulLine].ulTexChangeFlags & TEXCHANGE_FRONTBOTTOM )
	{
		command.SetTextureName( lines[ulLine].sidedef[0]->GetTexture(side_t::bottom).isValid() ? TexMan[lines[ulLine].sidedef[0]->GetTexture(side_t::bottom)]->Name : "-" );
		command.SetSide( 0 );
		command.SetPosition( 2 );
		command.sendCommandToClients( ulPlayerExtra, flags );
	}

	if ( lines[ulLine].sidedef[1] != NULL )
	{
		if ( lines[ulLine].ulTexChangeFlags & TEXCHANGE_BACKTOP )
		{
			command.SetTextureName( lines[ulLine].sidedef[1]->GetTexture(side_t::top).isValid() ? TexMan[lines[ulLine].sidedef[1]->GetTexture(side_t::top)]->Name : "-" );
			command.SetSide( 1 );
			command.SetPosition( 0 );
			command.sendCommandToClients( ulPlayerExtra, flags );
		}

		if ( lines[ulLine].ulTexChangeFlags & TEXCHANGE_BACKMEDIUM )
		{
			command.SetTextureName( lines[ulLine].sidedef[1]->GetTexture(side_t::mid).isValid() ? TexMan[lines[ulLine].sidedef[1]->GetTexture(side_t::mid)]->Name : "-" );
			command.SetSide( 1 );
			command.SetPosition( 1 );
			command.sendCommandToClients( ulPlayerExtra, flags );
		}

		if ( lines[ulLine].ulTexChangeFlags & TEXCHANGE_BACKBOTTOM )
		{
			command.SetTextureName( lines[ulLine].sidedef[1]->GetTexture(side_t::bottom).isValid() ? TexMan[lines[ulLine].sidedef[1]->GetTexture(side_t::bottom)]->Name : "-" );
			command.SetSide( 1 );
			command.SetPosition( 2 );
			command.sendCommandToClients( ulPlayerExtra, flags );
		}
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetLineTextureByID( ULONG ulLineID, ULONG ulSide, ULONG ulPosition, const char *pszTexName, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSide > 1 )
		SERVER_PrintWarning( "SERVERCOMMANDS_SetLineTextureByID: invalid side: %lu!\n", ulSide );

	ServerCommands::SetLineTextureByID command;
	command.SetLineID( ulLineID );
	command.SetTextureName( pszTexName );
	command.SetSide( !!ulSide );
	command.SetPosition( ulPosition );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSomeLineFlags( ULONG ulLine, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulLine >= (ULONG)numlines )
		return;

	ServerCommands::SetSomeLineFlags command;
	command.SetLine( &lines[ulLine] );
	command.SetBlockFlags( lines[ulLine].flags & ( ML_BLOCKING | ML_BLOCKEVERYTHING | ML_RAILING | ML_BLOCK_PLAYERS | ML_ADDTRANS ));
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_ACSScriptExecute( int ScriptNum, AActor *pActivator, LONG lLineIdx, int levelnum, bool bBackSide, const int *args, int argcount, bool bAlways, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ACS_ExistsScript( ScriptNum ) == false )
		return;

	if (( argcount < 3 ) || ( argcount > 4 ))
	{
		SERVER_PrintWarning( "SERVERCOMMANDS_ACSScriptExecute: invalid argcount: %d!\n", argcount );
		return;
	}

	int netid = NETWORK_ACSScriptToNetID( ScriptNum );

	if ( netid == NO_SCRIPT_NETID )
	{
		// [TP] This shouldn't happen
		if ( sv_showwarnings )
		{
			Printf( "SERVERCOMMANDS_ACSScriptExecute: Failed to find a netid for script %s!\n",
				FName( ENamedName( -ScriptNum )).GetChars() );
		}
		return;
	}

	int arg3 = ( argcount == 4 ) ? args[3] : 0;

	ServerCommands::ACSScriptExecute command;
	command.SetNetid( netid );
	command.SetActivator( pActivator );
	command.SetLineid( lLineIdx );
	command.SetLevelnum( levelnum );
	command.SetArg0( args[0] );
	command.SetArg1( args[1] );
	command.SetArg2( args[2] );
	command.SetArg3( arg3 );
	command.SetAlways( bAlways );
	command.SetBackSide( bBackSide );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_ACSSendString( int ScriptNum, AActor *pActivator, const char *pszString, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	int netid = NETWORK_ACSScriptToNetID( ScriptNum );

	if ( netid == NO_SCRIPT_NETID )
	{
		// [AK] This shouldn't happen.
		if ( sv_showwarnings )
		{
			Printf( "SERVERCOMMANDS_ACSSendString: Failed to find a netid for script %s!\n",
				FName( ENamedName( -ScriptNum )).GetChars() );
		}
		return;
	}

	ServerCommands::ACSSendString command;
	command.SetNetid( netid );
	command.SetActivator( pActivator );
	command.SetString( pszString );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_SetSideFlags( ULONG ulSide, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ulSide >= (ULONG)numsides )
		return;

	ServerCommands::SetSideFlags command;
	command.SetSide( &sides[ulSide] );
	command.SetFlags( sides[ulSide].Flags );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_Sound( LONG lChannel, const char *pszSound, float fVolume, float fAttenuation, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::Sound command;
	command.SetChannel( lChannel );
	command.SetSound( pszSound );
	command.SetVolume( LONG ( clamp( fVolume, 0.0f, 2.0f ) * 127 ) );
	command.SetAttenuation( NETWORK_AttenuationFloatToInt ( fAttenuation ) );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SoundActor( AActor *pActor, LONG lChannel, const char *pszSound, float fVolume, float fAttenuation, ULONG ulPlayerExtra, ServerCommandFlags flags, bool bRespectActorPlayingSomething )
{
	if ( pActor == NULL )
		return;

	// [BB] If the actor doesn't have a NetID, we have to instruct the clients differently how to play the sound.
	if ( pActor->lNetID == -1 )
	{
		SERVERCOMMANDS_SoundPoint( pActor->x, pActor->y, pActor->z, lChannel, pszSound, fVolume, fAttenuation, ulPlayerExtra, flags );
		return;
	}

	ServerCommands::SoundActor normalCommand;
	ServerCommands::SoundActorIfNotPlaying commandIfNotPlaying;
	ServerCommands::SoundActor &command = bRespectActorPlayingSomething ? commandIfNotPlaying : normalCommand;
	command.SetActor( pActor );
	command.SetChannel( lChannel );
	command.SetSound( pszSound );
	command.SetVolume( LONG ( clamp( fVolume, 0.0f, 2.0f ) * 127 ) );
	command.SetAttenuation( NETWORK_AttenuationFloatToInt ( fAttenuation ));
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_StopSoundActor( AActor *pActor, LONG lChannel, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( pActor == NULL )
		return;

	ServerCommands::StopSoundActor command;
	command.SetActor( pActor );
	command.SetChannel( lChannel );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SoundSector( sector_t *sector, int channel, const char *sound, float volume, float attenuation, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( sector == NULL )
		return;

	ServerCommands::SoundSector command;
	command.SetSector( sector );
	command.SetChannel( channel );
	command.SetSound( sound );
	command.SetVolume( LONG ( clamp( volume, 0.0f, 2.0f ) * 127 ) );
	command.SetAttenuation( NETWORK_AttenuationFloatToInt ( attenuation ));
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SoundPoint( LONG lX, LONG lY, LONG lZ, LONG lChannel, const char *pszSound, float fVolume, float fAttenuation, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SoundPoint command;
	command.SetX( lX );
	command.SetY( lY );
	command.SetZ( lZ );
	command.SetChannel( lChannel );
	command.SetSound( pszSound );
	command.SetVolume( LONG ( clamp( fVolume, 0.0f, 2.0f ) * 127 ) );
	command.SetAttenuation( NETWORK_AttenuationFloatToInt ( fAttenuation ) );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_AnnouncerSound( const char *pszSound, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::AnnouncerSound command;
	command.SetSound( pszSound );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_StartSectorSequence( sector_t *pSector, const int Channel, const char *pszSequence, const int Modenum, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( pSector == NULL )
		return;

	ServerCommands::StartSectorSequence command;
	command.SetSector( pSector );
	command.SetChannel( Channel );
	command.SetSequence( pszSequence );
	command.SetModeNum( Modenum );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_StopSectorSequence( sector_t *pSector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( pSector == NULL )
		return;

	ServerCommands::StopSectorSequence command;
	command.SetSector( pSector );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_CallVote( ULONG ulPlayer, FString Command, FString Parameters, FString Reason, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::CallVote command;
	command.SetPlayer ( &players[ulPlayer] );
	command.SetCommand ( Command );
	command.SetParameters ( Parameters );
	command.SetReason ( Reason );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_PlayerVote( ULONG ulPlayer, bool bVoteYes, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::PlayerVote command;
	command.SetPlayer ( &players[ulPlayer] );
	command.SetVoteYes ( bVoteYes );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_VoteEnded( bool bVotePassed, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::VoteEnded command;
	command.SetVotePassed ( bVotePassed );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_MapLoad( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::MapLoad command;
	command.SetMapName( level.mapname );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_MapNew( const char *pszMapName, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::MapNew command;
	command.SetMapName( pszMapName );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_MapExit( LONG lPosition, const char *pszNextMap, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( pszNextMap == NULL )
		return;

	ServerCommands::MapExit command;
	command.SetPosition( lPosition );
	command.SetNextMap( pszNextMap );
	command.sendCommandToClients ( ulPlayerExtra, flags );

	// [BB] The clients who are authenticated, but still didn't finish loading
	// the map are not covered by the code above and need special treatment.
	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_GetClient( ulIdx )->State == CLS_AUTHENTICATED )
			SERVER_GetClient( ulIdx )->State = CLS_AUTHENTICATED_BUT_OUTDATED_MAP;
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_MapAuthenticate( const char *pszMapName, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::MapAuthenticate command;
	command.SetMapName( pszMapName );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SecretFound( AActor *actor, BYTE secretFlags, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SecretFound command;
	command.SetActor( actor );
	command.SetSecretFlags( (BYTE)secretFlags );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SecretMarkSectorFound( sector_t *sector, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SecretMarkSectorFound command;
	command.SetSector( sector );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetMapTime( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetMapTime command;
	command.SetTime( level.time );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetMapNumKilledMonsters( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetMapNumKilledMonsters command;
	command.SetKilledMonsters( level.killed_monsters );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetMapNumFoundItems( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetMapNumFoundItems command;
	command.SetFoundItems( level.found_items );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetMapNumFoundSecrets( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetMapNumFoundSecrets command;
	command.SetFoundSecrets( level.found_secrets );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetMapNumTotalMonsters( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetMapNumTotalMonsters command;
	command.SetTotalMonsters( level.total_monsters );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetMapNumTotalItems( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetMapNumTotalItems command;
	command.SetTotalItems( level.total_items );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetMapNumTotalSecrets( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetMapNumTotalSecrets command;
	command.SetTotalSecrets( level.total_secrets );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetMapMusic( const char *pszMusic, int track, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetMapMusic command;
	command.SetMusic( pszMusic );
	command.SetOrder( track );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetMapSky( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetMapSky command;
	command.SetSky1( level.skypic1 );
	command.SetSky2( level.skypic2 );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_GiveInventory( ULONG ulPlayer, AInventory *pInventory, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	if ( pInventory == NULL )
		return;

	if ( pInventory->ulNetworkFlags & NETFL_SERVERSIDEONLY )
		return;

	ServerCommands::GiveInventory command;
	command.SetPlayer ( &players[ulPlayer] );
	command.SetActorNetworkIndex (  pInventory->GetClass()->getActorNetworkIndex() );
	command.SetAmount ( pInventory->Amount );
	command.sendCommandToClients ( ulPlayerExtra, flags );

	// [BB] Clients don't know that a BackpackItem may be depleted. In this case we have to resync the ammo count.
	if ( pInventory->IsKindOf (RUNTIME_CLASS(ABackpackItem)) && static_cast<ABackpackItem*> ( pInventory )->bDepleted )
		SERVERCOMMANDS_SyncPlayerAmmoAmount( ulPlayer, ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_GiveInventoryNotOverwritingAmount( AActor *pReceiver, AInventory *pItem )
{
	if( (pItem == NULL) || (pReceiver == NULL) || (pReceiver->player == NULL) )
		return;
	
	// [BB] Since SERVERCOMMANDS_GiveInventory overwrites the item amount
	// of the client with pItem->Amount, we have have to set this to the
	// correct amount the player has.
	AInventory *pInventory = NULL;
	if( pReceiver->player && pReceiver->player->mo )
		pInventory = pReceiver->player->mo->FindInventory( pItem->GetClass() );
	if ( pInventory != NULL )
		pItem->Amount = pInventory->Amount;

	SERVERCOMMANDS_GiveInventory( ULONG( pReceiver->player - players ), pItem );
	// [BB] The armor display amount has to be updated separately.
	if( pItem->GetClass()->IsDescendantOf (RUNTIME_CLASS(AArmor)))
		SERVERCOMMANDS_SetPlayerArmor( ULONG( pReceiver->player - players ));
}
//*****************************************************************************
//
void SERVERCOMMANDS_TakeInventory( ULONG ulPlayer, const PClass *inventoryClass, ULONG ulAmount, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	if ( inventoryClass == NULL || ( GetDefaultByType ( inventoryClass )->ulNetworkFlags & NETFL_SERVERSIDEONLY ) )
		return;

	ServerCommands::TakeInventory command;
	command.SetPlayer ( &players[ulPlayer] );
	command.SetActorNetworkIndex ( inventoryClass->getActorNetworkIndex() );
	command.SetAmount ( ulAmount );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_GivePowerup( ULONG ulPlayer, APowerup *pPowerup, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	if ( pPowerup == NULL )
		return;

	ServerCommands::GivePowerup command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetActorNetworkIndex( pPowerup->GetClass( )->getActorNetworkIndex() );
	// Can we have multiple amounts of a powerup? Probably not, but I'll be safe for now.
	command.SetAmount( pPowerup->Amount );
	command.SetIsActiveRune( pPowerup->IsActiveRune() );

	if ( pPowerup->IsActiveRune() == false )
		command.SetEffectTicks( pPowerup->EffectTics );

	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
// [WS]
void SERVERCOMMANDS_SetPowerupBlendColor( ULONG ulPlayer, APowerup *pPowerup, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	if ( pPowerup == NULL )
		return;

	ServerCommands::SetPowerupBlendColor command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetActorNetworkIndex( pPowerup->GetClass( )->getActorNetworkIndex() );
	command.SetBlendColor( pPowerup->BlendColor );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
// [Dusk]
void SERVERCOMMANDS_GiveWeaponHolder( ULONG ulPlayer, AWeaponHolder *pHolder, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	if ( pHolder == NULL )
		return;

	if ( pHolder->ulNetworkFlags & NETFL_SERVERSIDEONLY )
		return;

	ServerCommands::GiveWeaponHolder command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetPieceMask( pHolder->PieceMask );
	command.SetPieceWeapon( pHolder->PieceWeapon );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoInventoryPickup( ULONG ulPlayer, const char *pszClassName, const char *pszPickupMessage, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::DoInventoryPickup command;
	command.SetPlayer ( &players[ulPlayer] );
	command.SetClassName ( pszClassName );
	command.SetPickupMessage ( pszPickupMessage );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DestroyAllInventory( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::DestroyAllInventory command;
	command.SetPlayer ( &players[ulPlayer] );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetInventoryIcon( ULONG ulPlayer, AInventory *pInventory, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( ( pInventory == NULL ) || ( pInventory->Icon.isValid() == false ) )
		return;

	FString iconTexName = TexMan( pInventory->Icon )->Name;

	ServerCommands::SetInventoryIcon command;
	command.SetPlayer ( &players[ulPlayer] );
	command.SetActorNetworkIndex ( pInventory->GetClass()->getActorNetworkIndex() );
	command.SetIconTexName ( iconTexName.GetChars() );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
// [Dusk]
void SERVERCOMMANDS_SetHexenArmorSlots( ULONG ulPlayer, AHexenArmor *hexenArmor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	if ( hexenArmor == NULL )
		return;

	ServerCommands::SetHexenArmorSlots command;
	command.SetPlayer( &players[ulPlayer] );
	command.SetSlot0( hexenArmor->Slots[0] );
	command.SetSlot1( hexenArmor->Slots[1] );
	command.SetSlot2( hexenArmor->Slots[2] );
	command.SetSlot3( hexenArmor->Slots[3] );
	command.SetSlot4( hexenArmor->Slots[4] );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
void SERVERCOMMANDS_SyncHexenArmorSlots( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayerWithMo( ulPlayer ) == false )
		return;

	AHexenArmor *aHXArmor = static_cast<AHexenArmor *>( players[ulPlayer].mo->FindInventory( RUNTIME_CLASS( AHexenArmor )));
	SERVERCOMMANDS_SetHexenArmorSlots( ulPlayer, aHXArmor, ulPlayerExtra, flags );
}

//*****************************************************************************
// [Dusk]
void SERVERCOMMANDS_SetFastChaseStrafeCount( AActor *mobj, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (mobj) )
		return;

	ServerCommands::SetFastChaseStrafeCount command;
	command.SetActor( mobj );
	command.SetStrafeCount( mobj->FastChaseStrafeCount );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
// [Dusk] This function is called to set an actor's health directly on the
// client. I don't expect many things to call it (it was created for the sake
// of syncing hellstaff rain health fields) so it's an extended command for now
// instead of a regular one, despite its genericness.
//
void SERVERCOMMANDS_SetThingHealth( AActor* mobj, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (mobj) )
		return;

	ServerCommands::SetThingHealth command;
	command.SetActor( mobj );
	command.SetHealth( mobj->health );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingScale( AActor* mobj, unsigned int scaleFlags, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (mobj) )
		return;

	if ( scaleFlags == 0 )
		return;

	ServerCommands::SetThingScale command;
	command.SetActor( mobj );
	command.SetScaleflags( scaleFlags );
	command.SetScaleX( mobj->scaleX );
	command.SetScaleY( mobj->scaleY );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_UpdateThingScaleNotAtDefault( AActor* pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	// [BB] Sanity check.
	if ( pActor == NULL )
		return;

	unsigned int scaleFlags = 0;
	if ( pActor->scaleX != pActor->GetDefault()->scaleX )
		scaleFlags |= ACTORSCALE_X;
	if ( pActor->scaleY != pActor->GetDefault()->scaleY )
		scaleFlags |= ACTORSCALE_Y;

	if ( scaleFlags != 0 )
		SERVERCOMMANDS_SetThingScale( pActor, scaleFlags, ulPlayerExtra, flags  );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingFillColor( AActor* pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	// [BB] Sanity check.
	if ( pActor == NULL )
		return;

	ServerCommands::SetThingFillColor command;
	command.SetActor( pActor );
	command.SetFillcolor( pActor->fillcolor );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetThingSprite( AActor* pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	// [BB] Sanity check.
	if ( pActor == NULL )
		return;

	ServerCommands::SetThingSprite command;
	command.SetActor( pActor );
	command.SetSprite( pActor->sprite );
	command.SetFrame( pActor->frame );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_FlashStealthMonster( AActor* pActor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( EnsureActorHasNetID( pActor ) == false )
		return;

	ServerCommands::FlashStealthMonster command;
	command.SetActor( pActor );
	command.sendCommandToClients();
}

//*****************************************************************************
//
void SERVERCOMMANDS_FullUpdateCompleted( ULONG ulClient )
{
	ServerCommands::FullUpdateCompleted().sendCommandToClients( ulClient, SVCF_ONLYTHISCLIENT );
}

//*****************************************************************************
//
void SERVERCOMMANDS_ResetMap( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::ResetMap command;
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetIgnoreWeaponSelect( ULONG ulClient, const bool bIgnoreWeaponSelect )
{
	ServerCommands::SetIgnoreWeaponSelect command;
	command.SetIgnore ( bIgnoreWeaponSelect );
	command.sendCommandToClients( ulClient, SVCF_ONLYTHISCLIENT );
}

//*****************************************************************************
//
void SERVERCOMMANDS_ClearConsolePlayerWeapon( ULONG ulClient )
{
	ServerCommands::ClearConsolePlayerWeapon command;
	command.sendCommandToClients( ulClient, SVCF_ONLYTHISCLIENT );
}

//*****************************************************************************
//
void SERVERCOMMANDS_Lightning( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::Lightning command;
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_CancelFade( const ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::CancelFade command;
	if (ulPlayer == MAXPLAYERS)
		command.SetPlayer ( NULL );
	else
		command.SetPlayer ( &players[ulPlayer] );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_PlayBounceSound( AActor *pActor, bool bOnfloor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pActor) )
		return;

	ServerCommands::PlayBounceSound command;
	command.SetActor ( pActor );
	command.SetOnFloor ( bOnfloor );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_DoDoor( DDoor *Door, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	LONG	lSectorID;

	lSectorID = LONG( Door->GetSector() - sectors );
	if (( lSectorID < 0 ) || ( lSectorID >= numsectors ))
		return;

	if (Door->ObjectFlags & OF_EuthanizeMe)
	{
		ServerCommands::DestroyDoor command;
		command.SetSectorID ( lSectorID );
		command.SetPosition ( Door->GetPosition() );
		command.sendCommandToClients ( ulPlayerExtra, flags );
	}
	else
	{
		ServerCommands::DoDoor command;
		command.SetSectorID ( lSectorID );
		command.SetPosition ( Door->GetPosition() );
		command.SetInstigator ( (BYTE) ( Door->GetLastInstigator() - players ) );
		command.SetType ( (BYTE)Door->GetType() );
		command.SetDirection ( SERVER_AdjustDoorDirection( Door->GetDirection() ) );
		command.SetSpeed ( Door->GetSpeed() );
		command.SetTopWait ( Door->GetTopWait() );
		command.SetCountdown ( Door->GetCountdown() );
		command.SetLightTag ( Door->GetLightTag() );
		command.sendCommandToClients ( ulPlayerExtra, flags );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_DoFloor( DFloor *Floor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	LONG lSectorID = LONG( Floor->GetSector() - sectors );
	if (( lSectorID < 0 ) || ( lSectorID >= numsectors ))
		return;

	if (Floor->ObjectFlags & OF_EuthanizeMe)
	{
		ServerCommands::DestroyFloor command;
		command.SetSectorID ( lSectorID );
		command.SetPosition ( Floor->GetPosition() );
		command.sendCommandToClients ( ulPlayerExtra, flags );
	}
	else
	{
		ServerCommands::DoFloor command;
		command.SetSectorID ( lSectorID );
		command.SetPosition ( Floor->GetPosition() );
		command.SetInstigator ( Floor->GetLastInstigator() - players );
		command.SetType ( (ULONG) Floor->GetType() );
		command.SetDirection ( SERVER_AdjustFloorDirection( Floor->GetDirection() ) );
		command.SetSpeed ( Floor->GetSpeed() );
		command.SetOrgDist ( Floor->GetOrgDist() );
		command.SetDestDist ( Floor->GetFloorDestDist() );
		command.SetCrush ( clamp<LONG>(Floor->GetCrush(),-128,127) );
		command.SetHexenCrush ( Floor->GetHexencrush() );
		command.SetNewSpecial ( Floor->GetNewSpecial() );
		command.sendCommandToClients ( ulPlayerExtra, flags );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_BuildStair( DFloor *Floor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	int SectorID = int( Floor->GetSector() - sectors );
	if (( SectorID < 0 ) || ( SectorID >= numsectors ))
		return;

	if (Floor->ObjectFlags & OF_EuthanizeMe)
	{
		ServerCommands::DestroyStair command;
		command.SetSectorID ( SectorID );
		command.SetPosition ( Floor->GetPosition() );
		command.sendCommandToClients ( ulPlayerExtra, flags );
	}
	else
	{
		ServerCommands::BuildStair command;
		command.SetSectorID ( SectorID );
		command.SetPosition ( Floor->GetPosition() );
		command.SetInstigator ( Floor->GetLastInstigator() - players );
		command.SetType ( (ULONG) Floor->GetType() );
		command.SetDirection ( SERVER_AdjustFloorDirection( Floor->GetDirection() ) );
		command.SetSpeed ( Floor->GetSpeed() );
		command.SetOrgDist ( Floor->GetOrgDist() );
		command.SetDestDist ( Floor->GetFloorDestDist() );
		command.SetCrush ( clamp<LONG>(Floor->GetCrush(),-128,127) );
		command.SetHexenCrush ( Floor->GetHexencrush() );
		command.SetNewSpecial ( Floor->GetNewSpecial() );
		command.SetResetCount ( Floor->GetResetCount() );
		command.SetDelay ( Floor->GetDelay() );
		command.SetPauseTime ( Floor->GetPauseTime() );
		command.SetStepTime ( Floor->GetStepTime() );
		command.SetPerStepTime ( Floor->GetPerStepTime() );
		command.sendCommandToClients ( ulPlayerExtra, flags );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_DoCeiling( DCeiling *Ceiling, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	LONG lSectorID = LONG( Ceiling->GetSector() - sectors );
	if (( lSectorID < 0 ) || ( lSectorID >= numsectors ))
		return;

	if (Ceiling->ObjectFlags & OF_EuthanizeMe)
	{
		ServerCommands::DestroyCeiling command;
		command.SetSectorID ( lSectorID );
		command.SetPosition ( Ceiling->GetPosition() );
		command.sendCommandToClients ( ulPlayerExtra, flags );
	}
	else
	{
		ServerCommands::DoCeiling command;
		command.SetSectorID ( lSectorID );
		command.SetPosition ( Ceiling->GetPosition() );
		command.SetInstigator ( Ceiling->GetLastInstigator() - players );
		command.SetTag ( Ceiling->GetTag() );
		command.SetType ( (ULONG)Ceiling->GetType() );
		command.SetDirection ( SERVER_AdjustCeilingDirection( Ceiling->GetDirection() ) );
		command.SetOldDirection ( SERVER_AdjustCeilingDirection( Ceiling->GetOldDirection() ) );
		command.SetBottomHeight ( Ceiling->GetBottomHeight() );
		command.SetTopHeight ( Ceiling->GetTopHeight() );
		command.SetSpeed ( Ceiling->GetSpeed() );
		command.SetSpeed1 ( Ceiling->GetSpeed1() );
		command.SetSpeed2 ( Ceiling->GetSpeed2() );
		command.SetCrush ( clamp<LONG>(Ceiling->GetCrush(),-128,127) );
		command.SetHexenCrush ( Ceiling->GetHexencrush() );
		command.SetSilent ( Ceiling->GetSilent() );
		command.sendCommandToClients ( ulPlayerExtra, flags );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_DoPlat( DPlat *Plat, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	LONG lSectorID = LONG( Plat->GetSector() - sectors );
	if (( lSectorID < 0 ) || ( lSectorID >= numsectors ))
		return;

	if (Plat->ObjectFlags & OF_EuthanizeMe)
	{
		ServerCommands::DestroyPlat command;
		command.SetSectorID ( lSectorID );
		command.SetPosition ( Plat->GetPosition() );
		command.sendCommandToClients ( ulPlayerExtra, flags );
	}
	else
	{
		ServerCommands::DoPlat command;
		command.SetSectorID ( lSectorID );
		command.SetPosition ( Plat->GetPosition() );
		command.SetInstigator ( Plat->GetLastInstigator() - players );
		command.SetTag ( Plat->GetTag() );
		command.SetType ( (ULONG)Plat->GetType() );
		command.SetStatus ( (ULONG)Plat->GetStatus() );
		command.SetOldStatus ( (ULONG)Plat->GetOldStatus() );
		command.SetSpeed ( Plat->GetSpeed() );
		command.SetHigh ( Plat->GetHigh() );
		command.SetLow ( Plat->GetLow() );
		command.SetWait ( Plat->GetWait() );
		command.SetCount ( Plat->GetCount() );
		command.SetCrush ( Plat->GetCrush() );
		command.sendCommandToClients ( ulPlayerExtra, flags );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_DoElevator( DElevator *Elevator, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	LONG	lSectorID = LONG( Elevator->GetSector() - sectors );
	if (( lSectorID < 0 ) || ( lSectorID >= numsectors ))
		return;

	if (Elevator->ObjectFlags & OF_EuthanizeMe)
	{
		ServerCommands::DestroyElevator command;
		command.SetSectorID ( lSectorID );
		command.SetFloorPosition ( Elevator->GetFloorPosition() );
		command.SetCeilingPosition ( Elevator->GetCeilingPosition() );
		command.sendCommandToClients ( ulPlayerExtra, flags );
	}
	else
	{
		ServerCommands::DoElevator command;
		command.SetSectorID ( lSectorID );
		command.SetFloorPosition ( Elevator->GetFloorPosition() );
		command.SetCeilingPosition ( Elevator->GetCeilingPosition() );
		command.SetInstigator ( Elevator->GetLastInstigator() - players );
		command.SetType ( Elevator->GetType() );
		command.SetSpeed ( Elevator->GetSpeed() );
		command.SetDirection ( SERVER_AdjustElevatorDirection( Elevator->GetDirection() ) );
		command.SetFloorDestDist ( Elevator->GetFloorDestDist() );
		command.SetCeilingDestDist ( Elevator->GetCeilingDestDist() );
		command.sendCommandToClients ( ulPlayerExtra, flags );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_DoPillar( DPillar *Pillar, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	LONG	lSectorID = LONG( Pillar->GetSector() - sectors );
	if (( lSectorID < 0 ) || ( lSectorID >= numsectors ))
		return;

	if (Pillar->ObjectFlags & OF_EuthanizeMe)
	{
		ServerCommands::DestroyPillar command;
		command.SetSectorID ( lSectorID );
		command.SetFloorPosition ( Pillar->GetFloorPosition() );
		command.SetCeilingPosition ( Pillar->GetCeilingPosition() );
		command.sendCommandToClients ( ulPlayerExtra, flags );
	}
	else
	{
		ServerCommands::DoPillar command;
		command.SetSectorID ( lSectorID );
		command.SetFloorPosition ( Pillar->GetFloorPosition() );
		command.SetCeilingPosition ( Pillar->GetCeilingPosition() );
		command.SetInstigator ( Pillar->GetLastInstigator() - players );
		command.SetType ( Pillar->GetType() );
		command.SetFloorSpeed ( Pillar->GetFloorSpeed() );
		command.SetCeilingSpeed ( Pillar->GetCeilingSpeed() );
		command.SetFloorTarget ( Pillar->GetFloorTarget() );
		command.SetCeilingTarget ( Pillar->GetCeilingTarget() );
		command.SetCrush ( Pillar->GetCrush() );
		command.SetHexenCrush ( Pillar->GetHexencrush() );
		command.sendCommandToClients ( ulPlayerExtra, flags );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_DoWaggle( DWaggleBase *Waggle, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	LONG	lSectorID = LONG( Waggle->GetSector() - sectors );
	if (( lSectorID < 0 ) || ( lSectorID >= numsectors ))
		return;

	if (Waggle->ObjectFlags & OF_EuthanizeMe)
	{
		ServerCommands::DestroyWaggle command;
		command.SetSectorID ( lSectorID );
		command.SetPosition ( Waggle->GetPosition() );
		command.SetIsCeilingWaggle ( Waggle->GetClass( ) == RUNTIME_CLASS( DCeilingWaggle ) );
		command.sendCommandToClients ( ulPlayerExtra, flags );
	}
	else
	{
		ServerCommands::DoWaggle command;
		command.SetSectorID ( lSectorID );
		command.SetPosition ( Waggle->GetPosition() );
		command.SetIsCeilingWaggle ( Waggle->GetClass( ) == RUNTIME_CLASS( DCeilingWaggle ) );
		command.SetInstigator ( Waggle->GetLastInstigator() - players );
		command.SetOriginalDistance ( Waggle->GetOriginalDistance() );
		command.SetAccumulator ( Waggle->GetAccumulator() );
		command.SetAccelerationDelta ( Waggle->GetAccelerationDelta() );
		command.SetTargetScale ( Waggle->GetTargetScale() );
		command.SetScale ( Waggle->GetScale() );
		command.SetScaleDelta ( Waggle->GetScaleDelta() );
		command.SetTicker ( Waggle->GetTicker() );
		command.SetState ( Waggle->GetState() );
		command.sendCommandToClients ( ulPlayerExtra, flags );
	}
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_DoRotatePoly( DRotatePoly *RotatePoly, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::DoRotatePoly command;
	command.SetPolyObj ( RotatePoly->GetPolyObj() );
	command.SetInstigator ( RotatePoly->GetLastInstigator() - players );
	command.SetSpeed ( RotatePoly->GetSpeed() );
	command.SetDist ( RotatePoly->GetDist() );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_DoMovePoly( DMovePoly *MovePoly, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::DoMovePoly command;
	command.SetPolyObj ( MovePoly->GetPolyObj() );
	command.SetInstigator ( MovePoly->GetLastInstigator() - players );
	command.SetXSpeed ( MovePoly->GetXSpeed() );
	command.SetYSpeed ( MovePoly->GetYSpeed() );
	command.SetSpeed ( MovePoly->GetSpeed() );
	command.SetAngle ( MovePoly->GetAngle() );
	command.SetDist ( MovePoly->GetDist() );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_DoMovePolyTo( DMovePolyTo *MovePolyTo, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::DoMovePolyTo command;
	command.SetPolyObj ( MovePolyTo->GetPolyObj() );
	command.SetInstigator ( MovePolyTo->GetLastInstigator() - players );
	command.SetXTarget ( MovePolyTo->GetXTarget() );
	command.SetYTarget ( MovePolyTo->GetYTarget() );
	command.SetXSpeed ( MovePolyTo->GetXSpeed() );
	command.SetYSpeed ( MovePolyTo->GetYSpeed() );
	command.SetSpeed ( MovePolyTo->GetSpeed() );
	command.SetDist ( MovePolyTo->GetDist() );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_DoPolyDoor( DPolyDoor *PolyDoor, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::DoPolyDoor command;
	command.SetPolyObj ( PolyDoor->GetPolyObj() );
	command.SetType ( PolyDoor->GetType() );
	command.SetInstigator ( PolyDoor->GetLastInstigator() - players );
	command.SetX ( PolyDoor->GetX() );
	command.SetY ( PolyDoor->GetY() );
	command.SetXSpeed ( PolyDoor->GetXSpeed() );
	command.SetYSpeed ( PolyDoor->GetYSpeed() );
	command.SetSpeed ( PolyDoor->GetSpeed() );
	command.SetAngle ( PolyDoor->GetAngle() );
	command.SetDist ( PolyDoor->GetDist() );
	command.SetTotalDist ( PolyDoor->GetTotalDist() );
	command.SetClose ( PolyDoor->GetClose() );
	command.SetDirection ( PolyDoor->GetDirection() );
	command.SetTics ( PolyDoor->GetTics() );
	command.SetWaitTics ( PolyDoor->GetWaitTics() );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//*****************************************************************************
//
void SERVERCOMMANDS_Earthquake( AActor *pCenter, LONG lIntensity, LONG lDuration, LONG lTemorRadius, FSoundID Quakesound, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( !EnsureActorHasNetID (pCenter) )
		return;

	const char *pszQuakeSound = S_GetName( Quakesound );
	
	ServerCommands::EarthQuake command;
	command.SetCenter ( pCenter );
	command.SetIntensity ( lIntensity );
	command.SetDuration ( lDuration );
	command.SetTremorRadius ( lTemorRadius );
	command.SetQuakeSound ( pszQuakeSound );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SyncJoinQueue( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SyncJoinQueue command;
	TArray<ServerCommands::JoinSlot> joinSlots;
	joinSlots.Reserve( JOINQUEUE_GetSize() );

	for ( unsigned int i = 0; i < JOINQUEUE_GetSize(); ++i )
	{
		joinSlots[i].player = JOINQUEUE_GetSlotAt( i ).player;
		joinSlots[i].team = JOINQUEUE_GetSlotAt( i ).team;
	}

	command.SetSlots( joinSlots );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_PushToJoinQueue( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( JOINQUEUE_GetSize() > 0 )
	{
		const JoinSlot& slot = JOINQUEUE_GetSlotAt( JOINQUEUE_GetSize() - 1 );
		ServerCommands::PushToJoinQueue command;
		command.SetPlayerNum( slot.player );
		command.SetTeam( slot.team );
		command.sendCommandToClients( ulPlayerExtra, flags );
	}
}

//*****************************************************************************
//
void SERVERCOMMANDS_RemoveFromJoinQueue( unsigned int index, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::RemoveFromJoinQueue command;
	command.SetIndex( index );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoScroller( LONG lType, LONG lXSpeed, LONG lYSpeed, LONG lAffectee, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::DoScroller command;
	command.SetType ( lType );
	command.SetXSpeed ( lXSpeed );
	command.SetYSpeed ( lYSpeed );
	command.SetAffectee ( lAffectee );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetScroller( LONG lType, LONG lXSpeed, LONG lYSpeed, LONG lTag, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetScroller command;
	command.SetType ( lType );
	command.SetXSpeed ( lXSpeed );
	command.SetYSpeed ( lYSpeed );
	command.SetTag ( lTag );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetWallScroller( LONG lId, LONG lSidechoice, LONG lXSpeed, LONG lYSpeed, LONG lWhere, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetWallScroller command;
	command.SetId ( lId );
	command.SetSideChoice ( lSidechoice );
	command.SetXSpeed ( lXSpeed );
	command.SetYSpeed ( lYSpeed );
	command.SetLWhere ( lWhere );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoFlashFader( float fR1, float fG1, float fB1, float fA1, float fR2, float fG2, float fB2, float fA2, float fTime, ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::DoFlashFader command;
	command.SetR1 ( fR1 );
	command.SetG1 ( fG1 );
	command.SetB1 ( fB1 );
	command.SetA1 ( fA1 );
	command.SetR2 ( fR2 );
	command.SetG2 ( fG2 );
	command.SetB2 ( fB2 );
	command.SetA2 ( fA2 );
	command.SetTime ( fTime );
	command.SetPlayer ( &players[ulPlayer] );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_GenericCheat( ULONG ulPlayer, ULONG ulCheat, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::GenericCheat command;
	command.SetPlayer ( &players[ulPlayer] );
	command.SetCheat ( ulCheat );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetCameraToTexture( AActor *pCamera, char *pszTexture, LONG lFOV, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ((!EnsureActorHasNetID (pCamera) ) ||
		( pszTexture == NULL ))
	{
		return;
	}

	ServerCommands::SetCameraToTexture command;
	command.SetCamera ( pCamera );
	command.SetTexture ( pszTexture );
	command.SetFov ( lFOV );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_CreateTranslation( ULONG ulTranslation, ULONG ulStart, ULONG ulEnd, ULONG ulPal1, ULONG ulPal2, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	const bool bIsEdited = SERVER_IsTranslationEdited ( ulTranslation );

	ServerCommands::CreateTranslation command;
	command.SetTranslation ( ulTranslation );
	command.SetIsEdited ( bIsEdited );
	command.SetStart ( ulStart );
	command.SetEnd ( ulEnd );
	command.SetPal1 ( ulPal1 );
	command.SetPal2 ( ulPal2 );
	command.sendCommandToClients ( ulPlayerExtra, flags );

}
//*****************************************************************************
//
void SERVERCOMMANDS_CreateTranslation( ULONG ulTranslation, ULONG ulStart, ULONG ulEnd, ULONG ulR1, ULONG ulG1, ULONG ulB1, ULONG ulR2, ULONG ulG2, ULONG ulB2, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	const bool bIsEdited = SERVER_IsTranslationEdited ( ulTranslation );

	ServerCommands::CreateTranslation2 command;
	command.SetTranslation ( ulTranslation );
	command.SetIsEdited ( bIsEdited );
	command.SetStart ( ulStart );
	command.SetEnd ( ulEnd );
	command.SetR1 ( ulR1 );
	command.SetG1 ( ulG1 );
	command.SetB1 ( ulB1 );
	command.SetR2 ( ulR2 );
	command.SetG2 ( ulG2 );
	command.SetB2 ( ulB2 );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_ReplaceTextures( const char *Fromname, const char *Toname, int iTexFlags, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::ReplaceTextures command;
	command.SetFromTexture( Fromname );
	command.SetToTexture( Toname );
	command.SetTextureFlags( iTexFlags );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetSectorLink( ULONG ulSector, int iArg1, int iArg2, int iArg3, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetSectorLink command;
	command.SetSector( &sectors[ulSector] );
	command.SetTag( iArg1 );
	command.SetCeiling( iArg2 );
	command.SetMoveType( iArg3 );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_DoPusher( ULONG ulType, line_t *pLine, int iMagnitude, int iAngle, AActor *pSource, int iAffectee, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	const int iLineNum = pLine ? static_cast<ULONG>( pLine - lines ) : -1;

	ServerCommands::DoPusher command;
	command.SetType ( ulType );
	command.SetLineNum ( iLineNum );
	command.SetMagnitude ( iMagnitude );
	command.SetAngle ( iAngle );
	command.SetSourceActor ( pSource );
	command.SetAffectee ( iAffectee );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_AdjustPusher( int iTag, int iMagnitude, int iAngle, ULONG ulType, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::AdjustPusher command;
	command.SetTag ( iTag );
	command.SetMagnitude ( iMagnitude );
	command.SetAngle ( iAngle );
	command.SetType ( ulType );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}


//*****************************************************************************
// [Dusk]
void SERVERCOMMANDS_SetPlayerHazardCount ( ULONG ulPlayer, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::SetPlayerHazardCount command;
	command.SetPlayer ( &players[ulPlayer] );
	command.SetHazardCount ( players[ulPlayer].hazardcount );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
// [EP] All the clients must change their own mugshot state.
void SERVERCOMMANDS_SetMugShotState ( const char *statename )
{
	ServerCommands::SetMugShotState command;
	command.SetStateName( statename );
	command.sendCommandToClients();
}

//*****************************************************************************
// [Dusk] Used in map resets to move a 3d midtexture moves without sector it's attached to.
void SERVERCOMMANDS_Scroll3dMidtexture ( sector_t* sector, fixed_t move, bool ceiling, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::Scroll3dMidTex command;
	command.SetSectorNum ( sector - sectors );
	command.SetMove ( move );
	command.SetIsCeiling ( ceiling );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
void SERVERCOMMANDS_SetPlayerLogNumber ( const ULONG ulPlayer, const int Arg0, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	ServerCommands::SetPlayerLogNumber command;
	command.SetPlayer ( &players[ulPlayer] );
	command.SetArg0 ( Arg0 );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetCVar( const FBaseCVar &CVar, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetCVar command;
	command.SetName( CVar.GetName() );
	command.SetValue( CVar.GetGenericRep (CVAR_String).String );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SetDefaultSkybox( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	ServerCommands::SetDefaultSkybox command;
	command.SetSkyboxActor( level.DefaultSkybox );
	command.sendCommandToClients( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SRPUserStartAuthentication ( const ULONG ulClient )
{
	if ( SERVER_IsValidClient( ulClient ) == false )
		return;

	CLIENT_s *pClient = SERVER_GetClient ( ulClient );
	NetCommand command ( SVC2_SRP_USER_START_AUTHENTICATION );
	command.addString ( pClient->username.GetChars() );
	command.sendCommandToClients ( ulClient, SVCF_ONLYTHISCLIENT );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SRPUserProcessChallenge ( const ULONG ulClient )
{
	if ( SERVER_IsValidClient( ulClient ) == false )
		return;

	CLIENT_s *pClient = SERVER_GetClient ( ulClient );
	NetCommand command ( SVC2_SRP_USER_PROCESS_CHALLENGE );
	command.addByte ( pClient->salt.Size() );
	for ( unsigned int i = 0; i < pClient->salt.Size(); ++i )
		command.addByte ( pClient->salt[i] );
	command.addShort ( pClient->bytesB.Size() );
	for ( unsigned int i = 0; i < pClient->bytesB.Size(); ++i )
		command.addByte ( pClient->bytesB[i] );
	command.sendCommandToClients ( ulClient, SVCF_ONLYTHISCLIENT );
}

//*****************************************************************************
//
void SERVERCOMMANDS_SRPUserVerifySession ( const ULONG ulClient )
{
	if ( SERVER_IsValidClient( ulClient ) == false )
		return;

	CLIENT_s *pClient = SERVER_GetClient ( ulClient );
	NetCommand command ( SVC2_SRP_USER_VERIFY_SESSION );
	command.addShort ( pClient->bytesHAMK.Size() );
	for ( unsigned int i = 0; i < pClient->bytesHAMK.Size(); ++i )
		command.addByte ( pClient->bytesHAMK[i] );
	command.sendCommandToClients ( ulClient, SVCF_ONLYTHISCLIENT );
}

//*****************************************************************************
//
void SERVERCOMMANDS_ShootDecal ( const FDecalTemplate* tpl, AActor* actor, fixed_t z, angle_t angle, fixed_t tracedist,
	bool permanent, ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	if ( EnsureActorHasNetID( actor ) == false )
		return;

	NetCommand command ( SVC2_SHOOTDECAL );
	command.addName( tpl->GetName() );
	command.addShort( actor->lNetID );
	command.addShort( z >> FRACBITS );
	command.addShort( angle >> FRACBITS );
	command.addLong( tracedist );
	command.addByte( permanent );
	command.sendCommandToClients ( ulPlayerExtra, flags );
}

//*****************************************************************************
//
void APathFollower::SyncWithClient ( const ULONG ulClient )
{
	if ( !EnsureActorHasNetID (this) )
		return;

	NetCommand command( SVC2_SYNCPATHFOLLOWER );
	command.addShort( this->lNetID );
	command.addShort( this->CurrNode ? this->CurrNode->lNetID : -1 );
	command.addShort( this->PrevNode ? this->PrevNode->lNetID : -1 );
	command.addFloat( this->Time );
	command.sendCommandToOneClient( ulClient );
}
