const char rcsid_doc_c[] = "@(#)$KmKId: doc.c,v 1.2 2023-06-05 18:59:51+00 kentd Exp $";

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

// Ensoniq 5503 DOC routines
// The Ensoniq DOC is controlled through the Sound GLU at $C03C-$C03F,
//  where $C03E-$C03F are a 16-bit pointer to "what" to access, $C03D is
//  the data register, and $C03C is a control register with volume.
// The DOC operates at 894.886KHZ (7M clock divided by 8).  It visits each
//  oscillator (up to 32) once per clock, plus 2 extra clocks for DOC RAM
//  refresh.
// KEGS cheats and pretends the DOC runs at 48KHz, and visits all oscillators
//  each "sample".  KEGS adjusts the internal accumulators so the right
//  frequency is achieved.  This allows the sample calculations to be
//  greatly simplified, and achieves higher fidelity when all 32 osc are
//  enabled (which is generally how most Apple IIgs code works).

#include "defc.h"
#include "sound.h"

#define DOC_LOG(a,b,c,d)

extern int Verbose;
extern int g_use_shmem;
extern word32 g_vbl_count;
extern int g_preferred_rate;

extern word32 g_c03ef_doc_ptr;

extern dword64 g_last_vbl_dfcyc;

byte doc_ram[0x10000 + 16];

word32 g_doc_sound_ctl = 0;
word32 g_doc_saved_val = 0;
int	g_doc_num_osc_en = 1;
double	g_drecip_osc_en_plus_2 = 1.0 / (double)(1 + 2);

int	g_doc_vol = 8;
int	g_doc_saved_ctl = 0;
int	g_num_osc_interrupting = 0;
Doc_reg g_doc_regs[32];

word32 doc_reg_e0 = 0xff;

extern double g_drecip_audio_rate;
extern double g_dsamps_per_dfcyc;
extern double g_fcyc_per_samp;

/* local function prototypes */
void doc_write_ctl_reg(dword64 dfcyc, int osc, int val);

#define DOC_SCAN_RATE	(DCYCS_28_MHZ/32.0)

#define UPDATE_G_DCYCS_PER_DOC_UPDATE(osc_en)				\
	g_drecip_osc_en_plus_2 = 1.0 / (double)(osc_en + 2);

#define SND_PTR_SHIFT		14
#define SND_PTR_SHIFT_DBL	((double)(1 << SND_PTR_SHIFT))

void
doc_init()
{
	Doc_reg	*rptr;
	int	i;

	for(i = 0; i < 32; i++) {
		rptr = &(g_doc_regs[i]);
		rptr->dsamp_ev = 0.0;
		rptr->dsamp_ev2 = 0.0;
		rptr->complete_dsamp = 0.0;
		rptr->samps_left = 0;
		rptr->cur_acc = 0;
		rptr->cur_inc = 0;
		rptr->cur_start = 0;
		rptr->cur_end = 0;
		rptr->cur_mask = 0;
		rptr->size_bytes = 0;
		rptr->event = 0;
		rptr->running = 0;
		rptr->has_irq_pending = 0;
		rptr->freq = 0;
		rptr->vol = 0;
		rptr->waveptr = 0;
		rptr->ctl = 1;
		rptr->wavesize = 0;
		rptr->last_samp_val = 0;
	}
}
void
doc_reset(dword64 dfcyc)
{
	int	i;

	for(i = 0; i < 32; i++) {
		doc_write_ctl_reg(dfcyc, i, g_doc_regs[i].ctl | 1);
		doc_reg_e0 = 0xff;
		if(g_doc_regs[i].has_irq_pending) {
			halt_printf("reset: has_irq[%02x] = %d\n", i,
				g_doc_regs[i].has_irq_pending);
		}
		g_doc_regs[i].has_irq_pending = 0;
	}
	if(g_num_osc_interrupting) {
		halt_printf("reset: num_osc_int:%d\n", g_num_osc_interrupting);
	}
	g_num_osc_interrupting = 0;

	g_doc_num_osc_en = 1;
	UPDATE_G_DCYCS_PER_DOC_UPDATE(1);
}

