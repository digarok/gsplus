const char rcsid_sim65816_c[] = "@(#)$KmKId: sim65816.c,v 1.491 2025-04-27 18:03:43+00 kentd Exp $";

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

#include <math.h>

#define INCLUDE_RCSID_C
#include "defc.h"
#undef INCLUDE_RCSID_C

double g_dtime_sleep = 0;
double g_dtime_in_sleep = 0;
extern char *g_argv0_path;

#define MAX_EVENTS	64

/* All EV_* must be less than 256, since upper bits reserved for other use */
/*  e.g., DOC_INT uses upper bits to encode oscillator */
#define EV_60HZ			1
#define EV_STOP			2
#define EV_SCAN_INT		3
#define EV_DOC_INT		4
#define EV_VBL_INT		5
#define EV_SCC			6
#define EV_VID_UPD		7
#define EV_MOCKINGBOARD		8

extern int g_stepping;

extern word32 g_c068_statereg;
extern int g_cur_a2_stat;

extern word32 g_c02d_int_crom;

extern word32 g_c035_shadow_reg;
extern word32 g_c036_val_speed;

extern word32 g_c023_val;
extern word32 g_c041_val;
extern word32 g_c046_val;
extern word32 g_zipgs_reg_c059;
extern word32 g_zipgs_reg_c05a;
extern word32 g_zipgs_reg_c05b;
extern word32 g_zipgs_unlock;
extern Iwm g_iwm;

Engine_reg engine;
extern word32 table8[];
extern word32 table16[];

extern byte doc_ram[];

extern int g_iwm_motor_on;
extern int g_fast_disk_emul;
extern int g_slow_525_emul_wr;
extern int g_config_control_panel;

extern int g_audio_enable;
extern int g_preferred_rate;

int	g_a2_fatal_err = 0;
dword64	g_dcycles_end = 0;
int	g_halt_sim = 0;
int	g_rom_version = -1;
int	g_user_halt_bad = 0;
int	g_halt_on_bad_read = 0;
int	g_ignore_bad_acc = 1;
int	g_ignore_halts = 1;
int	g_code_red = 0;
int	g_code_yellow = 0;
int	g_emul_6502_ind_page_cross_bug = 0;

int	g_config_iwm_vbl_count = 0;
const char g_kegs_version_str[] = "1.38";

dword64	g_last_vbl_dfcyc = 0;
dword64	g_cur_dfcyc = 1;

double	g_last_vbl_dadjcycs = 0;
double	g_dadjcycs = 0;


int	g_wait_pending = 0;
int	g_stp_pending = 0;
extern int g_irq_pending;

int	g_num_irq = 0;
int	g_num_brk = 0;
int	g_num_cop = 0;
int	g_num_enter_engine = 0;
int	g_io_amt = 0;
int	g_engine_action = 0;
int	g_engine_recalc_event = 0;
int	g_engine_scan_int = 0;
int	g_engine_doc_int = 0;

#define MAX_FATAL_LOGS		20

int	g_debug_file_fd = -1;
int	g_fatal_log = -1;
char *g_fatal_log_strs[MAX_FATAL_LOGS];

word32 stop_run_at;

int g_25sec_cntr = 0;
int g_1sec_cntr = 0;

double g_dnatcycs_1sec = 0.0;
word32 g_natcycs_lastvbl = 0;

int Verbose = 0;
int Halt_on = 0;

word32 g_mem_size_base = 128*1024;	/* size of motherboard memory */
word32 g_mem_size_exp = 8*1024*1024;	/* size of expansion RAM card */
word32 g_mem_size_total = 128*1024;	/* Total contiguous RAM from 0 */

extern word32 g_slow_mem_changed[];

byte *g_slow_memory_ptr = 0;
byte *g_memory_ptr = 0;
byte *g_dummy_memory1_ptr = 0;
byte *g_rom_fc_ff_ptr = 0;
byte *g_rom_cards_ptr = 0;

void *g_memory_alloc_ptr = 0;		/* for freeing memory area */

Page_info page_info_rd_wr[2*65536 + PAGE_INFO_PAD_SIZE];

word32	g_word32_tmp = 0;
int	g_force_depth = -1;
int	g_use_shmem = 1;

extern word32 g_cycs_in_40col;
extern word32 g_cycs_in_xredraw;
extern word32 g_cycs_in_refresh_line;
extern word32 g_cycs_outside_run_16ms;
extern word32 g_refresh_bytes_xfer;

extern int g_num_snd_plays;
extern int g_num_recalc_snd_parms;

extern int g_doc_vol;

extern int g_status_refresh_needed;

int
sim_get_force_depth()
{
	return g_force_depth;
}

int
sim_get_use_shmem()
{
	return g_use_shmem;
}

void
sim_set_use_shmem(int use_shmem)
{
	g_use_shmem = use_shmem;
}

#define TOOLBOX_LOG_LEN		64

int g_toolbox_log_pos = 0;
word32 g_toolbox_log_array[TOOLBOX_LOG_LEN][8];

word32
toolbox_debug_4byte(word32 addr)
{
	word32	part1, part2;

	/* If addr looks safe, use it */
	if(addr > 0xbffc) {
		return (word32)-1;
	}

	part1 = get_memory16_c(addr);
	part1 = (part1 >> 8) + ((part1 & 0xff) << 8);
	part2 = get_memory16_c(addr+2);
	part2 = (part2 >> 8) + ((part2 & 0xff) << 8);

	return (part1 << 16) + part2;
}

void
toolbox_debug_c(word32 xreg, word32 stack, dword64 *dcyc_ptr)
{
	int	pos;

	pos = g_toolbox_log_pos;

	stack += 9;
	g_toolbox_log_array[pos][0] = (word32)
					((g_last_vbl_dfcyc + *dcyc_ptr) >> 16);
	g_toolbox_log_array[pos][1] = stack+1;
	g_toolbox_log_array[pos][2] = xreg;
	g_toolbox_log_array[pos][3] = toolbox_debug_4byte(stack+1);
	g_toolbox_log_array[pos][4] = toolbox_debug_4byte(stack+5);
	g_toolbox_log_array[pos][5] = toolbox_debug_4byte(stack+9);
	g_toolbox_log_array[pos][6] = toolbox_debug_4byte(stack+13);
	g_toolbox_log_array[pos][7] = toolbox_debug_4byte(stack+17);

	pos++;
	if(pos >= TOOLBOX_LOG_LEN) {
		pos = 0;
	}

	g_toolbox_log_pos = pos;
}

void
show_toolbox_log()
{
	int	pos;
	int	i;

	pos = g_toolbox_log_pos;

	for(i = TOOLBOX_LOG_LEN - 1; i >= 0; i--) {
		printf("%2d:%2d: %08x %06x  %04x: %08x %08x %08x %08x %08x\n",
			i, pos,
			g_toolbox_log_array[pos][0],
			g_toolbox_log_array[pos][1],
			g_toolbox_log_array[pos][2],
			g_toolbox_log_array[pos][3],
			g_toolbox_log_array[pos][4],
			g_toolbox_log_array[pos][5],
			g_toolbox_log_array[pos][6],
			g_toolbox_log_array[pos][7]);
		pos++;
		if(pos >= TOOLBOX_LOG_LEN) {
			pos = 0;
		}
	}
}

word32
get_memory_io(word32 loc, dword64 *dcyc_ptr)
{
	int	tmp;

	if(loc > 0xffffff) {
		halt_printf("get_memory_io:%08x out of range==halt!\n", loc);
		return 0;
	}

	tmp = loc & 0xfef000;
	if(tmp == 0xc000 || tmp == 0xe0c000) {
		return(io_read(loc & 0xfff, dcyc_ptr));
	}

	/* Else it's an illegal addr...skip if memory sizing */
	if(loc >= g_mem_size_total) {
		if((loc & 0xfffe) == 0) {
			//printf("get_io assuming mem sizing, not halting\n");
			return 0;
		}
	}

	/* Skip reads to f80000 and f00000, just return 0 */
	if((loc & 0xf70000) == 0xf00000) {
		return 0;
	}

	if((loc & 0xff0000) == 0xef0000) {
		/* DOC RAM */
		return (doc_ram[loc & 0xffff]);
	}
	if((loc & 0xffff00) == 0xbcff00) {
		/* TWGS mapped some ROM here, we'll force in all 0's */
		/* If user has selected >= 12MB of memory, then mem will be */
		/*  returned and we won't ever get here */
		return 0;
	}

	g_code_yellow++;
	if(g_ignore_bad_acc && !g_user_halt_bad) {
		/* print no message, just get out.  User doesn't want */
		/*  to be bothered by buggy programs */
		return 0;
	}

	printf("get_memory_io for addr: %06x\n", loc);
	printf("stat for addr: %06x = %p\n", loc,
				GET_PAGE_INFO_RD((loc >> 8) & 0xffff));
	set_halt(g_halt_on_bad_read | g_user_halt_bad);

	return 0;
}

