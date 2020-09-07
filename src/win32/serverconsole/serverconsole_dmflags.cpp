//------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2009 Rivecoder
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
// Date created: 9/18/09
//
//
// Filename: serverconsole_dmflags.ccpp
//
// Description: Win32 code for the server's "DMFlags" dialog.
//
//----------------------------------------------------------------------------------------------------------------------------------------------------

#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>
#define USE_WINDOWS_DWORD
#include "i_system.h"
#include "network.h"
#include "callvote.h"
#include "v_text.h"
#include "c_dispatch.h"
#include "cmdlib.h"
#include "cooperative.h"
#include "deathmatch.h"
#include "duel.h"
#include "gamemode.h"
#include "lastmanstanding.h"
#include "maprotation.h"
#include "network.h"
#include "resource.h"
#include "serverconsole.h"
#include "serverconsole_dmflags.h"
#include "sv_ban.h"
#include "sv_main.h"
#include "team.h"
#include "v_text.h"
#include "version.h"
#include "network.h"
#include "survival.h"
#ifdef	GUI_SERVER_CONSOLE

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- EXTERNALS -------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

// Global instance of Skulltag.
extern	HINSTANCE		g_hInst;

// References to the names defined in m_options.cpp (to reduce code duplication).
// [BB] FIXME
value_t DF_Crouch[3] = {
	{ 0, "Default" },
	{ DF_NO_CROUCH, "Off" },
	{ DF_YES_CROUCH, "On" }
};
value_t DF_Jump[3] = {
	{ 0, "Default" },
	{ DF_NO_JUMP, "Off" },
	{ DF_YES_JUMP, "On" }
};

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- VARIABLES -------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

// References to the dialogs.
static	HWND			g_hDlg = NULL;
static	HWND			g_hDlg_LMS = NULL;

// Stored values of cvars (we update them when the user hits "OK" (if they changed))
static	ULONG			g_ulCompatFlags;
static	ULONG			g_ulCompatFlags2;
static	ULONG			g_ulDMFlags;
static	ULONG			g_ulDMFlags2;
static	ULONG			g_ulZADMFlags;
static	ULONG			g_ulLMSAllowedWeapons;
static	ULONG			g_ulLMSSpectatorSettings;

//==================================================================================
// [RC] This big map of fun ties all of the DMFlags to their respective checkboxes.
//==================================================================================
#define NUMBER_OF_FLAGS 99

