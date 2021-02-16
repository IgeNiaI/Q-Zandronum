// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id: s_sound.c,v 1.3 1998/01/05 16:26:08 pekangas Exp $
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
//
// DESCRIPTION:  none
//
//-----------------------------------------------------------------------------


#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>

#include "i_system.h"
#include "i_sound.h"
#include "i_music.h"
#include "i_cd.h"
#include "s_sound.h"
#include "s_sndseq.h"
#include "s_playlist.h"
#include "c_dispatch.h"
#include "m_random.h"
#include "w_wad.h"
#include "doomdef.h"
#include "p_local.h"
#include "doomstat.h"
#include "cmdlib.h"
#include "v_video.h"
#include "v_text.h"
#include "a_sharedglobal.h"
#include "gstrings.h"
#include "gi.h"
#include "templates.h"
#include "timidity/timidity.h"
#include "g_level.h"
#include "po_man.h"
#include "farchive.h"
// [BB] New #includes.
#include "cl_demo.h"
#include "deathmatch.h"
#include "network.h"
#include "sv_commands.h"
#include "cl_main.h"

// MACROS ------------------------------------------------------------------

#ifdef NeXT
// NeXT doesn't need a binary flag in open call
#define O_BINARY 0
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifndef FIXED2FLOAT
#define FIXED2FLOAT(f)			(((float)(f))/(float)65536)
#endif

#define NORM_PITCH				128
#define NORM_PRIORITY			64
#define NORM_SEP				0

#define S_PITCH_PERTURB 		1
#define S_STEREO_SWING			0.75

// TYPES -------------------------------------------------------------------

struct MusPlayingInfo
{
	FString name;
	MusInfo *handle;
	int   baseorder;
	bool  loop;
};

