//------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Skulltag Statsmaker Source
// Copyright (C) 2008 Rivecoder
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
// Date created: 8/15/08
//
//
// Filename: gui.cpp
//
// Description: Win32 code for the statsmaker's window.
//
//----------------------------------------------------------------------------------------------------------------------------------------------------

#ifdef NO_GUI
void GUI_CreateDialog( ) { };
void GUI_UpdateStatisticsDisplay( void ) { };
void GUI_SetWritingMode( bool bWriting ) { };
void GUI_UpdateTrayTooltip( const char *szTooltip ) { };
#else

#include "..\src\networkheaders.h"
#include "..\src\networkshared.h"
#include "main.h"
#include "gui.h"
#include "collector.h"
#include "resource.h"

#include <windows.h>
#include <uxtheme.h>
#include <shellapi.h>

// Look pretty under XP and Vista.
#pragma comment(linker, "\"/manifestdependency:type='Win32' ""name='Microsoft.Windows.Common-Controls' ""version='6.0.0.0' ""processorArchitecture='*' ""publicKeyToken='6595b64144ccf1df' ""language='*'\"")

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- VARIABLES -------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

extern	PORT_s					g_PortInfo[NUM_PORTS];
extern  HINSTANCE				g_hInst;
extern 	HANDLE					g_hThread;

HWND							g_hDlg_Overview = NULL;

static	NOTIFYICONDATA			g_NotifyIconData;
static	HICON					g_hSmallIcon = NULL;
static	char					g_szTooltip[128];

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- PROTOTYPES ------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

// (Uxtheme) Styles tabs in Windows XP/Vista. 
static	HRESULT					(__stdcall *pEnableThemeDialogTexture) ( HWND hwnd, DWORD dwFlags );

BOOL	CALLBACK				gui_MainDialogBoxCallback( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam );
BOOL	CALLBACK				tab_OverviewCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam );
BOOL	CALLBACK				tab_PortCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam );

static	void					gui_ToggleWindow( HWND hDlg );

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

//==========================================================================
//
// GUI_CreateDialog
//
// Creates the statsmaker dialog and starts collection when done.
//
//==========================================================================

void GUI_CreateDialog( )
{	
	// Load up uxtheme on XP so we can theme tabs correctly.
	HMODULE uxtheme = LoadLibrary( "uxtheme.dll" );
	if ( uxtheme != NULL )
		pEnableThemeDialogTexture = ( HRESULT (__stdcall *)(HWND,DWORD))GetProcAddress ( uxtheme, "EnableThemeDialogTexture" );

	sprintf( g_szTooltip, "Skulltats!" );

	// Show the dialog.
	DialogBox( g_hInst, MAKEINTRESOURCE( IDD_MAINDIALOG ), NULL, gui_MainDialogBoxCallback );
}

//==========================================================================
//
// gui_MainDialogBoxCallback
//
// Callback for the main dialog.
//
//==========================================================================

