/*
** EnumToString.h
** This is a small extension of the "enums to strings" code from Marcos F. Cardoso.
** The original code can be found at
** http://www.codeproject.com/KB/cpp/C___enums_to_strings.aspx.
**
**---------------------------------------------------------------------------
** Copyright 2009 Benjamin Berkels
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

#undef ENUM_ELEMENT
#undef ENUM_ELEMENT2
#undef BEGIN_ENUM
#undef END_ENUM

#ifndef GENERATE_ENUM_STRINGS
    #define ENUM_ELEMENT( element ) element
    #define ENUM_ELEMENT2( element, val ) element = val
    #define BEGIN_ENUM( ENUM_NAME ) typedef enum tag##ENUM_NAME
    #define END_ENUM( ENUM_NAME ) ENUM_NAME; \
            extern const char* GetString##ENUM_NAME(enum tag##ENUM_NAME index); \
			extern int GetValue##ENUM_NAME( const char* Name );
#else
    typedef struct { const char * desc; int type;} EnumDesc_t;
    #define ENUM_ELEMENT( element ) { #element, (int)(element) }
    #define ENUM_ELEMENT2( element, val ) { #element, val }
    #define BEGIN_ENUM( ENUM_NAME ) EnumDesc_t gs_##ENUM_NAME [] =
    #define END_ENUM( ENUM_NAME ) ; \
			const char* GetString##ENUM_NAME(enum tag##ENUM_NAME index) \
			{ \
				for (unsigned int i = 0; i < sizeof(gs_##ENUM_NAME)/sizeof(EnumDesc_t); i++) \
				{ \
					if ((int)index == gs_##ENUM_NAME [i].type) \
						return gs_##ENUM_NAME [i].desc; \
					} \
					return "Unknown Enum type!!"; \
			} \
			int GetValue##ENUM_NAME( const char* Name ) \
			{ \
				for (unsigned int i = 0; i < sizeof(gs_##ENUM_NAME)/sizeof(EnumDesc_t); i++) { \
					if (strcmp (Name, gs_##ENUM_NAME [i].desc) == 0 ) \
						return (gs_##ENUM_NAME [i].type); \
				} \
				return -1; \
			}
#endif
