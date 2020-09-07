//------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2008 Rivecoder
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
// Date created: 8/17/08
//
//
// Filename: sv_rcon.cpp
//
// Description: Server-side management of the RCON utility sessions. (This doesn't cover players using the 'RCON' command, yet)
//
//----------------------------------------------------------------------------------------------------------------------------------------------------

#include "sv_rcon.h"
// [BB] Do not include these system headers earlier to prevent header inclusion conflicts.
#include <list>
#include <time.h>
#include "sv_ban.h"
#include "c_console.h"
#include "doomstat.h"
#include "network.h"
#include "g_level.h"
#include "i_system.h"
#include "m_random.h"
#include "version.h"
#include "v_text.h"

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- VARIABLES -------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

// Message buffer for sending messages. 
static	NETBUFFER_s						g_MessageBuffer;

// Clients who are trying to connect. Once they send a proper password they are promoted to the authed clients list.
static	TArray<RCONCANDIDATE_s>			g_Candidates;

// Authenticated clients who can execute commands.
static	TArray<RCONCLIENT_s>			g_AuthedClients;

// The last 32 lines that were printed in the console; sent to clients when they connect. (The server doesn't use the c_console buffer.)
static	std::list<FString>				g_RecentConsoleLines;

// IPs that we're ignoring (bad passwords, old protocol versions) to prevent flooding.
static	QueryIPQueue					g_BadRequestFloodQueue( BAD_QUERY_IGNORE_TIME );

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- PROTOTYPES ------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

static	void							server_WriteUpdateInfo( BYTESTREAM_s *pByteStream, int iUpdateType );
static	void							server_rcon_HandleNewConnection( NETADDRESS_s Address, int iProtocolVersion );
static	void							server_rcon_HandleLogin( int iCandidateIndex, const char *pszHash );
static	void							server_rcon_CreateSalt( char *pszBuffer );
static	LONG							server_rcon_FindClient( NETADDRESS_s Address );
static	LONG							server_rcon_FindCandidate( NETADDRESS_s Address );

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

//*****************************************************************************
//
void SERVER_RCON_Construct( )
{
	g_MessageBuffer.Init( MAX_UDP_PACKET, BUFFERTYPE_WRITE );
	g_MessageBuffer.Clear();
	
	// Call SERVER_RCON_Destruct() when Skulltag closes.
	atterm( SERVER_RCON_Destruct );
}

//*****************************************************************************
//
void SERVER_RCON_Destruct( )
{
	// Free our local buffer.
	g_MessageBuffer.Free();
}

//*****************************************************************************
//
void SERVER_RCON_Tick( )
{
	// Remove timed-out candidates.
	for ( unsigned int i = 0; i < g_Candidates.Size( ); )
	{
		if (( gametic - g_Candidates[i].iLastMessageTic ) >= ( RCON_CANDIDATE_TIMEOUT_TIME * TICRATE ))
			g_Candidates.Delete( i );
		else
			i++;
	}

	// Remove timed-out clients.
	for ( unsigned int i = 0; i < g_AuthedClients.Size( ); )
	{
		if (( gametic - g_AuthedClients[i].iLastMessageTic ) >= ( RCON_CLIENT_TIMEOUT_TIME * TICRATE ))
		{
			Printf( "RCON client at %s timed out.\n", g_AuthedClients[i].Address.ToString() );
			g_AuthedClients.Delete( i );
			SERVER_RCON_UpdateInfo( SVRCU_ADMINCOUNT );
		}
		else
			i++;
	}

	g_BadRequestFloodQueue.adjustHead( gametic / 1000 );
}

//==========================================================================
//
// SERVER_RCON_ParseMessage
//
// Parses a CLRC_* packet. 
//
//==========================================================================

