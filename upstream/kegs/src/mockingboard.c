const char rcsid_mockingboard_c[] = "@(#)$KmKId: mockingboard.c,v 1.26 2023-06-13 16:54:18+00 kentd Exp $";

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

#include "defc.h"
#include "sound.h"

// Mockingboard contains two pairs of a 6522/AY-8913, where the 6522 interfaces
//  the AY-8913 (which makes the sounds) to the Apple II.  Each AY-8913
//  contains 3 channels of sound: A,B,C.  Model each pair separately.
// The AY-8913 has 16 registers.  The documentation numbers them using octal!
// The AY8913 is accessed using ORB of the 6522 as control: 0 = Reset, 4=Idle
//	5=Reg read; 6=Write reg; 7=Latch reg address

extern Mockingboard g_mockingboard;
dword64	g_mockingboard_last_int_dusec = 0ULL;
dword64	g_mockingboard_event_int_dusec = 0ULL;	// 0 -> no event pending

extern int g_irq_pending;
extern double g_dsamps_per_dfcyc;

void
mock_ay8913_reset(int pair_num, dword64 dfcyc)
{
	Ay8913	*ay8913ptr;
	int	i;

	if(dfcyc) {
		// Avoid unused parameter warning
	}
	ay8913ptr = &(g_mockingboard.pair[pair_num].ay8913);
	ay8913ptr->reg_addr_latch = 16;		// out-of-range, mb-audit1.3
	for(i = 0; i < 16; i++) {
		ay8913ptr->regs[i] = 0;
	}
	for(i = 0; i < 3; i++) {
		ay8913ptr->toggle_tone[i] = 0;
		ay8913ptr->tone_samp[i] = 0;
	}
	ay8913ptr->noise_val = 0x12345678;
	ay8913ptr->noise_samp = 0;
	ay8913ptr->env_dsamp = 0;
}

void
mockingboard_reset(dword64 dfcyc)
{
	word32	timer1_latch;

	timer1_latch = g_mockingboard.pair[0].mos6522.timer1_latch;
	memset(&g_mockingboard, 0, sizeof(g_mockingboard));

	g_mockingboard_last_int_dusec = (dfcyc >> 16) << 16;
	if(g_mockingboard_event_int_dusec != 0) {
		(void)remove_event_mockingboard();
	}
	g_mockingboard_event_int_dusec = 0;
	printf("At reset, timer1_latch: %08x\n", timer1_latch);
	if((timer1_latch & 0xffff) == 0x234) {			// MB-audit
		g_mockingboard.pair[0].mos6522.timer1_latch = timer1_latch;
	} else {
		g_mockingboard.pair[0].mos6522.timer1_latch = 0xff00;
	}
	g_mockingboard.pair[0].mos6522.timer1_counter = 0x2ff00;
	g_mockingboard.pair[0].mos6522.timer2_counter = 0x2ff00;
	g_mockingboard.pair[1].mos6522.timer1_latch = 0xff00;
	g_mockingboard.pair[1].mos6522.timer1_counter = 0x2ff00;
	g_mockingboard.pair[1].mos6522.timer2_counter = 0x2ff00;

	mock_ay8913_reset(0, dfcyc);
	mock_ay8913_reset(1, dfcyc);
}

void
mock_show_pair(int pair_num, dword64 dfcyc, const char *str)
{
	Mos6522	*mos6522ptr;

	mos6522ptr = &(g_mockingboard.pair[pair_num].mos6522);
	printf("Mock %d %s, t1_lat:%05x, t1_c:%05x, t2_l:%05x t2_c:%05x, ifr:"
		"%02x, acr:%02x, ier:%02x\n", pair_num, str,
		mos6522ptr->timer1_latch, mos6522ptr->timer1_counter,
		mos6522ptr->timer2_latch, mos6522ptr->timer2_counter,
		mos6522ptr->ifr, mos6522ptr->acr, mos6522ptr->ier);
	printf("  dfcyc:%016llx, event_int:%lld\n", dfcyc,
					g_mockingboard_event_int_dusec);

}

