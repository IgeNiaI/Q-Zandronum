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
// Filename: cl_commands.cpp
//
// Description: Contains a set of functions that correspond to each message a client
// can send out. Each functions handles the send out of each message.
//
//-----------------------------------------------------------------------------

#include "cl_commands.h"
#include "cl_demo.h"
#include "cl_main.h"
#include "doomstat.h"
#include "d_event.h"
#include "gi.h"
#include "network.h"
#include "r_state.h"
#include "v_text.h" // [RC] To conform player names
#include "gamemode.h"
#include "lastmanstanding.h"
#include "deathmatch.h"
#include "chat.h"
#include "network_enums.h"
#include "p_acs.h"
#include "v_video.h"

//*****************************************************************************
//	VARIABLES

static	ULONG	g_ulLastChangeTeamTime = 0;
static	ULONG	g_ulLastSuicideTime = 0;
static	ULONG	g_ulLastJoinTime = 0;
static	ULONG	g_ulLastDropTime = 0;
static	ULONG	g_ulLastSVCheatMessageTime = 0;
static	bool g_bIgnoreWeaponSelect = false;
SDWORD g_sdwCheckCmd = 0;

//*****************************************************************************
//	FUNCTIONS

void CLIENT_ResetFloodTimers( void )
{
	g_ulLastChangeTeamTime = 0;
	g_ulLastSuicideTime = 0;
	g_ulLastJoinTime = 0;
	g_ulLastDropTime = 0;
	g_ulLastSVCheatMessageTime = 0;
}

//*****************************************************************************
//
void CLIENT_IgnoreWeaponSelect( bool bIgnore )
{
	g_bIgnoreWeaponSelect = bIgnore;
}

//*****************************************************************************
//
bool CLIENT_GetIgnoreWeaponSelect( void )
{
	return g_bIgnoreWeaponSelect;
}

//*****************************************************************************
//
bool CLIENT_AllowSVCheatMessage( void )
{
	if ( ( g_ulLastSVCheatMessageTime > 0 ) && ( (ULONG)gametic < ( g_ulLastSVCheatMessageTime + ( TICRATE * 1 ))))
		return false;
	else
	{
		g_ulLastSVCheatMessageTime = gametic;
		return true;
	}
}

//*****************************************************************************
//
bool UserInfoSortingFunction::operator()( FName cvar1Name, FName cvar2Name ) const
{
	FBaseCVar* cvar1 = FindCVar( cvar1Name, nullptr );
	FBaseCVar* cvar2 = FindCVar( cvar2Name, nullptr );

	if ( cvar1 && cvar2 && ((( cvar1->GetFlags() & CVAR_MOD ) ^ ( cvar2->GetFlags() & CVAR_MOD )) != 0 ))
	{
		// If one of the cvars contains the mod flag and the other one does not,the one that
		// does is goes before the other one.
		return !!( cvar2->GetFlags() & CVAR_MOD );
	}
	else
	{
		// Otherwise we don't really care.
		return cvar1 < cvar2;
	}
}

