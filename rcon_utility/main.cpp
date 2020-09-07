//------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Skulltag RCON Utility Source
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
// Date created: 8/16/08
//
//
// Filename: main.cpp
//
// Description:
//
//----------------------------------------------------------------------------------------------------------------------------------------------------

#define IN_RCON_UTILITY

#include <list>
#include <time.h>
#include "../src/networkheaders.h"
#include "../masterserver/network.h"
#include "zstring.h"
#include "../src/configfile.h"
#include "../src/sv_rcon.h"
#include "MD5Checksum.h"
#include "main.h"
#include "resource.h"

#include <windows.h>
#include <commctrl.h>
#include <time.h>
#include <mmsystem.h>
#include <uxtheme.h>
#include <shellapi.h>

// Look pretty under XP and Vista.
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- VARIABLES -------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

// Thread handle for the main thread that listens for messages, among other things.
static	HANDLE					g_hThread;

static	HINSTANCE				g_hInst;
static	HWND					g_hDlg;
static	HWND					g_hDlgStatusBar;

static	STATE_e					g_State;
static	NETADDRESS_s			g_ServerAddress;
static	NETBUFFER_s				g_MessageBuffer;
static	char					g_szPassword[128];
static	char					g_szHostname[256];
static	char					g_szMapname[32];
static	std::list<FString>		g_InitialPlayers;
static	int						g_iServerProtocolVersion;
static	int						g_iNumPlayers;
static	int						g_iNumOtherAdmins;
static	int						g_iLines;
static	std::list<FString>		g_RecentConsoleHistory;
static	bool					g_bRCONDialogVisible = false;
static	time_t					g_tLastIncorrectLogin;
static	std::vector<FAVORITE_s>	g_Favorites;

// When did we last refresh the connect button?
static	long					g_lLastCountdownTime;

// When did we last send a command to the server?
static	time_t					g_tLastSentCommand;

static	FConfigFile				g_Config;

// How many times have we tried to reconnect?
static	int						g_iRetries = 0;

// A solid white brush that paints the top of the "connect" dialog.
static	HBRUSH					g_hWhiteBrush;

// Tray icon data.
static	NOTIFYICONDATA			g_NotifyIconData;
static	HICON					g_hSmallIcon = NULL;
static	char					g_szTooltip[128];

// Menus.
static	HMENU					g_hMainMenu;
static	HMENU					g_hFavoritesMenu;
static	HMENU					g_hTrayMenu;

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- PROTOTYPES ------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

BOOL	CALLBACK		main_ConnectDialogCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam );
BOOL	CALLBACK		main_RCONDialogCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam );
BOOL	CALLBACK		main_AboutDialogCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam );
DWORD	WINAPI			main_Loop( LPVOID );
DWORD	WINAPI			main_Show( LPVOID );
static	void			main_Quit( );
static	void			main_AttemptConnection( );
static	void			main_Disconnect( );
static	void			main_UpdateStatusbar( const char *pszMessage );
static	void			main_UpdateServerStatus( );
static	void			main_ShowMessage( const char *pszMessage, UINT uType );
static	void			main_EnableConnectionButtons( BOOL bEnable );
static	void			main_ParseCommands( BYTESTREAM_s *pByteStream );
static	void			main_ParseUpdate( BYTESTREAM_s *pByteStream );
static	void			main_SendPassword( const char *pszSalt );
static	void			main_ToggleWindow( HWND hDlg );
static	void			main_UpdateTrayTooltip( const char *szTooltip );
static	BOOL			main_TrayIconClicked( HWND hDlg, LPARAM lParam );
static	void			main_SetState( STATE_e NewState );
static	void			main_ConnectToFavorite( int iIndex );

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

//==========================================================================
//
// WinMain
//
// Application entry point.
//
//==========================================================================

int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd )
{
	g_hInst = hInstance;
	g_Config.ChangePathName( "settings.ini" );
	g_Config.LoadConfigFile( NULL, NULL );

	// Read the user's favorites.
	if ( g_Config.SetSection("Favorites"))
	{			
		const char *key;
		const char *value;

		while ( g_Config.NextInSection( key, value ))
		{
			FAVORITE_s fav;

			strncpy( fav.szName, key, 128 );
			fav.szName[128] = 0;
			strncpy( fav.szAddress, value, strchr( value, '/' ) - value );
			fav.szAddress[strchr( value, '/' ) - value] = 0;
			strncpy( fav.szPassword, strchr( value, '/' ) + 1, 128 );
			fav.szPassword[128] = 0;

			g_Favorites.push_back( fav );			
		}
	}

	NETWORK_Construct( 99999 );
	main_SetState( STATE_WAITING );
	DialogBox( g_hInst, MAKEINTRESOURCE( IDD_CONNECTDIALOG ), NULL, main_ConnectDialogCallback );
}

//==========================================================================
//
// main_Quit
//
// Exits the program.
//
//==========================================================================

static void main_Quit( )
{
	if ( g_State == STATE_CONNECTED )
		main_Disconnect( );

	Shell_NotifyIcon( NIM_DELETE, &g_NotifyIconData );
	EndDialog( g_hDlg, -1 );
	CloseHandle( g_hThread );
	exit( 0 );
}

//==========================================================================
//
// main_Loop
//
// Waits for packets and parses them, ad infinity. Also does some time-related tasks.
//
//==========================================================================

