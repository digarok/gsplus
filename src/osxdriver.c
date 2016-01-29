/*
 GSport - an Apple //gs Emulator
 Copyright (C) 2010 - 2012 by GSport contributors

 Based on the KEGS emulator written by and Copyright (C) 2003 Kent Dickey

 This program is free software; you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by the
 Free Software Foundation; either version 2 of the License, or (at your
 option) any later version.

 This program is distributed in the hope that it will be useful, but
 WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 for more details.

 You should have received a copy of the GNU General Public License along
 with this program; if not, write to the Free Software Foundation, Inc.,
 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/


// @todo: force refresh after screen mode change
#include "SDL.h"
#include <stdbool.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <time.h>
#include <stdlib.h>
#include <signal.h>


# include <sys/ipc.h>
# include <sys/shm.h>
# include <X11/extensions/XShm.h>


void x_push_done() { }
#define KEYBUFLEN	128
#define MAKE_2(val)	( (val << 8) + val)

int g_num_check_input_calls = 0;
int g_check_input_flush_rate = 2;

#include "defc.h"
#include "protos_xdriver.h"

int XShmQueryExtension(Display *display);

int g_use_shmem = 1;

Display *g_display = 0;
Visual	*g_vis = 0;
Window g_a2_win;
Atom    WM_DELETE_WINDOW;

Colormap g_a2_colormap = 0;
Colormap g_default_colormap = 0;
XColor g_xcolor_a2vid_array[256];
int	g_alt_left_up = 1;
int	g_alt_right_up = 1;
int	g_needs_cmap = 0;
int	g_win_status_debug = 0;			// Current visibility of status lines.
int g_win_status_debug_request = 0;	// Desired visibility of status lines.
int g_screen_mdepth = 0;
int	g_has_focus = 0;
int	g_auto_repeat_on = -1;
int	g_x_shift_control_state = 0;
int kb_shift_control_state = 0;
int g_depth_attempt_list[] = { 16, 24, 15, 8 };



extern int Verbose;
extern int g_warp_pointer;
extern int g_screen_depth;
extern int g_force_depth;
extern int _Xdebug;
extern int g_send_sound_to_file;
extern int g_quit_sim_now;
extern Kimage g_mainwin_kimage;
extern word32 g_red_mask;
extern word32 g_green_mask;
extern word32 g_blue_mask;
extern int g_red_left_shift;
extern int g_green_left_shift;
extern int g_blue_left_shift;
extern int g_red_right_shift;
extern int g_green_right_shift;
extern int g_blue_right_shift;
extern int Max_color_size;
extern int g_border_sides_refresh_needed;
extern int g_border_special_refresh_needed;
extern int g_status_refresh_needed;
extern int g_lores_colors[];
extern int g_cur_a2_stat;
extern int g_a2vid_palette;
extern int g_installed_full_superhires_colormap;
extern int g_screen_redraw_skip_amt;
extern word32 g_a2_screen_buffer_changed;
extern word32 g_full_refresh_needed;
extern char *g_status_ptrs[MAX_STATUS_LINES];
extern word32 g_palette_8to1624[256];
extern word32 g_a2palette_8to1624[256];



#define X_EVENT_LIST_ALL_WIN						\
	(ExposureMask | ButtonPressMask | ButtonReleaseMask |		\
	 OwnerGrabButtonMask | KeyPressMask | KeyReleaseMask |		\
	 KeymapStateMask | ColormapChangeMask | FocusChangeMask)

#define X_BASE_WIN_EVENT_LIST						\
	(X_EVENT_LIST_ALL_WIN | PointerMotionMask | ButtonMotionMask)

#define X_A2_WIN_EVENT_LIST						\
	(X_BASE_WIN_EVENT_LIST)

int	g_num_a2_keycodes = 0;
int a2_key_to_sdlkeycode[][3] = {
	{ 0x12, SDLK_1, 0},
	{ 0x35,	SDLK_ESCAPE,0 },
	{ 0x7a,	SDLK_F1,	0 },
	{ 0x78,	SDLK_F2,	0 },
	{ 0x63,	SDLK_F3,	0 },
	{ 0x76,	SDLK_F4,	0 },
	{ 0x60,	SDLK_F5,	0 },
	{ 0x61,	SDLK_F6,	0 },
	{ 0x62,	SDLK_F7,	0 },
	{ 0x64,	SDLK_F8,	0 },
	{ 0x65,	SDLK_F9,	0 },
	{ 0x6d,	SDLK_F10,	0 },
	{ 0x67,	SDLK_F11,	0 },
	{ 0x6f,	SDLK_F12,	0 },
	{ 0x69,	SDLK_F13,	0 },
	{ 0x6b,	SDLK_F14,	0 },
	{ 0x71,	SDLK_F15,	0 },
	{ 0x7f, SDLK_PAUSE, XK_Break },
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
	{ 0x33,	SDLK_BACKSPACE, 0 },
	{ 0x72,	SDLK_INSERT, XK_Help },	/* Help? */