//*****************************************************************************
//
static void clientcommands_WriteCVarToUserinfo( FName name, FBaseCVar *cvar )
{
	// [BB] It's pointless to tell the server of the class, if only one class is available.
	if (( name == NAME_PlayerClass ) && ( PlayerClasses.Size( ) == 1 ))
		return;

	// [TP] Don't bother sending these
	if (( cvar == nullptr ) || ( cvar->GetFlags() & CVAR_UNSYNCED_USERINFO ))
		return;

	FString value;
	// [BB] Skin needs special treatment, so that the clients can use skins the server doesn't have.
	if ( name == NAME_Skin )
		value = skins[players[consoleplayer].userinfo.GetSkin()].name;
	else
		value = cvar->GetGenericRep( CVAR_String ).String;

	unsigned int elementNetSize = value.Len() + 1;

	// [BB] Name will be transferred as short.
	if ( name.IsPredefined() )
		elementNetSize += 2;
	// [BB] Name will be transferred as short (-1) + string + terminating 0.
	else
		elementNetSize += strlen ( name.GetChars() ) + 2 + 1;

	// [BB] If the this cvar doesn't fit into the packet anymore, send what we have
	// and start a new packet.
	// NAME_None is transferred as short and the maximum packet size is intentionally
	// hard coded to 1024. The clients shouldn't mess with this setting.
	if ( ( CLIENT_GetLocalBuffer( )->CalcSize() + elementNetSize + 2 ) >= 1024 )
	{
		// [BB] Terminate the current CLC_USERINFO command.
		NETWORK_WriteName( &CLIENT_GetLocalBuffer( )->ByteStream, NAME_None );
		CLIENT_SendServerPacket();
		NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_USERINFO );
	}

	NETWORK_WriteName( &CLIENT_GetLocalBuffer( )->ByteStream, name );
	NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, value );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_SendAllUserInfo()
{
	// Temporarily disable userinfo for when the player setup menu updates our userinfo. Then
	// we can just send all our userinfo in one big bulk, instead of each time it updates
	// a userinfo property.
	if ( CLIENT_GetAllowSendingOfUserInfo( ) == false )
		return;

	const userinfo_t &userinfo = players[consoleplayer].userinfo;
	userinfo_t::ConstIterator iterator ( userinfo );
	UserInfoChanges cvarNames;

	for ( userinfo_t::ConstPair *pair; iterator.NextPair( pair ); )
		cvarNames.insert( pair->Key );

	CLIENTCOMMANDS_UserInfo ( cvarNames );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_UserInfo( const UserInfoChanges &cvars )
{
	// Temporarily disable userinfo for when the player setup menu updates our userinfo. Then
	// we can just send all our userinfo in one big bulk, instead of each time it updates
	// a userinfo property.
	if ( CLIENT_GetAllowSendingOfUserInfo( ) == false )
		return;

	// [BB] Make sure that we only send anything to the server, if cvarNames actually
	// contains cvars that we want to send.
	bool sendUserinfo = false;

	for ( UserInfoChanges::const_iterator iterator = cvars.begin(); iterator != cvars.end(); ++iterator )
	{
		FBaseCVar **cvarPointer = players[consoleplayer].userinfo.CheckKey( *iterator );
		if ( cvarPointer && ( (*cvarPointer)->GetFlags() & CVAR_UNSYNCED_USERINFO ) == false )
		{
			sendUserinfo = true;
			break;
		}
	}

	if ( sendUserinfo == false )
		return;

	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_USERINFO );

	for ( UserInfoChanges::const_iterator iterator = cvars.begin(); iterator != cvars.end(); ++iterator )
	{
		FName name = *iterator;
		FBaseCVar **cvarPointer = players[consoleplayer].userinfo.CheckKey( name );
		FBaseCVar *cvar = cvarPointer ? *cvarPointer : nullptr;
		clientcommands_WriteCVarToUserinfo( name, cvar );
	}

	NETWORK_WriteName( &CLIENT_GetLocalBuffer( )->ByteStream, NAME_None );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_StartChat( void )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_STARTCHAT );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_EndChat( void )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_ENDCHAT );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_EnterConsole( void )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_ENTERCONSOLE );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_ExitConsole( void )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_EXITCONSOLE );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_Say( ULONG ulMode, const char *pszString )
{
	// [TP] Limit messages to certain length.
	FString chatstring ( pszString );

	if ( chatstring.Len() > MAX_CHATBUFFER_LENGTH )
		chatstring = chatstring.Left( MAX_CHATBUFFER_LENGTH );

	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_SAY );
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, ulMode );
	NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, chatstring );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_Ignore( ULONG ulPlayer, bool bIgnore, LONG lTicks )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_IGNORE );
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, ulPlayer );
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, bIgnore );
	NETWORK_WriteLong( &CLIENT_GetLocalBuffer( )->ByteStream, lTicks );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_ClientMove( void )
{
	ticcmd_t	*pCmd;
	ULONG		ulBits;

	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_CLIENTMOVE );
	NETWORK_WriteLong( &CLIENT_GetLocalBuffer( )->ByteStream, gametic );
	// [CK] Send the server the latest known server-gametic
	NETWORK_WriteLong( &CLIENT_GetLocalBuffer( )->ByteStream, CLIENT_GetLatestServerGametic( ) );

	// Decide what additional information needs to be sent.
	ulBits = 0;
	pCmd = &players[consoleplayer].cmd;

	// [BB] If we think that we can't move, don't even try to tell the server that we
	// want to move.
	if ( players[consoleplayer].mo->reactiontime )
	{
		pCmd->ucmd.forwardmove = 0;
		pCmd->ucmd.sidemove = 0;
		pCmd->ucmd.buttons &= ~BT_JUMP;
	}

	if ( pCmd->ucmd.yaw )
		ulBits |= CLIENT_UPDATE_YAW;
	if ( pCmd->ucmd.pitch )
		ulBits |= CLIENT_UPDATE_PITCH;
	if ( pCmd->ucmd.roll )
		ulBits |= CLIENT_UPDATE_ROLL;
	if ( pCmd->ucmd.buttons )
	{
		ulBits |= CLIENT_UPDATE_BUTTONS;
		if ( zacompatflags & ZACOMPATF_CLIENTS_SEND_FULL_BUTTON_INFO )
			ulBits |= CLIENT_UPDATE_BUTTONS_LONG;
	}
	if ( pCmd->ucmd.forwardmove )
		ulBits |= CLIENT_UPDATE_FORWARDMOVE;
	if ( pCmd->ucmd.sidemove )
		ulBits |= CLIENT_UPDATE_SIDEMOVE;
	if ( pCmd->ucmd.upmove )
		ulBits |= CLIENT_UPDATE_UPMOVE;

	// Tell the server what information we'll be sending.
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, ulBits );

	// Send the necessary movement/steering information.
	if ( ulBits & CLIENT_UPDATE_YAW )
		NETWORK_WriteShort( &CLIENT_GetLocalBuffer( )->ByteStream, pCmd->ucmd.yaw );
	if ( ulBits & CLIENT_UPDATE_PITCH )
		NETWORK_WriteShort( &CLIENT_GetLocalBuffer( )->ByteStream, pCmd->ucmd.pitch );
	if ( ulBits & CLIENT_UPDATE_ROLL )
		NETWORK_WriteShort( &CLIENT_GetLocalBuffer( )->ByteStream, pCmd->ucmd.roll );
	if ( ulBits & CLIENT_UPDATE_BUTTONS )
	{
		if ( ulBits & CLIENT_UPDATE_BUTTONS_LONG )
			NETWORK_WriteLong( &CLIENT_GetLocalBuffer( )->ByteStream, pCmd->ucmd.buttons );
		else
			NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, pCmd->ucmd.buttons );
	}
	if ( ulBits & CLIENT_UPDATE_FORWARDMOVE )
		NETWORK_WriteShort( &CLIENT_GetLocalBuffer( )->ByteStream, pCmd->ucmd.forwardmove );
	if ( ulBits & CLIENT_UPDATE_SIDEMOVE )
		NETWORK_WriteShort( &CLIENT_GetLocalBuffer( )->ByteStream, pCmd->ucmd.sidemove );
	if ( ulBits & CLIENT_UPDATE_UPMOVE )
		NETWORK_WriteShort( &CLIENT_GetLocalBuffer( )->ByteStream, pCmd->ucmd.upmove );

	NETWORK_WriteLong( &CLIENT_GetLocalBuffer( )->ByteStream, players[consoleplayer].mo->angle );
	NETWORK_WriteLong( &CLIENT_GetLocalBuffer( )->ByteStream, players[consoleplayer].mo->pitch );
	// [BB] Send the checksum of our ticcmd we calculated when we originally generated the ticcmd from the user input.
	NETWORK_WriteLong( &CLIENT_GetLocalBuffer( )->ByteStream, g_sdwCheckCmd );

	// Attack button.
	if ( pCmd->ucmd.buttons & BT_ATTACK )
	{
		if ( players[consoleplayer].ReadyWeapon == NULL )
			NETWORK_WriteShort( &CLIENT_GetLocalBuffer( )->ByteStream, 0 );
		else
			NETWORK_WriteShort( &CLIENT_GetLocalBuffer( )->ByteStream, players[consoleplayer].ReadyWeapon->GetClass( )->getActorNetworkIndex() );
	}
}

