//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2002-2006 Brad Carney
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
// Date created:  7/5/06
//
//
// Filename: a_flags.cpp
//
// Description: Contains definitions for the flags, as well as skulltag's skulls.
//
//-----------------------------------------------------------------------------

#include "a_sharedglobal.h"
#include "announcer.h"
#include "doomstat.h"
#include "cl_demo.h"
#include "cl_main.h"
#include "g_level.h"
#include "network.h"
#include "p_acs.h"
#include "sbar.h"
#include "scoreboard.h"
#include "sv_commands.h"
#include "team.h"
#include "v_text.h"
#include "v_video.h"
#include "gamemode.h"

//*****************************************************************************
//	DEFINES

#define	DENY_PICKUP			0
#define	ALLOW_PICKUP		1
#define	RETURN_FLAG			2

// Base team item -----------------------------------------------------------

IMPLEMENT_CLASS( ATeamItem )

//===========================================================================
//
// ATeamItem :: ShouldRespawn
//
// A flag should never respawn, so this function should always return false.
//
//===========================================================================

bool ATeamItem::ShouldRespawn( )
{
	return ( false );
}

//===========================================================================
//
// ATeamItem :: TryPickup
//
//===========================================================================

bool ATeamItem::TryPickup( AActor *&pToucher )
{
	AInventory	*pCopy;
	AInventory	*pInventory;

	// If we're not in teamgame mode, just use the default pickup handling.
	if ( !( GAMEMODE_GetCurrentFlags() & GMF_USETEAMITEM ) )
		return ( Super::TryPickup( pToucher ));

	// First, check to see if any of the toucher's inventory items want to
	// handle the picking up of this flag (other flags, perhaps?).

	// If HandlePickup() returns true, it will set the IF_PICKUPGOOD flag
	// to indicate that this item has been picked up. If the item cannot be
	// picked up, then it leaves the flag cleared.
	ItemFlags &= ~IF_PICKUPGOOD;
	if (( pToucher->Inventory != NULL ) && ( pToucher->Inventory->HandlePickup( this )))
	{
		// Let something else the player is holding intercept the pickup.
		if (( ItemFlags & IF_PICKUPGOOD ) == false )
			return ( false );

		ItemFlags &= ~IF_PICKUPGOOD;
		GoAwayAndDie( );

		// Nothing more to do in this case.
		return ( true );
	}

	// Only players that are on a team may pickup flags.
	if (( pToucher->player == NULL ) || ( pToucher->player->bOnTeam == false ))
		return ( false );

	switch ( AllowFlagPickup( pToucher ))
	{
	case DENY_PICKUP:

		// If we're not allowed to pickup this flag, return false.
		return ( false );
	case RETURN_FLAG:

		// Execute the return scripts.
		if ( NETWORK_InClientMode() == false )
		{
			if ( this->GetClass( ) == PClass::FindClass( "WhiteFlag" ))
			{
				FBehavior::StaticStartTypedScripts( SCRIPT_WhiteReturn, NULL, true );
			}
			else
			{
				FBehavior::StaticStartTypedScripts( TEAM_GetReturnScriptOffset( TEAM_GetTeamFromItem( this )), NULL, true );
			}
		}

		// In non-simple CTF mode, scripts take care of the returning and displaying messages.
		if ( TEAM_GetSimpleCTFSTMode( ))
		{
			if ( NETWORK_InClientMode() == false )
			{
				// The player is touching his own dropped flag; return it now.
				ReturnFlag( pToucher );

				// Mark the flag as no longer being taken.
				MarkFlagTaken( false );
			}

			// Display text saying that the flag has been returned.
			DisplayFlagReturn( );
		}

		// Reset the return ticks for this flag.
		ResetReturnTicks( );

		// Announce that the flag has been returned.
		AnnounceFlagReturn( );

		// Delete the flag.
		GoAwayAndDie( );

		// If we're the server, tell clients to destroy the flag.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_DestroyThing( this );

		// Tell clients that the flag has been returned.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_TeamFlagReturned( TEAM_GetTeamFromItem( this ) );
		}
		else
			SCOREBOARD_RefreshHUD( );

		return ( false );
	}

	// Announce the pickup of this flag.
	AnnounceFlagPickup( pToucher );

	// Player is picking up the flag.
	if ( NETWORK_InClientMode() == false )
	{
		FBehavior::StaticStartTypedScripts( SCRIPT_Pickup, pToucher, true );

		// If we're in simple CTF mode, we need to display the pickup messages.
		if ( TEAM_GetSimpleCTFSTMode( ))
		{
			// [CK] Signal that the flag/skull/some pickableable team item was taken
			GAMEMODE_HandleEvent ( GAMEEVENT_TOUCHES, pToucher, static_cast<int> ( TEAM_GetTeamFromItem( this ) ) );

			// Display the flag taken message.
			DisplayFlagTaken( pToucher );

			// Also, mark the flag as being taken.
			MarkFlagTaken( true );
		}

		// Reset the return ticks for this flag.
		ResetReturnTicks( );

		// Also, refresh the HUD.
		SCOREBOARD_RefreshHUD( );
	}

	pCopy = CreateCopy( pToucher );
	if ( pCopy == NULL )
		return ( false );

	pCopy->AttachToOwner( pToucher );

	// When we pick up a flag, take away any invisibility objects the player has.
	pInventory = pToucher->Inventory;
	while ( pInventory )
	{
		if (( pInventory->IsKindOf( RUNTIME_CLASS( APowerInvisibility ))) ||
			( pInventory->IsKindOf( RUNTIME_CLASS( APowerTranslucency ))))
		{
			// If we're the server, tell clients to destroy this inventory item.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_TakeInventory( ULONG( pToucher->player - players ), pInventory->GetClass(), 0 );

			pInventory->Destroy( );
		}

		pInventory = pInventory->Inventory;
	}

	return ( true );
}

//===========================================================================
//
// ATeamItem :: HandlePickup
//
//===========================================================================

bool ATeamItem::HandlePickup( AInventory *pItem )
{
	// Don't allow the pickup of invisibility objects when carrying a flag.
	if (( pItem->IsKindOf( RUNTIME_CLASS( APowerInvisibility ))) ||
		( pItem->IsKindOf( RUNTIME_CLASS( APowerTranslucency ))))
	{
		ItemFlags &= ~IF_PICKUPGOOD;

		return ( true );
	}

	return ( Super::HandlePickup( pItem ));
}

