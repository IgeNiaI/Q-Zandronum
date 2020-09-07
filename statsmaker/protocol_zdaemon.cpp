//-----------------------------------------------------------------------------
//
// Skulltag Statsmaker Source
// Copyright (C) 2007 Brad Carney
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
// Date created:  11/17/07
//
//
// Filename: protocol_zdaemon.cpp
//
// Description: Contains the protocol for communication with ZDaemon servers.
//
//-----------------------------------------------------------------------------

#include "i_system.h"
#include "../src/networkheaders.h"
#include "../src/networkshared.h"
#include "main.h"
#include "protocol_zdaemon.h"

//*****************************************************************************
//	VARIABLES

// Address of master server.
static	NETADDRESS_s		g_AddressMasterServer;

// Message buffer for sending messages.
static	NETBUFFER_s			g_MessageBuffer;

//*****************************************************************************
//	FUNCTIONS

void ZDAEMON_Construct( void )
{
	// Setup our message buffer.
	NETWORK_InitBuffer( &g_MessageBuffer, 8192, BUFFERTYPE_WRITE );
	NETWORK_ClearBuffer( &g_MessageBuffer );

	NETWORK_StringToAddress( "zdaemon.ath.cx", &g_AddressMasterServer );
	g_AddressMasterServer.usPort = htons( 15300 );

	// Call ZDAEMON_Destruct when the program terminates.
	atexit( ZDAEMON_Destruct );
}

//*****************************************************************************
//
void ZDAEMON_Destruct( void )
{
	// Free our local buffer.
	NETWORK_FreeBuffer( &g_MessageBuffer );
}

//*****************************************************************************
//
void ZDAEMON_QueryMasterServer( void )
{
	// Clear out the buffer, and write out launcher challenge.
	NETWORK_ClearBuffer( &g_MessageBuffer );
	NETWORK_WriteLong( &g_MessageBuffer.ByteStream, ZDAEMON_MASTER_REQUESTLIST );

	// Send the master server our packet.
	NETWORK_LaunchPacket( &g_MessageBuffer, g_AddressMasterServer, false );
}

//*****************************************************************************
//
bool ZDAEMON_ParseMasterServerResponse( BYTESTREAM_s *pByteStream, TArray<SERVER_s>&aServerInfo, TArray<QUERY_s>&aQueryInfo )
{
	LONG			lIdx;
	LONG			lCommand;
	//SERVER_s	ServerInfo;
	QUERY_s		QueryInfo;
	LONG			lNumServers;

	lCommand = NETWORK_ReadLong( pByteStream );
	switch ( lCommand )
	{
	case ZDAEMON_MASTER_PLAYERLIST:

		// Add a new query.
		QueryInfo.qTotal.lNumPlayers = 0;
		QueryInfo.qTotal.lNumServers = 0;
		aQueryInfo.Push( QueryInfo );

		// Clear out the server list.
		aServerInfo.Clear( );

		// Get the list of servers.
		Printf( "Receiving ZDaemon server list...\n" );
		lNumServers = NETWORK_ReadShort( pByteStream );
		for ( lIdx = 0; lIdx < lNumServers; lIdx++ )
		{
			const char *pszString;

			pszString = NETWORK_ReadString( pByteStream );
		}
/*
		while ( NETWORK_ReadByte( pByteStream ) != MSC_ENDSERVERLIST )
		{
			ServerInfo.ulActiveState = AS_WAITINGFORREPLY;

			// Read in address information.
			ServerInfo.Address.abIP[0] = NETWORK_ReadByte( pByteStream );
			ServerInfo.Address.abIP[1] = NETWORK_ReadByte( pByteStream );
			ServerInfo.Address.abIP[2] = NETWORK_ReadByte( pByteStream );
			ServerInfo.Address.abIP[3] = NETWORK_ReadByte( pByteStream );
			ServerInfo.Address.usPort = htons( NETWORK_ReadShort( pByteStream ));

			// Add this server's info to our list, and query it.
			i = aServerInfo.Push( ServerInfo );
			SKULLTAG_QueryServer( &aServerInfo[i] );
		}
*/
		// Since we got the server list, return true.
		return true;
	}

	return false;
}

