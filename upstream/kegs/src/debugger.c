const char rcsid_debugger_c[] = "@(#)$KmKId: debugger.c,v 1.60 2023-09-11 12:55:28+00 kentd Exp $";

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


#include <stdio.h>
#include <stdarg.h>
#include "defc.h"

#include "disas.h"

#define LINE_SIZE		160		/* Input buffer size */
#define PRINTF_BUF_SIZE		239
#define DEBUG_ENTRY_MAX_CHARS	80

STRUCT(Debug_entry) {
	byte str_buf[DEBUG_ENTRY_MAX_CHARS];
};

char g_debug_printf_buf[PRINTF_BUF_SIZE];
char g_debug_stage_buf[PRINTF_BUF_SIZE];
int	g_debug_stage_pos = 0;

Debug_entry *g_debug_lines_ptr = 0;
int	g_debug_lines_total = 0;
int	g_debug_lines_pos = 0;
int	g_debug_lines_alloc = 0;
int	g_debug_lines_max = 1024*1024;
int	g_debug_lines_view = -1;
int	g_debug_lines_viewable_lines = 20;
int	g_debugwin_changed = 1;
int	g_debug_to_stdout = 1;

extern byte *g_memory_ptr;
extern byte *g_slow_memory_ptr;
extern int g_halt_sim;
extern word32 g_c068_statereg;
extern word32 stop_run_at;
extern int Verbose;
extern int Halt_on;
extern int g_a2_key_to_ascii[][4];
extern Kimage g_debugwin_kimage;

extern int g_config_control_panel;
extern word32 g_mem_size_total;

extern char *g_sound_file_str;
extern word32 g_sound_file_bytes;

int	g_num_breakpoints = 0;
Break_point g_break_pts[MAX_BREAK_POINTS];

extern int g_irq_pending;

extern dword64 g_last_vbl_dcyc;
extern int g_ret1;
extern Engine_reg engine;
extern dword64 g_dcycles_end;

int g_stepping = 0;

word32	g_list_kpc;
int	g_hex_line_len = 0x10;
word32	g_a1 = 0;
word32	g_a2 = 0;
word32	g_a3 = 0;
word32	g_a4 = 0;
word32	g_a1bank = 0;
word32	g_a2bank = 0;
word32	g_a3bank = 0;
word32	g_a4bank = 0;

#define	MAX_CMD_BUFFER		229

#define PC_LOG_LEN		(2*1024*1024)

Pc_log g_pc_log_array[PC_LOG_LEN + 2];
Data_log g_data_log_array[PC_LOG_LEN + 2];

word32	g_log_pc_enable = 0;
Pc_log	*g_log_pc_ptr = &(g_pc_log_array[0]);
Pc_log	*g_log_pc_start_ptr = &(g_pc_log_array[0]);
Pc_log	*g_log_pc_end_ptr = &(g_pc_log_array[PC_LOG_LEN]);

Data_log *g_log_data_ptr = &(g_data_log_array[0]);
Data_log *g_log_data_start_ptr = &(g_data_log_array[0]);
Data_log *g_log_data_end_ptr = &(g_data_log_array[PC_LOG_LEN]);

char	g_cmd_buffer[MAX_CMD_BUFFER + 2] = { 0 };
int	g_cmd_buffer_len = 2;

#define MAX_DISAS_BUF		150
char g_disas_buffer[MAX_DISAS_BUF];

void
debugger_init()
{
	debugger_help();
	g_list_kpc = engine.kpc;
#if 0
	if(g_num_breakpoints == 0) {
		set_bp(0xff5a0e, 0xff5a0e, 4);
		set_bp(0x00c50a, 0x00c50a, 4);
		set_bp(0x00c50d, 0x00c50d, 4);
	}
#endif
}

int g_dbg_new_halt = 0;

void
check_breakpoints(word32 addr, dword64 dfcyc, word32 maybe_stack,
							word32 type)
{
	Break_point *bp_ptr;
	int	count;
	int	i;

	count = g_num_breakpoints;
	for(i = 0; i < count; i++) {
		bp_ptr = &(g_break_pts[i]);
		if((type & bp_ptr->acc_type) == 0) {
			continue;
		}
		if((addr >= (bp_ptr->start_addr & 0xffffff)) &&
				(addr <= (bp_ptr->end_addr & 0xffffff))) {
			debug_hit_bp(addr, dfcyc, maybe_stack, type, i);
		}
	}

	if((type == 4) && ((addr == 0xe10000) || (addr == 0xe10004))) {
		FINISH(RET_TOOLTRACE, 0);
	}
}

void
debug_hit_bp(word32 addr, dword64 dfcyc, word32 maybe_stack, word32 type,
								int pos)
{
	word32	trk_side, side, trk, cmd, unit, buf, blk, param_cnt, list_ptr;
	word32	status_code, cmd_list, stack, rts;

	if((addr == 0xff5a0e) && (type == 4)) {
		trk_side = get_memory_c(0xe10f32);
		side = (trk_side >> 5) & 1;
		trk = get_memory_c(0xe10f34) + ((trk_side & 0x1f) << 6);
		buf = get_memory_c(0x42) | (get_memory_c(0x43) << 8) |
						(get_memory_c(0x44) << 16);
		printf("ff5a0e: 3.5 read of track %03x side:%d sector:%03x to "
			"%06x at %016llx\n", trk, side, get_memory_c(0xe10f33),
			buf, dfcyc);
		return;
	}
	if((addr == 0x00c50a) && (type == 4)) {
		cmd = get_memory_c(0x42);
		unit = get_memory_c(0x43);
		buf = get_memory_c(0x44) | (get_memory_c(0x45) << 8);
		blk = get_memory_c(0x46) | (get_memory_c(0x47) << 8);
		printf("00c50a: cmd %02x u:%02x buf:%04x blk:%04x at %016llx\n",
			cmd, unit, buf, blk, dfcyc);
		return;
	}
	if((addr == 0x00c50d) && (type == 4)) {
		stack = maybe_stack & 0xffff;
		rts = get_memory_c(stack + 1) | (get_memory_c(stack + 2) << 8);
		cmd = get_memory_c(rts + 1);
		cmd_list = get_memory_c(rts + 2) | (get_memory_c(rts+3) << 8);
		param_cnt = get_memory_c(cmd_list);
		unit = get_memory_c(cmd_list + 1);
		list_ptr = get_memory_c(cmd_list + 2) |
					(get_memory_c(cmd_list + 3) << 8);
		status_code = get_memory_c(cmd_list + 4);
		printf("00c50d: stack:%04x rts:%04x cmd:%02x cmd_list:%04x "
			"param_cnt:%02x unit:%02x listptr:%04x "
			"status:%02x at %016llx\n", stack, rts, cmd, cmd_list,
			param_cnt, unit, list_ptr, status_code, dfcyc);
		printf("  list_ptr: %04x: %02x %02x %02x %02x %02x %02x %02x\n",
			list_ptr, get_memory_c(list_ptr),
			get_memory_c(list_ptr + 1), get_memory_c(list_ptr + 2),
			get_memory_c(list_ptr + 3), get_memory_c(list_ptr + 4),
			get_memory_c(list_ptr + 5), get_memory_c(list_ptr + 6));
		return;
	}

	dbg_log_info(dfcyc, addr, pos, 0x6270);
	halt2_printf("Hit breakpoint at %06x\n", addr);
}

int
debugger_run_16ms()
{
	// Called when g_halt_sim is set
	if(g_dbg_new_halt) {
		g_list_kpc = engine.kpc;
		show_regs();
	}
	g_dbg_new_halt = 0;
	adb_nonmain_check();
	// printf("debugger_run_16ms: g_halt_sim:%d\n", g_halt_sim);
	return 0;
}

void
dbg_log_info(dword64 dfcyc, word32 info1, word32 info2, word32 type)
{
	if(dfcyc == 0) {
		return;		// Ignore some IWM t:00e7 events and others
	}
	g_log_data_ptr->dfcyc = dfcyc;
	g_log_data_ptr->stat = 0;
	g_log_data_ptr->addr = info1;
	g_log_data_ptr->val = info2;
	g_log_data_ptr->size = type;		// type must be > 4
	g_log_data_ptr++;
	if(g_log_data_ptr >= g_log_data_end_ptr) {
		g_log_data_ptr = g_log_data_start_ptr;
	}
}

void
debugger_update_list_kpc()
{
	g_dbg_new_halt = 1;
}

