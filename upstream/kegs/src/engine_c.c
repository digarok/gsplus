const char rcsid_engine_c_c[] = "@(#)$KmKId: engine_c.c,v 1.99 2025-04-27 18:54:08+00 kentd Exp $";

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

#include "defc.h"

// PSR[8:0] is E_NVMX_DIZC

extern int g_limit_speed;
extern int g_halt_sim;
extern int g_engine_recalc_event;
extern int g_code_red;
extern int g_ignore_halts;
extern int g_user_halt_bad;
extern dword64 g_dcycles_end;
extern dword64 g_last_vbl_dfcyc;
extern dword64 g_cur_dfcyc;
extern int g_wait_pending;
extern int g_irq_pending;
extern int g_num_brk;
extern int g_num_cop;
extern int g_emul_6502_ind_page_cross_bug;
extern byte *g_slow_memory_ptr;
extern byte *g_memory_ptr;
extern byte *g_rom_fc_ff_ptr;
extern byte *g_rom_cards_ptr;
extern byte *g_dummy_memory1_ptr;

extern int g_num_breakpoints;
extern Break_point g_break_pts[];

extern Kimage g_debugwin_kimage;

extern word32 g_log_pc_enable;
extern Pc_log *g_log_pc_ptr;
extern Pc_log *g_log_pc_start_ptr;
extern Pc_log *g_log_pc_end_ptr;

extern Data_log *g_log_data_ptr;
extern Data_log *g_log_data_start_ptr;
extern Data_log *g_log_data_end_ptr;

int	g_ret1 = 0;

int size_tab[] = {
#include "size_c.h"
};

int bogus[] = {
	0,
#include "op_routs.h"
};

#define INC_KPC_1	kpc = (kpc & 0xff0000) + ((kpc + 1) & 0xffff);
#define INC_KPC_2	kpc = (kpc & 0xff0000) + ((kpc + 2) & 0xffff);
#define INC_KPC_3	kpc = (kpc & 0xff0000) + ((kpc + 3) & 0xffff);
#define INC_KPC_4	kpc = (kpc & 0xff0000) + ((kpc + 4) & 0xffff);

#define CYCLES_PLUS_1	dfcyc += dplus_1;
#define CYCLES_PLUS_2	dfcyc += dplus_1 * 2;
#define CYCLES_PLUS_3	dfcyc += dplus_1 * 3;
#define CYCLES_PLUS_4	dfcyc += dplus_1 * 4;
#define CYCLES_PLUS_5	dfcyc += dplus_1 * 5;
#define CYCLES_MINUS_1	dfcyc -= dplus_1;
#define CYCLES_MINUS_2	dfcyc -= dplus_1 * 2;

#define FCYCLES_ROUND	dfcyc = dfcyc + dplus_x_m1;		\
			dfcyc = (dfcyc >> 16) << 16;


#define	GET_1BYTE_ARG	arg = arg_ptr[1];
#define	GET_2BYTE_ARG	arg = arg_ptr[1] + (arg_ptr[2] << 8);
#define	GET_3BYTE_ARG	arg = arg_ptr[1] + (arg_ptr[2] << 8) + (arg_ptr[3]<<16);

#define LOG_DATA_MACRO_ACT(in_addr, in_val, in_size, in_stat)		\
		g_log_data_ptr->dfcyc = dfcyc;				\
		g_log_data_ptr->stat = in_stat;				\
		g_log_data_ptr->addr = in_addr;				\
		g_log_data_ptr->val = in_val;				\
		g_log_data_ptr->size = in_size;				\
		g_log_data_ptr++;					\
		if(g_log_data_ptr >= g_log_data_end_ptr) {		\
			g_log_data_ptr = g_log_data_start_ptr;		\
		}

/* HACK HACK HACK */
#define	UPDATE_PSR(dummy, old_psr)				\
	if(psr & 0x100) {					\
		psr |= 0x30;					\
		stack = 0x100 + (stack & 0xff);			\
	}							\
	if((~old_psr & psr) & 0x10) {				\
		xreg = xreg & 0xff;				\
		yreg = yreg & 0xff;				\
	}							\
	if(((psr & 4) == 0) && g_irq_pending) {			\
		FINISH(RET_IRQ, 0);				\
	}							\
	if((old_psr ^ psr) & 0x20) {				\
		FINISH(RET_PSR, 0);				\
	}

extern Page_info page_info_rd_wr[];
extern word32 g_slow_mem_changed[];

