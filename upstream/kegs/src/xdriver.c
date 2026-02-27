const char rcsid_xdriver_c[] = "@(#)$KmKId: xdriver.c,v 1.244 2025-01-07 16:40:09+00 kentd Exp $";

/************************************************************************/
/*			KEGS: Apple //gs Emulator			*/
/*			Copyright 2002-2025 by Kent Dickey		*/
/*									*/
/*	This code is covered by the GNU GPL v3				*/
/*	See the file COPYING.txt or https://www.gnu.org/licenses/	*/
/*	This program is provided with no warranty			*/
/*									*/
/*	The KEGS web page is kegs.sourceforge.net			*/
/*	You may contact the author at: kadickey@alumni.princeton.edu	*/
/************************************************************************/

# if !defined(__CYGWIN__) && !defined(__POWERPC__)
/* No shared memory on Cygwin */
# define X_SHARED_MEM
#endif /* CYGWIN */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <time.h>
#include <stdlib.h>
#include <signal.h>

#ifdef X_SHARED_MEM
# include <sys/ipc.h>
# include <sys/shm.h>
# include <X11/extensions/XShm.h>
#endif

int XShmQueryExtension(Display *display);
void _XInitImageFuncPtrs(XImage *xim);

#include "defc.h"

extern int g_video_scale_algorithm;
extern int g_audio_enable;

typedef struct windowinfo {
	XShmSegmentInfo *seginfo;
	XImage *xim;
	Kimage *kimage_ptr;		// KEGS Image pointer for window content
	char	*name_str;
	Window	x_win;
	GC	x_winGC;
	Atom	delete_atom;
	int	x_use_shmem;
	int	active;
	int	width_req;
	int	pixels_per_line;
	int	main_height;
	int	full_min_width;
	int	full_min_height;
} Window_info;

#include "protos_xdriver.h"

int	g_x_warp_x = 0;
int	g_x_warp_y = 0;
int	g_x_warp_pointer = 0;
int	g_x_hide_pointer = 0;

extern int Verbose;
int	g_x_screen_depth = 24;
int	g_x_screen_mdepth = 32;
int	g_x_max_width = 0;
int	g_x_max_height = 0;
int	g_screen_num = 0;

extern int _Xdebug;

int	g_auto_repeat_on = -1;

Display *g_display = 0;
Visual	*g_vis = 0;
Colormap g_default_colormap = 0;

Window_info g_mainwin_info = { 0 };
Window_info g_debugwin_info = { 0 };

Cursor	g_cursor;
Pixmap	g_cursor_shape;
Pixmap	g_cursor_mask;

XColor	g_xcolor_black = { 0, 0x0000, 0x0000, 0x0000, DoRed|DoGreen|DoBlue, 0 };
XColor	g_xcolor_white = { 0, 0xffff, 0xffff, 0xffff, DoRed|DoGreen|DoBlue, 0 };

const char *g_x_selection_strings[3] = {
	// If we get SelectionRequests with target=Atom("TARGETS"), then
	//  send XA_STRING plus this list to say what we can provide for copy
	"UTF8_STRING", "text/plain", "text/plain;charset=utf-8"
};

int	g_x_num_targets = 0;
Atom g_x_targets_array[5] = { 0 };

Atom g_x_atom_targets = None;		// Will be set to "TARGETS"

int g_depth_attempt_list[] = { 24, 16, 15 };

#define X_EVENT_LIST_ALL_WIN						\
	(ExposureMask | ButtonPressMask | ButtonReleaseMask |		\
	OwnerGrabButtonMask | KeyPressMask | KeyReleaseMask |		\
	KeymapStateMask | FocusChangeMask)

#define X_BASE_WIN_EVENT_LIST						\
	(X_EVENT_LIST_ALL_WIN | PointerMotionMask | ButtonMotionMask |	\
	StructureNotifyMask)

#define X_A2_WIN_EVENT_LIST						\
	(X_BASE_WIN_EVENT_LIST)

int	g_num_a2_keycodes = 0;

