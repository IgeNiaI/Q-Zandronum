//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2003 Brad Carney
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
// Filename: chat.cpp
//
// Description: Contains chat routines
//
//-----------------------------------------------------------------------------

#include "botcommands.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "cl_commands.h"
#include "cl_demo.h"
#include "cl_main.h"
#include "chat.h"
#include "doomstat.h"
#include "d_gui.h"
#include "d_net.h"
#include "deathmatch.h"
#include "gi.h"
#include "gamemode.h"
#include "hu_stuff.h"
#include "i_input.h"
#include "d_gui.h"
#include "d_player.h"
#include "network.h"
#include "s_sound.h"
#include "scoreboard.h"
#include "st_stuff.h"
//#include "sv_main.h"
#include "team.h"
#include "v_text.h"
#include "v_video.h"
#include "w_wad.h"
#include "sbar.h"
#include "st_hud.h"
#include "sectinfo.h"
#include "g_level.h"
#include "p_acs.h"

//*****************************************************************************
//
// [TP] ChatBuffer
//
// Encapsulates the storage for the message in the chat input line.
//
class ChatBuffer
{
public:
	enum { MaxMessages = 10 + 1 };
	ChatBuffer();

	void BeginNewMessage();
	void Clear();
	FString &GetEditableMessage();
	const FString &GetMessage() const;
	int GetPosition() const;
	int Length() const;
	void Insert( char character );
	void Insert( const char *text );
	bool IsInArchive() const;
	void MoveCursor( int offset );
	void MoveCursorTo( int position );
	void MoveInArchive( int offset );
	void RemoveCharacter( bool forward );
	void RemoveRange( int start, int end );
	void ResetTabCompletion();
	void PasteChat( const char *clip );
	void SetCurrentMessage( int position );
	void TabComplete();

	const char &operator[]( int position ) const;

private:
	TArray<FString> Messages; // List of messages stored. Last one is the current message being edited.
	int MessagePosition; // Index in Messages that the user is currently using.
	int CursorPosition; // Position of the cursor within the message.
	int TabCompletionPlayerOffset; // Player the tab completion routine should look at.
	int TabCompletionStart; // Position of the message where current tab completion begins.
	bool IsInTabCompletion; // Is tab completion routine currently active?
	FString TabCompletionWord; // The word being tab completed.
};

//*****************************************************************************
//	VARIABLES

static	ChatBuffer g_ChatBuffer;
static	ULONG	g_ulChatMode;

//*****************************************************************************
//	CONSOLE VARIABLES

CVAR( String, chatmacro1, "I'm ready to kick butt!", CVAR_ARCHIVE )
CVAR( String, chatmacro2, "I'm OK.", CVAR_ARCHIVE )
CVAR( String, chatmacro3, "I'm not looking too good!", CVAR_ARCHIVE )
CVAR( String, chatmacro4, "Help!", CVAR_ARCHIVE )
CVAR( String, chatmacro5, "You suck!", CVAR_ARCHIVE )
CVAR( String, chatmacro6, "Next time, scumbag...", CVAR_ARCHIVE )
CVAR( String, chatmacro7, "Come here!", CVAR_ARCHIVE )
CVAR( String, chatmacro8, "I'll take care of it.", CVAR_ARCHIVE )
CVAR( String, chatmacro9, "Yes", CVAR_ARCHIVE )
CVAR( String, chatmacro0, "No", CVAR_ARCHIVE )

// [CW]
CVAR( Bool, chat_substitution, false, CVAR_ARCHIVE )

EXTERN_CVAR( Int, con_colorinmessages );
// [RC] Played when a chat message arrives. Values: off, default, Doom 1 (dstink), Doom 2 (dsradio).
CVAR (Int, chat_sound, 1, CVAR_ARCHIVE)

//*****************************************************************************
FStringCVar	*g_ChatMacros[10] =
{
	&chatmacro0,
	&chatmacro1,
	&chatmacro2,
	&chatmacro3,
	&chatmacro4,
	&chatmacro5,
	&chatmacro6,
	&chatmacro7,
	&chatmacro8,
	&chatmacro9
};

//*****************************************************************************
//	PROTOTYPES

void	chat_SetChatMode( ULONG ulMode );
void	chat_SendMessage( ULONG ulMode, const char *pszString );
void	chat_GetIgnoredPlayers( FString &Destination ); // [RC]
void	chat_DoSubstitution( FString &Input ); // [CW]

//*****************************************************************************
//	FUNCTIONS
ChatBuffer::ChatBuffer() :
    MessagePosition( 0 ),
    CursorPosition( 0 ),
    TabCompletionPlayerOffset( 0 ),
    TabCompletionStart( 0 )
{
	Messages.Push( "" );
}

//*****************************************************************************
//
// [TP] Returns the message the user is currently looking at, for viewing purposes.
//
const FString &ChatBuffer::GetMessage() const
{
	return Messages[MessagePosition];
}

