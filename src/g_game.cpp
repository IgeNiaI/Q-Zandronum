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
// DESCRIPTION:  none
//
//-----------------------------------------------------------------------------



#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <time.h>
#ifdef __APPLE__
#include <CoreServices/CoreServices.h>
#endif

#include "templates.h"
#include "version.h"
#include "doomdef.h" 
#include "doomstat.h"
#include "d_protocol.h"
#include "d_netinf.h"
#include "intermission/intermission.h"
#include "m_argv.h"
#include "m_misc.h"
#include "menu/menu.h"
#include "m_random.h"
#include "m_crc32.h"
#include "i_system.h"
#include "i_input.h"
#include "p_saveg.h"
#include "p_tick.h"
#include "d_main.h"
#include "wi_stuff.h"
#include "hu_stuff.h"
#include "st_stuff.h"
#include "am_map.h"
#include "c_console.h"
#include "c_cvars.h"
#include "c_bind.h"
#include "c_dispatch.h"
#include "v_video.h"
#include "w_wad.h"
#include "p_local.h" 
#include "s_sound.h"
#include "gstrings.h"
#include "r_main.h"
#include "r_sky.h"
#include "g_game.h"
#include "g_level.h"
#include "sbar.h"
#include "m_swap.h"
#include "m_png.h"
#include "gi.h"
#include "a_keys.h"
#include "a_artifacts.h"
#include "r_data/r_translate.h"
#include "cmdlib.h"
#include "d_net.h"
#include "d_event.h"
#include "p_acs.h"
#include "m_joy.h"
#include "farchive.h"
#include "r_renderer.h"
#include "r_data/colormaps.h"
// [BB] New #includes.
#include "network.h"
#include "chat.h"
#include "deathmatch.h"
#include "duel.h"
#include "team.h"
#include "a_doomglobal.h"
#include "sv_commands.h"
#include "medal.h"
#include "cl_demo.h"
#include "cl_main.h"
#include "cl_statistics.h"
#include "browser.h"
#include "lastmanstanding.h"
#include "campaign.h"
#include "callvote.h"
#include "cooperative.h"
#include "invasion.h"
#include "scoreboard.h"
#include "survival.h"
#include "announcer.h"
#include "p_acs.h"
#include "p_effect.h"
#include "cl_commands.h"
#include "possession.h"
#include "statnums.h"
#include "domination.h"
#include "win32/g15/g15.h"
#include "gl/dynlights/gl_dynlight.h"
#include "unlagged.h"
#include "p_3dmidtex.h"
#include "a_lightning.h"
#include "po_man.h"

#include <zlib.h>

#include "g_hub.h"

static FRandom pr_dmspawn ("DMSpawn");
static FRandom pr_pspawn ("PlayerSpawn");

const int SAVEPICWIDTH = 216;
const int SAVEPICHEIGHT = 162;

bool	G_CheckDemoStatus (void);
void	G_ReadDemoTiccmd (ticcmd_t *cmd, int player);
void	G_WriteDemoTiccmd (ticcmd_t *cmd, int player, int buf);
// [BB] Added bGiveInventory and moved the declaration to g_game.h.
//void	G_PlayerReborn (int player);

void	G_DoNewGame (void);
void	G_DoLoadGame (void);
void	G_DoPlayDemo (void);
void	G_DoCompleted (void);
void	G_DoVictory (void);
void	G_DoWorldDone (void);
void	G_DoSaveGame (bool okForQuicksave, FString filename, const char *description);
void	G_DoAutoSave ();

void STAT_Write(FILE *file);
void STAT_Read(PNGHandle *png);

FIntCVar gameskill ("skill", 2, CVAR_SERVERINFO|CVAR_LATCH);
CVAR (Bool, chasedemo, false, 0);
CVAR (Bool, storesavepic, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, longsavemessages, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (String, save_dir, "", CVAR_ARCHIVE|CVAR_GLOBALCONFIG);
EXTERN_CVAR (Float, con_midtime);

// [BB]
EXTERN_CVAR (Int, vid_renderer)

// [BB]
FString GetEngineString ( )
{
	FString engine;
	engine.Format ( "%s (%s)", GAMESIG, ( vid_renderer ? "GL" : "Software" ) );
	return engine;
}

//==========================================================================
//
// CVAR displaynametags
//
// Selects whether to display name tags or not when changing weapons/items
//
//==========================================================================

// [BB] Changed default to 3 to be consistent with Zandronum's 1.x behavior.
CUSTOM_CVAR (Int, displaynametags, 3, CVAR_ARCHIVE)
{
	if (self < 0 || self > 3)
	{
		self = 0;
	}
}

CVAR(Int, nametagcolor, CR_GOLD, CVAR_ARCHIVE)


gameaction_t	gameaction;
gamestate_t 	gamestate = GS_STARTUP;

int 			paused;
bool 			sendpause;				// send a pause event next tic 
bool			sendsave;				// send a save event next tic 
bool			sendturn180;			// [RH] send a 180 degree turn next tic
bool 			usergame;				// ok to save / end game
bool			insave;					// Game is saving - used to block exit commands

bool			timingdemo; 			// if true, exit with report on completion 
bool 			nodrawers;				// for comparative timing purposes 
bool 			noblit; 				// for comparative timing purposes 

bool	 		viewactive;

player_t		players[MAXPLAYERS + 1];	// [EP] Add 1 slot for the DummyPlayer
bool			playeringame[MAXPLAYERS + 1];	// [EP] Add 1 slot for the DummyPlayer

int 			consoleplayer;			// player taking events
int 			gametic;

CVAR(Bool, demo_compress, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);
FString			demoname;
bool 			demorecording;
bool 			demoplayback;
bool			demonew;				// [RH] Only used around G_InitNew for demos
int				demover;
BYTE*			demobuffer;
BYTE*			demo_p;
BYTE*			democompspot;
BYTE*			demobodyspot;
size_t			maxdemosize;
BYTE*			zdemformend;			// end of FORM ZDEM chunk
BYTE*			zdembodyend;			// end of ZDEM BODY chunk
bool 			singledemo; 			// quit after playing a demo from cmdline 
 
bool 			precache = true;		// if true, load all graphics at start 
 
wbstartstruct_t wminfo; 				// parms for world map / intermission 
 
short			consistancy[MAXPLAYERS][BACKUPTICS];
 
 
#define MAXPLMOVE				(forwardmove[1]) 
 
#define TURBOTHRESHOLD	12800

float	 		normforwardmove[2] = {0x19, 0x32};		// [RH] For setting turbo from console
float	 		normsidemove[2] = {0x18, 0x28};			// [RH] Ditto

fixed_t			forwardmove[2], sidemove[2];
fixed_t 		angleturn[4] = {640, 1280, 320, 320};		// + slow turn
fixed_t			flyspeed[2] = {1*256, 3*256};
int				lookspeed[2] = {450, 512};

#define SLOWTURNTICS	6 

// [BB] This is a new school port, so cl_run defaults to true.
CVAR (Bool,		cl_run,			true,	CVAR_GLOBALCONFIG|CVAR_ARCHIVE)		// Always run?
CVAR (Bool,		invertmouse,	false,	CVAR_GLOBALCONFIG|CVAR_ARCHIVE)		// Invert mouse look down/up?
// [BB] This is a new school port, so freelook defaults to true.
CVAR (Bool,		freelook,		true,	CVAR_GLOBALCONFIG|CVAR_ARCHIVE)		// Always mlook?
CVAR (Bool,		lookstrafe,		false,	CVAR_GLOBALCONFIG|CVAR_ARCHIVE)		// Always strafe with mouse?
CVAR (Float,	m_pitch,		1.f,	CVAR_GLOBALCONFIG|CVAR_ARCHIVE)		// Mouse speeds
CVAR (Float,	m_yaw,			1.f,	CVAR_GLOBALCONFIG|CVAR_ARCHIVE)
CVAR (Float,	m_forward,		1.f,	CVAR_GLOBALCONFIG|CVAR_ARCHIVE)
CVAR (Float,	m_side,			2.f,	CVAR_GLOBALCONFIG|CVAR_ARCHIVE)
 
int 			turnheld;								// for accelerative turning 
 
// mouse values are used once 
int 			mousex;
int 			mousey; 		

FString			savegamefile;
char			savedescription[SAVESTRINGSIZE];

// [RH] Name of screenshot file to generate (usually NULL)
FString			shotfile;

AActor* 		bodyque[BODYQUESIZE]; 
int 			bodyqueslot; 

void R_ExecuteSetViewSize (void);

FString savename;
FString BackupSaveName;

bool SendLand;
const AInventory *SendItemUse, *SendItemDrop;

// [BB] Shall the map be reset as soon as possible?
static	bool	g_bResetMap = false;

// [BC] How many ticks of the end level delay remain?
static	ULONG	g_ulEndLevelDelay = 0;

// [BC] How many ticks until we announce various messages?
static	ULONG	g_ulLevelIntroTicks = 0;

// [BB]
extern SDWORD g_sdwCheckCmd;

EXTERN_CVAR (Int, team)

// [RH] Allow turbo setting anytime during game
CUSTOM_CVAR (Float, turbo, 100.f, 0)
{
	// [BB] Limit CVAR turbo on clients to 100.
	// [TP] Unless sv_cheats is true.
	if ( ( NETWORK_GetState( ) == NETSTATE_CLIENT ) && ( sv_cheats == false ) && ( self > 100.f ) )
		self = 100.f;

	if (self < 10.f)
	{
		self = 10.f;
	}
	else if (self > 255.f)
	{
		self = 255.f;
	}
	else
	{
		double scale = self * 0.01;

		forwardmove[0] = (int)(normforwardmove[0]*scale);
		forwardmove[1] = (int)(normforwardmove[1]*scale);
		sidemove[0] = (int)(normsidemove[0]*scale);
		sidemove[1] = (int)(normsidemove[1]*scale);
	}
}

CCMD (turnspeeds)
{
	if (argv.argc() == 1)
	{
		Printf ("Current turn speeds: %d %d %d %d\n", angleturn[0],
			angleturn[1], angleturn[2], angleturn[3]);
	}
	else
	{
		int i;

		for (i = 1; i <= 4 && i < argv.argc(); ++i)
		{
			angleturn[i-1] = atoi (argv[i]);
		}
		if (i <= 2)
		{
			angleturn[1] = angleturn[0] * 2;
		}
		if (i <= 3)
		{
			angleturn[2] = angleturn[0] / 2;
		}
		if (i <= 4)
		{
			angleturn[3] = angleturn[2];
		}
	}
}

CCMD (slot)
{
	// [BB] The server can't do this.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	if (argv.argc() > 1)
	{
		int slot = atoi (argv[1]);

		if (slot < NUM_WEAPON_SLOTS)
		{
			SendItemUse = players[consoleplayer].weapons.Slots[slot].PickWeapon (&players[consoleplayer], 
				!(dmflags2 & DF2_DONTCHECKAMMO));
		}
	}
}

CCMD (centerview)
{
	// [BC] Only write this byte if we're recording a demo. Otherwise, just do it!
	if ( demorecording )
		Net_WriteByte (DEM_CENTERVIEW);
	else
	{
		Net_DoCommand( DEM_CENTERVIEW, NULL, consoleplayer );

		if ( CLIENTDEMO_IsRecording( ))
			CLIENTDEMO_WriteLocalCommand( CLD_LCMD_CENTERVIEW, NULL );
	}
}

CCMD(crouch)
{
	// [BB] The clients don't use any of the DEM stuff, so emulate the crouch toggling
	// by executing "+crouch" or "-crouch".
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		if ( Button_Crouch.bDown )
			C_DoCommand ( "-crouch" );
		else
			C_DoCommand ( "+crouch" );
		return;
	}

	Net_WriteByte(DEM_CROUCH);
}

CCMD (land)
{
	// [BB] Landing is not allowed, so don't do anything.
	if ( zacompatflags & ZACOMPATF_NO_LAND )
		return;

	SendLand = true;
}

CCMD (pause)
{
	sendpause = true;
}

CCMD (turn180)
{
	sendturn180 = true;
}

// [BB] If possible use team starts in deathmatch game modes with teams, e.g. TDM, TLMS.
CVAR( Bool, sv_useteamstartsindm, false, CVAR_SERVERINFO )

// [BB] In cooperative game modes players are spawned at random player starts instead of the one designated for them.
CVAR( Bool, sv_randomcoopstarts, false, CVAR_SERVERINFO )

CCMD (weapnext)
{
	// [BB] No weapnext while playing a demo.
	if ( CLIENTDEMO_IsPlaying( ) == true )
	{
		Printf ( "You can't use weapnext during demo playback.\n" );
		return;
	}

	// [Zandronum] No weapnext when player is spectating.
	if ( players[consoleplayer].bSpectating )
		return;

	SendItemUse = players[consoleplayer].weapons.PickNextWeapon (&players[consoleplayer]);
 	// [BC] Option to display the name of the weapon being cycled to.
 	if ((displaynametags & 2) && StatusBar && SmallFont && SendItemUse)
 	{
 		StatusBar->AttachMessage(new DHUDMessageFadeOut(SmallFont, SendItemUse->GetTag(),
			1.5f, 0.90f, 0, 0, (EColorRange)*nametagcolor, 2.f, 0.35f), MAKE_ID( 'W', 'E', 'P', 'N' ));
 	}
}

CCMD (weapprev)
{
	// [BB] No weapprev while playing a demo.
	if ( CLIENTDEMO_IsPlaying( ) == true )
	{
		Printf ( "You can't use weapprev during demo playback.\n" );
		return;
	}

	// [Zandronum] No weapprev when player is spectating.
	if ( players[consoleplayer].bSpectating )
		return;

	SendItemUse = players[consoleplayer].weapons.PickPrevWeapon (&players[consoleplayer]);
 	// [BC] Option to display the name of the weapon being cycled to.
 	if ((displaynametags & 2) && StatusBar && SmallFont && SendItemUse)
 	{
 		StatusBar->AttachMessage(new DHUDMessageFadeOut(SmallFont, SendItemUse->GetTag(),
			1.5f, 0.90f, 0, 0, (EColorRange)*nametagcolor, 2.f, 0.35f), MAKE_ID( 'W', 'E', 'P', 'N' ));
 	}
}

CCMD (invnext)
{
	AInventory *next;

	if (who == NULL)
		return;

	if (who->InvSel != NULL)
	{
		if ((next = who->InvSel->NextInv()) != NULL)
		{
			who->InvSel = next;
		}
		else
		{
			// Select the first item in the inventory
			if (!(who->Inventory->ItemFlags & IF_INVBAR))
			{
				who->InvSel = who->Inventory->NextInv();
			}
			else
			{
				who->InvSel = who->Inventory;
			}
		}
		if ((displaynametags & 1) && StatusBar && SmallFont && who->InvSel)
			StatusBar->AttachMessage (new DHUDMessageFadeOut (SmallFont, who->InvSel->GetTag(), 
			1.5f, 0.80f, 0, 0, (EColorRange)*nametagcolor, 2.f, 0.35f), MAKE_ID('S','I','N','V'));
	}
	who->player->inventorytics = 5*TICRATE;
}

CCMD (invprev)
{
	AInventory *item, *newitem;

	if (who == NULL)
		return;

	if (who->InvSel != NULL)
	{
		if ((item = who->InvSel->PrevInv()) != NULL)
		{
			who->InvSel = item;
		}
		else
		{
			// Select the last item in the inventory
			item = who->InvSel;
			while ((newitem = item->NextInv()) != NULL)
			{
				item = newitem;
			}
			who->InvSel = item;
		}
		if ((displaynametags & 1) && StatusBar && SmallFont && who->InvSel)
			StatusBar->AttachMessage (new DHUDMessageFadeOut (SmallFont, who->InvSel->GetTag(), 
			1.5f, 0.80f, 0, 0, (EColorRange)*nametagcolor, 2.f, 0.35f), MAKE_ID('S','I','N','V'));
	}
	who->player->inventorytics = 5*TICRATE;
}

CCMD (invuseall)
{
	SendItemUse = (const AInventory *)1;
}

CCMD (invuse)
{
	if (players[consoleplayer].inventorytics == 0)
	{
		if (players[consoleplayer].mo) SendItemUse = players[consoleplayer].mo->InvSel;
	}
	players[consoleplayer].inventorytics = 0;
}

CCMD(invquery)
{
	AInventory *inv = players[consoleplayer].mo->InvSel;
	if (inv != NULL)
	{
		Printf(PRINT_HIGH, "%s (%dx)\n", inv->GetTag(), inv->Amount);
	}
}

CCMD (use)
{
	if (argv.argc() > 1 && who != NULL)
	{
		SendItemUse = who->FindInventory (PClass::FindClass (argv[1]));
	}
}