static	FLAGMAPPING_t	g_Flags[NUMBER_OF_FLAGS] = 
{
	// If a flag does not have a checkbox, simply use NULL:
	// { DF_NO_ITEMS,					NULL,							&g_ulDMFlags, },
	{ DF_NO_HEALTH,						IDC_NO_HEALTH1,					&g_ulDMFlags, },	
	{ DF_WEAPONS_STAY,					IDC_WEAPONS_STAY,				&g_ulDMFlags, },		
	{ DF_SPAWN_FARTHEST,				IDC_SPAWN_FARTHEST,				&g_ulDMFlags, },	
	{ DF_FORCE_RESPAWN,					IDC_FORCE_RESPAWN,				&g_ulDMFlags, },	
	{ DF_NO_ARMOR,						IDC_NO_ARMOR,					&g_ulDMFlags, },
	{ DF_INFINITE_AMMO,					IDC_INFINITE_AMMO,				&g_ulDMFlags, },
	{ DF_NO_MONSTERS,					IDC_NO_MONSTERS,				&g_ulDMFlags, },
	{ DF_MONSTERS_RESPAWN,				IDC_MONSTERS_RESPAWN,			&g_ulDMFlags, },
	{ DF_ITEMS_RESPAWN,					IDC_ITEMS_RESPAWN,				&g_ulDMFlags, },
	{ DF_FAST_MONSTERS,					IDC_FAST_MONSTERS,				&g_ulDMFlags, },	
	{ DF_NO_FREELOOK,					IDC_NO_FREELOOK,				&g_ulDMFlags, },
	{ DF_RESPAWN_SUPER,					IDC_RESPAWN_SUPER,				&g_ulDMFlags, },
	{ DF_NO_FOV,						IDC_NO_FOV,						&g_ulDMFlags, },
	{ DF_NO_COOP_WEAPON_SPAWN,			IDC_NO_COOP_MP_WEAPON_SPAWN,	&g_ulDMFlags, },
	{ DF_COOP_LOSE_INVENTORY,			IDC_COOP_LOSE_INVENTORY,		&g_ulDMFlags, },
	{ DF_COOP_LOSE_KEYS,				IDC_LOSE_KEYS,					&g_ulDMFlags, },
	{ DF_COOP_LOSE_WEAPONS,				IDC_LOSE_WEAPONS,				&g_ulDMFlags, },
	{ DF_COOP_LOSE_ARMOR,				IDC_COOP_LOSE_ARMOR,			&g_ulDMFlags, },
	{ DF_COOP_LOSE_POWERUPS,			IDC_COOP_LOSE_POWERUPS,			&g_ulDMFlags, },
	{ DF_COOP_LOSE_AMMO,				IDC_COOP_LOSE_AMMO,				&g_ulDMFlags, },
	{ DF_COOP_HALVE_AMMO,				IDC_COOP_HALVE_AMMO,			&g_ulDMFlags, },
	{ DF2_YES_WEAPONDROP,				IDC_YES_WEAPONDROP,				&g_ulDMFlags2, },
	{ DF2_NO_RUNES,						IDC_NO_RUNES,					&g_ulDMFlags2, },
	{ DF2_INSTANT_RETURN,				IDC_INSTANT_RETURN,				&g_ulDMFlags2, },
	{ DF2_NO_TEAM_SWITCH,				IDC_NO_TEAM_SWITCH,				&g_ulDMFlags2, },
	{ DF2_NO_TEAM_SELECT,				IDC_NO_TEAM_SELECT,				&g_ulDMFlags2, },
	{ DF2_YES_DOUBLEAMMO,				IDC_YES_DOUBLEAMMO,				&g_ulDMFlags2, },
	{ DF2_YES_DEGENERATION,				IDC_YES_DEGENERATION,			&g_ulDMFlags2, },
	{ DF2_YES_FREEAIMBFG,				IDC_YES_FREEAIMBFG,				&g_ulDMFlags2, },
	{ DF2_BARRELS_RESPAWN,				IDC_BARRELS_RESPAWN,			&g_ulDMFlags2, },
	{ DF2_NO_RESPAWN_INVUL,				IDC_NO_RESPAWN_INVUL,			&g_ulDMFlags2, },
	{ DF2_COOP_SHOTGUNSTART,			IDC_SHOTGUN_START,				&g_ulDMFlags2, },
	{ DF2_SAME_SPAWN_SPOT,				IDC_SAME_SPAWN_SPOT,			&g_ulDMFlags2, },
	{ ZADF_YES_KEEP_TEAMS,				IDC_ZADF_YES_KEEP_TEAMS,		&g_ulZADMFlags, },
	{ DF2_YES_KEEPFRAGS,				IDC_DF2_YES_KEEP_FRAGS,			&g_ulDMFlags2, },
	{ DF2_NO_RESPAWN,					IDC_DF2_NO_RESPAWN,				&g_ulDMFlags2, },
	{ DF2_YES_LOSEFRAG,					IDC_DF2_YES_LOSEFRAG,			&g_ulDMFlags2, },
	{ DF2_INFINITE_INVENTORY,			IDC_DF2_INFINITE_INVENTORY,		&g_ulDMFlags2, },
	{ DF2_KILL_MONSTERS,				IDC_DF2_KILL_MONSTERS,			&g_ulDMFlags2, },
	{ DF2_NO_AUTOMAP,					IDC_DF2_NO_AUTOMAP,				&g_ulDMFlags2, },
	{ DF2_NO_AUTOMAP_ALLIES,			IDC_DF2_NO_AUTOMAP_ALLIES,		&g_ulDMFlags2, },
	{ DF2_DISALLOW_SPYING,				IDC_DF2_DISALLOW_SPYING,		&g_ulDMFlags2, },
	{ DF2_CHASECAM,						IDC_DF2_CHASECAM,				&g_ulDMFlags2, },
	{ DF2_NOSUICIDE,					IDC_DF2_NOSUICIDE,				&g_ulDMFlags2, },
	{ DF2_NOAUTOAIM,					IDC_DF2_NOAUTOAIM,				&g_ulDMFlags2, },
	{ ZADF_FORCE_GL_DEFAULTS,			IDC_ZADF_FORCE_GL_DEFAULTS,		&g_ulZADMFlags, },
	{ ZADF_NO_ROCKET_JUMPING,			IDC_ZADF_NO_ROCKET_JUMPING,		&g_ulZADMFlags, },
	{ ZADF_AWARD_DAMAGE_INSTEAD_KILLS,	IDC_ZADF_AWARD_DAMAGE_INSTEAD_KILLS,	&g_ulZADMFlags, },
	{ ZADF_FORCE_ALPHA,					IDC_ZADF_FORCE_ALPHA,			&g_ulZADMFlags, },
	{ ZADF_COOP_SP_ACTOR_SPAWN,			NULL,							&g_ulZADMFlags, },
	{ COMPATF_SHORTTEX,					IDC_SHORTTEX,					&g_ulCompatFlags, },
	{ COMPATF_STAIRINDEX,				IDC_STAIRINDEX,					&g_ulCompatFlags, },
	{ COMPATF_LIMITPAIN,				IDC_LIMITPAIN,					&g_ulCompatFlags, },
	{ COMPATF_SILENTPICKUP,				IDC_SILENTPICKUP,				&g_ulCompatFlags, },	
	{ COMPATF_NO_PASSMOBJ,				IDC_NO_PASSMOBJ,				&g_ulCompatFlags, },
	{ COMPATF_MAGICSILENCE,				IDC_MAGICSILENCE,				&g_ulCompatFlags, },
	{ COMPATF_WALLRUN,					IDC_WALLRUN,					&g_ulCompatFlags, },
	{ COMPATF_NOTOSSDROPS,				IDC_NOTOSSDROPS,				&g_ulCompatFlags, },
	{ COMPATF_USEBLOCKING,				IDC_USEBLOCKING,				&g_ulCompatFlags, },
	{ COMPATF_NODOORLIGHT,				IDC_NODOORLIGHT,				&g_ulCompatFlags, },
	{ COMPATF_RAVENSCROLL,				IDC_COMPATF_RAVENSCROLL,		&g_ulCompatFlags, },
	{ COMPATF_SOUNDTARGET,				IDC_COMPATF_SOUNDTARGET,		&g_ulCompatFlags, },
	{ COMPATF_DEHHEALTH,				IDC_COMPATF_DEHHEALTH,			&g_ulCompatFlags, },
	{ COMPATF_TRACE,					IDC_COMPATF_TRACE,				&g_ulCompatFlags, },
	{ COMPATF_DROPOFF,					IDC_COMPATF_DROPOFF,			&g_ulCompatFlags, },
	{ COMPATF_BOOMSCROLL,				IDC_COMPATF_BOOMSCROLL,			&g_ulCompatFlags, },
	{ COMPATF_INVISIBILITY,				IDC_COMPATF_INVISIBILITY,		&g_ulCompatFlags, },
	{ COMPATF_SILENT_INSTANT_FLOORS,	IDC_COMPATF_SILENT_INSTANT_FLOORS,		&g_ulCompatFlags, },
	{ COMPATF_SECTORSOUNDS,				IDC_COMPATF_SECTORSOUNDS,		&g_ulCompatFlags, },
	{ COMPATF_MISSILECLIP,				IDC_COMPATF_MISSILECLIP,		&g_ulCompatFlags, },
	{ COMPATF_CROSSDROPOFF,				IDC_COMPATF_CROSSDROPOFF,		&g_ulCompatFlags, },
	{ ZACOMPATF_LIMITED_AIRMOVEMENT,		IDC_LIMITED_AIRMOVEMENT,		&g_ulCompatFlags2, },
	{ ZACOMPATF_PLASMA_BUMP_BUG,			IDC_PLASMA_BUMP_BUG,			&g_ulCompatFlags2, },
	{ ZACOMPATF_INSTANTRESPAWN,			IDC_INSTANTRESPAWN,				&g_ulCompatFlags2, },
	{ ZACOMPATF_DISABLETAUNTS,			IDC_DISABLETAUNTS,				&g_ulCompatFlags2, },
	{ ZACOMPATF_ORIGINALSOUNDCURVE,		IDC_ORIGINALSOUNDCURVE,			&g_ulCompatFlags2, },
	{ ZACOMPATF_OLDINTERMISSION,			IDC_OLDINTERMISSION,			&g_ulCompatFlags2, },
	{ ZACOMPATF_DISABLESTEALTHMONSTERS,	IDC_DISABLESTEALTHMONSTERS,		&g_ulCompatFlags2, },	
	{ ZACOMPATF_OLDRADIUSDMG,				IDC_COMPATF_OLDRADIUSDMG,		&g_ulCompatFlags2, },
	{ ZACOMPATF_NO_CROSSHAIR,				IDC_COMPATF_NO_CROSSHAIR,		&g_ulCompatFlags2, },
	{ ZACOMPATF_OLD_WEAPON_SWITCH,		IDC_COMPATF_OLD_WEAPON_SWITCH,	&g_ulCompatFlags2, },
	{ ZACOMPATF_NETSCRIPTS_ARE_CLIENTSIDE,IDC_ZACOMPATF_NETSCRIPTS_ARE_CLIENTSIDE, &g_ulCompatFlags2, },
	{ ZACOMPATF_CLIENTS_SEND_FULL_BUTTON_INFO, NULL,						&g_ulCompatFlags2, },
	{ ZACOMPATF_NO_LAND,					NULL,							&g_ulCompatFlags2, },
	{ ZACOMPATF_OLD_RANDOM_GENERATOR,					NULL,							&g_ulCompatFlags2, },
	{ ZACOMPATF_NOGRAVITY_SPHERES,					NULL,							&g_ulCompatFlags2, },
	{ ZACOMPATF_DONT_STOP_PLAYER_SCRIPTS_ON_DISCONNECT,					NULL,							&g_ulCompatFlags2, },
	{ LMS_AWF_CHAINSAW,					IDC_LMS_ALLOWCHAINSAW,			&g_ulLMSAllowedWeapons, },
	{ LMS_AWF_PISTOL,					IDC_LMS_ALLOWPISTOL,			&g_ulLMSAllowedWeapons, },
	{ LMS_AWF_SHOTGUN,					IDC_LMS_ALLOWSHOTGUN,			&g_ulLMSAllowedWeapons, },
	{ LMS_AWF_SSG,						IDC_LMS_ALLOWSSG,				&g_ulLMSAllowedWeapons, },
	{ LMS_AWF_CHAINGUN,					IDC_LMS_ALLOWCHAINGUN,			&g_ulLMSAllowedWeapons, },
	{ LMS_AWF_MINIGUN,					IDC_LMS_ALLOWMINIGUN,			&g_ulLMSAllowedWeapons, },
	{ LMS_AWF_ROCKETLAUNCHER,			IDC_LMS_ALLOWROCKETLAUNCHER,	&g_ulLMSAllowedWeapons, },
	{ LMS_AWF_GRENADELAUNCHER,			IDC_LMS_ALLOWGRENADELAUNCHER,	&g_ulLMSAllowedWeapons, },
	{ LMS_AWF_PLASMA,					IDC_LMS_ALLOWPLASMA,			&g_ulLMSAllowedWeapons, },
	{ LMS_AWF_RAILGUN,					IDC_LMS_ALLOWRAILGUN,			&g_ulLMSAllowedWeapons, },
	{ LMS_SPF_VIEW,						IDC_LMS_SPECTATORVIEW,			&g_ulLMSSpectatorSettings, },
	{ LMS_SPF_CHAT,						IDC_LMS_SPECTATORTALK,			&g_ulLMSSpectatorSettings, },
};