//*****************************************************************************
//
void CLIENTCOMMANDS_MissingPacket( void )
{
}

//*****************************************************************************
//
void CLIENTCOMMANDS_Pong( ULONG ulTime )
{
	// [BB] CLIENTCOMMANDS_Pong is the only client command function that
	// immediately launches a network packet. This is something that
	// we obviously don't want to do when playing a demo.
	if ( CLIENTDEMO_IsPlaying( ) )
		return;

	// [BB] For an accurate ping measurement, the client has to answer
	// immediately instead of sending the answer together with the the
	// other commands tic-synced in CLIENT_EndTick().
	NETBUFFER_s	TempBuffer;
	TempBuffer.Init( MAX_UDP_PACKET, BUFFERTYPE_WRITE );
	TempBuffer.Clear();
	NETWORK_WriteByte( &TempBuffer.ByteStream, CLC_PONG );
	NETWORK_WriteLong( &TempBuffer.ByteStream, ulTime );
	NETWORK_LaunchPacket( &TempBuffer, NETWORK_GetFromAddress( ) );
	TempBuffer.Free();
}

//*****************************************************************************
//
void CLIENTCOMMANDS_WeaponSelect( const PClass *pType )
{
	if ( ( pType == NULL ) || g_bIgnoreWeaponSelect )
		return;

	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_WEAPONSELECT );
	NETWORK_WriteShort( &CLIENT_GetLocalBuffer( )->ByteStream, pType->getActorNetworkIndex() );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_Taunt( void )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_TAUNT );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_Spectate( void )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_SPECTATE );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_RequestJoin( const char *pszJoinPassword )
{
	if (( sv_limitcommands ) && ( g_ulLastJoinTime > 0 ) && ( (ULONG)gametic < ( g_ulLastJoinTime + ( TICRATE * 3 ))))
	{
		Printf( "You must wait at least 3 seconds before joining again.\n" );
		return;
	}

	g_ulLastJoinTime = gametic;
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_REQUESTJOIN );
	NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, pszJoinPassword );

	// [BB/Spleen] Send the gametic so that the client doesn't think it's lagging.
	NETWORK_WriteLong( &CLIENT_GetLocalBuffer( )->ByteStream, gametic );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_RequestRCON( const char *pszRCONPassword )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_REQUESTRCON );
	NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, pszRCONPassword );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_RCONCommand( const char *pszCommand )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_RCONCOMMAND );
	NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, pszCommand );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_Suicide( void )
{
	if (( sv_limitcommands ) && ( g_ulLastSuicideTime > 0 ) && ( (ULONG)gametic < ( g_ulLastSuicideTime + ( TICRATE * 10 ))))
	{
		Printf( "You must wait at least 10 seconds before suiciding again.\n" );
		return;
	}

	g_ulLastSuicideTime = gametic;
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_SUICIDE );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_ChangeTeam( const char *pszJoinPassword, LONG lDesiredTeam )
{
	if (( sv_limitcommands ) && !( ( lastmanstanding || teamlms ) && ( LASTMANSTANDING_GetState( ) == LMSS_COUNTDOWN ) ) && ( g_ulLastChangeTeamTime > 0 ) && ( (ULONG)gametic < ( g_ulLastChangeTeamTime + ( TICRATE * 3 ))))
	{
		Printf( "You must wait at least 3 seconds before changing teams again.\n" );
		return;
	}

	g_ulLastChangeTeamTime = gametic;
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_CHANGETEAM );
	NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, pszJoinPassword );
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, lDesiredTeam );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_SpectateInfo( void )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_SPECTATEINFO );
	NETWORK_WriteLong( &CLIENT_GetLocalBuffer( )->ByteStream, gametic );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_GenericCheat( LONG lCheat )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_GENERICCHEAT );
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, lCheat );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_GiveCheat( char *pszItem, LONG lAmount )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_GIVECHEAT );
	NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, pszItem );
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, lAmount );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_TakeCheat( const char *item, LONG amount )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_TAKECHEAT );
	NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, item );
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, amount );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_SummonCheat( const char *pszItem, LONG lType, const bool bSetAngle, const SHORT sAngle )
{
	int commandtype = 0;

	switch ( lType )
	{
	case DEM_SUMMON: commandtype = CLC_SUMMONCHEAT; break;
	case DEM_SUMMONFRIEND: commandtype = CLC_SUMMONFRIENDCHEAT; break;
	case DEM_SUMMONFOE: commandtype = CLC_SUMMONFOECHEAT; break;
	default: return;
	}

	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, commandtype );
	NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, pszItem );
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, bSetAngle );
	if ( bSetAngle )
		NETWORK_WriteShort( &CLIENT_GetLocalBuffer( )->ByteStream, sAngle );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_ReadyToGoOn( void )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_READYTOGOON );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_ChangeDisplayPlayer( LONG lDisplayPlayer )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_CHANGEDISPLAYPLAYER );
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, lDisplayPlayer );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_AuthenticateLevel( void )
{
}