#define GET_MEMORY8(addr,dest)					\
	addr_latch = (addr);					\
	CYCLES_PLUS_1;						\
	stat = GET_PAGE_INFO_RD(((addr) >> 8) & 0xffff);	\
	wstat = PTR2WORD(stat) & 0xff;				\
	ptr = stat - wstat + ((addr) & 0xff);			\
	if(wstat & (1 << (31 - BANK_IO_BIT))) {			\
		dcycles_tmp1 = dfcyc;				\
		dest = get_memory8_io_stub((addr), stat,	\
				&dcycles_tmp1, dplus_x_m1);	\
		dfcyc = dcycles_tmp1;				\
	} else {						\
		dest = *ptr;					\
	}

#define GET_MEMORY(addr,dest)	GET_MEMORY8(addr, dest)

#define GET_MEMORY16(addr, dest, in_bank)			\
	save_addr = addr;					\
	stat = GET_PAGE_INFO_RD(((addr) >> 8) & 0xffff);	\
	wstat = PTR2WORD(stat) & 0xff;				\
	ptr = stat - wstat + ((addr) & 0xff);			\
	if((wstat & (1 << (31 - BANK_IO_BIT))) || (((addr) & 0xff) == 0xff)) { \
		dcycles_tmp1 = dfcyc;				\
		dest = get_memory16_pieces_stub((addr), stat,	\
			&dcycles_tmp1, fplus_ptr, in_bank);	\
		dfcyc = dcycles_tmp1;				\
	} else {						\
		CYCLES_PLUS_2;					\
		dest = ptr[0] + (ptr[1] << 8);			\
	}							\
	addr_latch = save_addr;

#define GET_MEMORY24(addr, dest, in_bank)			\
	save_addr = addr;					\
	stat = GET_PAGE_INFO_RD(((addr) >> 8) & 0xffff);	\
	wstat = PTR2WORD(stat) & 0xff;				\
	ptr = stat - wstat + ((addr) & 0xff);			\
	if((wstat & (1 << (31 - BANK_IO_BIT))) || (((addr) & 0xfe) == 0xfe)) { \
		dcycles_tmp1 = dfcyc;				\
		dest = get_memory24_pieces_stub((addr), stat,	\
			&dcycles_tmp1, fplus_ptr, in_bank);	\
		dfcyc = dcycles_tmp1;				\
	} else {						\
		CYCLES_PLUS_3;					\
		dest = ptr[0] + (ptr[1] << 8) + (ptr[2] << 16);	\
	}							\
	addr_latch = save_addr;

#define GET_MEMORY_DIRECT_PAGE16(addr, dest, dloc_x_wrap)		\
	save_addr = addr;						\
	if(psr & 0x100) {						\
		if((direct & 0xff) == 0) {				\
			save_addr = (save_addr & 0xff) + direct;	\
		}							\
	}								\
	if((psr & 0x100) && (((addr) & 0xff) == 0xff)) {		\
		GET_MEMORY8(save_addr, getmem_tmp);			\
		if(dloc_x_wrap) {					\
			save_addr = (save_addr & 0xff00) |		\
					((save_addr + 1) & 0xff);	\
		} else {						\
			save_addr = (save_addr + 1) & 0xffff;		\
		}							\
		if((direct & 0xff) == 0) {				\
			save_addr = (save_addr & 0xff) + direct;	\
		}							\
		GET_MEMORY8(save_addr, dest);				\
		dest = (dest << 8) + getmem_tmp;			\
	} else {							\
		GET_MEMORY16(save_addr, dest, 1);			\
	}


#define PUSH8(arg)						\
	SET_MEMORY8(stack, arg);				\
	stack = (stack - 1) & 0xffff;				\
	if(psr & 0x100) {					\
		stack = 0x100 | (stack & 0xff);			\
	}

#define PUSH16(arg)						\
	if((stack & 0xfe) == 0) {				\
		/* stack will cross page! */			\
		PUSH8((arg) >> 8);				\
		PUSH8(arg);					\
	} else {						\
		stack = (stack - 2) & 0xffff;			\
		SET_MEMORY16(stack + 1, arg, 1);		\
	}

#define PUSH16_UNSAFE(arg)					\
	save_addr = (stack - 1) & 0xffff;			\
	stack = (stack - 2) & 0xffff;				\
	SET_MEMORY16(save_addr, arg, 1);			\
	if(psr & 0x100) {					\
		stack = 0x100 | (stack & 0xff);			\
	}

