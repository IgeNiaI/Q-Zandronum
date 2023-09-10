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
// Filename: cl_main.cpp
//
// Description: Contains variables and routines related to the client portion
// of the program.
//
//-----------------------------------------------------------------------------

#include "networkheaders.h"
#include "a_action.h"
#include "a_sharedglobal.h"
#include "a_doomglobal.h"
#include "announcer.h"
#include "browser.h"
#include "cl_commands.h"
#include "cl_demo.h"
#include "cl_statistics.h"
#include "cooperative.h"
#include "doomerrors.h"
#include "doomtype.h"
#include "doomstat.h"
#include "d_player.h"
#include "g_game.h"
#include "gamemode.h"
#include "d_net.h"
#include "p_local.h"
#include "s_sound.h"
#include "gi.h"
#include "i_net.h"
#include "i_system.h"
#include "c_dispatch.h"
#include "st_stuff.h"
#include "m_argv.h"
#include "m_cheat.h"
#include "cl_main.h"
#include "p_effect.h"
#include "p_lnspec.h"
#include "possession.h"
#include "c_console.h"
#include "s_sndseq.h"
#include "version.h"
#include "p_acs.h"
#include "p_enemy.h"
#include "survival.h"
#include "v_palette.h"
#include "v_video.h"
#include "w_wad.h"
#include "deathmatch.h"
#include "duel.h"
#include "team.h"
#include "chat.h"
#include "announcer.h"
#include "network.h"
#include "sv_main.h"
#include "sbar.h"
#include "m_random.h"
#include "templates.h"
#include "stats.h"
#include "lastmanstanding.h"
#include "scoreboard.h"
#include "joinqueue.h"
#include "callvote.h"
#include "invasion.h"
#include "r_sky.h"
#include "r_data/r_translate.h"
#include "domination.h"
#include "p_3dmidtex.h"
#include "a_lightning.h"
#include "a_movingcamera.h"
#include "d_netinf.h"
#include "po_man.h"
#include "network/cl_auth.h"
#include "r_data/colormaps.h"
#include "r_main.h"
#include "network_enums.h"
#include "decallib.h"
#include "network/servercommands.h"
#include "am_map.h"
#include "maprotation.h"

//*****************************************************************************
//	MISC CRAP THAT SHOULDN'T BE HERE BUT HAS TO BE BECAUSE OF SLOPPY CODING

//void	ChangeSpy (bool forward);
int		D_PlayerClassToInt (const char *classname);
void	ClientObituary (AActor *self, AActor *inflictor, AActor *attacker, int dmgflags, FName MeansOfDeath);
void	P_CrouchMove(player_t * player, int direction);
void	I_UpdateDiscordPresence(bool SendPresence);
int		client_curplayers = 0;
int		client_maxplayers = 0;
extern	bool	SpawningMapThing;
extern FILE *Logfile;
void	D_ErrorCleanup ();
extern bool FriendlyFire;
extern const AInventory *SendItemUse;
DECLARE_ACTION(A_RestoreSpecialPosition)

EXTERN_CVAR( Bool, telezoom )
EXTERN_CVAR( Bool, sv_cheats )
EXTERN_CVAR( Int, cl_bloodtype )
EXTERN_CVAR( Int, cl_pufftype )
EXTERN_CVAR( String, playerclass )
EXTERN_CVAR( Int, am_cheat )
EXTERN_CVAR( Bool, cl_oldfreelooklimit )
EXTERN_CVAR( Float, turbo )
EXTERN_CVAR( Float, sv_gravity )
EXTERN_CVAR( Float, sv_aircontrol )
EXTERN_CVAR( Float, splashfactor )
EXTERN_CVAR( Float, sv_headbob )
EXTERN_CVAR( Bool, cl_hideaccount )
EXTERN_CVAR( String, name )

// [AK] Restores the old mouse behaviour from Skulltag.
CVAR( Bool, cl_useskulltagmouse, false, CVAR_GLOBALCONFIG | CVAR_ARCHIVE )


//*****************************************************************************
//	CONSOLE COMMANDS/VARIABLES

#ifdef	_DEBUG
CVAR( Bool, cl_emulatepacketloss, false, 0 )
#endif
// [BB]
CVAR( Bool, cl_connectsound, true, CVAR_ARCHIVE )
CVAR( Bool, cl_showwarnings, false, CVAR_ARCHIVE )

CUSTOM_CVAR(Int, cl_packetdup, 0, CVAR_ARCHIVE)
{
	if (self < 0)
		self = 0;
	else if (self > 3)
		self = 3;
}

CUSTOM_CVAR(Int, fov_change_cooldown_tics, 0, CVAR_SERVERINFO)
{
	if (self < 0)
		self = 0;
	else if (self > 255)
		self = 255;

	if (NETWORK_GetState() == NETSTATE_SERVER)
		SERVERCOMMANDS_SetCVar(fov_change_cooldown_tics);
}
//*****************************************************************************
//	VARIABLES

// Local network buffer for the client.
static	NETBUFFER_s			g_LocalBuffer;

// The address of the server we're connected to or trying to connect to.
static	NETADDRESS_s		g_AddressServer;

// The address of the last server we tried to connect to/we're connected to.
static	NETADDRESS_s		g_AddressLastConnected;

// Last time we heard from the server.
static	ULONG				g_ulLastServerTick;

// State of the client's connection to the server.
static	CONNECTIONSTATE_e	g_ConnectionState;

// Have we heard from the server recently?
static	bool				g_bServerLagging;

// What's the time of the last message the server got from us?
static	bool				g_bClientLagging;

// [BB] Are we currently receiving a full update?
static	bool				g_bFullUpdateIncomplete = false;

// [BB] Time we received the end of the last full update from the server.
static ULONG				g_ulEndFullUpdateTic = 0;

// Used for item respawning client-side.
static	FRandom				g_RestorePositionSeed( "ClientRestorePos" );

// Is it okay to send user info, or is that currently disabled?
static	bool				g_bAllowSendingOfUserInfo = true;

// The name of the map we need to authenticate.
static	char				g_szMapName[9];

// Last console player update tick.
static	ULONG				g_ulLastConsolePlayerUpdateTick;

// This is how many ticks left before we try again to connect to the server,
// request a snapshot, etc.
static	ULONG				g_ulRetryTicks;

// The "message of the day" string. Display it when we're done receiving
// our snapshot.
static	FString				g_MOTD;

// Is the client module parsing a packet?
static	bool				g_bIsParsingPacket;

// This contains the last PACKET_BUFFER_SIZE packets we've received.
static	PACKETBUFFER_s		g_ReceivedPacketBuffer;

// This is the start position of each packet within that buffer.
static	LONG				g_lPacketBeginning[PACKET_BUFFER_SIZE];

// This is the sequences of the last PACKET_BUFFER_SIZE packets we've received.
static	LONG				g_lPacketSequence[PACKET_BUFFER_SIZE];

// This is the  size of the last PACKET_BUFFER_SIZE packets we've received.
static	LONG				g_lPacketSize[PACKET_BUFFER_SIZE];

// This is the index of the incoming packet.
static	BYTE				g_bPacketNum;

// This is the current position in the received packet buffer.
static	LONG				g_lCurrentPosition;

// This is the sequence of the last packet we parsed.
static	LONG				g_lLastParsedSequence;

// This is the highest sequence of any packet we've received.
static	LONG				g_lHighestReceivedSequence;

// Delay for sending a request missing packets.
static	LONG				g_lMissingPacketTicks;

// Debugging variables.
static	LONG				g_lLastCmd;

// [CK] The most up-to-date server gametic
static	int				g_lLatestServerGametic = 0;

// [TP] Client's understanding of the account names of players.
static FString				g_PlayerAccountNames[MAXPLAYERS];

//*****************************************************************************
//	FUNCTIONS

//*****************************************************************************
//
void CLIENT_ClearAllPlayers( void )
{
	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ++ulIdx )
	{
		playeringame[ulIdx] = false;

		// Zero out all the player information.
		PLAYER_ResetPlayerData( &players[ulIdx] );
	}
}

//*****************************************************************************
//
void CLIENT_LimitProtectedCVARs( void )
{
	if ( turbo > 100.f )
		turbo = 100.f;
}

//*****************************************************************************
//
void CLIENT_Construct( void )
{
	const char	*pszPort;
	const char	*pszIPAddress;
	const char	*pszDemoName;
	USHORT		usPort;
	UCVarValue	Val;

	// Start off as being disconnected.
	g_ConnectionState = CTS_DISCONNECTED;
	g_ulRetryTicks = 0;

	// Check if the user wants to use an alternate port for the server.
	pszPort = Args->CheckValue( "-port" );
    if ( pszPort )
    {
       usPort = atoi( pszPort );
       Printf( PRINT_HIGH, "Connecting using alternate port %i.\n", usPort );
    }
	else 
	   usPort = DEFAULT_CLIENT_PORT;

	// Set up a socket and network message buffer.
	NETWORK_Construct( usPort, true );

	g_LocalBuffer.Init( MAX_UDP_PACKET * 8, BUFFERTYPE_WRITE );
	g_LocalBuffer.Clear();

	// Initialize the stored packets buffer.
	g_ReceivedPacketBuffer.lMaxSize = MAX_UDP_PACKET * PACKET_BUFFER_SIZE;
	memset( g_ReceivedPacketBuffer.abData, 0, MAX_UDP_PACKET * PACKET_BUFFER_SIZE );

	// Connect to a server right off the bat.
    pszIPAddress = Args->CheckValue( "-connect" );
    if ( pszIPAddress )
    {
		// Convert the given IP string into our server address.
		g_AddressServer.LoadFromString( pszIPAddress );

		// If the user didn't specify a port, use the default one.
		if ( g_AddressServer.usPort == 0 )
			g_AddressServer.SetPort( DEFAULT_SERVER_PORT );

		// If we try to reconnect, use this address.
		g_AddressLastConnected = g_AddressServer;

		// Put us in a "connection attempt" state.
		g_ConnectionState = CTS_ATTEMPTINGCONNECTION;

		// Put the game in client mode.
		NETWORK_SetState( NETSTATE_CLIENT );

		// Make sure cheats are off.
		Val.Bool = false;
		sv_cheats.ForceSet( Val, CVAR_Bool );
		am_cheat = 0;

		// Make sure our visibility is normal.
		R_SetVisibility( 8.0f );

		CLIENT_ClearAllPlayers();

		// If we've elected to record a demo, begin that process now.
		pszDemoName = Args->CheckValue( "-record" );
		if ( pszDemoName )
			CLIENTDEMO_BeginRecording( pszDemoName );
    }
	// User didn't specify an IP to connect to, so don't try to connect to anything.
	else
		g_ConnectionState = CTS_DISCONNECTED;

	g_MOTD = "";
	g_bIsParsingPacket = false;

	// Call CLIENT_Destruct() when Skulltag closes.
	atterm( CLIENT_Destruct );
}

//*****************************************************************************
//
void CLIENT_Destruct( void )
{
	// [BC] Tell the server we're leaving the game.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENT_QuitNetworkGame( NULL );

	// Free our local buffer.
	g_LocalBuffer.Free();
}

//*****************************************************************************
//
void CLIENT_Tick( void )
{
	// Determine what to do based on our connection state.
	switch ( g_ConnectionState )
	{
	// Just chillin' at the full console. Nothing to do.
	case CTS_DISCONNECTED:

		break;
	// At the fullconsole, attempting to connect to a server.
	case CTS_ATTEMPTINGCONNECTION:

		// If we're not connected to a server, and have an IP specified, try to connect.
		if ( g_AddressServer.abIP[0] )
			CLIENT_AttemptConnection( );
		break;
	// A connection has been established with the server; now authenticate the level.
	case CTS_ATTEMPTINGAUTHENTICATION:

		CLIENT_AttemptAuthentication( g_szMapName );
		break;
	// The level has been authenticated. Request level data and send user info.
	case CTS_REQUESTINGSNAPSHOT:

		// Send data request and userinfo.
		CLIENT_RequestSnapshot( );
		break;

	// [BB] We started receiving the snapshot. Make sure that we don't wait
	// for it forever to finish, if the packet with the end of the snapshot
	// was dropped and the server had no reason to send more packets.
	case CTS_RECEIVINGSNAPSHOT:
		if ( g_ulRetryTicks )
		{
			g_ulRetryTicks--;
			break;
		}

		g_ulRetryTicks = GAMESTATE_RESEND_TIME;

		// [BB] This will cause the server to send another reliable packet.
		// This way, we notice whether we are missing the latest packets
		// from the server.
		CLIENTCOMMANDS_EndChat();

		break;

	default:

		break;
	}
}

//*****************************************************************************
//
void CLIENT_EndTick( void )
{
	// [TP] Do we want to change our weapon or use an item or something like that?
	if ( SendItemUse )
	{
		// Why did ZDoom make SendItemUse const? That's just stupid.
		AInventory* item = const_cast<AInventory*>( SendItemUse );

		if ( item == (AInventory*)1 )
		{
			if ( zacompatflags & ZACOMPATF_PREDICT_FUNCTIONS )
			{
				AInventory *item = players[consoleplayer].mo->Inventory;

				while (item != NULL)
				{
					AInventory *next = item->Inventory;
					if (item->ItemFlags & IF_INVBAR && !(item->IsKindOf(RUNTIME_CLASS(APuzzleItem))))
					{
						players[consoleplayer].mo->UseInventory (item);
					}
					item = next;
				}
			}
			CLIENTCOMMANDS_RequestInventoryUseAll();
		}
		else if ( SendItemUse->IsKindOf( RUNTIME_CLASS( AWeapon ) ) )
		{
			players[consoleplayer].mo->UseInventory( item );
		}
		else
		{
			if ( zacompatflags & ZACOMPATF_PREDICT_FUNCTIONS )
			{
				players[consoleplayer].mo->UseInventory( item );
			}
			CLIENTCOMMANDS_RequestInventoryUse( item );
		}

		SendItemUse = NULL;
	}

	// If there's any data in our packet, send it to the server.
	if ( g_LocalBuffer.CalcSize() > 0 )
		CLIENT_SendServerPacket( );
}

//*****************************************************************************
//*****************************************************************************
//
CONNECTIONSTATE_e CLIENT_GetConnectionState( void )
{
	return ( g_ConnectionState );
}

//*****************************************************************************
//
void CLIENT_SetConnectionState( CONNECTIONSTATE_e State )
{
	UCVarValue	Val;

	g_ConnectionState = State;
	switch ( g_ConnectionState )
	{
	case CTS_RECEIVINGSNAPSHOT:

		Val.Bool = false;
		cooperative.ForceSet( Val, CVAR_Bool );
		survival.ForceSet( Val, CVAR_Bool );
		invasion.ForceSet( Val, CVAR_Bool );

		deathmatch.ForceSet( Val, CVAR_Bool );
		teamplay.ForceSet( Val, CVAR_Bool );
		duel.ForceSet( Val, CVAR_Bool );
		terminator.ForceSet( Val, CVAR_Bool );
		lastmanstanding.ForceSet( Val, CVAR_Bool );
		teamlms.ForceSet( Val, CVAR_Bool );
		possession.ForceSet( Val, CVAR_Bool );
		teampossession.ForceSet( Val, CVAR_Bool );
		domination.ForceSet( Val, CVAR_Bool );

		teamgame.ForceSet( Val, CVAR_Bool );
		ctf.ForceSet( Val, CVAR_Bool );
		oneflagctf.ForceSet( Val, CVAR_Bool );
		skulltag.ForceSet( Val, CVAR_Bool );

		sv_cheats.ForceSet( Val, CVAR_Bool );

		Val.Int = false;
		dmflags.ForceSet( Val, CVAR_Int );
		dmflags2.ForceSet( Val, CVAR_Int );
		compatflags.ForceSet( Val, CVAR_Int );

		gameskill.ForceSet( Val, CVAR_Int );
		botskill.ForceSet( Val, CVAR_Int );

		fraglimit.ForceSet( Val, CVAR_Int );
		timelimit.ForceSet( Val, CVAR_Int );
		pointlimit.ForceSet( Val, CVAR_Int );
		duellimit.ForceSet( Val, CVAR_Int );
		winlimit.ForceSet( Val, CVAR_Int );
		wavelimit.ForceSet( Val, CVAR_Int );
		break;
	default:
		break;
	}
}

//*****************************************************************************
//
NETBUFFER_s *CLIENT_GetLocalBuffer( void )
{
	return ( &g_LocalBuffer );
}

//*****************************************************************************
//
void CLIENT_SetLocalBuffer( NETBUFFER_s *pBuffer )
{
	g_LocalBuffer = *pBuffer;
}

//*****************************************************************************
//
ULONG CLIENT_GetLastServerTick( void )
{
	return ( g_ulLastServerTick );
}

//*****************************************************************************
//
void CLIENT_SetLastServerTick( ULONG ulTick )
{
	g_ulLastServerTick = ulTick;
}

//*****************************************************************************
//
ULONG CLIENT_GetLastConsolePlayerUpdateTick( void )
{
	return ( g_ulLastConsolePlayerUpdateTick );
}

//*****************************************************************************
//
void CLIENT_SetLastConsolePlayerUpdateTick( ULONG ulTick )
{
	g_ulLastConsolePlayerUpdateTick = ulTick;
}

//*****************************************************************************
//
bool CLIENT_GetServerLagging( void )
{
	return ( g_bServerLagging );
}

//*****************************************************************************
//
void CLIENT_SetServerLagging( bool bLagging )
{
	g_bServerLagging = bLagging;
}

//*****************************************************************************
//
bool CLIENT_GetClientLagging( void )
{
	return ( g_bClientLagging );
}

//*****************************************************************************
//
void CLIENT_SetClientLagging( bool bLagging )
{
	g_bClientLagging = bLagging;
}

//*****************************************************************************
//
NETADDRESS_s CLIENT_GetServerAddress( void )
{
	return ( g_AddressServer );
}

//*****************************************************************************
//
void CLIENT_SetServerAddress( NETADDRESS_s Address )
{
	g_AddressServer = Address;
}

//*****************************************************************************
//
bool CLIENT_GetAllowSendingOfUserInfo( void )
{
	return ( g_bAllowSendingOfUserInfo );
}

//*****************************************************************************
//
void CLIENT_SetAllowSendingOfUserInfo( bool bAllow )
{
	g_bAllowSendingOfUserInfo = bAllow;
}

//*****************************************************************************
//
// [CK] Get the latest gametic the server sent us (this can be zero if none is
// received).
int CLIENT_GetLatestServerGametic( void )
{
	return ( g_lLatestServerGametic );
}

//*****************************************************************************
// [CK] Set the latest gametic the server sent us. Negative numbers not allowed.
void CLIENT_SetLatestServerGametic( int latestServerGametic )
{
	if ( latestServerGametic >= 0 )
		g_lLatestServerGametic = latestServerGametic;
}

//*****************************************************************************
//
bool CLIENT_GetFullUpdateIncomplete ( void )
{
	return g_bFullUpdateIncomplete;
}

//*****************************************************************************
//
unsigned int CLIENT_GetEndFullUpdateTic( void )
{
	return g_ulEndFullUpdateTic;
}

//*****************************************************************************
//
const FString &CLIENT_GetPlayerAccountName( int player )
{
	static FString empty;

	if ( static_cast<unsigned>( player ) < countof( g_PlayerAccountNames ))
		return g_PlayerAccountNames[player];
	else
		return empty;
}

//*****************************************************************************
//
void CLIENT_SendServerPacket( void )
{
	// Add the size of the packet to the number of bytes sent.
	CLIENTSTATISTICS_AddToBytesSent( g_LocalBuffer.CalcSize());

	// Launch the packet, and clear out the buffer.
	NETWORK_LaunchPacket( &g_LocalBuffer, g_AddressServer );
	g_LocalBuffer.Clear();
}

//*****************************************************************************
//*****************************************************************************
//
void CLIENT_AttemptConnection( void )
{
	ULONG	ulIdx;

	if ( g_ulRetryTicks )
	{
		g_ulRetryTicks--;
		return;
	}

	g_ulRetryTicks = CONNECTION_RESEND_TIME;
	Printf( "Connecting to %s\n", g_AddressServer.ToString() );

	// Reset a bunch of stuff.
	g_LocalBuffer.Clear();
	memset( g_ReceivedPacketBuffer.abData, 0, MAX_UDP_PACKET * 32 );
	for ( ulIdx = 0; ulIdx < PACKET_BUFFER_SIZE; ulIdx++ )
	{
		g_lPacketBeginning[ulIdx] = 0;
		g_lPacketSequence[ulIdx] = -1;
		g_lPacketSize[ulIdx] = 0;
	}

	g_bServerLagging = false;
	g_bClientLagging = false;

	g_bPacketNum = 0;
	g_lCurrentPosition = 0;
	g_lLastParsedSequence = -1;
	g_lHighestReceivedSequence = -1;

	g_lMissingPacketTicks = 0;
	g_lLatestServerGametic = 0; // [CK] Reset this here since we plan on connecting to a new server

	 // Send connection signal to the server.
	NETWORK_WriteByte( &g_LocalBuffer.ByteStream, CLCC_ATTEMPTCONNECTION );
	//NETWORK_WriteString( &g_LocalBuffer.ByteStream, DOTVERSIONSTR );
	// [BB] Stay network compatible.
	NETWORK_WriteString( &g_LocalBuffer.ByteStream, NETGAMEVER_STRING );
	NETWORK_WriteString( &g_LocalBuffer.ByteStream, cl_password );
	NETWORK_WriteByte( &g_LocalBuffer.ByteStream, cl_connect_flags );
	NETWORK_WriteByte( &g_LocalBuffer.ByteStream, cl_hideaccount );
	NETWORK_WriteByte( &g_LocalBuffer.ByteStream, NETGAMEVERSION );
	NETWORK_WriteString( &g_LocalBuffer.ByteStream, g_lumpsAuthenticationChecksum.GetChars() );
}

//*****************************************************************************
//
void CLIENT_AttemptAuthentication( char *pszMapName )
{
	if ( g_ulRetryTicks )
	{
		g_ulRetryTicks--;
		return;
	}

	g_ulRetryTicks = CONNECTION_RESEND_TIME;
	Printf( "Authenticating level...\n" );

	memset( g_lPacketSequence, -1, sizeof(g_lPacketSequence) );
	g_bPacketNum = 0;

	NETWORK_WriteByte( &g_LocalBuffer.ByteStream, CLCC_ATTEMPTAUTHENTICATION );

	// Send a checksum of our verticies, linedefs, sidedefs, and sectors.
	CLIENT_AuthenticateLevel( pszMapName );

	// Make sure all players are gone from the level.
	CLIENT_ClearAllPlayers();
}

//*****************************************************************************
//
void CLIENT_RequestSnapshot( void )
{
	if ( g_ulRetryTicks )
	{
		g_ulRetryTicks--;
		return;
	}

	g_ulRetryTicks = GAMESTATE_RESEND_TIME;
	Printf( "Requesting snapshot...\n" );

	memset( g_lPacketSequence, -1, sizeof (g_lPacketSequence) );
	g_bPacketNum = 0;

	// Send them a message to get data from the server, along with our userinfo.
	NETWORK_WriteByte( &g_LocalBuffer.ByteStream, CLCC_REQUESTSNAPSHOT );
	CLIENTCOMMANDS_SendAllUserInfo();

	// [TP] Send video resolution for ACS scripting support.
	CLIENTCOMMANDS_SetVideoResolution();

	// Make sure all players are gone from the level.
	CLIENT_ClearAllPlayers();

	// [BB] Clear the value of the FriendlyFire bool defined in p_interaction.cpp. This is pretty hackish,
	// but prevents the clients from needing to call P_DamageMobj.
	FriendlyFire = false;
}

//*****************************************************************************
//
bool CLIENT_GetNextPacket( void )
{
	ULONG	ulIdx;

	// Find the next packet in the sequence.
	for ( ulIdx = 0; ulIdx < PACKET_BUFFER_SIZE; ulIdx++ )
	{
		// Found it!
		if ( g_lPacketSequence[ulIdx] == ( g_lLastParsedSequence + 1 ))
		{
			memset( NETWORK_GetNetworkMessageBuffer( )->pbData, -1, MAX_UDP_PACKET );
			memcpy( NETWORK_GetNetworkMessageBuffer( )->pbData, g_ReceivedPacketBuffer.abData + g_lPacketBeginning[ulIdx], g_lPacketSize[ulIdx] );
			NETWORK_GetNetworkMessageBuffer( )->ulCurrentSize = g_lPacketSize[ulIdx];
			NETWORK_GetNetworkMessageBuffer( )->ByteStream.pbStream = NETWORK_GetNetworkMessageBuffer( )->pbData;
			NETWORK_GetNetworkMessageBuffer( )->ByteStream.pbStreamEnd = NETWORK_GetNetworkMessageBuffer( )->ByteStream.pbStream + g_lPacketSize[ulIdx];

			return ( true );
		}
	}

	// We didn't find it!
	return ( false );
}

//*****************************************************************************
//
void CLIENT_GetPackets( void )
{
	LONG lSize;
#ifdef	_DEBUG
	static	ULONG	s_ulEmulatingPacketLoss = 0;
#endif
	while (( lSize = NETWORK_GetPackets( )) > 0 )
	{
		BYTESTREAM_s	*pByteStream;

		pByteStream = &NETWORK_GetNetworkMessageBuffer( )->ByteStream;

		// If we're a client and receiving a message from the server...
		if ( NETWORK_GetState() == NETSTATE_CLIENT
			&& NETWORK_GetFromAddress().Compare( CLIENT_GetServerAddress() ))
		{
			// Statistics.
			CLIENTSTATISTICS_AddToBytesReceived( lSize );

#ifdef	_DEBUG
			// Emulate packet loss for debugging.
			if (( cl_emulatepacketloss ) && gamestate == GS_LEVEL )
			{
				if ( s_ulEmulatingPacketLoss )
				{
					--s_ulEmulatingPacketLoss;
					break;
				}

				// Should activate once every two/three seconds or so.
				if ( M_Random( ) < 4 )
				{
					// This should give a range anywhere from 1 to 128 ticks.
					s_ulEmulatingPacketLoss = ( M_Random( ) / 4 );
				}

				//if (( M_Random( ) < 170 ) && gamestate == GS_LEVEL )
				//	break;
			}
#endif
			// We've gotten a packet! Now figure out what it's saying.
			if ( CLIENT_ReadPacketHeader( pByteStream ))
			{
				CLIENT_ParsePacket( pByteStream, false );
			}
			else
			{
				while ( CLIENT_GetNextPacket( ))
				{
					pByteStream = &NETWORK_GetNetworkMessageBuffer( )->ByteStream;

					// Parse this packet.
					CLIENT_ParsePacket( pByteStream, true );

					// Don't continue parsing if we've exited the network game.
					if ( CLIENT_GetConnectionState( ) == CTS_DISCONNECTED )
						break;
				}
			}

			// We've heard from the server; don't time out!
			CLIENT_SetLastServerTick( gametic );
			CLIENT_SetServerLagging( false );
		}
		else
		{
			const char		*pszPrefix1 = "127.0.0.1";
			const char		*pszPrefix2 = "10.";
			const char		*pszPrefix3 = "172.16.";
			const char		*pszPrefix4 = "192.168.";
			const char		*pszAddressBuf;
			NETADDRESS_s	AddressFrom;
			LONG			lCommand;
			const char		*pszMasterPort;
			// [BB] This conversion potentially does a DNS lookup.
			// There is absolutely no reason to call this at beginning of the while loop above (like done before). 
			NETADDRESS_s MasterAddress ( masterhostname );

			// Allow the user to specify which port the master server is on.
			pszMasterPort = Args->CheckValue( "-masterport" );
			if ( pszMasterPort )
				MasterAddress.usPort = NETWORK_ntohs( atoi( pszMasterPort ));
			else 
				MasterAddress.usPort = NETWORK_ntohs( DEFAULT_MASTER_PORT );


			pszAddressBuf = NETWORK_GetFromAddress().ToString();

			// Skulltag is receiving a message from something on the LAN.
			if (( strncmp( pszAddressBuf, pszPrefix1, 9 ) == 0 ) || 
				( strncmp( pszAddressBuf, pszPrefix2, 3 ) == 0 ) ||
				( strncmp( pszAddressBuf, pszPrefix3, 7 ) == 0 ) ||
				( strncmp( pszAddressBuf, pszPrefix4, 8 ) == 0 ))
			{
				AddressFrom = MasterAddress;

				// Keep the same port as the from address.
				AddressFrom.usPort = NETWORK_GetFromAddress( ).usPort;
			}
			else
				AddressFrom = NETWORK_GetFromAddress( );

			// If we're receiving info from the master server...
			if ( AddressFrom.Compare( MasterAddress ))
			{
				lCommand = NETWORK_ReadLong( pByteStream );
				switch ( lCommand )
				{
				case MSC_BEGINSERVERLISTPART:
					{
						ULONG ulPacketNum = NETWORK_ReadByte( pByteStream );

						// Get the list of servers.
						bool bLastPartReceived = BROWSER_GetServerList( pByteStream );

						// [BB] We received the final part of the server list, now query the servers.
						if ( bLastPartReceived )
						{
							// Now, query all the servers on the list.
							BROWSER_QueryAllServers( );

							// Finally, clear the server list. Server slots will be reactivated when
							// they come in.
							BROWSER_DeactivateAllServers( );
						}
					}
					break;

				case MSC_REQUESTIGNORED:

					Printf( "Refresh request ignored. Please wait 10 seconds before refreshing the list again.\n" );
					break;

				case MSC_IPISBANNED:

					Printf( "You are banned from the master server.\n" );
					break;

				case MSC_WRONGVERSION:

					Printf( "The master server is using a different version of the launcher-master protocol.\n" );
					break;

				default:

					Printf( "Unknown command from master server: %d\n", static_cast<int> (lCommand) );
					break;
				}
			}
			// Perhaps it's a message from a server we're queried.
			else
			{
				lCommand = NETWORK_ReadLong( pByteStream );
				if ( lCommand == SERVER_LAUNCHER_CHALLENGE )
					BROWSER_ParseServerQuery( pByteStream, false );
				else if ( lCommand == SERVER_LAUNCHER_IGNORING )
					Printf( "WARNING! Please wait a full 10 seconds before refreshing the server list.\n" );
				//else
				//	Printf( "Unknown network message from %s.\n", g_AddressFrom.ToString() );
			}
		}
	}
}

//*****************************************************************************
//
void CLIENT_CheckForMissingPackets( void )
{
	LONG	lIdx;
	LONG	lIdx2;

	// We already told the server we're missing packets a little bit ago. No need
	// to do it again.
	if ( g_lMissingPacketTicks > 0 )
	{
		g_lMissingPacketTicks--;
		return;
	}

	if ( g_lLastParsedSequence != g_lHighestReceivedSequence )
	{
		// If we've missed more than PACKET_BUFFER_SIZE packets, there's no hope that we can recover from this
		// since the server only backs up PACKET_BUFFER_SIZE of our packets. We have to end the game.
		if (( g_lHighestReceivedSequence - g_lLastParsedSequence ) >= PACKET_BUFFER_SIZE )
		{
			FString quitMessage;
			quitMessage.Format ( "CLIENT_CheckForMissingPackets: Missing more than %d packets. Unable to recover.", PACKET_BUFFER_SIZE );
			CLIENT_QuitNetworkGame( quitMessage.GetChars() );
			return;
		}

		NETWORK_WriteByte( &g_LocalBuffer.ByteStream, CLC_MISSINGPACKET );

		// Now, go through and figure out what packets we're missing. Request these from the server.
		for ( lIdx = g_lLastParsedSequence + 1; lIdx <= g_lHighestReceivedSequence - 1; lIdx++ )
		{
			for ( lIdx2 = 0; lIdx2 < PACKET_BUFFER_SIZE; lIdx2++ )
			{
				// We've found this packet! No need to tell the server we're missing it.
				if ( g_lPacketSequence[lIdx2] == lIdx )
				{
					if ( debugfile )
						fprintf( debugfile, "We have packet %d.\n", static_cast<int> (lIdx) );

					break;
				}
			}

			// If we didn't find the packet, tell the server we're missing it.
			if ( lIdx2 == PACKET_BUFFER_SIZE )
			{
				if ( debugfile )
					fprintf( debugfile, "Missing packet %d.\n", static_cast<int> (lIdx) );

				NETWORK_WriteLong( &g_LocalBuffer.ByteStream, lIdx );
			}
		}

		// When we're done, write -1 to indicate that we're finished.
		NETWORK_WriteLong( &g_LocalBuffer.ByteStream, -1 );
	}

	// Don't send out a request for the missing packets for another 1/4 second.
	g_lMissingPacketTicks = ( TICRATE / 4 );
}

