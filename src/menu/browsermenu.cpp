//-----------------------------------------------------------------------------
//
// Zandronum Source
// Copyright (C) 2016 Benjamin Berkels
// Copyright (C) 2016 Zandronum Development Team
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
// 3. Neither the name of the Zandronum Development Team nor the names of its
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
//
//
// Filename: menu/browsermenu.cpp
//
//-----------------------------------------------------------------------------

#include <float.h>

#include "menu/menu.h"
#include "c_dispatch.h"
#include "w_wad.h"
#include "sc_man.h"
#include "v_font.h"
#include "g_level.h"
#include "d_player.h"
#include "v_video.h"
#include "gi.h"
#include "i_system.h"
#include "c_bind.h"
#include "v_palette.h"
#include "d_event.h"
#include "d_gui.h"

#define NO_IMP
#include "menu/optionmenuitems.h"

#include "browser.h"

static	LONG	g_lSelectedServer = -1;
static	int		g_iSortedServers[MAX_BROWSER_SERVERS];
static	int		g_sortedServerListOffest = 0;

void M_RefreshServers( void );
void M_BuildServerList( void );
LONG M_CalcLastSortedIndex( void );
bool M_ShouldShowServer( LONG lServer );

static	void			browsermenu_SortServers( ULONG ulSortType );
static	int	STACK_ARGS	browsermenu_PingCompareFunc( const void *arg1, const void *arg2 );
static	int	STACK_ARGS	browsermenu_ServerNameCompareFunc( const void *arg1, const void *arg2 );
static	int	STACK_ARGS	browsermenu_MapNameCompareFunc( const void *arg1, const void *arg2 );
static	int	STACK_ARGS	browsermenu_PlayersCompareFunc( const void *arg1, const void *arg2 );

#define	NUM_SERVER_SLOTS	8

CUSTOM_CVAR( Int, menu_browser_servers, 0, CVAR_ARCHIVE )
{
	M_BuildServerList();
}
CUSTOM_CVAR( Int, menu_browser_gametype, 0, CVAR_ARCHIVE )
{
	M_BuildServerList();
}
CUSTOM_CVAR( Int, menu_browser_sortby, 0, CVAR_ARCHIVE )
{
	M_BuildServerList();
}
CUSTOM_CVAR( Bool, menu_browser_showempty, true, CVAR_ARCHIVE )
{
	M_BuildServerList();
}
CUSTOM_CVAR( Bool, menu_browser_showfull, true, CVAR_ARCHIVE )
{
	M_BuildServerList();
}

// =================================================================================================
//
// [BB] FOptionMenuServerBrowserLine
//
// =================================================================================================

bool FOptionMenuServerBrowserLine::Activate()
{
	S_Sound (CHAN_VOICE | CHAN_UI, "menu/choose", snd_menuvolume, ATTN_NONE);
	g_lSelectedServer = g_iSortedServers[ mSlotNum + g_sortedServerListOffest ];
	return true;
}