//*****************************************************************************
//
void CLIENTCOMMANDS_CallVote( LONG lVoteCommand, const char *pszArgument, const char *pszReason )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_CALLVOTE );
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, lVoteCommand );
	NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, pszArgument );
	NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, pszReason );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_VoteYes( void )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_VOTEYES );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_VoteNo( void )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_VOTENO );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_RequestInventoryUseAll( void )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_INVENTORYUSEALL );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_RequestInventoryUse( AInventory *item )
{
	if ( item == NULL )
		return;

	const USHORT usActorNetworkIndex = item->GetClass( )->getActorNetworkIndex();

	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_INVENTORYUSE );
	NETWORK_WriteShort( &CLIENT_GetLocalBuffer( )->ByteStream, usActorNetworkIndex );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_RequestInventoryDrop( AInventory *pItem )
{
	// [BB] The server may forbid dropping completely.
	if ( zadmflags & ZADF_NODROP )
	{
		Printf( "Dropping items is not allowed in this server.\n" );
		return;
	}

	if ( sv_limitcommands && ( g_ulLastDropTime > 0 ) && ( (ULONG)gametic < g_ulLastDropTime + TICRATE ))
	{
		Printf( "You must wait at least one second before using drop again.\n" );
		return;
	}

	if ( pItem == NULL )
		return;

	const USHORT usActorNetworkIndex = pItem->GetClass( )->getActorNetworkIndex();

	g_ulLastDropTime = gametic;

	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_INVENTORYDROP );
	NETWORK_WriteShort( &CLIENT_GetLocalBuffer( )->ByteStream, usActorNetworkIndex );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_Puke ( int scriptNum, int args[4], bool always )
{
	if ( ACS_ExistsScript( scriptNum ) == false )
		return;

	// [TP] Calculate argn from args.
	int argn = ( args[3] != 0 ) ? 4 :
	           ( args[2] != 0 ) ? 3 :
	           ( args[1] != 0 ) ? 2 :
	           ( args[0] != 0 ) ? 1 : 0;

	const int scriptNetID = NETWORK_ACSScriptToNetID( scriptNum );

	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_PUKE );
	NETWORK_WriteLong( &CLIENT_GetLocalBuffer( )->ByteStream, scriptNetID );

	// [TP/BB] If we don't have a netID on file for this script, we send the name as a string.
	if ( scriptNetID == NO_SCRIPT_NETID )
		NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, FName( ENamedName( -scriptNum )));

	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, argn );

	for ( int i = 0; i < argn; ++i )
		NETWORK_WriteLong ( &CLIENT_GetLocalBuffer( )->ByteStream, args[i] );

	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, always );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_MorphCheat ( const char *pszMorphClass )
{
	if ( pszMorphClass == NULL )
		return;

	if ( PClass::FindClass ( pszMorphClass ) == NULL )
	{
		Printf ( "Unknown class %s\n", pszMorphClass );
		return;
	}

	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_MORPHEX );
	NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, pszMorphClass );
}

