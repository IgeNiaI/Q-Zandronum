/*
** v_text.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#ifndef __V_TEXT_H__
#define __V_TEXT_H__

#include "doomtype.h"
#include "v_font.h"

struct FBrokenLines
{
	int			Width;
	FString		Text;
};

#define TEXTCOLOR_ESCAPE		'\034'

#define TEXTCOLOR_BRICK			"\034A"
#define TEXTCOLOR_TAN			"\034B"
#define TEXTCOLOR_GRAY			"\034C"
#define TEXTCOLOR_GREY			"\034C"
#define TEXTCOLOR_GREEN			"\034D"
#define TEXTCOLOR_BROWN			"\034E"
#define TEXTCOLOR_GOLD			"\034F"
#define TEXTCOLOR_RED			"\034G"
#define TEXTCOLOR_BLUE			"\034H"
#define TEXTCOLOR_ORANGE		"\034I"
#define TEXTCOLOR_WHITE			"\034J"
#define TEXTCOLOR_YELLOW		"\034K"
#define TEXTCOLOR_UNTRANSLATED	"\034L"
#define TEXTCOLOR_BLACK			"\034M"
#define TEXTCOLOR_LIGHTBLUE		"\034N"
#define TEXTCOLOR_CREAM			"\034O"
#define TEXTCOLOR_OLIVE			"\034P"
#define TEXTCOLOR_DARKGREEN		"\034Q"
#define TEXTCOLOR_DARKRED		"\034R"
#define TEXTCOLOR_DARKBROWN		"\034S"
#define TEXTCOLOR_PURPLE		"\034T"
#define TEXTCOLOR_DARKGRAY		"\034U"
#define TEXTCOLOR_CYAN			"\034V"

#define TEXTCOLOR_NORMAL		"\034-"
#define TEXTCOLOR_BOLD			"\034+"

#define TEXTCOLOR_CHAT			"\034C"
#define TEXTCOLOR_TEAMCHAT		"\034C"

// [BC] New text functions.
void	V_ApplyCharArrayFunctionToFString ( FString &String, void (*CharArrayFunction) ( char *pszString ) );
void	V_ColorizeString( char *pszString );
void	V_ColorizeString( FString &String );
void	V_UnColorizeString( FString &String );
void	V_RemoveColorCodes( FString &String );
void	V_RemoveColorCodes( char *pszString );
void	V_StripColors( char *pszString );
char	V_GetColorChar( ULONG ulColor );
void	V_EscapeBacklashes( FString &String );
void	V_RemoveTrailingCrap( char *pszString );
void	V_RemoveTrailingCrapFromFString( FString &String );
void	V_RemoveInvalidColorCodes( char *pszString );
void	V_RemoveInvalidColorCodes( FString &String );

// [RC] Functions related to user name cleaning.
bool	v_IsCharAcceptableInNames ( char c );
bool	v_IsCharacterWhitespace ( char c );
void	V_CleanPlayerName( char *pszString );
void	V_CleanPlayerName( FString &String );

FBrokenLines *V_BreakLines (FFont *font, int maxwidth, const BYTE *str);
void V_FreeBrokenLines (FBrokenLines *lines);
inline FBrokenLines *V_BreakLines (FFont *font, int maxwidth, const char *str)
 { return V_BreakLines (font, maxwidth, (const BYTE *)str); }

#endif //__V_TEXT_H__
