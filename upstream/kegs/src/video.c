const char rcsid_video_c[] = "@(#)$KmKId: video.c,v 1.213 2025-04-27 18:03:43+00 kentd Exp $";

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

#include <time.h>

#include "defc.h"

extern int Verbose;

word32 g_a2_filt_stat[200];
int g_a2_line_left_edge[200];
int g_a2_line_right_edge[200];

byte g_cur_border_colors[270];

word32	g_a2_screen_buffer_changed = (word32)-1;
word32	g_full_refresh_needed = (word32)-1;

word32 g_cycs_in_40col = 0;
word32 g_cycs_in_xredraw = 0;
word32 g_refresh_bytes_xfer = 0;

extern byte *g_slow_memory_ptr;
extern int g_fatal_log;

extern dword64 g_cur_dfcyc;

extern int g_line_ref_amt;

extern word32 g_c034_val;
extern int g_config_control_panel;
extern int g_halt_sim;

word32 g_slow_mem_changed[SLOW_MEM_CH_SIZE];
word32 g_slow_mem_ch2[SLOW_MEM_CH_SIZE];

word32 g_a2font_bits[0x100][8];

word32 g_superhires_scan_save[2][256];

Kimage g_mainwin_kimage = { 0 };
Kimage g_debugwin_kimage = { 0 };
int g_debugwin_last_total = 0;

extern int g_debug_lines_total;

extern dword64 g_last_vbl_dfcyc;
extern dword64 g_video_pixel_dcount;

dword64	g_video_dfcyc_check_input = 0;
int	g_video_act_margin_left = BASE_MARGIN_LEFT;
int	g_video_act_margin_right = BASE_MARGIN_RIGHT;
int	g_video_act_margin_top = BASE_MARGIN_TOP;
int	g_video_act_margin_bottom = BASE_MARGIN_BOTTOM;
int	g_video_act_width = X_A2_WINDOW_WIDTH;
int	g_video_act_height = X_A2_WINDOW_HEIGHT;
int	g_mainwin_width = X_A2_WINDOW_WIDTH;
int	g_mainwin_height = X_A2_WINDOW_HEIGHT + MAX_STATUS_LINES*16 + 2;
int	g_mainwin_xpos = 100;
int	g_mainwin_ypos = 300;
int	g_video_no_scale_window = 0;

word32	g_palette_change_cnt[2][16];
int	g_border_sides_refresh_needed = 1;
int	g_border_special_refresh_needed = 1;
int	g_border_line24_refresh_needed = 1;
int	g_status_refresh_needed = 1;

int	g_vbl_border_color = 0;
int	g_border_last_vbl_changes = 0;
int	g_border_reparse = 0;

int	g_use_dhr140 = 0;
int	g_use_bw_hires = 0;

int	g_vid_update_last_line = 0;
int	g_video_save_all_stat_pos = 0;

int g_cur_a2_stat = ALL_STAT_TEXT | ALL_STAT_ANNUNC3 |
					(0xf << BIT_ALL_STAT_TEXT_COLOR);

word32 g_palette_8to1624[2][256];
word32 g_a2palette_1624[16];

word32	g_saved_line_palettes[2][200][8];

word32 g_cycs_in_refresh_line = 0;
word32 g_cycs_in_refresh_ximage = 0;
word32 g_cycs_in_run_16ms = 0;

int	g_num_lines_superhires = 0;
int	g_num_lines_superhires640 = 0;
int	g_num_lines_prev_superhires = 0;
int	g_num_lines_prev_superhires640 = 0;

int	g_screen_redraw_skip_count = 0;
int	g_screen_redraw_skip_amt = -1;

word32	g_alpha_mask = 0;
word32	g_red_mask = 0xff;
word32	g_green_mask = 0xff;
word32	g_blue_mask = 0xff;
int	g_red_left_shift = 16;
int	g_green_left_shift = 8;
int	g_blue_left_shift = 0;
int	g_red_right_shift = 0;
int	g_green_right_shift = 0;
int	g_blue_right_shift = 0;

int	g_status_enable = 1;
int	g_status_enable_previous = 1;
char	g_status_buf[MAX_STATUS_LINES][STATUS_LINE_LENGTH + 1];
char	*g_status_ptrs[MAX_STATUS_LINES] = { 0 };
word16	g_pixels_widened[128];

int	g_video_scale_algorithm = 0;

STRUCT(Video_all_stat) {
	word32	lines_since_vbl;
	word32	cur_all_stat;
};

#define MAX_VIDEO_ALL_STAT	((200*42) + 40)
int g_video_all_stat_pos = 0;
Video_all_stat g_video_all_stat[MAX_VIDEO_ALL_STAT];

STRUCT(Video_filt_stat) {
	word32	line_bytes;
	word32	filt_stat;
};

#define MAX_VIDEO_FILT_STAT	10000
int g_video_filt_stat_pos = 0;
Video_filt_stat g_video_filt_stat[MAX_VIDEO_FILT_STAT];

int g_video_stat_old_pos = 0;
Video_filt_stat g_video_filt_stat_old[MAX_VIDEO_FILT_STAT];


word16 g_dhires_convert[4096];	/* look up { next4, this4, prev 4 } */

const byte g_dhires_colors_16[] = {		// Convert dhires to lores color
		0x00,	/* 0x0 black */
		0x02,	/* 0x1 dark blue */
		0x04,	/* 0x2 dark green */
		0x06,	/* 0x3 medium blue */
		0x08,	/* 0x4 brown */
		0x0a,	/* 0x5 light gray */
		0x0c,	/* 0x6 green */
		0x0e,	/* 0x7 aquamarine */
		0x01,	/* 0x8 deep red */
		0x03,	/* 0x9 purple */
		0x05,	/* 0xa dark gray */
		0x07,	/* 0xb light blue */
		0x09,	/* 0xc orange */
		0x0b,	/* 0xd pink */
		0x0d,	/* 0xe yellow */
		0x0f	/* 0xf white */
};

const int g_lores_colors[] = {		// From IIgs Technote #63
		/* rgb */
		0x000,		/* 0x0 black */
		0xd03,		/* 0x1 deep red */
		0x009,		/* 0x2 dark blue */
		0xd2d,		/* 0x3 purple */
		0x072,		/* 0x4 dark green */
		0x555,		/* 0x5 dark gray */
		0x22f,		/* 0x6 medium blue */
		0x6af,		/* 0x7 light blue */
		0x850,		/* 0x8 brown */
		0xf60,		/* 0x9 orange */
		0xaaa,		/* 0xa light gray */
		0xf98,		/* 0xb pink */
		0x1d0,		/* 0xc green */
		0xff0,		/* 0xd yellow */
		0x4f9,		/* 0xe aquamarine */
		0xfff		/* 0xf white */
};

const byte g_hires_lookup[64] = {
// Indexed by { next_bit, this_bit, prev_bit, hibit, odd_byte, odd_col }.
//  Return lores colors: 0, 3, 6, 9, 0xc, 0xf
	0x00,			// 00,0000	// black: this and next are 0
	0x00,			// 00,0001
	0x00,			// 00,0010
	0x00,			// 00,0011
	0x00,			// 00,0100
	0x00,			// 00,0101
	0x00,			// 00,0110
	0x00,			// 00,0111
	0x00,			// 00,1000
	0x00,			// 00,1001
	0x00,			// 00,1010
	0x00,			// 00,1011
	0x00,			// 00,1100
	0x00,			// 00,1101
	0x00,			// 00,1110
	0x00,			// 00,1111

	0x03,			// 01,0000	// purple
	0x03,			// 01,0001	// purple
	0x0c,			// 01,0010	// green (odd column)
	0x0c,			// 01,0011	// green
	0x06,			// 01,0100	// blue
	0x06,			// 01,0101	// blue
	0x09,			// 01,0110	// orange
	0x09,			// 01,0111	// orange
	0x0f,			// 01,1000	// white: this and prev are 1
	0x0f,			// 01,1001
	0x0f,			// 01,1010
	0x0f,			// 01,1011
	0x0f,			// 01,1100
	0x0f,			// 01,1101
	0x0f,			// 01,1110
	0x0f,			// 01,1111

	0x00,			// 10,0000	// black
	0x00,			// 10,0001	// black
	0x00,			// 10,0010	// black
	0x00,			// 10,0011	// black
	0x00,			// 10,0100	// black
	0x00,			// 10,0101	// black
	0x00,			// 10,0110	// black
	0x00,			// 10,0111	// black
	0x0c,			// 10,1000	// green
	0x0c,			// 10,1001	// green
	0x03,			// 10,1010	// purple
	0x03,			// 10,1011	// purple
	0x09,			// 10,1100	// orange
	0x09,			// 10,1101	// orange
	0x06,			// 10,1110	// blue
	0x06,			// 10,1111	// blue

	0x0f,			// 11,0000	// white
	0x0f,			// 11,0001
	0x0f,			// 11,0010
	0x0f,			// 11,0011
	0x0f,			// 11,0100
	0x0f,			// 11,0101
	0x0f,			// 11,0110
	0x0f,			// 11,0111
	0x0f,			// 11,1000
	0x0f,			// 11,1001
	0x0f,			// 11,1010
	0x0f,			// 11,1011
	0x0f,			// 11,1100
	0x0f,			// 11,1101
	0x0f,			// 11,1110
	0x0f			// 11,1111
};

const int g_screen_index[] = {
		0x000, 0x080, 0x100, 0x180, 0x200, 0x280, 0x300, 0x380,
		0x028, 0x0a8, 0x128, 0x1a8, 0x228, 0x2a8, 0x328, 0x3a8,
		0x050, 0x0d0, 0x150, 0x1d0, 0x250, 0x2d0, 0x350, 0x3d0,
		0x078, 0x0f8, 0x178, 0x1f8, 0x278, 0x2f8, 0x378, 0x3f8
			// Last row is for float_bus() during VBL
};

byte g_font_array[256][8] = {
#include "kegsfont.h"
};

void
video_set_red_mask(word32 red_mask)
{
	video_set_mask_and_shift(red_mask, &g_red_mask, &g_red_left_shift,
							&g_red_right_shift);
}

void
video_set_green_mask(word32 green_mask)
{
	video_set_mask_and_shift(green_mask, &g_green_mask, &g_green_left_shift,
							&g_green_right_shift);
}

void
video_set_blue_mask(word32 blue_mask)
{
	video_set_mask_and_shift(blue_mask, &g_blue_mask, &g_blue_left_shift,
							&g_blue_right_shift);
}

void
video_set_alpha_mask(word32 alpha_mask)
{
	g_alpha_mask = alpha_mask;
	printf("Set g_alpha_mask=%08x\n", alpha_mask);
}

void
video_set_mask_and_shift(word32 x_mask, word32 *mask_ptr, int *shift_left_ptr,
						int *shift_right_ptr)
{
	int	shift;
	int	i;

	/* Shift until we find first set bit in mask, then remember mask,shift*/

	shift = 0;
	for(i = 0; i < 32; i++) {
		if(x_mask & 1) {
			/* we're done! */
			break;
		}
		x_mask = x_mask >> 1;
		shift++;
	}
	*mask_ptr = x_mask;
	*shift_left_ptr = shift;
	/* Now, calculate shift_right_ptr */
	shift = 0;
	x_mask |= 1;		// make sure at least one bit is set
	for(i = 0; i < 32; i++) {
		if(x_mask >= 0x80) {
			break;
		}
		shift++;
		x_mask = x_mask << 1;
	}

	*shift_right_ptr = shift;
}

void
video_set_palette()
{
	int	i;

	for(i = 0; i < 16; i++) {
		video_update_color_raw(0, i, g_lores_colors[i]);
		g_a2palette_1624[i] = g_palette_8to1624[0][i];
	}
}

void
video_set_redraw_skip_amt(int amt)
{
	if(g_screen_redraw_skip_amt < amt) {
		g_screen_redraw_skip_amt = amt;
		printf("Set g_screen_redraw_skip_amt = %d\n", amt);
	}
}

Kimage *
video_get_kimage(int win_id)
{
	if(win_id == 0) {
		return &g_mainwin_kimage;
	}
	if(win_id == 1) {
		return &g_debugwin_kimage;
	}
	printf("win_id: %d not supported\n", win_id);
	exit(1);
}

char *
video_get_status_ptr(int line)
{
	if(line < MAX_STATUS_LINES) {
		return g_status_ptrs[line];
	}
	return 0;
}

#if 0
int
video_get_x_refresh_needed(Kimage *kimage_ptr)
{
	int	ret;

	ret = kimage_ptr->x_refresh_needed;
	kimage_ptr->x_refresh_needed = 0;

	return ret;
}
#endif

void
video_set_x_refresh_needed(Kimage *kimage_ptr, int do_refresh)
{
	kimage_ptr->x_refresh_needed = do_refresh;
}

int
video_get_active(Kimage *kimage_ptr)
{
	return kimage_ptr->active;
}

