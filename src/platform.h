#ifndef PLATFORM_H
#define PLATFORM_H

// [C] now this file is also included by some /src files
// [C] therefore this file should probably be moved to another place but definetely not to /src/gl dir

// [BB] Some Winelib specfic things.
#ifdef __WINE__
#ifdef _WIN32
#ifdef unix
#undef unix
#endif	//unix
#endif	//_WIN32

#include <cmath>
#define sqrtf sqrt
#define sinf sin
#define cosf cos
#define fabsf(x) static_cast<float>(fabs(x))
#define ceilf(x) static_cast<float>(ceil(x))
#define atan2f(x,y) static_cast<float>(atan2(x,y))
#define fmodf(x,y) static_cast<float>(fmod(x,y))
#define modff(x,y)  ((float)modf((double)(x), (double *)(y)))
#endif	//__WINE__

// [BB] Linux specific thigs, mostly missing functions.
#ifndef _WIN32

#ifndef stricmp
#define stricmp(x,y) strcasecmp(x,y)
#endif
#define _stricmp(x,y) strcasecmp(x,y)
#define _itoa(x,y,z) itoa(x,y,z)

char* itoa(int value, char* str, int base);
char *strupr(char *string);

void I_Sleep( int iMS );
char *I_ConsoleInput (void);

typedef unsigned char UCHAR;
typedef bool BOOL;

#endif

// [BB] FreeBSD specific defines
#if defined ( __FreeBSD__ ) && ( __FreeBSD__ < 8 )
#define __va_copy(x,y) va_copy(x,y)
#endif	//__FreeBSD__

#ifdef _MSC_VER
// [BB] Silence the "'stricmp': The POSIX name for this item is deprecated." warning.
#pragma warning(disable:4996)
// [BB] This is necessary under VC++ 2008 along with the _DO_NOT_DECLARE_INTERLOCKED_INTRINSICS_IN_MEMORY
// define to be able to include <intrin.h> and <memory>, because they contain conflicting definitions
// of the interlocked intrinsics.
#include <intrin.h>
#endif	//_MSC_VER

#if defined(__MINGW32_VERSION) || defined(__MINGW64__)

// [BB] For i_keyboard.cpp (from dinput.h of the Windows SDK)
#ifndef DIK_PREVTRACK
#define DIK_PREVTRACK       0x90    /* Previous Track (DIK_CIRCUMFLEX on Japanese keyboard) */
#endif

// [BB] For i_xinput.cpp (from Xinput.h of the Windows SDK)
#ifndef XINPUT_DLL
#define XINPUT_DLL_A  "xinput9_1_0.dll"
#define XINPUT_DLL_W L"xinput9_1_0.dll"
#ifdef UNICODE
    #define XINPUT_DLL XINPUT_DLL_W
#else
    #define XINPUT_DLL XINPUT_DLL_A
#endif 
#endif

#endif

#endif	//ndef PLATFORM_H