DWORD WINAPI main_Loop( LPVOID )
{
	char	szBuffer[128];

	while ( 1 )
	{		
		while ( NETWORK_GetPackets( ))
		{
			// Set up our byte stream.
			BYTESTREAM_s *pByteStream = &NETWORK_GetNetworkMessageBuffer( )->ByteStream;
			pByteStream->pbStream = NETWORK_GetNetworkMessageBuffer( )->pbData;
			pByteStream->pbStreamEnd = pByteStream->pbStream + NETWORK_GetNetworkMessageBuffer( )->ulCurrentSize;

			// Parse the packet, but only if it came from the server we're trying to reach. Ignore everything else.
			 if ( g_State > STATE_WAITING && NETWORK_GetFromAddress().Compare( g_ServerAddress ))
				main_ParseCommands( pByteStream );
		}
		
		time_t tNow = time( 0 );
		
		// Refresh the "connect" button.
		if ( g_tLastIncorrectLogin > 0 && ( timeGetTime( ) - g_lLastCountdownTime > 250 ))
		{
			g_lLastCountdownTime = timeGetTime( );
			if ( tNow - g_tLastIncorrectLogin < BAD_QUERY_IGNORE_TIME )
				sprintf( szBuffer, "Connect [%d]", BAD_QUERY_IGNORE_TIME - ( tNow - g_tLastIncorrectLogin ));
			else
				strcpy( szBuffer, "Connect" );

			SetDlgItemText( g_hDlg, IDOK, szBuffer );
			EnableWindow( GetDlgItem( g_hDlg, IDOK ), ( tNow - g_tLastIncorrectLogin >= BAD_QUERY_IGNORE_TIME ) && ( g_State == STATE_WAITING ));
		}

		// If we've been waiting for a while without response, try again, then error out.
		if ( g_State == STATE_CONNECTING && ( tNow - g_tLastSentCommand ) > 2 )
		{
			if ( g_iRetries < 3 )
			{
				main_AttemptConnection();
				g_iRetries++;
				sprintf( szBuffer, "Retrying (%d) to reach %s...", g_iRetries, g_ServerAddress.ToString() );
				main_UpdateStatusbar( szBuffer );
				main_UpdateTrayTooltip( szBuffer );
			}
			else
			{
				main_ShowMessage( "That address doesn't seem valid; there was no response.", MB_ICONEXCLAMATION );
				main_SetState( STATE_WAITING );
				main_EnableConnectionButtons( TRUE );
				g_iRetries = 0;
			}
		}

		// If we haven't sent a message recently, let the server know we're still here.
		if ( g_State == STATE_CONNECTED && ( tNow - g_tLastSentCommand ) > RCON_CLIENT_TIMEOUT_TIME / 4 )
		{
			NETWORK_ClearBuffer( &g_MessageBuffer );
			NETWORK_WriteByte( &g_MessageBuffer.ByteStream, CLRC_PONG );
			NETWORK_LaunchPacket( &g_MessageBuffer, g_ServerAddress );
			time( &g_tLastSentCommand );
		}

		Sleep( 200 );
	}

}

//==========================================================================
//
// main_Show
//
// Creates the second dialog. Unfortunately, there doesn't seem to be a better way of doing this without stopping the main_Loop thread.
//
//==========================================================================

DWORD WINAPI main_Show( LPVOID )
{
	DialogBox( g_hInst, MAKEINTRESOURCE( IDD_RCONDIALOG ), NULL, main_RCONDialogCallback );
	return 0;
}

//==========================================================================
//
// main_PaintRectangle
//
// Paints a certain section of the dialog a certain color.
// From http://www.catch22.net/tuts/tips.asp
//
//==========================================================================

void main_PaintRectangle( HDC hDC, RECT *rect, COLORREF color )
{
    COLORREF oldcr = SetBkColor( hDC, color );
    ExtTextOut( hDC, 0, 0, ETO_OPAQUE, rect, "", 0, 0 );
    SetBkColor( hDC, oldcr );
}

//==========================================================================
//
// main_ConnectDialogCallback
//
// Callback for the "connect" dialog box.
//
//==========================================================================