#define PUSH24_UNSAFE(arg)					\
	save_addr = (stack - 2) & 0xffff;			\
	stack = (stack - 3) & 0xffff;				\
	SET_MEMORY24(save_addr, arg, 1);			\
	if(psr & 0x100) {					\
		stack = 0x100 | (stack & 0xff);			\
	}

#define PULL8(dest)						\
	stack++;						\
	if(psr & 0x100) {					\
		stack = 0x100 | (stack & 0xff);			\
	}							\
	stack = stack & 0xffff;					\
	GET_MEMORY8(stack, dest);

#define PULL8_UNSAFE(dest)					\
	stack = (stack + 1) & 0xffff;				\
	GET_MEMORY8(stack, dest);				\
	if(psr & 0x100) {					\
		stack = 0x100 | (stack & 0xff);			\
	}

#define PULL16(dest)						\
	if((stack & 0xfe) == 0xfe) {	/* page cross */	\
		PULL8(dest);					\
		PULL8(pull_tmp);				\
		dest = (pull_tmp << 8) + dest;			\
	} else {						\
		GET_MEMORY16(stack + 1, dest, 1);		\
		stack = (stack + 2) & 0xffff;			\
		if(psr & 0x100) {				\
			stack = 0x100 | (stack & 0xff);		\
		}						\
	}

#define PULL16_UNSAFE(dest)					\
	stack = (stack + 1) & 0xffff;				\
	GET_MEMORY16(stack, dest, 1);				\
	stack = (stack + 1) & 0xffff;				\
	if(psr & 0x100) {					\
		stack = 0x100 | (stack & 0xff);			\
	}

#define PULL24(dest)						\
	if((stack & 0xfc) == 0xfc) {	/* page cross */	\
		PULL8(dest);					\
		PULL8(pull_tmp);				\
		pull_tmp = (pull_tmp << 8) + dest;		\
		PULL8(dest);					\
		dest = (dest << 16) + pull_tmp;			\
	} else {						\
		GET_MEMORY24(stack + 1, dest, 1);		\
		stack = (stack + 3) & 0xffff;			\
		if(psr & 0x100) {				\
			stack = 0x100 | (stack & 0xff);		\
		}						\
	}

#define PULL24_UNSAFE(dest)					\
	stack = (stack + 1) & 0xffff;				\
	GET_MEMORY24(stack, dest, 1);				\
	stack = (stack + 2) & 0xffff;				\
	if(psr & 0x100) {					\
		stack = 0x100 | (stack & 0xff);			\
	}

#define SET_MEMORY8(addr, val)						\
	stat = GET_PAGE_INFO_WR(((addr) >> 8) & 0xffff);		\
	LOG_DATA_MACRO(addr, val, 8, stat);				\
	CYCLES_PLUS_1;							\
	wstat = PTR2WORD(stat) & 0xff;					\
	ptr = stat - wstat + ((addr) & 0xff);				\
	if(wstat) {							\
		dcycles_tmp1 = dfcyc;					\
		set_memory8_io_stub((addr), val, stat, &dcycles_tmp1,	\
				dplus_x_m1);				\
		dfcyc = dcycles_tmp1;					\
	} else {							\
		*ptr = val;						\
	}


#define SET_MEMORY16(addr, val, in_bank)				\
	stat = GET_PAGE_INFO_WR(((addr) >> 8) & 0xffff);		\
	LOG_DATA_MACRO(addr, val, 16, stat);				\
	wstat = PTR2WORD(stat) & 0xff;					\
	ptr = stat - wstat + ((addr) & 0xff);				\
	if((wstat) || (((addr) & 0xff) == 0xff)) {			\
		dcycles_tmp1 = dfcyc;					\
		set_memory16_pieces_stub((addr), (val),			\
			&dcycles_tmp1, dplus_1, dplus_x_m1, in_bank);	\
		dfcyc = dcycles_tmp1;					\
	} else {							\
		CYCLES_PLUS_2;						\
		ptr[0] = (val);						\
		ptr[1] = (val) >> 8;					\
	}

#define SET_MEMORY24(addr, val, in_bank)				\
	stat = GET_PAGE_INFO_WR(((addr) >> 8) & 0xffff);		\
	LOG_DATA_MACRO(addr, val, 24, stat);				\
	wstat = PTR2WORD(stat) & 0xff;					\
	ptr = stat - wstat + ((addr) & 0xff);				\
	if((wstat) || (((addr) & 0xfe) == 0xfe)) {			\
		dcycles_tmp1 = dfcyc;					\
		set_memory24_pieces_stub((addr), (val),			\
			&dcycles_tmp1, fplus_ptr, in_bank);		\
		dfcyc = dcycles_tmp1;					\
	} else {							\
		CYCLES_PLUS_3;						\
		ptr[0] = (val);						\
		ptr[1] = (val) >> 8;					\
		ptr[2] = (val) >> 16;					\
	}


