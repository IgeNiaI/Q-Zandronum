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
//		DOOM main program (D_DoomMain) and game loop (D_DoomLoop),
//		plus functions to determine game mode (shareware, registered),
//		parse command line parameters, configure game parameters (turbo),
//		and call the startup functions.
//
//-----------------------------------------------------------------------------

// HEADER FILES ------------------------------------------------------------

#ifdef _WIN32
#ifdef unix
#undef unix
#endif
#include <direct.h>
#define mkdir(a,b) _mkdir (a)
#else
#include <sys/stat.h>
#endif

#ifdef HAVE_FPU_CONTROL
#include <fpu_control.h>
#endif
#include <float.h>

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#endif

#include <time.h>
#include <math.h>
#include <assert.h>
#include <sys/stat.h>

// [BB] network.h has to be included before stats.h under Linux.
// The reason should be investigated.
#include "network.h"

#include "doomerrors.h"

#include "d_gui.h"
#include "m_random.h"
#include "doomdef.h"
#include "doomstat.h"
#include "gstrings.h"
#include "w_wad.h"
#include "s_sound.h"
#include "v_video.h"
#include "intermission/intermission.h"
#include "f_wipe.h"
#include "m_argv.h"
#include "m_misc.h"
#include "menu/menu.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "i_system.h"
#include "i_sound.h"
#include "i_video.h"
#include "g_game.h"
#include "hu_stuff.h"
#include "wi_stuff.h"
#include "st_stuff.h"
#include "am_map.h"
#include "p_setup.h"
#include "r_utility.h"
#include "r_sky.h"
#include "d_main.h"
#include "d_dehacked.h"
#include "cmdlib.h"
#include "s_sound.h"
#include "m_swap.h"
#include "v_text.h"
#include "gi.h"
#include "stats.h"
#include "gameconfigfile.h"
#include "sbar.h"
#include "decallib.h"
#include "version.h"
#include "v_text.h"
// [BC] New #includes.
#include "announcer.h"
#include "chat.h"
#include "cooperative.h"
#include "deathmatch.h"
#include "duel.h"
#include "scoreboard.h"
#include "team.h"
#include "medal.h"
#include "cl_commands.h"
#include "cl_main.h"
#include "cl_statistics.h"
#include "maprotation.h"
#include "browser.h"
#include "p_spec.h"
#include "joinqueue.h"
#include "lastmanstanding.h"
#include "campaign.h"
#include "sv_save.h"
#include "callvote.h"
#include "invasion.h"
#include "survival.h"
#include "possession.h"
#include "cl_demo.h"
#include "gamemode.h"
#include "sectinfo.h"
#include "md5.h"
#include "za_database.h"

#include "st_start.h"
#include "templates.h"
#include "teaminfo.h"
#include "hardware.h"
#include "sbarinfo.h"
#include "d_net.h"
#include "g_level.h"
#include "d_event.h"
#include "d_netinf.h"
#include "v_palette.h"
#include "m_cheat.h"
#include "compatibility.h"
#include "m_joy.h"
#include "sc_man.h"
#include "po_man.h"
#include "resourcefiles/resourcefile.h"
#include "r_renderer.h"
#include "p_local.h"

#ifdef USE_POLYMOST
#include "r_polymost.h"
#endif


// [ZZ] PWO header file
#include "g_shared/pwo.h"

#include "win32/g15/g15.h"
EXTERN_CVAR(Bool, hud_althud)
void DrawHUD();

extern player_t *Player;

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

extern void ReadStatistics();
extern void M_RestoreMode ();
extern void M_SetDefaultMode ();
extern void R_ExecuteSetViewSize ();
extern void G_NewInit ();
extern void SetupPlayerClasses ();
extern void HUD_InitHud();
const FIWADInfo *D_FindIWAD(TArray<FString> &wadfiles, const char *iwad, const char *basewad);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void D_CheckNetGame ();
void D_ProcessEvents ();
void G_BuildTiccmd (ticcmd_t* cmd);
void D_DoAdvanceDemo ();
void D_AddWildFile (TArray<FString> &wadfiles, const char *pattern);
void D_LoadWadSettings ();

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

void D_DoomLoop ();
static const char *BaseFileSearch (const char *file, const char *ext, bool lookfirstinprogdir=false);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

EXTERN_CVAR (Float, turbo)
EXTERN_CVAR (Bool, freelook)
EXTERN_CVAR (Float, m_pitch)
EXTERN_CVAR (Float, m_yaw)
EXTERN_CVAR (Bool, invertmouse)
EXTERN_CVAR (Bool, lookstrafe)
EXTERN_CVAR (Int, screenblocks)
EXTERN_CVAR (Bool, sv_cheats)
EXTERN_CVAR (Bool, sv_unlimited_pickup)

extern int testingmode;
extern bool setmodeneeded;
extern int NewWidth, NewHeight, NewBits, DisplayBits;
EXTERN_CVAR (Bool, st_scale)
extern bool gameisdead;
extern bool demorecording;
extern bool M_DemoNoPlay;	// [RH] if true, then skip any demos in the loop
extern bool insave;


// PUBLIC DATA DEFINITIONS -------------------------------------------------

// [BC] fraglimit/timelimit have been moved to a more appropriate location.

#ifdef USE_POLYMOST
CVAR(Bool, testpolymost, false, 0)
#endif
CVAR (Int, wipetype, 1, CVAR_ARCHIVE);
CVAR (Int, snd_drawoutput, 0, 0);
CUSTOM_CVAR (String, vid_cursor, "None", CVAR_ARCHIVE | CVAR_NOINITCALL)
{
	// [TP/BB] The server does not use cursors.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	bool res = false;

	if (!stricmp(self, "None" ) && gameinfo.CursorPic.IsNotEmpty())
	{
		res = I_SetCursor(TexMan[gameinfo.CursorPic]);
	}
	else
	{
		res = I_SetCursor(TexMan[self]);
	}
	if (!res)
	{
		I_SetCursor(TexMan["cursor"]);
	}
}

bool DrawFSHUD;				// [RH] Draw fullscreen HUD?
TArray<FString> allwads;
// [BB]
TArray<FString> autoloadedwads;
TArray<FString> optionalwads; // [TP] Files loaded with -optfile
bool devparm;				// started game with -devparm
const char *D_DrawIcon;	// [RH] Patch name of icon to draw on next refresh
int NoWipe;				// [RH] Allow wipe? (Needs to be set each time)
bool singletics = false;	// debug flag to cancel adaptiveness
FString startmap;
bool autostart;
FString StoredWarp;
bool advancedemo;
FILE *debugfile;
event_t events[MAXEVENTS];
int eventhead;
int eventtail;
gamestate_t wipegamestate = GS_DEMOSCREEN;	// can be -1 to force a wipe
bool PageBlank;
FTexture *Page;
FTexture *Advisory;
bool nospriterename;
FStartupInfo DoomStartupInfo;
FString lastIWAD;
int restart = 0;

cycle_t FrameCycles;


// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int demosequence;
static int pagetic;

// CODE --------------------------------------------------------------------

//==========================================================================
//
// D_ProcessEvents
//
// Send all the events of the given timestamp down the responder chain.
// Events are asynchronous inputs generally generated by the game user.
// Events can be discarded if no responder claims them
//
//==========================================================================

void D_ProcessEvents (void)
{
	event_t *ev;
		
	// [RH] If testing mode, do not accept input until test is over
	if (testingmode)
	{
		if (testingmode == 1)
		{
			M_SetDefaultMode ();
		}
		else if (testingmode <= I_GetTime(false))
		{
			M_RestoreMode ();
		}
		return;
	}

	for (; eventtail != eventhead ; eventtail = (eventtail+1)&(MAXEVENTS-1))
	{
		ev = &events[eventtail];
		if (ev->type == EV_None)
			continue;
		if (ev->type == EV_DeviceChange)
			UpdateJoystickMenu(I_UpdateDeviceList());
		if (C_Responder (ev))
			continue;				// console ate the event
		if (M_Responder (ev))
			continue;				// menu ate the event
		#ifdef USE_POLYMOST
			if (testpolymost)
				Polymost_Responder (ev);
		#endif
		G_Responder (ev);
	}
}

//==========================================================================
//
// D_PostEvent
//
// Called by the I/O functions when input is detected.
//
//==========================================================================

void D_PostEvent (const event_t *ev)
{
	// Do not post duplicate consecutive EV_DeviceChange events.
	if (ev->type == EV_DeviceChange && events[eventhead].type == EV_DeviceChange)
	{
		return;
	}
	events[eventhead] = *ev;
	if (ev->type == EV_Mouse && !paused && menuactive == MENU_Off && ConsoleState != c_down && ConsoleState != c_falling
#ifdef USE_POLYMOST
		&& !testpolymost		
#endif
		)
	{
		if (Button_Mlook.bDown || freelook)
		{
			int look = int(ev->y * m_pitch * mouse_sensitivity * 16.0);
			if (invertmouse)
				look = -look;
			G_AddViewPitch (look);
			events[eventhead].y = 0;
		}
		if (!Button_Strafe.bDown && !lookstrafe)
		{
			G_AddViewAngle (int(ev->x * m_yaw * mouse_sensitivity * 8.0));
			events[eventhead].x = 0;
		}
		if ((events[eventhead].x | events[eventhead].y) == 0)
		{
			return;
		}
	}
	eventhead = (eventhead+1)&(MAXEVENTS-1);
}

//==========================================================================
//
// D_RemoveNextCharEvent
//
// Removes the next EV_GUI_Char event in the input queue. Used by the menu,
// since it (generally) consumes EV_GUI_KeyDown events and not EV_GUI_Char
// events, and it needs to ensure that there is no left over input when it's
// done. If there are multiple EV_GUI_KeyDowns before the EV_GUI_Char, then
// there are dead chars involved, so those should be removed, too. We do
// this by changing the message type to EV_None rather than by actually
// removing the event from the queue.
// 
//==========================================================================

void D_RemoveNextCharEvent()
{
	assert(events[eventtail].type == EV_GUI_Event && events[eventtail].subtype == EV_GUI_KeyDown);
	for (int evnum = eventtail; evnum != eventhead; evnum = (evnum+1) & (MAXEVENTS-1))
	{
		event_t *ev = &events[evnum];
		if (ev->type != EV_GUI_Event)
			break;
		if (ev->subtype == EV_GUI_KeyDown || ev->subtype == EV_GUI_Char)
		{
			ev->type = EV_None;
			if (ev->subtype == EV_GUI_Char)
				break;
		}
		else
		{
			break;
		}
	}
}

//==========================================================================
//
// CVAR dmflags
//
//==========================================================================

CUSTOM_CVAR (Int, dmflags, 0, CVAR_SERVERINFO | CVAR_CAMPAIGNLOCK)
{
	// In case DF_NO_FREELOOK was changed, reinitialize the sky
	// map. (If no freelook, then no need to stretch the sky.)
	if (sky1texture.isValid())
		R_InitSkyMap ();

	if (self & DF_NO_FREELOOK)
	{
		// [BC] Only write this byte if we're recording a demo. Otherwise, just do it!
		if ( demorecording )
			Net_WriteByte (DEM_CENTERVIEW);
		else
		{
			if ( players[consoleplayer].mo )
				players[consoleplayer].mo->pitch = 0;
		}
	}
	// If nofov is set, force everybody to the arbitrator's FOV.
	if ((self & DF_NO_FOV) && consoleplayer == Net_Arbitrator)
	{
		BYTE fov;

		Net_WriteByte (DEM_FOV);

		// If the game is started with DF_NO_FOV set, the arbitrator's
		// DesiredFOV will not be set when this callback is run, so
		// be sure not to transmit a 0 FOV.
		fov = (BYTE)players[consoleplayer].DesiredFOV;
		if (fov == 0)
		{
			fov = 90;
		}
		Net_WriteByte (fov);
	}

	// [BC] If we're the server, tell clients that the dmflags changed.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( gamestate != GS_STARTUP ))
	{
		SERVER_Printf( "%s changed to: %d\n", self.GetName( ), (int)self );
		SERVERCOMMANDS_SetGameDMFlags( );
	}
}