//===========================================================================
//
// ATeamItem :: AllowFlagPickup
//
// Determine whether or not we should be allowed to pickup this flag.
//
//===========================================================================

LONG ATeamItem::AllowFlagPickup( AActor *pToucher )
{
	// [BB] Only players on a team can pick up team items.
	if (( pToucher == NULL ) || ( pToucher->player == NULL ) || ( pToucher->player->bOnTeam == false ))
		return ( DENY_PICKUP );

	// [BB] Players are always allowed to return their own dropped team item.
	if (( this->GetClass( ) == TEAM_GetItem( pToucher->player->ulTeam )) &&
		( this->flags & MF_DROPPED ))
		return ( RETURN_FLAG );

	// [BB] If a client gets here, the server already made all necessary checks. So just allow the pickup.
	if ( NETWORK_InClientMode() )
		return ( ALLOW_PICKUP );

	// [BB] If a player already carries an enemy team item, don't let him pick up another one.
	if ( TEAM_FindOpposingTeamsItemInPlayersInventory ( pToucher->player ) )
		return ( DENY_PICKUP );

	// [BB] If the team the item belongs to doesn't have any players, don't let it be picked up.
	if ( TEAM_CountPlayers ( TEAM_GetTeamFromItem ( this ) ) == 0 )
	{
		FString message = "You can't pick up the ";
		if ( GAMEMODE_GetCurrentFlags() & GMF_USEFLAGASTEAMITEM )
			message += "flag";
		else
			message += "skull";
		message += "\nof a team with no players!";
		GAMEMODE_DisplaySUBSMessage( message.GetChars(), true, static_cast<ULONG>(pToucher->player - players), SVCF_ONLYTHISCLIENT );
		return ( DENY_PICKUP );
	}

	// [CK] Do not let pickups occur after the match has ended
	if ( GAMEMODE_IsGameInProgress( ) == false ) 
		return ( DENY_PICKUP );

	// Player is touching the enemy flag.
	if ( this->GetClass( ) != TEAM_GetItem( pToucher->player->ulTeam ))
		return ( ALLOW_PICKUP );

	return ( DENY_PICKUP );
}

//===========================================================================
//
// ATeamItem :: AnnounceFlagPickup
//
// Play the announcer sound for picking up this flag.
//
//===========================================================================

void ATeamItem::AnnounceFlagPickup( AActor *pToucher )
{
}

//===========================================================================
//
// ATeamItem :: DisplayFlagTaken
//
// Display the text for picking up this flag.
//
//===========================================================================

void ATeamItem::DisplayFlagTaken( AActor *pToucher )
{
}

//===========================================================================
//
// ATeamItem :: MarkFlagTaken
//
// Signal to the team module whether or not this flag has been taken.
//
//===========================================================================

void ATeamItem::MarkFlagTaken( bool bTaken )
{
	TEAM_SetItemTaken( TEAM_GetTeamFromItem( this ), bTaken );
}

//===========================================================================
//
// ATeamItem :: ResetReturnTicks
//
// Reset the return ticks for the team associated with this flag.
//
//===========================================================================

void ATeamItem::ResetReturnTicks( void )
{
}

//===========================================================================
//
// ATeamItem :: ReturnFlag
//
// Spawn a new flag at its original location.
//
//===========================================================================

void ATeamItem::ReturnFlag( AActor *pReturner )
{
}

//===========================================================================
//
// ATeamItem :: AnnounceFlagReturn
//
// Play the announcer sound for this flag being returned.
//
//===========================================================================

void ATeamItem::AnnounceFlagReturn( void )
{
}

//===========================================================================
//
// ATeamItem :: DisplayFlagReturn
//
// Display the text for this flag being returned.
//
//===========================================================================

void ATeamItem::DisplayFlagReturn( void )
{
}

// Skulltag flag ------------------------------------------------------------

IMPLEMENT_CLASS( AFlag )

//===========================================================================
//
// AFlag :: HandlePickup
//
// Ask this item in the actor's inventory to potentially react to this object
// attempting to be picked up.
//
//===========================================================================

