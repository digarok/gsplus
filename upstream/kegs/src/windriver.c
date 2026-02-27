const char rcsid_windriver_c[] = "@(#)$KmKId: windriver.c,v 1.26 2024-09-15 13:55:35+00 kentd Exp $";

/************************************************************************/
/*			KEGS: Apple //gs Emulator			*/
/*			Copyright 2002-2024 by Kent Dickey		*/
/*									*/
/*	This code is covered by the GNU GPL v3				*/
/*	See the file COPYING.txt or https://www.gnu.org/licenses/	*/
/*	This program is provided with no warranty			*/
/*									*/
/*	The KEGS web page is kegs.sourceforge.net			*/
/*	You may contact the author at: kadickey@alumni.princeton.edu	*/
/************************************************************************/

// Based on code from Chea Chee Keong from KEGS32, which was available at
//  http://www.geocities.com/akilgard/kegs32 (geocities is gone now)

#define WIN32_LEAN_AND_MEAN	/* Tell windows we want less header gunk */
#define STRICT			/* Tell Windows we want compile type checks */

#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <winsock.h>
#include <commctrl.h>
#include <io.h>			/* For _get_osfhandle */

#include "defc.h"
#include "win_dirent.h"

extern int Verbose;

typedef struct windowinfo {
	HWND	win_hwnd;
	HDC	win_dc;
	HDC	win_cdc;
	BITMAPINFO *win_bmapinfo_ptr;
	BITMAPINFOHEADER *win_bmaphdr_ptr;
	HBITMAP	win_dev_handle;

	Kimage	*kimage_ptr;	// KEGS Image pointer for window content
	char	*name_str;
	byte	*data_ptr;
	int	motion;
	int	mdepth;
	int	active;
	int	pixels_per_line;
	int	x_xpos;
	int	x_ypos;
	int	width;
	int	height;
	int	extra_width;
	int	extra_height;
} Window_info;

#include "protos_windriver.h"

Window_info g_mainwin_info = { 0 };
Window_info g_debugwin_info = { 0 };

int	g_win_max_width = 0;
int	g_win_max_height = 0;
int	g_num_a2_keycodes = 0;

int	g_win_button_states = 0;
int	g_win_hide_pointer = 0;
int	g_win_warp_pointer = 0;
int	g_win_warp_x = 0;
int	g_win_warp_y = 0;


