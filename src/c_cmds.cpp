/*
** c_cmds.cpp
** Miscellaneous console commands.
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
** It might be a good idea to move these into files that they are more
** closely related to, but right now, I am too lazy to do that.
*/

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#include "version.h"
#include "c_console.h"
#include "c_dispatch.h"

#include "i_system.h"

#include "doomerrors.h"
#include "doomstat.h"
#include "gstrings.h"
#include "s_sound.h"
#include "g_game.h"
#include "g_level.h"
#include "w_wad.h"
#include "g_level.h"
#include "gi.h"
#include "r_defs.h"
#include "d_player.h"
#include "templates.h"
#include "p_local.h"
#include "r_sky.h"
#include "p_setup.h"
#include "cmdlib.h"
#include "d_net.h"
#include "v_text.h"
#include "p_lnspec.h"
#include "v_video.h"
// [BC] New #includes.
#include "deathmatch.h"
#include "cl_demo.h"
#include "cl_main.h"
#include "network.h"
#include "p_local.h"
#include "p_acs.h"
#include "team.h"
#include "maprotation.h"
#include "cl_commands.h"
#include "cooperative.h"
#include "survival.h"
#include "m_cheat.h"

extern FILE *Logfile;
extern bool insave;

// [BC] The user's desired name of the logfile.
extern char g_szDesiredLogFilename[256];

// [RC] The actual name of the logfile (most likely g_szDesiredLogFilename with a timestamp).
extern char g_szActualLogFilename[256];


CVAR (Bool, sv_cheats, false, CVAR_SERVERINFO | CVAR_LATCH)
CVAR (Bool, sv_unlimited_pickup, false, CVAR_SERVERINFO)
CVAR (Bool, sv_logfilenametimestamp, true, CVAR_ARCHIVE)
CVAR (Bool, sv_logfile_append, false, CVAR_ARCHIVE)

CCMD (toggleconsole)
{
	C_ToggleConsole();
}

bool CheckCheatmode (bool printmsg)
{
	if ((( G_SkillProperty( SKILLP_DisableCheats )) ||
		NETWORK_InClientMode() ||
		( NETWORK_GetState( ) == NETSTATE_SERVER )) &&
		( sv_cheats == false ))
	{
		if (printmsg)
		{
			// [BB] Don't allow clients to lag themself by flooding the cheat message.
			if ( ( NETWORK_InClientMode() == false ) || CLIENT_AllowSVCheatMessage( ) )
 				Printf ("sv_cheats must be true to enable this command.\n");
 		}
		return true;
	}
	else
	{
		return false;
	}
}

// [RC] Creates the log file and starts logging to it.
void StartLogging( const char *szFileName )
{
	char		logfilename[512];
	time_t		tNow;
	// [BB] Write the current system time to tNow.
	time (&tNow);
	struct tm	*lt = localtime( &tNow );

	// [BB] If sv_logfilenametimestamp, we append the current date/time to the logfile name.
	if ( lt != NULL && sv_logfilenametimestamp )
		sprintf( logfilename, "%s__%d_%02d_%02d-%02d_%02d_%02d.log", szFileName, lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday, lt->tm_hour, lt->tm_min, lt->tm_sec);
	else
		strncpy( logfilename, szFileName, 256 );

	if (( Logfile = fopen( logfilename, sv_logfile_append ? "a" : "w" )))
	{
		sprintf( g_szDesiredLogFilename, "%s", szFileName );
		sprintf( g_szActualLogFilename, "%s", logfilename );
		Printf( "Log started: %s, %s", g_szActualLogFilename, myasctime( ));
	}
	else
		Printf( "Could not start log: %s.\n", szFileName );
}

// Closes the log file, if there is one.
void StopLogging( void )
{
	if ( Logfile )
	{
		Printf( "Log stopped: %s, %s", g_szActualLogFilename, myasctime() );
		fclose( Logfile );
		Logfile = NULL;
		g_szActualLogFilename[0] = 0;
	}
}

CCMD (quit)
{
	// [BC] This function may not be used by ConsoleCommand.
	if ( ACS_IsCalledFromConsoleCommand( ))
		return;

	if (!insave) exit (0);
}

CCMD (exit)
{
	// [BC] This function may not be used by ConsoleCommand.
	if ( ACS_IsCalledFromConsoleCommand( ))
		return;

	if (!insave) exit (0);
}

/*
==================
Cmd_God

Sets client to godmode

argv(0) god
==================
*/
CCMD (god)
{
	if (CheckCheatmode ())
		return;

	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENTCOMMANDS_GenericCheat( CHT_GOD );
	else
	{
		Net_WriteByte (DEM_GENERICCHEAT);
		Net_WriteByte (CHT_GOD);
	}
}

CCMD (iddqd)
{
	if (CheckCheatmode ())
		return;

	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENTCOMMANDS_GenericCheat( CHT_IDDQD );
	else
	{
		Net_WriteByte (DEM_GENERICCHEAT);
		Net_WriteByte (CHT_IDDQD);
	}
}

// [BC] Allow users to execute the "idfa" cheat from the console.
CCMD( idfa )
{
	if ( CheckCheatmode( ))
		return;

	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENTCOMMANDS_GenericCheat( CHT_IDFA );
	else
	{
		Net_WriteByte (DEM_GENERICCHEAT);
		Net_WriteByte (CHT_IDFA);
	}
}

// [BC] Allow users to execute the "idkfa" cheat from the console.
CCMD( idkfa )
{
	if ( CheckCheatmode( ))
		return;

	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENTCOMMANDS_GenericCheat( CHT_IDKFA );
	else
	{
		Net_WriteByte (DEM_GENERICCHEAT);
		Net_WriteByte (CHT_IDKFA);
	}
}

