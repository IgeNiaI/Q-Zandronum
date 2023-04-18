/*
** c_bind.cpp
** Functions for using and maintaining key bindings
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
*/

#include "doomtype.h"
#include "doomdef.h"
#include "cmdlib.h"
#include "c_dispatch.h"
#include "c_bind.h"
#include "g_level.h"
#include "hu_stuff.h"
#include "gi.h"
#include "configfile.h"
#include "i_system.h"
#include "d_event.h"
// [BC] New #includes.
#include "chat.h"
#include "p_local.h"
#include "p_acs.h"

#include <math.h>
#include <stdlib.h>

/* Default keybindings for Doom (and all other games)
 */
static const FBinding DefBindings[] =
{
	{ "`", "toggleconsole" },

	// Movement
	{ "w", "+forward" },
	{ "s", "+back" },
	{ "a", "+moveleft" },
	{ "d", "+moveright" },
	{ "uparrow", "+forward" },
	{ "downarrow", "+back" },
	{ "leftarrow", "+moveleft" },
	{ "rightarrow", "+moveright" },
	{ "space", "+jump" },
	{ "ctrl", "+crouch" },
	{ "c", "+crouch" },
	{ "shift", "+speed" },

	// Weapons
	{ "mouse1", "+attack" },
	{ "mouse2", "+altattack" },
	{ "1", "slot 1" },
	{ "2", "slot 2" },
	{ "3", "slot 3" },
	{ "4", "slot 4" },
	{ "5", "slot 5" },
	{ "6", "slot 6" },
	{ "7", "slot 7" },
	{ "8", "slot 8" },
	{ "9", "slot 9" },
	{ "0", "slot 0" },
	{ "mwheeldown", "weapnext" },
	{ "mwheelup", "weapprev" },
	{ "g", "weapdrop" },

	// UI
	{ "f1", "menu_help" },
	{ "f2", "menu_save" },
	{ "f3", "menu_load" },
	{ "f4", "ready" },
	{ "f6", "quicksave" },
	{ "f7", "menu_endgame" },
	{ "f8", "togglemessages" },
	{ "f9", "quickload" },
	{ "m", "togglemap" },
	{ "t", "messagemode" },
	{ "y", "messagemode2" },
	{ "tab", "+showscores" },

	// Inventory
	{ "enter", "invuse" },
	{ "[", "invprev" },
	{ "kp-", "invprev" },
	{ "]", "invnext" },
	{ "kp*", "invnext" },
	{ "'", "invdrop" },
	{ "kp/", "invdrop" },
	{ "del", "drop_upgrade" },

	// Spectating
	{ "f10", "spectate" },
	{ "f11", "spyprev" },
	{ "f12", "spynext" },

	// Etc
	{ "e", "+use" },
	{ "-", "sizedown" },
	{ "=", "sizeup" },
	{ "sysrq", "screenshot" },
	{ "pause", "pause" },
	{ "pgup", "vote_yes" },
	{ "pgdn", "vote_no" },

	{ NULL, NULL }
};

static const FBinding DefAutomapBindings[] =
{
	{ "f", "am_togglefollow" },
	{ "g", "am_togglegrid" },
	{ "p", "am_toggletexture" },
	{ "n", "am_setmark" },
	{ "c", "am_clearmarks" },
	{ "0", "am_gobig" },
	{ "rightarrow", "+am_panright" },
	{ "leftarrow", "+am_panleft" },
	{ "uparrow", "+am_panup" },
	{ "downarrow", "+am_pandown" },
	{ "-", "+am_zoomout" },
	{ "=", "+am_zoomin" },
	{ "kp-", "+am_zoomout" },
	{ "kp+", "+am_zoomin" },
	{ "mwheelup", "am_zoom 1.2" },
	{ "mwheeldown", "am_zoom -1.2" },
	{ NULL, NULL }
};