/*	{ 0x73,	XK_Home, 0 },		alias XK_Home to be XK_KP_Equal! */
	{ 0x74,	XK_Page_Up, 0 },
	{ 0x47,	SDLK_NUMLOCKCLEAR, XK_Clear },	/* Clear */
	{ 0x51,	SDLK_KP_EQUALS, XK_Home },		/* Note XK_Home alias! */
	{ 0x4b,	SDLK_KP_DIVIDE, 0 },
	{ 0x43,	SDLK_KP_MULTIPLY, 0 },
	{ 0x30,	SDLK_TAB, 0 },
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
	{ 0x59,	SDLK_KP_7, XK_KP_Home },
	{ 0x5b,	SDLK_KP_8, SDLK_UP },
	{ 0x5c,	SDLK_KP_9, XK_KP_Page_Up },
	{ 0x4e,	SDLK_KP_MINUS, 0 },

	{ 0x39,	SDLK_CAPSLOCK, 0 },
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
	{ 0x24,	SDLK_RETURN, 0 },
	{ 0x56,	SDLK_KP_4, SDLK_LEFT},
	{ 0x57,	SDLK_KP_5, 0 },
	{ 0x58,	SDLK_KP_6, SDLK_RIGHT },
	{ 0x45,	SDLK_KP_PLUS, 0 },

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
	{ 0x3e,	SDLK_UP, 0 },
	{ 0x53,	SDLK_KP_1, XK_KP_End },
	{ 0x54,	SDLK_KP_2, SDLK_DOWN },
	{ 0x55,	SDLK_KP_3, XK_KP_Page_Down },

	{ 0x36,	XK_Control_L, XK_Control_R },
	{ 0x3a,	SDLK_LALT, SDLK_RALT },		/* Option */
	{ 0x37,	SDLK_LGUI, SDLK_RGUI },		/* Command */
	{ 0x31,	' ', 0 },
	{ 0x3b,	SDLK_LEFT, 0 },
	{ 0x3d,	SDLK_DOWN, 0 },
	{ 0x3c,	SDLK_RIGHT, 0 },
	{ 0x52,	SDLK_KP_0, XK_KP_Insert },
	{ 0x41,	SDLK_KP_PERIOD, XK_KP_Separator },
	{ 0x4c,	SDLK_KP_ENTER, 0 },
	{ -1, -1, -1 }

};
int
main(int argc, char **argv)
{
        return gsportmain(argc, argv);
}





//
// SDL CODE START  !!!!!!!!!!!
//



SDL_Window *window;                    // Declare a pointer
SDL_Renderer *renderer;
SDL_Texture *texture;



void set_refresh_needed() {
	g_a2_screen_buffer_changed = -1;
	g_full_refresh_needed = -1;

	g_border_sides_refresh_needed = 1;
	g_border_special_refresh_needed = 1;
	g_status_refresh_needed = 1;
}



