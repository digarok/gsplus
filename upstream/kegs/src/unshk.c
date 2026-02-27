const char rcsid_unshk_c[] = "@(#)$KmKId: unshk.c,v 1.14 2023-05-19 13:57:52+00 kentd Exp $";

/************************************************************************/
/*			KEGS: Apple //gs Emulator			*/
/*			Copyright 2002-2021 by Kent Dickey		*/
/*									*/
/*	This code is covered by the GNU GPL v3				*/
/*	See the file COPYING.txt or https://www.gnu.org/licenses/	*/
/*	This program is provided with no warranty			*/
/*									*/
/*	The KEGS web page is kegs.sourceforge.net			*/
/*	You may contact the author at: kadickey@alumni.princeton.edu	*/
/************************************************************************/

// This code is based on the official NuFX documentation in Apple II FTN.e08002
//  available at: http://nulib.com/library/FTN.e08002.htm.  Andy McFadden has
//  reverse-engineered GSHK (and its quirks) in Nulib, at: http://nulib.com/,
//  and that code was very helpful in getting the basic algorithms correct.

#include "defc.h"

word32
unshk_get_long4(byte *bptr)
{
	word32	val;
	int	i;

	// Get 4 bytes in little-endian form
	val = 0;
	for(i = 3; i >=0 ; i--) {
		val = (val << 8) | bptr[i];
	}
	return val;
}

word32
unshk_get_word2(byte *bptr)
{
	// Get 2 bytes in little-endian form
	return (bptr[1] << 8) | bptr[0];
}

word32
unshk_calc_crc(byte *bptr, int size, word32 start_crc)
{
	word32	crc;
	int	i, j;

	// No table used: do basic CRC operation on size bytes.  For CCITT-16
	//  (the one used in ShrinkIt), xor the byte into the upper 8 bits of
	//  the current crc, then xor in 0x1021 for each '1' bit shifted out
	//  the top.  Use a 32-bit crc variable to get the bit shifted out.
	crc = start_crc & 0xffff;
	for(i = 0; i < size; i++) {
		crc = crc ^ (bptr[i] << 8);
		for(j = 0; j < 8; j++) {
			crc = crc << 1;
			if(crc & 0x10000) {
				crc = crc ^ 0x11021;
				// XOR in 0x1021, and clear bit 16 as well.
			}
		}
		// printf("CRC after [%04x]=%02x is %04x\n", i, bptr[i], crc);
	}
	return crc & 0xffff;
}

int
unshk_unrle(byte *cptr, int len, word32 rle_delim, byte *ucptr)
{
	byte	*start_ucptr;
	word32	c;
	int	outlen, count;
	int	i;

	// RLE is 3 bytes: { 0xdb, char, count}, where count==0 means output
	//  one char.
	start_ucptr = ucptr;
	while(len > 0) {
		c = *cptr++;
		len--;
		if(c == rle_delim) {
			c = *cptr++;
			count = *cptr++;
			len -= 2;
			for(i = 0; i <= count; i++) {
				*ucptr++ = c;
			}
		} else {
			*ucptr++ = c;
		}
	}
	outlen = (int)(ucptr - start_ucptr);
	if(outlen != 0x1000) {
		printf("RLE failed, output %d bytes\n", outlen);
		return 1;
	}
	return 0;
}

void
unshk_lzw_clear(Lzw_state *lzw_ptr)
{
	int	i;

	lzw_ptr->entry = 0x100;			// First expected table pos
	lzw_ptr->bits = 9;
	for(i = 0; i < 256; i++) {
		lzw_ptr->table[i] = i << 12;	// Encodes depth==0 as well
	}
	lzw_ptr->table[0x100] = 0;
}

// LZW Table format in 32-bit word: { depth[11:0], finalc[7:0], code[11:0] }