const char *KeyNames[NUM_KEYS] =
{
	// This array is dependant on the particular keyboard input
	// codes generated in i_input.c. If they change there, they
	// also need to change here. In this case, we use the
	// DirectInput codes and assume a qwerty keyboard layout.
	// See <dinput.h> for the DIK_* codes

	NULL,		"Escape",	"1",		"2",		"3",		"4",		"5",		"6",		//00
	"7",		"8",		"9",		"0",		"-",		"=",		"Backspace","Tab",		//08
	"Q",		"W",		"E",		"R",		"T",		"Y",		"U",		"I",		//10
	"O",		"P",		"[",		"]",		"Enter",	"Ctrl",		"A",		"S",		//18
	"D",		"F",		"G",		"H",		"J",		"K",		"L",		";",		//20
	"'",		"`",		"Shift",	"\\",		"Z",		"X",		"C",		"V",		//28
	"B",		"N",		"M",		",",		".",		"/",		"rShift",	"kp*",		//30
	"Alt",		"Space",	"CapsLock",	"F1",		"F2",		"F3",		"F4",		"F5",		//38
	"F6",		"F7",		"F8",		"F9",		"F10",		"NumLock",	"Scroll",	"kp7",		//40
	"kp8",		"kp9",		"kp-",		"kp4",		"kp5",		"kp6",		"kp+",		"kp1",		//48
	"kp2",		"kp3",		"kp0",		"kp.",		NULL,		NULL,		"oem102",	"F11",		//50
	"F12",		NULL,		NULL,		NULL,		NULL,		NULL,		NULL,		NULL,		//58
	NULL,		NULL,		NULL,		NULL,		"F13",		"F14",		"F15",		"F16",		//60
	NULL,		NULL,		NULL,		NULL,		NULL,		NULL,		NULL,		NULL,		//68
	"Kana",		NULL,		NULL,		"abnt_c1",	NULL,		NULL,		NULL,		NULL,		//70
	NULL,		"Convert",	NULL,		"NoConvert",NULL,		"Yen",		"abnt_c2",	NULL,		//78
	NULL,		NULL,		NULL,		NULL,		NULL,		NULL,		NULL,		NULL,		//80
	NULL,		NULL,		NULL,		NULL,		NULL,		"kp=",		NULL,		NULL,		//88
	"CircumFlex","@",		":",		"_",		"Kanji",	"Stop",		"ax",		"Unlabeled",//90
	NULL,		"PrevTrack",NULL,		NULL,		"kp-enter",	"rCtrl",	NULL,		NULL,		//98
	"Mute",		"Calculator","Play",	NULL,		"Stop",		NULL,		NULL,		NULL,		//A0
	NULL,		NULL,		NULL,		NULL,		NULL,		NULL,		"VolDown",	NULL,		//A8
	"VolUp",	NULL,		"WebHome",	"kp,",		NULL,		"kp/",		NULL,		"sysrq",	//B0
	"rAlt",		NULL,		NULL,		NULL,		NULL,		NULL,		NULL,		NULL,		//B8
	NULL,		NULL,		NULL,		NULL,		NULL,		"Pause",	NULL,		"Home",		//C0
	"UpArrow",	"PgUp",		NULL,		"LeftArrow",NULL,		"RightArrow",NULL,		"End",		//C8
	"DownArrow","PgDn",		"Ins",		"Del",		NULL,		NULL,		NULL,		NULL,		//D0
	NULL,		NULL,		NULL,		"lWin",		"rWin",		"Apps",		"Power",	"Sleep",	//D8
	NULL,		NULL,		NULL,		"Wake",		NULL,		"Search",	"Favorites","Refresh",	//E0
	"WebStop",	"WebForward","WebBack",	"MyComputer","Mail",	"MediaSelect",NULL,		NULL,		//E8
	NULL,		NULL,		NULL,		NULL,		NULL,		NULL,		NULL,		NULL,		//F0
	NULL,		NULL,		NULL,		NULL,		NULL,		NULL,		NULL,		NULL,		//F8

	// non-keyboard buttons that can be bound
	"Mouse1",	"Mouse2",	"Mouse3",	"Mouse4",		// 8 mouse buttons
	"Mouse5",	"Mouse6",	"Mouse7",	"Mouse8",

	"Joy1",		"Joy2",		"Joy3",		"Joy4",			// 128 joystick buttons!
	"Joy5",		"Joy6",		"Joy7",		"Joy8",
	"Joy9",		"Joy10",	"Joy11",	"Joy12",
	"Joy13",	"Joy14",	"Joy15",	"Joy16",
	"Joy17",	"Joy18",	"Joy19",	"Joy20",
	"Joy21",	"Joy22",	"Joy23",	"Joy24",
	"Joy25",	"Joy26",	"Joy27",	"Joy28",
	"Joy29",	"Joy30",	"Joy31",	"Joy32",
	"Joy33",	"Joy34",	"Joy35",	"Joy36",
	"Joy37",	"Joy38",	"Joy39",	"Joy40",
	"Joy41",	"Joy42",	"Joy43",	"Joy44",
	"Joy45",	"Joy46",	"Joy47",	"Joy48",
	"Joy49",	"Joy50",	"Joy51",	"Joy52",
	"Joy53",	"Joy54",	"Joy55",	"Joy56",
	"Joy57",	"Joy58",	"Joy59",	"Joy60",
	"Joy61",	"Joy62",	"Joy63",	"Joy64",
	"Joy65",	"Joy66",	"Joy67",	"Joy68",
	"Joy69",	"Joy70",	"Joy71",	"Joy72",
	"Joy73",	"Joy74",	"Joy75",	"Joy76",
	"Joy77",	"Joy78",	"Joy79",	"Joy80",
	"Joy81",	"Joy82",	"Joy83",	"Joy84",
	"Joy85",	"Joy86",	"Joy87",	"Joy88",
	"Joy89",	"Joy90",	"Joy91",	"Joy92",
	"Joy93",	"Joy94",	"Joy95",	"Joy96",
	"Joy97",	"Joy98",	"Joy99",	"Joy100",
	"Joy101",	"Joy102",	"Joy103",	"Joy104",
	"Joy105",	"Joy106",	"Joy107",	"Joy108",
	"Joy109",	"Joy110",	"Joy111",	"Joy112",
	"Joy113",	"Joy114",	"Joy115",	"Joy116",
	"Joy117",	"Joy118",	"Joy119",	"Joy120",
	"Joy121",	"Joy122",	"Joy123",	"Joy124",
	"Joy125",	"Joy126",	"Joy127",	"Joy128",

	"Pov1Up",	"Pov1Right","Pov1Down",	"Pov1Left",		// First POV hat
	"Pov2Up",	"Pov2Right","Pov2Down",	"Pov2Left",		// Second POV hat
	"Pov3Up",	"Pov3Right","Pov3Down",	"Pov3Left",		// Third POV hat
	"Pov4Up",	"Pov4Right","Pov4Down",	"Pov4Left",		// Fourth POV hat

	"MWheelUp",		"MWheelDown",							// the mouse wheel
	"MWheelRight",	"MWheelLeft",

	"Axis1Plus","Axis1Minus","Axis2Plus","Axis2Minus",	// joystick axes as buttons
	"Axis3Plus","Axis3Minus","Axis4Plus","Axis4Minus",
	"Axis5Plus","Axis5Minus","Axis6Plus","Axis6Minus",
	"Axis7Plus","Axis7Minus","Axis8Plus","Axis8Minus",

	"lStickRight","lStickLeft","lStickDown","lStickUp",			// Gamepad axis-based buttons
	"rStickRight","rStickLeft","rStickDown","rStickUp",

	"DpadUp","DpadDown","DpadLeft","DpadRight",	// Gamepad buttons
	"pad_start","pad_back","lThumb","rThumb",
	"lShoulder","rShoulder","lTrigger","rTrigger",
	"pad_a", "pad_b", "pad_x", "pad_y"
};

