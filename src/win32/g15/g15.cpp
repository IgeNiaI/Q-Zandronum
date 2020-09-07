//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2007 Rivecoder
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
// Created:  9/28/07
//
//
// Filename: g15.cpp
//
// Description: Contains routines for Skulltag's Logitech G15 applet.
//				The Logitech G15 is a gaming keyboard with a LCD screen.
//
// License: This section uses the Logitech LCD SDK lawfully under
//			the "End-User License Agreement for Logitech LCD SDK".
//
//-----------------------------------------------------------------------------

#include "g15.h"

//*****************************************************************************
//	PLATFORM

#ifndef G15_ENABLED

	// Just define the stubs.
	void		G15_Construct( void ) { }
	void		G15_NextLevel( const char *lump, const char *name ) { }
	void		G15_Tick( void ) { }
	bool		G15_TryConnect( void ) { return false; }
	bool		G15_IsReady( void ) { return false; }
	bool		G15_IsDeviceConnected( void );
	void		G15_Deconstruct( void ) { }
	void		G15_Printf( const char *pszString ) { }
	void		G15_ShowLargeFragMessage( const char *name, bool bWeFragged ) { }
#else

#include "include/lcdwin32.h"
#include "d_player.h"
#include "c_dispatch.h"
#include "include/EZ_LCD.h"
#include "g15.h"
#include "g_game.h"
#include "../resource.h"
#include "network.h"
#include "sbar.h"
#include "version.h"
#include "v_text.h"
#include "doomstat.h"

//*****************************************************************************
//	VARIABLES

// Pointer to our ezLCD object.
static	CEzLcd		*g_LCD;

// Safety switch.
static	bool		bCanDraw;

// Are we connected?
static	bool		bHasDevice;

// Our current display mode.
static	LCDMODE_e	g_CurrentLCDMode;

// Display elements for the logo mode.
static	HANDLE		g_hLogo_icon;
static	HANDLE		g_hLogo_title;
static	HANDLE		g_hLogo_text;

// Display elements for the HUD mode.
static	char		g_szLastConsoleMessage[128];
static	HANDLE		g_hHUD_message1;
static	HANDLE		g_hHUD_message2;
static	HANDLE		g_hHUD_health;
static	HANDLE		g_hHUD_armor;
static	HANDLE		g_hHUD_ammo;
static	HANDLE		g_hHUD_stimpack_ico;
static	HANDLE		g_hHUD_armor_ico;
static	HANDLE		g_hHUD_ammo_ico;

// Display elements for the message mode.
static	HANDLE		g_hMessage_message1;
static	ULONG		g_ulMessageTicks;

// External reference to the main window.
extern	HINSTANCE	g_hInst;

//*****************************************************************************
//	PROTOTYPES

void g15_ChangeMode( LCDMODE_e LCDMode );
void g15_HideAll( void );
void g15_ShowOrHideItems( LCDMODE_e LCDMode, bool bVisible );
void g15_SetUpModes( void );
void g15_ClearConsole( void );

//*****************************************************************************
//	CONSOLE COMMANDS / VARIABLES

CUSTOM_CVAR (Bool, g15_enable, true, CVAR_ARCHIVE)
{	
	if(g15_enable)
	{		
		// (Re) create the applet.
		G15_Construct( );
	}
	else
	{
		// Destroy the applet.		
		G15_Deconstruct( );	
	}
}

//*****************************************************************************
//
CCMD ( g15_isconnected )
{
	Printf("Logitech G15: %s, %s.\n",
		G15_IsDeviceConnected() ? "connected" : "disconnected",
		G15_IsReady() ? "enabled" : "disabled");
}

CVAR (Bool, g15_showlargefragmessages, true, CVAR_ARCHIVE);

//*****************************************************************************
//	PUBLIC FUNCTIONS