int
doc_play(dword64 dfcyc, double last_dsamp, double dsamp_now, int num_samps,
		int snd_buf_init, int *outptr_start)
{
	Doc_reg *rptr;
	int	*outptr;
	double	complete_dsamp, cur_dsamp;
	word32	cur_acc, cur_pos, cur_mask, cur_inc, cur_end;
	int	val, val2, imul, off, ctl, num_osc_en;
	int	samps_left, samps_to_do, samp_offset, pos, osc, done;
	int	i, j;

	num_osc_en = g_doc_num_osc_en;

	dbg_log_info(dfcyc, num_samps, 0, 0xd0c0);

	done = 0;
	// Do Ensoniq oscillators
	while(!done) {
		done = 1;
		for(j = 0; j < num_osc_en; j++) {
			osc = j;
			rptr = &(g_doc_regs[osc]);
			complete_dsamp = rptr->complete_dsamp;
			samps_left = rptr->samps_left;
			cur_acc = rptr->cur_acc;
			cur_mask = rptr->cur_mask;
			cur_inc = rptr->cur_inc;
			cur_end = rptr->cur_end;
			if(!rptr->running || cur_inc == 0 ||
						(complete_dsamp >= dsamp_now)) {
				continue;
			}

			done = 0;
			ctl = rptr->ctl;

			samp_offset = 0;
			if(complete_dsamp > last_dsamp) {
				samp_offset = (int)(complete_dsamp- last_dsamp);
				if(samp_offset > num_samps) {
					rptr->complete_dsamp = dsamp_now;
					continue;
				}
			}
			outptr = outptr_start + 2 * samp_offset;
			if(ctl & 0x10) {
				/* other channel */
				outptr += 1;
			}

			imul = (rptr->vol * g_doc_vol);
			off = imul * 128;

			samps_to_do = MY_MIN(samps_left,
						num_samps - samp_offset);
			if(imul == 0 || samps_to_do == 0) {
				/* produce no sound */
				samps_left = samps_left - samps_to_do;
				cur_acc += cur_inc * samps_to_do;
				rptr->samps_left = samps_left;
				rptr->cur_acc = cur_acc;
				cur_dsamp = last_dsamp +
					(double)(samps_to_do + samp_offset);
				DOC_LOG("nosnd", osc, cur_dsamp, samps_to_do);
				rptr->complete_dsamp = dsamp_now;
				cur_pos = rptr->cur_start+(cur_acc & cur_mask);
				if(samps_left <= 0) {
					doc_sound_end(osc, 1, cur_dsamp,
								dsamp_now);
					val = 0;
					j--;
				} else {
					val = doc_ram[cur_pos >> SND_PTR_SHIFT];
				}
				rptr->last_samp_val = val;
				continue;
			}
			if(snd_buf_init == 0) {
				memset(outptr_start, 0,
					2*sizeof(outptr_start[0])*num_samps);
				snd_buf_init++;
			}
			val = 0;
			rptr->complete_dsamp = dsamp_now;
			cur_pos = rptr->cur_start + (cur_acc & cur_mask);
			pos = 0;
			for(i = 0; i < samps_to_do; i++) {
				pos = cur_pos >> SND_PTR_SHIFT;
				cur_pos += cur_inc;
				cur_acc += cur_inc;
				val = doc_ram[pos];
				val2 = (val * imul - off) >> 4;
				if((val == 0) || (cur_pos >= cur_end)) {
					cur_dsamp = last_dsamp +
						(double)(samp_offset + i + 1);
					rptr->cur_acc = cur_acc;
					rptr->samps_left = 0;
					DOC_LOG("end or 0", osc, cur_dsamp,
						((pos & 0xffffU) << 16) |
						((i &0xff) << 8) | val);
					doc_sound_end(osc, val, cur_dsamp,
								dsamp_now);
					val = 0;
					break;
				}
				val2 = outptr[0] + val2;
				samps_left--;
				*outptr = val2;
				outptr += 2;
			}
			rptr->last_samp_val = val;

			if(val != 0) {
				rptr->cur_acc = cur_acc;
				rptr->samps_left = samps_left;
				rptr->complete_dsamp = dsamp_now;
			}
			DOC_LOG("splayed", osc, dsamp_now,
				(samps_to_do << 16) + (pos & 0xffff));
		}
	}

	return snd_buf_init;
}
void
doc_handle_event(int osc, dword64 dfcyc)
{
	/* handle osc stopping and maybe interrupting */

	//DOC_LOG("doc_ev", osc, dsamps, 0);

	g_doc_regs[osc].event = 0;

	sound_play(dfcyc);
}

