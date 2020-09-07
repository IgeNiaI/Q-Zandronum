/*  <- declarations */
/*
 * Library routines to fetch files through the HTTP of FTP protocols.
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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#ifdef _WIN32
#  include <windows.h>
#  include <winsock.h>
#  include <wininet.h>
#  include <process.h>
#  define START_THREAD(f,p)	(_beginthread((f),0,(p)) != -1L)
#else
#  include <unistd.h>
#  include <pthread.h>
#  include <netdb.h>
#  include <sys/types.h>
#  include <sys/socket.h>
#  include <sys/time.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  define SOCKET_ERROR		(-1)
#  define INVALID_SOCKET	(-1)
   typedef int			SOCKET;
   typedef int   		WSADATA;
   typedef struct sockaddr_in	SOCKADDR_IN;
   typedef SOCKADDR_IN		*LPSOCKADDR;
#  define closesocket(x)	close(x)
#  define _sleep(n)		sleep(1)
#  define BOOL			int
#  define TRUE			(1)
#  define FALSE			(0)
#  define DWORD			unsigned
#  define InternetGetCookie(a,b,c,d)	(0)
#  define MAX_PATH		256
#  define START_THREAD(f,p)	(pthread_create(&tid,NULL,(f),(p))==0)
static pthread_t		tid;
#endif
#define IS_HTTP_URL(u)	(!memcmp(u,"http://",7))
#define IS_FTP_URL(u)	(!memcmp(u,"ftp://",6))

#include <time.h>
#include "inet.h"
#include "base64.h"
#define ISDIGIT(x)		((x)>='0' && (x)<='9')
#define	MAX_URL_SIZE		16384

#ifdef _WIN32
static CRITICAL_SECTION	timeout_cs;
#  define INIT_CRITICAL_TIMEOUT()	InitializeCriticalSection(&timeout_cs)
#  define DONE_CRITICAL_TIMEOUT()	DeleteCriticalSection(&timeout_cs)
#  define BEGIN_CRITICAL_TIMEOUT()	EnterCriticalSection(&timeout_cs)
#  define END_CRITICAL_TIMEOUT()	LeaveCriticalSection(&timeout_cs)
#else
static pthread_mutex_t timeout_cs = PTHREAD_MUTEX_INITIALIZER;
#  define INIT_CRITICAL_TIMEOUT()
#  define DONE_CRITICAL_TIMEOUT()
#  define BEGIN_CRITICAL_TIMEOUT()	pthread_mutex_lock(&timeout_cs)
#  define END_CRITICAL_TIMEOUT()	pthread_mutex_unlock(&timeout_cs)
#endif

typedef struct
{
  time_t		nsecs;
  BOOL			stop_bgthread;
  BOOL			bgthread_on;
  time_t		last_time;
  inet_error_type	rc;
  SOCKET		skt;
} transfer_context;

static	char		proxy_name[MAX_PATH+1];
static	int		proxy_port;
static	int		proxy_enabled;

/*  -> declarations */
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
#if !_WIN32
/*  <- strcpymax */

static void
strcpymax(char *dest,char *src,int destsz)
{
  int n;

  n = strlen(src);
  if (n>=destsz) n = destsz-1;
  memcpy(dest,src,n);
  dest[n] = '\0';
}

/*  -> strcpymax */
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
#endif
/*  <- combine_url */

