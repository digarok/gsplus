const char rcsid_dynapro_c[] = "@(#)$KmKId: dynapro.c,v 1.49 2023-09-23 17:53:24+00 kentd Exp $";

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
//  descriptions.  Upper/lowercase is from Technote tn-gsos-008, and
//  forked files are storage_type $5 from Technote tn-pdos-025.

#include "defc.h"
#ifdef _WIN32
# include "win_dirent.h"
#else
# include <dirent.h>
#endif
#include <time.h>

extern int Verbose;

#define DYNAPRO_PATH_MAX		2048
char g_dynapro_path_buf[DYNAPRO_PATH_MAX];

extern word32 g_vbl_count, g_iwm_dynapro_last_vbl_count;

byte g_prodos_block0[512] = {
	// From Beagle Bros Pro-Byter disk
	// This is a ProDOS boot block, able to load PRODOS and jump to it
	0x01, 0x38, 0xb0, 0x03, 0x4c, 0x32, 0xa1, 0x86,	// 0x000
	0x43, 0xc9, 0x03, 0x08, 0x8a, 0x29, 0x70, 0x4a,
	0x4a, 0x4a, 0x4a, 0x09, 0xc0, 0x85, 0x49, 0xa0,	// 0x010
	0xff, 0x84, 0x48, 0x28, 0xc8, 0xb1, 0x48, 0xd0,
	0x3a, 0xb0, 0x0e, 0xa9, 0x03, 0x8d, 0x00, 0x08,	// 0x020
	0xe6, 0x3d, 0xa5, 0x49, 0x48, 0xa9, 0x5b, 0x48,
	0x60, 0x85, 0x40, 0x85, 0x48, 0xa0, 0x63, 0xb1,	// 0x030
	0x48, 0x99, 0x94, 0x09, 0xc8, 0xc0, 0xeb, 0xd0,
	0xf6, 0xa2, 0x06, 0xbc, 0x1d, 0x09, 0xbd, 0x24,	// 0x040
	0x09, 0x99, 0xf2, 0x09, 0xbd, 0x2b, 0x09, 0x9d,
	0x7f, 0x0a, 0xca, 0x10, 0xee, 0xa9, 0x09, 0x85,	// 0x050
	0x49, 0xa9, 0x86, 0xa0, 0x00, 0xc9, 0xf9, 0xb0,
	0x2f, 0x85, 0x48, 0x84, 0x60, 0x84, 0x4a, 0x84,	// 0x060
	0x4c, 0x84, 0x4e, 0x84, 0x47, 0xc8, 0x84, 0x42,
	0xc8, 0x84, 0x46, 0xa9, 0x0c, 0x85, 0x61, 0x85,	// 0x070
	0x4b, 0x20, 0x12, 0x09, 0xb0, 0x68, 0xe6, 0x61,
	0xe6, 0x61, 0xe6, 0x46, 0xa5, 0x46, 0xc9, 0x06,	// 0x080
	0x90, 0xef, 0xad, 0x00, 0x0c, 0x0d, 0x01, 0x0c,
	0xd0, 0x6d, 0xa9, 0x04, 0xd0, 0x02, 0xa5, 0x4a,	// 0x090
	0x18, 0x6d, 0x23, 0x0c, 0xa8, 0x90, 0x0d, 0xe6,
	0x4b, 0xa5, 0x4b, 0x4a, 0xb0, 0x06, 0xc9, 0x0a,	// 0x0a0
	0xf0, 0x55, 0xa0, 0x04, 0x84, 0x4a, 0xad, 0x02,
	0x09, 0x29, 0x0f, 0xa8, 0xb1, 0x4a, 0xd9, 0x02,	// 0x0b0
	0x09, 0xd0, 0xdb, 0x88, 0x10, 0xf6, 0x29, 0xf0,
	0xc9, 0x20, 0xd0, 0x3b, 0xa0, 0x10, 0xb1, 0x4a,	// 0x0c0
	0xc9, 0xff, 0xd0, 0x33, 0xc8, 0xb1, 0x4a, 0x85,
	0x46, 0xc8, 0xb1, 0x4a, 0x85, 0x47, 0xa9, 0x00,	// 0x0d0
	0x85, 0x4a, 0xa0, 0x1e, 0x84, 0x4b, 0x84, 0x61,
	0xc8, 0x84, 0x4d, 0x20, 0x12, 0x09, 0xb0, 0x17,	// 0x0e0
	0xe6, 0x61, 0xe6, 0x61, 0xa4, 0x4e, 0xe6, 0x4e,
	0xb1, 0x4a, 0x85, 0x46, 0xb1, 0x4c, 0x85, 0x47,	// 0x0f0
	0x11, 0x4a, 0xd0, 0xe7, 0x4c, 0x00, 0x20, 0x4c,

	0x3f, 0x09, 0x26, 0x50, 0x52, 0x4f, 0x44, 0x4f,	// 0x100
	0x53, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0xa5, 0x60, 0x85, 0x44, 0xa5, 0x61,	// 0x110
	0x85, 0x45, 0x6c, 0x48, 0x00, 0x08, 0x1e, 0x24,
	0x3f, 0x45, 0x47, 0x76, 0xf4, 0xd7, 0xd1, 0xb6,	// 0x120
	0x4b, 0xb4, 0xac, 0xa6, 0x2b, 0x18, 0x60, 0x4c,
	0xbc, 0x09, 0xa9, 0x9f, 0x48, 0xa9, 0xff, 0x48,	// 0x130
	0xa9, 0x01, 0xa2, 0x00, 0x4c, 0x79, 0xf4, 0x20,
	0x58, 0xfc, 0xa0, 0x1c, 0xb9, 0x50, 0x09, 0x99,	// 0x140
	0xae, 0x05, 0x88, 0x10, 0xf7, 0x4c, 0x4d, 0x09,
	0xaa, 0xaa, 0xaa, 0xa0, 0xd5, 0xce, 0xc1, 0xc2,	// 0x150
	0xcc, 0xc5, 0xa0, 0xd4, 0xcf, 0xa0, 0xcc, 0xcf,
	0xc1, 0xc4, 0xa0, 0xd0, 0xd2, 0xcf, 0xc4, 0xcf,	// 0x160
	0xd3, 0xa0, 0xaa, 0xaa, 0xaa, 0xa5, 0x53, 0x29,
	0x03, 0x2a, 0x05, 0x2b, 0xaa, 0xbd, 0x80, 0xc0,	// 0x170
	0xa9, 0x2c, 0xa2, 0x11, 0xca, 0xd0, 0xfd, 0xe9,
	0x01, 0xd0, 0xf7, 0xa6, 0x2b, 0x60, 0xa5, 0x46,	// 0x180
	0x29, 0x07, 0xc9, 0x04, 0x29, 0x03, 0x08, 0x0a,
	0x28, 0x2a, 0x85, 0x3d, 0xa5, 0x47, 0x4a, 0xa5,	// 0x190
	0x46, 0x6a, 0x4a, 0x4a, 0x85, 0x41, 0x0a, 0x85,
	0x51, 0xa5, 0x45, 0x85, 0x27, 0xa6, 0x2b, 0xbd,	// 0x1a0
	0x89, 0xc0, 0x20, 0xbc, 0x09, 0xe6, 0x27, 0xe6,
	0x3d, 0xe6, 0x3d, 0xb0, 0x03, 0x20, 0xbc, 0x09,	// 0x1b0
	0xbc, 0x88, 0xc0, 0x60, 0xa5, 0x40, 0x0a, 0x85,
	0x53, 0xa9, 0x00, 0x85, 0x54, 0xa5, 0x53, 0x85,	// 0x1c0
	0x50, 0x38, 0xe5, 0x51, 0xf0, 0x14, 0xb0, 0x04,
	0xe6, 0x53, 0x90, 0x02, 0xc6, 0x53, 0x38, 0x20,	// 0x1d0
	0x6d, 0x09, 0xa5, 0x50, 0x18, 0x20, 0x6f, 0x09,
	0xd0, 0xe3, 0xa0, 0x7f, 0x84, 0x52, 0x08, 0x28,	// 0x1e0
	0x38, 0xc6, 0x52, 0xf0, 0xce, 0x18, 0x08, 0x88,
	0xf0, 0xf5, 0xbd, 0x8c, 0xc0, 0x10, 0xfb, 0x00,	// 0x1f0
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

word32
dynapro_get_word32(byte *bptr)
{
	return (bptr[3] << 24) | (bptr[2] << 16) | (bptr[1] << 8) | bptr[0];
}

word32
dynapro_get_word24(byte *bptr)
{
	return (bptr[2] << 16) | (bptr[1] << 8) | bptr[0];
}

word32
dynapro_get_word16(byte *bptr)
{
	return (bptr[1] << 8) | bptr[0];
}

void
dynapro_set_word24(byte *bptr, word32 val)
{
	// Write 3 bytes in little-endian form
	*bptr++ = val;
	*bptr++ = (val >> 8);
	*bptr++ = (val >> 16);
}

void
dynapro_set_word32(byte *bptr, word32 val)
{
	// Write 4 bytes in little-endian form
	*bptr++ = val;
	*bptr++ = (val >> 8);
	*bptr++ = (val >> 16);
	*bptr++ = (val >> 24);
}

void
dynapro_set_word16(byte *bptr, word32 val)
{
	// Write 2 bytes in little-endian form
	*bptr++ = val;
	*bptr++ = (val >> 8);
}

void
dynapro_error(Disk *dsk, const char *fmt, ...)
{
	Dynapro_info *info_ptr;
	va_list	args;

	va_start(args, fmt);
	cfg_err_vprintf("Dynapro", fmt, args);
	va_end(args);
	info_ptr = dsk->dynapro_info_ptr;
	if(info_ptr) {
		cfg_err_printf("", "Path: %s\n", info_ptr->root_path);
	}
}

Dynapro_file *
dynapro_alloc_file()
{
	Dynapro_file *fileptr;

	fileptr = calloc(1, sizeof(Dynapro_file));
	return fileptr;
}

void
dynapro_free_file(Dynapro_file *fileptr, int check_map)
{
	if(!fileptr) {
		return;
	}
	if(fileptr->subdir_ptr) {
		dynapro_free_recursive_file(fileptr->subdir_ptr, check_map);
	}
	fileptr->subdir_ptr = 0;
	free(fileptr->unix_path);
	fileptr->unix_path = 0;
	free(fileptr->buffer_ptr);
	fileptr->buffer_ptr = 0;
	fileptr->next_ptr = 0;
	// printf("FREE %p\n", fileptr);
	if(check_map && (fileptr->map_first_block != 0)) {
		printf(" ERROR: map_first_block is %08x\n",
						fileptr->map_first_block);
		exit(1);
	}
	free(fileptr);
}

void
dynapro_free_recursive_file(Dynapro_file *fileptr, int check_map)
{
	Dynapro_file *nextptr;

	if(!fileptr) {
		return;
	}
	// printf("free_recursive %s\n", fileptr->unix_path);
	while(fileptr) {
		nextptr = fileptr->next_ptr;
		dynapro_free_file(fileptr, check_map);
		fileptr = nextptr;
	};
}

void
dynapro_free_dynapro_info(Disk *dsk)
{
	Dynapro_info *info_ptr;

	info_ptr = dsk->dynapro_info_ptr;
	if(info_ptr) {
		free(info_ptr->root_path);

		dynapro_free_recursive_file(info_ptr->volume_ptr, 0);
		info_ptr->volume_ptr = 0;
	}
	free(info_ptr);
	dsk->dynapro_info_ptr = 0;
}

word32
dynapro_find_free_block_internal(Disk *dsk)
{
	byte	*bptr;
	word32	num_blocks, bitmap_size_bytes, val, mask;
	word32	ui;
	int	j;

	num_blocks = (word32)(dsk->raw_dsize >> 9);
	bitmap_size_bytes = (num_blocks + 7) >> 3;
	bptr = &(dsk->raw_data[6 * 512]);		// Block 6
	for(ui = 0; ui < bitmap_size_bytes; ui++) {
		val = bptr[ui];
		if(val == 0) {
			continue;
		}
		mask = 0x80;
		for(j = 0; j < 8; j++) {
			if(val & mask) {
				bptr[ui] = val & (~mask);
				return 8*ui + j;
			}
			mask = mask >> 1;
		}
		return 0;
	}
	return 0;
}

word32
dynapro_find_free_block(Disk *dsk)
{
	byte	*bptr;
	word32	block;
	int	i;

	// Find first free block, and zero it out

	block = dynapro_find_free_block_internal(dsk);
	if(block == 0) {
		return 0;
	}
	bptr = &(dsk->raw_data[block * 512]);
	for(i = 0; i < 512; i++) {
		bptr[i] = 0;
	}

	return block;
}

byte *
dynapro_malloc_file(char *path_ptr, dword64 *dsize_ptr, int extra_size)
{
	byte	*bptr;
	dword64	dsize, dpos;
	int	fd;
	int	i;

	*dsize_ptr = 0;
	fd = open(path_ptr, O_RDONLY | O_BINARY, 0x1b6);
	if(fd < 0) {
		return 0;
	}
	dsize = cfg_get_fd_size(fd);

	if((size_t)(dsize + extra_size) != (dsize + extra_size)) {
		return 0;
	}
	bptr = malloc((size_t)(dsize + extra_size));
	if(bptr == 0) {
		return bptr;
	}
	// printf("dynapro_malloc_file %p, size:%08lld\n", bptr, dsize);
	for(i = 0; i < extra_size; i++) {
		bptr[dsize + i] = 0;
	}
	dpos = cfg_read_from_fd(fd, bptr, 0, dsize);
	close(fd);
	if(dpos != dsize) {
		free(bptr);
		return 0;
	}
	*dsize_ptr = dsize;
	return bptr;
}

void
dynapro_join_path_and_file(char *outstr, const char *unix_path, const char *str,
						int path_max)
{
	int	len;

	// Create "unix_path" + "/" + "str" in outstr (which has size path_max)
	cfg_strncpy(outstr, unix_path, path_max);
	len = (int)strlen(outstr);
	if((len > 0) && (outstr[len - 1] != '/')) {
		cfg_strlcat(outstr, "/", path_max);
	}
	cfg_strlcat(outstr, str, path_max);
}


word32
dynapro_fill_fileptr_from_prodos(Disk *dsk, Dynapro_file *fileptr,
				char *buf32_ptr, word32 dir_byte)
{
	byte	*bptr;
	word32	upper_lower;
	int	len, c;
	int	i;

	buf32_ptr[0] = 0;
	if((dir_byte < 0x400) || (dir_byte >= dsk->dimage_size)) {
		return 0;			// Directory is damaged
	}
	if(!fileptr) {
		return 0;
	}
	bptr = &(dsk->raw_data[dir_byte]);
	memset(fileptr, 0, sizeof(Dynapro_file));

	fileptr->dir_byte = dir_byte;
	fileptr->file_type = bptr[0x10];
	fileptr->key_block = dynapro_get_word16(&bptr[0x11]);
	fileptr->blocks_used = dynapro_get_word16(&bptr[0x13]);
	fileptr->eof = dynapro_get_word24(&bptr[0x15]);
	//printf("Filling from entry %07x, eof:%06x\n", dir_byte, fileptr->eof);
	fileptr->creation_time = dynapro_get_word32(&bptr[0x18]);
	fileptr->upper_lower = dynapro_get_word16(&bptr[0x1c]);
	fileptr->aux_type = dynapro_get_word16(&bptr[0x1f]);
	fileptr->lastmod_time = dynapro_get_word32(&bptr[0x21]);
	fileptr->header_pointer = dynapro_get_word16(&bptr[0x25]);
	if(dir_byte == 0x404) {			// Volume header
		fileptr->upper_lower = dynapro_get_word32(&bptr[0x1a]);
		fileptr->creation_time &= 0xffff;
	}

	len = (bptr[0] & 0xf) + 1;
	upper_lower = fileptr->upper_lower;
	if((upper_lower & 0x8000) == 0) {		// Not valid
		upper_lower = 0;
	}
	for(i = 0; i < 16; i++) {
		c = bptr[i];
		if(i > len) {
			c = 0;
		}
		fileptr->prodos_name[i] = c;
		if(i > 0) {
			if(upper_lower & 0x4000) {
				if((c >= 'A') && (c <= 'Z')) {
					c = c - 'A' + 'a';	// Make lower
				}
			}
			upper_lower = upper_lower << 1;
			buf32_ptr[i - 1] = c;
			buf32_ptr[i] = 0;
		}
	}
	if(((bptr[0] & 0xf0) == 0) || ((bptr[0] & 0xf) == 0)) {
		fileptr->prodos_name[0] = 0;
		return 1;		// Invalid entry
	}
	if(fileptr->prodos_name[0] >= 0xe0) {		// Dir/Volume header
		fileptr->key_block = dir_byte >> 9;
		if((dir_byte & 0x1ff) != 4) {
			printf("Header at dir_byte:%07x != 4\n", dir_byte);
			return 0;			// Not in first pos
		}
		if(bptr[-4] || bptr[-3]) {
			printf("prev_link %02x,%02x should be 0\n",
						bptr[-4], bptr[-3]);
			return 0;			// Not first dir block
		}
		if(fileptr->prodos_name[0] >= 0xf0) {
			if(dir_byte != 0x0404) {
				printf("Volume head dir_byte:%07x\n", dir_byte);
				return 0;
			}
		} else if(dir_byte == 0x0404) {
			printf("Directory head dir_byte 0x0404\n");
			return 0;			// 0xe0 in block 2->bad
		}
	} else {
		// Normal entry.  Make sure it's not the first entry in a dir
		if((bptr[-4] == 0) && (bptr[-3] == 0) &&
						((dir_byte & 0x1ff) == 4)) {
			printf("dir_byte:%07x, normal, prev:0\n", dir_byte);
			return 0;		// This is a dir/volume header!
		}
	}
#if 0
	printf("Fill resulted in buf32:%s, upper_lower:%04x\n", buf32_ptr,
						fileptr->upper_lower);
#endif

	return 2;		// OK
}

word32
dynapro_diff_fileptrs(Dynapro_file *oldfileptr, Dynapro_file *newfileptr)
{
	word32	ret, new_storage, old_storage;
	int	i;

	// Return 0 if the directory is damaged
	// Return 1 if the entry is invalid (and not case 3!)
	// Return 3 if the entry was valid and is now deleted
	// Return 4 if no changes are needed
	// Return 5 if oldfileptr needs to be rewritten
	// Return 7 if oldfileptr needs to be erased and replaced with newfile

	old_storage = oldfileptr->prodos_name[0];
	new_storage = newfileptr->prodos_name[0];
	if(new_storage == 0) {				// Erased
		if(old_storage >= 0xe0) {	// Vol/Dir header
			return 0;
		}
		if(oldfileptr->dir_byte == newfileptr->dir_byte) {
			return 3;	// Entry just deleted
		}
		return 1;		// Just an invalid entry
	}
	if(oldfileptr->dir_byte != newfileptr->dir_byte) {
		return 0;
	}
	ret = 4;		// No changes needed

	// Handle file expanding from seedling to tree
	if((new_storage >= 0x10) && (new_storage < 0x40) &&
			(old_storage >= 0x10) && (old_storage < 0x40)) {
		// Copy upper 4 bits from new_storage to old_storage
		old_storage = (old_storage & 0x0f) | (new_storage & 0xf0);
		if(oldfileptr->prodos_name[0] != old_storage) {
			// Storage type changed, rewrite the file
			oldfileptr->prodos_name[0] = old_storage;
			ret |= 5;
		}
	}
	for(i = 0; i < 16; i++) {
		if(oldfileptr->prodos_name[i] != newfileptr->prodos_name[i]) {
			ret |= 7;		// Name changed
		}
		oldfileptr->prodos_name[i] = newfileptr->prodos_name[i];
	}

	if(oldfileptr->file_type != newfileptr->file_type) {
		ret |= 7;		// Filetype changed
		oldfileptr->file_type = newfileptr->file_type;
	}
	if(newfileptr->prodos_name[0] < 0xe0) {
		// Not a directory or volume header
		if(oldfileptr->key_block != newfileptr->key_block) {
			ret |= 5;		// Key block has changed
			oldfileptr->key_block = newfileptr->key_block;
		}
		if(oldfileptr->blocks_used != newfileptr->blocks_used) {
			// ret stays 1, we don't care about this field
			oldfileptr->blocks_used = newfileptr->blocks_used;
		}
		if(oldfileptr->eof != newfileptr->eof) {
			ret |= 5;		// eof has changed
			oldfileptr->eof = newfileptr->eof;
		}
	} else {
		// Directory or volume header
		// Ignore key_block (used internally by dynapro.c, but not in
		//  the ProDOS disk image), blocks_used, eof.
		// We ignore file_count at +0x21,0x22.  But bitmap_ptr matters
		//  and if it moves, we are damaged
		if((oldfileptr->lastmod_time >> 16) !=
					(newfileptr->lastmod_time >> 16)) {
			return 0;	// Bitmap_ptr moved, we are damaged
		}
	}
	if(oldfileptr->upper_lower != newfileptr->upper_lower) {
		ret |= 7;		// lowercase flags have changed
		oldfileptr->upper_lower = newfileptr->upper_lower;
	}
	if(oldfileptr->aux_type != newfileptr->aux_type) {
		ret |= 5;		// aux_type has changed
		oldfileptr->aux_type = newfileptr->aux_type;
	}
	if(oldfileptr->header_pointer != newfileptr->header_pointer) {
		return 0;		// We are damaged
	}
	if(newfileptr->prodos_name[0] >= 0xe0) {
		if(ret > 5) {
			ret = 5;	// No renaming volume or dir headers
		}
	}
	return ret;
}

word32
dynapro_do_one_dir_entry(Disk *dsk, Dynapro_file *fileptr,
		Dynapro_file *localfile_ptr, char *buf32_ptr, word32 dir_byte)
{
	word32	ret, diffs;

	ret = dynapro_fill_fileptr_from_prodos(dsk, localfile_ptr,
						buf32_ptr, dir_byte);
	if((ret == 0) || ((ret == 1) && !fileptr)) {
		return ret;		// Damaged or not valid
	}
	if(!fileptr) {
		return 2;		// must allocate new
	}

	// Now, head_ptr must be non-null
	diffs = dynapro_diff_fileptrs(fileptr, localfile_ptr);
	return diffs;
}

void
dynapro_fix_damaged_entry(Disk *dsk, Dynapro_file *fileptr)
{
	if(fileptr->prodos_name[0] >= 0xe0) {
		// This is a volume/directory header.  Re-parse entire dir
		dynapro_handle_write_dir(dsk, fileptr->parent_ptr, fileptr,
					(fileptr->key_block * 0x200UL) + 4);
	} else if(fileptr->prodos_name[0] >= 0xd0) {
		// This is a directory entry.
		dynapro_handle_write_dir(dsk, fileptr, fileptr->subdir_ptr,
					(fileptr->key_block * 0x200UL) + 4);
	} else {
		dynapro_handle_write_file(dsk, fileptr);
	}
}

void
dynapro_try_fix_damage(Disk *dsk, Dynapro_file *fileptr)
{
	// Walk entire tree (recursing to dynapro_try_fix_damage)
	if(!fileptr) {
		return;
	}
	while(fileptr) {
		if(fileptr->damaged) {
			dyna_printf("try_fix_damage %p %s\n", fileptr,
							fileptr->unix_path);
			dynapro_fix_damaged_entry(dsk, fileptr);
		}
		dynapro_try_fix_damage(dsk, fileptr->subdir_ptr);
		fileptr = fileptr->next_ptr;
	}
}

void
dynapro_try_fix_damaged_disk(Disk *dsk)
{
	Dynapro_info *info_ptr;

	info_ptr = dsk->dynapro_info_ptr;
	if(!info_ptr) {				// This is impossible
		return;
	}
	if(info_ptr->damaged == 0) {
		return;
	}

	dyna_printf("************************************\n");
	dyna_printf("try_fix_damaged_dsk called, damaged:%d\n",
							info_ptr->damaged);
	dyna_printf(" vbl_count:%d, g_iwm_dynapro_last_vbl_count:%d\n",
		g_vbl_count, g_iwm_dynapro_last_vbl_count);

	info_ptr->damaged = 0;
	dynapro_try_fix_damage(dsk, info_ptr->volume_ptr);

	dyna_printf("try_fix_damaged_dsk, damaged:%d\n", info_ptr->damaged);
}


void
dynapro_new_unix_path(Dynapro_file *fileptr, const char *path_str,
						const char *name_str)
{
	if(fileptr->unix_path) {
		free(fileptr->unix_path);
	}
	dynapro_join_path_and_file(&g_dynapro_path_buf[0], path_str, name_str,
							DYNAPRO_PATH_MAX);
	dynatype_fix_unix_name(fileptr, &g_dynapro_path_buf[0],
							DYNAPRO_PATH_MAX);
	fileptr->unix_path = kegs_malloc_str(&g_dynapro_path_buf[0]);
}

Dynapro_file *
dynapro_process_write_dir(Disk *dsk, Dynapro_file *parent_ptr,
				Dynapro_file **head_ptr_ptr, word32 dir_byte)
{
	char	buf32[32];
	Dynapro_file localfile;
	Dynapro_file *fileptr, *prev_ptr, *head_ptr;
	byte	*bptr;
	const char *str;
	word32	tmp_byte, prev, next, ret, last_block, parent_dir_byte;
	int	cnt, error, iret, is_header;

	head_ptr = *head_ptr_ptr;		// head_ptr_ptr must be valid
						//  but head_ptr can be 0
	// We can be called with parent_ptr=0, head_ptr != 0: this is for the
	//  volume header.  Otherwise, parent_ptr should be valid.
	// If head_ptr==0, it means we need to allocate directory header and
	//  all other dir entries
	// head_ptr is a pointer to a directory or volume header.
	// For all entries, see if anything changed.  We need to also
	//  possibly update head_ptr->parent_ptr
	// Return 0 if the directory is damaged.  If directory only contains
	//  damaged files, try to fix them, and always return success

	// First, unmap the directory blocks (this is done even if nothing
	//  changed, we'll map them back at the end).
	if(head_ptr) {
		dynapro_unmap_file(dsk, head_ptr);
	}

	parent_dir_byte = 0;
	if(parent_ptr) {
		str = parent_ptr->unix_path;
		parent_dir_byte = parent_ptr->dir_byte;
		if(head_ptr == 0) {
			// Do mkdir to make sure it exists
#ifdef _WIN32
			iret = _mkdir(str);
#else
			iret = mkdir(str, 0x1ff);
#endif
			error = errno;
			dyna_printf("Did mkdir %s, iret:%d\n", str, iret);
			if(iret < 0) {
				if((error == EEXIST) || (error == EISDIR)) {
					error = 0;	// These are OK errors
				}
				if(error) {
					printf("mkdir(%s) failed, error=%d\n",
							str, error);
				}
			}
		}
	} else {
		str = head_ptr->unix_path;		// volume header
	}
#if 0
	printf("process_write_dir str:%s %p parent:%p\n", str, head_ptr,
								parent_ptr);
#endif

	// The directory blocks have already been unmapped

	// Then, walk the directory, noting if anything changed.  If new
	//  files appear in the directory, add then to the chain.  We may need
	//  to erase existing entries which no longer exist (or their directory
	//  entry was changed to a different file)
	bptr = dsk->raw_data;
	prev_ptr = 0;
	fileptr = head_ptr;
	last_block = 0;
	cnt = 0;
	is_header = 1;
	while(dir_byte) {
		//printf("process_write_dir, dir_byte:%07x, prev_ptr:%p\n",
		//			dir_byte, prev_ptr);
		if((dir_byte & 0x1ff) == 4) {
			// First entry in this block: check prev/next
			tmp_byte = dir_byte & -0x200;		// Block align
			prev = dynapro_get_word16(&bptr[tmp_byte + 0]);
			next = dynapro_get_word16(&bptr[tmp_byte + 2]);
			if((prev != last_block) ||
					(next >= (dsk->raw_dsize >> 9))) {
				// This is a damaged directory
				printf("dir %s is damaged in the link fields\n",
							str);
				return 0;
			}
			last_block = dir_byte >> 9;
		}
		if(cnt++ >= 65536) {
			printf("dir %s has a loop in block pointers\n",
							head_ptr->unix_path);
			return 0;
		}

		ret = dynapro_do_one_dir_entry(dsk, fileptr, &localfile,
							&buf32[0], dir_byte);
#if 0
		printf(" do_one_dir_entry ret:%08x fileptr:%p, &localfile:%p\n",
				ret, fileptr, &localfile);
#endif
		if((ret == 7) && !is_header) {	// Entry dramatically changed
			// Erase this file
			dynapro_mark_damaged(dsk, fileptr);
		}
		if(ret == 0) {
			return 0;
		} else if((ret == 1) || (ret == 3)) {
			if((ret == 3) && fileptr) {
				// This entry was valid and is now deleted.
				//  Erase it right now and fix links
				dyna_printf("fileptr %p deleted\n", fileptr);
				prev_ptr->next_ptr = fileptr->next_ptr;
				dynapro_erase_free_entry(dsk, fileptr);
			}
			if(head_ptr == 0) {
				printf("return, head_ptr==0, deleted file at "
							"%07x\n", dir_byte);
				return 0;		// Directory damaged
			}
			fileptr = prev_ptr;
		}
		if(ret == 2) {
			// prev_ptr->next_ptr is 0, this is a new entry we
			//  need to put on the list
			if(fileptr) {
				halt_printf("file %s was ignored!\n",
							fileptr->unix_path);
				exit(1);
			}
			fileptr = dynapro_alloc_file();
			if(!fileptr) {
				return 0;
			}
			*fileptr = localfile;		// STRUCT copy!
			dyna_printf("Allocated new fileptr:%p\n", fileptr);
			fileptr->parent_ptr = parent_ptr;
			if(head_ptr) {
				fileptr->parent_ptr = head_ptr;
			}
		}
		if((ret == 2) || (ret == 7)) {
			// New entry, or dramatically changed, update path
			if(!head_ptr) {
				if(!parent_ptr) {
					printf("parent_ptr is 0!\n");
					return 0;
				}
				parent_ptr->subdir_ptr = fileptr;
				printf("2/7 set %p %s subdir=%p\n", parent_ptr,
					parent_ptr->unix_path, fileptr);
				fileptr->unix_path = kegs_malloc_str(str);
				head_ptr = fileptr;
				*head_ptr_ptr = head_ptr;
			} else {
				dyna_printf("Forming new path: %s buf32:%s\n",
								str, buf32);
				dynapro_new_unix_path(fileptr, str, buf32);
			}
			// If we are a directory entry (fileptr->subdir_ptr!=0)
			//  then now fileptr->unix_path != subdirptr->unix_path
			// The subdir will be erased in
			//  dynapro_handle_changed_entry()
			if(prev_ptr) {
				prev_ptr->next_ptr = fileptr;
			}
		}
		if((ret == 5) || (ret == 2) || (ret == 7)) {
			// Changed, or new entry
			if(is_header) {
				ret = dynapro_validate_header(dsk, fileptr,
						dir_byte, parent_dir_byte);
				if(ret == 0) {
					return 0;
				}
			} else {
				dynapro_handle_changed_entry(dsk, fileptr);
			}
		}
		prev_ptr = fileptr;
		fileptr = prev_ptr->next_ptr;
		dir_byte = dir_byte + 0x27;
		tmp_byte = (dir_byte & 0x1ff) + 0x27;
		is_header = 0;
		if(tmp_byte < 0x200) {
			continue;
		}
		tmp_byte = (dir_byte - 0x27) & (0 - 0x200UL);
		dir_byte = dynapro_get_word16(&bptr[tmp_byte + 2]) * 0x200UL;
		dyna_printf(" dir link at %07x = %04x\n", tmp_byte + 2,
								dir_byte);
		if(dir_byte == 0) {
			if(fileptr) {
				printf("At dir end, fileptr: %p\n", fileptr);
				prev_ptr->next_ptr = 0;
				dynapro_erase_free_dir(dsk, fileptr);
				return 0;
			}
			ret = dynapro_map_dir_blocks(dsk, head_ptr);
			// printf("process_write_dir %s done, remap ret:%d\n",
			//			head_ptr->unix_path, ret);
			if(ret == 0) {
				return 0;
			}
			return head_ptr;	// Success
		}
		dir_byte += 4;
		if(dir_byte >= dsk->dimage_size) {
			// printf(" invalid link pointer, dir_byte:%08x\n",
			//			dir_byte);
			return 0;		// Bad link, get out
		}
	}
	dyna_printf("At end of process_write_dir, returning 0\n");
	return 0;
}

void
dynapro_handle_write_dir(Disk *dsk, Dynapro_file *parent_ptr,
				Dynapro_file *head_ptr, word32 dir_byte)
{
	Dynapro_file *fileptr;

	//save_headptr = head_ptr;
	dyna_printf("handle_write_dir parent_ptr:%p, head_ptr:%p, dir_b:%07x\n",
			parent_ptr, head_ptr, dir_byte);
	if(parent_ptr && head_ptr) {
		dyna_printf(" parent:%s head:%s\n", parent_ptr->unix_path,
							head_ptr->unix_path);
		if(parent_ptr->subdir_ptr != head_ptr) {
			printf("parent subdir:%p does not match %p\n",
				parent_ptr->subdir_ptr, head_ptr);
			exit(1);
		}
	}
	if(parent_ptr == 0) {
		if(head_ptr != dsk->dynapro_info_ptr->volume_ptr) {
			printf("handle_write_dir %p %p %07x\n", parent_ptr,
						head_ptr, dir_byte);
			exit(1);
		}
	}
	fileptr = dynapro_process_write_dir(dsk, parent_ptr, &head_ptr,
								dir_byte);
	if(fileptr == 0) {
		// Directory is damaged.  Free and erase it
		dynapro_erase_free_dir(dsk, head_ptr);
		head_ptr = 0;
	}
	//printf("handle_write_dir, process returned %p (was %p), parent:%p, "
	//	"save:%p\n", fileptr, head_ptr, parent_ptr, save_headptr);
	if(parent_ptr) {
		parent_ptr->subdir_ptr = head_ptr;
		if(!fileptr) {
			parent_ptr->damaged = 1;
			dsk->dynapro_info_ptr->damaged = 1;
		}
		dyna_printf("hwd set parent %p %s subdir=%p\n", parent_ptr,
			parent_ptr->unix_path, head_ptr);
	}
}

word32
dynapro_process_write_file(Disk *dsk, Dynapro_file *fileptr)
{
	word32	ret;

	//printf("dynapro_process_write_file %p %s\n", fileptr,
	//						fileptr->unix_path);

	if(fileptr->subdir_ptr) {
		printf("dynapro_handle_write_file, has subdir: %s\n",
			fileptr->unix_path);
		halt_printf("dynapro_handle_write_file, has subdir: %s\n",
			fileptr->unix_path);
		return 0;
	}

	// First, unmap the file (the sapling/tree blocks may have changed).
	dynapro_unmap_file(dsk, fileptr);

	// Then remap the blocks. This will copy the new data to buffer_ptr

	dynapro_debug_map(dsk, "handle_write_file, right before re-map");

	ret = dynapro_map_file(dsk, fileptr, 1);

	// printf("dynapro_handle_write_file ending, ret:%d\n", ret);

	return ret;
}

void
dynapro_handle_write_file(Disk *dsk, Dynapro_file *fileptr)
{
	word32	ret;

	ret = dynapro_process_write_file(dsk, fileptr);
	if(ret == 0) {
		dynapro_mark_damaged(dsk, fileptr);
	}
}

void
dynapro_handle_changed_entry(Disk *dsk, Dynapro_file *fileptr)
{
	// printf("handle_changed_entry with fileptr:%p\n", fileptr);
	fileptr->damaged = 0;
	if(fileptr->prodos_name[0] >= 0xe0) {
		// Directory header, not valid to be called here
		fileptr->damaged = 1;
		dsk->dynapro_info_ptr->damaged = 1;
	} else if(fileptr->prodos_name[0] >= 0xd0) {
		// Directory entry
		if(fileptr->subdir_ptr) {
			dynapro_erase_free_dir(dsk, fileptr->subdir_ptr);
			fileptr->subdir_ptr = 0;
		}
		dynapro_handle_write_dir(dsk, fileptr, 0,
					(fileptr->key_block * 0x200UL) + 4);
	} else {
		dynapro_handle_write_file(dsk, fileptr);
#if 0
		printf("handle_changed_entry called handle_write_file for %p, "
					"%s\n", fileptr, fileptr->unix_path);
#endif
	}
}

word32
dynapro_write_to_unix_file(const char *unix_path, byte *data_ptr, word32 size)
{
	dword64	dret;
	int	fd;

	fd = open(unix_path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0x1b6);
	if(fd < 0) {
		printf("Open %s for writing failed\n", unix_path);
		exit(1);
		return 0;
	}
	dret = cfg_write_to_fd(fd, data_ptr, 0, size);
	close(fd);
	dyna_printf("dynapro_write_to_unix: %s size:%d, dret:%lld\n",
						unix_path, size, dret);


	if(size == 0) {
		return 1;
	}
	return (word32)dret;
}

void
dynapro_unmap_file(Disk *dsk, Dynapro_file *fileptr)
{
	Dynapro_file *this_fileptr;
	Dynapro_map *map_ptr;
	//const char *str;
	word32	map_block, next_map_block, max_blocks;
	int	i;

	//printf("File %p: %s is unmapped\n", fileptr, fileptr->unix_path);
	dynapro_debug_map(dsk, "start unmap file");

	// Unmap all blocks to this file/dir
	map_ptr = dsk->dynapro_info_ptr->block_map_ptr;
	max_blocks = (word32)(dsk->dimage_size >> 9);
	map_block = fileptr->map_first_block;
	//printf(" map_block:%04x, fileptr:%p %s\n", map_block, fileptr,
	//					fileptr->unix_path);
	fileptr->map_first_block = 0;
	//printf("  unmap starting, map_block:%08x, max_blocks:%07x\n",
	//			map_block, max_blocks);
	for(i = 0; i < 65536; i++) {
		if((map_block == 0) || (map_block >= max_blocks)) {
			break;
		}
		next_map_block = map_ptr[map_block].next_map_block;
		this_fileptr = map_ptr[map_block].file_ptr;
		if(this_fileptr != fileptr) {
			//str = "??";
			if(this_fileptr) {
				//str = this_fileptr->unix_path;
				this_fileptr->damaged = 1;
			}
#if 0
			printf("Found map[%04x]=%s while walking %s\n",
				map_block, str, fileptr->unix_path);
#endif
		}
		map_ptr[map_block].file_ptr = 0;
		map_ptr[map_block].next_map_block = 0;
		map_ptr[map_block].modified = 0;
		//printf("  just unmapped block %05x\n", map_block);
		map_block = next_map_block;
	}

	// printf(" unmap ending\n");
}

void
dynapro_unlink_file(Dynapro_file *fileptr)
{
	const char *str;
	int	ret, err;

	// Try to unlink unix_path
	dyna_printf("Unlink %s (%p)\n", fileptr->unix_path, fileptr);
	if(fileptr->unix_path == 0) {
		printf("unix_path of %p is null!\n", fileptr);
		exit(1);
	}
	if(fileptr->subdir_ptr != 0) {
		printf("unlink_file %s, but subdirptr is valid!\n",
				fileptr->unix_path);
		exit(1);
	}

	ret = unlink(fileptr->unix_path);
	if(ret != 0) {
		// Maybe it's a directory, rmdir
		ret = rmdir(fileptr->unix_path);
	}
	if(ret != 0) {
		// Cannot erase, try to rename
		cfg_strncpy_dirname(&g_dynapro_path_buf[0], fileptr->unix_path,
							DYNAPRO_PATH_MAX);
		cfg_strlcat(&g_dynapro_path_buf[0], ".kegsrm_",
							DYNAPRO_PATH_MAX);
		str = cfg_str_basename(fileptr->unix_path);
		cfg_strlcat(&g_dynapro_path_buf[0], str, DYNAPRO_PATH_MAX);
		printf("Could not erase %s, renaming to: %s\n",
			fileptr->unix_path, &g_dynapro_path_buf[0]);
		ret = rename(fileptr->unix_path, &g_dynapro_path_buf[0]);
		err = errno;
		if(ret != 0) {
			printf("Rename of %s failed, err:%d\n",
						fileptr->unix_path, err);
		}
	}
}

void
dynapro_erase_free_entry(Disk *dsk, Dynapro_file *fileptr)
{
	if(!fileptr) {
		return;
	}
	dynapro_mark_damaged(dsk, fileptr);

	fileptr->next_ptr = 0;
	if(fileptr != dsk->dynapro_info_ptr->volume_ptr) {
		// Free everything--except the volume header
		dyna_printf("erase_free_entry erasing %p since it != %p\n",
			fileptr, dsk->dynapro_info_ptr->volume_ptr);
		dynapro_free_file(fileptr, 1);
	}
}

void
dynapro_erase_free_dir(Disk *dsk, Dynapro_file *fileptr)
{
	Dynapro_file *nextptr, *parent_ptr, *save_fileptr;

	dyna_printf("dynapro_erase_free_dir of %p\n", fileptr);
	if(fileptr == 0) {
		return;
	}
	dyna_printf("  dynapro_erase_free_dir of %s\n", fileptr->unix_path);
	dsk->dynapro_info_ptr->damaged = 1;
	parent_ptr = fileptr->parent_ptr;
	if(parent_ptr) {
		if(parent_ptr->subdir_ptr) {
			parent_ptr->damaged = 1;
		}
	}
	save_fileptr = fileptr;
	nextptr = fileptr->next_ptr;
	fileptr->next_ptr = 0;
	fileptr = nextptr;
	while(fileptr) {
		nextptr = fileptr->next_ptr;
		dynapro_erase_free_entry(dsk, fileptr);
		fileptr = nextptr;
	}
	dynapro_erase_free_entry(dsk, save_fileptr);
}

void
dynapro_mark_damaged(Disk *dsk, Dynapro_file *fileptr)
{
	if(fileptr == 0) {
		return;
	}
	dyna_printf("dynapro_mark_damaged: %s damaged\n", fileptr->unix_path);
	fileptr->damaged = 1;
	dsk->dynapro_info_ptr->damaged = 1;
	dynapro_unmap_file(dsk, fileptr);
	if(fileptr->subdir_ptr) {
		dynapro_erase_free_dir(dsk, fileptr->subdir_ptr);
		fileptr->subdir_ptr = 0;
	}

	if((fileptr->prodos_name[0] >= 0xe0) && fileptr->parent_ptr) {
		// We are a directory header, mark the directory entry of our
		//  parent as damaged (but don't actually damage it)
		fileptr->parent_ptr->damaged = 1;
	} else if(fileptr != dsk->dynapro_info_ptr->volume_ptr) {
		dynapro_unlink_file(fileptr);
	}
}

int
dynapro_write(Disk *dsk, byte *bufptr, dword64 doffset, word32 size)
{
	Dynapro_info *info_ptr;
	Dynapro_map *map_ptr;
	Dynapro_file *fileptr;
	byte	*bptr;
	word32	ui, block;
	int	num;
	int	i;

	// Return 1 if write was done.  Return < 0 if an error occurs

	dyna_printf("\n");
	dyna_printf("------------------------------------------------\n");
	dyna_printf("dynapro_write to %08llx, size:%08x\n", doffset, size);
	dynapro_debug_update(dsk);

	bptr = dsk->raw_data;
	if((doffset + size) > dsk->dimage_size) {
		printf("Write past end of disk, ignored\n");
		return -1;
	}
	for(ui = 0; ui < size; ui++) {
#if 0
		if((bptr[doffset + ui] != bufptr[ui]) && (diffs < 500)) {
			printf("%07llx:%02x (was %02x)\n", doffset+ui,
				bufptr[ui], bptr[doffset + ui]);
			diffs++;
		}
#endif
		bptr[doffset + ui] = bufptr[ui];
	}

	info_ptr = dsk->dynapro_info_ptr;
	if(info_ptr == 0) {
		printf("dynapro_info_ptr==0\n");
		return -1;
	}

	num = (size + 511) >> 9;
	block = (word32)(doffset >> 9);
	dyna_printf("Marking blocks %05x-%05x modified\n", block,
							block + num - 1);
	for(i = 0; i < num; i++) {
		map_ptr = &(info_ptr->block_map_ptr[block + i]);
		map_ptr->modified = 1;
	}
	for(i = 0; i < num; i++) {
		map_ptr = &(info_ptr->block_map_ptr[block + i]);
		if(!map_ptr->modified) {
			continue;			// Already cleared
		}
		fileptr = map_ptr->file_ptr;
		if(fileptr == 0) {
			continue;
		}
		if(fileptr->prodos_name[0] >= 0xe0) {
			dynapro_handle_write_dir(dsk, fileptr->parent_ptr,
				fileptr, fileptr->dir_byte);
		} else {
			dynapro_handle_write_file(dsk, fileptr);
		}
	}

	return 1;
}


void
dynapro_debug_update(Disk *dsk)
{
	dyna_printf("Writing out DYNAPRO_IMAGE, %p\n", dsk);
#if 0
	// This causes the file DYNAPRO_IMAGE to be written out as the raw
	//  image after any write to the Dynapro volume.  This is for manual
	//  debugging.
	dynapro_write_to_unix_file("DYNAPRO_IMAGE", dsk->raw_data,
						(word32)dsk->dimage_size);
#endif
}

void
dynapro_debug_map(Disk *dsk, const char *str)
{
	Dynapro_map *map_ptr;
	Dynapro_file *lastfileptr, *fileptr;
	const char *newstr;
	int	num_blocks;
	int	i;

	return;			// HACK!

	num_blocks = (word32)((dsk->dimage_size + 511) >> 9);
	map_ptr = dsk->dynapro_info_ptr->block_map_ptr;
	lastfileptr = 0;
	printf(" Showing map for %s, %05x blocks, %s\n",
			dsk->dynapro_info_ptr->root_path, num_blocks, str);
	for(i = 0; i < num_blocks; i++) {
		fileptr = map_ptr[i].file_ptr;
		if(fileptr != lastfileptr) {
			newstr = "";
			if(fileptr) {
				newstr = fileptr->unix_path;
			}
			printf("  %04x (%07x): %p %s\n", i, i << 9, fileptr,
									newstr);
		}
		lastfileptr = fileptr;
	}
	printf("Recursive file map:\n");
	dynapro_debug_recursive_file_map(dsk->dynapro_info_ptr->volume_ptr, 1);
}

void
dynapro_debug_recursive_file_map(Dynapro_file *fileptr, int start)
{
	if(!fileptr) {
		return;
	}
	while(fileptr) {
		printf("  file %p %s map_first_block:%05x, storage:%02x key:"
			"%04x\n", fileptr, fileptr->unix_path,
			fileptr->map_first_block, fileptr->prodos_name[0],
			fileptr->key_block);
		printf("      n:%p, sub:%p, eof:%06x, parent:%p dam:%d\n",
			fileptr->next_ptr, fileptr->subdir_ptr,
			fileptr->eof, fileptr->parent_ptr, fileptr->damaged);
		if(fileptr->unix_path == 0) {
			printf("Filename is invalid, exiting\n");
			exit(1);
		}
		if(!fileptr->parent_ptr && !start) {
			printf("parent_ptr is 0, exiting\n");
			exit(1);
		}
		dynapro_debug_recursive_file_map(fileptr->subdir_ptr, 0);
		start = 0;
		fileptr = fileptr->next_ptr;
	}
}

word32
dynapro_unix_to_prodos_time(const time_t *time_ptr)
{
	struct tm *tm_ptr;
	word32	ymd, hours_mins, date_time;
	int	year;

	tm_ptr = localtime(time_ptr);
	hours_mins = (tm_ptr->tm_hour << 8) | tm_ptr->tm_min;
	year = tm_ptr->tm_year;		// years since 1900
	if(year < 80) {
		year = 80;
	} else if(year >= 100) {
		year -= 100;
		if(year >= 80) {
			year = 79;
		}
	}
	ymd = (year << 9) | ((tm_ptr->tm_mon + 1) << 5) | (tm_ptr->tm_mday);
	date_time = (ymd & 0xffff) | (hours_mins << 16);
	// printf("Unix time:%s results in:%08x\n", asctime(tm_ptr), date_time);

	return date_time;
}

int
dynapro_create_prodos_name(Dynapro_file *newfileptr, Dynapro_file *matchptr,
		word32 storage_type)
{
	Dynapro_file *thisptr;
	char	*str;
	word32	upper_lower;
	int	len, outpos, inpos, max_inpos, c, done, dot_pos, inc_pos;
	int	i;

#if 0
	printf("dynapro_create_prodos_name to %s, match:%p, st:%03x\n",
		newfileptr->unix_path, matchptr, storage_type);
#endif

	for(i = 0; i < 17; i++) {
		newfileptr->prodos_name[i] = 0;
	}
	str = newfileptr->unix_path;
	if(!str) {
		return 0;
	}
	inpos = (int)strlen(str);
	max_inpos = inpos + 1;
	while(inpos >= 1) {
		inpos--;
		if(str[inpos] == '/') {
			inpos++;
			break;
		}
	}
	if(storage_type == 0x50) {
		// printf("max_inpos:%d, inpos:%d\n", max_inpos, inpos);
	}
	if((storage_type == 0x50) && ((max_inpos - inpos) > 12)) {
		// Remove .applesingle extension
		max_inpos -= 13;
	}
	//printf(" inpos:%d max_inpos:%d str:%s\n", inpos, max_inpos,
	//						&(str[inpos]));
	outpos = 0;
	while(outpos < (DYNAPRO_PATH_MAX - 1)) {
		c = 0;
		if(inpos < max_inpos) {
			c = str[inpos++];
		}
		g_dynapro_path_buf[outpos] = c;
		if(c == 0) {
			break;
		}
		outpos++;
		if((c >= 'A') && (c <= 'Z')) {
			continue;		// This is legal
		}
		if((c >= 'a') && (c <= 'z')) {
			continue;		// Also legal
		}
		if((outpos > 1) && (c >= '0') && (c <= '9')) {
			continue;		// Also legal
		}
		if((outpos > 1) && (c == '.')) {
			continue;		// Also legal
		}
		// If this is the first character, make it "A" and continue
		if(outpos == 1) {
			g_dynapro_path_buf[0] = 'A';
			continue;
		}
		if((c == ',') || (c == '#')) {		// ,ttxt,a$2000, ignore
			outpos--;
			break;			// All done
		}

		// This is not legal.  Make it a '.'
		if((c >= 0x20) && (c <= 0x7e)) {
			g_dynapro_path_buf[outpos - 1] = '@';	// do '.' later
		} else {
			outpos--;		// Ignore it
		}
	}

	g_dynapro_path_buf[outpos] = 0;
	// printf(" initial path_buf:%s, %d\n", &g_dynapro_path_buf[0], outpos);
	while((outpos >= 0) && (g_dynapro_path_buf[outpos-1] == '@')) {
		// Remove trailing '@' since they are not useful
		outpos--;
		g_dynapro_path_buf[outpos] = 0;
	}
	for(i = 1; i < outpos; i++) {
		// Convert '@' to '.' to make name legal
		if(g_dynapro_path_buf[i] == '@') {
			g_dynapro_path_buf[i] = '.';
		}
	}
	if(outpos == 0) {
		// Not a valid file, just skip it
		return 0;
	}
	// Now, it's valid.  Squeeze it to 15 character but saving extension
	len = (int)strlen(&g_dynapro_path_buf[0]);
	if(len > 15) {
		// Copy last 8 characters to be in positions 7..14
		for(i = 7; i < 16; i++) {
			g_dynapro_path_buf[i] = g_dynapro_path_buf[len-15 + i];
		}
	}

	len = (int)strlen(&g_dynapro_path_buf[0]);
	if((len > 15) || (len == 0)) {
		printf("Bad filename handling: %s\n", &g_dynapro_path_buf[0]);
		return 0;
	}

	// See if it conflicts with matchptr
	thisptr = matchptr;
	for(i = 0; i < 10000; i++) {
		if(!thisptr || (thisptr == newfileptr)) {
			thisptr = 0;
			break;
		}
		//printf("Comparing %s to %s\n", &g_dynapro_path_buf[0],
		//			(char *)&(thisptr->prodos_name[1]));
		len = (int)strlen(&g_dynapro_path_buf[0]);
		if((len == (thisptr->prodos_name[0] & 0xf)) &&
				(cfgcasecmp(&g_dynapro_path_buf[0],
				(char *)&(thisptr->prodos_name[1])) == 0)) {
			dyna_printf(" that was a match\n");
			dot_pos = 0;
			inc_pos = len - 1;
			for(i = len - 2; i >= 1; i--) {
				if(g_dynapro_path_buf[i] == '.') {
					dot_pos = i;
				}
			}
			if(len < 15) {
				// Append "1" to the end
				len++;
				inc_pos = len - 1;
				g_dynapro_path_buf[len] = 0;
				g_dynapro_path_buf[len - 1] = '1';
				if(dot_pos > 1) {
					for(i = len - 1; i >= dot_pos; i--) {
						g_dynapro_path_buf[i] =
							g_dynapro_path_buf[i-1];
					}
					inc_pos = dot_pos;
				}
				g_dynapro_path_buf[inc_pos] = '1';
			} else if(dot_pos > 3) {
				inc_pos = dot_pos - 1;
			}
			done = 0;
			for(i = inc_pos; i >= 1; i--) {
				c = g_dynapro_path_buf[i];
				c++;
				if(c == ('9' + 1)) {
					c = '0';
				} else if(c == ('z' + 1)) {
					c = 'a';
				} else if(c == ('Z' + 1)) {
					c = 'A';
				} else {
					done = 1;
				}
				g_dynapro_path_buf[i] = c;
				if(done) {
					break;
				}
			}
			thisptr = matchptr;
		} else {
			thisptr = thisptr->next_ptr;
		}
	}
	if(thisptr) {
		// File could not be made unique
		printf("Could not make a unique ProDOS filename: %s\n",
						newfileptr->unix_path);
		return 0;
	}

	upper_lower = 0;
	for(i = 0; i < len; i++) {
		c = g_dynapro_path_buf[i];
		if((c >= 'a') && (c <= 'z')) {
			c = c - 'a' + 'A';
			upper_lower |= 0x8000 | (0x4000 >> i);
		}
		newfileptr->prodos_name[1 + i] = c;
	}
	newfileptr->prodos_name[0] = len | storage_type;
	newfileptr->upper_lower = upper_lower;

	return len;
}

Dynapro_file *
dynapro_new_unix_file(const char *path, Dynapro_file *parent_ptr,
				Dynapro_file *match_ptr, word32 storage_type)
{
	Dynapro_file *fileptr;
	int	len;
	int	i;

#if 0
	printf("dynapro_new_unix_file for %s, parent:%p, m:%p, st:%03x\n",
		path, parent_ptr, match_ptr, storage_type);
#endif
	fileptr = dynapro_alloc_file();
	if(!fileptr) {
		return 0;
	}

	fileptr->next_ptr = 0;
	fileptr->parent_ptr = parent_ptr;
	fileptr->subdir_ptr = 0;
	fileptr->buffer_ptr = 0;
	fileptr->unix_path = kegs_malloc_str(path);
	for(i = 0; i < 17; i++) {
		fileptr->prodos_name[i] = 0;
	}
	fileptr->dir_byte = 0;
	fileptr->eof = 0;
	fileptr->blocks_used = 0;
	fileptr->creation_time = 0;
	fileptr->lastmod_time = 0;
	fileptr->upper_lower = 0;
	fileptr->key_block = 0;
	fileptr->aux_type = 0;
	fileptr->header_pointer = 0;
	fileptr->map_first_block = 0;
	fileptr->file_type = 0x0f;			// Default to "DIR"
	fileptr->modified_flag = 0;
	fileptr->damaged = 0;

	len = (int)strlen(fileptr->unix_path);
	for(i = len - 1; i >= 0; i--) {
		if(fileptr->unix_path[i] == '/') {
			fileptr->unix_path[i] = 0;	// Strip trailing /
		} else {
			break;
		}
	}

	if(storage_type < 0xd0) {
		storage_type = dynatype_detect_file_type(fileptr,
					fileptr->unix_path, storage_type);
	}

	len = dynapro_create_prodos_name(fileptr, match_ptr, storage_type);
	if(len == 0) {
		printf("Could not create prodos name for: %s\n", path);
		free(fileptr);
		return 0;
	}

#if 0
	printf("dynapro_create_new_unix_file: %s prodos:%s, st:%02x, ft:%02x, "
		"aux:%04x\n", fileptr->unix_path, &(fileptr->prodos_name[1]),
		fileptr->prodos_name[0], fileptr->file_type, fileptr->aux_type);
#endif
	return fileptr;
}

int
dynapro_create_dir(Disk *dsk, char *unix_path, Dynapro_file *parent_ptr,
				word32 dir_byte)
{
	struct stat stat_buf;
	struct dirent *direntptr;
	DIR	*opendirptr;
	Dynapro_file *fileptr, *head_ptr, *prev_ptr;
	mode_t	fmt;
	word32	storage_type, val;
	int	ret;

	// Create a directory entry at dir_byte first
#if 0
	printf("\n");
	printf("dynapro_add_files to %s, %p dir_byte:%08x\n", unix_path,
							parent_ptr, dir_byte);
#endif

	storage_type = 0xe0;		// Directory header
	if(dir_byte < 0x600) {		// Block 2: volume header
		storage_type = 0xf0;
	}
	head_ptr = dynapro_new_unix_file(unix_path, parent_ptr, 0,
								storage_type);
	if(parent_ptr) {
		parent_ptr->subdir_ptr = head_ptr;
#if 0
		printf("set parent %s subdir_ptr=%p\n", parent_ptr->unix_path,
						head_ptr);
#endif
	}
	if(dsk->dynapro_info_ptr->volume_ptr == 0) {
		dsk->dynapro_info_ptr->volume_ptr = head_ptr;
	}
	if(head_ptr == 0) {
		printf("new_file returned 0, skipping %s\n", unix_path);
		return dir_byte;
	}
	head_ptr->key_block = dir_byte >> 9;
	head_ptr->aux_type = 0x0d27;			// 0x27,0x0d
	head_ptr->file_type = 0x75;			// Directory header type
	if(storage_type >= 0xf0) {
		head_ptr->file_type = 0x00;
	}
	ret = cfg_stat(unix_path, &stat_buf, 0);
	if(ret != 0) {
		printf("stat %s ret %d, errno:%d\n", unix_path, ret, errno);
		return 0;
	}
	head_ptr->creation_time = dynapro_unix_to_prodos_time(
							&stat_buf.st_ctime);

	dir_byte = dynapro_add_file_entry(dsk, head_ptr, 0, dir_byte, 0);

	opendirptr = opendir(unix_path);
	if(opendirptr == 0) {
		printf("Could not open %s as a dir\n", unix_path);
		return 0;
	}
	prev_ptr = head_ptr;
	while(1) {
		direntptr = readdir(opendirptr);
		if(direntptr == 0) {
			break;
		}
		if(direntptr->d_name[0] == '.') {
			continue;			// Ignore all '.' files
		}
		dynapro_join_path_and_file(&(g_dynapro_path_buf[0]), unix_path,
					direntptr->d_name, DYNAPRO_PATH_MAX);
		ret = cfg_stat(&(g_dynapro_path_buf[0]), &stat_buf, 0);
		if(ret != 0) {
			printf("stat %s ret %d, errno:%d\n",
					&g_dynapro_path_buf[0], ret, errno);
			continue;	// skip it
		}

		fmt = stat_buf.st_mode & S_IFMT;
		storage_type = 0;
		if(fmt == S_IFDIR) {
			// Ignore symlinks to directories (since they may point
			//  outside the base directory, and so dynamically
			//  removing files could be a security issue).
			ret = cfg_stat(&g_dynapro_path_buf[0], &stat_buf, 1);
			if(ret != 0) {
				printf("lstat %s ret %d, errno:%d\n",
					&g_dynapro_path_buf[0], ret, errno);
				continue;
			}
			storage_type = 0xd0;		// Directory
		} else if(fmt != S_IFREG) {
			continue;		// Skip this
		}
#if 0
		printf("GOT file: %s, is_dir:%d (%s), parent:%p\n",
			&(g_dynapro_path_buf[0]), is_dir, direntptr->d_name,
			parent_ptr);
#endif
		fileptr = dynapro_new_unix_file(&(g_dynapro_path_buf[0]),
			head_ptr, head_ptr->next_ptr, storage_type);
		if(fileptr == 0) {
			closedir(opendirptr);
			return 0;
		}
		prev_ptr->next_ptr = fileptr;
		fileptr->key_block = dynapro_find_free_block(dsk);
		fileptr->creation_time = dynapro_unix_to_prodos_time(
							&stat_buf.st_ctime);
		fileptr->lastmod_time = dynapro_unix_to_prodos_time(
							&stat_buf.st_mtime);
		if(fileptr->key_block == 0) {
			printf("Allocating directory block failed\n");
			closedir(opendirptr);
			return 0;
		}
		fileptr->blocks_used = 1;
		fileptr->eof = 1*0x200;
		dir_byte = dynapro_add_file_entry(dsk, fileptr, head_ptr,
							dir_byte, 0x27);
		if(dir_byte == 0) {
			closedir(opendirptr);
			return 0;
		}
		if(fmt == S_IFDIR) {
			val = dynapro_create_dir(dsk, fileptr->unix_path,
				fileptr, (fileptr->key_block << 9) + 4);
			if(val == 0) {
				closedir(opendirptr);
				return 0;
			}
		} else {
			val = dynapro_file_from_unix(dsk, fileptr);
			if(val == 0) {
				closedir(opendirptr);
				return 0;
			}
		}
		prev_ptr = fileptr;
	}

	closedir(opendirptr);
	return dir_byte;
}

word32
dynapro_add_file_entry(Disk *dsk, Dynapro_file *fileptr, Dynapro_file *head_ptr,
	word32 dir_byte, word32 inc)
{
	Dynapro_file *parent_ptr;
	byte	*bptr, *pkeyptr;
	word32	storage_type, val, ent, new_dir_blk, new_dir_byte;
	word32	header_pointer;
	int	i;

#if 0
	printf("dynapro_add_file_entry: %p %p %s head:%p dir_byte:%08x "
		"inc:%03x\n", dsk, fileptr, fileptr->unix_path, head_ptr,
		dir_byte, inc);
#endif
	bptr = dsk->raw_data;
	if(((dir_byte & 0x1ff) + inc + inc) >= 0x200) {
		// This entry will not fit in this directory block.
		//  Try to step to next block, otherwise allocate a new one
		new_dir_byte = dir_byte & -0x200L;
		new_dir_blk = dynapro_get_word16(&bptr[new_dir_byte + 2]);
		dyna_printf(" Entry does not fit, new_dir_blk:%04x\n",
							new_dir_blk);
		if(new_dir_blk != 0) {
			// Follow to the next block
			dir_byte = (new_dir_blk * 0x200) + 4;
		} else if(dir_byte < (6 * 0x200)) {
			// Otherwise, allocate a new block (not for volume dir)
			// This is a volume header, always 4 blocks, don't
			//  allocate any more, this is now full
			printf("Too many file in volume directory\n");
			return 0;		// Out of space
		} else {
			new_dir_blk = dynapro_find_free_block(dsk);
			if(new_dir_blk == 0) {
				return 0;
			}
			new_dir_byte = new_dir_blk * 512;
			dynapro_set_word16(&bptr[new_dir_byte], dir_byte >> 9);
			dynapro_set_word16(&bptr[new_dir_byte + 2], 0);
			dir_byte = (dir_byte >> 9) << 9;
			dynapro_set_word16(&bptr[dir_byte + 2], new_dir_blk);
			dir_byte = new_dir_byte + 4;
			if(!head_ptr) {
				dyna_printf("No head:%s\n", fileptr->unix_path);
			}
			parent_ptr = head_ptr->parent_ptr;
			if(!parent_ptr) {
				printf("No parent: %s\n", fileptr->unix_path);
				return 0;
			}
			parent_ptr->blocks_used++;
			parent_ptr->eof += 0x200;
			new_dir_byte = parent_ptr->dir_byte;
			if(new_dir_byte == 0) {
				printf("Invalid dir_byte for %s\n",
						parent_ptr->unix_path);
				return 0;
			}
			dynapro_set_word16(&bptr[new_dir_byte + 0x13],
						parent_ptr->blocks_used);
			dynapro_set_word24(&bptr[new_dir_byte + 0x15],
						parent_ptr->eof);
		}
	} else {
		dir_byte += inc;
	}
	bptr = &(dsk->raw_data[dir_byte]);

	fileptr->dir_byte = dir_byte;
	for(i = 0; i < 0x27; i++) {
		bptr[i] = 0;
	}
	for(i = 0; i < 16; i++) {
		bptr[i] = fileptr->prodos_name[i];	// [0] = len,storage_t
	}
	bptr[0x10] = fileptr->file_type;
	dynapro_set_word16(&bptr[0x11], fileptr->key_block);
	dynapro_set_word16(&bptr[0x13], fileptr->blocks_used);
	dynapro_set_word24(&bptr[0x15], fileptr->eof);
	dynapro_set_word32(&bptr[0x18], fileptr->creation_time);
							// creation date&time
	bptr[0x1c] = fileptr->upper_lower & 0xff;	// Version
	bptr[0x1d] = fileptr->upper_lower >> 8;		// Min_Version
	bptr[0x1e] = 0xe3;				// Access
	dynapro_set_word16(&bptr[0x1f], fileptr->aux_type);
	storage_type = bptr[0];
	if(storage_type >= 0xf0) {		// Volume header
		dynapro_set_word16(&bptr[0x11], 0);
		fileptr->lastmod_time = 0x00060000;
			// low 16 bits: file_count, upper 16 bits: bitmap_block
		fileptr->header_pointer = (word32)(dsk->raw_dsize >> 9);
							// Total blocks
		dynapro_set_word16(&bptr[0x1c], 0x0005);
		dynapro_set_word16(&bptr[0x16], fileptr->upper_lower);
	} else if(storage_type >= 0xe0) {		// Directory header
		dynapro_set_word16(&bptr[0x11], 0);
		dynapro_set_word16(&bptr[0x1c], 0x0005);
		parent_ptr = fileptr->parent_ptr;		// subdir entry
		if(parent_ptr == 0) {
			printf("parent_ptr of %s is 0\n", fileptr->unix_path);
			return 0;
		}
		val = parent_ptr->dir_byte >> 9;
		fileptr->lastmod_time = (val << 16);		// Parent block
		val = parent_ptr->dir_byte & 0x1ff;
		ent = (val - 4) / 0x27;
		fileptr->header_pointer = 0x2700 | (ent + 1);
	} else {
		// Directory entry, or normal file
		if(head_ptr == 0) {
			printf("head_ptr of %s is 0\n", fileptr->unix_path);
			return 0;
		}
		header_pointer = head_ptr->key_block;
		fileptr->header_pointer = header_pointer;
		dynapro_set_word16(&bptr[0x25], header_pointer);
		pkeyptr = &(dsk->raw_data[header_pointer << 9]);
		val = head_ptr->lastmod_time + 1;
		head_ptr->lastmod_time = val;
		dynapro_set_word16(&pkeyptr[4 + 0x21], val);	// File count
	}
	dynapro_set_word32(&bptr[0x21], fileptr->lastmod_time);
				// Last Modified date&time (or header info)
	dynapro_set_word16(&bptr[0x25], fileptr->header_pointer);
#if 0
	printf("Set dir_byte %07x=%04x\n", dir_byte + 0x25,
						fileptr->header_pointer);
#endif

	return dir_byte;
}

// When creating sparse files, always ensure first block is not sparse.  GS/OS
//  does not treat the first block as sparse, it will actually read block 0
// This handles normal files, and one fork of a forked file
word32
dynapro_fork_from_unix(Disk *dsk, byte *fptr, word32 *storage_type_ptr,
					word32 key_block, dword64 dsize)
{
	byte	*bptr;
	word32	sap_block, tree_block, sap_byte, tree_byte, sparse, block_num;
	word32	num_blocks, blocks_used, block_off, storage_type;
	int	num_bytes;
	int	i;

	bptr = &(dsk->raw_data[0]);
	*storage_type_ptr = 0;
	sap_block = 0;
	tree_block = 0;
	num_blocks = (word32)((dsize + 511) >> 9);
	if(num_blocks == 0) {			// 0-length file
		num_blocks = 1;
	} else if(num_blocks > 0x8000) {	// >= 16MB (32K*512)
		printf("File is too large, failing\n");
		return 0;
	}
	block_off = 0;
	blocks_used = 1;
	while(block_off < num_blocks) {
		sparse = (block_off > 0);	// sparse=0 for first block
		for(i = 0; i < 0x200; i++) {
			if(fptr[(block_off << 9) + i] != 0) {
				sparse = 0;
				break;
			}
		}
		if(sparse) {
			block_off++;
			continue;
		}

		if((tree_block == 0) && (num_blocks > 256)) {
			tree_block = dynapro_find_free_block(dsk);
			if(tree_block == 0) {
				return 0;
			}
			blocks_used++;
		}
		tree_byte = (tree_block << 9) + ((block_off >> 8) & 0xff);
		if(tree_block) {
			sap_block = bptr[tree_byte + 0] |
						(bptr[tree_byte + 256] << 8);
		}
		if((sap_block == 0) && (num_blocks > 1)) {
			sap_block = dynapro_find_free_block(dsk);
			if(sap_block == 0) {
				return 0;
			}
			blocks_used++;
			if(tree_block) {
				bptr[tree_byte + 0] = sap_block;
				bptr[tree_byte + 256] = sap_block >> 8;
			}
		}
		if(block_off == 0) {
			block_num = key_block;
		} else {
			block_num = dynapro_find_free_block(dsk);
			if(block_num == 0) {
				return 0;
			}
			blocks_used++;
		}
		sap_byte = (sap_block << 9) | (block_off & 0xff);
		if(sap_block) {
			bptr[sap_byte + 0] = block_num;
			bptr[sap_byte + 256] = block_num >> 8;
		}

		num_bytes = 0x200;
		if(block_off == (dsize >> 9)) {			// Last block
			num_bytes = dsize & 0x1ff;
		}
		for(i = 0; i < num_bytes; i++) {
			bptr[(block_num << 9) + i] = fptr[(block_off << 9) + i];
		}

		block_off++;
	}

	storage_type = 0x10;
	if(tree_block) {
		storage_type = 0x30;
		key_block = tree_block;
	} else if(sap_block) {
		storage_type = 0x20;
		key_block = sap_block;
	}
	*storage_type_ptr = storage_type;
	return (blocks_used << 16) | key_block;
}

word32
dynapro_file_from_unix(Disk *dsk, Dynapro_file *fileptr)
{
	byte	*bptr, *fptr;
	dword64	dsize;
	word32	storage_type, blocks_out, dir_byte;

	fptr = dynapro_malloc_file(fileptr->unix_path, &dsize, 0x200);
	fileptr->eof = (word32)dsize;
#if 0
	printf("file_from_unix %s, size:%08llx, file_type:%02x, dir_byte:"
		"%07x, storage:%02x\n", fileptr->unix_path, dsize,
		fileptr->file_type, fileptr->dir_byte, fileptr->prodos_name[0]);
#endif
	storage_type = 0;
	if((fileptr->prodos_name[0] & 0xf0) == 0x50) {
		// .applesingle file with data and/or resource forks
		blocks_out = applesingle_from_unix(dsk, fileptr, fptr, dsize);
	} else {
		// Normal file
		fileptr->prodos_name[0] = (fileptr->prodos_name[0] & 0xf);

		blocks_out = dynapro_fork_from_unix(dsk, fptr, &storage_type,
					fileptr->key_block, dsize);
	}
	free(fptr);
	fileptr->prodos_name[0] |= storage_type;
	fileptr->key_block = blocks_out & 0xffff;
	fileptr->blocks_used = (blocks_out >> 16) & 0xffff;

	// Update dir_byte information for this file
	dir_byte = fileptr->dir_byte;
	if(dir_byte == 0) {
		dyna_printf("dir_byte is 0 for %s\n", fileptr->unix_path);
	}
	bptr = &(dsk->raw_data[dir_byte]);
	bptr[0] = fileptr->prodos_name[0];
	bptr[0x10] = fileptr->file_type;
	dynapro_set_word16(&bptr[0x11], fileptr->key_block);
	dynapro_set_word16(&bptr[0x13], fileptr->blocks_used);
	dynapro_set_word24(&bptr[0x15], fileptr->eof);
	dynapro_set_word16(&bptr[0x1f], fileptr->aux_type);
#if 0
	printf("Set %s dir_byte:%07x+0x10=%02x (file_type)\n",
		fileptr->unix_path, dir_byte, fileptr->file_type);
#endif

	return blocks_out;
}

word32
dynapro_prep_image(Disk *dsk, const char *dir_path, word32 num_blocks)
{
	Dynapro_info *infoptr;
	byte	*bptr;
	word32	bitmap_size_bytes, bitmap_size_blocks;
	int	pos;
	word32	ui;
	int	i;

	dsk->raw_data = calloc(num_blocks, 512);
	if(dsk->raw_data == 0) {
		dynapro_error(dsk, "Could not allocate %d bytes\n",
							num_blocks * 512);
		return 0;
	}
	dsk->dimage_size = num_blocks * 512LL;
	dsk->dimage_start = 0;
	dsk->raw_dsize = num_blocks * 512LL;

	bptr = &(dsk->raw_data[0]);
	for(i = 0; i < 512; i++) {
		bptr[i] = g_prodos_block0[i];
	}

	// Directory is from blocks 2 through 5.  Set up prev and next ptrs
	bptr = &(dsk->raw_data[2 * 0x200]);
	for(i = 0; i < 3; i++) {		// Blocks 2,3,4 (or 3,4,5)
		dynapro_set_word16(&bptr[(i + 1)*0x200], i + 2); // Prev_blk
		dynapro_set_word16(&bptr[i*0x200 + 2], i + 3);	// Next_blk
	}

	// Calculate bitmap to go in blocks 6...
	bitmap_size_bytes = (num_blocks + 7) >> 3;
	bitmap_size_blocks = (bitmap_size_bytes + 512 - 1) >> 9;
	bptr = &(dsk->raw_data[6 * 512]);		// Block 6
	bptr[0] = 0;
	for(ui = (6 + bitmap_size_blocks); ui < num_blocks; ui++) {
		pos = (ui >> 3);
		bptr[pos] |= (0x80U >> (ui & 7));
	}

	infoptr = calloc(sizeof(Dynapro_info), 1);
	if(!infoptr) {
		return 0;
	}
	infoptr->root_path = kegs_malloc_str(dir_path);
	infoptr->volume_ptr = 0;
	infoptr->block_map_ptr = calloc(num_blocks * sizeof(Dynapro_map), 1);
	infoptr->damaged = 0;
	if((infoptr->root_path == 0) || (infoptr->block_map_ptr == 0)) {
		dynapro_error(dsk, "Could not allocate memory!\n");
		return 0;
	}
	dsk->dynapro_info_ptr = infoptr;
	return 1;
}

word32
dynapro_map_one_file_block(Disk *dsk, Dynapro_file *fileptr, word32 block_num,
			word32 file_offset, word32 eof)
{
	Dynapro_info *info_ptr;
	Dynapro_map *map_ptr;
	byte	*buffer_ptr;
	word32	size, size_to_end;

	info_ptr = dsk->dynapro_info_ptr;
	if(!info_ptr || (block_num >= (dsk->dimage_size >> 9))) {
		printf(" mapping file %s, block %04x is invalid\n",
						fileptr->unix_path, block_num);
		return 0;
	}
	if(info_ptr->block_map_ptr == 0) {
		return 0;
	}
	if(block_num == 0) {
		return 1;
	}
	map_ptr = &(info_ptr->block_map_ptr[block_num]);
	if((map_ptr->file_ptr != 0) || (map_ptr->next_map_block != 0)) {
		dyna_printf("Mapping %s to block %04x, already has file_ptr:"
			"%p, next_map:%04x, mod:%d\n", fileptr->unix_path,
			block_num, map_ptr->file_ptr, map_ptr->next_map_block,
			map_ptr->modified);
		if(map_ptr->file_ptr) {
			dyna_printf(" Existing file: %s\n",
						map_ptr->file_ptr->unix_path);
		}
		return 0;
	}
	//printf(" map file %s block %05x off:%08x\n", fileptr->unix_path,
	//					block_num, file_offset);

	map_ptr->next_map_block = fileptr->map_first_block;
	fileptr->map_first_block = block_num;
	map_ptr->modified = 0;
	map_ptr->file_ptr = fileptr;

	if(file_offset >= eof) {
		return 1;		// This block was an "overhead" block
	}

	buffer_ptr = fileptr->buffer_ptr;
	if(buffer_ptr) {
		// Copy this block in at file_offset
		size = 0x200;
		size_to_end = eof - file_offset;
		if(size_to_end < size) {
			size = size_to_end;
		}
#if 0
		printf("mofb: Write to %p + %07x from block %04x, size:%04x\n",
			buffer_ptr, file_offset, block_num, size);
#endif
		memcpy(buffer_ptr + file_offset,
				&(dsk->raw_data[block_num * 0x200]), size);
	}
	return 1;
}

word32
dynapro_map_file_blocks(Disk *dsk, Dynapro_file *fileptr, word32 block_num,
	int level, word32 file_offset, word32 eof)
{
	byte	*bptr;
	word32	entry_inc, tmp, ret;
	int	i;

#if 0
	printf("dynapro_map_file_blocks %s block_num %05x level:%d off:%08x\n",
			fileptr->unix_path, block_num, level, file_offset);
#endif
	if(level == 0) {
		return 0;		// Bad value, should not happen
	}
	if(level == 1) {
		return dynapro_map_one_file_block(dsk, fileptr, block_num,
							file_offset, eof);
	}
	ret = dynapro_map_one_file_block(dsk, fileptr, block_num, 1U << 30, 0);
	if(ret == 0) {
		return ret;
	}
	entry_inc = 512;
	if(level == 3) {		// Tree
		entry_inc = 256*512;
	}
	bptr = &(dsk->raw_data[block_num * 0x200]);
	for(i = 0; i < 256; i++) {
		tmp = bptr[i] + (bptr[256 + i] << 8);
		if(tmp == 0) {
			continue;
		}
		ret = dynapro_map_file_blocks(dsk, fileptr, tmp, level - 1,
						file_offset + i*entry_inc, eof);
		if(ret == 0) {
			return ret;
		}
	}
	dynapro_debug_map(dsk, "post map_file_blocks");

	return 1;
}

word32
dynapro_map_file(Disk *dsk, Dynapro_file *fileptr, int do_file_data)
{
	word32	block_num, ret;
	int	level;

	level = (fileptr->prodos_name[0] >> 4) & 0xf;
	block_num = fileptr->key_block;
	if(level == 5) {			// Forked file
		return applesingle_map_from_prodos(dsk, fileptr,
							do_file_data);
	} else if((level < 0) || (level >= 4)) {
		printf("Storage_type: %02x for %s is bad\n", level,
							fileptr->unix_path);
		return 0;
	}

	fileptr->buffer_ptr = 0;
	if(do_file_data) {
		// Create a place for data.  We will free before returning
		fileptr->buffer_ptr = calloc(1, fileptr->eof + 0x200);
		if(fileptr->buffer_ptr == 0) {
			printf("malloc failed!\n");
			return 0;
		}
	}
	// Must not return now before free'ing fileptr->buffer_ptr!

	ret = dynapro_map_file_blocks(dsk, fileptr, block_num, level, 0,
								fileptr->eof);

	// printf(" dynapro_map_file, map_file_blocks ret:%04x\n", ret);
	if((ret != 0) && (do_file_data)) {
		// Then, write buffer_ptr to the unix file
		ret = dynapro_write_to_unix_file(fileptr->unix_path,
				fileptr->buffer_ptr, fileptr->eof);
		// printf(" map_file, write_to_unix_file ret:%04x\n", ret);
	}

	// And free the buffer_ptr
	free(fileptr->buffer_ptr);
	fileptr->buffer_ptr = 0;

	return ret;
}

word32
dynapro_map_dir_blocks(Disk *dsk, Dynapro_file *fileptr)
{
	byte	*bptr;
	word32	block_num, ret;
	int	cnt;

	// Loop over all directory blocks marking the map
	block_num = fileptr->key_block;
	if(block_num == 0) {
		printf("dynapro_map_dir_blocks, block_num is 0\n");
		return 0;
	}
	bptr = &(dsk->raw_data[0]);
	cnt = 0;
	fileptr->map_first_block = 0;
	while(block_num != 0) {
		ret = dynapro_map_one_file_block(dsk, fileptr, block_num,
							1U << 30, 0);
		if(ret == 0) {
			printf("dynapro_map_dir_on_block, ret 0, block:%04x\n",
								block_num);
			return 0;
		}
		block_num = dynapro_get_word16(&bptr[(block_num * 0x200) + 2]);
		cnt++;
		if(cnt > 1000) {
			printf("Directory had loop in it, error\n");
			return 0;
		}
	}

	dynapro_debug_map(dsk, "post map_dir_blocks");
	return 1;
}

word32
dynapro_build_map(Disk *dsk, Dynapro_file *fileptr)
{
	word32	ret;

	if(fileptr == 0) {
		return 0;
	}

	// printf("### dynapro_build_map for dir:%s\n", fileptr->unix_path);

	// fileptr points to a directory header (volume or subdir).  Walk
	//  all siblings and build a map
	ret = 1;
	while(fileptr && ret) {
		if(fileptr->prodos_name[0] >= 0xe0) {
			// Directory/Volume header
			ret = dynapro_map_dir_blocks(dsk, fileptr);
		} else if(fileptr->subdir_ptr) {
			// Recurse to handle subdirectory
			ret = dynapro_build_map(dsk, fileptr->subdir_ptr);
		} else {
			ret = dynapro_map_file(dsk, fileptr, 0);
		}
		fileptr = fileptr->next_ptr;
	}
	dynapro_debug_map(dsk, "post build_map");
	return ret;
}

int
dynapro_mount(Disk *dsk, char *dir_path, word32 num_blocks)
{
	word32	ret;

#if 0
	printf("dynapro_mount: %p, %s %08x\n", dsk, dir_path, num_blocks);
#endif
	if(num_blocks >= 65536) {
		num_blocks = 65535;
	}
	ret = dynapro_prep_image(dsk, dir_path, num_blocks);
	if(ret == 0) {
		return -1;
	}

	ret = dynapro_create_dir(dsk, dir_path, 0, 0x404);	// Block 2, +4
	// printf("dynapro_mount will end with ret:%05x\n", ret);
	if(ret != 0) {
		ret = dynapro_build_map(dsk, dsk->dynapro_info_ptr->volume_ptr);
	}
	// dynapro_debug_update(dsk);
	if(ret == 0) {
		dynapro_error(dsk, "Folder too large.  dynapro_build_map "
								"ret:0\n");
	} else {
		ret = dynapro_validate_disk(dsk);
	}
	if(ret == 0) {
		dynapro_error(dsk, "dynapro_validate_disk ret:0\n");
		dynapro_free_dynapro_info(dsk);
		return -1;
	}
#ifndef _WIN32
	setvbuf(stdout, 0, _IOLBF, 0);
#endif
	dsk->fd = 0;
	return 0;
}

