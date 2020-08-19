/*
  GSPLUS - Advanced Apple IIGS Emulator Environment
  Based on the KEGS emulator written by Kent Dickey
  See COPYRIGHT.txt for Copyright information
	See LICENSE.txt for license (GPL v2)
*/

inst00_SYM		/*  brk */
	GET_1BYTE_ARG;
	if(g_testing) {
		CYCLES_PLUS_2;
		FINISH(RET_BREAK, arg);
	}
	g_num_brk++;
	INC_KPC_2;
	if(psr & 0x100) {
		PUSH16(kpc & 0xffff);
		PUSH8(psr & 0xff);
		GET_MEMORY16(0xfffe, kpc, 0);
		dbank = 0;
	} else {
		PUSH8(kpc >> 16);
		PUSH16(kpc);
		PUSH8(psr & 0xff);
		GET_MEMORY16(0xffe6, kpc, 0);
		halt_printf("Halting for native break!\n");
	}
	kpc = kpc & 0xffff;
	psr |= 0x4;
	psr &= ~(0x8);

inst01_SYM		/*  ORA (Dloc,X) */
/*  called with arg = val to ORA in */
	GET_DLOC_X_IND_RD();
	ORA_INST();

inst02_SYM		/*  COP */
	g_num_cop++;
	INC_KPC_2;
	if(psr & 0x100) {
		halt_printf("Halting for emul COP at %04x\n", kpc);
		PUSH16(kpc & 0xffff);
		PUSH8(psr & 0xff);
		GET_MEMORY16(0xfff4, kpc, 0);
		dbank = 0;
	} else {
		PUSH8(kpc >> 16);
		PUSH16(kpc & 0xffff);
		PUSH8(psr & 0xff);
		GET_MEMORY16(0xffe4, kpc, 0);
	}
	kpc = kpc & 0xffff;
	psr |= 4;
	psr &= ~(0x8);

inst03_SYM		/*  ORA Disp8,S */
	GET_DISP8_S_RD();
	ORA_INST();

inst04_SYM		/*  TSB Dloc */
	GET_DLOC_RD();
	TSB_INST(1);

inst05_SYM		/*  ORA Dloc */
	GET_DLOC_RD();
	ORA_INST();

inst06_SYM		/*  ASL Dloc */
	GET_DLOC_RD();
	ASL_INST(1);

inst07_SYM		/*  ORA [Dloc] */
	GET_DLOC_L_IND_RD();
	ORA_INST();

inst08_SYM		/*  PHP */
	INC_KPC_1;
	psr = (psr & ~0x82) | ((neg & 1) << 7) | ((!zero) << 1);
	PUSH8(psr);


inst09_SYM		/*  ORA #imm */
	GET_IMM_MEM();
	ORA_INST();

inst0a_SYM		/*  ASL a */
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

inst0b_SYM		/*  PHD */
	INC_KPC_1;
	PUSH16_UNSAFE(direct);

inst0c_SYM		/*  TSB abs */
	GET_ABS_RD();
	TSB_INST(0);

inst0d_SYM		/*  ORA abs */
	GET_ABS_RD();
	ORA_INST();

inst0e_SYM		/*  ASL abs */
	GET_ABS_RD();
	ASL_INST(0);

inst0f_SYM		/*  ORA long */
	GET_LONG_RD();
	ORA_INST();


inst10_SYM		/*  BPL disp8 */
	BRANCH_DISP8(neg == 0);

inst11_SYM		/*  ORA (Dloc),y */
	GET_DLOC_IND_Y_RD();
	ORA_INST();

inst12_SYM		/*  ORA (Dloc) */
	GET_DLOC_IND_RD();
	ORA_INST();

inst13_SYM		/*  ORA (Disp8,s),y */
	GET_DISP8_S_IND_Y_RD();
	ORA_INST();

inst14_SYM		/*  TRB Dloc */
	GET_DLOC_RD();
	TRB_INST(1);

inst15_SYM		/*  ORA Dloc,x */
	GET_DLOC_X_RD();
	ORA_INST();

inst16_SYM		/*  ASL Dloc,X */
	GET_DLOC_X_RD();
	ASL_INST(1);

inst17_SYM		/*  ORA [Dloc],Y */
	GET_DLOC_L_IND_Y_RD();
	ORA_INST();

inst18_SYM		/*  CLC */
	psr = psr & (~1);
	INC_KPC_1;

inst19_SYM		/*  ORA abs,y */
	GET_ABS_Y_RD();
	ORA_INST();


inst1a_SYM		/*  INC a */
	INC_KPC_1;
#ifdef ACC8
	acc = (acc & 0xff00) | ((acc + 1) & 0xff);
	SET_NEG_ZERO8(acc & 0xff);
#else
	acc = (acc + 1) & 0xffff;
	SET_NEG_ZERO16(acc);
