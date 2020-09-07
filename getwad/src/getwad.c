/*  <- declarations */
/*
 * Library to locate, download, unpack and install a given WAD file.
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
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #include <windowsx.h>
  #include <commctrl.h>
  #include <shellapi.h>
  #include <shlobj.h>
  #include <process.h>
  #include <wininet.h>
  #include <shlwapi.h>
  #include <io.h>
  #include "getwadrc.h"
  #define PATH_SEP	'\\'
  static	char	*progname = "GetWAD";
  static	BOOL	dont_wait_upon_success;
#else
  #include <stdlib.h>
  #include <ctype.h>
  #include <sys/types.h>
  #include <sys/stat.h>
  #define BOOL		int
  #define TRUE		(1)
  #define FALSE		(0)
  #define MAX_PATH	(256)
  #define HWND		int
  #define PATH_SEP	'/'
#endif
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "getwad.h"
#include "inet.h"
#include "unzip.h"
#include "misc.h"
#define MAX_WADNAME	63
#define MAX_PREDEF	100
#define TIMEOUT_SECS	60
#define MAX_HREFS	1023
#define MAX_HREF_SIZE	1023
#define IS_HTTP_URL(u)	(!memcmp(u,"http://",7))
#define IS_FTP_URL(u)	(!memcmp(u,"ftp://",6))

static	char		target_wadname[MAX_WADNAME+1];
static	char		target_zipname[MAX_WADNAME+1];
static	char		target_txtname[MAX_WADNAME+1];
static	BOOL		use_google;
static	BOOL		enable_ftp;
static	int		npredef;
static	char		*predef[MAX_PREDEF];
static	int		nextras;
static	char		*extras[MAX_PREDEF];
static	char		wad_dir[MAX_PATH+1];

typedef struct
{
  HWND			hdlg, hpb, hst;
  BOOL			please_stop, running, success;
  char			*pargs;
} getwad_parms;

typedef struct
{
  FILE			*f;
  BOOL			first_bytes;
  getwad_parms		*gwp;
} transfer_parms;

#if _WIN32
  #define EOL			"\r\n"
  static const char *g_ini_name	= "getwad.ini";
  #define READBIN		"rb"
  #define WRITEBIN		"wb"
  static  HINSTANCE		hinst;
#else
  #define EOL			"\n"
  static const char *g_ini_name	= "/usr/local/etc/getwad.ini";
  #define READBIN		"r"
  #define WRITEBIN		"w"
  #define SendMessage(hwnd,msg,wp,lp)
  #define PostMessage(hwnd,msg,wp,lp)
  #define lstrcmpi(a,b)		strcasecmp((a),(b))
#endif
static const char	*g_ini_section	= "Options";
static int	getwad_version	= 1;
static const char	*g_server_search = NULL;

/*
 * Default search pages for getwad 1.x OR
 * when the master search list is not available.
 */
static char *suggested[] = {
	"http://www.getsomewhere.net/?dep=doom&pg=files",
	"http://hs.keystone.gr/lap/",
	"http://www.rarefiles.com/download/",
	"http://raider.dnsalias.com:8001/doom/userwads/",
     NULL };

static char *master_pages[] = {
     "http://getwad.keystone.gr/master/",
     "http://zdaemon.org/getwad/",
     NULL };

/*  -> declarations */
/*  <- short_name */

static const char *
short_name(const char *url)
{
  const char	*p,*q;

  if (IS_HTTP_URL(url))		p = url+7;
  else if (IS_FTP_URL(url))	p = url+6;
  else				return url;
  if ( (q=strchr(p,'/')) != NULL )
    p = q+1;
  return p;
}

/*  -> short_name */
/*  <- make_outname */

static char *
make_outname(const char *dirname,const char *fname)
{
  int n;
  static char temp[MAX_PATH+1];

  strcpy(temp,dirname);
  n = strlen(temp);
  if (temp[n-1]!='\\' && temp[n-1]!='/') temp[n++] = PATH_SEP;
  strcpy(temp+n,fname);
  return temp;
}

/*  -> make_outname */
/*  <- strcpymax */

static void
strcpymax(char *dest,const char *src,int destsz)
{
  int n;

  n = strlen(src);
  if (n>=destsz) n = destsz-1;
  memcpy(dest,src,n);
  dest[n] = '\0';
}

/*  -> strcpymax */
#if !_WIN32
/*  <- GetPrivateProfileString */

static void
GetPrivateProfileString(const char *section,const char *key,const char *defval,
                        char *retbuf,int bufsz,const char *ininame)
{
  FILE	*f;
  int	n;
  char	c,*p;
  char	buf[1024], secname[256];

  strcpy(retbuf,defval);
  sprintf(secname,"[%s]\n",section);
  if ( (f=fopen(ininame,"r")) == NULL ) return;
  n = strlen(key);
  while (fgets(buf,sizeof(buf)-1,f))
    {
      if (!lstrcmpi(buf,secname))
        {
          while (fgets(buf,sizeof(buf)-1,f))
            {
              if (buf[0]=='[') break;
              c = buf[n];
              buf[n] = '\0';
              if (lstrcmpi(buf,key)==0 && c=='=')
                {
                  p = buf+n+1;
                  n = strlen(p);
                  if (n>0 && p[n-1]=='\n') p[n-1] = '\0';
                  strcpymax(retbuf,p,bufsz);
                  break;
                }
            }
          break;
        }
    }
  fclose(f);
}

/*  -> GetPrivateProfileString */
/*  <- GetPrivateProfileInt */

static int
GetPrivateProfileInt(const char *section,const char *key,int defval,const char *ininame)
{
  int	v;
  char	jnk[64];

  GetPrivateProfileString(section,key,"",jnk,sizeof(jnk),ininame);
  return (sscanf(jnk,"%d",&v)==1) ? v : defval;
}

/*  -> GetPrivateProfileInt */
/*  <- strdel */

static void
strdel(char *s,int pos,int len)
{
  int k,m;

  if (pos>=0  &&  pos<(k=strlen(s)))
    {
      if ((m=pos+len)>=k)  s[pos] = '\0';
      else  memcpy(s+pos,s+m,k-m+1);
    }
}

/*  -> strdel */
/*  <- clean_up_relative_paths */

static void
clean_up_relative_paths(char *urlname)
{
  char *p,*q;

  while ( (p=strstr(urlname,"/../")) != NULL )
    {
      for (q=p-1; q>urlname && *q!='/'; q--)
        ;
      if (*q != '/') break;	/* hopelessly bad input */
      strdel(q,0, (int)(p-q) + 3);
    }
}

/*  -> clean_up_relative_paths */
/*  <- AnsiLower */

static void
AnsiLower(char *s)
{
  unsigned char *p;

  for (p = (unsigned char *)s;  *p;  p++)
    if (isupper(*p))	*p = tolower(*p);
}

/*  -> AnsiLower */
#endif
/*  <- combine_url */

static void
combine_url(const char *base_url,const char *rel_url,char *outbuf,unsigned bufsz)
{
  #if _WIN32
    memset(outbuf,0,bufsz);
    InternetCombineUrl(base_url,rel_url,outbuf,(unsigned long *)&bufsz,ICU_NO_ENCODE);
  #else
    int		n;
    char	*p;

    strcpymax(outbuf,rel_url,bufsz);
    if (IS_HTTP_URL(rel_url) || IS_FTP_URL(rel_url))  return;
/*  <-     absolute path in relative url */
    if (*rel_url=='/')
      {
        if (IS_HTTP_URL(base_url))
          {
            if ( (p=strchr(base_url+7,'/')) == NULL ) return;
            n = (int)(p-base_url);
            if (n>=bufsz) return;
            memcpy(outbuf,base_url,n);
            strcpymax(outbuf+n,rel_url,bufsz-n);
          }
        else if (IS_FTP_URL(base_url))
          {
            if ( (p=strchr(base_url+6,'/')) == NULL ) return;
            n = (int)(p-base_url);
            if (n>=bufsz) return;
            memcpy(outbuf,base_url,n);
            strcpymax(outbuf+n,rel_url,bufsz-n);
          }
        return;
      }
/*  ->     absolute path in relative url */
/*  <-     relative path */
    if (IS_HTTP_URL(base_url))
      {
        if ( (p=strrchr(base_url+7,'/')) == NULL ) return;
        n = 1 + (int)(p-base_url);
        if (n>=bufsz) return;
        memcpy(outbuf,base_url,n);
        strcpymax(outbuf+n,rel_url,bufsz-n);
        clean_up_relative_paths(outbuf);
      }
    else if (IS_FTP_URL(base_url))
      {
        if ( (p=strrchr(base_url+6,'/')) == NULL ) return;
        n = 1 + (int)(p-base_url);
        if (n>=bufsz) return;
        memcpy(outbuf,base_url,n);
        strcpymax(outbuf+n,rel_url,bufsz-n);
        clean_up_relative_paths(outbuf);
     }
/*  ->     relative path */
  #endif
}

