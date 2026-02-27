const char rcsid_iwm_c[] = "@(#)$KmKId: iwm.c,v 1.207 2023-09-26 02:59:52+00 kentd Exp $";

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

// Information from Beneath Apple DOS, Apple IIgs HW reference, Apple IIgs
//  Firmware reference, and Inside Macintosh Vol 3 (for status35 info).
// Neil Parker wrote about the Apple 3.5 drive using IWM at
//  http://nparker.llx.com/a2/disk and it is very useful.
//  http://nparker.llx.com/a2/sethook lists hooks for IWM routines, from
//   $e1/0c00 - 0fab.
// ff/5fa4 is STAT35.  ff/5fae is CONT35
// When testing DOS3.3, set bp at $b942, which is bad sector detected
// IWM mode register: 7:reserved; 6:5: 0; 4: 1=8MHz,0=7MHz (should always be 0);
//	3: bit-cells are 2usec, 2: 1sec timer off; 1: async handshake (writes)
//	0: latch mode enabled for reads (holds data for 8 bit times)
// dsk->fd >= 0 indicates a valid disk.  dsk->raw_data != 0 indicates this is
//  a compressed disk mounted read-only, and ignore dsk->fd (which will always
//  be 0, to indicate a valid disk).
// fbit_pos encodes head position on track in increments of 1/64 usecs for 5.25"
//  { byte_offset, bit[2:0], sub_bit[8:0] }, so fbit_pos >> 12 gives byte offset
// fbit_mult turns dfcyc into fbit_pos, where (512/fbit_mult) = the bit cell
//  5.25" bit cell of 4usec has fbit_mult=128; cell of 3.75usec has mult=136.
// For 3.5", fbit_mult=256 indicates a 2usec bit cell.
// https://support.apple.com/kb/TA39910?locale=en_US&viewlocale=en_US gives
//  the RPMs of 800KB disks

#include "defc.h"

int g_halt_arm_move = 0;

extern int Verbose;
extern word32 g_vbl_count;
extern word32 g_c036_val_speed;
extern int g_config_kegs_update_needed;
extern Engine_reg engine;

const byte phys_to_dos_sec[] = {
	0x00, 0x07, 0x0e, 0x06,  0x0d, 0x05, 0x0c, 0x04,
	0x0b, 0x03, 0x0a, 0x02,  0x09, 0x01, 0x08, 0x0f
};

const byte phys_to_prodos_sec[] = {
	0x00, 0x08, 0x01, 0x09,  0x02, 0x0a, 0x03, 0x0b,
	0x04, 0x0c, 0x05, 0x0d,  0x06, 0x0e, 0x07, 0x0f
};


const byte to_disk_byte[] = {
	0x96, 0x97, 0x9a, 0x9b,  0x9d, 0x9e, 0x9f, 0xa6,
	0xa7, 0xab, 0xac, 0xad,  0xae, 0xaf, 0xb2, 0xb3,
/* 0x10 */
	0xb4, 0xb5, 0xb6, 0xb7,  0xb9, 0xba, 0xbb, 0xbc,
	0xbd, 0xbe, 0xbf, 0xcb,  0xcd, 0xce, 0xcf, 0xd3,
/* 0x20 */
	0xd6, 0xd7, 0xd9, 0xda,  0xdb, 0xdc, 0xdd, 0xde,
	0xdf, 0xe5, 0xe6, 0xe7,  0xe9, 0xea, 0xeb, 0xec,
/* 0x30 */
	0xed, 0xee, 0xef, 0xf2,  0xf3, 0xf4, 0xf5, 0xf6,
	0xf7, 0xf9, 0xfa, 0xfb,  0xfc, 0xfd, 0xfe, 0xff
};

int	g_track_bytes_35[] = {
	0x200*12,
	0x200*11,
	0x200*10,
	0x200*9,
	0x200*8
};

int	g_track_bits_35[] = {
	// Do 1.001 * (1020484 * (60/rpm)) / 2 usec = bits per track
	77779,			// Trks 0-15: 394 rpm
	71433,			// Trks 16-31: 429 rpm
	64926,			// Trks 32-47: 472 rpm
	58371,			// Trks 48-63: 525 rpm
	51940			// Trks 64-79: 590 rpm
};

int	g_fast_disk_emul_en = 1;
int	g_fast_disk_emul = 0;
int	g_slow_525_emul_wr = 0;
dword64	g_dfcyc_end_emul_wr = 0;
int	g_fast_disk_unnib = 0;
int	g_iwm_fake_fast = 0;

word32	g_from_disk_byte[256];
int	g_from_disk_byte_valid = 0;

Iwm	g_iwm;

int	g_iwm_motor_on = 0;
// g_iwm_motor_on is set when drive turned on, 0 when $c0e8 is touched.
//  Use this to throttle speed to 1MHz.
// g_iwm.motor_on=1 means drive is spinning.  g_iwm.motor_off=1 means we're
//  in the 1-second countdown of g_iwm.motor_off_vbl_count.

int	g_check_nibblization = 1;

void
iwm_init_drive(Disk *dsk, int smartport, int drive, int disk_525)
{
	int	num_tracks;
	int	i;

	dsk->dfcyc_last_read = 0;
	dsk->raw_data = 0;
	dsk->wozinfo_ptr = 0;
	dsk->dynapro_info_ptr = 0;
	dsk->name_ptr = 0;
	dsk->partition_name = 0;
	dsk->partition_num = -1;
	dsk->fd = -1;
	dsk->dynapro_blocks = 0;
	dsk->raw_dsize = 0;
	dsk->dimage_start = 0;
	dsk->dimage_size = 0;
	dsk->smartport = smartport;
	dsk->disk_525 = disk_525;
	dsk->drive = drive;
	dsk->cur_frac_track = 0;
	dsk->image_type = 0;
	dsk->vol_num = 254;
	dsk->write_prot = 1;
	dsk->write_through_to_unix = 0;
	dsk->disk_dirty = 0;
	dsk->just_ejected = 0;
	dsk->last_phases = 0;
	dsk->cur_fbit_pos = 0;
	dsk->fbit_mult = 128;			// 128 for 5.25", 256 for 3.5"
	dsk->cur_track_bits = 0;
	dsk->raw_bptr_malloc = 0;
	dsk->cur_trk_ptr = 0;
	dsk->num_tracks = 0;
	dsk->trks = 0;
	if(!smartport) {
		// 3.5" and 5.25" drives.  This is only called at init time,
		//  so one-time malloc can be done now
		num_tracks = 2*80;		// 3.5" needs 160
		dsk->trks = (Trk *)malloc(num_tracks * sizeof(Trk));

		for(i = 0; i < num_tracks; i++) {
			dsk->trks[i].raw_bptr = 0;
			dsk->trks[i].sync_ptr = 0;
			dsk->trks[i].dunix_pos = 0;
			dsk->trks[i].unix_len = 0;
			dsk->trks[i].dirty = 0;
			dsk->trks[i].track_bits = 0;
		}
		// num_tracks != 0 indicates a valid disk, do not set it here
	}
}

void
iwm_init()
{
	word32	val;
	int	i;

	memset(&g_iwm, 0, sizeof(g_iwm));

	for(i = 0; i < 2; i++) {
		iwm_init_drive(&(g_iwm.drive525[i]), 0, i, 1);
		iwm_init_drive(&(g_iwm.drive35[i]), 0, i, 0);
	}

	for(i = 0; i < MAX_C7_DISKS; i++) {
		iwm_init_drive(&(g_iwm.smartport[i]), 1, i, 0);
	}

	if(g_from_disk_byte_valid == 0) {
		for(i = 0; i < 256; i++) {
			g_from_disk_byte[i] = 0x100 + i;
		}
		for(i = 0; i < 64; i++) {
			val = to_disk_byte[i];
			g_from_disk_byte[val] = i;
		}
		g_from_disk_byte_valid = 1;
	} else {
		halt_printf("iwm_init called twice!\n");
	}
}

void
iwm_reset()
{
	int	i;

	g_iwm.state = 0;
	g_iwm.motor_off_vbl_count = 0;
	g_iwm.forced_sync_bit = 32*12345;
	g_iwm.last_rd_bit = 32*12345;
	g_iwm.write_val = 0;
	for(i = 0; i < 5; i++) {
		g_iwm.wr_last_bit[i] = 0;
		g_iwm.wr_qtr_track[i] = 0;
		g_iwm.wr_num_bits[i] = 0;
		g_iwm.wr_prior_num_bits[i] = 0;
		g_iwm.wr_delta[i] = 0;
	}
	g_iwm.num_active_writes = 0;

	g_iwm_motor_on = 0;
}

void
disk_set_num_tracks(Disk *dsk, int num_tracks)
{
	if(num_tracks <= 2*80) {
		dsk->num_tracks = num_tracks;
	} else {
		halt_printf("num_tracks out of range: %d\n", num_tracks);
	}
}

word32
iwm_get_default_track_bits(Disk *dsk, word32 qtr_trk)
{
	word32	track_bits, extra;

	extra = (qtr_trk + (qtr_trk >> 5)) & 0xf;
		// up to 15 extra bits
	if(dsk->disk_525) {
		track_bits = NIB_LEN_525 * 8;		// 0x18f2 = 51088
	} else {
		track_bits = g_track_bits_35[qtr_trk >> 5];
	}
	return track_bits + extra;
}

void
draw_iwm_status(int line, char *buf)
{
	char	*flag[2][2];
	int	apple35_sel, drive_select;

	flag[0][0] = " ";
	flag[0][1] = " ";
	flag[1][0] = " ";
	flag[1][1] = " ";

	apple35_sel = (g_iwm.state >> IWM_BIT_C031_APPLE35SEL) & 1;
	drive_select = (g_iwm.state >> IWM_BIT_DRIVE_SEL) & 1;
	if(g_iwm.state & IWM_STATE_MOTOR_ON) {
		flag[apple35_sel][drive_select] = "*";
	}

	sprintf(buf, "s6d1:%2d%s   s6d2:%2d%s   s5d1:%2d/%d%s   "
		"s5d2:%2d/%d%s fast_disk_emul:%d,%d c036:%02x",
		g_iwm.drive525[0].cur_frac_track >> 18, flag[0][0],
		g_iwm.drive525[1].cur_frac_track >> 18, flag[0][1],
		g_iwm.drive35[0].cur_frac_track >> 17,
		(g_iwm.drive35[0].cur_frac_track >> 16) & 1, flag[1][0],
		g_iwm.drive35[1].cur_frac_track >> 17,
		(g_iwm.drive35[1].cur_frac_track >> 16) & 1, flag[1][1],
		g_fast_disk_emul, g_slow_525_emul_wr, g_c036_val_speed);

	video_update_status_line(line, buf);
}

void
iwm_flush_cur_disk()
{
	Disk	*dsk;

	dsk = iwm_get_dsk(g_iwm.state);
	iwm_flush_disk_to_unix(dsk);
}

void
iwm_flush_disk_to_unix(Disk *dsk)
{
	byte	buffer[0x4000];
	int	ret, did_write;
	int	i;

	if((dsk->disk_dirty == 0) || (dsk->write_through_to_unix == 0)) {
		return;
	}
	if((dsk->raw_data != 0) && !dsk->dynapro_info_ptr) {
		return;
	}

	printf("Writing disk %s to Unix\n", dsk->name_ptr);

	for(i = 0; i < 160; i++) {
		ret = iwm_track_to_unix(dsk, i, &(buffer[0]));

		if((ret != 1) && (ret != 0)) {
			printf("iwm_flush_disk_to_unix ret: %d, cannot write "
				"image to unix, qtrk:%04x\n", ret, i);
			halt_printf("Converting to WOZ format!\n");
			woz_reparse_woz(dsk);
			return;		// Don't clear dsk->disk_dirty
		}
		if(ret == 1) {
			did_write = 1;
		}
	}
	if(dsk->wozinfo_ptr && did_write) {
		woz_check_file(dsk);
	}

	dsk->disk_dirty = 0;
}

/* Check for dirty disk 3 times a second */

word32	g_iwm_dynapro_last_vbl_count = 0;

void
iwm_vbl_update()
{
	word32	state;
	int	i;

	state = g_iwm.state;
	if((state & IWM_STATE_MOTOR_ON) && (state & IWM_STATE_MOTOR_OFF)) {
		if(g_iwm.motor_off_vbl_count <= g_vbl_count) {
			printf("Disk timer expired, drive off: %08x\n",
				g_vbl_count);
			iwm_flush_cur_disk();
			g_iwm.state = state &
				(~(IWM_STATE_MOTOR_ON | IWM_STATE_MOTOR_OFF));
			g_iwm.drive525[0].dfcyc_last_phases = 0;
			g_iwm.drive525[1].dfcyc_last_phases = 0;
		}
	}
	if((g_vbl_count - g_iwm_dynapro_last_vbl_count) >= 60) {
		for(i = 0; i < 2; i++) {
			dynapro_try_fix_damaged_disk(&(g_iwm.drive525[i]));
			dynapro_try_fix_damaged_disk(&(g_iwm.drive35[i]));
		}
		for(i = 0; i < MAX_C7_DISKS; i++) {
			dynapro_try_fix_damaged_disk(&(g_iwm.smartport[i]));
		}
		g_iwm_dynapro_last_vbl_count = g_vbl_count;
	}
}

void
iwm_update_fast_disk_emul(int fast_disk_emul_en)
{
	if(g_iwm_motor_on == 0) {
		g_fast_disk_emul = fast_disk_emul_en;
	}
}

void
iwm_show_stats(int slot_drive)
{
	Trk	*trkptr;
	Disk	*dsk;
	int	slot, drive;
	int	i;

	dbg_printf("IWM state: %07x (slot_drive:%d)\n", g_iwm.state,
								slot_drive);
	dbg_printf("g_iwm.drive525[0].fd: %d, [1].fd: %d\n",
		g_iwm.drive525[0].fd, g_iwm.drive525[1].fd);
	dbg_printf("g_iwm.drive525[0].last_phases: %d, [1].last_phases: %d\n",
		g_iwm.drive525[0].last_phases, g_iwm.drive525[1].last_phases);
	dbg_printf("g_iwm.write_val:%02x, wr_num_bits[0]:%d, last_rd_bit:"
		"%06x, forced_sync_bit:%06x\n", g_iwm.write_val,
		g_iwm.wr_num_bits[0], g_iwm.last_rd_bit,
		g_iwm.forced_sync_bit);
	if(slot_drive < 0) {
		return;
	}
	drive = slot_drive & 1;
	slot = 5 + ((slot_drive >> 1) & 1);
	dsk = iwm_get_dsk_from_slot_drive(slot, drive);
	if(dsk->trks == 0) {
		return;
	}
	for(i = 0; i < 160; i++) {
		trkptr = &(dsk->trks[i]);
		printf("Qtrk:%02x: bits:%05x dunix_pos:%08llx unix_len:%06x, "
			"d:%d %p,%p\n", i, trkptr->track_bits,
			trkptr->dunix_pos, trkptr->unix_len, trkptr->dirty,
			trkptr->raw_bptr, trkptr->sync_ptr);
	}
}

Disk *
iwm_get_dsk(word32 state)
{
	Disk	*dsk;
	int	drive;

	drive = (state >> IWM_BIT_DRIVE_SEL) & 1;
	if(state & IWM_STATE_C031_APPLE35SEL) {
		dsk = &(g_iwm.drive35[drive]);
	} else {
		dsk = &(g_iwm.drive525[drive]);
	}

	return dsk;
}