CCMD (invdrop)
{
	// [BB/BC] If we are a client, we have to bypass the way ZDoom handles the item usage.
	if( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENTCOMMANDS_RequestInventoryDrop( players[consoleplayer].mo->InvSel );
	else
		if (players[consoleplayer].mo) SendItemDrop = players[consoleplayer].mo->InvSel;
}

CCMD (weapdrop)
{
	// [BB/BC] If we are a client, we have to bypass the way ZDoom handles the item usage.
	if( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENTCOMMANDS_RequestInventoryDrop( players[consoleplayer].ReadyWeapon );
	else
		SendItemDrop = players[consoleplayer].ReadyWeapon;
}

CCMD (drop)
{
	// [BB/BC] If we are a client, we have to bypass the way ZDoom handles the item usage.
	if( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		if (argv.argc() > 1 && who != NULL)
		{
			CLIENTCOMMANDS_RequestInventoryDrop( who->FindInventory (PClass::FindClass (argv[1])) );
		}
	}
	else
	{
		if (argv.argc() > 1 && who != NULL)
		{
			SendItemDrop = who->FindInventory (PClass::FindClass (argv[1]));
		}
	}
}

const PClass *GetFlechetteType(AActor *other);

CCMD (useflechette)
{ // Select from one of arti_poisonbag1-3, whichever the player has
	static const ENamedName bagnames[3] =
	{
		NAME_ArtiPoisonBag1,
		NAME_ArtiPoisonBag2,
		NAME_ArtiPoisonBag3
	};

	if (who == NULL)
		return;

	const PClass *type = GetFlechetteType(who);
	if (type != NULL)
	{
		AInventory *item;
		if ( (item = who->FindInventory (type) ))
		{
			SendItemUse = item;
			return;
		}
	}

	// The default flechette could not be found. Try all 3 types then.
	for (int j = 0; j < 3; ++j)
	{
		AInventory *item;
		if ( (item = who->FindInventory (bagnames[j])) )
		{
			SendItemUse = item;
			break;
		}
	}
}

CCMD (select)
{
	// [BB] The server can't do this.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	if (argv.argc() > 1)
	{
		AInventory *item = who->FindInventory (PClass::FindClass (argv[1]));
		if (item != NULL)
		{
			who->InvSel = item;
		}
	}
	who->player->inventorytics = 5*TICRATE;
}

static inline int joyint(double val)
{
	if (val >= 0)
	{
		return int(ceil(val));
	}
	else
	{
		return int(floor(val));
	}
}

//
// G_BuildTiccmd
// Builds a ticcmd from all of the available inputs
// or reads it from the demo buffer.
// If recording a demo, write it out
//
void G_BuildTiccmd (ticcmd_t *cmd)
{
	int 		strafe;
	int 		speed;
	int 		forward;
	int 		side;
	int			fly;

	ticcmd_t	*base;

	base = I_BaseTiccmd (); 			// empty, or external driver
	*cmd = *base;

	cmd->consistancy = consistancy[consoleplayer][(maketic/ticdup)%BACKUPTICS];

	strafe = Button_Strafe.bDown;
	speed = Button_Speed.bDown ^ (int)cl_run;

	forward = side = fly = 0;

	// [RH] only use two stage accelerative turning on the keyboard
	//		and not the joystick, since we treat the joystick as
	//		the analog device it is.
	if (Button_Left.bDown || Button_Right.bDown)
		turnheld += ticdup;
	else
		turnheld = 0;

	// let movement keys cancel each other out
	if (strafe)
	{
		if (Button_Right.bDown)
			side += sidemove[speed];
		if (Button_Left.bDown)
			side -= sidemove[speed];
	}
	else
	{
		int tspeed = speed;

		if (turnheld < SLOWTURNTICS)
			tspeed += 2;		// slow turn
		
		if (Button_Right.bDown)
		{
			G_AddViewAngle (angleturn[tspeed]);
			LocalKeyboardTurner = true;
		}
		if (Button_Left.bDown)
		{
			G_AddViewAngle (-angleturn[tspeed]);
			LocalKeyboardTurner = true;
		}
	}

	if (Button_LookUp.bDown)
	{
		G_AddViewPitch (lookspeed[speed]);
		LocalKeyboardTurner = true;
	}
	if (Button_LookDown.bDown)
	{
		G_AddViewPitch (-lookspeed[speed]);
		LocalKeyboardTurner = true;
	}

	if (Button_MoveUp.bDown)
		fly += flyspeed[speed];
	if (Button_MoveDown.bDown)
		fly -= flyspeed[speed];

	if (Button_Klook.bDown)
	{
		if (Button_Forward.bDown)
			G_AddViewPitch (lookspeed[speed]);
		if (Button_Back.bDown)
			G_AddViewPitch (-lookspeed[speed]);
	}
	else
	{
		if (Button_Forward.bDown)
			forward += forwardmove[speed];
		if (Button_Back.bDown)
			forward -= forwardmove[speed];
	}

	if (Button_MoveRight.bDown)
		side += sidemove[speed];
	if (Button_MoveLeft.bDown)
		side -= sidemove[speed];

	// buttons
	if (Button_Attack.bDown)		cmd->ucmd.buttons |= BT_ATTACK;
	if (Button_AltAttack.bDown)		cmd->ucmd.buttons |= BT_ALTATTACK;
	if (Button_Use.bDown)			cmd->ucmd.buttons |= BT_USE;
	if (Button_Jump.bDown)			cmd->ucmd.buttons |= BT_JUMP;
	if (Button_Crouch.bDown)		cmd->ucmd.buttons |= BT_CROUCH;
	if (Button_Zoom.bDown)			cmd->ucmd.buttons |= BT_ZOOM;
	if (Button_Reload.bDown)		cmd->ucmd.buttons |= BT_RELOAD;

	if (Button_User1.bDown)			cmd->ucmd.buttons |= BT_USER1;
	if (Button_User2.bDown)			cmd->ucmd.buttons |= BT_USER2;
	if (Button_User3.bDown)			cmd->ucmd.buttons |= BT_USER3;
	if (Button_User4.bDown)			cmd->ucmd.buttons |= BT_USER4;

	if (Button_Speed.bDown)			cmd->ucmd.buttons |= BT_SPEED;
	if (Button_Strafe.bDown)		cmd->ucmd.buttons |= BT_STRAFE;
	if (Button_MoveRight.bDown)		cmd->ucmd.buttons |= BT_MOVERIGHT;
	if (Button_MoveLeft.bDown)		cmd->ucmd.buttons |= BT_MOVELEFT;
	if (Button_LookDown.bDown)		cmd->ucmd.buttons |= BT_LOOKDOWN;
	if (Button_LookUp.bDown)		cmd->ucmd.buttons |= BT_LOOKUP;
	if (Button_Back.bDown)			cmd->ucmd.buttons |= BT_BACK;
	if (Button_Forward.bDown)		cmd->ucmd.buttons |= BT_FORWARD;
	if (Button_Right.bDown)			cmd->ucmd.buttons |= BT_RIGHT;
	if (Button_Left.bDown)			cmd->ucmd.buttons |= BT_LEFT;
	if (Button_MoveDown.bDown)		cmd->ucmd.buttons |= BT_MOVEDOWN;
	if (Button_MoveUp.bDown)		cmd->ucmd.buttons |= BT_MOVEUP;
	if (Button_ShowScores.bDown)	cmd->ucmd.buttons |= BT_SHOWSCORES;

	// Handle joysticks/game controllers.
	float joyaxes[NUM_JOYAXIS];

	I_GetAxes(joyaxes);

	// [Leo] Clamp JOYAXIS_Side.
	joyaxes[JOYAXIS_Side] = clamp<float>(joyaxes[JOYAXIS_Side], -1.f, 1.f);

	// Remap some axes depending on button state.
	if (Button_Strafe.bDown || (Button_Mlook.bDown && lookstrafe))
	{
		joyaxes[JOYAXIS_Side] = joyaxes[JOYAXIS_Yaw];
		joyaxes[JOYAXIS_Yaw] = 0;
	}
	if (Button_Mlook.bDown)
	{
		joyaxes[JOYAXIS_Pitch] = joyaxes[JOYAXIS_Forward];
		joyaxes[JOYAXIS_Forward] = 0;
	}

	if (joyaxes[JOYAXIS_Pitch] != 0)
	{
		G_AddViewPitch(joyint(joyaxes[JOYAXIS_Pitch] * 2048));
		LocalKeyboardTurner = true;
	}
	if (joyaxes[JOYAXIS_Yaw] != 0)
	{
		G_AddViewAngle(joyint(-1280 * joyaxes[JOYAXIS_Yaw]));
		LocalKeyboardTurner = true;
	}

	side -= joyint(sidemove[speed] * joyaxes[JOYAXIS_Side]);
	forward += joyint(joyaxes[JOYAXIS_Forward] * forwardmove[speed]);
	fly += joyint(joyaxes[JOYAXIS_Up] * 2048);

	// Handle mice.
	if (!Button_Mlook.bDown && !freelook)
	{
		forward += (int)((float)mousey * m_forward);
	}

	cmd->ucmd.pitch = LocalViewPitch >> 16;

	if (SendLand)
	{
		SendLand = false;
		fly = -32768;
	}

	if (strafe || lookstrafe)
		side += (int)((float)mousex * m_side);

	mousex = mousey = 0;

	// Build command.
	if (forward > MAXPLMOVE)
		forward = MAXPLMOVE;
	else if (forward < -MAXPLMOVE)
		forward = -MAXPLMOVE;
	if (side > MAXPLMOVE)
		side = MAXPLMOVE;
	else if (side < -MAXPLMOVE)
		side = -MAXPLMOVE;

	cmd->ucmd.forwardmove += forward;
	cmd->ucmd.sidemove += side;
	cmd->ucmd.yaw = LocalViewAngle >> 16;
	cmd->ucmd.upmove = fly;
	LocalViewAngle = 0;
	LocalViewPitch = 0;

	// special buttons
	if (sendturn180)
	{
		sendturn180 = false;
		cmd->ucmd.buttons |= BT_TURN180;
	}
	if (sendpause)
	{
		sendpause = false;
		Net_WriteByte (DEM_PAUSE);
	}
	if (sendsave)
	{
		sendsave = false;
		Net_WriteByte (DEM_SAVEGAME);
		Net_WriteString (savegamefile);
		Net_WriteString (savedescription);
		savegamefile = "";
	}
	// [TP] Don't do this in client mode
	if ( NETWORK_InClientMode() == false )
	{
		if (SendItemUse == (const AInventory *)1)
		{
			Net_WriteByte (DEM_INVUSEALL);
			SendItemUse = NULL;
		}
		else if (SendItemUse != NULL)
		{
			Net_WriteByte (DEM_INVUSE);
			Net_WriteLong (SendItemUse->InventoryID);
			SendItemUse = NULL;
		}
	}
	if (SendItemDrop != NULL)
	{
		Net_WriteByte (DEM_INVDROP);
		Net_WriteLong (SendItemDrop->InventoryID);
		SendItemDrop = NULL;
	}

	cmd->ucmd.forwardmove <<= 8;
	cmd->ucmd.sidemove <<= 8;

	if (cmd->ucmd.sidemove > ( sidemove[1] << 8 ) || cmd->ucmd.sidemove < - ( sidemove[1] << 8 ) ) //[Hyp] Lock client's turning to zero when speed is above sr40
		cmd->ucmd.yaw = 0;

	// [BB] The client calculates a checksum of the ticcmd just built. This
	// should allow us to detect if the ticcmd is manipulated later.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		g_sdwCheckCmd = NETWORK_Check ( cmd );
}

//[Graf Zahl] This really helps if the mouse update rate can't be increased!
CVAR (Bool,		smooth_mouse,	false,	CVAR_GLOBALCONFIG|CVAR_ARCHIVE)

void G_AddViewPitch (int look)
{
	if (gamestate == GS_TITLELEVEL)
	{
		return;
	}
	look <<= 16;
	if (players[consoleplayer].playerstate != PST_DEAD &&		// No adjustment while dead.
		players[consoleplayer].ReadyWeapon != NULL &&			// No adjustment if no weapon.
		players[consoleplayer].ReadyWeapon->FOVScale > 0)		// No adjustment if it is non-positive.
	{
		look = int(look * players[consoleplayer].ReadyWeapon->FOVScale);
	}
	// [BB] Allow spectators to freelook no matter what. Note: This probably causes some
	// sky rendering errors in software mode.
	if (!level.IsFreelookAllowed() && ( players[consoleplayer].bSpectating == false ) && ( CLIENTDEMO_IsInFreeSpectateMode() == false ))
	{
		LocalViewPitch = 0;
	}
	else if (look > 0)
	{
		// Avoid overflowing
		if (LocalViewPitch > INT_MAX - look)
		{
			LocalViewPitch = 0x78000000;
		}
		else
		{
			LocalViewPitch = MIN(LocalViewPitch + look, 0x78000000);
		}
	}
	else if (look < 0)
	{
		// Avoid overflowing
		if (LocalViewPitch < INT_MIN - look)
		{
			LocalViewPitch = -0x78000000;
		}
		else
		{
			LocalViewPitch = MAX(LocalViewPitch + look, -0x78000000);
		}
	}
	if (look != 0)
	{
		LocalKeyboardTurner = smooth_mouse;
	}
}

void G_AddViewAngle (int yaw)
{
	if (gamestate == GS_TITLELEVEL)
	{
		return;
	}
	yaw <<= 16;
	if (players[consoleplayer].playerstate != PST_DEAD &&	// No adjustment while dead.
		players[consoleplayer].ReadyWeapon != NULL &&		// No adjustment if no weapon.
		players[consoleplayer].ReadyWeapon->FOVScale > 0)	// No adjustment if it is non-positive.
	{
		yaw = int(yaw * players[consoleplayer].ReadyWeapon->FOVScale);
	}
	LocalViewAngle -= yaw;
	if (yaw != 0)
	{
		LocalKeyboardTurner = smooth_mouse;
	}
}

EXTERN_CVAR( Bool, sv_cheats );


enum {
	SPY_CANCEL = 0,
	SPY_NEXT,
	SPY_PREV,
};

// [RH] Spy mode has been separated into two console commands.
//		One goes forward; the other goes backward.
// [BC] Prototype for new function.
static void FinishChangeSpy( int pnum );
static void ChangeSpy (int changespy)
{
	// If you're not in a level, then you can't spy.
	if (gamestate != GS_LEVEL)
	{
		return;
	}

	// If not viewing through a player, return your eyes to your own head.
	if (!players[consoleplayer].camera || players[consoleplayer].camera->player == NULL)
	{
		// When watching demos, you will just have to wait until your player
		// has done this for you, since it could desync otherwise.
		if (!demoplayback)
		{
			// [BB] The client doesn't use the DEM stuff.
			if ( NETWORK_InClientMode() )
				players[consoleplayer].camera = players[consoleplayer].mo;
			else
				Net_WriteByte(DEM_REVERTCAMERA);
		}
		return;
	}

	// We may not be allowed to spy on anyone.
	// [SP] Ignore this condition if playing demo or spectating.
	if ( (dmflags2 & DF2_DISALLOW_SPYING) && ( CLIENTDEMO_IsPlaying( ) == false ) &&
		( players[consoleplayer].bSpectating == false ))
		return;

	// [BC] Check a wide array of conditions to see if this is legal.
	if (( demoplayback == false ) &&
		( CLIENTDEMO_IsPlaying( ) == false ) &&
		( players[consoleplayer].bSpectating == false ) &&
		( NETWORK_GetState( ) == NETSTATE_CLIENT ) &&
		( teamgame == false ) &&
		( teamlms == false ) &&
		( teamplay == false ) &&
		( teampossession == false ) &&
		((( deathmatch == false ) && ( teamgame == false )) == false ) &&
		( sv_cheats == false ))
	{
		return;
	}

	// Otherwise, cycle to the next player.
	int pnum = consoleplayer;
	if (changespy != SPY_CANCEL) 
	{
		player_t *player = players[consoleplayer].camera->player;
		// only use the camera as starting index if it's a valid player.
		if (player != NULL) pnum = int(players[consoleplayer].camera->player - players);

		int step = (changespy == SPY_NEXT) ? 1 : -1;

		// [SP] Let's ignore special LMS settigns if we're playing a demo. Otherwise, we need to enforce
		// LMS rules for spectators using spy.
		if ( CLIENTDEMO_IsPlaying( ) == false )
		{
			// [BC] Special conditions for team LMS.
			if (( teamlms ) && (( lmsspectatorsettings & LMS_SPF_VIEW ) == false ))
			{
				// If this player is a true spectator (aka not on a team), don't allow him to change spy.
				if ( PLAYER_IsTrueSpectator( &players[consoleplayer] ))
				{
					players[consoleplayer].camera = players[consoleplayer].mo;
					FinishChangeSpy( consoleplayer );
					return;
				}

				// Break if the player isn't on a team.
				if ( players[consoleplayer].bOnTeam == false )
				{
					players[consoleplayer].camera = players[consoleplayer].mo;
					FinishChangeSpy( consoleplayer );
					return;
				}

				// Loop through all the players, and stop when we find one that's on our team.
				do
				{
					pnum += step;
					pnum &= MAXPLAYERS-1;

					// Skip players not in the game.
					if ( playeringame[pnum] == false )
						continue;

					// Skip other spectators.
					if ( players[pnum].bSpectating )
						continue;

					// Skip players not on our team.
					if (( players[pnum].bOnTeam == false ) || ( players[pnum].ulTeam != players[consoleplayer].ulTeam ))
						continue;

					break;
				} while ( pnum != consoleplayer );

				players[consoleplayer].camera = players[pnum].mo;
				FinishChangeSpy( pnum );
				return;
			}
			// [BC] Don't allow spynext in LMS when the spectator settings forbid it.
			if (( lastmanstanding ) && (( lmsspectatorsettings & LMS_SPF_VIEW ) == false ))
			{
				players[consoleplayer].camera = players[consoleplayer].mo;
				FinishChangeSpy( consoleplayer );
				return;
			}
		}
		// [BC] Always allow spectator spying. [BB] Same when playing a demo.
		if ( players[consoleplayer].bSpectating || CLIENTDEMO_IsPlaying( ) )
		{
			// Loop through all the players, and stop when we find one.
			do
			{
				pnum += step;
				pnum &= MAXPLAYERS-1;

				// Skip players not in the game.
				if ( playeringame[pnum] == false )
					continue;

				// Skip other spectators.
				if ( players[pnum].bSpectating )
					continue;

				break;
			} while ( pnum != consoleplayer );

			FinishChangeSpy( pnum );
			return;
		}

		// [BC] Allow view switch to players on our team.
		if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
		{
			// Break if the player isn't on a team.
			if ( players[consoleplayer].bOnTeam == false )
				return;

			// Loop through all the players, and stop when we find one that's on our team.
			do
			{
				pnum += step;
				pnum &= MAXPLAYERS - 1;

				// Skip players not in the game.
				if ( playeringame[pnum] == false )
					continue;

				// Skip other spectators.
				if ( players[pnum].bSpectating )
					continue;

				// Skip players not on our team.
				if (( players[pnum].bOnTeam == false ) || ( players[pnum].ulTeam != players[consoleplayer].ulTeam ))
					continue;

				break;
			} while ( pnum != consoleplayer );
		}
		// Deathmatch and co-op.
		else
		{
			// Loop through all the players, and stop when we find one that's on our team.
			while ( 1 )
			{
				pnum += step;
				pnum &= MAXPLAYERS-1;

				// Skip other spectators.
				if ( players[pnum].bSpectating )
					continue;

				if ( playeringame[pnum] )
					break;
			}
		}
	}

	// [BC] When we're all done, put the camera in the display player's body, etc.
	FinishChangeSpy( pnum );
}

// [BC] Split this out of ChangeSpy() so it can be called from within that function.
static void FinishChangeSpy( int pnum )
{
	players[consoleplayer].camera = players[pnum].mo;
	S_UpdateSounds(players[consoleplayer].camera);
	StatusBar->AttachToPlayer (&players[pnum]);

	// [TP] Rebuild translations if we're overriding player colors, they
	// may very likely have changed by now.
	if ( D_ShouldOverridePlayerColors() )
		D_UpdatePlayerColors( MAXPLAYERS );

	// [BC] We really no longer need to do this since we have a message
	// that says "FOLLOWING - xxx" on the status bar.
/*
	if (demoplayback || ( NETWORK_GetState( ) != NETSTATE_SINGLE ))
	{
		StatusBar->ShowPlayerName ();
	}
*/

	// [BC] If we're a client, tell the server that we're switching our displayplayer.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENTCOMMANDS_ChangeDisplayPlayer( pnum );

	// [BC] Also, refresh the HUD since the display player is changing.
	SCOREBOARD_RefreshHUD( );
}

CCMD (spynext)
{
	// [BB] The server can't use this.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		Printf ( "CCMD spynext can't be used on the server\n" );
		return;
	}

	// allow spy mode changes even during the demo
	ChangeSpy (SPY_NEXT);
}

CCMD (spyprev)
{
	// [BB] The server can't use this.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		Printf ( "CCMD spyprev can't be used on the server\n" );
		return;
	}

	// allow spy mode changes even during the demo
	ChangeSpy (SPY_PREV);
}

CCMD (spycancel)
{
	// allow spy mode changes even during the demo
	ChangeSpy (SPY_CANCEL);
}

// [TP]
enum { JOINMENUKEY_Space, JOINMENUKEY_Enter };
CVAR( Int, joinmenukey, JOINMENUKEY_Space, CVAR_ARCHIVE );

int G_GetJoinMenuKey()
{
	switch ( joinmenukey )
	{
	default:
	case JOINMENUKEY_Space:
		return KEY_SPACE;

	case JOINMENUKEY_Enter:
		return KEY_ENTER;
	}
}

const char* G_DescribeJoinMenuKey()
{
	const char* names[] = { "Space", "Enter" };
	if ( static_cast<unsigned>( joinmenukey ) < countof( names ))
		return names[joinmenukey];
	else
		return names[0];
}

//
// G_Responder
// Get info needed to make ticcmd_ts for the players.
//
bool G_Responder (event_t *ev)
{
	// any other key pops up menu if in demos
	// [RH] But only if the key isn't bound to a "special" command
	// [BB] We explicitly don't check if a client side demo is played to allow binding demo_pause, etc..
	if (gameaction == ga_nothing && 
		(demoplayback || gamestate == GS_DEMOSCREEN || gamestate == GS_TITLELEVEL))
	{
		const char *cmd = Bindings.GetBind (ev->data1);

		if (ev->type == EV_KeyDown)
		{

			if (!cmd || (
				strnicmp (cmd, "menu_", 5) &&
				stricmp (cmd, "toggleconsole") &&
				stricmp (cmd, "sizeup") &&
				stricmp (cmd, "sizedown") &&
				stricmp (cmd, "togglemap") &&
				stricmp (cmd, "spynext") &&
				stricmp (cmd, "spyprev") &&
				stricmp (cmd, "chase") &&
				stricmp (cmd, "+showscores") &&
				// [BC]
				stricmp (cmd, "+showmedals") &&
				stricmp (cmd, "bumpgamma") &&
				stricmp (cmd, "screenshot")))
			{
				M_StartControlPanel(true);
				M_SetMenu(NAME_Mainmenu, -1);
				return true;
			}
			else
			{
				return C_DoKey (ev, &Bindings, &DoubleBindings);
			}
		}
		if (cmd && cmd[0] == '+')
			return C_DoKey (ev, &Bindings, &DoubleBindings);

		return false;
	}

	// Handle chat input at the level and intermission screens.
	if ( gamestate == GS_LEVEL || gamestate == GS_INTERMISSION )
	{
		if ( CHAT_Input( ev )) 
			return ( true );

		// [RC] If the player hits the spacebar, and they aren't in the game, ask them if they'd like to join.
		// [BB] This "eats" the key, therefore we must return true here.
		if ( ( ev->type == EV_KeyDown ) && ( ev->data1 == G_GetJoinMenuKey() ) && players[consoleplayer].bSpectating )
		{
			C_DoCommand( "menu_join" );
			return true;
		}
	}

	if (gamestate == GS_LEVEL)
	{
		// Player function ate it.
		if ( PLAYER_Responder( ev ))
			return ( true );

		if (ST_Responder (ev))
			return true;		// status window ate it
		if (!viewactive && AM_Responder (ev, false))
			return true;		// automap ate it
	}
	else if (gamestate == GS_FINALE)
	{
		if (F_Responder (ev))
			return true;		// finale ate the event
	}

	switch (ev->type)
	{
	case EV_KeyDown:
		if (C_DoKey (ev, &Bindings, &DoubleBindings))
			return true;
		break;

	case EV_KeyUp:
		C_DoKey (ev, &Bindings, &DoubleBindings);
		break;

	// [RH] mouse buttons are sent as key up/down events
	case EV_Mouse: 
		mousex = (int)(ev->x * mouse_sensitivity);
		mousey = (int)(ev->y * mouse_sensitivity);
		break;
	}

	// [RH] If the view is active, give the automap a chance at
	// the events *last* so that any bound keys get precedence.

	if (gamestate == GS_LEVEL && viewactive)
		return AM_Responder (ev, true);

	return (ev->type == EV_KeyDown ||
			ev->type == EV_Mouse);
}


//
// G_Ticker
// Make ticcmd_ts for the players.
//
extern FTexture *Page;


void G_Ticker ()
{
	int i;
	gamestate_t	oldgamestate;
	int			buf;
	ticcmd_t*	cmd;
	LONG		lSize;

	// Client's don't spawn players until instructed by the server.
	if ( NETWORK_InClientMode() == false )
	{
		// do player reborns if needed
		for (i = 0; i < MAXPLAYERS; i++)
		{
			// [BC] PST_REBORNNOINVENTORY, PST_ENTERNOINVENTORY
			if (playeringame[i] &&
				(players[i].playerstate == PST_REBORN ||
				players[i].playerstate == PST_REBORNNOINVENTORY ||
				players[i].playerstate == PST_ENTER ||
				players[i].playerstate == PST_ENTERNOINVENTORY))
			{
				G_DoReborn (i, false);
			}
		}
	}
	// [BC] Tick the client and client statistics modules.
	else if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		CLIENT_Tick( );
		CLIENTSTATISTICS_Tick( );
	}

	if (ToggleFullscreen)
	{
		static char toggle_fullscreen[] = "toggle fullscreen";
		ToggleFullscreen = false;
		AddCommandString (toggle_fullscreen);
	}

	// do things to change the game state
	oldgamestate = gamestate;
	while (gameaction != ga_nothing)
	{
		if (gameaction == ga_newgame2)
		{
			gameaction = ga_newgame;
			break;
		}
		switch (gameaction)
		{
		case ga_loadlevel:
			G_DoLoadLevel (-1, false);
			break;
		case ga_newgame2:	// Silence GCC (see above)
		case ga_newgame:
			G_DoNewGame ();
			break;
		case ga_loadgame:
		case ga_loadgamehidecon:
		case ga_autoloadgame:
			G_DoLoadGame ();
			break;
		case ga_savegame:
			G_DoSaveGame (true, savegamefile, savedescription);
			gameaction = ga_nothing;
			savegamefile = "";
			savedescription[0] = '\0';
			break;
		case ga_autosave:
			G_DoAutoSave ();
			gameaction = ga_nothing;
			break;
		case ga_loadgameplaydemo:
			G_DoLoadGame ();
			// fallthrough
		case ga_playdemo:
			G_DoPlayDemo ();
			break;
		case ga_completed:
			G_DoCompleted ();
			break;
		case ga_slideshow:
			if (gamestate == GS_LEVEL) F_StartIntermission(level.info->slideshow, FSTATE_InLevel);
			break;
		case ga_worlddone:
			G_DoWorldDone ();
			break;
		case ga_screenshot:
			M_ScreenShot (shotfile);
			shotfile = "";
			gameaction = ga_nothing;
			break;
		case ga_fullconsole:
			C_FullConsole ();
			gameaction = ga_nothing;
			break;
		case ga_togglemap:
			AM_ToggleMap ();
			gameaction = ga_nothing;
			break;
		case ga_nothing:
			break;
		}
		C_AdjustBottom ();
	}

	if (oldgamestate != gamestate)
	{
		if (oldgamestate == GS_DEMOSCREEN && Page != NULL)
		{
			Page->Unload();
			Page = NULL;
		}
		else if (oldgamestate == GS_FINALE)
		{
			F_EndFinale ();
		}
	}

	// get commands, check consistancy, and build new consistancy check
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		buf = gametic % CLIENT_PREDICTION_TICS;
	else
		buf = (gametic/ticdup)%BACKUPTICS;

	// If we're the client, attempt to read in packets from the server.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		cmd = &players[consoleplayer].cmd;