/*  -> combine_url */
/*  <- Option-related stuff */
/*  <- get_reg_keyval */

#if _WIN32
static BOOL
get_reg_keyval(HKEY hk0,const char *branch,const char *key,char *buf,int bufsize)
{
  HKEY	hk;
  LONG	res;
  DWORD	sz;

  memset(buf,0,bufsize);
  if (RegOpenKeyEx(hk0,branch,0,KEY_QUERY_VALUE,&hk)!=ERROR_SUCCESS) return FALSE;

  sz = bufsize-1;
  res = RegQueryValueEx(hk,key,NULL,NULL,(unsigned char *) buf,&sz);
  RegCloseKey(hk);
  return (buf[0]!='\0');
}
#else
#define get_reg_keyval(hk0,brach,key,buf,bufsz)		(0)
#endif

/*  -> get_reg_keyval */
/*  <- url_ok */

static BOOL
url_ok(const char *s)
{
  BOOL	first_time;
  unsigned char	c;

  if (!IS_HTTP_URL(s) && !IS_FTP_URL(s)) return FALSE;
  first_time = TRUE;
  while (*s)
    {
      c = (unsigned char) *s++;
	  if (c<=' ' || c>=127) return FALSE;
      if (c != '%') continue;
      c = (unsigned char) *s++;
      if (c == '%')	continue;
      if (c != 's')	return FALSE;
      if (first_time)	first_time = FALSE;
      else		return FALSE;
    }
  return TRUE;
}

/*  -> url_ok */
/*  <- add_check */

static BOOL
add_check(const char *url)
{
  int i;

  if (!url || !url_ok(url)) return FALSE;
  if (npredef>=MAX_PREDEF) return FALSE;
  for (i=0; i<npredef; i++)
    if (!strcmp(predef[i],url)) return TRUE;
  if ( (predef[npredef]=strdup(url)) == NULL ) return FALSE;
  npredef++;
  return TRUE;
}

/*  -> add_check */
/*  <- add_check2 */

static BOOL
add_check2(const char *url)
{
  int i;

  if (!url || !url_ok(url)) return FALSE;
  if (nextras>=MAX_PREDEF) return FALSE;
  for (i=0; i<nextras; i++)
    if (!strcmp(extras[i],url)) return TRUE;
  if ( (extras[nextras]=strdup(url)) == NULL ) return FALSE;
  nextras++;
  return TRUE;
}

/*  -> add_check2 */
/*  <- read_params */

static int
read_params(const char *force_wad_dir, BOOL fetching)
{
  int		i,n;
  char		jnk[32],temp[MAX_HREF_SIZE+1];

  for (i=0; i<npredef; i++) free(predef[i]);
  for (i=0; i<nextras; i++) free(extras[i]);

  use_google	= GetPrivateProfileInt(g_ini_section,"use_google",1,g_ini_name);
  enable_ftp	= GetPrivateProfileInt(g_ini_section,"enable_ftp",1,g_ini_name);

  npredef = nextras = 0;
  if (getwad_version==1)
    {
      n = GetPrivateProfileInt(g_ini_section,"npredef",0,g_ini_name);
      if (n<0) n = 0;
      for (i=0; i<n; i++)
        {
          sprintf(jnk,"predef%03d",i);
          GetPrivateProfileString(g_ini_section,jnk,"",temp,MAX_HREF_SIZE,g_ini_name);
          ltrim(temp);
          rtrim(temp);
          add_check(temp);
        }
      if (npredef==0)
        for (i=0; suggested[i]; i++)
          add_check(suggested[i]);
    }
  else
    {
      n = GetPrivateProfileInt(g_ini_section,"nextra",0,g_ini_name);
      if (n<0) n = 0;
      for (i=0; i<n; i++)
        {
          sprintf(jnk,"extra%03d",i);
          GetPrivateProfileString(g_ini_section,jnk,"",temp,MAX_HREF_SIZE,g_ini_name);
          ltrim(temp);
          rtrim(temp);
          add_check2(temp);
        }
      if (fetching)
        {
          add_check2(g_server_search);
          n = GetPrivateProfileInt(g_ini_section,"nmaster",0,g_ini_name);
          if (n<0) n = 0;
          for (i=0; i<n; i++)
            {
              sprintf(jnk,"master%03d",i);
              GetPrivateProfileString(g_ini_section,jnk,"",temp,MAX_HREF_SIZE,g_ini_name);
              ltrim(temp);
              rtrim(temp);
              add_check2(temp);
            }
          if (n==0)
            for (i=0; suggested[i]; i++)
              add_check2(suggested[i]);
        }
    }

  GetPrivateProfileString(g_ini_section,"wad_dir","",wad_dir,MAX_PATH-1,g_ini_name);
  ltrim(wad_dir);
  rtrim(wad_dir);
  if (wad_dir[0]=='\0')
    {
      if (get_reg_keyval(HKEY_LOCAL_MACHINE,"Software\\Internet Doom",
                         "InstallDir",wad_dir,sizeof(wad_dir)-32))
        strcpy(wad_dir,make_outname(wad_dir,"wads"));
    }

  if (force_wad_dir && force_wad_dir[0])
    strcpymax(wad_dir,force_wad_dir,MAX_PATH);

  return (wad_dir[0] != '\0');
}

/*  -> read_params */
#if _WIN32
/*  <- save_params */

static void
save_params(BOOL allow_dest_spec)
{
  int i;
  char jnk[32];

  sprintf(jnk,"%d",use_google);
  WritePrivateProfileString(g_ini_section,"use_google",jnk,g_ini_name);
  sprintf(jnk,"%d",enable_ftp);
  WritePrivateProfileString(g_ini_section,"enable_ftp",jnk,g_ini_name);

  if (allow_dest_spec)
    WritePrivateProfileString(g_ini_section,"wad_dir",wad_dir,g_ini_name);

  if (getwad_version==1)
    {
      sprintf(jnk,"%d",npredef);
      WritePrivateProfileString(g_ini_section,"npredef",jnk,g_ini_name);
      for (i=0; i<npredef; i++)
        {
          sprintf(jnk,"predef%03d",i);
          WritePrivateProfileString(g_ini_section,jnk,predef[i],g_ini_name);
        }
    }
  else
    {
      sprintf(jnk,"%d",nextras);
      WritePrivateProfileString(g_ini_section,"nextra",jnk,g_ini_name);
      for (i=0; i<nextras; i++)
        {
          sprintf(jnk,"extra%03d",i);
          WritePrivateProfileString(g_ini_section,jnk,extras[i],g_ini_name);
        }
    }
}

/*  -> save_params */
/*  <- BrowseCallbackProc */

static int WINAPI
BrowseCallbackProc(HWND hwnd,UINT msg,LPARAM lp, LPARAM pdata)
{
  TCHAR szDir[MAX_PATH];

  switch (msg)
    {
      case BFFM_INITIALIZED:
        SendMessage(hwnd,BFFM_SETSELECTION,TRUE,pdata);
        break;

      case BFFM_SELCHANGED:
         if (SHGetPathFromIDList((LPITEMIDLIST) lp ,szDir))
           SendMessage(hwnd,BFFM_SETSTATUSTEXT,0,(LPARAM)szDir);
         break;
    }
  return 0;
}

/*  -> BrowseCallbackProc */
/*  <- select_directory */

static BOOL
select_directory(HWND hwnd,char *dirname)
{
  BOOL		rc;
  LPMALLOC	pMalloc;
  LPITEMIDLIST	pidl;
  BROWSEINFO	bi;

  if (SHGetMalloc(&pMalloc)) return FALSE;
  rc = FALSE;
  memset(&bi,0,sizeof(bi));
  bi.hwndOwner	= hwnd;
  bi.lpszTitle	= "Select the directory where the WADs will be placed";
  bi.ulFlags	= BIF_RETURNONLYFSDIRS | BIF_STATUSTEXT;
  bi.lpfn	= BrowseCallbackProc;
  bi.lParam	= (LPARAM) dirname;

  if ( (pidl=SHBrowseForFolder(&bi)) != NULL )
    {
      if (SHGetPathFromIDList(pidl,dirname))	rc = TRUE;
      #ifdef __cplusplus
        pMalloc->Free(pidl);
      #else
        pMalloc->lpVtbl->Free(pMalloc,pidl);
      #endif
    }
  #ifdef __cplusplus
    pMalloc->Release();
  #else
    pMalloc->lpVtbl->Release(pMalloc);
  #endif
  return rc;
}