BOOL CALLBACK main_ConnectDialogCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	char	szBuffer[128];

	switch ( Message )
	{
	case WM_CTLCOLORSTATIC:

		switch ( GetDlgCtrlID( (HWND) lParam ))
		{
		// Paint these two labels white.
		case IDC_INTROTEXT:
		case IDC_DESCTEXT:

			return (LRESULT) g_hWhiteBrush;
		// Ignore everything else.
		default:

			return NULL;
		}
		break;
	case WM_PAINT:
		{
			// Paint the top of the form white.
			PAINTSTRUCT Ps;
			RECT r;
			r.left = 0;
			r.top = 3;
			r.bottom = 55;
			r.right = 400;
			main_PaintRectangle( BeginPaint(hDlg, &Ps), &r, RGB(255, 255, 255));
		}
		break;
	case WM_INITDIALOG:

		{
			g_hDlg = hDlg;

			// Load the icon.
			SendMessage( hDlg, WM_SETICON, (WPARAM)ICON_SMALL, (LPARAM) (HICON) LoadImage( g_hInst,	MAKEINTRESOURCE( AAA_MAIN_ICON ), IMAGE_ICON, 16, 16, LR_SHARED ));
			SendMessage( hDlg, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)LoadIcon( g_hInst, MAKEINTRESOURCE( AAA_MAIN_ICON )));

			//==============================
			// Create the notification icon.
			//==============================

			ZeroMemory( &g_NotifyIconData, sizeof( g_NotifyIconData ));
			g_NotifyIconData.cbSize = sizeof( g_NotifyIconData );
			g_NotifyIconData.hWnd = hDlg;
			g_NotifyIconData.uID = 0;
			g_NotifyIconData.uFlags = NIF_ICON|NIF_MESSAGE|NIF_TIP;
			g_NotifyIconData.uCallbackMessage = UWM_TRAY_TRAYID;
			g_NotifyIconData.hIcon =  (HICON) LoadImage( g_hInst,	MAKEINTRESOURCE( AAA_MAIN_ICON ), IMAGE_ICON, 16, 16, LR_SHARED );			
			lstrcpy( g_NotifyIconData.szTip, g_szTooltip );
			Shell_NotifyIcon( NIM_ADD, &g_NotifyIconData );

			//==================
			// Create the menus.
			//==================
		
			// Create the favorites menu.
			g_hFavoritesMenu = CreatePopupMenu( );
			int iIndex = 1;
			for( std::vector<FAVORITE_s>::iterator i = g_Favorites.begin(); i != g_Favorites.end(); ++i )			
				AppendMenu( g_hFavoritesMenu, MF_STRING, IDR_DYNAMIC_MENU + iIndex++, (LPCTSTR)(&(*i->szName)) );

			// Create the tray menu.
			g_hTrayMenu = CreatePopupMenu( );
			AppendMenu( g_hTrayMenu, MF_STRING, IDR_TOGGLE, "Show/Hide" );
			AppendMenu( g_hTrayMenu, MF_STRING|MF_POPUP, (UINT)g_hFavoritesMenu, "Favorites");
			AppendMenu( g_hTrayMenu, MF_SEPARATOR, 0, 0 );
			AppendMenu( g_hTrayMenu, MF_STRING, IDR_EXIT, "Exit" );

			// Create the file menu.
			HMENU hFileMenu = CreatePopupMenu( );
			AppendMenu( hFileMenu, MF_STRING, IDR_EXIT, "Exit" );

			// Create the file menu.
			HMENU hHelpMenu = CreatePopupMenu( );
			AppendMenu( hHelpMenu, MF_STRING, IDR_ABOUT, "About..." );

			// Create the main menu.
			g_hMainMenu = CreateMenu( );
			AppendMenu( g_hMainMenu, MF_STRING|MF_POPUP, (UINT)hFileMenu, "File" );
			AppendMenu( g_hMainMenu, MF_STRING|MF_POPUP, (UINT)g_hFavoritesMenu, "Favorites");
			AppendMenu( g_hMainMenu, MF_STRING|MF_POPUP, (UINT)hHelpMenu, "Help");
			AppendMenu( g_hMainMenu, MF_SEPARATOR, 0, 0 );
			SetMenu( hDlg, g_hMainMenu );

			// Set up the status bar.
			g_hDlgStatusBar = CreateStatusWindow( WS_CHILD | WS_VISIBLE, (LPCTSTR)NULL, hDlg, IDC_STATIC );

			// Set up the top, white section.
			SendMessage( GetDlgItem( g_hDlg, IDC_INTROTEXT ), WM_SETFONT, (WPARAM) CreateFont( 13, 0, 0, 0, 600, 0, 0, 0, 0, 0, 0, 0, 0, "Tahoma" ), (LPARAM) 1 );
			LOGBRUSH LogBrush;
			LogBrush.lbStyle = BS_SOLID;
			LogBrush.lbColor = RGB( 255, 255, 255 );
			g_hWhiteBrush = CreateBrushIndirect( &LogBrush );

			// Load the server address that was used last time.
			if ( g_Config.HaveSections( ) && g_Config.SetSection( "Settings", true ) && g_Config.GetValueForKey( "LastServer" ) )
				SetDlgItemText( hDlg, IDC_SERVERIP, g_Config.GetValueForKey( "LastServer" ) );

		}
		break;
	case WM_COMMAND:

			// Selecting a favorite from the menu?
			if ( LOWORD( wParam ) > IDR_DYNAMIC_MENU && LOWORD( wParam ) <= IDR_DYNAMIC_MENU + g_Favorites.size( ))
			{
				main_ConnectToFavorite( LOWORD( wParam ) - IDR_DYNAMIC_MENU - 1 );
				return TRUE;
			}

			switch ( LOWORD( wParam ))
			{
			// This also occurs when esc is pressed.
			case IDCANCEL:

				if ( g_State == STATE_CONNECTING )
				{
					main_SetState( STATE_WAITING );
					main_EnableConnectionButtons( TRUE );
					main_UpdateStatusbar( "Cancelled." );
				}
				else
					main_Quit( );
				break;
			// The "connect" button.
			case IDOK:	

				// Disable all the inputs.
				main_EnableConnectionButtons( FALSE );

				// Read in what the user gave us.
				GetDlgItemText( hDlg, IDC_SERVERIP, szBuffer, 128 );
				g_ServerAddress = NETADDRESS_s::FromString( szBuffer );
				GetDlgItemText( hDlg, IDC_PASSWORD, g_szPassword, 128 );

				// If the user didn't specify a port, use the default one.
				if ( g_ServerAddress.usPort == 0 )
					g_ServerAddress.SetPort( DEFAULT_SERVER_PORT );

				// Do some quick error checking.
				if ( !strlen( szBuffer ))
					MessageBox( hDlg, "You should probably enter a server address.", "Input error.", MB_ICONEXCLAMATION );
				else if ( strlen( g_szPassword ) < 4 )
					MessageBox( hDlg, "RCON passwords must be at least four characters long.", "Input error.", MB_ICONEXCLAMATION );
				else
				{
					main_AttemptConnection( );
					break;
				}

				// Re-enable the form so the user can try again.
				main_EnableConnectionButtons( TRUE );
				break;
			case IDR_EXIT:
				
				main_Quit( );
				break;
			case IDR_ABOUT:

				DialogBox( g_hInst, MAKEINTRESOURCE( IDD_ABOUTDIALOG ), hDlg, main_AboutDialogCallback );
				break;
			}
			break;
	case WM_SYSCOMMAND:

		// Hide the window when minimized.
		if ( wParam == SC_MINIMIZE )
			ShowWindow( hDlg, SW_HIDE );
		else
			DefWindowProc( hDlg, Message, wParam, lParam );
		break;
	case WM_CLOSE:

		main_Quit( );
		break;
	case WM_DESTROY:

		Shell_NotifyIcon( NIM_DELETE, &g_NotifyIconData );
		PostQuitMessage( 0 );
		break;
	case UWM_TRAY_TRAYID:

		return main_TrayIconClicked( hDlg, lParam );		
	default:

		return FALSE;
	}

	return TRUE;
}

