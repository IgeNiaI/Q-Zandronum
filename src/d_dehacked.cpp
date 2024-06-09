/*
** d_dehacked.cpp
** Parses dehacked/bex patches and changes game structures accordingly
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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
** Much of this file is fudging code to compensate for the fact that most of
** what could be changed with Dehacked is no longer in the same state it was
** in as of Doom 1.9. There is a lump in zdoom.wad (DEHSUPP) that stores most
** of the lookup tables used so that they need not be loaded all the time.
** Also, their total size is less in the lump than when they were part of the
** executable, so it saves space on disk as well as in memory.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "doomtype.h"
#include "templates.h"
#include "doomstat.h"
#include "info.h"
#include "d_dehacked.h"
#include "s_sound.h"
#include "g_level.h"
#include "cmdlib.h"
#include "gstrings.h"
#include "m_alloc.h"
#include "m_misc.h"
#include "w_wad.h"
#include "d_player.h"
#include "r_state.h"
#include "gi.h"
#include "c_dispatch.h"
#include "decallib.h"
#include "v_palette.h"
#include "a_sharedglobal.h"
#include "thingdef/thingdef.h"
#include "thingdef/thingdef_exp.h"
#include "vectors.h"
#include "dobject.h"
#include "r_data/r_translate.h"
#include "sc_man.h"
#include "i_system.h"
#include "doomerrors.h"
#include "p_effect.h"
#include "farchive.h"
// [BC] New #includes.
#include "cl_demo.h"
#include "network.h"
#include "sv_commands.h"

// [SO] Just the way Randy said to do it :)
// [RH] Made this CVAR_SERVERINFO
CVAR (Int, infighting, 0, CVAR_SERVERINFO)

static bool LoadDehSupp ();
static void UnloadDehSupp ();


// This is a list of all the action functions used by each of Doom's states.
static TArray<PSymbol *> Actions;

// These are the original heights of every Doom 2 thing. They are used if a patch
// specifies that a thing should be hanging from the ceiling but doesn't specify
// a height for the thing, since these are the heights it probably wants.
static TArray<int> OrgHeights;

// DeHackEd made the erroneous assumption that if a state didn't appear in
// Doom with an action function, then it was incorrect to assign it one.
// This is a list of the states that had action functions, so we can figure
// out where in the original list of states a DeHackEd codepointer is.
// (DeHackEd might also have done this for compatibility between Doom
// versions, because states could move around, but actions would never
// disappear, but that doesn't explain why frame patches specify an exact
// state rather than a code pointer.)
static TArray<int> CodePConv;

// [TP] Which external patches are currently applied?
static TArray<FString> g_LoadedDehFiles;

// Sprite names in the order Doom originally had them.
struct DEHSprName
{
	char c[5];
};
static TArray<DEHSprName> OrgSprNames;

struct StateMapper
{
	FState *State;
	int StateSpan;
	const PClass *Owner;
	bool OwnerIsPickup;
};

static TArray<StateMapper> StateMap;

// Sound equivalences. When a patch tries to change a sound,
// use these sound names.
static TArray<FSoundID> SoundMap;

// Names of different actor types, in original Doom 2 order
static TArray<const PClass *> InfoNames;

// bit flags for PatchThing (a .bex extension):
struct BitName
{
	char Name[20];
	BYTE Bit;
	BYTE WhichFlags;
};

static TArray<BitName> BitNames;

// Render styles
struct StyleName
{
	char Name[20];
	BYTE Num;
};

static TArray<StyleName> StyleNames;

static TArray<const PClass *> AmmoNames;
static TArray<const PClass *> WeaponNames;

// DeHackEd trickery to support MBF-style parameters
// List of states that are hacked to use a codepointer
struct MBFParamState
{
	FState *state;
	int pointer;
};
static TArray<MBFParamState> MBFParamStates;
// Data on how to correctly modify the codepointers
struct CodePointerAlias
{
	char name[20];
	char alias[20];
	BYTE params;
};
static TArray<CodePointerAlias> MBFCodePointers;

struct AmmoPerAttack
{
	actionf_p func;
	int ammocount;
};

DECLARE_ACTION(A_Punch)
DECLARE_ACTION(A_FirePistol)
DECLARE_ACTION(A_FireShotgun)
DECLARE_ACTION(A_FireShotgun2)
DECLARE_ACTION(A_FireCGun)
DECLARE_ACTION(A_FireMissile)
DECLARE_ACTION_PARAMS(A_Saw)
DECLARE_ACTION(A_FirePlasma)
DECLARE_ACTION(A_FireBFG)
DECLARE_ACTION(A_FireOldBFG)
DECLARE_ACTION_PARAMS(A_FireRailgun) // [TP/BB] Added params

// Default ammo use of the various weapon attacks
static AmmoPerAttack AmmoPerAttacks[] = {
	{ AF_A_Punch, 0},
	{ AF_A_FirePistol, 1},
	{ AF_A_FireShotgun, 1}, 
	{ AF_A_FireShotgun2, 2},
	{ AF_A_FireCGun, 1},
	{ AF_A_FireMissile, 1},
	{ AFP_A_Saw, 0},
	{ AF_A_FirePlasma, 1},
	{ AF_A_FireBFG, -1},	// uses deh.BFGCells
	{ AF_A_FireOldBFG, 1},
	{ AFP_A_FireRailgun, 1}, // [TP/BB] Added params
	{ NULL, 0}
};


// Miscellaneous info that used to be constant
DehInfo deh =
{
	100,	// .StartHealth
	 50,	// .StartBullets
	100,	// .MaxHealth
	200,	// .MaxArmor
	  1,	// .GreenAC
	  2,	// .BlueAC
	200,	// .MaxSoulsphere
	100,	// .SoulsphereHealth
	200,	// .MegasphereHealth
	100,	// .GodHealth
	200,	// .FAArmor
	  2,	// .FAAC
	200,	// .KFAArmor
	  2,	// .KFAAC
	"PLAY",	// Name of player sprite
	255,	// Rocket explosion style, 255=use cvar
	FRACUNIT*2/3,		// Rocket explosion alpha
	false,	// .NoAutofreeze
	40,		// BFG cells per shot
};

// Doom identified pickup items by their sprites. ZDoom prefers to use their
// class type to identify them instead. To support the traditional Doom
// behavior, for every thing touched by dehacked that has the MF_PICKUP flag,
// a new subclass of ADehackedPickup will be created with properties copied
// from the original actor's defaults. The original actor is then changed to
// spawn the new class.

IMPLEMENT_POINTY_CLASS (ADehackedPickup)
 DECLARE_POINTER (RealPickup)
END_POINTERS

TArray<PClass *> TouchedActors;

char *UnchangedSpriteNames;
int NumUnchangedSprites;

// Sprite<->Class map for ADehackedPickup::DetermineType
static struct DehSpriteMap
{
	char Sprite[5];
	const char *ClassName;
}
DehSpriteMappings[] =
{
	{ "AMMO", "ClipBox" },
	{ "ARM1", "GreenArmor" },
	{ "ARM2", "BlueArmor" },
	{ "BFUG", "BFG9000" },
	{ "BKEY", "BlueCard" },
	{ "BON1", "HealthBonus" },
	{ "BON2", "ArmorBonus" },
	{ "BPAK", "Backpack" },
	{ "BROK", "RocketBox" },
	{ "BSKU", "BlueSkull" },
	{ "CELL", "Cell" },
	{ "CELP", "CellPack" },
	{ "CLIP", "Clip" },
	{ "CSAW", "Chainsaw" },
	{ "LAUN", "RocketLauncher" },
	{ "MEDI", "Medikit" },
	{ "MEGA", "Megasphere" },
	{ "MGUN", "Chaingun" },
	{ "PINS", "BlurSphere" },
	{ "PINV", "InvulnerabilitySphere" },
	{ "PLAS", "PlasmaRifle" },
	{ "PMAP", "Allmap" },
	{ "PSTR", "Berserk" },
	{ "PVIS", "Infrared" },
	{ "RKEY", "RedCard" },
	{ "ROCK", "RocketAmmo" },
	{ "RSKU", "RedSkull" },
	{ "SBOX", "ShellBox" },
	{ "SGN2", "SuperShotgun" },
	{ "SHEL", "Shell" },
	{ "SHOT", "Shotgun" },
	{ "SOUL", "Soulsphere" },
	{ "STIM", "Stimpack" },
	{ "SUIT", "RadSuit" },
	{ "YKEY", "YellowCard" },
	{ "YSKU", "YellowSkull" }
};

#define LINESIZE 2048

#define CHECKKEY(a,b)		if (!stricmp (Line1, (a))) (b) = atoi(Line2);

static char *PatchFile, *PatchPt, *PatchName;
static int PatchSize;
static char *Line1, *Line2;
static int	 dversion, pversion;
static bool  including, includenotext;

static const char *unknown_str = "Unknown key %s encountered in %s %d.\n";

static FStringTable *EnglishStrings;

// This is an offset to be used for computing the text stuff.
// Straight from the DeHackEd source which was
// Written by Greg Lewis, gregl@umich.edu.
static int toff[] = {129044, 129044, 129044, 129284, 129380};

struct Key {
	const char *name;
	ptrdiff_t offset;
};

static int PatchThing (int);
static int PatchSound (int);
static int PatchFrame (int);
static int PatchSprite (int);
static int PatchAmmo (int);
static int PatchWeapon (int);
static int PatchPointer (int);
static int PatchCheats (int);
static int PatchMisc (int);
static int PatchText (int);
static int PatchStrings (int);
static int PatchPars (int);
static int PatchCodePtrs (int);
static int PatchMusic (int);
static int DoInclude (int);
static bool DoDehPatch();

static const struct {
	const char *name;
	int (*func)(int);
} Modes[] = {
	// These appear in .deh and .bex files
	{ "Thing",		PatchThing },
	{ "Sound",		PatchSound },
	{ "Frame",		PatchFrame },
	{ "Sprite",		PatchSprite },
	{ "Ammo",		PatchAmmo },
	{ "Weapon",		PatchWeapon },
	{ "Pointer",	PatchPointer },
	{ "Cheat",		PatchCheats },
	{ "Misc",		PatchMisc },
	{ "Text",		PatchText },
	// These appear in .bex files
	{ "include",	DoInclude },
	{ "[STRINGS]",	PatchStrings },
	{ "[PARS]",		PatchPars },
	{ "[CODEPTR]",	PatchCodePtrs },
	{ "[MUSIC]",	PatchMusic },
	{ NULL, NULL },
};

static int HandleMode (const char *mode, int num);
static bool HandleKey (const struct Key *keys, void *structure, const char *key, int value);
static bool ReadChars (char **stuff, int size);
static char *igets (void);
static int GetLine (void);

static void PushTouchedActor(PClass *cls)
{
	for(unsigned i = 0; i < TouchedActors.Size(); i++)
	{
		if (TouchedActors[i] == cls) return;
	}
	TouchedActors.Push(cls);
}


static int HandleMode (const char *mode, int num)
{
	int i = 0;
	while (Modes[i].name && stricmp (Modes[i].name, mode))
		i++;

	if (Modes[i].name)
		return Modes[i].func (num);

	// Handle unknown or unimplemented data
	Printf ("Unknown chunk %s encountered. Skipping.\n", mode);
	do
		i = GetLine ();
	while (i == 1);

	return i;
}

static bool HandleKey (const struct Key *keys, void *structure, const char *key, int value)
{
	while (keys->name && stricmp (keys->name, key))
		keys++;

	if (keys->name) {
		*((int *)(((BYTE *)structure) + keys->offset)) = value;
		return false;
	}

	return true;
}

static int FindSprite (const char *sprname)
{
	int i;
	DWORD nameint = *((DWORD *)sprname);

	for (i = 0; i < NumUnchangedSprites; ++i)
	{
		if (*((DWORD *)&UnchangedSpriteNames[i*4]) == nameint)
		{
			return i;
		}
	}
	return -1;
}

static FState *FindState (int statenum)
{
	int stateacc;
	unsigned i;

	if (statenum == 0)
		return NULL;

	for (i = 0, stateacc = 1; i < StateMap.Size(); i++)
	{
		if (stateacc <= statenum && stateacc + StateMap[i].StateSpan > statenum)
		{
			if (StateMap[i].State != NULL)
			{
				if (StateMap[i].OwnerIsPickup)
				{
					PushTouchedActor(const_cast<PClass *>(StateMap[i].Owner));
				}
				return StateMap[i].State + statenum - stateacc;
			}
			else return NULL;
		}
		stateacc += StateMap[i].StateSpan;
	}
	return NULL;
}

int FindStyle (const char *namestr)
{
	for(unsigned i = 0; i < StyleNames.Size(); i++)
	{
		if (!stricmp(StyleNames[i].Name, namestr)) return StyleNames[i].Num;
	}
	DPrintf("Unknown render style %s\n", namestr);
	return -1;
}

static bool ReadChars (char **stuff, int size)
{
	char *str = *stuff;

	if (!size) {
		*str = 0;
		return true;
	}

	do {
		// Ignore carriage returns
		if (*PatchPt != '\r')
			*str++ = *PatchPt;
		else
			size++;

		PatchPt++;
	} while (--size && *PatchPt != 0);

	*str = 0;
	return true;
}

static void ReplaceSpecialChars (char *str)
{
	char *p = str, c;
	int i;

	while ( (c = *p++) ) {
		if (c != '\\') {
			*str++ = c;
		} else {
			switch (*p) {
				case 'n':
				case 'N':
					*str++ = '\n';
					break;
				case 't':
				case 'T':
					*str++ = '\t';
					break;
				case 'r':
				case 'R':
					*str++ = '\r';
					break;
				case 'x':
				case 'X':
					c = 0;
					p++;
					for (i = 0; i < 2; i++) {
						c <<= 4;
						if (*p >= '0' && *p <= '9')
							c += *p-'0';
						else if (*p >= 'a' && *p <= 'f')
							c += 10 + *p-'a';
						else if (*p >= 'A' && *p <= 'F')
							c += 10 + *p-'A';
						else
							break;
						p++;
					}
					*str++ = c;
					break;
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
					c = 0;
					for (i = 0; i < 3; i++) {
						c <<= 3;
						if (*p >= '0' && *p <= '7')
							c += *p-'0';
						else
							break;
						p++;
					}
					*str++ = c;
					break;
				default:
					*str++ = *p;
					break;
			}
			p++;
		}
	}
	*str = 0;
}

static char *skipwhite (char *str)
{
	if (str)
		while (*str && isspace(*str))
			str++;
	return str;
}

static void stripwhite (char *str)
{
	char *end = str + strlen(str) - 1;

	while (end >= str && isspace(*end))
		end--;

	end[1] = '\0';
}

static char *igets (void)
{
	char *line;

	if (*PatchPt == '\0' || PatchPt >= PatchFile + PatchSize )
		return NULL;

	line = PatchPt;

	while (*PatchPt != '\n' && *PatchPt != '\0')
		PatchPt++;

	if (*PatchPt == '\n')
		*PatchPt++ = 0;

	return line;
}

static int GetLine (void)
{
	char *line, *line2;

	do {
		while ( (line = igets ()) )
			if (line[0] != '#')		// Skip comment lines
				break;

		if (!line)
			return 0;

		Line1 = skipwhite (line);
	} while (Line1 && *Line1 == 0);	// Loop until we get a line with
									// more than just whitespace.
	line = strchr (Line1, '=');

	if (line) {					// We have an '=' in the input line
		line2 = line;
		while (--line2 >= Line1)
			if (*line2 > ' ')
				break;

		if (line2 < Line1)
			return 0;			// Nothing before '='

		*(line2 + 1) = 0;

		line++;
		while (*line && *line <= ' ')
			line++;

		if (*line == 0)
			return 0;			// Nothing after '='

		Line2 = line;

		return 1;
	} else {					// No '=' in input line
		line = Line1 + 1;
		while (*line > ' ')
			line++;				// Get beyond first word

		*line++ = 0;
		while (*line && *line <= ' ')
			line++;				// Skip white space

		//.bex files don't have this restriction
		//if (*line == 0)
		//	return 0;			// No second word

		Line2 = line;

		return 2;
	}
}

// This enum must be in sync with the Aliases array in DEHSUPP.
enum MBFCodePointers
{
	// Die and Detonate are not in this list because these codepointers have
	// no dehacked arguments and therefore do not need special handling.
	// NailBomb has no argument but is implemented as new parameters for A_Explode.
	MBF_Mushroom,	// misc1 = vrange (arg +3), misc2 = hrange (arg+4)
	MBF_Spawn,		// misc1 = type (arg +0), misc2 = Z-pos (arg +2)
	MBF_Turn,		// misc1 = angle (in degrees) (arg +0 but factor in current actor angle too)
	MBF_Face,		// misc1 = angle (in degrees) (arg +0)
	MBF_Scratch,	// misc1 = damage, misc 2 = sound
	MBF_PlaySound,	// misc1 = sound, misc2 = attenuation none (true) or normal (false)
	MBF_RandomJump,	// misc1 = state, misc2 = probability
	MBF_LineEffect,	// misc1 = Boom linedef type, misc2 = sector tag
	SMMU_NailBomb,	// No misc, but it's basically A_Explode with an added effect
};

int PrepareStateParameters(FState * state, int numparams, const PClass *cls);// Should probably be in a .h file.

// Hacks the parameter list for the given state so as to convert MBF-args (misc1 and misc2) into real args.

void SetDehParams(FState * state, int codepointer)
{
	int value1 = state->GetMisc1();
	int value2 = state->GetMisc2();
	if (!(value1|value2)) return;
	
	// Fakey fake script position thingamajig. Because NULL cannot be used instead.
	// Even if the lump was parsed by an FScanner, there would hardly be a way to
	// identify which line is troublesome.
	FScriptPosition * pos = new FScriptPosition(FString("DEHACKED"), 0);
	
	// Let's identify the codepointer we're dealing with.
	PSymbolActionFunction * sym; PSymbol * s;	
	s = RUNTIME_CLASS(AInventory)->Symbols.FindSymbol(FName(MBFCodePointers[codepointer].name), true);
	if (!s || s->SymbolType != SYM_ActionFunction) return;
	sym = static_cast<PSymbolActionFunction*>(s);


	// Bleargh! This will all have to be redone once scripting works

	// Not sure exactly why the index for a state is greater by one point than the index for a symbol.
	DPrintf("SetDehParams: Paramindex is %d, default is %d.\n", 
		state->ParameterIndex-1, sym->defaultparameterindex);
	if (state->ParameterIndex-1 == sym->defaultparameterindex)
	{
		int a = PrepareStateParameters(state, MBFCodePointers[codepointer].params+1, 
			FState::StaticFindStateOwner(state)) -1;
		int b = sym->defaultparameterindex;
		//		StateParams.Copy(a, b, MBFParams[codepointer]);
		// Meh, function doesn't work. For some reason it resets the paramindex to the default value.
		// For instance, a dehacked Commander Keen calling A_Explode would result in a crash as
		// ACTION_PARAM_INT(damage, 0) would properly evaluate at paramindex 1377, but then 
		// ACTION_PARAM_INT(distance, 1) would improperly evaluate at paramindex 148! Now I'm not sure
		// whether it's a genuine problem or working as intended and merely not appropriate for the
		// task at hand here. So rather than modify it, I use a simple for loop of Set()s and Get()s,
		// with a small modification to Set() that I know will have no repercussion anywhere else.
		for (int i = 0; i<MBFCodePointers[codepointer].params; i++)
		{
			StateParams.Set(a+i, StateParams.Get(b+i), true);
		}
		DPrintf("New paramindex is %d.\n", state->ParameterIndex-1);
	}
	int ParamIndex = state->ParameterIndex - 1;

	switch (codepointer)
	{
	case MBF_Mushroom:
		StateParams.Set(ParamIndex+2, new FxConstant(1, *pos)); // Flag
		// NOTE: Do not convert to float here because it will lose precision. It must be double.
		if (value1) StateParams.Set(ParamIndex+3, new FxConstant(value1/65536., *pos)); // vrange
		if (value2) StateParams.Set(ParamIndex+4, new FxConstant(value2/65536., *pos)); // hrange
		break;
	case MBF_Spawn:
		if (InfoNames[value1-1] == NULL)
		{
			I_Error("No class found for dehackednum %d!\n", value1+1);
			return;
		}
		StateParams.Set(ParamIndex+0, new FxConstant(InfoNames[value1-1], *pos));	// type
		StateParams.Set(ParamIndex+2, new FxConstant(value2, *pos));				// height
		break;
	case MBF_Turn:
		// Intentional fall through. I tried something more complicated by creating an
		// FxExpression that corresponded to "variable angle + angle" so as to use A_SetAngle
		// as well, but it became an overcomplicated mess that didn't even work as I had to
		// create a compile context as well and couldn't get it right.
	case MBF_Face:
		StateParams.Set(ParamIndex+0, new FxConstant(value1, *pos)); // angle
		break;
	case MBF_Scratch:	// misc1 = damage, misc 2 = sound
		StateParams.Set(ParamIndex+0, new FxConstant(value1, *pos));							// damage
		if (value2) StateParams.Set(ParamIndex+1, new FxConstant(SoundMap[value2-1], *pos));	// hit sound
		break;
	case MBF_PlaySound:
		StateParams.Set(ParamIndex+0, new FxConstant(SoundMap[value1-1], *pos));				// soundid
		StateParams.Set(ParamIndex+1, new FxConstant(CHAN_BODY, *pos));							// channel
		StateParams.Set(ParamIndex+2, new FxConstant(1.0, *pos));								// volume
		StateParams.Set(ParamIndex+3, new FxConstant(false, *pos));								// looping
		StateParams.Set(ParamIndex+4, new FxConstant((value2 ? ATTN_NONE : ATTN_NORM), *pos));	// attenuation
		break;
	case MBF_RandomJump:
		StateParams.Set(ParamIndex+0, new FxConstant(2, *pos));					// count
		StateParams.Set(ParamIndex+1, new FxConstant(value2, *pos));			// maxchance
		StateParams.Set(ParamIndex+2, new FxConstant(FindState(value1), *pos));	// jumpto
		break;
	case MBF_LineEffect:
		// This is the second MBF codepointer that couldn't be translated easily.
		// Calling P_TranslateLineDef() here was a simple matter, as was adding an
		// extra parameter to A_CallSpecial so as to replicate the LINEDONE stuff,
		// but unfortunately DEHACKED lumps are processed before the map translation
		// arrays are initialized so this didn't work.
		StateParams.Set(ParamIndex+0, new FxConstant(value1, *pos));	// special
		StateParams.Set(ParamIndex+1, new FxConstant(value2, *pos));	// tag
		break;
	case SMMU_NailBomb:
		// That one does not actually have MBF-style parameters. But since
		// we're aliasing it to an extension of A_Explode...
		StateParams.Set(ParamIndex+5, new FxConstant(30, *pos));	// nails
		StateParams.Set(ParamIndex+6, new FxConstant(10, *pos));	// naildamage
		break;
	default:
		// This simply should not happen.
		Printf("Unmanaged dehacked codepointer alias num %i\n", codepointer);
	}
}

static int PatchThing (int thingy)
{
	enum
	{
		// Boom flags
		MF_TRANSLATION	= 0x0c000000,	// if 0x4 0x8 or 0xc, use a translation
		MF_TRANSSHIFT	= 26,			// table for player colormaps
		// A couple of Boom flags that don't exist in ZDoom
		MF_SLIDE		= 0x00002000,	// Player: keep info about sliding along walls.
		MF_TRANSLUCENT	= 0x80000000,	// Translucent sprite?
		// MBF flags: TOUCHY is remapped to flags6, FRIEND is turned into FRIENDLY,
		// and finally BOUNCES is replaced by bouncetypes with the BOUNCES_MBF bit.
		MF_TOUCHY		= 0x10000000,	// killough 11/98: dies when solids touch it
		MF_BOUNCES		= 0x20000000,	// killough 7/11/98: for beta BFG fireballs
		MF_FRIEND		= 0x40000000,	// killough 7/18/98: friendly monsters
	};

	int result;
	AActor *info;
	BYTE dummy[sizeof(AActor)];
	bool hadHeight = false;
	bool hadTranslucency = false;
	bool hadStyle = false;
	FStateDefinitions statedef;
	bool patchedStates = false;
	int oldflags;
	const PClass *type;
	SWORD *ednum, dummyed;

	type = NULL;
	info = (AActor *)&dummy;
	ednum = &dummyed;
	if (thingy > (int)InfoNames.Size() || thingy <= 0)
	{
		Printf ("Thing %d out of range.\n", thingy);
	}
	else
	{
		DPrintf ("Thing %d\n", thingy);
		if (thingy > 0)
		{
			type = InfoNames[thingy - 1];
			if (type == NULL)
			{
				info = (AActor *)&dummy;
				ednum = &dummyed;
				// An error for the name has already been printed while loading DEHSUPP.
				Printf ("Could not find thing %d\n", thingy);
			}
			else
			{
				info = GetDefaultByType (type);
				ednum = &type->ActorInfo->DoomEdNum;
			}
		}
	}

	oldflags = info->flags;

	while ((result = GetLine ()) == 1)
	{
		char *endptr;
		unsigned long val = strtoul (Line2, &endptr, 10);
		size_t linelen = strlen (Line1);

		if (linelen == 10 && stricmp (Line1, "Hit points") == 0)
		{
			info->health = val;
		}
		else if (linelen == 13 && stricmp (Line1, "Reaction time") == 0)
		{
			info->reactiontime = val;
		}
		else if (linelen == 11 && stricmp (Line1, "Pain chance") == 0)
		{
			info->PainChance = (SWORD)val;
		}
		else if (linelen == 12 && stricmp (Line1, "Translucency") == 0)
		{
			info->alpha = val;
			info->RenderStyle = STYLE_Translucent;
			hadTranslucency = true;
			hadStyle = true;
		}
		else if (linelen == 6 && stricmp (Line1, "Height") == 0)
		{
			info->height = val;
			info->projectilepassheight = 0;	// needs to be disabled
			hadHeight = true;
		}
		else if (linelen == 14 && stricmp (Line1, "Missile damage") == 0)
		{
			info->Damage = val;
		}
		else if (linelen == 5)
		{
			if (stricmp (Line1, "Speed") == 0)
			{
				info->Speed = val;
			}
			else if (stricmp (Line1, "Width") == 0)
			{
				info->radius = val;
			}
			else if (stricmp (Line1, "Alpha") == 0)
			{
				info->alpha = (fixed_t)(atof (Line2) * FRACUNIT);
				hadTranslucency = true;
			}
			else if (stricmp (Line1, "Scale") == 0)
			{
				info->scaleY = info->scaleX = clamp<fixed_t> (FLOAT2FIXED(atof (Line2)), 1, 256*FRACUNIT);
			}
			else if (stricmp (Line1, "Decal") == 0)
			{
				stripwhite (Line2);
				const FDecalTemplate *decal = DecalLibrary.GetDecalByName (Line2);
				if (decal != NULL)
				{
					info->DecalGenerator = const_cast <FDecalTemplate *>(decal);
				}
				else
				{
					Printf ("Thing %d: Unknown decal %s\n", thingy, Line2);
				}
			}
		}
		else if (linelen == 12 && stricmp (Line1, "Render Style") == 0)
		{
			stripwhite (Line2);
			int style = FindStyle (Line2);
			if (style >= 0)
			{
				info->RenderStyle = ERenderStyle(style);
				hadStyle = true;
			}
		}
		else if (linelen > 6)
		{
			if (linelen == 12 && stricmp (Line1, "No Ice Death") == 0)
			{
				if (val)
				{
					info->flags4 |= MF4_NOICEDEATH;
				}
				else
				{
					info->flags4 &= ~MF4_NOICEDEATH;
				}
			}
			else if (stricmp (Line1 + linelen - 6, " frame") == 0)
			{
				FState *state = FindState (val);

				if (type != NULL && !patchedStates)
				{
					statedef.MakeStateDefines(type);
					patchedStates = true;
				}

				if (!strnicmp (Line1, "Initial", 7))
					statedef.SetStateLabel("Spawn", state ? state : GetDefault<AActor>()->SpawnState);
				else if (!strnicmp (Line1, "First moving", 12))
					statedef.SetStateLabel("See", state);
				else if (!strnicmp (Line1, "Injury", 6))
					statedef.SetStateLabel("Pain", state);
				else if (!strnicmp (Line1, "Close attack", 12))
				{
					if (thingy != 1)	// Not for players!
					{
						statedef.SetStateLabel("Melee", state);
					}
				}
				else if (!strnicmp (Line1, "Far attack", 10))
				{
					if (thingy != 1)	// Not for players!
					{
						statedef.SetStateLabel("Missile", state);
					}
				}
				else if (!strnicmp (Line1, "Death", 5))
					statedef.SetStateLabel("Death", state);
				else if (!strnicmp (Line1, "Exploding", 9))
					statedef.SetStateLabel("XDeath", state);
				else if (!strnicmp (Line1, "Respawn", 7))
					statedef.SetStateLabel("Raise", state);
			}
			else if (stricmp (Line1 + linelen - 6, " sound") == 0)
			{
				FSoundID snd;
				
				if (val == 0 || val >= SoundMap.Size())
				{
					if (endptr == Line2)
					{ // Sound was not a (valid) number,
						// so treat it as an actual sound name.
						stripwhite (Line2);
						snd = Line2;
					}
				}
				else
				{
					snd = SoundMap[val-1];
				}

				if (!strnicmp (Line1, "Alert", 5))
					info->SeeSound = snd;
				else if (!strnicmp (Line1, "Attack", 6))
					info->AttackSound = snd;
				else if (!strnicmp (Line1, "Pain", 4))
					info->PainSound = snd;
				else if (!strnicmp (Line1, "Death", 5))
					info->DeathSound = snd;
				else if (!strnicmp (Line1, "Action", 6))
					info->ActiveSound = snd;
			}
		}
		else if (linelen == 4)
		{
			if (stricmp (Line1, "Mass") == 0)
			{
				info->Mass = val;
			}
			else if (stricmp (Line1, "Bits") == 0)
			{
				DWORD value[4] = { 0, 0, 0 };
				bool vchanged[4] = { false, false, false };
				// ZDoom used to block the upper range of bits to force use of mnemonics for extra flags.
				// MBF also defined extra flags in the same range, but without forcing mnemonics. For MBF
				// compatibility, the upper bits are freed, but we have conflicts between the ZDoom bits
				// and the MBF bits. The only such flag exposed to DEHSUPP, though, is STEALTH -- the others
				// are not available through mnemonics, and aren't available either through their bit value.
				// So if we find the STEALTH keyword, it's a ZDoom mod, otherwise assume FRIEND.
				bool zdoomflags = false;
				char *strval;

				for (strval = Line2; (strval = strtok (strval, ",+| \t\f\r")); strval = NULL)
				{
					if (IsNum (strval))
					{
						// I have no idea why everyone insists on using strtol here even though it fails
						// dismally if a value is parsed where the highest bit it set. Do people really
						// use negative values here? Let's better be safe and check both.
						if (strchr(strval, '-')) value[0] |= (unsigned long)strtol(strval, NULL, 10);
						else value[0] |= (unsigned long)strtoul(strval, NULL, 10);
						vchanged[0] = true;
					}
					else
					{
						// STEALTH FRIEND HACK!
						if (!stricmp(strval, "STEALTH")) zdoomflags = true;
						unsigned i;
						for(i = 0; i < BitNames.Size(); i++)
						{
							if (!stricmp(strval, BitNames[i].Name))
							{
								vchanged[BitNames[i].WhichFlags] = true;
								value[BitNames[i].WhichFlags] |= 1 << BitNames[i].Bit;
								break;
							}
						}
						if (i == BitNames.Size())
						{
							DPrintf("Unknown bit mnemonic %s\n", strval);
						}
					}
				}
				if (vchanged[0])
				{
					if (value[0] & MF_SLIDE)
					{
						// SLIDE (which occupies in Doom what is the MF_INCHASE slot in ZDoom)
						value[0] &= ~MF_SLIDE; // clean the slot
						// Nothing else to do, this flag is never actually used.
					}
					if (value[0] & MF_TRANSLATION)
					{
						info->Translation = TRANSLATION (TRANSLATION_Standard,
							((value[0] & MF_TRANSLATION) >> (MF_TRANSSHIFT))-1);
						value[0] &= ~MF_TRANSLATION;
					}
					if (value[0] & MF_TOUCHY)
					{
						// TOUCHY (which occupies in MBF what is the MF_UNMORPHED slot in ZDoom)
						value[0] &= ~MF_TOUCHY; // clean the slot
						info->flags6 |= MF6_TOUCHY; // remap the flag
					}
					if (value[0] & MF_BOUNCES)
					{
						// BOUNCES (which occupies in MBF the MF_NOLIFTDROP slot)
						// This flag is especially convoluted as what it does depend on what
						// other flags the actor also has, and whether it's "sentient" or not.
						value[0] &= ~MF_BOUNCES; // clean the slot

						// MBF considers that things that bounce can be damaged, even if not shootable.
						info->flags6 |= MF6_VULNERABLE;
						// MBF also considers that bouncers pass through blocking lines as projectiles.
						info->flags3 |= MF3_NOBLOCKMONST;
						// MBF also considers that bouncers that explode are grenades, and MBF grenades
						// are supposed to hurt everything, except cyberdemons if they're fired by cybies.
						// Let's translate that in a more generic way as grenades which hurt everything
						// except the class of their shooter. Yes, it does diverge a bit from MBF, as for
						// example a dehacked arachnotron that shoots grenade would kill itself quickly
						// in MBF and will not here. But class-specific checks are cumbersome and limiting.
						info->flags4 |= (MF4_FORCERADIUSDMG | MF4_DONTHARMCLASS);

						// MBF bouncing missiles rebound on floors and ceiling, but not on walls.
						// This is different from BOUNCE_Heretic behavior as in Heretic the missiles
						// die when they bounce, while in MBF they will continue to bounce until they
						// collide with a wall or a solid actor.
						if (value[0] & MF_MISSILE) info->BounceFlags = BOUNCE_Classic;
						// MBF bouncing actors that do not have the missile flag will also rebound on
						// walls, and this does correspond roughly to the ZDoom bounce style.
						else info->BounceFlags = BOUNCE_Grenade;

						// MBF grenades are dehacked rockets that gain the BOUNCES flag but
						// lose the MISSILE flag, so they can be identified here easily.
						if (!(value[0] & MF_MISSILE) && info->effects & FX_ROCKET)
						{
							info->effects &= ~FX_ROCKET;	// replace rocket trail
							info->effects |= FX_GRENADE;	// by grenade trail
						}

						// MBF bounce factors depend on flag combos:
						enum
						{
							MBF_BOUNCE_NOGRAVITY	= FRACUNIT,				// With NOGRAVITY: full momentum
							MBF_BOUNCE_FLOATDROPOFF	= (FRACUNIT * 85) / 100,// With FLOAT and DROPOFF: 85%
							MBF_BOUNCE_FLOAT		= (FRACUNIT * 70) / 100,// With FLOAT alone: 70%
							MBF_BOUNCE_DEFAULT		= (FRACUNIT * 45) / 100,// Without the above flags: 45%
							MBF_BOUNCE_WALL			= (FRACUNIT * 50) / 100,// Bouncing off walls: 50%
						};
						info->bouncefactor = ((value[0] & MF_NOGRAVITY) ? MBF_BOUNCE_NOGRAVITY
							: (value[0] & MF_FLOAT) ? (value[0] & MF_DROPOFF) ? MBF_BOUNCE_FLOATDROPOFF
							: MBF_BOUNCE_FLOAT : MBF_BOUNCE_DEFAULT);

						info->wallbouncefactor = ((value[0] & MF_NOGRAVITY) ? MBF_BOUNCE_NOGRAVITY : MBF_BOUNCE_WALL);

						// MBF sentient actors with BOUNCE and FLOAT are able to "jump" by floating up.
						if (info->IsSentient())
						{
							if (value[0] & MF_FLOAT) info->flags6 |= MF6_CANJUMP;
						}
						// Non sentient actors can be damaged but they shouldn't bleed.
						else
						{
							value[0] |= MF_NOBLOOD;
						}
					}
					if (zdoomflags && (value [0] & MF_STEALTH))
					{
						// STEALTH FRIEND HACK!
					}
					else if (value[0] & MF_FRIEND) 
					{
						// FRIEND (which occupies in MBF the MF_STEALTH slot)
						value[0] &= ~MF_FRIEND; // clean the slot
						value[0] |= MF_FRIENDLY; // remap the flag to its ZDoom equivalent
						// MBF friends are not blocked by monster blocking lines:
						info->flags3 |= MF3_NOBLOCKMONST;
					}
					if (value[0] & MF_TRANSLUCENT)
					{
						// TRANSLUCENT (which occupies in Boom the MF_ICECORPSE slot)
						value[0] &= ~MF_TRANSLUCENT; // clean the slot
						vchanged[2] = true; value[2] |= 2; // let the TRANSLUCxx code below handle it
					}
					if ((info->flags & MF_MISSILE) && (info->flags2 & MF2_NOTELEPORT)
						&& !(value[0] & MF_MISSILE))
					{
						// ZDoom gives missiles flags that did not exist in Doom: MF2_NOTELEPORT, 
						// MF2_IMPACT, and MF2_PCROSS. The NOTELEPORT one can be a problem since 
						// some projectile actors (those new to Doom II) were not excluded from 
						// triggering line effects and can teleport when the missile flag is removed.
						info->flags2 &= ~MF2_NOTELEPORT;
					}
					info->flags = value[0];
				}
				if (vchanged[1])
				{
					info->flags2 = value[1];
					if (info->flags2 & 0x00000004)	// old BOUNCE1
					{ 	
						info->flags2 &= ~4;
						info->BounceFlags = BOUNCE_DoomCompat;
					}
					// Damage types that once were flags but now are not
					if (info->flags2 & 0x20000000)
					{
						info->DamageType = NAME_Ice;
						info->flags2 &= ~0x20000000;
					}
					if (info->flags2 & 0x10000)
					{
						info->DamageType = NAME_Fire;
						info->flags2 &= ~0x10000;
					}
					if (info->flags2 & 1)
					{
						info->gravity = FRACUNIT/4;
						info->flags2 &= ~1;
					}
				}
				if (vchanged[2])
				{
					if (value[2] & 7)
					{
						hadTranslucency = true;
						if (value[2] & 1)
							info->alpha = TRANSLUC25;
						else if (value[2] & 2)
							info->alpha = TRANSLUC50;
						else if (value[2] & 4)
							info->alpha = TRANSLUC75;
						info->RenderStyle = STYLE_Translucent;
					}
					if (value[2] & 8)
						info->renderflags |= RF_INVISIBLE;
					else
						info->renderflags &= ~RF_INVISIBLE;
				}
				DPrintf ("Bits: %d,%d (0x%08x,0x%08x)\n", info->flags, info->flags2,
													      info->flags, info->flags2);
			}
			else if (stricmp (Line1, "ID #") == 0)
			{
				*ednum = (SWORD)val;
			}
		}
		else Printf (unknown_str, Line1, "Thing", thingy);
	}

	if (info != (AActor *)&dummy)
	{
		// Reset heights for things hanging from the ceiling that
		// don't specify a new height.
		if (info->flags & MF_SPAWNCEILING &&
			!hadHeight &&
			thingy <= (int)OrgHeights.Size() && thingy > 0)
		{
			info->height = OrgHeights[thingy - 1] * FRACUNIT;
			info->projectilepassheight = 0;
		}
		// If the thing's shadow changed, change its fuzziness if not already specified
		if ((info->flags ^ oldflags) & MF_SHADOW)
		{
			if (info->flags & MF_SHADOW)
			{ // changed to shadow
				if (!hadStyle)
					info->RenderStyle = STYLE_OptFuzzy;
				if (!hadTranslucency)
					info->alpha = FRACUNIT/5;
			}
			else
			{ // changed from shadow
				if (!hadStyle)
					info->RenderStyle = STYLE_Normal;
			}
		}
		// If this thing's speed is really low (i.e. meant to be a monster),
		// bump it up, because all speeds are fixed point now.
		if (abs(info->Speed) < 256)
		{
			info->Speed <<= FRACBITS;
		}

		if (info->flags & MF_SPECIAL)
		{
			PushTouchedActor(const_cast<PClass *>(type));
		}

		// If MF_COUNTKILL is set, make sure the other standard monster flags are
		// set, too. And vice versa.
		if (thingy != 1) // don't mess with the player's flags
		{
			if (info->flags & MF_COUNTKILL)
			{
				info->flags2 |= MF2_PUSHWALL | MF2_MCROSS | MF2_PASSMOBJ;
				info->flags3 |= MF3_ISMONSTER;
			}
			else
			{
				info->flags2 &= ~(MF2_PUSHWALL | MF2_MCROSS);
				info->flags3 &= ~MF3_ISMONSTER;
			}
		}
		// Everything that's altered here gets the CANUSEWALLS flag, just in case
		// it calls P_Move().
		info->flags4 |= MF4_CANUSEWALLS;
		if (patchedStates)
		{
			statedef.InstallStates(type->ActorInfo, info);
		}
	}

	return result;
}

// The only remotely useful thing Dehacked sound patches could do
// was change where the sound's name was stored. Since there is no
// real benefit to doing this, and it would be very difficult for
// me to emulate it, I have disabled them entirely.

static int PatchSound (int soundNum)
{
	int result;

	DPrintf ("Sound %d (no longer supported)\n", soundNum);
/*
	sfxinfo_t *info, dummy;
	int offset = 0;
	if (soundNum >= 1 && soundNum <= NUMSFX) {
		info = &S_sfx[soundNum];
	} else {
		info = &dummy;
		Printf ("Sound %d out of range.\n");
	}
*/
	while ((result = GetLine ()) == 1) {
		/*
		if (!stricmp  ("Offset", Line1))
			offset = atoi (Line2);
		else CHECKKEY ("Zero/One",			info->singularity)
		else CHECKKEY ("Value",				info->priority)
		else CHECKKEY ("Zero 1",			info->link)
		else CHECKKEY ("Neg. One 1",		info->pitch)
		else CHECKKEY ("Neg. One 2",		info->volume)
		else CHECKKEY ("Zero 2",			info->data)
		else CHECKKEY ("Zero 3",			info->usefulness)
		else CHECKKEY ("Zero 4",			info->lumpnum)
		else Printf (unknown_str, Line1, "Sound", soundNum);
		*/
	}