void
set_memory_io(word32 loc, int val, dword64 *dcyc_ptr)
{
	word32	tmp;

	tmp = loc & 0xfef000;
	if(tmp == 0xc000 || tmp == 0xe0c000) {
		io_write(loc, val, dcyc_ptr);
		return;
	}

	/* Else it's an illegal addr */
	if(loc >= g_mem_size_total) {
		if((loc & 0xfffe) == 0) {
			//printf("set_io assuming mem sizing, not halting\n");
			return;
		}
	}

	/* ignore writes to ROM */
	if((loc & 0xfc0000) == 0xfc0000) {
		return;
	}

	if((loc & 0xff0000) == 0xef0000) {
		/* DOC RAM */
		doc_ram[loc & 0xffff] = val;
		return;
	}

	if(g_ignore_bad_acc && !g_user_halt_bad) {
		/* print no message, just get out.  User doesn't want */
		/*  to be bothered by buggy programs */
		return;
	}

	if((loc & 0xffc000) == 0x00c000) {
		printf("set_memory %06x = %02x, warning\n", loc, val);
		return;
	}

	halt_printf("set_memory %06x = %02x, stopping\n", loc, val);

	return;
}

void
show_regs_act(Engine_reg *eptr)
{
	int	tmp_acc, tmp_x, tmp_y, tmp_psw, kpc, direct_page, dbank, stack;

	kpc = eptr->kpc;
	tmp_acc = eptr->acc;
	direct_page = eptr->direct;
	dbank = eptr->dbank;
	stack = eptr->stack;

	tmp_x = eptr->xreg;
	tmp_y = eptr->yreg;

	tmp_psw = eptr->psr;

	dbg_printf("  PC=%02x.%04x A=%04x X=%04x Y=%04x P=%03x",
		kpc>>16, kpc & 0xffff ,tmp_acc,tmp_x,tmp_y,tmp_psw);
	dbg_printf(" S=%04x D=%04x B=%02x,cyc:%016llx\n", stack, direct_page,
		dbank, g_cur_dfcyc);
}

void
show_regs()
{
	show_regs_act(&engine);
}

void
my_exit(int ret)
{
	g_a2_fatal_err = 0x10 + ret;
	printf("exiting\n");
}


void
do_reset()
{
	g_c035_shadow_reg = 0;

	g_c068_statereg = 0x200 | 0x08 | 0x04;	// Set wrdefram, rdrom, lcbank2
	g_c02d_int_crom = 0xff;
	if(g_rom_version != 0) {		// IIgs ROM01 or ROM03
		g_c068_statereg |= 0x01;	// also set intcx
		g_c02d_int_crom = 0;
	}
	g_c023_val = 0;
	g_c041_val = 0;

	engine.psr = (engine.psr | 0x134) & ~(0x08);
	engine.stack = 0x100 + (engine.stack & 0xff);
	engine.dbank = 0;
	engine.direct = 0;
	engine.xreg &= 0xff;
	engine.yreg &= 0xff;
	g_wait_pending = 0;
	g_stp_pending = 0;

	video_reset();
	adb_reset();
	iwm_reset();
	scc_reset();
	sound_reset(g_cur_dfcyc);
	setup_pageinfo();
	change_display_mode(g_cur_dfcyc);

	g_irq_pending = 0;
	g_code_yellow = 0;
	g_code_red = 0;

	engine.kpc = get_memory16_c(0x00fffc);

	g_stepping = 0;
}

#define CHECK(start, var, value, var1, var2)				\
	var2 = PTR2WORD(&(var));					\
	var1 = PTR2WORD((start));					\
	if((var2 - var1) != value) {					\
		printf("CHECK: " #var " is 0x%x, but " #value " is 0x%x\n", \
			(var2 - var1), value);				\
		exit(5);						\
	}

byte *
memalloc_align(int size, int skip_amt, void **alloc_ptr)
{
	byte	*bptr;
	word32	addr;
	word32	offset;

	skip_amt = MY_MAX(256, skip_amt);
	bptr = calloc(size + skip_amt, 1);
	if(alloc_ptr) {
		/* Save allocation address */
		*alloc_ptr = bptr;
	}

	addr = PTR2WORD(bptr) & 0xff;

	/* must align bptr to be 256-byte aligned */
	/* this code should work even if ptrs are > 32 bits */

	offset = ((addr + skip_amt - 1) & (~0xff)) - addr;

	return (bptr + offset);
}

void
memory_ptr_init()
{
	word32	mem_size;

	/* This routine may be called several times--each time the ROM file */
	/*  changes this will be called */
	mem_size = MY_MIN(0xdf0000, g_mem_size_base + g_mem_size_exp);
	if(g_rom_version == 0) {			// Apple //e
		mem_size = g_mem_size_base;
	}
	g_mem_size_total = mem_size;
	if(g_memory_alloc_ptr) {
		free(g_memory_alloc_ptr);
		g_memory_alloc_ptr = 0;
	}
	g_memory_ptr = memalloc_align(mem_size, 256, &g_memory_alloc_ptr);

	printf("RAM size is 0 - %06x (%.2fMB)\n", mem_size,
		(double)mem_size/(1024.0*1024.0));
}

extern int g_screen_redraw_skip_amt;
extern int g_use_dhr140;
extern int g_use_bw_hires;

char g_display_env[512];
int	g_screen_depth = 8;