CVAR (Flag, sv_nohealth,		dmflags, DF_NO_HEALTH);
CVAR (Flag, sv_noitems,			dmflags, DF_NO_ITEMS);
CVAR (Flag, sv_weaponstay,		dmflags, DF_WEAPONS_STAY);
CVAR (Flag, sv_falldamage,		dmflags, DF_FORCE_FALLINGHX);
CVAR (Flag, sv_oldfalldamage,	dmflags, DF_FORCE_FALLINGZD);
CVAR (Flag, sv_samelevel,		dmflags, DF_SAME_LEVEL);
CVAR (Flag, sv_spawnfarthest,	dmflags, DF_SPAWN_FARTHEST);
CVAR (Flag, sv_forcerespawn,	dmflags, DF_FORCE_RESPAWN);
CVAR (Flag, sv_noarmor,			dmflags, DF_NO_ARMOR);
CVAR (Flag, sv_noexit,			dmflags, DF_NO_EXIT);
CVAR (Flag, sv_infiniteammo,	dmflags, DF_INFINITE_AMMO);
CVAR (Flag, sv_nomonsters,		dmflags, DF_NO_MONSTERS);
CVAR (Flag, sv_monsterrespawn,	dmflags, DF_MONSTERS_RESPAWN);
CVAR (Flag, sv_itemrespawn,		dmflags, DF_ITEMS_RESPAWN);
CVAR (Flag, sv_fastmonsters,	dmflags, DF_FAST_MONSTERS);
CVAR (Flag, sv_nojump,			dmflags, DF_NO_JUMP);
CVAR (Flag, sv_allowjump,		dmflags, DF_YES_JUMP);
CVAR (Flag, sv_nofreelook,		dmflags, DF_NO_FREELOOK);
CVAR (Flag, sv_respawnsuper,	dmflags, DF_RESPAWN_SUPER);
CVAR (Flag, sv_nofov,			dmflags, DF_NO_FOV);
CVAR (Flag, sv_noweaponspawn,	dmflags, DF_NO_COOP_WEAPON_SPAWN);
CVAR (Flag, sv_nocrouch,		dmflags, DF_NO_CROUCH);
CVAR (Flag, sv_allowcrouch,		dmflags, DF_YES_CROUCH);
// [BB] New CVARs for Skulltag's flags.
CVAR (Flag, sv_coop_loseinventory,	dmflags, DF_COOP_LOSE_INVENTORY);
CVAR (Flag, sv_coop_losekeys,		dmflags, DF_COOP_LOSE_KEYS);
CVAR (Flag, sv_coop_loseweapons,	dmflags, DF_COOP_LOSE_WEAPONS);
CVAR (Flag, sv_coop_losearmor,		dmflags, DF_COOP_LOSE_ARMOR);
CVAR (Flag, sv_coop_losepowerups,	dmflags, DF_COOP_LOSE_POWERUPS);
CVAR (Flag, sv_coop_loseammo,		dmflags, DF_COOP_LOSE_AMMO);
CVAR (Flag, sv_coop_halveammo,		dmflags, DF_COOP_HALVE_AMMO);
// Some (hopefully cleaner) interface to these settings.
CVAR (Mask, sv_crouch,			dmflags, DF_NO_CROUCH|DF_YES_CROUCH);
CVAR (Mask, sv_jump,			dmflags, DF_NO_JUMP|DF_YES_JUMP);
CVAR (Mask, sv_fallingdamage,	dmflags, DF_FORCE_FALLINGHX|DF_FORCE_FALLINGZD);

//==========================================================================
//
// CVAR dmflags2
//
// [RH] From Skull Tag. Some of these were already done as separate cvars
// (such as bfgaiming), but I collected them here like Skull Tag does.
//
//==========================================================================

CUSTOM_CVAR (Int, dmflags2, 0, CVAR_SERVERINFO | CVAR_CAMPAIGNLOCK)
{
	// [BC] If we're the server, tell clients that the dmflags changed.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( gamestate != GS_STARTUP ))
	{
		SERVER_Printf( "%s changed to: %d\n", self.GetName( ), (int)self );
		SERVERCOMMANDS_SetGameDMFlags( );
	}

	// Stop the automap if we aren't allowed to use it.
	if ((self & DF2_NO_AUTOMAP) && automapactive)
		AM_Stop ();

	for (int i = 0; i < MAXPLAYERS; i++)
	{
		player_t *p = &players[i];

		if (!playeringame[i])
			continue;

		// Revert our view to our own eyes if spying someone else.
		if (self & DF2_DISALLOW_SPYING)
		{
			// The player isn't looking through its own eyes, so make it.
			if (p->camera != p->mo)
			{
				p->camera = p->mo;

				S_UpdateSounds (p->camera);
				// [BB] The server doesn't have a status bar.
				if ( StatusBar != NULL )
				{
					StatusBar->AttachToPlayer (p);

					if (demoplayback || (NETWORK_GetState( ) != NETSTATE_SINGLE) )
						StatusBar->ShowPlayerName ();
				}
			}
		}

		// Come out of chasecam mode if we're not allowed to use chasecam.
		if (!(dmflags2 & DF2_CHASECAM) && !G_SkillProperty (SKILLP_DisableCheats) && !sv_cheats)
		{
			// Take us out of chasecam mode only.
			if (p->cheats & CF_CHASECAM)
				cht_DoCheat (p, CHT_CHASECAM);
		}
	}
}

CVAR (Flag, sv_weapondrop,			dmflags2, DF2_YES_WEAPONDROP);
CVAR (Flag, sv_noteamswitch,		dmflags2, DF2_NO_TEAM_SWITCH);
CVAR (Flag, sv_doubleammo,			dmflags2, DF2_YES_DOUBLEAMMO);
CVAR (Flag, sv_degeneration,		dmflags2, DF2_YES_DEGENERATION);
CVAR (Flag, sv_bfgfreeaim,			dmflags2, DF2_YES_FREEAIMBFG);
CVAR (Flag, sv_barrelrespawn,		dmflags2, DF2_BARRELS_RESPAWN);
CVAR (Flag, sv_keepfrags,			dmflags2, DF2_YES_KEEPFRAGS);
CVAR (Flag, sv_norespawn,			dmflags2, DF2_NO_RESPAWN);
CVAR (Flag, sv_losefrag,			dmflags2, DF2_YES_LOSEFRAG);
CVAR (Flag, sv_norespawninvul,		dmflags2, DF2_NO_RESPAWN_INVUL);
CVAR (Flag, sv_samespawnspot,		dmflags2, DF2_SAME_SPAWN_SPOT);
CVAR (Flag, sv_infiniteinventory,	dmflags2, DF2_INFINITE_INVENTORY);
CVAR (Flag, sv_killallmonsters,		dmflags2, DF2_KILL_MONSTERS);
CVAR (Flag, sv_noautomap,			dmflags2, DF2_NO_AUTOMAP);
CVAR (Flag, sv_noautomapallies,		dmflags2, DF2_NO_AUTOMAP_ALLIES);
CVAR (Flag, sv_disallowspying,		dmflags2, DF2_DISALLOW_SPYING);
CVAR (Flag, sv_chasecam,			dmflags2, DF2_CHASECAM);
CVAR (Flag, sv_disallowsuicide,		dmflags2, DF2_NOSUICIDE);
CVAR (Flag, sv_noautoaim,			dmflags2, DF2_NOAUTOAIM);
CVAR (Flag, sv_dontcheckammo,		dmflags2, DF2_DONTCHECKAMMO);
CVAR (Flag, sv_killbossmonst,		dmflags2, DF2_KILLBOSSMONST);
CVAR (Flag, sv_nocountendmonst,		dmflags2, DF2_NOCOUNTENDMONST);
CVAR (Flag, sv_norunes,				dmflags2, DF2_NO_RUNES);
CVAR (Flag, sv_instantreturn,		dmflags2, DF2_INSTANT_RETURN);
CVAR (Flag, sv_noteamselect,		dmflags2, DF2_NO_TEAM_SELECT);
CVAR (Flag, sv_shotgunstart,		dmflags2, DF2_COOP_SHOTGUNSTART);

//==========================================================================
//
// [BB] CVAR zadmflags
//
//==========================================================================

// [BB/EP] Only necessary to handle ZADF_FORCE_GL_DEFAULTS.
#ifndef NO_GL
EXTERN_CVAR(Int, gl_lightmode)
EXTERN_CVAR(Int, gl_distfog)
#endif

CUSTOM_CVAR (Int, zadmflags, 0, CVAR_SERVERINFO)
{
	// [Dusk] If we just turned sv_sharedkeys on, share keys now.
	if ((( self ^ self.GetPastValue() ) & ZADF_SHARE_KEYS ) & ( self & ZADF_SHARE_KEYS ))
		SERVER_SyncSharedKeys( MAXPLAYERS, true );

	// [BB] If we're the server, tell clients that the dmflags changed.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( gamestate != GS_STARTUP ))
	{
		SERVER_Printf( "%s changed to: %d\n", self.GetName( ), (int)self );
		SERVERCOMMANDS_SetGameDMFlags( );
	}

#ifndef NO_GL
	// [BB/EP] This makes gl_lightmode and gl_distfog handle ZADF_FORCE_GL_DEFAULTS.
	// [BB] Don't do this on startup since gl.flags is not properly initialized yet.
	if ( gamestate != GS_STARTUP )
	{
		gl_lightmode.Callback();
		gl_distfog.Callback();
	}
#endif
}

CVAR (Flag, sv_noidentifytarget,			zadmflags, ZADF_NO_IDENTIFY_TARGET);
CVAR (Flag, sv_applylmsspectatorsettings,	zadmflags, ZADF_ALWAYS_APPLY_LMS_SPECTATORSETTINGS);
CVAR (Flag, sv_nocoopinfo,			zadmflags, ZADF_NO_COOP_INFO);
CVAR (Flag, sv_unblockplayers,			zadmflags, ZADF_UNBLOCK_PLAYERS);
CVAR (Flag, sv_nomedals,			zadmflags, ZADF_NO_MEDALS);
CVAR (Flag, sv_sharekeys,			zadmflags, ZADF_SHARE_KEYS);
CVAR (Flag, sv_keepteams,			zadmflags, ZADF_YES_KEEP_TEAMS);
CVAR (Flag, sv_forcegldefaults,		zadmflags, ZADF_FORCE_GL_DEFAULTS);
CVAR (Flag, sv_norocketjumping,		zadmflags, ZADF_NO_ROCKET_JUMPING);
CVAR (Flag, sv_awarddamageinsteadkills,		zadmflags, ZADF_AWARD_DAMAGE_INSTEAD_KILLS);
CVAR (Flag, sv_forcealpha,		zadmflags, ZADF_FORCE_ALPHA);
CVAR (Flag, sv_coop_spactorspawn,	zadmflags, ZADF_COOP_SP_ACTOR_SPAWN);
CVAR (Flag, sv_maxbloodscalar,		zadmflags, ZADF_MAX_BLOOD_SCALAR);
CVAR (Flag, sv_unblockallies,		zadmflags, ZADF_UNBLOCK_ALLIES);
CVAR (Flag, sv_nodrop,				zadmflags, ZADF_NODROP);
CVAR (Flag, sv_survival_nomapresetondeath, zadmflags, ZADF_SURVIVAL_NO_MAP_RESET_ON_DEATH);
CVAR (Flag, sv_deadplayerscankeepinventory, zadmflags, ZADF_DEAD_PLAYERS_CAN_KEEP_INVENTORY);

//==========================================================================
//
// CVAR compatflags
//
//==========================================================================

int i_compatflags, i_compatflags2;	// internal compatflags composed from the compatflags CVAR and MAPINFO settings
int ii_compatflags, ii_compatflags2, ib_compatflags;

EXTERN_CVAR(Int, compatmode)

static int GetCompatibility(int mask)
{
	if (level.info == NULL) return mask;
	else return (mask & ~level.info->compatmask) | (level.info->compatflags & level.info->compatmask);
}

static int GetCompatibility2(int mask)
{
	return (level.info == NULL) ? mask
		: (mask & ~level.info->compatmask2) | (level.info->compatflags2 & level.info->compatmask2);
}

// [BB] Removed the CVAR_ARCHIVE flag.
CUSTOM_CVAR (Int, compatflags, 0, CVAR_SERVERINFO)
{
	int old = i_compatflags;
	i_compatflags = GetCompatibility(self) | ii_compatflags;
	if ((old ^ i_compatflags) & COMPATF_POLYOBJ)
	{
		FPolyObj::ClearAllSubsectorLinks();
	}

	// [BC] If we're the server, tell clients that the dmflags changed.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( gamestate != GS_STARTUP ))
	{
		SERVER_Printf( "%s changed to: %d\n", self.GetName( ), (int)self );
		SERVERCOMMANDS_SetGameDMFlags( );
	}
}

// [BB] Removed the CVAR_ARCHIVE flag.
CUSTOM_CVAR (Int, compatflags2, 0, CVAR_SERVERINFO)
{
	i_compatflags2 = GetCompatibility2(self) | ii_compatflags2;

	// [BB] If we're the server, tell clients that compatflags2 changed.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( gamestate != GS_STARTUP ))
	{
		SERVER_Printf( "%s changed to: %d\n", self.GetName( ), (int)self );
		SERVERCOMMANDS_SetGameDMFlags( );
	}
}

//==========================================================================
//
// [BB] CVAR zacompatflags
//
//==========================================================================

CUSTOM_CVAR (Int, zacompatflags, 0, CVAR_SERVERINFO)
{
	// [BC] If we're the server, tell clients that the dmflags changed.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( gamestate != GS_STARTUP ))
	{
		SERVER_Printf( "%s changed to: %d\n", self.GetName( ), (int)self );
		SERVERCOMMANDS_SetGameDMFlags( );
	}
}