void
doc_sound_end(int osc, int can_repeat, double eff_dsamps, double dsamps)
{
	Doc_reg	*rptr, *orptr;
	int	mode, omode;
	int	other_osc;
	int	one_shot_stop;
	int	ctl;

	/* handle osc stopping and maybe interrupting */

	if(osc < 0 || osc > 31) {
		printf("doc_handle_event: osc: %d!\n", osc);
		return;
	}

	rptr = &(g_doc_regs[osc]);
	ctl = rptr->ctl;

	if(rptr->event) {
		remove_event_doc(osc);
	}
	rptr->event = 0;
	rptr->cur_acc = 0;		/* reset internal accumulator*/

	/* check to make sure osc is running */
	if(ctl & 0x01) {
		/* Oscillator already stopped. */
		halt_printf("Osc %d interrupt, but it was already stop!\n",osc);
#ifdef HPUX
		U_STACK_TRACE();
#endif
		return;
	}

	if(ctl & 0x08) {
		if(rptr->has_irq_pending == 0) {
			doc_add_sound_irq(osc);
		}
	}

	if(!rptr->running) {
		halt_printf("Doc event for osc %d, but ! running\n", osc);
	}

	rptr->running = 0;

	mode = (ctl >> 1) & 3;
	other_osc = osc ^ 1;
	orptr = &(g_doc_regs[other_osc]);
	omode = (orptr->ctl >> 1) & 3;

	/* If either this osc or it's partner is in swap mode, treat the */
	/*  pair as being in swap mode.  This Ensoniq feature pointed out */
	/*  by Ian Schmidt */
	if(mode == 0 && can_repeat) {
		/* free-running mode with no 0 byte! */
		/* start doing it again */

		doc_start_sound(osc, eff_dsamps, dsamps);

		return;
	} else if((mode == 3) || (omode == 3)) {
		/* swap mode (even if we're one_shot and partner is swap)! */
		/* unless we're one-shot and we hit a 0-byte--then */
		/* Olivier Goguel says just stop */
		rptr->ctl |= 1;
		one_shot_stop = (mode == 1) && (!can_repeat);
		if(!one_shot_stop && !orptr->running &&
							(orptr->ctl & 0x1)) {
			orptr->ctl = orptr->ctl & (~1);
			doc_start_sound(other_osc, eff_dsamps, dsamps);
		}
		return;
	} else {
		/* stop the oscillator */
		rptr->ctl |= 1;
	}

	return;
}

void
doc_add_sound_irq(int osc)
{
	int	num_osc_interrupting;

	if(g_doc_regs[osc].has_irq_pending) {
		halt_printf("Adding sound_irq for %02x, but irq_p: %d\n", osc,
			g_doc_regs[osc].has_irq_pending);
	}

	num_osc_interrupting = g_num_osc_interrupting + 1;
	g_doc_regs[osc].has_irq_pending = num_osc_interrupting;
	g_num_osc_interrupting = num_osc_interrupting;

	add_irq(IRQ_PENDING_DOC);
	if(num_osc_interrupting == 1) {
		doc_reg_e0 = 0x00 + (osc << 1);
	}

	DOC_LOG("add_irq", osc, g_cur_dfcyc * g_dsamps_per_dfcyc, 0);
}

void
doc_remove_sound_irq(int osc, int must)
{
	Doc_reg	*rptr;
	int	num_osc_interrupt;
	int	has_irq_pending;
	int	first;
	int	i;

	doc_printf("remove irq for osc: %d, has_irq: %d\n",
		osc, g_doc_regs[osc].has_irq_pending);

	num_osc_interrupt = g_doc_regs[osc].has_irq_pending;
	first = 0;
	if(num_osc_interrupt) {
		g_num_osc_interrupting--;
		g_doc_regs[osc].has_irq_pending = 0;
		DOC_LOG("rem_irq", osc, g_cur_dfcyc * g_dsamps_per_dfcyc, 0);
		if(g_num_osc_interrupting == 0) {
			remove_irq(IRQ_PENDING_DOC);
		}

		first = 0x40 | (doc_reg_e0 >> 1);
					/* if none found, then def = no ints */
		for(i = 0; i < g_doc_num_osc_en; i++) {
			rptr = &(g_doc_regs[i]);
			has_irq_pending = rptr->has_irq_pending;
			if(has_irq_pending > num_osc_interrupt) {
				has_irq_pending--;
				rptr->has_irq_pending = has_irq_pending;
			}
			if(has_irq_pending == 1) {
				first = i;
			}
		}
		if(num_osc_interrupt == 1) {
			doc_reg_e0 = (first << 1);
		} else {
#if 0
			halt_printf("remove_sound_irq[%02x]=%d, first:%d\n",
				osc, num_osc_interrupt, first);
#endif
		}
	} else {
#if 0
		/* make sure no int pending */
		if(doc_reg_e0 != 0xff) {
			halt_printf("remove_sound_irq[%02x]=0, but e0: %02x\n",
				osc, doc_reg_e0);
		}
#endif
		if(must) {
			halt_printf("REMOVE_sound_irq[%02x]=0, but e0: %02x\n",
				osc, doc_reg_e0);
		}
	}

	if(doc_reg_e0 & 0x80) {
		for(i = 0; i < 0x20; i++) {
			has_irq_pending = g_doc_regs[i].has_irq_pending;
			if(has_irq_pending) {
				halt_printf("remove_sound_irq[%02x], but "
					"[%02x]=%d!\n", osc,i,has_irq_pending);
				printf("num_osc_int: %d, first: %02x\n",
					num_osc_interrupt, first);
			}
		}
	}
}

