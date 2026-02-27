const char rcsid_smartport_c[] = "@(#)$KmKId: smartport.c,v 1.61 2024-09-15 13:53:46+00 kentd Exp $";

/************************************************************************/
/*			KEGS: Apple //gs Emulator			*/
/*			Copyright 2002-2024 by Kent Dickey		*/
/*									*/
/*	This code is covered by the GNU GPL v3				*/
/*	See the file COPYING.txt or https://www.gnu.org/licenses/	*/
/*	This program is provided with no warranty			*/
/*									*/
/*	The KEGS web page is kegs.sourceforge.net			*/
/*	You may contact the author at: kadickey@alumni.princeton.edu	*/
/************************************************************************/

#include "defc.h"

extern int Verbose;
extern int Halt_on;
extern int g_rom_version;
extern int g_io_amt;
extern int g_highest_smartport_unit;
extern dword64 g_cur_dfcyc;

extern Engine_reg engine;

extern Iwm g_iwm;

#define LEN_SMPT_LOG	16
STRUCT(Smpt_log) {
	word32	start_addr;
	int	cmd;
	int	rts_addr;
	int	cmd_list;
	int	extras;
	int	unit;
	int	buf;
	int	blk;
};

Smpt_log g_smpt_log[LEN_SMPT_LOG];
int	g_smpt_log_pos = 0;

void
smartport_error(void)
{
	int	pos;
	int	i;

	pos = g_smpt_log_pos;
	printf("Smartport log pos: %d\n", pos);
	for(i = 0; i < LEN_SMPT_LOG; i++) {
		pos--;
		if(pos < 0) {
			pos = LEN_SMPT_LOG - 1;
		}
		printf("%d:%d: t:%04x, cmd:%02x, rts:%04x, "
			"cmd_l:%04x, x:%d, unit:%d, buf:%04x, blk:%04x\n",
			i, pos,
			g_smpt_log[pos].start_addr,
			g_smpt_log[pos].cmd,
			g_smpt_log[pos].rts_addr,
			g_smpt_log[pos].cmd_list,
			g_smpt_log[pos].extras,
			g_smpt_log[pos].unit,
			g_smpt_log[pos].buf,
			g_smpt_log[pos].blk);
	}
}
void
smartport_log(word32 start_addr, word32 cmd, word32 rts_addr, word32 cmd_list)
{
	int	pos;

	pos = g_smpt_log_pos;
	if(start_addr != 0) {
		g_smpt_log[pos].start_addr = start_addr;
		g_smpt_log[pos].cmd = cmd;
		g_smpt_log[pos].rts_addr = rts_addr;
		g_smpt_log[pos].cmd_list = cmd_list;
		g_smpt_log[pos].extras = 0;
		g_smpt_log[pos].unit = 0;
		g_smpt_log[pos].buf = 0;
		g_smpt_log[pos].blk = 0;
	} else {
		pos--;
		if(pos < 0) {
			pos = LEN_SMPT_LOG - 1;
		}
		g_smpt_log[pos].extras = 1;
		g_smpt_log[pos].unit = cmd;
		g_smpt_log[pos].buf = rts_addr;
		g_smpt_log[pos].blk = cmd_list;
	}
	pos++;
	if(pos >= LEN_SMPT_LOG) {
		pos = 0;
	}
	g_smpt_log_pos = pos;
}