//==========================================================================
//
// main_TrayIconClicked
//
// Toggles the window, or shows a menu.
//
//==========================================================================

static BOOL main_TrayIconClicked( HWND hDlg, LPARAM lParam )
{
	switch ( lParam )
	{
	case WM_LBUTTONDOWN:

		return TRUE;
	case WM_LBUTTONDBLCLK:

		main_ToggleWindow( g_hDlg );			
		return TRUE;
	case WM_RBUTTONUP:

		{
			// Show the tray menu.
			POINT	pt;
			GetCursorPos( &pt );
			int iSelection = ::TrackPopupMenu( g_hTrayMenu, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RETURNCMD | TPM_HORIZONTAL, 
				pt.x, pt.y, 0, hDlg, NULL );

			if ( iSelection == IDR_EXIT )	
				main_Quit( );
			else if ( iSelection == IDR_TOGGLE )
				main_ToggleWindow( g_hDlg );
			else if ( iSelection > IDR_DYNAMIC_MENU )
				main_ConnectToFavorite( iSelection - IDR_DYNAMIC_MENU - 1 );
		}
	}

	return FALSE;
}

//==========================================================================
//
// main_ConnectToFavorite
//
// Connects to the given favorite.
//
//==========================================================================

static void main_ConnectToFavorite( int iIndex )
{
	// Update gui.
	main_EnableConnectionButtons( FALSE );
	SetDlgItemText( g_hDlg, IDC_SERVERIP, g_Favorites[iIndex].szAddress );
	SetDlgItemText( g_hDlg, IDC_PASSWORD, g_Favorites[iIndex].szPassword );

	// Connect.
	g_ServerAddress = NETADDRESS_s::FromString( g_Favorites[iIndex].szAddress );
	strncpy( g_szPassword, g_Favorites[iIndex].szPassword, 127 );
	main_AttemptConnection( );
}

//==========================================================================
//
// main_UpdateStatusbar
//
// Sets the statusbar's text.
//
//==========================================================================

static void main_UpdateStatusbar( const char *pszMessage )
{
	SendMessage( g_hDlgStatusBar, SB_SETTEXT, (WPARAM)0, (LPARAM) pszMessage );
}

//==========================================================================
//
// main_ShowMessage
//
// Shows a message box, and updates the status bar.
//
//==========================================================================

static void main_ShowMessage( const char *pszMessage, UINT uType )
{
	main_UpdateStatusbar( pszMessage );
	MessageBox( g_hDlg, pszMessage, "RCON utility", uType );
}

//==========================================================================
//
// main_EnableConnectionButtons
//
// Enables or disables all the items on the "connect" dialog.
//
//==========================================================================

static void main_EnableConnectionButtons( BOOL bEnable )
{
	time_t tNow;
	time( &tNow );

	EnableWindow( GetDlgItem( g_hDlg, IDOK ), bEnable && ( tNow - g_tLastIncorrectLogin >= BAD_QUERY_IGNORE_TIME ));
	EnableWindow( GetDlgItem( g_hDlg, IDC_SERVERIP ), bEnable );
	EnableWindow( GetDlgItem( g_hDlg, IDC_PASSWORD ), bEnable );
}

//==========================================================================
//
// main_UpdateTrayTooltip
//
// Sets the tooltip of the tray icon.
//
//==========================================================================

void main_UpdateTrayTooltip( const char *pszTooltip )
{
	strncpy( g_szTooltip, pszTooltip, 128 );
	lstrcpy( g_NotifyIconData.szTip, g_szTooltip );
	Shell_NotifyIcon( NIM_MODIFY, &g_NotifyIconData );
}

//==========================================================================
//
// main_ToggleWindow
//
// Shows or hides the main window.
//
//==========================================================================

static void main_ToggleWindow( HWND hDlg )
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

//==========================================================================
//
// main_SetState
//
// Updates the connection state.
//
//==========================================================================

