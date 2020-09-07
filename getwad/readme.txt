This is the source code of GetWAD for Win32 (GUI version)
and Unix (command line version). It consists of 4 directories:

1. The "src" directory contains the program's source code
   which can be compiled with the Microsoft Visual C++
   compiler (practically any version) under Win32, or
   an ANSI C compiler under Unix. For the Win32 environment,
   I have included a precompiled multithreaded version of
   ZLIB; if you use another compiler, you'll have to compile
   this yourself. When compiling under Unix, make sure that
   you have installed the zlib library and that your system
   supports Posix threads. To compile the program, make any
   relevant changes to the appropriate makefile (makefile.win
   for Win32 and makefile.unx for Unix) and type:
      nmake -fmakefile.win	(for Win32)
      make -fmakefile.unx	(for Unix)
   It should be quite easy.

2. The "docs" directory contains the program documentation
   which consists of the user's manual (getwad.htm) and
   the programmer's reference (progref.htm). The first one
   gives you an overview from the point of view of the user;
   the second one is intended for people who may want to
   use the GetWAD DLL in their own programs.

3. The "setup" directory which contains an installer script
   for making the GetWAD setup program. The script uses the
   Nullsoft NSIS installer.

4. The "unix" directory; it contains a unix version of the
   GetWAD configuration file (getwad.ini) that should be
   placed in /usr/local/etc. Make sure that you edit this
   file and specify the appropriate WAD directory; this
   directory must exist and it must have write permission
   for the intended users of the program.

Hippocrates Sendoukas
December 2003, Athens, Greece