#endif

inst1b_SYM		/*  TCS */
	stack = acc;
	INC_KPC_1;
	if(psr & 0x100) {
		stack = (stack & 0xff) + 0x100;
	}

inst1c_SYM		/*  TRB Abs */
	GET_ABS_RD();
	TRB_INST(0);

inst1d_SYM		/*  ORA Abs,X */
	GET_ABS_X_RD();
	ORA_INST();

inst1e_SYM		/*  ASL Abs,X */
	GET_ABS_X_RD_WR();
	ASL_INST(0);

inst1f_SYM		/*  ORA Long,X */
	GET_LONG_X_RD();
	ORA_INST();


inst20_SYM		/*  JSR abs */
	GET_2BYTE_ARG;
	INC_KPC_2;
	PUSH16(kpc);
	kpc = (kpc & 0xff0000) + arg;
	CYCLES_PLUS_2;


inst21_SYM		/*  AND (Dloc,X) */
/*  called with arg = val to AND in */
	GET_DLOC_X_IND_RD();
	AND_INST();

inst22_SYM		/*  JSL Long */
	GET_3BYTE_ARG;
	tmp1 = arg;
	CYCLES_PLUS_3;
	INC_KPC_3;
	PUSH24_UNSAFE(kpc);
	kpc = tmp1 & 0xffffff;

inst23_SYM		/*  AND Disp8,S */
/*  called with arg = val to AND in */
	GET_DISP8_S_RD();
	AND_INST();

inst24_SYM		/*  BIT Dloc */
	GET_DLOC_RD();
	BIT_INST();

inst25_SYM		/*  AND Dloc */
/*  called with arg = val to AND in */
	GET_DLOC_RD();
	AND_INST();

inst26_SYM		/*  ROL Dloc */
	GET_DLOC_RD();
/*  save1 is now apple addr */
/*  ret0 is data */
	ROL_INST(1);

inst27_SYM		/*  AND [Dloc] */
	GET_DLOC_L_IND_RD();
	AND_INST();

inst28_SYM		/*  PLP */
	PULL8(tmp1);
	tmp2 = psr;
	CYCLES_PLUS_1;
	INC_KPC_1;
	psr = (psr & ~0xff) | (tmp1 & 0xff);
	zero = !(psr & 2);
	neg = (psr >> 7) & 1;
	UPDATE_PSR(psr, tmp2);
	

inst29_SYM		/*  AND #imm */
	GET_IMM_MEM();
	AND_INST();

inst2a_SYM		/*  ROL a */
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


inst2b_SYM		/*  PLD */
	INC_KPC_1;
	PULL16_UNSAFE(direct);
	CYCLES_PLUS_1;
	SET_NEG_ZERO16(direct);

inst2c_SYM		/*  BIT abs */
	GET_ABS_RD();
	BIT_INST();

inst2d_SYM		/*  AND abs */
	GET_ABS_RD();
	AND_INST();

inst2e_SYM		/*  ROL abs */
	GET_ABS_RD();
	ROL_INST(0);

inst2f_SYM		/*  AND long */
	GET_LONG_RD();
	AND_INST();


inst30_SYM		/*  BMI disp8 */
	BRANCH_DISP8(neg);

inst31_SYM		/*  AND (Dloc),y */
	GET_DLOC_IND_Y_RD();
	AND_INST();

inst32_SYM		/*  AND (Dloc) */
	GET_DLOC_IND_RD();
	AND_INST();

inst33_SYM		/*  AND (Disp8,s),y */
	GET_DISP8_S_IND_Y_RD();
	AND_INST();

inst34_SYM		/*  BIT Dloc,x */
	GET_DLOC_X_RD();
	BIT_INST();

inst35_SYM		/*  AND Dloc,x */
	GET_DLOC_X_RD();
	AND_INST();

inst36_SYM		/*  ROL Dloc,X */
	GET_DLOC_X_RD();
	ROL_INST(1);

inst37_SYM		/*  AND [Dloc],Y */
	GET_DLOC_L_IND_Y_RD();
	AND_INST();

inst38_SYM		/*  SEC */
	psr = psr | 1;
	INC_KPC_1;

inst39_SYM		/*  AND abs,y */
	GET_ABS_Y_RD();
	AND_INST();

inst3a_SYM		/*  DEC a */
	INC_KPC_1;
#ifdef ACC8
	acc = (acc & 0xff00) | ((acc - 1) & 0xff);
	SET_NEG_ZERO8(acc & 0xff);
#else
	acc = (acc - 1) & 0xffff;
	SET_NEG_ZERO16(acc);
#endif

inst3b_SYM		/*  TSC */
/*  set N,Z according to 16 bit acc */
	INC_KPC_1;
	acc = stack;
	SET_NEG_ZERO16(acc);