byte *
unshk_unlzw(byte *cptr, Lzw_state *lzw_ptr, byte *ucptr, word32 uclen)
{
	byte	*end_ucptr, *bptr;
	word32	mask, val, entry, newcode, finalc_code;
	int	bit_pos, depth, bits;

	// This routine handles ShrinkIt LZW/1 and LZW/2 streams.  It expects
	//  the caller has set entry=0x100 and bits=9 at the start of each
	//  LZW/1 chunk

	entry = lzw_ptr->entry;
	bits = lzw_ptr->bits;
	end_ucptr = ucptr + uclen;
	//printf("unlzw block: format:%d, uclen:%04x\n", thread_format, uclen);
	mask = (1 << bits) - 1;
	bit_pos = 0;

	while(ucptr < end_ucptr) {
		newcode = (cptr[2] << 16) | (cptr[1] << 8) | cptr[0];
		newcode = (newcode >> bit_pos) & mask;
		bit_pos += bits;
		cptr += (bit_pos >> 3);
		bit_pos = bit_pos & 7;
		// printf("At entry:%04x, bits:%d newcode:%04x\n", entry, bits,
		//						newcode);
		if((entry + 1) >= mask) {
			bits++;
			mask = (mask << 1) | 1;
			// Note this is one too early, but this is needed to
			//  match ShrinkIt
		}

		// Newcode is up to 12-bits, where <= 0xff means just this
		//  char, and >= 0x101 means chase down that code, output the
		//  character in that table entry, and repeat
		if(newcode == 0x100) {
			// printf("Got clear code\n");
			entry = 0x100;
			bits = 9;
			mask = 0x1ff;
			continue;
		}
		if(newcode > entry) {
			printf("Bad code: %04x, entry:%04x\n", newcode, entry);
			return 0;
		}
#if 0
		if(newcode == entry) {
			// KwKwK case: operate on oldcode
			printf("KwKwK case!\n");
		}
#endif
		finalc_code = newcode;
		depth = lzw_ptr->table[newcode & 0xfff] >> 20;
		// depth will be 0 for 1 character, 1 for 2 characters, etc.
		bptr = ucptr + depth;
		while(bptr >= ucptr) {
			finalc_code = lzw_ptr->table[finalc_code & 0xfff];
			*bptr-- = (finalc_code >> 12) & 0xff;
		}
		val = lzw_ptr->table[entry];
		lzw_ptr->table[entry] = (val & (~0xff000)) |
						(finalc_code & 0xff000);
			// [entry] has code from last iteration (which stuck in
			//  the last finalc char, which we need to toss now),
			//  and update it with the correct finalc character.
		// printf("Table[%04x]=%08x\n", entry, lzw_ptr->table[entry]);

		depth++;
		ucptr += depth;
#if 0
		bptr = ucptr - depth;
		printf("src:%04x, out+%06x: ", (int)(cptr - cptr_start),
				(int)(ucptr - depth - (end_ucptr - uclen)));
		for(i = 0; i < depth; i++) {
			printf(" %02x", *bptr++);
		}
		printf("\n");
#endif
		lzw_ptr->table[entry + 1] = (depth << 20) |
					(finalc_code & 0xff000) | newcode;
			// Set tab[entry+1] for KwKwK case, with this newcode,
			//  and this finalc character.  This also saves this
			//  newcode when the next code is received.
		entry++;
	}
	lzw_ptr->entry = entry;
	lzw_ptr->bits = bits;
	if(bit_pos) {			// We used part of this byte, use it
		cptr++;
	}
	return cptr;
}

