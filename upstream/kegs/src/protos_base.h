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

#ifdef INCLUDE_RCSID_C
const char rcsid_protos_base_h[] = "@(#)$KmKId: protos_base.h,v 1.161 2024-09-15 13:56:41+00 kentd Exp $";
#endif

#ifdef __GNUC__
void halt_printf(const char *fmt, ...) __attribute__ ((
						__format__(printf, 1, 2)));
void cfg_err_printf(const char *pre_str, const char *fmt, ...) __attribute__ ((
						__format__(printf, 2, 3)));
void dynapro_error(Disk *dsk, const char *fmt, ...) __attribute__ ((
						__format__(printf, 2, 3)));
#endif

/* xdriver.c and macdriver.c and windriver.c */
int win_nonblock_read_stdin(int fd, char *bufptr, int len);

/* special scc_unixdriver.c prototypes */
void scc_serial_unix_open(int port);
void scc_serial_unix_close(int port);
void scc_serial_unix_change_params(int port);
void scc_serial_unix_fill_readbuf(dword64 dfcyc, int port, int space_left);
void scc_serial_unix_empty_writebuf(int port);

/* special scc_windriver.c prototypes */
void scc_serial_win_open(int port);
void scc_serial_win_close(int port);
void scc_serial_win_change_params(int port);
void scc_serial_win_fill_readbuf(dword64 dfcyc, int port, int space_left);
void scc_serial_win_empty_writebuf(int port);

/* special joystick_driver.c prototypes */
void joystick_init(void);
void joystick_update(dword64 dfcyc);
void joystick_update_buttons(void);

/* END_HDR */

/* adb.c */
int adb_get_hide_warp_info(Kimage *kimage_ptr, int *warpptr);
int adb_get_copy_requested(void);
void adb_nonmain_check(void);
void adb_init(void);
void adb_reset(void);
void adb_log(word32 addr, int val);
void show_adb_log(void);
void adb_error(void);
void adb_add_kbd_srq(void);
void adb_clear_kbd_srq(void);
void adb_add_data_int(void);
void adb_add_mouse_int(void);
void adb_clear_data_int(void);
void adb_clear_mouse_int(void);
void adb_send_bytes(int num_bytes, word32 val0, word32 val1, word32 val2);
void adb_send_1byte(word32 val);
void adb_response_packet(int num_bytes, word32 val);
void adb_kbd_reg0_data(int a2code, int is_up);
void adb_kbd_talk_reg0(void);
void adb_set_config(word32 val0, word32 val1, word32 val2);
void adb_set_new_mode(word32 val);
int adb_read_c026(void);
void adb_write_c026(int val);
void do_adb_cmd(void);
int adb_read_c027(void);
void adb_write_c027(int val);
int read_adb_ram(word32 addr);
void write_adb_ram(word32 addr, int val);
int adb_get_keypad_xy(int get_y);
int adb_update_mouse(Kimage *kimage_ptr, int x, int y, int button_states, int buttons_valid);
int mouse_read_c024(dword64 dfcyc);
void mouse_compress_fifo(dword64 dfcyc);
void adb_paste_update_state(void);
int adb_paste_add_buf(word32 key);
void adb_key_event(int a2code, int is_up);
word32 adb_read_c000(void);
word32 adb_access_c010(void);
word32 adb_read_c025(void);
int adb_is_cmd_key_down(void);
int adb_is_option_key_down(void);
void adb_increment_speed(void);
void adb_update_c025_mask(Kimage *kimage_ptr, word32 new_c025_val, word32 mask);
int adb_ascii_to_a2code(int unicode_c, int a2code, int *shift_down_ptr);
void adb_physical_key_update(Kimage *kimage_ptr, int raw_a2code, word32 unicode_c, int is_up);
void adb_maybe_virtual_key_update(int a2code, int is_up);
void adb_virtual_key_update(int a2code, int is_up);
void adb_kbd_repeat_off(void);
void adb_mainwin_focus(int has_focus);



/* engine_c.c */
word32 get_memory8_io_stub(word32 addr, byte *stat, dword64 *dcycs_ptr, dword64 dplus_x_m1);
word32 get_memory16_pieces_stub(word32 addr, byte *stat, dword64 *dcycs_ptr, Fplus *fplus_ptr, int in_bank);
word32 get_memory24_pieces_stub(word32 addr, byte *stat, dword64 *dcycs_ptr, Fplus *fplus_ptr, int in_bank);
void set_memory8_io_stub(word32 addr, word32 val, byte *stat, dword64 *dcycs_ptr, dword64 dplus_x_m1);
void set_memory16_pieces_stub(word32 addr, word32 val, dword64 *dcycs_ptr, dword64 dplus_1, dword64 dplus_x_m1, int in_bank);
void set_memory24_pieces_stub(word32 addr, word32 val, dword64 *dcycs_ptr, Fplus *fplus_ptr, int in_bank);
word32 get_memory_c(word32 addr);
word32 get_memory16_c(word32 addr);
word32 get_memory24_c(word32 addr);
void set_memory_c(word32 addr, word32 val, int do_log);
void set_memory16_c(word32 addr, word32 val, int do_log);
void set_memory24_c(word32 addr, word32 val);
word32 do_adc_sbc8(word32 in1, word32 in2, word32 psr, int sub);
word32 do_adc_sbc16(word32 in1, word32 in2, word32 psr, int sub);
void fixed_memory_ptrs_init(void);
word32 get_itimer(void);
void engine_recalc_events(void);
void set_halt_act(int val);
void clr_halt_act(void);
word32 get_remaining_operands(word32 addr, word32 opcode, word32 psr, dword64 *dcyc_ptr, Fplus *fplus_ptr);
int enter_engine(Engine_reg *engine_ptr);



/* clock.c */
double get_dtime(void);
int micro_sleep(double dtime);
void clk_bram_zero(void);
void clk_bram_set(int bram_num, int offset, int val);
void clk_setup_bram_version(void);
void clk_write_bram(FILE *fconf);
void update_cur_time(void);
void clock_update(void);
void clock_update_if_needed(void);
void clock_write_c034(word32 val);
void do_clock_data(void);



/* compile_time.c */