inst3c_SYM		/*  BIT Abs,x */
	GET_ABS_X_RD();
	BIT_INST();

inst3d_SYM		/*  AND Abs,X */
	GET_ABS_X_RD();
	AND_INST();

inst3e_SYM		/*  ROL Abs,X */
	GET_ABS_X_RD_WR();
	ROL_INST(0);

inst3f_SYM		/*  AND Long,X */
	GET_LONG_X_RD();
	AND_INST();


inst40_SYM		/*  RTI */
	CYCLES_PLUS_1
	if(psr & 0x100) {
		PULL24(tmp1);
		kpc = (kpc & 0xff0000) + ((tmp1 >> 8) & 0xffff);
		tmp2 = psr;
		psr = (psr & ~0xff) + (tmp1 & 0xff);
		neg = (psr >> 7) & 1;
		zero = !(psr & 2);
		UPDATE_PSR(psr, tmp2);
	} else {
		PULL8(tmp1);
		tmp2 = psr;
		psr = (tmp1 & 0xff);
		neg = (psr >> 7) & 1;
		zero = !(psr & 2);
		PULL24(kpc);
		UPDATE_PSR(psr, tmp2);
	}

inst41_SYM		/*  EOR (Dloc,X) */
/*  called with arg = val to EOR in */
	GET_DLOC_X_IND_RD();
	EOR_INST();

inst42_SYM		/*  WDM */
	GET_1BYTE_ARG;
	INC_KPC_2;
	CYCLES_PLUS_5;
	CYCLES_PLUS_2;
	FINISH(RET_WDM, arg & 0xff);

inst43_SYM		/*  EOR Disp8,S */
/*  called with arg = val to EOR in */
	GET_DISP8_S_RD();
	EOR_INST();

inst44_SYM		/*  MVP */
	GET_2BYTE_ARG;
	/* arg & 0xff = dest bank, arg & 0xff00 = src bank */
	if(psr & 0x110) {
		halt_printf("MVP but not native m or x!\n");
		break;
	}
	CYCLES_MINUS_2
	dbank = arg & 0xff;
	tmp1 = (arg >> 8) & 0xff;
	while(1) {
		CYCLES_PLUS_3;
		GET_MEMORY8((tmp1 << 16) + xreg, arg);
		SET_MEMORY8((dbank << 16) + yreg, arg);
		CYCLES_PLUS_2;
		xreg = (xreg - 1) & 0xffff;
		yreg = (yreg - 1) & 0xffff;
		acc = (acc - 1) & 0xffff;
		if(acc == 0xffff) {
			INC_KPC_3;
			break;
		}
		if(fcycles >= g_fcycles_stop) {
			break;
		}
	}


inst45_SYM		/*  EOR Dloc */
/*  called with arg = val to EOR in */
	GET_DLOC_RD();
	EOR_INST();

inst46_SYM		/*  LSR Dloc */
	GET_DLOC_RD();
/*  save1 is now apple addr */
/*  ret0 is data */
	LSR_INST(1);

inst47_SYM		/*  EOR [Dloc] */
	GET_DLOC_L_IND_RD();
	EOR_INST();

inst48_SYM		/*  PHA */
	INC_KPC_1;
#ifdef ACC8
	PUSH8(acc);
#else
	PUSH16(acc);
#endif

inst49_SYM		/*  EOR #imm */
	GET_IMM_MEM();
	EOR_INST();

inst4a_SYM		/*  LSR a */
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

inst4b_SYM		/*  PHK */
	PUSH8(kpc >> 16);
	INC_KPC_1;

inst4c_SYM		/*  JMP abs */
	GET_2BYTE_ARG;
	CYCLES_PLUS_1;
	kpc = (kpc & 0xff0000) + arg;
	

inst4d_SYM		/*  EOR abs */
	GET_ABS_RD();
	EOR_INST();

inst4e_SYM		/*  LSR abs */
	GET_ABS_RD();
	LSR_INST(0);

inst4f_SYM		/*  EOR long */
	GET_LONG_RD();
	EOR_INST();


inst50_SYM		/*  BVC disp8 */
	BRANCH_DISP8((psr & 0x40) == 0);

inst51_SYM		/*  EOR (Dloc),y */
	GET_DLOC_IND_Y_RD();
	EOR_INST();

inst52_SYM		/*  EOR (Dloc) */
	GET_DLOC_IND_RD();
	EOR_INST();

inst53_SYM		/*  EOR (Disp8,s),y */
	GET_DISP8_S_IND_Y_RD();
	EOR_INST();