static void
combine_url(char *base_url,char *rel_url,char *outbuf,unsigned bufsz)
{
  #if _WIN32
    memset(outbuf,0,bufsz);
    InternetCombineUrl(base_url,rel_url,outbuf,(unsigned long *) &bufsz,ICU_NO_ENCODE);
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
/*  <- skt_init */

static BOOL
skt_init(void)
{
  return TRUE;
}

/*  -> skt_init */
/*  <- skt_done */

static void
skt_done(void)
{
}

/*  -> skt_done */
/*  <- skt_close */

static void
skt_close(SOCKET *s)
{
  if (*s == SOCKET_ERROR) return;
  BEGIN_CRITICAL_TIMEOUT();
  closesocket(*s);
  *s = SOCKET_ERROR;
  END_CRITICAL_TIMEOUT();
}

/*  -> skt_close */
/*  <- skt_open */

/*
 * Return values:
 * 	 0: OK
 *	 1: cannot create socket
 *	 3: cannot connect socket
 *	 4: Unknown host
 */
static int
skt_open(SOCKET *s0,char *host,int port)
{
  SOCKET		s;
  SOCKADDR_IN		sa;
  struct hostent	*he;

  *s0 = SOCKET_ERROR;
  s = socket(PF_INET,SOCK_STREAM,0);
  if (s == INVALID_SOCKET)
    {
      return 1;
    }

  sa.sin_family = AF_INET;
  sa.sin_port = htons((short) port);
  if ( (sa.sin_addr.s_addr=inet_addr(host)) != (u_long) 0xFFFFFFFFLU )
    ;
  else if ( (he=gethostbyname(host)) != NULL )
    memcpy(&sa.sin_addr,he->h_addr,he->h_length);
  else
    {
      skt_close(&s);
      return 4;
    }
  if (connect(s,(LPSOCKADDR)&sa,sizeof(sa)))
    {
      skt_close(&s);
      return 3;
    }

  *s0 = s;
  return 0;
}

/*  -> skt_open */
/*  <- skt_write */

static int
skt_write(SOCKET skt,char *buf,int n)
{
  int rc,tosend;

  if (skt==SOCKET_ERROR) return 0;
  while (n>0)
    {
      tosend = (n>32768) ? 32768 : n;
      rc = send(skt,buf,tosend,0);
      if (rc==SOCKET_ERROR || rc<=0) return 0;
      buf += rc;
      n -= rc;
    }
  return 1;
}

/*  -> skt_write */
/*  <- skt_read */

static int
skt_read(transfer_context *tc,char *buf,int bufsz)
{
  int n;

  if (tc->skt==SOCKET_ERROR) return 0;
  tc->last_time = time(NULL);
  n = recv(tc->skt,buf,bufsz,0);
  BEGIN_CRITICAL_TIMEOUT();
  tc->last_time = 0;
  END_CRITICAL_TIMEOUT();
  return n;
}

/*  -> skt_read */
/*  <- skt_printf */

static int
skt_printf(SOCKET skt,char *fmt,...)
{
  va_list	ap;
  char		jnk[MAX_URL_SIZE];

  va_start(ap,fmt);
  vsprintf(jnk,fmt,ap);
  va_end(ap);
  return skt_write(skt,jnk,strlen(jnk));
}

/*  -> skt_printf */
/*  <- skt_getc */

static int
skt_getc(SOCKET skt)
{
  unsigned char buf[4];

  if (skt==SOCKET_ERROR) return EOF;
  if (recv(skt,(char *)buf,1,0) != 1) return EOF;
  return buf[0];
}

/*  -> skt_getc */
/*  <- response_code */

static int
response_code(char *s)
{
  int n;

  if (!ISDIGIT(*s)) return -1;
  n = 0;
  while (ISDIGIT(*s))
    {
      n = 10*n + (int)(*s-'0');
      s++;
    }
  return (*s==' ') ? n : -1;
}

/*  -> response_code */
/*  <- ftp_response */

static int
ftp_response(SOCKET s,int okcode1,int okcode2,char *buf,int bufsz)
{
  int	c,i,code;

  do
    {
      for (i=0; i<bufsz-1; i++)
        {
          do
            {
              c = skt_getc(s);
              if (c==EOF)
                {
                  buf[i] = '\0';
                  return 0;
                }
            }
          while (c=='\r');
          if (c=='\n') break;
          buf[i] = (char) c;
        }
      buf[i] = '\0';
      code = response_code(buf);
    }
  while (code<0);
  if (code==okcode1) return 1;
  if (okcode2>0 && code==okcode2) return 1;
  return 0;
}

/*  -> ftp_response */
/*  <- get_byte_count */

static int
get_byte_count(char *buf)
{
  char	*p;
  int	n;

  n = strlen(buf);
  if (n<10 || strcmp(buf+n-8," bytes).")) return 0;
  p = buf + n-8;
  while (*p==' ')
    {
      p--;
      if (p<=buf) return 0;
    }
  if (!ISDIGIT(*p)) return 0;
  do
    {
      p--;
      if (p<=buf) return 0;
    }
  while (ISDIGIT(*p));
  p++;
  n = 0;
  while (ISDIGIT(*p))
    {
      n = 10*n + (int)(*p-'0');
      p++;
    }
  return (n<0) ? 0 : n;
}

/*  -> get_byte_count */
/*  <- dummystatusproc */

static int WINAPI
dummystatusproc(void *cookie,inet_status_type ns,int ne,
           char *host,unsigned cur_bytes,unsigned tot_bytes)
{
  return 1;
}

/*  -> dummystatusproc */
/*  <- ftp_fetch */

static inet_error_type
ftp_fetch(char *orig_url,inet_transferprocptr transf,inet_statusprocptr stf,
	  void *cookie,transfer_context *tc)
{
  SOCKET	s;
  int		i,portnum,v[6],total_bytes,bytes_so_far;
  char		*url,*p,*pdoc,*user,*pwd,*host,*pfname,ipadr[32],
  		jnk[MAX_URL_SIZE],temp_url[MAX_URL_SIZE];

  if (strlen(orig_url)>=sizeof(temp_url))		return INETERR_BADURL;
  strcpy(temp_url,orig_url);

  url = temp_url;
  if (!IS_FTP_URL(url))					return INETERR_BADURL;

  url += 6;
  if ( (pdoc=strchr(url,'/')) == NULL )			return INETERR_BADURL;
  *pdoc++ = '\0';
  if (*pdoc=='\0')					return INETERR_BADURL;

  if ( (pfname=strrchr(pdoc,'/')) != NULL )
    *pfname++ = '\0';
  else
    {
      pfname = pdoc;
      pdoc = "";
    }
  if (*pfname=='\0')					return INETERR_BADURL;

  if ( (host=strchr(url,'@')) != NULL )
    {
      *host++ = '\0';
      user = url;
    }
  else
    {
      user = "anonymous";
      host = url;
    }

  if ( (pwd=strchr(user,':')) != NULL )
    *pwd++ = '\0';
  else
    pwd = "guest@";

  if ( (p=strchr(host,':')) == NULL )
    portnum = 21;
  else
    {
      *p++ = '\0';
      portnum = atoi(p);
      if (portnum==0) portnum = 21;
    }

  if (*host=='\0' || *user=='\0')			return INETERR_BADURL;

  if (!stf(cookie,INETST_CONNECTING,0,host,0,0))	return INETERR_CANCELED;
  if (skt_open(&s,host,portnum))			return INETERR_NOCONNECT;
  if (!ftp_response(s,220,0,jnk,sizeof(jnk)))		{ skt_close(&s); return INETERR_BADRESPONSE;	}

  if (!stf(cookie,INETST_AUTHENTICATING,0,NULL,0,0))	{ skt_close(&s); return INETERR_CANCELED;	}
  if (!skt_printf(s,"USER %s\r\n",user))		{ skt_close(&s); return INETERR_CANNOTSEND;	}
  if (!ftp_response(s,331,0,jnk,sizeof(jnk)))		{ skt_close(&s); return INETERR_NOLOGIN;		}

  if (!skt_printf(s,"PASS %s\r\n",pwd))			{ skt_close(&s); return INETERR_CANNOTSEND;	}
  if (!ftp_response(s,230,0,jnk,sizeof(jnk)))		{ skt_close(&s); return INETERR_NOLOGIN;		}

  if (!stf(cookie,INETST_STARTING,0,orig_url,0,0))	{ skt_close(&s); return INETERR_CANCELED;	}
  if (!skt_printf(s,"TYPE I\r\n"))			{ skt_close(&s); return INETERR_CANNOTSEND;	}
  if (!ftp_response(s,200,0,jnk,sizeof(jnk)))		{ skt_close(&s); return INETERR_BADRESPONSE;	}

  if (*pdoc)
    {
      if (!skt_printf(s,"CWD %s\r\n",pdoc))		{ skt_close(&s); return INETERR_CANNOTSEND;	}
      if (!ftp_response(s,250,0,jnk,sizeof(jnk)))	{ skt_close(&s); return INETERR_BADPATH;		}
    }

  if (!skt_printf(s,"PASV\r\n"))			{ skt_close(&s); return INETERR_CANNOTSEND;	}
  if (!ftp_response(s,227,0,jnk,sizeof(jnk)))
    {
      BAD_PASV_RESP:
      skt_close(&s);
      return INETERR_BADRESPONSE;
    }
  p = jnk + 4;
  while (*p && !ISDIGIT(*p)) p++;
  if (*p=='\0') goto BAD_PASV_RESP;

  for (i=0; i<6; i++)
    {
      if (i>0)
        {
          if (*p!=',') goto BAD_PASV_RESP;
          p++;
          while (*p==' ') p++;
        }
      if (!ISDIGIT(*p)) goto BAD_PASV_RESP;
      v[i] = 0;
      while (ISDIGIT(*p))
        {
          v[i] = 10*v[i] + (int)(*p-'0');
          p++;
        }
      if (v[i]<0 || v[i]>255) goto BAD_PASV_RESP;
      while (*p==' ') p++;
    }
  sprintf(ipadr,"%d.%d.%d.%d",v[0],v[1],v[2],v[3]);
  portnum = 256*v[4]+v[5];

  if (!skt_printf(s,"RETR %s\r\n",pfname))		{ skt_close(&s); return INETERR_CANNOTSEND;	}

  if (!stf(cookie,INETST_CONNECTING,0,ipadr,0,0))	{ skt_close(&s); return INETERR_CANCELED;	}
  if (skt_open(&(tc->skt),ipadr,portnum))		{ skt_close(&s); return INETERR_NOCONNECT;	}

  if (!ftp_response(s,125,150,jnk,sizeof(jnk)))
    {
      skt_close(&s);
      skt_close(&(tc->skt));
      return INETERR_BADFILE;
    }

  total_bytes = get_byte_count(jnk+4);
  bytes_so_far = 0;
  for (;;)
    {
      if ( (i=skt_read(tc,jnk,sizeof(jnk))) <= 0 ) break;
      bytes_so_far += i;
      if ( !stf(cookie,INETST_TRANSFERRING,0,orig_url,bytes_so_far,total_bytes) ||
           transf(cookie,(unsigned char *)jnk,i) <= 0 )
        {
          skt_close(&(tc->skt));
          skt_close(&s);
          return INETERR_CANCELED;
        }
    }
  skt_close(&(tc->skt));

  if (!ftp_response(s,226,0,jnk,sizeof(jnk)))		{ skt_close(&s); return INETERR_BADRESPONSE;	}

  skt_close(&s);
  if (bytes_so_far==0) return INETERR_NODATA;
  if (total_bytes>0 && bytes_so_far<total_bytes) return INETERR_SHORTFILE;

  return INETERR_NOERROR;
}

/*  -> ftp_fetch */
/*  <- is_url */

static int
is_url(char *s)
{
  return (IS_HTTP_URL(s) || IS_FTP_URL(s));
}

/*  -> is_url */
/*  <- trim_newline */

static void
trim_newline(char *s)
{
  char *p;

  if ( (p=strchr(s,'\r')) != NULL ) *p = '\0';
  if ( (p=strchr(s,'\n')) != NULL ) *p = '\0';
}

/*  -> trim_newline */
/*  <- http_fetch */

static inet_error_type
http_fetch(char *orig_url, int include_headers, inet_transferprocptr transf,
           inet_statusprocptr stf,char *red_url, unsigned red_url_size,
           char	*def_host,int use_cookies,char *referer,void *cookie,
           transfer_context *tc)
{
  inet_error_type	rc;
  int			n,portnum,state,bytes_so_far,total_bytes,first_time,v;
  char			*url,*p,*pdoc,*q,*user_pwd,
  			jnk[MAX_URL_SIZE+1],temp_url[MAX_URL_SIZE+1],
  			temp_pwd[128];

  if (strlen(orig_url)>=sizeof(temp_url))		return INETERR_BADURL;
  strcpy(temp_url,orig_url);

  url = temp_url;
  if (!IS_HTTP_URL(url))				return INETERR_BADURL;

  url += 7;
  if ( (pdoc=strchr(url,'/')) == NULL )
    pdoc = "";
  else
    *pdoc++ = '\0';

  user_pwd = NULL;
  if ( (p=strchr(url,'@')) != NULL )
    {
      *p++ = '\0';
      user_pwd = url;
      url = p;
      if (!strchr(user_pwd,':')  || *user_pwd==':')  user_pwd = NULL;
    }

  if ( (p=strchr(url,':')) == NULL )
    portnum = 80;
  else
    {
      *p++ = '\0';
      portnum = atoi(p);
      if (portnum==0) portnum = 80;
    }

  if (*url=='\0')					return INETERR_BADURL;

  if (!stf(cookie,INETST_CONNECTING,0,url,0,0))		return INETERR_CANCELED;
  if (skt_open(&(tc->skt),url,portnum))			return INETERR_NOCONNECT;

  sprintf(jnk,"GET %s%s HTTP/1.0\r\n"
              "Accept: */*\r\n"
              "User-Agent: inet library\r\n"
              "Host: %s\r\n",
              (is_url(pdoc)) ? "" : "/",
              pdoc,(def_host[0]) ? def_host : url);
  if (referer && *referer)
    sprintf(jnk+strlen(jnk),"Referer: %s\r\n",referer);
  if (user_pwd)
    {
      n = b64_encode((unsigned char *)user_pwd,strlen(user_pwd),(unsigned char *)temp_pwd,sizeof(temp_pwd)-1);
      if (n>0)
        {
          temp_pwd[n] = '\0';
          sprintf(jnk+strlen(jnk),"Authorization: Basic %s\r\n",temp_pwd);
        }
    }
  if (use_cookies)
    {
      DWORD	max_cookie_size;
      unsigned	l1,l2;

      l1 = strlen(jnk);
      strcpy(jnk+l1,"Cookie: ");
      l2 = strlen(jnk);
      max_cookie_size = sizeof(jnk) - l2 - 6;
      if (!InternetGetCookie(orig_url,"jnk",jnk+l2,&max_cookie_size))
        jnk[l1] = '\0';
      else strcat(jnk,"\r\n");
    }
  strcat(jnk,"\r\n");

  if (*pdoc=='\0') pdoc = "/";
  if (!stf(cookie,INETST_STARTING,0,pdoc,0,0))		{ skt_close(&(tc->skt)); return INETERR_CANCELED;	}
  if (!skt_write(tc->skt,jnk,strlen(jnk)))		{ skt_close(&(tc->skt)); return INETERR_CANNOTSEND;	}

  rc = INETERR_NODATA;
  state = (include_headers) ? 1 : 0;
  total_bytes = 0;
  bytes_so_far = 0;
  first_time = 1;
  for (;;)
    {

      memset(jnk,0,sizeof(jnk));
      if ( (n=skt_read(tc,jnk,sizeof(jnk)-1)) <= 0 ) break;

      if (first_time && !memcmp(jnk,"HTTP/",5))
        {
          for (p=jnk+5;  ISDIGIT(*p) || *p=='.';  p++)
            ;
          while (*p==' ') p++;
          if (ISDIGIT(*p))
            {
              v = 0;
              while (ISDIGIT(*p))
                {
                  v = 10*v + (int)(*p - '0');
                  p++;
                }
              if (v==300 || v==301 || v==302)
                {
                  if ( (p=strstr(jnk,"\nLocation:")) != NULL )
                    {
                      p += 10;
                      while (*p==' ') p++;
                      trim_newline(p);
                      if (*p && strlen(p)<red_url_size)
                        {
                          strcpy(red_url,p);
                          stf(cookie,INETST_REDIRECTING,0,red_url,0,0);
                          rc = INETERR_REDIRECTED;
                          break;
                        }
                    }
                }
              if (v!=200)
                {
                  rc = INETERR_BADFILE;
                  break;
                }
            }
        }
      first_time = 0;

      p = jnk;
      if (state==0)
        {
          if ( (p=strstr(p,"\r\n\r\n")) == NULL ) continue;
          *p = '\0';
          if ( (q=strstr(jnk,"\nContent-Length:")) == NULL )
            q = strstr(jnk,"\nContent-length:");
          if (q)
            {
              q += 16;
              while (*q==' ') q++;
              while (ISDIGIT(*q))
                {
                  total_bytes = 10*total_bytes + (int)(*q-'0');
                  q++;
                }
            }
          p += 4;
          n -= p-jnk;
          state++;
        }
      if (state==1)
        {
          if (n>0)
            {
              bytes_so_far += n;
              rc = INETERR_NOERROR;
              if (transf(cookie,(unsigned char *)p,n) <= 0)
                {
                  rc = INETERR_CANCELED;
                  break;
                }
            }
        }

      if (!stf(cookie,INETST_TRANSFERRING,0,orig_url,bytes_so_far,total_bytes))
        {
          rc = INETERR_CANCELED;
          break;
        }

    }

  skt_close(&(tc->skt));
  if (rc==INETERR_NOERROR && total_bytes>0 && bytes_so_far<total_bytes)
    rc = INETERR_SHORTFILE;
  return rc;
}

/*  -> http_fetch */
/*  <- timeout_watch */

static void
#ifndef _WIN32
*
#endif
timeout_watch(void *pv)
{
  time_t		t,t0;
  transfer_context	*tc;

  tc = (transfer_context *) pv;
  tc->bgthread_on = TRUE;
  for (;;)
    {
      _sleep(500);
      if (tc->stop_bgthread) break;
      if ( (t0=tc->last_time) != 0)
        {
          t = time(NULL);
          t -= t0;
          if (t >= tc->nsecs)
            {
              tc->rc = INETERR_TIMEOUT;
              skt_close(&(tc->skt));
              break;
            }
        }
    }

  tc->bgthread_on = FALSE;

#ifndef _WIN32
  return NULL;
#endif
}

/*  -> timeout_watch */
/*  <- inet_fetch			public */

inet_error_type
inet_fetch( char *url, inet_transferprocptr transf_f,
           inet_statusprocptr status_f, int get_hdrs,
           int use_cookies,char *referer,void *cookie,
           unsigned timeout_seconds, char *real_url )
{
  int			i;
  inet_error_type	rc;
  transfer_context	tc;
  char			*p,
  			temp_url[MAX_URL_SIZE],red_url[MAX_URL_SIZE],
  			host[MAX_URL_SIZE],jnk_url[MAX_URL_SIZE];

  if (!status_f)	status_f = dummystatusproc;

  if (!url || !transf_f)
    {
      status_f(cookie,INETST_FINISHED,INETERR_BADPARMS,NULL,0,0);
      return INETERR_BADPARMS;
    }
  if (!is_url(url))
    {
      status_f(cookie,INETST_FINISHED,INETERR_BADURL,NULL,0,0);
      return INETERR_BADURL;
    }

  if (!skt_init())
    {
      status_f(cookie,INETST_FINISHED,INETERR_NOCONNECT,NULL,0,0);
      return INETERR_NOCONNECT;
    }
  INIT_CRITICAL_TIMEOUT();

  tc.stop_bgthread	= FALSE;
  tc.bgthread_on	= FALSE;
  tc.last_time		= 0;
  tc.rc			= INETERR_NOERROR;
  tc.skt		= SOCKET_ERROR;
  tc.nsecs		= timeout_seconds;

  if ( timeout_seconds>0 )
    {
      if (!START_THREAD(timeout_watch,&tc))
        {
          tc.nsecs = 0;
        }
      else
        {
          /* Make sure that the background thread is on */
          while (!tc.bgthread_on)
            _sleep(100);
        }
    }

  for (i=5; i>0; i--)
    {
      if (IS_HTTP_URL(url))	p = url+7;
      else if (IS_FTP_URL(url))	p = url+6;
      else
        {
          rc = INETERR_BADREDIRECTION;
          break;
        }
      strcpy(host,p);
      if ( (p=strchr(host,'/')) != NULL ) *p = '\0';
      if ( (p=strchr(host,'@')) != NULL ) strdel(host,0,1+(int)(p-host));
      if ( (p=strchr(host,':')) != NULL ) *p = '\0';

	  if (real_url) strcpy(real_url,url);

      if (proxy_enabled)
        sprintf(temp_url,"http://%s:%d/%s",proxy_name,proxy_port,url);
      else
        strcpy(temp_url,url);

      red_url[0] = '\0';
      if (IS_HTTP_URL(temp_url))
        rc = http_fetch(temp_url,get_hdrs,transf_f,status_f,red_url,
                        sizeof(red_url),host,use_cookies,referer,
                        cookie,&tc);
      else
        rc = ftp_fetch(temp_url,transf_f,status_f,cookie,&tc);

      if (rc != INETERR_REDIRECTED) break;
      if (red_url[0]=='\0') break;

      combine_url(temp_url,red_url,jnk_url,sizeof(jnk_url));
      strcpy(red_url,jnk_url);

      url = red_url;
    }

  tc.last_time = 0;
  tc.stop_bgthread = TRUE;
  while (tc.bgthread_on)
    _sleep(100);
  if (tc.rc != INETERR_NOERROR) rc = tc.rc;

  status_f(cookie,INETST_FINISHED,rc,NULL,0,0);

  DONE_CRITICAL_TIMEOUT();
  skt_done();
  return rc;
}

/*  -> inet_fetch			public */
#if 0
/*  <- ftp_put */

static inet_error_type
ftp_put(char *orig_url,inet_transferprocptr transf,inet_statusprocptr stf,
	void *cookie)
{
  SOCKET	s,sdata;
  int		i,portnum,v[6],bytes_so_far;
  char		*url,*p,*pdoc,*user,*pwd,*host,*pfname,ipadr[32],
  		jnk[MAX_URL_SIZE],temp_url[MAX_URL_SIZE];

  if (strlen(orig_url)>=sizeof(temp_url))		return INETERR_BADURL;
  strcpy(temp_url,orig_url);

  url = temp_url;
  if (!IS_FTP_URL(url))					return INETERR_BADURL;

  url += 6;
  if ( (pdoc=strchr(url,'/')) == NULL )			return INETERR_BADURL;
  *pdoc++ = '\0';
  if (*pdoc=='\0')					return INETERR_BADURL;

  if ( (pfname=strrchr(pdoc,'/')) != NULL )
    *pfname++ = '\0';
  else
    {
      pfname = pdoc;
      pdoc = "";
    }
  if (*pfname=='\0')					return INETERR_BADURL;

  if ( (host=strchr(url,'@')) != NULL )
    {
      *host++ = '\0';
      user = url;
    }
  else
    {
      user = "anonymous";
      host = url;
    }

  if ( (pwd=strchr(user,':')) != NULL )
    *pwd++ = '\0';
  else
    pwd = "guest@";

  if ( (p=strchr(host,':')) == NULL )
    portnum = 21;
  else
    {
      *p++ = '\0';
      portnum = atoi(p);
      if (portnum==0) portnum = 21;
    }

  if (*host=='\0' || *user=='\0')			return INETERR_BADURL;

  if (!stf(cookie,INETST_CONNECTING,0,host,0,0))	return INETERR_CANCELED;
  if (skt_open(&s,host,portnum))			return INETERR_NOCONNECT;
  if (!ftp_response(s,220,0,jnk,sizeof(jnk)))		{ skt_close(&s); return INETERR_BADRESPONSE;	}

  if (!stf(cookie,INETST_AUTHENTICATING,0,NULL,0,0))	{ skt_close(&s); return INETERR_CANCELED;	}
  if (!skt_printf(s,"USER %s\r\n",user))		{ skt_close(&s); return INETERR_CANNOTSEND;	}
  if (!ftp_response(s,331,0,jnk,sizeof(jnk)))		{ skt_close(&s); return INETERR_NOLOGIN;	}

  if (!skt_printf(s,"PASS %s\r\n",pwd))			{ skt_close(&s); return INETERR_CANNOTSEND;	}
  if (!ftp_response(s,230,0,jnk,sizeof(jnk)))		{ skt_close(&s); return INETERR_NOLOGIN;	}

  if (!stf(cookie,INETST_STARTING,0,orig_url,0,0))	{ skt_close(&s); return INETERR_CANCELED;	}
  if (!skt_printf(s,"TYPE I\r\n"))			{ skt_close(&s); return INETERR_CANNOTSEND;	}
  if (!ftp_response(s,200,0,jnk,sizeof(jnk)))		{ skt_close(&s); return INETERR_BADRESPONSE;	}

  if (*pdoc)
    {
      if (!skt_printf(s,"CWD %s\r\n",pdoc))		{ skt_close(&s); return INETERR_CANNOTSEND;	}
      if (!ftp_response(s,250,0,jnk,sizeof(jnk)))	{ skt_close(&s); return INETERR_BADPATH;	}
    }

  if (!skt_printf(s,"PASV\r\n"))			{ skt_close(&s); return INETERR_CANNOTSEND;	}
  if (!ftp_response(s,227,0,jnk,sizeof(jnk)))
    {
      BAD_PASV_RESP:
      skt_close(&s);
      return INETERR_BADRESPONSE;
    }
  p = jnk + 4;
  while (*p && !ISDIGIT(*p)) p++;
  if (*p=='\0') goto BAD_PASV_RESP;

  for (i=0; i<6; i++)
    {
      if (i>0)
        {
          if (*p!=',') goto BAD_PASV_RESP;
          p++;
          while (*p==' ') p++;
        }
      if (!ISDIGIT(*p)) goto BAD_PASV_RESP;
      v[i] = 0;
      while (ISDIGIT(*p))
        {
          v[i] = 10*v[i] + (int)(*p-'0');
          p++;
        }
      if (v[i]<0 || v[i]>255) goto BAD_PASV_RESP;
      while (*p==' ') p++;
    }
  sprintf(ipadr,"%d.%d.%d.%d",v[0],v[1],v[2],v[3]);
  portnum = 256*v[4]+v[5];

  if (!skt_printf(s,"STOR %s\r\n",pfname))		{ skt_close(&s); return INETERR_CANNOTSEND;	}
  if (!stf(cookie,INETST_CONNECTING,0,ipadr,0,0))	{ skt_close(&s); return INETERR_CANCELED;	}
  if (skt_open(&sdata,ipadr,portnum))			{ skt_close(&s); return INETERR_NOCONNECT;	}
  if (!ftp_response(s,125,150,jnk,sizeof(jnk)))
    {
      skt_close(&s);
      skt_close(&sdata);
      return INETERR_BADFILE;
    }
  bytes_so_far	= 0;
  for (;;)
    {
      if ( !stf(cookie,INETST_TRANSFERRING,0,orig_url,bytes_so_far,0) ||
           (i=transf(cookie,(unsigned char *)jnk,sizeof(jnk))) < 0 )
        {
          skt_close(&sdata);
          skt_close(&s);
          return INETERR_CANCELED;
        }
      if (i==0) break;
      if ( !skt_write(sdata,jnk,i) )
        {
          skt_close(&sdata);
          skt_close(&s);
          return INETERR_CANNOTSEND;
        }
      bytes_so_far += i;
    }
  skt_close(&sdata);
  if (!ftp_response(s,226,0,jnk,sizeof(jnk)))		{ skt_close(&s); return INETERR_BADRESPONSE;	}
  skt_close(&s);
  if (bytes_so_far==0) return INETERR_NODATA;
  return INETERR_NOERROR;
}

/*  -> ftp_put */
/*  <- inet_put			public */

inet_error_type
inet_put( char *url, inet_transferprocptr transf_f,
           inet_statusprocptr status_f, void *cookie)
{
  inet_error_type	rc;
  char			*p,
  			temp_url[MAX_URL_SIZE],host[MAX_URL_SIZE];

  if (!status_f)	status_f = dummystatusproc;

  if (!url || !transf_f)
    {
      status_f(cookie,INETST_FINISHED,INETERR_BADPARMS,NULL,0,0);
      return INETERR_BADPARMS;
    }
  if (IS_FTP_URL(url))	p = url+6;
  else
    {
      status_f(cookie,INETST_FINISHED,INETERR_BADURL,NULL,0,0);
      return INETERR_BADURL;
    }
  if (!skt_init())
    {
      status_f(cookie,INETST_FINISHED,INETERR_NOCONNECT,NULL,0,0);
      return INETERR_NOCONNECT;
    }
  INIT_CRITICAL_TIMEOUT();
  strcpy(host,p);
  if ( (p=strchr(host,'/')) != NULL ) *p = '\0';
  if ( (p=strchr(host,'@')) != NULL ) strdel(host,0,1+(int)(p-host));
  if ( (p=strchr(host,':')) != NULL ) *p = '\0';
  strcpy(temp_url,url);
  rc = ftp_put(temp_url,transf_f,status_f,cookie);
  status_f(cookie,INETST_FINISHED,rc,NULL,0,0);
  DONE_CRITICAL_TIMEOUT();
  skt_done();
  return rc;
}

/*  -> inet_put			public */
/*  <- ensure_space */

static int
ensure_space(char **buf,int *allocated,int requested)
{
  int	n;
  char	*result;

  requested += 2;
  if (*allocated >= requested) return 1;
  n = (*allocated==0) ? 512 : *allocated * 2;
  if (n<requested) n = requested + 512;
  result = (char *) realloc(*buf,n);
  if (result==NULL) return 0;
  *buf = result;
  *allocated = n;
  return 1;
}

/*  -> ensure_space */
/*  <- get_file_data */

static int
get_file_data(char *fname,char **contents,int *fsize)
{
  FILE	*f;
  int	allocated,totlen,n;
  char	fbuf[8192];

  if ( (f=fopen(fname,"rb")) == NULL ) return 0;
  *contents = NULL;
  allocated = 0;
  totlen = 0;
  for (;;)
    {
      n = fread(fbuf,1,sizeof(fbuf),f);
      if (n<=0) break;
      if (!ensure_space(contents,&allocated,totlen+n))
        {
          if (*contents) free(*contents);
          fclose(f);
          return 0;
        }
      memcpy( (*contents) + totlen, fbuf, n );
      totlen += n;
    }
  fclose(f);
  if (totlen==0)
    {
      if (*contents) free(*contents);
      return 0;
    }
  *fsize = totlen;
  return 1;
}

/*  -> get_file_data */
/*  <- get_posted_data */

static inet_error_type
get_posted_data(char *post_data_file,char *delimiter,char **pdat,int *plen)
{
  FILE	*f;
  int	len,totlen,n,incsize,allocated;
  char	*q,*pval,*pkey,*mbuf,*incbuf,fbuf[1024];

  static char	*simple_entry =
      "--%s\r\n"
      "Content-Disposition: form-data; name=\"%s\"\r\n"
      "\r\n"
      "%s\r\n";

  static char	*file_entry =
      "--%s\r\n"
      "Content-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\n"
      "Content-Type: application/octet-stream\r\n"
      "\r\n";

  mbuf = NULL;
  allocated = 0;
  if ( (f=fopen(post_data_file,"r")) == NULL ) return INETERR_NOPOSTFILE;
  totlen = 0;
  memset(fbuf,0,sizeof(fbuf));
  while (fgets(fbuf,sizeof(fbuf)-1,f))
    {
      if (fbuf[0]==';' || fbuf[0]=='#') continue;
      if ( (q=strchr(fbuf,'\n')) != NULL ) *q = '\0';
      if ( (pval=strchr(fbuf,'=')) == NULL )
        {
          BADFILE:
          if (mbuf) free(mbuf);
          fclose(f);
          return INETERR_BADPOSTFILE;
        }
      *pval++ = '\0';

      if (fbuf[0]=='@')		/* Included file */
        {
          pkey = fbuf + 1;
          if (*pkey=='\0' || *pval=='\0') goto BADFILE;

          if (!get_file_data(pval,&incbuf,&incsize)) goto BADFILE;

          /* The -6 is for the 3 %s in file_entry */
          len = strlen(file_entry)-6 +
                strlen(delimiter)+ strlen(pkey) + strlen(pval) +
                incsize + 2;
          if ( !ensure_space(&mbuf,&allocated,totlen+len) ) goto BADFILE;
          n = sprintf(mbuf+totlen,file_entry,delimiter,pkey,pval);
          memcpy(mbuf+totlen+n,incbuf,incsize);
          strcpy(mbuf+totlen+n+incsize,"\r\n");
          totlen += len;
          free(incbuf);
        }
      else
        {
          pkey = fbuf;
          if (*pkey=='\0') goto BADFILE;

          /* The -6 is for the 3 %s in simple_entry */
          len = strlen(simple_entry)-6 +
                strlen(delimiter) + strlen(pkey) + strlen(pval);
          if ( !ensure_space(&mbuf,&allocated,totlen+len) ) goto BADFILE;
          sprintf(mbuf+totlen,simple_entry,delimiter,pkey,pval);
          totlen += len;
        }
    }
  if (totlen==0) goto BADFILE;
  len = 2 + strlen(delimiter) + 2 + 2;
  if (!ensure_space(&mbuf,&allocated,totlen+len)) goto BADFILE;
  sprintf(mbuf+totlen,"--%s--\r\n",delimiter);
  totlen += len;
  fclose(f);

  *pdat = mbuf;
  *plen = totlen;
  return INETERR_NOERROR;
}

/*  -> get_posted_data */
/*  <- http_post */

static inet_error_type
http_post(char *orig_url, char *post_data_file,inet_transferprocptr transf,
           inet_statusprocptr stf, char	*def_host,int use_cookies,
           char *referer,void *cookie,transfer_context *tc)
{
  inet_error_type	rc;
  int			n,portnum,state,bytes_so_far,total_bytes,
  			first_time,v,posted_length;
  char			*url,*p,*pdoc,*q,*user_pwd,*posted_data,
  			jnk[4*MAX_URL_SIZE+1],temp_url[MAX_URL_SIZE+1],
  			temp_pwd[128];
  static char		*delimiter = "---------------------------7d2f05368";

  if (strlen(orig_url)>=sizeof(temp_url))		return INETERR_BADURL;
  strcpy(temp_url,orig_url);

  url = temp_url;
  if (!IS_HTTP_URL(url))				return INETERR_BADURL;

  url += 7;
  if ( (pdoc=strchr(url,'/')) == NULL )
    pdoc = "";
  else
    *pdoc++ = '\0';

  user_pwd = NULL;
  if ( (p=strchr(url,'@')) != NULL )
    {
      *p++ = '\0';
      user_pwd = url;
      url = p;
      if (!strchr(user_pwd,':')  || *user_pwd==':')  user_pwd = NULL;
    }

  if ( (p=strchr(url,':')) == NULL )
    portnum = 80;
  else
    {
      *p++ = '\0';
      portnum = atoi(p);
      if (portnum==0) portnum = 80;
    }

  if (*url=='\0')					return INETERR_BADURL;

  rc = get_posted_data(post_data_file,delimiter,&posted_data,&posted_length);
  if (rc != INETERR_NOERROR)				return rc;


  if (!stf(cookie,INETST_CONNECTING,0,url,0,0))
    {
      free(posted_data);
      return INETERR_CANCELED;
    }
  if (skt_open(&(tc->skt),url,portnum))
    {
      free(posted_data);
      return INETERR_NOCONNECT;
    }

  sprintf(jnk,"POST %s%s HTTP/1.0\r\n"
              "Accept: */*\r\n"
              "User-Agent: Mozilla/2.0 (compatible; MSIE 3.0; Windows 95)\r\n"
              "Content-Type: multipart/form-data; boundary=%s\r\n"
              "Content-Length: %d\r\n"
              "Host: %s\r\n",
              (is_url(pdoc)) ? "" : "/", pdoc,
              delimiter,
              posted_length,
              (def_host[0]) ? def_host : url);
  if (referer && *referer)
    sprintf(jnk+strlen(jnk),"Referer: %s\r\n",referer);
  if (user_pwd)
    {
      n = b64_encode(user_pwd,strlen(user_pwd),temp_pwd,sizeof(temp_pwd)-1);
      if (n>0)
        {
          temp_pwd[n] = '\0';
          sprintf(jnk+strlen(jnk),"Authorization: Basic %s\r\n",temp_pwd);
        }
    }
  if (use_cookies)
    {
      DWORD	max_cookie_size;
      unsigned	l1,l2;

      l1 = strlen(jnk);
      strcpy(jnk+l1,"Cookie: ");
      l2 = strlen(jnk);
      max_cookie_size = sizeof(jnk) - l2 - 6;
      if (!InternetGetCookie(orig_url,"jnk",jnk+l2,&max_cookie_size))
        jnk[l1] = '\0';
      else strcat(jnk,"\r\n");
    }
  strcat(jnk,
           "Pragma: no-cache\r\n"
           "Connection: Close\r\n"
           "\r\n");

  if (*pdoc=='\0') pdoc = "/";
  if (!stf(cookie,INETST_STARTING,0,pdoc,0,0))
    {
      skt_close(&(tc->skt));
      free(posted_data);
      return INETERR_CANCELED;
    }

  if ( !skt_write(tc->skt,jnk,strlen(jnk)) ||
       !skt_write(tc->skt,posted_data,posted_length) )
    {
      skt_close(&(tc->skt));
      free(posted_data);
      return INETERR_CANNOTSEND;
    }

  rc = INETERR_NODATA;
  state = 0;
  total_bytes = 0;
  bytes_so_far = 0;
  first_time = 1;
  for (;;)
    {

      memset(jnk,0,sizeof(jnk));
      if ( (n=skt_read(tc,jnk,sizeof(jnk)-1)) <= 0 ) break;

      if (first_time && !memcmp(jnk,"HTTP/",5))
        {
          for (p=jnk+5;  ISDIGIT(*p) || *p=='.';  p++)
            ;
          while (*p==' ') p++;
          if (ISDIGIT(*p))
            {
              v = 0;
              while (ISDIGIT(*p))
                {
                  v = 10*v + (int)(*p - '0');
                  p++;
                }
              if (v!=200)
                {
                  rc = INETERR_BADFILE;
                  break;
                }
            }
        }
      first_time = 0;

      p = jnk;
      if (state==0)
        {
          if ( (p=strstr(p,"\r\n\r\n")) == NULL ) continue;
          *p = '\0';
          if ( (q=strstr(jnk,"\nContent-Length:")) == NULL )
            q = strstr(jnk,"\nContent-length:");
          if (q)
            {
              q += 16;
              while (*q==' ') q++;
              while (ISDIGIT(*q))
                {
                  total_bytes = 10*total_bytes + (int)(*q-'0');
                  q++;
                }
            }
          p += 4;
          n -= p-jnk;
          state++;
        }
      if (state==1)
        {
          if (n>0)
            {
              bytes_so_far += n;
              rc = INETERR_NOERROR;
              if (transf(cookie,(unsigned char *)p,n) <= 0)
                {
                  rc = INETERR_CANCELED;
                  break;
                }
            }
        }

      if (!stf(cookie,INETST_TRANSFERRING,0,orig_url,bytes_so_far,total_bytes))
        {
          rc = INETERR_CANCELED;
          break;
        }

    }

  skt_close(&(tc->skt));
  free(posted_data);
  if (rc==INETERR_NOERROR && total_bytes>0 && bytes_so_far<total_bytes)
    rc = INETERR_SHORTFILE;
  return rc;
}

/*  -> http_post */
/*  <- inet_post			public */

inet_error_type
inet_post(char *url, char *post_data_file,
          inet_transferprocptr transf_f,
          inet_statusprocptr status_f,
          int use_cookies, char *referer,
          void *cookie, unsigned timeout_seconds )
{
  inet_error_type	rc;
  transfer_context	tc;
  char			*p,
  			temp_url[MAX_URL_SIZE],host[MAX_URL_SIZE];

  if (!status_f)	status_f = dummystatusproc;
  if (!url || !post_data_file || !transf_f)
    {
      status_f(cookie,INETST_FINISHED,INETERR_BADPARMS,NULL,0,0);
      return INETERR_BADPARMS;
    }
  if (IS_HTTP_URL(url))	p = url+7;
  else
    {
      status_f(cookie,INETST_FINISHED,INETERR_BADURL,NULL,0,0);
      return INETERR_BADURL;
    }

  if (!skt_init())
    {
      status_f(cookie,INETST_FINISHED,INETERR_NOCONNECT,NULL,0,0);
      return INETERR_NOCONNECT;
    }
  INIT_CRITICAL_TIMEOUT();
  tc.stop_bgthread	= FALSE;
  tc.bgthread_on	= FALSE;
  tc.last_time		= 0;
  tc.rc			= INETERR_NOERROR;
  tc.skt		= SOCKET_ERROR;
  tc.nsecs		= timeout_seconds;

  if (timeout_seconds>0)
    if (!START_THREAD(timeout_watch,&tc))
      tc.nsecs = 0;
    else
      {
        /* Make sure that the background thread is on */
        while (!tc.bgthread_on)
          _sleep(100);
      }

  strcpy(host,p);
  if ( (p=strchr(host,'/')) != NULL ) *p = '\0';
  if ( (p=strchr(host,'@')) != NULL ) strdel(host,0,1+(int)(p-host));
  if ( (p=strchr(host,':')) != NULL ) *p = '\0';

  strcpy(temp_url,url);

  rc = http_post(temp_url,post_data_file,transf_f,status_f,
                 host,use_cookies,referer,cookie,&tc);

  tc.last_time = 0;
  tc.stop_bgthread = TRUE;
  while (tc.bgthread_on)
    _sleep(100);
  if (tc.rc != INETERR_NOERROR) rc = tc.rc;

  status_f(cookie,INETST_FINISHED,rc,NULL,0,0);

  DONE_CRITICAL_TIMEOUT();
  skt_done();
  return rc;
}

/*  -> inet_post			public */
#endif
/*  <- get_reg_keyval */

#ifdef _WIN32
static int
get_reg_keyval(HKEY hk0,char *branch,char *key,char *buf,int bufsize)
{
  HKEY	hk;
  LONG	res;
  DWORD	sz,typ;

  memset(buf,0,bufsize);
  if (RegOpenKey(hk0,branch,&hk)!=ERROR_SUCCESS) return -1;

  sz = bufsize-1;
  res = RegQueryValueEx(hk,key,NULL,&typ,(unsigned char *)buf,&sz);
  RegCloseKey(hk);

  return (buf[0]!='\0');
}
#endif

/*  -> get_reg_keyval */
/*  <- inet_init			public */

void
inet_init(void)
{
#ifdef _WIN32
  WSADATA	wsad;

  char *p,*q,temp[MAX_PATH+1];
  static char *secname = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\"
                         "Internet Settings";

  q = temp;
  get_reg_keyval(HKEY_CURRENT_USER,secname,"ProxyEnable",temp,sizeof(temp)-1);
  proxy_enabled = (temp[0]!='\0');
  get_reg_keyval(HKEY_CURRENT_USER,secname,"ProxyServer",temp,sizeof(temp));
  if ( (p=strstr(temp,"http=")) != NULL )
    {
      p += 5;
      if ( (q=strchr(p,';')) != NULL ) *q = '\0';
      q = p;
    }
  if ( (p=strchr(q,':')) != NULL )
    {
      *p++ = '\0';
      strcpy(proxy_name,q);
      proxy_port = atoi(p);
      if (proxy_port<=0 || proxy_port>=65536) proxy_port = 8080;
    }
  else
    {
      proxy_name[0] = '\0';
      proxy_port = 8080;
    }
  if (proxy_name[0]=='\0') proxy_enabled = 0;

  WSAStartup(MAKEWORD(1,1),&wsad);
#endif
}

/*  -> inet_init			public */
/*  <- inet_done			public */

void
inet_done(void)
{
#ifdef _WIN32
  WSACleanup();
#endif
}

/*  -> inet_done			public */
