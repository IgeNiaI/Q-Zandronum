#ifndef INETH__
#define INETH__

#ifdef _WIN32
#  include <windows.h>
#else
#  define WINAPI
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  INETST_CONNECTING,
  INETST_AUTHENTICATING,
  INETST_STARTING,
  INETST_TRANSFERRING,
  INETST_REDIRECTING,
  INETST_FINISHED
  } inet_status_type;

typedef enum {
  INETERR_NOERROR,
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
  INETERR_NOPOSTFILE,
  INETERR_BADPOSTFILE,
  INETERR_TIMEOUT
  } inet_error_type;


typedef int (WINAPI *inet_transferprocptr)(void *,unsigned char *,int);
typedef int (WINAPI *inet_statusprocptr)(void *,inet_status_type,
                                         int,char *,
                                         unsigned,unsigned);

inet_error_type	inet_fetch( char *url, inet_transferprocptr transf_f,
                            inet_statusprocptr status_f,
                            int get_hdrs, int use_cookies,
                            char *referer, void *cookie,
                            unsigned timeout_seconds,
							char *real_url);

inet_error_type	inet_put(char *url, inet_transferprocptr transf_f,
			inet_statusprocptr status_f, void *cookie );

inet_error_type inet_post(char *url, char *post_data_file,
                            inet_transferprocptr transf_f,
                            inet_statusprocptr status_f,
                            int use_cookies,
                            char *referer, void *cookie,
                            unsigned timeout_seconds );

void		inet_init(void);
void		inet_done(void);
#ifdef __cplusplus
}
#endif

#endif
