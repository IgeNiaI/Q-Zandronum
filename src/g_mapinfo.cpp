/*
** g_level.cpp
** Parses MAPINFO
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
** Copyright 2009 Christoph Oelckers
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

#include <assert.h>
#include "templates.h"
#include "g_level.h"
#include "sc_man.h"
#include "w_wad.h"
#include "cmdlib.h"
#include "v_video.h"
#include "p_lnspec.h"
#include "p_setup.h"
#include "i_system.h"
#include "gi.h"
#include "gstrings.h"
#include "farchive.h"
#include "p_acs.h"
#include "doomstat.h"
#include "d_player.h"
#include "autosegs.h"
#include "version.h"
#include "v_text.h"

TArray<cluster_info_t> wadclusterinfos;
TArray<level_info_t> wadlevelinfos;

level_info_t TheDefaultLevelInfo;
static cluster_info_t TheDefaultClusterInfo;

int botepisodei = 0;
TArray<FEpisode> AllEpisodes;

extern TMap<int, FString> HexenMusic;

//==========================================================================
//
//
//==========================================================================

int FindWadLevelInfo (const char *name)
{
	for (unsigned int i = 0; i < wadlevelinfos.Size(); i++)
		if (!strnicmp (name, wadlevelinfos[i].mapname, 8))
			return i;
		
	return -1;
}

//==========================================================================
//
//
//==========================================================================

level_info_t *FindLevelInfo (const char *mapname, bool allowdefault)
{
	int i;

	if ((i = FindWadLevelInfo (mapname)) > -1)
	{
		return &wadlevelinfos[i];
	}
	else if (allowdefault)
	{
		if (TheDefaultLevelInfo.LevelName.IsEmpty())
		{
			uppercopy(TheDefaultLevelInfo.skypic1, "SKY1");
			uppercopy(TheDefaultLevelInfo.skypic2, "SKY1");
			TheDefaultLevelInfo.LevelName = "Unnamed";
		}
		return &TheDefaultLevelInfo;
	}
	return NULL;
}
//==========================================================================
//
// [RC] Finds a level given its name.
// Like FindLevelInfo but returns NULL if the level wasn't found.
//
//==========================================================================

level_info_t *FindLevelByName( const char *mapname )
{
	int i = FindWadLevelInfo( mapname );
	if ( i > -1 )
		return &wadlevelinfos[i];
	else
		return NULL;
}

//==========================================================================
//
//
//==========================================================================

level_info_t *FindLevelByNum (int num)
{
	for (unsigned int i = 0; i < wadlevelinfos.Size(); i++)
		if (wadlevelinfos[i].levelnum == num)
			return &wadlevelinfos[i];

	return NULL;
}


//==========================================================================
//
//
//==========================================================================

static level_info_t *FindLevelByWarpTrans (int num)
{
	for (unsigned i = wadlevelinfos.Size(); i-- != 0; )
		if (wadlevelinfos[i].WarpTrans == num)
			return &wadlevelinfos[i];

	return NULL;
}

//==========================================================================
//
//
//==========================================================================

bool CheckWarpTransMap (FString &mapname, bool substitute)
{
	if (mapname[0] == '&' && (mapname[1] & 0xDF) == 'W' &&
		(mapname[2] & 0xDF) == 'T' && mapname[3] == '@')
	{
		level_info_t *lev = FindLevelByWarpTrans (atoi (&mapname[4]));
		if (lev != NULL)
		{
			mapname = lev->mapname;
			return true;
		}
		else if (substitute)
		{
			char a = mapname[4], b = mapname[5];
			mapname = "MAP";
			mapname << a << b;
		}
	}
	return false;
}

//==========================================================================
//
//
//==========================================================================

static int FindWadClusterInfo (int cluster)
{
	for (unsigned int i = 0; i < wadclusterinfos.Size(); i++)
		if (wadclusterinfos[i].cluster == cluster)
			return i;
		
	return -1;
}

//==========================================================================
//
//
//==========================================================================

cluster_info_t *FindClusterInfo (int cluster)
{
	int i;

	if ((i = FindWadClusterInfo (cluster)) > -1)
		return &wadclusterinfos[i];
	else
		return &TheDefaultClusterInfo;
}

//==========================================================================
//
//
//==========================================================================

void G_ClearSnapshots (void)
{
	for (unsigned int i = 0; i < wadlevelinfos.Size(); i++)
	{
		wadlevelinfos[i].ClearSnapshot();
	}
	// Since strings are only locked when snapshotting a level, unlock them
	// all now, since we got rid of all the snapshots that cared about them.
	GlobalACSStrings.UnlockAll();
}

//==========================================================================
//
// Remove any existing defereds
//
//==========================================================================

void P_RemoveDefereds (void)
{
	for (unsigned int i = 0; i < wadlevelinfos.Size(); i++)
	{
		wadlevelinfos[i].ClearDefered();
	}
}


//==========================================================================
//
//
//==========================================================================

void level_info_t::Reset()
{
	mapname[0] = 0;
	mapbg[0] = 0;
	levelnum = 0;
	pname[0] = 0;
	nextmap[0] = 0;
	secretmap[0] = 0;
	campaignhubmap[0] = 0;
	strcpy (skypic1, "-NOFLAT-");
	strcpy (skypic2, "-NOFLAT-");
	cluster = 0;
	partime = 0;
	sucktime = 0;
	flags = 0;
	if (gameinfo.gametype == GAME_Hexen)
		flags2 = 0;
	else
		flags2 = LEVEL2_LAXMONSTERACTIVATION;
	// [BB]
	flagsZA = 0;
	Music = "";
	LevelName = "";
	strcpy (fadetable, "COLORMAP");
	WallHorizLight = -8;
	WallVertLight = +8;
	f1[0] = 0;
	musicorder = 0;
	snapshot = NULL;
	snapshotVer = 0;
	defered = 0;
	skyspeed1 = skyspeed2 = 0.f;
	fadeto = 0;
	outsidefog = 0xff000000;
	cdtrack = 0;
	cdid = 0;
	gravity = 0.f;
	aircontrol = 0.f;
	WarpTrans = 0;
	airsupply = 20;
	compatflags = compatflags2 = 0;
	compatmask = compatmask2 = 0;
	Translator = "";
	RedirectType = 0;
	RedirectMap[0] = 0;
	PreviewPic = "NOMAPPIC";
	EnterPic = "";
	ExitPic = "";
	InterMusic = "";
	intermusicorder = 0;
	SoundInfo = "";
	SndSeq = "";
	bordertexture[0] = 0;
	teamdamage = 0.f;
	specialactions.Clear();
	DefaultEnvironment = 0;
	PrecacheSounds.Clear();
	categoryflags = MAPCATEGORYF_0;
}


//==========================================================================
//
//
//==========================================================================

FString level_info_t::LookupLevelName()
{
	if (flags & LEVEL_LOOKUPLEVELNAME)
	{
		const char *thename;
		const char *lookedup;

		lookedup = GStrings[LevelName];
		if (lookedup == NULL)
		{
			thename = LevelName;
		}
		else
		{
			char checkstring[32];

			// Strip out the header from the localized string
			if (mapname[0] == 'E' && mapname[2] == 'M')
			{
				mysnprintf (checkstring, countof(checkstring), "%s: ", mapname);
			}
			else if (mapname[0] == 'M' && mapname[1] == 'A' && mapname[2] == 'P')
			{
				mysnprintf (checkstring, countof(checkstring), "%d: ", atoi(mapname + 3));
			}
			else if (mapname[0] == 'L' && mapname[1] == 'E' && mapname[2] == 'V' && mapname[3] == 'E' && mapname[4] == 'L')
			{
				mysnprintf (checkstring, countof(checkstring), "%d: ", atoi(mapname + 5));
			}
			else
			{
				checkstring[0] = '\0';
			}
			thename = strstr (lookedup, checkstring);
			if (thename == NULL)
			{
				thename = lookedup;
			}
			else
			{
				thename += strlen (checkstring);
			}
		}
		return thename;
	}
	else return LevelName;
}


//==========================================================================
//
//
//==========================================================================

void level_info_t::ClearSnapshot()
{
	if (snapshot != NULL) delete snapshot;
	snapshot = NULL;
}

//==========================================================================
//
//
//==========================================================================

void level_info_t::ClearDefered()
{
	acsdefered_t *def = defered;
	while (def)
	{
		acsdefered_t *next = def->next;
		delete def;
		def = next;
	}
	defered = NULL;
}

//==========================================================================
//
//
//==========================================================================

level_info_t *level_info_t::CheckLevelRedirect ()
{
	if (RedirectType != NAME_None)
	{
		const PClass *type = PClass::FindClass(RedirectType);
		if (type != NULL)
		{
			for (int i = 0; i < MAXPLAYERS; ++i)
			{
				if (playeringame[i] && players[i].mo->FindInventory (type))
				{
					// check for actual presence of the map.
					if (P_CheckMapData(RedirectMap))
					{
						return FindLevelInfo(RedirectMap);
					}
					break;
				}
			}
		}
	}
	return NULL;
}

//==========================================================================
//
//
//==========================================================================

bool level_info_t::isValid()
{
	return mapname[0] != 0 || this == &TheDefaultLevelInfo;
}

//==========================================================================
//
//
//==========================================================================

void cluster_info_t::Reset()
{
	cluster = 0;
	FinaleFlat = "";
	ExitText = "";
	EnterText = "";
	MessageMusic = "";
	musicorder = 0;
	flags = 0;
	cdtrack = 0;
	ClusterName = "";
	cdid = 0;
}



//==========================================================================
//
//
//==========================================================================

void FMapInfoParser::ParseOpenBrace()
{
	switch(format_type)
	{
	default:
		format_type = sc.CheckString("{") ? FMT_New : FMT_Old;
		if (format_type == FMT_New) 
			sc.SetCMode(true);
		break;

	case FMT_Old:
		break;

	case FMT_New:
		sc.MustGetStringName("{");
		sc.SetCMode(true);
		break;
	}
}

//==========================================================================
//
//
//==========================================================================

bool FMapInfoParser::ParseCloseBrace()
{
	if (format_type == FMT_New)
	{
		return sc.Compare("}");
	}
	else
	{
		// We have to assume that the next keyword
		// starts a new top level block
		sc.UnGet();
		return true;
	}
}

//==========================================================================
//
//
//==========================================================================

bool FMapInfoParser::CheckAssign()
{
	if (format_type == FMT_New) return sc.CheckString("=");
	else return false;	// force explicit handling
}

//==========================================================================
//
//
//==========================================================================

void FMapInfoParser::ParseAssign()
{
	if (format_type == FMT_New) sc.MustGetStringName("=");
}

//==========================================================================
//
//
//==========================================================================

void FMapInfoParser::MustParseAssign()
{
	if (format_type == FMT_New) sc.MustGetStringName("=");
	else sc.ScriptError(NULL);
}

//==========================================================================
//
//
//==========================================================================

void FMapInfoParser::ParseComma()
{
	if (format_type == FMT_New) sc.MustGetStringName(",");
}

//==========================================================================
//
//
//==========================================================================

bool FMapInfoParser::CheckNumber()
{
	if (format_type == FMT_New) 
	{
		if (sc.CheckString(","))
		{
			sc.MustGetNumber();
			return true;
		}
		return false;
	}
	else return sc.CheckNumber();
}

//==========================================================================
//
//
//==========================================================================

bool FMapInfoParser::CheckFloat()
{
	if (format_type == FMT_New) 
	{
		if (sc.CheckString(","))
		{
			sc.MustGetFloat();
			return true;
		}
		return false;
	}
	else return sc.CheckFloat();
}

//==========================================================================
//
// skips an entire parameter list that's separated by commas
//
//==========================================================================

void FMapInfoParser::SkipToNext()
{
	if (sc.CheckString("="))
	{
		do
		{
			sc.MustGetString();
		}
		while (sc.CheckString(","));
	}
}


//==========================================================================
//
// checks if the current block was properly terminated
//
//==========================================================================

void FMapInfoParser::CheckEndOfFile(const char *block)
{
	if (format_type == FMT_New && !sc.Compare("}"))
	{
		sc.ScriptError("Unexpected end of file in %s definition", block);
	}
}

//==========================================================================
//
// ParseLookupname
// Parses a string that may reference the string table
//
//==========================================================================

bool FMapInfoParser::ParseLookupName(FString &dest)
{
	sc.MustGetString();
	if (sc.Compare("lookup"))
	{
		ParseComma();
		sc.MustGetString();
		dest = sc.String;
		return true;
	}
	else if (sc.String[0] == '$')
	{
		dest = sc.String+1;
		return true;
	}
	else if (format_type == FMT_Old)
	{
		dest = sc.String;
		return false;
	}
	else
	{
		sc.UnGet();
		dest = "";
		do
		{
			sc.MustGetString();
			dest << sc.String << '\n';
		}
		while (sc.CheckString(","));
		// strip off the last newline
		dest.Truncate(long(dest.Len()-1));
		return false;
	}
}

//==========================================================================
//
//
//==========================================================================

void FMapInfoParser::ParseLumpOrTextureName(char *name)
{
	sc.MustGetString();
	uppercopy(name, sc.String);
	name[8]=0;
}

void FMapInfoParser::ParseLumpOrTextureName(FString &name)
{
	sc.MustGetString();
	name = sc.String;
}


//==========================================================================
//
//
//==========================================================================

void FMapInfoParser::ParseMusic(FString &name, int &order)
{
	sc.MustGetString();

	order = 0;
	char *colon = strchr (sc.String, ':');
	if (colon)
	{
		order = atoi(colon+1);
		*colon = 0;
	}
	name = sc.String;
	if (!colon && CheckNumber())
	{
		order = sc.Number;
	}
}

//==========================================================================
//
// ParseCluster
// Parses a cluster definition
//
//==========================================================================

void FMapInfoParser::ParseCluster()
{
	sc.MustGetNumber ();
	int clusterindex = FindWadClusterInfo (sc.Number);
	if (clusterindex == -1)
	{
		clusterindex = wadclusterinfos.Reserve(1);
	}

	cluster_info_t *clusterinfo = &wadclusterinfos[clusterindex];
	clusterinfo->Reset();
	clusterinfo->cluster = sc.Number;

	ParseOpenBrace();

	while (sc.GetString())
	{
		if (sc.Compare("name"))
		{
			ParseAssign();
			if (ParseLookupName(clusterinfo->ClusterName))
				clusterinfo->flags |= CLUSTER_LOOKUPCLUSTERNAME;
		}
		else if (sc.Compare("entertext"))
		{
			ParseAssign();
			if (ParseLookupName(clusterinfo->EnterText))
				clusterinfo->flags |= CLUSTER_LOOKUPENTERTEXT;
		}
		else if (sc.Compare("exittext"))
		{
			ParseAssign();
			if (ParseLookupName(clusterinfo->ExitText))
				clusterinfo->flags |= CLUSTER_LOOKUPEXITTEXT;
		}
		else if (sc.Compare("music"))
		{
			int order = 0;

			ParseAssign();
			ParseMusic(clusterinfo->MessageMusic, clusterinfo->musicorder);
		}
		else if (sc.Compare("flat"))
		{
			ParseAssign();
			ParseLumpOrTextureName(clusterinfo->FinaleFlat);
		}
		else if (sc.Compare("pic"))
		{
			ParseAssign();
			ParseLumpOrTextureName(clusterinfo->FinaleFlat);
			clusterinfo->flags |= CLUSTER_FINALEPIC;
		}
		else if (sc.Compare("hub"))
		{
			clusterinfo->flags |= CLUSTER_HUB;
		}
		else if (sc.Compare("cdtrack"))
		{
			ParseAssign();
			sc.MustGetNumber();
			clusterinfo->cdtrack = sc.Number;
		}
		else if (sc.Compare("cdid"))
		{
			ParseAssign();
			sc.MustGetString();
			clusterinfo->cdid = strtoul (sc.String, NULL, 16);
		}
		else if (sc.Compare("entertextislump"))
		{
			clusterinfo->flags |= CLUSTER_ENTERTEXTINLUMP;
		}
		else if (sc.Compare("exittextislump"))
		{
			clusterinfo->flags |= CLUSTER_EXITTEXTINLUMP;
		}
		else if (!ParseCloseBrace())
		{
			// Unknown
			sc.ScriptMessage("Unknown property '%s' found in map definition\n", sc.String);
			SkipToNext();
		}
		else
		{
			break;
		}
	}
	CheckEndOfFile("cluster");
}


//==========================================================================
//
// ParseNextMap
// Parses a next map field
//
//==========================================================================

void FMapInfoParser::ParseNextMap(char *mapname)
{
	if (sc.CheckNumber())
	{
		if (HexenHack)
		{
			mysnprintf (mapname, 9, "&wt@%02d", sc.Number);
		}
		else
		{
			mysnprintf (mapname, 9, "MAP%02d", sc.Number);
		}
	}
	else
	{
		*mapname = 0;
		sc.MustGetString();
		strncpy (mapname, sc.String, 8);
		mapname[8] = 0;
		FName seq = CheckEndSequence();
		if (seq != NAME_None)
		{
			mysnprintf(mapname, 11, "enDSeQ%04x", int(seq));
		}
		// [BB] If we're in a multiplayer game, don't do the finale, just go back to the
		// beginning.
		else if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			if ( strnicmp( mapname, "EndGame", 7 ) == 0 )
			{
				switch ( mapname[7] )
				{
				case '1':	strcpy( mapname, "E1M1" );	break;
				case '2':	strcpy( mapname, "E2M1" );	break;
				case '3':	strcpy( mapname, "E3M1" );	break;
				case '4':	strcpy( mapname, "E4M1" );	break;
				case 'C':	strcpy( mapname, "MAP01" );	break;
//				case 'W':	type = END_Underwater;	break;
//				case 'S':	type = END_Strife;		break;
				default:	strcpy( mapname, "MAP01" );	break;
				}
			}

			// [BB] In a multiplayer game, we can't go to the title either.
			if ( ( stricmp (mapname, "EndTitle") == 0 ) && ( AllEpisodes.Size() > 0 ) )
				strncpy( mapname, AllEpisodes[0].mEpisodeMap.GetChars(), 8 );
		}
	}
}

//==========================================================================
//
// Map options
//
//==========================================================================

DEFINE_MAP_OPTION(levelnum, true)
{
	parse.ParseAssign();
	parse.sc.MustGetNumber();
	info->levelnum = parse.sc.Number;
}

DEFINE_MAP_OPTION(next, true)
{
	parse.ParseAssign();
	parse.ParseNextMap(info->nextmap);
}

DEFINE_MAP_OPTION(secretnext, true)
{
	parse.ParseAssign();
	parse.ParseNextMap(info->secretmap);
}

DEFINE_MAP_OPTION(secret, true) // Just an alias for secretnext, for Vavoom compatibility
{
	parse.ParseAssign();
	parse.ParseNextMap(info->secretmap);
}

DEFINE_MAP_OPTION(campaignhub, true)
{
	parse.ParseAssign();
	parse.ParseNextMap(info->campaignhubmap);
}

DEFINE_MAP_OPTION(cluster, true)
{
	parse.ParseAssign();
	parse.sc.MustGetNumber();
	info->cluster = parse.sc.Number;

	// If this cluster hasn't been defined yet, add it. This is especially needed
	// for Hexen, because it doesn't have clusterdefs. If we don't do this, every
	// level on Hexen will sometimes be considered as being on the same hub,
	// depending on the check done.
	if (FindWadClusterInfo (parse.sc.Number) == -1)
	{
		unsigned int clusterindex = wadclusterinfos.Reserve(1);
		cluster_info_t *clusterinfo = &wadclusterinfos[clusterindex];
		clusterinfo->Reset();
		clusterinfo->cluster = parse.sc.Number;
		if (parse.HexenHack)
		{
			clusterinfo->flags |= CLUSTER_HUB;
		}
	}
}

DEFINE_MAP_OPTION(sky1, true)
{
	parse.ParseAssign();
	parse.ParseLumpOrTextureName(info->skypic1);
	if (parse.CheckFloat())
	{
		if (parse.HexenHack)
		{
			parse.sc.Float /= 256;
		}
		info->skyspeed1 = float(parse.sc.Float * (35. / 1000.));
	}
}

DEFINE_MAP_OPTION(sky2, true)
{
	parse.ParseAssign();
	parse.ParseLumpOrTextureName(info->skypic2);
	if (parse.CheckFloat())
	{
		if (parse.HexenHack)
		{
			parse.sc.Float /= 256;
		}
		info->skyspeed2 = float(parse.sc.Float * (35. / 1000.));
	}
}

// Vavoom compatibility
DEFINE_MAP_OPTION(skybox, true)
{
	parse.ParseAssign();
	parse.ParseLumpOrTextureName(info->skypic1);
	info->skyspeed1 = 0;
}

DEFINE_MAP_OPTION(fade, true)
{
	parse.ParseAssign();
	parse.sc.MustGetString();
	info->fadeto = V_GetColor(NULL, parse.sc.String);
}

DEFINE_MAP_OPTION(outsidefog, true)
{
	parse.ParseAssign();
	parse.sc.MustGetString();
	info->outsidefog = V_GetColor(NULL, parse.sc.String);
}

DEFINE_MAP_OPTION(titlepatch, true)
{
	parse.ParseAssign();
	parse.ParseLumpOrTextureName(info->pname);
}

DEFINE_MAP_OPTION(partime, true)
{
	parse.ParseAssign();
	parse.sc.MustGetNumber();
	info->partime = parse.sc.Number;
}

DEFINE_MAP_OPTION(par, true)
{
	parse.ParseAssign();
	parse.sc.MustGetNumber();
	info->partime = parse.sc.Number;
}

DEFINE_MAP_OPTION(sucktime, true)
{
	parse.ParseAssign();
	parse.sc.MustGetNumber();
	info->sucktime = parse.sc.Number;
}

DEFINE_MAP_OPTION(music, true)
{
	parse.ParseAssign();
	parse.ParseMusic(info->Music, info->musicorder);
}

DEFINE_MAP_OPTION(intermusic, true)
{
	parse.ParseAssign();
	parse.ParseMusic(info->InterMusic, info->intermusicorder);
}

DEFINE_MAP_OPTION(fadetable, true)
{
	parse.ParseAssign();
	parse.ParseLumpOrTextureName(info->fadetable);
}

DEFINE_MAP_OPTION(evenlighting, true)
{
	info->WallVertLight = info->WallHorizLight = 0;
}

DEFINE_MAP_OPTION(cdtrack, true)
{
	parse.ParseAssign();
	parse.sc.MustGetNumber();
	info->cdtrack = parse.sc.Number;
}

DEFINE_MAP_OPTION(cdid, true)
{
	parse.ParseAssign();
	parse.sc.MustGetString();
	info->cdid = strtoul (parse.sc.String, NULL, 16);
}

DEFINE_MAP_OPTION(warptrans, true)
{
	parse.ParseAssign();
	parse.sc.MustGetNumber();
	info->WarpTrans = parse.sc.Number;
}

DEFINE_MAP_OPTION(vertwallshade, true)
{
	parse.ParseAssign();
	parse.sc.MustGetNumber();
	info->WallVertLight = (SBYTE)clamp (parse.sc.Number / 2, -128, 127);
}

DEFINE_MAP_OPTION(horizwallshade, true)
{
	parse.ParseAssign();
	parse.sc.MustGetNumber();
	info->WallHorizLight = (SBYTE)clamp (parse.sc.Number / 2, -128, 127);
}

DEFINE_MAP_OPTION(gravity, true)
{
	parse.ParseAssign();
	parse.sc.MustGetFloat();
	info->gravity = float(parse.sc.Float);
}

DEFINE_MAP_OPTION(aircontrol, true)
{
	parse.ParseAssign();
	parse.sc.MustGetFloat();
	info->aircontrol = float(parse.sc.Float);
}

DEFINE_MAP_OPTION(airsupply, true)
{
	parse.ParseAssign();
	parse.sc.MustGetNumber();
	info->airsupply = parse.sc.Number;
}

DEFINE_MAP_OPTION(previewpic, true)
{
	parse.ParseAssign();
	parse.sc.MustGetString();
	info->PreviewPic = parse.sc.String;
}

DEFINE_MAP_OPTION(interpic, true)
{
	parse.ParseAssign();
	parse.sc.MustGetString();
	info->ExitPic = parse.sc.String;
}

DEFINE_MAP_OPTION(exitpic, true)
{
	parse.ParseAssign();
	parse.sc.MustGetString();
	info->ExitPic = parse.sc.String;
}

DEFINE_MAP_OPTION(enterpic, true)
{
	parse.ParseAssign();
	parse.sc.MustGetString();
	info->EnterPic = parse.sc.String;
}

DEFINE_MAP_OPTION(specialaction, true)
{
	parse.ParseAssign();

	FSpecialAction *sa = &info->specialactions[info->specialactions.Reserve(1)];
	int min_arg, max_arg;
	if (parse.format_type == parse.FMT_Old) parse.sc.SetCMode(true);
	parse.sc.MustGetString();
	sa->Type = FName(parse.sc.String);
	parse.sc.CheckString(",");
	parse.sc.MustGetString();
	sa->Action = P_FindLineSpecial(parse.sc.String, &min_arg, &max_arg);
	if (sa->Action == 0 || min_arg < 0)
	{
		parse.sc.ScriptError("Unknown specialaction '%s'", parse.sc.String);
	}
	int j = 0;
	while (j < 5 && parse.sc.CheckString(","))
	{
		parse.sc.MustGetNumber();
		sa->Args[j++] = parse.sc.Number;
	}
	if (parse.format_type == parse.FMT_Old) parse.sc.SetCMode(false);
}

DEFINE_MAP_OPTION(PrecacheSounds, true)
{
	parse.ParseAssign();

	do
	{
		parse.sc.MustGetString();
		FSoundID snd = parse.sc.String;
		if (snd == 0)
		{
			parse.sc.ScriptMessage("Unknown sound \"%s\"", parse.sc.String);
		}
		else
		{
			info->PrecacheSounds.Push(snd);
		}
	} while (parse.sc.CheckString(","));
}

DEFINE_MAP_OPTION(redirect, true)
{
	parse.ParseAssign();
	parse.sc.MustGetString();
	info->RedirectType = parse.sc.String;
	parse.ParseComma();
	parse.ParseLumpOrTextureName(info->RedirectMap);
}

DEFINE_MAP_OPTION(sndseq, true)
{
	parse.ParseAssign();
	parse.sc.MustGetString();
	info->SndSeq = parse.sc.String;
}

DEFINE_MAP_OPTION(sndinfo, true)
{
	parse.ParseAssign();
	parse.sc.MustGetString();
	info->SoundInfo = parse.sc.String;
}

DEFINE_MAP_OPTION(soundinfo, true)
{
	parse.ParseAssign();
	parse.sc.MustGetString();
	info->SoundInfo = parse.sc.String;
}

DEFINE_MAP_OPTION(translator, true)
{
	parse.ParseAssign();
	parse.sc.MustGetString();
	info->Translator = parse.sc.String;
}

DEFINE_MAP_OPTION(deathsequence, false)
{
	parse.ParseAssign();
	parse.sc.MustGetString();
	info->deathsequence = parse.sc.String;
}

DEFINE_MAP_OPTION(slideshow, false)
{
	parse.ParseAssign();
	parse.sc.MustGetString();
	info->slideshow = parse.sc.String;
}

DEFINE_MAP_OPTION(bordertexture, true)
{
	parse.ParseAssign();
	parse.ParseLumpOrTextureName(info->bordertexture);
}

DEFINE_MAP_OPTION(f1, true)
{
	parse.ParseAssign();
	parse.ParseLumpOrTextureName(info->f1);
}

DEFINE_MAP_OPTION(teamdamage, true)
{
	parse.ParseAssign();
	parse.sc.MustGetFloat();
	info->teamdamage = float(parse.sc.Float);
}

DEFINE_MAP_OPTION(mapbackground, true)
{
	parse.ParseAssign();
	parse.ParseLumpOrTextureName(info->mapbg);
}

DEFINE_MAP_OPTION(defaultenvironment, false)
{
	int id;

	parse.ParseAssign();
	if (parse.sc.CheckNumber())
	{ // Numeric ID XXX [, YYY]
		id = parse.sc.Number << 8;
		if (parse.CheckNumber())
		{
			id |= parse.sc.Number;
		}
	}
	else
	{ // Named environment
		parse.sc.MustGetString();
		ReverbContainer *reverb = S_FindEnvironment(parse.sc.String);
		if (reverb == NULL)
		{
			parse.sc.ScriptMessage("Unknown sound environment '%s'\n", parse.sc.String);
			id = 0;
		}
		else
		{
			id = reverb->ID;
		}
	}
	info->DefaultEnvironment = id;
}


#ifdef NO_GL
// Define these options so that Linux servers don't complain about unknown map definitions
DEFINE_MAP_OPTION(fogdensity, false)
{
	parse.ParseAssign();
	parse.sc.MustGetNumber();
}

DEFINE_MAP_OPTION(brightfog, false)
{
	parse.ParseAssign();
	parse.sc.MustGetNumber();
}

DEFINE_MAP_OPTION(outsidefogdensity, false)
{
	parse.ParseAssign();
	parse.sc.MustGetNumber();
}

DEFINE_MAP_OPTION(skyfog, false)
{
	parse.ParseAssign();
	parse.sc.MustGetNumber();
}

DEFINE_MAP_OPTION(lightmode, false)
{
	parse.ParseAssign();
	parse.sc.MustGetNumber();
}

DEFINE_MAP_OPTION(nocoloredspritelighting, false)
{
	if (parse.CheckAssign())
	{
		parse.sc.MustGetNumber();
	}
}

DEFINE_MAP_OPTION(notexturefill, false)
{
	if (parse.CheckAssign())
	{
		parse.sc.MustGetNumber();
	}
}

DEFINE_MAP_OPTION(skyrotate, false)
{
	parse.ParseAssign();
	parse.sc.MustGetFloat();
	if (parse.format_type == FMapInfoParser::FMT_New) parse.sc.MustGetStringName(",");
	parse.sc.MustGetFloat();
	if (parse.format_type == FMapInfoParser::FMT_New) parse.sc.MustGetStringName(",");
	parse.sc.MustGetFloat();
}

DEFINE_MAP_OPTION(skyrotate2, false)
{
	parse.ParseAssign();
	parse.sc.MustGetFloat();
	if (parse.format_type == FMapInfoParser::FMT_New) parse.sc.MustGetStringName(",");
	parse.sc.MustGetFloat();
	if (parse.format_type == FMapInfoParser::FMT_New) parse.sc.MustGetStringName(",");
	parse.sc.MustGetFloat();
}
#endif

//==========================================================================
//
// All flag based map options 
//
//==========================================================================

enum EMIType
{
	MITYPE_IGNORE,
	MITYPE_EATNEXT,
	MITYPE_SETFLAG,
	MITYPE_CLRFLAG,
	MITYPE_SCFLAGS,
	MITYPE_SETFLAG2,
	MITYPE_CLRFLAG2,
	MITYPE_SCFLAGS2,
	MITYPE_COMPATFLAG,
	MITYPE_SETFLAGZA, // [BB]
	MITYPE_CATEGORYFLAGS, // [geNia]
};

struct MapInfoFlagHandler
{
	const char *name;
	EMIType type;
	DWORD data1, data2;
}
MapFlagHandlers[] =
{
	{ "nointermission",					MITYPE_SETFLAG,	LEVEL_NOINTERMISSION, 0 },
	{ "intermission",					MITYPE_CLRFLAG,	LEVEL_NOINTERMISSION, 0 },
	{ "doublesky",						MITYPE_SETFLAG,	LEVEL_DOUBLESKY, 0 },
	{ "nosoundclipping",				MITYPE_IGNORE,	0, 0 },	// was nosoundclipping
	{ "allowmonstertelefrags",			MITYPE_SETFLAG,	LEVEL_MONSTERSTELEFRAG, 0 },
	{ "map07special",					MITYPE_SETFLAG,	LEVEL_MAP07SPECIAL, 0 },
	{ "baronspecial",					MITYPE_SETFLAG,	LEVEL_BRUISERSPECIAL, 0 },
	{ "cyberdemonspecial",				MITYPE_SETFLAG,	LEVEL_CYBORGSPECIAL, 0 },
	{ "spidermastermindspecial",		MITYPE_SETFLAG,	LEVEL_SPIDERSPECIAL, 0 },
	{ "minotaurspecial",				MITYPE_SETFLAG,	LEVEL_MINOTAURSPECIAL, 0 },
	{ "dsparilspecial",					MITYPE_SETFLAG,	LEVEL_SORCERER2SPECIAL, 0 },
	{ "ironlichspecial",				MITYPE_SETFLAG,	LEVEL_HEADSPECIAL, 0 },
	{ "specialaction_exitlevel",		MITYPE_SCFLAGS,	0, ~LEVEL_SPECACTIONSMASK },
	{ "specialaction_opendoor",			MITYPE_SCFLAGS,	LEVEL_SPECOPENDOOR, ~LEVEL_SPECACTIONSMASK },
	{ "specialaction_lowerfloor",		MITYPE_SCFLAGS,	LEVEL_SPECLOWERFLOOR, ~LEVEL_SPECACTIONSMASK },
	{ "specialaction_lowerfloortohighest",MITYPE_SCFLAGS,LEVEL_SPECLOWERFLOORTOHIGHEST, ~LEVEL_SPECACTIONSMASK },
	{ "specialaction_killmonsters",		MITYPE_SETFLAG,	LEVEL_SPECKILLMONSTERS, 0 },
	{ "lightning",						MITYPE_SETFLAG,	LEVEL_STARTLIGHTNING, 0 },
	{ "smoothlighting",					MITYPE_SETFLAG2,	LEVEL2_SMOOTHLIGHTING, 0 },
	{ "noautosequences",				MITYPE_SETFLAG,	LEVEL_SNDSEQTOTALCTRL, 0 },
	{ "autosequences",					MITYPE_CLRFLAG,	LEVEL_SNDSEQTOTALCTRL, 0 },
	{ "forcenoskystretch",				MITYPE_SETFLAG,	LEVEL_FORCENOSKYSTRETCH, 0 },
	{ "skystretch",						MITYPE_CLRFLAG,	LEVEL_FORCENOSKYSTRETCH, 0 },
	{ "allowfreelook",					MITYPE_SCFLAGS,	LEVEL_FREELOOK_YES, ~LEVEL_FREELOOK_NO },
	{ "nofreelook",						MITYPE_SCFLAGS,	LEVEL_FREELOOK_NO, ~LEVEL_FREELOOK_YES },
	{ "allowjump",						MITYPE_CLRFLAG,	LEVEL_JUMP_NO, 0 },
	{ "nojump",							MITYPE_SETFLAG,	LEVEL_JUMP_NO, 0 },
	{ "fallingdamage",					MITYPE_SCFLAGS,	LEVEL_FALLDMG_HX, ~LEVEL_FALLDMG_ZD },
	{ "oldfallingdamage",				MITYPE_SCFLAGS,	LEVEL_FALLDMG_ZD, ~LEVEL_FALLDMG_HX },
	{ "forcefallingdamage",				MITYPE_SCFLAGS,	LEVEL_FALLDMG_ZD, ~LEVEL_FALLDMG_HX },
	{ "strifefallingdamage",			MITYPE_SETFLAG,	LEVEL_FALLDMG_ZD|LEVEL_FALLDMG_HX, 0 },
	{ "nofallingdamage",				MITYPE_SCFLAGS,	0, ~(LEVEL_FALLDMG_ZD|LEVEL_FALLDMG_HX) },
	{ "noallies",						MITYPE_SETFLAG,	LEVEL_NOALLIES, 0 },
	{ "filterstarts",					MITYPE_SETFLAG,	LEVEL_FILTERSTARTS, 0 },
	{ "useplayerstartz",				MITYPE_SETFLAG, LEVEL_USEPLAYERSTARTZ, 0 },
	{ "randomplayerstarts",				MITYPE_SETFLAG2, LEVEL2_RANDOMPLAYERSTARTS, 0 },
	{ "activateowndeathspecials",		MITYPE_SETFLAG,	LEVEL_ACTOWNSPECIAL, 0 },
	{ "killeractivatesdeathspecials",	MITYPE_CLRFLAG,	LEVEL_ACTOWNSPECIAL, 0 },
	{ "missilesactivateimpactlines",	MITYPE_SETFLAG2,	LEVEL2_MISSILESACTIVATEIMPACT, 0 },
	{ "missileshootersactivetimpactlines",MITYPE_CLRFLAG2,	LEVEL2_MISSILESACTIVATEIMPACT, 0 },
	{ "noinventorybar",					MITYPE_SETFLAG,	LEVEL_NOINVENTORYBAR, 0 },
	{ "deathslideshow",					MITYPE_IGNORE,		0, 0 },
	{ "strictmonsteractivation",		MITYPE_CLRFLAG2,	LEVEL2_LAXMONSTERACTIVATION, LEVEL2_LAXACTIVATIONMAPINFO },
	{ "laxmonsteractivation",			MITYPE_SETFLAG2,	LEVEL2_LAXMONSTERACTIVATION, LEVEL2_LAXACTIVATIONMAPINFO },
	{ "additive_scrollers",				MITYPE_COMPATFLAG, COMPATF_BOOMSCROLL, 0 },
	{ "keepfullinventory",				MITYPE_SETFLAG2,	LEVEL2_KEEPFULLINVENTORY, 0 },
	{ "monsterfallingdamage",			MITYPE_SETFLAG2,	LEVEL2_MONSTERFALLINGDAMAGE, 0 },
	{ "nomonsterfallingdamage",			MITYPE_CLRFLAG2,	LEVEL2_MONSTERFALLINGDAMAGE, 0 },
	{ "clipmidtextures",				MITYPE_SETFLAG2,	LEVEL2_CLIPMIDTEX, 0 },
	{ "wrapmidtextures",				MITYPE_SETFLAG2,	LEVEL2_WRAPMIDTEX, 0 },
	{ "allowcrouch",					MITYPE_CLRFLAG,	LEVEL_CROUCH_NO, 0 },
	{ "nocrouch",						MITYPE_SETFLAG,	LEVEL_CROUCH_NO, 0 },
	{ "pausemusicinmenus",				MITYPE_SCFLAGS2,	LEVEL2_PAUSE_MUSIC_IN_MENUS, 0 },
	{ "noinfighting",					MITYPE_SCFLAGS2,	LEVEL2_NOINFIGHTING, ~LEVEL2_TOTALINFIGHTING },
	{ "normalinfighting",				MITYPE_SCFLAGS2,	0, ~(LEVEL2_NOINFIGHTING|LEVEL2_TOTALINFIGHTING)},
	{ "totalinfighting",				MITYPE_SCFLAGS2,	LEVEL2_TOTALINFIGHTING, ~LEVEL2_NOINFIGHTING },
	{ "infiniteflightpowerup",			MITYPE_SETFLAG2,	LEVEL2_INFINITE_FLIGHT, 0 },
	{ "noinfiniteflightpowerup",		MITYPE_CLRFLAG2,	LEVEL2_INFINITE_FLIGHT, 0 },
	{ "allowrespawn",					MITYPE_SETFLAG2,	LEVEL2_ALLOWRESPAWN, 0 },
	{ "teamplayon",						MITYPE_SCFLAGS2,	LEVEL2_FORCETEAMPLAYON, ~LEVEL2_FORCETEAMPLAYOFF },
	{ "teamplayoff",					MITYPE_SCFLAGS2,	LEVEL2_FORCETEAMPLAYOFF, ~LEVEL2_FORCETEAMPLAYON },
	{ "checkswitchrange",				MITYPE_SETFLAG2,	LEVEL2_CHECKSWITCHRANGE, 0 },
	{ "nocheckswitchrange",				MITYPE_CLRFLAG2,	LEVEL2_CHECKSWITCHRANGE, 0 },
	{ "grinding_polyobj",				MITYPE_SETFLAG2,	LEVEL2_POLYGRIND, 0 },
	{ "no_grinding_polyobj",			MITYPE_CLRFLAG2,	LEVEL2_POLYGRIND, 0 },
	{ "resetinventory",					MITYPE_SETFLAG2,	LEVEL2_RESETINVENTORY, 0 },
	{ "resethealth",					MITYPE_SETFLAG2,	LEVEL2_RESETHEALTH, 0 },
	{ "endofgame",						MITYPE_SETFLAG2,	LEVEL2_ENDGAME, 0 },
	{ "nostatistics",					MITYPE_SETFLAG2,	LEVEL2_NOSTATISTICS, 0 },
	{ "noautosavehint",					MITYPE_SETFLAG2,	LEVEL2_NOAUTOSAVEHINT, 0 },
	{ "forgetstate",					MITYPE_SETFLAG2,	LEVEL2_FORGETSTATE, 0 },
	{ "rememberstate",					MITYPE_CLRFLAG2,	LEVEL2_FORGETSTATE, 0 },
	{ "unfreezesingleplayerconversations",MITYPE_SETFLAG2,	LEVEL2_CONV_SINGLE_UNFREEZE, 0 },
	{ "spawnwithweaponraised",			MITYPE_SETFLAG2,	LEVEL2_PRERAISEWEAPON, 0 },
	{ "nobotnodes",						MITYPE_SETFLAGZA,	LEVEL_ZA_NOBOTNODES, 0 },// [BC] Allow the prevention of spawning bot nodes (helpful for very large maps).
	{ "lobby",							MITYPE_SETFLAGZA,	LEVEL_ZA_ISLOBBY, 0 },	// [AM] Prefer this.
	{ "islobby",						MITYPE_SETFLAGZA,	LEVEL_ZA_ISLOBBY, 0 },	// [BB]
	{ "compat_shorttex",				MITYPE_COMPATFLAG, COMPATF_SHORTTEX, 0 },
	{ "compat_stairs",					MITYPE_COMPATFLAG, COMPATF_STAIRINDEX, 0 },
	{ "compat_limitpain",				MITYPE_COMPATFLAG, COMPATF_LIMITPAIN, 0 },
	{ "compat_nopassover",				MITYPE_COMPATFLAG, COMPATF_NO_PASSMOBJ, 0 },
	{ "compat_notossdrops",				MITYPE_COMPATFLAG, COMPATF_NOTOSSDROPS, 0 },
	{ "compat_useblocking", 			MITYPE_COMPATFLAG, COMPATF_USEBLOCKING, 0 },
	{ "compat_nodoorlight",				MITYPE_COMPATFLAG, COMPATF_NODOORLIGHT, 0 },
	{ "compat_ravenscroll",				MITYPE_COMPATFLAG, COMPATF_RAVENSCROLL, 0 },
	{ "compat_soundtarget",				MITYPE_COMPATFLAG, COMPATF_SOUNDTARGET, 0 },
	{ "compat_dehhealth",				MITYPE_COMPATFLAG, COMPATF_DEHHEALTH, 0 },
	{ "compat_trace",					MITYPE_COMPATFLAG, COMPATF_TRACE, 0 },
	{ "compat_dropoff",					MITYPE_COMPATFLAG, COMPATF_DROPOFF, 0 },
	{ "compat_boomscroll",				MITYPE_COMPATFLAG, COMPATF_BOOMSCROLL, 0 },
	{ "compat_invisibility",			MITYPE_COMPATFLAG, COMPATF_INVISIBILITY, 0 },
	{ "compat_silent_instant_floors",	MITYPE_COMPATFLAG, COMPATF_SILENT_INSTANT_FLOORS, 0 },
	{ "compat_sectorsounds",			MITYPE_COMPATFLAG, COMPATF_SECTORSOUNDS, 0 },
	{ "compat_missileclip",				MITYPE_COMPATFLAG, COMPATF_MISSILECLIP, 0 },
	{ "compat_crossdropoff",			MITYPE_COMPATFLAG, COMPATF_CROSSDROPOFF, 0 },
	{ "compat_anybossdeath",			MITYPE_COMPATFLAG, COMPATF_ANYBOSSDEATH, 0 },
	{ "compat_minotaur",				MITYPE_COMPATFLAG, COMPATF_MINOTAUR, 0 },
	{ "compat_mushroom",				MITYPE_COMPATFLAG, COMPATF_MUSHROOM, 0 },
	{ "compat_mbfmonstermove",			MITYPE_COMPATFLAG, COMPATF_MBFMONSTERMOVE, 0 },
	{ "compat_corpsegibs",				MITYPE_COMPATFLAG, COMPATF_CORPSEGIBS, 0 },
	{ "compat_noblockfriends",			MITYPE_COMPATFLAG, COMPATF_NOBLOCKFRIENDS, 0 },
	{ "compat_spritesort",				MITYPE_COMPATFLAG, COMPATF_SPRITESORT, 0 },
	{ "compat_light",					MITYPE_COMPATFLAG, COMPATF_LIGHT, 0 },
	{ "compat_polyobj",					MITYPE_COMPATFLAG, COMPATF_POLYOBJ, 0 },
	{ "compat_maskedmidtex",			MITYPE_COMPATFLAG, COMPATF_MASKEDMIDTEX, 0 },
	{ "compat_badangles",				MITYPE_COMPATFLAG, 0, COMPATF2_BADANGLES },
	{ "compat_floormove",				MITYPE_COMPATFLAG, 0, COMPATF2_FLOORMOVE },
	{ "compat_pushwindow",				MITYPE_COMPATFLAG, 0, COMPATF2_PUSHWINDOW },
	{ "cd_start_track",					MITYPE_EATNEXT,	0, 0 },
	{ "cd_end1_track",					MITYPE_EATNEXT,	0, 0 },
	{ "cd_end2_track",					MITYPE_EATNEXT,	0, 0 },
	{ "cd_end3_track",					MITYPE_EATNEXT,	0, 0 },
	{ "cd_intermission_track",			MITYPE_EATNEXT,	0, 0 },
	{ "cd_title_track",					MITYPE_EATNEXT,	0, 0 },
	{ "notcategory0",					MITYPE_CATEGORYFLAGS,	0, MAPCATEGORYF_0 },
	{ "category1",						MITYPE_CATEGORYFLAGS,	MAPCATEGORYF_1, 0 },
	{ "category2",						MITYPE_CATEGORYFLAGS,	MAPCATEGORYF_2, 0 },
	{ "category3",						MITYPE_CATEGORYFLAGS,	MAPCATEGORYF_3, 0 },
	{ "category4",						MITYPE_CATEGORYFLAGS,	MAPCATEGORYF_4, 0 },
	{ "category5",						MITYPE_CATEGORYFLAGS,	MAPCATEGORYF_5, 0 },
	{ "category6",						MITYPE_CATEGORYFLAGS,	MAPCATEGORYF_6, 0 },
	{ "category7",						MITYPE_CATEGORYFLAGS,	MAPCATEGORYF_7, 0 },
	{ "category8",						MITYPE_CATEGORYFLAGS,	MAPCATEGORYF_8, 0 },
	{ "category9",						MITYPE_CATEGORYFLAGS,	MAPCATEGORYF_9, 0 },
	{ NULL, MITYPE_IGNORE, 0, 0}
};

//==========================================================================
//
// ParseMapDefinition
// Parses the body of a map definition, including defaultmap etc.
//
//==========================================================================

void FMapInfoParser::ParseMapDefinition(level_info_t &info)
{
	int index;

	ParseOpenBrace();

	while (sc.GetString())
	{
		if ((index = sc.MatchString(&MapFlagHandlers->name, sizeof(*MapFlagHandlers))) >= 0)
		{
			MapInfoFlagHandler *handler = &MapFlagHandlers[index];
			switch (handler->type)
			{
			case MITYPE_EATNEXT:
				ParseAssign();
				sc.MustGetString();
				break;

			case MITYPE_IGNORE:
				break;

			case MITYPE_SETFLAG:
				info.flags |= handler->data1;
				info.flags |= handler->data2;
				break;

			case MITYPE_CLRFLAG:
				info.flags &= ~handler->data1;
				info.flags |= handler->data2;
				break;

			case MITYPE_SCFLAGS:
				info.flags = (info.flags & handler->data2) | handler->data1;
				break;

			case MITYPE_SETFLAG2:
				info.flags2 |= handler->data1;
				info.flags2 |= handler->data2;
				break;

			case MITYPE_CLRFLAG2:
				info.flags2 &= ~handler->data1;
				info.flags2 |= handler->data2;
				break;

			case MITYPE_SCFLAGS2:
				info.flags2 = (info.flags2 & handler->data2) | handler->data1;
				break;

			case MITYPE_COMPATFLAG:
			{
				int set = 1;
				if (format_type == FMT_New)
				{
					if (CheckAssign())
					{
						sc.MustGetNumber();
						set = sc.Number;
					}
				}
				else
				{
					if (sc.CheckNumber()) set = sc.Number;
				}

				if (set)
				{
					info.compatflags |= handler->data1;
					info.compatflags2 |= handler->data2;
				}
				else
				{
					info.compatflags &= ~handler->data1;
					info.compatflags2 &= ~handler->data2;
				}
				info.compatmask |= handler->data1;
				info.compatmask2 |= handler->data2;
			}
			break;

			// [BB]
			case MITYPE_SETFLAGZA:
				info.flagsZA |= handler->data1;
				info.flagsZA |= handler->data2;
				break;
				
			// [geNia]
			case MITYPE_CATEGORYFLAGS:
				info.categoryflags |= handler->data1;
				info.categoryflags &= ~handler->data2;
				break;

			default:
				// should never happen
				assert(false);
				break;
			}
		}
		else
		{
			FAutoSegIterator probe(YRegHead, YRegTail);
			bool success = false;

			while (*++probe != NULL)
			{
				if (sc.Compare(((FMapOptInfo *)(*probe))->name))
				{
					if (!((FMapOptInfo *)(*probe))->old && format_type != FMT_New)
					{
						sc.ScriptError("MAPINFO option '%s' requires the new MAPINFO format", sc.String);
					}
					((FMapOptInfo *)(*probe))->handler(*this, &info);
					success = true;
					break;
				}
			}

			if (!success)
			{
				if (!ParseCloseBrace())
				{
					// Unknown
					sc.ScriptMessage("Unknown property '%s' found in map definition\n", sc.String);
					SkipToNext();
				}
				else
				{
					break;
				}
			}
		}
	}
	CheckEndOfFile("map");
}


//==========================================================================
//
// GetDefaultLevelNum
// Gets a default level num from a map name.
//
//==========================================================================

static int GetDefaultLevelNum(const char *mapname)
{
	if (!strnicmp (mapname, "MAP", 3) && strlen(mapname) <= 5)
	{
		int mapnum = atoi (mapname + 3);

		if (mapnum >= 1 && mapnum <= 99)
			return mapnum;
	}
	else if (mapname[0] == 'E' &&
			mapname[1] >= '0' && mapname[1] <= '9' &&
			mapname[2] == 'M' &&
			mapname[3] >= '0' && mapname[3] <= '9')
	{
		int epinum = mapname[1] - '1';
		int mapnum = mapname[3] - '0';
		return epinum*10 + mapnum;
	}
	return 0;
}

//==========================================================================
//
// ParseMapHeader
// Parses the header of a map definition ('map mapxx mapname')
//
//==========================================================================

level_info_t *FMapInfoParser::ParseMapHeader(level_info_t &defaultinfo)
{
	FName mapname;

	if (sc.CheckNumber())
	{	// MAPNAME is a number; assume a Hexen wad
		char maptemp[8];
		mysnprintf (maptemp, countof(maptemp), "MAP%02d", sc.Number);
		mapname = maptemp;
		HexenHack = true;
	}
	else 
	{
		sc.MustGetString();
		mapname = sc.String;
	}
	int levelindex = FindWadLevelInfo (mapname);
	if (levelindex == -1)
	{
		levelindex = wadlevelinfos.Reserve(1);
	}
	level_info_t *levelinfo = &wadlevelinfos[levelindex];
	*levelinfo = defaultinfo;
	if (HexenHack)
	{
		levelinfo->WallHorizLight = levelinfo->WallVertLight = 0;

		// Hexen levels are automatically nointermission,
		// no auto sound sequences, falling damage,
		// monsters activate their own specials, and missiles
		// are always the activators of impact lines.
		levelinfo->flags |= LEVEL_NOINTERMISSION
						 | LEVEL_SNDSEQTOTALCTRL
						 | LEVEL_FALLDMG_HX
						 | LEVEL_ACTOWNSPECIAL;
		levelinfo->flags2|= LEVEL2_HEXENHACK
						 | LEVEL2_INFINITE_FLIGHT
						 | LEVEL2_MISSILESACTIVATEIMPACT
						 | LEVEL2_MONSTERFALLINGDAMAGE;

	}

	uppercopy (levelinfo->mapname, mapname);
	levelinfo->mapname[8] = 0;
	sc.MustGetString ();
	if (sc.String[0] == '$')
	{
		// For consistency with other definitions allow $Stringtablename here, too.
		levelinfo->flags |= LEVEL_LOOKUPLEVELNAME;
		levelinfo->LevelName = sc.String + 1;
	}
	else
	{
		if (sc.Compare ("lookup"))
		{
			sc.MustGetString ();
			levelinfo->flags |= LEVEL_LOOKUPLEVELNAME;
		}
		levelinfo->LevelName = sc.String;
	}

	// Set up levelnum now so that you can use Teleport_NewMap specials
	// to teleport to maps with standard names without needing a levelnum.
	levelinfo->levelnum = GetDefaultLevelNum(levelinfo->mapname);

	// Does this map have a song defined via SNDINFO's $map command?
	// Set that as this map's default music if it does.
	FString *song;
	if ((song = HexenMusic.CheckKey(levelinfo->levelnum)) != NULL)
	{
		levelinfo->Music = *song;
	}

	return levelinfo;
}


//==========================================================================
//
// Episode definitions start with the header "episode <start-map>"
// and then can be followed by any of the following:
//
// name "Episode name as text"
// picname "Picture to display the episode name"
// key "Shortcut key for the menu"
// noskillmenu
// remove
// [BC] botepisode
// [BC] botskillname "Title at botskill menu"
// [BC] botskillpicname "Picture to display as botskill menu title"
//
//==========================================================================

void FMapInfoParser::ParseEpisodeInfo ()
{
	unsigned int i;
	char map[9];
	FString pic;
	FString name;
	bool remove = false;
	char key = 0;
	bool noskill = false;
	bool optional = false;
	bool extended = false;
	//[BC]
	bool	bBotEpisode = false;
	FString botskilltitle;
	bool	bBotSkillPicIsGFX = false;

	// Get map name
	sc.MustGetString ();
	uppercopy (map, sc.String);
	map[8] = 0;

	if (sc.CheckString ("teaser"))
	{
		sc.MustGetString ();
		if (gameinfo.flags & GI_SHAREWARE)
		{
			uppercopy (map, sc.String);
		}
	}

	ParseOpenBrace();

	while (sc.GetString())
	{
		if (sc.Compare ("optional"))
		{
			// For M4 in Doom
			optional = true;
		}
		else if (sc.Compare ("extended"))
		{
			// For M4 and M5 in Heretic
			extended = true;
		}
		else if (sc.Compare ("name"))
		{
			ParseAssign();
			sc.MustGetString ();
			name = sc.String;
		}
		else if (sc.Compare ("picname"))
		{
			ParseAssign();
			sc.MustGetString ();
			pic = sc.String;
		}
		else if (sc.Compare ("remove"))
		{
			remove = true;
		}
		else if (sc.Compare ("key"))
		{
			ParseAssign();
			sc.MustGetString ();
			key = sc.String[0];
		}
		else if (sc.Compare("noskillmenu"))
		{
			noskill = true;
		}
		// [BC]
		else if ( sc.Compare( "botepisode" ))
		{
			bBotEpisode = true;
		}
		else if ( sc.Compare( "botskillname" ))
		{
			sc.MustGetString( );
			botskilltitle = sc.String;
			bBotSkillPicIsGFX = false;
		}
		else if ( sc.Compare( "botskillpicname" ))
		{
			sc.MustGetString( );
			botskilltitle = sc.String;
			bBotSkillPicIsGFX = true;
		}
		else if (!ParseCloseBrace())
		{
			// Unknown
			sc.ScriptMessage("Unknown property '%s' found in episode definition\n", sc.String);
			SkipToNext();
		}
		else
		{
			break;
		}
	}
	CheckEndOfFile("episode");

	if (extended && !(gameinfo.flags & GI_MENUHACK_EXTENDED))
	{ // If the episode is for the extended Heretic, but this is
	  // not the extended Heretic, ignore it.
		return;
	}

	if (optional && !remove)
	{
		if (!P_CheckMapData(map))
		{
			// If the episode is optional and the map does not exist
			// just ignore this episode definition.
			return;
		}
	}


	for (i = 0; i < AllEpisodes.Size(); i++)
	{
		if (AllEpisodes[i].mEpisodeMap.CompareNoCase(map) == 0)
		{
			break;
		}
	}

	if (remove)
	{
		// If the remove property is given for an episode, remove it.
		AllEpisodes.Delete(i);
	}
	else
	{
		FEpisode *epi;
		
		// Only allocate a new entry if this doesn't replace an existing episode.
		if (i >= AllEpisodes.Size())
			epi = new FEpisode();
		else
			epi = &AllEpisodes[i];

		epi->mEpisodeMap = map;
		epi->mEpisodeName = name;
		epi->mPicName = pic;
		epi->mShortcut = tolower(key);
		epi->mNoSkill = noskill;
		// [BB] Support for Skulltag's bot episodes
		if ( botskilltitle.IsNotEmpty() && ( bBotEpisode == false ))
			sc.ScriptMessage("'%s' used without 'botepisode' in the episode definition. It will be ignored.\n", bBotSkillPicIsGFX ? "botskillpicname" : "botskillname");
		epi->bBotEpisode = bBotEpisode;
		epi->bBotSkillFullText = !bBotSkillPicIsGFX;
		epi->BotSkillTitle = botskilltitle;
		
		// Only allocate a new entry if this doesn't replace an existing episode.
		if (i >= AllEpisodes.Size())
		{
			// [geNia] Add bot episodes on top of normal episodes
			if (bBotEpisode)
			{
				AllEpisodes.Insert(botepisodei, *epi);
				botepisodei++;
			}
			else
			{
				AllEpisodes.Push(*epi);
			}
		}

	}
}


//==========================================================================
//
// Clears episode definitions
//
//==========================================================================

void ClearEpisodes()
{
	botepisodei = 0;
	AllEpisodes.Clear();
}

//==========================================================================
//
// SetLevelNum
// Avoid duplicate levelnums. The level being set always has precedence.
//
//==========================================================================

static void SetLevelNum (level_info_t *info, int num)
{
	for (unsigned int i = 0; i < wadlevelinfos.Size(); ++i)
	{
		if (wadlevelinfos[i].levelnum == num)
			wadlevelinfos[i].levelnum = 0;
	}
	info->levelnum = num;
}

//==========================================================================
//
// G_DoParseMapInfo
// Parses a single MAPINFO lump
// data for wadlevelinfos and wadclusterinfos.
//
//==========================================================================

void FMapInfoParser::ParseMapInfo (int lump, level_info_t &gamedefaults, level_info_t &defaultinfo)
{
	sc.OpenLumpNum(lump);

	defaultinfo = gamedefaults;
	HexenHack = false;

	while (sc.GetString ())
	{
		if (sc.Compare("include"))
		{
			sc.MustGetString();
			int inclump = Wads.CheckNumForFullName(sc.String, true);
			if (inclump < 0)
			{
				sc.ScriptError("include file '%s' not found", sc.String);
			}
			if (Wads.GetLumpFile(sc.LumpNum) != Wads.GetLumpFile(inclump))
			{
				// Do not allow overriding includes from the default MAPINFO
				if (Wads.GetLumpFile(sc.LumpNum) == 0)
				{
					I_FatalError("File %s is overriding core lump %s.",
						Wads.GetWadFullName(Wads.GetLumpFile(inclump)), sc.String);
				}
			}
			FScanner saved_sc = sc;
			ParseMapInfo(inclump, gamedefaults, defaultinfo);
			sc = saved_sc;
		}
		else if (sc.Compare("gamedefaults"))
		{
			gamedefaults.Reset();
			ParseMapDefinition(gamedefaults);
			defaultinfo = gamedefaults;
		}
		else if (sc.Compare("defaultmap"))
		{
			defaultinfo = gamedefaults;
			ParseMapDefinition(defaultinfo);
		}
		else if (sc.Compare("adddefaultmap"))
		{
			// Same as above but adds to the existing definitions instead of replacing them completely
			ParseMapDefinition(defaultinfo);
		}
		else if (sc.Compare("map"))
		{
			level_info_t *levelinfo = ParseMapHeader(defaultinfo);

			ParseMapDefinition(*levelinfo);

			// When the second sky is -NOFLAT-, make it a copy of the first sky
			if (strcmp (levelinfo->skypic2, "-NOFLAT-") == 0)
			{
				strcpy (levelinfo->skypic2, levelinfo->skypic1);
			}
			SetLevelNum (levelinfo, levelinfo->levelnum);	// Wipe out matching levelnums from other maps.
		}
		// clusterdef is the old keyword but the new format has enough 
		// structuring that 'cluster' can be handled, too. The old format does not.
		else if (sc.Compare("clusterdef") || (format_type != FMT_Old && sc.Compare("cluster")))
		{
			ParseCluster();
		}
		else if (sc.Compare("episode"))
		{
			ParseEpisodeInfo();
		}
		else if (sc.Compare("clearepisodes"))
		{
			ClearEpisodes();
		}
		else if (sc.Compare("skill"))
		{
			ParseSkill();
		}
		else if (sc.Compare("clearskills"))
		{
			AllSkills.Clear();
			DefaultSkill = -1;
		}
		else if (sc.Compare("gameinfo"))
		{
			if (format_type != FMT_Old)
			{
				format_type = FMT_New;
				ParseGameInfo();
			}
			else
			{
				sc.ScriptError("gameinfo definitions not supported with old MAPINFO syntax");
			}
		}
		else if (sc.Compare("intermission"))
		{
			if (format_type != FMT_Old)
			{
				format_type = FMT_New;
				ParseIntermission();
			}
			else
			{
				sc.ScriptError("intermission definitions not supported with old MAPINFO syntax");
			}
		}
		else if (sc.Compare("automap") || sc.Compare("automap_overlay"))
		{
			if (format_type != FMT_Old)
			{
				format_type = FMT_New;
				ParseAMColors(sc.Compare("automap_overlay"));
			}
			else
			{
				sc.ScriptError("automap colorset definitions not supported with old MAPINFO syntax");
			}
		}
		else
		{
			sc.ScriptError("%s: Unknown top level keyword", sc.String);
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void DeinitIntermissions();

static void ClearMapinfo()
{
	wadclusterinfos.Clear();
	wadlevelinfos.Clear();
	ClearEpisodes();
	AllSkills.Clear();
	DefaultSkill = -1;
	DeinitIntermissions();
	level.info = NULL;
}

//==========================================================================
//
// G_ParseMapInfo
// Parses the MAPINFO lumps of all loaded WADs and generates
// data for wadlevelinfos and wadclusterinfos.
//
//==========================================================================

void G_ParseMapInfo (const char *basemapinfo)
{
	int lump, lastlump = 0;
	level_info_t gamedefaults;

	ClearMapinfo();
	atterm(ClearMapinfo);

	// Parse the default MAPINFO for the current game. This lump *MUST* come from zdoom.pk3.
	if (basemapinfo != NULL)
	{
		FMapInfoParser parse;
		level_info_t defaultinfo;
		int baselump = Wads.GetNumForFullName(basemapinfo);
		if (Wads.GetLumpFile(baselump) > 0)
		{
			I_FatalError("File %s is overriding core lump %s.", 
				Wads.GetWadFullName(Wads.GetLumpFile(baselump)), basemapinfo);
		}
		parse.ParseMapInfo(baselump, gamedefaults, defaultinfo);
	}

	static const char *mapinfonames[] = { "MAPINFO", "ZMAPINFO", NULL };
	int nindex;

	// Parse any extra MAPINFOs.
	while ((lump = Wads.FindLumpMulti (mapinfonames, &lastlump, false, &nindex)) != -1)
	{
		if (nindex == 0)
		{
			// If this lump is named MAPINFO we need to check if the same WAD contains a ZMAPINFO lump.
			// If that exists we need to skip this one.

			int wad = Wads.GetLumpFile(lump);
			int altlump = Wads.CheckNumForName("ZMAPINFO", ns_global, wad, true);

			if (altlump >= 0) continue;
		}
		FMapInfoParser parse(nindex == 1? FMapInfoParser::FMT_New : FMapInfoParser::FMT_Unknown);
		level_info_t defaultinfo;
		parse.ParseMapInfo(lump, gamedefaults, defaultinfo);
	}

	if (AllEpisodes.Size() == 0)
	{
		I_FatalError ("You cannot use clearepisodes in a MAPINFO if you do not define any new episodes after it.");
	}
	if (AllSkills.Size() == 0)
	{
		I_FatalError ("You cannot use clearskills in a MAPINFO if you do not define any new skills after it.");
	}
}

