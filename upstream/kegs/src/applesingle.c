const char rcsid_applesing_c[] = "@(#)$KmKId: applesingle.c,v 1.5 2021-12-19 04:14:31+00 kentd Exp $";

/************************************************************************/
/*			KEGS: Apple //gs Emulator			*/
/*			Copyright 2021 by Kent Dickey			*/
/*									*/
/*	This code is covered by the GNU GPL v3				*/
/*	See the file COPYING.txt or https://www.gnu.org/licenses/	*/
/*	This program is provided with no warranty			*/
/*									*/
/*	The KEGS web page is kegs.sourceforge.net			*/
/*	You may contact the author at: kadickey@alumni.princeton.edu	*/
/************************************************************************/

// From Wikipedia AppleSingle_and_AppleDouble_formats):
// https://web.archive.org/web/20180311140826/http://kaiser-edv.de/
//		documents/AppleSingle_AppleDouble.pdf
// All fields in an Applesingle file are in big-endian format
// ProDOS forked files are described in Technote tn-pdos-025.

#include "defc.h"

word32
applesingle_get_be32(const byte *bptr)
{
	return (bptr[0] << 24) | (bptr[1] << 16) | (bptr[2] << 8) | bptr[3];
}

word32
applesingle_get_be16(const byte *bptr)
{
	return (bptr[0] << 8) | bptr[1];
}

void
applesingle_set_be32(byte *bptr, word32 val)
{
	bptr[3] = val;
	bptr[2] = val >> 8;
	bptr[1] = val >> 16;
	bptr[0] = val >> 24;
}

void
applesingle_set_be16(byte *bptr, word32 val)
{
	bptr[1] = val;
	bptr[0] = val >> 8;
}

