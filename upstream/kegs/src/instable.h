// "@(#)$KmKId: instable.h,v 1.121 2023-11-12 15:31:14+00 kentd Exp $"

/************************************************************************/
/*			KEGS: Apple //gs Emulator			*/
/*			Copyright 2002-2021 by Kent Dickey		*/
/*									*/
/*	This code is covered by the GNU GPL v3				*/
/*	See the file COPYING.txt or https://www.gnu.org/licenses/	*/
/*	This program is provided with no warranty			*/
/*									*/
/*	The KEGS web page is kegs.sourceforge.net			*/
/*	You may contact the author at: kadickey@alumni.princeton.edu	*/
/************************************************************************/

case 0x00:			/*  brk */
	GET_1BYTE_ARG;
	g_num_brk++;
	INC_KPC_2;
	psr = (psr & (~0x82)) | (neg7 & 0x80) | ((!zero) << 1);
	if(psr & 0x100) {
		PUSH16(kpc & 0xffff);
		PUSH8(psr & 0xff);
		tmp1 = 0xfffffe;
		dbank = 0;
	} else {
		PUSH8(kpc >> 16);
		PUSH16(kpc);
		PUSH8(psr & 0xff);
		tmp1 = 0xffffe6;
		halt_printf("Halting for native break!\n");
	}
	tmp1 = moremem_fix_vector_pull(tmp1);
	GET_MEMORY16(tmp1, kpc, 0);
	kpc = kpc & 0xffff;
	psr |= 0x4;
	psr &= ~(0x8);
	break;

case 0x01:			/*  ORA (Dloc,X) */
	GET_DLOC_X_IND_RD();
	ORA_INST();
	break;

case 0x02:			/*  COP */
	g_num_cop++;
	INC_KPC_2;
	psr = (psr & ~0x82) | (neg7 & 0x80) | ((!zero) << 1);
	if(psr & 0x100) {
		halt_printf("Halting for emul COP at %04x\n", kpc);
		PUSH16(kpc & 0xffff);
		PUSH8(psr & 0xff);
		tmp1 = 0xfffff4;
		dbank = 0;
	} else {
		PUSH8(kpc >> 16);
		PUSH16(kpc & 0xffff);
		PUSH8(psr & 0xff);
		tmp1 = 0xffffe4;
	}
	tmp1 = moremem_fix_vector_pull(tmp1);
	GET_MEMORY16(tmp1, kpc, 0);
	kpc = kpc & 0xffff;
	psr |= 4;
	psr &= ~(0x8);
	break;

case 0x03:			/*  ORA Disp8,S */
	GET_DISP8_S_RD();
	ORA_INST();
	break;

case 0x04:			/*  TSB Dloc */
	GET_DLOC_RD_RMW();
	TSB_INST(1);
	break;

case 0x05:			/*  ORA Dloc */
	GET_DLOC_RD();
	ORA_INST();
	break;

case 0x06:			/*  ASL Dloc */
	GET_DLOC_RD_RMW();
	ASL_INST(1);
	break;

case 0x07:			/*  ORA [Dloc] */
	GET_DLOC_L_IND_RD();
	ORA_INST();
	break;

case 0x08:			/*  PHP */
	INC_KPC_1;
	psr = (psr & ~0x82) | (neg7 & 0x80) | ((!zero) << 1);
	PUSH8(psr);
	break;

case 0x09:			/*  ORA #imm */
	GET_IMM_MEM();
	ORA_INST();
	break;

case 0x0a:			/*  ASL a */
	INC_KPC_1;
	tmp1 = acc + acc;
#ifdef ACC8
	SET_CARRY8(tmp1);
	acc = (acc & 0xff00) + (tmp1 & 0xff);
	SET_NEG_ZERO8(acc & 0xff);
#else
	SET_CARRY16(tmp1);
	acc = tmp1 & 0xffff;
	SET_NEG_ZERO16(acc);
#endif
	break;

case 0x0b:			/*  PHD */
	INC_KPC_1;
	PUSH16_UNSAFE(direct);
	break;

case 0x0c:			/*  TSB abs */
	GET_ABS_RD_RMW();
	TSB_INST(0);
	break;

case 0x0d:			/*  ORA abs */
	GET_ABS_RD();
	ORA_INST();
	break;

case 0x0e:			/*  ASL abs */
	GET_ABS_RD_RMW();
	ASL_INST(0);
	break;

case 0x0f:			/*  ORA long */
	GET_LONG_RD();
	ORA_INST();
	break;

case 0x10:			/*  BPL disp8 */
	BRANCH_DISP8((neg7 & 0x80) == 0);
	break;

case 0x11:			/*  ORA (Dloc),y */
	GET_DLOC_IND_Y_RD();
	ORA_INST();
	break;

case 0x12:			/*  ORA (Dloc) */
	GET_DLOC_IND_RD();
	ORA_INST();
	break;

case 0x13:			/*  ORA (Disp8,s),y */
	GET_DISP8_S_IND_Y_RD();
	ORA_INST();
	break;

case 0x14:			/*  TRB Dloc */
	GET_DLOC_RD_RMW();
	TRB_INST(1);
	break;

case 0x15:			/*  ORA Dloc,x */
	GET_DLOC_X_RD();
	ORA_INST();
	break;

case 0x16:			/*  ASL Dloc,X */
	GET_DLOC_X_RD_RMW();
	ASL_INST(1);
	break;

