// $KmKId: defs_instr.h,v 1.70 2023-11-05 16:22:26+00 kentd Exp $

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

#undef GET_DLOC_X_IND_RD

#ifdef ACC8
# define GET_DLOC_X_IND_RD()			\
	GET_1BYTE_ARG;				\
	GET_DLOC_X_IND_WR();			\
	GET_MEMORY8(arg, arg);
#else
# define GET_DLOC_X_IND_RD()			\
	GET_1BYTE_ARG;				\
	GET_DLOC_X_IND_WR();			\
	GET_MEMORY16(arg, arg, 0);
#endif

#undef GET_DISP8_S_RD

#ifdef ACC8
# define GET_DISP8_S_RD()			\
	GET_1BYTE_ARG;				\
	GET_DISP8_S_WR();			\
	GET_MEMORY8(arg, arg);
#else
# define GET_DISP8_S_RD()			\
	GET_1BYTE_ARG;				\
	GET_DISP8_S_WR();			\
	GET_MEMORY16(arg, arg, 0);
#endif

#undef GET_DLOC_RD

#ifdef ACC8
# define GET_DLOC_RD()				\
	GET_1BYTE_ARG;				\
	if(direct & 0xff) {			\
		CYCLES_PLUS_1;			\
	}					\
	INC_KPC_2;				\
	GET_MEMORY8((direct + arg) & 0xffff, arg);
#else
# define GET_DLOC_RD()				\
	GET_1BYTE_ARG;				\
	if(direct & 0xff) {			\
		CYCLES_PLUS_1;			\
	}					\
	INC_KPC_2;				\
	GET_MEMORY16((direct + arg) & 0xffff, arg, 1);
#endif

#undef GET_DLOC_RD_RMW
#undef GET_MEM_RMW

#define GET_MEM_RMW()						\
	if(!IS_ACC16) {						\
		if(psr & 0x100) {				\
			/* emulation re-writes the address */	\
			SET_MEMORY8(addr_latch, arg);		\
		} else {					\
			/* otherwise, just read addr again */	\
			GET_MEMORY8(addr_latch, dummy1);	\
		}						\
	} else {						\
		/* 16-bit re-reads addr+1 again */		\
		dummy1 = addr_latch + 1;			\
		GET_MEMORY8(dummy1, dummy1);			\
		addr_latch--;					\
	}

#define GET_DLOC_RD_RMW()			\
	GET_DLOC_RD();				\
	GET_MEM_RMW();


#undef GET_DLOC_L_IND_RD

#ifdef ACC8
# define GET_DLOC_L_IND_RD()			\
	GET_1BYTE_ARG;				\
	GET_DLOC_L_IND_WR();			\
	GET_MEMORY8(arg, arg);
#else
# define GET_DLOC_L_IND_RD()			\
	GET_1BYTE_ARG;				\
	GET_DLOC_L_IND_WR();			\
	GET_MEMORY16(arg, arg, 0);
#endif

#undef GET_IMM_MEM

#ifdef ACC8
# define GET_IMM_MEM()				\
	GET_1BYTE_ARG;				\
	INC_KPC_2;
#else
# define GET_IMM_MEM()				\
	GET_2BYTE_ARG;				\
	CYCLES_PLUS_1;				\
	INC_KPC_3;
#endif

#undef GET_ABS_RD

#ifdef ACC8
# define GET_ABS_RD()				\
	GET_2BYTE_ARG;				\
	CYCLES_PLUS_1;				\
	GET_MEMORY8((dbank << 16) + arg, arg);	\
	INC_KPC_3;
#else
# define GET_ABS_RD()				\
	GET_2BYTE_ARG;				\
	CYCLES_PLUS_1;				\
	GET_MEMORY16((dbank << 16) + arg, arg, 0); \
	INC_KPC_3;
#endif

#undef GET_ABS_RD_RMW

#define GET_ABS_RD_RMW()			\
	GET_ABS_RD();				\
	GET_MEM_RMW();

#undef GET_LONG_RD

#ifdef ACC8
# define GET_LONG_RD()				\
	GET_3BYTE_ARG;				\
	CYCLES_PLUS_2;				\
	GET_MEMORY8(arg, arg);			\
	INC_KPC_4;