CUSTOM_CVAR(Int, compatmode, 0, CVAR_ARCHIVE|CVAR_NOINITCALL)
{
	int v, w = 0;

	switch (self)
	{
	default:
	case 0:
		v = 0;
		break;

	case 1:	// Doom2.exe compatible with a few relaxed settings
		v = COMPATF_SHORTTEX|COMPATF_STAIRINDEX|COMPATF_USEBLOCKING|COMPATF_NODOORLIGHT|COMPATF_SPRITESORT|
			COMPATF_TRACE|COMPATF_MISSILECLIP|COMPATF_SOUNDTARGET|COMPATF_DEHHEALTH|COMPATF_CROSSDROPOFF|
			COMPATF_LIGHT;
		w= COMPATF2_FLOORMOVE;
		break;

	case 2:	// same as 1 but stricter (NO_PASSMOBJ and INVISIBILITY are also set)
		v = COMPATF_SHORTTEX|COMPATF_STAIRINDEX|COMPATF_USEBLOCKING|COMPATF_NODOORLIGHT|COMPATF_SPRITESORT|
			COMPATF_TRACE|COMPATF_MISSILECLIP|COMPATF_SOUNDTARGET|COMPATF_NO_PASSMOBJ|COMPATF_LIMITPAIN|
			COMPATF_DEHHEALTH|COMPATF_INVISIBILITY|COMPATF_CROSSDROPOFF|COMPATF_CORPSEGIBS|COMPATF_HITSCAN|
			COMPATF_WALLRUN|COMPATF_NOTOSSDROPS|COMPATF_LIGHT|COMPATF_MASKEDMIDTEX;
		w = COMPATF2_BADANGLES|COMPATF2_FLOORMOVE;
		break;

	case 3: // Boom compat mode
		v = COMPATF_TRACE|COMPATF_SOUNDTARGET|COMPATF_BOOMSCROLL|COMPATF_MISSILECLIP;
		break;

	case 4: // Old ZDoom compat mode
		v = COMPATF_SOUNDTARGET|COMPATF_LIGHT;
		// [BB] Out of order ZDoom backport.
		w = COMPATF2_PUSHWINDOW;
		break;

	case 5: // MBF compat mode
		v = COMPATF_TRACE|COMPATF_SOUNDTARGET|COMPATF_BOOMSCROLL|COMPATF_MISSILECLIP|COMPATF_MUSHROOM|
			COMPATF_MBFMONSTERMOVE|COMPATF_NOBLOCKFRIENDS;
		break;

	case 6:	// Boom with some added settings to reenable some 'broken' behavior
		v = COMPATF_TRACE|COMPATF_SOUNDTARGET|COMPATF_BOOMSCROLL|COMPATF_MISSILECLIP|COMPATF_NO_PASSMOBJ|
			COMPATF_INVISIBILITY|COMPATF_CORPSEGIBS|COMPATF_HITSCAN|COMPATF_WALLRUN|COMPATF_NOTOSSDROPS;
		break;

	}
	compatflags = v;
	compatflags2 = w;
}

CVAR (Flag, compat_shortTex,			compatflags,  COMPATF_SHORTTEX);
CVAR (Flag, compat_stairs,				compatflags,  COMPATF_STAIRINDEX);
CVAR (Flag, compat_limitpain,			compatflags,  COMPATF_LIMITPAIN);
CVAR (Flag, compat_silentpickup,		compatflags,  COMPATF_SILENTPICKUP);
CVAR (Flag, compat_nopassover,			compatflags,  COMPATF_NO_PASSMOBJ);
CVAR (Flag, compat_soundslots,			compatflags,  COMPATF_MAGICSILENCE);
CVAR (Flag, compat_wallrun,				compatflags,  COMPATF_WALLRUN);
CVAR (Flag, compat_notossdrops,			compatflags,  COMPATF_NOTOSSDROPS);
CVAR (Flag, compat_useblocking,			compatflags,  COMPATF_USEBLOCKING);
CVAR (Flag, compat_nodoorlight,			compatflags,  COMPATF_NODOORLIGHT);
CVAR (Flag, compat_ravenscroll,			compatflags,  COMPATF_RAVENSCROLL);
CVAR (Flag, compat_soundtarget,			compatflags,  COMPATF_SOUNDTARGET);
CVAR (Flag, compat_dehhealth,			compatflags,  COMPATF_DEHHEALTH);
CVAR (Flag, compat_trace,				compatflags,  COMPATF_TRACE);
CVAR (Flag, compat_dropoff,				compatflags,  COMPATF_DROPOFF);
CVAR (Flag, compat_boomscroll,			compatflags,  COMPATF_BOOMSCROLL);
CVAR (Flag, compat_invisibility,		compatflags,  COMPATF_INVISIBILITY);
CVAR (Flag, compat_silentinstantfloors,	compatflags,  COMPATF_SILENT_INSTANT_FLOORS);
CVAR (Flag, compat_sectorsounds,		compatflags,  COMPATF_SECTORSOUNDS);
CVAR (Flag, compat_missileclip,			compatflags,  COMPATF_MISSILECLIP);
CVAR (Flag, compat_crossdropoff,		compatflags,  COMPATF_CROSSDROPOFF);
CVAR (Flag, compat_anybossdeath,		compatflags,  COMPATF_ANYBOSSDEATH);
CVAR (Flag, compat_minotaur,			compatflags,  COMPATF_MINOTAUR);
CVAR (Flag, compat_mushroom,			compatflags,  COMPATF_MUSHROOM);
CVAR (Flag, compat_mbfmonstermove,		compatflags,  COMPATF_MBFMONSTERMOVE);
CVAR (Flag, compat_corpsegibs,			compatflags,  COMPATF_CORPSEGIBS);
CVAR (Flag, compat_noblockfriends,		compatflags,  COMPATF_NOBLOCKFRIENDS);
CVAR (Flag, compat_spritesort,			compatflags,  COMPATF_SPRITESORT);
CVAR (Flag, compat_hitscan,				compatflags,  COMPATF_HITSCAN);
CVAR (Flag, compat_light,				compatflags,  COMPATF_LIGHT);
CVAR (Flag, compat_polyobj,				compatflags,  COMPATF_POLYOBJ);
CVAR (Flag, compat_maskedmidtex,		compatflags,  COMPATF_MASKEDMIDTEX);
CVAR (Flag, compat_badangles,			compatflags2, COMPATF2_BADANGLES);
CVAR (Flag, compat_floormove,			compatflags2, COMPATF2_FLOORMOVE);
// [BB] Out of order ZDoom backport.
CVAR (Flag, compat_pushwindow,			compatflags2, COMPATF2_PUSHWINDOW);
// [BB] Skulltag compat flags.
CVAR (Flag, compat_limited_airmovement, zacompatflags, ZACOMPATF_LIMITED_AIRMOVEMENT);
CVAR (Flag, compat_plasmabump,	zacompatflags, ZACOMPATF_PLASMA_BUMP_BUG);
CVAR (Flag, compat_instantrespawn,	zacompatflags, ZACOMPATF_INSTANTRESPAWN);
CVAR (Flag, compat_disabletaunts,	zacompatflags, ZACOMPATF_DISABLETAUNTS);
CVAR (Flag, compat_originalsoundcurve,	zacompatflags, ZACOMPATF_ORIGINALSOUNDCURVE);
CVAR (Flag, compat_oldintermission,	zacompatflags, ZACOMPATF_OLDINTERMISSION);
CVAR (Flag, compat_disablestealthmonsters,	zacompatflags, ZACOMPATF_DISABLESTEALTHMONSTERS);
CVAR (Flag, compat_oldradiusdmg,	zacompatflags, ZACOMPATF_OLDRADIUSDMG);
CVAR (Flag, compat_nocrosshair,		zacompatflags, ZACOMPATF_NO_CROSSHAIR);
CVAR (Flag, compat_oldweaponswitch,		zacompatflags, ZACOMPATF_OLD_WEAPON_SWITCH);
CVAR (Flag, compat_netscriptsareclientside,		zacompatflags, ZACOMPATF_NETSCRIPTS_ARE_CLIENTSIDE);
CVAR (Flag, compat_clientssendfullbuttoninfo,		zacompatflags, ZACOMPATF_CLIENTS_SEND_FULL_BUTTON_INFO);
CVAR (Flag, compat_noland,		zacompatflags, ZACOMPATF_NO_LAND);
CVAR (Flag, compat_oldrandom,		zacompatflags, ZACOMPATF_OLD_RANDOM_GENERATOR);
CVAR (Flag, compat_nogravity_spheres,		zacompatflags, ZACOMPATF_NOGRAVITY_SPHERES);
CVAR (Flag, compat_dont_stop_player_scripts_on_disconnect,		zacompatflags, ZACOMPATF_DONT_STOP_PLAYER_SCRIPTS_ON_DISCONNECT);
CVAR (Flag, compat_explosionthrust,		zacompatflags, ZACOMPATF_OLD_EXPLOSION_THRUST);
CVAR (Flag, compat_bridgedrops,		zacompatflags, ZACOMPATF_OLD_BRIDGE_DROPS);
CVAR (Flag, compat_oldzdoomzmovement, zacompatflags, ZACOMPATF_OLD_ZDOOM_ZMOVEMENT);
CVAR (Flag, compat_fullweaponlower,		zacompatflags, ZACOMPATF_FULL_WEAPON_LOWER);
CVAR (Flag, compat_autoaim,		zacompatflags, ZACOMPATF_AUTOAIM);
CVAR (Flag, compat_silentwestspawns,	zacompatflags, ZACOMPATF_SILENT_WEST_SPAWNS);
CVAR (Flag, compat_skulltagjumping,	zacompatflags, ZACOMPATF_SKULLTAG_JUMPING);

//==========================================================================
//
// D_Display
//
// Draw current display, possibly wiping it from the previous
//
//==========================================================================