int
parse_argv(int argc, char **argv, int slashes_to_find)
{
	byte	*bptr;
	char	*str, *arg2_str;
	int	skip_amt, tmp1, len;
	int	i;

#if 0
	// If launched from Finder, no stdout, so send it to /tmp/out1
	fflush(stdout);
	setvbuf(stdout, 0, _IONBF, 0);
	close(1);
	(void)open("/tmp/out1", O_WRONLY | O_CREAT | O_TRUNC, 0x1b6);
#endif

	printf("Starting KEGS v%s\n", &g_kegs_version_str[0]);

	// parse arguments
	// First, Check if KEGS_BIG_ENDIAN is set correctly
	g_word32_tmp = 0x01020304;
	bptr = (byte *)&(g_word32_tmp);
#ifdef KEGS_BIG_ENDIAN
	bptr[0] = 6;
	bptr[3] = 5;
#else
	bptr[0] = 5;
	bptr[3] = 6;
#endif
	if(g_word32_tmp != 0x06020305) {
		fatal_printf("KEGS_BIG_ENDIAN is not properly set\n");
		return 1;
	}

	str = kegs_malloc_str(argv[0]);
	len = (int)strlen(str);
	for(i = len; i > 0; i--) {
		if(str[i] == '/') {
			if(--slashes_to_find <= 0) {
				str[i] = 0;
				g_argv0_path = str;
				break;
			}
		}
	}
	printf("g_argv0_path: %s\n", g_argv0_path);

	i = 0;
	while(++i < argc) {
		printf("argv[%d] = %s\n", i, argv[i]);
		if(!strcmp("-badrd", argv[i])) {
			printf("Halting on bad reads\n");
			g_halt_on_bad_read = 2;
		} else if(!strcmp("-noignbadacc", argv[i])) {
			printf("Not ignoring bad memory accesses\n");
			g_ignore_bad_acc = 0;
		} else if(!strcmp("-noignhalt", argv[i])) {
			printf("Not ignoring code red halts\n");
			g_ignore_halts = 0;
		} else if(!strcmp("-24", argv[i])) {
			printf("Using 24-bit visual\n");
			g_force_depth = 24;
		} else if(!strcmp("-16", argv[i])) {
			printf("Using 16-bit visual\n");
			g_force_depth = 16;
		} else if(!strcmp("-15", argv[i])) {
			printf("Using 15-bit visual\n");
			g_force_depth = 15;
		} else if(!strcmp("-mem", argv[i])) {
			if((i+1) >= argc) {
				printf("Missing argument\n");
				return 1;
			}
			g_mem_size_exp = strtol(argv[i+1], 0, 0) & 0x00ff0000;
			printf("Using %d as memory size\n", g_mem_size_exp);
			i++;
		} else if(!strcmp("-skip", argv[i])) {
			if((i+1) >= argc) {
				printf("Missing to skip argument\n");
				return 1;
			}
			skip_amt = (int)strtol(argv[i+1], 0, 0);
			printf("Using %d as skip_amt\n", skip_amt);
			g_screen_redraw_skip_amt = skip_amt;
			i++;
		} else if(!strcmp("-audio", argv[i])) {
			if((i+1) >= argc) {
				printf("Missing argument to -audio\n");
				return 1;
			}
			tmp1 = (int)strtol(argv[i+1], 0, 0);
			printf("Using %d as audio enable val\n", tmp1);
			g_audio_enable = tmp1;
			i++;
		} else if(!strcmp("-arate", argv[i])) {
			if((i+1) >= argc) {
				printf("Missing argument to -arate\n");
				return 1;
			}
			tmp1 = (int)strtol(argv[i+1], 0, 0);
			printf("Using %d as preferred audio rate\n", tmp1);
			g_preferred_rate = tmp1;
			i++;
		} else if(!strcmp("-v", argv[i])) {
			if((i+1) >= argc) {
				printf("Missing argument to -v\n");
				return 1;
			}
			tmp1 = (int)strtol(argv[i+1], 0, 0);
			printf("Setting Verbose = 0x%03x\n", tmp1);
			Verbose = tmp1;
			i++;
		} else if(!strcmp("-display", argv[i])) {
			if((i+1) >= argc) {
				printf("Missing argument\n");
				exit(1);
			}
			printf("Using %s as display\n", argv[i+1]);
			snprintf(g_display_env, sizeof(g_display_env),
						"DISPLAY=%s", argv[i+1]);
			putenv(&g_display_env[0]);
			i++;
		} else if(!strcmp("-noshm", argv[i])) {
			printf("Not using X shared memory\n");
			g_use_shmem = 0;
		} else if(!strcmp("-joystick", argv[i])) {
			printf("Ignoring -joystick option\n");
		} else if(!strcmp("-dhr140", argv[i])) {
			printf("Using simple dhires color map\n");
			g_use_dhr140 = 1;
		} else if(!strcmp("-bw", argv[i])) {
			printf("Forcing black-and-white hires modes\n");
			g_cur_a2_stat |= ALL_STAT_COLOR_C021;
			g_use_bw_hires = 1;
		} else if(!strncmp("-NS", argv[i], 3)) {
			// Some Mac argument, just ignore it
			if((i + 1) < argc) {
				// If the next argument doesn't start with '-',
				//  then ignore it, too
				if(argv[i+1][0] != '-') {
					i++;
				}
			}
		} else if(!strcmp("-logpc", argv[i])) {
			printf("Force logpc enable\n");
			debug_logpc_on("on");
		} else if(!strncmp("-cfg", argv[i], 4)) {
			if((i + 1) < argc) {
				config_set_config_kegs_name(argv[i+1]);
			}
			i++;
		} else if(argv[i][0] == '-') {
			arg2_str = 0;
			if((i + 1) < argc) {
				arg2_str = argv[i+1];
			}
			i += config_add_argv_override(&argv[i][1], arg2_str);
		} else {
			printf("Bad option: %s\n", argv[i]);
			return 3;
		}
	}

	return 0;
}

int
kegs_init(int mdepth, int screen_width, int screen_height, int no_scale_window)
{
	g_config_control_panel = 0;

	woz_crc_init();
	fixed_memory_ptrs_init();

	if(sizeof(word32) != 4) {
		printf("sizeof(word32) = %d, must be 4!\n",
							(int)sizeof(word32));
		return 1;
	}
	prepare_a2_font();		// Prepare default built-in font
	scc_init();
	iwm_init();
	init_reg();
	adb_init();
	initialize_events();
	debugger_init();
	setup_pageinfo();

	config_init();
	load_roms_init_memory();

	clear_halt();

	video_init(mdepth, screen_width, screen_height, no_scale_window);

	sound_init();
	joystick_init();

	if(g_rom_version >= 3) {
		g_c036_val_speed |= 0x40;	/* set power-on bit */
	}

	do_reset();

	g_stepping = 0;
	clear_halt();
	cfg_set_config_panel(g_config_control_panel);

	return 0;
}

void
load_roms_init_memory()
{
	config_load_roms();
	memory_ptr_init();
	clk_setup_bram_version();	/* Must be after config_load_roms */
	if(g_rom_version >= 3) {
		g_c036_val_speed |= 0x40;	/* set power-on bit */
	} else {
		g_c036_val_speed &= (~0x40);	/* clear the bit */
	}
	// Do not call do_reset(), caller is responsible for that

	/* if user booted ROM 01, switches to ROM 03, then switches back */
	/*  to ROM 01, then the reset routines call to Tool $0102 looks */
	/*  at uninitialized $e1/15fe and if it is negative it will JMP */
	/*  through $e1/1688 which ROM 03 left pointing to fc/0199 */
	/* So set e1/15fe = 0 */
	set_memory16_c(0xe115fe, 0, 1);
}

Event g_event_list[MAX_EVENTS];
Event g_event_free;
Event g_event_start;

void
initialize_events()
{
	int	i;

	for(i = 1; i < MAX_EVENTS; i++) {
		g_event_list[i-1].next = &g_event_list[i];
	}
	g_event_free.next = &g_event_list[0];
	g_event_list[MAX_EVENTS-1].next = 0;

	g_event_start.next = 0;
	g_event_start.dfcyc = 0;

	add_event_entry(CYCLES_IN_16MS_RAW << 16, EV_60HZ);
}

void
check_for_one_event_type(int type, word32 mask)
{
	Event	*ptr;
	int	count, depth;

	type = type & 0xff;
	count = 0;
	depth = 0;
	ptr = g_event_start.next;
	while(ptr != 0) {
		depth++;
		if((ptr->type & mask) == (word32)type) {
			count++;
			if(count != 1) {
				halt_printf("in check_for_1, type %04x found "
					"at depth: %d, count: %d, at %016llx\n",
					ptr->type, depth, count, ptr->dfcyc);
			}
		}
		ptr = ptr->next;
	}
}

void
add_event_entry(dword64 dfcyc, int type)
{
	Event	*this_event;
	Event	*ptr, *prev_ptr;
	int	done;

	this_event = g_event_free.next;
	if(this_event == 0) {
		halt_printf("Out of queue entries!\n");
		show_all_events();
		return;
	}
	g_event_free.next = this_event->next;

	this_event->type = type;

	if((dfcyc > (g_cur_dfcyc + (50LL*1000*1000 << 16))) ||
						(dfcyc < g_cur_dfcyc)) {
		halt_printf("add_event bad dfcyc:%016llx, type:%05x, "
			"cur_dfcyc: %016llx!\n", dfcyc, type, g_cur_dfcyc);
		dfcyc = g_cur_dfcyc + (1000LL << 16);
	}

	ptr = g_event_start.next;
	if(ptr && (dfcyc < ptr->dfcyc)) {
		/* create event before next expected event */
		/* do this by calling engine_recalc_events */
		engine_recalc_events();
	}

	prev_ptr = &g_event_start;
	ptr = g_event_start.next;

	done = 0;
	while(!done) {
		if(ptr == 0) {
			this_event->next = ptr;
			this_event->dfcyc = dfcyc;
			prev_ptr->next = this_event;
			break;
		} else {
			if(ptr->dfcyc < dfcyc) {	// step across this guy
				prev_ptr = ptr;
				ptr = ptr->next;
			} else {			// go before this guy */
				this_event->dfcyc = dfcyc;
				this_event->next = ptr;
				prev_ptr->next = this_event;
				break;
			}
		}
	}

	check_for_one_event_type(type, 0xffff);
}

extern int g_doc_saved_ctl;