int FOptionMenuServerBrowserLine::Draw(FOptionMenuDescriptor *desc, int y, int indent, bool selected)
{
	const int serverNum = g_iSortedServers[ mSlotNum + g_sortedServerListOffest ];
	if ( M_ShouldShowServer ( serverNum ) == false )
		return 0;

	char szString[256];
	int color = ( serverNum == g_lSelectedServer ) ? CR_ORANGE : CR_GRAY;
	int localIndent = indent - 80 * CleanXfac_1;

	// Draw ping.
	sprintf( szString, "%d", static_cast<int> (BROWSER_GetPing( serverNum )));
	screen->DrawText( SmallFont, color, 16 * CleanXfac_1 + localIndent, y, szString, DTA_CleanNoMove_1, true, TAG_DONE );

	// Draw name.
	strncpy( szString, BROWSER_GetHostName( serverNum ), 12 );
	szString[12] = 0;
	if ( strlen( BROWSER_GetHostName( serverNum )) > 12 )
		sprintf( szString + strlen ( szString ), "..." );
	screen->DrawText( SmallFont, color, 48 * CleanXfac_1 + localIndent, y, szString, DTA_CleanNoMove_1, true, TAG_DONE );

	// Draw map.
	strncpy( szString, BROWSER_GetMapname( serverNum ), 8 );
	screen->DrawText( SmallFont, color, /*128*/160 * CleanXfac_1 + localIndent, y, szString, DTA_CleanNoMove_1, true, TAG_DONE );
	/*
	// Draw wad.
	if ( BROWSER_Get
	sprintf( szString, "%d", BROWSER_GetPing( lServer ));
	screen->DrawText( SmallFont, CR_GRAY, 160 * CleanXfac_1 + localIndent, y, "WAD", DTA_CleanNoMove_1, true, TAG_DONE );
	*/
	// Draw gametype.
	strncpy( szString, GAMEMODE_GetShortName( BROWSER_GetGameMode( serverNum )), 8 );
	screen->DrawText( SmallFont, color, 224 * CleanXfac_1 + localIndent, y, szString, DTA_CleanNoMove_1, true, TAG_DONE );

	// Draw players.
	sprintf( szString, "%d/%d", static_cast<int> (BROWSER_GetNumPlayers( serverNum )), static_cast<int> (BROWSER_GetMaxClients( serverNum )));
	screen->DrawText( SmallFont, color, 272 * CleanXfac_1 + localIndent, y, szString, DTA_CleanNoMove_1, true, TAG_DONE );
	return localIndent;
}

bool FOptionMenuServerBrowserLine::Selectable()
{
	return ( M_ShouldShowServer ( g_iSortedServers[ mSlotNum + g_sortedServerListOffest ] ) );
}

bool FOptionMenuServerBrowserLine::MenuEvent (int mkey, bool fromcontroller)
{
	if ( mkey == MKEY_Left )
	{
		if ( g_sortedServerListOffest >= NUM_SERVER_SLOTS )
		{
			g_sortedServerListOffest -= NUM_SERVER_SLOTS;
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", snd_menuvolume, ATTN_NONE);
			return true;
		}
		else
		{
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/invalid", snd_menuvolume, ATTN_NONE);
			return false;
		}
	}
	else if ( mkey == MKEY_Right )
	{
		if ( g_sortedServerListOffest < M_CalcLastSortedIndex( ) - NUM_SERVER_SLOTS )
		{
			g_sortedServerListOffest += NUM_SERVER_SLOTS;
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", snd_menuvolume, ATTN_NONE);
			return true;
		}
		else
		{
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/invalid", snd_menuvolume, ATTN_NONE);
			return false;
		}
	}
	else
	{
		return FOptionMenuItem::MenuEvent(mkey, fromcontroller);
	}
}

// =================================================================================================
//
// [BB] DServerInfoMenu
//
// =================================================================================================

class DServerInfoMenu : public DOptionMenu
{
	DECLARE_CLASS( DServerInfoMenu, DOptionMenu )

public:
	DServerInfoMenu(){}