/* config.c */
int config_add_argv_override(const char *str1, const char *str2);
void config_set_config_kegs_name(const char *str1);
void config_init_menus(Cfg_menu *menuptr);
void config_init(void);
void cfg_find_config_kegs_file(void);
int config_setup_kegs_file(char *outname, int maxlen, const char **name_ptr);
int config_expand_path(char *out_ptr, const char *in_ptr, int maxlen);
char *cfg_exit(int get_status);
void cfg_err_vprintf(const char *pre_str, const char *fmt, va_list ap);
void cfg_err_printf(const char *pre_str, const char *fmt, ...);
void cfg_toggle_config_panel(void);
void cfg_set_config_panel(int panel);
char *cfg_text_screen_dump(int get_status);
char *cfg_text_screen_str(void);
char *cfg_get_serial0_status(int get_status);
char *cfg_get_serial1_status(int get_status);
char *cfg_get_current_copy_selection(void);
void config_vbl_update(int doit_3_persec);
void cfg_file_update_rom(const char *str);
void cfg_file_update_ptr(char **strptr, const char *str, int need_update);
void cfg_int_update(int *iptr, int new_val);
void cfg_load_charrom(void);
void config_load_roms(void);
void config_parse_config_kegs_file(void);
void cfg_parse_one_line(char *buf, int line);
void cfg_parse_bram(char *buf, int pos, int len);
void cfg_parse_sxdx(char *buf, int pos, int len);
void config_generate_config_kegs_name(char *outstr, int maxlen, Disk *dsk, int with_extras);
char *config_write_config_kegs_file(int get_status);
void insert_disk(int slot, int drive, const char *name, int ejected, const char *partition_name, int part_num, word32 dynamic_blocks);
dword64 cfg_detect_dc42(byte *bptr, dword64 dsize, dword64 exp_dsize, int slot);
dword64 cfg_get_fd_size(int fd);
dword64 cfg_read_from_fd(int fd, byte *bufptr, dword64 dpos, dword64 dsize);
dword64 cfg_write_to_fd(int fd, byte *bufptr, dword64 dpos, dword64 dsize);
int cfg_partition_maybe_add_dotdot(void);
int cfg_partition_name_check(const byte *name_ptr, int name_len);
int cfg_partition_read_block(int fd, void *buf, dword64 blk, int blk_size);
int cfg_partition_find_by_name_or_num(Disk *dsk, const char *in_partnamestr, int part_num);
int cfg_partition_make_list_from_name(const char *namestr);
int cfg_partition_make_list(int fd);
int cfg_maybe_insert_disk(int slot, int drive, const char *namestr);
void cfg_insert_disk_dynapro(int slot, int drive, const char *name);
int cfg_stat(char *path, struct stat *sb, int do_lstat);
word32 cfg_get_le16(byte *bptr);
word32 cfg_get_le32(byte *bptr);
dword64 cfg_get_le64(byte *bptr);
word32 cfg_get_be_word16(word16 *ptr);
word32 cfg_get_be_word32(word32 *ptr);
void cfg_set_le32(byte *bptr, word32 val);
void config_file_to_pipe(Disk *dsk, const char *cmd_ptr, const char *name_ptr);
void cfg_htab_vtab(int x, int y);
void cfg_home(void);
void cfg_cleol(void);
void cfg_putchar(int c);
void cfg_printf(const char *fmt, ...);
void cfg_print_dnum(dword64 dnum, int max_len);
int cfg_get_disk_name(char *outstr, int maxlen, int type_ext, int with_extras);
int cfg_get_disk_locked(int type_ext);
void cfg_parse_menu(Cfg_menu *menuptr, int menu_pos, int highlight_pos, int change);
void cfg_get_base_path(char *pathptr, const char *inptr, int go_up);
char *cfg_name_new_image(int get_status);
void cfg_dup_existing_image(word32 slotdrive);
void cfg_dup_image_selected(void);
void cfg_validate_image(word32 slotdrive);
void cfg_toggle_lock_disk(word32 slotdrive);
int cfg_create_new_image_act(const char *str, int type, int size_blocks);
void cfg_create_new_image(void);
void cfg_file_init(void);
void cfg_free_alldirents(Cfg_listhdr *listhdrptr);
void cfg_file_add_dirent_unique(Cfg_listhdr *listhdrptr, const char *nameptr, int is_dir, dword64 dsize, dword64 dimage_start, dword64 compr_dsize, int part_num);
void cfg_file_add_dirent(Cfg_listhdr *listhdrptr, const char *nameptr, int is_dir, dword64 dsize, dword64 dimage_start, dword64 compr_dsize, int part_num);
int cfg_dirent_sortfn(const void *obj1, const void *obj2);
int cfg_str_match(const char *str1, const char *str2, int len);
int cfg_str_match_maybecase(const char *str1, const char *str2, int len, int ignorecase);
int cfgcasecmp(const char *str1, const char *str2);
int cfg_strlcat(char *dstptr, const char *srcptr, int dstsize);
char *cfg_strncpy(char *dstptr, const char *srcptr, int dstsize);
const char *cfg_str_basename(const char *str);
char *cfg_strncpy_dirname(char *dstptr, const char *srcptr, int dstsize);
void cfg_file_readdir(const char *pathptr);
char *cfg_shorten_filename(const char *in_ptr, int maxlen);
void cfg_fix_topent(Cfg_listhdr *listhdrptr);
void cfg_file_draw(void);
void cfg_partition_select_all(void);
void cfg_partition_selected(void);
void cfg_file_selected(void);
void cfg_file_handle_key(int key);
void cfg_draw_menu(void);
void cfg_newdisk_pick_menu(word32 slotdrive);
int cfg_control_panel_update(void);
void cfg_edit_mode_key(int key);
int cfg_control_panel_update1(void);



