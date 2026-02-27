const char rcsid_woz_c[] = "@(#)$KmKId: woz.c,v 1.34 2025-01-07 16:45:35+00 kentd Exp $";

/************************************************************************/
/*			KEGS: Apple //gs Emulator			*/
/*			Copyright 2002-2025 by Kent Dickey		*/
/*									*/
/*	This code is covered by the GNU GPL v3				*/
/*	See the file COPYING.txt or https://www.gnu.org/licenses/	*/
/*	This program is provided with no warranty			*/
/*									*/
/*	The KEGS web page is kegs.sourceforge.net			*/
/*	You may contact the author at: kadickey@alumni.princeton.edu	*/
/************************************************************************/

// Based on WOZ 2.0/1.0 spec at https://applesaucefdc.com/woz/reference2/

#include "defc.h"

extern const char g_kegs_version_str[];

byte g_woz_hdr_bytes[12] = { 0x57, 0x4f, 0x5a, 0x32,		// "WOZ2"
				0xff, 0x0a, 0x0d, 0x0a, 0, 0, 0, 0 };

word32 g_woz_crc32_tab[256];

void
woz_crc_init()
{
	word32	crc, val, xor;
	int	i, j;

	for(i = 0; i < 256; i++) {
		crc = 0;
		val = i;
		for(j = 0; j < 8; j++) {
			xor = 0;
			if((val ^ crc) & 1) {
				xor = 0xedb88320UL;
			}
			crc = (crc >> 1) ^ xor;
			val = val >> 1;
		}
		g_woz_crc32_tab[i] = crc;
		// printf("crc32_tab[%d] = %08x\n", i, crc);
	}
}

word32
woz_calc_crc32(byte *bptr, dword64 dlen, word32 bytes_to_skip)
{
	word32	crc, c;

	crc = (~0U);
	if(bytes_to_skip > dlen) {
		dlen = 0;
	} else {
		bptr += bytes_to_skip;
		dlen -= bytes_to_skip;
	}
	while(dlen != 0) {
		c = *bptr++;
		dlen--;
		crc = g_woz_crc32_tab[(crc ^ c) & 0xff] ^ (crc >> 8);
	}
	return (~crc);
}

void
woz_rewrite_crc(Disk *dsk, int min_write_size)
{
	Woz_info *wozinfo_ptr;
	byte	*wozptr;
	word32	crc;

	// Recalculate WOZ image CRC and write it to disk

	wozinfo_ptr = dsk->wozinfo_ptr;
	if(!wozinfo_ptr || (dsk->fd < 0)) {
		return;
	}
	wozptr = wozinfo_ptr->wozptr;
	crc = woz_calc_crc32(&wozptr[0], wozinfo_ptr->woz_size, 12);
	cfg_set_le32(&wozptr[8], crc);
	if(min_write_size < 12) {
		min_write_size = 12;
	}
	cfg_write_to_fd(dsk->fd, wozptr, 0, min_write_size);
}

void
woz_rewrite_lock(Disk *dsk)
{
	Woz_info *wozinfo_ptr;
	byte	*wozptr;
	int	offset;

	wozinfo_ptr = dsk->wozinfo_ptr;
	if(!wozinfo_ptr || (dsk->fd < 0)) {
		return;
	}
	wozptr = wozinfo_ptr->wozptr;
	offset = wozinfo_ptr->info_offset;

	wozptr[offset + 2] = (dsk->write_prot != 0);	// Update locked
	woz_rewrite_crc(dsk, offset + 2 + 1);
}

void
woz_check_file(Disk *dsk)
{
	Woz_info *wozinfo_ptr;
	byte	*wozptr, *newwozptr, *bptr;
	word32	fdval, memval, crc, mem_crc, crcnew, file_size;
	int	woz_size;
	int	i;

	wozinfo_ptr = dsk->wozinfo_ptr;
	if(!wozinfo_ptr) {
		return;
	}
	woz_size = wozinfo_ptr->woz_size;
	wozptr = wozinfo_ptr->wozptr;
	crc = woz_calc_crc32(wozptr, woz_size, 12);
	mem_crc = wozptr[8] | (wozptr[9] << 8) | (wozptr[10] << 16) |
							(wozptr[11] << 24);
	if(crc != mem_crc) {
		halt_printf("WOZ CRC calc:%08x, from mem:%08x\n", crc, mem_crc);
	}
	if((dsk->fd < 0) || dsk->raw_data || !dsk->write_through_to_unix) {
		printf("woz_check_file: CRC check done, cannot check fd\n");
		return;
	}

	file_size = (word32)cfg_get_fd_size(dsk->fd);
	if(file_size != (word32)woz_size) {
		halt_printf("woz_size:%08x != file_size %08x\n", woz_size,
								file_size);
		if((word32)woz_size > file_size) {
			woz_size = file_size;
		}
	}
	newwozptr = malloc(woz_size);
	cfg_read_from_fd(dsk->fd, newwozptr, 0, woz_size);

	for(i = 0; i < woz_size; i++) {
		fdval = newwozptr[i];
		memval = wozptr[i];
		if(fdval == memval) {
			continue;
		}
		halt_printf("byte %07x of %07x: mem %02x != %02x fd\n", i,
						woz_size, memval, fdval);
	}

	crcnew = woz_calc_crc32(newwozptr, woz_size, 12);
	free(newwozptr);
	printf("Woz check file complete.  mem %08x vs fd %08x, freed %p\n",
		crc, crcnew, newwozptr);

	bptr = dsk->cur_trk_ptr->raw_bptr;
	printf("dsk->cur_trk_ptr->raw_bptr = %p.  offset:%d\n", bptr,
			(int)(bptr - wozptr));
}