inst54_SYM		/*  MVN  */
	GET_2BYTE_ARG;
	/* arg & 0xff = dest bank, arg & 0xff00 = src bank */
	if(psr & 0x110) {
		halt_printf("MVN but not native m or x!\n");
		break;
	}
	CYCLES_MINUS_2;
	dbank = arg & 0xff;
	tmp1 = (arg >> 8) & 0xff;
	while(1) {
		CYCLES_PLUS_3;
		GET_MEMORY8((tmp1 << 16) + xreg, arg);
		SET_MEMORY8((dbank << 16) + yreg, arg);
		CYCLES_PLUS_2;
		xreg = (xreg + 1) & 0xffff;
		yreg = (yreg + 1) & 0xffff;
		acc = (acc - 1) & 0xffff;
		if(acc == 0xffff) {
			INC_KPC_3;
			break;
		}
		if(fcycles >= g_fcycles_stop) {
			break;
		}
	}

inst55_SYM		/*  EOR Dloc,x */
	GET_DLOC_X_RD();
	EOR_INST();

inst56_SYM		/*  LSR Dloc,X */
	GET_DLOC_X_RD();
	LSR_INST(1);

inst57_SYM		/*  EOR [Dloc],Y */
	GET_DLOC_L_IND_Y_RD();
	EOR_INST();

inst58_SYM		/*  CLI */
	psr = psr & (~4);
	INC_KPC_1;
	if(((psr & 0x4) == 0) && g_irq_pending) {
		FINISH(RET_IRQ, 0);
	}

inst59_SYM		/*  EOR abs,y */
	GET_ABS_Y_RD();
	EOR_INST();

inst5a_SYM		/*  PHY */
	INC_KPC_1;
	if(psr & 0x10) {
		PUSH8(yreg);
	} else {
		PUSH16(yreg);
	}

inst5b_SYM		/*  TCD */
	INC_KPC_1;
	direct = acc;
	SET_NEG_ZERO16(acc);

inst5c_SYM		/*  JMP Long */
	GET_3BYTE_ARG;
	CYCLES_PLUS_1;
	kpc = arg;

inst5d_SYM		/*  EOR Abs,X */
	GET_ABS_X_RD();
	EOR_INST();

inst5e_SYM		/*  LSR Abs,X */
	GET_ABS_X_RD_WR();
	LSR_INST(0);

inst5f_SYM		/*  EOR Long,X */
	GET_LONG_X_RD();
	EOR_INST();


inst60_SYM		/*  RTS */
	CYCLES_PLUS_2
	PULL16(tmp1);
	kpc = (kpc & 0xff0000) + ((tmp1 + 1) & 0xffff);

inst61_SYM		/*  ADC (Dloc,X) */
/*  called with arg = val to ADC in */
	GET_DLOC_X_IND_RD();
	ADC_INST();

inst62_SYM		/*  PER */
	GET_2BYTE_ARG;
	CYCLES_PLUS_2;
	INC_KPC_3;
	PUSH16_UNSAFE(kpc + arg);

inst63_SYM		/*  ADC Disp8,S */
/*  called with arg = val to ADC in */
	GET_DISP8_S_RD();
	ADC_INST();

inst64_SYM		/*  STZ Dloc */
	GET_DLOC_ADDR();
	STZ_INST(1);

inst65_SYM		/*  ADC Dloc */
/*  called with arg = val to ADC in */
	GET_DLOC_RD();
	ADC_INST();

inst66_SYM		/*  ROR Dloc */
	GET_DLOC_RD();
/*  save1 is now apple addr */
/*  ret0 is data */
	ROR_INST(1);

inst67_SYM		/*  ADC [Dloc] */
	GET_DLOC_L_IND_RD();
	ADC_INST();

inst68_SYM		/*  PLA */
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

inst69_SYM		/*  ADC #imm */
	GET_IMM_MEM();
	ADC_INST();

inst6a_SYM		/*  ROR a */
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

inst6b_SYM		/*  RTL */
	CYCLES_PLUS_1;
	PULL24(tmp1);
	kpc = (tmp1 & 0xff0000) + ((tmp1 + 1) & 0xffff);

inst6c_SYM		/*  JMP (abs) */
	GET_2BYTE_ARG;
	CYCLES_PLUS_1;
	GET_MEMORY16(arg, tmp1, 1);
	kpc = (kpc & 0xff0000) + tmp1;

inst6d_SYM		/*  ADC abs */
	GET_ABS_RD();
	ADC_INST();

inst6e_SYM		/*  ROR abs */
	GET_ABS_RD();
	ROR_INST(0);

inst6f_SYM		/*  ADC long */
	GET_LONG_RD();
	ADC_INST();


inst70_SYM		/*  BVS disp8 */
	BRANCH_DISP8((psr & 0x40));

inst71_SYM		/*  ADC (Dloc),y */
	GET_DLOC_IND_Y_RD();
	ADC_INST();

inst72_SYM		/*  ADC (Dloc) */
	GET_DLOC_IND_RD();
	ADC_INST();

inst73_SYM		/*  ADC (Disp8,s),y */
	GET_DISP8_S_IND_Y_RD();
	ADC_INST();

