//------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Skulltag Statsmaker Source
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
// Date created: 8/15/08
//
//
// Filename: collector.cpp
//
// Description: Executes the collection, reading, and writing of statistics.
//
//----------------------------------------------------------------------------------------------------------------------------------------------------

#include "i_system.h"
#include "../src/networkheaders.h"
#include "../src/networkshared.h"

#include "collector.h"
#include "main.h"
#include "gui.h"
#include "network.h"

#include <math.h>
#include <time.h>

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- VARIABLES -------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

extern	PORT_s				g_PortInfo[NUM_PORTS];
static	char				g_szGametypeNames[NUM_GAMETYPES][32] =
{
	"Cooperative", 
	"Survival",
	"Invasion",
	"Deathmatch",
	"Team DM",
	"Duel",
	"Terminator",
	"LMS",
	"Team LMS",
	"Possession",
	"Team Possession",
	"Teamgame",
	"CTF",
	"One Flag CTF",
	"Skulltag",
	"Domination",
};

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- PROTOTYPES ------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

static	void				collector_ExportToFile( );
static	void				collector_ImportFromFile( );
static	void				collector_ParseIncomingPackets( );
static	void				collector_StartNewQuery( PORT_s &Port );
static	void				collector_RetryServers( PORT_s &Port );
static	bool				collector_NeedToDecode( NETADDRESS_s Address );
static	SERVER_s			*collector_FindServerByAddress( NETADDRESS_s Address );

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

//==========================================================================
//
// COLLECTOR_StartCollecting
//
// Starts infinitely collecting statistics.
//
//==========================================================================

#ifdef NO_GUI
int COLLECTOR_StartCollecting( void )
#else
DWORD WINAPI COLLECTOR_StartCollecting( LPVOID )
#endif
{	
	// Look for a partial stats file from today.
	collector_ImportFromFile( );

	// Start the infinite loop of querying.
	while ( 1 )
	{
		time_t	tNow;
		time( &tNow );

		// First, parse any incoming packets.
		collector_ParseIncomingPackets( );
		
		// Run through all the ports.
		for ( unsigned int i = 0; i < NUM_PORTS; i++ )
		{
			if ( !g_PortInfo[i].bEnabled )
				continue;

			// Is it time to do a new query?
			if (( tNow - g_PortInfo[i].MasterServerInfo.tLastQuery ) > MASTER_REQUERY_RATE )
			{
				collector_StartNewQuery( g_PortInfo[i] );
				continue;
			}

			// Is it time to retry non-responding servers?
			collector_RetryServers( g_PortInfo[i] );
		}

		// 1 FPS ought to be enough!
		Sleep( 1000 );		
	}

	return 0;
}

//==========================================================================
//
// collector_StartNewQuery
//
// Begins a query. Asks the given port's master for the server list.
//
//==========================================================================

static void collector_StartNewQuery( PORT_s &Port )
{
	time_t	tNow;
	time( &tNow );

	// Send the request.
	Printf( "Querying %s...\n", Port.szName );
	Port.pvQueryMasterServer( );
	Port.MasterServerInfo.ulActiveState = AS_WAITINGFORREPLY;
	Port.MasterServerInfo.tLastQuery = tNow;
	Port.ulNumRetries = 0;
}

//==========================================================================
//
// collector_RetryServers
//
// Retries all the servers that haven't responded to the query. Has flood control.
//
//==========================================================================

