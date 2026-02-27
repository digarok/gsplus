/************************************************************************/
/*			KEGS: Apple //gs Emulator			*/
/*			Copyright 2002-2023 by Kent Dickey		*/
/*									*/
/*	This code is covered by the GNU GPL v3				*/
/*	See the file COPYING.txt or https://www.gnu.org/licenses/	*/
/*	This program is provided with no warranty			*/
/*									*/
/*	The KEGS web page is kegs.sourceforge.net			*/
/*	You may contact the author at: kadickey@alumni.princeton.edu	*/
/************************************************************************/

const char rcsid_protos_x_h[] = "@(#)$KmKId: protos_xdriver.h,v 1.40 2023-06-16 19:32:26+00 kentd Exp $";

/* END_HDR */

/* xdriver.c */
int main(int argc, char **argv);
int my_error_handler(Display *display, XErrorEvent *ev);
void xdriver_end(void);
void x_try_xset_r(void);
void x_badpipe(int signum);
int kegs_x_io_error_handler(Display *display);
int x_video_get_mdepth(void);
int x_try_find_visual(int depth, int screen_num);
void x_video_init(void);
void x_init_window(Window_info *win_info_ptr, Kimage *kimage_ptr, char *name_str);
void x_create_window(Window_info *win_info_ptr);
int xhandle_shm_error(Display *display, XErrorEvent *event);
void x_allocate_window_data(Window_info *win_info_ptr);
void get_shm(Window_info *win_info_ptr, int width, int height);
void get_ximage(Window_info *win_info_ptr, int width, int height);
void x_set_size_hints(Window_info *win_info_ptr);
void x_resize_window(Window_info *win_info_ptr);
void x_update_display(Window_info *win_info_ptr);
Window_info *x_find_xwin(Window in_win);
void x_send_copy_data(Window_info *win_info_ptr);
void x_handle_copy(XSelectionRequestEvent *req_ev_ptr);
void x_handle_targets(XSelectionRequestEvent *req_ev_ptr);
void x_request_paste_data(Window_info *win_info_ptr);
void x_handle_paste(Window w, Atom property);
int x_update_mouse(Window_info *win_info_ptr, int raw_x, int raw_y, int button_states, int buttons_valid);
void x_input_events(void);
void x_hide_pointer(Window_info *win_info_ptr, int do_hide);
void x_handle_keysym(XEvent *xev_in);
int x_keysym_to_a2code(Window_info *win_info_ptr, int keysym, int is_up);
void x_update_modifier_state(Window_info *win_info_ptr, int state);
void x_auto_repeat_on(int must);
void x_auto_repeat_off(int must);
void x_full_screen(int do_full);