inst74_SYM		/*  STZ Dloc,x */
	GET_1BYTE_ARG;
	GET_DLOC_X_WR();
	STZ_INST(1);

inst75_SYM		/*  ADC Dloc,x */
	GET_DLOC_X_RD();
	ADC_INST();

inst76_SYM		/*  ROR Dloc,X */
	GET_DLOC_X_RD();
	ROR_INST(1);

inst77_SYM		/*  ADC [Dloc],Y */
	GET_DLOC_L_IND_Y_RD();
	ADC_INST();

inst78_SYM		/*  SEI */
	psr = psr | 4;
	INC_KPC_1;

inst79_SYM		/*  ADC abs,y */
	GET_ABS_Y_RD();
	ADC_INST();

inst7a_SYM		/*  PLY */
	INC_KPC_1;
	CYCLES_PLUS_1
	if(psr & 0x10) {
		PULL8(yreg);
		SET_NEG_ZERO8(yreg);
	} else {
		PULL16(yreg);
		SET_NEG_ZERO16(yreg);
	}

inst7b_SYM		/*  TDC */
	INC_KPC_1;
	acc = direct;
	SET_NEG_ZERO16(direct);

inst7c_SYM		/*  JMP (Abs,x) */
/*  always access kbank, xreg cannot wrap into next bank */
	GET_2BYTE_ARG;
	arg = (kpc & 0xff0000) + ((xreg + arg) & 0xffff);
	CYCLES_PLUS_2;
	GET_MEMORY16(arg, tmp1, 1);
	kpc = (kpc & 0xff0000) + tmp1;

inst7d_SYM		/*  ADC Abs,X */
	GET_ABS_X_RD();
	ADC_INST();

inst7e_SYM		/*  ROR Abs,X */
	GET_ABS_X_RD_WR();
	ROR_INST(0);

inst7f_SYM		/*  ADC Long,X */
	GET_LONG_X_RD();
	ADC_INST();

inst80_SYM		/*  BRA */
	BRANCH_DISP8(1);

inst81_SYM		/*  STA (Dloc,X) */
	GET_DLOC_X_IND_ADDR();
	STA_INST(0);

inst82_SYM		/*  BRL disp16 */
	GET_2BYTE_ARG;
	CYCLES_PLUS_1;
	kpc = (kpc & 0xff0000) + ((kpc + 3 + arg) & 0xffff);

inst83_SYM		/*  STA Disp8,S */
	GET_DISP8_S_ADDR();
	STA_INST(1);

inst84_SYM		/*  STY Dloc */
	GET_DLOC_ADDR();
	STY_INST(1);

inst85_SYM		/*  STA Dloc */
	GET_DLOC_ADDR();
	STA_INST(1);

inst86_SYM		/*  STX Dloc */
	GET_DLOC_ADDR();
	STX_INST(1);

inst87_SYM		/*  STA [Dloc] */
	GET_DLOC_L_IND_ADDR();
	STA_INST(0);

inst88_SYM		/*  DEY */
	INC_KPC_1;
	SET_INDEX_REG(yreg - 1, yreg);

inst89_SYM		/*  BIT #imm */
	GET_IMM_MEM();
#ifdef ACC8
	zero = (acc & arg) & 0xff;
#else
	zero = (acc & arg) & 0xffff;
#endif

inst8a_SYM		/*  TXA */
	INC_KPC_1;
	arg = xreg;
	LDA_INST();

inst8b_SYM		/*  PHB */
	INC_KPC_1;
	PUSH8(dbank);

inst8c_SYM		/*  STY abs */
	GET_ABS_ADDR();
	STY_INST(0);

inst8d_SYM		/*  STA abs */
	GET_ABS_ADDR();
	STA_INST(0);

inst8e_SYM		/*  STX abs */
	GET_ABS_ADDR();
	STX_INST(0);


inst8f_SYM		/*  STA long */
	GET_LONG_ADDR();
	STA_INST(0);


inst90_SYM		/*  BCC disp8 */
	BRANCH_DISP8((psr & 0x01) == 0);

inst91_SYM		/*  STA (Dloc),y */
	GET_DLOC_IND_Y_ADDR_FOR_WR();
	STA_INST(0);

inst92_SYM		/*  STA (Dloc) */
	GET_DLOC_IND_ADDR();
	STA_INST(0);

inst93_SYM		/*  STA (Disp8,s),y */
	GET_DISP8_S_IND_Y_ADDR();
	STA_INST(0);

inst94_SYM		/*  STY Dloc,x */
	GET_DLOC_X_ADDR();
	STY_INST(1);

inst95_SYM		/*  STA Dloc,x */
	GET_DLOC_X_ADDR();
	STA_INST(1);

inst96_SYM		/*  STX Dloc,Y */
	GET_DLOC_Y_ADDR();
	STX_INST(1);