int	g_x_a2_key_to_xsym[][3] = {
	{ 0x35,	XK_Escape,	0 },
	{ 0x7a,	XK_F1,	0 },
	{ 0x78,	XK_F2,	0 },
	{ 0x63,	XK_F3,	0 },
	{ 0x76,	XK_F4,	0 },
	{ 0x60,	XK_F5,	0 },
	{ 0x61,	XK_F6,	0 },
	{ 0x62,	XK_F7,	0 },
	{ 0x64,	XK_F8,	0 },
	{ 0x65,	XK_F9,	0 },
	{ 0x6d,	XK_F10,	0 },
	{ 0x67,	XK_F11,	0 },
	{ 0x6f,	XK_F12,	0 },
	{ 0x69,	XK_F13,	0 },
	{ 0x6b,	XK_F14,	0 },
	{ 0x71,	XK_F15,	0 },
	{ 0x7f, XK_Pause, XK_Break },
	{ 0x32,	'`', '~' },		/* Key number 18? */
	{ 0x12,	'1', '!' },
	{ 0x13,	'2', '@' },
	{ 0x14,	'3', '#' },
	{ 0x15,	'4', '$' },
	{ 0x17,	'5', '%' },
	{ 0x16,	'6', '^' },
	{ 0x1a,	'7', '&' },
	{ 0x1c,	'8', '*' },
	{ 0x19,	'9', '(' },
	{ 0x1d,	'0', ')' },
	{ 0x1b,	'-', '_' },
	{ 0x18,	'=', '+' },
	{ 0x33,	XK_BackSpace, 0 },
	{ 0x72,	XK_Insert, XK_Help },	/* Help? */
/*	{ 0x73,	XK_Home, 0 },		alias XK_Home to be XK_KP_Equal! */
	{ 0x74,	XK_Page_Up, 0 },
	{ 0x47,	XK_Num_Lock, XK_Clear },	/* Clear */
	{ 0x51,	XK_KP_Equal, XK_Home },		/* Note XK_Home alias! */
	{ 0x4b,	XK_KP_Divide, 0 },
	{ 0x43,	XK_KP_Multiply, 0 },

	{ 0x30,	XK_Tab, 0 },
	{ 0x0c,	'q', 'Q' },
	{ 0x0d,	'w', 'W' },
	{ 0x0e,	'e', 'E' },
	{ 0x0f,	'r', 'R' },
	{ 0x11,	't', 'T' },
	{ 0x10,	'y', 'Y' },
	{ 0x20,	'u', 'U' },
	{ 0x22,	'i', 'I' },
	{ 0x1f,	'o', 'O' },
	{ 0x23,	'p', 'P' },
	{ 0x21,	'[', '{' },
	{ 0x1e,	']', '}' },
	{ 0x2a,	0x5c, '|' },	/* backslash, bar */
	{ 0x75,	XK_Delete, 0 },
	{ 0x77,	XK_End, 0 },
	{ 0x79,	XK_Page_Down, 0 },
	{ 0x59,	XK_KP_7, XK_KP_Home },
	{ 0x5b,	XK_KP_8, XK_KP_Up },
	{ 0x5c,	XK_KP_9, XK_KP_Page_Up },
	{ 0x4e,	XK_KP_Subtract, 0 },

	{ 0x39,	XK_Caps_Lock, 0 },
	{ 0x00,	'a', 'A' },
	{ 0x01,	's', 'S' },
	{ 0x02,	'd', 'D' },
	{ 0x03,	'f', 'F' },
	{ 0x05,	'g', 'G' },
	{ 0x04,	'h', 'H' },
	{ 0x26,	'j', 'J' },
	{ 0x28,	'k', 'K' },
	{ 0x25,	'l', 'L' },
	{ 0x29,	';', ':' },
	{ 0x27,	0x27, '"' },	/* single quote */
	{ 0x24,	XK_Return, 0 },
	{ 0x56,	XK_KP_4, XK_KP_Left },
	{ 0x57,	XK_KP_5, XK_KP_Begin },
	{ 0x58,	XK_KP_6, XK_KP_Right },
	{ 0x45,	XK_KP_Add, 0 },

	{ 0x38,	XK_Shift_L, XK_Shift_R },
	{ 0x06,	'z', 'Z' },
	{ 0x07,	'x', 'X' },
	{ 0x08,	'c', 'C' },
	{ 0x09,	'v', 'V' },
	{ 0x0b,	'b', 'B' },
	{ 0x2d,	'n', 'N' },
	{ 0x2e,	'm', 'M' },
	{ 0x2b,	',', '<' },
	{ 0x2f,	'.', '>' },
	{ 0x2c,	'/', '?' },
	{ 0x3e,	XK_Up, 0 },
	{ 0x53,	XK_KP_1, XK_KP_End },
	{ 0x54,	XK_KP_2, XK_KP_Down },
	{ 0x55,	XK_KP_3, XK_KP_Page_Down },

	{ 0x36,	XK_Control_L, XK_Control_R },
	{ 0x3a,	XK_Print, XK_Sys_Req },		/* Option */
	{ 0x37,	XK_Scroll_Lock, 0 },		/* Command */
	{ 0x31,	' ', 0 },
	{ 0x3b,	XK_Left, 0 },
	{ 0x3d,	XK_Down, 0 },
	{ 0x3c,	XK_Right, 0 },
	{ 0x52,	XK_KP_0, XK_KP_Insert },
	{ 0x41,	XK_KP_Decimal, XK_KP_Separator },
	{ 0x4c,	XK_KP_Enter, 0 },
	{ -1, -1, -1 }
};

int
main(int argc, char **argv)
{
	int	ret, mdepth;

	ret = parse_argv(argc, argv, 1);
	if(ret) {
		printf("kegsmain ret: %d, stopping\n", ret);
		exit(1);
	}

	mdepth = x_video_get_mdepth();

	ret = kegs_init(mdepth, g_x_max_width, g_x_max_height, 0);
	printf("kegs_init done\n");
	if(ret) {
		printf("kegs_init ret: %d, stopping\n", ret);
		exit(1);
	}
	x_video_init();

	// This is the main loop of KEGS, when this exits, KEGS exits
	//  run_16ms() does one video frame worth of instructions and video
	//  updates: 17030 1MHz clock cycles.
	while(1) {
		ret = run_16ms();
		if(ret != 0) {
			printf("run_16ms returned: %d\n", ret);
			break;
		}
		x_input_events();
		x_update_display(&g_mainwin_info);
		x_update_display(&g_debugwin_info);
	}
	xdriver_end();
	exit(0);
}

int
my_error_handler(Display *display, XErrorEvent *ev)
{
	char msg[1024];
	XGetErrorText(display, ev->error_code, msg, 1000);
	printf("X Error code %s\n", msg);
	fflush(stdout);

	return 0;
}

void
xdriver_end()
{
	printf("xdriver_end\n");
	if(g_display) {
		x_auto_repeat_on(1);
		XFlush(g_display);
	}
}

void
x_try_xset_r()
{
	/* attempt "xset r" */
	(void)!system("xset r");
	xdriver_end();
	exit(5);
}

void
x_badpipe(int signum)
{
	/* restore normal sigpipe handling */
	signal(SIGPIPE, SIG_DFL);

	x_try_xset_r();
}

int
kegs_x_io_error_handler(Display *display)
{
	printf("kegs_x_io_error_handler called (likely window closed)\n");
	g_display = 0;
	x_try_xset_r();
	return 0;
}

int
x_video_get_mdepth()
{
	XWindowAttributes get_attr;
	int	depth, len, ret, force_depth;
	int	i;

	printf("Preparing X Windows graphics system\n");
	ret = 0;

	signal(SIGPIPE, x_badpipe);
	signal(SIGPIPE, x_badpipe);

#if 0
	printf("Setting _Xdebug = 1, makes X synchronous\n");
	_Xdebug = 1;
#endif

	g_display = XOpenDisplay(NULL);
	if(g_display == NULL) {
		fprintf(stderr, "Can't open display\n");
		exit(1);
	}

	vid_printf("Just opened display = %p\n", g_display);
	fflush(stdout);

	g_screen_num = DefaultScreen(g_display);

	get_attr.width = 0;
	get_attr.height = 0;
	ret = XGetWindowAttributes(g_display, DefaultRootWindow(g_display),
			&get_attr);
	printf("XGetWindowAttributes ret: %d\n", ret);
	g_x_max_width = get_attr.width;
	g_x_max_height = get_attr.height;
	printf("get_attr.width:%d, height:%d\n", g_x_max_width, g_x_max_height);

	len = sizeof(g_depth_attempt_list)/sizeof(int);
	force_depth = sim_get_force_depth();
	if(force_depth > 0) {
		/* Only use the requested user depth */
		len = 1;
		g_depth_attempt_list[0] = force_depth;
	}

	for(i = 0; i < len; i++) {
		depth = g_depth_attempt_list[i];

		g_x_screen_mdepth = x_try_find_visual(depth, g_screen_num);
		if(g_x_screen_mdepth != 0) {
			break;
		}
	}
	if(g_x_screen_mdepth == 0) {
		fprintf(stderr, "Couldn't find any visuals at any depth!\n");
		exit(2);
	}

	return g_x_screen_mdepth;
}