#else
# define GET_LONG_RD()				\
	GET_3BYTE_ARG;				\
	CYCLES_PLUS_2;				\
	GET_MEMORY16(arg, arg, 0);		\
	INC_KPC_4;
#endif

#undef GET_DLOC_IND_Y_RD
#undef GET_DLOC_IND_Y_ADDR

#ifdef ACC8
# define GET_DLOC_IND_Y_RD()			\
	GET_DLOC_IND_Y_ADDR(0)						\
	GET_MEMORY8(arg, arg);
#else
# define GET_DLOC_IND_Y_RD()						\
	GET_DLOC_IND_Y_ADDR(0)						\
	GET_MEMORY16(arg, arg, 0);
#endif

#define GET_DLOC_IND_Y_ADDR(is_write)					\
	GET_1BYTE_ARG;							\
	if(direct & 0xff) {						\
		CYCLES_PLUS_1;						\
	}								\
	GET_MEMORY_DIRECT_PAGE16((direct + arg) & 0xffff, tmp1, 0);	\
	tmp1 += (dbank << 16);						\
	arg = (tmp1 + yreg) & 0xffffff;					\
	tmp2 = (tmp1 & 0xffff00) | (arg & 0xff);			\
	if((psr & 0x10) && ((arg != tmp2) | is_write)) {		\
		GET_MEMORY8(tmp2, tmp1);				\
	} else if(((psr & 0x10) == 0) | (arg != tmp2) | is_write) {	\
		CYCLES_PLUS_1;						\
	}								\
	INC_KPC_2;

#undef GET_DLOC_IND_RD

#ifdef ACC8
# define GET_DLOC_IND_RD()				\
	GET_1BYTE_ARG;					\
	INC_KPC_2;					\
	if(direct & 0xff) {				\
		CYCLES_PLUS_1;				\
	}						\
	GET_MEMORY_DIRECT_PAGE16((direct + arg) & 0xffff, arg, 0);	\
	GET_MEMORY8((dbank << 16) + arg, arg);
#else
# define GET_DLOC_IND_RD()				\
	GET_1BYTE_ARG;					\
	INC_KPC_2;					\
	if(direct & 0xff) {				\
		CYCLES_PLUS_1;				\
	}						\
	GET_MEMORY_DIRECT_PAGE16((direct + arg) & 0xffff, arg, 0);	\
	GET_MEMORY16((dbank << 16) + arg, arg, 0);
#endif

#undef GET_DLOC_X_RD

#ifdef ACC8
# define GET_DLOC_X_RD()						\
	GET_1BYTE_ARG;							\
	CYCLES_PLUS_1;							\
	if(direct & 0xff) {						\
		CYCLES_PLUS_1;						\
	}								\
	INC_KPC_2;							\
	arg = (arg + xreg + direct) & 0xffff;				\
	if(psr & 0x100) {						\
		if((direct & 0xff) == 0) {				\
			arg = (direct & 0xff00) | (arg & 0xff);		\
		}							\
	}								\
	GET_MEMORY8(arg, arg);
#else
# define GET_DLOC_X_RD()						\
	GET_1BYTE_ARG;							\
	CYCLES_PLUS_1;							\
	if(direct & 0xff) {						\
		CYCLES_PLUS_1;						\
	}								\
	INC_KPC_2;							\
	arg = (arg + xreg + direct) & 0xffff;				\
	if(!IS_ACC16 && (psr & 0x100)) {				\
		if((direct & 0xff) == 0) {				\
			arg = (direct & 0xff00) | (arg & 0xff);		\
		}							\
	}								\
	GET_MEMORY16(arg, arg, 1);
#endif

#undef GET_DLOC_X_RD_RMW

#define GET_DLOC_X_RD_RMW()						\
	GET_DLOC_X_RD();						\
	GET_MEM_RMW();


#undef GET_DISP8_S_IND_Y_RD

#ifdef ACC8
# define GET_DISP8_S_IND_Y_RD()				\
	GET_1BYTE_ARG;					\
	GET_DISP8_S_IND_Y_WR();				\
	GET_MEMORY8(arg, arg);
#else
# define GET_DISP8_S_IND_Y_RD()				\
	GET_1BYTE_ARG;					\
	GET_DISP8_S_IND_Y_WR();				\
	GET_MEMORY16(arg, arg, 0);
#endif

#undef GET_DLOC_L_IND_Y_RD