inst97_SYM		/*  STA [Dloc],Y */
	GET_DLOC_L_IND_Y_ADDR();
	STA_INST(0);

inst98_SYM		/*  TYA */
	INC_KPC_1;
	arg = yreg;
	LDA_INST();

inst99_SYM		/*  STA abs,y */
	GET_ABS_INDEX_ADDR_FOR_WR(yreg)
	STA_INST(0);

inst9a_SYM		/*  TXS */
	stack = xreg;
	if(psr & 0x100) {
		stack = 0x100 | (stack & 0xff);
	}
	INC_KPC_1;

inst9b_SYM		/*  TXY */
	SET_INDEX_REG(xreg, yreg);
	INC_KPC_1;

inst9c_SYM		/*  STZ Abs */
	GET_ABS_ADDR();
	STZ_INST(0);

inst9d_SYM		/*  STA Abs,X */
	GET_ABS_INDEX_ADDR_FOR_WR(xreg);
	STA_INST(0);

inst9e_SYM		/*  STZ Abs,X */
	GET_ABS_INDEX_ADDR_FOR_WR(xreg);
	STZ_INST(0);

inst9f_SYM		/*  STA Long,X */
	GET_LONG_X_ADDR_FOR_WR();
	STA_INST(0);

insta0_SYM		/*  LDY #imm */
	INC_KPC_2;
	if((psr & 0x10) == 0) {
		GET_2BYTE_ARG;
		CYCLES_PLUS_1
		INC_KPC_1;
	} else {
		GET_1BYTE_ARG;
	}
	SET_INDEX_REG(arg, yreg);

insta1_SYM		/*  LDA (Dloc,X) */
/*  called with arg = val to LDA in */
	GET_DLOC_X_IND_RD();
	LDA_INST();

insta2_SYM		/*  LDX #imm */
	INC_KPC_2;
	if((psr & 0x10) == 0) {
		GET_2BYTE_ARG;
		CYCLES_PLUS_1
		INC_KPC_1;
	} else {
		GET_1BYTE_ARG;
	}
	SET_INDEX_REG(arg, xreg);

insta3_SYM		/*  LDA Disp8,S */
/*  called with arg = val to LDA in */
	GET_DISP8_S_RD();
	LDA_INST();

insta4_SYM		/*  LDY Dloc */
	C_LDY_DLOC();

insta5_SYM		/*  LDA Dloc */
/*  called with arg = val to LDA in */
	GET_DLOC_RD();
	LDA_INST();

insta6_SYM		/*  LDX Dloc */
	C_LDX_DLOC();

insta7_SYM		/*  LDA [Dloc] */
	GET_DLOC_L_IND_RD();
	LDA_INST();

insta8_SYM		/*  TAY */
	INC_KPC_1;
	SET_INDEX_REG(acc, yreg);

insta9_SYM		/*  LDA #imm */
	GET_IMM_MEM();
	LDA_INST();

instaa_SYM		/*  TAX */
	INC_KPC_1;
	SET_INDEX_REG(acc, xreg);

instab_SYM		/*  PLB */
	INC_KPC_1;
	CYCLES_PLUS_1
	PULL8(dbank);
	SET_NEG_ZERO8(dbank);

instac_SYM		/*  LDY abs */
	C_LDY_ABS();

instad_SYM		/*  LDA abs */
	GET_ABS_RD();
	LDA_INST();

instae_SYM		/*  LDX abs */
	C_LDX_ABS();

instaf_SYM		/*  LDA long */
	GET_LONG_RD();
	LDA_INST();


instb0_SYM		/*  BCS disp8 */
	BRANCH_DISP8((psr & 0x01));

instb1_SYM		/*  LDA (Dloc),y */
	GET_DLOC_IND_Y_RD();
	LDA_INST();

instb2_SYM		/*  LDA (Dloc) */
	GET_DLOC_IND_RD();
	LDA_INST();

instb3_SYM		/*  LDA (Disp8,s),y */
	GET_DISP8_S_IND_Y_RD();
	LDA_INST();

instb4_SYM		/*  LDY Dloc,x */
	C_LDY_DLOC_X();

instb5_SYM		/*  LDA Dloc,x */
	GET_DLOC_X_RD();
	LDA_INST();

instb6_SYM		/*  LDX Dloc,y */
	C_LDX_DLOC_Y();

instb7_SYM		/*  LDA [Dloc],Y */
	GET_DLOC_L_IND_Y_RD();
	LDA_INST();

instb8_SYM		/*  CLV */
	psr = psr & ~0x40;
	INC_KPC_1;

instb9_SYM		/*  LDA abs,y */
	GET_ABS_Y_RD();
	LDA_INST();

instba_SYM		/*  TSX */
	INC_KPC_1;
	SET_INDEX_REG(stack, xreg);