void SERVER_RCON_ParseMessage( NETADDRESS_s Address, LONG lMessage, BYTESTREAM_s *pByteStream )
{
	int iIndex = -1;

	switch ( lMessage )
	{
	case CLRC_BEGINCONNECTION:

		server_rcon_HandleNewConnection( Address, NETWORK_ReadByte( pByteStream ));
		break;
	case CLRC_PASSWORD:

		server_rcon_HandleLogin( server_rcon_FindCandidate( Address ), NETWORK_ReadString( pByteStream ));
		break;	
	case CLRC_PONG:

		iIndex = server_rcon_FindClient( Address );
		if ( iIndex != -1 )	
			g_AuthedClients[iIndex].iLastMessageTic = gametic;
		break;
	case CLRC_COMMAND:

		// Execute the command (if this came from an admin).
		iIndex = server_rcon_FindClient( Address );
		if ( iIndex != -1 )
		{
			const char *szCommand = NETWORK_ReadString( pByteStream );
			// [BB] Log the command before adding it. If we don't have a server GUI, the command
			// is executed immediately and may cause Skulltag to exit before the command is logged.
			Printf( "-> %s (RCON by %s)\n", szCommand, Address.ToString() );
			SERVER_AddCommand( szCommand );
			g_AuthedClients[iIndex].iLastMessageTic = gametic;
		}
		break;
	case CLRC_DISCONNECT:
		
		iIndex = server_rcon_FindClient( Address );
		if ( iIndex != -1 )	
		{
			g_AuthedClients.Delete( iIndex );
			SERVER_RCON_UpdateInfo( SVRCU_ADMINCOUNT );
			Printf( "RCON client at %s disconnected.\n", Address.ToString() );
		}
		break;
	case CLRC_TABCOMPLETE:

		// [TP] RCON client wishes to tab-complete
		iIndex = server_rcon_FindClient( Address );
		if ( iIndex != -1 )
		{
			const char* part = NETWORK_ReadString( pByteStream );
			TArray<FString> list = C_GetTabCompletes( part );
			g_MessageBuffer.Clear();

			// [TP] Let's not send too many of these though
			if ( list.Size() < 50 )
			{
				NETWORK_WriteByte( &g_MessageBuffer.ByteStream, SVRC_TABCOMPLETE );
				NETWORK_WriteByte( &g_MessageBuffer.ByteStream, list.Size() );

				for ( unsigned i = 0; i < list.Size(); ++i )
					NETWORK_WriteString( &g_MessageBuffer.ByteStream, list[i] );
			}
			else
			{
				NETWORK_WriteByte( &g_MessageBuffer.ByteStream, SVRC_TOOMANYTABCOMPLETES );
				NETWORK_WriteShort( &g_MessageBuffer.ByteStream, list.Size() );
			}

			NETWORK_LaunchPacket( &g_MessageBuffer, g_AuthedClients[iIndex].Address );
		}
		break;
	}
}

//==========================================================================
//
// SERVER_RCON_Print
//
// Sends the message to all connected administrators.
//
//==========================================================================

void SERVER_RCON_Print( const char *pszString )
{
	for ( unsigned int i = 0; i < g_AuthedClients.Size( ); i++ )
	{
		g_MessageBuffer.Clear();
		NETWORK_WriteByte( &g_MessageBuffer.ByteStream, SVRC_MESSAGE );
		NETWORK_WriteString( &g_MessageBuffer.ByteStream, pszString );
		NETWORK_LaunchPacket( &g_MessageBuffer, g_AuthedClients[i].Address );
	}

	//==========================================
	// Add this to the cache of recent messages.
	//==========================================

	if ( g_RecentConsoleLines.size() >= 32 )
		g_RecentConsoleLines.pop_front();

	FString			fsLogged = pszString;
	time_t			tNow = time(0);
	struct	tm		*pTimeInfo = localtime( &tNow );

	if ( pTimeInfo->tm_hour < 12 )
		fsLogged.Format( "[%02d:%02d:%02d am] %s ", ( pTimeInfo->tm_hour == 0 ) ? 12 : pTimeInfo->tm_hour, pTimeInfo->tm_min, pTimeInfo->tm_sec, pszString );
	else
		fsLogged.Format( "[%02d:%02d:%02d pm] %s", ( pTimeInfo->tm_hour == 12 ) ? 12 : pTimeInfo->tm_hour % 12, pTimeInfo->tm_min, pTimeInfo->tm_sec, pszString );

	g_RecentConsoleLines.push_back( fsLogged );
}

//==========================================================================
//
// SERVER_RCON_UpdateInfo
//
// Updates the connected admins about the server's information (map, # players, # admins).
//
//==========================================================================

void SERVER_RCON_UpdateInfo( int iUpdateType )
{
	int		iNumPlayers = (int) SERVER_CountPlayers( true );

	for ( unsigned int i = 0; i < g_AuthedClients.Size( ); i++ )
	{	
		g_MessageBuffer.Clear();
		NETWORK_WriteByte( &g_MessageBuffer.ByteStream, SVRC_UPDATE );		
		server_WriteUpdateInfo( &g_MessageBuffer.ByteStream, iUpdateType );
		NETWORK_LaunchPacket( &g_MessageBuffer, g_AuthedClients[i].Address );
	}
}