enum
{
	SOURCE_None,		// Sound is always on top of the listener.
	SOURCE_Actor,		// Sound is coming from an actor.
	SOURCE_Sector,		// Sound is coming from a sector.
	SOURCE_Polyobj,		// Sound is coming from a polyobject.
	SOURCE_Unattached,	// Sound is not attached to any particular emitter.
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

extern float S_GetMusicVolume (const char *music);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static bool S_CheckSoundLimit(sfxinfo_t *sfx, const FVector3 &pos, int near_limit, float limit_range, AActor *actor, int channel);
static bool S_IsChannelUsed(AActor *actor, int channel, int *seen);
static void S_ActivatePlayList(bool goBack);
static void CalcPosVel(FSoundChan *chan, FVector3 *pos, FVector3 *vel);
static void CalcPosVel(int type, const AActor *actor, const sector_t *sector, const FPolyObj *poly,
	const float pt[3], int channel, int chanflags, FVector3 *pos, FVector3 *vel);
static void CalcSectorSoundOrg(const sector_t *sec, int channum, fixed_t *x, fixed_t *y, fixed_t *z);
static void CalcPolyobjSoundOrg(const FPolyObj *poly, fixed_t *x, fixed_t *y, fixed_t *z);
static FSoundChan *S_StartSound(AActor *mover, const sector_t *sec, const FPolyObj *poly,
	const FVector3 *pt, int channel, FSoundID sound_id, float volume, float attenuation, FRolloffInfo *rolloff);
static void S_SetListener(SoundListener &listener, AActor *listenactor);

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static bool		SoundPaused;		// whether sound is paused
static bool		MusicPaused;		// whether music is paused
static MusPlayingInfo mus_playing;	// music currently being played
static FString	 LastSong;			// last music that was played
static FPlayList *PlayList;
static int		RestartEvictionsAt;	// do not restart evicted channels before this level.time
static bool		g_bNewSoundCurve;
static BYTE		*g_aOriginalSoundCurve;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int sfx_empty;

FSoundChan *Channels;
FSoundChan *FreeChannels;

FRolloffInfo S_Rolloff;
BYTE *S_SoundCurve;
int S_SoundCurveSize;

FBoolCVar noisedebug ("noise", false, 0);	// [RH] Print sound debugging info?
CVAR (Int, snd_channels, 32, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)	// number of channels available
CVAR (Bool, snd_flipstereo, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

// CODE --------------------------------------------------------------------

//==========================================================================
//
// S_NoiseDebug
//
// [RH] Print sound debug info. Called by status bar.
//==========================================================================

void S_NoiseDebug (void)
{
	// [BB] On clients, only allow this when cheats are on.
	if ( NETWORK_InClientMode() && ( sv_cheats == false ) )
		return;

	FSoundChan *chan;
	FVector3 listener;
	FVector3 origin;
	int y, color;

	y = 32 * CleanYfac;
	screen->DrawText (SmallFont, CR_YELLOW, 0, y, "*** SOUND DEBUG INFO ***", TAG_DONE);
	y += 8;

	screen->DrawText (SmallFont, CR_GOLD, 0, y, "name", TAG_DONE);
	screen->DrawText (SmallFont, CR_GOLD, 70, y, "x", TAG_DONE);
	screen->DrawText (SmallFont, CR_GOLD, 120, y, "y", TAG_DONE);
	screen->DrawText (SmallFont, CR_GOLD, 170, y, "z", TAG_DONE);
	screen->DrawText (SmallFont, CR_GOLD, 220, y, "vol", TAG_DONE);
	screen->DrawText (SmallFont, CR_GOLD, 260, y, "dist", TAG_DONE);
	screen->DrawText (SmallFont, CR_GOLD, 300, y, "chan", TAG_DONE);
	screen->DrawText (SmallFont, CR_GOLD, 340, y, "pri", TAG_DONE);
	screen->DrawText (SmallFont, CR_GOLD, 380, y, "flags", TAG_DONE);
	screen->DrawText (SmallFont, CR_GOLD, 460, y, "aud", TAG_DONE);
	screen->DrawText (SmallFont, CR_GOLD, 520, y, "pos", TAG_DONE);
	y += 8;

	if (Channels == NULL)
	{
		return;
	}

	listener.X = FIXED2FLOAT(players[consoleplayer].camera->x);
	listener.Y = FIXED2FLOAT(players[consoleplayer].camera->z);
	listener.Z = FIXED2FLOAT(players[consoleplayer].camera->y);

	// Display the oldest channel first.
	for (chan = Channels; chan->NextChan != NULL; chan = chan->NextChan)
	{ }
	while (y < SCREENHEIGHT - 16)
	{
		char temp[32];

		CalcPosVel(chan, &origin, NULL);
		color = (chan->ChanFlags & CHAN_LOOP) ? CR_BROWN : CR_GREY;

		// Name
		Wads.GetLumpName (temp, S_sfx[chan->SoundID].lumpnum);
		temp[8] = 0;
		screen->DrawText (SmallFont, color, 0, y, temp, TAG_DONE);

		if (!(chan->ChanFlags & CHAN_IS3D))
		{
			screen->DrawText(SmallFont, color, 70, y, "---", TAG_DONE);		// X
			screen->DrawText(SmallFont, color, 120, y, "---", TAG_DONE);	// Y
			screen->DrawText(SmallFont, color, 170, y, "---", TAG_DONE);	// Z
			screen->DrawText(SmallFont, color, 260, y, "---", TAG_DONE);	// Distance
		}
		else
		{
			// X coordinate
			mysnprintf(temp, countof(temp), "%.0f", origin.X);
			screen->DrawText(SmallFont, color, 70, y, temp, TAG_DONE);

			// Y coordinate
			mysnprintf(temp, countof(temp), "%.0f", origin.Z);
			screen->DrawText(SmallFont, color, 120, y, temp, TAG_DONE);

			// Z coordinate
			mysnprintf(temp, countof(temp), "%.0f", origin.Y);
			screen->DrawText(SmallFont, color, 170, y, temp, TAG_DONE);

			// Distance
			if (chan->DistanceScale > 0)
			{
				mysnprintf(temp, countof(temp), "%.0f", (origin - listener).Length());
				screen->DrawText(SmallFont, color, 260, y, temp, TAG_DONE);
			}
			else
			{
				screen->DrawText(SmallFont, color, 260, y, "---", TAG_DONE);
			}
		}

		// Volume
		mysnprintf(temp, countof(temp), "%.2g", chan->Volume);
		screen->DrawText(SmallFont, color, 220, y, temp, TAG_DONE);

		// Channel
		mysnprintf(temp, countof(temp), "%d", chan->EntChannel);
		screen->DrawText(SmallFont, color, 300, y, temp, TAG_DONE);

		// Priority
		mysnprintf(temp, countof(temp), "%d", chan->Priority);
		screen->DrawText(SmallFont, color, 340, y, temp, TAG_DONE);

		// Flags
		mysnprintf(temp, countof(temp), "%s3%sZ%sU%sM%sN%sA%sL%sE%sV",
			(chan->ChanFlags & CHAN_IS3D)			? TEXTCOLOR_GREEN : TEXTCOLOR_BLACK,
			(chan->ChanFlags & CHAN_LISTENERZ)		? TEXTCOLOR_GREEN : TEXTCOLOR_BLACK,
			(chan->ChanFlags & CHAN_UI)				? TEXTCOLOR_GREEN : TEXTCOLOR_BLACK,
			(chan->ChanFlags & CHAN_MAYBE_LOCAL)	? TEXTCOLOR_GREEN : TEXTCOLOR_BLACK,
			(chan->ChanFlags & CHAN_NOPAUSE)		? TEXTCOLOR_GREEN : TEXTCOLOR_BLACK,
			(chan->ChanFlags & CHAN_AREA)			? TEXTCOLOR_GREEN : TEXTCOLOR_BLACK,
			(chan->ChanFlags & CHAN_LOOP)			? TEXTCOLOR_GREEN : TEXTCOLOR_BLACK,
			(chan->ChanFlags & CHAN_EVICTED)		? TEXTCOLOR_GREEN : TEXTCOLOR_BLACK,
			(chan->ChanFlags & CHAN_VIRTUAL)		? TEXTCOLOR_GREEN : TEXTCOLOR_BLACK);
		screen->DrawText(SmallFont, color, 380, y, temp, TAG_DONE);

		// Audibility
		mysnprintf(temp, countof(temp), "%.4f", GSnd->GetAudibility(chan));
		screen->DrawText(SmallFont, color, 460, y, temp, TAG_DONE);

		// Position
		mysnprintf(temp, countof(temp), "%u", GSnd->GetPosition(chan));
		screen->DrawText(SmallFont, color, 520, y, temp, TAG_DONE);


		y += 8;
		if (chan->PrevChan == &Channels)
		{
			break;
		}
		chan = (FSoundChan *)((size_t)chan->PrevChan - myoffsetof(FSoundChan, NextChan));
	}
	V_SetBorderNeedRefresh();
}

static FString LastLocalSndInfo;
static FString LastLocalSndSeq;
void S_AddLocalSndInfo(int lump);

//==========================================================================
//
// S_Init
//
// Initializes sound stuff, including volume. Sets channels, SFX and
// music volume, allocates channel buffer, and sets S_sfx lookup.
//==========================================================================

void S_Init ()
{
	int curvelump;

	// [BC] Server doesn't use music/sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	atterm (S_Shutdown);

	// remove old data (S_Init can be called multiple times!)
	if (S_SoundCurve != NULL)
	{
		delete[] S_SoundCurve;
		S_SoundCurve = NULL;
	}

	if ( g_aOriginalSoundCurve )
		delete [] ( g_aOriginalSoundCurve );

	g_bNewSoundCurve = false;
	g_aOriginalSoundCurve = NULL;

	// Heretic and Hexen have sound curve lookup tables. Doom does not.
	curvelump = Wads.CheckNumForName ("SNDCURVE");
	if (curvelump >= 0)
	{
		/*
		// We have a new custom sound curve.
		g_bNewSoundCurve = true;
		*/

		S_SoundCurveSize = Wads.LumpLength (curvelump);
		S_SoundCurve = new BYTE[S_SoundCurveSize];
		Wads.ReadLump(curvelump, S_SoundCurve);

		/*
		// Also, initialize the original sound curve.
		g_aOriginalSoundCurve = new BYTE[S_CLIPPING_DIST];
		for (i = 0; i < S_CLIPPING_DIST; ++i)
		{
			g_aOriginalSoundCurve[i] = (powf(10.f, MIN(1.f, (S_CLIPPING_DIST - i) / S_ATTENUATOR)) - 1.f) / 9.f;
		}
		*/
	}

	// Free all channels for use.
	while (Channels != NULL)
	{
		S_ReturnChannel(Channels);
	}

	// no sounds are playing, and they are not paused
	MusicPaused = false;
}

//==========================================================================
//
// S_InitData
//
//==========================================================================

void S_InitData ()
{
	LastLocalSndInfo = LastLocalSndSeq = "";
	S_ParseSndInfo (false);
	S_ParseSndSeq (-1);
}

//==========================================================================
//
// S_Shutdown
//
//==========================================================================

void S_Shutdown ()
{
	FSoundChan *chan, *next;

	chan = Channels;
	while (chan != NULL)
	{
		next = chan->NextChan;
		S_StopChannel(chan);
		chan = next;
	}

	GSnd->UpdateSounds();
	for (chan = FreeChannels; chan != NULL; chan = next)
	{
		next = chan->NextChan;
		delete chan;
	}
	FreeChannels = NULL;

	if (S_SoundCurve != NULL)
	{
		delete[] S_SoundCurve;
		S_SoundCurve = NULL;
	}
	if (PlayList != NULL)
	{
		delete PlayList;
		PlayList = NULL;
	}
	S_StopMusic (true);
	mus_playing.name = "";
	LastSong = "";

	// [BC/BB]
	if ( g_aOriginalSoundCurve )
	{
		delete[] ( g_aOriginalSoundCurve );
		g_aOriginalSoundCurve = NULL;
	}
}

//==========================================================================
//
// S_Start
//
// Per level startup code. Kills playing sounds at start of level
// and starts new music.
//==========================================================================

void S_Start ()
{
	if (GSnd)
	{
		// kill all playing sounds at start of level (trust me - a good idea)
		S_StopAllChannels();
		
		// Check for local sound definitions. Only reload if they differ
		// from the previous ones.
		FString LocalSndInfo;
		FString LocalSndSeq;
		
		// To be certain better check whether level is valid!
		if (level.info)
		{
			LocalSndInfo = level.info->SoundInfo;
		}

		if (level.info)
		{
			LocalSndSeq  = level.info->SndSeq;
		}

		bool parse_ss = false;

		// This level uses a different local SNDINFO
		if (LastLocalSndInfo.CompareNoCase(LocalSndInfo) != 0 || !level.info)
		{
			// First delete the old sound list
			for(unsigned i = 1; i < S_sfx.Size(); i++) 
			{
				S_UnloadSound(&S_sfx[i]);
			}
			
			// Parse the global SNDINFO
			S_ParseSndInfo(true);
		
			if (*LocalSndInfo)
			{
				// Now parse the local SNDINFO
				int j = Wads.CheckNumForFullName(LocalSndInfo, true);
				if (j>=0) S_AddLocalSndInfo(j);
			}

			// Also reload the SNDSEQ if the SNDINFO was replaced!
			parse_ss = true;
		}
		else if (LastLocalSndSeq.CompareNoCase(LocalSndSeq) != 0)
		{
			parse_ss = true;
		}

		if (parse_ss)
		{
			S_ParseSndSeq(*LocalSndSeq? Wads.CheckNumForFullName(LocalSndSeq, true) : -1);
		}
		
		LastLocalSndInfo = LocalSndInfo;
		LastLocalSndSeq = LocalSndSeq;
	}

	// stop the old music if it has been paused.
	// This ensures that the new music is started from the beginning
	// if it's the same as the last one and it has been paused.
	if (MusicPaused) S_StopMusic(true);

	// start new music for the level
	MusicPaused = false;

	// [BC] In client mode, let the server tell us what music to play.
	if ( NETWORK_InClientMode() && level.Music.IsNotEmpty() )
		return;

	// Don't start the music if loading a savegame, because the music is stored there.
	// Don't start the music if revisiting a level in a hub for the same reason.
	if (!savegamerestore && (level.info == NULL || level.info->snapshot == NULL || !level.info->isValid()))
	{
		if (level.cdtrack == 0 || !S_ChangeCDMusic (level.cdtrack, level.cdid))
		{
			S_ChangeMusic (level.Music, level.musicorder);

			// [BC] If we're the server, save this music selection so we can tell clients
			// what music to play when they connect.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVER_SetMapMusic( level.Music, level.musicorder );
		}
	}
}

//==========================================================================
//
// S_PrecacheLevel
//
// Like R_PrecacheLevel, but for sounds.
//
//==========================================================================

void S_PrecacheLevel ()
{
	unsigned int i;

	if (GSnd)
	{
		for (i = 0; i < S_sfx.Size(); ++i)
		{
			S_sfx[i].bUsed = false;
		}

		AActor *actor;
		TThinkerIterator<AActor> iterator;

		// Precache all sounds known to be used by the currently spawned actors.
		while ( (actor = iterator.Next()) != NULL )
		{
			actor->MarkPrecacheSounds();
		}
		// Precache all extra sounds requested by this map.
		for (i = 0; i < level.info->PrecacheSounds.Size(); ++i)
		{
			level.info->PrecacheSounds[i].MarkUsed();
		}

		for (i = 1; i < S_sfx.Size(); ++i)
		{
			if (S_sfx[i].bUsed)
			{
				S_CacheSound (&S_sfx[i]);
			}
		}
		for (i = 1; i < S_sfx.Size(); ++i)
		{
			if (!S_sfx[i].bUsed && S_sfx[i].link == sfxinfo_t::NO_LINK)
			{
				S_UnloadSound (&S_sfx[i]);
			}
		}
	}
}

//==========================================================================
//
// S_CacheSound
//
//==========================================================================

void S_CacheSound (sfxinfo_t *sfx)
{
	if (GSnd)
	{
		if (sfx->bPlayerReserve)
		{
			return;
		}
		else if (sfx->bRandomHeader)
		{
			S_CacheRandomSound (sfx);
		}
		else
		{
			while (sfx->link != sfxinfo_t::NO_LINK)
			{
				sfx = &S_sfx[sfx->link];
			}
			sfx->bUsed = true;
			S_LoadSound (sfx);
		}
	}
}

//==========================================================================
//
// S_UnloadSound
//
//==========================================================================

void S_UnloadSound (sfxinfo_t *sfx)
{
	if (sfx->data.isValid())
	{
		GSnd->UnloadSound(sfx->data);
		sfx->data.Clear();
		DPrintf("Unloaded sound \"%s\" (%td)\n", sfx->name.GetChars(), sfx - &S_sfx[0]);
	}
}

//==========================================================================
//
// S_GetChannel
//
// Returns a free channel for the system sound interface.
//
//==========================================================================

FISoundChannel *S_GetChannel(void *syschan)
{
	FSoundChan *chan;

	if (FreeChannels != NULL)
	{
		chan = FreeChannels;
		S_UnlinkChannel(chan);
	}
	else
	{
		chan = new FSoundChan;
		memset(chan, 0, sizeof(*chan));
	}
	S_LinkChannel(chan, &Channels);
	chan->SysChannel = syschan;
	return chan;
}

//==========================================================================
//
// S_ReturnChannel
//
// Returns a channel to the free pool.
//
//==========================================================================

void S_ReturnChannel(FSoundChan *chan)
{
	S_UnlinkChannel(chan);
	memset(chan, 0, sizeof(*chan));
	S_LinkChannel(chan, &FreeChannels);
}

//==========================================================================
//
// S_UnlinkChannel
//
//==========================================================================

void S_UnlinkChannel(FSoundChan *chan)
{
	*(chan->PrevChan) = chan->NextChan;
	if (chan->NextChan != NULL)
	{
		chan->NextChan->PrevChan = chan->PrevChan;
	}
}

//==========================================================================
//
// S_LinkChannel
//
//==========================================================================

void S_LinkChannel(FSoundChan *chan, FSoundChan **head)
{
	chan->NextChan = *head;
	if (chan->NextChan != NULL)
	{
		chan->NextChan->PrevChan = &chan->NextChan;
	}
	*head = chan;
	chan->PrevChan = head;
}

// [RH] Split S_StartSoundAtVolume into multiple parts so that sounds can
//		be specified both by id and by name. Also borrowed some stuff from
//		Hexen and parameters from Quake.

//==========================================================================
//
// CalcPosVel
//
// Retrieves a sound's position and velocity for 3D sounds. This version
// is for an already playing sound.
//
//=========================================================================

static void CalcPosVel(FSoundChan *chan, FVector3 *pos, FVector3 *vel)
{
	CalcPosVel(chan->SourceType, chan->Actor, chan->Sector, chan->Poly, chan->Point,
		chan->EntChannel, chan->ChanFlags, pos, vel);
}

//=========================================================================
//
// CalcPosVel
//
// This version is for sounds that haven't started yet so have no channel.
//
//=========================================================================

static void CalcPosVel(int type, const AActor *actor, const sector_t *sector,
	const FPolyObj *poly, const float pt[3], int channum, int chanflags, FVector3 *pos, FVector3 *vel)
{
	if (pos != NULL)
	{
		fixed_t x, y, z;

		if (players[consoleplayer].camera != NULL)
		{
			x = players[consoleplayer].camera->x;
			y = players[consoleplayer].camera->z;
			z = players[consoleplayer].camera->y;
		}
		else
		{
			z = y = x = 0;
		}

		switch (type)
		{
		case SOURCE_None:
		default:
			break;

		case SOURCE_Actor:
//			assert(actor != NULL);
			if (actor != NULL)
			{
				x = actor->x;
				y = actor->z;
				z = actor->y;
			}
			break;

		case SOURCE_Sector:
			assert(sector != NULL);
			if (sector != NULL)
			{
				if (chanflags & CHAN_AREA)
				{
					CalcSectorSoundOrg(sector, channum, &x, &z, &y);
				}
				else
				{
					x = sector->soundorg[0];
					z = sector->soundorg[1];
					chanflags |= CHAN_LISTENERZ;
				}
			}
			break;

		case SOURCE_Polyobj:
			assert(poly != NULL);
			CalcPolyobjSoundOrg(poly, &x, &z, &y);
			break;

		case SOURCE_Unattached:
			pos->X = pt[0];
			pos->Y = !(chanflags & CHAN_LISTENERZ) ? pt[1] : FIXED2FLOAT(y);
			pos->Z = pt[2];
			break;
		}
		if (type != SOURCE_Unattached)
		{
			if ((chanflags & CHAN_LISTENERZ) && players[consoleplayer].camera != NULL)
			{
				y = players[consoleplayer].camera != NULL ? players[consoleplayer].camera->z : 0;
			}
			pos->X = FIXED2FLOAT(x);
			pos->Y = FIXED2FLOAT(y);
			pos->Z = FIXED2FLOAT(z);
		}
	}
	if (vel != NULL)
	{
		// Only actors maintain velocity information.
		if (type == SOURCE_Actor && actor != NULL)
		{
			vel->X = FIXED2FLOAT(actor->velx) * TICRATE;
			vel->Y = FIXED2FLOAT(actor->velz) * TICRATE;
			vel->Z = FIXED2FLOAT(actor->vely) * TICRATE;
		}
		else
		{
			vel->Zero();
		}
	}
}

//==========================================================================
//
// CalcSectorSoundOrg
//
// Returns the perceived sound origin for a sector. If the listener is
// inside the sector, then the origin is their location. Otherwise, the
// origin is from the nearest wall on the sector.
//
//==========================================================================

static void CalcSectorSoundOrg(const sector_t *sec, int channum, fixed_t *x, fixed_t *y, fixed_t *z)
{
	if (!(i_compatflags & COMPATF_SECTORSOUNDS))
	{
		// Are we inside the sector? If yes, the closest point is the one we're on.
		// [BB] We need to check if players[consoleplayer].camera is valid.
		if ( (P_PointInSector(*x, *y) == sec) && (players[consoleplayer].camera != NULL) )
		{
			*x = players[consoleplayer].camera->x;
			*y = players[consoleplayer].camera->y;
		}
		else
		{
			// Find the closest point on the sector's boundary lines and use
			// that as the perceived origin of the sound.
			sec->ClosestPoint(*x, *y, *x, *y);
		}
	}
	else
	{
		*x = sec->soundorg[0];
		*y = sec->soundorg[1];
	}

	// Set sound vertical position based on channel.
	if (channum == CHAN_FLOOR)
	{
		*z = MIN(sec->floorplane.ZatPoint(*x, *y), *z);
	}
	else if (channum == CHAN_CEILING)
	{
		*z = MAX(sec->ceilingplane.ZatPoint(*x, *y), *z);
	}
	else if (channum == CHAN_INTERIOR)
	{
		*z = clamp(*z, sec->floorplane.ZatPoint(*x, *y), sec->ceilingplane.ZatPoint(*x, *y));
	}
}

//==========================================================================
//
// CalcPolySoundOrg
//
// Returns the perceived sound origin for a polyobject. This is similar to
// CalcSectorSoundOrg, except there is no special case for being "inside"
// a polyobject, so the sound literally comes from the polyobject's walls.
// Vertical position of the sound always comes from the visible wall.
//
//==========================================================================

static void CalcPolyobjSoundOrg(const FPolyObj *poly, fixed_t *x, fixed_t *y, fixed_t *z)
{
	side_t *side;
	sector_t *sec;

	poly->ClosestPoint(*x, *y, *x, *y, &side);
	sec = side->sector;
	*z = clamp(*z, sec->floorplane.ZatPoint(*x, *y), sec->ceilingplane.ZatPoint(*x, *y));
}

//==========================================================================
//
// S_StartSound
//
// 0 attenuation means full volume over whole level.
// 0 < attenuation means to scale the distance by that amount when
//		calculating volume.
//
//==========================================================================

static FSoundChan *S_StartSound(AActor *actor, const sector_t *sec, const FPolyObj *poly,
	const FVector3 *pt, int channel, FSoundID sound_id, float volume, float attenuation,
	FRolloffInfo *forcedrolloff=NULL)
{
	// [BC] Server and predicting clients doesn't use music/sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER || CLIENT_PREDICT_IsPredicting())
		return NULL;

	// [BB] Don't play sounds while skipping in a demo.
	if ( CLIENTDEMO_IsSkipping() == true )
		return NULL;

	if (sound_id <= 0 || volume <= 0 || nosfx || nosound )
		return NULL;

	sfxinfo_t *sfx;
	int chanflags;
	int basepriority;
	int org_id;
	int pitch;
	FSoundChan *chan;
	FVector3 pos, vel;
	FRolloffInfo *rolloff;
	int type;

	if (actor != NULL)
	{
		type = SOURCE_Actor;
	}
	else if (sec != NULL)
	{
		type = SOURCE_Sector;
	}
	else if (poly != NULL)
	{
		type = SOURCE_Polyobj;
	}
	else if (pt != NULL)
	{
		type = SOURCE_Unattached;
	}
	else
	{
		type = SOURCE_None;
	}

	org_id = sound_id;
	chanflags = channel & ~7;
	channel &= 7;

	CalcPosVel(type, actor, sec, poly, &pt->X, channel, chanflags, &pos, &vel);

	if (i_compatflags & COMPATF_MAGICSILENCE)
	{ // For people who just can't play without a silent BFG.
		channel = CHAN_WEAPON;
	}

	// [BB] COMPATF_SILENTPICKUP needs to be checked no matter if COMPATF_MAGICSILENCE is set or not.
	if ((chanflags & CHAN_MAYBE_LOCAL) && (i_compatflags & COMPATF_SILENTPICKUP))
	{
		if (actor != NULL && actor != players[consoleplayer].camera)
		{
			return NULL;
		}
	}

	// [TP] Spectating players shouldn't make any noise.
	if ( actor && actor->player && actor->player->bSpectating )
		return NULL;

	sfx = &S_sfx[sound_id];

	// Scale volume according to SNDINFO data.
	volume = MIN(volume * sfx->Volume, 1.f);
	if (volume <= 0)
		return NULL;

	// When resolving a link we do not want to get the NearLimit of
	// the referenced sound so some additional checks are required
	int near_limit = sfx->NearLimit;
	float limit_range = sfx->LimitRange;
	rolloff = &sfx->Rolloff;

	// Resolve player sounds, random sounds, and aliases
	while (sfx->link != sfxinfo_t::NO_LINK)
	{
		if (sfx->bPlayerReserve)
		{
			sound_id = FSoundID(S_FindSkinnedSound (actor, sound_id));
			near_limit = S_sfx[sound_id].NearLimit;
			limit_range = S_sfx[sound_id].LimitRange;
			rolloff = &S_sfx[sound_id].Rolloff;
		}
		else if (sfx->bRandomHeader)
		{
			// Random sounds attenuate based on the original (random) sound as well as the chosen one.
			attenuation *= sfx->Attenuation;
			sound_id = FSoundID(S_PickReplacement (sound_id));
			if (near_limit < 0) 
			{
				near_limit = S_sfx[sound_id].NearLimit;
				limit_range = S_sfx[sound_id].LimitRange;
			}
			if (rolloff->MinDistance == 0)
			{
				rolloff = &S_sfx[sound_id].Rolloff;
			}
		}
		else
		{
			sound_id = FSoundID(sfx->link);
			if (near_limit < 0) 
			{
				near_limit = S_sfx[sound_id].NearLimit;
				limit_range = S_sfx[sound_id].LimitRange;
			}
			if (rolloff->MinDistance == 0)
			{
				rolloff = &S_sfx[sound_id].Rolloff;
			}
		}
		sfx = &S_sfx[sound_id];
	}

	// Attenuate the attenuation based on the sound.
	attenuation *= sfx->Attenuation;

	// The passed rolloff overrides any sound-specific rolloff.
	if (forcedrolloff != NULL && forcedrolloff->MinDistance != 0)
	{
		rolloff = forcedrolloff;
	}

	// If no valid rolloff was set, use the global default.
	if (rolloff->MinDistance == 0)
	{
		rolloff = &S_Rolloff;
	}

	// If this is a singular sound, don't play it if it's already playing.
	if (sfx->bSingular && S_CheckSingular(sound_id))
	{
		chanflags |= CHAN_EVICTED;
	}

	// If the sound is unpositioned or comes from the listener, it is
	// never limited.
	if (type == SOURCE_None || actor == players[consoleplayer].camera)
	{
		near_limit = 0;
	}

	// If this sound doesn't like playing near itself, don't play it if
	// that's what would happen.
	if (near_limit > 0 && S_CheckSoundLimit(sfx, pos, near_limit, limit_range, actor, channel))
	{
		chanflags |= CHAN_EVICTED;
	}

	// If the sound is blocked and not looped, return now. If the sound
	// is blocked and looped, pretend to play it so that it can
	// eventually play for real.
	if ((chanflags & (CHAN_EVICTED | CHAN_LOOP)) == CHAN_EVICTED)
	{
		return NULL;
	}

	// Make sure the sound is loaded.
	sfx = S_LoadSound(sfx);

	// The empty sound never plays.
	if (sfx->lumpnum == sfx_empty)
	{
		return NULL;
	}

	// Select priority.
	if (type == SOURCE_None || actor == players[consoleplayer].camera)
	{
		basepriority = 80;
	}
	else
	{
		basepriority = 0;
	}

	int seen = 0;
	if (actor != NULL && channel == CHAN_AUTO)
	{
		// Select a channel that isn't already playing something.
		// Try channel 0 first, then travel from channel 7 down.
		if (!S_IsChannelUsed(actor, 0, &seen))
		{
			channel = 0;
		}
		else
		{
			for (channel = 7; channel > 0; --channel)
			{
				if (!S_IsChannelUsed(actor, channel, &seen))
				{
					break;
				}
			}
			if (channel == 0)
			{ // Crap. No free channels.
				return NULL;
			}
		}
	}

	// If this actor is already playing something on the selected channel, stop it.
	if (type != SOURCE_None && ((actor == NULL && channel != CHAN_AUTO) || (actor != NULL && S_IsChannelUsed(actor, channel, &seen))))
	{
		for (chan = Channels; chan != NULL; chan = chan->NextChan)
		{
			if (chan->SourceType == type && chan->EntChannel == channel)
			{
				bool foundit;

				switch (type)
				{
				case SOURCE_Actor:		foundit = (chan->Actor == actor);	break;
				case SOURCE_Sector:		foundit = (chan->Sector == sec);	break;
				case SOURCE_Polyobj:	foundit = (chan->Poly == poly);		break;
				case SOURCE_Unattached:	foundit = (chan->Point[0] == pt->X && chan->Point[2] == pt->Z && chan->Point[1] == pt->Y);		break;
				default:				foundit = false;					break;
				}
				if (foundit)
				{
					S_StopChannel(chan);
					break;
				}
			}
		}
	}

	// sound is paused and a non-looped sound is being started.
	// Such a sound would play right after unpausing which wouldn't sound right.
	if (!(chanflags & CHAN_LOOP) && !(chanflags & (CHAN_UI|CHAN_NOPAUSE)) && SoundPaused)
	{
		return NULL;
	}

	// Vary the sfx pitches.
	if (sfx->PitchMask != 0)
	{
		pitch = NORM_PITCH - (M_Random() & sfx->PitchMask) + (M_Random() & sfx->PitchMask);
	}
	else
	{
		pitch = NORM_PITCH;
	}

	if (chanflags & CHAN_EVICTED)
	{
		chan = NULL;
	}
	else 
	{
		int startflags = 0;
		if (chanflags & CHAN_LOOP) startflags |= SNDF_LOOP;
		if (chanflags & CHAN_AREA) startflags |= SNDF_AREA;
		if (chanflags & (CHAN_UI|CHAN_NOPAUSE)) startflags |= SNDF_NOPAUSE;
		if (chanflags & CHAN_UI) startflags |= SNDF_NOREVERB;

		if (attenuation > 0)
		{
			SoundListener listener;
			S_SetListener(listener, players[consoleplayer].camera);
			chan = (FSoundChan*)GSnd->StartSound3D (sfx->data, &listener, volume, rolloff, attenuation, pitch, basepriority, pos, vel, channel, startflags, NULL);
		}
		else
		{
			chan = (FSoundChan*)GSnd->StartSound (sfx->data, volume, pitch, startflags, NULL);
		}
	}
	if (chan == NULL && (chanflags & CHAN_LOOP))
	{
		chan = (FSoundChan*)S_GetChannel(NULL);
		GSnd->MarkStartTime(chan);
		chanflags |= CHAN_EVICTED;
	}
	if (attenuation > 0)
	{
		chanflags |= CHAN_IS3D | CHAN_JUSTSTARTED;
	}
	else
	{
		chanflags |= CHAN_LISTENERZ | CHAN_JUSTSTARTED;
	}
	if (chan != NULL)
	{
		chan->SoundID = sound_id;
		chan->OrgID = FSoundID(org_id);
		chan->EntChannel = channel;
		chan->Volume = volume;
		chan->ChanFlags |= chanflags;
		chan->NearLimit = near_limit;
		chan->LimitRange = limit_range;
		chan->Pitch = pitch;
		chan->Priority = basepriority;
		chan->DistanceScale = attenuation;
		chan->SourceType = type;
		switch (type)
		{
		case SOURCE_Actor:		chan->Actor = actor;	break;
		case SOURCE_Sector:		chan->Sector = sec;		break;
		case SOURCE_Polyobj:	chan->Poly = poly;		break;
		case SOURCE_Unattached:	chan->Point[0] = pt->X; chan->Point[1] = pt->Y; chan->Point[2] = pt->Z;	break;
		default:										break;
		}
	}
	return chan;
}

//==========================================================================
//
// S_RestartSound
//
// Attempts to restart looping sounds that were evicted from their channels.
//
//==========================================================================

void S_RestartSound(FSoundChan *chan)
{
	if (CLIENT_PREDICT_IsPredicting())
		return;

	assert(chan->ChanFlags & CHAN_EVICTED);

	FSoundChan *ochan;
	sfxinfo_t *sfx = &S_sfx[chan->SoundID];

	// If this is a singular sound, don't play it if it's already playing.
	if (sfx->bSingular && S_CheckSingular(chan->SoundID))
		return;

	sfx = S_LoadSound(sfx);

	// The empty sound never plays.
	if (sfx->lumpnum == sfx_empty)
	{
		return;
	}

	int oldflags = chan->ChanFlags;

	int startflags = 0;
	if (chan->ChanFlags & CHAN_LOOP) startflags |= SNDF_LOOP;
	if (chan->ChanFlags & CHAN_AREA) startflags |= SNDF_AREA;
	if (chan->ChanFlags & (CHAN_UI|CHAN_NOPAUSE)) startflags |= SNDF_NOPAUSE;
	if (chan->ChanFlags & CHAN_ABSTIME) startflags |= SNDF_ABSTIME;

	if (chan->ChanFlags & CHAN_IS3D)
	{
		FVector3 pos, vel;

		CalcPosVel(chan, &pos, &vel);

		// If this sound doesn't like playing near itself, don't play it if
		// that's what would happen.
		if (chan->NearLimit > 0 && S_CheckSoundLimit(&S_sfx[chan->SoundID], pos, chan->NearLimit, chan->LimitRange, NULL, 0))
		{
			return;
		}

		SoundListener listener;
		S_SetListener(listener, players[consoleplayer].camera);

		chan->ChanFlags &= ~(CHAN_EVICTED|CHAN_ABSTIME);
		ochan = (FSoundChan*)GSnd->StartSound3D(sfx->data, &listener, chan->Volume, &chan->Rolloff, chan->DistanceScale, chan->Pitch,
			chan->Priority, pos, vel, chan->EntChannel, startflags, chan);
	}
	else
	{
		chan->ChanFlags &= ~(CHAN_EVICTED|CHAN_ABSTIME);
		ochan = (FSoundChan*)GSnd->StartSound(sfx->data, chan->Volume, chan->Pitch, startflags, chan);
	}
	assert(ochan == NULL || ochan == chan);
	if (ochan == NULL)
	{
		chan->ChanFlags = oldflags;
	}
}

//==========================================================================
//
// S_Sound - Unpositioned version
//
//==========================================================================

void S_Sound (int channel, FSoundID sound_id, float volume, float attenuation, bool bSoundOnClient) // [EP] Added bSoundOnClient.
{
	S_StartSound (NULL, NULL, NULL, NULL, channel, sound_id, volume, attenuation);

	// [EP] If we're the server, tell the clients to make a sound.
	if ( bSoundOnClient && ( NETWORK_GetState( ) == NETSTATE_SERVER ))
		SERVERCOMMANDS_Sound( channel, S_GetName( sound_id ), volume, attenuation );
}

//==========================================================================
//
// S_Sound - An actor is source
//
//==========================================================================

void S_Sound (AActor *ent, int channel, FSoundID sound_id, float volume, float attenuation, bool bSoundOnClient, int playerNumToSkip) // [EP] Added bSoundOnClient.
{
	if (ent == NULL || ent->Sector->Flags & SECF_SILENT)
		return;

	if ((channel & CHAN_LOCAL) && ( NETWORK_GetState( ) != NETSTATE_SERVER ) && !ent->CheckLocalView( consoleplayer ))
		return;

	S_StartSound (ent, NULL, NULL, NULL, channel, sound_id, volume, attenuation);

	// [EP] If we're the server, tell the clients to make a sound.
	if (bSoundOnClient && ( NETWORK_GetState( ) == NETSTATE_SERVER ))
		if (playerNumToSkip >= 0)
			SERVERCOMMANDS_SoundActor(ent, channel, S_GetName( sound_id ), volume, attenuation, playerNumToSkip, SVCF_SKIPTHISCLIENT);
		else
			SERVERCOMMANDS_SoundActor(ent, channel, S_GetName( sound_id ), volume, attenuation);
}

//==========================================================================
//
// S_SoundMinMaxDist - An actor is source
//
// Attenuation is specified as min and max distances, rather than a scalar.
//
//==========================================================================

void S_SoundMinMaxDist(AActor *ent, int channel, FSoundID sound_id, float volume, float mindist, float maxdist)
{
	if (ent == NULL || ent->Sector->Flags & SECF_SILENT)
		return;

	FRolloffInfo rolloff;

	rolloff.RolloffType = ROLLOFF_Linear;
	rolloff.MinDistance = mindist;
	rolloff.MaxDistance = maxdist;
	S_StartSound(ent, NULL, NULL, NULL, channel, sound_id, volume, 1, &rolloff);
}

//==========================================================================
//
// S_Sound - A polyobject is source
//
//==========================================================================

void S_Sound (const FPolyObj *poly, int channel, FSoundID sound_id, float volume, float attenuation)
{
	S_StartSound (NULL, NULL, poly, NULL, channel, sound_id, volume, attenuation);
}

//==========================================================================
//
// S_Sound - A point is source
//
//==========================================================================

void S_Sound (fixed_t x, fixed_t y, fixed_t z, int channel, FSoundID sound_id, float volume, float attenuation)
{
	FVector3 pt(FIXED2FLOAT(x), FIXED2FLOAT(z), FIXED2FLOAT(y));
	S_StartSound (NULL, NULL, NULL, &pt, channel, sound_id, volume, attenuation);
}

//==========================================================================
//
// S_Sound - An entire sector is source
//
//==========================================================================

void S_Sound (const sector_t *sec, int channel, FSoundID sfxid, float volume, float attenuation)
{
	S_StartSound (NULL, sec, NULL, NULL, channel, sfxid, volume, attenuation);
}

//==========================================================================
//
// S_LoadSound
//
// Returns a pointer to the sfxinfo with the actual sound data.
//
//==========================================================================

sfxinfo_t *S_LoadSound(sfxinfo_t *sfx)
{
	if (GSnd->IsNull()) return sfx;

	while (!sfx->data.isValid())
	{
		unsigned int i;

		// If the sound doesn't exist, replace it with the empty sound.
		if (sfx->lumpnum == -1)
		{
			sfx->lumpnum = sfx_empty;
		}
		
		// See if there is another sound already initialized with this lump. If so,
		// then set this one up as a link, and don't load the sound again.
		for (i = 0; i < S_sfx.Size(); i++)
		{
			if (S_sfx[i].data.isValid() && S_sfx[i].link == sfxinfo_t::NO_LINK && S_sfx[i].lumpnum == sfx->lumpnum)
			{
				DPrintf ("Linked %s to %s (%d)\n", sfx->name.GetChars(), S_sfx[i].name.GetChars(), i);
				sfx->link = i;
				// This is necessary to avoid using the rolloff settings of the linked sound if its
				// settings are different.
				if (sfx->Rolloff.MinDistance == 0) sfx->Rolloff = S_Rolloff;
				return &S_sfx[i];
			}
		}

		DPrintf("Loading sound \"%s\" (%td)\n", sfx->name.GetChars(), sfx - &S_sfx[0]);

		int size = Wads.LumpLength(sfx->lumpnum);
		if (size > 0)
		{
			BYTE *sfxdata;
			BYTE *sfxstart;
			FWadLump wlump = Wads.OpenLumpNum(sfx->lumpnum);
			sfxstart = sfxdata = new BYTE[size];
			wlump.Read(sfxdata, size);
			SDWORD len = LittleLong(((SDWORD *)sfxdata)[1]);

			// If the sound is voc, use the custom loader.
			if (strncmp ((const char *)sfxstart, "Creative Voice File", 19) == 0)
			{
				sfx->data = GSnd->LoadSoundVoc(sfxstart, len);
			}
			// If the sound is raw, just load it as such.
			// Otherwise, try the sound as DMX format.
			// If that fails, let FMOD try and figure it out.
			else if (sfx->bLoadRAW ||
				(((BYTE *)sfxdata)[0] == 3 && ((BYTE *)sfxdata)[1] == 0 && len <= size - 8))
			{
				int frequency;

				if (sfx->bLoadRAW)
				{
					len = Wads.LumpLength (sfx->lumpnum);
					frequency = (sfx->bForce22050 ? 22050 : 11025);
				}
				else
				{
					frequency = LittleShort(((WORD *)sfxdata)[1]);
					if (frequency == 0)
					{
						frequency = 11025;
					}
					sfxstart = sfxdata + 8;
				}
				sfx->data = GSnd->LoadSoundRaw(sfxstart, len, frequency, 1, 8, sfx->LoopStart);
			}
			else
			{
				len = Wads.LumpLength (sfx->lumpnum);
				sfx->data = GSnd->LoadSound(sfxstart, len);
			}
			
			if (sfxdata != NULL)
			{
				delete[] sfxdata;
			}
		}

		if (!sfx->data.isValid())
		{
			if (sfx->lumpnum != sfx_empty)
			{
				sfx->lumpnum = sfx_empty;
				continue;
			}
		}
		break;
	}
	return sfx;
}

//==========================================================================
//
// S_CheckSingular
//
// Returns true if a copy of this sound is already playing.
//
//==========================================================================

bool S_CheckSingular(int sound_id)
{
	for (FSoundChan *chan = Channels; chan != NULL; chan = chan->NextChan)
	{
		if (chan->OrgID == sound_id)
		{
			return true;
		}
	}
	return false;
}

//==========================================================================
//
// S_CheckSoundLimit
//
// Limits the number of nearby copies of a sound that can play near
// each other. If there are NearLimit instances of this sound already
// playing within sqrt(limit_range) (typically 256 units) of the new sound, the
// new sound will not start.
//
// If an actor is specified, and it is already playing the same sound on
// the same channel, this sound will not be limited. In this case, we're
// restarting an already playing sound, so there's no need to limit it.
//
// Returns true if the sound should not play.
//
//==========================================================================

bool S_CheckSoundLimit(sfxinfo_t *sfx, const FVector3 &pos, int near_limit, float limit_range,
	AActor *actor, int channel)
{
	FSoundChan *chan;
	int count;
	
	for (chan = Channels, count = 0; chan != NULL && count < near_limit; chan = chan->NextChan)
	{
		if (!(chan->ChanFlags & CHAN_EVICTED) && &S_sfx[chan->SoundID] == sfx)
		{
			FVector3 chanorigin;

			if (actor != NULL && chan->EntChannel == channel &&
				chan->SourceType == SOURCE_Actor && chan->Actor == actor)
			{ // We are restarting a playing sound. Always let it play.
				return false;
			}

			CalcPosVel(chan, &chanorigin, NULL);
			if ((chanorigin - pos).LengthSquared() <= limit_range)
			{
				count++;
			}
		}
	}
	return count >= near_limit;
}

//==========================================================================
//
// [BB] S_IsMusicPaused
//
//==========================================================================

bool S_IsMusicPaused ( void )
{
	return ( MusicPaused );
}

//==========================================================================
//
// [BB] S_StopSoundID
//
//==========================================================================

void S_StopSoundID (int sound_id, int channel)
{
	for (FSoundChan *chan = Channels; chan != NULL; chan = chan->NextChan)
	{
		if ( (chan->SoundID == sound_id) && (chan->EntChannel == channel) )
		{
			S_StopChannel(chan);
		}
	}
}

//==========================================================================
//
// S_StopSound
//
// Stops an unpositioned sound from playing on a specific channel.
//
//==========================================================================

void S_StopSound (int channel)
{
	FSoundChan *chan = Channels;
	while (chan != NULL)
	{
		FSoundChan *next = chan->NextChan;
		if (chan->SourceType == SOURCE_None &&
			(chan->EntChannel == channel || (i_compatflags & COMPATF_MAGICSILENCE)))
		{
			S_StopChannel(chan);
		}
		chan = next;
	}
}

//==========================================================================
//
// S_StopSound
//
// Stops a sound from a single actor from playing on a specific channel.
//
//==========================================================================

void S_StopSound (AActor *actor, int channel)
{
	FSoundChan *chan = Channels;
	while (chan != NULL)
	{
		FSoundChan *next = chan->NextChan;
		if (chan->SourceType == SOURCE_Actor &&
			chan->Actor == actor &&
			(chan->EntChannel == channel || (i_compatflags & COMPATF_MAGICSILENCE)))
		{
			S_StopChannel(chan);
		}
		chan = next;
	}
}

//==========================================================================
//
// S_StopSound
//
// Stops a sound from a single sector from playing on a specific channel.
//
//==========================================================================

void S_StopSound (const sector_t *sec, int channel)
{
	FSoundChan *chan = Channels;
	while (chan != NULL)
	{
		FSoundChan *next = chan->NextChan;
		if (chan->SourceType == SOURCE_Sector &&
			chan->Sector == sec &&
			(chan->EntChannel == channel || (i_compatflags & COMPATF_MAGICSILENCE)))
		{
			S_StopChannel(chan);
		}
		chan = next;
	}
}

//==========================================================================
//
// S_StopSound
//
// Stops a sound from a single polyobject from playing on a specific channel.
//
//==========================================================================

void S_StopSound (const FPolyObj *poly, int channel)
{
	FSoundChan *chan = Channels;
	while (chan != NULL)
	{
		FSoundChan *next = chan->NextChan;
		if (chan->SourceType == SOURCE_Polyobj &&
			chan->Poly == poly &&
			(chan->EntChannel == channel || (i_compatflags & COMPATF_MAGICSILENCE)))
		{
			S_StopChannel(chan);
		}
		chan = next;
	}
}

//==========================================================================
//
// S_StopAllChannels
//
//==========================================================================

void S_StopAllChannels ()
{
	SN_StopAllSequences();

	FSoundChan *chan = Channels;
	while (chan != NULL)
	{
		FSoundChan *next = chan->NextChan;
		S_StopChannel(chan);
		chan = next;
	}
	GSnd->UpdateSounds();
}

//==========================================================================
//
// [BB] S_StopAllSoundsFromActor
//
//==========================================================================

void S_StopAllSoundsFromActor (AActor *ent)
{
	for (FSoundChan *chan = Channels; chan != NULL; chan = chan->NextChan)
	{
		if ( chan->Actor == ent )
		{
			S_StopChannel(chan);
		}
	}
}

//==========================================================================
//
// S_RelinkSound
//
// Moves all the sounds from one thing to another. If the destination is
// NULL, then the sound becomes a positioned sound.
//==========================================================================

void S_RelinkSound (AActor *from, AActor *to)
{
	if (from == NULL)
		return;

	FSoundChan *chan = Channels;
	while (chan != NULL)
	{
		FSoundChan *next = chan->NextChan;
		if (chan->SourceType == SOURCE_Actor && chan->Actor == from)
		{
			if (to != NULL)
			{
				chan->Actor = to;
			}
			else if (!(chan->ChanFlags & CHAN_LOOP))
			{
				chan->Actor = NULL;
				chan->SourceType = SOURCE_Unattached;
				chan->Point[0] = FIXED2FLOAT(from->x);
				chan->Point[1] = FIXED2FLOAT(from->z);
				chan->Point[2] = FIXED2FLOAT(from->y);
			}
			else
			{
				S_StopChannel(chan);
			}
		}
		chan = next;
	}
}


//==========================================================================
//
// S_ChangeSoundVolume
//
//==========================================================================

bool S_ChangeSoundVolume(AActor *actor, int channel, float volume)
{
	for (FSoundChan *chan = Channels; chan != NULL; chan = chan->NextChan)
	{
		if (chan->SourceType == SOURCE_Actor &&
			chan->Actor == actor &&
			(chan->EntChannel == channel || (i_compatflags & COMPATF_MAGICSILENCE)))
		{
			GSnd->ChannelVolume(chan, volume);
			chan->Volume = volume;
			return true;
		}
	}
	return false;
}

//==========================================================================
//
// S_GetSoundPlayingInfo
//
// Is a sound being played by a specific emitter?
//==========================================================================

bool S_GetSoundPlayingInfo (const AActor *actor, int sound_id)
{
	if (sound_id > 0)
	{
		for (FSoundChan *chan = Channels; chan != NULL; chan = chan->NextChan)
		{
			if (chan->OrgID == sound_id &&
				chan->SourceType == SOURCE_Actor &&
				chan->Actor == actor)
			{
				return true;
			}
		}
	}
	return false;
}

bool S_GetSoundPlayingInfo (const sector_t *sec, int sound_id)
{
	if (sound_id > 0)
	{
		for (FSoundChan *chan = Channels; chan != NULL; chan = chan->NextChan)
		{
			if (chan->OrgID == sound_id &&
				chan->SourceType == SOURCE_Sector &&
				chan->Sector == sec)
			{
				return true;
			}
		}
	}
	return false;
}

bool S_GetSoundPlayingInfo (const FPolyObj *poly, int sound_id)
{
	if (sound_id > 0)
	{
		for (FSoundChan *chan = Channels; chan != NULL; chan = chan->NextChan)
		{
			if (chan->OrgID == sound_id &&
				chan->SourceType == SOURCE_Polyobj &&
				chan->Poly == poly)
			{
				return true;
			}
		}
	}
	return false;
}

//==========================================================================
//
// S_IsChannelUsed
//
// Returns true if the channel is in use. Also fills in a bitmask of
// channels seen while scanning for this one, to make searching for unused
// channels faster. Initialize seen to 0 for the first call.
//
//==========================================================================

static bool S_IsChannelUsed(AActor *actor, int channel, int *seen)
{
	if (*seen & (1 << channel))
	{
		return true;
	}
	for (FSoundChan *chan = Channels; chan != NULL; chan = chan->NextChan)
	{
		if (chan->SourceType == SOURCE_Actor && chan->Actor == actor)
		{
			*seen |= 1 << chan->EntChannel;
			if (chan->EntChannel == channel)
			{
				return true;
			}
		}
	}
	return false;
}

//==========================================================================
//
// S_IsActorPlayingSomething
//
//==========================================================================

bool S_IsActorPlayingSomething (AActor *actor, int channel, int sound_id)
{
	if (i_compatflags & COMPATF_MAGICSILENCE)
	{
		channel = 0;
	}

	for (FSoundChan *chan = Channels; chan != NULL; chan = chan->NextChan)
	{
		if (chan->SourceType == SOURCE_Actor && chan->Actor == actor)
		{
			if (channel == 0 || chan->EntChannel == channel)
			{
				return sound_id <= 0 || chan->OrgID == sound_id;
			}
		}
	}
	return false;
}

//==========================================================================
//
// S_PauseSound
//
// Stop music and sound effects, during game PAUSE.
//==========================================================================

void S_PauseSound (bool notmusic, bool notsfx)
{
	// [BC] Server doesn't use music/sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	if (!notmusic && mus_playing.handle && !MusicPaused)
	{
		mus_playing.handle->Pause();
		MusicPaused = true;
	}
	if (!notsfx)
	{
		SoundPaused = true;
		GSnd->SetSfxPaused (true, 0);
	}
}

//==========================================================================
//
// S_ResumeSound
//
// Resume music and sound effects, after game PAUSE.
//==========================================================================

void S_ResumeSound (bool notsfx)
{
	// [BC] Server doesn't use music/sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	if (mus_playing.handle && MusicPaused)
	{
		mus_playing.handle->Resume();
		MusicPaused = false;
	}
	if (!notsfx)
	{
		SoundPaused = false;
		GSnd->SetSfxPaused (false, 0);
	}
}

//==========================================================================
//
// S_SetSoundPaused
//
// Called with state non-zero when the app is active, zero when it isn't.
//
//==========================================================================

void S_SetSoundPaused (int state)
{
	if (state)
	{
		if (paused <= 0)
		{
			S_ResumeSound(true);
			if (GSnd != NULL)
			{
				GSnd->SetInactive(SoundRenderer::INACTIVE_Active);
			}
			// [BB] !netgame -> (NETWORK_GetState( ) == NETSTATE_SINGLE)
			if ((NETWORK_GetState( ) == NETSTATE_SINGLE)
#ifdef _DEBUG
				&& !demoplayback
#endif
				)
			{
				paused = 0;
			}
		}
	}
	else
	{
		if (paused == 0)
		{
			S_PauseSound(false, true);
			if (GSnd !=  NULL)
			{
				GSnd->SetInactive(gamestate == GS_LEVEL || gamestate == GS_TITLELEVEL ?
					SoundRenderer::INACTIVE_Complete :
					SoundRenderer::INACTIVE_Mute);
			}
			// [BB] !netgame -> (NETWORK_GetState( ) == NETSTATE_SINGLE)
			if ((NETWORK_GetState( ) == NETSTATE_SINGLE)
#ifdef _DEBUG
				&& !demoplayback
#endif
				)
			{
				paused = -1;
			}
		}
	}
}

//==========================================================================
//
// S_EvictAllChannels
//
// Forcibly evicts all channels so that there are none playing, but all
// information needed to restart them is retained.
//
//==========================================================================

void S_EvictAllChannels()
{
	FSoundChan *chan, *next;

	for (chan = Channels; chan != NULL; chan = next)
	{
		next = chan->NextChan;

		if (!(chan->ChanFlags & CHAN_EVICTED))
		{
			chan->ChanFlags |= CHAN_EVICTED;
			if (chan->SysChannel != NULL)
			{
				if (!(chan->ChanFlags & CHAN_ABSTIME))
				{
					chan->StartTime.AsOne = GSnd ? GSnd->GetPosition(chan) : 0;
					chan->ChanFlags |= CHAN_ABSTIME;
				}
				S_StopChannel(chan);
			}
//			assert(chan->NextChan == next);
		}
	}
}

//==========================================================================
//
// S_RestoreEvictedChannel
//
// Recursive helper for S_RestoreEvictedChannels().
//
//==========================================================================

void S_RestoreEvictedChannel(FSoundChan *chan)
{
	if (chan == NULL)
	{
		return;
	}
	S_RestoreEvictedChannel(chan->NextChan);
	if (chan->ChanFlags & CHAN_EVICTED)
	{
		S_RestartSound(chan);
		if (!(chan->ChanFlags & CHAN_LOOP))
		{
			if (chan->ChanFlags & CHAN_EVICTED)
			{ // Still evicted and not looping? Forget about it.
				S_ReturnChannel(chan);
			}
			else if (!(chan->ChanFlags & CHAN_JUSTSTARTED))
			{ // Should this sound become evicted again, it's okay to forget about it.
				chan->ChanFlags |= CHAN_FORGETTABLE;
			}
		}
	}
	else if (chan->SysChannel == NULL && (chan->ChanFlags & (CHAN_FORGETTABLE | CHAN_LOOP)) == CHAN_FORGETTABLE)
	{
		S_ReturnChannel(chan);
	}
}

//==========================================================================
//
// S_RestoreEvictedChannels
//
// Restarts as many evicted channels as possible. Any channels that could
// not be started and are not looping are moved to the free pool.
//
//==========================================================================

void S_RestoreEvictedChannels()
{
	// Restart channels in the same order they were originally played.
	S_RestoreEvictedChannel(Channels);
}

//==========================================================================
//
// S_UpdateSounds
//
// Updates music & sounds
//==========================================================================

void S_UpdateSounds (AActor *listenactor)
{
	FVector3 pos, vel;
	SoundListener listener;

	// [BC] Server doesn't use music/sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	I_UpdateMusic();

	// [RH] Update music and/or playlist. IsPlaying() must be called
	// to attempt to reconnect to broken net streams and to advance the
	// playlist when the current song finishes.
	if (mus_playing.handle != NULL &&
		!mus_playing.handle->IsPlaying() &&
		PlayList)
	{
		PlayList->Advance();
		S_ActivatePlayList(false);
	}

	// should never happen
	S_SetListener(listener, listenactor);

	for (FSoundChan *chan = Channels; chan != NULL; chan = chan->NextChan)
	{
		if ((chan->ChanFlags & (CHAN_EVICTED | CHAN_IS3D)) == CHAN_IS3D)
		{
			CalcPosVel(chan, &pos, &vel);
			GSnd->UpdateSoundParams3D(&listener, chan, !!(chan->ChanFlags & CHAN_AREA), pos, vel);
		}
		chan->ChanFlags &= ~CHAN_JUSTSTARTED;
	}

	SN_UpdateActiveSequences();


	GSnd->UpdateListener(&listener);
	GSnd->UpdateSounds();

	if (level.time >= RestartEvictionsAt)
	{
		RestartEvictionsAt = 0;
		S_RestoreEvictedChannels();
	}
}

//==========================================================================
//
// Sets the internal listener structure
//
//==========================================================================

static void S_SetListener(SoundListener &listener, AActor *listenactor)
{
	if (listenactor != NULL)
	{
		listener.angle = (float)(listenactor->angle) * ((float)PI / 2147483648.f);
		/*
		listener.velocity.X = listenactor->velx * (TICRATE/65536.f);
		listener.velocity.Y = listenactor->velz * (TICRATE/65536.f);
		listener.velocity.Z = listenactor->vely * (TICRATE/65536.f);
		*/
		listener.velocity.Zero();
		listener.position.X = FIXED2FLOAT(listenactor->x);
		listener.position.Y = FIXED2FLOAT(listenactor->z);
		listener.position.Z = FIXED2FLOAT(listenactor->y);
		listener.underwater = listenactor->waterlevel == 3;
		assert(zones != NULL);
		listener.Environment = zones[listenactor->Sector->ZoneNumber].Environment;
		listener.valid = true;
	}
	else
	{
		listener.angle = 0;
		listener.position.Zero();
		listener.velocity.Zero();
		listener.underwater = false;
		listener.Environment = NULL;
		listener.valid = false;
	}
}



//==========================================================================
//
// S_GetRolloff
//
//==========================================================================

float S_GetRolloff(FRolloffInfo *rolloff, float distance, bool logarithmic)
{
	if (rolloff == NULL)
	{
		return 0;
	}

	if (distance <= rolloff->MinDistance)
	{
		return 1;
	}
	if (rolloff->RolloffType == ROLLOFF_Log)
	{ // Logarithmic rolloff has no max distance where it goes silent.
		return rolloff->MinDistance / (rolloff->MinDistance + rolloff->RolloffFactor * (distance - rolloff->MinDistance));
	}
	if (distance >= rolloff->MaxDistance)
	{
		return 0;
	}

	float volume = (rolloff->MaxDistance - distance) / (rolloff->MaxDistance - rolloff->MinDistance);
	if (rolloff->RolloffType == ROLLOFF_Custom && S_SoundCurve != NULL)
	{
		volume = S_SoundCurve[int(S_SoundCurveSize * (1 - volume))] / 127.f;
	}
	if (logarithmic)
	{
		if (rolloff->RolloffType == ROLLOFF_Linear)
		{
			return volume;
		}
		else
		{
			return float((powf(10.f, volume) - 1.) / 9.);
		}
	}
	else
	{
		if (rolloff->RolloffType == ROLLOFF_Linear)
		{
			return float(log10(9. * volume + 1.));
		}
		else
		{
			return volume;
		}
	}
}


//==========================================================================
//
// S_ChannelEnded (callback for sound interface code)
//
//==========================================================================

void S_ChannelEnded(FISoundChannel *ichan)
{
	FSoundChan *schan = static_cast<FSoundChan*>(ichan);
	bool evicted;

	if (schan != NULL)
	{
		// If the sound was stopped with GSnd->StopSound(), then we know
		// it wasn't evicted. Otherwise, if it's looping, it must have
		// been evicted. If it's not looping, then it was evicted if it
		// didn't reach the end of its playback.
		if (schan->ChanFlags & CHAN_FORGETTABLE)
		{
			evicted = false;
		}
		else if (schan->ChanFlags & (CHAN_LOOP | CHAN_EVICTED))
		{
			evicted = true;
		}
		else
		{ 
			unsigned int pos = GSnd->GetPosition(schan);
			unsigned int len = GSnd->GetSampleLength(S_sfx[schan->SoundID].data);
			if (pos == 0)
			{
				evicted = !!(schan->ChanFlags & CHAN_JUSTSTARTED);
			}
			else
			{
				evicted = (pos < len);
			}
		}
		/*
		else
		{
			evicted = false;
		}
		*/
		if (!evicted)
		{
			S_ReturnChannel(schan);
		}
		else
		{
			schan->ChanFlags |= CHAN_EVICTED;
			schan->SysChannel = NULL;
		}
	}
}

//==========================================================================
//
// S_ChannelVirtualChanged (callback for sound interface code)
//
//==========================================================================

void S_ChannelVirtualChanged(FISoundChannel *ichan, bool is_virtual)
{
	FSoundChan *schan = static_cast<FSoundChan*>(ichan);
	if (is_virtual)
	{
		schan->ChanFlags |= CHAN_VIRTUAL;
	}
	else
	{
		schan->ChanFlags &= ~CHAN_VIRTUAL;
	}
}

//==========================================================================
//
// S_StopChannel
//
//==========================================================================

void S_StopChannel(FSoundChan *chan)
{
	if (chan == NULL || CLIENT_PREDICT_IsPredicting())
		return;

	if (chan->SysChannel != NULL)
	{
		// S_EvictAllChannels() will set the CHAN_EVICTED flag to indicate
		// that it wants to keep all the channel information around.
		if (!(chan->ChanFlags & CHAN_EVICTED))
		{
			chan->ChanFlags |= CHAN_FORGETTABLE;
			if (chan->SourceType == SOURCE_Actor)
			{
				chan->Actor = NULL;
			}
		}
		GSnd->StopChannel(chan);
	}
	else
	{
		S_ReturnChannel(chan);
	}
}

//==========================================================================
//
// (FArchive &) << (FSoundID &)
//
//==========================================================================

FArchive &operator<<(FArchive &arc, FSoundID &sid)
{
	if (arc.IsStoring())
	{
		arc.WriteName((const char *)sid);
	}
	else
	{
		sid = arc.ReadName();
	}
	return arc;
}

//==========================================================================
//
// (FArchive &) << (FSoundChan &)
//
//==========================================================================

static FArchive &operator<<(FArchive &arc, FSoundChan &chan)
{
	arc << chan.SourceType;
	switch (chan.SourceType)
	{
	case SOURCE_None:								break;
	case SOURCE_Actor:		arc << chan.Actor;		break;
	case SOURCE_Sector:		arc << chan.Sector;		break;
	case SOURCE_Polyobj:	arc << chan.Poly;		break;
	case SOURCE_Unattached:	arc << chan.Point[0] << chan.Point[1] << chan.Point[2];	break;
	default:				I_Error("Unknown sound source type %d\n", chan.SourceType);	break;
	}
	arc << chan.SoundID
		<< chan.OrgID
		<< chan.Volume
		<< chan.DistanceScale
		<< chan.Pitch
		<< chan.ChanFlags
		<< chan.EntChannel
		<< chan.Priority
		<< chan.NearLimit
		<< chan.StartTime
		<< chan.Rolloff.RolloffType
		<< chan.Rolloff.MinDistance
		<< chan.Rolloff.MaxDistance
		<< chan.LimitRange;

	return arc;
}

//==========================================================================
//
// S_SerializeSounds
//
//==========================================================================

void S_SerializeSounds(FArchive &arc)
{
	FSoundChan *chan;

	GSnd->Sync(true);

	if (arc.IsStoring())
	{
		TArray<FSoundChan *> chans;

		// Count channels and accumulate them so we can store them in
		// reverse order. That way, they will be in the same order when
		// reloaded later as they are now.
		for (chan = Channels; chan != NULL; chan = chan->NextChan)
		{
			// If the sound is forgettable, this is as good a time as
			// any to forget about it. And if it's a UI sound, it shouldn't
			// be stored in the savegame.
			if (!(chan->ChanFlags & (CHAN_FORGETTABLE | CHAN_UI)))
			{
				chans.Push(chan);
			}
		}

		arc.WriteCount(chans.Size());

		for (unsigned int i = chans.Size(); i-- != 0; )
		{
			// Replace start time with sample position.
			QWORD start = chans[i]->StartTime.AsOne;
			chans[i]->StartTime.AsOne = GSnd ? GSnd->GetPosition(chans[i]) : 0;
			arc << *chans[i];
			chans[i]->StartTime.AsOne = start;
		}
	}
	else
	{
		unsigned int count;

		S_StopAllChannels();
		count = arc.ReadCount();
		for (unsigned int i = 0; i < count; ++i)
		{
			chan = (FSoundChan*)S_GetChannel(NULL);
			arc << *chan;
			// Sounds always start out evicted when restored from a save.
			chan->ChanFlags |= CHAN_EVICTED | CHAN_ABSTIME;
		}
		// The two tic delay is to make sure any screenwipes have finished.
		// This needs to be two because the game is run for one tic before
		// the wipe so that it can produce a screen to wipe to. So if we
		// only waited one tic to restart the sounds, they would start
		// playing before the wipe, and depending on the synchronization
		// between the main thread and the mixer thread at the time, the
		// sounds might be heard briefly before pausing for the wipe.
		RestartEvictionsAt = level.time + 2;
	}
	DSeqNode::SerializeSequences(arc);
	GSnd->Sync(false);
	GSnd->UpdateSounds();
}

//==========================================================================
//
// S_ActivatePlayList
//
// Plays the next song in the playlist. If no songs in the playlist can be
// played, then it is deleted.
//==========================================================================

void S_ActivatePlayList (bool goBack)
{
	int startpos, pos;

	// [BC] Server doesn't use music/sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	startpos = pos = PlayList->GetPosition ();
	S_StopMusic (true);
	while (!S_ChangeMusic (PlayList->GetSong (pos), 0, false, true))
	{
		pos = goBack ? PlayList->Backup () : PlayList->Advance ();
		if (pos == startpos)
		{
			delete PlayList;
			PlayList = NULL;
			Printf ("Cannot play anything in the playlist.\n");
			return;
		}
	}
}

//==========================================================================
//
// S_ChangeCDMusic
//
// Starts a CD track as music.
//==========================================================================

bool S_ChangeCDMusic (int track, unsigned int id, bool looping)
{
	char temp[32];

	// [BC] Server doesn't use music/sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return ( false );

	if (id != 0)
	{
		mysnprintf (temp, countof(temp), ",CD,%d,%x", track, id);
	}
	else
	{
		mysnprintf (temp, countof(temp), ",CD,%d", track);
	}
	return S_ChangeMusic (temp, 0, looping);
}

//==========================================================================
//
// S_StartMusic
//
// Starts some music with the given name.
//==========================================================================

bool S_StartMusic (const char *m_id)
{
	return S_ChangeMusic (m_id, 0, false);
}

//==========================================================================
//
// S_ChangeMusic
//
// Starts playing a music, possibly looping.
//
// [RH] If music is a MOD, starts it at position order. If name is of the
// format ",CD,<track>,[cd id]" song is a CD track, and if [cd id] is
// specified, it will only be played if the specified CD is in a drive.
//==========================================================================

TArray<BYTE> musiccache;

bool S_ChangeMusic (const char *musicname, int order, bool looping, bool force)
{
	// [BC] Server doesn't use music/sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return ( false );

	if (!force && PlayList)
	{ // Don't change if a playlist is active
		return false;
	}

	// allow specifying "*" as a placeholder to play the level's default music.
	if (musicname != NULL && !strcmp(musicname, "*"))
	{
		if (gamestate == GS_LEVEL || gamestate == GS_TITLELEVEL)
		{
			musicname = level.Music;
			order = level.musicorder;
		}
		else
		{
			musicname = NULL;
		}
	}

	if (musicname == NULL || musicname[0] == 0)
	{
		// Don't choke if the map doesn't have a song attached
		S_StopMusic (true);
		mus_playing.name = "";
		LastSong = "";
		return true;
	}

	FString DEH_Music;
	if (musicname[0] == '$')
	{
		// handle dehacked replacement.
		// Any music name defined this way needs to be prefixed with 'D_' because
		// Doom.exe does not contain the prefix so these strings don't either.
		const char * mus_string = GStrings[musicname+1];
		if (mus_string != NULL)
		{
			DEH_Music << "D_" << mus_string;
			musicname = DEH_Music;
		}
	}

	if (!mus_playing.name.IsEmpty() &&
		mus_playing.handle != NULL &&
		stricmp (mus_playing.name, musicname) == 0 &&
		mus_playing.handle->m_Looping == looping)
	{
		if (order != mus_playing.baseorder)
		{
			if (mus_playing.handle->SetSubsong(order))
			{
				mus_playing.baseorder = order;
			}
		}
		else if (!mus_playing.handle->IsPlaying())
		{
			mus_playing.handle->Play(looping, order);
		}
		return true;
	}

	if (strnicmp (musicname, ",CD,", 4) == 0)
	{
		int track = strtoul (musicname+4, NULL, 0);
		const char *more = strchr (musicname+4, ',');
		unsigned int id = 0;

		if (more != NULL)
		{
			id = strtoul (more+1, NULL, 16);
		}
		S_StopMusic (true);
		mus_playing.handle = I_RegisterCDSong (track, id);
	}
	else
	{
		int lumpnum = -1;
		int offset = 0, length = 0;
		int device = MDEV_DEFAULT;
		MusInfo *handle = NULL;
		FName musicasname = musicname;

		FName *aliasp = MusicAliases.CheckKey(musicasname);
		if (aliasp != NULL) 
		{
			musicname = (musicasname = *aliasp).GetChars();
			if (musicasname == NAME_None) return true;
		}

		int *devp = MidiDevices.CheckKey(musicasname);
		if (devp != NULL) device = *devp;

		// Strip off any leading file:// component.
		if (strncmp(musicname, "file://", 7) == 0)
		{
			musicname += 7;
		}

		if (!FileExists (musicname))
		{
			if ((lumpnum = Wads.CheckNumForFullName (musicname, true, ns_music)) == -1)
			{
				if (strstr(musicname, "://") > musicname)
				{
					// Looks like a URL; try it as such.
					handle = I_RegisterURLSong(musicname);
					if (handle == NULL)
					{
						Printf ("Could not open \"%s\"\n", musicname);
						return false;
					}
				}
				else
				{
					Printf ("Music \"%s\" not found\n", musicname);
					return false;
				}
			}
			if (handle == NULL)
			{
				if (!Wads.IsUncompressedFile(lumpnum))
				{
					// We must cache the music data and use it from memory.

					// shut down old music before reallocating and overwriting the cache!
					S_StopMusic (true);

					offset = -1;							// this tells the low level code that the music 
															// is being used from memory
					length = Wads.LumpLength (lumpnum);
					if (length == 0)
					{
						return false;
					}
					musiccache.Resize(length);
					Wads.ReadLump(lumpnum, &musiccache[0]);
				}
				else
				{
					offset = Wads.GetLumpOffset (lumpnum);
					length = Wads.LumpLength (lumpnum);
					if (length == 0)
					{
						return false;
					}
				}
			}
		}

		// shutdown old music
		S_StopMusic (true);

		// Just record it if volume is 0
		if (snd_musicvolume <= 0)
		{
			mus_playing.loop = looping;
			mus_playing.name = musicname;
			mus_playing.baseorder = order;
			LastSong = musicname;
			return true;
		}

		// load & register it
		if (handle != NULL)
		{
			mus_playing.handle = handle;
		}
		else if (offset != -1)
		{
			mus_playing.handle = I_RegisterSong (lumpnum != -1 ?
				Wads.GetWadFullName (Wads.GetLumpFile (lumpnum)) :
				musicname, NULL, offset, length, device);
		}
		else
		{
			mus_playing.handle = I_RegisterSong (NULL, &musiccache[0], -1, length, device);
		}
	}

	mus_playing.loop = looping;
	mus_playing.name = musicname;
	mus_playing.baseorder = 0;
	LastSong = "";

	if (mus_playing.handle != 0)
	{ // play it
		mus_playing.handle->Start(looping, S_GetMusicVolume (musicname), order);
		mus_playing.baseorder = order;
		return true;
	}
	return false;
}

//==========================================================================
//
// S_RestartMusic
//
// Must only be called from snd_reset in i_sound.cpp!
//==========================================================================

void S_RestartMusic ()
{
	// [BC] Server doesn't use music/sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	if (!LastSong.IsEmpty())
	{
		FString song = LastSong;
		LastSong = "";
		S_ChangeMusic (song, mus_playing.baseorder, mus_playing.loop, true);
	}
}

//==========================================================================
//
// S_MIDIDeviceChanged
//
//==========================================================================

void S_MIDIDeviceChanged()
{
	if (mus_playing.handle != NULL && mus_playing.handle->IsMIDI())
	{
		mus_playing.handle->Stop();
		mus_playing.handle->Start(mus_playing.loop, -1, mus_playing.baseorder);
	}
}

//==========================================================================
//
// S_GetMusic
//
//==========================================================================

int S_GetMusic (char **name)
{
	int order;

	// [BC] Server doesn't use music/sound, [BB] but saves the values.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		*name = copystring ( SERVER_GetMapMusic( ) );
		return ( SERVER_GetMapMusicOrder( ) );
	}

	if (mus_playing.name)
	{
		*name = copystring (mus_playing.name);
		order = mus_playing.baseorder;
	}
	else
	{
		*name = NULL;
		order = 0;
	}
	return order;
}

//==========================================================================
//
// S_StopMusic
//
//==========================================================================

void S_StopMusic (bool force)
{
	// [BC] Server doesn't use music/sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	// [RH] Don't stop if a playlist is active.
	if ((force || PlayList == NULL) && !mus_playing.name.IsEmpty())
	{
		if (mus_playing.handle != NULL)
		{
			if (MusicPaused)
				mus_playing.handle->Resume();

			mus_playing.handle->Stop();
			delete mus_playing.handle;
			mus_playing.handle = NULL;
		}
		LastSong = mus_playing.name;
		mus_playing.name = "";
	}
}

//==========================================================================
//
// CCMD playsound
//
//==========================================================================

CCMD (playsound)
{
	// [BC] Server doesn't use music/sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	if (argv.argc() > 1)
	{
		FSoundID id = argv[1];
		if (id == 0)
		{
			Printf("'%s' is not a sound\n", argv[1]);
		}
		else
		{
			S_Sound (CHAN_AUTO | CHAN_UI, id, 1.f, ATTN_NONE);
		}
	}
}

//==========================================================================
//
// CCMD loopsound
//
//==========================================================================

CCMD (loopsound)
{
	// [BB] !netgame -> ( NETWORK_GetState( ) != NETSTATE_CLIENT )
	if (players[consoleplayer].mo != NULL && ( NETWORK_GetState( ) != NETSTATE_CLIENT ) && argv.argc() > 1)
	{
		FSoundID id = argv[1];
		if (id == 0)
		{
			Printf("'%s' is not a sound\n", argv[1]);
		}
		else
		{
			AActor *icon = Spawn("SpeakerIcon", players[consoleplayer].mo->x,
				players[consoleplayer].mo->y,
				players[consoleplayer].mo->z + 32*FRACUNIT,
				ALLOW_REPLACE);
			if (icon != NULL)
			{
				S_Sound(icon, CHAN_BODY | CHAN_LOOP, id, 1.f, ATTN_IDLE);
			}
		}
	}
}

//==========================================================================
//
// CCMD idmus
//
//==========================================================================

CCMD (idmus)
{
	level_info_t *info;
	FString map;
	int l;

	if (!nomusic)
	{
		if (argv.argc() > 1)
		{
			if (gameinfo.flags & GI_MAPxx)
			{
				l = atoi (argv[1]);
			if (l <= 99)
				{
					map = CalcMapName (0, l);
				}
				else
				{
					Printf ("%s\n", GStrings("STSTR_NOMUS"));
					return;
				}
			}
			else
			{
				map = CalcMapName (argv[1][0] - '0', argv[1][1] - '0');
			}

			if ( (info = FindLevelInfo (map)) )
			{
				if (info->Music.IsNotEmpty())
				{
					S_ChangeMusic (info->Music, info->musicorder);
					Printf ("%s\n", GStrings("STSTR_MUS"));
				}
			}
			else
			{
				Printf ("%s\n", GStrings("STSTR_NOMUS"));
			}
		}
	}
}

//==========================================================================
//
// CCMD changemus
//
//==========================================================================

CCMD (changemus)
{
	// [BB] The server is not playing music, but can instruct the clients to do so.
	if (!nomusic || ( NETWORK_GetState( ) == NETSTATE_SERVER ))
	{
		if (argv.argc() > 1)
		{
			if (PlayList)
			{
				delete PlayList;
				PlayList = NULL;
			}
		S_ChangeMusic (argv[1], argv.argc() > 2 ? atoi (argv[2]) : 0);

		// [BB] If we're the server, tell clients to change the music, and
		// save the current music setting for when new clients connect.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			int musicorder =  argv.argc() > 2 ? atoi (argv[2]) : 0;
			SERVERCOMMANDS_SetMapMusic( argv[1], musicorder );
			SERVER_SetMapMusic( argv[1], musicorder );
		}
		}
		else
		{
			const char *currentmus = mus_playing.name.GetChars();
			if(currentmus != NULL && *currentmus != 0)
			{
				Printf ("currently playing %s\n", currentmus);
			}
			else
			{
				Printf ("no music playing\n");
			}
		}
	}
}