//*****************************************************************************
//
// [TP] Returns the current message to be edited. Only edit the current chat message through this function,
//      as it ensures the user sends the message he intends to, and that the archive won't be edited!
//      Never edit the elements of Messages directly! If you do not intend to edit this string, use
//      GetMessage() instead, as this has the side effect of unwinding the archive.
//
FString &ChatBuffer::GetEditableMessage()
{
	// We're about to edit the message, so tab completion needs to reset, unless tab completion is causing the edit.
	if ( IsInTabCompletion == false )
		ResetTabCompletion();

	if ( IsInArchive() )
	{
		// If we're not already using the newest message, make the current message the newest, and then use it.
		Messages.Last() = GetMessage();
		MessagePosition = Messages.Size() - 1;
		CursorPosition = clamp( CursorPosition, 0, static_cast<signed>( GetMessage().Len() ));
	}

	// The user should be out of the archive now.
	assert( &GetMessage() == &Messages.Last() );
	return Messages.Last();
}

//*****************************************************************************
//
void ChatBuffer::Insert( char character )
{
	char text[2] = { character, '\0' };
	Insert( text );
}

//*****************************************************************************
//
void ChatBuffer::Insert( const char *text )
{
	FString &message = GetEditableMessage();
	message.Insert( GetPosition(), text );

	if ( message.Len() > MAX_CHATBUFFER_LENGTH )
		message.Truncate( MAX_CHATBUFFER_LENGTH );

	MoveCursor( strlen( text ));
}

//*****************************************************************************
//
void ChatBuffer::RemoveCharacter( bool forward )
{
	int deletePosition = GetPosition();
	FString &message = GetEditableMessage();

	if ( forward == false )
		deletePosition--;

	if ( message.IsNotEmpty() && ( deletePosition >= 0 ) && ( deletePosition < Length() ))
	{
		char *messageBuffer = message.LockBuffer();

		// Move all characters from the cursor position to the end of string back by one.
		for ( int i = deletePosition; i < Length() - 1; ++i )
			messageBuffer[i] = messageBuffer[i + 1];

		// Remove the last character.
		message.UnlockBuffer();
		message.Truncate( Length() - 1 );

		if ( forward == false )
			MoveCursor( -1 );
	}
}

//*****************************************************************************
//
// [TP] Removes all characters from 'start' to 'end', including 'start' but not 'end'.
//
void ChatBuffer::RemoveRange( int start, int end )
{
	FString &message = GetEditableMessage();
	message = message.Mid( 0, start ) + message.Mid( end );

	// Move the cursor if appropriate.
	if ( CursorPosition >= start )
	{
		CursorPosition -= end - start;

		// Don't move it in front of 'start', though, in case the cursor is inside the range.
		CursorPosition = MAX( CursorPosition, start );
	}
}

//*****************************************************************************
//
void ChatBuffer::Clear()
{
	MessagePosition = Messages.Size() - 1;
	MoveCursorTo( 0 );
	GetEditableMessage() = "";
}

//*****************************************************************************
//
int ChatBuffer::Length() const
{
	return GetMessage().Len();
}

//*****************************************************************************
//
const char &ChatBuffer::operator[]( int position ) const
{
	return GetMessage()[position];
}

//*****************************************************************************
//
int ChatBuffer::GetPosition() const
{
	return CursorPosition;
}

//*****************************************************************************
//
void ChatBuffer::MoveCursor( int offset )
{
	MoveCursorTo( GetPosition() + offset );
}

//*****************************************************************************
//
void ChatBuffer::MoveCursorTo( int position )
{
	CursorPosition = clamp( position, 0, Length() );

	if ( IsInTabCompletion == false )
		ResetTabCompletion();
}

//*****************************************************************************
//
void ChatBuffer::MoveInArchive( int offset )
{
	SetCurrentMessage( MessagePosition + offset );
}

//*****************************************************************************
//
void ChatBuffer::SetCurrentMessage( int position )
{
	MessagePosition = clamp( position, 0, static_cast<signed>( Messages.Size() - 1 ));
	MoveCursorTo( GetMessage().Len() );
}

//*****************************************************************************
//
// [BB] From ZDoom
void ChatBuffer::PasteChat(const char *clip)
{
	if (clip != NULL && *clip != '\0')
	{
		// Only paste the first line.
		while (*clip != '\0')
		{
			if (*clip == '\n' || *clip == '\r' || *clip == '\b')
			{
				break;
			}
			Insert( *clip++ );
		}
	}
}