dword64
remove_event_entry(int type, word32 mask)
{
	Event	*ptr, *prev_ptr;
	Event	*next_ptr;

	ptr = g_event_start.next;
	prev_ptr = &g_event_start;

	while(ptr != 0) {
		if((ptr->type & mask) == (word32)type) {	// got it
			next_ptr = ptr->next;			// remove it
			prev_ptr->next = next_ptr;

			/* Add ptr to free list */
			ptr->next = g_event_free.next;
			g_event_free.next = ptr;

			return ptr->dfcyc;
		}
		prev_ptr = ptr;
		ptr = ptr->next;
	}

	halt_printf("remove event_entry: %08x, but not found!\n", type);
	if((type & 0xff) == EV_DOC_INT) {
		printf("DOC, g_doc_saved_ctl = %02x\n", g_doc_saved_ctl);
	}
	show_all_events();

	return 0;
}

void
add_event_stop(dword64 dfcyc)
{
	add_event_entry(dfcyc, EV_STOP);
}

void
add_event_doc(dword64 dfcyc, int osc)
{
	if(dfcyc < g_cur_dfcyc) {
		dfcyc = g_cur_dfcyc;
		//halt_printf("add_event_doc: dfcyc:%016llx, cur_dfcyc:"
		//	"%016llx\n", dfcyc, g_cur_dfcyc);
	}

	add_event_entry(dfcyc, EV_DOC_INT + (osc << 8));
}

void
add_event_scc(dword64 dfcyc, int type)
{
	if(dfcyc < g_cur_dfcyc) {
		dfcyc = g_cur_dfcyc;
	}

	add_event_entry(dfcyc, EV_SCC + (type << 8));
}

void
add_event_vbl()
{
	dword64	dfcyc;

	dfcyc = g_last_vbl_dfcyc + ((192*65LL) << 16);
	add_event_entry(dfcyc, EV_VBL_INT);
}

void
add_event_vid_upd(int line)
{
	dword64	dfcyc;

	dfcyc = g_last_vbl_dfcyc + (((line + 1) * 65LL) << 16);
	add_event_entry(dfcyc, EV_VID_UPD + (line << 8));
		// Redraw line when video counters first read video data
}

void
add_event_mockingboard(dword64 dfcyc)
{
	if(dfcyc < g_cur_dfcyc) {
		dfcyc = g_cur_dfcyc;
	}
	add_event_entry(dfcyc, EV_MOCKINGBOARD);
}

dword64 g_dfcyc_scan_int = 0;

void
add_event_scan_int(dword64 dfcyc, int line)
{
	dword64	dfcyc_scan_int;

	dfcyc_scan_int = g_dfcyc_scan_int;
	if(dfcyc_scan_int) {				// Event is pending
		if(dfcyc >= g_dfcyc_scan_int) {
			// We are after (or the same) as current, do nothing
			return;
		}
		remove_event_entry(EV_SCAN_INT, 0xff);
	}
	if(dfcyc < g_cur_dfcyc) {
		// scan_int for line 0 is found during EV_60HZ, and some
		//  cycles may have passed before the EV_60HZ was handled.
		// We need it to happen now, so just adjust dfcyc
		dfcyc = g_cur_dfcyc;
	}
	add_event_entry(dfcyc, EV_SCAN_INT + (line << 8));
	g_dfcyc_scan_int = dfcyc;

	check_for_one_event_type(EV_SCAN_INT, 0xff);
}


dword64
remove_event_doc(int osc)
{
	return remove_event_entry(EV_DOC_INT + (osc << 8), 0xffff);
}

dword64
remove_event_scc(int type)
{
	return remove_event_entry(EV_SCC + (type << 8), 0xffff);
}

void
remove_event_mockingboard()
{
	(void)remove_event_entry(EV_MOCKINGBOARD, 0xff);
}

void
show_all_events()
{
	Event	*ptr;
	dword64	dfcyc;
	int	count;

	count = 0;
	ptr = g_event_start.next;
	while(ptr != 0) {
		dfcyc = ptr->dfcyc;
		printf("Event: %02x: type: %05x, dfcyc: %016llx (%08llx)\n",
			count, ptr->type, dfcyc, dfcyc - g_cur_dfcyc);
		ptr = ptr->next;
		count++;
	}

}

word32	g_vbl_count = 0;
int	g_vbl_index_count = 0;
double	dtime_array[60];
double	g_dadjcycs_array[60];
double	g_dtime_diff3_array[60];
double	g_dtime_this_vbl_array[60];
double	g_dtime_exp_array[60];
double	g_dtime_pmhz_array[60];
double	g_dtime_eff_pmhz_array[60];
double	g_dtime_in_run_16ms = 0;
double	g_dtime_outside_run_16ms = 0;
double	g_dtime_end_16ms = 0;
int	g_limit_speed = 3;
int	g_zip_speed_mhz = 16;		// 16MHz default
double	sim_time[60];
double	g_sim_sum = 0.0;

double	g_cur_sim_dtime = 0.0;
double	g_projected_pmhz = 1.0;
double	g_zip_pmhz = 8.0;
double	g_sim_mhz = 100.0;
int	g_line_ref_amt = 1;
int	g_video_line_update_interval = 0;
dword64	g_video_pixel_dcount = 0;

Fplus	g_recip_projected_pmhz_slow;
Fplus	g_recip_projected_pmhz_fast;
Fplus	g_recip_projected_pmhz_zip;
Fplus	g_recip_projected_pmhz_unl;

void
show_pmhz()
{
	printf("Pmhz: %f, c036:%02x, limit: %d\n",
		g_projected_pmhz, g_c036_val_speed, g_limit_speed);

}

void
setup_zip_speeds()
{
	dword64	drecip;
	double	fmhz;
	int	mult;

	mult = 16 - ((g_zipgs_reg_c05a >> 4) & 0xf);
		// 16 = full speed, 1 = 1/16th speed
	fmhz = (g_zip_speed_mhz * mult) / 16.0;
#if 0
	if(mult == 16) {
		/* increase full speed by 19% to make zipgs freq measuring */
		/* programs work correctly */
		fmhz = fmhz * 1.19;
	}
#endif
	drecip = (dword64)(65536 / fmhz);
	g_zip_pmhz = fmhz;
	g_recip_projected_pmhz_zip.dplus_1 = drecip;
	if(fmhz <= 2.0) {
		g_recip_projected_pmhz_zip.dplus_x_minus_1 =
						(dword64)(1.01 * 65536);
	} else {
		g_recip_projected_pmhz_zip.dplus_x_minus_1 =
					(dword64)(1.01 * 65536 - drecip);
	}
}

word32 g_cycs_end_16ms = 0;

int
run_16ms()
{
	double	dtime_start, dtime_end, dtime_end2, dtime_outside;
	int	ret;

	dtime_start = get_dtime();
	ret = 0;
	fflush(stdout);
	g_dtime_sleep = 1.0/61.0;		// For control_panel/debugger
	if(g_config_control_panel) {
		ret = cfg_control_panel_update();
		if(!g_config_control_panel) {
			return 0;	// Was just switched off, get out
		}
	} else {
		if(g_halt_sim) {
			ret = debugger_run_16ms();
		} else {
			ret = run_a2_one_vbl();
		}
	}
	video_update();
	g_vbl_count++;
	dtime_end = get_dtime();
	g_dtime_in_run_16ms += (dtime_end - dtime_start);

	// If we are ahead, then do the sleep now
	micro_sleep(g_dtime_sleep);
	dtime_end2 = get_dtime();
	//printf("Did sleep for %f, dtime passed:%f\n", g_dtime_sleep,
	//					dtime_end2 - dtime_end);
	g_dtime_sleep = 0.0;

	g_dtime_in_sleep += (dtime_end2 - dtime_end);

	dtime_outside = 0.0;
	if(g_vbl_count > 1) {
		dtime_outside += (dtime_start - g_dtime_end_16ms);
	}
	g_dtime_outside_run_16ms += dtime_outside;
	g_dtime_end_16ms = dtime_end2;
#if 0
	if((g_vbl_count & 0xf) == 0) {
		printf("run_16ms end at %.3lf, dtime_16ms:%1.5lf out:%1.5lf\n",
			dtime_end, dtime_end - dtime_start, dtime_outside);
	}
#endif
	return ret | g_a2_fatal_err;
}

