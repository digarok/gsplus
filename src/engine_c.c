/*
   GSPLUS - Advanced Apple IIGS Emulator Environment
   Based on the KEGS emulator written by Kent Dickey
   See COPYRIGHT.txt for Copyright information
   See LICENSE.txt for license (GPL v2)
 */

#include "defc.h"
#include "protos_engine_c.h"

#ifdef _WIN32
#include <Windows.h>
#endif

#if 0
# define LOG_PC
/* define FCYCS_PTR_FCYCLES_ROUND_SLOW to get accurate 1MHz write to slow mem*/
/*  this might help joystick emulation in some Apple //gs games like */
/*  Madness */
# define FCYCS_PTR_FCYCLES_ROUND_SLOW   FCYCLES_ROUND; *fcycs_ptr = fcycles;
#endif

#ifndef FCYCS_PTR_FCYCLES_ROUND_SLOW
# define FCYCS_PTR_FCYCLES_ROUND_SLOW
#endif

extern int halt_sim;
extern int g_code_red;
extern int g_ignore_halts;
extern int g_user_halt_bad;
extern double g_fcycles_stop;
extern double g_last_vbl_dcycs;
extern double g_cur_dcycs;
extern int g_wait_pending;
extern int g_irq_pending;
extern int g_testing;
extern int g_num_brk;
extern int g_num_cop;
extern byte *g_slow_memory_ptr;
extern byte *g_memory_ptr;
extern byte *g_rom_fc_ff_ptr;
extern byte *g_rom_cards_ptr;
extern byte *g_dummy_memory1_ptr;
extern int g_c068_statereg;
unsigned char ioslotsel = 0;
unsigned char iostrobe = 0;

// OG Need allocated memory
extern byte *g_slow_memory_ptr_allocated;
extern byte *g_memory_ptr_allocated;
extern byte *g_rom_fc_ff_ptr_allocated;
extern byte *g_rom_cards_ptr_allocated;
extern byte *g_dummy_memory1_ptr_allocated;

extern Pc_log *g_log_pc_ptr;
extern Pc_log *g_log_pc_start_ptr;
extern Pc_log *g_log_pc_end_ptr;

extern Data_log *g_log_data_ptr;
extern Data_log *g_log_data_start_ptr;
extern Data_log *g_log_data_end_ptr;

int size_tab[] = {
#include "size_c.h"
};

int bogus[] = {
  0,
#include "op_routs.h"
};

#define FINISH(arg1, arg2)      g_ret1 = arg1; g_ret2 = arg2; goto finish;
#define INC_KPC_1       kpc = (kpc & 0xff0000) + ((kpc + 1) & 0xffff);
#define INC_KPC_2       kpc = (kpc & 0xff0000) + ((kpc + 2) & 0xffff);
#define INC_KPC_3       kpc = (kpc & 0xff0000) + ((kpc + 3) & 0xffff);
#define INC_KPC_4       kpc = (kpc & 0xff0000) + ((kpc + 4) & 0xffff);

#define CYCLES_PLUS_1   fcycles += fplus_1;
#define CYCLES_PLUS_2   fcycles += fplus_2;
#define CYCLES_PLUS_3   fcycles += fplus_3;
#define CYCLES_PLUS_4   fcycles += (fplus_1 + fplus_3);
#define CYCLES_PLUS_5   fcycles += (fplus_2 + fplus_3);
#define CYCLES_MINUS_1  fcycles -= fplus_1;
#define CYCLES_MINUS_2  fcycles -= fplus_2;

#define CYCLES_FINISH   fcycles = g_fcycles_stop + fplus_1;

#define FCYCLES_ROUND   fcycles = (int)(fcycles + fplus_x_m1);

#ifdef LOG_PC
# define LOG_PC_MACRO()                                                 \
  tmp_pc_ptr = g_log_pc_ptr++;                            \
  tmp_pc_ptr->dbank_kpc = (dbank << 24) + kpc;            \
  tmp_pc_ptr->instr = (opcode << 24) + arg_ptr[1] +       \
                      (arg_ptr[2] << 8) + (arg_ptr[3] << 16);         \
  tmp_pc_ptr->psr_acc = ((psr & ~(0x82)) << 16) + acc +   \
                        (neg << 23) + ((!zero) << 17);                  \
  tmp_pc_ptr->xreg_yreg = (xreg << 16) + yreg;            \
  tmp_pc_ptr->stack_direct = (stack << 16) + direct;      \
  tmp_pc_ptr->dcycs = fcycles + g_last_vbl_dcycs - fplus_2; \
  if(g_log_pc_ptr >= g_log_pc_end_ptr) {                  \
    /*halt2_printf("log_pc oflow %f\n", tmp_pc_ptr->dcycs);*/ \
    g_log_pc_ptr = g_log_pc_start_ptr;              \
  }

