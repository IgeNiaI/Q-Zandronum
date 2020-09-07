/*
** teaminfo.h
** Implementation of the TEAMINFO lump.
**
**---------------------------------------------------------------------------
** Copyright 2007-2008 Christopher Westley
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#ifndef __TEAMINFO_H__
#define __TEAMINFO_H__

#define TEAM_None 255

#define MAX_TEAMS 4
//	[CW] When bumping MAX_TEAMS don't forget to update other pieces of code.
//	[CW] The code that needs to be changed is marked with: "[CW] Add to this when bumping 'MAX_TEAMS'."

#include "doomtype.h"
// [BB] New #includes.
#include "doomdata.h"

struct TEAMINFO
{
/* [BB] ST has its own teaminfo code.
	FString		name;
	int			playercolor;
	FString		textcolor;
	int			GetTextColor () const;
	FString		logo;
	int			players;
	int			score;
	int			present;
	int			ties;
*/
	// The name of this team.
	FString		Name;

	// The color of the players on this team.
	LONG		lPlayerColor;

	// [BB] May team members use a custom player color?
	bool		bCustomPlayerColorAllowed;

	// The text color of this team.
	FString		TextColor;
//	int			GetTextColor () const;

	// Text color we print various team messages in.
	ULONG		ulTextColor;

	// Color of the railgun trail made by all team members.
	LONG		lRailColor;

	// Current amount of points this team has.
	LONG		lScore;

	// Icon that appears over a player that posseses this team's "flag".
//	statenum_t	Icon;

	// Amount of time left before this team's "flag" is returned to it's proper place.
	ULONG		ulReturnTicks;

	// Offset for the type of scripts that are run to return this team's "flag".
	ULONG		ulReturnScriptOffset;

	// Number of frags this team has (teamplay deathmatch).
	LONG		lFragCount;

	// Number of combined deaths this team has (teamplay deathmatch).
	LONG		lDeathCount;

	// Number of wins this team has (team LMS).
	LONG		lWinCount;

	// Who is carrying the red flag/skull?
	player_t	*g_pCarrier;

	POS_t		g_Origin;
	bool		g_bTaken;

	FString		SmallFlagHUDIcon;
	FString		SmallSkullHUDIcon;
	FString		LargeFlagHUDIcon;
	FString		LargeSkullHUDIcon;

	// Game mode specific items.
	FString		FlagItem;
	FString		SkullItem;

	FString		WinnerPic;
	FString		LoserPic;
	FString		WinnerTheme;
	FString		LoserTheme;

	bool		bAnnouncedLeadState;

	// This team's player starts.
	TArray<FPlayerStart> TeamStarts;

	// The DoomEdNum of the team's player starts.
	ULONG		ulPlayerStartThingNumber;

	// Keep track of players eligable for assist medals.
	ULONG	g_ulAssistPlayer;
};

extern TArray <TEAMINFO> teams;

extern void TEAMINFO_Init ();
extern void TEAMINFO_ParseTeam ();

// [CW] See 'TEAM_CheckIfValid' in 'team.cpp'.
//extern bool TEAMINFO_IsValidTeam (int team);

#endif