	void Drawer()
	{
		Super::Drawer();

		ULONG	ulIdx;
		ULONG	ulCurYPos;
		ULONG	ulTextHeight;
		char	szString[256];

		if ( g_lSelectedServer == -1 )
			return;

		ulCurYPos = 32;
		ulTextHeight = ( gameinfo.gametype == GAME_Doom ? 8 : 9 );

		sprintf( szString, "Name: \\cc%s", BROWSER_GetHostName( g_lSelectedServer ));
		V_ColorizeString( szString );
		screen->DrawText( SmallFont, CR_UNTRANSLATED, 16, ulCurYPos, szString, DTA_Clean, true, TAG_DONE );

		ulCurYPos += ulTextHeight;

		sprintf( szString, "IP: \\cc%s", BROWSER_GetAddress( g_lSelectedServer ).ToString() );
		V_ColorizeString( szString );
		screen->DrawText( SmallFont, CR_UNTRANSLATED, 16, ulCurYPos, szString, DTA_Clean, true, TAG_DONE );

		ulCurYPos += ulTextHeight;

		sprintf( szString, "Map: \\cc%s", BROWSER_GetMapname( g_lSelectedServer ));
		V_ColorizeString( szString );
		screen->DrawText( SmallFont, CR_UNTRANSLATED, 16, ulCurYPos, szString, DTA_Clean, true, TAG_DONE );

		ulCurYPos += ulTextHeight;

		sprintf( szString, "Gametype: \\cc%s", GAMEMODE_GetName ( BROWSER_GetGameMode( g_lSelectedServer ) ) );
		V_ColorizeString( szString );
		screen->DrawText( SmallFont, CR_UNTRANSLATED, 16, ulCurYPos, szString, DTA_Clean, true, TAG_DONE );

		ulCurYPos += ulTextHeight;

		sprintf( szString, "IWAD: \\cc%s", BROWSER_GetIWADName( g_lSelectedServer ));
		V_ColorizeString( szString );
		screen->DrawText( SmallFont, CR_UNTRANSLATED, 16, ulCurYPos, szString, DTA_Clean, true, TAG_DONE );

		ulCurYPos += ulTextHeight;

		sprintf( szString, "PWADs: \\cc%d", static_cast<int> (BROWSER_GetNumPWADs( g_lSelectedServer )));
		V_ColorizeString( szString );
		screen->DrawText( SmallFont, CR_UNTRANSLATED, 16, ulCurYPos, szString, DTA_Clean, true, TAG_DONE );

		ulCurYPos += ulTextHeight;

		for ( ulIdx = 0; ulIdx < static_cast<unsigned> (MIN( (int)BROWSER_GetNumPWADs( g_lSelectedServer ), 4 )); ulIdx++ )
		{
			sprintf( szString, "\\cc%s", BROWSER_GetPWADName( g_lSelectedServer, ulIdx ));
			V_ColorizeString( szString );
			screen->DrawText( SmallFont, CR_UNTRANSLATED, 32, ulCurYPos, szString, DTA_Clean, true, TAG_DONE );

			ulCurYPos += ulTextHeight;
		}

		sprintf( szString, "WAD URL: \\cc%s", BROWSER_GetWadURL( g_lSelectedServer ));
		V_ColorizeString( szString );
		screen->DrawText( SmallFont, CR_UNTRANSLATED, 16, ulCurYPos, szString, DTA_Clean, true, TAG_DONE );

		ulCurYPos += ulTextHeight;

		sprintf( szString, "Host e-mail: \\cc%s", BROWSER_GetEmailAddress( g_lSelectedServer ));
		V_ColorizeString( szString );
		screen->DrawText( SmallFont, CR_UNTRANSLATED, 16, ulCurYPos, szString, DTA_Clean, true, TAG_DONE );

		ulCurYPos += ulTextHeight;

		sprintf( szString, "Players: \\cc%d/%d", static_cast<int> (BROWSER_GetNumPlayers( g_lSelectedServer )), static_cast<int> (BROWSER_GetMaxClients( g_lSelectedServer )));
		V_ColorizeString( szString );
		screen->DrawText( SmallFont, CR_UNTRANSLATED, 16, ulCurYPos, szString, DTA_Clean, true, TAG_DONE );

		ulCurYPos += ulTextHeight;

		if ( BROWSER_GetNumPlayers( g_lSelectedServer ))
		{
			ulCurYPos += ulTextHeight;

			screen->DrawText( SmallFont, CR_UNTRANSLATED, 32, ulCurYPos, "NAME", DTA_Clean, true, TAG_DONE );
			screen->DrawText( SmallFont, CR_UNTRANSLATED, 192, ulCurYPos, "FRAGS", DTA_Clean, true, TAG_DONE );
			screen->DrawText( SmallFont, CR_UNTRANSLATED, 256, ulCurYPos, "PING", DTA_Clean, true, TAG_DONE );

			ulCurYPos += ( ulTextHeight * 2 );

			for ( ulIdx = 0; static_cast<signed> (ulIdx) < MIN( (int)BROWSER_GetNumPlayers( g_lSelectedServer ), 4 ); ulIdx++ )
			{
				sprintf( szString, "%s", BROWSER_GetPlayerName( g_lSelectedServer, ulIdx ));
				V_ColorizeString( szString );
				screen->DrawText( SmallFont, CR_GRAY, 32, ulCurYPos, szString, DTA_Clean, true, TAG_DONE );

				sprintf( szString, "%d", static_cast<int> (BROWSER_GetPlayerFragcount( g_lSelectedServer, ulIdx )));
				V_ColorizeString( szString );
				screen->DrawText( SmallFont, CR_GRAY, 192, ulCurYPos, szString, DTA_Clean, true, TAG_DONE );

				sprintf( szString, "%d", static_cast<int> (BROWSER_GetPlayerPing( g_lSelectedServer, ulIdx )));
				V_ColorizeString( szString );
				screen->DrawText( SmallFont, CR_GRAY, 256, ulCurYPos, szString, DTA_Clean, true, TAG_DONE );

				ulCurYPos += ulTextHeight;
			}
		}
	}
};

