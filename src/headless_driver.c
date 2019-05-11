/*
   GSPLUS - Advanced Apple IIGS Emulator Environment
   Based on the KEGS emulator written by Kent Dickey
   See COPYRIGHT.txt for Copyright information
   See LICENSE.txt for license (GPL v2)
 */

#include "defc.h"
#include "glog.h"

#include <signal.h>


extern int get_byte_at_address(int addr);

static unsigned char fix_char(unsigned char c) {
  // inverse upper case
  if (c >= 0x00 && c <= 0x1f) return c + 0x40;

  // inverse special
  if (c >= 0x20 && c <= 0x3f) return c;

  // mouse text
  if (c >= 0x40 && c <= 0x5f) return ' ';

  // inverse lowercase
  if (c >= 0x60 && c <= 0x7f) return c;

  // uppercase normal
  if (c >= 0x80 && c <= 0x9f) return c - 0x40;
  

  // special characters, uppercase normal, lowercase normal.
  if (c >= 0xa0 && c <= 0xff) return c - 0x80;

  return ' ';
}


static void siginfo_handler(int signum) {

  static unsigned table[] = {
    0x0400,
    0x0480,
    0x0500,
    0x0580,
    0x0600,
    0x0680,
    0x0700,
    0x0780,
    0x0428,
    0x04A8,
    0x0528,
    0x05A8,
    0x0628,
    0x06A8,
    0x0728,
    0x07A8,
    0x0450,
    0x04D0,
    0x0550,
    0x05D0,
    0x0650,
    0x06D0,
    0x0750,
    0x07D0,
  };

  extern int g_cur_a2_stat;
  extern int get_byte_at_address(int addr);


  if (g_cur_a2_stat & ALL_STAT_SUPER_HIRES) return; // not text...

  int st80 = g_cur_a2_stat & ALL_STAT_VID80 /* ALL_STAT_ST80 */;

  //if (!(g_cur_a2_stat & ALL_STAT_TEXT)) return; // text is not enabled.

  char buffer[80+1];

  for (int row = 0; row < 24; ++row) {
    unsigned address = table[row];
    int column = 0;

    memset(buffer, ' ', sizeof(buffer));

    for(int j = 0; j < 40; ++j, ++address) {

      unsigned char c;

      if (st80) {
        c  = get_byte_at_address(0xe10000 + address);
        c = fix_char(c);
        buffer[column++] = c;
      }

      c  = get_byte_at_address(0xe00000 + address);
      c = fix_char(c);
      buffer[column++] = c;

    }
    if (st80) {
      buffer[80] = '\n';
      write(1, buffer, 81);
    } else {
      buffer[40] = '\n';
      write(1, buffer, 41);
    }
  }
  write(1, "\n\n", 2);
}

int
main(int argc, char **argv)
{

  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));

  sa.sa_flags = SA_RESTART;
  sa.sa_handler = siginfo_handler;

	#if defined(SIGINFO)
	/* control-T by default */

  sigaction(SIGINFO, &sa, NULL);
  //signal(SIGINFO, siginfo_handler);
  #endif

	//signal(SIGUSR1, siginfo_handler);
  sigaction(SIGUSR1, &sa, NULL);


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