#ifdef ACC8
# define GET_DLOC_L_IND_Y_RD()			\
	GET_1BYTE_ARG;				\
	GET_DLOC_L_IND_Y_WR();			\
	GET_MEMORY8(arg, arg);
#else
# define GET_DLOC_L_IND_Y_RD()			\
	GET_1BYTE_ARG;				\
	GET_DLOC_L_IND_Y_WR();			\
	GET_MEMORY16(arg, arg, 0);
#endif

#undef GET_ABS_INDEX_ADDR

#define GET_ABS_INDEX_ADDR(index_reg, is_write)			\
	GET_2BYTE_ARG;						\
	CYCLES_PLUS_1;						\
	INC_KPC_3;						\
	tmp1 = (dbank << 16) + arg;				\
	arg = tmp1 + index_reg;					\
	tmp1 = (tmp1 & 0xffff00) + (arg & 0xff);		\
	if((psr & 0x10) && ((tmp1 != arg) | is_write)) {	\
		GET_MEMORY8(tmp1, tmp2);			\
	} else if(((psr & 0x10) == 0) | (tmp1 != arg) | is_write) {	\
		CYCLES_PLUS_1;					\
	}

#undef GET_ABS_Y_RD

#ifdef ACC8
# define GET_ABS_Y_RD()				\
	GET_ABS_INDEX_ADDR(yreg, 0);		\
	GET_MEMORY8(arg, arg);
#else
# define GET_ABS_Y_RD()				\
	GET_ABS_INDEX_ADDR(yreg, 0);		\
	GET_MEMORY16(arg, arg, 0);
#endif

#undef GET_ABS_X_RD
#undef GET_ABS_X_RD_RMW

#ifdef ACC8
# define GET_ABS_X_RD()					\
	GET_ABS_INDEX_ADDR(xreg, 0);			\
	GET_MEMORY8(arg, arg);
#define GET_ABS_X_RD_RMW()				\
	GET_ABS_INDEX_ADDR(xreg, 1);			\
	GET_MEMORY8(arg, arg);				\
	GET_MEM_RMW();
#else
# define GET_ABS_X_RD()					\
	GET_ABS_INDEX_ADDR(xreg, 0);			\
	GET_MEMORY16(arg, arg, 0);
#define GET_ABS_X_RD_RMW()				\
	GET_ABS_INDEX_ADDR(xreg, 1);			\
	GET_MEMORY16(arg, arg, 0);			\
	GET_MEM_RMW();
#endif

#undef GET_LONG_X_RD

#ifdef ACC8
# define GET_LONG_X_RD()			\
	GET_3BYTE_ARG;				\
	arg = (arg + xreg) & 0xffffff;		\
	INC_KPC_4;				\
	CYCLES_PLUS_2;				\
	GET_MEMORY8(arg, arg);
#else
# define GET_LONG_X_RD()			\
	GET_3BYTE_ARG;				\
	arg = (arg + xreg) & 0xffffff;		\
	INC_KPC_4;				\
	CYCLES_PLUS_2;				\
	GET_MEMORY16(arg, arg, 0);
#endif

#define SET_NEG_ZERO16(val)	\
	zero = val;		\
	neg7 = (val) >> 8;

#define SET_NEG_ZERO8(val)	\
	zero = val;		\
	neg7 = val;

#define SET_CARRY8(val)		\
	psr = (psr & ~1) + (((val) >> 8) & 1);

#define SET_CARRY16(val)	\
	psr = (psr & ~1) + (((val) >> 16) & 1);

#define SET_INDEX_REG(src, dest)		\
	if(psr & 0x10) {			\
		dest = (src) & 0xff;		\
		SET_NEG_ZERO8(dest);		\
	} else {				\
		dest = (src) & 0xffff;		\
		SET_NEG_ZERO16(dest);		\
	}

#define LD_INDEX_INST(index_reg, in_bank)	\
	if(psr & 0x10) {			\
		GET_MEMORY8(arg, arg);		\
	} else {				\
		GET_MEMORY16(arg, arg, in_bank);\
	}					\
	SET_INDEX_REG(arg, index_reg);

#define LDX_INST(in_bank)	LD_INDEX_INST(xreg, in_bank)
#define LDY_INST(in_bank)	LD_INDEX_INST(yreg, in_bank)

