const char rcsid_undeflate_c[] = "@(#)$KmKId: undeflate.c,v 1.20 2023-09-26 00:20:53+00 kentd Exp $";

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

// This file has routines for the undeflate uncompression algorithm for
//  gzip/zip, and routines for reading .zip files.

// Based on https://www.ietf.org/rfc/rfc1951.txt for Deflate algorithm,
//  and https://www.ietf.org/rfc/rfc1952.txt for gzip file format.

// .zip file format from:
// https://pkware.cachefly.net/webdocs/APPNOTE/APPNOTE-6.3.3.TXT

#include "defc.h"

// FILE *g_outf = 0;

#define LENGTH_ENCODED	0xffffff444449ULL
	// LENGTH_ENCODED encodes the first table of section 3.2.5 for
	//  fixed Huffman: 257-264 = 0 extra bits, length=3-10 (so 8 entries)
	//  then 265-268 = 1 extra bit, length=11... (so 4 entries), etc.
	//  which is encoded in each nibble of this word.  Code 285 (entry 29)
	//  has extra_bits=0, indicated by the encoded nibble being 0xf.
#define DIST_ENCODED	0xff22222222222224ULL
	// DIST_ENCODED encodes the second table of section 3.2.5 for the
	//  fixed Huffman table:  Codes 0-3 have 0 extra bits, dist=1,2,3,4
	//  (so 4 entries), codes 4,5 have 1 extra bit, dest=5... (so 2
	//  entries), etc.  0xf indicates an invalid entry

word32	g_undeflate_fixed_len_tab[512+1];
			// extrabits << 20 | bits << 16 | len/literal
word32	g_undeflate_fixed_dist_tab[32+1];
			// extrabits << 20 | bits << 16 | distance
word32	g_undeflate_length_tab[32+1];	// extra_bits << 20 | len
word32	g_undeflate_dist_tab[32+1];	// extra_bits << 20 | dist
word32	g_undeflate_bit_rev[512];
word32 g_undeflate_lencode_positions[19] =
{		0x200310, 0x300311, 0x700b12,
		0x100, 0x108, 0x107, 0x109, 0x106, 0x10a, 0x105, 0x10b,
		0x104, 0x10c, 0x103, 0x10d, 0x102, 0x10e, 0x101, 0x10f };
word32	g_undeflate_lencode_tab[128 + 1];
word32	*g_undeflate_dynamic_tabptr = 0;
word32	g_undeflate_dynamic_bits = 0;
word32	*g_undeflate_dynamic_dist_tabptr = 0;
word32	g_undeflate_dynamic_dist_bits = 0;

void *
undeflate_realloc(void *ptr, dword64 dsize)
{
	if((size_t)dsize != dsize) {
		return 0;
	}
	return realloc(ptr, (size_t)dsize);
}

void *
undeflate_malloc(dword64 dsize)
{
	if((size_t)dsize != dsize) {
		return 0;
	}
	return malloc((size_t)dsize);
}

void
show_bits(unsigned *llptr, int nl)
{
	int	i;

	fprintf(stdout, "Showing %03x bits entries\n", nl);
	for(i = 0; i < nl; i++) {
		fprintf(stdout, "%03x: %03x\n", i, (llptr[i] >> 16) & 0xf);
	}
}

void
show_huftb(unsigned *tabptr, int bits)
{
	unsigned char seen[512];
	word32	entry, code, val;
	int	i, j;

	printf("Showing hufftab of %d bits\n", bits);

	for(i = 0; i < 256+32; i++) {
		seen[i] = 0;
	}
	for(i = 0; i < (1 << bits); i++) {
		entry = tabptr[i];
		code = entry & 0xff;
		if((entry >> 24) & 1) {
			val = entry & 0xfff0ffffUL;
			for(j = 0; j < 32; j++) {
				if(val == g_undeflate_length_tab[j]) {
					code = 256 + j;
					break;
				}
			}
			if(code < 256) {
				printf("entry %08x (%08x) not found, [0]=%08x "
					"[1]=%08x\n", entry, val,
					g_undeflate_length_tab[0],
					g_undeflate_length_tab[1]);
				code = 256 + 31;
			}
		}
		if(!seen[code]) {
			printf("code %03x has bits:%d huffcode:%04x\n", code,
				(entry >> 16) & 0xf, i);
			seen[code] = 1;
		}
	}
}

void
undeflate_init_len_dist_tab(word32 *tabptr, dword64 drepeats, word32 start)
{
	word32	pos, repeats, extra_bits;
	int	i;

	// Initializes g_undeflate_length_tab[] and g_underflate_dist_tab[]
	// printf("undeflate len_dist_tab repeats:%016llx\n", drepeats);
	pos = 0;
	extra_bits = 0;
	while(pos < 30) {
		repeats = drepeats & 0xf;
		drepeats = drepeats >> 4;
		if(repeats == 0xf) {
			// Special handling for code=285 (pos=29)
			extra_bits = 0;
			start--;		// 258, not 259
			repeats = 1;
		}
		for(i = 0; i < (int)repeats; i++) {
			tabptr[pos] = start | (extra_bits << 20) | (1 << 24);
			//printf("Set table[%d]=%08x (%d) i:%d out of %d\n",
			//	pos, tabptr[pos], start, i, repeats);
			pos++;
			start += (1 << extra_bits);
		}
		extra_bits++;
	}
}

void
undeflate_init_bit_rev_tab(word32 *tabptr, int num)
{
	word32	pos, val;
	int	i, j;

	// Initializes g_undeflate_bit_rev[]
	for(i = 0; i < num; i++) {
		pos = i;
		val = 0;
		for(j = 0; j < 9; j++) {
			val = (val << 1) | (pos & 1);
			pos = pos >> 1;
		}
		tabptr[i] = val;
		// printf("Bit reverse[%03x]=%03x\n", i, val);
	}
}

word32
undeflate_bit_reverse(word32 val, word32 bits)
{
	word32	new_val, val2, shift;

	new_val = g_undeflate_bit_rev[val & 0x1ff];	// at most 9 bits
	shift = 9 - bits;
	if(bits <= 9) {
		return new_val >> shift;
	} else if(bits <= 18) {
		shift += 9;		// bits:10->shift=8, bits:11->shift=7,..
		val2 = g_undeflate_bit_rev[(val >> 9) & 0x1ff] >> shift;
		shift = bits - 9;
		return (new_val << shift) | val2;
	}
	printf("Cannot reverse %08x bits:%d!\n", val, bits);
	return 0;
}