Disk *
iwm_touch_switches(int loc, dword64 dfcyc)
{
	Disk	*dsk;
	dword64	dadd, dcycs_passed;
	word32	track_bits, fbit_pos, forced_sync_bit, mask, mask_on, state;
	int	phase, on, motor_on;

	if(g_iwm.state & IWM_STATE_RESET) {
		iwm_printf("IWM under reset: %06x\n", g_iwm.state);
	}

	dsk = iwm_get_dsk(g_iwm.state);

	track_bits = dsk->cur_track_bits;
	fbit_pos = track_bits * 4096;		// Set to an illegal value
	motor_on = g_iwm.state & IWM_STATE_MOTOR_ON;
	if(motor_on) {
		dcycs_passed = (dfcyc - dsk->dfcyc_last_read) >> 16;
		dsk->dfcyc_last_read = dfcyc;

		// track_bits can be 0, indicating no valid data
		dadd = dcycs_passed * dsk->fbit_mult;
		if(track_bits) {
			if(dadd >= (track_bits * 512)) {
				dadd = dadd % (track_bits * 512);
			}
		}
		fbit_pos = dsk->cur_fbit_pos + (word32)dadd;
		if(fbit_pos >= (track_bits * 512)) {
			fbit_pos -= (track_bits * 512);
		}
		if(g_slow_525_emul_wr || (g_iwm.state & IWM_STATE_Q7) ||
					!g_fast_disk_emul || !track_bits) {
			dsk->cur_fbit_pos = fbit_pos;
		} else {
			fbit_pos = track_bits << 12;	// out of range value
		}
#if 0
		printf("dsk->cur_fbit:%08x, fbit_pos:%08x\n",
					dsk->cur_fbit_pos, fbit_pos);
#endif
	}

	dbg_log_info(dfcyc, dsk->cur_fbit_pos,
		((loc & 0xff) << 24) | g_iwm.state,
		((dsk->cur_frac_track + 0x8000) & 0x1ff0000) | 0xe0);

	if(loc < 0) {
		// Not a real access, just a way to update fbit_pos
		return dsk;
	}

	if(dsk->dfcyc_last_phases) {
		iwm525_update_head(dsk, dfcyc);
	}

	on = loc & 1;
	phase = loc >> 1;
	state = g_iwm.state;
	if(loc < 8) {
		/* phase adjustments.  See if motor is on */

		mask = (1 << (phase & 3)) << IWM_BIT_PHASES;
		mask_on = (on << (phase & 3)) << IWM_BIT_PHASES;
		state = (state & (~mask)) | mask_on;
		g_iwm.state = state;
		iwm_printf("Iwm phase %d=%d, all phases: %06x (%016llx)\n",
			phase, on, state, dfcyc);

		if(state & IWM_STATE_MOTOR_ON) {
			if(state & IWM_STATE_C031_APPLE35SEL) {
				if((phase == 3) && on) {
					iwm_do_action35(dfcyc);
				}
			} else {
				// Move apple525 head
				iwm525_update_phases(dsk, dfcyc);
			}
		}
		state = g_iwm.state;
		/* See if enable or reset is asserted */
		if(((state >> IWM_BIT_PHASES) & 5) == 5) {	// Ph 0, 2 set
			state |= IWM_STATE_RESET;
			iwm_printf("IWM reset active\n");
		} else {
			state &= (~IWM_STATE_RESET);
		}
		if(((state >> IWM_BIT_PHASES) & 0xa) == 0xa) {	// Ph 1, 3 set
			state |= IWM_STATE_ENABLE2;
			iwm_printf("IWM ENABLE2 active\n");
		} else {
			state &= (~IWM_STATE_ENABLE2);
		}
		g_iwm.state = state;
	} else {
		/* loc >= 8 */
		switch(loc) {
		case 0x8:
			iwm_printf("Turning IWM motor off!\n");
			if(g_iwm.state & 0x04) {		// iwm MODE
				/* Turn off immediately */
				state &= (~(IWM_STATE_MOTOR_ON |
							IWM_STATE_MOTOR_OFF));
			} else {
				/* 1 second delay */
				if((state & IWM_STATE_MOTOR_ON) &&
						!(state & IWM_STATE_MOTOR_OFF)){
					state |= IWM_STATE_MOTOR_OFF;
					g_iwm.motor_off_vbl_count = g_vbl_count
									+ 60;
				}
			}
			g_iwm.state = state;

#if 0
			printf("IWM motor off, g_iwm_motor_on:%d, slow:%d\n",
				g_iwm_motor_on, g_slow_525_emul_wr);
#endif
			if(g_iwm_motor_on || g_slow_525_emul_wr) {
				/* recalc current speed */
				g_fast_disk_emul = g_fast_disk_emul_en;
				engine_recalc_events();
				iwm_flush_cur_disk();
			}

			g_iwm_motor_on = 0;
			g_slow_525_emul_wr = 0;
			break;
		case 0x9:
			iwm_printf("Turning IWM motor on!\n");
			state |= IWM_STATE_MOTOR_ON;
			state &= (~IWM_STATE_MOTOR_OFF);
			state &= (~IWM_STATE_LAST_SEL35);
			state |= ((state >> 6) & 1) << IWM_BIT_LAST_SEL35;
			g_iwm.state = state;
			dsk->dfcyc_last_read = dfcyc;

			if(g_iwm_motor_on == 0) {
				/* recalc current speed */
				if(dsk->wozinfo_ptr) {
					g_fast_disk_emul = 0;
				}
				engine_recalc_events();
			}
			g_iwm_motor_on = 1;

			break;
		case 0xa:
		case 0xb:
			if(((state >> IWM_BIT_DRIVE_SEL) & 1) != on) {
				iwm_flush_cur_disk();
			}
			state &= (~IWM_STATE_DRIVE_SEL);
			state |= (on << IWM_BIT_DRIVE_SEL);
			g_iwm.state = state;
			dsk = iwm_get_dsk(state);
			if(dsk->wozinfo_ptr && g_iwm_motor_on) {
				g_fast_disk_emul = 0;
			} else {
				g_fast_disk_emul = g_fast_disk_emul_en;
			}
			break;
		case 0xc:
		case 0xd:
			mask = IWM_STATE_Q6 | IWM_STATE_Q7 |
					IWM_STATE_MOTOR_ON | IWM_STATE_ENABLE2;
			mask_on = IWM_STATE_Q6 | IWM_STATE_MOTOR_ON;
			if(((state & mask) == mask_on) && !on) {
				// q6 on, q7 off, !on, motor_on, !enable2
				// Switched from $c08d to $c08c, resync now
				// If latch mode, back up one more bit
				//  (for The School Speller - Tools.woz)
				forced_sync_bit = iwm_calc_bit_sum(
					fbit_pos >> 9, 0 - (state & 1),
					track_bits);
				g_iwm.forced_sync_bit = forced_sync_bit;
				g_iwm.last_rd_bit = forced_sync_bit;
				dbg_log_info(dfcyc, forced_sync_bit*2, 0,
								0x500ec);
			}
			state &= (~IWM_STATE_Q6);
			state |= (on << IWM_BIT_Q6);
			g_iwm.state = state;
			break;
		case 0xe:
		case 0xf:
			mask = IWM_STATE_Q7 | IWM_STATE_MOTOR_ON |
							IWM_STATE_ENABLE2;
			mask_on = IWM_STATE_Q7 | IWM_STATE_MOTOR_ON;
			if(((state & mask) == mask_on) && !on) {
				// q7, !on, motor_on, !enable2
#if 0
				printf("state:%04x and new q7:%d, write_end\n",
					state, on);
#endif
				dbg_log_info(dfcyc, state, mask, 0xed);
				iwm_write_end(dsk, 1, dfcyc);
				// printf("write end complete, the track:\n");
				// iwm_check_nibblization(dfcyc);
			}
			state &= (~IWM_STATE_Q7);
			state |= (on << IWM_BIT_Q7);
			g_iwm.state = state;
			break;
		default:
			printf("iwm_touch_switches: loc: %02x unknown!\n", loc);
			exit(2);
		}
	}

	if(!(state & IWM_STATE_Q7)) {
		g_iwm.num_active_writes = 0;
		if(g_slow_525_emul_wr) {
			g_slow_525_emul_wr = 0;
			engine_recalc_events();
		}
	}

	return dsk;
}

void
iwm_move_to_ftrack(Disk *dsk, word32 new_frac_track, int delta, dword64 dfcyc)
{
	Trk	*trkptr;
	word32	track_bits, cur_frac_track, wr_last_bit, write_val;
	int	disk_525, drive, new_track, cur_track, max_track;
	int	num_active_writes;

	if(dsk->smartport) {
		return;
	}
	cur_frac_track = dsk->cur_frac_track;
	if(delta != 0) {
		// 3.5" move, clear out lower fractional track bits
		new_frac_track = new_frac_track & (-0x10000);
		if((delta < 0) && (new_frac_track < (word32)(-delta))) {
			// Moving down past track 0...stop, but preserve side
			new_frac_track = cur_frac_track & 0x10000;
			delta = 0;
		}
	}
	new_frac_track = new_frac_track + delta;

	disk_525 = dsk->disk_525;
#if 0
	printf("iwm_move_to_track: %07x, num_tracks:%03x (cur:%07x) %016llx\n",
		new_frac_track, dsk->num_tracks, dsk->cur_frac_track, dfcyc);
#endif

	max_track = 36*4 - 1;			// Go to track 35.75 on 5.25"
	if(dsk->num_tracks >= (max_track + 1)) {
		max_track = dsk->num_tracks - 1;
	}
	if(max_track >= 159) {
		max_track = 159;		// Limit to 159 always
	}
	if(new_frac_track > (word32)(max_track << 16)) {
		new_frac_track = max_track << 16;
	}
	new_track = (new_frac_track + 0x8000) >> 16;

	cur_track = (cur_frac_track + 0x8000) >> 16;
	num_active_writes = g_iwm.num_active_writes;
	wr_last_bit = g_iwm.wr_last_bit[0];
	write_val = g_iwm.write_val;
	if(num_active_writes) {
		iwm_write_end(dsk, 0, dfcyc);
		printf("moving arm to new_track:%d, write active:%d, bit:%08x, "
			"write_val:%02x\n", new_track, num_active_writes,
			wr_last_bit, write_val);
	}
	if((cur_track != new_track) || (dsk->cur_trk_ptr == 0)) {
		drive = dsk->drive + 1;
		if(1) {
			// Don't do this printf
		} else if(disk_525) {
			printf("s6d%d Track: %d.%02d\n", drive,
				new_track >> 2, 25* (new_track & 3));
		} else {
			printf("s5d%d Track: %d Side: %d\n", drive,
				new_track >> 1, new_track & 1);
		}

		trkptr = &(dsk->trks[new_track]);
		track_bits = trkptr->track_bits;
		if((track_bits == 0) && disk_525) {
			// Use an adjacent .25 track if it is valid
			if((new_track > 0) && (trkptr[-1].track_bits != 0)) {
				new_track--;
				// printf("Moved dn to qtrk:%04x\n", new_track);
			} else if((new_track < max_track) &&
						(trkptr[1].track_bits != 0)) {
				new_track++;
				// printf("Moved up to qtrk:%04x\n", new_track);
			}
		}
		iwm_flush_disk_to_unix(dsk);
		iwm_move_to_qtr_track(dsk, new_track);
		if(dfcyc != 0) {
			dbg_log_info(dfcyc, cur_frac_track, new_frac_track,
				(g_iwm.state << 16) | 0x00f000e1);
#if 0
			printf("Just moved from track %08x to %08x %016llx\n",
				cur_frac_track, new_frac_track, dfcyc);
#endif
		}
	}
	dsk->cur_frac_track = new_frac_track;
	if(num_active_writes) {
		iwm_start_write(dsk, wr_last_bit, write_val, 1);
	}
}

void
iwm_move_to_qtr_track(Disk *dsk, word32 qtr_track)
{
	Trk	*trkptr;
	word32	track_bits, fbit_pos;

	trkptr = &(dsk->trks[qtr_track]);
	track_bits = trkptr->track_bits;

	dsk->cur_trk_ptr = trkptr;
	dsk->cur_track_bits = track_bits;
	fbit_pos = dsk->cur_fbit_pos;
	if(track_bits) {
		// Moving to a valid track.  Ensure fbit_pos in range
		fbit_pos = fbit_pos % (track_bits * 512);
	}
	dsk->cur_fbit_pos = fbit_pos;
}

dword64 g_iwm_last_phase_dfcyc = 0;

void
iwm525_update_phases(Disk *dsk, dword64 dfcyc)
{
	word32	ign_mask;
	int	last_phases, new_phases, eff_last_phases, eff_new_phases;
	int	my_phase;

	// Decide if dsk->last_phases needs to change.
	last_phases = dsk->last_phases;
	new_phases = (g_iwm.state >> IWM_BIT_PHASES) & 0xf;
	if(last_phases != new_phases) {
		iwm_printf("Phases changing %02x -> %02x, ftrack:%08x at %lld, "
			"diff:%.2fmsec\n", last_phases, new_phases,
			dsk->cur_frac_track, dfcyc >> 16,
			((dfcyc - g_iwm_last_phase_dfcyc) >> 16) / 1000.0);
		g_iwm_last_phase_dfcyc = dfcyc;
	}
	my_phase = (dsk->cur_frac_track >> 17) & 3;
	ign_mask = 1 << ((2 + my_phase) & 3);
	eff_last_phases = last_phases & (~ign_mask);
	eff_new_phases = new_phases & (~ign_mask);
	if(eff_last_phases == eff_new_phases) {
		dsk->last_phases = new_phases;
		return;				// Nothing to do
	}

	// Update last_phases
	iwm525_update_head(dsk, dfcyc);

	dsk->last_phases = new_phases;
	dsk->dfcyc_last_phases = dfcyc;
	dbg_log_info(dfcyc, new_phases, dsk->cur_frac_track, 0x100e1);
}

void
iwm525_update_head(Disk *dsk, dword64 dfcyc)
{
	double	dinc;
	dword64	diff_dusec, dfcyc_last_phases;
	word32	cur_frac_track, frac_track, sum, num, phases, diff;
	int	new_qtrk, cur_qtrk, my_phase, prev_phase, grind, inc;
	int	i;

	cur_frac_track = dsk->cur_frac_track;
	my_phase = (dsk->cur_frac_track >> 17) & 3;

	// If my_phase is 1, then phase 0 being on should decrement the trk
	//  number, and phase 2 being on should increment the trk number
	phases = dsk->last_phases & 0xf;	// one bit for each phase
	phases = (phases << 4) | phases;
	prev_phase = (my_phase + 4 - 1) & 3;
	phases = (phases >> prev_phase) & 0xf;

	// Now, phases[0] means decrement trk, phases[1] is where we are,
	//  phases[2] means increment trk, and phases[3] MIGHT mean increment
	sum = 0;
	num = 0;
	grind = 0;
	for(i = 0; i < 4; i++) {
		if(((phases >> i) & 1) == 0) {
			continue;
		}
		frac_track = cur_frac_track & -0x20000;
		switch(i) {
		case 0:		// Previous phase is on, decrement trk
			if(frac_track >= 0x20000) {
				frac_track -= 0x20000;
			} else {
				frac_track = 0;
			}
			sum += frac_track;
			num++;
			if(cur_frac_track == 0) {
				grind++;
			}
			break;
		case 1:		// "our" phase, pull to aligned track
			sum += frac_track;
			num++;
			break;
		case 2:		// Next phase, increment track
			sum += frac_track + 0x20000;
			num++;
			break;
		case 3:		// Next next phase: might pull
			// Phase is nominally 2 away...but if cur_frac_track
			//  is just a little less than a new halftrack,
			//  (0xxx1ffff), then it may be within a half track
			//  and we'll let it pull us
			frac_track += 0x20000;
			if((frac_track - cur_frac_track) < 0x30000) {
				sum += frac_track;
				num++;
			}
		}
	}

	frac_track = cur_frac_track;
	dfcyc_last_phases = dsk->dfcyc_last_phases;
	dsk->dfcyc_last_phases = 0;
	if(num) {
		frac_track = sum / num;		// New desired track
		// Now see if enough time has elapsed that we got to frac_track
		diff_dusec = (dfcyc - dfcyc_last_phases) >> 16;
		dinc = 0x20000 / 2800.0;	// 2.8msec to move one phase
		inc = (int)dinc;
		if(cur_frac_track >= frac_track) {
			diff = cur_frac_track - frac_track;
			inc = -inc;
		} else {
			diff = frac_track - cur_frac_track;
		}
		if(g_fast_disk_emul) {
			diff = 0;		// Always moved enough
		}
		// Add inc*diff_dusec to cur_frac_track, unless we already reach
		if(diff > (dinc * diff_dusec)) {
			// Enough time has NOT elapsed, so calc where head is
			// Update frac_track to be cur_frac_track + inc * dusec
			frac_track = cur_frac_track +
						(word32)(inc * diff_dusec);
			dsk->dfcyc_last_phases = dfcyc;
		}
	}

	new_qtrk = (frac_track + 0x8000) >> 16;
	cur_qtrk = (cur_frac_track + 0x8000) >> 16;
	if(new_qtrk != cur_qtrk) {
		if(g_halt_arm_move) {
			halt_printf("Halt on arm move\n");
			g_halt_arm_move = 0;
		}
		if(grind) {
			printf("GRIND GRIND GRIND\n");
		}

		dbg_log_info(dfcyc, frac_track, dsk->cur_frac_track,
						(phases << 24) | 0x00e1);
		iwm_move_to_ftrack(dsk, frac_track, 0, dfcyc);

		if(new_qtrk > 2) {
#if 0
			printf("Moving to qtr track: %04x (trk:%d.%02d), %d, "
				"%02x, %08x dfcyc:%lld\n", new_qtrk,
				new_qtrk >> 2, 25*(new_qtrk & 3), my_phase,
				phases, g_iwm.state, dfcyc >> 16);
#endif
		}
	} else {
		// On the same qtr_track, but update the fraction
		dsk->cur_frac_track = frac_track;
	}

#if 0
	/* sanity check stepping algorithm */
	if((qtr_track & 7) == 0) {
		/* check for just access phase 0 */
		if(last_phase != 0) {
			halt_printf("last_phase: %d!\n", last_phase);
		}
	}
#endif
}

