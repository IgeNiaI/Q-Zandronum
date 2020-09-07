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
// Date created:  11/10/07
//
//
// Filename: protocol_zandronum.cpp
//
// Description: Contains the protocol for communication with Zandronum servers.
//
//-----------------------------------------------------------------------------

#include "i_system.h"
#include "../src/networkheaders.h"
#include "../src/networkshared.h"
#include <time.h>
#include "main.h"
#include "protocol_zandronum.h"

//*****************************************************************************
//	VARIABLES

// Address of master server.
static	NETADDRESS_s		g_AddressMasterServer;

// Message buffer for sending messages.
static	NETBUFFER_s			g_MessageBuffer;

//*****************************************************************************
//	FUNCTIONS

void ZANDRONUM_Construct( void )
{
	// Setup our message buffer.
	NETWORK_InitBuffer( &g_MessageBuffer, 8192, BUFFERTYPE_WRITE );
	NETWORK_ClearBuffer( &g_MessageBuffer );

	NETWORK_StringToAddress( "master.zandronum.com", &g_AddressMasterServer );
	g_AddressMasterServer.usPort = htons( DEFAULT_MASTER_PORT );

	// Call ZANDRONUM_Destruct when the program terminates.
	atexit( ZANDRONUM_Destruct );
}

//*****************************************************************************
//
void ZANDRONUM_Destruct( void )
{
	// Free our local buffer.
	NETWORK_FreeBuffer( &g_MessageBuffer );
}

//*****************************************************************************
//
NETADDRESS_s *ZANDRONUM_GetMasterServerAddress ( void )
{
	return &g_AddressMasterServer;
}

//*****************************************************************************
//
void ZANDRONUM_QueryMasterServer( void )
{
	// Clear out the buffer, and write out launcher challenge.
	NETWORK_ClearBuffer( &g_MessageBuffer );
	NETWORK_WriteLong( &g_MessageBuffer.ByteStream, LAUNCHER_MASTER_CHALLENGE );
	NETWORK_WriteShort( &g_MessageBuffer.ByteStream, MASTER_SERVER_VERSION );

	// Send the master server our packet.
	NETWORK_LaunchPacket( &g_MessageBuffer, g_AddressMasterServer, true );
}

//*****************************************************************************
//
bool ZANDRONUM_ParseMasterServerResponse( BYTESTREAM_s *pByteStream, TArray<SERVER_s>&aServerInfo, TArray<QUERY_s>&aQueryInfo )
{
	ULONG			i;
	LONG			lCommand;
	SERVER_s	ServerInfo;
	QUERY_s		QueryInfo;
	time_t		tNow;

	lCommand = NETWORK_ReadLong( pByteStream );
	switch ( lCommand )
	{
	case MSC_BEGINSERVERLISTPART:
		const ULONG ulPacketNum = NETWORK_ReadByte( pByteStream );

		if ( ulPacketNum == 0 )
		{
			// Add a new query.
			QueryInfo.qTotal.lNumPlayers = 0;
			QueryInfo.qTotal.lNumServers = 0;
			QueryInfo.qTotal.lNumSpectators = 0;

			for ( unsigned int g = 0; g < NUM_GAMETYPES; g++ )
			{	
				QueryInfo.qByGameMode[g].lNumPlayers = 0;
				QueryInfo.qByGameMode[g].lNumServers = 0;
				QueryInfo.qByGameMode[g].lNumSpectators = 0;
			}

			time( &QueryInfo.tTime );
			aQueryInfo.Push( QueryInfo );

			// Clear out the server list.
			aServerInfo.Clear( );
		}

		time( &tNow );

		while ( true )
		{
			const LONG lCommand = NETWORK_ReadByte( pByteStream );

			switch ( lCommand )
			{
			case MSC_SERVER:
				{
					ServerInfo.ulActiveState = AS_WAITINGFORREPLY;

					// Read in address information.
					ServerInfo.Address.abIP[0] = NETWORK_ReadByte( pByteStream );
					ServerInfo.Address.abIP[1] = NETWORK_ReadByte( pByteStream );
					ServerInfo.Address.abIP[2] = NETWORK_ReadByte( pByteStream );
					ServerInfo.Address.abIP[3] = NETWORK_ReadByte( pByteStream );
					ServerInfo.Address.usPort = htons( NETWORK_ReadShort( pByteStream ));

					ServerInfo.tLastQuery = tNow;

					// Add this server's info to our list, and query it.			
					i = aServerInfo.Push( ServerInfo );
					ZANDRONUM_QueryServer( &aServerInfo[i] );
				}
				break;

			case MSC_SERVERBLOCK:
				{
					// Read in address information.
					ULONG ulPorts = 0;
					while (( ulPorts = NETWORK_ReadByte( pByteStream ) ))
					{
						ServerInfo.Address.abIP[0] = NETWORK_ReadByte( pByteStream );
						ServerInfo.Address.abIP[1] = NETWORK_ReadByte( pByteStream );
						ServerInfo.Address.abIP[2] = NETWORK_ReadByte( pByteStream );
						ServerInfo.Address.abIP[3] = NETWORK_ReadByte( pByteStream );
						for ( ULONG ulIdx = 0; ulIdx < ulPorts; ++ulIdx )
						{
							ServerInfo.ulActiveState = AS_WAITINGFORREPLY;
							ServerInfo.Address.usPort = htons( NETWORK_ReadShort( pByteStream ));

							ServerInfo.tLastQuery = tNow;

							// Add this server's info to our list, and query it.			
							i = aServerInfo.Push( ServerInfo );
							ZANDRONUM_QueryServer( &aServerInfo[i] );
						}
					}

				}
				break;

			case MSC_ENDSERVERLISTPART:
				return false;

			case MSC_ENDSERVERLIST:
				Printf( "Received %d Zandronum servers.\n", aServerInfo.Size( ));

				// Since we got the server list, return true.
				return true;

			default:

				Printf( "Unknown server list command from master server: %d\n", static_cast<int> (lCommand) );
				return false;
			}
		}
	}

	return true;
}