//================================================
// [RC] And here we define all of the drop downs.
//================================================

//*****************************************************************************
// [RC] We're using the m_menu.h definition because we're going to re-use all that code. Falling damage just isn't defined there yet.
value_t DF_FallingDamage[4] = {
	{ 0, "None" },
	{ DF_FORCE_FALLINGZD, "Old (ZDoom)" },
	{ DF_FORCE_FALLINGHX, "Hexen" },
	{ DF_FORCE_FALLINGST, "Strife" }
};

//*****************************************************************************
//
value_t DF_LevelExit[3] = {
	{ 0, "Continue to the next map" },
	{ DF_SAME_LEVEL, "Restart the current level" },
	{ DF_NO_EXIT, "Kill the player" },
};

//*****************************************************************************
//
#define NUMBER_OF_DROPDOWNS 4
static MULTIFLAG_t g_MultiFlags[NUMBER_OF_DROPDOWNS] = 
{
	{ DF_FallingDamage, 4, IDC_FALLINGDAMAGE, &g_ulDMFlags, },
	{ DF_Crouch, 3, IDC_DMF_CROUCHING, &g_ulDMFlags, },
	{ DF_Jump, 3, IDC_DMF_JUMPING, &g_ulDMFlags, },
	{ DF_LevelExit, 3, IDC_DMF_EXIT1, &g_ulDMFlags, },

};

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- PROTOTYPES ------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

