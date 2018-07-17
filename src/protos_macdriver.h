/*
  GSPLUS - Advanced Apple IIGS Emulator Environment
  Based on the KEGS emulator written by Kent Dickey
  See COPYRIGHT.txt for Copyright information
	See LICENSE.txt for license (GPL v2)
*/

void show_simple_alert(char *str1, char *str2, char *str3, int num);
void x_dialog_create_kegs_conf(const char *str);
int x_show_alert(int is_fatal, const char *str);void update_window(void);

void mac_update_modifiers(word32 state);
void mac_warp_mouse(void);
void check_input_events(void);
void temp_run_application_event_loop(void);
int main(int argc, char *argv[]);
void x_update_color(int col_num, int red, int green, int blue, word32 rgb);
void x_update_physical_colormap(void);
void show_xcolor_array(void);
void xdriver_end(void);
void x_get_kimage(Kimage *kimage_ptr);
void dev_video_init(void);
void x_redraw_status_lines(void);
void x_push_kimage(Kimage *kimage_ptr, int destx, int desty, int srcx, int srcy, int width, int height);
void x_push_done(void);
void x_auto_repeat_on(int must);
void x_auto_repeat_off(int must);
void x_hide_pointer(int do_hide);
void x_full_screen(int do_full);
void update_main_window_size(void);