case 0x17:			/*  ORA [Dloc],Y */
	GET_DLOC_L_IND_Y_RD();
	ORA_INST();
	break;

case 0x18:			/*  CLC */
	psr = psr & (~1);
	INC_KPC_1;
	break;

case 0x19:			/*  ORA abs,y */
	GET_ABS_Y_RD();
	ORA_INST();
	break;

case 0x1a:			/*  INC a */
	INC_KPC_1;
#ifdef ACC8
	acc = (acc & 0xff00) | ((acc + 1) & 0xff);
	SET_NEG_ZERO8(acc & 0xff);
#else
	acc = (acc + 1) & 0xffff;
	SET_NEG_ZERO16(acc);
#endif
	break;

case 0x1b:			/*  TCS */
	stack = acc;
	INC_KPC_1;
	if(psr & 0x100) {
		stack = (stack & 0xff) + 0x100;
	}
	break;

case 0x1c:			/*  TRB Abs */
	GET_ABS_RD_RMW();
	TRB_INST(0);
	break;

case 0x1d:			/*  ORA Abs,X */
	GET_ABS_X_RD();
	ORA_INST();
	break;

case 0x1e:			/*  ASL Abs,X */
	GET_ABS_X_RD_RMW();
	ASL_INST(0);
	break;

case 0x1f:			/*  ORA Long,X */
	GET_LONG_X_RD();
	ORA_INST();
	break;

case 0x20:			/*  JSR abs */
	GET_2BYTE_ARG;
	INC_KPC_2;
	PUSH16(kpc);
	kpc = (kpc & 0xff0000) + arg;
	CYCLES_PLUS_2;
	break;

case 0x21:			/*  AND (Dloc,X) */
	GET_DLOC_X_IND_RD();
	AND_INST();
	break;

case 0x22:			/*  JSL Long */
	GET_3BYTE_ARG;
	tmp1 = arg;
	CYCLES_PLUS_3;
	INC_KPC_3;
	PUSH24_UNSAFE(kpc);
	kpc = tmp1 & 0xffffff;
	break;

case 0x23:			/*  AND Disp8,S */
	GET_DISP8_S_RD();
	AND_INST();
	break;

case 0x24:			/*  BIT Dloc */
	GET_DLOC_RD();
	BIT_INST();
	break;

case 0x25:			/*  AND Dloc */
	GET_DLOC_RD();
	AND_INST();
	break;

case 0x26:			/*  ROL Dloc */
	GET_DLOC_RD_RMW();
	ROL_INST(1);
	break;

case 0x27:			/*  AND [Dloc] */
	GET_DLOC_L_IND_RD();
	AND_INST();
	break;

case 0x28:			/*  PLP */
	PULL8(tmp1);
	tmp2 = psr;
	CYCLES_PLUS_1;
	INC_KPC_1;
	psr = (psr & ~0xff) | (tmp1 & 0xff);
	zero = !(psr & 2);
	neg7 = psr;
	UPDATE_PSR(psr, tmp2);
	break;

case 0x29:			/*  AND #imm */
	GET_IMM_MEM();
	AND_INST();
	break;

case 0x2a:			/*  ROL a */
	INC_KPC_1;
#ifdef ACC8
	tmp1 = ((acc & 0xff) << 1) + (psr & 1);
	SET_CARRY8(tmp1);
	acc = (acc & 0xff00) + (tmp1 & 0xff);
	SET_NEG_ZERO8(tmp1 & 0xff);
#else
	tmp1 = (acc << 1) + (psr & 1);
	SET_CARRY16(tmp1);
	acc = (tmp1 & 0xffff);
	SET_NEG_ZERO16(acc);
#endif
	break;

case 0x2b:			/*  PLD */
	INC_KPC_1;
	PULL16_UNSAFE(direct);
	CYCLES_PLUS_1;
	SET_NEG_ZERO16(direct);
	break;

case 0x2c:			/*  BIT abs */
	GET_ABS_RD();
	BIT_INST();
	break;

case 0x2d:			/*  AND abs */
	GET_ABS_RD();
	AND_INST();
	break;

case 0x2e:			/*  ROL abs */
	GET_ABS_RD_RMW();
	ROL_INST(0);
	break;

case 0x2f:			/*  AND long */
	GET_LONG_RD();
	AND_INST();
	break;

case 0x30:			/*  BMI disp8 */
	BRANCH_DISP8(neg7 & 0x80);
	break;

case 0x31:			/*  AND (Dloc),y */
	GET_DLOC_IND_Y_RD();
	AND_INST();
	break;

case 0x32:			/*  AND (Dloc) */
	GET_DLOC_IND_RD();
	AND_INST();
	break;

case 0x33:			/*  AND (Disp8,s),y */
	GET_DISP8_S_IND_Y_RD();
	AND_INST();
	break;

case 0x34:			/*  BIT Dloc,x */
	GET_DLOC_X_RD();
	BIT_INST();
	break;

case 0x35:			/*  AND Dloc,x */
	GET_DLOC_X_RD();
	AND_INST();
	break;

case 0x36:			/*  ROL Dloc,X */
	GET_DLOC_X_RD_RMW();
	ROL_INST(1);
	break;

case 0x37:			/*  AND [Dloc],Y */
	GET_DLOC_L_IND_Y_RD();
	AND_INST();
	break;

case 0x38:			/*  SEC */
	psr = psr | 1;
	INC_KPC_1;
	break;

