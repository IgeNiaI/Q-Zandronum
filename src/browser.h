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
// Date created:  11/26/03
//
//
// Filename: browser.h
//
// Description: Contains browser structures and prototypes
//
//-----------------------------------------------------------------------------

#ifndef __BROWSER_H__
#define __BROWSER_H__

#include "gamemode.h"
#include "network.h"

//*****************************************************************************
//  DEFINES

// Maximum number of servers listed in the browser.
#define		MAX_BROWSER_SERVERS		1024

//*****************************************************************************
enum
{
	AS_INACTIVE,
	AS_WAITINGFORREPLY,
	AS_ACTIVE,

	NUM_ACTIVESTATES
};

//*****************************************************************************
//	STRUCTURES

//*****************************************************************************
typedef struct
{
	// Player's name.
	FString		Name;

	// Fragcount.
	LONG		lFragcount;

	// Ping.
	LONG		lPing;

	// Spectating?
	bool		bSpectating;

	// Is a bot?
	bool		bIsBot;

} SERVERPLAYER_t;

//*****************************************************************************
typedef struct
{
	// What's the state of this server's activity?
	ULONG			ulActiveState;

	// Network address of this server.
	NETADDRESS_s	Address;

	// Name of the server.
	FString			HostName;

	// Website URL of the wad the server is using.
	FString			WadURL;

	// Host's email address.
	FString			EmailAddress;

	// Mapname of the level the server is currently on.
	FString			Mapname;

	// Maximum number of players that can join the server.
	LONG			lMaxClients;

	// Number of PWADs the server is using.
	LONG			lNumPWADs;

	// Names of each PWAD the server is using.
	FString			PWADNames[32];

	// Name of the IWAD being used.
	FString			IWADName;

	// Game mode of the server.
	GAMEMODE_e		GameMode;

	// Number of players on the server.
	LONG			lNumPlayers;

	// Player's playing on the server.
	SERVERPLAYER_t	Players[MAXPLAYERS];

	// Version of the server.
	FString			Version;

	// Was this server broadcasted to us on a LAN?
	bool			bLAN;

	// MS time of when we queried this server.
	LONG			lMSTime;

	// Ping to this server.
	LONG			lPing;

} SERVER_t;

//*****************************************************************************
//	PROTOTYPES

void			BROWSER_Construct( void );
void			BROWSER_Destruct( void );

bool			BROWSER_IsActive( ULONG ulServer );
bool			BROWSER_IsLAN( ULONG ulServer );
NETADDRESS_s	BROWSER_GetAddress( ULONG ulServer );
const char		*BROWSER_GetHostName( ULONG ulServer );
const char		*BROWSER_GetWadURL( ULONG ulServer );
const char		*BROWSER_GetEmailAddress( ULONG ulServer );
const char		*BROWSER_GetMapname( ULONG ulServer );
LONG			BROWSER_GetMaxClients( ULONG ulServer );
LONG			BROWSER_GetNumPWADs( ULONG ulServer );
const char		*BROWSER_GetPWADName( ULONG ulServer, ULONG ulWadIdx );
const char		*BROWSER_GetIWADName( ULONG ulServer );
GAMEMODE_e		BROWSER_GetGameMode( ULONG ulServer );
LONG			BROWSER_GetNumPlayers( ULONG ulServer );
const char		*BROWSER_GetPlayerName( ULONG ulServer, ULONG ulPlayer );
LONG			BROWSER_GetPlayerFragcount( ULONG ulServer, ULONG ulPlayer );
LONG			BROWSER_GetPlayerPing( ULONG ulServer, ULONG ulPlayer );
LONG			BROWSER_GetPlayerSpectating( ULONG ulServer, ULONG ulPlayer );
LONG			BROWSER_GetPing( ULONG ulServer );
const char		*BROWSER_GetVersion( ULONG ulServer );

void			BROWSER_ClearServerList( void );
void			BROWSER_DeactivateAllServers( void );
bool			BROWSER_GetServerList( BYTESTREAM_s *pByteStream );
void			BROWSER_ParseServerQuery( BYTESTREAM_s *pByteStream, bool bLAN );
void			BROWSER_QueryMasterServer( void );
bool			BROWSER_WaitingForMasterResponse( void );
void			BROWSER_QueryAllServers( void );
LONG			BROWSER_CalcNumServers( void );

//*****************************************************************************
//	EXTERNAL CONSOLE VARIABLES

#endif // __BROWSER_H__