//*****************************************************************************
//
bool CLIENT_ReadPacketHeader( BYTESTREAM_s *pByteStream )
{
	LONG	lIdx;
	LONG	lCommand;
	LONG	lSequence;

	// Read in the command. Since it's the first one in the packet, it should be
	// SVC_HEADER or SVC_UNRELIABLEPACKET.
	lCommand = NETWORK_ReadByte( pByteStream );

	// If this is an unreliable packet, just break out of here and begin parsing it. There's no
	// need to store it.
	if ( lCommand == SVC_UNRELIABLEPACKET )
		return ( true );
	else
	{
		// Read in the sequence. This is the # of the packet the server has sent us.
		lSequence = NETWORK_ReadLong( pByteStream );
	}
	if ( lCommand != SVC_HEADER )
		Printf( "CLIENT_ReadPacketHeader: WARNING! Expected SVC_HEADER or SVC_UNRELIABLEPACKET!\n" );

	// Check to see if we've already received this packet. If so, skip it.
	for ( lIdx = 0; lIdx < PACKET_BUFFER_SIZE; lIdx++ )
	{
		if ( g_lPacketSequence[lIdx] == lSequence )
			return ( false );
	}

	// The end of the buffer has been reached.
	if (( g_lCurrentPosition + ( NETWORK_GetNetworkMessageBuffer( )->CalcSize())) >= g_ReceivedPacketBuffer.lMaxSize )
		g_lCurrentPosition = 0;

	// Save a bunch of information about this incoming packet.
	g_lPacketBeginning[g_bPacketNum] = g_lCurrentPosition;
	g_lPacketSize[g_bPacketNum] = NETWORK_GetNetworkMessageBuffer( )->CalcSize();
	g_lPacketSequence[g_bPacketNum] = lSequence;

	// Save the received packet.
	memcpy( g_ReceivedPacketBuffer.abData + g_lPacketBeginning[g_bPacketNum], NETWORK_GetNetworkMessageBuffer( )->ByteStream.pbStream, NETWORK_GetNetworkMessageBuffer( )->CalcSize());
	g_lCurrentPosition += NETWORK_GetNetworkMessageBuffer( )->CalcSize();

	if ( lSequence > g_lHighestReceivedSequence )
		g_lHighestReceivedSequence = lSequence;

	g_bPacketNum++;

	return ( false );
}

//*****************************************************************************
//
void CLIENT_ParsePacket( BYTESTREAM_s *pByteStream, bool bSequencedPacket )
{
	LONG	lCommand;

	// We're currently parsing a packet.
	g_bIsParsingPacket = true;

	while ( 1 )
	{  
		// [BB] Processing this command will possibly make use write additional client-side demo
		// commands to our demo stream. We have to make sure that all the commands end up in the
		// demo stream exactly in the order they are processed. We should write the command we
		// are parsing to the demo stream right now, but unfortunately we don't know the size
		// of the command. Thus, we memorize where this command started, process the command,
		// check the new position of the processed stream to determine the command size
		// and then insert the commend into the demo stream before any additional client-side
		// demo commands we wrote while processing the command.

		// [BB] Memorize where the current server command started.
		BYTESTREAM_s commandAsStream;
		commandAsStream.pbStream = pByteStream->pbStream;
		commandAsStream.pbStreamEnd = pByteStream->pbStream;

		// [BB] Memorize the current position in our demo stream.
		CLIENTDEMO_MarkCurrentPosition();
		lCommand = NETWORK_ReadByte( pByteStream );

		// [TP] Reset the bit reading buffer.
		pByteStream->bitBuffer = NULL;
		pByteStream->bitShift = -1;

		// End of message.
		if ( lCommand == -1 )
			break;

		// Option to print commands for debugging purposes.
		if ( cl_showcommands )
			CLIENT_PrintCommand( lCommand );

		// Process this command.
		CLIENT_ProcessCommand( lCommand, pByteStream );

		g_lLastCmd = lCommand;

		// [BB] If we're recording a demo, write the contents of this command at the position
		// our demo was at when we started to process the server command.
		if ( CLIENTDEMO_IsRecording( ) )
		{
			// [BB] Since we just processed the server command, this command ends where we are now.
			commandAsStream.pbStreamEnd = pByteStream->pbStream;
			CLIENTDEMO_InsertPacketAtMarkedPosition( &commandAsStream );
		}

	}

	// All done!
	g_bIsParsingPacket = false;

	if ( debugfile )
	{
		if ( bSequencedPacket )
			fprintf( debugfile, "End parsing packet %d.\n", static_cast<int> (g_lLastParsedSequence + 1) );
		else
			fprintf( debugfile, "End parsing packet.\n" );
	}

	if ( bSequencedPacket )
		g_lLastParsedSequence++;
}

//*****************************************************************************
//
void CLIENT_ProcessCommand( LONG lCommand, BYTESTREAM_s *pByteStream )
{
	char	szString[128];

	if ( CLIENT_ParseServerCommand( static_cast<SVC>( lCommand ), pByteStream ))
		return;

	switch ( lCommand )
	{
	case SVCC_AUTHENTICATE:

		// Print a status message.
		Printf( "Connected!\n" );

		// Read in the map name we now need to authenticate.
		strncpy( g_szMapName, NETWORK_ReadString( pByteStream ), 8 );
		g_szMapName[8] = 0;

		// [CK] Use the server's gametic to start at a reasonable number
		CLIENT_SetLatestServerGametic( NETWORK_ReadLong( pByteStream ) );

		// [BB] If we don't have the map, something went horribly wrong.
		if ( P_CheckIfMapExists( g_szMapName ) == false )
			I_Error ( "SVCC_AUTHENTICATE: Unknown map: %s\n", g_szMapName );

		// The next step is the authenticate the level.
		if ( CLIENTDEMO_IsPlaying( ) == false )
		{
			g_ulRetryTicks = 0;
			CLIENT_SetConnectionState( CTS_ATTEMPTINGAUTHENTICATION );
			CLIENT_AttemptAuthentication( g_szMapName );
		}
		// [BB] Make sure there is no old player information left. This is possible if
		// the demo shows a "map" map change.
		else
			CLIENT_ClearAllPlayers();
		break;
	case SVCC_MAPLOAD:
		{
			// [BB] We are about to change the map, so if we are playing a demo right now
			// and wanted to skip the current map, we are done with it now.
			CLIENTDEMO_SetSkippingToNextMap ( false );

			// [BB] Setting the game mode is necessary to decide whether 3D floors should be spawned or not.
			GAMEMODE_SetCurrentMode ( static_cast<GAMEMODE_e>(NETWORK_ReadByte( pByteStream )) );

			bool	bPlaying;

			// Print a status message.
			Printf( "Level authenticated!\n" );

			// Check to see if we have the map.
			if ( P_CheckIfMapExists( g_szMapName ))
			{
				// Save our demo recording status since G_InitNew resets it.
				bPlaying = CLIENTDEMO_IsPlaying( );

				// Start new level.
				G_InitNew( g_szMapName, false );

				// [BB] We'll receive a full update for the new map from the server.
				g_bFullUpdateIncomplete = true;

				// For right now, the view is not active.
				viewactive = false;

				g_ulLastConsolePlayerUpdateTick = 0;

				// Restore our demo recording status.
				CLIENTDEMO_SetPlaying( bPlaying );
			}
			// [BB] If we don't have the map, something went horribly wrong.
			else
				I_Error ( "SVCC_MAPLOAD: Unknown map: %s\n", g_szMapName );

			// Now that we've loaded the map, request a snapshot from the server.
			if ( CLIENTDEMO_IsPlaying( ) == false )
			{
				g_ulRetryTicks = 0;
				CLIENT_SetConnectionState( CTS_REQUESTINGSNAPSHOT );
				CLIENT_RequestSnapshot( );
			}
		}
		break;
	case SVCC_ERROR:
		{
			FString	szErrorString;
			ULONG	ulErrorCode;

			// Read in the error code.
			ulErrorCode = NETWORK_ReadByte( pByteStream );

			// Build the error string based on the error code.
			switch ( ulErrorCode )
			{
			case NETWORK_ERRORCODE_WRONGPASSWORD:

				szErrorString = "Incorrect password.";
				break;
			case NETWORK_ERRORCODE_WRONGVERSION:

				szErrorString.Format( "Failed to connect. Your client version is different.\nThis server uses version: %s\nPlease check http://www." DOMAIN_NAME "/ for a matching version.", NETWORK_ReadString( pByteStream ) );
				break;
			case NETWORK_ERRORCODE_WRONGPROTOCOLVERSION:

				szErrorString.Format( "Failed to connect. Your protocol version is different.\nThis server uses: %s\nYou use:     %s\nPlease check http://www." DOMAIN_NAME "/ for a matching version.", NETWORK_ReadString( pByteStream ), GetVersionStringRev() );
				break;
			case NETWORK_ERRORCODE_BANNED:

				// [TP] Is this a master ban?
				if ( !!NETWORK_ReadByte( pByteStream ))
				{
					szErrorString = "Couldn't connect. \\cgYou have been banned from " GAMENAME "'s master server!\\c-\n"
						"If you feel this is in error, you may contact the staff at " DISCORD_URL;
				}
				else
				{
					szErrorString = "Couldn't connect. \\cgYou have been banned from this server!\\c-";

					// [RC] Read the reason for this ban.
					const char		*pszBanReason = NETWORK_ReadString( pByteStream );
					if ( strlen( pszBanReason ))
						szErrorString = szErrorString + "\nReason for ban: " + pszBanReason;

					// [RC] Read the expiration date for this ban.
					time_t			tExpiration = (time_t) NETWORK_ReadLong( pByteStream );
					if ( tExpiration > 0 )
					{
						struct tm	*pTimeInfo;
						char		szDate[32];

						pTimeInfo = localtime( &tExpiration );
						strftime( szDate, 32, "%m/%d/%Y %H:%M", pTimeInfo);
						szErrorString = szErrorString + "\nYour ban expires on: " + szDate + " (server time)";
					}

					// [TP] Read in contact information, if any.
					FString contact = NETWORK_ReadString( pByteStream );
					if ( contact.IsNotEmpty() )
					{
						szErrorString.AppendFormat( "\nIf you feel this is in error, you may contact the server "
							"host at: %s", contact.GetChars() );
					}
					else
						szErrorString += "\nThis server has not provided any contact information.";
				}
				break;
			case NETWORK_ERRORCODE_SERVERISFULL:

				szErrorString = "Server is full.";
				break;
			case NETWORK_ERRORCODE_AUTHENTICATIONFAILED:
			case NETWORK_ERRORCODE_PROTECTED_LUMP_AUTHENTICATIONFAILED:
				{
					std::pair<FString, FString> MainPWAD;
					MainPWAD.first = NETWORK_ReadString( pByteStream );
					MainPWAD.second = NETWORK_ReadString( pByteStream );

					std::list<std::pair<FString, FString> > serverPWADs;
					const int numServerPWADs = NETWORK_ReadByte( pByteStream );
					for ( int i = 0; i < numServerPWADs; ++i )
					{
						std::pair<FString, FString> pwad;
						pwad.first = NETWORK_ReadString( pByteStream );
						pwad.second = NETWORK_ReadString( pByteStream );
						serverPWADs.push_back ( pwad );
					}

					szErrorString.Format( "%s authentication failed.\nPlease make sure you are using the exact same WAD(s) as the server, and try again.", ( ulErrorCode == NETWORK_ERRORCODE_PROTECTED_LUMP_AUTHENTICATIONFAILED ) ? "Protected lump" : "Level" );
					
					if ( stricmp( NETWORK_GetMainPWAD().checksum, MainPWAD.second ) != 0 )
					{
						Printf ( "- Your \"%s\" is different from the server: \"%s\" vs \"%s\"\n", NETWORK_GetMainPWAD().name.GetChars(), NETWORK_GetMainPWAD().checksum.GetChars(), MainPWAD.second.GetChars() );
					}
					
					TArray<NetworkPWAD> LocalPWADs = NETWORK_GetPWADList();
					for ( unsigned int i = 0; i < LocalPWADs.Size(); ++i )
					{
						for ( std::list<std::pair<FString, FString> >::iterator j = serverPWADs.begin(); j != serverPWADs.end(); ++j )
						{
							if ( stricmp( LocalPWADs[i].name, j->first ) == 0 )
							{
								if ( stricmp(LocalPWADs[i].checksum, j->second ) != 0 )
								{
									Printf ( "- Your \"%s\" is different from the server: \"%s\" vs \"%s\"\n", LocalPWADs[i].name.GetChars(), LocalPWADs[i].checksum.GetChars(), j->second.GetChars() );
								}

								serverPWADs.remove( *j );
								goto cnt; // continue first loop
							}
						}

						Printf ( "- You are using \"%s\", but the server doesn't: \"%s\"\n", LocalPWADs[i].name.GetChars(), LocalPWADs[i].checksum.GetChars() );
						cnt: ;
					}

					for ( std::list<std::pair<FString, FString> >::iterator j = serverPWADs.begin(); j != serverPWADs.end(); ++j )
					{
						Printf ( "- The server is using the \"%s\", but you don't: \"%s\"\n", j->first.GetChars(), j->second.GetChars() );
					}

					break;
				}
			case NETWORK_ERRORCODE_TOOMANYCONNECTIONSFROMIP:

				szErrorString = "Too many connections from your IP.";
				break;
			case NETWORK_ERRORCODE_USERINFOREJECTED:

				szErrorString = "The server rejected the userinfo.";
				break;
			default:

				szErrorString.Format( "Unknown error code: %d!\n\nYour version may be different. Please check http://www." DOMAIN_NAME "/ for updates.", static_cast<unsigned int> (ulErrorCode) );
				break;
			}

			CLIENT_QuitNetworkGame( szErrorString );
		}
		return;

	case SVC_EXTENDEDCOMMAND:
		{
			const LONG lExtCommand = NETWORK_ReadByte( pByteStream );

			if ( cl_showcommands )
				Printf( "%s\n", GetStringSVC2 ( static_cast<SVC2> ( lExtCommand ) ) );

			if ( CLIENT_ParseExtendedServerCommand( static_cast<SVC2>( lExtCommand ), pByteStream ))
				break;

			switch ( lExtCommand )
			{

			// [Dusk]

			case SVC2_SYNCPATHFOLLOWER:
				{
					APathFollower::InitFromStream ( pByteStream );
				}
				break;

			case SVC2_SRP_USER_START_AUTHENTICATION:
			case SVC2_SRP_USER_PROCESS_CHALLENGE:
			case SVC2_SRP_USER_VERIFY_SESSION:
				CLIENT_ProcessSRPServerCommand ( lExtCommand, pByteStream );
				break;

			// [EP]

			case SVC2_SHOOTDECAL:
				{
					FName decalName = NETWORK_ReadName( pByteStream );
					AActor* actor = CLIENT_FindThingByNetID( NETWORK_ReadShort( pByteStream ));
					fixed_t z = NETWORK_ReadShort( pByteStream ) << FRACBITS;
					angle_t angle = NETWORK_ReadShort( pByteStream ) << FRACBITS;
					fixed_t tracedist = NETWORK_ReadLong( pByteStream );
					bool permanent = !!NETWORK_ReadByte( pByteStream );
					const FDecalTemplate* tpl = DecalLibrary.GetDecalByName( decalName );

					if ( actor && tpl )
						ShootDecal( tpl, actor, actor->Sector, actor->x, actor->y, z, angle, tracedist, permanent );
				}
				break;

			case SVC2_REPORTLUMPS:
				{
					CLIENTCOMMANDS_ReportLumps( );
				}
				break;
				
			case SVC2_ADDTOMAPROTATION:
				{
					const char *pszMapName = NETWORK_ReadString( pByteStream );
					int position = NETWORK_ReadByte( pByteStream );

					// [AK] Add this map to the rotation.
					MAPROTATION_AddMap( pszMapName, true, position );
				}
				break;

			case SVC2_DELFROMMAPROTATION:
				if ( NETWORK_ReadByte( pByteStream ) )
				{
					// [AK] Clear all maps from the rotation.
					MAPROTATION_Construct();
				}
				else
				{
					// [AK] Remove the map with the given name from the rotation.
					MAPROTATION_DelMap( NETWORK_ReadString( pByteStream ), true );
				}
				break;

			default:
				sprintf( szString, "CLIENT_ParsePacket: Illegible server message: %d\nLast command: %d\n", static_cast<int> (lExtCommand), static_cast<int> (g_lLastCmd) );
				CLIENT_QuitNetworkGame( szString );
				return;
			}
		}
		break;

	default:

#ifdef _DEBUG
		sprintf( szString, "CLIENT_ParsePacket: Illegible server message: %d\nLast command: (%s)\n", static_cast<int> (lCommand), GetStringSVC ( static_cast<SVC> ( g_lLastCmd ) ) );
#else
		sprintf( szString, "CLIENT_ParsePacket: Illegible server message: %d\nLast command: %d\n", static_cast<int> (lCommand), static_cast<int> (g_lLastCmd) );
#endif
		CLIENT_QuitNetworkGame( szString );
		return;
	}
}

//*****************************************************************************
//
void CLIENT_PrintCommand( LONG lCommand )
{
	const char	*pszString;

	if ( lCommand < 0 )
		return;

	if ( lCommand < NUM_SERVERCONNECT_COMMANDS )
	{
		switch ( lCommand )
		{
		case SVCC_AUTHENTICATE:

			pszString = "SVCC_AUTHENTICATE";
			break;
		case SVCC_MAPLOAD:

			pszString = "SVCC_MAPLOAD";
			break;
		case SVCC_ERROR:

			pszString = "SVCC_ERROR";
			break;
		}
	}
	else
	{
		if (( cl_showcommands >= 2 ) && ( lCommand == SVC_MOVELOCALPLAYER ))
			return;
		if (( cl_showcommands >= 3 ) && ( lCommand == SVC_MOVEPLAYER ))
			return;
		if (( cl_showcommands >= 5 ) && ( lCommand == SVC_UPDATEPLAYERPING ))
			return;
		if (( cl_showcommands >= 6 ) && ( lCommand == SVC_PING ))
			return;

		if ( ( lCommand - NUM_SERVERCONNECT_COMMANDS ) < 0 )
			return;

		pszString = GetStringSVC ( static_cast<SVC> ( lCommand ) );
	}

	Printf( "%s\n", pszString );

	if ( debugfile )
		fprintf( debugfile, "%s\n", pszString );
}

//*****************************************************************************
//
void CLIENT_QuitNetworkGame( const char *pszString )
{
	if ( pszString )
		Printf( "%s\n", pszString );

	// Set the consoleplayer back to 0 and keep our userinfo to avoid desync if we ever reconnect.
	if ( consoleplayer != 0 )
	{
		players[0].userinfo.TransferFrom ( players[consoleplayer].userinfo );
		consoleplayer = 0;
	}

	// Clear out the existing players.
	CLIENT_ClearAllPlayers();

	// If we're connected in any way, send a disconnect signal.
	// [BB] But only if we are actually a client. Otherwise we can't send the signal anywhere.
	if ( ( g_ConnectionState != CTS_DISCONNECTED ) && ( NETWORK_GetState() == NETSTATE_CLIENT ) )
	{
		NETWORK_WriteByte( &g_LocalBuffer.ByteStream, CLC_QUIT );

		// Send the server our packet.
		CLIENT_SendServerPacket( );
	}

	// Clear out our copy of the server address.
	memset( &g_AddressServer, 0, sizeof( g_AddressServer ));
	CLIENT_SetConnectionState( CTS_DISCONNECTED );

	// Go back to the full console.
	// [BB] This is what the CCMD endgame is doing and thus should be
	// enough to handle all non-network related things.
	gameaction = ga_fullconsole;

	// [BB] If we arrive here, because a client used the map CCMD, the
	// gameaction will be ignored. So we manually stop the automap.
	if ( automapactive )
		AM_Stop ();

	g_lLastParsedSequence = -1;
	g_lHighestReceivedSequence = -1;

	g_lMissingPacketTicks = 0;
	
	// [AK] Clear the map rotation when we leave the server.
	MAPROTATION_Construct( );

	// Set the network state back to single player.
	NETWORK_SetState( NETSTATE_SINGLE );

	// [BB] Reset gravity to its default, discarding the setting the server used.
	// Although this is not done for any other sv_* CVAR, it is necessary here,
	// since the server sent us level.gravity instead of its own sv_gravity value,
	// see SERVERCOMMANDS_SetGameModeLimits.
	sv_gravity.ResetToDefault();

	// If we're recording a demo, then finish it!
	if ( CLIENTDEMO_IsRecording( ))
		CLIENTDEMO_FinishRecording( );

	// [BB] Also, if we're playing a demo, finish it
	if ( CLIENTDEMO_IsPlaying( ))
		CLIENTDEMO_FinishPlaying( );

	// [BB] If the server assigned a different name to us, reset that now.
	if ( strcmp ( static_cast<const char*>(name), players[consoleplayer].userinfo.GetName() ) != 0 )
		D_SetupUserInfo ();
}

//*****************************************************************************
//
void CLIENT_SendCmd( void )
{		
	if (( gametic < 1 ) ||
		( players[consoleplayer].mo == NULL ) ||
		(( gamestate != GS_LEVEL ) && ( gamestate != GS_INTERMISSION )))
	{
		return;
	}

	// If we're at intermission, and toggling our "ready to go" status, tell the server.
	if ( gamestate == GS_INTERMISSION )
	{
		if (( players[consoleplayer].cmd.ucmd.buttons ^ players[consoleplayer].oldbuttons ) &&
			(( players[consoleplayer].cmd.ucmd.buttons & players[consoleplayer].oldbuttons ) == players[consoleplayer].oldbuttons ))
		{
			CLIENTCOMMANDS_ReadyToGoOn( );
		}

		players[consoleplayer].oldbuttons = players[consoleplayer].cmd.ucmd.buttons;
		return;
	}

	// Don't send movement information if we're spectating!
	if ( players[consoleplayer].bSpectating )
	{
		if (( gametic % ( TICRATE * 2 )) == 0 )
		{
			// Send the gametic.
			CLIENTCOMMANDS_SpectateInfo( );
		}

		return;
	}

	CLIENT_SendServerPacket();
	// Send the move header and the gametic.
	for (int i = 0; i <= cl_packetdup; i++)
	{
		CLIENTCOMMANDS_ClientMove();
		CLIENT_SendServerPacket();
	}
}

//*****************************************************************************
//
void CLIENT_WaitForServer( void )
{
	if ( players[consoleplayer].bSpectating )
	{
		// If the last time we heard from the server exceeds five seconds, we're lagging!
		if ((( gametic - CLIENTDEMO_GetGameticOffset( ) - g_ulLastServerTick ) >= ( TICRATE * 5 )) &&
			(( gametic - CLIENTDEMO_GetGameticOffset( )) > ( TICRATE * 5 )))
		{
			g_bServerLagging = true;
		}
	}
	else
	{
		// If the last time we heard from the server exceeds one second, we're lagging!
		if ((( gametic - CLIENTDEMO_GetGameticOffset( ) - g_ulLastServerTick ) >= TICRATE ) &&
			(( gametic - CLIENTDEMO_GetGameticOffset( )) > TICRATE ))
		{
			g_bServerLagging = true;
		}
	}
}

//*****************************************************************************
//
void CLIENT_AuthenticateLevel( const char *pszMapName )
{
	FString		Checksum;
	MapData		*pMap;

	// [BB] Check if the wads contain the map at all. If not, don't send any checksums.
	pMap = P_OpenMapData( pszMapName, false );

	if ( pMap == NULL )
	{
		Printf( "CLIENT_AuthenticateLevel: Map %s not found!\n", pszMapName );
		return;
	}

	// [Dusk] Include a byte to check if this is an UDMF or a non-UDMF map.
	NETWORK_WriteByte( &g_LocalBuffer.ByteStream, pMap->isText );

	if ( pMap->isText )
	{
		// [Dusk] If this is an UDMF map, send the TEXTMAP checksum.
		NETWORK_GenerateMapLumpMD5Hash( pMap, ML_TEXTMAP, Checksum );
		NETWORK_WriteString( &g_LocalBuffer.ByteStream, Checksum.GetChars() );
	}
	else
	{
		// Generate and send checksums for the map lumps.
		const int ids[4] = { ML_VERTEXES, ML_LINEDEFS, ML_SIDEDEFS, ML_SECTORS };
		for( ULONG i = 0; i < 4; ++i )
		{
			NETWORK_GenerateMapLumpMD5Hash( pMap, ids[i], Checksum );
			NETWORK_WriteString( &g_LocalBuffer.ByteStream, Checksum.GetChars() );
		}
	}

	if ( pMap->HasBehavior )
		NETWORK_GenerateMapLumpMD5Hash( pMap, ML_BEHAVIOR, Checksum );
	else
		Checksum = "";
	NETWORK_WriteString( &g_LocalBuffer.ByteStream, Checksum.GetChars() );

	// Finally, free the map.
	delete ( pMap );
}

//*****************************************************************************
//
AActor *CLIENT_SpawnThing( const PClass *pType, fixed_t X, fixed_t Y, fixed_t Z, LONG lNetID, BYTE spawnFlags, int randomSeed )
{
	AActor			*pActor;

	// Only spawn actors if we're actually in a level.
	if ( gamestate != GS_LEVEL )
		return ( NULL );

	if ( pType == NULL )
		return NULL;

	// Potentially print the name, position, and network ID of the thing spawning.
	if ( cl_showspawnnames )
		Printf( "Name: %s: (%d, %d, %d), %d\n", pType->TypeName.GetChars( ), X >> FRACBITS, Y >> FRACBITS, Z >> FRACBITS, static_cast<int> (lNetID) );

	// If there's already an actor with the network ID of the thing we're spawning, kill it!
	pActor = CLIENT_FindThingByNetID( lNetID );
	if ( pActor )
	{
#ifdef	_DEBUG
		if ( pActor == players[consoleplayer].mo )
		{
			Printf( "CLIENT_SpawnThing: WARNING! Tried to delete console player's body! lNetID = %ld\n", lNetID );
			return NULL;
		}
#endif
		pActor->Destroy( );
	}

	// Handle sprite/particle display options.
	if ( stricmp( pType->TypeName.GetChars( ), "blood" ) == 0 )
	{
		if ( cl_bloodtype >= 1 )
		{
			angle_t	Angle;

			Angle = ( M_Random( ) - 128 ) << 24;
			P_DrawSplash2( 32, X, Y, Z, Angle, 2, 0 );
		}

		// Just do particles.
		if ( cl_bloodtype == 2 )
			return ( NULL );
	}

	if ( stricmp( pType->TypeName.GetChars( ), "BulletPuff" ) == 0 )
	{
		if ( cl_pufftype )
		{
			angle_t	Angle;

			Angle = ( M_Random( ) - 128 ) << 24;
			P_DrawSplash2( 32, X, Y, Z, Angle, 1, 1 );
			return ( NULL );
		}
	}

	bool levelThing = ((spawnFlags & SPAWNFLAG_LEVELTHING) != 0);

	// Now that all checks have been done, spawn the actor.
	pActor = AActor::StaticSpawn( pType, X, Y, Z, NO_REPLACE, levelThing );
	if ( pActor )
	{
		// [BB] Calling StaticSpawn with "levelThing == true" will prevent
		// BeginPlay from being called on pActor, so we have to do this manually.
		if ( levelThing )
			pActor->BeginPlay ();

		pActor->lNetID = lNetID;
		g_NetIDList.useID ( lNetID, pActor );
		
		if ( ( lNetID > 0 ) && ( (int) ( lNetID / g_NetIDList.MAX_NETID_FOR_PLAYER ) == consoleplayer ) && ( lNetID >= players[consoleplayer].firstFreeNetId || lNetID == 1 ) ) {
			int oldfreenetid = players[consoleplayer].firstFreeNetId;
			players[consoleplayer].firstFreeNetId = lNetID % g_NetIDList.MAX_NETID_FOR_PLAYER + 1;
			if (players[consoleplayer].firstFreeNetId >= g_NetIDList.MAX_NETID_FOR_PLAYER * (consoleplayer + 1)) {
				players[consoleplayer].firstFreeNetId = 1;
			}
		}

		pActor->SpawnPoint[0] = X;
		pActor->SpawnPoint[1] = Y;
		pActor->SpawnPoint[2] = Z;

		// [Zalewa] Bullet puffs don't do 3D floor checks server-side.
		if (!(spawnFlags & SPAWNFLAG_PUFF))
		{
			// [BB] The "Spawn" call apparently doesn't properly take into account 3D floors,
			// so we have to explicitly adjust the floor again.
			P_FindFloorCeiling ( pActor, FFCF_SAMESECTOR | FFCF_ONLY3DFLOORS | FFCF_3DRESTRICT );
		}

		// [BB] The current position of the actor comes straight from the server, so it's safe
		// to assume that it's correct and thus a valid value for the last updated position.
		pActor->lastX = X;
		pActor->lastY = Y;
		pActor->lastZ = Z;

		// Whenever blood spawns, its velz is always 2 * FRACUNIT.
		if ( stricmp( pType->TypeName.GetChars( ), "blood" ) == 0 )
			pActor->velz = FRACUNIT*2;

		// Allow for client-side body removal in invasion mode.
		if ( invasion )
			pActor->ulInvasionWave = INVASION_GetCurrentWave( );

		if ( randomSeed >= 0 )
			pActor->SetRandomSeed( randomSeed );
	}
	else
		CLIENT_PrintWarning( "CLIENT_SpawnThing: Failed to spawn actor %s with id %ld\n", pType->TypeName.GetChars( ), lNetID );

	return ( pActor );
}