void
unshk_data(Disk *dsk, byte *cptr, word32 compr_size, byte *ucptr,
		word32 uncompr_size, word32 thread_format, byte *base_cptr)
{
	Lzw_state lzw_state;
	byte	*end_cptr, *end_ucptr, *rle_inptr;
	word32	rle_delim, len, use_lzw, lzw_len, crc, chunk_crc;
	int	ret;
	int	i;

	printf("Uncompress %d compress bytes into %d bytes, source offset:"
		"%08x\n", compr_size, uncompr_size, (word32)(cptr - base_cptr));

	// LZW/1 format: crc_lo, crc_hi, vol, rle_delim then start the chunk
	//	each chunk: rle_len_lo, rle_len_hi, lzw_used
	// LZW/2 format: vol, rle_delim then start the chunk
	//	each chunk: rle_len_lo, rle_len_hi, lzw_len_lo, lzw_len_hi
	//	where rle_len_hi[7]==1 means LZW was used
	end_cptr = cptr + compr_size;
	end_ucptr = ucptr + uncompr_size;

	chunk_crc = 0;
	if(thread_format != 3) {		// LZW/1
		chunk_crc = (cptr[1] << 8) | cptr[0];
		cptr += 2;			// Skip over CRC bytes
	}
	dsk->vol_num = cptr[0];		// LZW/1
	rle_delim = cptr[1];
	cptr += 2;
	unshk_lzw_clear(&lzw_state);

	// printf("vol_num:%02x, rle_delim:%02x\n", dsk->vol_num, rle_delim);

	// LZW/1 format for each chunk: len_lo, len_hi, use_lzw
	// LZW/2 format for each chunk: len_lo, len_hi.  If len_hi[7]=1, then
	//	two more bytes: lzw_len_lo, lzw_len_hi
	while(cptr < (end_cptr - 4)) {
		if(ucptr >= end_ucptr) {
			break;
		}
		len = (cptr[1] << 8) | cptr[0];
#if 0
		printf("chunk at +%08x, len:%04x, dest offset:%08x\n",
				(word32)(cptr - base_cptr), len,
				(word32)(ucptr - (end_ucptr - uncompr_size)));
#endif
		cptr += 2;
		use_lzw = (len >> 15) & 1;
		if(len & 0x6000) {
			printf("Illegal length: %04x\n", len);
			return;				// Ilegal length
		}
		len = len & 0x1fff;
		lzw_len = 0;
		if(thread_format == 3) {		// LZW/2
			if(use_lzw) {
				lzw_len = (cptr[1] << 8) | cptr[0];
				if(lzw_len > 0x1004) {
					printf("Bad lzw_len: %04x\n", lzw_len);
					return;		// Illegal
				}
				cptr += 2;
				lzw_len -= 4;		// Counts from [-4]
			}
		} else {				// LZW/1
			use_lzw = *cptr++;
			if(use_lzw >= 2) {
				printf("Bad use_lzw:%02x\n", use_lzw);
				return;			// Bad format
			}
		}
		rle_inptr = cptr;
		if(use_lzw) {
			//printf("lzw on %02x.%02x.%02x.., %d bytes (rle:%d)\n",
			//	cptr[0], cptr[1], cptr[2], lzw_len, len);
			rle_inptr = ucptr;
			if(len != 0x1000) {
				// RLE pass is needed: Write to ucptr+0x1000,
				//  and then UnRLE down to ucptr;
				rle_inptr = ucptr + 0x1000;
			}
			cptr = unshk_unlzw(cptr, &lzw_state, rle_inptr, len);
			if(cptr == 0) {
				printf("Bad LZW stream\n");
				return;
			}
			if(thread_format != 3) {		// LZW/1
				lzw_state.entry = 0x100;	// Reset table
				lzw_state.bits = 9;
			}
		} else {
			lzw_state.entry = 0x100;	// Reset table
			lzw_state.bits = 9;
		}
		if(len != 0x1000) {
			// printf("RLE on %02x.%02x.%02x... %d bytes\n",
			//	cptr[0], cptr[1], cptr[2], len);
			ret = unshk_unrle(rle_inptr, len, rle_delim, ucptr);
			if(ret) {
				printf("unRLE failed\n");
				return;
			}
			if(!use_lzw) {
				cptr += len;
			}
		} else if(!use_lzw) {
			// Uncompressed
			// printf("Uncompressed %02x.%02x.%02x....%d bytes\n",
			//	cptr[0], cptr[1], cptr[2], len);
			for(i = 0; i < 0x1000; i++) {
				ucptr[i] = *cptr++;
			}
		}
		// write(g_out_fd, ucptr, 0x1000);
		ucptr += 0x1000;
	}

	printf("cptr:%p, end_cptr:%p, uncompr_size:%08x\n", cptr, end_cptr,
								uncompr_size);
	if(thread_format != 3) {		// LZW/1
		crc = unshk_calc_crc(ucptr - uncompr_size, uncompr_size, 0);
		//printf("LZW/1 calc CRC %04x vs CRC %04x\n", crc, chunk_crc);
		if(crc != chunk_crc) {
			printf("Bad LZW/1 CRC: %04x != %04x\n", crc, chunk_crc);
			return;
		}
	}
	dsk->fd = 0;
}

