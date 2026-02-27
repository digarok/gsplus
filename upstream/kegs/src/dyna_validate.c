const char rcsid_dyna_validate_c[] = "@(#)$KmKId: dyna_validate.c,v 1.8 2023-05-21 20:06:24+00 kentd Exp $";

/************************************************************************/
/*			KEGS: Apple //gs Emulator			*/
/*			Copyright 2021-2022 by Kent Dickey		*/
/*									*/
/*	This code is covered by the GNU GPL v3				*/
/*	See the file COPYING.txt or https://www.gnu.org/licenses/	*/
/*	This program is provided with no warranty			*/
/*									*/
/*	The KEGS web page is kegs.sourceforge.net			*/
/*	You may contact the author at: kadickey@alumni.princeton.edu	*/
/************************************************************************/

// Main information is from Beneath Apple ProDOS which has disk layout
//  descriptions.  Forked files are described in Technote tn-pdos-025.

#include "defc.h"

word32
dynapro_validate_header(Disk *dsk, Dynapro_file *fileptr, word32 dir_byte,
						word32 parent_dir_byte)
{
	word32	storage_type, exp_type, val, parent_block, exp_val;

	storage_type = fileptr->prodos_name[0] & 0xf0;
	exp_type = 0xe0;
	if(dir_byte == 0x0404) {
		exp_type = 0xf0;		// Volume header
	}
	if(storage_type != exp_type) {
		printf("Volume/Dir header is %02x at %07x\n",
						storage_type, dir_byte);
		return 0;
	}

	if(fileptr->aux_type != 0x0d27) {
		printf("entry_length, entries_per_block:%04x at %07x\n",
			fileptr->aux_type, dir_byte);
		return 0;
	}

	if(exp_type == 0xf0) {			// Volume header
		val = fileptr->lastmod_time >> 16;
		if(val != 6) {
			printf("bit_map_ptr:%04x, should be 6\n", val);
			return 0;
		}
		val = fileptr->header_pointer;
		if(val != (dsk->dimage_size >> 9)) {
			printf("Num blocks at %07x is wrong: %04x\n", dir_byte,
							val);
			return 0;
		}
	} else {				// Directory header
		val = fileptr->lastmod_time >> 16;	// parent_pointer
		parent_block = parent_dir_byte >> 9;
		if(val != parent_block) {
			printf("Dir at %07x parent:%04x should be %04x\n",
				dir_byte, val, parent_block);
			return 0;
		}
		val = fileptr->header_pointer;
		exp_val = ((parent_dir_byte & 0x1ff) - 4) / 0x27;
		exp_val = (exp_val + 1) | 0x2700;
		if(val != exp_val) {
			printf("Parent entry at %07x is:%04x, should be:%04x\n",
				dir_byte, val, exp_val);
			return 0;
		}
	}

	return 1;
}

void
dynapro_validate_init_freeblks(byte *freeblks_ptr, word32 num_blocks)
{
	word32	num_map_blocks, mask;
	int	pos;
	word32	ui;

	for(ui = 0; ui < (num_blocks + 7)/8; ui++) {
		freeblks_ptr[ui] = 0xff;
	}
	freeblks_ptr[0] &= 0x3f;
	if(num_blocks & 7) {
		freeblks_ptr[num_blocks / 8] = 0xff00 >> (num_blocks & 7);
	}

	num_map_blocks = (num_blocks + 4095) >> 12;	// 4096 bits per block
	for(ui = 0; ui < num_map_blocks; ui++) {
		// Mark blocks used in the bitmap as in use
		pos = (ui + 6) >> 3;
		mask = 0x80 >> ((ui + 6) & 7);
		freeblks_ptr[pos] &= (~mask);
	}
}