//*****************************************************************************
//
void CLIENT_SpawnMissile( const PClass *pType, fixed_t X, fixed_t Y, fixed_t Z, fixed_t VelX, fixed_t VelY, fixed_t VelZ, LONG lNetID, LONG lTargetNetID, int randomSeed )
{
	AActor				*pActor;

	// Only spawn missiles if we're actually in a level.
	if ( gamestate != GS_LEVEL )
		return;

	if ( pType == NULL )
		return;

	// Potentially print the name, position, and network ID of the thing spawning.
	if ( cl_showspawnnames )
		Printf( "Name: %s: (%d, %d, %d), %d\n", pType->TypeName.GetChars( ), X >> FRACBITS, Y >> FRACBITS, Z >> FRACBITS, static_cast<int> (lNetID) );

	// If there's already an actor with the network ID of the thing we're spawning, kill it!
	pActor = CLIENT_FindThingByNetID( lNetID );
	if ( pActor )
	{
		pActor->Destroy( );
	}

	// Now that all checks have been done, spawn the actor.
	pActor = Spawn( pType, X, Y, Z, NO_REPLACE );
	if ( pActor == NULL )
	{
		CLIENT_PrintWarning( "CLIENT_SpawnMissile: Failed to spawn missile: %ld\n", lNetID );
		return;
	}

	// Set the thing's velocity.
	pActor->velx = VelX;
	pActor->vely = VelY;
	pActor->velz = VelZ;

	// Derive the thing's angle from its velocity.
	pActor->angle = R_PointToAngle2( 0, 0, VelX, VelY );

	pActor->lNetID = lNetID;
	g_NetIDList.useID ( lNetID, pActor );
	
	if ( ( lNetID > 0 ) && ( (int) ( lNetID / g_NetIDList.MAX_NETID_FOR_PLAYER ) == consoleplayer ) && ( lNetID >= players[consoleplayer].firstFreeNetId || lNetID == 1 ) ) {
		int oldfreenetid = players[consoleplayer].firstFreeNetId;
		players[consoleplayer].firstFreeNetId = lNetID % g_NetIDList.MAX_NETID_FOR_PLAYER + 1;
		if (players[consoleplayer].firstFreeNetId >= g_NetIDList.MAX_NETID_FOR_PLAYER * (consoleplayer + 1)) {
			players[consoleplayer].firstFreeNetId = 1;
		}
	}

	pActor->target = CLIENT_FindThingByNetID( lTargetNetID );

	if ( randomSeed >= 0 )
		pActor->SetRandomSeed( randomSeed );

	// Play the seesound if this missile has one.
	if ( pActor->SeeSound )
		if ( !( zacompatflags & ZACOMPATF_PREDICT_FUNCTIONS )
			|| !pActor->target
			|| pActor->target->player - players != consoleplayer )
			S_Sound( pActor, CHAN_VOICE, pActor->SeeSound, 1, ATTN_NORM );
}

//*****************************************************************************
//
void CLIENT_AdjustPredictionToServerSideConsolePlayerMove( fixed_t X, fixed_t Y, fixed_t Z )
{
	players[consoleplayer].ServerXYZ[0] = X;
	players[consoleplayer].ServerXYZ[1] = Y;
	players[consoleplayer].ServerXYZ[2] = Z;
	CLIENT_PREDICT_PlayerTeleported( );
}

//*****************************************************************************
//
// [WS] These are specials checks for client-side actors of whether or not they are
// allowed to move through other actors/walls/ceilings/floors.
bool CLIENT_CanClipMovement( AActor *pActor )
{
	// [WS] If it's not a client, of course clip its movement.
	if ( NETWORK_InClientMode() == false )
		return true;

	// [Dusk] Clients clip missiles the server has no control over.
	if ( NETWORK_IsActorClientHandled ( pActor ) )
		return true;

	// [BB] The client needs to clip its own movement for the prediction.
	if ( pActor->player == &players[consoleplayer] )
		return true;

	// [WS] Non-bouncing client missiles do not get their movement clipped.
	if ( pActor->flags & MF_MISSILE && !pActor->BounceFlags )
		return false;

	return true;
}

//*****************************************************************************
//
// :(. This is needed so that the MOTD can be printed in the color the user wishes to print
// mid-screen messages in.
extern	int PrintColors[7];
void CLIENT_DisplayMOTD( void )
{
	FString	ConsoleString;

	if ( g_MOTD.Len( ) <= 0 )
		return;

	// Add pretty colors/formatting!
	V_ColorizeString( g_MOTD );

	ConsoleString.AppendFormat( TEXTCOLOR_RED
		"\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36"
		"\36\36\36\36\36\36\36\36\36\36\36\36\37" TEXTCOLOR_TAN
		"\n\n%s\n" TEXTCOLOR_RED
		"\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36"
		"\36\36\36\36\36\36\36\36\36\36\36\36\37" TEXTCOLOR_NORMAL "\n\n" ,
		g_MOTD.GetChars() );

	// Add this message to the console window.
	AddToConsole( -1, ConsoleString );

	// We cannot create the message if there's no status bar to attach it to.
	if ( StatusBar == NULL )
		return;

	StatusBar->AttachMessage( new DHUDMessageFadeOut( SmallFont, g_MOTD,
		1.5f,
		0.375f,
		0,
		0,
		(EColorRange)PrintColors[5],
		cl_motdtime,
		0.35f ), MAKE_ID('M','O','T','D') );
}

//*****************************************************************************
//
AActor *CLIENT_FindThingByNetID( LONG lNetID )
{
    return ( g_NetIDList.findPointerByID ( lNetID ) );
}

//*****************************************************************************
//
void CLIENT_RestoreSpecialPosition( AActor *pActor )
{
	CALL_ACTION(A_RestoreSpecialPosition, pActor);
}

//*****************************************************************************
//
void CLIENT_RestoreSpecialDoomThing( AActor *pActor, bool bFog )
{
	// Change an actor that's been put into a hidden for respawning back to its spawn state
	// (respawning it). The reason we need to duplicate this function (this same code basically
	// exists in A_RestoreSpecialDoomThing( )) is because normally, when items are touched and
	// we want to respawn them, we put them into a hidden state, with the first frame being 1050
	// ticks long. Once that frame is finished, it calls A_ResotoreSpecialDoomThing and Position.
	// We don't want that to happen on the client end, because we don't want items to respawn on
	// the client end. However, there really is no good way to do that. If we don't call the
	// function while in client mode, items won't respawn at all. We could do it with external
	// variables, but that's just not clean. We could just not tick actors, but we definitely 
	// don't want that. Anyway, the solution we're going to use here is to break out of restore
	// special doomthing( ) if we're in client mode to avoid client-side respawning, and just
	// call a different function all together when the server wants us to respawn an item that
	// does basically the same thing.

	// Make the item visible and touchable again.
	pActor->renderflags &= ~RF_INVISIBLE;
	pActor->flags |= MF_SPECIAL;

	if (( pActor->GetDefault( )->flags & MF_NOGRAVITY ) == false )
		pActor->flags &= ~MF_NOGRAVITY;

	// Should this item even respawn?
	// [BB] You can call DoRespawn only on AInventory and descendants.
	if ( !(pActor->IsKindOf( RUNTIME_CLASS( AInventory ))) )
		return;
	if ( static_cast<AInventory *>( pActor )->DoRespawn( ))
	{
		// Put the actor back to its spawn state.
		pActor->SetState( pActor->SpawnState );

		// Play the spawn sound, and make a little fog.
		if ( bFog )
		{
			S_Sound( pActor, CHAN_VOICE, "misc/spawn", 1, ATTN_IDLE );
			Spawn( "ItemFog", pActor->x, pActor->y, pActor->z, ALLOW_REPLACE );
		}
	}
}

//*****************************************************************************
//
AInventory *CLIENT_FindPlayerInventory( ULONG ulPlayer, const PClass *pType )
{
	AInventory		*pInventory;

	// Try to find this object within the player's personal inventory.
	pInventory = players[ulPlayer].mo->FindInventory( pType );

	// If the player doesn't have this type, give it to him.
	if ( pInventory == NULL )
		pInventory = players[ulPlayer].mo->GiveInventoryType( pType );

	// If he still doesn't have the object after trying to give it to him... then YIKES!
	if ( pInventory == NULL )
		CLIENT_PrintWarning( "CLIENT_FindPlayerInventory: Failed to give inventory type, %s!\n", pType->TypeName.GetChars( ));

	return ( pInventory );
}

//*****************************************************************************
//
AInventory *CLIENT_FindPlayerInventory( ULONG ulPlayer, const char *pszName )
{
	const PClass	*pType;

	pType = PClass::FindClass( pszName );
	if ( pType == NULL )
		return ( NULL );

	return ( CLIENT_FindPlayerInventory( ulPlayer, pType ));
}

//*****************************************************************************
//
/* [BB] Causes major problems with Archviles at the moment, therefore deactivated.
void CLIENT_RemoveMonsterCorpses( void )
{
	AActor	*pActor;
	ULONG	ulMonsterCorpseCount;

	// Allow infinite corpses.
	if ( cl_maxmonstercorpses == 0 )
		return;

	// Initialize the number of corpses.
	ulMonsterCorpseCount = 0;

	TThinkerIterator<AActor> iterator;
	while (( pActor = iterator.Next( )))
	{
		if (( pActor->IsKindOf( RUNTIME_CLASS( APlayerPawn )) == true ) ||
			!( pActor->flags & MF_CORPSE ))
		{
			continue;
		}

		ulMonsterCorpseCount++;
		if ( ulMonsterCorpseCount >= cl_maxmonstercorpses )
		{
			pActor->Destroy( );
		}
	}
}
*/

//*****************************************************************************
//
sector_t *CLIENT_FindSectorByID( ULONG ulID )
{
	if ( ulID >= static_cast<ULONG>(numsectors) )
		return ( NULL );

	return ( &sectors[ulID] );
}

//*****************************************************************************
//
line_t *CLIENT_FindLineByID( ULONG lineID )
{
	if ( lineID >= static_cast<ULONG>( numlines ) )
		return ( NULL );

	return ( &lines[lineID] );
}

//*****************************************************************************
//
side_t *CLIENT_FindSideByID( ULONG sideID )
{
	if ( sideID >= static_cast<ULONG>( numsides ) )
		return ( NULL );

	return ( &sides[sideID] );
}

//*****************************************************************************
//
bool CLIENT_IsParsingPacket( void )
{
	return ( g_bIsParsingPacket );
}

//*****************************************************************************
//
void CLIENT_ResetConsolePlayerCamera( void )
{
	players[consoleplayer].camera = players[consoleplayer].mo;
	if ( players[consoleplayer].camera != NULL )
		S_UpdateSounds( players[consoleplayer].camera );
	if ( StatusBar != NULL )
		StatusBar->AttachToPlayer( &players[consoleplayer] );
}

//*****************************************************************************
//
void PLAYER_ResetPlayerData( player_t *pPlayer )
{
	pPlayer->mo = NULL;
	pPlayer->playerstate = 0;
	pPlayer->cls = 0;
	pPlayer->DesiredFOV = 0;
	pPlayer->FOV = 0;
	pPlayer->viewz = 0;
	pPlayer->viewheight = 0;
	pPlayer->deltaviewheight = 0;
	pPlayer->bob = 0;
	pPlayer->velx = 0;
	pPlayer->vely = 0;
	pPlayer->centering = 0;
	pPlayer->turnticks = 0;
	pPlayer->oldbuttons = 0;
	pPlayer->attackdown = 0;
	pPlayer->health = 0;
	pPlayer->inventorytics = 0;
	pPlayer->CurrentPlayerClass = 0;
	pPlayer->backpack = 0;
	pPlayer->fragcount = 0;
	pPlayer->ReadyWeapon = 0;
	pPlayer->PendingWeapon = 0;
	pPlayer->cheats = 0;
	pPlayer->refire = 0;
	pPlayer->killcount = 0;
	pPlayer->itemcount = 0;
	pPlayer->secretcount = 0;
	pPlayer->damagecount = 0;
	pPlayer->bonuscount = 0;
	pPlayer->hazardcount = 0;
	pPlayer->poisoncount = 0;
	pPlayer->poisoner = 0;
	pPlayer->attacker = 0;
	pPlayer->extralight = 0;
	pPlayer->morphTics = 0;
	pPlayer->PremorphWeapon = 0;
	pPlayer->chickenPeck = 0;
	pPlayer->respawn_time = 0;
	pPlayer->force_respawn_time = 0;
	pPlayer->camera = 0;
	pPlayer->air_finished = 0;
	pPlayer->BlendR = 0;
	pPlayer->BlendG = 0;
	pPlayer->BlendB = 0;
	pPlayer->BlendA = 0;
// 		pPlayer->LogText(),
	pPlayer->crouching = 0;
	pPlayer->crouchdir = 0;
	pPlayer->crouchfactor = 0;
	pPlayer->crouchoffset = 0;
	pPlayer->crouchviewdelta = 0;
	pPlayer->bOnTeam = 0;
	pPlayer->ulTeam = 0;
	pPlayer->lPointCount = 0;
	pPlayer->ulDeathCount = 0;
	PLAYER_ResetSpecialCounters ( pPlayer );
	pPlayer->bChatting = 0;
	pPlayer->bInConsole = 0;
	pPlayer->bInMenu = 0;
	pPlayer->bSpectating = 0;
	pPlayer->bIgnoreChat = 0;
	pPlayer->lIgnoreChatTicks = -1;
	pPlayer->bDeadSpectator = 0;
	pPlayer->ulLivesLeft = 0;
	pPlayer->bStruckPlayer = 0;
	pPlayer->pIcon = 0;
	pPlayer->lMaxHealthBonus = 0;
	pPlayer->ulWins = 0;
	pPlayer->pSkullBot = 0;
	pPlayer->bIsBot = 0;
	pPlayer->ulPing = 0;
	pPlayer->bReadyToGoOn = 0;
	pPlayer->bSpawnOkay = 0;
	pPlayer->SpawnX = 0;
	pPlayer->SpawnY = 0;
	pPlayer->SpawnAngle = 0;
	pPlayer->OldPendingWeapon = 0;
	pPlayer->bLagging = 0;
	pPlayer->bSpawnTelefragged = 0;
	pPlayer->ulTime = 0;

	memset( &pPlayer->cmd, 0, sizeof( pPlayer->cmd ));
	if (( pPlayer - players ) != consoleplayer )
	{
		pPlayer->userinfo.Reset();
	}
	memset( pPlayer->psprites, 0, sizeof( pPlayer->psprites ));

	memset( &pPlayer->ulMedalCount, 0, sizeof( ULONG ) * NUM_MEDALS );
	memset( &pPlayer->ServerXYZ, 0, sizeof( fixed_t ) * 3 );
	memset( &pPlayer->ServerXYZVel, 0, sizeof( fixed_t ) * 3 );
}

//*****************************************************************************
//
LONG CLIENT_AdjustDoorDirection( LONG lDirection )
{
	lDirection -= 1;

	// Not a valid door direction.
	if (( lDirection < -1 ) || ( lDirection > 2 ))
		return ( INT_MAX );

	return ( lDirection );
}

//*****************************************************************************
//
LONG CLIENT_AdjustFloorDirection( LONG lDirection )
{
	lDirection -= 1;

	// Not a valid door direction.
	if (( lDirection < -1 ) || ( lDirection > 1 ))
		return ( INT_MAX );

	return ( lDirection );
}

//*****************************************************************************
//
LONG CLIENT_AdjustCeilingDirection( LONG lDirection )
{
	lDirection -= 1;

	// Not a valid door direction.
	if (( lDirection < -1 ) || ( lDirection > 1 ))
		return ( INT_MAX );

	return ( lDirection );
}

//*****************************************************************************
//
LONG CLIENT_AdjustElevatorDirection( LONG lDirection )
{
	lDirection -= 1;

	// Not a valid door direction.
	if (( lDirection < -1 ) || ( lDirection > 1 ))
		return ( INT_MAX );

	return ( lDirection );
}

//*****************************************************************************
//
void CLIENT_LogHUDMessage( const char *pszString, LONG lColor )
{
	static const char	szBar[] = TEXTCOLOR_ORANGE "\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36"
"\36\36\36\36\36\36\36\36\36\36\36\36\37" TEXTCOLOR_NORMAL "\n";
	static const char	szLogBar[] = "\n<------------------------------->\n";
	char				acLogColor[3];

	acLogColor[0] = '\x1c';
	acLogColor[1] = static_cast<char> ( (( lColor >= CR_BRICK ) && ( lColor <= CR_YELLOW )) ? ( lColor + 'A' ) : ( '-' ) );
	acLogColor[2] = '\0';

	AddToConsole( -1, szBar );
	AddToConsole( -1, acLogColor );
	AddToConsole( -1, pszString );
	AddToConsole( -1, szBar );

	// If we have a log file open, log it too.
	if ( Logfile )
	{
		fputs( szLogBar, Logfile );
		fputs( pszString, Logfile );
		fputs( szLogBar, Logfile );
		fflush( Logfile );
	}
}

//*****************************************************************************
//
void CLIENT_UpdatePendingWeapon( const player_t *pPlayer )
{
	// [BB] Only the client needs to do this.
	if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
		return;

	// [BB] Invalid argument, nothing to do.
	if ( pPlayer == NULL )
		return;

	// [BB] The current PendingWeapon is invalid.
	if ( ( pPlayer->PendingWeapon == WP_NOCHANGE ) || ( pPlayer->PendingWeapon == NULL ) )
		return;

	// [BB] A client only needs to handle its own weapon changes.
	if ( static_cast<LONG>( pPlayer - players ) == consoleplayer )
	{
		CLIENTCOMMANDS_WeaponSelect( pPlayer->PendingWeapon->GetClass( ));

		if ( CLIENTDEMO_IsRecording( ))
			CLIENTDEMO_WriteLocalCommand( CLD_LCMD_INVUSE, pPlayer->PendingWeapon->GetClass( )->TypeName.GetChars( ) );
	}
}

//*****************************************************************************
//
void CLIENT_SetActorToLastDeathStateFrame ( AActor *pActor )
{
	FState	*pDeadState = pActor->FindState( NAME_Death );
	FState	*pBaseState = NULL;

	do
	{
		pBaseState = pDeadState;
		if ( pBaseState != NULL )
		{
			pActor->SetState( pBaseState, true );
			pDeadState = pBaseState->GetNextState( );
		}
	} while (( pDeadState != NULL ) && ( pDeadState == pBaseState + 1 ) && ( pBaseState->GetTics() != -1 ) );
	// [BB] The "pBaseState->GetTics() != -1" check prevents jumping over frames with ininite duration.
	// This matters if the death state is not ended by "Stop".
}

//*****************************************************************************
//
// [TP] For the code generator -- turns an actor net ID into an actor class
// - subclass is the subclass the actor must be an instance of (AActor * if no specific specialization)
// - allowNull is true of the result value is allowed to be NULL
// - commandName is the name of the command, used in warnings
// - parameterName is the name of the parameter, used in warnings
// - actor is a reference to the desired actor pointer.
//
// 'actor' MUST be either NULL or an instance of the provided subclass!
//
bool CLIENT_ReadActorFromNetID( int netid, const PClass *subclass, bool allowNull, AActor *&actor,
								const char *commandName, const char *parameterName )
{
	actor = CLIENT_FindThingByNetID( netid );

	if ( actor && ( actor->IsKindOf( subclass ) == false ))
	{
		if ( allowNull == false )
		{
			CLIENT_PrintWarning( "%s: %s (%s) does not derive from %s\n",
				commandName, parameterName, actor->GetClass()->TypeName.GetChars(), subclass->TypeName.GetChars() );
			return false;
		}
		actor = NULL;
	}

	if (( actor == NULL ) && ( allowNull == false ))
	{
		CLIENT_PrintWarning( "%s: couldn't find %s: %d\n", commandName, parameterName, netid );
		return false;
	}

	return true;
}

//*****************************************************************************
//*****************************************************************************
//
void ServerCommands::Header::Execute()
{
	// This is the # of the packet the server has sent us.
    // This function shouldn't ever be called since the packet header is parsed seperately.
}
//*****************************************************************************
//
/*
static void client_ResetSequence( BYTESTREAM_s *pByteStream )
{
//	Printf( "SEQUENCE RESET! g_lLastParsedSequence = %d\n", g_lLastParsedSequence );
	g_lLastParsedSequence = g_lHighestReceivedSequence - 2;
}
*/
//*****************************************************************************
//
void ServerCommands::Ping::Execute()
{
	// We send the time back to the server, so that it can tell how many milliseconds passed since it sent the
	// ping message to us.
	CLIENTCOMMANDS_Pong( time );

#ifdef WIN32
	// Also use the time to drift client tics
	I_SaveLastPingTime( );
#endif
}

//*****************************************************************************
//
void ServerCommands::BeginSnapshot::Execute()
{
	// Display in the console that we're receiving a snapshot.
	Printf( "Receiving snapshot...\n" );

	// Set the client connection state to receiving a snapshot. This disables things like
	// announcer sounds.
	CLIENT_SetConnectionState( CTS_RECEIVINGSNAPSHOT );
	// [BB] If we don't receive the full snapshot in time, we need to request what's missing eventually.
	g_ulRetryTicks = GAMESTATE_RESEND_TIME;
}

//*****************************************************************************
//
void ServerCommands::EndSnapshot::Execute()
{
	// We're all done! Set the new client connection state to active.
	CLIENT_SetConnectionState( CTS_ACTIVE );

	// Display in the console that we have the snapshot now.
	Printf( "Snapshot received.\n" );

	// Hide the console.
	C_HideConsole( );

	// Make the view active.
	viewactive = true;

	// Clear the notify strings.
	C_FlushDisplay( );

	// Now that we've received the snapshot, create the status bar.
	if ( StatusBar != NULL )
	{
		StatusBar->Destroy();
		StatusBar = NULL;
	}

	StatusBar = CreateStatusBar ();
	/*  [BB] Moved to CreateStatusBar()
	switch ( gameinfo.gametype )
	{
	case GAME_Doom:

		StatusBar = CreateDoomStatusBar( );
		break;
	case GAME_Heretic:

		StatusBar = CreateHereticStatusBar( );
		break;
	case GAME_Hexen:

		StatusBar = CreateHexenStatusBar( );
		break;
	case GAME_Strife:

		StatusBar = CreateStrifeStatusBar( );
		break;
	default:

		StatusBar = new FBaseStatusBar( 0 );
		break;
	}
	*/

	if ( StatusBar )
	{
		StatusBar->AttachToPlayer( &players[consoleplayer] );
		StatusBar->NewGame( );
	}

	// Display the message of the day.
	CLIENT_DisplayMOTD( );
}

//*****************************************************************************
//
void ServerCommands::FullUpdateCompleted::Execute()
{
	// [BB] The server doesn't send any info with this packet, it's just there to allow us
	// keeping track of the current time so that we don't think we are lagging immediately after receiving a full update.
	g_ulEndFullUpdateTic = gametic;
	g_bFullUpdateIncomplete = false;
	g_bClientLagging = false;
	// [BB] Tell the server that we received the full update.
	CLIENTCOMMANDS_FullUpdateReceived();
}

//*****************************************************************************
//
void ServerCommands::Nothing::Execute()
{
}

//*****************************************************************************
//
void ServerCommands::ResetMap::Execute()
{
	// [EP] Clear all the HUD messages.
	if ( StatusBar )
		StatusBar->DetachAllMessages();

	GAME_ResetMap();
}

//*****************************************************************************
//
void ServerCommands::SpawnPlayer::Execute()
{
	ULONG					ulPlayer = player - players;
	player_t				*pPlayer = player;
	APlayerPawn				*pActor;
	LONG					lSkin;
	bool					bWasWatchingPlayer;
	AActor					*pCameraActor;
	APlayerPawn				*pOldActor;

	if ( gamestate != GS_LEVEL )
	{
		CLIENT_PrintWarning( "SpawnPlayer: not in a level\n" );
		return;
	}

	AActor *pOldNetActor = g_NetIDList.findPointerByID ( netid );

	// If there's already an actor with this net ID, kill it!
	if ( pOldNetActor != NULL )
	{
		pOldNetActor->Destroy( );
		g_NetIDList.freeID ( netid );
	}

	// [BB] Potentially print the player number, position, and network ID of the player spawning.
	if ( cl_showspawnnames )
		Printf( "Player %d body: (%d, %d, %d), %d\n", static_cast<int>(ulPlayer), x >> FRACBITS, y >> FRACBITS, z >> FRACBITS, static_cast<int> (netid) );

	// [BB] Remember if we were already ignoring WeaponSelect commands. If so, the server
	// told us to ignore them and we need to continue to do so after spawning the player.
	const bool bSavedIgnoreWeaponSelect = CLIENT_GetIgnoreWeaponSelect ();
	// [BB] Don't let the client send the server WeaponSelect commands for all weapons that
	// are temporarily selected while getting the inventory. 
	CLIENT_IgnoreWeaponSelect ( true );

	// [BB] Possibly play a connect sound.
	if ( cl_connectsound && ( playeringame[ulPlayer] == false ) && isSpectating && ( ulPlayer != static_cast<ULONG>(consoleplayer) )
		&& ( CLIENT_GetConnectionState() != CTS_RECEIVINGSNAPSHOT ) )
		S_Sound( CHAN_AUTO, "zandronum/connect", 1.f, ATTN_NONE );

	// This player is now in the game!
	playeringame[ulPlayer] = true;
	pPlayer = &players[ulPlayer];

	// Kill the player's old icon if necessary.
	if ( pPlayer->pIcon )
	{
		pPlayer->pIcon->Destroy( );
		pPlayer->pIcon = NULL;
	}

	// If the console player is being respawned, and watching another player in demo
	// mode, allow the player to continue watching that player.
	if ((( pPlayer - players ) == consoleplayer ) &&
		( pPlayer->camera ) &&
		( pPlayer->camera != pPlayer->mo ) &&
		( CLIENTDEMO_IsPlaying( )))
	{
		pCameraActor = pPlayer->camera;
	}
	else
		pCameraActor = NULL;

	// First, disassociate the player's corpse.
	bWasWatchingPlayer = false;
	pOldActor = pPlayer->mo;
	if ( pOldActor )
	{
		// If the player's old body is not in a death state, put the body into the last
		// frame of its death state.
		if ( pOldActor->health > 0 )
		{
			A_Unblock( pOldActor, true, true );

			// Put him in the last frame of his death state.
			CLIENT_SetActorToLastDeathStateFrame ( pOldActor );
		}

		// Check to see if the console player is spectating through this player's eyes.
		// If so, we need to reattach his camera to it when the player respawns.
		if ( pOldActor->CheckLocalView( consoleplayer ))
			bWasWatchingPlayer = true;

		if (( priorState == PST_REBORN ) ||
			( priorState == PST_REBORNNOINVENTORY ) ||
			( priorState == PST_ENTER ) ||
			( priorState == PST_ENTERNOINVENTORY ) || isDeadSpectator)
		{
			// [BB] This will eventually free the player's body's network ID.
			G_QueueBody (pOldActor);
			pOldActor->player = NULL;
			pOldActor->id = -1;
		}
		else
		{
			pOldActor->Destroy( );
			pOldActor->player = NULL;
			pOldActor = NULL;
		}
	}

	// [BB] We may not filter coop inventory if the player changed the player class.
	// Thus we need to keep track of the old class.
	const BYTE oldPlayerClass = pPlayer->CurrentPlayerClass;

	// Set up the player class.
	pPlayer->CurrentPlayerClass = playerClass;
	pPlayer->cls = PlayerClasses[pPlayer->CurrentPlayerClass].Type;

	if ( isMorphed )
	{
		// [BB] We'll be casting the spawned body to APlayerPawn, so we must check
		// if the desired class is valid.
		if ( morphedClass && morphedClass->IsDescendantOf( RUNTIME_CLASS( APlayerPawn ) ) )
			pPlayer->cls = morphedClass;
	}

	// Spawn the body.
	pActor = static_cast<APlayerPawn *>( Spawn( pPlayer->cls, x, y, z, NO_REPLACE ));

	// [BB] If the player was morphed or unmorphed, we to substitute all pointers
	// to the old body to the new one. Otherwise (among other things) CLIENTSIDE
	// scripts will lose track of the player body as activator.
	if (pPlayer->mo && ( priorState == PST_LIVE ) && ( isMorphed != ( pPlayer->morphTics != 0 ) ) )
		DObject::StaticPointerSubstitution (pPlayer->mo, pActor);

	pPlayer->mo = pActor;
	pActor->player = pPlayer;
	pPlayer->playerstate = playerState;

	// If we were watching through this player's eyes, reattach the camera.
	if ( bWasWatchingPlayer )
		players[consoleplayer].camera = pPlayer->mo;

	// Set the network ID.
	pPlayer->mo->lNetID = netid;
	g_NetIDList.useID ( netid, pPlayer->mo );
	
	if ( ( netid > 0 ) && ( (int) ( netid / g_NetIDList.MAX_NETID_FOR_PLAYER ) == consoleplayer ) && ( netid >= players[consoleplayer].firstFreeNetId || netid == 1 ) ) {
		int oldfreenetid = players[consoleplayer].firstFreeNetId;
		players[consoleplayer].firstFreeNetId = netid % g_NetIDList.MAX_NETID_FOR_PLAYER + 1;
		if (players[consoleplayer].firstFreeNetId >= g_NetIDList.MAX_NETID_FOR_PLAYER * (consoleplayer + 1)) {
			players[consoleplayer].firstFreeNetId = 1;
		}
	}

	// Set the spectator variables [after G_PlayerReborn so our data doesn't get lost] [BB] Why?.
	// [BB] To properly handle that true spectators don't get default inventory, we need to set this
	// before calling G_PlayerReborn (which in turn calls GiveDefaultInventory).
	pPlayer->bSpectating = isSpectating;
	pPlayer->bDeadSpectator = isDeadSpectator;

	if (( priorState == PST_REBORN ) ||
		( priorState == PST_REBORNNOINVENTORY ) ||
		( priorState == PST_ENTER ) ||
		( priorState == PST_ENTERNOINVENTORY ) || isDeadSpectator)
	{
		G_PlayerReborn( pPlayer - players );
	}
	// [BB] The player possibly changed the player class, so we have to correct the health (usually done in G_PlayerReborn).
	else if ( priorState == PST_LIVE )
		pPlayer->health = pPlayer->mo->GetDefault ()->health;

	// Give all cards in death match mode.
	if ( deathmatch )
		pPlayer->mo->GiveDeathmatchInventory( );
	// [BC] Don't filter coop inventory in teamgame mode.
	// Special inventory handling for respawning in coop.
	// [BB] Also don't do so if the player changed the player class.
	else if (( teamgame == false ) &&
			 ( priorState == PST_REBORN ) &&
			 ( oldPlayerClass == pPlayer->CurrentPlayerClass ) &&
			 ( pOldActor ))
	{
		pPlayer->mo->FilterCoopRespawnInventory( pOldActor );
	}

	// Also, destroy all of the old actor's inventory.
	if ( pOldActor )
		pOldActor->DestroyAllInventory( );

	// If this is the console player, and he's spawned as a regular player, he's definitely not
	// in line anymore!
	if ((( pPlayer - players ) == consoleplayer ) && ( pPlayer->bSpectating == false ))
		JOINQUEUE_RemovePlayerFromQueue( consoleplayer );

	// Set the player's bot status.
	pPlayer->bIsBot = isBot;

	// [BB] If this if not "our" player, clear the weapon selected from the inventory and wait for
	// the server to tell us the selected weapon.
	if ( ( ( pPlayer - players ) != consoleplayer ) && ( pPlayer->bIsBot == false ) )
		PLAYER_ClearWeapon ( pPlayer );

	// [GRB] Reset skin
	pPlayer->userinfo.SkinNumChanged ( R_FindSkin (skins[pPlayer->userinfo.GetSkin()].name, pPlayer->CurrentPlayerClass) );

	// [WS] Don't set custom skin color when the player is morphed.
	// [BB] In team games (where we assume that all players are on a team), we allow the team color for morphed players.
	if (!(pActor->flags2 & MF2_DONTTRANSLATE) && ( !isMorphed || ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )))
	{
		// [RH] Be sure the player has the right translation
		R_BuildPlayerTranslation ( ulPlayer );

		// [RH] set color translations for player sprites
		pActor->Translation = TRANSLATION( TRANSLATION_Players, ulPlayer );
	}
	pActor->angle = angle;
	pActor->pitch = pActor->roll = 0;
	pActor->health = pPlayer->health;
	pActor->lFixedColormap = NOFIXEDCOLORMAP;

	//Added by MC: Identification (number in the players[MAXPLAYERS] array)
	pActor->id = ulPlayer;

	// [RH] Set player sprite based on skin
	// [BC] Handle cl_skins here.
	if ( cl_skins <= 0 )
	{
		lSkin = R_FindSkin( "base", pPlayer->CurrentPlayerClass );
		pActor->flags4 |= MF4_NOSKIN;
	}
	else if ( cl_skins >= 2 )
	{
		if ( skins[pPlayer->userinfo.GetSkin()].bCheat )
		{
			lSkin = R_FindSkin( "base", pPlayer->CurrentPlayerClass );
			pActor->flags4 |= MF4_NOSKIN;
		}
		else
			lSkin = pPlayer->userinfo.GetSkin();
	}
	else
		lSkin = pPlayer->userinfo.GetSkin();

	if (( lSkin < 0 ) || ( lSkin >= static_cast<LONG>(skins.Size()) ))
		lSkin = R_FindSkin( "base", pPlayer->CurrentPlayerClass );

	// [BB] There is no skin for the morphed class.
	if ( !isMorphed )
	{
		pActor->sprite = skins[lSkin].sprite;
	}

	pPlayer->DesiredFOV = pPlayer->FOV = float(fov);
	// If the console player was watching another player in demo mode, continue to follow
	// that other player.
	if ( pCameraActor )
		pPlayer->camera = pCameraActor;
	else
		pPlayer->camera = pActor;
	pPlayer->playerstate = PST_LIVE;
	pPlayer->refire = 0;
	pPlayer->damagecount = 0;
	pPlayer->bonuscount = 0;
	// [BB] Remember if the player was morphed.
	const bool bPlayerWasMorphed = ( pPlayer->morphTics != 0 );
	pPlayer->morphTics = 0;
	pPlayer->extralight = 0;
	pPlayer->MorphedPlayerClass = NULL;
	pPlayer->fixedcolormap = NOFIXEDCOLORMAP;
	pPlayer->fixedlightlevel = -1;
	pPlayer->viewheight = pPlayer->mo->ViewHeight;

	pPlayer->attacker = NULL;
	pPlayer->BlendR = pPlayer->BlendG = pPlayer->BlendB = pPlayer->BlendA = 0.f;
	pPlayer->mo->ResetAirSupply(false);
	pPlayer->cheats = 0;
	pPlayer->Uncrouch( );

	// killough 10/98: initialize bobbing to 0.
	pPlayer->velx = 0;
	pPlayer->vely = 0;
