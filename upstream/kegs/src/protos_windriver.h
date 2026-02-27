/************************************************************************/
/*			KEGS: Apple //gs Emulator			*/
/*			Copyright 2002-2022 by Kent Dickey		*/
/*									*/
/*	This code is covered by the GNU GPL v3				*/
/*	See the file COPYING.txt or https://www.gnu.org/licenses/	*/
/*	This program is provided with no warranty			*/
/*									*/
/*	The KEGS web page is kegs.sourceforge.net			*/
/*	You may contact the author at: kadickey@alumni.princeton.edu	*/
/************************************************************************/

// $KmKId: protos_windriver.h,v 1.15 2023-05-17 22:37:57+00 kentd Exp $

/* END_HDR */

/* windriver.c */
Window_info *win_find_win_info_ptr(HWND hwnd);
void win_hide_pointer(Window_info *win_info_ptr, int do_hide);
int win_update_mouse(Window_info *win_info_ptr, int raw_x, int raw_y, int button_states, int buttons_valid);
void win_event_mouse(HWND hwnd, WPARAM wParam, LPARAM lParam);
void win_event_key(HWND hwnd, WPARAM wParam, LPARAM lParam, int down);
void win_event_redraw(HWND hwnd);
void win_event_destroy(HWND hwnd);
void win_event_size(HWND hwnd, WPARAM wParam, LPARAM lParam);
void win_event_minmaxinfo(HWND hwnd, LPARAM lParam);
void win_event_focus(HWND hwnd, int gain_focus);
LRESULT CALLBACK win_event_handler(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam);
int main(int argc, char **argv);
void check_input_events(void);
void win_video_init(int mdepth);
void win_init_window(Window_info *win_info_ptr, Kimage *kimage_ptr, char *name_str, int mdepth);
void win_create_window(Window_info *win_info_ptr);
void xdriver_end(void);
void win_resize_window(Window_info *win_info_ptr);
void x_update_display(Window_info *win_info_ptr);
void x_hide_pointer(int do_hide);
int opendir_int(DIR *dirp, const char *in_filename);
DIR *opendir(const char *in_filename);
struct dirent *readdir(DIR *dirp);
int closedir(DIR *dirp);
int lstat(const char *path, struct stat *bufptr);
int ftruncate(int fd, word32 length);



/* win32snd_driver.c */
void win32snd_init(word32 *shmaddr);
void win32snd_shutdown(void);
void CALLBACK handle_wav_snd(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2);
void check_wave_error(int res, char *str);
void child_sound_init_win32(void);
void win32snd_set_playing(int snd_playing);
void win32_send_audio2(byte *ptr, int size);
int win32_send_audio(byte *ptr, int in_size);


