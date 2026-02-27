#ifdef INCLUDE_RCSID_C
const char rcsid_defc_h[] = "@(#)$KmKId: defc.h,v 1.142 2024-09-15 13:56:12+00 kentd Exp $";
#endif

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

#include "defcomm.h"

#define STRUCT(a) typedef struct a ## _st a; struct a ## _st

typedef unsigned char byte;
typedef unsigned short word16;
typedef unsigned int word32;
#if _MSC_VER
typedef unsigned __int64 dword64;
#else
typedef unsigned long long dword64;
#endif

/* 28MHz crystal, plus every 65th 1MHz cycle is stretched 140ns */
#define CYCS_28_MHZ		(28636360)
#define DCYCS_28_MHZ		(1.0*CYCS_28_MHZ)
#define CYCS_3_5_MHZ		(CYCS_28_MHZ/8)
#define DCYCS_1_MHZ		((DCYCS_28_MHZ/28.0)*(65.0*7/(65.0*7+1.0)))

// DCYCS_1_MHZ is 1020484.32016

#define CYCLES_IN_16MS_RAW	(262 * 65)
/* Use precisely 17030 instead of forcing 60 Hz since this is the number of */
/*  1MHz cycles per screen */

#define DCYCS_IN_16MS		((double)(CYCLES_IN_16MS_RAW))
#define DRECIP_DCYCS_IN_16MS	(1.0 / (DCYCS_IN_16MS))
#define VBL_RATE		(DCYCS_1_MHZ / DCYCS_IN_16MS)
// VBL rate is about 59.9227 frames/sec

#define MAXNUM_HEX_PER_LINE	32
#define MAX_SCALE_SIZE		5100

#ifdef __NeXT__
# include <libc.h>
#endif

#ifndef _WIN32
# include <unistd.h>
# include <sys/ioctl.h>
# include <sys/wait.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>
#ifdef HPUX
# include <machine/inline.h>		/* for GET_ITIMER */
#endif

#ifdef SOLARIS
# include <sys/filio.h>
#endif

#ifdef _WIN32
# include <direct.h>
# include <io.h>
# pragma warning(disable : 4996)	/* open() is deprecated...sigh */
int ftruncate(int fd, word32 length);
int lstat(const char *path, struct stat *bufptr);
#endif

#ifndef O_BINARY
/* work around some Windows junk */
# define O_BINARY	0
#endif

#define MAX_CHANGE_RECTS	20

#ifdef __GNUC__
int dbg_printf(const char *fmt, ...) __attribute__ ((
						__format__(printf, 1, 2)));
#endif

STRUCT(Pc_log) {
	dword64	dfcyc;
	word32	dbank_kpc;
	word32	instr;
	word32	psr_acc;
	word32	xreg_yreg;
	word32	stack_direct;
	word32	pad;
};

STRUCT(Data_log) {
	dword64	dfcyc;
	byte	*stat;
	word32	addr;
	word32	val;
	word32	size;
};

STRUCT(Event) {
	dword64	dfcyc;
	int	type;
	Event	*next;
};

STRUCT(Fplus) {
	dword64	dplus_1;
	dword64	dplus_x_minus_1;
};

STRUCT(Engine_reg) {
	dword64	dfcyc;
	word32	kpc;
	word32	acc;

	word32	xreg;
	word32	yreg;

	word32	stack;
	word32	dbank;

	word32	direct;
	word32	psr;
	Fplus	*fplus_ptr;
};

STRUCT(Break_point) {
	word32	start_addr;
	word32	end_addr;
	word32	acc_type;
};

STRUCT(Change_rect) {
	int	x;
	int	y;
	int	width;
	int	height;
};