// [BC] Allow users to execute the "idchoppers" cheat from the console.
CCMD( idchoppers )
{
	if ( CheckCheatmode( ))
		return;

	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENTCOMMANDS_GenericCheat( CHT_CHAINSAW );
	else
	{
		Net_WriteByte (DEM_GENERICCHEAT);
		Net_WriteByte (CHT_CHAINSAW);
	}
}

CCMD (buddha)
{
	if (CheckCheatmode())
		return;

	// [BB] Clients need to request the cheat from the server.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		CLIENTCOMMANDS_GenericCheat( CHT_BUDDHA );
		return;
	}

	Net_WriteByte(DEM_GENERICCHEAT);
	Net_WriteByte(CHT_BUDDHA);
}

CCMD (notarget)
{
	if (CheckCheatmode ())
		return;

	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENTCOMMANDS_GenericCheat( CHT_NOTARGET );
	else
	{
		Net_WriteByte (DEM_GENERICCHEAT);
		Net_WriteByte (CHT_NOTARGET);
	}
}

//
// [TP] CheckOnlineCheat
//
// Online handling code common for noclip, noclip2 and fly.
// Returns true if the cheat command was handled.
//
static bool CheckOnlineCheat ( ECheatCommand cheat )
{
	if (( cheat != CHT_NOCLIP ) && ( cheat != CHT_NOCLIP2 ) && ( cheat != CHT_FLY ))
		return false;

	// [BB] Allow spectators to use noclip.
	// [Leo] Allow spectators to use the fly command.
	if ( ( CLIENTDEMO_IsInFreeSpectateMode( ) == false )
		&& ( players[consoleplayer].bSpectating == false )
		&& CheckCheatmode( ) )
	{
		return true;
	}

	// [BB] Clients need to request the cheat from the server.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		// [TP] Don't bother sending a request to the server when we're spectating.
		// Spectator movement is entirely clientside and thus we can run this cheat on our own.
		if ( players[consoleplayer].bSpectating )
		{
			cht_DoCheat( &players[consoleplayer], cheat );

			// [TP] If we are recording a demo, we need to write this noclip use down
			// so that it can be re-enacted correctly
			if ( CLIENTDEMO_IsRecording( ))
				CLIENTDEMO_WriteCheat( cheat );
		}
		else
			CLIENTCOMMANDS_GenericCheat( cheat );

		return true;
	}
	else if ( CLIENTDEMO_IsInFreeSpectateMode( ) )
	{
		cht_DoCheat( CLIENTDEMO_GetFreeSpectatorPlayer( ), cheat );
		return true;
	}

	return false;
}

CCMD (fly)
{
	// [TP] Check if this noclip needs to be handled some other way
	if ( CheckOnlineCheat( CHT_FLY ))
		return;

	if (CheckCheatmode ())
		return;

	Net_WriteByte (DEM_GENERICCHEAT);
	Net_WriteByte (CHT_FLY);
}

/*
==================
Cmd_Noclip

argv(0) noclip
==================
*/
CCMD (noclip)
{
	// [TP] Check if this noclip needs to be handled some other way
	if ( CheckOnlineCheat( CHT_NOCLIP ))
		return;

	if (CheckCheatmode ())
		return;

	Net_WriteByte (DEM_GENERICCHEAT);
	Net_WriteByte (CHT_NOCLIP);
}

CCMD (noclip2)
{
	// [TP] Check if this noclip needs to be handled some other way
	if ( CheckOnlineCheat( CHT_NOCLIP2 ))
		return;

	if (CheckCheatmode ())
		return;

	Net_WriteByte (DEM_GENERICCHEAT);
	Net_WriteByte (CHT_NOCLIP2);
}

CCMD (powerup)
{
	if (CheckCheatmode ())
		return;

	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENTCOMMANDS_GenericCheat( CHT_POWER );
	else
	{
		Net_WriteByte (DEM_GENERICCHEAT);
		Net_WriteByte (CHT_POWER);
	}
}

CCMD (morphme)
{
	if (CheckCheatmode ())
		return;

	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		if (argv.argc() == 1)
			CLIENTCOMMANDS_GenericCheat( CHT_MORPH );
		else
			CLIENTCOMMANDS_MorphCheat( argv[1] );
	}
	else
	{
		if (argv.argc() == 1)
		{
			Net_WriteByte (DEM_GENERICCHEAT);
			Net_WriteByte (CHT_MORPH);
		}
		else
		{
			Net_WriteByte (DEM_MORPHEX);
			Net_WriteString (argv[1]);
		}
	}
}

CCMD (anubis)
{
	if (CheckCheatmode ())
		return;

	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		CLIENTCOMMANDS_GenericCheat( CHT_ANUBIS );
	else
	{
		Net_WriteByte (DEM_GENERICCHEAT);
		Net_WriteByte (CHT_ANUBIS);
	}
}

// [GRB]
CCMD (resurrect)
{
	if (CheckCheatmode ())
		return;

	// [TP]
	if ( NETWORK_GetState() == NETSTATE_CLIENT )
		CLIENTCOMMANDS_GenericCheat( CHT_RESSURECT );
	else
	{
		Net_WriteByte (DEM_GENERICCHEAT);
		Net_WriteByte (CHT_RESSURECT);
	}
}

EXTERN_CVAR (Bool, chasedemo)