/*
	if (offset) {
		// Calculate offset from start of sound names
		offset -= toff[dversion] + 21076;

		if (offset <= 64)			// pistol .. bfg
			offset >>= 3;
		else if (offset <= 260)		// sawup .. oof
			offset = (offset + 4) >> 3;
		else						// telept .. skeatk
			offset = (offset + 8) >> 3;

		if (offset >= 0 && offset < NUMSFX) {
			S_sfx[soundNum].name = OrgSfxNames[offset + 1];
		} else {
			Printf ("Sound name %d out of range.\n", offset + 1);
		}
	}
*/
	return result;
}

static int PatchFrame (int frameNum)
{
	int result;
	int tics, misc1, frame;
	FState *info, dummy;

	info = FindState (frameNum);
	if (info)
	{
		DPrintf ("Frame %d\n", frameNum);
		if (frameNum == 47)
		{ // Use original tics for S_DSGUNFLASH1
			tics = 5;
		}
		else if (frameNum == 48)
		{ // Ditto for S_DSGUNFLASH2
			tics = 4;
		}
		else
		{
			tics = info->GetTics ();
		}
		misc1 = info->GetMisc1 ();
		frame = info->GetFrame () | (info->GetFullbright() ? 0x8000 : 0);
	}
	else
	{
		info = &dummy;
		tics = misc1 = frame = 0;
		Printf ("Frame %d out of range\n", frameNum);
	}

	while ((result = GetLine ()) == 1)
	{
		int val = atoi (Line2);
		size_t keylen = strlen (Line1);

		if (keylen == 8 && stricmp (Line1, "Duration") == 0)
		{
			tics = clamp (val, -1, SHRT_MAX);
		}
		else if (keylen == 9 && stricmp (Line1, "Unknown 1") == 0)
		{
			misc1 = val;
		}
		else if (keylen == 9 && stricmp (Line1, "Unknown 2") == 0)
		{
			info->Misc2 = val;
			info->Misc2Updated = true;
		}
		else if (keylen == 13 && stricmp (Line1, "Sprite number") == 0)
		{
			unsigned int i;

			if (val < (int)OrgSprNames.Size())
			{
				for (i = 0; i < sprites.Size(); i++)
				{
					if (memcmp (OrgSprNames[val].c, sprites[i].name, 4) == 0)
					{
						info->sprite = (int)i;
						break;
					}
				}
				if (i == sprites.Size ())
				{
					Printf ("Frame %d: Sprite %d (%s) is undefined\n",
						frameNum, val, OrgSprNames[val].c);
				}
			}
			else
			{
				Printf ("Frame %d: Sprite %d out of range\n", frameNum, val);
			}
		}
		else if (keylen == 10 && stricmp (Line1, "Next frame") == 0)
		{
			info->NextState = FindState (val);
		}
		else if (keylen == 16 && stricmp (Line1, "Sprite subnumber") == 0)
		{
			frame = val;
		}
		else
		{
			Printf (unknown_str, Line1, "Frame", frameNum);
		}
	}

	if (info != &dummy)
	{
		info->DefineFlags |= SDF_DEHACKED;	// Signals the state has been modified by dehacked
		if ((unsigned)(frame & 0x7fff) > 63)
		{
			Printf ("Frame %d: Subnumber must be in range [0,63]\n", frameNum);
		}
		info->Tics = tics;
		info->Misc1 = misc1;
		info->Misc1Updated = true;
		info->Frame = frame & 0x3f;
		info->Fullbright = frame & 0x8000 ? true : false;
	}

	return result;
}