//*****************************************************************************
//
void ChatBuffer::BeginNewMessage()
{
	// Only begin a new message if we don't already have a cleared message buffer.
	if ( GetMessage().IsNotEmpty() )
	{
		// If we re-sent something from the archive, store it into the current chat line. This way it is copied and becomes the most recent
		// message again.
		if ( &GetMessage() != &Messages.Last() )
			Messages.Last() = GetMessage();

		// Put a new empty string to be our current message, but avoid side-by-side duplicate entries in history: if the current buffer is
		// the same as the most recent entry in history (Messages[Messages.Size() - 2]), then we just clear the input buffer and don't push
		// anything.
		if (( Messages.Size() >= 2 ) && ( Messages.Last().CompareNoCase( Messages[Messages.Size() - 2] ) == 0 ))
			Messages.Last() = "";
		else
			Messages.Push( "" );

		// If there now are too many messages, drop some from the archive.
		while ( Messages.Size() > MaxMessages )
			Messages.Delete( 0 );

		// Select the newly created message.
		MessagePosition = Messages.Size() - 1;
		MoveCursorTo( 0 );
		ResetTabCompletion();
	}
}

//*****************************************************************************
//
bool ChatBuffer::IsInArchive() const
{
	return &GetMessage() != &Messages.Last();
}

//*****************************************************************************
//
void ChatBuffer::TabComplete()
{
	IsInTabCompletion = true;
	FString message = GetMessage();

	// If we do not yet have a word to tab complete, then find one now.
	// Only consider doing tab completion if there is no graphic character in front of the cursor.
	if ( TabCompletionWord.IsEmpty() && ( isgraph( message[GetPosition()] ) == false ))
	{
		TabCompletionStart = GetPosition();

		// Where's the beginning of the word?
		while (( TabCompletionStart > 0 ) && ( isspace( message[TabCompletionStart - 1] ) == false ))
			TabCompletionStart--;

		TabCompletionPlayerOffset = 0;
		TabCompletionWord = message.Mid( TabCompletionStart, CursorPosition - TabCompletionStart );
	}

	// If we now have a word to tab complete, proceed to try to complete it.
	if ( TabCompletionWord.IsNotEmpty() )
	{
		// Go through each player, and compare our string with their names.
		for ( int i = 0; i < MAXPLAYERS; ++i )
		{
			player_t &player = players[( i + TabCompletionPlayerOffset ) % MAXPLAYERS];

			if ( PLAYER_IsValidPlayer( &player - players ))
			{
				FString name = player.userinfo.GetName();
				V_RemoveColorCodes( name );

				// See if this player's name begins with our tab completion word.
				if ( strnicmp( name, TabCompletionWord, TabCompletionWord.Len() ) == 0 )
				{
					// Do the completion.
					RemoveRange( TabCompletionStart, GetPosition() );
					Insert( name );

					// Remove any whitespace from in front of the cursor.
					while ( isspace( GetMessage()[GetPosition()] ))
						RemoveCharacter( true );

					// Now add the suffix.
					Insert(( TabCompletionStart == 0 ) ? ": " : " " );

					// Mark down where to start looking next time. This allows us to cycle through player names.
					TabCompletionPlayerOffset = ( &player - players ) + 1;
					TabCompletionPlayerOffset %= MAXPLAYERS;
					break;
				}
			}
		}
	}

	IsInTabCompletion = false;
}

//*****************************************************************************
//
void ChatBuffer::ResetTabCompletion()
{
	TabCompletionPlayerOffset = 0;
	TabCompletionStart = 0;
	TabCompletionWord = "";
}

//*****************************************************************************
//
void CHAT_Construct( void )
{
	// Initialize the chat mode.
	g_ulChatMode = CHATMODE_NONE;

	// Clear out the chat buffer.
	g_ChatBuffer.Clear();
}

//*****************************************************************************
//
void CHAT_Tick( void )
{
	// Check the chat ignore timers.
	for ( ULONG i = 0; i < MAXPLAYERS; i++ )
	{
		// [BB] Nothing to do for players that are not in the game.
		if ( playeringame[i] == false )
			continue;

		// Decrement this player's timer.
		if ( players[i].bIgnoreChat && ( players[i].lIgnoreChatTicks > 0 ))
			players[i].lIgnoreChatTicks--;

		// Is it time to un-ignore him?
		if ( players[i].lIgnoreChatTicks == 0 )
		{
			players[i].bIgnoreChat = false;
			// [BB] The player is unignored indefinitely. If we wouldn't do this,
			// bIgnoreChat would be set to false every tic once lIgnoreChatTicks reaches 0.
			players[i].lIgnoreChatTicks = -1;
		}
	}
}