void
video_set_active(Kimage *kimage_ptr, int active)
{
	kimage_ptr->active = active;
	if(kimage_ptr != &g_mainwin_kimage) {
		adb_nonmain_check();
	}
}

void
video_init(int mdepth, int screen_width, int screen_height, int no_scale_window)
{
	word32	col[4];
	word32	val0, val1, val2, val3, next_col, next2_col;
	word32	val, cur_col;
	int	i, j;

// Initialize video system (called one-time only)

	g_video_no_scale_window = no_scale_window;

	for(i = 0; i < 200; i++) {
		g_a2_line_left_edge[i] = 0;
		g_a2_line_right_edge[i] = 0;
	}
	for(i = 0; i < 200; i++) {
		g_a2_filt_stat[i] = -1;
		for(j = 0; j < 8; j++) {
			g_saved_line_palettes[0][i][j] = (word32)-1;
			g_saved_line_palettes[1][i][j] = (word32)-1;
		}
	}
	for(i = 0; i < 262; i++) {
		g_cur_border_colors[i] = -1;
	}
	for(i = 0; i < 128; i++) {
		val0 = i;
		val1 = 0;
		for(j = 0; j < 7; j++) {
			val1 = val1 << 2;
			if(val0 & 0x40) {
				val1 |= 3;
			}
			val0 = val0 << 1;
		}
		g_pixels_widened[i] = val1;
	}

	vid_printf("Zeroing out video memory, mdepth:%d\n", mdepth);

	for(i = 0; i < SLOW_MEM_CH_SIZE; i++) {
		g_slow_mem_changed[i] = (word32)-1;
		g_slow_mem_ch2[i] = 0;
	}

	// create g_dhires_convert[] array
	// TODO: Look at patent #4786893 for details on VGC and dhr
	for(i = 0; i < 4096; i++) {
		/* Convert index bits 11:0 where 3:0 is the previous color */
		/*  and 7:4 is the current color to translate */
		/*  Bit 4 will be the first pixel displayed on the screen */
		for(j = 0; j < 4; j++) {
			cur_col = (i >> (1 + j)) & 0xf;
			next_col = (i >> (2 + j)) & 0xf;
			next2_col = (i >> (3 + j)) & 0xf;
			cur_col = (((cur_col << 4) + cur_col) >> (3 - j)) & 0xf;

			if((cur_col == 0xf) || (next_col == 0xf) ||
							(next2_col == 0xf)) {
				cur_col = 0xf;
				col[j] = cur_col;
			} else if((cur_col == 0) || (next_col == 0) ||
						(next2_col == 0)) {
				cur_col = 0;
				col[j] = cur_col;
			} else {
				col[j] = cur_col;
			}
		}
		if(g_use_dhr140) {
			for(j = 0; j < 4; j++) {
				col[j] = (i >> 4) & 0xf;
			}
		}
		val0 = g_dhires_colors_16[col[0] & 0xf];
		val1 = g_dhires_colors_16[col[1] & 0xf];
		val2 = g_dhires_colors_16[col[2] & 0xf];
		val3 = g_dhires_colors_16[col[3] & 0xf];
		val = val0 | (val1 << 4) | (val2 << 8) | (val3 << 12);
		g_dhires_convert[i] = val;
		if((i == 0x7bc) || (i == 0xfff)) {
			//printf("g_dhires_convert[%03x] = %04x\n", i, val);
		}
	}

	video_init_kimage(&g_mainwin_kimage, X_A2_WINDOW_WIDTH,
				X_A2_WINDOW_HEIGHT + MAX_STATUS_LINES*16 + 2,
				screen_width, screen_height);

	video_init_kimage(&g_debugwin_kimage, 80*8 + 8 + 8, 25*16 + 8 + 8,
				screen_width, screen_height);

	change_display_mode(g_cur_dfcyc);
	video_reset();
	g_vid_update_last_line = 0;
	g_video_all_stat_pos = 1;
	g_video_all_stat[0].cur_all_stat = 0;
	g_video_all_stat[0].lines_since_vbl = 0;
	g_video_save_all_stat_pos = 0;
	g_video_filt_stat_pos = 0;
	video_update_status_enable(&g_mainwin_kimage);
	video_update_through_line(262);

	printf("g_mainwin_kimage created and init'ed\n");
	fflush(stdout);
}

int
video_clamp(int value, int min, int max)
{
	// Ensure value is >= min and <= max.  If max <= min, return min
	if(value > max) {
		value = max;
	}
	if(value <= min) {
		value = min;
	}
	return value;
}

void
video_init_kimage(Kimage *kimage_ptr, int width, int height,
			int screen_width, int screen_height)
{
	int	x_width, x_height, a2_height, x_xpos, x_ypos;
	int	i;

	if(screen_width < width) {
		screen_width = width;
	}
	if(screen_height < height) {
		screen_width = height;
	}
	x_width = width;
	x_height = height;
	a2_height = height;
	x_xpos = 100;
	x_ypos = 300;
	if(kimage_ptr == &g_mainwin_kimage) {
		x_width = g_mainwin_width;
		x_height = g_mainwin_height;
		x_xpos = g_mainwin_xpos;
		x_ypos = g_mainwin_ypos;

		// Handle status lines now
		if(!g_status_enable) {
			a2_height = g_video_act_margin_top + A2_WINDOW_HEIGHT +
						g_video_act_margin_bottom;
		}
	}

	x_width = video_clamp(x_width, width, screen_width);
	x_height = video_clamp(x_height, height, screen_height);
	x_xpos = video_clamp(x_xpos, 0, screen_width - 640);
	x_ypos = video_clamp(x_ypos, 0, screen_height - 420);

	kimage_ptr->wptr = (word32 *)calloc(1, width * (height + 2) * 4);
		// Scaling routines read from line+1, expect it to be 0
	kimage_ptr->a2_width_full = width;
	kimage_ptr->a2_height_full = height;
	kimage_ptr->a2_width = width;
	kimage_ptr->a2_height = a2_height;
	kimage_ptr->x_width = x_width;
	kimage_ptr->x_height = x_height;
	kimage_ptr->x_refresh_needed = 1;
	kimage_ptr->x_max_width = screen_width;
	kimage_ptr->x_max_height = screen_height;
	kimage_ptr->x_xpos = x_xpos;
	kimage_ptr->x_ypos = x_ypos;
	kimage_ptr->active = 0;
	kimage_ptr->vbl_of_last_resize = 0;
	kimage_ptr->c025_val = 0;
	//printf("Created window, width:%d x_width:%d height:%d x_height:%d\n",
	//	width, x_width, height, x_height);

	kimage_ptr->scale_width_to_a2 = 0x10000;
	kimage_ptr->scale_width_a2_to_x = 0x10000;
	kimage_ptr->scale_height_to_a2 = 0x10000;
	kimage_ptr->scale_height_a2_to_x = 0x10000;
	kimage_ptr->num_change_rects = 0;
	for(i = 0; i <= MAX_SCALE_SIZE; i++) {
		kimage_ptr->scale_width[i] = i;
		kimage_ptr->scale_height[i] = i;
	}

	video_update_scale(kimage_ptr, x_width, x_height, 1);
}

void
show_a2_line_stuff()
{
	int	num, num_filt;
	int	i;

	for(i = 0; i < 200; i++) {
		printf("line: %d: stat: %07x, "
			"left_edge:%d, right_edge:%d\n",
			i, g_a2_filt_stat[i],
			g_a2_line_left_edge[i],
			g_a2_line_right_edge[i]);
	}

	num = g_video_all_stat_pos;
	num_filt = g_video_stat_old_pos;
	printf("cur_a2_stat:%04x, all_stat_pos:%d, num_filt:%d\n",
			g_cur_a2_stat, num, num_filt);
	for(i = 0; i < num; i++) {
		printf("all_stat[%3d]=%08x stat:%08x\n", i,
			g_video_all_stat[i].lines_since_vbl,
			g_video_all_stat[i].cur_all_stat);
	}
	for(i = 0; i < num_filt; i++) {
		printf("filt[%3d]=%08x filt_stat:%08x\n", i,
			g_video_filt_stat_old[i].line_bytes,
			g_video_filt_stat_old[i].filt_stat);
	}
}

int	g_flash_count = 0;

void
video_reset()
{
	int	stat;
	int	i;

	voc_reset();

	stat = ALL_STAT_TEXT | ALL_STAT_ANNUNC3 |
					(0xf << BIT_ALL_STAT_TEXT_COLOR);
	if(g_use_bw_hires) {
		stat |= ALL_STAT_COLOR_C021;
	}
	if(g_config_control_panel) {
		/* Don't update cur_a2_stat when in configuration panel */
		//g_save_cur_a2_stat = stat;
	} else {
		g_cur_a2_stat = stat;
	}

	for(i = 0; i < 16; i++) {
		g_palette_change_cnt[0][i] = 0;
		g_palette_change_cnt[1][i] = 0;
	}
}

word32	g_cycs_in_check_input = 0;

void
video_update()
{
	int	did_video;

	if(g_fatal_log > 0) {
		// NOT IMPLEMENTED YET
		//adb_all_keys_up();
		clear_fatal_logs();
	}
	if(g_status_enable != g_status_enable_previous) {
		g_status_enable_previous = g_status_enable;
		video_update_status_enable(&g_mainwin_kimage);
	}

	debugger_redraw_screen(&g_debugwin_kimage);
	if(g_config_control_panel) {
		return;		// Nothing else to do
	}

	g_screen_redraw_skip_count--;
	did_video = 0;
	if(g_screen_redraw_skip_count < 0) {
		did_video = 1;
		video_copy_changed2();
		video_update_event_line(262);
		update_border_info();
		g_screen_redraw_skip_count = g_screen_redraw_skip_amt;
	}

	/* update flash */
	g_flash_count++;
	if(g_flash_count >= 16) {
		g_flash_count = 0;
		g_cur_a2_stat ^= ALL_STAT_FLASH_STATE;
		change_display_mode(g_cur_dfcyc);
	}

	if(did_video) {
		g_vid_update_last_line = 0;
		g_video_all_stat_pos = 1;
		g_video_all_stat[0].cur_all_stat = g_cur_a2_stat;
		g_video_all_stat[0].lines_since_vbl = 0;
		g_video_save_all_stat_pos = 0;
		g_video_filt_stat_pos = 0;
	}
}

word32
video_all_stat_to_filt_stat(int line, word32 new_all_stat)
{
	word32	filt_stat, merge_mask, mix_t_gr;

	filt_stat = new_all_stat & ALL_STAT_TEXT;
	merge_mask = 0;
	if((new_all_stat & ALL_STAT_ST80) == 0) {
		merge_mask = ALL_STAT_PAGE2;
	}
	mix_t_gr = new_all_stat & ALL_STAT_MIX_T_GR;
	if(new_all_stat & ALL_STAT_SUPER_HIRES) {
		filt_stat = ALL_STAT_SUPER_HIRES;
		merge_mask = ALL_STAT_VOC_INTERLACE | ALL_STAT_VOC_MAIN;
	} else if(line >= 192) {
		filt_stat = ALL_STAT_BORDER;
	} else if(filt_stat || (line >= 160 && mix_t_gr)) {
		// text mode
		filt_stat |= ALL_STAT_TEXT;
		merge_mask |= ALL_STAT_ALTCHARSET | ALL_STAT_BG_COLOR |
					ALL_STAT_TEXT_COLOR | ALL_STAT_VID80;
		if((new_all_stat & ALL_STAT_ALTCHARSET) == 0) {
			merge_mask |= ALL_STAT_FLASH_STATE;
		}
	} else {
		// GR or Hires
		merge_mask |= ALL_STAT_ANNUNC3 | ALL_STAT_HIRES;
		if((new_all_stat & ALL_STAT_ANNUNC3) == 0) {
			// AN3 must be 0 to enable dbl-lores or dbl-hires
			merge_mask |= ALL_STAT_VID80;
		}
		if(new_all_stat & ALL_STAT_HIRES) {
			merge_mask |= ALL_STAT_COLOR_C021 |
						ALL_STAT_DIS_COLOR_DHIRES;
		}
	}
	filt_stat = filt_stat | (new_all_stat & merge_mask);
	return filt_stat;
}

void
change_display_mode(dword64 dfcyc)
{
	word32	lines_since_vbl;

	lines_since_vbl = get_lines_since_vbl(dfcyc);
	video_add_new_all_stat(dfcyc, lines_since_vbl);

}