//*****************************************************************************
//
void ZANDRONUM_QueryServer( SERVER_s *pServer )
{
	// Clear out the buffer, and write out launcher challenge.
	NETWORK_ClearBuffer( &g_MessageBuffer );
	NETWORK_WriteLong( &g_MessageBuffer.ByteStream, LAUNCHER_SERVER_CHALLENGE );
	NETWORK_WriteLong( &g_MessageBuffer.ByteStream, SQF_NUMPLAYERS|SQF_PLAYERDATA|SQF_GAMETYPE );
	NETWORK_WriteLong( &g_MessageBuffer.ByteStream, 0 );

	// Send the server our packet.
	NETWORK_LaunchPacket( &g_MessageBuffer, pServer->Address, true );
}

//*****************************************************************************
//
bool ZANDRONUM_ParseServerResponse( BYTESTREAM_s *pByteStream, SERVER_s *pServer, TArray<QUERY_s>&aQueryInfo )
{
	LONG		lCommand;
	LONG		lGameType = GAMETYPE_COOPERATIVE;
	LONG		lNumPWADs;
	LONG		lNumPlayers;
	LONG		lNumRealPlayers;
	LONG		lSpectators = 0;
	ULONG		i;
	ULONG		ulBits;

	// Check to see if we got an "ignored" or "banned" response.
	lCommand = NETWORK_ReadLong( pByteStream );
	switch ( lCommand )
	{
	case SERVER_LAUNCHER_IGNORING:

//		pServer->ulActiveState = AS_IGNORED;
		return true;
	case SERVER_LAUNCHER_BANNED:

		pServer->ulActiveState = AS_BANNED;
		Printf( "We're BANNED! AAAAHH!\n" );
		return true;
	}

	if ( lCommand != SERVER_LAUNCHER_CHALLENGE )
	{
		Printf ( "Received unknown command %d from a server.\n", lCommand );
		return true;
	}

	// Make sure this is a server we're actually waiting for a reply from.
	if ( pServer->ulActiveState != AS_WAITINGFORREPLY )
	{
		while ( NETWORK_ReadByte( pByteStream ) != -1 )
			;

		return true;
	}

	// This server is now active.
	pServer->ulActiveState = AS_ACTIVE;

	// Read in the time we sent to the server.
	NETWORK_ReadLong( pByteStream );

	std::string version = NETWORK_ReadString( pByteStream );

	// [BB] If we want do ignore a server based on its version, do so here.
	/*
	if ( strnicmp( version.c_str(), "0.98", 4 ) != 0 )
	{
		while ( NETWORK_ReadByte( pByteStream ) != -1 )
			;

		return true;
	}
	*/

	// Read in the bits.
	ulBits = NETWORK_ReadLong( pByteStream );

	// Read the server name.
	if ( ulBits & SQF_NAME )
		NETWORK_ReadString( pByteStream );

	// Read the website URL.
	if ( ulBits & SQF_URL )
		NETWORK_ReadString( pByteStream );

	// Read the host's e-mail address.
	if ( ulBits & SQF_EMAIL )
		NETWORK_ReadString( pByteStream );

	// Read the map name.
	if ( ulBits & SQF_MAPNAME )
		NETWORK_ReadString( pByteStream );

	// Read the maximum number of clients.
	if ( ulBits & SQF_MAXCLIENTS )
		NETWORK_ReadByte( pByteStream );

	// Maximum slots.
	if ( ulBits & SQF_MAXPLAYERS )
		NETWORK_ReadByte( pByteStream );

	// Read in the PWAD information.
	if ( ulBits & SQF_PWADS )
	{
		lNumPWADs = NETWORK_ReadByte( pByteStream );
		for ( i = 0; i < (ULONG)lNumPWADs; i++ )
			NETWORK_ReadString( pByteStream );
	}

	// Read the game type.
	if ( ulBits & SQF_GAMETYPE )
	{
		lGameType = NETWORK_ReadByte( pByteStream );
		NETWORK_ReadByte( pByteStream );
		NETWORK_ReadByte( pByteStream );
	}

	// Game name.
	if ( ulBits & SQF_GAMENAME )
		NETWORK_ReadString( pByteStream );

	// Read in the IWAD name.
	if ( ulBits & SQF_IWAD )
		NETWORK_ReadString( pByteStream );

	// Force password.
	if ( ulBits & SQF_FORCEPASSWORD )
		NETWORK_ReadByte( pByteStream );

	// Force join password.
	if ( ulBits & SQF_FORCEJOINPASSWORD )
		NETWORK_ReadByte( pByteStream );

	// Game skill.
	if ( ulBits & SQF_GAMESKILL )
		NETWORK_ReadByte( pByteStream );

	// Bot skill.
	if ( ulBits & SQF_BOTSKILL )
		NETWORK_ReadByte( pByteStream );

	// DMFlags, DMFlags2, compatflags.
	if ( ulBits & SQF_DMFLAGS )
	{
		NETWORK_ReadLong( pByteStream );
		NETWORK_ReadLong( pByteStream );
		NETWORK_ReadLong( pByteStream );
	}

	// Fraglimit, timelimit, duellimit, pointlimit, winlimit.
	if ( ulBits & SQF_LIMITS )
	{
		NETWORK_ReadShort( pByteStream );
		if ( NETWORK_ReadShort( pByteStream ))
		{
			// Time left.
			NETWORK_ReadShort( pByteStream );
		}

		NETWORK_ReadShort( pByteStream );
		NETWORK_ReadShort( pByteStream );
		NETWORK_ReadShort( pByteStream );
	}

	// Team damage scale.
	if ( ulBits & SQF_TEAMDAMAGE )
		NETWORK_ReadFloat( pByteStream );

	if ( ulBits & SQF_TEAMSCORES )
	{
		// Blue score.
		NETWORK_ReadShort( pByteStream );

		// Red score.
		NETWORK_ReadShort( pByteStream );
	}

	// Read in the number of players.
	if ( ulBits & SQF_NUMPLAYERS )
	{
		lNumPlayers = NETWORK_ReadByte( pByteStream );
		lNumRealPlayers = lNumPlayers;

		if ( ulBits & SQF_PLAYERDATA )
		{
			for ( i = 0; i < (ULONG)lNumPlayers; i++ )
			{
				// Read in this player's name.
				NETWORK_ReadString( pByteStream );

				// Read in "fragcount" (could be frags, points, etc.)
				NETWORK_ReadShort( pByteStream );

				// Read in the player's ping.
				NETWORK_ReadShort( pByteStream );

				// Read in whether or not the player is spectating.
				if ( NETWORK_ReadByte( pByteStream ) )
					lSpectators++;

				// Read in whether or not the player is a bot.
				if ( NETWORK_ReadByte( pByteStream ))
					lNumRealPlayers--;

				if (( lGameType == GAMETYPE_TEAMPLAY ) ||
					( lGameType == GAMETYPE_TEAMLMS ) ||
					( lGameType == GAMETYPE_TEAMPOSSESSION ) ||
					( lGameType == GAMETYPE_SKULLTAG ) ||
					( lGameType == GAMETYPE_CTF ) ||
					( lGameType == GAMETYPE_ONEFLAGCTF ) ||
					( lGameType == GAMEMODE_DOMINATION ) )
				{
					// Team.
					NETWORK_ReadByte( pByteStream );
				}

				// Time.
				NETWORK_ReadByte( pByteStream );
			}
		}
	}

	aQueryInfo[aQueryInfo.Size( ) - 1].qTotal.lNumPlayers += lNumRealPlayers;
	aQueryInfo[aQueryInfo.Size( ) - 1].qTotal.lNumSpectators += lSpectators;
	aQueryInfo[aQueryInfo.Size( ) - 1].qTotal.lNumServers++;

	aQueryInfo[aQueryInfo.Size( ) - 1].qByGameMode[lGameType].lNumPlayers += lNumRealPlayers;
	aQueryInfo[aQueryInfo.Size( ) - 1].qByGameMode[lGameType].lNumSpectators += lSpectators;
	aQueryInfo[aQueryInfo.Size( ) - 1].qByGameMode[lGameType].lNumServers++;

	// Success!
	return true;
}