instbb_SYM		/*  TYX */
	INC_KPC_1;
	SET_INDEX_REG(yreg, xreg);

instbc_SYM		/*  LDY Abs,X */
	C_LDY_ABS_X();

instbd_SYM		/*  LDA Abs,X */
	GET_ABS_X_RD();
	LDA_INST();

instbe_SYM		/*  LDX Abs,y */
	C_LDX_ABS_Y();

instbf_SYM		/*  LDA Long,X */
	GET_LONG_X_RD();
	LDA_INST();


instc0_SYM		/*  CPY #imm */
	C_CPY_IMM();

instc1_SYM		/*  CMP (Dloc,X) */
/*  called with arg = val to CMP in */
	GET_DLOC_X_IND_RD();
	CMP_INST();

instc2_SYM		/*  REP #imm */
	GET_1BYTE_ARG;
	tmp2 = psr;
	CYCLES_PLUS_1;
	INC_KPC_2;
	psr = (psr & ~0x82) | ((neg & 1) << 7) | ((!zero) << 1);
	psr = psr & ~(arg & 0xff);
	zero = !(psr & 2);
	neg = (psr >> 7) & 1;
	UPDATE_PSR(psr, tmp2);

instc3_SYM		/*  CMP Disp8,S */
/*  called with arg = val to CMP in */
	GET_DISP8_S_RD();
	CMP_INST();

instc4_SYM		/*  CPY Dloc */
	C_CPY_DLOC();

instc5_SYM		/*  CMP Dloc */
	GET_DLOC_RD();
	CMP_INST();

instc6_SYM		/*  DEC Dloc */
	GET_DLOC_RD();
	DEC_INST(1);

instc7_SYM		/*  CMP [Dloc] */
	GET_DLOC_L_IND_RD();
	CMP_INST();

instc8_SYM		/*  INY */
	INC_KPC_1;
	SET_INDEX_REG(yreg + 1, yreg);

instc9_SYM		/*  CMP #imm */
	GET_IMM_MEM();
	CMP_INST();

instca_SYM		/*  DEX */
	INC_KPC_1;
	SET_INDEX_REG(xreg - 1, xreg);

instcb_SYM		/*  WAI */
	if(g_irq_pending) {
		g_wait_pending = 0;
		INC_KPC_1;
	} else {
		g_wait_pending = 1;
	}

instcc_SYM		/*  CPY abs */
	C_CPY_ABS();

instcd_SYM		/*  CMP abs */
	GET_ABS_RD();
	CMP_INST();

instce_SYM		/*  DEC abs */
	GET_ABS_RD();
	DEC_INST(0);

instcf_SYM		/*  CMP long */
	GET_LONG_RD();
	CMP_INST();

instd0_SYM		/*  BNE disp8 */
	BRANCH_DISP8(zero != 0);

instd1_SYM		/*  CMP (Dloc),y */
	GET_DLOC_IND_Y_RD();
	CMP_INST();

instd2_SYM		/*  CMP (Dloc) */
	GET_DLOC_IND_RD();
	CMP_INST();

instd3_SYM		/*  CMP (Disp8,s),y */
	GET_DISP8_S_IND_Y_RD();
	CMP_INST();

instd4_SYM		/*  PEI Dloc */
	GET_DLOC_ADDR()
	GET_MEMORY16(arg, arg, 1);
	CYCLES_PLUS_1;
	PUSH16_UNSAFE(arg);

instd5_SYM		/*  CMP Dloc,x */
	GET_DLOC_X_RD();
	CMP_INST();

instd6_SYM		/*  DEC Dloc,x */
	GET_DLOC_X_RD();
	DEC_INST(1);

instd7_SYM		/*  CMP [Dloc],Y */
	GET_DLOC_L_IND_Y_RD();
	CMP_INST();

instd8_SYM		/*  CLD */
	psr = psr & (~0x8);
	INC_KPC_1;

instd9_SYM		/*  CMP abs,y */
	GET_ABS_Y_RD();
	CMP_INST();

instda_SYM		/*  PHX */
	INC_KPC_1;
	if(psr & 0x10) {
		PUSH8(xreg);
	} else {
		PUSH16(xreg);
	}

instdb_SYM		/*  STP */
	CYCLES_FINISH
	FINISH(RET_STP, 0);

instdc_SYM		/*  JML (Abs) */
	GET_2BYTE_ARG;
	CYCLES_PLUS_1;
	GET_MEMORY24(arg, kpc, 1);

instdd_SYM		/*  CMP Abs,X */
	GET_ABS_X_RD();
	CMP_INST();

instde_SYM		/*  DEC Abs,X */
	GET_ABS_X_RD_WR();
	DEC_INST(0);

instdf_SYM		/*  CMP Long,X */
	GET_LONG_X_RD();
	CMP_INST();