void
doc_start_sound2(int osc, dword64 dfcyc)
{
	double	dsamps;

	dsamps = dfcyc * g_dsamps_per_dfcyc;
	doc_start_sound(osc, dsamps, dsamps);
}

void
doc_start_sound(int osc, double eff_dsamps, double dsamps)
{
	Doc_reg	*rptr;
	int	ctl;
	int	mode;
	word32	sz;
	word32	size;
	word32	wave_size;

	if(osc < 0 || osc > 31) {
		halt_printf("start_sound: osc: %02x!\n", osc);
	}

	rptr = &(g_doc_regs[osc]);

	if(osc >= g_doc_num_osc_en) {
		rptr->ctl |= 1;
		return;
	}

	ctl = rptr->ctl;
	mode = (ctl >> 1) & 3;
	wave_size = rptr->wavesize;
	sz = ((wave_size >> 3) & 7) + 8;
	size = 1 << sz;

	if(size < 0x100) {
		halt_printf("size: %08x is too small, sz: %08x!\n", size, sz);
	}

	if(rptr->running) {
		halt_printf("start_sound osc: %d, already running!\n", osc);
	}

	rptr->running = 1;

	rptr->complete_dsamp = eff_dsamps;

	doc_printf("Starting osc %02x, dsamp: %f\n", osc, dsamps);
	doc_printf("size: %04x\n", size);

	if((mode == 2) && ((osc & 1) == 0)) {
		printf("Sync mode osc %d starting!\n", osc);
		/* set_halt(1); */

		/* see if we should start our odd partner */
		if((rptr[1].ctl & 7) == 5) {
			/* odd partner stopped in sync mode--start him */
			rptr[1].ctl &= (~1);
			doc_start_sound(osc + 1, eff_dsamps, dsamps);
		} else {
			printf("Osc %d starting sync, but osc %d ctl: %02x\n",
				osc, osc+1, rptr[1].ctl);
		}
	}

	doc_wave_end_estimate(osc, eff_dsamps, dsamps);

	DOC_LOG("st playing", osc, eff_dsamps, size);
#if 0
	if(rptr->cur_acc != 0) {
		halt_printf("Start osc %02x, acc: %08x\n", osc, rptr->cur_acc);
	}
#endif
}

void
doc_wave_end_estimate2(int osc, dword64 dfcyc)
{
	double	dsamps;

	dsamps = dfcyc * g_dsamps_per_dfcyc;
	doc_wave_end_estimate(osc, dsamps, dsamps);
}

void
doc_wave_end_estimate(int osc, double eff_dsamps, double dsamps)
{
	Doc_reg *rptr;
	byte	*ptr1;
	dword64	event_dfcyc;
	double	event_dsamp, dfcycs_per_samp, dsamps_per_byte, num_dsamps;
	double	dcur_inc;
	word32	tmp1, cur_inc, save_val;
	int	save_size, pos, size, estimate;

	dfcycs_per_samp = g_fcyc_per_samp;

	rptr = &(g_doc_regs[osc]);

	cur_inc = rptr->cur_inc;
	dcur_inc = (double)cur_inc;
	dsamps_per_byte = 0.0;
	if(cur_inc) {
		dsamps_per_byte = SND_PTR_SHIFT_DBL / (double)dcur_inc;
	}

	/* see if there's a zero byte */
	tmp1 = rptr->cur_start + (rptr->cur_acc & rptr->cur_mask);
	pos = tmp1 >> SND_PTR_SHIFT;
	size = ((rptr->cur_end) >> SND_PTR_SHIFT) - pos;

	ptr1 = &doc_ram[pos];

	estimate = 0;
	if(rptr->ctl & 0x08 || g_doc_regs[osc ^ 1].ctl & 0x08) {
		estimate = 1;
	}

#if 0
	estimate = 1;
#endif
	if(estimate) {
		save_size = size;
		save_val = ptr1[size];
		ptr1[size] = 0;
		size = (int)strlen((char *)ptr1);
		ptr1[save_size] = save_val;
	}

	/* calc samples to play */
	num_dsamps = (dsamps_per_byte * (double)size) + 1.0;

	rptr->samps_left = (int)num_dsamps;

	if(rptr->event) {
		remove_event_doc(osc);
	}
	rptr->event = 0;

	event_dsamp = eff_dsamps + num_dsamps;
	if(estimate) {
		rptr->event = 1;
		rptr->dsamp_ev = event_dsamp;
		rptr->dsamp_ev2 = dsamps;
		event_dfcyc = (dword64)(event_dsamp * dfcycs_per_samp) +
								65536LL;
		add_event_doc(event_dfcyc, osc);
	}
}

