/*
  GSPLUS - Advanced Apple IIGS Emulator Environment
  Based on the KEGS emulator written by Kent Dickey
  See COPYRIGHT.txt for Copyright information
	See COPYING.txt for license (GPL v2)
*/

/* END_HDR */

/* engine_c.c */
void check_breakpoints(word32 addr);
word32 get_memory8_io_stub(word32 addr, byte *stat, double *fcycs_ptr, double fplus_x_m1);
word32 get_memory16_pieces_stub(word32 addr, byte *stat, double *fcycs_ptr, Fplus *fplus_ptr, int in_bank);
word32 get_memory24_pieces_stub(word32 addr, byte *stat, double *fcycs_ptr, Fplus *fplus_ptr, int in_bank);
void set_memory8_io_stub(word32 addr, word32 val, byte *stat, double *fcycs_ptr, double fplus_x_m1);
void set_memory16_pieces_stub(word32 addr, word32 val, double *fcycs_ptr, double fplus_1, double fplus_x_m1, int in_bank);
void set_memory24_pieces_stub(word32 addr, word32 val, double *fcycs_ptr, Fplus *fplus_ptr, int in_bank);
word32 get_memory_c(word32 addr, int cycs);
word32 get_memory16_c(word32 addr, int cycs);
word32 get_memory24_c(word32 addr, int cycs);
word32 get_memory32_c(word32 addr, int cycs);
void set_memory_c(word32 addr, word32 val, int cycs);
void set_memory16_c(word32 addr, word32 val, int cycs);
void set_memory24_c(word32 addr, word32 val, int cycs);
void set_memory32_c(word32 addr, word32 val, int cycs);
word32 do_adc_sbc8(word32 in1, word32 in2, word32 psr, int sub);
word32 do_adc_sbc16(word32 in1, word32 in2, word32 psr, int sub);
void fixed_memory_ptrs_init(void);
word32 get_itimer(void);
void set_halt_act(int val);
void clr_halt_act(void);
word32 get_remaining_operands(word32 addr, word32 opcode, word32 psr, Fplus *fplus_ptr);
int enter_engine(Engine_reg *engine_ptr);