int
run_a2_one_vbl()
{
	Fplus	*fplus_ptr;
	Event	*this_event, *db1;
	double	now_dtime, prev_dtime, fspeed_mult;
	dword64	dfcyc, dfcyc_stop, prerun_dfcyc;
	word32	ret, zip_speed_0tof, zip_speed_0tof_new;
	int	zip_en, zip_follow_cps, type, motor_on, iwm_1, iwm_25, fast;
	int	limit_speed, apple35_sel, zip_speed, faster_than_28, unl_speed;

	fflush(stdout);

	g_cur_sim_dtime = 0.0;

	g_recip_projected_pmhz_slow.dplus_1 = 0x10000;
	g_recip_projected_pmhz_slow.dplus_x_minus_1 = (dword64)(0.9 * 0x10000);

	g_recip_projected_pmhz_fast.dplus_1 = (dword64)(0x10000 / 2.8);
	g_recip_projected_pmhz_fast.dplus_x_minus_1 = (dword64)
				((1.98 - (1.0/2.8)) * 0x10000);

	zip_speed_0tof = g_zipgs_reg_c05a & 0xf0;
	setup_zip_speeds();

	if(engine.fplus_ptr == 0) {
		g_recip_projected_pmhz_unl = g_recip_projected_pmhz_slow;
	}

	while(1) {
		fflush(stdout);

		if(g_irq_pending && !(engine.psr & 0x4)) {
			irq_printf("taking an irq!\n");
			take_irq();
			/* Interrupt! */
		}

		motor_on = g_iwm_motor_on;
		limit_speed = g_limit_speed;
		apple35_sel = g_iwm.state & IWM_STATE_C031_APPLE35SEL;
		zip_en = ((g_zipgs_reg_c05b & 0x10) == 0);
		zip_follow_cps = ((g_zipgs_reg_c059 & 0x8) != 0);
		zip_speed_0tof_new = g_zipgs_reg_c05a & 0xf0;
		fast = (g_c036_val_speed & 0x80) || (zip_en && !zip_follow_cps);

		if(zip_speed_0tof_new != zip_speed_0tof) {
			zip_speed_0tof = zip_speed_0tof_new;
			setup_zip_speeds();
		}

		iwm_1 = motor_on && !apple35_sel &&
				(g_c036_val_speed & 0x4) &&
				(g_slow_525_emul_wr || !g_fast_disk_emul);
		iwm_25 = (motor_on && apple35_sel) && !g_fast_disk_emul;
		faster_than_28 = fast && (!iwm_1 && !iwm_25) && zip_en &&
			((limit_speed == 0) || (limit_speed == 3));
		zip_speed = faster_than_28 &&
			((zip_speed_0tof != 0) || (limit_speed == 3) ||
							(g_zipgs_unlock >= 4) );
		unl_speed = faster_than_28 && !zip_speed;
		if(unl_speed) {
			/* use unlimited speed */
			fspeed_mult = g_projected_pmhz;
			fplus_ptr = &g_recip_projected_pmhz_unl;
		} else if(zip_speed) {
			fspeed_mult = g_zip_pmhz;
			fplus_ptr = &g_recip_projected_pmhz_zip;
		} else if(fast && !iwm_1 && (iwm_25 || (limit_speed != 1))) {
			fspeed_mult = 2.5;
			fplus_ptr = &g_recip_projected_pmhz_fast;
		} else {
			/* else run slow */
			fspeed_mult = 1.0;
			fplus_ptr = &g_recip_projected_pmhz_slow;
		}

		engine.fplus_ptr = fplus_ptr;

		prerun_dfcyc = g_cur_dfcyc;
		engine.dfcyc = prerun_dfcyc;
		dfcyc_stop = g_event_start.next->dfcyc + 1;
		if(g_stepping) {
			dfcyc_stop = prerun_dfcyc + 1;
		}
		g_dcycles_end = dfcyc_stop;

#if 0
		printf("Enter engine, fcycs: %f, stop: %f\n",
			prerun_fcycles, fcycles_stop);
		printf("g_cur_dfcyc:%016llx, last_vbl_dfcyc:%016llx\n",
			g_cur_dfcyc, g_last_vbl_dfcyc);
#endif

		g_num_enter_engine++;
		prev_dtime = get_dtime();

		ret = enter_engine(&engine);

		now_dtime = get_dtime();

		g_cur_sim_dtime += (now_dtime - prev_dtime);

		dfcyc = engine.dfcyc;
		g_cur_dfcyc = dfcyc;

		g_dadjcycs += (engine.dfcyc - prerun_dfcyc) * (1/65536.0) *
					fspeed_mult;

#if 0
		printf("...back, engine.dfcyc: %016llx, dfcyc: %016llx\n",
			(double)engine.dfcyc, dfcyc);
#endif

		if(ret != 0) {
			g_engine_action++;
			handle_action(ret);
		}

		this_event = g_event_start.next;
		while(dfcyc >= this_event->dfcyc) {
			/* Pop this guy off of the queue */
			g_event_start.next = this_event->next;

			type = this_event->type;
			this_event->next = g_event_free.next;
			g_event_free.next = this_event;
			dbg_log_info(dfcyc, type, 0, 0x101);
			switch(type & 0xff) {
			case EV_60HZ:
				update_60hz(dfcyc, now_dtime);
				return 0;
				break;
			case EV_STOP:
				printf("type: EV_STOP\n");
				printf("next: %p, dfcyc: %016llx\n",
						g_event_start.next, dfcyc);
				db1 = g_event_start.next;
				halt_printf("next.dfcyc:%016llx\n", db1->dfcyc);
				break;
			case EV_SCAN_INT:
				g_engine_scan_int++;
				irq_printf("type: scan int\n");
				do_scan_int(dfcyc, type >> 8);
				break;
			case EV_DOC_INT:
				g_engine_doc_int++;
				doc_handle_event(type >> 8, dfcyc);
				break;
			case EV_VBL_INT:
				do_vbl_int();
				break;
			case EV_SCC:
				scc_do_event(dfcyc, type >> 8);
				break;
			case EV_VID_UPD:
				video_update_event_line(type >> 8);
				break;
			case EV_MOCKINGBOARD:
				mockingboard_event(dfcyc);
				break;
			default:
				printf("Unknown event: %d!\n", type);
				exit(3);
			}

			this_event = g_event_start.next;

		}

		if(g_event_start.next == 0) {
			halt_printf("ERROR...run_prog, event_start.n=0!\n");
		}

		if(g_halt_sim) {
			break;
		}
		if(g_stepping) {
			break;
		}
	}

	printf("leaving run_prog, g_halt_sim:%d\n", g_halt_sim);

	return 0;
}

void
add_irq(word32 irq_mask)
{
	if(g_irq_pending & irq_mask) {
		/* Already requested, just get out */
		return;
	}
	g_irq_pending |= irq_mask;
	engine_recalc_events();
}

void
remove_irq(word32 irq_mask)
{
	g_irq_pending = g_irq_pending & (~irq_mask);
}

void
take_irq()
{
	word32	new_kpc, va;

	irq_printf("Taking irq, at: %02x/%04x, psw: %02x, dfcyc:%016llx\n",
			engine.kpc >> 16, engine.kpc & 0xffff, engine.psr,
			g_cur_dfcyc);

	g_num_irq++;
	if(g_wait_pending) {
		/* step over WAI instruction */
		engine.kpc = (engine.kpc & 0xff0000) |
						((engine.kpc + 1) & 0xffff);
		g_wait_pending = 0;
	}

	if(engine.psr & 0x100) {
		/* Emulation */
		set_memory_c(engine.stack, (engine.kpc >> 8) & 0xff, 1);
		engine.stack = ((engine.stack -1) & 0xff) + 0x100;

		set_memory_c(engine.stack, engine.kpc & 0xff, 1);
		engine.stack = ((engine.stack -1) & 0xff) + 0x100;

		set_memory_c(engine.stack, (engine.psr & 0xef), 1);
			/* Clear B bit in psr on stack */
		engine.stack = ((engine.stack -1) & 0xff) + 0x100;

		va = 0xfffffe;
	} else {
		/* native */
		set_memory_c(engine.stack, (engine.kpc >> 16) & 0xff, 1);
		engine.stack = ((engine.stack -1) & 0xffff);

		set_memory_c(engine.stack, (engine.kpc >> 8) & 0xff, 1);
		engine.stack = ((engine.stack -1) & 0xffff);

		set_memory_c(engine.stack, engine.kpc & 0xff, 1);
		engine.stack = ((engine.stack -1) & 0xffff);

		set_memory_c(engine.stack, engine.psr & 0xff, 1);
		engine.stack = ((engine.stack -1) & 0xffff);

		va = 0xffffee;
	}

	va = moremem_fix_vector_pull(va);
	new_kpc = get_memory_c(va);
	new_kpc = new_kpc + (get_memory_c(va + 1) << 8);

	engine.psr = ((engine.psr & 0x1f3) | 0x4);

	engine.kpc = new_kpc;
	HALT_ON(HALT_ON_IRQ, "Halting on IRQ\n");

}