CCMD (chase)
{
	// [BC] Support for client-side demos.
	if (demoplayback || ( CLIENTDEMO_IsPlaying( )))
	{
		int i;

		if (chasedemo)
		{
			chasedemo = false;
			for (i = 0; i < MAXPLAYERS; i++)
				players[i].cheats &= ~CF_CHASECAM;
		}
		else
		{
			chasedemo = true;
			for (i = 0; i < MAXPLAYERS; i++)
				players[i].cheats |= CF_CHASECAM;
		}
		R_ResetViewInterpolation ();
	}
	else
	{
		// Check if we're allowed to use chasecam.
		// [BB] Unlike ZDoom we disallow chasecam by default in all game modes.
		// [BB] Always allow chasecam for spectators. CheckCheatmode has to be checked last
		// because it prints a message if cheats are not allowed.
		if (gamestate != GS_LEVEL || (!(dmflags2 & DF2_CHASECAM)
			&& (players[consoleplayer].bSpectating == false)
			&& CheckCheatmode () ) )
			return;

		if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
			CLIENTCOMMANDS_GenericCheat( CHT_CHASECAM );
		else
		{
			Net_WriteByte (DEM_GENERICCHEAT);
			Net_WriteByte (CHT_CHASECAM);
		}
	}
}

CCMD (idclev)
{
	// [BB] Check if client instead of "netgame"
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		return;

	if ((argv.argc() > 1) && (*(argv[1] + 2) == 0) && *(argv[1] + 1) && *argv[1])
	{
		int epsd, map;
		char buf[2];
		FString mapname;

		buf[0] = argv[1][0] - '0';
		buf[1] = argv[1][1] - '0';

		if (gameinfo.flags & GI_MAPxx)
		{
			epsd = 1;
			map = buf[0]*10 + buf[1];
		}
		else
		{
			epsd = buf[0];
			map = buf[1];
		}

		// Catch invalid maps.
		mapname = CalcMapName (epsd, map);

		if (!P_CheckMapData(mapname))
			return;

		// So be it.
		Printf ("%s\n", GStrings("STSTR_CLEV"));
      	G_DeferedInitNew (mapname);
		//players[0].health = 0;		// Force reset
	}
}

CCMD (hxvisit)
{
	// [BB] Check if client instead of "netgame"
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		return;

	if ((argv.argc() > 1) && (*(argv[1] + 2) == 0) && *(argv[1] + 1) && *argv[1])
	{
		FString mapname("&wt@");

		mapname << argv[1][0] << argv[1][1];

		if (CheckWarpTransMap (mapname, false))
		{
			// Just because it's in MAPINFO doesn't mean it's in the wad.

			if (P_CheckMapData(mapname))
			{
				// So be it.
				Printf ("%s\n", GStrings("STSTR_CLEV"));
      			G_DeferedInitNew (mapname);
				return;
			}
		}
		Printf ("No such map found\n");
	}
}

CCMD (changemap)
{
	if ( level.info == NULL )
	{
		Printf( "You can't use changemap, when not in a level.\n" );
		return;
	}

	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		Printf( "Only the server can change the map.\n" );
		return;
	}

	if (argv.argc() > 1)
	{
		try
		{
			if (!P_CheckMapData(argv[1]))
			{
				Printf ("No map %s\n", argv[1]);
			}
			else
			{
				// [BB] We cannot end the map during survival's countdown, so just end the map after the countdown ends.
				if ( ( survival ) && ( SURVIVAL_GetState( ) == SURVS_COUNTDOWN ) )
				{
					char commandString[128];
					sprintf ( commandString, "wait %lu;changemap %s", SURVIVAL_GetCountdownTicks() + TICRATE, argv[1] );
					Printf ( "changemap called during a survival countdown. Delaying the map change till the countdown ends.\n" );
					AddCommandString ( commandString );
					return;
				}

				// Fuck that DEM shit!
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					strncpy( level.nextmap, argv[1], 8 );

					level.flags |= LEVEL_CHANGEMAPCHEAT;

					G_ExitLevel( 0, false );
				}
				else
				{
					if (argv.argc() > 2)
					{
						Net_WriteByte (DEM_CHANGEMAP2);
						Net_WriteByte (atoi(argv[2]));
					}
					else
					{
						Net_WriteByte (DEM_CHANGEMAP);
					}
					Net_WriteString (argv[1]);
				}
			}
		}
		catch(CRecoverableError &error)
		{
			if (error.GetMessage())
				Printf("%s", error.GetMessage());
		}
	}
	else
	{
		Printf ("Usage: changemap <map name> [position]\n");
	}
}

CCMD (give)
{
	if (CheckCheatmode () || argv.argc() < 2)
		return;

	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		if ( argv.argc( ) > 2 )
			CLIENTCOMMANDS_GiveCheat( argv[1], clamp( atoi( argv[2]), 1, 255 ));
		else
			CLIENTCOMMANDS_GiveCheat( argv[1], 0 );
	}
	else
	{
		Net_WriteByte (DEM_GIVECHEAT);
		Net_WriteString (argv[1]);
		if (argv.argc() > 2)
			Net_WriteWord (clamp (atoi (argv[2]), 1, 32767));
		else
			Net_WriteWord (0);
	}
}

CCMD (take)
{
	if (CheckCheatmode () || argv.argc() < 2)
		return;

	// [TP] Handle take cheat online
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		if ( argv.argc( ) > 2 )
			CLIENTCOMMANDS_TakeCheat( argv[1], clamp( atoi( argv[2]), 1, 255 ));
		else
			CLIENTCOMMANDS_TakeCheat( argv[1], 0 );
		return;
	}

	Net_WriteByte (DEM_TAKECHEAT);
	Net_WriteString (argv[1]);
	if (argv.argc() > 2)
		Net_WriteWord (clamp (atoi (argv[2]), 1, 32767));
	else
		Net_WriteWord (0);
}

CCMD (gameversion)
{
	Printf ("%s @ %s\nCommit %s", GetVersionString(), GetGitTime(), GetGitHash());
}

CCMD (print)
{
	if (argv.argc() != 2)
	{
		Printf ("print <name>: Print a string from the string table\n");
		return;
	}
	const char *str = GStrings[argv[1]];
	if (str == NULL)
	{
		Printf ("%s unknown\n", argv[1]);
	}
	else
	{
		Printf ("%s\n", str);
	}
}