word32
undeflate_calc_crc32(byte *bptr, word32 len)
{
	word32	crc, c, xor;
	int	i;

	// Old version, don't use other than for testing purposes.
	//  Use woz_calc_crc32() instead.

	// Generate CCITT-32 CRC, with remainder initialized to -1 and return
	//  the complement of the CRC value
	// This is slow--but it doesn't matter for KEGS, where the images
	//  are generally only 800KB max.
	crc = (word32)-1;
	while(len != 0) {
		c = *bptr++;
		len--;
		for(i = 0; i < 8; i++) {
			xor = 0;
			if((crc ^ c) & 1) {
				xor = 0xedb88320UL;
			}
			crc = (crc >> 1) ^ xor;
			c = c >> 1;
		}
	}
	return (~crc);
}

byte *
undeflate_ensure_dest_len(Disk *dsk, byte *ucptr, word32 len)
{
	byte	*raw_ptr;
	dword64	raw_dsize, dimage_size;
	word32	new_image_size;

	raw_dsize = dsk->raw_dsize;
	dimage_size = dsk->dimage_size;
	if(ucptr) {
		new_image_size = (word32)(ucptr - dsk->raw_data);
		if(new_image_size < dimage_size) {
			printf("ucptr moved backwards!\n");
			return 0;
		}
		if((new_image_size >> 31) != 0) {
			printf("Output file > 2GB, failing\n");
			return 0;
		}
		dimage_size = new_image_size;
		dsk->dimage_size = dimage_size;
	}
	if(dimage_size > raw_dsize) {
		printf("dimage_size %08llx overflowed raw_dsize %08llx\n",
			dimage_size, raw_dsize);
		return 0;
	}
	if((dimage_size + len) > raw_dsize) {
		raw_dsize = ((dimage_size + len) * 3ULL) / 2;
		raw_ptr = undeflate_realloc(dsk->raw_data, raw_dsize);
		//printf("Did realloc to %08x, new new_data:%p, was %p\n",
		//			raw_size, raw_ptr, dsk->raw_data);
		if(raw_ptr == 0) {
			printf("undeflate realloc failed\n");
			free(dsk->raw_data);
			dsk->raw_data = 0;
			return 0;
		}
		dsk->raw_data = raw_ptr;
		dsk->raw_dsize = raw_dsize;
	}
#if 0
	printf("undeflate_ensure_dest_len will ret %p, dsk->raw_data:%p, "
		"image_size:%08llx, raw_dsize:%08llx\n",
		dsk->raw_data + dimage_size, dsk->raw_data, dimage_size,
		raw_dsize);
#endif
	return dsk->raw_data + dimage_size;
}

void
undeflate_add_tab_code(word32 *tabptr, word32 tabsz_lg2, word32 code,
							word32 entry)
{
	word32	rev_code, bits, pos, tab_size;
	int	num;
	int	i;

	if(tabsz_lg2 > 15) {
		printf("tabsz_lg2: %04x is not supported\n", tabsz_lg2);
		return;
	}
	tab_size = 1 << tabsz_lg2;
	rev_code = 0;
	bits = (entry >> 16) & 0xf;
	rev_code = undeflate_bit_reverse(code, bits);
	if(rev_code >= tab_size) {
		printf("rev_code:%04x out of range for entry %08x\n", rev_code,
									entry);
		tabptr[tab_size] = 1;
		return;
	}
	num = 1 << (tabsz_lg2 - bits);
	if(num < 0) {
		printf("num %d out of range for entry %08x\n", num, entry);
		tabptr[tab_size] = 1;
		return;
	}
	for(i = 0; i < num; i++) {
		pos = rev_code | (i << bits);
		if(tabptr[pos] != 0) {
			printf("Overwriting old [%04x]=%08x with %08x\n", pos,
						tabptr[pos], entry);
			tabptr[tab_size] = 1;
		}
#if 0
		if(i >= 0) {
			printf("Set code tab[%04x]=%08x (code:%04x)\n", pos,
							entry, save_code);
		}
#endif
		tabptr[pos] = entry;
	}
}

word32 *
undeflate_init_fixed_tabs()
{
	word32	*tabptr;
	int	i;

	tabptr = &(g_undeflate_fixed_len_tab[0]);
	// Init g_undeflate_fixed_len_tab[] for the fixed Huffman code
	for(i = 0; i < 513; i++) {
		tabptr[i] = 0;
	}
	// printf("Add fixed_len_tab for literals 0 - 143\n");
	for(i = 0; i < 144; i++) {
		undeflate_add_tab_code(tabptr, 9, 0x30 + i, (8 << 16) | i);
	}
	// printf("Add fixed_len_tab for literals 144 - 255\n");
	for(i = 144; i < 256; i++) {
		undeflate_add_tab_code(tabptr, 9, 0x190 + i - 144,
								(9 << 16) | i);
	}
	// printf("Add fixed_len_tab for length codes 256 - 279\n");
	for(i = 256; i < 280; i++) {
		// printf("code: %03x fixed_len_tab[%03x]=%08x\n", i, i - 256,
		//			g_undeflate_length_tab[i - 256]);
		undeflate_add_tab_code(tabptr, 9, 0 + i - 256,
				(7 << 16) | g_undeflate_length_tab[i - 256]);
	}
	// printf("Add fixed_len_tab for length codes 280 - 287\n");
	for(i = 280; i < 288; i++) {
		undeflate_add_tab_code(tabptr, 9, 0xc0 + i - 280,
				(8 << 16) | g_undeflate_length_tab[i - 256]);
	}
	if(tabptr[512]) {
		return 0;
	}

	// And init g_undeflate_fixed_dist_tab[]
	tabptr = &(g_undeflate_fixed_dist_tab[0]);
	for(i = 0; i < 33; i++) {
		tabptr[i] = 0;
	}
	// printf("Add fixed_dist_tab for codes 0 - 29\n");
	for(i = 0; i < 30; i++) {
		undeflate_add_tab_code(tabptr, 5, i,
					(5 << 16) | g_undeflate_dist_tab[i]);
	}

	if(tabptr[32]) {
		return 0;
	}
	return tabptr;
}