/* debugger.c */
void debugger_init(void);
void check_breakpoints(word32 addr, dword64 dfcyc, word32 maybe_stack, word32 type);
void debug_hit_bp(word32 addr, dword64 dfcyc, word32 maybe_stack, word32 type, int pos);
int debugger_run_16ms(void);
void dbg_log_info(dword64 dfcyc, word32 info1, word32 info2, word32 type);
void debugger_update_list_kpc(void);
void debugger_key_event(Kimage *kimage_ptr, int a2code, int is_up);
void debugger_page_updown(int isup);
void debugger_redraw_screen(Kimage *kimage_ptr);
void debug_draw_debug_line(Kimage *kimage_ptr, int line, int vid_line);
void debugger_help(void);
void dbg_help_show_strs(int help_depth, const char *str, const char *help_str);
const char *debug_find_cmd_in_table(const char *line_ptr, Dbg_longcmd *longptr, int help_depth);
void do_debug_cmd(const char *in_str);
word32 dis_get_memory_ptr(word32 addr);
void show_one_toolset(FILE *toolfile, int toolnum, word32 addr);
void show_toolset_tables(word32 a2bank, word32 addr);
word32 debug_getnum(const char **str_ptr);
char *debug_get_filename(const char **str_ptr);
void debug_help(const char *str);
void debug_bp(const char *str);
void debug_bp_set(const char *str);
void debug_bp_clear(const char *str);
void debug_bp_clear_all(const char *str);
void debug_bp_setclr(const char *str, int is_set_clear);
void debug_soundfile(const char *cmd_str);
void debug_logpc(const char *str);
void debug_logpc_on(const char *str);
void debug_logpc_off(const char *str);
void debug_logpc_out_data(FILE *pcfile, Data_log *log_data_ptr, dword64 start_dcyc);
Data_log *debug_show_data_info(FILE *pcfile, Data_log *log_data_ptr, dword64 base_dcyc, dword64 dfcyc, dword64 start_dcyc, int *data_wrap_ptr, int *count_ptr);
void debug_logpc_save(const char *cmd_str);
void set_bp(word32 addr, word32 end_addr, word32 acc_type);
void show_bp(void);
void delete_bp(word32 addr, word32 end_addr);
void debug_iwm(const char *str);
void debug_iwm_check(const char *str);
int do_blank(int mode, int old_mode);
void do_go(void);
void do_step(void);
void xam_mem(int count);
void show_hex_mem(word32 startbank, word32 start, word32 end, int count);
void do_debug_list(void);
void dis_do_memmove(void);
void dis_do_pattern_search(void);
void dis_do_compare(void);
const char *do_debug_unix(const char *str, int old_mode);
void do_debug_load(void);
char *do_dis(word32 kpc, int accsize, int xsize, int op_provided, word32 instr, int *size_ptr);
int debug_get_view_line(int back);
int debug_add_output_line(char *in_str);
void debug_add_output_string(char *in_str, int len);
void debug_add_output_chars(char *str);
int dbg_printf(const char *fmt, ...);
int dbg_vprintf(const char *fmt, va_list args);
void halt_printf(const char *fmt, ...);
void halt2_printf(const char *fmt, ...);



/* scc.c */
void scc_init(void);
void scc_reset(void);
void scc_hard_reset_port(int port);
void scc_reset_port(int port);
void scc_regen_clocks(int port);
void scc_port_close(int port);
void scc_port_open(dword64 dfcyc, int port);
int scc_is_port_closed(dword64 dfcyc, int port);
char *scc_get_serial_status(int get_status, int port);
void scc_config_changed(int port, int cfg_changed, int remote_changed, int serial_dev_changed);
void scc_update(dword64 dfcyc);
void scc_try_to_empty_writebuf(dword64 dfcyc, int port);
void scc_try_fill_readbuf(dword64 dfcyc, int port);
void scc_do_event(dword64 dfcyc, int type);
void show_scc_state(void);
word32 scc_read_reg(dword64 dfcyc, int port);
void scc_write_reg(dword64 dfcyc, int port, word32 val);
word32 scc_read_data(dword64 dfcyc, int port);
void scc_write_data(dword64 dfcyc, int port, word32 val);
word32 scc_do_read_rr2b(void);
void scc_maybe_br_event(dword64 dfcyc, int port);
void scc_evaluate_ints(int port);
void scc_maybe_rx_event(dword64 dfcyc, int port);
void scc_maybe_rx_int(int port);
void scc_clr_rx_int(int port);
void scc_handle_tx_event(int port);
void scc_maybe_tx_event(dword64 dfcyc, int port);
void scc_clr_tx_int(int port);
void scc_set_zerocnt_int(int port);
void scc_clr_zerocnt_int(int port);
void scc_add_to_readbuf(dword64 dfcyc, int port, word32 val);
void scc_add_to_readbufv(dword64 dfcyc, int port, const char *fmt, ...);
void scc_transmit(dword64 dfcyc, int port, word32 val);
void scc_add_to_writebuf(dword64 dfcyc, int port, word32 val);



/* scc_socket_driver.c */
void scc_socket_open(dword64 dfcyc, int port, int cfg);
void scc_socket_close(int port);
void scc_socket_close_extended(dword64 dfcyc, int port, int allow_retry);
void scc_socket_maybe_open(dword64 dfcyc, int port, int must);
void scc_socket_open_incoming(dword64 dfcyc, int port);
void scc_socket_open_outgoing(dword64 dfcyc, int port, const char *remote_ip_str, int remote_port);
void scc_socket_make_nonblock(dword64 dfcyc, int port);
void scc_accept_socket(dword64 dfcyc, int port);
void scc_socket_telnet_reqs(dword64 dfcyc, int port);
void scc_socket_fill_readbuf(dword64 dfcyc, int port, int space_left);
void scc_socket_recvd_char(dword64 dfcyc, int port, int c);
void scc_socket_empty_writebuf(dword64 dfcyc, int port);
void scc_socket_modem_write(dword64 dfcyc, int port, int c);
void scc_socket_do_cmd_str(dword64 dfcyc, int port);
void scc_socket_send_modem_code(dword64 dfcyc, int port, int code);
void scc_socket_modem_connect(dword64 dfcyc, int port);
void scc_socket_modem_do_ring(dword64 dfcyc, int port);
void scc_socket_do_answer(dword64 dfcyc, int port);



/* scc_windriver.c */



/* scc_unixdriver.c */