static void main_SetState( STATE_e NewState )
{
	g_State = NewState;

	if ( g_State == STATE_WAITING )
		main_UpdateTrayTooltip( "Disconnected" );
}

//==========================================================================
//
// main_ParseCommands
//
// Acts on messages we get back from our server.
//
//==========================================================================

static void main_ParseCommands( BYTESTREAM_s *pByteStream )
{	
	switch ( NETWORK_ReadByte( pByteStream ))
	{
	case SVRC_BANNED:
				
		main_ShowMessage( "You have been banned from the server!", MB_ICONEXCLAMATION );
		main_SetState( STATE_WAITING );
		main_EnableConnectionButtons( TRUE );		
		return;
	case SVRC_OLDPROTOCOL:
		{
			char	szBuffer[256];

			// Ignore the protocol version.
			NETWORK_ReadByte( pByteStream );
			sprintf( szBuffer, "The server is using a newer version than you are (%s).\nThis version is compatible with %s.", NETWORK_ReadString( pByteStream ), COMPATIBLE_WITH );
			main_ShowMessage( szBuffer, MB_ICONEXCLAMATION );
			main_SetState( STATE_WAITING );
			main_EnableConnectionButtons( TRUE );
			return;
		}
		return;
	case SVRC_INVALIDPASSWORD:

		time( &g_tLastIncorrectLogin );
		main_ShowMessage( "Sorry, the password you gave is incorrect.", MB_ICONEXCLAMATION );
		main_SetState( STATE_WAITING );
		main_EnableConnectionButtons( TRUE );
		return;
	case SVRC_SALT:

		main_SendPassword( NETWORK_ReadString( pByteStream ));		
		return;
	case SVRC_LOGGEDIN:
	
		// Read in info about the server.
		g_iServerProtocolVersion = NETWORK_ReadByte( pByteStream );
		strncpy( g_szHostname, NETWORK_ReadString( pByteStream ), 256 );
		
		for ( int i = 0, num = NETWORK_ReadByte( pByteStream ); i < num; i++ )
			main_ParseUpdate( pByteStream );

		// Read the console history.
		g_iLines = NETWORK_ReadByte( pByteStream );
		for ( int i = 0; i < g_iLines; i++ )
			g_RecentConsoleHistory.push_back( NETWORK_ReadString( pByteStream ));

		// Show the main dialog.
		CreateThread( NULL, 0, main_Show, 0, 0, 0 );		
		break;
	case SVRC_MESSAGE:
		
		// [BB] The string may contain format parameters like %n, so we may not use Printf!
		if ( g_State == STATE_CONNECTED )
			MAIN_Print( true, NETWORK_ReadString( pByteStream ));
		break;
	case SVRC_UPDATE:

		main_ParseUpdate( pByteStream );
		break;
	}
}

//==========================================================================
//
// main_ParseUpdate
//
// Reads an SVRC_UPDATE message.
//
//==========================================================================

static void main_ParseUpdate( BYTESTREAM_s *pByteStream )
{
	switch ( NETWORK_ReadByte( pByteStream ))
	{
	case SVRCU_PLAYERDATA:

		g_iNumPlayers = NETWORK_ReadByte( pByteStream );

		// Add the players to the list.
		SendDlgItemMessage( g_hDlg, IDC_PLAYERLIST, LVM_DELETEALLITEMS, 0, 0 ) ;
		LVITEM		Item;
		Item.mask = LVIF_TEXT;
		Item.iSubItem = COLUMN_NAME;
		Item.iItem = MAXPLAYERS;
		for ( LONG lIdx = 0; lIdx < g_iNumPlayers; lIdx++ )
		{
			// A yummy hack. At the beginning, the player list doesn't exist yet, so we need to cache the list of names until it does.
			if ( g_bRCONDialogVisible )
			{
				Item.pszText = (LPSTR) NETWORK_ReadString( pByteStream );
				SendDlgItemMessage( g_hDlg, IDC_PLAYERLIST, LVM_INSERTITEM, 0, (LPARAM)&Item ) ;
			}
			else
				g_InitialPlayers.push_front( NETWORK_ReadString( pByteStream ));
		}		
		main_UpdateServerStatus( );
		break;
	case SVRCU_MAP:

		strncpy( g_szMapname, NETWORK_ReadString( pByteStream ), 32 );
		main_UpdateServerStatus( );
		break;
	case SVRCU_ADMINCOUNT:

		g_iNumOtherAdmins = NETWORK_ReadByte( pByteStream );
		main_UpdateServerStatus( );
		break;
	}
}

//==========================================================================
//
// main_AttemptConnection
//
// Sends the server our first request.
//
//==========================================================================

static void main_AttemptConnection( )
{
	char	szBuffer[128];

	// Update the GUI.
	time( &g_tLastSentCommand );
	main_SetState( STATE_CONNECTING );
	sprintf( szBuffer, "Connecting to %s...", g_ServerAddress.ToString() );
	main_UpdateStatusbar( szBuffer );
	main_UpdateTrayTooltip( szBuffer );
	GetDlgItemText( g_hDlg, IDC_SERVERIP, szBuffer, 128 );

	// Save this server to our config file.
	g_Config.SetSection( "Settings", true );
	g_Config.SetValueForKey( "LastServer", szBuffer );
	g_Config.WriteConfigFile( );

	// Start listening for packets.
	if ( g_hThread == NULL )
	{
		g_hThread = CreateThread( NULL, 0, main_Loop, 0, 0, 0 );
		NETWORK_InitBuffer( &g_MessageBuffer, 8192, BUFFERTYPE_WRITE );
	}

	NETWORK_ClearBuffer( &g_MessageBuffer );
	NETWORK_WriteByte( &g_MessageBuffer.ByteStream, CLRC_BEGINCONNECTION );
	NETWORK_WriteByte( &g_MessageBuffer.ByteStream, PROTOCOL_VERSION );
	NETWORK_LaunchPacket( &g_MessageBuffer, g_ServerAddress );	
}