word32 *
undeflate_init_tables()
{
	undeflate_init_len_dist_tab(&(g_undeflate_length_tab[0]),
							LENGTH_ENCODED, 2);
		// code=257 has length 3, but the first entry is really code=256
		//  so set 256 to length=2
	undeflate_init_len_dist_tab(&(g_undeflate_dist_tab[0]), DIST_ENCODED,
									1);
	undeflate_init_bit_rev_tab(&(g_undeflate_bit_rev[0]), 512);
	// undeflate_check_bit_reverse();
	return undeflate_init_fixed_tabs();
}

void
undeflate_free_tables()
{
	free(g_undeflate_dynamic_tabptr);
	g_undeflate_dynamic_tabptr = 0;
	free(g_undeflate_dynamic_dist_tabptr);
	g_undeflate_dynamic_dist_tabptr = 0;
}

void
undeflate_check_bit_reverse()
{
	word32	rev, tmp, checked;
	int	i, j, bits;

	// Check bit-reverse function.  Reverse all values from 0-32767
	checked = 0;
	for(bits = 1; bits <= 16; bits++) {
		// printf("Checking bit reverse bits=%d\n", bits);
		for(i = 0; i < 65536; i++) {
			if(i >= (1 << bits)) {
				break;
			}
			tmp = i;
			rev = 0;
			for(j = 0; j < bits; j++) {
				rev = (rev << 1) | (tmp & 1);
				tmp = tmp >> 1;
			}
			tmp = undeflate_bit_reverse(i, bits);
			if(tmp != rev) {
				printf("Reverse %04x bits:%d ret:%04x, "
					"exp:%04x\n", i, bits, tmp, rev);
				exit(2);
			}
			checked++;
		}
	}
	printf("Checked %08x values\n", checked);
}

word32 *
undeflate_build_huff_tab(word32 *tabptr, word32 *entry_ptr, word32 len_size,
					word32 *bl_count_ptr, int max_bits)
{
	word32	next_code[16];
	word32	code, tab_size, bits, entry;
	int	i;

	tab_size = (1 << max_bits);
	if(max_bits > 15) {
		printf("max_bits: %d out of range\n", max_bits);
		return 0;
	}
	next_code[0] = 0;
	bl_count_ptr[0] = 0;		// Force number of 0-bit lengths to 0
	code = 0;
	// printf("build_huff_tab, max_bits:%d, tab_size:%08x\n", max_bits,
	//							tab_size);
	for(i = 1; i <= max_bits; i++) {
		// printf("bl_count[%d] = %03x\n", i - 1, bl_count_ptr[i-1]);
		code = (code + bl_count_ptr[i - 1]) << 1;
		next_code[i] = code;
		// printf("Set next_code[%d] = %03x\n", i, code);
	}
	for(i = 0; i < (int)tab_size; i++) {
		tabptr[i] = 0;
	}
	tabptr[tab_size] = 0;

	for(i = 0; i < (int)len_size; i++) {
		entry = entry_ptr[i];
		bits = (entry >> 16) & 0xf;
		//printf("i:%03x, bits:%d, entry:%08x\n", i, bits, entry);
		if(!bits) {
			continue;
		}
		code = next_code[bits]++;
		//printf("Set tab code:%03x = %08x\n", code, entry);
		undeflate_add_tab_code(tabptr, max_bits, code, entry);
	}

	// printf("All done, returning tabptr\n");
	return tabptr;
}