//		memcpy( cmd, &netcmds[consoleplayer][buf], sizeof( ticcmd_t ));
		memcpy( cmd, &netcmds[0][buf], sizeof( ticcmd_t ));
	}

	// [BB] If we are playing a demo in free spectate mode, hand the player's ticcmd to the free spectator player.
	if ( CLIENTDEMO_IsInFreeSpectateMode() )
		CLIENTDEMO_SetFreeSpectatorTiccmd ( &netcmds[0][buf] );

	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		// [RC] Refresh the Skulltag's G15 applet.
		G15_Tick( );

		CLIENT_GetPackets( );

		while (( lSize = NETWORK_GetLANPackets( )) > 0 )
		{
			LONG			lCommand;
			BYTESTREAM_s	*pByteStream;

			pByteStream = &NETWORK_GetNetworkMessageBuffer( )->ByteStream;

			lCommand = NETWORK_ReadLong( pByteStream );
			if ( lCommand == SERVER_LAUNCHER_CHALLENGE )
				BROWSER_ParseServerQuery( pByteStream, true );
		}

		// Now that we're done parsing the multiple packets the server has sent our way, check
		// to see if any packets are missing.
		if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) && ( CLIENT_GetConnectionState( ) >= CTS_ATTEMPTINGAUTHENTICATION ))
			CLIENT_CheckForMissingPackets( );
	}

	// If we're playing back a demo, read packets and ticcmds now.
	if ( CLIENTDEMO_IsPlaying( ) )
	{
		// [BB] .. only if the demo is not currently paused.
		if ( CLIENTDEMO_IsPaused( ) == false )
			CLIENTDEMO_ReadPacket( );
		// [BB] If the demo is paused, the tic offset increases.
		else
			CLIENTDEMO_SetGameticOffset ( CLIENTDEMO_GetGameticOffset() + 1 );
	}

	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		// Check if either us or the server is lagging.
		CLIENT_WaitForServer( );

		// If all is good, send our commands.
		if ( CLIENT_GetServerLagging( ) == false )
			CLIENT_SendCmd( );

		// If we're recording a demo, write the player's commands.
		if ( CLIENTDEMO_IsRecording( ))
			CLIENTDEMO_WriteTiccmd( &players[consoleplayer].cmd );
	}

	if (( NETWORK_InClientMode() == false ) &&
		( NETWORK_GetState( ) != NETSTATE_SERVER ))
	{
		// [RH] Include some random seeds and player stuff in the consistancy
		// check, not just the player's x position like BOOM.
		DWORD rngsum = FRandom::StaticSumSeeds ();

		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (playeringame[i])
			{
				ticcmd_t *cmd = &players[i].cmd;
				ticcmd_t *newcmd = &netcmds[i][buf];

				if ((gametic % ticdup) == 0)
				{
					RunNetSpecs (i, buf);
				}
				if (demorecording)
				{
					G_WriteDemoTiccmd (newcmd, i, buf);
				}
				players[i].oldbuttons = cmd->ucmd.buttons;
				// If the user alt-tabbed away, paused gets set to -1. In this case,
				// we do not want to read more demo commands until paused is no
				// longer negative.
				if (demoplayback && paused >= 0)
				{
					G_ReadDemoTiccmd (cmd, i);
				}
				else
				{
					memcpy (cmd, newcmd, sizeof(ticcmd_t));
				}

				// check for turbo cheats
				if (cmd->ucmd.forwardmove > TURBOTHRESHOLD &&
					!(gametic&31) && ((gametic>>5)&(MAXPLAYERS-1)) == i )
				{
					Printf ("%s is turbo!\n", players[i].userinfo.GetName());
				}
			}
		}
	}

	// [BB] Since this is now done in the block above that the server doesn't enter, we need to do it here:
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		for ( i = 0; i < MAXPLAYERS; i++ )
		{
			if ( playeringame[i] )
				players[i].oldbuttons = players[i].cmd.ucmd.buttons;
		}
	}

	// Check if the players are moving faster than they should.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( sv_cheats == false ))
	{
		for ( i = 0; i < MAXPLAYERS; i++ )
		{
			if ( playeringame[i] )
			{
				LONG		lMaxThreshold;
				ticcmd_t	*cmd = &players[i].cmd;

				lMaxThreshold = TURBOTHRESHOLD;
				if ( players[i].cheats & CF_SPEED25 )
					lMaxThreshold *= 2;

				// [BB] In case the speed of the player is increased by 50 percent, lMaxThreshold is multiplied by 4,
				// exactly like it was done with CF_SPEED before.
				if ( (players[i].mo) && (players[i].mo->Inventory) && (players[i].mo->Inventory->GetSpeedFactor() > FRACUNIT) )
				{
					float floatSpeedFactor = static_cast<float>(players[i].mo->Inventory->GetSpeedFactor())/static_cast<float>(FRACUNIT);
					lMaxThreshold = static_cast<LONG> ( lMaxThreshold * (6.*floatSpeedFactor-5.) );
				}

				// Check for turbo cheats.
				if (( cmd->ucmd.forwardmove > lMaxThreshold ) ||
					( cmd->ucmd.forwardmove < -lMaxThreshold ) ||
					( cmd->ucmd.sidemove > lMaxThreshold ) ||
					( cmd->ucmd.sidemove < -lMaxThreshold ))
				{
					// If the player was moving faster than he was allowed to, kick him!
					SERVER_KickPlayer( i, "Speed exceeds that allowed (turbo cheat)." );
				}
			}
		}
	}

	// do main actions
	switch (gamestate)
	{
	case GS_LEVEL:

		// [BC] Tick these modules, but only if the ticker should run.
		if (( paused == false ) &&
			( P_CheckTickerPaused( ) == false ))
		{
			// Update some general bot stuff.
			BOTS_Tick( );

			// Tick the duel module.
			DUEL_Tick( );

			// Tick the LMS module.
			LASTMANSTANDING_Tick( );

			// Tick the possession module.
			POSSESSION_Tick( );

			// Tick the survival module.
			SURVIVAL_Tick( );

			// Tick the invasion module.
			INVASION_Tick( );

			// Tick the domination module.
			DOMINATION_Tick( );

			// [BB]
			GAMEMODE_Tick( );

			// Reset the bot cycles counter before we tick their logic.
			BOTS_ResetCyclesCounter( );

			// Tick the callvote module.
			CALLVOTE_Tick( );

			// Tick the chat module.
			CHAT_Tick( );

			// [BB] Possibly award points for the damage players dealt.
			// Is there a better place to put this?
			PLAYER_AwardDamagePointsForAllPlayers( );

			// [BB] If we are supposed to reset the map, do that now.
			if ( GAME_IsMapRestRequested() )
			{
				// [BB] Tell the clients to do their part of the map reset and do so
				// before doing the map reset on the server.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_ResetMap();
				else if ( NETWORK_InClientMode() == false )
				{
					// [EP] Clear all the HUD messages.
					if ( StatusBar )
						StatusBar->DetachAllMessages();	
				}

				GAME_ResetMap( );
				GAMEMODE_RespawnAllPlayers ( );
			}

		}

		// [BB] Tick the unlagged module.
		UNLAGGED_Tick( );

		// [BB] Don't call P_Ticker on the server if there are no players.
		// This significantly reduces CPU usage on maps with many monsters
		// (of course only as long as there are no connected clients).
		if ( ( NETWORK_GetState( ) != NETSTATE_SERVER ) || ( SERVER_CalcNumConnectedClients() > 0 ) )
			P_Ticker ();
		AM_Ticker ();

		// Tick the medal system.
		MEDAL_Tick( );

		// Play "Welcome" sounds for teamgame modes.
		if ( g_ulLevelIntroTicks < TICRATE )
		{
			g_ulLevelIntroTicks++;
			if ( g_ulLevelIntroTicks == TICRATE )
			{
				if ( oneflagctf )
					ANNOUNCER_PlayEntry( cl_announcer, "WelcomeToOneFlagCTF" );
				else if ( ctf )
					ANNOUNCER_PlayEntry( cl_announcer, "WelcomeToCTF" );
				else if ( skulltag )
					ANNOUNCER_PlayEntry( cl_announcer, "WelcomeToST" );
			}
		}

		// Apply end level delay.
		if (( g_ulEndLevelDelay ) &&
			( NETWORK_InClientMode() == false ))
		{
			if ( --g_ulEndLevelDelay == 0 )
			{
				// Tell the clients about the expired end level delay.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SetGameEndLevelDelay( g_ulEndLevelDelay );

				// If we're in a duel, set up the next duel.
				if ( duel )
				{
					// If the player must win all duels, and lost this one, then he's DONE!
					if (( DUEL_GetLoser( ) == static_cast<unsigned> (consoleplayer) ) && ( CAMPAIGN_InCampaign( )) && ( CAMPAIGN_GetCampaignInfo( level.mapname )->bMustWinAllDuels ))
					{
						// Tell the player he loses!
						Printf( "You lose!\n" );

						// End the level.
						G_ExitLevel( 0, false );

						// When the level loads, start the next duel.
						DUEL_SetStartNextDuelOnLevelLoad( true );
					}
					// If we've reached the duel limit, exit the level.
					else if (( duellimit > 0 ) && ( static_cast<signed> (DUEL_GetNumDuels( )) >= duellimit ))
					{
						NETWORK_Printf( "Duellimit hit.\n" );
						G_ExitLevel( 0, false );

						// When the level loads, start the next duel.
						DUEL_SetStartNextDuelOnLevelLoad( true );
					}
					else
					{
						// Send the loser back to the spectators! Doing so will automatically set up
						// the next duel.
						DUEL_SendLoserToSpectators( );
					}
				}
				else if ( lastmanstanding || teamlms )
				{
					bool	bLimitHit = false;

					if ( winlimit > 0 )
					{
						if ( lastmanstanding )
						{
							ULONG	ulIdx;
							
							for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
							{
								if (( playeringame[ulIdx] == false ) || ( PLAYER_IsTrueSpectator( &players[ulIdx] )))
									continue;

								if ( static_cast<signed> (players[ulIdx].ulWins) >= winlimit )
								{
									bLimitHit = true;
									break;
								}
							}
						}
						else
						{
							if ( TEAM_GetHighestWinCount( ) >= winlimit)
								bLimitHit = true;
						}

						if ( bLimitHit )
						{
							NETWORK_Printf( "Winlimit hit.\n" );
							G_ExitLevel( 0, false );

							// When the level loads, start the next match.
//							LASTMANSTANDING_SetStartNextMatchOnLevelLoad( true );
						}
						else
						{
							LASTMANSTANDING_SetState( LMSS_WAITINGFORPLAYERS );
							LASTMANSTANDING_Tick( );
						}
					}
					else
					{
						LASTMANSTANDING_SetState( LMSS_WAITINGFORPLAYERS );
						LASTMANSTANDING_Tick( );
					}
				}
				else if ( possession || teampossession )
				{
					bool	bLimitHit = false;

					if ( pointlimit > 0 )
					{
						if ( possession )
						{
							ULONG	ulIdx;
							
							for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
							{
								if (( playeringame[ulIdx] == false ) || ( PLAYER_IsTrueSpectator( &players[ulIdx] )))
									continue;

								if ( players[ulIdx].lPointCount >= pointlimit )
								{
									bLimitHit = true;
									break;
								}
							}
						}
						else
						{
							if ( TEAM_GetHighestScoreCount( ) >= pointlimit)
								bLimitHit = true;
						}

						if ( bLimitHit )
						{
							G_ExitLevel( 0, false );
						}
						else
						{
							POSSESSION_SetState( PSNS_PRENEXTROUNDCOUNTDOWN );
							POSSESSION_Tick( );
						}
					}
					else
					{
						POSSESSION_SetState( PSNS_PRENEXTROUNDCOUNTDOWN );
						POSSESSION_Tick( );
					}
				}
				else if ( survival )
				{
					SURVIVAL_RestartMission( );
					SURVIVAL_Tick( );
				}
				else if ( invasion )
				{
					INVASION_SetState( IS_WAITINGFORPLAYERS );
					INVASION_Tick( );
				}
				else
					G_ExitLevel( 0, false );
			}
		}

		break;

	case GS_TITLELEVEL:
		P_Ticker ();
		break;

	case GS_INTERMISSION:

		// Make sure bots are ticked during intermission.
		{
			ULONG	ulIdx;

			// Reset the bot cycles counter before we tick their logic.
			BOTS_ResetCyclesCounter( );

			for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
			{
				if ( playeringame[ulIdx] == false )
					continue;

				if ( players[ulIdx].pSkullBot )
					players[ulIdx].pSkullBot->Tick( );
			}
		}

		// [BB] Going to intermission automatically stops any active vote.
		if ( CALLVOTE_GetVoteState() == VOTESTATE_INVOTE )
			CALLVOTE_ClearVote();

		WI_Ticker ();
		break;

	case GS_FINALE:
		F_Ticker ();
		break;

	case GS_DEMOSCREEN:
		D_PageTicker ();
		break;

	case GS_STARTUP:
		if (gameaction == ga_nothing)
		{
			gamestate = GS_FULLCONSOLE;
			gameaction = ga_fullconsole;
		}
		break;

	default:
		break;
	}

	// [BC] If any data has accumulated in our packet, send it out now.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENT_EndTick( );
}


//
// PLAYER STRUCTURE FUNCTIONS
// also see P_SpawnPlayer in P_Mobj
//

//
// G_PlayerFinishLevel
// Called when a player completes a level.
//
// flags is checked for RESETINVENTORY and RESETHEALTH only.

void G_PlayerFinishLevel (int player, EFinishLevelType mode, int flags)
{
	AInventory *item, *next;
	player_t *p;

	p = &players[player];

	if (p->morphTics != 0)
	{ // Undo morph
		P_UndoPlayerMorph (p, p, 0, true);
	}

	// [BB] Under some circumstances a client may come here with p->mo == NULL.
	if ( (NETWORK_GetState() == NETSTATE_CLIENT) && (p->mo == NULL) )
		return;

	// Strip all current powers, unless moving in a hub and the power is okay to keep.
	item = p->mo->Inventory;
	while (item != NULL)
	{
		next = item->Inventory;
		if (item->IsKindOf (RUNTIME_CLASS(APowerup)))
		{
			if (deathmatch || ((mode != FINISH_SameHub || !(item->ItemFlags & IF_HUBPOWER))
				&& !(item->ItemFlags & IF_PERSISTENTPOWER))) // Keep persistent powers in non-deathmatch games
			{
				item->Destroy ();
			}
		}
		item = next;
	}
	if (p->ReadyWeapon != NULL &&
		p->ReadyWeapon->WeaponFlags&WIF_POWERED_UP &&
		p->PendingWeapon == p->ReadyWeapon->SisterWeapon)
	{
		// Unselect powered up weapons if the unpowered counterpart is pending
		p->ReadyWeapon=p->PendingWeapon;
	}
	// reset invisibility to default
	if (p->mo->GetDefault()->flags & MF_SHADOW)
	{
		p->mo->flags |= MF_SHADOW;
	}
	else
	{
		p->mo->flags &= ~MF_SHADOW;
	}
	p->mo->RenderStyle = p->mo->GetDefault()->RenderStyle;
	p->mo->alpha = p->mo->GetDefault()->alpha;
	p->extralight = 0;					// cancel gun flashes
	p->fixedcolormap = NOFIXEDCOLORMAP;	// cancel ir goggles
	p->fixedlightlevel = -1;
	p->damagecount = 0; 				// no palette changes
	p->bonuscount = 0;
	p->poisoncount = 0;
	p->inventorytics = 0;

	if (mode != FINISH_SameHub)
	{
		// Take away flight and keys (and anything else with IF_INTERHUBSTRIP set)
		item = p->mo->Inventory;
		while (item != NULL)
		{
			next = item->Inventory;
			if (item->InterHubAmount < 1)
			{
				item->Destroy ();
			}
			item = next;
		}
	}

	if (mode == FINISH_NoHub && !(level.flags2 & LEVEL2_KEEPFULLINVENTORY))
	{ // Reduce all owned (visible) inventory to defined maximum interhub amount
		for (item = p->mo->Inventory; item != NULL; item = item->Inventory)
		{
			// If the player is carrying more samples of an item than allowed, reduce amount accordingly
			if (item->ItemFlags & IF_INVBAR && item->Amount > item->InterHubAmount)
			{
				item->Amount = item->InterHubAmount;
			}
		}
	}

	// Resets player health to default if not dead.
	if ((flags & CHANGELEVEL_RESETHEALTH) && p->playerstate != PST_DEAD)
	{
		p->health = p->mo->health = p->mo->SpawnHealth();
	}

	// [BC] Reset a bunch of other Skulltag stuff.
	PLAYER_ResetSpecialCounters ( p );
	if ( p->pIcon )
	{
		p->pIcon->Destroy( );
		p->pIcon = NULL;
	}

	// Clears the entire inventory and gives back the defaults for starting a game
	if (flags & CHANGELEVEL_RESETINVENTORY)
	{
		p->mo->ClearInventory();
		p->mo->GiveDefaultInventory();
	}
}

//
// G_PlayerReborn
// Called after a player dies
// almost everything is cleared and initialized
//
// [BB] Added bGiveInventory.
void G_PlayerReborn (int player, bool bGiveInventory)
{
	player_t*	p;
	int			fragcount;	// [RH] Cumulative frags
	int 		killcount;
	int 		itemcount;
	int 		secretcount;
	int			chasecam;
	BYTE		currclass;
	userinfo_t  userinfo;	// [RH] Save userinfo
	// [BB]
	//botskill_t  b_skill;	//Added by MC:
	APlayerPawn *actor;
	const PClass *cls;
	FString		log;
	bool		bOnTeam;
	bool		bSpectating;
	bool		bDeadSpectator;
	ULONG		ulLivesLeft;
	ULONG		ulTeam;
	LONG		lPointCount;
	ULONG		ulDeathCount;
	ULONG		ulConsecutiveHits;
	ULONG		ulConsecutiveRailgunHits;
	ULONG		ulDeathsWithoutFrag;
	ULONG		ulUnrewardedDamageDealt;
	ULONG		ulMedalCount[NUM_MEDALS];
	CSkullBot	*pSkullBot;
	bool		bIgnoreChat;
	LONG		lIgnoreChatTicks;
	ULONG		ulPing;
	ULONG		ulPingAverages;
	ULONG		ulWins;
	ULONG		ulTime;
	int			timefreezer;
	FName		StartingWeaponName;

	p = &players[player];

	fragcount = p->fragcount;
	killcount = p->killcount;
	itemcount = p->itemcount;
	secretcount = p->secretcount;
	currclass = p->CurrentPlayerClass;
	userinfo.TransferFrom(p->userinfo);
	actor = p->mo;
	cls = p->cls;
	log = p->LogText;
	chasecam = p->cheats & CF_CHASECAM;

	bOnTeam = p->bOnTeam;
	const bool bChatting = p->bChatting;
	const bool bInConsole = p->bInConsole;
	bSpectating = p->bSpectating;
	bDeadSpectator = p->bDeadSpectator;
	ulLivesLeft = p->ulLivesLeft;
	ulTeam = p->ulTeam;
	lPointCount = p->lPointCount;
	ulDeathCount = p->ulDeathCount;
	ulConsecutiveHits = p->ulConsecutiveHits;
	ulConsecutiveRailgunHits = p->ulConsecutiveRailgunHits;
	ulDeathsWithoutFrag = p->ulDeathsWithoutFrag;
	ulUnrewardedDamageDealt = p->ulUnrewardedDamageDealt;
	memcpy( &ulMedalCount, &p->ulMedalCount, sizeof( ulMedalCount ));
	pSkullBot = p->pSkullBot;
	bIgnoreChat = p->bIgnoreChat;
	lIgnoreChatTicks = p->lIgnoreChatTicks;
	ulPing = p->ulPing;
	ulPingAverages = p->ulPingAverages;
	ulWins = p->ulWins;
	ulTime = p->ulTime;
	timefreezer = p->timefreezer;
	StartingWeaponName = p->StartingWeaponName;
	const bool bLagging = p->bLagging;

	// Reset player structure to its defaults
	p->~player_t();
	::new(p) player_t;

	p->fragcount = fragcount;
	p->killcount = killcount;
	p->itemcount = itemcount;
	p->secretcount = secretcount;
	p->CurrentPlayerClass = currclass;
	p->userinfo.TransferFrom(userinfo);
	p->mo = actor;
	p->cls = cls;
	p->LogText = log;
	p->cheats |= chasecam;

	p->oldbuttons = ~0, p->attackdown = true; p->usedown = true;	// don't do anything immediately
	p->original_oldbuttons = ~0;

	p->bOnTeam = bOnTeam;
	p->bChatting = bChatting;
	p->bInConsole = bInConsole;
	p->bSpectating = bSpectating;
	p->bDeadSpectator = bDeadSpectator;
	p->ulLivesLeft = ulLivesLeft;
	p->ulTeam = ulTeam;
	p->lPointCount = lPointCount;
	p->ulDeathCount = ulDeathCount;
	p->ulConsecutiveHits = ulConsecutiveHits;
	p->ulConsecutiveRailgunHits = ulConsecutiveRailgunHits;
	p->ulDeathsWithoutFrag = ulDeathsWithoutFrag;
	p->ulUnrewardedDamageDealt = ulUnrewardedDamageDealt;
	memcpy( &p->ulMedalCount, &ulMedalCount, sizeof( ulMedalCount ));
	p->pSkullBot = pSkullBot;
	p->bIgnoreChat = bIgnoreChat;
	p->lIgnoreChatTicks = lIgnoreChatTicks;
	p->ulPing = ulPing;
	p->ulPingAverages = ulPingAverages;
	p->ulWins = ulWins;
	p->ulTime = ulTime;
	// [BB] Players who were able to move while a APowerTimeFreezer is active,
	// should also be able to do so after being reborn.
	p->timefreezer = timefreezer;
	p->StartingWeaponName = StartingWeaponName;
	p->bLagging = bLagging;
	p->bIsBot = p->pSkullBot ? true : false;

	p->playerstate = PST_LIVE;

	if (gamestate != GS_TITLELEVEL)
	{
		// [BB] Added bGiveInventory.
		if ( bGiveInventory )
			actor->GiveDefaultInventory ();
		// [BB] Even if we don't give the inventory, we need to give the player the default health.
		// Otherwise we get a zombie player with 0 health (at least on the clients).
		else if ( actor->player )
			actor->player->health = actor->GetDefault ()->health;

		p->ReadyWeapon = p->PendingWeapon;
	}
}