void
woz_parse_meta(Disk *dsk, int offset, int size)
{
	Woz_info *wozinfo_ptr;
	byte	*wozptr, *bptr;
	int	c;
	int	i;

	wozinfo_ptr = dsk->wozinfo_ptr;
	wozptr = wozinfo_ptr->wozptr;
	if(wozinfo_ptr->meta_offset) {
		printf("Bad WOZ file, 2 META chunks\n");
		wozinfo_ptr->woz_size = 0;
		return;
	}
	wozinfo_ptr->meta_offset = offset;
	wozinfo_ptr->meta_size = size;
	printf("META field, %d bytes:\n", size);
	bptr = &(wozptr[offset]);
	for(i = 0; i < size; i++) {
		c = bptr[i];
		if(c == 0) {
			break;
		}
		putchar(c);
	}
	putchar('\n');
}

void
woz_parse_info(Disk *dsk, int offset, int size)
{
	byte	new_buf[36];
	Woz_info *wozinfo_ptr;
	byte	*wozptr, *bptr;
	int	info_version, disk_type, write_protect, synchronized;
	int	cleaned, ram, largest_track;

	wozinfo_ptr = dsk->wozinfo_ptr;
	wozptr = wozinfo_ptr->wozptr;
	if(wozinfo_ptr->info_offset) {
		printf("Two INFO chunks, bad WOZ file\n");
		wozinfo_ptr->woz_size = 0;
		return;
	}
	wozinfo_ptr->info_offset = offset;
	bptr = &(wozptr[offset]);
	if(size < 60) {
		printf("INFO field is %d, too short\n", size);
		wozinfo_ptr->woz_size = 0;
		return;
	}
	info_version = bptr[0];		// Only "1" or "2" is supported
	disk_type = bptr[1];		// 1==5.25", 2=3.5"
	write_protect = bptr[2];	// 1==write protected
	synchronized = bptr[3];		// 1==cross track sync during imaging
	cleaned = bptr[4];		// 1==MC3470 fake bits have been removed
	memcpy(&new_buf[0], &(bptr[5]), 32);
	new_buf[32] = 0;		// Null terminate
	printf("INFO, %d bytes.  info_version:%d, disk_type:%d, wp:%d, sync:"
		"%d, cleaned:%d\n", size, info_version, disk_type,
		write_protect, synchronized, cleaned);
	printf("Creator: %s\n", (char *)&new_buf[0]);
	if(info_version >= 2) {
		ram = bptr[42] + (bptr[43] << 8);
		largest_track = (bptr[44] + (bptr[45] << 8)) * 512;
		printf("Disk sides:%d, boot_format:%d bit_timing:%d, hw:"
			"%02x%02x, ram:%d, largest_track:0x%07x\n", bptr[37],
			bptr[38], bptr[39], bptr[41], bptr[40], ram,
			largest_track);
	}

	if(write_protect) {
		printf("Write protected\n");
		dsk->write_prot = 1;
	}
}

void
woz_parse_tmap(Disk *dsk, int offset, int size)
{
	Woz_info *wozinfo_ptr;
	byte	*bptr, *wozptr;
	int	i;

	wozinfo_ptr = dsk->wozinfo_ptr;
	wozptr = wozinfo_ptr->wozptr;
	if(wozinfo_ptr->tmap_offset) {
		printf("Second TMAP chunk, bad WOZ file!\n");
		wozinfo_ptr->woz_size = 0;
		return;
	}
	wozinfo_ptr->tmap_offset = offset;
	printf("TMAP field, %d bytes\n", size);
	bptr = &(wozptr[offset]);
	for(i = 0; i < 40; i++) {
		printf("Track %2d.00: %02x, %2d.25:%02x %2d.50:%02x %2d.75:"
			"%02x\n", i, bptr[0], i, bptr[1],
			i, bptr[2], i, bptr[3]);
		bptr += 4;
	}
}

void
woz_parse_trks(Disk *dsk, int offset, int size)
{
	Woz_info *wozinfo_ptr;

	printf("TRKS field, %d bytes, offset: %d\n", size, offset);
	wozinfo_ptr = dsk->wozinfo_ptr;
	if(wozinfo_ptr->trks_offset) {
		printf("Second TRKS chunk, illegal Woz file\n");
		wozinfo_ptr->woz_size = 0;
		return;
	}
	wozinfo_ptr->trks_offset = offset;
	wozinfo_ptr->trks_size = size;
}