word32 *
undeflate_dynamic_table(byte *cptr, word32 *bit_pos_ptr, byte *cptr_base)
{
	word32	code_list[256+32+32+1];
	word32	len_codes[19];
	word32	bl_count[19], bl_count_dist[16];
	word32	*tabptr, *tabptr_dist;
	byte	*cptr_start;
	word32	bit_pos, val, hlit, hdist, hclen, pos, max_bits, code_pos;
	word32	total_codes_needed, repeat, mask, entry, bits;
	word32	max_length_bits, max_distance_bits, extra_bits;
	int	i;

	// This is compressed compressed huffman lengths.  First
	//  get the length codes, then get the actual lengths
	// Get 14 bits, which always fits in 3 bytes
	bit_pos = *bit_pos_ptr;
	cptr_start = cptr;
	val = (cptr[0] + (cptr[1] << 8) + (cptr[2] << 16)) >> bit_pos;
	hlit = (val & 0x1f) + 257;		// 257 - 288
	hdist = ((val >> 5) & 0x1f) + 1;
	hclen = (val >> 10) & 0xf;
#if 0
	printf("At +%06x, bit:%d, hlit:%02x hdist:%02x, hclen:%02x\n",
		(word32)(cptr - cptr_base), bit_pos, hlit, hdist, hclen);
#endif
	if(cptr_base) {
		// Avoid unused parameter warning
	}
	bit_pos += 14;
	cptr += (bit_pos >> 3);
	bit_pos = bit_pos & 7;
	for(i = 0; i < 19; i++) {
		len_codes[i] = 0;
		bl_count[i] = 0;
	}
	hclen += 4;			// 19*3 = 57 bits, at most
	max_bits = 0;
	for(i = 0; i < (int)hclen; i++) {
		val = ((cptr[0] + (cptr[1] << 8)) >> bit_pos) & 7;
		entry = g_undeflate_lencode_positions[i];
		entry = entry & (~0xf0000);		// clear bits from entry
		pos = entry & 0x1f;
		len_codes[pos] = entry | (val << 16);
		// printf("len_codes[%d]=%08x\n", pos, len_codes[pos]);
		bl_count[val]++;
		if(val > max_bits) {
			max_bits = val;
		}
		// printf("Num bits for len code %02x = %d\n", pos, val);
		bit_pos += 3;
		cptr += (bit_pos >> 3);
		bit_pos = bit_pos & 7;
	}
	// Build huffman table
	tabptr = undeflate_build_huff_tab(&(g_undeflate_lencode_tab[0]),
			&(len_codes[0]), 19, &(bl_count[0]), max_bits);
	if(tabptr == 0) {
		printf("Bad table\n");
		return 0;
	}

	// Now we've made the table in tabptr.  Read the length codes now
	total_codes_needed = hlit + hdist;
	// printf("Getting %04x total codes\n", total_codes_needed);
	code_pos = 0;
	mask = (1 << max_bits) - 1;
	if(total_codes_needed > (256+32+32)) {
		printf("total_codes_needed high: %04x\n", total_codes_needed);
		return 0;
	}
	for(i = 0; i < 16; i++) {
		bl_count[i] = 0;
		bl_count_dist[i] = 0;
	}
	while(code_pos < total_codes_needed) {
		pos = (cptr[0] | (cptr[1] << 8)) >> bit_pos;
		pos = pos & mask;
		entry = tabptr[pos & mask];
#if 0
		printf("At +%06x, bit:%d: Raw code: %02x, entry:%08x\n",
			(word32)(cptr - cptr_base), bit_pos, pos, entry);
#endif
		val = entry & 0x1f;

		bits = (entry >> 16) & 7;
		extra_bits = (entry >> 20) & 7;
		repeat = (entry >> 8) & 0xf;
		entry = (val << 16);			// Set bits
		bit_pos += bits;
		cptr += (bit_pos >> 3);
		bit_pos = bit_pos & 7;
		pos = (cptr[0] | (cptr[1] << 8)) >> bit_pos;
#if 0
		printf("At +%06x, bit:%d: Raw pos:%04x\n",
			(word32)(cptr - cptr_base), bit_pos, pos);
#endif
		pos = pos & ((1 << extra_bits) - 1);
		repeat = repeat + pos;
		bit_pos += extra_bits;
		cptr += (bit_pos >> 3);
		bit_pos = bit_pos & 7;
		if(!repeat) {
			printf("Bad repeat value\n");
			return 0;
		}
		if(val >= 0x10) {
			entry = 0;
			if(val == 0x10) {		// Repeat prev entry
				entry = code_list[code_pos - 1];
				if(!code_pos) {
					printf("Got repeat code 0x10 at 0!\n");
					return 0;
				}
			}
		}
		for(i = 0; i < (int)repeat; i++) {
			code_list[code_pos] = entry;
			// printf("Added code_list[%03x] = %08x\n", code_pos,
			//						entry);
			code_pos++;
		}
	}

	// Fix lengths and literals
	max_length_bits = 0;
	for(i = 0; i < (int)hlit; i++) {
		entry = code_list[i];
		bits = (entry >> 16) & 0xf;
		bl_count[bits]++;
		if(i >= 256) {
			entry |= g_undeflate_length_tab[i - 256];
		} else {
			entry |= i;
		}
		code_list[i] = entry;
		if(bits > max_length_bits) {
			max_length_bits = bits;
		}
	}

	// Fix distances
	max_distance_bits = 0;
	for(i = 0; i < (int)hdist; i++) {
		entry = code_list[i + hlit];
		bits = (entry >> 16) & 0xf;
		bl_count_dist[bits]++;
		entry |= g_undeflate_dist_tab[i];
		code_list[i + hlit] = entry;
		if(bits > max_distance_bits) {
			max_distance_bits = bits;
		}
	}
	if(code_pos != total_codes_needed) {
		printf("Got %03x codes, needed %03x codes\n", code_pos,
							total_codes_needed);
		return 0;
	}
	// printf("max_length_bits: %d, max_distance_bits: %d\n",
	//				max_length_bits, max_distance_bits);
	tabptr = g_undeflate_dynamic_tabptr;
	if(!tabptr) {
		tabptr = malloc(sizeof(word32)*((1 << 15) + 1));
		g_undeflate_dynamic_tabptr = tabptr;
		// printf("malloc literal table\n");
	}
	g_undeflate_dynamic_bits = max_length_bits;
	//printf("Building literal/length table, %d entries, %d bits\n", hlit,
	//						max_length_bits);
	//show_bits(&(code_list[0]), hlit);

	tabptr = undeflate_build_huff_tab(tabptr, &(code_list[0]),
			hlit, &(bl_count[0]), max_length_bits);
	if(tabptr == 0) {
		printf("Building literal table failed\n");
		return 0;
	}
	//show_huftb(tabptr, max_length_bits);

	tabptr_dist = g_undeflate_dynamic_dist_tabptr;
	if(!tabptr_dist) {
		tabptr_dist = malloc(sizeof(word32) * ((1 << 15) + 1));
		g_undeflate_dynamic_dist_tabptr = tabptr_dist;
		// printf("malloc dist table\n");
	}
	g_undeflate_dynamic_dist_bits = max_distance_bits;
	tabptr_dist = undeflate_build_huff_tab(tabptr_dist, &(code_list[hlit]),
			hdist, &(bl_count_dist[0]), max_distance_bits);
	if(tabptr_dist == 0) {
		printf("Building dist table failed\n");
		return 0;
	}

	// Update *bit_pos_ptr to skip over the table
	*bit_pos_ptr = bit_pos + (int)(8*(cptr - cptr_start));
	return tabptr;
}