/// Queries the Screen to see if it's set to Fullscreen or Not
/// @return SDL_FALSE if windowed, SDL_TRUE if fullscreen
SDL_bool IsFullScreen(SDL_Window *win)
{
	Uint32 flags = SDL_GetWindowFlags(win);
	if (flags & SDL_WINDOW_FULLSCREEN) return SDL_TRUE; // return SDL_TRUE if fullscreen
	return SDL_FALSE; // Return SDL_FALSE if windowed
}



int
sdl_keysym_to_a2code(int keysym, int is_up)
{
	int	i;

	if(keysym == 0) {
		return -1;
	}

	if((keysym == SDLK_LSHIFT) || (keysym == SDLK_RSHIFT)) {
		if(is_up) {
			kb_shift_control_state &= ~ShiftMask;
		} else {
			kb_shift_control_state |= ShiftMask;
		}
	}
	if(keysym == SDLK_CAPSLOCK) {
		if(is_up) {
			kb_shift_control_state &= ~LockMask;
		} else {
			kb_shift_control_state |= LockMask;
		}
	}
	if((keysym == SDLK_LCTRL) || (keysym == SDLK_RCTRL)) {
		if(is_up) {
			kb_shift_control_state &= ~ControlMask;
		} else {
			kb_shift_control_state |= ControlMask;
		}
	}

	/* Look up Apple 2 keycode */
	for(i = g_num_a2_keycodes - 1; i >= 0; i--) {
		if((keysym == a2_key_to_sdlkeycode[i][1]) ||
			(keysym == a2_key_to_sdlkeycode[i][2])) {
			return a2_key_to_sdlkeycode[i][0];
		}
	}

	return -1;
}



void
handle_sdl_key_event(SDL_Event event)
{

	int	state_xor;
	int state = 0;
	int	is_up;

	int mod = event.key.keysym.mod;

	// simulate xmask style here
	//state = state & (ControlMask | LockMask | ShiftMask);

	// when mod key is first press, comes as event, otherwise just a modifier
	if( mod & KMOD_LCTRL  || mod & KMOD_RCTRL ||
		event.type == SDL_KEYDOWN && (event.key.keysym.sym == SDLK_LCTRL || event.key.keysym.sym == SDLK_RCTRL)) {
		state = state | ControlMask;
	}
	if( (mod & KMOD_LSHIFT)  || (mod & KMOD_RSHIFT) ||
	     event.type == SDL_KEYDOWN && (event.key.keysym.sym == SDLK_LSHIFT || event.key.keysym.sym == SDLK_RSHIFT)) {
		state = state | ShiftMask;
	}
	if( mod & KMOD_CAPS) {
		state = state | LockMask;
	}

	state_xor = kb_shift_control_state ^ state;
	is_up = 0;
	if(state_xor & ControlMask) {
		is_up = ((state & ControlMask) == 0);
		adb_physical_key_update(0x36, is_up);
	}
	if(state_xor & LockMask) {
		is_up = ((state & LockMask) == 0);
		adb_physical_key_update(0x39, is_up);
	}
	if(state_xor & ShiftMask) {
		is_up = ((state & ShiftMask) == 0);
		adb_physical_key_update(0x38, is_up);
	}

	kb_shift_control_state = state;


	is_up = 0;
	int a2code;
	if (event.type == SDL_KEYUP) {
		is_up = 1;
	}
	switch( event.key.keysym.sym ){
		case SDLK_F11:
			printf("Toggle Fullscreen");
			if (!is_up) {
				if (!IsFullScreen(window)) {
					SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
				} else {
					SDL_SetWindowFullscreen(window, 0);
					SDL_SetWindowSize(window, BASE_WINDOW_WIDTH, X_A2_WINDOW_HEIGHT);
				}
			}
			break;
		default:
			a2code = sdl_keysym_to_a2code(event.key.keysym.sym, is_up);
			if(a2code >= 0) {
				adb_physical_key_update(a2code, is_up);
			}
	}
}



