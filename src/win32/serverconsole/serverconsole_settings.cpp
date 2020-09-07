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
// Date created: 10/7/09
//
//
// Filename: serconsole_settings.cpp
//
// Description: Win32 code for the server's new "Settings" dialog.
// Note: This file uses an experimental coding style.
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
#include "g_level.h"
#include "lastmanstanding.h"
#include "maprotation.h"
#include "network.h"
#include "resource.h"
#include "serverconsole.h"
#include "serverconsole_settings.h"
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
value_t GameModeVals[16] = {
	{ 0.0, "Cooperative" },
	{ 1.0, "Survival Cooperative" },
	{ 2.0, "Invasion" },
	{ 3.0, "Deathmatch" },
	{ 4.0, "Team Deathmatch" },
	{ 5.0, "Duel" },
	{ 6.0, "Terminator" },
	{ 7.0, "Last Man Standing" },
	{ 8.0, "Team Last Man Standing" },
	{ 9.0, "Possession" },
	{ 10.0, "Team Possession" },
	{ 11.0, "Teamgame (ACS)" },
	{ 12.0, "Capture the Flag" },
	{ 13.0, "One flag CTF" },
	{ 14.0, "Skulltag" },
	{ 15.0, "Domination" },
};

value_t GameskillVals[5] = {
	{ 0.0, "I'm too young to die." },
	{ 1.0, "Hey, not too rough." },
	{ 2.0, "Hurt me plenty." },
	{ 3.0, "Ultra-Violence." },
	{ 4.0, "Nightmare!" }
};

value_t BotskillVals[5] = {
	{ 0.0, "I want my mommy!" },
	{ 1.0, "I'm allergic to pain." },
	{ 2.0, "Bring it on." },
	{ 3.0, "I thrive off pain." },
	{ 4.0, "Nightmare!" }
};

value_t ModifierVals[3] = {
	{ 0.0, "None" },
	{ 1.0, "Instagib" },
	{ 2.0, "Buckshot" },
};

// Logging.
extern	FILE			*Logfile;
extern	char			g_szDesiredLogFilename[MAX_PATH];
extern	char			g_szActualLogFilename[MAX_PATH];
extern	GAMEMODE_e		g_CurrentGameMode;
void					StartLogging( const char *szFileName );
void					StopLogging( void );
EXTERN_CVAR( Bool, sv_logfilenametimestamp );

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- VARIABLES -------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

// References to the various dialogs.
static	HWND			g_hDlg_Dialog = NULL;
static	HWND			g_hDlg_ServerTab = NULL;
static	HWND			g_hDlg_GameplayTab = NULL;
static	HWND			g_hDlg_MessagesTab = NULL;
static	HWND			g_hDlg_AccessTab = NULL;
static	HWND			g_hDlg_AdminTab = NULL;
static	FString			g_fsMOTD;
static	ULONG			g_ulNumPWADs;

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- PROTOTYPES ------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

// (Uxtheme) Styles tabs in Windows XP/Vista. 
static	HRESULT			(__stdcall *pEnableThemeDialogTexture) ( HWND hwnd, DWORD dwFlags );

BOOL	CALLBACK		settings_Dialog_Callback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam );
BOOL	CALLBACK		settings_ServerTab_Callback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam );
BOOL	CALLBACK		settings_GameplayTab_Callback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam );
BOOL	CALLBACK		settings_AdminTab_Callback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam );
BOOL	CALLBACK		settings_AccessTab_Callback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam );

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

//==========================================================================
//
// SETTINGS_Display
//
// Creates and shows the settings dialog.
//
//==========================================================================

void SERVERCONSOLE_SETTINGS_Display( HWND hDlg )
{
	DialogBox( g_hInst, MAKEINTRESOURCE( IDD_SERVERSETTINGS_DIALOG ), hDlg, (DLGPROC)settings_Dialog_Callback );
}

//==========================================================================
//
// HandleTabSwitch
//
// Helper method to switch tabs properly.
//
//==========================================================================

BOOL settings_HandleTabSwitch( HWND hDlg, LPNMHDR nmhdr )
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
// DoLimit
//
// Helper method for SaveSettings. Updates a limit (fraglimit, winlimit, etc).
//
//==========================================================================

void settings_Dialog_DoLimit( int iResource, FIntCVar &cvar, char *szBuffer )
{
	GetDlgItemText( g_hDlg_GameplayTab, iResource, szBuffer, 1024 );
	if ( cvar != atoi( szBuffer ))
		cvar = atoi( szBuffer );
}

//==========================================================================
//
// SaveSettings
//
// Saves all the settings.
// This method is rather lengthy.
//
//==========================================================================

