//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2009 Benjamin Berkels
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
// Filename: gamemode_enums.h
//
// Description: 
//
//-----------------------------------------------------------------------------

#if ( !defined(__GAMEMODE_ENUMS_H__) || defined(GENERATE_ENUM_STRINGS) )

#if (!defined(GENERATE_ENUM_STRINGS))
	#define __GAMEMODE_ENUMS_H__
#endif

#include "EnumToString.h"

//*****************************************************************************
BEGIN_ENUM( GMF )
{
	ENUM_ELEMENT2 ( GMF_COOPERATIVE,		0x00000001 ),
	ENUM_ELEMENT2 ( GMF_DEATHMATCH,			0x00000002 ),
	ENUM_ELEMENT2 ( GMF_TEAMGAME,			0x00000004 ),
	ENUM_ELEMENT2 ( GMF_USEFLAGASTEAMITEM,	0x00000008 ),
	ENUM_ELEMENT2 ( GMF_PLAYERSEARNKILLS,	0x00000010 ),
	ENUM_ELEMENT2 ( GMF_PLAYERSEARNFRAGS,	0x00000020 ),
	ENUM_ELEMENT2 ( GMF_PLAYERSEARNPOINTS,	0x00000040 ),
	ENUM_ELEMENT2 ( GMF_PLAYERSEARNWINS,	0x00000080 ),
	ENUM_ELEMENT2 ( GMF_DONTSPAWNMAPTHINGS,	0x00000100 ),
	ENUM_ELEMENT2 ( GMF_MAPRESETS,			0x00000200 ),
	ENUM_ELEMENT2 ( GMF_DEADSPECTATORS,		0x00000400 ),
	ENUM_ELEMENT2 ( GMF_PLAYERSONTEAMS,		0x00000800 ),
	ENUM_ELEMENT2 ( GMF_USEMAXLIVES,		0x00001000 ),
	ENUM_ELEMENT2 ( GMF_USETEAMITEM,		0x00002000 ),
	ENUM_ELEMENT2 ( GMF_MAPRESET_RESETS_MAPTIME,		0x00004000 ),
}
END_ENUM( GMF )

//*****************************************************************************
BEGIN_ENUM( GAMEMODE_e )
{
	ENUM_ELEMENT ( GAMEMODE_COOPERATIVE ),
	ENUM_ELEMENT ( GAMEMODE_SURVIVAL ),
	ENUM_ELEMENT ( GAMEMODE_INVASION ),
	ENUM_ELEMENT ( GAMEMODE_DEATHMATCH ),
	ENUM_ELEMENT ( GAMEMODE_TEAMPLAY ),
	ENUM_ELEMENT ( GAMEMODE_DUEL ),
	ENUM_ELEMENT ( GAMEMODE_TERMINATOR ),
	ENUM_ELEMENT ( GAMEMODE_LASTMANSTANDING ),
	ENUM_ELEMENT ( GAMEMODE_TEAMLMS ),
	ENUM_ELEMENT ( GAMEMODE_POSSESSION ),
	ENUM_ELEMENT ( GAMEMODE_TEAMPOSSESSION ),
	ENUM_ELEMENT ( GAMEMODE_TEAMGAME ),
	ENUM_ELEMENT ( GAMEMODE_CTF ),
	ENUM_ELEMENT ( GAMEMODE_ONEFLAGCTF ),
	ENUM_ELEMENT ( GAMEMODE_SKULLTAG ),
	ENUM_ELEMENT ( GAMEMODE_DOMINATION ),

	ENUM_ELEMENT ( NUM_GAMEMODES ),

}
END_ENUM( GAMEMODE_e )

#endif // ( !defined(__GAMEMODE_ENUMS_H__) || defined(GENERATE_ENUM_STRINGS) )
