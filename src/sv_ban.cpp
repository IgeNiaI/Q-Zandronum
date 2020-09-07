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
// Filename: sv_ban.cpp
//
// Description: Support for banning IPs from the server.
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <errno.h>

#include "c_dispatch.h"
#include "doomstat.h"
#include "network.h"
#include "sv_ban.h"
#include "version.h"
#include "v_text.h"

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- VARIABLES -------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

static	IPList	g_ServerBans;
static	IPList	g_ServerBanExemptions;

static	IPList	g_MasterServerBans;
static	IPList	g_MasterServerBanExemptions;

static	ULONG	g_ulReParseTicker;

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- PROTOTYPES ------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

static	void	serverban_LoadBansAndBanExemptions( void );
static	void	serverban_KickBannedPlayers( void );
static	LONG	serverban_ExtractBanLength( FString fSearchString, const char *pszPattern );
static	time_t	serverban_CreateBanDate( LONG lAmount, ULONG ulUnitSize, time_t tNow );

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- CVARS -----------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

CVAR( Bool, sv_enforcebans, true, CVAR_ARCHIVE|CVAR_NOSETBYACS )
CVAR( Int, sv_banfilereparsetime, 0, CVAR_ARCHIVE|CVAR_NOSETBYACS )

//*****************************************************************************
//
CUSTOM_CVAR( Bool, sv_enforcemasterbanlist, true, CVAR_ARCHIVE|CVAR_NOSETBYACS )
{
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		return;

	// [BB] If we are enforcing the master bans, make sure master bannded players are kicked now.
	if ( self == true )
		serverban_KickBannedPlayers( );
}

//*****************************************************************************
//
CUSTOM_CVAR( String, sv_banfile, "banlist.txt", CVAR_ARCHIVE|CVAR_NOSETBYACS )
{
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		return;

	if ( !( g_ServerBans.clearAndLoadFromFile( sv_banfile )))
		Printf( "%s", g_ServerBans.getErrorMessage( ));

	// Re-parse the file periodically.
	g_ulReParseTicker = sv_banfilereparsetime * TICRATE;
}

//*****************************************************************************
//
CUSTOM_CVAR( String, sv_banexemptionfile, "whitelist.txt", CVAR_ARCHIVE|CVAR_NOSETBYACS )
{
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		return;

	if ( ! ( g_ServerBanExemptions.clearAndLoadFromFile( sv_banexemptionfile )))
		Printf( "%s", g_ServerBanExemptions.getErrorMessage( ));
}

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

//*****************************************************************************
//
void SERVERBAN_Tick( void )
{
	// [RC] Remove any old tempbans.
	g_ServerBans.removeExpiredEntries( );

	// Is it time to re-parse the ban lists?
	if ( g_ulReParseTicker && (( --g_ulReParseTicker ) == 0 ))
	{
		serverban_LoadBansAndBanExemptions( );

		// Parse again periodically.
		g_ulReParseTicker = sv_banfilereparsetime * TICRATE;
	}
}

//*****************************************************************************
//
bool SERVERBAN_IsIPBanned( const IPStringArray &szAddress )
{
	// Is this address is banned on the master server?
	if ( sv_enforcemasterbanlist && g_MasterServerBans.isIPInList( szAddress ) && !g_MasterServerBanExemptions.isIPInList( szAddress ))
		return true;

	// If not, let the server decide.
	return ( sv_enforcebans && g_ServerBans.isIPInList( szAddress ) && !g_ServerBanExemptions.isIPInList( szAddress ));
}

//*****************************************************************************
//
bool SERVERBAN_IsIPMasterBanned( const NETADDRESS_s &Address )
{
	return sv_enforcemasterbanlist
		&& g_MasterServerBans.isIPInList( Address )
		&& g_MasterServerBanExemptions.isIPInList( Address ) == false;
}

//*****************************************************************************
//
bool SERVERBAN_IsIPBanned( const NETADDRESS_s &Address )
{
	// Is this address is banned on the master server?
	if ( SERVERBAN_IsIPMasterBanned( Address ))
		return true;

	// If not, let the server decide.
	return ( sv_enforcebans && g_ServerBans.isIPInList( Address ) && !g_ServerBanExemptions.isIPInList( Address ));
}