case 0x39:			/*  AND abs,y */
	GET_ABS_Y_RD();
	AND_INST();
	break;

case 0x3a:			/*  DEC a */
	INC_KPC_1;
#ifdef ACC8
	acc = (acc & 0xff00) | ((acc - 1) & 0xff);
	SET_NEG_ZERO8(acc & 0xff);
#else
	acc = (acc - 1) & 0xffff;
	SET_NEG_ZERO16(acc);
#endif
	break;

case 0x3b:			/*  TSC */
/*  set N,Z according to 16 bit acc */
	INC_KPC_1;
	acc = stack;
	SET_NEG_ZERO16(acc);
	break;

case 0x3c:			/*  BIT Abs,x */
	GET_ABS_X_RD();
	BIT_INST();
	break;

case 0x3d:			/*  AND Abs,X */
	GET_ABS_X_RD();
	AND_INST();
	break;

case 0x3e:			/*  ROL Abs,X */
	GET_ABS_X_RD_RMW();
	ROL_INST(0);
	break;

case 0x3f:			/*  AND Long,X */
	GET_LONG_X_RD();
	AND_INST();
	break;

case 0x40:			/*  RTI */
	CYCLES_PLUS_1
	if(psr & 0x100) {
		PULL24(tmp1);
		kpc = (kpc & 0xff0000) + ((tmp1 >> 8) & 0xffff);
		tmp2 = psr;
		psr = (psr & ~0xff) + (tmp1 & 0xff);
		neg7 = psr;
		zero = !(psr & 2);
		UPDATE_PSR(psr, tmp2);
	} else {
		PULL8(tmp1);
		tmp2 = psr;
		psr = (tmp1 & 0xff);
		neg7 = psr;
		zero = !(psr & 2);
		PULL24(kpc);
		UPDATE_PSR(psr, tmp2);
	}
	break;

case 0x41:			/*  EOR (Dloc,X) */
	GET_DLOC_X_IND_RD();
	EOR_INST();
	break;

case 0x42:			/*  WDM */
	GET_2BYTE_ARG;
	INC_KPC_2;
	if(arg < 0x100) {	// Next byte is 00
		INC_KPC_1;	// Skip over the BRK
	}
	FINISH(RET_WDM, arg);
	break;

case 0x43:			/*  EOR Disp8,S */
	GET_DISP8_S_RD();
	EOR_INST();
	break;

case 0x44:			/*  MVP */
	GET_2BYTE_ARG;
	/* arg & 0xff = dest bank, arg & 0xff00 = src bank */
	if(psr & 0x100) {
		halt_printf("MVP in emulation!\n");
		break;
	}
	dbank = arg & 0xff;
	tmp1 = (arg >> 8) & 0xff;
	CYCLES_PLUS_1;
	GET_MEMORY8((tmp1 << 16) + xreg, arg);
	SET_MEMORY8((dbank << 16) + yreg, arg);
	CYCLES_PLUS_2;
	xreg = (xreg - 1) & 0xffff;
	yreg = (yreg - 1) & 0xffff;
	if(psr & 0x10) {	// 8-bit index registers
		xreg = xreg & 0xff;
		yreg = yreg & 0xff;
	}
	acc = (acc - 1) & 0xffff;
	if(acc == 0xffff) {
		INC_KPC_3;
	}
	break;

case 0x45:			/*  EOR Dloc */
	GET_DLOC_RD();
	EOR_INST();
	break;

case 0x46:			/*  LSR Dloc */
	GET_DLOC_RD_RMW();
	LSR_INST(1);
	break;

case 0x47:			/*  EOR [Dloc] */
	GET_DLOC_L_IND_RD();
	EOR_INST();
	break;

case 0x48:			/*  PHA */
	INC_KPC_1;
#ifdef ACC8
	PUSH8(acc);
#else
	PUSH16(acc);
#endif
	break;

case 0x49:			/*  EOR #imm */
	GET_IMM_MEM();
	EOR_INST();
	break;

case 0x4a:			/*  LSR a */
	INC_KPC_1;
#ifdef ACC8
	tmp1 = ((acc & 0xff) >> 1);
	SET_CARRY8(acc << 8);
	acc = (acc & 0xff00) + (tmp1 & 0xff);
	SET_NEG_ZERO8(tmp1 & 0xff);
#else
	tmp1 = (acc >> 1);
	SET_CARRY8((acc << 8));
	acc = (tmp1 & 0xffff);
	SET_NEG_ZERO16(acc);
#endif
	break;

case 0x4b:			/*  PHK */
	PUSH8(kpc >> 16);
	INC_KPC_1;
	break;

case 0x4c:			/*  JMP abs */
	GET_2BYTE_ARG;
	CYCLES_PLUS_1;
	kpc = (kpc & 0xff0000) + arg;
	break;

case 0x4d:			/*  EOR abs */
	GET_ABS_RD();
	EOR_INST();
	break;

case 0x4e:			/*  LSR abs */
	GET_ABS_RD_RMW();
	LSR_INST(0);
	break;

case 0x4f:			/*  EOR long */
	GET_LONG_RD();
	EOR_INST();
	break;

case 0x50:			/*  BVC disp8 */
	BRANCH_DISP8((psr & 0x40) == 0);
	break;

case 0x51:			/*  EOR (Dloc),y */
	GET_DLOC_IND_Y_RD();
	EOR_INST();
	break;