/* this table is used to search for the Windows VK_* in col 1 or 2 */
/* flags bit 8 is or'ed into the VK, so we can distinguish keypad keys */
/* regardless of numlock */
int g_a2_key_to_wsym[][3] = {
	{ 0x35,	VK_ESCAPE,	0 },
	{ 0x7a,	VK_F1,	0 },
	{ 0x78,	VK_F2,	0 },
	{ 0x63,	VK_F3,	0 },
	{ 0x76,	VK_F4,	0 },
	{ 0x60,	VK_F5,	0 },
	{ 0x61,	VK_F6,	0 },
	{ 0x62,	VK_F7,	0 },
	{ 0x64,	VK_F8,	0 },
	{ 0x65,	VK_F9,	0 },
	{ 0x6d,	VK_F10,	0 },
	{ 0x67,	VK_F11,	0 },
	{ 0x6f,	VK_F12,	0 },
	{ 0x69,	VK_F13,	0 },
	{ 0x6b,	VK_F14,	0 },
	{ 0x71,	VK_F15,	0 },
	{ 0x7f, VK_PAUSE, VK_CANCEL+0x100 },	// Reset

	{ 0x12,	'1', 0 },
	{ 0x13,	'2', 0 },
	{ 0x14,	'3', 0 },
	{ 0x15,	'4', 0 },
	{ 0x17,	'5', 0 },
	{ 0x16,	'6', 0 },
	{ 0x1a,	'7', 0 },
	{ 0x1c,	'8', 0 },
	{ 0x19,	'9', 0 },
	{ 0x1d,	'0', 0 },
	{ 0x1b,	0xbd, 0 },		/* '-' */
	{ 0x18,	0xbb, 0 },		/* '=' */
	{ 0x33,	VK_BACK, 0 },		/* backspace */
	{ 0x72,	VK_INSERT+0x100, 0 },	/* Insert key */
	{ 0x74,	VK_PRIOR+0x100, 0 },	/* pageup */
	{ 0x47,	VK_NUMLOCK, VK_NUMLOCK+0x100 },	/* clear */
	{ 0x51,	VK_HOME+0x100, 0 },		/* KP_equal is HOME key */
	{ 0x4b,	VK_DIVIDE, VK_DIVIDE+0x100 },		// KP /
	{ 0x43,	VK_MULTIPLY, VK_MULTIPLY+0x100 },	// KP *

	{ 0x30,	VK_TAB, 0 },
	{ 0x32,	0xc0, 0 },		/* '`' */
	{ 0x0c,	'Q', 0 },
	{ 0x0d,	'W', 0 },
	{ 0x0e,	'E', 0 },
	{ 0x0f,	'R', 0 },
	{ 0x11,	'T', 0 },
	{ 0x10,	'Y', 0 },
	{ 0x20,	'U', 0 },
	{ 0x22,	'I', 0 },
	{ 0x1f,	'O', 0 },
	{ 0x23,	'P', 0 },
	{ 0x21,	0xdb, 0 },		/* [ */
	{ 0x1e,	0xdd, 0 },		/* ] */
	{ 0x2a,	0xdc, 0 },		/* backslash, bar */
	{ 0x75,	VK_DELETE+0x100, 0 },
	{ 0x77,	VK_END+0x100, VK_END },
	{ 0x79,	VK_NEXT+0x100, 0 },
	{ 0x59,	VK_NUMPAD7, VK_HOME },
	{ 0x5b,	VK_NUMPAD8, VK_UP },
	{ 0x5c,	VK_NUMPAD9, VK_PRIOR },
	{ 0x4e,	VK_SUBTRACT, VK_SUBTRACT+0x100 },

	{ 0x39,	VK_CAPITAL, 0 },  // Capslock
	{ 0x00,	'A', 0 },
	{ 0x01,	'S', 0 },
	{ 0x02,	'D', 0 },
	{ 0x03,	'F', 0 },
	{ 0x05,	'G', 0 },
	{ 0x04,	'H', 0 },
	{ 0x26,	'J', 0 },
	{ 0x28,	'K', 0 },
	{ 0x25,	'L', 0 },
	{ 0x29,	0xba, 0 },	/* ; */
	{ 0x27,	0xde, 0 },	/* single quote */
	{ 0x24,	VK_RETURN, 0 },
	{ 0x56,	VK_NUMPAD4, VK_LEFT },
	{ 0x57,	VK_NUMPAD5, VK_CLEAR },
	{ 0x58,	VK_NUMPAD6, VK_RIGHT },
	{ 0x45,	VK_ADD, 0 },

	{ 0x38,	VK_SHIFT, 0 },
	{ 0x06,	'Z', 0 },
	{ 0x07,	'X', 0 },
	{ 0x08,	'C', 0 },
	{ 0x09,	'V', 0 },
	{ 0x0b,	'B', 0 },
	{ 0x2d,	'N', 0 },
	{ 0x2e,	'M', 0 },
	{ 0x2b,	0xbc, 0 },		/* , */
	{ 0x2f,	0xbe, 0 },		/* . */
	{ 0x2c,	0xbf, 0 },		/* / */
	{ 0x3e,	VK_UP+0x100, 0 },
	{ 0x53,	VK_NUMPAD1, VK_END },
	{ 0x54,	VK_NUMPAD2, VK_DOWN },
	{ 0x55,	VK_NUMPAD3, VK_NEXT },

	{ 0x36,	VK_CONTROL, VK_CONTROL+0x100 },
	{ 0x37,	VK_SCROLL, VK_MENU+0x100 },	// Command=scr_lock or alt-r
	{ 0x3a,	VK_SNAPSHOT+0x100, VK_MENU },	// Opt=prntscrn or alt-l
	{ 0x31,	' ', 0 },
	{ 0x3b,	VK_LEFT+0x100, 0 },
	{ 0x3d,	VK_DOWN+0x100, 0 },
	{ 0x3c,	VK_RIGHT+0x100, 0 },
	{ 0x52,	VK_NUMPAD0, VK_INSERT },
	{ 0x41,	VK_DECIMAL, VK_DELETE },
	{ 0x4c,	VK_RETURN+0x100, 0 },
	{ -1, -1, -1 }
};

#if 0
int
win_nonblock_read_stdin(int fd, char *bufptr, int len)
{
	HANDLE	oshandle;
	DWORD	dwret;
	int	ret;

	errno = EAGAIN;
	oshandle = (HANDLE)_get_osfhandle(fd);	// get stdin handle
	dwret = WaitForSingleObject(oshandle, 1);	// wait 1msec for data
	ret = -1;
	if(dwret == WAIT_OBJECT_0) {
		ret = read(fd, bufptr, len);
	}
	return ret;
}
#endif

Window_info *
win_find_win_info_ptr(HWND hwnd)
{
	if(hwnd == g_mainwin_info.win_hwnd) {
		return &g_mainwin_info;
	}
	if(hwnd == g_debugwin_info.win_hwnd) {
		return &g_debugwin_info;
	}
	return 0;
}

void
win_hide_pointer(Window_info *win_info_ptr, int do_hide)
{
	ShowCursor(!do_hide);
	// printf("Doing ShowCursor(%d)\n", !do_hide);
}