//*****************************************************************************
//
void SERVERBAN_ClearBans( void )
{
	FILE		*pFile;

	// Clear out the existing bans in memory.
	g_ServerBans.clear( );

	// Export the cleared banlist.
	if (( pFile = fopen( sv_banfile, "w" )) != NULL )
	{
		FString message;
		message.AppendFormat( "// This is a %s server IP list.\n// Format: 0.0.0.0 <mm/dd/yy> :optional comment\n\n", GAMENAME );
		fputs( message.GetChars(), pFile );
		fclose( pFile );

		Printf( "Banlist cleared.\n" );
	}
	else
		Printf( "SERVERBAN_ClearBans: Could not open %s for writing: %s\n", *sv_banfile, strerror( errno ));
}

//*****************************************************************************
//
void SERVERBAN_ReadMasterServerBans( BYTESTREAM_s *pByteStream )
{	
	g_MasterServerBans.clear( );
	g_MasterServerBanExemptions.clear( );

	// Read the list of bans.
	for ( LONG i = 0, lNumEntries = NETWORK_ReadLong( pByteStream ); i < lNumEntries; i++ )
	{
		const char		*pszBan = NETWORK_ReadString( pByteStream );
		std::string		Message;

		g_MasterServerBans.addEntry( pszBan, "", "", Message, 0 );
	}

	// Read the list of exemptions.
	for ( LONG i = 0, lNumEntries = NETWORK_ReadLong( pByteStream ); i < lNumEntries; i++ )
	{
		const char		*pszBan = NETWORK_ReadString( pByteStream );
		std::string		Message;

		g_MasterServerBanExemptions.addEntry( pszBan, "", "", Message, 0 );
	}

	// [BB] If we are enforcing the master bans, make sure newly master bannded players are kicked now.
	if ( sv_enforcemasterbanlist )
		serverban_KickBannedPlayers( );

	// [BB] Inform the master that we received the banlist.
	SERVER_MASTER_SendBanlistReceipt();

	// Printf( "Imported %d bans, %d exceptions from the master.\n", g_MasterServerBans.size( ), g_MasterServerBanExemptions.size( ));
}

//*****************************************************************************
//
void SERVERBAN_ReadMasterServerBanlistPart( BYTESTREAM_s *pByteStream )
{
	const ULONG ulPacketNum = NETWORK_ReadByte ( pByteStream );

	// [BB] The implementation assumes that the packets arrive in the correct order.
	if ( ulPacketNum == 0 )
	{
		g_MasterServerBans.clear( );
		g_MasterServerBanExemptions.clear( );
	}

	while ( 1 )
	{
		const LONG lCommand = NETWORK_ReadByte( pByteStream );

		// [BB] End of packet (shouldn't be triggered for proper packets).
		if ( lCommand == -1 )
			break;

		switch ( lCommand )
		{
		case MSB_BAN:
		case MSB_BANEXEMPTION:
			{
				const char *pszBan = NETWORK_ReadString( pByteStream );
				std::string Message;

				if ( lCommand == MSB_BAN )
					g_MasterServerBans.addEntry( pszBan, "", "", Message, 0 );
				else
					g_MasterServerBanExemptions.addEntry( pszBan, "", "", Message, 0 );
			}
			break;

		case MSB_ENDBANLISTPART:
			return;

		case MSB_ENDBANLIST:
			{
				// [BB] If we are enforcing the master bans, make sure newly master bannded players are kicked now.
				if ( sv_enforcemasterbanlist )
					serverban_KickBannedPlayers( );

				// [BB] Inform the master that we received the banlist.
				SERVER_MASTER_SendBanlistReceipt();
			}
			return;
		}
	}
}
//*****************************************************************************
//
// Parses the given ban expiration string, returnining either the time_t, NULL for infinite, or -1 for an error.
time_t SERVERBAN_ParseBanLength( const char *szLengthString )
{
	time_t	tExpiration;
	time_t	tNow;

	FString	fInput = szLengthString;	

	// If the ban is permanent, use NULL.
	// [Dusk] Can't use NULL for time_t...
	if ( stricmp( szLengthString, "perm" ) == 0 )
		return 0;
	else
	{
		time( &tNow );
		tExpiration = 0;

		// Now we check for patterns in the string.

		// Minutes: covers "min", "minute", "minutes".
		if ( !tExpiration )
			tExpiration = serverban_CreateBanDate( serverban_ExtractBanLength( fInput, "min" ), MINUTE, tNow );

		// Hours: covers "hour", "hours".
		if ( !tExpiration )
			tExpiration = serverban_CreateBanDate( serverban_ExtractBanLength( fInput, "hour" ), HOUR, tNow );

		// Hours: covers "hr", "hrs".
		if ( !tExpiration )
			tExpiration = serverban_CreateBanDate( serverban_ExtractBanLength( fInput, "hr" ), HOUR, tNow );

		// Days: covers "day", "days".
		if ( !tExpiration )
			tExpiration = serverban_CreateBanDate( serverban_ExtractBanLength( fInput, "day" ), DAY, tNow );

		// Days: covers "dy", "dys".
		if ( !tExpiration )
			tExpiration = serverban_CreateBanDate( serverban_ExtractBanLength( fInput, "dy" ), DAY, tNow );

		// Weeks: covers "week", "weeks".
		if ( !tExpiration )
			tExpiration = serverban_CreateBanDate( serverban_ExtractBanLength( fInput, "week" ), WEEK, tNow );

		// Weeks: covers "wk", "wks".
		if ( !tExpiration )
			tExpiration = serverban_CreateBanDate( serverban_ExtractBanLength( fInput, "wk" ), WEEK, tNow );

		// Months work a bit differently, since we don't have an arbitrary number of days to move.
		if ( !tExpiration )
		{
			LONG lAmount = serverban_ExtractBanLength( fInput, "mon" );

			if ( lAmount > 0 )
			{
				// Create a new time structure based on the current time.
				struct tm	*pTimeInfo = localtime( &tNow );

				// Move the month forward, and stitch it into a new time.
				pTimeInfo->tm_mon += lAmount;
				tExpiration = mktime( pTimeInfo );
			}
		}

		// So do years (because of leap years).
		if ( !tExpiration )
		{
			LONG lAmount = serverban_ExtractBanLength( fInput, "year" );
			if ( lAmount <= 0 )
				lAmount = serverban_ExtractBanLength( fInput, "yr" );
			if ( lAmount <= 0 )
				lAmount = serverban_ExtractBanLength( fInput, "decade" ) * 10; // :)

			if ( lAmount > 0 )
			{
				// Create a new time structure based on the current time.
				struct tm	*pTimeInfo = localtime( &tNow );

				// Move the year forward, and stitch it into a new time.
				pTimeInfo->tm_year += lAmount;
				tExpiration = mktime( pTimeInfo );
			}
		}
	}

	if ( !tExpiration )
		return -1;

	return tExpiration;
}