byte *
undeflate_block(Disk *dsk, byte *cptr, word32 *bit_pos_ptr, byte *cptr_base,
							byte *cptr_end)
{
	word32	*lit_tabptr, *dist_tabptr;
	byte	*ucptr, *ucptr_end;
	word32	bfinal, btype, bit_pos, len, pos, extra_bits, entry, dist_entry;
	word32	bits, is_len, dist, lit_mask, dist_mask, tmp;
	int	i;

	bit_pos = *bit_pos_ptr;

	// printf("At file offset %08x,bit %d cptr[0]:%02x %02x\n",
	//		(word32)(cptr - cptr_base), bit_pos, cptr[0], cptr[1]);
	bfinal = (cptr[0] >> bit_pos) & 1;
	bit_pos++;
	btype = (((cptr[1] << 8) | cptr[0]) >> bit_pos) & 3;
	bit_pos += 2;
	cptr += (bit_pos >> 3);
	bit_pos = bit_pos & 7;
	// printf("bfinal:%d, btype:%d\n", bfinal, btype);

	if(bfinal) {
		dsk->fd = 0;			// Last block
	}
	if(btype == 3) {			// Reserved: error
		return 0;
	} else if(btype == 0) {			// uncompressed
		// Align cptr to next byte
		bit_pos += 7;
		cptr += (bit_pos >> 3);
		*bit_pos_ptr = 0;
		len = cptr[0] + (cptr[1] << 8);
		ucptr = undeflate_ensure_dest_len(dsk, 0, len);
		if(!ucptr) {
			return 0;
		}
		cptr += 4;
		for(i = 0; i < (int)len; i++) {
			*ucptr++ = *cptr++;
		}
		dsk->dimage_size += len;
		return cptr;
	}

	if(btype == 1) {			// Fixed Huffman codes
		lit_tabptr = &(g_undeflate_fixed_len_tab[0]);
		dist_tabptr = &(g_undeflate_fixed_dist_tab[0]);
		lit_mask = 0x1ff;
		dist_mask = 0x1f;
	} else {				// Dynamic Huffman codes
		*bit_pos_ptr = bit_pos;
		lit_tabptr = undeflate_dynamic_table(cptr, bit_pos_ptr,
								cptr_base);
		dist_tabptr = g_undeflate_dynamic_dist_tabptr;
		// printf("dynamic table used %d bits\n",
		//				*bit_pos_ptr - bit_pos);
		lit_mask = (1 << g_undeflate_dynamic_bits) - 1;
		dist_mask = (1 << g_undeflate_dynamic_dist_bits) - 1;
		bit_pos = *bit_pos_ptr;
		cptr += (bit_pos >> 3);
		bit_pos = bit_pos & 7;
	}
	if(!lit_tabptr || !dist_tabptr) {
		printf("Code table failure\n");
		return 0;
	}

	ucptr = undeflate_ensure_dest_len(dsk, 0, 65536);	// Just a guess
	if(!ucptr) {
		return 0;
	}
	ucptr_end = dsk->raw_data + dsk->raw_dsize - 500;

	while(cptr < cptr_end) {
#if 0
		printf("Top of loop, cptr:%p, lit_tabptr:%p, dsk->raw:%p\n",
				cptr, lit_tabptr, dsk->raw_data);
#endif

		if(ucptr > ucptr_end) {
			ucptr = undeflate_ensure_dest_len(dsk, ucptr, 65536);
			ucptr_end = dsk->raw_data + dsk->raw_dsize - 500;
			// printf("Update ucptr to %p\n", ucptr);
			if(!ucptr) {
				return 0;
			}
		}
		pos = cptr[0] | (cptr[1] << 8) | (cptr[2] << 16);
		pos = pos >> bit_pos;
		entry = lit_tabptr[pos & lit_mask];
		bits = (entry >> 16) & 0xf;
		is_len = (entry >> 24) & 1;
		len = entry & 0xffff;
#if 0
		printf("At offset +%08x bit:%d, huffcode=%04x, is %d bits, "
			"entry=%08x\n", (int)(cptr - cptr_base), bit_pos,
			pos & lit_mask, bits, entry);
#endif
		if(bits == 0) {
			printf("bits=0, %08x bad table\n", lit_mask);
			return 0;
		}
		bit_pos += bits;
		cptr += (bit_pos >> 3);
		bit_pos = bit_pos & 7;
		if(!is_len) {			// Literal
			// literal byte
			// printf(" Out +%06x: %02x\n",
			//	(int)(ucptr - dsk->raw_data), len & 0xff);
			//putc(len, g_outf);
			*ucptr++ = len;
		} else {
			if(len == 2) {			// Code=0x100, end block
				// All done
				// printf("Got the 0x100 code!  All done!\n");
				*bit_pos_ptr = bit_pos;
				dsk->dimage_size = ucptr - dsk->raw_data;
				// printf("Set dsk->image_size = %08x\n",
				//			dsk->image_size);
				return cptr;
			}
			extra_bits = (entry >> 20) & 7;
			if(extra_bits) {
				pos = cptr[0] | (cptr[1] << 8);
				pos = pos >> bit_pos;
				pos = pos & ((1 << extra_bits) - 1);
				len += pos;
			}
#if 0
			printf("At offset +%08x, bit:%d got extra_bits=%d, "
				"len=%08x\n", (int)(cptr - cptr_base), bit_pos,
							extra_bits, len);
#endif
			bit_pos += extra_bits;
			cptr += (bit_pos >> 3);
			bit_pos = bit_pos & 7;

			// Get distance code
			pos = cptr[0] | (cptr[1] << 8) | (cptr[2] << 16);
			pos = pos >> bit_pos;
#if 0
			printf("At offset +%08x, bit:%d raw distance code: "
				"%02x\n", (int)(cptr - cptr_base), bit_pos,
				pos & dist_mask);
#endif
			dist_entry = dist_tabptr[pos & dist_mask];
			bits = (dist_entry >> 16) & 0xf;
			if(bits == 0) {
				printf("bits=0 for dist_entry:%08x %08x\n",
					dist_entry, pos);
			}
			extra_bits = (dist_entry >> 20) & 0xf;
			dist = dist_entry & 0xffff;
			//printf("dist_entry:%08x, extra_bits:%d, dist:%05x\n",
			//	dist_entry, extra_bits, dist);
			bit_pos += bits;
			cptr += (bit_pos >> 3);
			bit_pos = bit_pos & 7;
			if(extra_bits) {
				pos = (cptr[0] | (cptr[1] << 8) |
						(cptr[2] << 16)) >> bit_pos;
#if 0
				printf(" At offset +%08x, bit:%d, raw ex:"
					"%08x\n", (int)(cptr - cptr_base),
					bit_pos, pos);
#endif
				tmp = pos & ((1 << extra_bits) - 1);
				dist += tmp;
#if 0
				printf("at offset +%08x, got %d extra dist "
					"for total dist=%d (%05x)\n",
					(int)(cptr - cptr_base), extra_bits,
					dist, pos);
#endif
				bit_pos += extra_bits;
				cptr += (bit_pos >> 3);
				bit_pos = bit_pos & 7;
			}
			//printf("Repeating %d bytes from dist:%05x\n", len,
			//						dist);
			if(ucptr < (dsk->raw_data + dist)) {
				printf("Dist out of bounds:%04x %p %p\n",
						dist, ucptr, dsk->raw_data);
				return 0;
			}
			for(i = 0; i < (int)len; i++) {
				ucptr[0] = ucptr[0-(int)dist];
#if 0
				putc(ucptr[0], g_outf);
				printf(" Out +%06x: %02x\n",
					(int)(ucptr - dsk->raw_data),
					ucptr[0]);
#endif
				ucptr++;
			}
		}
	}

	printf("Ran out of compressed data, bad gzip file\n");

	return 0;
}

