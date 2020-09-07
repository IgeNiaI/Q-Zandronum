//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2004 Brad Carney
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
// Created:  4/5/04
//
//
// Filename: serverconsole.cpp
//
// Description: Contains variables and routines related to the GUI server console
//
//-----------------------------------------------------------------------------

#include <time.h>
#include <windows.h>
#include <commctrl.h>
#define USE_WINDOWS_DWORD
#include "i_system.h"
#include "i_input.h"

#include "c_dispatch.h"
#include "cmdlib.h"
#include "cooperative.h"
#include "d_dehacked.h"
#include "deathmatch.h"
#include "doomstat.h"
#include "duel.h"
#include "g_level.h"
#include "lastmanstanding.h"
#include "maprotation.h"
#include "network.h"
#include "resource.h"
#include "../resource.h"
#include "serverconsole.h"
#include "serverconsole_dmflags.h"
#include "serverconsole_settings.h"
#include "sv_ban.h"
#include "sv_main.h"
#include "team.h"
#include "v_text.h"
#include "version.h"
#include "network.h"
#include "survival.h"
#include "gamemode.h"
#include "g_level.h"

#ifdef	GUI_SERVER_CONSOLE

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- VARIABLES -------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

// Global handle for the server console dialog box.
static	HWND				g_hDlg = NULL;
static	HWND				g_hDlgStatusBar = NULL;

// Global handle for the server statistic dialog box.
static	HWND				g_hStatisticDlg = NULL;

// Thread handle for D_DoomMain.
HANDLE						g_hThread;

// Number of lines present in the console box.
static	ULONG				g_ulNumLines = 0;
static	bool				g_bScrollConsoleOnNewline = false;

// Array of player indicies for the scoreboard.
static	LONG				g_lPlayerIndicies[MAXPLAYERS];

static	bool				g_bServerLoaded = false;
static	char				g_szBanEditString[256];
static	NOTIFYICONDATA		g_NotifyIconData;
static	HICON				g_hSmallIcon = NULL;
static	bool				g_bSmallIconClicked = false;
static	NETADDRESS_s		g_LocalAddress;

// [RC] Commands that the server admin sent recently.
static	std::vector<FString>	g_RecentConsoleMessages;

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- GLOBAL VARIABLES ------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

extern	HINSTANCE	g_hInst;

// For the right click|ban dialogs.
static	char		g_szScoreboard_SelectedUser[64];
static	char		g_szScoreboard_Reason[512];
static	char		g_szBanLength[64];

DWORD	WINAPI	MainDoomThread( LPVOID );
EXTERN_CVAR( Bool, sv_markchatlines );

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- PROTOTYPES ------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

static	void		serverconsole_ScoreboardRightClicked( void );
BOOL	CALLBACK	serverconsole_BanPlayerSimpleCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam );
BOOL	CALLBACK	serverconsole_BanPlayerAdvancedCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam );

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

void serverconsole_UpdateSendButton( HWND hDlg )
{
	char	szBuffer[1024];

	GetDlgItemText( hDlg, IDC_INPUTBOX, szBuffer, sizeof( szBuffer ));
	if ( szBuffer[0] == '/' || szBuffer[0] == ':' )
		EnableWindow( GetDlgItem( hDlg, IDC_SEND ), strlen( szBuffer ) > 1 && g_bServerLoaded );
	else
		EnableWindow( GetDlgItem( hDlg, IDC_SEND ), strlen( szBuffer ) > 0 && g_bServerLoaded );

	SetDlgItemText( hDlg, IDC_SEND, ( szBuffer[0] == ':' ) ? "Chat" : "Send" );		
}

