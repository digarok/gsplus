const char rcsid_voc_c[] = "@(#)$KmKId: voc.c,v 1.12 2023-09-23 17:52:44+00 kentd Exp $";

/************************************************************************/
/*			KEGS: Apple //gs Emulator			*/
/*			Copyright 2002-2023 by Kent Dickey		*/
/*									*/
/*	This code is covered by the GNU GPL v3				*/
/*	See the file COPYING.txt or https://www.gnu.org/licenses/	*/
/*	This program is provided with no warranty			*/
/*									*/
/*	The KEGS web page is kegs.sourceforge.net			*/
/*	You may contact the author at: kadickey@alumni.princeton.edu	*/
/************************************************************************/

// This file provides emulation of the Apple Video Overlay Card, which
//  will appear to be in slot 3 if g_voc_enable=1 (there's a config.c
//  setting to control enabling VOC).  The only currently supported VOC
//  feature is the SHR interlaced display using both Main and Aux memory
//  to provide a 640x400 (or 320x400) pixel display.

#include "defc.h"

extern word32 g_c02b_val;
extern int g_cur_a2_stat;
extern word32 g_vbl_count;

int	g_voc_enable = 0;		// Default to disabled for now
word32	g_voc_reg1 = 0x09;
word32	g_voc_reg3 = 0;
word32	g_voc_reg4 = 0;
word32	g_voc_reg5 = 0;
word32	g_voc_reg6 = 0;

word32
voc_devsel_read(word32 loc, dword64 dfcyc)
{
	// Reads to $c0b0-$c0bf.
	loc = loc & 0xf;
	switch(loc) {
	case 0:		// 0xc0b0
		return voc_read_reg0(dfcyc);
		break;
	case 1:		// 0xc0b1
		return g_voc_reg1;
		break;
	case 3:		// 0xc0b3
		return g_voc_reg3;
		break;
	case 4:		// 0xc0b4
		return g_voc_reg4;
		break;
	case 5:		// 0xc0b5
		return g_voc_reg5;
		break;
	case 6:		// 0xc0b6
		return g_voc_reg6;
		break;
	case 7:		// 0xc0b7, possible Uthernet 2 detection
		return 0x00;
		break;
	case 8:		// 0xc0b8, Second Sight detection by jpeGS program
		return 0x00;		// Second Sight returns 0x01
		break;
	case 0xd:	// 0xc0bd, A2OSX Uthernet 1 detection code
		return 0x00;
		break;
	}

	halt_printf("Tried to read: %04x\n", 0xc0b0 + loc);

	return 0;
}