//*****************************************************************************
//
bool CHAT_Input( event_t *pEvent )
{
	if ( pEvent->type != EV_GUI_Event )
		return ( false );

	// Server doesn't use this at all.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return ( false );

	// Determine to do with our keypress.
	if ( g_ulChatMode != CHATMODE_NONE )
	{
		if ( pEvent->subtype == EV_GUI_KeyDown || pEvent->subtype == EV_GUI_KeyRepeat )
		{
			if ( pEvent->data1 == '\r' )
			{
				chat_SendMessage( g_ulChatMode, g_ChatBuffer.GetMessage() );
				chat_SetChatMode( CHATMODE_NONE );
				return ( true );
			}
			else if ( pEvent->data1 == GK_ESCAPE )
			{
				chat_SetChatMode( CHATMODE_NONE );
				return ( true );
			}
			else if ( pEvent->data1 == '\b' )
			{
				g_ChatBuffer.RemoveCharacter( false );
				return ( true );
			}
			else if ( pEvent->data1 == GK_DEL )
			{
				g_ChatBuffer.RemoveCharacter( true );
				return ( true );
			}
			// Ctrl+C. 
			else if ( pEvent->data1 == 'C' && ( pEvent->data3 & GKM_CTRL ))
			{
				I_PutInClipboard( g_ChatBuffer.GetMessage() );
				return ( true );
			}
			// Ctrl+V.
			else if ( pEvent->data1 == 'V' && ( pEvent->data3 & GKM_CTRL ))
			{
				g_ChatBuffer.PasteChat( I_GetFromClipboard( false ));
				return ( true );
			}
			// Arrow keys
			else if ( pEvent->data1 == GK_LEFT )
			{
				g_ChatBuffer.MoveCursor( -1 );
				return ( true );
			}
			else if ( pEvent->data1 == GK_RIGHT )
			{
				g_ChatBuffer.MoveCursor( 1 );
				return ( true );
			}
			else if ( pEvent->data1 == GK_UP )
			{
				g_ChatBuffer.MoveInArchive( -1 );
				return ( true );
			}
			else if ( pEvent->data1 == GK_DOWN )
			{
				g_ChatBuffer.MoveInArchive( 1 );
				return ( true );
			}
			// Home
			else if ( pEvent->data1 == GK_HOME )
			{
				g_ChatBuffer.MoveCursorTo( 0 );
				return ( true );
			}
			// End
			else if ( pEvent->data1 == GK_END )
			{
				g_ChatBuffer.MoveCursorTo( g_ChatBuffer.Length() );
				return ( true );
			}
			else if ( pEvent->data1 == '\t' )
			{
				g_ChatBuffer.TabComplete();
				return ( true );
			}
		}
		else if ( pEvent->subtype == EV_GUI_Char )
		{
			// Send a macro.
			if ( pEvent->data2 && (( pEvent->data1 >= '0' ) && ( pEvent->data1 <= '9' )))
			{
				chat_SendMessage( g_ulChatMode, *g_ChatMacros[pEvent->data1 - '0'] );
				chat_SetChatMode( CHATMODE_NONE );
			}
			else
				g_ChatBuffer.Insert( static_cast<char> ( pEvent->data1 ) );

			return ( true );
		}
#ifdef __unix__
		else if (pEvent->subtype == EV_GUI_MButtonDown)
		{
			g_ChatBuffer.PasteChat( I_GetFromClipboard( true ));
		}
#endif
	}

	return ( false );
}

//*****************************************************************************
//
void CHAT_Render( void )
{
	static const char *prompt = "SAY: ";
	static const char *cursor = gameinfo.gametype == GAME_Doom ? "_" : "[";
	bool scale = ( con_scaletext ) && ( con_virtualwidth > 0 ) && ( con_virtualheight > 0 );
	float scaleX = 1.0f;
	float scaleY = 1.0f;
	int positionY = ( gamestate == GS_INTERMISSION ) ? SCREENHEIGHT : ST_Y;

	if ( g_ulChatMode == CHATMODE_NONE )
		return;

	if ( scale )
	{
		scaleX = static_cast<float>( *con_virtualwidth ) / SCREENWIDTH;
		scaleY = static_cast<float>( *con_virtualheight ) / SCREENHEIGHT;
		positionY = positionY - Scale( SCREENHEIGHT, SmallFont->GetHeight() + 1, con_virtualheight ) + 1;
		positionY = static_cast<int> ( positionY * scaleY );
	}
	else
	{
		positionY = positionY - SmallFont->GetHeight() + 1;
	}

	int chatWidth = static_cast<int> ( SCREENWIDTH * scaleX );
	chatWidth -= static_cast<int> ( round( SmallFont->GetCharWidth( '_' ) * scaleX * 2 + SmallFont->StringWidth( prompt )) );

	// Build the message that we will display to clients.
	FString displayString = g_ChatBuffer.GetMessage();
	// Insert the cursor string into the message.
	displayString = displayString.Mid( 0, g_ChatBuffer.GetPosition() ) + cursor + displayString.Mid( g_ChatBuffer.GetPosition() );
	EColorRange promptColor = CR_GREEN;
	EColorRange messageColor = CR_GRAY;

	// Use different colors in team chat.
	if ( g_ulChatMode == CHATMODE_TEAM )
	{
		promptColor = CR_GREY;
		messageColor = static_cast<EColorRange>( TEAM_GetTextColor( players[consoleplayer].ulTeam ));
	}

	// [TP] If we're currently viewing the archive, use a different color
	if ( g_ChatBuffer.IsInArchive() )
		messageColor = CR_DARKGRAY;

	// Render the chat string.
	HUD_DrawText( SmallFont, promptColor, 0, positionY, prompt );

	if ( SmallFont->StringWidth( displayString ) > chatWidth )
	{
		// Break it onto multiple lines, if necessary.
		const BYTE *bytes = reinterpret_cast<const BYTE*>( displayString.GetChars() );
		FBrokenLines *lines = V_BreakLines( SmallFont, chatWidth, bytes );
		int messageY = positionY;

		for ( int i = 0; lines[i].Width != -1; ++i )
		{
			HUD_DrawText( SmallFont, messageColor, SmallFont->StringWidth( prompt ), messageY, lines[i].Text );
			messageY += SmallFont->GetHeight();
		}

		V_FreeBrokenLines( lines );
	}
	else
	{
		HUD_DrawText( SmallFont, messageColor, SmallFont->StringWidth( prompt ), positionY, displayString );
	}

	// [RC] Tell chatters about the iron curtain of LMS chat.
	if ( GAMEMODE_AreSpectatorsFordiddenToChatToPlayers() )
	{
		FString note;

		// Is this the spectator talking?
		if ( players[consoleplayer].bSpectating )
			note = "\\cdNOTE: \\ccPlayers cannot hear you chat";
		else
			note = "\\cdNOTE: \\ccSpectators cannot talk to you";

		V_ColorizeString( note );
		HUD_DrawText( SmallFont, CR_UNTRANSLATED,
			(LONG)(( ( scale ? *con_virtualwidth : SCREENWIDTH )/ 2 ) - ( SmallFont->StringWidth( note ) / 2 )),
			(LONG)(( positionY * scaleY ) - ( SmallFont->GetHeight( ) * 2 ) + 1 ),
			note );
	}

	BorderTopRefresh = screen->GetPageCount( );
}