//*****************************************************************************
//
BOOL CALLBACK SERVERCONSOLE_ServerDialogBoxCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	switch ( Message )
	{
	case WM_CLOSE:

		SERVERCONSOLE_Quit( );
		break;
	case WM_DESTROY:

		Shell_NotifyIcon( NIM_DELETE, &g_NotifyIconData );
		PostQuitMessage( 0 );
		break;
	case WM_INITDIALOG:
		{
			LVCOLUMN	ColumnData;
			char		szColumnTitle[64];
			LONG		lIndex;
			ULONG		ulIdx;

			// Set the global handle for the dialog box.
			g_hDlg = hDlg;

			// Load the icons.
			g_hSmallIcon = (HICON)LoadImage( g_hInst,
					MAKEINTRESOURCE( IDI_ICONST ),
					IMAGE_ICON,
					16,
					16,
					LR_SHARED );

			SendMessage( hDlg, WM_SETICON, (WPARAM)ICON_SMALL, (LPARAM)g_hSmallIcon );
			SendMessage( hDlg, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)LoadIcon( g_hInst, MAKEINTRESOURCE( IDI_ICONST )));

			// Set up the server dialog's status bar.
			g_hDlgStatusBar = CreateStatusWindow(WS_CHILD | WS_VISIBLE, (LPCTSTR)NULL, hDlg, IDC_SERVER_STATUSBAR);
			const int aDivideWidths[] = {47, 85, 195, 305, 450 };
			SendMessage(g_hDlgStatusBar, SB_SETPARTS, (WPARAM) 5, (LPARAM) aDivideWidths);			

			// Create and set our fonts for the titlescreen and console.
			HFONT hTitleFont = CreateFont(14, 0, 0, 0, 600, 0, 0, 0, 0, 0, 0, 0, 0, "Tahoma"); 
			SendMessage(GetDlgItem( g_hDlg, IDC_TITLEBOX), WM_SETFONT, (WPARAM) hTitleFont, (LPARAM) 1);

			HFONT hConsoleFont = CreateFont(12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Tahoma"); 
			SendMessage(GetDlgItem( g_hDlg, IDC_CONSOLEBOX), WM_SETFONT, (WPARAM) hConsoleFont, (LPARAM) 1);

			SendMessage( GetDlgItem( hDlg, IDC_MAPMODE ), WM_SETFONT, (WPARAM) CreateFont( 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Tahoma" ), (LPARAM) 1 );

			// Initialize the server console text.
			SetDlgItemText( hDlg, IDC_CONSOLEBOX, "=== " GAMENAME " server ===" );
			Printf( "\nRunning version: %s\n", GetVersionStringRev() );

			// Append the time.
			struct	tm		*pTimeInfo;
			time_t			clock;
			time (&clock);
			pTimeInfo = localtime (&clock);
			Printf("Started on %d/%d/%d at %d:%d:%d.\n", pTimeInfo->tm_mon, pTimeInfo->tm_mday,
				(1900+pTimeInfo->tm_year), pTimeInfo->tm_hour, pTimeInfo->tm_min, pTimeInfo->tm_sec);
				// [RC] TODO: AM/PM, localization, central time class

			Printf("\n");

			// Initialize the title string.
			std::string versionString = GetVersionString();
			if ( BUILD_ID != BUILD_RELEASE )
			{
				versionString += " (r";
				versionString += GetGitTime();
				versionString += ")";
			}
			SendMessage( g_hDlgStatusBar, SB_SETTEXT, (WPARAM)4, (LPARAM) versionString.c_str() );
			SetDlgItemText( hDlg, IDC_MAPMODE, "Please wait..." );

			// Set the text limits for the console and input boxes.
			SendDlgItemMessage( hDlg, IDC_CONSOLEBOX, EM_SETLIMITTEXT, 4096, 0 );
			SendDlgItemMessage( hDlg, IDC_INPUTBOX, EM_SETLIMITTEXT, 256, 0 );

			// Insert the name column.
			sprintf( szColumnTitle, "Name" );
			ColumnData.mask = LVCF_FMT|LVCF_TEXT|LVCF_WIDTH;
			ColumnData.fmt = LVCFMT_LEFT;
			ColumnData.cx = 192;
			ColumnData.pszText = szColumnTitle;
			ColumnData.cchTextMax = 64;
			ColumnData.iSubItem = 0;
			lIndex = SendDlgItemMessage( hDlg, IDC_PLAYERLIST, LVM_INSERTCOLUMN, COLUMN_NAME, (LPARAM)&ColumnData );

			// Insert the frags column.
			sprintf( szColumnTitle, "Frags" );
			ColumnData.mask = LVCF_FMT|LVCF_TEXT|LVCF_WIDTH;
			ColumnData.fmt = LVCFMT_LEFT;
			ColumnData.cx = 64;
			ColumnData.pszText = szColumnTitle;
			ColumnData.cchTextMax = 64;
			ColumnData.iSubItem = 0;
			lIndex = SendDlgItemMessage( hDlg, IDC_PLAYERLIST, LVM_INSERTCOLUMN, COLUMN_FRAGS, (LPARAM)&ColumnData );

			// Insert the ping column.
			sprintf( szColumnTitle, "Ping" );
			ColumnData.mask = LVCF_FMT|LVCF_TEXT|LVCF_WIDTH;
			ColumnData.fmt = LVCFMT_LEFT;
			ColumnData.cx = 64;
			ColumnData.pszText = szColumnTitle;
			ColumnData.cchTextMax = 64;
			ColumnData.iSubItem = 0;
			lIndex = SendDlgItemMessage( hDlg, IDC_PLAYERLIST, LVM_INSERTCOLUMN, COLUMN_PING, (LPARAM)&ColumnData );

			// Insert the time column.
			sprintf( szColumnTitle, "Time" );
			ColumnData.mask = LVCF_FMT|LVCF_TEXT|LVCF_WIDTH;
			ColumnData.fmt = LVCFMT_LEFT;
			ColumnData.cx = 60;
			ColumnData.pszText = szColumnTitle;
			ColumnData.cchTextMax = 64;
			ColumnData.iSubItem = 0;
			lIndex = SendDlgItemMessage( hDlg, IDC_PLAYERLIST, LVM_INSERTCOLUMN, COLUMN_TIME, (LPARAM)&ColumnData );

			// Initialize the player indicies array.
			for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
				g_lPlayerIndicies[ulIdx] = -1;

			// Create the thread that runs the game.
			I_DetectOS( );
			g_hThread = CreateThread( NULL, 0, MainDoomThread, 0, 0, 0 );
		}
		break;
	case WM_NOTIFY:
		{
			// Show a pop-up menu to kick or ban users.
			LPNMHDR	nmhdr = (LPNMHDR) lParam;
			if (( nmhdr->code == NM_RCLICK ) && ( nmhdr->idFrom == IDC_PLAYERLIST ))
				serverconsole_ScoreboardRightClicked( );

			// Let users click the statusbar.
			if (( nmhdr->code == NM_CLICK ) && ( nmhdr->idFrom == IDC_SERVER_STATUSBAR ))
			{
				int iIndex = ((LPNMMOUSE) lParam )->dwItemSpec;
				if ( iIndex == 0 )
					sv_updatemaster = !sv_updatemaster;
				else if ( iIndex == 1 )
					sv_broadcast = !sv_broadcast;
				else if ( iIndex == 2 && g_bServerLoaded )
				{
					char	szIPString[256];
			
					sprintf( szIPString, "%s", g_LocalAddress.ToString() );
					Printf( "This server's IP address (%s) has been copied to the clipboard.\n", szIPString );

					// Paste it into the clipboard.
					I_PutInClipboard( szIPString );
				}
				else if ( iIndex == 3 )
					DialogBox( g_hInst, MAKEINTRESOURCE( IDD_SERVERSTATISTICS ), hDlg, (DLGPROC)SERVERCONSOLE_ServerStatisticsCallback );
			}
			break;
		}
	case WM_COMMAND:
		{
			switch ( LOWORD( wParam ))
			{
/*
			// This even occurs when esc is pressed.
			case IDCANCEL:

				SERVERCONSOLE_Quit( );
				break;
*/
			// Server admin has submitted a command.
			case IDC_SEND:
				{
					char	szBuffer[1024];
					char	*pszEntry;

					GetDlgItemText( hDlg, IDC_INPUTBOX, szBuffer, sizeof( szBuffer ));
					
					if ( szBuffer[0] == ':' ) // If the text in the send buffer begins with a :, the admin is just talking.
					{
						char	szBuffer2[2056];

						sprintf( szBuffer2, "say %s", szBuffer + 1 );
						SERVER_AddCommand( szBuffer2 );
						SetDlgItemText( hDlg, IDC_INPUTBOX, "" );
						break;
					}
					else if ( szBuffer[0] == '/' ) // If the text in the send buffer begins with a slash, error out -- Skulltag used to require you to do this to send commands.
					{
						Printf( "You no longer have to prefix commands with a / to send them.\n" );
						SetDlgItemText( hDlg, IDC_INPUTBOX, szBuffer + 1 );
						SendMessage( GetDlgItem( hDlg, IDC_INPUTBOX ), EM_SETSEL, strlen( szBuffer ) - 1, strlen( szBuffer ) - 1 );
						break;
					}
					else
						pszEntry = szBuffer;

					// Add the command to history.
					if ( g_RecentConsoleMessages.size() > 19 )
						g_RecentConsoleMessages.erase( g_RecentConsoleMessages.begin() );
					g_RecentConsoleMessages.push_back( pszEntry );	

					SERVER_AddCommand( pszEntry );
					SetDlgItemText( hDlg, IDC_INPUTBOX, "" );
					EnableWindow( GetDlgItem( hDlg, IDC_HISTORY ), !g_RecentConsoleMessages.empty() );
				}
				break;
			case IDC_HISTORY:
				{
					//==========================
					// Create and show the menu.
					//==========================

					int	iSelection = -1;
					if ( !g_RecentConsoleMessages.empty() )
					{
						HMENU	hMenu = CreatePopupMenu();
						POINT	pt;
						int		iIndex = IDC_HISTORY_MENU;
						
						AppendMenu( hMenu, MF_STRING | MF_DISABLED, 0, "Recent entries..." );
						AppendMenu( hMenu, MF_SEPARATOR, 0, 0 );

						for( std::vector<FString>::reverse_iterator i = g_RecentConsoleMessages.rbegin(); i != g_RecentConsoleMessages.rend(); ++i )
						{
							FString entry = *i;
							if ( entry.Len() > 35 )
								entry = entry.Left( 32 ) + "...";
							AppendMenu( hMenu, MF_STRING, iIndex++, entry );
						}

						// Show it, and get the selected item.
						GetCursorPos( &pt );
						iSelection = ::TrackPopupMenu( hMenu, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RETURNCMD | TPM_HORIZONTAL, 
							pt.x, pt.y, 0, g_hDlg, NULL );
						DestroyMenu( hMenu );
					}

					//===================================
					// Fill in the box with the command.
					//===================================

					if ( iSelection >= IDC_HISTORY_MENU && iSelection < IDC_HISTORY_MENU + static_cast<int> ( g_RecentConsoleMessages.size() ) )
					{
						FString entry = g_RecentConsoleMessages[ g_RecentConsoleMessages.size() - ( iSelection - IDC_HISTORY_MENU ) - 1];

						SetDlgItemText( hDlg, IDC_INPUTBOX, entry );
						SetFocus( GetDlgItem( hDlg, IDC_INPUTBOX ));
						SendMessage( GetDlgItem( hDlg, IDC_INPUTBOX ), EM_SETSEL, entry.Len(), entry.Len() );
					}
				}
				break;
			case IDC_INPUTBOX:

				if ( HIWORD( wParam ) == EN_CHANGE )
					serverconsole_UpdateSendButton( hDlg );
				break;
			case ID_FILE_EXIT:

				SERVERCONSOLE_Quit( );
				break;
			case IDC_TITLEBOX:
			case ID_SETTINGS_GENERALSETTINGS:

				SERVERCONSOLE_SETTINGS_Display( hDlg );
				break;
			case ID_SETTINGS_CONFIGREDMFLAGS:

				DialogBox( g_hInst, MAKEINTRESOURCE( IDD_DMFLAGS ), hDlg, (DLGPROC)SERVERCONSOLE_DMFlagsCallback );
				break;
			case ID_SETTINGS_MAPROTATION:

				DialogBox( g_hInst, MAKEINTRESOURCE( IDD_MAPROTATION ), hDlg, (DLGPROC)SERVERCONSOLE_MapRotationCallback );
				break;
			case ID_SERVER_STATISTICS:

				DialogBox( g_hInst, MAKEINTRESOURCE( IDD_SERVERSTATISTICS ), hDlg, (DLGPROC)SERVERCONSOLE_ServerStatisticsCallback );
				break;
			case ID_ADMIN_ADDREMOVEBOT:

				MessageBox( hDlg, "To add or remove a bot, right click on the scoreboard.\nYou can add a specific bot by typing \"addbot [name]\" into the console.", "Help", MB_ICONINFORMATION );
				break;
			case ID_ADMIN_KICKBAN:

				MessageBox( hDlg, "To kick or ban a player, right click him on the scoreboard.", "Help", MB_ICONINFORMATION );
				break;
			case IDC_MAPMODE:
			case ID_ADMIN_CHANGEMAP:

				if ( g_bServerLoaded )
					DialogBox( g_hInst, MAKEINTRESOURCE( IDD_CHANGEMAP ), hDlg, (DLGPROC)SERVERCONSOLE_ChangeMapCallback );
				break;
			case ID_ADMIN_BANIP:

				DialogBox( g_hInst, MAKEINTRESOURCE( IDD_BANIP ), hDlg, (DLGPROC)SERVERCONSOLE_BanIPCallback );
				break;
			case ID_ADMIN_VIEWBANLIST:

				DialogBox( g_hInst, MAKEINTRESOURCE( IDD_BANLIST ), hDlg, (DLGPROC)SERVERCONSOLE_BanListCallback );
				break;
			case IDM_HOW_TO_USE1:

				I_RunProgram( "http://" DOMAIN_NAME "/wiki/How_to_use_the_Skulltag_Server" );
				break;
			case IDM_COMMAND_REFERENCE1:

				I_RunProgram( "http://" DOMAIN_NAME "/wiki/Commands" );
				break;
			case IDM_DMFLAGS1:

				I_RunProgram( "http://" DOMAIN_NAME "/wiki/DMFlags" );
				break;
			case IDM_GAME_MODES1:

				I_RunProgram( "http://" DOMAIN_NAME "/wiki/Game_Modes" );
				break;
			case IDM_NOBODY_CAN_SEE_MY_SERVER_1:

				I_RunProgram( "http://" DOMAIN_NAME "/wiki/Port_Forwarding" );
				break;
			case IDR_JOIN_SERVER:

				if ( g_bServerLoaded )
				{
					// [BB] Load all wads the server loaded and connect to it.
					FString arguments = NETWORK_GetPWADList().Size() ? "-file " : "";

					for ( unsigned int i = 0; i < NETWORK_GetPWADList().Size(); ++i )
					{
						// [BB] Load the wads using their full path, they don't need to be in our search path.
						const int wadnum = Wads.CheckIfWadLoaded ( NETWORK_GetPWADList()[i].name );
						const char *wadFullName = ( wadnum != -1 ) ? Wads.GetWadFullName ( wadnum ) : NULL;
						if ( wadFullName )
							arguments.AppendFormat( "\"%s\" ", wadFullName );
					}

					const TArray<FString> &dehfiles = D_GetDehFiles( );
					for ( unsigned int dehidx = 0; dehidx < dehfiles.Size( ); ++dehidx )
					{
						arguments.AppendFormat( "-deh \"%s\" ", dehfiles[dehidx] );
					}
					arguments.AppendFormat( "-iwad %s ", NETWORK_GetIWAD( ) );
					arguments.AppendFormat( "-connect %s ", g_LocalAddress.ToString() );

					// Run it!
					ShellExecute( hDlg, "open", Args->GetArg( 0 ), arguments.GetChars( ), NULL, SW_SHOW );
				}
				break;
			}
		}
		break;
	case WM_SYSCOMMAND:

		if (( wParam == SC_MINIMIZE ) && ( sv_minimizetosystray ))
		{
			RECT			DesktopRect;
			RECT			ThisWindowRect;
			ANIMATIONINFO	AnimationInfo;
			NOTIFYICONDATA	NotifyIconData;
			UCVarValue		Val;
			char			szString[64];

			AnimationInfo.cbSize = sizeof( AnimationInfo );
			SystemParametersInfo( SPI_GETANIMATION, sizeof( AnimationInfo ), &AnimationInfo, 0 );

			// Animations are turned ON, go ahead with the animation.
			if ( AnimationInfo.iMinAnimate )
			{
				GetWindowRect( GetDesktopWindow( ),&DesktopRect );
				GetWindowRect( hDlg,&ThisWindowRect );

				// Set the destination rect to the lower right corner of the screen
				DesktopRect.left = DesktopRect.right;
				DesktopRect.top = DesktopRect.bottom;

				// Do the little animation showing the window moving to the systray.
#ifndef __WINE__
				DrawAnimatedRects( hDlg, IDANI_CAPTION, &ThisWindowRect,&DesktopRect );
#endif
			}

			// Hide the window.
			ShowWindow( hDlg, SW_HIDE );

			// Show the notification icon.
			ZeroMemory( &NotifyIconData, sizeof( NotifyIconData ));
			NotifyIconData.cbSize = sizeof( NOTIFYICONDATA );
			NotifyIconData.hWnd = hDlg;
			NotifyIconData.uID = 0;
			NotifyIconData.uFlags = NIF_ICON|NIF_MESSAGE|NIF_TIP;

			NotifyIconData.uCallbackMessage = UWM_TRAY_TRAYID;
			NotifyIconData.hIcon = g_hSmallIcon;
			
			Val = sv_hostname.GetGenericRep( CVAR_String );
			strncpy( szString, Val.String, 63 );
			szString[63] = 0;
			lstrcpy( NotifyIconData.szTip, szString );

			Shell_NotifyIcon( NIM_ADD, &NotifyIconData );
			break;
		}

		DefWindowProc( hDlg, Message, wParam, lParam );
		break;
//		return ( FALSE );
	case UWM_TRAY_TRAYID:

		switch ( lParam )
		{
		case WM_LBUTTONDOWN:

			g_bSmallIconClicked = true;
			return true;
		case WM_LBUTTONUP:

			{
				RECT			DesktopRect;
				RECT			ThisWindowRect;
				NOTIFYICONDATA	NotifyIconData;
				UCVarValue		Val;
				char			szString[64];

				GetWindowRect( GetDesktopWindow( ), &DesktopRect );
				DesktopRect.left = DesktopRect.right;
				DesktopRect.top = DesktopRect.bottom;
				GetWindowRect( hDlg, &ThisWindowRect );

				// Animate the maximization.
#ifndef __WINE__
				DrawAnimatedRects( hDlg, IDANI_CAPTION, &DesktopRect, &ThisWindowRect );
#endif

				ShowWindow( hDlg, SW_SHOW );
				SetActiveWindow( hDlg );
				SetForegroundWindow( hDlg );

				// Hide the notification icon.
				ZeroMemory( &NotifyIconData, sizeof( NotifyIconData ));
				NotifyIconData.cbSize = sizeof( NOTIFYICONDATA );
				NotifyIconData.hWnd = hDlg;
				NotifyIconData.uID = 0;
				NotifyIconData.uFlags = NIF_ICON|NIF_MESSAGE|NIF_TIP;
				NotifyIconData.uCallbackMessage = UWM_TRAY_TRAYID;
				NotifyIconData.hIcon = g_hSmallIcon;//LoadIcon( g_hInst, MAKEINTRESOURCE( IDI_ICONST ));

				Val = sv_hostname.GetGenericRep( CVAR_String );
				strncpy( szString, Val.String, 63 );
				szString[63] = 0;
				lstrcpy( g_NotifyIconData.szTip, szString );

				Shell_NotifyIcon( NIM_DELETE, &NotifyIconData );
				g_bSmallIconClicked = false;
			}
			return ( TRUE );
		default:

			break;
		}

		return ( FALSE );
	default:

		return ( FALSE );
	}

	return ( TRUE );
}

//*****************************************************************************
//
void SERVERCONSOLE_Hide( void )
{
	ShowWindow( g_hDlg, SW_HIDE );
}

//*****************************************************************************
//
void serverconsole_ScoreboardRightClicked( void )
{
	char	szString[64];
	bool	bNothingSelected = false, bBotSelected = false;
	
	//=================================
	// First, we get the selected item.
	//=================================

	LVITEM	pItem;
	HWND	hList = GetDlgItem( g_hDlg, IDC_PLAYERLIST );
	pItem.pszText = szString;
	pItem.mask = LVIF_TEXT;
	pItem.iItem = ListView_GetHotItem( hList );
	pItem.iSubItem = COLUMN_PING;
	pItem.cchTextMax = 16;

	// If it fails, nothing is selected.
	bNothingSelected = !ListView_GetItem( hList, &pItem );

	// If the ping is "Bot", it's a bot.
	bBotSelected = ( strcmp( pItem.pszText, "Bot" ) == 0 );

	// Player or bot selected? Get its name.
	if ( !bNothingSelected )
	{
		pItem.pszText = g_szScoreboard_SelectedUser;
		pItem.mask = LVIF_TEXT;
		pItem.iItem = ListView_GetHotItem(hList);
		pItem.iSubItem = COLUMN_NAME;
		pItem.cchTextMax = 64;

		if ( !ListView_GetItem( hList, &pItem ) )
			return; // This shouldn't really happen by here.
	}
	
	//==========================
	// Create and show the menu.
	//==========================

	int	iSelection = -1;
	{
		HMENU	hMenu = CreatePopupMenu();
		POINT	pt;					

		// Nothing selected. Offer to add a bot.
		if ( bNothingSelected )
			AppendMenu( hMenu, MF_STRING, IDR_BOT_ADD, "Add bot" );

		// Bot selected. Offer to add/remove him.
		else if ( bBotSelected )
		{
			AppendMenu( hMenu, MF_STRING, IDR_BOT_CLONE, "Clone" );
			AppendMenu( hMenu, MF_STRING, IDR_BOT_REMOVE, "Remove" );
			AppendMenu( hMenu, MF_STRING, IDR_BOT_REMOVEALL, "Remove all bots" );
		}

		// Player selected. Offer to ban him.
		else
		{
			AppendMenu( hMenu, MF_STRING, IDR_PLAYER_KICK, "Kick" );
			HMENU	hBanMenu = CreatePopupMenu();
			AppendMenu( hBanMenu, MF_STRING, IDR_PLAYER_BAN_10MINS, "For 10 minutes" );
			AppendMenu( hBanMenu, MF_STRING, IDR_PLAYER_BAN_30MINS, "For 30 minutes" );
			AppendMenu( hBanMenu, MF_STRING, IDR_PLAYER_BAN_4HRS, "For 4 hours" );
			AppendMenu( hBanMenu, MF_STRING, IDR_PLAYER_BAN_1WEEK, "For 1 week" );
			AppendMenu( hBanMenu, MF_STRING, IDR_PLAYER_BAN_1MONTH, "For 1 month" );
			AppendMenu( hBanMenu, MF_STRING, IDR_PLAYER_BAN_1YEAR, "For 1 year" );
			AppendMenu( hBanMenu, MF_STRING, IDR_PLAYER_BAN_DIALOG, "For..." );
			AppendMenu( hBanMenu, MF_STRING, IDR_PLAYER_BAN_PERM, "Permanently" );
			AppendMenu( hMenu, MF_STRING, IDR_PLAYER_GETIP, "Copy IP" );
			AppendMenu( hMenu, MF_POPUP, (UINT_PTR) hBanMenu, "Ban" );
		}

		// Show it, and get the selected item.
		GetCursorPos( &pt );
		iSelection = ::TrackPopupMenu( hMenu, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RETURNCMD | TPM_HORIZONTAL, 
			pt.x, pt.y, 0, g_hDlg, NULL );
		DestroyMenu( hMenu );
	}

	//============================
	// Execute the user's command.
	//============================
	
	char	szCommand[128];

	if ( bNothingSelected && iSelection == IDR_BOT_ADD )
		SERVER_AddCommand( "addbot" );
	else if ( bBotSelected )
	{					
		// Clone the bot; add him in again.
		if ( iSelection == IDR_BOT_CLONE )
			sprintf( szCommand, "addbot \"%s\" red", g_szScoreboard_SelectedUser );

		// Remove the bot.
		else if ( iSelection == IDR_BOT_REMOVE )
			sprintf( szCommand, "removebot \"%s\"", g_szScoreboard_SelectedUser );
		
		// Clear all bots.
		else if ( iSelection == IDR_BOT_REMOVEALL )
			strcpy( szCommand, "removebots" );

		SERVER_AddCommand( szCommand );
	}
	else
	{		
		// Kick the player.
		if ( iSelection == IDR_PLAYER_KICK )
		{
			sprintf( szCommand, "kick \"%s\"", g_szScoreboard_SelectedUser );
			SERVER_AddCommand( szCommand );
			return;
		}
		// Copy the player's IP.
		else if ( iSelection == IDR_PLAYER_GETIP )
		{
			char	szIPString[256];
			ULONG ulIndex = SERVER_GetPlayerIndexFromName( g_szScoreboard_SelectedUser, true, false );
			
			if ( ulIndex != MAXPLAYERS )
			{
				sprintf( szIPString, "%s", SERVER_GetClient( ulIndex )->Address.ToString() );
				Printf( "%s's IP address (%s) has been copied to the clipboard.\n", g_szScoreboard_SelectedUser, szIPString );
		
				// Paste it into the clipboard.
				I_PutInClipboard( szIPString );
			}
			return;
		}

		// Ban the player...
		strcpy( g_szBanLength, "" );
		if ( iSelection == IDR_PLAYER_BAN_10MINS )
			strcpy( g_szBanLength, "10 minutes" );
		else if ( iSelection == IDR_PLAYER_BAN_30MINS )
			strcpy( g_szBanLength, "30 minutes" );
		else if ( iSelection == IDR_PLAYER_BAN_4HRS )
			strcpy( g_szBanLength, "4 hours" );
		else if ( iSelection == IDR_PLAYER_BAN_1WEEK )
			strcpy( g_szBanLength, "1 week" );
		else if ( iSelection == IDR_PLAYER_BAN_1MONTH )
			strcpy( g_szBanLength, "1 month" );
		else if ( iSelection == IDR_PLAYER_BAN_1YEAR )
			strcpy( g_szBanLength, "1 year" );
		else if ( iSelection == IDR_PLAYER_BAN_PERM )
			strcpy( g_szBanLength, "perm" );

		// Show the dialog.
		if ( iSelection == IDR_PLAYER_BAN_DIALOG )
		{
			strcpy( g_szScoreboard_Reason, "" );
			DialogBox( g_hInst, MAKEINTRESOURCE( IDD_BANPLAYER_ADV ), g_hDlg, (DLGPROC)serverconsole_BanPlayerAdvancedCallback );
		}
		else if ( strlen( g_szBanLength ))
			DialogBox( g_hInst, MAKEINTRESOURCE( IDD_BANPLAYER_SIMPLE ), g_hDlg, (DLGPROC)serverconsole_BanPlayerSimpleCallback );
	}
}

//*****************************************************************************
//
void serverconsole_FillListWithAvailableMaps( HWND hDlg, int iComboBoxID )
{
	// Fill the list box with available maps.
	for ( unsigned int i = 0; i < wadlevelinfos.Size( ); ++i )
	{
		FString map;
		map.Format( "%s - %s", wadlevelinfos[i].mapname, wadlevelinfos[i].LookupLevelName( ).GetChars( ));
		SendDlgItemMessage( hDlg, iComboBoxID, CB_INSERTSTRING, -1, (WPARAM)(LPSTR) map.GetChars( ) );
	}
}

//*****************************************************************************
//
BOOL CALLBACK SERVERCONSOLE_ChangeMapCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	switch ( Message )
	{
	case WM_CLOSE:

		EndDialog( hDlg, -1 );
		break;
	case WM_INITDIALOG:
		
			// Set the text limit for the map name box. [BC] I don't think a lump will ever be more than 8 characters in length, but just in case, I'll allow for 32 chars of text.
			SendDlgItemMessage( hDlg, IDC_MAPNAME, CB_LIMITTEXT, 32, 0 );
			serverconsole_FillListWithAvailableMaps( hDlg, IDC_MAPNAME );
		break;
	case WM_COMMAND:
		{		
			switch ( LOWORD( wParam ))
			{		
			case IDC_MAPNAME:

				if ( HIWORD( wParam ) != LBN_DBLCLK ) // Double-clicking a map acts as if "OK" was pushed.
					break;
			case IDOK:
				{
					char	szBuffer[32];
					char	szString[48];

					// Get the text from the input box.
					GetDlgItemText( hDlg, IDC_MAPNAME, szBuffer, 32 );
					if ( strstr( szBuffer, " " ) != NULL )
						szBuffer[ strstr( szBuffer, " " ) - szBuffer ] = 0; // Trim the description.

					if ( strlen( szBuffer ) > 0 )
					{
						// Build and execute the command string.
						if ( SendDlgItemMessage( hDlg, IDC_INTERMISSION, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
							sprintf( szString, "changemap %s", szBuffer );
						else
							sprintf( szString, "map %s", szBuffer );
						SERVER_AddCommand( szString );
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

//*****************************************************************************
//
void serverconsole_MapRotation_Update( HWND hDlg )
{
	EnableWindow( GetDlgItem( hDlg, IDC_MAPLISTBOX ), SendDlgItemMessage( hDlg, IDC_USEMAPROTATIONLIST, BM_GETCHECK, 0, 0 ) == BST_CHECKED );
	EnableWindow( GetDlgItem( hDlg, IDC_USERANDOMMAP ), SendDlgItemMessage( hDlg, IDC_USEMAPROTATIONLIST, BM_GETCHECK, 0, 0 ) == BST_CHECKED );
	EnableWindow( GetDlgItem( hDlg, IDC_REMOVE ), SendDlgItemMessage( hDlg, IDC_USEMAPROTATIONLIST, BM_GETCHECK, 0, 0 ) == BST_CHECKED && SendDlgItemMessage( hDlg, IDC_MAPLISTBOX, LB_GETCOUNT, 0, 0 ) > 0 && SendMessage( GetDlgItem( hDlg, IDC_MAPLISTBOX ), LB_GETSELCOUNT, 0, 0 ) > 0 );
	EnableWindow( GetDlgItem( hDlg, IDC_CLEAR ), SendDlgItemMessage( hDlg, IDC_USEMAPROTATIONLIST, BM_GETCHECK, 0, 0 ) == BST_CHECKED && SendDlgItemMessage( hDlg, IDC_MAPLISTBOX, LB_GETCOUNT, 0, 0 ) > 0 );

	// En/disable the "Add >>" button.
	char	szBuffer[32];
	GetDlgItemText( hDlg, IDC_MAPNAME, szBuffer, 32 );					
	if ( strstr( szBuffer, " " ) != NULL )
		szBuffer[ strstr( szBuffer, " " ) - szBuffer ] = 0; // Trim the description.
	EnableWindow( GetDlgItem( hDlg, IDC_ADDMAP ), szBuffer[0] && FindLevelByName( szBuffer ) != NULL );
}

//*****************************************************************************
//
BOOL CALLBACK SERVERCONSOLE_MapRotationCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	switch ( Message )
	{
	case WM_CLOSE:

		EndDialog( hDlg, -1 );
		break;
	case WM_INITDIALOG:
		{
			// Initialize the form.
			SendDlgItemMessage( hDlg, IDC_MAPNAME, CB_LIMITTEXT, 32, 0 );
			SendDlgItemMessage( hDlg, IDC_USEMAPROTATIONLIST, BM_SETCHECK, !!sv_maprotation, 0 );
			SendDlgItemMessage( hDlg, IDC_USERANDOMMAP, BM_SETCHECK, !!sv_randommaprotation, 0 );
			serverconsole_MapRotation_Update( hDlg );

			// Populate the "available maps" box.
			serverconsole_FillListWithAvailableMaps( hDlg, IDC_MAPNAME );

			// Populate the box with the current map rotation list.
			SendDlgItemMessage( hDlg, IDC_MAPLISTBOX, LB_RESETCONTENT, 0, 0 );
			for ( ULONG ulIdx = 0; ulIdx < MAPROTATION_GetNumEntries( ); ulIdx++ )
			{				
				FString map;
				map.Format( "%s - %s", MAPROTATION_GetMap( ulIdx )->mapname, MAPROTATION_GetMap( ulIdx )->LookupLevelName( ).GetChars( ));
				SendDlgItemMessage( hDlg, IDC_MAPLISTBOX, LB_INSERTSTRING, -1, (WPARAM)(LPSTR) map.GetChars( ) );
			}
		}
		break;
	case WM_COMMAND:
		{
			switch ( LOWORD( wParam ))
			{
			case IDOK:
				{		
					// First, clear the existing map rotation list.
					MAPROTATION_Construct( );

					// Now, add the maps in the listbox to the map rotation list.
					LONG lCount = SendDlgItemMessage( hDlg, IDC_MAPLISTBOX, LB_GETCOUNT, 0, 0 );
					if ( lCount != LB_ERR )
					{						
						for ( LONG lIdx = 0; lIdx < lCount; lIdx++ )
						{
							char	szBuffer[32];
							char	szString[32];

							SendDlgItemMessage( hDlg, IDC_MAPLISTBOX, LB_GETTEXT, lIdx, (LPARAM) (LPCTSTR)szBuffer );
							if ( strstr( szBuffer, " " ) != NULL )
								szBuffer[ strstr( szBuffer, " " ) - szBuffer ] = 0; // Trim the description.

							sprintf( szString, "addmapsilent %s", szBuffer );
							SERVER_AddCommand( szString );
						}
					}

					// Set the rotation options.
					sv_maprotation = ( SendDlgItemMessage( hDlg, IDC_USEMAPROTATIONLIST, BM_GETCHECK, 0, 0 ) == BST_CHECKED );
					sv_randommaprotation = ( SendDlgItemMessage( hDlg, IDC_USERANDOMMAP, BM_GETCHECK, 0, 0 ) == BST_CHECKED );
				}
				EndDialog( hDlg, -1 );
				break;
			case IDCANCEL:

				EndDialog( hDlg, -1 );
				break;
			case IDC_USEMAPROTATIONLIST:

				serverconsole_MapRotation_Update( hDlg );
				break;
			case IDC_MAPLISTBOX:

				if ( HIWORD( wParam ) == LBN_SELCHANGE )
					serverconsole_MapRotation_Update( hDlg );
				break;
			case IDC_MAPNAME:

				if ( HIWORD( wParam ) == CBN_EDITCHANGE || HIWORD( wParam ) == CBN_SELCHANGE )
					serverconsole_MapRotation_Update( hDlg );

				if ( HIWORD( wParam ) != CBN_DBLCLK ) // Double-clicking a map acts as if "Add >>" was pushed
					break;			
			case IDC_ADDMAP:
				{
					char	szBuffer[32];

					// Get the text from the input box.
					GetDlgItemText( hDlg, IDC_MAPNAME, szBuffer, 32 );					
					if ( strstr( szBuffer, " " ) != NULL )
						szBuffer[ strstr( szBuffer, " " ) - szBuffer ] = 0; // Trim the description.

					// Add the string to the end of the list.
					if ( szBuffer[0] )
					{						
						// Find the map.
						level_info_t *pMap = FindLevelByName( szBuffer );
						if ( pMap == NULL )
							break;

						FString map;
						map.Format( "%s - %s", pMap->mapname, pMap->LookupLevelName( ).GetChars( ));
						SendDlgItemMessage( hDlg, IDC_MAPLISTBOX, LB_INSERTSTRING, -1, (LPARAM)map.GetChars( ) );
					}

					serverconsole_MapRotation_Update( hDlg );
				}
				break;
			case IDC_REMOVE:
				{
					LONG	lCount = SendMessage( GetDlgItem( hDlg, IDC_MAPLISTBOX ), LB_GETSELCOUNT, 0, 0 );

					if ( lCount == LB_ERR )
						break;

					// And then allocate room to store the list of selected items.
					LONG	lIdx;
					LONG	*palBuffer;

					palBuffer = (LONG *)malloc( sizeof( LONG ) * lCount );
					SendMessage( GetDlgItem( hDlg, IDC_MAPLISTBOX ), LB_GETSELITEMS, (WPARAM)lCount, (LPARAM)palBuffer );
					
					// Now we loop through the list and remove each item that was selected. 
					for( lIdx = lCount - 1; lIdx >= 0; lIdx-- )
						SendMessage( GetDlgItem( hDlg, IDC_MAPLISTBOX ), LB_DELETESTRING, palBuffer[lIdx], 0 );

					free( palBuffer );					
				}
				break;
			case IDC_CLEAR:

				SendDlgItemMessage( hDlg, IDC_MAPLISTBOX, LB_RESETCONTENT, 0, 0 );
				break;
			}
		}
		break;
	default:

		return ( FALSE );
	}

	return ( TRUE );
}

//*****************************************************************************

BOOL CALLBACK SERVERCONSOLE_MessagesCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	switch ( Message )
	{
	case WM_CLOSE:

		EndDialog( hDlg, -1 );
		break;
	case WM_INITDIALOG:
		{
			// Initialize the "Enable timestamp" checkbox.
			SendDlgItemMessage( hDlg, IDC_TIMESTAMP, BM_SETCHECK, ( sv_timestamp ) ? BST_CHECKED : BST_UNCHECKED, 0 );
			SendDlgItemMessage( hDlg, IDC_TIMESTAMPFORMAT, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)"[HH:MM:SS]" );
			SendDlgItemMessage( hDlg, IDC_TIMESTAMPFORMAT, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)"[HH:MM:SS AM/PM]" );
			SendDlgItemMessage( hDlg, IDC_TIMESTAMPFORMAT, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)"[HH:MM:SS am/pm]" );
			SendDlgItemMessage( hDlg, IDC_TIMESTAMPFORMAT, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)"[HH:MM]" );
			SendDlgItemMessage( hDlg, IDC_TIMESTAMPFORMAT, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)"[HH:MM AM/PM]" );
			SendDlgItemMessage( hDlg, IDC_TIMESTAMPFORMAT, CB_INSERTSTRING, -1, (WPARAM)(LPSTR)"[HH:MM am/pm]" );
			SendDlgItemMessage( hDlg, IDC_TIMESTAMPFORMAT, CB_SETCURSEL, sv_timestampformat, 0 );
			SendDlgItemMessage( hDlg, IDC_MARKCHATLINES, BM_SETCHECK, ( sv_markchatlines ) ? BST_CHECKED : BST_UNCHECKED, 0 );
			EnableWindow( GetDlgItem( hDlg, IDC_TIMESTAMPFORMAT ), ( SendDlgItemMessage( hDlg, IDC_TIMESTAMP, BM_GETCHECK, 0, 0 ) == BST_CHECKED ) );

			switch ( sv_colorstripmethod )
			{
			// Strip color codes.
			case 0:

				SendDlgItemMessage( hDlg, IDC_STRIPCODES, BM_SETCHECK, BST_CHECKED, 0 );
				break;
			// Don't strip color codes.
			case 1:

				SendDlgItemMessage( hDlg, IDC_DONOTSTRIPCODES, BM_SETCHECK, BST_CHECKED, 0 );
				break;
			// Leave in "\c<X>" format.
			case 2:

				SendDlgItemMessage( hDlg, IDC_LEAVEINFORMAT, BM_SETCHECK, BST_CHECKED, 0 );
				break;
			default:

				SendDlgItemMessage( hDlg, IDC_STRIPCODES, BM_SETCHECK, BST_CHECKED, 0 );
				break;
			}
		}
		break;
	case WM_COMMAND:
		{
			switch ( LOWORD( wParam ))
			{
			case IDC_TIMESTAMP:

				EnableWindow( GetDlgItem( hDlg, IDC_TIMESTAMPFORMAT ), ( SendDlgItemMessage( hDlg, IDC_TIMESTAMP, BM_GETCHECK, 0, 0 ) == BST_CHECKED ) );
				break;
			case IDOK:
				{
					// Enable timestamping if the user has the checkbox checked.
					sv_timestamp = ( SendDlgItemMessage( hDlg, IDC_TIMESTAMP, BM_GETCHECK, 0, 0 ) == BST_CHECKED );
					sv_timestampformat = SendDlgItemMessage( hDlg, IDC_TIMESTAMPFORMAT, CB_GETCURSEL, 0, 0 );
					sv_markchatlines = ( SendDlgItemMessage( hDlg, IDC_MARKCHATLINES, BM_GETCHECK, 0, 0 ) == BST_CHECKED );

					// Set the method of color code stripping.
					if ( SendDlgItemMessage( hDlg, IDC_STRIPCODES, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
						sv_colorstripmethod = 0;
					else if ( SendDlgItemMessage( hDlg, IDC_DONOTSTRIPCODES, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
						sv_colorstripmethod = 1;
					else if ( SendDlgItemMessage( hDlg, IDC_LEAVEINFORMAT, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
						sv_colorstripmethod = 2;
					else
						sv_colorstripmethod = 0;				
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

//*****************************************************************************
//
BOOL CALLBACK serverconsole_BanPlayerSimpleCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	char	szString[1024];

	switch ( Message )
	{
	case WM_CLOSE:

		EndDialog( hDlg, -1 );
		break;
	case WM_INITDIALOG:

		// Update the label.
		if ( stricmp( g_szBanLength, "perm" ))
			sprintf( szString, "%s will be banned for %s.", g_szScoreboard_SelectedUser, g_szBanLength );
		else
			sprintf( szString, "%s will be banned permanently.", g_szScoreboard_SelectedUser );
		SetDlgItemText( hDlg, IDC_BANDESCRIPTION, szString );

		// Set the length limit for the text box.
		SendDlgItemMessage( hDlg, IDC_REASON, EM_SETLIMITTEXT, 511, 0 );
		break;
	case WM_COMMAND:
		{
			switch ( LOWORD( wParam ))
			{			
			case IDOK:

				GetDlgItemText( hDlg, IDC_REASON, g_szScoreboard_Reason, 512 );
				if ( !strlen( g_szScoreboard_Reason ))
					strcpy( g_szScoreboard_Reason, "None given." );
				sprintf( szString, "ban \"%s\" \"%s\" \"%s\"", g_szScoreboard_SelectedUser, g_szBanLength, g_szScoreboard_Reason );				
				SERVER_AddCommand( szString );
				EndDialog( hDlg, -1 );
				break;
			case IDC_CHANGEBANLENGTH:

				GetDlgItemText( hDlg, IDC_REASON, g_szScoreboard_Reason, 512 );				
				EndDialog( hDlg, -1 );
				DialogBox( g_hInst, MAKEINTRESOURCE( IDD_BANPLAYER_ADV ), g_hDlg, (DLGPROC)serverconsole_BanPlayerAdvancedCallback );
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

//*****************************************************************************
//
BOOL CALLBACK serverconsole_BanPlayerAdvancedCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	char	szString[1024];

	switch ( Message )
	{
	case WM_CLOSE:

		EndDialog( hDlg, -1 );
		break;
	case WM_INITDIALOG:

		// Insert preset ban lengths.
		SendDlgItemMessage( hDlg, IDC_BANLENGTH, CB_INSERTSTRING, -1, (WPARAM)(LPSTR) "10 minutes" );
		SendDlgItemMessage( hDlg, IDC_BANLENGTH, CB_INSERTSTRING, -1, (WPARAM)(LPSTR) "30 minutes" );
		SendDlgItemMessage( hDlg, IDC_BANLENGTH, CB_INSERTSTRING, -1, (WPARAM)(LPSTR) "4 hours" );
		SendDlgItemMessage( hDlg, IDC_BANLENGTH, CB_INSERTSTRING, -1, (WPARAM)(LPSTR) "1 week" );
		SendDlgItemMessage( hDlg, IDC_BANLENGTH, CB_INSERTSTRING, -1, (WPARAM)(LPSTR) "1 month" );
		SendDlgItemMessage( hDlg, IDC_BANLENGTH, CB_INSERTSTRING, -1, (WPARAM)(LPSTR) "1 year" );
		SendDlgItemMessage( hDlg, IDC_BANLENGTH, CB_INSERTSTRING, -1, (WPARAM)(LPSTR) "perm" );		

		// Update the labels and text boxes.
		sprintf( szString, "%s will be banned...", g_szScoreboard_SelectedUser );		
		SetDlgItemText( hDlg, IDC_BANDESCRIPTION, szString );
		SetDlgItemText( hDlg, IDC_REASON, g_szScoreboard_Reason );
		if ( strlen( g_szBanLength ))
			SetDlgItemText( hDlg, IDC_BANLENGTH, g_szBanLength );
		else
			SetDlgItemText( hDlg, IDC_BANLENGTH, "1 week" );

		// Set the length limit for the text boxes.
		SendDlgItemMessage( hDlg, IDC_REASON, EM_SETLIMITTEXT, 511, 0 );
		SendDlgItemMessage( hDlg, IDC_BANLENGTH, EM_SETLIMITTEXT, 63, 0 );
		break;
	case WM_COMMAND:
		{
			switch ( LOWORD( wParam ))
			{			
			case IDOK:

				GetDlgItemText( hDlg, IDC_REASON, g_szScoreboard_Reason, 512 );
				GetDlgItemText( hDlg, IDC_BANLENGTH, g_szBanLength, 64 );
				if ( !strlen( g_szScoreboard_Reason ))
					strcpy( g_szScoreboard_Reason, "None given." );
				sprintf( szString, "ban \"%s\" \"%s\" \"%s\"", g_szScoreboard_SelectedUser, g_szBanLength, g_szScoreboard_Reason );				
				SERVER_AddCommand( szString );
				EndDialog( hDlg, -1 );
				break;
			case IDC_BANLENGTH:

				if (( HIWORD( wParam ) == CBN_EDITUPDATE ) || ( HIWORD( wParam ) == CBN_SELCHANGE ) || ( HIWORD( wParam ) == CBN_SELENDOK ))
				{
					GetDlgItemText( hDlg, IDC_BANLENGTH, g_szBanLength, 64 );
					EnableWindow( GetDlgItem( hDlg, IDOK ), strlen( g_szBanLength ));
				}
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

//*****************************************************************************
//
BOOL CALLBACK SERVERCONSOLE_BanIPCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	switch ( Message )
	{
	case WM_CLOSE:

		EndDialog( hDlg, -1 );
		break;
	case WM_INITDIALOG:

		{
			// Set the text limit for the IP box.
			SendDlgItemMessage( hDlg, IDC_MAPNAME, EM_SETLIMITTEXT, 256, 0 );
		}

		break;
	case WM_COMMAND:

		{
			switch ( LOWORD( wParam ))
			{
			case IDOK:

				{
					char	szBuffer[64];
					char	szString[384];

					// Get the text from the input box.
					GetDlgItemText( hDlg, IDC_IPADDRESS, szBuffer, 256 );

					if ( strlen( szBuffer ) > 0 )
					{
						char	szComment[256];

						// Get the text from the input box.
						GetDlgItemText( hDlg, IDC_BANCOMMENT, szComment, 256 );

						// Build and execute the command string.
						if ( strlen( szComment ) > 0 )
							sprintf( szString, "addban %s perm \"%s\"", szBuffer, szComment );
						else
							sprintf( szString, "addban %s perm", szBuffer );
						SERVER_AddCommand( szString );
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

//*****************************************************************************
//
BOOL CALLBACK SERVERCONSOLE_BanListCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	ULONG		ulIdx;
	UCVarValue	Val;

	Val = sv_banfile.GetGenericRep( CVAR_String );

	switch ( Message )
	{
	case WM_CLOSE:

		EndDialog( hDlg, -1 );
		break;
	case WM_INITDIALOG:

		{
			// Set the text limit for the IP box.
			SendDlgItemMessage( hDlg, IDC_BANFILE, EM_SETLIMITTEXT, 256, 0 );

			// Set the text limit for the IP box.
			SetDlgItemText( hDlg, IDC_BANFILE, Val.String );

			if ( sv_enforcebans )
				SendDlgItemMessage( hDlg, IDC_ENFORCEBANS, BM_SETCHECK, BST_CHECKED, 0 );
			else
				SendDlgItemMessage( hDlg, IDC_ENFORCEBANS, BM_SETCHECK, BST_UNCHECKED, 0 );

			// Populate the box with the current ban list.
			for ( ulIdx = 0; ulIdx < SERVERBAN_GetBanList( )->size( ); ulIdx++ )
				SendDlgItemMessage( hDlg, IDC_BANLIST, LB_INSERTSTRING, -1, (LPARAM)SERVERBAN_GetBanList( )->getEntryAsString( ulIdx, true, true, false ).c_str( ));
		}

		break;
	case WM_COMMAND:

		{
			switch ( LOWORD( wParam ))
			{
			case IDC_EDIT:

				{
					LONG	lIdx;

					lIdx = SendDlgItemMessage( hDlg, IDC_BANLIST, LB_GETCURSEL, 0, 0 );
					if ( lIdx != LB_ERR )
					{
						SendDlgItemMessage( hDlg, IDC_BANLIST, LB_GETTEXT, lIdx, (LPARAM)g_szBanEditString );
						if ( DialogBox( g_hInst, MAKEINTRESOURCE( IDD_EDITBAN ), hDlg, (DLGPROC)SERVERCONSOLE_EditBanCallback ))
						{
							SendDlgItemMessage( hDlg, IDC_BANLIST, LB_DELETESTRING, lIdx, 0 );
							SendDlgItemMessage( hDlg, IDC_BANLIST, LB_INSERTSTRING, lIdx, (LPARAM)g_szBanEditString );
						}
					}
					else
						MessageBox( hDlg, "Please select a ban to edit first.", SERVERCONSOLE_TITLESTRING, MB_OK );
				}
				break;
			case IDC_REMOVE:

				{
					LONG	lIdx;

					lIdx = SendDlgItemMessage( hDlg, IDC_BANLIST, LB_GETCURSEL, 0, 0 );
					if ( lIdx != LB_ERR )
						SendDlgItemMessage( hDlg, IDC_BANLIST, LB_DELETESTRING, lIdx, 0 );
					else
						MessageBox( hDlg, "Please select a ban to remove first.", SERVERCONSOLE_TITLESTRING, MB_OK );
				}
				break;
			case IDC_CLEAR:

				// Clear out the ban list box.
				if ( MessageBox( hDlg, "Are you sure you want to clear the ban list?", SERVERCONSOLE_TITLESTRING, MB_YESNO|MB_ICONQUESTION ) == IDYES )
					SendDlgItemMessage( hDlg, IDC_BANLIST, LB_RESETCONTENT, 0, 0 );
				break;
			case IDOK:

				{
					LONG	lIdx;
					LONG	lCount;
					char	szBuffer[256];
					char	szString[256+32];

					// Get the text from the input box.
					GetDlgItemText( hDlg, IDC_BANFILE, szBuffer, 256 );

					sprintf( szString, "sv_banfile \"%s\"", szBuffer );
					SERVER_AddCommand( szString );

					if ( SendDlgItemMessage( hDlg, IDC_ENFORCEBANS, BM_GETCHECK, BST_CHECKED, 0 ))
						sv_enforcebans = true;
					else
						sv_enforcebans = false;

					// Clear out the ban list, and then add all the bans in the ban list.
					SERVERBAN_ClearBans( );

					// Now, add the maps in the listbox to the map rotation list.
					lCount = SendDlgItemMessage( hDlg, IDC_BANLIST, LB_GETCOUNT, 0, 0 );
					if ( lCount != LB_ERR )
					{
						char	*pszIP;
						char	szIP[32];
						char	*pszComment;
						char	szComment[224];
						char	szDate[128];
						char	*pszBuffer;

						for ( lIdx = 0; lIdx < lCount; lIdx++ )
						{
							SendDlgItemMessage( hDlg, IDC_BANLIST, LB_GETTEXT, lIdx, (LPARAM) (LPCTSTR)szBuffer );

							pszIP = szIP;
							*pszIP = 0;
							szDate[0] = 0;
							pszComment = szComment;
							*pszComment = 0;
							pszBuffer = szBuffer;
							while ( *pszBuffer != 0 && *pszBuffer != ':' && *pszBuffer != '/' && *pszBuffer != '<' )
							{
								*pszIP = *pszBuffer;
								pszBuffer++;
								pszIP++;
								*pszIP = 0;
							}

							//======================================================================================================
							// [RC] Read the expiration date.
							// This is a very klunky temporary solution that I've already fixed it in my redo of the server dialogs.
							//======================================================================================================

							time_t tExpiration = NULL;
							if ( *pszBuffer == '<' )
							{							
								int	iMonth = 0, iDay = 0, iYear = 0, iHour = 0, iMinute = 0;

								pszBuffer++;
								iMonth = strtol( pszBuffer, NULL, 10 );
								pszBuffer += 3;
								iDay = strtol( pszBuffer, NULL, 10 );
								pszBuffer += 3;
								iYear = strtol( pszBuffer, NULL, 10 );
								pszBuffer += 5;
								iHour = strtol( pszBuffer, NULL, 10 );
								pszBuffer += 3;
								iMinute = strtol( pszBuffer, NULL, 10 );
								pszBuffer += 2;
																
								// If fewer than 5 elements (the %ds) were read, the user probably edited the file incorrectly.
								if ( *pszBuffer != '>' )
								{
									Printf("parseNextLine: WARNING! Failure to read the ban expiration date!" );
									return NULL;
								}
								pszBuffer++;
								
								// Create the time structure, based on the current time.
								time_t		tNow;
								time( &tNow );
								struct tm	*pTimeInfo = localtime( &tNow );

								// Edit the values, and stitch them into a new time.
								pTimeInfo->tm_mon = iMonth - 1;
								pTimeInfo->tm_mday = iDay;

								if ( iYear < 100 )
									pTimeInfo->tm_year = iYear + 2000;
								else
									pTimeInfo->tm_year = iYear - 1900;

								pTimeInfo->tm_hour = iHour;
								pTimeInfo->tm_min = iMinute;
								pTimeInfo->tm_sec = 0;
								
								tExpiration = mktime( pTimeInfo );							
							}

							// Don't include the comment denotion character in the comment string.
							while ( *pszBuffer == ':' || *pszBuffer == '/' )
								pszBuffer++;

							while ( *pszBuffer != 0 )
							{
								*pszComment = *pszBuffer;
								pszBuffer++;
								pszComment++;
								*pszComment = 0;
							}

							std::string Message;
							SERVERBAN_GetBanList( )->addEntry( szIP, "", szComment, Message, tExpiration );
						}
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

//*****************************************************************************
//
BOOL CALLBACK SERVERCONSOLE_EditBanCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	switch ( Message )
	{
	case WM_CLOSE:

		EndDialog( hDlg, false );
		break;
	case WM_INITDIALOG:

		// Set the text limit for the IP box.
		SendDlgItemMessage( hDlg, IDC_BANBOX, EM_SETLIMITTEXT, 256, 0 );

		// Set the text limit for the ban box.
		SetDlgItemText( hDlg, IDC_BANBOX, g_szBanEditString );
		break;
	case WM_COMMAND:

		{
			switch ( LOWORD( wParam ))
			{
			case IDOK:

				// Get the text from the input box.
				GetDlgItemText( hDlg, IDC_BANFILE, g_szBanEditString, 256 );

				EndDialog( hDlg, true );
				break;
			case IDCANCEL:

				EndDialog( hDlg, false );
				break;
			}
		}
		break;
	default:

		return ( FALSE );
	}

	return ( TRUE );
}

//*****************************************************************************
//
BOOL CALLBACK SERVERCONSOLE_ServerStatisticsCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	switch ( Message )
	{
	case WM_CLOSE:

		g_hStatisticDlg = NULL;

		EndDialog( hDlg, -1 );
		break;
	case WM_INITDIALOG:
		{
			g_hStatisticDlg = hDlg;

			// Bold some labels.			
			SendMessage( GetDlgItem( hDlg, IDC_TRAFFICIN ), WM_SETFONT, (WPARAM) CreateFont( 13, 0, 0, 0, 600, 0, 0, 0, 0, 0, 0, 0, 0, "Tahoma" ), (LPARAM) 1 );
			SendMessage( GetDlgItem( hDlg, IDC_TRAFFICOUT ), WM_SETFONT, (WPARAM) CreateFont( 13, 0, 0, 0, 600, 0, 0, 0, 0, 0, 0, 0, 0, "Tahoma" ), (LPARAM) 1 );		

			SERVERCONSOLE_UpdateStatistics( );			
		}
		break;
	case WM_COMMAND:
		{
			switch ( LOWORD( wParam ))
			{
			case IDOK:

				g_hStatisticDlg = NULL;
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


//*****************************************************************************
//
void SERVERCONSOLE_UpdateTitleString( const char *pszString )
{
	if ( !g_bServerLoaded )
		return;

	char		szString[256];
	strncpy( szString, pszString, 255 );
	SetDlgItemText( g_hDlg, IDC_TITLEBOX, szString );
	SetWindowText( g_hDlg, szString );
}

//*****************************************************************************
//
void SERVERCONSOLE_UpdateIP( NETADDRESS_s LocalAddress )
{
	g_LocalAddress = LocalAddress;
	SendMessage( g_hDlgStatusBar, SB_SETTEXT, (WPARAM)2, (LPARAM) LocalAddress.ToString() );
	SERVERCONSOLE_UpdateBroadcasting( );
}

//*****************************************************************************
//
void SERVERCONSOLE_UpdateBroadcasting( void )
{
	SendMessage( g_hDlgStatusBar, SB_SETTEXT, (WPARAM)0, (LPARAM) (sv_updatemaster ? "Public" : "Private" ));
	SendMessage( g_hDlgStatusBar, SB_SETTEXT, (WPARAM)1, (LPARAM) (sv_broadcast ? "LAN" : "" ));
}

//*****************************************************************************
//
void SCOREBOARD_BuildLimitStrings( std::list<FString> &lines, bool bAcceptColors );
void SERVERCONSOLE_UpdateScoreboard( void )
{
	int labels[4] = { IDC_SCOREBOARD1, IDC_SCOREBOARD2, IDC_SCOREBOARD3, IDC_SCOREBOARD4, };	

	// Clear the labels.
	for ( int i = 0; i < 4; i++ )
		SetDlgItemText( g_hDlg, labels[i], "" );

	// Build the strings.
	std::list<FString> ScoreboardNotices;
	SCOREBOARD_BuildLimitStrings( ScoreboardNotices, false );

	// Put the strings on the labels.
	int index = 0;
	for ( std::list<FString>::iterator i = ScoreboardNotices.begin(); i != ScoreboardNotices.end(); ++i )
	{
		if ( index < 4 )
			SetDlgItemText( g_hDlg, labels[index++], *i );
	}

	// If we just have one label, center it.
	if ( index == 1 )
	{
		ShowWindow( GetDlgItem( g_hDlg, IDC_SCOREBOARDW ), SW_SHOW );
		SetDlgItemText( g_hDlg, IDC_SCOREBOARDW, ScoreboardNotices.front().GetChars( ));
	}
	else
		ShowWindow( GetDlgItem( g_hDlg, IDC_SCOREBOARDW ), SW_HIDE );
}

//*****************************************************************************
//
// [RC] Shuts the server down. Just using SERVER_AddCommand( "quit" ) will not work if the game thread is frozen.
//
void SERVERCONSOLE_Quit( void )
{
	if ( SERVER_CalcNumConnectedClients( ) > 0 )
	{	
		FString fsQuitMessage = "Are you sure you want to shut down this server?";
		fsQuitMessage.AppendFormat( "\nThere %s %d player%s connected.", ( SERVER_CalcNumConnectedClients( ) == 1 ? "is" : "are" ), SERVER_CalcNumConnectedClients( ), ( SERVER_CalcNumConnectedClients( ) == 1 ? "" : "s" ));
		if ( MessageBox( g_hDlg, fsQuitMessage.GetChars( ), SERVERCONSOLE_TITLESTRING, MB_YESNO|MB_ICONEXCLAMATION|MB_DEFBUTTON2 ) != IDYES )
			return;
	}

	EndDialog( g_hDlg, -1 );
	SuspendThread( g_hThread );
	CloseHandle( g_hThread );
	exit( 0 );
}

//*****************************************************************************
// [RC] Helper methods for SERVERCONSOLE_UpdateStatistics.
//
void serverconsole_AppendFormattedDataAmount( FString &DataString, float fData )
{
	if ( fData / GIGABYTE > GIGABYTE ) // [RC] Exabyte
		DataString.AppendFormat( "%0.1f EB", fData / GIGABYTE * 1.0 / GIGABYTE );
	else if ( fData / MEGABYTE > GIGABYTE ) // [RC] Petabyte
		DataString.AppendFormat( "%0.1f PB", fData / GIGABYTE * 1.0 / MEGABYTE );
	else if ( fData / KILOBYTE > GIGABYTE ) // [RC] Terabyte
		DataString.AppendFormat( "%0.1f TB", fData / GIGABYTE * 1.0 / KILOBYTE );
	else if ( fData > GIGABYTE ) // Gigabyte
		DataString.AppendFormat( "%0.1f GB", fData / GIGABYTE );
	else if ( fData > MEGABYTE ) // Megabyte
		DataString.AppendFormat( "%0.1f MB", fData / MEGABYTE );
	else // Kilobyte and below
		DataString.AppendFormat( "%0.1f KB", fData / KILOBYTE );
}

//*****************************************************************************
//
void serverconsole_UpdateTraffic_Total( int label, QWORD qwData )
{
	FString string = "Total: ";
	serverconsole_AppendFormattedDataAmount( string, static_cast<float>( qwData ));
	SetDlgItemText( g_hStatisticDlg, label, string.GetChars( ));
}

//*****************************************************************************
//
void serverconsole_UpdateTraffic_Average( int label, QWORD qwData )
{
	FString string = "Average: ";
	if ( SERVER_STATISTIC_GetTotalSecondsElapsed( ) == 0 )
		string += "0.0 KB";
	else
		serverconsole_AppendFormattedDataAmount( string, static_cast<float>( qwData ) / static_cast<float>( SERVER_STATISTIC_GetTotalSecondsElapsed( )));

	string += "/s";
	SetDlgItemText( g_hStatisticDlg, label, string.GetChars( ));
}

//*****************************************************************************
//
void serverconsole_UpdateTraffic_Rate( int label, LONG lData, const char *pszLabel )
{
	FString string = pszLabel;
	serverconsole_AppendFormattedDataAmount( string, static_cast<float>( lData ));
	string += "/s";
	SetDlgItemText( g_hStatisticDlg, label, string.GetChars( ));
}

//*****************************************************************************
//
void SERVERCONSOLE_UpdateStatistics( void )
{
	char szString[256];

	// Update average players for the statusbar.
	if ( SERVER_STATISTIC_GetTotalSecondsElapsed( ) == 0 )
		sprintf( szString, "Avg. players: 0.0" );
	else
		sprintf( szString, "Avg. players: %0.1f", (float)SERVER_STATISTIC_GetTotalPlayers( ) / (float)SERVER_STATISTIC_GetTotalSecondsElapsed( ));
	SendMessage( g_hDlgStatusBar, SB_SETTEXT, (WPARAM)3, (LPARAM) szString );

	if ( g_hStatisticDlg == NULL )
		return;

	// Update traffic statistics.
	serverconsole_UpdateTraffic_Total( IDC_TOTALINBOUNDDATATRANSFER, SERVER_STATISTIC_GetTotalInboundDataTransferred( ));
	serverconsole_UpdateTraffic_Total( IDC_TOTALOUTBOUNDDATATRANSFER, SERVER_STATISTIC_GetTotalOutboundDataTransferred( ));	
	serverconsole_UpdateTraffic_Average( IDC_AVERAGEINBOUNDDATATRANSFER, SERVER_STATISTIC_GetTotalInboundDataTransferred( ));
	serverconsole_UpdateTraffic_Average( IDC_AVERAGEOUTBOUNDDATATRANSFER, SERVER_STATISTIC_GetTotalOutboundDataTransferred( ));
	serverconsole_UpdateTraffic_Rate( IDC_PEAKINBOUNDDATATRANSFER, SERVER_STATISTIC_GetPeakInboundDataTransfer( ), "Peak: " );
	serverconsole_UpdateTraffic_Rate( IDC_PEAKOUTBOUNDDATATRANSFER, SERVER_STATISTIC_GetPeakOutboundDataTransfer( ), "Peak: " );
	serverconsole_UpdateTraffic_Rate( IDC_CURRENTINBOUNDDATATRANSFER, SERVER_STATISTIC_GetCurrentInboundDataTransfer( ), "Current: " );
	serverconsole_UpdateTraffic_Rate( IDC_CURRENTOUTBOUNDDATATRANSFER, SERVER_STATISTIC_GetCurrentOutboundDataTransfer( ), "Current: " );
	
	// Update average players.
	if ( SERVER_STATISTIC_GetTotalSecondsElapsed( ) == 0 )
		sprintf( szString, "Average: 0.0" );
	else
		sprintf( szString, "Average: %0.1f", (float)SERVER_STATISTIC_GetTotalPlayers( ) / (float)SERVER_STATISTIC_GetTotalSecondsElapsed( ));	
	SetDlgItemText( g_hStatisticDlg, IDC_AVGNUMPLAYERS, szString );

	// Update peak players.
	sprintf( szString, "Peak: %ld", SERVER_STATISTIC_GetMaxNumPlayers( ));
	SetDlgItemText( g_hStatisticDlg, IDC_MAXPLAYERSATONETIME, szString );

	// Update total frags.
	sprintf( szString, "Total frags: %ld", SERVER_STATISTIC_GetTotalFrags( ));
	SetDlgItemText( g_hStatisticDlg, IDC_TOTALFRAGS, szString );

	// Update uptime.
	long lData = SERVER_STATISTIC_GetTotalSecondsElapsed( );
	if ( lData >= WEEK )
		sprintf( szString, "Uptime: %ldw, %ldd, %ldh, %ldm, %lds", lData / WEEK, ( lData / DAY ) % 7, ( lData / HOUR ) % 24, ( lData / MINUTE ) % 60, lData % MINUTE );
	else if ( lData >= DAY )
		sprintf( szString, "Uptime: %ldd, %ldh, %ldm, %lds", lData / DAY, ( lData / HOUR ) % 24, ( lData / MINUTE ) % 60, lData % MINUTE );
	else if ( lData >= HOUR )
		sprintf( szString, "Uptime: %ldh, %ldm, %lds", ( lData / HOUR ) % 24, ( lData / MINUTE ) % 60, lData % MINUTE );
	else if ( lData >= MINUTE )
		sprintf( szString, "Uptime: %ldm, %lds", ( lData / MINUTE ) % 60, lData % MINUTE );
	else
		sprintf( szString, "Uptime: %lds", lData );

	SetDlgItemText( g_hStatisticDlg, IDC_TOTALUPTIME, szString );
}

//*****************************************************************************
//
void SERVERCONSOLE_SetCurrentMapname( const char *pszString )
{
	if ( !g_bServerLoaded )
	{
		g_bServerLoaded = true;
		EnableWindow( GetDlgItem( g_hDlg, IDC_PLAYERLIST ), true );
		EnableWindow( GetDlgItem( g_hDlg, IDC_CONSOLEBOX ), true );	
		serverconsole_UpdateSendButton( g_hDlg );
		SERVERCONSOLE_UpdateTitleString( sv_hostname.GetGenericRep( CVAR_String ).String );
	}

	FString fsMapMode = "";
	fsMapMode.Format( "%s | %s", strupr( GAMEMODE_GetName( GAMEMODE_GetCurrentMode( ))), pszString );
	SetDlgItemText( g_hDlg, IDC_MAPMODE, fsMapMode.GetChars( ));
}

//*****************************************************************************
//
void SERVERCONSOLE_SetupColumns( void )
{
	LVCOLUMN	ColumnData;

	if ( SendDlgItemMessage( g_hDlg, IDC_PLAYERLIST, LVM_GETCOLUMN, COLUMN_FRAGS, (LPARAM)&ColumnData ))
	{
		ColumnData.mask = LVCF_TEXT;

		if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNWINS )
			ColumnData.pszText = "Wins";
		else if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNFRAGS )
			ColumnData.pszText = "Frags";
		else if (( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNPOINTS ) || (( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNKILLS ) && ( zadmflags & ZADF_AWARD_DAMAGE_INSTEAD_KILLS )))
			ColumnData.pszText = "Points";
		else
			ColumnData.pszText = "Kills";

		SendDlgItemMessage( g_hDlg, IDC_PLAYERLIST, LVM_SETCOLUMN, COLUMN_FRAGS, (LPARAM)&ColumnData );
	}
	else
		Printf( "SERVERCONSOLE_SetupColumns: Couldn't get column!\n" );
}

//*****************************************************************************
//
void SERVERCONSOLE_ReListPlayers( void )
{
	LVITEM		Item;
	char		szString[MAXPLAYERNAME+1];
	LONG		lIndex;
	LONG		lIdx;

	// Find the player in the global player indicies array.
	for ( lIdx = 0; lIdx < MAXPLAYERS; lIdx++ )
	{
		g_lPlayerIndicies[lIdx] = -1;

		// Delete the list view item.
		SendDlgItemMessage( g_hDlg, IDC_PLAYERLIST, LVM_DELETEITEM, 0, 0 );
	}

	Item.mask = LVIF_TEXT;
	Item.iSubItem = COLUMN_NAME;
	Item.iItem = MAXPLAYERS;

	// Add each player.
	for ( lIdx = 0; lIdx < MAXPLAYERS; lIdx++ )
	{
		if ( playeringame[lIdx] == false )
			continue;

		sprintf( szString, "%s", players[lIdx].userinfo.GetName() );
		V_RemoveColorCodes( szString );
		Item.pszText = szString;

		lIndex = SendDlgItemMessage( g_hDlg, IDC_PLAYERLIST, LVM_INSERTITEM, 0, (LPARAM)&Item );
		if ( lIndex == -1 )
			return;

		g_lPlayerIndicies[lIndex] = lIdx;

		// Initialize all the fields for this player.
		SERVERCONSOLE_UpdatePlayerInfo( lIdx, UDF_NAME|UDF_FRAGS|UDF_PING|UDF_TIME );
	}
}

//*****************************************************************************
//
void SERVERCONSOLE_UpdatePlayerInfo( LONG lPlayer, ULONG ulUpdateFlags )
{
	LVITEM		Item;
	char		szString[MAXPLAYERNAME+1];
	LONG		lIndex = -1;
	LONG		lIdx;

	for ( lIdx = 0; lIdx < MAXPLAYERS; lIdx++ )
	{
		if ( g_lPlayerIndicies[lIdx] == lPlayer )
		{
			lIndex = lIdx;
			break;
		}
	}

	if ( lIndex == -1 )
		return;

	Item.mask = LVIF_TEXT;
	Item.iItem = lIndex;

	if ( ulUpdateFlags & UDF_NAME )
	{
		Item.iSubItem = COLUMN_NAME;
		sprintf( szString, "%s", players[lPlayer].userinfo.GetName() );
		Item.pszText = szString;
		V_RemoveColorCodes( szString );

		SendDlgItemMessage( g_hDlg, IDC_PLAYERLIST, LVM_SETITEM, lIndex, (LPARAM)&Item );
	}

	if ( ulUpdateFlags & UDF_FRAGS )
	{
		Item.iSubItem = COLUMN_FRAGS;
		if ( PLAYER_IsTrueSpectator( &players[lPlayer] ))
			sprintf( szString, "Spectating" );
		else if (( GAMEMODE_GetCurrentFlags( ) & GMF_PLAYERSONTEAMS ) && ( !players[lPlayer].bOnTeam ))
			sprintf( szString, "No team" );
		else if ( lastmanstanding || teamlms )
		{
			if ( players[lPlayer].health <=0 )
				sprintf( szString, "Dead" );
			else if ( lastmanstanding )
				sprintf( szString, "%ld", players[lPlayer].ulWins );
			else
				sprintf( szString, "%d", players[lPlayer].fragcount );
		}
		else if (( GAMEMODE_GetCurrentFlags( ) & GMF_PLAYERSEARNPOINTS ) || (( GAMEMODE_GetCurrentFlags( ) & GMF_PLAYERSEARNKILLS ) && ( zadmflags & ZADF_AWARD_DAMAGE_INSTEAD_KILLS )))
			sprintf( szString, "%ld", players[lPlayer].lPointCount );
		else if ( GAMEMODE_GetCurrentFlags( ) & GMF_PLAYERSEARNFRAGS )
			sprintf( szString, "%d", players[lPlayer].fragcount );
		else
			sprintf( szString, "%d", players[lPlayer].killcount );
		Item.pszText = szString;
		SendDlgItemMessage( g_hDlg, IDC_PLAYERLIST, LVM_SETITEM, lIndex, (LPARAM)&Item );
	}

	if ( ulUpdateFlags & UDF_PING )
	{
		Item.iSubItem = COLUMN_PING;
		if ( players[lPlayer].bIsBot )
			sprintf( szString, "Bot" );
		else
			sprintf( szString, "%ld", players[lPlayer].ulPing );
		Item.pszText = szString;

		SendDlgItemMessage( g_hDlg, IDC_PLAYERLIST, LVM_SETITEM, lIndex, (LPARAM)&Item );
	}

	if ( ulUpdateFlags & UDF_TIME )
	{
		Item.iSubItem = COLUMN_TIME;
		sprintf( szString, "%ld", ( players[lPlayer].ulTime / ( TICRATE * 60 )));
		Item.pszText = szString;

		SendDlgItemMessage( g_hDlg, IDC_PLAYERLIST, LVM_SETITEM, lIndex, (LPARAM)&Item );
	}
}

//*****************************************************************************
//
void serverconsole_InsertTimestamp( char *pszBuffer )
{
	pszBuffer[0] = 0;
	if (( sv_timestamp ) && ( gamestate == GS_LEVEL ))
	{
		time_t			CurrentTime;
		struct	tm		*pTimeInfo;
		char			szTimeStamp[32];
		char			*pszTimeStampFormats[] = { "%H:%M:%S", "%I:%M:%S %p", "%I:%M:%S %p", "%H:%M", "%I:%M %p", "%I:%M %p" };

		time( &CurrentTime );
		pTimeInfo = localtime( &CurrentTime );				
		strftime( szTimeStamp, 32, pszTimeStampFormats[ sv_timestampformat.GetGenericRep( CVAR_Int ).Int ], pTimeInfo );
		if ( sv_timestampformat == 2 || sv_timestampformat == 5 ) // With these two, the AM/PM signs are lowercase.
			strlwr( szTimeStamp );
		sprintf( pszBuffer, "[%s] ", szTimeStamp );
	}
}

//*****************************************************************************
//
void SERVERCONSOLE_Print( char *pszString )
{
	V_StripColors( pszString );
	bool bAppendChatMessage = ( strstr( pszString, "Unknown command" ) == pszString );

	// Start with a timestamp.
	char	szInputString[SERVERCONSOLE_TEXTLENGTH];	
	serverconsole_InsertTimestamp( szInputString );

	// Copy the input string, and convert the line endings to Windows format.
	char	c;	
	char	*psz = szInputString + strlen( szInputString );
	while ( 1 )
	{
		c = *pszString++;
		if ( c == '\0' )
		{
			*psz = c;
			break;
		}
		else if ( c == '\n' )
		{
			*psz++ = '\r';
			g_ulNumLines++;
		}

		*psz++ = c;
	}

	if ( bAppendChatMessage )
	{
		szInputString[ strlen( szInputString ) - 2] = 0;
		strcat( szInputString, ". To chat, start your message with a :\r\n" );
	}

	// Check where the scrollbars are, currently.
	LONG	lVisibleLine = SendDlgItemMessage( g_hDlg, IDC_CONSOLEBOX, EM_GETFIRSTVISIBLELINE, 0, 0 );
	LONG	lTotalLines = SendDlgItemMessage( g_hDlg, IDC_CONSOLEBOX, EM_GETLINECOUNT, 0, 0 );
	LONG	lLineDiff = lTotalLines - lVisibleLine;
	bool	bScroll = ( lLineDiff <= 12 );
	bool	bRecycled = false;

	// Append the new message to the text already in the log box.
	char	szBuffer[SERVERCONSOLE_TEXTLENGTH];
	GetDlgItemText( g_hDlg, IDC_CONSOLEBOX, szBuffer, sizeof( szBuffer ));

	// But first -- if the log box is already full, trim the older entries. (yuck)
	LONG	lDifference;
	psz = szBuffer;
	if (( lDifference = ( (LONG)strlen( szBuffer ) + (LONG)strlen( szInputString ) - SERVERCONSOLE_TEXTLENGTH )) >= 0 )
	{
		bRecycled = true;		
		while ( 1 )
		{
			psz++;
			lDifference--;
			if ( *psz == 0 )
				break;
			if ( lDifference < 0 )
			{
				while ( 1 )
				{
					if ( *psz == 0 )
					{
						psz++;
						break;
					}
					else if ( *psz == '\r' )
					{
						psz += 2;
						break;
					}
					psz++;
				}
				break;
			}
		}
	}

	// Append the text.
	char	szConsoleBuffer[SERVERCONSOLE_TEXTLENGTH];
	sprintf( szConsoleBuffer, "%s%s", psz, szInputString );
	SetDlgItemText( g_hDlg, IDC_CONSOLEBOX, szConsoleBuffer );

	// Finally, make sure the scrollbars are in the right place.
	if ( bScroll ) // If the user had scrolled all the way down, autoscroll.
		SendDlgItemMessage( g_hDlg, IDC_CONSOLEBOX, EM_LINESCROLL, 0, lTotalLines );		
	else if ( bRecycled && ( lVisibleLine > 0 )) // If they'd scrolled up, but we've trimmed the text, don't autoscroll and compensate. 
		SendDlgItemMessage( g_hDlg, IDC_CONSOLEBOX, EM_LINESCROLL, 0, lVisibleLine - 1 );		
	else // If they've scrolled up, don't autoscroll.
		SendDlgItemMessage( g_hDlg, IDC_CONSOLEBOX, EM_LINESCROLL, 0, lVisibleLine );	
}

#endif	// GUI_SERVER_CONSOLE
