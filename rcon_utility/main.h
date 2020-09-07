//------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Skulltag RCON Utility Source
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
// Date created: 8/16/08
//
//
// Filename: main.h
//
// Description:
//
//----------------------------------------------------------------------------------------------------------------------------------------------------

#ifndef __MAIN_H__
#define __MAIN_H__

#include "../src/version.h"

#define	RCONCONSOLE_TEXTLENGTH	4096
#define COMPATIBLE_WITH "stock "DOTVERSIONSTR_NOREV" servers"

typedef enum
{
	STATE_WAITING,
	STATE_CONNECTING,
	STATE_CONNECTED

} STATE_e;

typedef struct
{
	char	szName[128];
	char	szAddress[128];
	char	szPassword[128];

} FAVORITE_s;

#define	COLUMN_NAME			0

int				Printf( const char *pszString, ... );
int				Printf_NoTimestamp( const char *pszString, ... );
void			VPrintf( bool bTimestamp, const char *pszString, va_list Parms );
void			MAIN_Print( bool bTimestamp, const char *pszString );

#endif // __MAIN_H__