//*****************************************************************************
//
ULONG CHAT_GetChatMode( void )
{
	return ( g_ulChatMode );
}

//*****************************************************************************
//
void CHAT_PrintChatString( ULONG ulPlayer, ULONG ulMode, const char *pszString )
{
	ULONG		ulChatLevel = 0;
	FString		OutString;
	FString		ChatString;

	// [RC] Are we ignoring this player?
	if (( ulPlayer != MAXPLAYERS ) && players[ulPlayer].bIgnoreChat )
		return;

	// If ulPlayer == MAXPLAYERS, it is the server talking.
	if ( ulPlayer == MAXPLAYERS )
	{
		// Special support for "/me" commands.
		ulChatLevel = PRINT_HIGH;
		if ( strnicmp( "/me", pszString, 3 ) == 0 )
		{
			pszString += 3;
			OutString = "* <server>";
		}
		else
			OutString = "<server>: ";
	}
	else if ( ulMode == CHATMODE_GLOBAL )
	{
		ulChatLevel = PRINT_CHAT;

		// Special support for "/me" commands.
		if ( strnicmp( "/me", pszString, 3 ) == 0 )
		{
			ulChatLevel = PRINT_HIGH;
			pszString += 3;
			OutString.AppendFormat( "* %s\\cc", players[ulPlayer].userinfo.GetName() );
		}
		else
		{
			OutString.AppendFormat( "%s" TEXTCOLOR_CHAT ": ", players[ulPlayer].userinfo.GetName() );
		}
	}
	else if ( ulMode == CHATMODE_TEAM )
	{
		ulChatLevel = PRINT_TEAMCHAT;
		if ( PLAYER_IsTrueSpectator ( &players[consoleplayer] ) )
			OutString += "<SPEC> ";
		else
		{
			OutString = "\\c";
			OutString += V_GetColorChar( TEAM_GetTextColor( players[consoleplayer].ulTeam ));
			OutString += "<TEAM> ";
		}

		// Special support for "/me" commands.
		if ( strnicmp( "/me", pszString, 3 ) == 0 )
		{
			ulChatLevel = PRINT_HIGH;
			pszString += 3;
			OutString.AppendFormat( "\\cc* %s\\cc", players[ulPlayer].userinfo.GetName() );
		}
		else
		{
			OutString.AppendFormat( "\\cd%s" TEXTCOLOR_TEAMCHAT ": ", players[ulPlayer].userinfo.GetName() );
		}
	}

	ChatString = pszString;

	// [RC] Remove linebreaks and other escape codes from chat.
	ChatString.Substitute("\\", "\\\\");

	// [RC] ...but allow chat colors.
	ChatString.Substitute("\\\\c", "\\c");

	// [BB] Remove invalid color codes, those can confuse the printing and create new lines.
	V_RemoveInvalidColorCodes( ChatString );

	// [RC] ...if the user wants them.
	if ( con_colorinmessages == 2)
		V_RemoveColorCodes( ChatString );

	// [BB] Remove any kind of trailing crap.
	V_RemoveTrailingCrapFromFString ( ChatString );

	// [BB] If the chat string is empty now, it only contained crap and is ignored.
	if ( ChatString.IsEmpty() )
		return;

	OutString += ChatString;

	Printf( ulChatLevel, "%s\n", OutString.GetChars() );

	// [BB] If the user doesn't want to see the messages, they shouldn't make a sound.
	if ( show_messages )
	{
		// [RC] User can choose the chat sound.
		if ( chat_sound == 1 ) // Default
			S_Sound( CHAN_VOICE | CHAN_UI, gameinfo.chatSound, 1, ATTN_NONE );
		else if ( chat_sound == 2 ) // Doom 1
			S_Sound( CHAN_VOICE | CHAN_UI, "misc/chat2", 1, ATTN_NONE );
		else if ( chat_sound == 3 ) // Doom 2
			S_Sound( CHAN_VOICE | CHAN_UI, "misc/chat", 1, ATTN_NONE );
	}

	BOTCMD_SetLastChatString( pszString );
	BOTCMD_SetLastChatPlayer( PLAYER_IsValidPlayer( ulPlayer ) ? players[ulPlayer].userinfo.GetName() : "" );

	{
		ULONG	ulIdx;

		for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			if ( playeringame[ulIdx] == false )
				continue;

			// Don't tell the bot someone talked if it was it who talked.
			if ( ulIdx == ulPlayer )
				continue;

			// If this is a bot, tell it a player said something.
			if ( players[ulIdx].pSkullBot )
				players[ulIdx].pSkullBot->PostEvent( BOTEVENT_PLAYER_SAY );
		}
	}
}