/*
	// If the console player is being respawned, place the camera back in his own body.
	if ( ulPlayer == consoleplayer )
		players[consoleplayer].camera = players[consoleplayer].mo;
*/
	// setup gun psprite
	P_SetupPsprites (pPlayer, false);

	// If this console player is looking through this player's eyes, attach the status
	// bar to this player.
	if (( StatusBar ) && ( players[ulPlayer].mo->CheckLocalView( consoleplayer )))
		StatusBar->AttachToPlayer( pPlayer );
/*
	if (( StatusBar ) &&
		( players[consoleplayer].camera ) &&
		( players[consoleplayer].camera->player ))
	{
		StatusBar->AttachToPlayer( players[consoleplayer].camera->player );
	}
*/
	// If the player is a spectator, set some properties.
	if ( pPlayer->bSpectating )
	{
		// [BB] Set a bunch of stuff, e.g. make the player unshootable, etc.
		PLAYER_SetDefaultSpectatorValues ( pPlayer );

		// Don't lag anymore if we're a spectator.
		if ( ulPlayer == static_cast<ULONG>(consoleplayer) )
			g_bClientLagging = false;
	}
	// [BB] Don't spawn fog when receiving a snapshot.
	// [WS] Don't spawn fog when a player is morphing. The server will tell us.
	else if ( CLIENT_GetConnectionState() != CTS_RECEIVINGSNAPSHOT && !isMorphed && !bPlayerWasMorphed )
	{
		// Spawn the respawn fog.
		unsigned an = pActor->angle >> ANGLETOFINESHIFT;
		// [CK] Don't spawn fog for facing west spawners online, if compatflag is on.
		if (!(pActor->angle == ANGLE_180 && (zacompatflags & ZACOMPATF_SILENT_WEST_SPAWNS)))
		{
			AActor *fog = Spawn( "TeleportFog", pActor->x + 20 * finecosine[an],
				pActor->y + 20 * finesine[an],
				pActor->z + TELEFOGHEIGHT, ALLOW_REPLACE );
			if ( fog )
				fog->target = pActor;
		}
	}

	pPlayer->playerstate = PST_LIVE;

	// [BB] If the player is reborn, we have to substitute all pointers
	// to the old body to the new one. Otherwise (among other things) CLIENTSIDE
	// ENTER scripts stop working after the corresponding player is respawned.
	if (priorState == PST_REBORN || priorState == PST_REBORNNOINVENTORY)
	{
		if ( pOldActor != NULL )
		{
			DObject::StaticPointerSubstitution (pOldActor, pPlayer->mo);
			// PointerSubstitution() will also affect the bodyque, so undo that now.
			for (int ii=0; ii < BODYQUESIZE; ++ii)
				if (bodyque[ii] == pPlayer->mo)
					bodyque[ii] = pOldActor;
		}
	}

	if ( isMorphed )
	{
		// [BB] Bring up the weapon of the morphed class.
		pPlayer->mo->ActivateMorphWeapon();
		// [BB] This marks the player as morphed. The client doesn't need to know the real
		// morph time since the server handles the timing of the unmorphing.
		pPlayer->morphTics = -1;
		// [EP] Still, assign the current class pointer, because it's used by the status bar
		pPlayer->MorphedPlayerClass = morphedClass;
		// [EP] Set the morph style, too.
		pPlayer->MorphStyle = morphStyle;
	}
	else
	{
		pPlayer->morphTics = 0;

		// [BB] If the player was just unmorphed, we need to set reactiontime to the same value P_UndoPlayerMorph uses.
		// [EP] The morph style, too.
		if ( bPlayerWasMorphed )
		{
			pPlayer->mo->reactiontime = 18;
			pPlayer->MorphStyle = 0;
		}
	}


	// If this is the consoleplayer, set the realorigin and ServerXYZMom.
	if ( ulPlayer == static_cast<ULONG>(consoleplayer) )
	{
		CLIENT_AdjustPredictionToServerSideConsolePlayerMove( pPlayer->mo->x, pPlayer->mo->y, pPlayer->mo->z );

		pPlayer->ServerXYZVel[0] = 0;
		pPlayer->ServerXYZVel[1] = 0;
		pPlayer->ServerXYZVel[2] = 0;
	}

	// [BB] Now that we have our inventory, tell the server the weapon we selected from it.
	// [BB] Only do this if the server didn't tell us to ignore the WeaponSelect commands.
	if ( bSavedIgnoreWeaponSelect == false ) {
		CLIENT_IgnoreWeaponSelect ( false );
		if ((( pPlayer - players ) == consoleplayer ) && ( pPlayer->ReadyWeapon ) )
		{
			CLIENTCOMMANDS_WeaponSelect( pPlayer->ReadyWeapon->GetClass( ));

			if ( CLIENTDEMO_IsRecording( ))
				CLIENTDEMO_WriteLocalCommand( CLD_LCMD_INVUSE, pPlayer->ReadyWeapon->GetClass( )->TypeName.GetChars( ) );
			// [BB] When playing a demo, we will bring up what we recorded with CLD_LCMD_INVUSE.
			else if ( CLIENTDEMO_IsPlaying() )
				PLAYER_ClearWeapon ( pPlayer );
		}
	}

	// [TP] If we're overriding colors, rebuild translations now.
	// If we just joined the game, rebuild all translations,
	// otherwise recoloring the player in question is sufficient.
	if ( D_ShouldOverridePlayerColors() )
	{
		bool joinedgame = ( ulPlayer == static_cast<ULONG>( consoleplayer ))
			&& ( priorState == PST_ENTER || priorState == PST_ENTERNOINVENTORY );
		D_UpdatePlayerColors( joinedgame ? MAXPLAYERS : ulPlayer );
	}

	if ( randomSeed >= 0 )
		pActor->SetRandomSeed( randomSeed );

	// Refresh the HUD because this is potentially a new player.
	SCOREBOARD_RefreshHUD( );
}

//*****************************************************************************
//
void ServerCommands::MovePlayer::Execute()
{
	// Check to make sure everything is valid. If not, break out.
	if ( gamestate != GS_LEVEL )
	{
		CLIENT_PrintWarning( "MovePlayer: not in a level\n" );
		return;
	}

	// If we're not allowed to know the player's location, then just make him invisible.
	if ( isVisible == false )
	{
		player->mo->renderflags |= RF_INVISIBLE;

		// Don't move the player since the server didn't send any useful position information.
		return;
	}
	else
		player->mo->renderflags &= ~RF_INVISIBLE;

	// [geNia] Set the player server position, which will be actually applied on next tick
	player->mo->serverX = x;
	player->mo->serverY = y;
	player->mo->serverZ = z;
	player->mo->serverPosUpdated = true;

	// [BB] If the spectated player uses the GL renderer and we are using software,
	// the viewangle has to be limited.	We don't care about cl_disallowfullpitch here.
	if (!currentrenderer)
	{
		// [BB] The user can restore ZDoom's freelook limit.
		const fixed_t pitchLimit = -ANGLE_1*(cl_oldfreelooklimit ? 32 : 56);
		if (pitch < pitchLimit)
			pitch = pitchLimit;
		if (pitch > ANGLE_1 * 56)
			pitch = ANGLE_1 * 56;
	}

	player->mo->waterlevel = waterlevel;

	// [geNia] Don't set player's angle and pitch when dead, this is handled by P_DeathThink
	if (player->mo->health > 0)
	{
		// Set the player's angle.
		player->mo->serverAngle = angle;
		player->mo->serverAngleUpdated = true;
		// Set the player's pitch.
		player->mo->serverPitch = pitch;
		player->mo->serverPitchUpdated = true;
	}

	// Set the player's XYZ momentum.
	player->mo->serverVelX = velx;
	player->mo->serverVelY = vely;
	player->mo->serverVelXYUpdated = true;
	player->mo->serverVelZ = velz;
	player->mo->serverVelZUpdated = true;

	player->cmd.ucmd.forwardmove = ucmd_forwardmove;
	player->cmd.ucmd.sidemove = ucmd_sidemove;
	player->cmd.ucmd.upmove = ucmd_upmove;
	player->cmd.ucmd.yaw = ucmd_yaw;
	player->cmd.ucmd.pitch = ucmd_pitch;
	player->cmd.ucmd.buttons = ucmd_buttons;
}

//*****************************************************************************
//
void ServerCommands::DamagePlayer::Execute()
{
	// Level not loaded, ignore...
	if ( gamestate != GS_LEVEL )
		return;

	// Calculate the amount of damage being taken based on the old health value, and the
	// new health value.
	int damage = player->health - health;

	// Do the damage.
//	P_DamageMobj( players[ulPlayer].mo, NULL, NULL, lDamage, 0, 0 );

	// Set the new health value.
	player->mo->health = player->health = health;

	ABasicArmor *basicArmor = player->mo->FindInventory<ABasicArmor>( );
	if ( basicArmor )
		basicArmor->Amount = armor;

	// [BB] Set the inflictor of the damage (necessary to let the HUD mugshot look in direction of the inflictor).
	player->attacker = attacker;
	player->damageTic = gametic;

	// Set the damagecount, for blood on the screen.
	player->damagecount += damage;
	if ( player->damagecount > 100 )
		player->damagecount = 100;
	if ( player->damagecount < 0 )
		player->damagecount = 0;

	if ( player->mo->CheckLocalView( consoleplayer ))
	{
		if ( damage > 100 )
			damage = 100;

		I_Tactile( 40,10,40 + damage * 2 );
	}
}

//*****************************************************************************
//
void ServerCommands::KillPlayer::Execute()
{
	// Set the player's new health.
	player->health = player->mo->health = health;

	// Set the player's damage type.
	player->mo->DamageType = damageType;

	// Kill the player.
	player->mo->Die( source, inflictor, 0 );

	// [BB] Set the attacker, necessary to let the death view follow the killer.
	player->attacker = source;
	player->damageTic = gametic;

	// If health on the status bar is less than 0%, make it 0%.
	if ( player->health <= 0 )
		player->health = 0;

	// [TP] FIXME: Wouldn't this be much easier to compute using source->player?
	ULONG ulSourcePlayer = MAXPLAYERS;
	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( playeringame[ulIdx] == false ) ||
			( players[ulIdx].mo == NULL ))
		{
			continue;
		}

		if ( players[ulIdx].mo == source )
		{
			ulSourcePlayer = ulIdx;
			break;
		}
	}

	if (( (GAMEMODE_GetCurrentFlags() & GMF_COOPERATIVE) == false ) &&
		( cl_showlargefragmessages ) &&
		( ulSourcePlayer < MAXPLAYERS ) &&
		( static_cast<ULONG>( player - players ) != ulSourcePlayer ) &&
		( MOD != NAME_SpawnTelefrag ) &&
		( GAMEMODE_IsGameInProgress() ))
	{
		if ((( ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNFRAGS ) == false ) || (( fraglimit == 0 ) || ( players[ulSourcePlayer].fragcount < fraglimit ))) &&
			(( ( ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNWINS ) && !( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS ) ) == false ) || (( winlimit == 0 ) || ( players[ulSourcePlayer].ulWins < static_cast<ULONG>(winlimit) ))) &&
			(( ( ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNWINS ) && ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS ) ) == false ) || (( winlimit == 0 ) || ( TEAM_GetWinCount( players[ulSourcePlayer].ulTeam ) < winlimit ))))
		{
			int viewplayer = static_cast<int>( SCOREBOARD_GetViewPlayer() );
			// Display a large "You were fragged by <name>." message in the middle of the screen.
			if ( player == &players[viewplayer] )
				SCOREBOARD_DisplayFraggedMessage( &players[ulSourcePlayer] );
			// Display a large "You fragged <name>!" message in the middle of the screen.
			else if ( ulSourcePlayer == static_cast<ULONG>(viewplayer) )
				SCOREBOARD_DisplayFragMessage( player );
		}
	}

	// [BB] Temporarily change the ReadyWeapon of ulSourcePlayer to the one the server told us.
	AWeapon *pSavedReadyWeapon = ( ulSourcePlayer < MAXPLAYERS ) ? players[ulSourcePlayer].ReadyWeapon : NULL;

	if ( ulSourcePlayer < MAXPLAYERS )
	{
		if ( weaponType == NULL )
			players[ulSourcePlayer].ReadyWeapon = NULL;
		else if ( players[ulSourcePlayer].mo )
		{
			AWeapon *weapon = static_cast<AWeapon *>( players[ulSourcePlayer].mo->FindInventory( weaponType ));
			if ( weapon == NULL )
				weapon = static_cast<AWeapon *>( players[ulSourcePlayer].mo->GiveInventoryType( weaponType ));

			if ( weapon )
				players[ulSourcePlayer].ReadyWeapon = weapon;
		}
	}

	// Finally, print the obituary string.
	ClientObituary( player->mo, inflictor, source, dmgflags, MOD );

	// [BB] Restore the weapon the player actually is using now.
	if ( ( ulSourcePlayer < MAXPLAYERS ) && ( players[ulSourcePlayer].ReadyWeapon != pSavedReadyWeapon ) )
		players[ulSourcePlayer].ReadyWeapon = pSavedReadyWeapon;
/*
	if ( ulSourcePlayer < MAXPLAYERS )
		ClientObituary( players[ulPlayer].mo, pInflictor, players[ulSourcePlayer].mo, MOD );
	else
		ClientObituary( players[ulPlayer].mo, pInflictor, NULL, MOD );
*/

	// Refresh the HUD, since this could affect the number of players left in an LMS game.
	SCOREBOARD_RefreshHUD( );
}

//*****************************************************************************
//
void ServerCommands::SetPlayerHealth::Execute()
{
	player->health = health;
	if ( player->mo )
		player->mo->health = health;
}

//*****************************************************************************
//
void ServerCommands::SetPlayerArmor::Execute()
{
	AInventory *pArmor = player->mo->FindInventory<ABasicArmor>();
	if ( pArmor != NULL )
	{
		pArmor->Amount = armorAmount;
		pArmor->Icon = TexMan.GetTexture( armorIcon, 0 );
	}
}

//*****************************************************************************
//
void ServerCommands::SetPlayerState::Execute()
{
	// If the player is dead, then we shouldn't have to update his state.
	if ( player->mo->health <= 0 )
		return;

	// Finally, change the player's state to whatever the server told us it was.
	switch( static_cast<PLAYERSTATE_e>( state ))
	{
	case STATE_PLAYER_IDLE:
		player->mo->PlayIdle( );
		break;

	case STATE_PLAYER_SEE:
		player->mo->SetState( player->mo->SpawnState );
		player->mo->PlayRunning( );
		break;

	case STATE_PLAYER_ATTACK:
	case STATE_PLAYER_ATTACK_ALTFIRE:
		player->mo->PlayAttacking( );
		// [BB] This partially fixes the problem that attack animations are not displayed in demos
		// if you are spying a player that you didn't spy when recording the demo. Still has problems
		// with A_ReFire.
		//
		// [BB] This is also needed to update the ammo count of the other players in coop game modes.
		//
		// [BB] It's necessary at all, because the server only informs a client about the cmd.ucmd.buttons
		// value (containing the information if a player uses BT_ATTACK or BT_ALTATTACK) of the player
		// who's eyes the client is spying through.
		// [BB] SERVERCOMMANDS_MovePlayer/client_MovePlayer now informs a client about BT_ATTACK or BT_ALTATTACK
		// of every player. This hopefully properly fixes these problems once and for all.
		/*
		if ( ( CLIENTDEMO_IsPlaying( ) || ( ( GAMEMODE_GetCurrentFlags() & GMF_COOPERATIVE ) && static_cast<signed> (ulPlayer) != consoleplayer ) )
				&& players[ulPlayer].ReadyWeapon )
		{
			if ( ulState == STATE_PLAYER_ATTACK )
				P_SetPsprite (&players[ulPlayer], ps_weapon, players[ulPlayer].ReadyWeapon->GetAtkState(!!players[ulPlayer].refire));
			else
				P_SetPsprite (&players[ulPlayer], ps_weapon, players[ulPlayer].ReadyWeapon->GetAltAtkState(!!players[ulPlayer].refire));

		}
		*/
		break;

	case STATE_PLAYER_ATTACK2:
		player->mo->PlayAttacking2( );
		break;

	default:
		CLIENT_PrintWarning( "client_SetPlayerState: Unknown state: %d\n", state );
		break;
	}
}

//*****************************************************************************
//
void ServerCommands::SetPlayerUserInfo::Execute()
{
	for ( unsigned int i = 0; i < cvars.Size(); ++i )
	{
		FName name = cvars[i].name;
		FString value = cvars[i].value;

		// Player's name.
		if ( name == NAME_Name )
		{
			if ( value.Len() > MAXPLAYERNAME )
				value = value.Left( MAXPLAYERNAME );
			player->userinfo.NameChanged ( value );
		}
		// Other info.
		else if ( name == NAME_Gender )
			player->userinfo.GenderNumChanged ( value.ToLong() );
		else if ( name == NAME_Color )
			player->userinfo.ColorChanged ( value );
		else if ( name == NAME_RailColor )
			player->userinfo.RailColorChanged ( value.ToLong() );
		// Make sure the skin is valid.
		else if ( name == NAME_Skin )
		{
			int skin;
			player->userinfo.SkinNumChanged ( R_FindSkin( value, player->CurrentPlayerClass ) );

			// [BC] Handle cl_skins here.
			if ( cl_skins <= 0 )
			{
				skin = R_FindSkin( "base", player->CurrentPlayerClass );
				if ( player->mo )
					player->mo->flags4 |= MF4_NOSKIN;
			}
			else if ( cl_skins >= 2 )
			{
				if ( skins[player->userinfo.GetSkin()].bCheat )
				{
					skin = R_FindSkin( "base", player->CurrentPlayerClass );
					if ( player->mo )
						player->mo->flags4 |= MF4_NOSKIN;
				}
				else
					skin = player->userinfo.GetSkin();
			}
			else
				skin = player->userinfo.GetSkin();

			if (( skin < 0 ) || ( skin >= static_cast<signed>(skins.Size()) ))
				skin = R_FindSkin( "base", player->CurrentPlayerClass );

			if ( player->mo )
			{
				player->mo->sprite = skins[skin].sprite;
			}
		}
		// Read in the player's handicap.
		else if ( name == NAME_Handicap )
			player->userinfo.HandicapChanged ( value.ToLong() );
		// [CK] We do compressed bitfields now.
		else if ( name == NAME_CL_ClientFlags )
			player->userinfo.ClientFlagsChanged ( value.ToLong() );
		else
		{
			FBaseCVar **cvarPointer = player->userinfo.CheckKey( name );
			FBaseCVar *cvar = cvarPointer ? *cvarPointer : nullptr;

			if ( cvar )
			{
				UCVarValue cvarValue;
				cvarValue.String = value;
				cvar->SetGenericRep( cvarValue, CVAR_String );
			}
		}
	}

	// Build translation tables, always gotta do this!
	R_BuildPlayerTranslation( player - players );
}

//*****************************************************************************
//
void ServerCommands::SetPlayerAccountName::Execute()
{
	g_PlayerAccountNames[player - players] = accountName;
}

//*****************************************************************************
//
void ServerCommands::SetPlayerFrags::Execute()
{
	if (( g_ConnectionState == CTS_ACTIVE ) &&
		( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNFRAGS ) &&
		!( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS ) &&
		( GAMEMODE_IsGameInProgress() ) &&
		// [BB] If we are still in the first tic of the level, we are receiving the frag count
		// as part of the full update (that is not considered as a snapshot after a "changemap"
		// map change). Thus don't announce anything in this case.
		( level.time != 0 ))
	{
		ANNOUNCER_PlayFragSounds( player - players, player->fragcount, fragCount );
	}

	// Finally, set the player's frag count, and refresh the HUD.
	player->fragcount = fragCount;
	SCOREBOARD_RefreshHUD( );
}

//*****************************************************************************
//
void ServerCommands::SetPlayerPoints::Execute()
{
	player->lPointCount = pointCount;
	SCOREBOARD_RefreshHUD( );
}

//*****************************************************************************
//
void ServerCommands::SetPlayerWins::Execute()
{
	player->ulWins = wins;
	SCOREBOARD_RefreshHUD( );
}

//*****************************************************************************
//
void ServerCommands::SetPlayerDeaths::Execute()
{
	player->ulDeathCount = deathCount;
	SCOREBOARD_RefreshHUD();
}

//*****************************************************************************
//
void ServerCommands::SetPlayerKillCount::Execute()
{
	player->killcount = killCount;
	SCOREBOARD_RefreshHUD( );
}

//*****************************************************************************
//
void ServerCommands::SetPlayerChatStatus::Execute()
{
	player->bChatting = chatting;
}

//*****************************************************************************
//
void ServerCommands::SetPlayerConsoleStatus::Execute()
{
	player->bInConsole = inConsole;
}

void ServerCommands::SetPlayerMenuStatus::Execute()
{
	player->bInMenu = inMenu;
}

//*****************************************************************************
//
void ServerCommands::SetPlayerLaggingStatus::Execute()
{
	player->bLagging = lagging;
}

//*****************************************************************************
//
void ServerCommands::SetPlayerReadyToGoOnStatus::Execute()
{
	player->bReadyToGoOn = readyToGoOn;
}

//*****************************************************************************
//
void ServerCommands::SetPlayerTeam::Execute()
{
	PLAYER_SetTeam( player, team, false );
}

//*****************************************************************************
//
void ServerCommands::SetPlayerCamera::Execute()
{
	AActor *oldcamera = player->camera;

	if ( camera == NULL )
	{
		player->camera = player->mo;
		player->cheats &= ~CF_REVERTPLEASE;
		if (oldcamera != players[consoleplayer].camera)
			R_ClearPastViewer (players[consoleplayer].camera);
		return;
	}

	// Finally, change the player's camera.
	player->camera = camera;
	if ( revertPlease == false )
		player->cheats &= ~CF_REVERTPLEASE;
	else
		player->cheats |= CF_REVERTPLEASE;

	if (oldcamera != players[consoleplayer].camera)
		R_ClearPastViewer (players[consoleplayer].camera);
}

//*****************************************************************************
//
void ServerCommands::SetPlayerPoisonCount::Execute()
{
	player->poisoncount = poisonCount;
}

//*****************************************************************************
//
void ServerCommands::SetPlayerAmmoCapacity::Execute()
{
	// [BB] Remember whether we already had this ammo.
	const bool hadAmmo = ( player->mo->FindInventory( ammoType ) != NULL );
	AInventory *pAmmo = CLIENT_FindPlayerInventory( player - players, ammoType );

	if ( pAmmo == NULL )
		return;

	if ( !(pAmmo->GetClass()->IsDescendantOf (RUNTIME_CLASS(AAmmo))) )
		return;

	// [BB] If we didn't have this kind of ammo yet, CLIENT_FindPlayerInventory gave it to us.
	// In this case make sure that the amount is zero.
	if ( hadAmmo == false )
		pAmmo->Amount = 0;

	// Set the new maximum amount of the inventory object.
	pAmmo->MaxAmount = maxAmount;

	// Since an item displayed on the HUD may have been given, refresh the HUD.
	SCOREBOARD_RefreshHUD( );
}

//*****************************************************************************
//
void ServerCommands::SetPlayerCheats::Execute()
{
	// [TP] If we're setting the cheats of the consoleplayer and we're spectating, don't let this command modify the noclip cheats.
	if ( player == &players[consoleplayer] && player->bSpectating )
		cheats = ( cheats & ~( CF_NOCLIP | CF_NOCLIP2 )) | ( player->cheats & ( CF_NOCLIP | CF_NOCLIP2 ));

	player->cheats = cheats;
}

//*****************************************************************************
//
void ServerCommands::SetPlayerPendingWeapon::Execute()
{
	// If we dont have this weapon already, we do now!
	AWeapon *weapon = static_cast<AWeapon *>( player->mo->FindInventory( weaponType ));
	if ( weapon == NULL )
		weapon = static_cast<AWeapon *>( player->mo->GiveInventoryType( weaponType ));

	// If he still doesn't have the object after trying to give it to him... then YIKES!
	if ( weapon == NULL )
	{
		CLIENT_PrintWarning( "client_SetPlayerPendingWeapon: Failed to give inventory type, %s!\n",
							 weaponType->TypeName.GetChars() );
		return;
	}

	player->PendingWeapon = weapon;
}

//*****************************************************************************
//
/* [BB] Does not work with the latest ZDoom changes. Check if it's still necessary.
static void client_SetPlayerPieces( BYTESTREAM_s *pByteStream )
{
	ULONG			ulPlayer;
	ULONG			ulPieces;

	// Read in the player.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read in the player's pieces.
	ulPieces = NETWORK_ReadByte( pByteStream );

	// If the player doesn't exist, get out!
	if ( playeringame[ulPlayer] == false )
		return;

	players[ulPlayer].pieces = ulPieces;
}
*/

//*****************************************************************************
//
void ServerCommands::SetPlayerPSprite::Execute()
{
	// Not in a level; nothing to do (shouldn't happen!)
	if ( gamestate != GS_LEVEL )
		return;

	if ( player->ReadyWeapon == NULL )
		return;

	if ( stateOwner->ActorInfo == NULL )
		return;

	FState *pNewState = stateOwner->ActorInfo->OwnedStates + offset;

	// [BB] The offset is only guaranteed to work if the actor owns the state.
	if ( stateOwner->ActorInfo->OwnsState( pNewState ) == false )
	{
		CLIENT_PrintWarning( "SetPlayerPSprite: %s does not own its state at offset %d\n",
		                     stateOwner->TypeName.GetChars(), offset );
	}
	// [BB] Don't switch to a state belonging to a completely unrelated actor.
	else if ( player->ReadyWeapon->GetClass()->IsDescendantOf ( stateOwner ) == false )
	{
		CLIENT_PrintWarning( "SetPlayerPSprite: %s is not a descendant of %s\n",
		                     player->ReadyWeapon->GetClass()->TypeName.GetChars(), stateOwner->TypeName.GetChars() );
	}
	else
	{
		P_SetPsprite( player, position, pNewState );
	}
}

//*****************************************************************************
//
void ServerCommands::SetPlayerBlend::Execute()
{
	player->BlendR = blendR;
	player->BlendG = blendG;
	player->BlendB = blendB;
	player->BlendA = blendA;
}

//*****************************************************************************
//
void ServerCommands::SetPlayerMaxHealth::Execute()
{
	player->mo->MaxHealth = maxHealth;
}

//*****************************************************************************
//
void ServerCommands::SetPlayerLivesLeft::Execute()
{
	player->ulLivesLeft = livesLeft;
}

//*****************************************************************************
//
void ServerCommands::SetPlayerViewHeight::Execute()
{
	player->mo->ViewHeight = viewHeight;
	player->viewheight = viewHeight;
}

//*****************************************************************************
//
void ServerCommands::UpdatePlayerPing::Execute()
{
	player->ulPing = ping;

#ifdef WIN32
	if (player - players == consoleplayer)
		I_SavePlayerPing( ping );
#endif
}

//*****************************************************************************
//
void ServerCommands::UpdatePlayerTime::Execute()
{
	player->ulTime = time * ( TICRATE * 60 );
}

//*****************************************************************************
//
void ServerCommands::MoveLocalPlayer::Execute()
{
	player_t *pPlayer = &players[consoleplayer];

	// No player object to update.
	if ( pPlayer->mo == NULL )
		return;

	// [BB] If the server already sent us our position for a later tic,
	// the current update is outdated and we have to ignore it completely.
	// This happens if packets from the unreliable buffer arrive in the wrong order.
	if ( CLIENT_GetLatestServerGametic ( ) > latestServerGametic )
		return;

	// [BB] Update the latest server tic.
	CLIENT_SetLatestServerGametic( latestServerGametic );

	// "ulClientTicOnServerEnd" is the gametic of the last time we sent a movement command.
	CLIENT_SetLastConsolePlayerUpdateTick( clientTicOnServerEnd );

	// If the last time the server heard from us exceeds one second, the client is lagging!
	// [BB] But don't think we are lagging immediately after receiving a full update.
	if (( gametic - CLIENTDEMO_GetGameticOffset( ) - clientTicOnServerEnd >= TICRATE ) && (( gametic + CLIENTDEMO_GetGameticOffset( ) - g_ulEndFullUpdateTic ) > TICRATE ))
		g_bClientLagging = true;
	else
		g_bClientLagging = false;

	// If the player is dead, simply ignore this (remember, this could be parsed from an
	// out-of-order packet, since it's sent unreliably)!
	if ( pPlayer->playerstate == PST_DEAD )
		return;

	// Now that everything's check out, update stuff.
	if ( pPlayer->bSpectating == false )
	{
		pPlayer->ServerXYZ[0] = x;
		pPlayer->ServerXYZ[1] = y;
		pPlayer->ServerXYZ[2] = z;

		pPlayer->ServerXYZVel[0] = velx;
		pPlayer->ServerXYZVel[1] = vely;
		pPlayer->ServerXYZVel[2] = velz;
	}
	else
	{
		// [geNia] Set the player server position and apply it immediately
		pPlayer->mo->serverX = x;
		pPlayer->mo->serverY = y;
		pPlayer->mo->serverZ = z;
		pPlayer->mo->serverPosUpdated = true;
		pPlayer->mo->MoveToServerPosition();

		pPlayer->mo->velx = velx;
		pPlayer->mo->vely = vely;
		pPlayer->mo->velz = velz;
	}
}

//*****************************************************************************
//
void ServerCommands::FutureThrustLocalPlayer::Execute()
{
	CLIENT_PREDICT_SaveFutureThrust( futureTic, futureVelx, futureVely, futureVelz, overrideVelocity, bob );
}