//*****************************************************************************
//
IPList *SERVERBAN_GetBanList( void )
{
	return &g_ServerBans;
}

//*****************************************************************************
//
IPList *SERVERBAN_GetBanExemptionList( void )
{
	return &g_ServerBanExemptions;
}

//*****************************************************************************
//
void SERVERBAN_BanPlayer( ULONG ulPlayer, const char *pszBanLength, const char *pszBanReason )
{
	// Make sure the target is valid and applicable.
	if (( ulPlayer >= MAXPLAYERS ) || ( !playeringame[ulPlayer] ) || players[ulPlayer].bIsBot )
	{
		Printf("Error: bad player index, or player is a bot.\n");
		return;
	}

	// [RC] Read the ban length.
	time_t tExpiration = SERVERBAN_ParseBanLength( pszBanLength );
	if ( tExpiration == -1 )
	{
		Printf("Error: couldn't read that length. Try something like \\cg6day\\c- or \\cg\"5 hours\"\\c-.\n");
		return;
	}

	// Removes the color codes from the player name, for the ban record.
	char	szPlayerName[64];
	sprintf( szPlayerName, "%s", players[ulPlayer].userinfo.GetName() );
	V_RemoveColorCodes( szPlayerName );

	// Add the ban and kick the player.
	std::string message;
	g_ServerBans.addEntry( SERVER_GetClient( ulPlayer )->Address.ToString(), szPlayerName, pszBanReason, message, tExpiration );
	Printf( "addban: %s", message.c_str() );
	SERVER_KickPlayer( ulPlayer, pszBanReason ? pszBanReason : "" );  // [RC] serverban_KickBannedPlayers would cover this, but we want the messages to be distinct so there's no confusion.

	// Kick any other players using the newly-banned address.
	serverban_KickBannedPlayers( );
}