static int PatchSprite (int sprNum)
{
	int result;
	int offset = 0;

	if ((unsigned)sprNum < OrgSprNames.Size())
	{
		DPrintf ("Sprite %d\n", sprNum);
	}
	else
	{
		Printf ("Sprite %d out of range.\n", sprNum);
		sprNum = -1;
	}

	while ((result = GetLine ()) == 1)
	{
		if (!stricmp ("Offset", Line1))
			offset = atoi (Line2);
		else Printf (unknown_str, Line1, "Sprite", sprNum);
	}

	if (offset > 0 && sprNum != -1)
	{
		// Calculate offset from beginning of sprite names.
		offset = (offset - toff[dversion] - 22044) / 8;
	
		if ((unsigned)offset < OrgSprNames.Size())
		{
			sprNum = FindSprite (OrgSprNames[sprNum].c);
			if (sprNum != -1)
				strncpy (sprites[sprNum].name, OrgSprNames[offset].c, 4);
		}
		else
		{
			Printf ("Sprite name %d out of range.\n", offset);
		}
	}

	return result;
}

static int PatchAmmo (int ammoNum)
{
	const PClass *ammoType = NULL;
	AAmmo *defaultAmmo = NULL;
	int result;
	int oldclip;
	int dummy;
	int *max = &dummy;
	int *per = &dummy;

	if (ammoNum >= 0 && ammoNum < 4 && (unsigned)ammoNum <= AmmoNames.Size())
	{
		DPrintf ("Ammo %d.\n", ammoNum);
		ammoType = AmmoNames[ammoNum];
		if (ammoType != NULL)
		{
			defaultAmmo = (AAmmo *)GetDefaultByType (ammoType);
			if (defaultAmmo != NULL)
			{
				max = &defaultAmmo->MaxAmount;
				per = &defaultAmmo->Amount;
			}
		}
	}

	if (ammoType == NULL)
	{
		Printf ("Ammo %d out of range.\n", ammoNum);
	}

	oldclip = *per;

	while ((result = GetLine ()) == 1)
	{
			 CHECKKEY ("Max ammo", *max)
		else CHECKKEY ("Per ammo", *per)
		else Printf (unknown_str, Line1, "Ammo", ammoNum);
	}

	// Calculate the new backpack-given amounts for this ammo.
	if (ammoType != NULL)
	{
		defaultAmmo->BackpackMaxAmount = defaultAmmo->MaxAmount * 2;
		defaultAmmo->BackpackAmount = defaultAmmo->Amount;
	}

	// Fix per-ammo/max-ammo amounts for descendants of the base ammo class
	if (oldclip != *per)
	{
		for (unsigned int i = 0; i < PClass::m_Types.Size(); ++i)
		{
			PClass *type = PClass::m_Types[i];

			if (type == ammoType)
				continue;

			if (type->IsDescendantOf (ammoType))
			{
				defaultAmmo = (AAmmo *)GetDefaultByType (type);
				defaultAmmo->MaxAmount = *max;
				defaultAmmo->Amount = Scale (defaultAmmo->Amount, *per, oldclip);
			}
			else if (type->IsDescendantOf (RUNTIME_CLASS(AWeapon)))
			{
				AWeapon *defWeap = (AWeapon *)GetDefaultByType (type);
				if (defWeap->AmmoType1 == ammoType)
				{
					defWeap->AmmoGive1 = Scale (defWeap->AmmoGive1, *per, oldclip);
				}
				if (defWeap->AmmoType2 == ammoType)
				{
					defWeap->AmmoGive2 = Scale (defWeap->AmmoGive2, *per, oldclip);
				}
			}
		}
	}

	return result;
}

