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
// Filename: sv_rcon.h
//
// Description: Server-side management of the RCON utility sessions. (This doesn't cover players using the 'RCON' command, yet)
//
//----------------------------------------------------------------------------------------------------------------------------------------------------

#ifndef __SV_RCON_H__
#define __SV_RCON_H__

#ifndef IN_RCON_UTILITY
#include "network.h"
#endif

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- DEFINES ---------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

#define PROTOCOL_VERSION			4
#define MIN_PROTOCOL_VERSION        3
#define RCON_CANDIDATE_TIMEOUT_TIME 10
#define RCON_CLIENT_TIMEOUT_TIME	40
#define BAD_QUERY_IGNORE_TIME		4

//*****************************************************************************
// Messages sent from the server to the RCON utility.
enum
{
	SVRC_OLDPROTOCOL = 32,
	SVRC_BANNED,
	SVRC_SALT,
	SVRC_LOGGEDIN,
	SVRC_INVALIDPASSWORD,
	SVRC_MESSAGE,
	SVRC_UPDATE,
	SVRC_TABCOMPLETE,
	SVRC_TOOMANYTABCOMPLETES,
};

//*****************************************************************************
// Messages sent from the RCON utility to the server.
enum
{
	CLRC_BEGINCONNECTION = 52,
	CLRC_PASSWORD,
	CLRC_COMMAND,
	CLRC_PONG,
	CLRC_DISCONNECT,
	CLRC_TABCOMPLETE,
};

//*****************************************************************************
// Various types of updates sent from the server to the RCON utility.
enum
{
	SVRCU_PLAYERDATA,
	SVRCU_ADMINCOUNT,
	SVRCU_MAP,

	NUM_RCON_UPDATES
};

#ifndef IN_RCON_UTILITY

//*****************************************************************************
struct RCONCANDIDATE_s
{		
	NETADDRESS_s	Address;

	char			szSalt[33];	

	int				iLastMessageTic;

};

//*****************************************************************************
struct RCONCLIENT_s
{
	NETADDRESS_s	Address;

	int				iLastMessageTic;

};

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- PROTOTYPES ------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

void SERVER_RCON_Construct( );
void SERVER_RCON_Destruct( );
void SERVER_RCON_Tick( );
void SERVER_RCON_ParseMessage( NETADDRESS_s Address, LONG lMessage, BYTESTREAM_s *pByteStream );
void SERVER_RCON_Print( const char *pszString );
void SERVER_RCON_UpdateInfo( int iUpdateType );

#endif

#endif	// __SV_RCON_H__