void G15_Construct( void )
{
	// First, prevent any drawing to an existing applet.
	G15_Deconstruct( );

	// Try connecting to the LCD.
	// If the user has the display disabled or doesn't have the keyboard, stop here.
	if ( ! g15_enable || ! G15_TryConnect() )
		return;

	// Create our display modes.
	g15_SetUpModes( );

	// Good! Enable the device.
	bCanDraw = true;
	g15_HideAll( );
	g15_ChangeMode( LCDMODE_LOGO );

	// Draw the logo while Skulltag loads.
	G15_Tick( );

	// Call G15_Deconstruct() when Skulltag closes.
	atterm( G15_Deconstruct );
}

//*****************************************************************************
//
void G15_Tick( void )
{
	if ( G15_IsReady( ) )
	{
		// Update the message mode.
		if ( g_ulMessageTicks > 0 )
		{
			g_ulMessageTicks--;
			if ( g_CurrentLCDMode != LCDMODE_MESSAGE )
				g15_ChangeMode( LCDMODE_MESSAGE );
		}

		// Update the in-game HUD.
		else if (( gamestate == GS_LEVEL )  && ( players[consoleplayer].mo ) && ( !players[consoleplayer].bSpectating ))
		{
			FString		text;

			if ( g_CurrentLCDMode != LCDMODE_HUD )
				g15_ChangeMode( LCDMODE_HUD );			
			
			// Update health.
			text.Format( "%d", players[consoleplayer].health );
			g_LCD->SetText( g_hHUD_health, text );

			// Update armor.
			{
				ABasicArmor		*pInventory = players[consoleplayer].mo->FindInventory<ABasicArmor>( );
				
				if ( pInventory )
				{
					text.Format( "%d", pInventory->Amount );
					g_LCD->SetText( g_hHUD_armor, text );
				}
				g_LCD->SetVisible( g_hHUD_armor, pInventory != NULL );
			}

			// Update ammo (if the player isn't using an ammo-less weapon).
			{
				bool	bAmmo = ( players[consoleplayer].ReadyWeapon && players[consoleplayer].ReadyWeapon->Ammo1 );
				
				if ( bAmmo )
				{
					text.Format("%d", players[consoleplayer].ReadyWeapon->Ammo1->Amount);			
					g_LCD->SetText( g_hHUD_ammo, text );
				}
				g_LCD->SetVisible( g_hHUD_ammo, bAmmo );
			}
		}

		// Otherwise, just show the Skulltag logo.
		else if ( g_CurrentLCDMode != LCDMODE_LOGO )
			g15_ChangeMode( LCDMODE_LOGO );

		// Update the LCD.
		g_LCD->SetExpiration( INFINITE );
		g_LCD->Update( );
	}
}

//*****************************************************************************
//
void G15_Deconstruct( void )
{
	bCanDraw = false;
	g_szLastConsoleMessage[0] = 0;
	g_CurrentLCDMode = LCDMODE_LOGO;

	if ( g_LCD != NULL )
	{
		delete g_LCD;
		g_LCD = NULL;
	}
}

//*****************************************************************************
//
void G15_NextLevel( const char *lump, const char *name )
{
	if( G15_IsReady( ) )
	{
		// Remove the messages from the last level.
		g15_ClearConsole( );

		// Print a little "MAP - name" message.
		sprintf(g_szLastConsoleMessage,"%s - %s", lump, name);
		g_LCD->SetText(g_hHUD_message1, g_szLastConsoleMessage);
	}
}

//*****************************************************************************
//
bool G15_IsReady( void )
{
	// Checks if we can draw to the LCD, and that the user wants us to.
	return ( G15_IsDeviceConnected() && g15_enable && bCanDraw && NETWORK_GetState( ) != NETSTATE_SERVER );
}

//*****************************************************************************
//
bool G15_TryConnect( void )
{
	// See if we can connect to the keyboard.
 	g_LCD = new CEzLcd(GAMENAME, G15_LCDWIDTH, G15_LCDHEIGHT);
	if ( g_LCD->Connect() )
	{
		bHasDevice = true;
		bCanDraw = true;
		return true;
	}
	else
	{
		bHasDevice = false;
		G15_Deconstruct( );
		return false;
	}
}

//*****************************************************************************
//
bool G15_IsDeviceConnected( void )
{
	// Checks if the user has a G15 keyboard that we can draw to.
	return ( g_LCD && bHasDevice );
}

