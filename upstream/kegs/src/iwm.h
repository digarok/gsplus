#ifdef INCLUDE_RCSID_C
const char rcsid_iwm_h[] = "@(#)$KmKId: iwm.h,v 1.46 2023-09-23 17:53:09+00 kentd Exp $";
#endif

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

#define MAX_TRACKS	(2*80)
#define MAX_C7_DISKS	16

#define NIB_LEN_525		0x18f2		/* 51088 bits per track */
// Expected bits per track: (1020484/5)/4 = 51024.  A little extra seems good

#define NIBS_FROM_ADDR_TO_DATA	28
// Copy II+ Manual Sector Copy fails if this is 20, so make it 28

// image_type settings.  0 means unknown type
#define DSK_TYPE_PRODOS		1
#define DSK_TYPE_DOS33		2
#define DSK_TYPE_DYNAPRO	3
#define DSK_TYPE_NIB		4
#define DSK_TYPE_WOZ		5

// Note: C031_APPLE35SEL must be 6, C031_CTRL must be 7, MOTOR_ON must be 5!
// Q7 needs to be adjacent and higher than Q6
// Bits 4:0 are IWM mode register: 0: latch mode; 1: async handshake;
//   2: immediate motor off (no 1 sec delay); 3: 2us bit timing;
//   4: Divide input clock by 8 (instead of 7)
#define IWM_BIT_MOTOR_ON		5
#define IWM_BIT_C031_APPLE35SEL		6
#define IWM_BIT_C031_CTRL		7
#define IWM_BIT_STEP_DIRECTION35	8
#define IWM_BIT_MOTOR_ON35		9
#define IWM_BIT_MOTOR_OFF		10
#define IWM_BIT_DRIVE_SEL		11
#define IWM_BIT_Q6			12
#define IWM_BIT_Q7			13
#define IWM_BIT_ENABLE2			14
#define IWM_BIT_LAST_SEL35		15
#define IWM_BIT_PHASES			16
#define IWM_BIT_RESET			20

#define IWM_STATE_MOTOR_ON		(1 << IWM_BIT_MOTOR_ON)
#define IWM_STATE_C031_APPLE35SEL	(1 << IWM_BIT_C031_APPLE35SEL)
#define IWM_STATE_C031_CTRL		(1 << IWM_BIT_C031_CTRL)
#define IWM_STATE_STEP_DIRECTION35	(1 << IWM_BIT_STEP_DIRECTION35)
#define IWM_STATE_MOTOR_ON35		(1 << IWM_BIT_MOTOR_ON35)
#define IWM_STATE_MOTOR_OFF		(1 << IWM_BIT_MOTOR_OFF)
#define IWM_STATE_DRIVE_SEL		(1 << IWM_BIT_DRIVE_SEL)
#define IWM_STATE_Q6			(1 << IWM_BIT_Q6)
#define IWM_STATE_Q7			(1 << IWM_BIT_Q7)
#define IWM_STATE_ENABLE2		(1 << IWM_BIT_ENABLE2)
#define IWM_STATE_LAST_SEL35		(1 << IWM_BIT_LAST_SEL35)
#define IWM_STATE_PHASES		(1 << IWM_BIT_PHASES)
#define IWM_STATE_RESET			(1 << IWM_BIT_RESET)

STRUCT(Trk) {
	byte	*raw_bptr;
	byte	*sync_ptr;
	dword64	dunix_pos;
	word16	unix_len;
	word16	dirty;
	word32	track_bits;
};

STRUCT(Woz_info) {
	byte	*wozptr;
	word32	woz_size;
	int	version;
	int	reparse_needed;
	word32	max_trk_blocks;
	int	meta_size;
	int	trks_size;
	int	tmap_offset;
	int	trks_offset;
	int	info_offset;
	int	meta_offset;
};

typedef struct Dynapro_map_st Dynapro_map;

STRUCT(Dynapro_file) {
	Dynapro_file *next_ptr;
	Dynapro_file *parent_ptr;
	Dynapro_file *subdir_ptr;
	char	*unix_path;
	byte	*buffer_ptr;
	byte	prodos_name[17];	// +0x00-0x0f: [0] is len, nul at end
	word32	dir_byte;		// Byte address of this file's dir ent
	word32	eof;			// +0x15-0x17
	word32	blocks_used;		// +0x13-0x14
	word32	creation_time;		// +0x18-0x1b
	word32	lastmod_time;		// +0x21-0x24
	word16	upper_lower;		// +0x1c-0x1d: Versions: lowercase flags
	word16	key_block;		// +0x11-0x12
	word16	aux_type;		// +0x1f-0x20
	word16	header_pointer;		// +0x25-0x26
	word16	map_first_block;
	byte	file_type;		// +0x10
	byte	modified_flag;
	byte	damaged;
};

struct Dynapro_map_st {
	Dynapro_file *file_ptr;
	word16	next_map_block;
	word16	modified;
};

STRUCT(Dynapro_info) {
	char	*root_path;
	Dynapro_file *volume_ptr;
	Dynapro_map *block_map_ptr;
	int	damaged;
};

STRUCT(Disk) {
	dword64	dfcyc_last_read;
	byte	*raw_data;
	Woz_info *wozinfo_ptr;
	Dynapro_info *dynapro_info_ptr;
	char	*name_ptr;
	char	*partition_name;
	int	partition_num;
	int	fd;
	word32	dynapro_blocks;
	dword64	raw_dsize;
	dword64	dimage_start;
	dword64	dimage_size;
	int	smartport;
	int	disk_525;
	int	drive;
	word32	cur_frac_track;
	int	image_type;
	int	vol_num;
	int	write_prot;
	int	write_through_to_unix;
	int	disk_dirty;
	int	just_ejected;
	int	last_phases;
	dword64	dfcyc_last_phases;
	word32	cur_fbit_pos;
	word32	fbit_mult;
	word32	cur_track_bits;
	int	raw_bptr_malloc;
	Trk	*cur_trk_ptr;
	int	num_tracks;
	Trk	*trks;
};

STRUCT(Iwm) {
	Disk	drive525[2];
	Disk	drive35[2];
	Disk	smartport[MAX_C7_DISKS];
	dword64	dfcyc_last_fastemul_read;
	word32	state;
	word32	motor_off_vbl_count;
	word32	forced_sync_bit;
	word32	last_rd_bit;
	word32	write_val;
	word32	wr_last_bit[5];
	word32	wr_qtr_track[5];
	word32	wr_num_bits[5];
	word32	wr_prior_num_bits[5];
	word32	wr_delta[5];
	int	num_active_writes;
};

STRUCT(Driver_desc) {
	word16	sig;
	word16	blk_size;
	word32	blk_count;
	word16	dev_type;
	word16	dev_id;
	word32	data;
	word16	drvr_count;
};

STRUCT(Part_map) {
	word16	sig;
	word16	sigpad;
	word32	map_blk_cnt;
	word32	phys_part_start;
	word32	part_blk_cnt;
	char	part_name[32];
	char	part_type[32];
	word32	data_start;
	word32	data_cnt;
	word32	part_status;
	word32	log_boot_start;
	word32	boot_size;
	word32	boot_load;
	word32	boot_load2;
	word32	boot_entry;
	word32	boot_entry2;
	word32	boot_cksum;
	char	processor[16];
	char	junk[128];
};

