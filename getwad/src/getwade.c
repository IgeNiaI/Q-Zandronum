/*
 * Sample "driver" program for the GetWAD library.
 *
 * Written by Hippocrates Sendoukas, Athens, Greece, June-December 2003
 *
 * Copyright (C) 2003 Hippocrates Sendoukas.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the author be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * * The origin of this software must not be misrepresented; you must not
 *   claim that you wrote the original software. If you use this software
 *   in a product, an acknowledgment in the product documentation would be
 *   appreciated but is not required.
 *
 * * Altered source versions must be plainly marked as such, and must not be
 *   misrepresented as being the original software.
 *
 * * This notice may not be removed or altered from any source distribution.
 */
#if _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "getwad.h"
/*  <- skip_spaces */

static char *
skip_spaces(char *p)
{
  while (*p==' ' || *p=='\t')
    p++;
  return p;
}

/*  -> skip_spaces */
/*  <- get_arg */

static char *
get_arg(char *cmdline,char **arg)
{
  *arg = NULL;
  cmdline = skip_spaces(cmdline);
  if (*cmdline=='"')
    {
      *arg = ++cmdline;
      while (*cmdline && *cmdline!='"')
        cmdline++;
      if (*cmdline)
        *cmdline++ = '\0';
    }
  else if (*cmdline)
    {
      *arg = cmdline;
      while (*cmdline && *cmdline!=' ' && *cmdline!='\t')
        cmdline++;
      if (*cmdline)
        *cmdline++ = '\0';
    }
  return cmdline;
}

/*  -> get_arg */
/*  <- err_msg */

static int
err_msg(char *msg)
{
  MessageBox(0,msg,"GetWAD",MB_ICONSTOP);
  return 1;
}

/*  -> err_msg */
/*  <- WinMain */

int PASCAL
WinMain(HINSTANCE hinstance,HINSTANCE hprevinst,LPSTR cmdline,int cmdshow)
{
  char	*wad_dir;
  static char *no_init_msg =
    "The program has not been initialized. Run the "
    "\"GetWAD Setup\" command from the \"GetWAD\" "
    "program group.";
  static char *usage_msg = "Usage: getwad [-setup] [-d wad_directory] [wad_name [wad_name ...]]";

  cmdline = skip_spaces(cmdline);
  if (!strcmp(cmdline,"-setup"))
    {
      getwad2_setup(0,TRUE,NULL,NULL);
      return 0;
    }

  wad_dir = NULL;
  if (!memcmp(cmdline,"-d",2) || !memcmp(cmdline,"-D",2))
    {
      cmdline = get_arg(cmdline+2,&wad_dir);
      if (!wad_dir || *wad_dir=='\0') return err_msg(usage_msg);
    }
  if (getwad2_fetch(0,skip_spaces(cmdline),wad_dir,FALSE,NULL,NULL,NULL) < 0)
    return err_msg(no_init_msg);
  return 0;
}

/*  -> WinMain */
#else
/*  <- main */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "getwad.h"
#include "misc.h"

int main(int ac,char *av[])
{
  int	i,n;
  char	*wad_dir, *p, *q;

  wad_dir = NULL;
  if (ac>1)
  {
    if (av[1][0]=='-' && (av[1][1]=='d' || av[1][1]=='D'))
      {
      	if (av[1][2])
      	  {
      	    wad_dir = av[1]+2;
      	    ac--;
      	    av++;
      	  }
      	else if (ac>2)
      	  {
      	    wad_dir = av[2];
      	    ac -= 2;
      	    av += 2;
          }
        else
          {
            USAGE:
            printf("Usage: getwad [-d wad_directory] wad_name [wad_name ...]\n");
            return 1;
          }
        if (wad_dir[0]=='\0') goto USAGE;
      }
  }
  if (ac<2) goto USAGE;
  n = 0;
  for (i=1; i<ac; i++)
    n += 1+strlen(av[i]);
  if ( (p=malloc(n+1)) == NULL )
    {
      printf("getwad: out of memory\n");
      return 1;
    }
  q = p;
  for (i=1; i<ac; i++)
    {
      if (i>1) *q++ = ' ';
      strcpy(q,av[i]);
      q += strlen(q);
    }
  n = getwad2_fetch(p,wad_dir,NULL,NULL,NULL);
  free(p);
  if (n<0)
    {
      printf("The program has not been properly initialized.\n"
             "Edit /usr/local/etc/getwad.ini to specify the WAD directory.\n");
      return 1;
    }
  else if (n==0) return 2;
  else return 0;
}

/*  -> main */
#endif