//*****************************************************************************
//
void ServerCommands::DisconnectPlayer::Execute()
{
	// If we were a spectator and looking through this player's eyes, revert them.
	if ( player->mo->CheckLocalView( consoleplayer ))
	{
		CLIENT_ResetConsolePlayerCamera( );
	}

	// Create a little disconnect particle effect thingamabobber!
	// [BB] Only do this if a non-spectator disconnects.
	if ( player->bSpectating == false )
	{
		P_DisconnectEffect( player->mo );

		// [BB] Stop all CLIENTSIDE scripts of the player that are still running.
		if ( !( zacompatflags & ZACOMPATF_DONT_STOP_PLAYER_SCRIPTS_ON_DISCONNECT ) )
			FBehavior::StaticStopMyScripts ( player->mo );
	}

	// Destroy the actor associated with the player.
	if ( player->mo )
	{
		player->mo->Destroy( );
		player->mo = NULL;
	}

	playeringame[player - players] = false;

	// Zero out all the player information.
	PLAYER_ResetPlayerData( player );

	// Refresh the HUD because this affects the number of players in the game.
	SCOREBOARD_RefreshHUD( );
}

//*****************************************************************************
//
void ServerCommands::SecretFound::Execute()
{
	const bool allowclient = true;
	P_GiveSecret( actor, !!(secretFlags & SECRETFOUND_MESSAGE), secretFlags & SECRETFOUND_SOUND, allowclient );
}

//*****************************************************************************
//
void ServerCommands::SecretMarkSectorFound::Execute()
{
	sector->special &= ~SECRET_MASK;
}

//*****************************************************************************
//
void ServerCommands::Lightning::Execute()
{
	// [Dusk] The client doesn't care about the mode given to P_ForceLightning since
	// it doesn't do the next lightning calculations anyway.
	P_ForceLightning( 0 );
}

//*****************************************************************************
//
void ServerCommands::SetConsolePlayer::Execute()
{
	// If this index is invalid, break out.
	// [TP] playerNumber < 0 could happen if the packet is too short. However, the code generator
	// should already ward against executing truncated commands. But let's add a check anyway...
	if (( playerNumber < 0 ) || ( playerNumber >= MAXPLAYERS ))
		return;

	// In a client demo, don't lose the userinfo we gave to our console player.
	if ( CLIENTDEMO_IsPlaying() && ( playerNumber != consoleplayer ))
	{
		players[playerNumber].userinfo.TransferFrom( players[consoleplayer].userinfo );
		players[consoleplayer].userinfo.Reset();
	}

	// Otherwise, since it's valid, set our local player index to this.
	consoleplayer = playerNumber;

	// Finally, apply our local userinfo to this player slot.
	D_SetupUserInfo( );
}

//*****************************************************************************
//
void ServerCommands::ConsolePlayerKicked::Execute()
{
	// Set the connection state to "disconnected" before calling CLIENT_QuitNetworkGame( ),
	// so that we don't send a disconnect signal to the server.
	CLIENT_SetConnectionState( CTS_DISCONNECTED );

	// End the network game.
	CLIENT_QuitNetworkGame( "You have been kicked from the server.\n" );
}

//*****************************************************************************
//
void ServerCommands::GivePlayerMedal::Execute()
{
	MEDAL_GiveMedal( player - players, medal );
}

//*****************************************************************************
//
void ServerCommands::ResetAllPlayersFragcount::Execute()
{
	// This function pretty much takes care of everything we need to do!
	PLAYER_ResetAllPlayersFragcount( );
}

//*****************************************************************************
//
void ServerCommands::PlayerIsSpectator::Execute()
{
	// [BB] If the player turns into a true spectator, create the disconnect particle effect.
	// Note: We have to do this before the player is actually turned into a spectator, because
	// the tiny height of a spectator's body would alter the effect.
	if ( ( deadSpectator == false ) && player->mo )
		P_DisconnectEffect( player->mo );

	// Make the player a spectator.
	PLAYER_SetSpectator( player, false, deadSpectator );

	// If we were a spectator and looking through this player's eyes, revert them.
	if ( player->mo && player->mo->CheckLocalView( consoleplayer ))
	{
		CLIENT_ResetConsolePlayerCamera();
	}

	// Don't lag anymore if we're a spectator.
	if ( player == &players[consoleplayer] )
		g_bClientLagging = false;

	// [EP] Refresh the HUD, since this could affect the number of players left in a dead spectators game.
	SCOREBOARD_RefreshHUD();
}

//*****************************************************************************
//
void ServerCommands::PlayerSay::Execute()
{
	// If playerNumber == MAXPLAYERS, that means the server is talking.
	if ( playerNumber != MAXPLAYERS )
	{
		// If this is an invalid player, break out.
		if ( PLAYER_IsValidPlayer( playerNumber ) == false )
			return;
	}

	// Finally, print out the chat string.
	CHAT_PrintChatString( playerNumber, mode, message );
}

//*****************************************************************************
//
void ServerCommands::PlayerTaunt::Execute()
{
	// Don't taunt if we're not in a level!
	if ( gamestate != GS_LEVEL )
		return;

	if (( player->bSpectating ) ||
		( player->health <= 0 ) ||
		( player->mo == NULL ) ||
		( zacompatflags & ZACOMPATF_DISABLETAUNTS ))
	{
		return;
	}

	// Play the taunt sound!
	if ( cl_taunts )
		S_Sound( player->mo, CHAN_VOICE, "*taunt", 1, ATTN_NORM );
}

//*****************************************************************************
//
void ServerCommands::PlayerRespawnInvulnerability::Execute()
{
	// Don't taunt if we're not in a level!
	if ( gamestate != GS_LEVEL )
		return;

	// First, we need to adjust the blend color, so the player's screen doesn't go white.
	APowerInvulnerable *invulnerability = player->mo->FindInventory<APowerInvulnerable>();

	if ( invulnerability == NULL )
		return;

	invulnerability->BlendColor = 0;

	// Apply respawn invulnerability effect.
	switch ( cl_respawninvuleffect )
	{
	case 1:
		player->mo->RenderStyle = STYLE_Translucent;
		player->mo->effects |= FX_VISIBILITYFLICKER;
		break;

	case 2:
		player->mo->effects |= FX_RESPAWNINVUL;
		break;
	}
}

//*****************************************************************************
//
void ServerCommands::PlayerUseInventory::Execute()
{
	// Try to find this object within the player's personal inventory.
	AInventory *item = player->mo->FindInventory( itemType );

	// If the player doesn't have this type, give it to him.
	if ( item == NULL )
		item = player->mo->GiveInventoryType( itemType );

	// If he still doesn't have the object after trying to give it to him... then YIKES!
	if ( item == NULL )
	{
		CLIENT_PrintWarning( "client_PlayerUseInventory: Failed to give inventory type, %s!\n", itemType->TypeName.GetChars() );
		return;
	}

	// Finally, use the item.
	const bool success = player->mo->UseInventory( item );

	// [BB] The server only instructs the client to use the item, if the use was successful.
	// So using it on the client also should be successful. If it is not, something went wrong,
	// e.g. the use has a A_JumpIfHealthLower(100,"Success") check and a successful use heals the
	// player such that the check will fail after the item was used. Since the server informs the
	// client about the effects of the use before it tells the client to use the item, the use
	// on the client will fail. In this case at least reduce the inventory amount according to a
	// successful use.
	if ( ( success == false ) && !( dmflags2 & DF2_INFINITE_INVENTORY ) )
	{
		if (--item->Amount <= 0 && !(item->ItemFlags & IF_KEEPDEPLETED))
		{
			item->Destroy ();
		}
	}
}

//*****************************************************************************
//
void ServerCommands::PlayerDropInventory::Execute()
{
	// Try to find this object within the player's personal inventory.
	AInventory *item = player->mo->FindInventory( itemType );

	// If the player doesn't have this type, give it to him.
	if ( item == NULL )
		item = player->mo->GiveInventoryType( itemType );

	// If he still doesn't have the object after trying to give it to him... then YIKES!
	if ( item == NULL )
	{
		CLIENT_PrintWarning( "client_PlayerDropInventory: Failed to give inventory type, %s!\n", itemType->TypeName.GetChars() );
		return;
	}

	// Finally, drop the item.
	player->mo->DropInventory( item );
}

//*****************************************************************************
//
void ServerCommands::IgnorePlayer::Execute()
{
	player->bIgnoreChat = true;
	player->lIgnoreChatTicks = ignoreChatTicks;

	Printf( "%s\\c- will be ignored, because you're ignoring %s IP.\n", player->userinfo.GetName(), player->userinfo.GetGender() == GENDER_MALE ? "his" : player->userinfo.GetGender() == GENDER_FEMALE ? "her" : "its" );
}

//*****************************************************************************
//
void ServerCommands::GiveWeaponHolder::Execute()
{
	AWeaponHolder *holder = player->mo->FindInventory<AWeaponHolder>();

	if ( holder == NULL )
		holder = static_cast<AWeaponHolder *>( player->mo->GiveInventoryType( RUNTIME_CLASS( AWeaponHolder ) ) );

	if ( holder == NULL )
	{
		CLIENT_PrintWarning( "GiveWeaponHolder: Failed to give AWeaponHolder!\n" );
		return;
	}

	// Set the special fields. This is why this function exists in the first place.
	holder->PieceMask = pieceMask;
	holder->PieceWeapon = pieceWeapon;
}

//*****************************************************************************
//
void ServerCommands::SetHexenArmorSlots::Execute()
{
	AHexenArmor *hexenArmor = player->mo->FindInventory<AHexenArmor>();

	if ( hexenArmor )
	{
		hexenArmor->Slots[0] = slot0;
		hexenArmor->Slots[1] = slot1;
		hexenArmor->Slots[2] = slot2;
		hexenArmor->Slots[3] = slot3;
		hexenArmor->Slots[4] = slot4;
	}
	else
	{
		CLIENT_PrintWarning( "SetHexenArmorSlots: Player %td does not have HexenArmor!\n", player - players );
	}
}

//*****************************************************************************
//
void ServerCommands::SetIgnoreWeaponSelect::Execute()
{
	CLIENT_IgnoreWeaponSelect ( ignore );
}

//*****************************************************************************
//
void ServerCommands::ClearConsolePlayerWeapon::Execute()
{
	PLAYER_ClearWeapon ( &players[consoleplayer] );
}

//*****************************************************************************
//
void ServerCommands::CancelFade::Execute()
{
	// [BB] Needed to copy the code below from the implementation of PCD_CANCELFADE.
	TThinkerIterator<DFlashFader> iterator;
	DFlashFader *fader;

	while ( (fader = iterator.Next()) )
	{
		if (player == NULL || fader->WhoFor() == player->mo)
		{
			fader->Cancel ();
		}
	}
}

//*****************************************************************************
//
void ServerCommands::SetFastChaseStrafeCount::Execute()
{
	actor->FastChaseStrafeCount = strafeCount;
}

//*****************************************************************************
//
void ServerCommands::PlayBounceSound::Execute()
{
	actor->PlayBounceSound ( onFloor );
}

//*****************************************************************************
//
void ServerCommands::SpawnThing::Execute()
{
	CLIENT_SpawnThing( type, x, y, z, id, 0, randomSeed );
}

//*****************************************************************************
//
void ServerCommands::SpawnThingNoNetID::Execute()
{
	CLIENT_SpawnThing( type, x, y, z, -1, 0, randomSeed );
}

//*****************************************************************************
//
void ServerCommands::LevelSpawnThing::Execute()
{
	CLIENT_SpawnThing( type, x, y, z, id, SPAWNFLAG_LEVELTHING, randomSeed );
}

//*****************************************************************************
//
void ServerCommands::LevelSpawnThingNoNetID::Execute()
{
	CLIENT_SpawnThing( type, x, y, z, -1, SPAWNFLAG_LEVELTHING, -1 );
}

//*****************************************************************************
//
void ServerCommands::MoveThing::Execute()
{
	if (( actor == NULL ) || gamestate != GS_LEVEL )
		return;

	fixed_t x = ( bits & CM_XY ) ? newX : actor->x;
	fixed_t y = ( bits & CM_XY ) ? newY : actor->y;
	fixed_t z = ( bits & CM_Z ) ? newZ : actor->z;

	// Read in the last position data.
	if ( bits & CM_LAST_XY )
	{
		actor->lastX = lastX;
		actor->lastY = lastY;
	}
	else
	{
		actor->lastX = x;
		actor->lastY = y;
	}
	if ( bits & CM_LAST_Z )
		actor->lastZ = lastZ;
	else
		actor->lastZ = z;

	// [WS] Clients will reuse their last updated position.
	if ( bits & CM_REUSE_XY )
	{
		x = actor->lastX;
		y = actor->lastY;
	}
	if ( bits & CM_REUSE_Z )
		z = actor->lastZ;

	// Update the thing's position.
	if ( bits & ( CM_XY|CM_Z|CM_REUSE_XY|CM_REUSE_Z ))
	{
		if ( bits & CM_NOSMOOTH )
		{
			actor->x = x;
			actor->y = y;
			actor->z = z;
			actor->PrevX = x;
			actor->PrevY = y;
			actor->PrevZ = z;

			if ( actor == players[consoleplayer].camera )
			{
				R_ResetViewInterpolation();
			}
		}
		else
		{
			actor->serverX = x;
			actor->serverY = y;
			actor->serverZ = z;
			actor->serverPosUpdated = true;
		}
	}

	// Read in the angle data.
	if ( ContainsAngle() )
	{
		if ( bits & CM_NOSMOOTH )
		{
			actor->angle = angle;
			actor->PrevAngle = angle;
		}
		else
		{
			actor->serverAngle = angle;
			actor->serverAngleUpdated = true;
		}
	}

	// Read in the momentum data.
	if ( bits & CM_VELXY )
	{
		if ( bits & CM_NOSMOOTH )
		{
			actor->velx = velX;
			actor->vely = velY;
		}
		else
		{
			actor->serverVelX = velX;
			actor->serverVelY = velY;
			actor->serverVelXYUpdated = true;
		}
	}
	if ( bits & CM_VELZ )
	{
		if ( bits & CM_NOSMOOTH )
		{
			actor->velz = velZ;
		}
		else
		{
			actor->serverVelZ = velZ;
			actor->serverVelZUpdated = true;
		}
	}

	// [TP] If this is a player that's being moved around, and his velocity becomes zero,
	// we need to stop his bobbing as well.
	if ( actor->player && ( actor->velx == 0 ) && ( actor->vely == 0 ))
		actor->player->velx = actor->player->vely = 0;

	// Read in the pitch data.
	if ( ContainsPitch() )
	{
		if ( bits & CM_NOSMOOTH )
		{
			actor->pitch = pitch;
		}
		else
		{
			actor->serverPitch = pitch;
			actor->serverPitchUpdated = true;
		}
	}

	// Read in the movedir data.
	if ( ContainsMovedir() )
		actor->movedir = movedir;
}

//*****************************************************************************
//
void ServerCommands::KillThing::Execute()
{
	// Level not loaded; ingore.
	if ( gamestate != GS_LEVEL )
		return;

	// Set the thing's health. This should enable the proper death state to play.
	victim->health = health;

	// Set the thing's damage type.
	victim->DamageType = damageType;

	// Kill the thing.
	victim->Die( source, inflictor );
}

//*****************************************************************************
//
void ServerCommands::SetThingState::Execute()
{
	FState *newState = NULL;

	switch ( state )
	{
	case STATE_SEE:

		newState = actor->SeeState;
		break;
	case STATE_SPAWN:

		newState = actor->SpawnState;
		break;
	case STATE_PAIN:

		newState = actor->FindState(NAME_Pain);
		break;
	case STATE_MELEE:

		newState = actor->MeleeState;
		break;
	case STATE_MISSILE:

		newState = actor->MissileState;
		break;
	case STATE_DEATH:

		newState = actor->FindState(NAME_Death);
		break;
	case STATE_XDEATH:

		newState = actor->FindState(NAME_XDeath);
		break;
	case STATE_RAISE:

		// When an actor raises, we need to do a whole bunch of other stuff.
		P_Thing_Raise( actor, true );
		return;
	case STATE_HEAL:
	case STATE_ARCHVILE_HEAL:

		// [BB] The monster count is increased when STATE_RAISE is set, so
		// don't do it here.
		if ( actor->FindState(NAME_Heal) )
		{
			newState = actor->FindState(NAME_Heal);
		}
		else if ( state == STATE_ARCHVILE_HEAL )
		{
			const PClass *archvile = PClass::FindClass("Archvile");
			if (archvile != NULL)
			{
				newState = archvile->ActorInfo->FindState(NAME_Heal);
			}
		}
		break;

	case STATE_IDLE:

		actor->SetIdle();
		return;

	// [Dusk]
	case STATE_WOUND:

		newState = actor->FindState( NAME_Wound );
		break;
	default:

		CLIENT_PrintWarning( "client_SetThingState: Unknown state: %d\n", state );
		return;
	}

	// [BB] We don't allow pNewState == NULL here. This function should set a state
	// not destroy the thing, which happens if you call SetState with NULL as argument.
	if ( newState )
		actor->SetState( newState );
}

//*****************************************************************************
//
void ServerCommands::SetThingTarget::Execute()
{
	actor->target = target;
}

//*****************************************************************************
//
void ServerCommands::DestroyThing::Execute()
{
	// [BB] If we spied the actor we are supposed to destory, reset our camera.
	if ( actor->CheckLocalView( consoleplayer ) )
		CLIENT_ResetConsolePlayerCamera( );

	// [BB] If we are destroying a player's body here, we must NULL the corresponding pointer.
	if ( actor->player && ( actor->player->mo == actor ) )
	{
		// [BB] We also have to stop all its associated CLIENTSIDE scripts. Otherwise
		// they would get disassociated and continue to run even if the player disconnects later.
		if ( !( zacompatflags & ZACOMPATF_DONT_STOP_PLAYER_SCRIPTS_ON_DISCONNECT ) )
			FBehavior::StaticStopMyScripts ( actor->player->mo );

		actor->player->mo = NULL;
	}

	// Destroy the thing.
	actor->Destroy( );
}

//*****************************************************************************
//
void ServerCommands::SetThingAngle::Execute()
{
	actor->serverAngle = angle;
	actor->serverAngleUpdated = true;
}

//*****************************************************************************
//
void ServerCommands::SetThingWaterLevel::Execute()
{
	actor->waterlevel = waterlevel;
}

//*****************************************************************************
//
void ServerCommands::SetThingFlags::Execute()
{
	switch ( static_cast<FlagSet>( flagset ))
	{
	case FLAGSET_FLAGS:
		{
			// [BB/EP] Before changing MF_NOBLOCKMAP and MF_NOSECTOR, we have to unlink the actor from all blocks.
			const bool relinkActor = ( ( flags & ( MF_NOBLOCKMAP | MF_NOSECTOR ) ) ^ ( actor->flags & ( MF_NOBLOCKMAP | MF_NOSECTOR ) ) ) != 0;
			// [BB] Unlink based on the old flags.
			if ( relinkActor )
				actor->UnlinkFromWorld ();

			actor->flags = flags;

			// [BB] Link based on the new flags.
			if ( relinkActor )
				actor->LinkToWorld ();
		}
		break;
	case FLAGSET_FLAGS2:

		actor->flags2 = flags;
		break;
	case FLAGSET_FLAGS3:

		actor->flags3 = flags;
		break;
	case FLAGSET_FLAGS4:

		actor->flags4 = flags;
		break;
	case FLAGSET_FLAGS5:

		actor->flags5 = flags;
		break;
	case FLAGSET_FLAGS6:

		actor->flags6 = flags;
		break;
	case FLAGSET_FLAGS7:

		actor->flags7 = flags;
		break;
	case FLAGSET_FLAGS8:

		actor->flags8 = flags;
		break;
	case FLAGSET_FLAGSST:

		actor->ulSTFlags = flags;
		break;
	case FLAGSET_MVFLAGS:

		actor->mvFlags = flags;
		break;
	default:
		CLIENT_PrintWarning( "client_SetThingFlags: Received an unknown flagset value: %d\n", static_cast<int>( flagset ) );
		break;
	}
}

//*****************************************************************************
//
void ServerCommands::SetThingArguments::Execute()
{
	if (isNamed)
		actor->args[0] = NETWORK_ACSScriptFromNetID( arg0 );
	else
		actor->args[0] = arg0;
	actor->args[1] = arg1;
	actor->args[2] = arg2;
	actor->args[3] = arg3;
	actor->args[4] = arg4;

	// Gross hack for invisible bridges, since they set their height/radius
	// based on their args the instant they're spawned.
	// [WS] Added check for CustomBridge
	if (( actor->IsKindOf( PClass::FindClass( "InvisibleBridge" ) )) ||
		( actor->IsKindOf( PClass::FindClass( "AmbientSound" ) )) ||
		( actor->IsKindOf( PClass::FindClass( "CustomBridge" ) )))
	{
		actor->BeginPlay( );
	}
}

//*****************************************************************************
//
void ServerCommands::SetThingTranslation::Execute()
{
	actor->Translation = translation;
}

//*****************************************************************************
//
void ServerCommands::SetThingSound::Execute()
{
	// Set one of the actor's sounds, depending on what was read in.
	switch ( soundType )
	{
	case ACTORSOUND_SEESOUND:
		actor->SeeSound = sound;
		break;

	case ACTORSOUND_ATTACKSOUND:
		actor->AttackSound = sound;
		break;

	case ACTORSOUND_PAINSOUND:
		actor->PainSound = sound;
		break;

	case ACTORSOUND_DEATHSOUND:
		actor->DeathSound = sound;
		break;

	case ACTORSOUND_ACTIVESOUND:
		actor->ActiveSound = sound;
		break;

	default:
		CLIENT_PrintWarning( "client_SetThingSound: Unknown sound, %d!\n", static_cast<unsigned int> (soundType) );
		return;
	}
}

//*****************************************************************************
//
void ServerCommands::SetThingSpawnPoint::Execute()
{
	actor->SpawnPoint[0] = spawnPointX;
	actor->SpawnPoint[1] = spawnPointY;
	actor->SpawnPoint[2] = spawnPointZ;
}

//*****************************************************************************
//
void ServerCommands::SetThingSpecial::Execute()
{
	actor->special = special;
}

//*****************************************************************************
//
void ServerCommands::SetThingSpecial1::Execute()
{
	actor->special1 = special1;
}

//*****************************************************************************
//
void ServerCommands::SetThingSpecial2::Execute()
{
	actor->special1 = special2;
}

//*****************************************************************************
//
void ServerCommands::SetThingWeaveIndex::Execute()
{
	actor->WeaveIndexXY = indexXY;
	actor->WeaveIndexZ = indexZ;
}

//*****************************************************************************
//
void ServerCommands::SetThingTics::Execute()
{
	actor->tics = tics;
}

//*****************************************************************************
//
void ServerCommands::SetThingTID::Execute()
{
	// [BB] Set the tid, but be careful doing so (cf. FUNC(LS_Thing_ChangeTID)).
	if (!(actor->ObjectFlags & OF_EuthanizeMe))
	{
		actor->RemoveFromHash();
		actor->tid = tid;
		actor->AddToHash();
	}
}

//*****************************************************************************
//
void ServerCommands::SetThingReactionTime::Execute()
{
	actor->reactiontime = reactiontime;
}

//*****************************************************************************
//
void ServerCommands::SetThingGravity::Execute()
{
	actor->gravity = gravity;
}

//*****************************************************************************
//
static void client_SetThingFrame( AActor* pActor, const PClass *stateOwner, int offset, bool bCallStateFunction )
{
	// Not in a level; nothing to do (shouldn't happen!)
	if ( gamestate != GS_LEVEL )
		return;

	// [BB] While skipping to the next map, DThinker::RunThinkers is not called, which seems to invalidate
	// the arguments of this command. Since we don't need to update the actors anyway, just skip this.
	if ( CLIENTDEMO_IsSkippingToNextMap() == true )
		return;

	if ( stateOwner->ActorInfo == NULL )
		return;

	FState *state = stateOwner->ActorInfo->OwnedStates + offset;

	// [BB] The offset is only guaranteed to work if the actor owns the state.
	if ( stateOwner->ActorInfo->OwnsState( state ) == false )
	{
		CLIENT_PrintWarning( "client_SetThingFrame: %s does not own its state at offset %d\n",
		                     stateOwner->TypeName.GetChars(), offset );
		return;
	}
	// [BB] Don't switch to a state belonging to a completely unrelated actor.
	else if ( pActor->GetClass()->IsDescendantOf ( stateOwner ) == false )
	{
		CLIENT_PrintWarning( "client_SetThingFrame: %s is not a descendant of %s\n",
		                     pActor->GetClass()->TypeName.GetChars(), stateOwner->TypeName.GetChars() );
	}
	else
	{
		// [BB] Workaround for actors whose spawn state has NoDelay. Make them execute the
		// spawn state function before jumping to the new state.
		if ( (pActor->ObjectFlags & OF_JustSpawned) && pActor->state && pActor->state->GetNoDelay() )
		{
			pActor->PostBeginPlay();
			pActor->Tick();
		}

		pActor->SetState( state, ( bCallStateFunction == false ));
	}
}

//*****************************************************************************
//
void ServerCommands::SetThingFrame::Execute()
{
	client_SetThingFrame( actor, stateOwner, offset, true );
}

//*****************************************************************************
//
void ServerCommands::SetThingFrameNF::Execute()
{
	client_SetThingFrame( actor, stateOwner, offset, false );
}

//*****************************************************************************
//
void ServerCommands::SetActorProperty::Execute()
{
	P_DoSetActorProperty( actor, property, value );
}

//*****************************************************************************
//
void ServerCommands::SetThingHealth::Execute()
{
	actor->health = health;
}

//*****************************************************************************
//
void ServerCommands::SetThingScale::Execute()
{
	if ( ContainsScaleX() )
		actor->scaleX = scaleX;

	if ( ContainsScaleY() )
		actor->scaleY = scaleY;
}

//*****************************************************************************
//
void ServerCommands::SetThingSpeed::Execute()
{
	actor->Speed = newspeed;
}

//*****************************************************************************
//
void ServerCommands::SetThingSize::Execute()
{
	actor->radius = newradius;
	actor->height = newheight;
}

//*****************************************************************************
//
void ServerCommands::SetThingFillColor::Execute()
{
	actor->fillcolor = fillcolor;
}

//*****************************************************************************
//
void ServerCommands::SetThingSprite::Execute()
{
	actor->sprite = sprite;
	actor->frame = frame;
}

//*****************************************************************************
//
void ServerCommands::FlashStealthMonster::Execute()
{
	if ( actor && (actor->flags & MF_STEALTH ))
	{
		actor->alpha = OPAQUE;
		actor->visdir = -1;
	}
}

//*****************************************************************************
//
void ServerCommands::SetWeaponAmmoGive::Execute()
{
	weapon->AmmoGive1 = ammoGive1;
	weapon->AmmoGive2 = ammoGive2;
}

//*****************************************************************************
//
void ServerCommands::ThingIsCorpse::Execute()
{
	A_Unblock( actor, true, true );

	// Do some other stuff done in AActor::Die.
	actor->flags &= ~(MF_SHOOTABLE|MF_FLOAT|MF_SKULLFLY|MF_NOGRAVITY);
	actor->flags |= MF_CORPSE|MF_DROPOFF;
	actor->height >>= 2;

	// Set the thing to the last frame of its death state.
	CLIENT_SetActorToLastDeathStateFrame ( actor );

	if ( isMonster )
		level.killed_monsters++;

	// If this is a player, put the player in his dead state.
	if ( actor->player )
	{
		actor->player->playerstate = PST_DEAD;
		// [BB] Also set the health to 0.
		actor->player->health = actor->health = 0;
	}
}

//*****************************************************************************
//
void ServerCommands::HideThing::Execute()
{
	item->HideIndefinitely( );
}

//*****************************************************************************
//
void ServerCommands::TeleportThing::Execute()
{
	// Spawn teleport fog at the source.
	if ( sourcefog )
	{
		AActor *fog = Spawn<ATeleportFog>( actor->x, actor->y, actor->z + (( actor->flags & MF_MISSILE ) ? 0 : TELEFOGHEIGHT ), ALLOW_REPLACE );
		if ( fog )
			fog->target = actor;
	}

	actor->x = x;
	actor->y = y;
	actor->z = z;

	actor->PrevX = x;
	actor->PrevY = y;
	actor->PrevZ = z;

	actor->serverX = x;
	actor->serverY = y;
	actor->serverZ = z;
	actor->serverPosUpdated = true;
	actor->MoveToServerPosition();

	actor->serverVelX = momx;
	actor->serverVelY = momy;
	actor->serverVelZ = momz;

	// Spawn a teleport fog at the destination.
	if ( destfog )
	{
		// Spawn the fog slightly in front of the thing's destination.
		angle_t fineangle = angle >> ANGLETOFINESHIFT;

		AActor *fog = Spawn<ATeleportFog>( actor->x + ( 20 * finecosine[fineangle] ),
			actor->y + ( 20 * finesine[fineangle] ),
			actor->z + (( actor->flags & MF_MISSILE ) ? 0 : TELEFOGHEIGHT ),
			ALLOW_REPLACE );
		if ( fog )
			fog->target = actor;
	}

	// Set the thing's new momentum.
	actor->velx = momx;
	actor->vely = momy;
	actor->velz = momz;

	// Also, if this is a player, set his bobbing appropriately.
	if ( actor->player )
	{
		actor->player->velx = momx;
		actor->player->vely = momy;
		actor->player->viewz = actor->z + actor->player->viewheight;

		// [BB] If the server is teleporting us, don't let our prediction get messed up.
		if ( actor == players[consoleplayer].mo )
			CLIENT_AdjustPredictionToServerSideConsolePlayerMove( x, y, z );
	}

	if ( actor == players[consoleplayer].camera )
	{
		R_ResetViewInterpolation();
	}

	// Reset the thing's new reaction time.
	actor->reactiontime = reactiontime;

	// Set the thing's new angle.
	actor->angle = angle;
	actor->PrevAngle = angle;

	// User variable to do a weird zoom thingy when you teleport.
	if (( teleportzoom ) && ( telezoom ) && ( actor->player ))
		actor->player->FOV = MIN( 175.f, actor->player->DesiredFOV + 45.f );
}

//*****************************************************************************
//
void ServerCommands::ThingActivate::Execute()
{
	actor->Activate( activator );
}

//*****************************************************************************
//
void ServerCommands::ThingDeactivate::Execute()
{
	actor->Deactivate( activator );
}

//*****************************************************************************
//
void ServerCommands::RespawnDoomThing::Execute()
{
	// Nothing to do if the level isn't loaded!
	if ( gamestate != GS_LEVEL )
		return;

	CLIENT_RestoreSpecialPosition( actor );
	CLIENT_RestoreSpecialDoomThing( actor, fog );
}

//*****************************************************************************
//
void ServerCommands::RespawnRavenThing::Execute()
{
	// Nothing to do if the level isn't loaded!
	if ( gamestate != GS_LEVEL )
		return;

	actor->renderflags &= ~RF_INVISIBLE;
	S_Sound( actor, CHAN_VOICE, "misc/spawn", 1, ATTN_IDLE );

	actor->SetState( RUNTIME_CLASS ( AInventory )->ActorInfo->FindState("HideSpecial") + 3 );
}

//*****************************************************************************
//
void ServerCommands::SpawnBlood::Execute()
{
	P_SpawnBlood( x, y, z, dir, damage, originator );
}

//*****************************************************************************
//
void ServerCommands::SpawnBloodSplatter::Execute()
{
	P_BloodSplatter( x, y, z, originator );
}

//*****************************************************************************
//
void ServerCommands::SpawnBloodSplatter2::Execute()
{
	P_BloodSplatter2( x, y, z, originator );
}

//*****************************************************************************
//
void ServerCommands::SpawnPuff::Execute()
{
	CLIENT_SpawnThing( pufftype, x, y, z, id, SPAWNFLAG_PUFF, randomSeed );
}

