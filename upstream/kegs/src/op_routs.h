// $KmKId: op_routs.h,v 1.47 2023-11-05 16:21:51+00 kentd Exp $

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


#define GET_DLOC_X_IND_WR()			\
	CYCLES_PLUS_1;				\
	INC_KPC_2;				\
	if(direct & 0xff) {			\
		CYCLES_PLUS_1;			\
	}					\
	arg = arg + xreg + direct;		\
	GET_MEMORY_DIRECT_PAGE16(arg & 0xffff, arg, 1);	\
	arg = (dbank << 16) + arg;


#define GET_DLOC_X_IND_ADDR()		\
	GET_1BYTE_ARG;			\
	GET_DLOC_X_IND_WR()

#define GET_DISP8_S_WR()		\
	CYCLES_PLUS_1;			\
	arg = (arg + stack) & 0xffff;	\
	INC_KPC_2;


#define GET_DISP8_S_ADDR()		\
	GET_1BYTE_ARG;			\
	GET_DISP8_S_WR()

#define GET_DLOC_WR()			\
	arg = (arg + direct) & 0xffff;	\
	if(direct & 0xff) {		\
		CYCLES_PLUS_1;		\
	}				\
	INC_KPC_2;

#define GET_DLOC_ADDR()		\
	GET_1BYTE_ARG;			\
	GET_DLOC_WR()

#define GET_DLOC_L_IND_WR()		\
	arg = (arg + direct) & 0xffff;	\
	if(direct & 0xff) {		\
		CYCLES_PLUS_1;		\
	}				\
	INC_KPC_2;			\
	GET_MEMORY24(arg, arg, 1);

#define GET_DLOC_L_IND_ADDR()		\
	GET_1BYTE_ARG;			\
	GET_DLOC_L_IND_WR()


#define GET_DLOC_IND_WR()		\
	INC_KPC_2;			\
	if(direct & 0xff) {		\
		CYCLES_PLUS_1;		\
	}				\
	GET_MEMORY_DIRECT_PAGE16((direct + arg) & 0xffff, arg, 0);	\
	arg = (dbank << 16) + arg;


#define GET_DLOC_IND_ADDR()		\
	GET_1BYTE_ARG;			\
	GET_DLOC_IND_WR();

#define GET_DLOC_INDEX_WR(index_reg)	\
	CYCLES_PLUS_1;			\
	arg = (arg & 0xff) + index_reg;	\
	INC_KPC_2;			\
	if(direct & 0xff) {		\
		CYCLES_PLUS_1;		\
	}				\
	if((psr & 0x100) && ((direct & 0xff) == 0)) {	\
		arg = (arg & 0xff);	\
	}				\
	arg = (arg + direct) & 0xffff;

#define GET_DLOC_X_WR()	\
	GET_DLOC_INDEX_WR(xreg)
#define GET_DLOC_Y_WR()	\
	GET_DLOC_INDEX_WR(yreg)

#define GET_DLOC_X_ADDR()	\
	GET_1BYTE_ARG;		\
	GET_DLOC_INDEX_WR(xreg)

#define GET_DLOC_Y_ADDR()	\
	GET_1BYTE_ARG;		\
	GET_DLOC_INDEX_WR(yreg)

#define GET_DISP8_S_IND_Y_WR()		\
	arg = (stack + arg) & 0xffff;	\
	GET_MEMORY16(arg,arg,1);	\
	CYCLES_PLUS_2;			\
	arg += (dbank << 16);		\
	INC_KPC_2;			\
	arg = (arg + yreg) & 0xffffff;

#define GET_DISP8_S_IND_Y_ADDR()	\
	GET_1BYTE_ARG;			\
	GET_DISP8_S_IND_Y_WR()

#define GET_DLOC_L_IND_Y_WR()		\
	arg = (direct + arg) & 0xffff;	\
	if(direct & 0xff) {		\
		CYCLES_PLUS_1;		\
	}				\
	GET_MEMORY24(arg,arg,1);	\
	INC_KPC_2;			\
	arg = (arg + yreg) & 0xffffff;

#define GET_DLOC_L_IND_Y_ADDR()	\
	GET_1BYTE_ARG;			\
	GET_DLOC_L_IND_Y_WR()


#define GET_ABS_ADDR()			\
	GET_2BYTE_ARG;			\
	CYCLES_PLUS_1;			\
	arg = arg + (dbank << 16);	\
	INC_KPC_3;

#define GET_LONG_ADDR()		\
	GET_3BYTE_ARG;			\
	CYCLES_PLUS_2;			\
	INC_KPC_4;

#define GET_LONG_X_ADDR_FOR_WR()		\
	GET_3BYTE_ARG;			\
	INC_KPC_4;			\
	arg = (arg + xreg) & 0xffffff;	\
	CYCLES_PLUS_2;

