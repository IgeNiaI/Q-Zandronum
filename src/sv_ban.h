//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2004 Brad Carney
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
// Filename: sv_ban.h
//
// Description: Support for banning IPs from the server.
//
//-----------------------------------------------------------------------------

#ifndef __SV_BAN_H__
#define __SV_BAN_H__

#include <time.h>
#include "sv_main.h"

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- PROTOTYPES ------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

void			SERVERBAN_Tick( void );
bool			SERVERBAN_IsIPBanned( const IPStringArray &szAddress );
bool			SERVERBAN_IsIPBanned( const NETADDRESS_s &Address );
bool			SERVERBAN_IsIPMasterBanned( const NETADDRESS_s &Address );
void			SERVERBAN_ClearBans( void );
void			SERVERBAN_ReadMasterServerBans( BYTESTREAM_s *pByteStream );
void			SERVERBAN_ReadMasterServerBanlistPart( BYTESTREAM_s *pByteStream );
time_t			SERVERBAN_ParseBanLength( const char *szLengthString );
IPList			*SERVERBAN_GetBanList( void );
IPList			*SERVERBAN_GetBanExemptionList( void );
void			SERVERBAN_BanPlayer( ULONG ulPlayer, const char *pszBanLength, const char *pszBanReason );

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- EXTERNAL CONSOLE VARIABLES --------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

EXTERN_CVAR( Bool, sv_enforcebans );
EXTERN_CVAR( Bool, sv_enforcemasterbanlist );
EXTERN_CVAR( String, sv_banfile );
EXTERN_CVAR( String, sv_banexemptionfile );
EXTERN_CVAR( String, sv_adminlistfile );

#endif	// __SV_BAN_H__