//*****************************************************************************
//
void CLIENTCOMMANDS_FullUpdateReceived ( void )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_FULLUPDATE );
}

//*****************************************************************************
// [Dusk]
void CLIENTCOMMANDS_InfoCheat( AActor* mobj, bool extended )
{
	if ( mobj == NULL || mobj->lNetID == -1 )
		return;

	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_INFOCHEAT );
	NETWORK_WriteShort( &CLIENT_GetLocalBuffer( )->ByteStream, mobj->lNetID );
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, extended );
}

//*****************************************************************************
// [TP]
void CLIENTCOMMANDS_WarpCheat( fixed_t x, fixed_t y )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_WARPCHEAT );
	NETWORK_WriteLong( &CLIENT_GetLocalBuffer( )->ByteStream, x );
	NETWORK_WriteLong( &CLIENT_GetLocalBuffer( )->ByteStream, y );
}

//*****************************************************************************
// [TP]
void CLIENTCOMMANDS_KillCheat( const char* what )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_KILLCHEAT );
	NETWORK_WriteString( &CLIENT_GetLocalBuffer( )->ByteStream, what );
}

//*****************************************************************************
// [TP]
void CLIENTCOMMANDS_SpecialCheat( int special, const TArray<int> &args )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_SPECIALCHEAT );
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, special );
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, args.Size() );

	for ( unsigned int i = 0; i < args.Size(); ++i )
		NETWORK_WriteLong( &CLIENT_GetLocalBuffer( )->ByteStream, args[i] );
}

//*****************************************************************************
// [TP]
void CLIENTCOMMANDS_SetWantHideAccount( bool wantHideAccount )
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_SETWANTHIDEACCOUNT );
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, wantHideAccount );
}

//*****************************************************************************
// [TP]
void CLIENTCOMMANDS_SetVideoResolution()
{
	NETWORK_WriteByte( &CLIENT_GetLocalBuffer( )->ByteStream, CLC_SETVIDEORESOLUTION );
	NETWORK_WriteShort( &CLIENT_GetLocalBuffer( )->ByteStream, SCREENWIDTH );
	NETWORK_WriteShort( &CLIENT_GetLocalBuffer( )->ByteStream, SCREENHEIGHT );
}