static int PatchWeapon (int weapNum)
{
	int result;
	const PClass *type = NULL;
	BYTE dummy[sizeof(AWeapon)];
	AWeapon *info = (AWeapon *)&dummy;
	bool patchedStates = false;
	FStateDefinitions statedef;

	if (weapNum >= 0 && weapNum < 9 && (unsigned)weapNum < WeaponNames.Size())
	{
		type = WeaponNames[weapNum];
		if (type != NULL)
		{
			info = (AWeapon *)GetDefaultByType (type);
			DPrintf ("Weapon %d\n", weapNum);
		}
	}

	if (type == NULL)
	{
		Printf ("Weapon %d out of range.\n", weapNum);
	}

	while ((result = GetLine ()) == 1)
	{
		int val = atoi (Line2);

		if (strlen (Line1) >= 9)
		{
			if (stricmp (Line1 + strlen (Line1) - 6, " frame") == 0)
			{
				FState *state = FindState (val);

				if (type != NULL && !patchedStates)
				{
					statedef.MakeStateDefines(type);
					patchedStates = true;
				}

				if (strnicmp (Line1, "Deselect", 8) == 0)
					statedef.SetStateLabel("Select", state);
				else if (strnicmp (Line1, "Select", 6) == 0)
					statedef.SetStateLabel("Deselect", state);
				else if (strnicmp (Line1, "Bobbing", 7) == 0)
					statedef.SetStateLabel("Ready", state);
				else if (strnicmp (Line1, "Shooting", 8) == 0)
					statedef.SetStateLabel("Fire", state);
				else if (strnicmp (Line1, "Firing", 6) == 0)
					statedef.SetStateLabel("Flash", state);
			}
			else if (stricmp (Line1, "Ammo type") == 0)
			{
				if (val < 0 || val >= 12 || (unsigned)val >= AmmoNames.Size())
				{
					val = 5;
				}
				info->AmmoType1 = AmmoNames[val];
				if (info->AmmoType1 != NULL)
				{
					info->AmmoGive1 = ((AAmmo*)GetDefaultByType (info->AmmoType1))->Amount * 2;
					if (info->AmmoUse1 == 0)
					{
						info->AmmoUse1 = 1;
					}
				}
			}
			else
			{
				Printf (unknown_str, Line1, "Weapon", weapNum);
			}
		}
		else if (stricmp (Line1, "Decal") == 0)
		{
			stripwhite (Line2);
			const FDecalTemplate *decal = DecalLibrary.GetDecalByName (Line2);
			if (decal != NULL)
			{
				info->DecalGenerator = const_cast <FDecalTemplate *>(decal);
			}
			else
			{
				Printf ("Weapon %d: Unknown decal %s\n", weapNum, Line2);
			}
		}
		else if (stricmp (Line1, "Ammo use") == 0 || stricmp (Line1, "Ammo per shot") == 0)
		{
			info->AmmoUse1 = val;
			info->flags6 |= MF6_INTRYMOVE;	// flag the weapon for postprocessing (reuse a flag that can't be set by external means)
		}
		else if (stricmp (Line1, "Min ammo") == 0)
		{
			info->MinAmmo1 = val;
		}
		else
		{
			Printf (unknown_str, Line1, "Weapon", weapNum);
		}
	}

	if (info->AmmoType1 == NULL)
	{
		info->AmmoUse1 = 0;
	}

	if (patchedStates)
	{
		statedef.InstallStates(type->ActorInfo, info);
	}

	return result;
}