int
iwm_read_status35(dword64 dfcyc)
{
	Disk	*dsk;
	word32	state;
	int	drive, stat35, tmp;

	// This is usually done by STAT35 at ff/5fa4
	//  This code is called on the rising edge of Q6 asserted
	state = g_iwm.state;
	drive = (state >> IWM_BIT_DRIVE_SEL) & 1;
	dsk = &(g_iwm.drive35[drive]);

	if(state & IWM_STATE_MOTOR_ON) {
		/* Read status: ph[1], ph[0], disk35, ph[2] */
		stat35 = (((state >> IWM_BIT_PHASES) & 3) << 2) |
			((state >> 6) & 2) |
			(((state >> IWM_BIT_PHASES) >> 2) & 1);

		iwm_printf("Iwm status read state: %02x\n", state);
		dbg_log_info(dfcyc, state, stat35, 0xe7);

		switch(stat35) {
		case 0x00:	/* step direction */
			return (state >> IWM_BIT_STEP_DIRECTION35) & 1;
			break;
		case 0x01:	/* lower head activate */
			/* also return instantaneous data from head */
			iwm_move_to_ftrack(dsk,
				(dsk->cur_frac_track & (-0x20000)), 0, dfcyc);
			return (dsk->cur_fbit_pos >> 15) & 1;
			break;
		case 0x02:	/* disk in place */
			/* 1 = no disk, 0 = disk */
			iwm_printf("read disk in place, num_tracks: %d\n",
				dsk->num_tracks);
			tmp = (dsk->num_tracks <= 0);
			dbg_log_info(dfcyc, 0, dsk->num_tracks, 0x100e7);
			return tmp;
			break;
		case 0x03:	/* upper head activate */
			/* also return instantaneous data from head */
			iwm_move_to_ftrack(dsk, dsk->cur_frac_track | 0x10000,
					0, dfcyc);
			return (dsk->cur_fbit_pos >> 15) & 1;
			break;
		case 0x04:	/* disk is stepping? */
			/* 1 = not stepping, 0 = stepping */
			return 1;
			break;
		case 0x05:	/* Unknown function of ROM 03? */
			/* 1 = or $20 into 0xe1/f24+drive, 0 = don't */
			return 1;
			break;
		case 0x06:	/* disk is locked */
			/* 0 = locked, 1 = unlocked */
			return (!dsk->write_prot);
			break;
		case 0x08:	/* motor on */
			/* 0 = on, 1 = off */
			return !(state & IWM_STATE_MOTOR_ON35);
			break;
		case 0x09:	/* number of sides */
			/* 1 = 2 sides, 0 = 1 side */
			return 1;
			break;
		case 0x0a:	/* at track 0 */
			/* 1 = not at track 0, 0 = there */
			tmp = (dsk->cur_frac_track != 0);
			iwm_printf("Read at track0_35: %d\n", tmp);
			return tmp;
			break;
		case 0x0b:	/* disk ready??? */
			/* 0 = ready, 1 = not ready? */
			tmp = !(state & IWM_STATE_MOTOR_ON35);
			iwm_printf("Read disk ready, ret: %d\n", tmp);
			return tmp;
			break;
		case 0x0c:	/* disk switched?? */
			/* 0 = not switched, 1 = switched? */
			tmp = (dsk->just_ejected != 0);
			iwm_printf("Read disk switched: %d\n", tmp);
			return tmp;
			break;
		case 0x0d:	/* false read when ejecting disk */
			return 1;
		case 0x0e:	/* tachometer */
			halt_printf("Reading tachometer!\n");
			return (dsk->cur_fbit_pos >> 11) & 1;
			break;
		case 0x0f:	/* drive installed? */
			/* 0 = drive exists, 1 = no drive */
			if(drive) {
				/* pretend no drive 1 */
				return 1;
			}
			return 0;
			break;
		default:
			halt_printf("Read 3.5 status, stat35: %02x\n", stat35);
			return 1;
		}
	} else {
		iwm_printf("Read 3.5 status with drive off!\n");
		return 1;
	}
}

void
iwm_do_action35(dword64 dfcyc)
{
	Disk	*dsk;
	word32	state;
	int	drive, stat35;

	// Actions done by CONT35 routine at ff/5fae
	//  This code is called on the rising edge of phase[3] asserted
	state = g_iwm.state;
	drive = (state >> IWM_BIT_DRIVE_SEL) & 1;
	dsk = &(g_iwm.drive35[drive]);

	if(state & IWM_STATE_MOTOR_ON) {
		/* Perform action */
		/* stat35: ph[1], ph[0], disk35_ctrl, ph[2] */
		stat35 = (((state >> IWM_BIT_PHASES) & 3) << 2) |
			((state >> 6) & 2) |
			(((state >> IWM_BIT_PHASES) >> 2) & 1);
		dbg_log_info(dfcyc, state, stat35, 0xf00e7);

		switch(stat35) {
		case 0x00:	/* Set step direction inward (higher tracks) */
			/* towards higher tracks, clear STEP_DIRECTION35 */
			state &= (~IWM_STATE_STEP_DIRECTION35);
			iwm_printf("Iwm set step dir35 = 0\n");
			break;
		case 0x01:	/* Set step direction outward (lower tracks) */
			/* towards lower tracks */
			state |= IWM_STATE_STEP_DIRECTION35;
			dbg_log_info(dfcyc, state, stat35, 0x300e7);
			iwm_printf("Iwm set step dir35 = 1\n");
			break;
		case 0x03:	/* reset disk-switched flag? */
			iwm_printf("Iwm reset disk switch\n");
			dsk->just_ejected = 0;
			break;
		case 0x04:	/* step disk */
			if(state & IWM_STATE_STEP_DIRECTION35) {
				iwm_move_to_ftrack(dsk, dsk->cur_frac_track,
					-0x20000, dfcyc);
			} else {
				iwm_move_to_ftrack(dsk, dsk->cur_frac_track,
					0x20000, dfcyc);
			}
			break;
		case 0x08:	/* turn motor on */
			iwm_printf("Iwm set motor_on35 = 1\n");
			state |= IWM_STATE_MOTOR_ON35;
			break;
		case 0x09:	/* turn motor off */
			iwm_printf("Iwm set motor_on35 = 0\n");
			state &= (~IWM_STATE_MOTOR_ON35);
			break;
		case 0x0d:	/* eject disk */
			printf("Action 0x0d, will eject disk\n");
			iwm_eject_disk(dsk);
			break;
		case 0x02:
		case 0x07:
		case 0x0b: /* hacks to allow AE 1.6MB driver to not crash me */
			break;
		default:
			halt_printf("Do 3.5 action, state: %02x\n", state);
			return;
		}
	} else {
		halt_printf("Set 3.5 status with drive off!\n");
		return;
	}
	g_iwm.state = state;
	dbg_log_info(dfcyc, state, stat35, 0x400e7);
}

int
read_iwm(int loc, dword64 dfcyc)
{
	Disk	*dsk;
	word32	status, bit_diff, bit_pos, state;
	int	on, q7_q6, val;

	loc = loc & 0xf;
	on = loc & 1;

	dsk = iwm_touch_switches(loc, dfcyc);

	state = g_iwm.state;
	q7_q6 = (state >> IWM_BIT_Q6) & 3;

	if(on) {
		/* odd address, return 0 */
		return 0;
	} else {
		/* even address */
		switch(q7_q6) {
		case 0x00:	/* q7 = 0, q6 = 0 */
			if(state & IWM_STATE_ENABLE2) {
				return iwm_read_enable2(dfcyc);
			} else {
				if(state & IWM_STATE_MOTOR_ON) {
					return iwm_read_data(dsk, dfcyc);
				} else {
					iwm_printf("read iwm st 0, m off!\n");
/* HACK!!!! */
					return 0xff;
					//return (((int)dfcyc) & 0x7f) + 0x80;
				}
			}
			break;
		case 0x01:	/* q7 = 0, q6 = 1 */
			/* read IWM status reg */
			if(state & IWM_STATE_ENABLE2) {
				iwm_printf("Read status under enable2: 1\n");
				status = 1;
			} else {
				if(state & IWM_STATE_C031_APPLE35SEL) {
					status = iwm_read_status35(dfcyc);
				} else {
					status = dsk->write_prot;
				}
			}

			val = ((status & 1) << 7) | (state & 0x3f);
			// bit 5 is motor_on, bits[4:0] are iwm_mode
			iwm_printf("Read status: %02x\n", val);

			return val;
			break;
		case 0x02:	/* q7 = 1, q6 = 0 */
			/* read handshake register */
			if(state & IWM_STATE_ENABLE2) {
				return iwm_read_enable2_handshake(dfcyc);
			} else {
				status = 0xc0;
				bit_pos = dsk->cur_fbit_pos >> 9;
				bit_diff = iwm_calc_bit_diff(bit_pos,
						g_iwm.wr_last_bit[0],
						dsk->cur_track_bits);
				if(bit_diff > 8) {
					iwm_printf("Write underrun!\n");
					status = status & 0xbf;
				}
				return status;
			}
			break;
		case 0x03:	/* q7 = 1, q6 = 1 */
			iwm_printf("read iwm q7_q6=3!\n");
			return 0;
		break;
		}
	}
	halt_printf("Got to end of read_iwm, loc: %02x!\n", loc);

	return 0;
}

void
write_iwm(int loc, int val, dword64 dfcyc)
{
	Disk	*dsk;
	word32	state;
	int	on, q7_q6, drive, fast_writes;

	loc = loc & 0xf;
	on = loc & 1;

	dsk = iwm_touch_switches(loc, dfcyc);

	state = g_iwm.state;
	q7_q6 = (state >> IWM_BIT_Q6) & 3;
	drive = (state >> IWM_BIT_DRIVE_SEL) & 1;
	fast_writes = g_fast_disk_emul;
	if(state & IWM_STATE_C031_APPLE35SEL) {
		dsk = &(g_iwm.drive35[drive]);
	} else {
		dsk = &(g_iwm.drive525[drive]);
		fast_writes = !g_slow_525_emul_wr && fast_writes;
	}

	if(on) {
		/* odd address, write something */
		if(q7_q6 == 3) {
			/* q7, q6 = 1,1 */
			if(state & IWM_STATE_MOTOR_ON) {
				if(state & IWM_STATE_ENABLE2) {
					iwm_write_enable2(val);
				} else {
					iwm_write_data(dsk, val, dfcyc);
				}
			} else {
				/* write mode register */
				// bit 0: latch mode (should set if async hand)
				// bit 1: async handshake
				// bit 2: immediate motor off (no 1 sec delay)
				// bit 3: 2us bit timing
				// bit 4: Divide input clock by 8 (instead of 7)
				val = val & 0x1f;
				state = (state & (~0x1f)) | val;
				g_iwm.state = state;
				if(val & 0x10) {
					iwm_printf("set iwm_mode:%02x!\n",val);
				}
			}
		} else {
			if(state & IWM_STATE_ENABLE2) {
				iwm_write_enable2(val);
			} else {
#if 0
// Flobynoid writes to 0xc0e9 causing these messages...
				printf("Write iwm1, st: %02x, loc: %x: %02x\n",
					q7_q6, loc, val);
#endif
			}
		}
		return;
	} else {
		/* even address */
		if(state & IWM_STATE_ENABLE2) {
			iwm_write_enable2(val);
		} else {
			iwm_printf("Write iwm2, st: %02x, loc: %x: %02x\n",
				q7_q6, loc, val);
		}
		return;
	}

	return;
}


int
iwm_read_enable2(dword64 dfcyc)
{
	iwm_printf("Read under enable2 %016llx!\n", dfcyc);
	return 0xff;
}

int g_cnt_enable2_handshake = 0;

int
iwm_read_enable2_handshake(dword64 dfcyc)
{
	int	val;

	iwm_printf("Read handshake under enable2, %016llx!\n", dfcyc);

	val = 0xc0;
	g_cnt_enable2_handshake++;
	if(g_cnt_enable2_handshake > 3) {
		g_cnt_enable2_handshake = 0;
		val = 0x80;
	}

	return val;
}

void
iwm_write_enable2(int val)
{
	// Smartport is selected (PH3=1, PH1=1, Sel35=0), just ignore this data
	iwm_printf("Write under enable2: %02x!\n", val);

	return;
}

word32
iwm_fastemul_start_write(Disk *dsk, dword64 dfcyc)
{
	double	new_fast_cycs;
	dword64	dfcyc_passed;
	word32	fbit_pos, fbit_diff, track_bits;

	// Nox Archaist doesn't finish reading sector header's checksum, but
	//  instead waits for 7 bytes to pass and then writes.  This code
	//  tries to allow fast_disk_emul mode to not overwrite the checksum.
	// Move the fbit_pos forward to try to account for a delay from the
	//  last read to the current write.  But accesses to I/O locations
	//  would still take a lot of time.  Let's assume there were
	//  2 slow cycles, and clamp the skip to a min of 1 byte, max 3 bytes.
	dfcyc_passed = dfcyc - g_iwm.dfcyc_last_fastemul_read;
	new_fast_cycs = (((double)dfcyc_passed) - 0x20000) /
					((double)engine.fplus_ptr->dplus_1);
	// new_fast_cycs approximates the number of fast cycles that have
	//  passed, so 4.0 means 4 fast cycles have passed

	iwm_printf("start write, dfcyc:%016llx, new_f_cycs:%f, plus_1:%08llx\n",
			dfcyc_passed, new_fast_cycs, engine.fplus_ptr->dplus_1);

	fbit_diff = (word32)(new_fast_cycs * dsk->fbit_mult);
	if(new_fast_cycs < 0.0) {
		fbit_diff = 8*512;		// 8 bits
	} else {
		if(fbit_diff < 8*512) {		// 8 bits
			fbit_diff = 8*512;
		} else if(fbit_diff > (32*512)) {
			fbit_diff = 32*512;
		}
	}
	track_bits = dsk->cur_track_bits;
	fbit_pos = dsk->cur_fbit_pos;

	if(track_bits == 0) {
		return fbit_pos;
	}

	fbit_pos = fbit_pos + fbit_diff;
	if(fbit_pos >= (track_bits * 512)) {
		fbit_pos = fbit_pos - (track_bits * 512);
	}
#if 0
	printf(" adjusted fbit_pos from %07x to %07x\n",
					dsk->cur_fbit_pos, fbit_pos);
#endif
	dbg_log_info(dfcyc, fbit_pos, dsk->cur_fbit_pos, 0xee);
	dsk->cur_fbit_pos = fbit_pos;

	return fbit_pos;
}