void
doc_remove_sound_event(int osc)
{
	if(g_doc_regs[osc].event) {
		g_doc_regs[osc].event = 0;
		remove_event_doc(osc);
	}
}


void
doc_write_ctl_reg(dword64 dfcyc, int osc, int val)
{
	Doc_reg *rptr;
	word32	old_halt, new_halt;
	int	old_val;

	if(osc < 0 || osc >= 0x20) {
		halt_printf("doc_write_ctl_reg: osc: %02x, val: %02x\n",
			osc, val);
		return;
	}

	rptr = &(g_doc_regs[osc]);
	old_val = rptr->ctl;
	g_doc_saved_ctl = old_val;

	if(old_val == val) {
		return;
	}

	//DOC_LOG("ctl_reg", osc, dsamps, (old_val << 16) + val);

	old_halt = (old_val & 1);
	new_halt = (val & 1);

	/* bits are:	28: old int bit */
	/*		29: old halt bit */
	/*		30: new int bit */
	/*		31: new halt bit */

#if 0
	if(osc == 0x10) {
		printf("osc %d new_ctl: %02x, old: %02x\n", osc, val, old_val);
	}
#endif

	/* no matter what, remove any pending IRQs on this osc */
	doc_remove_sound_irq(osc, 0);

#if 0
	if(old_halt) {
		printf("doc_write_ctl to osc %d, val: %02x, old: %02x\n",
			osc, val, old_val);
	}
#endif

	if(new_halt != 0) {
		/* make sure sound is stopped */
		doc_remove_sound_event(osc);
		if(old_halt == 0) {
			/* it was playing, finish it up */
#if 0
			halt_printf("Aborted osc %d at eff_dsamps: %f, ctl: "
				"%02x, oldctl: %02x\n", osc, eff_dsamps,
				val, old_val);
#endif
			sound_play(dfcyc);
		}
		if(((old_val >> 1) & 3) > 0) {
			/* don't clear acc if free-running */
			g_doc_regs[osc].cur_acc = 0;
		}

		g_doc_regs[osc].ctl = val;
		g_doc_regs[osc].running = 0;
	} else {
		/* new halt == 0 = make sure sound is running */
		if(old_halt != 0) {
			/* start sound */
			//DOC_LOG("ctl_sound_play", osc, eff_dsamps, val);
			sound_play(dfcyc);
			g_doc_regs[osc].ctl = val;

			doc_start_sound2(osc, dfcyc);
		} else {
			/* was running, and something changed */
			doc_printf("osc %d old ctl:%02x new:%02x!\n",
				osc, old_val, val);
			sound_play(dfcyc);
			g_doc_regs[osc].ctl = val;
			if((old_val ^ val) & val & 0x8) {
				/* now has ints on */
				doc_wave_end_estimate2(osc, dfcyc);
			}
		}
	}
}

void
doc_recalc_sound_parms(dword64 dfcyc, int osc)
{
	Doc_reg	*rptr;
	double	dfreq, dtmp1, dacc, dacc_recip;
	word32	res, sz, size, wave_size, cur_start, shifted_size;

	rptr = &(g_doc_regs[osc]);

	wave_size = rptr->wavesize;

	dfreq = (double)rptr->freq;

	sz = ((wave_size >> 3) & 7) + 8;
	size = 1 << sz;
	rptr->size_bytes = size;
	res = wave_size & 7;

	shifted_size = size << SND_PTR_SHIFT;
	cur_start = (rptr->waveptr << (8 + SND_PTR_SHIFT)) & (0-shifted_size);

	dtmp1 = dfreq * (DOC_SCAN_RATE * g_drecip_audio_rate);
	dacc = (double)(1 << (20 - (17 - sz + res)));
	dacc_recip = (SND_PTR_SHIFT_DBL) / ((double)(1 << 20));
	dtmp1 = dtmp1 * g_drecip_osc_en_plus_2 * dacc * dacc_recip;

	rptr->cur_inc = (int)(dtmp1);
	rptr->cur_start = cur_start;
	rptr->cur_end = cur_start + shifted_size;
	rptr->cur_mask = (shifted_size - 1);

	dbg_log_info(dfcyc, (rptr->waveptr << 16) + wave_size, osc, 0xd0cf);
}

int
doc_read_c03c()
{
	return g_doc_sound_ctl;
}