void
video_add_new_all_stat(dword64 dfcyc, word32 lines_since_vbl)
{
	word32	my_start, first_start, prev_lines_since_vbl;
	int	pos, prev;

	pos = g_video_all_stat_pos;
	my_start = lines_since_vbl & 0x1ff00;
	first_start = my_start + 24;
	if(lines_since_vbl >= (200 << 8)) {
		return;			// In VBL, don't log this
	}
	if(pos && (lines_since_vbl < first_start)) {
		prev = pos - 1;
		prev_lines_since_vbl = g_video_all_stat[prev].lines_since_vbl;
		// If the previous toggle has the same line, and it is before
		//  offset 24, then ignore it and overwrite it
		if((my_start <= prev_lines_since_vbl) &&
					(prev_lines_since_vbl < first_start)) {
			// needless toggling during HBL, just toss earlier
			pos = prev;
		}
	}
	g_video_all_stat[pos].lines_since_vbl = lines_since_vbl;
	g_video_all_stat[pos].cur_all_stat = g_cur_a2_stat;
	if(!g_halt_sim || g_config_control_panel) {
		dbg_log_info(dfcyc, g_cur_a2_stat, lines_since_vbl,
							(pos << 16) | 0x102);
	}
	pos++;
	if(pos >= MAX_VIDEO_ALL_STAT) {
		pos--;
	}
	g_video_all_stat_pos = pos;
}

#define MAX_BORDER_CHANGES	16384

STRUCT(Border_changes) {
	word32	usec;
	int	val;
};

Border_changes g_border_changes[MAX_BORDER_CHANGES];
int	g_num_border_changes = 0;

void
change_border_color(dword64 dfcyc, int val)
{
	int	pos;

	pos = g_num_border_changes;
	g_border_changes[pos].usec = (word32)((dfcyc - g_last_vbl_dfcyc) >> 16);
	g_border_changes[pos].val = val;

	pos++;
	g_num_border_changes = pos;

	if(pos >= MAX_BORDER_CHANGES) {
		halt_printf("num border changes: %d\n", pos);
		g_num_border_changes = 0;
	}
}

void
update_border_info()
{
	dword64	drecip_usec, dline;
	word32	usec;
	int	offset, new_line_offset, last_line_offset, new_line, new_val;
	int	limit, color_now;
	int	i;

	/* to get this routine to redraw the border, change */
	/*  g_vbl_border_color, set g_border_last_vbl_changes = 1 */
	/*  and change the cur_border_colors[] array */

	color_now = g_vbl_border_color;

	drecip_usec = (65536LL * 65536LL) / 65;
	limit = g_num_border_changes;
	if(g_border_last_vbl_changes || limit || g_border_reparse) {
		/* add a dummy entry */
		g_border_changes[limit].usec = CYCLES_IN_16MS_RAW + 21;
		g_border_changes[limit].val = (g_c034_val & 0xf);
		limit++;
	}
	last_line_offset = (((word32)-1L) << 8) + 44;
	for(i = 0; i < limit; i++) {
		usec = g_border_changes[i].usec;
		dline = usec * drecip_usec;
		new_line = dline >> 32;
		offset = ((dword64)(word32)dline * 65ULL) >> 32;

		/* here comes the tricky part */
		/* offset is from 0 to 65, where 0-3 is the right border of */
		/*  the previous line, 4-20 is horiz blanking, 21-24 is the */
		/*  left border and 25-64 is the main window */
		/* Convert this to a new notation which is 0-3 is the left */
		/*  border, 4-43 is the main window, and 44-47 is the right */
		/* basically, add -21 to offset, and wrap < 0 to previous ln */
		/* note this makes line -1 offset 44-47 the left hand border */
		/* for true line 261 on the screen */
		offset -= 21;
		if(offset < 0) {
			new_line--;
			offset += 64;
		}
		new_val = g_border_changes[i].val;
		new_line_offset = (new_line << 8) + offset;

		if((new_line_offset < -256) ||
					(new_line_offset > (262*256 + 0x80))) {
			printf("new_line_offset: %05x\n", new_line_offset);
			new_line_offset = last_line_offset;
		}
		while(last_line_offset < new_line_offset) {
			/* see if this will finish it */
			if((last_line_offset & -256)==(new_line_offset & -256)){
				update_border_line(last_line_offset,
						new_line_offset, color_now);
				last_line_offset = new_line_offset;
			} else {
				update_border_line(last_line_offset,
						(last_line_offset & -256) + 65,
						color_now);
				last_line_offset =(last_line_offset & -256)+256;
			}
		}

		color_now = new_val;
	}

#if 0
	if(g_num_border_changes) {
		printf("Border changes: %d\n", g_num_border_changes);
	}
#endif

	if(limit > 1) {
		g_border_last_vbl_changes = 1;
	} else {
		g_border_last_vbl_changes = 0;
	}

	g_num_border_changes = 0;
	g_border_reparse = 0;
	g_vbl_border_color = (g_c034_val & 0xf);
}

void
update_border_line(int st_line_offset, int end_line_offset, int color)
{
	word32	filt_stat;
	int	st_offset, end_offset, left, right, width, line;

	line = st_line_offset >> 8;
	if(line != (end_line_offset >> 8)) {
		halt_printf("ubl, %04x %04x %02x!\n", st_line_offset,
					end_line_offset, color);
	}
	if(line < -1 || line >= 262) {
		halt_printf("ubl-b, mod line is %d\n", line);
		line = 0;
	}
	if(line < 0 || line >= 262) {
		line = 0;
	}

	st_offset = st_line_offset & 0xff;
	end_offset = end_line_offset & 0xff;

	if((st_offset == 0) && (end_offset >= 0x41) && !g_border_reparse) {
		/* might be the same as last time, save some work */
		if(g_cur_border_colors[line] == color) {
			return;
		}
		g_cur_border_colors[line] = color;
	} else {
		g_cur_border_colors[line] = -1;
	}

	/* 0-3: left border, 4-43: main window, 44-47: right border */
	/* 48-65: horiz blanking */
	/* first, do the sides from line 0 to line 199 */
	if((line < 200) || (line >= 262)) {
		if(line >= 262) {
			line = 0;
		}
		if(st_offset < 4) {
			/* left side */
			left = st_offset;
			right = MY_MIN(4, end_offset);
			video_border_pixel_write(&g_mainwin_kimage,
				g_video_act_margin_top + 2*line, 2, color,
				(left * BORDER_WIDTH)/4,
				(right * BORDER_WIDTH) / 4);

			g_border_sides_refresh_needed = 1;
		}
		if((st_offset < 48) && (end_offset >= 44)) {
			/* right side */
			filt_stat = g_a2_filt_stat[line];
			width = BORDER_WIDTH;
			if((filt_stat & ALL_STAT_SUPER_HIRES) == 0) {
				width += 80;
			}
			left = MY_MAX(0, st_offset - 44);
			right = MY_MIN(4, end_offset - 44);
			video_border_pixel_write(&g_mainwin_kimage,
				g_video_act_margin_top + 2*line, 2, color,
				X_A2_WINDOW_WIDTH - width +
						(left * width/4),
				X_A2_WINDOW_WIDTH - width +
						(right * width/4));
			g_border_sides_refresh_needed = 1;
		}
	}

	if((line >= 192) && (line < 200)) {
		filt_stat = g_a2_filt_stat[line];
		if((filt_stat & ALL_STAT_BORDER) && (st_offset < 44) &&
							(end_offset > 4)) {
			left = MY_MAX(0, st_offset - 4);
			right = MY_MIN(40, end_offset - 4);
			video_border_pixel_write(&g_mainwin_kimage,
				g_video_act_margin_top + 2*line, 2, color,
				g_video_act_margin_left + (left * 640 / 40),
				g_video_act_margin_left + (right * 640 / 40));
			g_border_line24_refresh_needed = 1;
		}
	}

	/* now do the bottom, lines 200 to 215 */
	if((line >= 200) && (line < (200 + BASE_MARGIN_BOTTOM/2)) ) {
		line -= 200;
		left = st_offset;
		right = MY_MIN(48, end_offset);
		video_border_pixel_write(&g_mainwin_kimage,
			g_video_act_margin_top + 200*2 + 2*line, 2,
			color,
			(left * X_A2_WINDOW_WIDTH / 48),
			(right * X_A2_WINDOW_WIDTH / 48));
		g_border_special_refresh_needed = 1;
	}

	/* and top, lines 236 to 262 */
	if((line >= (262 - BASE_MARGIN_TOP/2)) && (line < 262)) {
		line -= (262 - BASE_MARGIN_TOP/2);
		left = st_offset;
		right = MY_MIN(48, end_offset);
		video_border_pixel_write(&g_mainwin_kimage, 2*line, 2, color,
			(left * X_A2_WINDOW_WIDTH / 48),
			(right * X_A2_WINDOW_WIDTH / 48));
		g_border_special_refresh_needed = 1;
	}
}

void
video_border_pixel_write(Kimage *kimage_ptr, int starty, int num_lines,
			int color, int st_off, int end_off)
{
	word32	*wptr, *wptr0;
	word32	pixel;
	int	width, width_full, offset;
	int	i, j;

	if(end_off <= st_off) {
		return;
	}

	width = end_off - st_off;
	width_full = kimage_ptr->a2_width_full;

	if(width > width_full) {
		halt_printf("border write but width %d > act %d\n", width,
				width_full);
		return;
	}
	if((starty + num_lines) > kimage_ptr->a2_height) {
		halt_printf("border write line %d, > act %d\n",
				starty+num_lines, kimage_ptr->a2_height);
		return;
	}
	pixel = g_a2palette_1624[color & 0xf];

	offset = starty * width_full;
	wptr0 = kimage_ptr->wptr;
	wptr0 += offset;
	for(i = 0; i < num_lines; i++) {
		wptr = wptr0 + st_off;
		for(j = 0; j < width; j++) {
			*wptr++ = pixel;
		}
		wptr0 += width_full;
	}
}

word32
video_get_ch_mask(word32 mem_ptr, word32 filt_stat, int reparse)
{
	word32	ch_mask, mask;
	int	shift;

	if(reparse) {
		return (word32)-1;
	}
	shift = (mem_ptr >> SHIFT_PER_CHANGE) & 0x1f;
	mask = (1 << (40 >> SHIFT_PER_CHANGE)) - 1;
	ch_mask = g_slow_mem_changed[mem_ptr >> CHANGE_SHIFT] |
			g_slow_mem_ch2[mem_ptr >> CHANGE_SHIFT];
	if(filt_stat & ALL_STAT_VID80) {
		mem_ptr += 0x10000;
		ch_mask |= (g_slow_mem_changed[mem_ptr >> CHANGE_SHIFT]);
		ch_mask |= (g_slow_mem_ch2[mem_ptr >> CHANGE_SHIFT]);
	}
	ch_mask = (ch_mask >> shift) & mask;

	return ch_mask;
}

void
video_update_edges(int line, int left, int right, const char *str)
{
	g_a2_line_left_edge[line] = MY_MIN(left, g_a2_line_left_edge[line]);
	g_a2_line_right_edge[line] = MY_MAX(right, g_a2_line_right_edge[line]);

	if((left < 0) || (right < 0) || (left > 640) || (right > 640)) {
		printf("video_update_edges: %s: line %d: %d (left) >= %d "
			"(right)\n", str, line, left, right);
	}
}

void
redraw_changed_text(word32 line_bytes, int reparse, word32 *in_wptr,
				int pixels_per_line, word32 filt_stat)
{
	byte	str_buf[81];
	byte	*slow_mem_ptr;
	word32	ch_mask, line_mask, mem_ptr, val0, val1, bg_pixel, fg_pixel;
	int	flash_state, y, bg_color, fg_color, start_line;
	int	x1, x2;

	// Redraws a single line, will be called over 8 lines to finish a byte.

	start_line = line_bytes >> 16;
	bg_color = (filt_stat >> BIT_ALL_STAT_BG_COLOR) & 0xf;
	fg_color = (filt_stat >> BIT_ALL_STAT_TEXT_COLOR) & 0xf;
	bg_pixel = g_a2palette_1624[bg_color];
	fg_pixel = g_a2palette_1624[fg_color];

	y = start_line >> 3;
	line_mask = 1 << y;
	mem_ptr = 0x400 + g_screen_index[y];
	if(filt_stat & ALL_STAT_PAGE2) {
		mem_ptr += 0x400;
	}
	if((mem_ptr < 0x400) || (mem_ptr >= 0xc00)) {
		halt_printf("redraw_changed_text: mem_ptr: %08x, y:%d\n",
								mem_ptr, y);
		return;
	}

	ch_mask = video_get_ch_mask(mem_ptr, filt_stat, reparse);
	if(!ch_mask) {
		return;
	}

	g_a2_screen_buffer_changed |= line_mask;
	x2 = 0;
	slow_mem_ptr = &(g_slow_memory_ptr[mem_ptr]);
	flash_state = -0x40;
	if(g_cur_a2_stat & ALL_STAT_FLASH_STATE) {
		flash_state = 0x40;
	}

	for(x1 = 0; x1 < 40; x1++) {
		val0 = slow_mem_ptr[0x10000];
		val1 = *slow_mem_ptr++;
		if(!(filt_stat & ALL_STAT_ALTCHARSET)) {
			if((val0 >= 0x40) && (val0 < 0x80)) {
				val0 += flash_state;
			}
			if((val1 >= 0x40) && (val1 < 0x80)) {
				val1 += flash_state;
			}
		}
		if(filt_stat & ALL_STAT_VID80) {
			str_buf[x2++] = val0;		// aux mem
		}
		str_buf[x2++] = val1;			// main mem
	}
	str_buf[x2] = 0;				// null terminate

	redraw_changed_string(&str_buf[0], line_bytes, ch_mask, in_wptr,
		bg_pixel, fg_pixel, pixels_per_line,
		(filt_stat & ALL_STAT_VID80));
}