IMPLEMENT_CLASS( DServerInfoMenu )

// =================================================================================================
//
// [BB] DBrowserMenu
//
// Internal server browser menu
//
// =================================================================================================

class DBrowserMenu : public DOptionMenu
{
	DECLARE_CLASS( DBrowserMenu, DOptionMenu )

	int refreshTicker;
public:
	DBrowserMenu() : refreshTicker ( 0 ) {}

	void Init ( DMenu* parent = NULL, FOptionMenuDescriptor* desc = NULL )
	{
		M_RefreshServers();
		M_BuildServerList();
		Super::Init( parent, desc );
	}

	void Ticker ()
	{
		Super::Ticker();

		// [BB] Query the servers that didn't respond yet a couple of times.

		if ( refreshTicker >= 10 * TICRATE )
			return;

		if ( ( ++refreshTicker % TICRATE ) == 0 )
			BROWSER_QueryAllServers();
	}

	void Drawer()
	{
		Super::Drawer();

		int y = 25;
		screen->DrawText( SmallFont, CR_UNTRANSLATED, 16, y, "PING", DTA_Clean, true, TAG_DONE );
		screen->DrawText( SmallFont, CR_UNTRANSLATED, 48, y, "NAME", DTA_Clean, true, TAG_DONE );
		screen->DrawText( SmallFont, CR_UNTRANSLATED, /*128*/160, y, "MAP", DTA_Clean, true, TAG_DONE );
		//screen->DrawText( SmallFont, CR_UNTRANSLATED, 160, y, "WAD", DTA_Clean, true, TAG_DONE );
		screen->DrawText( SmallFont, CR_UNTRANSLATED, 224, y, "TYPE", DTA_Clean, true, TAG_DONE );
		screen->DrawText( SmallFont, CR_UNTRANSLATED, 272, y, "PLYRS", DTA_Clean, true, TAG_DONE );

		FString str;
		const int numServers = static_cast<int> ( M_CalcLastSortedIndex( ) );
		if ( numServers > NUM_SERVER_SLOTS )
			str.Format( "Currently showing servers %d to %d out of %d", g_sortedServerListOffest + 1, MIN ( g_sortedServerListOffest + NUM_SERVER_SLOTS, numServers ), numServers );
		else
			str.Format( "Currently showing %d servers", numServers );
		screen->DrawText( SmallFont, CR_WHITE, 160 - ( SmallFont->StringWidth( str ) / 2 ), 190, str, DTA_Clean, true, TAG_DONE );
	}
};