# define LOG_DATA_MACRO(in_addr, in_val, in_size)                       \
  g_log_data_ptr->dcycs = fcycles + g_last_vbl_dcycs;     \
  g_log_data_ptr->addr = in_addr;                         \
  g_log_data_ptr->val = in_val;                           \
  g_log_data_ptr->size = in_size;                         \
  g_log_data_ptr++;                                       \
  if(g_log_data_ptr >= g_log_data_end_ptr) {              \
    g_log_data_ptr = g_log_data_start_ptr;          \
  }

#else
# define LOG_PC_MACRO()
# define LOG_DATA_MACRO(addr, val, size)
/* Just do nothing */
#endif


#define GET_1BYTE_ARG   arg = arg_ptr[1];
#define GET_2BYTE_ARG   arg = arg_ptr[1] + (arg_ptr[2] << 8);
#define GET_3BYTE_ARG   arg = arg_ptr[1] + (arg_ptr[2] << 8) + (arg_ptr[3]<<16);

/* HACK HACK HACK */
#define UPDATE_PSR(dummy, old_psr)                              \
  if(psr & 0x100) {                                       \
    psr |= 0x30;                                    \
    stack = 0x100 + (stack & 0xff);                 \
  }                                                       \
  if((old_psr ^ psr) & 0x10) {                            \
    if(psr & 0x10) {                                \
      xreg = xreg & 0xff;                     \
      yreg = yreg & 0xff;                     \
    }                                               \
  }                                                       \
  if(((psr & 0x4) == 0) && g_irq_pending) {               \
    FINISH(RET_IRQ, 0);                             \
  }                                                       \
  if((old_psr ^ psr) & 0x20) {                            \
    goto recalc_accsize;                            \
  }

extern Page_info page_info_rd_wr[];
extern word32 slow_mem_changed[];

#define GET_MEMORY8(addr,dest)                                  \
  addr_latch = (addr); \
  CYCLES_PLUS_1;  \
  stat = GET_PAGE_INFO_RD(((addr) >> 8) & 0xffff);        \
  wstat = PTR2WORD(stat) & 0xff;                          \
  ptr = stat - wstat + ((addr) & 0xff);                   \
  if(wstat & BANK_IO_TMP || iostrobe == 1) {                        \
    fcycles_tmp1 = fcycles;                         \
    dest = get_memory8_io_stub((addr), stat,        \
                               &fcycles_tmp1, fplus_x_m1);     \
    fcycles = fcycles_tmp1;                         \
  } else {                                                \
    dest = *ptr;                                    \
  }

#define GET_MEMORY(addr,dest)   GET_MEMORY8(addr, dest)

#define GET_MEMORY16(addr, dest, in_bank)                       \
  save_addr = addr;                                       \
  stat = GET_PAGE_INFO_RD(((addr) >> 8) & 0xffff);        \
  wstat = PTR2WORD(stat) & 0xff;                          \
  ptr = stat - wstat + ((addr) & 0xff);                   \
  if((wstat & BANK_IO_TMP) || (((addr) & 0xff) == 0xff)) { \
    fcycles_tmp1 = fcycles;                         \
    dest = get_memory16_pieces_stub((addr), stat,   \
                                    &fcycles_tmp1, fplus_ptr, in_bank);     \
    fcycles = fcycles_tmp1;                         \
  } else {                                                \
    CYCLES_PLUS_2;                                  \
    dest = ptr[0] + (ptr[1] << 8);                  \
  }                                                       \
  addr_latch = save_addr;

#define GET_MEMORY24(addr, dest, in_bank)                       \
  save_addr = addr;                                       \
  stat = GET_PAGE_INFO_RD(((addr) >> 8) & 0xffff);        \
  wstat = PTR2WORD(stat) & 0xff;                          \
  ptr = stat - wstat + ((addr) & 0xff);                   \
  if((wstat & BANK_IO_TMP) || (((addr) & 0xfe) == 0xfe)) { \
    fcycles_tmp1 = fcycles;                         \
    dest = get_memory24_pieces_stub((addr), stat,   \
                                    &fcycles_tmp1, fplus_ptr, in_bank);     \
    fcycles = fcycles_tmp1;                         \
  } else {                                                \
    CYCLES_PLUS_3;                                  \
    dest = ptr[0] + (ptr[1] << 8) + (ptr[2] << 16); \
  }                                                       \
  addr_latch = save_addr;

#define GET_MEMORY_DIRECT_PAGE16(addr, dest)                            \
  save_addr = addr;                                               \
  if(psr & 0x100) {                                               \
    if((direct & 0xff) == 0) {                              \
      save_addr = (save_addr & 0xff) + direct;        \
    }                                                       \
  }                                                               \
  if((psr & 0x100) && (((addr) & 0xff) == 0xff)) {                \
    GET_MEMORY8(save_addr, getmem_tmp);                     \
    save_addr = (save_addr + 1) & 0xffff;                   \
    if((direct & 0xff) == 0) {                              \
      save_addr = (save_addr & 0xff) + direct;        \
    }                                                       \
    GET_MEMORY8(save_addr, dest);                           \
    dest = (dest << 8) + getmem_tmp;                        \
  } else {                                                        \
    GET_MEMORY16(save_addr, dest, 1);                       \
  }