double	g_dtime_last_vbl = 0.0;
double	g_dtime_expected = (1.0/VBL_RATE);	// Approximately 1.0/60.0

int g_scan_int_events = 0;

void
show_dtime_array()
{
	double	dfirst_time;
	double	first_total_cycs;
	int	i;
	int	pos;

	dfirst_time = 0.0;
	first_total_cycs = 0.0;


	for(i = 0; i < 60; i++) {
		pos = (g_vbl_index_count + i) % 60;
		printf("%2d:%2d dt:%.5f adjc:%9.1f this_vbl:%.6f "
			"exp:%.5f p:%2.2f ep:%2.2f\n",
			i, pos,
			dtime_array[pos] - dfirst_time,
			g_dadjcycs_array[pos] - first_total_cycs,
			g_dtime_this_vbl_array[pos],
			g_dtime_exp_array[pos] - dfirst_time,
			g_dtime_pmhz_array[pos],
			g_dtime_eff_pmhz_array[pos]);
		dfirst_time = dtime_array[pos];
		first_total_cycs = g_dadjcycs_array[pos];
	}
}


void
update_60hz(dword64 dfcyc, double dtime_now)
{
	register word32 end_time;
	char	status_buf[1024];
	char	sim_mhz_buf[128];
	char	total_mhz_buf[128];
	char	sp_buf[128];
	char	*sim_mhz_ptr, *total_mhz_ptr, *code_str1, *code_str2, *sp_str;
	dword64	planned_dcyc;
	double	eff_pmhz, predicted_pmhz, recip_predicted_pmhz;
	double	dtime_this_vbl_sim, dtime_diff_1sec, dratio, dtime_diff;
	double	dtime_till_expected, dtime_this_vbl, dadjcycs_this_vbl;
	double	dadj_cycles_1sec, dnatcycs_1sec;
	int	tmp, doit_3_persec, cur_vbl_index, prev_vbl_index;

	/* NOTE: this event is defined to occur before line 0 */
	/* It's actually happening at the start of the border for line (-1) */
	/* All other timings should be adjusted for this */

	irq_printf("vbl_60hz: vbl: %d, dfcyc:%016llx, last_vbl_dfcyc:%016llx\n",
		g_vbl_count, dfcyc, g_last_vbl_dfcyc);

	planned_dcyc = CYCLES_IN_16MS_RAW << 16;

	g_last_vbl_dfcyc = g_last_vbl_dfcyc + planned_dcyc;

	add_event_entry(g_last_vbl_dfcyc + planned_dcyc, EV_60HZ);
	check_for_one_event_type(EV_60HZ, 0xff);

	cur_vbl_index = g_vbl_index_count;

	/* figure out dtime spent running SIM, not all the overhead */
	dtime_this_vbl_sim = g_cur_sim_dtime;
	g_cur_sim_dtime = 0.0;
	g_sim_sum = g_sim_sum - sim_time[cur_vbl_index] + dtime_this_vbl_sim;
	sim_time[cur_vbl_index] = dtime_this_vbl_sim;

	dadj_cycles_1sec = g_dadjcycs - g_dadjcycs_array[cur_vbl_index];

	/* dtime_diff_1sec is dtime total spent over the last 60 ticks */
	dtime_diff_1sec = dtime_now - dtime_array[cur_vbl_index];

	dtime_array[cur_vbl_index] = dtime_now;
	g_dadjcycs_array[cur_vbl_index] = g_dadjcycs;

	prev_vbl_index = cur_vbl_index;
	cur_vbl_index = prev_vbl_index + 1;
	if(cur_vbl_index >= 60) {
		cur_vbl_index = 0;
	}
	g_vbl_index_count = cur_vbl_index;

	GET_ITIMER(end_time);
	g_dnatcycs_1sec += (double)(end_time - g_natcycs_lastvbl);
	g_natcycs_lastvbl = end_time;

	if(prev_vbl_index == 0) {
		if(g_sim_sum < (1.0/250.0)) {
			sim_mhz_ptr = "???";
			g_sim_mhz = 250.0;
		} else {
			g_sim_mhz = (dadj_cycles_1sec / g_sim_sum) /
							(1000.0*1000.0);
			if(g_sim_mhz > 8000.0) {
				g_sim_mhz = 8000.0;
			}
			snprintf(sim_mhz_buf, sizeof(sim_mhz_buf), "%6.2f",
								g_sim_mhz);
			sim_mhz_ptr = sim_mhz_buf;
		}
		if(dtime_diff_1sec < (1.0/250.0)) {
			total_mhz_ptr = "???";
		} else {
			snprintf(total_mhz_buf, sizeof(total_mhz_buf), "%6.2f",
				(dadj_cycles_1sec / dtime_diff_1sec) /
								(1000000.0));
			total_mhz_ptr = total_mhz_buf;
		}

		switch(g_limit_speed) {
		case 1:	sp_str = "1Mhz"; break;
		case 2:	sp_str = "2.8Mhz"; break;
		case 3:	sp_str = "8.0Mhz"; break;
		default: sp_str = "Unlimited"; break;
		}
		if(g_limit_speed == 3) {		// ZipGS
			snprintf(sp_buf, sizeof(sp_buf), "%1.1fMHz",
							g_zip_pmhz);
			sp_str = sp_buf;
		}

		snprintf(status_buf, sizeof(status_buf), "dfcyc:%7.1f sim "
			"MHz:%s Eff MHz:%s, sec:%1.3f vol:%02x Limit:%s",
			(double)(dfcyc >> 20)/65536.0, sim_mhz_ptr,
			total_mhz_ptr, dtime_diff_1sec, g_doc_vol, sp_str);
		video_update_status_line(0, status_buf);

		if(g_video_line_update_interval == 0) {
			if(g_sim_mhz > 12.0) {
				/* just set video line_ref_amt to 1 */
				g_line_ref_amt = 1;
			} else if(g_line_ref_amt == 1 && g_sim_mhz < 4.0) {
				g_line_ref_amt = 8;
			}
		} else {
			g_line_ref_amt = g_video_line_update_interval;
		}

		dnatcycs_1sec = g_dnatcycs_1sec;
		if(g_dnatcycs_1sec < (1000.0*1000.0)) {
			/* make it so large that all %'s become 0 */
			dnatcycs_1sec = 800.0*1000.0*1000.0*1000.0;
		}
		dnatcycs_1sec = dnatcycs_1sec / 100.0; /* eff mult by 100 */

		g_video_pixel_dcount = 0;

		code_str1 = "";
		code_str2 = "";
		if(g_code_yellow) {
			code_str1 = "Code: Yellow";
			code_str2 = "Emulated state suspect";
		}
		if(g_code_red) {
			code_str1 = "Code: RED";
			code_str2 = "Emulated state corrupt?";
		}
		snprintf(status_buf, sizeof(status_buf), "sleep_dtime:%8.6f, "
			"out_16ms:%8.6f, in_16ms:%8.6f, snd_pl:%d",
			g_dtime_in_sleep, g_dtime_outside_run_16ms,
			g_dtime_in_run_16ms, g_num_snd_plays);
		video_update_status_line(1, status_buf);

		draw_iwm_status(2, status_buf);

		snprintf(status_buf, sizeof(status_buf), " KEGS v%-6s       "
			"Press F4 for Config Menu    %s %s",
			g_kegs_version_str, code_str1, code_str2);
		video_update_status_line(3, status_buf);

		g_status_refresh_needed = 1;

		g_num_irq = 0;
		g_num_brk = 0;
		g_num_cop = 0;
		g_num_enter_engine = 0;
		g_io_amt = 0;
		g_engine_action = 0;
		g_engine_recalc_event = 0;
		g_engine_scan_int = 0;
		g_engine_doc_int = 0;

		g_cycs_in_40col = 0;
		g_cycs_in_xredraw = 0;
		g_cycs_in_refresh_line = 0;
		g_dnatcycs_1sec = 0.0;
		g_dtime_outside_run_16ms = 0.0;
		g_dtime_in_run_16ms = 0.0;
		g_refresh_bytes_xfer = 0;

		g_dtime_in_sleep = 0;
		g_num_snd_plays = 0;
		g_num_recalc_snd_parms = 0;
	}

	dtime_this_vbl = dtime_now - g_dtime_last_vbl;
	if(dtime_this_vbl < 0.001) {
		dtime_this_vbl = 0.001;
	}

	g_dtime_last_vbl = dtime_now;

	dadjcycs_this_vbl = g_dadjcycs - g_last_vbl_dadjcycs;
	g_last_vbl_dadjcycs = g_dadjcycs;

	g_dtime_expected += (1.0/VBL_RATE);	// Approx. 1/60

	eff_pmhz = (dadjcycs_this_vbl / dtime_this_vbl) / DCYCS_1_MHZ;

	/* using eff_pmhz, predict how many cycles can be run by */
	/*  g_dtime_expected */

	dtime_till_expected = g_dtime_expected - dtime_now;

	dratio = VBL_RATE * dtime_till_expected;	// Approx. 60*dtime_exp

	predicted_pmhz = eff_pmhz * dratio;

	if(! (predicted_pmhz < (1.4 * g_projected_pmhz))) {
		predicted_pmhz = 1.4 * g_projected_pmhz;
	}

	if(! (predicted_pmhz > (0.7 * g_projected_pmhz))) {
		predicted_pmhz = 0.7 * g_projected_pmhz;
	}

	if(!(predicted_pmhz >= 1.0)) {
		irq_printf("predicted: %f, setting to 1.0\n", predicted_pmhz);
		predicted_pmhz = 1.0;
	}

	if(!(predicted_pmhz < 4500.0)) {
		irq_printf("predicted: %f, set to 1900.0\n", predicted_pmhz);
		predicted_pmhz = 4500.0;
	}

	recip_predicted_pmhz = 1.0/predicted_pmhz;
	g_projected_pmhz = predicted_pmhz;

	g_recip_projected_pmhz_unl.dplus_1 = (dword64)
						(65536 * recip_predicted_pmhz);
	g_recip_projected_pmhz_unl.dplus_x_minus_1 =
			(dword64)(65536 * (1.01 - recip_predicted_pmhz));

	if(dtime_till_expected < -0.125) {
		/* If we were way off, get back on track */
		/* this happens because our sim took much longer than */
		/* expected, so we're going to skip some VBL */
		irq_printf("adj1: dtexp:%f, dt_new:%f\n",
			g_dtime_expected, dtime_now);

		dtime_diff = -dtime_till_expected;

		irq_printf("dtime_till_exp:%f, dtime_diff:%f, dfcyc:%016llx\n",
			dtime_till_expected, dtime_diff, dfcyc);

		g_dtime_expected += dtime_diff;
	}

	g_dtime_sleep = 0.0;
	if(dtime_till_expected > (1.0/VBL_RATE)) {
		/* we're running fast, usleep */
		g_dtime_sleep = dtime_till_expected - (1.0/VBL_RATE);
	}
#if 0
	printf("Sleep %f, till_exp:%f, dtime_now:%f, exp:%f\n",
		g_dtime_sleep, dtime_till_expected, dtime_now,
		g_dtime_expected);
#endif

	g_dtime_this_vbl_array[prev_vbl_index] = dtime_this_vbl;
	g_dtime_exp_array[prev_vbl_index] = g_dtime_expected;
	g_dtime_pmhz_array[prev_vbl_index] = predicted_pmhz;
	g_dtime_eff_pmhz_array[prev_vbl_index] = eff_pmhz;


	if(g_c041_val & C041_EN_VBL_INTS) {
		add_event_vbl();
	}

	g_25sec_cntr++;
	if(g_25sec_cntr >= 16) {
		g_25sec_cntr = 0;
		if(g_c041_val & C041_EN_25SEC_INTS) {
			add_irq(IRQ_PENDING_C046_25SEC);
			g_c046_val |= 0x10;
			irq_printf("Setting c046 .25 sec int, g_irq_pend:%d\n",
						g_irq_pending);
		}
	}

	g_1sec_cntr++;
	if(g_1sec_cntr >= 60) {
		g_1sec_cntr = 0;
		tmp = g_c023_val;
		tmp |= 0x40;	/* set 1sec int */
		if(tmp & 0x04) {
			tmp |= 0x80;
			add_irq(IRQ_PENDING_C023_1SEC);
			irq_printf("Setting c023 to %02x irq_pend: %d\n",
				tmp, g_irq_pending);
		}
		g_c023_val = tmp;
	}

	if(!g_scan_int_events) {
		check_scan_line_int(0);
	}

	doit_3_persec = 0;
	if(g_config_iwm_vbl_count > 0) {
		g_config_iwm_vbl_count--;
	} else {
		g_config_iwm_vbl_count = 20;
		doit_3_persec = 1;
	}

	iwm_vbl_update();
	config_vbl_update(doit_3_persec);

	sound_update(dfcyc);
	clock_update();
	scc_update(dfcyc);
	paddle_update_buttons();
}