BOOL CALLBACK gui_MainDialogBoxCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	switch ( Message )
	{
	case WM_INITDIALOG:

		// Load the icon.
		g_hSmallIcon = (HICON)LoadImage( g_hInst,
				MAKEINTRESOURCE( IDI_ICON ),
				IMAGE_ICON,
				16,
				16,
				LR_SHARED );

		SendMessage( hDlg, WM_SETICON, (WPARAM)ICON_SMALL, (LPARAM)g_hSmallIcon );
		SendMessage( hDlg, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)LoadIcon( g_hInst, MAKEINTRESOURCE( IDI_ICON )));

		//========================
		// Set up the tab control.
		//========================

		HWND		edit;
		TCITEM		tcitem;
		RECT		tabrect, tcrect;
		LPNMHDR		nmhdr;
		tcitem.mask = TCIF_TEXT | TCIF_PARAM;
		edit = GetDlgItem( hDlg, IDC_TABC );

		GetWindowRect( edit, &tcrect );
		ScreenToClient( hDlg, (LPPOINT) &tcrect.left );
		ScreenToClient( hDlg, (LPPOINT) &tcrect.right );

		TabCtrl_GetItemRect( edit, 0, &tabrect );

		// Create the main tab.
		tcitem.pszText = "Overview";
		tcitem.lParam = (LPARAM) CreateDialogParam( g_hInst, MAKEINTRESOURCE( IDD_OVERVIEW ), hDlg, tab_OverviewCallback, (LPARAM) edit );
		TabCtrl_InsertItem( edit, 0, &tcitem );
		SetWindowPos( (HWND) tcitem.lParam, HWND_TOP, tcrect.left + 3, tcrect.top + tabrect.bottom + 3,
			tcrect.right - tcrect.left - 8, tcrect.bottom - tcrect.top - tabrect.bottom - 8, 0 );

		// Create a tab for each port.
		for ( unsigned int i = 0; i < NUM_PORTS; i++ )
		{
			tcitem.pszText = g_PortInfo[i].szName;			
			tcitem.lParam = (LPARAM) CreateDialogParam( g_hInst, MAKEINTRESOURCE( IDD_ZANDRONUM ), hDlg, tab_PortCallback, (LPARAM) i );
			TabCtrl_InsertItem( edit, i + 1, &tcitem );		
			SetWindowPos( (HWND) tcitem.lParam, HWND_TOP, tcrect.left + 3, tcrect.top + tabrect.bottom + 3,
			tcrect.right - tcrect.left - 8, tcrect.bottom - tcrect.top - tabrect.bottom - 8, 0 );
		}
		
		//==============================
		// Create the notification icon.
		//==============================

		ZeroMemory( &g_NotifyIconData, sizeof( g_NotifyIconData ));
		g_NotifyIconData.cbSize = sizeof( g_NotifyIconData );
		g_NotifyIconData.hWnd = hDlg;
		g_NotifyIconData.uID = 0;
		g_NotifyIconData.uFlags = NIF_ICON|NIF_MESSAGE|NIF_TIP;
		g_NotifyIconData.uCallbackMessage = UWM_TRAY_TRAYID;
		g_NotifyIconData.hIcon = g_hSmallIcon;			
		lstrcpy( g_NotifyIconData.szTip, g_szTooltip );
		Shell_NotifyIcon( NIM_ADD, &g_NotifyIconData );

		break;
	case WM_CLOSE:

		Shell_NotifyIcon( NIM_DELETE, &g_NotifyIconData );
		EndDialog( hDlg, -1 );
		CloseHandle( g_hThread );
		exit( 0 );
		break;
	case WM_DESTROY:

		Shell_NotifyIcon( NIM_DELETE, &g_NotifyIconData );
		PostQuitMessage( 0 );
		break;
	case WM_SYSCOMMAND:

		if ( wParam == SC_MINIMIZE )
		{
			// Hide the window.
			ShowWindow( hDlg, SW_HIDE );
			break;
		}

		DefWindowProc( hDlg, Message, wParam, lParam );
		break;
	case UWM_TRAY_TRAYID:

		switch ( lParam )
		{
		case WM_LBUTTONDOWN:

			return TRUE;
		case WM_LBUTTONDBLCLK:

			gui_ToggleWindow( hDlg );			
			return TRUE;
		case WM_RBUTTONUP:

			{
				// Show a little menu.
				HMENU	hMenu = CreatePopupMenu();
				POINT	pt;					
		
				AppendMenu( hMenu, MF_STRING, IDR_TOGGLE, "Show/Hide" );
				// AppendMenu( hMenu, MF_STRING, IDR_BROWSE, "Browse files" );				
				AppendMenu( hMenu, MF_SEPARATOR, 0, 0 );
				AppendMenu( hMenu, MF_STRING, IDR_EXIT, "Exit" );								

				// Show it, and get the selected item.
				GetCursorPos( &pt );
				int iSelection = ::TrackPopupMenu( hMenu, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RETURNCMD | TPM_HORIZONTAL, 
					pt.x, pt.y, 0, hDlg, NULL );
				DestroyMenu( hMenu );

				if ( iSelection == IDR_EXIT )
				{					
					Shell_NotifyIcon( NIM_DELETE, &g_NotifyIconData );
					PostQuitMessage( 0 );
					EndDialog( hDlg, -1 );
					CloseHandle( g_hThread );
					exit( 0 );
				}
				else if ( iSelection == IDR_TOGGLE )
					gui_ToggleWindow( hDlg );

			}
			break;
		default:

			break;
		}
		return FALSE;
	case WM_NOTIFY:

		nmhdr = (LPNMHDR)lParam;

		// Tab switching.
		if (nmhdr->idFrom == IDC_TABC)
		{
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
		break;
	default:

		return FALSE;
	}

	return TRUE;
}