void
do_c70d(word32 arg0)
{
	dword64	dsize;
	word32	status_ptr, rts_addr, cmd_list,	cmd_list_lo, cmd_list_mid;
	word32	cmd_list_hi, status_ptr_lo, status_ptr_mid, status_ptr_hi;
	word32	rts_lo, rts_hi, buf_ptr_lo, buf_ptr_hi, buf_ptr, mask, cmd;
	word32	block_lo, block_mid, block_hi, block_hi2, unit, ctl_code;
	word32	ctl_ptr_lo, ctl_ptr_hi, ctl_ptr, block, stat_val;
	int	param_cnt, ret, ext, slot;
	int	i;

	slot = (engine.kpc >> 8) & 7;
	set_memory_c(0x7f8, 0xc0 | slot, 1);

	if((engine.psr & 0x100) == 0) {
		disk_printf("c70d %02x called in native mode!\n", arg0);
		if((engine.psr & 0x30) != 0x30) {
			halt_printf("c70d called native, psr: %03x!\n",
							engine.psr);
		}
	}

	engine.stack = ((engine.stack + 1) & 0xff) + 0x100;
	rts_lo = get_memory_c(engine.stack);
	engine.stack = ((engine.stack + 1) & 0xff) + 0x100;
	rts_hi = get_memory_c(engine.stack);
	rts_addr = (rts_lo + (256*rts_hi) + 1) & 0xffff;
	disk_printf("rts_addr: %04x\n", rts_addr);

	cmd = get_memory_c(rts_addr);
	cmd_list_lo = get_memory_c((rts_addr + 1) & 0xffff);
	cmd_list_mid = get_memory_c((rts_addr + 2) & 0xffff);
	cmd_list_hi = 0;
	mask = 0xffff;
	ext = 0;
	if(cmd & 0x40) {
		ext = 2;
		mask = 0xffffff;
		cmd_list_hi = get_memory_c((rts_addr + 3) & 0xffff);
	}

	cmd_list = cmd_list_lo + (256*cmd_list_mid) + (65536*cmd_list_hi);

	disk_printf("cmd: %02x, cmd_list: %06x\n", cmd, cmd_list);
	param_cnt = get_memory_c(cmd_list);
	unit = get_memory_c((cmd_list + 1) & mask);
	ctl_code = get_memory_c((cmd_list + 4 + ext) & mask);

	smartport_log(0xc70d, cmd, rts_addr, cmd_list);
	dbg_log_info(g_cur_dfcyc, (rts_addr << 16) | (unit << 8) | cmd,
							cmd_list, 0xc70d);
#if 0
	if(cmd != 0x41) {
		printf("SMTPT: c70d %08x, %08x at %016llx\n",
			(rts_addr << 16) | (unit << 8) | cmd, cmd_list,
			g_cur_dfcyc);
	}
#endif
	ret = 0;
	if((unit >= 1) && (unit <= MAX_C7_DISKS) && ext) {
		if(g_iwm.smartport[unit-1].just_ejected) {
			ret = 0x2e;		// DISKSW error
		}
		g_iwm.smartport[unit-1].just_ejected = 0;
	}

	switch(cmd & 0x3f) {
	case 0x00:	/* Status == 0x00 and 0x40 */
		if(param_cnt != 3) {
			disk_printf("param_cnt %d is != 3!\n", param_cnt);
			ret = 0x04;		// BADPCNT
			break;
		}
		status_ptr_lo = get_memory_c((cmd_list+2) & mask);
		status_ptr_mid = get_memory_c((cmd_list+3) & mask);
		status_ptr_hi = 0;
		if(cmd & 0x40) {
			status_ptr_hi = get_memory_c((cmd_list+4) & mask);
		}

		status_ptr = status_ptr_lo + (256*status_ptr_mid) +
							(65536*status_ptr_hi);

		smartport_log(0, unit, status_ptr, ctl_code);
		dbg_log_info(g_cur_dfcyc, (ctl_code << 16) | unit,
							cmd_list, 0xc700);

		disk_printf("unit: %02x, status_ptr: %06x, code: %02x\n",
			unit, status_ptr, ctl_code);
		if((unit == 0) && (ctl_code == 0)) {
			/* Smartport driver status */
			/* see technotes/smpt/tn-smpt-002 */
			set_memory_c(status_ptr, MAX_C7_DISKS, 1);
			set_memory_c(status_ptr+1, 0xff, 1);	// intrpt stat
			set_memory16_c(status_ptr+2, 0x004b, 1); // vendor id
			set_memory16_c(status_ptr+4, 0x1000, 1); // version
			set_memory16_c(status_ptr+6, 0x0000, 1);
			//printf(" driver status, highest_unit:%02x\n",
			//		g_highest_smartport_unit+1);

			engine.xreg = 8;
			engine.yreg = 0;
		} else if((unit > 0) && (ctl_code == 0)) {
			/* status for unit x */
			if((unit > MAX_C7_DISKS) ||
					(g_iwm.smartport[unit-1].fd < 0)) {
				stat_val = 0x80;
				dsize = 0;
				ret = 0;	// Not DISK_SWITCHed error
			} else {
				stat_val = 0xf8;
				dsize = g_iwm.smartport[unit-1].dimage_size;
				dsize = (dsize+511) / 512;
				if(g_iwm.smartport[unit-1].write_prot) {
					stat_val |= 4;		// Write prot
				}
			}
#if 0
			printf("  status unit:%02x just_ejected:%d, "
					"stat_val:%02x\n", unit,
					g_iwm.smartport[unit-1].just_ejected,
					stat_val);
#endif
			set_memory_c(status_ptr, stat_val, 1);
			set_memory24_c(status_ptr + 1, (word32)dsize);
			engine.xreg = 4;
			if(cmd & 0x40) {
				set_memory_c(status_ptr + 4,
						(dsize >> 24) & 0xff, 1);
				engine.xreg = 5;
			}
			engine.yreg = 0;
			disk_printf("just finished unit %d, stat 0\n", unit);
		} else if(ctl_code == 3) {
			if((unit > MAX_C7_DISKS) ||
					(g_iwm.smartport[unit-1].fd < 0)) {
				stat_val = 0x80;
				dsize = 0;
				ret = 0;	// Not a disk-switched error
			} else {
				stat_val = 0xf8;
				dsize = g_iwm.smartport[unit-1].dimage_size;
				dsize = (dsize + 511) / 512;
			}
			if(cmd & 0x40) {
				disk_printf("extended for stat_code 3!\n");
			}
			/* DIB for unit 1 */
			set_memory_c(status_ptr, stat_val, 1);
			set_memory24_c(status_ptr + 1, (word32)dsize);
			if(cmd & 0x40) {
				set_memory_c(status_ptr + 4,
						(dsize >> 24) & 0xff, 1);
				status_ptr++;
			}
			set_memory_c(status_ptr + 4, 4, 1);
			for(i = 5; i < 21; i++) {
				set_memory_c(status_ptr + i, 0x20, 1);
			}
			set_memory_c(status_ptr + 5, 'K', 1);
			set_memory_c(status_ptr + 6, 'E', 1);
			set_memory_c(status_ptr + 7, 'G', 1);
			set_memory_c(status_ptr + 8, 'S', 1);

			// Profile hard disk supporting extended calls+disk_sw
			set_memory16_c(status_ptr + 21, 0xc002, 1);
			set_memory16_c(status_ptr + 23, 0x0000, 1);

			if(cmd & 0x40) {
				engine.xreg = 26;
			} else {
				engine.xreg = 25;
			}
#if 0
			printf("  DIB unit:%02x just_ejected:%d, "
					"stat_val:%02x\n", unit,
					g_iwm.smartport[unit-1].just_ejected,
					stat_val);
#endif
			engine.yreg = 0;

			disk_printf("Just finished unit %d, stat 3\n", unit);
			if(unit == 0 || unit > MAX_C7_DISKS) {
				ret = 0x28;		// NODRIVE error
			}
		} else {
			printf("cmd: 00, unknown unit/status code %02x!\n",
								ctl_code);
			ret = 0x21;			// BADCTL
		}
		break;
	case 0x01:	/* Read Block == 0x01 and 0x41 */
		if(param_cnt != 3) {
			halt_printf("param_cnt %d is != 3!\n", param_cnt);
			ret = 0x04;		// BADPCNT
			break;
		}
		buf_ptr_lo = get_memory_c((cmd_list+2) & mask);
		buf_ptr_hi = get_memory_c((cmd_list+3) & mask);

		buf_ptr = buf_ptr_lo + (256*buf_ptr_hi);
		if(cmd & 0x40) {
			buf_ptr_lo = get_memory_c((cmd_list+4) & mask);
			buf_ptr_hi = get_memory_c((cmd_list+5) & mask);
			buf_ptr += ((buf_ptr_hi*256) + buf_ptr_lo)*65536;
			cmd_list += 2;
		}
		block_lo = get_memory_c((cmd_list+4) & mask);
		block_mid = get_memory_c((cmd_list+5) & mask);
		block_hi = get_memory_c((cmd_list+6) & mask);
		block_hi2 = 0;
		if(cmd & 0x40) {
			block_hi2 = get_memory_c((cmd_list+7) & mask);
		}
		block = (block_hi2 << 24) | (block_hi << 16) |
					(block_mid << 8) | block_lo;
		disk_printf("smartport read unit %d of block %06x to %06x\n",
			unit, block, buf_ptr);
		if(unit < 1 || unit > MAX_C7_DISKS) {
			halt_printf("Unknown unit #: %d\n", unit);
		}

		smartport_log(0, unit - 1, buf_ptr, block);

		if(ret == 0) {
			ret = do_read_c7(unit - 1, buf_ptr, block);
		}
		engine.xreg = 0;
		engine.yreg = 2;
		break;
	case 0x02:	/* Write Block == 0x02 and 0x42 */
		if(param_cnt != 3) {
			halt_printf("param_cnt %d is != 3!\n", param_cnt);
			ret = 0x04;		// BADPCNT
			break;
		}
		buf_ptr_lo = get_memory_c((cmd_list+2) & mask);
		buf_ptr_hi = get_memory_c((cmd_list+3) & mask);

		buf_ptr = buf_ptr_lo + (256*buf_ptr_hi);
		if(cmd & 0x40) {
			buf_ptr_lo = get_memory_c((cmd_list+4) & mask);
			buf_ptr_hi = get_memory_c((cmd_list+5) & mask);
			buf_ptr += ((buf_ptr_hi*256) + buf_ptr_lo)*65536;
			cmd_list += 2;
		}
		block_lo = get_memory_c((cmd_list+4) & mask);
		block_mid = get_memory_c((cmd_list+5) & mask);
		block_hi = get_memory_c((cmd_list+6) & mask);
		block_hi2 = 0;
		if(cmd & 0x40) {
			block_hi2 = get_memory_c((cmd_list+7) & mask);
		}
		block = (block_hi2 << 24) | (block_hi << 16) |
					(block_mid << 8) | block_lo;
		disk_printf("smartport write unit %d of block %04x from %04x\n",
			unit, block, buf_ptr);
		if(unit < 1 || unit > MAX_C7_DISKS) {
			halt_printf("Unknown unit #: %d\n", unit);
		}

		smartport_log(0, unit - 1, buf_ptr, block);

		if(ret == 0) {
			ret = do_write_c7(unit - 1, buf_ptr, block);
		}
		engine.xreg = 0;
		engine.yreg = 2;

		HALT_ON(HALT_ON_C70D_WRITES, "c70d Write done\n");
		break;
	case 0x03:	/* Format == 0x03 and 0x43 */
		if(param_cnt != 1) {
			halt_printf("param_cnt %d is != 1!\n", param_cnt);
			ret = 0x04;		// BADPCNT
			break;
		}
		if((unit < 1) || (unit > MAX_C7_DISKS)) {
			halt_printf("Unknown unit #: %d\n", unit);
			ret = 0x11;		// BADUNIT
		}

		smartport_log(0, unit - 1, 0, 0);

		if(ret == 0) {
			ret = do_format_c7(unit - 1);
		}
		engine.xreg = 0;
		engine.yreg = 2;

		HALT_ON(HALT_ON_C70D_WRITES, "c70d Format done\n");
		break;
	case 0x04:	/* Control == 0x04 and 0x44 */
		if(cmd == 0x44) {
			halt_printf("smartport code 0x44 not supported\n");
		}
		if(param_cnt != 3) {
			halt_printf("param_cnt %d is != 3!\n", param_cnt);
			break;
		}
		ctl_ptr_lo = get_memory_c((cmd_list+2) & mask);
		ctl_ptr_hi = get_memory_c((cmd_list+3) & mask);
		ctl_ptr = (ctl_ptr_hi << 8) + ctl_ptr_lo;
		if(cmd & 0x40) {
			ctl_ptr_lo = get_memory_c((cmd_list+4) & mask);
			ctl_ptr_hi = get_memory_c((cmd_list+5) & mask);
			ctl_ptr += ((ctl_ptr_hi << 8) + ctl_ptr_lo) << 16;
			cmd_list += 2;
		}

		switch(ctl_code) {
		case 0x00:
			printf("Performing a reset on unit %d\n", unit);
			break;
		default:
			halt_printf("control code: %02x ptr:%06x unknown!\n",
							ctl_code, ctl_ptr);
		}
		// printf("CONTROL, ctl_code:%02x\n", ctl_code);

		engine.xreg = 0;
		engine.yreg = 2;
		break;
	default:	/* Unknown command! */
		/* set acc = 1, and set carry, and set kpc */
		engine.xreg = (rts_addr) & 0xff;
		engine.yreg = (rts_addr >> 8) & 0xff;
		ret = 0x01;		// BADCMD error
		if((cmd != 0x4b) && (cmd != 0x48) && (cmd != 0x4a)) {
			// Finder does 0x4a before dialog for formatting disk
			// Finder does 0x4b call before formatting disk
			// Many things do 0x48 call to see online drives
			// So: ignore those, just return BADCMD
			halt_printf("Just did smtport cmd:%02x rts_addr:%04x, "
				"cmdlst:%06x\n", cmd, rts_addr, cmd_list);
		}
	}

	engine.acc = (engine.acc & 0xff00) | (ret & 0xff);
	engine.psr &= ~1;
	if(ret) {
		engine.psr |= 1;
		printf("Smtport cmd:%02x unit:%02x ctl_code:%02x ret:%02x\n",
			cmd, unit, ctl_code, ret);
	}
	engine.kpc = (rts_addr + 3 + ext) & 0xffff;
	// printf("   ret:%02x psr_c:%d\n", ret & 0xff, engine.psr & 1);
}