byte *
undeflate_gzip_header(Disk *dsk, byte *cptr, word32 compr_size)
{
	word32	*wptr;
	byte	*cptr_base, *cptr_end;
	word32	flg, xfl, xlen, bit_offset, exp_crc, len, crc;

	cptr_base = cptr;
	cptr_end = cptr + compr_size;

	if((cptr[0] != 0x1f) || (cptr[1] != 0x8b) || (cptr[2] != 0x08)) {
		printf("Not gzip file, exiting\n");
		return 0;
	}

	flg = cptr[3];
	xfl = cptr[8];
	printf("flg:%02x and xflags:%02x\n", flg, xfl);
	cptr += 10;

	if(flg & 4) {		// FEXTRA set
		xlen = cptr[0] + (cptr[1] * 256);
		printf("FEXTRA XLEN is %d, skipping that many bytes\n", xlen);
		cptr += 2 + xlen;
	}

	if(flg & 8) {		// FNAME set
		cptr += strlen((char *)cptr) + 1;
	}
	if(flg & 0x10) {	// FCOMMENT set
		cptr += strlen((char *)cptr) + 1;
	}
	if(flg & 2) {		// FHCRC set
		cptr += 2;
	}
	printf("gzip header was %02x bytes long\n", (int)(cptr - cptr_base));

	dsk->raw_dsize = 140*1024;		// Just a guess, alloc size
	dsk->raw_data = undeflate_malloc(dsk->raw_dsize);
	if(dsk->raw_data == 0) {
		return 0;
	}
	printf("Initial malloc (not realloc) set raw_data=%p\n", dsk->raw_data);

	dsk->dimage_size = 0;			// Used size

	wptr = undeflate_init_tables();
	if(wptr == 0) {
		return 0;			// Some sort of error, get out
	}

	bit_offset = 0;
	while(cptr < cptr_end) {
		cptr = undeflate_block(dsk, cptr, &bit_offset, cptr_base,
								cptr_end);
		if(cptr == 0) {
			// Failed
			break;
		}
		if(dsk->fd == 0) {
			printf("undeflate_block set fd=0, success\n");
			// Done, success!
			// Check crc
			if(bit_offset) {
				cptr++;
			}
			if((cptr + 8) > cptr_end) {
				printf("No CRC or LEN fields at end\n");
				break;
			}
			exp_crc = cptr[0] | (cptr[1] << 8) | (cptr[2] << 16) |
						(cptr[3] << 24);
			len = cptr[4] | (cptr[5] << 8) | (cptr[6] << 16) |
						(cptr[7] << 24);
			if(len != dsk->dimage_size) {
				printf("Len mismatch: exp %08x != %08llx\n",
							len, dsk->dimage_size);
				break;
			}
			crc = woz_calc_crc32(dsk->raw_data, len, 0);
			if(crc != exp_crc) {
				printf("CRC mismatch: %08x != exp %08x\n",
						crc, exp_crc);
				break;
			}
			// Real success, set raw_dsize
			dsk->raw_data = undeflate_realloc(dsk->raw_data,
							dsk->dimage_size);
			dsk->raw_dsize = dsk->dimage_size;
			return cptr;
		}
	}

	printf("Failed\n");
	// Disk image thread not found, get out
	free(dsk->raw_data);
	dsk->fd = -1;
	dsk->dimage_size = 0;
	dsk->raw_data = 0;
	dsk->raw_dsize = 0;
	return 0;
}

void
undeflate_gzip(Disk *dsk, const char *name_str)
{
	byte	*cptr;
	dword64	compr_dsize, dret;
	word32	compr_size;
	int	fd;
	int	i;

	// On success, set dsk->fd=0 and dsk->raw_data,raw_dsize properly.
	printf("undeflate_gzip on file %s\n", name_str);
	fd = open(name_str, O_RDONLY | O_BINARY, 0x1b6);
	if(fd < 0) {
		return;
	}
	compr_dsize = cfg_get_fd_size(fd);
	printf("size: %lld\n", compr_dsize);
	if((compr_dsize >> 31) != 0) {
		// > 2GB...too big for this code
		printf("gzip file is too large\n");
		dsk->fd = -1;
		return;
	}
	compr_size = (word32)compr_dsize;

	cptr = malloc(compr_size + 0x1000);
	for(i = 0; i < 0x1000; i++) {
		cptr[compr_size + i] = 0;
	}
	dret = cfg_read_from_fd(fd, cptr, 0, compr_size);
	if(dret != compr_size) {
		compr_size = 0;		// Make header searching fail
	}
	//g_outf = fopen("out.dbg", "w");
	undeflate_gzip_header(dsk, cptr, compr_size);

	free(cptr);
	undeflate_free_tables();
}

byte *
undeflate_zipfile_blocks(Disk *dsk, byte *cptr, dword64 dcompr_size)
{
	word32	*wptr;
	byte	*cptr_base, *cptr_end;
	word32	bit_offset;

	cptr_base = cptr;
	cptr_end = cptr + dcompr_size;

	dsk->raw_data = undeflate_malloc(dsk->raw_dsize);
	if(dsk->raw_data == 0) {
		return 0;
	}
	printf("Initial malloc (not realloc) set raw_data=%p\n", dsk->raw_data);

	dsk->dimage_size = 0;			// Used size

	wptr = undeflate_init_tables();
	if(wptr == 0) {
		return 0;			// Some sort of error, get out
	}

	bit_offset = 0;
	while(cptr < cptr_end) {
		cptr = undeflate_block(dsk, cptr, &bit_offset, cptr_base,
								cptr_end);
		if(cptr == 0) {
			// Failed
			break;
		}
		if(dsk->fd == 0) {
			printf("undeflate_block set fd=0, success\n");
			// Done, success!
			// Check crc
			if(bit_offset) {
				cptr++;
			}
			// Real success, set raw_dsize
			dsk->raw_data = undeflate_realloc(dsk->raw_data,
							dsk->dimage_size);
			dsk->raw_dsize = dsk->dimage_size;
			return cptr;
		}
	}

	printf("Failed\n");
	// Disk image thread not found, get out
	free(dsk->raw_data);
	dsk->fd = -1;
	dsk->dimage_size = 0;
	dsk->raw_data = 0;
	dsk->raw_dsize = 0;
	return 0;
}

byte g_zip_local_file_header[] = { 0x50, 0x4b, 0x03, 0x04 };
byte g_zip_central_file_header[] = { 0x50, 0x4b, 0x01, 0x02 };
byte g_zip_end_central_dir_header[] = { 0x50, 0x4b, 0x05, 0x06, 0, 0, 0, 0 };
byte g_zip64_end_central_dir_locator[] = { 0x50, 0x4b, 0x06, 0x07, 0, 0, 0, 0 };
byte g_zip64_end_central_dir_header[] = { 0x50, 0x4b, 0x06, 0x06 };