FKeyBindings Bindings;
FKeyBindings DoubleBindings;
FKeyBindings AutomapBindings;

static unsigned int DClickTime[NUM_KEYS];
static BYTE DClicked[(NUM_KEYS+7)/8];

//=============================================================================
//
//
//
//=============================================================================

static int GetKeyFromName (const char *name)
{
	int i;

	// Names of the form #xxx are translated to key xxx automatically
	if (name[0] == '#' && name[1] != 0)
	{
		return atoi (name + 1);
	}

	// Otherwise, we scan the KeyNames[] array for a matching name
	for (i = 0; i < NUM_KEYS; i++)
	{
		if (KeyNames[i] && !stricmp (KeyNames[i], name))
			return i;
	}
	return 0;
}

//=============================================================================
//
//
//
//=============================================================================

static int GetConfigKeyFromName (const char *key)
{
	int keynum = GetKeyFromName(key);
	if (keynum == 0)
	{
		if (stricmp (key, "LeftBracket") == 0)
		{
			keynum = GetKeyFromName ("[");
		}
		else if (stricmp (key, "RightBracket") == 0)
		{
			keynum = GetKeyFromName ("]");
		}
		else if (stricmp (key, "Equals") == 0)
		{
			keynum = GetKeyFromName ("=");
		}
		else if (stricmp (key, "KP-Equals") == 0)
		{
			keynum = GetKeyFromName ("kp=");
		}
	}
	return keynum;
}