int
x_try_find_visual(int depth, int screen_num)
{
	XVisualInfo *visualList;
	XVisualInfo *v_chosen;
	XVisualInfo vTemplate;
	int	visualsMatched, visual_chosen, mdepth;
	int	i;

	vTemplate.screen = screen_num;
	vTemplate.depth = depth;

	visualList = XGetVisualInfo(g_display,
		(VisualScreenMask | VisualDepthMask),
		&vTemplate, &visualsMatched);

	vid_printf("visuals matched: %d\n", visualsMatched);
	if(visualsMatched == 0) {
		return 0;
	}

	visual_chosen = -1;
	for(i = 0; i < visualsMatched; i++) {
		printf("Visual %d\n", i);
		printf("	id: %08x, screen: %d, depth: %d, class: %d\n",
			(word32)visualList[i].visualid,
			visualList[i].screen,
			visualList[i].depth,
			visualList[i].class);
		printf("	red: %08lx, green: %08lx, blue: %08lx\n",
			visualList[i].red_mask,
			visualList[i].green_mask,
			visualList[i].blue_mask);
		printf("	cmap size: %d, bits_per_rgb: %d\n",
			visualList[i].colormap_size,
			visualList[i].bits_per_rgb);
		if((depth != 8) && (visualList[i].class == TrueColor)) {
			visual_chosen = i;
			break;
		}
	}

	if(visual_chosen < 0) {
		printf("Couldn't find any good visuals at depth %d!\n",
			depth);
		return 0;
	}

	printf("Chose visual: %d\n", visual_chosen);

	v_chosen = &(visualList[visual_chosen]);
	video_set_red_mask((word32)v_chosen->red_mask);
	video_set_green_mask((word32)v_chosen->green_mask);
	video_set_blue_mask((word32)v_chosen->blue_mask);

	video_set_palette();	// Uses above masks to initialize palettes

	g_x_screen_depth = depth;
	mdepth = depth;
	if(depth > 8) {
		mdepth = 16;
	}
	if(depth > 16) {
		mdepth = 32;
	}

	// XFree(visualList); -- Cannot free, still using g_vis...

	g_vis = v_chosen->visual;

	return mdepth;
}

void
x_video_init()
{
	int	tmp_array[0x80];
	Atom	target;
	char	cursor_data;
	int	keycode, num_targets, max_targets, num;
	int	i;

	printf("Opening X Window now\n");

	g_num_a2_keycodes = 0;
	for(i = 0; i <= 0x7f; i++) {
		tmp_array[i] = 0;
	}
	for(i = 0; i < 0x7f; i++) {
		keycode = g_x_a2_key_to_xsym[i][0];
		if(keycode < 0) {
			g_num_a2_keycodes = i;
			break;
		} else if(keycode > 0x7f) {
			printf("a2_key_to_xsym[%d] = %02x!\n", i, keycode);
				exit(2);
		} else {
			if(tmp_array[keycode]) {
				printf("a2_key_to_x[%d] = %02x used by %d\n",
					i, keycode, tmp_array[keycode] - 1);
			}
			tmp_array[keycode] = i + 1;
		}
	}


	g_default_colormap = XDefaultColormap(g_display, g_screen_num);
	if(!g_default_colormap) {
		printf("g_default_colormap == 0!\n");
		exit(4);
	}

	/* and define cursor */
	cursor_data = 0;
	g_cursor_shape = XCreatePixmapFromBitmapData(g_display,
		RootWindow(g_display,g_screen_num), &cursor_data, 1, 1, 1, 0,1);
	g_cursor_mask = XCreatePixmapFromBitmapData(g_display,
		RootWindow(g_display,g_screen_num), &cursor_data, 1, 1, 1, 0,1);

	g_cursor = XCreatePixmapCursor(g_display, g_cursor_shape,
			g_cursor_mask, &g_xcolor_black, &g_xcolor_white, 0, 0);

	XFreePixmap(g_display, g_cursor_shape);
	XFreePixmap(g_display, g_cursor_mask);

	XFlush(g_display);
	g_x_atom_targets = XInternAtom(g_display, "TARGETS", 0);
	num = sizeof(g_x_selection_strings)/sizeof(g_x_selection_strings[0]);
	g_x_targets_array[0] = XA_STRING;
	num_targets = 1;
	for(i = 0; i < num; i++) {
		target = XInternAtom(g_display, g_x_selection_strings[i], 0);
		if(target != None) {
			g_x_targets_array[num_targets++] = target;
		}
	}
	g_x_num_targets = num_targets;
	max_targets = sizeof(g_x_targets_array)/sizeof(g_x_targets_array[0]);
	if(num_targets > max_targets) {
		printf("Overflowed g_x_targets_array: %d out of %d\n",
						num_targets, max_targets);
		exit(-1);
	}

	x_init_window(&g_mainwin_info, video_get_kimage(0), "KEGS");
	x_init_window(&g_debugwin_info, video_get_kimage(1), "KEGS Debugger");

	x_create_window(&g_mainwin_info);

	vid_printf("Set error handler to my_x_handler\n");
	XSetIOErrorHandler(kegs_x_io_error_handler);
}

void
x_init_window(Window_info *win_info_ptr, Kimage *kimage_ptr, char *name_str)
{
	int	width, height, x_use_shmem, ret;

	height = video_get_x_height(kimage_ptr);
	width = video_get_x_width(kimage_ptr);

	win_info_ptr->seginfo = 0;
	win_info_ptr->xim = 0;
	win_info_ptr->kimage_ptr = kimage_ptr;
	win_info_ptr->name_str = name_str;
	win_info_ptr->x_win = 0;
	win_info_ptr->x_winGC = 0;
	win_info_ptr->delete_atom = 0;
	win_info_ptr->active = 0;
	win_info_ptr->x_use_shmem = 0;
	win_info_ptr->width_req = width;
	win_info_ptr->pixels_per_line = width;
	win_info_ptr->main_height = height;
	win_info_ptr->full_min_width = width;
	win_info_ptr->full_min_height = height;
	vid_printf("init win %p (main:%p) width:%d, height:%d\n", win_info_ptr,
		&g_mainwin_info, width, height);

	x_use_shmem = 0;
		// Allow cmd-line args to force shmem off
/* Check for XShm */
#ifdef X_SHARED_MEM
	if(sim_get_use_shmem()) {
		ret = XShmQueryExtension(g_display);
		if(ret == 0) {
			printf("XShmQueryExt ret: %d, not using shared mem\n",
									ret);
		} else {
			vid_printf("Will use shared memory for X\n");
			x_use_shmem = 1;
		}
	}
#endif
	win_info_ptr->x_use_shmem = x_use_shmem;

	video_update_scale(kimage_ptr, win_info_ptr->width_req,
						win_info_ptr->main_height, 1);
}