IMPLEMENT_CLASS( DBrowserMenu )

//*****************************************************************************
//
void M_RefreshServers( void )
{
	// Don't do anything if we're still waiting for a response from the master server.
	if ( BROWSER_WaitingForMasterResponse( ))
		return;

	g_lSelectedServer = -1;
	g_sortedServerListOffest = 0;

	// First, clear the existing server list.
	BROWSER_ClearServerList( );

	// Then, query the master server.
	BROWSER_QueryMasterServer( );
}

//*****************************************************************************
//
LONG M_CalcLastSortedIndex( void )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAX_BROWSER_SERVERS; ulIdx++ )
	{
		if ( M_ShouldShowServer( g_iSortedServers[ulIdx] ) == false )
			return ( ulIdx );
	}

	return ( ulIdx );
}

//*****************************************************************************
//
void M_BuildServerList( void )
{
	browsermenu_SortServers( menu_browser_sortby );
	g_sortedServerListOffest = 0;
}

//*****************************************************************************
//
bool M_ShouldShowServer( LONG lServer )
{
	// Don't show inactive servers.
	if ( BROWSER_IsActive( lServer ) == false )
		return ( false );
/*
	// Don't show servers that don't have the same IWAD we do.
	if ( stricmp( SERVER_MASTER_GetIWADName( ), BROWSER_GetIWADName( lServer )) != 0 )
		return ( false );
*/
	// Don't show Internet servers if we are only showing LAN servers.
	if ( menu_browser_servers == 1 )
	{
		if ( BROWSER_IsLAN( lServer ) == false )
			return ( false );
	}

	// Don't show LAN servers if we are only showing Internet servers.
	if ( menu_browser_servers == 0 )
	{
		if ( BROWSER_IsLAN( lServer ) == true )
			return ( false );
	}

	// Don't show empty servers.
	if ( menu_browser_showempty == false )
	{
		if ( BROWSER_GetNumPlayers( lServer ) == 0 )
			return ( false );
	}

	// Don't show full servers.
	if ( menu_browser_showfull == false )
	{
		if ( BROWSER_GetNumPlayers( lServer ) ==  BROWSER_GetMaxClients( lServer ))
			return ( false );
	}

	// Don't show servers that have the gameplay mode we want.
	if ( menu_browser_gametype != 0 )
	{
		if ( BROWSER_GetGameMode( lServer ) != ( menu_browser_gametype - 1 ))
			return ( false );
	}

	return ( true );
}

//*****************************************************************************
//
static void browsermenu_SortServers( ULONG ulSortType )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAX_BROWSER_SERVERS; ulIdx++ )
		g_iSortedServers[ulIdx] = ulIdx;

	switch ( ulSortType )
	{
	// Ping.
	case 0:

		qsort( g_iSortedServers, MAX_BROWSER_SERVERS, sizeof( int ), browsermenu_PingCompareFunc );
		break;
	// Server name.
	case 1:

		qsort( g_iSortedServers, MAX_BROWSER_SERVERS, sizeof( int ), browsermenu_ServerNameCompareFunc );
		break;
	// Map name.
	case 2:

		qsort( g_iSortedServers, MAX_BROWSER_SERVERS, sizeof( int ), browsermenu_MapNameCompareFunc );
		break;
	// Players.
	case 3:

		qsort( g_iSortedServers, MAX_BROWSER_SERVERS, sizeof( int ), browsermenu_PlayersCompareFunc );
		break;
	}
}

//*****************************************************************************
//
static int STACK_ARGS browsermenu_PingCompareFunc( const void *arg1, const void *arg2 )
{
	if (( M_ShouldShowServer( *(int *)arg1 ) == false ) && ( M_ShouldShowServer( *(int *)arg2 ) == false ))
		return ( 0 );

	if ( M_ShouldShowServer( *(int *)arg1 ) == false )
		return ( 1 );

	if ( M_ShouldShowServer( *(int *)arg2 ) == false )
		return ( -1 );

	return ( BROWSER_GetPing( *(int *)arg1 ) - BROWSER_GetPing( *(int *)arg2 ));
}

