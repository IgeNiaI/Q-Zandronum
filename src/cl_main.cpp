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

//*****************************************************************************
//	MISC CRAP THAT SHOULDN'T BE HERE BUT HAS TO BE BECAUSE OF SLOPPY CODING

//void	ChangeSpy (bool forward);
int		D_PlayerClassToInt (const char *classname);
bool	P_OldAdjustFloorCeil (AActor *thing);
void	ClientObituary (AActor *self, AActor *inflictor, AActor *attacker, int dmgflags, FName MeansOfDeath);
void	P_CrouchMove(player_t * player, int direction);
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
EXTERN_CVAR( Bool, cl_hideaccount )
EXTERN_CVAR( String, name )

//*****************************************************************************
//	CONSOLE COMMANDS/VARIABLES

#ifdef	_DEBUG
CVAR( Bool, cl_emulatepacketloss, false, 0 )
#endif
// [BB]
CVAR( Bool, cl_connectsound, true, CVAR_ARCHIVE )
CVAR( Bool, cl_showwarnings, false, CVAR_ARCHIVE )

//*****************************************************************************
//	PROTOTYPES

// Player functions.
// [BB] Does not work with the latest ZDoom changes. Check if it's still necessary.
//static	void	client_SetPlayerPieces( BYTESTREAM_s *pByteStream );
static	void	client_IgnorePlayer( BYTESTREAM_s *pByteStream );

// Game commands.
static	void	client_SetGameMode( BYTESTREAM_s *pByteStream );
static	void	client_SetGameSkill( BYTESTREAM_s *pByteStream );
static	void	client_SetGameDMFlags( BYTESTREAM_s *pByteStream );
static	void	client_SetGameModeLimits( BYTESTREAM_s *pByteStream );
static	void	client_SetGameEndLevelDelay( BYTESTREAM_s *pByteStream );
static	void	client_SetGameModeState( BYTESTREAM_s *pByteStream );
static	void	client_SetDuelNumDuels( BYTESTREAM_s *pByteStream );
static	void	client_SetLMSSpectatorSettings( BYTESTREAM_s *pByteStream );
static	void	client_SetLMSAllowedWeapons( BYTESTREAM_s *pByteStream );
static	void	client_SetInvasionNumMonstersLeft( BYTESTREAM_s *pByteStream );
static	void	client_SetInvasionWave( BYTESTREAM_s *pByteStream );
static	void	client_SetSimpleCTFSTMode( BYTESTREAM_s *pByteStream );
static	void	client_DoPossessionArtifactPickedUp( BYTESTREAM_s *pByteStream );
static	void	client_DoPossessionArtifactDropped( BYTESTREAM_s *pByteStream );
static	void	client_DoGameModeFight( BYTESTREAM_s *pByteStream );
static	void	client_DoGameModeCountdown( BYTESTREAM_s *pByteStream );
static	void	client_DoGameModeWinSequence( BYTESTREAM_s *pByteStream );
static	void	client_SetDominationState( BYTESTREAM_s *pByteStream );
static	void	client_SetDominationPointOwnership( BYTESTREAM_s *pByteStream );

// Team commands.
static	void	client_SetTeamFrags( BYTESTREAM_s *pByteStream );
static	void	client_SetTeamScore( BYTESTREAM_s *pByteStream );
static	void	client_SetTeamWins( BYTESTREAM_s *pByteStream );
static	void	client_SetTeamReturnTicks( BYTESTREAM_s *pByteStream );
static	void	client_TeamFlagReturned( BYTESTREAM_s *pByteStream );
static	void	client_TeamFlagDropped( BYTESTREAM_s *pByteStream );

// Vote commands.
static	void	client_CallVote( BYTESTREAM_s *pByteStream );
static	void	client_PlayerVote( BYTESTREAM_s *pByteStream );
static	void	client_VoteEnded( BYTESTREAM_s *pByteStream );

// Inventory commands.
static	void	client_GiveInventory( BYTESTREAM_s *pByteStream );
static	void	client_TakeInventory( BYTESTREAM_s *pByteStream );
static	void	client_GivePowerup( BYTESTREAM_s *pByteStream );
static	void	client_DoInventoryPickup( BYTESTREAM_s *pByteStream );
static	void	client_DestroyAllInventory( BYTESTREAM_s *pByteStream );
static	void	client_SetInventoryIcon( BYTESTREAM_s *pByteStream );

// Door commands.
static	void	client_DoDoor( BYTESTREAM_s *pByteStream );
static	void	client_DestroyDoor( BYTESTREAM_s *pByteStream );
static	void	client_ChangeDoorDirection( BYTESTREAM_s *pByteStream );

// Floor commands.
static	void	client_DoFloor( BYTESTREAM_s *pByteStream );
static	void	client_DestroyFloor( BYTESTREAM_s *pByteStream );
static	void	client_ChangeFloorDirection( BYTESTREAM_s *pByteStream );
static	void	client_ChangeFloorType( BYTESTREAM_s *pByteStream );
static	void	client_ChangeFloorDestDist( BYTESTREAM_s *pByteStream );
static	void	client_StartFloorSound( BYTESTREAM_s *pByteStream );
static	void	client_BuildStair( BYTESTREAM_s *pByteStream );

// Ceiling commands.
static	void	client_DoCeiling( BYTESTREAM_s *pByteStream );
static	void	client_DestroyCeiling( BYTESTREAM_s *pByteStream );
static	void	client_ChangeCeilingDirection( BYTESTREAM_s *pByteStream );
static	void	client_ChangeCeilingSpeed( BYTESTREAM_s *pByteStream );
static	void	client_PlayCeilingSound( BYTESTREAM_s *pByteStream );

// Plat commands.
static	void	client_DoPlat( BYTESTREAM_s *pByteStream );
static	void	client_DestroyPlat( BYTESTREAM_s *pByteStream );
static	void	client_ChangePlatStatus( BYTESTREAM_s *pByteStream );
static	void	client_PlayPlatSound( BYTESTREAM_s *pByteStream );

// Elevator commands.
static	void	client_DoElevator( BYTESTREAM_s *pByteStream );
static	void	client_DestroyElevator( BYTESTREAM_s *pByteStream );
static	void	client_StartElevatorSound( BYTESTREAM_s *pByteStream );

// Pillar commands.
static	void	client_DoPillar( BYTESTREAM_s *pByteStream );
static	void	client_DestroyPillar( BYTESTREAM_s *pByteStream );

// Waggle commands.
static	void	client_DoWaggle( BYTESTREAM_s *pByteStream );
static	void	client_DestroyWaggle( BYTESTREAM_s *pByteStream );
static	void	client_UpdateWaggle( BYTESTREAM_s *pByteStream );

// Rotate poly commands.
static	void	client_DoRotatePoly( BYTESTREAM_s *pByteStream );
static	void	client_DestroyRotatePoly( BYTESTREAM_s *pByteStream );

// Move poly commands.
static	void	client_DoMovePoly( BYTESTREAM_s *pByteStream );
static	void	client_DestroyMovePoly( BYTESTREAM_s *pByteStream );

// Poly door commands.
static	void	client_DoPolyDoor( BYTESTREAM_s *pByteStream );
static	void	client_DestroyPolyDoor( BYTESTREAM_s *pByteStream );
static	void	client_SetPolyDoorSpeedPosition( BYTESTREAM_s *pByteStream );
static  void	client_SetPolyDoorSpeedRotation( BYTESTREAM_s *pByteStream );

// Generic polyobject commands.
static	void	client_PlayPolyobjSound( BYTESTREAM_s *pByteStream );
static	void	client_StopPolyobjSound( BYTESTREAM_s *pByteStream );
static	void	client_SetPolyobjPosition( BYTESTREAM_s *pByteStream );
static	void	client_SetPolyobjRotation( BYTESTREAM_s *pByteStream );

