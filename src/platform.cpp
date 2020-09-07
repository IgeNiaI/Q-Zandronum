#include "zstring.h"

#ifndef _WIN32

#include <ctype.h>
#include <unistd.h>

// [BB] itoa is not available under Linux, so we supply a replacement here.
// Code taken from http://www.jb.man.ac.uk/~slowe/cpp/itoa.html
/**
 * Ansi C "itoa" based on Kernighan & Ritchie's "Ansi C"
 * with slight modification to optimize for specific architecture:
 */
	
void strreverse(char* begin, char* end) {
	char aux;
	while(end>begin)
		aux=*end, *end--=*begin, *begin++=aux;
}
	
char* itoa(int value, char* str, int base) {
	static char num[] = "0123456789abcdefghijklmnopqrstuvwxyz";
	char* wstr=str;
	int sign;
	div_t res;
	
	// Validate base
	if (base<2 || base>35){ *wstr='\0'; return str; }

	// Take care of sign
	if ((sign=value) < 0) value = -value;

	// Conversion. Number is reversed.
	do {
		res = div(value,base);
		*wstr++ = num[res.rem];
	}while((value=res.quot));
	if(sign<0) *wstr++='-';
	*wstr='\0';

	// Reverse string
	strreverse(str,wstr-1);
	
	return str;
}

/*
**  Portable, public domain replacements for strupr() by Bob Stout
*/

char *strupr(char *string)
{
      char *s;

      if (string)
      {
            for (s = string; *s; ++s)
                  *s = toupper(*s);
      }
      return string;
} 

void I_Sleep( int iMS )
{
	usleep( 1000 * iMS );
}

#endif

#ifdef NO_SERVER_GUI

#include <iostream>
#include "networkheaders.h"
#include "networkshared.h"
#include "v_text.h"

// [BB] I collect dummy implementations of many functions, which are either
// GL or server console gui related, here. This way one doesn't have to make
// define constructions whenever the functions are called elsewhere in the code.

// ------------------- Server console related stuff ------------------- 
// [BB] Most of these functions just do nothing if we don't have a GUI.
// Only SERVERCONSOLE_Print is really needed.
void SERVERCONSOLE_UpdateTitleString( const char *pszString ) {}
void SERVERCONSOLE_UpdateIP( NETADDRESS_s LocalAddress ) {}
void SERVERCONSOLE_UpdateBroadcasting( void ) {}
void SERVERCONSOLE_UpdateScoreboard( void ) {}
void SERVERCONSOLE_UpdateStatistics( void ) {}
void SERVERCONSOLE_SetCurrentMapname( const char *pszString ) {}
void SERVERCONSOLE_SetupColumns( void ) {}
void SERVERCONSOLE_ReListPlayers( void ) {}
void SERVERCONSOLE_UpdatePlayerInfo( LONG lPlayer, ULONG ulUpdateFlags ) {}
void SERVERCONSOLE_Print( char *pszString )
{
	V_StripColors( pszString );
	std::cout << pszString;
}
#endif //NO_SERVER_GUI
// ------------------- GL related stuff ------------------- 
#ifdef NO_GL
#include "d_player.h"
#include "p_setup.h"
#include "doomtype.h"

#ifdef _WIN32

#include "sdl/glstubs.cpp"
void FGLTexture::FlushAll() {}

#endif //_WIN32
#endif //NO_GL

//
// I_ConsoleInput - [NightFang] - pulled from the old 0.99 code
//
#ifndef	WIN32
int		stdin_ready = 0;
int		do_stdin = 1;
#else
#include "conio.h"
#endif

char *I_ConsoleInput (void)
{
#ifndef	WIN32
	static 	char text[256];
	int	len;
	if (!stdin_ready || !do_stdin)
	{ return NULL; }

	stdin_ready = 0;

	len = read(0, text, sizeof(text));
	if (len < 1)
	{ return NULL; }

	text[len-1] = 0;

	return text;
#else
	// Windows code
	static char     text[256];
    static int              len;
    int             c;

    // read a line out
    while (_kbhit())
    {
		c = _getch();
        putch (c);
        if (c == '\r')
        {
			text[len] = 0;
            putch ('\n');
            len = 0;
            return text;
        }
        
		if (c == 8)
        {
			if (len)
            {
				putch (' ');
                putch (c);
                len--;
                text[len] = 0;
            }
            continue;
        }
    
		text[len] = c;
        len++;
        text[len] = 0;
        if (len == sizeof(text))
		    len = 0;
	}

    return NULL;
#endif
}