// $C70A is the ProDOS entry point, documented in ProDOS 8 Technical Ref
//  Manual, section 6.3.
void
do_c70a(word32 arg0)
{
	dword64	dsize;
	word32	cmd, unit, buf_lo, buf_hi, blk_lo, blk_hi, blk, buf;
	word32	prodos_unit;
	int	ret, slot;

	slot = (engine.kpc >> 8) & 7;
	set_memory_c(0x7f8, 0xc0 | slot, 1);

	cmd = get_memory_c((engine.direct + 0x42) & 0xffff);
	prodos_unit = get_memory_c((engine.direct + 0x43) & 0xffff);
	buf_lo = get_memory_c((engine.direct + 0x44) & 0xffff);
	buf_hi = get_memory_c((engine.direct + 0x45) & 0xffff);
	blk_lo = get_memory_c((engine.direct + 0x46) & 0xffff);
	blk_hi = get_memory_c((engine.direct + 0x47) & 0xffff);

	blk = (blk_hi << 8) + blk_lo;
	buf = (buf_hi << 8) + buf_lo;
	disk_printf("c70a %02x cmd:%02x, pro_unit:%02x, buf:%04x, blk:%04x\n",
		arg0, cmd, prodos_unit, buf, blk);

	unit = 0 + (prodos_unit >> 7);		// units 0,1
	if((prodos_unit & 0x7f) != (slot << 4)) {
		unit += 2;			// units 2,3
	}

	smartport_log(0xc70a, cmd, blk, buf);
	dbg_log_info(g_cur_dfcyc,
		(buf << 16) | ((unit & 0xff) << 8) | (cmd & 0xff), blk, 0xc70a);

#if 0
	if(cmd != 0x1ff) {
		printf("SMTPT: c70a %08x %08x\n",
			(buf << 16) | ((unit & 0xff) << 8) | (cmd & 0xff), blk);
	}
#endif
	engine.psr &= ~1;	/* clear carry */

	ret = 0x27;	/* I/O error */
	if(cmd == 0x00) {
		dsize = g_iwm.smartport[unit].dimage_size;
		dsize = (dsize + 511) / 512;

		smartport_log(0, unit, (word32)dsize, 0);
		dbg_log_info(g_cur_dfcyc, ((unit & 0xff) << 8) | (cmd & 0xff),
							(word32)dsize, 0x1c700);

		ret = 0;
		engine.xreg = dsize & 0xff;
		engine.yreg = (word32)(dsize >> 8);
	} else if(cmd == 0x01) {
		smartport_log(0, unit, buf, blk);
		ret = do_read_c7(unit, buf, blk);
	} else if(cmd == 0x02) {
		smartport_log(0, unit, buf, blk);
		ret = do_write_c7(unit, buf, blk);
	} else if(cmd == 0x03) {	/* format */
		smartport_log(0, unit, buf, blk);
		ret = do_format_c7(unit);
	}

	engine.acc = (engine.acc & 0xff00) | (ret & 0xff);
	if(ret != 0) {
		engine.psr |= 1;			// Set carry
	}
	return;
}

