#ifndef GETWADH__
#define GETWADH__
/*
 * Library to locate, download, unpack and install WAD files
 * automatically. The library interface is minimal in order
 * to facilitate its use from other programs. The "getwad_setup()"
 * function presents a dialog that lets the user specify various
 * searching parameters and the "getwad_fetch()" function does
 * the actual work. The DLL can be called from either single-
 * or multi-threaded programs (or DLLs), but it must not be
 * called CONCURRENTLY from multiple threads of the same program
 * (ie., it's not reentrant). This should not matter for the
 * intended use. It should be able to be used from practically
 * any language that supports DLLs.
 *
 * Written by Hippocrates Sendoukas, Athens, Greece, June-December 2003
 */

#ifdef __cplusplus
extern "C" {
#endif

#if _WIN32
#include <windows.h>

void WINAPI	getwad_setup(HWND hwnd, BOOL allow_dest_spec,
			LPCSTR ini_name,LPCSTR ini_section);
int WINAPI	getwad_fetch(HWND hwnd,LPCSTR wadname,LPCSTR waddir,
			BOOL dont_wait_upon_success,
			LPCSTR ini_name, LPCSTR ini_section);
void WINAPI	getwad_init(HINSTANCE hinstance);

void WINAPI	getwad2_setup(HWND hwnd, BOOL allow_dest_spec,
			LPCSTR ini_name,LPCSTR ini_section);
int WINAPI	getwad2_fetch(HWND hwnd,LPCSTR wadname,LPCSTR waddir,
			BOOL dont_wait_upon_success,
			LPCSTR ini_name, LPCSTR ini_section,
			LPCSTR server_search);

#else

int getwad_fetch(const char *wadname, const char *waddir,
			const char *ini_name, const char *ini_section);

int getwad2_fetch(const char *wadname, const char *waddir,
			const char *ini_name, const char *ini_section,
			const char *server_search);

#endif

#ifdef __cplusplus
}
#endif

#endif