word32
get_memory8_io_stub(word32 addr, byte *stat, dword64 *dcycs_ptr,
							dword64 dplus_x_m1)
{
	dword64	dfcyc;
	word32	wstat;
	byte	*ptr;

	wstat = PTR2WORD(stat) & 0xff;
	dfcyc = *dcycs_ptr;
	if(wstat & BANK_BREAK) {
		check_breakpoints(addr, dfcyc, 0, 1);
	}
	if(wstat & BANK_IO2_TMP) {
		FCYCLES_ROUND;
		*dcycs_ptr = dfcyc;
		return get_memory_io((addr), dcycs_ptr);
	} else {
		ptr = stat - wstat + (addr & 0xff);
		return *ptr;
	}
}

word32
get_memory16_pieces_stub(word32 addr, byte *stat, dword64 *dcycs_ptr,
		Fplus	*fplus_ptr, int in_bank)
{
	byte	*ptr;
	dword64	dfcyc, dplus_1, dcycles_tmp1, dplus_x_m1;
	word32	addrp1, wstat, ret, tmp1, addr_latch;

	addr_latch = 0;
	if(addr_latch != 0) {	// "Use" addr_latch to avoid warning
	}
	dfcyc = *dcycs_ptr;
	dplus_1 = fplus_ptr->dplus_1;
	dplus_x_m1 = fplus_ptr->dplus_x_minus_1;
	GET_MEMORY8(addr, tmp1);
	addrp1 = addr + 1;
	if(in_bank) {
		addrp1 = (addr & 0xff0000) + (addrp1 & 0xffff);
	}
	GET_MEMORY8(addrp1, ret);
	*dcycs_ptr = dfcyc;
	return (ret << 8) + (tmp1);
}

word32
get_memory24_pieces_stub(word32 addr, byte *stat, dword64 *dcycs_ptr,
		Fplus *fplus_ptr, int in_bank)
{
	byte	*ptr;
	dword64	dfcyc, dplus_1, dcycles_tmp1, dplus_x_m1;
	word32	addrp1, addrp2, wstat, addr_latch, ret, tmp1, tmp2;

	addr_latch = 0;
	if(addr_latch != 0) {	// "Use" addr_latch to avoid warning
	}
	dfcyc = *dcycs_ptr;
	dplus_1 = fplus_ptr->dplus_1;
	dplus_x_m1 = fplus_ptr->dplus_x_minus_1;
	GET_MEMORY8(addr, tmp1);
	addrp1 = addr + 1;
	if(in_bank) {
		addrp1 = (addr & 0xff0000) + (addrp1 & 0xffff);
	}
	GET_MEMORY8(addrp1, tmp2);
	addrp2 = addr + 2;
	if(in_bank) {
		addrp2 = (addr & 0xff0000) + (addrp2 & 0xffff);
	}
	GET_MEMORY8(addrp2, ret);
	*dcycs_ptr = dfcyc;
	return (ret << 16) + (tmp2 << 8) + tmp1;
}

void
set_memory8_io_stub(word32 addr, word32 val, byte *stat, dword64 *dcycs_ptr,
		dword64 dplus_x_m1)
{
	byte	*ptr;
	dword64	dfcyc;
	word32	setmem_tmp1, tmp1, tmp2, wstat;

	wstat = PTR2WORD(stat) & 0xff;
	dfcyc = *dcycs_ptr;
	if(wstat & (1 << (31 - BANK_BREAK_BIT))) {
		check_breakpoints(addr, dfcyc, 0, 2);
	}
	ptr = stat - wstat + ((addr) & 0xff);
	if(wstat & (1 << (31 - BANK_IO2_BIT))) {
		FCYCLES_ROUND;
		*dcycs_ptr = dfcyc;
		set_memory_io((addr), val, dcycs_ptr);
	} else if(wstat & (1 << (31 - BANK_SHADOW_BIT))) {
		if(g_limit_speed) {
			FCYCLES_ROUND;
			*dcycs_ptr = dfcyc;
		}
		tmp1 = (addr & 0xffff);
		setmem_tmp1 = g_slow_memory_ptr[tmp1];
		*ptr = val;
		g_slow_memory_ptr[tmp1] = val;
		if(setmem_tmp1 != ((val) & 0xff)) {
			g_slow_mem_changed[tmp1 >> CHANGE_SHIFT] |=
				(1U << ((tmp1 >> SHIFT_PER_CHANGE) & 31));
		}
	} else if(wstat & (1 << (31 - BANK_SHADOW2_BIT))) {
		if(g_limit_speed) {
			FCYCLES_ROUND;
			*dcycs_ptr = dfcyc;
		}
		tmp2 = (addr & 0xffff);
		tmp1 = 0x10000 + tmp2;
		setmem_tmp1 = g_slow_memory_ptr[tmp1];
		*ptr = val;
		g_slow_memory_ptr[tmp1] = val;
		if(setmem_tmp1 != ((val) & 0xff)) {
			g_slow_mem_changed[tmp1 >> CHANGE_SHIFT] |=
				(1U << ((tmp1 >> SHIFT_PER_CHANGE) & 31));
			if((tmp1 & 0xff00) == 0x9d00) {
				scb_changed(dfcyc, tmp1, val, setmem_tmp1);
			}
		}
	} else {
		/* breakpoint only */
		*ptr = val;
	}
}