void
redraw_changed_string(const byte *bptr, word32 line_bytes, word32 ch_mask,
	word32 *in_wptr, word32 bg_pixel,
	word32 fg_pixel, int pixels_per_line, int dbl)
{
	register word32 start_time, end_time;
	word32	*wptr;
	word32	val0, val1, val2, val3, pixel;
	int	left, right, st_line_mod8, offset, pos, shift, start_line;
	int	start_byte, end_byte;
	int	x1, j;

	left = 40;
	right = 0;

	GET_ITIMER(start_time);

	start_line = line_bytes >> 16;
	start_byte = line_bytes & 0x3f;
	end_byte = (line_bytes >> 8) & 0x3f;
	st_line_mod8 = start_line & 7;

	for(x1 = start_byte; x1 < end_byte; x1++) {
		shift = x1 >> SHIFT_PER_CHANGE;
		if(((ch_mask >> shift) & 1) == 0) {
			continue;
		}

		left = MY_MIN(x1, left);
		right = MY_MAX(x1 + 1, right);
		offset = (start_line * 2 * pixels_per_line) + x1*14;
		pos = x1;
		if(dbl) {
			pos = pos * 2;
		}

		wptr = in_wptr + offset;

		val0 = bptr[pos];
		if(dbl) {
			pos++;
		}
		val1 = bptr[pos++];
		val2 = g_a2font_bits[val0][st_line_mod8];
		val3 = g_a2font_bits[val1][st_line_mod8];
			// val2, [6:0] is 80-column character bits, and
			//  [21:8] are the 40-column char bits (double-wide)
		if(dbl) {
			val2 = (val3 << 7) | (val2 & 0x7f);
		} else {
			val2 = val3 >> 8;	// 40-column format
		}
		for(j = 0; j < 14; j++) {
			pixel = bg_pixel;
			if(val2 & 1) {		// LSB is first pixel
				pixel = fg_pixel;
			}
			wptr[pixels_per_line] = pixel;
			*wptr++ = pixel;
			val2 = val2 >> 1;
		}
	}
	GET_ITIMER(end_time);
	if(start_line < 200) {
		video_update_edges(start_line, left * 14, right * 14, "text");
	}

	if((left >= right) || (left < 0) || (right < 0)) {
		printf("str line %d, 40: left >= right: %d >= %d\n",
			start_line, left, right);
		printf(" line_bytes:%08x ch_mask:%08x\n", line_bytes, ch_mask);
	}

	g_cycs_in_40col += (end_time - start_time);
}

// gr with an3=0:
// 0=0
// 1,0=3 (purple). 1,1=0
// 2,0=c (green). 2,1=0
// 3,0=f (white). 3,1=0
// 4,0=0. 4,1=c (green)
// 5,0=3 (purple). 5,1=c (green)
// 6,0=c (green). 6,1=c (green)
// 7,0=f (white). 7,1=c (green)
// 8,0=0 (black). 7,1=3 (purple)
// 9,0=3 (purple). 9,1=3 (purple)
// a,0=c (green). a,1=3 (purple)
// b,0=f (white). b,1=3 (purple)
// c,0=0 (black). c,1=f (white)
// d,0=3 (purple). d,1=f (white)
// e,0=c (green). e,1=f (white)
// f,0=f (white). e,1=f (white)

void
redraw_changed_gr(word32 line_bytes, int reparse, word32 *in_wptr,
				int pixels_per_line, word32 filt_stat)
{
	word32	*wptr;
	byte	*slow_mem_ptr;
	word32	line_mask, mem_ptr, val0, val1, pixel0, pixel1, ch_mask;
	int	y, shift, left, right, st_line_mod8, start_line, offset;
	int	start_byte, end_byte;
	int	x1, i;

	start_line = line_bytes >> 16;
	st_line_mod8 = start_line & 7;

	y = start_line >> 3;
	line_mask = 1 << (y);
	mem_ptr = 0x400 + g_screen_index[y];
	if(filt_stat & ALL_STAT_PAGE2) {
		mem_ptr += 0x400;
	}
	if((mem_ptr < 0x400) || (mem_ptr >= 0xc00)) {
		halt_printf("redraw_changed_gr: mem_ptr: %08x, y:%d\n",
							mem_ptr, y);
		return;
	}

	ch_mask = video_get_ch_mask(mem_ptr, filt_stat, reparse);
	if(!ch_mask) {
		return;
	}

	g_a2_screen_buffer_changed |= line_mask;

	left = 40;
	right = 0;

	slow_mem_ptr = &(g_slow_memory_ptr[mem_ptr]);
	offset = (start_line * 2 * pixels_per_line);
	start_byte = line_bytes & 0x3f;
	end_byte = (line_bytes >> 8) & 0x3f;
	for(x1 = start_byte; x1 < end_byte; x1++) {
		shift = x1 >> SHIFT_PER_CHANGE;
		if(((ch_mask >> shift) & 1) == 0) {
			continue;
		}

		left = MY_MIN(x1, left);
		right = MY_MAX(x1 + 1, right);

		wptr = in_wptr + offset + x1*14;

		val0 = slow_mem_ptr[0x10000 + x1];
		val1 = slow_mem_ptr[x1];

		if(st_line_mod8 >= 4) {
			val0 = val0 >> 4;
			val1 = val1 >> 4;
		}
		if(filt_stat & ALL_STAT_VID80) {
			// aux pixel is { [2:0],[3] }
			val0 = (val0 << 1) | ((val0 >> 3) & 1);
		} else if((filt_stat & ALL_STAT_ANNUNC3) == 0) {
			if(x1 & 1) {			// odd cols
				val0 = ((val1 >> 1) & 2) | ((val1 >> 3) & 1);
			} else {
				val0 = val1 & 3;	// even cols
			}
			// map val0: 0->0, 1->3, 2->c, 3->f
			val1 = 0;
			if(val0 & 1) {
				val1 |= 3;
			}
			if(val0 & 2) {
				val1 |= 0xc;
			}
			val0 = val1;
		} else {
			val0 = val1;
		}
		pixel0 = g_a2palette_1624[val0 & 0xf];
		pixel1 = g_a2palette_1624[val1 & 0xf];
		for(i = 0; i < 7; i++) {
			wptr[pixels_per_line] = pixel0;
			wptr[pixels_per_line + 7] = pixel1;
			wptr[0] = pixel0;
			wptr[7] = pixel1;
			wptr++;
		}
	}

	video_update_edges(start_line, left * 14, right * 14, "gr");
}

void
video_hgr_line_segment(byte *slow_mem_ptr, word32 *wptr, int start_byte,
		int end_byte, int pixels_per_line, word32 filt_stat)
{
	word32	val0, val1, val2, prev_bits, val1_hi, dbl_step, pixel, color;
	word32	monochrome;
	int	shift;
	int	x2, i;

	monochrome = filt_stat & (ALL_STAT_COLOR_C021 |
						ALL_STAT_DIS_COLOR_DHIRES);

	prev_bits = 0;
	if(start_byte) {
		prev_bits = (slow_mem_ptr[-1] >> 3) & 0xf;
		if(!(filt_stat & ALL_STAT_VID80)) {
			// prev_bits is 4 bits, widen to 8 for std HGR
			prev_bits = g_pixels_widened[prev_bits] >> 4;
		}
		prev_bits = prev_bits & 0xf;
	}
	for(x2 = start_byte; x2 < end_byte; x2++) {
		val0 = slow_mem_ptr[0x10000];
		val1 = *slow_mem_ptr++;
		val2 = slow_mem_ptr[0x10000];	// next pixel, aux mem
		if(x2 >= 39) {
			val2 = 0;
		}
		val1_hi = ((val1 >> 5) & 4) | ((x2 & 1) << 1);
			// Hi-order bit in bit 2, odd pixel is in bit 0

		dbl_step = 3;
		if(filt_stat & ALL_STAT_VID80) {
			// aux+1[6:0], main[6:0], aux[6:0], prev[3:0]
			val0 = (val2 << 18) | ((val1 & 0x7f) << 11) |
							((val0 & 0x7f) << 4);
			if(!monochrome && (x2 & 1)) {	// Get 6 bits from prev
				val0 = (val0 << 2);
				dbl_step = 1;
			}
			val0 = val0 | prev_bits;
		} else if(monochrome) {
			val0 = g_pixels_widened[val1 & 0x7f] << 4;
		} else {			// color, normal hgr
			val2 = g_pixels_widened[*slow_mem_ptr & 0x7f];
			if(x2 >= 39) {
				val2 = 0;
			}
			val0 = ((val1 & 0x7f) << 4) | prev_bits | (val2 << 11);
			if((filt_stat & ALL_STAT_ANNUNC3) == 0) {
				val1_hi = val1_hi & 3;
			}
		}
#if 0
		if(st_line < 8) {
			printf("hgrl %d c:%d,d:%d, off:%03x val0:%02x 1:%02x\n",
				st_line, monochrome, dbl, x1 + x2, val0, val1);
		}
#endif
		for(i = 0; i < 14; i++) {
			color = 0;			// black
			if(monochrome) {
				if(val0 & 0x10) {
					color = 0xf;	// white
				}
				val0 = val0 >> 1;
			} else {			// color
				if(filt_stat & ALL_STAT_VID80) {
					color = g_dhires_convert[val0 & 0xfff];
					shift = (x2 + x2 + i) & 3;
					color = color >> (4 * shift);
					if((i & 3) == dbl_step) {
						val0 = val0 >> 4;
					}
				} else {
					val2 = (val0 & 0x38) ^ val1_hi ^(i & 3);
					color = g_hires_lookup[val2 & 0x7f];
					if(i & 1) {
						val0 = val0 >> 1;
					}
				}
			}
			pixel = g_a2palette_1624[color & 0xf];
			wptr[pixels_per_line] = pixel;
			*wptr++ = pixel;
		}
		if((filt_stat & ALL_STAT_VID80) && ((x2 & 1) == 0)) {
			prev_bits = val0 & 0x3f;
		} else {
			prev_bits = val0 & 0xf;
		}
	}
}

void
redraw_changed_hgr(word32 line_bytes, int reparse,
	word32 *in_wptr, int pixels_per_line, word32 filt_stat)
{
	word32	*wptr;
	byte	*slow_mem_ptr;
	word32	ch_mask, line_mask, mem_ptr;
	int	y, shift, st_line_mod8, start_line, offset, start_byte;
	int	end_byte;
	int	x1;

	start_line = line_bytes >> 16;
	start_byte = line_bytes & 0x3f;
	end_byte = (line_bytes >> 8) & 0x3f;	// Usually '40'

	y = start_line >> 3;
	st_line_mod8 = start_line & 7;
	line_mask = 1 << y;
	mem_ptr = 0x2000 + g_screen_index[y] + (st_line_mod8 * 0x400);
	if(filt_stat & ALL_STAT_PAGE2) {
		mem_ptr += 0x2000;
	}
	if((mem_ptr < 0x2000) || (mem_ptr >= 0x6000)) {
		halt_printf("redraw_changed_hgr: mem_ptr: %08x, y:%d\n",
								mem_ptr, y);
		return;
	}

	ch_mask = video_get_ch_mask(mem_ptr, filt_stat, reparse);
	if(ch_mask == 0) {
		return;
	}

	// Hires depends on adjacent bits, so also reparse adjacent regions
	//  to handle redrawing of pixels on the boundaries
	ch_mask = ch_mask | (ch_mask >> 1) | (ch_mask << 1);

	g_a2_screen_buffer_changed |= line_mask;

	for(x1 = start_byte; x1 < end_byte; x1++) {
		shift = x1 >> SHIFT_PER_CHANGE;
		if(((ch_mask >> shift) & 1) == 0) {
			continue;
		}

		slow_mem_ptr = &(g_slow_memory_ptr[mem_ptr + x1]);
		offset = (start_line * 2 * pixels_per_line) + x1*14;

		wptr = in_wptr + offset;
		video_hgr_line_segment(slow_mem_ptr, wptr, x1, end_byte,
				pixels_per_line, filt_stat);

		video_update_edges(start_line, x1 * 14, end_byte * 14, "hgr");
		break;
	}
}