//==========================================================================
//
// main_SendPassword
//
// Sends the server our password, salted as requested.
//
//==========================================================================

static void main_SendPassword( const char *pszSalt )
{
	char	szString[512];

	main_UpdateStatusbar( "Authenticating..." );
	FString fsString, fsHash;
	fsString.Format( "%s%s", pszSalt, g_szPassword );
	CMD5Checksum::GetMD5( reinterpret_cast<const BYTE *>(fsString.GetChars()), fsString.Len(), fsHash );

	NETWORK_ClearBuffer( &g_MessageBuffer );
	NETWORK_WriteByte( &g_MessageBuffer.ByteStream, CLRC_PASSWORD );
	NETWORK_WriteString( &g_MessageBuffer.ByteStream, fsHash.GetChars() );
	NETWORK_LaunchPacket( &g_MessageBuffer, g_ServerAddress );
	time( &g_tLastSentCommand );
}

//==========================================================================
//
// main_Disconnect
//
// Tells the server we're leaving.
//
//==========================================================================

static void main_Disconnect( )
{
	NETWORK_ClearBuffer( &g_MessageBuffer );
	NETWORK_WriteByte( &g_MessageBuffer.ByteStream, CLRC_DISCONNECT );
	NETWORK_LaunchPacket( &g_MessageBuffer, g_ServerAddress );
	main_SetState( STATE_WAITING );
}

//*****************************************************************************
//
int Printf( const char *pszString, ... )
{
	va_list		ArgPtr;
	va_start( ArgPtr, pszString );
	VPrintf( true, pszString, ArgPtr );
	va_end( ArgPtr );
	return 0;
}

//*****************************************************************************
//
int Printf_NoTimestamp( const char *pszString, ... )
{
	va_list		ArgPtr;
	va_start( ArgPtr, pszString );
	VPrintf( false, pszString, ArgPtr );
	va_end( ArgPtr );
	return 0;
}

//*****************************************************************************
//
void VPrintf( bool bTimestamp, const char *pszString, va_list Parms )
{
	char	szOutLine[8192];
	vsprintf_s( szOutLine, pszString, Parms );
	MAIN_Print( bTimestamp, szOutLine );
}

//*****************************************************************************
//
void MAIN_Print( bool bTimestamp, const char *pszString )
{
	main_UpdateStatusbar( pszString );
	
	if ( g_State == STATE_CONNECTED )
	{
		char	szBuffer[RCONCONSOLE_TEXTLENGTH];
		char	szInputString[RCONCONSOLE_TEXTLENGTH];
		char	*psz;
		char	c;
		bool	bScroll = false;
		bool	bRecycled = false;

		// Prefix the line with a timestamp (unless it's a newline).
		if ( strlen( pszString ) && bTimestamp && pszString[0] != '\n' )
		{
			time_t			tNow = time(0);
			struct	tm		*pTimeInfo = localtime( &tNow );

			if ( pTimeInfo->tm_hour < 12 )
				sprintf( szInputString, "[%02d:%02d:%02d am] ", ( pTimeInfo->tm_hour == 0 ) ? 12 : pTimeInfo->tm_hour, pTimeInfo->tm_min, pTimeInfo->tm_sec );
			else
				sprintf( szInputString, "[%02d:%02d:%02d pm] ", ( pTimeInfo->tm_hour == 12 ) ? 12 : pTimeInfo->tm_hour % 12, pTimeInfo->tm_min, pTimeInfo->tm_sec );

			psz = szInputString + strlen( szInputString );
		}
		else
			psz = szInputString;

		// Check where the scrollbars are.
		LONG	lVisibleLine;
		LONG	lTotalLines;
		LONG	lLineDiff;

		while ( 1 )
		{
			c = *pszString++;
			if ( c == '\0' )
			{
				*psz = c;
				break;
			}

			if ( c == '\n' )
			{
				*psz++ = '\r';
			}

			*psz++ = c;
		}

		lVisibleLine = SendDlgItemMessage( g_hDlg, IDC_CONSOLEBOX, EM_GETFIRSTVISIBLELINE, 0, 0 );
		lTotalLines = SendDlgItemMessage( g_hDlg, IDC_CONSOLEBOX, EM_GETLINECOUNT, 0, 0 );
		lLineDiff = lTotalLines - lVisibleLine;
		bScroll = ( lLineDiff <= 14 );

		if ( GetDlgItemText( g_hDlg, IDC_CONSOLEBOX, szBuffer, sizeof( szBuffer )))
		{
			LONG	lDifference;
			char	szConsoleBuffer[RCONCONSOLE_TEXTLENGTH];

			// If the amount of text added to the buffer will cause a buffer overflow, shuffle the text upwards.
			psz = szBuffer;
			if (( lDifference = ( (LONG)strlen( szBuffer ) + (LONG)strlen( szInputString ) - RCONCONSOLE_TEXTLENGTH )) >= 0 )
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

			sprintf( szConsoleBuffer, "%s%s", psz, szInputString );
			SetDlgItemText( g_hDlg, IDC_CONSOLEBOX, szConsoleBuffer );

			// If the user has scrolled all the way down, autoscroll.
			if ( bScroll )
				SendDlgItemMessage( g_hDlg, IDC_CONSOLEBOX, EM_LINESCROLL, 0, lTotalLines );

			// If they've scrolled up but we've trimmed the text, don't autoscroll and compensate. 
			else if( bRecycled && ( lVisibleLine > 0 ) )
				SendDlgItemMessage( g_hDlg, IDC_CONSOLEBOX, EM_LINESCROLL, 0, lVisibleLine - 1 );

			// If they've scrolled up, don't autoscroll.
			else
				SendDlgItemMessage( g_hDlg, IDC_CONSOLEBOX, EM_LINESCROLL, 0, lVisibleLine );
		}
	}
}