CCMD (exec)
{
	if (argv.argc() < 2)
		return;

	for (int i = 1; i < argv.argc(); ++i)
	{
		if (!C_ExecFile(argv[i]))
		{
			Printf ("Could not exec \"%s\"\n", argv[i]);
			break;
		}
	}
}

void execLogfile(const char *fn)
{
	if ((Logfile = fopen(fn, "w")))
	{
		const char *timestr = myasctime();
		Printf("Log started: %s\n", timestr);
	}
	else
	{
		Printf("Could not start log\n");
	}
}

CCMD (logfile)
{
	// [BC/BB] This function may not be used by ConsoleCommand.
	if ( ACS_IsCalledFromConsoleCommand( ))
		return;

	// [BB] Zandronum handles logging differently.
	if ( Logfile )
		StopLogging( );

	if ( argv.argc() >= 2 )
		StartLogging( argv[1] );
}

// [TP] CCMD(puke)'s online handling encapsulated into a function.
// Returns true if the function eats the script call.
static bool CheckOnlinePuke ( int script, int args[4], bool always )
{
	if ( NETWORK_InClientMode() == false && ( NETWORK_GetState( ) != NETSTATE_SERVER ))
		return false;

	// [BB] The check if the client is allowed to puke a CLIENTSIDE script
	// is done in P_StartScript, no need to check here.
	if ( ( NETWORK_GetState( ) == NETSTATE_SERVER ) || ACS_IsScriptClientSide ( script ) )
	{
		P_StartScript( players[consoleplayer].mo, NULL, script, level.mapname,
			args, 4, ( (script < 0 ) ? ACS_ALWAYS : 0 ) | ACS_NET );

		// [BB] If the server (and not any ACS script via ConsoleCommand) calls puke, let the clients know.
		if ( ( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( ACS_IsCalledFromConsoleCommand( ) == false ) )
		{
			SERVER_Printf( "The Server host or an RCON user is possibly cheating "
				"by calling \"puke %s %d %d %d %d\"\n",
				FBehavior::RepresentScript( script ).GetChars(),
				args[0], args[1], args[2], args[3] );
		}
	}
	else if ( ( NETWORK_GetState( ) == NETSTATE_CLIENT ) && ACS_IsScriptPukeable ( script ) )
	{
		CLIENTCOMMANDS_Puke ( script, args, always );
	}

	return true;
}

CCMD (puke)
{
	int argc = argv.argc();

	if (argc < 2 || argc > 6)
	{
		Printf ("Usage: puke <script> [arg1] [arg2] [arg3] [arg4]\n");
	}
	else
	{
		int script = atoi (argv[1]);

		if (script == 0)
		{ // Script 0 is reserved for Strife support. It is not pukable.
			return;
		}
		int arg[4] = { 0, 0, 0, 0 };
		int argn = MIN<int>(argc - 2, countof(arg)), i;

		for (i = 0; i < argn; ++i)
		{
			arg[i] = atoi (argv[2+i]);
		}

		// [TP] Check online handling
		if ( CheckOnlinePuke( abs( script ), arg, script < 0 ) )
			return;

		if (script > 0)
		{
			Net_WriteByte (DEM_RUNSCRIPT);
			Net_WriteWord (script);
		}
		else
		{
			Net_WriteByte (DEM_RUNSCRIPT2);
			Net_WriteWord (-script);
		}
		Net_WriteByte (argn);
		for (i = 0; i < argn; ++i)
		{
			Net_WriteLong (arg[i]);
		}
	}
}

CCMD (pukename)
{
	int argc = argv.argc();

	if (argc < 2 || argc > 7)
	{
		Printf ("Usage: pukename \"<script>\" [\"always\"] [arg1] [arg2] [arg3] [arg4]\n");
	}
	else
	{
		bool always = false;
		int argstart = 2;
		int arg[4] = { 0, 0, 0, 0 };
		int argn = 0, i;
		
		if (argc > 2)
		{
			if (stricmp(argv[2], "always") == 0)
			{
				always = true;
				argstart = 3;
			}
			argn = MIN<int>(argc - argstart, countof(arg));
			for (i = 0; i < argn; ++i)
			{
				arg[i] = atoi(argv[argstart + i]);
			}
		}

		// [TP] Check online handling
		if ( CheckOnlinePuke( -FName( argv[1] ), arg, always ) )
			return;

		Net_WriteByte(DEM_RUNNAMEDSCRIPT);
		Net_WriteString(argv[1]);
		Net_WriteByte(argn | (always << 7));
		for (i = 0; i < argn; ++i)
		{
			Net_WriteLong(arg[i]);
		}
	}
}

CCMD (special)
{
	int argc = argv.argc();

	if (argc < 2 || argc > 7)
	{
		Printf("Usage: special <special-name> [arg1] [arg2] [arg3] [arg4] [arg5]\n");
	}
	else
	{
		int specnum;

		if (argv[1][0] >= '0' && argv[1][0] <= '9')
		{
			specnum = atoi(argv[1]);
			if (specnum < 0 || specnum > 255)
			{
				Printf("Bad special number\n");
				return;
			}
		}
		else
		{
			int min_args;
			specnum = P_FindLineSpecial(argv[1], &min_args);
			if (specnum == 0 || min_args < 0)
			{
				Printf("Unknown special\n");
				return;
			}
			if (argc < 2 + min_args)
			{
				Printf("%s needs at least %d argument%s\n", argv[1], min_args, min_args == 1 ? "" : "s");
				return;
			}
		}

		// [TP] Online handling for this cheat
		if ( NETWORK_GetState() == NETSTATE_CLIENT )
		{
			TArray<int> args;
			for ( int i = 2; i < argc; ++i )
				args.Push( atoi( argv[i] ));
			CLIENTCOMMANDS_SpecialCheat( specnum, args );
			return;
		}

		Net_WriteByte(DEM_RUNSPECIAL);
		Net_WriteByte(specnum);
		Net_WriteByte(argc - 2);
		for (int i = 2; i < argc; ++i)
		{
			Net_WriteLong(atoi(argv[i]));
		}
	}
}

CCMD (error)
{
	// [BB] This function may not be used by ConsoleCommand.
	if ( ACS_IsCalledFromConsoleCommand( ))
		return;

	if (argv.argc() > 1)
	{
		char *textcopy = copystring (argv[1]);
		I_Error ("%s", textcopy);
	}
	else
	{
		Printf ("Usage: error <error text>\n");
	}
}

CCMD (error_fatal)
{
	// [BB] This function may not be used by ConsoleCommand.
	if ( ACS_IsCalledFromConsoleCommand( ))
		return;

	if (argv.argc() > 1)
	{
		char *textcopy = copystring (argv[1]);
		I_FatalError ("%s", textcopy);
	}
	else
	{
		Printf ("Usage: error_fatal <error text>\n");
	}
}

//==========================================================================
//
// CCMD crashout
//
// Debugging routine for testing the crash logger.
// Useless in a win32 debug build, because that doesn't enable the crash logger.
//
//==========================================================================

#if !defined(_WIN32) || !defined(_DEBUG)
CCMD (crashout)
{
	// [BB] This function may not be used by ConsoleCommand.
	if ( ACS_IsCalledFromConsoleCommand( ))
		return;

	*(volatile int *)0 = 0;
}
#endif


CCMD (dir)
{
	FString dir, path;
	char curdir[256];
	const char *match;
	findstate_t c_file;
	void *file;

	if (!getcwd (curdir, countof(curdir)))
	{
		Printf ("Current path too long\n");
		return;
	}

	if (argv.argc() > 1)
	{
		path = NicePath(argv[1]);
		if (chdir(path))
		{
			match = path;
			dir = ExtractFilePath(path);
			if (dir[0] != '\0')
			{
				match += dir.Len();
			}
			else
			{
				dir = "./";
			}
			if (match[0] == '\0')
			{
				match = "*";
			}
			if (chdir (dir))
			{
				Printf ("%s not found\n", dir.GetChars());
				return;
			}
		}
		else
		{
			match = "*";
			dir = path;
		}
	}
	else
	{
		match = "*";
		dir = curdir;
	}
	if (dir[dir.Len()-1] != '/')
	{
		dir += '/';
	}

	if ( (file = I_FindFirst (match, &c_file)) == ((void *)(-1)))
		Printf ("Nothing matching %s%s\n", dir.GetChars(), match);
	else
	{
		Printf ("Listing of %s%s:\n", dir.GetChars(), match);
		do
		{
			if (I_FindAttr (&c_file) & FA_DIREC)
				Printf (PRINT_BOLD, "%s <dir>\n", I_FindName (&c_file));
			else
				Printf ("%s\n", I_FindName (&c_file));
		} while (I_FindNext (file, &c_file) == 0);
		I_FindClose (file);
	}

	chdir (curdir);
}

CCMD (fov)
{
	player_t *player = who ? who->player : &players[consoleplayer];

	if (argv.argc() != 2)
	{
		Printf ("fov is %g\n", player->DesiredFOV);
		return;
	}
	else if (dmflags & DF_NO_FOV)
	{
		if (consoleplayer == Net_Arbitrator)
		{
			Net_WriteByte (DEM_FOV);
		}
		else
		{
			Printf ("A setting controller has disabled FOV changes.\n");
			return;
		}
	}
	else
	{
		// Just do this here in client games.
		if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
			player->DesiredFOV = static_cast<float> ( clamp (atoi (argv[1]), 5, 179) );

		Net_WriteByte (DEM_MYFOV);
	}
	Net_WriteByte (clamp (atoi (argv[1]), 5, 179));
}

//==========================================================================
//
// CCMD warp
//
// Warps to a specific location on a map
//
//==========================================================================

CCMD (warp)
{
	// [TP] The server cannot warp
	if ( NETWORK_GetState() == NETSTATE_SERVER )
		return;

	// [TP] Let spectators warp regardless of cheats (the spectator check must come first so that the CheckCheatmode
	// call is eliminated by lazy evaluation to avoid the sv_cheats message)
	if (( players[consoleplayer].bSpectating == false ) && CheckCheatmode ())
	{
		return;
	}
	if (gamestate != GS_LEVEL)
	{
		Printf ("You can only warp inside a level.\n");
		return;
	}
	if (argv.argc() != 3)
	{
		Printf ("Usage: warp <x> <y>\n");
	}
	else
	{
		// [TP] Handle warp cheat online.
		fixed_t x = FLOAT2FIXED( atof( argv[1] ));
		fixed_t y = FLOAT2FIXED( atof( argv[2] ));
		if (( NETWORK_GetState() == NETSTATE_CLIENT ) && ( players[consoleplayer].bSpectating == false ))
		{
			CLIENTCOMMANDS_WarpCheat( x, y );
			return;
		}

		if ( CLIENTDEMO_IsRecording() )
			CLIENTDEMO_WriteWarpCheat( x, y );

		P_TeleportMove (players[consoleplayer].mo, fixed_t(atof(argv[1])*65536.0), fixed_t(atof(argv[2])*65536.0), ONFLOORZ, true);
	}
}

//==========================================================================
//
// CCMD load
//
// Load a saved game.
//
//==========================================================================

CCMD (load)
{
    // [BB] This function may not be used by ConsoleCommand.
    if ( ACS_IsCalledFromConsoleCommand() )
        return;

    if (argv.argc() != 2)
	{
        Printf ("usage: load <filename>\n");
        return;
    }
	if (( NETWORK_GetState( ) != NETSTATE_SINGLE ) && ( NETWORK_GetState( ) != NETSTATE_SINGLE_MULTIPLAYER ))
	{
		Printf ("cannot load during a network game\n");
		return;
	}
	FString fname = argv[1];
	DefaultExtension (fname, ".zds");
    G_LoadGame (fname);
}

//==========================================================================
//
// CCMD save
//
// Save the current game.
//
//==========================================================================

CCMD (save)
{
    // [BB] This function may not be used by ConsoleCommand.
    if ( ACS_IsCalledFromConsoleCommand() )
        return;

    if (argv.argc() < 2 || argv.argc() > 3)
	{
        Printf ("usage: save <filename> [description]\n");
        return;
    }
    if (!usergame)
	{
        Printf ("not in a saveable game\n");
        return;
    }
    if (gamestate != GS_LEVEL)
	{
        Printf ("not in a level\n");
        return;
    }
    if(players[consoleplayer].health <= 0 && ( NETWORK_GetState( ) == NETSTATE_SINGLE ))
    {
        Printf ("player is dead in a single-player game\n");
        return;
    }

	// [BB] No saving in multiplayer.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		Printf ("You cannot save the game in multiplayer.\n");
		return;
	}

	// [BB] Saving bots is not supported yet.
	if ( BOTS_CountBots() > 0 )
	{
		Printf ("You cannot save the game while bots are in use.\n");
		return;
	}

    FString fname = argv[1];
	DefaultExtension (fname, ".zds");
	G_SaveGame (fname, argv.argc() > 2 ? argv[2] : argv[1]);
}