//
// G_CheckSpot	
// Returns false if the player cannot be respawned
// at the given mapthing spot  
// because something is occupying it 
//

bool G_CheckSpot (int playernum, FPlayerStart *mthing)
{
	fixed_t x;
	fixed_t y;
	fixed_t z, oldz;
	int i;

	x = mthing->x;
	y = mthing->y;
	z = mthing->z;

	if (!(level.flags & LEVEL_USEPLAYERSTARTZ))
	{
		z = 0;
	}

	z += P_PointInSector (x, y)->floorplane.ZatPoint (x, y);

	if (!players[playernum].mo)
	{ // first spawn of level, before corpses
		for (i = 0; i < playernum; i++)
			if (players[i].mo && players[i].mo->x == x && players[i].mo->y == y)
				return false;
		return true;
	}

	oldz = players[playernum].mo->z;	// [RH] Need to save corpse's z-height
	players[playernum].mo->z = z;		// [RH] Checks are now full 3-D

	// killough 4/2/98: fix bug where P_CheckPosition() uses a non-solid
	// corpse to detect collisions with other players in DM starts
	//
	// Old code:
	// if (!P_CheckPosition (players[playernum].mo, x, y))
	//    return false;

	// [EP] Spectator flags must be disabled for the position checking, too.
	DWORD oldflags2 = players[playernum].mo->flags2;
	if ( players[playernum].mo->ulSTFlags & STFL_OBSOLETE_SPECTATOR_BODY )
		players[playernum].mo->flags2 &= ~MF2_THRUACTORS;

	players[playernum].mo->flags |=  MF_SOLID;
	i = P_CheckPosition(players[playernum].mo, x, y);
	players[playernum].mo->flags &= ~MF_SOLID;
	players[playernum].mo->z = oldz;	// [RH] Restore corpse's height

	// [EP] Restore spectator flags.
	players[playernum].mo->flags2 = oldflags2;

	if (!i)
		return false;

	return true;
}


//
// G_DeathMatchSpawnPlayer 
// Spawns a player at one of the random death match spots 
// called at level load and each death 
//

// [RH] Returns the distance of the closest player to the given mapthing
static fixed_t PlayersRangeFromSpot (FPlayerStart *spot)
{
	fixed_t closest = INT_MAX;
	fixed_t distance;
	int i;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || !players[i].mo || players[i].health <= 0)
			continue;

		distance = P_AproxDistance (players[i].mo->x - spot->x,
									players[i].mo->y - spot->y);

		if (distance < closest)
			closest = distance;
	}

	return closest;
}

// Returns the average distance this spot is from all the enemies of ulPlayer.
static fixed_t TeamLMSPlayersRangeFromSpot( ULONG ulPlayer, FPlayerStart *spot )
{
	ULONG	ulNumSpots;
	fixed_t	distance = INT_MAX;
	int i;

	ulNumSpots = 0;
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || !players[i].mo || players[i].health <= 0)
			continue;

		// Ignore players on our team.
		if (( players[ulPlayer].bOnTeam ) && ( players[i].bOnTeam ) && ( players[ulPlayer].ulTeam == players[i].ulTeam ))
			continue;

		ulNumSpots++;
		distance += P_AproxDistance (players[i].mo->x - spot->x,
									players[i].mo->y - spot->y);
	}

	if ( ulNumSpots )
		return ( distance / ulNumSpots );
	else
		return ( distance );
}

// [RH] Select the deathmatch spawn spot farthest from everyone.
static FPlayerStart *SelectFarthestDeathmatchSpot( ULONG ulPlayer, size_t selections )
{
	fixed_t bestdistance = 0;
	FPlayerStart *bestspot = NULL;
	unsigned int i;

	for (i = 0; i < selections; i++)
	{
		fixed_t distance = PlayersRangeFromSpot (&deathmatchstarts[i]);

		// Did not find a spot.
		if ( distance == INT_MAX )
			continue;

		if ( G_CheckSpot( ulPlayer, &deathmatchstarts[i] ) == false )
			continue;

		if (distance > bestdistance)
		{
			bestdistance = distance;
			bestspot = &deathmatchstarts[i];
		}
	}

	return bestspot;
}


// Try to find a deathmatch spawn spot farthest from our enemies.
static FPlayerStart *SelectBestTeamLMSSpot( ULONG ulPlayer, size_t selections )
{
	ULONG		ulIdx;
	fixed_t		Distance;
	fixed_t		BestDistance;
	FPlayerStart	*pBestSpot;

	pBestSpot = NULL;
	BestDistance = 0;
	for ( ulIdx = 0; ulIdx < selections; ulIdx++ )
	{
		Distance = TeamLMSPlayersRangeFromSpot( ulPlayer, &deathmatchstarts[ulIdx] );

		// Did not find a spot.
		if ( Distance == INT_MAX )
			continue;

		if ( G_CheckSpot( ulPlayer, &deathmatchstarts[ulIdx] ) == false )
			continue;

		if ( Distance > BestDistance )
		{
			BestDistance = Distance;
			pBestSpot = &deathmatchstarts[ulIdx];
		}
	}

	return ( pBestSpot );
}

// [RH] Select a deathmatch spawn spot at random (original mechanism)
static FPlayerStart *SelectRandomDeathmatchSpot (int playernum, unsigned int selections)
{
	unsigned int i, j;

	for (j = 0; j < 20; j++)
	{
		i = pr_dmspawn() % selections;
		if (G_CheckSpot (playernum, &deathmatchstarts[i]) )
		{
			return &deathmatchstarts[i];
		}
	}

	// [RH] return a spot anyway, since we allow telefragging when a player spawns
	return &deathmatchstarts[i];
}

// Select a temporary team spawn spot at random.
static FPlayerStart *SelectTemporaryTeamSpot( USHORT usPlayer, ULONG ulNumSelections )
{
	ULONG	ulNumAttempts;
	ULONG	ulSelection;

	// Try up to 20 times to find a valid spot.
	for ( ulNumAttempts = 0; ulNumAttempts < 20; ulNumAttempts++ )
	{
		ulSelection = ( pr_dmspawn( ) % ulNumSelections );
		if ( G_CheckSpot( usPlayer, &TemporaryTeamStarts[ulSelection] ))
			return ( &TemporaryTeamStarts[ulSelection] );
	}

	// Return a spot anyway, since we allow telefragging when a player spawns.
	return ( &TemporaryTeamStarts[ulSelection] );
}

// Select a team spawn spot at random.
static FPlayerStart *SelectRandomTeamSpot( USHORT usPlayer, ULONG ulTeam, ULONG ulNumSelections )
{
	ULONG	ulNumAttempts;
	ULONG	ulSelection;

	// Try up to 20 times to find a valid spot.
	for ( ulNumAttempts = 0; ulNumAttempts < 20; ulNumAttempts++ )
	{
		ulSelection = ( pr_dmspawn( ) % ulNumSelections );
		if ( G_CheckSpot( usPlayer, &teams[ulTeam].TeamStarts[ulSelection] ))
			return ( &teams[ulTeam].TeamStarts[ulSelection] );
	}

	// Return a spot anyway, since we allow telefragging when a player spawns.
	return ( &teams[ulTeam].TeamStarts[ulSelection] );
}

// Select a cooperative spawn spot at random.
FPlayerStart *SelectRandomCooperativeSpot( ULONG ulPlayer )
{
	ULONG		ulNumAttempts;
	ULONG		ulSelection;
	ULONG		ulIdx;

	// [BB] Count the number of available player starts.
	ULONG ulNumSelections = 0;
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( playerstarts[ulIdx].type != 0 )
			ulNumSelections++;
	}

	if ( ulNumSelections < 1 )
		I_Error( "No cooperative starts!" );

	// Try up to 20 times to find a valid spot.
	for ( ulNumAttempts = 0; ulNumAttempts < 20; ulNumAttempts++ )
	{
		// Find the first valid playerstart.
		ulIdx = 0;
		while (( ulIdx < MAXPLAYERS ) && ( playerstarts[ulIdx].type == 0 ))
			ulIdx++;

		ulSelection = ( pr_dmspawn( ) % ulNumSelections );
		while ( ulSelection > 0 )
		{
			ulSelection--;
			// [BB] Find the next valid playerstart (assuming that ulNumSelections gives us the number of available starts).
			ulIdx++;
			while (( ulIdx < MAXPLAYERS ) && ( playerstarts[ulIdx].type == 0 ))
				ulIdx++;
		}

		if ( ( ulIdx < MAXPLAYERS ) && G_CheckSpot( ulPlayer, &playerstarts[ulIdx] ))
			return ( &playerstarts[ulIdx] );
	}

	// Return a spot anyway, since we allow telefragging when a player spawns.
	if ( ulIdx < MAXPLAYERS )
		return ( &playerstarts[ulIdx] );
	else
		return NULL;
}

void G_DeathMatchSpawnPlayer( int playernum, bool bClientUpdate )
{
	unsigned int selections;
	FPlayerStart *spot;

	// [BB] If sv_useteamstartsindm is true, we want to use team starts in deathmatch
	// game modes with teams, e.g. TDM, TLMS.
	if ( ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
		&& players[playernum].bOnTeam
		&& TEAM_CheckIfValid ( players[playernum].ulTeam )
		&& ( teams[players[playernum].ulTeam].TeamStarts.Size( ) >= 1 )
		&& sv_useteamstartsindm )
	{
		G_TeamgameSpawnPlayer( playernum, players[playernum].ulTeam, bClientUpdate );
		return;
	}

	selections = deathmatchstarts.Size ();
	// [RH] We can get by with just 1 deathmatch start
	if (selections < 1)
		I_Error( "No deathmatch starts!" );

	if ( teamlms && ( players[playernum].bOnTeam ))
	{
		// If we didn't find a valid spot, just pick one at random.
		if (( spot = SelectBestTeamLMSSpot( playernum, selections )) == NULL )
			spot = SelectRandomDeathmatchSpot( playernum, selections );
	}
	else if ( dmflags & DF_SPAWN_FARTHEST )
	{
		// If we didn't find a valid spot, just pick one at random.
		if (( spot = SelectFarthestDeathmatchSpot( playernum, selections )) == NULL )
			spot = SelectRandomDeathmatchSpot( playernum, selections );
	}
	else
		spot = SelectRandomDeathmatchSpot (playernum, selections);

	if ( spot == NULL )
		I_Error( "Could not find a valid deathmatch spot! (this should not happen)" );

	AActor *mo = P_SpawnPlayer( spot, playernum, bClientUpdate ? SPF_CLIENTUPDATE : 0 );
	if (mo != NULL) P_PlayerStartStomp(mo);
}

void G_TemporaryTeamSpawnPlayer( ULONG ulPlayer, bool bClientUpdate )
{
	ULONG		ulNumSelections;
	FPlayerStart	*pSpot;

	ulNumSelections = TemporaryTeamStarts.Size( );

	// If there aren't any temporary starts, just spawn them at a random team location.
	if ( ulNumSelections < 1 )
	{
		bool	bCanUseStarts[MAX_TEAMS];
		LONG	lAllowedTeamCount = 0;
		ULONG	ulTeam = 0;
		ULONG	ulOnTeamNum = 0;

		// Set each of these to specific values.
		for ( ULONG i = 0; i < MAX_TEAMS; i++ )
			bCanUseStarts[i] = false;

		for ( ULONG i = 0; i < TEAM_GetNumAvailableTeams( ); i++ )
		{
			if ( teams[i].TeamStarts.Size( ) == 0 )
				continue;

			bCanUseStarts[i] = true;
			lAllowedTeamCount++;
		}

		if ( lAllowedTeamCount > 0 )
		{
			ulTeam = M_Random( ) % lAllowedTeamCount;
		}
		else
		{
			I_Error( "No teamgame starts!" );
		}

		for ( ULONG i = 0; i < TEAM_GetNumAvailableTeams( ); i++ )
		{
			if ( bCanUseStarts[i] == false )
				continue;

			if ( ulOnTeamNum == ulTeam )
				ulTeam = i;

			ulOnTeamNum++;
		}

		G_TeamgameSpawnPlayer( ulPlayer, ulTeam, bClientUpdate );
		return;
	}

	// SelectTemporaryTeamSpot should always return a valid spot. If not, we have a problem.
	pSpot = SelectTemporaryTeamSpot( static_cast<USHORT> ( ulPlayer ), ulNumSelections );

	// ANAMOLOUS HAPPENING!!!
	if ( pSpot == NULL )
		I_Error( "Could not find a valid temporary spot! (this should not happen)" );

	AActor *mo = P_SpawnPlayer( pSpot, ulPlayer, bClientUpdate ? SPF_CLIENTUPDATE : 0 );
	if (mo != NULL)
	{
		P_PlayerStartStomp(mo);
		// [BL] Say goodbye to selection room pistol-fights! I'm fed up with them!
		// [BB] Spectators have to be excluded from this (they don't have any inventory anyway).
		if ( players[ulPlayer].bSpectating == false )
		{
			players[ulPlayer].bUnarmed = true;
			players[ulPlayer].mo->ClearInventory();
		}
	}
}

void G_TeamgameSpawnPlayer( ULONG ulPlayer, ULONG ulTeam, bool bClientUpdate )
{
	ULONG		ulNumSelections;
	FPlayerStart	*pSpot;

	ulNumSelections = teams[ulTeam].TeamStarts.Size( );
	if ( ulNumSelections < 1 )
		I_Error( "No %s team starts!", TEAM_GetName( ulTeam ));

	// SelectRandomTeamSpot should always return a valid spot. If not, we have a problem.
	pSpot = SelectRandomTeamSpot( static_cast<USHORT> ( ulPlayer ), ulTeam, ulNumSelections );

	// ANAMOLOUS HAPPENING!!!
	if ( pSpot == NULL )
		I_Error( "Could not find a valid temporary spot! (this should not happen)" );

	AActor *mo = P_SpawnPlayer( pSpot, ulPlayer, bClientUpdate ? SPF_CLIENTUPDATE : 0 );
	if (mo != NULL) P_PlayerStartStomp(mo);
}

void G_CooperativeSpawnPlayer( ULONG ulPlayer, bool bClientUpdate, bool bTempPlayer )
{
	// If there's a valid start for this player, spawn him there.
	// [BB] Don't do this, if we want to randomize starts.
	if (( sv_randomcoopstarts == false ) && ( playerstarts[ulPlayer].type != 0 ) && ( G_CheckSpot( ulPlayer, &playerstarts[ulPlayer] )))
	{
		AActor *mo = P_SpawnPlayer( &playerstarts[ulPlayer], ulPlayer, ( bTempPlayer ? SPF_TEMPPLAYER : 0 ) | ( bClientUpdate ? SPF_CLIENTUPDATE : 0 ) );
		if (mo != NULL) P_PlayerStartStomp(mo);
		return;
	}

	// Now, try to find a valid cooperative start.
	FPlayerStart *pSpot = SelectRandomCooperativeSpot( ulPlayer );

	// ANAMOLOUS HAPPENING!!!
	if ( pSpot == NULL )
		I_Error( "Could not find a valid deathmatch spot! (this should not happen)" );

	AActor *mo = P_SpawnPlayer( pSpot, ulPlayer, ( bTempPlayer ? SPF_TEMPPLAYER : 0 ) | ( bClientUpdate ? SPF_CLIENTUPDATE : 0 ) );
	if (mo != NULL) P_PlayerStartStomp(mo);
}

//
// G_PickPlayerStart
//
FPlayerStart *G_PickPlayerStart(int playernum, int flags)
{
	if ((level.flags2 & LEVEL2_RANDOMPLAYERSTARTS) || (flags & PPS_FORCERANDOM))
	{
		if (!(flags & PPS_NOBLOCKINGCHECK))
		{
			TArray<FPlayerStart *> good_starts;
			unsigned int i;

			// Find all unblocked player starts.
			for (i = 0; i < AllPlayerStarts.Size(); ++i)
			{
				if (G_CheckSpot(playernum, &AllPlayerStarts[i]))
				{
					good_starts.Push(&AllPlayerStarts[i]);
				}
			}
			if (good_starts.Size() > 0)
			{ // Pick an open spot at random.
				return good_starts[pr_pspawn(good_starts.Size())];
			}
	}
		// Pick a spot at random, whether it's open or not.
		return &AllPlayerStarts[pr_pspawn(AllPlayerStarts.Size())];
	}
	return &playerstarts[playernum];
}

//
// G_QueueBody
//
// [BB] Skulltag needs this also in cl_main.cpp.
/*static*/ void G_QueueBody (AActor *body)
{
	// flush an old corpse if needed
	int modslot = bodyqueslot%BODYQUESIZE;

	if (bodyqueslot >= BODYQUESIZE && bodyque[modslot] != NULL)
	{
		bodyque[modslot]->Destroy ();
	}
	bodyque[modslot] = body;

	// Copy the player's translation, so that if they change their color later, only
	// their current body will change and not all their old corpses.
	if (GetTranslationType(body->Translation) == TRANSLATION_Players ||
		GetTranslationType(body->Translation) == TRANSLATION_PlayersExtra)
	{
		*translationtables[TRANSLATION_PlayerCorpses][modslot] = *TranslationToTable(body->Translation);
		body->Translation = TRANSLATION(TRANSLATION_PlayerCorpses,modslot);
		translationtables[TRANSLATION_PlayerCorpses][modslot]->UpdateNative();
	}

	const int skinidx = body->player->userinfo.GetSkin();

	if (0 != skinidx && !(body->flags4 & MF4_NOSKIN))
	{
		// Apply skin's scale to actor's scale, it will be lost otherwise
		const AActor *const defaultActor = body->GetDefault();
		const FPlayerSkin &skin = skins[skinidx];

		body->scaleX = Scale(body->scaleX, skin.ScaleX, defaultActor->scaleX);
		body->scaleY = Scale(body->scaleY, skin.ScaleY, defaultActor->scaleY);
	}

	bodyqueslot++;
}

//
// G_DoReborn
//
void G_DoReborn (int playernum, bool freshbot)
{
	// All of this is done remotely.
	if ( NETWORK_InClientMode() )
	{
		return;
	}
	else if ((NETWORK_GetState( ) == NETSTATE_SINGLE) && !(level.flags2 & LEVEL2_ALLOWRESPAWN))
	{
		if (BackupSaveName.Len() > 0 && FileExists (BackupSaveName.GetChars()))
		{ // Load game from the last point it was saved
			savename = BackupSaveName;
			gameaction = ga_autoloadgame;
		}
		else
		{ // Reload the level from scratch
			bool indemo = demoplayback;
			BackupSaveName = "";
			G_InitNew (level.mapname, false);
			demoplayback = indemo;
//			gameaction = ga_loadlevel;
		}
	}
	else
	{
		// respawn at the start
		// first disassociate the corpse
		AActor *pOldBody = players[playernum].mo; // [BB] ST still needs the old body pointer.
		if (players[playernum].mo)
		{
			// [BB] Skulltag has its own body queue. If G_QueueBody is used, the
			// STFL_OBSOLETE_SPECTATOR_BODY code below has to be adapted.
			if ( !( players[playernum].mo->ulSTFlags & STFL_OBSOLETE_SPECTATOR_BODY ) )
				G_QueueBody (players[playernum].mo);
			players[playernum].mo->player = NULL;
		}
		// [BB] The old body is not a corpse, but an obsolete spectator body. Remove it.
		if ( pOldBody && ( pOldBody->ulSTFlags & STFL_OBSOLETE_SPECTATOR_BODY ) )
		{
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_DestroyThing( pOldBody );

			pOldBody->Destroy( );
			pOldBody = NULL;
		}

		/* [BB] ST has its own way of doing this.
		// spawn at random spot if in deathmatch
		if (deathmatch)
		{
			G_DeathMatchSpawnPlayer (playernum);
			return;
		}

		if (!(level.flags2 & LEVEL2_RANDOMPLAYERSTARTS) &&
			G_CheckSpot (playernum, &playerstarts[playernum]))
		{
			AActor *mo = P_SpawnPlayer(&playerstarts[playernum], playernum);
			if (mo != NULL) P_PlayerStartStomp(mo);
		}
		else
		{ // try to spawn at any random player's spot
			FPlayerStart *start = G_PickPlayerStart(playernum, PPS_FORCERANDOM);
			AActor *mo = P_SpawnPlayer(start, playernum);
			if (mo != NULL) P_PlayerStartStomp(mo);
		}
		*/

		GAMEMODE_SpawnPlayer( playernum );

		// If the player fired a missile before he died, update its target so he gets
		// credit if it kills someone.
		if ( pOldBody )
		{
			AActor						*pActor;
			TThinkerIterator<AActor>	Iterator;
			
			while ( (pActor = Iterator.Next( )))
			{
				if ( pActor->target == pOldBody )
					pActor->target = players[playernum].mo;
			}
		}
	}
}