word32
iwm_read_data_fast(Disk *dsk, dword64 dfcyc)
{
	dword64	dval, dsync_val_tmp, dsync_val;
	word32	bit_pos, new_bit_pos, track_bits, val;
	int	msb, dec;

	if(!g_fast_disk_unnib) {
		g_iwm.dfcyc_last_fastemul_read = dfcyc;
	}
	track_bits = dsk->cur_track_bits;
	bit_pos = dsk->cur_fbit_pos >> 9;

	// fbit_pos points just after the LSB of the last nibble.  Read +8
	//  past to get the next nibble.  Get more to ensure a valid nibble
	new_bit_pos = iwm_calc_bit_sum(bit_pos, 15, track_bits);
	dval = iwm_get_raw_bits(dsk, new_bit_pos, 16, &dsync_val_tmp);
	dsync_val = dsync_val_tmp;
#if 0
	printf("fast %010llx syncs:%010llx bit:%07x\n", dval, dsync_val,
							bit_pos * 2);
#endif

	if(!g_fast_disk_unnib) {
		//dbg_log_info(dfcyc, dval, dsync_val, 0xeb);
	}
	// Find the sync val closest to 15 without going over
	msb = (dsync_val >> 8) & 0xff;
	if((msb > 15) || (msb == 0)) {
		msb = dsync_val & 0xff;
	}
	if(msb > 15) {
		msb = 8;	// Just return something
	}
	if(msb < 7) {
		// This can happen when the arm is moved from a long track to a
		//  shorter track, fbit_pos at an arbitrary position at the
		//  track start, placing us at dsync_val=0x1006.  Just ignore
		msb = 7;
	}
	val = (word32)(dval >> (msb - 7));

	// We've moved new_fbit_pos forward 15 bits, move back 7 bits for msb=15
	dec = 15 - msb - 7;
	if(!g_iwm_motor_on && (msb == 15) && !g_fast_disk_unnib) {
		// Return this byte properly...but not the next one for the
		//  DOS3.3 RWTS motor on detect code, which reads the drive
		//  without enabling the motor, to see if the motor is on
		dec--;
	}
	if((msb != 15) && !g_fast_disk_unnib) {
		// Pull a trick to make the disk motor-on test pass ($bd34 in
		//  DOS 3.3 RWTS): if this is a sync byte, don't return whole
		//  byte, but return whole byte next time
		val = val & 0x7f;
		dec = dec - 8;
	}
	dsk->cur_fbit_pos = iwm_calc_bit_sum(new_bit_pos, dec, track_bits) << 9;
#if 0
	printf("  val:%02x fbit:%07x new_fbit:%07x dec:%d, msb:%d\n",
		val & 0xff, dsk->cur_fbit_pos, new_fbit_pos, dec, msb);
#endif
	if(!g_fast_disk_unnib) {
		dbg_log_info(dfcyc, dsk->cur_fbit_pos,
			(dec << 24) | (bit_pos * 2), (val << 16) | 0xeb);
	}
	return val & 0xff;
}

dword64
iwm_return_rand_data(Disk *dsk, dword64 dfcyc)
{
	dword64	dval, dval2;

	if(dsk) {
		// Use dsk
	}
	dval = dfcyc >> 16;
	dval2 = dval ^ (dval >> 16) ^ (dval << 10) ^ (dval << 26) ^
							(dval << 41);
	dval = dval2 & ((dval >> 6) ^ (dval >> 17));
	return dval;
}

dword64
iwm_get_raw_bits(Disk *dsk, word32 bit_pos, int num_bits, dword64 *dsyncs_ptr)
{
	byte	*bptr, *sync_ptr;
	dword64	dval, dtmp_val, sync_dval;
	word32	track_bits;
	int	pos, bits, total_bits, sync_pos, sync;

	track_bits = dsk->cur_track_bits;
	if(track_bits == 0) {
		halt_printf("iwm_get_raw_bits track_bits 0, %08x\n",
						dsk->cur_frac_track);
		return 0;
	}
	total_bits = 0;
	bits = 1 + (bit_pos & 7);		// 1..8
	pos = bit_pos >> 3;
	dval = 0;
	sync_dval = 0;
	bptr = &(dsk->cur_trk_ptr->raw_bptr[0]);
	sync_ptr = &(dsk->cur_trk_ptr->sync_ptr[0]);
	sync_pos = 0;
	if((bptr == 0) || (sync_ptr == 0)) {
		halt_printf("bptr:%p, sync:%p, bit_pos:%08x, track_bits:%08x, "
			"cur_trk_ptr eff:%05lx\n", bptr, sync_ptr, bit_pos,
			track_bits, dsk->cur_trk_ptr - &(dsk->trks[0]));
		*dsyncs_ptr = 0xffffffffffffffULL;
		return 0;
	}
	while(total_bits < num_bits) {
		dtmp_val = bptr[pos];
		dtmp_val = dtmp_val >> (8 - bits);
		dval = dval | (dtmp_val << total_bits);
		sync = sync_ptr[pos];
		if((sync < 8) && (sync >= (8 - bits))) {	// MSB is here
			sync = sync - (8 - bits);
			sync = sync + total_bits;
			if((sync_pos < 64) && (sync != (sync_dval & 0xff))){
				sync_dval |= ((dword64)sync & 0xff) << sync_pos;
				sync_pos += 8;
			}
		}
		total_bits += bits;
		bits = 8;
		pos--;
		if(pos < 0) {
			pos = (track_bits - 1) >> 3;
			bits = 1 + ((track_bits - 1) & 7);
		}
	}

	if(sync_pos == 0) {
		sync_dval = 0xff;
	}
	*dsyncs_ptr = sync_dval;
	return dval;
}

word32
iwm_calc_bit_diff(word32 first, word32 last, word32 track_bits)
{
	word32	val;

	val = first - last;
	if(val >= track_bits) {
		val = val + track_bits;
	}
	return val;
}

word32
iwm_calc_bit_sum(word32 bit_pos, int add_ival, word32 track_bits)
{
	word32	val;

	val = bit_pos + add_ival;
	if(val >= track_bits) {
		if(add_ival < 0) {
			val = val + track_bits;
		} else {
			val = val - track_bits;
		}
	}
	return val;
}

dword64
iwm_calc_forced_sync(dword64 dval, int forced_bit)
{
	dword64	sync_dval;

	// Something like the "E7" protection scheme has toggled
	//  $c0ed in the middle of reading a nibble, causing a new sync.  This
	//  is used to "move" sync bits inside disk nibbles, to defeat
	//  nibble copiers (since a 36-cycle sync nibble cannot be
	//  differentiated from a 40-cycle sync nibble in one read pass)
	// Return these misaligned nibbles until we re-sync (then clear
	//  g_iwm.forced_sync_bit.  Toggling $c0ed can set the data latch
	//  to $ff is the disk is write-protected, but this gets cleared
	//  to $00 within 4 CPU cycles always (or less).  A phantom 1 will
	//  also shift in

	dval |= (1ULL << forced_bit);
	sync_dval = 30;			// A default for the previous nibble
	while(forced_bit >= 0) {
		// Scan until this bit is set
		if((dval >> forced_bit) & 1) {
			// this bit is the MSB of a nibble, note it
			sync_dval = (sync_dval << 8) | forced_bit;
			forced_bit -= 7;
		}
		forced_bit--;
	}
	return sync_dval;
}

int
iwm_calc_forced_sync_0s(dword64 sync_dval, int bits)
{
	int	sync, forced_bits, done;
	int	i;

	// Return number between 7 and bits as to the oldest valid sync bit
	//  in the sync_dval array.  0xff in the first position means no syncs
	if(bits <= 8) {
		return bits;
	}
	forced_bits = 8;
	done = 0;
	if((sync_dval & 0xff) <= 7) {
		sync_dval = sync_dval >> 8;
	}
	for(i = 0; i < 8; i++) {
		sync = sync_dval & 0xff;
		if(sync == 0xff) {
			return bits;
		}
		sync_dval = sync_dval >> 8;
		while(sync > bits) {
			sync -= 8;		// Bring it back into range
			done = 1;
		}
		if(sync < forced_bits) {
			break;
		}
		if(sync < bits) {
			forced_bits = sync;
		}
		if(done) {
			break;
		}
	}
	return forced_bits;
}

word32
iwm_read_data(Disk *dsk, dword64 dfcyc)
{
	dword64	dval, sync_dval, dsync_val_tmp, dval0, dval2;
	word32	track_bits, fbit_pos, bit_pos, val, forced_sync_bit;
	int	msb_bit, bits, forced_bits, diff;

	track_bits = dsk->cur_track_bits;

	fbit_pos = dsk->cur_fbit_pos;
	bit_pos = fbit_pos >> 9;
	if((track_bits == 0) || (dsk->cur_trk_ptr == 0)) {
		val = ((fbit_pos * 25) >> 11) & 0xff;
		iwm_printf("Reading c0ec, track_len 0, returning %02x\n", val);
		return val;
	}

	if(g_fast_disk_emul) {
		return iwm_read_data_fast(dsk, dfcyc);
	}

	// First, get the last few bytes of data
	bits = iwm_calc_bit_diff(bit_pos, g_iwm.last_rd_bit, track_bits) + 10;
	if(bits < 25) {
		bits = 25;
	} else if(bits > 60) {
		bits = 60;
	}
	forced_sync_bit = g_iwm.forced_sync_bit;
	forced_bits = -1;
	if(forced_sync_bit < track_bits) {
		forced_bits = iwm_calc_bit_diff(bit_pos, forced_sync_bit,
								track_bits);
		if(forced_bits >= 64) {
			forced_bits = 63;
		}
		if(forced_bits > bits) {
			bits = forced_bits;
		}
	}
	dval = iwm_get_raw_bits(dsk, bit_pos, bits, &dsync_val_tmp);
	sync_dval = dsync_val_tmp;

	// See if there are runs of more than three 0 bits...introduce noise
	dval0 = (1ULL << bits) | dval;
	dval0 = dval0 | (dval0 >> 1) | (dval0 >> 2) | (dval0 >> 3);
	dval0 = (~dval0) & ((1ULL << bits) - 1);

	if(dval0) {
		// Each set bit of dval0 indicates the previous 3 bits are 0
		dval2 = dval0 | (dval0 << 1);
		dval2 = dval0 & iwm_return_rand_data(dsk, dfcyc);
#if 0
		printf("dval0 is %016llx, dval2:%016llx, bits:%d\n", dval0,
						dval2, bits);
#endif
		dval = dval ^ dval2;
		if(forced_bits < 0) {
			forced_bits = iwm_calc_forced_sync_0s(sync_dval, bits);
			g_iwm.forced_sync_bit = iwm_calc_bit_sum(bit_pos,
					0 - forced_bits, track_bits);
			dbg_log_info(dfcyc, g_iwm.forced_sync_bit * 2,
				(forced_bits << 24) | (bit_pos * 2), 0x400ec);
#if 0
			printf("Forced bits are %d at %06x, %016llx "
				"%016llx\n", forced_bits, bit_pos * 2, dval,
				sync_dval);
#endif
		}
	}
	if(forced_bits >= 0) {
		sync_dval = iwm_calc_forced_sync(dval, forced_bits);
		dval = dval & ((2ULL << forced_bits) - 1LL);
		dbg_log_info(dfcyc, (word32)dval, forced_sync_bit * 32,
						(forced_bits << 16) | 0xea);
		if(((dsync_val_tmp & 0xff) == (sync_dval & 0xff)) && (!dval0)) {
			// Only clear it if there are no 0's in the dval,
			//  otherwise we'll reenter and could calc a diff sync
			g_iwm.forced_sync_bit = 234567*4;
			//printf("cleared forced_sync\n");
		} else {
			g_iwm.forced_sync_bit = iwm_calc_bit_sum(bit_pos,
					0 - (sync_dval & 0xf), track_bits);
		}

#if 0
		printf("Forced_bits were %d, sync_was %016llx\n",
						forced_bits, dsync_val_tmp);
#endif
	}
#if 0
	printf(" Raw bits: %016llx, dsync:%016llx, fbit:%08x\n", dval,
					sync_dval, fbit_pos);
#endif

	dbg_log_info(dfcyc, (word32)dval,
		(bits << 24) | ((word32)sync_dval & 0x00ffffffUL), 0x300ec);
	msb_bit = sync_dval & 0xff;
	if(g_iwm.state & 1) {		// Latch mode (3.5" disk)
		// last_rd_bit points just past the last bit read (it is
		//  fbit_pos normally, which is one bit past the read bit).
		// Use latched data if that bit is before the latched LSB
		diff = iwm_calc_bit_diff(bit_pos, g_iwm.last_rd_bit,
								track_bits);
		diff = diff + 8;		// Calc to the MSB
		msb_bit = (sync_dval >> 8) & 0xff;
		if((msb_bit >= 8) && (diff > msb_bit)) {
			// We've not seen the LSB of this data: ret latched data
			dbg_log_info(dfcyc, bit_pos * 2,
					(msb_bit << 16) | (diff & 0xffff),
								0x2200ec);
			bit_pos = iwm_calc_bit_sum(bit_pos, -msb_bit + 8,
								track_bits);
			dbg_log_info(dfcyc, bit_pos * 2, g_iwm.last_rd_bit*2,
								0x200ec);
		} else {
			msb_bit = sync_dval & 0xff;
			if(diff <= msb_bit) {
				// Handle case of 10-bit nibble which we've
				//  already returned...don't return it again!
				msb_bit = 1;
			}
		}
#if 0
		printf("Latch mode diff:%d, msb_bit:%d, new_msb:%d\n",
					diff, msb_bit, (int)(sync_dval & 0xff));
#endif
		dbg_log_info(dfcyc, ((word32)diff << 12) | (msb_bit & 0xff),
						(word32)sync_dval, 0x100ec);
	} else {
		if((sync_dval & 0xff) <= 1) {
			// The LSbit or the next one is a sync bit--but we need
			//  to ignore it.  The LSbit is not valid yet, and
			//  the next bit being the MSB of the next nibble will
			//  be ignored to make the 7usec hold time of the
			//  previous nibble work
			sync_dval = sync_dval >> 8;
			msb_bit = sync_dval & 0xff;
		}
	}
	val = (word32)dval;
	if(msb_bit < 8) {
		// We only have a partial byte
		val = val & ((2ULL << msb_bit) - 1);
	} else if(msb_bit < 63) {
		val = (word32)(dval >> (msb_bit - 8));
	}

	// val is now valid from bit 8 down to bit 1
	// We've allowed in to dval an extra bit which we must toss
	val = val >> 1;

	g_iwm.last_rd_bit = bit_pos;
	dbg_log_info(dfcyc, (msb_bit << 24) | (fbit_pos >> 8),
			(word32)(dval0 << 8) | (val & 0xff), 0xec);
#if 0
	if(forced_bits >= 0) {
		printf("Forced bits, msb:%d, val:%02x, dval:%016llx b_p:%06x\n",
			msb_bit, val, dval, bit_pos * 2);
	}
#endif
	return val & 0xff;
}

void
iwm_write_data(Disk *dsk, word32 val, dword64 dfcyc)
{
	Trk	*trk;
	word32	track_bits, bit_pos, fbit_pos, bit_last, tmp_val;
	int	bits;

	trk = dsk->cur_trk_ptr;
	if((trk == 0) || dsk->write_prot) {
		return;
	}
	track_bits = dsk->cur_track_bits;

	bit_pos = (dsk->cur_fbit_pos + 511) >> 9;
	if(track_bits && (bit_pos >= track_bits)) {
		bit_pos = bit_pos - track_bits;
		if(bit_pos >= track_bits) {
			bit_pos = bit_pos % track_bits;
		}
	}
#if 0
	printf("iwm_write_data: %02x %016llx, bit*2:%06x\n", val, dfcyc,
								bit_pos *2);
#endif
	if(dsk->disk_525) {
		if(!g_slow_525_emul_wr) {
			g_slow_525_emul_wr = 1;
			engine_recalc_events();
			if(track_bits && g_fast_disk_emul) {
				fbit_pos = iwm_fastemul_start_write(dsk, dfcyc);
				bit_pos = fbit_pos >> 9;
			}
		}
	}
	dsk->cur_fbit_pos = bit_pos * 512;
	if(g_iwm.num_active_writes == 0) {
		// No write was pending, enter write mode now
		// printf("Starting write of data to the track, track now:\n");
		// iwm_show_track(-1, -1, dfcyc);
		// printf("Write data to track at bit*2:%06x\n", bit_pos);
		iwm_start_write(dsk, bit_pos, val, 0);
		return;
	}
	if(track_bits == 0) {
		halt_printf("Impossible: track_bits: 0\n");
		return;
	}
	if(g_iwm.state & 2) {		// async handshake mode, 3.5"
		iwm_write_data35(dsk, val, dfcyc);
		return;
	}

	// From here on, it's 5.25" disks only
	bit_last = g_iwm.wr_last_bit[0];
	bits = iwm_calc_bit_diff(bit_pos, bit_last, track_bits);
	if(bits >= 500) {
		halt_printf("bits are %d. bit*2:%06x, bit_last*2:%06x\n",
				bits, bit_pos * 2, bit_last * 2);
		bits = 40;
	}
	iwm_write_one_nib(dsk, bits, dfcyc);

	tmp_val = g_iwm.write_val;
	dbg_log_info(dfcyc, (g_iwm.wr_last_bit[0] << 25) | (bit_last * 2),
		(bits << 16) | ((val & 0xff) << 8) | (tmp_val & 0xff), 0xed);
	g_iwm.write_val = val;
}