STRUCT(Kimage) {
	word32	*wptr;
	int	a2_width_full;
	int	a2_height_full;
	int	a2_width;
	int	a2_height;
	int	x_width;
	int	x_height;
	int	x_refresh_needed;
	int	x_max_width;
	int	x_max_height;
	int	x_xpos;
	int	x_ypos;
	int	active;
	word32	vbl_of_last_resize;
	word32	c025_val;
	word32	scale_width_to_a2;
	word32	scale_width_a2_to_x;
	word32	scale_height_to_a2;
	word32	scale_height_a2_to_x;
	int	num_change_rects;
	Change_rect change_rect[MAX_CHANGE_RECTS];
	word32	scale_width[MAX_SCALE_SIZE + 1];
	word32	scale_height[MAX_SCALE_SIZE + 1];
};

typedef byte *Pg_info;
STRUCT(Page_info) {
	Pg_info rd_wr;
};

STRUCT(Cfg_menu) {
	const char *str;
	void	*ptr;
	const char *name_str;
	void	*defptr;
	int	cfgtype;
};

STRUCT(Cfg_dirent) {
	char	*name;
	int	is_dir;
	int	part_num;
	dword64	dsize;
	dword64	dimage_start;
	dword64	compr_dsize;
};

STRUCT(Cfg_listhdr) {
	Cfg_dirent *direntptr;
	int	max;
	int	last;
	int	invalid;

	int	curent;
	int	topent;

	int	num_to_show;
};

typedef void (Dbg_fn)(const char *str);

STRUCT(Dbg_longcmd) {
	const char *str;
	Dbg_fn	*fnptr;
	Dbg_longcmd *subptr;
	const char *help_str;
};

STRUCT(Emustate_intlist) {
	const char *str;
	word32	*iptr;
};

STRUCT(Emustate_dword64list) {
	const char *str;
	dword64	*dptr;
};

STRUCT(Emustate_word32list) {
	const char *str;
	word32	*wptr;
};

STRUCT(Lzw_state) {
	word32	table[4096 + 2];
	word32	entry;
	int	bits;
};

#ifdef __LP64__
# define PTR2WORD(a)	((word32)(unsigned long long)(a))
#else
# define PTR2WORD(a)	((word32)(unsigned long long)(a))
#endif


#define ALTZP	(g_c068_statereg & 0x80)
/* #define PAGE2 (g_c068_statereg & 0x40) */
#define RAMRD	(g_c068_statereg & 0x20)
#define RAMWRT	(g_c068_statereg & 0x10)
#define RDROM	(g_c068_statereg & 0x08)
#define LCBANK2	(g_c068_statereg & 0x04)
#define ROMB	(g_c068_statereg & 0x02)
// #define INTCX	(g_c068_statereg & 0x01)

#define C041_EN_25SEC_INTS	0x10
#define C041_EN_VBL_INTS	0x08
#define C041_EN_SWITCH_INTS	0x04
#define C041_EN_MOVE_INTS	0x02
#define C041_EN_MOUSE		0x01

/* WARNING: SCC1 and SCC0 interrupts must be in this order for scc.c */
/*  This order matches the SCC hardware, and SCC1_ZEROCNT must be 0x0001 */
#define IRQ_PENDING_SCC1_ZEROCNT	0x00001
#define IRQ_PENDING_SCC1_TX		0x00002
#define IRQ_PENDING_SCC1_RX		0x00004
#define IRQ_PENDING_SCC0_ZEROCNT	0x00008
#define IRQ_PENDING_SCC0_TX		0x00010
#define IRQ_PENDING_SCC0_RX		0x00020
#define IRQ_PENDING_C023_SCAN		0x00100
#define IRQ_PENDING_C023_1SEC		0x00200
#define IRQ_PENDING_C046_25SEC		0x00400
#define IRQ_PENDING_C046_VBL		0x00800
#define IRQ_PENDING_ADB_KBD_SRQ		0x01000
#define IRQ_PENDING_ADB_DATA		0x02000
#define IRQ_PENDING_ADB_MOUSE		0x04000
#define IRQ_PENDING_DOC			0x08000
#define IRQ_PENDING_MOCKINGBOARDA	0x10000
#define IRQ_PENDING_MOCKINGBOARDB	0x20000		/* must be BOARDA*2 */