int
woz_add_track(Disk *dsk, int qtr_track, word32 tmap, dword64 dfcyc)
{
	Woz_info *wozinfo_ptr;
	Trk	*trk;
	byte	*wozptr, *sync_ptr, *bptr;
	word32	raw_bytes, num_bytes, trks_size, len_bits, offset, num_blocks;
	word32	block;
	int	trks_offset;
	int	i;

	wozinfo_ptr = dsk->wozinfo_ptr;
	wozptr = wozinfo_ptr->wozptr;
	trks_offset = wozinfo_ptr->trks_offset;
	trks_size = wozinfo_ptr->trks_size;
	if(wozinfo_ptr->version == 1) {
		offset = tmap * 6656;
		if((offset + 6656) > trks_size) {
			printf("Trk %d is out of range!\n", tmap);
			return 0;
		}

		offset = trks_offset + offset;
		bptr = &(wozptr[offset]);
		len_bits = bptr[6648] | (bptr[6649] << 8);
		if(len_bits > (6656*8)) {
			printf("Trk bits: %d too big\n", len_bits);
			return 0;
		}
	} else {
		bptr = &(wozptr[trks_offset + (tmap * 8)]);
		// This is a TRK 8-byte structure
		block = cfg_get_le16(&bptr[0]);		// Starting Block
		num_blocks = cfg_get_le16(&bptr[2]);	// Block Count
		len_bits = cfg_get_le32(&bptr[4]);	// Bits Count
#if 0
		printf("qtr_track:%02x, block:%04x, num_blocks:%04x, "
			"len_bits:%06x\n", qtr_track, block, num_blocks,
			len_bits);
		printf("File offset: %05lx\n", bptr - wozinfo_ptr->wozptr);
#endif

		if(block < 3) {
			printf("block %04x is < 3\n", block);
			return 0;
		}
		offset = (block * 512);		// Offset from wozptr
		if((offset + (num_blocks * 512)) > (trks_size + trks_offset)) {
			printf("Trk %d is out of range!\n", tmap);
			return 0;
		}
		bptr = &(wozptr[offset]);
#if 0
		printf("Qtr_track %03x offset:%06x, bptr:%p, trks_bptr:%p\n",
			qtr_track, offset, bptr, trks_bptr);
		printf(" len_bits:%d %06x bptr-wozptr: %07lx\n", len_bits,
				len_bits, bptr - wozinfo_ptr->wozptr);
#endif
		if(len_bits > (num_blocks * 512 * 8)) {
			printf("Trk bits: %d too big\n", len_bits);
			return 0;
		}
	}

	dsk->raw_bptr_malloc = 0;

	raw_bytes = (len_bits + 7) >> 3;
	num_bytes = raw_bytes + 8;
	trk = &(dsk->trks[qtr_track]);
	trk->raw_bptr = bptr;
	trk->dunix_pos = offset;
	trk->unix_len = raw_bytes;
	trk->dirty = 0;
	trk->track_bits = len_bits;
#if 0
	printf("track %d.%d dunix_pos:%08llx\n", qtr_track >> 2,
				(qtr_track & 3) * 25, trk->dunix_pos);
#endif
	trk->sync_ptr = (byte *)malloc(num_bytes);

	dsk->cur_trk_ptr = 0;
	iwm_move_to_ftrack(dsk, qtr_track << 16, 0, dfcyc);
	sync_ptr = &(trk->sync_ptr[0]);
	for(i = 0; i < (int)raw_bytes; i++) {
		sync_ptr[i] = 0xff;
	}

	iwm_recalc_sync_from(dsk, qtr_track, 0, dfcyc);

	if(qtr_track == 0) {
		printf("Track 0 data begins: %02x %02x %02x, offset:%d\n",
			bptr[0], bptr[1], bptr[2], offset);
	}
	for(i = 0; i < qtr_track; i++) {
		trk = &(dsk->trks[i]);
		if(trk->track_bits && (trk->raw_bptr == bptr)) {
			// Multiple tracks point to the same woz track
			// This is not allowed, reparse
			wozinfo_ptr->reparse_needed = 1;
			printf("Track %04x matchs track %04x, reparse needed\n",
							qtr_track, i);
			break;
		}
	}

	return 1;
}