void
x_create_window(Window_info *win_info_ptr)
{
	XGCValues new_gc;
	XSetWindowAttributes win_attr;
	Window	x_win;
	GC	x_winGC;
	word32	create_win_list;
	int	width, height, x_xpos, x_ypos;

	win_attr.event_mask = X_A2_WIN_EVENT_LIST;
	win_attr.colormap = g_default_colormap;
	win_attr.backing_store = WhenMapped;
	win_attr.border_pixel = 1;
	win_attr.background_pixel = 0;
	if(g_x_warp_pointer) {
		win_attr.cursor = g_cursor;
	} else {
		win_attr.cursor = None;
	}

	vid_printf("About to win, depth: %d\n", g_x_screen_depth);
	fflush(stdout);

	create_win_list = CWEventMask | CWBackingStore | CWCursor;
	create_win_list |= CWColormap | CWBorderPixel | CWBackPixel;

	x_xpos = video_get_x_xpos(win_info_ptr->kimage_ptr);
	x_ypos = video_get_x_ypos(win_info_ptr->kimage_ptr);
	width = win_info_ptr->width_req;
	height = win_info_ptr->main_height;

	x_win = XCreateWindow(g_display, RootWindow(g_display, g_screen_num),
		x_xpos, x_ypos, width, height, 0, g_x_screen_depth,
		InputOutput, g_vis, create_win_list, &win_attr);
	vid_printf("x_win = %d, width:%d, height:%d\n", (int)x_win, width,
									height);

	XFlush(g_display);

	win_info_ptr->x_win = x_win;
	win_info_ptr->active = 0;
	video_set_active(win_info_ptr->kimage_ptr, 1);

	x_allocate_window_data(win_info_ptr);

	if(!win_info_ptr->x_use_shmem) {
		printf("Not using shared memory, setting skip_amt = 2, "
							"g_audio_enable=0\n");
		video_set_redraw_skip_amt(2);
		g_audio_enable = 0;
	}

	x_set_size_hints(win_info_ptr);

	XMapRaised(g_display, x_win);

	if(win_info_ptr != &g_mainwin_info) {
		// Debugger window
		win_info_ptr->delete_atom = XInternAtom(g_display,
						"WM_DELETE_WINDOW", False);
		XSetWMProtocols(g_display, x_win, &(win_info_ptr->delete_atom),
									1);
	}

	XSync(g_display, False);

	x_winGC = XCreateGC(g_display, x_win, 0, (XGCValues *) 0);
	win_info_ptr->x_winGC = x_winGC;
	win_info_ptr->active = 1;

	new_gc.fill_style = FillSolid;
	XChangeGC(g_display, x_winGC, GCFillStyle, &new_gc);

	/* XSync(g_display, False); */

	XFlush(g_display);
	fflush(stdout);
}

int g_xshm_error = 0;

int
xhandle_shm_error(Display *display, XErrorEvent *event)
{
	g_xshm_error = 1;
	return 0;
}

void
x_allocate_window_data(Window_info *win_info_ptr)
{
	int	width, height;

	width = g_x_max_width;
	height = g_x_max_height;
	if(win_info_ptr->x_use_shmem) {
		win_info_ptr->x_use_shmem = 0;		// Default to no shmem
		get_shm(win_info_ptr, width, height);
	}
	if(!win_info_ptr->x_use_shmem) {
		get_ximage(win_info_ptr, width, height);
	}
}

void
get_shm(Window_info *win_info_ptr, int width, int height)
{
#ifdef X_SHARED_MEM
	XShmSegmentInfo *seginfo;
	XImage *xim;
	int	(*old_x_handler)(Display *, XErrorEvent *);
	int	depth, size;

	depth = g_x_screen_depth;		// 24, actual bits per pixel

	seginfo = (XShmSegmentInfo *)malloc(sizeof(XShmSegmentInfo));
	xim = XShmCreateImage(g_display, g_vis, depth, ZPixmap,
		(char *)0, seginfo, width, height);

	/* check mdepth, which should be 32 */
	if(xim->bits_per_pixel != g_x_screen_mdepth) {
		printf("get_shm bits_per_pix: %d != %d\n",
				xim->bits_per_pixel, g_x_screen_mdepth);
	}
	vid_printf("xim: %p, DO_VERBOSE:%d, Verbose:%d, VERBOSE_VIDEO:%d\n",
		xim, DO_VERBOSE, Verbose, VERBOSE_VIDEO);

	vid_printf("xim: %p\n", xim);
	win_info_ptr->seginfo = seginfo;
	if(xim == 0) {
		return;
	}
	vid_printf("bytes_per_line:%d, height:%d\n", xim->bytes_per_line,
								xim->height);
	size = xim->bytes_per_line * xim->height;
	vid_printf("size: %d\n", size);

	/* It worked, we got it */
	seginfo->shmid = shmget(IPC_PRIVATE, size, IPC_CREAT | 0777);
	vid_printf("seginfo->shmid = %d, errno:%d, %s\n", seginfo->shmid,
		errno, strerror(errno));
	if(seginfo->shmid < 0) {
		XDestroyImage(xim);
		return;
	}

	/* Still working */
	seginfo->shmaddr = (char *)shmat(seginfo->shmid, 0, 0);
	vid_printf("seginfo->shmaddr: %p\n", seginfo->shmaddr);
	if(seginfo->shmaddr == ((char *) -1)) {
		XDestroyImage(xim);
		return;
	}

	/* Still working */
	xim->data = seginfo->shmaddr;
	seginfo->readOnly = False;
	vid_printf("xim->data is %p, size:%08x\n", xim->data, size);

	/* XShmAttach will trigger X error if server is remote, so catch it */
	g_xshm_error = 0;
	old_x_handler = XSetErrorHandler(xhandle_shm_error);

	XShmAttach(g_display, seginfo);
	XSync(g_display, False);


	vid_printf("about to RMID the shmid\n");
	shmctl(seginfo->shmid, IPC_RMID, 0);

	XFlush(g_display);
	XSetErrorHandler(old_x_handler);

	if(g_xshm_error) {
		XDestroyImage(xim);
		/* We could release the shared mem segment, but by doing the */
		/* RMID, it will go away when we die now, so just leave it */
		printf("Not using shared memory\n");
		return;
	}

	width = xim->bytes_per_line;
	win_info_ptr->pixels_per_line = width / 4;
	win_info_ptr->xim = xim;
	win_info_ptr->x_use_shmem = 1;

	vid_printf("Sharing memory. xim: %p, xim->data: %p, width:%d\n", xim,
		xim->data, win_info_ptr->pixels_per_line);
#endif	/* X_SHARED_MEM */
}