word32
dynapro_validate_freeblk(Disk *dsk, byte *freeblks_ptr, word32 block)
{
	word32	mask, ret;
	int	pos;

	// Return != 0 if block is free (which is success), returns == 0
	//  if it is in use (which is an error).  Marks block as in use
	pos = block >> 3;
	if(block >= (dsk->dimage_size >> 9)) {
		return 0x100;		// Out of range
	}
	mask = 0x80 >> (block & 7);
	ret = freeblks_ptr[pos] & mask;
	freeblks_ptr[pos] &= (~mask);

	if(!ret) {
		printf("Block %04x was already in use\n", block);
	}
	return ret;
}

word32
dynapro_validate_file(Disk *dsk, byte *freeblks_ptr, word32 block_num,
						word32 eof, int level_first)
{
	byte	*bptr;
	word32	num_blocks, tmp, ret, exp_blocks, extra_blocks;
	int	level, first;
	int	i;

	level = level_first & 0xf;
	first = level_first & 0x10;

	if(!dynapro_validate_freeblk(dsk, freeblks_ptr, block_num)) {
		return 0;
	}
	if(level_first == 0x15) {
		return dynapro_validate_forked_file(dsk, freeblks_ptr,
							block_num, eof);
	}
	if((level < 1) || (level >= 4)) {
		printf("level %d out of range, %08x\n", level, level_first);
		return 0;
	}
	if(level == 1) {
		return 1;
	}
	num_blocks = 1;
	bptr = &(dsk->raw_data[block_num * 0x200]);
	for(i = 0; i < 256; i++) {
		tmp = bptr[i] + (bptr[256 + i] << 8);
		if(tmp == 0) {
			if(first) {
				printf("First block is spare, illegal!\n");
				return 0;
			}
			continue;
		}
		ret = dynapro_validate_file(dsk, freeblks_ptr, tmp, eof,
							first | (level - 1));
		if(ret == 0) {
			return 0;
		}
		num_blocks += ret;
		first = 0;
	}

	if(level_first & 0x10) {
		// Try to estimate exp_blocks based on eof
		exp_blocks = (eof + 0x1ff) >> 9;
		if(exp_blocks == 0) {
			exp_blocks = 1;
		} else if(exp_blocks > 1) {
			// Add in sapling blocks
			extra_blocks = ((exp_blocks + 255) >> 8);
			if(exp_blocks > 256) {
				extra_blocks++;
			}
			exp_blocks += extra_blocks;
		}
		if(num_blocks > exp_blocks) {
			printf("blocks_used:%04x, eof:%07x, exp:%04x\n",
				num_blocks, eof, exp_blocks);
			return 0;
		}
	}
	return num_blocks;
}

word32
dynapro_validate_forked_file(Disk *dsk, byte *freeblks_ptr, word32 block_num,
								word32 eof)
{
	byte	*bptr;
	word32	num_blocks, tmp, ret, size, type, exp_blocks;
	int	level;
	int	i;

	bptr = &(dsk->raw_data[block_num * 0x200]);

	if(eof != 0x200) {
		printf("In forked file block %04x, eof in dir:%08x, exp 0200\n",
								block_num, eof);
		return 0;
	}
	// Check that most of the block is 0
	for(i = 44; i < 512; i++) {
		if((i >= 0x100) && (i < 0x108)) {
			continue;
		}
		if(bptr[i] != 0) {
			printf("In forked file block:%04x, byte %03x is %02x\n",
				block_num, i, bptr[i]);
			return 0;
		}
	}

	// Check for basic Finder Info format
	for(i = 0; i < 2; i++) {
		size = bptr[8 + 18*i];
		type = bptr[9 + 18*i];
		if(((size != 0) && (size != 18)) || (type > 2)) {
			printf("Finder Info size %04x+%03x=%02x, type:%02x\n",
					block_num, 8 + 18*i, size, type);
			return 0;
		}
	}

	num_blocks = 1;
	for(i = 0; i < 2; i++) {
		tmp = bptr[1 + 0x100*i] + (bptr[2 + 0x100*i] << 8);
		if(tmp == 0) {
			printf("First fork %d block is spare, illegal!\n", i);
			return 0;
		}
		eof = bptr[5 + 0x100*i] + (bptr[6 + 0x100*i] << 8) +
						(bptr[7 + 0x100*i] << 16);
		level = bptr[0 + 0x100*i];
		ret = dynapro_validate_file(dsk, freeblks_ptr, tmp, eof,
								0x10 | level);
		if(ret == 0) {
			printf("Fork %d failed, eof:%08x, block:%04x "
				"fork:%04x, level:%d\n", i, eof, block_num,
				tmp, level);
			return 0;
		}
		exp_blocks = bptr[3 + 0x100*i] + (bptr[4 + 0x100*i] << 8);
		if(ret != exp_blocks) {
			printf("Fork %d at %04x, blocks:%04x, exp:%04x\n",
				i, block_num, ret, exp_blocks);
		}
		num_blocks += ret;
	}

	return num_blocks;
}