//==========================================================================
//
// CCMD stopmus
//
//==========================================================================

CCMD (stopmus)
{
	if (PlayList)
	{
		delete PlayList;
		PlayList = NULL;
	}
	S_StopMusic (false);
	LastSong = "";	// forget the last played song so that it won't get restarted if some volume changes occur
}

//==========================================================================
//
// CCMD cd_play
//
// Plays a specified track, or the entire CD if no track is specified.
//==========================================================================

CCMD (cd_play)
{
	char musname[16];

	// [BC] Server doesn't use music/sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	if (argv.argc() == 1)
	{
		strcpy (musname, ",CD,");
	}
	else
	{
		mysnprintf (musname, countof(musname), ",CD,%d", atoi(argv[1]));
	}
	S_ChangeMusic (musname, 0, true);
}

//==========================================================================
//
// CCMD cd_stop
//
//==========================================================================

CCMD (cd_stop)
{
	// [BC] Server doesn't use music/sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	CD_Stop ();
}

//==========================================================================
//
// CCMD cd_eject
//
//==========================================================================

CCMD (cd_eject)
{
	// [BC] Server doesn't use music/sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	CD_Eject ();
}

//==========================================================================
//
// CCMD cd_close
//
//==========================================================================

CCMD (cd_close)
{
	// [BC] Server doesn't use music/sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	CD_UnEject ();
}