/*  -> select_directory */
/*  <- Msg */

static void
Msg(HWND h,const char *s)
{
  if (!h) h = GetActiveWindow();
  MessageBox(h,s,progname,MB_ICONSTOP|MB_OK);
}

/*  -> Msg */
/*  <- CheckURL */

static BOOL FAR WINAPI
CheckURL(HSRECORD *p)
{
  char	*s;

  rtrim(p->fld[0].buf);
  ltrim(p->fld[0].buf);
  s = p->fld[0].buf;
  if (s[0]=='\0')
    {
      MessageBeep(0);
      return FALSE;
    }
  if (!url_ok(s))
    {
      Msg(0,"You must specify an HTTP or FTP URL! It may also "
            "contain the %s sequence which will be replaced by "
            "the desired WAD name (this is useful for search "
            "engine URLs). To use a % character inside the "
            "URL, you must specify it twice (ie., %%).");
      return FALSE;
    }
  return TRUE;
}

/*  -> CheckURL */
/*  <- lb_update_width */

static void
lb_update_width(HWND hlb)
{
  int	n,i,maxx;
  HDC	hdc;
  SIZE	sz;
  HFONT	hfold;
  char	buf[MAX_HREF_SIZE+1];

  if ( (hdc=GetDC(hlb)) == 0 ) return;
  hfold = SelectFont(hdc,GetWindowFont(hlb));
  maxx = 0;
  n = ListBox_GetCount(hlb);
  for (i=0; i<n; i++)
    {
      memset(buf,0,sizeof(buf));
      ListBox_GetText(hlb,i,buf);
      memset(&sz,0,sizeof(sz));
      GetTextExtentPoint32(hdc,buf,strlen(buf),&sz);
      if (sz.cx>maxx) maxx = sz.cx;
    }
  SelectFont(hdc,hfold);
  ReleaseDC(hlb,hdc);
  maxx += GetSystemMetrics(SM_CXVSCROLL);
  ListBox_SetHorizontalExtent(hlb,maxx);
}

/*  -> lb_update_width */
/*  <- add_edit_predef */

static void
add_edit_predef(HWND hdlg,HWND hlb,int i)
{
  HSRECORD	r;
  BOOL		is_new;
  char		temp[MAX_HREF_SIZE+1];

  is_new = (i<0);
  memset(temp,0,sizeof(temp));
  if (!is_new) ListBox_GetText(hlb,i,temp);

  memset(&r,0,sizeof(r));
  r.hwnd = hdlg;
  r.caption = "URL of Search Page";
  r.oklabel = "OK";
  r.canlabel = "Cancel";
  r.valfun = (valfun_ptr) CheckURL;
  r.nfields = 1;
  r.fld[0].label = "";
  r.fld[0].buf = temp;
  r.fld[0].size = MAX_HREF_SIZE;
  r.fld[0].enabled = 1;
  if (!edit_record(hinst,"dlg1",&r)) return;
  ltrim(temp);
  rtrim(temp);
  if (temp[0]=='\0')
    {
      MessageBeep(0);
      return;
    }
  if (is_new)
    {
      ListBox_AddString(hlb,temp);
      i = ListBox_GetCount(hlb)-1;
    }
  else
    {
      ListBox_DeleteString(hlb,i);
      ListBox_InsertString(hlb,i,temp);
    }
  ListBox_SetCurSel(hlb,i);
  lb_update_width(hlb);
}

/*  -> add_edit_predef */
/*  <- OptProc */

BOOL WINAPI
OptProc(HWND hdlg,UINT msg,UINT wparam,LONG lparam)
{
  HANDLE	hbmp;
  HWND		h;
  int		i,n,k;
  char		temp[MAX_HREF_SIZE+1],temp_dir[MAX_PATH+1];
  static BOOL	allow_dest_spec;

  switch(msg)
    {
/*  <-       WM_INITDIALOG */

      case WM_INITDIALOG:
        allow_dest_spec = (BOOL) lparam;
        center_window(hdlg);
        read_params(NULL,FALSE);
        hbmp = LoadImage(hinst,"add_img",IMAGE_BITMAP,0,0,LR_LOADMAP3DCOLORS);
        SendMessage(GetDlgItem(hdlg,IDD_ADD),BM_SETIMAGE,IMAGE_BITMAP,(LPARAM)hbmp);

        hbmp = LoadImage(hinst,"del_img",IMAGE_BITMAP,0,0,LR_LOADMAP3DCOLORS);
        SendMessage(GetDlgItem(hdlg,IDD_DEL),BM_SETIMAGE,IMAGE_BITMAP,(LPARAM)hbmp);

        hbmp = LoadImage(hinst,"prop_img",IMAGE_BITMAP,0,0,LR_LOADMAP3DCOLORS);
        SendMessage(GetDlgItem(hdlg,IDD_EDIT),BM_SETIMAGE,IMAGE_BITMAP,(LPARAM)hbmp);

        hbmp = LoadImage(hinst,"up_img",IMAGE_BITMAP,0,0,LR_LOADMAP3DCOLORS);
        SendMessage(GetDlgItem(hdlg,IDD_UP),BM_SETIMAGE,IMAGE_BITMAP,(LPARAM)hbmp);

        hbmp = LoadImage(hinst,"down_img",IMAGE_BITMAP,0,0,LR_LOADMAP3DCOLORS);
        SendMessage(GetDlgItem(hdlg,IDD_DOWN),BM_SETIMAGE,IMAGE_BITMAP,(LPARAM)hbmp);

        if (allow_dest_spec)
          {
            hbmp = LoadImage(hinst,"browse_img",IMAGE_BITMAP,0,0,LR_LOADMAP3DCOLORS);
            SendMessage(GetDlgItem(hdlg,IDD_DIRBR),BM_SETIMAGE,IMAGE_BITMAP,(LPARAM)hbmp);
            h = GetDlgItem(hdlg,IDD_WADDIR);
            Edit_SetText(h,wad_dir);
            Edit_LimitText(h,MAX_PATH-1);
          }
        else
          {
            ShowWindow(GetDlgItem(hdlg,IDD_DESTLBL),SW_HIDE);
            ShowWindow(GetDlgItem(hdlg,IDD_WADDIR),SW_HIDE);
            ShowWindow(GetDlgItem(hdlg,IDD_DIRBR),SW_HIDE);
          }

        h = GetDlgItem(hdlg,IDD_USEGOOGLE);
        Button_SetCheck(h,use_google);

        h = GetDlgItem(hdlg,IDD_ENABLEFTP);
        Button_SetCheck(h,enable_ftp);

        h = GetDlgItem(hdlg,IDD_PREDEF);
        if (getwad_version==1)
          {
            for (i=0; i<npredef; i++)
              ListBox_AddString(h,predef[i]);
          }
        else
          {
            for (i=0; i<nextras; i++)
              ListBox_AddString(h,extras[i]);
            SetDlgItemText(hdlg,IDD_LBLABEL,"Extra search pages:");
          }
        if (ListBox_GetCount(h)>0) ListBox_SetCurSel(h,0);
        lb_update_width(h);
        return TRUE;
/*  ->       WM_INITDIALOG */

      case WM_COMMAND:
        switch (GET_WM_COMMAND_ID(wparam,lparam))
          {
/*  <-             IDD_ADD */

            case IDD_ADD:
              h = GetDlgItem(hdlg,IDD_PREDEF);
              if (ListBox_GetCount(h)>=MAX_PREDEF)
                {
                  MessageBox(hdlg,"You cannot add any more search pages",progname,MB_ICONSTOP);
                  return TRUE;
                }
              add_edit_predef(hdlg,h,-1);
              return TRUE;
/*  ->             IDD_ADD */
/*  <-             IDD_PREDEF */

            case IDD_PREDEF:
              if (GET_WM_COMMAND_CMD(wparam,lparam)!=LBN_DBLCLK) break;
/*  ->             IDD_PREDEF */
/*  <-             IDD_EDIT */

            case IDD_EDIT:
              h = GetDlgItem(hdlg,IDD_PREDEF);
              i = ListBox_GetCurSel(h);
              if (i<0 || i>=ListBox_GetCount(h))
                MessageBeep(0);
              else add_edit_predef(hdlg,h,i);
              return TRUE;
/*  ->             IDD_EDIT */
/*  <-             IDD_UP */

            case IDD_UP:
              h = GetDlgItem(hdlg,IDD_PREDEF);
              i = ListBox_GetCurSel(h);
              n = ListBox_GetCount(h);
              if (i<=0 || i>=n)
                {
                  MessageBeep(0);
                  return TRUE;
                }
              ListBox_GetText(h,i,temp);
              ListBox_DeleteString(h,i);
              ListBox_InsertString(h,i-1,temp);
              ListBox_SetCurSel(h,i-1);
              return TRUE;
/*  ->             IDD_UP */
/*  <-             IDD_DOWN */

            case IDD_DOWN:
              h = GetDlgItem(hdlg,IDD_PREDEF);
              i = ListBox_GetCurSel(h);
              n = ListBox_GetCount(h);
              if (i<0 || i>=n-1)
                {
                  MessageBeep(0);
                  return TRUE;
                }
              ListBox_GetText(h,i,temp);
              ListBox_DeleteString(h,i);
              ListBox_InsertString(h,i+1,temp);
              ListBox_SetCurSel(h,i+1);
              return TRUE;
/*  ->             IDD_DOWN */
/*  <-             IDD_DEL */

            case IDD_DEL:
              h = GetDlgItem(hdlg,IDD_PREDEF);
              i = ListBox_GetCurSel(h);
              if (i<0 || i>=ListBox_GetCount(h))
                MessageBeep(0);
              else if (MessageBox(hdlg,"Are you sure you want to delete this item?",progname,MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2)==IDYES)
                {
                  ListBox_DeleteString(h,i);
                  lb_update_width(h);
                }
              return TRUE;
/*  ->             IDD_DEL */
/*  <-             IDD_DIRBR */

            case IDD_DIRBR:
              if (select_directory(hdlg,temp_dir))
                Edit_SetText(GetDlgItem(hdlg,IDD_WADDIR),temp_dir);
              return TRUE;
/*  ->             IDD_DIRBR */
/*  <-             OK */

            case IDOK:
              if (allow_dest_spec)
                {
                  Edit_GetText(GetDlgItem(hdlg,IDD_WADDIR),temp_dir,MAX_PATH-1);
                  ltrim(temp_dir);
                  rtrim(temp_dir);
                  if (temp_dir[0]=='\0')
                    {
                      Msg(hdlg,"You have to specify the WAD directory!");
                      return TRUE;
                    }
                  strcpy(wad_dir,temp_dir);
                }
              use_google = Button_GetCheck(GetDlgItem(hdlg,IDD_USEGOOGLE));
              enable_ftp = Button_GetCheck(GetDlgItem(hdlg,IDD_ENABLEFTP));
              if (getwad_version==1)
                {
                  for (i=0; i<npredef; i++) free(predef[i]);
                }
              else
                {
                  for (i=0; i<nextras; i++) free(extras[i]);
                }

              h = GetDlgItem(hdlg,IDD_PREDEF);
              n = ListBox_GetCount(h);
              k = 0;
              for (i=0; i<n; i++)
                {
                  ListBox_GetText(h,i,temp);
                  if (temp[0]=='\0') break;
                  if (k>=MAX_PREDEF) break;
                  if (getwad_version==1)
                    {
                      if ( (predef[k]=strdup(temp)) == NULL ) break;
                    }
                  else
                    {
                      if ( (extras[k]=strdup(temp)) == NULL ) break;
                    }
                  k++;
                }
              if (getwad_version==1)	npredef = k;
              else			nextras = k;
              save_params(allow_dest_spec);
              EndDialog(hdlg,TRUE);
              return TRUE;
/*  ->             OK */
/*  <-             CANCEL */

            case IDCANCEL:
              EndDialog(hdlg,TRUE);
              return TRUE;
/*  ->             CANCEL */
/*  <-             IDD_REVERT */

            case IDD_REVERT:
              if (MessageBox(hdlg,"Are you sure you want to reset the options to their default values?",
                             progname,MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2)!=IDYES)
                return TRUE;

              Button_SetCheck(GetDlgItem(hdlg,IDD_USEGOOGLE),TRUE);
              Button_SetCheck(GetDlgItem(hdlg,IDD_ENABLEFTP),TRUE);

              h = GetDlgItem(hdlg,IDD_PREDEF);
              ListBox_ResetContent(h);
              if (getwad_version==1)
                {
                  for (i=0;  suggested[i];  i++)
                    ListBox_AddString(h,suggested[i]);
                  ListBox_SetCurSel(h,0);
                }
              lb_update_width(h);
              return TRUE;
/*  ->             IDD_REVERT */
          }
        break;
    }
  return FALSE;
}