void D_Display ()
{
	bool wipe;
	bool hw2d;

	// [BC] No need for servers to do this.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	if (nodrawers || screen == NULL)
		return; 				// for comparative timing / profiling
	
	cycle_t cycles;
	
	cycles.Reset();
	cycles.Clock();

	if (players[consoleplayer].camera == NULL)
	{
		players[consoleplayer].camera = players[consoleplayer].mo;
	}

	if (viewactive)
	{
		R_SetFOV (players[consoleplayer].camera && players[consoleplayer].camera->player ?
			players[consoleplayer].camera->player->FOV : 90.f);
	}

	// [RH] change the screen mode if needed
	if (setmodeneeded)
	{
		// Change screen mode.
		if (Video->SetResolution (NewWidth, NewHeight, NewBits))
		{
			// Recalculate various view parameters.
			setsizeneeded = true;
			// Let the status bar know the screen size changed
			if (StatusBar != NULL)
			{
				StatusBar->ScreenSizeChanged ();
			}
			// Refresh the console.
			C_NewModeAdjust ();
			// Reload crosshair if transitioned to a different size
			ST_LoadCrosshair (true);
			AM_NewResolution ();
			// Reset the mouse cursor in case the bit depth changed
			vid_cursor.Callback();
		}
	}

	RenderTarget = screen;

	// change the view size if needed
	if (setsizeneeded && StatusBar != NULL)
	{
		R_ExecuteSetViewSize ();
	}
	setmodeneeded = false;

	if (screen->Lock (false))
	{
		ST_SetNeedRefresh();
		V_SetBorderNeedRefresh();
	}

	// [RH] Allow temporarily disabling wipes
	// [BB] Wipes cause more harm than good on the client. Disable them for now.
	// [Leo] Disable them while playing demos too.
	if ( NoWipe || NETWORK_InClientMode() ) 
	{
		V_SetBorderNeedRefresh();
		NoWipe--;
		wipe = false;
		wipegamestate = gamestate;
	}
	else if (gamestate != wipegamestate && gamestate != GS_FULLCONSOLE && gamestate != GS_TITLELEVEL)
	{ // save the current screen if about to wipe
		V_SetBorderNeedRefresh();
		switch (wipegamestate)
		{
		default:
			wipe = screen->WipeStartScreen (wipetype);
			break;

		case GS_FORCEWIPEFADE:
			wipe = screen->WipeStartScreen (wipe_Fade);
			break;

		case GS_FORCEWIPEBURN:
			wipe = screen->WipeStartScreen (wipe_Burn);
			break;

		case GS_FORCEWIPEMELT:
			wipe = screen->WipeStartScreen (wipe_Melt);
			break;
		}
		wipegamestate = gamestate;
	}
	else
	{
		wipe = false;
	}

	hw2d = false;

#ifdef USE_POLYMOST
	if (testpolymost)
	{
		drawpolymosttest();
		C_DrawConsole(hw2d);
		M_Drawer();
	}
	else
#endif
	{
		unsigned int nowtime = I_FPSTime();
		TexMan.UpdateAnimations(nowtime);
		R_UpdateSky(nowtime);
		switch (gamestate)
		{
		case GS_FULLCONSOLE:
// [BB] Added label
drawfullconsole:
			screen->SetBlendingRect(0,0,0,0);
			hw2d = screen->Begin2D(false);
			C_DrawConsole (false);
			M_Drawer ();
			screen->Update ();
			return;

		case GS_LEVEL:
		case GS_TITLELEVEL:
			if (!gametic)
				break;

			// [Leo] Don't do that while requesting/receiving a snapshot to prevent potential HOMs.
			if ( NETWORK_InClientMode() && ( CLIENT_GetConnectionState( ) != CTS_ACTIVE ) )
			{
				// [BB] Keep drawing the console.
				goto drawfullconsole;
			}

			// [BB/EP] if (automapactive || viewactive) is necessary here. Otherwise it could try to render a NULL actor.
			// This happens for example if you start a new game, while being on a server.
			if (automapactive || viewactive)
			{
				if (StatusBar != NULL)
				{
					float blend[4] = { 0, 0, 0, 0 };
					StatusBar->BlendView (blend);
				}
				screen->SetBlendingRect(viewwindowx, viewwindowy,
					viewwindowx + viewwidth, viewwindowy + viewheight);
				// [BB] Zandronum handles prediction differently.
				//P_PredictPlayer(&players[consoleplayer]);
				// [BB] This check shouldn't be necessary, but should completely prevent
				// the "tried to render NULL actor" errors.
				if ( (players[consoleplayer].mo != NULL) && (players[consoleplayer].camera != NULL) )
					Renderer->RenderView(&players[consoleplayer]);
				// [BB] Zandronum handles prediction differently.
				//P_UnPredictPlayer();
			}

			if ((hw2d = screen->Begin2D(viewactive)))
			{
				// Redraw everything every frame when using 2D accel
				ST_SetNeedRefresh();
				V_SetBorderNeedRefresh();
			}
			Renderer->DrawRemainingPlayerSprites();
			screen->DrawBlendingRect();
			if (automapactive)
			{
				int saved_ST_Y = ST_Y;
				if (hud_althud && viewheight == SCREENHEIGHT)
				{
					ST_Y = viewheight;
				}
				AM_Drawer ();
				ST_Y = saved_ST_Y;
			}
			if (!automapactive || viewactive)
			{
				V_RefreshViewBorder ();
			}

			if (hud_althud && viewheight == SCREENHEIGHT && screenblocks > 10)
			{
				StatusBar->DrawBottomStuff (HUD_AltHud);
				if (DrawFSHUD || automapactive) DrawHUD();
				StatusBar->Draw (HUD_AltHud);
				StatusBar->DrawTopStuff (HUD_AltHud);
			}
			else 
			if (viewheight == SCREENHEIGHT && viewactive && screenblocks > 10)
			{
				EHudState state = DrawFSHUD ? HUD_Fullscreen : HUD_None;
				StatusBar->DrawBottomStuff (state);
				StatusBar->Draw (state);
				StatusBar->DrawTopStuff (state);
			}
			else
			{
				StatusBar->DrawBottomStuff (HUD_StatusBar);
				StatusBar->Draw (HUD_StatusBar);
				StatusBar->DrawTopStuff (HUD_StatusBar);
			}

			if ( viewactive )
			{
				// [BC] Handle rendering for the possession module.
				POSSESSION_Render( );

				// [BC] Render the scoreboard [BB] respecting free spectate mode.
				if (( players[consoleplayer].camera != NULL ) && ( players[consoleplayer].camera->player != NULL ) && !CLIENTDEMO_IsInFreeSpectateMode())
					SCOREBOARD_Render( players[consoleplayer].camera->player - players );
				else
					SCOREBOARD_Render( consoleplayer );

				// Render any medals the player might have been awarded.
				MEDAL_Render( );

				// Render all medals the player currently has.
				if ( Button_ShowMedals.bDown )
				{
					if (( players[consoleplayer].camera != NULL ) && ( players[consoleplayer].camera->player != NULL ))
						MEDAL_RenderAllMedalsFullscreen( players[consoleplayer].camera->player ); // [CK] Fixed 'mo' to 'camera' (which was probably intended)
					else
						MEDAL_RenderAllMedalsFullscreen( &players[consoleplayer] );
				}
			}

			// Render chat prompt.
			CHAT_Render( );
			break;

		case GS_INTERMISSION:
			screen->SetBlendingRect(0,0,0,0);
			hw2d = screen->Begin2D(false);
			WI_Drawer ();

			// Render all medals the player currently has.
			if ( Button_ShowMedals.bDown )
			{
				if (( players[consoleplayer].camera != NULL ) && ( players[consoleplayer].camera->player != NULL ))
					MEDAL_RenderAllMedalsFullscreen( players[consoleplayer].camera->player );
				else
					MEDAL_RenderAllMedalsFullscreen( &players[consoleplayer] );
			}

			// Allow people to see the full scoreboard in campaign mode.
			if (( CAMPAIGN_InCampaign( )) && Button_ShowScores.bDown )
			{
				// Render the scoreboard.
				if (( players[consoleplayer].camera != NULL ) && ( players[consoleplayer].camera->player != NULL ))
					SCOREBOARD_RenderBoard( players[consoleplayer].camera->player - players );
				else
					SCOREBOARD_RenderBoard( consoleplayer );
			}

			// Render chat prompt.
			CHAT_Render( );
			break;

		case GS_FINALE:
			screen->SetBlendingRect(0,0,0,0);
			hw2d = screen->Begin2D(false);
			F_Drawer ();
			break;

		case GS_DEMOSCREEN:
			screen->SetBlendingRect(0,0,0,0);
			hw2d = screen->Begin2D(false);
			D_PageDrawer ();
			break;

		default:
			break;
		}
	}
	if ( NETWORK_InClientMode() )
	{
		// Draw a "Waiting for server..." message if the server is lagging.
		if ( CLIENT_GetServerLagging( ) == true )
		{
			USHORT				usTextColor;
			char				szString[64];
			DHUDMessageFadeOut	*pMsg;

			// Build the string and text color;
			sprintf( szString, "Waiting for server..." );
			usTextColor = CR_GREEN;

			pMsg = new DHUDMessageFadeOut( SmallFont, szString,
				1.5f,
				0.9f,
				0,
				0,
				(EColorRange)usTextColor,
				0.15f,
				0.35f );

			StatusBar->AttachMessage( pMsg, MAKE_ID('C','L','A','G') );
		}
		// Draw a "CONNECTION INTERRUPTED" message if the client is lagging.
		else if ( CLIENT_GetClientLagging( ) == true )
		{
			USHORT				usTextColor;
			char				szString[64];
			DHUDMessageFadeOut	*pMsg;

			// Build the string and text color;
			sprintf( szString, "CONNECTION INTERRUPTED!" );
			usTextColor = CR_GREEN;

			pMsg = new DHUDMessageFadeOut( SmallFont, szString,
				1.5f,
				0.9f,
				0,
				0,
				(EColorRange)usTextColor,
				0.15f,
				0.35f );

			StatusBar->AttachMessage( pMsg, MAKE_ID('C','L','A','G') );
		}
	}

	// draw pause pic
	if (paused && menuactive == MENU_Off)
	{
		FTexture *tex;
		int x;

		tex = TexMan(gameinfo.PauseSign);
		x = (SCREENWIDTH - tex->GetScaledWidth() * CleanXfac)/2 +
			tex->GetScaledLeftOffset() * CleanXfac;
		screen->DrawTexture (tex, x, 4, DTA_CleanNoMove, true, TAG_DONE);
	}

	// [RH] Draw icon, if any
	if (D_DrawIcon)
	{
		FTextureID picnum = TexMan.CheckForTexture (D_DrawIcon, FTexture::TEX_MiscPatch);

		D_DrawIcon = NULL;
		if (picnum.isValid())
		{
			FTexture *tex = TexMan[picnum];
			screen->DrawTexture (tex, 160 - tex->GetScaledWidth()/2, 100 - tex->GetScaledHeight()/2,
				DTA_320x200, true, TAG_DONE);
		}
		NoWipe = 10;
	}

	if (snd_drawoutput)
	{
		GSnd->DrawWaveDebug(snd_drawoutput);
	}

	if (!wipe || NoWipe < 0)
	{
		NetUpdate ();			// send out any new accumulation
		// normal update
		C_DrawConsole (hw2d);	// draw console
		M_Drawer ();			// menu is drawn even on top of everything
		FStat::PrintStat ();
		screen->Update ();		// page flip or blit buffer
	}
	else
	{
		// wipe update
		unsigned int wipestart, nowtime, diff;
		bool done;

		GSnd->SetSfxPaused(true, 1);
		I_FreezeTime(true);
		screen->WipeEndScreen ();

		wipestart = I_MSTime();
		NetUpdate();		// send out any new accumulation

		do
		{
			do
			{
				I_WaitVBL(2);
				nowtime = I_MSTime();
				diff = (nowtime - wipestart) * 40 / 1000;	// Using 35 here feels too slow.
			} while (diff < 1);
			wipestart = nowtime;
			done = screen->WipeDo (1);
			C_DrawConsole (hw2d);	// console and
			M_Drawer ();			// menu are drawn even on top of wipes
			screen->Update ();		// page flip or blit buffer
			NetUpdate ();			// [RH] not sure this is needed anymore
		} while (!done);
		screen->WipeCleanup();
		I_FreezeTime(false);
		GSnd->SetSfxPaused(false, 1);
	}

	cycles.Unclock();
	FrameCycles = cycles;
}

//==========================================================================
//
// D_ErrorCleanup ()
//
// Cleanup after a recoverable error or a restart
//==========================================================================

void D_ErrorCleanup ()
{
	// [BC] Handle server error cleanup seperately.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVER_ErrorCleanup( );
		return;
	}

	savegamerestore = false;
	screen->Unlock ();

	// [BC] Remove all the bots from this game.
	BOTS_RemoveAllBots( false );

	D_QuitNetGame ();

	// [BB] Tell the server we're leaving the game.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENT_QuitNetworkGame( NULL );

	if (demorecording || demoplayback)
		G_CheckDemoStatus ();
	// [BC] Support for client-side demos.
	if ( CLIENTDEMO_IsRecording( ))
		CLIENTDEMO_FinishRecording( );
	if ( CLIENTDEMO_IsPlaying( ))
		CLIENTDEMO_FinishPlaying( );
	Net_ClearBuffers ();
	G_NewInit ();
	singletics = false;
	playeringame[0] = 1;
	players[0].playerstate = PST_LIVE;
	gameaction = ga_fullconsole;
	if (gamestate == GS_DEMOSCREEN)
	{
		menuactive = MENU_Off;
	}
	insave = false;
	Renderer->ErrorCleanup();

	// [BB] We are not in a level anymore.
	level.info = NULL;
	// [BB] Also reset the local flood timers of the client.
	CLIENT_ResetFloodTimers();
}

//==========================================================================
//
// D_DoomLoop
//
// Manages timing and IO, calls all ?_Responder, ?_Ticker, and ?_Drawer,
// calls I_GetTime, I_StartFrame, and I_StartTic
//
//==========================================================================

void D_DoomLoop ()
{
	int lasttic = 0;

	// Clamp the timer to TICRATE until the playloop has been entered.
	r_NoInterpolate = true;
	Page = Advisory = NULL;

	vid_cursor.Callback();

	for (;;)
	{
		try
		{
			switch ( NETWORK_GetState( ))
			{
			case NETSTATE_CLIENT:

				// [BB] Recieve packets whenever possible (not only once each tic) to allow
				// for an accurate ping measurement.
				// [BB] Possibly it's bad when we parse packets before we spawned in the map
				// or the first tic was completed, so wait until then.
				if ( ( CLIENT_GetConnectionState( ) == CTS_ACTIVE ) && level.maptime != 0 )
					CLIENT_GetPackets( );

				// frame syncronous IO operations
				if (gametic > lasttic)
				{
					lasttic = gametic;
					I_StartFrame ();
				}

				// Run at least 1 tick.
				TryRunTics( );

				// Move positional sounds.
				// NOTE: .camera can be NULL if player has loaded the level but
				// but the player hasn't spawned yet.
				if ( players[consoleplayer].camera )
					S_UpdateSounds( players[consoleplayer].camera );

				// Update display, next frame, with current state.

//		if ( players[consoleplayer].mo )
//		players[consoleplayer].viewz = players[consoleplayer].mo->z + 41*FRACUNIT;

				D_Display( );
				break;
			case NETSTATE_SERVER:

				SERVER_Tick( );
				break;
			default:

				// frame syncronous IO operations
				if (gametic > lasttic)
				{
					lasttic = gametic;
					I_StartFrame ();
				}
				
				// process one or more tics
				if (singletics)
				{
					I_StartTic ();
					D_ProcessEvents ();
					G_BuildTiccmd (&netcmds[consoleplayer][maketic%BACKUPTICS]);
					if (advancedemo)
						D_DoAdvanceDemo ();
					// Console Ticker
					C_Ticker ();
					// Menu Ticker
					M_Ticker ();
					// Game Ticker
					G_Ticker ();
					gametic++;
					maketic++;
					GC::CheckGC ();
					Net_NewMakeTic ();
				}
				else
				{
					TryRunTics (); // will run at least one tic
				}
				// Update display, next frame, with current state.
				I_StartTic ();
				D_Display ();
				break;
			}
		}
		catch (CRecoverableError &error)
		{
			if (error.GetMessage ())
			{
				// [BC] Give this message a little more presence in server mode.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					Printf( "*** ERROR: %s\n", error.GetMessage( ));
				else
					Printf (PRINT_BOLD, "\n%s\n", error.GetMessage());
			}
			D_ErrorCleanup ();
		}
	}
}

//==========================================================================
//
// D_PageTicker
//
//==========================================================================

void D_PageTicker (void)
{
	if (--pagetic < 0)
		D_AdvanceDemo ();
}

//==========================================================================
//
// D_PageDrawer
//
//==========================================================================

void D_PageDrawer (void)
{
	if (Page != NULL)
	{
		screen->DrawTexture (Page, 0, 0,
			DTA_Fullscreen, true,
			DTA_Masked, false,
			DTA_BilinearFilter, true,
			TAG_DONE);
		screen->FillBorder (NULL);
	}
	else
	{
		screen->Clear (0, 0, SCREENWIDTH, SCREENHEIGHT, 0, 0);
		if (!PageBlank)
		{
			screen->DrawText (SmallFont, CR_WHITE, 0, 0, "Page graphic goes here", TAG_DONE);
		}
	}
	if (Advisory != NULL)
	{
		screen->DrawTexture (Advisory, 4, 160, DTA_320x200, true, TAG_DONE);
	}
}

//==========================================================================
//
// D_AdvanceDemo
//
// Called after each demo or intro demosequence finishes
//
//==========================================================================

void D_AdvanceDemo (void)
{
	advancedemo = true;
}

//==========================================================================
//
// D_DoStrifeAdvanceDemo
//
//==========================================================================