// Timers work as follows: if written with '10' in cycle 0, on cycle 1 the
//  counters loads with '10', and counts down to 9 on cycle 2...down to 1
//  on cycle 10, then 0 on cycle 11, and then signals an interrupt on cycle 12
// To handle this, timers are "read value + 1" so that timer==0 means the
//  timer should interrupt right now.  So to write '10' to a timer, the code
//  needs to write 10+2=12 to the variable, so that on the next cycle, it is
//  11, which will be returned as 10 to software reading the reg.

void
mock_update_timers(int doit, dword64 dfcyc)
{
	Mos6522	*mos6522ptr;
	dword64	dusec, ddiff, dleft, timer1_int_dusec, timer2_int_dusec;
	dword64	closest_int_dusec, event_int_dusec;
	word32	timer_val, ier, timer_eff, timer_latch, log_ifr;
	int	i;

	dbg_log_info(dfcyc, doit, g_mockingboard.pair[0].mos6522.ifr |
			(g_mockingboard.pair[0].mos6522.ier << 8), 0xc0);

	dusec = dfcyc >> 16;
	ddiff = dusec - g_mockingboard_last_int_dusec;
	if(!doit && (dusec <= g_mockingboard_last_int_dusec)) {
		return;					// Nothing more to do
	}

	// printf("mock_update_timers at %lf %016llx, ddiff:%llx\n", dfcyc,
	//						dusec, ddiff);
	// Update timers by ddiff integer cycles, calculate next event time
	g_mockingboard_last_int_dusec = dusec;
	closest_int_dusec = 0;
	for(i = 0; i < 2; i++) {		// pair_num
		mos6522ptr = &(g_mockingboard.pair[i].mos6522);
		timer1_int_dusec = 0;
		timer_val = mos6522ptr->timer1_counter;
		ier = mos6522ptr->ier;
		timer_eff = (timer_val & 0x1ffff);
		dleft = ddiff;
		timer_latch = mos6522ptr->timer1_latch + 2;
		dbg_log_info(dfcyc, (word32)dleft, timer_val, 0xcb);
		dbg_log_info(dfcyc, ier, timer_latch, 0xcb);
		if(dleft < timer_eff) {
			// Move ahead only a little, no triggering
			timer_val = timer_val - (word32)dleft;
			if(ddiff) {
				// printf("New timer1_val:%05x, dleft:%08llx\n",
				//		timer_val, dleft);
			}
			if(((mos6522ptr->ifr & 0x40) == 0) && (ier & 0x40)) {
				// IFR not set yet, prepare an event
				timer1_int_dusec = dusec +
							(timer_val & 0x1ffff);
				dbg_log_info(dfcyc, (word32)timer1_int_dusec,
							timer_val, 0xcd);
				// printf("t1_int_dusec: %016llx\n",
				//			timer1_int_dusec);
			}
		} else {
			// Timer1 has triggered now (maybe rolled over more
			//  than once).
			log_ifr = 0;
			if((timer_val & 0x20000) == 0) {
				// Either free-running, or not one-shot already
				//  triggered
				// Set interrupt: Ensure IFR | 0x40 is set
				mos6522ptr->ifr = mock_6522_new_ifr(dfcyc, i,
						mos6522ptr->ifr | 0x40, ier);
				log_ifr = 1;
			}
			dleft -= timer_eff;
			if(dleft >= timer_latch) {
				// It's rolled over several times, remove those
				dleft = dleft % timer_latch;
				dbg_log_info(dfcyc, (word32)dleft, timer_latch,
									0xcc);
			}
			if(dleft == 0) {
				dleft = timer_latch;
			}
			timer_val = (timer_latch - dleft) & 0x1ffff;
			if((mos6522ptr->acr & 0x40) == 0) {
				// ACR6=0: One shot mode, mark it as triggered
				timer_val |= 0x20000;
			}
			if(log_ifr) {
				dbg_log_info(dfcyc,
					mos6522ptr->ifr | (ier << 8),
					timer_val, 0xc3);
			}
		}

#if 0
		printf("%016llx ch%d timer1 was %05x, now %05x\n", dfcyc, i,
					mos6522ptr->timer1_counter, timer_val);
#endif

		mos6522ptr->timer1_counter = timer_val;
		dbg_log_info(dfcyc, timer_val, timer_latch, 0xce);

		// Handle timer2
		timer2_int_dusec = 0;
		timer_val = mos6522ptr->timer2_counter;
		timer_eff = timer_val & 0x1ffff;
		dleft = ddiff;
		if(mos6522ptr->acr & 0x20) {
			// Count pulses mode.  Just don't count
			dleft = 0;
		}
		if(dleft < timer_eff) {
			// Move ahead only a little, no triggering
			timer_val = timer_val - (word32)dleft;
			if(((mos6522ptr->ifr & 0x20) == 0) && (ier & 0x20)) {
				// IFR not set yet, prepare an event
				timer2_int_dusec = dusec +
							(timer_val & 0x1ffff);
				//printf("t2_int_dusec: %016llx\n",
				//			timer1_int_dusec);
			}
		} else if(timer_val & 0x20000) {
			// And already triggered once, just update count
			timer_val = ((timer_eff - dleft) & 0xffff) | 0x20000;
		} else {
			// Has not triggered once yet, but it will now
			mos6522ptr->ifr = mock_6522_new_ifr(dfcyc, i,
						mos6522ptr->ifr | 0x20, ier);
			timer_val = ((timer_val - dleft) & 0xffff) | 0x20000;
			dbg_log_info(dfcyc, mos6522ptr->ifr | (ier << 8),
					timer_val, 0xc4);
		}

		// printf("ch%d timer2 was %05x, now %05x\n", i,
		//			mos6522ptr->timer2_counter, timer_val);

		mos6522ptr->timer2_counter = timer_val;

		if(timer1_int_dusec && timer2_int_dusec) {
			timer1_int_dusec = MY_MIN(timer1_int_dusec,
							timer2_int_dusec);
		}
		if(timer1_int_dusec) {
			if(closest_int_dusec) {
				closest_int_dusec = MY_MIN(closest_int_dusec,
							timer1_int_dusec);
			} else {
				closest_int_dusec = timer1_int_dusec;
			}
		}
	}

	event_int_dusec = g_mockingboard_event_int_dusec;
	if(closest_int_dusec) {
		// See if this is sooner than the current pending event
		// printf("closest_int_dusec: %016llx, event_int:%016llx\n",
		//			closest_int_dusec, event_int_dusec);
		doit = 0;
		if(event_int_dusec && (closest_int_dusec < event_int_dusec)) {
			// There was a pending event.  Discard it
			// printf("Call remove_event_mockingboard\n");
			remove_event_mockingboard();
			doit = 1;
		}
		if(!event_int_dusec || doit) {
			//printf("Call add_event_mockingboard: %016llx %lld\n",
			//	closest_int_dusec, closest_int_dusec);
			add_event_mockingboard(closest_int_dusec << 16);
			g_mockingboard_event_int_dusec = closest_int_dusec;
			dbg_log_info(dfcyc,
				(word32)(closest_int_dusec - (dfcyc >> 16)),
								0, 0xc1);
		}
	}
}

