/*
  GSPLUS - Advanced Apple IIGS Emulator Environment
  Based on the KEGS emulator written by Kent Dickey
  See COPYRIGHT.txt for Copyright information
	See LICENSE.txt for license (GPL v2)
*/

enum {
	ABS = 1,
	ABSX,
	ABSY,
	ABSIND,
	ABSXIND,
	IMPLY,
	ACCUM,
	IMMED,
	JUST8,
	DLOC,
	DLOCX,
	DLOCY,
	ABSLONG,
	ABSLONGX,
	DLOCIND,
	DLOCINDY,
	DLOCXIND,
	DLOCBRAK,
	DLOCBRAKY,
	DISP8,
	DISP8S,
	DISP8SINDY,
	DISP16,
	MVPMVN,
	REPVAL,
	SEPVAL
};


extern const char * const disasm_opcodes[256];


extern const unsigned disasm_types[256];