void D_DoStrifeAdvanceDemo ()
{
	static const char *const fullVoices[6] =
	{
		"svox/pro1", "svox/pro2", "svox/pro3", "svox/pro4", "svox/pro5", "svox/pro6"
	};
	static const char *const teaserVoices[6] =
	{
		"svox/voc91", "svox/voc92", "svox/voc93", "svox/voc94", "svox/voc95", "svox/voc96"
	};
	const char *const *voices = gameinfo.flags & GI_SHAREWARE ? teaserVoices : fullVoices;
	const char *pagename = NULL;

	gamestate = GS_DEMOSCREEN;
	PageBlank = false;

	switch (demosequence)
	{
	default:
	case 0:
		pagetic = 6 * TICRATE;
		pagename = "TITLEPIC";
		if (Wads.CheckNumForName ("d_logo", ns_music) < 0)
		{ // strife0.wad does not have d_logo
			S_StartMusic ("");
		}
		else
		{
			S_StartMusic ("d_logo");
		}
		C_HideConsole ();
		break;

	case 1:
		// [RH] Strife fades to black and then to the Rogue logo, but
		// I think it looks better if it doesn't fade.
		pagetic = 10 * TICRATE/35;
		pagename = "";	// PANEL0, but strife0.wad doesn't have it, so don't use it.
		PageBlank = true;
		S_Sound (CHAN_VOICE | CHAN_UI, "bishop/active", 1, ATTN_NORM);
		break;

	case 2:
		pagetic = 4 * TICRATE;
		pagename = "RGELOGO";
		break;

	case 3:
		pagetic = 7 * TICRATE;
		pagename = "PANEL1";
		S_Sound (CHAN_VOICE | CHAN_UI, voices[0], 1, ATTN_NORM);
		// The new Strife teaser has D_FMINTR.
		// The full retail Strife has D_INTRO.
		// And the old Strife teaser has both. (I do not know which one it actually uses, nor do I care.)
		S_StartMusic (gameinfo.flags & GI_TEASER2 ? "d_fmintr" : "d_intro");
		break;

	case 4:
		pagetic = 9 * TICRATE;
		pagename = "PANEL2";
		S_Sound (CHAN_VOICE | CHAN_UI, voices[1], 1, ATTN_NORM);
		break;

	case 5:
		pagetic = 12 * TICRATE;
		pagename = "PANEL3";
		S_Sound (CHAN_VOICE | CHAN_UI, voices[2], 1, ATTN_NORM);
		break;

	case 6:
		pagetic = 11 * TICRATE;
		pagename = "PANEL4";
		S_Sound (CHAN_VOICE | CHAN_UI, voices[3], 1, ATTN_NORM);
		break;

	case 7:
		pagetic = 10 * TICRATE;
		pagename = "PANEL5";
		S_Sound (CHAN_VOICE | CHAN_UI, voices[4], 1, ATTN_NORM);
		break;

	case 8:
		pagetic = 16 * TICRATE;
		pagename = "PANEL6";
		S_Sound (CHAN_VOICE | CHAN_UI, voices[5], 1, ATTN_NORM);
		break;

	case 9:
		pagetic = 6 * TICRATE;
		pagename = "vellogo";
		wipegamestate = GS_FORCEWIPEFADE;
		break;

	case 10:
		pagetic = 12 * TICRATE;
		pagename = "CREDIT";
		wipegamestate = GS_FORCEWIPEFADE;
		break;
	}
	if (demosequence++ > 10)
		demosequence = 0;
	if (demosequence == 9 && !(gameinfo.flags & GI_SHAREWARE))
		demosequence = 10;

	if (pagename)
	{
		if (Page != NULL)
		{
			Page->Unload ();
			Page = NULL;
		}
		if (pagename[0])
		{
			Page = TexMan[pagename];
		}
	}
}

//==========================================================================
//
// D_DoAdvanceDemo
//
//==========================================================================

void D_DoAdvanceDemo (void)
{
	static char demoname[8] = "DEMO1";
	static int democount = 0;
	static int pagecount;
	const char *pagename = NULL;

	advancedemo = false;

	if (gameaction != ga_nothing)
	{
		return;
	}

	V_SetBlend (0,0,0,0);
	players[consoleplayer].playerstate = PST_LIVE;	// not reborn
	usergame = false;				// no save / end game here
	paused = 0;

	// [RH] If you want something more dynamic for your title, create a map
	// and name it TITLEMAP. That map will be loaded and used as the title.

	if (P_CheckMapData("TITLEMAP"))
	{
		G_InitNew ("TITLEMAP", true);
		return;
	}

	if (gameinfo.gametype == GAME_Strife)
	{
		D_DoStrifeAdvanceDemo ();
		return;
	}

	switch (demosequence)
	{
	case 3:
		if (gameinfo.advisoryTime)
		{
			Advisory = TexMan["ADVISOR"];
			demosequence = 1;
			pagetic = (int)(gameinfo.advisoryTime * TICRATE);
			break;
		}
		// fall through to case 1 if no advisory notice

	case 1:
		Advisory = NULL;
		if (!M_DemoNoPlay)
		{
			V_SetBorderNeedRefresh();
			democount++;
			mysnprintf (demoname + 4, countof(demoname) - 4, "%d", democount);
			if (Wads.CheckNumForName (demoname) < 0)
			{
				demosequence = 0;
				democount = 0;
				// falls through to case 0 below
			}
			else
			{
				G_DeferedPlayDemo (demoname);
				demosequence = 2;
				break;
			}
		}

	default:
	case 0:
		gamestate = GS_DEMOSCREEN;
		pagename = gameinfo.titlePage;
		pagetic = (int)(gameinfo.titleTime * TICRATE);
		S_ChangeMusic (gameinfo.titleMusic, gameinfo.titleOrder, false);
		demosequence = 3;
		pagecount = 0;
		C_HideConsole ();
		break;

	case 2:
		pagetic = (int)(gameinfo.pageTime * TICRATE);
		gamestate = GS_DEMOSCREEN;
		if (gameinfo.creditPages.Size() > 0)
		{
			pagename = gameinfo.creditPages[pagecount];
			pagecount = (pagecount+1) % gameinfo.creditPages.Size();
		}
		demosequence = 1;
		break;
	}

	if (pagename)
	{
		if (Page != NULL)
		{
			Page->Unload ();
		}
		Page = TexMan[pagename];
	}
}

//==========================================================================
//
// D_StartTitle
//
//==========================================================================

void D_StartTitle (void)
{
	gameaction = ga_nothing;
	demosequence = -1;
	D_AdvanceDemo ();
}

//==========================================================================
//
// Cmd_Endgame
//
// [RH] Quit the current game and go to fullscreen console
//
//==========================================================================

CCMD (endgame)
{
	// [Dusk] If we're a client, ending the game involves disconnecting.
	// Therefore this should do whatever disconnect does so I'll just call that.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		C_DoCommand( "disconnect" );
		return;
	}

	if ( NETWORK_GetState( ) == NETSTATE_SINGLE )
	{
		gameaction = ga_fullconsole;
		demosequence = -1;
	}
}

//==========================================================================
//
// ParseCVarInfo
//
//==========================================================================

void ParseCVarInfo()
{
	int lump, lastlump = 0;
	bool addedcvars = false;

	while ((lump = Wads.FindLump("CVARINFO", &lastlump)) != -1)
	{
		FScanner sc(lump);
		sc.SetCMode(true);

		while (sc.GetToken())
		{
			FString cvarname;
			char *cvardefault = NULL;
			ECVarType cvartype = CVAR_Dummy;
			int cvarflags = CVAR_MOD|CVAR_ARCHIVE;
			FBaseCVar *cvar;
			bool local = false; // [TP] true for local cvars

			// Check for flag tokens.
			while (sc.TokenType == TK_Identifier)
			{
				if (stricmp(sc.String, "server") == 0)
				{
					cvarflags |= CVAR_SERVERINFO;
				}
				else if (stricmp(sc.String, "user") == 0)
				{
					cvarflags |= CVAR_USERINFO;
				}
				else if (stricmp(sc.String, "noarchive") == 0)
				{
					cvarflags &= ~CVAR_ARCHIVE;
				}
				// [TP]
				else if ( stricmp(sc.String, "local" ) == 0 )
				{
					local = true;
				}
				else
				{
					sc.ScriptError("Unknown cvar attribute '%s'", sc.String);
				}
				sc.MustGetAnyToken();
			}
			// Do some sanity checks.
			if ((cvarflags & (CVAR_SERVERINFO|CVAR_USERINFO)) == 0 ||
				(cvarflags & (CVAR_SERVERINFO|CVAR_USERINFO)) == (CVAR_SERVERINFO|CVAR_USERINFO))
			{
				// [TP] Allow local cvars if given explicitly
				if ( local == false )
					sc.ScriptError("One of 'server', 'user' or 'local' must be specified");
			}
			// The next token must be the cvar type.
			if (sc.TokenType == TK_Bool)
			{
				cvartype = CVAR_Bool;
			}
			else if (sc.TokenType == TK_Int)
			{
				cvartype = CVAR_Int;
			}
			else if (sc.TokenType == TK_Float)
			{
				cvartype = CVAR_Float;
			}
			else if (sc.TokenType == TK_Color)
			{
				cvartype = CVAR_Color;
			}
			else if (sc.TokenType == TK_String)
			{
				cvartype = CVAR_String;
			}
			else
			{
				sc.ScriptError("Bad cvar type '%s'", sc.String);
			}
			// The next token must be the cvar name.
			sc.MustGetToken(TK_Identifier);
			if (FindCVar(sc.String, NULL) != NULL)
			{
				// [BB] Extended error message.
				sc.ScriptError("cvar '%s' already exists\n\nRemove '%s' and all other conflicting cvars from your ini and restart %s to continue.", sc.String, sc.String, GAMENAME);
			}
			cvarname = sc.String;
			// A default value is optional and signalled by a '=' token.
			if (sc.CheckToken('='))
			{
				switch (cvartype)
				{
				case CVAR_Bool:
					if (!sc.CheckToken(TK_True) && !sc.CheckToken(TK_False))
					{
						sc.ScriptError("Expected true or false");
					}
					cvardefault = sc.String;
					break;
				case CVAR_Int:
					sc.MustGetNumber();
					cvardefault = sc.String;
					break;
				case CVAR_Float:
					sc.MustGetFloat();
					cvardefault = sc.String;
					break;
				default:
					sc.MustGetString();
					cvardefault = sc.String;
					break;
				}
			}
			// Now create the cvar.
			cvar = C_CreateCVar(cvarname, cvartype, cvarflags);
			if (cvardefault != NULL)
			{
				UCVarValue val;
				val.String = cvardefault;
				cvar->SetGenericRepDefault(val, CVAR_String);
			}
			// To be like C and ACS, require a semicolon after everything.
			sc.MustGetToken(';');
			addedcvars = true;
		}
	}
	// Only load mod cvars from the config if we defined some, so we don't
	// clutter up the cvar space when not playing mods with custom cvars.
	if (addedcvars)
	{
		GameConfig->DoModSetup (gameinfo.ConfigName);
	}
}

//==========================================================================
//
//
// D_AddFile
//
//==========================================================================

bool D_AddFile (TArray<FString> &wadfiles, const char *file, bool check, int position)
{
	if (file == NULL)
	{
		return false;
	}

	if (check && !DirEntryExists (file))
	{
		const char *f = BaseFileSearch (file, ".wad");
		if (f == NULL)
		{
			Printf ("Can't find '%s'\n", file);
			return false;
		}
		file = f;
	}

	FString f = file;
	FixPathSeperator(f);
	if (position == -1) wadfiles.Push(f);
	else wadfiles.Insert(position, f);
	return true;

}

//==========================================================================
//
// D_AddWildFile
//
//==========================================================================

void D_AddWildFile (TArray<FString> &wadfiles, const char *value)
{
	const char *wadfile = BaseFileSearch (value, ".wad");

	if (wadfile != NULL)
	{
		D_AddFile (wadfiles, wadfile);
	}
	else
	{ // Try pattern matching
		findstate_t findstate;
		char path[PATH_MAX];
		char *sep;
		void *handle = I_FindFirst (value, &findstate);

		strcpy (path, value);
		sep = strrchr (path, '/');
		if (sep == NULL)
		{
			sep = strrchr (path, '\\');
#ifdef _WIN32
			if (sep == NULL && path[1] == ':')
			{
				sep = path + 1;
			}
#endif
		}

		if (handle != ((void *)-1))
		{
			do
			{
				if (!(I_FindAttr(&findstate) & FA_DIREC))
				{
					if (sep == NULL)
					{
						D_AddFile (wadfiles, I_FindName (&findstate));
					}
					else
					{
						strcpy (sep+1, I_FindName (&findstate));
						D_AddFile (wadfiles, path);
					}
				}
			} while (I_FindNext (handle, &findstate) == 0);
		}
		I_FindClose (handle);
	}
}

//==========================================================================
//
// D_AddConfigWads
//
// Adds all files in the specified config file section.
//
//==========================================================================

void D_AddConfigWads (TArray<FString> &wadfiles, const char *section)
{
	if (GameConfig->SetSection (section))
	{
		const char *key;
		const char *value;
		FConfigFile::Position pos;

		while (GameConfig->NextInSection (key, value))
		{
			if (stricmp (key, "Path") == 0)
			{
				// D_AddWildFile resets GameConfig's position, so remember it
				GameConfig->GetPosition (pos);
				D_AddWildFile (wadfiles, value);
				// Reset GameConfig's position to get next wad
				GameConfig->SetPosition (pos);
			}
		}
	}
}

//==========================================================================
//
// D_AddDirectory
//
// Add all .wad and .pk3 files in a directory. Does not descend into subdirectories.
//
//==========================================================================

// [BB] To prevent any code duplication from loading .wad and .pk3 instead of just .wad,
// I moved all code that needs to be excecuted separately for wad and pk3 into this helper function.
void D_AddDirectoryHelper( const char* FileMask, char skindir[PATH_MAX], size_t stuffstart )
{
	void *handle;
	findstate_t findstate;
	if ((handle = I_FindFirst (FileMask, &findstate)) != (void *)-1)
	{
		do
		{
			if (!(I_FindAttr (&findstate) & FA_DIREC))
			{
				strcpy (skindir + stuffstart, I_FindName (&findstate));
				// [BB] We consider this file as loaded automatically.
				if ( D_AddFile (allwads, skindir) )
					autoloadedwads.Push ( allwads[ allwads.Size() - 1 ] );
			}
		} while (I_FindNext (handle, &findstate) == 0);
		I_FindClose (handle);
	}
}