bool AFlag::HandlePickup( AInventory *pItem )
{
	char				szString[256];
	DHUDMessageFadeOut	*pMsg;
	AInventory			*pInventory;
	bool				selfAssist = false;
	int playerAssistNumber = GAMEEVENT_CAPTURE_NOASSIST; // [CK] Need these for game event activators

	// If this object being given isn't a flag, then we don't really care.
	if ( pItem->GetClass( )->IsDescendantOf( RUNTIME_CLASS( AFlag )) == false )
		return ( Super::HandlePickup( pItem ));

	// If we're carrying the opposing team's flag, and trying to pick up our flag,
	// then that means we've captured the flag. Award a point.
	if (( this->GetClass( ) != TEAM_GetItem( Owner->player->ulTeam )) &&
		( pItem->GetClass( ) == TEAM_GetItem( Owner->player->ulTeam )))
	{
		// Don't award a point if we're touching a dropped version of our flag.
		if ( static_cast<AFlag *>( pItem )->AllowFlagPickup( Owner ) == RETURN_FLAG )
			return ( Super::HandlePickup( pItem )); 

		if (( TEAM_GetSimpleCTFSTMode( )) && ( NETWORK_InClientMode() == false ))
		{
			// Give his team a point.
			TEAM_SetScore( Owner->player->ulTeam, TEAM_GetScore( Owner->player->ulTeam ) + 1, true );
			PLAYER_SetPoints ( Owner->player, Owner->player->lPointCount + 1 );

			// Award the scorer with a "Capture!" medal.
			MEDAL_GiveMedal( ULONG( Owner->player - players ), MEDAL_CAPTURE );

			// Tell clients about the medal that been given.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_GivePlayerMedal( ULONG( Owner->player - players ), MEDAL_CAPTURE );

			// [RC] Clear the 'returned automatically' message. A bit hackish, but leaves the flag structure unchanged.
			this->ReturnFlag( NULL );
			if ( NETWORK_GetState( ) != NETSTATE_SERVER )
			{
				pMsg = new DHUDMessageFadeOut( SmallFont, "", 1.5f, TEAM_MESSAGE_Y_AXIS_SUB, 0, 0, CR_UNTRANSLATED, 3.0f, 0.5f );
				StatusBar->AttachMessage( pMsg, MAKE_ID( 'S','U','B','S' ));
			}
			// If necessary, send it to clients.
			else
				SERVERCOMMANDS_PrintHUDMessageFadeOut( "", 1.5f, TEAM_MESSAGE_Y_AXIS_SUB, 0, 0, CR_UNTRANSLATED, 3.0f, 0.5f, "SmallFont", false, MAKE_ID( 'S','U','B','S' ));

			// Create the "captured" message.
			sprintf( szString, "\\c%c%s team scores!", V_GetColorChar( TEAM_GetTextColor( Owner->player->ulTeam )), TEAM_GetName( Owner->player->ulTeam ));
			V_ColorizeString( szString );

			// Now, print it.
			if ( NETWORK_GetState( ) != NETSTATE_SERVER )
			{
				pMsg = new DHUDMessageFadeOut( BigFont, szString,
					1.5f,
					TEAM_MESSAGE_Y_AXIS,
					0,
					0,
					CR_UNTRANSLATED,
					3.0f,
					0.5f );
				StatusBar->AttachMessage( pMsg, MAKE_ID( 'C','N','T','R' ));
			}
			// If necessary, send it to clients.
			else
			{
				SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS, 0, 0, CR_UNTRANSLATED, 3.0f, 0.5f, "BigFont", false, MAKE_ID( 'C','N','T','R' ));
			}

			// [RC] Create the "scored by" and "assisted by" message.
			sprintf( szString, "\\c%cScored by: %s", V_GetColorChar( TEAM_GetTextColor( Owner->player->ulTeam )), Owner->player->userinfo.GetName() );
			const bool bAssisted = (TEAM_GetAssistPlayer(Owner->player->ulTeam) != MAXPLAYERS);
			if ( bAssisted )
			{
				for(ULONG i = 0; i < MAXPLAYERS; i++)
					if(&players[i] == Owner->player)
						if( TEAM_GetAssistPlayer( Owner->player->ulTeam) == i)
							selfAssist = true;

				if ( selfAssist )
					sprintf( szString + strlen ( szString ), "\\n\\c%c[ Self-Assisted ]", V_GetColorChar( TEAM_GetTextColor( Owner->player->ulTeam )));
				else
					sprintf( szString + strlen ( szString ), "\\n\\c%cAssisted by: %s", V_GetColorChar( TEAM_GetTextColor( Owner->player->ulTeam )), players[TEAM_GetAssistPlayer( Owner->player->ulTeam )].userinfo.GetName());
			}

			V_ColorizeString( szString );

			// Now, print it.
			if ( NETWORK_GetState( ) != NETSTATE_SERVER )
			{
				pMsg = new DHUDMessageFadeOut( SmallFont, szString,
					1.5f,
					TEAM_MESSAGE_Y_AXIS_SUB,
					0,
					0,
					CR_UNTRANSLATED,
					3.0f,
					0.5f );
				StatusBar->AttachMessage( pMsg, MAKE_ID( 'S','U','B','S' ));
			}
			// If necessary, send it to clients.
			else
			{
				SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS_SUB, 0, 0, CR_UNTRANSLATED, 3.0f, 0.5f, "SmallFont", false, MAKE_ID( 'S','U','B','S' ));

				if( ( bAssisted ) && ( ! selfAssist ) )
					SERVER_Printf( "%s \\c-and %s \\c-scored for the \\c%c%s \\c-team!\n", Owner->player->userinfo.GetName(), players[TEAM_GetAssistPlayer( Owner->player->ulTeam )].userinfo.GetName(), V_GetColorChar( TEAM_GetTextColor( Owner->player->ulTeam )), TEAM_GetName( Owner->player->ulTeam ));
				else
					SERVER_Printf( "%s \\c-scored for the \\c%c%s \\c-team!\n", Owner->player->userinfo.GetName(), V_GetColorChar( TEAM_GetTextColor( Owner->player->ulTeam )), TEAM_GetName( Owner->player->ulTeam ));
			}

			
			// If someone just recently returned the flag, award him with an "Assist!" medal.
			if ( bAssisted )
			{
				// [CK] Mark the assisting player
				playerAssistNumber = TEAM_GetAssistPlayer( Owner->player->ulTeam );

				MEDAL_GiveMedal( TEAM_GetAssistPlayer( Owner->player->ulTeam ), MEDAL_ASSIST );

				// Tell clients about the medal that been given.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_GivePlayerMedal( TEAM_GetAssistPlayer( Owner->player->ulTeam ), MEDAL_ASSIST );

				TEAM_SetAssistPlayer( Owner->player->ulTeam, MAXPLAYERS );
			}

			// [CK] Now we have the information to trigger an event script (Activator is the capturer, assister is the second arg)
			// PlayerAssistNumber will be GAMEEVENT_CAPTURE_NOASSIST (-1) if there was no assister
			GAMEMODE_HandleEvent ( GAMEEVENT_CAPTURES, Owner, playerAssistNumber );

			// Take the flag away.
			pInventory = Owner->FindInventory( this->GetClass( ));

			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_TakeInventory( ULONG( Owner->player - players ), pInventory->GetClass(), 0 );
			if ( pInventory )
				Owner->RemoveInventory( pInventory );


			// Also, refresh the HUD.
			SCOREBOARD_RefreshHUD( );
		}

		return ( true );
	}

	return ( Super::HandlePickup( pItem ));
}

//===========================================================================
//
// AFlag :: AllowFlagPickup
//
// Determine whether or not we should be allowed to pickup this flag.
//
//===========================================================================

LONG AFlag::AllowFlagPickup( AActor *pToucher )
{
	// Don't allow the pickup of flags in One Flag CTF.
	if ( oneflagctf )
		return ( DENY_PICKUP );

	return Super::AllowFlagPickup( pToucher );
}

//===========================================================================
//
// AFlag :: AnnounceFlagPickup
//
// Play the announcer sound for picking up this flag.
//
//===========================================================================

void AFlag::AnnounceFlagPickup( AActor *pToucher )
{
	// Don't announce the pickup if the flag is being given to someone as part of a snapshot.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) && ( CLIENT_GetConnectionState( ) == CTS_RECEIVINGSNAPSHOT ))
		return;

	// Build the message.
	// Whatever the team's name is, is the first part of the message. For example:
	// if the "blue" team has been defined then the message will be "BlueFlagTaken".
	// This way we don't have to change every announcer to use a new system. 
	FString name;
	name += TEAM_GetName( TEAM_GetTeamFromItem( this ));
	name += "FlagTaken";
	ANNOUNCER_PlayEntry( cl_announcer, name.GetChars( ));
}