void
iwm_start_write(Disk *dsk, word32 bit_pos, word32 val, int no_prior)
{
	int	num;

	g_iwm.write_val = val;
	num = 0;
	num = iwm_start_write_act(dsk, bit_pos, num, no_prior, 0);
	num = iwm_start_write_act(dsk, bit_pos, num, no_prior, -1);	// -0.25
	num = iwm_start_write_act(dsk, bit_pos, num, no_prior, +1);	// +0.25
	num = iwm_start_write_act(dsk, bit_pos, num, no_prior, -2);	// -0.50
	num = iwm_start_write_act(dsk, bit_pos, num, no_prior, +2);	// +0.50
	g_iwm.num_active_writes = num;

	woz_maybe_reparse(dsk);
}

int
iwm_start_write_act(Disk *dsk, word32 bit_pos, int num, int no_prior, int delta)
{
	Trk	*trkptr;
	word32	qtr_track, track_bits, bit_diff, prior_sum, allow_diff;

	qtr_track = (dsk->cur_frac_track + 0x8000) >> 16;
	qtr_track += delta;
	if(qtr_track >= 160) {			// Could be unsigned wrap around
		if(delta == 0) {
			halt_printf("iwm_start_write_act0, qtr_track:%04x\n",
								qtr_track);
			g_iwm.num_active_writes = 0;
		}
		return num;
	}
	if(!dsk->disk_525 && delta) {
		return num;		// 3.5" does not affect adjacent trks
	}

	trkptr = &(dsk->trks[qtr_track]);
	track_bits = trkptr->track_bits;
	if(track_bits == 0) {
		if((delta == -2) || (delta == 2)) {
			return num;		// Nothing to do
		}
		if((delta == -1) && (qtr_track > 0)) {
			if(trkptr[-1].track_bits == 0) {
				return num;	// Nothing to do
			}
		}
		if((delta == 1) && (qtr_track < 159)) {
			if(trkptr[1].track_bits == 0) {
				return num;	// Nothing to do
			}
		}
		// Otherwise, we need to create this track and write to it
		track_bits = woz_add_a_track(dsk, qtr_track);
	}
	if(track_bits) {
		bit_pos = bit_pos % track_bits;
	}
	bit_diff = iwm_calc_bit_diff(bit_pos, g_iwm.wr_last_bit[num],
								track_bits);
	prior_sum = 0;
	allow_diff = 16 + (16 * g_fast_disk_emul);
	if((g_iwm.wr_qtr_track[num] == qtr_track) && (bit_diff < allow_diff)) {
		// consider this write a continuation of the previous write
		prior_sum = g_iwm.wr_prior_num_bits[num] +
					g_iwm.wr_num_bits[num] + bit_diff;
#if 0
		printf("prior_sum is %d, qtr_track:%04x, bit_diff:%d\n",
			prior_sum, qtr_track, bit_diff);
#endif
	} else {
#if 0
		printf("No prior_sum, qtr_track:%04x vs %04x, bit_diff:%08x, "
			"bit_pos:%05x wr_last_bit:%05x\n",
			g_iwm.wr_qtr_track[num], qtr_track, bit_diff,
			bit_pos * 2, g_iwm.wr_last_bit[num]*2);
#endif
	}
	if(no_prior) {
		prior_sum = 0;
	}
	if(delta == 0) {
		dsk->cur_fbit_pos = bit_pos << 9;
		iwm_move_to_qtr_track(dsk, qtr_track);
	}
	g_iwm.wr_last_bit[num] = bit_pos;
	g_iwm.wr_qtr_track[num] = qtr_track;
	g_iwm.wr_num_bits[num] = 0;
	g_iwm.wr_prior_num_bits[num] = prior_sum;
	g_iwm.wr_delta[num] = abs(delta);		// 0, 1 or 2
	return num + 1;
}

void
iwm_write_data35(Disk *dsk, word32 val, dword64 dfcyc)
{
	// Just always write 8 bits to the track
	iwm_write_one_nib(dsk, 8, dfcyc);
	g_iwm.write_val = val;
	dsk->cur_fbit_pos = g_iwm.wr_last_bit[0] * 512;
}

void
iwm_write_end(Disk *dsk, int write_through_now, dword64 dfcyc)
{
	Trk	*trkptr;
	word32	last_bit, qtr_track, num_bits, bit_start, delta, track_bits;
	word32	prior_sum;
	int	num_active_writes;
	int	i;

	// Flush out previous write, then turn writing off
	num_active_writes = g_iwm.num_active_writes;
#if 0
	printf("In iwm_write_end at %016llx, num:%d, %d\n", dfcyc,
					num_active_writes, dsk->disk_dirty);
#endif
	if(num_active_writes == 0) {
		return;			// Invalid, not in a write
	}
	if(write_through_now) {
		iwm_write_data(dsk, 0, dfcyc);
	}

	for(i = 0; i < num_active_writes; i++) {
		last_bit = g_iwm.wr_last_bit[i];
		qtr_track = g_iwm.wr_qtr_track[i];
		num_bits = g_iwm.wr_num_bits[i];
		delta = g_iwm.wr_delta[i];
#if 0
		printf(" end %d, last_bit:%05x, qtrk:%04x, num_b:%d, "
			"delta:%d\n", i, last_bit * 2, qtr_track, num_bits,
			delta);
#endif
		if((num_bits == 0) || (qtr_track >= 160)) {
			continue;
		}
		trkptr = &(dsk->trks[qtr_track]);
		track_bits = trkptr->track_bits;
		prior_sum = g_iwm.wr_prior_num_bits[i];
		if((num_bits + prior_sum) >= track_bits) {
			// Full track write.  If delta != 0, erase this track
			printf("Full track write at qtrk:%04x %016llx\n",
							qtr_track, dfcyc);
#if 0
			if(qtr_track == 4) {
				halt_printf("Full track at qtr_trk:4\n");
			}
#endif
			if(delta != 0) {
				woz_remove_a_track(dsk, qtr_track);
				printf("TRACK %04x REMOVED\n", qtr_track);
				continue;
			}
			g_iwm.wr_prior_num_bits[i] = 0;
		}

		// Otherwise, recalc sync for this track
		if(num_bits >= track_bits) {
			num_bits = num_bits % track_bits;
		}
		bit_start = iwm_calc_bit_sum(last_bit, -(int)num_bits - 24,
								track_bits);
		iwm_recalc_sync_from(dsk, qtr_track, bit_start, dfcyc);
#if 0
		printf("Wrote %d bits to qtrk:%04x at %05x %016llx, i:%d,%d, "
			"%d\n", num_bits, qtr_track, bit_start*2, dfcyc, i,
			num_active_writes, dsk->disk_dirty);
#endif
	}
	g_iwm.num_active_writes = 0;

	woz_maybe_reparse(dsk);
}

void
iwm_write_one_nib(Disk *dsk, int bits, dword64 dfcyc)
{
	word32	qtr_track, bit_pos, delta, val, track_bits;
	int	num;
	int	i;

	num = g_iwm.num_active_writes;
	val = g_iwm.write_val;
	for(i = 0; i < num; i++) {
		qtr_track = g_iwm.wr_qtr_track[i];
		bit_pos = g_iwm.wr_last_bit[i];
		delta = g_iwm.wr_delta[i];
		dbg_log_info(dfcyc, val, bit_pos * 2, 0x200ed);
		if(delta == 2) {		// Trk +0.50 and -0.50: corrupt
			val = (val & 0x7f) ^ i ^ 0x0c;
		}
		bit_pos = disk_nib_out_raw(dsk, qtr_track, val, bits, bit_pos,
									dfcyc);
		track_bits = dsk->trks[qtr_track].track_bits;
		if(bit_pos >= track_bits) {
			bit_pos = bit_pos - track_bits;
		}
		g_iwm.wr_last_bit[i] = bit_pos;
		g_iwm.wr_num_bits[i] += bits;
		dbg_log_info(dfcyc, (bits << 24) | (bit_pos * 2),
			2*iwm_calc_bit_sum(bit_pos, 0-g_iwm.wr_num_bits[i],
							track_bits), 0x300ed);
	}
}

void
iwm_recalc_sync_from(Disk *dsk, word32 qtr_track, word32 bit_pos, dword64 dfcyc)
{
	Trk	*trkptr;
	byte	*bptr, *sync_ptr;
	word32	track_bits, val, this_sync, sync0;
	int	pos, next_pos, wrap, bit, last_bit, last_byte, match, this_bits;

	// We are called with a bit_pos 3 bytes before the desired byte.
	//  Look at pos, pos-1, pos+1 in a safe way, and find a valid sync_num
	if(dfcyc) {
		//printf("new sync from %d, %016llx %p\n", bit_pos, dfcyc, dsk);
	}
	if(qtr_track >= 160) {
		halt_printf("iwm_recalc_sync_from bad qtr:%04x bit_pos:%06x\n",
							qtr_track, bit_pos);
		return;
	}
	trkptr = &(dsk->trks[qtr_track]);

	track_bits = trkptr->track_bits;
	if(track_bits == 0) {
		halt_printf("iwm_recalc_sync_from track_bits 0 for %04x\n",
			qtr_track);
		return;
	}
	last_byte = (track_bits - 1) >> 3;
	last_bit = ((track_bits - 1) & 7) + 1;		// 1...8

	sync_ptr = &(trkptr->sync_ptr[0]);
	bptr = &(trkptr->raw_bptr[0]);
	pos = bit_pos >> 3;
	bit = bit_pos & 7;	// 0...7
	sync0 = sync_ptr[pos];
	if(sync0 >= 8) {
		if(pos > 0) {
			pos--;
		} else {
			pos++;		// pos is now 1
		}
		sync0 = sync_ptr[pos];
	}
	bit = (7 - sync0) & 7;
	match = 0;
	wrap = 0;
	next_pos = pos + 1;
	// cnt = 0;
	// printf("recalc_sync_from pos:%04x, bit:%d\n", pos, bit);
	while(1) {
		val = bptr[pos];
		this_bits = 8;
		this_sync = sync_ptr[pos];
		sync_ptr[pos] = 0x20;
		next_pos = pos + 1;
		if(pos >= last_byte) {
#if 0
			printf("At last_byte, val:%02x, pos:%04x, last_bit:%d, "
				"bit:%d\n", val, pos, last_bit, bit);
#endif
			this_bits = last_bit;
			val = val & (0xff00 >> last_bit);
			next_pos = 0;
			wrap++;
			if(wrap >= 3) {
				halt_printf("no stable sync found\n");
				return;
			}
			if(bit >= this_bits) {
				// Skip over this partial byte to byte 0
				bit -= this_bits;
				pos = 0;
				continue;
			}
		}
#if 0
		if(wrap || (pos >= 0x2630)) {
			printf("pos:%04x, bit:%d, val:%04x, this_bits:%d\n",
				pos, bit, val, this_bits);
		}
#endif
#if 0
		if((cnt++ < 10) && (dsk->cur_frac_track == 0)) {
			printf("sync[%04x]=%02x, val:%02x bit:%d, this_b:%d\n",
				pos, this_sync, val, bit, this_bits);
		}
#endif
		// bit is within this byte.  Find next set bit
		while(bit < this_bits) {
			if(((val << bit) & 0x80) == 0) {
				// Slide to next bit
				bit++;
				continue;
			}
			sync_ptr[pos] = 7 - bit;
			if(this_sync == (word32)(7 - bit)) {
				match++;
				if(match >= 7) {
#if 0
					printf("match %d at pos:%04x wrap:%d\n",
						match, pos, wrap);
#endif
					return;
				}
			} else {
				match = 0;
			}
			break;
		}
		bit = (bit + 8 - this_bits) & 7;
		pos = next_pos;
	}
}

/* c600 */
void
sector_to_partial_nib(byte *in, byte *nib_ptr)
{
	byte	*aux_buf, *nib_out;
	word32	val, val2;
	int	x;
	int	i;

	/* Convert 256(+1) data bytes to 342+1 disk nibbles */

	aux_buf = nib_ptr;
	nib_out = nib_ptr + 0x56;

	for(i = 0; i < 0x56; i++) {
		aux_buf[i] = 0;
	}

	x = 0x55;
	for(i = 0x101; i >= 0; i--) {
		val = in[i];
		if(i >= 0x100) {
			val = 0;
		}
		val2 = (aux_buf[x] << 1) + (val & 1);
		val = val >> 1;
		val2 = (val2 << 1) + (val & 1);
		val = val >> 1;
		nib_out[i] = val;
		aux_buf[x] = val2;
		x--;
		if(x < 0) {
			x = 0x55;
		}
	}
}


int
disk_unnib_4x4(Disk *dsk)
{
	int	val1;
	int	val2;

	val1 = iwm_read_data_fast(dsk, 0);
	val2 = iwm_read_data_fast(dsk, 0);

	return ((val1 << 1) + 1) & val2;
}