static void collector_RetryServers( PORT_s &Port )
{
	int		iNumServersToRetry = 0;
	time_t	tNow;

	time( &tNow );

	// Count all the unresponsive servers (that we haven't tried recently).
	for ( unsigned int s = 0; s < Port.aServerInfo.Size( ); s++ )
	{
		if ( Port.aServerInfo[s].ulActiveState == AS_WAITINGFORREPLY && ( tNow - Port.aServerInfo[s].tLastQuery > SERVER_RETRY_RATE ))
			iNumServersToRetry++;
	}

	// Requery them. [BB] but give up when they don't answer too often.
	if ( ( iNumServersToRetry > 0 ) && ( Port.ulNumRetries < SERVER_MAX_RETRY_AMOUNT ) )
	{
		++Port.ulNumRetries;

		Printf( "Retrying %d non-responding %s servers...\n", iNumServersToRetry, Port.szName );
		for ( unsigned int s = 0; s < Port.aServerInfo.Size( ); s++ )
		{
			if ( Port.aServerInfo[s].ulActiveState == AS_WAITINGFORREPLY && ( tNow - Port.aServerInfo[s].tLastQuery > SERVER_RETRY_RATE ))
			{
				Port.pvQueryServer( &Port.aServerInfo[s] );
				Port.aServerInfo[s].tLastQuery = tNow;
			}
		}
	}
}

//==========================================================================
//
// collector_ParseIncomingPackets
//
// Reads and parses any packets sent to us.
//
//==========================================================================

void collector_ParseIncomingPackets( )
{
	while ( NETWORK_GetPackets( ))
	{
		BYTESTREAM_s	*pByteStream;

		// If the packet is from a port using Huffman compression, decompress it.
		if ( collector_NeedToDecode( NETWORK_GetFromAddress( )))
			NETWORK_DecodePacket( );

		// Set up our byte stream.
		pByteStream = &NETWORK_GetNetworkMessageBuffer( )->ByteStream;
		pByteStream->pbStream = NETWORK_GetNetworkMessageBuffer( )->pbData;
		pByteStream->pbStreamEnd = pByteStream->pbStream + NETWORK_GetNetworkMessageBuffer( )->ulCurrentSize;

		// Find which port the packet belongs to.
		for ( int ulIdx = 0; ulIdx < NUM_PORTS; ulIdx++ )
		{
			if ( !g_PortInfo[ulIdx].bEnabled )
				continue;

			// If it's from the master, parse the server list.
			if ( NETWORK_CompareAddress( NETWORK_GetFromAddress( ), g_PortInfo[ulIdx].MasterServerInfo.Address, false ))
			{
				if ( g_PortInfo[ulIdx].pvParseMasterServerResponse( pByteStream, g_PortInfo[ulIdx].aServerInfo, g_PortInfo[ulIdx].aQueryInfo ))										
					g_PortInfo[ulIdx].MasterServerInfo.ulActiveState = AS_ACTIVE;
			}

			// If it's from a server, collect its statistics.
			else
			{
				SERVER_s *pServer = collector_FindServerByAddress( NETWORK_GetFromAddress( ));

				if (( pServer ) &&
					( g_PortInfo[ulIdx].pvParseServerResponse( pByteStream, pServer, g_PortInfo[ulIdx].aQueryInfo )))
				{
					collector_ExportToFile( );
					GUI_UpdateStatisticsDisplay( );
				}
				break;
			}
		}
	}
}

//==========================================================================
//
// collector_GetStatsFileName
//
// Creates a filename for today's stats file, which goes into pszBuffer.
//
//==========================================================================

void collector_GetStatsFileName( PORT_s Port, char *pszBuffer )
{
	time_t			tNow;	
	char			szDate[64];

	time( &tNow );
	struct tm		*pTimeInfo = localtime( &tNow );

	strftime( szDate, 64, "%Y_%m_%d", pTimeInfo );
	sprintf( pszBuffer, "stats_%s_%s.csv", Port.szName, szDate );
}

//==========================================================================
//
// collector_ImportFromFile
//
// Attempts to read the stats file for each port.
//
//==========================================================================