//*****************************************************************************
//
static void serverban_LoadBansAndBanExemptions( void )
{
	if ( !( g_ServerBans.clearAndLoadFromFile( sv_banfile )))
		Printf( "%s", g_ServerBans.getErrorMessage( ));

	if ( !( g_ServerBanExemptions.clearAndLoadFromFile( sv_banexemptionfile )))
		Printf( "%s", g_ServerBanExemptions.getErrorMessage( ));

	// Kick any players using a banned address.
	serverban_KickBannedPlayers( );
}

//*****************************************************************************
//
// [RC] Refresher method. Kicks any players who are playing under a banned IP.
//
static void serverban_KickBannedPlayers( void )
{
	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( SERVER_GetClient( ulIdx )->State == CLS_FREE )
			continue;

		if ( SERVERBAN_IsIPBanned( SERVER_GetClient( ulIdx )->Address ))
		{
			const char *pszReason = g_ServerBans.getEntryComment( SERVER_GetClient( ulIdx )->Address );
			FString fsReason = "IP is now banned";

			if ( pszReason != NULL )
				fsReason.AppendFormat( " - %s", pszReason );

			SERVER_KickPlayer( ulIdx, fsReason );
		}
	}
}

//*****************************************************************************
//
// [RC] Helper method for serverban_ParseLength.
static LONG serverban_ExtractBanLength( FString fSearchString, const char *pszPattern )
{
	// Look for the pattern (e.g, "min").
	LONG lIndex = fSearchString.IndexOf( pszPattern );

	if ( lIndex > 0 )
	{
		// Extract the number preceding it ("45min" becomes 45).
		return atoi( fSearchString.Left( lIndex ));
	}
	else
		return 0;
}