extern Cfg_listhdr g_cfg_partitionlist;


int
undeflate_zipfile(Disk *dsk, int fd, dword64 dlocal_header_off,
			dword64 uncompr_dsize, dword64 compr_dsize)
{
	byte	buf[64];
	byte	*cptr, *cptr2;
	dword64	dret, compr_doffset;
	word32	compr_method, name_len, extra_len;
	word32	bit_flags;
	int	ret;
	int	i;

	// return -1 on failure, >= 0 on success

	printf("undeflate_zipfile called, fd:%d, offset:%08llx, unc:%lld "
		"compr:%lld\n", fd, dlocal_header_off, uncompr_dsize,
		compr_dsize);

	dret = cfg_read_from_fd(fd, &buf[0], dlocal_header_off, 64);
	if(dret != 64) {
		printf("read dret:%08llx != 64\n", dret);
		return -1;
	}

	for(i = 0; i < 4; i++) {
		if(buf[i] != g_zip_local_file_header[i]) {
			printf("hdr[%d]=%02x\n", i, buf[i]);
			return -1;
		}
	}

	if(((uncompr_dsize | compr_dsize) >> 31) != 0) {
		printf("Size >2GB, not supported\n");
		return -1;
	}
	bit_flags = cfg_get_le16(&(buf[6]));
	compr_method = cfg_get_le16(&(buf[8]));
	// compr_size = cfg_get_le32(&(buf[18]));	// Probably 0
	// uncompr_size = cfg_get_le32(&(buf[22]));	// Probably 0
	name_len = cfg_get_le16(&(buf[26]));
	extra_len = cfg_get_le16(&(buf[28]));

	// The ZIP file format is annoying, the local header doesn't have
	//  compr_size and uncompr_size generally (if bit_flags bit 3 is set).
	// Even if it does, it's fine to always use the central directory
	printf("bit_flags: %04x\n", bit_flags);

	compr_doffset = dlocal_header_off + 30 + name_len + extra_len;

	cptr = undeflate_malloc(compr_dsize + 0x1000);
	for(i = 0; i < 0x1000; i++) {
		cptr[compr_dsize + i] = 0;
	}

	dret = cfg_read_from_fd(fd, cptr, compr_doffset, compr_dsize);
	if(dret != compr_dsize) {
		return -1;
	}
	dsk->raw_dsize = uncompr_dsize;
	dsk->dimage_size = uncompr_dsize;

	ret = -1;
	if(compr_method == 0) {			// Stored, just use cptr
		dsk->raw_data = cptr;
		dsk->raw_dsize = uncompr_dsize;
		dsk->dimage_start = 0;
		close(fd);
		dsk->fd = 0;
		cptr = 0;			// So free(cptr) does nothing
		ret = 0;
	} else if(compr_method == 8) {		// Deflate
		cptr2 = undeflate_zipfile_blocks(dsk, cptr, compr_dsize);
		printf("undeflate_zipfile_blocks ret:%p\n", cptr2);
		if(cptr2 != 0) {
			ret = 0;
		}
	} else {
		printf("Unknown compr_method:%04x\n", compr_method);
	}
	free(cptr);
	undeflate_free_tables();

	return ret;
}

int
undeflate_zipfile_search(byte *bptr, byte *cmp_ptr, int size, int cmp_len,
				int min_size)
{
	int	pos, good;
	int	i;

	// Search for cmp_ptr in the bptr buffer (basically, look for "PKxx"
	//  header strings).
	pos = size - min_size;
	good = 0;
	while(pos >= 0) {
		good = 1;
		for(i = 0; i < cmp_len; i++) {
			if(bptr[pos + i] != cmp_ptr[i]) {
				good = 0;
				break;
			}
		}
		if(good) {
			break;
		}
		pos--;
	}

	if(!good) {
		return -1;
	}
	return pos;
}

