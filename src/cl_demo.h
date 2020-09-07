//-----------------------------------------------------------------------------
//
// Skulltag Source
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
// Date created:  7/2/07
//
//
// Filename: cl_demo.h
//
// Description: 
//
//-----------------------------------------------------------------------------

#ifndef __CL_DEMO_H__
#define __CL_DEMO_H__

#include "d_ticcmd.h"
#include "network.h"
#include "networkshared.h"

//*****************************************************************************
//	DEFINES

enum ClientDemoLocalCommand
{
	CLD_LCMD_INVUSE,
	CLD_LCMD_CENTERVIEW,
	CLD_LCMD_TAUNT,
	CLD_LCMD_CHEAT,
	CLD_LCMD_WARPCHEAT,
};

//*****************************************************************************
//	PROTOTYPES

void		CLIENTDEMO_BeginRecording( const char *pszDemoName );
bool		CLIENTDEMO_ProcessDemoHeader( void );
void		CLIENTDEMO_WriteUserInfo( void );
void		CLIENTDEMO_ReadUserInfo( void );
void		CLIENTDEMO_WriteTiccmd( ticcmd_t *pCmd );
void		CLIENTDEMO_ReadTiccmd( ticcmd_t *pCmd );
void		CLIENTDEMO_WritePacket( BYTESTREAM_s *pByteStream );
void		CLIENTDEMO_InsertPacketAtMarkedPosition( BYTESTREAM_s *pByteStream );
void		CLIENTDEMO_MarkCurrentPosition( void );
void		CLIENTDEMO_ReadPacket( void );
void		CLIENTDEMO_FinishRecording( void );
void		CLIENTDEMO_DoPlayDemo( const char *pszDemoName );
void		CLIENTDEMO_FinishPlaying( void );
LONG		CLIENTDEMO_GetGameticOffset( void );
void		CLIENTDEMO_SetGameticOffset( LONG lOffset );
void		CLIENTDEMO_WriteLocalCommand( ClientDemoLocalCommand command, const char *pszArg );
void		CLIENTDEMO_WriteCheat( ECheatCommand cheat );
void		CLIENTDEMO_WriteWarpCheat( fixed_t x, fixed_t y );
void		CLIENTDEMO_ReadDemoWads( void );
BYTESTREAM_s *CLIENTDEMO_GetDemoStream( void );

bool		CLIENTDEMO_IsRecording( void );
void		CLIENTDEMO_SetRecording( bool bRecording );
bool		CLIENTDEMO_IsPlaying( void );
void		CLIENTDEMO_SetPlaying( bool bPlaying );
bool		CLIENTDEMO_IsPaused( void );
bool		CLIENTDEMO_IsSkipping( void );
bool		CLIENTDEMO_IsSkippingToNextMap( void );
void		CLIENTDEMO_SetSkippingToNextMap( bool bSkipToNextMap );
bool		CLIENTDEMO_IsInFreeSpectateMode( void );
void		CLIENTDEMO_SetFreeSpectatorTiccmd( ticcmd_t *pCmd );
void		CLIENTDEMO_FreeSpectatorPlayerThink( bool bTickBody = false );
player_t	*CLIENTDEMO_GetFreeSpectatorPlayer( void );
bool		CLIENTDEMO_IsFreeSpectatorPlayer( player_t *pPlayer );
void		CLIENTDEMO_ClearFreeSpectatorPlayer( void );

#endif // __CL_DEMO__