int
woz_parse_header(Disk *dsk)
{
	Woz_info *wozinfo_ptr;
	byte	*wozptr, *bptr;
	word32	chunk_id, size, woz_size;
	int	pos, version;
	int	i;

	wozinfo_ptr = dsk->wozinfo_ptr;
	wozptr = wozinfo_ptr->wozptr;
	woz_size = wozinfo_ptr->woz_size;
	version = 2;
	if(woz_size < 8) {
		return 0;
	}
	for(i = 0; i < 8; i++) {
		if(wozptr[i] != g_woz_hdr_bytes[i]) {
			if(i == 3) {		// Check for WOZ1
				if(wozptr[i] == 0x31) {		// WOZ1
					version = 1;
					continue;
				}
			}
			printf("WOZ header[%d]=%02x, invalid\n", i, wozptr[i]);
			return 0;
		}
	}
	wozinfo_ptr->version = version;

	printf("WOZ version: %d\n", version);
	pos = 12;
	while(pos < (int)woz_size) {
		bptr = &(wozptr[pos]);
		chunk_id = bptr[0] | (bptr[1] << 8) | (bptr[2] << 16) |
				(bptr[3] << 24);
		size = bptr[4] | (bptr[5] << 8) | (bptr[6] << 16) |
							(bptr[7] << 24);
		pos += 8;
		printf("chunk_id: %08x, size:%08x\n", chunk_id, size);
		if(((pos + size) > woz_size) || (size < 8) ||
							((size >> 30) != 0)) {
			return 0;
		}
		bptr = &(wozptr[pos]);
		if(chunk_id == 0x4f464e49) {		// "INFO"
			woz_parse_info(dsk, pos, size);
		} else if(chunk_id == 0x50414d54) {	// "TMAP"
			woz_parse_tmap(dsk, pos, size);
		} else if(chunk_id == 0x534b5254) {	// "TRKS"
			woz_parse_trks(dsk, pos, size);
		} else if(chunk_id == 0x4154454d) {	// "META"
			woz_parse_meta(dsk, pos, size);
		} else {
			printf("Chunk header %08x is unknown\n", chunk_id);
		}
		pos += size;
	}

	return 1;		// Good so far
}

Woz_info *
woz_malloc(byte *wozptr, word32 woz_size)
{
	Woz_info *wozinfo_ptr;

	wozinfo_ptr = malloc(sizeof(Woz_info));
	printf("malloc wozinfo_ptr:%p\n", wozinfo_ptr);
	if(!wozinfo_ptr) {
		fprintf(stderr, "Out of memory\n");
		exit(1);
	}

	wozinfo_ptr->wozptr = wozptr;
	wozinfo_ptr->woz_size = woz_size;
	wozinfo_ptr->version = 0;
	wozinfo_ptr->reparse_needed = 0;
	wozinfo_ptr->max_trk_blocks = 0;
	wozinfo_ptr->meta_size = 0;
	wozinfo_ptr->trks_size = 0;
	wozinfo_ptr->tmap_offset = 0;
	wozinfo_ptr->trks_offset = 0;
	wozinfo_ptr->info_offset = 0;
	wozinfo_ptr->meta_offset = 0;

	if(woz_size < 12) {
		if(wozptr) {
			free(wozptr);
		}
		wozptr = 0;
	}
	if(!wozptr) {
		wozptr = malloc(12);
		woz_append_bytes(wozptr, &g_woz_hdr_bytes[0], 12);
		wozinfo_ptr->wozptr = wozptr;
		wozinfo_ptr->woz_size = 12;
	}

	return wozinfo_ptr;
}

int
woz_reopen(Disk *dsk, dword64 dfcyc)
{
	byte	act_tmap[160];
	Woz_info *wozinfo_ptr;
	byte	*wozptr, *tmap_bptr;
	word32	tmap, prev_tmap, last_act;
	int	ret, num_tracks, num_match;
	int	i, j;

	wozinfo_ptr = dsk->wozinfo_ptr;
	wozptr = wozinfo_ptr->wozptr;

	if(!wozinfo_ptr->tmap_offset) {
		printf("No TMAP found\n");
		return 0;
	}
	if(!wozinfo_ptr->trks_offset) {
		printf("No TRKS found\n");
		return 0;
	}
	if(!wozinfo_ptr->info_offset) {
		printf("No INFO found\n");
		return 0;
	}
	if(wozinfo_ptr->woz_size == 0) {
		printf("woz_size is 0!\n");
		return 0;
	}

	tmap_bptr = wozptr + wozinfo_ptr->tmap_offset;
	dsk->cur_fbit_pos = 0;
	num_tracks = 4*35;
	for(i = num_tracks; i < 160; i++) {
		// See what the largest track is, go to track 39.50
		if(tmap_bptr[i] != 0xff) {
			num_tracks = i + 1;
			//printf("Will set num_tracks=%d\n", num_tracks);
		}
	}
	dsk->fbit_mult = 128;		// 5.25" multipler value
	if(!dsk->disk_525) {
		num_tracks = 160;
		dsk->fbit_mult = 256;	// 3.5" multipler value
	}
	disk_set_num_tracks(dsk, num_tracks);
	for(i = 0; i < 160; i++) {
		act_tmap[i] = 0xff;
	}
	for(i = 0; i < 160; i++) {
		tmap = tmap_bptr[i];
		if(tmap >= 0xff) {
			continue;		// Skip
		}
		// WOZ format adds dup entries for adjacent qtr tracks, so
		//  track 2.0 has entries at 1.75 and 2.25 pointing to 2.0.
		// KEGS doesn't want these, it handles this itself, so remove
		//  them.
		if(dsk->disk_525) {
			num_match = 1;
			for(j = i + 1; j < 160; j++) {
				if(tmap_bptr[j] != tmap) {
					break;
				}
				num_match++;
			}
			// From i, num_match is the number of time tmap repeats
			prev_tmap = 0xff;
			last_act = 0xff;
			if(i) {
				prev_tmap = tmap_bptr[i - 1];
				last_act = act_tmap[i - 1];
			} else if(num_match == 3) {
				// Weird case where WOZ starts with 0,0,0, we
				//  should treat track 0.25 as the real track
				continue;
			}
			if(num_match >= 3) {
				// long run of repeats, add a track every other
				//  one.
				if(last_act == tmap) {		// Just did trk
					continue;
				}
				if(prev_tmap != tmap) {		// Start of run
					continue;
				}
			} else if(num_match == 2) {
				// Handle A, B, [B], B, C and A, B, B, [B], B, C
				// ALWAYS add this track
			} else {			// num_match==1
				if(prev_tmap == tmap) {
					continue;
				}
				// Otherwise, a lone track, always add it
			}
		}
		ret = woz_add_track(dsk, i, tmap, dfcyc);
		if(ret == 0) {
			printf("woz_add_track i:%04x tmap:%04x ret 0\n", i,
								tmap);
			return ret;
		}
	}

	return 1;		// WOZ file is good!
}