// (Uxtheme) Styles tabs in Windows XP and up. 
static	HRESULT			(__stdcall *pEnableThemeDialogTexture)( HWND hwnd, DWORD dwFlags );

static	void			flags_ReadValuesFromForm( void );
static	void			flags_WriteValuesToForm( void );
static	void			flags_ReadNewValue( HWND hDlg, int iControlID, ULONG &ulFlags );
static	BOOL			flags_HandleTabSwitch( HWND hDlg, LPNMHDR nmhdr );
static	void			flags_InsertTab( char *pszTitle, int iResource, HWND hDlg, TCITEM tcitem, HWND edit, RECT tabrect, RECT tcrect, int &index );
BOOL	CALLBACK		flags_GenericTabCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam );

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

//==========================================================================
//
// DMFlagsCallback
//
// The callback for the main dialog itself.
//
//==========================================================================

BOOL CALLBACK SERVERCONSOLE_DMFlagsCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	switch ( Message )
	{
	case WM_CLOSE:

		EndDialog( hDlg, -1 );
		break;
	case WM_INITDIALOG:
		{
			// Enable tab gradients on XP and later.
			HMODULE uxtheme = LoadLibrary ("uxtheme.dll");
			if ( uxtheme != NULL )
				pEnableThemeDialogTexture = (HRESULT (__stdcall *)(HWND,DWORD))GetProcAddress (uxtheme, "EnableThemeDialogTexture");

			g_hDlg = hDlg;
			g_ulDMFlags = dmflags;
			g_ulDMFlags2 = dmflags2;
			g_ulZADMFlags = zadmflags;
			g_ulCompatFlags = compatflags;
			g_ulCompatFlags2 = zacompatflags;
			g_ulLMSAllowedWeapons = lmsallowedweapons;
			g_ulLMSSpectatorSettings = lmsspectatorsettings;

			SendDlgItemMessage( hDlg, IDC_DMFLAGS_VALUE, EM_SETLIMITTEXT, 12, 0 );
			SendDlgItemMessage( hDlg, IDC_DMFLAGS2_VALUE, EM_SETLIMITTEXT, 12, 0 );
			SendDlgItemMessage( hDlg, IDC_COMPATFLAGS_VALUE, EM_SETLIMITTEXT, 12, 0 );
			SendDlgItemMessage( hDlg, IDC_COMPATFLAGS2_VALUE, EM_SETLIMITTEXT, 12, 0 );
			SendMessage( GetDlgItem( hDlg, IDC_TITLE ), WM_SETFONT, (WPARAM) CreateFont( 24, 0, 0, 0, 900, 0, 0, 0, 0, 0, 0, 0, 0, "Tahoma" ), (LPARAM) 1);

			// Set up the tab control.
			TCITEM		tcitem;
			tcitem.mask = TCIF_TEXT | TCIF_PARAM;
			HWND edit = GetDlgItem( hDlg, IDC_FLAGSTABCONTROL );
			RECT		tabrect, tcrect;
			GetWindowRect( edit, &tcrect );
			ScreenToClient( hDlg, (LPPOINT) &tcrect.left );
			ScreenToClient( hDlg, (LPPOINT) &tcrect.right );
			TabCtrl_GetItemRect( edit, 0, &tabrect );

			// Create the tabs.		
			int index = 0;
			flags_InsertTab( "General", IDD_DMFLAGS_GENERAL, hDlg, tcitem, edit, tabrect, tcrect, index );
			flags_InsertTab( "Players", IDD_DMFLAGS_PLAYERS, hDlg, tcitem, edit, tabrect, tcrect, index );
			flags_InsertTab( "Cooperative", IDD_DMFLAGS_COOP, hDlg, tcitem, edit, tabrect, tcrect, index );
			flags_InsertTab( "Deathmatch", IDD_DMFLAGS_DM, hDlg, tcitem, edit, tabrect, tcrect, index );
			flags_InsertTab( "LMS", IDD_DMFLAGS_LMS, hDlg, tcitem, edit, tabrect, tcrect, index );
			flags_InsertTab( "Compatibility", IDD_DMFLAGS_COMPAT, hDlg, tcitem, edit, tabrect, tcrect, index );
			
			// Check for any orphan flags that don't have checkboxes.
			for ( unsigned int i = 0; i < NUMBER_OF_FLAGS; i++ )
			{
				if ( g_Flags[i].hObject == NULL )
					g_Flags[i].bStaticValue = !!( *g_Flags[i].ulFlagVariable & g_Flags[i].ulThisFlag );				
			}

			flags_ReadValuesFromForm( ); // Repair damaged flags. Also updates the textboxes.
		}
		break;
	case WM_COMMAND:

		// User is typing in some new flags.
		if ( HIWORD ( wParam ) == EN_CHANGE )
		{
			switch ( LOWORD( wParam ))
			{
			case IDC_DMFLAGS_VALUE:

				flags_ReadNewValue( hDlg, LOWORD( wParam ), g_ulDMFlags );
				break;
			case IDC_DMFLAGS2_VALUE:

				flags_ReadNewValue( hDlg, LOWORD( wParam ), g_ulDMFlags2 );
				break;
			case IDC_COMPATFLAGS_VALUE:

				flags_ReadNewValue( hDlg, LOWORD( wParam ), g_ulCompatFlags );
				break;
			case IDC_COMPATFLAGS2_VALUE:

				flags_ReadNewValue( hDlg, LOWORD( wParam ), g_ulCompatFlags2 );
				break;
			}
		}
		else if ( HIWORD( wParam ) == EN_KILLFOCUS )
			flags_ReadValuesFromForm( );

		switch ( LOWORD( wParam ))
		{
		case IDOK:

			// Save the flags.
			if ( dmflags != g_ulDMFlags )
				dmflags = g_ulDMFlags;
			if ( dmflags2 != g_ulDMFlags2 )
				dmflags2 = g_ulDMFlags2;
			if ( zadmflags != g_ulZADMFlags )
				zadmflags = g_ulZADMFlags;
			if ( compatflags != g_ulCompatFlags )
				compatflags = g_ulCompatFlags;
			if ( zacompatflags != g_ulCompatFlags2 )
				zacompatflags = g_ulCompatFlags2;
			if ( lmsallowedweapons != g_ulLMSAllowedWeapons )
				lmsallowedweapons = g_ulLMSAllowedWeapons;
			if ( lmsspectatorsettings != g_ulLMSSpectatorSettings )
				lmsspectatorsettings = g_ulLMSSpectatorSettings;

			EndDialog( hDlg, -1 );

			// These might have changed.
			SERVERCONSOLE_SetupColumns( );
			SERVERCONSOLE_ReListPlayers( );
			break;
		case IDCANCEL:

			EndDialog( hDlg, -1 );
			break;
		}
		break;
	case WM_NOTIFY:
		{
			LPNMHDR	nmhdr = (LPNMHDR) lParam;

			// User clicked the tab control.
			if ( nmhdr->idFrom == IDC_FLAGSTABCONTROL )
				return flags_HandleTabSwitch( hDlg, nmhdr );
		}
		break;
	default:

		return ( FALSE );
	}

	return ( TRUE );
}