/* iwm.c */
void iwm_init_drive(Disk *dsk, int smartport, int drive, int disk_525);
void iwm_init(void);
void iwm_reset(void);
void disk_set_num_tracks(Disk *dsk, int num_tracks);
word32 iwm_get_default_track_bits(Disk *dsk, word32 qtr_trk);
void draw_iwm_status(int line, char *buf);
void iwm_flush_cur_disk(void);
void iwm_flush_disk_to_unix(Disk *dsk);
void iwm_vbl_update(void);
void iwm_update_fast_disk_emul(int fast_disk_emul_en);
void iwm_show_stats(int slot_drive);
Disk *iwm_get_dsk(word32 state);
Disk *iwm_touch_switches(int loc, dword64 dfcyc);
void iwm_move_to_ftrack(Disk *dsk, word32 new_frac_track, int delta, dword64 dfcyc);
void iwm_move_to_qtr_track(Disk *dsk, word32 qtr_track);
void iwm525_update_phases(Disk *dsk, dword64 dfcyc);
void iwm525_update_head(Disk *dsk, dword64 dfcyc);
int iwm_read_status35(dword64 dfcyc);
void iwm_do_action35(dword64 dfcyc);
int read_iwm(int loc, dword64 dfcyc);
void write_iwm(int loc, int val, dword64 dfcyc);
int iwm_read_enable2(dword64 dfcyc);
int iwm_read_enable2_handshake(dword64 dfcyc);
void iwm_write_enable2(int val);
word32 iwm_fastemul_start_write(Disk *dsk, dword64 dfcyc);
word32 iwm_read_data_fast(Disk *dsk, dword64 dfcyc);
dword64 iwm_return_rand_data(Disk *dsk, dword64 dfcyc);
dword64 iwm_get_raw_bits(Disk *dsk, word32 bit_pos, int num_bits, dword64 *dsyncs_ptr);
word32 iwm_calc_bit_diff(word32 first, word32 last, word32 track_bits);
word32 iwm_calc_bit_sum(word32 bit_pos, int add_ival, word32 track_bits);
dword64 iwm_calc_forced_sync(dword64 dval, int forced_bit);
int iwm_calc_forced_sync_0s(dword64 sync_dval, int bits);
word32 iwm_read_data(Disk *dsk, dword64 dfcyc);
void iwm_write_data(Disk *dsk, word32 val, dword64 dfcyc);
void iwm_start_write(Disk *dsk, word32 bit_pos, word32 val, int no_prior);
int iwm_start_write_act(Disk *dsk, word32 bit_pos, int num, int no_prior, int delta);
void iwm_write_data35(Disk *dsk, word32 val, dword64 dfcyc);
void iwm_write_end(Disk *dsk, int write_through_now, dword64 dfcyc);
void iwm_write_one_nib(Disk *dsk, int bits, dword64 dfcyc);
void iwm_recalc_sync_from(Disk *dsk, word32 qtr_track, word32 bit_pos, dword64 dfcyc);
void sector_to_partial_nib(byte *in, byte *nib_ptr);
int disk_unnib_4x4(Disk *dsk);
int iwm_denib_track525(Disk *dsk, word32 qtr_track, byte *outbuf);
int iwm_denib_track35(Disk *dsk, word32 qtr_track, byte *outbuf);
int iwm_track_to_unix(Disk *dsk, word32 qtr_track, byte *outbuf);
void show_hex_data(byte *buf, int count);
void iwm_check_nibblization(dword64 dfcyc);
void disk_check_nibblization(Disk *dsk, byte *in_buf, int size, dword64 dfcyc);
void disk_unix_to_nib(Disk *dsk, int qtr_track, dword64 dunix_pos, word32 unix_len, int len_bits, dword64 dfcyc);
void iwm_nibblize_track_nib525(Disk *dsk, byte *track_buf, int qtr_track, word32 unix_len);
void iwm_nibblize_track_525(Disk *dsk, byte *track_buf, int qtr_track, dword64 dfcyc);
void iwm_nibblize_track_35(Disk *dsk, byte *track_buf, int qtr_track, word32 unix_len, dword64 dfcyc);
void disk_4x4_nib_out(Disk *dsk, word32 val);
void disk_nib_out(Disk *dsk, word32 val, int size);
void disk_nib_end_track(Disk *dsk, dword64 dfcyc);
word32 disk_nib_out_raw(Disk *dsk, word32 qtr_track, word32 val, int bits, word32 bit_pos, dword64 dfcyc);
Disk *iwm_get_dsk_from_slot_drive(int slot, int drive);
void iwm_toggle_lock(Disk *dsk);
void iwm_eject_named_disk(int slot, int drive, const char *name, const char *partition_name);
void iwm_eject_disk_by_num(int slot, int drive);
void iwm_eject_disk(Disk *dsk);
void iwm_show_track(int slot_drive, int track, dword64 dfcyc);
void iwm_show_a_track(Disk *dsk, Trk *trk, dword64 dfcyc);
void dummy1(word32 psr);
void dummy2(word32 psr);



/* joystick_driver.c */
void joystick_callback_init(int native_type);
void joystick_callback_update(word32 buttons, int paddle_x, int paddle_y);



/* moremem.c */
void fixup_brks(void);
void fixup_hires_on(void);
void fixup_bank0_2000_4000(void);
void fixup_bank0_0400_0800(void);
void fixup_any_bank_any_page(word32 start_page, int num_pages, byte *mem0rd, byte *mem0wr);
void fixup_intcx(void);
void fixup_st80col(dword64 dfcyc);
void fixup_altzp(void);
void fixup_page2(dword64 dfcyc);
void fixup_ramrd(void);
void fixup_ramwrt(void);
void fixup_lc(void);
void set_statereg(dword64 dfcyc, word32 val);
void fixup_shadow_txt1(void);
void fixup_shadow_txt2(void);
void fixup_shadow_hires1(void);
void fixup_shadow_hires2(void);
void fixup_shadow_shr(void);
void fixup_shadow_iolc(void);
void update_shadow_reg(dword64 dfcyc, word32 val);
void fixup_shadow_all_banks(void);
void setup_pageinfo(void);
void show_bankptrs_bank0rdwr(void);
void show_bankptrs(int bnk);
void show_addr(byte *ptr);
word32 moremem_fix_vector_pull(word32 addr);
word32 io_read(word32 loc, dword64 *cyc_ptr);
void io_write(word32 loc, word32 val, dword64 *cyc_ptr);
word32 slinky_devsel_read(dword64 dfcyc, word32 loc);
void slinky_devsel_write(dword64 dfcyc, word32 loc, word32 val);
word32 c3xx_read(dword64 dfcyc, word32 loc);
word32 get_lines_since_vbl(dword64 dfcyc);
int in_vblank(dword64 dfcyc);
int read_vid_counters(int loc, dword64 dfcyc);



