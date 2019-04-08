/*
  GSPLUS - Advanced Apple IIGS Emulator Environment
  Based on the KEGS emulator written by Kent Dickey
  See COPYRIGHT.txt for Copyright information
	See LICENSE.txt for license (GPL v2)
*/

#define SHIFT_PER_CHANGE	3
#define CHANGE_SHIFT		(5 + SHIFT_PER_CHANGE)

#define SLOW_MEM_CH_SIZE	(0x10000 >> CHANGE_SHIFT)

#define MAXNUM_HEX_PER_LINE	32

/* Different Joystick defines */
#define JOYSTICK_MOUSE		1
#define JOYSTICK_LINUX		2
#define JOYSTICK_KEYPAD		3
#define JOYSTICK_WIN32_1	4
#define JOYSTICK_WIN32_2	5


#define HALT_EVENT	0x10
#define	HALT_WANTTOQUIT	0x20	// OG : added WANTTOQUIT event
#define	HALT_WANTTOBRK	0x40	// OG : added WANTTOBRK event

#define MAX_BREAK_POINTS	0x20

#define MAX_BP_INDEX		0x100
#define MAX_BP_PER_INDEX	3	/* 4 word32s total = 16 bytes */
#define SIZE_BREAKPT_ENTRY_BITS	4	/* 16 bytes = 4 bits */

/* Warning--next defines used by asm! */
#define PAGE_INFO_PAD_SIZE	0x800
#define PAGE_INFO_WR_OFFSET	0x10000+PAGE_INFO_PAD_SIZE

#define BANK_IO_BIT		31
#define BANK_SHADOW_BIT		30
#define BANK_SHADOW2_BIT	29
#define BANK_IO2_BIT		28
#define BANK_BREAK_BIT		27
#define BANK_BREAK		(1 << (31 - BANK_BREAK_BIT))
#define BANK_IO2_TMP		(1 << (31 - BANK_IO2_BIT))
#define BANK_IO_TMP		(1 << (31 - BANK_IO_BIT))
#define BANK_SHADOW		(1 << (31 - BANK_SHADOW_BIT))
#define BANK_SHADOW2		(1 << (31 - BANK_SHADOW2_BIT))
#define SET_BANK_IO	\
		(&g_dummy_memory1_ptr[BANK_IO_TMP | BANK_IO2_TMP])

#define BANK_BAD_MEM		(&g_dummy_memory1_ptr[0xff])

/*
 * this is only relevant for the PA RISC asm.
 *
 */
#define ENGINE_FPLUS_PTR	0x00
#define ENGINE_FCYCLES		0x08
#define ENGINE_REG_KPC		0x10
#define ENGINE_REG_ACC		0x14
#define ENGINE_REG_XREG		0x18
#define ENGINE_REG_YREG		0x1c
#define ENGINE_REG_STACK	0x20
#define ENGINE_REG_DBANK	0x24
#define ENGINE_REG_DIRECT	0x28
#define ENGINE_REG_PSR		0x2c
#define ENGINE_FLAGS		0x30

#define LOG_PC_DCYCS		0x00
#define LOG_PC_DBANK_KPC	0x08
#define LOG_PC_INSTR		0x0c
#define LOG_PC_PSR_ACC		0x10
#define LOG_PC_XREG_YREG	0x14
#define LOG_PC_STACK_DIRECT	0x18
#define LOG_PC_PAD		0x1c

#define LOG_PC_SIZE		0x20


#define FPLUS_PLUS_1		0x00
#define FPLUS_PLUS_2		0x08
#define FPLUS_PLUS_3		0x10
#define FPLUS_PLUS_X_M1		0x18

#define RET_BRK		0x1
#define RET_COP		0x2
#define RET_WDM		0x3
#define RET_MVP		0x4
#define RET_MVN		0x5
#define RET_WAI		0x6
#define RET_STP		0x7
#define RET_HALT	0x8
#define RET_C700	0xa
#define RET_C70A	0xb
#define RET_C70D	0xc
#define RET_IRQ		0xd
#define RET_BP		0xe
#define RET_MP		0xf

#define FLAG_IGNORE_MP	0x01
#define FLAG_IGNORE_BP	0x02
#define FLAG_STEP		0x04
#define FLAG_WANT_BRK	0x08
#define FLAG_WANT_COP	0x10
#define FLAG_WANT_RET	0x20
#define FLAG_WANT_JSL	0x40
#define FLAG_IGNORE_BRK	0x80 /* and cop */