//*****************************************************************************
//
static int STACK_ARGS browsermenu_ServerNameCompareFunc( const void *arg1, const void *arg2 )
{
	if (( M_ShouldShowServer( *(int *)arg1 ) == false ) && ( M_ShouldShowServer( *(int *)arg2 ) == false ))
		return ( 0 );

	if ( M_ShouldShowServer( *(int *)arg1 ) == false )
		return ( 1 );

	if ( M_ShouldShowServer( *(int *)arg2 ) == false )
		return ( -1 );

	return ( stricmp( BROWSER_GetHostName( *(int *)arg1 ), BROWSER_GetHostName( *(int *)arg2 )));
}

//*****************************************************************************
//
static int STACK_ARGS browsermenu_MapNameCompareFunc( const void *arg1, const void *arg2 )
{
	if (( M_ShouldShowServer( *(int *)arg1 ) == false ) && ( M_ShouldShowServer( *(int *)arg2 ) == false ))
		return ( 0 );

	if ( M_ShouldShowServer( *(int *)arg1 ) == false )
		return ( 1 );

	if ( M_ShouldShowServer( *(int *)arg2 ) == false )
		return ( -1 );

	return ( stricmp( BROWSER_GetMapname( *(int *)arg1 ), BROWSER_GetMapname( *(int *)arg2 )));
}

//*****************************************************************************
//
static int STACK_ARGS browsermenu_PlayersCompareFunc( const void *arg1, const void *arg2 )
{
	if (( M_ShouldShowServer( *(int *)arg1 ) == false ) && ( M_ShouldShowServer( *(int *)arg2 ) == false ))
		return ( 0 );

	if ( M_ShouldShowServer( *(int *)arg1 ) == false )
		return ( 1 );

	if ( M_ShouldShowServer( *(int *)arg2 ) == false )
		return ( -1 );

	return ( BROWSER_GetNumPlayers( *(int *)arg2 ) - BROWSER_GetNumPlayers( *(int *)arg1 ));
}

//*****************************************************************************
//
CCMD( querymaster )
{
	M_RefreshServers();
}

//*****************************************************************************
//
CCMD ( menu_join_selected_server )
{
	if ( g_lSelectedServer < 0 )
	{
		M_StartMessage( "No server selected.\n\npress a key.", 1 );
		return;
	}

	FString command;
	command.Format( "restart -connect %s -iwad %s", BROWSER_GetAddress( g_lSelectedServer ).ToString(), BROWSER_GetIWADName ( g_lSelectedServer ) );

	TArray<FString> wadfiles;
	TArray<FString> missingWadfiles;

	if ( BROWSER_GetNumPWADs( g_lSelectedServer ) > 0 )
	{
		command.AppendFormat ( " -file" );
		for ( int i = 0; i < BROWSER_GetNumPWADs( g_lSelectedServer ); ++i )
		{
			if ( D_AddFile ( wadfiles, BROWSER_GetPWADName( g_lSelectedServer, i ) ) == false )
				missingWadfiles.Push ( BROWSER_GetPWADName( g_lSelectedServer, i ) );

			command.AppendFormat ( " %s", BROWSER_GetPWADName( g_lSelectedServer, i ) );
		}
	}

	if ( missingWadfiles.Size() > 0 )
	{
		FString errorMessage = "Can't find the following files:\n";
		for ( unsigned int i = 0; i < missingWadfiles.Size(); ++i )
		{
			errorMessage += missingWadfiles[i];
			errorMessage += "\n";
		}
		errorMessage += "\nPress a key.";
		M_StartMessage( errorMessage.GetChars(), 1 );
		return;
	}

	M_ClearMenus();
	Printf ( "Restarting with command \"%s\"\n", command.GetChars() );
	C_DoCommand( command );
}