void
mockingboard_event(dword64 dfcyc)
{
	// Received an event--we believe we may need to set an IRQ now.
	// Event was already removed from the event queue
	// printf("Mockingboard_event received at %016llx\n", dfcyc);
	dbg_log_info(dfcyc, 0, 0, 0xc2);
	g_mockingboard_event_int_dusec = 0;
	mock_update_timers(1, dfcyc);
}

word32
mockingboard_read(word32 loc, dword64 dfcyc)
{
	int	pair_num;

	// printf("mockingboard read: %04x\n", loc);
	pair_num = (loc >> 7) & 1;		// 0 or 1
	return mock_6522_read(pair_num, loc & 0xf, dfcyc);
}

void
mockingboard_write(word32 loc, word32 val, dword64 dfcyc)
{
	int	pair_num;

	// printf("mockingboard write: %04x=%02x\n", loc, val);
	pair_num = (loc >> 7) & 1;		// 0 or 1
	mock_6522_write(pair_num, loc & 0xf, val, dfcyc);
}

word32
mock_6522_read(int pair_num, word32 loc, dword64 dfcyc)
{
	Mos6522	*mos6522ptr;
	word32	val;

	// Read from 6522 #pair_num loc (0-15)
	mos6522ptr = &(g_mockingboard.pair[pair_num].mos6522);
	val = 0;
	switch(loc) {
	case 0x0:		// ORB/IRB
		// Connected to AY8913 { RESET, BDIR, BC1 }
		val = mos6522ptr->orb;
			// There are no outputs from AY8913 to the 6522 B Port
		break;
	case 0x1:		// ORA
	case 0xf:		// ORA, no handshake
		val = mos6522ptr->ora;
		break;
	case 0x2:		// DDRB
		val = mos6522ptr->ddrb;
		break;
	case 0x3:		// DDRA
		val = mos6522ptr->ddra;
		break;
	case 0x4:		// T1C-L (timer[0])
		mock_update_timers(1, dfcyc);
		val = (mos6522ptr->timer1_counter - 1) & 0xff;
		mos6522ptr->ifr = mock_6522_new_ifr(dfcyc, pair_num,
				mos6522ptr->ifr & (~0x40), mos6522ptr->ier);
			// Clear Bit 6
		mock_update_timers(1, dfcyc);	// Prepare another int (maybe)
		dbg_log_info(dfcyc, mos6522ptr->ifr, val, 0xc5);
		break;
	case 0x5:		// T1C-H
		mock_update_timers(1, dfcyc);
		val = ((mos6522ptr->timer1_counter - 1) >> 8) & 0xff;
		break;
	case 0x6:		// T1L-L
		val = mos6522ptr->timer1_latch & 0xff;
		break;
	case 0x7:		// T1L-H
		val = (mos6522ptr->timer1_latch >> 8) & 0xff;
		break;
	case 0x8:		// T2C-L
		mock_update_timers(1, dfcyc);
		val = (mos6522ptr->timer2_counter - 1) & 0xff;
		mos6522ptr->ifr = mock_6522_new_ifr(dfcyc, pair_num,
			mos6522ptr->ifr & (~0x20), mos6522ptr->ier);
			// Clear Bit 5
		mock_update_timers(1, dfcyc);	// Prepare another int (maybe)
		dbg_log_info(dfcyc, mos6522ptr->ifr, val, 0xc6);
		break;
	case 0x9:		// T2C-H
		mock_update_timers(1, dfcyc);
		val = ((mos6522ptr->timer2_counter - 1) >> 8) & 0xff;
		break;
	case 0xa:		// SR
		val = mos6522ptr->sr;
		//halt_printf("Reading SR %d %02x\n", pair_num, val);
		break;
	case 0xb:		// ACR
		val = mos6522ptr->acr;
		break;
	case 0xc:		// PCR
		val = mos6522ptr->pcr;
		break;
	case 0xd:		// IFR
		mock_update_timers(1, dfcyc);
		val = mos6522ptr->ifr;
		break;
	case 0xe:		// IER
		val = mos6522ptr->ier | 0x80;		// Despite MOS6522
		break;					//  datasheet, bit 7 = 1
	}
	// printf("6522 %d loc:%x ret:%02x\n", pair_num, loc, val);
	return val;
}

