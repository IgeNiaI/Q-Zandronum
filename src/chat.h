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
// Filename: chat.h
//
// Description: Contains chat structures and prototypes
//
//-----------------------------------------------------------------------------

#ifndef __CHAT_H__
#define __CHAT_H__

#include "c_cvars.h"
#include "d_event.h"

//*****************************************************************************
//	DEFINES

// Maximum size of the chat buffer.
#define	MAX_CHATBUFFER_LENGTH		128

//*****************************************************************************
typedef enum
{
	CHATMODE_NONE,
	CHATMODE_GLOBAL,
	CHATMODE_TEAM,

	NUM_CHATMODES

} CHATMODE_e;

//*****************************************************************************
//	PROTOTYPES

void	CHAT_Construct( void );
void	CHAT_Tick( void );
bool	CHAT_Input( event_t *pEvent );
void	CHAT_Render( void );

ULONG	CHAT_GetChatMode( void );
void	CHAT_PrintChatString( ULONG ulPlayer, ULONG ulMode, const char *pszString );

//*****************************************************************************
//  EXTERNAL CONSOLE VARIABLES

EXTERN_CVAR( Bool, con_scaletext )
EXTERN_CVAR( Int, con_virtualwidth )
EXTERN_CVAR( Int, con_virtualheight )
EXTERN_CVAR( Bool, con_scaletext_usescreenratio ) // [BB]
EXTERN_CVAR( Bool, show_messages )

#endif	// __CHAT_H__