static void SetPointer(FState *state, PSymbol *sym, int frame = 0)
{
	if (sym==NULL || sym->SymbolType != SYM_ActionFunction)
	{
		state->SetAction(NULL);
		return;
	}
	else
	{
		FString symname = sym->SymbolName.GetChars();
		state->SetAction(static_cast<PSymbolActionFunction*>(sym));

		// Note: CompareNoCase() calls stricmp() and therefore returns 0 when they're the same.
		for (unsigned int i = 0; i < MBFCodePointers.Size(); i++)
		{
			if (!symname.CompareNoCase(MBFCodePointers[i].name))
			{
				MBFParamState newstate;
				newstate.state = state;
				newstate.pointer = i;
				MBFParamStates.Push(newstate);
				break; // No need to cycle through the rest of the list.
			}
		}
	}
}

static int PatchPointer (int ptrNum)
{
	int result;

	// Hack for some Boom dehacked patches that are of the form Pointer 0 (x statenumber)
	char * key;
	int indexnum;
	key=strchr(Line2, '(');
	if (key++) key=strchr(key, ' '); else key=NULL;
	if ((ptrNum == 0) && key++)
	{
		*strchr(key, ')') = '\0';
		indexnum = atoi(key);
		for (ptrNum = 0; (unsigned int) ptrNum < CodePConv.Size(); ++ptrNum)
		{
			if (CodePConv[ptrNum] == indexnum) break;
		}
		DPrintf("Final ptrNum: %i\n", ptrNum);
	}
	// End of hack.

	// 448 Doom states with codepointers + 74 extra MBF states with codepointers = 522 total
	// Better to just use the size of the array rather than a hardcoded value.
	if (ptrNum >= 0 && (unsigned int) ptrNum < CodePConv.Size())
	{
		DPrintf ("Pointer %d\n", ptrNum);
	}
	else
	{
		Printf ("Pointer %d out of range.\n", ptrNum);
		ptrNum = -1;
	}

	while ((result = GetLine ()) == 1)
	{
		if ((unsigned)ptrNum < CodePConv.Size() && (!stricmp (Line1, "Codep Frame")))
		{
			FState *state = FindState (CodePConv[ptrNum]);
			if (state)
			{
				int index = atoi(Line2);
				if ((unsigned)(index) >= Actions.Size())
					SetPointer(state, NULL);
				else
				{
					SetPointer(state, Actions[index], CodePConv[ptrNum]);
				}
				DPrintf("%s has a hacked state for pointer num %i with index %i\nLine1=%s, Line2=%s\n", 
					state->StaticFindStateOwner(state)->TypeName.GetChars(), ptrNum, index, Line1, Line2);
			}
			else
			{
				Printf ("Bad code pointer %d\n", ptrNum);
			}
		}
		else Printf (unknown_str, Line1, "Pointer", ptrNum);
	}
	return result;
}

static int PatchCheats (int dummy)
{
	int result;

	DPrintf ("Cheats (support removed by request)\n");

	while ((result = GetLine ()) == 1)
	{
	}
	return result;
}

static int PatchMisc (int dummy)
{
	static const struct Key keys[] = {
		{ "Initial Health",			static_cast<ptrdiff_t>(myoffsetof(struct DehInfo,StartHealth)) },
		{ "Initial Bullets",		static_cast<ptrdiff_t>(myoffsetof(struct DehInfo,StartBullets)) },
		{ "Max Health",				static_cast<ptrdiff_t>(myoffsetof(struct DehInfo,MaxHealth)) },
		{ "Max Armor",				static_cast<ptrdiff_t>(myoffsetof(struct DehInfo,MaxArmor)) },
		{ "Green Armor Class",		static_cast<ptrdiff_t>(myoffsetof(struct DehInfo,GreenAC)) },
		{ "Blue Armor Class",		static_cast<ptrdiff_t>(myoffsetof(struct DehInfo,BlueAC)) },
		{ "Max Soulsphere",			static_cast<ptrdiff_t>(myoffsetof(struct DehInfo,MaxSoulsphere)) },
		{ "Soulsphere Health",		static_cast<ptrdiff_t>(myoffsetof(struct DehInfo,SoulsphereHealth)) },
		{ "Megasphere Health",		static_cast<ptrdiff_t>(myoffsetof(struct DehInfo,MegasphereHealth)) },
		{ "God Mode Health",		static_cast<ptrdiff_t>(myoffsetof(struct DehInfo,GodHealth)) },
		{ "IDFA Armor",				static_cast<ptrdiff_t>(myoffsetof(struct DehInfo,FAArmor)) },
		{ "IDFA Armor Class",		static_cast<ptrdiff_t>(myoffsetof(struct DehInfo,FAAC)) },
		{ "IDKFA Armor",			static_cast<ptrdiff_t>(myoffsetof(struct DehInfo,KFAArmor)) },
		{ "IDKFA Armor Class",		static_cast<ptrdiff_t>(myoffsetof(struct DehInfo,KFAAC)) },
		{ "No Autofreeze",			static_cast<ptrdiff_t>(myoffsetof(struct DehInfo,NoAutofreeze)) },
		{ NULL, 0 }
	};
	int result;

	DPrintf ("Misc\n");

	while ((result = GetLine()) == 1)
	{
		if (HandleKey (keys, &deh, Line1, atoi (Line2)))
		{
			if (stricmp (Line1, "BFG Cells/Shot") == 0)
			{
				deh.BFGCells = atoi (Line2);
			}
			else if (stricmp (Line1, "Rocket Explosion Style") == 0)
			{
				stripwhite (Line2);
				int style = FindStyle (Line2);
				if (style >= 0)
				{
					deh.ExplosionStyle = style;
				}
			}
			else if (stricmp (Line1, "Rocket Explosion Alpha") == 0)
			{
				deh.ExplosionAlpha = (fixed_t)(atof (Line2) * FRACUNIT);
			}
			else if (stricmp (Line1, "Monsters Infight") == 0)
			{
				infighting = atoi (Line2);
			}
			else if (stricmp (Line1, "Monsters Ignore Each Other") == 0)
			{
				infighting = atoi (Line2) ? -1 : 0;
			}
			else if (strnicmp (Line1, "Powerup Color ", 14) == 0)
			{
				static const char * const names[] =
				{
					"Invulnerability",
					"Berserk",
					"Invisibility",
					"Radiation Suit",
					"Infrared",
					"Tome of Power",
					"Wings of Wrath",
					"Speed",
					"Minotaur",
					NULL
				};
				static const PClass * const types[] =
				{
					RUNTIME_CLASS(APowerInvulnerable),
					RUNTIME_CLASS(APowerStrength),
					RUNTIME_CLASS(APowerInvisibility),
					RUNTIME_CLASS(APowerIronFeet),
					RUNTIME_CLASS(APowerLightAmp),
					RUNTIME_CLASS(APowerWeaponLevel2),
					RUNTIME_CLASS(APowerSpeed),
					RUNTIME_CLASS(APowerMinotaur)
				};
				int i;

				for (i = 0; names[i] != NULL; ++i)
				{
					if (stricmp (Line1 + 14, names[i]) == 0)
					{
						break;
					}
				}
				if (names[i] == NULL)
				{
					Printf ("Unknown miscellaneous info %s.\n", Line1);
				}
				else
				{
					int r, g, b;
					float a;

					if (4 != sscanf (Line2, "%d %d %d %f", &r, &g, &b, &a))
					{
						Printf ("Bad powerup color description \"%s\" for %s\n", Line2, Line1);
					}
					else if (a > 0)
					{
						static_cast<APowerup *>(GetDefaultByType (types[i]))->BlendColor = PalEntry(
							BYTE(clamp(a,0.f,1.f)*255.f),
							clamp(r,0,255),
							clamp(g,0,255),
							clamp(b,0,255));
					}
					else
					{
						static_cast<APowerup *>(GetDefaultByType (types[i]))->BlendColor = 0;
					}
				}
			}
			else
			{
				Printf ("Unknown miscellaneous info %s.\n", Line1);
			}
		}
	}

	// Update default item properties by patching the affected items
	// Note: This won't have any effect on DECORATE derivates of these items!
	ABasicArmorPickup *armor;

	armor = static_cast<ABasicArmorPickup *> (GetDefaultByName ("GreenArmor"));
	if (armor!=NULL)
	{
		armor->SaveAmount = 100 * deh.GreenAC;
		armor->SavePercent = deh.GreenAC == 1 ? FRACUNIT/3 : FRACUNIT/2;
	}
	armor = static_cast<ABasicArmorPickup *> (GetDefaultByName ("BlueArmor"));
	if (armor!=NULL)
	{
		armor->SaveAmount = 100 * deh.BlueAC;
		armor->SavePercent = deh.BlueAC == 1 ? FRACUNIT/3 : FRACUNIT/2;
	}

	ABasicArmorBonus *barmor;
	barmor = static_cast<ABasicArmorBonus *> (GetDefaultByName ("ArmorBonus"));
	if (barmor!=NULL)
	{
		barmor->MaxSaveAmount = deh.MaxArmor;
	}

	AHealth *health;
	health = static_cast<AHealth *> (GetDefaultByName ("HealthBonus"));
	if (health!=NULL) 
	{
		health->MaxAmount = 2 * deh.MaxHealth;
	}

	health = static_cast<AHealth *> (GetDefaultByName ("Soulsphere"));
	if (health!=NULL)
	{
		health->Amount = deh.SoulsphereHealth;
		health->MaxAmount = deh.MaxSoulsphere;
	}

	health = static_cast<AHealth *> (GetDefaultByName ("MegasphereHealth"));
	if (health!=NULL)
	{
		health->Amount = health->MaxAmount = deh.MegasphereHealth;
	}

	APlayerPawn *player = static_cast<APlayerPawn *> (GetDefaultByName ("DoomPlayer"));
	if (player != NULL)
	{
		player->health = deh.StartHealth;

		// Hm... I'm not sure that this is the right way to change this info...
		int index = PClass::FindClass(NAME_DoomPlayer)->Meta.GetMetaInt (ACMETA_DropItems) - 1;
		if (index >= 0 && index < (signed)DropItemList.Size())
		{
			FDropItem * di = DropItemList[index];
			while (di != NULL)
			{
				if (di->Name == NAME_Clip)
				{
					di->amount = deh.StartBullets;
				}
				di = di->Next;
			}
		}
	}


	// 0xDD means "enable infighting"
	if (infighting == 0xDD)
	{
		infighting = 1;
	}
	else if (infighting != -1)
	{
		infighting = 0;
	}

	return result;
}