int
video_rebuild_super_hires_palette(int bank, word32 scan_info, int line,
								int reparse)
{
	word32	*word_ptr;
	byte	*byte_ptr;
	word32	ch_mask, mem_ptr, scan, old_scan, val0, val1;
	int	diffs, palette;
	int	j;

	palette = scan_info & 0xf;

	mem_ptr = (bank << 16) + 0x9e00 + (palette * 0x20);
	ch_mask = video_get_ch_mask(mem_ptr, 0, 0);

	old_scan = g_superhires_scan_save[bank][line];
	scan = (scan_info & 0xfaf) +
				(g_palette_change_cnt[bank][palette] << 12);
	g_superhires_scan_save[bank][line] = scan;

#if 0
	if(line == 1) {
		word_ptr = (word32 *)&(g_slow_memory_ptr[0x19e00+palette*0x20]);
		printf("y1vrshp, ch:%08x, s:%08x,os:%08x %d = %08x %08x %08x "
			"%08x %08x %08x %08x %08x\n",
			ch_mask, scan, old_scan, reparse,
			word_ptr[0], word_ptr[1], word_ptr[2], word_ptr[3],
			word_ptr[4], word_ptr[5], word_ptr[6], word_ptr[7]);
	}
#endif

	diffs = reparse | ((scan ^ old_scan) & 0xf0f);
		/* we must do full reparse if palette changed for this line */

	if(!diffs && (ch_mask == 0) && (((scan ^ old_scan) & (~0xf0)) == 0)) {
		/* nothing changed, get out fast */
		return 0;
	}

	if(ch_mask) {
		/* indicates the palette has changed, and other scan lines */
		/*  using this palette need to do a full 32-byte compare to */
		/*  decide if they need to update or not */
		g_palette_change_cnt[bank][palette]++;
	}

	word_ptr = (word32 *)&(g_slow_memory_ptr[(bank << 16) + 0x9e00 +
							palette*0x20]);
	for(j = 0; j < 8; j++) {
		if(word_ptr[j] != g_saved_line_palettes[bank][line][j]) {
			diffs = 1;
			break;
		}
	}

	if(diffs == 0) {
		return 0;
	}

	/* first, save this word_ptr into saved_line_palettes */
	byte_ptr = (byte *)word_ptr;
	for(j = 0; j < 8; j++) {
		g_saved_line_palettes[bank][line][j] = word_ptr[j];
	}

	byte_ptr = (byte *)word_ptr;
	/* this palette has changed */
	for(j = 0; j < 16; j++) {
		val0 = *byte_ptr++;
		val1 = *byte_ptr++;
		video_update_color_raw(bank, palette*16 + j, (val1<<8) + val0);
	}

	return 1;
}

word32
redraw_changed_super_hires_oneline(int bank, word32 *in_wptr,
		int pixels_per_line, int y, int scan, word32 ch_mask)
{
	word32	*palptr, *wptr;
	byte	*slow_mem_ptr;
	word32	mem_ptr, val0, pal, pix0, pix1, pix2, pix3, save_pix;
	int	offset, shift_per, left, right, shift;
	int	x1, x2;

	mem_ptr = (bank << 16) + 0x2000 + (0xa0 * y);

	shift_per = (1 << SHIFT_PER_CHANGE);
	pal = (scan & 0xf);

	save_pix = 0;
	if(scan & 0x20) {			// Fill mode
		ch_mask = (word32)-1;
	}

	palptr = &(g_palette_8to1624[bank][pal * 16]);
	left = 160;
	right = 0;

	for(x1 = 0; x1 < 0xa0; x1 += shift_per) {
		shift = x1 >> SHIFT_PER_CHANGE;
		if(((ch_mask >> shift) & 1) == 0) {
			continue;
		}

		left = MY_MIN(x1, left);
		right = MY_MAX(x1 + shift_per, right);

		slow_mem_ptr = &(g_slow_memory_ptr[mem_ptr + x1]);
		offset = x1*4;

		wptr = in_wptr + offset;

		for(x2 = 0; x2 < shift_per; x2++) {
			val0 = *slow_mem_ptr++;

			if(scan & 0x80) {		// 640 mode
				pix0 = (val0 >> 6) & 3;
				pix1 = (val0 >> 4) & 3;
				pix2 = (val0 >> 2) & 3;
				pix3 = val0 & 3;
				pix0 = palptr[pix0 + 8];
				pix1 = palptr[pix1 + 12];
				pix2 = palptr[pix2 + 0];
				pix3 = palptr[pix3 + 4];
			} else {	/* 320 mode */
				pix0 = (val0 >> 4);
				pix2 = (val0 & 0xf);
				if(scan & 0x20) {	// Fill mode
					if(!pix0) {	// 0 = repeat last color
						pix0 = save_pix;
					}
					if(!pix2) {
						pix2 = pix0;
					}
					save_pix = pix2;
				}
				pix0 = palptr[pix0];
				pix1 = pix0;
				pix2 = palptr[pix2];
				pix3 = pix2;
			}
			wptr[pixels_per_line] = pix0;
			*wptr++ = pix0;
			wptr[pixels_per_line] = pix1;
			*wptr++ = pix1;
			wptr[pixels_per_line] = pix2;
			*wptr++ = pix2;
			wptr[pixels_per_line] = pix3;
			*wptr++ = pix3;
		}
	}

	return (left << 16) | (right & 0xffff);
}

void
redraw_changed_super_hires_bank(int bank, int start_line, int reparse,
					word32 *wptr, int pixels_per_line)
{
	dword64	dval, dval1;
	word32	this_check, mask, tmp, scan, old_scan, mem_ptr;
	int	left, right, ret, shift;

	mem_ptr = (bank << 16) + 0x2000 + (160 * start_line);
	dval1 = g_slow_mem_changed[(mem_ptr >> CHANGE_SHIFT) + 1] |
			g_slow_mem_ch2[(mem_ptr >> CHANGE_SHIFT) + 1];
	dval = g_slow_mem_changed[mem_ptr >> CHANGE_SHIFT] |
		g_slow_mem_ch2[mem_ptr >> CHANGE_SHIFT] | (dval1 << 32);
	shift = (mem_ptr >> SHIFT_PER_CHANGE) & 0x1f;
	mask = (1 << (160 >> SHIFT_PER_CHANGE)) - 1;
	this_check = (dval >> shift) & mask;

	scan = g_slow_memory_ptr[(bank << 16) + 0x9d00 + start_line];

	old_scan = g_superhires_scan_save[bank][start_line];

	ret = video_rebuild_super_hires_palette(bank, scan, start_line,
								reparse);
	if(ret || reparse || ((scan ^ old_scan) & 0xa0)) {
					/* 0x80 == mode640, 0x20 = fill */
		this_check = (word32)-1;
	}

	if(!this_check) {
		return;			// Nothing to do, get out
	}

	if(scan & 0x80) {			// 640 mode
		g_num_lines_superhires640++;
	}

	if((scan >> 5) & 1) {		// fill mode--redraw whole line
		this_check = (word32)-1;
	}

	g_a2_screen_buffer_changed |= (1 << (start_line >> 3));
	tmp = redraw_changed_super_hires_oneline(bank, wptr, pixels_per_line,
						start_line, scan, this_check);
	left = tmp >> 16;
	right = tmp & 0xffff;

	video_update_edges(start_line, left * 4, right * 4, "shr");
}

void
redraw_changed_super_hires(word32 line_bytes, int reparse, word32 *wptr,
				int pixels_per_line, word32 filt_stat)
{
	int	bank, start_line;

	start_line = line_bytes >> 16;
	wptr += start_line * 2 * pixels_per_line;

	if(filt_stat & ALL_STAT_VOC_INTERLACE) {
		// Do 400 interlaced lines.  Do aux first, then main mem
		redraw_changed_super_hires_bank(1, start_line, reparse, wptr,
									0);
		redraw_changed_super_hires_bank(0, start_line, reparse,
						wptr + pixels_per_line, 0);
	} else {
		bank = 1;
		if(filt_stat & ALL_STAT_VOC_MAIN) {
			bank = 0;		// VOC SHR in main memory
		}
		redraw_changed_super_hires_bank(bank, start_line, reparse, wptr,
							pixels_per_line);
	}
}

void
video_copy_changed2()
{
	word32	*ch_ptr, *ch2_ptr;
	int	bank1_off;
	int	i;

	// Copy entries from g_slow_mem_changed[] to g_slow_mem_ch2[] and
	//  clear g_slow_mem_changed[]
	ch_ptr = &g_slow_mem_changed[0];
	ch2_ptr = &g_slow_mem_ch2[0];
	bank1_off = 0x10000 >> CHANGE_SHIFT;
	for(i = 4; i < 0xa0; i++) {		// Pages 0x0400 through 0x9fff
		ch2_ptr[i] = ch_ptr[i];
		ch2_ptr[i + bank1_off] = ch_ptr[i + bank1_off];
		ch_ptr[i] = 0;
		ch_ptr[i + bank1_off] = 0;
	}
}

void
video_update_event_line(int line)
{
	int	new_line;

	video_update_through_line(line);

	new_line = line + g_line_ref_amt;
	if(new_line < 200) {
		if(!g_config_control_panel && !g_halt_sim) {
			add_event_vid_upd(new_line);
		}
	} else if(line >= 262) {
		if(!g_config_control_panel && !g_halt_sim) {
			add_event_vid_upd(0);	/* add event for new screen */
		}
	}
}

void
video_force_reparse()
{
	word32	*wptr;
	int	height, width_full;
	int	i, j;

	g_video_stat_old_pos = 1;
	g_video_filt_stat_old[0].filt_stat = (word32)-1;
	height = g_video_act_margin_top + A2_WINDOW_HEIGHT +
						g_video_act_margin_bottom;
	height = MY_MIN(height, g_mainwin_kimage.a2_height);
	width_full = g_mainwin_kimage.a2_width_full;
	wptr = g_mainwin_kimage.wptr;
	for(i = 0; i < height; i++) {
		for(j = 0; j < width_full; j++) {
			*wptr++ = 0;
		}
	}
	g_border_reparse = 1;
}

void
video_update_through_line(int line)
{
	register word32 start_time;
	register word32 end_time;
	word32	my_start_lines, my_end_lines, prev_all_stat, next_all_stat;
	word32	prev_lines_since_vbl, next_lines_since_vbl;
	int	last_line, pos, last_pos, end, num;
	int	i;

#if 0
	vid_printf("\nvideo_upd for line %d, lines: %06x\n", line,
				get_lines_since_vbl(g_cur_dfcyc));
#endif

	GET_ITIMER(start_time);

	last_line = MY_MIN(200, line+1); /* go through line, but not past 200 */

	pos = g_video_save_all_stat_pos;
	last_pos = g_video_all_stat_pos;
	prev_all_stat = g_video_all_stat[pos].cur_all_stat;
	prev_lines_since_vbl = g_video_all_stat[pos].lines_since_vbl;
	g_video_all_stat[last_pos].cur_all_stat = g_cur_a2_stat;
	g_video_all_stat[last_pos].lines_since_vbl = (line + 1) << 8;
	next_all_stat = g_video_all_stat[pos+1].cur_all_stat;
	next_lines_since_vbl = g_video_all_stat[pos+1].lines_since_vbl;
	for(i = g_vid_update_last_line; i < last_line; i++) {
		// We need to step through pos in g_video_all_stat[] and find
		//  the start/end pairs for each line
		g_a2_line_left_edge[i] = 640;
		g_a2_line_right_edge[i] = 0;
		my_start_lines = (i << 8) + 25;
		my_end_lines = (i << 8) + 65;
		if(prev_lines_since_vbl > my_start_lines) {
			printf("prev:%08x > %08x start at i:%d\n",
				prev_lines_since_vbl, my_start_lines, i);
		}
		while(my_start_lines < my_end_lines) {
			while(next_lines_since_vbl <= my_start_lines) {
				// Step into next entry
				prev_all_stat = next_all_stat;
				prev_lines_since_vbl = next_lines_since_vbl;
				pos++;
				g_video_save_all_stat_pos = pos;
				next_all_stat =
					g_video_all_stat[pos+1].cur_all_stat;
				next_lines_since_vbl =
					g_video_all_stat[pos+1].lines_since_vbl;
				if(pos >= last_pos) {
					printf("FELL OFF %d %d!\n", pos,
								last_pos);
					pos--;
					break;
				}
			}
			end = 65;
			if(next_lines_since_vbl < my_end_lines) {
				end = (next_lines_since_vbl & 0xff);
				if(end < 25) {
					printf("i:%d next_lines_since_vbl:"
						"%08x!\n", i,
						next_lines_since_vbl);
					end = 25;
				}
			}
			video_do_partial_line(my_start_lines, end,
							prev_all_stat);
			my_start_lines = (i << 8) + end;
		}
	}


	g_vid_update_last_line = last_line;
	g_video_save_all_stat_pos = pos;

	/* deal with border and forming rects for xdriver.c to use */
	if(line >= 262) {
		if(g_num_lines_prev_superhires != g_num_lines_superhires) {
			/* switched in/out from superhires--refresh borders */
			g_border_sides_refresh_needed = 1;
		}

		video_form_change_rects();
		g_num_lines_prev_superhires = g_num_lines_superhires;
		g_num_lines_prev_superhires640 = g_num_lines_superhires640;
		g_num_lines_superhires = 0;
		g_num_lines_superhires640 = 0;

		num = g_video_filt_stat_pos;
		g_video_stat_old_pos = num;
		for(i = 0; i < num; i++) {
			g_video_filt_stat_old[i] = g_video_filt_stat[i];
		}
		g_video_filt_stat_pos = 0;
	}
	GET_ITIMER(end_time);
	g_cycs_in_refresh_line += (end_time - start_time);
}