//*****************************************************************************
//
void ZDAEMON_QueryServer( SERVER_s *pServer )
{
	return;
	// Clear out the buffer, and write out launcher challenge.
	NETWORK_ClearBuffer( &g_MessageBuffer );
	NETWORK_WriteLong( &g_MessageBuffer.ByteStream, LAUNCHER_SERVER_CHALLENGE );
	NETWORK_WriteLong( &g_MessageBuffer.ByteStream, SQF_NUMPLAYERS|SQF_PLAYERDATA );
	NETWORK_WriteLong( &g_MessageBuffer.ByteStream, 0 );

	// Send the server our packet.
	NETWORK_LaunchPacket( &g_MessageBuffer, pServer->Address, false );
}

//*****************************************************************************
//
bool ZDAEMON_ParseServerResponse( BYTESTREAM_s *pByteStream, SERVER_s *pServer, TArray<QUERY_s>&aQueryInfo )
{
	//LONG		lCommand;
	LONG		lGameType = GAMETYPE_COOPERATIVE;
	LONG		lNumPWADs;
	LONG		lNumPlayers;
	LONG		lNumRealPlayers;
	ULONG		i;
	//ULONG		ulBits;

	// Make sure this is a server we're actually waiting for a reply from.
	if ( pServer->ulActiveState != AS_WAITINGFORREPLY )
	{
		while ( NETWORK_ReadByte( pByteStream ) != -1 )
			;

		return false;
	}

	// This server is now active.
	pServer->ulActiveState = AS_ACTIVE;

	// Read the server name.
	NETWORK_ReadString( pByteStream );

	// Read the maximum number of clients.
	NETWORK_ReadByte( pByteStream );

	// Maximum slots.
	NETWORK_ReadByte( pByteStream );

	// Read the map name.
	NETWORK_ReadString( pByteStream );

	// Read in the PWAD information.
	lNumPWADs = NETWORK_ReadByte( pByteStream );
	for ( i = 0; i < (ULONG)lNumPWADs; i++ )
		NETWORK_ReadString( pByteStream );

	// Read the game type.
	lGameType = NETWORK_ReadByte( pByteStream );

	// Game name.
	NETWORK_ReadString( pByteStream );

	// Read in the IWAD name.
	NETWORK_ReadString( pByteStream );

	// Game skill.
	NETWORK_ReadByte( pByteStream );

	// Read the website URL.
	NETWORK_ReadString( pByteStream );

	// Read the host's e-mail address.
	NETWORK_ReadString( pByteStream );

	// DMFlags and DMFlags2.
	NETWORK_ReadLong( pByteStream );
	NETWORK_ReadLong( pByteStream );

	// Read in the number of players.
	lNumPlayers = NETWORK_ReadByte( pByteStream );
	lNumRealPlayers = lNumPlayers;

	for ( i = 0; i < (ULONG)lNumPlayers; i++ )
	{
		// Read in this player's name.
		NETWORK_ReadString( pByteStream );

		// Read in "fragcount" (could be frags, points, etc.)
		NETWORK_ReadShort( pByteStream );

		// Read in the player's ping.
		NETWORK_ReadShort( pByteStream );

		// Read in the player's level.
		NETWORK_ReadByte( pByteStream );

		// Time.
		NETWORK_ReadShort( pByteStream );

		// Read in whether or not the player is a bot.
		if ( NETWORK_ReadByte( pByteStream ))
			lNumRealPlayers--;
	}

	// And we don't care about anything else.

	aQueryInfo[aQueryInfo.Size( ) - 1].qTotal.lNumPlayers += lNumRealPlayers;
	aQueryInfo[aQueryInfo.Size( ) - 1].qTotal.lNumServers++;

	// Success!
	return true;
}