/* paddles.c */
void paddle_fixup_joystick_type(void);
void paddle_trigger(dword64 dfcyc);
void paddle_trigger_mouse(dword64 dfcyc);
void paddle_trigger_keypad(dword64 dfcyc);
void paddle_update_trigger_dcycs(dword64 dfcyc);
int read_paddles(dword64 dfcyc, int paddle);
void paddle_update_buttons(void);



/* mockingboard.c */
void mock_ay8913_reset(int pair_num, dword64 dfcyc);
void mockingboard_reset(dword64 dfcyc);
void mock_show_pair(int pair_num, dword64 dfcyc, const char *str);
void mock_update_timers(int doit, dword64 dfcyc);
void mockingboard_event(dword64 dfcyc);
word32 mockingboard_read(word32 loc, dword64 dfcyc);
void mockingboard_write(word32 loc, word32 val, dword64 dfcyc);
word32 mock_6522_read(int pair_num, word32 loc, dword64 dfcyc);
void mock_6522_write(int pair_num, word32 loc, word32 val, dword64 dfcyc);
word32 mock_6522_new_ifr(dword64 dfcyc, int pair_num, word32 ifr, word32 ier);
void mock_ay8913_reg_read(int pair_num);
void mock_ay8913_reg_write(int pair_num, dword64 dfcyc);
void mock_ay8913_control_update(int pair_num, word32 new_val, word32 prev_val, dword64 dfcyc);
void mockingboard_show(int got_num, word32 disable_mask);



/* sim65816.c */
int sim_get_force_depth(void);
int sim_get_use_shmem(void);
void sim_set_use_shmem(int use_shmem);
word32 toolbox_debug_4byte(word32 addr);
void toolbox_debug_c(word32 xreg, word32 stack, dword64 *dcyc_ptr);
void show_toolbox_log(void);
word32 get_memory_io(word32 loc, dword64 *dcyc_ptr);
void set_memory_io(word32 loc, int val, dword64 *dcyc_ptr);
void show_regs_act(Engine_reg *eptr);
void show_regs(void);
void my_exit(int ret);
void do_reset(void);
byte *memalloc_align(int size, int skip_amt, void **alloc_ptr);
void memory_ptr_init(void);
int parse_argv(int argc, char **argv, int slashes_to_find);
int kegs_init(int mdepth, int screen_width, int screen_height, int no_scale_window);
void load_roms_init_memory(void);
void initialize_events(void);
void check_for_one_event_type(int type, word32 mask);
void add_event_entry(dword64 dfcyc, int type);
dword64 remove_event_entry(int type, word32 mask);
void add_event_stop(dword64 dfcyc);
void add_event_doc(dword64 dfcyc, int osc);
void add_event_scc(dword64 dfcyc, int type);
void add_event_vbl(void);
void add_event_vid_upd(int line);
void add_event_mockingboard(dword64 dfcyc);
void add_event_scan_int(dword64 dfcyc, int line);
dword64 remove_event_doc(int osc);
dword64 remove_event_scc(int type);
void remove_event_mockingboard(void);
void show_all_events(void);
void show_pmhz(void);
void setup_zip_speeds(void);
int run_16ms(void);
int run_a2_one_vbl(void);
void add_irq(word32 irq_mask);
void remove_irq(word32 irq_mask);
void take_irq(void);
void show_dtime_array(void);
void update_60hz(dword64 dfcyc, double dtime_now);
void do_vbl_int(void);
void do_scan_int(dword64 dfcyc, int line);
void check_scan_line_int(int cur_video_line);
void check_for_new_scan_int(dword64 dfcyc);
void scb_changed(dword64 dfcyc, word32 addr, word32 new_val, word32 old_val);
void init_reg(void);
void handle_action(word32 ret);
void do_break(word32 ret);
void do_cop(word32 ret);
void do_wdm(word32 arg);
void do_wai(void);
void do_stp(void);
void do_wdm_emulator_id(void);
void size_fail(int val, word32 v1, word32 v2);
int fatal_printf(const char *fmt, ...);
int kegs_vprintf(const char *fmt, va_list ap);
dword64 must_write(int fd, byte *bufptr, dword64 dsize);
void clear_fatal_logs(void);
char *kegs_malloc_str(const char *in_str);
dword64 kegs_lseek(int fd, dword64 offs, int whence);



/* smartport.c */
void smartport_error(void);
void smartport_log(word32 start_addr, word32 cmd, word32 rts_addr, word32 cmd_list);
void do_c70d(word32 arg0);
void do_c70a(word32 arg0);
int do_read_c7(int unit_num, word32 buf, word32 blk);
int do_write_c7(int unit_num, word32 buf, word32 blk);
int smartport_memory_write(Disk *dsk, byte *bufptr, dword64 doffset, word32 size);
int do_format_c7(int unit_num);
void do_c700(word32 ret);



/* doc.c */
void doc_init(void);
void doc_reset(dword64 dfcyc);
int doc_play(dword64 dfcyc, double last_dsamp, double dsamp_now, int num_samps, int snd_buf_init, int *outptr_start);
void doc_handle_event(int osc, dword64 dfcyc);
void doc_sound_end(int osc, int can_repeat, double eff_dsamps, double dsamps);
void doc_add_sound_irq(int osc);
void doc_remove_sound_irq(int osc, int must);
void doc_start_sound2(int osc, dword64 dfcyc);
void doc_start_sound(int osc, double eff_dsamps, double dsamps);
void doc_wave_end_estimate2(int osc, dword64 dfcyc);
void doc_wave_end_estimate(int osc, double eff_dsamps, double dsamps);
void doc_remove_sound_event(int osc);
void doc_write_ctl_reg(dword64 dfcyc, int osc, int val);
void doc_recalc_sound_parms(dword64 dfcyc, int osc);
int doc_read_c03c(void);
int doc_read_c03d(dword64 dfcyc);
void doc_write_c03c(dword64 dfcyc, word32 val);
void doc_write_c03d(dword64 dfcyc, word32 val);
void doc_show_ensoniq_state(void);