extern word32 g_vbl_count;

void
video_do_partial_line(word32 lines_since_vbl, int end, word32 cur_all_stat)
{
	word32	filt_stat, old_filt_stat, line_bytes, old_line_bytes;
	int	pos, old_pos, reparse, line;

	pos = g_video_filt_stat_pos;
	old_pos = g_video_stat_old_pos;
	filt_stat = video_all_stat_to_filt_stat(lines_since_vbl >> 8,
								cur_all_stat);
	line_bytes = ((lines_since_vbl & 0x1ff00) << 8) |
			((end - 25) << 8) | ((lines_since_vbl - 25) & 0x3f);
	g_video_filt_stat[pos].line_bytes = line_bytes;
	g_video_filt_stat[pos].filt_stat = filt_stat;
	reparse = 1;
	old_filt_stat = (word32)-1;
	old_line_bytes = (word32)-1;
	if(pos < old_pos) {
		old_filt_stat = g_video_filt_stat_old[pos].filt_stat;
		old_line_bytes = g_video_filt_stat_old[pos].line_bytes;
	}
	if((old_filt_stat == filt_stat) && (line_bytes == old_line_bytes)) {
		reparse = 0;
	} else if((old_filt_stat ^ filt_stat) & ALL_STAT_SUPER_HIRES) {
		g_border_reparse = 1;
	}
	video_refresh_line(line_bytes, reparse, filt_stat);
	line = lines_since_vbl >> 8;
	if(line < 200) {
		g_a2_filt_stat[line] = filt_stat;
	} else {
		printf("partial_line %08x %d %08x out of range!\n",
					lines_since_vbl, end, cur_all_stat);
	}
	if((end <= 25) || (end < (int)(lines_since_vbl & 0xff))) {
		printf("Bad lsv:%08x, end:%d, stat:%08x\n", lines_since_vbl,
						end, filt_stat);
	}
	pos++;
	if(pos >= MAX_VIDEO_FILT_STAT) {
		pos--;
	}
	g_video_filt_stat_pos = pos;
}

void
video_refresh_line(word32 line_bytes, int must_reparse, word32 filt_stat)
{
	word32	*wptr;
	int	pixels_per_line, offset, line;

	line = line_bytes >> 16;
	if((word32)line >= 200) {
		printf("video_refresh %08x %d %08x!\n", line_bytes,
						must_reparse, filt_stat);
		return;
	}
	wptr = g_mainwin_kimage.wptr;
	pixels_per_line = g_mainwin_kimage.a2_width_full;
	offset = (pixels_per_line * g_video_act_margin_top) +
						g_video_act_margin_left;
	wptr = wptr + offset;

	if(filt_stat & ALL_STAT_SUPER_HIRES) {
		g_num_lines_superhires++;
		redraw_changed_super_hires(line_bytes, must_reparse, wptr,
						pixels_per_line, filt_stat);
	} else if(filt_stat & ALL_STAT_BORDER) {
		if(line < 192) {
			halt_printf("Border line not 192: %d\n", line);
		}
		g_a2_line_left_edge[line] = 0;
		g_a2_line_right_edge[line] = 560;
		if(g_border_line24_refresh_needed) {
			g_border_line24_refresh_needed = 0;
			g_a2_screen_buffer_changed |= (1 << 24);
		}
	} else if(filt_stat & ALL_STAT_TEXT) {
		redraw_changed_text(line_bytes, must_reparse, wptr,
						pixels_per_line, filt_stat);
	} else if(filt_stat & ALL_STAT_HIRES) {
		redraw_changed_hgr(line_bytes, must_reparse, wptr,
						pixels_per_line, filt_stat);
	} else {
		redraw_changed_gr(line_bytes, must_reparse, wptr,
						pixels_per_line, filt_stat);
	}
}

void
prepare_a2_font()
{
	word32	val0, val1, val2;
	int	i, j, k;

	// Prepare g_a2font_bits[char][line] where each entry indicates the
	//  set pixels in this line of the character.  Bits 6:0 are an
	//  80-column character, and bits 21:8 are  the 14 expanded bits of a
	//  40-columns character, both with the first visible bit at the
	//  rightmost bit address.  But g_font_array[] is in big-endian bit
	//  order, which is less useful
	for(i = 0; i < 256; i++) {
		for(j = 0; j < 8; j++) {
			val0 = g_font_array[i][j] >> 1;
			val1 = 0;		// 80-column bits
			val2 = 0;		// 40-column bits (doubled)
			for(k = 0; k < 7; k++) {
				val1 = val1 << 1;
				val2 = val2 << 2;
				if(val0 & 1) {
					val1 |= 1;
					val2 |= 3;
				}
				val0 = val0 >> 1;
			}
			g_a2font_bits[i][j] = (val2 << 8) | val1;
		}
	}
}

void
prepare_a2_romx_font(byte *font_ptr)
{
	word32	val0, val1, val2;
	int	i, j, k;

	// ROMX file
	for(i = 0; i < 256; i++) {
		for(j = 0; j < 8; j++) {
			val0 = font_ptr[i*8 + j];
			val1 = 0;		// 80-column bits
			val2 = 0;		// 40-column bits (doubled)
			for(k = 0; k < 7; k++) {
				val1 = val1 << 1;
				val2 = val2 << 2;
				if((val0 & 0x40) == 0) {
					val1 |= 1;
					val2 |= 3;
				}
				val0 = val0 << 1;
			}
			g_a2font_bits[i][j] = (val2 << 8) | val1;
		}
	}
}

void
video_add_rect(Kimage *kimage_ptr, int x, int y, int width, int height)
{
	int	pos;

	pos = kimage_ptr->num_change_rects++;
	if(pos >= MAX_CHANGE_RECTS) {
		return;			// This will be handled later
	}
	kimage_ptr->change_rect[pos].x = x;
	kimage_ptr->change_rect[pos].y = y;
	kimage_ptr->change_rect[pos].width = width;
	kimage_ptr->change_rect[pos].height = height;

	g_video_pixel_dcount += (width * height);
#if 0
	printf("Add rect %d, x:%d y:%d, w:%d h:%d\n", pos, x, y, width, height);
#endif
}

void
video_add_a2_rect(int start_line, int end_line, int left_pix, int right_pix)
{
	int	srcy;

	if((left_pix >= right_pix) || (left_pix < 0) || (right_pix <= 0)) {
		halt_printf("video_push_lines: lines %d to %d, pix %d to %d\n",
			start_line, end_line, left_pix, right_pix);
		printf("a2_screen_buf_ch:%08x, g_full_refr:%08x\n",
			g_a2_screen_buffer_changed, g_full_refresh_needed);
		return;
	}

	srcy = 2*start_line;

	video_add_rect(&g_mainwin_kimage, g_video_act_margin_left + left_pix,
			g_video_act_margin_top + srcy,
			(right_pix - left_pix), 2*(end_line - start_line));
}

void
video_form_change_rects()
{
	Kimage	*kimage_ptr;
	register word32 start_time;
	register word32 end_time;
	dword64	save_pixel_dcount;
	word32	mask;
	int	start, line, left_pix, right_pix, left, right, line_div8;
	int	x, y, width, height;

	kimage_ptr = &g_mainwin_kimage;
	if(g_border_sides_refresh_needed) {
		g_border_sides_refresh_needed = 0;
		// Add left side border
		video_add_rect(kimage_ptr, 0, g_video_act_margin_top,
						BORDER_WIDTH, A2_WINDOW_HEIGHT);

		// Add right-side border.  Resend x from 560 through
		//  X_A2_WINDOW_WIDTH
		x = g_video_act_margin_left + 560;
		width = X_A2_WINDOW_WIDTH - x;
		video_add_rect(kimage_ptr, x, g_video_act_margin_top, width,
							A2_WINDOW_HEIGHT);
	}
	if(g_border_special_refresh_needed) {
		g_border_special_refresh_needed = 0;

		// Do top border
		width = g_video_act_width;
		height = g_video_act_margin_top;
		video_add_rect(kimage_ptr, 0, 0, width, height);

		// Do bottom border
		height = g_video_act_margin_bottom;
		y = g_video_act_margin_top + A2_WINDOW_HEIGHT;
		video_add_rect(kimage_ptr, 0, y, width, height);
	}
	if(g_status_refresh_needed) {
		g_status_refresh_needed = 0;
		width = g_mainwin_kimage.a2_width;
		y = g_video_act_margin_top + A2_WINDOW_HEIGHT +
						g_video_act_margin_bottom;
		height = kimage_ptr->a2_height - y;
		if(height > 0) {
			save_pixel_dcount = g_video_pixel_dcount;
			video_add_rect(kimage_ptr, 0, y, width, height);
			g_video_pixel_dcount = save_pixel_dcount;
		}
	}

	if(g_a2_screen_buffer_changed == 0) {
		return;
	}

	GET_ITIMER(start_time);

	start = -1;

	left_pix = 640;
	right_pix = 0;

	for(line = 0; line < 200; line++) {
		line_div8 = line >> 3;
		mask = 1 << (line_div8);
		if((g_full_refresh_needed & mask) != 0) {
			left = 0;
			right = 640;
		} else {
			left = g_a2_line_left_edge[line];
			right = g_a2_line_right_edge[line];
		}

		if(!(g_a2_screen_buffer_changed & mask) || (left >= right)) {
			/* No need to update this line */
			/* Refresh previous chunks of lines, if any */
			if(start >= 0) {
				video_add_a2_rect(start, line, left_pix,
								right_pix);
				start = -1;
				left_pix = 640;
				right_pix = 0;
			}
		} else {
			/* Need to update this line */
			if(start < 0) {
				start = line;
			}
			left_pix = MY_MIN(left, left_pix);
			right_pix = MY_MAX(right, right_pix);
		}
	}

	if(start >= 0) {
		video_add_a2_rect(start, 200, left_pix, right_pix);
	}

	g_a2_screen_buffer_changed = 0;
	g_full_refresh_needed = 0;

	GET_ITIMER(end_time);

	g_cycs_in_xredraw += (end_time - start_time);
}

int
video_get_a2_width(Kimage *kimage_ptr)
{
	return kimage_ptr->a2_width;
}

int
video_get_x_width(Kimage *kimage_ptr)
{
	return kimage_ptr->x_width;
}

int
video_get_a2_height(Kimage *kimage_ptr)
{
	return kimage_ptr->a2_height;
}

int
video_get_x_height(Kimage *kimage_ptr)
{
	return kimage_ptr->x_height;
}

int
video_get_x_xpos(Kimage *kimage_ptr)
{
	return kimage_ptr->x_xpos;
}

int
video_get_x_ypos(Kimage *kimage_ptr)
{
	return kimage_ptr->x_ypos;
}

void
video_update_xpos_ypos(Kimage *kimage_ptr, int x_xpos, int x_ypos)
{
	x_xpos = video_clamp(x_xpos, 0, kimage_ptr->x_max_width - 640);
	x_ypos = video_clamp(x_ypos, 0, kimage_ptr->x_max_height - 420);
	kimage_ptr->x_xpos = x_xpos;
	kimage_ptr->x_ypos = x_ypos;
	if(kimage_ptr == &g_mainwin_kimage) {
		g_mainwin_xpos = x_xpos;
		g_mainwin_ypos = x_ypos;
		// printf("Set g_mainwin_xpos:%d, ypos:%d\n", x_xpos, x_ypos);
	}
}

int
video_change_aspect_needed(Kimage *kimage_ptr, int x_width, int x_height)
{
	// Return 1 if the passed in height, width do not match the kimage
	//  aspect-corrected version, and at least 2 VBL periods have passed

	if((kimage_ptr->vbl_of_last_resize + 6) > g_vbl_count) {
		return 0;
	}
	if((kimage_ptr->x_height != x_height) ||
					(kimage_ptr->x_width != x_width)) {
#if 0
		printf("change_aspect_needed, vbl:%d kimage width:%d height:%d "
			"but x width:%d height:%d\n", g_vbl_count,
			kimage_ptr->x_width, kimage_ptr->x_height,
			x_width, x_height);
#endif
		return 1;
	}
	return 0;
}