//==========================================================================
//
// main_UpdateServerStatus
//
// Updates the "MAP01 | 5 players | 1 other admin" display.
//
//==========================================================================

static void main_UpdateServerStatus( )
{
	char	szBuffer[128];

	sprintf( szBuffer, "%s | %d player%s | %d other admin%s", g_szMapname, g_iNumPlayers, ( g_iNumPlayers == 1 ? "" : "s" ), g_iNumOtherAdmins, ( g_iNumOtherAdmins == 1 ? "" : "s" ));
	SetDlgItemText( g_hDlg, IDC_SERVERSUBINFO, szBuffer );
}

//==========================================================================
//
// main_RCONDialogCallback
//
// Callback for the main dialog box.
//
//==========================================================================

BOOL CALLBACK main_RCONDialogCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	char	szBuffer[128];

	switch ( Message )
	{
	case WM_CTLCOLORSTATIC:

		switch ( GetDlgCtrlID( (HWND) lParam ))
		{
		// Paint these two labels (and the disconnct button's background) white.
		case IDCANCEL:
		case IDC_STATUS:
		case IDC_SERVERSUBINFO:

			return (LRESULT) g_hWhiteBrush;
		// Ignore everything else.
		default:

			return NULL;
		}
		break;
	case WM_PAINT:
		{
			// Paint the top of the form white.
			PAINTSTRUCT Ps;
			RECT r;
			r.left = 0;
			r.top = 0;
			r.bottom = 48;
			r.right = 800;
			main_PaintRectangle( BeginPaint(hDlg, &Ps), &r, RGB(255, 255, 255));
		}
		break;
	case WM_INITDIALOG:
		
		// Hide the old dialog, and take its place.
		ShowWindow( g_hDlg, SW_HIDE );
		g_hDlg = hDlg;		

		SendDlgItemMessage( hDlg, IDC_CONSOLEBOX, EM_SETLIMITTEXT, 4096, 0 );
		SendDlgItemMessage( hDlg, IDC_INPUTBOX, EM_SETLIMITTEXT, 256, 0 );
		SetWindowText( hDlg, g_szHostname );
		main_SetState( STATE_CONNECTED );
		Printf( "\nMap: %s\n", g_szMapname );

		// Fill the console with the received history.
		sprintf( szBuffer, "Connected to \"%s\".", g_szHostname );
		SetDlgItemText( hDlg, IDC_CONSOLEBOX, szBuffer );
		SetDlgItemText( hDlg, IDC_STATUS, szBuffer );
		main_UpdateTrayTooltip( szBuffer );
		Printf_NoTimestamp( "\n" );
		for( std::list<FString>::iterator i = g_RecentConsoleHistory.begin(); i != g_RecentConsoleHistory.end(); ++i )
			Printf_NoTimestamp( "%s", *i );
		g_RecentConsoleHistory.clear();

		// Set up the top, white section.
		SendMessage( GetDlgItem( g_hDlg, IDC_STATUS ), WM_SETFONT, (WPARAM) CreateFont( 13, 0, 0, 0, 600, 0, 0, 0, 0, 0, 0, 0, 0, "Tahoma" ), (LPARAM) 1 );
		LOGBRUSH LogBrush;
		LogBrush.lbStyle = BS_SOLID;
		LogBrush.lbColor = RGB( 255, 255, 255 );
		g_hWhiteBrush = CreateBrushIndirect( &LogBrush );
		main_UpdateServerStatus( );

		// Set up the player list
		LVCOLUMN	ColumnData;
		ColumnData.mask = LVCF_FMT|LVCF_TEXT|LVCF_WIDTH;
		ColumnData.fmt = LVCFMT_LEFT;
		ColumnData.cx = 192;
		ColumnData.pszText = "Name";
		ColumnData.cchTextMax = 64;
		ColumnData.iSubItem = 0;
		SendDlgItemMessage( hDlg, IDC_PLAYERLIST, LVM_INSERTCOLUMN, COLUMN_NAME, (LPARAM)&ColumnData );

		// Add the cached list of players.
		LVITEM		Item;
		Item.mask = LVIF_TEXT;
		Item.iSubItem = COLUMN_NAME;
		Item.iItem = MAXPLAYERS;
		while ( g_InitialPlayers.size( ) )
		{
			Item.pszText = (LPSTR) g_InitialPlayers.front( ).GetChars( );
			g_InitialPlayers.pop_front( );
			SendDlgItemMessage( g_hDlg, IDC_PLAYERLIST, LVM_INSERTITEM, 0, (LPARAM)&Item ) ;
		}

		// Load the icon.
		SendMessage( hDlg, WM_SETICON, (WPARAM)ICON_SMALL, (LPARAM) (HICON) LoadImage( g_hInst,	MAKEINTRESOURCE( AAA_MAIN_ICON ), IMAGE_ICON, 16, 16, LR_SHARED ));
		SendMessage( hDlg, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)LoadIcon( g_hInst, MAKEINTRESOURCE( AAA_MAIN_ICON )));

		// Set up the status bar.
		g_hDlgStatusBar = CreateStatusWindow(WS_CHILD | WS_VISIBLE, (LPCTSTR)NULL, hDlg, IDC_STATIC);

		g_bRCONDialogVisible = true;
		break;
	case WM_COMMAND:

			switch ( LOWORD( wParam ))
			{

			// This also occurs when esc is pressed.
			case IDCANCEL:

				main_Quit( );
				break;

			// "Send" button.
			case IDC_SEND:
			
				char	szCommand[256];

				GetDlgItemText( hDlg, IDC_INPUTBOX, szCommand, sizeof( szCommand ));
				NETWORK_ClearBuffer( &g_MessageBuffer );
				NETWORK_WriteByte( &g_MessageBuffer.ByteStream, CLRC_COMMAND );

				if ( szCommand[0] == ':' ) // If the text in the send buffer begins with a :, the admin is just talking.
				{
					char	szBuffer2[256 + 4];

					sprintf( szBuffer2, "say %s", szCommand + 1 );
					NETWORK_WriteString( &g_MessageBuffer.ByteStream, szBuffer2 );
					break;
				}
				else if ( szCommand[0] == '/' ) // If the text in the send buffer begins with a slash, error out -- Skulltag used to require you to do this to send commands.
				{
					Printf( "You longer have to prefix commands with a / to send them.\n" );
					SetDlgItemText( hDlg, IDC_INPUTBOX, szCommand + 1 );
					SendMessage( GetDlgItem( hDlg, IDC_INPUTBOX ), EM_SETSEL, strlen( szCommand ) - 1, strlen( szCommand ) - 1 );
					break;
				}
				else
					NETWORK_WriteString( &g_MessageBuffer.ByteStream, szCommand );				
				
				NETWORK_LaunchPacket( &g_MessageBuffer, g_ServerAddress );
				time( &g_tLastSentCommand );
				SetDlgItemText( hDlg, IDC_INPUTBOX, "" );
				break;
			}
			break;
	case WM_SYSCOMMAND:

		// Hide the window when minimized.
		if ( wParam == SC_MINIMIZE )
			ShowWindow( hDlg, SW_HIDE );
		else
			DefWindowProc( hDlg, Message, wParam, lParam );
		break;
	case WM_CLOSE:

		main_Quit( );
		break;
	case WM_DESTROY:

		Shell_NotifyIcon( NIM_DELETE, &g_NotifyIconData );
		PostQuitMessage( 0 );
		break;
	case UWM_TRAY_TRAYID:

		return main_TrayIconClicked( hDlg, lParam );	
	default:

		return FALSE;
	}

	return TRUE; // If this is false, minimizing the window won't hide it.
}