word32
dynapro_validate_dir(Disk *dsk, byte *freeblks_ptr, word32 dir_byte,
				word32 parent_dir_byte, word32 exp_blocks_used)
{
	char	buf32[32];
	Dynapro_file localfile;
	byte	*bptr;
	word32	start_dir_block, last_block, max_block, tmp_byte, sub_blocks;
	word32	ret, act_entries, exp_entries, blocks_used, prev, next;
	int	cnt, is_header;

	// Read directory, make sure each entry is consistent
	// Return 0 if there is damage, != 0 if OK.
	bptr = dsk->raw_data;
	start_dir_block = dir_byte >> 9;
	last_block = 0;
	max_block = (word32)(dsk->dimage_size >> 9);
	cnt = 0;
	is_header = 1;
	exp_entries = 0xdeadbeef;
	act_entries = 0;
	blocks_used = 0;
	while(dir_byte) {
		if((dir_byte & 0x1ff) == 4) {
			// First entry in this block, check prev/next
			tmp_byte = dir_byte & -0x200;		// Block align
			prev = dynapro_get_word16(&bptr[tmp_byte + 0]);
			next = dynapro_get_word16(&bptr[tmp_byte + 2]);
			if((prev != last_block) || (next >= max_block)) {
				printf("dir at %07x is damaged in links\n",
								dir_byte);
				return 0;
			}
			last_block = dir_byte >> 9;
			ret = dynapro_validate_freeblk(dsk, freeblks_ptr,
								dir_byte >> 9);
			if(!ret) {
				return 0;
			}
			blocks_used++;
		}
		if(cnt++ >= 65536) {
			printf("Loop detected, dir_byte:%07x\n", dir_byte);
			return 0;
		}
		ret = dynapro_fill_fileptr_from_prodos(dsk, &localfile,
							&buf32[0], dir_byte);
		if(ret == 0) {
			return 0;
		}
		if(ret != 1) {
			act_entries = act_entries + 1 - is_header;
		}
		if(is_header) {
			if(ret == 1) {
				printf("Volume/Dir header is erased\n");
				return 0;
			}
			ret = dynapro_validate_header(dsk, &localfile, dir_byte,
							parent_dir_byte);
			if(ret == 0) {
				return 0;
			}
			exp_entries = localfile.lastmod_time & 0xffff;
		} else if(ret != 1) {
			if(localfile.header_pointer != start_dir_block) {
				printf("At %07x, header_ptr:%04x != %04x\n",
					dir_byte, localfile.header_pointer,
					start_dir_block);
				return 0;
			}
			if(localfile.prodos_name[0] >= 0xd0) {
				sub_blocks = localfile.blocks_used;
				if(localfile.eof != (sub_blocks * 0x200UL)) {
					printf("At %07x, eof:%08x != %08x\n",
						dir_byte, localfile.eof,
						sub_blocks * 0x200U);
					return 0;
				}
				ret = dynapro_validate_dir(dsk, freeblks_ptr,
					(localfile.key_block * 0x200) + 4,
					dir_byte, sub_blocks);
				if(ret == 0) {
					return 0;
				}
			} else {
				ret = dynapro_validate_file(dsk, freeblks_ptr,
					localfile.key_block, localfile.eof,
					0x10 | (localfile.prodos_name[0] >> 4));
				if(ret == 0) {
					printf("At %07x, bad file\n", dir_byte);
					return 0;
				}
				if(localfile.blocks_used != ret) {
					printf("At %07x, blocks_used prodos "
						"%04x != %04x calc\n", dir_byte,
						localfile.blocks_used, ret);
					return 0;
				}
			}
		}

		is_header = 0;
		dir_byte = dir_byte + 0x27;
		tmp_byte = (dir_byte & 0x1ff) + 0x27;
		if(tmp_byte < 0x200) {
			continue;
		}

		tmp_byte = (dir_byte - 0x27) & (0 - 0x200UL);
		dir_byte = dynapro_get_word16(&bptr[tmp_byte + 2]) * 0x200UL;
		if(dir_byte == 0) {
			if(act_entries != exp_entries) {
				printf("act_entries:%04x != exp:%04x, "
					"dir_block:%04x\n", act_entries,
					exp_entries, start_dir_block);
				return 0;
			}
			if(blocks_used != exp_blocks_used) {
				printf("At dir %07x, blocks_used:%04x!=%04x "
					"exp\n", tmp_byte, blocks_used,
					exp_blocks_used);
				return 0;
			}
			return 1;
		}
		dir_byte += 4;
		if(dir_byte >= (max_block * 0x200L)) {
			printf(" invalid link pointer %07x\n", dir_byte);
			return 0;
		}
	}

	return 0;
}