//*****************************************************************************
//	PUBLIC DISPLAY FUNCTIONS

void G15_Printf( const char *pszString )
{
	// Print a message to the HUD's 'console'.
	if ( G15_IsReady() && g_CurrentLCDMode == LCDMODE_HUD)
	{
		char szIncoming[128];
		strncpy( szIncoming, pszString, 127 );

		V_ColorizeString( szIncoming );
		V_RemoveColorCodes( szIncoming );

		// If there's no last message, use the upper string.
		if( strlen( g_szLastConsoleMessage ) == 0 )
			g_LCD->SetText(g_hHUD_message1, szIncoming);
		else
		{
			g_LCD->SetText(g_hHUD_message1, g_szLastConsoleMessage);
			g_LCD->SetText(g_hHUD_message2, szIncoming);
		}

		strncpy(g_szLastConsoleMessage, pszString, 127);
	}
}

//*****************************************************************************
//
void G15_ShowLargeFragMessage( const char *name, bool bWeFragged )
{
	// Show the "you fragged!" / "you were fragged!" message.
	g_ulMessageTicks = TICRATE * 2;
	FString text;

	if ( bWeFragged )
		text.Format("You fragged %s!", name);
	else
		text.Format("You were fragged by %s!", name);

	g_LCD->SetText(g_hMessage_message1,text);
}

//*****************************************************************************
//	HELPER FUNCTIONS

void g15_ChangeMode( LCDMODE_e LCDMode )
{
	// First, hide all of the objects from the old mode.
	g15_ShowOrHideItems( g_CurrentLCDMode, false );

	g_CurrentLCDMode = LCDMode;

	// Now, show the needed items for the next mode.
	g15_ShowOrHideItems( g_CurrentLCDMode, true );
}

//*****************************************************************************
//
void g15_HideAll( void )
{
	for ( int i = 0; i < NUM_LCDMODES; i++ )
		g15_ShowOrHideItems( ((LCDMODE_e) i), false );
}

//*****************************************************************************
//
void g15_ShowOrHideItems( LCDMODE_e LCDMode, bool bVisible )
{
	// Shows or hides the items of this game mode (labels, graphics, progress bars, etc).
	switch ( LCDMode )
	{
	case LCDMODE_LOGO:
		g_LCD->SetVisible(g_hLogo_icon, bVisible);
		g_LCD->SetVisible(g_hLogo_title, bVisible);
		g_LCD->SetVisible(g_hLogo_text, bVisible);
		break;
	case LCDMODE_HUD:
		g_LCD->SetVisible(g_hHUD_health, bVisible);
		g_LCD->SetVisible(g_hHUD_armor, bVisible);
		g_LCD->SetVisible(g_hHUD_ammo, bVisible);
		g_LCD->SetVisible(g_hHUD_stimpack_ico, bVisible);
		g_LCD->SetVisible(g_hHUD_armor_ico, bVisible);
		g_LCD->SetVisible(g_hHUD_ammo_ico, bVisible);
		g_LCD->SetVisible(g_hHUD_message1, bVisible);
		g_LCD->SetVisible(g_hHUD_message2, bVisible);
		break;
	case LCDMODE_MESSAGE:
		g_LCD->SetVisible(g_hMessage_message1, bVisible);
		break;
	}
}

//*****************************************************************************
//
void g15_ClearConsole( void )
{
	g_szLastConsoleMessage[0] = 0;
	g_LCD->SetText(g_hHUD_message1, g_szLastConsoleMessage);
	g_LCD->SetText(g_hHUD_message2, g_szLastConsoleMessage);
}