int
iwm_denib_track525(Disk *dsk, word32 qtr_track, byte *outbuf)
{
	byte	aux_buf[0x80];
	int	sector_done[16];
	byte	*buf;
	word32	val, val2, prev_val, save_frac_track;
	int	track_len, vol, track, phys_sec, log_sec, cksum, x, my_nib_cnt;
	int	save_fbit_pos, tmp_fbit_pos, status, ret, num_sectors_done;
	int	i;

	//printf("iwm_denib_track525\n");

	save_fbit_pos = dsk->cur_fbit_pos;
	save_frac_track = dsk->cur_frac_track;
	iwm_move_to_qtr_track(dsk, qtr_track);

	dsk->cur_fbit_pos = 0;
	g_fast_disk_unnib = 1;

	track_len = (dsk->cur_track_bits + 7) >> 3;

	for(i = 0; i < 16; i++) {
		sector_done[i] = 0;
	}

	num_sectors_done = 0;

	val = 0;
	status = -1;
	my_nib_cnt = 0;
	while(my_nib_cnt++ < 2*track_len) {
		/* look for start of a sector */
		if(val != 0xd5) {
			val = iwm_read_data_fast(dsk, 0);
			continue;
		}

		val = iwm_read_data_fast(dsk, 0);
		if(val != 0xaa) {
			continue;
		}

		val = iwm_read_data_fast(dsk, 0);
		if(val != 0x96) {
			continue;
		}

		/* It's a sector start */
		vol = disk_unnib_4x4(dsk);
		track = disk_unnib_4x4(dsk);
		phys_sec = disk_unnib_4x4(dsk);
		if(phys_sec < 0 || phys_sec > 15) {
			printf("Track %02x, read sec as %02x\n",
						qtr_track >> 2, phys_sec);
			break;
		}
		if(dsk->image_type == DSK_TYPE_DOS33) {
			log_sec = phys_to_dos_sec[phys_sec];
		} else {
			log_sec = phys_to_prodos_sec[phys_sec];
		}
		cksum = disk_unnib_4x4(dsk);
		if((vol ^ track ^ phys_sec ^ cksum) != 0) {
			/* not correct format */
			printf("Track %02x not DOS 3.3 since hdr cksum, %02x "
				"%02x %02x %02x\n", qtr_track >> 2,
				vol, track, phys_sec, cksum);
			break;
		}

		/* see what sector it is */
		if(track != (qtr_track >> 2) || (phys_sec < 0) ||
							(phys_sec > 15)) {
			printf("Track %02x bad since track: %02x, sec: %02x\n",
				qtr_track>>2, track, phys_sec);
			break;
		}

		if(sector_done[phys_sec]) {
			printf("Already done sector %02x on track %02x!\n",
				phys_sec, qtr_track>>2);
			break;
		}

		/* So far so good, let's do it! */
		val = 0;
		i = 0;
		while(i < NIBS_FROM_ADDR_TO_DATA) {
			i++;
			if(val != 0xd5) {
				val = iwm_read_data_fast(dsk, 0);
				continue;
			}

			val = iwm_read_data_fast(dsk, 0);
			if(val != 0xaa) {
				continue;
			}

			val = iwm_read_data_fast(dsk, 0);
			if(val != 0xad) {
				continue;
			}

			/* got it, just break */
			break;
		}

		if(i >= NIBS_FROM_ADDR_TO_DATA) {
			printf("No data header, track %02x, sec %02x at %07x\n",
				qtr_track>>2, phys_sec, dsk->cur_fbit_pos);
			break;
		}

		buf = outbuf + 0x100*log_sec;

		/* Data start! */
		prev_val = 0;
		for(i = 0x55; i >= 0; i--) {
			val = iwm_read_data_fast(dsk, 0);
			val2 = g_from_disk_byte[val];
			if(val2 >= 0x100) {
				printf("Bad data area1, val:%02x,val2:%03x\n",
								val, val2);
				printf(" i:%03x, fbit_pos:%08x\n", i,
							dsk->cur_fbit_pos);
				break;
			}
			prev_val = val2 ^ prev_val;
			aux_buf[i] = prev_val;
		}

		/* rest of data area */
		for(i = 0; i < 0x100; i++) {
			val = iwm_read_data_fast(dsk, 0);
			val2 = g_from_disk_byte[val];
			if(val2 >= 0x100) {
				printf("Bad data area2, read: %02x\n", val);
				printf("  fbit_pos: %07x\n", dsk->cur_fbit_pos);
				break;
			}
			prev_val = val2 ^ prev_val;
			buf[i] = prev_val;
		}

		/* checksum */
		val = iwm_read_data_fast(dsk, 0);
		val2 = g_from_disk_byte[val];
		if(val2 >= 0x100) {
			printf("Bad data area3, read: %02x\n", val);
			printf("  fbit_pos: %07x\n", dsk->cur_fbit_pos);
			break;
		}
		if(val2 != prev_val) {
			printf("Bad data cksum, got %02x, wanted: %02x\n",
				val2, prev_val);
			printf("  fbit_pos: %07x\n", dsk->cur_fbit_pos);
			break;
		}

		val = iwm_read_data_fast(dsk, 0);
		if(val != 0xde) {
			printf("No 0xde at end of sector data:%02x\n", val);
			printf("  fbit_pos: %07x\n", dsk->cur_fbit_pos);
			break;
		}

		val = iwm_read_data_fast(dsk, 0);
		if(val != 0xaa) {
			printf("No 0xde,0xaa at end of sector:%02x\n", val);
			printf("  fbit_pos: %07x\n", dsk->cur_fbit_pos);
			break;
		}

		/* Got this far, data is good, merge aux_buf into buf */
		x = 0x55;
		for(i = 0; i < 0x100; i++) {
			val = aux_buf[x];
			val2 = (buf[i] << 1) + (val & 1);
			val = val >> 1;
			val2 = (val2 << 1) + (val & 1);
			buf[i] = val2;
			val = val >> 1;
			aux_buf[x] = val;
			x--;
			if(x < 0) {
				x = 0x55;
			}
		}
		sector_done[phys_sec] = 1;
		num_sectors_done++;
		if(num_sectors_done >= 16) {
			status = 0;
			break;
		}
	}

	tmp_fbit_pos = dsk->cur_fbit_pos;
	g_fast_disk_unnib = 0;

	ret = 0;
	if(status != 0) {
		printf("Nibblization not done, %02x sectors found qtrk %04x, "
			"drive:%d, slot:%d\n", num_sectors_done, qtr_track,
			dsk->drive, dsk->disk_525 + 5);
		printf("my_nib_cnt: %05x, fbit_pos:%07x, trk_len:%05x\n",
			my_nib_cnt, tmp_fbit_pos, track_len);
		ret = 16;
		for(i = 0; i < 16; i++) {
			printf("sector_done[%d] = %d\n", i, sector_done[i]);
			if(sector_done[i]) {
				ret--;
			}
		}
		iwm_show_a_track(dsk, dsk->cur_trk_ptr, 0);
		if(!ret) {
			ret = -1;
		}
	} else {
		//printf("iwm_denib_track525 succeeded\n");
	}

	dsk->cur_fbit_pos = save_fbit_pos;
	iwm_move_to_ftrack(dsk, save_frac_track, 0, 0);

	return ret;
}

int
iwm_denib_track35(Disk *dsk, word32 qtr_track, byte *outbuf)
{
	word32	buf_c00[0x100];
	word32	buf_d00[0x100];
	word32	buf_e00[0x100];
	int	sector_done[16];
	byte	*buf;
	word32	tmp_5c, tmp_5d, tmp_5e, tmp_66, tmp_67, val, val2;
	word32	save_frac_track;
	int	num_sectors_done, track_len, phys_track, phys_sec, phys_side;
	int	phys_capacity, cksum, tmp, track, side, num_sectors, x, y;
	int	carry, my_nib_cnt, save_fbit_pos, status, ret;
	int	i;

	save_fbit_pos = dsk->cur_fbit_pos;
	save_frac_track = dsk->cur_frac_track;
	iwm_move_to_qtr_track(dsk, qtr_track);

	if(dsk->cur_trk_ptr == 0) {
		return 0;
	}

	dsk->cur_fbit_pos = 0;
	g_fast_disk_unnib = 1;

	track_len = dsk->cur_track_bits >> 3;

	num_sectors = g_track_bytes_35[qtr_track >> 5] >> 9;

	for(i = 0; i < num_sectors; i++) {
		sector_done[i] = 0;
	}

	num_sectors_done = 0;

	val = 0;
	status = -1;
	my_nib_cnt = 0;

	track = qtr_track >> 1;
	side = qtr_track & 1;

	while(my_nib_cnt++ < 2*track_len) {
		/* look for start of a sector */
		if(val != 0xd5) {
			val = iwm_read_data_fast(dsk, 0);
			continue;
		}

		val = iwm_read_data_fast(dsk, 0);
		if(val != 0xaa) {
			continue;
		}

		val = iwm_read_data_fast(dsk, 0);
		if(val != 0x96) {
			continue;
		}

		/* It's a sector start */
		val = iwm_read_data_fast(dsk, 0);
		phys_track = g_from_disk_byte[val];
		if(phys_track != (track & 0x3f)) {
			printf("Track %02x.%d, read track %02x, %02x\n",
				track, side, phys_track, val);
			break;
		}

		phys_sec = g_from_disk_byte[iwm_read_data_fast(dsk, 0)];
		if((phys_sec < 0) || (phys_sec >= num_sectors)) {
			printf("Track %02x.%d, read sector %02x??\n",
				track, side, phys_sec);
			break;
		}
		phys_side = g_from_disk_byte[iwm_read_data_fast(dsk, 0)];

		if(phys_side != ((side << 5) + (track >> 6))) {
			printf("Track %02x.%d, read side %02x??\n",
				track, side, phys_side);
			break;
		}
		phys_capacity = g_from_disk_byte[iwm_read_data_fast(dsk, 0)];
		if(phys_capacity != 0x24 && phys_capacity != 0x22) {
			printf("Track %02x.%x capacity: %02x != 0x24/22\n",
				track, side, phys_capacity);
		}
		cksum = g_from_disk_byte[iwm_read_data_fast(dsk, 0)];

		tmp = phys_track ^ phys_sec ^ phys_side ^ phys_capacity;
		if(cksum != tmp) {
			printf("Track %02x.%d, sector %02x, cksum: %02x.%02x\n",
				track, side, phys_sec, cksum, tmp);
			break;
		}


		if(sector_done[phys_sec]) {
			printf("Already done sector %02x on track %02x.%x!\n",
				phys_sec, track, side);
			break;
		}

		/* So far so good, let's do it! */
		val = 0;
		for(i = 0; i < 38; i++) {
			val = iwm_read_data_fast(dsk, 0);
			if(val == 0xd5) {
				break;
			}
		}
		if(val != 0xd5) {
			printf("No data header, track %02x.%x, sec %02x\n",
				track, side, phys_sec);
			break;
		}

		val = iwm_read_data_fast(dsk, 0);
		if(val != 0xaa) {
			printf("Bad data hdr1,val:%02x trk %02x.%x, sec %02x\n",
				val, track, side, phys_sec);
			printf("fbit_pos: %08x\n", dsk->cur_fbit_pos);
			break;
		}

		val = iwm_read_data_fast(dsk, 0);
		if(val != 0xad) {
			printf("Bad data hdr2,val:%02x trk %02x.%x, sec %02x\n",
				val, track, side, phys_sec);
			printf("dsk->cur_fbit_pos:%07x\n", dsk->cur_fbit_pos);
			break;
		}

		buf = outbuf + (phys_sec << 9);

		/* check sector again */
		tmp = g_from_disk_byte[iwm_read_data_fast(dsk, 0)];
		if(tmp != phys_sec) {
			printf("Bad data hdr3,val:%02x trk %02x.%x, sec %02x\n",
				val, track, side, phys_sec);
			break;
		}

		/* Data start! */
		tmp_5c = 0;
		tmp_5d = 0;
		tmp_5e = 0;
		y = 0xaf;
		carry = 0;

		while(y > 0) {
/* 626f */
			val = iwm_read_data_fast(dsk, 0);
			val2 = g_from_disk_byte[val];
			if(val2 >= 0x100) {
				printf("Bad data area1b, read: %02x\n", val);
				printf(" i:%03x, fbit_pos:%07x\n", i,
							dsk->cur_fbit_pos);
				break;
			}
			tmp_66 = val2;

			tmp_5c = tmp_5c << 1;
			carry = (tmp_5c >> 8);
			tmp_5c = (tmp_5c + carry) & 0xff;

			val = iwm_read_data_fast(dsk, 0);
			val2 = g_from_disk_byte[val];
			if(val2 >= 0x100) {
				printf("Bad data area2, read: %02x\n", val);
				break;
			}

			val2 = val2 + ((tmp_66 << 2) & 0xc0);

			val2 = val2 ^ tmp_5c;
			buf_c00[y] = val2;

			tmp_5e = val2 + tmp_5e + carry;
			carry = (tmp_5e >> 8);
			tmp_5e = tmp_5e & 0xff;
/* 62b8 */
			val = iwm_read_data_fast(dsk, 0);
			val2 = g_from_disk_byte[val];
			val2 = val2 + ((tmp_66 << 4) & 0xc0);
			val2 = val2 ^ tmp_5e;
			buf_d00[y] = val2;
			tmp_5d = val2 + tmp_5d + carry;

			carry = (tmp_5d >> 8);
			tmp_5d = tmp_5d & 0xff;

			y--;
			if(y <= 0) {
				break;
			}

/* 6274 */
			val = iwm_read_data_fast(dsk, 0);
			val2 = g_from_disk_byte[val];
			val2 = val2 + ((tmp_66 << 6) & 0xc0);
			val2 = val2 ^ tmp_5d;
			buf_e00[y+1] = val2;

			tmp_5c = val2 + tmp_5c + carry;
			carry = (tmp_5c >> 8);
			tmp_5c = tmp_5c & 0xff;
		}

/* 62d0 */
		val = iwm_read_data_fast(dsk, 0);
		val2 = g_from_disk_byte[val];

		tmp_66 = (val2 << 6) & 0xc0;
		tmp_67 = (val2 << 4) & 0xc0;
		val2 = (val2 << 2) & 0xc0;

		val = iwm_read_data_fast(dsk, 0);
		val2 = g_from_disk_byte[val] + val2;
		if(tmp_5e != (word32)val2) {
			printf("Checksum 5e bad: %02x vs %02x\n", tmp_5e, val2);
			printf("val:%02x trk %02x.%x, sec %02x\n",
				val, track, side, phys_sec);
			break;
		}

		val = iwm_read_data_fast(dsk, 0);
		val2 = g_from_disk_byte[val] + tmp_67;
		if(tmp_5d != (word32)val2) {
			printf("Checksum 5d bad: %02x vs %02x\n", tmp_5e, val2);
			printf("val:%02x trk %02x.%x, sec %02x\n",
				val, track, side, phys_sec);
			break;
		}

		val = iwm_read_data_fast(dsk, 0);
		val2 = g_from_disk_byte[val] + tmp_66;
		if(tmp_5c != (word32)val2) {
			printf("Checksum 5c bad: %02x vs %02x\n", tmp_5e, val2);
			printf("val:%02x trk %02x.%x, sec %02x\n",
				val, track, side, phys_sec);
			break;
		}

		/* Whew, got it!...check for DE AA */
		val = iwm_read_data_fast(dsk, 0);
		if(val != 0xde) {
			printf("Bad data epi1,val:%02x trk %02x.%x, sec %02x\n",
				val, track, side, phys_sec);
			printf("fbit_pos: %08x\n", dsk->cur_fbit_pos);
			break;
		}

		val = iwm_read_data_fast(dsk, 0);
		if(val != 0xaa) {
			printf("Bad data epi2,val:%02x trk %02x.%x, sec %02x\n",
				val, track, side, phys_sec);
			break;
		}

		/* Now, convert buf_c/d/e to output */
/* 6459 */
		y = 0;
		for(x = 0xab; x >= 0; x--) {
			*buf++ = buf_c00[x];
			y++;
			if(y >= 0x200) {
				break;
			}

			*buf++ = buf_d00[x];
			y++;
			if(y >= 0x200) {
				break;
			}

			*buf++ = buf_e00[x];
			y++;
			if(y >= 0x200) {
				break;
			}
		}

		sector_done[phys_sec] = 1;
		num_sectors_done++;
		if(num_sectors_done >= num_sectors) {
			status = 0;
			break;
		}
		val = 0;
	}

	g_fast_disk_unnib = 0;

	ret = 0;
	if(status != 0) {
		printf("dsk->fbit_pos: %07x, status: %d\n", dsk->cur_fbit_pos,
								status);
		for(i = 0; i < num_sectors; i++) {
			printf("sector done[%d] = %d\n", i, sector_done[i]);
		}
		printf("Nibblization not done, %02x blocks found qtrk %04x\n",
			num_sectors_done, qtr_track);

		ret = -1;
	}

	dsk->cur_fbit_pos = save_fbit_pos;
	iwm_move_to_ftrack(dsk, save_frac_track, 0, 0);

	return ret;

}

/* ret = 1 -> dirty data written out */
/* ret = 0 -> not dirty, no error */
/* ret < 0 -> error */
int
iwm_track_to_unix(Disk *dsk, word32 qtr_track, byte *outbuf)
{
	Trk	*trk;
	dword64	dunix_pos, dret, unix_len;
	int	ret;

	trk = &(dsk->trks[qtr_track]);
	if((trk->track_bits == 0) || (trk->dirty == 0)) {
		return 0;
	}

	printf("iwm_track_to_unix dirty qtr:%04x, dirty:%d\n", qtr_track,
							trk->dirty);
#if 0
	if((qtr_track & 3) && disk_525) {
		halt_printf("You wrote to phase %02x!  Can't wr bk to unix!\n",
			qtr_track);
		dsk->write_through_to_unix = 0;
		return -1;
	}
#endif

	if(dsk->wozinfo_ptr) {				// WOZ disk
		outbuf = trk->raw_bptr;
		ret = 0;
	} else {
		if(dsk->disk_525) {
			if(qtr_track & 3) {
				// Not a valid track
				ret = -1;
			} else {
				ret = iwm_denib_track525(dsk, qtr_track,
									outbuf);
			}
		} else {
			ret = iwm_denib_track35(dsk, qtr_track, outbuf);
		}
	}

	if(ret != 0) {
		return -1;
	}

	/* Write it out */
	dunix_pos = trk->dunix_pos;
	unix_len = trk->unix_len;
	if(unix_len < 0x1000) {
		halt_printf("Disk:%s trk:%06x, dunix_pos:%08llx, len:%08llx\n",
			dsk->name_ptr, dsk->cur_frac_track, dunix_pos,
			unix_len);
		return -1;
	}

	trk->dirty = 0;
	if(dsk->dynapro_info_ptr) {
		return dynapro_write(dsk, outbuf, dunix_pos, (word32)unix_len);
	}

	dret = cfg_write_to_fd(dsk->fd, outbuf, dunix_pos, unix_len);
#if 0
	printf("Write: qtr_trk:%04x, dunix_pos:%08llx, %s\n", qtr_track,
			dunix_pos, dsk->name_ptr);
#endif
	if(dret != unix_len) {
		printf("write: %08llx, errno:%d, trk: %06x, disk: %s\n",
			dret, errno, dsk->cur_frac_track, dsk->name_ptr);
		return -1;
	}
	if(dsk->wozinfo_ptr) {				// WOZ disk
		printf("Wrote track %07x to fd:%d off:%08llx, len:%07llx\n",
			dsk->cur_frac_track, dsk->fd, dunix_pos, unix_len);
		woz_rewrite_crc(dsk, 0);
	}

	return 1;
}