void
do_vbl_int()
{
	if(g_c041_val & C041_EN_VBL_INTS) {
		g_c046_val |= 0x08;
		add_irq(IRQ_PENDING_C046_VBL);
		irq_printf("Setting c046 vbl_int_status to 1, irq_pend: %d\n",
			g_irq_pending);
	}
}

void
do_scan_int(dword64 dfcyc, int line)
{
	int	c023_val;

	if(dfcyc) {
		// Avoid unused param warning
	}

	g_scan_int_events = 0;
	g_dfcyc_scan_int = 0;

	c023_val = g_c023_val;
	if(c023_val & 0x20) {
		halt_printf("c023 scan_int and another on line %03x\n", line);
	}

#if 0
	dvbl = (dfcyc >> 16) / 17030;
	dline = ((dfcyc >> 16) - (dvbl * 17030)) / 65;
	printf("do_scan_int at time %lld (%lld,line %lld), line:%d, SCB:%02x, "
		"a2_stat_ok:%d, c023:%02x\n", dfcyc >> 16, dvbl, dline, line,
		(g_slow_memory_ptr[0x19d00 + line] & 0x40),
		(g_cur_a2_stat & ALL_STAT_SUPER_HIRES) != 0, c023_val);
	for(i = 0; i < 200; i++) {
		if(g_slow_memory_ptr[0x19d00 + i] & 0x40) {
			printf("  Line %d has SCB:%02x\n", i,
				g_slow_memory_ptr[0x19d00 + i]);
		}
	}
#endif

	/* make sure scan int is still enabled for this line */
	if((g_slow_memory_ptr[0x19d00 + line] & 0x40) &&
				(g_cur_a2_stat & ALL_STAT_SUPER_HIRES)) {
		/* valid interrupt, do it */
		c023_val |= 0xa0;	/* vgc_int and scan_int */
		if(c023_val & 0x02) {
			add_irq(IRQ_PENDING_C023_SCAN);
			irq_printf("Setting c023 to %02x, irq_pend: %d\n",
				c023_val, g_irq_pending);
		}
		g_c023_val = c023_val;
		HALT_ON(HALT_ON_SCAN_INT, "In do_scan_int\n");
	} else {
		/* scan int bit cleared on scan line control byte */
		/* look for next line, if any */
		check_scan_line_int(line+1);
	}
}

void
check_scan_line_int(int cur_video_line)
{
	dword64	ddelay;
	int	start, line;
	int	i;

	/* Called during VBL interrupt phase */

	if(!(g_cur_a2_stat & ALL_STAT_SUPER_HIRES)) {
		return;
	}

	if(g_c023_val & 0x20) {
		/* don't check for any more */
		return;
	}

	start = cur_video_line;
	if(start < 0) {
		halt_printf("check_scan_line_int: cur_video_line: %d\n",
			cur_video_line);
		start = 0;
	}

	for(line = start; line < 200; line++) {
		i = line;

		if(i < 0 || i >= 200) {
			halt_printf("check_new_scan_int:i:%d, line:%d, st:%d\n",
				i, line, start);
			i = 0;
		}
		if(g_slow_memory_ptr[0x19d00 + i] & 0x40) {
			irq_printf("Adding scan_int for line %d\n", i);
			ddelay = (65ULL * line) << 16;
			add_event_scan_int(g_last_vbl_dfcyc + ddelay, line);
			g_scan_int_events = 1;
			break;
		}
	}
}