static int PatchPars (int dummy)
{
	char *space, mapname[8], *moredata;
	level_info_t *info;
	int result, par;

	DPrintf ("[Pars]\n");

	while ( (result = GetLine()) ) {
		// Argh! .bex doesn't follow the same rules as .deh
		if (result == 1) {
			Printf ("Unknown key in [PARS] section: %s\n", Line1);
			continue;
		}
		if (stricmp ("par", Line1))
			return result;

		space = strchr (Line2, ' ');

		if (!space) {
			Printf ("Need data after par.\n");
			continue;
		}

		*space++ = '\0';

		while (*space && isspace(*space))
			space++;

		moredata = strchr (space, ' ');

		if (moredata) {
			// At least 3 items on this line, must be E?M? format
			mysnprintf (mapname, countof(mapname), "E%cM%c", *Line2, *space);
			par = atoi (moredata + 1);
		} else {
			// Only 2 items, must be MAP?? format
			mysnprintf (mapname, countof(mapname), "MAP%02d", atoi(Line2) % 100);
			par = atoi (space);
		}

		if (!(info = FindLevelInfo (mapname)) ) {
			Printf ("No map %s\n", mapname);
			continue;
		}

		info->partime = par;
		DPrintf ("Par for %s changed to %d\n", mapname, par);
	}
	return result;
}

static int PatchCodePtrs (int dummy)
{
	int result;

	DPrintf ("[CodePtr]\n");

	while ((result = GetLine()) == 1)
	{
		if (!strnicmp ("Frame", Line1, 5) && isspace(Line1[5]))
		{
			int frame = atoi (Line1 + 5);
			FState *state = FindState (frame);

			stripwhite (Line2);
			if (state == NULL)
			{
				Printf ("Frame %d out of range\n", frame);
			}
			else if (!stricmp(Line2, "NULL"))
			{
				SetPointer(state, NULL);
			}
			else
			{
				FString symname;


				if ((Line2[0] == 'A' || Line2[0] == 'a') && Line2[1] == '_')
					symname = Line2;
				else
					symname.Format("A_%s", Line2);

				// Let's consider as aliases some redundant MBF pointer
				for (unsigned int i = 0; i < MBFCodePointers.Size(); i++)
				{
					if (!symname.CompareNoCase(MBFCodePointers[i].alias))
					{
						symname = MBFCodePointers[i].name;
						Printf("%s --> %s\n", MBFCodePointers[i].alias, MBFCodePointers[i].name);
					}
				}

				// This skips the action table and goes directly to the internal symbol table
				// DEH compatible functions are easy to recognize.
				PSymbol *sym = RUNTIME_CLASS(AInventory)->Symbols.FindSymbol(symname, true);
				if (sym == NULL || sym->SymbolType != SYM_ActionFunction)
				{
					Printf("Frame %d: Unknown code pointer '%s'\n", frame, Line2);
				}
				else
				{
					FString &args = static_cast<PSymbolActionFunction*>(sym)->Arguments;
					if (args.Len()!=0 && (args[0]<'a' || args[0]>'z'))
					{
						Printf("Frame %d: Incompatible code pointer '%s'\n", frame, Line2);
						sym = NULL;
					}
				}
				SetPointer(state, sym, frame);
			}
		}
	}
	return result;
}

static int PatchMusic (int dummy)
{
	int result;

	DPrintf ("[Music]\n");

	while ((result = GetLine()) == 1)
	{
		const char *newname = skipwhite (Line2);
		FString keystring;
		
		keystring << "MUSIC_" << Line1;

		GStrings.SetString (keystring, newname);
		DPrintf ("Music %s set to:\n%s\n", keystring.GetChars(), newname);
	}

	return result;
}

static int PatchText (int oldSize)
{
	int newSize;
	char *oldStr;
	char *newStr;
	char *temp;
	INTBOOL good;
	int result;
	int i;

	// Skip old size, since we already know it
	temp = Line2;
	while (*temp > ' ')
		temp++;
	while (*temp && *temp <= ' ')
		temp++;

	if (*temp == 0)
	{
		Printf ("Text chunk is missing size of new string.\n");
		return 2;
	}
	newSize = atoi (temp);

	oldStr = new char[oldSize + 1];
	newStr = new char[newSize + 1];

	if (!oldStr || !newStr)
	{
		Printf ("Out of memory.\n");
		goto donewithtext;
	}

	good = ReadChars (&oldStr, oldSize);
	good += ReadChars (&newStr, newSize);

	if (!good)
	{
		delete[] newStr;
		delete[] oldStr;
		Printf ("Unexpected end-of-file.\n");
		return 0;
	}

	if (includenotext)
	{
		Printf ("Skipping text chunk in included patch.\n");
		goto donewithtext;
	}

	DPrintf ("Searching for text:\n%s\n", oldStr);
	good = false;

	// Search through sprite names; they are always 4 chars
	if (oldSize == 4)
	{
		i = FindSprite (oldStr);
		if (i != -1)
		{
			strncpy (sprites[i].name, newStr, 4);
			if (strncmp ("PLAY", oldStr, 4) == 0)
			{
				strncpy (deh.PlayerSprite, newStr, 4);
			}
			// If this sprite is used by a pickup, then the DehackedPickup sprite map
			// needs to be updated too.
			for (i = 0; (size_t)i < countof(DehSpriteMappings); ++i)
			{
				if (strncmp (DehSpriteMappings[i].Sprite, oldStr, 4) == 0)
				{
					// Found a match, so change it.
					strncpy (DehSpriteMappings[i].Sprite, newStr, 4);

					// Now shift the map's entries around so that it stays sorted.
					// This must be done because the map is scanned using a binary search.
					while (i > 0 && strncmp (DehSpriteMappings[i-1].Sprite, newStr, 4) > 0)
					{
						swapvalues (DehSpriteMappings[i-1], DehSpriteMappings[i]);
						--i;
					}
					while ((size_t)i < countof(DehSpriteMappings)-1 &&
						strncmp (DehSpriteMappings[i+1].Sprite, newStr, 4) < 0)
					{
						swapvalues (DehSpriteMappings[i+1], DehSpriteMappings[i]);
						++i;
					}
					break;
				}
			}
			goto donewithtext;
		}
	}

	if (!good)
	{	
		// Search through most other texts
		const char *str;
		str = EnglishStrings->MatchString (oldStr);
		if (str != NULL)
		{
			GStrings.SetString (str, newStr);
			good = true;
		}

		if (!good)
		{
			DPrintf ("   (Unmatched)\n");
		}
	}
		

donewithtext:
	if (newStr)
		delete[] newStr;
	if (oldStr)
		delete[] oldStr;

	// Fetch next identifier for main loop
	while ((result = GetLine ()) == 1)
		;

	return result;
}

static int PatchStrings (int dummy)
{
	int result;

	DPrintf ("[Strings]\n");

	while ((result = GetLine()) == 1)
	{
		FString holdstring;
		do
		{
			holdstring += skipwhite (Line2);
			holdstring.StripRight();
			if (holdstring.Len() > 0 && holdstring[holdstring.Len()-1] == '\\')
			{
				holdstring.Truncate((long)holdstring.Len()-1);
				Line2 = igets ();
			}
			else
			{
				Line2 = NULL;
			}
		} while (Line2 && *Line2);

		ReplaceSpecialChars (holdstring.LockBuffer());
		holdstring.UnlockBuffer();
		GStrings.SetString (Line1, holdstring);
		DPrintf ("%s set to:\n%s\n", Line1, holdstring.GetChars());
	}

	return result;
}

static int DoInclude (int dummy)
{
	char *data;
	int savedversion, savepversion, savepatchsize;
	char *savepatchfile, *savepatchpt, *savepatchname;

	if (including)
	{
		Printf ("Sorry, can't nest includes\n");
		return GetLine();
	}

	if (strnicmp (Line2, "notext", 6) == 0 && Line2[6] != 0 && isspace(Line2[6]))
	{
		includenotext = true;
		Line2 = skipwhite (Line2+7);
	}

	stripwhite (Line2);
	if (*Line2 == '\"')
	{
		data = ++Line2;
		while (*data && *data != '\"')
			data++;
		*data = 0;
	}

	if (*Line2 == 0)
	{
		Printf ("Include directive is missing filename\n");
	}
	else
	{
		data = Line2;
		DPrintf ("Including %s\n", data);
		savepatchname = PatchName;
		savepatchfile = PatchFile;
		savepatchpt = PatchPt;
		savepatchsize = PatchSize;
		savedversion = dversion;
		savepversion = pversion;
		including = true;

		// Try looking for the included file in the same directory
		// as the patch before looking in the current file.
		const char *lastSlash = savepatchname ? strrchr (savepatchname, '/') : NULL;
		char *path = data;

		if (lastSlash != NULL)
		{
			size_t pathlen = lastSlash - savepatchname + strlen (data) + 2;
			path = new char[pathlen];
			strncpy (path, savepatchname, (lastSlash - savepatchname) + 1);
			strcpy (path + (lastSlash - savepatchname) + 1, data);
			if (!FileExists (path))
			{
				delete[] path;
				path = data;
			}
		}

		D_LoadDehFile(path);

		if (data != path)
		{
			delete[] path;
		}

		DPrintf ("Done with include\n");
		PatchName = savepatchname;
		PatchFile = savepatchfile;
		PatchPt = savepatchpt;
		PatchSize = savepatchsize;
		dversion = savedversion;
		pversion = savepversion;
	}

	including = false;
	includenotext = false;
	return GetLine();
}

CVAR(Int, dehload, 0, CVAR_ARCHIVE)	// Autoloading of .DEH lumps is disabled by default.

// checks if lump is a .deh or .bex file. Only lumps in the root directory are considered valid.
static bool isDehFile(int lumpnum)
{
	const char* const fullName  = Wads.GetLumpFullName(lumpnum);
	const char* const extension = strrchr(fullName, '.');

	return NULL != extension && strchr(fullName, '/') == NULL
		&& (0 == stricmp(extension, ".deh") || 0 == stricmp(extension, ".bex"));
}