void
get_ximage(Window_info *win_info_ptr, int width, int height)
{
	XImage	*xim;
	byte	*ptr;
	int	depth, mdepth, size;

	depth = g_x_screen_depth;
	mdepth = g_x_screen_mdepth;

	size = (width * height * mdepth) >> 3;
	printf("Get_ximage, w:%d, h:%d, mdepth:%d, size:%08x\n", width,
						height, mdepth, size);
	ptr = (byte *)malloc(size);

	vid_printf("ptr: %p\n", ptr);

	if(ptr == 0) {
		printf("malloc for data failed, mdepth: %d\n", mdepth);
		exit(2);
	}

	win_info_ptr->pixels_per_line = width;

	xim = XCreateImage(g_display, g_vis, depth, ZPixmap, 0,
		(char *)ptr, width, height, 8, 0);

#ifdef KEGS_BIG_ENDIAN
	xim->byte_order = MSBFirst;
#else
	xim->byte_order = LSBFirst;
#endif
	_XInitImageFuncPtrs(xim);	/* adjust to new byte order */

	/* check mdepth! */
	if(xim->bits_per_pixel != mdepth) {
		printf("shm_ximage bits_per_pix: %d != %d\n",
					xim->bits_per_pixel, mdepth);
	}

	vid_printf("xim: %p\n", xim);

	win_info_ptr->xim = xim;
	win_info_ptr->x_use_shmem = 0;

	return;
}

void
x_set_size_hints(Window_info *win_info_ptr)
{
	XSizeHints my_winSizeHints;
	XClassHint my_winClassHint;
	XTextProperty my_winText;
	Kimage	*kimage_ptr;
	int	width, height, a2_width, a2_height;

	width = win_info_ptr->width_req;
	height = win_info_ptr->main_height;
	kimage_ptr = win_info_ptr->kimage_ptr;
	video_update_scale(kimage_ptr, width, height, 0);

	a2_width = video_get_a2_width(kimage_ptr);
	a2_height = video_get_a2_height(kimage_ptr);

	XStringListToTextProperty(&(win_info_ptr->name_str), 1, &my_winText);

	my_winSizeHints.flags = PSize | PMinSize | PMaxSize | PAspect;
	my_winSizeHints.width = width;
	my_winSizeHints.height = height;
	my_winSizeHints.min_width = a2_width;
	my_winSizeHints.min_height = a2_height;
	my_winSizeHints.max_width = g_x_max_width;
	my_winSizeHints.max_height = g_x_max_height;
	my_winSizeHints.min_aspect.x = a2_width - 1;
	my_winSizeHints.min_aspect.y = a2_height;
	my_winSizeHints.max_aspect.x = a2_width + 1;
	my_winSizeHints.max_aspect.y = a2_height;
	my_winClassHint.res_name = win_info_ptr->name_str;
	my_winClassHint.res_class = win_info_ptr->name_str;

	XSetWMProperties(g_display, win_info_ptr->x_win, &my_winText,
		&my_winText, 0, 0, &my_winSizeHints, 0, &my_winClassHint);
	// printf("Did XSetWMProperties w:%d h:%d\n", width, height);
}

void
x_resize_window(Window_info *win_info_ptr)
{
	Kimage	*kimage_ptr;
	int	x_width, x_height, ret;

	kimage_ptr = win_info_ptr->kimage_ptr;
	x_width = video_get_x_width(kimage_ptr);
	x_height = video_get_x_height(kimage_ptr);

	win_info_ptr->main_height = MY_MIN(x_height, g_x_max_height);
	win_info_ptr->width_req = MY_MIN(x_width, g_x_max_width);

	ret = XResizeWindow(g_display, win_info_ptr->x_win, x_width, x_height);
	if(0) {
		printf("XResizeWindow ret:%d, w:%d, h:%d\n", ret, x_width,
								x_height);
	}
}

void
x_update_display(Window_info *win_info_ptr)
{
	Change_rect rect;
	int	did_copy, valid, x_active, a2_active;
	int	i;

	// Update active state
	a2_active = video_get_active(win_info_ptr->kimage_ptr);
	x_active = win_info_ptr->active;
	if(x_active && !a2_active) {
		// We need to unmap this window
		XUnmapWindow(g_display, win_info_ptr->x_win);
		x_active = 0;
		win_info_ptr->active = x_active;
	}
	if(!x_active && a2_active) {
		// We need to map this window (and maybe create it)
		if(win_info_ptr->xim == 0) {
			x_create_window(win_info_ptr);
		}
		XMapWindow(g_display, win_info_ptr->x_win);
		x_active = 1;
		win_info_ptr->active = x_active;
	}
	if(x_active == 0) {
		return;
	}

	if(video_change_aspect_needed(win_info_ptr->kimage_ptr,
			win_info_ptr->width_req, win_info_ptr->main_height)) {
		x_resize_window(win_info_ptr);
	}
	did_copy = 0;
	for(i = 0; i < MAX_CHANGE_RECTS; i++) {
		valid = video_out_data(win_info_ptr->xim->data,
				win_info_ptr->kimage_ptr,
				win_info_ptr->pixels_per_line, &rect, i);
		if(!valid) {
			break;
		}
#if 0
		if(win_info_ptr == &g_debugwin_info) {
			printf("  i:%d valid:%d, w:%d h:%d\n", i, valid,
					rect.width, rect.height);
		}
#endif
		did_copy = 1;
#ifdef X_SHARED_MEM
		if(win_info_ptr->x_use_shmem) {
			XShmPutImage(g_display, win_info_ptr->x_win,
				win_info_ptr->x_winGC,
				win_info_ptr->xim, rect.x, rect.y,
				rect.x, rect.y,
				rect.width, rect.height, False);
		}
#endif
		if(!win_info_ptr->x_use_shmem) {
			XPutImage(g_display, win_info_ptr->x_win,
				win_info_ptr->x_winGC,
				win_info_ptr->xim, rect.x, rect.y,
				rect.x, rect.y,
				rect.width, rect.height);
		}
	}

	if(did_copy) {
		XFlush(g_display);
	}
}