int
win_update_mouse(Window_info *win_info_ptr, int raw_x, int raw_y,
				int button_states, int buttons_valid)
{
	Kimage	*kimage_ptr;
	int	buttons_changed, x, y;

	kimage_ptr = win_info_ptr->kimage_ptr;
	x = video_scale_mouse_x(kimage_ptr, raw_x, 0);
	y = video_scale_mouse_y(kimage_ptr, raw_y, 0);

	// printf("wum: %d,%d -> %d,%d\n", raw_x, raw_y, x, y);

	buttons_changed = ((g_win_button_states & buttons_valid) !=
								button_states);
	g_win_button_states = (g_win_button_states & ~buttons_valid) |
					(button_states & buttons_valid);
	if(g_win_warp_pointer && (raw_x == g_win_warp_x) &&
			(raw_y == g_win_warp_y) && (!buttons_changed) ) {
		/* tell adb routs to recenter but ignore this motion */
		adb_update_mouse(kimage_ptr, x, y, 0, -1);
		return 0;
	}
	return adb_update_mouse(kimage_ptr, x, y, button_states,
							buttons_valid & 7);
}

void
win_event_mouse(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	Window_info *win_info_ptr;
	word32	flags;
	int	buttons, x, y, hide, warp;

	win_info_ptr = win_find_win_info_ptr(hwnd);
	if(!win_info_ptr) {
		return;
	}

	flags = (word32)wParam;
	x = LOWORD(lParam);
	y = HIWORD(lParam);

	buttons = (flags & 1) | (((flags >> 1) & 1) << 2) |
						(((flags >> 4) & 1) << 1);
#if 0
	printf("Mouse at %d, %d fl: %08x, but: %d\n", x, y, flags, buttons);
#endif
	win_info_ptr->motion |= win_update_mouse(win_info_ptr, x, y, buttons,
									7);

	hide = 0;
	warp = 0;
	hide = adb_get_hide_warp_info(win_info_ptr->kimage_ptr, &warp);
	if(warp != g_win_warp_pointer) {
		win_info_ptr->motion = 1;
	}
	g_win_warp_pointer = warp;
	if(g_win_hide_pointer != hide) {
		win_hide_pointer(win_info_ptr, hide);
	}
	g_win_hide_pointer = hide;
}

void
win_event_key(HWND hwnd, WPARAM wParam, LPARAM lParam, int down)
{
	Window_info *win_info_ptr;
	Kimage	*kimage_ptr;
	word32	vk, raw_vk, flags, capslock_state;
	int	a2code, is_up;
	int	i;

	win_info_ptr = win_find_win_info_ptr(hwnd);
	if(!win_info_ptr) {
		return;
	}
	kimage_ptr = win_info_ptr->kimage_ptr;
	raw_vk = (word32)wParam;
	flags = HIWORD(lParam);
#if 0
	printf("win_event_key: raw:%04x lParam:%08x d:%d flags:%08x\n",
		raw_vk, (word32)lParam, down, flags);
#endif

	if((flags & 0x4000) && down) {
		/* auto-repeating, just ignore it */
		return;
	}

	vk = raw_vk + (flags & 0x100);
#if 0
	printf("Key event, vk=%04x, down:%d, repeat: %d, flags: %08x\n",
			vk, down, repeat, flags);
#endif

	/* remap a few keys here.. sigh */
	if((vk & 0xff) == VK_APPS) {
		/* remap to command */
		vk = VK_MENU;
	}

	if((vk & 0xff) == VK_CAPITAL) {
		// Fix up capslock info: Windows gives us a down, then up event
		//  when the capslock key itself is pressed and released.  We
		//  need to ask for the true toggle state instead
		capslock_state = (GetKeyState(VK_CAPITAL) & 1);
		down = capslock_state;
	}

	/* search a2key_to_wsym to find wsym in col 1 or 2 */
	i = 0;
	is_up = !down;
	for(i = g_num_a2_keycodes-1; i >= 0; i--) {
		a2code = g_a2_key_to_wsym[i][0];
		if((vk == g_a2_key_to_wsym[i][1]) ||
					(vk == g_a2_key_to_wsym[i][2])) {
			vid_printf("Found vk:%04x = %02x\n", vk, a2code);
			adb_physical_key_update(kimage_ptr, a2code, 0, is_up);
			return;
		}
	}
	printf("VK: %04x unknown\n", vk);
}

void
win_event_redraw(HWND hwnd)
{
	Window_info *win_info_ptr;

	win_info_ptr = win_find_win_info_ptr(hwnd);

	if(win_info_ptr) {
		video_set_x_refresh_needed(win_info_ptr->kimage_ptr, 1);
	}
}