int
doc_read_c03d(dword64 dfcyc)
{
	Doc_reg	*rptr;
	int	osc, type, ret;

	ret = g_doc_saved_val;

	if(g_doc_sound_ctl & 0x40) {
		/* Read RAM */
		g_doc_saved_val = doc_ram[g_c03ef_doc_ptr];
	} else {
		/* Read DOC */
		g_doc_saved_val = 0;

		osc = g_c03ef_doc_ptr & 0x1f;
		type = (g_c03ef_doc_ptr >> 5) & 0x7;
		rptr = &(g_doc_regs[osc]);

		switch(type) {
		case 0x0:	/* freq lo */
			g_doc_saved_val = rptr->freq & 0xff;
			break;
		case 0x1:	/* freq hi */
			g_doc_saved_val = rptr->freq >> 8;
			break;
		case 0x2:	/* vol */
			g_doc_saved_val = rptr->vol;
			break;
		case 0x3:	/* data register */
			sound_play(dfcyc);		// Fix for Paperboy GS
			g_doc_saved_val = rptr->last_samp_val;
			break;
		case 0x4:	/* wave ptr register */
			g_doc_saved_val = rptr->waveptr;
			break;
		case 0x5:	/* control register */
			g_doc_saved_val = rptr->ctl;
			break;
		case 0x6:	/* control register */
			g_doc_saved_val = rptr->wavesize;
			break;
		case 0x7:	/* 0xe0-0xff */
			switch(osc) {
			case 0x00:	/* 0xe0 */
				g_doc_saved_val = doc_reg_e0;
				doc_printf("Reading doc 0xe0, ret: %02x\n",
							g_doc_saved_val);

				/* Clear IRQ on read of e0, if any irq pend */
				if((doc_reg_e0 & 0x80) == 0) {
					doc_remove_sound_irq(doc_reg_e0 >> 1,
									1);
				}
				break;
			case 0x01:	/* 0xe1 */
				g_doc_saved_val = (g_doc_num_osc_en - 1) << 1;
				break;
			case 0x02:	/* 0xe2 */
				g_doc_saved_val = 0x80;
#if 0
				halt_printf("Reading doc 0xe2, ret: %02x\n",
							g_doc_saved_val);
#endif
				break;
			default:
				g_doc_saved_val = 0;
				halt_printf("Reading bad doc_reg[%04x]: %02x\n",
					g_c03ef_doc_ptr, g_doc_saved_val);
			}
			break;
		default:
			g_doc_saved_val = 0;
			halt_printf("Reading bad doc_reg[%04x]: %02x\n",
					g_c03ef_doc_ptr, g_doc_saved_val);
		}
	}

	doc_printf("read c03d, doc_ptr: %04x, ret: %02x, saved: %02x\n",
		g_c03ef_doc_ptr, ret, g_doc_saved_val);

	//DOC_LOG("read c03d", -1, dsamps, (g_c03ef_doc_ptr << 16) +
	//		(g_doc_saved_val << 8) + ret);

	if(g_doc_sound_ctl & 0x20) {
		g_c03ef_doc_ptr = (g_c03ef_doc_ptr + 1) & 0xffff;
	}


	return ret;
}

void
doc_write_c03c(dword64 dfcyc, word32 val)
{
	int	vol;

	vol = val & 0xf;
	dbg_log_info(dfcyc, val, g_doc_vol, 0xc03c);
	if(g_doc_vol != vol) {
		sound_play(dfcyc);

		g_doc_vol = vol;
		doc_printf("Setting doc vol to 0x%x at %016llx\n", vol, dfcyc);
	}

	g_doc_sound_ctl = val;
}