//===========================================================================
//
// AFlag :: DisplayFlagTaken
//
// Display the text for picking up this flag.
//
//===========================================================================

void AFlag::DisplayFlagTaken( AActor *pToucher )
{
	char				szString[256];
	DHUDMessageFadeOut	*pMsg;

	// Create the "pickup" message.
	if (( pToucher->player - players ) == consoleplayer )
		sprintf( szString, "\\c%cYou have the %s flag!", V_GetColorChar( TEAM_GetTextColor( TEAM_GetTeamFromItem( this ))), TEAM_GetName( TEAM_GetTeamFromItem( this )));
	else
		sprintf( szString, "\\c%c%s flag taken!", V_GetColorChar( TEAM_GetTextColor( TEAM_GetTeamFromItem( this ))), TEAM_GetName( TEAM_GetTeamFromItem( this )));

	V_ColorizeString( szString );

	// Now, print it.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		pMsg = new DHUDMessageFadeOut( BigFont, szString,
			1.5f,
			TEAM_MESSAGE_Y_AXIS,
			0,
			0,
			CR_UNTRANSLATED,
			3.0f,
			0.25f );
		StatusBar->AttachMessage( pMsg, MAKE_ID( 'C','N','T','R' ));
	}
	// If necessary, send it to clients.
	else
	{
		sprintf( szString, "\\c%cYou have the %s flag!", V_GetColorChar( TEAM_GetTextColor( TEAM_GetTeamFromItem( this ))), TEAM_GetName( TEAM_GetTeamFromItem( this )));
		V_ColorizeString( szString );
		SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS, 0, 0, CR_UNTRANSLATED, 3.0f, 0.25f, "BigFont", false, MAKE_ID('C','N','T','R'), ULONG( pToucher->player - players ), SVCF_ONLYTHISCLIENT );

		sprintf( szString, "\\c%c%s flag taken!", V_GetColorChar( TEAM_GetTextColor( TEAM_GetTeamFromItem( this ))), TEAM_GetName( TEAM_GetTeamFromItem( this )));
		V_ColorizeString( szString );
		SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS, 0, 0, CR_UNTRANSLATED, 3.0f, 0.25f, "BigFont", false, MAKE_ID('C','N','T','R'), ULONG( pToucher->player - players ), SVCF_SKIPTHISCLIENT );
	}

	// [RC] Create the "held by" message for the team.
	ULONG playerIndex = ULONG( pToucher->player - players );
	sprintf( szString, "\\c%cHeld by: %s", V_GetColorChar( TEAM_GetTextColor( players[playerIndex].ulTeam )), players[playerIndex].userinfo.GetName() );

	V_ColorizeString( szString );

	// Now, print it.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		if (( pToucher->player - players ) != consoleplayer )
		{
			pMsg = new DHUDMessageFadeOut( SmallFont, szString,
				1.5f,
				TEAM_MESSAGE_Y_AXIS_SUB,
				0,
				0,
				CR_UNTRANSLATED,
				3.0f,
				0.25f );
			StatusBar->AttachMessage( pMsg, MAKE_ID( 'S','U','B','S' ));
		}
	}
	// If necessary, send it to clients.
	else
	{
		SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS_SUB, 0, 0, CR_UNTRANSLATED, 3.0f, 0.25f, "SmallFont", false, MAKE_ID('S','U','B','S'), ULONG( pToucher->player - players ), SVCF_SKIPTHISCLIENT  );
		SERVER_Printf( PRINT_MEDIUM, "%s \\c-has taken the \\c%c%s \\c-flag.\n", players[playerIndex].userinfo.GetName(), V_GetColorChar( TEAM_GetTextColor( TEAM_GetTeamFromItem( this ))), TEAM_GetName( TEAM_GetTeamFromItem( this )));
	}
}

//===========================================================================
//
// AFlag :: MarkFlagTaken
//
// Signal to the team module whether or not this flag has been taken.
//
//===========================================================================

void AFlag::MarkFlagTaken( bool bTaken )
{
	Super::MarkFlagTaken ( bTaken );
}

//===========================================================================
//
// AFlag :: ResetReturnTicks
//
// Reset the return ticks for the team associated with this flag.
//
//===========================================================================

void AFlag::ResetReturnTicks( void )
{
	TEAM_SetReturnTicks( TEAM_GetTeamFromItem( this ), 0 );
}

//===========================================================================
//
// AFlag :: ReturnFlag
//
// Spawn a new flag at its original location.
//
//===========================================================================