int
undeflate_zipfile_make_list(int fd)
{
	byte	buf[1024];
	dword64	dret, dsize, dir_doff, dir_dsize, unc_dsize, compr_dsize;
	dword64	local_dheader, dneg1, dval, doff, dpos, dlen;
	byte	*dirptr, *name_ptr, *bptr, *bptr2;
	char	*str;
	word32	extra_len, comment_len, ent, entries, part_len, inc;
	word32	tmp_off, ex_off, this_size, this_id;
	int	pos, good, add_it, need_compr, need_unc, need_dheader;
	int	name_len;
	int	i;

	dret = cfg_read_from_fd(fd, &buf[0], 0, 64);
	if(dret != 64) {
		return 0;		// Not a ZIP file
	}

	// See if it's a PKZIP file, starting 0x50, 0x4b, 0x03, 0x04
	for(i = 0; i < 4; i++) {
		if(buf[i] != g_zip_local_file_header[i]) {
			return 0;
		}
	}

	printf("This looks like a .zip file\n");

	// Find end of central directory record in last 1024 bytes.  If it's
	//  not there, this is too complex of a ZIP file for us, give up
	dsize = cfg_get_fd_size(fd);

	for(i = 0; i < 1024; i++) {
		buf[i] = 0;
	}
	dpos = 0;
	dlen = dsize;
	if(dsize > 1024) {
		dpos = dsize - 1024;
		dlen = 1024;
	}
	dret = cfg_read_from_fd(fd, &buf[0], dpos, dlen);
	if(dret != dlen) {
		return 0;		// Unknown problem
	}

	pos = undeflate_zipfile_search(&buf[0],
				&g_zip_end_central_dir_header[0], 1024, 8, 22);
			// End of Central Directory is at least 22 bytes
	if(pos < 0) {
		printf("Cannot parse this .zip file\n");
		return 0;
	}

	entries = cfg_get_le16(&(buf[pos + 8]));
	dir_dsize = cfg_get_le32(&(buf[pos + 12]));
	dir_doff = cfg_get_le32(&(buf[pos + 16]));

#if 0
	printf(".zip entries:%04x, dir_dsize:%06llx, dir_doff:%08llx\n",
					entries, dir_dsize, dir_doff);
#endif
	dneg1 = 0xffffffffULL;
	if(dir_doff == dneg1) {
		printf("We must look for the ZIP64 end dir locator\n");
		pos = undeflate_zipfile_search(&buf[0],
			&g_zip64_end_central_dir_locator[0], 1024, 8, 20);
		if(pos < 0) {
			printf("Cannot parse this ZIP64 file\n");
			return 0;
		}
		doff = cfg_get_le64(&(buf[pos + 8]));
		printf("ZIP64 end of central dir record at 0x%08llx\n", doff);
		if((doff + 64) > dsize) {
			printf("End Central Dir record out of bounds\n");
			return 0;
		}
		// Now read end of central directory record.  Just read 64 bytes
		//  It has to be at least 56 bytes, and the locator had to be
		//  after, so it must fit
		dret = cfg_read_from_fd(fd, &buf[0], doff, 64);
		if(dret != 64) {
			return 0;		// Unknown problem
		}
		pos = undeflate_zipfile_search(&buf[0],
			&g_zip64_end_central_dir_header[0], 64, 4, 64);
		if(pos != 0) {
			printf("ZIP64 end of central dir record not found\n");
			return 0;
		}

		entries = cfg_get_le32(&(buf[32]));
		dir_dsize = cfg_get_le64(&(buf[40]));
		dir_doff = cfg_get_le64(&(buf[48]));
	}

	if((entries < 1) || (dir_dsize > dsize) || (dir_dsize > (1L << 20)) ||
					((dir_doff + dir_dsize) > dsize)) {
		printf("Malformed zip file\n");
		return 0;
	}

	dirptr = undeflate_malloc(dir_dsize);
	dret = cfg_read_from_fd(fd, dirptr, dir_doff, dir_dsize);
	if(dret != dir_dsize) {
		printf("Couldn't read central dir\n");
		return 0;
	}

	part_len = cfg_partition_maybe_add_dotdot();
		// part_len is strlen(g_cfg_part_path[]);

	pos = 0;
	ent = 0;
	while(pos < (int)dir_dsize) {
#if 0
		printf("Working on ent %d at pos %d\n", ent, pos);
#endif
		if(ent >= entries) {
			break;		// all done
		}
		good = 1;
		for(i = 0; i < 4; i++) {
			if(dirptr[pos + i] != g_zip_central_file_header[i]) {
				// corrupt index, get out
				printf("At pos %04x, i:%d bad hdr\n", pos, i);
				good = 0;
				break;
			}
		}
		if(!good) {
			break;
		}
		compr_dsize = cfg_get_le32(&dirptr[pos + 20]);
		unc_dsize = cfg_get_le32(&dirptr[pos + 24]);
		name_len = cfg_get_le16(&dirptr[pos + 28]);
		extra_len = cfg_get_le16(&dirptr[pos + 30]);
		comment_len = cfg_get_le16(&dirptr[pos + 32]);
		local_dheader = cfg_get_le32(&dirptr[pos + 42]);
		if((pos + 46UL + name_len) > dir_dsize) {
			printf("Corrupt entry: pos:%04x, name_len:%04x, "
				"dir_dsize:%05llx\n", pos, name_len, dir_dsize);
			break;
		}

		need_unc = (unc_dsize == dneg1);
		need_compr = (compr_dsize == dneg1);
		need_dheader = (local_dheader == dneg1);

		// Walk extras to update unc/compr size and file offset, if
		//  the standard fields are 0xffffffff.
		bptr = &(dirptr[pos + 46 + name_len]);
		ex_off = 0;
		add_it = 1;
		while(ex_off < extra_len) {
#if 0
			printf("Working on ex_off:%d out of %d\n", ex_off,
								extra_len);
#endif
			this_id = cfg_get_le16(&bptr[ex_off]);
			this_size = cfg_get_le16(&bptr[ex_off + 2]);
			if((this_size + ex_off + pos + 46UL + name_len) >
								dir_dsize) {
				printf("Corrupt ZIP64 extra info entry\n");
				add_it = 0;
				break;
			}
			ex_off += 4;
			if(this_id == 0x0001) {
				tmp_off = 0;
				bptr2 = &(bptr[ex_off]);
				while(tmp_off < this_size) {
					dval = cfg_get_le64(bptr2);
#if 0
					printf("tmp_off %d of %d, dval:"
						"%016llx\n", tmp_off,
						this_size, dval);
#endif
					if(need_compr) {
						compr_dsize = dval;
						need_compr = 0;
					} else if(need_unc) {
						unc_dsize = dval;
						need_unc = 0;
					} else if(need_dheader) {
						local_dheader = dval;
						need_dheader = 0;
					} else {
						printf("Corrupt ZIP64\n");
						add_it = 0;
					}
					tmp_off += 8;
					bptr += 8;
				}
			}
			ex_off += this_size;
		}

		if(need_unc || need_compr || need_dheader) {
			printf("Bad ZIP64 overrides\n");
			add_it = 0;
		}

		// See if filename is at the proper depth
		name_ptr = &(dirptr[pos + 46]);
		if(add_it) {
			add_it = cfg_partition_name_check(name_ptr, name_len);
		}

		//printf("ent:%d name:%s len:%d had add_it:%d, part_len:%d\n",
		//		ent, name_ptr, name_len, add_it, part_len);

		inc = 46 + name_len + extra_len + comment_len;
		if(add_it) {
			// Handle directories either explicitly listed, as
			//  foo/, foo/bar/, foo/bar/1, foo/bar/2 ; or
			//  implied:  foo/bar/1 and foo/bar/2 as entries
			//  implies foo/ and foo/bar/ are directories.
			// Add any name at the current part_len level, but
			//  make sure it's unique (don't add lots of "foo"s).
			name_ptr += part_len;
			name_len -= part_len;
			if(name_len <= 0) {
				add_it = 0;
			}
			for(i = 0; i < name_len; i++) {
				if(name_ptr[i] == '/') {
					// This ends this name at this level
					if(i > 0) {
						add_it = 2;
						name_len = i + 1;
					} else {
						add_it = 0;
					}
					break;
				}
			}
		}
		if((add_it < 2) && (unc_dsize < 140*1024)) {
			add_it = 0;
		}
		if(add_it) {
			str = malloc(name_len + 1);
			cfg_strncpy(str, (char *)&name_ptr[0], name_len + 1);
			cfg_file_add_dirent_unique(&g_cfg_partitionlist, str,
				add_it - 1, unc_dsize, local_dheader,
				compr_dsize, ent);
			free(str);
		}
		pos += inc;
		ent++;
	}
	free(dirptr);

	printf("Returning %d, pos:%05x, dir_dsize:%05llx\n", ent, pos,
								dir_dsize);
	return g_cfg_partitionlist.last;
}