void
doc_write_c03d(dword64 dfcyc, word32 val)
{
	Doc_reg	*rptr;
	int	osc, type, ctl, tmp;
	int	i;

	val = val & 0xff;

	doc_printf("write c03d, doc_ptr: %04x, val: %02x\n",
		g_c03ef_doc_ptr, val);

	dbg_log_info(dfcyc, g_c03ef_doc_ptr, val, 0xc03d);

	if(g_doc_sound_ctl & 0x40) {
		/* RAM */
		doc_ram[g_c03ef_doc_ptr] = val;
	} else {
		/* DOC */
		osc = g_c03ef_doc_ptr & 0x1f;
		type = (g_c03ef_doc_ptr >> 5) & 0x7;
		rptr = &(g_doc_regs[osc]);
		ctl = rptr->ctl;
#if 0
		if((ctl & 1) == 0) {
			if(type < 2 || type == 4 || type == 6) {
				halt_printf("Osc %d is running, old ctl: %02x, "
					"but write reg %02x=%02x\n",
					osc, ctl, g_c03ef_doc_ptr & 0xff, val);
			}
		}
#endif

		switch(type) {
		case 0x0:	/* freq lo */
			if((rptr->freq & 0xff) == (word32)val) {
				break;
			}
			if((ctl & 1) == 0) {
				/* play through current status */
				//DOC_LOG("flo_sound_play", osc, dsamps, val);
				sound_play(dfcyc);
			}
			rptr->freq = (rptr->freq & 0xff00) + val;
			doc_recalc_sound_parms(dfcyc, osc);
			break;
		case 0x1:	/* freq hi */
			if((rptr->freq >> 8) == (word32)val) {
				break;
			}
			if((ctl & 1) == 0) {
				/* play through current status */
				//DOC_LOG("fhi_sound_play", osc, dsamps, val);
				sound_play(dfcyc);
			}
			rptr->freq = (rptr->freq & 0xff) + (val << 8);
			doc_recalc_sound_parms(dfcyc, osc);
			break;
		case 0x2:	/* vol */
			if(rptr->vol == (word32)val) {
				break;
			}
			if((ctl & 1) == 0) {
				/* play through current status */
				//DOC_LOG("vol_sound_play", osc, dsamps, val);
				sound_play(dfcyc);
			}
			rptr->vol = val;
			break;
		case 0x3:	/* data register */
#if 0
			printf("Writing %02x into doc_data_reg[%02x]!\n",
				val, osc);
#endif
			break;
		case 0x4:	/* wave ptr register */
			if(rptr->waveptr == (word32)val) {
				break;
			}
			if((ctl & 1) == 0) {
				/* play through current status */
				//DOC_LOG("wptr_sound_play", osc, dsamps, val);
				sound_play(dfcyc);
			}
			rptr->waveptr = val;
			doc_recalc_sound_parms(dfcyc, osc);
			break;
		case 0x5:	/* control register */
#if 0
			printf("doc_write ctl osc %d, val: %02x\n", osc, val);
#endif
			if(rptr->ctl == (word32)val) {
				break;
			}
			doc_write_ctl_reg(dfcyc, osc, val);
			break;
		case 0x6:	/* wavesize register */
			if(rptr->wavesize == (word32)val) {
				break;
			}
			if((ctl & 1) == 0) {
				/* play through current status */
				//DOC_LOG("wsz_sound_play", osc, dsamps, val);
				sound_play(dfcyc);
			}
			rptr->wavesize = val;
			doc_recalc_sound_parms(dfcyc, osc);
			break;
		case 0x7:	/* 0xe0-0xff */
			switch(osc) {
			case 0x00:	/* 0xe0 */
				doc_printf("writing doc 0xe0 with %02x, "
					"was:%02x\n", val, doc_reg_e0);
#if 0
				if(val != doc_reg_e0) {
					halt_printf("writing doc 0xe0 with "
						"%02x, was:%02x\n", val,
						doc_reg_e0);
				}
#endif
				break;
			case 0x01:	/* 0xe1 */
				doc_printf("Writing doc 0xe1 with %02x\n", val);
				tmp = val & 0x3e;
				tmp = (tmp >> 1) + 1;
				if(tmp < 1) {
					tmp = 1;
				}
				if(tmp > 32) {
					halt_printf("doc 0xe1: %02x!\n", val);
					tmp = 32;
				}
				g_doc_num_osc_en = tmp;
				UPDATE_G_DCYCS_PER_DOC_UPDATE(tmp);

				/* Stop any oscs that were running but now */
				/*  are disabled */
				for(i = g_doc_num_osc_en; i < 0x20; i++) {
					doc_write_ctl_reg(dfcyc, i,
							g_doc_regs[i].ctl | 1);
				}

				break;
			default:
				/* this should be illegal, but Turkey Shoot */
				/* and apparently TaskForce, OOTW, etc */
				/*  writes to e2-ff, for no apparent reason */
				doc_printf("Writing doc 0x%x with %02x\n",
						g_c03ef_doc_ptr, val);
				break;
			}
			break;
		default:
			halt_printf("Writing %02x into bad doc_reg[%04x]\n",
				val, g_c03ef_doc_ptr);
		}
	}

	if(g_doc_sound_ctl & 0x20) {
		g_c03ef_doc_ptr = (g_c03ef_doc_ptr + 1) & 0xffff;
	}

	g_doc_saved_val = val;
}