//*****************************************************************************
//
// Sets default dmflags based on gamemode
//
void GAME_SetDefaultDMFlags()
{
	int flags = dmflags;
	int flags2 = dmflags2;

	if ( deathmatch )
	{
		// Don't do "spawn farthest" for duels.
		if ( duel )
			flags |= DF_WEAPONS_STAY | DF_ITEMS_RESPAWN | DF_NO_MONSTERS | DF_NO_CROUCH;
		else
			flags |= DF_SPAWN_FARTHEST | DF_WEAPONS_STAY | DF_ITEMS_RESPAWN | DF_NO_MONSTERS | DF_NO_CROUCH;

		flags2 |= DF2_YES_DOUBLEAMMO;
	}
	else if ( teamgame )
	{
		flags |= DF_WEAPONS_STAY | DF_ITEMS_RESPAWN | DF_NO_MONSTERS | DF_NO_CROUCH;
		flags2 |= DF2_YES_DOUBLEAMMO;
	}
	else
	{
		flags &= ~DF_WEAPONS_STAY | ~DF_ITEMS_RESPAWN | ~DF_NO_MONSTERS | ~DF_NO_CROUCH;
		flags2 &= ~DF2_YES_DOUBLEAMMO;
	}

	if ( dmflags != flags )
		dmflags = flags;

	if ( dmflags2 != flags2 )
		dmflags2 = flags2;
}

//*****************************************************************************
// Determine is a level is a deathmatch, CTF, etc. level by items that are placed on it.
//
void GAME_CheckMode( void )
{
	ULONG						ulFlags = (ULONG)dmflags;
	ULONG						ulFlags2 = (ULONG)dmflags2;
	UCVarValue					Val;
	ULONG						ulIdx;
	ULONG						ulNumFlags;
	ULONG						ulNumSkulls;
	bool						bPlayerStarts = false;
	bool						bTeamStarts = false;
	AActor						*pItem;
	AActor						*pNewSkull;
	cluster_info_t				*pCluster;
	TThinkerIterator<AActor>	iterator;

	// Clients can't change flags/modes!
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	// By default, we're in regular CTF/ST mode.
	TEAM_SetSimpleCTFSTMode( false );

	// Also, reset the team module.
	TEAM_Reset( );

	// First, we need to count the number of real single/coop player starts.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( playerstarts[ulIdx].type > 0 )
		{
			bPlayerStarts = true;
			break;
		}
	}

	// [CW] Check whether any team starts are available.
	for ( ULONG i = 0; i < TEAM_GetNumAvailableTeams( ); i++ )
	{
		if ( teams[i].TeamStarts.Size( ) != 0 )
		{
			bTeamStarts = true;
			break;
		}
	}

	// We have deathmatch starts, but nothing else.
	if (( deathmatchstarts.Size( ) > 0 ) && 
		( TemporaryTeamStarts.Size( ) == 0 ) && !bTeamStarts && 
		 bPlayerStarts == false )
	{
		// Since we only have deathmatch starts, enable deathmatch, and disable teamgame/coop.
		Val.Bool = true;
		deathmatch.ForceSet( Val, CVAR_Bool );
		Val.Bool = false;
		teamgame.ForceSet( Val, CVAR_Bool );
	}

	// We have team starts, but nothing else.
	if ((( TemporaryTeamStarts.Size( ) > 0 ) || bTeamStarts ) &&
		( deathmatchstarts.Size( ) == 0 ) &&
		bPlayerStarts == false )
	{
		// Since we only have teamgame starts, enable teamgame, and disable deathmatch/coop.
		Val.Bool = true;
		teamgame.ForceSet( Val, CVAR_Bool );
		Val.Bool = false;
		deathmatch.ForceSet( Val, CVAR_Bool );

		// Furthermore, we can determine between a ST and CTF level by whether or not
		// there are skulls or flags placed on the level.
//		if ( oneflagctf == false )
		{
			TThinkerIterator<AActor>	iterator;

			ulNumFlags = TEAM_CountFlags( );
			ulNumSkulls = TEAM_CountSkulls( );

			// We found flags but no skulls. Set CTF mode.
			if ( ulNumFlags && ( ulNumSkulls == 0 ))
			{
				Val.Bool = true;

				if ( oneflagctf == false )
					ctf.ForceSet( Val, CVAR_Bool );
			}

			// We found skulls but no flags. Set Skulltag mode.
			else if ( ulNumSkulls && ( ulNumFlags == 0 ))
			{
				Val.Bool = true;

				skulltag.ForceSet( Val, CVAR_Bool );
			}
			// [BB] There are domination points, but no skulls/flags. Activate domination.
			else if ( level.info->SectorInfo.Points.Size() > 0 )
			{
				Val.Bool = true;

				domination.ForceSet( Val, CVAR_Bool );
			}
		}
	}

	// We have single player starts, but nothing else.
	if ( bPlayerStarts == true &&
		( TemporaryTeamStarts.Size( ) == 0 ) && !bTeamStarts &&
		( deathmatchstarts.Size( ) == 0 ))
	{
/*
		// Ugh, this is messy... tired and don't feel like explaning why this is needed...
		if (( NETWORK_GetState( ) != NETSTATE_SINGLE ) || deathmatch || teamgame )
		{
			for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
			{
				if ( playeringame[ulIdx] == false )
					continue;

				AActor *mo = P_SpawnPlayer( &playerstarts[ulIdx], false, NULL );
				if (mo != NULL) P_PlayerStartStomp(mo);
			}
		}
*/
		// Since we only have single player starts, disable deathmatch and teamgame.
		Val.Bool = false;
		deathmatch.ForceSet( Val, CVAR_Bool );
		teamgame.ForceSet( Val, CVAR_Bool );

		// If invasion starts are present, load the map in invasion mode.
		if ( GenericInvasionStarts.Size( ) > 0 )
		{
			Val.Bool = true;
			invasion.ForceSet( Val, CVAR_Bool );
		}
		else
		{
			Val.Bool = false;
			invasion.ForceSet( Val, CVAR_Bool );
		}
	}

	if ( cooperative && ( GenericInvasionStarts.Size( ) == 0 ))
	{
		Val.Bool = false;
		invasion.ForceSet( Val, CVAR_Bool );
	}

	// Disallow survival mode in hubs.
	pCluster = FindClusterInfo( level.cluster );
	if (( cooperative ) &&
		( pCluster ) &&
		( pCluster->flags & CLUSTER_HUB ))
	{
		Val.Bool = false;
		survival.ForceSet( Val, CVAR_Bool );
	}

	// In a campaign, just use whatever dmflags are assigned.
	if ( sv_defaultdmflags && ( CAMPAIGN_InCampaign() == false ))
	{
		// Allow servers to use their own dmflags.
		GAME_SetDefaultDMFlags();
	}

	// If there aren't any pickup, blue return, or red return scripts, then use the
	// simplified, hardcoded version of the CTF or ST modes.
	// [BB] The loop over the teams is a tricky way to check if at least one of the
	// scripts necessary is missing and works because of the break further down.
	for ( ULONG i = 0; i < TEAM_GetNumAvailableTeams( ); i++ )
	{
		if ( ( GAMEMODE_GetCurrentFlags() & GMF_USETEAMITEM ) &&
			(( FBehavior::StaticCountTypedScripts( SCRIPT_Pickup ) == 0 ) ||
			( FBehavior::StaticCountTypedScripts( TEAM_GetReturnScriptOffset( i ) ) == 0 )) )
		{
			if ( GAMEMODE_GetCurrentFlags() & GMF_USEFLAGASTEAMITEM )
			{
				TEAM_SetSimpleCTFSTMode( true );

				while ( (pItem = iterator.Next( )))
				{
					for ( ULONG i = 0; i < TEAM_GetNumAvailableTeams( ); i++ )
					{
						if ( pItem->GetClass( ) == TEAM_GetItem( i ))
						{
							POS_t	Origin;

							Origin.x = pItem->x;
							Origin.y = pItem->y;
							Origin.z = pItem->z;

							TEAM_SetTeamItemOrigin( i, Origin );
						}
					}

					if ( pItem->IsKindOf( PClass::FindClass( "WhiteFlag" )))
					{
						POS_t	Origin;

						Origin.x = pItem->x;
						Origin.y = pItem->y;
						Origin.z = pItem->z;

						TEAM_SetWhiteFlagOrigin( Origin );
					}
				}
			}
			// We found skulls but no flags. Set Skulltag mode.
			else
			{
				TEAM_SetSimpleCTFSTMode( true );

				while ( (pItem = iterator.Next( )))
				{
					for ( ULONG i = 0; i < TEAM_GetNumAvailableTeams( ); i++ )
					{
						if ( pItem->GetClass( ) == TEAM_GetItem( i ))
						{
							POS_t	Origin;

							Origin.x = pItem->x;
							Origin.y = pItem->y;
							Origin.z = pItem->z;

							TEAM_SetTeamItemOrigin( i, Origin );
						}
					}

					if ( pItem->IsKindOf( PClass::FindClass( "BlueSkull" )))
					{
						POS_t	Origin;

						// Replace this skull with skulltag mode's version of the skull.
						pNewSkull = Spawn( PClass::FindClass( "BlueSkullST" ), pItem->x, pItem->y, pItem->z, NO_REPLACE );
						if ( pNewSkull )
						{
							pNewSkull->flags &= ~MF_DROPPED;

							// [BB] If we replace a map spawned item, the new item still needs to
							// be considered map spawned. Otherwise it vanishes in a map reset.
							if ( pItem->ulSTFlags & STFL_LEVELSPAWNED )
								pNewSkull->ulSTFlags |= STFL_LEVELSPAWNED ;
						}

						Origin.x = pItem->x;
						Origin.y = pItem->y;
						Origin.z = pItem->z;

						TEAM_SetTeamItemOrigin( 0, Origin );
						pItem->Destroy( );
					}

					if ( pItem->IsKindOf( PClass::FindClass( "RedSkull" )))
					{
						POS_t	Origin;

						// Replace this skull with skulltag mode's version of the skull.
						pNewSkull = Spawn( PClass::FindClass( "RedSkullST" ), pItem->x, pItem->y, pItem->z, NO_REPLACE );
						if ( pNewSkull )
						{
							pNewSkull->flags &= ~MF_DROPPED;

							// [BB] If we replace a map spawned item, the new item still needs to
							// be considered map spawned. Otherwise it vanishes in a map reset.
							if ( pItem->ulSTFlags & STFL_LEVELSPAWNED )
								pNewSkull->ulSTFlags |= STFL_LEVELSPAWNED ;
						}

						Origin.x = pItem->x;
						Origin.y = pItem->y;
						Origin.z = pItem->z;

						TEAM_SetTeamItemOrigin( 1, Origin );
						pItem->Destroy( );
					}
				}
			}

			// [BB] See comment at the "team" loop.
			break;
		}
	}

	// Reset the status bar.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		if ( StatusBar )
		{
			StatusBar->Destroy();
			StatusBar = NULL;
		}

		if ( gamestate == GS_TITLELEVEL )
			StatusBar = new DBaseStatusBar( 0 );
		else
			StatusBar = CreateStatusBar ();
		/* [BB] Moved to CreateStatusBar()
		else if ( gameinfo.gametype == GAME_Doom )
			StatusBar = CreateDoomStatusBar( );
		else if ( gameinfo.gametype == GAME_Heretic )
			StatusBar = CreateHereticStatusBar( );
		else if ( gameinfo.gametype == GAME_Hexen )
			StatusBar = CreateHexenStatusBar( );
		else if ( gameinfo.gametype == GAME_Strife )
			StatusBar = CreateStrifeStatusBar( );
		else
			StatusBar = new FBaseStatusBar( 0 );
		*/

		StatusBar->AttachToPlayer( &players[consoleplayer] );
		StatusBar->NewGame( );
	}

	// [BB] Since we possibly just changed the game mode, make sure that players are not on a team anymore
	// if the current game mode doesn't have teams.
	if ( ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS ) == false )
	{
		for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
			PLAYER_SetTeam( &players[ulIdx], teams.Size( ), true );
	}
	// [BB] If we are using teams and the teams are supposed to be selected automatically, select the team
	// for all non-spectator players that are not on a team yet now.
	else if ( dmflags2 & DF2_NO_TEAM_SELECT ) 
	{
		for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			if ( playeringame[ulIdx] && ( players[ulIdx].bSpectating == false ) && ( players[ulIdx].bOnTeam == false ) )
				PLAYER_SetTeam( &players[ulIdx], TEAM_ChooseBestTeamForPlayer( ), true );
		}
	}
}

//*****************************************************************************
//
bool GAME_ZPositionMatchesOriginal( AActor *pActor )
{
	fixed_t		Space;

	// Determine the Z position to spawn this actor in.
	if ( pActor->flags & MF_SPAWNCEILING )
		return ( pActor->z == ( pActor->Sector->ceilingplane.ZatPoint( pActor->x, pActor->y ) - pActor->height - ( pActor->SpawnPoint[2] )));
	else if ( pActor->flags2 & MF2_SPAWNFLOAT )
	{
		Space = pActor->Sector->ceilingplane.ZatPoint( pActor->x, pActor->y ) - pActor->height - pActor->Sector->floorplane.ZatPoint( pActor->x, pActor->y );
		if ( Space > ( 48 * FRACUNIT ))
		{
			Space -= ( 40 * FRACUNIT );
			if (( pActor->z >= MulScale8( Space, 0 ) + ( pActor->Sector->floorplane.ZatPoint( pActor->x, pActor->y ) + 40 * FRACUNIT )) ||
				( pActor->z <= MulScale8( Space, INT_MAX ) + ( pActor->Sector->floorplane.ZatPoint( pActor->x, pActor->y ) + 40 * FRACUNIT )))
			{
				return ( true );
			}
			return ( false );
		}
		else
			return ( pActor->z == pActor->Sector->floorplane.ZatPoint( pActor->x, pActor->y ));
	}
	else
		return ( pActor->z == ( pActor->Sector->floorplane.ZatPoint( pActor->x, pActor->y ) + ( pActor->SpawnPoint[2] )));
}

//*****************************************************************************
//
bool GAME_DormantStatusMatchesOriginal( AActor *pActor )
{
	// For objects that have just spawned, assume that their dormant status is fine.
	if ( pActor->ObjectFlags & OF_JustSpawned )
		return ( true );

	if (( pActor->flags3 & MF3_ISMONSTER ) &&
		(( pActor->health > 0 ) || ( pActor->flags & MF_ICECORPSE )))
	{
		if ( pActor->flags2 & MF2_DORMANT )
			return !!( pActor->SpawnFlags & MTF_DORMANT );
		else
			return (( pActor->SpawnFlags & MTF_DORMANT ) == false );
	}

	if ( pActor->GetClass( )->IsDescendantOf( PClass::FindClass( "ParticleFountain" )))
	{
		if (( pActor->effects & ( pActor->health << FX_FOUNTAINSHIFT )) == false )
			return !!( pActor->SpawnFlags & MTF_DORMANT );
		else
			return (( pActor->SpawnFlags & MTF_DORMANT ) == false );
	}

	return ( true );
}

//*****************************************************************************
//
void GAME_BackupLineProperties ( line_t *li )
{
	li->SavedSpecial = li->special;
	li->SavedFlags = li->flags;
	li->SavedArgs[0] = li->args[0];
	li->SavedArgs[1] = li->args[1];
	li->SavedArgs[2] = li->args[2];
	li->SavedArgs[3] = li->args[3];
	li->SavedArgs[4] = li->args[4];
}

//*****************************************************************************
//
// Ugh.
void P_LoadBehavior( MapData *pMap );

void GAME_ResetScripts ( )
{
	// Unload the ACS scripts so we can reload them.
	FBehavior::StaticUnloadModules( );
	if ( DACSThinker::ActiveThinker != NULL )
	{
		// [BB] Stop and destroy all active ACS scripts.
		DACSThinker::ActiveThinker->StopAndDestroyAllScripts();
	}

	// Open the current map and load its BEHAVIOR lump.
	MapData *pMap = P_OpenMapData( level.mapname, false );
	if ( pMap == NULL )
		I_Error( "GAME_ResetMap: Unable to open map '%s'\n", level.mapname );
	else if ( pMap->HasBehavior )
		P_LoadBehavior( pMap );

	// Run any default scripts that needed to be run.
	FBehavior::StaticLoadDefaultModules( );

	// Restart running any open scripts on this map, since we just destroyed them all!
	// [BB] The server instructs the clients to start the CLIENTSIDE open scripts.
	if ( NETWORK_InClientMode() == false )
		FBehavior::StaticStartTypedScripts( SCRIPT_Open, NULL, false );

	delete ( pMap );
}