void AFlag::ReturnFlag( AActor *pReturner )
{
	POS_t	FlagOrigin;
	AFlag	*pActor;

	// Respawn the flag.
	FlagOrigin = TEAM_GetItemOrigin( TEAM_GetTeamFromItem( this ));

	pActor = (AFlag *)Spawn( this->GetClass( ), FlagOrigin.x, FlagOrigin.y, FlagOrigin.z, NO_REPLACE );

	// If we're the server, tell clients to spawn the new skull.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( pActor ))
		SERVERCOMMANDS_SpawnThing( pActor );

	// Since all inventory spawns with the MF_DROPPED flag, we need to unset it.
	if ( pActor )
		pActor->flags &= ~MF_DROPPED;

	ULONG ulItemTeam = TEAM_GetTeamFromItem( this );
	// Mark the flag as no longer being taken.
	TEAM_SetItemTaken( ulItemTeam, false );

	// If an opposing team's flag has been taken by one of the team members of the returner
	// the player who returned this flag has the chance to earn an "Assist!" medal.
	if ( pReturner && pReturner->player )
	{
		for ( ULONG i = 0; i < MAXPLAYERS; i++ )
		{
			if ( ( players[i].ulTeam == ulItemTeam )
			     && TEAM_FindOpposingTeamsItemInPlayersInventory ( &players[i] ) )
			{
				TEAM_SetAssistPlayer( pReturner->player->ulTeam, ULONG( pReturner->player - players ));
			}
		}
	}

	char szString[256];
	if ( pReturner && pReturner->player )
	{
		// [RC] Create the "returned by" message for this team.
		ULONG playerIndex = ULONG( pReturner->player - players );
		sprintf( szString, "\\c%cReturned by: %s", V_GetColorChar( TEAM_GetTextColor( players[playerIndex].ulTeam )), players[playerIndex].userinfo.GetName() );

		// [CK] Send out an event that a flag/skull was returned, this is the easiest place to do it
		// Second argument is the team index, third argument is what kind of return it was
		GAMEMODE_HandleEvent ( GAMEEVENT_RETURNS, pReturner, static_cast<int> ( ulItemTeam ), GAMEEVENT_RETURN_PLAYERRETURN );
	}
	else
	{
		// [RC] Create the "returned automatically" message for this team.
		sprintf( szString, "\\c%cReturned automatically.", V_GetColorChar( TEAM_GetTextColor( TEAM_GetTeamFromItem( this ))));

		// [CK] Indicate the server returned the flag/skull after a timeout
		GAMEMODE_HandleEvent ( GAMEEVENT_RETURNS, NULL, static_cast<int> ( ulItemTeam ), GAMEEVENT_RETURN_TIMEOUTRETURN );
	}

	V_ColorizeString( szString );
	GAMEMODE_DisplaySUBSMessage( szString, true );

	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		if ( pReturner && pReturner->player )
			SERVER_Printf( PRINT_MEDIUM, "%s \\c-returned the \\c%c%s \\c-flag.\n", pReturner->player->userinfo.GetName(), V_GetColorChar( TEAM_GetTextColor( TEAM_GetTeamFromItem( this ))), TEAM_GetName( TEAM_GetTeamFromItem( this )));
		else
			SERVER_Printf( PRINT_MEDIUM, "\\c%c%s \\c-flag returned.\n", V_GetColorChar( TEAM_GetTextColor( TEAM_GetTeamFromItem( this ))), TEAM_GetName( TEAM_GetTeamFromItem( this )));
	}
}

//===========================================================================
//
// AFlag :: AnnounceFlagReturn
//
// Play the announcer sound for this flag being returned.
//
//===========================================================================

void AFlag::AnnounceFlagReturn( void )
{
	// Build the message.
	// Whatever the team's name is, is the first part of the message. For example:
	// if the "blue" team has been defined then the message will be "BlueFlagReturned".
	// This way we don't have to change every announcer to use a new system. 
	FString name;
	name += TEAM_GetName( TEAM_GetTeamFromItem( this ));
	name += "FlagReturned";
	ANNOUNCER_PlayEntry( cl_announcer, name.GetChars( ));
}

//===========================================================================
//
// AFlag :: DisplayFlagReturn
//
// Display the text for this flag being returned.
//
//===========================================================================

void AFlag::DisplayFlagReturn( void )
{
	char						szString[256];

	// Create the "returned" message.
	sprintf( szString, "\\c%c%s flag returned", V_GetColorChar( TEAM_GetTextColor ( TEAM_GetTeamFromItem( this ))), TEAM_GetName ( TEAM_GetTeamFromItem( this )));

	V_ColorizeString( szString );

	GAMEMODE_DisplayCNTRMessage( szString, false );
}

// White flag ---------------------------------------------------------------

class AWhiteFlag : public AFlag
{
	DECLARE_CLASS( AWhiteFlag, AFlag )
protected:

	virtual bool HandlePickup( AInventory *pItem );
	virtual LONG AllowFlagPickup( AActor *pToucher );
	virtual void AnnounceFlagPickup( AActor *pToucher );
	virtual void DisplayFlagTaken( AActor *pToucher );
	virtual void MarkFlagTaken( bool bTaken );
	virtual void ResetReturnTicks( void );
	virtual void ReturnFlag( AActor *pReturner );
	virtual void AnnounceFlagReturn( void );
	virtual void DisplayFlagReturn( void );
};

IMPLEMENT_CLASS( AWhiteFlag )

//===========================================================================
//
// AWhiteFlag :: HandlePickup
//
// Ask this item in the actor's inventory to potentially react to this object
// attempting to be picked up.
//
//===========================================================================

bool AWhiteFlag::HandlePickup( AInventory *pItem )
{
	char				szString[256];
	DHUDMessageFadeOut	*pMsg;
	AInventory			*pInventory;
	ULONG				ulTeam;

	// If this object being given isn't a flag, then we don't really care.
	if ( pItem->GetClass( )->IsDescendantOf( RUNTIME_CLASS( AFlag )) == false )
		return ( Super::HandlePickup( pItem ));

	// If this isn't one flag CTF mode, then we don't really care here.
	if ( oneflagctf == false )
		return ( Super::HandlePickup( pItem ));

	// [BB] Bringing a WhiteFlag to another WhiteFlag doesn't give a point.
	if ( pItem->IsKindOf ( PClass::FindClass( "WhiteFlag" ) ) )
		return ( false );

	ulTeam = TEAM_GetTeamFromItem( this );

	if ( TEAM_GetTeamFromItem( pItem ) == Owner->player->ulTeam )
		return ( false );

	// If we're trying to pick up the opponent's flag, award a point since we're
	// carrying the white flag.
	if (( TEAM_GetSimpleCTFSTMode( )) && ( NETWORK_InClientMode() == false ))
	{
		// Give his team a point.
		TEAM_SetScore( Owner->player->ulTeam, TEAM_GetScore( Owner->player->ulTeam ) + 1, true );
		PLAYER_SetPoints ( Owner->player, Owner->player->lPointCount + 1 );

		// Award the scorer with a "Capture!" medal.
		MEDAL_GiveMedal( ULONG( Owner->player - players ), MEDAL_CAPTURE );

		// Tell clients about the medal that been given.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_GivePlayerMedal( ULONG( Owner->player - players ), MEDAL_CAPTURE );

		// If someone just recently returned the flag, award him with an "Assist!" medal.
		if ( TEAM_GetAssistPlayer( Owner->player->ulTeam ) != MAXPLAYERS )
		{
			MEDAL_GiveMedal( TEAM_GetAssistPlayer( Owner->player->ulTeam ), MEDAL_ASSIST );

			// Tell clients about the medal that been given.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_GivePlayerMedal( TEAM_GetAssistPlayer( Owner->player->ulTeam ), MEDAL_ASSIST );

			TEAM_SetAssistPlayer( Owner->player->ulTeam, MAXPLAYERS );
		}

		// Create the "captured" message.
		sprintf( szString, "\\c%c%s team scores!", V_GetColorChar( TEAM_GetTextColor( Owner->player->ulTeam )), TEAM_GetName( Owner->player->ulTeam ));
		V_ColorizeString( szString );

		// Now, print it.
		if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		{
			pMsg = new DHUDMessageFadeOut( BigFont, szString,
				1.5f,
				TEAM_MESSAGE_Y_AXIS,
				0,
				0,
				CR_WHITE,
				3.0f,
				0.25f );
			StatusBar->AttachMessage( pMsg, MAKE_ID( 'C','N','T','R' ));
		}
		// If necessary, send it to clients.
		else
		{
			SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS, 0, 0, CR_WHITE, 3.0f, 0.25f, "BigFont", false, MAKE_ID( 'C','N','T','R' ));
		}

		// [BC] Rivecoder's "scored by" message.
		sprintf( szString, "\\c%cScored by: %s", V_GetColorChar( TEAM_GetTextColor( Owner->player->ulTeam )), Owner->player->userinfo.GetName() );
		V_ColorizeString( szString );

		// Now, print it.
		if ( NETWORK_GetState( ) != NETSTATE_SERVER )
		{
			pMsg = new DHUDMessageFadeOut( SmallFont, szString,
				1.5f,
				TEAM_MESSAGE_Y_AXIS_SUB,
				0,
				0,
				CR_UNTRANSLATED,
				3.0f,
				0.5f );
			StatusBar->AttachMessage( pMsg, MAKE_ID( 'S','U','B','S' ));
		}
		// If necessary, send it to clients.
		else
			SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS_SUB, 0, 0, CR_UNTRANSLATED, 3.0f, 0.5f, "SmallFont", false, MAKE_ID( 'S','U','B','S' ));

		// Take the flag away.
		pInventory = Owner->FindInventory( this->GetClass( ));
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_TakeInventory( ULONG( Owner->player - players ), this->GetClass(), 0 );
		if ( pInventory )
			Owner->RemoveInventory( pInventory );

		this->ReturnFlag( NULL );

		// Also, refresh the HUD.
		SCOREBOARD_RefreshHUD( );
	
		return ( true );
	}

	return ( Super::HandlePickup( pItem ));
}