void
mock_6522_write(int pair_num, word32 loc, word32 val, dword64 dfcyc)
{
	Mos6522	*mos6522ptr;
	word32	ora, orb, new_val, mask;

	// Write to 6522 #num6522 loc (0-15)

	// printf("6522 %d loc:%x write:%02x\n", pair_num, loc, val);

	mos6522ptr = &(g_mockingboard.pair[pair_num].mos6522);
	switch(loc) {
	case 0x0:		// ORB
		mask = mos6522ptr->ddrb;
		orb = mos6522ptr->orb;
		new_val = (val & mask) | (orb & (~mask));
		if(orb != new_val) {
			mock_ay8913_control_update(pair_num, new_val, orb,
									dfcyc);
		}
		mos6522ptr->orb = new_val;
		break;
	case 0x1:		// ORA
	case 0xf:		// ORA, no handshake
		mask = mos6522ptr->ddra;
		ora = mos6522ptr->ora;
		new_val = (val & mask) | (ora & (~mask));
		mos6522ptr->ora = new_val;
		break;
	case 0x2:		// DDRB
		orb = mos6522ptr->orb;
		new_val = (orb & val) | (orb & (~val));
		if(orb != new_val) {
			mock_ay8913_control_update(pair_num, new_val, orb,
									dfcyc);
		}
		mos6522ptr->orb = new_val;
		mos6522ptr->ddrb = val;
		return;
	case 0x3:		// DDRA
		ora = mos6522ptr->ora;
		mos6522ptr->ora = (ora & val) | (ora & (~val));
		mos6522ptr->ddra = val;
		return;
	case 0x4:		// T1C-L
		mock_update_timers(0, dfcyc);
		mos6522ptr->timer1_latch =
				(mos6522ptr->timer1_latch & 0x1ff00) | val;
		// printf("Set T1C-L, timer1_latch=%05x\n",
		//				mos6522ptr->timer1_latch);
		break;
	case 0x5:		// T1C-H
		mock_update_timers(1, dfcyc);
		val = (mos6522ptr->timer1_latch & 0xff) | (val << 8);
		mos6522ptr->timer1_latch = val;
		mos6522ptr->timer1_counter = val + 2;
			// The actual timer1_counter update happens next cycle,
			//  so we want val+1, plus another 1
		// printf("Set T1C-H, timer1_latch=%05x\n",
		//				mos6522ptr->timer1_latch);
		mos6522ptr->ifr = mock_6522_new_ifr(dfcyc, pair_num,
			mos6522ptr->ifr & (~0x40), mos6522ptr->ier);
			// Clear Bit 6
		mock_update_timers(1, dfcyc);
		dbg_log_info(dfcyc, mos6522ptr->ifr, val, 0xc7);
		break;
	case 0x6:		// T1L-L
		mock_update_timers(0, dfcyc);
		mos6522ptr->timer1_latch =
				(mos6522ptr->timer1_latch & 0x1ff00) | val;
		break;
	case 0x7:		// T1L-H
		mock_update_timers(1, dfcyc);
		val = (mos6522ptr->timer1_latch & 0xff) | (val << 8);
		mos6522ptr->timer1_latch = val;
		mos6522ptr->ifr = mock_6522_new_ifr(dfcyc, pair_num,
			mos6522ptr->ifr & (~0x40), mos6522ptr->ier);
			// Clear Bit 6
		mock_update_timers(1, dfcyc);
		// mock_show_pair(pair_num, dfcyc, "Wrote T1L-H");
		dbg_log_info(dfcyc, mos6522ptr->ifr, val, 0xc8);
		break;
	case 0x8:		// T2C-L
		mos6522ptr->timer2_latch = (mos6522ptr->timer2_latch & 0xff00) |
									val;
		break;
	case 0x9:		// T2C-H
		mock_update_timers(1, dfcyc);
		val = (mos6522ptr->timer2_latch & 0xff) | (val << 8);
		mos6522ptr->timer2_latch = val;
		mos6522ptr->timer2_counter = val + 2;
		mos6522ptr->ifr = mock_6522_new_ifr(dfcyc, pair_num,
			mos6522ptr->ifr & (~0x20), mos6522ptr->ier);
			// Clear bit 5
		mock_update_timers(1, dfcyc);
		break;
	case 0xa:		// SR
		mos6522ptr->sr = val;
		halt_printf("Wrote SR reg: %d %02x\n", pair_num, val);
		break;
	case 0xb:		// ACR
		mock_update_timers(0, dfcyc);
		mos6522ptr->acr = val;
		mock_update_timers(1, dfcyc);
		break;
	case 0xc:		// PCR
		mos6522ptr->pcr = val;
		break;
	case 0xd:		// IFR
		mock_update_timers(1, dfcyc);
		mos6522ptr->ifr = mock_6522_new_ifr(dfcyc, pair_num,
				mos6522ptr->ifr & (~val), mos6522ptr->ier);
		mock_update_timers(1, dfcyc);
		dbg_log_info(dfcyc, mos6522ptr->ifr, val, 0xc9);
		break;
	case 0xe:		// IER
		// Recalculate effective IFR with new IER
		mock_update_timers(1, dfcyc);
		if(val & 0x80) {			// Set EIR bits
			val = mos6522ptr->ier | val;
		} else {				// Clear EIR bits
			val = mos6522ptr->ier & (~val);
		}
		val = val & 0x7f;
		mos6522ptr->ier = val;
		mos6522ptr->ifr = mock_6522_new_ifr(dfcyc, pair_num,
							mos6522ptr->ifr, val);
		mock_update_timers(1, dfcyc);
		// mock_show_pair(pair_num, dfcyc, "Wrote IER");
		dbg_log_info(dfcyc, mos6522ptr->ifr, val, 0xca);
		break;
	}
}