/* sound.c */
void sound_init(void);
void sound_set_audio_rate(int rate);
void sound_reset(dword64 dfcyc);
void sound_shutdown(void);
void sound_update(dword64 dfcyc);
void sound_file_start(char *filename);
void sound_file_open(void);
void sound_file_close(void);
void send_sound_to_file(word32 *wptr, int shm_pos, int num_samps, int real_samps);
void show_c030_state(dword64 dfcyc);
void show_c030_samps(dword64 dfcyc, int *outptr, int num);
int sound_play_c030(dword64 dfcyc, dword64 dsamp, int *outptr_start, int num_samps);
void sound_play(dword64 dfcyc);
void sound_mock_envelope(int pair, int *env_ptr, int num_samps, int *vol_ptr);
void sound_mock_noise(int pair, byte *noise_ptr, int num_samps);
void sound_mock_play(int pair, int channel, int *outptr, int *env_ptr, byte *noise_ptr, int *vol_ptr, int num_samps);
word32 sound_read_c030(dword64 dfcyc);
void sound_write_c030(dword64 dfcyc);



/* sound_driver.c */
void snddrv_init(void);
void sound_child_fork(int size);
void parent_sound_get_sample_rate(int read_fd);
void snddrv_shutdown(void);
void snddrv_send_sound(int real_samps, int size);
void child_sound_playit(word32 tmp);
void reliable_buf_write(word32 *shm_addr, int pos, int size);
void reliable_zero_write(int amt);
int child_send_samples(byte *ptr, int size);
void child_sound_loop(int read_fd, int write_fd, word32 *shm_addr);



/* woz.c */
void woz_crc_init(void);
word32 woz_calc_crc32(byte *bptr, dword64 dlen, word32 bytes_to_skip);
void woz_rewrite_crc(Disk *dsk, int min_write_size);
void woz_rewrite_lock(Disk *dsk);
void woz_check_file(Disk *dsk);
void woz_parse_meta(Disk *dsk, int offset, int size);
void woz_parse_info(Disk *dsk, int offset, int size);
void woz_parse_tmap(Disk *dsk, int offset, int size);
void woz_parse_trks(Disk *dsk, int offset, int size);
int woz_add_track(Disk *dsk, int qtr_track, word32 tmap, dword64 dfcyc);
int woz_parse_header(Disk *dsk);
Woz_info *woz_malloc(byte *wozptr, word32 woz_size);
int woz_reopen(Disk *dsk, dword64 dfcyc);
int woz_open(Disk *dsk, dword64 dfcyc);
byte *woz_append_bytes(byte *wozptr, byte *in_bptr, int len);
byte *woz_append_word32(byte *wozptr, word32 val);
int woz_append_chunk(Woz_info *wozinfo_ptr, word32 chunk_id, word32 length, byte *bptr);
byte *woz_append_a_trk(Woz_info *wozinfo_ptr, Disk *dsk, int trk_num, byte *bptr, word32 *num_blocks_ptr, dword64 *tmap_dptr);
Woz_info *woz_new_from_woz(Disk *dsk, int disk_525);
int woz_new(int fd, const char *str, int size_kb);
void woz_maybe_reparse(Disk *dsk);
void woz_set_reparse(Disk *dsk);
void woz_reparse_woz(Disk *dsk);
void woz_remove_a_track(Disk *dsk, word32 qtr_track);
word32 woz_add_a_track(Disk *dsk, word32 qtr_track);



/* unshk.c */
word32 unshk_get_long4(byte *bptr);
word32 unshk_get_word2(byte *bptr);
word32 unshk_calc_crc(byte *bptr, int size, word32 start_crc);
int unshk_unrle(byte *cptr, int len, word32 rle_delim, byte *ucptr);
void unshk_lzw_clear(Lzw_state *lzw_ptr);
byte *unshk_unlzw(byte *cptr, Lzw_state *lzw_ptr, byte *ucptr, word32 uclen);
void unshk_data(Disk *dsk, byte *cptr, word32 compr_size, byte *ucptr, word32 uncompr_size, word32 thread_format, byte *base_cptr);
void unshk_parse_header(Disk *dsk, byte *cptr, int compr_size, byte *base_cptr);
void unshk(Disk *dsk, const char *name_str);
void unshk_dsk_raw_data(Disk *dsk);



/* undeflate.c */
void *undeflate_realloc(void *ptr, dword64 dsize);
void *undeflate_malloc(dword64 dsize);
void show_bits(unsigned *llptr, int nl);
void show_huftb(unsigned *tabptr, int bits);
void undeflate_init_len_dist_tab(word32 *tabptr, dword64 drepeats, word32 start);
void undeflate_init_bit_rev_tab(word32 *tabptr, int num);
word32 undeflate_bit_reverse(word32 val, word32 bits);
word32 undeflate_calc_crc32(byte *bptr, word32 len);
byte *undeflate_ensure_dest_len(Disk *dsk, byte *ucptr, word32 len);
void undeflate_add_tab_code(word32 *tabptr, word32 tabsz_lg2, word32 code, word32 entry);
word32 *undeflate_init_fixed_tabs(void);
word32 *undeflate_init_tables(void);
void undeflate_free_tables(void);
void undeflate_check_bit_reverse(void);
word32 *undeflate_build_huff_tab(word32 *tabptr, word32 *entry_ptr, word32 len_size, word32 *bl_count_ptr, int max_bits);
word32 *undeflate_dynamic_table(byte *cptr, word32 *bit_pos_ptr, byte *cptr_base);
byte *undeflate_block(Disk *dsk, byte *cptr, word32 *bit_pos_ptr, byte *cptr_base, byte *cptr_end);
byte *undeflate_gzip_header(Disk *dsk, byte *cptr, word32 compr_size);
void undeflate_gzip(Disk *dsk, const char *name_str);
byte *undeflate_zipfile_blocks(Disk *dsk, byte *cptr, dword64 dcompr_size);
int undeflate_zipfile(Disk *dsk, int fd, dword64 dlocal_header_off, dword64 uncompr_dsize, dword64 compr_dsize);
int undeflate_zipfile_search(byte *bptr, byte *cmp_ptr, int size, int cmp_len, int min_size);
int undeflate_zipfile_make_list(int fd);