//=============================================================================
//
//
//
//=============================================================================

static const char *KeyName (int key)
{
	static char name[5];

	if (KeyNames[key])
		return KeyNames[key];

	mysnprintf (name, countof(name), "#%d", key);
	return name;
}

//=============================================================================
//
//
//
//=============================================================================

static const char *ConfigKeyName(int keynum)
{
	const char *name = KeyName(keynum);
	if (name[1] == 0)	// Make sure given name is config-safe
	{
		if (name[0] == '[')
			return "LeftBracket";
		else if (name[0] == ']')
			return "RightBracket";
		else if (name[0] == '=')
			return "Equals";
		else if (strcmp (name, "kp=") == 0)
			return "KP-Equals";
	}
	return name;
}

//=============================================================================
//
//
//
//=============================================================================

void C_NameKeys (char *str, int first, int second)
{
	int c = 0;

	*str = 0;
	if (first)
	{
		c++;
		strcpy (str, KeyName (first));
		if (second)
			strcat (str, " or ");
	}

	if (second)
	{
		c++;
		strcat (str, KeyName (second));
	}

	if (!c)
		*str = '\0';
}

//=============================================================================
//
//
//
//=============================================================================

void FKeyBindings::DoBind (const char *key, const char *bind)
{
	int keynum = GetConfigKeyFromName (key);
	if (keynum != 0)
	{
		Binds[keynum] = bind;
	}
}

//=============================================================================
//
//
//
//=============================================================================

void FKeyBindings::SetBinds(const FBinding *binds)
{
	while (binds->Key)
	{
		DoBind (binds->Key, binds->Bind);
		binds++;
	}
}

//=============================================================================
//
//
//
//=============================================================================

void FKeyBindings::UnbindAll ()
{
	for (int i = 0; i < NUM_KEYS; ++i)
	{
		Binds[i] = "";
	}
}

//=============================================================================
//
//
//
//=============================================================================

void FKeyBindings::UnbindKey(const char *key)
{
	int i;

	if ( (i = GetKeyFromName (key)) )
	{
		Binds[i] = "";
	}
	else
	{
		Printf ("Unknown key \"%s\"\n", key);
		return;
	}
}

//=============================================================================
//
//
//
//=============================================================================

void FKeyBindings::PerformBind(FCommandLine &argv, const char *msg)
{
	int i;

	// [BC] This function may not be used by ConsoleCommand.
	if ( ACS_IsCalledFromConsoleCommand( ))
		return;

	if (argv.argc() > 1)
	{
		i = GetKeyFromName (argv[1]);
		if (!i)
		{
			Printf ("Unknown key \"%s\"\n", argv[1]);
			return;
		}
		if (argv.argc() == 2)
		{
			Printf ("\"%s\" = \"%s\"\n", argv[1], Binds[i].GetChars());
		}
		else
		{
			Binds[i] = argv[2];
		}
	}
	else
	{
		Printf ("%s:\n", msg);
		
		for (i = 0; i < NUM_KEYS; i++)
		{
			if (!Binds[i].IsEmpty())
				Printf ("%s \"%s\"\n", KeyName (i), Binds[i].GetChars());
		}
	}
}