case 0x52:			/*  EOR (Dloc) */
	GET_DLOC_IND_RD();
	EOR_INST();
	break;

case 0x53:			/*  EOR (Disp8,s),y */
	GET_DISP8_S_IND_Y_RD();
	EOR_INST();
	break;

case 0x54:			/*  MVN  */
	GET_2BYTE_ARG;
	/* arg & 0xff = dest bank, arg & 0xff00 = src bank */
	if(psr & 0x100) {
		halt_printf("MVN in emulation!\n");
		break;
	}
	dbank = arg & 0xff;
	tmp1 = (arg >> 8) & 0xff;
	CYCLES_PLUS_1;
	GET_MEMORY8((tmp1 << 16) + xreg, arg);
	SET_MEMORY8((dbank << 16) + yreg, arg);
	CYCLES_PLUS_2;
	xreg = (xreg + 1) & 0xffff;
	yreg = (yreg + 1) & 0xffff;
	if(psr & 0x10) {	// 8-bit index registers
		xreg = xreg & 0xff;
		yreg = yreg & 0xff;
	}
	acc = (acc - 1) & 0xffff;
	if(acc == 0xffff) {
		INC_KPC_3;
	}
	break;

case 0x55:			/*  EOR Dloc,x */
	GET_DLOC_X_RD();
	EOR_INST();
	break;

case 0x56:			/*  LSR Dloc,X */
	GET_DLOC_X_RD_RMW();
	LSR_INST(1);
	break;

case 0x57:			/*  EOR [Dloc],Y */
	GET_DLOC_L_IND_Y_RD();
	EOR_INST();
	break;

case 0x58:			/*  CLI */
	psr = psr & (~4);
	INC_KPC_1;
	if(((psr & 0x4) == 0) && g_irq_pending) {
		FINISH(RET_IRQ, 0);
	}
	break;

case 0x59:			/*  EOR abs,y */
	GET_ABS_Y_RD();
	EOR_INST();
	break;

case 0x5a:			/*  PHY */
	INC_KPC_1;
	if(psr & 0x10) {
		PUSH8(yreg);
	} else {
		PUSH16(yreg);
	}
	break;

case 0x5b:			/*  TCD */
	INC_KPC_1;
	direct = acc;
	SET_NEG_ZERO16(acc);
	break;

case 0x5c:			/*  JMP Long */
	GET_3BYTE_ARG;
	CYCLES_PLUS_2;
	kpc = arg;
	break;

case 0x5d:			/*  EOR Abs,X */
	GET_ABS_X_RD();
	EOR_INST();
	break;

case 0x5e:			/*  LSR Abs,X */
	GET_ABS_X_RD_RMW();
	LSR_INST(0);
	break;

case 0x5f:			/*  EOR Long,X */
	GET_LONG_X_RD();
	EOR_INST();
	break;

case 0x60:			/*  RTS */
	CYCLES_PLUS_2
	PULL16(tmp1);
	kpc = (kpc & 0xff0000) + ((tmp1 + 1) & 0xffff);
	break;

case 0x61:			/*  ADC (Dloc,X) */
	GET_DLOC_X_IND_RD();
	ADC_INST();
	break;

case 0x62:			/*  PER */
	GET_2BYTE_ARG;
	CYCLES_PLUS_2;
	INC_KPC_3;
	PUSH16_UNSAFE(kpc + arg);
	break;

case 0x63:			/*  ADC Disp8,S */
	GET_DISP8_S_RD();
	ADC_INST();
	break;

case 0x64:			/*  STZ Dloc */
	GET_DLOC_ADDR();
	STZ_INST(1);
	break;

case 0x65:			/*  ADC Dloc */
	GET_DLOC_RD();
	ADC_INST();
	break;

case 0x66:			/*  ROR Dloc */
	GET_DLOC_RD_RMW();
	ROR_INST(1);
	break;

case 0x67:			/*  ADC [Dloc] */
	GET_DLOC_L_IND_RD();
	ADC_INST();
	break;

case 0x68:			/*  PLA */
	INC_KPC_1;
	CYCLES_PLUS_1;
#ifdef ACC8
	PULL8(tmp1);
	acc = (acc & 0xff00) + tmp1;
	SET_NEG_ZERO8(tmp1);
#else
	PULL16(tmp1);
	acc = tmp1;
	SET_NEG_ZERO16(tmp1);
#endif
	break;

case 0x69:			/*  ADC #imm */
	GET_IMM_MEM();
	ADC_INST();
	break;

case 0x6a:			/*  ROR a */
	INC_KPC_1;
#ifdef ACC8
	tmp1 = ((acc & 0xff) >> 1) + ((psr & 1) << 7);
	SET_CARRY8((acc << 8));
	acc = (acc & 0xff00) + (tmp1 & 0xff);
	SET_NEG_ZERO8(tmp1 & 0xff);
#else
	tmp1 = (acc >> 1) + ((psr & 1) << 15);
	SET_CARRY16((acc << 16));
	acc = (tmp1 & 0xffff);
	SET_NEG_ZERO16(acc);
#endif
	break;

case 0x6b:			/*  RTL */
	CYCLES_PLUS_1;
	PULL24_UNSAFE(tmp1);
	kpc = (tmp1 & 0xff0000) + ((tmp1 + 1) & 0xffff);
	break;