word32
mock_6522_new_ifr(dword64 dfcyc, int pair_num, word32 ifr, word32 ier)
{
	word32	irq_mask;

	// Determine if there are any interrupts pending now
	irq_mask = IRQ_PENDING_MOCKINGBOARDA << pair_num;
	if((ifr & ier & 0x7f) == 0) {
		// No IRQ pending anymore
		ifr = ifr & 0x7f;		// Clear bit 7
		if(g_irq_pending & irq_mask) {
			// printf("MOCK INT OFF\n");
		}
		remove_irq(irq_mask);
		dbg_log_info(dfcyc, (ier << 8) | ifr, g_irq_pending, 0xcf);
	} else {
		// IRQ is pending
		ifr = ifr | 0x80;		// Set bit 7
		if(!(g_irq_pending & irq_mask)) {
			// printf("MOCK INT ON\n");
		}
		add_irq(irq_mask);
		dbg_log_info(dfcyc, (ier << 8) | ifr, g_irq_pending, 0xcf);
	}
	return ifr;
}

void
mock_ay8913_reg_read(int pair_num)
{
	Mos6522	*mos6522ptr;
	Ay8913	*ay8913ptr;
	word32	reg_addr_latch, mask, val, ora;

	mos6522ptr = &(g_mockingboard.pair[pair_num].mos6522);
	ay8913ptr = &(g_mockingboard.pair[pair_num].ay8913);
	reg_addr_latch = ay8913ptr->reg_addr_latch;
	val = 0;
	if(reg_addr_latch < 16) {
		val = ay8913ptr->regs[reg_addr_latch];
	}
	// ORA at 6522 is merge of ORA using DDRA
	mask = mos6522ptr->ddra;
	ora = mos6522ptr->ora;
	mos6522ptr->ora = (ora & mask) | (val & (~mask));
}