//*****************************************************************************
//
void ServerCommands::SpawnPuffNoNetID::Execute()
{
	AActor *puff = CLIENT_SpawnThing( pufftype, x, y, z, -1, SPAWNFLAG_PUFF, -1 );

	if ( puff == NULL )
		return;

	// [BB] If we are supposed to set the translation, set it, if we sucessfully spawned the actor.
	if ( ContainsTranslation() )
	{
		puff->Translation = translation;
	}

	// Put the puff in the proper state.
	FState *state;
	switch ( stateid )
	{
	case STATE_CRASH:

		state = puff->FindState( NAME_Crash );
		if ( state )
			puff->SetState( state );
		break;
	case STATE_MELEE:

		state = puff->MeleeState;
		if ( state )
			puff->SetState( state );
		break;
	}
}

//*****************************************************************************
//
void ServerCommands::Print::Execute()
{
	Printf( printlevel, "%s", message.GetChars() );
}

//*****************************************************************************
//
void ServerCommands::PrintMid::Execute()
{
	if ( bold )
		C_MidPrintBold( SmallFont, message );
	else
		C_MidPrint( SmallFont, message );
}

//*****************************************************************************
//
void ServerCommands::PrintMOTD::Execute()
{
	// Read in the MOTD, and display it later.
	g_MOTD = motd;
	// [BB] Some cleaning of the string since we can't trust the server.
	V_RemoveTrailingCrapFromFString ( g_MOTD );
}

//*****************************************************************************
//
void ServerCommands::PrintHUDMessage::Execute()
{
	// We cannot create the message if there's no status bar to attach it to.
	if ( StatusBar == NULL )
		return;

	// [BB] We can't create the message if the font doesn't exist.
	FFont *font = V_GetFont( fontName );
	if ( font == NULL )
		return;

	// Create the message.
	DHUDMessage *hudMessage = new DHUDMessage( font, message,
		x,
		y,
		hudWidth,
		hudHeight,
		(EColorRange)color,
		holdTime );

	// Now attach the message.
	StatusBar->AttachMessage( hudMessage, id, layer );

	// Log the message if desired.
	if ( log )
		CLIENT_LogHUDMessage( message, color );
}

//*****************************************************************************
//
void ServerCommands::PrintHUDMessageFadeOut::Execute()
{
	// We cannot create the message if there's no status bar to attach it to.
	if ( StatusBar == NULL )
		return;

	// [BB] We can't create the message if the font doesn't exist.
	FFont *font = V_GetFont( fontName );
	if ( font == NULL )
		return;

	// Create the message.
	DHUDMessageFadeOut *hudMessage = new DHUDMessageFadeOut( font, message,
		x,
		y,
		hudWidth,
		hudHeight,
		(EColorRange)color,
		holdTime,
		fadeOutTime );

	// Now attach the message.
	StatusBar->AttachMessage( hudMessage, id, layer );

	// Log the message if desired.
	if ( log )
		CLIENT_LogHUDMessage( message, color );
}

//*****************************************************************************
//
void ServerCommands::PrintHUDMessageFadeInOut::Execute()
{
	// We cannot create the message if there's no status bar to attach it to.
	if ( StatusBar == NULL )
		return;

	// [BB] We can't create the message if the font doesn't exist.
	FFont *font = V_GetFont( fontName );
	if ( font == NULL )
		return;

	// Create the message.
	DHUDMessageFadeInOut *hudMessage = new DHUDMessageFadeInOut( font, message,
		x,
		y,
		hudWidth,
		hudHeight,
		(EColorRange)color,
		holdTime,
		fadeInTime,
		fadeOutTime );

	// Now attach the message.
	StatusBar->AttachMessage( hudMessage, id, layer );

	// Log the message if desired.
	if ( log )
		CLIENT_LogHUDMessage( message, color );
}

//*****************************************************************************
//
void ServerCommands::PrintHUDMessageTypeOnFadeOut::Execute()
{
	// We cannot create the message if there's no status bar to attach it to.
	if ( StatusBar == NULL )
		return;

	// [BB] We can't create the message if the font doesn't exist.
	FFont *font = V_GetFont( fontName );
	if ( font == NULL )
		return;

	// Create the message.
	DHUDMessageTypeOnFadeOut *hudMessage = new DHUDMessageTypeOnFadeOut( font, message,
		x,
		y,
		hudWidth,
		hudHeight,
		(EColorRange)color,
		typeOnTime,
		holdTime,
		fadeOutTime );

	// Now attach the message.
	StatusBar->AttachMessage( hudMessage, id, layer );

	// Log the message if desired.
	if ( log )
		CLIENT_LogHUDMessage( message, color );
}

//*****************************************************************************
//
void ServerCommands::SetGameMode::Execute()
{
	UCVarValue	Value;

	GAMEMODE_SetCurrentMode ( static_cast<GAMEMODE_e> ( theCurrentMode ) );

	// [BB] The client doesn't necessarily know the game mode in P_SetupLevel, so we have to call this here.
	if ( domination )
		DOMINATION_Init();

	Value.Bool = theInstaGib;
	instagib.ForceSet( Value, CVAR_Bool );

	Value.Bool = theBuckShot;
	buckshot.ForceSet( Value, CVAR_Bool );
}

//*****************************************************************************
//
void ServerCommands::SetGameSkill::Execute()
{
	UCVarValue	Value;

	// Read in the gameskill setting, and set gameskill to this setting.
	Value.Int = theGameSkill;
	gameskill.ForceSet( Value, CVAR_Int );

	// Do the same for botskill.
	Value.Int = theBotSkill;
	botskill.ForceSet( Value, CVAR_Int );
}

//*****************************************************************************
//
void ServerCommands::SetGameDmFlags::Execute()
{
	UCVarValue	Value;

	// Read in the dmflags value, and set it to this value.
	Value.Int = theDmFlags;
	dmflags.ForceSet( Value, CVAR_Int );

	// Do the same for dmflags2.
	Value.Int = theDmFlags2;
	dmflags2.ForceSet( Value, CVAR_Int );

	// ... and compatflags.
	Value.Int = theCompatFlags;
	compatflags.ForceSet( Value, CVAR_Int );

	// ... and compatflags2.
	Value.Int = theCompatFlags2;
	compatflags2.ForceSet( Value, CVAR_Int );

	// [BB] ... and zacompatflags.
	Value.Int = theZacompatFlags;
	zacompatflags.ForceSet( Value, CVAR_Int );

	// [BB] ... and zadmflags.
	Value.Int = theZadmFlags;
	zadmflags.ForceSet( Value, CVAR_Int );
}

//*****************************************************************************
//
void ServerCommands::SetGameModeLimits::Execute()
{
	UCVarValue	Value;

	// Read in, and set the value for fraglimit.
	Value.Int = theFragLimit;
	fraglimit.ForceSet( Value, CVAR_Int );

	// Read in, and set the value for timelimit.
	Value.Float = theTimeLimit;
	timelimit.ForceSet( Value, CVAR_Float );

	// Read in, and set the value for pointlimit.
	Value.Int = thePointLimit;
	pointlimit.ForceSet( Value, CVAR_Int );

	// Read in, and set the value for duellimit.
	Value.Int = theDuelLimit;
	duellimit.ForceSet( Value, CVAR_Int );

	// Read in, and set the value for winlimit.
	Value.Int = theWinLimit;
	winlimit.ForceSet( Value, CVAR_Int );

	// Read in, and set the value for wavelimit.
	Value.Int = theWaveLimit;
	wavelimit.ForceSet( Value, CVAR_Int );

	// Read in, and set the value for sv_cheats.
	Value.Int = theCheats;
	sv_cheats.ForceSet( Value, CVAR_Int );
	// [BB] This ensures that am_cheat respects the sv_cheats value we just set.
	am_cheat.Callback();
	// [TP] Same for turbo
	turbo.Callback();

	// Read in, and set the value for sv_fastweapons.
	Value.Int = theFastWeapons;
	sv_fastweapons.ForceSet( Value, CVAR_Int );

	// Read in, and set the value for sv_maxlives.
	Value.Int = theMaxLives;
	sv_maxlives.ForceSet( Value, CVAR_Int );

	// Read in, and set the value for sv_maxteams.
	Value.Int = theMaxTeams;
	sv_maxteams.ForceSet( Value, CVAR_Int );

	// [BB] Read in, and set the value for sv_gravity.
	Value.Float = theLevelGravity;
	sv_gravity.ForceSet( Value, CVAR_Float );

	// [BB] Read in, and set the value for sv_aircontrol.
	Value.Float = theLevelAirControl;
	sv_aircontrol.ForceSet( Value, CVAR_Float );

	// [WS] Read in, and set the value for sv_coop_damagefactor.
	Value.Float = theCoopDamageFactor;
	sv_coop_damagefactor.ForceSet( Value, CVAR_Float );

	// [geNia] Read in, and set the value for splashfactor.
	Value.Float = splashFactor;
	splashfactor.ForceSet( Value, CVAR_Float );

	// [WS] Read in, and set the value for alwaysapplydmflags.
	Value.Bool = theAlwaysApplyDmFlags;
	alwaysapplydmflags.ForceSet( Value, CVAR_Bool );

	// [AM] Read in, and set the value for lobby.
	Value.String = theLobby;
	lobby.ForceSet( Value, CVAR_String );

	// [TP] Yea.
	Value.Bool = theLimitCommands;
	sv_limitcommands.ForceSet( Value, CVAR_Bool );

	// [geNia] Read in, and set the value for fov_change_cooldown_tics.
	Value.Int = fovChangeCooldownTics;
	fov_change_cooldown_tics.ForceSet( Value, CVAR_Int );

	// [geNia] Read in, and set the value for sv_headbob.
	Value.Float = headbob;
	sv_headbob.ForceSet( Value, CVAR_Float );
}

//*****************************************************************************
//
void ServerCommands::SetGameEndLevelDelay::Execute()
{
	GAME_SetEndLevelDelay( theEndLevelDelay );
}

//*****************************************************************************
//
void ServerCommands::SetGameModeState::Execute()
{
	if ( duel )
	{
		DUEL_SetState( (DUELSTATE_e)theState);
		DUEL_SetCountdownTicks( theCountdownTicks );
	}
	else if ( lastmanstanding || teamlms )
	{
		LASTMANSTANDING_SetState( (LMSSTATE_e)theState);
		LASTMANSTANDING_SetCountdownTicks( theCountdownTicks );
	}
	else if ( possession || teampossession )
	{
		POSSESSION_SetState( (PSNSTATE_e)theState);
		if ( (PSNSTATE_e)theState == PSNS_ARTIFACTHELD )
			POSSESSION_SetArtifactHoldTicks( theCountdownTicks );
		else
			POSSESSION_SetCountdownTicks( theCountdownTicks );
	}
	else if ( survival )
	{
		SURVIVAL_SetState( (SURVIVALSTATE_e)theState);
		SURVIVAL_SetCountdownTicks( theCountdownTicks );
	}
	else if ( invasion )
	{
		INVASION_SetState( (INVASIONSTATE_e)theState);
		INVASION_SetCountdownTicks( theCountdownTicks );
	}
	else if ( teamgame )
	{
		TEAM_SetState( (TEAMSTATE_e)theState);
		TEAM_SetCountdownTicks( theCountdownTicks );
	}
	else if ( deathmatch )
	{
		DEATHMATCH_SetState( (DEATHMATCHSTATE_e)theState);
		DEATHMATCH_SetCountdownTicks( theCountdownTicks );
	}
}

//*****************************************************************************
//
void ServerCommands::SetDuelNumDuels::Execute()
{
	DUEL_SetNumDuels( theNumDuels );
}

//*****************************************************************************
//
void ServerCommands::SetLMSSpectatorSettings::Execute()
{
	UCVarValue	Value;

	Value.Int = theLMSSpectatorSettings;
	lmsspectatorsettings.ForceSet( Value, CVAR_Int );
}

//*****************************************************************************
//
void ServerCommands::SetLMSAllowedWeapons::Execute()
{
	UCVarValue	Value;

	Value.Int = theLMSAllowedWeapons;
	lmsallowedweapons.ForceSet( Value, CVAR_Int );
}

//*****************************************************************************
//
void ServerCommands::SetInvasionNumMonstersLeft::Execute()
{
	// Set the number of monsters/archies left.
	INVASION_SetNumMonstersLeft( theNumMonstersLeft );
	INVASION_SetNumArchVilesLeft( theNumArchVilesLeft );
}

//*****************************************************************************
//
void ServerCommands::SetInvasionWave::Execute()
{
	// Set the current wave in the invasion module.
	INVASION_SetCurrentWave( theInvasionWave );
}

//*****************************************************************************
//
void ServerCommands::SetSimpleCTFSTMode::Execute()
{
	// Set the simple CTF/ST mode.
	TEAM_SetSimpleCTFSTMode( theSimpleCTFSTMode );
}

//*****************************************************************************
//
void ServerCommands::DoPossessionArtifactPickedUp::Execute()
{
	POSSESSION_ArtifactPickedUp( player, ticks );
}

//*****************************************************************************
//
void ServerCommands::DoPossessionArtifactDropped::Execute()
{
	// If we're not playing possession, there's no need to do this.
	if (( possession == false ) && ( teampossession == false ))
		return;

	// Simply call this function.
	POSSESSION_ArtifactDropped( );
}

//*****************************************************************************
//
void ServerCommands::DoGameModeFight::Execute()
{
	// Play fight sound, and draw gfx.
	if ( duel )
		DUEL_DoFight( );
	else if ( lastmanstanding || teamlms )
		LASTMANSTANDING_DoFight( );
	else if ( possession || teampossession )
		POSSESSION_DoFight( );
	else if ( survival )
		SURVIVAL_DoFight( );
	else if ( invasion )
		INVASION_BeginWave( currentWave );
	else if ( teamgame )
		TEAM_DoFight( );
	else
		DEATHMATCH_DoFight( );
}

//*****************************************************************************
//
void ServerCommands::DoGameModeCountdown::Execute()
{
	// Begin the countdown.
	if ( duel )
		DUEL_StartCountdown( ticks );
	else if ( lastmanstanding || teamlms )
		LASTMANSTANDING_StartCountdown( ticks );
	else if ( possession || teampossession )
		POSSESSION_StartCountdown( ticks );
	else if ( survival )
		SURVIVAL_StartCountdown( ticks );
	else if ( invasion )
		INVASION_StartCountdown( ticks );
	else if ( teamgame )
		TEAM_StartCountdown( ticks );
	else
		DEATHMATCH_StartCountdown( ticks );
}

//*****************************************************************************
//
void ServerCommands::DoGameModeWinSequence::Execute()
{
	ULONG ulWinner = (ULONG) winner;

	// Begin the win sequence.
	if ( duel )
		DUEL_DoWinSequence( ulWinner );
	else if ( lastmanstanding || teamlms )
		LASTMANSTANDING_DoWinSequence( ulWinner );
//	else if ( possession || teampossession )
//		POSSESSION_DoWinSequence( ulWinner );
	else if ( invasion )
		INVASION_DoWaveComplete( );
	else if ( teamgame )
		TEAM_SetState( TEAMS_WINSEQUENCE );
	else if ( deathmatch )
		DEATHMATCH_SetState( DEATHMATCHS_WINSEQUENCE );

	if ( ( deathmatch || teamgame ) && !players[consoleplayer].bSpectating )
	{
		if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
		{
			if ( ulWinner >= teams.Size( ) )
				ANNOUNCER_PlayEntry( cl_announcer, "DrawGame" );
			else if ( ulWinner == players[consoleplayer].ulTeam )
				ANNOUNCER_PlayEntry( cl_announcer, "YouWin" );
			else
				ANNOUNCER_PlayEntry( cl_announcer, "YouLose" );
		}
		else
		{
			if ( ulWinner >= MAXPLAYERS )
				ANNOUNCER_PlayEntry( cl_announcer, "DrawGame" );
			else if ( ulWinner == static_cast<ULONG>(consoleplayer) )
				ANNOUNCER_PlayEntry( cl_announcer, "YouWin" );
			else
				ANNOUNCER_PlayEntry( cl_announcer, "YouLose" );
		}
	}
}

//*****************************************************************************
//
void ServerCommands::DoGameModeSuddenDeath::Execute()
{
	ANNOUNCER_PlayEntry( cl_announcer, "SuddenDeath" );
}

//*****************************************************************************
//
void ServerCommands::SetDominationState::Execute()
{
	unsigned int *PointOwners = new unsigned int[pointOwners.Size()];
	for ( unsigned int i = 0; i < pointOwners.Size(); ++i )
		PointOwners[i] = pointOwners[i];
	DOMINATION_LoadInit( pointOwners.Size(), PointOwners );
}

//*****************************************************************************
//
void ServerCommands::SetDominationPointOwner::Execute()
{
	DOMINATION_SetOwnership( point, player );
}

//*****************************************************************************
//
void ServerCommands::SetTeamFrags::Execute()
{
	// Announce a lead change... but don't do it if we're receiving a snapshot of the level!
	if ( g_ConnectionState != CTS_ACTIVE )
		announce = false;

	TEAM_SetFragCount( team, frags, announce );
}

//*****************************************************************************
//
void ServerCommands::SetTeamScore::Execute()
{
	// Don't announce the score change if we're receiving a snapshot of the level!
	if ( g_ConnectionState != CTS_ACTIVE )
		announce = false;

	TEAM_SetScore( team, score, announce );

	SCOREBOARD_RefreshHUD( );
}

//*****************************************************************************
//
void ServerCommands::SetTeamWins::Execute()
{
	// Don't announce if we're receiving a snapshot of the level!
	if ( g_ConnectionState != CTS_ACTIVE )
		announce = false;

	TEAM_SetWinCount( team, wins, announce );
}

//*****************************************************************************
//
void ServerCommands::SetTeamReturnTicks::Execute()
{
	TEAM_SetReturnTicks( team, returnTicks );
}

//*****************************************************************************
//
void ServerCommands::TeamFlagReturned::Execute()
{
	TEAM_ExecuteReturnRoutine( team, NULL );
}

//*****************************************************************************
//
void ServerCommands::TeamFlagDropped::Execute()
{
	TEAM_FlagDropped( player, team );
}

//*****************************************************************************
//
void ServerCommands::SpawnMissile::Execute()
{
	CLIENT_SpawnMissile( missileType, x, y, z, velX, velY, velZ, netID, targetNetID, randomSeed );
}

//*****************************************************************************
//
void ServerCommands::MissileExplode::Execute()
{
	line_t *line;

	if ( lineId >= 0 && lineId < numlines )
		line = &lines[lineId];
	else
		line = NULL;

	missile->SetOrigin(x, y, z);
	missile->PrevX = x;
	missile->PrevY = y;
	missile->PrevZ = z;

	// Blow it up!
	// [BB] Only if it's not already in its death state.
	if ( missile->InState ( NAME_Death ) == false )
		P_ExplodeMissile( missile, line, NULL );
}

//*****************************************************************************
//
void ServerCommands::WeaponSound::Execute()
{
	// Don't play the sound if the console player is spying through this player's eyes.
	if ( player->mo->CheckLocalView( consoleplayer ) )
		return;

	S_Sound( player->mo, CHAN_WEAPON, sound, 1, ATTN_NORM );
}

//*****************************************************************************
//
void ServerCommands::WeaponChange::Execute()
{
	// If we dont have this weapon already, we do now!
	AWeapon *weapon = static_cast<AWeapon *>( player->mo->FindInventory( weaponType ));
	if ( weapon == NULL )
		weapon = static_cast<AWeapon *>( player->mo->GiveInventoryType( weaponType ));

	// If he still doesn't have the object after trying to give it to him... then YIKES!
	if ( weapon == NULL )
	{
		CLIENT_PrintWarning( "client_WeaponChange: Failed to give inventory type, %s!\n", weaponType->TypeName.GetChars() );
		return;
	}

	// Bring the weapon up if necessary.
	if ( ( player->ReadyWeapon != weapon ) || ( ( player->PendingWeapon != WP_NOCHANGE ) && ( player->PendingWeapon != weapon ) ) )
		player->PendingWeapon = weapon;

	// Confirm to the server that this is the weapon we're using.
	CLIENT_UpdatePendingWeapon( player );

	// [BB] Ensure that the weapon is brought up. Note: Needs to be done after calling
	// CLIENT_UpdatePendingWeapon because P_BringUpWeapon clears PendingWeapon to WP_NOCHANGE.
	P_BringUpWeapon( player );
}

//*****************************************************************************
//
void ServerCommands::WeaponRailgun::Execute()
{
	// If this is not an extended railgun command, we'll need to assume some defaults.
	if ( CheckExtended() == false )
	{
		angle = 0;
		pitch = 0;
		spawnclass = NULL;
		duration = 0;
		sparsity = 1.0f;
		drift = 1.0f;
	}

	P_DrawRailTrail( source, start, end, color1, color2, maxdiff, flags, spawnclass, angle, pitch, duration, sparsity, drift );
}

//*****************************************************************************
//
void ServerCommands::SetWeaponFovScale::Execute()
{
	AWeapon* weapon = static_cast<AWeapon*>(player->mo->FindInventory(weaponType));
	if (weapon)
		weapon->FOVScale = fovScale;
}

//*****************************************************************************
//
void ServerCommands::SetSectorFloorPlane::Execute()
{
	P_SetFloorPlane( sector, height );
}

//*****************************************************************************
//
void ServerCommands::SetSectorCeilingPlane::Execute()
{
	P_SetCeilingPlane( sector, height );
}

//*****************************************************************************
//
void ServerCommands::SetSectorFloorPlaneSlope::Execute()
{
	sector->floorplane.a = a;
	sector->floorplane.b = b;
	sector->floorplane.c = c;
	sector->floorplane.ic = DivScale32( 1, sector->floorplane.c );
}

//*****************************************************************************
//
void ServerCommands::SetSectorCeilingPlaneSlope::Execute()
{
	sector->ceilingplane.a = a;
	sector->ceilingplane.b = b;
	sector->ceilingplane.c = c;
	sector->ceilingplane.ic = DivScale32( 1, sector->ceilingplane.c );
}

//*****************************************************************************
//
void ServerCommands::SetSectorLightLevel::Execute()
{
	sector->lightlevel = lightLevel;
}

//*****************************************************************************
//
void ServerCommands::SetSectorColor::Execute()
{
	sector->SetColor( red, green, blue, desaturate, false, true );
}

//*****************************************************************************
//
void ServerCommands::SetSectorColorByTag::Execute()
{
	int secnum = -1;

	while (( secnum = P_FindSectorFromTag( tag, secnum )) >= 0 )
		sectors[secnum].SetColor( red, green, blue, desaturate, false, true );
}

//*****************************************************************************
//
void ServerCommands::SetSectorFade::Execute()
{
	sector->SetFade( red, green, blue, false, true );
}

//*****************************************************************************
//
void ServerCommands::SetSectorFadeByTag::Execute()
{
	int secnum = -1;

	while (( secnum = P_FindSectorFromTag( tag, secnum )) >= 0 )
		sectors[secnum].SetFade( red, green, blue, false, true );
}

//*****************************************************************************
//
void ServerCommands::SetSectorFlat::Execute()
{
	// Not in a level. Nothing to do!
	if ( gamestate != GS_LEVEL )
		return;

	sector->SetTexture( sector_t::ceiling, TexMan.GetTexture( ceilingFlatName, FTexture::TEX_Flat ));
	sector->SetTexture( sector_t::floor, TexMan.GetTexture( floorFlatName, FTexture::TEX_Flat ));
}

//*****************************************************************************
//
void ServerCommands::SetSectorPanning::Execute()
{
	// Finally, set the offsets.
	sector->SetXOffset( sector_t::ceiling, ceilingXOffset );
	sector->SetYOffset( sector_t::ceiling, ceilingYOffset );
	sector->SetXOffset( sector_t::floor, floorXOffset );
	sector->SetYOffset( sector_t::floor, floorYOffset );
}

//*****************************************************************************
//
void ServerCommands::SetSectorRotation::Execute()
{
	sector->SetAngle(sector_t::ceiling, ceilingRotation );
	sector->SetAngle(sector_t::floor, floorRotation );
}

//*****************************************************************************
//
void ServerCommands::SetSectorRotationByTag::Execute()
{
	int secnum = -1;

	while (( secnum = P_FindSectorFromTag( tag, secnum )) >= 0 )
	{
		sectors[secnum].SetAngle( sector_t::floor, floorRotation );
		sectors[secnum].SetAngle( sector_t::ceiling, ceilingRotation );
	}
}

//*****************************************************************************
//
void ServerCommands::SetSectorScale::Execute()
{
	sector->SetXScale( sector_t::ceiling, ceilingXScale );
	sector->SetYScale( sector_t::ceiling, ceilingYScale );
	sector->SetXScale( sector_t::floor, floorXScale );
	sector->SetYScale( sector_t::floor, floorYScale );
}

//*****************************************************************************
//
void ServerCommands::SetSectorSpecial::Execute()
{
	sector->special = special;
}

//*****************************************************************************
//
void ServerCommands::SetSectorFriction::Execute()
{
	sector->friction = friction;
	sector->movefactor = moveFactor;

	// I'm not sure if we need to do this, but let's do it anyway.
	if ( friction == ORIG_FRICTION )
		sector->special &= ~FRICTION_MASK;
	else
		sector->special |= FRICTION_MASK;
}

//*****************************************************************************
//
void ServerCommands::SetSectorAngleYOffset::Execute()
{
	sector->planes[sector_t::ceiling].xform.base_angle = ceilingBaseAngle;
	sector->planes[sector_t::ceiling].xform.base_yoffs = ceilingBaseYOffset;
	sector->planes[sector_t::floor].xform.base_angle = floorBaseAngle;
	sector->planes[sector_t::floor].xform.base_yoffs = floorBaseYOffset;
}

//*****************************************************************************
//
void ServerCommands::SetSectorGravity::Execute()
{
	sector->gravity = gravity;
}

//*****************************************************************************
//
void ServerCommands::SetSectorReflection::Execute()
{
	sector->reflect[sector_t::ceiling] = ceilingReflection;
	sector->reflect[sector_t::floor] = floorReflection;
}

//*****************************************************************************
//
void ServerCommands::StopSectorLightEffect::Execute()
{
	TThinkerIterator<DLighting>		Iterator;
	DLighting						*pEffect;

	while (( pEffect = Iterator.Next( )) != NULL )
	{
		if ( pEffect->GetSector( ) == sector )
		{
			pEffect->Destroy( );
			return;
		}
	}
}

//*****************************************************************************
//
void ServerCommands::DestroyAllSectorMovers::Execute()
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < (ULONG)numsectors; ulIdx++ )
	{
		if ( sectors[ulIdx].ceilingdata )
		{
			// Stop the sound sequence (if any) associated with this sector.
			SN_StopSequence( &sectors[ulIdx], CHAN_CEILING );

			sectors[ulIdx].ceilingdata->Destroy( );
			sectors[ulIdx].ceilingdata = NULL;
		}

		if ( sectors[ulIdx].floordata )
		{
			// Stop the sound sequence (if any) associated with this sector.
			SN_StopSequence( &sectors[ulIdx], CHAN_FLOOR );

			sectors[ulIdx].floordata->Destroy( );
			sectors[ulIdx].floordata = NULL;
		}
	}
}

//*****************************************************************************
//
void ServerCommands::SetSectorLink::Execute()
{
	P_AddSectorLinks( sector, tag, ceiling, moveType );
}

//*****************************************************************************
//
void ServerCommands::DoSectorLightFireFlicker::Execute()
{
	new DFireFlicker( sector, maxLight, minLight );
}

//*****************************************************************************
//
void ServerCommands::DoSectorLightFlicker::Execute()
{
	new DFlicker( sector, maxLight, minLight );
}

//*****************************************************************************
//
void ServerCommands::DoSectorLightLightFlash::Execute()
{
	new DLightFlash( sector, minLight, maxLight );
}

//*****************************************************************************
//
void ServerCommands::DoSectorLightStrobe::Execute()
{
	DStrobe *strobe = new DStrobe( sector, maxLight, minLight, brightTime, darkTime );
	strobe->SetCount( count );
}

//*****************************************************************************
//
void ServerCommands::DoSectorLightGlow::Execute()
{
	new DGlow( sector );
}

//*****************************************************************************
//
void ServerCommands::DoSectorLightGlow2::Execute()
{
	// Create the light effect.
	DGlow2 *glow2 = new DGlow2( sector, startLight, endLight, maxTics, oneShot );
	glow2->SetTics( tics );
}

//*****************************************************************************
//
void ServerCommands::DoSectorLightPhased::Execute()
{
	new DPhased( sector, baseLevel, phase );
}

//*****************************************************************************
//
void ServerCommands::SetLineSpecial::Execute()
{
	line->special = special;
}

//*****************************************************************************
//
void ServerCommands::SetLineAlpha::Execute()
{
	line->Alpha = alpha;
}

//*****************************************************************************
//
static void client_SetLineTextureHelper ( line_t *pLine, ULONG ulSide, ULONG ulPosition, FTextureID texture )
{
	if ( pLine->sidedef[ulSide] == NULL )
		return;

	side_t *pSide = pLine->sidedef[ulSide];

	switch ( ulPosition )
	{
	case 0 /*TEXTURE_TOP*/:

		pSide->SetTexture(side_t::top, texture);
		break;
	case 1 /*TEXTURE_MIDDLE*/:

		pSide->SetTexture(side_t::mid, texture);
		break;
	case 2 /*TEXTURE_BOTTOM*/:

		pSide->SetTexture(side_t::bottom, texture);
		break;
	default:

		break;
	}
}
//*****************************************************************************
//
void ServerCommands::SetLineTexture::Execute()
{
	FTextureID texture = TexMan.GetTexture( textureName, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable );

	if ( texture.Exists() )
		client_SetLineTextureHelper( line, side, position, texture );
	else
		CLIENT_PrintWarning( "SetLineTexture: unknown texture: %s\n", textureName.GetChars() );
}

//*****************************************************************************
//
void ServerCommands::SetLineTextureByID::Execute()
{
	FTextureID texture = TexMan.GetTexture( textureName, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable );

	if ( texture.Exists() )
	{
		int linenum = -1;
		while (( linenum = P_FindLineFromID( lineID, linenum )) >= 0)
			client_SetLineTextureHelper( &lines[linenum], side, position, texture );
	}
	else
		CLIENT_PrintWarning( "SetLineTextureByID: unknown texture: %s\n", textureName.GetChars() );
}

//*****************************************************************************
//
void ServerCommands::SetSomeLineFlags::Execute()
{
	line->flags &= ~(ML_BLOCKING|ML_BLOCK_PLAYERS|ML_BLOCKEVERYTHING|ML_RAILING|ML_ADDTRANS);
	line->flags |= blockFlags;
}

//*****************************************************************************
//
void ServerCommands::SetSideFlags::Execute()
{
	side->Flags = flags;
}

//*****************************************************************************
//
void ServerCommands::ACSScriptExecute::Execute()
{
	// [TP] Resolve the script netid into a script number
	int scriptNum = NETWORK_ACSScriptFromNetID( netid );

	// [TP] Make a name out of the levelnum
	FString mapname;
	level_info_t *levelinfo;
	if ( levelnum == 0 )
	{
		mapname = level.mapname;
	}
	else if (( levelinfo = FindLevelByNum ( levelnum )))
	{
		mapname = levelinfo->mapname;
	}
	else
	{
		CLIENT_PrintWarning( "client_ACSScriptExecute: Couldn't find map by levelnum: %d\n", levelnum );
		return;
	}

	line_t* line;
	if (( lineid <= 0 ) || ( lineid >= numlines ))
		line = NULL;
	else
		line = &lines[lineid];

	int args[4] = { arg0, arg1, arg2, arg3 };
	P_StartScript( activator, line, scriptNum, mapname, args, 4, ( backSide ? ACS_BACKSIDE : 0 ) | ( always ? ACS_ALWAYS : 0 ) );
}