// [BB] Removed wadfiles
static void D_AddDirectory (/*TArray<FString> &wadfiles,*/ const char *dir)
{
	char curdir[PATH_MAX];

	if (getcwd (curdir, PATH_MAX))
	{
		char skindir[PATH_MAX];
		size_t stuffstart;

		stuffstart = strlen (dir);
		memcpy (skindir, dir, stuffstart*sizeof(*dir));
		skindir[stuffstart] = 0;

		if (skindir[stuffstart-1] == '/')
		{
			skindir[--stuffstart] = 0;
		}

		if (!chdir (skindir))
		{
			skindir[stuffstart++] = '/';
			D_AddDirectoryHelper( "*.wad", skindir, stuffstart );
			D_AddDirectoryHelper( "*.pk3", skindir, stuffstart );
		}
		chdir (curdir);
	}
}

//==========================================================================
//
// D_AddSubdirectory
//
// [BB] Add all .wad and .pk3 files in a subdirectory of the program-
// directory and of HOME/.zdoom (if the enviroment variable HOME is defined).
// Under Unix in addition all these files in the subdirectory of SHARE_DIR
// are loaded.
//
//==========================================================================

void D_AddSubdirectory (const char *Subdirectory)
{
	FString dirName;
#ifdef __unix__
	dirName = SHARE_DIR;
	dirName += Subdirectory;
	D_AddDirectory (dirName);
#endif
	dirName = progdir;
	dirName += Subdirectory;
	D_AddDirectory (dirName);

/* [BB] New ZDoom code, unused so far.
#ifdef __unix__
	dirName = NicePath("~/" GAME_DIR "/"Subdirectory);
	D_AddDirectory (dirName);
#endif	
*/
	const char *home = getenv ("HOME");
	if (home)
	{
		dirName = home;
		if (home[strlen(home) - 1] != '/')
		{
			dirName += '/';
		}
		dirName += "." GAMENAMELOWERCASE "/";
		dirName += Subdirectory;
		D_AddDirectory (dirName);
	}

}


//==========================================================================
//
// BaseFileSearch
//
// If a file does not exist at <file>, looks for it in the directories
// specified in the config file. Returns the path to the file, if found,
// or NULL if it could not be found.
//
//==========================================================================

static const char *BaseFileSearch (const char *file, const char *ext, bool lookfirstinprogdir)
{
	static char wad[PATH_MAX];

	if (lookfirstinprogdir)
	{
		mysnprintf (wad, countof(wad), "%s%s%s", progdir.GetChars(), progdir[progdir.Len() - 1] != '/' ? "/" : "", file);
		if (DirEntryExists (wad))
		{
			return wad;
		}
	}

	if (DirEntryExists (file))
	{
		mysnprintf (wad, countof(wad), "%s", file);
		return wad;
	}

	if (GameConfig->SetSection ("FileSearch.Directories"))
	{
		const char *key;
		const char *value;

		while (GameConfig->NextInSection (key, value))
		{
			if (stricmp (key, "Path") == 0)
			{
				FString dir;

				dir = NicePath(value);
				if (dir.IsNotEmpty())
				{
					mysnprintf (wad, countof(wad), "%s%s%s", dir.GetChars(), dir[dir.Len() - 1] != '/' ? "/" : "", file);
					if (DirEntryExists (wad))
					{
						return wad;
					}
				}
			}
		}
	}

	// Retry, this time with a default extension
	if (ext != NULL)
	{
		FString tmp = file;
		DefaultExtension (tmp, ext);
		return BaseFileSearch (tmp, NULL);
	}
	return NULL;
}

//==========================================================================
//
// ConsiderPatches
//
// Tries to add any deh/bex patches from the command line.
//
//==========================================================================

bool ConsiderPatches (const char *arg)
{
	int i, argc;
	FString *args;
	const char *f;

	argc = Args->CheckParmList(arg, &args);
	for (i = 0; i < argc; ++i)
	{
		if ( (f = BaseFileSearch(args[i], ".deh")) ||
			 (f = BaseFileSearch(args[i], ".bex")) )
		{
			D_LoadDehFile(f);
		}
	}
	return argc > 0;
}

//==========================================================================
//
// D_MultiExec
//
//==========================================================================

FExecList *D_MultiExec (DArgs *list, FExecList *exec)
{
	for (int i = 0; i < list->NumArgs(); ++i)
	{
		exec = C_ParseExecFile(list->GetArg(i), exec);
	}
	return exec;
}

// [TP] Added parameterName
static void GetCmdLineFiles(TArray<FString> &wadfiles, const char* parameterName = "-file")
{
	FString *args;
	int i, argc;

	argc = Args->CheckParmList(parameterName, &args); // [TP] Added parameterName
	for (i = 0; i < argc; ++i)
	{
		D_AddWildFile(wadfiles, args[i]);
	}
}

static void CopyFiles(TArray<FString> &to, TArray<FString> &from)
{
	unsigned int ndx = to.Reserve(from.Size());
	for(unsigned i=0;i<from.Size(); i++)
	{
		to[ndx+i] = from[i];
	}
}

static FString ParseGameInfo(TArray<FString> &pwads, const char *fn, const char *data, int size)
{
	FScanner sc;
	FString iwad;
	int pos = 0;

	const char *lastSlash = strrchr (fn, '/');

	sc.OpenMem("GAMEINFO", data, size);
	while(sc.GetToken())
	{
		sc.TokenMustBe(TK_Identifier);
		FString nextKey = sc.String;
		sc.MustGetToken('=');
		if (!nextKey.CompareNoCase("IWAD"))
		{
			sc.MustGetString();
			iwad = sc.String;
		}
		else if (!nextKey.CompareNoCase("LOAD"))
		{
			do
			{
				sc.MustGetString();

				// Try looking for the wad in the same directory as the .wad
				// before looking for it in the current directory.

				FString checkpath;
				if (lastSlash != NULL)
				{
					checkpath = FString(fn, (lastSlash - fn) + 1);
					checkpath += sc.String;
				}
				else
				{
					checkpath = sc.String;
				}

				// [BB] The server informs the launcher about all pwads loaded here,
				// so the client simply ignores the auto loading and relies on the 
				// information from the launcher instead.
				if ( Args->CheckParm ( "-connect" ) )
					continue;

				if (!FileExists(checkpath))
				{
					pos += D_AddFile(pwads, sc.String, true, pos);
				}
				else
				{
					pos += D_AddFile(pwads, checkpath, true, pos);
				}
			}
			while (sc.CheckToken(','));
		}
		else if (!nextKey.CompareNoCase("NOSPRITERENAME"))
		{
			sc.MustGetString();
			nospriterename = sc.Compare("true");
		}
		else if (!nextKey.CompareNoCase("STARTUPTITLE"))
		{
			sc.MustGetString();
			DoomStartupInfo.Name = sc.String;
		}
		else if (!nextKey.CompareNoCase("STARTUPCOLORS"))
		{
			sc.MustGetString();
			DoomStartupInfo.FgColor = V_GetColor(NULL, sc.String);
			sc.MustGetStringName(",");
			sc.MustGetString();
			DoomStartupInfo.BkColor = V_GetColor(NULL, sc.String);
		}
		else if (!nextKey.CompareNoCase("STARTUPTYPE"))
		{
			sc.MustGetString();
			FString sttype = sc.String;
			if (!sttype.CompareNoCase("DOOM"))
				DoomStartupInfo.Type = FStartupInfo::DoomStartup;
			else if (!sttype.CompareNoCase("HERETIC"))
				DoomStartupInfo.Type = FStartupInfo::HereticStartup;
			else if (!sttype.CompareNoCase("HEXEN"))
				DoomStartupInfo.Type = FStartupInfo::HexenStartup;
			else if (!sttype.CompareNoCase("STRIFE"))
				DoomStartupInfo.Type = FStartupInfo::StrifeStartup;
			else DoomStartupInfo.Type = FStartupInfo::DefaultStartup;
		}
		else if (!nextKey.CompareNoCase("STARTUPSONG"))
		{
			sc.MustGetString();
			DoomStartupInfo.Song = sc.String;
		}
		else
		{
			// Silently ignore unknown properties
			do
			{
				sc.MustGetAnyToken();
			}
			while(sc.CheckToken(','));
		}
	}
	return iwad;
}

static FString CheckGameInfo(TArray<FString> & pwads)
{
	DWORD t = I_FPSTime();
	// scan the list of WADs backwards to find the last one that contains a GAMEINFO lump
	for(int i=pwads.Size()-1; i>=0; i--)
	{
		bool isdir = false;
		FileReader *wadinfo;
		FResourceFile *resfile;
		const char *filename = pwads[i];

		// Does this exist? If so, is it a directory?
		struct stat info;
		if (stat(pwads[i], &info) != 0)
		{
			Printf(TEXTCOLOR_RED "Could not stat %s\n", filename);
			continue;
		}
		isdir = (info.st_mode & S_IFDIR) != 0;

		if (!isdir)
		{
			try
			{
				wadinfo = new FileReader(filename);
			}
			catch (CRecoverableError &)
			{ 
				// Didn't find file
				continue;
			}
			resfile = FResourceFile::OpenResourceFile(filename, wadinfo, true);
		}
		else
			resfile = FResourceFile::OpenDirectory(filename, true);

		if (resfile != NULL)
		{
			DWORD cnt = resfile->LumpCount();
			for(int i=cnt-1; i>=0; i--)
			{
				FResourceLump *lmp = resfile->GetLump(i);

				if (lmp->Namespace == ns_global && !stricmp(lmp->Name, "GAMEINFO"))
				{
					// Found one!
					FString iwad = ParseGameInfo(pwads, resfile->Filename, (const char*)lmp->CacheLump(), lmp->LumpSize);
					delete resfile;
					return iwad;
				}
			}
			delete resfile;
		}
	}
	t = I_FPSTime() - t;
	Printf("Gameinfo scan took %d ms\n", t);
	return "";
}

//==========================================================================
//
// Checks the IWAD for MAP01 and if found sets GI_MAPxx
//
//==========================================================================

static void SetMapxxFlag()
{
	int lump_name = Wads.CheckNumForName("MAP01", ns_global, FWadCollection::IWAD_FILENUM);
	int lump_wad = Wads.CheckNumForFullName("maps/map01.wad", FWadCollection::IWAD_FILENUM);
	int lump_map = Wads.CheckNumForFullName("maps/map01.map", FWadCollection::IWAD_FILENUM);

	if (lump_name >= 0 || lump_wad >= 0 || lump_map >= 0) gameinfo.flags |= GI_MAPxx;
}

//==========================================================================
//
// Initialize
//
//==========================================================================

#ifndef	WIN32
extern int do_stdin;
#endif

static void D_DoomInit()
{
	// Set the FPU precision to 53 significant bits. This is the default
	// for Visual C++, but not for GCC, so some slight math variances
	// might crop up if we leave it alone.
#if defined(_FPU_GETCW) && defined(_FPU_EXTENDED) && defined(_FPU_DOUBLE)
	{
		int cw;
		_FPU_GETCW(cw);
		cw = (cw & ~_FPU_EXTENDED) | _FPU_DOUBLE;
		_FPU_SETCW(cw);
	}
#elif defined(_PC_53)
// On the x64 architecture, changing the floating point precision is not supported.
#ifndef _WIN64
	int cfp = _control87(_PC_53, _MCW_PC);
#endif
#endif

	// Check response files before coalescing file parameters.
	M_FindResponseFile ();

	// Combine different file parameters with their pre-switch bits.
	Args->CollectFiles("-deh", ".deh");
	Args->CollectFiles("-bex", ".bex");
	Args->CollectFiles("-exec", ".cfg");
	Args->CollectFiles("-playdemo", ".lmp");
	Args->CollectFiles("-file", NULL);	// anything left goes after -file
	Args->CollectFiles( "-optfile", NULL ); // [TP]

	atterm (C_DeinitConsole);

	gamestate = GS_STARTUP;

	// Initialize the game mode module.
	GAMEMODE_Construct( );

	// Determine if we're going to be a server, client, or local player.
	if ( Args->CheckParm( "-host" ))
		NETWORK_SetState( NETSTATE_SERVER );

#ifndef	WIN32
	// Check if we should read standard input.
	if (Args->CheckParm("-noinput"))
		do_stdin = 0;
#endif

	SetLanguageIDs ();

	const char *v = Args->CheckValue("-rngseed");
	if (v)
	{
		rngseed = staticrngseed = atoi(v);
		use_staticrng = true;
		Printf("D_DoomInit: Static RNGseed %d set.\n", rngseed);
	}
	else
	{
		rngseed = I_MakeRNGSeed();
		use_staticrng = false;
	}
		
	// Initialize the map rotation list. We need to do this before we call M_LoadDefaults,
	// because that executes autoexec.cfg, where people may have +addmap. We don't want to over-
	// write what they do.
	MAPROTATION_Construct( );

	// Initialize the pathing module.
	ASTAR_Construct( );

	// Initialize the callvote module.
	CALLVOTE_Construct( );

	FRandom::StaticClearRandom ();

	Printf ("M_LoadDefaults: Load system defaults.\n");
	M_LoadDefaults ();			// load before initing other systems

}

//==========================================================================
//
// AddAutoloadFiles
//
//==========================================================================