//==========================================================================
//
// main_AboutDialogCallback
//
// Callback for the "About" dialog box.
//
//==========================================================================

BOOL CALLBACK main_AboutDialogCallback( HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam )
{
	switch ( Message )
	{
	case WM_CTLCOLORSTATIC:

		switch ( GetDlgCtrlID( (HWND) lParam ))
		{
		// Paint these things white.
		case IDC_DESCTEXT:
		case IDC_REVISION:
		case IDC_INTROTEXT:

			return (LRESULT) g_hWhiteBrush;
		// Ignore everything else.
		default:

			return NULL;
		}
		break;
	case WM_PAINT:
		{
			// Paint the top of the form white.
			PAINTSTRUCT Ps;
			RECT r;
			r.left = 0;
			r.top = 0;
			r.bottom = 109;
			r.right = 800;
			main_PaintRectangle( BeginPaint(hDlg, &Ps), &r, RGB(255, 255, 255));
		}
		break;
	case WM_INITDIALOG:

		// Fonts.
		SendMessage( GetDlgItem( hDlg, IDC_INTROTEXT ), WM_SETFONT, (WPARAM) CreateFont( 17, 0, 0, 0, 600, 0, 0, 0, 0, 0, 0, 0, 0, "Tahoma" ), (LPARAM) 1 );
		SendMessage( GetDlgItem( hDlg, IDC_DESCTEXT ), WM_SETFONT, (WPARAM) CreateFont( 13, 0, 0, 0, 0, TRUE, 0, 0, 0, 0, 0, 0, 0, "Tahoma" ), (LPARAM) 1 );

		SetDlgItemText( hDlg, IDC_REVISION, "Revision "HG_TIME" - compatible with "COMPATIBLE_WITH"." );

		break;
	case WM_COMMAND:

			switch ( LOWORD( wParam ))
			{
			
			case IDOK:
			case IDCANCEL: // This also occurs when esc is pressed.
				
				EndDialog( hDlg, -1 );
				break;
			}
		break;
	default:

		return FALSE;
	}

	return TRUE;
}