void
unshk_parse_header(Disk *dsk, byte *cptr, int compr_size, byte *base_cptr)
{
	byte	*cptr_end, *dptr, *ucptr;
	word32	total_records, attrib_count, total_threads, thread_class;
	word32	thread_format, thread_kind, thread_eof, comp_thread_eof;
	word32	thread_crc, crc, version, disk_size, block_size, num_blocks;
	word32	filename_length;
	int	i;

	cptr_end = cptr + compr_size;
	if(compr_size < 0xa0) {
		printf("Didn't read everything\n");
		return;
	}

	// Parse NuFX format: "NuFile" with alternating high bits
	if((cptr[0] != 0x4e) || (cptr[1] != 0xf5) || (cptr[2] != 0x46) ||
		(cptr[3] != 0xe9) || (cptr[4] != 0x6c) || (cptr[5] != 0xe5)) {
		printf("Not NuFile, exiting\n");
		return;
	}
	total_records = unshk_get_long4(&cptr[8]);
	if(total_records < 1) {
		return;
	}

	// Master Header to NuFile is apparently 48 bytes.  Look for "NuFX"
	//  Header to describe threads
	cptr += 0x30;
	if((cptr[0] != 0x4e) || (cptr[1] != 0xf5) || (cptr[2] != 0x46) ||
							(cptr[3] != 0xd8)) {
		return;
	}
	attrib_count = unshk_get_word2(&cptr[6]);
	version = unshk_get_word2(&cptr[8]);		// >= 3 means File CRC
	total_threads = unshk_get_long4(&cptr[10]);
	num_blocks = unshk_get_long4(&cptr[26]);	// extra_type
	block_size = unshk_get_word2(&cptr[30]);	// storage_type
	// P8 ShrinkIt is riddled with bugs.  Disk archives have incorrect
	//  thread_eof for the uncompressed total size.  So we need to do
	//  num_blocks * block_size to get the real size.  But this can be
	//  buggy too!  These fixes are from NuFxLib::Thread.c actualThreadEOF
	//  comments
	// First, fix block_size.  SHK v3.0.1 stored it as a small value
	if(block_size < 256) {
		block_size = 512;
	}
	disk_size = block_size * num_blocks;
	if(disk_size == (70*1024)) {
		// Old GSHK apparently set block_size==256 but blocks=280 for
		//  5.25" DOS 3.3 disks...block size must be 512 to equal 140K.
		disk_size = 140*1024;
	}
	if(disk_size < 140*1024) {
		printf("disk_size %dK is invalid\n", disk_size >> 10);
		return;
	}
	cptr += attrib_count;
	filename_length = unshk_get_word2(&cptr[-2]);	// filename_length
	cptr += filename_length;
	dptr = cptr + 16*total_threads;
		// Each thread is 16 bytes, so the data is at +16*total_threads
		// The data is in the same order as the header for the threads
		// We ignore anything other than a data thread for SDK
	for(i = 0; i < (int)total_threads; i++) {
		if((dptr >= cptr_end) || (cptr >= cptr_end)) {
			return;
		}
		thread_class = unshk_get_word2(&cptr[0]);
		thread_format = unshk_get_word2(&cptr[2]);
		thread_kind = unshk_get_word2(&cptr[4]);
		thread_crc = unshk_get_word2(&cptr[6]);
		//thread_eof = unshk_get_long4(&cptr[8]);
		// thread_eof is wrong in P8 ShrinkIt, so just use disk_size
		thread_eof = disk_size;
		comp_thread_eof = unshk_get_long4(&cptr[12]);
		if((dptr + comp_thread_eof) > cptr_end) {
			return;			// Corrupt
		}
		if((thread_class == 2) && (thread_kind == 1)) {
			// Disk image!
			ucptr = malloc(thread_eof + 0x1000);
			unshk_data(dsk, dptr, comp_thread_eof, ucptr,
					thread_eof, thread_format, base_cptr);
			if(dsk->fd == 0) {
				// Success, so far.  Check CRC
				printf("Version:%d, thread_crc:%04x\n",
					version, thread_crc);
				if(version >= 3) {		// CRC is valid
					crc = unshk_calc_crc(ucptr, thread_eof,
								0xffff);
#if 0
					printf("Thread CRC:%04x, exp:%04x\n",
						crc, thread_crc);
#endif
					if(crc != thread_crc) {
						printf("Bad CRC: %04x != exp "
							"%04x\n", crc,
							thread_crc);
						dsk->fd = -1;
					}
				}
			}
			if(dsk->fd < 0) {
				free(ucptr);
			} else {
				// Real success, set raw_size
				dsk->raw_dsize = thread_eof;
				dsk->raw_data = ucptr;
				return;
			}
		} else {
			dptr += comp_thread_eof;
			cptr += 16;
		}
	}

	// Disk image thread not found, get out
}