void
win_event_destroy(HWND hwnd)
{
	Window_info *win_info_ptr;

	win_info_ptr = win_find_win_info_ptr(hwnd);
	if(win_info_ptr == 0) {
		return;
	}
	video_set_active(win_info_ptr->kimage_ptr, 0);
	win_info_ptr->active = 0;
	if(win_info_ptr == &g_mainwin_info) {
		my_exit(0);
	} else {
		ShowWindow(win_info_ptr->win_hwnd, SW_HIDE);
		ReleaseDC(hwnd, win_info_ptr->win_dc);
		DeleteDC(win_info_ptr->win_cdc);
		DeleteObject(win_info_ptr->win_dev_handle);
		GlobalFree(win_info_ptr->win_bmapinfo_ptr);
		win_info_ptr->win_hwnd = 0;
		win_info_ptr->data_ptr = 0;
	}
}

void
win_event_move(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	Window_info *win_info_ptr;
	int	x_xpos, x_ypos;

	// These WM_MOVE events indicate the window is being moved

	win_info_ptr = win_find_win_info_ptr(hwnd);
	if(!win_info_ptr) {
		return;
	}
	// printf("WM_MOVE: %04x %08x\n", (word32)wParam, (word32)lParam);
	x_xpos = lParam & 0xffff;
	x_ypos = (lParam >> 16) & 0xffff;
	video_update_xpos_ypos(win_info_ptr->kimage_ptr, x_xpos, x_ypos);
}

void
win_event_size(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	Window_info *win_info_ptr;
	int	width, height;

	// These WM_SIZE events indicate the window is being resized

	win_info_ptr = win_find_win_info_ptr(hwnd);
	if(!win_info_ptr) {
		return;
	}
	// printf("WM_SIZE: %04x %08x\n", (word32)wParam, (word32)lParam);
	width = lParam & 0xffff;
	height = (lParam >> 16) & 0xffff;
	video_update_scale(win_info_ptr->kimage_ptr, width, height, 0);
#if 0
	printf("Frac width: %f\n",
		win_info_ptr->kimage_ptr->scale_width_a2_to_x / 65536.0);
#endif

	// The following try to do "live updating" of the resize
	win_info_ptr->kimage_ptr->x_refresh_needed = 1;
	x_update_display(win_info_ptr);
}

void
win_event_minmaxinfo(HWND hwnd, LPARAM lParam)
{
	Window_info *win_info_ptr;
	MINMAXINFO *minmax_ptr;
	int	a2_width, a2_height;

	// Windows sends WM_GETMINMAXINFO events when resizing is occurring,
	//  and we can modify the *lParam MINMAXINFO structure to set the
	//  minimum and maximum Track size (the size of the window)
	// This code forces the minimum to be the A2 window size, and the
	//  maximum to be the screen size
	win_info_ptr = win_find_win_info_ptr(hwnd);
	if(!win_info_ptr) {
		return;
	}
	minmax_ptr = (MINMAXINFO *)lParam;
#if 0
	printf("MinMax: mintrack.x:%d, mintrack.y:%d\n",
		minmax_ptr->ptMinTrackSize.x,
		minmax_ptr->ptMinTrackSize.y);
#endif
	a2_width = video_get_a2_width(win_info_ptr->kimage_ptr);
	a2_height = video_get_a2_height(win_info_ptr->kimage_ptr);
	minmax_ptr->ptMinTrackSize.x = a2_width + win_info_ptr->extra_width;
	minmax_ptr->ptMinTrackSize.y = a2_height + win_info_ptr->extra_height;
	minmax_ptr->ptMaxTrackSize.x = g_win_max_width +
						win_info_ptr->extra_width;
	minmax_ptr->ptMaxTrackSize.y = g_win_max_height +
						win_info_ptr->extra_height;
}

void
win_event_focus(HWND hwnd, int gain_focus)
{
	Window_info *win_info_ptr;
	word32	c025_val, info;

	win_info_ptr = win_find_win_info_ptr(hwnd);
	if(!win_info_ptr) {
		return;
	}

	if(gain_focus) {
		// printf("Got focus on %p\n", hwnd);
		// Get shift, ctrl, capslock state
		c025_val = 0;
		info = GetKeyState(VK_SHIFT);		// left or right
		if(info & 0x8000) {
			c025_val |= 1;			// Shift key is down
		}
		info = GetKeyState(VK_CONTROL);		// left or right
		if(info & 0x8000) {
			c025_val |= 2;
		}
		info = GetKeyState(VK_CAPITAL);		// Capslock?
		if(info & 1) {
			c025_val |= 4;			// Capslock key is down
		}
		//printf("Calling update_c025 with %03x\n", c025_val);
		adb_update_c025_mask(win_info_ptr->kimage_ptr, c025_val, 7);
	} else {
		// printf("Lost focus on %p\n", hwnd);
	}
	if(win_info_ptr == &g_mainwin_info) {
		adb_kbd_repeat_off();
		adb_mainwin_focus(gain_focus);
	}
}