//==========================================================================
//
// CCMD cd_pause
//
//==========================================================================

CCMD (cd_pause)
{
	// [BC] Server doesn't use music/sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	CD_Pause ();
}

//==========================================================================
//
// CCMD cd_resume
//
//==========================================================================

CCMD (cd_resume)
{
	// [BC] Server doesn't use music/sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	CD_Resume ();
}

//==========================================================================
//
// CCMD playlist
//
//==========================================================================

CCMD (playlist)
{
	int argc = argv.argc();

	// [BC] Server doesn't use music/sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	if (argc < 2 || argc > 3)
	{
		Printf ("playlist <playlist.m3u> [<position>|shuffle]\n");
	}
	else
	{
		if (PlayList != NULL)
		{
			PlayList->ChangeList (argv[1]);
		}
		else
		{
			PlayList = new FPlayList (argv[1]);
		}
		if (PlayList->GetNumSongs () == 0)
		{
			delete PlayList;
			PlayList = NULL;
		}
		else
		{
			if (argc == 3)
			{
				if (stricmp (argv[2], "shuffle") == 0)
				{
					PlayList->Shuffle ();
				}
				else
				{
					PlayList->SetPosition (atoi (argv[2]));
				}
			}
			S_ActivatePlayList (false);
		}
	}
}

