//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2004-2006 Brad Carney
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
// Date created:  10/6/06
//
//
// Filename: possession.h
//
// Description: Contains possession structures and prototypes
//
//-----------------------------------------------------------------------------

#ifndef __POSSESSION_H__
#define __POSSESSION_H__

#include "d_player.h"
#include "doomtype.h"

//*****************************************************************************
//	DEFINES

typedef enum
{
	PSNS_WAITINGFORPLAYERS,
	PSNS_COUNTDOWN,
	PSNS_INPROGRESS,
	PSNS_ARTIFACTHELD,
	PSNS_PRENEXTROUNDCOUNTDOWN,
	PSNS_NEXTROUNDCOUNTDOWN,
	PSNS_HOLDERSCORED,

} PSNSTATE_e;

//*****************************************************************************
//	PROTOTYPES

void		POSSESSION_Construct( void );
void		POSSESSION_Tick( void );
void		POSSESSION_Render( void );

void		POSSESSION_StartCountdown( ULONG ulTicks );
void		POSSESSION_StartNextRoundCountdown( ULONG ulTicks );
void		POSSESSION_DoFight( void );
void		POSSESSION_ScorePossessionPoint( player_t *pPlayer );
void		POSSESSION_ArtifactPickedUp( player_t *pPlayer, ULONG ulTicks );
void		POSSESSION_ArtifactDropped( void );
bool		POSSESSION_ShouldRespawnArtifact( void );
void		POSSESSION_TimeExpired( void );

// Access functions.
ULONG		POSSESSION_GetCountdownTicks( void );
void		POSSESSION_SetCountdownTicks( ULONG ulTicks );

PSNSTATE_e	POSSESSION_GetState( void );
void		POSSESSION_SetState( PSNSTATE_e State );

ULONG		POSSESSION_GetArtifactHoldTicks( void );
void		POSSESSION_SetArtifactHoldTicks( ULONG ulTicks );

//*****************************************************************************
//	EXTERNAL CONSOLE VARIABLES

EXTERN_CVAR( Int, sv_possessioncountdowntime )
EXTERN_CVAR( Int, sv_possessionholdtime )
EXTERN_CVAR( Bool, sv_usemapsettingspossessionholdtime )

#endif	// __POSSESSION_H__