LRESULT CALLBACK
win_event_handler(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam)
{

#if 0
	printf("Message: umsg: %04x, wparam:%04x lParam:%08x\n", umsg,
			(word32)wParam, (word32)lParam);
#endif

	switch(umsg) {
	case WM_MOUSEMOVE:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
		win_event_mouse(hwnd, wParam, lParam);
		return 0;
	case WM_KEYUP:
	case WM_SYSKEYUP:
		win_event_key(hwnd, wParam, lParam, 0);
		return 0;
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		win_event_key(hwnd, wParam, lParam, 1);
		return 0;
	case WM_SYSCOMMAND:
		// Alt key press can cause this.  Return 0 for SC_KEYMENU
		if(wParam == SC_KEYMENU) {
			return 0;
		}
		break;
	case WM_KILLFOCUS:
		win_event_focus(hwnd, 0);
		break;
	case WM_SETFOCUS:
		win_event_focus(hwnd, 1);
		break;
	case WM_DESTROY:
		win_event_destroy(hwnd);
		return 0;
	case WM_PAINT:
		win_event_redraw(hwnd);
		break;
	case WM_MOVE:
		win_event_move(hwnd, wParam, lParam);
		break;
	case WM_SIZE:
		win_event_size(hwnd, wParam, lParam);
		break;
	case WM_GETMINMAXINFO:
		win_event_minmaxinfo(hwnd, lParam);
		break;
	}
#if 0
	switch(umsg) {
		HANDLE_MSG(hwnd, WM_KEYUP, win_event_key);
		HANDLE_MSG(hwnd, WM_KEYDOWN, win_event_key);
		HANDLE_MSG(hwnd, WM_SYSKEYUP, win_event_key);
		HANDLE_MSG(hwnd, WM_SYSKEYDOWN, win_event_key);
		HANDLE_MSG(hwnd, WM_DESTROY, win_event_destroy);
	}
#endif

#if 0
	switch(umsg) {
	case WM_NCACTIVATE:
	case WM_NCHITTEST:
	case WM_NCMOUSEMOVE:
	case WM_SETCURSOR:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_CONTEXTMENU:
	case WM_RBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_PAINT:

		break;
	default:
		printf("Got umsg2: %d\n", umsg);
	}
#endif

	return DefWindowProc(hwnd, umsg, wParam, lParam);
}


int
main(int argc, char **argv)
{
	int	ret, mdepth;

	ret = parse_argv(argc, argv, 1);
	if(ret) {
		printf("parse_argv ret: %d, stopping\n", ret);
		exit(1);
	}

	mdepth = 32;

	video_set_blue_mask(0x0000ff);
	video_set_green_mask(0x00ff00);
	video_set_red_mask(0xff0000);

	g_win_max_width = GetSystemMetrics(SM_CXSCREEN);
	g_win_max_height = GetSystemMetrics(SM_CYSCREEN);
	vid_printf("g_win_max_width:%d, g_win_max_height:%d\n",
					g_win_max_width, g_win_max_height);

	ret = kegs_init(mdepth, g_win_max_width, g_win_max_height, 0);
	printf("kegs_init done\n");
	if(ret) {
		printf("kegs_init ret: %d, stopping\n", ret);
		exit(1);
	}

	win_video_init(mdepth);

	printf("Entering main loop!\n");
	fflush(stdout);
	while(1) {
		ret = run_16ms();
		if(ret != 0) {
			printf("run_16ms returned: %d\n", ret);
			break;
		}
		x_update_display(&g_mainwin_info);
		x_update_display(&g_debugwin_info);
		check_input_events();
	}
	xdriver_end();
	exit(0);
}

void
check_input_events()
{
	MSG	msg;
	POINT	pt;
	BOOL	ret;
	Window_info *win_info_ptr;

	while(PeekMessage(&msg, 0, 0, 0, PM_NOREMOVE)) {
		if(GetMessage(&msg, 0, 0, 0) > 0) {
			//TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else {
			printf("GetMessage returned <= 0\n");
			my_exit(2);
		}
	}

	win_info_ptr = &g_mainwin_info;
	if(win_info_ptr->motion == 0) {
		return;
	}

	// ONLY look at g_mainwin_info!
	win_info_ptr->motion = 0;

	if(g_win_warp_pointer) {
		/* move mouse to center of screen */
		g_win_warp_x = video_unscale_mouse_x(win_info_ptr->kimage_ptr,
			BASE_MARGIN_LEFT + (A2_WINDOW_WIDTH/2), 0);
		g_win_warp_y = video_unscale_mouse_y(win_info_ptr->kimage_ptr,
			BASE_MARGIN_TOP + (A2_WINDOW_HEIGHT/2), 0);
		pt.x = g_win_warp_x;
		pt.y = g_win_warp_y;
		ClientToScreen(win_info_ptr->win_hwnd, &pt);
		ret = SetCursorPos(pt.x, pt.y);
#if 0
		printf("Did SetCursorPos(%d, %d) warp_x:%d,y:%d, ret:%d\n",
			pt.x, pt.y, g_win_warp_x, g_win_warp_y, ret);
#endif
	}

	return;
}