static void AddAutoloadFiles(const char *gamesection)
{
	if (!(gameinfo.flags & GI_SHAREWARE) && !Args->CheckParm("-noautoload"))
	{
		FString file;

		// [RH] zvox.wad - A wad I had intended to be automatically generated
		// from Q2's pak0.pak so the female and cyborg player could have
		// voices. I never got around to writing the utility to do it, though.
		// And I probably never will now. But I know at least one person uses
		// it for something else, so this gets to stay here.
		// [BB] Loading zvox with Skulltag introduces a bag of problems and does't do any good.
		//const char *wad = BaseFileSearch ("zvox.wad", NULL);
		//if (wad)
		//	D_AddFile (wad, true, false);	// [BC]

		// [RH] Add any .wad files in the skins directory
		// [BB] Also add pk3 files and add the files from
		// the announcer and bots directories.
		// Under Unix looks into SHARE_DIR, progdir and HOME/.zdoom dir.
		// Under Windows looks into progdir and HOME/.zdoom dir.
		D_AddSubdirectory ( "skins" );
		D_AddSubdirectory ( "announcer" );
		D_AddSubdirectory ( "bots" );

		// Add common (global) wads
		D_AddConfigWads (allwads, "Global.Autoload");

		// Add game-specific wads
		file = gameinfo.ConfigName;
		file += ".Autoload";
		D_AddConfigWads (allwads, file);

		// Add IWAD-specific wads
		if (gamesection != NULL)
		{
			file = gamesection;
			file += ".Autoload";
			D_AddConfigWads(allwads, file);
		}
	}
}

//==========================================================================
//
// CheckCmdLine
//
//==========================================================================

static void CheckCmdLine()
{
	int flags = dmflags;
	int p;
	const char *v;

	Printf ("Checking cmd-line parameters...\n");
	if (Args->CheckParm ("-nomonsters"))	flags |= DF_NO_MONSTERS;
	if (Args->CheckParm ("-respawn"))		flags |= DF_MONSTERS_RESPAWN;
	if (Args->CheckParm ("-fast"))			flags |= DF_FAST_MONSTERS;

	devparm = !!Args->CheckParm ("-devparm");

	if (Args->CheckParm ("-altdeath"))
	{
		deathmatch = 1;
		flags |= DF_SPAWN_FARTHEST | DF_ITEMS_RESPAWN | DF_NO_MONSTERS;
	}
	else if (Args->CheckParm ("-deathmatch"))
	{
		deathmatch = 1;
		flags |= DF_SPAWN_FARTHEST | DF_WEAPONS_STAY | DF_ITEMS_RESPAWN | DF_NO_MONSTERS;
	}
	if (Args->CheckParm ("-teamplay"))
	{
		teamplay = 1;
		flags |= DF_SPAWN_FARTHEST | DF_WEAPONS_STAY | DF_ITEMS_RESPAWN | DF_NO_MONSTERS;
	}
	if (Args->CheckParm ("-duel"))
	{
		duel = 1;
		flags |= DF_WEAPONS_STAY | DF_ITEMS_RESPAWN | DF_NO_MONSTERS;
	}
	if (Args->CheckParm ("-terminator"))
	{
		terminator = 1;
		flags |= DF_SPAWN_FARTHEST | DF_WEAPONS_STAY | DF_ITEMS_RESPAWN | DF_NO_MONSTERS;
	}
	if (Args->CheckParm ("-lastmanstanding"))
	{
		lastmanstanding = 1;
		flags |= DF_SPAWN_FARTHEST | DF_WEAPONS_STAY | DF_ITEMS_RESPAWN | DF_NO_MONSTERS;
	}
	if (Args->CheckParm ("-teamlms"))
	{
		teamlms = 1;
		flags |= DF_SPAWN_FARTHEST | DF_WEAPONS_STAY | DF_ITEMS_RESPAWN | DF_NO_MONSTERS;
	}
	if (Args->CheckParm ("-possession"))
	{
		possession = 1;
		flags |= DF_SPAWN_FARTHEST | DF_WEAPONS_STAY | DF_ITEMS_RESPAWN | DF_NO_MONSTERS;
	}
	if (Args->CheckParm ("-teampossession"))
	{
		teampossession = 1;
		flags |= DF_SPAWN_FARTHEST | DF_WEAPONS_STAY | DF_ITEMS_RESPAWN | DF_NO_MONSTERS;
	}
	if ( Args->CheckParm( "-teamgame" ))
	{
		teamgame = 1;
		flags |= DF_WEAPONS_STAY | DF_ITEMS_RESPAWN | DF_NO_MONSTERS;
	}	
	if ( Args->CheckParm( "-ctf" ))
	{
		ctf = 1;
		flags |= DF_WEAPONS_STAY | DF_ITEMS_RESPAWN | DF_NO_MONSTERS;
	}
	if ( Args->CheckParm( "-oneflagctf" ))
	{
		oneflagctf = 1;
		flags |= DF_WEAPONS_STAY | DF_ITEMS_RESPAWN | DF_NO_MONSTERS;
	}
	if ( Args->CheckParm( "-skulltag" ))
	{
		skulltag = 1;
		flags |= DF_WEAPONS_STAY | DF_ITEMS_RESPAWN | DF_NO_MONSTERS;
	}

	dmflags = flags;

	// get skill / episode / map from parms
	if (gameinfo.gametype != GAME_Hexen)
	{
		startmap = (gameinfo.flags & GI_MAPxx) ? "MAP01" : "E1M1";
	}
	else
	{
		// [BB] The server crashes, if you select "&wt@01" as startmap.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			startmap = "MAP01";
		else
			startmap = "&wt@01";
	}
	autostart = StoredWarp.IsNotEmpty();
				
	const char *val = Args->CheckValue ("-skill");
	if (val)
	{
		gameskill = val[0] - '1';
		autostart = true;
	}

	p = Args->CheckParm ("-warp");
	if (p && p < Args->NumArgs() - 1)
	{
		int ep, map;

		if (gameinfo.flags & GI_MAPxx)
		{
			ep = 1;
			map = atoi (Args->GetArg(p+1));
		}
		else 
		{
			ep = atoi (Args->GetArg(p+1));
			map = p < Args->NumArgs() - 2 ? atoi (Args->GetArg(p+2)) : 10;
			if (map < 1 || map > 9)
			{
				map = ep;
				ep = 1;
			}
		}

		startmap = CalcMapName (ep, map);
		autostart = true;
	}

	// [RH] Hack to handle +map. The standard console command line handler
	// won't be able to handle it, so we take it out of the command line and set
	// it up like -warp.
	FString mapvalue = Args->TakeValue("+map");
	if (mapvalue.IsNotEmpty())
	{
		if (!P_CheckMapData(mapvalue))
		{
			Printf ("Can't find map %s\n", mapvalue.GetChars());
		}
		else
		{
			startmap = mapvalue;
			autostart = true;
		}
	}

	if (devparm)
	{
		Printf ("%s", GStrings("D_DEVSTR"));
	}

	// turbo option  // [RH] (now a cvar)
	v = Args->CheckValue("-turbo");
	if (v != NULL)
	{
		double amt = atof(v);
		Printf ("turbo scale: %.0f%%\n", amt);
		turbo = (float)amt;
	}

	v = Args->CheckValue ("-timer");
	if (v)
	{
		double time = strtod (v, NULL);
		Printf ("Levels will end after %g minute%s.\n", time, time > 1 ? "s" : "");
		timelimit = (float)time;
	}

	v = Args->CheckValue ("-avg");
	if (v)
	{
		Printf ("Austin Virtual Gaming: Levels will end after 20 minutes\n");
		timelimit = 20.f;
	}

	//
	//  Build status bar line!
	//
	if (deathmatch)
		StartScreen->AppendStatusLine("DeathMatch...");
	if (dmflags & DF_NO_MONSTERS)
		StartScreen->AppendStatusLine("No Monsters...");
	if (dmflags & DF_MONSTERS_RESPAWN)
		StartScreen->AppendStatusLine("Respawning...");
	if (autostart)
	{
		FString temp;
		temp.Format ("Warp to map %s, Skill %d ", startmap.GetChars(), gameskill + 1);
		StartScreen->AppendStatusLine(temp);
	}
}

//==========================================================================
//
// FinalGC
//
// If this doesn't free everything, the debug CRT will let us know.
//
//==========================================================================

static void FinalGC()
{
	Args = NULL;
	GC::FullGC();
	GC::DelSoftRootHead();	// the soft root head will not be collected by a GC so we have to do it explicitly
}

//==========================================================================
//
// D_DoomMain
//
//==========================================================================