#define LOG_PC_MACRO()
#define LOG_PC_MACRO2()
#define LOG_DATA_MACRO(addr, val, size, in_stat)

void
set_memory16_pieces_stub(word32 addr, word32 val, dword64 *dcycs_ptr,
		dword64 dplus_1, dword64 dplus_x_m1, int in_bank)
{
	byte	*ptr;
	byte	*stat;
	dword64	dfcyc, dcycles_tmp1;
	word32	addrp1, wstat;

	dfcyc = *dcycs_ptr;
	SET_MEMORY8(addr, val);
	addrp1 = addr + 1;
	if(in_bank) {
		addrp1 = (addr & 0xff0000) + (addrp1 & 0xffff);
	}
	SET_MEMORY8(addrp1, val >> 8);

	*dcycs_ptr = dfcyc;
}

void
set_memory24_pieces_stub(word32 addr, word32 val, dword64 *dcycs_ptr,
		Fplus	*fplus_ptr, int in_bank)
{
	byte	*ptr;
	byte	*stat;
	dword64	dfcyc, dplus_1, dcycles_tmp1, dplus_x_m1;
	word32	addrp1, addrp2;
	word32	wstat;

	dfcyc = *dcycs_ptr;
	dplus_1 = fplus_ptr->dplus_1;
	dplus_x_m1 = fplus_ptr->dplus_x_minus_1;
	SET_MEMORY8(addr, val);
	addrp1 = addr + 1;
	if(in_bank) {
		addrp1 = (addr & 0xff0000) + (addrp1 & 0xffff);
	}
	SET_MEMORY8(addrp1, val >> 8);
	addrp2 = addr + 2;
	if(in_bank) {
		addrp2 = (addr & 0xff0000) + (addrp2 & 0xffff);
	}
	SET_MEMORY8(addrp2, val >> 16);

	*dcycs_ptr = dfcyc;
}

word32
get_memory_c(word32 addr)
{
	byte	*stat, *ptr;
	dword64	dfcyc, dplus_1, dcycles_tmp1, dplus_x_m1;
	word32	addr_latch, wstat, ret;

	dfcyc = 0;
	dplus_1 = 0;
	dplus_x_m1 = 0;
	addr_latch = 0;
	if(addr_latch != 0) {	// "Use" addr_latch to avoid warning
	}
	GET_MEMORY8(addr, ret);
	return ret;
}

word32
get_memory16_c(word32 addr)
{
	return get_memory_c(addr) +
			(get_memory_c(addr+1) << 8);
}

word32
get_memory24_c(word32 addr)
{
	return get_memory_c(addr) +
			(get_memory_c(addr+1) << 8) +
			(get_memory_c(addr+2) << 16);
}

void
set_memory_c(word32 addr, word32 val, int do_log)
{
	byte	*stat, *ptr;
	dword64	dfcyc, dcycles_tmp1, dplus_1, dplus_x_m1;
	word32	wstat;

	dfcyc = g_cur_dfcyc;
	dplus_1 = 0;
	dplus_x_m1 = 0;
	SET_MEMORY8(addr, val);
	if(g_log_pc_enable && do_log) {
		LOG_DATA_MACRO_ACT(addr, val, 8, stat)
	}
}

void
set_memory16_c(word32 addr, word32 val, int do_log)
{
	byte	*stat, *ptr;
	dword64	dfcyc, dcycles_tmp1, dplus_1, dplus_x_m1;
	word32	wstat;

	dfcyc = g_cur_dfcyc;
	dplus_1 = 0;
	dplus_x_m1 = 0;
	SET_MEMORY16(addr, val, 0);
	if(g_log_pc_enable && do_log) {
		LOG_DATA_MACRO_ACT(addr, val, 16, stat)
	}
}