Window_info *
x_find_xwin(Window in_win)
{
	if(g_mainwin_info.kimage_ptr) {
		if(g_mainwin_info.x_win == in_win) {
			return &g_mainwin_info;
		}
	}
	if(g_debugwin_info.kimage_ptr) {
		if(g_debugwin_info.x_win == in_win) {
			return &g_debugwin_info;
		}
	}
	printf("in_win:%d not found\n", (int)in_win);
	exit(1);
}

#define KEYBUFLEN	128

int g_num_check_input_calls = 0;
int g_check_input_flush_rate = 2;

// https://stackoverflow.com/questions/72236711/trouble-with-xsetselectionowner
//  See answer from "n.m" on May 17th.
void
x_send_copy_data(Window_info *win_info_ptr)
{
	printf("x_send_copy_data!\n");
	XSetSelectionOwner(g_display, XA_PRIMARY, win_info_ptr->x_win,
							CurrentTime);
	(void)cfg_text_screen_str();
}

void
x_handle_copy(XSelectionRequestEvent *req_ev_ptr)
{
	byte	*bptr;
	int	ret;

	bptr = (byte *)cfg_get_current_copy_selection();
	ret = XChangeProperty(g_display, req_ev_ptr->requestor,
		req_ev_ptr->property, req_ev_ptr->target, 8,
		PropModeReplace, bptr, strlen((char *)bptr));
		// req_ev_ptr->target is either XA_STRING, or equivalent
	if(0) {
		// Seems to return 1, BadRequest always, but it works
		printf("XChangeProperty ret: %d\n", ret);
	}
}

void
x_handle_targets(XSelectionRequestEvent *req_ev_ptr)
{
	int	ret;

	// Tell the other client what targets we can supply from
	//  g_x_targets_array[]
	ret = XChangeProperty(g_display, req_ev_ptr->requestor,
		req_ev_ptr->property, XA_ATOM, 32,
		PropModeReplace, (byte *)&g_x_targets_array[0],
		g_x_num_targets);
	if(0) {
		// Seems to return 1, BadRequest always, but it works
		printf("XChangeProperty TARGETS ret: %d\n", ret);
	}
}

void
x_request_paste_data(Window_info *win_info_ptr)
{
	printf("Pasting selection\n");
	// printf("Calling XConvertSelection\n");
	XConvertSelection(g_display, XA_PRIMARY, XA_STRING, XA_STRING,
		win_info_ptr->x_win, CurrentTime);
	// This will cause a SelectionNotify event, and we get the data
	//  by using XGetWindowProperty on our own window.  This will eventually
	//  call x_handle_paste().
}

void
x_handle_paste(Window w, Atom property)
{
	byte	*bptr;
	Atom	sel_type;
	unsigned long sel_nitems, sel_bytes_after;
	long	sel_length;
	int	sel_format, ret, ret2, c;
	int	i;

	sel_length = 16384;
	sel_type = 0;
	sel_format = 0;
	sel_nitems = 0;
	sel_bytes_after = 0;
	bptr = 0;
	ret = XGetWindowProperty(g_display, w, property, 0, sel_length, 1,
		AnyPropertyType, &sel_type, &sel_format, &sel_nitems,
		&sel_bytes_after, &bptr);
#if 0
	printf("XGetWindowProperty ret:%d, sel_type:%ld, sel_format:%d, "
		"sel_nitems:%ld, sel_bytes_after:%ld, bptr:%p\n",
		ret, sel_type, sel_format, sel_nitems, sel_bytes_after, bptr);
#endif
	if(bptr && (sel_type == property) && sel_nitems && (sel_format == 8)) {
		//printf("bptr: %s\n", (char *)bptr);
		for(i = 0; i < sel_nitems; i++) {
			c = bptr[i];
			ret2 = adb_paste_add_buf(c);
			if(ret2) {
				printf("Paste buffer full!\n");
				break;
			}
		}
	}
	if(ret == 0) {
		XFree(bptr);
	}
}

int
x_update_mouse(Window_info *win_info_ptr, int raw_x, int raw_y,
				int button_states, int buttons_valid)
{
	Kimage	*kimage_ptr;
	int	x, y, ret;

	if((button_states & buttons_valid & 2) == 2) {
		// Middle button: Paste request
		x_request_paste_data(win_info_ptr);
		button_states = button_states & (~2);
	}
	kimage_ptr = win_info_ptr->kimage_ptr;
	x = video_scale_mouse_x(kimage_ptr, raw_x, 0);
	y = video_scale_mouse_y(kimage_ptr, raw_y, 0);

	if(g_x_warp_pointer && (raw_x == g_x_warp_x) &&
			(raw_y == g_x_warp_y) && (buttons_valid == 0) ) {
		/* tell adb routs to recenter but ignore this motion */
		adb_update_mouse(kimage_ptr, x, y, 0, -1);
		return 0;
	}
	ret = adb_update_mouse(kimage_ptr, x, y, button_states,
							buttons_valid & 7);
	return ret;
}