//==========================================================================
//
// flags_WriteValuesToForm
//
// Regenerates the checkboxes' and dropdowns' values FROM the flags.
//
//==========================================================================

static void flags_WriteValuesToForm( void )
{
	for ( unsigned int i = 0; i < NUMBER_OF_FLAGS; i++ )
		SendMessage( g_Flags[i].hObject, BM_SETCHECK, ( *g_Flags[i].ulFlagVariable & g_Flags[i].ulThisFlag ) ? BST_CHECKED : BST_UNCHECKED, 0 );

	// Update the drop-downs.
	for ( unsigned int i = 0; i < NUMBER_OF_DROPDOWNS; i++ )
	{
		// Go through the possible sub-values and find the one that's enabled.
		int largestIndex = 0, largestBit = 0;
		for ( int setting = 0; setting < g_MultiFlags[i].numPairings; setting++ )
		{
			int iBit = (int)(g_MultiFlags[i].dataPairings[setting].value);

			// This is equal to the largest fully-enabled subvalue. For example, falling damage is (8|16|24), so if 24 is on, we have to be careful not to choose 8.
			if ( (( *g_MultiFlags[i].ulFlagVariable & iBit ) == iBit ) && iBit > largestBit )
			{
				largestIndex = setting;
				largestBit = iBit;
			}
		}

		SendMessage( g_MultiFlags[i].hObject, CB_SETCURSEL, largestIndex, 0 );
	}
}