void
set_memory24_c(word32 addr, word32 val)
{
	set_memory_c(addr, val, 1);
	set_memory_c(addr + 1, val >> 8, 1);
	set_memory_c(addr + 2, val >> 16, 1);
}

word32
do_adc_sbc8(word32 in1, word32 in2, word32 psr, int sub)
{
	word32	sum, carry, overflow;
	word32	zero;
	int	decimal;

	overflow = 0;
	decimal = psr & 8;
	if(sub) {
		in2 = (in2 ^ 0xff);
	}
	if(!decimal) {
		sum = (in1 & 0xff) + in2 + (psr & 1);
		overflow = ((sum ^ in2) >> 1) & 0x40;
	} else {
		/* decimal */
		sum = (in1 & 0xf) + (in2 & 0xf) + (psr & 1);
		if(sub) {
			if(sum < 0x10) {
				sum = (sum - 0x6) & 0xf;
			}
		} else {
			if(sum >= 0xa) {
				sum = (sum - 0xa) | 0x10;
			}
		}

		sum = (in1 & 0xf0) + (in2 & 0xf0) + sum;
		overflow = ((sum >> 2) ^ (sum >> 1)) & 0x40;
		if(sub) {
			if(sum < 0x100) {
				sum = (sum + 0xa0) & 0xff;
			}
		} else {
			if(sum >= 0xa0) {
				sum += 0x60;
			}
		}
	}

	zero = ((sum & 0xff) == 0);
	carry = (sum >= 0x100);
	if((in1 ^ in2) & 0x80) {
		overflow = 0;
	}

	psr = psr & (~0xc3);
	psr = psr + (sum & 0x80) + overflow + (zero << 1) + carry;

	return (psr << 16) + (sum & 0xff);
}

word32
do_adc_sbc16(word32 in1, word32 in2, word32 psr, int sub)
{
	word32	sum, carry, overflow;
	word32	tmp1, tmp2;
	word32	zero;
	int	decimal;

	overflow = 0;
	decimal = psr & 8;
	if(!decimal) {
		if(sub) {
			in2 = (in2 ^ 0xffff);
		}
		sum = in1 + in2 + (psr & 1);
		overflow = ((sum ^ in2) >> 9) & 0x40;
	} else {
		/* decimal */
		if(sub) {
			tmp1 = do_adc_sbc8(in1 & 0xff, in2 & 0xff, psr, sub);
			psr = (tmp1 >> 16);
			tmp2 = do_adc_sbc8((in1 >> 8) & 0xff,
						(in2 >> 8) & 0xff, psr, sub);
			in2 = (in2 ^ 0xfffff);
		} else {
			tmp1 = do_adc_sbc8(in1 & 0xff, in2 & 0xff, psr, sub);
			psr = (tmp1 >> 16);
			tmp2 = do_adc_sbc8((in1 >> 8) & 0xff,
						(in2 >> 8) &0xff, psr, sub);
		}
		sum = ((tmp2 & 0xff) << 8) + (tmp1 & 0xff) +
					(((tmp2 >> 16) & 1) << 16);
		overflow = (tmp2 >> 16) & 0x40;
	}

	zero = ((sum & 0xffff) == 0);
	carry = (sum >= 0x10000);
	if((in1 ^ in2) & 0x8000) {
		overflow = 0;
	}

	psr = psr & (~0xc3);
	psr = psr + ((sum & 0x8000) >> 8) + overflow + (zero << 1) + carry;

	return (psr << 16) + (sum & 0xffff);
}

void
fixed_memory_ptrs_init()
{
	/* set g_slow_memory_ptr, g_rom_fc_ff_ptr, g_dummy_memory1_ptr, */
	/*  and rom_cards_ptr */

	g_slow_memory_ptr = memalloc_align(128*1024, 0, 0);
	g_dummy_memory1_ptr = memalloc_align(256, 1024, 0);
	g_rom_fc_ff_ptr = memalloc_align(256*1024, 512, 0);
	g_rom_cards_ptr = memalloc_align(16*256, 256, 0);

#if 0
	printf("g_memory_ptr: %08x, dummy_mem: %08x, slow_mem_ptr: %08x\n",
		(word32)g_memory_ptr, (word32)g_dummy_memory1_ptr,
		(word32)g_slow_memory_ptr);
	printf("g_rom_fc_ff_ptr: %08x, g_rom_cards_ptr: %08x\n",
		(word32)g_rom_fc_ff_ptr, (word32)g_rom_cards_ptr);
	printf("page_info_rd = %08x, page_info_wr end = %08x\n",
		(word32)&(page_info_rd_wr[0]),
		(word32)&(page_info_rd_wr[PAGE_INFO_PAD_SIZE+0x1ffff].rd_wr));
#endif
}