#undef ORA_INST

#ifdef ACC8
# define ORA_INST()				\
	tmp1 = (acc | arg) & 0xff;		\
	acc = (acc & 0xff00) + tmp1;		\
	SET_NEG_ZERO8(tmp1);
#else
# define ORA_INST()				\
	acc = (acc | arg);			\
	SET_NEG_ZERO16(acc);
#endif

#undef AND_INST

#ifdef ACC8
# define AND_INST()				\
	tmp1 = (acc & arg) & 0xff;		\
	acc = (acc & 0xff00) + tmp1;		\
	SET_NEG_ZERO8(tmp1);
#else
# define AND_INST()				\
	acc = (acc & arg);			\
	SET_NEG_ZERO16(acc);
#endif

#undef EOR_INST

#ifdef ACC8
# define EOR_INST()				\
	tmp1 = (acc ^ arg) & 0xff;		\
	acc = (acc & 0xff00) + tmp1;		\
	SET_NEG_ZERO8(tmp1);
#else
# define EOR_INST()				\
	acc = (acc ^ arg);			\
	SET_NEG_ZERO16(acc);
#endif

#undef LDA_INST

#ifdef ACC8
# define LDA_INST()				\
	acc = (acc & 0xff00) + (arg & 0xff);	\
	SET_NEG_ZERO8(arg & 0xff);
#else
# define LDA_INST()				\
	acc = (arg & 0xffff);			\
	SET_NEG_ZERO16(acc);
#endif

#undef ADC_INST

#ifdef ACC8
# define ADC_INST()						\
	tmp1 = do_adc_sbc8(acc & 0xff, arg & 0xff, psr, 0);	\
	acc = (acc & 0xff00) + (tmp1 & 0xff);			\
	psr = (tmp1 >> 16);					\
	zero = !(psr & 0x2);					\
	neg7 = psr;
#else
# define ADC_INST()						\
	tmp1 = do_adc_sbc16(acc, arg & 0xffff, psr, 0);		\
	acc = (tmp1 & 0xffff);					\
	psr = (tmp1 >> 16);					\
	zero = !(psr & 0x2);					\
	neg7 = psr;
#endif

#undef SBC_INST

#ifdef ACC8
# define SBC_INST()						\
	tmp1 = do_adc_sbc8(acc & 0xff, arg & 0xff, psr, 1);	\
	acc = (acc & 0xff00) + (tmp1 & 0xff);			\
	psr = (tmp1 >> 16);					\
	zero = !(psr & 0x2);					\
	neg7 = psr;
#else
# define SBC_INST()						\
	tmp1 = do_adc_sbc16(acc, arg & 0xffff, psr, 1);		\
	acc = (tmp1 & 0xffff);					\
	psr = (tmp1 >> 16);					\
	zero = !(psr & 0x2);					\
	neg7 = psr;
#endif


#undef CMP_INST

#ifdef ACC8
# define CMP_INST()					\
	arg = (acc & 0xff) + (0x100 - arg);		\
	SET_CARRY8(arg);				\
	arg = arg & 0xff;				\
	SET_NEG_ZERO8(arg & 0xff);
#else
# define CMP_INST()					\
	arg = (acc & 0xffff) + (0x10000 - arg);		\
	SET_CARRY16(arg);				\
	arg = arg & 0xffff;				\
	SET_NEG_ZERO16(arg & 0xffff);
#endif

#undef BIT_INST

#ifdef ACC8
# define BIT_INST()				\
	neg7 = arg;				\
	zero = (acc & arg & 0xff);		\
	psr = (psr & (~0x40)) | (arg & 0x40);
#else
# define BIT_INST()				\
	neg7 = arg >> 8;			\
	zero = (acc & arg & 0xffff);		\
	psr = (psr & (~0x40)) | ((arg >> 8) & 0x40);
#endif

#undef STA_INST

#ifdef ACC8
# define STA_INST(in_bank)			\
	SET_MEMORY8(arg, acc);
#else
# define STA_INST(in_bank)			\
	SET_MEMORY16(arg, acc, in_bank);
#endif

#undef TSB_INST

#ifdef ACC8
# define TSB_INST(in_bank)			\
	arg = arg & 0xff;			\
	tmp1 = arg | acc;			\
	zero = arg & acc;			\
	SET_MEMORY8(addr_latch, tmp1);