//*****************************************************************************
//*****************************************************************************
//
void chat_SetChatMode( ULONG ulMode )
{
	if ( ulMode < NUM_CHATMODES )
	{
		player_t	*pPlayer = &players[consoleplayer];

		g_ulChatMode = ulMode;

		if ( ulMode != CHATMODE_NONE )
		{
			pPlayer->bChatting = true;

			// Tell the server we're beginning to chat.
			if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
				CLIENTCOMMANDS_StartChat( );
		}
		else
		{
			pPlayer->bChatting = false;

			// Tell the server we're done chatting.
			if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
				CLIENTCOMMANDS_EndChat( );
		}

	}
}

//*****************************************************************************
//
void chat_SendMessage( ULONG ulMode, const char *pszString )
{
	FString ChatMessage = pszString;

	// [CW] Substitute the message if necessary.
	chat_DoSubstitution( ChatMessage );

	// If we're the client, let the server handle formatting/sending the msg to other players.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		CLIENTCOMMANDS_Say( ulMode, ChatMessage.GetChars( ));
	}
	else if ( demorecording )
	{
		Net_WriteByte( DEM_SAY );
		Net_WriteByte( static_cast<BYTE> ( ulMode ) );
		Net_WriteString( ChatMessage.GetChars( ));
	}
	else
	{
		CHAT_PrintChatString( consoleplayer, ulMode, ChatMessage.GetChars( ));
	}

	// [TP] The message has been sent. Start creating a new one.
	g_ChatBuffer.BeginNewMessage();
}

//*****************************************************************************
//
// [RC] Fills Destination with a list of ignored players.
//
void chat_GetIgnoredPlayers( FString &Destination )
{
	Destination = "";

	// Append all the players' names.
	for ( ULONG i = 0; i < MAXPLAYERS; i++ )
	{
		if ( players[i].bIgnoreChat )
		{
			Destination += players[i].userinfo.GetName();
			Destination += "\\c-";
			
			// Add the time remaining.
			if ( players[i].lIgnoreChatTicks > 0 )
			{
				int iMinutesLeft = static_cast<int>( 1 + players[i].lIgnoreChatTicks / ( MINUTE * TICRATE ));
				Destination.AppendFormat( " (%d minute%s left)", iMinutesLeft, ( iMinutesLeft == 1 ? "" : "s" ));
			}

			Destination += ", ";
		}
	}

	// Remove the last ", ".
	if ( Destination.Len( ) )
		Destination = Destination.Left( Destination.Len( ) - 2 );
}