word32
get_itimer()
{
#if defined(__i386) && defined(__GNUC__)
	/* Here's my bad ia32 asm code to do rdtsc */
	/* Linux source uses: */
	/* asm volatile("rdtsc" : "=a"(ret) : : "edx"); */
	/* asm volatile("rdtsc" : "=%eax"(ret) : : "%edx"); */

	/* GCC bug report 2001-03/msg00786.html used: */
	/*register dword64 dtmp; */
	/*asm volatile ("rdtsc" : "=A" (dtmp)); */
	/*return (word32)dtmp; */

	register word32 ret;

	asm volatile ("rdtsc;movl %%eax,%0" : "=r"(ret) : : "%eax","%edx");

	return ret;
#else
# if defined(__POWERPC__) && defined(__GNUC__)
	register word32 ret;

	asm volatile ("mftb %0" : "=r"(ret));
	return ret;
# else
#  if defined(__x86_64__)
	register word32 ret, hi;
	//ret = __rdtsc();
	asm volatile ("rdtsc" : "=a" (ret), "=d" (hi));
	return ret;
#  else
#	if defined(__aarch64__)		/* 64-bit ARM architecture */
		register dword64 ret;
		asm volatile("mrs %0,CNTVCT_EL0" : "=r"(ret));
		return ret;
#	else
		return 0;
#	endif
#  endif
# endif
#endif
}

void
engine_recalc_events()
{
	g_dcycles_end = 0;		// End inner loop
	g_engine_recalc_event++;
}

void
set_halt_act(int val)
{
	if((val == 1) && g_ignore_halts && !g_user_halt_bad) {
		g_code_red++;
	} else {
		if(g_halt_sim == 0) {
			debugger_update_list_kpc();
		}
		g_halt_sim |= val;
		if(g_halt_sim) {
			video_set_active(&g_debugwin_kimage, 1);
		}
		g_dcycles_end = 0;
	}
}

void
clr_halt_act()
{
	g_halt_sim = 0;
}

word32
get_remaining_operands(word32 addr, word32 opcode, word32 psr,
					dword64 *dcyc_ptr, Fplus *fplus_ptr)
{
	byte	*stat, *ptr;
	dword64	dfcyc, dcycles_tmp1, dplus_1, dplus_x_m1;
	word32	addr_latch, wstat, save_addr, arg, addrp1;
	int	size;

	dfcyc = *dcyc_ptr;
	dplus_1 = fplus_ptr->dplus_1;
	dplus_x_m1 = fplus_ptr->dplus_x_minus_1;
	addr_latch = 0;
	if(addr_latch != 0) {	// "Use" addr_latch to avoid warning
	}

	size = size_tab[opcode];

	addrp1 = (addr & 0xff0000) + ((addr + 1) & 0xffff);
	switch(size) {
	case 0:
		// Always read pc+1 for single-byte opcodes
		GET_MEMORY8(addrp1, arg);
		arg = 0;	/* no args */
		break;
	case 1:
		GET_MEMORY8(addrp1, arg);
		break;		/* 1 arg, already done */
	case 2:
		GET_MEMORY16(addrp1, arg, 1);
		dfcyc -= dplus_1;
		break;
	case 3:
		GET_MEMORY24(addrp1, arg, 1);
		dfcyc = dfcyc - dplus_1 - dplus_1;
		break;
	case 4:
		if(psr & 0x20) {
			GET_MEMORY8(addrp1, arg);
		} else {
			GET_MEMORY16(addrp1, arg, 1);
			dfcyc -= dplus_1;
		}
		break;
	case 5:
		if(psr & 0x10) {
			GET_MEMORY8(addrp1, arg);
		} else {
			GET_MEMORY16(addrp1, arg, 1);
			dfcyc -= dplus_1;
		}
		break;
	default:
		printf("Unknown size: %d\n", size);
		arg = 0;
		exit(-2);
	}
	*dcyc_ptr = dfcyc;

	return arg;
}