void
debugger_key_event(Kimage *kimage_ptr, int a2code, int is_up)
{
	word32	c025_val, special;
	int	key, pos, changed;

	pos = 1;

	c025_val = kimage_ptr->c025_val;

	if(c025_val & 1) {		// Shift is down
		pos = 2;
	} else if(c025_val & 4) {	// Capslock is down
		key = g_a2_key_to_ascii[a2code][1];
		if((key >= 'a') && (key <= 'z')) {
			pos = 2;		// CAPS LOCK on
		}
	}
	if(c025_val & 2) {		// Ctrl is down
		pos = 3;
	}
	key = g_a2_key_to_ascii[a2code][pos];
	if(key < 0) {
		return;
	}
	special = (key >> 8) & 0xff;		// c025 changes
	if(is_up) {
		c025_val = c025_val & (~special);
	} else {
		c025_val = c025_val | special;
	}
	kimage_ptr->c025_val = c025_val;
	if(is_up) {
		return;		// Nothing else to do
	}
	if(key >= 0x80) {
		// printf("key: %04x\n", key);
		if(key == 0x8007) {			// F7 - close debugger
			video_set_active(kimage_ptr, !kimage_ptr->active);
			printf("Toggled debugger window to:%d\n",
						kimage_ptr->active);
		}
		if((key & 0xff) == 0x74) {		// Page up keycode
			debugger_page_updown(1);
		}
		if((key & 0xff) == 0x79) {		// Page down keycode
			debugger_page_updown(-1);
		}
		return;
	}
	pos = g_cmd_buffer_len;
	changed = 0;
	if((key >= 0x20) && (key < 0x7f)) {
		// printable character, add it
		if(pos < MAX_CMD_BUFFER) {
			// printf("cmd[%d]=%c\n", pos, key);
			g_cmd_buffer[pos++] = key;
			changed = 1;
		}
	} else if((key == 0x08) || (key == 0x7f)) {
		// Left arrow or backspace
		if(pos > 2) {
			pos--;
			changed = 1;
		}
	} else if((key == 0x0d) || (key == 0x0a)) {
		//dbg_printf("Did return, pos:%d, str:%s\n", pos, g_cmd_buffer);
		do_debug_cmd(&g_cmd_buffer[2]);
		pos = 2;
		changed = 1;
	} else {
		// printf("ctrl key:%04x\n", key);
	}
	g_cmd_buffer[pos] = 0;
	g_cmd_buffer_len = pos;
	g_debug_lines_view = -1;
	g_debugwin_changed |= changed;
	// printf("g_cmd_buffer: %s\n", g_cmd_buffer);
}

void
debugger_page_updown(int isup)
{
	int	view, max;

	view = g_debug_lines_view;
	if(view < 0) {
		view = 0;
	}
	view = view + (isup*g_debug_lines_viewable_lines);
	if(view < 0) {
		view = -1;
	}
	max = g_debug_lines_pos;
	if(g_debug_lines_alloc >= g_debug_lines_max) {
		max = g_debug_lines_alloc - 4;
	}
	view = MY_MIN(view, max - g_debug_lines_viewable_lines);

	// printf("new view:%d, was:%d\n", view, g_debug_lines_view);
	if(view != g_debug_lines_view) {
		g_debug_lines_view = view;
		g_debugwin_changed++;
	}
}

void
debugger_redraw_screen(Kimage *kimage_ptr)
{
	int	line, vid_line, back, border_top, save_pos, num, lines_done;
	int	save_view, save_to_stdout;
	int	i;

	if((g_debugwin_changed == 0) || (kimage_ptr->active == 0)) {
		return;				// Nothing to do
	}

	save_pos = g_debug_lines_pos;
	save_view = g_debug_lines_view;
	// printf("DEBUGGER drawing SCREEN!\n");
	g_cmd_buffer[0] = '>';
	g_cmd_buffer[1] = ' ';
	g_cmd_buffer[g_cmd_buffer_len] = 0xa0;		// Cursor: inverse space
	g_cmd_buffer[g_cmd_buffer_len+1] = 0;
	save_to_stdout = g_debug_to_stdout;
	g_debug_to_stdout = 0;
	dbg_printf("%s\n", &g_cmd_buffer[0]);
	g_cmd_buffer[g_cmd_buffer_len] = 0;
	dbg_printf("g_halt_sim:%02x\n", g_halt_sim);
	border_top = 8;
	g_debug_to_stdout = save_to_stdout;

	vid_line = (((kimage_ptr->a2_height - 2*border_top) / 16) * 8) - 1;
	num = g_debug_lines_pos - save_pos;
	if(num < 0) {
		num = num + g_debug_lines_alloc;
	}
	if(num > 4) {
		// printf("num is > 4!\n");
		num = 4;
	}
	for(i = 0; i < num; i++) {
		line = debug_get_view_line(i);
		debug_draw_debug_line(kimage_ptr, line, vid_line);
		vid_line -= 8;
	}
	g_debug_lines_pos = save_pos;
	g_debug_lines_view = save_view;
	back = save_view;
	if(back < 0) {			// -1 means always show most recent
		back = 0;
	}
	lines_done = 0;
	while(vid_line >= border_top) {
		line = debug_get_view_line(back);
		debug_draw_debug_line(kimage_ptr, line, vid_line);
		back++;
		vid_line -= 8;
		lines_done++;
#if 0
		printf(" did a line, line is now: %d after str:%s\n", line,
									str);
#endif
	}
	g_debug_lines_viewable_lines = lines_done;
	g_debugwin_changed = 0;
	kimage_ptr->x_refresh_needed = 1;
	// printf("x_refresh_needed = 1, viewable_lines:%d\n", lines_done);
}

void
debug_draw_debug_line(Kimage *kimage_ptr, int line, int vid_line)
{
	word32	line_bytes;
	int	i;

	// printf("draw debug line:%d at vid_line:%d\n", line, vid_line);
	for(i = 7; i >= 0; i--) {
		line_bytes = (vid_line << 16) | (40 << 8) | 0;
		redraw_changed_string(&(g_debug_lines_ptr[line].str_buf[0]),
			line_bytes, -1L, kimage_ptr->wptr + 8, 0, 0x00ffffff,
			kimage_ptr->a2_width_full, 1);
		vid_line--;
	}
}

Dbg_longcmd g_debug_bp_clear[] = {
	{ "all",	debug_bp_clear_all,	0,
					"clear all breakpoints" },
	{ 0, 0, 0, 0 }
};
Dbg_longcmd g_debug_bp[] = {
	{ "set",	debug_bp_set,	0,
					"Set breakpoint: ADDR or ADDR0-ADDR1" },
	{ "clear",	debug_bp_clear,	&g_debug_bp_clear[0],
				"Clear breakpoint: ADDR OR ADDR0-ADDR1"},
	{ 0, 0, 0, 0 }
};

Dbg_longcmd g_debug_logpc[] = {
	{ "on",		debug_logpc_on,	0, "Turn on logging of pc and data" },
	{ "off",	debug_logpc_off,0, "Turn off logging of pc and data" },
	{ "save",	debug_logpc_save,0, "logpc save FILE: save to file" },
	{ 0, 0, 0, 0 }
};

Dbg_longcmd g_debug_iwm[] = {
	{ "check",	debug_iwm_check, 0, "Denibblize current track" },
	{ 0, 0, 0, 0 }
};

// Main table of commands
Dbg_longcmd g_debug_longcmds[] = {
	{ "help",	debug_help,	0,	"Help" },
	{ "bp",		debug_bp,	&g_debug_bp[0],
					"bp ADDR: sets breakpoint on addr" },
	{ "logpc",	debug_logpc,	&g_debug_logpc[0], "Log PC" },
	{ "iwm",	debug_iwm,	&g_debug_iwm[0], "IWM" },
	{ "soundfile",	debug_soundfile, 0, "Save sound to a WAV file" },
	{ 0, 0, 0, 0 }
};

