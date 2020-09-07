/*
 * For dialogs
 */
#if _WIN32
#define IDD_STRED1		100
#define	IDD_LABEL1		120
#define	IDD_HELP		999
#define	IDD_HSPARM1		998
#define	IDD_HSPARM2		997
#define	IDD_HSPARM3		996

typedef void (WINAPI *helpproc)(char *);

typedef struct
  {
    LPSTR	label;
    LPSTR	buf;
    int		size,enabled;
  } HSFIELD, *LPHSFIELD;

typedef BOOL FAR WINAPI valfun_t(void *);
typedef valfun_t *valfun_ptr;

typedef struct
  {
    int		version;
    HWND	hwnd;
    LPSTR	caption;
    helpproc   	helpfun;
    LPSTR	helpkey;
    LPSTR	oklabel,canlabel,helplabel;
    valfun_ptr	valfun;
    int		nfields;
    HSFIELD	fld[1];
  } HSRECORD, *LPHSRECORD;

int			edit_record(HINSTANCE hinst,char *dlgname,LPHSRECORD r);
void			center_window(HWND h);
#endif

#define	IsSpace(c)	((c)==' ' || (c)=='\t')
char			*get_arg(char *cmdline,char **arg);
char			*skip_spaces(char *p);
char			*fmt_size(int v);
int			rtrim(char *s);
int			ltrim(char *s);
