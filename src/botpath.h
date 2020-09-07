//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2004-2005 Brad Carney
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
// Date created:  2/4/05
//
//
// Filename: botpath.h
//
// Description: 
//
//-----------------------------------------------------------------------------

#ifndef __BOTPATH_H__
#define __BOTPATH_H__

#include "actor.h"
#include "r_defs.h"

//*****************************************************************************
//	DEFINES

#define	BOTPATH_OBSTRUCTED			1
#define	BOTPATH_STAIRS				2
#define	BOTPATH_JUMPABLELEDGE		4
#define	BOTPATH_DAMAGINGSECTOR		8
#define	BOTPATH_DROPOFF				16
#define	BOTPATH_TELEPORT			32
#define	BOTPATH_DOOR				64

//*****************************************************************************
//	FUNCTIONS

bool		BOTPATH_IsPositionBlocked( AActor *pActor, fixed_t DestX, fixed_t DestY );
ULONG		BOTPATH_TryWalk( AActor *pActor, fixed_t StartX, fixed_t StartY, fixed_t StartZ, fixed_t DestX, fixed_t DestY );
void		BOTPATH_LineOpening( line_t *pLine, fixed_t X, fixed_t Y, fixed_t RefX, fixed_t RefY );
sector_t	*BOTPATH_GetDoorSector( void );

#endif	// __BOTPATH_H__