int
woz_open(Disk *dsk, dword64 dfcyc)
{
	Woz_info *wozinfo_ptr;
	byte	*wozptr;
	dword64	doff;
	word32	woz_size, crc, file_crc;
	int	fd, ret;

	// return 0 for bad WOZ file, 1 for success
	// We set dsk->wozinfo_ptr, and caller will free it if we return 0
	printf("woz_open on file %s, write_prot:%d\n", dsk->name_ptr,
						dsk->write_prot);
	if(dsk->trks == 0) {
		return 0;		// Smartport?
	}
	if(dsk->raw_data) {
		wozptr = dsk->raw_data;
		woz_size = (word32)dsk->raw_dsize;
		dsk->write_prot = 1;
		dsk->write_through_to_unix = 0;
	} else {
		fd = dsk->fd;
		if(fd < 0) {
			return 0;
		}
		woz_size = (word32)cfg_get_fd_size(fd);
		printf("size: %d\n", woz_size);

		wozptr = malloc(woz_size);
		doff = cfg_read_from_fd(fd, wozptr, 0, woz_size);
		if(doff != woz_size) {
			close(fd);
			return 0;
		}
	}

	wozinfo_ptr = woz_malloc(wozptr, woz_size);
	dsk->wozinfo_ptr = wozinfo_ptr;

	if(woz_size < 16) {
		return 0;
	}
	crc = woz_calc_crc32(wozptr, woz_size, 12);
	file_crc = wozptr[8] | (wozptr[9] << 8) | (wozptr[10] << 16) |
							(wozptr[11] << 24);
	if((crc != file_crc) && (file_crc != 0)) {
		printf("Bad Woz CRC:%08x in file, calc:%08x\n", file_crc, crc);
		return 0;
	}

	ret = woz_parse_header(dsk);
	printf("woz_parse_header ret:%d, write_prot:%d\n", ret,
							dsk->write_prot);
	if(ret == 0) {
		return ret;
	}

	ret = woz_reopen(dsk, dfcyc);
	printf("woz_reopen ret:%d\n", ret);

	woz_maybe_reparse(dsk);
	return ret;
}

byte *
woz_append_bytes(byte *wozptr, byte *in_bptr, int len)
{
	int	i;

	for(i = 0; i < len; i++) {
		*wozptr++ = *in_bptr++;
	}

	return wozptr;
}

byte *
woz_append_word32(byte *wozptr, word32 val)
{
	int	i;

	for(i = 0; i < 4; i++) {
		*wozptr++ = val & 0xff;
		val = val >> 8;
	}

	return wozptr;
}

int
woz_append_chunk(Woz_info *wozinfo_ptr, word32 chunk_id, word32 length,
							byte *bptr)
{
	byte	*wozptr, *new_wozptr, *save_wozptr;
	word32	woz_size, new_size;
	int	offset;

	wozptr = wozinfo_ptr->wozptr;
	woz_size = wozinfo_ptr->woz_size;

	new_size = woz_size + 4 + 4 + length;
	new_wozptr = realloc(wozptr, new_size);
	if(new_wozptr == 0) {
		return 0;
	}
	wozptr = new_wozptr + woz_size;
	wozptr = woz_append_word32(wozptr, chunk_id);
	save_wozptr = woz_append_word32(wozptr, length);
	wozptr = woz_append_bytes(save_wozptr, bptr, length);

	wozinfo_ptr->wozptr = new_wozptr;
	wozinfo_ptr->woz_size = new_size;
	offset = (int)(save_wozptr - new_wozptr);
	switch(chunk_id) {
	case 0x4f464e49:		// "INFO"
		wozinfo_ptr->info_offset = offset;
		break;
	case 0x50414d54:		// "TMAP"
		wozinfo_ptr->tmap_offset = offset;
		break;
	case 0x534b5254:		// "TRKS"
		wozinfo_ptr->trks_offset = offset;
		wozinfo_ptr->trks_size = new_size;
		break;
	}
	if(wozptr != (new_wozptr + new_size)) {
		halt_printf("wozptr:%p != %p + %08x\n", wozptr, new_wozptr,
								new_size);
		return 0;
	}

	return 1;
}