//*****************************************************************************
//
// [CW]
void chat_DoSubstitution( FString &Input )
{
	player_t *pPlayer = &players[consoleplayer];
	AWeapon *pReadyWeapon = pPlayer->ReadyWeapon;

	if ( chat_substitution )
	{
		FString Output;
		const char *pszString = Input.GetChars( );

		for ( ; *pszString != 0; pszString++ )
		{
			if ( !strncmp( pszString, "$ammocount", 10 ))
			{
				if ( pReadyWeapon && pReadyWeapon->Ammo1 )
				{
					Output.AppendFormat( "%d", pReadyWeapon->Ammo1->Amount );

					if ( pReadyWeapon->Ammo2 )
						Output.AppendFormat( "/%d", pReadyWeapon->Ammo2->Amount );
				}
				else
				{
					Output.AppendFormat( "no ammo" );
				}

				pszString += 9;
			}
			else if ( !strncmp( pszString, "$ammo", 5 ))
			{
				if ( pReadyWeapon && pReadyWeapon->Ammo1 )
				{
					Output.AppendFormat( "%s", pReadyWeapon->Ammo1->GetClass( )->TypeName.GetChars( ) );

					if ( pReadyWeapon->Ammo2 )
					{
						Output.AppendFormat( "/%s", pReadyWeapon->Ammo2->GetClass( )->TypeName.GetChars( ));
					}
				}
				else
				{
					Output.AppendFormat( "no ammo" );
				}

				pszString += 4;
			}
			else if ( !strncmp( pszString, "$armor", 6 ))
			{
				AInventory *pArmor = pPlayer->mo->FindInventory<ABasicArmor>( );
				int iArmorCount = 0;
				
				if ( pArmor )
					iArmorCount = pArmor->Amount;

				Output.AppendFormat( "%d", iArmorCount );

				pszString += 5;
			}
			else if ( !strncmp( pszString, "$health", 7 ))
			{
				Output.AppendFormat ("%d", pPlayer->health);

				pszString += 6;
			}
			else if ( !strncmp( pszString, "$weapon", 7 ))
			{
				if ( pReadyWeapon )
					Output.AppendFormat( "%s", pReadyWeapon->GetClass( )->TypeName.GetChars( ) );
				else
					Output.AppendFormat( "no weapon" );

				pszString += 6;
			}
			else if ( !strncmp( pszString, "$location", 9 ))
			{
				Output += SECTINFO_GetPlayerLocation( consoleplayer );
				pszString += 8;
			}
			else
			{
				Output.AppendCStrPart( pszString, 1 );
			}
		}

		Input = Output;
	}
}

//*****************************************************************************
//	CONSOLE COMMANDS/VARIABLES

CCMD( say )
{
	ULONG		ulIdx;
	FString		ChatString;

	// [BB] Mods are not allowed to say anything in the player's name.
	if ( ACS_IsCalledFromConsoleCommand( ))
		return;

	// [BB] No chatting while playing a demo.
	if ( CLIENTDEMO_IsPlaying( ) == true )
	{
		Printf ( "You can't chat during demo playback.\n" );
		return;
	}

	if ( argv.argc( ) < 2 )
	{
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			return;

		// The message we send will be a global chat message to everyone.
		chat_SetChatMode( CHATMODE_GLOBAL );
		
		// Hide the console.
		C_HideConsole( );

		// Clear out the chat buffer.
		g_ChatBuffer.Clear();
	}
	else
	{
		for ( ulIdx = 1; ulIdx < static_cast<unsigned int>(argv.argc( )); ulIdx++ )
			ChatString.AppendFormat( "%s ", argv[ulIdx] );

		// Send the server's chat string out to clients, and print it in the console.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVER_SendChatMessage( MAXPLAYERS, CHATMODE_GLOBAL, ChatString.GetChars( ));
		else
			// We typed out our message in the console or with a macro. Go ahead and send the message now.
			chat_SendMessage( CHATMODE_GLOBAL, ChatString.GetChars( ));
	}
}

//*****************************************************************************
//
CCMD( say_team )
{
	ULONG		ulIdx;
	FString		ChatString;

	// [BB] Mods are not allowed to say anything in the player's name.
	if ( ACS_IsCalledFromConsoleCommand( ))
		return;

	// [BB] No chatting while playing a demo.
	if ( CLIENTDEMO_IsPlaying( ) == true )
	{
		Printf ( "You can't chat during demo playback.\n" );
		return;
	}

	// Make sure we have teammates to talk to before we use team chat.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
	{
		// Not on a team. No one to talk to.
		if ( ( players[consoleplayer].bOnTeam == false ) && ( PLAYER_IsTrueSpectator ( &players[consoleplayer] ) == false ) )
			return;
	}
	// Not in any team mode. Nothing to do!
	else
		return;

	// [BB] Don't allow dead spectators to team chat.
	if ( players[consoleplayer].bDeadSpectator )
		return;

	// The server never should have a team!
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	if ( argv.argc( ) < 2 )
	{
		// The message we send is a message to teammates only.
		chat_SetChatMode( CHATMODE_TEAM );
		
		// Hide the console.
		C_HideConsole( );

		// Clear out the chat buffer.
		g_ChatBuffer.Clear();
	}
	else
	{
		for ( ulIdx = 1; ulIdx < static_cast<unsigned int>(argv.argc( )); ulIdx++ )
			ChatString.AppendFormat( "%s ", argv[ulIdx] );

		// We typed out our message in the console or with a macro. Go ahead and send the message now.
		chat_SendMessage( CHATMODE_TEAM, ChatString.GetChars( ));
	}
}