void
show_hex_data(byte *buf, int count)
{
	int	i;

	for(i = 0; i < count; i += 16) {
		printf("%04x: %02x %02x %02x %02x %02x %02x %02x %02x "
			"%02x %02x %02x %02x %02x %02x %02x %02x\n", i,
			buf[i+0], buf[i+1], buf[i+2], buf[i+3],
			buf[i+4], buf[i+5], buf[i+6], buf[i+7],
			buf[i+8], buf[i+9], buf[i+10], buf[i+11],
			buf[i+12], buf[i+13], buf[i+14], buf[i+15]);
	}
}

void
iwm_check_nibblization(dword64 dfcyc)
{
	Disk	*dsk;
	int	slot, drive, sel35;

	drive = (g_iwm.state >> IWM_BIT_DRIVE_SEL) & 1;
	slot = 6;
	if(g_iwm.state & IWM_STATE_MOTOR_ON) {
		sel35 = (g_iwm.state >> IWM_BIT_C031_APPLE35SEL) & 1;
	} else {
		sel35 = (g_iwm.state >> IWM_BIT_LAST_SEL35) & 1;
	}
	if(sel35) {
		dsk = &(g_iwm.drive35[drive]);
		slot = 5;
	} else {
		dsk = &(g_iwm.drive525[drive]);
	}
	printf("iwm_check_nibblization, s%d d%d\n", slot, drive);
	disk_check_nibblization(dsk, 0, 4096, dfcyc);
}

void
disk_check_nibblization(Disk *dsk, byte *in_buf, int size, dword64 dfcyc)
{
	byte	buffer[0x3000];
	word32	qtr_track;
	int	ret, ret2;
	int	i;

	if(size > 0x3000) {
		printf("size %08x is > 0x3000, disk_check_nibblization\n",size);
		exit(3);
	}

	for(i = 0; i < size; i++) {
		buffer[i] = 0;
	}

	//printf("About to call iwm_denib_track*, here's the track:\n");
	//iwm_show_a_track(dsk, dsk->cur_trk_ptr, dfcyc);

	qtr_track = (word32)(dsk->cur_trk_ptr - &(dsk->trks[0]));
	if(qtr_track >= 160) {
		halt_printf("cur_trk_ptr points to bad qtr_track:%08x\n",
								qtr_track);
		return;
	}
	if(dsk->disk_525) {
		ret = iwm_denib_track525(dsk, qtr_track, &(buffer[0]));
	} else {
		ret = iwm_denib_track35(dsk, qtr_track, &(buffer[0]));
	}

	ret2 = -1;
	if(in_buf) {
		for(i = 0; i < size; i++) {
			if(buffer[i] != in_buf[i]) {
				printf("buffer[%04x]: %02x != %02x\n", i,
							buffer[i], in_buf[i]);
				ret2 = i;
				break;
			}
		}
	}

	if((ret != 0) || (ret2 >= 0)) {
		printf("disk_check_nib ret:%d, ret2:%d for track %06x\n",
			ret, ret2, dsk->cur_frac_track);
		if(in_buf) {
			show_hex_data(in_buf, 0x1000);
		}
		show_hex_data(buffer, size);
		iwm_show_a_track(dsk, dsk->cur_trk_ptr, dfcyc);
		if(ret == 16) {
			printf("No sectors found, ignore it\n");
			return;
		}

		halt_printf("Stop\n");
		exit(2);
	}
}

#define TRACK_BUF_LEN		0x2000

void
disk_unix_to_nib(Disk *dsk, int qtr_track, dword64 dunix_pos, word32 unix_len,
	int len_bits, dword64 dfcyc)
{
	byte	track_buf[TRACK_BUF_LEN];
	Trk	*trk;
	byte	*bptr;
	dword64	dret, dlen;
	word32	num_bytes, must_clear_track;
	int	i;

	/* Read track from dsk int track_buf */
#if 0
	printf("disk_unix_to_nib: qtr:%04x, unix_pos:%08llx, unix_len:%08x, "
		"len_bits:%06x\n", qtr_track, dunix_pos, unix_len, len_bits);
#endif

	must_clear_track = 0;

	if(unix_len > TRACK_BUF_LEN) {
		printf("diks_unix_to_nib: requested len of image %s = %05x\n",
			dsk->name_ptr, unix_len);
	}

	bptr = dsk->raw_data;
	if(bptr != 0) {
		// raw_data is valid, so use it
		if((dunix_pos + unix_len) > dsk->raw_dsize) {
			must_clear_track = 1;
		} else {
			bptr += dunix_pos;
			for(i = 0; i < (int)unix_len; i++) {
				track_buf[i] = bptr[i];
			}
		}
	} else if(unix_len > 0) {
		dret = kegs_lseek(dsk->fd, dunix_pos, SEEK_SET);
		if(dret != dunix_pos) {
			printf("lseek of disk %s len 0x%llx ret: %lld, errno:"
				"%d\n", dsk->name_ptr, dunix_pos, dret, errno);
			must_clear_track = 1;
		}

		dlen = read(dsk->fd, track_buf, unix_len);
		if(dlen != unix_len) {
			printf("read of disk %s q_trk %d ret: %lld, errno:%d\n",
				dsk->name_ptr, qtr_track, dlen, errno);
			must_clear_track = 1;
		}
	}

	if(must_clear_track) {
		for(i = 0; i < TRACK_BUF_LEN; i++) {
			track_buf[i] = 0;
		}
	}

#if 0
	printf("Q_track %02x dumped out\n", qtr_track);

	for(i = 0; i < 4096; i += 32) {
		printf("%04x: %02x%02x%02x%02x%02x%02x%02x%02x "
			"%02x%02x%02x%02x%02x%02x%02x%02x "
			"%02x%02x%02x%02x%02x%02x%02x%02x "
			"%02x%02x%02x%02x%02x%02x%02x%02x\n", i,
			track_buf[i+0], track_buf[i+1], track_buf[i+2],
			track_buf[i+3], track_buf[i+4], track_buf[i+5],
			track_buf[i+6], track_buf[i+7], track_buf[i+8],
			track_buf[i+9], track_buf[i+10], track_buf[i+11],
			track_buf[i+12], track_buf[i+13], track_buf[i+14],
			track_buf[i+15], track_buf[i+16], track_buf[i+17],
			track_buf[i+18], track_buf[i+19], track_buf[i+20],
			track_buf[i+21], track_buf[i+22], track_buf[i+23],
			track_buf[i+24], track_buf[i+25], track_buf[i+26],
			track_buf[i+27], track_buf[i+28], track_buf[i+29],
			track_buf[i+30], track_buf[i+31]);
	}
#endif

	dsk->cur_fbit_pos = 0;		/* for consistency */
	dsk->raw_bptr_malloc = 1;

	trk = &(dsk->trks[qtr_track]);
	num_bytes = 2 + ((len_bits + 7) >> 3) + 4;
	trk->track_bits = len_bits;
	trk->dunix_pos = dunix_pos;
	trk->unix_len = unix_len;
	trk->dirty = 0;
	trk->raw_bptr = (byte *)malloc(num_bytes);
	trk->sync_ptr = (byte *)malloc(num_bytes);

	iwm_move_to_ftrack(dsk, qtr_track << 16, 0, dfcyc);

	/* create nibblized image */

	if(dsk->disk_525 && (dsk->image_type == DSK_TYPE_NIB)) {
		iwm_nibblize_track_nib525(dsk, track_buf, qtr_track, unix_len);
	} else if(dsk->disk_525) {
		iwm_nibblize_track_525(dsk, track_buf, qtr_track, dfcyc);
	} else {
		iwm_nibblize_track_35(dsk, track_buf, qtr_track, unix_len,
									dfcyc);
	}

	//printf("For qtr_track:%04x, trk->dirty:%d\n", qtr_track, trk->dirty);
	trk->dirty = 0;
}

void
iwm_nibblize_track_nib525(Disk *dsk, byte *track_buf, int qtr_track,
						word32 unix_len)
{
	byte	*bptr, *sync_ptr;
	int	len;
	int	i;

	// This is the old, dumb .nib format.  It consists of 0x1a00 bytes
	//  per track, but there's no sync information.  Just mark each byte
	//  as being sync=7
	len = unix_len;
	bptr = &(dsk->cur_trk_ptr->raw_bptr[0]);
	sync_ptr = &(dsk->cur_trk_ptr->sync_ptr[0]);
	for(i = 0; i < len; i++) {
		bptr[i] = track_buf[i];
	}
	for(i = 0; i < len; i++) {
		sync_ptr[i] = 7;
	}
	if(dsk->cur_track_bits != (unix_len * 8)) {
		fatal_printf("Track %d.%02d of nib image should be bits:%06x "
			"but it is: %06x\n", qtr_track >> 2, (qtr_track & 3)*25,
			unix_len * 8, dsk->cur_track_bits);
	}

	iwm_printf("Nibblized q_track %02x\n", qtr_track);
}

void
iwm_nibblize_track_525(Disk *dsk, byte *track_buf, int qtr_track, dword64 dfcyc)
{
	byte	partial_nib_buf[0x300];
	word32	val, last_val;
	int	phys_sec, log_sec, num_sync;
	int	i;

#if 0
	printf("nibblize track 525, qtr_track:%04x, trk:%p, trk->raw_bptr:%p, "
		"sync_ptr:%p\n", qtr_track, trk, trk->raw_bptr, trk->sync_ptr);
#endif

	for(phys_sec = 0; phys_sec < 16; phys_sec++) {
		if(dsk->image_type == DSK_TYPE_DOS33) {
			log_sec = phys_to_dos_sec[phys_sec];
		} else {
			log_sec = phys_to_prodos_sec[phys_sec];
		}

		/* Create sync headers */
		if(phys_sec == 0) {
			num_sync = 70;
		} else {
			num_sync = 22;
		}

		for(i = 0; i < num_sync; i++) {
			disk_nib_out(dsk, 0xff, 10);
		}
		disk_nib_out(dsk, 0xd5, 8);		// prolog: d5,aa,96
		disk_nib_out(dsk, 0xaa, 8);
		disk_nib_out(dsk, 0x96, 8);
		disk_4x4_nib_out(dsk, dsk->vol_num);
		disk_4x4_nib_out(dsk, qtr_track >> 2);
		disk_4x4_nib_out(dsk, phys_sec);
		disk_4x4_nib_out(dsk, dsk->vol_num ^ (qtr_track>>2) ^ phys_sec);
		disk_nib_out(dsk, 0xde, 8);		// epilog: de,aa,eb
		disk_nib_out(dsk, 0xaa, 8);
		disk_nib_out(dsk, 0xeb, 8);

		/* Inter sync */
		disk_nib_out(dsk, 0xff, 10);
		for(i = 0; i < 6; i++) {
			disk_nib_out(dsk, 0xff, 10);
		}
		disk_nib_out(dsk, 0xd5, 8);	// data prolog: d5,aa,ad
		disk_nib_out(dsk, 0xaa, 8);
		disk_nib_out(dsk, 0xad, 8);

		sector_to_partial_nib( &(track_buf[log_sec*256]),
			&(partial_nib_buf[0]));

		last_val = 0;
		for(i = 0; i < 0x156; i++) {
			val = partial_nib_buf[i];
			disk_nib_out(dsk, to_disk_byte[last_val ^ val], 8);
			last_val = val;
		}
		disk_nib_out(dsk, to_disk_byte[last_val], 8);

		/* data epilog */
		disk_nib_out(dsk, 0xde, 8);	// data epilog: de,aa,eb
		disk_nib_out(dsk, 0xaa, 8);
		disk_nib_out(dsk, 0xeb, 8);
	}

	/* finish nibblization */
	disk_nib_end_track(dsk, dfcyc);

	iwm_printf("Nibblized q_track %02x\n", qtr_track);

	if(g_check_nibblization) {
		disk_check_nibblization(dsk, &(track_buf[0]), 0x1000, dfcyc);
	}

	//printf("Showing track after nibblization:\n");
	//iwm_show_a_track(dsk, dsk->cur_trk_ptr, dfcyc);
}

void
iwm_nibblize_track_35(Disk *dsk, byte *track_buf, int qtr_track,
						word32 unix_len, dword64 dfcyc)
{
	int	phys_to_log_sec[16];
	word32	buf_c00[0x100];
	word32	buf_d00[0x100];
	word32	buf_e00[0x100];
	byte	*buf;
	word32	val, phys_track, phys_side, capacity, cksum, acc_hi;
	word32	tmp_5c, tmp_5d, tmp_5e, tmp_5f, tmp_63, tmp_64, tmp_65;
	int	num_sectors, log_sec, track, side, num_sync, carry;
	int	interleave, x, y;
	int	i, phys_sec;

	if(dsk->cur_fbit_pos & 511) {
		halt_printf("fbit_pos:%07x is not bit-aligned!\n",
							dsk->cur_fbit_pos);
	}

	num_sectors = (unix_len >> 9);

	for(i = 0; i < num_sectors; i++) {
		phys_to_log_sec[i] = -1;
	}

	phys_sec = 0;
	interleave = 2;
	for(log_sec = 0; log_sec < num_sectors; log_sec++) {
		while(phys_to_log_sec[phys_sec] >= 0) {
			phys_sec++;
			if(phys_sec >= num_sectors) {
				phys_sec = 0;
			}
		}
		phys_to_log_sec[phys_sec] = log_sec;
		phys_sec += interleave;
		if(phys_sec >= num_sectors) {
			phys_sec -= num_sectors;
		}
	}

	track = qtr_track >> 1;
	side = qtr_track & 1;
	for(phys_sec = 0; phys_sec < num_sectors; phys_sec++) {

		log_sec = phys_to_log_sec[phys_sec];
		if(log_sec < 0) {
			printf("Track: %02x.%x phys_sec: %02x = %d!\n",
				track, side, phys_sec, log_sec);
			exit(2);
		}

		/* Create sync headers */
		if(phys_sec == 0) {
			num_sync = 400;
		} else {
			num_sync = 54;
		}

		for(i = 0; i < num_sync; i++) {
			disk_nib_out(dsk, 0xff, 10);
		}

		disk_nib_out(dsk, 0xd5, 8);		/* prolog */
		disk_nib_out(dsk, 0xaa, 8);		/* prolog */
		disk_nib_out(dsk, 0x96, 8);		/* prolog */

		phys_track = track & 0x3f;
		phys_side = (side << 5) + (track >> 6);
		capacity = 0x22;
		disk_nib_out(dsk, to_disk_byte[phys_track], 8);	/* trk */
		disk_nib_out(dsk, to_disk_byte[log_sec], 8);	/* sec */
		disk_nib_out(dsk, to_disk_byte[phys_side], 8);	/* sides+trk */
		disk_nib_out(dsk, to_disk_byte[capacity], 8);	/* capacity*/

		cksum = (phys_track ^ log_sec ^ phys_side ^ capacity) & 0x3f;
		disk_nib_out(dsk, to_disk_byte[cksum], 8);	/* cksum*/

		disk_nib_out(dsk, 0xde, 8);		/* epi */
		disk_nib_out(dsk, 0xaa, 8);		/* epi */

		/* Inter sync */
		for(i = 0; i < 5; i++) {
			disk_nib_out(dsk, 0xff, 10);
		}
		disk_nib_out(dsk, 0xd5, 8);	/* data prolog */
		disk_nib_out(dsk, 0xaa, 8);	/* data prolog */
		disk_nib_out(dsk, 0xad, 8);	/* data prolog */
		disk_nib_out(dsk, to_disk_byte[log_sec], 8);	/* sec again */

		/* do nibblizing! */
		buf = track_buf + (log_sec << 9);

/* 6320 */
		tmp_5e = 0;
		tmp_5d = 0;
		tmp_5c = 0;
		y = 0;
		x = 0xaf;
		buf_c00[0] = 0;
		buf_d00[0] = 0;
		buf_e00[0] = 0;
		buf_e00[1] = 0;
		for(y = 0x4; y > 0; y--) {
			buf_c00[x] = 0;
			buf_d00[x] = 0;
			buf_e00[x] = 0;
			x--;
		}

		while(x >= 0) {
/* 6338 */
			tmp_5c = tmp_5c << 1;
			carry = (tmp_5c >> 8);
			tmp_5c = (tmp_5c + carry) & 0xff;

			val = buf[y];
			tmp_5e = val + tmp_5e + carry;
			carry = (tmp_5e >> 8);
			tmp_5e = tmp_5e & 0xff;

			val = val ^ tmp_5c;
			buf_c00[x] = val;
			y++;
/* 634c */
			val = buf[y];
			tmp_5d = tmp_5d + val + carry;
			carry = (tmp_5d >> 8);
			tmp_5d = tmp_5d & 0xff;
			val = val ^ tmp_5e;
			buf_d00[x] = val;
			y++;
			x--;
			if(x <= 0) {
				break;
			}

/* 632a */
			val = buf[y];
			tmp_5c = tmp_5c + val + carry;
			carry = (tmp_5c >> 8);
			tmp_5c = tmp_5c & 0xff;

			val = val ^ tmp_5d;
			buf_e00[x+1] = val;
			y++;
		}

/* 635f */
		val = ((tmp_5c >> 2) ^ tmp_5d) & 0x3f;
/* 6367 */
		val = (val ^ tmp_5d) >> 2;
/* 636b */
		val = (val ^ tmp_5e) & 0x3f;
/* 636f */
		val = (val ^ tmp_5e) >> 2;
/* 6373 */
		tmp_5f = val;
/* 6375 */
		tmp_63 = 0;
		tmp_64 = 0;
		tmp_65 = 0;
		acc_hi = 0;


		y = 0xae;
		while(y >= 0) {
/* 63e4 */
			/* write out acc_hi */
			val = to_disk_byte[acc_hi & 0x3f];
			disk_nib_out(dsk, val, 8);

/* 63f2 */
			val = to_disk_byte[tmp_63 & 0x3f];
			tmp_63 = buf_c00[y];
			acc_hi = tmp_63 >> 6;
			disk_nib_out(dsk, val, 8);
/* 640b */
			val = to_disk_byte[tmp_64 & 0x3f];
			tmp_64 = buf_d00[y];
			acc_hi = (acc_hi << 2) + (tmp_64 >> 6);
			disk_nib_out(dsk, val, 8);
			y--;
			if(y < 0) {
				break;
			}

/* 63cb */
			val = to_disk_byte[tmp_65 & 0x3f];
			tmp_65 = buf_e00[y+1];
			acc_hi = (acc_hi << 2) + (tmp_65 >> 6);
			disk_nib_out(dsk, val, 8);
		}
/* 6429 */
		val = to_disk_byte[tmp_5f & 0x3f];
		disk_nib_out(dsk, val, 8);

		val = to_disk_byte[tmp_5e & 0x3f];
		disk_nib_out(dsk, val, 8);

		val = to_disk_byte[tmp_5d & 0x3f];
		disk_nib_out(dsk, val, 8);

		val = to_disk_byte[tmp_5c & 0x3f];
		disk_nib_out(dsk, val, 8);

/* 6440 */
		/* data epilog */
		disk_nib_out(dsk, 0xde, 8);	/* epi */
		disk_nib_out(dsk, 0xaa, 8);	/* epi */
		disk_nib_out(dsk, 0xff, 8);
	}

	disk_nib_end_track(dsk, dfcyc);

	if(g_check_nibblization) {
		disk_check_nibblization(dsk, &(track_buf[0]), unix_len, dfcyc);
	}
}