word32 g_mock_channel_regs[3] = {
	0x39c3,		// channel A: regs 0,1,6,7,8,11,12,13
	0x3acc,		// channel B: regs 2,3,6,7,9,11,12,13
	0x3cf0		// channel C: regs 4,5,6,7,10,11,12,13
};

void
mock_ay8913_reg_write(int pair_num, dword64 dfcyc)
{
	Mos6522	*mos6522ptr;
	Ay8913	*ay8913ptr;
	double	dsamps;
	word32	reg_addr_latch, ora, mask, rmask, do_print;
	int	i;

	mos6522ptr = &(g_mockingboard.pair[pair_num].mos6522);
	ay8913ptr = &(g_mockingboard.pair[pair_num].ay8913);
	reg_addr_latch = ay8913ptr->reg_addr_latch;
	ora = mos6522ptr->ora;
	dsamps = dfcyc * g_dsamps_per_dfcyc;
	if(reg_addr_latch < 16) {
		mask = (g_mockingboard.disable_mask >> (3*pair_num)) & 7;
		rmask = 0;
		do_print = 0;
		for(i = 0; i < 3; i++) {
			if(((mask >> i) & 1) == 0) {
				rmask |= g_mock_channel_regs[i];
			}
		}
		do_print = (rmask >> reg_addr_latch) & 1;
		if((ora != ay8913ptr->regs[reg_addr_latch]) ||
						(reg_addr_latch == 13)) {
			// New value, or writing to Envelope control
			do_print = 0;
			if(do_print) {
				printf("%.2lf %016llx mock pair%d reg[%d]="
					"%02x. [2,3]=%02x_%02x [67]=%02x,%02x, "
					"[9]=%02x, [12,11]=%02x_%02x [13]="
					"%02x\n", dsamps, dfcyc, pair_num,
					reg_addr_latch, ora,
					ay8913ptr->regs[3], ay8913ptr->regs[2],
					ay8913ptr->regs[6], ay8913ptr->regs[7],
					ay8913ptr->regs[9], ay8913ptr->regs[12],
					ay8913ptr->regs[11],
					ay8913ptr->regs[13]);
			}
			sound_play(dfcyc);
		}
		ay8913ptr->regs[reg_addr_latch] = ora;
		if(reg_addr_latch == 13) {		// Envelope control
			ay8913ptr->env_dsamp &= 0x1fffffffffffULL;
			// Clear "hold" in (env_val & (0x80 << 40))
		}
	}
}

