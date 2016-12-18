/*
 GSPLUS - Advanced Apple IIGS Emulator Environment

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


#include "defc.h"
#include "glog.h"

int
main(int argc, char **argv)
{
	return gsplusmain(argc, argv);
}

int g_screen_mdepth = 0;
int g_screenshot_requested = 0;
int g_use_shmem = 0;

extern int g_screen_depth;
extern word32 g_palette_8to1624[256];
extern word32 g_a2palette_8to1624[256];
extern Kimage g_mainwin_kimage;
extern int g_lores_colors[];

void check_input_events() {}

int clipboard_get_char() { return 0; }

void clipboard_paste(void) { }

void dev_video_init() { 

	g_screen_depth = 24;
	g_screen_mdepth = 32;
	video_get_kimages();
	video_get_kimage(&g_mainwin_kimage, 0, g_screen_depth, g_screen_mdepth);

	for(int i = 0; i < 256; i++) {
		word32 lores_col = g_lores_colors[i & 0xf];
		video_update_color_raw(i, lores_col);
		g_a2palette_8to1624[i] = g_palette_8to1624[i];
	}


}

void x_dialog_create_gsport_conf(const char *str) { }


int x_show_alert(int is_fatal, const char *str) { return 0; }

void get_ximage(Kimage *kimage_ptr) { }
void x_toggle_status_lines() { }
void x_redraw_status_lines() { }
void x_hide_pointer(int do_hide) { }
void x_auto_repeat_on(int must) { }
void x_auto_repeat_off(int must) { }
void x_release_kimage(Kimage* kimage_ptr) { }
int x_calc_ratio(float x,float y) { return 1; }


void x_set_mask_and_shift(word32 x_mask, word32 *mask_ptr, int *shift_left_ptr, int *shift_right_ptr) { }
void x_update_color(int col_num, int red, int green, int blue, word32 rgb) { }
void x_update_physical_colormap() { }
void show_xcolor_array() { }
void xdriver_end() { }
void x_push_done() { }

void x_full_screen(int do_full) { }

void
x_get_kimage(Kimage *kimage_ptr) {
			byte *data;
			int	width;
			int	height;
			int	depth;

			width = kimage_ptr->width_req;
			height = kimage_ptr->height;
			depth = kimage_ptr->depth;
			data = malloc(width*height*(depth/4));
			kimage_ptr->data_ptr = data;
}

void
x_push_kimage(Kimage *kimage_ptr, int destx, int desty, int srcx, int srcy, int width, int height)
{
}