void
debugger_help()
{
	dbg_printf("KEGS Debugger help (courtesy Fredric Devernay\n");
	dbg_printf("General command syntax: [bank]/[address][command]\n");
	dbg_printf("e.g. 'e1/0010B' to set a breakpoint at the interrupt jump "
								"pt\n");
	dbg_printf("Enter all addresses using lower-case\n");
	dbg_printf("As with the IIgs monitor, you can omit the bank number "
								"after\n");
	dbg_printf("having set it: 'e1/0010B' followed by '14B' will set\n");
	dbg_printf("breakpoints at e1/0010 and e1/0014\n");
	dbg_printf("\n");
	dbg_printf("g                       Go\n");
	dbg_printf("[bank]/[addr]g          Go from [bank]/[address]\n");
	dbg_printf("s                       Step one instruction\n");
	dbg_printf("[bank]/[addr]s          Step one instr at [bank]/[addr]\n");
	dbg_printf("[bank]/[addr]B          Set breakpoint at [bank]/[addr]\n");
	dbg_printf("B                       Show all breakpoints\n");
	dbg_printf("[bank]/[addr]D          Delete breakpoint at [bank]/"
								"[addr]\n");
	dbg_printf("[bank]/[addr1].[addr2]  View memory\n");
	dbg_printf("[bank]/[addr]L          Disassemble memory\n");

	dbg_printf("Z                       Dump SCC state\n");
	dbg_printf("I                       Dump IWM state\n");
	dbg_printf("[drive].[track]I        Dump IWM state\n");
	dbg_printf("E                       Dump Ensoniq state\n");
	dbg_printf("[osc]E                  Dump oscillator [osc] state\n");
	dbg_printf("R                       Dump dtime array and events\n");
	dbg_printf("T                       Show toolbox log\n");
	dbg_printf("[bank]/[addr]T          Dump tools using ptr [bank]/"
								"[addr]\n");
	dbg_printf("                            as 'tool_set_info'\n");
	dbg_printf("[mode]V                 XOR verbose with 1=DISK, 2=IRQ,\n");
	dbg_printf("                         4=CLK,8=SHADOW,10=IWM,20=DOC,\n");
	dbg_printf("                         40=ABD,80=SCC, 100=TEST, 200="
								"VIDEO\n");
	dbg_printf("[mode]H                 XOR halt_on with 1=SCAN_INT,\n");
	dbg_printf("                         2=IRQ, 4=SHADOW_REG, 8="
							"C70D_WRITES\n");
	dbg_printf("r                       Reset\n");
	dbg_printf("[0/1]=m                 Changes m bit for l listings\n");
	dbg_printf("[0/1]=x                 Changes x bit for l listings\n");
	dbg_printf("S                       show_bankptr_bank0 & smartport "
								"errs\n");
	dbg_printf("P                       show_pmhz\n");
	dbg_printf("A                       show_a2_line_stuff show_adb_log\n");
	dbg_printf("Ctrl-e                  Dump registers\n");
	dbg_printf("[bank]/[addr1].[addr2]us[file]  Save mem area to [file]\n");
	dbg_printf("[bank]/[addr1].[addr2]ul[file]  Load mem area from "
								"[file]\n");
	dbg_printf("v                       Show video information\n");
	dbg_printf("q                       Exit Debugger (and KEGS)\n");
}

void
dbg_help_show_strs(int help_depth, const char *str, const char *help_str)
{
	const char *blank_str, *pre_str, *post_str;
	int	column, len, blank_len, pre_len, post_len;

	// Indent by 3*help_depth chars, then output str, then hit
	//  column 14, then output help_str.  This can be done in just 2-3
	//  lines, but I made it longer and clearer to avoid any "overflow"
	//  cases
	if(help_str == 0) {
		return;
	}
	blank_str = "        " "        " "        ";
	blank_len = (int)strlen(blank_str);		// should be >=17
	column = 17;
	len = (int)strlen(str);
	if(help_depth < 0) {
		help_depth = 0;
	}
	pre_str = blank_str;
	pre_len = 3 * help_depth;
	if(pre_len < blank_len) {
		pre_str = blank_str + blank_len - pre_len;
	}
	post_str = "";
	post_len = column - pre_len - len;
	if((post_len >= 1) && (post_len < blank_len)) {
		post_str = blank_str + blank_len - post_len;
	}
	dbg_printf("%s%s%s: %s\n", pre_str, str, post_str, help_str);
}

const char *
debug_find_cmd_in_table(const char *line_ptr, Dbg_longcmd *longptr,
							int help_depth)
{
	Dbg_fn	*fnptr;
	Dbg_longcmd *subptr;
	const char *str, *newstr;
	int	len, c;
	int	i;

	// See if the command is from the longcmd list
	while(*line_ptr == ' ') {
		line_ptr++;		// eat spaces
	}
	// Output "   str     :" where : is at column 14 always
	// printf("dfcit: %s, help_depth:%d\n", line_ptr, help_depth);
	for(i = 0; i < 1000; i++) {
		// Provide a limit to avoid hang if table not terminated right
		str = longptr[i].str;
		fnptr = longptr[i].fnptr;
		if(!str) {			// End of table
			break;			// No match found
		}
		if(help_depth < 0) {
			// Print the help string for all entries in this table
			dbg_help_show_strs(-1 - help_depth, str,
							longptr[i].help_str);
			continue;
		}
		len = (int)strlen(str);
		if(strncmp(line_ptr, str, len) != 0) {
			continue;		// Not a match
		}
		// Ensure next char is either a space, or 0
		// Let's us avoid commands which are prefixes, or
		//  which are old Apple II monitor hex+commands
		c = line_ptr[len];
		if((c != 0) && (c != ' ')) {
			continue;		// Not valid
		}
		if(help_depth) {
			dbg_help_show_strs(help_depth, str,
							longptr[i].help_str);
		}
		subptr = longptr[i].subptr;
		// Try a subcmd first
		newstr = line_ptr + len;
		if(subptr != 0) {
			if(help_depth) {
				help_depth++;
			}
			newstr = debug_find_cmd_in_table(newstr, subptr,
								help_depth);
			// If a subcmd was found, newstr is now 0
		}
		if((newstr == 0) || help_depth) {
			return 0;
		}
		if((newstr != 0) && (fnptr != 0)) {
			(*fnptr)(line_ptr + len);
			return 0;		// Success
		}
	}
	if(help_depth >= 1) {
		// No subcommands found, print out all entries in this table
		debug_find_cmd_in_table(line_ptr, longptr, -1 - help_depth);
		return 0;
	}
	return line_ptr;
}