//===========================================================================
//
// AWhiteFlag :: AllowFlagPickup
//
// Determine whether or not we should be allowed to pickup this flag.
//
//===========================================================================

LONG AWhiteFlag::AllowFlagPickup( AActor *pToucher )
{
	// [BB] Carrying more than one WhiteFlag is not allowed.
	if (( pToucher == NULL ) || ( pToucher->FindInventory( PClass::FindClass( "WhiteFlag" ) ) == NULL ) )
		return ( ALLOW_PICKUP );
	else
		return ( DENY_PICKUP );
}

//===========================================================================
//
// AWhiteFlag :: AnnounceFlagPickup
//
// Play the announcer sound for picking up this flag.
//
//===========================================================================

void AWhiteFlag::AnnounceFlagPickup( AActor *pToucher )
{
	// Don't announce the pickup if the flag is being given to someone as part of a snapshot.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) && ( CLIENT_GetConnectionState( ) == CTS_RECEIVINGSNAPSHOT ))
		return;

	if (( pToucher == NULL ) || ( pToucher->player == NULL ))
		return;

	if ( playeringame[consoleplayer] && players[consoleplayer].bOnTeam && players[consoleplayer].mo )
	{
		if (( pToucher->player - players ) == consoleplayer )
			ANNOUNCER_PlayEntry( cl_announcer, "YouHaveTheFlag" );
		else if ( players[consoleplayer].mo->IsTeammate( pToucher ))
			ANNOUNCER_PlayEntry( cl_announcer, "YourTeamHasTheFlag" );
		else
			ANNOUNCER_PlayEntry( cl_announcer, "TheEnemyHasTheFlag" );
	}
}

//===========================================================================
//
// AWhiteFlag :: DisplayFlagTaken
//
// Display the text for picking up this flag.
//
//===========================================================================

void AWhiteFlag::DisplayFlagTaken( AActor *pToucher )
{
	ULONG				ulPlayer;
	char				szString[256];
	char				szName[48];
	DHUDMessageFadeOut	*pMsg;

	// Create the "pickup" message.
	if (( pToucher != NULL ) && ( pToucher->player != NULL ) && ( pToucher->player - players ) == consoleplayer )
		sprintf( szString, "\\cCYou have the flag!" );
	else
		sprintf( szString, "\\cCWhite flag taken!" );
	V_ColorizeString( szString );

	// Now, print it.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		pMsg = new DHUDMessageFadeOut( BigFont, szString,
			1.5f,
			TEAM_MESSAGE_Y_AXIS,
			0,
			0,
			CR_WHITE,
			3.0f,
			0.25f );
		StatusBar->AttachMessage( pMsg, MAKE_ID( 'C','N','T','R' ));
	}
	// If necessary, send it to clients.
	else
	{
		sprintf( szString, "\\cCYou have the flag!" );
		V_ColorizeString( szString );
		SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS, 0, 0, CR_WHITE, 3.0f, 0.25f, "BigFont", false, MAKE_ID('C','N','T','R'), ULONG( pToucher->player - players ), SVCF_ONLYTHISCLIENT );

		sprintf( szString, "\\cCWhite flag taken!" );
		V_ColorizeString( szString );
		SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS, 0, 0, CR_WHITE, 3.0f, 0.25f, "BigFont", false, MAKE_ID('C','N','T','R'), ULONG( pToucher->player - players ), SVCF_SKIPTHISCLIENT );
	}

	// [BC] Rivecoder's "held by" messages.
	ulPlayer = ULONG( pToucher->player - players );
	sprintf( szName, "%s", players[ulPlayer].userinfo.GetName() );
	V_RemoveColorCodes( szName );

	sprintf( szString, "\\ccHeld by: \\c%c%s", V_GetColorChar( TEAM_GetTextColor( players[ulPlayer].ulTeam )), szName );
	V_ColorizeString( szString );

	// Now, print it.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		if (( pToucher->player - players ) != consoleplayer )
		{
			pMsg = new DHUDMessageFadeOut( SmallFont, szString,
				1.5f,
				TEAM_MESSAGE_Y_AXIS_SUB,
				0,
				0,
				CR_UNTRANSLATED,
				3.0f,
				0.25f );
			StatusBar->AttachMessage( pMsg, MAKE_ID( 'S','U','B','S' ));
		}
	}
	// If necessary, send it to clients.
	else
		SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS_SUB, 0, 0, CR_UNTRANSLATED, 3.0f, 0.25f, "SmallFont", false, MAKE_ID('S','U','B','S'), ULONG( pToucher->player - players ), SVCF_SKIPTHISCLIENT  );
}