void
win_video_init(int mdepth)
{
	WNDCLASS wndclass;
	int	a2code;
	int	i;

	video_set_palette();

	g_num_a2_keycodes = 0;
	for(i = 0; i < 0x7f; i++) {
		a2code = g_a2_key_to_wsym[i][0];
		if(a2code < 0) {
			g_num_a2_keycodes = i;
			break;
		}
	}
	wndclass.style = 0;
	wndclass.lpfnWndProc = (WNDPROC)win_event_handler;
	wndclass.cbClsExtra = 0;
	wndclass.cbWndExtra = 0;
	wndclass.hInstance = GetModuleHandle(NULL);
	wndclass.hIcon = LoadIcon((HINSTANCE)NULL, IDI_APPLICATION);
	wndclass.hCursor = LoadCursor((HINSTANCE) NULL, IDC_ARROW);
	wndclass.hbrBackground = GetStockObject(WHITE_BRUSH);
	wndclass.lpszMenuName = NULL;
	wndclass.lpszClassName = "kegswin";

	// Register the window
	if(!RegisterClass(&wndclass)) {
		printf("Registering window failed\n");
		exit(1);
	}

	win_init_window(&g_mainwin_info, video_get_kimage(0), "KEGS", mdepth);
	win_init_window(&g_debugwin_info, video_get_kimage(1),
						"KEGS Debugger", mdepth);

	win_create_window(&g_mainwin_info);
}

void
win_init_window(Window_info *win_info_ptr, Kimage *kimage_ptr, char *name_str,
							int mdepth)
{
	int	height, width, x_xpos, x_ypos;

	height = video_get_x_height(kimage_ptr);
	width = video_get_x_width(kimage_ptr);

	x_xpos = video_get_x_xpos(kimage_ptr);
	x_ypos = video_get_x_ypos(kimage_ptr);

	win_info_ptr->win_hwnd = 0;
	win_info_ptr->win_dc = 0;
	win_info_ptr->win_cdc = 0;
	win_info_ptr->win_bmapinfo_ptr = 0;
	win_info_ptr->win_bmaphdr_ptr = 0;
	win_info_ptr->win_dev_handle = 0;
	win_info_ptr->kimage_ptr = kimage_ptr;
	win_info_ptr->name_str = name_str;
	win_info_ptr->data_ptr = 0;
	win_info_ptr->motion = 0;
	win_info_ptr->mdepth = mdepth;
	win_info_ptr->active = 0;
	win_info_ptr->pixels_per_line = width;
	win_info_ptr->x_xpos = x_xpos;
	win_info_ptr->x_ypos = x_ypos;
	win_info_ptr->width = width;
	win_info_ptr->height = height;
}