int
do_read_c7(int unit_num, word32 buf, word32 blk)
{
	byte	local_buf[0x200];
	Disk	*dsk;
	byte	*bptr;
	dword64	dimage_start, dimage_size, dret;
	word32	val;
	int	len, fd;
	int	i;

	dbg_log_info(g_cur_dfcyc, (buf << 8) | (unit_num & 0xff), blk, 0xc701);
	if((unit_num < 0) || (unit_num > MAX_C7_DISKS)) {
		halt_printf("do_read_c7: unit_num: %d\n", unit_num);
		smartport_error();
		return 0x28;
	}

	dsk = &(g_iwm.smartport[unit_num]);
	fd = dsk->fd;
	dimage_start = dsk->dimage_start;
	dimage_size = dsk->dimage_size;
	if(fd < 0) {
		printf("c7_fd == %d!\n", fd);
#if 0
		if(blk != 2 && blk != 0) {
			/* don't print error if only reading directory */
			smartport_error();
			halt_printf("Read unit:%02x blk:%04x\n", unit_num, blk);
		}
#endif
		return 0x2f;
	}
	if(((blk + 1) * 0x200ULL) > (dimage_start + dimage_size)) {
		halt_printf("Tried to read past %08llx on disk (blk:%04x)\n",
			dimage_start + dimage_size, blk);
		smartport_error();
		return 0x27;
	}

	if(dsk->raw_data) {
		// image was compressed and is in dsk->raw_data
		bptr = dsk->raw_data + dimage_start + (blk*0x200ULL);
		for(i = 0; i < 0x200; i++) {
			local_buf[i] = bptr[i];
		}
	} else {
		dret = kegs_lseek(fd, dimage_start + blk*0x200ULL, SEEK_SET);
		if(dret != (dimage_start + blk*0x200ULL)) {
			halt_printf("lseek ret %08llx, errno:%d\n", dret,
									errno);
			smartport_error();
			return 0x27;
		}

		len = (int)read(fd, &local_buf[0], 0x200);
		if(len != 0x200) {
			printf("read returned %08x, errno:%d, blk:%04x, unit:"
				"%02x\n", len, errno, blk, unit_num);
			halt_printf("name: %s\n", dsk->name_ptr);
			smartport_error();
			return 0x27;
		}
	}

	g_io_amt += 0x200;

	if(buf >= 0xfc0000) {
		disk_printf("reading into ROM, just returning\n");
		return 0;
	}

	for(i = 0; i < 0x200; i += 2) {
		val = (local_buf[i+1] << 8) + local_buf[i];
		set_memory16_c(buf + i, val, 0);
	}

	return 0;
}