int D_LoadDehLumps()
{
	int lastlump = 0, lumpnum, count = 0;

	while ((lumpnum = Wads.FindLump("DEHACKED", &lastlump)) >= 0)
	{
		count += D_LoadDehLump(lumpnum);
	}

	if (0 == PatchSize && dehload > 0)
	{
		// No DEH/BEX patch is loaded yet, try to find lump(s) with specific extensions

		if (dehload == 1)	// load all .DEH lumps that are found.
		{
			for (lumpnum = 0, lastlump = Wads.GetNumLumps(); lumpnum < lastlump; ++lumpnum)
			{
				if (isDehFile(lumpnum))
				{
					count += D_LoadDehLump(lumpnum);
				}
			}
		}
		else 	// only load the last .DEH lump that is found.
		{
			for (lumpnum = Wads.GetNumLumps()-1; lumpnum >=0; --lumpnum)
			{
				if (isDehFile(lumpnum))
				{
					count += D_LoadDehLump(lumpnum);
					break;
				}
			}
		}
	}

	return count;
}

bool D_LoadDehLump(int lumpnum)
{
	PatchSize = Wads.LumpLength(lumpnum);

	PatchName = copystring(Wads.GetLumpFullPath(lumpnum));
	PatchFile = new char[PatchSize + 1];
	Wads.ReadLump(lumpnum, PatchFile);
	PatchFile[PatchSize] = '\0';		// terminate with a '\0' character
	return DoDehPatch();
}

// [TP]
TArray<FString> D_GetDehFileNames()
{
	TArray<FString> filenames;
	for ( unsigned int i = 0; i < g_LoadedDehFiles.Size( ); ++i )
		filenames.Push( ExtractFileBase( g_LoadedDehFiles[i], true ) );
	return filenames;
}

// [Zalewa]
const TArray<FString>& D_GetDehFiles( )
{
	return g_LoadedDehFiles;
}

bool D_LoadDehFile(const char *patchfile)
{
	FILE *deh;

	deh = fopen(patchfile, "rb");
	if (deh != NULL)
	{
		PatchSize = Q_filelength(deh);

		PatchName = copystring(patchfile);
		PatchFile = new char[PatchSize + 1];
		fread(PatchFile, 1, PatchSize, deh);
		fclose(deh);
		PatchFile[PatchSize] = '\0';		// terminate with a '\0' character
		// return DoDehPatch();
		bool result = DoDehPatch();

		// [TP] If the patching succeeded, write this patch down so we can broadcast it to the
		// launcher (and to "File|Join" in server console ~[Zalewa]).
		if ( result )
			g_LoadedDehFiles.Push( patchfile );

		return result;
	}
	else
	{
		// Couldn't find it in the filesystem; try from a lump instead.
		int lumpnum = Wads.CheckNumForFullName(patchfile, true);
		if (lumpnum < 0)
		{
			// Compatibility fallback. It's just here because
			// some WAD may need it. Should be deleted if it can
			// be confirmed that nothing uses this case.
			FString filebase(ExtractFileBase(patchfile));
			lumpnum = Wads.CheckNumForName(filebase);
		}
		if (lumpnum >= 0)
		{
			return D_LoadDehLump(lumpnum);
		}
	}
	Printf ("Could not open DeHackEd patch \"%s\"\n", patchfile);
	return false;
}

static bool DoDehPatch()
{
	Printf("Adding dehacked patch %s\n", PatchName);

	int cont;

	dversion = pversion = -1;
	cont = 0;
	if (0 == strncmp (PatchFile, "Patch File for DeHackEd v", 25))
	{
		if (PatchFile[25] < '3')
		{
			delete[] PatchName;
			delete[] PatchFile;
			Printf (PRINT_BOLD, "\"%s\" is an old and unsupported DeHackEd patch\n", PatchFile);
			return false;
		}
		// fix for broken WolfenDoom patches which contain \0 characters in some places.
		for (int i = 0; i < PatchSize; i++)
		{
			if (PatchFile[i] == 0) PatchFile[i] = ' ';	
		}

		PatchPt = strchr (PatchFile, '\n');
		while ((cont = GetLine()) == 1)
		{
				 CHECKKEY ("Doom version", dversion)
			else CHECKKEY ("Patch format", pversion)
		}
		if (!cont || dversion == -1 || pversion == -1)
		{
			delete[] PatchName;
			delete[] PatchFile;
			Printf (PRINT_BOLD, "\"%s\" is not a DeHackEd patch file\n", PatchFile);
			return false;
		}
	}
	else
	{
		DPrintf ("Patch does not have DeHackEd signature. Assuming .bex\n");
		dversion = 19;
		pversion = 6;
		PatchPt = PatchFile;
		while ((cont = GetLine()) == 1)
		{}
	}

	if (pversion != 6)
	{
		Printf ("DeHackEd patch version is %d.\nUnexpected results may occur.\n", pversion);
	}

	if (dversion == 16)
		dversion = 0;
	else if (dversion == 17)
		dversion = 2;
	else if (dversion == 19)
		dversion = 3;
	else if (dversion == 20)
		dversion = 1;
	else if (dversion == 21)
		dversion = 4;
	else
	{
		Printf ("Patch created with unknown DOOM version.\nAssuming version 1.9.\n");
		dversion = 3;
	}

	if (!LoadDehSupp ())
	{
		Printf ("Could not load DEH support data\n");
		UnloadDehSupp ();
		delete[] PatchName;
		delete[] PatchFile;
		return false;
	}

	do
	{
		if (cont == 1)
		{
			Printf ("Key %s encountered out of context\n", Line1);
			cont = 0;
		}
		else if (cont == 2)
		{
			cont = HandleMode (Line1, atoi (Line2));
		}
	} while (cont);

	UnloadDehSupp ();
	delete[] PatchName;
	delete[] PatchFile;
	Printf ("Patch installed\n");
	return true;
}

static inline bool CompareLabel (const char *want, const BYTE *have)
{
	return *(DWORD *)want == *(DWORD *)have;
}

static inline short GetWord (const BYTE *in)
{
	return (in[0] << 8) | (in[1]);
}

static short *GetWordSpace (void *in, size_t size)
{
	short *ptr;
	size_t i;

	ptr = (short *)in;

	for (i = 0; i < size; i++)
	{
		ptr[i] = GetWord ((BYTE *)in + i*2);
	}
	return ptr;
}

static int DehUseCount;

static void UnloadDehSupp ()
{
	if (--DehUseCount <= 0)
	{
		// Handle MBF params here, before the required arrays are cleared
		for (unsigned int i=0; i < MBFParamStates.Size(); i++)
		{
			SetDehParams(MBFParamStates[i].state, MBFParamStates[i].pointer);
		}
		MBFParamStates.Clear();
		MBFParamStates.ShrinkToFit();
		MBFCodePointers.Clear();
		MBFCodePointers.ShrinkToFit();
		// StateMap is not freed here, because if you load a second
		// dehacked patch through some means other than including it
		// in the first patch, it won't see the state information
		// that was altered by the first. So we need to keep the
		// StateMap around until all patches have been applied.
		DehUseCount = 0;
		Actions.Clear();
		Actions.ShrinkToFit();
		OrgHeights.Clear();
		OrgHeights.ShrinkToFit();
		CodePConv.Clear();
		CodePConv.ShrinkToFit();
		OrgSprNames.Clear();
		OrgSprNames.ShrinkToFit();
		SoundMap.Clear();
		SoundMap.ShrinkToFit();
		InfoNames.Clear();
		InfoNames.ShrinkToFit();
		BitNames.Clear();
		BitNames.ShrinkToFit();
		StyleNames.Clear();
		StyleNames.ShrinkToFit();
		AmmoNames.Clear();
		AmmoNames.ShrinkToFit();

		if (UnchangedSpriteNames != NULL)
		{
			delete[] UnchangedSpriteNames;
			UnchangedSpriteNames = NULL;
			NumUnchangedSprites = 0;
		}
		if (EnglishStrings != NULL)
		{
			delete EnglishStrings;
			EnglishStrings = NULL;
		}
	}
}