case 0x6c:			/*  JMP (abs) */
	GET_2BYTE_ARG;
	CYCLES_PLUS_1;
	GET_MEMORY16(arg, tmp1, 1);
	if((psr & 0x100) && g_emul_6502_ind_page_cross_bug &&
					((arg & 0xff) == 0xff)) {
		GET_MEMORY8(arg - 0xff, tmp2);
		tmp1 = (tmp1 & 0xff) + (tmp2 << 8);
		halt_printf("Halting for emul ind jmp\n");
	}
	kpc = (kpc & 0xff0000) + tmp1;
	break;

case 0x6d:			/*  ADC abs */
	GET_ABS_RD();
	ADC_INST();
	break;

case 0x6e:			/*  ROR abs */
	GET_ABS_RD_RMW();
	ROR_INST(0);
	break;

case 0x6f:			/*  ADC long */
	GET_LONG_RD();
	ADC_INST();
	break;

case 0x70:			/*  BVS disp8 */
	BRANCH_DISP8((psr & 0x40));
	break;

case 0x71:			/*  ADC (Dloc),y */
	GET_DLOC_IND_Y_RD();
	ADC_INST();
	break;

case 0x72:			/*  ADC (Dloc) */
	GET_DLOC_IND_RD();
	ADC_INST();
	break;

case 0x73:			/*  ADC (Disp8,s),y */
	GET_DISP8_S_IND_Y_RD();
	ADC_INST();
	break;

case 0x74:			/*  STZ Dloc,x */
	GET_1BYTE_ARG;
	GET_DLOC_X_WR();
	STZ_INST(1);
	break;

case 0x75:			/*  ADC Dloc,x */
	GET_DLOC_X_RD();
	ADC_INST();
	break;

case 0x76:			/*  ROR Dloc,X */
	GET_DLOC_X_RD_RMW();
	ROR_INST(1);
	break;

case 0x77:			/*  ADC [Dloc],Y */
	GET_DLOC_L_IND_Y_RD();
	ADC_INST();
	break;

case 0x78:			/*  SEI */
	psr = psr | 4;
	INC_KPC_1;
	break;

case 0x79:			/*  ADC abs,y */
	GET_ABS_Y_RD();
	ADC_INST();
	break;

case 0x7a:			/*  PLY */
	INC_KPC_1;
	CYCLES_PLUS_1
	if(psr & 0x10) {
		PULL8(yreg);
		SET_NEG_ZERO8(yreg);
	} else {
		PULL16(yreg);
		SET_NEG_ZERO16(yreg);
	}
	break;

case 0x7b:			/*  TDC */
	INC_KPC_1;
	acc = direct;
	SET_NEG_ZERO16(direct);
	break;

case 0x7c:			/*  JMP (Abs,x) */
/*  always access kbank, xreg cannot wrap into next bank */
	GET_2BYTE_ARG;
	arg = (kpc & 0xff0000) + ((xreg + arg) & 0xffff);
	CYCLES_PLUS_2;
	GET_MEMORY16(arg, tmp1, 1);
	kpc = (kpc & 0xff0000) + tmp1;
	break;

case 0x7d:			/*  ADC Abs,X */
	GET_ABS_X_RD();
	ADC_INST();
	break;

case 0x7e:			/*  ROR Abs,X */
	GET_ABS_X_RD_RMW();
	ROR_INST(0);
	break;

case 0x7f:			/*  ADC Long,X */
	GET_LONG_X_RD();
	ADC_INST();
	break;

case 0x80:			/*  BRA */
	BRANCH_DISP8(1);
	break;

case 0x81:			/*  STA (Dloc,X) */
	GET_DLOC_X_IND_ADDR();
	STA_INST(0);
	break;

case 0x82:			/*  BRL disp16 */
	GET_2BYTE_ARG;
	CYCLES_PLUS_1;
	kpc = (kpc & 0xff0000) + ((kpc + 3 + arg) & 0xffff);
	break;

case 0x83:			/*  STA Disp8,S */
	GET_DISP8_S_ADDR();
	STA_INST(1);
	break;

case 0x84:			/*  STY Dloc */
	GET_DLOC_ADDR();
	STY_INST(1);
	break;

case 0x85:			/*  STA Dloc */
	GET_DLOC_ADDR();
	STA_INST(1);
	break;

case 0x86:			/*  STX Dloc */
	GET_DLOC_ADDR();
	STX_INST(1);
	break;

case 0x87:			/*  STA [Dloc] */
	GET_DLOC_L_IND_ADDR();
	STA_INST(0);
	break;

case 0x88:			/*  DEY */
	INC_KPC_1;
	SET_INDEX_REG(yreg - 1, yreg);
	break;

case 0x89:			/*  BIT #imm */
	GET_IMM_MEM();
#ifdef ACC8
	zero = (acc & arg) & 0xff;
#else
	zero = (acc & arg) & 0xffff;
#endif
	break;

case 0x8a:			/*  TXA */
	INC_KPC_1;
	arg = xreg;
	LDA_INST();
	break;

case 0x8b:			/*  PHB */
	INC_KPC_1;
	PUSH8(dbank);
	break;

case 0x8c:			/*  STY abs */
	GET_ABS_ADDR();
	STY_INST(0);
	break;

case 0x8d:			/*  STA abs */
	GET_ABS_ADDR();
	STA_INST(0);
	break;

case 0x8e:			/*  STX abs */
	GET_ABS_ADDR();
	STX_INST(0);
	break;