/*
 _ macros do not do any MMU checking
*/

#if 1
#define MMU_CHECK(addr, val, bytes, in_page, in_bank)     \
  if (abort_support && check_mp_breakpoints(addr, val, bytes, in_page, in_bank)) { \
    g_ret1 = RET_MP;                                      \
    g_ret2 = saved_pc;                                    \
    kpc = saved_pc;                                       \
    psr = saved_psr;                                      \
    goto finish;                                          \
  }
#else
#define MMU_CHECK(addr, val, bytes, in_page, in_bank)
#endif
#define PUSH8(arg)                                        \
  MMU_CHECK(stack, arg, 1, 0, 0)                          \
  _PUSH8(arg)                                             \

#define _PUSH8(arg)                                       \
  _SET_MEMORY8(stack, arg);                               \
  stack--;                                                \
  if(psr & 0x100) {                                       \
    stack = 0x100 | (stack & 0xff);                       \
  }                                                       \
  stack = stack & 0xffff;

#define PUSH16(arg)                                       \
  tmp2 = stack - 1;                                       \
  if (psr >> 8) tmp2 = (tmp2 & 0xff) | 0x0100;            \
  tmp2 &= 0xffff;                                         \
  MMU_CHECK(tmp2, arg, 2, psr >> 8, 1)                    \
  _PUSH16(arg)

#define _PUSH16(arg)                                             \
  if((stack & 0xfe) == 0) {                               \
    /* stack will cross page! */                    \
    _PUSH8((arg) >> 8);                              \
    _PUSH8(arg);                                     \
  } else {                                                \
    stack -= 2;                                     \
    stack = stack & 0xffff;                         \
    _SET_MEMORY16(stack + 1, arg, 1);                \
  }

#define PUSH16_UNSAFE(arg)                                \
  MMU_CHECK((stack - 1) & 0xffff, arg, 2, 0, 1)           \
  _PUSH16_UNSAFE(arg)

#define _PUSH16_UNSAFE(arg)                               \
  save_addr = (stack - 1) & 0xffff;                       \
  stack -= 2;                                             \
  if(psr & 0x100) {                                       \
    stack = 0x100 | (stack & 0xff);                       \
  }                                                       \
  stack = stack & 0xffff;                                 \
  _SET_MEMORY16(save_addr, arg, 1);

#define PUSH24_UNSAFE(arg)                                \
  MMU_CHECK((stack - 2) & 0xffff, arg, 3, 0, 1)           \
  _PUSH24_UNSAFE(arg)

#define _PUSH24_UNSAFE(arg)                               \
  save_addr = (stack - 2) & 0xffff;                       \
  stack -= 3;                                             \
  if(psr & 0x100) {                                       \
    stack = 0x100 | (stack & 0xff);                       \
  }                                                       \
  stack = stack & 0xffff;                                 \
  _SET_MEMORY24(save_addr, arg, 1);

#define PULL8(dest)                                             \
  stack++;                                                \
  if(psr & 0x100) {                                       \
    stack = 0x100 | (stack & 0xff);                 \
  }                                                       \
  stack = stack & 0xffff;                                 \
  GET_MEMORY8(stack, dest);

#define PULL16(dest)                                            \
  if((stack & 0xfe) == 0xfe) {          /* page cross */        \
    PULL8(dest);                                    \
    PULL8(pull_tmp);                                \
    dest = (pull_tmp << 8) + dest;                  \
  } else {                                                \
    GET_MEMORY16(stack + 1, dest, 1);               \
    stack = (stack + 2) & 0xffff;                   \
    if(psr & 0x100) {                               \
      stack = 0x100 | (stack & 0xff);         \
    }                                               \
  }

#define PULL16_UNSAFE(dest)                                     \
  stack = (stack + 1) & 0xffff;                           \
  if(psr & 0x100) {                                       \
    stack = 0x100 | (stack & 0xff);                 \
  }                                                       \
  GET_MEMORY16(stack, dest, 1);                           \
  stack = (stack + 1) & 0xffff;                           \
  if(psr & 0x100) {                                       \
    stack = 0x100 | (stack & 0xff);                 \
  }

#define PULL24(dest)                                            \
  if((stack & 0xfc) == 0xfc) {          /* page cross */        \
    PULL8(dest);                                    \
    PULL8(pull_tmp);                                \
    pull_tmp = (pull_tmp << 8) + dest;              \
    PULL8(dest);                                    \
    dest = (dest << 16) + pull_tmp;                 \
  } else {                                                \
    GET_MEMORY24(stack + 1, dest, 1);               \
    stack = (stack + 3) & 0xffff;                   \
    if(psr & 0x100) {                               \
      stack = 0x100 | (stack & 0xff);         \
    }                                               \
  }

#define SET_MEMORY8(addr, val)                                    \
  MMU_CHECK(addr, val, 1, 0, 0)                                   \
  _SET_MEMORY8(addr, val)