void
voc_devsel_write(word32 loc, word32 val, dword64 dfcyc)
{
	// Writes to $c0b0-$c0bf.
	loc = loc & 0xf;
	switch(loc) {
	case 0:		// 0xc0b0
		// Write 0 to clear VBL interrupts
		if(val != 0) {
			halt_printf("VOC write %04x = %02x\n", loc, val);
		}
		return;
		break;
	case 1:		// 0xc0b1
		// bit 0: R/W: 1=GG Bus Enable
		//	When 0, I think VOC ignores writes to $c023,etc.
		// bit 2: R/W: 0=OutChromaFilter enabled, 1=ChromaFilter disab
		//   bit 2 is also TextMonoOver somehow using bit[5]==1
		// bit 3: R/W: 1=MainPageLin
		// bits 5:4: R/W: 00=Aux mem; 01=Main Memory; 11=Interlaced
		// bit 6: R/W: 1=Enable VBL Interrupt
		// bit 7: R/W: 1=Enable Line interrupts
		if(!g_voc_enable) {
			val = 0;
		}
		if(val & 0xc0) {
			halt_printf("VOC write %04x = %02x\n", loc, val);
		}
#if 0
		if(val != g_voc_reg1) {
			printf("$c0b1:%02x (was %02x)\n", val, g_voc_reg1);
		}
#endif
		g_voc_reg1 = val;
		voc_update_interlace(dfcyc);
		return;
		break;
	case 3:		// 0xc0b3
		// bits 2:0: R/W: Key Dissolve, 0=100% graphics, 7=100% video
		// bit 3: R/W: 1=Enhanced Dissolve enabled
		// bits 6:4: R/W: Non-Key Dissolve, 0=100% graphics, 7=100% vid
		// bit 7: R/W: 0=Output Setup Enabled, 1=Output Setup Disabled
		g_voc_reg3 = val;
		return;
		break;
	case 4:		// 0xc0b4
		// bits 3:0: R/W: KeyColor Blue
		// bits 7:4: R/W: KeyColor Green
		g_voc_reg4 = val;
		return;
		break;
	case 5:		// 0xc0b5
		// bits 3:0: R/W: KeyColor Red
		// bit 4: R/W: OutExtBlank: 0=Graphics, 1=External
		// bit 5: R/W: 0=GenLock enabled, 1=GenLock disabled
		// bit 6: R/W: 0=KeyColor enabled, 1=KeyColor disabled
		// bit 7: R/W: 1=Interlace mode enabled
		g_voc_reg5 = val;
		voc_update_interlace(dfcyc);
		return;
		break;
	case 6:		// 0xc0b6
		// Write 0 to cause AdjSave to occur
		// Write 8, then 9, then 8 again to cause AdjInc for Hue
		// Write a, then b, then a again to cause AdjDec for Hue
		// Write 4, then 5, then 4 again to cause AdjInc for Saturation
		// Write 6, then 7, then 6 again to cause AdjDec for Saturation
		// bit 3: hue, bit 2: saturation
		g_voc_reg6 = val;
		return;
		break;
	case 7:		// 0xc0b7
		// Written by System Disk 1.1 Desktop.sys to 0xfd, ignore
		if(val == 0xfd) {
			return;
		}
		break;
	case 0xa:
	case 0xb:	// 0xc0ba,0xc0bb written to 0 by A2OSX Uthernet1 detect
		if(val == 0) {
			return;
		}
		break;
	}
	halt_printf("Unknown Write %04x = %02x %016llx\n", 0xc0b0 + loc, val,
								dfcyc);
}

void
voc_iosel_c300_write(word32 loc, word32 val, dword64 dfcyc)
{
	// Writes to $c300-$c3ff
	halt_printf("Wrote VOC %04x = %02x %016llx\n", 0xc300 + (loc & 0xff),
								val, dfcyc);
}

void
voc_reset()
{
	g_voc_reg1 = 0x0d;		// [0]: GG Bus enable, [3]:MainPageLin
	g_voc_reg3 = 0x07;
	g_voc_reg4 = 0;
	g_voc_reg5 = 0x40;
	g_voc_reg6 = 0;
}

double g_voc_last_pal_vbl = 0;

word32
voc_read_reg0(dword64 dfcyc)
{
	word32	frame, in_vbl;

	if(!g_voc_enable) {
		return 0;
	}
	// Reading $c0b0.
	// c0b0: bit 2: R/O: 1=In VBL
	// c0b0: bit 3: R/O: 0=No Video Detected, 1=Video Detected
	// c0b0: bit 4: R/O: 1=Video Genlocked
	// c0b0: bit 5: R/O: 0=showing Field 0, 1=showing Field 1
	// c0b0: bit 6: R/O: 1=VBL Int Request pending
	// c0b0: bit 7: R/O: 1=Line Int Request pending
	in_vbl = in_vblank(dfcyc);
	dbg_log_info(dfcyc, 0, in_vbl, 0x1c0b0);
	frame = g_vbl_count & 1;
	return (frame << 5) | (in_vbl << 2);
}

void
voc_update_interlace(dword64 dfcyc)
{
	word32	new_stat, mask;

	new_stat = 0;
	if(((g_voc_reg1 & 0x30) == 0x30) && (g_voc_reg5 & 0x80)) {
		new_stat = ALL_STAT_VOC_INTERLACE;
	}
	if((g_voc_reg1 & 0x30) == 0x10) {		// Draw SHR from mainmem
		new_stat = ALL_STAT_VOC_MAIN;
	}
	mask = ALL_STAT_VOC_INTERLACE | ALL_STAT_VOC_MAIN;
	if((g_cur_a2_stat ^ new_stat) & mask) {
		// Interlace mode has changed
		g_cur_a2_stat &= (~mask);
		g_cur_a2_stat |= new_stat;
		printf("Change VOC interlace mode: %08x\n", new_stat);
		change_display_mode(dfcyc);
	}
}

