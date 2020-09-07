//-----------------------------------------------------------------------------
//
// Skulltag Statsmaker Source
// Copyright (C) 2005 Brad Carney
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
// Date created:  4/4/05
//
//
// Filename: main.cpp
//
// Description: Entry point, Printf, and misc functions.
//
//-----------------------------------------------------------------------------

#include "i_system.h"
#include "../src/networkheaders.h"
#include "../src/networkshared.h"
#include "main.h"
#include "gui.h"
#include "collector.h"
#include "protocol_zandronum.h"
#include "protocol_zdaemon.h"
#include "resource.h"

#include <time.h>

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- VARIABLES -------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

#ifndef NO_GUI
extern HWND						g_hDlg_Overview;

// Thread handle for the main thread that gathers stats.
HANDLE							g_hThread;
HINSTANCE						g_hInst;
#endif

// Data for each of the port we're gathering stats for.
PORT_s							g_PortInfo[NUM_PORTS];

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- PROTOTYPES ------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

void main_SetupProtocols ( ) {
	//=====================	
	// Setup the protocols.
	//=====================

	// Zandronum (zandronum.com)
	ZANDRONUM_Construct( );
	g_PortInfo[PORT_ZANDRONUM].pvQueryMasterServer			= ZANDRONUM_QueryMasterServer;
	g_PortInfo[PORT_ZANDRONUM].pvParseMasterServerResponse	= ZANDRONUM_ParseMasterServerResponse;
	g_PortInfo[PORT_ZANDRONUM].pvQueryServer					= ZANDRONUM_QueryServer;
	g_PortInfo[PORT_ZANDRONUM].pvParseServerResponse			= ZANDRONUM_ParseServerResponse;
	g_PortInfo[PORT_ZANDRONUM].bHuffman = true;
	g_PortInfo[PORT_ZANDRONUM].bEnabled = true;
	strcpy( g_PortInfo[PORT_ZANDRONUM].szName, "Zandronum" );
	g_PortInfo[PORT_ZANDRONUM].iOverviewNowLabelID = IDC_OVERVIEW_ZAPLAYERS_NOW;
	g_PortInfo[PORT_ZANDRONUM].iOverviewTodayLabelID = IDC_OVERVIEW_ZAPLAYERS_TOD;
	g_PortInfo[PORT_ZANDRONUM].iOnTopNowIconID = IDC_ONTOPNOW_ZA;
	g_PortInfo[PORT_ZANDRONUM].iOnTopTodayIconID = IDC_ONTOPTODAY_ZA;
	g_PortInfo[PORT_ZANDRONUM].MasterServerInfo.Address = *ZANDRONUM_GetMasterServerAddress();


	// ZDaemon (www.zdaemon.org)
	g_PortInfo[PORT_ZDAEMON].pvQueryMasterServer			= ZDAEMON_QueryMasterServer;
	g_PortInfo[PORT_ZDAEMON].pvParseMasterServerResponse	= ZDAEMON_ParseMasterServerResponse;
	g_PortInfo[PORT_ZDAEMON].pvQueryServer					= ZDAEMON_QueryServer;
	g_PortInfo[PORT_ZDAEMON].pvParseServerResponse			= ZDAEMON_ParseServerResponse;
	g_PortInfo[PORT_ZDAEMON].bHuffman = false;
	g_PortInfo[PORT_ZDAEMON].bEnabled = false;
	strcpy( g_PortInfo[PORT_ZDAEMON].szName, "ZDaemon" );
	g_PortInfo[PORT_ZDAEMON].iOverviewNowLabelID = IDC_OVERVIEW_ZDPLAYERS_NOW;
	g_PortInfo[PORT_ZDAEMON].iOverviewTodayLabelID = IDC_OVERVIEW_ZDPLAYERS_TOD;
	g_PortInfo[PORT_ZDAEMON].iOnTopNowIconID = IDC_ONTOPNOW_ZD;
	g_PortInfo[PORT_ZDAEMON].iOnTopTodayIconID = IDC_ONTOPTODAY_ZD;
	NETWORK_StringToAddress( "zdaemon.ath.cx", &g_PortInfo[PORT_ZDAEMON].MasterServerInfo.Address );
	g_PortInfo[PORT_ZDAEMON].MasterServerInfo.Address.usPort = htons( 15300 );
	ZDAEMON_Construct( );
}

