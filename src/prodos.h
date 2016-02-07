/*
 GSPLUS - Advanced Apple IIGS Emulator Environment
 Copyright (C) 2016 - Dagen Brock
 
 Copyright (C) 2010 by GSport contributors

 Based on the KEGS emulator written by and Copyright (C) 2003 Kent Dickey

 This program is free software; you can redistribute it and/or modify it 
 under the terms of the GNU General Public License as published by the 
 Free Software Foundation; either version 2 of the License, or (at your 
 option) any later version.

 This program is distributed in the hope that it will be useful, but 
 WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
 or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License 
 for more details.

 You should have received a copy of the GNU General Public License along 
 with this program; if not, write to the Free Software Foundation, Inc., 
 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

typedef struct l2byte_st L2byte;
struct l2byte_st {
	byte low;
	byte hi;
};

typedef struct l3byte_st L3byte;
struct l3byte_st {
	byte low;
	byte hi;
	byte higher;
};

typedef L2byte Block;

typedef struct pro_time_st Pro_time;
struct pro_time_st {
	byte times[4];
};

typedef struct file_entry_st File_entry;
struct file_entry_st {
	byte storage_type_name_len;
	byte file_name[15];
/* 0x10 */
	byte file_type;
	Block key_pointer;
/* 0x13 */
	L2byte blocks_used;
/* 0x15 */
	L3byte eof;
/* 0x18 */
	Pro_time creation_time;
/* 0x1c */
	byte version;
	byte min_version;
/* 0x1e */
	byte access;
/* 0x1f */
	L2byte aux_type;
/* 0x21 */
	Pro_time last_mod;
/* 0x25 */
	Block header_pointer;
};

STRUCT(Vol_hdr) {
/* 0x4 */
	byte storage_type_name_len;
/* 0x5 */
	byte vol_name[15];
/* 0x14 */
	byte res1[8];
/* 0x1c */
	Pro_time creation_time;
/* 0x20 */
	byte version;
	byte min_version;
	byte access;
	byte entry_length;
/* 0x24 */
	byte entries_per_block;
	L2byte file_count;
/* 0x27 */
	Block bit_map;
/* 0x29 */
	L2byte total_blocks;
};

typedef struct directory_st Directory;
struct directory_st {
	Block prev_blk;
	Block next_blk;
	File_entry file_entries[13];
};

STRUCT(ProDisk) {
	int	fd;
	int	total_blocks;
	int	size_bitmap_blocks;
	int	disk_bytes_left;

	int	bitmap_bytes;
	int	bitmap_cur_pos;
	byte	*bitmap_ptr;

	int	file_open;
	File_entry *file_ptr;
	int	dir_blk_num;
	int	ind_blk_num;
	int	master_ind_blk_num;
	byte	dir_blk_data[512];
	byte	ind_blk_data[512];
	byte	master_ind_blk_data[512];
};


#include "prodos_protos.h"