#define MODE_BORDER		0
#define MODE_TEXT		1
#define MODE_GR			2
#define MODE_HGR		3
#define MODE_SUPER_HIRES	4

#define BIT_ALL_STAT_TEXT		0
#define BIT_ALL_STAT_VID80		1
#define BIT_ALL_STAT_ST80		2
#define BIT_ALL_STAT_COLOR_C021		3
#define BIT_ALL_STAT_MIX_T_GR		4
#define BIT_ALL_STAT_DIS_COLOR_DHIRES	5	/* special val, c029 */
#define BIT_ALL_STAT_PAGE2		6	/* special val, statereg */
#define BIT_ALL_STAT_SUPER_HIRES	7	/* special, c029 */
#define BIT_ALL_STAT_HIRES		8
#define BIT_ALL_STAT_ANNUNC3		9
#define BIT_ALL_STAT_BG_COLOR		10	/* 4 bits */
#define BIT_ALL_STAT_TEXT_COLOR		14	/* 4 bits */
						/* Text must be just above */
						/* bg to match c022 reg */
#define BIT_ALL_STAT_ALTCHARSET		18
#define BIT_ALL_STAT_FLASH_STATE	19
#define BIT_ALL_STAT_A2VID_PALETTE	20	/* 4 bits */

#define ALL_STAT_SUPER_HIRES		(1 << (BIT_ALL_STAT_SUPER_HIRES))
#define ALL_STAT_TEXT			(1 << (BIT_ALL_STAT_TEXT))
#define ALL_STAT_VID80			(1 << (BIT_ALL_STAT_VID80))
#define ALL_STAT_PAGE2			(1 << (BIT_ALL_STAT_PAGE2))
#define ALL_STAT_ST80			(1 << (BIT_ALL_STAT_ST80))
#define ALL_STAT_COLOR_C021		(1 << (BIT_ALL_STAT_COLOR_C021))
#define ALL_STAT_DIS_COLOR_DHIRES	(1 << (BIT_ALL_STAT_DIS_COLOR_DHIRES))
#define ALL_STAT_MIX_T_GR		(1 << (BIT_ALL_STAT_MIX_T_GR))
#define ALL_STAT_HIRES			(1 << (BIT_ALL_STAT_HIRES))
#define ALL_STAT_ANNUNC3		(1 << (BIT_ALL_STAT_ANNUNC3))
#define ALL_STAT_TEXT_COLOR		(0xf << (BIT_ALL_STAT_TEXT_COLOR))
#define ALL_STAT_BG_COLOR		(0xf << (BIT_ALL_STAT_BG_COLOR))
#define ALL_STAT_ALTCHARSET		(1 << (BIT_ALL_STAT_ALTCHARSET))
#define ALL_STAT_FLASH_STATE		(1 << (BIT_ALL_STAT_FLASH_STATE))
#define ALL_STAT_A2VID_PALETTE		(0xf << (BIT_ALL_STAT_A2VID_PALETTE))

#define BORDER_WIDTH		32

// OG Eff_border_widht == border side in A2 mode
#define EFF_BORDER_WIDTH	(BORDER_WIDTH + (640-560)/2)

/* BASE_MARGIN_BOTTOM+MARGIN_TOP must equal 62.  There are 262 scan lines */
/*  at 60Hz (15.7KHz line rate) and so we just make 62 border lines */
#define BASE_MARGIN_TOP		32
#define BASE_MARGIN_BOTTOM	30
#define BASE_MARGIN_LEFT	BORDER_WIDTH
#define BASE_MARGIN_RIGHT	BORDER_WIDTH

#define A2_WINDOW_WIDTH		640
#define A2_WINDOW_HEIGHT	400

#define X_A2_WINDOW_WIDTH	(A2_WINDOW_WIDTH + BASE_MARGIN_LEFT + BASE_MARGIN_RIGHT)
#define X_A2_WINDOW_HEIGHT	(A2_WINDOW_HEIGHT + BASE_MARGIN_TOP +	BASE_MARGIN_BOTTOM)
#define BASE_WINDOW_WIDTH	(X_A2_WINDOW_WIDTH)
#define BASE_WINDOW_HEIGHT	(X_A2_WINDOW_HEIGHT)

#define MAX_STATUS_LINES	7
#define STATUS_LINE_LENGTH	88

#define A2_BORDER_COLOR_NUM	0xfe
