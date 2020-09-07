//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2008 Benjamin Berkels
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
//
//
// Filename: st_hud.cpp
//
// Description: Contains extensions to the HUD code.
//
//-----------------------------------------------------------------------------

#include "st_hud.h"
#include "c_console.h"
#include "chat.h"
#include "v_video.h"
#include "v_text.h"
#include "gamemode.h"
#include "g_level.h"
#include "doomstat.h"
#include "team.h"

//*****************************************************************************
//	CONSOLE VARIABLES

CVAR( Bool, cl_drawcoopinfo, true, CVAR_ARCHIVE )
EXTERN_CVAR( Int, con_notifylines )

//*****************************************************************************
//	FUNCTIONS

bool HUD_IsScaled ( )
{
	return ( con_scaletext ) && ( con_virtualwidth > 0 ) && ( con_virtualheight > 0 );
}

int HUD_GetWidth ( )
{
	return ( HUD_IsScaled() ? con_virtualwidth : SCREENWIDTH );
}

void HUD_DrawTexture( FTexture *Img, int X, int Y, const bool Scale )
{
	screen->DrawTexture( Img,
		X,
		Y,
		DTA_UseVirtualScreen, Scale,
		TAG_DONE );
}

void HUD_DrawTexture( FTexture *Img, int X, int Y )
{
	HUD_DrawTexture( Img, X, Y, HUD_IsScaled() );
}

void HUD_DrawText( FFont* Font, int Normalcolor, int X, int Y, const char *String, const bool Scale )
{
	screen->DrawText( Font, Normalcolor,
		X,
		Y,
		String,
		DTA_UseVirtualScreen, Scale,
		TAG_DONE );
}

void HUD_DrawText( FFont* Font, int Normalcolor, int X, int Y, const char *String )
{
	HUD_DrawText( Font, Normalcolor, X, Y, String, HUD_IsScaled() );
}

void HUD_DrawText( int Normalcolor, int X, int Y, const char *String, const bool Scale )
{
	HUD_DrawText( SmallFont, Normalcolor, X, Y, String, Scale );
}

void HUD_DrawTextAligned( int Normalcolor, int Y, const char *String, bool AlignLeft, const bool Scale )
{
	int screenWidthSacled = Scale ? con_virtualwidth : SCREENWIDTH;
	HUD_DrawText ( Normalcolor, AlignLeft ? 0 : ( screenWidthSacled - SmallFont->StringWidth ( String ) ) , Y, String, Scale );
}

void DrawHUD_CoopInfo()
{
	// [BB] Only draw the info if the user wishes to see it (cl_drawcoopinfo)
	// and if this is a cooperative or team based game mode. Further don't draw this in single player.
	if ( ( cl_drawcoopinfo == false ) || ( zadmflags & ZADF_NO_COOP_INFO )
		|| ! ( (GAMEMODE_GetCurrentFlags() & GMF_COOPERATIVE)
			|| (GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS) )
		|| NETWORK_GetState() == NETSTATE_SINGLE )
		return;

	const bool bScale = HUD_IsScaled();

	FString drawString;

	// [BB] We may not draw in the first 4 lines, this is reserved for chat messages.
	// Leave free another line to prevent the keys from being drawn over in ST's
	// fullscreen HUD.
	// [Dusk] Said message field can now have an arbitrary amount of lines, so
	// we cannot assume the default 4.
	const int yOffset = ( 1 + con_notifylines ) * SmallFont->GetHeight( );
	int playersDrawn = 0;

	for ( int i = 0; i < MAXPLAYERS; i++ )
	{
		// [BB] Only draw the info of players who are actually in the game.
		if ( (playeringame[i] == false) || ( players[i].bSpectating ) || (players[i].mo == NULL) )
			continue;

		// [BB] No need to draw the info of the player who's eyes we are looking through.
		if ( players[i].mo->CheckLocalView( consoleplayer ) )
			continue;

		// [BB] Only display team mates (in coop all players are team mates). Spectators see everybody.
		if ( players[consoleplayer].camera && !players[consoleplayer].camera->IsTeammate ( players[i].mo )
			&& !( players[consoleplayer].camera->player && players[consoleplayer].camera->player->bSpectating ) )
			continue;

		// [BB] We need more spacing if there is SECTINFO.
		int curYPos = yOffset + (playersDrawn/2) * ( ( 4 + ( level.info->SectorInfo.Names.Size() > 0 ) ) * SmallFont->GetHeight( ) + 3 ) ;

		const bool drawLeft = ( playersDrawn % 2 == 0 );

		// [BB] Draw player name.
		drawString = players[i].userinfo.GetName();
		V_ColorizeString( drawString );
		EColorRange nameColor = CR_GREY;
		// [BB] If the player is on a team, use the team's text color.
		if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
			nameColor = static_cast<EColorRange> ( TEAM_GetTextColor ( players[i].ulTeam ) );
		HUD_DrawTextAligned ( nameColor, curYPos, drawString.GetChars(), drawLeft, bScale );
		curYPos += SmallFont->GetHeight( ) + 1;

		// [BL] Draw the player's location, [BB] but only if the map has any SectorInfo.
		if ( level.info->SectorInfo.Names.Size() > 0 )
		{
			drawString = SECTINFO_GetPlayerLocation( i );
			V_ColorizeString( drawString );
			HUD_DrawTextAligned ( CR_GREY, curYPos, drawString.GetChars(), drawLeft, bScale );
			curYPos += SmallFont->GetHeight( ) + 1;
		}

		// [BB] Draw player health (color coded) and armor.
		EColorRange healthColor = CR_RED;
		// [BB] Player is alive.
		if ( players[i].mo->health <= 0 )
			drawString = "dead";
		else if ( SERVER_IsPlayerAllowedToKnowHealth ( consoleplayer, i ) )
		{
			AInventory* pArmor = players[i].mo->FindInventory(RUNTIME_CLASS(ABasicArmor));
			drawString.Format( "%d \\cD/ %d", players[i].mo->health, pArmor ? pArmor->Amount : 0 );
			V_ColorizeString( drawString );
			if ( players[i].mo->health > 66 )
				healthColor = CR_GREEN;
			else if ( players[i].mo->health > 33 )
				healthColor = CR_GOLD;
		}
		else
			drawString = "??? / ???";
		HUD_DrawTextAligned ( healthColor, curYPos, drawString.GetChars(), drawLeft, bScale );
		curYPos += SmallFont->GetHeight( ) + 1;

		// [BB] Draw player weapon and Ammo1/Ammo2, but only if the player is alive.
		// [Spleen] And don't draw ammo if sv_infiniteammo is enabled.
		if ( players[i].ReadyWeapon && players[i].mo->health > 0 )
		{
			drawString = players[i].ReadyWeapon->GetTag();
			if ( players[i].ReadyWeapon->Ammo1 && ( ( dmflags & DF_INFINITE_AMMO ) == false ) )
				drawString.AppendFormat( " \\cf%d", players[i].ReadyWeapon->Ammo1->Amount );
			else
				drawString += " \\cg-";
			if ( players[i].ReadyWeapon->Ammo2 && ( ( dmflags & DF_INFINITE_AMMO ) == false ) )
				drawString.AppendFormat( " \\cf%d", players[i].ReadyWeapon->Ammo2->Amount );
			V_ColorizeString( drawString );
			HUD_DrawTextAligned ( CR_GREEN, curYPos, drawString.GetChars(), drawLeft, bScale );
		}

		playersDrawn++;
	}
}