/*  -> OptProc */
/*  <- getwad_setup		public */

void WINAPI
getwad_setup(HWND hwnd, BOOL allow_dest_spec, LPCSTR ini_name, LPCSTR ini_sec)
{
  if (ini_name && *ini_name) g_ini_name = ini_name;
  if (ini_sec  && *ini_sec ) g_ini_section = ini_sec;
  getwad_version = 1;
  DialogBoxParam(hinst,"OPTS",hwnd,OptProc,allow_dest_spec);
}


/*  -> getwad_setup		public */
/*  <- getwad2_setup		public */

void WINAPI
getwad2_setup(HWND hwnd, BOOL allow_dest_spec, LPCSTR ini_name, LPCSTR ini_sec)
{
  if (ini_name && *ini_name) g_ini_name = ini_name;
  if (ini_sec  && *ini_sec ) g_ini_section = ini_sec;
  getwad_version = 2;
  DialogBoxParam(hinst,"OPTS",hwnd,OptProc,allow_dest_spec);
}


/*  -> getwad2_setup		public */
#endif
/*  -> Option-related stuff */
/*  <- zip_extract */

/*
 * Returns TRUE if the file was found, and FALSE otherwise. If
 * there was an error, it will also set an error message.
 */
static BOOL
zip_extract(const char *zipname,const char *fname_in_zip,const char *fname_extracted,
            int case_sensitive,int discard_path,char **s)
{
  unzFile	uzf;
  FILE		*fout;
  int		n;
  char		buf[2048];
  static char	errstr[512+2*MAX_PATH];

  *s = NULL;
  if ( (uzf=unzOpen(zipname)) == NULL )
    {
      sprintf(errstr,"The downloaded file is either not a zip file or it has been corrupted");
      *s = errstr;
      return FALSE;
    }
  if (unzLocateFile(uzf,fname_in_zip, (case_sensitive) ? 1 : 2,discard_path,buf) != UNZ_OK )
    {
      unzClose(uzf);
      sprintf(errstr,"Cannot find \"%s\" in the downloaded zip file",fname_in_zip);
      *s = errstr;
      return FALSE;
    }
  if (unzOpenCurrentFile(uzf) != UNZ_OK )
    {
      unzClose(uzf);
      sprintf(errstr,"Cannot open file \"%s\" in the downloaded zip file." EOL
                     "The zip file is either corrupt or uses an unsupported compression scheme.",
                     fname_in_zip);
      *s = errstr;
      return FALSE;
    }

  if ( (fout=fopen(fname_extracted,WRITEBIN)) == NULL )
    {
      unzCloseCurrentFile(uzf);
      unzClose(uzf);
      sprintf(errstr,"Cannot open \"%s\" for writing",fname_extracted);
      *s = errstr;
      return FALSE;
    }

  for (;;)
    {
      n = unzReadCurrentFile(uzf,buf,sizeof(buf));
      if (n==0) break;
      if (n<0)
        {
          sprintf(errstr,"Cannot read file \"%s\" in the downloaded zip file." EOL
                         "The zip file is probably corrupt.",
                         fname_in_zip);
          DEEP_SHIT:
          *s = errstr;
          unzCloseCurrentFile(uzf);
          unzClose(uzf);
          fclose(fout);
          remove(fname_extracted);
          return FALSE;
        }
      if (fwrite(buf,n,1,fout) != 1)
        {
          sprintf(errstr,"Cannot write to \"%s\"",fname_extracted);
          goto DEEP_SHIT;
        }
    }
  if (unzCloseCurrentFile(uzf) != UNZ_OK)
    {
      unzClose(uzf);
      fclose(fout);
      remove(fname_extracted);
      sprintf(errstr,"Cannot read file \"%s\" in the downloaded zip file." EOL
                     "The zip file is probably corrupt.",
                     fname_in_zip);
      *s = errstr;
      return FALSE;
    }
  fclose(fout);
  unzClose(uzf);
  #if !_WIN32
  chmod(fname_extracted,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
  #endif
  return TRUE;
}

/*  -> zip_extract */
/*  <- log_out */

#if _WIN32

static void
log_out(getwad_parms *gwp,const char *msg)
{
  int n = GetWindowTextLength(gwp->hst);
  SendMessage(gwp->hst,EM_SETSEL,n,n);
  SendMessage(gwp->hst,EM_REPLACESEL,FALSE,(LPARAM)msg);
}

#else

#define log_out(a,b)	fputs((b),stdout)

#endif

/*  -> log_out */
/*  <- page_transfer */

int WINAPI
page_transfer(void *cookie,unsigned char *buf,int n)
{
  transfer_parms *tp = (transfer_parms *) cookie;

  if (tp->gwp->please_stop) return 0;
  return (fwrite(buf,n,1,tp->f)==1);

}

/*  -> page_transfer */
/*  <- page_status */

int WINAPI
page_status(void *cookie,inet_status_type ns, int ne,
            char *host,unsigned cur_bytes,unsigned tot_bytes)
{
  transfer_parms *tp;
  int	i;
  char	jnk[1024];
  static inet_error_type nea[] = {
    INETERR_BADPARMS,
    INETERR_BADURL,
    INETERR_NOCONNECT,
    INETERR_CANNOTSEND,
    INETERR_CANCELED,
    INETERR_BADRESPONSE,
    INETERR_NOLOGIN,
    INETERR_BADPATH,
    INETERR_BADFILE,
    INETERR_NODATA,
    INETERR_REDIRECTED,
    INETERR_BADREDIRECTION,
    INETERR_SHORTFILE,
    INETERR_BADZSTREAM,
    INETERR_TIMEOUT
  };
  static char *msg[] = {
    "Invalid parameters",
    "Invalid URL",
    "Cannot connect to remote host",
    "Cannot send data to remote host",
    "Canceled",
    "Invalid host responce",
    "Cannot login to remote host. Check your user ID and password",
    "The specified directory is invalid",
    "The specified file cannot be found",
    "The specified URL contains no data",
    "The specified URL has been redirected too many times",
    "The specified URL has been redirected to a non-supported protocol",
    "The connection appears to have been broken prematurely",
    "Decompression error",
    "Timeout"
  };

  tp = (transfer_parms *) cookie;
  jnk[0] = '\0';
  switch (ns)
    {
      case INETST_CONNECTING:
        sprintf(jnk,"Connecting to %s..." EOL,host);
        tp->first_bytes = TRUE;
        break;
      case INETST_AUTHENTICATING:
        sprintf(jnk,"Authenticating user..." EOL);
        tp->first_bytes = TRUE;
        break;
      case INETST_STARTING:
        //sprintf(jnk,"Starting transfer of %s..." EOL,short_name(host));
        tp->first_bytes = TRUE;
        break;
      case INETST_REDIRECTING:
        sprintf(jnk,"Redirecting to %s..." EOL,host);
        tp->first_bytes = TRUE;
        break;

      case INETST_TRANSFERRING:
        if (tp->first_bytes)
          {
            if (tot_bytes>0)
              sprintf(jnk,"Transferring %s (%s) ..." EOL,short_name(host),fmt_size(tot_bytes));
            else
              sprintf(jnk,"Transferring %s ..." EOL,short_name(host));
            tp->first_bytes = FALSE;
          }
        #if _WIN32
          {
            int n;

            if (tot_bytes>0)
              {
                n = MulDiv(cur_bytes,100,tot_bytes);
                if (n<0) n = 0;
                if (n>100) n = 100;
              }
            else
              {
                n = SendMessage(tp->gwp->hpb,PBM_GETPOS,0,0) + 5;
                if (n>100) n = 0;
              }
            SendMessage(tp->gwp->hpb,PBM_SETPOS,n,0);
          }
        #endif
        break;

      case INETST_FINISHED:
        if (ne==INETERR_NOERROR)
          {
            sprintf(jnk,"Finished" EOL EOL);
            SendMessage(tp->gwp->hpb,PBM_SETPOS,100,0);
          }
        else
          {
            for (i=sizeof(nea)/sizeof(nea[0])-1; i>=0; i--)
              if (ne==nea[i]) break;
            if (i>=0) sprintf(jnk,"ERROR: %s" EOL EOL,msg[i]);
            else sprintf(jnk,"ERROR: invalid net error (%d)" EOL EOL,(int)ne);
          }
        break;

      default:
        sprintf(jnk,"INTERNAL ERROR: invalid net status (%d)" EOL EOL,(int)ns);
    }
  if (jnk[0]) log_out(tp->gwp,jnk);
  return (tp->gwp->please_stop) ? 0 : 1;
}

/*  -> page_status */
/*  <- new_tempfile */

static char *
new_tempfile(void)
{
  static char temp[MAX_PATH+1];
#if _WIN32
  int n;

  memset(temp,0,sizeof(temp));
  if (GetTempPath(MAX_PATH-32,temp)==0)
    GetWindowsDirectory(temp,MAX_PATH-32);
  if (temp[0]=='\0') strcpy(temp,"c:\\");
  n = strlen(temp);
  if (temp[n-1]!='\\' && temp[n-1]!='/') temp[n++] = '\\';
  strcpy(temp+n,"gwXXXXXX");
#else
  strcpy(temp,"/tmp/gwXXXXXX");
#endif
  return mktemp(temp);
}

/*  -> new_tempfile */
/*  <- delete_hrefs */

static void
delete_hrefs(char **hrefs)
{
  int i;

  if (!hrefs) return;
  for (i=0; hrefs[i]; i++)
    free(hrefs[i]);
  free(hrefs);
}

/*  -> delete_hrefs */
/*  <- find_hrefs */

static char **
find_hrefs(const char *fname,const char *base_url,char *to_ignore[])
{
  FILE		*f;
  char		**hrefs, *thref;
  int		i,j,c,nrefs,len;
  char		temp[MAX_HREF_SIZE+1];
  char		combined[2*MAX_HREF_SIZE+1];
  static char * preferred[] = {
  		"doom",		"DOOM",		"Doom",
  		"level",	"LEVEL",	"Level",
  		"wad",		"WAD",		"Wad",
  		NULL };

//Msg(0,fname);

  if ( (f=fopen(fname,READBIN)) == NULL ) return NULL;
  if ( (hrefs=(char **)malloc((MAX_HREFS+1)*sizeof(char *))) == NULL )
    {
      fclose(f);
      return NULL;
    }
  memset(hrefs,0,(MAX_HREFS+1)*sizeof(char *));
  nrefs = 0;
  for (;;)
    {
      c = getc(f);
      AGAIN:
      if (c==EOF) break;
      if (c!='h' && c!='H') continue;
      c = getc(f);
      if (c!='r' && c!='R') goto AGAIN;
      c = getc(f);
      if (c!='e' && c!='E') goto AGAIN;
      c = getc(f);
      if (c!='f' && c!='F') goto AGAIN;
      c = getc(f);
      while (c==' ' || c=='\t') c = getc(f);
      if (c!='=') goto AGAIN;
      c = getc(f);
      while (c==' ' || c=='\t') c = getc(f);
      len = 0;
      if (c=='"')
        for (;;)
          {
            c = getc(f);
            if (c=='"' || c=='\r' || c=='\n' || c==EOF) break;
            if (len<MAX_HREF_SIZE) temp[len++] = (char) c;
          }
      else if (c=='\'')
        for (;;)
          {
            c = getc(f);
            if (c=='\'' || c=='\r' || c=='\n' || c==EOF) break;
            if (len<MAX_HREF_SIZE) temp[len++] = (char) c;
          }
      else
        for (;;)
          {
            if (c==' ' || c=='\t' || c=='>' || c=='\r' || c=='\n' || c==EOF) break;
            if (len<MAX_HREF_SIZE) temp[len++] = (char) c;
            c = getc(f);
          }
      temp[len] = '\0';
      if (len==0 || nrefs>=MAX_HREFS) continue;
      if (base_url)
        combine_url(base_url,temp,combined,sizeof(combined));
      else
        strcpy(combined,temp);

      if (!IS_HTTP_URL(combined) && !IS_FTP_URL(combined)) continue;

      if (to_ignore)
        {
          for (i=0; to_ignore[i]; i++)
            if (strstr(combined,to_ignore[i]))
              break;
          if (to_ignore[i]) continue;
        }

      if ( (hrefs[nrefs]=strdup(combined)) == NULL ) continue;
      nrefs++;
    }
  fclose(f);
  if (nrefs==0)
    {
      free(hrefs);
      return NULL;
    }
  /*
   * Arrange the list, so that hrefs with the words "doom" or
   * "level" or "wad" come out first. It helps
   * a lot with the output of search engines.
   */
  for (i=1; i<nrefs; i++)
    {
      for (j=0; preferred[j]; j++)
        if (strstr(hrefs[i],preferred[j])) break;
      if (preferred[j]==NULL) continue;
      thref = hrefs[i];
      for (j=i; j>0; j--) hrefs[j] = hrefs[j-1];
      hrefs[0] = thref;
    }
  return hrefs;
}

/*  -> find_hrefs */
/*  <- end_test_1 */

static BOOL
end_test_1(const char *s,const char *tgt)
{
  int n1,n2;

  n1 = strlen(s);
  n2 = strlen(tgt);
  if (n1<n2)			return FALSE;
  if (lstrcmpi(s+n1-n2,tgt))	return FALSE;
  if (n1==n2)			return TRUE;
  return ( strchr("/?=&\\",s[n1-n2-1]) != NULL );
}

/*  -> end_test_1 */
/*  <- end_test_2 */

static BOOL
end_test_2(const char *s,const char *tgt)
{
  int		n1,n2;
  char		*p;

  if ( (p=strchr(s,'?')) == NULL ) return FALSE;
  *p = '\0';
  n1 = strlen(s);
  n2 = strlen(tgt);
  if (n1<n2)
    {
      *p = '?';
      return FALSE;
    }
  if (lstrcmpi(s+n1-n2,tgt))
    {
      *p = '?';
      return FALSE;
    }
  *p = '?';
  if (n1==n2) return TRUE;
  return ( strchr("/?=&\\",s[n1-n2-1]) != NULL );
}

/*  -> end_test_2 */
/*  <- url_ends_with */

static BOOL
url_ends_with(const char *s,const char *tgt)
{
  if (end_test_1(s,tgt)) return TRUE;
  return end_test_2(s,tgt);
}

/*  -> url_ends_with */
/*  <- file_header */

static const char *
file_header(const char *fname)
{
  FILE		*f;
  static char	buf[16];

  memset(buf,0,16);
  if ( (f=fopen(fname,READBIN)) != NULL )
    {
      fread(buf,4,1,f);
      fclose(f);
    }
  return buf;
}

/*  -> file_header */
/*  <- handle_zip_or_wad */

static BOOL
handle_zip_or_wad(getwad_parms *gwp,const char *fname,const char *urlname)
{
  int		j, nread;
  FILE		*f, *fout;
  const char	*hdr, *stry[2];
  char		*errmsg, *outname, buf[8192];
  int       lookingForAPK3, urlEndsWithPK3;

  hdr = file_header(fname);

  lookingForAPK3 = !strcmp(target_wadname+strlen(target_wadname)-4,".pk3");
  urlEndsWithPK3 = !strcmp(urlname+strlen(urlname)-4,".pk3");

  if (!memcmp(hdr,"PK",2) && !urlEndsWithPK3)
    {
      log_out(gwp,"Opening ZIP file...");
      outname = make_outname(wad_dir,target_wadname);
      if (!zip_extract(fname, target_wadname, outname, 0, 1, &errmsg))
        {
          log_out(gwp,errmsg);
          log_out(gwp,EOL EOL);
          return TRUE;
        }
      hdr = file_header(outname);

      if (memcmp(hdr,"PWAD",4) && memcmp(hdr,"IWAD",4) && !lookingForAPK3)
        {
          log_out(gwp,"This is not a valid Doom WAD file" EOL EOL);
          remove(outname);
          return TRUE;
        }
      if (memcmp(hdr,"PK",2) && lookingForAPK3)
        {
          log_out(gwp,"This is not a valid PK3 file" EOL EOL);
          remove(outname);
          return TRUE;
        }

      log_out(gwp,"SUCCESS!" EOL EOL "WAD FILE INSTALLED." EOL);
      stry[0] = target_txtname;
      stry[1] = "readme.txt";
	  outname = make_outname(wad_dir,target_txtname);
      for (j=0; j<2; j++)
        if (zip_extract(fname,stry[j],outname,0,1,&errmsg))
          break;
      gwp->success = TRUE;
      return TRUE;
    }

  if (!memcmp(hdr,"PWAD",4) || !memcmp(hdr,"IWAD",4) || (urlEndsWithPK3 && !memcmp(hdr,"PK",2)) )
    {
      if (!url_ends_with(urlname,target_wadname) )
        {
          log_out(gwp,"This is NOT the WAD we're looking for." EOL EOL);
          return TRUE;
        }

	  if ( (f=fopen(fname,READBIN)) == NULL)
        {
          log_out(gwp,"Cannot open temp file" EOL);
          return TRUE;
        }
      outname = make_outname(wad_dir,target_wadname);
      if ( (fout=fopen(outname,WRITEBIN)) == NULL )
        {
          log_out(gwp,"Cannot write WAD file" EOL);
          fclose(f);
          return TRUE;
        }
      for (;;)
        {
          nread = (int) fread(buf,1,sizeof(buf),f);
          if (nread<=0) break;
          if (nread != (int) fwrite(buf,1,nread,fout))
            {
              log_out(gwp,"Cannot write WAD file" EOL);
              fclose(f);
              fclose(fout);
              remove(outname);
              return TRUE;
            }
        }
      fclose(fout);
      fclose(f);
      log_out(gwp,"SUCCESS!" EOL EOL "WAD FILE INSTALLED." EOL);
      gwp->success = TRUE;
      return TRUE;
    }

  return FALSE;
}

/*  -> handle_zip_or_wad */
/*  <- parse_page */

static void
parse_page(getwad_parms *gwp,const char *fname,const char *base_url,int level)
{
  transfer_parms	tp;
  inet_error_type	iet;
  char			*tempname, **hrefs;
  int			i;
  char			real_url[4096];

  if (handle_zip_or_wad(gwp,fname,base_url))
    {
      remove(fname);
      return;
    }
  hrefs = find_hrefs(fname,base_url,NULL);
  remove(fname);
  if (!hrefs)
    {
      log_out(gwp,"No HREFs found" EOL EOL);
      return;
    }
  if ( (tempname=new_tempfile()) == NULL )
    {
      log_out(gwp,"Cannot create temp filename" EOL);
      delete_hrefs(hrefs);
      return;
    }

  for (i=0; hrefs[i]; i++)
    {
      if (gwp->please_stop || gwp->success) break;

      if (!enable_ftp && IS_FTP_URL(hrefs[i]))		continue;
      if (!url_ends_with(hrefs[i],target_zipname) &&
          !url_ends_with(hrefs[i],target_wadname) )	continue;

      tp.first_bytes	= TRUE;
      tp.gwp		= gwp;
      if ( (tp.f=fopen(tempname,WRITEBIN)) == NULL )
        {
          log_out(gwp,"Cannot open temp file" EOL);
          continue;
        }
      SendMessage(gwp->hpb,PBM_SETPOS,0,0);
      iet = inet_fetch(hrefs[i],page_transfer,page_status,0,0,NULL,&tp,
      			TIMEOUT_SECS,real_url);
      fclose(tp.f);
      if (gwp->please_stop)
        {
          remove(tempname);
          break;
        }
      if (iet!=INETERR_NOERROR)
        {
          remove(tempname);
          continue;
        }
      if (handle_zip_or_wad(gwp,tempname,real_url))
        {
          remove(tempname);
          continue;
        }
      /*
       * This appears to be an HTML file rather than a ZIP file.
       * Try parsing it (recursion to one level only)
       */
      if (level < 1)
        {
          log_out(gwp,"This is NOT a ZIP or WAD file. Trying to parse it as HTML..." EOL EOL);
          parse_page(gwp,tempname,real_url,level+1);
        }
      else
        {
          log_out(gwp,"This is NOT a ZIP or WAD file. Ignoring it..." EOL EOL);
          remove(tempname);
        }
    }
  delete_hrefs(hrefs);
}

/*  -> parse_page */
/*  <- try_page */

static void
try_page(getwad_parms *gwp,char *page_url)
{
  transfer_parms	tp;
  inet_error_type	iet;
  char			*tempname, real_url[1024];

  if (!enable_ftp && IS_FTP_URL(page_url)) return;
  if ( (tempname=new_tempfile()) == NULL )
    {
      log_out(gwp,"Cannot create temp filename" EOL);
      return;
    }
  tp.first_bytes = TRUE;
  tp.gwp = gwp;
  if ( (tp.f=fopen(tempname,WRITEBIN)) == NULL )
    {
      log_out(gwp,"Cannot open temp file" EOL);
      return;
    }
  SendMessage(gwp->hpb,PBM_SETPOS,0,0);
  iet = inet_fetch(page_url,page_transfer,page_status,0,0,NULL,&tp,TIMEOUT_SECS,real_url);
  fclose(tp.f);
  if (iet!=INETERR_NOERROR || gwp->please_stop)
    {
      remove(tempname);
      return;
    }
  parse_page(gwp,tempname,real_url,0);
}

/*  -> try_page */
/*  <- get_next_wadname */

static BOOL
get_next_wadname(char **pargs)
{
  int	n;
  char	*wadname;

  *pargs = get_arg(*pargs,&wadname);
  if (!wadname || *wadname=='\0') return FALSE;

  strcpymax(target_wadname,wadname,MAX_WADNAME-4+1);

  AnsiLower(target_wadname);
  n = strlen(target_wadname);
  if (n>4 && !strcmp(target_wadname+n-4,".zip"))
    {
      n -= 4;
      target_wadname[n] = '\0';
    }
  if (n<5 || (strcmp(target_wadname+n-4,".wad") && strcmp(target_wadname+n-4,".pk3")) )
    {
      strcpy(target_wadname+n,".wad");
      n += 4;
    }
  strcpy(target_zipname,target_wadname);  strcpy(target_zipname+n-4,".zip");
  strcpy(target_txtname,target_wadname);  strcpy(target_txtname+n-4,".txt");
  return TRUE;
}

/*  -> get_next_wadname */
/*  <- has_replaceable_param */

static BOOL
has_replaceable_param(const char *s)
{
  char c;

  while (*s)
    {
      c = *s++;
      if (c != '%') continue;
      c = *s++;
      if (c == 's') return TRUE;
    }
  return FALSE;
}

/*  -> has_replaceable_param */
/*  <- get_master_search_list */

static void
get_master_search_list(getwad_parms *gwp)
{
  int			i, nmaster, nmasterpages, attempt, n, j;
  transfer_parms	tp;
  inet_error_type	iet;
  FILE			*f;
  char			*tempname;
  char			*master[MAX_PREDEF], buf[MAX_HREF_SIZE+1];

  if ( (tempname=new_tempfile()) == NULL )
    {
      log_out(gwp,"Cannot create temp file" EOL);
      return;
    }
  log_out(gwp,"FETCHING THE SEARCH LIST..." EOL EOL);

  for (i=0; master_pages[i]; i++)
	  ;
  nmasterpages = i;
  srand(time(NULL));

  i = rand() % nmasterpages;
  for (attempt=0;  attempt<nmasterpages;  i=(i+1) % nmasterpages, attempt++)
  {
      tp.first_bytes = TRUE;
      tp.gwp = gwp;
      if ( (tp.f=fopen(tempname,WRITEBIN)) == NULL )
        {
          log_out(gwp,"Cannot open temp file" EOL);
          continue;
        }
      SendMessage(gwp->hpb,PBM_SETPOS,0,0);
      iet = inet_fetch(master_pages[i],page_transfer,page_status,0,0,
                       NULL,&tp,TIMEOUT_SECS,NULL);
      fclose(tp.f);
      if (gwp->please_stop)
        {
          remove(tempname);
          return;
        }
      if (iet!=INETERR_NOERROR)
        {
          remove(tempname);
          continue;
        }
      if ( (f=fopen(tempname,"r")) == NULL )
        {
          log_out(gwp,"Cannot read temp file" EOL);
          remove(tempname);
          continue;
        }
      nmaster = 0;
      while ( fgets(buf,sizeof(buf)-1,f) )
        {
          n = strlen(buf);
          if (n>0 && buf[n-1]=='\n') buf[n-1] = '\0';
          if (!url_ok(buf)) continue;
          if (nmaster>=MAX_PREDEF) break;
          if ( (master[nmaster]=strdup(buf)) != NULL )
            nmaster++;
        }
      fclose(f);
      remove(tempname);
      if (nmaster==0) continue;

      #if _WIN32
      sprintf(buf,"%d",nmaster);
      WritePrivateProfileString(g_ini_section,"nmaster",buf,g_ini_name);
      for (j=0; j<nmaster; j++)
        {
          sprintf(buf,"master%03d",j);
          WritePrivateProfileString(g_ini_section,buf,master[j],g_ini_name);
        }
      #else
      {
        int	use_google, enable_ftp;
        char	wad_dir[MAX_PATH+1];

        use_google = GetPrivateProfileInt(g_ini_section,"use_google",1,g_ini_name);
        enable_ftp = GetPrivateProfileInt(g_ini_section,"enable_ftp",1,g_ini_name);
        GetPrivateProfileString(g_ini_section,"wad_dir","",wad_dir,MAX_PATH-1,g_ini_name);
        ltrim(wad_dir);
        rtrim(wad_dir);
        f = fopen(g_ini_name,"w");
        if (f)
        {
		static char *hdr =
		";----------------------------------------------------------------------\n"
		"; Place this file in /usr/local/etc and edit it according to your\n"
		"; preferences. In most cases you will not need to edit the predefined\n"
		"; locations searched by GetWAD. However you need to MAKE SURE that\n"
		"; the \"wad_dir\" entry points to the correct directory containing the\n"
		"; WAD files. The directory MUST exist and the user running GetWAD\n"
		"; must have WRITE permissions to it.\n"
		";----------------------------------------------------------------------\n"
		"[Options]\n";

        	fprintf(f,"%s",hdr);
        	fprintf(f,"wad_dir=%s\n",wad_dir);
        	fprintf(f,"use_google=%d\n",use_google);
        	fprintf(f,"enable_ftp=%d\n",enable_ftp);
        	fprintf(f,"nmaster=%d\n",nmaster);
		for (j=0; j<nmaster; j++)
			fprintf(f,"master%03d=%s\n",j,master[j]);
		fclose(f);
	}
      }
      #endif
      for (j=0; j<nmaster; j++)
      	free(master[j]);
      break;
    }
}

/*  -> get_master_search_list */
/*  <- back_work */

static void
back_work(void *pv)
{
  getwad_parms		*gwp;
  transfer_parms	tp;
  inet_error_type	iet;
  int			i, nwads, nsuccess, j, npages, npasses;
  char			*tempname,**hrefs, **spages;
  char			search_page[MAX_HREF_SIZE+MAX_WADNAME+1+16];
  static char		*google_page = "http://www.google.com/search?"
  				"num=100&hl=en&ie=ISO-8859-1&q=%s";
  static char		*google_ignore[] = {
  				".google.com/",
  				"/search?q=cache:",
  				NULL };

  gwp = (getwad_parms *) pv;
  gwp->running = TRUE;

  nwads = nsuccess = 0;

  if (getwad_version != 1)
    {
      char temp_dir[MAX_PATH+1];

      get_master_search_list(gwp);
      strcpy(temp_dir,wad_dir);
      read_params(temp_dir,TRUE);
    }

  if (!gwp->please_stop)
    {
      while (get_next_wadname(&(gwp->pargs)))
        {
          sprintf(search_page,"SEARCHING FOR %s..." EOL EOL,target_wadname);
          log_out(gwp,search_page);

          gwp->success = FALSE;
          if (getwad_version==1)
            {
              npages = npredef;
              spages = predef;
            }
          else
            {
              npages = nextras;
              spages = extras;
            }
          for (i=0; i<npages; i++)
            {
              npasses = has_replaceable_param(spages[i]) ? 2 : 1;
              for (j=0; j<npasses; j++)
                {
                  sprintf(search_page,spages[i],
                          (j==0) ? target_zipname : target_wadname );
                  try_page(gwp,search_page);
                  if (gwp->success || gwp->please_stop) goto GETOUT;
                }
            }

          if (use_google)
            {
              for (j=0; j<2; j++)
                {
                  sprintf(search_page,google_page,
                          (j==0) ? target_zipname : target_wadname );
                  if ( (tempname=new_tempfile()) == NULL )
                    {
                      log_out(gwp,"Cannot create temp file" EOL);
                      goto GETOUT;
                    }
                  tp.first_bytes = TRUE;
                  tp.gwp = gwp;
                  if ( (tp.f=fopen(tempname,WRITEBIN)) == NULL )
                    {
                      log_out(gwp,"Cannot open temp file" EOL);
                      goto GETOUT;
                    }
                  SendMessage(gwp->hpb,PBM_SETPOS,0,0);
                  iet = inet_fetch(search_page,page_transfer,page_status,0,0,NULL,&tp,TIMEOUT_SECS,NULL);
                  fclose(tp.f);
                  if (gwp->please_stop)
                    {
                      remove(tempname);
                      goto GETOUT;
                    }
                  if (iet!=INETERR_NOERROR)
                    {
                      remove(tempname);
                      continue;
                    }
                  hrefs = find_hrefs(tempname,NULL,google_ignore);
                  remove(tempname);
                  if (!hrefs) continue;
                  for (i=0; hrefs[i]; i++)
                    {
                      try_page(gwp,hrefs[i]);
                      if (gwp->success || gwp->please_stop)
                        {
                          delete_hrefs(hrefs);
                          goto GETOUT;
                        }
                    }
                  delete_hrefs(hrefs);
                }
            }

          GETOUT:

          nwads++;
          if (gwp->success) nsuccess++;

          if      (gwp->please_stop)	break;
          else if (!gwp->success)	log_out(gwp,"WAD NOT FOUND" EOL);
          log_out(gwp,EOL);
        }
    }
  gwp->success = (!gwp->please_stop && nwads==nsuccess);
  PostMessage(gwp->hdlg,WM_COMMAND,IDD_FINISHED,0);
}

/*  -> back_work */
#if _WIN32
/*  <- CheckWADName */

static BOOL FAR WINAPI
CheckWADName(HSRECORD *p)
{
  int	i;
  char	*s;
  static char	*admissible = "abcdefghijklmnopqrstuvwxyz"
                              "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                              "0123456789-_";

  rtrim(p->fld[0].buf);
  ltrim(p->fld[0].buf);
  s = p->fld[0].buf;
  if (s[0]=='\0')
    {
      MessageBeep(0);
      return FALSE;
    }
  for (i=0; s[i]; i++)
    if (!strchr(admissible,s[i]))
      {
        Msg(p->hwnd,"The WAD name can contain only alphanumeric characters, hyphens or underscores!");
        return FALSE;
      }
  return TRUE;
}

/*  -> CheckWADName */
/*  <- GetWADProc */


BOOL WINAPI
GetWADProc(HWND hdlg,UINT msg,UINT wparam,LONG lparam)
{
  HWND			h;
  static getwad_parms	gwp;
  static BOOL		first_time;

  switch(msg)
    {
/*  <-       WM_INITDIALOG */

      case WM_INITDIALOG:
        center_window(hdlg);
        gwp.hdlg = hdlg;
        gwp.hpb = GetDlgItem(hdlg,IDD_PB);
        gwp.hst = GetDlgItem(hdlg,IDD_ST);
        gwp.please_stop = FALSE;
        gwp.running = FALSE;
        gwp.success = FALSE;
        gwp.pargs = (char *) lparam;
        SendMessage(gwp.hpb,PBM_SETRANGE,0,MAKELPARAM(0,100));
        first_time = TRUE;
        return FALSE;
/*  ->       WM_INITDIALOG */
/*  <-       WM_PAINT */

      case WM_PAINT:
        if (first_time)
          {
            PostMessage(hdlg,WM_COMMAND,IDD_STARTBACK,0);
            first_time = FALSE;
          }
        break;
/*  ->       WM_PAINT */

      case WM_COMMAND:
        switch (GET_WM_COMMAND_ID(wparam,lparam))
          {
/*  <-             IDCANCEL */
            case IDCANCEL:
              if (gwp.running)
                {
                  SetDlgItemText(hdlg,IDCANCEL,"Canceling. Please wait...");
                  gwp.please_stop = TRUE;
                }
              else EndDialog(hdlg,gwp.success);
              return TRUE;

/*  ->             IDCANCEL */
/*  <-             IDD_STARTBACK */

            case IDD_STARTBACK:
              if (_beginthread(back_work,0,&gwp)==-1L)
                {
                  Msg(hdlg,"Cannot start background thread!");
                  PostMessage(hdlg,WM_COMMAND,IDCANCEL,0);
                }
              return TRUE;
/*  ->             IDD_STARTBACK */
/*  <-             IDD_FINISHED */

            case IDD_FINISHED:
              h = GetDlgItem(hdlg,IDCANCEL);
              SetWindowText(h,"Close");
              EnableWindow(h,TRUE);
              gwp.running = FALSE;
              if (gwp.success && dont_wait_upon_success)
                PostMessage(hdlg,WM_COMMAND,IDCANCEL,0);
              return TRUE;
/*  ->             IDD_FINISHED */
          }
        break;
    }
  return FALSE;
}


/*  -> GetWADProc */
/*  <- getwad_fetch_aux */

static int
getwad_fetch_aux(HWND hwnd,LPCSTR cmdline,LPCSTR waddir,BOOL dont_wait,
                 LPCSTR ini_name, LPCSTR ini_sec,int ver,LPCSTR server_search)
{
  int	rc;
  char	buf[256];

  if (ini_name && *ini_name) g_ini_name = ini_name;
  if (ini_sec  && *ini_sec ) g_ini_section = ini_sec;
  g_server_search = server_search;
  getwad_version = ver;

  if (!read_params(waddir,FALSE)) return -1;

  if (cmdline==NULL || *cmdline=='\0')
    {
      HSRECORD	r;

      memset(buf,0,sizeof(buf));
      memset(&r,0,sizeof(r));
      r.hwnd = hwnd;
      r.caption = "Enter the desired WAD name";
      r.oklabel = "OK";
      r.canlabel = "Cancel";
      r.valfun = (valfun_ptr) CheckWADName;
      r.nfields = 1;
      r.fld[0].label = "";
      r.fld[0].buf = buf;
      r.fld[0].size = MAX_WADNAME-4;
      r.fld[0].enabled = 1;
      if (!edit_record(hinst,"dlg1",&r)) return 0;
      ltrim(buf);
      rtrim(buf);
      if (buf[0]=='\0') return 0;
    }
  else
    {
      strcpymax(buf,cmdline,sizeof(buf));
    }
  inet_init();
  InitCommonControls();
  dont_wait_upon_success = dont_wait;
  rc = DialogBoxParam(hinst,"GETWAD",hwnd,GetWADProc,(LPARAM)buf);
  inet_done();
  return rc;
}


/*  -> getwad_fetch_aux */
/*  <- getwad_fetch		public */

int WINAPI
getwad_fetch(HWND hwnd,LPCSTR cmdline,LPCSTR waddir,BOOL dont_wait,
             LPCSTR ini_name, LPCSTR ini_sec)
{
  return getwad_fetch_aux(hwnd,cmdline,waddir,dont_wait,ini_name,ini_sec,
                          1,NULL);
}


/*  -> getwad_fetch		public */
/*  <- getwad2_fetch		public */

int WINAPI
getwad2_fetch(HWND hwnd,LPCSTR cmdline,LPCSTR waddir,BOOL dont_wait,
             LPCSTR ini_name, LPCSTR ini_sec, LPCSTR server_search)
{
  return getwad_fetch_aux(hwnd,cmdline,waddir,dont_wait,ini_name,ini_sec,
                          2,server_search);
}


/*  -> getwad2_fetch		public */
/*  <- getwad_init			public */

void WINAPI
getwad_init(HINSTANCE hinstance)
{
  hinst = hinstance;
  npredef = nextras = 0;
  getwad_version = 1;
}

/*  -> getwad_init			public */
/*  <- DllEntryPoint */
#if !GETWAD_STATIC

BOOL WINAPI _CRT_INIT(HINSTANCE,DWORD,LPVOID);

BOOL WINAPI
DllEntryPoint(HINSTANCE hinstance,DWORD reason,LPVOID p)
{
#ifndef __TURBOC__
  if (reason==DLL_PROCESS_ATTACH || reason==DLL_THREAD_ATTACH)
    if(!_CRT_INIT(hinst,reason,p)) return FALSE;
  if (reason==DLL_PROCESS_DETACH || reason==DLL_THREAD_DETACH)
    if (!_CRT_INIT(hinst,reason,p)) return FALSE;
#endif
  if (reason==DLL_PROCESS_ATTACH)
    getwad_init(hinstance);
  return TRUE;
}

#endif
/*  -> DllEntryPoint */
#else
/*  <- getwad_fetch_aux */

int
getwad_fetch_aux(const char *cmdline,const char *waddir,const char *ini_name,const char *ini_sec,
                 int ver, const char *server_search)
{
  getwad_parms	gwp;
  char		buf[8192];

  if (ini_name && *ini_name) g_ini_name = ini_name;
  if (ini_sec  && *ini_sec ) g_ini_section = ini_sec;
  g_server_search = server_search;
  getwad_version = ver;
  if (!read_params(waddir,FALSE)) return -1;
  if (!cmdline || *cmdline=='\0') return 0;
  strcpymax(buf,cmdline,sizeof(buf));
  inet_init();
  memset(&gwp,0,sizeof(gwp));
  gwp.pargs = buf;
  back_work(&gwp);
  inet_done();
  return gwp.success;
}

/*  -> getwad_fetch_aux */
/*  <- getwad_fetch		public */

int
getwad_fetch(const char *cmdline,const char *waddir,const char *ini_name,const char *ini_sec)
{
  return getwad_fetch_aux(cmdline,waddir,ini_name,ini_sec,1,NULL);
}

/*  -> getwad_fetch		public */
/*  <- getwad2_fetch		public */

int
getwad2_fetch(const char *cmdline,const char *waddir,const char *ini_name, const char *ini_sec,
              const char *server_search)
{
  return getwad_fetch_aux(cmdline,waddir,ini_name,ini_sec,2,server_search);
}

/*  -> getwad2_fetch		public */
#endif