#else
# define TSB_INST(in_bank)			\
	tmp1 = arg | acc;			\
	zero = arg & acc;			\
	SET_MEMORY16(addr_latch, tmp1, in_bank);
#endif

#undef ASL_INST

#ifdef ACC8
# define ASL_INST(in_bank)			\
	psr = (psr & 0x1fe) + ((arg >> 7) & 1);	\
	tmp1 = (arg << 1) & 0xff;		\
	SET_NEG_ZERO8(tmp1);			\
	SET_MEMORY8(addr_latch, tmp1);
#else
# define ASL_INST(in_bank)			\
	psr = (psr & 0x1fe) + ((arg >> 15) & 1);\
	tmp1 = (arg << 1) & 0xffff;		\
	SET_NEG_ZERO16(tmp1);			\
	SET_MEMORY16(addr_latch, tmp1, in_bank);
#endif

#undef ROL_INST

#ifdef ACC8
# define ROL_INST(in_bank)			\
	arg = arg & 0xff;			\
	arg = (arg << 1) | (psr & 1);		\
	SET_MEMORY8(addr_latch, arg);		\
	SET_NEG_ZERO8(arg & 0xff);		\
	SET_CARRY8(arg);
#else
# define ROL_INST(in_bank)			\
	arg = (arg << 1) | (psr & 1);		\
	SET_MEMORY16(addr_latch, arg, in_bank);	\
	SET_NEG_ZERO16(arg & 0xffff);		\
	SET_CARRY16(arg);
#endif

#undef LSR_INST

#ifdef ACC8
# define LSR_INST(in_bank)				\
	SET_CARRY8(arg << 8);				\
	arg = (arg >> 1) & 0x7f;			\
	SET_NEG_ZERO8(arg);				\
	SET_MEMORY8(addr_latch, arg);
#else
# define LSR_INST(in_bank)				\
	SET_CARRY16(arg << 16);				\
	arg = (arg >> 1) & 0x7fff;			\
	SET_NEG_ZERO16(arg);				\
	SET_MEMORY16(addr_latch, arg, in_bank);
#endif

#undef ROR_INST

#ifdef ACC8
# define ROR_INST(in_bank)				\
	tmp1 = psr & 1;					\
	SET_CARRY8(arg << 8);				\
	arg = ((arg >> 1) & 0x7f) | (tmp1 << 7);	\
	SET_MEMORY8(addr_latch, arg);			\
	SET_NEG_ZERO8(arg);
#else
# define ROR_INST(in_bank)				\
	tmp1 = psr & 1;					\
	SET_CARRY16(arg << 16);				\
	arg = ((arg >> 1) & 0x7fff) | (tmp1 << 15);	\
	SET_MEMORY16(addr_latch, arg, in_bank);		\
	SET_NEG_ZERO16(arg);
#endif

#undef TRB_INST

#ifdef ACC8
# define TRB_INST(in_bank)			\
	arg = arg & 0xff;			\
	tmp1 = arg & ~acc;			\
	zero = arg & acc;			\
	SET_MEMORY8(addr_latch, tmp1);
#else
# define TRB_INST(in_bank)			\
	tmp1 = arg & ~acc;			\
	zero = arg & acc;			\
	SET_MEMORY16(addr_latch, tmp1, in_bank);
#endif

#undef DEC_INST

#ifdef ACC8
# define DEC_INST(in_bank)			\
	arg = (arg - 1) & 0xff;			\
	SET_MEMORY8(addr_latch, arg);		\
	SET_NEG_ZERO8(arg);
#else
# define DEC_INST(in_bank)			\
	arg = (arg - 1) & 0xffff;		\
	SET_MEMORY16(addr_latch, arg, in_bank);	\
	SET_NEG_ZERO16(arg);
#endif

#undef INC_INST

#ifdef ACC8
# define INC_INST(in_bank)			\
	arg = (arg + 1) & 0xff;			\
	SET_MEMORY8(addr_latch, arg);		\
	SET_NEG_ZERO8(arg);
#else
# define INC_INST(in_bank)			\
	arg = (arg + 1) & 0xffff;		\
	SET_MEMORY16(addr_latch, arg, in_bank);	\
	SET_NEG_ZERO16(arg);
#endif

#undef STZ_INST

