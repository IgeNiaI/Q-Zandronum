//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2002 Brad Carney
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
// Filename: medal.h
//
// Description: Contains medal structures and prototypes
//
//-----------------------------------------------------------------------------

#ifndef __MEDAL_H__
#define __MEDAL_H__

#include "doomdef.h"
#include "info.h"

//*****************************************************************************
//	DEFINES

#define	MEDAL_ICON_DURATION			( 3 * TICRATE )

// Same values as defined in AFloatyIcon::States[].
// [Dusk] Now an enumeration.
enum
{
	S_TERMINATORARTIFACT		= 0,
	S_CHAT						= ( S_TERMINATORARTIFACT + 4 ),
	S_INCONSOLE					= ( S_CHAT + 1 ),
	S_ALLY						= ( S_INCONSOLE + 2 ),
	S_WHITEFLAG					= ( S_ALLY + 1 ),
	S_EXCELLENT					= ( S_WHITEFLAG + 6 ),
	S_INCREDIBLE				= ( S_EXCELLENT + 1 ),
	S_IMPRESSIVE				= ( S_INCREDIBLE + 1 ),
	S_MOST_IMPRESSIVE			= ( S_IMPRESSIVE + 1 ),
	S_DOMINATION				= ( S_MOST_IMPRESSIVE + 1 ),
	S_TOTAL_DOMINATION			= ( S_DOMINATION + 1 ),
	S_ACCURACY					= ( S_TOTAL_DOMINATION + 1 ),
	S_PRECISION					= ( S_ACCURACY + 1 ),
	S_VICTORY					= ( S_PRECISION + 1 ),
	S_PERFECT					= ( S_VICTORY + 1 ),
	S_FIRSTFRAG					= ( S_PERFECT + 1 ),
	S_TERMINATION				= ( S_FIRSTFRAG + 1 ),
	S_CAPTURE					= ( S_TERMINATION + 1 ),
	S_TAG						= ( S_CAPTURE + 1 ),
	S_ASSIST					= ( S_TAG + 1 ),
	S_DEFENSE					= ( S_ASSIST + 1 ),
	S_LLAMA						= ( S_DEFENSE + 1 ),
	S_YOUFAILIT					= ( S_LLAMA + 1 ),
	S_YOURSKILLISNOTENOUGH		= ( S_YOUFAILIT + 1 ),
	S_LAG						= ( S_YOURSKILLISNOTENOUGH + 1 ),
	S_FISTING					= ( S_LAG + 1 ),
	S_SPAM						= ( S_FISTING + 1 ),
	S_POSSESSIONARTIFACT		= ( S_SPAM + 1 ),
};

//*****************************************************************************
enum
{
	MEDAL_EXCELLENT,
	MEDAL_INCREDIBLE,

	MEDAL_IMPRESSIVE,
	MEDAL_MOSTIMPRESSIVE,

	MEDAL_DOMINATION,
	MEDAL_TOTALDOMINATION,

	MEDAL_ACCURACY,
	MEDAL_PRECISION,

	MEDAL_YOUFAILIT,
	MEDAL_YOURSKILLISNOTENOUGH,

	MEDAL_LLAMA,
	MEDAL_SPAM,

	MEDAL_VICTORY,
	MEDAL_PERFECT,

	MEDAL_TERMINATION,
	MEDAL_FIRSTFRAG,
	MEDAL_CAPTURE,
	MEDAL_TAG,
	MEDAL_ASSIST,
	MEDAL_DEFENSE,
	MEDAL_FISTING,

	NUM_MEDALS
};


//*****************************************************************************
#define	MEDALQUEUE_DEPTH			NUM_MEDALS

//*****************************************************************************
//	STRUCTURES

typedef struct
{
	// Icon that displays on the screen when this medal is received.
	const char	szLumpName[8];

	// Frame the floaty icon above the player's head is set to.
	USHORT		usFrame;

	// Text that appears below the medal icon when received.
	const char	*szStr;

	// Color that text is displayed in.
	ULONG		ulTextColor;

	// Announcer entry that's played when this medal is triggered.
	const char	szAnnouncerEntry[32];

	// [RC] The "lower" medal that this overrides.
	ULONG		ulLowerMedal;

	// Name of sound to play when this medal type is triggered.
	const char	*szSoundName;

} MEDAL_t;

//*****************************************************************************
typedef struct
{
	// Type of medal in this queue entry.
	ULONG		ulMedal;

	// Amount of time before the medal display in this queue expires.
	ULONG		ulTick;

} MEDALQUEUE_t;

//*****************************************************************************
//	PROTOTYPES

// Standard API.
void	MEDAL_Construct( void );
void	MEDAL_Input( void );
void	MEDAL_Tick( void );
void	MEDAL_Render( void );

void	MEDAL_GiveMedal( ULONG ulPlayer, ULONG ulMedal );
void	MEDAL_RenderAllMedals( LONG lYOffset );
void	MEDAL_RenderAllMedalsFullscreen( player_t *pPlayer );
ULONG	MEDAL_GetDisplayedMedal( ULONG ulPlayer );
void	MEDAL_ClearMedalQueue( ULONG ulPlayer );
void	MEDAL_PlayerDied( ULONG ulPlayer, ULONG ulSourcePlayer );
void	MEDAL_ResetFirstFragAwarded( void );

//*****************************************************************************
//	EXTERNAL CONSOLE VARIABLES

EXTERN_CVAR( Bool, cl_medals )
EXTERN_CVAR( Bool, cl_icons )

#endif
