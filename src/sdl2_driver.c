/*
 GSPLUS - Advanced Apple IIGS Emulator Environment
 Copyright (C) 2016 - Dagen Brock
 
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

// This is an experimental video driver for the KEGS/GSport emulator.
// It requires SDL2 libraries to build.  I've tested on Mac, but should
// be easy to port to other platforms. -DagenBrock

// @todo: mouse clip bugs.. great western shootout.
// @todo: force refresh after screen mode change

#include "SDL.h"
#include <stdbool.h>
#include <time.h>
#include <stdlib.h>
#include <signal.h>
#include "defc.h"

// BITMASKS
#define ShiftMask  	1
#define ControlMask	4
#define LockMask  	2

int g_use_shmem = 0;

int g_num_check_input_calls = 0;
int g_check_input_flush_rate = 2;
int	g_win_status_debug = 0;			// Current visibility of status lines.
int g_win_status_debug_request = 0;	// Desired visibility of status lines.
int g_screen_mdepth = 0;
int kb_shift_control_state = 0;

extern int g_screen_depth;
extern int g_quit_sim_now;
extern int g_border_sides_refresh_needed;
extern int g_border_special_refresh_needed;
extern int g_status_refresh_needed;
extern int g_lores_colors[];
extern int g_a2vid_palette;
extern int g_installed_full_superhires_colormap;
extern char *g_status_ptrs[MAX_STATUS_LINES];
extern word32 g_a2_screen_buffer_changed;
extern word32 g_full_refresh_needed;
extern word32 g_palette_8to1624[256];
extern word32 g_a2palette_8to1624[256];
extern Kimage g_mainwin_kimage;

SDL_Window *window;                    // Declare a pointer
SDL_Renderer *renderer;
SDL_Texture *texture;

void dev_video_init_sdl();
void handle_sdl_key_event(SDL_Event event);
void check_input_events_sdl();
int handle_sdl_mouse_motion_event(SDL_Event event);

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
	{ 0x7f, SDLK_PAUSE, 0 },
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
	{ 0x72,	SDLK_INSERT, 0 },	/* Help? XK_Help  */
/*	{ 0x73,	XK_Home, 0 },		alias XK_Home to be XK_KP_Equal! */
	{ 0x74,	SDLK_PAGEUP, 0 },
	{ 0x47,	SDLK_NUMLOCKCLEAR, 0 },	/* Clear, XK_Clear */
	{ 0x51,	SDLK_KP_EQUALS, 0 },		/* Note XK_Home alias! XK_Home */
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
	{ 0x75,	SDLK_DELETE, 0 },
	{ 0x77,	SDLK_END, 0 },
	{ 0x79,	SDLK_PAGEDOWN, 0 },
	{ 0x59,	SDLK_KP_7, 0 },
	{ 0x5b,	SDLK_KP_8, 0 },
	{ 0x5c,	SDLK_KP_9, 0 },
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
	{ 0x38,	SDLK_LSHIFT, SDLK_RSHIFT },
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
	{ 0x53,	SDLK_KP_1, 0 },
	{ 0x54,	SDLK_KP_2, SDLK_DOWN },
	{ 0x55,	SDLK_KP_3, 0 },
	{ 0x36,	SDLK_RCTRL, SDLK_LCTRL },
	{ 0x3a,	SDLK_LALT, SDLK_RALT },		/* Option */
	{ 0x37,	SDLK_LGUI, SDLK_RGUI },		/* Command */
	{ 0x31,	' ', 0 },
	{ 0x3b,	SDLK_LEFT, 0 },
	{ 0x3d,	SDLK_DOWN, 0 },
	{ 0x3c,	SDLK_RIGHT, 0 },
	{ 0x52,	SDLK_KP_0, 0 },
	{ 0x41,	SDLK_KP_PERIOD, 0 },
	{ 0x4c,	SDLK_KP_ENTER, 0 },
	{ -1, -1, -1 }

};
int
main(int argc, char **argv)
{
        return gsportmain(argc, argv);
}



/// Queries the Screen to see if it's set to Fullscreen or Not
/// @return SDL_FALSE if windowed, SDL_TRUE if fullscreen
SDL_bool IsFullScreen(SDL_Window *win)
{
	Uint32 flags = SDL_GetWindowFlags(win);
	if (flags & SDL_WINDOW_FULLSCREEN) return SDL_TRUE; // return SDL_TRUE if fullscreen
	return SDL_FALSE; // Return SDL_FALSE if windowed
}