#define EXTRU(val, pos, len)					\
	( ( (len) >= (pos) + 1) ? ((val) >> (31-(pos))) :	\
		(((val) >> (31-(pos)) ) & ( (1<<(len) ) - 1) ) )

#define DEP1(val, pos, old_val)					\
		(((old_val) & ~(1 << (31 - (pos))) ) |		\
			( ((val) & 1) << (31 - (pos))) )

#define set_halt(val) \
	if(val) { set_halt_act(val); }

#define clear_halt() \
	clr_halt_act()

#define GET_PAGE_INFO_RD(page) \
	(page_info_rd_wr[page].rd_wr)

#define GET_PAGE_INFO_WR(page) \
	(page_info_rd_wr[0x10000 + PAGE_INFO_PAD_SIZE + (page)].rd_wr)

#define SET_PAGE_INFO_RD(page,val) \
	;page_info_rd_wr[page].rd_wr = (Pg_info)val;

#define SET_PAGE_INFO_WR(page,val) \
	;page_info_rd_wr[0x10000 + PAGE_INFO_PAD_SIZE + (page)].rd_wr = \
							(Pg_info)val;

#define VERBOSE_DISK	0x001
#define VERBOSE_IRQ	0x002
#define VERBOSE_CLK	0x004
#define VERBOSE_SHADOW	0x008
#define VERBOSE_IWM	0x010
#define VERBOSE_DOC	0x020
#define VERBOSE_ADB	0x040
#define VERBOSE_SCC	0x080
#define VERBOSE_TEST	0x100
#define VERBOSE_VIDEO	0x200
#define VERBOSE_MAC	0x400
#define VERBOSE_DYNA	0x800

#ifdef NO_VERB
# define DO_VERBOSE	0
#else
# define DO_VERBOSE	1
#endif

#define disk_printf	if(DO_VERBOSE && (Verbose & VERBOSE_DISK)) printf
#define irq_printf	if(DO_VERBOSE && (Verbose & VERBOSE_IRQ)) printf
#define clk_printf	if(DO_VERBOSE && (Verbose & VERBOSE_CLK)) printf
#define shadow_printf	if(DO_VERBOSE && (Verbose & VERBOSE_SHADOW)) printf
#define iwm_printf	if(DO_VERBOSE && (Verbose & VERBOSE_IWM)) printf
#define doc_printf	if(DO_VERBOSE && (Verbose & VERBOSE_DOC)) printf
#define adb_printf	if(DO_VERBOSE && (Verbose & VERBOSE_ADB)) printf
#define scc_printf	if(DO_VERBOSE && (Verbose & VERBOSE_SCC)) printf
#define test_printf	if(DO_VERBOSE && (Verbose & VERBOSE_TEST)) printf
#define vid_printf	if(DO_VERBOSE && (Verbose & VERBOSE_VIDEO)) printf
#define mac_printf	if(DO_VERBOSE && (Verbose & VERBOSE_MAC)) printf
#define dyna_printf	if(DO_VERBOSE && (Verbose & VERBOSE_DYNA)) printf


#define HALT_ON_SCAN_INT	0x001
#define HALT_ON_IRQ		0x002
#define HALT_ON_SHADOW_REG	0x004
#define HALT_ON_C70D_WRITES	0x008

#define HALT_ON(a, msg)			\
	if(Halt_on & a) {		\
		halt_printf(msg);	\
	}


#define MY_MIN(a,b)	(((a) < (b)) ? (a) : (b))
#define MY_MAX(a,b)	(((a) > (b)) ? (a) : (b))

#define GET_ITIMER(dest)	dest = get_itimer();

#define FINISH(arg1, arg2)	g_ret1 = arg1 | ((arg2) << 8); g_dcycles_end=0;

#include "iwm.h"
#include "protos.h"