void collector_ImportFromFile( )
{
	FILE			*pFile;
	char			szFileName[128];
	char			cChar;

	for( int i = 0; i < NUM_PORTS; i++ )
	{
		if ( !g_PortInfo[i].bEnabled )
			continue;

		collector_GetStatsFileName( g_PortInfo[i], szFileName );
		if ( pFile = fopen( szFileName, "r" ))
		{
			//=============================
			// Read off the column headers.
			//=============================

			while ( !feof( pFile ))
			{
				cChar = fgetc( pFile );

				if (( cChar == '\n' ) || ( cChar == '\r' ))
					break;
			}

			//==========================
			// Now, read in the queries.
			//==========================

			unsigned int iHour = 0, iMinute = 0, iNumPlayers = 0, iNumServers = 0, iTries = 0;
			while ( !feof( pFile ))
			{
				if ( fscanf( pFile, "%d:%d,", &iHour, &iMinute ) == 2 )
				{
					iTries = 0;

					// Recreate the query.
					QUERY_s Query;

					char	szLine[2048];

					int		iGametype = -1;
					int		iComponent = 0;

					int		i = 0;
					while ( !feof( pFile ))
					{
						cChar = fgetc( pFile );
						bool bEndOfLine = false;
						
						if (( cChar == '\n' ) || ( cChar == '\r' ))
							bEndOfLine = true;
						if ( cChar == ',' || bEndOfLine )
						{
							szLine[i] = 0;
							i = 0;
							int number = atoi( szLine );

							QSTAT_s *pqStat;

							// First read the total count; then, do each game mode.
							if ( iGametype == -1 )
								pqStat = &Query.qTotal;
							else
								pqStat = &Query.qByGameMode[iGametype];

							// The first number is the player count.
							if ( iComponent == 0 )
							{
								pqStat->lNumPlayers = number;
								iComponent++;
							}
							// Followed by the server count.
							else if ( iComponent == 1 )
							{
								pqStat->lNumServers = number;
								iComponent++;
							}
							// And the spectator count.
							else
							{
								pqStat->lNumSpectators = number;
								iComponent = 0;
								iGametype++;
							}

							if ( i > 127 )
							{
								Printf( "collector_ImportFromFile: Entry is too big!\n" );
								return;
							}

							if ( iGametype > NUM_GAMETYPES )
							{
								Printf( "collector_ImportFromFile: Too many game types!\n" );
								return;
							}
						}
						else
						{
							szLine[i++] = cChar;	
						}
						
						if ( bEndOfLine )
							break;
					}

					// Recreate the time_t for this query.
					time_t tNow;
					time( &tNow );
					struct tm		*pTimeInfo = localtime( &tNow );
					pTimeInfo->tm_hour = iHour;
					pTimeInfo->tm_min = iMinute;
					Query.tTime = mktime( pTimeInfo );

					// Add the query to the port's list.
					g_PortInfo[i].aQueryInfo.Push( Query );
				}

				iTries++;
				if ( iTries > 5 )
					break;
			}		

			if ( g_PortInfo[i].aQueryInfo.Size( ) > 0 )
				Printf( "Read in %d %s queries from the stats file.\n", g_PortInfo[i].aQueryInfo.Size( ), g_PortInfo[i].szName );
		}
	}

	GUI_UpdateStatisticsDisplay( );
}

//==========================================================================
//
// collector_ExportToFile
//
// Rewrites today's stats file.
//
//==========================================================================