/* dynapro.c */
word32 dynapro_get_word32(byte *bptr);
word32 dynapro_get_word24(byte *bptr);
word32 dynapro_get_word16(byte *bptr);
void dynapro_set_word24(byte *bptr, word32 val);
void dynapro_set_word32(byte *bptr, word32 val);
void dynapro_set_word16(byte *bptr, word32 val);
void dynapro_error(Disk *dsk, const char *fmt, ...);
Dynapro_file *dynapro_alloc_file(void);
void dynapro_free_file(Dynapro_file *fileptr, int check_map);
void dynapro_free_recursive_file(Dynapro_file *fileptr, int check_map);
void dynapro_free_dynapro_info(Disk *dsk);
word32 dynapro_find_free_block_internal(Disk *dsk);
word32 dynapro_find_free_block(Disk *dsk);
byte *dynapro_malloc_file(char *path_ptr, dword64 *dsize_ptr, int extra_size);
void dynapro_join_path_and_file(char *outstr, const char *unix_path, const char *str, int path_max);
word32 dynapro_fill_fileptr_from_prodos(Disk *dsk, Dynapro_file *fileptr, char *buf32_ptr, word32 dir_byte);
word32 dynapro_diff_fileptrs(Dynapro_file *oldfileptr, Dynapro_file *newfileptr);
word32 dynapro_do_one_dir_entry(Disk *dsk, Dynapro_file *fileptr, Dynapro_file *localfile_ptr, char *buf32_ptr, word32 dir_byte);
void dynapro_fix_damaged_entry(Disk *dsk, Dynapro_file *fileptr);
void dynapro_try_fix_damage(Disk *dsk, Dynapro_file *fileptr);
void dynapro_try_fix_damaged_disk(Disk *dsk);
void dynapro_new_unix_path(Dynapro_file *fileptr, const char *path_str, const char *name_str);
Dynapro_file *dynapro_process_write_dir(Disk *dsk, Dynapro_file *parent_ptr, Dynapro_file **head_ptr_ptr, word32 dir_byte);
void dynapro_handle_write_dir(Disk *dsk, Dynapro_file *parent_ptr, Dynapro_file *head_ptr, word32 dir_byte);
word32 dynapro_process_write_file(Disk *dsk, Dynapro_file *fileptr);
void dynapro_handle_write_file(Disk *dsk, Dynapro_file *fileptr);
void dynapro_handle_changed_entry(Disk *dsk, Dynapro_file *fileptr);
word32 dynapro_write_to_unix_file(const char *unix_path, byte *data_ptr, word32 size);
void dynapro_unmap_file(Disk *dsk, Dynapro_file *fileptr);
void dynapro_unlink_file(Dynapro_file *fileptr);
void dynapro_erase_free_entry(Disk *dsk, Dynapro_file *fileptr);
void dynapro_erase_free_dir(Disk *dsk, Dynapro_file *fileptr);
void dynapro_mark_damaged(Disk *dsk, Dynapro_file *fileptr);
int dynapro_write(Disk *dsk, byte *bufptr, dword64 doffset, word32 size);
void dynapro_debug_update(Disk *dsk);
void dynapro_debug_map(Disk *dsk, const char *str);
void dynapro_debug_recursive_file_map(Dynapro_file *fileptr, int start);
word32 dynapro_unix_to_prodos_time(const time_t *time_ptr);
int dynapro_create_prodos_name(Dynapro_file *newfileptr, Dynapro_file *matchptr, word32 storage_type);
Dynapro_file *dynapro_new_unix_file(const char *path, Dynapro_file *parent_ptr, Dynapro_file *match_ptr, word32 storage_type);
int dynapro_create_dir(Disk *dsk, char *unix_path, Dynapro_file *parent_ptr, word32 dir_byte);
word32 dynapro_add_file_entry(Disk *dsk, Dynapro_file *fileptr, Dynapro_file *head_ptr, word32 dir_byte, word32 inc);
word32 dynapro_fork_from_unix(Disk *dsk, byte *fptr, word32 *storage_type_ptr, word32 key_block, dword64 dsize);
word32 dynapro_file_from_unix(Disk *dsk, Dynapro_file *fileptr);
word32 dynapro_prep_image(Disk *dsk, const char *dir_path, word32 num_blocks);
word32 dynapro_map_one_file_block(Disk *dsk, Dynapro_file *fileptr, word32 block_num, word32 file_offset, word32 eof);
word32 dynapro_map_file_blocks(Disk *dsk, Dynapro_file *fileptr, word32 block_num, int level, word32 file_offset, word32 eof);
word32 dynapro_map_file(Disk *dsk, Dynapro_file *fileptr, int do_file_data);
word32 dynapro_map_dir_blocks(Disk *dsk, Dynapro_file *fileptr);
word32 dynapro_build_map(Disk *dsk, Dynapro_file *fileptr);
int dynapro_mount(Disk *dsk, char *dir_path, word32 num_blocks);



/* dyna_type.c */
word32 dynatype_scan_extensions(const char *str);
word32 dynatype_find_prodos_type(const char *str);
const char *dynatype_find_file_type(word32 file_type);
word32 dynatype_detect_file_type(Dynapro_file *fileptr, const char *path_ptr, word32 storage_type);
int dynatype_get_extension(const char *str, char *out_ptr, int buf_len);
int dynatype_comma_arg(const char *str, word32 *type_or_aux_ptr);
void dynatype_fix_unix_name(Dynapro_file *fileptr, char *outbuf_ptr, int path_max);



/* dyna_filt.c */



/* dyna_validate.c */
word32 dynapro_validate_header(Disk *dsk, Dynapro_file *fileptr, word32 dir_byte, word32 parent_dir_byte);
void dynapro_validate_init_freeblks(byte *freeblks_ptr, word32 num_blocks);
word32 dynapro_validate_freeblk(Disk *dsk, byte *freeblks_ptr, word32 block);
word32 dynapro_validate_file(Disk *dsk, byte *freeblks_ptr, word32 block_num, word32 eof, int level_first);
word32 dynapro_validate_forked_file(Disk *dsk, byte *freeblks_ptr, word32 block_num, word32 eof);
word32 dynapro_validate_dir(Disk *dsk, byte *freeblks_ptr, word32 dir_byte, word32 parent_dir_byte, word32 exp_blocks_used);
int dynapro_validate_disk(Disk *dsk);
void dynapro_validate_any_image(Disk *dsk);



