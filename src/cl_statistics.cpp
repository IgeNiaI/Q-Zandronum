//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2003-2007 Brad Carney
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
// Date created:  8/19/07
//
//
// Filename: cl_statistics.cpp
//
// Description: Keeps track of the amount of data sent to and from the server,
// and possibly other related things.
//
//-----------------------------------------------------------------------------

#include "cl_statistics.h"
#include "doomtype.h"
#include "stats.h"
#include "doomdef.h"
#include "doomstat.h"

//*****************************************************************************
//	VARIABLES

// Number of bytes sent/received this tick from the server.
static	LONG				g_lBytesSentThisTick;
static	LONG				g_lBytesReceivedThisTick;

// Total number of bytes sent/received this second.
static	LONG				g_lBytesSentThisSecond;
static	LONG				g_lBytesReceivedThisSecond;

// Total number of bytes sent/received the previous second.
static	LONG				g_lBytesSentLastSecond;
static	LONG				g_lBytesReceivedLastSecond;

// This is the maximum number of bytes per second we've send/received.
static	LONG				g_lMaxBytesSentPerSecond;
static	LONG				g_lMaxBytesReceivedPerSecond;

//*****************************************************************************
//	FUNCTIONS

void CLIENTSTATISTICS_Construct( void )
{
	g_lBytesSentThisTick = 0;
	g_lBytesReceivedThisTick = 0;

	g_lBytesSentThisSecond = 0;
	g_lBytesReceivedThisSecond = 0;

	g_lBytesSentLastSecond = 0;
	g_lBytesReceivedLastSecond = 0;

	g_lMaxBytesSentPerSecond = 0;
	g_lMaxBytesReceivedPerSecond = 0;
}

//*****************************************************************************
//
void CLIENTSTATISTICS_Tick( void )
{
	// Add to the number of bytes sent/received this second.
	g_lBytesSentThisSecond += g_lBytesSentThisTick;
	g_lBytesReceivedThisSecond += g_lBytesReceivedThisTick;
	g_lBytesSentThisTick = 0;
	g_lBytesReceivedThisTick = 0;

	// Every second, update the number of bytes sent last second with the number of bytes
	// sent this second, and then reset the number of bytes sent this second.
	if (( gametic % TICRATE ) == 0 )
	{
		g_lBytesSentLastSecond = g_lBytesSentThisSecond;
		g_lBytesReceivedLastSecond = g_lBytesReceivedThisSecond;

		g_lBytesSentThisSecond = 0;
		g_lBytesReceivedThisSecond = 0;

		if ( g_lMaxBytesSentPerSecond < g_lBytesSentLastSecond )
			g_lMaxBytesSentPerSecond = g_lBytesSentLastSecond;
		if ( g_lMaxBytesReceivedPerSecond < g_lBytesReceivedLastSecond )
			g_lMaxBytesReceivedPerSecond = g_lBytesReceivedLastSecond;
	}
}

//*****************************************************************************
//
void CLIENTSTATISTICS_AddToBytesSent( ULONG ulBytes )
{
	g_lBytesSentThisTick += ulBytes;
}

//*****************************************************************************
//
void CLIENTSTATISTICS_AddToBytesReceived( ULONG ulBytes )
{
	g_lBytesReceivedThisTick += ulBytes;
}

//*****************************************************************************
//	STATISTICS

ADD_STAT( nettraffic )
{
	FString	Out;

	Out.Format( "In: %5d/%5d/%5d        Out: %5d/%5d/%5d", 
		static_cast<int> (g_lBytesReceivedThisTick),
		static_cast<int> (g_lBytesReceivedLastSecond),
		static_cast<int> (g_lMaxBytesReceivedPerSecond),
		static_cast<int> (g_lBytesSentThisTick),
		static_cast<int> (g_lBytesSentLastSecond),
		static_cast<int> (g_lMaxBytesSentPerSecond) );

	return ( Out );
}