void D_DoomMain (void)
{
	int p;
	const char *v;
	const char *wad;
	DArgs *execFiles;
	TArray<FString> pwads;
	/* [BB] Zandronum uses different bot code and thus doesn't need these.
	FString *args;
	int argcount;
	*/

	// +logfile gets checked too late to catch the full startup log in the logfile so do some extra check for it here.
  /* [BB] Zandronum uses some cvars to configure the logfile, these are not loaded yet.
	FString logfile = Args->TakeValue("+logfile");
	if (logfile != NULL)
	{
		execLogfile(logfile);
	}
  */

	D_DoomInit();
	PClass::StaticInit ();
	atterm(FinalGC);

	// [RH] Make sure zdoom.pk3 is always loaded,
	// as it contains magic stuff we need.

	wad = BaseFileSearch (BASEWAD, NULL, true);
	if (wad == NULL)
	{
		I_FatalError ("Cannot find " BASEWAD);
	}
	FString basewad = wad;


	// reinit from here

	do
	{
		if (restart)
		{
			C_InitConsole(SCREENWIDTH, SCREENHEIGHT, false);
		}
		nospriterename = false;

		// Load zdoom.pk3 alone so that we can get access to the internal gameinfos before 
		// the IWAD is known.

		GetCmdLineFiles(pwads);
		GetCmdLineFiles( optionalwads, "-optfile" ); // [TP] Note - this goes directly into the global variable
		FString iwad = CheckGameInfo(pwads);

		// The IWAD selection dialogue does not show in fullscreen so if the
		// restart is initiated without a defined IWAD assume for now that it's not going to change.
		if (iwad.Len() == 0) iwad = lastIWAD;

		FIWadManager *iwad_man = new FIWadManager;
		const FIWADInfo *iwad_info = iwad_man->FindIWAD(allwads, iwad, basewad);
		gameinfo.gametype = iwad_info->gametype;
		gameinfo.flags = iwad_info->flags;
		gameinfo.ConfigName = iwad_info->Configname;
		lastIWAD = iwad;

		if ((gameinfo.flags & GI_SHAREWARE) && pwads.Size() > 0)
		{
			I_FatalError ("You cannot -file with the shareware version. Register!");
		}

		FBaseCVar::DisableCallbacks();
		GameConfig->DoGameSetup (gameinfo.ConfigName);

		AddAutoloadFiles(iwad_info->Autoname);

		// Process automatically executed files
		FExecList *exec;
		execFiles = new DArgs;
		GameConfig->AddAutoexec(execFiles, gameinfo.ConfigName);
		exec = D_MultiExec(execFiles, NULL);

		// Process .cfg files at the start of the command line.
		execFiles = Args->GatherFiles ("-exec");
		exec = D_MultiExec(execFiles, exec);

		// [RH] process all + commands on the command line
		exec = C_ParseCmdLineParams(exec);

		CopyFiles(allwads, pwads);
		CopyFiles( allwads, optionalwads ); // [TP]
		if (exec != NULL)
		{
			exec->AddPullins(allwads);
		}

		// Since this function will never leave we must delete this array here manually.
		pwads.Clear();
		pwads.ShrinkToFit();

		Printf ("W_Init: Init WADfiles.\n");
		Wads.InitMultipleFiles (/*allwads*/); // [BB] Removed argument.
		allwads.Clear();
		allwads.ShrinkToFit();
		SetMapxxFlag();

		// Now that wads are loaded, define mod-specific cvars.
		ParseCVarInfo();

		// Actually exec command line commands and exec files.
		if (exec != NULL)
		{
			exec->ExecCommands();
			delete exec;
			exec = NULL;
		}

		// Initialize the chat module.
		CHAT_Construct( );

		// Initialize the team info.
		TEAM_Construct( );

		// Initialize the duel module.
		DUEL_Construct( );

		// Initialize the LMS module.
		LASTMANSTANDING_Construct( );

		// Initialize the possession module.
		POSSESSION_Construct( );

		// Initialize the survival module.
		SURVIVAL_Construct( );

		// Initialize the invasion module.
		INVASION_Construct( );

		// Initialize the join queue module.
		JOINQUEUE_Construct( );

		// Initialize the medal info.
		MEDAL_Construct( );

		// Initialize the announcer info.
		ANNOUNCER_Construct( );
		ANNOUNCER_ParseAnnouncerInfo( );

		// Initialize the campaign module.
		CAMPAIGN_Construct( );
		CAMPAIGN_ParseCampaignInfo( );

		// [BB] Parse the GAMEMODE lump.
		GAMEMODE_ParseGamemodeInfo( );

		// [RH] Initialize localizable strings.
		GStrings.LoadStrings (false);

		V_InitFontColors ();

		// [RH] Moved these up here so that we can do most of our
		//		startup output in a fullscreen console.

		// [BB] Zandronum handles chat differently.
		//CT_Init ();

		if (!restart)
		{
			Printf ("I_Init: Setting up machine state.\n");
			I_Init ();
			I_CreateRenderer();
		}

		// Server doesn't need video.
		if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		{
			Printf ("V_Init: allocate screen.\n");
			V_Init (!!restart);
		}
		// [BB] We still need to initialize the palette for the ACS
		// function CreateTranslation.
		else
			InitPalette ();

		// Base systems have been inited; enable cvar callbacks
		FBaseCVar::EnableCallbacks ();

		// [BB] Make sure that the callback of cooperative is first called after the ones of deathmatch and teamgame.
		cooperative.Callback();

		// [RC] Start the G15 LCD module here.
		G15_Construct ();

		Printf ("S_Init: Setting up sound.\n");
		S_Init ();

		Printf ("ST_Init: Init startup screen.\n");
		if (!restart)
		{
			StartScreen = FStartupScreen::CreateInstance (TexMan.GuesstimateNumTextures() + 5);
		}
		else
		{
			StartScreen = new FStartupScreen(0);
		}

		ParseCompatibility();

		CheckCmdLine();

		// [RH] Load sound environments
		S_ParseReverbDef ();

		// [RH] Parse any SNDINFO lumps
		Printf ("S_InitData: Load sound definitions.\n");
		S_InitData ();

		// [RH] Parse through all loaded mapinfo lumps
		Printf ("G_ParseMapInfo: Load map definitions.\n");
		G_ParseMapInfo (iwad_info->MapInfo);
		ReadStatistics();

		// MUSINFO must be parsed after MAPINFO
		S_ParseMusInfo();

		// [BL] Load SectInfo
		SECTINFO_Load();

		Printf ("Texman.Init: Init texture manager.\n");
		TexMan.Init();
		C_InitConback();

		// [CW] Parse any TEAMINFO lumps.
		Printf ("ParseTeamInfo: Load team definitions.\n");
		//TeamLibrary.ParseTeamInfo ();
		// [BB] At the moment Skulltag still doesn't use the new ZDoom TeamLibrary class.
		TEAMINFO_Init ();

		FActorInfo::StaticInit ();

		// [GRB] Initialize player class list
		SetupPlayerClasses ();


		// [RH] Load custom key and weapon settings from WADs
		D_LoadWadSettings ();

		// [GRB] Check if someone used clearplayerclasses but not addplayerclass
		if (PlayerClasses.Size () == 0)
		{
			I_FatalError ("No player classes defined");
		}

		StartScreen->Progress ();

		Printf ("R_Init: Init %s refresh subsystem.\n", gameinfo.ConfigName.GetChars());
		StartScreen->LoadingStatus ("Loading graphics", 0x3f);
		R_Init ();

		Printf ("DecalLibrary: Load decals.\n");
		DecalLibrary.ReadAllDecals ();

		// [RH] Add any .deh and .bex files on the command line.
		// If there are none, try adding any in the config file.
		// Note that the command line overrides defaults from the config.

		if ((ConsiderPatches("-deh") | ConsiderPatches("-bex")) == 0 &&
			gameinfo.gametype == GAME_Doom && GameConfig->SetSection ("Doom.DefaultDehacked"))
		{
			const char *key;
			const char *value;

			while (GameConfig->NextInSection (key, value))
			{
				if (stricmp (key, "Path") == 0 && FileExists (value))
				{
					Printf ("Applying patch %s\n", value);
					D_LoadDehFile(value);
				}
			}
		}

		// Load embedded Dehacked patches
		D_LoadDehLumps();

		// Create replacements for dehacked pickups
		FinishDehPatch();

		FActorInfo::StaticSetActorNums ();

		// [TP] Init preferred weapon order
		PWO_Init();

		/* [BB] Zandronum uses different bot code.
		//Added by MC:
		bglobal.getspawned.Clear();
		argcount = Args->CheckParmList("-bots", &args);
		for (p = 0; p < argcount; ++p)
		{
			bglobal.getspawned.Push(args[p]);
		}
		bglobal.spawn_tries = 0;
		bglobal.wanted_botnum = bglobal.getspawned.Size();
		*/

		// [BC] Server doesn't use any status bar stuff.
		// [BC] Now that all the skins have been loaded, parse the bot info.
		// [TP] This needs to be done before the menus are initialized.
		BOTS_Construct( );
		BOTS_ParseBotInfo( );
		GameConfig->ReadRevealedBotsAndSkins( );

		Printf ("M_Init: Init menus.\n");
		M_Init ();

		Printf ("P_Init: Init Playloop state.\n");
		StartScreen->LoadingStatus ("Init game engine", 0x3f);
		AM_StaticInit();
		P_Init ();

		P_SetupWeapons_ntohton();

		//SBarInfo support.
		SBarInfo::Load();
		HUD_InitHud();

		// [RH] User-configurable startup strings. Because BOOM does.
		static const char *startupString[5] = {
			"STARTUP1", "STARTUP2", "STARTUP3", "STARTUP4", "STARTUP5"
		};
		for (p = 0; p < 5; ++p)
		{
			const char *str = GStrings[startupString[p]];
			if (str != NULL && str[0] != '\0')
			{
				Printf ("%s\n", str);
			}
		}

		if (!restart)
		{
			Printf ("D_CheckNetGame: Checking network game status.\n");
			StartScreen->LoadingStatus ("Checking network game status.", 0x3f);
			D_CheckNetGame ();
		}

		// [BC] 
		Printf( "Initializing network subsystem.\n" );
		if ( Args->CheckParm( "-host" ))
			SERVER_Construct( );
		else
		{
			CLIENT_Construct( );
			CLIENT_PREDICT_Construct( );
			CLIENTSTATISTICS_Construct( );
		}

		// [BB]
		DATABASE_Construct( );

		// [BC] Initialize the browser module.
		BROWSER_Construct( );

		// [RH] Lock any cvars that should be locked now that we're
		// about to begin the game.
		FBaseCVar::EnableNoSet ();

		delete iwad_man;	// now we won't need this anymore

		// [RH] Run any saved commands from the command line or autoexec.cfg now.
		gamestate = GS_FULLCONSOLE;
		Net_NewMakeTic ();
		DThinker::RunThinkers ();
		gamestate = GS_STARTUP;

		if (!restart)
		{
			// Server doesn't record/play demos, etc.
			if ( NETWORK_GetState( ) != NETSTATE_SERVER )
			{
				// Make sure that if we're using the -record parameter to record a client demo, we
				// also don't record a regular demo.
				if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
				{
					// start the apropriate game based on parms
					v = Args->CheckValue ("-record");

					if (v)
					{
						G_RecordDemo (v);
						autostart = true;
					}
				}

				delete StartScreen;
				StartScreen = NULL;
				S_Sound (CHAN_BODY, "misc/startupdone", 1, ATTN_NONE);

				if (Args->CheckParm("-norun"))
				{
					throw CNoRunExit();
				}

				V_Init2();
				UpdateJoystickMenu(NULL);

				v = Args->CheckValue ("-loadgame");
				if (v)
				{
					FString file(v);
					FixPathSeperator (file);
					DefaultExtension (file, ".zds");
					G_LoadGame (file);
				}

				v = Args->CheckValue("-playdemo");
				if (v != NULL)
				{
					singledemo = true;				// quit after one demo
					G_DeferedPlayDemo (v);
					D_DoomLoop ();	// never returns
				}

				v = Args->CheckValue ("-timedemo");
				if (v)
				{
					G_TimeDemo (v);
					D_DoomLoop ();	// never returns
				}

			}
			// [BC] The server still needs to delete the start screen.
			else
			{
				delete ( StartScreen );
				StartScreen = NULL;
			}
			if (gameaction != ga_loadgame && gameaction != ga_loadgamehidecon)
			{
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					G_NewInit( );

					// Check if we have map rotation setup. If we do, use the first map there.
					if (( sv_maprotation ) && ( MAPROTATION_GetNumEntries( ) > 0 ))
					{
						// [BB] G_InitNew seems to alter the contents of the first argument, which it shouldn't.
						// This causes the "Frags" bug. The following is just a workaround, the behavior of
						// G_InitNew should be fixed.
						char levelname[10];
						// [K6] Start with a random map if we are using sv_randommaprotation.
						sprintf( levelname, "%s", MAPROTATION_GetMap( sv_randommaprotation ? M_Random.Random( ) % MAPROTATION_GetNumEntries( ) : 0 )->mapname );
						MAPROTATION_SetPositionToMap( levelname );
						G_InitNew( levelname, false );
						//G_InitNew( MAPROTATION_GetMapName( 0 ), false );
					}
					else
						G_InitNew( startmap, false );
				}
				else
				{
					if (autostart)// || ( NETWORK_GetState( ) != NETSTATE_SINGLE ))
					{
						// Do not do any screenwipes when autostarting a game.
						if (!Args->CheckParm("-warpwipe"))
						{
							NoWipe = TICRATE;
						}
						CheckWarpTransMap (startmap, true);
						if (demorecording)
							G_BeginRecording (startmap);
						G_InitNew (startmap, false);
					if (StoredWarp.IsNotEmpty())
					{
						AddCommandString(StoredWarp.LockBuffer());
						StoredWarp = NULL;
					}
					}
					else if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
					{
						D_StartTitle ();				// start up intro loop
					}
				}
			}
			else if (demorecording)
			{
				G_BeginRecording (NULL);
			}
						
			atterm (D_QuitNetGame);		// killough
		}
		// [BB] The server doesn't need any of this to restart.
		else if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		{
			// let the renderer reinitialize some stuff if needed
			screen->GameRestart();
			// These calls from inside V_Init2 are still necessary
			C_NewModeAdjust();
			M_InitVideoModesMenu();
			D_StartTitle ();				// start up intro loop
			setmodeneeded = false;			// This may be set to true here, but isn't needed for a restart
		}
		// [BB] .. but the server needs to load the new startmap.
		else
		{
			G_InitNew( startmap, false );
		}

		// Client mode starts off in the full console!
		if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
			gameaction = ga_fullconsole;

		// [BC] If we specified -private, make sv_updatemaster false.
		if ( Args->CheckParm( "-private" ))
			sv_updatemaster = false;

		// [BC] Potentially send an update to the master server.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVER_MASTER_Tick( );
			SERVER_MASTER_Broadcast( );
		}

		// [BC] Little hack for +addbot.
		for ( LONG lIdx = 0; lIdx < ( Args->NumArgs( ) - 1 ); lIdx++ )
		{
			if ( stricmp( Args->GetArg( lIdx ), "+addbot" ) == 0 )
			{
				char	szString[128];

				sprintf( szString, "addbot %s", Args->GetArg( lIdx + 1 ));
				AddCommandString( szString );
			}
		}

		try
		{
			D_DoomLoop ();		// never returns
		}
		catch (CRestartException &)
		{
			// Music and sound should be stopped first
			S_StopMusic(true);
			S_StopAllChannels ();

			M_ClearMenus();					// close menu if open
			F_EndFinale();					// If an intermission is active, end it now

			// clean up game state
			ST_Clear();
			D_ErrorCleanup ();
			P_FreeLevelData();
			P_FreeExtraLevelData();

			M_SaveDefaults(NULL);			// save config before the restart

			// delete all data that cannot be left until reinitialization
			V_ClearFonts();					// must clear global font pointers
			R_DeinitTranslationTables();	// some tables are initialized from outside the translation code.
			gameinfo.~gameinfo_t();
			new (&gameinfo) gameinfo_t;		// Reset gameinfo
			S_Shutdown();					// free all channels and delete playlist
			C_ClearAliases();				// CCMDs won't be reinitialized so these need to be deleted here

			// [BB]
			NETWORK_Destruct();

			GC::FullGC();					// perform one final garbage collection before deleting the class data
			PClass::ClearRuntimeData();		// clear all runtime generated class data
			restart++;
		}
	}
	while (1);
}

//==========================================================================
//
// restart the game
//
//==========================================================================

CCMD(restart)
{
	// remove command line args that would get in the way during restart
	Args->RemoveArgs("-iwad");
	Args->RemoveArgs("-deh");
	Args->RemoveArgs("-bex");
	Args->RemoveArgs("-playdemo");
	Args->RemoveArgs("-file");
	Args->RemoveArgs("-altdeath");
	Args->RemoveArgs("-deathmatch");
	Args->RemoveArgs("-skill");
	Args->RemoveArgs("-savedir");
	Args->RemoveArgs("-xlat");
	Args->RemoveArgs("-oldsprites");
	// [BB]
	Args->RemoveArgs("-connect");

	if (argv.argc() > 1)
	{
		for(int i=1;i<argv.argc(); i++)
		{
			Args->AppendArg(argv[i]);
		}
	}

	// initiate the restart
	throw CRestartException();
}

//==========================================================================
//
// FStartupScreen Constructor
//
//==========================================================================

FStartupScreen::FStartupScreen(int max_progress)
{
	MaxPos = max_progress;
	CurPos = 0;
	NotchPos = 0;
}

//==========================================================================
//
// FStartupScreen Destructor
//
//==========================================================================

FStartupScreen::~FStartupScreen()
{
}

//==========================================================================
//
// FStartupScreen :: LoadingStatus
//
// Used by Heretic for the Loading Status "window."
//
//==========================================================================

void FStartupScreen::LoadingStatus(const char *message, int colors)
{
}

//==========================================================================
//
// FStartupScreen :: AppendStatusLine
//
// Used by Heretic for the "status line" at the bottom of the screen.
//
//==========================================================================

void FStartupScreen::AppendStatusLine(const char *status)
{
}


void FStartupScreen::Progress(void) {}
void FStartupScreen::NetInit(char const *,int) {}
void FStartupScreen::NetProgress(int) {}
void FStartupScreen::NetMessage(char const *,...) {}
void FStartupScreen::NetDone(void) {}
bool FStartupScreen::NetLoop(bool (*)(void *),void *) { return false; }