void
do_debug_cmd(const char *in_str)
{
	const char *line_ptr;
	const char *newstr;
	int	slot_drive, track, ret_val, mode, old_mode, got_num;
	int	save_to_stdout;

	mode = 0;
	old_mode = 0;

	save_to_stdout = g_debug_to_stdout;
	g_debug_to_stdout = 1;
	dbg_printf("*%s\n", in_str);
	line_ptr = in_str;

	// See if the command is from the longcmd list
	newstr = debug_find_cmd_in_table(in_str, &(g_debug_longcmds[0]), 0);
	if(newstr == 0) {
		g_debug_to_stdout = save_to_stdout;
		return;			// Command found get out
	}

	// If we get here, parse an Apple II monitor like command:
	//  {address}{cmd} repeat.
	while(1) {
		ret_val = 0;
		g_a2 = 0;
		got_num = 0;
		while(1) {
			if((mode == 0) && (got_num != 0)) {
				g_a3 = g_a2;
				g_a3bank = g_a2bank;
				g_a1 = g_a2;
				g_a1bank = g_a2bank;
			}
			ret_val = *line_ptr++ & 0x7f;
			if((ret_val >= '0') && (ret_val <= '9')) {
				g_a2 = (g_a2 << 4) + ret_val - '0';
				got_num = 1;
				continue;
			}
			if((ret_val >= 'a') && (ret_val <= 'f')) {
				g_a2 = (g_a2 << 4) + ret_val - 'a' + 10;
				got_num = 1;
				continue;
			}
			if(ret_val == '/') {
				g_a2bank = g_a2;
				g_a2 = 0;
				continue;
			}
			break;
		}
		old_mode = mode;
		mode = 0;
		switch(ret_val) {
		case 'h':
			debugger_help();
			break;
		case 'R':
			show_dtime_array();
			show_all_events();
			break;
		case 'I':
			slot_drive = -1;
			track = -1;
			if(got_num) {
				if(old_mode == '.') {
					slot_drive = g_a1;
				}
				track = g_a2;
			}
			iwm_show_track(slot_drive, track, 0);
			iwm_show_stats(slot_drive);
			break;
		case 'E':
			doc_show_ensoniq_state();
			break;
		case 'T':
			if(got_num) {
				show_toolset_tables(g_a2bank, g_a2);
			} else {
				show_toolbox_log();
			}
			break;
		case 'v':
			if(got_num) {
				dis_do_compare();
			} else {
				video_show_debug_info();
			}
			break;
		case 'V':
			dbg_printf("g_irq_pending: %05x\n", g_irq_pending);
			dbg_printf("Setting Verbose ^= %04x\n", g_a1);
			Verbose ^= g_a1;
			dbg_printf("Verbose is now: %04x\n", Verbose);
			break;
		case 'H':
			dbg_printf("Setting Halt_on ^= %04x\n", g_a1);
			Halt_on ^= g_a1;
			dbg_printf("Halt_on is now: %04x\n", Halt_on);
			break;
		case 'r':
			do_reset();
			g_list_kpc = engine.kpc;
			break;
		case 'm':
			if(old_mode == '=') {
				if(!g_a1) {
					engine.psr &= ~0x20;
				} else {
					engine.psr |= 0x20;
				}
				if(engine.psr & 0x100) {
					engine.psr |= 0x30;
				}
			} else {
				dis_do_memmove();
			}
			break;
		case 'p':
			dis_do_pattern_search();
			break;
		case 'x':
			if(old_mode == '=') {
				if(!g_a1) {
					engine.psr &= ~0x10;
				} else {
					engine.psr |= 0x10;
				}
				if(engine.psr & 0x100) {
					engine.psr |= 0x30;
				}
			}
			break;
		case 'z':
			if(old_mode == '=') {
				stop_run_at = g_a1;
				dbg_printf("Calling add_event for t:%08x\n",
									g_a1);
				add_event_stop(((dword64)g_a1) << 16);
				dbg_printf("set stop_run_at = %x\n", g_a1);
			}
			break;
		case 'l': case 'L':
			if(got_num) {
				g_list_kpc = (g_a2bank << 16) + (g_a2 & 0xffff);
			}
			do_debug_list();
			break;
		case 'Z':
			show_scc_state();
			break;
		case 'S':
			show_bankptrs_bank0rdwr();
			smartport_error();
			break;
		case 'M':
			show_pmhz();
			mockingboard_show(got_num, g_a1);
			break;
		case 'A':
			show_a2_line_stuff();
			show_adb_log();
			break;
		case 's':
			g_stepping = 1;
			if(got_num) {
				engine.kpc = (g_a2bank << 16) + (g_a2 & 0xffff);
			}
			mode = 's';
			g_list_kpc = engine.kpc;
			break;
		case 'B':
			if(got_num) {
				dbg_printf("got_num:%d, a2bank:%x, g_a2:%x\n",
						got_num, g_a2bank, g_a2);
				set_bp((g_a2bank << 16) + g_a2,
						(g_a2bank << 16) + g_a2, 4);
			} else {
				show_bp();
			}
			break;
		case 'D':
			if(got_num) {
				dbg_printf("got_num: %d, a2bank: %x, a2: %x\n",
						got_num, g_a2bank, g_a2);
				delete_bp((g_a2bank << 16) + g_a2,
						(g_a2bank << 16) + g_a2);
			}
			break;
		case 'g':
		case 'G':
			dbg_printf("Going..\n");
			g_stepping = 0;
			if(got_num) {
				engine.kpc = (g_a2bank << 16) + (g_a2 & 0xffff);
			}
			do_go();
			g_list_kpc = engine.kpc;
			break;
		case 'u':
			dbg_printf("Unix commands\n");
			line_ptr = do_debug_unix(line_ptr, old_mode);
			break;
		case ':': case '.':
		case '+': case '-':
		case '=': case ',':
			mode = ret_val;
			dbg_printf("Setting mode = %x\n", mode);
			break;
		case ' ': case '\t':
			if(!got_num) {
				mode = old_mode;
				break;
			}
			mode = do_blank(mode, old_mode);
			break;
		case '<':
			g_a4 = g_a2;
			g_a4bank = g_a2bank;
			break;
		case 0x05: /* ctrl-e */
		case 'Q':
		case 'q':
			show_regs();
			break;
		case 0:			// The final null char
			if(old_mode == 's') {
				mode = do_blank(mode, old_mode);
				g_debug_to_stdout = save_to_stdout;
				return;
			}
			if(line_ptr == &in_str[1]) {
				g_a2 = g_a1 | (g_hex_line_len - 1);
				show_hex_mem(g_a1bank, g_a1, g_a2, -1);
				g_a1 = g_a2 + 1;
			} else {
				if((got_num == 1) || (mode == 's')) {
					mode = do_blank(mode, old_mode);
				}
			}
			g_debug_to_stdout = save_to_stdout;
			return;			// Get out, all done
			break;
		default:
			dbg_printf("\nUnrecognized command: %s\n", in_str);
			g_debug_to_stdout = save_to_stdout;
			return;
		}
	}
}

word32
dis_get_memory_ptr(word32 addr)
{
	word32	tmp1, tmp2, tmp3;

	tmp1 = get_memory_c(addr);
	tmp2 = get_memory_c(addr + 1);
	tmp3 = get_memory_c(addr + 2);

	return (tmp3 << 16) + (tmp2 << 8) + tmp1;
}

void
show_one_toolset(FILE *toolfile, int toolnum, word32 addr)
{
	word32	rout_addr;
	int	num_routs;
	int	i;

	num_routs = dis_get_memory_ptr(addr);
	fprintf(toolfile, "Tool 0x%02x, table: 0x%06x, num_routs:%03x\n",
		toolnum, addr, num_routs);
	if((addr < 0x10000) || (num_routs > 0x100)) {
		fprintf(toolfile, "addr in page 0, or num_routs too large\n");
		return;
	}

	for(i = 1; i < num_routs; i++) {
		rout_addr = dis_get_memory_ptr(addr + 4*i);
		fprintf(toolfile, "%06x = %02x%02x\n", rout_addr, i, toolnum);
	}
}

void
show_toolset_tables(word32 a2bank, word32 addr)
{
	FILE	*toolfile;
	word32	tool_addr;
	int	num_tools;
	int	i;

	addr = (a2bank << 16) + (addr & 0xffff);

	toolfile = fopen("tool_set_info", "w");
	if(toolfile == 0) {
		fprintf(stderr, "fopen of tool_set_info failed: %d\n", errno);
		exit(2);
	}

	num_tools = dis_get_memory_ptr(addr);
	fprintf(toolfile, "There are 0x%02x tools using ptr at %06x\n",
			num_tools, addr);

	if(num_tools > 40) {
		fprintf(toolfile, "Too many tools, aborting\n");
		num_tools = 0;
	}
	for(i = 1; i < num_tools; i++) {
		tool_addr = dis_get_memory_ptr(addr + 4*i);
		show_one_toolset(toolfile, i, tool_addr);
	}

	fclose(toolfile);
}

word32
debug_getnum(const char **str_ptr)
{
	const char *str;
	word32	val;
	int	c, got_num;

	str = *str_ptr;
	while(*str == ' ') {
		str++;
	}
	got_num = 0;
	val = 0;
	while(1) {
		c = tolower(*str);
		//printf("got c:%02x %c val was %08x got_num:%d\n", c, c, val,
		//						got_num);
		if((c >= '0') && (c <= '9')) {
			val = (val << 4) + (c - '0');
			got_num = 1;
		} else if((c >= 'a') && (c <= 'f')) {
			val = (val << 4) + 10 + (c - 'a');
			got_num = 1;
		} else {
			break;
		}
		str++;
	}
	*str_ptr = str;
	if(got_num) {
		return val;
	}
	return (word32)-1L;
}

char *
debug_get_filename(const char **str_ptr)
{
	const char *str, *start_str;
	char	*new_str;
	int	c, len;

	// Go to first whitespace (or end of str), then kegs_malloc_str()
	//  the string and copy to it
	str = *str_ptr;
	start_str = 0;
	//printf("get_filename, str now :%s:\n", str);
	while(1) {
		c = *str++;
		if(c == 0) {
			break;
		}
		if((c == ' ') || (c == '\t') || (c == '\n')) {
			//printf("c:%02x at str :%s: , start_str:%p\n", c, str,
			//					start_str);
			if(start_str) {
				break;
			}
			continue;
		}
		// Else it's a valid char, set start_str if needed
		if(!start_str) {
			start_str = str - 1;
			//printf("Got c:%02x, start_str :%s:\n", c, start_str);
		}
	}
	new_str = 0;
	if(start_str) {
		len = (int)(str - start_str);
		if(len > 1) {
			new_str = malloc(len);
			memcpy(new_str, start_str, len);
			new_str[len - 1] = 0;
		}
	}
	*str_ptr = str;
	return new_str;
}

void
debug_help(const char *str)
{
	dbg_printf("Help:\n");
	(void)debug_find_cmd_in_table(str, &(g_debug_longcmds[0]), 1);
}

void
debug_bp(const char *str)
{
	// bp without a following set/clear command.  Set a breakpoint if
	//  an address range follows, otherwise just print current breakpoints
	debug_bp_setclr(str, 0);
}

void
debug_bp_set(const char *str)
{
	debug_bp_setclr(str, 1);
}

void
debug_bp_clear(const char *str)
{
	debug_bp_setclr(str, 2);
}