void
win_create_window(Window_info *win_info_ptr)
{
	HWND	win_hwnd;
	RECT	rect;
	BITMAPINFO *bmapinfo_ptr;
	BITMAPINFOHEADER *bmaphdr_ptr;
	HBITMAP	win_dev_handle;
	Kimage	*kimage_ptr;
	int	height, width, extra_width, extra_height;
	int	extra_size, w_flags;

	kimage_ptr = win_info_ptr->kimage_ptr;

	height = win_info_ptr->height;
	width = win_info_ptr->width;

	printf("Got height: %d, width:%d\n", height, width);

	// We must call CreateWindow with a width,height that accounts for
	//  the title bar and any other stuff.  Use AdjustWindowRect() to
	//  calculate this info for us
	w_flags = WS_TILED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX |
								WS_SIZEBOX;
	rect.left = 0;
	rect.top = 0;
	rect.right = width;
	rect.bottom = height;
	(void)AdjustWindowRect(&rect, w_flags, 0);
	extra_width = rect.right - rect.left - width;
	extra_height = rect.bottom - rect.top - height;
	win_info_ptr->extra_width = extra_width;
	win_info_ptr->extra_height = extra_height;
	win_hwnd = CreateWindow("kegswin", win_info_ptr->name_str, w_flags,
		win_info_ptr->x_xpos, win_info_ptr->x_ypos, width + extra_width,
		height + extra_height, NULL, NULL, GetModuleHandle(NULL), NULL);
	win_info_ptr->win_hwnd = win_hwnd;
	win_info_ptr->active = 0;

	video_set_active(kimage_ptr, 1);
	video_update_scale(kimage_ptr, win_info_ptr->width,
						win_info_ptr->height, 1);

	printf("win_hwnd = %p, height = %d\n", win_hwnd, height);
	GetWindowRect(win_hwnd, &rect);
	printf("...rect is: %ld, %ld, %ld, %ld\n", rect.left, rect.top,
		rect.right, rect.bottom);

	win_info_ptr->win_dc = GetDC(win_hwnd);

	SetTextColor(win_info_ptr->win_dc, 0);
	SetBkColor(win_info_ptr->win_dc, 0xffffff);

	win_info_ptr->win_cdc = CreateCompatibleDC(win_info_ptr->win_dc);
	printf("win_cdc: %p, win_dc:%p\n", win_info_ptr->win_cdc,
						win_info_ptr->win_dc);


	printf("Getting height, kimage_ptr:%p\n", kimage_ptr);
	fflush(stdout);

	win_info_ptr->data_ptr = 0;

	extra_size = sizeof(RGBQUAD);
	bmapinfo_ptr = (BITMAPINFO *)GlobalAlloc(GPTR,
			sizeof(BITMAPINFOHEADER) + extra_size);
	win_info_ptr->win_bmapinfo_ptr = bmapinfo_ptr;

	bmaphdr_ptr = (BITMAPINFOHEADER *)bmapinfo_ptr;
	win_info_ptr->win_bmaphdr_ptr = bmaphdr_ptr;
	bmaphdr_ptr->biSize = sizeof(BITMAPINFOHEADER);
	bmaphdr_ptr->biWidth = g_win_max_width;
	bmaphdr_ptr->biHeight = -g_win_max_height;
	bmaphdr_ptr->biPlanes = 1;
	bmaphdr_ptr->biBitCount = win_info_ptr->mdepth;
	bmaphdr_ptr->biCompression = BI_RGB;
	bmaphdr_ptr->biClrUsed = 0;

	/* Use g_bmapinfo_ptr, adjusting width, height */
	printf("bmaphdr_ptr:%p\n", bmaphdr_ptr);

	printf("About to call CreateDIBSection, win_dc:%p\n",
						win_info_ptr->win_dc);
	fflush(stdout);
	win_dev_handle = CreateDIBSection(win_info_ptr->win_dc,
			win_info_ptr->win_bmapinfo_ptr, DIB_RGB_COLORS,
			(VOID **)&(win_info_ptr->data_ptr), NULL, 0);
	win_info_ptr->win_dev_handle = win_dev_handle;

	win_info_ptr->pixels_per_line = g_win_max_width;
	printf("kim: %p, dev:%p data: %p\n", kimage_ptr,
			win_dev_handle, win_info_ptr->data_ptr);
	fflush(stdout);
}

void
xdriver_end()
{
	printf("xdriver_end\n");
}

void
win_resize_window(Window_info *win_info_ptr)
{
	RECT	rect1, rect2;
	BOOL	ret;
	Kimage	*kimage_ptr;
	int	x_width, x_height;

	kimage_ptr = win_info_ptr->kimage_ptr;
	x_width = video_get_x_width(kimage_ptr);
	x_height = video_get_x_height(kimage_ptr);
	// printf("win_resize_window, x_w:%d, x_h:%d\n", x_width, x_height);

	ret = GetWindowRect(win_info_ptr->win_hwnd, &rect1);
	ret = GetClientRect(win_info_ptr->win_hwnd, &rect2);
#if 0
	printf("window_rect: l:%d, t:%d, r:%d, b:%d\n",
			rect1.left, rect1.top, rect1.right, rect1.bottom);
	printf("client_rect: l:%d, t:%d, r:%d, b:%d\n",
			rect2.left, rect2.top, rect2.right, rect2.bottom);
#endif
	ret = MoveWindow(win_info_ptr->win_hwnd, rect1.left, rect1.top,
		x_width + win_info_ptr->extra_width,
		x_height + win_info_ptr->extra_height, TRUE);
	// printf("MoveWindow ret:%d\n", ret);
	win_info_ptr->width = x_width;
	win_info_ptr->height = x_height;
}