#ifdef ACC8
# define STZ_INST(in_bank)				\
	SET_MEMORY8(arg, 0);
#else
# define STZ_INST(in_bank)				\
	SET_MEMORY16(arg, 0, in_bank);
#endif

#undef BRANCH_DISP8

#define BRANCH_DISP8(cond)					\
	GET_1BYTE_ARG;						\
	tmp2 = kpc & 0xff0000;					\
	kpc += 2;						\
	tmp1 = kpc;						\
	if(cond) {						\
		kpc = kpc + arg - ((arg & 0x80) << 1);		\
		CYCLES_PLUS_1;					\
		if((tmp1 ^ kpc) & psr & 0x100) {		\
			CYCLES_PLUS_1;				\
		}						\
	}							\
	kpc = tmp2 + (kpc & 0xffff);

#undef STY_INST
#undef STX_INST

#define STY_INST(in_bank)			\
	if(psr & 0x10) {			\
		SET_MEMORY8(arg, yreg);		\
	} else {				\
		SET_MEMORY16(arg, yreg, in_bank);\
	}
#define STX_INST(in_bank)			\
	if(psr & 0x10) {			\
		SET_MEMORY8(arg, xreg);		\
	} else {				\
		SET_MEMORY16(arg, xreg, in_bank);\
	}

#define C_LDX_ABS_Y()			\
	GET_ABS_INDEX_ADDR(yreg, 0);	\
	LDX_INST(0);

#define C_LDY_ABS_X()			\
	GET_ABS_INDEX_ADDR(xreg, 0);	\
	LDY_INST(0);

#define C_LDX_ABS()		\
	GET_ABS_ADDR();		\
	LDX_INST(0);

#define C_LDY_ABS()		\
	GET_ABS_ADDR();		\
	LDY_INST(0);

#define C_LDX_DLOC()		\
	GET_DLOC_ADDR();	\
	LDX_INST(1);

#define C_LDY_DLOC()		\
	GET_DLOC_ADDR();	\
	LDY_INST(1);

#define C_LDY_DLOC_X()		\
	GET_DLOC_X_ADDR();	\
	LDY_INST(1);

#define C_LDX_DLOC_Y()		\
	GET_DLOC_Y_ADDR();	\
	LDX_INST(1);

#define CP_INDEX_VAL(index_reg)	\
	arg = 0x100 - arg + index_reg;	\
	if((psr & 0x10) == 0) {		\
		arg += 0xff00;		\
		SET_NEG_ZERO16(arg & 0xffff);	\
		SET_CARRY16(arg);	\
	} else {			\
		SET_NEG_ZERO8(arg & 0xff);\
		SET_CARRY8(arg);	\
	}

#define CP_INDEX_LOAD(index_reg, in_bank)	\
	if((psr & 0x10) != 0) {			\
		GET_MEMORY8(arg, arg);		\
	} else {				\
		GET_MEMORY16(arg, arg, in_bank);\
	}					\
	CP_INDEX_VAL(index_reg)

#define CPX_INST(in_bank)		\
	CP_INDEX_LOAD(xreg, in_bank);

#define CPY_INST(in_bank)		\
	CP_INDEX_LOAD(yreg, in_bank);

#define C_CPX_IMM()		\
	INC_KPC_2;		\
	if((psr & 0x10) == 0) {	\
		GET_2BYTE_ARG;	\
		CYCLES_PLUS_1;	\
		INC_KPC_1;	\
	} else {		\
		GET_1BYTE_ARG;	\
	}			\
	CP_INDEX_VAL(xreg);

#define C_CPY_IMM()		\
	INC_KPC_2;		\
	if((psr & 0x10) == 0) {	\
		GET_2BYTE_ARG;	\
		CYCLES_PLUS_1;	\
		INC_KPC_1;	\
	} else {		\
		GET_1BYTE_ARG;	\
	}			\
	CP_INDEX_VAL(yreg);

#define C_CPX_DLOC()		\
	GET_DLOC_ADDR();	\
	CPX_INST(1);

#define C_CPY_DLOC()		\
	GET_DLOC_ADDR();	\
	CPY_INST(1);

#define C_CPX_ABS()		\
	GET_ABS_ADDR();		\
	CPX_INST(0);

#define C_CPY_ABS()		\
	GET_ABS_ADDR();		\
	CPY_INST(0);