void
disk_4x4_nib_out(Disk *dsk, word32 val)
{
	disk_nib_out(dsk, 0xaa | (val >> 1), 8);
	disk_nib_out(dsk, 0xaa | val, 8);
}

void
disk_nib_out(Disk *dsk, word32 val, int size)
{
	word32	bit_pos;

	bit_pos = dsk->cur_fbit_pos >> 9;
	dsk->cur_fbit_pos = disk_nib_out_raw(dsk,
		(dsk->cur_frac_track + 0x8000) >> 16, val, size, bit_pos, 0) *
									512;
}

void
disk_nib_end_track(Disk *dsk, dword64 dfcyc)
{
	// printf("disk_nib_end_track %p\n", dsk);

	dsk->cur_fbit_pos = 0;
	dsk->disk_dirty = 0;
	iwm_recalc_sync_from(dsk, (word32)(dsk->cur_trk_ptr - &(dsk->trks[0])),
								0, dfcyc);
}

word32
disk_nib_out_raw(Disk *dsk, word32 qtr_track, word32 val, int bits,
				word32 bit_pos, dword64 dfcyc)
{
	Trk	*trkptr;
	byte	*bptr, *sync_ptr;
	word32	track_bits, tmp, mask;
	int	pos, next_pos, bit, to_do, shift_left, shift_right, last_byte;
	int	this_bits;
	int	do_print;

	// write bits from val[7:x].  If bits=3 and val=0xaf, write bits 101.
	// If bits=10 and val=0xaf, write 0xaf then bits 00.

	if(qtr_track >= 160) {
		return bit_pos;
	}
	trkptr = &(dsk->trks[qtr_track]);
	track_bits = trkptr->track_bits;
	if(track_bits == 0) {
		halt_printf("disk_nib_out_raw track_bits=0, %04x\n", qtr_track);
		return bit_pos;
	}

	last_byte = (track_bits - 1) >> 3;
	bit = bit_pos & 7;
	pos = bit_pos >> 3;
	bptr = &(trkptr->raw_bptr[0]);
	sync_ptr = &(trkptr->sync_ptr[0]);
	if(dfcyc != 0) {
		dbg_log_info(dfcyc, (bits << 24) | (bit_pos << 1),
				(track_bits << 16) | (val & 0xffff), 0x100ed);
	}
	dsk->disk_dirty = 1;
	trkptr->dirty = 1;
	do_print = ((pos <= 0x10) || (pos >= 0x18e0)) &&
					(dsk->cur_frac_track == 0xb0000);
	do_print = 0;
	if(do_print) {
		printf("disk_nib_out %02x, %d, %06x\n", val, bits, bit_pos*2);
	}
	while(1) {
		this_bits = 8;
		next_pos = pos + 1;
		if(pos >= last_byte) {
			this_bits = ((track_bits - 1) & 7) + 1;	// 1..8
			next_pos = 0;
		}
		this_bits = (this_bits - bit);		// 1..8
		to_do = bits;				// 1...inf
		if(to_do > this_bits) {
			to_do = this_bits;		// 1..8
		}
		shift_left = (8 - bit - to_do) & 7;
		shift_right = 8 - to_do;
		mask = (1U << to_do) - 1;
		tmp = (val >> shift_right) & mask;
		mask = mask << shift_left;
		if(do_print) {
			printf(" pos:%04x bit:%d tmp:%02x mask:%02x bits:%d "
				"bptr[]=%02x new:%02x todo:%d, r:%d l:%d\n",
				pos, bit, tmp, mask, bits, bptr[pos],
				(bptr[pos] & (~mask)) |
						((tmp << shift_left) & mask),
				to_do, shift_right, shift_left);
		}
		bptr[pos] = (bptr[pos] & (~mask)) |
						((tmp << shift_left) & mask);
		sync_ptr[pos] = 0xff;
		bits -= to_do;
		if(bits <= 0) {
			pos = (pos * 8) + bit + to_do;
			if((bit + to_do) >= 8) {
				pos = next_pos * 8;
			}
			if((word32)pos >= track_bits) {
				pos -= track_bits;
			}
			if(do_print) {
				printf(" returning %05x, bits:%d orig:%05x "
					"%05x\n",
					pos*2, bits, bit_pos*2, last_byte);
			}
			return pos;
		}
		val = (val << to_do) & 0xff;
		bit = 0;
		pos = next_pos;
	}
}

Disk *
iwm_get_dsk_from_slot_drive(int slot, int drive)
{
	Disk	*dsk;
	int	max_drive;

	// pass in slot=5,6,7 drive=0,1 (or more for slot 7)
	max_drive = 2;
	switch(slot) {
	case 5:
		dsk = &(g_iwm.drive35[drive]);
		break;
	case 6:
		dsk = &(g_iwm.drive525[drive]);
		break;
	default:	// slot 7
		max_drive = MAX_C7_DISKS;
		dsk = &(g_iwm.smartport[drive]);
	}
	if(drive >= max_drive) {
		dsk -= drive;		// Move back to drive 0 effectively
	}

	return dsk;
}

void
iwm_toggle_lock(Disk *dsk)
{
	printf("iwm_toggle_lock: write_prot:%d, write_through:%d\n",
		dsk->write_prot, dsk->write_through_to_unix);
	if(dsk->write_prot == 2) {
		// nothing to do
		return;
	}

	if(dsk->write_prot) {
		dsk->write_prot = 0;
	} else {
		dsk->write_prot = 1;
	}
	printf("New dsk->write_prot: %d\n", dsk->write_prot);
	if(dsk->wozinfo_ptr && dsk->write_through_to_unix) {
		woz_rewrite_lock(dsk);
		printf("Called woz_rewrite_lock()\n");
		return;
	}
}

void
iwm_eject_named_disk(int slot, int drive, const char *name,
						const char *partition_name)
{
	Disk	*dsk;

	dsk = iwm_get_dsk_from_slot_drive(slot, drive);
	if(dsk->fd < 0) {
		return;
	}

	/* If name matches, eject the disk! */
	if(!strcmp(dsk->name_ptr, name)) {
		/* It matches, eject it */
		if((partition_name != 0) && (dsk->partition_name != 0)) {
			/* If both have partitions, and they differ, then */
			/*  don't eject.  Otherwise, eject */
			if(strcmp(dsk->partition_name, partition_name) != 0) {
				/* Don't eject */
				return;
			}
		}
		iwm_eject_disk(dsk);
	}
}

void
iwm_eject_disk_by_num(int slot, int drive)
{
	Disk	*dsk;

	dsk = iwm_get_dsk_from_slot_drive(slot, drive);

	iwm_eject_disk(dsk);
}

void
iwm_eject_disk(Disk *dsk)
{
	Woz_info *wozinfo_ptr;
	word32	state;
	int	motor_on;
	int	i;

	printf("Ejecting dsk: %s, fd:%d\n", dsk->name_ptr, dsk->fd);

	if(dsk->fd < 0) {
		return;
	}

	g_config_kegs_update_needed = 1;

	state = g_iwm.state;
	motor_on = state & IWM_STATE_MOTOR_ON;
	if(state & IWM_STATE_C031_APPLE35SEL) {
		motor_on = state & IWM_STATE_MOTOR_ON35;
	}
	if(motor_on) {
		halt_printf("Try eject dsk:%s, but motor_on!\n", dsk->name_ptr);
	}

	dynapro_try_fix_damaged_disk(dsk);
	iwm_flush_disk_to_unix(dsk);

	printf("Ejecting disk: %s\n", dsk->name_ptr);

	/* Free all memory, close file */

	/* free the tracks first */
	if(dsk->trks != 0) {
		for(i = 0; i < MAX_TRACKS; i++) {
			if(dsk->raw_bptr_malloc) {
				free(dsk->trks[i].raw_bptr);
			}
			free(dsk->trks[i].sync_ptr);
			dsk->trks[i].raw_bptr = 0;
			dsk->trks[i].sync_ptr = 0;
			dsk->trks[i].track_bits = 0;
		}
	}
	dsk->num_tracks = 0;
	dsk->raw_bptr_malloc = 0;

	wozinfo_ptr = dsk->wozinfo_ptr;
	if(wozinfo_ptr) {
		if(dsk->raw_data == 0) {
			free(wozinfo_ptr->wozptr);
		}
		wozinfo_ptr->wozptr = 0;
		free(wozinfo_ptr);
	}
	dsk->wozinfo_ptr = 0;

	dynapro_free_dynapro_info(dsk);

	/* close file, clean up dsk struct */
	if(dsk->raw_data) {
		free(dsk->raw_data);
	} else {
		close(dsk->fd);
	}

	dsk->fd = -1;
	dsk->raw_dsize = 0;
	dsk->raw_data = 0;
	dsk->dimage_start = 0;
	dsk->dimage_size = 0;
	dsk->cur_fbit_pos = 0;
	dsk->cur_track_bits = 0;
	dsk->disk_dirty = 0;
	dsk->write_through_to_unix = 0;
	dsk->write_prot = 1;
	dsk->just_ejected = 1;

	/* Leave name_ptr valid */
}

void
iwm_show_track(int slot_drive, int track, dword64 dfcyc)
{
	Disk	*dsk;
	Trk	*trk;
	word32	state;
	int	drive, sel35, qtr_track;

	state = g_iwm.state;
	if(slot_drive < 0) {
		drive = (state >> IWM_BIT_DRIVE_SEL) & 1;
		if(state & IWM_STATE_MOTOR_ON) {
			sel35 = (state >> IWM_BIT_C031_APPLE35SEL) & 1;
		} else {
			sel35 = (state >> IWM_BIT_LAST_SEL35) & 1;
		}
	} else {
		drive = slot_drive & 1;
		sel35 = !((slot_drive >> 1) & 1);
	}
	if(sel35) {
		dsk = &(g_iwm.drive35[drive]);
	} else {
		dsk = &(g_iwm.drive525[drive]);
	}

	if(track < 0) {
		qtr_track = dsk->cur_frac_track >> 16;
	} else {
		qtr_track = track;
	}
	if((dsk->trks == 0) || (qtr_track >= 160)) {
		return;
	}
	trk = &(dsk->trks[qtr_track]);

	if(trk->track_bits == 0) {
		dbg_printf("Track_bits: %d\n", trk->track_bits);
		dbg_printf("No track for type: %d, drive: %d, qtrk:0x%02x\n",
			sel35, drive, qtr_track);
		return;
	}

	dbg_printf("Current s%dd%d, q_track:0x%02x\n", 6 - sel35,
							drive + 1, qtr_track);

	iwm_show_a_track(dsk, trk, dfcyc);
}

void
iwm_show_a_track(Disk *dsk, Trk *trk, dword64 dfcyc)
{
	byte	*bptr;
	byte	*sync_ptr;
	word32	val, track_bits, len, shift, line_len, sync;
	int	i, j;

	track_bits = trk->track_bits;
	dbg_printf("  Showtrack:track_bits*2: %06x, fbit_pos: %07x, "
		"trk:%06x, dfcyc:%016llx\n", track_bits*2, dsk->cur_fbit_pos,
		dsk->cur_frac_track, dfcyc);
	dbg_printf("  disk_525:%d, drive:%d name:%s fd:%d, dimage_start:"
		"%08llx, dimage_size:%08llx\n", dsk->disk_525, dsk->drive,
		dsk->name_ptr, dsk->fd, dsk->dimage_start, dsk->dimage_size);
	dbg_printf("  image_type:%d, vol_num:%02x, write_prot:%d, "
		"write_through:%d, disk_dirty:%d\n", dsk->image_type,
		dsk->vol_num, dsk->write_prot, dsk->write_through_to_unix,
		dsk->disk_dirty);
	dbg_printf("  just_ejected:%d, last_phases:%d, num_tracks:%d\n",
		dsk->just_ejected, dsk->last_phases, dsk->num_tracks);

	len = (track_bits + 7) >> 3;
	if(len >= 0x3000) {
		len = 0x3000;
		dbg_printf("len too big, using %04x\n", len);
	}

	bptr = trk->raw_bptr;
	sync_ptr = trk->sync_ptr;

	len = len + 2;		// Show an extra 2 bytes
	for(i = 0; i < (int)len; i += 16) {
		line_len = 16;
		if((i + line_len) > len) {
			line_len = len - i;
		}
		// First, print raw bptr bytes
		dbg_printf("%04x:  ", i);
		for(j = 0; j < (int)line_len; j++) {
			dbg_printf(" %02x", bptr[i + j]);
			if(((i + j) * 8U) >= track_bits) {
				dbg_printf("*");
			}
		}
		dbg_printf("\n");
		dbg_printf("  sync:");
		for(j = 0; j < (int)line_len; j++) {
			dbg_printf(" %2d", sync_ptr[i + j]);
		}
		dbg_printf("\n");
		dbg_printf("  nibs:");
		for(j = 0; j < (int)line_len; j++) {
			sync = sync_ptr[i+j];
			if(sync >= 8) {
				dbg_printf(" XX");
			} else {
				shift = (7 - sync) & 7;
				val = (bptr[i + j] << 8) | bptr[i + j + 1];
				val = ((val << shift) >> 8) & 0xff;
				dbg_printf(" %02x", val);
			}
		}
		dbg_printf("\n");
	}
}


void
dummy1(word32 psr)
{
	printf("dummy1 psr: %05x\n", psr);
}

void
dummy2(word32 psr)
{
	printf("dummy2 psr: %05x\n", psr);
}