//==========================================================================
// Updates the numbers inside a "flags: ##" textbox.
// (Helper method for flags_ReadValuesFromForm.)
//
static void flags_UpdateValueLabel( int iControlID, HWND hDlg, ULONG ulValue )
{
	FString fsLabel;

	fsLabel.Format( "%ld", ulValue );
	SetDlgItemText( hDlg, iControlID, fsLabel.GetChars( ));
}

//==========================================================================
//
// flags_ReadValuesFromForm
//
// Recreates the flags' values FROM the checkboxes and drop-downs.
//
//==========================================================================

static void flags_ReadValuesFromForm( void )
{
	g_ulDMFlags = 0;
	g_ulDMFlags2 = 0;
	g_ulCompatFlags = 0;
	g_ulCompatFlags2 = 0;
	g_ulLMSAllowedWeapons = 0;
	g_ulLMSSpectatorSettings = 0;

	for ( unsigned int i = 0; i < NUMBER_OF_FLAGS; i++ )
	{
		if ( g_Flags[i].hObject == NULL )
		{
			if ( g_Flags[i].bStaticValue ) // Restore the values of any orphan flags that don't have checkboxes.
				*g_Flags[i].ulFlagVariable |= g_Flags[i].ulThisFlag;
		}
		else if ( SendMessage( g_Flags[i].hObject, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
			*g_Flags[i].ulFlagVariable |= g_Flags[i].ulThisFlag;
	}
	
	// Update the drop-downs.
	for ( unsigned int i = 0; i < NUMBER_OF_DROPDOWNS; i++ )
	{
		int selectedIndex = SendMessage( g_MultiFlags[i].hObject, CB_GETCURSEL, 0, 0 );
		*g_MultiFlags[i].ulFlagVariable |= (int)( g_MultiFlags[i].dataPairings[selectedIndex].value );
	}

	// Update the textboxes.
	flags_UpdateValueLabel( IDC_DMFLAGS_VALUE, g_hDlg, g_ulDMFlags );
	flags_UpdateValueLabel( IDC_DMFLAGS2_VALUE, g_hDlg, g_ulDMFlags2 );
	flags_UpdateValueLabel( IDC_COMPATFLAGS_VALUE, g_hDlg, g_ulCompatFlags );
	flags_UpdateValueLabel( IDC_COMPATFLAGS2_VALUE, g_hDlg, g_ulCompatFlags2 );
	flags_UpdateValueLabel( IDC_LMSWEAPONS_VALUE, g_hDlg_LMS, g_ulLMSAllowedWeapons );
	flags_UpdateValueLabel( IDC_LMSSPECTATORS_VALUE, g_hDlg_LMS, g_ulLMSSpectatorSettings );
}

//==========================================================================
//
// flags_ReadNewValue
//
// Reads an entirely new flag from the "flags: ##" textboxes.
//
//==========================================================================

void flags_ReadNewValue( HWND hDlg, int iControlID, ULONG &ulFlags )
{
	char	szBuffer[1024];

	GetDlgItemText( hDlg, iControlID, szBuffer, 1024 );
	ulFlags = atoi( szBuffer );

	// Update the orphan flags that don't have checkboxes.
	for ( unsigned int i = 0; i < NUMBER_OF_FLAGS; i++ )
	{
		if ( g_Flags[i].hObject == NULL )
			g_Flags[i].bStaticValue = !!( *g_Flags[i].ulFlagVariable & g_Flags[i].ulThisFlag );				
	}

	flags_WriteValuesToForm( );
}

//==========================================================================
//
// InsertTab
//
// Helper method to insert a tab.
//
//==========================================================================

static void flags_InsertTab( char *pszTitle, int iResource, HWND hDlg, TCITEM tcitem, HWND edit, RECT tabrect, RECT tcrect, int &index )
{
	tcitem.pszText = pszTitle;
	tcitem.lParam = (LPARAM) CreateDialogParam( g_hInst, MAKEINTRESOURCE( iResource ), hDlg, (DLGPROC)flags_GenericTabCallback, (LPARAM) edit );
	TabCtrl_InsertItem( edit, index, &tcitem );
	SetWindowPos( (HWND) tcitem.lParam, HWND_TOP, tcrect.left + 3, tcrect.top + tabrect.bottom + 3,
		tcrect.right - tcrect.left - 8, tcrect.bottom - tcrect.top - tabrect.bottom - 8, 0 );
	index++;
}

//==========================================================================
//
// HandleTabSwitch
//
// Helper method to switch tabs properly.
//
//==========================================================================

static BOOL flags_HandleTabSwitch( HWND hDlg, LPNMHDR nmhdr )
{
	TCITEM		tcitem;
	HWND		edit;

	int i = TabCtrl_GetCurSel( nmhdr->hwndFrom );
	tcitem.mask = TCIF_PARAM;
	TabCtrl_GetItem( nmhdr->hwndFrom, i, &tcitem );
	edit = (HWND) tcitem.lParam;

	// The tab is right about to switch. Hide the current tab pane.
	if ( nmhdr->code == TCN_SELCHANGING )
	{
		ShowWindow( edit, SW_HIDE );
		SetWindowLongPtr( hDlg, DWLP_MSGRESULT, FALSE );
		return TRUE;
	}
	// The tab has just switched. Show the new tab pane.
	else if ( nmhdr->code == TCN_SELCHANGE )
	{
		ShowWindow ( edit, SW_SHOW );
		return TRUE;
	}
	else
		return FALSE;
}

//==========================================================================
//
// flags_GenericTabCallback
//
// Generic initializer for all the sub-tabs.
//
//==========================================================================

BOOL CALLBACK flags_GenericTabCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	switch ( Message )
	{
	case WM_INITDIALOG:

		// Enable tab gradients on XP and later.
		if ( pEnableThemeDialogTexture != NULL )
			pEnableThemeDialogTexture ( hDlg, ETDT_ENABLETAB );

		// Go through and claim the checkboxes on this tab.
		for ( unsigned int i = 0; i < NUMBER_OF_FLAGS; i++ )
		{
			if ( GetDlgItem( hDlg, g_Flags[i].iControlID ) != NULL )
			{
				g_Flags[i].hObject = GetDlgItem( hDlg, g_Flags[i].iControlID );

				// Check to see if we need to disable the flag.
				EnableWindow( g_Flags[i].hObject, !g_Flags[i].bDisabled );
				if ( g_Flags[i].bDisabled )
					*g_Flags[i].ulFlagVariable &= ~g_Flags[i].ulThisFlag;
			}
		}

		// Init the drop-down boxes.
		for ( unsigned int i = 0; i < NUMBER_OF_DROPDOWNS; i++ )
		{
			if ( GetDlgItem( hDlg, g_MultiFlags[i].iControlID ) != NULL )
			{
				g_MultiFlags[i].hObject = GetDlgItem( hDlg, g_MultiFlags[i].iControlID );
				for ( int setting = 0; setting < g_MultiFlags[i].numPairings; setting++ )
					SendDlgItemMessage( hDlg, g_MultiFlags[i].iControlID, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)g_MultiFlags[i].dataPairings[setting].name );
			}
		}

		// Are the LMS textboxes on this tab?
		if ( GetDlgItem( hDlg, IDC_LMSSPECTATORS_VALUE ) != NULL )
		{
			g_hDlg_LMS = hDlg;
			SendDlgItemMessage( hDlg, IDC_LMSWEAPONS_VALUE, EM_SETLIMITTEXT, 12, 0 );
			SendDlgItemMessage( hDlg, IDC_LMSSPECTATORS_VALUE, EM_SETLIMITTEXT, 12, 0 );
		}

		flags_WriteValuesToForm( );		
		break;
	case WM_COMMAND:

		// Check if user changed any of the checkboxes.
		for ( unsigned int i = 0; i < NUMBER_OF_FLAGS; i++ )
		{
			// They did! Update the flags.
			if ( g_Flags[i].iControlID == LOWORD( wParam ) )
			{
				flags_ReadValuesFromForm( );
				break;
			}
		}

		// Check if user changed any of the dropdowns.
		for ( unsigned int i = 0; i < NUMBER_OF_DROPDOWNS; i++ )
		{
			// They did! Update the flags.
			if ( g_MultiFlags[i].iControlID == LOWORD( wParam ) && HIWORD( wParam ) == CBN_SELCHANGE )
			{
				flags_ReadValuesFromForm( );
				break;
			}
		}

		// User is typing in some new flags.
		if ( HIWORD ( wParam ) == EN_CHANGE )
		{
			switch ( LOWORD( wParam ))
			{
			case IDC_LMSSPECTATORS_VALUE:

				flags_ReadNewValue( hDlg, LOWORD( wParam ), g_ulLMSSpectatorSettings );
				break;
			case IDC_LMSWEAPONS_VALUE:

				flags_ReadNewValue( hDlg, LOWORD( wParam ), g_ulLMSAllowedWeapons );
				break;
			}
		}
		break;
	}

	return FALSE;
}

#endif	// GUI_SERVER_CONSOLE
