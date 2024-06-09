// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//      Put all global state variables here.
//
//-----------------------------------------------------------------------------



#include "stringtable.h"
#include "doomstat.h"
#include "c_cvars.h"
#include "i_system.h"
#include "g_level.h"
#include "p_local.h"
// [WS] New includes
#include "sv_commands.h"

int SaveVersion;

// Localizable strings
FStringTable	GStrings;

// Game speed
EGameSpeed		GameSpeed = SPEED_Normal;

// Show developer messages if true.
CVAR (Bool, developer, false, 0)

// [RH] Feature control cvars
CVAR (Bool, var_friction, true, CVAR_SERVERINFO);
CVAR (Bool, var_pushers, true, CVAR_SERVERINFO);

// [WS] Changed CVAR to CUSTOM_CVAR as we need to send clients the updated state of this.
CUSTOM_CVAR (Bool, alwaysapplydmflags, false, CVAR_SERVERINFO)
{
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( gamestate != GS_STARTUP ))
	{
		if (self.GetPastValue() != self)
			SERVER_Printf( "%s changed to: %d\n", self.GetName( ), (bool)self );
		SERVERCOMMANDS_SetGameModeLimits( );
	}
}

CUSTOM_CVAR (Float, teamdamage, 0.f, CVAR_SERVERINFO)
{
	level.teamdamage = self;
}

CUSTOM_CVAR (String, language, "auto", CVAR_ARCHIVE)
{
	SetLanguageIDs ();
	GStrings.LoadStrings (false, false);
	if (level.info != NULL) level.LevelName = level.info->LookupLevelName();
}

// [RH] Network arbitrator
int Net_Arbitrator = 0;

int NextSkill = -1;

int SinglePlayerClass[MAXPLAYERS];

bool ToggleFullscreen;
int BorderTopRefresh;