byte *
woz_append_a_trk(Woz_info *wozinfo_ptr, Disk *dsk, int trk_num, byte *bptr,
		word32 *num_blocks_ptr, dword64 *tmap_dptr)
{
	byte	*new_bptr;
	word32	num_blocks, blocks_start, this_blocks, size_bytes, track_bits;
	int	i;

	// Align trks_buf_size to 512 bytes
	blocks_start = *num_blocks_ptr;

	track_bits = dsk->trks[trk_num].track_bits;
	size_bytes = (dsk->trks[trk_num].track_bits + 7) >> 3;
	this_blocks = (size_bytes + 511) >> 9;
	if(wozinfo_ptr->max_trk_blocks < this_blocks) {
		wozinfo_ptr->max_trk_blocks = this_blocks;
		printf("max_trk_blocks=%d from trk %03x\n", this_blocks,
								trk_num);
	}

	num_blocks = blocks_start + this_blocks;
	*num_blocks_ptr = num_blocks;

	*tmap_dptr = (((dword64)track_bits) << 32) | (this_blocks << 16) |
								blocks_start;

	new_bptr = realloc(bptr, num_blocks << 9);
	if(new_bptr == 0) {
		return 0;
	}
	// Zero out last 512 byte block, to ensure a partial track has all 0's
	//  in the last block
	for(i = 0; i < 512; i++) {
		new_bptr[(num_blocks << 9) - 512 + i] = 0;
	}
	woz_append_bytes(new_bptr + (blocks_start << 9),
			dsk->trks[trk_num].raw_bptr, size_bytes);

	return new_bptr;
}