//===========================================================================
//
// AWhiteFlag :: MarkFlagTaken
//
// Signal to the team module whether or not this flag has been taken.
//
//===========================================================================

void AWhiteFlag::MarkFlagTaken( bool bTaken )
{
	TEAM_SetWhiteFlagTaken( bTaken );
}

//===========================================================================
//
// AWhiteFlag :: ResetReturnTicks
//
// Reset the return ticks for the team associated with this flag.
//
//===========================================================================

void AWhiteFlag::ResetReturnTicks( void )
{
	TEAM_SetReturnTicks( teams.Size( ), 0 );
}

//===========================================================================
//
// AWhiteFlag :: ReturnFlag
//
// Spawn a new flag at its original location.
//
//===========================================================================

void AWhiteFlag::ReturnFlag( AActor *pReturner )
{
	POS_t	WhiteFlagOrigin;
	AActor	*pActor;

	// Respawn the white flag.
	WhiteFlagOrigin = TEAM_GetWhiteFlagOrigin( );
	pActor = Spawn( this->GetClass( ), WhiteFlagOrigin.x, WhiteFlagOrigin.y, WhiteFlagOrigin.z, NO_REPLACE );

	// If we're the server, tell clients to spawn the new skull.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( pActor ))
		SERVERCOMMANDS_SpawnThing( pActor );

	// Since all inventory spawns with the MF_DROPPED flag, we need to unset it.
	if ( pActor )
		pActor->flags &= ~MF_DROPPED;

	// Mark the white flag as no longer being taken.
	TEAM_SetWhiteFlagTaken( false );
}

//===========================================================================
//
// AWhiteFlag :: AnnounceFlagReturn
//
// Play the announcer sound for this flag being returned.
//
//===========================================================================

void AWhiteFlag::AnnounceFlagReturn( void )
{
	ANNOUNCER_PlayEntry( cl_announcer, "WhiteFlagReturned" );
}

//===========================================================================
//
// AWhiteFlag :: DisplayFlagReturn
//
// Display the text for this flag being returned.
//
//===========================================================================

void AWhiteFlag::DisplayFlagReturn( void )
{
	char						szString[256];

	// Create the "returned" message.
	sprintf( szString, "\\cCWhite flag returned" );
	V_ColorizeString( szString );

	GAMEMODE_DisplayCNTRMessage( szString, false );
}

// Skulltag skull -----------------------------------------------------------

IMPLEMENT_CLASS( ASkull )

//===========================================================================
//
// ASkull :: AllowFlagPickup
//
// Determine whether or not we should be allowed to pickup this flag.
//
//===========================================================================

LONG ASkull::AllowFlagPickup( AActor *pToucher )
{
	return Super::AllowFlagPickup( pToucher );
}

//===========================================================================
//
// ASkull :: AnnounceFlagPickup
//
// Play the announcer sound for picking up this flag.
//
//===========================================================================

void ASkull::AnnounceFlagPickup( AActor *pToucher )
{
	// Don't announce the pickup if the flag is being given to someone as part of a snapshot.
	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) && ( CLIENT_GetConnectionState( ) == CTS_RECEIVINGSNAPSHOT ))
		return;

	// Build the message.
	// Whatever the team's name is, is the first part of the message. For example:
	// if the "blue" team has been defined then the message will be "BlueSkullTaken".
	// This way we don't have to change every announcer to use a new system. 
	FString name;
	name += TEAM_GetName( TEAM_GetTeamFromItem( this ));
	name += "SkullTaken";
	ANNOUNCER_PlayEntry( cl_announcer, name.GetChars( ));
}

//===========================================================================
//
// ASkull :: DisplayFlagTaken
//
// Display the text for picking up this flag.
//
//===========================================================================

void ASkull::DisplayFlagTaken( AActor *pToucher )
{
	char				szString[256];
	DHUDMessageFadeOut	*pMsg;

	// Create the "pickup" message.
	if (( pToucher->player - players ) == consoleplayer )
		sprintf( szString, "\\c%cYou have the %s skull!", V_GetColorChar( TEAM_GetTextColor( TEAM_GetTeamFromItem( this ))), TEAM_GetName( TEAM_GetTeamFromItem( this )));
	else
		sprintf( szString, "\\c%c%s skull taken!", V_GetColorChar( TEAM_GetTextColor( TEAM_GetTeamFromItem( this ))), TEAM_GetName( TEAM_GetTeamFromItem( this )));

	V_ColorizeString( szString );

	// Now, print it.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		pMsg = new DHUDMessageFadeOut( BigFont, szString,
			1.5f,
			TEAM_MESSAGE_Y_AXIS,
			0,
			0,
			CR_UNTRANSLATED,
			3.0f,
			0.25f );
		StatusBar->AttachMessage( pMsg, MAKE_ID( 'C','N','T','R' ));
	}
	// If necessary, send it to clients.
	else
	{
		sprintf( szString, "\\c%cYou have the %s skull!", V_GetColorChar( TEAM_GetTextColor( TEAM_GetTeamFromItem( this ))), TEAM_GetName( TEAM_GetTeamFromItem( this )));
		V_ColorizeString( szString );
		SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS, 0, 0, CR_UNTRANSLATED, 3.0f, 0.25f, "BigFont", false, MAKE_ID('C','N','T','R'), ULONG( pToucher->player - players ), SVCF_ONLYTHISCLIENT );

		sprintf( szString, "\\c%c%s skull taken!", V_GetColorChar( TEAM_GetTextColor( TEAM_GetTeamFromItem( this ))), TEAM_GetName( TEAM_GetTeamFromItem( this )));
		V_ColorizeString( szString );
		SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS, 0, 0, CR_UNTRANSLATED, 3.0f, 0.25f, "BigFont", false, MAKE_ID('C','N','T','R'), ULONG( pToucher->player - players ), SVCF_SKIPTHISCLIENT );
	}

	// [RC] Create the "held by" message for this team.
	ULONG playerIndex = ULONG( pToucher->player - players );
	sprintf( szString, "\\c%cHeld by: %s", V_GetColorChar( TEAM_GetTextColor( players[playerIndex].ulTeam )), players[playerIndex].userinfo.GetName());

	V_ColorizeString( szString );

	// Now, print it.
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		if (( pToucher->player - players ) != consoleplayer )
		{
			pMsg = new DHUDMessageFadeOut( SmallFont, szString,
				1.5f,
				TEAM_MESSAGE_Y_AXIS_SUB,
				0,
				0,
				CR_UNTRANSLATED,
				3.0f,
				0.25f );
			StatusBar->AttachMessage( pMsg, MAKE_ID( 'S','U','B','S' ));
		}
	}
	// If necessary, send it to clients.
	else
	{
		SERVERCOMMANDS_PrintHUDMessageFadeOut( szString, 1.5f, TEAM_MESSAGE_Y_AXIS_SUB, 0, 0, CR_UNTRANSLATED, 3.0f, 0.25f, "SmallFont", false, MAKE_ID('S','U','B','S'), ULONG( pToucher->player - players ), SVCF_SKIPTHISCLIENT  );
		SERVER_Printf( PRINT_MEDIUM, "%s \\c-has taken the \\c%c%s \\c-skull.\n", players[playerIndex].userinfo.GetName(), V_GetColorChar( TEAM_GetTextColor( TEAM_GetTeamFromItem( this ))), TEAM_GetName( TEAM_GetTeamFromItem( this )));
	}
}