void
dev_video_init()
{
	word32	lores_col;

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

	// This actually creates our window
  dev_video_init_sdl();

	// @todo DANGER DANGER.  HARD CODING THESE.. there was logic for stepping values in xdriver
	g_screen_depth = 24;
	g_screen_mdepth =32;
	video_get_kimages();
	video_get_kimage(&g_mainwin_kimage, 0, g_screen_depth,g_screen_mdepth);

	for(i = 0; i < 256; i++) {
		//g_xcolor_a2vid_array[i].pixel = i;
		lores_col = g_lores_colors[i & 0xf];
		video_update_color_raw(i, lores_col);
		g_a2palette_8to1624[i] = g_palette_8to1624[i];
	}
}



// Initialize our SDL window and texture
void
dev_video_init_sdl()
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

		texture = SDL_CreateTexture(renderer,
		                               SDL_PIXELFORMAT_ARGB8888,
		                               SDL_TEXTUREACCESS_STREAMING,
		                               BASE_WINDOW_WIDTH, X_A2_WINDOW_HEIGHT);
    // The window is open: could enter program loop here (see SDL_PollEvent())

		SDL_ShowCursor(SDL_DISABLE);

}


// Copy a rect to our SDL window
void sdl_push_kimage(Kimage *kimage_ptr,
	int destx, int desty, int srcx, int srcy, int width, int height)
{
	byte *src_ptr;
	int pixel_size = 4;
	src_ptr = kimage_ptr->data_ptr + (srcy * kimage_ptr->width_act + srcx) * pixel_size;
  //src_ptr = kimage_ptr->data_ptr;

	SDL_Rect dstrect;
	dstrect.x = destx;
	dstrect.y = desty;
	dstrect.w = width;
	dstrect.h = height;
	int pitch = 640;
	if (width < 560) {
		pitch = EFF_BORDER_WIDTH;
		// This is another bad hack.  Possibly not cross platform.
		pitch = BORDER_WIDTH+72;
		//printf("EFF_BORDER_WIDTH : %d" ,EFF_BORDER_WIDTH);
	}
	//SDL_UpdateTexture(texture, NULL, src_ptr, 640 * sizeof (Uint32));
	SDL_UpdateTexture(texture, &dstrect, src_ptr, pitch*4 );
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);

}


void set_refresh_needed() {
	g_a2_screen_buffer_changed = -1;
	g_full_refresh_needed = -1;

	g_border_sides_refresh_needed = 1;
	g_border_special_refresh_needed = 1;
	g_status_refresh_needed = 1;
}


void
x_get_kimage(Kimage *kimage_ptr) {
			byte *data;
			int	width;
			int	height;
			int	depth;

			width = kimage_ptr->width_req;
			height = kimage_ptr->height;
			depth = kimage_ptr->depth;
			// this might be too big!!! I had it at depth/3 but it segfaults
			data = malloc(width*height*(depth/4));
			kimage_ptr->data_ptr = data;
}


void
check_input_events()
{
  check_input_events_sdl();
}


void
check_input_events_sdl()
{
	int	motion = 0;
  SDL_Event event;

  while (SDL_PollEvent(&event)) {
		/* Check all window events (mostly for Fullscreen) */
		if (event.type == SDL_WINDOWEVENT) {
			set_refresh_needed();
		}
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
			case SDL_DROPFILE:
				{
				char *file = event.drop.file;
				cfg_inspect_maybe_insert_file(file, 0);
				SDL_free(file);
				}
				break;

			default:
				break;
		}
  }
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
	// @todo: this can probably all be refactored now that X is gone
	//state = state & (ControlMask | LockMask | ShiftMask);

	// when mod key is first press, comes as event, otherwise just a modifier
	if( mod & KMOD_LCTRL  || mod & KMOD_RCTRL ||
		event.type == (SDL_KEYDOWN && (event.key.keysym.sym == SDLK_LCTRL || event.key.keysym.sym == SDLK_RCTRL))) {
		state = state | ControlMask;
	}
	if( (mod & KMOD_LSHIFT)  || (mod & KMOD_RSHIFT) ||
	     event.type == (SDL_KEYDOWN && (event.key.keysym.sym == SDLK_LSHIFT || event.key.keysym.sym == SDLK_RSHIFT))) {
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
			break;
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




// Old driver cruft

// called by src/sim65816.c
int x_show_alert(int is_fatal, const char *str) { return 0; }

void get_ximage(Kimage *kimage_ptr) { }
void x_toggle_status_lines() { }
void x_redraw_status_lines() { }
void x_hide_pointer(int do_hide) { }
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
void x_update_color(int col_num, int red, int green, int blue, word32 rgb) { }
void x_update_physical_colormap() { }
void show_xcolor_array() { }
void xdriver_end() { }
void x_push_done() { }