void
video_update_status_enable(Kimage *kimage_ptr)
{
	int	height, a2_height;

	height = g_video_act_margin_top + A2_WINDOW_HEIGHT +
						g_video_act_margin_bottom;
	a2_height = height;
	if(g_status_enable) {
		a2_height = kimage_ptr->a2_height_full;
	}
	kimage_ptr->a2_height = a2_height;
	height = (a2_height * kimage_ptr->scale_width_a2_to_x) >> 16;
	if(height > kimage_ptr->x_max_height) {
		height = kimage_ptr->x_max_height;
	}
	kimage_ptr->x_height = height;
#if 0
	printf("new a2_height:%d, x_height:%d\n", kimage_ptr->a2_height,
							kimage_ptr->x_height);
#endif
	//printf("Calling video_update_scale from video_update_status_en\n");
	video_update_scale(kimage_ptr, kimage_ptr->x_width, height, 0);
}

// video_out_query: return 0 if no screen drawing at all is needed.
//  returns 1 or the number of change_rects if any drawing is needed
int
video_out_query(Kimage *kimage_ptr)
{
	int	num_change_rects, x_refresh_needed;

	num_change_rects = kimage_ptr->num_change_rects;
	x_refresh_needed = kimage_ptr->x_refresh_needed;
	if(x_refresh_needed) {
		return 1;
	}
	return num_change_rects;
}

// video_out_done: used by specialize xdriver platform code which needs to
//  clear the num_change_rects=0.
void
video_out_done(Kimage *kimage_ptr)
{
	kimage_ptr->num_change_rects = 0;
	kimage_ptr->x_refresh_needed = 0;
}

// Called by xdriver.c to copy KEGS's kimage data to the vptr buffer
int
video_out_data(void *vptr, Kimage *kimage_ptr, int out_width_act,
		Change_rect *rectptr, int pos)
{
	word32	*out_wptr, *wptr;
	int	a2_width, a2_width_full, width, a2_height, height, x, eff_y;
	int	x_width, x_height, num_change_rects, x_refresh_needed;
	int	i, j;

	// Copy from kimage_ptr->wptr to vptr
	num_change_rects = kimage_ptr->num_change_rects;
	x_refresh_needed = kimage_ptr->x_refresh_needed;
	if(((pos >= num_change_rects) || (pos >= MAX_CHANGE_RECTS)) &&
							!x_refresh_needed) {
		kimage_ptr->num_change_rects = 0;
		return 0;
	}
	a2_width = kimage_ptr->a2_width;
	a2_width_full = kimage_ptr->a2_width_full;
	a2_height = kimage_ptr->a2_height;
	if((num_change_rects >= MAX_CHANGE_RECTS) || x_refresh_needed) {
		// Table overflow, just copy everything in one go
		kimage_ptr->x_refresh_needed = 0;
		if(pos >= 1) {
			kimage_ptr->num_change_rects = 0;
			return 0;		// No more to do
		}
		// Force full update
		rectptr->x = 0;
		rectptr->y = 0;
		rectptr->width = a2_width;
		rectptr->height = a2_height;
	} else {
		*rectptr = kimage_ptr->change_rect[pos];	// Struct copy
	}
#if 0
	printf("video_out_data, %p rectptr:%p, pos:%d, x:%d y:%d w:%d h:%d, "
		"wptr:%p\n", vptr, rectptr, pos, rectptr->x,
		rectptr->y, rectptr->width, rectptr->height,
		kimage_ptr->wptr);
#endif

	width = rectptr->width;
	height = rectptr->height;
	x = rectptr->x;
	x_width = kimage_ptr->x_width;
	x_height = kimage_ptr->x_height;
	if(!g_video_no_scale_window &&
			((a2_width != x_width) || (a2_height != x_height))) {
#if 0
		printf("a2_width:%d, x_width:%d, a2_height:%d, x_height:"
			"%d\n", a2_width, x_width, a2_height, x_height);
#endif
		return video_out_data_scaled(vptr, kimage_ptr, out_width_act,
								rectptr);
	} else {
		out_wptr = (word32 *)vptr;
		for(i = 0; i < height; i++) {
			eff_y = rectptr->y + i;
			wptr = kimage_ptr->wptr + (eff_y * a2_width_full) + x;
			out_wptr = ((word32 *)vptr) +
						(eff_y * out_width_act) + x;
			for(j = 0; j < width; j++) {
				*out_wptr++ = *wptr++;
			}
		}
	}

	return 1;
}


int
video_out_data_intscaled(void *vptr, Kimage *kimage_ptr, int out_width_act,
					Change_rect *rectptr)
{
	word32	*out_wptr, *wptr;
	word32	pos_scale, alpha_mask;
	int	a2_width_full, eff_y, src_y, x, y, new_x, out_x, out_y;
	int	out_width, out_height, max_x, max_y, out_max_x, out_max_y, pos;
	int	i, j;

	// Faster scaling routine which does simple pixel replication rather
	//  than blending.  Intended for scales >= 3.0 (or so) since at
	//  these scales, replication looks fine.
	x = rectptr->x;
	y = rectptr->y;
	max_x = rectptr->width + x;
	max_y = rectptr->height + y;
	max_x = MY_MIN(kimage_ptr->a2_width_full, max_x + 1);
	max_y = MY_MIN(kimage_ptr->a2_height, max_y + 1);
	x = MY_MAX(0, x - 1);
	y = MY_MAX(0, y - 1);
	a2_width_full = kimage_ptr->a2_width_full;

	out_x = (x * kimage_ptr->scale_width_a2_to_x) >> 16;
	out_y = (y * kimage_ptr->scale_height_a2_to_x) >> 16;
	out_max_x = (max_x * kimage_ptr->scale_width_a2_to_x + 65535) >> 16;
	out_max_y = (max_y * kimage_ptr->scale_height_a2_to_x + 65535) >> 16;
	out_max_x = MY_MIN(out_max_x, out_width_act);
	out_max_y = MY_MIN(out_max_y, kimage_ptr->x_height);
	out_width = out_max_x - out_x;
	out_height = out_max_y - out_y;
	out_wptr = (word32 *)vptr;
	rectptr->x = out_x;
	rectptr->y = out_y;
	rectptr->width = out_width;
	rectptr->height = out_height;
	alpha_mask = g_alpha_mask;
	for(i = 0; i < out_height; i++) {
		eff_y = out_y + i;
		pos_scale = kimage_ptr->scale_height[eff_y];
		src_y = pos_scale >> 16;
		wptr = kimage_ptr->wptr + (src_y * a2_width_full);
		out_wptr = ((word32 *)vptr) + (eff_y * out_width_act) + out_x;
		for(j = 0; j < out_width; j++) {
			new_x = j + out_x;
			pos_scale = kimage_ptr->scale_width[new_x];
			pos = pos_scale >> 16;
			*out_wptr++ = wptr[pos] | alpha_mask;
		}
	}
	rectptr->width = kimage_ptr->x_width - rectptr->x;

	return 1;
}

int
video_out_data_scaled(void *vptr, Kimage *kimage_ptr, int out_width_act,
					Change_rect *rectptr)
{
	word32	*out_wptr, *wptr;
	dword64	dval0a, dval0b, dval1a, dval1b, dscale, dscale_y, dval;
	word32	new_val, pos_scale, alpha_mask;
	int	a2_width_full, eff_y, src_y, x, y, new_x, out_x, out_y;
	int	out_width, out_height, max_x, max_y, out_max_x, out_max_y, pos;
	int	i, j;

	if((kimage_ptr->scale_width_a2_to_x >= 0x34000) ||
			(kimage_ptr->scale_height_a2_to_x >= 0x34000)) {
		return video_out_data_intscaled(vptr, kimage_ptr,
						out_width_act, rectptr);
	}
	x = rectptr->x;
	y = rectptr->y;
	max_x = rectptr->width + x;
	max_y = rectptr->height + y;
	max_x = MY_MIN(kimage_ptr->a2_width_full, max_x + 1);
	max_y = MY_MIN(kimage_ptr->a2_height, max_y + 1);
	x = MY_MAX(0, x - 1);
	y = MY_MAX(0, y - 1);
	a2_width_full = kimage_ptr->a2_width_full;

	out_x = (x * kimage_ptr->scale_width_a2_to_x) >> 16;
	out_y = (y * kimage_ptr->scale_height_a2_to_x) >> 16;
	out_max_x = (max_x * kimage_ptr->scale_width_a2_to_x + 65535) >> 16;
	out_max_y = (max_y * kimage_ptr->scale_height_a2_to_x + 65535) >> 16;
	out_max_x = MY_MIN(out_max_x, out_width_act);
	out_max_y = MY_MIN(out_max_y, kimage_ptr->x_height);
	out_width = out_max_x - out_x;
	out_height = out_max_y - out_y;
#if 0
	printf("scaled: in %d,%d %d,%d becomes %d,%d %d,%d\n", x, y, width,
		height, out_x, out_y, out_width, out_height);
#endif
	out_wptr = (word32 *)vptr;
	rectptr->x = out_x;
	rectptr->y = out_y;
	rectptr->width = out_width;
	rectptr->height = out_height;
	alpha_mask = g_alpha_mask;
	for(i = 0; i < out_height; i++) {
		eff_y = out_y + i;
		pos_scale = kimage_ptr->scale_height[eff_y];
		src_y = pos_scale >> 16;
		dscale_y = (pos_scale & 0xffff) >> 8;
		wptr = kimage_ptr->wptr + (src_y * a2_width_full);
		out_wptr = ((word32 *)vptr) + (eff_y * out_width_act) + out_x;
		for(j = 0; j < out_width; j++) {
			new_x = j + out_x;
			pos_scale = kimage_ptr->scale_width[new_x];
			pos = pos_scale >> 16;
			dscale = (pos_scale & 0xffff) >> 8;
			dval0a = wptr[pos];
			dval0a = (dval0a & 0x00ff00ffULL) |
					((dval0a & 0xff00ff00ULL) << 24);
			dval0b = wptr[pos + 1];
			dval0b = (dval0b & 0x00ff00ffULL) |
					((dval0b & 0xff00ff00ULL) << 24);
			dval1a = wptr[pos + a2_width_full];
			dval1a = (dval1a & 0x00ff00ffULL) |
					((dval1a & 0xff00ff00ULL) << 24);
			dval1b = wptr[pos + 1 + a2_width_full];
			dval1b = (dval1b & 0x00ff00ffULL) |
					((dval1b & 0xff00ff00ULL) << 24);
			dval0a = ((0x100 - dscale) * dval0a) +
					(dscale * dval0b);
			dval1a = ((0x100 - dscale) * dval1a) +
					(dscale * dval1b);
			dval0a = (dval0a >> 8) & 0x00ff00ff00ff00ffULL;
			dval1a = (dval1a >> 8) & 0x00ff00ff00ff00ffULL;
			dval = ((0x100 - dscale_y) * dval0a) +
					(dscale_y * dval1a);
			new_val = ((dval >> 8) & 0x00ff00ffULL) |
				((dval >> 32) & 0xff00ff00ULL);
			*out_wptr++ = new_val | alpha_mask;
#if 0
			if((pos == 300) && (eff_y == 100)) {
				printf("x:%d pos:%d %08x.  %016llx,%016llx "
					"pos_sc:%08x, %08x\n", new_x, pos,
					new_val, dval0a, dval0b, pos_scale,
					wptr[pos]);
			}
#endif
		}
	}
	rectptr->width = kimage_ptr->x_width - rectptr->x;
#if 0
	for(i = 0; i < kimage_ptr->x_height; i++) {
		out_wptr = ((word32 *)vptr) + (i * out_width_act) +
						kimage_ptr->x_width - 1;
		*out_wptr = 0x00ff00ff;
# if 0
		for(j = 0; j < 10; j++) {
			if(*out_wptr != 0) {
				printf("out_wptr:%p is %08x at %d,%d\n",
					out_wptr, *out_wptr,
					out_width_act - 1 - j, i);
			}
			out_wptr--;
		}
# endif
	}
#endif

	return 1;
}

word32
video_scale_calc_frac(int pos, word32 max, word32 frac_inc, word32 frac_inc_inv)
{
	word32	frac, frac_to_next, new_frac;

	frac = pos * frac_inc;
	if(frac >= max) {
		return max;			// Clear frac bits
	}
	if(g_video_scale_algorithm == 2) {
		return frac & -65536;			// nearest neighbor
	}
	if(g_video_scale_algorithm == 1) {
		return frac;				// bilinear interp
	}
	// Do proper scaling.  fraction=0 means 100% this pixel, fraction=ffff
	//  means 99.99% the next pixel
	frac_to_next = frac_inc + (frac & 0xffff);
	if(frac_to_next < 65536) {
		frac_to_next = 0;
	}
	frac_to_next = (frac_to_next & 0xffff) * frac_inc_inv;
	frac_to_next = frac_to_next >> 16;
	new_frac = (frac & -65536) | (frac_to_next & 0xffff);
#if 0
	if((frac >= (30 << 16)) && (frac < (38 << 16))) {
		printf("scale %d (%02x) -> %08x (was %08x) %08x %08x\n",
			pos, pos, new_frac, frac, frac_inc, frac_inc_inv);
	}
#endif
	return new_frac;
}