void
debug_bp_clear_all(const char *str)
{
	if(str) {
		// Use str to avoid warning
	}
	if(g_num_breakpoints) {
		g_num_breakpoints = 0;
		setup_pageinfo();
		dbg_printf("Deleted all breakpoints\n");
	}
}

void
debug_bp_setclr(const char *str, int is_set_clear)
{
	word32	addr, end_addr, acc_type;

	printf("In debug_bp: %s\n", str);

	addr = debug_getnum(&str);
	// printf("getnum ret:%08x\n", addr);
	if(addr == (word32)-1L) {		// No argument
		show_bp();
		return;
	}
	end_addr = addr;
	if(*str == '-') {		// Range
		str++;
		end_addr = debug_getnum(&str);
		// printf("end_addr is %08x\n", end_addr);
		if(end_addr == (word32)-1L) {
			end_addr = addr;
		}
	}
	acc_type = 4;
	acc_type = debug_getnum(&str);
	if(acc_type == (word32)-1L) {
		acc_type = 4;			// Code breakpoint
	}
	if(is_set_clear == 2) {			// clear
		delete_bp(addr, end_addr);
	} else {				// set, or nothing
		set_bp(addr, end_addr, acc_type);
	}
}

void
debug_soundfile(const char *cmd_str)
{
	char	*str;

	// See if there's an argument
	str = debug_get_filename(&cmd_str);	// str=0 if no argument
	sound_file_start(str);			// str==0 means close file
}

void
debug_logpc(const char *str)
{
	if(str) {
		// Dummy use of argument
	}
	dbg_printf("logpc enable:%d, cur offset:%08lx\n", g_log_pc_enable,
			(long)(g_log_pc_ptr - g_log_pc_start_ptr));
}

void
debug_logpc_on(const char *str)
{
	if(str) {
		// Dummy use of argument
	}
	g_log_pc_enable = 1;
	g_dcycles_end = 0;
	dbg_printf("Enabled logging of PC and data accesses\n");
}

void
debug_logpc_off(const char *str)
{
	if(str) {
		// Dummy use of argument
	}
	g_log_pc_enable = 0;
	g_dcycles_end = 0;
	dbg_printf("Disabled logging of PC and data accesses\n");
}

void
debug_logpc_out_data(FILE *pcfile, Data_log *log_data_ptr, dword64 start_dcyc)
{
	char	*str, *shadow_str;
	dword64	lstat, offset64, offset64slow, addr64;
	word32	wstat, addr, size, val;

	addr = log_data_ptr->addr;
	lstat = (dword64)(log_data_ptr->stat);
	wstat = lstat & 0xff;
	addr64 = lstat - wstat + (addr & 0xff);
	offset64 = addr64 - (dword64)&(g_memory_ptr[0]);
	str = "IO";
	shadow_str = "";
	if((wstat & BANK_SHADOW) || (wstat & BANK_SHADOW2)) {
		shadow_str = "SHADOWED";
	}
	size = log_data_ptr->size;
	if(size > 32) {
		fprintf(pcfile, "INFO %08x %08x %04x t:%04x %lld.%02lld\n",
			log_data_ptr->addr, log_data_ptr->val, size >> 16,
			size & 0xffff, (log_data_ptr->dfcyc - start_dcyc)>>16,
			((log_data_ptr->dfcyc & 0xffff) * 100) >> 16);
	} else {
		offset64slow = addr64 - (dword64)&(g_slow_memory_ptr[0]);
		if(offset64 < g_mem_size_total) {
			str = "mem";
		} else if(offset64slow < 0x20000) {
			str = "slow_mem";
			offset64 = offset64slow;
		} else {
			str = "IO";
			offset64 = offset64 & 0xff;
		}
		val = log_data_ptr->val;
		fprintf(pcfile, "DATA set %06x = ", addr);
		if(size == 8) {
			fprintf(pcfile, "%02x (8) ", val & 0xff);
		} else if(size == 16) {
			fprintf(pcfile, "%04x (16) ", val & 0xffff);
		} else {
			fprintf(pcfile, "%06x (%d) ", val, size);
		}
		fprintf(pcfile, "%lld.%02lld, %s[%06llx] %s\n",
				(log_data_ptr->dfcyc - start_dcyc) >> 16,
				((log_data_ptr->dfcyc & 0xffff) * 100) >> 16,
				str, offset64 & 0xffffffULL, shadow_str);
	}
}

Data_log *
debug_show_data_info(FILE *pcfile, Data_log *log_data_ptr, dword64 base_dcyc,
			dword64 dfcyc, dword64 start_dcyc, int *data_wrap_ptr,
			int *count_ptr)
{

	while((*data_wrap_ptr < 2) && (log_data_ptr->dfcyc <= dfcyc) &&
					(log_data_ptr->dfcyc >= start_dcyc)) {
		if(*count_ptr >= PC_LOG_LEN) {
			break;
		}
		debug_logpc_out_data(pcfile, log_data_ptr, base_dcyc);
		if(log_data_ptr->dfcyc == 0) {
			break;
		}
		log_data_ptr++;
		(*count_ptr)++;
		if(log_data_ptr >= g_log_data_end_ptr) {
			log_data_ptr = g_log_data_start_ptr;
			(*data_wrap_ptr)++;
		}
	}
	return log_data_ptr;
}

void
debug_logpc_save(const char *cmd_str)
{
	FILE	*pcfile;
	Pc_log	*log_pc_ptr;
	Data_log *log_data_ptr;
	char	*str;
	dword64	dfcyc, start_dcyc, base_dcyc, max_dcyc;
	word32	instr, psr, acc, xreg, yreg, stack, direct, dbank, kpc, num;
	int	data_wrap, accsize, xsize, abs_time, data_count;
	int	i;

	// See if there's an argument
	num = debug_getnum(&cmd_str);
	abs_time = 1;
	if(num != (word32)-1L) {
		dbg_printf("Doing relative time\n");
		abs_time = 0;
	}

	pcfile = fopen("logpc_out", "w");
	if(pcfile == 0) {
		fprintf(stderr,"fopen failed...errno: %d\n", errno);
		exit(2);
	}

	log_pc_ptr = g_log_pc_ptr;
	log_data_ptr = g_log_data_ptr;
#if 0
	printf("debug_logpc_save called, log_pc_ptr:%p, %p,%p log_data_ptr:%p, "
		"%p,%p\n", log_pc_ptr, g_log_pc_start_ptr, g_log_pc_end_ptr,
		log_data_ptr, g_log_data_start_ptr, g_log_data_end_ptr);
#endif
#if 0
	fprintf(pcfile, "current pc_log_ptr: %p, start: %p, end: %p\n",
		log_pc_ptr, g_log_pc_start_ptr, g_log_pc_end_ptr);
#endif

	// See if we haven't filled buffer yet
	if(log_pc_ptr->dfcyc == 0) {
		log_pc_ptr = g_log_pc_start_ptr;
	}
	if(log_data_ptr->dfcyc == 0) {
		log_data_ptr = g_log_data_start_ptr;
		data_wrap = 1;
	}

	start_dcyc = log_pc_ptr->dfcyc;
	// Round to an exact usec
	start_dcyc = (start_dcyc >> 16) << 16;
	base_dcyc = start_dcyc;
	if(abs_time) {
		base_dcyc = 0;				// Show absolute time
	}
	dfcyc = start_dcyc;

	data_wrap = 0;
	data_count = 0;
	/* find first data entry */
	while((data_wrap < 2) && (log_data_ptr->dfcyc < dfcyc)) {
		log_data_ptr++;
		if(log_data_ptr >= g_log_data_end_ptr) {
			log_data_ptr = g_log_data_start_ptr;
			data_wrap++;
		}
	}
	fprintf(pcfile, "start_dcyc: %016llx, first entry:%016llx\n",
						start_dcyc, log_pc_ptr->dfcyc);

	dfcyc = start_dcyc;
	max_dcyc = dfcyc;
	for(i = 0; i < PC_LOG_LEN; i++) {
		dfcyc = log_pc_ptr->dfcyc;
		log_data_ptr = debug_show_data_info(pcfile, log_data_ptr,
				base_dcyc, dfcyc, start_dcyc,
				&data_wrap, &data_count);
		dbank = (log_pc_ptr->dbank_kpc >> 24) & 0xff;
		kpc = log_pc_ptr->dbank_kpc & 0xffffff;
		instr = log_pc_ptr->instr;
		psr = (log_pc_ptr->psr_acc >> 16) & 0xffff;
		acc = log_pc_ptr->psr_acc & 0xffff;
		xreg = (log_pc_ptr->xreg_yreg >> 16) & 0xffff;
		yreg = log_pc_ptr->xreg_yreg & 0xffff;
		stack = (log_pc_ptr->stack_direct >> 16) & 0xffff;
		direct = log_pc_ptr->stack_direct & 0xffff;

		accsize = 2;
		xsize = 2;
		if(psr & 0x20) {
			accsize = 1;
		}
		if(psr & 0x10) {
			xsize = 1;
		}

		str = do_dis(kpc, accsize, xsize, 1, instr, 0);
		fprintf(pcfile, "%06x] A:%04x X:%04x Y:%04x P:%03x "
			"S:%04x D:%04x B:%02x %lld.%02lld %s\n", i,
			acc, xreg, yreg, psr, stack, direct, dbank,
			(dfcyc - base_dcyc) >> 16,
			((dfcyc & 0xffff) * 100) >> 16, str);

		if((dfcyc == 0) && (i != 0)) {
			break;
		}
		max_dcyc = dfcyc;
		log_pc_ptr++;
		if(log_pc_ptr >= g_log_pc_end_ptr) {
			log_pc_ptr = g_log_pc_start_ptr;
		}
	}

	// Print any more DATA or INFO after last PC entry
	log_data_ptr = debug_show_data_info(pcfile, log_data_ptr,
			base_dcyc, max_dcyc + 10 * 65536, start_dcyc,
			&data_wrap, &data_count);

	fclose(pcfile);
}