void
doc_show_ensoniq_state()
{
	Doc_reg	*rptr;
	int	i;

	printf("Ensoniq state\n");
	printf("c03c doc_sound_ctl: %02x, doc_saved_val: %02x\n",
		g_doc_sound_ctl, g_doc_saved_val);
	printf("doc_ptr: %04x,    num_osc_en: %02x, e0: %02x\n",
		g_c03ef_doc_ptr, g_doc_num_osc_en, doc_reg_e0);

	for(i = 0; i < 32; i += 8) {
		printf("irqp: %02x: %04x %04x %04x %04x %04x %04x %04x %04x\n",
			i,
			g_doc_regs[i].has_irq_pending,
			g_doc_regs[i + 1].has_irq_pending,
			g_doc_regs[i + 2].has_irq_pending,
			g_doc_regs[i + 3].has_irq_pending,
			g_doc_regs[i + 4].has_irq_pending,
			g_doc_regs[i + 5].has_irq_pending,
			g_doc_regs[i + 6].has_irq_pending,
			g_doc_regs[i + 7].has_irq_pending);
	}

	for(i = 0; i < 32; i += 8) {
		printf("freq: %02x: %04x %04x %04x %04x %04x %04x %04x %04x\n",
			i,
			g_doc_regs[i].freq, g_doc_regs[i + 1].freq,
			g_doc_regs[i + 2].freq, g_doc_regs[i + 3].freq,
			g_doc_regs[i + 4].freq, g_doc_regs[i + 5].freq,
			g_doc_regs[i + 6].freq, g_doc_regs[i + 7].freq);
	}

	for(i = 0; i < 32; i += 8) {
		printf("vol: %02x: %02x %02x %02x %02x %02x %02x %02x %02x\n",
			i,
			g_doc_regs[i].vol, g_doc_regs[i + 1].vol,
			g_doc_regs[i + 2].vol, g_doc_regs[i + 3].vol,
			g_doc_regs[i + 4].vol, g_doc_regs[i + 5].vol,
			g_doc_regs[i + 6].vol, g_doc_regs[i + 6].vol);
	}

	for(i = 0; i < 32; i += 8) {
		printf("wptr: %02x: %02x %02x %02x %02x %02x %02x %02x %02x\n",
			i,
			g_doc_regs[i].waveptr, g_doc_regs[i + 1].waveptr,
			g_doc_regs[i + 2].waveptr, g_doc_regs[i + 3].waveptr,
			g_doc_regs[i + 4].waveptr, g_doc_regs[i + 5].waveptr,
			g_doc_regs[i + 6].waveptr, g_doc_regs[i + 7].waveptr);
	}

	for(i = 0; i < 32; i += 8) {
		printf("ctl: %02x: %02x %02x %02x %02x %02x %02x %02x %02x\n",
			i,
			g_doc_regs[i].ctl, g_doc_regs[i + 1].ctl,
			g_doc_regs[i + 2].ctl, g_doc_regs[i + 3].ctl,
			g_doc_regs[i + 4].ctl, g_doc_regs[i + 5].ctl,
			g_doc_regs[i + 6].ctl, g_doc_regs[i + 7].ctl);
	}

	for(i = 0; i < 32; i += 8) {
		printf("wsize: %02x: %02x %02x %02x %02x %02x %02x %02x %02x\n",
			i,
			g_doc_regs[i].wavesize, g_doc_regs[i + 1].wavesize,
			g_doc_regs[i + 2].wavesize, g_doc_regs[i + 3].wavesize,
			g_doc_regs[i + 4].wavesize, g_doc_regs[i + 5].wavesize,
			g_doc_regs[i + 6].wavesize, g_doc_regs[i + 7].wavesize);
	}

	for(i = 0; i < 32; i++) {
		rptr = &(g_doc_regs[i]);
		printf("%2d: ctl:%02x wp:%02x ws:%02x freq:%04x vol:%02x "
			"ev:%d run:%d irq:%d sz:%04x\n", i,
			rptr->ctl, rptr->waveptr, rptr->wavesize, rptr->freq,
			rptr->vol, rptr->event, rptr->running,
			rptr->has_irq_pending, rptr->size_bytes);
		printf("    acc:%08x inc:%08x st:%08x end:%08x m:%08x\n",
			rptr->cur_acc, rptr->cur_inc, rptr->cur_start,
			rptr->cur_end, rptr->cur_mask);
		printf("    compl_ds:%f samps_left:%d ev:%f ev2:%f\n",
			rptr->complete_dsamp, rptr->samps_left,
			rptr->dsamp_ev, rptr->dsamp_ev2);
	}

#if 0
	for(osc = 0; osc < 32; osc++) {
		fmax = 0.0;
		printf("osc %d has %d samps\n", osc, g_fsamp_num[osc]);
		for(i = 0; i < g_fsamp_num[osc]; i++) {
			printf("%4d: %f\n", i, g_fsamps[osc][i]);
			fmax = MY_MAX(fmax, g_fsamps[osc][i]);
		}
		printf("osc %d, fmax: %f\n", osc, fmax);
	}
#endif
}