void settings_Dialog_SaveSettings( )
{
	char	szBuffer[1024];
	FString fsRestartMessage = "";

	//==================================
	// Save the "server" tab's settings.
	//==================================

	HWND hDlg = g_hDlg_ServerTab;

	GetDlgItemText( hDlg, IDC_SERVERNAME, szBuffer, 1024 );
	sv_hostname = szBuffer;

	if ( g_ulNumPWADs > 0 )
	{
		GetDlgItemText( hDlg, IDC_WADURL, szBuffer, 1024 );
		sv_website = szBuffer;
	}

	GetDlgItemText( hDlg, IDC_EMAIL, szBuffer, 1024 );
	sv_hostemail = szBuffer;

	sv_updatemaster = !!SendDlgItemMessage( hDlg, IDC_UPDATEMASTER, BM_GETCHECK, 0, 0 );
	sv_broadcast = !!SendDlgItemMessage( hDlg, IDC_BROADCAST, BM_GETCHECK, 0, 0 );
	sv_motd = g_fsMOTD;

	//====================================
	// Save the "gameplay" tab's settings.
	//====================================

	hDlg = g_hDlg_GameplayTab;

	// Save limits.
	settings_Dialog_DoLimit( IDC_FRAGLIMIT,  fraglimit,   szBuffer );
	settings_Dialog_DoLimit( IDC_POINTLIMIT, pointlimit,  szBuffer );
	settings_Dialog_DoLimit( IDC_WINLIMIT,   winlimit,    szBuffer );
	settings_Dialog_DoLimit( IDC_DUELLIMIT,  duellimit,   szBuffer );
	settings_Dialog_DoLimit( IDC_MAXLIVES,   sv_maxlives, szBuffer );

	// Timelimit is a float.
	GetDlgItemText( hDlg, IDC_TIMELIMIT, szBuffer, 1024 );
	// [BB] We shouldn't compare two floats with "!=".
	if ( abs( timelimit - atof( szBuffer ) ) > 1e-8 )
		timelimit = static_cast<float> ( atof( szBuffer ) );

	// Save game mode.
	if ( (LONG) GAMEMODE_GetCurrentMode( ) != SendDlgItemMessage( hDlg, IDC_GAMEPLAYMODE, CB_GETCURSEL, 0, 0 ))
	{
		fsRestartMessage += "Game mode\n";
		GAMEMODE_SetCurrentMode( (GAMEMODE_e) SendDlgItemMessage( hDlg, IDC_GAMEPLAYMODE, CB_GETCURSEL, 0, 0 ) );		
	}

	// Save modifier.
	if ( (LONG) GAMEMODE_GetModifier( ) != SendDlgItemMessage( hDlg, IDC_MODIFIER, CB_GETCURSEL, 0, 0 ))
	{
		fsRestartMessage += "Modifier\n";
		GAMEMODE_SetModifier( (MODIFIER_e) SendDlgItemMessage( hDlg, IDC_MODIFIER, CB_GETCURSEL, 0, 0 ) );
	}

	// Save skill.
	if ( SendDlgItemMessage( hDlg, IDC_SKILL, CB_GETCURSEL, 0, 0 ) != gameskill )
	{
		fsRestartMessage += "Skill\n";
		gameskill = SendDlgItemMessage( hDlg, IDC_SKILL, CB_GETCURSEL, 0, 0 );
	}

	// Save botskill.
	if ( SendDlgItemMessage( hDlg, IDC_BOTSKILL, CB_GETCURSEL, 0, 0 ) != botskill )
	{
		fsRestartMessage += "Bots' skill\n";
		botskill = SendDlgItemMessage( hDlg, IDC_BOTSKILL, CB_GETCURSEL, 0, 0 );
	}

	//====================================
	// Save the "admin" tab's settings.
	//====================================
	hDlg = g_hDlg_AdminTab;

	// Update logfile.
	GetDlgItemText( hDlg, IDC_LOGFILE, szBuffer, 256 );
	if ( stricmp( szBuffer, g_szDesiredLogFilename ) != 0 || sv_logfilenametimestamp != ( SendDlgItemMessage( hDlg, IDC_LOGFILENAME_TIMESTAMP, BM_GETCHECK, 0, 0 ) == BST_CHECKED ))
	{
		sv_logfilenametimestamp = ( SendDlgItemMessage( hDlg, IDC_LOGFILENAME_TIMESTAMP, BM_GETCHECK, 0, 0 ) == BST_CHECKED );

		if ( Logfile )
			StopLogging( );

		if ( SendDlgItemMessage( hDlg, IDC_ENABLELOGGING, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
			StartLogging( szBuffer );
	}

	GetDlgItemText( hDlg, IDC_RCONPASSWORD, szBuffer, 1024 );
	sv_rconpassword = ( SendDlgItemMessage( hDlg, IDC_ALLOWRCON, BM_GETCHECK, 0, 0 ) == BST_CHECKED ) ? szBuffer : "";

	//==================================
	// Save the "access" tab's settings.
	//==================================

	hDlg = g_hDlg_AccessTab;

	// Save voting settings.
	if ( SendDlgItemMessage( hDlg, IDC_ALLOW_CALLVOTE, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
	{
		if ( SendDlgItemMessage( hDlg, IDC_ALLOWVOTE_SPECTATOR, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
			sv_nocallvote = 0;
		else
			sv_nocallvote = 2;
	}
	else
		sv_nocallvote = 1;

	sv_noduellimitvote = !SendDlgItemMessage( hDlg, IDC_ALLOWVOTE_DUELLIMIT, BM_GETCHECK, 0, 0 );
	sv_nofraglimitvote = !SendDlgItemMessage( hDlg, IDC_ALLOWVOTE_FRAGLIMIT, BM_GETCHECK, 0, 0 );
	sv_nokickvote = !SendDlgItemMessage( hDlg, IDC_ALLOWVOTE_KICKLIMIT, BM_GETCHECK, 0, 0 );
	sv_nopointlimitvote = !SendDlgItemMessage( hDlg, IDC_ALLOWVOTE_POINTLIMIT, BM_GETCHECK, 0, 0 );
	sv_notimelimitvote = !SendDlgItemMessage( hDlg, IDC_ALLOWVOTE_TIMELIMIT, BM_GETCHECK, 0, 0 );
	sv_nowinlimitvote = !SendDlgItemMessage( hDlg, IDC_ALLOWVOTE_WINLIMIT, BM_GETCHECK, 0, 0 );
	sv_nomapvote = !SendDlgItemMessage( hDlg, IDC_ALLOWVOTE_MAP, BM_GETCHECK, 0, 0 );

	GetDlgItemText( hDlg, IDC_PASSWORD, szBuffer, 1024 );
	sv_password = szBuffer;
	sv_forcepassword = !!SendDlgItemMessage( hDlg, IDC_REQUIREPW, BM_GETCHECK, 0, 0 );

	GetDlgItemText( hDlg, IDC_JOINPASSWORD, szBuffer, 1024 );
	sv_joinpassword = szBuffer;
	sv_forcejoinpassword = !!SendDlgItemMessage( hDlg, IDC_REQUIREJOINPW, BM_GETCHECK, 0, 0 );

	GetDlgItemText( hDlg, IDC_MAXCLIENTS, szBuffer, 1024 );
	sv_maxclients = atoi( szBuffer );

	GetDlgItemText( hDlg, IDC_MAXPLAYERS, szBuffer, 1024 );
	sv_maxplayers = atoi( szBuffer );

	// Is a map change needed?
	if ( fsRestartMessage.Len( ))
	{
		fsRestartMessage = "The following settings require a map change to take effect:\n\n" + fsRestartMessage + "\nRestart the map?";
		if ( MessageBox( g_hDlg_Dialog, fsRestartMessage.GetChars( ), SERVERCONSOLE_TITLESTRING, MB_YESNO|MB_ICONQUESTION ) == IDYES )
		{
			FString String;
			String.Format( "map %s", level.mapname );	
			SERVER_AddCommand( String.GetChars( ));
		}
	}

	// Update our 'public/private/LAN' section in the statusbar.
	SERVERCONSOLE_UpdateBroadcasting( );
}

//==========================================================================
//
// DialogCallback
//
// Sets up the main settings dialog, which holds the individual tabs.
//
//==========================================================================

BOOL CALLBACK settings_Dialog_Callback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	HWND		edit;
	TCITEM		tcitem;
	RECT		tabrect, tcrect;
	LPNMHDR		nmhdr;
	HMODULE		uxtheme;

	switch (Message)
	{
	case WM_INITDIALOG:

		g_hDlg_Dialog = hDlg;
		uxtheme = LoadLibrary ("uxtheme.dll");
		if ( uxtheme != NULL )
			pEnableThemeDialogTexture = (HRESULT (__stdcall *)(HWND,DWORD))GetProcAddress (uxtheme, "EnableThemeDialogTexture");

		SendMessage( GetDlgItem( hDlg, IDC_TITLE ), WM_SETFONT, (WPARAM) CreateFont( 24, 0, 0, 0, 900, 0, 0, 0, 0, 0, 0, 0, 0, "Tahoma" ), (LPARAM) 1);

		// Set up the tab control.
		tcitem.mask = TCIF_TEXT | TCIF_PARAM;
		edit = GetDlgItem( hDlg, IDC_SETTINGSTAB );

		GetWindowRect( edit, &tcrect );
		ScreenToClient( hDlg, (LPPOINT) &tcrect.left );
		ScreenToClient( hDlg, (LPPOINT) &tcrect.right );

		TabCtrl_GetItemRect( edit, 0, &tabrect );

		// Create the tabs.
		tcitem.pszText = "Server information";
		tcitem.lParam = (LPARAM) CreateDialogParam( g_hInst, MAKEINTRESOURCE( IDD_SERVERSETTINGS_SERVERTAB ), hDlg, (DLGPROC)settings_ServerTab_Callback, (LPARAM) edit );
		TabCtrl_InsertItem( edit, 0, &tcitem );
		SetWindowPos( (HWND) tcitem.lParam, HWND_TOP, tcrect.left + 3, tcrect.top + tabrect.bottom + 3,
			tcrect.right - tcrect.left - 8, tcrect.bottom - tcrect.top - tabrect.bottom - 8, 0 );

		tcitem.pszText = "Gameplay";
		tcitem.lParam = (LPARAM) CreateDialogParam( g_hInst, MAKEINTRESOURCE( IDD_SERVERSETTINGS_GAMEPLAYTAB ), hDlg, (DLGPROC)settings_GameplayTab_Callback, (LPARAM) edit );
		TabCtrl_InsertItem( edit, 1, &tcitem );		
		SetWindowPos( (HWND) tcitem.lParam, HWND_TOP, tcrect.left + 3, tcrect.top + tabrect.bottom + 3,
			tcrect.right - tcrect.left - 8, tcrect.bottom - tcrect.top - tabrect.bottom - 8, 0 );

		tcitem.pszText = "Access";
		tcitem.lParam = (LPARAM) CreateDialogParam( g_hInst, MAKEINTRESOURCE( IDD_SERVERSETTINGS_ACCESSTAB ), hDlg, (DLGPROC)settings_AccessTab_Callback, (LPARAM) edit );
		TabCtrl_InsertItem( edit, 3, &tcitem );		
		SetWindowPos( (HWND) tcitem.lParam, HWND_TOP, tcrect.left + 3, tcrect.top + tabrect.bottom + 3,
			tcrect.right - tcrect.left - 8, tcrect.bottom - tcrect.top - tabrect.bottom - 8, 0 );

		tcitem.pszText = "Administration";
		tcitem.lParam = (LPARAM) CreateDialogParam( g_hInst, MAKEINTRESOURCE( IDD_SERVERSETTINGS_ADMINTAB ), hDlg, (DLGPROC)settings_AdminTab_Callback, (LPARAM) edit );
		TabCtrl_InsertItem( edit, 4, &tcitem );		
		SetWindowPos( (HWND) tcitem.lParam, HWND_TOP, tcrect.left + 3, tcrect.top + tabrect.bottom + 3,
			tcrect.right - tcrect.left - 8, tcrect.bottom - tcrect.top - tabrect.bottom - 8, 0 );
		break;
	case WM_NOTIFY:

		nmhdr = (LPNMHDR) lParam;

		// User clicked the tab control.
		if (nmhdr->idFrom == IDC_SETTINGSTAB )
			return settings_HandleTabSwitch( hDlg, nmhdr );

		break;

	case WM_COMMAND:

			switch ( LOWORD( wParam ))
			{				
			case IDOK:

				settings_Dialog_SaveSettings( );				
				EndDialog( hDlg, -1 );
				break;
			case IDCANCEL:

				EndDialog( hDlg, -1 );
				break;
			}
		break;
	}
	return FALSE;
}

//==========================================================================
// Server tab
//==========================================================================

//*****************************************************************************
//
BOOL CALLBACK settings_ServerTab_MOTDCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	switch ( Message )
	{
	case WM_CLOSE:

		EndDialog( hDlg, -1 );
		break;
	case WM_INITDIALOG:
		{
			SendDlgItemMessage( hDlg, IDC_MOTD, EM_SETLIMITTEXT, 512, 0 );

			// Initialize the MOTD. We have to turn "\n" into carriage returns.
			{
				FString motd = g_fsMOTD;
				motd.Substitute( "\\n", "\r\n" );
				SetDlgItemText( hDlg, IDC_MOTD, motd.GetChars() );
				SendDlgItemMessage( hDlg, IDC_MOTD, EM_SETSEL, -1, 0 ); // [RC] Why doesn't this work?
			}			
		}
		break;
	case WM_COMMAND:
		{
			switch ( LOWORD( wParam ))
			{
			case IDOK:
				{
					// MOTD. Zip up those linebreaks into \ns.
					char	szBuffer[1024];
					GetDlgItemText( hDlg, IDC_MOTD, szBuffer, 1024 );
					{						
						char	szString[1024+64];
						char	*psz;
						char	*pszString;
						char	c;

						psz = szBuffer;
						pszString = szString;
						while ( 1 )
						{
							c = *psz++;
							if ( c == 0 )
							{
								*pszString = c;
								break;
							}
							else if ( c == '\r' )
							{
								*pszString++ = '\\';
								*pszString++ = 'n';
								psz++;
							}
							else
								*pszString++ = c;
						}

						g_fsMOTD = szString;
					}
				}
				EndDialog( hDlg, -1 );
				break;
			case IDCANCEL:

				EndDialog( hDlg, -1 );
				break;
			}
		}
		break;
	default:

		return ( FALSE );
	}

	return ( TRUE );
}

//==========================================================================
//
// ServerTabCallback
//
// Sets up the "Server" tab.
//
//==========================================================================

BOOL CALLBACK settings_ServerTab_Callback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	switch ( Message )
	{
	case WM_INITDIALOG:

		g_hDlg_ServerTab = hDlg;

		// Enable tab gradients on XP and later.
		if ( pEnableThemeDialogTexture != NULL )
			pEnableThemeDialogTexture ( hDlg, ETDT_ENABLETAB );

		// Limit the input length for the text boxes.
		SendDlgItemMessage( hDlg, IDC_SERVERNAME, EM_SETLIMITTEXT, 96, 0 );
		SendDlgItemMessage( hDlg, IDC_WADURL, EM_SETLIMITTEXT, 96, 0 );
		SendDlgItemMessage( hDlg, IDC_EMAIL, EM_SETLIMITTEXT, 96, 0 );
		
		// Initialize all the fields.		
		SetDlgItemText( hDlg, IDC_SERVERNAME, sv_hostname.GetGenericRep( CVAR_String ).String );
		SetDlgItemText( hDlg, IDC_EMAIL, sv_hostemail.GetGenericRep( CVAR_String ).String );
		SetDlgItemText( hDlg, IDC_WADURL, sv_website.GetGenericRep( CVAR_String ).String );
		SendDlgItemMessage( hDlg, IDC_UPDATEMASTER, BM_SETCHECK, ( sv_updatemaster ? BST_CHECKED : BST_UNCHECKED ), 0 );
		SendDlgItemMessage( hDlg, IDC_BROADCAST, BM_SETCHECK, ( sv_broadcast ? BST_CHECKED : BST_UNCHECKED ), 0 );
		g_fsMOTD = sv_motd.GetGenericRep( CVAR_String ).String;

		// PWADs.		
		SendMessage( GetDlgItem( hDlg, IDC_PWADS ), WM_SETFONT, (WPARAM) CreateFont( 12, 0, 0, 0, 0, TRUE, 0, 0, 0, 0, 0, 0, 0, "Tahoma" ), (LPARAM) 1 );
		{
			char		szString[256];
			
			int iNumChars = 0;
			sprintf( szString, "PWADs:" );
			for ( unsigned int i = 0; i < NETWORK_GetPWADList().Size(); ++i )
			{
				const NetworkPWAD& pwad = NETWORK_GetPWADList()[i];
				iNumChars += pwad.name.Len( );
				if ( iNumChars > 50 - 3 ) // Determined by width of label
				{
					sprintf( szString + strlen ( szString ), "..." );
					break;
				}
				else
					sprintf( szString + strlen ( szString ), " %s", pwad.name.GetChars( ));
			}

			g_ulNumPWADs = NETWORK_GetPWADList().Size();
			if ( g_ulNumPWADs == 0 )
			{
				ShowWindow( GetDlgItem( hDlg, IDC_WADURL ), SW_HIDE );
				ShowWindow( GetDlgItem( hDlg, IDC_WADURLLABEL ), SW_HIDE );
				ShowWindow( GetDlgItem( hDlg, IDC_PWADS ), SW_HIDE );
				ShowWindow( GetDlgItem( hDlg, IDC_PWADGROUP ), SW_HIDE );
			}
			else
				SetDlgItemText( hDlg, IDC_PWADS, szString );
		}
		break;
	case WM_COMMAND:

		if ( LOWORD( wParam ) == IDC_STARTUPMESSAGE )
			DialogBox( g_hInst, MAKEINTRESOURCE( IDD_EDITMOTD ), hDlg, (DLGPROC)settings_ServerTab_MOTDCallback );
		break;
	}

	return FALSE;
}

//==========================================================================
// Gameplay tab
//==========================================================================

//==========================================================================
//
// ShowOrHideGameplayOptions
//
// Shows or hides various items on the "Gameplay" tab.
//
//==========================================================================

void settings_GameplayTab_ShowOrHideItems( HWND hDlg )
{
	GAMEMODE_e	GameMode;
	LONG		lInput;

	// Get the selected game mode.
	lInput = SendDlgItemMessage( hDlg, IDC_GAMEPLAYMODE, CB_GETCURSEL, 0, 0 );
	if ( lInput >= 0 && lInput < NUM_GAMEMODES )
	{
		GameMode = (GAMEMODE_e) lInput;

		// Show lives if this mode uses them.
		ShowWindow( GetDlgItem( hDlg, IDC_MAXLIVES ), ( GAMEMODE_GetFlags( GameMode ) & GMF_USEMAXLIVES ) ? SW_SHOW : SW_HIDE );
		ShowWindow( GetDlgItem( hDlg, IDC_MAXLIVESSPIN ), ( GAMEMODE_GetFlags( GameMode ) & GMF_USEMAXLIVES ) ? SW_SHOW : SW_HIDE );
		ShowWindow( GetDlgItem( hDlg, IDC_MAXLIVES_LABEL ), ( GAMEMODE_GetFlags( GameMode ) & GMF_USEMAXLIVES ) ? SW_SHOW : SW_HIDE );
	
		// Show the duellimit if Duel is selected.
		ShowWindow( GetDlgItem( hDlg, IDC_DUELLIMIT ), ( GameMode == GAMEMODE_DUEL ) ? SW_SHOW : SW_HIDE );
		ShowWindow( GetDlgItem( hDlg, IDC_DUELLIMITSPIN ), ( GameMode == GAMEMODE_DUEL ) ? SW_SHOW : SW_HIDE );
		ShowWindow( GetDlgItem( hDlg, IDC_DUELLIMIT_LABEL ), (GameMode == GAMEMODE_DUEL ) ? SW_SHOW : SW_HIDE );

		// Show pointlimit if players earn points.
		ShowWindow( GetDlgItem( hDlg, IDC_POINTLIMIT ), ( GAMEMODE_GetFlags( GameMode ) & GMF_PLAYERSEARNPOINTS ) ? SW_SHOW : SW_HIDE );
		ShowWindow( GetDlgItem( hDlg, IDC_POINTLIMITSPIN ), ( GAMEMODE_GetFlags( GameMode ) & GMF_PLAYERSEARNPOINTS ) ? SW_SHOW : SW_HIDE );
		ShowWindow( GetDlgItem( hDlg, IDC_POINTLIMIT_LABEL ), ( GAMEMODE_GetFlags( GameMode ) & GMF_PLAYERSEARNPOINTS ) ? SW_SHOW : SW_HIDE );

		// Show winlimit if players earn wins.
		ShowWindow( GetDlgItem( hDlg, IDC_WINLIMIT ), ( GAMEMODE_GetFlags( GameMode ) & GMF_PLAYERSEARNWINS || GameMode == GAMEMODE_DUEL ) ? SW_SHOW : SW_HIDE );
		ShowWindow( GetDlgItem( hDlg, IDC_WINLIMITSPIN ), ( GAMEMODE_GetFlags( GameMode ) & GMF_PLAYERSEARNWINS || GameMode == GAMEMODE_DUEL ) ? SW_SHOW : SW_HIDE );
		ShowWindow( GetDlgItem( hDlg, IDC_WINLIMIT_LABEL ), ( GAMEMODE_GetFlags( GameMode ) & GMF_PLAYERSEARNWINS || GameMode == GAMEMODE_DUEL ) ? SW_SHOW : SW_HIDE );
	}
}

//==========================================================================
//
// GameplayTabCallback
//
// Sets up the "Gameplay" tab.
//
//==========================================================================

BOOL CALLBACK settings_GameplayTab_Callback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	switch ( Message )
	{
	case WM_INITDIALOG:

		g_hDlg_GameplayTab = hDlg;

		// Enable tab gradients on XP and later.
		if ( pEnableThemeDialogTexture != NULL )
			pEnableThemeDialogTexture ( hDlg, ETDT_ENABLETAB );

		// Limit the input length for the spin boxes.
		SendDlgItemMessage( hDlg, IDC_FRAGLIMIT, EM_SETLIMITTEXT, 4, 0 );
		SendDlgItemMessage( hDlg, IDC_TIMELIMIT, EM_SETLIMITTEXT, 4, 0 );
		SendDlgItemMessage( hDlg, IDC_POINTLIMIT, EM_SETLIMITTEXT, 4, 0 );
		SendDlgItemMessage( hDlg, IDC_DUELLIMIT, EM_SETLIMITTEXT, 4, 0 );
		SendDlgItemMessage( hDlg, IDC_WINLIMIT, EM_SETLIMITTEXT, 4, 0 );
		SendDlgItemMessage( hDlg, IDC_MAXLIVES, EM_SETLIMITTEXT, 4, 0 );
		SendDlgItemMessage( hDlg, IDC_TIMELIMITSPIN, UDM_SETRANGE, 0, MAKELONG( 9999,0 ));
		SendDlgItemMessage( hDlg, IDC_FRAGLIMITSPIN, UDM_SETRANGE, 0, MAKELONG( 9999,0 ));
		SendDlgItemMessage( hDlg, IDC_POINTLIMITSPIN, UDM_SETRANGE, 0, MAKELONG( 9999,0 ));
		SendDlgItemMessage( hDlg, IDC_DUELLIMITSPIN, UDM_SETRANGE, 0, MAKELONG( 9999,0 ));
		SendDlgItemMessage( hDlg, IDC_WINLIMITSPIN, UDM_SETRANGE, 0, MAKELONG( 9999,0 ));
		SendDlgItemMessage( hDlg, IDC_MAXLIVESSPIN, UDM_SETRANGE, 0, MAKELONG( 9999,0 ));

		// Initialize all the fields.		
		SetDlgItemText( hDlg, IDC_FRAGLIMIT, fraglimit.GetGenericRep( CVAR_String ).String );
		SetDlgItemText( hDlg, IDC_TIMELIMIT, timelimit.GetGenericRep( CVAR_String ).String );
		SetDlgItemText( hDlg, IDC_POINTLIMIT, pointlimit.GetGenericRep( CVAR_String ).String );
		SetDlgItemText( hDlg, IDC_DUELLIMIT, duellimit.GetGenericRep( CVAR_String ).String );
		SetDlgItemText( hDlg, IDC_WINLIMIT, winlimit.GetGenericRep( CVAR_String ).String );
		SetDlgItemText( hDlg, IDC_MAXLIVES, sv_maxlives.GetGenericRep( CVAR_String ).String );

		// Fill the list with game modes, and select the current one.
		for ( int i = 0; i < NUM_GAMEMODES; i++ )
			SendDlgItemMessage( hDlg, IDC_GAMEPLAYMODE, CB_INSERTSTRING, -1, (WPARAM)(LPSTR) GameModeVals[i].name );
		SendDlgItemMessage( hDlg, IDC_GAMEPLAYMODE, CB_SETCURSEL, (LONG) GAMEMODE_GetCurrentMode( ), 0 );

		// Fill and select modifiers.
		for ( int i = 0; i < NUM_MODIFIERS; i++ )
			SendDlgItemMessage( hDlg, IDC_MODIFIER, CB_INSERTSTRING, -1, (WPARAM)(LPSTR) ModifierVals[i].name );
		SendDlgItemMessage( hDlg, IDC_MODIFIER, CB_SETCURSEL, (LONG) GAMEMODE_GetModifier( ), 0 );

		// Fill and select skills.
		for ( int i = 0; i < NUM_SKILLS; i++ )
			SendDlgItemMessage( hDlg, IDC_SKILL, CB_INSERTSTRING, -1, (WPARAM)(LPSTR) GameskillVals[i].name );
		SendDlgItemMessage( hDlg, IDC_SKILL, CB_SETCURSEL, gameskill.GetGenericRep( CVAR_Int ).Int, 0 );

		// Fill and select botskills.
		for ( int i = 0; i < NUM_BOTSKILLS; i++ )
			SendDlgItemMessage( hDlg, IDC_BOTSKILL, CB_INSERTSTRING, -1, (WPARAM)(LPSTR) BotskillVals[i].name );
		SendDlgItemMessage( hDlg, IDC_BOTSKILL, CB_SETCURSEL, botskill.GetGenericRep( CVAR_Int ).Int, 0 );

		settings_GameplayTab_ShowOrHideItems( hDlg );
		break;
	case WM_COMMAND:

		if ( LOWORD( wParam ) == IDC_GAMEPLAYMODE )
			settings_GameplayTab_ShowOrHideItems( hDlg );
		if ( LOWORD( wParam ) == IDC_MAPLIST )
			DialogBox( g_hInst, MAKEINTRESOURCE( IDD_MAPROTATION ), hDlg, (DLGPROC)SERVERCONSOLE_MapRotationCallback );
		if ( LOWORD( wParam ) == IDC_SHOWFLAGS )
			DialogBox( g_hInst, MAKEINTRESOURCE( IDD_DMFLAGS ), hDlg, (DLGPROC)SERVERCONSOLE_DMFlagsCallback );
		break;
	}

	return FALSE;
}

//==========================================================================
// Admin tab
//==========================================================================

//==========================================================================
//
// ShowOrHideAdminOptions
//
// Shows or hides various items on the "Admin" tab.
//
//==========================================================================

void settings_AdminTab_ShowOrHideItems( HWND hDlg )
{
	EnableWindow( GetDlgItem( hDlg, IDC_RCONPASSWORD ), ( SendDlgItemMessage( hDlg, IDC_ALLOWRCON, BM_GETCHECK, 0, 0 ) == BST_CHECKED ) );
	EnableWindow( GetDlgItem( hDlg, IDC_LOGFILE ), ( SendDlgItemMessage( hDlg, IDC_ENABLELOGGING, BM_GETCHECK, 0, 0 ) == BST_CHECKED ) );
	EnableWindow( GetDlgItem( hDlg, IDC_LOGFILENAME_TIMESTAMP ), ( SendDlgItemMessage( hDlg, IDC_ENABLELOGGING, BM_GETCHECK, 0, 0 ) == BST_CHECKED ) );	
	EnableWindow( GetDlgItem( hDlg, IDC_VIEWLOGFILE ), FileExists( g_szActualLogFilename ) );
}

//==========================================================================
//
// AdminTabCallback
//
// Sets up the "Admin" tab.
//
//==========================================================================

BOOL CALLBACK settings_AdminTab_Callback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	switch ( Message )
	{
	case WM_INITDIALOG:

		// Enable tab gradients on XP and later.
		if ( pEnableThemeDialogTexture != NULL )
			pEnableThemeDialogTexture ( hDlg, ETDT_ENABLETAB );

		g_hDlg_AdminTab = hDlg;

		// Set the text limits for the input box.
		SendDlgItemMessage( hDlg, IDC_LOGFILE, EM_SETLIMITTEXT, 128, 0 );

		// Initialize the "Enable timestamp" checkbox.		
		SendDlgItemMessage( hDlg, IDC_LOGFILENAME_TIMESTAMP, BM_SETCHECK, ( sv_logfilenametimestamp ) ? BST_CHECKED : BST_UNCHECKED, 0 );
		SendDlgItemMessage( hDlg, IDC_ENABLELOGGING, BM_SETCHECK, ( Logfile ) ? BST_CHECKED : BST_UNCHECKED, 0 );
		SetDlgItemText( hDlg, IDC_LOGFILE, g_szDesiredLogFilename );		

		// [BB] Initialize the "Server control" checkbox.
		SendDlgItemMessage( hDlg, IDC_ALLOWRCON, BM_SETCHECK, ( strlen ( sv_rconpassword ) > 4 ) ? BST_CHECKED : BST_UNCHECKED, 0 );
		SetDlgItemText( hDlg, IDC_RCONPASSWORD, sv_rconpassword );

		settings_AdminTab_ShowOrHideItems( hDlg );
		break;
	case WM_COMMAND:

		if ( LOWORD( wParam ) == IDC_TIMESTAMP || LOWORD( wParam ) == IDC_ENABLELOGGING || LOWORD( wParam ) == IDC_ALLOWRCON )
			settings_AdminTab_ShowOrHideItems( hDlg );

		if ( LOWORD( wParam ) == IDC_VIEWLOGFILE )
		{
			if ( FileExists( g_szActualLogFilename ))
				I_RunProgram( g_szActualLogFilename );
		}

		if ( LOWORD( wParam ) == IDC_MESSAGES )
			DialogBox( g_hInst, MAKEINTRESOURCE( IDD_MESSAGEOPTIONS ), hDlg, (DLGPROC)SERVERCONSOLE_MessagesCallback );
		if ( LOWORD( wParam ) == IDC_BANLIST )
			DialogBox( g_hInst, MAKEINTRESOURCE( IDD_BANLIST ), hDlg, (DLGPROC)SERVERCONSOLE_BanListCallback );

		break;
	}

	return FALSE;
}

//==========================================================================
// Access tab
//==========================================================================

//==========================================================================
//
// ShowOrHideAccessOptions
//
// Shows or hides various items on the "Access" tab.
//
//==========================================================================

void settings_AccessTab_ShowOrHideItems( HWND hDlg )
{
	EnableWindow( GetDlgItem( hDlg, IDC_PASSWORD ), ( SendDlgItemMessage( hDlg, IDC_REQUIREPW, BM_GETCHECK, 0, 0 ) == BST_CHECKED ) );
	EnableWindow( GetDlgItem( hDlg, IDC_JOINPASSWORD ), ( SendDlgItemMessage( hDlg, IDC_REQUIREJOINPW, BM_GETCHECK, 0, 0 ) == BST_CHECKED ) );	
	
	// Enable / disable all the vote options.
	bool bVotesEnabled = ( SendDlgItemMessage( hDlg, IDC_ALLOW_CALLVOTE, BM_GETCHECK, 0, 0 ) == BST_CHECKED );
	EnableWindow( GetDlgItem( hDlg, IDC_ALLOWVOTE_FRAGLIMIT ), bVotesEnabled );
	EnableWindow( GetDlgItem( hDlg, IDC_ALLOWVOTE_KICKLIMIT ), bVotesEnabled );
	EnableWindow( GetDlgItem( hDlg, IDC_ALLOWVOTE_MAP ), bVotesEnabled );
	EnableWindow( GetDlgItem( hDlg, IDC_ALLOWVOTE_POINTLIMIT ), bVotesEnabled );
	EnableWindow( GetDlgItem( hDlg, IDC_ALLOWVOTE_DUELLIMIT ), bVotesEnabled );
	EnableWindow( GetDlgItem( hDlg, IDC_ALLOWVOTE_TIMELIMIT ), bVotesEnabled );
	EnableWindow( GetDlgItem( hDlg, IDC_ALLOWVOTE_WINLIMIT ), bVotesEnabled );
	EnableWindow( GetDlgItem( hDlg, IDC_ALLOWVOTE_SPECTATOR ), bVotesEnabled );
}

//==========================================================================
//
// AccessTabCallback
//
// Sets up the "Access" tab.
//
//==========================================================================

BOOL CALLBACK settings_AccessTab_Callback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	switch ( Message )
	{
	case WM_INITDIALOG:

		// Enable tab gradients on XP and later.
		if ( pEnableThemeDialogTexture != NULL )
			pEnableThemeDialogTexture ( hDlg, ETDT_ENABLETAB );

		g_hDlg_AccessTab = hDlg;

		SendDlgItemMessage( hDlg, IDC_PASSWORD, EM_SETLIMITTEXT, 32, 0 );
		SendDlgItemMessage( hDlg, IDC_JOINPASSWORD, EM_SETLIMITTEXT, 32, 0 );
		SendDlgItemMessage( hDlg, IDC_RCONPASSWORD, EM_SETLIMITTEXT, 32, 0 );
		SendDlgItemMessage( hDlg, IDC_MAXCLIENTS, EM_SETLIMITTEXT, 4, 0 );
		SendDlgItemMessage( hDlg, IDC_MAXPLAYERS, EM_SETLIMITTEXT, 4, 0 );		
		SendDlgItemMessage( hDlg, IDC_SPIN1, UDM_SETRANGE, 0, MAKELONG( MAXPLAYERS,0 ));
		SendDlgItemMessage( hDlg, IDC_SPIN6, UDM_SETRANGE, 0, MAKELONG( MAXPLAYERS,0 ));

		SetDlgItemText( hDlg, IDC_PASSWORD, sv_password.GetGenericRep( CVAR_String ).String );
		SendDlgItemMessage( hDlg, IDC_REQUIREPW, BM_SETCHECK, sv_forcepassword ? BST_CHECKED : BST_UNCHECKED, 0 );

		SetDlgItemText( hDlg, IDC_JOINPASSWORD, sv_joinpassword.GetGenericRep( CVAR_String ).String );
		SendDlgItemMessage( hDlg, IDC_REQUIREJOINPW, BM_SETCHECK, sv_forcejoinpassword ? BST_CHECKED : BST_UNCHECKED, 0 );

		SetDlgItemText( hDlg, IDC_RCONPASSWORD, sv_rconpassword.GetGenericRep( CVAR_String ).String );
		SendDlgItemMessage( hDlg, IDC_ALLOWRCON, BM_SETCHECK, strlen( sv_rconpassword ) ? BST_CHECKED : BST_UNCHECKED, 0 );

		SetDlgItemText( hDlg, IDC_MAXPLAYERS, sv_maxplayers.GetGenericRep( CVAR_String ).String );		
		SetDlgItemText( hDlg, IDC_MAXCLIENTS, sv_maxclients.GetGenericRep( CVAR_String ).String );

		// Update voting checkboxes.
		SendDlgItemMessage( hDlg, IDC_ALLOW_CALLVOTE, BM_SETCHECK, ( sv_nocallvote != 1 ? BST_CHECKED : BST_UNCHECKED ), 0 );
		SendDlgItemMessage( hDlg, IDC_ALLOWVOTE_DUELLIMIT, BM_SETCHECK, ( !sv_noduellimitvote ? BST_CHECKED : BST_UNCHECKED ), 0 );
		SendDlgItemMessage( hDlg, IDC_ALLOWVOTE_FRAGLIMIT, BM_SETCHECK, ( !sv_nofraglimitvote ? BST_CHECKED : BST_UNCHECKED ), 0 );
		SendDlgItemMessage( hDlg, IDC_ALLOWVOTE_KICKLIMIT, BM_SETCHECK, ( !sv_nokickvote ? BST_CHECKED : BST_UNCHECKED ), 0 );
		SendDlgItemMessage( hDlg, IDC_ALLOWVOTE_POINTLIMIT, BM_SETCHECK, ( !sv_nopointlimitvote ? BST_CHECKED : BST_UNCHECKED ), 0 );
		SendDlgItemMessage( hDlg, IDC_ALLOWVOTE_TIMELIMIT, BM_SETCHECK, ( !sv_notimelimitvote ? BST_CHECKED : BST_UNCHECKED ), 0 );
		SendDlgItemMessage( hDlg, IDC_ALLOWVOTE_WINLIMIT, BM_SETCHECK, ( !sv_nowinlimitvote ? BST_CHECKED : BST_UNCHECKED ), 0 );
		SendDlgItemMessage( hDlg, IDC_ALLOWVOTE_MAP, BM_SETCHECK, ( !sv_nomapvote ? BST_CHECKED : BST_UNCHECKED ), 0 );
		SendDlgItemMessage( hDlg, IDC_ALLOWVOTE_SPECTATOR, BM_SETCHECK, ( sv_nocallvote != 2 ? BST_CHECKED : BST_UNCHECKED ), 0 );

		settings_AccessTab_ShowOrHideItems( hDlg );
		break;
	case WM_COMMAND:

		switch ( LOWORD( wParam ) )
		{
		case IDC_ALLOW_CALLVOTE:
		case IDC_REQUIREPW:
		case IDC_REQUIREJOINPW:	
		
			// Enable / disable the password fields.
			settings_AccessTab_ShowOrHideItems( hDlg );
			break;
		case IDC_BANLIST:

			DialogBox( g_hInst, MAKEINTRESOURCE( IDD_BANLIST ), hDlg, (DLGPROC)SERVERCONSOLE_BanListCallback );
			/*
			SERVERCONSOLE_IPLIST_Display( hDlg, IPLIST_BAN );			
			break;
		case IDC_WHITELIST:

			SERVERCONSOLE_IPLIST_Display( hDlg, IPLIST_WHITE );		
			break;
		case IDC_ADMINLIST:

			SERVERCONSOLE_IPLIST_Display( hDlg, IPLIST_ADMIN );
			break;
*/
		break;
		}
	}

	return FALSE;
}

#endif