//-----------------------------------------------------------------------------
//
// Unlagged Source
// Copyright (C) 2010 "Spleen"
// Copyright (C) 2010-2012 Skulltag Development Team
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
// Filename: unlagged.h
//
// Description: Contains variables and routines related to the backwards
// reconciliation.
//
//-----------------------------------------------------------------------------

#ifndef __UNLAGGED_H__
#define __UNLAGGED_H__

#include "d_player.h"
#include "doomdef.h"
#include "p_trace.h"

void	UNLAGGED_Tick( void );
int		UNLAGGED_Gametic( player_t *player );
int		UNLAGGED_Reconcile( AActor *actor );
void	UNLAGGED_ReconcileTick( AActor *actor, int Tick );
int		UNLAGGED_GetDelayTics( );
void	UNLAGGED_Restore( AActor *actor );
void	UNLAGGED_RecordPlayer( player_t *player, int unlaggedIndex );
void	UNLAGGED_ResetPlayer( player_t *player );
void	UNLAGGED_RecordActor( AUnlaggedActor* unlaggedActor, int unlaggedIndex );
void	UNLAGGED_ResetActor( AUnlaggedActor* unlaggedActor );
void	UNLAGGED_RecordSectors( int unlaggedIndex );
void	UNLAGGED_RecordPolyobj( int unlaggedIndex );
bool	UNLAGGED_DrawRailClientside ( AActor *attacker );
void	UNLAGGED_GetHitOffset ( const AActor *attacker, const FTraceResults &trace, TVector3<fixed_t> &hitOffset );
bool	UNLAGGED_IsReconciled ( );
player_t *UNLAGGED_GetReconciledPlayer ( );
void	UNLAGGED_AddReconciliationBlocker ( );
void	UNLAGGED_RemoveReconciliationBlocker ( );
void	UNLAGGED_SpawnDebugActors ( player_t *player, bool server );
void	UNLAGGED_UnlagAndReplicateThing ( AActor *source, AActor *thing, bool bSkipOwner, bool bNoUnlagged, bool bUnlagDeath );
void	UNLAGGED_UnlagAndReplicateMissile ( AActor *source, AActor *missile, bool bSkipOwner, bool bNoUnlagged, bool bUnlagDeath );

#endif // __UNLAGGED_H__