#define _SET_MEMORY8(addr, val)                                   \
  LOG_DATA_MACRO(addr, val, 8);                                   \
  CYCLES_PLUS_1;                                                  \
  stat = GET_PAGE_INFO_WR(((addr) >> 8) & 0xffff);                \
  wstat = PTR2WORD(stat) & 0xff;                                  \
  ptr = stat - wstat + ((addr) & 0xff);                           \
  if(wstat) {                                                     \
    fcycles_tmp1 = fcycles;                                 \
    set_memory8_io_stub((addr), val, stat, &fcycles_tmp1,   \
                        fplus_x_m1);                            \
    fcycles = fcycles_tmp1;                                 \
  } else {                                                        \
    *ptr = val;                                             \
  }

#define SET_MEMORY16(addr, val, in_bank)                          \
  MMU_CHECK(addr, val, 2, 0, in_bank)                             \
  _SET_MEMORY16(addr, val, in_bank)


#define _SET_MEMORY16(addr, val, in_bank)                         \
  LOG_DATA_MACRO(addr, val, 16);                                  \
  stat = GET_PAGE_INFO_WR(((addr) >> 8) & 0xffff);                \
  wstat = PTR2WORD(stat) & 0xff;                                  \
  ptr = stat - wstat + ((addr) & 0xff);                           \
  if((wstat) || (((addr) & 0xff) == 0xff)) {                      \
    fcycles_tmp1 = fcycles;                                 \
    set_memory16_pieces_stub((addr), (val),                 \
                             &fcycles_tmp1, fplus_1, fplus_x_m1, in_bank);   \
    fcycles = fcycles_tmp1;                                 \
  } else {                                                        \
    CYCLES_PLUS_2;                                          \
    ptr[0] = (val);                                         \
    ptr[1] = (val) >> 8;                                    \
  }

#define SET_MEMORY24(addr, val, in_bank)                          \
  MMU_CHECK(addr, val, 3, 0, in_bank)                             \
  _SET_MEMORY24(addr, val, in_bank)


#define _SET_MEMORY24(addr, val, in_bank)                         \
  LOG_DATA_MACRO(addr, val, 24);                                  \
  stat = GET_PAGE_INFO_WR(((addr) >> 8) & 0xffff);                \
  wstat = PTR2WORD(stat) & 0xff;                                  \
  ptr = stat - wstat + ((addr) & 0xff);                           \
  if((wstat) || (((addr) & 0xfe) == 0xfe)) {                      \
    fcycles_tmp1 = fcycles;                                 \
    set_memory24_pieces_stub((addr), (val),                 \
                             &fcycles_tmp1, fplus_ptr, in_bank);             \
    fcycles = fcycles_tmp1;                                 \
  } else {                                                        \
    CYCLES_PLUS_3;                                          \
    ptr[0] = (val);                                         \
    ptr[1] = (val) >> 8;                                    \
    ptr[2] = (val) >> 16;                                   \
  }



extern int g_num_bp_breakpoints;
extern word32 g_bp_breakpoints[];

extern int g_num_tp_breakpoints;
extern word32 g_tp_breakpoints[];

extern int g_num_mp_breakpoints;
extern word32 g_mp_breakpoints[];

extern word32 g_abort_address;
extern word32 g_abort_value;
extern word32 g_abort_bytes;

int check_bp_breakpoints(word32 addr) {
  int i;

  for (i = 0; i < g_num_bp_breakpoints; ++i) {
    if (g_bp_breakpoints[i] == addr) {
      return 1;
    }
  }
  /* temp breakpoints */
  for (i = 0; i < g_num_tp_breakpoints; ++i) {
    if (g_tp_breakpoints[i] == addr) {
      return 1;
    }
  }
  return 0;
}


/* 65816 abort just pushes the orignal pc (-1?)  on the stack.
   MMU hardware could, of course, store the address [and value?]

*/


int check_mp_breakpoints(word32 addr, word32 value, unsigned bytes, int in_page, int in_bank) {
  byte *stat1;
  byte *stat2;
  word32 wstat;
  word32 mask = 0xffffff;
  int i;


  /* assumptions:
   * bytes is 1-3, so no possibility of crossing multiple pages
   * g_mp_breakpoints is in ascending order.
   */

  word32 a1 = addr;
  word32 a2 = addr + bytes - 1;

  if (in_page) {
    mask = 0x00ff;
  } else if (in_bank) {
    mask = 0xffff;
  } else {
    mask = 0xffffff;
  }

  a2 = (a2 & mask) | (addr & ~mask);

  stat1 = GET_PAGE_INFO_WR(((a1) >> 8) & 0xffff);
  stat2 = GET_PAGE_INFO_WR(((a2) >> 8) & 0xffff);
  wstat = PTR2WORD(stat1) | PTR2WORD(stat2);

  if (wstat & BANK_BREAK) {
    for (i = 0; i < g_num_mp_breakpoints; ++i) {
      word32 bp = g_mp_breakpoints[i];
      if (bp >= a1 && bp <= a2) {
        abort_it:
          g_abort_address = addr;
          g_abort_bytes = bytes;
          mask = (1 << (bytes << 3))-1;
          g_abort_value = value & mask;
          return 1;        
      }
      if (a2 < a1) {
        /* special logic for page/bank-wrapping... */
        if (bp >= a1 && bp <= (a1 | mask)) goto abort_it;
        if (bp >= (a1 & ~mask) && bp <= a2) goto abort_it;
      }
    }
  }

  return 0;
}



