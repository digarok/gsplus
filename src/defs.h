/*
 GSPLUS - Advanced Apple IIGS Emulator Environment
 Copyright (C) 2016 - Dagen Brock
 
 Copyright (C) 2010 by GSport contributors

 Based on the KEGS emulator written by and Copyright (C) 2003 Kent Dickey

 This program is free software; you can redistribute it and/or modify it 
 under the terms of the GNU General Public License as published by the 
 Free Software Foundation; either version 2 of the License, or (at your 
 option) any later version.

 This program is distributed in the hope that it will be useful, but 
 WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
 or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License 
 for more details.

 You should have received a copy of the GNU General Public License along 
 with this program; if not, write to the Free Software Foundation, Inc., 
 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "defcomm.h"

link		.reg	%r2
acc		.reg	%r3
xreg		.reg	%r4
yreg		.reg	%r5
stack		.reg	%r6
dbank		.reg	%r7
direct		.reg	%r8
neg		.reg	%r9
zero		.reg	%r10
psr		.reg	%r11
kpc		.reg	%r12
const_fd	.reg	%r13
instr		.reg	%r14
#if 0
cycles		.reg	%r13
kbank		.reg	%r14
#endif

page_info_ptr	.reg	%r15
inst_tab_ptr	.reg	%r16
fcycles_stop_ptr .reg	%r17
addr_latch	.reg	%r18

scratch1	.reg	%r19
scratch2	.reg	%r20
scratch3	.reg	%r21
scratch4	.reg	%r22
;instr		.reg	%r23		; arg3

fcycles		.reg	%fr12
fr_plus_1	.reg	%fr13
fr_plus_2	.reg	%fr14
fr_plus_3	.reg	%fr15
fr_plus_x_m1	.reg	%fr16
fcycles_stop	.reg	%fr17
fcycles_last_dcycs .reg	%fr18

ftmp1		.reg	%fr4
ftmp2		.reg	%fr5
fscr1		.reg	%fr6

#define LDC(val,reg) ldil L%val,reg ! ldo R%val(reg),reg