static bool LoadDehSupp ()
{
	try
	{
		// Make sure we only get the DEHSUPP lump from zdoom.pk3
		// User modifications are not supported!
		int lump = Wads.CheckNumForName("DEHSUPP");

		if (lump == -1)
		{
			return false;
		}

		if (Wads.GetLumpFile(lump) > 0)
		{
			Printf("Warning: DEHSUPP no longer supported. DEHACKED patch disabled.\n");
			return false;
		}
		bool gotnames = false;
		int i;


		if (++DehUseCount > 1)
		{
			return true;
		}

		if (EnglishStrings == NULL)
		{
			EnglishStrings = new FStringTable;
			EnglishStrings->LoadStrings (true, false);
		}

		if (UnchangedSpriteNames == NULL)
		{
			UnchangedSpriteNames = new char[sprites.Size()*4];
			NumUnchangedSprites = sprites.Size();
			for (i = 0; i < NumUnchangedSprites; ++i)
			{
				memcpy (UnchangedSpriteNames+i*4, &sprites[i].name, 4);
			}
		}

		FScanner sc;

		sc.OpenLumpNum(lump);
		sc.SetCMode(true);

		while (sc.GetString())
		{
			if (sc.Compare("ActionList"))
			{
				sc.MustGetStringName("{");
				while (!sc.CheckString("}"))
				{
					sc.MustGetString();
					if (sc.Compare("NULL"))
					{
						Actions.Push(NULL);
					}
					else
					{
						// all relevant code pointers are either defined in AInventory 
						// or AActor so this will find all of them.
						FString name = "A_";
						name << sc.String;
						PSymbol *sym = RUNTIME_CLASS(AInventory)->Symbols.FindSymbol(name, true);
						if (sym == NULL || sym->SymbolType != SYM_ActionFunction)
						{
							sc.ScriptError("Unknown code pointer '%s'", sc.String);
						}
						else
						{
							FString &args = static_cast<PSymbolActionFunction*>(sym)->Arguments;
							if (args.Len()!=0 && (args[0]<'a' || args[0]>'z'))
							{
								sc.ScriptError("Incompatible code pointer '%s'", sc.String);
							}
						}
						Actions.Push(sym);
					}
					if (sc.CheckString("}")) break;
					sc.MustGetStringName(",");
				}
			}
			else if (sc.Compare("OrgHeights"))
			{
				sc.MustGetStringName("{");
				while (!sc.CheckString("}"))
				{
					sc.MustGetNumber();
					OrgHeights.Push(sc.Number);
					if (sc.CheckString("}")) break;
					sc.MustGetStringName(",");
				}
			}
			else if (sc.Compare("CodePConv"))
			{
				sc.MustGetStringName("{");
				while (!sc.CheckString("}"))
				{
					sc.MustGetNumber();
					CodePConv.Push(sc.Number);
					if (sc.CheckString("}")) break;
					sc.MustGetStringName(",");
				}
			}
			else if (sc.Compare("OrgSprNames"))
			{
				sc.MustGetStringName("{");
				while (!sc.CheckString("}"))
				{
					sc.MustGetString();
					DEHSprName s;
					// initialize with zeroes
					memset(&s, 0, sizeof(s));
					if (strlen(sc.String) ==4)
					{
						s.c[0] = sc.String[0];
						s.c[1] = sc.String[1];
						s.c[2] = sc.String[2];
						s.c[3] = sc.String[3];
						s.c[4] = 0;
					}
					else
					{
						sc.ScriptError("Invalid sprite name '%s' (must be 4 characters)", sc.String);
					}
					OrgSprNames.Push(s);
					if (sc.CheckString("}")) break;
					sc.MustGetStringName(",");
				}
			}
			else if (sc.Compare("StateMap"))
			{
				bool addit = StateMap.Size() == 0;

				sc.MustGetStringName("{");
				while (!sc.CheckString("}"))
				{
					StateMapper s;
					sc.MustGetString();

					const PClass *type = PClass::FindClass (sc.String);
					if (type == NULL)
					{
						sc.ScriptError ("Can't find type %s", sc.String);
					}
					else if (type->ActorInfo == NULL)
					{
						sc.ScriptError ("%s has no ActorInfo", sc.String);
					}

					sc.MustGetStringName(",");
					sc.MustGetString();
					s.State = type->ActorInfo->FindState(sc.String);
					if (s.State == NULL)
					{
						sc.ScriptError("Invalid state '%s' in '%s'", sc.String, type->TypeName.GetChars());
					}

					sc.MustGetStringName(",");
					sc.MustGetNumber();
					if (s.State == NULL || s.State + sc.Number > type->ActorInfo->OwnedStates + type->ActorInfo->NumOwnedStates)
					{
						sc.ScriptError("Invalid state range in '%s'", type->TypeName.GetChars());
					}
					AActor *def = GetDefaultByType(type);
					
					s.StateSpan = sc.Number;
					s.Owner = type;
					s.OwnerIsPickup = def != NULL && (def->flags & MF_SPECIAL) != 0;
					if (addit) StateMap.Push(s);

					if (sc.CheckString("}")) break;
					sc.MustGetStringName(",");
				}
			}
			else if (sc.Compare("SoundMap"))
			{
				sc.MustGetStringName("{");
				while (!sc.CheckString("}"))
				{
					sc.MustGetString();
					SoundMap.Push(S_FindSound(sc.String));
					if (sc.CheckString("}")) break;
					sc.MustGetStringName(",");
				}
			}
			else if (sc.Compare("InfoNames"))
			{
				sc.MustGetStringName("{");
				while (!sc.CheckString("}"))
				{
					sc.MustGetString();
					const PClass *cls = PClass::FindClass(sc.String);
					if (cls == NULL)
					{
						sc.ScriptError("Unknown actor type '%s'", sc.String);
					}
					InfoNames.Push(cls);
					if (sc.CheckString("}")) break;
					sc.MustGetStringName(",");
				}
			}
			else if (sc.Compare("ThingBits"))
			{
				sc.MustGetStringName("{");
				while (!sc.CheckString("}"))
				{
					BitName bit;
					sc.MustGetNumber();
					if (sc.Number < 0 || sc.Number > 31)
					{
						sc.ScriptError("Invalid bit value %d", sc.Number);
					}
					bit.Bit = sc.Number;
					sc.MustGetStringName(",");
					sc.MustGetNumber();
					if (sc.Number < 0 || sc.Number > 2)
					{
						sc.ScriptError("Invalid flag word %d", sc.Number);
					}
					bit.WhichFlags = sc.Number;
					sc.MustGetStringName(",");
					sc.MustGetString();
					strncpy(bit.Name, sc.String, 19);
					bit.Name[19]=0;
					BitNames.Push(bit);
					if (sc.CheckString("}")) break;
					sc.MustGetStringName(",");
				}
			}
			else if (sc.Compare("RenderStyles"))
			{
				sc.MustGetStringName("{");
				while (!sc.CheckString("}"))
				{
					StyleName style;
					sc.MustGetNumber();
					style.Num = sc.Number;
					sc.MustGetStringName(",");
					sc.MustGetString();
					strncpy(style.Name, sc.String, 19);
					style.Name[19]=0;
					StyleNames.Push(style);
					if (sc.CheckString("}")) break;
					sc.MustGetStringName(",");
				}
			}
			else if (sc.Compare("AmmoNames"))
			{
				sc.MustGetStringName("{");
				while (!sc.CheckString("}"))
				{
					sc.MustGetString();
					if (sc.Compare("NULL"))
					{
						AmmoNames.Push(NULL);
					}
					else
					{
						const PClass *cls = PClass::FindClass(sc.String);
						if (cls == NULL || cls->ParentClass != RUNTIME_CLASS(AAmmo))
						{
							sc.ScriptError("Unknown ammo type '%s'", sc.String);
						}
						AmmoNames.Push(cls);
					}
					if (sc.CheckString("}")) break;
					sc.MustGetStringName(",");
				}
			}
			else if (sc.Compare("WeaponNames"))
			{
				WeaponNames.Clear();	// This won't be cleared by UnloadDEHSupp so we need to do it here explicitly
				sc.MustGetStringName("{");
				while (!sc.CheckString("}"))
				{
					sc.MustGetString();
					const PClass *cls = PClass::FindClass(sc.String);
					if (cls == NULL || !cls->IsDescendantOf(RUNTIME_CLASS(AWeapon)))
					{
						sc.ScriptError("Unknown weapon type '%s'", sc.String);
					}
					WeaponNames.Push(cls);
					if (sc.CheckString("}")) break;
					sc.MustGetStringName(",");
				}
			}
			else if (sc.Compare("Aliases"))
			{
				sc.MustGetStringName("{");
				while (!sc.CheckString("}"))
				{
					CodePointerAlias temp;
					sc.MustGetString();
					strncpy(temp.alias, sc.String, 19);
					temp.alias[19]=0;
					sc.MustGetStringName(",");
					sc.MustGetString();
					strncpy(temp.name, sc.String, 19);
					temp.name[19]=0;
					sc.MustGetStringName(",");
					sc.MustGetNumber();
					temp.params = sc.Number;
					MBFCodePointers.Push(temp);
					if (sc.CheckString("}")) break;
					sc.MustGetStringName(",");
				}
			}
			else
			{
				sc.ScriptError("Unknown section '%s'", sc.String);
			}

			sc.MustGetStringName(";");
		}
		return true;
	}
	catch(CRecoverableError &err)
	{
		// Don't abort if DEHSUPP loading fails. 
		// Just print the message and continue.
		Printf("%s\n", err.GetMessage());
		return false;
	}
}

void FinishDehPatch ()
{
	unsigned int touchedIndex;

	for (touchedIndex = 0; touchedIndex < TouchedActors.Size(); ++touchedIndex)
	{
		PClass *type = TouchedActors[touchedIndex];
		AActor *defaults1 = GetDefaultByType (type);
		if (!(defaults1->flags & MF_SPECIAL))
		{ // We only need to do this for pickups
			continue;
		}

		// Create a new class that will serve as the actual pickup
		char typeNameBuilder[32];
		mysnprintf (typeNameBuilder, countof(typeNameBuilder), "DehackedPickup%d", touchedIndex);
		PClass *subclass = RUNTIME_CLASS(ADehackedPickup)->CreateDerivedClass
			(typeNameBuilder, sizeof(ADehackedPickup));
		AActor *defaults2 = GetDefaultByType (subclass);
		memcpy ((void *)defaults2, (void *)defaults1, sizeof(AActor));

		// Make a copy of the replaced class's state labels 
		FStateDefinitions statedef;
		statedef.MakeStateDefines(type);

		if (!type->IsDescendantOf(RUNTIME_CLASS(AInventory)))
		{
			// If this is a hacked non-inventory item we must also copy AInventory's special states
			statedef.AddStateDefines(RUNTIME_CLASS(AInventory)->ActorInfo->StateList);
		}
		statedef.InstallStates(subclass->ActorInfo, defaults2);

		// Use the DECORATE replacement feature to redirect all spawns
		// of the original class to the new one.
		FActorInfo *old_replacement = type->ActorInfo->Replacement;

		type->ActorInfo->Replacement = subclass->ActorInfo;
		subclass->ActorInfo->Replacee = type->ActorInfo;
		// If this actor was already replaced by another actor, copy that
		// replacement over to this item.
		if (old_replacement != NULL)
		{
			subclass->ActorInfo->Replacement = old_replacement;
		}

		DPrintf ("%s replaces %s\n", subclass->TypeName.GetChars(), type->TypeName.GetChars());
	}

	// Now that all Dehacked patches have been processed, it's okay to free StateMap.
	StateMap.Clear();
	StateMap.ShrinkToFit();
	TouchedActors.Clear();
	TouchedActors.ShrinkToFit();

	// Now it gets nasty: We have to fiddle around with the weapons' ammo use info to make Doom's original
	// ammo consumption work as intended.

	for(unsigned i = 0; i < WeaponNames.Size(); i++)
	{
		AWeapon *weap = (AWeapon*)GetDefaultByType(WeaponNames[i]);
		bool found = false;
		if (weap->flags6 & MF6_INTRYMOVE)
		{
			// Weapon sets an explicit amount of ammo to use so we won't need any special processing here
			weap->flags6 &= ~MF6_INTRYMOVE;
		}
		else
		{
			weap->WeaponFlags |= WIF_DEHAMMO;
			weap->AmmoUse1 = 0;
			// to allow proper checks in CheckAmmo we have to find the first attack pointer in the Fire sequence
			// and set its default ammo use as the weapon's AmmoUse1.

			TMap<FState*, bool> StateVisited;

			FState *state = WeaponNames[i]->ActorInfo->FindState(NAME_Fire);
			while (state != NULL)
			{
				bool *check = StateVisited.CheckKey(state);
				if (check != NULL && *check)
				{
					break;	// State has already been checked so we reached a loop
				}
				StateVisited[state] = true;
				for(unsigned j = 0; AmmoPerAttacks[j].func != NULL; j++)
				{
					if (state->ActionFunc == AmmoPerAttacks[j].func)
					{
						found = true;
						int use = AmmoPerAttacks[j].ammocount;
						if (use < 0) use = deh.BFGCells;
						weap->AmmoUse1 = use;
						break;
					}
				}
				if (found) break;
				state = state->GetNextState();
			}
		}
	}
	WeaponNames.Clear();
	WeaponNames.ShrinkToFit();
}

void ModifyDropAmount(AInventory *inv, int dropamount);

bool ADehackedPickup::TryPickup (AActor *&toucher)
{
	const PClass *type = DetermineType ();
	if (type == NULL)
	{
		return false;
	}
	RealPickup = static_cast<AInventory *>(Spawn (type, x, y, z, NO_REPLACE));
	if (RealPickup != NULL)
	{
		// The internally spawned item should never count towards statistics.
		RealPickup->ClearCounters();
		if (!(flags & MF_DROPPED))
		{
			RealPickup->flags &= ~MF_DROPPED;
		}
		// If this item has been dropped by a monster the
		// amount of ammo this gives must be adjusted.
		if (droppedbymonster)
		{
			ModifyDropAmount(RealPickup, 0);
		}
		if (!RealPickup->CallTryPickup (toucher))
		{
			RealPickup->Destroy ();
			RealPickup = NULL;
			return false;
		}

		// [BC] Tell the client that he successfully picked up the item.
		if (( NETWORK_GetState( ) == NETSTATE_SERVER ) &&
			( toucher->player ))
		{
			// [BB] Since SERVERCOMMANDS_GiveInventory overwrites the RealPickup amount
			// of the client with RealPickup->Amount, we have have to set this to the
			// correct amount the player has.
			AInventory *pInventory = NULL;
			if ( toucher->player->mo )
				pInventory = toucher->player->mo->FindInventory( type );

			if ( pInventory != NULL )
				RealPickup->Amount = pInventory->Amount;

			SERVERCOMMANDS_GiveInventory( ULONG( toucher->player - players ), RealPickup );

			if (( ItemFlags & IF_QUIET ) == false )
				SERVERCOMMANDS_DoInventoryPickup( ULONG( toucher->player - players ), RealPickup->GetClass( )->TypeName.GetChars( ), RealPickup->PickupMessage( ));
		}

		GoAwayAndDie ();
		return true;
	}
	return false;
}

const char *ADehackedPickup::PickupMessage ()
{
	return RealPickup->PickupMessage ();
}

bool ADehackedPickup::ShouldStay ()
{
	return RealPickup->ShouldStay ();
}

bool ADehackedPickup::ShouldRespawn ()
{
	return RealPickup->ShouldRespawn ();
}

void ADehackedPickup::PlayPickupSound (AActor *toucher)
{
	RealPickup->PlayPickupSound (toucher);
}

void ADehackedPickup::DoPickupSpecial (AActor *toucher)
{
	Super::DoPickupSpecial (toucher);
	// If the real pickup hasn't joined the toucher's inventory, make sure it
	// doesn't stick around.
	if (RealPickup->Owner != toucher)
	{
		RealPickup->Destroy ();
	}
	RealPickup = NULL;
}

void ADehackedPickup::Destroy ()
{
	if (RealPickup != NULL)
	{
		RealPickup->Destroy ();
		RealPickup = NULL;
	}
	Super::Destroy ();
}

const PClass *ADehackedPickup::DetermineType ()
{
	// Look at the actor's current sprite to determine what kind of
	// item to pretend to me.
	int min = 0;
	int max = countof(DehSpriteMappings) - 1;

	while (min <= max)
	{
		int mid = (min + max) / 2;
		int lex = memcmp (DehSpriteMappings[mid].Sprite, sprites[sprite].name, 4);
		if (lex == 0)
		{
			return PClass::FindClass (DehSpriteMappings[mid].ClassName);
		}
		else if (lex < 0)
		{
			min = mid + 1;
		}
		else
		{
			max = mid - 1;
		}
	}
	return NULL;
}

void ADehackedPickup::Serialize(FArchive &arc)
{
	Super::Serialize(arc);
	arc << droppedbymonster;
}