word32 get_memory8_io_stub(word32 addr, byte *stat, double *fcycs_ptr,
                           double fplus_x_m1) {
  double fcycles;
  word32 wstat;
  byte    *ptr;
  wstat = PTR2WORD(stat) & 0xff;

  fcycles = *fcycs_ptr;
  if(wstat & BANK_IO2_TMP || iostrobe == 1) {
    FCYCLES_ROUND;
    *fcycs_ptr = fcycles;
    return get_memory_io((addr), fcycs_ptr);
  } else {
    ptr = stat - wstat + (addr & 0xff);
    return *ptr;
  }
}

word32 get_memory16_pieces_stub(word32 addr, byte *stat, double *fcycs_ptr,
                                Fplus   *fplus_ptr, int in_bank) {
  byte    *ptr;
  double fcycles, fcycles_tmp1;
  double fplus_1;
  double fplus_x_m1;
  word32 addrp1;
  word32 wstat;
  word32 addr_latch;
  word32 ret;
  word32 tmp1;
  fcycles = *fcycs_ptr;
  fplus_1 = fplus_ptr->plus_1;
  fplus_x_m1 = fplus_ptr->plus_x_minus_1;
  GET_MEMORY8(addr, tmp1);
  addrp1 = addr + 1;
  if(in_bank) {
    addrp1 = (addr & 0xff0000) + (addrp1 & 0xffff);
  }
  GET_MEMORY8(addrp1, ret);
  *fcycs_ptr = fcycles;
  return (ret << 8) + (tmp1);
}

word32 get_memory24_pieces_stub(word32 addr, byte *stat, double *fcycs_ptr,
                                Fplus *fplus_ptr, int in_bank) {
  byte    *ptr;
  double fcycles, fcycles_tmp1;
  double fplus_1;
  double fplus_x_m1;
  word32 addrp1, addrp2;
  word32 wstat;
  word32 addr_latch;
  word32 ret;
  word32 tmp1;
  word32 tmp2;
  fcycles = *fcycs_ptr;
  fplus_1 = fplus_ptr->plus_1;
  fplus_x_m1 = fplus_ptr->plus_x_minus_1;
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
  *fcycs_ptr = fcycles;
  return (ret << 16) + (tmp2 << 8) + tmp1;
}

void set_memory8_io_stub(word32 addr, word32 val, byte *stat, double *fcycs_ptr,
                         double fplus_x_m1) {
  double fcycles;
  word32 setmem_tmp1;
  word32 tmp1, tmp2;
  byte    *ptr;
  word32 wstat;

  wstat = PTR2WORD(stat) & 0xff;

  ptr = stat - wstat + ((addr) & 0xff);
  fcycles = *fcycs_ptr;
  if(wstat & BANK_IO2_TMP) {
    FCYCLES_ROUND;
    *fcycs_ptr = fcycles;
    set_memory_io((addr), val, fcycs_ptr);
  } else if(wstat & BANK_SHADOW) {
    FCYCS_PTR_FCYCLES_ROUND_SLOW;
    tmp1 = (addr & 0xffff);
    setmem_tmp1 = g_slow_memory_ptr[tmp1];
    *ptr = val;
    if(setmem_tmp1 != ((val) & 0xff)) {
      g_slow_memory_ptr[tmp1] = val;
      slow_mem_changed[tmp1 >> CHANGE_SHIFT] |=
        (1 << (31-((tmp1 >> SHIFT_PER_CHANGE) & 0x1f)));
    }
  } else if(wstat & BANK_SHADOW2) {
    FCYCS_PTR_FCYCLES_ROUND_SLOW;
    tmp2 = (addr & 0xffff);
    tmp1 = 0x10000 + tmp2;
    setmem_tmp1 = g_slow_memory_ptr[tmp1];
    *ptr = val;
    if(setmem_tmp1 != ((val) & 0xff)) {
      g_slow_memory_ptr[tmp1] = val;
      slow_mem_changed[tmp2 >>CHANGE_SHIFT] |=
        (1 <<(31-((tmp2 >> SHIFT_PER_CHANGE) & 0x1f)));
    }
  } else {
    /* breakpoint only */
    *ptr = val;
  }
}

void set_memory16_pieces_stub(word32 addr, word32 val, double *fcycs_ptr,
                              double fplus_1, double fplus_x_m1, int in_bank) {
  byte    *ptr;
  byte    *stat;
  double fcycles, fcycles_tmp1;
  word32 addrp1;
  word32 wstat;
  fcycles = *fcycs_ptr;
  _SET_MEMORY8(addr, val);
  addrp1 = addr + 1;
  if(in_bank) {
    addrp1 = (addr & 0xff0000) + (addrp1 & 0xffff);
  }
  _SET_MEMORY8(addrp1, val >> 8);

  *fcycs_ptr = fcycles;
}