//==========================================================================
//
// server_WriteUpdateInfo
//
// Writes the actual update data for SERVER_RCON_UpdateInfo.
//
//==========================================================================

static void server_WriteUpdateInfo( BYTESTREAM_s *pByteStream, int iUpdateType )
{
	NETWORK_WriteByte( &g_MessageBuffer.ByteStream, iUpdateType );

	switch ( iUpdateType )
	{
	// Update the player data.
	case SVRCU_PLAYERDATA:

		NETWORK_WriteByte( pByteStream, SERVER_CountPlayers( true ));
		for ( unsigned int i = 0; i < MAXPLAYERS; i++ )
		{
			if ( playeringame[i] )
			{
				FString		fsName;

				fsName.Format( "%s", players[i].userinfo.GetName() );
				V_RemoveColorCodes( fsName );
				NETWORK_WriteString( pByteStream, fsName );
			}
		}
		break;
	// Update the current map.
	case SVRCU_MAP:

		NETWORK_WriteString( pByteStream, level.mapname );
		break;
	// Update the number of other admins.
	case SVRCU_ADMINCOUNT:

		NETWORK_WriteByte( pByteStream, g_AuthedClients.Size() - 1 );
		break;
	}
}

//==========================================================================
//
// server_rcon_HandleNewConnection
//
// Creates a new slot and salt for the client, and requests his password.
//
//==========================================================================

static void server_rcon_HandleNewConnection( NETADDRESS_s Address,  int iProtocolVersion )
{
	// Banned client? Notify him, ignore him, and get out of here.
	if ( SERVERBAN_IsIPBanned( Address ))
	{
		NETWORK_WriteByte( &g_MessageBuffer.ByteStream, SVRC_BANNED );
		NETWORK_LaunchPacket( &g_MessageBuffer, Address );
		g_BadRequestFloodQueue.addAddress( Address, gametic / 1000 );
		return;
	}

	// Old protocol version? Notify, ignore, and quit.
	if ( iProtocolVersion < MIN_PROTOCOL_VERSION )
	{
		NETWORK_WriteByte( &g_MessageBuffer.ByteStream, SVRC_OLDPROTOCOL );
		NETWORK_WriteByte( &g_MessageBuffer.ByteStream, PROTOCOL_VERSION );
		NETWORK_WriteString( &g_MessageBuffer.ByteStream, DOTVERSIONSTR );
		NETWORK_LaunchPacket( &g_MessageBuffer, Address );
		g_BadRequestFloodQueue.addAddress( Address, gametic / 1000 );
		return;
	}

	// Check if there's already a user at this address. Remove him if so (must be reconnecting).
	// This ensures that each address never has more than one entry (since this is the only function that gives out new candidate slots).
	int iIndex = server_rcon_FindCandidate( Address );
	if ( iIndex != -1 )
		g_Candidates.Delete( iIndex );
	iIndex = server_rcon_FindClient( Address );
	if ( iIndex != -1 )
		g_AuthedClients.Delete( iIndex );

	// Create a slot for him, and request his password.
	RCONCANDIDATE_s		Candidate;
	Candidate.iLastMessageTic = gametic;
	Candidate.Address = Address;
	server_rcon_CreateSalt( Candidate.szSalt );
	g_Candidates.Push( Candidate );

	g_MessageBuffer.Clear();
	NETWORK_WriteByte( &g_MessageBuffer.ByteStream, SVRC_SALT );
	NETWORK_WriteString( &g_MessageBuffer.ByteStream, Candidate.szSalt );
	NETWORK_LaunchPacket( &g_MessageBuffer, Address );
}

//==========================================================================
//
// server_rcon_HandleLogin
//
// Checks the given password hash, and, if it's valid, promotes the candidate.
//
//==========================================================================