void
video_update_scale(Kimage *kimage_ptr, int out_width, int out_height,
							int must_update)
{
	word32	frac_inc, frac_inc_inv, new_frac, max;
	int	a2_width, a2_height, exp_width, exp_height;
	int	i;

	out_width = video_clamp(out_width, 1, kimage_ptr->x_max_width);
	out_width = video_clamp(out_width, 1, MAX_SCALE_SIZE);

	out_height = video_clamp(out_height, 1, kimage_ptr->x_max_height);
	out_height = video_clamp(out_height, 1, MAX_SCALE_SIZE);

	a2_width = kimage_ptr->a2_width;
	a2_height = kimage_ptr->a2_height;
	kimage_ptr->vbl_of_last_resize = g_vbl_count;

	// Handle aspect ratio.  Calculate height/width based on the other's
	//  aspect ratio, and pick the smaller value
	exp_width = (a2_width * out_height) / a2_height;
	exp_height = (a2_height * out_width) / a2_width;

	if(exp_width < a2_width) {
		exp_width = a2_width;
	}
	if(exp_height < a2_height) {
		exp_height = a2_height;
	}
	if(exp_width < out_width) {
		// Allow off-by-one to be OK, so window doesn't keep resizing
		if((exp_width + 1) != out_width) {
			out_width = exp_width;
		}
	}
	if(exp_height < out_height) {
		if((exp_height + 1) != out_height) {
			out_height = exp_height;
		}
	}
	if(out_width <= 0) {
		out_width = 1;
	}
	if(out_height <= 0) {
		out_height = 1;
	}

	// See if anything changed.  If it's unchanged, don't do anything
	if((kimage_ptr->x_width == out_width) && !must_update &&
			(kimage_ptr->x_height == out_height)) {
		return;
	}
	kimage_ptr->x_width = out_width;
	kimage_ptr->x_height = out_height;
	kimage_ptr->x_refresh_needed = 1;
	if(kimage_ptr == &g_mainwin_kimage) {
		g_mainwin_width = out_width;
		g_mainwin_height = out_height;
		//printf("Set g_mainwin_width=%d, g_mainwin_height=%d\n",
		//				out_width, out_height);
	}

	// the per-pixel inc = a2_width / out_width.  Scale by 65536
	frac_inc = (a2_width * 65536UL) / out_width;
	kimage_ptr->scale_width_to_a2 = frac_inc;
	frac_inc_inv = (out_width * 65536UL) / a2_width;
	kimage_ptr->scale_width_a2_to_x = frac_inc_inv;
#if 0
	printf("scale_width_to_a2: %08x, a2_to_x:%08x, is_debugwin:%d\n",
		kimage_ptr->scale_width_to_a2, kimage_ptr->scale_width_a2_to_x,
		(kimage_ptr == &g_debugwin_kimage));
#endif
	max = (a2_width - 1) << 16;
	for(i = 0; i < out_width + 1; i++) {
		new_frac = video_scale_calc_frac(i, max, frac_inc,
								frac_inc_inv);
		kimage_ptr->scale_width[i] = new_frac;
	}

	frac_inc = (a2_height * 65536UL) / out_height;
	kimage_ptr->scale_height_to_a2 = frac_inc;
	frac_inc_inv = (out_height * 65536UL) / a2_height;
	kimage_ptr->scale_height_a2_to_x = frac_inc_inv;
#if 0
	printf("scale_height_to_a2: %08x, a2_to_x:%08x. w:%d h:%d\n",
		kimage_ptr->scale_height_to_a2,
		kimage_ptr->scale_height_a2_to_x, out_width, out_height);
#endif
	max = (a2_height - 1) << 16;
	for(i = 0; i < out_height + 1; i++) {
		new_frac = video_scale_calc_frac(i, max, frac_inc,
								frac_inc_inv);
		kimage_ptr->scale_height[i] = new_frac;
	}
}

int
video_scale_mouse_x(Kimage *kimage_ptr, int raw_x, int x_width)
{
	int	x;

	// raw_x is in output coordinates.  Scale down to a2 coordinates
	if(x_width == 0) {
		x = (kimage_ptr->scale_width_to_a2 * raw_x) / 65536;
	} else {
		// Scale raw_x using x_width
		x = (raw_x * kimage_ptr->a2_width_full) / x_width;
	}
	x = x - BASE_MARGIN_LEFT;
	return x;
}

int
video_scale_mouse_y(Kimage *kimage_ptr, int raw_y, int y_height)
{
	int	y;

	// raw_y is in output coordinates.  Scale down to a2 coordinates
	if(y_height == 0) {
		y = (kimage_ptr->scale_height_to_a2 * raw_y) / 65536;
	} else {
		// Scale raw_y using y_height
		y = (raw_y * kimage_ptr->a2_height) / y_height;
	}
	y = y - BASE_MARGIN_TOP;
	return y;
}

int
video_unscale_mouse_x(Kimage *kimage_ptr, int a2_x, int x_width)
{
	int	x;

	// Convert a2_x to output coordinates
	x = a2_x + BASE_MARGIN_LEFT;
	if(x_width == 0) {
		x = (kimage_ptr->scale_width_a2_to_x * x) / 65536;
	} else {
		// Scale a2_x using x_width
		x = (x * x_width) / kimage_ptr->a2_width_full;
	}
	return x;
}

int
video_unscale_mouse_y(Kimage *kimage_ptr, int a2_y, int y_height)
{
	int	y;

	// Convert a2_y to output coordinates
	y = a2_y + BASE_MARGIN_TOP;
	if(y_height == 0) {
		y = (kimage_ptr->scale_height_a2_to_x * y) / 65536;
	} else {
		// Scale a2_y using y_height
		y = (y * y_height) / kimage_ptr->a2_height;
	}
	return y;
}

void
video_update_color_raw(int bank, int col_num, int a2_color)
{
	word32	tmp;
	int	red, green, blue, newred, newgreen, newblue;

	if(col_num >= 256 || col_num < 0) {
		halt_printf("video_update_color_raw: col: %03x\n", col_num);
		return;
	}

	red = (a2_color >> 8) & 0xf;
	green = (a2_color >> 4) & 0xf;
	blue = (a2_color) & 0xf;
	red = ((red << 4) + red);
	green = ((green << 4) + green);
	blue = ((blue << 4) + blue);

	newred = red >> g_red_right_shift;
	newgreen = green >> g_green_right_shift;
	newblue = blue >> g_blue_right_shift;

	tmp = ((newred & g_red_mask) << g_red_left_shift) +
			((newgreen & g_green_mask) << g_green_left_shift) +
			((newblue & g_blue_mask) << g_blue_left_shift);
	g_palette_8to1624[bank][col_num] = tmp;
}

void
video_update_status_line(int line, const char *string)
{
	byte	a2_str_buf[STATUS_LINE_LENGTH+1];
	word32	*wptr;
	char	*buf;
	const char *ptr;
	word32	line_bytes;
	int	start_line, c, pixels_per_line, offset;
	int	i;

	if(line >= MAX_STATUS_LINES || line < 0) {
		printf("update_status_line: line: %d!\n", line);
		exit(1);
	}

	ptr = string;
	buf = &(g_status_buf[line][0]);
	g_status_ptrs[line] = buf;
	for(i = 0; i < STATUS_LINE_LENGTH; i++) {
		if(*ptr) {
			c = *ptr++;
		} else {
			c = ' ';
		}
		buf[i] = c;
		a2_str_buf[i] = c | 0x80;
	}

	buf[STATUS_LINE_LENGTH] = 0;
	a2_str_buf[STATUS_LINE_LENGTH] = 0;
	start_line = (200 + 2*8) + line*8;
	pixels_per_line = g_mainwin_kimage.a2_width_full;
	offset = (pixels_per_line * g_video_act_margin_top);
	wptr = g_mainwin_kimage.wptr;
	wptr += offset;
	for(i = 0; i < 8; i++) {
		line_bytes = ((start_line + i) << 16) | (40 << 8) | 0;
		redraw_changed_string(&(a2_str_buf[0]), line_bytes, -1L,
			wptr, 0, 0x00ffffff, pixels_per_line, 1);
	}

	// Don't add rectangle here, video_form_change_rects will do it
	//video_add_a2_rect(start_line, start_line + 8, 0, 640);
}

void
video_draw_a2_string(int line, const byte *bptr)
{
	word32	*wptr;
	word32	line_bytes;
	int	start_line, pixels_per_line, offset;
	int	i;

	start_line = line*8;
	pixels_per_line = g_mainwin_kimage.a2_width_full;
	offset = (pixels_per_line * g_video_act_margin_top) +
					g_video_act_margin_left;
	wptr = g_mainwin_kimage.wptr;
	wptr += offset;
	for(i = 0; i < 8; i++) {
		line_bytes = ((start_line + i) << 16) | (40 << 8) | 0;
		redraw_changed_string(bptr, line_bytes, -1L,
			wptr, 0, 0x00ffffff, pixels_per_line, 1);
	}
	g_mainwin_kimage.x_refresh_needed = 1;
}

void
video_show_debug_info()
{
	word32	tmp1;

	printf("g_cur_dfcyc: %016llx, last_vbl: %016llx\n", g_cur_dfcyc,
							g_last_vbl_dfcyc);
	tmp1 = get_lines_since_vbl(g_cur_dfcyc);
	printf("lines since vbl: %06x\n", tmp1);
	printf("Last line updated: %d\n", g_vid_update_last_line);
}

word32
read_video_data(dword64 dfcyc)
{
	word32	val, val2;
	int	lines_since_vbl, line;

	// Return Charrom data at $C02C for SuperConvert 4 TDM mode
	lines_since_vbl = get_lines_since_vbl(dfcyc);
	val = float_bus_lines(dfcyc, lines_since_vbl);
	line = lines_since_vbl >> 8;
	if(line < 192) {
		// Always do the character ROM
		val2 = g_a2font_bits[val & 0xff][line & 7];
		dbg_log_info(dfcyc, val,
			(lines_since_vbl << 8) | (val2 & 0xff), 0xc02c);
		val = ~val2;		// Invert it, maybe
	}
	return val & 0xff;
}

word32
float_bus(dword64 dfcyc)
{
	word32	lines_since_vbl;

	lines_since_vbl = get_lines_since_vbl(dfcyc);
	return float_bus_lines(dfcyc, lines_since_vbl);
}

word32
float_bus_lines(dword64 dfcyc, word32 lines_since_vbl)
{
	word32	val;
	int	line, eff_line, line24, all_stat, byte_offset;
	int	hires, page2, addr;

/* For floating bus, model hires style: Visible lines 0-191 are simply the */
/* data being displayed at that time.  Lines 192-255 are lines 0 - 63 again */
/*  and lines 256-261 are lines 58-63 again */
/* For each line, figure out starting byte at -25 mod 128 bytes from this */
/*  line's start */
/* This emulates an Apple II style floating bus.  A real IIgs does not */
/*  drive anything meaningful during the 25 horizontal blanking cycles, */
/*  nor during veritical blanking.  The data seems to be 0 or related to */
/*  the instruction fetches on a real IIgs during blankings */

	line = lines_since_vbl >> 8;
	byte_offset = lines_since_vbl & 0xff;
	// byte offset is from 0 through 64, where the visible screen is drawn
	//  from 25 through 64

	eff_line = line;
	if(eff_line >= 0x100) {
		eff_line = (eff_line - 6) & 0xff;
	}
	if(byte_offset == 0) {
		byte_offset = 1;
	}
	all_stat = g_cur_a2_stat;
	hires = (all_stat & ALL_STAT_HIRES) && !(all_stat & ALL_STAT_TEXT);
	if((all_stat & ALL_STAT_MIX_T_GR) && (line >= 160)) {
		hires = 0;
	}
	page2 = EXTRU(all_stat, 31 - BIT_ALL_STAT_PAGE2, 1);
	if(all_stat & ALL_STAT_ST80) {
		page2 = 0;
	}

	line24 = (eff_line >> 3) & 0x1f;
	addr = g_screen_index[line24] & 0x3ff;
	addr = (addr & 0x380) + (((addr & 0x7f) - 25 + byte_offset) & 0x7f);
	if(hires) {
		addr = 0x2000 + addr + ((eff_line & 7) << 10) + (page2 << 13);
	} else {
		addr = 0x400 + addr + (page2 << 10);
	}

	val = g_slow_memory_ptr[addr];
#if 0
	printf("For %04x (%d) addr=%04x, val=%02x, dfcyc:%016llx\n",
		lines_since_vbl, eff_line, addr, val, dfcyc - g_last_vbl_dfcyc);
#endif
	dbg_log_info(dfcyc, ((lines_since_vbl >> 11) << 24) |
			(lines_since_vbl - 25), (addr << 8) | val, 0xff);

	return val;
}