//=============================================================================
//
// This function is first called for functions in custom key sections.
// In this case, matchcmd is non-NULL, and only keys bound to that command
// are stored. If a match is found, its binding is set to "\1".
// After all custom key sections are saved, it is called one more for the
// normal Bindings and DoubleBindings sections for this game. In this case
// matchcmd is NULL and all keys will be stored. The config section was not
// previously cleared, so all old bindings are still in place. If the binding
// for a key is empty, the corresponding key in the config is removed as well.
// If a binding is "\1", then the binding itself is cleared, but nothing
// happens to the entry in the config.
//
//=============================================================================

void FKeyBindings::ArchiveBindings(FConfigFile *f, const char *matchcmd)
{
	int i;

	for (i = 0; i < NUM_KEYS; i++)
	{
		if (Binds[i].IsEmpty())
		{
			if (matchcmd == NULL)
			{
				f->ClearKey(ConfigKeyName(i));
			}
		}
		else if (matchcmd == NULL || stricmp(Binds[i], matchcmd) == 0)
		{
			if (Binds[i][0] == '\1')
			{
				Binds[i] = "";
				continue;
			}
			f->SetValueForKey(ConfigKeyName(i), Binds[i]);
			if (matchcmd != NULL)
			{ // If saving a specific command, set a marker so that
			  // it does not get saved in the general binding list.
				Binds[i] = "\1";
			}
		}
	}
}

//=============================================================================
//
//
//
//=============================================================================

int FKeyBindings::GetKeysForCommand (const char *cmd, int *first, int *second)
{
	int c, i;

	*first = *second = c = i = 0;

	while (i < NUM_KEYS && c < 2)
	{
		if (stricmp (cmd, Binds[i]) == 0)
		{
			if (c++ == 0)
				*first = i;
			else
				*second = i;
		}
		i++;
	}
	return c;
}

//=============================================================================
//
//
//
//=============================================================================

void FKeyBindings::UnbindACommand (const char *str)
{
	int i;

	for (i = 0; i < NUM_KEYS; i++)
	{
		if (!stricmp (str, Binds[i]))
		{
			Binds[i] = "";
		}
	}
}

//=============================================================================
//
//
//
//=============================================================================

void FKeyBindings::DefaultBind(const char *keyname, const char *cmd)
{
	int key = GetKeyFromName (keyname);
	if (key == 0)
	{
		Printf ("Unknown key \"%s\"\n", keyname);
		return;
	}
	if (!Binds[key].IsEmpty())
	{ // This key is already bound.
		return;
	}
	for (int i = 0; i < NUM_KEYS; ++i)
	{
		if (!Binds[i].IsEmpty() && stricmp (Binds[i], cmd) == 0)
		{ // This command is already bound to a key.
			return;
		}
	}
	// It is safe to do the bind, so do it.
	Binds[key] = cmd;
}

//=============================================================================
//
//
//
//=============================================================================

void C_UnbindAll ()
{
	Bindings.UnbindAll();
	DoubleBindings.UnbindAll();
	AutomapBindings.UnbindAll();
}

CCMD (unbindall)
{
	// [BC] This function may not be used by ConsoleCommand.
	if ( ACS_IsCalledFromConsoleCommand( ))
		return;

	C_UnbindAll ();
}

//=============================================================================
//
//
//
//=============================================================================

CCMD (unbind)
{
	// [BC] This function may not be used by ConsoleCommand.
	if ( ACS_IsCalledFromConsoleCommand( ))
		return;

	if (argv.argc() > 1)
	{
		Bindings.UnbindKey(argv[1]);
	}
}

CCMD (undoublebind)
{
	if (argv.argc() > 1)
	{
		DoubleBindings.UnbindKey(argv[1]);
	}
}

CCMD (unmapbind)
{
	if (argv.argc() > 1)
	{
		AutomapBindings.UnbindKey(argv[1]);
	}
}

//=============================================================================
//
//
//
//=============================================================================

CCMD (bind)
{
	Bindings.PerformBind(argv, "Current key bindings");
}

CCMD (doublebind)
{
	DoubleBindings.PerformBind(argv, "Current key doublebindings");
}