#define FETCH_OPCODE							\
	addr = kpc;							\
	CYCLES_PLUS_2;							\
	stat = GET_PAGE_INFO_RD(((addr) >> 8) & 0xffff);		\
	wstat = PTR2WORD(stat) & 0xff;					\
	ptr = stat - wstat + ((addr) & 0xff);				\
	arg_ptr = ptr;							\
	opcode = *ptr;							\
	if((wstat & (1 << (31-BANK_IO_BIT))) || ((addr & 0xff) > 0xfc)) {\
		CYCLES_MINUS_1;						\
		if(wstat & BANK_BREAK) {				\
			check_breakpoints(addr, dfcyc, stack, 4);	\
		}							\
		if(wstat & (1 << (31 - BANK_IO2_BIT))) {		\
			FCYCLES_ROUND;					\
			dcycles_tmp1 = dfcyc;				\
			opcode = get_memory_io((addr), &dcycles_tmp1);	\
			dfcyc = dcycles_tmp1;				\
		} else {						\
			opcode = *ptr;					\
		}							\
		dcycles_tmp1 = dfcyc;					\
		arg = get_remaining_operands(addr, opcode, psr,		\
					&dcycles_tmp1, fplus_ptr);	\
		dfcyc = dcycles_tmp1;					\
		arg_ptr = (byte *)&tmp_bytes;				\
		arg_ptr[1] = arg;					\
		arg_ptr[2] = arg >> 8;					\
		arg_ptr[3] = arg >> 16;					\
	}


#define ACC8
#define IS_ACC16	0
#define ENGINE_TYPE enter_engine_acc8
#include "engine.h"
// The above creates enter_engine_acc8

#undef ACC8
#undef IS_ACC16
#undef ENGINE_TYPE
#define IS_ACC16	1
#define ENGINE_TYPE enter_engine_acc16
#include "engine.h"
// The above creates enter_engine_acc16

#undef LOG_PC_MACRO
#undef LOG_PC_MACRO2
#undef LOG_DATA_MACRO

#define LOG_PC_MACRO()							\
		tmp_pc_ptr = g_log_pc_ptr;				\
		tmp_pc_ptr->dbank_kpc = (dbank << 24) + kpc;		\
		tmp_pc_ptr->instr = (opcode << 24) + arg_ptr[1] +	\
			(arg_ptr[2] << 8) + (arg_ptr[3] << 16);		\
		tmp_pc_ptr->dfcyc = dfcyc - dplus_1 * 2;

#define LOG_PC_MACRO2()						\
		tmp_pc_ptr->psr_acc = ((psr & ~(0x82)) << 16) | acc |	\
			((neg7 & 0x80) << 16) | ((!zero) << 17);	\
		tmp_pc_ptr->xreg_yreg = (xreg << 16) + yreg;		\
		tmp_pc_ptr->stack_direct = (stack << 16) + direct;	\
		tmp_pc_ptr++;						\
		if(tmp_pc_ptr >= g_log_pc_end_ptr) {			\
			tmp_pc_ptr = g_log_pc_start_ptr;		\
		}							\
		g_log_pc_ptr = tmp_pc_ptr;

#define LOG_DATA_MACRO(in_addr, in_val, in_size, in_stat)		\
		LOG_DATA_MACRO_ACT(in_addr, in_val, in_size, in_stat)

#undef ACC8
#undef IS_ACC16
#undef ENGINE_TYPE
#define ACC8
#define IS_ACC16	0
#define ENGINE_TYPE enter_engine_acc8_log
#include "engine.h"
// The above creates enter_engine_acc8_log

#undef ACC8
#undef IS_ACC16
#undef ENGINE_TYPE
#define IS_ACC16	1
#define ENGINE_TYPE enter_engine_acc16_log
#include "engine.h"
// The above creates enter_engine_acc16_log


int
enter_engine(Engine_reg *engine_ptr)
{
	dword64	dcycles_end_save;
	int	ret;

	dcycles_end_save = g_dcycles_end;
	while(1) {
		if(g_log_pc_enable) {
			if(engine_ptr->psr & 0x20) {	// 8-bit accumulator
				ret = enter_engine_acc8_log(engine_ptr);
			} else {
				ret = enter_engine_acc16_log(engine_ptr);
			}
		} else {
			if(engine_ptr->psr & 0x20) {	// 8-bit accumulator
				ret = enter_engine_acc8(engine_ptr);
			} else {
				ret = enter_engine_acc16(engine_ptr);
			}
		}
		if((ret == RET_PSR) && !g_halt_sim) {
			g_dcycles_end = dcycles_end_save;
			continue;
		}
		return ret;
	}
}

