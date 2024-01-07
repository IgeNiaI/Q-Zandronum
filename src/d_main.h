// Emacs style mode select	 -*- C++ -*- 
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
//		System specific interface stuff.
//
//-----------------------------------------------------------------------------


#ifndef __D_MAIN__
#define __D_MAIN__

#include "doomtype.h"
#include "gametype.h"

struct event_t;

//
// D_DoomMain()
// Not a globally visible function, just included for source reference,
// calls all startup code, parses command line options.
// If not overrided by user input, calls N_AdvanceDemo.
//

struct CRestartException
{
	char dummy;
};

void D_DoomMain (void);
void D_PrintStartup (void);


void D_Display ();


//
// BASE LEVEL
//
void D_PageTicker (void);
void D_PageDrawer (void);
void D_AdvanceDemo (void);
void D_StartTitle (void);
bool D_AddFile (TArray<FString> &wadfiles, const char *file, bool check = true, int position = -1);


// [RH] Set this to something to draw an icon during the next screen refresh.
extern const char *D_DrawIcon;


struct WadStuff
{
	WadStuff() : Type(0) {}

	FString Path;
	FString Name;
	int Type;
};

struct FIWADInfo
{
	FString Name;			// Title banner text for this IWAD
	FString Autoname;		// Name of autoload ini section for this IWAD
	FString Configname;		// Name of config section for this IWAD
	FString Required;		// Requires another IWAD
	DWORD FgColor;			// Foreground color for title banner
	DWORD BkColor;			// Background color for title banner
	EGameType gametype;		// which game are we playing?
	FString MapInfo;		// Base mapinfo to load
	TArray<FString> Load;	// Wads to be loaded with this one.
	TArray<FString> Lumps;	// Lump names for identification
	int flags;
	int preload;

	FIWADInfo() { flags = 0; preload = -1; FgColor = 0; BkColor= 0xc0c0c0; gametype = GAME_Doom; }
};

struct FStartupInfo
{
	FString Name;
	DWORD FgColor;			// Foreground color for title banner
	DWORD BkColor;			// Background color for title banner
	FString Song;
	int Type;
	FString DiscordAppId = nullptr;
	enum
	{
		DefaultStartup,
		DoomStartup,
		HereticStartup,
		HexenStartup,
		StrifeStartup,
	};

};

extern FStartupInfo DoomStartupInfo;

//==========================================================================
//
// IWAD identifier class
//
//==========================================================================

struct FIWadManager
{
private:
	TArray<FIWADInfo> mIWads;
	TArray<FString> mIWadNames;
	TArray<int> mLumpsFound;

	void ParseIWadInfo(const char *fn, const char *data, int datasize);
	void ParseIWadInfos(const char *fn);
	void ClearChecks();
	void CheckLumpName(const char *name);
	int GetIWadInfo();
	int ScanIWAD (const char *iwad);
	int CheckIWAD (const char *doomwaddir, WadStuff *wads);
	int IdentifyVersion (TArray<FString> &wadfiles, const char *iwad, const char *zdoom_wad);
public:
	const FIWADInfo *FindIWAD(TArray<FString> &wadfiles, const char *iwad, const char *basewad);

	// [RC] Checks if a directory contains IWADs. Used by "no IWAD" setup screen in I_system.cpp.
	bool DoesDirectoryHaveIWADs( const char *pszPath );
};


#endif