//*****************************************************************************
//
void g15_SetUpModes( void )
{
	HICON		hIcon;

	//======================
	// Set up the logo mode.
	//======================
	
	// Orion player logo.
	hIcon = static_cast<HICON>( LoadImage( g_hInst,
			MAKEINTRESOURCE( G15_ICON_ORION ),
			IMAGE_ICON,	32, 32, LR_SHARED ) );
	g_hLogo_icon = g_LCD->AddIcon(hIcon, 32, 32);
	g_LCD->SetOrigin(g_hLogo_icon, 17, 8);
	
	// Main 'SKULLTAG' title.
 	g_hLogo_title = g_LCD->AddText(LG_STATIC_TEXT, LG_BIG, DT_LEFT, 120);
	g_LCD->SetOrigin(g_hLogo_title, 56, 12);
	g_LCD->SetText(g_hLogo_title,GAMENAME);

	// Version string.
	g_hLogo_text = g_LCD->AddText(LG_STATIC_TEXT, LG_MEDIUM, DT_CENTER, 120);
	g_LCD->SetOrigin(g_hLogo_text, 51, 29);
	g_LCD->SetText(g_hLogo_text,GetVersionStringRev());

	//=============================
	// Set up the in-game HUD mode.
	//=============================

	// Console.
	g_hHUD_message1 = g_LCD->AddText(LG_STATIC_TEXT, LG_SMALL, DT_LEFT, G15_LCDWIDTH);
	g_LCD->SetOrigin(g_hHUD_message1, 0, 0);
	g_LCD->SetText(g_hHUD_message1, "");

	g_hHUD_message2 = g_LCD->AddText(LG_STATIC_TEXT, LG_SMALL, DT_LEFT, G15_LCDWIDTH);
	g_LCD->SetOrigin(g_hHUD_message2, 0, 10);
	g_LCD->SetText(g_hHUD_message2, "");

	// Health icon and indicator.
	hIcon = static_cast<HICON>( LoadImage( g_hInst,
			MAKEINTRESOURCE( G15_ICON_STIMPACK ),
			IMAGE_ICON,	16, 16, LR_SHARED ) );
	g_hHUD_stimpack_ico = g_LCD->AddIcon(hIcon, 16, 16);
	g_LCD->SetOrigin(g_hHUD_stimpack_ico, 1, 24);
	
	g_hHUD_health = g_LCD->AddText(LG_STATIC_TEXT, LG_BIG, DT_LEFT, 60);
	g_LCD->SetOrigin(g_hHUD_health, 20, 27);
	g_LCD->SetText(g_hHUD_health, "");

	// Armor icon and indicator.
	hIcon = static_cast<HICON>( LoadImage( g_hInst,
			MAKEINTRESOURCE( G15_ICON_ARMOR ),
			IMAGE_ICON,	16, 16, LR_SHARED ) );
	g_hHUD_armor_ico = g_LCD->AddIcon(hIcon, 16, 16);
	g_LCD->SetOrigin(g_hHUD_armor_ico, 54, 24);

	g_hHUD_armor = g_LCD->AddText(LG_STATIC_TEXT, LG_BIG, DT_LEFT, 60);
	g_LCD->SetOrigin(g_hHUD_armor, 74, 27);
	g_LCD->SetText(g_hHUD_armor, "");

	// Ammo icon and indicator.
	hIcon = static_cast<HICON>( LoadImage( g_hInst,
			MAKEINTRESOURCE( G15_ICON_SHELLS ),
			IMAGE_ICON,	16, 16, LR_SHARED ) );
	g_hHUD_ammo_ico = g_LCD->AddIcon(hIcon, 16, 16);
	g_LCD->SetOrigin(g_hHUD_ammo_ico, 108, 24);

	g_hHUD_ammo = g_LCD->AddText(LG_STATIC_TEXT, LG_BIG, DT_RIGHT, 30);
	g_LCD->SetOrigin(g_hHUD_ammo, 128, 27);
	g_LCD->SetText(g_hHUD_ammo, "");

	//=========================
	// Set up the message mode.
	//=========================

	g_hMessage_message1 = g_LCD->AddText(LG_STATIC_TEXT, LG_MEDIUM, DT_CENTER, G15_LCDWIDTH-10);
	g_LCD->SetOrigin(g_hMessage_message1, 5, 14);
	g_LCD->SetText(g_hMessage_message1, "");
	
	g_szLastConsoleMessage[0] = 0;
	g_ulMessageTicks = 0;
}

#endif