int
dynapro_validate_disk(Disk *dsk)
{
	byte	freeblks[65536/8];		// 8KB
	byte	*bptr;
	word32	num_blocks, ret;
	word32	ui;

	num_blocks = (word32)(dsk->dimage_size >> 9);
	printf("******************************\n");
	printf("Validate disk: %s, blocks:%05x\n", dsk->name_ptr, num_blocks);
	dynapro_validate_init_freeblks(&freeblks[0], num_blocks);

	// Validate starting at directory in block 2
	ret = dynapro_validate_dir(dsk, &freeblks[0], 0x0404, 0, 4);
	if(!ret) {
		printf("Disk does not validate!\n");
		exit(1);
		return ret;
	}

	// Check freeblks
	bptr = &(dsk->raw_data[6*0x200]);
	for(ui = 0; ui < (num_blocks + 7)/8; ui++) {
		if(freeblks[ui] != bptr[ui]) {
			printf("Expected free mask for blocks %04x-%04x:%02x, "
				"but it is %02x\n", ui*8, ui*8 + 7,
				freeblks[ui], bptr[ui]);
			exit(1);
			return 0;
		}
	}
	return 1;
}

void
dynapro_validate_any_image(Disk *dsk)
{
	byte	*bufptr;
	dword64	dsize;
	int	ret;

	// If dsk->raw_data already set, just use it.  Otherwise, we need to
	//  temporarily read in entire image, set it, do validate, and then
	//  free it

	if(dsk->fd < 0) {
		return;		// No disk
	}
	if(dsk->wozinfo_ptr) {
		return;
	}
	dsize = dsk->dimage_size;
	bufptr = 0;
	if((dsize >> 31) != 0) {
		printf("Disk is too large, not valid\n");
		ret = 0;
	} else if(dsk->raw_data == 0) {
		bufptr = malloc((size_t)dsize);
		dsk->raw_data = bufptr;
		cfg_read_from_fd(dsk->fd, bufptr, 0, dsize);
		ret = dynapro_validate_disk(dsk);
		dsk->raw_data = 0;
		free(bufptr);
	} else {
		ret = dynapro_validate_disk(dsk);
	}
	printf("validate_disk returned is_good: %d (0 is bad)\n", ret);
}