// Miscellaneous commands.
static	void	client_EarthQuake( BYTESTREAM_s *pByteStream );
static	void	client_DoScroller( BYTESTREAM_s *pByteStream );
static	void	client_SetScroller( BYTESTREAM_s *pByteStream );
static	void	client_SetWallScroller( BYTESTREAM_s *pByteStream );
static	void	client_DoFlashFader( BYTESTREAM_s *pByteStream );
static	void	client_GenericCheat( BYTESTREAM_s *pByteStream );
static	void	client_SetCameraToTexture( BYTESTREAM_s *pByteStream );
static	void	client_CreateTranslation( BYTESTREAM_s *pByteStream, bool bIsTypeTwo );
static	void	client_DoPusher( BYTESTREAM_s *pByteStream );
static	void	client_AdjustPusher( BYTESTREAM_s *pByteStream );

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
			CLIENTCOMMANDS_RequestInventoryUseAll();
		else if ( SendItemUse->IsKindOf( RUNTIME_CLASS( AWeapon ) ) )
			players[consoleplayer].mo->UseInventory( item );
		else
			CLIENTCOMMANDS_RequestInventoryUse( item );

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
	// [BB] Stay network compatible with 3.0.
	NETWORK_WriteString( &g_LocalBuffer.ByteStream, "3.0" );
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

				szErrorString.Format( "Failed connect. Your version is different.\nThis server is using version: %s\nPlease check http://www." DOMAIN_NAME "/ for updates.", NETWORK_ReadString( pByteStream ) );
				break;
			case NETWORK_ERRORCODE_WRONGPROTOCOLVERSION:

				szErrorString.Format( "Failed connect. Your protocol version is different.\nServer uses: %s\nYou use:     %s\nPlease check http://www." DOMAIN_NAME "/ for a matching version.", NETWORK_ReadString( pByteStream ), GetVersionStringRev() );
				break;
			case NETWORK_ERRORCODE_BANNED:

				// [TP] Is this a master ban?
				if ( !!NETWORK_ReadByte( pByteStream ))
				{
					szErrorString = "Couldn't connect. \\cgYou have been banned from " GAMENAME "'s master server!\\c-\n"
						"If you feel this is in error, you may contact the staff at " FORUM_URL;
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

					Printf ( "The server reports %d pwad(s):\n", numServerPWADs );
					for( std::list<std::pair<FString, FString> >::iterator i = serverPWADs.begin( ); i != serverPWADs.end( ); ++i )
						Printf( "PWAD: %s - %s\n", i->first.GetChars(), i->second.GetChars() );
					Printf ( "You have loaded %d pwad(s):\n", NETWORK_GetPWADList().Size() );
					for ( unsigned int i = 0; i < NETWORK_GetPWADList().Size(); ++i )
					{
						const NetworkPWAD& pwad = NETWORK_GetPWADList()[i];
						Printf( "PWAD: %s - %s\n", pwad.name.GetChars(), pwad.checksum.GetChars() );
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
	case SVC_NOTHING:

		break;
	/* [BB] Does not work with the latest ZDoom changes. Check if it's still necessary.
	case SVC_SETPLAYERPIECES:

		client_SetPlayerPieces( pByteStream );
		break;
	*/
	case SVC_SETGAMEMODE:

		client_SetGameMode( pByteStream );
		break;
	case SVC_SETGAMESKILL:

		client_SetGameSkill( pByteStream );
		break;
	case SVC_SETGAMEDMFLAGS:

		client_SetGameDMFlags( pByteStream );
		break;
	case SVC_SETGAMEMODELIMITS:

		client_SetGameModeLimits( pByteStream );
		break;
	case SVC_SETGAMEENDLEVELDELAY:
		
		client_SetGameEndLevelDelay( pByteStream );
		break;
	case SVC_SETGAMEMODESTATE:

		client_SetGameModeState( pByteStream );
		break;
	case SVC_SETDUELNUMDUELS:

		client_SetDuelNumDuels( pByteStream );
		break;
	case SVC_SETLMSSPECTATORSETTINGS:

		client_SetLMSSpectatorSettings( pByteStream );
		break;
	case SVC_SETLMSALLOWEDWEAPONS:

		client_SetLMSAllowedWeapons( pByteStream );
		break;
	case SVC_SETINVASIONNUMMONSTERSLEFT:

		client_SetInvasionNumMonstersLeft( pByteStream );
		break;
	case SVC_SETINVASIONWAVE:

		client_SetInvasionWave( pByteStream );
		break;
	case SVC_SETSIMPLECTFSTMODE:

		client_SetSimpleCTFSTMode( pByteStream );
		break;
	case SVC_DOPOSSESSIONARTIFACTPICKEDUP:

		client_DoPossessionArtifactPickedUp( pByteStream );
		break;
	case SVC_DOPOSSESSIONARTIFACTDROPPED:

		client_DoPossessionArtifactDropped( pByteStream );
		break;
	case SVC_DOGAMEMODEFIGHT:

		client_DoGameModeFight( pByteStream );
		break;
	case SVC_DOGAMEMODECOUNTDOWN:

		client_DoGameModeCountdown( pByteStream );
		break;
	case SVC_DOGAMEMODEWINSEQUENCE:

		client_DoGameModeWinSequence( pByteStream );
		break;
	case SVC_SETDOMINATIONSTATE:

		client_SetDominationState( pByteStream );
		break;
	case SVC_SETDOMINATIONPOINTOWNER:

		client_SetDominationPointOwnership( pByteStream );
		break;
	case SVC_SETTEAMFRAGS:

		client_SetTeamFrags( pByteStream );
		break;
	case SVC_SETTEAMSCORE:

		client_SetTeamScore( pByteStream );
		break;
	case SVC_SETTEAMWINS:

		client_SetTeamWins( pByteStream );
		break;
	case SVC_SETTEAMRETURNTICKS:

		client_SetTeamReturnTicks( pByteStream );
		break;
	case SVC_TEAMFLAGRETURNED:

		client_TeamFlagReturned( pByteStream );
		break;
	case SVC_TEAMFLAGDROPPED:

		client_TeamFlagDropped( pByteStream );
		break;
	case SVC_CALLVOTE:

		client_CallVote( pByteStream );
		break;
	case SVC_PLAYERVOTE:

		client_PlayerVote( pByteStream );
		break;
	case SVC_VOTEENDED:

		client_VoteEnded( pByteStream );
		break;
	case SVC_GIVEINVENTORY:

		client_GiveInventory( pByteStream );
		break;
	case SVC_TAKEINVENTORY:

		client_TakeInventory( pByteStream );
		break;
	case SVC_GIVEPOWERUP:

		client_GivePowerup( pByteStream );
		break;
	case SVC_DOINVENTORYPICKUP:

		client_DoInventoryPickup( pByteStream );
		break;
	case SVC_DESTROYALLINVENTORY:

		client_DestroyAllInventory( pByteStream );
		break;
	case SVC_DODOOR:

		client_DoDoor( pByteStream );
		break;
	case SVC_DESTROYDOOR:

		client_DestroyDoor( pByteStream );
		break;
	case SVC_CHANGEDOORDIRECTION:

		client_ChangeDoorDirection( pByteStream );
		break;
	case SVC_DOFLOOR:

		client_DoFloor( pByteStream );
		break;
	case SVC_DESTROYFLOOR:

		client_DestroyFloor( pByteStream );
		break;
	case SVC_CHANGEFLOORDIRECTION:

		client_ChangeFloorDirection( pByteStream );
		break;
	case SVC_CHANGEFLOORTYPE:

		client_ChangeFloorType( pByteStream );
		break;
	case SVC_CHANGEFLOORDESTDIST:

		client_ChangeFloorDestDist( pByteStream );
		break;
	case SVC_STARTFLOORSOUND:

		client_StartFloorSound( pByteStream );
		break;
	case SVC_DOCEILING:

		client_DoCeiling( pByteStream );
		break;
	case SVC_DESTROYCEILING:

		client_DestroyCeiling( pByteStream );
		break;
	case SVC_CHANGECEILINGDIRECTION:

		client_ChangeCeilingDirection( pByteStream );
		break;
	case SVC_CHANGECEILINGSPEED:

		client_ChangeCeilingSpeed( pByteStream );
		break;
	case SVC_PLAYCEILINGSOUND:

		client_PlayCeilingSound( pByteStream );
		break;
	case SVC_DOPLAT:

		client_DoPlat( pByteStream );
		break;
	case SVC_DESTROYPLAT:

		client_DestroyPlat( pByteStream );
		break;
	case SVC_CHANGEPLATSTATUS:

		client_ChangePlatStatus( pByteStream );
		break;
	case SVC_PLAYPLATSOUND:

		client_PlayPlatSound( pByteStream );
		break;
	case SVC_DOELEVATOR:

		client_DoElevator( pByteStream );
		break;
	case SVC_DESTROYELEVATOR:

		client_DestroyElevator( pByteStream );
		break;
	case SVC_STARTELEVATORSOUND:

		client_StartElevatorSound( pByteStream );
		break;
	case SVC_DOPILLAR:

		client_DoPillar( pByteStream );
		break;
	case SVC_DESTROYPILLAR:

		client_DestroyPillar( pByteStream );
		break;
	case SVC_DOWAGGLE:

		client_DoWaggle( pByteStream );
		break;
	case SVC_DESTROYWAGGLE:

		client_DestroyWaggle( pByteStream );
		break;
	case SVC_UPDATEWAGGLE:

		client_UpdateWaggle( pByteStream );
		break;
	case SVC_DOROTATEPOLY:

		client_DoRotatePoly( pByteStream );
		break;
	case SVC_DESTROYROTATEPOLY:

		client_DestroyRotatePoly( pByteStream );
		break;
	case SVC_DOMOVEPOLY:

		client_DoMovePoly( pByteStream );
		break;
	case SVC_DESTROYMOVEPOLY:

		client_DestroyMovePoly( pByteStream );
		break;
	case SVC_DOPOLYDOOR:

		client_DoPolyDoor( pByteStream );
		break;
	case SVC_DESTROYPOLYDOOR:

		client_DestroyPolyDoor( pByteStream );
		break;
	case SVC_SETPOLYDOORSPEEDPOSITION:

		client_SetPolyDoorSpeedPosition( pByteStream );
		break;
	case SVC_SETPOLYDOORSPEEDROTATION:

		client_SetPolyDoorSpeedRotation( pByteStream );
		break;
	case SVC_PLAYPOLYOBJSOUND:

		client_PlayPolyobjSound( pByteStream );
		break;
	case SVC_SETPOLYOBJPOSITION:

		client_SetPolyobjPosition( pByteStream );
		break;
	case SVC_SETPOLYOBJROTATION:

		client_SetPolyobjRotation( pByteStream );
		break;
	case SVC_EARTHQUAKE:

		client_EarthQuake( pByteStream );
		break;
	case SVC_DOSCROLLER:

		client_DoScroller( pByteStream );
		break;
	case SVC_SETSCROLLER:

		client_SetScroller( pByteStream );
		break;
	case SVC_SETWALLSCROLLER:

		client_SetWallScroller( pByteStream );
		break;
	case SVC_DOFLASHFADER:

		client_DoFlashFader( pByteStream );
		break;
	case SVC_GENERICCHEAT:

		client_GenericCheat( pByteStream );
		break;
	case SVC_SETCAMERATOTEXTURE:

		client_SetCameraToTexture( pByteStream );
		break;
	case SVC_CREATETRANSLATION:

		client_CreateTranslation( pByteStream, false );
		break;
	case SVC_CREATETRANSLATION2:

		client_CreateTranslation( pByteStream, true );
		break;

	case SVC_DOPUSHER:

		client_DoPusher( pByteStream );
		break;

	case SVC_ADJUSTPUSHER:

		client_AdjustPusher( pByteStream );
		break;

	case SVC_IGNOREPLAYER:

		client_IgnorePlayer( pByteStream );
		break;

	case SVC_EXTENDEDCOMMAND:
		{
			const LONG lExtCommand = NETWORK_ReadByte( pByteStream );

			if ( cl_showcommands )
				Printf( "%s\n", GetStringSVC2 ( static_cast<SVC2> ( lExtCommand ) ) );

			if ( CLIENT_ParseExtendedServerCommand( static_cast<SVC2>( lExtCommand ), pByteStream ))
				break;

			switch ( lExtCommand )
			{
			case SVC2_SETINVENTORYICON:

				client_SetInventoryIcon( pByteStream );
				break;

			case SVC2_SETIGNOREWEAPONSELECT:
				{
					const bool bIgnoreWeaponSelect = !!NETWORK_ReadByte( pByteStream );
					CLIENT_IgnoreWeaponSelect ( bIgnoreWeaponSelect );
				}

				break;

			case SVC2_CLEARCONSOLEPLAYERWEAPON:
				{
					PLAYER_ClearWeapon ( &players[consoleplayer] );
				}

				break;

			case SVC2_LIGHTNING:
				{
					// [Dusk] The client doesn't care about the mode given to P_ForceLightning since
					// it doesn't do the next lightning calculations anyway.
					P_ForceLightning( 0 );
				}

				break;

			case SVC2_CANCELFADE:
				{
					const ULONG ulPlayer = NETWORK_ReadByte( pByteStream );
					AActor *activator = NULL;
					// [BB] ( ulPlayer == MAXPLAYERS ) means that CancelFade was called with NULL as activator.
					if ( PLAYER_IsValidPlayer ( ulPlayer ) )
						activator = players[ulPlayer].mo;

					// [BB] Needed to copy the code below from the implementation of PCD_CANCELFADE.
					TThinkerIterator<DFlashFader> iterator;
					DFlashFader *fader;

					while ( (fader = iterator.Next()) )
					{
						if (activator == NULL || fader->WhoFor() == activator)
						{
							fader->Cancel ();
						}
					}
				}

				break;

			case SVC2_PLAYBOUNCESOUND:
				{
					AActor *pActor = CLIENT_FindThingByNetID( NETWORK_ReadShort( pByteStream ) );
					const bool bOnfloor = !!NETWORK_ReadByte( pByteStream );
					if ( pActor )
						pActor->PlayBounceSound ( bOnfloor );
				}
				break;

			case SVC2_SETTHINGREACTIONTIME:
				{
					const LONG lID = NETWORK_ReadShort( pByteStream ); 
					const LONG lReactionTime = NETWORK_ReadShort( pByteStream );
					AActor *pActor = CLIENT_FindThingByNetID( lID );

					if ( pActor == NULL )
					{
						CLIENT_PrintWarning( "SETTHINGREACTIONTIME: Couldn't find thing: %ld\n", lID );
						break;
					}
					pActor->reactiontime = lReactionTime;
				}
				break;

			// [Dusk]
			case SVC2_SETFASTCHASESTRAFECOUNT:
				{
					const LONG lID = NETWORK_ReadShort( pByteStream );
					const LONG lStrafeCount = NETWORK_ReadByte( pByteStream ); 
					AActor *pActor = CLIENT_FindThingByNetID( lID );

					if ( pActor == NULL )
					{
						CLIENT_PrintWarning( "SETFASTCHASESTRAFECOUNT: Couldn't find thing: %ld\n", lID );
						break;
					}
					pActor->FastChaseStrafeCount = lStrafeCount;
				}
				break;

			case SVC2_RESETMAP:
				{
					// [EP] Clear all the HUD messages.
					if ( StatusBar )
						StatusBar->DetachAllMessages();

					GAME_ResetMap();
				}
				break;

			case SVC2_SETPOWERUPBLENDCOLOR:
				{
					// Read in the player ID.
					const ULONG ulPlayer = NETWORK_ReadByte( pByteStream );

					// Read in the identification of the type of item to give.
					const USHORT usActorNetworkIndex = NETWORK_ReadShort( pByteStream );

					// Read in the blend color of the powerup.
					const ULONG ulBlendColor = NETWORK_ReadLong( pByteStream );

					// Check to make sure everything is valid. If not, break out.
					if ( PLAYER_IsValidPlayerWithMo( ulPlayer ) == false ) 
						break;

					const PClass *pType = NETWORK_GetClassFromIdentification( usActorNetworkIndex );
					if ( pType == NULL )
						break;

					// If this isn't a powerup, just quit.
					if ( pType->IsDescendantOf( RUNTIME_CLASS( APowerup )) == false )
						break;

					// Try to find this object within the player's personal inventory.
					AInventory *pInventory = players[ulPlayer].mo->FindInventory( pType );

					// [WS] If the player has this powerup, set its blendcolor appropriately.
					if ( pInventory )
						static_cast<APowerup*>(pInventory)->BlendColor = ulBlendColor;
					break;
				}

			// [Dusk]
			case SVC2_SETPLAYERHAZARDCOUNT:
				{
					const ULONG ulPlayer = NETWORK_ReadByte( pByteStream );
					const int hz = NETWORK_ReadShort( pByteStream );

					if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
						break;

					players[ulPlayer].hazardcount = hz;
				}
				break;

			case SVC2_SCROLL3DMIDTEX:
				{
					const int i = NETWORK_ReadByte ( pByteStream );
					const int move = NETWORK_ReadLong ( pByteStream );
					const bool ceiling = !!NETWORK_ReadByte ( pByteStream );

					if ( i < 0 || i >= numsectors || !move )
						break;

					P_Scroll3dMidtex( &sectors[i], 0, move, ceiling );
					break;
				}
			case SVC2_SETPLAYERLOGNUMBER:
				{
					const ULONG ulPlayer = NETWORK_ReadByte( pByteStream );
					const int arg0 = NETWORK_ReadShort( pByteStream );

					if ( PLAYER_IsValidPlayerWithMo( ulPlayer ) == false ) 
						break;

					if ( players[ulPlayer].mo->FindInventory(NAME_Communicator) )
						players[ulPlayer].SetLogNumber ( arg0 );
				}
				break;

			case SVC2_SETTHINGSPECIAL:
				{
					const LONG lID = NETWORK_ReadShort( pByteStream ); 
					const LONG lSpecial = NETWORK_ReadShort( pByteStream );
					AActor *pActor = CLIENT_FindThingByNetID( lID );

					if ( pActor == NULL )
					{
						CLIENT_PrintWarning( "SVC2_SETTHINGSPECIAL: Couldn't find thing: %ld\n", lID );
						break;
					}
					pActor->special = lSpecial;
				}
				break;

			case SVC2_SYNCPATHFOLLOWER:
				{
					APathFollower::InitFromStream ( pByteStream );
				}
				break;

			case SVC2_SETPLAYERVIEWHEIGHT:
				{
					const ULONG ulPlayer = NETWORK_ReadByte( pByteStream );
					const int viewHeight = NETWORK_ReadLong( pByteStream );

					if ( PLAYER_IsValidPlayerWithMo( ulPlayer ) == false ) 
						break;

					players[ulPlayer].mo->ViewHeight = viewHeight;
					players[ulPlayer].viewheight = viewHeight;
				}
				break;

			case SVC2_SRP_USER_START_AUTHENTICATION:
			case SVC2_SRP_USER_PROCESS_CHALLENGE:
			case SVC2_SRP_USER_VERIFY_SESSION:
				CLIENT_ProcessSRPServerCommand ( lExtCommand, pByteStream );
				break;

			case SVC2_SETTHINGHEALTH:
				{
					const LONG lID = NETWORK_ReadShort( pByteStream );
					const int health = NETWORK_ReadByte( pByteStream );
					AActor* mo = CLIENT_FindThingByNetID( lID );

					if ( mo == NULL )
					{
						CLIENT_PrintWarning( "SVC2_SETTHINGSPECIAL: Couldn't find thing: %ld\n", lID );
						break;
					}

					mo->health = health;
				}
				break;

			case SVC2_SETCVAR:
				{
					const FString cvarName = NETWORK_ReadString( pByteStream );
					const FString cvarValue = NETWORK_ReadString( pByteStream );

					// [TP] Only allow the server to set mod CVARs.
					FBaseCVar* cvar = FindCVar( cvarName, NULL );

					if (( cvar == NULL ) || (( cvar->GetFlags() & CVAR_MOD ) == 0 ))
					{
						CLIENT_PrintWarning( "SVC2_SETCVAR: The server attempted to set the value of "
							"%s to \"%s\"\n", cvarName.GetChars(), cvarValue.GetChars() );
						break;
					}

					UCVarValue vval;
					vval.String = cvarValue;
					cvar->ForceSet( vval, CVAR_String );
				}
				break;

			// [EP]
			case SVC2_STOPPOLYOBJSOUND:
				client_StopPolyobjSound( pByteStream );
				break;

			// [EP]
			case SVC2_BUILDSTAIR:
				client_BuildStair( pByteStream );
				break;

			// [EP]
			case SVC2_SETMUGSHOTSTATE:
				{
					const char *statename = NETWORK_ReadString( pByteStream );
					if ( StatusBar != NULL)
					{
						StatusBar->SetMugShotState( statename );
					}
				}
				break;

			case SVC2_PUSHTOJOINQUEUE:
				{
					int player = NETWORK_ReadByte( pByteStream );
					int team = NETWORK_ReadByte( pByteStream );
					JOINQUEUE_AddPlayer( player, team );
				}
				break;

			case SVC2_REMOVEFROMJOINQUEUE:
				JOINQUEUE_RemovePlayerAtPosition( NETWORK_ReadByte( pByteStream ) );
				break;

			case SVC2_SETDEFAULTSKYBOX:
				{
					int mobjNetID = NETWORK_ReadShort( pByteStream );
					if ( mobjNetID == -1  )
						level.DefaultSkybox = NULL;
					else
					{
						AActor *mo = CLIENT_FindThingByNetID( mobjNetID );
						if ( mo && mo->GetClass()->IsDescendantOf( RUNTIME_CLASS( ASkyViewpoint ) ) )
							level.DefaultSkybox = static_cast<ASkyViewpoint *>( mo );
					}
				}
				break;

			case SVC2_FLASHSTEALTHMONSTER:
				{
					AActor* mobj = CLIENT_FindThingByNetID( NETWORK_ReadShort( pByteStream ));

					if ( mobj && ( mobj->flags & MF_STEALTH ))
					{
						mobj->alpha = OPAQUE;
						mobj->visdir = -1;
					}
				}
				break;

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
		if (( cl_showcommands >= 4 ) && ( lCommand == SVC_UPDATEPLAYEREXTRADATA ))
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

	// Send the move header and the gametic.
	CLIENTCOMMANDS_ClientMove( );
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
AActor *CLIENT_SpawnThing( const PClass *pType, fixed_t X, fixed_t Y, fixed_t Z, LONG lNetID, BYTE spawnFlags )
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
	}
	else
		CLIENT_PrintWarning( "CLIENT_SpawnThing: Failed to spawn actor %s with id %ld\n", pType->TypeName.GetChars( ), lNetID );

	return ( pActor );
}

//*****************************************************************************
//
void CLIENT_SpawnMissile( const PClass *pType, fixed_t X, fixed_t Y, fixed_t Z, fixed_t VelX, fixed_t VelY, fixed_t VelZ, LONG lNetID, LONG lTargetNetID )
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

	// Play the seesound if this missile has one.
	if ( pActor->SeeSound )
		S_Sound( pActor, CHAN_VOICE, pActor->SeeSound, 1, ATTN_NORM );

	pActor->target = CLIENT_FindThingByNetID( lTargetNetID );
}

//*****************************************************************************
//
void CLIENT_MoveThing( AActor *pActor, fixed_t X, fixed_t Y, fixed_t Z )
{
	if (( pActor == NULL ) || ( gamestate != GS_LEVEL ))
		return;

	pActor->SetOrigin( X, Y, Z );

	// [BB] SetOrigin doesn't set the actor's floorz value properly, so we need to correct this.
	if ( ( pActor->flags & MF_NOBLOCKMAP ) == false )
	{
		// [BB] Unfortunately, P_OldAdjustFloorCeil messes up the floorz value under some circumstances.
		// Save the old value, so that we can restore it if necessary.
		// [EP] It seems it messes also with the ceilingz value.
		fixed_t oldfloorz = pActor->floorz;
		fixed_t oldceilingz = pActor->ceilingz;
		P_OldAdjustFloorCeil( pActor );
		// [BB] An actor can't be below its floorz, if the value is correct.
		// In this case, P_OldAdjustFloorCeil apparently didn't work, so revert to the old value.
		// [BB] But don't do this for the console player, it messes up the prediction.
		// [EP] Ditto for ceilingz.
		if ( NETWORK_IsConsolePlayer ( pActor ) == false )
		{
			if ( pActor->floorz > pActor->z )
				pActor->floorz = oldfloorz;
			if ( pActor->ceilingz < pActor->z + pActor->height )
				pActor->ceilingz = oldceilingz;
		}
	}
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
	pPlayer->jumpTics = 0;
	pPlayer->respawn_time = 0;
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
	pPlayer->ulPingAverages = 0;
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

	pPlayer->DesiredFOV = pPlayer->FOV = 90.f;
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
			Spawn( "TeleportFog", pActor->x + 20 * finecosine[an],
				pActor->y + 20 * finesine[an],
				pActor->z + TELEFOGHEIGHT, ALLOW_REPLACE );
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
	if ( IsVisible() == false )
	{
		player->mo->renderflags |= RF_INVISIBLE;

		// Don't move the player since the server didn't send any useful position information.
		return;
	}
	else
		player->mo->renderflags &= ~RF_INVISIBLE;

	// Set the player's XYZ position.
	// [BB] But don't just set the position, but also properly set floorz and ceilingz, etc.
	CLIENT_MoveThing( player->mo, x, y, z );

	// Set the player's angle.
	player->mo->angle = angle;

	// Set the player's XYZ momentum.
	player->mo->velx = velx;
	player->mo->vely = vely;
	player->mo->velz = velz;

	// Is the player crouching?
	player->crouchdir = ( isCrouching ) ? 1 : -1;

	if (( player->crouchdir == 1 ) &&
		( player->crouchfactor < FRACUNIT ) &&
		(( player->mo->z + player->mo->height ) < player->mo->ceilingz ))
	{
		P_CrouchMove( player, 1 );
	}
	else if (( player->crouchdir == -1 ) &&
		( player->crouchfactor > FRACUNIT/2 ))
	{
		P_CrouchMove( player, -1 );
	}

	// [BB] Set whether the player is attacking or not.
	// Check: Is it a good idea to only do this, when the player is visible?
	if ( flags & PLAYER_ATTACK )
		player->cmd.ucmd.buttons |= BT_ATTACK;
	else
		player->cmd.ucmd.buttons &= ~BT_ATTACK;

	if ( flags & PLAYER_ALTATTACK )
		player->cmd.ucmd.buttons |= BT_ALTATTACK;
	else
		player->cmd.ucmd.buttons &= ~BT_ALTATTACK;
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
			// Display a large "You were fragged by <name>." message in the middle of the screen.
			if ( player == &players[consoleplayer] )
				SCOREBOARD_DisplayFraggedMessage( &players[ulSourcePlayer] );
			// Display a large "You fragged <name>!" message in the middle of the screen.
			else if ( ulSourcePlayer == static_cast<ULONG>(consoleplayer) )
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
	ClientObituary( player->mo, inflictor, source, ( ulSourcePlayer < MAXPLAYERS ) ? DMG_PLAYERATTACK : 0, MOD );

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
		else if ( name == NAME_CL_TicsPerUpdate )
			player->userinfo.TicsPerUpdateChanged ( value.ToLong() );
		else if ( name == NAME_CL_ConnectionType )
			player->userinfo.ConnectionTypeChanged ( value.ToLong() );
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
	AActor *oldcamera = players[consoleplayer].camera;

	if ( camera == NULL )
	{
		players[consoleplayer].camera = players[consoleplayer].mo;
		players[consoleplayer].cheats &= ~CF_REVERTPLEASE;
		if (oldcamera != players[consoleplayer].camera)
			R_ClearPastViewer (players[consoleplayer].camera);
		return;
	}

	// Finally, change the player's camera.
	players[consoleplayer].camera = camera;
	if ( revertPlease == false )
		players[consoleplayer].cheats &= ~CF_REVERTPLEASE;
	else
		players[consoleplayer].cheats |= CF_REVERTPLEASE;

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
void ServerCommands::UpdatePlayerPing::Execute()
{
	player->ulPing = ping;
}

//*****************************************************************************
//
void ServerCommands::UpdatePlayerExtraData::Execute()
{
	// [BB] If the spectated player uses the GL renderer and we are using software,
	// the viewangle has to be limited.	We don't care about cl_disallowfullpitch here.
	if ( !currentrenderer )
	{
		// [BB] The user can restore ZDoom's freelook limit.
		const fixed_t pitchLimit = -ANGLE_1*( cl_oldfreelooklimit ? 32 : 56 );
		if (pitch < pitchLimit)
			pitch = pitchLimit;
		if (pitch > ANGLE_1*56)
			pitch = ANGLE_1*56;
	}
	player->mo->pitch = pitch;
	player->mo->waterlevel = waterLevel;
	// [BB] The attack buttons are now already set in *_MovePlayer, so additionally setting
	// them here is obsolete. I don't want to change this before 97D2 final though.
	player->cmd.ucmd.buttons = buttons;
	player->viewz = viewZ;
	player->bob = bob;
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
		// [BB] Calling CLIENT_MoveThing instead of setting the x,y,z position directly should make
		// sure that the spectator body doesn't get stuck.
		CLIENT_MoveThing ( pPlayer->mo, x, y, z );

		pPlayer->mo->velx = velx;
		pPlayer->mo->vely = vely;
		pPlayer->mo->velz = velz;
	}
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
void ServerCommands::SpawnThing::Execute()
{
	CLIENT_SpawnThing( type, x, y, z, id, 0 );
}

//*****************************************************************************
//
void ServerCommands::SpawnThingNoNetID::Execute()
{
	CLIENT_SpawnThing( type, x, y, z, -1 );
}

//*****************************************************************************
//
void ServerCommands::SpawnThingExact::Execute()
{
	CLIENT_SpawnThing( type, x, y, z, id );
}

//*****************************************************************************
//
void ServerCommands::SpawnThingExactNoNetID::Execute()
{
	CLIENT_SpawnThing( type, x, y, z, -1 );
}

//*****************************************************************************
//
void ServerCommands::LevelSpawnThing::Execute()
{
	CLIENT_SpawnThing( type, x, y, z, id, SPAWNFLAG_LEVELTHING );
}

//*****************************************************************************
//
void ServerCommands::LevelSpawnThingNoNetID::Execute()
{
	CLIENT_SpawnThing( type, x, y, z, -1, SPAWNFLAG_LEVELTHING );
}

//*****************************************************************************
//
void ServerCommands::MoveThing::Execute()
{
	if (( actor == NULL ) || gamestate != GS_LEVEL )
		return;

	fixed_t x = ContainsNewX() ? newX : actor->x;
	fixed_t y = ContainsNewY() ? newY : actor->y;
	fixed_t z = ContainsNewZ() ? newZ : actor->z;

	if ( ContainsNewX() && (( bits & CM_NOLAST ) == 0 ))
		actor->lastX = x;
	if ( ContainsNewY() && (( bits & CM_NOLAST ) == 0 ))
		actor->lastY = y;
	if ( ContainsNewZ() && (( bits & CM_NOLAST ) == 0 ))
		actor->lastZ = z;

	// Read in the last position data.
	if ( ContainsLastX() )
		actor->lastX = lastX;
	if ( ContainsLastY() )
		actor->lastY = lastY;
	if ( ContainsLastZ() )
		actor->lastZ = lastZ;

	// [WS] Clients will reuse their last updated position.
	if ( bits & CM_REUSE_X )
		x = actor->lastX;
	if ( bits & CM_REUSE_Y )
		y = actor->lastY;
	if ( bits & CM_REUSE_Z )
		z = actor->lastZ;

	// Update the thing's position.
	if ( bits & ( CM_X|CM_Y|CM_Z|CM_REUSE_X|CM_REUSE_Y|CM_REUSE_Z ))
		CLIENT_MoveThing( actor, x, y, z );

	// Read in the angle data.
	if ( ContainsAngle() )
		actor->angle = angle;

	// Read in the momentum data.
	if ( ContainsVelX() )
		actor->velx = velX;
	if ( ContainsVelY() )
		actor->vely = velY;
	if ( ContainsVelZ() )
		actor->velz = velZ;

	// [TP] If this is a player that's being moved around, and his velocity becomes zero,
	// we need to stop his bobbing as well.
	if ( actor->player && ( actor->velx == 0 ) && ( actor->vely == 0 ))
		actor->player->velx = actor->player->vely = 0;

	// Read in the pitch data.
	if ( ContainsPitch() )
		actor->pitch = pitch;

	// Read in the movedir data.
	if ( ContainsMovedir() )
		actor->movedir = movedir;

	// If the server is moving us, don't let our prediction get messed up.
	if ( actor == players[consoleplayer].mo )
	{
		players[consoleplayer].ServerXYZ[0] = x;
		players[consoleplayer].ServerXYZ[1] = y;
		players[consoleplayer].ServerXYZ[2] = z;
		CLIENT_PREDICT_PlayerTeleported( );
	}
}

//*****************************************************************************
//
void ServerCommands::MoveThingExact::Execute()
{
	if (( actor == NULL ) || gamestate != GS_LEVEL )
		return;

	fixed_t x = ContainsNewX() ? newX : actor->x;
	fixed_t y = ContainsNewY() ? newY : actor->y;
	fixed_t z = ContainsNewZ() ? newZ : actor->z;

	// Read in the position data.
	if ( ContainsNewX() && (( bits & CM_NOLAST ) == 0 ))
		actor->lastX = x;
	if ( ContainsNewY() && (( bits & CM_NOLAST ) == 0 ))
		actor->lastY = y;
	if ( ContainsNewZ() && (( bits & CM_NOLAST ) == 0 ))
		actor->lastZ = z;

	// Read in the last position data.
	if ( ContainsLastX() )
		actor->lastX = lastX;
	if ( ContainsLastY() )
		actor->lastY = lastY;
	if ( ContainsLastZ() )
		actor->lastZ = lastZ;

	// [WS] Clients will reuse their last updated position.
	if ( bits & CM_REUSE_X )
		x = actor->lastX;
	if ( bits & CM_REUSE_Y )
		y = actor->lastY;
	if ( bits & CM_REUSE_Z )
		z = actor->lastZ;

	// Update the thing's position.
	if ( bits & ( CM_X|CM_Y|CM_Z|CM_REUSE_X|CM_REUSE_Y|CM_REUSE_Z ))
		CLIENT_MoveThing( actor, x, y, z );

	// Read in the angle data.
	if ( ContainsAngle() )
		actor->angle = angle;

	// Read in the momentum data.
	if ( ContainsVelX() )
		actor->velx = velX;
	if ( ContainsVelY() )
		actor->vely = velY;
	if ( ContainsVelZ() )
		actor->velz = velZ;

	// [TP] If this is a player that's being moved around, and his velocity becomes zero,
	// we need to stop his bobbing as well.
	if ( actor->player && ( actor->velx == 0 ) && ( actor->vely == 0 ))
		actor->player->velx = actor->player->vely = 0;

	// Read in the pitch data.
	if ( ContainsPitch() )
		actor->pitch = pitch;

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
	actor->angle = angle;
}

//*****************************************************************************
//
void ServerCommands::SetThingAngleExact::Execute()
{
	actor->angle = angle;
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
	case FLAGSET_FLAGSST:

		actor->ulSTFlags = flags;
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
void ServerCommands::SetThingProperty::Execute()
{
	// Set one of the actor's properties, depending on what was read in.
	switch ( property )
	{
	case APROP_Speed:
		actor->Speed = value;
		break;

	case APROP_Alpha:
		actor->alpha = value;
		break;

	case APROP_RenderStyle:
		actor->RenderStyle.AsDWORD = value;
		break;

	case APROP_JumpZ:
		if ( actor->IsKindOf( RUNTIME_CLASS( APlayerPawn )))
			static_cast<APlayerPawn *>( actor )->JumpZ = value;
		break;

	default:
		CLIENT_PrintWarning( "client_SetThingProperty: Unknown property, %d!\n", static_cast<unsigned int> (property) );
		return;
	}
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
void ServerCommands::SetThingScale::Execute()
{
	if ( ContainsScaleX() )
		actor->scaleX = scaleX;

	if ( ContainsScaleY() )
		actor->scaleY = scaleY;
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
		Spawn<ATeleportFog>( actor->x, actor->y, actor->z + (( actor->flags & MF_MISSILE ) ? 0 : TELEFOGHEIGHT ), ALLOW_REPLACE );

	// Set the thing's new position.
	CLIENT_MoveThing( actor, x, y, z );

	// Spawn a teleport fog at the destination.
	if ( destfog )
	{
		// Spawn the fog slightly in front of the thing's destination.
		angle_t fineangle = angle >> ANGLETOFINESHIFT;

		Spawn<ATeleportFog>( actor->x + ( 20 * finecosine[fineangle] ),
			actor->y + ( 20 * finesine[fineangle] ),
			actor->z + (( actor->flags & MF_MISSILE ) ? 0 : TELEFOGHEIGHT ),
			ALLOW_REPLACE );
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

		// [BB] If the server is teleporting us, don't let our prediction get messed up.
		if ( actor == players[consoleplayer].mo )
			CLIENT_AdjustPredictionToServerSideConsolePlayerMove( x, y, z );
	}

	// Reset the thing's new reaction time.
	actor->reactiontime = reactiontime;

	// Set the thing's new angle.
	actor->angle = angle;

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
	CLIENT_SpawnThing( pufftype, x, y, z, id, SPAWNFLAG_PUFF );
}

//*****************************************************************************
//
void ServerCommands::SpawnPuffNoNetID::Execute()
{
	AActor *puff = CLIENT_SpawnThing( pufftype, x, y, z, -1, SPAWNFLAG_PUFF );

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
	StatusBar->AttachMessage( hudMessage, id );

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
	StatusBar->AttachMessage( hudMessage, id );

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
	StatusBar->AttachMessage( hudMessage, id );

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
	StatusBar->AttachMessage( hudMessage, id );

	// Log the message if desired.
	if ( log )
		CLIENT_LogHUDMessage( message, color );
}

//*****************************************************************************
//
static void client_SetGameMode( BYTESTREAM_s *pByteStream )
{
	UCVarValue	Value;

	GAMEMODE_SetCurrentMode ( static_cast<GAMEMODE_e> ( NETWORK_ReadByte( pByteStream ) ) );

	// [BB] The client doesn't necessarily know the game mode in P_SetupLevel, so we have to call this here.
	if ( domination )
		DOMINATION_Init();

	Value.Bool = !!NETWORK_ReadByte( pByteStream );
	instagib.ForceSet( Value, CVAR_Bool );

	Value.Bool = !!NETWORK_ReadByte( pByteStream );
	buckshot.ForceSet( Value, CVAR_Bool );
}

//*****************************************************************************
//
static void client_SetGameSkill( BYTESTREAM_s *pByteStream )
{
	UCVarValue	Value;

	// Read in the gameskill setting, and set gameskill to this setting.
	Value.Int = NETWORK_ReadByte( pByteStream );
	gameskill.ForceSet( Value, CVAR_Int );

	// Do the same for botskill.
	Value.Int = NETWORK_ReadByte( pByteStream );
	botskill.ForceSet( Value, CVAR_Int );
}

//*****************************************************************************
//
static void client_SetGameDMFlags( BYTESTREAM_s *pByteStream )
{
	UCVarValue	Value;

	// Read in the dmflags value, and set it to this value.
	Value.Int = NETWORK_ReadLong( pByteStream );
	dmflags.ForceSet( Value, CVAR_Int );

	// Do the same for dmflags2.
	Value.Int = NETWORK_ReadLong( pByteStream );
	dmflags2.ForceSet( Value, CVAR_Int );

	// ... and compatflags.
	Value.Int = NETWORK_ReadLong( pByteStream );
	compatflags.ForceSet( Value, CVAR_Int );

	// ... and compatflags.
	Value.Int = NETWORK_ReadLong( pByteStream );
	compatflags2.ForceSet( Value, CVAR_Int );

	// [BB] ... and zacompatflags.
	Value.Int = NETWORK_ReadLong( pByteStream );
	zacompatflags.ForceSet( Value, CVAR_Int );

	// [BB] ... and zadmflags.
	Value.Int = NETWORK_ReadLong( pByteStream );
	zadmflags.ForceSet( Value, CVAR_Int );
}

//*****************************************************************************
//
static void client_SetGameModeLimits( BYTESTREAM_s *pByteStream )
{
	UCVarValue	Value;

	// Read in, and set the value for fraglimit.
	Value.Int = NETWORK_ReadShort( pByteStream );
	fraglimit.ForceSet( Value, CVAR_Int );

	// Read in, and set the value for timelimit.
	Value.Float = NETWORK_ReadFloat( pByteStream );
	timelimit.ForceSet( Value, CVAR_Float );

	// Read in, and set the value for pointlimit.
	Value.Int = NETWORK_ReadShort( pByteStream );
	pointlimit.ForceSet( Value, CVAR_Int );

	// Read in, and set the value for duellimit.
	Value.Int = NETWORK_ReadByte( pByteStream );
	duellimit.ForceSet( Value, CVAR_Int );

	// Read in, and set the value for winlimit.
	Value.Int = NETWORK_ReadByte( pByteStream );
	winlimit.ForceSet( Value, CVAR_Int );

	// Read in, and set the value for wavelimit.
	Value.Int = NETWORK_ReadByte( pByteStream );
	wavelimit.ForceSet( Value, CVAR_Int );

	// Read in, and set the value for sv_cheats.
	Value.Int = NETWORK_ReadByte( pByteStream );
	sv_cheats.ForceSet( Value, CVAR_Int );
	// [BB] This ensures that am_cheat respects the sv_cheats value we just set.
	am_cheat.Callback();
	// [TP] Same for turbo
	turbo.Callback();

	// Read in, and set the value for sv_fastweapons.
	Value.Int = NETWORK_ReadByte( pByteStream );
	sv_fastweapons.ForceSet( Value, CVAR_Int );

	// Read in, and set the value for sv_maxlives.
	Value.Int = NETWORK_ReadByte( pByteStream );
	sv_maxlives.ForceSet( Value, CVAR_Int );

	// Read in, and set the value for sv_maxteams.
	Value.Int = NETWORK_ReadByte( pByteStream );
	sv_maxteams.ForceSet( Value, CVAR_Int );

	// [BB] Read in, and set the value for sv_gravity.
	Value.Float = NETWORK_ReadFloat( pByteStream );
	sv_gravity.ForceSet( Value, CVAR_Float );

	// [BB] Read in, and set the value for sv_aircontrol.
	Value.Float = NETWORK_ReadFloat( pByteStream );
	sv_aircontrol.ForceSet( Value, CVAR_Float );

	// [WS] Read in, and set the value for sv_coop_damagefactor.
	Value.Float = NETWORK_ReadFloat( pByteStream );
	sv_coop_damagefactor.ForceSet( Value, CVAR_Float );

	// [WS] Read in, and set the value for alwaysapplydmflags.
	Value.Bool = !!NETWORK_ReadByte( pByteStream );
	alwaysapplydmflags.ForceSet( Value, CVAR_Bool );

	// [AM] Read in, and set the value for lobby.
	Value.String = const_cast<char*>(NETWORK_ReadString( pByteStream ));
	lobby.ForceSet( Value, CVAR_String );

	// [TP] Yea.
	Value.Bool = !!NETWORK_ReadByte( pByteStream );
	sv_limitcommands.ForceSet( Value, CVAR_Bool );
}

//*****************************************************************************
//
static void client_SetGameEndLevelDelay( BYTESTREAM_s *pByteStream )
{
	ULONG	ulDelay;

	ulDelay = NETWORK_ReadShort( pByteStream );

	GAME_SetEndLevelDelay( ulDelay );
}

//*****************************************************************************
//
static void client_SetGameModeState( BYTESTREAM_s *pByteStream )
{
	ULONG	ulModeState;
	ULONG	ulCountdownTicks;

	ulModeState = NETWORK_ReadByte( pByteStream );
	ulCountdownTicks = NETWORK_ReadShort( pByteStream );

	if ( duel )
	{
		DUEL_SetState( (DUELSTATE_e)ulModeState );
		DUEL_SetCountdownTicks( ulCountdownTicks );
	}
	else if ( lastmanstanding || teamlms )
	{
		LASTMANSTANDING_SetState( (LMSSTATE_e)ulModeState );
		LASTMANSTANDING_SetCountdownTicks( ulCountdownTicks );
	}
	else if ( possession || teampossession )
	{
		POSSESSION_SetState( (PSNSTATE_e)ulModeState );
		if ( (PSNSTATE_e)ulModeState == PSNS_ARTIFACTHELD )
			POSSESSION_SetArtifactHoldTicks( ulCountdownTicks );
		else
			POSSESSION_SetCountdownTicks( ulCountdownTicks );
	}
	else if ( survival )
	{
		SURVIVAL_SetState( (SURVIVALSTATE_e)ulModeState );
		SURVIVAL_SetCountdownTicks( ulCountdownTicks );
	}
	else if ( invasion )
	{
		INVASION_SetState( (INVASIONSTATE_e)ulModeState );
		INVASION_SetCountdownTicks( ulCountdownTicks );
	}
}

//*****************************************************************************
//
static void client_SetDuelNumDuels( BYTESTREAM_s *pByteStream )
{
	ULONG	ulNumDuels;

	// Read in the number of duels that have occured.
	ulNumDuels = NETWORK_ReadByte( pByteStream );

	DUEL_SetNumDuels( ulNumDuels );
}

//*****************************************************************************
//
static void client_SetLMSSpectatorSettings( BYTESTREAM_s *pByteStream )
{
	UCVarValue	Value;

	Value.Int = NETWORK_ReadLong( pByteStream );
	lmsspectatorsettings.ForceSet( Value, CVAR_Int );
}

//*****************************************************************************
//
static void client_SetLMSAllowedWeapons( BYTESTREAM_s *pByteStream )
{
	UCVarValue	Value;

	Value.Int = NETWORK_ReadLong( pByteStream );
	lmsallowedweapons.ForceSet( Value, CVAR_Int );
}

//*****************************************************************************
//
static void client_SetInvasionNumMonstersLeft( BYTESTREAM_s *pByteStream )
{
	ULONG	ulNumMonstersLeft;
	ULONG	ulNumArchVilesLeft;

	// Read in the number of monsters left.
	ulNumMonstersLeft = NETWORK_ReadShort( pByteStream );

	// Read in the number of arch-viles left.
	ulNumArchVilesLeft = NETWORK_ReadShort( pByteStream );

	// Set the number of monsters/archies left.
	INVASION_SetNumMonstersLeft( ulNumMonstersLeft );
	INVASION_SetNumArchVilesLeft( ulNumArchVilesLeft );
}

//*****************************************************************************
//
static void client_SetInvasionWave( BYTESTREAM_s *pByteStream )
{
	ULONG	ulWave;

	// Read in the current wave we're on.
	ulWave = NETWORK_ReadByte( pByteStream );

	// Set the current wave in the invasion module.
	INVASION_SetCurrentWave( ulWave );
}

//*****************************************************************************
//
static void client_SetSimpleCTFSTMode( BYTESTREAM_s *pByteStream )
{
	bool	bSimpleCTFST;

	// Read in whether or not we're in the simple version of these game modes.
	bSimpleCTFST = !!NETWORK_ReadByte( pByteStream );

	// Set the simple CTF/ST mode.
	TEAM_SetSimpleCTFSTMode( bSimpleCTFST );
}

//*****************************************************************************
//
static void client_DoPossessionArtifactPickedUp( BYTESTREAM_s *pByteStream )
{
	ULONG	ulPlayer;
	ULONG	ulTicks;

	// Read in the player who picked up the possession artifact.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read in how many ticks remain until the player potentially scores a point.
	ulTicks = NETWORK_ReadShort( pByteStream );

	// If this is an invalid player, break out.
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	// If we're not playing possession, there's no need to do this.
	if (( possession == false ) && ( teampossession == false ))
		return;

	// Finally, call the function that handles the picking up of the artifact.
	POSSESSION_ArtifactPickedUp( &players[ulPlayer], ulTicks );
}

//*****************************************************************************
//
static void client_DoPossessionArtifactDropped( BYTESTREAM_s *pByteStream )
{
	// If we're not playing possession, there's no need to do this.
	if (( possession == false ) && ( teampossession == false ))
		return;

	// Simply call this function.
	POSSESSION_ArtifactDropped( );
}

//*****************************************************************************
//
static void client_DoGameModeFight( BYTESTREAM_s *pByteStream )
{
	ULONG	ulWave;

	// What wave are we starting? (invasion only).
	ulWave = NETWORK_ReadByte( pByteStream );

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
		INVASION_BeginWave( ulWave );
}

//*****************************************************************************
//
static void client_DoGameModeCountdown( BYTESTREAM_s *pByteStream )
{
	ULONG	ulTicks;

	ulTicks = NETWORK_ReadShort( pByteStream );

	// Begin the countdown.
	if ( duel )
		DUEL_StartCountdown( ulTicks );
	else if ( lastmanstanding || teamlms )
		LASTMANSTANDING_StartCountdown( ulTicks );
	else if ( possession || teampossession )
		POSSESSION_StartCountdown( ulTicks );
	else if ( survival )
		SURVIVAL_StartCountdown( ulTicks );
	else if ( invasion )
		INVASION_StartCountdown( ulTicks );
}

//*****************************************************************************
//
static void client_DoGameModeWinSequence( BYTESTREAM_s *pByteStream )
{
	ULONG	ulWinner;

	ulWinner = NETWORK_ReadByte( pByteStream );

	// Begin the win sequence.
	if ( duel )
		DUEL_DoWinSequence( ulWinner );
	else if ( lastmanstanding || teamlms )
	{
		if ( lastmanstanding && ( ulWinner == static_cast<ULONG>(consoleplayer) ))
			ANNOUNCER_PlayEntry( cl_announcer, "YouWin" );
		else if ( teamlms && players[consoleplayer].bOnTeam && ( ulWinner == players[consoleplayer].ulTeam ))
			ANNOUNCER_PlayEntry( cl_announcer, "YouWin" );

		LASTMANSTANDING_DoWinSequence( ulWinner );
	}
//	else if ( possession || teampossession )
//		POSSESSION_DoWinSequence( ulWinner );
	else if ( invasion )
		INVASION_DoWaveComplete( );
}

//*****************************************************************************
//
static void client_SetDominationState( BYTESTREAM_s *pByteStream )
{
	unsigned int NumPoints = NETWORK_ReadLong( pByteStream );

	// [BB] It's impossible that the server sends us this many points
	// in a single packet, so something must be wrong. Just parse
	// what the server has claimed to have send, but don't try to store
	// it or allocate memory for it.
	if ( NumPoints > MAX_UDP_PACKET )
	{
		for ( unsigned int i = 0; i < NumPoints; ++i )
			NETWORK_ReadByte( pByteStream );
		return;
	}

	unsigned int *PointOwners = new unsigned int[NumPoints];
	for(unsigned int i = 0;i < NumPoints;i++)
	{
		PointOwners[i] = NETWORK_ReadByte( pByteStream );
	}
	DOMINATION_LoadInit(NumPoints, PointOwners);
}

//*****************************************************************************
//
static void client_SetDominationPointOwnership( BYTESTREAM_s *pByteStream )
{
	unsigned int ulPoint = NETWORK_ReadByte( pByteStream );
	unsigned int ulPlayer = NETWORK_ReadByte( pByteStream );

	// If this is an invalid player, break out.
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	DOMINATION_SetOwnership(ulPoint, &players[ulPlayer]);
}

//*****************************************************************************
//
static void client_SetTeamFrags( BYTESTREAM_s *pByteStream )
{
	ULONG	ulTeam;
	LONG	lFragCount;
	bool	bAnnounce;

	// Read in the team.
	ulTeam = NETWORK_ReadByte( pByteStream );

	// Read in the fragcount.
	lFragCount = NETWORK_ReadShort( pByteStream );

	// Announce a lead change... but don't do it if we're receiving a snapshot of the level!
	bAnnounce = !!NETWORK_ReadByte( pByteStream );
	if ( g_ConnectionState != CTS_ACTIVE )
		bAnnounce = false;

	// Finally, set the team's fragcount.
	TEAM_SetFragCount( ulTeam, lFragCount, bAnnounce );
}

//*****************************************************************************
//
static void client_SetTeamScore( BYTESTREAM_s *pByteStream )
{
	ULONG	ulTeam;
	LONG	lScore;
	bool	bAnnounce;

	// Read in the team having its score updated.
	ulTeam = NETWORK_ReadByte( pByteStream );

	// Read in the team's new score.
	lScore = NETWORK_ReadShort( pByteStream );

	// Should it be announced?
	bAnnounce = !!NETWORK_ReadByte( pByteStream );
	
	// Don't announce the score change if we're receiving a snapshot of the level!
	if ( g_ConnectionState != CTS_ACTIVE )
		bAnnounce = false;

	TEAM_SetScore( ulTeam, lScore, bAnnounce );

	SCOREBOARD_RefreshHUD( );
}

//*****************************************************************************
//
static void client_SetTeamWins( BYTESTREAM_s *pByteStream )
{
	ULONG	ulTeamIdx;
	LONG	lWinCount;
	bool	bAnnounce;

	// Read in the team.
	ulTeamIdx = NETWORK_ReadByte( pByteStream );

	// Read in the wins.
	lWinCount = NETWORK_ReadShort( pByteStream );

	// Read in whether or not it should be announced.	
	bAnnounce = !!NETWORK_ReadByte( pByteStream );

	// Don't announce if we're receiving a snapshot of the level!
	if ( g_ConnectionState != CTS_ACTIVE )
		bAnnounce = false;

	// Finally, set the team's win count.
	TEAM_SetWinCount( ulTeamIdx, lWinCount, bAnnounce );
}

//*****************************************************************************
//
static void client_SetTeamReturnTicks( BYTESTREAM_s *pByteStream )
{
	ULONG	ulTeam;
	ULONG	ulTicks;

	// Read in the team having its return ticks altered.
	ulTeam = NETWORK_ReadByte( pByteStream );

	// Read in the return ticks value.
	ulTicks = NETWORK_ReadShort( pByteStream );

	// Finally, set the return ticks for the given team.
	TEAM_SetReturnTicks( ulTeam, ulTicks );
}

//*****************************************************************************
//
static void client_TeamFlagReturned( BYTESTREAM_s *pByteStream )
{
	ULONG	ulTeam;

	// Read in the team that the flag has been returned for.
	ulTeam = NETWORK_ReadByte( pByteStream );

	// Finally, just call this function that does all the dirty work.
	TEAM_ExecuteReturnRoutine( ulTeam, NULL );
}

//*****************************************************************************
//
static void client_TeamFlagDropped( BYTESTREAM_s *pByteStream )
{
	ULONG	ulPlayer;
	ULONG	ulTeamIdx;

	// Read in the player that dropped a flag.
	ulPlayer = NETWORK_ReadByte( pByteStream );
	ulTeamIdx = NETWORK_ReadByte( pByteStream );

	// Finally, just call this function that does all the dirty work.
	TEAM_FlagDropped( &players[ulPlayer], ulTeamIdx );
}

//*****************************************************************************
//
void ServerCommands::SpawnMissile::Execute()
{
	CLIENT_SpawnMissile( missileType, x, y, z, velX, velY, velZ, netID, targetNetID );
}

//*****************************************************************************
//
void ServerCommands::SpawnMissileExact::Execute()
{
	CLIENT_SpawnMissile( missileType, x, y, z, velX, velY, velZ, netID, targetNetID );
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

	// Move the new actor to the position.
	CLIENT_MoveThing( missile, x, y, z );

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
		angleoffset = 0;
		spawnclass = NULL;
		duration = 0;
		sparsity = 1.0f;
		drift = 1.0f;
	}

	angle_t angle = source->angle + angleoffset;
	P_DrawRailTrail( source, start, end, color1, color2, maxdiff, flags, spawnclass, angle, duration, sparsity, drift );
}

//*****************************************************************************
//
void ServerCommands::SetSectorFloorPlane::Execute()
{
	// Calculate the change in floor height.
	fixed_t delta = height - sector->floorplane.d;

	// Store the original height position.
	fixed_t lastPos = sector->floorplane.d;

	// Change the height.
	sector->floorplane.ChangeHeight( -delta );

	// Call this to update various actor's within the sector.
	P_ChangeSector( sector, false, -delta, 0, false );

	// Finally, adjust textures.
	sector->SetPlaneTexZ(sector_t::floor, sector->GetPlaneTexZ(sector_t::floor) + sector->floorplane.HeightDiff( lastPos ) );

	// [BB] We also need to move any linked sectors.
	P_MoveLinkedSectors(sector, false, -delta, false);
}

//*****************************************************************************
//
void ServerCommands::SetSectorCeilingPlane::Execute()
{
	// Calculate the change in ceiling height.
	fixed_t delta = height - sector->ceilingplane.d;

	// Store the original height position.
	fixed_t lastPos = sector->ceilingplane.d;

	// Change the height.
	sector->ceilingplane.ChangeHeight( delta );

	// Finally, adjust textures.
	sector->SetPlaneTexZ(sector_t::ceiling, sector->GetPlaneTexZ(sector_t::ceiling) + sector->ceilingplane.HeightDiff( lastPos ) );

	// [BB] We also need to move any linked sectors.
	P_MoveLinkedSectors(sector, false, delta, true);
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
	sector->SetAngle(sector_t::ceiling, ceilingRotation * ANGLE_1 );
	sector->SetAngle(sector_t::floor, floorRotation * ANGLE_1 );
}

//*****************************************************************************
//
void ServerCommands::SetSectorRotationByTag::Execute()
{
	int secnum = -1;

	while (( secnum = P_FindSectorFromTag( tag, secnum )) >= 0 )
	{
		sectors[secnum].SetAngle( sector_t::floor, floorRotation * ANGLE_1 );
		sectors[secnum].SetAngle( sector_t::ceiling, ceilingRotation * ANGLE_1 );
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
static void client_CallVote( BYTESTREAM_s *pByteStream )
{
	FString		command;
	FString		parameters;
	FString		reason;
	ULONG		ulVoteCaller;

	// Read in the vote starter.
	ulVoteCaller = NETWORK_ReadByte( pByteStream );

	// Read in the command.
	command = NETWORK_ReadString( pByteStream );

	// Read in the parameters.
	parameters = NETWORK_ReadString( pByteStream );
	
	// Read in the reason.
	reason = NETWORK_ReadString( pByteStream );

	// Begin the vote!
	CALLVOTE_BeginVote( command, parameters, reason, ulVoteCaller );
}

//*****************************************************************************
//
static void client_PlayerVote( BYTESTREAM_s *pByteStream )
{
	ULONG	ulPlayer;
	bool	bYes;

	// Read in the player making the vote.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Did the player vote yes?
	bYes = !!NETWORK_ReadByte( pByteStream );

	if ( bYes )
		CALLVOTE_VoteYes( ulPlayer );
	else
		CALLVOTE_VoteNo( ulPlayer );
}

//*****************************************************************************
//
static void client_VoteEnded( BYTESTREAM_s *pByteStream )
{
	bool	bPassed;

	// Did the vote pass?
	bPassed = !!NETWORK_ReadByte( pByteStream );

	CALLVOTE_EndVote( bPassed );
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
static void client_GiveInventory( BYTESTREAM_s *pByteStream )
{
	const PClass	*pType;
	ULONG			ulPlayer;
	USHORT			usActorNetworkIndex;
	LONG			lAmount;
	AInventory		*pInventory;

	// Read in the player ID.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read in the identification of the type of item to give.
	usActorNetworkIndex = NETWORK_ReadShort( pByteStream );

	// Read in the amount of this inventory type the player has.
	lAmount = NETWORK_ReadLong( pByteStream );

	// Check to make sure everything is valid. If not, break out.
	if (( PLAYER_IsValidPlayer( ulPlayer ) == false ) || ( players[ulPlayer].mo == NULL ))
		return;

	pType = NETWORK_GetClassFromIdentification( usActorNetworkIndex );
	if ( pType == NULL )
		return;

	// [BB] If pType is not derived from AInventory, the cast below will fail.
	if ( !(pType->IsDescendantOf( RUNTIME_CLASS( AInventory ))) )
		return;

	// [BB] Keep track of whether the player had a weapon.
	const bool playerHadNoWeapon = ( ( players[ulPlayer].ReadyWeapon == NULL ) &&  ( players[ulPlayer].PendingWeapon == WP_NOCHANGE ) );

	// Try to give the player the item.
	pInventory = static_cast<AInventory *>( Spawn( pType, 0, 0, 0, NO_REPLACE ));
	if ( pInventory != NULL )
	{
		if ( lAmount > 0 )
		{
			if ( pType->IsDescendantOf( RUNTIME_CLASS( ABasicArmorPickup )))
			{
				if ( static_cast<ABasicArmorPickup*>( pInventory )->SaveAmount != 0 )
					static_cast<ABasicArmorPickup*>( pInventory )->SaveAmount *= lAmount;
				else
					static_cast<ABasicArmorPickup*>( pInventory )->SaveAmount *= lAmount;
			}
			else if ( pType->IsDescendantOf( RUNTIME_CLASS( ABasicArmorBonus )))
			{
				static_cast<ABasicArmorBonus*>( pInventory )->SaveAmount *= lAmount;
				static_cast<ABasicArmorBonus*>( pInventory )->BonusCount *= lAmount;
			}
			else if ( pType->IsDescendantOf( RUNTIME_CLASS( AHealth ) ) )
			{
				if ( pInventory->MaxAmount > 0 )
					pInventory->Amount = MIN( lAmount, (LONG)pInventory->MaxAmount );
				else
					pInventory->Amount = lAmount;
			}
			else
				pInventory->Amount = lAmount;
		}
		if ( pInventory->CallTryPickup( players[ulPlayer].mo ) == false )
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
			 && ( players[ulPlayer].mo->FindInventory( pType ) == NULL ) )
			CLIENT_PrintWarning( "client_GiveInventory: Failed to give inventory type, %s!\n", NETWORK_GetClassNameFromIdentification( usActorNetworkIndex ));
		return;
	}

	// Don't count this towards the level statistics.
	if ( pInventory->flags & MF_COUNTITEM )
	{
		pInventory->flags &= ~MF_COUNTITEM;
		level.total_items--;
	}

	// Set the new amount of the inventory object.
	pInventory = players[ulPlayer].mo->FindInventory( pType );
	if ( pInventory )
		pInventory->Amount = lAmount;

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
	if ( players[ulPlayer].morphTics )
		players[ulPlayer].PendingWeapon = WP_NOCHANGE;

	// Since an item displayed on the HUD may have been given, refresh the HUD.
	SCOREBOARD_RefreshHUD( );

	// [BB] If this is not "our" player and this player didn't have a weapon before, we assume
	// that he was just spawned and didn't tell the server yet which weapon he selected. In this
	// case make sure that this pickup doesn't cause him to bring up a weapon and wait for the
	// server to tell us which weapon the player uses.
	if ( playerHadNoWeapon  && ( players[ulPlayer].bIsBot == false )&& ( ulPlayer != (ULONG)consoleplayer ) )
		PLAYER_ClearWeapon ( &players[ulPlayer] );
}

//*****************************************************************************
//
static void client_TakeInventory( BYTESTREAM_s *pByteStream )
{
	const PClass	*pType;
	ULONG			ulPlayer;
	USHORT			actorNetworkIndex;
	LONG			lAmount;
	AInventory		*pInventory;

	// Read in the player ID.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read in the identification of the type of item to take away.
	actorNetworkIndex = NETWORK_ReadShort( pByteStream );

	// Read in the new amount of this inventory type the player has.
	lAmount = NETWORK_ReadLong( pByteStream );

	// Check to make sure everything is valid. If not, break out.
	if (( PLAYER_IsValidPlayer( ulPlayer ) == false ) || ( players[ulPlayer].mo == NULL ))
		return;

	pType = NETWORK_GetClassFromIdentification( actorNetworkIndex );
	if ( pType == NULL )
		return;

	if ( pType->IsDescendantOf( RUNTIME_CLASS( AInventory )) == false )
		return;

	// Try to find this object within the player's personal inventory.
	pInventory = players[ulPlayer].mo->FindInventory( pType );

	// [TP] If we're trying to set the item amount to 0, then destroy the item if the player has it.
	if ( lAmount <= 0 )
	{
		if ( pInventory )
		{
			if ( pInventory->ItemFlags & IF_KEEPDEPLETED )
				pInventory->Amount = 0;
			else
				pInventory->Destroy( );
		}
	}
	else if ( lAmount > 0 )
	{
		// If the player doesn't have this type, give it to him.
		if ( pInventory == NULL )
			pInventory = players[ulPlayer].mo->GiveInventoryType( pType );

		// If he still doesn't have the object after trying to give it to him... then YIKES!
		if ( pInventory == NULL )
		{
			CLIENT_PrintWarning( "client_TakeInventory: Failed to give inventory type, %s!\n", pType->TypeName.GetChars());
			return;
		}

		// Set the new amount of the inventory object.
		pInventory->Amount = lAmount;
	}

	// Since an item displayed on the HUD may have been taken away, refresh the HUD.
	SCOREBOARD_RefreshHUD( );
}

//*****************************************************************************
//
static void client_GivePowerup( BYTESTREAM_s *pByteStream )
{
	const PClass	*pType;
	ULONG			ulPlayer;
	USHORT			usActorNetworkIndex;
	LONG			lAmount;
	LONG			lEffectTics;
	AInventory		*pInventory;

	// Read in the player ID.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read in the identification of the type of item to give.
	usActorNetworkIndex = NETWORK_ReadShort( pByteStream );

	// Read in the amount of this inventory type the player has.
	lAmount = NETWORK_ReadShort( pByteStream );

	// [TP]
	bool isRune = !!NETWORK_ReadByte( pByteStream );

	// Read in the amount of time left on this powerup.
	lEffectTics = ( isRune == false ) ? NETWORK_ReadShort( pByteStream ) : 0;

	// Check to make sure everything is valid. If not, break out.
	if (( PLAYER_IsValidPlayer( ulPlayer ) == false ) || ( players[ulPlayer].mo == NULL ))
		return;

	pType = NETWORK_GetClassFromIdentification( usActorNetworkIndex );
	if ( pType == NULL )
		return;

	// If this isn't a powerup, just quit.
	if ( pType->IsDescendantOf( RUNTIME_CLASS( APowerup )) == false )
		return;

	// Try to find this object within the player's personal inventory.
	pInventory = players[ulPlayer].mo->FindInventory( pType );

	// If the player doesn't have this type, give it to him.
	if ( pInventory == NULL )
		pInventory = players[ulPlayer].mo->GiveInventoryType( pType );

	// If he still doesn't have the object after trying to give it to him... then YIKES!
	if ( pInventory == NULL )
	{
		CLIENT_PrintWarning( "client_TakeInventory: Failed to give inventory type, %s!\n", NETWORK_GetClassNameFromIdentification( usActorNetworkIndex ));
		return;
	}

	// Set the new amount of the inventory object.
	pInventory->Amount = lAmount;
	if ( pInventory->Amount <= 0 )
	{
		pInventory->Destroy( );
		pInventory = NULL;
	}

	if ( pInventory )
	{
		static_cast<APowerup *>( pInventory )->EffectTics = lEffectTics;

		// [TP]
		if ( isRune )
			pInventory->Owner->Rune = static_cast<APowerup *>( pInventory );
	}

	// Since an item displayed on the HUD may have been given, refresh the HUD.
	SCOREBOARD_RefreshHUD( );
}

//*****************************************************************************
//
static void client_DoInventoryPickup( BYTESTREAM_s *pByteStream )
{
	ULONG			ulPlayer;
	FString			szClassName;
	FString			pszPickupMessage;
	AInventory		*pInventory;

	static LONG			s_lLastMessageTic = 0;
	static FString		s_szLastMessage;

	// Read in the player ID.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read in the class name of the item.
	szClassName = NETWORK_ReadString( pByteStream );

	// Read in the pickup message.
	pszPickupMessage = NETWORK_ReadString( pByteStream );

	// Check to make sure everything is valid. If not, break out.
	if (( PLAYER_IsValidPlayer( ulPlayer ) == false ) || ( players[ulPlayer].mo == NULL ))
		return;

	// If the player doesn't have this inventory item, break out.
	pInventory = static_cast<AInventory *>( Spawn( PClass::FindClass( szClassName ), 0, 0, 0, NO_REPLACE ));
	if ( pInventory == NULL )
		return;

	// Don't count this towards the level statistics.
	if ( pInventory->flags & MF_COUNTITEM )
	{
		// [BB] The server doesn't tell us about itemcount updates,
		// so try to keep track of this locally.
		players[ulPlayer].itemcount++;

		pInventory->flags &= ~MF_COUNTITEM;
		level.total_items--;
	}

	// Print out the pickup message.
	if (( players[ulPlayer].mo->CheckLocalView( consoleplayer )) &&
		(( s_lLastMessageTic != gametic ) || ( s_szLastMessage.CompareNoCase( pszPickupMessage ) != 0 )))
	{
		s_lLastMessageTic = gametic;
		s_szLastMessage = pszPickupMessage;

		// This code is from PrintPickupMessage().
		if ( pszPickupMessage.IsNotEmpty( ) )
		{
			if ( pszPickupMessage[0] == '$' )
				pszPickupMessage = GStrings( pszPickupMessage.GetChars( ) + 1 );

			Printf( PRINT_LOW, "%s\n", pszPickupMessage.GetChars( ) );
		}

		StatusBar->FlashCrosshair( );
	}

	// Play the inventory pickup sound and blend the screen.
	pInventory->PlayPickupSound( players[ulPlayer].mo );
	players[ulPlayer].bonuscount = BONUSADD;

	// Play the announcer pickup entry as well.
	if ( players[ulPlayer].mo->CheckLocalView( consoleplayer ) && cl_announcepickups )
		ANNOUNCER_PlayEntry( cl_announcer, pInventory->PickupAnnouncerEntry( ));

	// Finally, destroy the temporarily spawned inventory item.
	pInventory->Destroy( );
}

//*****************************************************************************
//
static void client_DestroyAllInventory( BYTESTREAM_s *pByteStream )
{
	ULONG			ulPlayer;

	// Read in the player ID.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Check to make sure everything is valid. If not, break out.
	if (( PLAYER_IsValidPlayer( ulPlayer ) == false ) || ( players[ulPlayer].mo == NULL ))
		return;

	// Finally, destroy the player's inventory.
	// [BB] Be careful here, we may not use mo->DestroyAllInventory( ), otherwise
	// AHexenArmor messes up.
	players[ulPlayer].mo->ClearInventory();
}

//*****************************************************************************
//
static void client_SetInventoryIcon( BYTESTREAM_s *pByteStream )
{
	const ULONG ulPlayer = NETWORK_ReadByte( pByteStream );
	const USHORT usActorNetworkIndex = NETWORK_ReadShort( pByteStream );
	const FString iconTexName = NETWORK_ReadString( pByteStream );

	// Check to make sure everything is valid. If not, break out.
	if (( PLAYER_IsValidPlayer( ulPlayer ) == false ) || ( players[ulPlayer].mo == NULL ))
		return;

	const PClass *pType = NETWORK_GetClassFromIdentification( usActorNetworkIndex );
	if ( pType == NULL )
		return;

	AInventory *pInventory = players[ulPlayer].mo->FindInventory( pType );

	if ( pInventory )
		pInventory->Icon = TexMan.GetTexture( iconTexName, 0 );
}

//*****************************************************************************
//
static void client_DoDoor( BYTESTREAM_s *pByteStream )
{
	LONG			lSectorID;
	sector_t		*pSector;
	BYTE			type;
	LONG			lSpeed;
	LONG			lDirection;
	LONG			lLightTag;
	LONG			lDoorID;
	DDoor			*pDoor;

	// Read in the sector ID.
	lSectorID = NETWORK_ReadShort( pByteStream );

	// Read in the door type.
	type = NETWORK_ReadByte( pByteStream );

	// Read in the speed.
	lSpeed = NETWORK_ReadLong( pByteStream );

	// Read in the direction.
	lDirection = NETWORK_ReadByte( pByteStream );

	// Read in the delay.
	lLightTag = NETWORK_ReadShort( pByteStream );

	// Read in the door ID.
	lDoorID = NETWORK_ReadShort( pByteStream );

	// Make sure the sector ID is valid.
	if (( lSectorID >= 0 ) && ( lSectorID < numsectors ))
		pSector = &sectors[lSectorID];
	else
		return;

	// Since we still want to receive direction as a byte, but -1 can't be represented in byte
	// form, adjust the value into something that can be represented.
	lDirection = CLIENT_AdjustDoorDirection( lDirection );
	if ( lDirection == INT_MAX )
		return;

	// If door already has a thinker, we can't spawn a new door on it.
	if ( pSector->ceilingdata )
	{
		CLIENT_PrintWarning( "client_DoDoor: WARNING! Door's sector already has a ceiling mover attached to it!\n" );
		return;
	}

	// Create the new door.
	if ( (pDoor = new DDoor( pSector, (DDoor::EVlDoor)type, lSpeed, 0, lLightTag, g_ConnectionState != CTS_ACTIVE )) )
	{
		pDoor->SetID( lDoorID );
		pDoor->SetDirection( lDirection );
	}
}

//*****************************************************************************
//
static void client_DestroyDoor( BYTESTREAM_s *pByteStream )
{
	DDoor	*pDoor;
	LONG	lDoorID;

	// Read in the door ID.
	lDoorID = NETWORK_ReadShort( pByteStream );

	pDoor = P_GetDoorByID( lDoorID );
	if ( pDoor == NULL )
	{
		CLIENT_PrintWarning( "client_DestroyDoor: Couldn't find door with ID: %ld!\n", lDoorID );
		return;
	}

	pDoor->Destroy( );
}

//*****************************************************************************
//
static void client_ChangeDoorDirection( BYTESTREAM_s *pByteStream )
{
	DDoor	*pDoor;
	LONG	lDoorID;
	LONG	lDirection;

	// Read in the door ID.
	lDoorID = NETWORK_ReadShort( pByteStream );

	// Read in the new direction the door should move in.
	lDirection = NETWORK_ReadByte( pByteStream );

	// Since we still want to receive direction as a byte, but -1 can't be represented in byte
	// form, adjust the value into something that can be represented.
	lDirection = CLIENT_AdjustDoorDirection( lDirection );
	if ( lDirection == INT_MAX )
		return;

	pDoor = P_GetDoorByID( lDoorID );
	if ( pDoor == NULL )
	{
		CLIENT_PrintWarning( "client_ChangeDoorDirection: Couldn't find door with ID: %ld!\n", lDoorID );
		return;
	}

	pDoor->SetDirection( lDirection );

	// Don't play a sound if the door is now motionless!
	if ( lDirection != 0 )
		pDoor->DoorSound( lDirection == 1 );
}

//*****************************************************************************
//
static void client_DoFloor( BYTESTREAM_s *pByteStream )
{
	LONG			lType;
	LONG			lDirection;
	LONG			FloorDestDist;
	LONG			lSpeed;
	LONG			lSectorID;
	LONG			Crush;
	bool			Hexencrush;
	LONG			lFloorID;
	sector_t		*pSector;
	DFloor			*pFloor;

	// Read in the type of floor.
	lType = NETWORK_ReadByte( pByteStream );

	// Read in the sector ID.
	lSectorID = NETWORK_ReadShort( pByteStream );

	// Read in the direction of the floor.
	lDirection = NETWORK_ReadByte( pByteStream );

	// Read in the speed of the floor.
	lSpeed = NETWORK_ReadLong( pByteStream );

	// Read in the floor's destination height.
	FloorDestDist = NETWORK_ReadLong( pByteStream );

	// Read in the floor's crush.
	Crush = static_cast<SBYTE>( NETWORK_ReadByte( pByteStream ) );

	// Read in the floor's crush type.
	Hexencrush = !!NETWORK_ReadByte( pByteStream );

	// Read in the floor's network ID.
	lFloorID = NETWORK_ReadShort( pByteStream );

	// Since we still want to receive direction as a byte, but -1 can't be represented in byte
	// form, adjust the value into something that can be represented.
	lDirection = CLIENT_AdjustFloorDirection( lDirection );
	if ( lDirection == INT_MAX )
		return;

	// Invalid sector.
	if (( lSectorID >= numsectors ) || ( lSectorID < 0 ))
		return;

	pSector = &sectors[lSectorID];

	// If the sector already has activity, don't override it.
	if ( pSector->floordata )
		return;

	pFloor = new DFloor( pSector );
	pFloor->SetType( (DFloor::EFloor)lType );
	pFloor->SetCrush( Crush );
	pFloor->SetHexencrush( Hexencrush );
	pFloor->SetDirection( lDirection );
	pFloor->SetFloorDestDist( FloorDestDist );
	pFloor->SetSpeed( lSpeed );
	pFloor->SetID( lFloorID );
}

//*****************************************************************************
//
static void client_DestroyFloor( BYTESTREAM_s *pByteStream )
{
	DFloor		*pFloor;
	LONG		lFloorID;

	// Read in the floor ID.
	lFloorID = NETWORK_ReadShort( pByteStream );

	pFloor = P_GetFloorByID( lFloorID );
	if ( pFloor == NULL )
	{
		CLIENT_PrintWarning( "client_ChangeFloorType: Couldn't find floor with ID: %ld!\n", lFloorID );
		return;
	}

	SN_StopSequence( pFloor->GetSector( ), CHAN_FLOOR );
	pFloor->Destroy( );
}

//*****************************************************************************
//
static void client_ChangeFloorDirection( BYTESTREAM_s *pByteStream )
{
	DFloor		*pFloor;
	LONG		lFloorID;
	LONG		lDirection;

	// Read in the floor ID.
	lFloorID = NETWORK_ReadShort( pByteStream );

	// Read in the new floor direction.
	lDirection = NETWORK_ReadByte( pByteStream );

	// Since we still want to receive direction as a byte, but -1 can't be represented in byte
	// form, adjust the value into something that can be represented.
	lDirection = CLIENT_AdjustFloorDirection( lDirection );
	if ( lDirection == INT_MAX )
		return;

	pFloor = P_GetFloorByID( lFloorID );
	if ( pFloor == NULL )
	{
		CLIENT_PrintWarning( "client_ChangeFloorType: Couldn't find floor with ID: %ld!\n", lFloorID );
		return;
	}

	pFloor->SetDirection( lDirection );
}

//*****************************************************************************
//
static void client_ChangeFloorType( BYTESTREAM_s *pByteStream )
{
	DFloor		*pFloor;
	LONG		lFloorID;
	LONG		lType;

	// Read in the floor ID.
	lFloorID = NETWORK_ReadShort( pByteStream );

	// Read in the new type of floor this is.
	lType = NETWORK_ReadByte( pByteStream );

	pFloor = P_GetFloorByID( lFloorID );
	if ( pFloor == NULL )
	{
		CLIENT_PrintWarning( "client_ChangeFloorType: Couldn't find ceiling with ID: %ld!\n", lFloorID );
		return;
	}

	pFloor->SetType( (DFloor::EFloor)lType );
}

//*****************************************************************************
//
static void client_ChangeFloorDestDist( BYTESTREAM_s *pByteStream )
{
	DFloor		*pFloor;
	LONG		lFloorID;
	fixed_t		DestDist;

	// Read in the floor ID.
	lFloorID = NETWORK_ReadShort( pByteStream );

	// Read in the new floor destination distance.
	DestDist = NETWORK_ReadLong( pByteStream );

	pFloor = P_GetFloorByID( lFloorID );
	if ( pFloor == NULL )
	{
		CLIENT_PrintWarning( "client_ChangeFloorType: Couldn't find floor with ID: %ld!\n", lFloorID );
		return;
	}

	pFloor->SetFloorDestDist( DestDist );
}

//*****************************************************************************
//
static void client_StartFloorSound( BYTESTREAM_s *pByteStream )
{
	DFloor		*pFloor;
	LONG		lFloorID;

	// Read in the floor ID.
	lFloorID = NETWORK_ReadShort( pByteStream );

	pFloor = P_GetFloorByID( lFloorID );
	if ( pFloor == NULL )
	{
		CLIENT_PrintWarning( "client_StartFloorSound: Couldn't find floor with ID: %ld!\n", lFloorID );
		return;
	}

	// Finally, start playing the floor's sound sequence.
	pFloor->StartFloorSound( );
}

//*****************************************************************************
//
static void client_BuildStair( BYTESTREAM_s *pByteStream )
{
	// Read in the type of floor.
	int Type = NETWORK_ReadByte( pByteStream );

	// Read in the sector ID.
	int SectorID = NETWORK_ReadShort( pByteStream );

	// Read in the direction of the floor.
	int Direction = static_cast<SBYTE>( NETWORK_ReadByte( pByteStream ) );

	// Read in the speed of the floor.
	fixed_t Speed = NETWORK_ReadLong( pByteStream );

	// Read in the floor's destination height.
	fixed_t FloorDestDist = NETWORK_ReadLong( pByteStream );

	// Read in the floor's crush.
	int Crush = static_cast<SBYTE>( NETWORK_ReadByte( pByteStream ) );

	// Read in the floor's crush type.
	bool Hexencrush = !!NETWORK_ReadByte( pByteStream );

	// Read in the floor's reset count.
	int ResetCount = NETWORK_ReadLong( pByteStream );

	// Read in the floor's delay time.
	int Delay = NETWORK_ReadLong( pByteStream );

	// Read in the floor's pause time.
	int PauseTime = NETWORK_ReadLong( pByteStream );

	// Read in the floor's step time.
	int StepTime = NETWORK_ReadLong( pByteStream );

	// Read in the floor's per step time.
	int PerStepTime = NETWORK_ReadLong( pByteStream );

	// Read in the floor's network ID.
	int FloorID = NETWORK_ReadShort( pByteStream );

	// Invalid sector.
	if (( SectorID >= numsectors ) || ( SectorID < 0 ))
		return;

	sector_t *sector = &sectors[SectorID];

	// If the sector already has activity, don't override it.
	if ( sector->floordata )
		return;

	DFloor *floor = new DFloor( sector );
	floor->SetType( (DFloor::EFloor)Type );
	floor->SetCrush( Crush );
	floor->SetHexencrush( Hexencrush );
	floor->SetDirection( Direction );
	floor->SetFloorDestDist( FloorDestDist );
	floor->SetSpeed( Speed );
	floor->SetResetCount( ResetCount );
	floor->SetDelay( Delay );
	floor->SetPauseTime( PauseTime );
	floor->SetStepTime( StepTime );
	floor->SetPerStepTime( PerStepTime );
	floor->SetID( FloorID );
}

//*****************************************************************************
//
static void client_DoCeiling( BYTESTREAM_s *pByteStream )
{
	LONG			lType;
	fixed_t			BottomHeight;
	fixed_t			TopHeight;
	LONG			lSpeed;
	LONG			lCrush;
	bool			Hexencrush;
	LONG			lSilent;
	LONG			lDirection;
	LONG			lSectorID;
	LONG			lCeilingID;
	sector_t		*pSector;
	DCeiling		*pCeiling;

	// Read in the type of ceiling this is.
	lType = NETWORK_ReadByte( pByteStream );

	// Read in the sector this ceiling is attached to.
	lSectorID = NETWORK_ReadShort( pByteStream );

	// Read in the direction this ceiling is moving in.
	lDirection = NETWORK_ReadByte( pByteStream );

	// Read in the lowest distance the ceiling can travel before it stops.
	BottomHeight = NETWORK_ReadLong( pByteStream );

	// Read in the highest distance the ceiling can travel before it stops.
	TopHeight = NETWORK_ReadLong( pByteStream );

	// Read in the speed of the ceiling.
	lSpeed = NETWORK_ReadLong( pByteStream );

	// Does this ceiling damage those who get squashed by it?
	lCrush = static_cast<SBYTE>( NETWORK_ReadByte( pByteStream ) );

	// Is this ceiling crush Hexen style?
	Hexencrush = !!NETWORK_ReadByte( pByteStream );

	// Does this ceiling make noise?
	lSilent = NETWORK_ReadShort( pByteStream );

	// Read in the network ID of the ceiling.
	lCeilingID = NETWORK_ReadShort( pByteStream );

	// Since we still want to receive direction as a byte, but -1 can't be represented in byte
	// form, adjust the value into something that can be represented.
	lDirection = CLIENT_AdjustCeilingDirection( lDirection );
	if ( lDirection == INT_MAX )
		return;

	// Invalid sector.
	if (( lSectorID >= numsectors ) || ( lSectorID < 0 ))
		return;

	pSector = &sectors[lSectorID];

	pCeiling = new DCeiling( pSector, lSpeed, 0, lSilent );
	pCeiling->SetBottomHeight( BottomHeight );
	pCeiling->SetTopHeight( TopHeight );
	pCeiling->SetCrush( lCrush );
	pCeiling->SetHexencrush( Hexencrush );
	pCeiling->SetDirection( lDirection );
	pCeiling->SetID( lCeilingID );
}

//*****************************************************************************
//
static void client_DestroyCeiling( BYTESTREAM_s *pByteStream )
{
	DCeiling	*pCeiling;
	LONG		lCeilingID;

	// Read in the ceiling ID.
	lCeilingID = NETWORK_ReadShort( pByteStream );

	pCeiling = P_GetCeilingByID( lCeilingID );
	if ( pCeiling == NULL )
	{
		CLIENT_PrintWarning( "client_DestroyCeiling: Couldn't find ceiling with ID: %ld!\n", lCeilingID );
		return;
	}

	SN_StopSequence( pCeiling->GetSector( ), CHAN_CEILING );
	pCeiling->Destroy( );
}

//*****************************************************************************
//
static void client_ChangeCeilingDirection( BYTESTREAM_s *pByteStream )
{
	DCeiling	*pCeiling;
	LONG		lCeilingID;
	LONG		lDirection;

	// Read in the ceiling ID.
	lCeilingID = NETWORK_ReadShort( pByteStream );

	// Read in the new ceiling direction.
	lDirection = NETWORK_ReadByte( pByteStream );

	// Since we still want to receive direction as a byte, but -1 can't be represented in byte
	// form, adjust the value into something that can be represented.
	lDirection = CLIENT_AdjustCeilingDirection( lDirection );
	if ( lDirection == INT_MAX )
		return;

	pCeiling = P_GetCeilingByID( lCeilingID );
	if ( pCeiling == NULL )
	{
		CLIENT_PrintWarning( "client_ChangeCeilingDirection: Couldn't find ceiling with ID: %ld!\n", lCeilingID );
		return;
	}

	// Finally, set the new ceiling direction.
	pCeiling->SetDirection( lDirection );
}

//*****************************************************************************
//
static void client_ChangeCeilingSpeed( BYTESTREAM_s *pByteStream )
{
	DCeiling	*pCeiling;
	LONG		lCeilingID;
	LONG		lSpeed;

	// Read in the ceiling ID.
	lCeilingID = NETWORK_ReadShort( pByteStream );

	// Read in the new ceiling speed.
	lSpeed = NETWORK_ReadLong( pByteStream );

	pCeiling = P_GetCeilingByID( lCeilingID );
	if ( pCeiling == NULL )
	{
		CLIENT_PrintWarning( "client_ChangeCeilingSpeed: Couldn't find ceiling with ID: %ld!\n", lCeilingID );
		return;
	}

	pCeiling->SetSpeed( lSpeed );
}

//*****************************************************************************
//
static void client_PlayCeilingSound( BYTESTREAM_s *pByteStream )
{
	DCeiling	*pCeiling;
	LONG		lCeilingID;

	// Read in the ceiling ID.
	lCeilingID = NETWORK_ReadShort( pByteStream );

	pCeiling = P_GetCeilingByID( lCeilingID );
	if ( pCeiling == NULL )
	{
		CLIENT_PrintWarning( "client_PlayCeilingSound: Couldn't find ceiling with ID: %ld!\n", lCeilingID );
		return;
	}

	pCeiling->PlayCeilingSound( );
}

//*****************************************************************************
//
static void client_DoPlat( BYTESTREAM_s *pByteStream )
{
	LONG			lType;
	LONG			lStatus;
	fixed_t			High;
	fixed_t			Low;
	LONG			lSpeed;
	LONG			lSectorID;
	LONG			lPlatID;
	sector_t		*pSector;
	DPlat			*pPlat;

	// Read in the type of plat.
	lType = NETWORK_ReadByte( pByteStream );

	// Read in the sector ID.
	lSectorID = NETWORK_ReadShort( pByteStream );

	// Read in the plat status (moving up, down, etc.).
	lStatus = NETWORK_ReadByte( pByteStream );

	// Read in the high range of the plat.
	High = NETWORK_ReadLong( pByteStream );

	// Read in the low range of the plat.
	Low = NETWORK_ReadLong( pByteStream );

	// Read in the speed.
	lSpeed = NETWORK_ReadLong( pByteStream );

	// Read in the plat ID.
	lPlatID = NETWORK_ReadShort( pByteStream );

	// Invalid sector.
	if (( lSectorID >= numsectors ) || ( lSectorID < 0 ))
		return;

	pSector = &sectors[lSectorID];

	// Create the plat, and set all its attributes that were read in.
	pPlat = new DPlat( pSector );
	pPlat->SetType( (DPlat::EPlatType)lType );
	pPlat->SetStatus( lStatus );
	pPlat->SetHigh( High );
	pPlat->SetLow( Low );
	pPlat->SetSpeed( lSpeed );
	pPlat->SetID( lPlatID );

	// Now, set other properties that don't really matter.
	pPlat->SetCrush( -1 );
	pPlat->SetTag( 0 );

	// Just set the delay to 0. The server will tell us when it should move again.
	pPlat->SetDelay( 0 );
}

//*****************************************************************************
//
static void client_DestroyPlat( BYTESTREAM_s *pByteStream )
{
	DPlat	*pPlat;
	LONG	lPlatID;

	// Read in the plat ID.
	lPlatID = NETWORK_ReadShort( pByteStream );

	pPlat = P_GetPlatByID( lPlatID );
	if ( pPlat == NULL )
	{
		CLIENT_PrintWarning( "client_DestroyPlat: Couldn't find plat with ID: %ld!\n", lPlatID );
		return;
	}

	pPlat->Destroy( );
}

//*****************************************************************************
//
static void client_ChangePlatStatus( BYTESTREAM_s *pByteStream )
{
	DPlat	*pPlat;
	LONG	lPlatID;
	LONG	lStatus;

	// Read in the plat ID.
	lPlatID = NETWORK_ReadShort( pByteStream );

	// Read in the direction (aka status).
	lStatus = NETWORK_ReadByte( pByteStream );

	pPlat = P_GetPlatByID( lPlatID );
	if ( pPlat == NULL )
	{
		CLIENT_PrintWarning( "client_ChangePlatStatus: Couldn't find plat with ID: %ld!\n", lPlatID );
		return;
	}

	pPlat->SetStatus( lStatus );
}

//*****************************************************************************
//
static void client_PlayPlatSound( BYTESTREAM_s *pByteStream )
{
	DPlat	*pPlat;
	LONG	lPlatID;
	LONG	lSoundType;

	// Read in the plat ID.
	lPlatID = NETWORK_ReadShort( pByteStream );

	// Read in the type of sound to be played.
	lSoundType = NETWORK_ReadByte( pByteStream );

	pPlat = P_GetPlatByID( lPlatID );
	if ( pPlat == NULL )
	{
		CLIENT_PrintWarning( "client_PlayPlatSound: Couldn't find plat with ID: %ld!\n", lPlatID );
		return;
	}

	switch ( lSoundType )
	{
	case 0:

		SN_StopSequence( pPlat->GetSector( ), CHAN_FLOOR );
		break;
	case 1:

		pPlat->PlayPlatSound( "Platform" );
		break;
	case 2:

		SN_StartSequence( pPlat->GetSector( ), CHAN_FLOOR, "Silence", 0 );
		break;
	case 3:

		pPlat->PlayPlatSound( "Floor" );
		break;
	}
}

//*****************************************************************************
//
static void client_DoElevator( BYTESTREAM_s *pByteStream )
{
	LONG			lType;
	LONG			lSectorID;
	LONG			lSpeed;
	LONG			lDirection;
	LONG			lFloorDestDist;
	LONG			lCeilingDestDist;
	LONG			lElevatorID;
	sector_t		*pSector;
	DElevator		*pElevator;

	// Read in the type of elevator.
	lType = NETWORK_ReadByte( pByteStream );

	// Read in the sector ID.
	lSectorID = NETWORK_ReadShort( pByteStream );

	// Read in the speed.
	lSpeed = NETWORK_ReadLong( pByteStream );

	// Read in the direction.
	lDirection = NETWORK_ReadByte( pByteStream );

	// Read in the floor's destination distance.
	lFloorDestDist = NETWORK_ReadLong( pByteStream );

	// Read in the ceiling's destination distance.
	lCeilingDestDist = NETWORK_ReadLong( pByteStream );

	// Read in the elevator ID.
	lElevatorID = NETWORK_ReadShort( pByteStream );

	// Invalid sector.
	if (( lSectorID >= numsectors ) || ( lSectorID < 0 ))
		return;

	// Since we still want to receive direction as a byte, but -1 can't be represented in byte
	// form, adjust the value into something that can be represented.
	lDirection = CLIENT_AdjustElevatorDirection( lDirection );
	if ( lDirection == INT_MAX )
		return;

	pSector = &sectors[lSectorID];

	// Create the elevator, and set all its attributes that were read in.
	pElevator = new DElevator( pSector );
	pElevator->SetType( (DElevator::EElevator)lType );
	pElevator->SetSpeed( lSpeed );
	pElevator->SetDirection( lDirection );
	pElevator->SetFloorDestDist( lFloorDestDist );
	pElevator->SetCeilingDestDist( lCeilingDestDist );
	pElevator->SetID( lElevatorID );
}

//*****************************************************************************
//
static void client_DestroyElevator( BYTESTREAM_s *pByteStream )
{
	LONG		lElevatorID;
	DElevator	*pElevator;

	// Read in the elevator ID.
	lElevatorID = NETWORK_ReadShort( pByteStream );

	pElevator = P_GetElevatorByID( lElevatorID );
	if ( pElevator == NULL )
	{
		CLIENT_PrintWarning( "client_DestroyElevator: Couldn't find elevator with ID: %ld!\n", lElevatorID );
		return;
	}

	/* [BB] I think ZDoom does all this is Destroy now.
	pElevator->GetSector( )->floordata = NULL;
	pElevator->GetSector( )->ceilingdata = NULL;
	stopinterpolation( INTERP_SectorFloor, pElevator->GetSector( ));
	stopinterpolation( INTERP_SectorCeiling, pElevator->GetSector( ));
	*/

	// Finally, destroy the elevator.
	pElevator->Destroy( );
}

//*****************************************************************************
//
static void client_StartElevatorSound( BYTESTREAM_s *pByteStream )
{
	LONG		lElevatorID;
	DElevator	*pElevator;

	// Read in the elevator ID.
	lElevatorID = NETWORK_ReadShort( pByteStream );

	pElevator = P_GetElevatorByID( lElevatorID );
	if ( pElevator == NULL )
	{
		CLIENT_PrintWarning( "client_StartElevatorSound: Couldn't find elevator with ID: %ld!\n", lElevatorID );
		return;
	}

	// Finally, start the elevator sound.
	pElevator->StartFloorSound( );
}

//*****************************************************************************
//
static void client_DoPillar( BYTESTREAM_s *pByteStream )
{
	LONG			lType;
	LONG			lSectorID;
	LONG			lFloorSpeed;
	LONG			lCeilingSpeed;
	LONG			lFloorTarget;
	LONG			lCeilingTarget;
	LONG			Crush;
	bool			Hexencrush;
	LONG			lPillarID;
	sector_t		*pSector;
	DPillar			*pPillar;

	// Read in the type of pillar.
	lType = NETWORK_ReadByte( pByteStream );

	// Read in the sector ID.
	lSectorID = NETWORK_ReadShort( pByteStream );

	// Read in the speeds.
	lFloorSpeed = NETWORK_ReadLong( pByteStream );
	lCeilingSpeed = NETWORK_ReadLong( pByteStream );

	// Read in the targets.
	lFloorTarget = NETWORK_ReadLong( pByteStream );
	lCeilingTarget = NETWORK_ReadLong( pByteStream );

	// Read in the crush info.
	Crush = static_cast<SBYTE>( NETWORK_ReadByte( pByteStream ) );
	Hexencrush = !!NETWORK_ReadByte( pByteStream );

	// Read in the pillar ID.
	lPillarID = NETWORK_ReadShort( pByteStream );

	// Invalid sector.
	if (( lSectorID >= numsectors ) || ( lSectorID < 0 ))
		return;

	pSector = &sectors[lSectorID];

	// Create the pillar, and set all its attributes that were read in.
	pPillar = new DPillar( pSector );
	pPillar->SetType( (DPillar::EPillar)lType );
	pPillar->SetFloorSpeed( lFloorSpeed );
	pPillar->SetCeilingSpeed( lCeilingSpeed );
	pPillar->SetFloorTarget( lFloorTarget );
	pPillar->SetCeilingTarget( lCeilingTarget );
	pPillar->SetCrush( Crush );
	pPillar->SetHexencrush( Hexencrush );
	pPillar->SetID( lPillarID );

	// Begin playing the sound sequence for the pillar.
	if ( pSector->seqType >= 0 )
		SN_StartSequence( pSector, CHAN_FLOOR, pSector->seqType, SEQ_PLATFORM, 0 );
	else
		SN_StartSequence( pSector, CHAN_FLOOR, "Floor", 0 );
}

//*****************************************************************************
//
static void client_DestroyPillar( BYTESTREAM_s *pByteStream )
{
	LONG		lPillarID;
	DPillar		*pPillar;

	// Read in the elevator ID.
	lPillarID = NETWORK_ReadShort( pByteStream );

	pPillar = P_GetPillarByID( lPillarID );
	if ( pPillar == NULL )
	{
		CLIENT_PrintWarning( "client_DestroyPillar: Couldn't find pillar with ID: %ld!\n", lPillarID );
		return;
	}

	// Finally, destroy the pillar.
	pPillar->Destroy( );
}

//*****************************************************************************
//
static void client_DoWaggle( BYTESTREAM_s *pByteStream )
{
	bool			bCeiling;
	LONG			lSectorID;
	LONG			lOriginalDistance;
	LONG			lAccumulator;
	LONG			lAccelerationDelta;
	LONG			lTargetScale;
	LONG			lScale;
	LONG			lScaleDelta;
	LONG			lTicker;
	LONG			lState;
	LONG			lWaggleID;
	sector_t		*pSector;
	DWaggleBase		*pWaggle;

	// Read in whether or not this is a ceiling waggle.
	bCeiling = !!NETWORK_ReadByte( pByteStream );

	// Read in the sector ID.
	lSectorID = NETWORK_ReadShort( pByteStream );

	// Read in the waggle's attributes.
	lOriginalDistance = NETWORK_ReadLong( pByteStream );
	lAccumulator = NETWORK_ReadLong( pByteStream );
	lAccelerationDelta = NETWORK_ReadLong( pByteStream );
	lTargetScale = NETWORK_ReadLong( pByteStream );
	lScale = NETWORK_ReadLong( pByteStream );
	lScaleDelta = NETWORK_ReadLong( pByteStream );
	lTicker = NETWORK_ReadLong( pByteStream );

	// Read in the state the waggle is in.
	lState = NETWORK_ReadByte( pByteStream );

	// Read in the waggle ID.
	lWaggleID = NETWORK_ReadShort( pByteStream );

	// Invalid sector.
	if (( lSectorID >= numsectors ) || ( lSectorID < 0 ))
		return;

	pSector = &sectors[lSectorID];

	// Create the waggle, and set all its attributes that were read in.
	if ( bCeiling )
		pWaggle = static_cast<DWaggleBase *>( new DCeilingWaggle( pSector ));
	else
		pWaggle = static_cast<DWaggleBase *>( new DFloorWaggle( pSector ));
	pWaggle->SetOriginalDistance( lOriginalDistance );
	pWaggle->SetAccumulator( lAccumulator );
	pWaggle->SetAccelerationDelta( lAccelerationDelta );
	pWaggle->SetTargetScale( lTargetScale );
	pWaggle->SetScale( lScale );
	pWaggle->SetScaleDelta( lScaleDelta );
	pWaggle->SetTicker( lTicker );
	pWaggle->SetState( lState );
	pWaggle->SetID( lWaggleID );
}

//*****************************************************************************
//
static void client_DestroyWaggle( BYTESTREAM_s *pByteStream )
{
	LONG			lWaggleID;
	DWaggleBase		*pWaggle;

	// Read in the waggle ID.
	lWaggleID = NETWORK_ReadShort( pByteStream );

	pWaggle = P_GetWaggleByID( lWaggleID );
	if ( pWaggle == NULL )
	{
		CLIENT_PrintWarning( "client_DestroyWaggle: Couldn't find waggle with ID: %ld!\n", lWaggleID );
		return;
	}

	// Finally, destroy the waggle.
	pWaggle->Destroy( );
}

//*****************************************************************************
//
static void client_UpdateWaggle( BYTESTREAM_s *pByteStream )
{
	LONG			lWaggleID;
	LONG			lAccumulator;
	DWaggleBase		*pWaggle;

	// Read in the waggle ID.
	lWaggleID = NETWORK_ReadShort( pByteStream );

	// Read in the waggle's accumulator.
	lAccumulator = NETWORK_ReadLong( pByteStream );

	pWaggle = P_GetWaggleByID( lWaggleID );
	if ( pWaggle == NULL )
	{
		CLIENT_PrintWarning( "client_DestroyWaggle: Couldn't find waggle with ID: %ld!\n", lWaggleID );
		return;
	}

	// Finally, update the waggle's accumulator.
	pWaggle->SetAccumulator( lAccumulator );
}

//*****************************************************************************
//
static void client_DoRotatePoly( BYTESTREAM_s *pByteStream )
{
	LONG			lSpeed;
	LONG			lPolyNum;
	FPolyObj		*pPoly;
	DRotatePoly		*pRotatePoly;

	// Read in the speed.
	lSpeed = NETWORK_ReadLong( pByteStream );

	// Read in the polyobject ID.
	lPolyNum = NETWORK_ReadShort( pByteStream );

	// Make sure the polyobj exists before we try to work with it.
	pPoly = PO_GetPolyobj( lPolyNum );
	if ( pPoly == NULL )
	{
		CLIENT_PrintWarning( "client_DoRotatePoly: Invalid polyobj number: %ld\n", lPolyNum );
		return;
	}

	// Create the polyobject.
	pRotatePoly = new DRotatePoly( lPolyNum );
	pRotatePoly->SetSpeed( lSpeed );

	// Attach the new polyobject to this ID.
	pPoly->specialdata = pRotatePoly;

	// Also, start the sound sequence associated with this polyobject.
	SN_StartSequence( pPoly, pPoly->seqType, SEQ_DOOR, 0 );
}

//*****************************************************************************
//
static void client_DestroyRotatePoly( BYTESTREAM_s *pByteStream )
{
	LONG							lID;
	DRotatePoly						*pPoly;
	DRotatePoly						*pTempPoly;
	TThinkerIterator<DRotatePoly>	Iterator;

	// Read in the DRotatePoly ID.
	lID = NETWORK_ReadShort( pByteStream );

	// Try to find the object from the ID. If it exists, destroy it.
	pPoly = NULL;
	while ( (pTempPoly = Iterator.Next( )) )
	{
		if ( pTempPoly->GetPolyObj( ) == lID )
		{
			pPoly = pTempPoly;
			break;
		}
	}

	if ( pPoly )
		pPoly->Destroy( );
}

//*****************************************************************************
//
static void client_DoMovePoly( BYTESTREAM_s *pByteStream )
{
	LONG			lXSpeed;
	LONG			lYSpeed;
	LONG			lPolyNum;
	FPolyObj		*pPoly;
	DMovePoly		*pMovePoly;

	// Read in the speed.
	lXSpeed = NETWORK_ReadLong( pByteStream );
	lYSpeed = NETWORK_ReadLong( pByteStream );

	// Read in the polyobject ID.
	lPolyNum = NETWORK_ReadShort( pByteStream );

	// Make sure the polyobj exists before we try to work with it.
	pPoly = PO_GetPolyobj( lPolyNum );
	if ( pPoly == NULL )
	{
		CLIENT_PrintWarning( "client_DoRotatePoly: Invalid polyobj number: %ld\n", lPolyNum );
		return;
	}

	// Create the polyobject.
	pMovePoly = new DMovePoly( lPolyNum );
	pMovePoly->SetXSpeed( lXSpeed );
	pMovePoly->SetYSpeed( lYSpeed );

	// Attach the new polyobject to this ID.
	pPoly->specialdata = pMovePoly;

	// Also, start the sound sequence associated with this polyobject.
	SN_StartSequence( pPoly, pPoly->seqType, SEQ_DOOR, 0 );
}

//*****************************************************************************
//
static void client_DestroyMovePoly( BYTESTREAM_s *pByteStream )
{
	LONG							lID;
	DMovePoly						*pPoly;
	DMovePoly						*pTempPoly;
	TThinkerIterator<DMovePoly>		Iterator;

	// Read in the DMovePoly ID.
	lID = NETWORK_ReadShort( pByteStream );

	// Try to find the object from the ID. If it exists, destroy it.
	pPoly = NULL;
	while ( (pTempPoly = Iterator.Next( )) )
	{
		if ( pTempPoly->GetPolyObj( ) == lID )
		{
			pPoly = pTempPoly;
			break;
		}
	}

	if ( pPoly )
		pPoly->Destroy( );
}

//*****************************************************************************
//
static void client_DoPolyDoor( BYTESTREAM_s *pByteStream )
{
	LONG			lType;
	LONG			lXSpeed;
	LONG			lYSpeed;
	LONG			lSpeed;
	LONG			lPolyNum;
	FPolyObj		*pPoly;
	DPolyDoor		*pPolyDoor;

	// Read in the type of poly door (swing or slide).
	lType = NETWORK_ReadByte( pByteStream );

	// Read in the speed.
	lXSpeed = NETWORK_ReadLong( pByteStream );
	lYSpeed = NETWORK_ReadLong( pByteStream );
	lSpeed = NETWORK_ReadLong( pByteStream );

	// Read in the polyobject ID.
	lPolyNum = NETWORK_ReadShort( pByteStream );

	// Make sure the polyobj exists before we try to work with it.
	pPoly = PO_GetPolyobj( lPolyNum );
	if ( pPoly == NULL )
	{
		CLIENT_PrintWarning( "client_DoPolyDoor: Invalid polyobj number: %ld\n", lPolyNum );
		return;
	}

	// Create the polyobject.
	pPolyDoor = new DPolyDoor( lPolyNum, (podoortype_t)lType );
	pPolyDoor->SetXSpeed( lXSpeed );
	pPolyDoor->SetYSpeed( lYSpeed );
	pPolyDoor->SetSpeed( lSpeed );

	// Attach the new polyobject to this ID.
	pPoly->specialdata = pPolyDoor;
}

//*****************************************************************************
//
static void client_DestroyPolyDoor( BYTESTREAM_s *pByteStream )
{
	LONG							lID;
	DPolyDoor						*pPoly;
	DPolyDoor						*pTempPoly;
	TThinkerIterator<DPolyDoor>		Iterator;

	// Read in the DPolyDoor ID.
	lID = NETWORK_ReadShort( pByteStream );

	// Try to find the object from the ID. If it exists, destroy it.
	pPoly = NULL;
	while ( (pTempPoly = Iterator.Next( )) )
	{
		if ( pTempPoly->GetPolyObj( ) == lID )
		{
			pPoly = pTempPoly;
			break;
		}
	}

	if ( pPoly )
		pPoly->Destroy( );
}

//*****************************************************************************
//
static void client_SetPolyDoorSpeedPosition( BYTESTREAM_s *pByteStream )
{
	LONG			lPolyID;
	LONG			lXSpeed;
	LONG			lYSpeed;
	LONG			lX;
	LONG			lY;
	FPolyObj		*pPoly;
	LONG			lDeltaX;
	LONG			lDeltaY;

	// Read in the polyobject ID.
	lPolyID = NETWORK_ReadShort( pByteStream );

	// Read in the polyobject x/yspeed.
	lXSpeed = NETWORK_ReadLong( pByteStream );
	lYSpeed = NETWORK_ReadLong( pByteStream );

	// Read in the polyobject X/.
	lX = NETWORK_ReadLong( pByteStream );
	lY = NETWORK_ReadLong( pByteStream );

	pPoly = PO_GetPolyobj( lPolyID );
	if ( pPoly == NULL )
		return;

	lDeltaX = lX - pPoly->StartSpot.x;
	lDeltaY = lY - pPoly->StartSpot.y;

	pPoly->MovePolyobj( lDeltaX, lDeltaY );
	
	if ( pPoly->specialdata == NULL )
		return;

	static_cast<DPolyDoor *>( pPoly->specialdata )->SetXSpeed( lXSpeed );
	static_cast<DPolyDoor *>( pPoly->specialdata )->SetYSpeed( lYSpeed );
}

//*****************************************************************************
//
static void client_SetPolyDoorSpeedRotation( BYTESTREAM_s *pByteStream )
{
	LONG			lPolyID;
	LONG			lSpeed;
	LONG			lAngle;
	FPolyObj		*pPoly;
	LONG			lDeltaAngle;

	// Read in the polyobject ID.
	lPolyID = NETWORK_ReadShort( pByteStream );

	// Read in the polyobject speed.
	lSpeed = NETWORK_ReadLong( pByteStream );

	// Read in the polyobject angle.
	lAngle = NETWORK_ReadLong( pByteStream );

	pPoly = PO_GetPolyobj( lPolyID );
	if ( pPoly == NULL )
		return;

	lDeltaAngle = lAngle - pPoly->angle;

	pPoly->RotatePolyobj( lDeltaAngle );

	if ( pPoly->specialdata == NULL )
		return;

	static_cast<DPolyDoor *>( pPoly->specialdata )->SetSpeed( lSpeed );
}

//*****************************************************************************
//
static void client_PlayPolyobjSound( BYTESTREAM_s *pByteStream )
{
	LONG		lID;
	bool		PolyMode;
	FPolyObj	*pPoly;

	// Read in the polyobject ID.
	lID = NETWORK_ReadShort( pByteStream );

	// Read in the polyobject mode.
	PolyMode = !!NETWORK_ReadByte( pByteStream );

	pPoly = PO_GetPolyobj( lID );
	if ( pPoly == NULL )
		return;

	SN_StartSequence( pPoly, pPoly->seqType, SEQ_DOOR, PolyMode );
}

//*****************************************************************************
//
static void client_StopPolyobjSound( BYTESTREAM_s *pByteStream )
{
	LONG		lID;
	FPolyObj	*pPoly;

	// Read in the polyobject ID.
	lID = NETWORK_ReadShort( pByteStream );

	pPoly = PO_GetPolyobj( lID );
	if ( pPoly == NULL )
		return;

	SN_StopSequence( pPoly );
}

//*****************************************************************************
//
static void client_SetPolyobjPosition( BYTESTREAM_s *pByteStream )
{
	LONG			lPolyNum;
	FPolyObj		*pPoly;
	LONG			lX;
	LONG			lY;
	LONG			lDeltaX;
	LONG			lDeltaY;

	// Read in the polyobject number.
	lPolyNum = NETWORK_ReadShort( pByteStream );

	// Read in the XY position of the polyobj.
	lX = NETWORK_ReadLong( pByteStream );
	lY = NETWORK_ReadLong( pByteStream );

	// Get the polyobject from the index given.
	pPoly = PO_GetPolyobj( lPolyNum );
	if ( pPoly == NULL )
	{
		CLIENT_PrintWarning( "client_SetPolyobjPosition: Invalid polyobj number: %ld\n", lPolyNum );
		return;
	}

	lDeltaX = lX - pPoly->StartSpot.x;
	lDeltaY = lY - pPoly->StartSpot.y;

//	Printf( "DeltaX: %d\nDeltaY: %d\n", lDeltaX, lDeltaY );

	// Finally, set the polyobject action.
	pPoly->MovePolyobj( lDeltaX, lDeltaY );
}

//*****************************************************************************
//
static void client_SetPolyobjRotation( BYTESTREAM_s *pByteStream )
{
	LONG			lPolyNum;
	FPolyObj		*pPoly;
	LONG			lAngle;
	LONG			lDeltaAngle;

	// Read in the polyobject number.
	lPolyNum = NETWORK_ReadShort( pByteStream );

	// Read in the angle of the polyobj.
	lAngle = NETWORK_ReadLong( pByteStream );

	// Make sure the polyobj exists before we try to work with it.
	pPoly = PO_GetPolyobj( lPolyNum );
	if ( pPoly == NULL )
	{
		CLIENT_PrintWarning( "client_SetPolyobjRotation: Invalid polyobj number: %ld\n", lPolyNum );
		return;
	}

	lDeltaAngle = lAngle - pPoly->angle;

	// Finally, set the polyobject action.
	pPoly->RotatePolyobj( lDeltaAngle );
}

//*****************************************************************************
//
static void client_EarthQuake( BYTESTREAM_s *pByteStream )
{
	AActor	*pCenter;
	LONG	lID;
	LONG	lIntensity;
	LONG	lDuration;
	LONG	lTremorRadius;

	// Read in the center's network ID.
	lID = NETWORK_ReadShort( pByteStream );

	// Read in the intensity of the quake.
	lIntensity = NETWORK_ReadByte( pByteStream );

	// Read in the duration of the quake.
	lDuration = NETWORK_ReadShort( pByteStream );

	// Read in the tremor radius of the quake.
	lTremorRadius = NETWORK_ReadShort( pByteStream );

	// [BB] Read in the quake sound.
	FSoundID quakesound = NETWORK_ReadString( pByteStream );

	// Find the actor that represents the center of the quake based on the network
	// ID sent. If we can't find the actor, then the quake has no center.
	pCenter = CLIENT_FindThingByNetID( lID );
	if ( pCenter == NULL )
		return;

	// Create the earthquake. Since this is client-side, damage is always 0.
	new DEarthquake( pCenter, lIntensity, lDuration, 0, lTremorRadius, quakesound );
}

//*****************************************************************************
//
static void client_DoScroller( BYTESTREAM_s *pByteStream )
{
	DScroller::EScrollType	Type;
	fixed_t					dX;
	fixed_t					dY;
	LONG					lAffectee;

	// Read in the type of scroller.
	Type = (DScroller::EScrollType)NETWORK_ReadByte( pByteStream );

	// Read in the X speed.
	dX = NETWORK_ReadLong( pByteStream );

	// Read in the Y speed.
	dY = NETWORK_ReadLong( pByteStream );

	// Read in the sector/side being scrolled.
	lAffectee = NETWORK_ReadLong( pByteStream );

	// Check to make sure what we've read in is valid.
	// [BB] sc_side is allowed, too, but we need to make a different check for it.
	if (( Type != DScroller::sc_floor ) && ( Type != DScroller::sc_ceiling ) &&
		( Type != DScroller::sc_carry ) && ( Type != DScroller::sc_carry_ceiling ) && ( Type != DScroller::sc_side ) )
	{
		CLIENT_PrintWarning( "client_DoScroller: Unknown type: %d!\n", static_cast<int> (Type) );
		return;
	}

	if( Type == DScroller::sc_side )
	{
		if (( lAffectee < 0 ) || ( lAffectee >= numsides ))
		{
			CLIENT_PrintWarning( "client_DoScroller: Invalid side ID: %ld!\n", lAffectee );
			return;
		}
	}
	else if (( lAffectee < 0 ) || ( lAffectee >= numsectors ))
	{
		CLIENT_PrintWarning( "client_DoScroller: Invalid sector ID: %ld!\n", lAffectee );
		return;
	}

	// Finally, create the scroller.
	new DScroller( Type, dX, dY, -1, lAffectee, 0 );
}

//*****************************************************************************
//
// [BB] SetScroller is defined in p_lnspec.cpp.
void SetScroller (int tag, DScroller::EScrollType type, fixed_t dx, fixed_t dy);

static void client_SetScroller( BYTESTREAM_s *pByteStream )
{
	DScroller::EScrollType	Type;
	fixed_t					dX;
	fixed_t					dY;
	LONG					lTag;

	// Read in the type of scroller.
	Type = (DScroller::EScrollType)NETWORK_ReadByte( pByteStream );

	// Read in the X speed.
	dX = NETWORK_ReadLong( pByteStream );

	// Read in the Y speed.
	dY = NETWORK_ReadLong( pByteStream );

	// Read in the sector being scrolled.
	lTag = NETWORK_ReadShort( pByteStream );

	// Check to make sure what we've read in is valid.
	if (( Type != DScroller::sc_floor ) && ( Type != DScroller::sc_ceiling ) &&
		( Type != DScroller::sc_carry ) && ( Type != DScroller::sc_carry_ceiling ))
	{
		CLIENT_PrintWarning( "client_SetScroller: Unknown type: %d!\n", static_cast<int> (Type) );
		return;
	}

	// Finally, create or update the scroller.
	SetScroller (lTag, Type, dX, dY );
}

//*****************************************************************************
//
// [BB] SetWallScroller is defined in p_lnspec.cpp.
void SetWallScroller(int id, int sidechoice, fixed_t dx, fixed_t dy, int Where);

static void client_SetWallScroller( BYTESTREAM_s *pByteStream )
{
	LONG					lId;
	LONG					lSidechoice;
	fixed_t					dX;
	fixed_t					dY;
	LONG					lWhere;

	// Read in the id.
	lId = NETWORK_ReadLong( pByteStream );

	// Read in the side choice.
	lSidechoice = NETWORK_ReadByte( pByteStream );

	// Read in the X speed.
	dX = NETWORK_ReadLong( pByteStream );

	// Read in the Y speed.
	dY = NETWORK_ReadLong( pByteStream );

	// Read in where.
	lWhere = NETWORK_ReadLong( pByteStream );

	// Finally, create or update the scroller.
	SetWallScroller (lId, lSidechoice, dX, dY, lWhere );
}

//*****************************************************************************
//
static void client_DoFlashFader( BYTESTREAM_s *pByteStream )
{
	float	fR1;
	float	fG1;
	float	fB1;
	float	fA1;
	float	fR2;
	float	fG2;
	float	fB2;
	float	fA2;
	float	fTime;
	ULONG	ulPlayer;

	// Read in the colors, time for the flash fader and which player to apply the effect to.
	fR1 = NETWORK_ReadFloat( pByteStream );
	fG1 = NETWORK_ReadFloat( pByteStream );
	fB1 = NETWORK_ReadFloat( pByteStream );
	fA1 = NETWORK_ReadFloat( pByteStream );

	fR2 = NETWORK_ReadFloat( pByteStream );
	fG2 = NETWORK_ReadFloat( pByteStream );
	fB2 = NETWORK_ReadFloat( pByteStream );
	fA2 = NETWORK_ReadFloat( pByteStream );

	fTime = NETWORK_ReadFloat( pByteStream );

	ulPlayer = NETWORK_ReadByte( pByteStream );

	// [BB] Sanity check.
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return;

	// Create the flash fader.
	if ( players[ulPlayer].mo )
		new DFlashFader( fR1, fG1, fB1, fA1, fR2, fG2, fB2, fA2, fTime, players[ulPlayer].mo );
}

//*****************************************************************************
//
static void client_GenericCheat( BYTESTREAM_s *pByteStream )
{
	ULONG	ulPlayer;
	ULONG	ulCheat;

	// Read in the player who's doing the cheat.
	ulPlayer = NETWORK_ReadByte( pByteStream );

	// Read in the cheat.
	ulCheat = NETWORK_ReadByte( pByteStream );

	if ( playeringame[ulPlayer] == false )
		return;

	// Finally, do the cheat.
	cht_DoCheat( &players[ulPlayer], ulCheat );
}

//*****************************************************************************
//
static void client_SetCameraToTexture( BYTESTREAM_s *pByteStream )
{
	LONG		lID;
	const char	*pszTexture;
	LONG		lFOV;
	AActor		*pCamera;
	FTextureID	picNum;

	// Read in the ID of the camera.
	lID = NETWORK_ReadShort( pByteStream );

	// Read in the name of the texture.
	pszTexture = NETWORK_ReadString( pByteStream );

	// Read in the FOV of the camera.
	lFOV = NETWORK_ReadByte( pByteStream );

	// Find the actor that represents the camera. If we can't find the actor, then
	// break out.
	pCamera = CLIENT_FindThingByNetID( lID );
	if ( pCamera == NULL )
		return;

	picNum = TexMan.CheckForTexture( pszTexture, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable );
	if ( !picNum.Exists() )
	{
		CLIENT_PrintWarning( "client_SetCameraToTexture: %s is not a texture\n", pszTexture );
		return;
	}

	FCanvasTextureInfo::Add( pCamera, picNum, lFOV );
}

//*****************************************************************************
//
static void client_CreateTranslation( BYTESTREAM_s *pByteStream, bool bIsTypeTwo )
{
	EDITEDTRANSLATION_s	Translation;
	FRemapTable	*pTranslation;

	// Read in which translation is being created.
	Translation.ulIdx = NETWORK_ReadShort( pByteStream );

	const bool bIsEdited = !!NETWORK_ReadByte( pByteStream );

	// Read in the range that's being translated.
	Translation.ulStart = NETWORK_ReadByte( pByteStream );
	Translation.ulEnd = NETWORK_ReadByte( pByteStream );

	if ( bIsTypeTwo == false )
	{
		Translation.ulPal1 = NETWORK_ReadByte( pByteStream );
		Translation.ulPal2 = NETWORK_ReadByte( pByteStream );
		Translation.ulType = DLevelScript::PCD_TRANSLATIONRANGE1;
	}
	else
	{
		Translation.ulR1 = NETWORK_ReadByte( pByteStream );
		Translation.ulG1 = NETWORK_ReadByte( pByteStream );
		Translation.ulB1 = NETWORK_ReadByte( pByteStream );
		Translation.ulR2 = NETWORK_ReadByte( pByteStream );
		Translation.ulG2 = NETWORK_ReadByte( pByteStream );
		Translation.ulB2 = NETWORK_ReadByte( pByteStream );
		Translation.ulType = DLevelScript::PCD_TRANSLATIONRANGE2;
	}

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
static void client_IgnorePlayer( BYTESTREAM_s *pByteStream )
{
	ULONG	ulPlayer = NETWORK_ReadByte( pByteStream );
	LONG	lTicks = NETWORK_ReadLong( pByteStream );

	if ( ulPlayer < MAXPLAYERS )
	{
		players[ulPlayer].bIgnoreChat = true;
		players[ulPlayer].lIgnoreChatTicks = lTicks;

		Printf( "%s\\c- will be ignored, because you're ignoring %s IP.\n", players[ulPlayer].userinfo.GetName(), players[ulPlayer].userinfo.GetGender() == GENDER_MALE ? "his" : players[ulPlayer].userinfo.GetGender() == GENDER_FEMALE ? "her" : "its" );
	}
}

//*****************************************************************************
//
static void client_DoPusher( BYTESTREAM_s *pByteStream )
{
	const ULONG ulType = NETWORK_ReadByte( pByteStream );
	const int iLineNum = NETWORK_ReadShort( pByteStream );
	const int iMagnitude = NETWORK_ReadLong( pByteStream );
	const int iAngle = NETWORK_ReadLong( pByteStream );
	const LONG lSourceNetID = NETWORK_ReadShort( pByteStream );
	const int iAffectee = NETWORK_ReadShort( pByteStream );

	line_t *pLine = ( iLineNum >= 0 && iLineNum < numlines ) ? &lines[iLineNum] : NULL;
	new DPusher ( static_cast<DPusher::EPusher> ( ulType ), pLine, iMagnitude, iAngle, CLIENT_FindThingByNetID( lSourceNetID ), iAffectee );
}

//*****************************************************************************
//
void AdjustPusher (int tag, int magnitude, int angle, DPusher::EPusher type);
static void client_AdjustPusher( BYTESTREAM_s *pByteStream )
{
	const int iTag = NETWORK_ReadShort( pByteStream );
	const int iMagnitude = NETWORK_ReadLong( pByteStream );
	const int iAngle = NETWORK_ReadLong( pByteStream );
	const ULONG ulType = NETWORK_ReadByte( pByteStream );
	AdjustPusher (iTag, iMagnitude, iAngle, static_cast<DPusher::EPusher> ( ulType ));
}

//*****************************************************************************
//
void ServerCommands::ReplaceTextures::Execute()
{
	DLevelScript::ReplaceTextures( fromTexture, toTexture, textureFlags );
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

CVAR( Bool, cl_predict_players, true, CVAR_ARCHIVE )
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