void
set_bp(word32 addr, word32 end_addr, word32 acc_type)
{
	int	count;

	dbg_printf("About to set BP at %06x - %06x, type:%02x\n", addr,
							end_addr, acc_type);
	count = g_num_breakpoints;
	if(count >= MAX_BREAK_POINTS) {
		dbg_printf("Too many (0x%02x) breakpoints set!\n", count);
		return;
	}

	g_break_pts[count].start_addr = addr;
	g_break_pts[count].end_addr = end_addr;
	g_break_pts[count].acc_type = acc_type;
	g_num_breakpoints = count + 1;
	fixup_brks();
}

void
show_bp()
{
	char	acc_str[4];
	word32	addr, end_addr, acc_type;
	int	i;

	dbg_printf("Showing breakpoints set\n");
	for(i = 0; i < g_num_breakpoints; i++) {
		addr = g_break_pts[i].start_addr;
		end_addr = g_break_pts[i].end_addr;
		acc_type = g_break_pts[i].acc_type;
		acc_str[0] = ' ';
		acc_str[1] = ' ';
		acc_str[2] = ' ';
		acc_str[3] = 0;
		if(acc_type & 4) {
			acc_str[2] = 'X';
		}
		if(acc_type & 2) {
			acc_str[1] = 'W';
		}
		if(acc_type & 1) {
			acc_str[0] = 'R';
		}
		if(end_addr != addr) {
			dbg_printf("bp:%02x: %06x-%06x, t:%02x %s\n", i, addr,
						end_addr, acc_type, acc_str);
		} else {
			dbg_printf("bp:%02x: %06x, t:%02x %s\n", i, addr,
							acc_type, acc_str);
		}
	}
}

void
delete_bp(word32 addr, word32 end_addr)
{
	int	count, hit;
	int	i, j;

	dbg_printf("About to delete BP at %06x\n", addr);
	count = g_num_breakpoints;

	hit = -1;
	for(i = count - 1; i >= 0; i--) {
		if((g_break_pts[i].start_addr > end_addr) ||
					(g_break_pts[i].end_addr < addr)) {
			continue;		// Not this entry
		}
		hit = i;
		dbg_printf("Deleting brkpoint #0x%02x\n", hit);
		for(j = i+1; j < count; j++) {
			g_break_pts[j-1] = g_break_pts[j];
		}
		count--;
	}
	g_num_breakpoints = count;
	if(hit < 0) {
		dbg_printf("Breakpoint not found!\n");
	} else {
		setup_pageinfo();
	}

	show_bp();
}

void
debug_iwm(const char *str)
{
	if(str) {
		// Dummy use of argument
	}
	iwm_show_track(-1, -1, 0);
}

void
debug_iwm_check(const char *str)
{
	if(str) {
		// Dummy use of argument
	}
	iwm_check_nibblization(0);
}

int
do_blank(int mode, int old_mode)
{
	int	tmp;

	switch(old_mode) {
	case 's':
		tmp = g_a2;
		if(tmp == 0) {
			tmp = 1;
		}
#if 0
		for(i = 0; i < tmp; i++) {
			g_stepping = 1;
			do_step();
			if(g_halt_sim != 0) {
				break;
			}
		}
#endif
		g_list_kpc = engine.kpc;
		/* video_update_through_line(262); */
		break;
	case ':':
		set_memory_c(((g_a3bank << 16) + g_a3), g_a2, 0);
		g_a3++;
		mode = old_mode;
		break;
	case '.':
	case 0:
		xam_mem(-1);
		break;
	case ',':
		xam_mem(16);
		break;
	case '+':
		dbg_printf("%x\n", g_a1 + g_a2);
		break;
	case '-':
		dbg_printf("%x\n", g_a1 - g_a2);
		break;
	default:
		dbg_printf("Unknown mode at space: %d\n", old_mode);
		break;
	}
	return mode;
}

void
do_go()
{
	/* also called by do_step */

	g_config_control_panel = 0;
	clear_halt();
}

void
do_step()
{
	int	size_mem_imm, size_x_imm;

	return;			// This is not correct

	do_go();

	size_mem_imm = 2;
	if(engine.psr & 0x20) {
		size_mem_imm = 1;
	}
	size_x_imm = 2;
	if(engine.psr & 0x10) {
		size_x_imm = 1;
	}
	dbg_printf("%s\n",
			do_dis(engine.kpc, size_mem_imm, size_x_imm, 0, 0, 0));
}

void
xam_mem(int count)
{
	show_hex_mem(g_a1bank, g_a1, g_a2, count);
	g_a1 = g_a2 + 1;
}

void
show_hex_mem(word32 startbank, word32 start, word32 end, int count)
{
	char	ascii[MAXNUM_HEX_PER_LINE];
	word32	i;
	int	val, offset;

	if(count < 0) {
		count = 16 - (start & 0xf);
	}

	offset = 0;
	ascii[0] = 0;
	dbg_printf("Showing hex mem: bank: %x, start: %x, end: %x\n",
		startbank, start, end);
	for(i = start; i <= end; i++) {
		if( (i==start) || (count == 16) ) {
			dbg_printf("%04x:",i);
		}
		dbg_printf(" %02x", get_memory_c((startbank <<16) + i));
		val = get_memory_c((startbank << 16) + i) & 0x7f;
		if((val < 32) || (val >= 0x7f)) {
			val = '.';
		}
		ascii[offset++] = val;
		ascii[offset] = 0;
		count--;
		if(count <= 0) {
			dbg_printf("   %s\n", ascii);
			offset = 0;
			ascii[0] = 0;
			count = 16;
		}
	}
	if(offset > 0) {
		dbg_printf("   %s\n", ascii);
	}
}

void
do_debug_list()
{
	char	*str;
	int	size, size_mem_imm, size_x_imm;
	int	i;

	dbg_printf("%d=m %d=x %d=LCBANK\n", (engine.psr >> 5)&1,
		(engine.psr >> 4) & 1, (g_c068_statereg & 0x4) >> 2);

	size_mem_imm = 2;
	if(engine.psr & 0x20) {
		size_mem_imm = 1;
	}
	size_x_imm = 2;
	if(engine.psr & 0x10) {
		size_x_imm = 1;
	}
	for(i = 0; i < 20; i++) {
		str = do_dis(g_list_kpc, size_mem_imm, size_x_imm, 0, 0, &size);
		g_list_kpc += size;
		dbg_printf("%s\n", str);
	}
}

void
dis_do_memmove()
{
	word32	val;

	dbg_printf("Memory move from %02x/%04x.%04x to %02x/%04x\n", g_a1bank,
						g_a1, g_a2, g_a4bank, g_a4);
	while(g_a1 <= (g_a2 & 0xffff)) {
		val = get_memory_c((g_a1bank << 16) + g_a1);
		set_memory_c((g_a4bank << 16) + g_a4, val, 0);
		g_a1++;
		g_a4++;
	}
	g_a1 = g_a1 & 0xffff;
	g_a4 = g_a4 & 0xffff;
}