void set_memory24_pieces_stub(word32 addr, word32 val, double *fcycs_ptr,
                              Fplus   *fplus_ptr, int in_bank) {
  byte    *ptr;
  byte    *stat;
  double fcycles, fcycles_tmp1;
  double fplus_1;
  double fplus_x_m1;
  word32 addrp1, addrp2;
  word32 wstat;

  fcycles = *fcycs_ptr;
  fplus_1 = fplus_ptr->plus_1;
  fplus_x_m1 = fplus_ptr->plus_x_minus_1;
  _SET_MEMORY8(addr, val);
  addrp1 = addr + 1;
  if(in_bank) {
    addrp1 = (addr & 0xff0000) + (addrp1 & 0xffff);
  }
  _SET_MEMORY8(addrp1, val >> 8);
  addrp2 = addr + 2;
  if(in_bank) {
    addrp2 = (addr & 0xff0000) + (addrp2 & 0xffff);
  }
  _SET_MEMORY8(addrp2, val >> 16);

  *fcycs_ptr = fcycles;
}


word32 get_memory_c(word32 addr, int cycs)        {
  byte    *stat;
  byte    *ptr;
  double fcycles, fcycles_tmp1;
  double fplus_1;
  double fplus_x_m1;
  word32 addr_latch;
  word32 wstat;
  word32 ret;

  fcycles = 0;
  fplus_1 = 0;
  fplus_x_m1 = 0;
  GET_MEMORY8(addr, ret);
  return ret;
}

word32 get_memory16_c(word32 addr, int cycs)        {
  double fcycs;

  fcycs = 0;
  return get_memory_c(addr, (int)fcycs) +
         (get_memory_c(addr+1, (int)fcycs) << 8);
}

word32 get_memory24_c(word32 addr, int cycs)        {
  double fcycs;

  fcycs = 0;
  return get_memory_c(addr, (int)fcycs) +
         (get_memory_c(addr+1, (int)fcycs) << 8) +
         (get_memory_c(addr+2, (int)fcycs) << 16);
}




word32 get_memory32_c(word32 addr, int cycs)        {
  double fcycs;

  fcycs = 0;
  return get_memory_c(addr, (int)fcycs) +
         (get_memory_c(addr+1, (int)fcycs) << 8) +
         (get_memory_c(addr+2, (int)fcycs) << 16) +
         (get_memory_c(addr+3, (int)fcycs) << 24);
}



void set_memory_c(word32 addr, word32 val, int cycs)      {
  byte    *stat;
  byte    *ptr;
  double fcycles, fcycles_tmp1;
  double fplus_1;
  double fplus_x_m1;
  word32 wstat;
  fcycles = g_cur_dcycs - g_last_vbl_dcycs;
  fplus_1 = 0;
  fplus_x_m1 = 0;
  _SET_MEMORY8(addr, val);
}

void set_memory16_c(word32 addr, word32 val, int cycs)      {
  byte    *stat;
  byte    *ptr;
  double fcycles, fcycles_tmp1;
  double fplus_1, fplus_2;
  double fplus_x_m1;
  word32 wstat;

  fcycles = g_cur_dcycs - g_last_vbl_dcycs;
  fplus_1 = 0;
  fplus_2 = 0;
  fplus_x_m1 = 0;
  _SET_MEMORY16(addr, val, 0);
}

void set_memory24_c(word32 addr, word32 val, int cycs)      {
  set_memory_c(addr, val, 0);
  set_memory_c(addr + 1, val >> 8, 0);
  set_memory_c(addr + 2, val >> 16, 0);
}

void set_memory32_c(word32 addr, word32 val, int cycs)      {
  set_memory_c(addr, val, 0);
  set_memory_c(addr + 1, val >> 8, 0);
  set_memory_c(addr + 2, val >> 16, 0);
  set_memory_c(addr + 3, val >> 24, 0);
}