/* applesingle.c */
word32 applesingle_get_be32(const byte *bptr);
word32 applesingle_get_be16(const byte *bptr);
void applesingle_set_be32(byte *bptr, word32 val);
void applesingle_set_be16(byte *bptr, word32 val);
word32 applesingle_map_from_prodos(Disk *dsk, Dynapro_file *fileptr, int do_file_data);
word32 applesingle_from_unix(Disk *dsk, Dynapro_file *fileptr, byte *fptr, dword64 dsize);
word32 applesingle_make_prodos_fork(Disk *dsk, byte *fptr, byte *tptr, word32 length);



/* video.c */
void video_set_red_mask(word32 red_mask);
void video_set_green_mask(word32 green_mask);
void video_set_blue_mask(word32 blue_mask);
void video_set_alpha_mask(word32 alpha_mask);
void video_set_mask_and_shift(word32 x_mask, word32 *mask_ptr, int *shift_left_ptr, int *shift_right_ptr);
void video_set_palette(void);
void video_set_redraw_skip_amt(int amt);
Kimage *video_get_kimage(int win_id);
char *video_get_status_ptr(int line);
void video_set_x_refresh_needed(Kimage *kimage_ptr, int do_refresh);
int video_get_active(Kimage *kimage_ptr);
void video_set_active(Kimage *kimage_ptr, int active);
void video_init(int mdepth, int screen_width, int screen_height, int no_scale_window);
void video_init_kimage(Kimage *kimage_ptr, int width, int height, int screen_width, int screen_height);
void show_a2_line_stuff(void);
void video_reset(void);
void video_update(void);
word32 video_all_stat_to_filt_stat(int line, word32 new_all_stat);
void change_display_mode(dword64 dfcyc);
void video_add_new_all_stat(dword64 dfcyc, word32 lines_since_vbl);
void change_border_color(dword64 dfcyc, int val);
void update_border_info(void);
void update_border_line(int st_line_offset, int end_line_offset, int color);
void video_border_pixel_write(Kimage *kimage_ptr, int starty, int num_lines, int color, int st_off, int end_off);
word32 video_get_ch_mask(word32 mem_ptr, word32 filt_stat, int reparse);
void video_update_edges(int line, int left, int right, const char *str);
void redraw_changed_text(word32 line_bytes, int reparse, word32 *in_wptr, int pixels_per_line, word32 filt_stat);
void redraw_changed_string(const byte *bptr, word32 line_bytes, word32 ch_mask, word32 *in_wptr, word32 bg_pixel, word32 fg_pixel, int pixels_per_line, int dbl);
void redraw_changed_gr(word32 line_bytes, int reparse, word32 *in_wptr, int pixels_per_line, word32 filt_stat);
void video_hgr_line_segment(byte *slow_mem_ptr, word32 *wptr, int start_byte, int end_byte, int pixels_per_line, word32 filt_stat);
void redraw_changed_hgr(word32 line_bytes, int reparse, word32 *in_wptr, int pixels_per_line, word32 filt_stat);
int video_rebuild_super_hires_palette(int bank, word32 scan_info, int line, int reparse);
word32 redraw_changed_super_hires_oneline(int bank, word32 *in_wptr, int pixels_per_line, int y, int scan, word32 ch_mask);
void redraw_changed_super_hires_bank(int bank, int start_line, int reparse, word32 *wptr, int pixels_per_line);
void redraw_changed_super_hires(word32 line_bytes, int reparse, word32 *wptr, int pixels_per_line, word32 filt_stat);
void video_copy_changed2(void);
void video_update_event_line(int line);
void video_force_reparse(void);
void video_update_through_line(int line);
void video_do_partial_line(word32 lines_since_vbl, int end, word32 cur_all_stat);
void video_refresh_line(word32 line_bytes, int must_reparse, word32 filt_stat);
void prepare_a2_font(void);
void prepare_a2_romx_font(byte *font_ptr);
void video_add_rect(Kimage *kimage_ptr, int x, int y, int width, int height);
void video_add_a2_rect(int start_line, int end_line, int left_pix, int right_pix);
void video_form_change_rects(void);
int video_get_a2_width(Kimage *kimage_ptr);
int video_get_x_width(Kimage *kimage_ptr);
int video_get_a2_height(Kimage *kimage_ptr);
int video_get_x_height(Kimage *kimage_ptr);
int video_get_x_xpos(Kimage *kimage_ptr);
int video_get_x_ypos(Kimage *kimage_ptr);
void video_update_xpos_ypos(Kimage *kimage_ptr, int x_xpos, int x_ypos);
int video_change_aspect_needed(Kimage *kimage_ptr, int x_width, int x_height);
void video_update_status_enable(Kimage *kimage_ptr);
int video_out_query(Kimage *kimage_ptr);
void video_out_done(Kimage *kimage_ptr);
int video_out_data(void *vptr, Kimage *kimage_ptr, int out_width_act, Change_rect *rectptr, int pos);
int video_out_data_intscaled(void *vptr, Kimage *kimage_ptr, int out_width_act, Change_rect *rectptr);
int video_out_data_scaled(void *vptr, Kimage *kimage_ptr, int out_width_act, Change_rect *rectptr);
word32 video_scale_calc_frac(int pos, word32 max, word32 frac_inc, word32 frac_inc_inv);
void video_update_scale(Kimage *kimage_ptr, int out_width, int out_height, int must_update);
int video_scale_mouse_x(Kimage *kimage_ptr, int raw_x, int x_width);
int video_scale_mouse_y(Kimage *kimage_ptr, int raw_y, int y_height);
int video_unscale_mouse_x(Kimage *kimage_ptr, int a2_x, int x_width);
int video_unscale_mouse_y(Kimage *kimage_ptr, int a2_y, int y_height);
void video_update_color_raw(int bank, int col_num, int a2_color);
void video_update_status_line(int line, const char *string);
void video_draw_a2_string(int line, const byte *bptr);
void video_show_debug_info(void);
word32 read_video_data(dword64 dfcyc);
word32 float_bus(dword64 dfcyc);
word32 float_bus_lines(dword64 dfcyc, word32 lines_since_vbl);



/* voc.c */
word32 voc_devsel_read(word32 loc, dword64 dfcyc);
void voc_devsel_write(word32 loc, word32 val, dword64 dfcyc);
void voc_iosel_c300_write(word32 loc, word32 val, dword64 dfcyc);
void voc_reset(void);
word32 voc_read_reg0(dword64 dfcyc);
void voc_update_interlace(dword64 dfcyc);


