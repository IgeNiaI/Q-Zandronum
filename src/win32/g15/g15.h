//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2007 Rivecoder
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
// Created:  9/28/07
//
//
// Filename: g15.cpp
//
// Description: Contains definitions and protypes for Skulltag's Logitech G15 applet.
//
//-----------------------------------------------------------------------------

#ifndef __G15_H__
#define __G15_H__

//*****************************************************************************
//	DEFINES

// The G15 software only runs on Windows.
#if defined ( USE_G15 ) && ( defined( WIN32 ) && !defined ( UNIX ) && !defined ( LINUX ) )
	#define G15_ENABLED
#endif

#define G15_LCDWIDTH 160
#define G15_LCDHEIGHT 43

typedef enum
{
	LCDMODE_LOGO,
	LCDMODE_HUD,
	LCDMODE_MESSAGE,

	NUM_LCDMODES

} LCDMODE_e;

//*****************************************************************************
//	PROTOTYPES

void		G15_Construct( void );
void		G15_NextLevel( const char *lump, const char *name );
void		G15_Tick( void );
bool		G15_TryConnect( void );
bool		G15_IsReady( void );
bool		G15_IsDeviceConnected( void );
void		G15_Deconstruct( void );
void		G15_Printf( const char *pszString );
void		G15_ShowLargeFragMessage( const char *name, bool bWeFragged );
#endif