int
handle_sdl_mouse_motion_event(SDL_Event event) {
	int x, y;
	// @todo: FIX MOUSE BUTTON MAPPING, AT LEAST CLEAN UP AND DOCUMENT BEHAVIOR
	//printf (" %04x\t", event.motion.state &7);
	x = event.motion.x - BASE_MARGIN_LEFT;
	y = event.motion.y - BASE_MARGIN_TOP;
	if (event.type == SDL_MOUSEBUTTONUP) {
		return update_mouse(x, y, 0 , event.motion.state &7 );
	} else {
		return update_mouse(x, y, event.motion.state, event.motion.state &7 );
	}
}



void
osx_dev_video_init()
{

    SDL_Init(SDL_INIT_VIDEO);              // Initialize SDL2

    // Create an application window with the following settings:
    window = SDL_CreateWindow(
        "GSPLUS V.0",                  // window title
        SDL_WINDOWPOS_UNDEFINED,           // initial x position
        SDL_WINDOWPOS_UNDEFINED,           // initial y position
        BASE_WINDOW_WIDTH,                               // width, in pixels
        X_A2_WINDOW_HEIGHT,                               // height, in pixels
        SDL_WINDOW_OPENGL                  // flags - see below
    );

    // Check that the window was successfully created
    if (window == NULL) {
        // In the case that the window could not be made...
        printf("Could not create window: %s\n", SDL_GetError());
				//@todo die, i guess
    } else {
        printf("SDL Window has been created\n");
    }

		renderer = SDL_CreateRenderer(window, -1, 0);

		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");  // make the scaled rendering look smoother.
	//			SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");  // make the scaled rendering look smoother.
		SDL_RenderSetLogicalSize(renderer, BASE_WINDOW_WIDTH, X_A2_WINDOW_HEIGHT);

		// SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
		// SDL_RenderClear(renderer);
		// SDL_RenderPresent(renderer);


		texture = SDL_CreateTexture(renderer,
		                               SDL_PIXELFORMAT_ARGB8888,
		                               SDL_TEXTUREACCESS_STREAMING,
		                               BASE_WINDOW_WIDTH, X_A2_WINDOW_HEIGHT);
    // The window is open: could enter program loop here (see SDL_PollEvent())



		SDL_ShowCursor(SDL_DISABLE);

    //SDL_Delay(3000);  // Pause execution for 3000 milliseconds, for example

    // Close and destroy the window
    //SDL_DestroyWindow(window);

    // Clean up
    //SDL_Quit();
    //return 0;
}



void sdl_push_kimage(Kimage *kimage_ptr,
	int destx, int desty, int srcx, int srcy, int width, int height)
{
	byte *src_ptr;
	int pixel_size = 4;
	src_ptr = kimage_ptr->data_ptr + (srcy * kimage_ptr->width_act + srcx) * pixel_size;
  //src_ptr = kimage_ptr->data_ptr;
  //src_ptr = kimage_ptr->data_ptr;
	//src_ptr = xim->data;
	SDL_Rect dstrect;
	dstrect.x = destx;
	dstrect.y = desty;
	dstrect.w = width;
	dstrect.h = height;
	int pitch = 640;
	if (width < 560) {
		pitch = EFF_BORDER_WIDTH;
		pitch = BORDER_WIDTH+72;
		//printf("EFF_BORDER_WIDTH : %d" ,EFF_BORDER_WIDTH);
	}
	//SDL_UpdateTexture(texture, NULL, src_ptr, 640 * sizeof (Uint32));
	SDL_UpdateTexture(texture, &dstrect, src_ptr, pitch*4 );
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);

}

















void
x_push_kimage(Kimage *kimage_ptr, int destx, int desty, int srcx, int srcy, int width, int height)
{
	sdl_push_kimage(kimage_ptr, destx, desty, srcx, srcy, width, height);
}

