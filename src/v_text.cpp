/*
** v_text.cpp
** Draws text to a canvas. Also has a text line-breaker thingy.
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

#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

#include "v_text.h"

#include "i_system.h"
#include "v_video.h"
#include "hu_stuff.h"
#include "w_wad.h"
#include "m_swap.h"

#include "doomstat.h"
#include "templates.h"

// [BB] New #includes.
#include "sv_main.h"

//
// DrawChar
//
// Write a single character using the given font
//
void STACK_ARGS DCanvas::DrawChar (FFont *font, int normalcolor, int x, int y, BYTE character, ...)
{
	if (font == NULL)
		return;

	if (normalcolor >= NumTextColors)
		normalcolor = CR_UNTRANSLATED;

	FTexture *pic;
	int dummy;

	if (NULL != (pic = font->GetChar (character, &dummy)))
	{
		const FRemapTable *range = font->GetColorTranslation ((EColorRange)normalcolor);
		va_list taglist;
		va_start (taglist, character);
		DrawTexture (pic, x, y, DTA_Translation, range, TAG_MORE, &taglist);
		va_end (taglist);
	}
}

//
// DrawText
//
// Write a string using the given font
//
void DCanvas::DrawTextV(FFont *font, int normalcolor, int x, int y, const char *string, va_list taglist)
{
	INTBOOL boolval;
	va_list tags;
	uint32 tag;

	int			maxstrlen = INT_MAX;
	int 		w, maxwidth;
	const BYTE *ch;
	int 		c;
	int 		cx;
	int 		cy;
	int			boldcolor;
	const FRemapTable *range;
	int			height;
	int			forcedwidth = 0;
	int			scalex, scaley;
	int			kerning;
	FTexture *pic;

	if (font == NULL || string == NULL)
		return;

	if (normalcolor >= NumTextColors)
		normalcolor = CR_UNTRANSLATED;
	boldcolor = normalcolor ? normalcolor - 1 : NumTextColors - 1;

	range = font->GetColorTranslation ((EColorRange)normalcolor);
	height = font->GetHeight () + 1;
	kerning = font->GetDefaultKerning ();

	ch = (const BYTE *)string;
	cx = x;
	cy = y;

	// Parse the tag list to see if we need to adjust for scaling.
 	maxwidth = Width;
	scalex = scaley = 1;

#ifndef NO_VA_COPY
	va_copy(tags, taglist);
#else
	tags = taglist;
#endif
	tag = va_arg(tags, uint32);

	while (tag != TAG_DONE)
	{
		va_list *more_p;
		DWORD data;

		switch (tag)
		{
		case TAG_IGNORE:
		default:
			data = va_arg (tags, DWORD);
			break;

		case TAG_MORE:
			more_p = va_arg (tags, va_list*);
			va_end (tags);
#ifndef NO_VA_COPY
			va_copy (tags, *more_p);
#else
			tags = *more_p;
#endif
			break;

		// We don't handle these. :(
		case DTA_DestWidth:
		case DTA_DestHeight:
		case DTA_Translation:
			assert("Bad parameter for DrawText" && false);
			return;

		case DTA_CleanNoMove_1:
			boolval = va_arg (tags, INTBOOL);
			if (boolval)
			{
				scalex = CleanXfac_1;
				scaley = CleanYfac_1;
				maxwidth = Width - (Width % scalex);
			}
			break;

		case DTA_CleanNoMove:
			boolval = va_arg (tags, INTBOOL);
			if (boolval)
			{
				scalex = CleanXfac;
				scaley = CleanYfac;
				maxwidth = Width - (Width % scalex);
			}
			break;

		case DTA_Clean:
		case DTA_320x200:
			boolval = va_arg (tags, INTBOOL);
			if (boolval)
			{
				scalex = scaley = 1;
				maxwidth = 320;
			}
			break;

		case DTA_VirtualWidth:
			maxwidth = va_arg (tags, int);
			scalex = scaley = 1;
			break;

		case DTA_TextLen:
			maxstrlen = va_arg (tags, int);
			break;

		case DTA_CellX:
			forcedwidth = va_arg (tags, int);
			break;

		case DTA_CellY:
			height = va_arg (tags, int);
			break;
		}
		tag = va_arg (tags, uint32);
	}
	va_end(tags);

	height *= scaley;
		
	while ((const char *)ch - string < maxstrlen)
	{
		c = *ch++;
		if (!c)
			break;

		if (c == TEXTCOLOR_ESCAPE)
		{
			EColorRange newcolor = V_ParseFontColor (ch, normalcolor, boldcolor);
			if (newcolor != CR_UNDEFINED)
			{
				range = font->GetColorTranslation (newcolor);
			}
			continue;
		}
		
		if (c == '\n')
		{
			cx = x;
			cy += height;
			continue;
		}

		if (NULL != (pic = font->GetChar (c, &w)))
		{
#ifndef NO_VA_COPY
			va_copy(tags, taglist);
#else
			tags = taglist;
#endif
			if (forcedwidth)
			{
				w = forcedwidth;
				DrawTexture (pic, cx, cy,
					DTA_Translation, range,
					DTA_DestWidth, forcedwidth,
					DTA_DestHeight, height,
					TAG_MORE, &tags);
			}
			else
			{
				DrawTexture (pic, cx, cy,
					DTA_Translation, range,
					TAG_MORE, &tags);
			}
			va_end (tags);
		}
		cx += (w + kerning) * scalex;
	}
	va_end(taglist);
}

void STACK_ARGS DCanvas::DrawText (FFont *font, int normalcolor, int x, int y, const char *string, ...)
{
	va_list tags;
	va_start(tags, string);
	DrawTextV(font, normalcolor, x, y, string, tags);
}

// A synonym so that this can still be used in files that #include Windows headers
void STACK_ARGS DCanvas::DrawTextA (FFont *font, int normalcolor, int x, int y, const char *string, ...)
{
	va_list tags;
	va_start(tags, string);
	DrawTextV(font, normalcolor, x, y, string, tags);
}

//
// Find string width using this font
//
int FFont::StringWidth (const BYTE *string) const
{
	int w = 0;
	int maxw = 0;
		
	while (*string)
	{
		if (*string == TEXTCOLOR_ESCAPE)
		{
			++string;
			if (*string == '[')
			{
				while (*string != '\0' && *string != ']')
				{
					++string;
				}
			}
			if (*string != '\0')
			{
				++string;
			}
			continue;
		}
		else if (*string == '\n')
		{
			if (w > maxw)
				maxw = w;
			w = 0;
			++string;
		}
		else
		{
			w += GetCharWidth (*string++) + GlobalKerning;
		}
	}
				
	return MAX (maxw, w);
}

// [BB] Allows to apply an arbitrary function that expects a char array to a FString.
void V_ApplyCharArrayFunctionToFString ( FString &String, void (*CharArrayFunction) ( char *pszString ) )
{
	const int length = static_cast<int>(String.Len());
	char *tempCharArray = new char[length+1];
	strncpy( tempCharArray, String.GetChars(), length );
	tempCharArray[length] = 0;
	CharArrayFunction( tempCharArray );
	String = tempCharArray;
	delete[] tempCharArray;
}

// [BC] This is essentially strbin() from the original ZDoom code. It's been made a public
// function so that we can colorize lots of things.
void V_ColorizeString( char *pszString )
{
	// [BB] If the string ends with "\c", i.e. an incomplete color code, remove it.
	const int length = strlen ( pszString );
	if ( ( length > 1 ) && ( pszString[length-2] == '\\' ) && ( pszString[length-1] == 'c' ) )
	{
		pszString[length-2] = 0;
		pszString[length-1] = 0;
	}

	char *p = pszString, c;
	int i;

	while ( (c = *p++) )
	{
		// If we don't encounter a slash, keep parsing.
		if (c != '\\')
			*pszString++ = c;
		else
		{
			switch (*p)
			{
				case 'c':
					*pszString++ = TEXTCOLOR_ESCAPE;
					break;
				case 'n':
					*pszString++ = '\n';
					break;
				case 't':
					*pszString++ = '\t';
					break;
				case 'r':
					*pszString++ = '\r';
					break;
//				case '\n':
//					break;
				case 'x':
				case 'X':
					c = 0;
					for (i = 0; i < 2; i++)
					{
						p++;
						if (*p >= '0' && *p <= '9')
							c = (c << 4) + *p-'0';
						else if (*p >= 'a' && *p <= 'f')
							c = (c << 4) + 10 + *p-'a';
						else if (*p >= 'A' && *p <= 'F')
							c = (c << 4) + 10 + *p-'A';
						else
						{
							p--;
							break;
						}
					}
					*pszString++ = c;
					break;
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
					c = 0;
					for (i = 0; i < 3; i++) {
						c <<= 3;
						if (*p >= '0' && *p <= '7')
							c += *p-'0';
						else
							break;
						p++;
					}
					*pszString++ = c;
					break;
				default:
					*pszString++ = *p;
					break;
			}
			p++;
		}
	}
	*pszString = 0;
}

// [BB] Version of V_ColorizeString that accepts a FString as argument.
// Hacks like "V_ColorizeString( (char *)g_MOTD.GetChars( ));"
// are not needed anymore using this.
void V_ColorizeString( FString &String )
{
	const int length = (int) String.Len();
	char *tempCharArray = new char[length+1];
	strncpy( tempCharArray, String.GetChars(), length );
	tempCharArray[length] = 0;
	V_ColorizeString( tempCharArray );
	String = tempCharArray;
	delete[] tempCharArray;
}

//*****************************************************************************
//
// V_UnColorizeString
//
// This essentially does the reverse of V_ColorizeString(). It takes a string with
// color codes and puts it back in \c<color code> format
//
void V_UnColorizeString( FString &String )
{
	String.Substitute( TEXTCOLOR_ESCAPE, "\\c" );
}

// [BC] This strips color codes from a string.
void V_RemoveColorCodes( char *pszString )
{
	char	*p;
	char	c;
	char	*pszEnd;

	// Start at the beginning of the string.
	p = pszString;

	// Look at the current character.
	while ( (c = *p) )
	{
		// If this is a color character, remove it along with the color code from the string.
		if ( c == TEXTCOLOR_ESCAPE )
		{
			pszEnd = p + 1;

			// This character is the color code. Ones that start with a left bracket are
			// multiple characters and end with the right bracket.
			switch ( *pszEnd )
			{
			case '[':

				while (( *pszEnd != ']' ) && ( *pszEnd != 0 ))
					pszEnd++;
				break;
			default:

				break;
			}

			if ( *pszEnd != 0 )
				pszEnd++;

			// [BB] Don't use memcpy here, source and dest would be overlapping.
			const ULONG ulLength = strlen( pszEnd );
			for ( ULONG i = 0; i < ulLength; ++i )
			  p[i] = pszEnd[i];
			p[ulLength] = 0;
		}
		// Otherwise, look at the next character.
		else
			p++;
	}
}

// [RC] Strips color codes from an FString.
void V_RemoveColorCodes( FString &String )
{
	const ULONG		length = (ULONG) String.Len();
	char			*szString = new char[length+1];
	
	// Copy the FString to a temporary char array.
	strncpy( szString, String.GetChars(), length );
	szString[length] = 0;

	// Remove the colors.
	V_ColorizeString( szString );
	V_RemoveColorCodes( szString );

	// Convert back and clean up.
	String = szString;
	delete[] szString;
}

// [BB] Strips color codes from a string respecting sv_colorstripmethod.
void V_StripColors( char *pszString )
{
	switch ( sv_colorstripmethod )
	{
	// Strip messages here of color codes.
	case 0:

		V_ColorizeString( pszString );
		V_RemoveColorCodes( pszString );
		break;
	// Don't strip out the color codes.
	case 1:

		V_ColorizeString( pszString );
		break;
	// Just leave the damn thing alone!
	case 2:
	default:

		break;
	}
}

// [RC] Returns whether this character is allowed in names.
bool v_IsCharAcceptableInNames ( char c )
{
	// Allow color codes.
	if ( c == 28 )
		return true;

	// No undisplayable system ascii.
	if ( c <= 31 )
		return false;

	// Percent is forbiddon in net names, because it can be abused for string exploits.
	if ( c == '%' )
		return false;

	// Ampersands aren't very distinguishable in Heretic
	if ( c == '&' )
		return false;

	// No escape codes (\c is handled differently).
	if ( c == 92 )
		return false;

	// Tilde is invisible.
	if ( c == 96 )
		return false;

	// Ascii above 123 is invisible or hard to type.
	if ( c >= 123 )
		return false;

	return true;
}

// [RC] Returns whether this character is invisible.
bool v_IsCharacterWhitespace ( char c )
{
	// System ascii < 32 is invisible.
	if ( c <= 31 )
		return true;

	// Text colorization.
	if ( c == TEXTCOLOR_ESCAPE )
		return true;

	// Space.
	if ( c == 32 )
		return true;

	// Delete.
	if ( c == 127 )
		return true;

	// Ascii 255.
	if ( c == static_cast<char> (255) ) //[BL] 255 is supposedly out of range for a char
		return true;

	return false;
}


// [RC] Conforms names to meet standards.
void V_CleanPlayerName( char *pszString )
{
	char	*pszStart;
	char	*p;
	char	c;
	ULONG	ulStringLength;
	ULONG   ulTotalLength;
	ULONG   ulNonWhitespace;
	char	szColorlessName[256];

	ulStringLength = static_cast<ULONG>(strlen( pszString ));
	ulTotalLength = 0;
	ulNonWhitespace = 0;

	// Start at the beginning of the string.
	p = pszString;
	pszStart = pszString;

	// The name must be longer than three characters.
	if ( ulStringLength < 3 )
	{
		strcpy( pszString, "Player" );
		return;
	}

	// Go through and remove the illegal characters.
	while ( (c = *p++) )
	{
		if ( !v_IsCharAcceptableInNames(c) )
		{
			ULONG	ulPos;
			ulStringLength = static_cast<ULONG>(strlen( pszString ));

			// Shift the rest of the string back one.
			for ( ulPos = 0; ulPos < ulStringLength; ulPos++ )
				pszString[ulPos] = pszString[ulPos + 1];

			// Don't skip a character.
			p--;
		}
		else
		{
			pszString++;
			ulTotalLength++;
		}
	}

	// Cut the string at its new end.
	*pszString = 0;

	// [BB] Remove any trailing incomplete escaped color codes. Since we just removed
	// quite a bit from the string, it's possible that those are there now.
	// Note: We need to work on pszStart now, by now pszString only contains a pointer
	// to the end of the string.
	// [BB] I don't want to implement the trailing crap removement for escaped and
	// unescaped color codes, so I have to uncolorize, clean and colorize the name here.
	// Not so efficient, but V_CleanPlayerName is called seldom enough so that this
	// doesn't matter.
	FString tempString = pszStart;
	V_UnColorizeString ( tempString );
	V_RemoveTrailingCrapFromFString ( tempString );
	// [BB] If the name uses color codes, make sure that it is terminated with "\\c-".
	// V_RemoveTrailingCrapFromFString removes all trailing color codes including "\\c-".
	// This is necessary to catch incomplete color codes before the trailing "\\c-".
	// Hence, we have to re-add "\\c-" here.
	if ( ( tempString.IndexOf ( "\\c" ) != -1 ) )
	{
		// [BB] In the uncolorized string, color codes need one additional char, take this
		// into account when checking whether the name is too long.
		FString tempColorizedString = tempString.GetChars();
		V_ColorizeString ( tempColorizedString );
		const unsigned int numColorCodes = tempString.Len() - tempColorizedString.Len();

		if ( tempString.Len() > MAXPLAYERNAME - 3 + numColorCodes )
		{
			tempString = tempString.Left ( MAXPLAYERNAME - 3 + numColorCodes );
			V_RemoveTrailingCrapFromFString ( tempString );
		}
		tempString += "\\c-";
	}
	V_ColorizeString ( tempString );
	sprintf ( pszStart, "%s", tempString.GetChars() );

	// Determine the name's actual length.
	strncpy( szColorlessName, pszStart, 256 );
	V_RemoveColorCodes( szColorlessName );
	
	p = szColorlessName;
	ulNonWhitespace = 0;
	while ( (c = *p++) )
	{
		if ( !v_IsCharacterWhitespace(c) )
			ulNonWhitespace++;
	}
		
	// Check the length again, as characters were removed.
	if ( ulNonWhitespace < 3 )
		strcpy( pszStart, "Player" );
}

// [BB] Version of V_CleanPlayerName that accepts a FString as argument.
void V_CleanPlayerName( FString &String )
{
	const int length = (int) String.Len();
	// [BB] V_CleanPlayerName possibly appends "\\c-", hence we need to reserve more memory than just "length + terminating 0".
	// [EP] Don't forget it might return "Player" in case the name results being too short.
	char *tempCharArray = new char[MAX<size_t>(7, length+4)];
	strncpy( tempCharArray, String.GetChars(), length );
	tempCharArray[length] = 0;
	V_CleanPlayerName( tempCharArray );
	String = tempCharArray;
	delete[] tempCharArray;
}

// [RC] Converts COL_ numbers to their \c counterparts.
char V_GetColorChar( ULONG ulColor )
{
	return (char) ( 97 + (int) ulColor );
}

// [RC] Escapes quotes and backslashes.
void V_EscapeBacklashes( FString &String )
{
	// Escape any backslashes or quotation marks before sending the name to the console.
	FString EscapedName = "";
	for ( const char *p = String; *p != '\0'; ++p )
	{
		if ( *p == '"' || *p == '\\' )
			EscapedName << '\\';
		EscapedName << *p;
	}

	String = EscapedName;
}

bool V_ColorCodeStart ( const char *pszString, ULONG ulPosition )
{
	return ( pszString[ulPosition] == '\\' ) && ( pszString[ulPosition+1] == 'c' );
}

// [BB] Removes trailing crap (so far invisilble charactes and color codes) from a string.
void V_RemoveTrailingCrap( char *pszString )
{
	if ( pszString == NULL )
		return;

	ULONG ulStringLength = static_cast<ULONG>(strlen( pszString ));
	while ( ulStringLength > 0 )
	{
		// [BB] Remove trailing whitespace.
		if ( v_IsCharacterWhitespace ( pszString[ulStringLength-1] ) )
		{
			pszString[ulStringLength-1] = 0;
			ulStringLength--;
		}
		// [BB] Remove trailing escaped terminator, i.e "\0".
		else if ( ( ulStringLength > 1 ) && ( pszString[ulStringLength-2] == '\\' ) && ( pszString[ulStringLength-1] == '0' ) )
		{
			pszString[ulStringLength-2] = 0;
			ulStringLength -= 2;
		}
		// [BB] Remove trailing incomplete color code, i.e "\c".
		else if ( ( ulStringLength > 1 ) && V_ColorCodeStart ( pszString, ulStringLength-2 ) )
		{
			pszString[ulStringLength-2] = 0;
			ulStringLength -= 2;
		}
		// [BB] Remove trailing color code of type "\cX".
		else if ( ( ulStringLength > 2 ) && V_ColorCodeStart ( pszString, ulStringLength-3 ) )
		{
			pszString[ulStringLength-3] = 0;
			ulStringLength -= 3;
		}
		// [BB] Remove trailing color code of type "\c[X]".
		else if ( pszString[ulStringLength-1] == ']' )
		{
			int i = 0;
			for ( i = ulStringLength-2; i >= 2; --i )
			{
				if ( pszString[i] == '[' )
					break;
			}
			if ( ( i >= 2 ) && V_ColorCodeStart ( pszString, i-2 ) )
			{
				pszString[i-2] = 0;
				ulStringLength = static_cast<ULONG>(strlen( pszString ));
			}
			else
				break;
		}
		// [BB] Remove trailing incomplete color code of type "\c[X".
		else
		{
			// [K6] We remove any unclosed color codes here.
			int i = 0;
			for ( i = ulStringLength-2; i >= 2; --i )
			{
				if ( pszString[i] == '[' && V_ColorCodeStart ( pszString, i-2 ))
					break;
			}
			const char* pChar = &pszString[i];
			if ( pChar > strrchr(pszString, ']') )
			{
				const int index = pChar - pszString;
				if ( ( index > 2 ) && V_ColorCodeStart ( pszString, index-2 ) )
				{
					pszString[index-2] = 0;
					ulStringLength = static_cast<ULONG>(strlen( pszString ));
				}
				else
					break;
			}
			else
				break;
		}
	}
}

// [BB] FString version of V_RemoveTrailingCrap.
void V_RemoveTrailingCrapFromFString( FString &String )
{
	V_ApplyCharArrayFunctionToFString ( String, &V_RemoveTrailingCrap );
}

// [BB] Removes invalid color codes, i.e. \cX where X not in [a,u] or '-'.
void V_RemoveInvalidColorCodes( char *pszString )
{
	if ( pszString == NULL )
		return;

	const int iStringLength = static_cast<int>(strlen( pszString ));

	for ( int i = 0; i < iStringLength - 2; ++ i )
	{
		// [BB] If there is an color code start followed by an invalid char, remove the
		// leading '\' to disable the color code.
		if ( V_ColorCodeStart ( pszString, i )
			&& ( pszString[i+2] != '-' )
			&& ( pszString[i+2] != '!' )
			&& ( pszString[i+2] != '*' )
			&& ( pszString[i+2] != '+' )
			&& ( pszString[i+2] != '[' )
			&& ( ( pszString[i+2] < 'a' ) || ( pszString[i+2] > 'v' ) )
			&& ( ( pszString[i+2] < 'A' ) || ( pszString[i+2] > 'V' ) ) )
			pszString[i] = ' ';
	}
}

// [BB] FString version of V_RemoveInvalidColorCodes.
void V_RemoveInvalidColorCodes( FString &String )
{
	V_ApplyCharArrayFunctionToFString ( String, &V_RemoveInvalidColorCodes );
}

//
// Break long lines of text into multiple lines no longer than maxwidth pixels
//
static void breakit (FBrokenLines *line, FFont *font, const BYTE *start, const BYTE *stop, FString &linecolor)
{
	if (!linecolor.IsEmpty())
	{
		line->Text = TEXTCOLOR_ESCAPE;
		line->Text += linecolor;
	}
	line->Text.AppendCStrPart ((const char *)start, stop - start);
	line->Width = font->StringWidth (line->Text);
}

FBrokenLines *V_BreakLines (FFont *font, int maxwidth, const BYTE *string)
{
	FBrokenLines lines[128];	// Support up to 128 lines (should be plenty)

	const BYTE *space = NULL, *start = string;
	size_t i, ii;
	int c, w, nw;
	FString lastcolor, linecolor;
	bool lastWasSpace = false;
	int kerning = font->GetDefaultKerning ();

	i = w = 0;

	while ( (c = *string++) && i < countof(lines) )
	{
		if (c == TEXTCOLOR_ESCAPE)
		{
			if (*string)
			{
				if (*string == '[')
				{
					const BYTE *start = string;
					while (*string != ']' && *string != '\0')
					{
						string++;
					}
					if (*string != '\0')
					{
						string++;
					}
					lastcolor = FString((const char *)start, string - start);
				}
				else
				{
					lastcolor = *string++;
				}
			}
			continue;
		}

		if (isspace(c)) 
		{
			if (!lastWasSpace)
			{
				space = string - 1;
				lastWasSpace = true;
			}
		}
		else
		{
			lastWasSpace = false;
		}

		nw = font->GetCharWidth (c);

		if ((w > 0 && w + nw > maxwidth) || c == '\n')
		{ // Time to break the line
			if (!space)
				space = string - 1;

			breakit (&lines[i], font, start, space, linecolor);
			if (c == '\n')
			{
				lastcolor = "";		// Why, oh why, did I do it like this?
			}
			linecolor = lastcolor;

			i++;
			w = 0;
			lastWasSpace = false;
			start = space;
			space = NULL;

			while (*start && isspace (*start) && *start != '\n')
				start++;
			if (*start == '\n')
				start++;
			else
				while (*start && isspace (*start))
					start++;
			string = start;
		}
		else
		{
			w += nw + kerning;
		}
	}

	// String here is pointing one character after the '\0'
	if (i < countof(lines) && --string - start >= 1)
	{
		const BYTE *s = start;

		while (s < string)
		{
			// If there is any non-white space in the remainder of the string, add it.
			if (!isspace (*s++))
			{
				breakit (&lines[i++], font, start, string, linecolor);
				break;
			}
		}
	}

	// Make a copy of the broken lines and return them
	FBrokenLines *broken = new FBrokenLines[i+1];

	for (ii = 0; ii < i; ++ii)
	{
		broken[ii] = lines[ii];
	}
	broken[ii].Width = -1;

	return broken;
}

void V_FreeBrokenLines (FBrokenLines *lines)
{
	if (lines)
	{
		delete[] lines;
	}
}