CCMD (mapbind)
{
	AutomapBindings.PerformBind(argv, "Current automap key bindings");
}

//==========================================================================
//
// CCMD defaultbind
//
// Binds a command to a key if that key is not already bound and if
// that command is not already bound to another key.
//
//==========================================================================

CCMD (defaultbind)
{
	if (argv.argc() < 3)
	{
		Printf ("Usage: defaultbind <key> <command>\n");
	}
	else
	{
		Bindings.DefaultBind(argv[1], argv[2]);
	}
}

//=============================================================================
//
//
//
//=============================================================================

CCMD (rebind)
{
	FKeyBindings *bindings;

	if (key == 0)
	{
		Printf ("Rebind cannot be used from the console\n");
		return;
	}

	if (key & KEY_DBLCLICKED)
	{
		bindings = &DoubleBindings;
		key &= KEY_DBLCLICKED-1;
	}
	else
	{
		bindings = &Bindings;
	}

	if (argv.argc() > 1)
	{
		bindings->SetBind(key, argv[1]);
	}
}

//=============================================================================
//
//
//
//=============================================================================

void C_BindDefaults ()
{
	Bindings.SetBinds (DefBindings);

	AutomapBindings.SetBinds(DefAutomapBindings);
}

CCMD(binddefaults)
{
	C_BindDefaults ();
}

void C_SetDefaultBindings ()
{
	C_UnbindAll ();
	C_BindDefaults ();
}

//=============================================================================
//
//
//
//=============================================================================

bool C_DoKey (event_t *ev, FKeyBindings *binds, FKeyBindings *doublebinds)
{
	FString binding;
	bool dclick;
	int dclickspot;
	BYTE dclickmask;
	unsigned int nowtime;

	if (ev->type != EV_KeyDown && ev->type != EV_KeyUp)
		return false;

	if ((unsigned int)ev->data1 >= NUM_KEYS)
		return false;

	dclickspot = ev->data1 >> 3;
	dclickmask = 1 << (ev->data1 & 7);
	dclick = false;

	// This used level.time which didn't work outside a level.
	nowtime = I_MSTime();
	if (doublebinds != NULL && DClickTime[ev->data1] > nowtime && ev->type == EV_KeyDown)
	{
		// Key pressed for a double click
		binding = doublebinds->GetBinding(ev->data1);
		DClicked[dclickspot] |= dclickmask;
		dclick = true;
	}
	else
	{
		if (ev->type == EV_KeyDown)
		{ // Key pressed for a normal press
			binding = binds->GetBinding(ev->data1);
			DClickTime[ev->data1] = nowtime + 571;
		}
		else if (doublebinds != NULL && DClicked[dclickspot] & dclickmask)
		{ // Key released from a double click
			binding = doublebinds->GetBinding(ev->data1);
			DClicked[dclickspot] &= ~dclickmask;
			DClickTime[ev->data1] = 0;
			dclick = true;
		}
		else
		{ // Key released from a normal press
			binding = binds->GetBinding(ev->data1);
		}
	}


	if (binding.IsEmpty())
	{
		binding = binds->GetBinding(ev->data1);
		dclick = false;
	}

	// [BC] chatmodeon becomes CHAT_GetChatMode().
	if (!binding.IsEmpty() && (( CHAT_GetChatMode( ) == CHATMODE_NONE ) || ev->data1 < 256))
	{
		if (ev->type == EV_KeyUp && binding[0] != '+')
		{
			return false;
		}

		char *copy = binding.LockBuffer();

		if (ev->type == EV_KeyUp)
		{
			copy[0] = '-';
		}

		AddCommandString (copy, dclick ? ev->data1 | KEY_DBLCLICKED : ev->data1);
		return true;
	}
	return false;
}


// [RC] Returns the (first) key name, if any, used for a command 
void C_FindBind(char *Command, char *Key) {
	int key1 = -1; int key2 = -1;
	Bindings.GetKeysForCommand(Command, &key1, &key2);
	if(key1 <= 0)
		sprintf(Key, "None");
	else
		sprintf(Key, "%s", KeyNames[key1]);
}