void DECAL_ClearDecals( void );
FPolyObj *GetPolyobjByIndex( ULONG ulPoly );
void GAME_ResetMap( bool bRunEnterScripts )
{
	ULONG							ulIdx;
	level_info_t					*pLevelInfo;
	bool							bSendSkyUpdate;
//	sector_t						*pSector;
//	fixed_t							Space;
	AActor							*pActor;
	AActor							*pNewActor;
	AActor							*pActorInfo;
	fixed_t							X;
	fixed_t							Y;
	fixed_t							Z;
	TThinkerIterator<AActor>		ActorIterator;

	// Unload decals.
	DECAL_ClearDecals( );

	// [BB] Possibly reset level time to 0.
	if ( GAMEMODE_GetCurrentFlags() & GMF_MAPRESET_RESETS_MAPTIME )
		level.time = 0;

	// [BB] The effect of MapRevealer needs to be reset manually.
	level.flags2 &= ~LEVEL2_ALLMAP;

	// [BB] If MAPINFO didn't tell us to start lightning, make sure all lightning is stopped now.
	if ( !(level.flags & LEVEL_STARTLIGHTNING) )
		P_ForceLightning( 2 );

	// [BB] Reset special stuff for the current gamemode, like control point ownership in Domination.
	GAMEMODE_ResetSpecalGamemodeStates();

	// [BB] If a PowerTimeFreezer was in effect, the sound could be paused. Make sure that it is resumed.
	S_ResumeSound( false );

	// [BB] We are going to reset the map now, so any request for a reset is fulfilled.
	g_bResetMap = false;

	// [Dusk] Clear list of keys found now.
	g_keysFound.Clear();

	// [BB] itemcount and secretcount are not synced between client and server, so just reset them here.
	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ++ulIdx )
	{
		players[ulIdx].itemcount = 0;
		players[ulIdx].secretcount = 0;
	}

	// [BB] Destroy all lighting effects that were not spawned by the map.
	{
		TThinkerIterator<DLighting>		Iterator;
		DLighting						*pEffect;

		while (( pEffect = Iterator.Next( )) != NULL )
		{
			if ( pEffect->bNotMapSpawned == true )
			{
				pEffect->GetSector()->lightlevel = pEffect->GetSector()->SavedLightLevel;
				pEffect->Destroy( );
			}
		}
	}

	// This is all we do in client mode.
	if ( NETWORK_InClientMode() )
	{
		// [BB] Clients still need to reset the automap.
		for ( ulIdx = 0; ulIdx < (ULONG)numlines; ulIdx++ )
			lines[ulIdx].flags &= ~ ML_MAPPED;

		// [BB] Remove all CLIENTSIDEONLY actors not spawned by the map.
		while (( pActor = ActorIterator.Next( )) != NULL )
		{
			if ( ( ( pActor->ulSTFlags & STFL_LEVELSPAWNED ) == false ) && ( pActor->ulNetworkFlags & NETFL_CLIENTSIDEONLY ) )
			{
				// [BB] This caused problems on the non-client code, so until we discover what
				// exactly happnes there, just do the same workaround here.
				if( !pActor->IsKindOf( RUNTIME_CLASS( ADynamicLight ) ) )
					pActor->Destroy( );
				continue;
			}

			// [BB] ALLOWCLIENTSPAWN actors spawned by the map are supposed to stay untouched. Some mods ignore
			// this restriction. To work around some problems caused by this, we reset their args. In particular,
			// this is helpful for DynamicLight tricks.
			if ( ( pActor->ulSTFlags & STFL_LEVELSPAWNED ) && ( pActor->ulNetworkFlags & NETFL_ALLOWCLIENTSPAWN ) )
				for ( int i = 0; i < 5; ++i )
					pActor->args[i] = pActor->SavedArgs[i];
		}

		// [BB] Clients may be running CLIENTSIDE scripts, so we also need to reset ACS scripts on the clients.
		GAME_ResetScripts ( );
		return;
	}

	// [BB] Reset the polyobjs.
	for ( ulIdx = 0; ulIdx <= static_cast<unsigned> (po_NumPolyobjs); ulIdx++ )
	{
		FPolyObj *pPoly = GetPolyobjByIndex( ulIdx );
		if ( pPoly == NULL )
			continue;

		// [BB] Is this object being moved?
		if ( pPoly->specialdata != NULL )
		{
			DPolyAction *pPolyAction = pPoly->specialdata;
			// [WS] We have a poly object door, lets destroy it.
			if ( pPolyAction->IsKindOf ( RUNTIME_CLASS( DPolyDoor ) ) )
			{
				// [WS] Tell clients to destroy the door.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_DestroyPolyDoor( pPolyAction->GetPolyObj() );
			}
			// [BB] We also have to destroy all other movers.
			// [EP/TP] Ensure 'DestroyMovePoly' is called on the poly movers, not on the poly rotors.
			else if ( pPolyAction->IsKindOf ( RUNTIME_CLASS( DMovePoly ) ) )
			{
				// [BB] Tell clients to destroy this mover.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_DestroyMovePoly( pPolyAction->GetPolyObj() );
			}
			// [EP/TP] Deal with the poly rotors, too.
			else if ( pPolyAction->IsKindOf ( RUNTIME_CLASS( DRotatePoly ) ) )
			{
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_DestroyRotatePoly( pPolyAction->GetPolyObj() );
			}

			// [BB] Tell clients to destroy the door and stop its sound.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_StopPolyobjSound( pPolyAction->GetPolyObj() );

			// [BB] Stop all sounds associated with this object.
			SN_StopSequence( pPoly );
			pPolyAction->Destroy();

			// [BB] We have destoyed the mover, so remove the pointer to it from the polyobj.
			pPoly->specialdata = NULL;
		}

		if ( pPoly->bMoved )
		{

			const LONG lDeltaX = pPoly->SavedStartSpot[0] - pPoly->StartSpot.x;
			const LONG lDeltaY = pPoly->SavedStartSpot[1] - pPoly->StartSpot.y;

			pPoly->MovePolyobj( lDeltaX, lDeltaY, true );
			pPoly->bMoved = false;

			// [BB] If we're the server, tell clients about this polyobj reset.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetPolyobjPosition( pPoly->tag );
		}

		if ( pPoly->bRotated )
		{
			// [BB] AFAIK a polyobj always starts with angle 0;
			const LONG lDeltaAngle = 0 - pPoly->angle;

			pPoly->RotatePolyobj( lDeltaAngle );
			pPoly->bRotated = false;

			// [BB] If we're the server, tell clients about this polyobj reset.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetPolyobjRotation( pPoly->tag );
		}
	}

	for ( ulIdx = 0; ulIdx < (ULONG)numlines; ulIdx++ )
	{
		// Reset the line's special.
		lines[ulIdx].special = lines[ulIdx].SavedSpecial;
		lines[ulIdx].args[0] = lines[ulIdx].SavedArgs[0];
		lines[ulIdx].args[1] = lines[ulIdx].SavedArgs[1];
		lines[ulIdx].args[2] = lines[ulIdx].SavedArgs[2];
		lines[ulIdx].args[3] = lines[ulIdx].SavedArgs[3];
		lines[ulIdx].args[4] = lines[ulIdx].SavedArgs[4];

		// Also, restore any changed textures.
		if ( lines[ulIdx].ulTexChangeFlags != 0 )
		{
			if ( lines[ulIdx].sidedef[0] != NULL )
			{
				lines[ulIdx].sidedef[0]->SetTexture(side_t::top, lines[ulIdx].sidedef[0]->SavedTopTexture);
				lines[ulIdx].sidedef[0]->SetTexture(side_t::mid, lines[ulIdx].sidedef[0]->SavedMidTexture);
				lines[ulIdx].sidedef[0]->SetTexture(side_t::bottom, lines[ulIdx].sidedef[0]->SavedBottomTexture);
			}

			if ( lines[ulIdx].sidedef[1] != NULL )
			{
				lines[ulIdx].sidedef[1]->SetTexture(side_t::top, lines[ulIdx].sidedef[1]->SavedTopTexture);
				lines[ulIdx].sidedef[1]->SetTexture(side_t::mid, lines[ulIdx].sidedef[1]->SavedMidTexture);
				lines[ulIdx].sidedef[1]->SetTexture(side_t::bottom, lines[ulIdx].sidedef[1]->SavedBottomTexture);
			}

			// If we're the server, tell clients about this texture change.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetLineTexture( ulIdx );

			// Mark the texture as no being changed.
			lines[ulIdx].ulTexChangeFlags = 0;
		}

		// Restore the line's alpha if it changed.
		if ( lines[ulIdx].Alpha != lines[ulIdx].SavedAlpha )
		{
			lines[ulIdx].Alpha = lines[ulIdx].SavedAlpha;

			// If we're the server, tell clients about this alpha change.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetLineAlpha( ulIdx );
		}

		// Restore the line's blocking status and the ML_ADDTRANS setting.
		if ( lines[ulIdx].flags != lines[ulIdx].SavedFlags )
		{
			lines[ulIdx].flags = lines[ulIdx].SavedFlags;

			// If we're the server, tell clients about this blocking change and the ML_ADDTRANS setting.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetSomeLineFlags( ulIdx );
		}
	}

	// Restore sector heights, flat changes, light changes, etc.
	for ( ulIdx = 0; ulIdx < (ULONG)numsectors; ulIdx++ )
	{
/*
		if (( sectors[ulIdx].ceilingplane.a != sectors[ulIdx].SavedCeilingPlane.a ) ||
			( sectors[ulIdx].ceilingplane.b != sectors[ulIdx].SavedCeilingPlane.b ) ||
			( sectors[ulIdx].ceilingplane.c != sectors[ulIdx].SavedCeilingPlane.c ) ||
			( sectors[ulIdx].ceilingplane.ic != sectors[ulIdx].SavedCeilingPlane.ic ))
		{
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetSectorCeilingPlaneSlope( ulIdx );
		}
*/
		if ( sectors[ulIdx].bCeilingHeightChange )
		{
			sectors[ulIdx].ceilingplane = sectors[ulIdx].SavedCeilingPlane;
			sectors[ulIdx].SetPlaneTexZ(sector_t::ceiling, sectors[ulIdx].SavedCeilingTexZ);
			sectors[ulIdx].bCeilingHeightChange = false;

			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetSectorCeilingPlane( ulIdx );
		}
/*
		if (( sectors[ulIdx].floorplane.a != sectors[ulIdx].floorplane.a ) ||
			( sectors[ulIdx].floorplane.b != sectors[ulIdx].floorplane.b ) ||
			( sectors[ulIdx].floorplane.c != sectors[ulIdx].floorplane.c ) ||
			( sectors[ulIdx].floorplane.ic != sectors[ulIdx].floorplane.ic ))
		{
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetSectorFloorPlaneSlope( ulIdx );
		}
*/
		if ( sectors[ulIdx].bFloorHeightChange )
		{
			sectors[ulIdx].floorplane = sectors[ulIdx].SavedFloorPlane;
			sectors[ulIdx].SetPlaneTexZ(sector_t::floor, sectors[ulIdx].SavedFloorTexZ);
			sectors[ulIdx].bFloorHeightChange = false;
			// [BB] Break any stair locks. This does not happen when the corresponding movers are destroyed.
			sectors[ulIdx].stairlock = 0;
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetSectorFloorPlane( ulIdx );
		}

		if ( sectors[ulIdx].bFlatChange )
		{
			sectors[ulIdx].SetTexture(sector_t::floor, sectors[ulIdx].SavedFloorPic);
			sectors[ulIdx].SetTexture(sector_t::ceiling, sectors[ulIdx].SavedCeilingPic);
			sectors[ulIdx].bFlatChange = false;

			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetSectorFlat( ulIdx );
		}

		if ( sectors[ulIdx].bLightChange )
		{
			sectors[ulIdx].lightlevel = sectors[ulIdx].SavedLightLevel;
			sectors[ulIdx].bLightChange = false;

			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetSectorLightLevel( ulIdx );
		}

		if ( sectors[ulIdx].ColorMap != sectors[ulIdx].SavedColorMap )
		{
			sectors[ulIdx].ColorMap = sectors[ulIdx].SavedColorMap;

			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
				SERVERCOMMANDS_SetSectorColor( ulIdx );
				SERVERCOMMANDS_SetSectorFade( ulIdx );
			}
		}

		if (( sectors[ulIdx].SavedFloorXOffset != sectors[ulIdx].GetXOffset(sector_t::floor) ) ||
			( sectors[ulIdx].SavedFloorYOffset != sectors[ulIdx].GetYOffset(sector_t::floor,false) ) ||
			( sectors[ulIdx].SavedCeilingXOffset != sectors[ulIdx].GetXOffset(sector_t::ceiling) ) ||
			( sectors[ulIdx].SavedCeilingYOffset != sectors[ulIdx].GetYOffset(sector_t::ceiling,false) ))
		{
			sectors[ulIdx].SetXOffset(sector_t::floor, sectors[ulIdx].SavedFloorXOffset);
			sectors[ulIdx].SetYOffset(sector_t::floor, sectors[ulIdx].SavedFloorYOffset);
			sectors[ulIdx].SetXOffset(sector_t::ceiling, sectors[ulIdx].SavedCeilingXOffset);
			sectors[ulIdx].SetYOffset(sector_t::ceiling, sectors[ulIdx].SavedCeilingYOffset);

			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetSectorPanning( ulIdx );
		}

		if (( sectors[ulIdx].SavedFloorXScale != sectors[ulIdx].GetXScale(sector_t::floor) ) ||
			( sectors[ulIdx].SavedFloorYScale != sectors[ulIdx].GetYScale(sector_t::floor) ) ||
			( sectors[ulIdx].SavedCeilingXScale != sectors[ulIdx].GetXScale(sector_t::ceiling) ) ||
			( sectors[ulIdx].SavedCeilingYScale != sectors[ulIdx].GetYScale(sector_t::ceiling) ))
		{
			sectors[ulIdx].SetXScale(sector_t::floor, sectors[ulIdx].SavedFloorXScale);
			sectors[ulIdx].SetYScale(sector_t::floor, sectors[ulIdx].SavedFloorYScale);
			sectors[ulIdx].SetXScale(sector_t::ceiling, sectors[ulIdx].SavedCeilingXScale);
			sectors[ulIdx].SetYScale(sector_t::ceiling, sectors[ulIdx].SavedCeilingYScale);

			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetSectorScale( ulIdx );
		}

		if (( sectors[ulIdx].SavedFloorAngle != static_cast<signed> (sectors[ulIdx].GetAngle(sector_t::floor,false)) ) ||
			( sectors[ulIdx].SavedCeilingAngle != static_cast<signed> (sectors[ulIdx].GetAngle(sector_t::ceiling,false)) ))
		{
			sectors[ulIdx].SetAngle(sector_t::floor, sectors[ulIdx].SavedFloorAngle);
			sectors[ulIdx].SetAngle(sector_t::ceiling, sectors[ulIdx].SavedCeilingAngle);

			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetSectorRotation( ulIdx );
		}

		if (( sectors[ulIdx].SavedBaseFloorAngle != sectors[ulIdx].planes[sector_t::floor].xform.base_angle ) ||
			( sectors[ulIdx].SavedBaseFloorYOffset != sectors[ulIdx].planes[sector_t::floor].xform.base_yoffs ) ||
			( sectors[ulIdx].SavedBaseCeilingAngle != sectors[ulIdx].planes[sector_t::ceiling].xform.base_angle ) ||
			( sectors[ulIdx].SavedBaseCeilingYOffset != sectors[ulIdx].planes[sector_t::ceiling].xform.base_yoffs ))
		{
			sectors[ulIdx].planes[sector_t::floor].xform.base_angle = sectors[ulIdx].SavedBaseFloorAngle;
			sectors[ulIdx].planes[sector_t::floor].xform.base_yoffs = sectors[ulIdx].SavedBaseFloorYOffset;
			sectors[ulIdx].planes[sector_t::ceiling].xform.base_angle = sectors[ulIdx].SavedBaseCeilingAngle;
			sectors[ulIdx].planes[sector_t::ceiling].xform.base_yoffs = sectors[ulIdx].SavedBaseCeilingYOffset;

			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetSectorAngleYOffset( ulIdx );
		}

		if (( sectors[ulIdx].SavedFriction != sectors[ulIdx].friction ) ||
			( sectors[ulIdx].SavedMoveFactor != sectors[ulIdx].movefactor ))
		{
			sectors[ulIdx].friction = sectors[ulIdx].SavedFriction;
			sectors[ulIdx].movefactor = sectors[ulIdx].SavedMoveFactor;

			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetSectorFriction( ulIdx );
		}

		if ( sectors[ulIdx].SavedSpecial != sectors[ulIdx].special )
		{
			sectors[ulIdx].special = sectors[ulIdx].SavedSpecial;

			// [BB] If we're the server, tell clients about the special (at least necessary for secrets).
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetSectorSpecial( ulIdx );
		}

		if (( sectors[ulIdx].SavedDamage != sectors[ulIdx].damage ) ||
			( sectors[ulIdx].SavedMOD != sectors[ulIdx].mod ))
		{
			sectors[ulIdx].damage = sectors[ulIdx].SavedDamage;
			sectors[ulIdx].mod = sectors[ulIdx].SavedMOD;

			// No client update necessary here.
		}

		if (( sectors[ulIdx].SavedCeilingReflect != sectors[ulIdx].reflect[sector_t::ceiling] ) ||
			( sectors[ulIdx].SavedFloorReflect != sectors[ulIdx].reflect[sector_t::floor] ))
		{
			sectors[ulIdx].reflect[sector_t::ceiling] = sectors[ulIdx].SavedCeilingReflect;
			sectors[ulIdx].reflect[sector_t::floor] = sectors[ulIdx].SavedFloorReflect;

			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetSectorReflection( ulIdx );
		}

		// [Dusk] Reset 3d midtextures
		if ( sectors[ulIdx].e ) {
			const extsector_t::midtex::plane* planes[2] = {
				&(sectors[ulIdx].e->Midtex.Floor),
				&(sectors[ulIdx].e->Midtex.Ceiling)
			};

			for ( int i = 0; i <= 1; i++ ) {
				const fixed_t move3d = ( planes[i] != NULL ) ? planes[i]->MoveDistance : 0;
				if ( !move3d )
					continue;

				P_Scroll3dMidtex( &sectors[ulIdx], 0, -move3d, !!i );
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_Scroll3dMidtexture( &sectors[ulIdx], -move3d, !!i );
			}
		}
	}

	// Reset the sky properties of the map.
	pLevelInfo = level.info;//FindLevelInfo( level.mapname );
	if ( pLevelInfo )
	{
		bSendSkyUpdate = false;
		if (( stricmp( level.skypic1, pLevelInfo->skypic1 ) != 0 ) ||
			( stricmp( level.skypic2, pLevelInfo->skypic2 ) != 0 ))
		{
			bSendSkyUpdate = true;
		}

		strncpy( level.skypic1, pLevelInfo->skypic1, 8 );
		strncpy( level.skypic2, pLevelInfo->skypic2, 8 );
		if ( level.skypic2[0] == 0 )
			strncpy( level.skypic2, level.skypic1, 8 );

		sky1texture = TexMan.GetTexture( level.skypic1, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable );
		sky2texture = TexMan.GetTexture( level.skypic2, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable );

		R_InitSkyMap( );

		// If we're the server, tell clients to update their sky.
		if (( bSendSkyUpdate ) && ( NETWORK_GetState( ) == NETSTATE_SERVER ))
			SERVERCOMMANDS_SetMapSky( );
	}

	// Reset the number of monsters killed,  items picked up, and found secrets on the level.
	level.killed_monsters = 0;
	level.found_items = 0;
	level.found_secrets = 0;
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVERCOMMANDS_SetMapNumKilledMonsters( );
		SERVERCOMMANDS_SetMapNumFoundItems( );
		SERVERCOMMANDS_SetMapNumFoundSecrets( );
	}

	// [BB] Recount the total number of monsters. We can't just adjust the old
	// level.total_monsters value, since we lost track of monsters that were not spawned
	// by the map and removed during the game, e.g. killed lost souls spawned by a pain
	// elemental.
	level.total_monsters = 0;

	// Restart the map music.
	S_ChangeMusic( level.Music.GetChars(), level.musicorder );
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVER_SetMapMusic( level.Music, level.musicorder );
		SERVERCOMMANDS_SetMapMusic( level.Music, level.musicorder );
	}

	// [BB] TThinkerIterator<AActor> doesn't seem to like if we create new actors while
	// iterating. So just create a list with all current actors and then go through it.
	TArray<AActor *> existingActors;
	while (( pActor = ActorIterator.Next( )) != NULL )
		existingActors.Push ( pActor );

	// Reload the actors on this level.
	for ( unsigned int i = 0; i < existingActors.Size(); ++i )
	{
		pActor = existingActors[i];

		// Don't reload players.
		// [BB] but reload voodoo dolls.
		if ( pActor->IsKindOf( RUNTIME_CLASS( APlayerPawn )) && pActor->player && ( pActor->player->mo == pActor ) )
			continue;

		// If this object belongs to someone's inventory, and it originally spawned on the
		// level, respawn the item in its original location, but don't take it out of the
		// player's inventory.
		if (( pActor->IsKindOf( RUNTIME_CLASS( AInventory ))) && 
			( static_cast<AInventory *>( pActor )->Owner ))
		{
			if ( pActor->ulSTFlags & STFL_LEVELSPAWNED )
			{
				// Get the default information for this actor, so we can determine how to
				// respawn it.
				pActorInfo = pActor->GetDefault( );

				// Spawn the new actor.
				X = pActor->SpawnPoint[0];
				Y = pActor->SpawnPoint[1];

				// Determine the Z point based on its flags.
				if ( pActorInfo->flags & MF_SPAWNCEILING )
					Z = ONCEILINGZ;
				else if ( pActorInfo->flags2 & MF2_SPAWNFLOAT )
					Z = FLOATRANDZ;
				else if ( pActorInfo->flags2 & MF2_FLOATBOB )
					Z = pActor->SpawnPoint[2];
				else
					Z = ONFLOORZ;

				pNewActor = AActor::StaticSpawn( RUNTIME_TYPE( pActor ), X, Y, Z, NO_REPLACE, true );

				// Adjust the Z position after it's spawned.
				if ( Z == ONFLOORZ )
					pNewActor->z += pActor->SpawnPoint[2];
				else if ( Z == ONCEILINGZ )
					pNewActor->z -= pActor->SpawnPoint[2];

				// Inherit attributes from the old actor.
				pNewActor->SpawnPoint[0] = pActor->SpawnPoint[0];
				pNewActor->SpawnPoint[1] = pActor->SpawnPoint[1];
				pNewActor->SpawnPoint[2] = pActor->SpawnPoint[2];
				pNewActor->SpawnAngle = pActor->SpawnAngle;
				pNewActor->SpawnFlags = pActor->SpawnFlags;
				P_FindFloorCeiling ( pNewActor, FFCF_SAMESECTOR | FFCF_ONLY3DFLOORS | FFCF_3DRESTRICT );
				pNewActor->angle = ANG45 * ( pActor->SpawnAngle / 45 );
				pNewActor->tid = pActor->SavedTID;
				pNewActor->SavedTID = pActor->SavedTID;
				pNewActor->special = pActor->SavedSpecial;
				pNewActor->SavedSpecial = pActor->SavedSpecial;
				for ( int i = 0; i < 5; ++i )
				{
					pNewActor->args[i] = pActor->SavedArgs[i];
					pNewActor->SavedArgs[i] = pActor->SavedArgs[i];
				}
				pNewActor->AddToHash( );

				pNewActor->ulSTFlags |= STFL_LEVELSPAWNED;

				// Handle the spawn flags of the item.
				pNewActor->HandleSpawnFlags( );

				pNewActor->BeginPlay ();
				if (!(pNewActor->ObjectFlags & OF_EuthanizeMe))
				{
					pNewActor->LevelSpawned ();
				}

				// Spawn the new actor.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					SERVERCOMMANDS_LevelSpawnThing( pNewActor );

					// Check and see if it's important that the client know the angle of the object.
					if ( pNewActor->angle != 0 )
						SERVERCOMMANDS_SetThingAngle( pNewActor );
				}
			}

			continue;
		}

		// Destroy any actor not present when the map loaded.
		if (( pActor->ulSTFlags & STFL_LEVELSPAWNED ) == false )
		{
			// If this is an item, decrement the total number of item on the level.
			if ( pActor->flags & MF_COUNTITEM )
				level.total_items--;

			// If we're the server, tell clients to delete the actor.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_DestroyThing( pActor );

			// [BB] Destroying lights here will result in crashes after the countdown
			// of a duel ends in skirmish. Has to be investigated.
			if( !pActor->IsKindOf( RUNTIME_CLASS( ADynamicLight ) ) )
				pActor->Destroy( );
			continue;
		}

		// Get the default information for this actor, so we can determine how to
		// respawn it.
		pActorInfo = pActor->GetDefault( );

		// This item appears to be untouched; no need to respawn it.
		if ((( pActor->ulSTFlags & STFL_POSITIONCHANGED ) == false ) &&
			( pActor->state == pActor->InitialState ) &&
			( GAME_DormantStatusMatchesOriginal( pActor )) &&
			( pActor->health == pActorInfo->health ))
		{
			if ( pActor->special != pActor->SavedSpecial )
				pActor->special = pActor->SavedSpecial;

			// [Dusk] Args must be reset too
			for ( ULONG i = 0; i < 5; ++i )
				if ( pActor->args[i] != pActor->SavedArgs[i] )
					pActor->args[i] = pActor->SavedArgs[i];

			// [BB] This is a valid monster on the map, count it.
			if ( pActor->CountsAsKill( ) && !(pActor->flags & MF_FRIENDLY) )
				level.total_monsters++;

			// [Dusk] The TID may have changed, update that.
			if ( pActor->tid != pActor->SavedTID )
			{
				pActor->RemoveFromHash();
				pActor->tid = pActor->SavedTID;
				pActor->AddToHash();
			}
			continue;
		}

		// [BB] Special handling for PointPusher/PointPuller: We can't just destroy and respawn them
		// since DPushers may store a pointer to them and rely on this pointer. Instead of
		// adjusting the DPushers to new pointers (which would need additional netcode)
		// we just move the PointPusher/PointPuller to its original location.
		if ( ( pActor->GetClass()->TypeName == NAME_PointPusher ) || ( pActor->GetClass()->TypeName == NAME_PointPuller ) )
		{
			if ( ( pActor->x != pActor->SpawnPoint[0] )
				|| ( pActor->y != pActor->SpawnPoint[1] )
				|| ( pActor->z != pActor->SpawnPoint[2] ) )
			{
				pActor->SetOrigin ( pActor->SpawnPoint[0], pActor->SpawnPoint[1], pActor->SpawnPoint[2] );

				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_MoveThingExact( pActor, CM_X|CM_Y|CM_Z );
			}

			continue;
		}

		// Spawn the new actor.
		X = pActor->SpawnPoint[0];
		Y = pActor->SpawnPoint[1];

		// Determine the Z point based on its flags.
		if ( pActorInfo->flags & MF_SPAWNCEILING )
			Z = ONCEILINGZ;
		else if ( pActorInfo->flags2 & MF2_SPAWNFLOAT )
			Z = FLOATRANDZ;
		else if ( pActorInfo->flags2 & MF2_FLOATBOB )
			Z = pActor->SpawnPoint[2];
		else
			Z = ONFLOORZ;

		pNewActor = AActor::StaticSpawn( RUNTIME_TYPE( pActor ), X, Y, Z, NO_REPLACE, true );

		// [BB] This if fixes a server crash, if ambient sounds are currently playing
		// at the end of a countdown (DUEL start countdown for example).
		if( pNewActor != NULL ){
			// Adjust the Z position after it's spawned.
			if ( Z == ONFLOORZ )
				pNewActor->z += pActor->SpawnPoint[2];
			else if ( Z == ONCEILINGZ )
				pNewActor->z -= pActor->SpawnPoint[2];

			// Inherit attributes from the old actor.
			pNewActor->SpawnPoint[0] = pActor->SpawnPoint[0];
			pNewActor->SpawnPoint[1] = pActor->SpawnPoint[1];
			pNewActor->SpawnPoint[2] = pActor->SpawnPoint[2];
			pNewActor->SpawnAngle = pActor->SpawnAngle;
			pNewActor->SpawnFlags = pActor->SpawnFlags;
			P_FindFloorCeiling ( pNewActor, FFCF_SAMESECTOR | FFCF_ONLY3DFLOORS | FFCF_3DRESTRICT );
			pNewActor->angle = ANG45 * ( pActor->SpawnAngle / 45 );
			pNewActor->tid = pActor->SavedTID;
			pNewActor->SavedTID = pActor->SavedTID;
			pNewActor->special = pActor->SavedSpecial;
			pNewActor->SavedSpecial = pActor->SavedSpecial;
			for ( int i = 0; i < 5; ++i )
			{
				pNewActor->args[i] = pActor->SavedArgs[i];
				pNewActor->SavedArgs[i] = pActor->SavedArgs[i];
			}
			pNewActor->AddToHash( );

			// Just do this stuff for monsters.
			if ( pActor->flags & MF_COUNTKILL )
			{
				if ( pActor->SpawnFlags & MTF_AMBUSH )
					pNewActor->flags |= MF_AMBUSH;

				pNewActor->reactiontime = 18;

				pNewActor->TIDtoHate = pActor->TIDtoHate;
				pNewActor->LastLookActor = pActor->LastLookActor;
				pNewActor->LastLookPlayerNumber = pActor->LastLookPlayerNumber;
				pNewActor->flags3 |= pActor->flags3 & MF3_HUNTPLAYERS;
				pNewActor->flags4 |= pActor->flags4 & MF4_NOHATEPLAYERS;
			}

			pNewActor->flags &= ~MF_DROPPED;
			pNewActor->ulSTFlags |= STFL_LEVELSPAWNED;

			// Handle the spawn flags of the item.
			pNewActor->HandleSpawnFlags( );

			pNewActor->BeginPlay ();
			if (!(pNewActor->ObjectFlags & OF_EuthanizeMe))
			{
				pNewActor->LevelSpawned ();
			}

			// [BB] Potentially adjust the default flags of this actor.
			GAMEMODE_AdjustActorSpawnFlags ( pNewActor );

			// If the old actor counts as an item, remove it from the total item count
			// since it's being deleted.
			if ( pActor->flags & MF_COUNTITEM )
				level.total_items--;

			// Remove the old actor.
			if ( ( NETWORK_GetState( ) == NETSTATE_SERVER )
				// [BB] The server doesn't tell the clients about indefinitely hidden non-inventory actors during a full update.
				&& ( ( pActor->IsKindOf( RUNTIME_CLASS( AInventory ) ) )
					|| ( pActor->state != RUNTIME_CLASS( AInventory )->ActorInfo->FindState ("HideIndefinitely") ) ) )
				SERVERCOMMANDS_DestroyThing( pActor );

			// [BB] A voodoo doll needs to stay assigned to the corresponding player.
			if ( pActor->IsKindOf( RUNTIME_CLASS( APlayerPawn )) )
				pNewActor->player = pActor->player;
			// Tell clients to spawn the new actor.
			// [BB] Voodoo dolls are not spawned on the clients.
			else if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
				SERVERCOMMANDS_LevelSpawnThing( pNewActor );
				// [BB] The clients assume that the current Z position of the actor is SpawnPoint[2].
				// If it's not, we need to inform them about the actual SpawnPoint.
				if ( pNewActor->z != pActor->SpawnPoint[2] )
					SERVERCOMMANDS_SetThingSpawnPoint( pNewActor, MAXPLAYERS, 0 );

				// Check and see if it's important that the client know the angle of the object.
				if ( pNewActor->angle != 0 )
					SERVERCOMMANDS_SetThingAngle( pNewActor );

				// [BB] The server reset the args of the old actor, inform the clients about this.
				if ( ( pNewActor->args[0] != 0 )
					|| ( pNewActor->args[1] != 0 )
					|| ( pNewActor->args[2] != 0 )
					|| ( pNewActor->args[3] != 0 )
					|| ( pNewActor->args[4] != 0 ) )
					SERVERCOMMANDS_SetThingArguments( pNewActor );

				// [BB] The server copied the tid from the old actor, inform the clients about this.
				if ( pActor->tid != 0 )
					SERVERCOMMANDS_SetThingTID( pNewActor );

				// [BB] Since we called AActor::HandleSpawnFlags and GAMEMODE_AdjustActorSpawnFlags, we possibly need to let the clients know about its effects.
				SERVERCOMMANDS_UpdateThingFlagsNotAtDefaults( pNewActor, MAXPLAYERS, 0 );

				// [EP] AActor::HandleSpawnFlags might have changed the actor's alpha and the RenderStyle, too. If so, inform the clients.
				if ( pNewActor->alpha != pNewActor->GetDefault()->alpha )
					SERVERCOMMANDS_SetThingProperty( pNewActor, APROP_Alpha );
				if ( pNewActor->RenderStyle.AsDWORD != pNewActor->GetDefault()->RenderStyle.AsDWORD )
					SERVERCOMMANDS_SetThingProperty( pNewActor, APROP_RenderStyle );
			}

			pActor->Destroy( );
		}
	}

	// [BB] Restore the special gamemode actors that were not spawned by the map, e.g. terminator sphere or hellstone.
	GAMEMODE_SpawnSpecialGamemodeThings();

	// If we're the server, tell clients the new number of total items/monsters.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVERCOMMANDS_SetMapNumTotalMonsters( );
		SERVERCOMMANDS_SetMapNumTotalItems( );
	}

	// Also, delete all floors, plats, etc. that are in progress.
	for ( ulIdx = 0; ulIdx < (ULONG)numsectors; ulIdx++ )
	{
		if ( sectors[ulIdx].ceilingdata )
		{
			// Stop the sound sequence (if any) associated with this sector.
			SN_StopSequence( &sectors[ulIdx], CHAN_CEILING );

			sectors[ulIdx].ceilingdata->Destroy( );
			sectors[ulIdx].ceilingdata = NULL;
		}
		if ( sectors[ulIdx].floordata )
		{
			// Stop the sound sequence (if any) associated with this sector.
			SN_StopSequence( &sectors[ulIdx], CHAN_FLOOR );

			sectors[ulIdx].floordata->Destroy( );
			sectors[ulIdx].floordata = NULL;
		}
	}

	// If we're the server, tell clients to delete all their ceiling/floor movers.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DestroyAllSectorMovers( );

	// [BB] Reset all ACS scripts.
	GAME_ResetScripts ( );

	// [BB] Restart players' enter scripts if necessary.
	if ( bRunEnterScripts )
	{
		for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			if (( playeringame[ulIdx] ) &&
				( players[ulIdx].bSpectating == false ) &&
				( players[ulIdx].mo ))
			{
				FBehavior::StaticStartTypedScripts( SCRIPT_Enter, players[ulIdx].mo, true );
			}
		}
	}
}