//*****************************************************************************
//
// [RC] Helper method for serverban_ParseLength.
static time_t serverban_CreateBanDate( LONG lAmount, ULONG ulUnitSize, time_t tNow )
{
	// Convert to a time in the future (45 MINUTEs becomes 2,700 seconds).
	if ( lAmount > 0 )
		return tNow + ulUnitSize * lAmount;

	// Not found, or bad format.
	else
		return 0;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- CCMDS -----------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

CCMD( getIP )
{
	// Only the server can look this up.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		return;

	if ( argv.argc( ) < 2 )
	{
		Printf( "Usage: getIP <playername> \nDescription: Returns the player's IP address.\n" );
		return;
	}

	// Look up the player.
	ULONG ulIdx = SERVER_GetPlayerIndexFromName( argv[1], true, false );
	if ( SERVER_IsValidClient( ulIdx ))
		Printf( "%s\\c-'s IP is: %s\n", players[ulIdx].userinfo.GetName(), SERVER_GetClient( ulIdx )->Address.ToString() );
	else
	{
		if ( SERVER_GetPlayerIndexFromName( argv[1], true, true ) != MAXPLAYERS )
			Printf( "%s\\c- is a bot.\n", argv[1] );
		else
			Printf( "Unknown player: %s\\c-\n",argv[1] );
	}
}

//*****************************************************************************
//
CCMD( getIP_idx )
{
	// Only the server can look this up.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		return;

	if ( argv.argc( ) < 2 )
	{
		Printf( "Usage: getIP_idx <player index>\nDescription: Returns the player's IP, via his index. You can get the list of players' indexes via the ccmd playerinfo.\n" );
		return;
	}

	int playerIndex;
	if ( argv.SafeGetNumber( 1, playerIndex ) == false )
		return;

	// Make sure the target is valid.
	if (( playerIndex >= MAXPLAYERS ) || ( !playeringame[playerIndex] ))
		return;

	if ( players[playerIndex].bIsBot )
		Printf( "%s\\c- is a bot.\n", players[playerIndex].userinfo.GetName() );
	else
		Printf( "%s\\c-'s IP is: %s\n", players[playerIndex].userinfo.GetName(), SERVER_GetClient( playerIndex )->Address.ToString() );
}

//*****************************************************************************
//
CCMD( ban_idx )
{
	// Only the server can ban players!
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		return;

	if ( argv.argc( ) < 3 )
	{
		Printf( "Usage: ban_idx <player index> <duration> [reason]\nDescriptions: Bans the player, via his index, for the given duration (\"perm\" for a permanent ban).\n To see the list of players' indexes, try the ccmd playerinfo.\n" );
		return;
	}

	int playerIndex;
	if ( argv.SafeGetNumber( 1, playerIndex ) == false )
		return;

	SERVERBAN_BanPlayer( playerIndex, argv[2], (argv.argc( ) >= 4) ? argv[3] : NULL );

}

//*****************************************************************************
//
CCMD( ban )
{
	ULONG	ulIdx;

	// Only the server can ban players!
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		return;

	if ( argv.argc( ) < 3 )
	{
		Printf( "Usage: ban <playername> <duration> [reason]\nDescription: Bans the player for the given duration  (\"perm\" for a permanent ban).\n" );
		return;
	}

	// Get the index.
	ulIdx = SERVER_GetPlayerIndexFromName( argv[1], true, false );

	// Found him?
	if ( ulIdx != MAXPLAYERS )
	{
		char	szString[256];

		// Ban via the index.
		sprintf( szString, "ban_idx %lu \"%s\" \"%s\"", ulIdx, argv[2], (argv.argc( ) >= 4) ? argv[3] : "" );
		SERVER_AddCommand( szString );
	}
	else
	{
		if ( SERVER_GetPlayerIndexFromName( argv[1], true, true ) != MAXPLAYERS )
			Printf( "%s\\c- is a bot.\n", argv[1] );
		else
			Printf( "Unknown player: %s\\c-\n",argv[1] );
	}
}

//*****************************************************************************
//
CCMD( addban )
{
	if ( argv.argc( ) < 3 )
	{
		Printf( "Usage: addban <IP address> <duration> [comment]\nDescription: bans the given IP address.\n" );
		return;
	}

	time_t tExpiration = SERVERBAN_ParseBanLength( argv[2] );
	if ( tExpiration == -1 )
	{
		Printf("Error: couldn't read that length. Try something like \\cg6day\\c- or \\cg\"5 hours\"\\c-.\n");
		return;
	}

	std::string message;
	g_ServerBans.addEntry( argv[1], NULL, (argv.argc( ) >= 4) ? argv[3] : NULL, message, tExpiration );
	Printf( "addban: %s", message.c_str() );

	// Kick any players using the newly-banned address.
	serverban_KickBannedPlayers( );
}

//*****************************************************************************
//
CCMD( delban )
{
	if ( argv.argc( ) < 2 )
	{
		Printf( "Usage: delban <IP address>\n" );
		return;
	}

	std::string message;
	g_ServerBans.removeEntry( argv[1], message );
	Printf( "delban: %s", message.c_str() );
}

//*****************************************************************************
//
CCMD( addbanexemption )
{
	if ( argv.argc( ) < 2 )
	{
		Printf( "Usage: addbanexemption <IP address> [comment]\n" );
		return;
	}

	std::string message;
	g_ServerBanExemptions.addEntry( argv[1], NULL, (argv.argc( ) >= 3) ? argv[2] : NULL, message, 0 );
	Printf( "addbanexemption: %s", message.c_str() );
}

//*****************************************************************************
//
CCMD( delbanexemption )
{
	if ( argv.argc( ) < 2 )
	{
		Printf( "Usage: delbanexemption <IP address>\n" );
		return;
	}

	std::string message;
	g_ServerBanExemptions.removeEntry( argv[1], message );
	Printf( "delbanexemption: %s", message.c_str() );
}

//*****************************************************************************
//
CCMD( viewbanlist )
{
	for ( ULONG ulIdx = 0; ulIdx < g_ServerBans.size(); ulIdx++ )
		Printf( "%s", g_ServerBans.getEntryAsString(ulIdx).c_str( ));
}

//*****************************************************************************
//
CCMD( viewbanexemptionlist )
{
	for ( ULONG ulIdx = 0; ulIdx < g_ServerBanExemptions.size(); ulIdx++ )
		Printf( "%s", g_ServerBanExemptions.getEntryAsString(ulIdx).c_str( ));
}

//*****************************************************************************
//
CCMD( viewmasterbanlist )
{
	for ( ULONG ulIdx = 0; ulIdx < g_MasterServerBans.size(); ulIdx++ )
		Printf( "%s", g_MasterServerBans.getEntryAsString(ulIdx).c_str( ));
}

//*****************************************************************************
//
CCMD( viewmasterexemptionbanlist )
{
	for ( ULONG ulIdx = 0; ulIdx < g_MasterServerBanExemptions.size(); ulIdx++ )
		Printf( "%s", g_MasterServerBanExemptions.getEntryAsString(ulIdx).c_str( ));
}

//*****************************************************************************
//
CCMD( clearbans )
{
	SERVERBAN_ClearBans( );
}

//*****************************************************************************
//
CCMD( reloadbans )
{
	serverban_LoadBansAndBanExemptions( );
}