word32
applesingle_map_from_prodos(Disk *dsk, Dynapro_file *fileptr, int do_file_data)
{
	byte	*fptr, *bptr;
	word32	data_size, resource_size, rounded_data_size, num_entries;
	word32	rounded_resource_size, hdr_size, max_size, hdr_pos, data_pos;
	word32	block, key_block, ret, good, has_finder_info, offset;
	int	level;
	int	i, j;

#if 0
	printf("applesingle_map_from_prodos: %s do_file_data:%d\n",
					fileptr->unix_path, do_file_data);
#endif

	// First, handle mini directory describing the forks
	key_block = fileptr->key_block;
	ret = dynapro_map_one_file_block(dsk, fileptr, key_block, 1U << 30, 0);
	if(ret == 0) {
		printf(" dynapro_map_one_file_block ret 0, applesingle done\n");
		return 0;
	}
	bptr = &(dsk->raw_data[key_block << 9]);
	data_size = dynapro_get_word24(&bptr[5]);
	resource_size = dynapro_get_word24(&bptr[0x100 + 5]);
	has_finder_info = bptr[9] | bptr[27];

	num_entries = 1;		// ProDOS info always
	if(has_finder_info) {
		num_entries++;
	}
	rounded_data_size = data_size;
	if(data_size) {
		rounded_data_size = (data_size + 0x200) & -0x200;
		num_entries++;
	}
	rounded_resource_size = resource_size;
	if(resource_size) {
		rounded_resource_size = (resource_size + 0x200) & -0x200;
		num_entries++;
	}
	hdr_size = 256;
	max_size = hdr_size + rounded_resource_size + rounded_data_size;
	fileptr->buffer_ptr = 0;
	fptr = 0;
	if(do_file_data) {
		fptr = calloc(1, max_size + 0x200);
#if 0
		printf(" fptr:%p, max_size:%08x, res:%08x, data:%08x\n",
			fptr, max_size, rounded_resource_size,
			rounded_data_size);
#endif
	}

	// From now on, errors cannot return without free'ing fptr
	good = 1;
	if(resource_size) {
		block = dynapro_get_word16(&bptr[0x100 + 1]);
		level = bptr[0x100];
		if(fptr) {
			fileptr->buffer_ptr = fptr + 256;
		}
		ret = dynapro_map_file_blocks(dsk, fileptr, block, level, 0,
								resource_size);
		if(ret == 0) {
			good = 0;
		}
	}

	if(data_size) {
		block = dynapro_get_word16(&bptr[1]);
		level = bptr[0];
		if(fptr) {
			fileptr->buffer_ptr = fptr + 256 +
							rounded_resource_size;
		}
		ret = dynapro_map_file_blocks(dsk, fileptr, block, level, 0,
								data_size);
		if(ret == 0) {
			good = 0;
		}
	}

	fileptr->buffer_ptr = 0;

	// Now prepare the header
	if(fptr) {
		applesingle_set_be32(&fptr[0], 0x00051600);	// Magic
		applesingle_set_be32(&fptr[4], 0x00020000);	// Version
		applesingle_set_be16(&fptr[24], num_entries);	// Version
		hdr_pos = 26;
		data_pos = 192;

		// First do ProDOS entry
		applesingle_set_be32(&fptr[hdr_pos + 0], 11);	// ProDOS Info
		applesingle_set_be32(&fptr[hdr_pos + 4], data_pos);
		applesingle_set_be32(&fptr[hdr_pos + 8], 8);

		applesingle_set_be16(&fptr[data_pos + 0], 0xc3);
		applesingle_set_be16(&fptr[data_pos + 2], fileptr->file_type);
		applesingle_set_be32(&fptr[data_pos + 4], fileptr->aux_type);
		hdr_pos += 12;
		data_pos += 8;

		// Then do FinderInfo
		if(has_finder_info) {
			applesingle_set_be32(&fptr[hdr_pos + 0], 9);	//Finder
			applesingle_set_be32(&fptr[hdr_pos + 4], data_pos);
			applesingle_set_be32(&fptr[hdr_pos + 8], 32);

			for(i = 0; i < 2; i++) {
				offset = bptr[9 + 18*i];
				if(!offset) {
					continue;		// skip it
				}
				offset = ((offset - 1) & 1) * 8;
				for(j = 0; j < 9; j++) {
					fptr[data_pos + offset + j] =
						bptr[10 + 18*i + j];
				}
			}
			hdr_pos += 12;
			data_pos += 32;
		}

		if(data_pos >= 256) {
			printf("data_pos:%08x is too big\n", data_pos);
			good = 0;
		}

		// First, do data fork
		if(data_size) {
			applesingle_set_be32(&fptr[hdr_pos + 0], 1);	// Data
			applesingle_set_be32(&fptr[hdr_pos + 4],
						256 + rounded_resource_size);
			applesingle_set_be32(&fptr[hdr_pos + 8], data_size);
			hdr_pos += 12;
		}
		// Then do resource fork
		if(resource_size) {
			applesingle_set_be32(&fptr[hdr_pos + 0], 2);	// Rsrc
			applesingle_set_be32(&fptr[hdr_pos + 4], 256);
			applesingle_set_be32(&fptr[hdr_pos + 8], resource_size);
			hdr_pos += 12;
		}
		if(hdr_pos > 192) {
			printf("hdr:%08x stomped on data\n", hdr_pos);
			good = 0;
		}
		if(good) {
			ret = dynapro_write_to_unix_file(fileptr->unix_path,
				fptr, 256 + rounded_resource_size + data_size);
			if(ret == 0) {
				good = 0;
			}
		}

		free(fptr);
	}

	// printf("applesingle_map_from_prodos done, good:%d\n", good);
	return good;
}