//*****************************************************************************
//
void GAME_RequestMapReset( void )
{
	g_bResetMap = true;
}

//*****************************************************************************
//
bool GAME_IsMapRestRequested( void )
{
	return g_bResetMap;
}

//*****************************************************************************
//
AActor* GAME_SelectRandomSpotForArtifact ( const PClass *pArtifactType, const TArray<FPlayerStart> &Spots )
{
	if ( Spots.Size() == 0 )
		return NULL;

	AActor *pArtifact = NULL;

	// [BB] Try to select a random spot sufficiently often so that hopefully all available spots are checked at least once.
	for (unsigned int j = 0; j < 2*Spots.Size(); ++j)
	{
		const int i = pr_dmspawn() % Spots.Size();

		pArtifact = Spawn( pArtifactType, Spots[i].x, Spots[i].y, ONFLOORZ, ALLOW_REPLACE );
		const DWORD spawnFlags = pArtifact->flags;
		// [BB] Ensure that the artifact is solid, otherwise P_TestMobjLocation won't complain if a player already is at the proposed position.
		pArtifact->flags |= MF_SOLID;

		if ( !P_TestMobjLocation (pArtifact) )
			pArtifact->Destroy ();
		else
		{
			// [BB] Restore the original spawn flags, possibly removing MF_SOLID if we added it.
			pArtifact->flags = spawnFlags;
			return pArtifact;
		}
	}

	// [BB] If there is no free spot, just select one and spawn the artifact there.
	const int spotNum = pr_dmspawn() % Spots.Size();
	return Spawn( pArtifactType, Spots[spotNum].x, Spots[spotNum].y, ONFLOORZ, ALLOW_REPLACE );
}

//*****************************************************************************
//
void GAME_SpawnTerminatorArtifact( void )
{
	AActor		*pTerminatorBall;

	// [BB] One can't hijack SelectRandomDeathmatchSpot to find a free spot for the artifact!
	// [RC] Spawn it at a Terminator start, or a deathmatch spot
	if(TerminatorStarts.Size() > 0) 	// Use the terminator starts, if the mapper added them
		pTerminatorBall = GAME_SelectRandomSpotForArtifact( PClass::FindClass( "Terminator" ), TerminatorStarts );
	else if(deathmatchstarts.Size() > 0) // Or use a deathmatch start, if one exists
		pTerminatorBall = GAME_SelectRandomSpotForArtifact( PClass::FindClass( "Terminator" ), deathmatchstarts );
	else // Or return! Be that way!
		return;

	if ( pTerminatorBall == NULL )
		return;

	// If we're the server, tell clients to spawn the new ball.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SpawnThing( pTerminatorBall );
}

//*****************************************************************************
//
void GAME_SpawnPossessionArtifact( void )
{
	AActor *pPossessionStone = NULL;

	// [BB] One can't hijack SelectRandomDeathmatchSpot to find a free spot for the artifact!
	// [RC] Spawn it at a Possession start, or a deathmatch spot
	if(PossessionStarts.Size() > 0) 	// Did the mapper place possession starts? Use those
		pPossessionStone = GAME_SelectRandomSpotForArtifact( PClass::FindClass( "PossessionStone" ), PossessionStarts );
	else if(deathmatchstarts.Size() > 0) // Or use a deathmatch start, if one exists
		pPossessionStone = GAME_SelectRandomSpotForArtifact( PClass::FindClass( "PossessionStone" ), deathmatchstarts );
	else // Or return! Be that way!
		return;

	if ( pPossessionStone == NULL )
		return;

	// If we're the server, tell clients to spawn the possession stone.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SpawnThing( pPossessionStone );
}

//*****************************************************************************
//
void GAME_SetEndLevelDelay( ULONG ulTicks )
{
	g_ulEndLevelDelay = ulTicks;

	// Tell the clients about the end level delay.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetGameEndLevelDelay( g_ulEndLevelDelay );
}

//*****************************************************************************
//
ULONG GAME_GetEndLevelDelay( void )
{
	return ( g_ulEndLevelDelay );
}

//*****************************************************************************
//
void GAME_SetLevelIntroTicks( USHORT usTicks )
{
	g_ulLevelIntroTicks = usTicks;
}

//*****************************************************************************
//
USHORT GAME_GetLevelIntroTicks( void )
{
	return ( static_cast<USHORT> ( g_ulLevelIntroTicks ) );
}

//*****************************************************************************
//
ULONG GAME_CountLivingAndRespawnablePlayers( void )
{
	ULONG	ulPlayers;

	ulPlayers = 0;
	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( playeringame[ulIdx] ) && ( players[ulIdx].bSpectating == false ) && ( PLAYER_IsAliveOrCanRespawn ( &players[ulIdx] ) == true ))
			ulPlayers++;
	}

	return ( ulPlayers );
}

//*****************************************************************************
//
ULONG GAME_CountActivePlayers( void )
{
	ULONG	ulPlayers;

	ulPlayers = 0;
	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( playeringame[ulIdx] ) && ( players[ulIdx].bSpectating == false ))
			ulPlayers++;
	}

	return ( ulPlayers );
}

void G_ScreenShot (char *filename)
{
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	shotfile = filename;
	gameaction = ga_screenshot;
}



//
// G_InitFromSavegame
// Can be called by the startup code or the menu task.
//
void G_LoadGame (const char* name, bool hidecon)
{
	if (name != NULL)
	{
		savename = name;
		gameaction = !hidecon ? ga_loadgame : ga_loadgamehidecon;
	}
}

static bool CheckSingleWad (char *name, bool &printRequires, bool printwarn)
{
	if (name == NULL)
	{
		return true;
	}
	if (Wads.CheckIfWadLoaded (name) < 0)
	{
		if (printwarn)
		{
			if (!printRequires)
			{
				Printf ("This savegame needs these wads:\n%s", name);
			}
			else
			{
				Printf (", %s", name);
			}
		}
		printRequires = true;
		delete[] name;
		return false;
	}
	delete[] name;
	return true;
}

// Return false if not all the needed wads have been loaded.
bool G_CheckSaveGameWads (PNGHandle *png, bool printwarn)
{
	char *text;
	bool printRequires = false;

	text = M_GetPNGText (png, "Game WAD");
	CheckSingleWad (text, printRequires, printwarn);
	text = M_GetPNGText (png, "Map WAD");
	CheckSingleWad (text, printRequires, printwarn);

	if (printRequires)
	{
		if (printwarn)
		{
			Printf ("\n");
		}
		return false;
	}

	return true;
}


void G_DoLoadGame ()
{
	char sigcheck[20];
	char *text = NULL;
	char *map;
	bool hidecon;

	if (gameaction != ga_autoloadgame)
	{
		demoplayback = false;
	}
	hidecon = gameaction == ga_loadgamehidecon;
	gameaction = ga_nothing;

	FILE *stdfile = fopen (savename.GetChars(), "rb");
	if (stdfile == NULL)
	{
		Printf ("Could not read savegame '%s'\n", savename.GetChars());
		return;
	}

	PNGHandle *png = M_VerifyPNG (stdfile);
	if (png == NULL)
	{
		fclose (stdfile);
		Printf ("'%s' is not a valid (PNG) savegame\n", savename.GetChars());
		return;
	}

	SaveVersion = 0;

	// Check whether this savegame actually has been created by a compatible engine.
	// Since there are ZDoom derivates using the exact same savegame format but
	// with mutual incompatibilities this check simplifies things significantly.
	char *engine = M_GetPNGText (png, "Engine");
	// [BB] Use GetEngineString() to distinguish Software from GL saves.
	if (engine == NULL || 0 != strcmp (engine, GetEngineString() ))
	{
		// Make a special case for the message printed for old savegames that don't
		// have this information.
		if (engine == NULL)
		{
			Printf ("Savegame is from an incompatible version\n");
		}
		else
		{
			Printf ("Savegame is from another ZDoom-based engine: %s\n", engine);
			delete[] engine;
		}
		delete png;
		fclose (stdfile);
		return;
	}
	if (engine != NULL)
	{
		delete[] engine;
	}

	SaveVersion = 0;
	if (!M_GetPNGText (png, "ZDoom Save Version", sigcheck, 20) ||
		0 != strncmp (sigcheck, SAVESIG, 9) ||		// ZDOOMSAVE is the first 9 chars
		(SaveVersion = atoi (sigcheck+9)) < MINSAVEVER)
	{
		delete png;
		fclose (stdfile);
		Printf ("Savegame is from an incompatible version");
		if (SaveVersion != 0)
		{
			Printf(": %d (%d is the oldest supported)", SaveVersion, MINSAVEVER);
		}
		Printf("\n");
		return;
	}

	if (!G_CheckSaveGameWads (png, true))
	{
		fclose (stdfile);
		return;
	}

	map = M_GetPNGText (png, "Current Map");
	if (map == NULL)
	{
		Printf ("Savegame is missing the current map\n");
		fclose (stdfile);
		return;
	}

	// Now that it looks like we can load this save, hide the fullscreen console if it was up
	// when the game was selected from the menu.
	if (hidecon && gamestate == GS_FULLCONSOLE)
	{
		gamestate = GS_HIDECONSOLE;
	}

	// [BB] Remove all bots that are currently used. This isn't done automatically.
	BOTS_RemoveAllBots ( false );

	// Read intermission data for hubs
	G_ReadHubInfo(png);

	text = M_GetPNGText (png, "Important CVARs");
	if (text != NULL)
	{
		BYTE *vars_p = (BYTE *)text;
		C_ReadCVars (&vars_p);
		delete[] text;
	}

	// dearchive all the modifications
	if (M_FindPNGChunk (png, MAKE_ID('p','t','I','c')) == 8)
	{
		DWORD time[2];
		fread (&time, 8, 1, stdfile);
		time[0] = BigLong((unsigned int)time[0]);
		time[1] = BigLong((unsigned int)time[1]);
		level.time = Scale (time[1], TICRATE, time[0]);
	}
	else
	{ // No ptIc chunk so we don't know how long the user was playing
		level.time = 0;
	}

	G_ReadSnapshots (png);
	STAT_Read(png);
	FRandom::StaticReadRNGState (png);
	P_ReadACSDefereds (png);

	// load a base level
	savegamerestore = true;		// Use the player actors in the savegame
	bool demoplaybacksave = demoplayback;
	G_InitNew (map, false);
	demoplayback = demoplaybacksave;
	delete[] map;
	savegamerestore = false;

	P_ReadACSVars(png);

	// [BC] Read the invasion state, etc.
	if ( invasion )
		INVASION_ReadSaveInfo( png );

	// [BB] Restore the netstate.
	if ( ( NETWORK_GetState( ) == NETSTATE_SINGLE ) || ( NETWORK_GetState( ) == NETSTATE_SINGLE_MULTIPLAYER ) )
	{
		if (M_FindPNGChunk (png, MAKE_ID('m','p','E','m')) == 1)
		{
			BYTE multiplayerEmulation;
			fread (&multiplayerEmulation, 1, 1, stdfile);
			if ( multiplayerEmulation )
				NETWORK_SetState( NETSTATE_SINGLE_MULTIPLAYER );
			else
				NETWORK_SetState( NETSTATE_SINGLE );
		}
	}

	NextSkill = -1;
	if (M_FindPNGChunk (png, MAKE_ID('s','n','X','t')) == 1)
	{
		BYTE next;
		fread (&next, 1, 1, stdfile);
		NextSkill = next;
	}

	if (level.info->snapshot != NULL)
	{
		delete level.info->snapshot;
		level.info->snapshot = NULL;
	}

	BackupSaveName = savename;

	delete png;
	fclose (stdfile);

	demoplayback = false;
	usergame = true;
	// At this point, the GC threshold is likely a lot higher than the
	// amount of memory in use, so bring it down now by starting a
	// collection.
	GC::StartCollection();
}


//
// G_SaveGame
// Called by the menu task.
// Description is a 24 byte text string
//
void G_SaveGame (const char *filename, const char *description)
{
	if (sendsave || gameaction == ga_savegame)
	{
		Printf ("A game save is still pending.\n");
		return;
	}
	savegamefile = filename;
	strncpy (savedescription, description, sizeof(savedescription)-1);
	savedescription[sizeof(savedescription)-1] = '\0';
	sendsave = true;
}

FString G_BuildSaveName (const char *prefix, int slot)
{
	FString name;
	FString leader;
	const char *slash = "";

	leader = Args->CheckValue ("-savedir");
	if (leader.IsEmpty())
	{
		leader = save_dir;
		if (leader.IsEmpty())
		{
			leader = M_GetSavegamesPath();
		}
	}
	size_t len = leader.Len();
	if (leader[0] != '\0' && leader[len-1] != '\\' && leader[len-1] != '/')
	{
		slash = "/";
	}
	name << leader << slash;
	name = NicePath(name);
	CreatePath(name);
	name << prefix;
	if (slot >= 0)
	{
		name.AppendFormat("%d.zds", slot);
	}
	return name;
}