//*****************************************************************************
//
// [RC] Lets clients ignore an annoying player's chat messages.
//
void chat_IgnorePlayer( FCommandLine &argv, const ULONG ulPlayer )
{
	// Print the explanation message.
	if ( argv.argc( ) < 2 )
	{
		// Create a list of currently ignored players.
		FString PlayersIgnored;
		chat_GetIgnoredPlayers( PlayersIgnored );

		if ( PlayersIgnored.Len( ))
			Printf( "\\cgIgnored players: \\c-%s\nUse \"unignore\" or \"unignore_idx\" to undo.\n", PlayersIgnored.GetChars() );
		else
			Printf( "Ignores a certain player's chat messages.\nUsage: ignore <name> [duration, in minutes]\n" );

		return;
	}
	
	LONG	lTicks = -1;
	const LONG lArgv2 = ( argv.argc( ) >= 3 ) ? atoi( argv[2] ) : -1;
	
	// Did the user specify a set duration?
	if ( ( lArgv2 > 0 ) && ( lArgv2 < LONG_MAX / ( TICRATE * MINUTE )))
		lTicks = lArgv2 * TICRATE * MINUTE;

	if ( ulPlayer == MAXPLAYERS )
		Printf( "There isn't a player named %s\\c-.\n", argv[1] );
	else if ( ( ulPlayer == (ULONG)consoleplayer ) && ( NETWORK_GetState( ) != NETSTATE_SERVER ) )
		Printf( "You can't ignore yourself.\n" );
	else if ( players[ulPlayer].bIgnoreChat && ( players[ulPlayer].lIgnoreChatTicks == lTicks ))
		Printf( "You're already ignoring %s\\c-.\n", players[ulPlayer].userinfo.GetName() );
	else
	{
		players[ulPlayer].bIgnoreChat = true;
		players[ulPlayer].lIgnoreChatTicks = lTicks;
		Printf( "%s\\c- will now be ignored", players[ulPlayer].userinfo.GetName() );
		if ( lTicks > 0 )
			Printf( ", for %d minutes", static_cast<int>(lArgv2));
		Printf( ".\n" );

		// Add a helpful note about bots.
		if ( players[ulPlayer].bIsBot )
			Printf( "Note: you can disable all bot chat by setting the CVAR bot_allowchat to false.\n" );

		// Notify the server so that others using this IP are also ignored.
		if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
			CLIENTCOMMANDS_Ignore( ulPlayer, true, lTicks );
	}
}

CCMD( ignore )
{
	// Find the player and ignore him.
	chat_IgnorePlayer( argv, argv.argc( ) >= 2 ? SERVER_GetPlayerIndexFromName( argv[1], true, true ) : MAXPLAYERS );
}

CCMD( ignore_idx )
{
	int playerIndex;
	if ( argv.SafeGetNumber( 1, playerIndex ) == false )
		return;

	if ( PLAYER_IsValidPlayer( playerIndex ) == false )
		return;

	chat_IgnorePlayer( argv, playerIndex );
}

//*****************************************************************************
//
// [RC] Undos "ignore".
//
void chat_UnignorePlayer( FCommandLine &argv, const ULONG ulPlayer )
{
	// Print the explanation message.
	if ( argv.argc( ) < 2 )
	{
		// Create a list of currently ignored players.
		FString PlayersIgnored = "";
		chat_GetIgnoredPlayers( PlayersIgnored );

		if ( PlayersIgnored.Len( ))
			Printf( "\\cgIgnored players: \\c-%s\n", PlayersIgnored.GetChars() );
		else
			Printf( "Un-ignores a certain player's chat messages.\nUsage: unignore <name>\n" );

		return;
	}
	
	if ( ulPlayer == MAXPLAYERS )
		Printf( "There isn't a player named %s\\c-.\n", argv[1] );
	else if ( ( ulPlayer == (ULONG)consoleplayer ) && ( NETWORK_GetState( ) != NETSTATE_SERVER ) )
		Printf( "You can't unignore yourself.\n" );
	else if ( !players[ulPlayer].bIgnoreChat )
		Printf( "You're not ignoring %s\\c-.\n", players[ulPlayer].userinfo.GetName() );
	else 
	{
		players[ulPlayer].bIgnoreChat = false;
		players[ulPlayer].lIgnoreChatTicks = -1;
		Printf( "%s\\c- will no longer be ignored.\n", players[ulPlayer].userinfo.GetName() );

		// Notify the server so that others using this IP are also ignored.
		if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
			CLIENTCOMMANDS_Ignore( ulPlayer, false );
	}
}

CCMD( unignore )
{
	chat_UnignorePlayer( argv, argv.argc( ) >= 2 ? SERVER_GetPlayerIndexFromName( argv[1], true, true ) : MAXPLAYERS );
}

CCMD( unignore_idx )
{
	int playerIndex;
	if ( argv.SafeGetNumber( 1, playerIndex ) == false )
		return;

	if ( PLAYER_IsValidPlayer( playerIndex ) == false )
		return;

	chat_UnignorePlayer( argv, playerIndex );
}

// [TP]
CCMD( messagemode )
{
	if ( NETWORK_GetState() != NETSTATE_SERVER )
	{
		chat_SetChatMode( CHATMODE_GLOBAL );
		C_HideConsole( );
		g_ChatBuffer.Clear();
	}
}

// [TP]
CCMD( messagemode2 )
{
	if ( NETWORK_GetState() != NETSTATE_SERVER )
	{
		chat_SetChatMode( CHATMODE_TEAM );
		C_HideConsole( );
		g_ChatBuffer.Clear();
	}
}