void
dis_do_pattern_search()
{
#if 0
	word32	match_val, val;
	int	match_shift, count;

	dbg_printf("Memory pattern search for %04x in %02x/%04x to %02x/%04x\n",
			g_a4, g_a1bank, g_a1, g_a2bank, g_a2);
	match_shift = 0;
	count = 0;
	match_val = g_a4;
	while(1) {
		if(g_a1bank > g_a2bank) {
			break;
		}
		if(g_a1 > g_a2) {
			break;
		}
		val = get_memory_c((g_a1bank << 16) + g_a1);
		if(val == ((match_val >> match_shift) & 0xff)) {
			match_shift += 8;
			if(match_shift >= 16) {
				dbg_printf("Found %04x at %02x/%04x\n",
						match_val, g_a1bank, g_a1);
				count++;
			}
		} else {
			match_shift = 0;
		}
		g_a1++;
		if(g_a1 >= 0x10000) {
			g_a1 = 0;
			g_a1bank++;
		}
	}
#endif
}

void
dis_do_compare()
{
	word32	val1, val2;

	dbg_printf("Memory Compare from %02x/%04x.%04x with %02x/%04x\n",
					g_a1bank, g_a1, g_a2, g_a4bank, g_a4);
	while(g_a1 <= (g_a2 & 0xffff)) {
		val1 = get_memory_c((g_a1bank << 16) + g_a1);
		val2 = get_memory_c((g_a4bank << 16) + g_a4);
		if(val1 != val2) {
			dbg_printf("%02x/%04x: %02x vs %02x\n", g_a1bank, g_a1,
								val1, val2);
		}
		g_a1++;
		g_a4++;
	}
	g_a1 = g_a1 & 0xffff;
	g_a4 = g_a4 & 0xffff;
}

const char *
do_debug_unix(const char *str, int old_mode)
{
	char	localbuf[LINE_SIZE+2];
	byte	*bptr;
	word32	offset, len, a1_val;
	long	ret;
	int	fd, load;
	int	i;

	load = 0;
	switch(*str++) {
	case 'l': case 'L':
		dbg_printf("Loading..");
		load = 1;
		break;
	case 's': case 'S':
		dbg_printf("Saving...");
		break;
	default:
		dbg_printf("Unknown unix command: %c\n", *(str - 1));
		if(str[-1] == 0) {
			return str - 1;
		}
		return str;
	}
	while((*str == ' ') || (*str == '\t')) {
		str++;
	}
	i = 0;
	while(i < LINE_SIZE) {
		localbuf[i++] = *str++;
		if((*str==' ') || (*str == '\t') || (*str == '\n') ||
								(*str == 0)) {
			break;
		}
	}
	localbuf[i] = 0;

	dbg_printf("About to open: %s,len: %d\n", localbuf,
						(int)strlen(localbuf));
	if(load) {
		fd = open(localbuf, O_RDONLY | O_BINARY);
	} else {
		fd = open(localbuf, O_WRONLY | O_CREAT | O_BINARY, 0x1b6);
	}
	if(fd < 0) {
		dbg_printf("Open %s failed: %d. errno:%d\n", localbuf, fd,
								errno);
		return str;
	}
	if(load) {
		offset = g_a1 & 0xffff;
		len = 0x20000 - offset;
	} else {
		if(old_mode == '.') {
			len = g_a2 - g_a1 + 1;
		} else {
			len = 0x100;
		}
	}
	a1_val = (g_a1bank << 16) | g_a1;
	bptr = &g_memory_ptr[a1_val];
	if((g_a1bank >= 0xe0) && (g_a1bank < 0xe2)) {
		bptr = &g_slow_memory_ptr[a1_val & 0x1ffff];
	}
	if(load) {
		ret = read(fd, bptr, len);
	} else {
		ret = write(fd, bptr, len);
	}
	dbg_printf("Read/write: addr %06x for %04x bytes, ret: %lx bytes\n",
		a1_val, len, ret);
	if(ret < 0) {
		dbg_printf("errno: %d\n", errno);
	}
	g_a1 = g_a1 + (int)ret;
	return str;
}

void
do_debug_load()
{
	dbg_printf("Sorry, can't load now\n");
}

char *
do_dis(word32 kpc, int accsize, int xsize, int op_provided, word32 instr,
							int *size_ptr)
{
	char	buffer[MAX_DISAS_BUF];
	char	buffer2[MAX_DISAS_BUF];
	const char *str;
	word32	val, oldkpc, dtype;
	int	args, type, opcode, signed_val;
	int	i;

	oldkpc = kpc;
	if(op_provided) {
		opcode = (instr >> 24) & 0xff;
	} else {
		opcode = (int)get_memory_c(kpc) & 0xff;
	}

	kpc++;

	dtype = disas_types[opcode];
	str = disas_opcodes[opcode];
	type = dtype & 0xff;
	args = dtype >> 8;

	if(args > 3) {
		if(args == 4) {
			args = accsize;
		} else if(args == 5) {
			args = xsize;
		}
	}

	val = -1;
	switch(args) {
	case 0:
		val = 0;
		break;
	case 1:
		if(op_provided) {
			val = instr & 0xff;
		} else {
			val = get_memory_c(kpc);
		}
		break;
	case 2:
		if(op_provided) {
			val = instr & 0xffff;
		} else {
			val = get_memory16_c(kpc);
		}
		break;
	case 3:
		if(op_provided) {
			val = instr & 0xffffff;
		} else {
			val = get_memory24_c(kpc);
		}
		break;
	default:
		fprintf(stderr, "args out of rang: %d, opcode: %08x\n",
			args, opcode);
		break;
	}
	kpc += args;

	if(!op_provided) {
		instr = (opcode << 24) | (val & 0xffffff);
	}

	switch(type) {
	case ABS:
		if(args != 2) {
			dbg_printf("arg # mismatch for opcode %x\n", opcode);
		}
		snprintf(&buffer[0], MAX_DISAS_BUF, "%s  $%04x", str, val);
		break;
	case ABSX:
		if(args != 2) {
			dbg_printf("arg # mismatch for opcode %x\n", opcode);
		}
		snprintf(&buffer[0], MAX_DISAS_BUF, "%s  $%04x,X", str, val);
		break;
	case ABSY:
		if(args != 2) {
			dbg_printf("arg # mismatch for opcode %x\n", opcode);
		}
		snprintf(&buffer[0], MAX_DISAS_BUF, "%s  $%04x,Y", str, val);
		break;
	case ABSLONG:
		if(args != 3) {
			dbg_printf("arg # mismatch for opcode %x\n", opcode);
		}
		snprintf(&buffer[0], MAX_DISAS_BUF, "%s  $%06x", str, val);
		break;
	case ABSIND:
		if(args != 2) {
			dbg_printf("arg # mismatch for opcode %x\n", opcode);
		}
		snprintf(&buffer[0], MAX_DISAS_BUF, "%s ($%04x)", str, val);
		break;
	case ABSXIND:
		if(args != 2) {
			dbg_printf("arg # mismatch for opcode %x\n", opcode);
		}
		snprintf(&buffer[0], MAX_DISAS_BUF, "%s ($%04x,X)", str, val);
		break;
	case IMPLY:
	case ACCUM:
		if(args != 0) {
			dbg_printf("arg # mismatch for opcode %x\n", opcode);
		}
		snprintf(&buffer[0], MAX_DISAS_BUF,  "%s", str);
		break;
	case IMMED:
		if(args == 1) {
			snprintf(&buffer[0], MAX_DISAS_BUF, "%s  #$%02x", str,
									val);
		} else if(args == 2) {
			snprintf(&buffer[0], MAX_DISAS_BUF, "%s  #$%04x", str,
									val);
		} else {
			dbg_printf("arg # mismatch for opcode %x\n", opcode);
		}
		break;
	case JUST8:
	case DLOC:
		if(args != 1) {
			dbg_printf("arg # mismatch for opcode %x\n", opcode);
		}
		snprintf(&buffer[0], MAX_DISAS_BUF, "%s  $%02x", str, val);
		break;
	case DLOCX:
		if(args != 1) {
			dbg_printf("arg # mismatch for opcode %x\n", opcode);
		}
		snprintf(&buffer[0], MAX_DISAS_BUF, "%s  $%02x,X", str, val);
		break;
	case DLOCY:
		if(args != 1) {
			dbg_printf("arg # mismatch for opcode %x\n", opcode);
		}
		snprintf(&buffer[0], MAX_DISAS_BUF, "%s  $%02x,Y", str, val);
		break;
	case LONG:
		if(args != 3) {
			dbg_printf("arg # mismatch for opcode %x\n", opcode);
		}
		snprintf(&buffer[0], MAX_DISAS_BUF, "%s  $%06x", str, val);
		break;
	case LONGX:
		if(args != 3) {
			dbg_printf("arg # mismatch for opcode %x\n", opcode);
		}
		snprintf(&buffer[0], MAX_DISAS_BUF, "%s  $%06x,X", str, val);
		break;
	case DLOCIND:
		if(args != 1) {
			dbg_printf("arg # mismatch for opcode %x\n", opcode);
		}
		snprintf(&buffer[0], MAX_DISAS_BUF, "%s  ($%02x)", str, val);
		break;
	case DLOCINDY:
		if(args != 1) {
			dbg_printf("arg # mismatch for opcode %x\n", opcode);
		}
		snprintf(&buffer[0], MAX_DISAS_BUF, "%s  ($%02x),Y", str, val);
		break;
	case DLOCXIND:
		if(args != 1) {
			dbg_printf("arg # mismatch for opcode %x\n", opcode);
		}
		snprintf(&buffer[0], MAX_DISAS_BUF, "%s  ($%02x,X)", str, val);
		break;
	case DLOCBRAK:
		if(args != 1) {
			dbg_printf("arg # mismatch for opcode %x\n", opcode);
		}
		snprintf(&buffer[0], MAX_DISAS_BUF, "%s  [$%02x]", str, val);
		break;
	case DLOCBRAKY:
		if(args != 1) {
			dbg_printf("arg # mismatch for opcode %x\n", opcode);
		}
		snprintf(&buffer[0], MAX_DISAS_BUF, "%s  [$%02x],y", str, val);
		break;
	case DISP8:
		if(args != 1) {
			dbg_printf("arg # mismatch for opcode %x\n", opcode);
		}
		signed_val = (signed char)val;
		snprintf(&buffer[0], MAX_DISAS_BUF, "%s  $%04x", str,
						(kpc + signed_val) & 0xffff);
		break;
	case DISP8S:
		if(args != 1) {
			dbg_printf("arg # mismatch for opcode %x\n", opcode);
		}
		snprintf(&buffer[0], MAX_DISAS_BUF, "%s  $%02x,S", str,
								val & 0xff);
		break;
	case DISP8SINDY:
		if(args != 1) {
			dbg_printf("arg # mismatch for opcode %x\n", opcode);
		}
		snprintf(&buffer[0], MAX_DISAS_BUF, "%s  ($%02x,S),Y", str,
								val & 0xff);
		break;
	case DISP16:
		if(args != 2) {
			dbg_printf("arg # mismatch for opcode %x\n", opcode);
		}
		snprintf(&buffer[0], MAX_DISAS_BUF, "%s  $%04x", str,
				(word32)(kpc+(signed)(word16)(val)) & 0xffff);
		break;
	case MVPMVN:
		if(args != 2) {
			dbg_printf("arg # mismatch for opcode %x\n", opcode);
		}
		snprintf(&buffer[0], MAX_DISAS_BUF, "%s  $%02x,$%02x", str,
							val & 0xff, val >> 8);
		break;
	case SEPVAL:
	case REPVAL:
		if(args != 1) {
			dbg_printf("arg # mismatch for opcode %x\n", opcode);
		}
		snprintf(&buffer[0], MAX_DISAS_BUF, "%s  #$%02x", str, val);
		break;
	default:
		dbg_printf("argument type: %d unexpected\n", type);
		break;
	}

	g_disas_buffer[0] = 0;
	snprintf(&g_disas_buffer[0], MAX_DISAS_BUF, "%02x/%04x: %02x ",
		oldkpc >> 16, oldkpc & 0xffff, opcode);
	for(i = 1; i <= args; i++) {
		snprintf(&buffer2[0], MAX_DISAS_BUF, "%02x ", instr & 0xff);
		cfg_strlcat(&g_disas_buffer[0], &buffer2[0], MAX_DISAS_BUF);
		instr = instr >> 8;
	}
	for(; i < 4; i++) {
		cfg_strlcat(&g_disas_buffer[0], "   ", MAX_DISAS_BUF);
	}
	cfg_strlcat(&g_disas_buffer[0], " ", MAX_DISAS_BUF);
	cfg_strlcat(&g_disas_buffer[0], &buffer[0], MAX_DISAS_BUF);

	if(size_ptr) {
		*size_ptr = args + 1;
	}
	return (&g_disas_buffer[0]);
}

