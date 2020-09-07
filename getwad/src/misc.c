/*
 * Utility functions for GetWAD
 *
 * Written by Hippocrates Sendoukas, Athens, Greece, June 2003
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
#include <windows.h>
#include <windowsx.h>
#endif

#include <stdio.h>
#include <string.h>
#include "misc.h"

#if _WIN32
/*  <- convtocl */

static void
convtocl(HWND hdlg,long *x,long *y)
{
  POINT p;

  p.x = *x;
  p.y = *y;
  ScreenToClient(hdlg,&p);
  *x = p.x;
  *y = p.y;
}

/*  -> convtocl */
/*  <- set_parm_ptr */

static void
set_parm_ptr(HWND hdlg,void *p)
{
  char temp[12];

  wsprintf(temp,"%08lx",(unsigned long)p);
  SetDlgItemText(hdlg,IDD_HSPARM1,temp);
}

/*  -> set_parm_ptr */
/*  <- hextodword */

static DWORD
hextodword(LPSTR s)
{
  DWORD		v;
  unsigned	c;

  for (v=0; *s; s++)
    {
      if (IsCharLower(*s))	c = *s - 'a' + 10;
      else if (IsCharUpper(*s))	c = *s - 'A' + 10;
      else c = *s - '0';
      v = (v << 4) | c;
    }
  return v;
}

/*  -> hextodword */
/*  <- get_parm_ptr */

static HSRECORD *
get_parm_ptr(HWND hdlg)
{
  char temp[32];

  GetDlgItemText(hdlg,IDD_HSPARM1,temp,sizeof(temp)-2);
  return (HSRECORD *) hextodword(temp);
}

/*  -> get_parm_ptr */
/*  <- fix_ok_cancel_help */

static void
fix_ok_cancel_help(HWND hdlg)
{
  LPHSRECORD	p;
  int		nx;
  RECT		r1,r3,r1p,r2p;
  HWND		h1,h2,h3;

  p = get_parm_ptr(hdlg);
  if (p->helpfun && p->helpkey) return;

  h1 = GetDlgItem(hdlg,IDOK);
  h2 = GetDlgItem(hdlg,IDCANCEL);
  h3 = GetDlgItem(hdlg,IDD_HELP);

  GetWindowRect(h1,&r1);
  GetWindowRect(h3,&r3);

  nx = 14 * (r3.right - r1.left) / 2;
  nx /= 15;

  r1p.left	= r1.left;
  r1p.right	= r1.left + nx;
  r1p.top	= r1.top;
  r1p.bottom	= r1.bottom;

  r2p.left	= r3.right - nx;
  r2p.right	= r3.right;
  r2p.top	= r3.top;
  r2p.bottom	= r3.bottom;

  convtocl(hdlg,&r1p.left, &r1p.top);
  convtocl(hdlg,&r1p.right,&r1p.bottom);
  convtocl(hdlg,&r2p.left, &r2p.top);
  convtocl(hdlg,&r2p.right,&r2p.bottom);

  MoveWindow(h3,-100,-100,20,20,0);
  ShowWindow(h3,SW_HIDE);
  MoveWindow(h1,r1p.left,r1p.top,r1p.right-r1p.left,r1p.bottom-r1p.top,1);
  MoveWindow(h2,r2p.left,r2p.top,r2p.right-r2p.left,r2p.bottom-r2p.top,1);
}

/*  -> fix_ok_cancel_help */
/*  <- fix_rec_dim */

static void
fix_rec_dim(HWND hdlg,int nf)
{
  int	i,dy;
  HWND	h,h1,h2,h3;
  RECT	r,r1,r2,r3,r4;

  if (nf<=1 || nf>=10) return;

  GetWindowRect(h1=GetDlgItem(hdlg,IDOK),&r1);
  GetWindowRect(h2=GetDlgItem(hdlg,IDCANCEL),&r2);
  GetWindowRect(h3=GetDlgItem(hdlg,IDD_HELP),&r3);

  GetWindowRect(hdlg,&r);

  i = r.bottom - r1.bottom;

  GetWindowRect(GetDlgItem(hdlg,IDD_LABEL1+nf),&r4);

  dy = r1.bottom - r1.top;
  r1.top = r2.top = r3.top = r4.top;
  r1.bottom = r2.bottom = r3.bottom = r1.top + dy;

  r.bottom = r1.bottom + i;

  MoveWindow(hdlg,r.left,r.top,r.right-r.left,r.bottom-r.top,1);

  for (i=nf; i<10; i++)
    {
      h = GetDlgItem(hdlg,IDD_STRED1+i);
      MoveWindow(h,-100,-100,20,20,0);
      ShowWindow(h,SW_HIDE);
      h = GetDlgItem(hdlg,IDD_LABEL1+i);
      MoveWindow(h,-100,-100,20,20,0);
      ShowWindow(h,SW_HIDE);
    }

  convtocl(hdlg,&r1.left,&r1.top);	convtocl(hdlg,&r1.right,&r1.bottom);
  convtocl(hdlg,&r2.left,&r2.top);	convtocl(hdlg,&r2.right,&r2.bottom);
  convtocl(hdlg,&r3.left,&r3.top);	convtocl(hdlg,&r3.right,&r3.bottom);

  MoveWindow(h1,r1.left,r1.top,r1.right-r1.left,r1.bottom-r1.top,1);
  MoveWindow(h2,r2.left,r2.top,r2.right-r2.left,r2.bottom-r2.top,1);
  MoveWindow(h3,r3.left,r3.top,r3.right-r3.left,r3.bottom-r3.top,1);
}

