/*
  GSPLUS - Advanced Apple IIGS Emulator Environment
  Based on the KEGS emulator written by Kent Dickey
  See COPYRIGHT.txt for Copyright information
	See LICENSE.txt for license (GPL v2)
*/

enum {
	IMPLIED,
	DP, DP_X, DP_Y,
	ABS, ABS_X, ABS_Y,
	ABS_LONG, ABS_LONG_X, ABS_LONG_Y,
	INDIR, INDIR_X, INDIR_Y,
	INDIR_LONG, INDIR_LONG_X, INDIR_LONG_Y,
	SR, SR_Y,
	IMMEDIATE,
	RELATIVE,
	INTERRUPT,
	BLOCK,
};


extern const char * const disasm_opcodes[256];


extern const unsigned disasm_types[256];