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
#ifndef _stricmp
	#define _stricmp(x,y) strcasecmp(x,y)
#endif
#ifndef strnicmp
	#define strnicmp(x,y,z) strncasecmp(x,y,z)
#endif

typedef unsigned char UCHAR;
typedef bool BOOL;

// [C]
typedef unsigned short	USHORT;
typedef short		SHORT;
typedef unsigned char 	byte;
typedef float		FLOAT;
typedef int 		LONG;
struct POINT { 
  LONG x; 
  LONG y; 
};
struct RECT { 
  LONG left; 
  LONG top; 
  LONG right; 
  LONG bottom; 
}; 

#define __cdecl
#define _access(a,b)	access(a,b)

#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

#endif	//ndef _WIN32

// [BB] FreeBSD specific defines
#ifdef __FreeBSD__
#define __va_copy(x,y) va_copy(x,y)
#endif	//__FreeBSD__

#ifdef _MSC_VER
// [BB] Silence the "'stricmp': The POSIX name for this item is deprecated." warning.
#pragma warning(disable:4996)
#endif	//_MSC_VER

#endif	//ndef PLATFORM_H