/*  -> fix_rec_dim */
/*  <- center_window */

void
center_window(HWND hwnd)
{
  HWND	hparent;
  RECT	r;
  int	nx,ny,par_nx,par_ny;
  POINT	p;

  GetWindowRect(hwnd,&r);
  nx = r.right - r.left;
  ny = r.bottom - r.top;
  if ( (hparent=GetParent(hwnd)) != 0 )
    {
      GetClientRect(hparent,&r);
      par_nx = r.right  - r.left;
      par_ny = r.bottom - r.top;
    }
  else
    {
      par_nx = GetSystemMetrics(SM_CXSCREEN);
      par_ny = GetSystemMetrics(SM_CYSCREEN);
    }
  p.x = (par_nx - nx)/2;
  p.y = (par_ny - ny)/3;
  if (hparent) ClientToScreen(hparent,&p);
  MoveWindow(hwnd,p.x,p.y,nx,ny,TRUE);
}

/*  -> center_window */
/*  <- RecDlgProc */

BOOL WINAPI
RecDlgProc(HWND hdlg,UINT msg,UINT wparam,LONG lparam)
{
  LPHSRECORD	p;
  LPHSFIELD	f;
  int		i,n;
  HWND		h;

  switch(msg)
    {
      case WM_INITDIALOG:
        p = (LPHSRECORD) lparam;
        n = p->nfields;
        fix_rec_dim(hdlg,n);
        SetWindowText(hdlg,p->caption);
        SetDlgItemText(hdlg,IDOK,p->oklabel);
        SetDlgItemText(hdlg,IDCANCEL,p->canlabel);
        SetDlgItemText(hdlg,IDD_HELP,p->helplabel);
        f = p->fld;
        for (i=0;  i<n;  f++,i++)
          {
            h = GetDlgItem(hdlg,IDD_STRED1+i);
            Edit_LimitText(h,f->size-2);
            Edit_SetText(h,f->buf);
            if (n!=1) SetDlgItemText(hdlg,IDD_LABEL1+i,f->label);
            EnableWindow(h,f->enabled);
          }
        set_parm_ptr(hdlg,p);
        fix_ok_cancel_help(hdlg);
        center_window(hdlg);
        return TRUE;

      case WM_COMMAND:
        switch(GET_WM_COMMAND_ID(wparam,lparam))
          {
            case IDOK:
              p = get_parm_ptr(hdlg);
              f = p->fld;
              n = p->nfields;
              for (i=0; i<n; f++,i++)
                GetDlgItemText(hdlg,IDD_STRED1+i,f->buf,f->size-1);
              if (p->valfun==0 || p->valfun(p))
                EndDialog(hdlg,TRUE);
              return TRUE;
            case IDCANCEL:
              EndDialog(hdlg,FALSE);
              return TRUE;
            case IDD_HELP:
              p = get_parm_ptr(hdlg);
              if (p->helpfun && p->helpkey)
                p->helpfun(p->helpkey);
              return TRUE;
          }
    }
  return FALSE;
}

/*  -> RecDlgProc */
/*  <- edit_record */

int
edit_record(HINSTANCE hinst,char *dlgname,LPHSRECORD r)
{
  int	i;

  if (r->version!=0) return -1;
  i = r->nfields;
  if (i<1 || i>10) return -1;
  if (!dlgname) dlgname = (i==1) ? "dlg1" : "dlg10";
  i = DialogBoxParam(hinst,dlgname,r->hwnd,RecDlgProc,(LONG)r);
  return i;
}

/*  -> edit_record */
#endif
/*  <- strins */

static void
strins(char *source,char *target,int pos)
{
  int  k,n;
  char *pt;

  if (pos<0 || pos>(k=strlen(target))) return;
  pt = target+pos;
  memmove(pt+(n=strlen(source)),pt,k-pos+1);
  memcpy(pt,source,n);
}

/*  -> strins */
/*  <- skip_spaces */

char *
skip_spaces(char *p)
{
  while (*p==' ' || *p=='\t')
    p++;
  return p;
}

/*  -> skip_spaces */
/*  <- rtrim */

int
rtrim(char *s)
{
  int i;

  for (i=strlen(s)-1;  i>=0 && IsSpace(s[i]);  i--)  ;
  s[++i] = '\0';
  return i;
}

/*  -> rtrim */
/*  <- ltrim */

int
ltrim(char *s)
{
  int i,j;

  for (i=0,j=strlen(s); i<j && IsSpace(s[i]);  i++)  ;
  if (i)
    {
      j -= i;
      if (j) strcpy(s,s+i);
      s[j] = '\0';
    }
  return j;
}

/*  -> ltrim */
/*  <- fmt_size */

char *
fmt_size(int v)
{
  int pos;
  static char temp[32];
  static char buf[4];

  if (buf[0]=='\0')
    {
      #if _WIN32
        GetProfileString("intl","sThousand","",buf,sizeof(buf));
      #endif
      if (buf[0]=='\0') buf[0] = ',';
    }
  v = (v+512) / 1024;
  if (v<=0) v = 1;
  sprintf(temp,"%d",v);
  for (pos=strlen(temp)-3;  pos>0;   pos-=3)
    strins(buf,temp,pos);
  strcat(temp," KB");
  return temp;
}

/*  -> fmt_size */
/*  <- get_arg */

char *
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