void
check_for_new_scan_int(dword64 dfcyc)
{
	int	cur_video_line;

	cur_video_line = get_lines_since_vbl(dfcyc) >> 8;
	check_scan_line_int(cur_video_line);
}

void
scb_changed(dword64 dfcyc, word32 addr, word32 new_val, word32 old_val)
{
	if(new_val & (~old_val) & 0x40) {
		check_for_new_scan_int(dfcyc);
	}
	if(addr) {
	}
}

void
init_reg()
{
	memset(&engine, 0, sizeof(engine));

	engine.acc = 0;
	engine.xreg = 0;
	engine.yreg = 0;
	engine.stack = 0x1ff;
	engine.direct = 0;
	engine.psr = 0x134;
	engine.fplus_ptr = 0;
}

void
handle_action(word32 ret)
{
	int	type, arg;

	type = ret & 0xff;
	arg = ret >> 8;
	switch(type) {
	case RET_BREAK:
		do_break(arg);
		break;
	case RET_COP:
		do_cop(arg);
		break;
	case RET_IRQ:
		irq_printf("Special fast IRQ response.  irq_pending: %x\n",
			g_irq_pending);
		break;
	case RET_WDM:
		do_wdm(arg);
		break;
	case RET_STP:
		do_stp();
		break;
	case RET_TOOLTRACE:
		dbg_log_info(g_cur_dfcyc, engine.kpc, engine.xreg,
						(engine.stack << 16) | 0xe100);
		break;
	default:
		halt_printf("Unknown special action: %08x!\n", ret);
	}
}


void
do_break(word32 ret)
{
	printf("I think I got a break, second byte: %02x!\n", ret);
	printf("kpc: %06x\n", engine.kpc);

	halt_printf("do_break, kpc: %06x\n", engine.kpc);
}

void
do_cop(word32 ret)
{
	halt_printf("COP instr %02x!\n", ret);
	fflush(stdout);
}

void
do_wdm(word32 arg)
{
	if(arg == 0x00c7) {
		// WDM, 0xc7, 0x00: WDM in Slot 7
		if(engine.psr & 0x40) {
			// Overflow set: $C700 called
			do_c700(arg);
		} else if(engine.psr & 1) {	// V=0, C=1: $C70D called
			do_c70d(arg);
		} else {			// V=0, C=0, $C70A called
			do_c70a(arg);
		}
		return;
	}
	if(arg == 0x00ea) {
		// WDM, 0xea, 0x00: WDM emulator ID
		do_wdm_emulator_id();
		return;
	}
	if((arg == 0xeaea) && ((engine.psr & 0x171) == 0x41) &&
						(engine.acc == 0x4d44)) {
		// WDM $EA,$EA with V=1,C=1 and ACC=0x4d44 ("EM")
		engine.psr = engine.psr & 0x1bf;	// V=0
		// printf("WDM $EA,$EA, cleared V=0, psr:%04x\n", engine.psr);
		return;
	}

	switch(arg & 0xff) {
	case 0x8d: /* Bouncin Ferno does WDM 8d */
		break;
	case 0xea:	// Detectiong feature, don't flag an error
		break;
	case 0xfc:	// HOST.FST "head_call" for ATINIT for ProDOS 8
	case 0xfd:	// HOST.FST "tail_call" for ATINIT for ProDOS 8
	case 0xff:	// HOST.FST "call_host" for GS/OS driver
		break;
	default:
		halt_printf("do_wdm: %04x!\n", arg);
	}
}

void
do_wai()
{
	halt_printf("do_wai!\n");
}

void
do_stp()
{
	if(!g_stp_pending) {
		g_stp_pending = 1;
		halt_printf("Hit STP instruction at: %06x, press RESET to "
				"continue\n", engine.kpc);
	}
}

char g_emulator_name[64];

void
do_wdm_emulator_id()
{
	word32	addr, version, subvers;
	int	maxlen, len, c, got_dot;
	int	i;

	// WDM, $EA, $00: WDM emulator ID
	// dbank.acc = address to write emulator description string
	// X = size of buffer to hold string
	// Y = 0 (not checked)
	// Returns: X: actual length of emulator string (always <=
	//	value in X at call)
	// ACC: emulator version as: $VVMN as BCD, so 1.32 is $0132
	// Y: Emulation feature flags.  bit 0: $c06c-$c06f timer available
	// Works in emulation mode

	printf("WDM EA at %06x.  acc:%04x, dbank:%02x xreg:%04x\n", engine.kpc,
				engine.acc, engine.dbank, engine.xreg);
	maxlen = engine.xreg;
	cfg_strncpy(&g_emulator_name[0], "KEGS v", 64);
	cfg_strlcat(&g_emulator_name[0], &g_kegs_version_str[0], 64);
	len = (int)strlen(&g_emulator_name[0]);
	addr = engine.acc;
	engine.xreg = 0;
	for(i = 0; i < len; i++) {
		if(i >= maxlen) {
			break;
		}
		addr = (engine.dbank << 8) | (addr & 0xffff);
		set_memory_c(addr, 0x80 | g_emulator_name[i], 1);
		addr++;
		engine.xreg = i + 1;
	}

	version = 0;
	subvers = 0;
	len = (int)strlen(&g_kegs_version_str[0]);
	got_dot = 0;
	for(i = 0; i < len; i++) {
		c = g_kegs_version_str[i];
		if(c == '.') {
			got_dot++;
		}
		if(got_dot >= 3) {
			break;
		}
		if((c >= '0') && (c <= '9')) {
			c = c - '0';
			if(got_dot) {
				subvers = (subvers << 4) | c;
				got_dot++;
			} else {
				version = (version << 4) | c;
			}
		}
	}

	engine.acc = ((version & 0xff) << 8) | (subvers & 0xff);
	engine.yreg = 0x01;		// $C06C timer available
}

void
size_fail(int val, word32 v1, word32 v2)
{
	halt_printf("Size failure, val: %08x, %08x %08x\n", val, v1, v2);
}

int
fatal_printf(const char *fmt, ...)
{
	va_list	ap;
	int	ret;

	va_start(ap, fmt);

	if(g_fatal_log < 0) {
		g_fatal_log = 0;
	}
	ret = kegs_vprintf(fmt, ap);
	va_end(ap);

	return ret;
}

int
kegs_vprintf(const char *fmt, va_list ap)
{
	char	*bufptr, *buf2ptr;
	int	len, ret;

	bufptr = malloc(4096);
	ret = vsnprintf(bufptr, 4090, fmt, ap);

	len = (int)strlen(bufptr);
	if(g_fatal_log >= 0 && g_fatal_log < MAX_FATAL_LOGS) {
		buf2ptr = malloc(len+1);
		memcpy(buf2ptr, bufptr, len+1);
		g_fatal_log_strs[g_fatal_log++] = buf2ptr;
	}
	(void)must_write(1, (byte *)bufptr, len);
	if(g_debug_file_fd >= 0) {
		(void)must_write(g_debug_file_fd, (byte *)bufptr, len);
	}
	free(bufptr);

	return ret;
}

dword64
must_write(int fd, byte *bufptr, dword64 dsize)
{
	dword64	dlen;
	long long ret;
	word32	this_len;

	dlen = dsize;
	while(dlen != 0) {
		// Support Windows64, which can only rd/wr 2GB max per call
		this_len = (1UL << 30);
		if(dlen < this_len) {
			this_len = (word32)dlen;
		}
		ret = write(fd, bufptr, this_len);
		if(ret >= 0) {
			dlen -= ret;
			bufptr += ret;
		} else if((errno != EAGAIN) && (errno != EINTR)) {
			return 0;		// just get out
		}
	}
	return dsize;
}

void
clear_fatal_logs()
{
	int	i;

	for(i = 0; i < g_fatal_log; i++) {
		free(g_fatal_log_strs[i]);
		g_fatal_log_strs[i] = 0;
	}
	g_fatal_log = -1;
}

char *
kegs_malloc_str(const char *in_str)
{
	char	*str;
	int	len;

	len = (int)strlen(in_str) + 1;
	str = malloc(len);
	memcpy(str, in_str, len);

	return str;
}

dword64
kegs_lseek(int fd, dword64 offs, int whence)
{
#ifdef _WIN32
	return _lseeki64(fd, offs, whence);
#else
	return lseek(fd, offs, whence);
#endif
}