//*****************************************************************************
//
void ServerCommands::ACSSendString::Execute()
{
	// [AK] Resolve the script netid into a script number
	int scriptNum = NETWORK_ACSScriptFromNetID( netid );

	// [AK] Add the string to the ACS string table.
	int args[4] = { GlobalACSStrings.AddString( string ), 0, 0, 0 };

	P_StartScript( activator, NULL, scriptNum, NULL, args, 4, ACS_ALWAYS );
}

//*****************************************************************************
//
void ServerCommands::Sound::Execute()
{
	if ( volume > 127 )
		volume = 127;

	S_Sound( channel, sound, (float)volume / 127.f, NETWORK_AttenuationIntToFloat ( attenuation ) );
}

//*****************************************************************************
//
void ServerCommands::SoundActor::Execute()
{
	if ( volume > 127 )
		volume = 127;

	S_Sound( actor, channel, sound, (float)volume / 127.f, NETWORK_AttenuationIntToFloat ( attenuation ) );
}

//*****************************************************************************
//
void ServerCommands::SoundActorIfNotPlaying::Execute()
{
	if ( volume > 127 )
		volume = 127;

	// [BB] Check whether the actor is already playing something.
	// Note: The checking may not include CHAN_LOOP.
	if ( S_IsActorPlayingSomething (actor, channel & (~CHAN_LOOP), S_FindSound ( sound ) ) )
		return;

	S_Sound( actor, channel, sound, (float)volume / 127.f, NETWORK_AttenuationIntToFloat ( attenuation ) );
}

//*****************************************************************************
//
void ServerCommands::StopSoundActor::Execute()
{
	S_StopSound( actor, channel );
}

//*****************************************************************************
//
void ServerCommands::SoundSector::Execute()
{
	if ( volume > 127 )
		volume = 127;

	S_Sound( sector, channel, sound, (float)volume / 127.f, NETWORK_AttenuationIntToFloat ( attenuation ) );
}

//*****************************************************************************
//
void ServerCommands::SoundPoint::Execute()
{
	if ( volume > 127 )
		volume = 127;

	// Finally, play the sound.
	S_Sound( x, y, z, channel, S_FindSound(sound), (float)volume / 127.f, NETWORK_AttenuationIntToFloat ( attenuation ) );
}

//*****************************************************************************
//
void ServerCommands::AnnouncerSound::Execute()
{
	ANNOUNCER_PlayEntry( cl_announcer, sound );
}

//*****************************************************************************
//
void ServerCommands::StartSectorSequence::Execute()
{
	SN_StartSequence( sector, channel, sequence.GetChars(), modeNum );
}

//*****************************************************************************
//
void ServerCommands::StopSectorSequence::Execute()
{
	// [BB] We don't know which channel is supposed to stop, so just stop floor and ceiling for now.
	SN_StopSequence( sector, CHAN_CEILING );
	SN_StopSequence( sector, CHAN_FLOOR );
}

//*****************************************************************************
//
void ServerCommands::CallVote::Execute()
{
	CALLVOTE_BeginVote( command, parameters, reason, player - players );
}

//*****************************************************************************
//
void ServerCommands::PlayerVote::Execute()
{
	if ( voteYes )
		CALLVOTE_VoteYes( player - players );
	else
		CALLVOTE_VoteNo( player - players );
}

//*****************************************************************************
//
void ServerCommands::VoteEnded::Execute()
{
	CALLVOTE_EndVote( votePassed );
}

//*****************************************************************************
//
void ServerCommands::MapLoad::Execute()
{
	// [BB] We are about to change the map, so if we are playing a demo right now
	// and wanted to skip the current map, we are done with it now.
	CLIENTDEMO_SetSkippingToNextMap ( false );

	// Check to see if we have the map.
	if ( P_CheckIfMapExists( mapName ))
	{
		// Save our demo recording status since G_InitNew resets it.
		bool playing = CLIENTDEMO_IsPlaying( );

		// Start new level.
		G_InitNew( mapName, false );

		// [BB] We'll receive a full update for the new map from the server.
		g_bFullUpdateIncomplete = true;

		// Restore our demo recording status.
		CLIENTDEMO_SetPlaying( playing );

		// [BB] viewactive is set in G_InitNew
		// For right now, the view is not active.
		//viewactive = false;

		// Kill the console.
		C_HideConsole( );
	}
	else
		CLIENT_PrintWarning( "client_MapLoad: Unknown map: %s\n", mapName.GetChars() );
}

//*****************************************************************************
//
void ServerCommands::MapNew::Execute()
{
	// [BB] We are about to change the map, so if we are playing a demo right now
	// and wanted to skip the current map, we are done with it now.
	if ( CLIENTDEMO_IsSkippingToNextMap() == true )
	{
		// [BB] All the skipping seems to mess up the information which player is in game.
		// Clearing everything will take care of this.
		CLIENT_ClearAllPlayers();
		CLIENTDEMO_SetSkippingToNextMap ( false );
	}
	
	// [AK] Reset the map rotation before reconnecting to the server.
	MAPROTATION_Construct( );

	// Clear out our local buffer.
	g_LocalBuffer.Clear();

	// Back to the full console.
	gameaction = ga_fullconsole;

	// Also, the view is no longer active.
	viewactive = false;

	Printf( "Connecting to %s\n%s\n", g_AddressServer.ToString(), mapName.GetChars() );

	// Update the connection state, and begin trying to reconnect.
	CLIENT_SetConnectionState( CTS_ATTEMPTINGCONNECTION );
	CLIENT_AttemptConnection( );
}

//*****************************************************************************
//
void ServerCommands::MapExit::Execute()
{
	// [BB] We are about to change the map, so if we are playing a demo right now
	// and wanted to skip the current map, we are done with it now.
	CLIENTDEMO_SetSkippingToNextMap ( false );

	if (( gamestate == GS_FULLCONSOLE ) ||
		( gamestate == GS_INTERMISSION ))
	{
		return;
	}

	G_ChangeLevel( nextMap, position, true );
}

//*****************************************************************************
//
void ServerCommands::MapAuthenticate::Execute()
{
	// Nothing to do in demo mode.
	if ( CLIENTDEMO_IsPlaying( ))
		return;

	NETWORK_WriteByte( &g_LocalBuffer.ByteStream, CLC_AUTHENTICATELEVEL );

	// [BB] Send the name of the map we are authenticating, this allows the
	// server to check whether we try to authenticate the correct map.
	NETWORK_WriteString( &g_LocalBuffer.ByteStream, mapName );

	// Send a checksum of our verticies, linedefs, sidedefs, and sectors.
	CLIENT_AuthenticateLevel( mapName );
}

//*****************************************************************************
//
void ServerCommands::SetMapTime::Execute()
{
	level.time = time;
}

//*****************************************************************************
//
void ServerCommands::SetMapNumKilledMonsters::Execute()
{
	level.killed_monsters = killedMonsters;
}

//*****************************************************************************
//
void ServerCommands::SetMapNumFoundItems::Execute()
{
	level.found_items = foundItems;
}

//*****************************************************************************
//
void ServerCommands::SetMapNumFoundSecrets::Execute()
{
	level.found_secrets = foundSecrets;
}

//*****************************************************************************
//
void ServerCommands::SetMapNumTotalMonsters::Execute()
{
	level.total_monsters = totalMonsters;
}

//*****************************************************************************
//
void ServerCommands::SetMapNumTotalItems::Execute()
{
	level.total_items = totalItems;
}

//*****************************************************************************
//
void ServerCommands::SetMapNumTotalSecrets::Execute()
{
	level.total_secrets = totalSecrets;
}

//*****************************************************************************
//
void ServerCommands::SetMapMusic::Execute()
{
	if ( strcmp(music.GetChars(), "DEFAULT") )
		S_ChangeMusic( level.Music, level.musicorder );
	else
		S_ChangeMusic( music, order );
}

//*****************************************************************************
//
void ServerCommands::SetMapSky::Execute()
{
	strncpy( level.skypic1, sky1, 8 );
	sky1texture = TexMan.GetTexture( sky1, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable );

	strncpy( level.skypic2, sky2, 8 );
	sky2texture = TexMan.GetTexture( sky2, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable );

	// Set some other sky properties.
	R_InitSkyMap( );
}

//*****************************************************************************
//
void ServerCommands::GiveInventory::Execute()
{
	const PClass	*pType;
	AInventory		*pInventory;

	// Check to make sure everything is valid. If not, break out.
	if ( player->mo == NULL )
		return;

	pType = NETWORK_GetClassFromIdentification( actorNetworkIndex );
	if ( pType == NULL )
		return;

	// [BB] If pType is not derived from AInventory, the cast below will fail.
	if ( !(pType->IsDescendantOf( RUNTIME_CLASS( AInventory ))) )
		return;

	// [BB] Keep track of whether the player had a weapon.
	const bool playerHadNoWeapon = ( ( player->ReadyWeapon == NULL ) &&  ( player->PendingWeapon == WP_NOCHANGE ) );

	// Try to give the player the item.
	pInventory = static_cast<AInventory *>( Spawn( pType, 0, 0, 0, NO_REPLACE ));
	if ( pInventory != NULL )
	{
		if ( amount > 0 )
		{
			if ( pType->IsDescendantOf( RUNTIME_CLASS( ABasicArmorPickup )))
			{
				if ( static_cast<ABasicArmorPickup*>( pInventory )->SaveAmount != 0 )
					static_cast<ABasicArmorPickup*>( pInventory )->SaveAmount *= amount;
				else
					static_cast<ABasicArmorPickup*>( pInventory )->SaveAmount *= amount;
			}
			else if ( pType->IsDescendantOf( RUNTIME_CLASS( ABasicArmorBonus )))
			{
				static_cast<ABasicArmorBonus*>( pInventory )->SaveAmount *= amount;
				static_cast<ABasicArmorBonus*>( pInventory )->BonusCount *= amount;
			}
			else if ( pType->IsDescendantOf( RUNTIME_CLASS( AHealth ) ) )
			{
				if ( pInventory->MaxAmount > 0 )
					pInventory->Amount = MIN( amount, pInventory->MaxAmount );
				else
					pInventory->Amount = amount;
			}
			else
				pInventory->Amount = amount;

			if ( randomSeed >= 0 )
				pInventory->SetRandomSeed( randomSeed );
		}
		if ( pInventory->CallTryPickup( player->mo ) == false )
		{
			pInventory->Destroy( );
			pInventory = NULL;
		}
	}

	if ( pInventory == NULL )
	{
		// If he still doesn't have the object after trying to give it to him... then YIKES!
		if ( ( pType->IsDescendantOf( RUNTIME_CLASS( ABasicArmorPickup ) ) == false ) 
			 && ( pType->IsDescendantOf( RUNTIME_CLASS( ABasicArmorBonus ) ) == false )
			 && ( pType->IsDescendantOf( RUNTIME_CLASS( AHealth ) ) == false )
			 && ( player->mo->FindInventory( pType ) == NULL ) )
			CLIENT_PrintWarning( "ServerCommands::GiveInventory: Failed to give inventory type, %s!\n", NETWORK_GetClassNameFromIdentification( actorNetworkIndex ));
		return;
	}

	// Don't count this towards the level statistics.
	if ( pInventory->flags & MF_COUNTITEM )
	{
		pInventory->flags &= ~MF_COUNTITEM;
		level.total_items--;
	}

	// Set the new amount of the inventory object.
	pInventory = player->mo->FindInventory( pType );
	if ( pInventory )
		pInventory->Amount = amount;

	// [BC] For some weird reason, the KDIZD new pistol has the amount of 0 when
	// picked up, so we can't actually destroy items when the amount is 0 or less.
	// Doesn't seem to make any sense, but whatever.
/*
	if ( pInventory->Amount <= 0 )
	{
		// We can't actually destroy ammo, since it's vital for weapons.
		if ( pInventory->ItemFlags & IF_KEEPDEPLETED )
			pInventory->Amount = 0;
		// But, we can destroy everything else.
		else
			pInventory->Destroy( );
	}
*/
	// [BB] Prevent the client from trying to switch to a different weapon while morphed.
	if ( player->morphTics && !( player->mo && (player->mo->PlayerFlags & PPF_NOMORPHLIMITATIONS) ) )
		player->PendingWeapon = WP_NOCHANGE;

	// Since an item displayed on the HUD may have been given, refresh the HUD.
	SCOREBOARD_RefreshHUD( );

	// [BB] If this is not "our" player and this player didn't have a weapon before, we assume
	// that he was just spawned and didn't tell the server yet which weapon he selected. In this
	// case make sure that this pickup doesn't cause him to bring up a weapon and wait for the
	// server to tell us which weapon the player uses.
	if ( playerHadNoWeapon  && ( player->bIsBot == false )&& ( player - players != (ULONG)consoleplayer ) )
		PLAYER_ClearWeapon ( player );
}

//*****************************************************************************
//
void ServerCommands::TakeInventory::Execute()
{
	const PClass	*pType;
	AInventory		*pInventory;

	// Check to make sure everything is valid. If not, break out.
	if ( player->mo == NULL )
		return;

	pType = NETWORK_GetClassFromIdentification( actorNetworkIndex );
	if ( pType == NULL )
		return;

	if ( pType->IsDescendantOf( RUNTIME_CLASS( AInventory )) == false )
		return;

	// Try to find this object within the player's personal inventory.
	pInventory = player->mo->FindInventory( pType );

	// [TP] If we're trying to set the item amount to 0, then destroy the item if the player has it.
	if ( amount <= 0 )
	{
		if ( pInventory )
		{
			if ( pInventory->ItemFlags & IF_KEEPDEPLETED )
				pInventory->Amount = 0;
			else
				pInventory->Destroy( );
		}
	}
	else if ( amount > 0 )
	{
		// If the player doesn't have this type, give it to him.
		if ( pInventory == NULL )
			pInventory = player->mo->GiveInventoryType( pType );

		// If he still doesn't have the object after trying to give it to him... then YIKES!
		if ( pInventory == NULL )
		{
			CLIENT_PrintWarning( "ServerCommands::TakeInventory: Failed to take inventory type, %s!\n", pType->TypeName.GetChars());
			return;
		}

		// Set the new amount of the inventory object.
		pInventory->Amount = amount;
	}

	// Since an item displayed on the HUD may have been taken away, refresh the HUD.
	SCOREBOARD_RefreshHUD( );
}

//*****************************************************************************
//
void ServerCommands::GivePowerup::Execute()
{
	const PClass	*pType;
	AInventory		*pInventory;

	// Check to make sure everything is valid. If not, break out.
	if ( player->mo == NULL )
		return;

	pType = NETWORK_GetClassFromIdentification( actorNetworkIndex );
	if ( pType == NULL )
		return;

	// If this isn't a powerup, just quit.
	if ( pType->IsDescendantOf( RUNTIME_CLASS( APowerup )) == false )
		return;

	// Try to find this object within the player's personal inventory.
	pInventory = player->mo->FindInventory( pType );

	// If the player doesn't have this type, give it to him.
	if ( pInventory == NULL )
		pInventory = player->mo->GiveInventoryType( pType );

	// If he still doesn't have the object after trying to give it to him... then YIKES!
	if ( pInventory == NULL )
	{
		CLIENT_PrintWarning( "ServerCommands::GivePowerup: Failed to give powerup, %s!\n", NETWORK_GetClassNameFromIdentification( actorNetworkIndex ));
		return;
	}

	// Set the new amount of the inventory object.
	pInventory->Amount = amount;
	if ( pInventory->Amount <= 0 )
	{
		pInventory->Destroy( );
		pInventory = NULL;
	}

	if ( pInventory )
	{
		static_cast<APowerup *>( pInventory )->EffectTics = effectTicks;

		// [TP]
		if ( isActiveRune )
			pInventory->Owner->Rune = static_cast<APowerup *>( pInventory );
	}

	// Since an item displayed on the HUD may have been given, refresh the HUD.
	SCOREBOARD_RefreshHUD( );
}


//*****************************************************************************
//
void ServerCommands::SetPowerupBlendColor::Execute()
{
	const PClass *pType = NETWORK_GetClassFromIdentification(actorNetworkIndex);
	if (pType == NULL)
		return;

	// If this isn't a powerup, just quit.
	if (pType->IsDescendantOf(RUNTIME_CLASS(APowerup)) == false)
		return;

	// Try to find this object within the player's personal inventory.
	AInventory *pInventory = player->mo->FindInventory(pType);

	// [WS] If the player has this powerup, set its blendcolor appropriately.
	if (pInventory)
		static_cast<APowerup*>(pInventory)->BlendColor = blendColor;
}

//*****************************************************************************
//
void ServerCommands::DoInventoryPickup::Execute()
{
	AInventory		*pInventory;

	static LONG			s_lLastMessageTic = 0;
	static FString		s_szLastMessage;

	// Check to make sure everything is valid. If not, break out.
	if ( player->mo == NULL )
		return;

	// If the player doesn't have this inventory item, break out.
	pInventory = static_cast<AInventory *>( Spawn( PClass::FindClass( className ), 0, 0, 0, NO_REPLACE ));
	if ( pInventory == NULL )
		return;

	// Don't count this towards the level statistics.
	if ( pInventory->flags & MF_COUNTITEM )
	{
		// [BB] The server doesn't tell us about itemcount updates,
		// so try to keep track of this locally.
		player->itemcount++;

		pInventory->flags &= ~MF_COUNTITEM;
		level.total_items--;
	}

	// Print out the pickup message.
	if (( player->mo->CheckLocalView( consoleplayer )) &&
		(( s_lLastMessageTic != gametic ) || ( s_szLastMessage.CompareNoCase( pickupMessage ) != 0 )))
	{
		s_lLastMessageTic = gametic;
		s_szLastMessage = pickupMessage;

		// This code is from PrintPickupMessage().
		if ( pickupMessage.IsNotEmpty( ) )
		{
			if ( pickupMessage[0] == '$' )
				pickupMessage = GStrings( pickupMessage.GetChars( ) + 1 );

			Printf( PRINT_LOW, "%s\n", pickupMessage.GetChars( ) );
		}

		StatusBar->FlashCrosshair( );
	}

	// Play the inventory pickup sound and blend the screen.
	pInventory->PlayPickupSound( player->mo );
	player->bonuscount = BONUSADD;

	// Play the announcer pickup entry as well.
	if ( player->mo->CheckLocalView( consoleplayer ) && cl_announcepickups )
		ANNOUNCER_PlayEntry( cl_announcer, pInventory->PickupAnnouncerEntry( ));

	// Finally, destroy the temporarily spawned inventory item.
	pInventory->Destroy( );
}

//*****************************************************************************
//
void ServerCommands::DestroyAllInventory::Execute()
{
	// Check to make sure everything is valid. If not, break out.
	if ( player->mo == NULL )
		return;

	// Finally, destroy the player's inventory.
	// [BB] Be careful here, we may not use mo->DestroyAllInventory( ), otherwise
	// AHexenArmor messes up.
	player->mo->ClearInventory();
}

//*****************************************************************************
//
void ServerCommands::SetInventoryIcon::Execute()
{
	// Check to make sure everything is valid. If not, break out.
	if ( player->mo == NULL )
		return;

	const PClass *pType = NETWORK_GetClassFromIdentification( actorNetworkIndex );
	if ( pType == NULL )
		return;

	AInventory *pInventory = player->mo->FindInventory( pType );

	if ( pInventory )
		pInventory->Icon = TexMan.GetTexture( iconTexName, 0 );
}

//*****************************************************************************
//
void ServerCommands::DoDoor::Execute()
{
	DDoor *pDoor = P_GetDoorBySectorNum( sectorID );
	
	sector_t *pSector = NULL;

	if (( sectorID >= 0 ) && ( sectorID < numsectors ))
		pSector = &sectors[sectorID];
	
	// Invalid sector.
	if ( !pSector )
		return;

	// If the sector already has activity, don't override it.
	if ( !pDoor && pSector->ceilingdata )
		return;
	
	if (pDoor == NULL)
	{
		// Create the new door.
		pDoor = new DDoor( pSector, (DDoor::EVlDoor)type, speed, topWait, lightTag, g_ConnectionState != CTS_ACTIVE );
	}
	else
	{
		pDoor->SetType( (DDoor::EVlDoor)type );
		pDoor->SetSpeed( speed );
		pDoor->SetTopWait( topWait );
		pDoor->SetLightTag( lightTag );
	}

	pDoor->SetLastInstigator( &players[instigator] );
	P_SetCeilingPlane( pSector, position );
	pDoor->SetDirection( CLIENT_AdjustDoorDirection ( direction ) );
	pDoor->SetCountdown( countdown );

	if ( instigator == consoleplayer )
	{
		pDoor->Predict();
	}
}

//*****************************************************************************
//
void ServerCommands::DestroyDoor::Execute()
{
	DDoor *pDoor = P_GetDoorBySectorNum( sectorID );

	sector_t *pSector = NULL;

	if (( sectorID >= 0 ) && ( sectorID < numsectors ))
		pSector = &sectors[sectorID];

	// Invalid sector.
	if ( !pSector )
		return;

	// Door doesn't exist
	if ( !pDoor )
		return;

	// Door is destroyed on server side
	P_SetCeilingPlane( pSector, position );
	pDoor->SetDirection( 0 );
	pDoor->Destroy();
}

//*****************************************************************************
//
void ServerCommands::DoFloor::Execute()
{
	DFloor *pFloor = P_GetFloorBySectorNum( sectorID );
	
	sector_t *pSector = NULL;
	DWaggleBase	*pWaggle = NULL;

	if (( sectorID >= 0 ) && ( sectorID < numsectors ))
		pSector = &sectors[sectorID];
	
	// Invalid sector.
	if ( !pSector )
		return;

	// If the sector already has activity, don't override it.
	if ( !pFloor && pSector->floordata )
		return;
	
	if (pFloor == NULL )
	{
		// Create the new floor.
		pFloor = new DFloor( pSector );
	}

	pFloor->SetLastInstigator( &players[instigator] );
	pFloor->SetType( (DFloor::EFloor)type );
	pFloor->SetCrush( static_cast<SBYTE>( crush ) );
	pFloor->SetHexencrush( hexenCrush );
	pFloor->SetOrgDist( orgDist );
	pFloor->SetFloorDestDist( destDist );
	P_SetFloorPlane( pSector, position );
	pFloor->SetDirection( CLIENT_AdjustFloorDirection( direction ) );
	pFloor->SetSpeed( speed );
	pFloor->SetNewSpecial( newSpecial );
	pFloor->SetTexture( TexMan.GetTexture( texture, FTexture::TEX_Flat ) );

	if ( instigator == consoleplayer )
	{
		pFloor->Predict();
	}
}

//*****************************************************************************
//
void ServerCommands::DestroyFloor::Execute()
{
	DFloor *pFloor = P_GetFloorBySectorNum( sectorID );
	
	sector_t *pSector = NULL;
	DWaggleBase	*pWaggle = NULL;

	if (( sectorID >= 0 ) && ( sectorID < numsectors ))
		pSector = &sectors[sectorID];
	
	// Invalid sector.
	if ( !pSector )
		return;

	// Floor doesn't exist
	if ( !pFloor )
		return;

	// Floor is destroyed on server side
	P_SetFloorPlane( pSector, position );
	pFloor->SetDirection( 0 );
	pFloor->Destroy();
}

//*****************************************************************************
//
void ServerCommands::BuildStair::Execute()
{
	DFloor *pFloor = P_GetFloorBySectorNum( sectorID );

	sector_t *pSector = NULL;

	if (( sectorID >= 0 ) && ( sectorID < numsectors ))
		pSector = &sectors[sectorID];

	// Invalid sector.
	if ( !pSector )
		return;

	// If the sector already has activity, don't override it.
	if ( !pFloor && pSector->floordata )
		return;
	
	if ( pFloor == NULL )
	{
		// Create the new floor.
		pFloor = new DFloor( pSector );
	}

	pFloor->SetLastInstigator( &players[instigator] );
	pFloor->SetType( (DFloor::EFloor)type );
	pFloor->SetCrush( static_cast<SBYTE>( crush ) );
	pFloor->SetHexencrush( hexenCrush );
	pFloor->SetOrgDist( orgDist );
	pFloor->SetFloorDestDist( destDist );
	P_SetFloorPlane( pSector, position );
	pFloor->SetDirection( CLIENT_AdjustFloorDirection( direction ) );
	pFloor->SetSpeed( speed );
	pFloor->SetNewSpecial( newSpecial );
	pFloor->SetResetCount( resetCount );
	pFloor->SetDelay( delay );
	pFloor->SetPauseTime( pauseTime );
	pFloor->SetStepTime( stepTime );
	pFloor->SetPerStepTime( perStepTime );
	pFloor->SetTexture( TexMan.GetTexture( texture, FTexture::TEX_Flat ) );

	if ( instigator == consoleplayer )
	{
		pFloor->Predict();
	}
}

//*****************************************************************************
//
void ServerCommands::DestroyStair::Execute()
{
	DFloor *pFloor = P_GetFloorBySectorNum( sectorID );
	
	sector_t *pSector = NULL;

	if (( sectorID >= 0 ) && ( sectorID < numsectors ))
		pSector = &sectors[sectorID];
	
	// Invalid sector.
	if ( !pSector )
		return;

	// Floor doesn't exist
	if ( !pFloor )
		return;

	// Floor is destroyed on server side
	P_SetFloorPlane( pSector, position );
	pFloor->SetDirection( 0 );
	pFloor->Destroy();
}

//*****************************************************************************
//
void ServerCommands::DoCeiling::Execute()
{
	DCeiling *pCeiling = P_GetCeilingBySectorNum( sectorID );

	sector_t *pSector = NULL;

	if (( sectorID >= 0 ) && ( sectorID < numsectors ))
		pSector = &sectors[sectorID];

	// Invalid sector.
	if ( !pSector )
		return;

	// If the sector already has activity, don't override it.
	if ( !pCeiling && pSector->ceilingdata )
		return;
	
	if ( pCeiling == NULL )
	{
		// Create the new ceiling.
		pCeiling = new DCeiling( pSector );
	}

	pCeiling->SetLastInstigator( &players[instigator] );
	pCeiling->SetTag( tag );
	pCeiling->SetType( (DCeiling::ECeiling)type );
	pCeiling->SetBottomHeight( bottomHeight );
	pCeiling->SetTopHeight( topHeight );
	P_SetCeilingPlane( pSector, position );
	pCeiling->SetDirection( CLIENT_AdjustCeilingDirection( direction ) );
	pCeiling->SetOldDirection( CLIENT_AdjustCeilingDirection( oldDirection ) );
	pCeiling->SetSpeed( speed );
	pCeiling->SetSpeed1( speed1 );
	pCeiling->SetSpeed2( speed2 );
	pCeiling->SetCrush( static_cast<SBYTE>( crush ) );
	pCeiling->SetHexencrush( hexenCrush );
	pCeiling->SetSilent( silent );
	pCeiling->SetTexture( TexMan.GetTexture( texture, FTexture::TEX_Flat ) );

	if ( instigator == consoleplayer )
	{
		pCeiling->Predict();
	}
}

//*****************************************************************************
//
void ServerCommands::DestroyCeiling::Execute()
{
	DCeiling *pCeiling = P_GetCeilingBySectorNum( sectorID );
	
	sector_t *pSector = NULL;

	if (( sectorID >= 0 ) && ( sectorID < numsectors ))
		pSector = &sectors[sectorID];
	
	// Invalid sector.
	if ( !pSector )
		return;

	// Ceiling doesn't exist
	if ( !pCeiling )
		return;

	// Ceiling is destroyed on server side
	P_SetCeilingPlane( pSector, position );
	pCeiling->SetDirection( 0 );
	pCeiling->Destroy();
}

//*****************************************************************************
//
void ServerCommands::DoPlat::Execute()
{
	DPlat *pPlat = P_GetPlatBySectorNum( sectorID );

	sector_t *pSector = NULL;

	if (( sectorID >= 0 ) && ( sectorID < numsectors ))
		pSector = &sectors[sectorID];
	
	// Invalid sector.
	if ( !pSector )
		return;

	// If the sector already has activity, don't override it.
	if ( !pPlat && pSector->ceilingdata )
		return;
	
	if (pPlat == NULL)
	{
		// Create the new door.
		pPlat = new DPlat( pSector );
	}

	pPlat->SetLastInstigator( &players[instigator] );
	pPlat->SetType( (DPlat::EPlatType)type );
	pPlat->SetStatus( status );
	pPlat->SetOldStatus( oldStatus );
	pPlat->SetSpeed( speed );
	pPlat->SetHigh( high );
	pPlat->SetLow( low );
	P_SetFloorPlane( pSector, position );
	pPlat->SetWait( wait );
	pPlat->SetCount( count );
	pPlat->SetCrush( crush );
	pPlat->SetTag( tag );

	if ( instigator == consoleplayer )
	{
		pPlat->Predict();
	}
}

//*****************************************************************************
//
void ServerCommands::DestroyPlat::Execute()
{
	DPlat *pPlat = P_GetPlatBySectorNum( sectorID );

	sector_t *pSector = NULL;

	if (( sectorID >= 0 ) && ( sectorID < numsectors ))
		pSector = &sectors[sectorID];

	// Invalid sector.
	if ( !pSector )
		return;

	// Plat doesn't exist
	if ( !pPlat )
		return;

	// Plat is destroyed on server side
	P_SetFloorPlane( pSector, position );
	pPlat->Destroy();
}

//*****************************************************************************
//
void ServerCommands::DoElevator::Execute()
{
	DElevator *pElevator = P_GetElevatorBySectorNum( sectorID );

	sector_t *pSector = NULL;

	if (( sectorID >= 0 ) && ( sectorID < numsectors ))
		pSector = &sectors[sectorID];

	// Invalid sector.
	if ( !pSector )
		return;

	// If the sector already has activity, don't override it.
	if ( !pElevator && (pSector->floordata || pSector->ceilingdata) )
		return;
	
	if (pElevator == NULL)
	{
		// Create the new elevator.
		pElevator = new DElevator( pSector );
	}

	pElevator->SetLastInstigator( &players[instigator] );
	pElevator->SetType( (DElevator::EElevator)type );
	pElevator->SetSpeed( speed );
	pElevator->SetDirection( CLIENT_AdjustElevatorDirection( direction ) );
	P_SetFloorPlane( pSector, floorPosition );
	P_SetCeilingPlane( pSector, ceilingPosition );
	pElevator->SetFloorDestDist( floorDestDist );
	pElevator->SetCeilingDestDist( ceilingDestDist );

	if ( instigator == consoleplayer )
	{
		pElevator->Predict();
	}
}

//*****************************************************************************
//
void ServerCommands::DestroyElevator::Execute()
{
	DElevator *pElevator = P_GetElevatorBySectorNum( sectorID );
	
	sector_t *pSector = NULL;

	if (( sectorID >= 0 ) && ( sectorID < numsectors ))
		pSector = &sectors[sectorID];
	
	// Invalid sector.
	if ( !pSector )
		return;

	// Elevator doesn't exist
	if ( !pElevator )
		return;

	// Elevator is destroyed on server side
	pElevator->SetDirection( 0 );
	P_SetFloorPlane( pSector, floorPosition );
	P_SetCeilingPlane( pSector, ceilingPosition );
	pElevator->Destroy();
}