//===========================================================================
//
// ASkull :: MarkFlagTaken
//
// Signal to the team module whether or not this flag has been taken.
//
//===========================================================================

void ASkull::MarkFlagTaken( bool bTaken )
{
	Super::MarkFlagTaken ( bTaken );
}

//===========================================================================
//
// ASkull :: ResetReturnTicks
//
// Reset the return ticks for the team associated with this flag.
//
//===========================================================================

void ASkull::ResetReturnTicks( void )
{
	TEAM_SetReturnTicks( TEAM_GetTeamFromItem( this ), 0 );
}

//===========================================================================
//
// ASkull :: ReturnFlag
//
// Spawn a new flag at its original location.
//
//===========================================================================

void ASkull::ReturnFlag( AActor *pReturner )
{
	POS_t	SkullOrigin;
	ASkull	*pActor;

	// Respawn the skull.
	SkullOrigin = TEAM_GetItemOrigin( TEAM_GetTeamFromItem( this ));

	pActor = (ASkull *)Spawn( this->GetClass( ), SkullOrigin.x, SkullOrigin.y, SkullOrigin.z, NO_REPLACE );

	// If we're the server, tell clients to spawn the new skull.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( pActor ))
		SERVERCOMMANDS_SpawnThing( pActor );

	// Since all inventory spawns with the MF_DROPPED flag, we need to unset it.
	if ( pActor )
		pActor->flags &= ~MF_DROPPED;

	ULONG ulItemTeam = TEAM_GetTeamFromItem( this );
	// Mark the flag as no longer being taken.
	TEAM_SetItemTaken( ulItemTeam, false );

	// If an opposing team's flag has been taken by one of the team members of the returner
	// the player who returned this flag has the chance to earn an "Assist!" medal.
	if ( pReturner && pReturner->player )
	{
		for ( ULONG i = 0; i < MAXPLAYERS; i++ )
		{
			if ( ( players[i].ulTeam == ulItemTeam )
			     && TEAM_FindOpposingTeamsItemInPlayersInventory ( &players[i] ) )
			{
				TEAM_SetAssistPlayer( pReturner->player->ulTeam, ULONG( pReturner->player - players ));
			}
		}
	}

	char szString[256];
	if ( pReturner && pReturner->player )
	{
		// [RC] Create the "returned by" message for this team.
		ULONG playerIndex = ULONG( pReturner->player - players );
		sprintf( szString, "\\c%cReturned by: %s", V_GetColorChar( TEAM_GetTextColor( players[playerIndex].ulTeam )), players[playerIndex].userinfo.GetName() );
	}
	else
	{
		// [RC] Create the "returned automatically" message for this team.
		sprintf( szString, "\\c%cReturned automatically.", V_GetColorChar( TEAM_GetTextColor( TEAM_GetTeamFromItem( this ))));
	}

	V_ColorizeString( szString );
	GAMEMODE_DisplaySUBSMessage( szString, true );

	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		if ( pReturner && pReturner->player )
			SERVER_Printf( PRINT_MEDIUM, "%s \\c-returned the \\c%c%s \\c-skull.\n", pReturner->player->userinfo.GetName(), V_GetColorChar( TEAM_GetTextColor( TEAM_GetTeamFromItem( this ))), TEAM_GetName( TEAM_GetTeamFromItem( this )));
		else
			SERVER_Printf( PRINT_MEDIUM, "\\c%c%s \\c-skull returned.\n", V_GetColorChar( TEAM_GetTextColor( TEAM_GetTeamFromItem( this ))), TEAM_GetName( TEAM_GetTeamFromItem( this )));
	}
}

//===========================================================================
//
// ASkull :: AnnounceFlagReturn
//
// Play the announcer sound for this flag being returned.
//
//===========================================================================

void ASkull::AnnounceFlagReturn( void )
{
	// Build the message.
	// Whatever the team's name is, is the first part of the message. For example:
	// if the "blue" team has been defined then the message will be "BlueSkullReturned".
	// This way we don't have to change every announcer to use a new system. 
	FString name;
	name += TEAM_GetName( TEAM_GetTeamFromItem( this ));
	name += "SkullReturned";
	ANNOUNCER_PlayEntry( cl_announcer, name.GetChars( ));
}

//===========================================================================
//
// ASkull :: DisplayFlagReturn
//
// Display the text for this flag being returned.
//
//===========================================================================

void ASkull::DisplayFlagReturn( void )
{
	char						szString[256];

	// Create the "returned" message.
	sprintf( szString, "\\c%c%s skull returned", V_GetColorChar( TEAM_GetTextColor( TEAM_GetTeamFromItem( this ))), TEAM_GetName( TEAM_GetTeamFromItem( this )));

	V_ColorizeString( szString );

	GAMEMODE_DisplayCNTRMessage( szString, false );
}