void
x_update_display(Window_info *win_info_ptr)
{
	Change_rect rect;
	void	*bitm_old;
	//POINT	point;
	int	valid, a2_active, x_active;
	int	i;

	a2_active = video_get_active(win_info_ptr->kimage_ptr);
	x_active = win_info_ptr->active;

	if(x_active && !a2_active) {
		// We need to SW_HIDE this window
		ShowWindow(win_info_ptr->win_hwnd, SW_HIDE);
		x_active = 0;
		win_info_ptr->active = x_active;
	}
	if(!x_active && a2_active) {
		// We need to SW_SHOWDEFAULT this window (and maybe create it)
		if(win_info_ptr->win_hwnd == 0) {
			win_create_window(win_info_ptr);
		}
		ShowWindow(win_info_ptr->win_hwnd, SW_SHOWDEFAULT);
		UpdateWindow(win_info_ptr->win_hwnd);
		x_active = 1;
		win_info_ptr->active = x_active;
	}
	if(x_active == 0) {
		return;
	}

	if(video_change_aspect_needed(win_info_ptr->kimage_ptr,
				win_info_ptr->width, win_info_ptr->height)) {
		win_resize_window(win_info_ptr);
	}
	for(i = 0; i < MAX_CHANGE_RECTS; i++) {
		valid = video_out_data(win_info_ptr->data_ptr,
			win_info_ptr->kimage_ptr, win_info_ptr->pixels_per_line,
			&rect, i);
		if(!valid) {
			break;
		}
#if 0
		point.x = 0;
		point.y = 0;
		ClientToScreen(win_info_ptr->win_hwnd, &point);
#endif
		bitm_old = SelectObject(win_info_ptr->win_cdc,
					win_info_ptr->win_dev_handle);

		BitBlt(win_info_ptr->win_dc, rect.x, rect.y, rect.width,
			rect.height, win_info_ptr->win_cdc, rect.x, rect.y,
			SRCCOPY);

		SelectObject(win_info_ptr->win_cdc, bitm_old);
	}
}

void
x_hide_pointer(int do_hide)
{
	if(do_hide) {
		ShowCursor(0);
	} else {
		ShowCursor(1);
	}
}

int
opendir_int(DIR *dirp, const char *in_filename)
{
	HANDLE	handle1;
	wchar_t	*wcstr;
	char	*filename;
	size_t	ret_val;
	int	buflen, len;
	int	i;

	printf("opendir on %s\n", in_filename);
	len = (int)strlen(in_filename);
	buflen = len + 8;
	if(buflen >= sizeof(dirp->dirent.d_name)) {
		printf("buflen %d >= d_name %d\n", buflen,
					(int)sizeof(dirp->dirent.d_name));
		return 1;
	}
	filename = &dirp->dirent.d_name[0];
	memcpy(filename, in_filename, len + 1);
	while(len && (filename[len-1] == '/')) {
		filename[len - 1] = 0;
		len--;
	}
	cfg_strlcat(filename, "/*.*", buflen);
	for(i = 0; i < len; i++) {
		if(filename[i] == '/') {
			filename[i] = '\\';
		}
	}
	len = (int)strlen(filename);
	wcstr = malloc(buflen * 2);
	(void)mbstowcs_s(&ret_val, wcstr, buflen, filename, _TRUNCATE);
	handle1 = FindFirstFileW(wcstr, dirp->find_data_ptr);
	dirp->win_handle = handle1;
	free(wcstr);
	if(handle1) {
		dirp->find_data_valid = 1;
		return 0;
	}
	return 1;
}

DIR *
opendir(const char *in_filename)
{
	DIR	*dirp;
	int	ret;

	dirp = calloc(1, sizeof(DIR));
	if(!dirp) {
		return 0;
	}
	dirp->find_data_valid = 1;
	dirp->find_data_ptr = calloc(1, sizeof(WIN32_FIND_DATAW));
	ret = 1;
	if(dirp->find_data_ptr) {
		ret = opendir_int(dirp, in_filename);
	}
	if(ret) {		// Bad
		free(dirp->find_data_ptr);		// free(0) is OK
		free(dirp);
		return 0;
	}
	return dirp;
}

struct dirent *
readdir(DIR *dirp)
{
	WIN32_FIND_DATAW *find_data_ptr;
	HANDLE	handle1;
	size_t	ret_val;
	BOOL	ret;

	handle1 = dirp->win_handle;
	find_data_ptr = dirp->find_data_ptr;
	if(!handle1 || !find_data_ptr) {
		return 0;
	}
	ret = 1;
	if(!dirp->find_data_valid) {
		if(handle1) {
			find_data_ptr->cFileName[MAX_PATH-1] = 0;
			ret = FindNextFileW(handle1, find_data_ptr);
		}
	}
	dirp->find_data_valid = 0;
	if(!ret) {
		return 0;
	}
	(void)wcstombs_s(&ret_val, &(dirp->dirent.d_name[0]),
				(int)sizeof(dirp->dirent.d_name),
				&(find_data_ptr->cFileName[0]), _TRUNCATE);
	printf("Returning file %s\n", &(dirp->dirent.d_name[0]));

	return &(dirp->dirent);;
}

int
closedir(DIR *dirp)
{
	FindClose(dirp->win_handle);
	free(dirp->find_data_ptr);
	free(dirp);

	return 0;
}

int
lstat(const char *path, struct stat *bufptr)
{
	return stat(path, bufptr);
}

int
ftruncate(int fd, word32 length)
{
	HANDLE	handle1;

	handle1 = (HANDLE)_get_osfhandle(fd);
	SetFilePointer(handle1, length, 0, FILE_BEGIN);
	SetEndOfFile(handle1);

	return 0;
}