//*****************************************************************************
//
void ServerCommands::DoPillar::Execute()
{
	DPillar	*pPillar = P_GetPillarBySectorNum( sectorID );

	sector_t *pSector = NULL;

	if (( sectorID >= 0 ) && ( sectorID < numsectors ))
		pSector = &sectors[sectorID];

	// Invalid sector.
	if ( !pSector )
		return;

	// If the sector already has activity, don't override it.
	if ( !pPillar && (pSector->floordata || pSector->ceilingdata) )
		return;

	if (pPillar == NULL)
	{
		// Create the new pillar.
		pPillar = new DPillar( pSector );
	}

	pPillar->SetLastInstigator( &players[instigator] );
	pPillar->SetType( (DPillar::EPillar)type );
	P_SetFloorPlane( pSector, floorPosition );
	P_SetCeilingPlane( pSector, ceilingPosition );
	pPillar->SetFloorSpeed( floorSpeed );
	pPillar->SetCeilingSpeed( ceilingSpeed );
	pPillar->SetFloorTarget( floorTarget );
	pPillar->SetCeilingTarget( ceilingTarget );
	pPillar->SetHexencrush( hexenCrush );

	// Begin playing the sound sequence for the pillar.
	if ( pSector->seqType >= 0 )
		SN_StartSequence( pSector, CHAN_FLOOR, pSector->seqType, SEQ_PLATFORM, 0 );
	else
		SN_StartSequence( pSector, CHAN_FLOOR, "Floor", 0 );

	if ( instigator == consoleplayer )
	{
		pPillar->Predict();
	}
}

//*****************************************************************************
//
void ServerCommands::DestroyPillar::Execute()
{
	DPillar	*pPillar = P_GetPillarBySectorNum( sectorID );
	
	sector_t *pSector = NULL;

	if (( sectorID >= 0 ) && ( sectorID < numsectors ))
		pSector = &sectors[sectorID];

	// Invalid sector.
	if ( !pSector )
		return;

	// Pillar doesn't exist
	if ( !pPillar )
		return;

	// Pillar is destroyed on server side
	P_SetFloorPlane( pSector, floorPosition );
	P_SetCeilingPlane( pSector, ceilingPosition );
	pPillar->Destroy();
}

//*****************************************************************************
//
void ServerCommands::DoWaggle::Execute()
{
	DWaggleBase	*pWaggle = P_GetWaggleBySectorNum( sectorID, isCeilingWaggle );

	sector_t *pSector = NULL;

	if (( sectorID >= 0 ) && ( sectorID < numsectors ))
		pSector = &sectors[sectorID];

	// Invalid sector.
	if ( !pSector )
		return;

	// If the sector already has activity, don't override it.
	if ( !pWaggle && ( isCeilingWaggle ? pSector->ceilingdata : pSector->floordata ))
		return;

	if (pWaggle == NULL)
	{
		// Create the waggle
		if ( isCeilingWaggle )
			pWaggle = static_cast<DWaggleBase *>( new DCeilingWaggle( pSector ));
		else
			pWaggle = static_cast<DWaggleBase *>( new DFloorWaggle( pSector ));
	}

	pWaggle->SetLastInstigator( &players[instigator] );
	if ( isCeilingWaggle )
		P_SetCeilingPlane( pSector, position );
	else
		P_SetFloorPlane( pSector, position );
	pWaggle->SetOriginalDistance( originalDistance );
	pWaggle->SetAccumulator( accumulator );
	pWaggle->SetAccelerationDelta( accelerationDelta );
	pWaggle->SetTargetScale( targetScale );
	pWaggle->SetScale( scale );
	pWaggle->SetScaleDelta( scaleDelta );
	pWaggle->SetTicker( ticker );
	pWaggle->SetState( state );

	if ( instigator == consoleplayer )
	{
		pWaggle->Predict();
	}
}

//*****************************************************************************
//
void ServerCommands::DestroyWaggle::Execute()
{
	DWaggleBase	*pWaggle = P_GetWaggleBySectorNum( sectorID, isCeilingWaggle );

	sector_t *pSector = NULL;

	if (( sectorID >= 0 ) && ( sectorID < numsectors ))
		pSector = &sectors[sectorID];

	// Invalid sector.
	if ( !pSector )
		return;

	// Waggle doesn't exist
	if ( !pWaggle )
		return;

	// Waggle is destroyed on server side
	if ( isCeilingWaggle )
		P_SetCeilingPlane( pSector, position );
	else
		P_SetFloorPlane( pSector, position );
	pWaggle->Destroy();
}

//*****************************************************************************
//
void ServerCommands::DoRotatePoly::Execute()
{
	// Make sure the polyobj exists before we try to work with it.
	FPolyObj *pPoly = PO_GetPolyobj( polyObj );
	if ( pPoly == NULL )
	{
		CLIENT_PrintWarning( "client_DoMovePoly: Invalid polyobj number: %d\n", polyObj );
		return;
	}

	DRotatePoly *pRotatePoly = NULL;

	if ( pPoly->specialdata != NULL )
	{
		if ( pPoly->specialdata->IsKindOf(RUNTIME_CLASS(DRotatePoly)) )
			// Reuse existing polyobject
			pRotatePoly = (DRotatePoly*) pPoly->specialdata;
		else
			// Some other polyobject uses this poly
			return;
	}

	if (pRotatePoly == NULL )
	{
		// Create the polyobject.
		pRotatePoly = new DRotatePoly( polyObj );
		pPoly->specialdata = pRotatePoly;
		
		if ( instigator != consoleplayer )
		{
			// Start the sound sequence associated with this polyobject.
			SN_StartSequence( pPoly, pPoly->seqType, SEQ_DOOR, 0 );
		}
	}
	
	pRotatePoly->SetLastInstigator( &players[instigator] );
	pRotatePoly->SetSpeed( speed );
	pRotatePoly->SetDist( dist );

	if ( instigator == consoleplayer )
	{
		pRotatePoly->Predict();
	}
}

//*****************************************************************************
//
void ServerCommands::DoMovePoly::Execute()
{
	// Make sure the polyobj exists before we try to work with it.
	FPolyObj *pPoly = PO_GetPolyobj( polyObj );
	if ( pPoly == NULL )
	{
		CLIENT_PrintWarning( "client_DoMovePoly: Invalid polyobj number: %d\n", polyObj );
		return;
	}

	DMovePoly *pMovePoly = NULL;

	if ( pPoly->specialdata != NULL )
	{
		if ( pPoly->specialdata->IsKindOf(RUNTIME_CLASS(DMovePoly)) )
			// Reuse existing polyobject
			pMovePoly = (DMovePoly*) pPoly->specialdata;
		else
			// Some other polyobject uses this poly
			return;
	}

	if ( pMovePoly == NULL )
	{
		// Create the polyobject.
		pMovePoly = new DMovePoly( polyObj );
		pPoly->specialdata = pMovePoly;
		
		if ( instigator != consoleplayer )
		{
			// Start the sound sequence associated with this polyobject.
			SN_StartSequence( pPoly, pPoly->seqType, SEQ_DOOR, 0 );
		}
	}
	
	pMovePoly->SetLastInstigator( &players[instigator] );
	pMovePoly->SetXSpeed( xSpeed );
	pMovePoly->SetYSpeed( ySpeed );
	pMovePoly->SetSpeed( speed );
	pMovePoly->SetAngle( angle );
	pMovePoly->SetDist( dist );

	if ( instigator == consoleplayer )
	{
		pMovePoly->Predict();
	}
}

//*****************************************************************************
//
void ServerCommands::DoMovePolyTo::Execute()
{
	// Make sure the polyobj exists before we try to work with it.
	FPolyObj *pPoly = PO_GetPolyobj( polyObj );
	if ( pPoly == NULL )
	{
		CLIENT_PrintWarning( "client_DoMovePolyTo: Invalid polyobj number: %d\n", polyObj );
		return;
	}

	DMovePolyTo	*pMovePolyTo = NULL;

	if ( pPoly->specialdata != NULL )
	{
		if ( pPoly->specialdata->IsKindOf(RUNTIME_CLASS(DMovePolyTo)) )
			// Reuse existing polyobject
			pMovePolyTo = (DMovePolyTo*) pPoly->specialdata;
		else
			// Some other polyobject uses this poly
			return;
	}

	if (pMovePolyTo == NULL )
	{
		// Create the polyobject.
		pMovePolyTo = new DMovePolyTo( polyObj );
		pPoly->specialdata = pMovePolyTo;
		
		if ( instigator != consoleplayer )
		{
			// Start the sound sequence associated with this polyobject.
			SN_StartSequence( pPoly, pPoly->seqType, SEQ_DOOR, 0 );
		}
	}
	
	pMovePolyTo->SetLastInstigator( &players[instigator] );
	pMovePolyTo->SetXTarget( xTarget );
	pMovePolyTo->SetYTarget( yTarget );
	pMovePolyTo->SetXSpeed( xSpeed );
	pMovePolyTo->SetYSpeed( ySpeed );
	pMovePolyTo->SetSpeed( speed );
	pMovePolyTo->SetDist( dist );

	if ( instigator == consoleplayer )
	{
		pMovePolyTo->Predict();
	}
}

//*****************************************************************************
//
void ServerCommands::DoPolyDoor::Execute()
{
	// Make sure the polyobj exists before we try to work with it.
	FPolyObj *pPoly = PO_GetPolyobj( polyObj );
	if ( pPoly == NULL )
	{
		CLIENT_PrintWarning( "client_DoPolyDoor: Invalid polyobj number: %d\n", polyObj );
		return;
	}

	DPolyDoor *pPolyDoor = NULL;

	if ( pPoly->specialdata != NULL )
	{
		if ( pPoly->specialdata->IsKindOf(RUNTIME_CLASS(DPolyDoor)) )
			// Reuse existing polyobject
			pPolyDoor = (DPolyDoor*) pPoly->specialdata;
		else
			// Some other polyobject uses this poly
			return;
	}

	if ( pPolyDoor == NULL )
	{
		// Create the polyobject.
		pPolyDoor = new DPolyDoor( polyObj, (podoortype_t)type );
		pPoly->specialdata = pPolyDoor;
	}
	
	pPolyDoor->SetLastInstigator( &players[instigator] );

	fixed_t DeltaX = x - pPoly->StartSpot.x;
	fixed_t DeltaY = y - pPoly->StartSpot.y;
	pPoly->MovePolyobj( DeltaX, DeltaY );

	pPolyDoor->SetXSpeed( xSpeed );
	pPolyDoor->SetYSpeed( ySpeed );
	pPolyDoor->SetSpeed( speed );

	angle_t DeltaAngle = angle - pPoly->angle;
	pPoly->RotatePolyobj(DeltaAngle);

	pPolyDoor->SetDist( dist );
	pPolyDoor->SetTotalDist( totalDist );

	pPolyDoor->SetTics( tics );
	pPolyDoor->SetWaitTics( waitTics );
	
	pPolyDoor->SetClose( close );
	pPolyDoor->SetDirection( direction );

	if ( instigator == consoleplayer )
	{
		pPolyDoor->Predict();
	}
	else
	{
		// Start the sound sequence associated with this polyobject.
		if ( tics == 0 )
		{
			SN_StartSequence( pPoly, pPoly->seqType, SEQ_DOOR, 0 );
		}
		else
		{
			SN_StopSequence( pPoly );
		}
	}
}

//*****************************************************************************
//
void ServerCommands::EarthQuake::Execute()
{
	// Create the earthquake. Since this is client-side, damage is always 0.
	new DEarthquake( center, intensity, duration, 0, tremorRadius, quakeSound );
}

//*****************************************************************************
//
void ServerCommands::DoScroller::Execute()
{
	// Check to make sure what we've read in is valid.
	// [BB] sc_side is allowed, too, but we need to make a different check for it.
	if (( type != DScroller::sc_floor ) && ( type != DScroller::sc_ceiling ) &&
		( type != DScroller::sc_carry ) && ( type != DScroller::sc_carry_ceiling ) && ( type != DScroller::sc_side ) )
	{
		CLIENT_PrintWarning( "client_DoScroller: Unknown type: %d!\n", static_cast<int> (type) );
		return;
	}

	if( type == DScroller::sc_side )
	{
		if (( affectee < 0 ) || ( affectee >= numsides ))
		{
			CLIENT_PrintWarning( "client_DoScroller: Invalid side ID: %d!\n", affectee );
			return;
		}
	}
	else if (( affectee < 0 ) || ( affectee >= numsectors ))
	{
		CLIENT_PrintWarning( "client_DoScroller: Invalid sector ID: %d!\n", affectee );
		return;
	}

	// Finally, create the scroller.
	new DScroller( (DScroller::EScrollType)type, xSpeed, ySpeed, -1, affectee, 0 );
}

//*****************************************************************************
//
// [BB] SetScroller is defined in p_lnspec.cpp.
void DoSetScroller (int tag, DScroller::EScrollType type, fixed_t dx, fixed_t dy);

void ServerCommands::SetScroller::Execute()
{
	// Check to make sure what we've read in is valid.
	if (( type != DScroller::sc_floor ) && ( type != DScroller::sc_ceiling ) &&
		( type != DScroller::sc_carry ) && ( type != DScroller::sc_carry_ceiling ))
	{
		CLIENT_PrintWarning( "client_SetScroller: Unknown type: %d!\n", static_cast<int> (type) );
		return;
	}

	// Finally, create or update the scroller.
	DoSetScroller (tag, (DScroller::EScrollType)type, xSpeed, ySpeed );
}

//*****************************************************************************
//
// [BB] SetWallScroller is defined in p_lnspec.cpp.
void DoSetWallScroller(int id, int sidechoice, fixed_t dx, fixed_t dy, int Where);

void ServerCommands::SetWallScroller::Execute()
{
	DoSetWallScroller (id, sideChoice, xSpeed, ySpeed, lWhere );
}

//*****************************************************************************
//
void ServerCommands::DoFlashFader::Execute()
{
	if ( player->mo )
		new DFlashFader( r1, g1, b1, a1, r2, g2, b2, a2, time, player->mo );
}

//*****************************************************************************
//
void ServerCommands::GenericCheat::Execute()
{
	cht_DoCheat( player, cheat );
}

//*****************************************************************************
//
void ServerCommands::SetCameraToTexture::Execute()
{
	FTextureID picNum = TexMan.CheckForTexture( texture, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable );
	if ( !picNum.Exists() )
	{
		CLIENT_PrintWarning( "ServerCommands::SetCameraToTexture: %s is not a texture\n", texture.GetChars() );
		return;
	}

	FCanvasTextureInfo::Add( camera, picNum, fov );
}

//*****************************************************************************
//
void ServerCommands::UpdateClientNetID::Execute()
{
	if ( ( netId > 0 ) && ( (int) ( netId / g_NetIDList.MAX_NETID_FOR_PLAYER ) == consoleplayer ) && ( netId >= players[consoleplayer].firstFreeNetId || netId == 1 || force ) ) {
		int oldfreenetid = players[consoleplayer].firstFreeNetId;
		players[consoleplayer].firstFreeNetId = netId % g_NetIDList.MAX_NETID_FOR_PLAYER;
		if (players[consoleplayer].firstFreeNetId >= g_NetIDList.MAX_NETID_FOR_PLAYER * (consoleplayer + 1)) {
			players[consoleplayer].firstFreeNetId = 1;
		}
	}
}

//*****************************************************************************
//
void ServerCommands::UpdateActorRandom::Execute()
{
	actor->SetRandomSeed( randomSeed );
}

//*****************************************************************************
//
void ServerCommands::CreateTranslation::Execute()
{
	EDITEDTRANSLATION_s	Translation = EDITEDTRANSLATION_s();
	FRemapTable	*pTranslation;

	// Read in which translation is being created.
	Translation.ulIdx = translation;

	const bool bIsEdited = isEdited;

	// Read in the range that's being translated.
	Translation.ulStart = start;
	Translation.ulEnd = end;

	Translation.ulPal1 = pal1;
	Translation.ulPal2 = pal2;
	Translation.ulType = DLevelScript::PCD_TRANSLATIONRANGE1;

	// [BB] We need to do this check here, otherwise the client could be crashed
	// by sending a SVC_CREATETRANSLATION packet with an illegal tranlation number.
	if ( Translation.ulIdx < 1 || Translation.ulIdx > MAX_ACS_TRANSLATIONS )
	{
		return;
	}

	pTranslation = translationtables[TRANSLATION_LevelScripted].GetVal(Translation.ulIdx - 1);

	if (pTranslation == NULL)
	{
		pTranslation = new FRemapTable;
		translationtables[TRANSLATION_LevelScripted].SetVal(Translation.ulIdx - 1, pTranslation);
		// [BB] It's mandatory to initialize the translation with the identity, because the server can only send
		// "bIsEdited == false" if the client is already in the game when the translation is created.
		pTranslation->MakeIdentity();
	}

	if ( bIsEdited == false )
		pTranslation->MakeIdentity();

	if ( Translation.ulType == DLevelScript::PCD_TRANSLATIONRANGE1 )
		pTranslation->AddIndexRange( Translation.ulStart, Translation.ulEnd, Translation.ulPal1, Translation.ulPal2 );
	else
		pTranslation->AddColorRange( Translation.ulStart, Translation.ulEnd, Translation.ulR1, Translation.ulG1, Translation.ulB1, Translation.ulR2, Translation.ulG2, Translation.ulB2 );
	pTranslation->UpdateNative();
}

//*****************************************************************************
//
void ServerCommands::CreateTranslation2::Execute()
{
	EDITEDTRANSLATION_s	Translation = EDITEDTRANSLATION_s();
	FRemapTable	*pTranslation;

	// Read in which translation is being created.
	Translation.ulIdx = translation;

	const bool bIsEdited = isEdited;

	// Read in the range that's being translated.
	Translation.ulStart = start;
	Translation.ulEnd = end;

	Translation.ulR1 = r1;
	Translation.ulG1 = g1;
	Translation.ulB1 = b1;
	Translation.ulR2 = r2;
	Translation.ulG2 = g2;
	Translation.ulB2 = b2;
	Translation.ulType = DLevelScript::PCD_TRANSLATIONRANGE2;

	// [BB] We need to do this check here, otherwise the client could be crashed
	// by sending a SVC_CREATETRANSLATION packet with an illegal tranlation number.
	if ( Translation.ulIdx < 1 || Translation.ulIdx > MAX_ACS_TRANSLATIONS )
	{
		return;
	}

	pTranslation = translationtables[TRANSLATION_LevelScripted].GetVal(Translation.ulIdx - 1);

	if (pTranslation == NULL)
	{
		pTranslation = new FRemapTable;
		translationtables[TRANSLATION_LevelScripted].SetVal(Translation.ulIdx - 1, pTranslation);
		// [BB] It's mandatory to initialize the translation with the identity, because the server can only send
		// "bIsEdited == false" if the client is already in the game when the translation is created.
		pTranslation->MakeIdentity();
	}

	if ( bIsEdited == false )
		pTranslation->MakeIdentity();

	if ( Translation.ulType == DLevelScript::PCD_TRANSLATIONRANGE1 )
		pTranslation->AddIndexRange( Translation.ulStart, Translation.ulEnd, Translation.ulPal1, Translation.ulPal2 );
	else
		pTranslation->AddColorRange( Translation.ulStart, Translation.ulEnd, Translation.ulR1, Translation.ulG1, Translation.ulB1, Translation.ulR2, Translation.ulG2, Translation.ulB2 );
	pTranslation->UpdateNative();
}

//*****************************************************************************
//
void ServerCommands::DoPusher::Execute()
{
	line_t *pLine = ( lineNum >= 0 && lineNum < numlines ) ? &lines[lineNum] : NULL;
	new DPusher ( static_cast<DPusher::EPusher> ( type ), pLine, magnitude, angle, sourceActor, affectee );
}

//*****************************************************************************
//
void DoAdjustPusher (int tag, int magnitude, int angle, DPusher::EPusher type);
void ServerCommands::AdjustPusher::Execute()
{
	DoAdjustPusher (tag, magnitude, angle, static_cast<DPusher::EPusher> ( type ));
}

//*****************************************************************************
//
void ServerCommands::SetPlayerHazardCount::Execute()
{
	player->hazardcount = hazardCount;
}

//*****************************************************************************
//
void ServerCommands::Scroll3dMidTex::Execute()
{
	if ( sectorNum < 0 || sectorNum >= numsectors || !move )
		return;

	P_Scroll3dMidtex( &sectors[sectorNum], 0, move, isCeiling );
}

//*****************************************************************************
//
void ServerCommands::SetPlayerLogNumber::Execute()
{
	if ( player->mo->FindInventory(NAME_Communicator) )
		player->SetLogNumber ( arg0 );
}

//*****************************************************************************
//
void ServerCommands::SetPlayerSkin::Execute()
{
	player->SkinOverride = skinName;
	player->overrideWeaponPreferredSkin = overrideWeaponSkin;
}

//*****************************************************************************
//
void ServerCommands::ReplaceTextures::Execute()
{
	DLevelScript::ReplaceTextures( fromTexture, toTexture, textureFlags );
}

//*****************************************************************************
//
void ServerCommands::SetCVar::Execute()
{
	// [TP] Only allow the server to set mod CVARs.
	FBaseCVar* cvar = FindCVar( name, NULL );

	if ( cvar == NULL )
	{
		CLIENT_PrintWarning( "SVC2_SETCVAR: The server attempted to set the value of "
			"%s to \"%s\"\n", name.GetChars(), value.GetChars() );
		return;
	}

	UCVarValue vval;
	vval.String = value;
	cvar->ForceSet( vval, CVAR_String );
}

//*****************************************************************************
//
void ServerCommands::SetMugShotState::Execute()
{
	if ( StatusBar != NULL)
	{
		StatusBar->SetMugShotState( stateName );
	}
}

//*****************************************************************************
//
void ServerCommands::SetDefaultSkybox::Execute()
{
	if ( skyboxActor == NULL )
		level.DefaultSkybox = NULL;
	else
		level.DefaultSkybox = static_cast<ASkyViewpoint *>( skyboxActor );
}

//*****************************************************************************
//
void APathFollower::InitFromStream ( BYTESTREAM_s *pByteStream )
{
	APathFollower *pPathFollower = static_cast<APathFollower*> ( CLIENT_FindThingByNetID( NETWORK_ReadShort( pByteStream ) ) );
	const int currNodeId = NETWORK_ReadShort( pByteStream );
	const int prevNodeId = NETWORK_ReadShort( pByteStream );
	const float serverTime = NETWORK_ReadFloat( pByteStream );

	if ( pPathFollower )
	{
		pPathFollower->lServerCurrNodeId = currNodeId;
		pPathFollower->lServerPrevNodeId = prevNodeId;
		pPathFollower->fServerTime = serverTime;
	}
	else
	{
		CLIENT_PrintWarning( "APathFollower::InitFromStream: Couldn't find actor.\n" );
		return;
	}
}

//*****************************************************************************
//
void ServerCommands::SyncJoinQueue::Execute()
{
	JOINQUEUE_ClearList();

	for ( unsigned int i = 0; i < slots.Size(); ++i )
		JOINQUEUE_AddPlayer( slots[i].player, slots[i].team );
}

//*****************************************************************************
//
void ServerCommands::SyncMapRotation::Execute()
{
	for ( unsigned int i = 0; i < entries.Size(); i++ )
		MAPROTATION_AddMap( entries[i].name, true, 0 );
}

//*****************************************************************************
//
void ServerCommands::PushToJoinQueue::Execute()
{
	JOINQUEUE_AddPlayer( playerNum, team );
}

//*****************************************************************************
//
void ServerCommands::RemoveFromJoinQueue::Execute()
{
	JOINQUEUE_RemovePlayerAtPosition( index );
}

//*****************************************************************************
//
void ServerCommands::UpdatePlayersCount::Execute()
{
	client_curplayers = curplayers;
	client_maxplayers = maxplayers;
	I_UpdateDiscordPresence(true);
}

//*****************************************************************************
//
void STACK_ARGS CLIENT_PrintWarning( const char* format, ... )
{
	if ( cl_showwarnings )
	{
		va_list args;
		va_start( args, format );
		VPrintf( PRINT_HIGH, format, args );
		va_end( args );
	}
}

//*****************************************************************************
//	CONSOLE COMMANDS

CCMD( connect )
{
	const char	*pszDemoName;
	UCVarValue	Val;

	// Servers can't connect to other servers!
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	// No IP specified.
	if ( argv.argc( ) <= 1 )
	{
		Printf( "Usage: connect <server IP>\n" );
		return;
	}

	// Potentially disconnect from the current server.
	// [EP] Makes sense only for clients.
	if ( NETWORK_GetState() == NETSTATE_CLIENT )
		CLIENT_QuitNetworkGame( NULL );

	// Put the game in client mode.
	NETWORK_SetState( NETSTATE_CLIENT );
	
	// [AK] Reset the map rotation before we connect to the server.
	MAPROTATION_Construct( );

	// Make sure cheats are off.
	Val.Bool = false;
	sv_cheats.ForceSet( Val, CVAR_Bool );
	am_cheat = 0;

	// Make sure our visibility is normal.
	R_SetVisibility( 8.0f );

	// Create a server IP from the given string.
	g_AddressServer.LoadFromString( argv[1] );

	// If the user didn't specify a port, use the default port.
	if ( g_AddressServer.usPort == 0 )
		g_AddressServer.SetPort( DEFAULT_SERVER_PORT );

	g_AddressLastConnected = g_AddressServer;

	// Put the game in the full console.
	gameaction = ga_fullconsole;

	// Send out a connection signal.
	CLIENT_AttemptConnection( );

	// Update the connection state.
	CLIENT_SetConnectionState( CTS_ATTEMPTINGCONNECTION );

	// If we've elected to record a demo, begin that process now.
	pszDemoName = Args->CheckValue( "-record" );
	if (( gamestate == GS_STARTUP ) && ( pszDemoName ))
		CLIENTDEMO_BeginRecording( pszDemoName );
}

//*****************************************************************************
//
CCMD( disconnect )
{
	// Nothing to do if we're not in client mode!
	if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
		return;

	CLIENT_QuitNetworkGame ( NULL );
}

//*****************************************************************************
//
#ifdef	_DEBUG
CCMD( timeout )
{
	// Nothing to do if we're not in client mode!
	if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
		return;

	// Clear out the existing players.
	CLIENT_ClearAllPlayers();
/*
	// If we're connected in any way, send a disconnect signal.
	if ( g_ConnectionState != CTS_DISCONNECTED )
	{
		NETWORK_WriteByte( &g_LocalBuffer, CONNECT_QUIT );
		g_lBytesSent += g_LocalBuffer.cursize;
		if ( g_lBytesSent > g_lMaxBytesSent )
			g_lMaxBytesSent = g_lBytesSent;
		NETWORK_LaunchPacket( g_LocalBuffer, g_AddressServer );
		g_LocalBuffer.Clear();
	}
*/
	// Clear out our copy of the server address.
	memset( &g_AddressServer, 0, sizeof( g_AddressServer ));
	CLIENT_SetConnectionState( CTS_DISCONNECTED );

	// Go back to the full console.
	gameaction = ga_fullconsole;
}
#endif
//*****************************************************************************
//
CCMD( reconnect )
{
	UCVarValue	Val;

	// If we're in the middle of a game, we first need to disconnect from the server.
	if ( g_ConnectionState != CTS_DISCONNECTED )
		CLIENT_QuitNetworkGame( NULL );
	
	// Store the address of the server we were on.
	if ( g_AddressLastConnected.abIP[0] == 0 )
	{
		Printf( "Unknown IP for last server. Use \"connect <server ip>\".\n" );
		return;
	}

	// Put the game in client mode.
	NETWORK_SetState( NETSTATE_CLIENT );

	// Make sure cheats are off.
	Val.Bool = false;
	sv_cheats.ForceSet( Val, CVAR_Bool );
	am_cheat = 0;

	// Make sure our visibility is normal.
	R_SetVisibility( 8.0f );

	// Set the address of the server we're trying to connect to to the previously connected to server.
	g_AddressServer = g_AddressLastConnected;

	// Put the game in the full console.
	gameaction = ga_fullconsole;

	// Send out a connection signal.
	CLIENT_AttemptConnection( );

	// Update the connection state.
	CLIENT_SetConnectionState( CTS_ATTEMPTINGCONNECTION );
}

//*****************************************************************************
//
CCMD( rcon )
{
	char		szString[1024];
	char		szAppend[256];

	if ( g_ConnectionState != CTS_ACTIVE )
		return;

	if ( argv.argc( ) > 1 )
	{
		LONG	lLast;
		LONG	lIdx;
		ULONG	ulIdx2;
		bool	bHasSpace;

		memset( szString, 0, 1024 );
		memset( szAppend, 0, 256 );
		
		lLast = argv.argc( );

		// Since we don't want "rcon" to be part of our string, start at 1.
		for ( lIdx = 1; lIdx < lLast; lIdx++ )
		{
			memset( szAppend, 0, 256 );

			bHasSpace = false;
			for ( ulIdx2 = 0; ulIdx2 < strlen( argv[lIdx] ); ulIdx2++ )
			{
				if ( argv[lIdx][ulIdx2] == ' ' )
				{
					bHasSpace = true;
					break;
				}
			}

			if ( bHasSpace )
				strcat( szAppend, "\"" );
			strcat( szAppend, argv[lIdx] );
			strcat( szString, szAppend );
			if ( bHasSpace )
				strcat( szString, "\"" );
			if (( lIdx + 1 ) < lLast )
				strcat( szString, " " );
		}
	
		// Alright, enviorment is correct, the string has been built.
		// SEND IT!
		CLIENTCOMMANDS_RCONCommand( szString );
	}
	else
		Printf( "Usage: rcon <command>\n" );
}

//*****************************************************************************
//
CCMD( send_password )
{
	if ( argv.argc( ) <= 1 )
	{
		Printf( "Usage: send_password <password>\n" );
		return;
	}

	if ( g_ConnectionState == CTS_ACTIVE )
		CLIENTCOMMANDS_RequestRCON( argv[1] );
}

//*****************************************************************************
// [Dusk] Redisplay the MOTD
CCMD( motd ) {CLIENT_DisplayMOTD();}

//*****************************************************************************
//	CONSOLE VARIABLES

//CVAR( Int, cl_maxmonstercorpses, 0, CVAR_ARCHIVE )
CVAR( Float, cl_motdtime, 5.0, CVAR_ARCHIVE )
CVAR( Bool, cl_taunts, true, CVAR_ARCHIVE )
CVAR( Int, cl_showcommands, 0, CVAR_ARCHIVE|CVAR_DEBUGONLY )
CVAR( Int, cl_showspawnnames, 0, CVAR_ARCHIVE )
CVAR( Int, cl_connect_flags, CCF_STARTASSPECTATOR, CVAR_ARCHIVE );
CVAR( Flag, cl_startasspectator, cl_connect_flags, CCF_STARTASSPECTATOR );
CVAR( Flag, cl_dontrestorefrags, cl_connect_flags, CCF_DONTRESTOREFRAGS )
CVAR( Flag, cl_hidecountry, cl_connect_flags, CCF_HIDECOUNTRY )
// [BB] Don't archive the passwords! Otherwise Skulltag would always send
// the last used passwords to all servers it connects to.
CVAR( String, cl_password, "password", 0 )
CVAR( String, cl_joinpassword, "password", 0 )
CVAR( Bool, cl_hitscandecalhack, true, CVAR_ARCHIVE )
CUSTOM_CVAR( Int, fov, 90, CVAR_ARCHIVE )
{
	if (self < 5)
		self = 5;
	else if (self > 179)
		self = 179;

	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i])
			{
				players[i].DesiredFOV = float(self);
			}
		}
	}
	else if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		if (gametic - players[consoleplayer].lastFOVTic >= fov_change_cooldown_tics)
		{
			players[consoleplayer].DesiredFOV = float(self);
			players[consoleplayer].lastFOVTic = gametic;
		}
		else
		{
			int tics = fov_change_cooldown_tics;
			Printf( "You can only use \"fov\" once in %d tics! \n", tics );
		}
	}
	else
	{
		players[consoleplayer].DesiredFOV = float(self);
	}
}

//*****************************************************************************
//	STATISTICS
/*
// [BC] TEMPORARY
ADD_STAT( momentum )
{
	FString	Out;

	Out.Format( "X: %3d     Y: %3d", players[consoleplayer].velx, players[consoleplayer].vely );

	return ( Out );
}
*/