word32 do_adc_sbc8(word32 in1, word32 in2, word32 psr, int sub)        {
  word32 sum, carry, overflow;
  word32 zero;
  int decimal;

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

word32 do_adc_sbc16(word32 in1, word32 in2, word32 psr, int sub)        {
  word32 sum, carry, overflow;
  word32 tmp1, tmp2;
  word32 zero;
  int decimal;

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

int g_ret1;
int g_ret2;


void fixed_memory_ptrs_init()      {
  /* set g_slow_memory_ptr, g_rom_fc_ff_ptr, g_dummy_memory1_ptr, */
  /*  and rom_cards_ptr */

  // OG Filled allocated ptr parameter to free the memory
  g_slow_memory_ptr = memalloc_align(128*1024, 0, (void**)&g_slow_memory_ptr_allocated);
  g_dummy_memory1_ptr = memalloc_align(256, 1024, (void**)&g_dummy_memory1_ptr_allocated);
  g_rom_fc_ff_ptr = memalloc_align(256*1024, 512, (void**)&g_rom_fc_ff_ptr_allocated);
  g_rom_cards_ptr = memalloc_align(16*256, 256, (void**)&g_rom_cards_ptr_allocated);

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

// OG added fixed_memory_ptrs_shut
void fixed_memory_ptrs_shut() {

  free(g_slow_memory_ptr_allocated);
  free(g_dummy_memory1_ptr_allocated);
  free(g_rom_fc_ff_ptr_allocated);
  free(g_rom_cards_ptr_allocated);
  g_slow_memory_ptr=g_slow_memory_ptr_allocated= NULL;
  g_dummy_memory1_ptr = g_dummy_memory1_ptr_allocated = NULL;
  g_rom_fc_ff_ptr = g_rom_fc_ff_ptr_allocated = NULL;
  //  g_rom_cards_ptr = g_rom_cards_ptr = NULL;   // a mistake?
  g_rom_cards_ptr =  NULL;
}

#if defined(_MSC_VER)
  #include <intrin.h>
#elif defined(__GNUC__)

  #if defined(__i386__) ||  defined(__x86_64__)
    #include <x86intrin.h>
  #elif defined(_POSIX_MONOTONIC_CLOCK)
    #include <time.h>
    int64_t __rdtsc() {
      struct timespec tp = {0};
      int64_t timestamp = 0;
      if (clock_gettime(CLOCK_MONOTONIC, &tp) == 0)
        timestamp = tp.tv_sec * 1000000000 + tp.tv_nsec;
      return timestamp;
    }
  #elif defined(__powerpc__) || defined(__ppc__)
    #define __rdtsc() __builtin_ppc_mftb()
  #else
    #warning __rdtsc unavailable.
    #define __rdtsc() 0
  #endif
#else
  #warning __rdtsc unavailable.
  #define __rdtsc() 0
#endif

word32 get_itimer()        {
#if defined(_WIN32)
  LARGE_INTEGER count;
  if (QueryPerformanceCounter(&count))
    return count.LowPart;
#endif

  return __rdtsc();

}

void set_halt_act(int val)      {
  extern int g_dbg_shell;
  if(val == 1 && g_ignore_halts && !g_user_halt_bad && !g_dbg_shell) {
    g_code_red++;
  } else {
    halt_sim |= val;
    g_fcycles_stop = (double)0.0;
  }
}

void clr_halt_act()      {
  halt_sim = 0;
}

word32 get_remaining_operands(word32 addr, word32 opcode, word32 psr, Fplus *fplus_ptr)        {
  byte    *stat;
  byte    *ptr;
  double fcycles, fcycles_tmp1;
  double fplus_1, fplus_2, fplus_3;
  double fplus_x_m1;
  word32 addr_latch;
  word32 wstat;
  word32 save_addr;
  word32 arg;
  word32 addrp1;
  int size;

  fcycles = 0;
  fplus_1 = 0;
  fplus_2 = 0;
  fplus_3 = 0;
  fplus_x_m1 = 0;

  size = size_tab[opcode];

  addrp1 = (addr & 0xff0000) + ((addr + 1) & 0xffff);
  switch(size) {
    case 0:
      arg = 0;                  /* no args */
      break;
    case 1:
      GET_MEMORY8(addrp1, arg);
      break;                    /* 1 arg, already done */
    case 2:
      GET_MEMORY16(addrp1, arg, 1);
      break;
    case 3:
      GET_MEMORY24(addrp1, arg, 1);
      break;
    case 4:
      if(psr & 0x20) {
        GET_MEMORY8(addrp1, arg);
      } else {
        GET_MEMORY16(addrp1, arg, 1);
      }
      break;
    case 5:
      if(psr & 0x10) {
        GET_MEMORY8(addrp1, arg);
      } else {
        GET_MEMORY16(addrp1, arg, 1);
      }
      break;
    default:
      printf("Unknown size: %d\n", size);
      arg = 0;
      exit(-2);
  }

  return arg;
}


#define FETCH_OPCODE                                              \
  addr = saved_pc = kpc;                                          \
  saved_psr = psr;                                                \
  CYCLES_PLUS_2;                                                  \
  stat = GET_PAGE_INFO_RD(((addr) >> 8) & 0xffff);                \
  wstat = PTR2WORD(stat) & 0xff;                                  \
  ptr = stat - wstat + ((addr) & 0xff);                           \
  arg_ptr = ptr;                                                  \
  opcode = *ptr;                                                  \
  if (wstat & BANK_BREAK) {                                       \
    wstat &= ~BANK_BREAK;                                         \
    if (kpc_support && check_bp_breakpoints(addr)) {              \
        FINISH(RET_BP, addr);                                     \
    }                                                             \
  }                                                               \
  if((wstat & BANK_IO_TMP) || ((addr & 0xff) > 0xfc)) { \
    if((addr & 0xfffff0) == 0x00c700) {                     \
      if(addr == 0xc700) {                            \
        FINISH(RET_C700, 0);                    \
      } else if(addr == 0xc70a) {                     \
        FINISH(RET_C70A, 0);                    \
      } else if(addr == 0xc70d) {                     \
        FINISH(RET_C70D, 0);                    \
      }                                               \
    }                                                       \
    if(wstat & BANK_IO2_TMP || iostrobe == 1) {               \
      FCYCLES_ROUND;                                  \
      fcycles_tmp1 = fcycles;                         \
      opcode = get_memory_io((addr), &fcycles_tmp1);  \
      fcycles = fcycles_tmp1;                         \
    } else {                                                \
      opcode = *ptr;                                  \
    }                                                       \
    arg = get_remaining_operands(addr, opcode, psr, fplus_ptr); \
    arg_ptr = (byte *)&tmp_bytes;                           \
    arg_ptr[1] = arg;                                       \
    arg_ptr[2] = arg >> 8;                                  \
    arg_ptr[3] = arg >> 16;                                 \
  }

int enter_engine(Engine_reg *engine_ptr)     {
  register byte   *ptr;
  byte    *arg_ptr;
  Pc_log  *tmp_pc_ptr;
  byte    *stat;
  word32 wstat;
  word32 arg;
  register word32 kpc;
  register word32 acc;
  register word32 xreg;
  register word32 yreg;
  word32 stack;
  word32 dbank;
  register word32 direct;
  register word32 psr;
  register word32 zero;
  register word32 neg;
  word32 getmem_tmp;
  word32 save_addr;
  word32 pull_tmp;
  word32 tmp_bytes;
  double fcycles;
  Fplus   *fplus_ptr;
  double fplus_1;
  double fplus_2;
  double fplus_3;
  double fplus_x_m1;
  double fcycles_tmp1;

  word32 opcode;
  register word32 addr;
  word32 addr_latch;
  word32 tmp1, tmp2;

  word32 flags = 0;
  word32 saved_pc = 0;
  word32 saved_psr = 0;

  word32 abort_support = g_num_mp_breakpoints ? 1 : 0;
  word32 kpc_support = g_num_bp_breakpoints  + g_num_tp_breakpoints ? 1 : 0;

  flags = engine_ptr->flags;
  if (flags & FLAG_IGNORE_MP) abort_support = 0;
  if (flags & FLAG_IGNORE_BP) kpc_support = 0;


  tmp_pc_ptr = 0;

  kpc = engine_ptr->kpc;
  acc = engine_ptr->acc;
  xreg = engine_ptr->xreg;
  yreg = engine_ptr->yreg;
  stack = engine_ptr->stack;
  dbank = engine_ptr->dbank;
  direct = engine_ptr->direct;
  psr = engine_ptr->psr;
  fcycles = engine_ptr->fcycles;
  fplus_ptr = engine_ptr->fplus_ptr;
  zero = !(psr & 2);
  neg = (psr >> 7) & 1;

  fplus_1 = fplus_ptr->plus_1;
  fplus_2 = fplus_ptr->plus_2;
  fplus_3 = fplus_ptr->plus_3;
  fplus_x_m1 = fplus_ptr->plus_x_minus_1;

  g_ret1 = 0;
  g_ret2 = 0;

recalc_accsize:
  if(psr & 0x20) {
    while(fcycles <= g_fcycles_stop) {
#if 0
      if((neg & ~1) || (psr & (~0x1ff))) {
        halt_printf("psr = %04x\n", psr);
      }
#endif

      FETCH_OPCODE;

      LOG_PC_MACRO();

      switch(opcode) {
        default:
          halt_printf("acc8 unk op: %02x\n", opcode);
          arg = 9
#define ACC8
#include "defs_instr.h"
                * 2;
          break;
#include "8inst_c.h"
          break;
      }
    }
  } else {
    while(fcycles <= g_fcycles_stop) {
      FETCH_OPCODE;
      LOG_PC_MACRO();

      switch(opcode) {
        default:
          halt_printf("acc16 unk op: %02x\n", opcode);
          arg = 9
#undef ACC8
#include "defs_instr.h"
                * 2;
          break;
#include "16inst_c.h"
          break;
      }
    }
  }

finish:
  engine_ptr->kpc = kpc;
  engine_ptr->acc = acc;
  engine_ptr->xreg = xreg;
  engine_ptr->yreg = yreg;
  engine_ptr->stack = stack;
  engine_ptr->dbank = dbank;
  engine_ptr->direct = direct;
  engine_ptr->fcycles = fcycles;

  psr = psr & (~0x82);
  psr |= (neg << 7);
  psr |= ((!zero) << 1);

  engine_ptr->psr = psr;

  return (g_ret1 << 28) + g_ret2;
}


int g_engine_c_mode = 1;

int defs_instr_start_8 = 0;
int defs_instr_end_8 = 0;
int defs_instr_start_16 = 0;
int defs_instr_end_16 = 0;
int op_routs_start = 0;
int op_routs_end = 0;
