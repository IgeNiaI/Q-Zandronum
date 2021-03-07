#include "gl/system/gl_system.h"

#include "win32iface.h"
#include "win32gliface.h"
//#include "gl/gl_intern.h"
#include "templates.h"
#include "version.h"
#include "c_console.h"
#include "hardware.h"
#include "v_video.h"
#include "i_input.h"
#include "i_system.h"
#include "doomstat.h"
#include "v_text.h"
//#include "gl_defs.h"

#include "gl/renderer/gl_renderer.h"
#include "gl/system/gl_framebuffer.h"
#include "gl/shaders/gl_shader.h"
#include "gl/utility/gl_templates.h"

void gl_CalculateCPUSpeed();
extern int NewWidth, NewHeight, NewBits, DisplayBits;

CUSTOM_CVAR(Int, gl_vid_multisample, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL )
{
	Printf("This won't take effect until " GAMENAME " is restarted.\n");
}

CVAR(Bool, gl_debug, false, 0)

// [BB]
CVAR(Bool, gl_quadbufferedstereo, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

EXTERN_CVAR(Int, vid_refreshrate)

//==========================================================================
//
// 
//
//==========================================================================

Win32GLVideo::Win32GLVideo(int parm) : m_Modes(NULL), m_IsFullscreen(false)
{
	#ifdef _WIN32
		 if (CPU.bRDTSC) gl_CalculateCPUSpeed();
	#endif
	I_SetWndProc();
	m_DisplayWidth = vid_defwidth;
	m_DisplayHeight = vid_defheight;
	m_DisplayBits = 32;
	m_DisplayHz = 60;

	GetDisplayDeviceName();
	MakeModesList();
	SetPixelFormat();

}

//==========================================================================
//
// 
//
//==========================================================================

Win32GLVideo::~Win32GLVideo()
{
	FreeModes();
	if (GLRenderer != NULL) GLRenderer->FlushTextures();
}

//==========================================================================
//
// 
//
//==========================================================================

void Win32GLVideo::SetWindowedScale(float scale)
{
}

//==========================================================================
//
// 
//
//==========================================================================

struct MonitorEnumState
{
	int curIdx;
	HMONITOR hFoundMonitor;
};

static BOOL CALLBACK GetDisplayDeviceNameMonitorEnumProc(HMONITOR hMonitor, HDC, LPRECT, LPARAM dwData)
{
	MonitorEnumState *state = reinterpret_cast<MonitorEnumState *>(dwData);

	MONITORINFOEX mi;
	mi.cbSize = sizeof mi;
	GetMonitorInfo(hMonitor, &mi);

	// This assumes the monitors are returned by EnumDisplayMonitors in the
	// order they're found in the Direct3D9 adapters list. Fingers crossed...
	if (state->curIdx == vid_adapter)
	{
		state->hFoundMonitor = hMonitor;

		// Don't stop enumeration; this makes EnumDisplayMonitors fail. I like
        // proper fails.
	}

	++state->curIdx;

	return TRUE;
}

//==========================================================================
//
// 
//
//==========================================================================

void Win32GLVideo::GetDisplayDeviceName()
{
	// If anything goes wrong, anything at all, everything uses the primary
    // monitor.
	m_DisplayDeviceName = 0;
	m_hMonitor = 0;

	MonitorEnumState mes;

	mes.curIdx = 1;
	mes.hFoundMonitor = 0;

	// Could also use EnumDisplayDevices, I guess. That might work.
	if (EnumDisplayMonitors(0, 0, &GetDisplayDeviceNameMonitorEnumProc, LPARAM(&mes)))
	{
		if (mes.hFoundMonitor)
		{
			MONITORINFOEX mi;

			mi.cbSize = sizeof mi;

			if (GetMonitorInfo(mes.hFoundMonitor, &mi))
			{
				strcpy(m_DisplayDeviceBuffer, mi.szDevice);
				m_DisplayDeviceName = m_DisplayDeviceBuffer;

				m_hMonitor = mes.hFoundMonitor;
			}
		}
	}
}

//==========================================================================
//
// 
//
//==========================================================================

void Win32GLVideo::MakeModesList()
{
	ModeInfo *pMode, *nextmode;
	DEVMODE dm;
	int mode = 0;

	memset(&dm, 0, sizeof(DEVMODE));
	dm.dmSize = sizeof(DEVMODE);

	while (EnumDisplaySettings(m_DisplayDeviceName, mode, &dm))
	{
		this->AddMode(dm.dmPelsWidth, dm.dmPelsHeight, dm.dmBitsPerPel, dm.dmPelsHeight, dm.dmDisplayFrequency);
		++mode;
	}

	for (pMode = m_Modes; pMode != NULL; pMode = nextmode)
	{
		nextmode = pMode->next;
		if (pMode->realheight == pMode->height && pMode->height * 4/3 == pMode->width)
		{
			if (pMode->width >= 360)
			{
				AddMode (pMode->width, pMode->width * 9/16, pMode->bits, pMode->height, pMode->refreshHz);
			}
			if (pMode->width > 640)
			{
				AddMode (pMode->width, pMode->width * 10/16, pMode->bits, pMode->height, pMode->refreshHz);
			}
		}
	}
}

//==========================================================================
//
// 
//
//==========================================================================

void Win32GLVideo::StartModeIterator(int bits, bool fs)
{
	m_IteratorMode = m_Modes;
	// I think it's better to ignore the game-side settings of bit depth.
	// The GL renderer will always default to 32 bits because 16 bit modes cannot have a stencil buffer.
	m_IteratorBits = 32;	
	m_IteratorFS = fs;
}

//==========================================================================
//
// 
//
//==========================================================================

bool Win32GLVideo::NextMode(int *width, int *height, bool *letterbox)
{
	if (m_IteratorMode)
	{
		while (m_IteratorMode && m_IteratorMode->bits != m_IteratorBits)
		{
			m_IteratorMode = m_IteratorMode->next;
		}

		if (m_IteratorMode)
		{
			*width = m_IteratorMode->width;
			*height = m_IteratorMode->height;
			if (letterbox != NULL) *letterbox = m_IteratorMode->realheight != m_IteratorMode->height;
			m_IteratorMode = m_IteratorMode->next;
			return true;
		}
	}

	return false;
}

//==========================================================================
//
// 
//
//==========================================================================

void Win32GLVideo::AddMode(int x, int y, int bits, int baseHeight, int refreshHz)
{
	ModeInfo **probep = &m_Modes;
	ModeInfo *probe = m_Modes;

	// This mode may have been already added to the list because it is
	// enumerated multiple times at different refresh rates. If it's
	// not present, add it to the right spot in the list; otherwise, do nothing.
	// Modes are sorted first by width, then by height, then by depth. In each
	// case the order is ascending.
	for (; probe != 0; probep = &probe->next, probe = probe->next)
	{
		if (probe->width != x)		continue;
		// Width is equal
		if (probe->height != y)		continue;
		// Width is equal
		if (probe->realheight != baseHeight)	continue;
		// Height is equal
		if (probe->bits != bits)	continue;
		// Bits is equal
		if (probe->refreshHz > refreshHz) continue;
		probe->refreshHz = refreshHz;
		return;
	}

	*probep = new ModeInfo (x, y, bits, baseHeight, refreshHz);
	(*probep)->next = probe;
}

//==========================================================================
//
// 
//
//==========================================================================

void Win32GLVideo::FreeModes()
{
	ModeInfo *mode = m_Modes;

	while (mode)
	{
		ModeInfo *tempmode = mode;
		mode = mode->next;
		delete tempmode;
	}

	m_Modes = NULL;
}

//==========================================================================
//
// 
//
//==========================================================================

bool Win32GLVideo::GoFullscreen(bool yes)
{
	m_IsFullscreen = yes;

	m_trueHeight = m_DisplayHeight;

	if (yes)
	{
		// If in windowed mode, any height is good. 
		for (ModeInfo *mode = m_Modes; mode != NULL; mode = mode->next)
		{
			if (mode->width == m_DisplayWidth && mode->height == m_DisplayHeight)
			{
				m_trueHeight = mode->realheight;
				break;
			}
		}
	}

	if (yes)
	{
		SetFullscreen(m_DisplayDeviceName, m_DisplayWidth, m_trueHeight, m_DisplayBits, m_DisplayHz);
	}
	else
	{
		SetFullscreen(m_DisplayDeviceName, 0,0,0,0);
	}
	return yes;
}


//==========================================================================
//
// 
//
//==========================================================================

DFrameBuffer *Win32GLVideo::CreateFrameBuffer(int width, int height, bool fs, DFrameBuffer *old)
{
	Win32GLFrameBuffer *fb;

	m_DisplayWidth = width;
	m_DisplayHeight = height;
	m_DisplayBits = 32;
	m_DisplayHz = 60;

	if (vid_refreshrate == 0)
	{
		for (ModeInfo *mode = m_Modes; mode != NULL; mode = mode->next)
		{
			if (mode->width == m_DisplayWidth && mode->height == m_DisplayHeight && mode->bits == m_DisplayBits)
			{
				m_DisplayHz = MAX<int>(m_DisplayHz, mode->refreshHz);
			}
		}
	}
	else
	{
		m_DisplayHz = vid_refreshrate;
	}

	if (old != NULL)
	{ // Reuse the old framebuffer if its attributes are the same
		fb = static_cast<Win32GLFrameBuffer *> (old);
		if (fb->m_Width == m_DisplayWidth &&
			fb->m_Height == m_DisplayHeight &&
			fb->m_Bits == m_DisplayBits &&
			fb->m_RefreshHz == m_DisplayHz &&
			fb->m_Fullscreen == fs)
		{
			return old;
		}
		//old->GetFlash(flashColor, flashAmount);
		delete old;
	}

	fb = new OpenGLFrameBuffer(m_hMonitor, m_DisplayWidth, m_DisplayHeight, m_DisplayBits, m_DisplayHz, fs);

	return fb;
}

//==========================================================================
//
// 
//
//==========================================================================

bool Win32GLVideo::SetResolution (int width, int height, int bits)
{
	if (GLRenderer != NULL) GLRenderer->FlushTextures();
	I_ShutdownGraphics();
	
	Video = new Win32GLVideo(0);
	if (Video == NULL) I_FatalError ("Failed to initialize display");
	
	bits=32;
	
	V_DoModeSetup(width, height, bits);
	return true;	// We must return true because the old video context no longer exists.
}

//==========================================================================
//
// 
//
//==========================================================================

struct DumpAdaptersState
{
	unsigned index;
	char *displayDeviceName;
};

static BOOL CALLBACK DumpAdaptersMonitorEnumProc(HMONITOR hMonitor, HDC, LPRECT, LPARAM dwData)
{
	DumpAdaptersState *state = reinterpret_cast<DumpAdaptersState *>(dwData);

	MONITORINFOEX mi;
	mi.cbSize=sizeof mi;

	char moreinfo[64] = "";

	bool active = true;

	if (GetMonitorInfo(hMonitor, &mi))
	{
		bool primary = !!(mi.dwFlags & MONITORINFOF_PRIMARY);

		mysnprintf(moreinfo, countof(moreinfo), " [%ldx%ld @ (%ld,%ld)]%s",
			mi.rcMonitor.right - mi.rcMonitor.left,
			mi.rcMonitor.bottom - mi.rcMonitor.top,
			mi.rcMonitor.left, mi.rcMonitor.top,
			primary ? " (Primary)" : "");

		if (!state->displayDeviceName && !primary)
			active = false;//primary selected, but this ain't primary
		else if (state->displayDeviceName && strcmp(state->displayDeviceName, mi.szDevice) != 0)
			active = false;//this isn't the selected one
	}

	Printf("%s%u. %s\n",
		active ? TEXTCOLOR_BOLD : "",
		state->index,
		moreinfo);

	++state->index;

	return TRUE;
}

//==========================================================================
//
// 
//
//==========================================================================

void Win32GLVideo::DumpAdapters()
{
	DumpAdaptersState das;

	das.index = 1;
	das.displayDeviceName = m_DisplayDeviceName;

	EnumDisplayMonitors(0, 0, DumpAdaptersMonitorEnumProc, LPARAM(&das));
}

//==========================================================================
//
// 
//
//==========================================================================

HWND Win32GLVideo::InitDummy()
{
	HMODULE g_hInst = GetModuleHandle(NULL);
	HWND dummy;
	//Create a rect structure for the size/position of the window
	RECT windowRect;
	windowRect.left = 0;
	windowRect.right = 64;
	windowRect.top = 0;
	windowRect.bottom = 64;

	//Window class structure
	WNDCLASS wc;

	//Fill in window class struct
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = (WNDPROC) DefWindowProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = g_hInst;
	wc.hIcon = LoadIcon(NULL, IDI_WINLOGO);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = "GZDoomOpenGLDummyWindow";

	//Register window class
	if(!RegisterClass(&wc))
	{
		return 0;
	}

	//Set window style & extended style
	DWORD style, exStyle;
	exStyle = WS_EX_CLIENTEDGE;
	style = WS_SYSMENU | WS_BORDER | WS_CAPTION;// | WS_VISIBLE;

	//Adjust the window size so that client area is the size requested
	AdjustWindowRectEx(&windowRect, style, false, exStyle);

	//Create Window
	if(!(dummy = CreateWindowEx(exStyle,
		"GZDoomOpenGLDummyWindow",
		"GZDOOM",
		WS_CLIPSIBLINGS | WS_CLIPCHILDREN | style,
		0, 0,
		windowRect.right-windowRect.left,
		windowRect.bottom-windowRect.top,
		NULL, NULL,
		g_hInst,
		NULL)))
	{
		UnregisterClass("GZDoomOpenGLDummyWindow", g_hInst);
		return 0;
	}
	ShowWindow(dummy, SW_HIDE);

	return dummy;
}

//==========================================================================
//
// 
//
//==========================================================================

void Win32GLVideo::ShutdownDummy(HWND dummy)
{
	DestroyWindow(dummy);
	UnregisterClass("GZDoomOpenGLDummyWindow", GetModuleHandle(NULL));
}


//==========================================================================
//
// 
//
//==========================================================================

bool Win32GLVideo::SetPixelFormat()
{
	HDC hDC;
	HGLRC hRC;
	HWND dummy;

	PIXELFORMATDESCRIPTOR pfd = {
		sizeof(PIXELFORMATDESCRIPTOR),
			1,
			PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
			PFD_TYPE_RGBA,
			32, // color depth
			0, 0, 0, 0, 0, 0,
			0,
			0,
			0,
			0, 0, 0, 0,
			16, // z depth
			0, // stencil buffer
			0,
			PFD_MAIN_PLANE,
			0,
			0, 0, 0
	};

	int pixelFormat;

	// we have to create a dummy window to init stuff from or the full init stuff fails
	dummy = InitDummy();

	hDC = GetDC(dummy);
	pixelFormat = ChoosePixelFormat(hDC, &pfd);
	DescribePixelFormat(hDC, pixelFormat, sizeof(pfd), &pfd);

	::SetPixelFormat(hDC, pixelFormat, &pfd);

	hRC = wglCreateContext(hDC);
	wglMakeCurrent(hDC, hRC);

	wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");
	wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
	// any extra stuff here?

	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(hRC);
	ReleaseDC(dummy, hDC);
	ShutdownDummy(dummy);

	return true;
}

//==========================================================================
//
// 
//
//==========================================================================

bool Win32GLVideo::SetupPixelFormat(bool allowsoftware, int multisample)
{
	int colorDepth;
	HDC deskDC;
	int attributes[28]; // [BB] Added two attributes.
	int pixelFormat;
	unsigned int numFormats;
	float attribsFloat[] = {0.0f, 0.0f};
	
	deskDC = GetDC(GetDesktopWindow());
	colorDepth = GetDeviceCaps(deskDC, BITSPIXEL);
	ReleaseDC(GetDesktopWindow(), deskDC);

	if (wglChoosePixelFormatARB)
	{
		attributes[0]	=	WGL_RED_BITS_ARB; //bits
		attributes[1]	=	8;
		attributes[2]	=	WGL_GREEN_BITS_ARB; //bits
		attributes[3]	=	8;
		attributes[4]	=	WGL_BLUE_BITS_ARB; //bits
		attributes[5]	=	8;
		attributes[6]	=	WGL_ALPHA_BITS_ARB;
		attributes[7]	=	8;
		attributes[8]	=	WGL_DEPTH_BITS_ARB;
		attributes[9]	=	24;
		attributes[10]	=	WGL_STENCIL_BITS_ARB;
		attributes[11]	=	8;
	
		attributes[12]	=	WGL_DRAW_TO_WINDOW_ARB;	//required to be true
		attributes[13]	=	true;
		attributes[14]	=	WGL_SUPPORT_OPENGL_ARB;
		attributes[15]	=	true;
		attributes[16]	=	WGL_DOUBLE_BUFFER_ARB;
		attributes[17]	=	true;
	
		attributes[18]	=	WGL_ACCELERATION_ARB;	//required to be FULL_ACCELERATION_ARB
		if (allowsoftware)
		{
			attributes[19]	=	WGL_NO_ACCELERATION_ARB;
		}
		else
		{
			attributes[19]	=	WGL_FULL_ACCELERATION_ARB;
		}
	
		if (multisample > 0)
		{
			attributes[20]	=	WGL_SAMPLE_BUFFERS_ARB;
			attributes[21]	=	true;
			attributes[22]	=	WGL_SAMPLES_ARB;
			attributes[23]	=	multisample;
		}
		else
		{
			attributes[20]	=	0;
			attributes[21]	=	0;
			attributes[22]	=	0;
			attributes[23]	=	0;
		}
	
		// [BB] Starting with driver version 314.07, NVIDIA GeForce cards support OpenGL quad buffered
		// stereo rendering with 3D Vision hardware. Select the corresponding attribute here.
		const int offset = ( multisample > 0 ) ? 24 : 20;
		attributes[offset]	=	gl_quadbufferedstereo ? WGL_STEREO_ARB : 0;
		attributes[offset+1]	=	true;
		attributes[offset+2]	=	0;
		attributes[offset+3]	=	0;
	
		if (!wglChoosePixelFormatARB(m_hDC, attributes, attribsFloat, 1, &pixelFormat, &numFormats))
		{
			Printf("R_OPENGL: Couldn't choose pixel format. Retrying in compatibility mode\n");
			goto oldmethod;
		}
	
		if (numFormats == 0)
		{
			Printf("R_OPENGL: No valid pixel formats found. Retrying in compatibility mode\n");
			goto oldmethod;
		}
	}
	else
	{
	oldmethod:
		// If wglChoosePixelFormatARB is not found we have to do it the old fashioned way.
		static PIXELFORMATDESCRIPTOR pfd = {
			sizeof(PIXELFORMATDESCRIPTOR),
				1,
				PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
				PFD_TYPE_RGBA,
				32, // color depth
				0, 0, 0, 0, 0, 0,
				0,
				0,
				0,
				0, 0, 0, 0,
				32, // z depth
				8, // stencil buffer
				0,
				PFD_MAIN_PLANE,
				0,
				0, 0, 0
		};

		pixelFormat = ChoosePixelFormat(m_hDC, &pfd);
		DescribePixelFormat(m_hDC, pixelFormat, sizeof(pfd), &pfd);

		if (pfd.dwFlags & PFD_GENERIC_FORMAT)
		{
			if (!allowsoftware)
			{
				Printf("R_OPENGL: OpenGL driver not accelerated!  Falling back to software renderer.\n");
				return false;
			}
		}
	}

	if (!::SetPixelFormat(m_hDC, pixelFormat, NULL))
	{
		Printf("R_OPENGL: Couldn't set pixel format.\n");
		return false;
	}
	return true;
}

//==========================================================================
//
// 
//
//==========================================================================

bool Win32GLVideo::InitHardware (HWND Window, bool allowsoftware, int multisample)
{
	m_Window=Window;
	m_hDC = GetDC(Window);

	if (!SetupPixelFormat(allowsoftware, multisample))
	{
		Printf ("R_OPENGL: Reverting to software mode...\n");
		return false;
	}

	m_hRC = 0;
	if (wglCreateContextAttribsARB != NULL)
	{
		int ctxAttribs[] = {
			WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
			WGL_CONTEXT_MINOR_VERSION_ARB, 3,
			WGL_CONTEXT_FLAGS_ARB, gl_debug? WGL_CONTEXT_DEBUG_BIT_ARB : 0,
			WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
			0
		};

		m_hRC = wglCreateContextAttribsARB(m_hDC, 0, ctxAttribs);
	}
	if (m_hRC == 0)
	{
		m_hRC = wglCreateContext(m_hDC);
	}

	if (m_hRC == NULL)
	{
		Printf ("R_OPENGL: Couldn't create render context. Reverting to software mode...\n");
		return false;
	}

	wglMakeCurrent(m_hDC, m_hRC);
	return true;
}

//==========================================================================
//
// 
//
//==========================================================================

void Win32GLVideo::Shutdown()
{
	if (m_hRC)
	{
		wglMakeCurrent(0, 0);
		wglDeleteContext(m_hRC);
	}
	if (m_hDC) ReleaseDC(m_Window, m_hDC);
}

//==========================================================================
//
// 
//
//==========================================================================

bool Win32GLVideo::SetFullscreen(const char *devicename, int w, int h, int bits, int hz)
{
	DEVMODE dm;

	if (w==0)
	{
		ChangeDisplaySettingsEx(devicename, 0, 0, 0, 0);
	}
	else
	{
		dm.dmSize = sizeof(DEVMODE);
		dm.dmSpecVersion = DM_SPECVERSION;//Somebody owes me...
		dm.dmDriverExtra = 0;//...1 hour of my life back
		dm.dmPelsWidth = w;
		dm.dmPelsHeight = h;
		dm.dmBitsPerPel = bits;
		dm.dmDisplayFrequency = hz;
		dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL | DM_DISPLAYFREQUENCY;
		if (DISP_CHANGE_SUCCESSFUL != ChangeDisplaySettingsEx(devicename, &dm, 0, CDS_FULLSCREEN, 0))
		{
			dm.dmFields &= ~DM_DISPLAYFREQUENCY;
			return DISP_CHANGE_SUCCESSFUL == ChangeDisplaySettingsEx(devicename, &dm, 0, CDS_FULLSCREEN, 0);
		}
	}
	return true;
}

//==========================================================================
//
// 
//
//==========================================================================

IMPLEMENT_ABSTRACT_CLASS(Win32GLFrameBuffer)

//==========================================================================
//
// 
//
//==========================================================================

Win32GLFrameBuffer::Win32GLFrameBuffer(void *hMonitor, int width, int height, int bits, int refreshHz, bool fullscreen) : BaseWinFB(width, height) 
{
	static int localmultisample=-1;
	
	if (localmultisample<0) localmultisample=gl_vid_multisample;

	m_Width = width;
	m_Height = height;
	m_Bits = bits;
	m_RefreshHz = refreshHz;
	m_Fullscreen = fullscreen;
	m_Lock=0;

	RECT r;
	LONG style, exStyle;

	static_cast<Win32GLVideo *>(Video)->GoFullscreen(fullscreen);

	m_displayDeviceName = 0;
	int monX = 0, monY = 0;

	if (hMonitor)
	{
		MONITORINFOEX mi;
		mi.cbSize = sizeof mi;

		if (GetMonitorInfo(HMONITOR(hMonitor), &mi))
		{
			strcpy(m_displayDeviceNameBuffer, mi.szDevice);
			m_displayDeviceName = m_displayDeviceNameBuffer;

			monX = int(mi.rcMonitor.left);
			monY = int(mi.rcMonitor.top);
		}
	}

	ShowWindow (Window, SW_SHOW);
	GetWindowRect(Window, &r);
	style = WS_VISIBLE | WS_CLIPSIBLINGS;
	exStyle = 0;

	if (fullscreen)
		style |= WS_POPUP;
	else
	{
		style |= WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX;
		exStyle |= WS_EX_WINDOWEDGE;
	}

	SetWindowLong(Window, GWL_STYLE, style);
	SetWindowLong(Window, GWL_EXSTYLE, exStyle);
	SetWindowPos(Window, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

	if (fullscreen)
	{
		MoveWindow(Window, monX, monY, width, GetTrueHeight(), FALSE);

		// And now, seriously, it IS in the right place. Promise.
	}
	else
	{
		MoveWindow(Window, r.left, r.top, width + (GetSystemMetrics(SM_CXSIZEFRAME) * 2), height + (GetSystemMetrics(SM_CYSIZEFRAME) * 2) + GetSystemMetrics(SM_CYCAPTION), FALSE);

		I_RestoreWindowedPos();
	}

	if (!static_cast<Win32GLVideo *>(Video)->InitHardware(Window, false, localmultisample))
	{
		vid_renderer = 0;
		return;
	}

	vsyncfunc = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");

	HDC hDC = GetDC(Window);
	m_supportsGamma = !!GetDeviceGammaRamp(hDC, (void *)m_origGamma);
	ReleaseDC(Window, hDC);
}

//==========================================================================
//
// 
//
//==========================================================================

Win32GLFrameBuffer::~Win32GLFrameBuffer()
{
	if (m_supportsGamma) 
	{
		HDC hDC = GetDC(Window);
		SetDeviceGammaRamp(hDC, (void *)m_origGamma);
		ReleaseDC(Window, hDC);
	}
	I_SaveWindowedPos();

	static_cast<Win32GLVideo *>(Video)->SetFullscreen(m_displayDeviceName, 0,0,0,0);

	ShowWindow (Window, SW_SHOW);
	SetWindowLong(Window, GWL_STYLE, WS_VISIBLE | WS_CLIPSIBLINGS | WS_OVERLAPPEDWINDOW);
	SetWindowLong(Window, GWL_EXSTYLE, WS_EX_WINDOWEDGE);
	SetWindowPos(Window, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
	I_GetEvent();

	static_cast<Win32GLVideo *>(Video)->Shutdown();
}


//==========================================================================
//
// 
//
//==========================================================================

void Win32GLFrameBuffer::InitializeState()
{
}

//==========================================================================
//
// 
//
//==========================================================================

bool Win32GLFrameBuffer::CanUpdate()
{
	if (!AppActive) return false;
	return true;
}

//==========================================================================
//
// 
//
//==========================================================================

void Win32GLFrameBuffer::SetGammaTable(WORD *tbl)
{
	HDC hDC = GetDC(Window);
	SetDeviceGammaRamp(hDC, (void *)tbl);
	ReleaseDC(Window, hDC);
}

//==========================================================================
//
// 
//
//==========================================================================

bool Win32GLFrameBuffer::Lock(bool buffered)
{
	m_Lock++;
	Buffer = MemBuffer;
	return true;
}

bool Win32GLFrameBuffer::Lock () 
{ 	
	return Lock(false); 
}

void Win32GLFrameBuffer::Unlock () 	
{ 
	m_Lock--;
}

bool Win32GLFrameBuffer::IsLocked () 
{ 
	return m_Lock>0;// true;
}

//==========================================================================
//
// 
//
//==========================================================================

bool Win32GLFrameBuffer::IsFullscreen()
{
	return m_Fullscreen;
}

void Win32GLFrameBuffer::PaletteChanged()
{
}

int Win32GLFrameBuffer::QueryNewPalette()
{
	return 0;
}

HRESULT Win32GLFrameBuffer::GetHR() 
{ 
	return 0; 
}

void Win32GLFrameBuffer::Blank () 
{
}

bool Win32GLFrameBuffer::PaintToWindow () 
{ 
	return false; 
}

bool Win32GLFrameBuffer::CreateResources () 
{ 
	return false; 
}

void Win32GLFrameBuffer::ReleaseResources () 
{
}

//==========================================================================
//
// 
//
//==========================================================================

void Win32GLFrameBuffer::SetVSync (bool vsync)
{
	if (vsyncfunc != NULL) vsyncfunc(vsync);
}

void Win32GLFrameBuffer::SwapBuffers()
{
	// Limiting the frame rate is as simple as waiting for the timer to signal this event.
	if (FPSLimitEvent != NULL)
	{
		WaitForSingleObject(FPSLimitEvent, 1000);
	}
	::SwapBuffers(static_cast<Win32GLVideo *>(Video)->m_hDC);
}

//==========================================================================
//
// 
//
//==========================================================================

void Win32GLFrameBuffer::NewRefreshRate ()
{
	if (m_Fullscreen)
	{
		setmodeneeded = true;
		NewWidth = screen->GetWidth();
		NewHeight = screen->GetHeight();
		NewBits = DisplayBits;
	}
}


IVideo *gl_CreateVideo()
{
	return new Win32GLVideo(0);
}