case 0x8f:			/*  STA long */
	GET_LONG_ADDR();
	STA_INST(0);
	break;

case 0x90:			/*  BCC disp8 */
	BRANCH_DISP8((psr & 0x01) == 0);
	break;

case 0x91:			/*  STA (Dloc),y */
	GET_DLOC_IND_Y_ADDR(1);
	STA_INST(0);
	break;

case 0x92:			/*  STA (Dloc) */
	GET_DLOC_IND_ADDR();
	STA_INST(0);
	break;

case 0x93:			/*  STA (Disp8,s),y */
	GET_DISP8_S_IND_Y_ADDR();
	STA_INST(0);
	break;

case 0x94:			/*  STY Dloc,x */
	GET_DLOC_X_ADDR();
	STY_INST(1);
	break;

case 0x95:			/*  STA Dloc,x */
	GET_DLOC_X_ADDR();
	STA_INST(1);
	break;

case 0x96:			/*  STX Dloc,Y */
	GET_DLOC_Y_ADDR();
	STX_INST(1);
	break;

case 0x97:			/*  STA [Dloc],Y */
	GET_DLOC_L_IND_Y_ADDR();
	STA_INST(0);
	break;

case 0x98:			/*  TYA */
	INC_KPC_1;
	arg = yreg;
	LDA_INST();
	break;

case 0x99:			/*  STA abs,y */
	GET_ABS_INDEX_ADDR(yreg, 1)
	STA_INST(0);
	break;

case 0x9a:			/*  TXS */
	stack = xreg;
	if(psr & 0x100) {
		stack = 0x100 | (stack & 0xff);
	}
	INC_KPC_1;
	break;

case 0x9b:			/*  TXY */
	SET_INDEX_REG(xreg, yreg);
	INC_KPC_1;
	break;

case 0x9c:			/*  STZ Abs */
	GET_ABS_ADDR();
	STZ_INST(0);
	break;

case 0x9d:			/*  STA Abs,X */
	GET_ABS_INDEX_ADDR(xreg, 1);
	STA_INST(0);
	break;

case 0x9e:			/*  STZ Abs,X */
	GET_ABS_INDEX_ADDR(xreg, 1);
	STZ_INST(0);
	break;

case 0x9f:			/*  STA Long,X */
	GET_LONG_X_ADDR_FOR_WR();
	STA_INST(0);
	break;

case 0xa0:			/*  LDY #imm */
	INC_KPC_2;
	if((psr & 0x10) == 0) {
		GET_2BYTE_ARG;
		CYCLES_PLUS_1
		INC_KPC_1;
	} else {
		GET_1BYTE_ARG;
	}
	SET_INDEX_REG(arg, yreg);
	break;

case 0xa1:			/*  LDA (Dloc,X) */
	GET_DLOC_X_IND_RD();
	LDA_INST();
	break;

case 0xa2:			/*  LDX #imm */
	INC_KPC_2;
	if((psr & 0x10) == 0) {
		GET_2BYTE_ARG;
		CYCLES_PLUS_1
		INC_KPC_1;
	} else {
		GET_1BYTE_ARG;
	}
	SET_INDEX_REG(arg, xreg);
	break;

case 0xa3:			/*  LDA Disp8,S */
	GET_DISP8_S_RD();
	LDA_INST();
	break;

case 0xa4:			/*  LDY Dloc */
	C_LDY_DLOC();
	break;

case 0xa5:			/*  LDA Dloc */
	GET_DLOC_RD();
	LDA_INST();
	break;

case 0xa6:			/*  LDX Dloc */
	C_LDX_DLOC();
	break;

case 0xa7:			/*  LDA [Dloc] */
	GET_DLOC_L_IND_RD();
	LDA_INST();
	break;

case 0xa8:			/*  TAY */
	INC_KPC_1;
	SET_INDEX_REG(acc, yreg);
	break;

case 0xa9:			/*  LDA #imm */
	GET_IMM_MEM();
	LDA_INST();
	break;

case 0xaa:			/*  TAX */
	INC_KPC_1;
	SET_INDEX_REG(acc, xreg);
	break;

case 0xab:			/*  PLB */
	INC_KPC_1;
	CYCLES_PLUS_1
	PULL8_UNSAFE(dbank);
	SET_NEG_ZERO8(dbank);
	break;

case 0xac:			/*  LDY abs */
	C_LDY_ABS();
	break;

case 0xad:			/*  LDA abs */
	GET_ABS_RD();
	LDA_INST();
	break;

case 0xae:			/*  LDX abs */
	C_LDX_ABS();
	break;

case 0xaf:			/*  LDA long */
	GET_LONG_RD();
	LDA_INST();
	break;

case 0xb0:			/*  BCS disp8 */
	BRANCH_DISP8((psr & 0x01));
	break;

case 0xb1:			/*  LDA (Dloc),y */
	GET_DLOC_IND_Y_RD();
	LDA_INST();
	break;

case 0xb2:			/*  LDA (Dloc) */
	GET_DLOC_IND_RD();
	LDA_INST();
	break;

case 0xb3:			/*  LDA (Disp8,s),y */
	GET_DISP8_S_IND_Y_RD();
	LDA_INST();
	break;

case 0xb4:			/*  LDY Dloc,x */
	C_LDY_DLOC_X();
	break;