static void server_rcon_HandleLogin( int iCandidateIndex, const char *pszHash )
{
	// If there's no slot, the candidate must have timed out, or is hacking. Bye!
	if ( iCandidateIndex == -1 )
		return;

	// Combine the salt and password, and hash it.
	FString fsString, fsCorrectHash;
	fsString.Format( "%s%s", g_Candidates[iCandidateIndex].szSalt, sv_rconpassword.GetGenericRep(CVAR_String).String );	
	CMD5Checksum::GetMD5( reinterpret_cast<const BYTE *>(fsString.GetChars()), fsString.Len(), fsCorrectHash );

	// Compare that to what he sent us.
	// Printf("Mine: %s\nTheirs: %s\n", fsCorrectHash, pszHash );
	g_MessageBuffer.Clear();
	// [BB] Do not allow the server to let anybody use RCON in case sv_rconpassword is empty.
	if ( fsCorrectHash.Compare( pszHash ) || ( strlen( sv_rconpassword.GetGenericRep(CVAR_String).String ) == 0 ) )
	{
		// Wrong password.
		NETWORK_WriteByte( &g_MessageBuffer.ByteStream, SVRC_INVALIDPASSWORD );
		NETWORK_LaunchPacket( &g_MessageBuffer, g_Candidates[iCandidateIndex].Address ); // [RC] Note: Be sure to finish any packets before calling Printf(). Otherwise SERVER_RCON_Print will clear your buffer.

		// To prevent mass password flooding, ignore the IP for a few seconds.
		g_BadRequestFloodQueue.addAddress( g_Candidates[iCandidateIndex].Address, gametic / 1000 );

		Printf( "Failed RCON login from %s. Ignoring IP for 10 seconds...\n", g_Candidates[iCandidateIndex].Address.ToString() );
	}
	else
	{		
		// [BB] Since we log when RCON clients disconnect, we should also log when they connect.
		// Do this before we do anything else so that this message is sent to the new RCON client
		// with the console history.
		Printf( "RCON client at %s connected.\n", g_Candidates[iCandidateIndex].Address.ToString() );

		// Correct password. Promote him to an authed client.
		RCONCLIENT_s Client;
		Client.Address = g_Candidates[iCandidateIndex].Address;
		Client.iLastMessageTic = gametic;
		g_AuthedClients.Push( Client );

		g_MessageBuffer.Clear();
		NETWORK_WriteByte( &g_MessageBuffer.ByteStream, SVRC_LOGGEDIN );

		// Tell him some info about the server.
		NETWORK_WriteByte( &g_MessageBuffer.ByteStream, PROTOCOL_VERSION );
		NETWORK_WriteString( &g_MessageBuffer.ByteStream, sv_hostname.GetGenericRep( CVAR_String ).String );
		
		// Send updates.
		NETWORK_WriteByte( &g_MessageBuffer.ByteStream, NUM_RCON_UPDATES );
		for ( int i = 0; i < NUM_RCON_UPDATES; i++ )
			server_WriteUpdateInfo( &g_MessageBuffer.ByteStream, i );

		// Send the console history.
		NETWORK_WriteByte( &g_MessageBuffer.ByteStream, g_RecentConsoleLines.size() );
		for( std::list<FString>::iterator i = g_RecentConsoleLines.begin(); i != g_RecentConsoleLines.end(); ++i )
			NETWORK_WriteString( &g_MessageBuffer.ByteStream, *i );

		NETWORK_LaunchPacket( &g_MessageBuffer, g_Candidates[iCandidateIndex].Address );
		SERVER_RCON_UpdateInfo( SVRCU_ADMINCOUNT );
	}

	// Remove his temporary slot.	
	g_Candidates.Delete( iCandidateIndex );
}

//==========================================================================
//
// server_rcon_CreateSalt
//
// Creates a random, 32 digit salt.
//
//==========================================================================

static void server_rcon_CreateSalt( char *pszBuffer )
{
	FString fsSalt;
	const char szCharacters[] = ".0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz!@#$%^&*()_+/\\[]<>?~`|";

	// Generate 32 random characters from szCharacters. (would this be faster with a char array?)
	for ( int i = 0; i < 32; i++ )
		fsSalt.AppendFormat( "%c", szCharacters[( gametic + M_Random( ) + M_Random( ) + M_Random( )) % strlen( szCharacters )] );

	// Copy it to the buffer.
	strncpy( pszBuffer, fsSalt.GetChars( ), 32 );
	pszBuffer[32] = 0;
}

//==========================================================================
//
// server_rcon_FindCandidate
//
// Returns the index (or -1 if none) of the RCON candidate with the given address.
//
//==========================================================================

static LONG server_rcon_FindCandidate( NETADDRESS_s Address )
{
	for ( unsigned int i = 0; i < g_Candidates.Size( ); i++ )
		if ( g_Candidates[i].Address.Compare( Address ))
			return i;

	return -1;
}


//==========================================================================
//
// server_rcon_FindClient
//
// Returns the index (or -1 if none) of the RCON client with the given address.
//
//==========================================================================

static LONG server_rcon_FindClient( NETADDRESS_s Address )
{
	for ( unsigned int i = 0; i < g_AuthedClients.Size( ); i++ )
		if ( g_AuthedClients[i].Address.Compare( Address ))
			return i;

	return -1;
}