int
do_write_c7(int unit_num, word32 buf, word32 blk)
{
	byte	local_buf[0x200];
	Disk	*dsk;
	dword64	dret, dimage_start, dimage_size;
	int	len, fd, ret;
	int	i;

	dbg_log_info(g_cur_dfcyc, (buf << 16) | (unit_num & 0xff), blk, 0xc702);

	if(unit_num < 0 || unit_num > MAX_C7_DISKS) {
		halt_printf("do_write_c7: unit_num: %d\n", unit_num);
		smartport_error();
		return 0x28;
	}

	dsk = &(g_iwm.smartport[unit_num]);
	fd = dsk->fd;
	dimage_start = dsk->dimage_start;
	dimage_size = dsk->dimage_size;
	if(fd < 0) {
		halt_printf("c7_fd == %d!\n", fd);
		smartport_error();
		return 0x28;
	}

	for(i = 0; i < 0x200; i++) {
		local_buf[i] = get_memory_c(buf + i);
	}

	if(dsk->write_prot) {
		printf("Write, but s7d%d %s is write protected!\n",
						unit_num + 1, dsk->name_ptr);
		return 0x2b;
	}

	if(dsk->write_through_to_unix == 0) {
		//halt_printf("Write to %s, but not wr_thru!\n", dsk->name_ptr);
		if(dsk->raw_data) {
			// Update the memory copy
			ret = smartport_memory_write(dsk, &local_buf[0],
						blk * 0x200ULL, 0x200);
			if(ret) {
				return 0x27;		// I/O Error
			}
		}
		return 0x00;
	}

	if(dsk->dynapro_info_ptr) {
		dynapro_write(dsk, &local_buf[0], blk*0x200UL, 0x200);
	} else {
		dret = kegs_lseek(fd, dimage_start + blk*0x200ULL, SEEK_SET);
		if(dret != (dimage_start + blk*0x200ULL)) {
			halt_printf("lseek returned %08llx, errno: %d\n", dret,
								errno);
			smartport_error();
			return 0x27;
		}

		if(dret >= (dimage_start + dimage_size)) {
			halt_printf("Tried to write to %08llx\n", dret);
			smartport_error();
			return 0x27;
		}

		len = (int)write(fd, &local_buf[0], 0x200);
		if(len != 0x200) {
			halt_printf("write ret %08x bytes, errno: %d\n", len,
									errno);
			smartport_error();
			dsk->write_prot = 1;
			return 0x2b;		// Write protected
		}
	}

	g_io_amt += 0x200;

	return 0;
}