void
unshk(Disk *dsk, const char *name_str)
{
	byte	*cptr;
	int	compr_size, fd, pos, ret;
	int	i;

	printf("unshk %s\n", name_str);
	// Handle .sdk inside a .zip
	if(dsk->raw_data) {
		unshk_dsk_raw_data(dsk);
		return;
	}

	// File is not opened yet, try to open it
	fd = open(name_str, O_RDONLY | O_BINARY, 0x1b6);
	if(fd < 0) {
		return;
	}
	compr_size = (int)cfg_get_fd_size(fd);
	printf("size: %d\n", compr_size);

	cptr = malloc(compr_size + 0x1000);
	pos = 0;
	for(i = 0; i < 0x1000; i++) {
		cptr[compr_size + i] = 0;
	}
	while(1) {
		if(pos >= compr_size) {
			break;
		}
		ret = read(fd, cptr + pos, compr_size - pos);
		if(ret <= 0) {
			break;
		}
		pos += ret;
	}
	close(fd);

	if(pos != compr_size) {
		compr_size = 0;		// Make header searching fail
	}
	unshk_parse_header(dsk, cptr, compr_size, cptr);

	free(cptr);
}

void
unshk_dsk_raw_data(Disk *dsk)
{
	byte	*save_raw_data, *cptr;
	dword64	save_raw_dsize;
	int	save_fd, compr_size;
	int	i;
	// This code handles the case of .sdk inside a .zip (for example).
	// Since unshk() code uses dsk->fd, dsk->raw_data, and dsk->raw_dsize
	//  to communicate success in unshk'ing the disk, we need to copy
	//  those, and restore them, if the unshk fails
	save_fd = dsk->fd;
	save_raw_data = dsk->raw_data;
	save_raw_dsize = dsk->raw_dsize;
	if(save_raw_dsize >= (1ULL << 30)) {
		return;			// Too large
	}

	dsk->fd = -1;
	dsk->raw_data = 0;
	dsk->raw_dsize = 0;

	compr_size = (int)save_raw_dsize;
	cptr = malloc(compr_size + 0x1000);
	for(i = 0; i < 0x1000; i++) {
		cptr[compr_size + i] = 0;
	}
	for(i = 0; i < compr_size; i++) {
		cptr[i] = save_raw_data[i];
	}

	unshk_parse_header(dsk, cptr, compr_size, cptr);
	free(cptr);

	if(dsk->raw_data) {
		// Success, free the old raw data
		free(save_raw_data);
		return;
	}
	dsk->fd = save_fd;
	dsk->raw_data = save_raw_data;
	dsk->raw_dsize = save_raw_dsize;
}