word32
applesingle_from_unix(Disk *dsk, Dynapro_file *fileptr, byte *fptr,
								dword64 dsize)
{
	byte	*bptr, *tptr;
	word32	key_block, blocks_used, entry_id, blocks_out, offset, length;
	word32	magic, version, hdr_pos, did_fork;
	int	num_entries;
	int	i;

	// Return 0 if anything is wrong with the .applesingle file
	// Otherwise, return (blocks_used << 16) | (key_block & 0xffff)

#if 0
	printf("applesingle_from_unix %s, size:%08llx\n", fileptr->unix_path,
								dsize);
#endif

	key_block = fileptr->key_block;
	bptr = &(dsk->raw_data[key_block << 9]);

	if(dsize < 50) {
		printf("Applesingle is too small\n");
		return 0;
	}
	magic = applesingle_get_be32(&fptr[0]);
	version = applesingle_get_be32(&fptr[4]);
	num_entries = applesingle_get_be16(&fptr[24]);
	if((magic != 0x00051600) || (version < 0x00020000)) {
		printf("Bad Applesingle magic number is: %08x, vers:%08x\n",
					magic, version);
		return 0;
	}
	hdr_pos = 26;
	blocks_used = 1;
	did_fork = 0;
	// printf(" num_entries:%d\n", num_entries);
	for(i = 0; i < num_entries; i++) {
		if((hdr_pos + 24) > dsize) {
			printf("Applesingle header exceeds file size i:%d of "
				"%d, hdr_pos:%04x dsize:%08llx\n", i,
				num_entries, hdr_pos, dsize);
			return 0;
		}
		entry_id = applesingle_get_be32(&fptr[hdr_pos + 0]);
		offset = applesingle_get_be32(&fptr[hdr_pos + 4]);
		length = applesingle_get_be32(&fptr[hdr_pos + 8]);
#if 0
		printf(" header[%d] at +%04x: id:%d, offset:%08x, len:%08x\n",
			i, hdr_pos, entry_id, offset, length);
#endif
		if((offset + length) > dsize) {
			printf("Applesingle entry_id:%d exceeds file size\n",
				entry_id);
			return 0;
		}
		switch(entry_id) {
		case 1:		// Data fork
		case 2:		// Resource fork
			tptr = bptr;
			if(entry_id == 2) {		// Resource fork
				tptr += 0x100;
			}
#if 0
			printf(" for entry_id %d, offset:%08x, length:%08x, "
				"fptr:%p\n", entry_id, offset, length, fptr);
#endif
			if(did_fork & (1 << entry_id)) {
				printf("fork %d repeated!\n", entry_id);
				return 0;
			}
			did_fork |= (1 << entry_id);
			blocks_out = applesingle_make_prodos_fork(dsk,
						fptr + offset, tptr, length);
			if(blocks_out == 0) {
				return 0;
			}
			blocks_used += (blocks_out >> 16);
			break;
		case 9:		// Finder Info
			if(length < 32) {
				printf("Invalid Finder info, len:%d\n", length);
			}
			bptr[8] = 0x12;
			bptr[8 + 18] = 0x12;
			bptr[9] = 1;
			bptr[9 + 18] = 2;
			for(i = 0; i < 16; i++) {
				bptr[10 + i] = fptr[offset + i];
				bptr[10 + 18 + i] = fptr[offset + 16 + i];
			}
			break;
		case 11:	// ProDOS File Info
			fileptr->file_type = fptr[offset + 3];
			fileptr->aux_type = (fptr[offset + 6] << 8) |
							fptr[offset + 7];
			break;
		default:
			break;		// Ignore it
		}
		hdr_pos += 12;
	}

	for(i = 1; i < 3; i++) {
		if((did_fork >> i) & 1) {
			continue;
		}
		// Create one block for this fork even though it's length==0
		// i==1: no data fork; i==2: no resource fork
		printf(" Doing dummy fork, i:%d, fptr:%p\n", i, fptr);
		blocks_out = applesingle_make_prodos_fork(dsk, fptr,
						bptr + ((i & 2) * 0x80), 0);
		if(blocks_out == 0) {
			return blocks_out;
		}
		blocks_used += (blocks_out >> 16);
	}

	fileptr->eof = 0x200;
	return (blocks_used << 16) | key_block;
}

word32
applesingle_make_prodos_fork(Disk *dsk, byte *fptr, byte *tptr, word32 length)
{
	word32	block, blocks_out, storage_type;

#if 0
	printf("applesingle_make_prodos_fork: fptr:%p, tptr:%p, length:%08x\n",
			fptr, tptr, length);
#endif

	// Handle creating either a resource or data fork
	block = dynapro_find_free_block(dsk);
	if(block == 0) {
		return 0;
	}
	blocks_out = dynapro_fork_from_unix(dsk, fptr, &storage_type, block,
								length);

	// printf(" dynapro_fork_from_unix ret: %08x, storage:%02x\n",
	//					blocks_out, storage_type);
	tptr[0] = storage_type >> 4;
	tptr[1] = blocks_out & 0xff;		// key_block lo
	tptr[2] = (blocks_out >> 8) & 0xff;	// key_block hi
	tptr[3] = (blocks_out >> 16) & 0xff;	// blocks_used lo
	tptr[4] = (blocks_out >> 24) & 0xff;	// blocks_used hi
	tptr[5] = length & 0xff;		// eof lo
	tptr[6] = (length >> 8) & 0xff;		// eof mid
	tptr[7] = (length >> 16) & 0xff;	// eof hi
	return blocks_out;
}
