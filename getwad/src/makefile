#
# Makefile for Microsoft C++ 8.0/9.0/10.0
#
SDKPATH = "c:\Programme\util\Microsoft Platform SDK"
CC	= cl
CFLAGS	= -Ox -W3 -D_WIN32 -D__MSC -nologo -MT -I./win32 -I$(SDKPATH)\Include
LFLAGS	= -nologo -DLL -subsystem:windows -entry:DllEntryPoint@12
OBJS	= getwad.obj inet.obj base64.obj unzip.obj misc.obj
LIBS	= kernel32.lib libcmt.lib user32.lib comctl32.lib shell32.lib\
	  wsock32.lib advapi32.lib gdi32.lib wininet.lib zlibmt.lib

getwad.exe: getwade.c getwad.h getwad.dll getwade.res
	$(CC) $(CFLAGS) -I$(SDKPATH)\Lib -Fegetwad.exe getwade.c getwade.res getwad.lib user32.lib
	@del getwade.obj

getwad.dll: $(OBJS) getwad.res
	lib /LIBPATH:$(SDKPATH)\Lib -nologo -machine:i386 -def:getwad.def $(OBJS) -out:getwad.lib
	@echo $(LFLAGS) -out:getwad.dll getwad.res > j.rsp
	@echo $(OBJS) getwad.exp $(LIBS) >> j.rsp
	link /LIBPATH:$(SDKPATH)\Lib @j.rsp
	@del j.rsp
	@del getwad.exp

getwad.res: getwad.rc getwadrc.h
	rc /i$(SDKPATH)\Include /r /fo getwad.tmp getwad.rc
    cvtres /nologo /machine:x86 /out:getwad.res getwad.tmp
	@del getwad.tmp

getwad.obj:	getwad.c getwadrc.h getwad.h inet.h unzip.h misc.h
	$(CC) $(CFLAGS) -c getwad.c

inet.obj: inet.c inet.h base64.h
	$(CC) $(CFLAGS) -c inet.c

base64.obj: base64.c base64.h
	$(CC) $(CFLAGS) -c base64.c

unzip.obj: unzip.c unzip.h
	$(CC) $(CFLAGS) -c unzip.c

getwade.res: getwade.rc getwad.ico
	rc /i$(SDKPATH)\Include /r /fo getwade.tmp getwade.rc
	cvtres /nologo /machine:x86 /out:getwade.res getwade.tmp
	@del getwade.tmp

misc.obj: misc.c misc.h
	$(CC) $(CFLAGS) -c misc.c