void
x_input_events()
{
	XEvent	ev, response;
	XSelectionRequestEvent *req_ev_ptr;
	Window_info *win_info_ptr;
	char	*str;
	int	len, motion, key_or_mouse, refresh_needed, buttons, hide, warp;
	int	width, height, resp_property, match, x_xpos, x_ypos;
	int	i;

	str = 0;
	if(str) {
		// Use str
	}
	if(adb_get_copy_requested()) {
		x_send_copy_data(&g_mainwin_info);
	}
	g_num_check_input_calls--;
	if(g_num_check_input_calls < 0) {
		len = XPending(g_display);
		g_num_check_input_calls = g_check_input_flush_rate;
	} else {
		len = QLength(g_display);
	}

	motion = 0;
	win_info_ptr = 0;
	refresh_needed = 0;
	key_or_mouse = 0;
	while(len > 0) {
		XNextEvent(g_display, &ev);
		len--;
		if(len == 0) {
			/* Xaccel on linux only buffers one X event */
			/*  must look for more now */
			len = XPending(g_display);
		}
		switch(ev.type) {
		case FocusIn:
		case FocusOut:
			win_info_ptr = x_find_xwin(ev.xfocus.window);
			if(ev.xfocus.type == FocusOut) {
				/* Allow keyrepeat again! */
				vid_printf("Left window, auto repeat on\n");
				x_auto_repeat_on(0);
			} else if(ev.xfocus.type == FocusIn &&
					(win_info_ptr == &g_mainwin_info)) {
				/* Allow keyrepeat again! */
				vid_printf("Enter window, auto repeat off\n");
				x_auto_repeat_off(0);
			}
			break;
		case EnterNotify:
		case LeaveNotify:
			/* These events are disabled now */
			printf("Enter/Leave event for winow %08x, sub: %08x\n",
				(word32)ev.xcrossing.window,
				(word32)ev.xcrossing.subwindow);
			printf("Enter/L mode: %08x, detail: %08x, type:%02x\n",
				ev.xcrossing.mode, ev.xcrossing.detail,
				ev.xcrossing.type);
			break;
		case ButtonPress:
			win_info_ptr = x_find_xwin(ev.xbutton.window);
			vid_printf("Got button press of button %d!\n",
						ev.xbutton.button);
			buttons = (1 << ev.xbutton.button) >> 1;
			motion |= x_update_mouse(win_info_ptr, ev.xbutton.x,
				ev.xbutton.y, buttons, buttons & 7);
			key_or_mouse = 1;
			break;
		case ButtonRelease:
			win_info_ptr = x_find_xwin(ev.xbutton.window);
			buttons = (1 << ev.xbutton.button) >> 1;
			motion |= x_update_mouse(win_info_ptr, ev.xbutton.x,
						ev.xbutton.y, 0, buttons & 7);
			key_or_mouse = 1;
			break;
		case MotionNotify:
			win_info_ptr = x_find_xwin(ev.xmotion.window);
			motion |= x_update_mouse(win_info_ptr, ev.xmotion.x,
						ev.xmotion.y, 0, 0);
			key_or_mouse = 1;
			break;
		case Expose:
			win_info_ptr = x_find_xwin(ev.xexpose.window);
			refresh_needed = -1;
			//printf("Got an Expose event\n");
			break;
		case NoExpose:
			/* do nothing */
			break;
		case KeyPress:
		case KeyRelease:
			x_handle_keysym(&ev);
			key_or_mouse = 1;
			break;
		case KeymapNotify:
			break;
		case DestroyNotify:
			win_info_ptr = x_find_xwin(ev.xdestroywindow.window);
			video_set_active(win_info_ptr->kimage_ptr, 0);
			win_info_ptr->active = 0;
			printf("Destroy %s\n", win_info_ptr->name_str);
			if(win_info_ptr == &g_mainwin_info) {
				x_try_xset_r();		// Mainwin: quit BURST
			}
			break;
		case ReparentNotify:
		case UnmapNotify:
		case MapNotify:
			break;
		case ClientMessage:
			win_info_ptr = x_find_xwin(ev.xclient.window);
			if(ev.xclient.data.l[0] == win_info_ptr->delete_atom) {
				// This is a WM_DELETE_WINDOW event
				// Just unmap the window
				win_info_ptr->kimage_ptr->active = 0;
			} else {
				printf("unknown ClientMessage\n");
			}
			break;
		case ConfigureNotify:
			win_info_ptr = x_find_xwin(ev.xconfigure.window);
			width = ev.xconfigure.width;
			height = ev.xconfigure.height;
#if 0
			printf("ConfigureNotify, width:%d, height:%d\n",
				width, height);
#endif
			video_update_scale(win_info_ptr->kimage_ptr, width,
								height, 0);
			x_xpos = ev.xconfigure.x;
			x_ypos = ev.xconfigure.y;
			video_update_xpos_ypos(win_info_ptr->kimage_ptr,
							x_xpos, x_ypos);
			break;
		case SelectionRequest:
			//printf("SelectionRequest received\n");
			req_ev_ptr = &(ev.xselectionrequest);
			// This is part of the dance for copy: Another client
			//  is asking us what format we can supply (TARGETS),
			//  or is doing to tell us one at a time what types
			//  it would like.
#if 0
			printf("req:%ld, property:%ld, target:%ld, "
				"selection:%ld\n", req_ev_ptr->requestor,
				req_ev_ptr->property, req_ev_ptr->target,
				req_ev_ptr->selection);
			str = XGetAtomName(g_display, req_ev_ptr->target);
			printf("XAtom target str: %s\n", str);
			XFree(str);
			str = XGetAtomName(g_display, req_ev_ptr->property);
			printf("XAtom property str: %s\n", str);
			XFree(str);
#endif
			resp_property = None;
			match = 0;
			for(i = 0; i < g_x_num_targets; i++) {
				if(req_ev_ptr->target == g_x_targets_array[i]) {
					match = 1;
					break;
				}
			}
			if(match) {
				x_handle_copy(req_ev_ptr);
				resp_property = req_ev_ptr->property;
			} else if(req_ev_ptr->target == g_x_atom_targets) {
				// Some other agent is asking us "TARGETS",
				//  so send our list of targets
				x_handle_targets(req_ev_ptr);
			}
			// But no matter what the request target was, respond
			//  so it will send an eventual request for XA_STRING
			response.xselection.type = SelectionNotify;
			response.xselection.display = req_ev_ptr->display;
			response.xselection.requestor = req_ev_ptr->requestor;
			response.xselection.selection = req_ev_ptr->selection;
			response.xselection.target = req_ev_ptr->target;
			response.xselection.property = resp_property;
			response.xselection.time = req_ev_ptr->time;
			XSendEvent(g_display, req_ev_ptr->requestor, 0, 0,
								&response);
			XFlush(g_display);	// Speed up getting more resp
			break;
		case SelectionNotify:
			// We get this event after we requested the PRIMARY
			//  selection, so paste this to adb().
			vid_printf("SelectionNotify received\n");
			vid_printf("req:%ld, selection:%ld, target:%ld, "
				"property:%ld\n", ev.xselection.requestor,
				ev.xselection.selection,
				ev.xselection.target,
				ev.xselection.property);
			if(ev.xselection.property == None) {
				printf("No selection\n");
				break;
			}
			x_handle_paste(ev.xselection.requestor,
							ev.xselection.property);

			break;
		default:
			printf("X event 0x%08x is unknown!\n",
				ev.type);
			break;
		}
	}

	if(key_or_mouse && (win_info_ptr == &g_mainwin_info)) {
		hide = adb_get_hide_warp_info(win_info_ptr->kimage_ptr, &warp);
		if(warp != g_x_warp_pointer) {
			motion = 1;
		}
		g_x_warp_pointer = warp;
		if(g_x_hide_pointer != hide) {
			x_hide_pointer(&g_mainwin_info, hide);
		}
		g_x_hide_pointer = hide;
	}

	if(motion && g_x_warp_pointer && (win_info_ptr == &g_mainwin_info)) {
		// Calculate where to warp to
		g_x_warp_x = video_unscale_mouse_x(win_info_ptr->kimage_ptr,
			BASE_MARGIN_LEFT + (BASE_WINDOW_WIDTH/2), 0);
		g_x_warp_y = video_unscale_mouse_y(win_info_ptr->kimage_ptr,
			BASE_MARGIN_TOP + (A2_WINDOW_HEIGHT/2), 0);

		XWarpPointer(g_display, None, win_info_ptr->x_win, 0, 0, 0, 0,
			g_x_warp_x, g_x_warp_y);
	}

	if(refresh_needed && win_info_ptr) {
		//printf("...at end, refresh_needed:%d\n", refresh_needed);
		video_set_x_refresh_needed(win_info_ptr->kimage_ptr, 1);
	}

}