int
debug_get_view_line(int back)
{
	int	pos;

	// where back==0 means return pos - 1.
	pos = g_debug_lines_pos - 1;
	pos = pos - back;
	if(pos < 0) {
		if(g_debug_lines_alloc >= g_debug_lines_max) {
			pos += g_debug_lines_alloc;
		} else {
			return 0;			// HACK: return -1
		}
	}
	return pos;
}

int
debug_add_output_line(char *in_str)
{
	Debug_entry *line_ptr;
	byte	*out_bptr;
	int	pos, alloc, view, used_len, c;
	int	i;

	// printf("debug_add_output_line %s len:%d\n", in_str, len);
	pos = g_debug_lines_pos;
	line_ptr = g_debug_lines_ptr;
	alloc = g_debug_lines_alloc;
	if(pos >= alloc) {
		if(alloc < g_debug_lines_max) {
			alloc = MY_MAX(2048, alloc*3);
			alloc = MY_MAX(alloc, pos*3);
			alloc = MY_MIN(alloc, g_debug_lines_max);
			line_ptr = realloc(line_ptr,
						alloc * sizeof(Debug_entry));
			printf("realloc.  now %p, alloc:%d\n", line_ptr, alloc);
			g_debug_lines_ptr = line_ptr;
			g_debug_lines_alloc = alloc;
			printf("Alloced debug lines to %d\n", alloc);
		} else {
			pos = 0;
		}
	}
	// Convert to A2 format chars: set high bit of each byte, 80 chars
	//  per line
	out_bptr = &(line_ptr[pos].str_buf[0]);
	used_len = 0;
	for(i = 0; i < DEBUG_ENTRY_MAX_CHARS; i++) {
		c = ' ';
		if(*in_str) {
			c = *in_str++;
			used_len++;
		}
		c = c ^ 0x80;		// Set highbit if not already set
		out_bptr[i] = c;
	}
	pos++;
	g_debug_lines_pos = pos;
	g_debug_lines_total++;		// For updating the window
	g_debugwin_changed++;
	view = g_debug_lines_view;
	if(view >= 0) {
		view++;		// view is back from pos, so to stay the same,
				//  it must be incremented when pos incs
		if((view - 50) >= g_debug_lines_max) {
			// We were viewing the oldest page, and by wrapping
			//  around we're about to wipe out this old data
			// Jump to most recent data
			view = -1;
		}
		g_debug_lines_view = view;
	}

	return used_len;
}

void
debug_add_output_string(char *in_str, int len)
{
	int	ret, tries;

	tries = 0;
	ret = 0;
	if(g_debug_to_stdout) {
		puts(in_str);		// Send output to stdout, too
	}
	while((len > 0) || (tries == 0)) {
		// printf("DEBUG: adding str: %s, len:%d, ret:%d\n", in_str,
		//						len, ret);
		ret = debug_add_output_line(in_str);
		len -= ret;
		in_str += ret;
		tries++;
	}
}

void
debug_add_output_chars(char *str)
{
	int	pos, c, tab_spaces;

	pos = g_debug_stage_pos;
	tab_spaces = 0;
	while(1) {
		if(tab_spaces > 0) {
			c = ' ';
			tab_spaces--;
		} else {
			c = *str++;
			if(c == '\t') {
				tab_spaces = 7 - (pos & 7);
				c = ' ';
			}
		}
		pos = MY_MIN(pos, (PRINTF_BUF_SIZE - 1));
		if((c == '\n') || (pos >= (PRINTF_BUF_SIZE - 1))) {
			g_debug_stage_buf[pos] = 0;
			debug_add_output_string(&g_debug_stage_buf[0], pos);
			pos = 0;
			g_debug_stage_pos = 0;
			continue;
		}
		if(c == 0) {
			g_debug_stage_pos = pos;
			return;
		}
		g_debug_stage_buf[pos++] = c;
	}
}

int
dbg_printf(const char *fmt, ...)
{
	va_list args;
	int	ret;

	va_start(args, fmt);
	ret = dbg_vprintf(fmt, args);
	va_end(args);
	return ret;
}

int
dbg_vprintf(const char *fmt, va_list args)
{
	int	ret;

	ret = vsnprintf(&g_debug_printf_buf[0], PRINTF_BUF_SIZE, fmt, args);
	debug_add_output_chars(&g_debug_printf_buf[0]);
	return ret;
}

void
halt_printf(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	dbg_vprintf(fmt, args);
	va_end(args);

	set_halt(1);
}

void
halt2_printf(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	dbg_vprintf(fmt, args);
	va_end(args);

	set_halt(2);
}