inste0_SYM		/*  CPX #imm */
	C_CPX_IMM();


inste1_SYM		/*  SBC (Dloc,X) */
/*  called with arg = val to SBC in */
	GET_DLOC_X_IND_RD();
	SBC_INST();

inste2_SYM		/*  SEP #imm */
	GET_1BYTE_ARG;
	tmp2 = psr;
	CYCLES_PLUS_1;
	INC_KPC_2;
	psr = (psr & ~0x82) | ((neg & 1) << 7) | ((!zero) << 1);
	psr = psr | (arg & 0xff);
	zero = !(psr & 2);
	neg = (psr >> 7) & 1;
	UPDATE_PSR(psr, tmp2);

inste3_SYM		/*  SBC Disp8,S */
/*  called with arg = val to SBC in */
	GET_DISP8_S_RD();
	SBC_INST();

inste4_SYM		/*  CPX Dloc */
	C_CPX_DLOC();

inste5_SYM		/*  SBC Dloc */
/*  called with arg = val to SBC in */
	GET_DLOC_RD();
	SBC_INST();

inste6_SYM		/*  INC Dloc */
	GET_DLOC_RD();
	INC_INST(1);

inste7_SYM		/*  SBC [Dloc] */
	GET_DLOC_L_IND_RD();
	SBC_INST();

inste8_SYM		/*  INX */
	INC_KPC_1;
	SET_INDEX_REG(xreg + 1, xreg);

inste9_SYM		/*  SBC #imm */
	GET_IMM_MEM();
	SBC_INST();

instea_SYM		/*  NOP */
	INC_KPC_1;

insteb_SYM		/*  XBA */
	tmp1 = acc & 0xff;
	CYCLES_PLUS_1
	acc = (tmp1 << 8) + (acc >> 8);
	INC_KPC_1;
	SET_NEG_ZERO8(acc & 0xff);

instec_SYM		/*  CPX abs */
	C_CPX_ABS();

insted_SYM		/*  SBC abs */
	GET_ABS_RD();
	SBC_INST();

instee_SYM		/*  INC abs */
	GET_ABS_RD();
	INC_INST(0);

instef_SYM		/*  SBC long */
	GET_LONG_RD();
	SBC_INST();

instf0_SYM		/*  BEQ disp8 */
	BRANCH_DISP8(zero == 0);

instf1_SYM		/*  SBC (Dloc),y */
	GET_DLOC_IND_Y_RD();
	SBC_INST();

instf2_SYM		/*  SBC (Dloc) */
	GET_DLOC_IND_RD();
	SBC_INST();

instf3_SYM		/*  SBC (Disp8,s),y */
	GET_DISP8_S_IND_Y_RD();
	SBC_INST();

instf4_SYM		/*  PEA Abs */
	GET_2BYTE_ARG;
	CYCLES_PLUS_1;
	INC_KPC_3;
	PUSH16_UNSAFE(arg);

instf5_SYM		/*  SBC Dloc,x */
	GET_DLOC_X_RD();
	SBC_INST();

instf6_SYM		/*  INC Dloc,x */
	GET_DLOC_X_RD();
	INC_INST(1);

instf7_SYM		/*  SBC [Dloc],Y */
	GET_DLOC_L_IND_Y_RD();
	SBC_INST();

instf8_SYM		/*  SED */
	INC_KPC_1;
	psr |= 0x8;

instf9_SYM		/*  SBC abs,y */
	GET_ABS_Y_RD();
	SBC_INST();

instfa_SYM		/*  PLX */
	INC_KPC_1;
	CYCLES_PLUS_1;
	if(psr & 0x10) {
		PULL8(xreg);
		SET_NEG_ZERO8(xreg);
	} else {
		PULL16(xreg);
		SET_NEG_ZERO16(xreg);
	}

instfb_SYM		/*  XCE */
	tmp2 = psr;
	INC_KPC_1;
	psr = (tmp2 & 0xfe) | ((tmp2 & 1) << 8) | ((tmp2 >> 8) & 1);
	UPDATE_PSR(psr, tmp2);

instfc_SYM		/*  JSR (Abs,X) */
	GET_2BYTE_ARG;
	INC_KPC_2;
	tmp1 = kpc;
	arg = (kpc & 0xff0000) + ((arg + xreg) & 0xffff);
	GET_MEMORY16(arg, tmp2, 1);
	kpc = (kpc & 0xff0000) + tmp2;
	CYCLES_PLUS_2
	PUSH16_UNSAFE(tmp1);

instfd_SYM		/*  SBC Abs,X */
	GET_ABS_X_RD();
	SBC_INST();

instfe_SYM		/*  INC Abs,X */
	GET_ABS_X_RD_WR();
	INC_INST(0);

instff_SYM		/*  SBC Long,X */
	GET_LONG_X_RD();
	SBC_INST();