#ifdef NO_GUI
int main ( ) {
	NETWORK_Construct( DEFAULT_STATS_PORT, false );
	main_SetupProtocols ( );
  COLLECTOR_StartCollecting ( );
}
#else

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

	// First, start networking.
	NETWORK_Construct( DEFAULT_STATS_PORT, false );

	main_SetupProtocols ( );

	// Create the dialog, which will call COLLECTOR_StartCollecting.
	GUI_CreateDialog( );

	return 0;
}

//*****************************************************************************
//
void Printf( const char *pszString, ... )
{
	va_list		ArgPtr;
	va_start( ArgPtr, pszString );
	VPrintf( pszString, ArgPtr );
	va_end( ArgPtr );
}

//*****************************************************************************
//
void VPrintf( const char *pszString, va_list Parms )
{
	char	szOutLine[8192];
	vsprintf( szOutLine, pszString, Parms );
	MAIN_Print( szOutLine );
}

//*****************************************************************************
//
void MAIN_Print( char *pszString )
{
	char	szBuffer[SERVERCONSOLE_TEXTLENGTH];
	char	szInputString[SERVERCONSOLE_TEXTLENGTH];
	char	*psz;
	char	c;
	bool	bScroll = false;
	bool	bRecycled = false;

	// Prefix the line with a timestamp (unless it's a newline).
	if ( strlen( pszString ) && pszString[0] != '\n' )
	{
		time_t			CurrentTime;
		struct	tm		*pTimeInfo;

		time( &CurrentTime );
		pTimeInfo = localtime( &CurrentTime );

		// It's AM if the hour in the day is less than 12.
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

	lVisibleLine = SendDlgItemMessage( g_hDlg_Overview, IDC_CONSOLEBOX, EM_GETFIRSTVISIBLELINE, 0, 0 );
	lTotalLines = SendDlgItemMessage( g_hDlg_Overview, IDC_CONSOLEBOX, EM_GETLINECOUNT, 0, 0 );
	lLineDiff = lTotalLines - lVisibleLine;
	bScroll = ( lLineDiff <= 9 );

	if ( GetDlgItemText( g_hDlg_Overview, IDC_CONSOLEBOX, szBuffer, sizeof( szBuffer )))
	{
		LONG	lDifference;
		char	szConsoleBuffer[SERVERCONSOLE_TEXTLENGTH];

		// If the amount of text added to the buffer will cause a buffer overflow, shuffle the text upwards.
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

		sprintf( szConsoleBuffer, "%s%s", psz, szInputString );
		SetDlgItemText( g_hDlg_Overview, IDC_CONSOLEBOX, szConsoleBuffer );

		// If the user has scrolled all the way down, autoscroll.
		if ( bScroll )
			SendDlgItemMessage( g_hDlg_Overview, IDC_CONSOLEBOX, EM_LINESCROLL, 0, lTotalLines );

		// If they've scrolled up but we've trimmed the text, don't autoscroll and compensate. 
		else if( bRecycled && ( lVisibleLine > 0 ) )
			SendDlgItemMessage( g_hDlg_Overview, IDC_CONSOLEBOX, EM_LINESCROLL, 0, lVisibleLine - 1 );

		// If they've scrolled up, don't autoscroll.
		else
			SendDlgItemMessage( g_hDlg_Overview, IDC_CONSOLEBOX, EM_LINESCROLL, 0, lVisibleLine );
	}
}
#endif