case 0xb5:			/*  LDA Dloc,x */
	GET_DLOC_X_RD();
	LDA_INST();
	break;

case 0xb6:			/*  LDX Dloc,y */
	C_LDX_DLOC_Y();
	break;

case 0xb7:			/*  LDA [Dloc],Y */
	GET_DLOC_L_IND_Y_RD();
	LDA_INST();
	break;

case 0xb8:			/*  CLV */
	psr = psr & ~0x40;
	INC_KPC_1;
	break;

case 0xb9:			/*  LDA abs,y */
	GET_ABS_Y_RD();
	LDA_INST();
	break;

case 0xba:			/*  TSX */
	INC_KPC_1;
	SET_INDEX_REG(stack, xreg);
	break;

case 0xbb:			/*  TYX */
	INC_KPC_1;
	SET_INDEX_REG(yreg, xreg);
	break;

case 0xbc:			/*  LDY Abs,X */
	C_LDY_ABS_X();
	break;

case 0xbd:			/*  LDA Abs,X */
	GET_ABS_X_RD();
	LDA_INST();
	break;

case 0xbe:			/*  LDX Abs,y */
	C_LDX_ABS_Y();
	break;

case 0xbf:			/*  LDA Long,X */
	GET_LONG_X_RD();
	LDA_INST();
	break;

case 0xc0:			/*  CPY #imm */
	C_CPY_IMM();
	break;

case 0xc1:			/*  CMP (Dloc,X) */
	GET_DLOC_X_IND_RD();
	CMP_INST();
	break;

case 0xc2:			/*  REP #imm */
	GET_1BYTE_ARG;
	tmp2 = psr;
	CYCLES_PLUS_1;
	INC_KPC_2;
	psr = (psr & ~0x82) | (neg7 & 0x80) | ((!zero) << 1);
	psr = psr & ~(arg & 0xff);
	zero = !(psr & 2);
	neg7 = psr;
	UPDATE_PSR(psr, tmp2);
	break;

case 0xc3:			/*  CMP Disp8,S */
	GET_DISP8_S_RD();
	CMP_INST();
	break;

case 0xc4:			/*  CPY Dloc */
	C_CPY_DLOC();
	break;

case 0xc5:			/*  CMP Dloc */
	GET_DLOC_RD();
	CMP_INST();
	break;

case 0xc6:			/*  DEC Dloc */
	GET_DLOC_RD_RMW();
	DEC_INST(1);
	break;

case 0xc7:			/*  CMP [Dloc] */
	GET_DLOC_L_IND_RD();
	CMP_INST();
	break;

case 0xc8:			/*  INY */
	INC_KPC_1;
	SET_INDEX_REG(yreg + 1, yreg);
	break;

case 0xc9:			/*  CMP #imm */
	GET_IMM_MEM();
	CMP_INST();
	break;

case 0xca:			/*  DEX */
	INC_KPC_1;
	SET_INDEX_REG(xreg - 1, xreg);
	break;

case 0xcb:			/*  WAI */
	if(g_irq_pending) {
		g_wait_pending = 0;
		INC_KPC_1;
	} else {
		g_wait_pending = 1;
	}
	break;

case 0xcc:			/*  CPY abs */
	C_CPY_ABS();
	break;

case 0xcd:			/*  CMP abs */
	GET_ABS_RD();
	CMP_INST();
	break;

case 0xce:			/*  DEC abs */
	GET_ABS_RD_RMW();
	DEC_INST(0);
	break;

case 0xcf:			/*  CMP long */
	GET_LONG_RD();
	CMP_INST();
	break;

case 0xd0:			/*  BNE disp8 */
	BRANCH_DISP8(zero != 0);
	break;

case 0xd1:			/*  CMP (Dloc),y */
	GET_DLOC_IND_Y_RD();
	CMP_INST();
	break;

case 0xd2:			/*  CMP (Dloc) */
	GET_DLOC_IND_RD();
	CMP_INST();
	break;

case 0xd3:			/*  CMP (Disp8,s),y */
	GET_DISP8_S_IND_Y_RD();
	CMP_INST();
	break;

case 0xd4:			/*  PEI Dloc */
	GET_DLOC_ADDR()
	GET_MEMORY16(arg, arg, 1);
	CYCLES_PLUS_1;
	PUSH16_UNSAFE(arg);
	break;

case 0xd5:			/*  CMP Dloc,x */
	GET_DLOC_X_RD();
	CMP_INST();
	break;

case 0xd6:			/*  DEC Dloc,x */
	GET_DLOC_X_RD_RMW();
	DEC_INST(1);
	break;

case 0xd7:			/*  CMP [Dloc],Y */
	GET_DLOC_L_IND_Y_RD();
	CMP_INST();
	break;

case 0xd8:			/*  CLD */
	psr = psr & (~0x8);
	INC_KPC_1;
	break;

case 0xd9:			/*  CMP abs,y */
	GET_ABS_Y_RD();
	CMP_INST();
	break;

case 0xda:			/*  PHX */
	INC_KPC_1;
	if(psr & 0x10) {
		PUSH8(xreg);
	} else {
		PUSH16(xreg);
	}
	break;

case 0xdb:			/*  STP */
	FINISH(RET_STP, 0);
	break;

case 0xdc:			/*  JML (Abs) */
	GET_2BYTE_ARG;
	CYCLES_PLUS_1;
	GET_MEMORY24(arg, kpc, 1);
	break;

