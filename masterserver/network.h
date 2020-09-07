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
// Filename: network.h
//
// Description: Contains network definitions and functions not specifically
// related to the server or client.
//
//-----------------------------------------------------------------------------

#ifndef __NETWORK_H__
#define __NETWORK_H__

#include <stdio.h>
//#include "c_cvars.h"
//#include "d_player.h"
//#include "i_net.h"
//#include "sv_main.h"
#include "../src/networkshared.h"

//*****************************************************************************
//	DEFINES

// Network messages (universal)
#define	NETWORK_ERROR			254

//*****************************************************************************
//	PROTOTYPES

void			NETWORK_Construct( USHORT usPort, const char *pszIPAddress = NULL );

int				NETWORK_GetPackets( void );
int				NETWORK_GetLANPackets( void );
NETADDRESS_s	NETWORK_GetFromAddress( void );
void			NETWORK_LaunchPacket( NETBUFFER_s *pBuffer, NETADDRESS_s Address );
//AActor			*NETWORK_FindThingByNetID( LONG lID );
NETADDRESS_s	NETWORK_GetLocalAddress( void );
NETBUFFER_s		*NETWORK_GetNetworkMessageBuffer( void );
ULONG			NETWORK_ntohs( ULONG ul );
USHORT			NETWORK_GetLocalPort( void );

// Access functions.
LONG			NETWORK_GetState( void );
void			NETWORK_SetState( LONG lState );

void			I_DoSelect( void );

// DEBUG FUNCTION!
void	NETWORK_FillBufferWithShit( NETBUFFER_s *pBuffer, ULONG ulSize );

#endif	// __NETWORK_H__