CVAR (Int, autosavenum, 0, CVAR_NOSET|CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
static int nextautosave = -1;
CVAR (Int, disableautosave, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CUSTOM_CVAR (Int, autosavecount, 4, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	if (self > 20)
		self = 20;
}

extern void P_CalcHeight (player_t *);

void G_DoAutoSave ()
{
	char description[SAVESTRINGSIZE];
	FString file;
	// Keep up to four autosaves at a time
	UCVarValue num;
	const char *readableTime;
	int count = autosavecount != 0 ? autosavecount : 1;
	
	if (nextautosave == -1) 
	{
		nextautosave = (autosavenum + 1) % count;
	}

	num.Int = nextautosave;
	autosavenum.ForceSet (num, CVAR_Int);

	file = G_BuildSaveName ("auto", nextautosave);

	if (!(level.flags2 & LEVEL2_NOAUTOSAVEHINT))
	{
		nextautosave = (nextautosave + 1) % count;
	}
	else
	{
		// This flag can only be used once per level
		level.flags2 &= ~LEVEL2_NOAUTOSAVEHINT;
	}

	readableTime = myasctime ();
	strcpy (description, "Autosave ");
	strncpy (description+9, readableTime+4, 12);
	description[9+12] = 0;

	G_DoSaveGame (false, file, description);
}


static void PutSaveWads (FILE *file)
{
	const char *name;

	// Name of IWAD
	name = Wads.GetWadName (FWadCollection::IWAD_FILENUM);
	M_AppendPNGText (file, "Game WAD", name);

	// Name of wad the map resides in
	if (Wads.GetLumpFile (level.lumpnum) > 1)
	{
		name = Wads.GetWadName (Wads.GetLumpFile (level.lumpnum));
		M_AppendPNGText (file, "Map WAD", name);
	}
}

static void PutSaveComment (FILE *file)
{
	char comment[256];
	const char *readableTime;
	WORD len;
	int levelTime;

	// Get the current date and time
	readableTime = myasctime ();

	strncpy (comment, readableTime, 10);
	strncpy (comment+10, readableTime+19, 5);
	strncpy (comment+15, readableTime+10, 9);
	comment[24] = 0;

	M_AppendPNGText (file, "Creation Time", comment);

	// Get level name
	//strcpy (comment, level.level_name);
	mysnprintf(comment, countof(comment), "%s - %s", level.mapname, level.LevelName.GetChars());
	len = (WORD)strlen (comment);
	comment[len] = '\n';

	// Append elapsed time
	levelTime = level.time / TICRATE;
	mysnprintf (comment + len + 1, countof(comment) - len - 1, "time: %02d:%02d:%02d",
		levelTime/3600, (levelTime%3600)/60, levelTime%60);
	comment[len+16] = 0;

	// Write out the comment
	M_AppendPNGText (file, "Comment", comment);
}

static void PutSavePic (FILE *file, int width, int height)
{
	if (width <= 0 || height <= 0 || !storesavepic)
	{
		M_CreateDummyPNG (file);
	}
	else
	{
		Renderer->WriteSavePic(&players[consoleplayer], file, width, height);
	}
}

void G_DoSaveGame (bool okForQuicksave, FString filename, const char *description)
{
	char buf[100];

	// Do not even try, if we're not in a level. (Can happen after
	// a demo finishes playback.)
	if (lines == NULL || sectors == NULL)
	{
		return;
	}

	if (demoplayback)
	{
		filename = G_BuildSaveName ("demosave.zds", -1);
	}

	insave = true;
	G_SnapshotLevel ();

	FILE *stdfile = fopen (filename, "wb");

	if (stdfile == NULL)
	{
		Printf ("Could not create savegame '%s'\n", filename.GetChars());
		insave = false;
		return;
	}

	SaveVersion = SAVEVER;
	PutSavePic (stdfile, SAVEPICWIDTH, SAVEPICHEIGHT);
	mysnprintf(buf, countof(buf), GAMENAME " %s", GetVersionString());
	M_AppendPNGText (stdfile, "Software", buf);
	// [BB] Use GetEngineString() to distinguish Software from GL saves.
	M_AppendPNGText (stdfile, "Engine", GetEngineString() );
	M_AppendPNGText (stdfile, "ZDoom Save Version", SAVESIG);
	M_AppendPNGText (stdfile, "Title", description);
	M_AppendPNGText (stdfile, "Current Map", level.mapname);
	PutSaveWads (stdfile);
	PutSaveComment (stdfile);

	// Intermission stats for hubs
	G_WriteHubInfo(stdfile);

	{
		FString vars = C_GetMassCVarString(CVAR_SERVERINFO);
		M_AppendPNGText (stdfile, "Important CVARs", vars.GetChars());
	}

	if (level.time != 0 || level.maptime != 0)
	{
		DWORD time[2] = { DWORD(BigLong(TICRATE)), DWORD(BigLong(level.time)) };
		M_AppendPNGChunk (stdfile, MAKE_ID('p','t','I','c'), (BYTE *)&time, 8);
	}

	G_WriteSnapshots (stdfile);
	STAT_Write(stdfile);
	FRandom::StaticWriteRNGState (stdfile);
	P_WriteACSDefereds (stdfile);

	P_WriteACSVars(stdfile);

	// [BC] Write the invasion state, etc.
	if ( invasion )
		INVASION_WriteSaveInfo( stdfile );

	// [BB] Save the netstate.
	if ( ( NETWORK_GetState( ) == NETSTATE_SINGLE ) || ( NETWORK_GetState( ) == NETSTATE_SINGLE_MULTIPLAYER ) )
	{
		BYTE multiplayerEmulation = !!( NETWORK_GetState( ) == NETSTATE_SINGLE_MULTIPLAYER );
		M_AppendPNGChunk (stdfile, MAKE_ID('m','p','E','m'), &multiplayerEmulation, 1);
	}

	if (NextSkill != -1)
	{
		BYTE next = NextSkill;
		M_AppendPNGChunk (stdfile, MAKE_ID('s','n','X','t'), &next, 1);
	}

	M_FinishPNG (stdfile);
	fclose (stdfile);

	M_NotifyNewSave (filename.GetChars(), description, okForQuicksave);

	// Check whether the file is ok.
	bool success = false;
	stdfile = fopen (filename.GetChars(), "rb");
	if (stdfile != NULL)
	{
		PNGHandle *pngh = M_VerifyPNG(stdfile);
		if (pngh != NULL)
		{
			success = true;
			delete pngh;
		}
		fclose(stdfile);
	}
	if (success) 
	{
		if (longsavemessages) Printf ("%s (%s)\n", GStrings("GGSAVED"), filename.GetChars());
		else Printf ("%s\n", GStrings("GGSAVED"));
	}
	else Printf(PRINT_HIGH, "Save failed\n");

	BackupSaveName = filename;

	// We don't need the snapshot any longer.
	if (level.info->snapshot != NULL)
	{
		delete level.info->snapshot;
		level.info->snapshot = NULL;
	}
		
	insave = false;
}




//
// DEMO RECORDING
//

void G_ReadDemoTiccmd (ticcmd_t *cmd, int player)
{
	int id = DEM_BAD;

	while (id != DEM_USERCMD && id != DEM_EMPTYUSERCMD)
	{
		if (!demorecording && demo_p >= zdembodyend)
		{
			// nothing left in the BODY chunk, so end playback.
			G_CheckDemoStatus ();
			break;
		}

		id = ReadByte (&demo_p);

		switch (id)
		{
		case DEM_STOP:
			// end of demo stream
			G_CheckDemoStatus ();
			break;

		case DEM_USERCMD:
			UnpackUserCmd (&cmd->ucmd, &cmd->ucmd, &demo_p);
			break;

		case DEM_EMPTYUSERCMD:
			// leave cmd->ucmd unchanged
			break;

		case DEM_DROPPLAYER:
			{
				BYTE i = ReadByte (&demo_p);
				if (i < MAXPLAYERS)
				{
					playeringame[i] = false;
				}
			}
			break;

		default:
			Net_DoCommand (id, &demo_p, player);
			break;
		}
	}
} 

bool stoprecording;

CCMD (stop)
{
	stoprecording = true;
}

extern BYTE *lenspot;

void G_WriteDemoTiccmd (ticcmd_t *cmd, int player, int buf)
{
	BYTE *specdata;
	int speclen;

	// Not in multiplayer.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		return;

	if (stoprecording)
	{ // use "stop" console command to end demo recording
		G_CheckDemoStatus ();
		if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
		{
			gameaction = ga_fullconsole;
		}
		return;
	}

	// [RH] Write any special "ticcmds" for this player to the demo
	if ((specdata = NetSpecs[player][buf].GetData (&speclen)) && gametic % ticdup == 0)
	{
		memcpy (demo_p, specdata, speclen);
		demo_p += speclen;
		NetSpecs[player][buf].SetData (NULL, 0);
	}

	// [RH] Now write out a "normal" ticcmd.
	WriteUserCmdMessage (&cmd->ucmd, &players[player].cmd.ucmd, &demo_p);

	// [RH] Bigger safety margin
	if (demo_p > demobuffer + maxdemosize - 64)
	{
		ptrdiff_t pos = demo_p - demobuffer;
		ptrdiff_t spot = lenspot - demobuffer;
		ptrdiff_t comp = democompspot - demobuffer;
		ptrdiff_t body = demobodyspot - demobuffer;
		// [RH] Allocate more space for the demo
		maxdemosize += 0x20000;
		demobuffer = (BYTE *)M_Realloc (demobuffer, maxdemosize);
		demo_p = demobuffer + pos;
		lenspot = demobuffer + spot;
		democompspot = demobuffer + comp;
		demobodyspot = demobuffer + body;
	}
}



//
// G_RecordDemo
//
void G_RecordDemo (const char* name)
{
	usergame = false;
	demoname = name;
	FixPathSeperator (demoname);
	DefaultExtension (demoname, ".lmp");
	maxdemosize = 0x20000;
	demobuffer = (BYTE *)M_Malloc (maxdemosize);
	demorecording = true; 
}


// [RH] Demos are now saved as IFF FORMs. I've also removed support
//		for earlier ZDEMs since I didn't want to bother supporting
//		something that probably wasn't used much (if at all).

void G_BeginRecording (const char *startmap)
{
	int i;

	if (startmap == NULL)
	{
		startmap = level.mapname;
	}
	demo_p = demobuffer;

	WriteLong (FORM_ID, &demo_p);			// Write FORM ID
	demo_p += 4;							// Leave space for len
	WriteLong (ZDEM_ID, &demo_p);			// Write ZDEM ID

	// Write header chunk
	StartChunk (ZDHD_ID, &demo_p);
	WriteWord (DEMOGAMEVERSION, &demo_p);			// Write ZDoom version
	*demo_p++ = 2;							// Write minimum version needed to use this demo.
	*demo_p++ = 3;							// (Useful?)
	for (i = 0; i < 8; i++)					// Write name of map demo was recorded on.
	{
		*demo_p++ = startmap[i];
	}
	WriteLong (rngseed, &demo_p);			// Write RNG seed
	*demo_p++ = consoleplayer;
	FinishChunk (&demo_p);

	// Write player info chunks
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i])
		{
			StartChunk (UINF_ID, &demo_p);
			WriteByte ((BYTE)i, &demo_p);
			D_WriteUserInfoStrings (i, &demo_p);
			FinishChunk (&demo_p);
		}
	}

	// It is possible to start a "multiplayer" game with only one player,
	// so checking the number of players when playing back the demo is not
	// enough.
	if ( NETWORK_GetState( ) != NETSTATE_SINGLE )
	{
		StartChunk (NETD_ID, &demo_p);
		FinishChunk (&demo_p);
	}

	// Write cvars chunk
	StartChunk (VARS_ID, &demo_p);
	C_WriteCVars (&demo_p, CVAR_SERVERINFO|CVAR_DEMOSAVE);
	FinishChunk (&demo_p);

	// Write weapon ordering chunk
	StartChunk (WEAP_ID, &demo_p);
	P_WriteDemoWeaponsChunk(&demo_p);
	FinishChunk (&demo_p);

	// Indicate body is compressed
	StartChunk (COMP_ID, &demo_p);
	democompspot = demo_p;
	WriteLong (0, &demo_p);
	FinishChunk (&demo_p);

	// Begin BODY chunk
	StartChunk (BODY_ID, &demo_p);
	demobodyspot = demo_p;
}


//
// G_PlayDemo
//

FString defdemoname;

void G_DeferedPlayDemo (const char *name)
{
	defdemoname = name;
	gameaction = (gameaction == ga_loadgame) ? ga_loadgameplaydemo : ga_playdemo;
}

extern bool advancedemo;
CCMD (playdemo)
{
	if (argv.argc() > 1)
	{
		// [BB] CLIENTDEMO_FinishPlaying() destroy the arguments, so we have to save
		// the demo name here.
		FString demoname = argv[1];
		if ( CLIENTDEMO_IsPlaying( ))
		{
			CLIENTDEMO_FinishPlaying( );
			// [BB] CLIENTDEMO_FinishPlaying() set's advancedemo to true, but we
			// don't want to advance to the next demo, we want to play the
			// specified demo.
			advancedemo = false;
		}

		G_DeferedPlayDemo (demoname.GetChars());
		singledemo = true;
	}
}

// [BC]
CCMD( stopdemo )
{
	if ( CLIENTDEMO_IsPlaying( ))
		CLIENTDEMO_FinishPlaying( );
}

CCMD (timedemo)
{
	if (argv.argc() > 1)
	{
		G_TimeDemo (argv[1]);
		singledemo = true;
	}
}

// [RH] Process all the information in a FORM ZDEM
//		until a BODY chunk is entered.
bool G_ProcessIFFDemo (char *mapname)
{
	bool headerHit = false;
	bool bodyHit = false;
	int numPlayers = 0;
	int id, len, i;
	uLong uncompSize = 0;
	BYTE *nextchunk;

	demoplayback = true;

	for (i = 0; i < MAXPLAYERS; i++)
		playeringame[i] = 0;

	len = ReadLong (&demo_p);
	zdemformend = demo_p + len + (len & 1);

	// Check to make sure this is a ZDEM chunk file.
	// TODO: Support multiple FORM ZDEMs in a CAT. Might be useful.

	id = ReadLong (&demo_p);
	if (id != ZDEM_ID)
	{
		Printf ("Not a ZDoom demo file!\n");
		return true;
	}

	// Process all chunks until a BODY chunk is encountered.

	while (demo_p < zdemformend && !bodyHit)
	{
		id = ReadLong (&demo_p);
		len = ReadLong (&demo_p);
		nextchunk = demo_p + len + (len & 1);
		if (nextchunk > zdemformend)
		{
			Printf ("Demo is mangled!\n");
			return true;
		}

		switch (id)
		{
		case ZDHD_ID:
			headerHit = true;

			demover = ReadWord (&demo_p);	// ZDoom version demo was created with
			if (demover < MINDEMOVERSION)
			{
				Printf ("Demo requires an older version of ZDoom!\n");
				//return true;
			}
			if (ReadWord (&demo_p) > DEMOGAMEVERSION)	// Minimum ZDoom version
			{
				Printf ("Demo requires a newer version of ZDoom!\n");
				return true;
			}
			memcpy (mapname, demo_p, 8);	// Read map name
			mapname[8] = 0;
			demo_p += 8;
			rngseed = ReadLong (&demo_p);
			// Only reset the RNG if this demo is not in conjunction with a savegame.
			if (mapname[0] != 0)
			{
				FRandom::StaticClearRandom ();
			}
			consoleplayer = *demo_p++;
			break;

		case VARS_ID:
			C_ReadCVars (&demo_p);
			break;

		case UINF_ID:
			i = ReadByte (&demo_p);
			if (!playeringame[i])
			{
				playeringame[i] = 1;
				numPlayers++;
			}
			D_ReadUserInfoStrings (i, &demo_p, false);
			break;

		case NETD_ID:

			NETWORK_SetState( NETSTATE_SINGLE_MULTIPLAYER );
			break;

		case WEAP_ID:
			P_ReadDemoWeaponsChunk(&demo_p);
			break;

		case BODY_ID:
			bodyHit = true;
			zdembodyend = demo_p + len;
			break;

		case COMP_ID:
			uncompSize = ReadLong (&demo_p);
			break;
		}

		if (!bodyHit)
			demo_p = nextchunk;
	}

	if (!numPlayers)
	{
		Printf ("Demo has no players!\n");
		return true;
	}

	if (!bodyHit)
	{
		zdembodyend = NULL;
		Printf ("Demo has no BODY chunk!\n");
		return true;
	}

	if (numPlayers > 1)
		NETWORK_SetState( NETSTATE_SINGLE_MULTIPLAYER );

	if (uncompSize > 0)
	{
		BYTE *uncompressed = new BYTE[uncompSize];
		int r = uncompress (uncompressed, &uncompSize, demo_p, uLong(zdembodyend - demo_p));
		if (r != Z_OK)
		{
			Printf ("Could not decompress demo! %s\n", M_ZLibError(r).GetChars());
			delete[] uncompressed;
			return true;
		}
		M_Free (demobuffer);
		zdembodyend = uncompressed + uncompSize;
		demobuffer = demo_p = uncompressed;
	}

	return false;
}

void G_DoPlayDemo (void)
{
	char mapname[9];
	int demolump;

	gameaction = ga_nothing;

	// [RH] Allow for demos not loaded as lumps
	demolump = Wads.CheckNumForFullName (defdemoname, true);
	if (demolump >= 0)
	{
		int demolen = Wads.LumpLength (demolump);
		demobuffer = (BYTE *)M_Malloc(demolen);
		Wads.ReadLump (demolump, demobuffer);
	}
	else
	{
		FixPathSeperator (defdemoname);
		char demoname[1024];
		strncpy( demoname, defdemoname.GetChars(), 1023 );
		ForceExtension (demoname, ".cld");
		if ( M_DoesFileExist( demoname ))
		{
			// Put the game in the full console.
			gameaction = ga_fullconsole;

			CLIENTDEMO_DoPlayDemo( demoname );
			return;
		}

		DefaultExtension (defdemoname, ".lmp");
		M_ReadFileMalloc (defdemoname, &demobuffer);
	}
	demo_p = demobuffer;

	Printf ("Playing demo %s\n", defdemoname.GetChars());

	C_BackupCVars ();		// [RH] Save cvars that might be affected by demo

	if (ReadLong (&demo_p) != FORM_ID)
	{
		const char *eek = "Cannot play non-ZDoom demos.\n";

		C_ForgetCVars();
		M_Free(demobuffer);
		demo_p = demobuffer = NULL;

		if (singledemo)
		{
			I_Error ("%s", eek);
		}
		else
		{
			Printf (PRINT_BOLD, "%s", eek);
			gameaction = ga_nothing;
		}
	}
	else if (G_ProcessIFFDemo (mapname))
	{
		C_RestoreCVars();
		gameaction = ga_nothing;
		demoplayback = false;
	}
	else
	{
		// don't spend a lot of time in loadlevel 
		precache = false;
		demonew = true;
		if (mapname[0] != 0)
		{
			G_InitNew (mapname, false);
		}
		else if (numsectors == 0)
		{
			I_Error("Cannot play demo without its savegame\n");
		}
		C_HideConsole ();
		demonew = false;
		precache = true;

		usergame = false;
		demoplayback = true;
	}
}

//
// G_TimeDemo
//
void G_TimeDemo (const char* name)
{
	nodrawers = !!Args->CheckParm ("-nodraw");
	noblit = !!Args->CheckParm ("-noblit");
	timingdemo = true;
	singletics = true;

	defdemoname = name;
	gameaction = (gameaction == ga_loadgame) ? ga_loadgameplaydemo : ga_playdemo;
}


/*
===================
=
= G_CheckDemoStatus
=
= Called after a death or level completion to allow demos to be cleaned up
= Returns true if a new demo loop action will take place
===================
*/

bool G_CheckDemoStatus (void)
{
	// [BC] Support for client-side demos.
	if (!demorecording && ( CLIENTDEMO_IsRecording( ) == false ))
	{ // [RH] Restore the player's userinfo settings.
		D_SetupUserInfo();
	}

	if (demoplayback)
	{
		extern int starttime;
		int endtime = 0;

		if (timingdemo)
			endtime = I_GetTime (false) - starttime;

		C_RestoreCVars ();		// [RH] Restore cvars demo might have changed
		M_Free (demobuffer);
		demobuffer = NULL;

		P_SetupWeapons_ntohton();
		demoplayback = false;
//		netgame = false;
//		multiplayer = false;
		singletics = false;
		for (int i = 1; i < MAXPLAYERS; i++)
			playeringame[i] = 0;
		consoleplayer = 0;
		players[0].camera = NULL;
		if (StatusBar != NULL)
		{
			StatusBar->AttachToPlayer (&players[0]);
		}
		if (singledemo || timingdemo)
		{
			if (timingdemo)
			{
				// Trying to get back to a stable state after timing a demo
				// seems to cause problems. I don't feel like fixing that
				// right now.
				I_FatalError ("timed %i gametics in %i realtics (%.1f fps)\n"
							  "(This is not really an error.)", gametic,
							  endtime, (float)gametic/(float)endtime*(float)TICRATE);
			}
			else
			{
				Printf ("Demo ended.\n");
			}
			gameaction = ga_fullconsole;
			timingdemo = false;
			return false;
		}
		else
		{
			D_AdvanceDemo (); 
		}

		return true; 
	}

	if (demorecording)
	{
		BYTE *formlen;

		WriteByte (DEM_STOP, &demo_p);

		if (demo_compress)
		{
			// Now that the entire BODY chunk has been created, replace it with
			// a compressed version. If the BODY successfully compresses, the
			// contents of the COMP chunk will be changed to indicate the
			// uncompressed size of the BODY.
			uLong len = uLong(demo_p - demobodyspot);
			uLong outlen = (len + len/100 + 12);
			Byte *compressed = new Byte[outlen];
			int r = compress2 (compressed, &outlen, demobodyspot, len, 9);
			if (r == Z_OK && outlen < len)
			{
				formlen = democompspot;
				WriteLong (len, &democompspot);
				memcpy (demobodyspot, compressed, outlen);
				demo_p = demobodyspot + outlen;
			}
			delete[] compressed;
		}
		FinishChunk (&demo_p);
		formlen = demobuffer + 4;
		WriteLong (int(demo_p - demobuffer - 8), &formlen);

		bool saved = M_WriteFile (demoname, demobuffer, int(demo_p - demobuffer)); 
		M_Free (demobuffer); 
		demorecording = false;
		stoprecording = false;
		if (saved)
		{
			Printf ("Demo %s recorded\n", demoname.GetChars()); 
		}
		else
		{
			Printf ("Demo %s could not be saved\n", demoname.GetChars());
		}
	}

	return false; 
}

// [BC] New console command that freezes all actors (except the player
// who activated the cheat).

//*****************************************************************************
//
CCMD( freeze )
{
	// [Dusk] Don't allow freeze while playing a demo
	if ( CLIENTDEMO_IsPlaying( ) == true ) {
		Printf ("Cannot freeze during demo playback!\n");
		return;
	}

	if (( NETWORK_GetState( ) == NETSTATE_SINGLE ) || ( NETWORK_GetState( ) == NETSTATE_SINGLE_MULTIPLAYER ))
	{
		// Toggle the freeze mode.
		if ( level.flags2 & LEVEL2_FROZEN )
			level.flags2 &= ~LEVEL2_FROZEN;
		else
			level.flags2|= LEVEL2_FROZEN;

		Printf( "Freeze mode %s\n", ( level.flags2 & LEVEL2_FROZEN ) ? "ON" : "OFF" );

		const int freezemask = 1 << consoleplayer;
		if ( level.flags2 & LEVEL2_FROZEN )
			players[consoleplayer].timefreezer = freezemask;
		else
			players[consoleplayer].timefreezer &= ~freezemask;
	}
}