case 0xdd:			/*  CMP Abs,X */
	GET_ABS_X_RD();
	CMP_INST();
	break;

case 0xde:			/*  DEC Abs,X */
	GET_ABS_X_RD_RMW();
	DEC_INST(0);
	break;

case 0xdf:			/*  CMP Long,X */
	GET_LONG_X_RD();
	CMP_INST();
	break;

case 0xe0:			/*  CPX #imm */
	C_CPX_IMM();
	break;

case 0xe1:			/*  SBC (Dloc,X) */
	GET_DLOC_X_IND_RD();
	SBC_INST();
	break;

case 0xe2:			/*  SEP #imm */
	GET_1BYTE_ARG;
	tmp2 = psr;
	CYCLES_PLUS_1;
	INC_KPC_2;
	psr = (psr & ~0x82) | (neg7 & 0x80) | ((!zero) << 1);
	psr = psr | (arg & 0xff);
	zero = !(psr & 2);
	neg7 = psr;
	UPDATE_PSR(psr, tmp2);
	break;

case 0xe3:			/*  SBC Disp8,S */
	GET_DISP8_S_RD();
	SBC_INST();
	break;

case 0xe4:			/*  CPX Dloc */
	C_CPX_DLOC();
	break;

case 0xe5:			/*  SBC Dloc */
	GET_DLOC_RD();
	SBC_INST();
	break;

case 0xe6:			/*  INC Dloc */
	GET_DLOC_RD_RMW();
	INC_INST(1);
	break;

case 0xe7:			/*  SBC [Dloc] */
	GET_DLOC_L_IND_RD();
	SBC_INST();
	break;

case 0xe8:			/*  INX */
	INC_KPC_1;
	SET_INDEX_REG(xreg + 1, xreg);
	break;

case 0xe9:			/*  SBC #imm */
	GET_IMM_MEM();
	SBC_INST();
	break;

case 0xea:			/*  NOP */
	INC_KPC_1;
	break;

case 0xeb:			/*  XBA */
	tmp1 = acc & 0xff;
	CYCLES_PLUS_1
	acc = (tmp1 << 8) + (acc >> 8);
	INC_KPC_1;
	SET_NEG_ZERO8(acc & 0xff);
	break;

case 0xec:			/*  CPX abs */
	C_CPX_ABS();
	break;

case 0xed:			/*  SBC abs */
	GET_ABS_RD();
	SBC_INST();
	break;

case 0xee:			/*  INC abs */
	GET_ABS_RD_RMW();
	INC_INST(0);
	break;

case 0xef:			/*  SBC long */
	GET_LONG_RD();
	SBC_INST();
	break;

case 0xf0:			/*  BEQ disp8 */
	BRANCH_DISP8(zero == 0);
	break;

case 0xf1:			/*  SBC (Dloc),y */
	GET_DLOC_IND_Y_RD();
	SBC_INST();
	break;

case 0xf2:			/*  SBC (Dloc) */
	GET_DLOC_IND_RD();
	SBC_INST();
	break;

case 0xf3:			/*  SBC (Disp8,s),y */
	GET_DISP8_S_IND_Y_RD();
	SBC_INST();
	break;

case 0xf4:			/*  PEA Abs */
	GET_2BYTE_ARG;
	CYCLES_PLUS_1;
	INC_KPC_3;
	PUSH16_UNSAFE(arg);
	break;

case 0xf5:			/*  SBC Dloc,x */
	GET_DLOC_X_RD();
	SBC_INST();
	break;

case 0xf6:			/*  INC Dloc,x */
	GET_DLOC_X_RD_RMW();
	INC_INST(1);
	break;

case 0xf7:			/*  SBC [Dloc],Y */
	GET_DLOC_L_IND_Y_RD();
	SBC_INST();
	break;

case 0xf8:			/*  SED */
	INC_KPC_1;
	psr |= 0x8;
	break;

case 0xf9:			/*  SBC abs,y */
	GET_ABS_Y_RD();
	SBC_INST();
	break;

case 0xfa:			/*  PLX */
	INC_KPC_1;
	CYCLES_PLUS_1;
	if(psr & 0x10) {
		PULL8(xreg);
		SET_NEG_ZERO8(xreg);
	} else {
		PULL16(xreg);
		SET_NEG_ZERO16(xreg);
	}
	break;

case 0xfb:			/*  XCE */
	tmp2 = psr;
	INC_KPC_1;
	psr = (tmp2 & 0xfe) | ((tmp2 & 1) << 8) | ((tmp2 >> 8) & 1);
	UPDATE_PSR(psr, tmp2);
	break;

case 0xfc:			/*  JSR (Abs,X) */
	GET_2BYTE_ARG;
	INC_KPC_2;
	tmp1 = kpc;
	arg = (kpc & 0xff0000) + ((arg + xreg) & 0xffff);
	GET_MEMORY16(arg, tmp2, 1);
	kpc = (kpc & 0xff0000) + tmp2;
	CYCLES_PLUS_2
	PUSH16_UNSAFE(tmp1);
	break;

case 0xfd:			/*  SBC Abs,X */
	GET_ABS_X_RD();
	SBC_INST();
	break;

case 0xfe:			/*  INC Abs,X */
	GET_ABS_X_RD_RMW();
	INC_INST(0);
	break;

case 0xff:			/*  SBC Long,X */
	GET_LONG_X_RD();
	SBC_INST();
	break;