Woz_info *
woz_new_from_woz(Disk *dsk, int disk_525)
{
	dword64	tmap_dvals[160];
	int	tmap_qtrk[160];
	byte	buf[160];
	Woz_info *wozinfo_ptr, *in_wozinfo_ptr;
	Trk	*trkptr;
	byte	*in_wozptr, *wozptr, *trks_bufptr;
	dword64	dval;
	word32	type, woz_size, num_blocks, crc, track_bits;
	int	c, num_valid_tmap, pos, offset, raw_bytes;
	int	i, j;

	wozinfo_ptr = woz_malloc(0, 0);		// New wozinfo
	in_wozptr = 0;
	in_wozinfo_ptr = 0;
	if(dsk) {
		in_wozinfo_ptr = dsk->wozinfo_ptr;
		if(!in_wozinfo_ptr) {
			halt_printf("Changing to WOZ format!\n");
		}
	}
	if(in_wozinfo_ptr) {
		in_wozptr = in_wozinfo_ptr->wozptr;
	}
	printf(" START woz_new_from_woz, in_wozinfo_ptr:%p, in_wozptr:%p\n",
					in_wozinfo_ptr, in_wozptr);
	for(i = 0; i < 60; i++) {
		buf[i] = 0;
	}
	if(in_wozptr) {
		// Output the INFO chunk
		memcpy(&buf[0], &in_wozptr[20], 60);
	} else {
		// Output an INFO chunk for KEGS
		buf[3] = 1;			// 1=synchronized tracks
		buf[4] = 1;			// 1=MC3470 fake bits removed
		for(i = 0; i < 32; i++) {
			buf[5 + i] = ' ';			// Creator field
		}
		memcpy(&buf[5], "KEGS", 4);
		// And put the revision number after it
		for(i = 0; i < 20; i++) {
			c = g_kegs_version_str[i];
			if(c == 0) {
				break;
			}
			buf[5 + 5 + i] = c;
		}
	}
	buf[0] = 2;			// WOZ2 version
	type = 2 - disk_525;		// 1=5.25, 2=3.5
	buf[1] = type;
	buf[2] = 0;					// write_prot=0
	if(buf[37] == 0) {
		buf[37] = type;				// sides, re-use type
	}
	if(buf[39] == 0) {
		buf[39] = 16 + 16 * (type & 1);		// 5.25=32, 3.5=16
	}
	woz_append_chunk(wozinfo_ptr, 0x4f464e49, 60, &buf[0]);		// INFO

	// TMAP
	for(i = 0; i < 160; i++) {
		buf[i] = 0xff;
	}
	num_blocks = 6;			// 1280 + 256 = 6x512
	trks_bufptr = malloc(num_blocks << 9);
	printf("trk_bufptr = %p\n", trks_bufptr);
	for(i = 0; i < (int)(num_blocks << 9); i++) {
		trks_bufptr[i] = 0;
	}
	num_valid_tmap = 0;
	if((dsk != 0) && (dsk->trks != 0)) {
		// Output all valid tracks, and set the TMAP for adjacent .25
		for(i = 0; i < 160; i++) {
			trkptr = &(dsk->trks[i]);
			if(trkptr->track_bits >= 10000) {
				buf[i] = num_valid_tmap;
				if(i && (buf[i-1] == 0xff)) {
					buf[i-1] = num_valid_tmap;
				}
				if((i < 159) && (trkptr[1].track_bits < 10000)){
					buf[i+1] = num_valid_tmap;
				}
				trks_bufptr = woz_append_a_trk(wozinfo_ptr, dsk,
					i, trks_bufptr, &num_blocks,
					&tmap_dvals[num_valid_tmap]);
				tmap_qtrk[num_valid_tmap] = i;
				printf("Did append, tmap:%04x qtrk:%04x dval:"
					"%016llx\n", num_valid_tmap, i,
					tmap_dvals[num_valid_tmap]);
				num_valid_tmap++;
			}
		}
	}
	woz_append_chunk(wozinfo_ptr, 0x50414d54, 160, &buf[0]);	// TMAP

	// TRKS.  Already all 0 if there are no tracks.  Fill in tmap_dvals[]
	for(i = 0; i < num_valid_tmap; i++) {
		pos = 256 + i*8;
		dval = tmap_dvals[i];
		for(j = 0; j < 8; j++) {
			trks_bufptr[pos + j] = dval & 0xff;
			dval = dval >> 8;
		}
	}
	woz_append_chunk(wozinfo_ptr, 0x534b5254, (num_blocks << 9) - 256,
						trks_bufptr + 256);
						// TRKS, 1280 minimum size
	free(trks_bufptr);

	// Final META chunk...just remove, we've added or removed tracks from
	//  an original WOZ image, so the Meta data is no longer accurate

	wozptr = wozinfo_ptr->wozptr;
	woz_size = wozinfo_ptr->woz_size;

	printf(" new wozptr:%p, woz_size:%08x\n", wozptr, woz_size);
	wozptr[64] = wozinfo_ptr->max_trk_blocks;	// largest track

	crc = woz_calc_crc32(wozptr, woz_size, 12);
	cfg_set_le32(&wozptr[8], crc);
	if(dsk) {
		pos = 0;
		for(i = 0; i < 160; i++) {
			// Go through and fix up trks structure to match new WOZ
			trkptr = &(dsk->trks[i]);
			if((pos < num_valid_tmap) && (tmap_qtrk[pos] == i)) {
				// This is a valid track
				dval = tmap_dvals[pos];
				track_bits = dval >> 32;
				offset = (dval & 0xffff) << 9;

				raw_bytes = (track_bits + 7) >> 3;
				if(trkptr->unix_len == 0) {
					free(trkptr->raw_bptr);
				}
				trkptr->raw_bptr = &wozptr[offset];
				trkptr->dunix_pos = offset;
				trkptr->unix_len = raw_bytes;
				trkptr->dirty = 0;
				trkptr->track_bits = track_bits;
				if(trkptr->sync_ptr == 0) {
					halt_printf("sync_ptr 0 qtrk:%04x\n",
									i);
				}
				pos++;
			} else {
				// No longer a valid track, free any ptrs
				if(dsk->raw_bptr_malloc) {
					free(trkptr->raw_bptr);
				}
				free(trkptr->sync_ptr);
				trkptr->raw_bptr = 0;
				trkptr->sync_ptr = 0;
				trkptr->dunix_pos = 0;
				trkptr->unix_len = 0;
				trkptr->dirty = 0;
				trkptr->track_bits = 0;
			}
		}
		dsk->raw_bptr_malloc = 0;

		if(dsk->raw_data) {
			free(dsk->raw_data);
			dsk->raw_data = wozptr;
			dsk->raw_dsize = woz_size;
			dsk->dimage_start = 0;
			dsk->dimage_size = woz_size;
			in_wozptr = 0;
		}
		free(in_wozptr);
		free(in_wozinfo_ptr);
		if(in_wozinfo_ptr == 0) {
			dsk->write_through_to_unix = 0;
			halt_printf("Force write_through_to_unix since image "
				"changed to WOZ format\n");
		}
	}

	printf(" END woz_new_from_woz, wozinfo_ptr:%p wozptr:%p\n",
					wozinfo_ptr, wozinfo_ptr->wozptr);
	return wozinfo_ptr;
}

int
woz_new(int fd, const char *str, int size_kb)
{
	Woz_info *wozinfo_ptr;
	byte	*wozptr;
	word32	size, woz_size;
	int	disk_525;

	disk_525 = 0;
	if(size_kb <= 140) {
		disk_525 = 1;
	}
	wozinfo_ptr = woz_new_from_woz(0, disk_525);

	wozptr = wozinfo_ptr->wozptr;
	woz_size = wozinfo_ptr->woz_size;


	size = (word32)must_write(fd, wozptr, woz_size);
	free(wozptr);
	free(wozinfo_ptr);
	if(size != woz_size) {
		return -1;
	}
	if(str) {
		// Avoid unused var warning
	}

	return 0;
}

void
woz_maybe_reparse(Disk *dsk)
{
	Woz_info *wozinfo_ptr;

	wozinfo_ptr = dsk->wozinfo_ptr;
	if(wozinfo_ptr) {
		if(wozinfo_ptr->reparse_needed) {
			woz_reparse_woz(dsk);
		}
	}
}