void
mock_ay8913_control_update(int pair_num, word32 new_val, word32 prev_val,
							dword64 dfcyc)
{
	Mos6522	*mos6522ptr;
	Ay8913	*ay8913ptr;

	mos6522ptr = &(g_mockingboard.pair[pair_num].mos6522);
	ay8913ptr = &(g_mockingboard.pair[pair_num].ay8913);
	// printf("ay8913 %d control now:%02x\n", pair_num, new_val);

	// new_val and prev_val are { reset_l, BDIR, BC1 }
	// 4=Idle; 5=Read; 6=Write; 7=Latch_addr
	// Latch new address and write data at the time the ctl changes to Idle
	// Do read as soon as the ctl indicates to do a read.

	if((new_val & 4) == 0) {
		mock_ay8913_reset(pair_num, dfcyc);
		return;
	}
	new_val = new_val & 7;
	prev_val = prev_val & 7;
	if(prev_val == 7) {		// Latch new address, latch it now
		ay8913ptr->reg_addr_latch = mos6522ptr->ora;
	} else if(prev_val == 6) {	// Write data, do it now
		mock_ay8913_reg_write(pair_num, dfcyc);
	}
	if(new_val == 5) {
		mock_ay8913_reg_read(pair_num);
	}
}

void
mockingboard_show(int got_num, word32 disable_mask)
{
	int	i, j;

	if(got_num) {
		g_mockingboard.disable_mask = disable_mask;
	}
	printf("g_mockingboard.disable_mask:%02x\n",
						g_mockingboard.disable_mask);
	for(i = 0; i < 2; i++) {
		for(j = 0; j < 14; j++) {
			printf("Mockingboard pair[%d].reg[%d]=%02x\n", i, j,
				g_mockingboard.pair[i].ay8913.regs[j]);
		}
	}
}

