// "@(#)$KmKId: engine.h,v 1.9 2023-09-11 12:55:16+00 kentd Exp $"

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

int
ENGINE_TYPE (Engine_reg *engine_ptr)
{
	register byte	*ptr;
	byte	*arg_ptr;
	Pc_log	*tmp_pc_ptr;
	Fplus	*fplus_ptr;
	byte	*stat;
	dword64	dfcyc, dplus_1, dcycles_tmp1, dplus_x_m1;
	register word32	kpc, acc, xreg, yreg, direct, psr, zero, neg7, addr;
	word32	wstat, arg, stack, dbank, opcode, addr_latch, tmp1, tmp2;
	word32	getmem_tmp, save_addr, pull_tmp, tmp_bytes, dummy1;

	tmp_pc_ptr = 0;
	dummy1 = 0;
	if(tmp_pc_ptr || dummy1) {	// "use" tmp_pc_ptr to avoid warning
	}

	kpc = engine_ptr->kpc;
	acc = engine_ptr->acc;
	xreg = engine_ptr->xreg;
	yreg = engine_ptr->yreg;
	stack = engine_ptr->stack;
	dbank = engine_ptr->dbank;
	direct = engine_ptr->direct;
	psr = engine_ptr->psr;
	fplus_ptr = engine_ptr->fplus_ptr;
	zero = !(psr & 2);
	neg7 = psr;

	dplus_1 = fplus_ptr->dplus_1;
	dplus_x_m1 = fplus_ptr->dplus_x_minus_1;
	dfcyc = engine_ptr->dfcyc;

	g_ret1 = 0;

	while(dfcyc <= g_dcycles_end) {

		FETCH_OPCODE;

		LOG_PC_MACRO();

		switch(opcode) {
		default:
			halt_printf("acc8 unk op: %02x\n", opcode);
			arg = 9
#include "defs_instr.h"
			* 2;
			break;
#include "instable.h"
			break;
		}
		LOG_PC_MACRO2();
	}

	engine_ptr->kpc = kpc;
	engine_ptr->acc = acc;
	engine_ptr->xreg = xreg;
	engine_ptr->yreg = yreg;
	engine_ptr->stack = stack;
	engine_ptr->dbank = dbank;
	engine_ptr->direct = direct;
	engine_ptr->dfcyc = dfcyc;

	psr = psr & (~0x82);
	psr |= (neg7 & 0x80);
	psr |= ((!zero) << 1);

	engine_ptr->psr = psr;

	return g_ret1;
}