void
woz_set_reparse(Disk *dsk)
{
	Woz_info *wozinfo_ptr;

	wozinfo_ptr = dsk->wozinfo_ptr;
	if(wozinfo_ptr) {
		wozinfo_ptr->reparse_needed = 1;
	} else {
		woz_reparse_woz(dsk);
	}
}

void
woz_reparse_woz(Disk *dsk)
{
	Woz_info *wozinfo_ptr;

#if 0
	printf("In woz_reparse_woz, showing track 0\n");

	iwm_show_a_track(dsk, &(dsk->trks[0]), 0.0);
	iwm_show_stats(3);
#endif

	wozinfo_ptr = woz_new_from_woz(dsk, dsk->disk_525);
		// This wozinfo_ptr has reparse_needed==0

	dsk->wozinfo_ptr = wozinfo_ptr;
	if(!dsk->raw_data && dsk->write_through_to_unix) {
		(void)!ftruncate(dsk->fd, wozinfo_ptr->woz_size);
		(void)cfg_write_to_fd(dsk->fd, wozinfo_ptr->wozptr, 0,
							wozinfo_ptr->woz_size);
		printf("did ftruncate and write of WOZ to %s\n", dsk->name_ptr);
	}

	// Need to recalculate dsk->cur_track_bits, cur_trk_ptr
	dsk->cur_trk_ptr = 0;
	iwm_move_to_ftrack(dsk, dsk->cur_frac_track, 0, 0);

#if 0
	printf("End of woz_reparse_woz, showing track 0\n");
	iwm_show_a_track(dsk, &(dsk->trks[0]), 0.0);
	iwm_show_stats(3);
#endif

	woz_check_file(dsk);
	printf("woz_reparse_woz complete!\n");
}

void
woz_remove_a_track(Disk *dsk, word32 qtr_track)
{
	Trk	*trkptr;

	printf("woz_remove_track: %s qtr_track:%03x\n", dsk->name_ptr,
								qtr_track);
	trkptr = &(dsk->trks[qtr_track]);
	trkptr->track_bits = 0;		// Track invalid

	woz_set_reparse(dsk);
}

word32
woz_add_a_track(Disk *dsk, word32 qtr_track)
{
	Trk	*trkptr, *other_trkptr;
	byte	*bptr, *other_bptr, *sync_ptr, *other_sync_ptr;
	word32	track_bits, val;
	int	raw_bytes;
	int	i;

	// Return track_bits for the new track

	trkptr = &(dsk->trks[qtr_track]);

	other_trkptr = 0;
	if((qtr_track > 0) && (trkptr[-1].track_bits > 0)) {
		// Copy this track
		other_trkptr = trkptr - 1;
	} else if((qtr_track < 159) && (trkptr[1].track_bits > 0)) {
		other_trkptr = trkptr + 1;
	}
	other_trkptr = 0;		// HACK
	if(dsk->disk_525 && other_trkptr) {
		// We're .25 tracks away from a valid track, copy it's data
		track_bits = other_trkptr->track_bits;
		raw_bytes = (track_bits + 7) >> 3;
		trkptr->track_bits = track_bits;
		trkptr->raw_bptr = malloc(raw_bytes + 8);
		trkptr->sync_ptr = malloc(raw_bytes + 8);
		printf(" add a track, copy bptr:%p sync_ptr:%p size:%08x\n",
			trkptr->raw_bptr, trkptr->sync_ptr, raw_bytes + 8);
		bptr = trkptr->raw_bptr;
		sync_ptr = trkptr->sync_ptr;
		other_bptr = other_trkptr->raw_bptr;
		other_sync_ptr = other_trkptr->sync_ptr;
		for(i = 0; i < raw_bytes; i++) {
			bptr[i] = other_bptr[i];
			sync_ptr[i] = other_sync_ptr[i];
		}
	} else {
		track_bits = iwm_get_default_track_bits(dsk, qtr_track);
		raw_bytes = (track_bits + 7) >> 3;
		trkptr->track_bits = track_bits;
		trkptr->raw_bptr = malloc(raw_bytes + 8);
		trkptr->sync_ptr = malloc(raw_bytes + 8);
		printf(" add a track, raw_bptr:%p sync_ptr:%p size:%08x\n",
			trkptr->raw_bptr, trkptr->sync_ptr, raw_bytes + 8);

		bptr = trkptr->raw_bptr;
		sync_ptr = trkptr->sync_ptr;
		for(i = 0; i < raw_bytes; i++) {
			val = ((i >> 6) ^ i) & 0x7f;
			if(((val & 0xf0) == 0) || ((val & 0x0f) == 0)) {
				val |= 0x21;
			}
			bptr[i] = val;
			sync_ptr[i] = 0xff;
		}
		bptr[raw_bytes - 1] = 0;

		iwm_recalc_sync_from(dsk, qtr_track, 0, 0);
	}
	trkptr->dunix_pos = 0;
	trkptr->unix_len = 0;			// Mark as a newly created trk
	trkptr->dirty = 0;

	printf("woz_add_new_track: %s qtr_track:%03x\n", dsk->name_ptr,
								qtr_track);
	woz_set_reparse(dsk);

	return track_bits;
}