//==========================================================================
//
// tab_OverviewCallback
//
// Callback for the first tab.
//
//==========================================================================

BOOL CALLBACK tab_OverviewCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	switch ( Message )
	{
	case WM_INITDIALOG:

		char		szString[256];
		g_hDlg_Overview = hDlg;

		// Enable tab gradients on XP and later.
		if ( pEnableThemeDialogTexture != NULL )
			pEnableThemeDialogTexture ( hDlg, ETDT_ENABLETAB );

		// Initialize the server console text.
		sprintf( szString, "                    === Z A N D R O N U M | Statsmaker ===" );
		SetDlgItemText( hDlg, IDC_CONSOLEBOX, szString );
		Printf( "\n\n" );
		Printf( "Welcome!\n" );

		GUI_UpdateStatisticsDisplay( );
		g_hThread = CreateThread( NULL, 0, COLLECTOR_StartCollecting, 0, 0, 0 );
		break;
	}

	return FALSE;
}

//==========================================================================
//
// tab_PortCallback
//
// Generic callback method used for the tab of each port. lParam = port index.
//
//==========================================================================

BOOL CALLBACK tab_PortCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	switch ( Message )
	{
	case WM_INITDIALOG:

		g_PortInfo[(int) lParam].hDlg = hDlg;		

		// Enable tab gradients on XP and later.
		if ( pEnableThemeDialogTexture != NULL )
			pEnableThemeDialogTexture ( hDlg, ETDT_ENABLETAB );
		
		break;
	}

	return FALSE;
}

//==========================================================================
//
// GUI_SetWritingMode
//
// Updates a label on the "Overview" tab signifying whether we're writing the stats file.
//
//==========================================================================

void GUI_SetWritingMode( bool bWriting )
{
	SetDlgItemText( g_hDlg_Overview, IDC_OVERVIEW_WRITING, bWriting ? "Writing" : "" );
}

//==========================================================================
//
// GUI_UpdateStatisticsDisplay
//
// Updates all of the statistics.
//
//==========================================================================