// called by src/sim65816.c
void
x_dialog_create_gsport_conf(const char *str)
{
	// Just write the config file already...
	config_write_config_gsport_file();
}

// called by src/sim65816.c
int
x_show_alert(int is_fatal, const char *str)
{
	return 0;
}


void
dev_video_init()
{
	// build keycode map ??
	g_num_a2_keycodes = 0;
	int	i;
	int	keycode;
	int	tmp_array[0x80];
	for(i = 0; i <= 0x7f; i++) {
		tmp_array[i] = 0;
	}
	for(i = 0; i < 0x7f; i++) {
		keycode = a2_key_to_sdlkeycode[i][0];
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

  osx_dev_video_init();
	//return;

	XVisualInfo *visualList;

	word32	create_win_list;
	int	depth;
	int	len;
	int	cmap_alloc_amt;
	int	cnt;
	int	base_height;
	int	screen_num;
	char	*myTextString[1];
	word32	lores_col;
	int	ret;


	g_display = XOpenDisplay(NULL);
	if(g_display == NULL) {
		fprintf(stderr, "Can't open display\n");
		exit(1);
	}
	screen_num = DefaultScreen(g_display);

	len = sizeof(g_depth_attempt_list)/sizeof(int);

	g_vis = 0;
	for(i = 0; i < len; i++) {
		depth = g_depth_attempt_list[i];

		g_vis = x_try_find_visual(depth, screen_num,
			&visualList);
		if(g_vis != 0) {
			break;
		}
	}

	video_get_kimages();
	video_get_kimage(&g_mainwin_kimage, 0, g_screen_depth,g_screen_mdepth);

	/* Done with visualList now */
	XFree(visualList);

	for(i = 0; i < 256; i++) {
		g_xcolor_a2vid_array[i].pixel = i;
		lores_col = g_lores_colors[i & 0xf];
		video_update_color_raw(i, lores_col);
		g_a2palette_8to1624[i] = g_palette_8to1624[i];
	}

}

Visual *
x_try_find_visual(int depth, int screen_num, XVisualInfo **visual_list_ptr)
{
	XVisualInfo *visualList;
	XVisualInfo *v_chosen;
	XVisualInfo vTemplate;
	int	visualsMatched;
	int	mdepth;
	int	needs_cmap;
	int	visual_chosen;
	int	match8, match24;
	int	i;

	vTemplate.screen = screen_num;
	vTemplate.depth = depth;

	visualList = XGetVisualInfo(g_display,
		(VisualScreenMask | VisualDepthMask),
		&vTemplate, &visualsMatched);

	vid_printf("visuals matched: %d\n", visualsMatched);
	if(visualsMatched == 0) {
		return (Visual *)0;
	}



	v_chosen = &(visualList[visual_chosen]);

	g_screen_depth = depth;
	mdepth = depth;
	if(depth > 8) {
		mdepth = 16;
	}
	if(depth > 16) {
		mdepth = 32;
	}
	g_screen_mdepth = mdepth;
	g_needs_cmap = needs_cmap;
	*visual_list_ptr = visualList;

	return v_chosen->visual;
}



int g_xshm_error = 0;

int xhandle_shm_error(Display *display, XErrorEvent *event) {	return 0; }

void
x_get_kimage(Kimage *kimage_ptr) {
	if(g_use_shmem) {
		g_use_shmem = get_shm(kimage_ptr);
	}
	if(!g_use_shmem) {
		get_ximage(kimage_ptr);
	}
}

int
get_shm(Kimage *kimage_ptr)
{

	XShmSegmentInfo *seginfo;
	XImage *xim;
	
	int	width;
	int	height;
	int	depth;

	width = kimage_ptr->width_req;
	height = kimage_ptr->height;
	depth = kimage_ptr->depth;

	seginfo = (XShmSegmentInfo *)malloc(sizeof(XShmSegmentInfo));
	xim = XShmCreateImage(g_display, g_vis, depth, ZPixmap,
		(char *)0, seginfo, width, height);

	/* check mdepth! */
	if(xim->bits_per_pixel != kimage_ptr->mdepth) {
		printf("get_shm bits_per_pix: %d != %d\n",
				xim->bits_per_pixel, g_screen_mdepth);
	}

	vid_printf("xim: %p\n", xim);
//	kimage_ptr->dev_handle = xim;
//	kimage_ptr->dev_handle2 = seginfo;
	if(xim == 0) {
		return 0;
	}

	/* It worked, we got it */
	seginfo->shmid = shmget(IPC_PRIVATE, xim->bytes_per_line * xim->height,
		IPC_CREAT | 0777);
	vid_printf("seginfo->shmid = %d\n", seginfo->shmid);
	if(seginfo->shmid < 0) {
		XDestroyImage(xim);
		return 0;
	}

	/* Still working */
	seginfo->shmaddr = (char *)shmat(seginfo->shmid, 0, 0);
	xim->data = seginfo->shmaddr;
	seginfo->readOnly = False;


	vid_printf("about to RMID the shmid\n");
	shmctl(seginfo->shmid, IPC_RMID, 0);

	XFlush(g_display);


	if(g_xshm_error) {
		XDestroyImage(xim);
		/* We could release the shared mem segment, but by doing the */
		/* RMID, it will go away when we die now, so just leave it */
		printf("Not using shared memory\n");
		return 0;
	}

	kimage_ptr->data_ptr = (byte *)xim->data;
	vid_printf("Sharing memory. xim: %p, xim->data: %p\n", xim, xim->data);

	return 1;

}

void
check_input_events_sdl()
{
	// @todo: make sure it's not queueing events / processing full queue each call
	int	motion = 0;
  SDL_Event event;
	int is_up = 0;
	int a2code = 0x00;
  while (SDL_PollEvent(&event)) {
		switch( event.type ){
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				handle_sdl_key_event(event);
				break;
			case SDL_MOUSEMOTION:
			case SDL_MOUSEBUTTONUP:
			case SDL_MOUSEBUTTONDOWN:
				motion |= handle_sdl_mouse_motion_event(event);
				break;
			case SDL_QUIT:
				//quit = 1;     /* SDL_QUIT event (window close) */
				SDL_DestroyWindow(window);
				iwm_shut();

				// Clean up
				SDL_Quit();

				my_exit(1);

				break;
			default:
				break;
		}
  }
}


void
check_input_events()
{
  check_input_events_sdl();
}


void get_ximage(Kimage *kimage_ptr) { }
void x_toggle_status_lines() { }
void x_redraw_status_lines() { }
void x_hide_pointer(int do_hide) { }
void handle_keysym(XEvent *xev_in) { }
int x_keysym_to_a2code(int keysym, int is_up) { return -1; }
void x_update_modifier_state(int state) { }
void x_auto_repeat_on(int must) { }
void x_auto_repeat_off(int must) { }
void x_full_screen(int do_full) { }
// OG Adding release
void x_release_kimage(Kimage* kimage_ptr) { }
// OG Addding ratio
int x_calc_ratio(float x,float y) { return 1; }
// TODO: Add clipboard support
void clipboard_paste(void) { }
int clipboard_get_char(void) { return 0; }
void x_set_mask_and_shift(word32 x_mask, word32 *mask_ptr, int *shift_left_ptr, int *shift_right_ptr) {	return; }
int x_update_mouse(int raw_x, int raw_y, int button_states, int buttons_valid) { }
void x_update_color(int col_num, int red, int green, int blue, word32 rgb) { }
void x_update_physical_colormap() { }
void show_xcolor_array() { }
int my_error_handler(Display *display, XErrorEvent *ev) { return 0; }
void xdriver_end() { }
void show_colormap(char *str, Colormap cmap, int index1, int index2, int index3) { }
void x_badpipe(int signum) { }