//==========================================================================
//
// CCMD playlistpos
//
//==========================================================================

static bool CheckForPlaylist ()
{
	if (PlayList == NULL)
	{
		Printf ("No playlist is playing.\n");
		return false;
	}
	return true;
}

CCMD (playlistpos)
{
	if (CheckForPlaylist() && argv.argc() > 1)
	{
		PlayList->SetPosition (atoi (argv[1]) - 1);
		S_ActivatePlayList (false);
	}
}

//==========================================================================
//
// CCMD playlistnext
//
//==========================================================================

CCMD (playlistnext)
{
	if (CheckForPlaylist())
	{
		PlayList->Advance ();
		S_ActivatePlayList (false);
	}
}

//==========================================================================
//
// CCMD playlistprev
//
//==========================================================================

CCMD (playlistprev)
{
	if (CheckForPlaylist())
	{
		PlayList->Backup ();
		S_ActivatePlayList (true);
	}
}

//==========================================================================
//
// CCMD playliststatus
//
//==========================================================================

CCMD (playliststatus)
{
	if (CheckForPlaylist ())
	{
		Printf ("Song %d of %d:\n%s\n",
			PlayList->GetPosition () + 1,
			PlayList->GetNumSongs (),
			PlayList->GetSong (PlayList->GetPosition ()));
	}
}

//==========================================================================
//
// CCMD cachesound <sound name>
//
//==========================================================================

CCMD (cachesound)
{
	if (argv.argc() < 2)
	{
		Printf ("Usage: cachesound <sound> ...\n");
		return;
	}
	for (int i = 1; i < argv.argc(); ++i)
	{
		FSoundID sfxnum = argv[i];
		if (sfxnum != 0)
		{
			S_CacheSound (&S_sfx[sfxnum]);
		}
	}
}