//==========================================================================
//
// CCMD wdir
//
// Lists the contents of a loaded wad file.
//
//==========================================================================

CCMD (wdir)
{
	if (argv.argc() != 2)
	{
		Printf ("usage: wdir <wadfile>\n");
		return;
	}
	int wadnum = Wads.CheckIfWadLoaded (argv[1]);
	if (wadnum < 0)
	{
		Printf ("%s must be loaded to view its directory.\n", argv[1]);
		return;
	}
	for (int i = 0; i < Wads.GetNumLumps(); ++i)
	{
		if (Wads.GetLumpFile(i) == wadnum)
		{
			Printf ("%s\n", Wads.GetLumpFullName(i));
		}
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------
CCMD(linetarget)
{
	AActor *linetarget;

	if (CheckCheatmode () || players[consoleplayer].mo == NULL) return;
	P_AimLineAttack(players[consoleplayer].mo,players[consoleplayer].mo->angle,MISSILERANGE, &linetarget, 0);
	if (linetarget)
	{
		// [TP] If we're the client, ask the server for information about the linetarget.
		if ( NETWORK_GetState() == NETSTATE_CLIENT && linetarget->lNetID != -1 )
		{
			CLIENTCOMMANDS_InfoCheat( linetarget, false );
			return;
		}

		Printf("Target=%s, Health=%d, Spawnhealth=%d\n",
			linetarget->GetClass()->TypeName.GetChars(),
			linetarget->health,
			linetarget->SpawnHealth());
	}
	else Printf("No target found\n");
}

// As linetarget, but also give info about non-shootable actors
CCMD(info)
{
	AActor *linetarget;

	if (CheckCheatmode () || players[consoleplayer].mo == NULL) return;
	P_AimLineAttack(players[consoleplayer].mo,players[consoleplayer].mo->angle,MISSILERANGE, 
		&linetarget, 0,	ALF_CHECKNONSHOOTABLE|ALF_FORCENOSMART);
	if (linetarget)
	{
		// [TP] If we're the client, ask the server for information about the linetarget.
		if ( NETWORK_GetState() == NETSTATE_CLIENT && linetarget->lNetID != -1 )
		{
			CLIENTCOMMANDS_InfoCheat( linetarget, true );
			return;
		}

		Printf("Target=%s, Health=%d, Spawnhealth=%d\n",
			linetarget->GetClass()->TypeName.GetChars(),
			linetarget->health,
			linetarget->SpawnHealth());
		PrintMiscActorInfo(linetarget);
	}
	else Printf("No target found. Info cannot find actors that have\
				the NOBLOCKMAP flag or have height/radius of 0.\n");
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------
CCMD(monster)
{
	AActor * mo;

	if (CheckCheatmode ()) return;
	TThinkerIterator<AActor> it;

	while ( (mo = it.Next()) )
	{
		if (mo->flags3&MF3_ISMONSTER && !(mo->flags&MF_CORPSE) && !(mo->flags&MF_FRIENDLY))
		{
			Printf ("%s at (%d,%d,%d)\n",
				mo->GetClass()->TypeName.GetChars(),
				mo->x >> FRACBITS, mo->y >> FRACBITS, mo->z >> FRACBITS);
		}
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------
CCMD(items)
{
	AActor * mo;

	if (CheckCheatmode ()) return;
	TThinkerIterator<AActor> it;

	while ( (mo = it.Next()) )
	{
		if (mo->IsKindOf(RUNTIME_CLASS(AInventory)) && mo->flags&MF_SPECIAL)
		{
			Printf ("%s at (%d,%d,%d)\n",
				mo->GetClass()->TypeName.GetChars(),
				mo->x >> FRACBITS, mo->y >> FRACBITS, mo->z >> FRACBITS);
		}
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------
CCMD(changesky)
{
	const char *sky1name;

	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) || ( NETWORK_GetState( ) == NETSTATE_SERVER ) || argv.argc()<2) return;

	sky1name = argv[1];
	if (sky1name[0] != 0)
	{
		strncpy (level.skypic1, sky1name, 8);
		sky1texture = TexMan.GetTexture (sky1name, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable);
	}
	R_InitSkyMap ();
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------
CCMD(thaw)
{
	if (CheckCheatmode())
		return;

	// [BB] Clients need to request the cheat from the server.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		CLIENTCOMMANDS_GenericCheat( CHT_CLEARFROZENPROPS );
		return;
	}

	Net_WriteByte (DEM_GENERICCHEAT);
	Net_WriteByte (CHT_CLEARFROZENPROPS);
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

CCMD(nextmap)
{
	if ( level.info == NULL )
	{
		Printf( "You can't use nextmap, when not in a level.\n" );
		return;
	}

	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		Printf( "Only the server can change the map.\n" );
		return;
	}

	// [TL] Get the next map if available.
	const char * next = G_GetExitMap( );	
	
	// [BB] G_GetExitMap() can return an empty string.
	if ( next && strncmp(next, "enDSeQ", 6) && ( strlen ( next ) > 0 ) )
	{	
		// Fuck that DEM shit!
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			G_ExitLevel( 0, false );
		}
		else
		{
			if (argv.argc() > 1)
			{
				Net_WriteByte (DEM_CHANGEMAP2);
				Net_WriteByte (atoi(argv[1]));
			}
			else
			{
				Net_WriteByte (DEM_CHANGEMAP);
			}			

			Net_WriteString( next );
		}
	}
	else
	{
		Printf( "No next map!\n" );
	}

/* [BB] For the time being I'll keep ST's nextmap version.
// [TL] I think it's safe to remove this (I've factored it in above).
	if (netgame)
	{
		Printf ("Use " TEXTCOLOR_BOLD "changemap" TEXTCOLOR_NORMAL " instead. " TEXTCOLOR_BOLD "Nextmap"
				TEXTCOLOR_NORMAL " is for single-player only.\n");
		return;
	}
	char *next = NULL;
	
	if (*level.nextmap)
		next = level.nextmap;

	if (next != NULL && strncmp(next, "enDSeQ", 6))
	{
		G_DeferedInitNew(next);
	}
	else
	{
		Printf("no next map!\n");
	}
*/
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------
CCMD(nextsecret)
{
	// [TL] Ensure a level is loaded.
	if ( level.info == NULL )
	{
		Printf( "You can't use nextsecret, when not in a level.\n" );
		return;
	}

	// [TL] Prevent clients from changing the map.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
	{
		Printf( "Only the server can change the map.\n" );
		return;
	}

	// [TL] Get the secret level or next map if not available.
	const char * next = G_GetSecretExitMap();
	
	// [BB] G_GetSecretExitMap() can return an empty string.
	if ( next && strncmp(next, "enDSeQ", 6) && ( strlen ( next ) > 0 ) )
	{
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			G_SecretExitLevel( 0 );
		}
		else
		{
			if (argv.argc() > 1)
			{
				Net_WriteByte (DEM_CHANGEMAP2);
				Net_WriteByte (atoi(argv[1]));
			}
			else
			{
				Net_WriteByte (DEM_CHANGEMAP);
			}
			Net_WriteString( next );
		}
	}
	else
	{
		Printf( "No next secret map!\n" );
	}
/* [BB] For the time being I'll keep ST's nextmap version.
	if (netgame)
	{
		Printf ("Use " TEXTCOLOR_BOLD "changemap" TEXTCOLOR_NORMAL " instead. " TEXTCOLOR_BOLD "Nextsecret"
				TEXTCOLOR_NORMAL " is for single-player only.\n");
		return;
	}
	char *next = NULL;
	
	if (*level.secretmap)
		next = level.secretmap;

	if (next != NULL && strncmp(next, "enDSeQ", 6))
	{
		G_DeferedInitNew(next);
	}
	else
	{
		Printf("no next secret map!\n");
	}
*/
}

//*****************************************************************************
//	Some console commands to emulate a multiplayer game while in single player mode.
CCMD( netgame )
{
	if ( NETWORK_GetState( ) != NETSTATE_SINGLE )
		Printf( "Multiplayer already enabled/emulated.\n" );
	else
	{
		NETWORK_SetState( NETSTATE_SINGLE_MULTIPLAYER );
		Printf( "Multiplayer emulation enabled.\n" );
	}
}

//*****************************************************************************
//
CCMD( multiplayer )
{
	C_DoCommand( "netgame", 0 );
}

//*****************************************************************************
//	A console command to emulate a single player game while in mutliplayer mode.
CCMD( singleplayer )
{
	if ( NETWORK_GetState( ) != NETSTATE_SINGLE_MULTIPLAYER )
		Printf( "Single player already enabled/emulated.\n" );
	else
	{
		ULONG ulCount = 0;

		for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++)
		{
			if (playeringame[ulCount])
				ulCount++;

			if (ulCount > 1)
			{
				Printf ("Cannot enable single player emulation as there are other players on the map!\n");
				return;
			}
		}

		NETWORK_SetState( NETSTATE_SINGLE );
		Printf( "Single player emulation enabled.\n" );
	}
}

//-----------------------------------------------------------------------------
//
// [BB]
//
//-----------------------------------------------------------------------------
CCMD(version_info)
{
	Printf ( "changeset: %s\n", GetGitHash() );
	const time_t hgDate = GetRevisionNumber();
	Printf ( "date:      %s\n", asctime ( gmtime ( &hgDate ) ) );
}

//-----------------------------------------------------------------------------
//
// [BB]
//
//-----------------------------------------------------------------------------
void CountActors ( )
{
	TMap<FName, int> actorCountMap;
	AActor * mo;
	int numActors = 0;
	int numActorsWithNetID = 0;

	TThinkerIterator<AActor> it;

	while ( (mo = it.Next()) )
	{
		numActors++;
		if ( mo->lNetID > 0 )
			numActorsWithNetID++;
		const FName curName = mo->GetClass()->TypeName.GetChars();
		if ( actorCountMap.CheckKey( curName ) == NULL )
			actorCountMap.Insert( curName, 1 );
		else
			actorCountMap [ curName ] ++;
	}

	const TMap<FName, int>::Pair *pair;

	Printf ( "%d actors in total found, %d have a NetID. Detailed listing:\n", numActors, numActorsWithNetID );

	TMap<FName, int>::ConstIterator mapit(actorCountMap);
	while (mapit.NextPair (pair))
	{
		Printf ( "%s %d\n", pair->Key.GetChars(), pair->Value );
	}
}

CCMD(countactors)
{
	CountActors ();
}

// [Cata] Executes cmd only if you are a spectator.
CCMD (ifspectator)
{
	// [BB] This function can call arbitrary other functions, so it may not be used by ConsoleCommand.
	if ( ACS_IsCalledFromConsoleCommand( ))
		return;

	if ( argv.argc() <= 1 )
	{
		Printf ( "Usage: ifspectator <command>\n" );
	}
	else if ( players[consoleplayer].bDeadSpectator || players[consoleplayer].bSpectating )
	{
		FString cmdTotal;
		for( int i = 1; i < argv.argc(); i++ )
		{
			cmdTotal += ' ';
			cmdTotal += argv[i];
		}

		
		AddCommandString(cmdTotal.LockBuffer());
		cmdTotal.UnlockBuffer();
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

CCMD(currentpos)
{
	AActor *mo = players[consoleplayer].mo;
	if(mo)
	{
		Printf("Current player position: (%1.3f,%1.3f,%1.3f), angle: %1.3f, floorheight: %1.3f, sector:%d, lightlevel: %d\n",
			FIXED2FLOAT(mo->x), FIXED2FLOAT(mo->y), FIXED2FLOAT(mo->z), mo->angle/float(ANGLE_1), FIXED2FLOAT(mo->floorz), mo->Sector->sectornum, mo->Sector->lightlevel);
	}
	else
	{
		Printf("You are not in game!");
	}
}

//-----------------------------------------------------------------------------
//
// Print secret info (submitted by Karl Murks)
//
//-----------------------------------------------------------------------------

static void PrintSecretString(const char *string, bool thislevel)
{
	const char *colstr = thislevel? TEXTCOLOR_YELLOW : TEXTCOLOR_CYAN;
	if (string != NULL)
	{
		if (*string == '$')
		{
			if (string[1] == 'S' || string[1] == 's')
			{
				long secnum = strtol(string+2, (char**)&string, 10);
				if (*string == ';') string++;
				if (thislevel && secnum >= 0 && secnum < numsectors)
				{
					if (sectors[secnum].secretsector)
					{
						if ((sectors[secnum].special & SECRET_MASK)) colstr = TEXTCOLOR_RED;
						else colstr = TEXTCOLOR_GREEN;
					}
					else colstr = TEXTCOLOR_ORANGE;
				}
			}
			else if (string[1] == 'T' || string[1] == 't')
			{
				long tid = strtol(string+2, (char**)&string, 10);
				if (*string == ';') string++;
				FActorIterator it(tid);
				AActor *actor;
				bool foundone = false;
				if (thislevel)
				{
					while ((actor = it.Next()))
					{
						if (!actor->IsKindOf(PClass::FindClass("SecretTrigger"))) continue;
						foundone = true;
						break;
					}
				}
				if (foundone) colstr = TEXTCOLOR_RED;
				else colstr = TEXTCOLOR_GREEN;
			}
		}
		FBrokenLines *brok = V_BreakLines(ConFont, screen->GetWidth()*95/100, string);

		for (int k = 0; brok[k].Width >= 0; k++)
		{
			Printf("%s%s\n", colstr, brok[k].Text.GetChars());
		}
		V_FreeBrokenLines(brok);
	}
}

//============================================================================
//
// Print secret hints
//
//============================================================================

CCMD(secret)
{
	const char *mapname = argv.argc() < 2? level.mapname : argv[1];
	bool thislevel = !stricmp(mapname, level.mapname);
	bool foundsome = false;

	int lumpno=Wads.CheckNumForName("SECRETS");
	if (lumpno < 0) return;

	FWadLump lump = Wads.OpenLumpNum(lumpno);
	FString maphdr;
	maphdr.Format("[%s]", mapname);

	FString linebuild;
	char readbuffer[1024];
	bool inlevel = false;

	while (lump.Gets(readbuffer, 1024))
	{
		if (!inlevel)
		{
			if (readbuffer[0] == '[')
			{
				inlevel = !strnicmp(readbuffer, maphdr, maphdr.Len());
				if (!foundsome)
				{
					FString levelname;
					level_info_t *info = FindLevelInfo(mapname);
					const char *ln = !(info->flags & LEVEL_LOOKUPLEVELNAME)? info->LevelName.GetChars() : GStrings[info->LevelName.GetChars()];
					levelname.Format("%s - %s\n", mapname, ln);
					size_t llen = levelname.Len() - 1;
					for(size_t ii=0; ii<llen; ii++) levelname += '-';
					Printf(TEXTCOLOR_YELLOW"%s\n", levelname.GetChars());
					foundsome = true;
				}
			}
			continue;
		}
		else
		{
			if (readbuffer[0] != '[')
			{
				linebuild += readbuffer;
				if (linebuild.Len() < 1023 || linebuild[1022] == '\n')
				{
					// line complete so print it.
					linebuild.Substitute("\r", "");
					linebuild.StripRight(" \t\n");
					PrintSecretString(linebuild, thislevel);
					linebuild = "";
				}
			}
			else inlevel = false;
		}
	}
}