void GUI_UpdateStatisticsDisplay( void )
{
	char	szString[128];
	char	szTooltip[128];
	int		iPortOnTopToday = -1, iPortOnTopNow = -1, iHighestCount = 0;
	float	fBestAverage = 0.0;

	strcpy( szTooltip, "" );

	// Update the statistics for each port.
	for ( int i = 0; i < NUM_PORTS; i++ )
	{
		float		fAverageNumPlayers = 0.0, fAverageNumServers = 0.0;
		int			iMaxNumPlayers = 0, iMaxNumServers = 0;

		//======================================
		// Recalculate the averages and records.
		//======================================

		if ( g_PortInfo[i].aQueryInfo.Size( ) > 0 )
		{
			int lTotalNumPlayers = 0;
			int lTotalNumServers = 0;

			// Go through all the queries.
			for ( unsigned int q = 0; q < g_PortInfo[i].aQueryInfo.Size( ); q++ )
			{
				lTotalNumPlayers += g_PortInfo[i].aQueryInfo[q].qTotal.lNumPlayers;
				lTotalNumServers += g_PortInfo[i].aQueryInfo[q].qTotal.lNumServers;

				if ( g_PortInfo[i].aQueryInfo[q].qTotal.lNumPlayers > iMaxNumPlayers )
					iMaxNumPlayers = g_PortInfo[i].aQueryInfo[q].qTotal.lNumPlayers;

				if ( g_PortInfo[i].aQueryInfo[q].qTotal.lNumServers > iMaxNumServers )
					iMaxNumServers = g_PortInfo[i].aQueryInfo[q].qTotal.lNumServers;
			}
			
			fAverageNumPlayers = (float)lTotalNumPlayers / (float)g_PortInfo[i].aQueryInfo.Size( );
			fAverageNumServers = (float)lTotalNumServers / (float)g_PortInfo[i].aQueryInfo.Size( );

			// Record the port that's "on top" with the most players today.
			if ( fAverageNumPlayers == fBestAverage )
				iPortOnTopToday = -1;
			else if ( fAverageNumPlayers > fBestAverage )
			{
				fBestAverage = fAverageNumPlayers;
				iPortOnTopToday = i;
			}
		}

		//===========================
		// Update the "Overview" tab.
		//===========================

		// "Now"
		sprintf( szString, "%s players: %d", g_PortInfo[i].szName, g_PortInfo[i].aQueryInfo.Size( ) ? g_PortInfo[i].aQueryInfo[g_PortInfo[i].aQueryInfo.Size( ) - 1].qTotal.lNumPlayers : 0 );
		SetDlgItemText( g_hDlg_Overview, g_PortInfo[i].iOverviewNowLabelID, szString );
		EnableWindow( GetDlgItem( g_hDlg_Overview, g_PortInfo[i].iOverviewNowLabelID ), g_PortInfo[i].bEnabled );

		// "Today"
		sprintf( szString, "%s players: %3.2f", g_PortInfo[i].szName, fAverageNumPlayers );
		SetDlgItemText( g_hDlg_Overview, g_PortInfo[i].iOverviewTodayLabelID, szString );
		EnableWindow( GetDlgItem( g_hDlg_Overview, g_PortInfo[i].iOverviewTodayLabelID ), g_PortInfo[i].bEnabled );

		//=======================
		// Update the port's tab.
		//=======================

		if ( g_PortInfo[i].hDlg != NULL )
		{
			// Update the current information.
			if ( g_PortInfo[i].aQueryInfo.Size( ) > 0 )
			{				
				sprintf( szString, "Number of queries: %d", g_PortInfo[i].aQueryInfo.Size( ) );
				SetDlgItemText( g_PortInfo[i].hDlg, IDC_NUMQUERIES, szString );

				sprintf( szString, "Current players: %d", g_PortInfo[i].aQueryInfo[g_PortInfo[i].aQueryInfo.Size( ) - 1].qTotal.lNumPlayers );
				SetDlgItemText( g_PortInfo[i].hDlg, IDC_CURRENTPLAYERS, szString );

				sprintf( szString, "Current servers: %d", g_PortInfo[i].aQueryInfo[g_PortInfo[i].aQueryInfo.Size( ) - 1].qTotal.lNumServers );
				SetDlgItemText( g_PortInfo[i].hDlg, IDC_CURRENTSERVERS, szString );

				// Record the port that's "on top" with the most players now.
				if ( g_PortInfo[i].aQueryInfo[g_PortInfo[i].aQueryInfo.Size( ) - 1].qTotal.lNumPlayers == iHighestCount )
					iPortOnTopNow = -1;
				else if ( g_PortInfo[i].aQueryInfo[g_PortInfo[i].aQueryInfo.Size( ) - 1].qTotal.lNumPlayers > iHighestCount )
				{
					iHighestCount = g_PortInfo[i].aQueryInfo[g_PortInfo[i].aQueryInfo.Size( ) - 1].qTotal.lNumPlayers;
					iPortOnTopNow = i;
				}
			}

			sprintf( szString, "Average players: %3.2f", fAverageNumPlayers );
			SetDlgItemText( g_PortInfo[i].hDlg, IDC_AVERAGEPLAYERS, szString );

			sprintf( szString, "Average servers: %3.2f", fAverageNumServers );
			SetDlgItemText( g_PortInfo[i].hDlg, IDC_AVERAGESERVERS, szString );

			sprintf( szString, "Max players: %d", iMaxNumPlayers );
			SetDlgItemText( g_PortInfo[i].hDlg, IDC_MAXPLAYERS, szString );

			sprintf( szString, "Max servers: %d", iMaxNumServers );
			SetDlgItemText( g_PortInfo[i].hDlg, IDC_MAXSERVERS, szString );

			// If the port is disabled, grey all the labels.
			EnableWindow( GetDlgItem( g_PortInfo[i].hDlg, IDC_AVERAGEPLAYERS ), g_PortInfo[i].bEnabled );
			EnableWindow( GetDlgItem( g_PortInfo[i].hDlg, IDC_AVERAGESERVERS ), g_PortInfo[i].bEnabled );
			EnableWindow( GetDlgItem( g_PortInfo[i].hDlg, IDC_MAXSERVERS ), g_PortInfo[i].bEnabled );
			EnableWindow( GetDlgItem( g_PortInfo[i].hDlg, IDC_MAXPLAYERS ), g_PortInfo[i].bEnabled );
			EnableWindow( GetDlgItem( g_PortInfo[i].hDlg, IDC_NUMQUERIES ), g_PortInfo[i].bEnabled && g_PortInfo[i].aQueryInfo.Size( ));
			EnableWindow( GetDlgItem( g_PortInfo[i].hDlg, IDC_CURRENTPLAYERS ), g_PortInfo[i].bEnabled && g_PortInfo[i].aQueryInfo.Size( ));
			EnableWindow( GetDlgItem( g_PortInfo[i].hDlg, IDC_CURRENTSERVERS ), g_PortInfo[i].bEnabled && g_PortInfo[i].aQueryInfo.Size( ));
		}

		//================================
		// Update the tray icon's tooltip.
		//================================

		
		/* [RC] Tooltips can't be more than 128 characters long, so just show the Zandronum info.
		// if ( g_PortInfo[i].bEnabled )
		*/
		if ( i == PORT_ZANDRONUM )
		{
			if ( strlen( szTooltip ))
				strcat( szTooltip, "\n" );

			if ( g_PortInfo[i].aQueryInfo.Size( ) > 0 )
				sprintf( szTooltip, "%s%s: %d currently playing; today's average is %3.2f", szTooltip, g_PortInfo[i].szName, g_PortInfo[i].aQueryInfo[g_PortInfo[i].aQueryInfo.Size( ) - 1].qTotal.lNumPlayers, fAverageNumPlayers );
			else
				sprintf( szTooltip, "%s%s: no queries yet", szTooltip, g_PortInfo[i].szName );
		}


		ShowWindow( GetDlgItem( g_hDlg_Overview, g_PortInfo[i].iOnTopNowIconID ), SW_HIDE );
		ShowWindow( GetDlgItem( g_hDlg_Overview, g_PortInfo[i].iOnTopTodayIconID ), SW_HIDE );
	}

	GUI_UpdateTrayTooltip( szTooltip );

	//=========================
	// Show the "on top" icons.
	//=========================

	ShowWindow( GetDlgItem( g_hDlg_Overview, IDC_ONTOPNOW ), ( iPortOnTopNow != -1 ) ? SW_SHOW : SW_HIDE );
	ShowWindow( GetDlgItem( g_hDlg_Overview, IDC_ONTOPTODAY ), ( iPortOnTopToday != -1 ) ? SW_SHOW : SW_HIDE );
	if ( iPortOnTopNow >= 0 )
		ShowWindow( GetDlgItem( g_hDlg_Overview, g_PortInfo[iPortOnTopNow].iOnTopNowIconID ), SW_SHOW );

	if ( iPortOnTopToday >= 0 )
		ShowWindow( GetDlgItem( g_hDlg_Overview, g_PortInfo[iPortOnTopToday].iOnTopTodayIconID ), SW_SHOW );	
}

//==========================================================================
//
// GUI_UpdateTrayTooltip
//
// Sets the tooltip of the tray icon.
//
//==========================================================================

void GUI_UpdateTrayTooltip( const char *pszTooltip )
{
	strncpy( g_szTooltip, pszTooltip, 128 );
	lstrcpy( g_NotifyIconData.szTip, g_szTooltip );
	Shell_NotifyIcon( NIM_MODIFY, &g_NotifyIconData );
}

//==========================================================================
//
// gui_ToggleWindow
//
// Shows or hides the main window.
//
//==========================================================================

static void gui_ToggleWindow( HWND hDlg )
{
	if ( IsWindowVisible( hDlg ))
		ShowWindow( hDlg, SW_HIDE );
	else
	{
		ShowWindow( hDlg, SW_SHOW );
		SetActiveWindow( hDlg );
		SetForegroundWindow( hDlg );
	}
}
#endif