int
smartport_memory_write(Disk *dsk, byte *bufptr, dword64 doffset, word32 size)
{
	byte	*bptr;
	word32	ui;

	bptr = dsk->raw_data;
	if((bptr == 0) || ((doffset + size) > dsk->dimage_size)) {
		printf("Write to %s failed, %08llx past end %08llx\n",
			dsk->name_ptr, doffset, dsk->dimage_size);
		return -1;
	}
	for(ui = 0; ui < size; ui++) {
		bptr[doffset + ui] = bufptr[ui];
	}

	return 0;
}

int
do_format_c7(int unit_num)
{
	byte	local_buf[0x1000];
	Disk	*dsk;
	dword64	dimage_start, dimage_size, dret, dtotal, dsum;
	int	len, max, fd, ret;
	int	i;

	dbg_log_info(g_cur_dfcyc, (unit_num & 0xff), 0, 0xc703);

	if(unit_num < 0 || unit_num > MAX_C7_DISKS) {
		halt_printf("do_format_c7: unit_num: %d\n", unit_num);
		smartport_error();
		return 0x28;
	}

	dsk = &(g_iwm.smartport[unit_num]);
	fd = dsk->fd;
	dimage_start = dsk->dimage_start;
	dimage_size = dsk->dimage_size;
	if(fd < 0) {
		halt_printf("c7_fd == %d!\n", fd);
		smartport_error();
		return 0x28;
	}

	if(dsk->write_prot || (dsk->raw_data && !dsk->dynapro_info_ptr)) {
		printf("Format, but %s is write protected!\n", dsk->name_ptr);
		return 0x2b;
	}

	if(dsk->write_through_to_unix == 0) {
		if(!dsk->raw_data) {
			printf("Format of %s ignored\n", dsk->name_ptr);
			return 0x00;
		}
	}

	for(i = 0; i < 0x1000; i++) {
		local_buf[i] = 0;
	}

	if(!dsk->dynapro_info_ptr) {
		dret = kegs_lseek(fd, dimage_start, SEEK_SET);
		if(dret != dimage_start) {
			halt_printf("lseek returned %08llx, errno: %d\n", dret,
									errno);
			smartport_error();
			return 0x27;
		}
	}

	dsum = 0;
	dtotal = dimage_size;

	while(dsum < dtotal) {
		max = (int)MY_MIN(0x1000, dtotal - dsum);
		len = max;
		if(dsk->dynapro_info_ptr) {
			dynapro_write(dsk, &local_buf[0], dsum, max);
		} else if(dsk->raw_data) {
			ret = smartport_memory_write(dsk, &local_buf[0],
								dsum, max);
			if(ret) {
				return 0x27;		// I/O Error
			}
		} else {
			len = (int)write(fd, &local_buf[0], max);
		}
		if(len != max) {
			halt_printf("write ret %08x, errno:%d\n", len, errno);
			smartport_error();
			dsk->write_prot = 1;
			return 0x2b;		// Write-protected
		}
		dsum += len;
	}

	return 0;
}

void
do_c700(word32 ret)
{
	int	slot;

	disk_printf("do_c700 called, ret: %08x\n", ret);
	dbg_log_info(g_cur_dfcyc, 0, 0, 0xc700);

	slot = (engine.kpc >> 8) & 7;
	ret = do_read_c7(0, 0x800, 0);		// Always read unit 0, block 0

	set_memory_c(0x7f8, slot, 1);
	set_memory16_c(0x42, (slot << 12) | 1, 1);
	set_memory16_c(0x44, 0x0800, 1);
	set_memory16_c(0x46, 0x0000, 1);
	engine.xreg = slot << 4;		// 0x70 for slot 7
	engine.kpc = 0x801;

	if(ret != 0) {
		printf("Failure reading boot disk in s7d1, trying slot 5!\n");
		engine.kpc = 0xc500;		// Try to boot slot 5
		if((slot == 5) || (g_rom_version == 0)) {
			engine.kpc = 0xc600;	// Try to boot slot 6
		}
	}
}