void
x_hide_pointer(Window_info *win_info_ptr, int do_hide)
{
	if(do_hide) {				// invisible
		XDefineCursor(g_display, win_info_ptr->x_win, g_cursor);
	} else {				// Default cursor
		XDefineCursor(g_display, win_info_ptr->x_win, None);
	}
}

void
x_handle_keysym(XEvent *xev_in)
{
	Window_info *win_info_ptr;
	KeySym	keysym;
	word32	state;
	int	keycode, a2code, type, is_up;

	win_info_ptr = x_find_xwin(xev_in->xkey.window);

	keycode = xev_in->xkey.keycode;
	type = xev_in->xkey.type;

	keysym = XLookupKeysym(&(xev_in->xkey), 0);

	state = xev_in->xkey.state;

	vid_printf("keycode: %d, type: %d, state:%d, sym: %08x\n",
		keycode, type, state, (word32)keysym);

	x_update_modifier_state(win_info_ptr, state);

	is_up = 0;
	if(type == KeyRelease) {
		is_up = 1;
	}

	/* first, do conversions */
	switch(keysym) {
	case XK_Alt_L:
	case XK_Meta_R:				// Windows key on right side
	case XK_Super_R:
	case XK_Mode_switch:
	case XK_Cancel:
		keysym = XK_Print;		/* option */
		break;
	case XK_Meta_L:				// Windows key on left side
	case XK_Alt_R:
	case XK_Super_L:
	case XK_Menu:
		keysym = XK_Scroll_Lock;	/* cmd */
		break;
	case XK_F5:
		break;
	case XK_F10:
		if(!is_up) {
			g_video_scale_algorithm++;
			if(g_video_scale_algorithm >= 3) {
				g_video_scale_algorithm = 0;
			}
			printf("g_video_scale_algorithm = %d\n",
						g_video_scale_algorithm);
			video_update_scale(win_info_ptr->kimage_ptr,
					win_info_ptr->width_req,
					win_info_ptr->main_height, 1);
		}
		break;
	case 0x1000003:
		if(keycode == 0x3c) {
			/* enter key on Mac OS X laptop--make it option */
			keysym = XK_Print;
		}
		break;
	case NoSymbol:
		switch(keycode) {
		/* 94-95 are for my PC101 kbd + windows keys on HPUX */
		case 0x0095:
			/* left windows key = option */
			keysym = XK_Print;
			break;
		case 0x0096:
		case 0x0094:
			/* right windows key = cmd */
			keysym = XK_Scroll_Lock;
			break;
		/* 0072 is for cra@WPI.EDU who says it's Break under XFree86 */
		case 0x0072:
		/* 006e is break according to mic@research.nj.nec.com */
		case 0x006e:
			keysym = XK_Break;
			break;

		/* 0x0042, 0x0046, and 0x0048 are the windows keys according */
		/*  to Geoff Weiss on Solaris x86 */
		case 0x0042:
		case 0x0046:
			/* flying windows == open apple */
			keysym = XK_Scroll_Lock;
			break;
		case 0x0048:
		case 0x0076:		/* Windows menu key on Mac OS X */
			/* menu windows == option */
			keysym = XK_Print;
			break;
		}
	}

	a2code = x_keysym_to_a2code(win_info_ptr, (int)keysym, is_up);
	if(a2code >= 0) {
		adb_physical_key_update(win_info_ptr->kimage_ptr, a2code,
			0, is_up);
	} else if(a2code != -2) {
		printf("Keysym: %04x of keycode: %02x unknown\n",
			(word32)keysym, keycode);
	}
}

int
x_keysym_to_a2code(Window_info *win_info_ptr, int keysym, int is_up)
{
	int	i;

	if(keysym == 0) {
		return -1;
	}

	/* Look up Apple 2 keycode */
	for(i = g_num_a2_keycodes - 1; i >= 0; i--) {
		if((keysym == g_x_a2_key_to_xsym[i][1]) ||
					(keysym == g_x_a2_key_to_xsym[i][2])) {

			vid_printf("Found keysym:%04x = a[%d] = %04x or %04x\n",
				(int)keysym, i, g_x_a2_key_to_xsym[i][1],
				g_x_a2_key_to_xsym[i][2]);

			return g_x_a2_key_to_xsym[i][0];
		}
	}

	return -1;
}

void
x_update_modifier_state(Window_info *win_info_ptr, int state)
{
	word32	c025_val;

	c025_val = 0;
	if(state & ShiftMask) {
		c025_val |= 1;
	}
	if(state & ControlMask) {
		c025_val |= 2;
	}
	if(state & LockMask) {
		c025_val |= 4;
	}
	adb_update_c025_mask(win_info_ptr->kimage_ptr, c025_val, 7);
}

void
x_auto_repeat_on(int must)
{
	if((g_auto_repeat_on <= 0) || must) {
		g_auto_repeat_on = 1;
		XAutoRepeatOn(g_display);
		XFlush(g_display);
		adb_kbd_repeat_off();
	}
}

void
x_auto_repeat_off(int must)
{
	if((g_auto_repeat_on != 0) || must) {
		XAutoRepeatOff(g_display);
		XFlush(g_display);
		g_auto_repeat_on = 0;
		adb_kbd_repeat_off();
	}
}

void
x_full_screen(int do_full)
{
	return;
}