void collector_ExportToFile( )
{
	GUI_SetWritingMode( true );

	time_t			tNow;
	struct tm		*pTimeInfo;

	time( &tNow );
	pTimeInfo = localtime( &tNow );
	unsigned int	iToday = pTimeInfo->tm_mday;
				

	for ( int i = 0; i < NUM_PORTS; i++ )
	{
		if ( !g_PortInfo[i].bEnabled )
			continue;

		//====================================
		// Remove any queries from other days.
		//====================================

		unsigned int q = 0;
		while ( q < g_PortInfo[i].aQueryInfo.Size( ) )
		{			
			struct tm *pQueryTimeInfo = localtime( &g_PortInfo[i].aQueryInfo[q].tTime );
			
			if ( pQueryTimeInfo->tm_mday != iToday )
			{
				g_PortInfo[i].aQueryInfo.Delete( q );
				Printf( "Removing old query from %d/%d...\n" , pQueryTimeInfo->tm_mon + 1, pQueryTimeInfo->tm_mday );
			}
			else
				q++;
		}

		// No queries.
		if ( g_PortInfo[i].aQueryInfo.Size( ) == 0 )
			continue;

		//==========================================
		// Open the file, and write all the queries.
		//==========================================

		FILE			*pFile;
		char			szFileName[128];

		collector_GetStatsFileName( g_PortInfo[i], szFileName );
		if ( pFile = fopen( szFileName, "w" ))
		{
			char	szOutString[128];

			fputs( "Time,Players,Servers,Spectators", pFile );
			for ( unsigned int g = 0; g < NUM_GAMETYPES; g++ )
			{
				sprintf( szOutString, ",%s Players,%s Servers,%s Spectators", g_szGametypeNames[g], g_szGametypeNames[g], g_szGametypeNames[g] );
				fputs( szOutString, pFile );
			}
			fputs( "\n", pFile );

			// Print each query.
			for ( q = 0; q < g_PortInfo[i].aQueryInfo.Size( ); q++ )
			{
				struct tm *pQueryTimeInfo = localtime( &g_PortInfo[i].aQueryInfo[q].tTime );

				sprintf( szOutString, "%02d:%02d,%d,%d,%d", pQueryTimeInfo->tm_hour, pQueryTimeInfo->tm_min, g_PortInfo[i].aQueryInfo[q].qTotal.lNumPlayers, g_PortInfo[i].aQueryInfo[q].qTotal.lNumServers, g_PortInfo[i].aQueryInfo[q].qTotal.lNumSpectators );
				fputs( szOutString, pFile );
				for ( unsigned int g = 0; g < NUM_GAMETYPES; g++ )
				{
					sprintf( szOutString, ",%d,%d,%d", g_PortInfo[i].aQueryInfo[q].qByGameMode[g].lNumPlayers, g_PortInfo[i].aQueryInfo[q].qByGameMode[g].lNumServers, g_PortInfo[i].aQueryInfo[q].qByGameMode[g].lNumSpectators );
					fputs( szOutString, pFile );
				}
				
				fputs( "\n", pFile );
			}

			fclose( pFile );
		}
	}

	GUI_SetWritingMode( false );
}

//==========================================================================
//
// collector_NeedToDecode
//
// Returns whether the given address corresponds to a port using Huffman.
//
//==========================================================================

bool collector_NeedToDecode( NETADDRESS_s Address )
{
	ULONG	ulIdx;
	ULONG	ulIdx2;

	for ( ulIdx = 0; ulIdx < NUM_PORTS; ulIdx++ )
	{
		if ( NETWORK_CompareAddress( Address, g_PortInfo[ulIdx].MasterServerInfo.Address, false ))
			return ( g_PortInfo[ulIdx].bHuffman );

		for ( ulIdx2 = 0; ulIdx2 < g_PortInfo[ulIdx].aServerInfo.Size( ); ulIdx2++ )
		{
			if ( NETWORK_CompareAddress( Address, g_PortInfo[ulIdx].aServerInfo[ulIdx2].Address, false ))
				return ( g_PortInfo[ulIdx].bHuffman );
		}
	}

	return false;
}

//==========================================================================
//
// collector_FindServerByAddress
//
// Returns the server (not master server!) residing at the given address.
//
//==========================================================================

static SERVER_s *collector_FindServerByAddress( NETADDRESS_s Address )
{
	for ( unsigned int i = 0; i < NUM_PORTS; i++ )
	{
		if ( !g_PortInfo[i].bEnabled )
			continue;

		for ( unsigned int s = 0; s < g_PortInfo[i].aServerInfo.Size( ); s++ )
		{
			if ( NETWORK_CompareAddress( Address, g_PortInfo[i].aServerInfo[s].Address, false ))
				return ( &g_PortInfo[i].aServerInfo[s] );
		}
	}
	
	return ( NULL );
}
