const char rcsid_config_c[] = "@(#)$KmKId: config.c,v 1.169 2025-01-11 23:42:49+00 kentd Exp $";

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

// g_cfg_slotdrive: 0: not doing file selection at all
//			1-0x7ff: doing file selection for given slot/drive
//			0xfff: doing file selection for ROM or charrom
#include "defc.h"
#include <stdarg.h>
#include "config.h"

#ifdef _WIN32
# include "win_dirent.h"
#else
# include <dirent.h>
#endif

extern int Verbose;
extern word32 g_vbl_count;

extern int g_track_bytes_35[];
extern int g_c031_disk35;

extern int g_cur_a2_stat;
extern byte *g_slow_memory_ptr;
extern byte *g_rom_fc_ff_ptr;
extern byte *g_rom_cards_ptr;
extern double g_cur_dcycs;
extern int g_rom_version;

extern word32 g_adb_repeat_vbl;
extern int g_adb_swap_command_option;

extern int g_limit_speed;
extern int g_zip_speed_mhz;
extern int g_force_depth;
int g_serial_cfg[2] = { 0, 1 };		// Slot 1=0=Real serial (printer?)
					// Slot 2=1=Virt Modem
int g_serial_mask[2] = { 0, 0 };
char *g_serial_remote_ip[2] = { "", "" };	// cfg_init_menus will malloc()
int g_serial_remote_port[2] = { 9100, 9100 };
char *g_serial_device[2] = { "/dev/tty.USB.0", "/dev/tty.USB.1" };
				// cfg_init_menus() will malloc() the above
int g_serial_win_device[2] = { 0, 0 };		// Disabled
int g_serial_modem_response_code = 10;		// 10 - 2400
int g_serial_modem_allow_incoming = 0;		// 1 for BBS'es
int g_serial_modem_init_telnet = 1;		// 1 for BBS'es

extern word32 g_mem_size_base;
extern word32 g_mem_size_exp;
extern int g_video_line_update_interval;
extern int g_user_halt_bad;
extern int g_joystick_type;
extern int g_joystick_scale_factor_x;
extern int g_joystick_scale_factor_y;
extern int g_joystick_trim_amount_x;
extern int g_joystick_trim_amount_y;
extern int g_swap_paddles;
extern int g_invert_paddles;
extern int g_voc_enable;
extern int g_status_enable;
extern int g_mainwin_width;
extern int g_mainwin_height;
extern int g_mainwin_xpos;
extern int g_mainwin_ypos;

extern int g_screen_index[];
extern word32 g_full_refresh_needed;
extern word32 g_a2_screen_buffer_changed;

extern int g_key_down;
extern const char g_kegs_version_str[];

int g_config_control_panel = 0;
char g_config_kegs_name[1024] = { 0 };
char g_cfg_cwd_str[CFG_PATH_MAX] = { 0 };

int g_config_kegs_auto_update = 1;
int g_config_kegs_update_needed = 0;
int g_cfg_newdisk_select = 0;
int g_cfg_newdisk_blocks = 0;
int g_cfg_newdisk_blocks_default = 140*2;
int g_cfg_newdisk_type = 1;
int g_cfg_newdisk_type_default = 1;
word32 g_cfg_newdisk_slotdrive = 0;

const char *g_config_kegs_name_list[] = {
		"config.kegs", "kegs_conf", ".config.kegs", 0
};

int	g_highest_smartport_unit = -1;
int	g_reparse_delay = 0;
int	g_user_page2_shadow = 1;

char	g_cfg_printf_buf[CFG_PRINTF_BUFSIZE];
char	g_config_kegs_buf[CONF_BUF_LEN];

#define CFG_ERR_BUFSIZE		80
#define CFG_ERR_MAX		5

int	g_cfg_err_pos = 0;
char	g_cfg_err_bufs[CFG_ERR_MAX][CFG_ERR_BUFSIZE];

int	g_cfg_curs_x = 0;
int	g_cfg_curs_y = 0;
int	g_cfg_curs_inv = 0;
int	g_cfg_curs_mousetext = 0;
int	g_cfg_screen_changed = 0;
byte	g_cfg_screen[24][80];

#if defined(MAC) || defined(_WIN32)
int	g_cfg_ignorecase = 1;		// Ignore case in filenames
#else
int	g_cfg_ignorecase = 0;
#endif

#define CFG_MAX_OPTS	34
#define CFG_OPT_MAXSTR	100

int g_cfg_opts_vals[CFG_MAX_OPTS];
char g_cfg_opts_str[CFG_PATH_MAX];
char g_cfg_opt_buf[CFG_OPT_MAXSTR];
char g_cfg_edit_buf[CFG_OPT_MAXSTR];

char *g_cfg_rom_path = "ROM";			// config_init_menus will malloc
char *g_cfg_charrom_path = "Undefined";		// config_init_menus will malloc
int g_cfg_charrom_pos = 0;
char *g_cfg_file_def_name = "Undefined";
char **g_cfg_file_strptr = 0;
int g_cfg_file_min_size = 1024;
int g_cfg_file_max_size = 2047*1024*1024;
int	g_cfg_edit_type = 0;
void	*g_cfg_edit_ptr = 0;

#define MAX_PARTITION_BLK_SIZE		65536

char *g_argv0_path = ".";

const char *g_kegs_default_paths[] = { "", "./", "${HOME}/",
	"${HOME}/Library/KEGS/", "${0}/../", "${0}/",
	"${0}/Contents/Resources/", 0 };


extern Cfg_menu g_cfg_main_menu[];

#define KNMP(a)		&a, #a, 0
#define KNM(a)		&a, #a

Cfg_menu g_cfg_disk_menu[] = {
{ "Disk Configuration", g_cfg_disk_menu, 0, 0, CFGTYPE_MENU },
{ "s5d1 = ", 0, 0, 0, CFGTYPE_DISK + 0x5000 },
{ "s5d2 = ", 0, 0, 0, CFGTYPE_DISK + 0x5010 },
{ "", 0, 0, 0, 0 },
{ "s6d1 = ", 0, 0, 0, CFGTYPE_DISK + 0x6000 },
{ "s6d2 = ", 0, 0, 0, CFGTYPE_DISK + 0x6010 },
{ "", 0, 0, 0, 0 },
{ "s7d1 = ", 0, 0, 0, CFGTYPE_DISK + 0x7000 },
{ "s7d2 = ", 0, 0, 0, CFGTYPE_DISK + 0x7010 },
{ "s7d3 = ", 0, 0, 0, CFGTYPE_DISK + 0x7020 },
{ "s7d4 = ", 0, 0, 0, CFGTYPE_DISK + 0x7030 },
{ "s7d5 = ", 0, 0, 0, CFGTYPE_DISK + 0x7040 },
{ "s7d6 = ", 0, 0, 0, CFGTYPE_DISK + 0x7050 },
{ "s7d7 = ", 0, 0, 0, CFGTYPE_DISK + 0x7060 },
{ "s7d8 = ", 0, 0, 0, CFGTYPE_DISK + 0x7070 },
{ "s7d9 = ", 0, 0, 0, CFGTYPE_DISK + 0x7080 },
{ "s7d10= ", 0, 0, 0, CFGTYPE_DISK + 0x7090 },
{ "s7d11= ", 0, 0, 0, CFGTYPE_DISK + 0x70a0 },
{ "s7d12= ", 0, 0, 0, CFGTYPE_DISK + 0x70b0 },
{ "", 0, 0, 0, 0 },
{ "Back to Main Config", g_cfg_main_menu, 0, 0, CFGTYPE_MENU },
{ 0, 0, 0, 0, 0 },
};

Cfg_menu g_cfg_newslot6_menu[] = {
{ "New 5.25\" disk image Configuration", g_cfg_newslot6_menu, 0, 0,
								CFGTYPE_MENU },
{ "size,280,140KB", KNM(g_cfg_newdisk_blocks),
			&g_cfg_newdisk_blocks_default, CFGTYPE_INT },
{ "Type,1,ProDOS/DOS 3.3,2,WOZ image,3,Dynamic ProDOS directory",
			KNM(g_cfg_newdisk_type),
			&g_cfg_newdisk_type_default, CFGTYPE_INT },
{ "", 0, 0, 0, 0 },
{ "Create and name the image", (void *)cfg_name_new_image, 0, 0, CFGTYPE_FUNC },
{ "", 0, 0, 0, 0 },
{ "Cancel, go back to Disk Config", g_cfg_disk_menu, 0, 0, CFGTYPE_MENU },
{ 0, 0, 0, 0, 0 },
};

Cfg_menu g_cfg_newslot5_menu[] = {
{ "New 3.5\" disk image Configuration", g_cfg_newslot5_menu, 0, 0,CFGTYPE_MENU},
{ "size,1600,800KB", KNM(g_cfg_newdisk_blocks),
			&g_cfg_newdisk_blocks_default, CFGTYPE_INT },
{ "Type,1,ProDOS,2,WOZ image,3,Dynamic ProDOS directory",
			KNM(g_cfg_newdisk_type),
			&g_cfg_newdisk_type_default, CFGTYPE_INT },
{ "", 0, 0, 0, 0 },
{ "Create and name the image", (void *)cfg_name_new_image, 0, 0, CFGTYPE_FUNC },
{ "", 0, 0, 0, 0 },
{ "Cancel, go back to Disk Config", g_cfg_disk_menu, 0, 0, CFGTYPE_MENU },
{ 0, 0, 0, 0, 0 },
};

Cfg_menu g_cfg_newslot7_menu[] = {
{ "New Smartport disk image Configuration", g_cfg_newslot7_menu, 0, 0,
								CFGTYPE_MENU},
{ "size,1600,800KB,3200,1600KB,16384,8MB,32768,16MB,65535,32MB",
		KNM(g_cfg_newdisk_blocks), &g_cfg_newdisk_blocks_default,
			CFGTYPE_INT },
{ "Type,1,ProDOS,3,Dynamic ProDOS directory", KNM(g_cfg_newdisk_type),
			&g_cfg_newdisk_type_default, CFGTYPE_INT },
{ "", 0, 0, 0, 0 },
{ "Create and name the image", (void *)cfg_name_new_image, 0, 0, CFGTYPE_FUNC },
{ "", 0, 0, 0, 0 },
{ "Cancel, go back to Disk Config", g_cfg_disk_menu, 0, 0, CFGTYPE_MENU },
{ 0, 0, 0, 0, 0 },
};

Cfg_menu g_cfg_joystick_menu[] = {
{ "Joystick Configuration", g_cfg_joystick_menu, 0, 0, CFGTYPE_MENU },
{ "Joystick Emulation,0,Keypad Joystick,1,Mouse Joystick,2,Native Joystick 1,"
	"3,Native Joystick 2", KNMP(g_joystick_type), CFGTYPE_INT },
{ "Joystick Scale X,0x100,Standard,0x119,+10%,0x133,+20%,"
	"0x150,+30%,0xb0,-30%,0xcd,-20%,0xe7,-10%",
		KNMP(g_joystick_scale_factor_x), CFGTYPE_INT },
{ "Joystick Scale Y,0x100,Standard,0x119,+10%,0x133,+20%,"
	"0x150,+30%,0xb0,-30%,0xcd,-20%,0xe7,-10%",
		KNMP(g_joystick_scale_factor_y), CFGTYPE_INT },
{ "Joystick Trim X", KNMP(g_joystick_trim_amount_x), CFGTYPE_INT },
{ "Joystick Trim Y", KNMP(g_joystick_trim_amount_y), CFGTYPE_INT },
{ "Swap Joystick X and Y,0,Normal operation,1,Paddle 1 and Paddle 0 swapped",
		KNMP(g_swap_paddles), CFGTYPE_INT },
{ "Invert Joystick,0,Normal operation,1,Left becomes right and up becomes down",
		KNMP(g_invert_paddles), CFGTYPE_INT },
{ "", 0, 0, 0, 0 },
{ "Back to Main Config", g_cfg_main_menu, 0, 0, CFGTYPE_MENU },
{ 0, 0, 0, 0, 0 },
};

Cfg_menu g_cfg_rom_menu[] = {
{ "ROM File Selection", g_cfg_rom_menu, 0, 0, CFGTYPE_MENU },
{ "ROM File", KNMP(g_cfg_rom_path), CFGTYPE_FILE },
{ "", 0, 0, 0, 0 },
{ "Back to Main Config", g_cfg_main_menu, 0, 0, CFGTYPE_MENU },
{ 0, 0, 0, 0, 0 },
};

Cfg_menu g_cfg_charrom_menu[] = {
{ "Character ROM File Selection", g_cfg_charrom_menu, 0, 0, CFGTYPE_MENU },
{ "Character ROM File", KNMP(g_cfg_charrom_path), CFGTYPE_FILE },
{ "Character Set,0,0x00 US Enhanced,1,0x01 US Un-enhanced,"
	"2,0x02 Clinton Turner V1 Enhanced,3,0x03 ReActiveMicro Enhanced,"
	"4,0x04 Dan Paymar Enhanced,5,0x05 Blippo Black Enhanced,"
	"6,0x06 Byte Enhanced,7,0x07 Colossal Enhanced,"
	"8,0x08 Count Enhanced,9,0x09 Flow Enhanced,"
	"10,0x0a Gothic Enhanced,11,0x0b Outline Enhanced,"
	"12,0x0c Pigfont Enhanced,13,0x0d Pinocchio Enhanced,"
	"14,0x0e Slant Enhanced,15,0x0f Stop Enhanced,"
	"16,0x10 Euro Un-Enhanced,17,0x11 Euro Enhanced,"
	"18,0x12 Clinton Turner V2 Enhanced,19,0x13 Improved German Enhanced,"
	"20,0x14 Improved German Un-Enhanced,21,0x15 Franch Canadian Enhanced,"
	"22,0x16 French Canadian Un-Enhanced,23,0x17 Hebrew Enhanced,"
	"24,0x18 Hebrew Un-Enhanced,25,0x19 Apple II+ Enhanced,"
	"26,0x1a Apple II+ Un-Enhanced,27,0x1b Katakana Enhanced,"
	"28,0x1c Cyrillic Enhanced,29,0x1d Greek Enhanced,"
	"30,0x1e Esperanto Enhanced,31,0x1f Videx Enhanced",
	KNMP(g_cfg_charrom_pos), CFGTYPE_INT },
{ "", 0, 0, 0, 0 },
{ "Back to Main Config", g_cfg_main_menu, 0, 0, CFGTYPE_MENU },
{ 0, 0, 0, 0, 0 },
};

Cfg_menu g_cfg_serial_menu[] = {
{ "Serial Port Configuration", g_cfg_serial_menu, 0, 0, CFGTYPE_MENU },
{ "Slot 1 (port 0) settings", 0, 0, 0, 0 },
{ "  Main setting  ,0,Use Real Device below,1,Use a virtual modem,"
		"2,Use Remote IP below,3,Use incoming port 6501",
		KNMP(g_serial_cfg[0]), CFGTYPE_INT },
{ "  Status        ", (void *)cfg_get_serial0_status, 0, 0, CFGTYPE_FUNC },
{ "  Real Device   ", KNMP(g_serial_device[0]), CFGTYPE_FILE },
{ "  Windows Device,0,Disabled,1,COM1,2,COM2,3,COM3,4,COM4",
		KNMP(g_serial_win_device[0]), CFGTYPE_INT },
{ "  Remote IP     ", KNMP(g_serial_remote_ip[0]), CFGTYPE_STR },
{ "  Remote Port   ", KNMP(g_serial_remote_port[0]), CFGTYPE_INT },
{ "  Serial Mask   ,0,Send full 8-bit data,1,Mask off high bit",
		KNMP(g_serial_mask[0]), CFGTYPE_INT },
{ "", 0, 0, 0, 0 },
{ "Slot 2 (port 1) settings", 0, 0, 0, 0, },
{ "  Main setting  ,0,Use Real Device below,1,Use a virtual modem,"
		"2,Use Remote IP below,3,Use incoming port 6502",
		KNMP(g_serial_cfg[1]), CFGTYPE_INT },
{ "  Status        ", (void *)cfg_get_serial1_status, 0, 0, CFGTYPE_FUNC },
{ "  Real Device   ", KNMP(g_serial_device[1]), CFGTYPE_FILE },
{ "  Windows Device,1,COM1,2,COM2,3,COM3,4,COM4",
		KNMP(g_serial_win_device[1]), CFGTYPE_INT },
{ "  Remote IP     ", KNMP(g_serial_remote_ip[1]), CFGTYPE_STR },
{ "  Remote Port   ", KNMP(g_serial_remote_port[1]), CFGTYPE_INT },
{ "  Serial Mask   ,0,Send full 8-bit data,1,Mask off high bit",
		KNMP(g_serial_mask[1]), CFGTYPE_INT },
{ "", 0, 0, 0, 0 },
{ "Back to Main Config", g_cfg_main_menu, 0, 0, CFGTYPE_MENU },
{ 0, 0, 0, 0, 0 },
};

Cfg_menu g_cfg_modem_menu[] = {
{ "Virtual Modem Configuration", g_cfg_modem_menu, 0, 0, CFGTYPE_MENU },
{ "Modem Speed Response Code          ,5,5 - CONNECT 1200,10,10 - CONNECT 2400,"
		"12,12 - CONNECT 9600 (HAYES/Warp6),"
		"13,13 - CONNECT 9600 (USR/HST),"
		"14,14 - CONNECT 19200 (HAYES/Warp6),"
		"28,28 - CONNECT 38400 (HAYES/Warp6)",
		KNMP(g_serial_modem_response_code), CFGTYPE_INT },
{ "Allow Modem incoming on 6501/6502  ,0,Outgoing only,"
		"1,Incoming and outgoing (BBS)",
		KNMP(g_serial_modem_allow_incoming), CFGTYPE_INT },
{ "Send Telnet Escape codes           ,0,Disable Telnet,1,Send Telnet codes",
		KNMP(g_serial_modem_init_telnet), CFGTYPE_INT },
{ "", 0, 0, 0, 0 },
{ "Back to Main Config", g_cfg_main_menu, 0, 0, CFGTYPE_MENU },
{ 0, 0, 0, 0, 0 },
};



Cfg_menu g_cfg_video_menu[] = {
{ "Force X-windows display depth", KNMP(g_force_depth), CFGTYPE_INT },
{ "Enable VOC,0,Disabled,1,Enabled", KNMP(g_voc_enable), CFGTYPE_INT },
{ "Default Main Window width", KNMP(g_mainwin_width), CFGTYPE_INT },
{ "Default Main Window height", KNMP(g_mainwin_height), CFGTYPE_INT },
{ "Main Window X position", KNMP(g_mainwin_xpos), CFGTYPE_INT },
{ "Main Window Y position", KNMP(g_mainwin_ypos), CFGTYPE_INT },
{ "3200 Color Enable,0,Auto (Full if fast enough),1,Full (Update every line),"
	"8,Off (Update video every 8 lines)",
		KNMP(g_video_line_update_interval), CFGTYPE_INT },
{ "Dump text screen to file", (void *)cfg_text_screen_dump, 0, 0, CFGTYPE_FUNC},
{ "", 0, 0, 0, 0 },
{ "Back to Main Config", g_cfg_main_menu, 0, 0, CFGTYPE_MENU },
{ 0, 0, 0, 0, 0 },
};

Cfg_menu g_cfg_main_menu[] = {
{ "KEGS Configuration", g_cfg_main_menu, 0, 0, CFGTYPE_MENU },
{ "Disk Configuration", g_cfg_disk_menu, 0, 0, CFGTYPE_MENU },
{ "Joystick Configuration", g_cfg_joystick_menu, 0, 0, CFGTYPE_MENU },
{ "ROM File Selection", g_cfg_rom_menu, 0, 0, CFGTYPE_MENU },
{ "Character ROM Selection", g_cfg_charrom_menu, 0, 0, CFGTYPE_MENU },
{ "Serial Port Configuration", g_cfg_serial_menu, 0, 0, CFGTYPE_MENU },
{ "Virtual Modem Configuration", g_cfg_modem_menu, 0, 0, CFGTYPE_MENU },
{ "Video Settings", g_cfg_video_menu, 0, 0, CFGTYPE_MENU },
{ "Auto-update config.kegs,0,Manual,1,Immediately",
		KNMP(g_config_kegs_auto_update), CFGTYPE_INT },
{ "Speed,0,Unlimited,1,1.0MHz,2,2.8MHz,3,8.0MHz (Zip)",
		KNMP(g_limit_speed), CFGTYPE_INT },
{ "ZipGS Speed,8,8MHz,16,16MHz,32,32MHz,64,64MHz,128,128MHz",
		KNMP(g_zip_speed_mhz), CFGTYPE_INT },
{ "Expansion Mem Size,0,0MB,0x100000,1MB,0x200000,2MB,0x300000,3MB,"
	"0x400000,4MB,0x600000,6MB,0x800000,8MB,0xa00000,10MB,0xc00000,12MB,"
	"0xe00000,14MB", KNMP(g_mem_size_exp), CFGTYPE_INT },
{ "Show Status lines,0,Disabled,1,Enabled", KNMP(g_status_enable), CFGTYPE_INT},
{ "Code Red Halts,0,Do not stop on bad accesses,1,Enter debugger on bad "
		"accesses", KNMP(g_user_halt_bad), CFGTYPE_INT },
{ "Enable Text Page 2 Shadow,0,Disabled on ROM 01 (matches real hardware),"
	"1,Enabled on ROM 01 and 03",
		KNMP(g_user_page2_shadow), CFGTYPE_INT },
{ "Swap Command/Option keys,0,Disabled,1,Swapped",
				KNMP(g_adb_swap_command_option), CFGTYPE_INT },
{ "", 0, 0, 0, 0 },
{ "Save changes to config.kegs", (void *)config_write_config_kegs_file, 0, 0,
		CFGTYPE_FUNC },
{ "", 0, 0, 0, 0 },
{ "Exit Config (or press F4)", (void *)cfg_exit, 0, 0, CFGTYPE_FUNC },
{ 0, 0, 0, 0, 0 },
};


#define CFG_MAX_DEFVALS	128
Cfg_defval g_cfg_defvals[CFG_MAX_DEFVALS];
int g_cfg_defval_index = 0;

word32 g_cfg_slotdrive = 0;
int g_cfg_select_partition = -1;
char g_cfg_tmp_path[CFG_PATH_MAX];
char g_cfg_file_path[CFG_PATH_MAX];
char g_cfg_file_cachedpath[CFG_PATH_MAX];
char g_cfg_file_cachedreal[CFG_PATH_MAX];
char g_cfg_file_curpath[CFG_PATH_MAX];
char g_cfg_file_shortened[CFG_PATH_MAX];
char g_cfg_file_match[CFG_PATH_MAX];

char g_cfg_part_path[CFG_PATH_MAX];
int	g_cfg_partition_is_zip = 0;

Cfg_listhdr g_cfg_dirlist = { 0 };
Cfg_listhdr g_cfg_partitionlist = { 0 };

int g_cfg_file_pathfield = 0;

const char *g_kegs_rom_names[] = { "ROM", "ROM", "ROM.01", "ROM.03",
	"APPLE2GS.ROM", "APPLE2GS.ROM2", "xgs.rom", "XGS.ROM", "Rom03gd",
	"342-0077-b", // MAME ROM.01
	0 };
	/* First entry is special--it will be overwritten by g_cfg_rom_path */

const char *g_kegs_c1rom_names[] = { 0 };
const char *g_kegs_c2rom_names[] = { 0 };
const char *g_kegs_c3rom_names[] = { 0 };
const char *g_kegs_c4rom_names[] = { 0 };
const char *g_kegs_c5rom_names[] = { 0 };
const char *g_kegs_c6rom_names[] = { "c600.rom", "controller.rom", "disk.rom",
				"DISK.ROM", "diskII.prom", 0 };
const char *g_kegs_c7rom_names[] = { 0 };

const char **g_kegs_rom_card_list[8] = {
	0,			g_kegs_c1rom_names,
	g_kegs_c2rom_names,	g_kegs_c3rom_names,
	g_kegs_c4rom_names,	g_kegs_c5rom_names,
	g_kegs_c6rom_names,	g_kegs_c7rom_names };

byte g_rom_c600_rom01_diffs[256] = {
	0x00, 0x00, 0x00, 0x00, 0xc6, 0x00, 0xe2, 0x00,
	0xd0, 0x50, 0x0f, 0x77, 0x5b, 0xb9, 0xc3, 0xb1,
	0xb1, 0xf8, 0xcb, 0x4e, 0xb8, 0x60, 0xc7, 0x2e,
	0xfc, 0xe0, 0xbf, 0x1f, 0x66, 0x37, 0x4a, 0x70,
	0x55, 0x2c, 0x3c, 0xfc, 0xc2, 0xa5, 0x08, 0x29,
	0xac, 0x21, 0xcc, 0x09, 0x55, 0x03, 0x17, 0x35,
	0x4e, 0xe2, 0x0c, 0xe9, 0x3f, 0x9d, 0xc2, 0x06,
	0x18, 0x88, 0x0d, 0x58, 0x57, 0x6d, 0x83, 0x8c,
	0x22, 0xd3, 0x4f, 0x0a, 0xe5, 0xb7, 0x9f, 0x7d,
	0x2c, 0x3e, 0xae, 0x7f, 0x24, 0x78, 0xfd, 0xd0,
	0xb5, 0xd6, 0xe5, 0x26, 0x85, 0x3d, 0x8d, 0xc9,
	0x79, 0x0c, 0x75, 0xec, 0x98, 0xcc, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x77, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x7b, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x39, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x39, 0x00, 0x35, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x4a, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x64, 0x00, 0x00,
	0x6c, 0x44, 0xce, 0x4c, 0x01, 0x08, 0x00, 0x00
};

byte g_rom_c700[256] = {
	//0xa2, 0x20, 0xa2, 0x00, 0xa2, 0x03, 0xc9, 0x3c,	// For Apple //e
	0xa2, 0x20, 0xa2, 0x00, 0xa2, 0x03, 0xc9, 0x00,
	//^^= LDX #$20; LDY #$00, LDX #$03  CMP #$3c
	0x80, 0x0c, 0x18, 0xb8, 0x70, 0x38, 0xb8, 0x42,
	//^^= BRA $c716; CLC; CLV; BVS $c746 (SEC); CLV; WDM $c7,$00
	0xc7, 0x00, 0x60, 0x00, 0x00, 0xea, 0xe2, 0x41,
	//^^=  ...; RTS..............; NOP; SEP #$41
	0x70, 0xf5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	//^^= BVS $c70f
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	// So does WDM $c7,$00 with psr.v=1 for $c700; v=0,c=0 for $c70a,
	//  and v=0,c=1 for $c70d
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0xbf, 0x0a
};

Cfg_menu *g_menuptr = 0;
int	g_menu_line = 1;
int	g_menu_inc = 1;
int	g_menu_max_line = 1;
int	g_menu_redraw_needed = 1;

#define MAX_CFG_ARGV_OVERRIDES		64

int g_cfg_argv_num_overrides = 0;
char *g_cfg_argv_overrides[MAX_CFG_ARGV_OVERRIDES];

int
config_add_argv_override(const char *str1, const char *str2)
{
	const char *equal_ptr;
	char	*str;
	int	ret, pos, len;

	// Handle things like "rom=rompath" and "rom", "rompath"
	// Look through str1, see if there is '=', if so ignore str2
	equal_ptr = strchr(str1, '=');
	ret = 1;
	if(equal_ptr) {			// str1 has '=' in it
		ret = 0;		// Don't eat up str2 argument
		str = kegs_malloc_str(str1);
	} else {
		// We need to form a new string of str1, =, str2
		len = (int)(strlen(str1) + strlen(str2) + 2);
		str = malloc(len);
		cfg_strncpy(str, str1, len);
		cfg_strlcat(str, "=", len);
		cfg_strlcat(str, str2, len);
	}
	pos = g_cfg_argv_num_overrides++;
	if(pos >= MAX_CFG_ARGV_OVERRIDES) {
		g_cfg_argv_num_overrides = MAX_CFG_ARGV_OVERRIDES;
		fatal_printf("MAX_CFG_ARGV_OVERRIDES overflow\n");
		my_exit(5);
		return ret;
	}
	g_cfg_argv_overrides[pos] = str;
	printf("Added config override %d, %s\n", pos, str);

	return ret;
}

void
config_set_config_kegs_name(const char *str1)
{
	int	maxlen;

	//	Command line override "-cfg cfg_file"
	g_config_kegs_name[0] = 0;
	maxlen = (int)sizeof(g_config_kegs_name);
	cfg_strncpy(&g_config_kegs_name[0], str1, maxlen);
}

void
config_init_menus(Cfg_menu *menuptr)
{
	void	*voidptr;
	const char *name_str;
	Cfg_defval *defptr;
	char	**str_ptr;
	char	*str;
	int	type, pos, val;

	if(menuptr[0].defptr != 0) {
		return;
	}
	menuptr[0].defptr = (void *)1;
	pos = 0;
	while(pos < 100) {
		type = menuptr->cfgtype;
		voidptr = menuptr->ptr;
		name_str = menuptr->name_str;
		if(menuptr->str == 0) {
			break;
		}
		if(name_str != 0) {
			defptr = &(g_cfg_defvals[g_cfg_defval_index++]);
			if(g_cfg_defval_index >= CFG_MAX_DEFVALS) {
				fatal_printf("CFG_MAX_DEFVAL overflow\n");
				my_exit(5);
				return;
			}
			defptr->menuptr = menuptr;
			defptr->intval = 0;
			defptr->strval = 0;
			switch(type) {
			case CFGTYPE_INT:
				val = *((int *)voidptr);
				defptr->intval = val;
				menuptr->defptr = &(defptr->intval);
				break;
			case CFGTYPE_FILE:
			case CFGTYPE_STR:
				str_ptr = (char **)menuptr->ptr;
				str = *str_ptr;
				// We need to malloc this string since all
				//  string values must be dynamically alloced
				defptr->strval = str;	// this can have a copy
				*str_ptr = kegs_malloc_str(str);
				menuptr->defptr = &(defptr->strval);
				break;
			default:
				fatal_printf("name_str is %p = %s, but type: "
					"%d\n", name_str, name_str, type);
				my_exit(5);
				return;
			}
		}
		if(type == CFGTYPE_MENU) {
			config_init_menus((Cfg_menu *)voidptr);
		}
		pos++;
		menuptr++;
	}
}

void
config_init()
{
	config_init_menus(g_cfg_main_menu);

	// Find the config.kegs file
	if(g_config_kegs_name[0] == 0) {
		cfg_find_config_kegs_file();
	}

	config_parse_config_kegs_file();
}

void
cfg_find_config_kegs_file()
{
	const char **path_ptr;
	int	maxlen, fd;

	g_config_kegs_name[0] = 0;
	maxlen = sizeof(g_config_kegs_name);
	fd = 0;
	if(!config_setup_kegs_file(&g_config_kegs_name[0], maxlen,
						&g_config_kegs_name_list[0])) {
		// Try to create config.kegs
		fd = -1;
		path_ptr = &g_kegs_default_paths[0];
		while(*path_ptr) {
			config_expand_path(&g_config_kegs_name[0], *path_ptr,
									maxlen);
			cfg_strlcat(&g_config_kegs_name[0], "config.kegs",
									maxlen);
			printf("Trying to create %s\n", &g_config_kegs_name[0]);
			fd = open(&g_config_kegs_name[0],
					O_CREAT | O_TRUNC | O_WRONLY, 0x1b6);
			close(fd);
			if(fd >= 0) {
				break;
			}
			path_ptr++;
		}
	}

	if(fd < 0) {
		fatal_printf("Could not create config.kegs!\n");
		my_exit(2);
	}
}

int
config_setup_kegs_file(char *outname, int maxlen, const char **name_ptr)
{
	struct stat stat_buf;
	const char **path_ptr, **cur_name_ptr;
	mode_t	fmt;
	int	ret, len;

	outname[0] = 0;

	path_ptr = &g_kegs_default_paths[0];		// Array of strings

	while(*path_ptr) {
		len = config_expand_path(outname, *path_ptr, maxlen);
		if(len != (int)strlen(outname)) {
			printf("config_expand_path ret %d, but strlen:%d!\n",
						len, (int)strlen(outname));
		}
		cur_name_ptr = name_ptr;
		while(*cur_name_ptr && (len < maxlen)) {
			outname[len] = 0;
			cfg_strlcat(outname, *cur_name_ptr, maxlen);
			// printf("Doing stat on %s\n", outname);
			ret = cfg_stat(outname, &stat_buf, 0);
			if(ret == 0) {
				fmt = stat_buf.st_mode & S_IFMT;
				if(fmt != S_IFDIR) {
					/* got it! */
					return 1;
				}
			}
			cur_name_ptr++;
		}
		path_ptr++;
	}

	return 0;
}

int
config_expand_path(char *out_ptr, const char *in_ptr, int maxlen)
{
	char	name_buf[256];
	char	*tmp_ptr;
	int	name_len, in_char, state, pos;

	out_ptr[0] = 0;

	pos = 0;
	name_len = 0;
	state = 0;

	/* See if in_ptr has ${} notation, replace with getenv or argv0 */
	while(pos < (maxlen - 1)) {
		in_char = *in_ptr++;
		out_ptr[pos++] = in_char;
		out_ptr[pos] = 0;
		if(in_char == 0) {
			return pos - 1;
		}
		if(state == 0) {
			/* No $ seen yet, look for it */
			if(in_char == '$') {
				state = 1;
			}
		} else if(state == 1) {
			/* See if next char is '{' (dummy }) */
			if(in_char == '{') {		/* add dummy } */
				state = 2;
				name_len = 0;
				pos -= 2;
				out_ptr[pos] = 0;
			} else {
				state = 0;
			}
		} else if(state == 2) {
			/* fill name_buf ... dummy '{' */
			pos--;
			out_ptr[pos] = 0;
			if(in_char == '}') {
				name_buf[name_len] = 0;

				/* got token, now look it up */
				tmp_ptr = "";
				if(!strncmp("0", name_buf, 128)) {
					/* Replace ${0} with g_argv0_path */
					tmp_ptr = g_argv0_path;
				} else {
					tmp_ptr = getenv(name_buf);
					if(tmp_ptr == 0) {
						tmp_ptr = "";
					}
				}
				pos = cfg_strlcat(out_ptr, tmp_ptr, maxlen);
				state = 0;
			} else {
				name_buf[name_len++] = in_char;
				if(name_len >= 250) {
					name_len--;
				}
			}
		}
	}

	return pos;
}

char *
cfg_exit(int get_status)
{
	/* printf("In cfg exit\n"); */
	if(get_status) {
		return 0;
	}
	cfg_toggle_config_panel();

	return 0;
}

void
cfg_err_vprintf(const char *pre_str, const char *fmt, va_list ap)
{
	char	*bufptr;
	int	pos, len, bufsize;

	pos = g_cfg_err_pos;
	if(pos >= CFG_ERR_MAX) {
		pos = CFG_ERR_MAX - 1;
	}
	bufptr = &g_cfg_err_bufs[pos][0];
	len = 0;
	bufsize = CFG_ERR_BUFSIZE;
	if(pre_str && *pre_str) {
		cfg_strncpy(bufptr, pre_str, CFG_ERR_BUFSIZE);
		cfg_strlcat(bufptr, " error: ", CFG_ERR_BUFSIZE);
		len = (int)strlen(bufptr);
		bufsize = CFG_ERR_BUFSIZE - len;
	}
	if(bufsize > 0) {
		vsnprintf(&bufptr[len], bufsize, fmt, ap);
	}

	fputs(bufptr, stderr);
	g_cfg_err_pos = pos + 1;
}

void
cfg_err_printf(const char *pre_str, const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	cfg_err_vprintf(pre_str, fmt, ap);
	va_end(ap);
}

void
cfg_toggle_config_panel()
{
	int	panel;

	panel = !g_config_control_panel;
	if(g_rom_version < 0) {
		panel = 1;	/* Stay in config mode */
	}
	if(panel != g_config_control_panel) {
		cfg_set_config_panel(panel);
	}
}

void
cfg_set_config_panel(int panel)
{
	int	i;

	g_config_control_panel = panel;
	if(panel) {
		// Entering configuration panel
		video_force_reparse();

		cfg_printf("Entering config_control_panel\n");

		for(i = 0; i < 20; i++) {
			// Toss any queued-up keypresses
			if(adb_read_c000() & 0x80) {
				(void)adb_access_c010();
			}
		}
		// HACK: Force adb keyboard (and probably mouse) to "normal"...

		cfg_home();

		g_menu_line = 1;
		g_menu_inc = 1;
		g_menu_redraw_needed = 1;
		g_cfg_slotdrive = 0;
		g_cfg_newdisk_select = 0;
		g_cfg_select_partition = -1;
	} else {
		// Leave config panel, go back to A2 emulation

		video_force_reparse();
	}
	g_full_refresh_needed = -1;
	g_a2_screen_buffer_changed = -1;
	g_adb_repeat_vbl = g_vbl_count + 60;
}

char *
cfg_text_screen_dump(int get_status)
{
	FILE	*ofile;
	char	*bufptr;
	char	*filename;

	if(get_status) {
		return 0;
	}
	filename = "kegs.screen.dump";
	printf("Writing text screen to the file %s\n", filename);
	ofile = fopen(filename, "w");
	if(ofile == 0) {
		fatal_printf("Could not write to file %s, (%d)\n", filename,
				errno);
		return 0;
	}
	bufptr = cfg_text_screen_str();
	fputs(bufptr, ofile);
	fclose(ofile);
	return 0;
}

char g_text_screen_buf[2100] = { 0 };

char *
cfg_text_screen_str()
{
	char	*bufptr;
	int	pos, start_pos, c, offset;
	int	i, j;

	// bufptr must be at least (81*24)+2 characters
	bufptr = &g_text_screen_buf[0];
	pos = 0;
	for(i = 0; i < 24; i++) {
		start_pos = pos;
		for(j = 0; j < 40; j++) {
			offset = g_screen_index[i] + j;
			if(g_cur_a2_stat & ALL_STAT_VID80) {
				c = g_slow_memory_ptr[0x10400 + offset] & 0x7f;
				if(c < 0x20) {
					c += 0x40;
				}
				bufptr[pos++] = c;
			}
			c = g_slow_memory_ptr[0x0400 + offset] & 0x7f;
			if(c < 0x20) {
				c += 0x40;
			}
			if(c == 0x7f) {
				c = ' ';
			}
			bufptr[pos++] = c;
		}
		while((pos > start_pos) && (bufptr[pos-1] == ' ')) {
			/* try to strip out trailing spaces */
			pos--;
		}
		bufptr[pos++] = '\n';
		bufptr[pos] = 0;
	}

	return bufptr;
}

char *
cfg_get_serial0_status(int get_status)
{
	return scc_get_serial_status(get_status, 0);
}

char *
cfg_get_serial1_status(int get_status)
{
	return scc_get_serial_status(get_status, 1);
}

char *
cfg_get_current_copy_selection()
{
	return &g_text_screen_buf[0];
}

void
config_vbl_update(int doit_3_persec)
{
	if(doit_3_persec) {
		if(g_config_kegs_auto_update && g_config_kegs_update_needed) {
			(void)config_write_config_kegs_file(0);
		}
	}
	return;
}

void
cfg_file_update_rom(const char *str)
{
	cfg_file_update_ptr(&g_cfg_rom_path, str, 1);
}

void
cfg_file_update_ptr(char **strptr, const char *str, int need_update)
{
	char	*newstr;
	int	remote_changed, serial_changed;
	int	i;

	// Update whatever g_cfg_file_strptr points to.  If changing
	//  ROM path or Charrom, then do the proper updates

	newstr = kegs_malloc_str(str);
	if(!strptr) {
		return;
	}
	if(*strptr) {
		free(*strptr);
	}
	*strptr = newstr;
	if(strptr == &(g_cfg_rom_path)) {
		printf("Updated ROM file\n");
		load_roms_init_memory();
		do_reset();
	}
	if(strptr == &(g_cfg_charrom_path)) {
		printf("Updated Char ROM file\n");
		cfg_load_charrom();
	}
	for(i = 0; i < 2; i++) {
		remote_changed = 0;
		serial_changed = 0;
		if(strptr == &g_serial_remote_ip[i]) {
			remote_changed = 1;
		}
		if(strptr == &g_serial_device[i]) {
			serial_changed = 1;
		}
		if(remote_changed || serial_changed) {
			scc_config_changed(i, 0, remote_changed,
								serial_changed);
		}
	}
	if(need_update) {
		g_config_kegs_update_needed = 1;
		printf("Set g_config_kegs_update_needed = 1\n");
	}
}

void
cfg_int_update(int *iptr, int new_val)
{
	int	old_val, cfg_changed, remote_changed, serial_changed;
	int	i;

	// Called to handle an integer being changed in the F4 config menus
	//  where it's value may need special handling

	old_val = *iptr;
	*iptr = new_val;
	if(old_val == new_val) {
		return;
	}
	if(iptr == &g_cfg_charrom_pos) {
		cfg_load_charrom();
	}

	for(i = 0; i < 2; i++) {
		remote_changed = 0;
		serial_changed = 0;
		cfg_changed = 0;
		if(iptr == &g_serial_cfg[i]) {
			cfg_changed = 1;
		}
		if(iptr == &g_serial_remote_port[i]) {
			remote_changed = 1;
		}
		if(iptr == &g_serial_win_device[i]) {
			serial_changed = 1;
		}
		if(cfg_changed || remote_changed || serial_changed) {
			scc_config_changed(i, cfg_changed, remote_changed,
							serial_changed);
		}
	}
}

void
cfg_load_charrom()
{
	byte	buffer[4096];
	dword64	dsize, dret;
	word32	upos;
	int	fd;

	printf("Loading character ROM from: %s\n", g_cfg_charrom_path);
	fd = open(g_cfg_charrom_path, O_RDONLY | O_BINARY);
	if(fd < 0) {
		printf("Cannot open %s\n", g_cfg_charrom_path);
		return;
	}
	dsize = cfg_get_fd_size(fd);
	upos = g_cfg_charrom_pos * 0x1000U;
	if(dsize < (upos + 0x1000)) {
		g_cfg_charrom_pos = 0;
		return;
	}
	dret = cfg_read_from_fd(fd, &buffer[0], upos, 4096);
	if(dret != 0) {
		prepare_a2_romx_font(&buffer[0]);
	}
}

void
config_load_roms()
{
	struct stat stat_buf;
	const char **names_ptr;
	int	more_than_8mb, changed_rom, len, fd, ret;
	int	i;

	g_rom_version = -1;

	/* set first entry of g_kegs_rom_names[] to g_cfg_rom_path so that */
	/*  it becomes the first place searched. */
	g_kegs_rom_names[0] = g_cfg_rom_path;
	ret = config_setup_kegs_file(&g_cfg_tmp_path[0], CFG_PATH_MAX,
							&g_kegs_rom_names[0]);
	if(ret == 0) {
		// Just get out, let config interface select ROM
		g_config_control_panel = 1;
		printf("No ROM, set g_config_control_panel=1\n");
		return;
	}
	printf("Found ROM at path: %s\n", g_cfg_tmp_path);
	fd = open(&g_cfg_tmp_path[0], O_RDONLY | O_BINARY);
	if(fd < 0) {
		fatal_printf("Open ROM file %s failed:%d, errno:%d\n",
				&g_cfg_tmp_path[0], fd, errno);
		g_config_control_panel = 1;
		return;
	}

	ret = fstat(fd, &stat_buf);
	if(ret != 0) {
		fatal_printf("fstat returned %d on fd %d, errno: %d\n",
			ret, fd, errno);
		g_config_control_panel = 1;
		return;
	}

	len = (int)stat_buf.st_size;
	memset(&g_rom_fc_ff_ptr[0], 0, 4*65536);
				/* Clear banks fc-ff to 0 */
	if(len == 32*1024) {		// Apple //e
		g_rom_version = 0;
		g_mem_size_base = 128*1024;
		ret = (int)read(fd, &g_rom_fc_ff_ptr[3*65536 + 32768], len);
	} else if(len == 128*1024) {
		g_rom_version = 1;
		g_mem_size_base = 128*1024;
		ret = (int)read(fd, &g_rom_fc_ff_ptr[2*65536], len);
	} else if(len == 256*1024) {
		g_rom_version = 3;
		g_mem_size_base = 1024*1024;
		ret = (int)read(fd, &g_rom_fc_ff_ptr[0], len);
	} else {
		fatal_printf("The ROM size should be 128K or 256K, this file "
						"is %d bytes\n", len);
		g_config_control_panel = 1;
		return;
	}

	printf("Read: %d bytes of ROM\n", ret);
	if(ret != len) {
		fatal_printf("errno: %d\n", errno);
		g_config_control_panel = 1;
		return;
	}
	close(fd);

	memset(&g_rom_cards_ptr[0], 0, 256*16);

	for(i = 0; i < 256; i++) {
		// Place HD PROM in slot 7
		g_rom_cards_ptr[0x700 + i] = g_rom_c700[i];
		// g_rom_cards_ptr[0x500 + i] = g_rom_c700[i];
	}
	/* initialize c600 rom to be diffs from the real ROM, to build-in */
	/*  Apple II compatibility without distributing ROMs */
	if(g_rom_version == 0) {			// Apple //e
		for(i = 0; i < 256; i++) {
			// Place Disk II PROM in slot 6
			g_rom_cards_ptr[0x600 + i] = g_rom_fc_ff_ptr[0x38600+i];
		}
	} else {
		for(i = 0; i < 256; i++) {
			g_rom_cards_ptr[0x600 + i] =
					g_rom_fc_ff_ptr[0x3c600 + i] ^
					g_rom_c600_rom01_diffs[i];
		}
	}
	if(g_rom_version >= 3) {
		/* some patches */
		g_rom_cards_ptr[0x61b] ^= 0x40;
		g_rom_cards_ptr[0x61c] ^= 0x33;
		g_rom_cards_ptr[0x632] ^= 0xc0;
		g_rom_cards_ptr[0x633] ^= 0x33;
	}

	for(i = 1; i < 8; i++) {
		names_ptr = g_kegs_rom_card_list[i];
		if(names_ptr == 0) {
			continue;
		}
		if(*names_ptr == 0) {
			continue;
		}

		ret = config_setup_kegs_file(&g_cfg_tmp_path[0], CFG_PATH_MAX,
								names_ptr);

		if(ret != 0) {
			fd = open(&(g_cfg_tmp_path[0]), O_RDONLY | O_BINARY);
			if(fd < 0) {
				fatal_printf("Open card ROM file %s failed: %d "
					"err:%d\n", &g_cfg_tmp_path[0], fd,
					errno);
				continue;
			}

			len = 256;
			ret = (int)read(fd, &g_rom_cards_ptr[i*0x100], len);

			if(ret != len) {
				fatal_printf("While reading card ROM %s, file "
					"is too short. (%d) Expected %d bytes, "
					"read %d bytes\n", errno, len, ret);
				continue;
			}
			close(fd);
		}
	}

	more_than_8mb = (g_mem_size_exp > 0x800000);
	/* Only do the patch if users wants more than 8MB of expansion mem */

	changed_rom = 0;
	if(g_rom_version == 1) {
		/* make some patches to ROM 01 */
#if 0
		/* 1: Patch ROM selftest to not do speed test */
		printf("Patching out speed test failures from ROM 01\n");
		g_rom_fc_ff_ptr[0x3785a] = 0x18;
		changed_rom = 1;
#endif

#if 0
		/* 2: Patch ROM selftests not to do tests 2,4 */
		/* 0 = skip, 1 = do it, test 1 is bit 0 of LSByte */
		g_rom_fc_ff_ptr[0x371e9] = 0xf5;
		g_rom_fc_ff_ptr[0x371ea] = 0xff;
		changed_rom = 1;
#endif

		if(more_than_8mb) {
			/* Geoff Weiss patch to use up to 14MB of RAM */
			g_rom_fc_ff_ptr[0x30302] = 0xdf;
			g_rom_fc_ff_ptr[0x30314] = 0xdf;
			g_rom_fc_ff_ptr[0x3031c] = 0x00;
			changed_rom = 1;
		}

		/* Patch ROM selftest to not do ROM cksum if any changes*/
		if(changed_rom) {
			g_rom_fc_ff_ptr[0x37a06] = 0x18;
			g_rom_fc_ff_ptr[0x37a07] = 0x18;
		}
	} else if(g_rom_version == 3) {
		/* patch ROM 03 */
		printf("Patching ROM 03 smartport bug\n");
		/* 1: Patch Smartport code to fix a stupid bug */
		/*  that causes it to write the IWM status reg into c036, */
		/*  which is the system speed reg...it's "safe" since */
		/*  IWM status reg bit 4 must be 0 (7MHz)..., otherwise */
		/*  it might have turned on shadowing in all banks! */
		g_rom_fc_ff_ptr[0x357c9] = 0x00;
		changed_rom = 1;

#if 0
		/* patch ROM 03 to not to speed test */
		/*  skip fast speed test */
		g_rom_fc_ff_ptr[0x36ad7] = 0x18;
		g_rom_fc_ff_ptr[0x36ad8] = 0x18;
		changed_rom = 1;
#endif

#if 0
		/*  skip slow speed test */
		g_rom_fc_ff_ptr[0x36ae7] = 0x18;
		g_rom_fc_ff_ptr[0x36ae8] = 0x6b;
		changed_rom = 1;
#endif

#if 0
		/* 4: Patch ROM 03 selftests not to do tests 1-4 */
		g_rom_fc_ff_ptr[0x364a9] = 0xf0;
		g_rom_fc_ff_ptr[0x364aa] = 0xff;
		changed_rom = 1;
#endif

		/* ROM tests are in ff/6403-642x, where 6403 = addr of */
		/*  test 1, etc. */

		if(more_than_8mb) {
			/* Geoff Weiss patch to use up to 14MB of RAM */
			g_rom_fc_ff_ptr[0x30b] = 0xdf;
			g_rom_fc_ff_ptr[0x31d] = 0xdf;
			g_rom_fc_ff_ptr[0x325] = 0x00;
			changed_rom = 1;
		}

		if(changed_rom) {
			/* patch ROM 03 selftest to not do ROM cksum */
			g_rom_fc_ff_ptr[0x36cb0] = 0x18;
			g_rom_fc_ff_ptr[0x36cb1] = 0x18;
		}

	}
}

void
config_parse_config_kegs_file()
{
	char	*bufptr;
	const char *str;
	dword64	dsize;
	int	fd, pos, start, size, last_c, line, ret, c, maxlen;
	int	i;

	printf("Parsing config.kegs file: %s\n", g_config_kegs_name);

	clk_bram_zero();

	g_highest_smartport_unit = -1;

	cfg_get_base_path(&g_cfg_cwd_str[0], g_config_kegs_name, 0);
	if(g_cfg_cwd_str[0] != 0) {
		ret = chdir(&g_cfg_cwd_str[0]);
		if(ret != 0) {
			printf("chdir to %s, errno:%d\n", g_cfg_cwd_str, errno);
		}
		// Do basename(g_config_kegs_name)--on it's own buffer
		str = cfg_str_basename(g_config_kegs_name);
		maxlen = sizeof(g_config_kegs_name);
		cfg_strncpy(&g_config_kegs_name[0], str, maxlen);
	}

	// In any case, copy the current directory path to g_cfg_cwd_str
	(void)!getcwd(&g_cfg_cwd_str[0], CFG_PATH_MAX);
	printf("CWD is now: %s\n", &g_cfg_cwd_str[0]);

	fd = open(g_config_kegs_name, O_RDONLY | O_BINARY);
	dsize = 0;
	if(fd >= 0) {
		dsize = cfg_get_fd_size(fd);
	}
	if((fd < 0) || (dsize >= (1 << 30))) {
		fatal_printf("cannot open config.kegs at %s, or it is too "
			"large!  Stopping!\n", g_config_kegs_name);
		my_exit(3);
		return;
	}
	size = (int)dsize;
	bufptr = malloc(size + 2);
	ret = (int)cfg_read_from_fd(fd, (byte *)bufptr, 0, size);
	close(fd);
	if(ret != size) {
		free(bufptr);
		fatal_printf("Could not read config.kegs at %s\n",
							g_config_kegs_name);
		my_exit(3);
		return;
	}
	bufptr[size] = 0;		// Ensure it's null terminated

	line = 0;
	pos = 0;
	last_c = 0;
	while(pos < size) {
		line++;
		if((bufptr[pos] == '\n') && (last_c == '\r')) {
			// CR,LF, just eat the LF
			pos++;
		}
		start = pos;
		while(pos < size) {
			c = bufptr[pos];
			if((c == 0) || (c == '\n') || (c == '\r')) {
				last_c = c;
				bufptr[pos] = 0;
				break;
			}
			pos++;
		}
		cfg_parse_one_line(&bufptr[start], line);
		pos++;
	}

	free(bufptr);

	// And now do command line argument overrides
	for(i = 0; i < g_cfg_argv_num_overrides; i++) {
		printf("Doing override %d, %s\n", i, g_cfg_argv_overrides[i]);
		cfg_parse_one_line(g_cfg_argv_overrides[i], 1001 + i);
		g_config_kegs_update_needed = 1;
	}
}

void
cfg_parse_one_line(char *buf, int line)
{
	Cfg_menu *menuptr;
	Cfg_defval *defptr;
	int	*iptr;
	const char *nameptr;
	int	pos, num_equals, type, val, c, len;
	int	i;

	// warning: modifies memory of bufptr (turns spaces to nulls)
	if(line) {		// Avoid unused parameter warning
	}

	len = (int)strlen(buf);
	if(len <= 1) {		// Not a valid line, just get out
		return;
	}

	// printf("disk_conf[%d]: %s\n", line, buf);
	if(buf[0] == '#') {
		iwm_printf("Skipping comment\n");
		return;
	}

	pos = 0;

	while((pos < len) && (buf[pos] == ' ' || buf[pos] == '\t') ) {
		pos++;		// Eat whitespace
	}
	if(pos >= len) {
		return;		// blank line
	}
	if((pos + 5) < len) {
		c = buf[pos+1];		// Slot number
		if((buf[pos] == 's') && (buf[pos+2] == 'd') &&
					(c >= '5' && c <= '7')) {
			// looks like s5d1 through s7d15, parse that
			cfg_parse_sxdx(buf, pos, len);
			return;
		}
	}

// parse buf from pos into option, "=" and then "rest"

	if(strncmp(&buf[pos], "bram", 4) == 0) {
		cfg_parse_bram(buf, pos+4, len);
		return;
	}

	// find "name" as first contiguous string
	//printf("...parse_option: line %d, %s (%s) len:%d\n", line, buf,
	//						&buf[pos], len);

	nameptr = &buf[pos];
	while(pos < len) {
		c = buf[pos];
		if((c == 0) || (c == ' ') || (c == '\t') || (c == '\n') ||
								(c == '=')) {
			break;
		}
		pos++;
	}
	buf[pos] = 0;
	if(strcmp(nameptr, "rom") == 0) {
		// Translate argument of "-rom" to "g_cfg_rom_path"
		nameptr = "g_cfg_rom_path";
	}
	pos++;

	// Eat up all whitespace and '='
	num_equals = 0;
	while(pos < len) {
		c = buf[pos];
		if((c == '=') && (num_equals == 0)) {
			pos++;
			num_equals++;
		} else if(c == ' ' || c == '\t') {
			pos++;
		} else {
			break;
		}
	}

	/* Look up nameptr to find type */
	type = -1;
	defptr = 0;
	menuptr = 0;
	for(i = 0; i < g_cfg_defval_index; i++) {
		defptr = &(g_cfg_defvals[i]);
		menuptr = defptr->menuptr;
		if(strcmp(menuptr->name_str, nameptr) == 0) {
			type = menuptr->cfgtype;
			break;
		}
	}

	switch(type) {
	case CFGTYPE_INT:
		/* use strtol */
		val = (int)strtol(&buf[pos], 0, 0);
		iptr = (int *)menuptr->ptr;
		cfg_int_update(iptr, val);
		break;
	case CFGTYPE_FILE:
	case CFGTYPE_STR:
		cfg_file_update_ptr(menuptr->ptr, &buf[pos], 0);
		break;
	default:
		printf("Config file variable %s is unknown type: %d\n",
			nameptr, type);
	}
}

void
cfg_parse_bram(char *buf, int pos, int len)
{
	word32	val;
	int	bram_num, offset;

	// Format: "bram1[00] = xx yy..." or "bram3[00] = xx yy ..."
	// pos = position just after "bram"
	if((len < (pos+5)) || (buf[pos+1] != '[') || (buf[pos+4] != ']')) {
		fatal_printf("While reading config.kegs, found malformed bram "
			"statement: %s\n", buf);
		return;
	}
	bram_num = buf[pos] - '0';
	if((bram_num != 1) && (bram_num != 3)) {
		fatal_printf("While reading config.kegs, found bad bram "
			"num: %s\n", buf);
		return;
	}

	bram_num = bram_num >> 1;	// turn 3->1 and 1->0

	offset = (int)strtoul(&(buf[pos+2]), 0, 16);
	pos += 5;
	while(pos < len) {
		if((buf[pos] == ' ') || (buf[pos] == '\t') ||
				(buf[pos] == 0x0a) || (buf[pos] == 0x0d) ||
				(buf[pos] == '=')) {
			pos++;
			continue;
		}
		val = (word32)strtoul(&buf[pos], 0, 16);	// As hex
		clk_bram_set(bram_num, offset, val);
		offset++;
		pos += 2;
	}
}

void
cfg_parse_sxdx(char *buf, int pos, int len)
{
	char	*name_ptr, *partition_name;
	word32	dynamic_blocks;
	int	part_num, ejected, slot, drive, c;

	slot = buf[pos+1] - '0';
	drive = buf[pos+3] - '0';

	/* skip over slot, drive */
	pos += 4;
	if((buf[pos] >= '0') && (buf[pos] <= '9')) {
		// Second digit of drive is valid
		drive = (drive * 10) + buf[pos] - '0';
		pos++;
	}

	drive--;	// make sxd1 mean index 0

	while((pos < len) && ((buf[pos] == ' ') || (buf[pos] == '\t') ||
							(buf[pos] == '=')) ) {
		pos++;
	}

	ejected = 0;
	if(buf[pos] == '#') {	// disk is ejected, but read info anyway
		ejected = 1;
		pos++;
	}

	partition_name = 0;
	part_num = -1;
	dynamic_blocks = 0;
	if(buf[pos] == ':') {		// yup, it's got a partition name!
		pos++;
		partition_name = &buf[pos];
		while((pos < len) && (buf[pos] != ':')) {
			pos++;
		}
		buf[pos] = 0;	/* null terminate partition name */
		pos++;
	}
	c = buf[pos];
	if((c == ';') || (c == '@')) {
		pos++;
		// ; - partition number;  @ - Dynamic ProDOS dir size
		part_num = 0;
		while((pos < len) && (buf[pos] != ':')) {
			part_num = (10*part_num) + buf[pos] - '0';
			pos++;
		}
		pos++;
		if(c == '@') {		// Dynamic ProDOS directory
			dynamic_blocks = part_num * 2;
			part_num = -1;
		}
	}

	/* Get filename */
	name_ptr = &(buf[pos]);
	if((pos >= len) || (name_ptr[0] == 0)) {
		return;
	}

	insert_disk(slot, drive, name_ptr, ejected, partition_name,
						part_num, dynamic_blocks);
}

void
config_generate_config_kegs_name(char *outstr, int maxlen, Disk *dsk,
		int with_extras)
{
	char	*str;

	str = outstr;

	if(with_extras) {
		if(dsk->fd < 0) {
			snprintf(str, maxlen - (str - outstr), "#");
			str = &outstr[strlen(outstr)];
		}
		if(dsk->dynapro_blocks) {
			snprintf(str, maxlen - (str - outstr), "@%d:",
					(dsk->dynapro_blocks + 1) / 2);
			str = &outstr[strlen(outstr)];
		} else if(dsk->partition_name != 0) {
			snprintf(str, maxlen - (str - outstr), ":%s:",
							dsk->partition_name);
			str = &outstr[strlen(outstr)];
		} else if(dsk->partition_num >= 0) {
			snprintf(str, maxlen - (str - outstr), ";%d:",
							dsk->partition_num);
			str = &outstr[strlen(outstr)];
		}
	}
	snprintf(str, maxlen - (str - outstr), "%s", dsk->name_ptr);
}

char *
config_write_config_kegs_file(int get_status)
{
	FILE	*fconf;
	Disk	*dsk;
	Cfg_defval *defptr;
	Cfg_menu *menuptr;
	char	*curstr, *defstr;
	int	defval, curval, type, slot, drive;
	int	i;

	if(get_status) {
		return 0;
	}
	printf("Writing config.kegs file to %s\n", g_config_kegs_name);

	fconf = fopen(g_config_kegs_name, "w+");
	if(fconf == 0) {
		halt_printf("cannot open %s!  Stopping!\n", g_config_kegs_name);
		return 0;
	}

	fprintf(fconf, "# KEGS configuration file version %s\n",
						g_kegs_version_str);

	for(i = 0; i < MAX_C7_DISKS + 4; i++) {
		slot = 7;
		drive = i - 4;
		if(i < 4) {
			slot = (i >> 1) + 5;
			drive = i & 1;
		}
		if(drive == 0) {
			fprintf(fconf, "\n");	/* an extra blank line */
		}

		dsk = iwm_get_dsk_from_slot_drive(slot, drive);
		if(dsk->name_ptr == 0 && (i > 4)) {
			/* No disk, not even ejected--just skip */
			continue;
		}
		fprintf(fconf, "s%dd%d = ", slot, drive + 1);
		if(dsk->name_ptr == 0) {
			fprintf(fconf, "\n");
			continue;
		}
		config_generate_config_kegs_name(&g_cfg_tmp_path[0],
							CFG_PATH_MAX, dsk, 1);
		fprintf(fconf, "%s\n", &g_cfg_tmp_path[0]);
	}

	fprintf(fconf, "\n");

	/* See if any variables are different than their default */
	for(i = 0; i < g_cfg_defval_index; i++) {
		defptr = &(g_cfg_defvals[i]);
		menuptr = defptr->menuptr;
		defval = defptr->intval;
		type = menuptr->cfgtype;
		if(type == CFGTYPE_INT) {
			curval = *((int *)menuptr->ptr);
			if(curval != defval) {
				fprintf(fconf, "%s = %d\n", menuptr->name_str,
								curval);
			}
		}
		if((type == CFGTYPE_FILE) || (type == CFGTYPE_STR)) {
			curstr = *((char **)menuptr->ptr);
			defstr = *((char **)menuptr->defptr);
			if(strcmp(curstr, defstr) != 0) {
				fprintf(fconf, "%s = %s\n", menuptr->name_str,
								curstr);
			}
		}
	}

	fprintf(fconf, "\n");

	/* write bram state */
	clk_write_bram(fconf);

	fclose(fconf);

	g_config_kegs_update_needed = 0;
	return 0;
}

void
insert_disk(int slot, int drive, const char *name, int ejected,
		const char *partition_name, int part_num, word32 dynamic_blocks)
{
	byte	buf_2img[512];
	Disk	*dsk;
	char	*name_ptr, *part_ptr;
	dword64	dsize, dunix_pos, exp_dsize, dtmp;
	word32	len_bits, save_ftrack;
	int	image_type, part_len, ret, locked, len, is_po, name_len;
	int	i;

	g_config_kegs_update_needed = 1;

	if((slot < 5) || (slot > 7)) {
		fatal_printf("Invalid slot for insertiing disk: %d\n", slot);
		return;
	}
	if(drive < 0 || ((slot == 7) && (drive >= MAX_C7_DISKS)) ||
					((slot < 7) && (drive > 1))) {
		fatal_printf("Invalid drive for inserting disk: %d\n", drive);
		return;
	}

	dsk = iwm_get_dsk_from_slot_drive(slot, drive);

#if 1
	printf("Inserting disk %s (%s or %d) in slot %d, drive: %d, "
		"dyna_blocks:%d\n", name, partition_name, part_num, slot, drive,
		dynamic_blocks);
#endif

	// DO NOT change dsk->just_ejected.  If a disk was just ejected, then
	//  leave it alone.  Otherwise, if we are a newly inserted disk,
	//  it should already be 0, so leave it alone
	//dsk->just_ejected = 1;

	if(dsk->fd >= 0) {
		iwm_eject_disk(dsk);
	}

	/* Before opening, make sure no other mounted disk has this name */
	/* If so, unmount it */
	if(!ejected) {
		for(i = 0; i < 2; i++) {
			iwm_eject_named_disk(5, i, name, partition_name);
			iwm_eject_named_disk(6, i, name, partition_name);
		}
		for(i = 0; i < MAX_C7_DISKS; i++) {
			iwm_eject_named_disk(7, i, name, partition_name);
		}
	}

	/* free old name_ptr, partition_name */
	free(dsk->name_ptr);
	free(dsk->partition_name);
	dsk->name_ptr = 0;
	dsk->partition_name = 0;

	name_ptr = kegs_malloc_str(name);
	dsk->name_ptr = name_ptr;
	name_len = (int)strlen(dsk->name_ptr);

	part_len = 0;
	part_ptr = 0;
	if(partition_name != 0) {
		part_ptr = kegs_malloc_str(partition_name);
		part_len = (int)strlen(part_ptr);
		dsk->partition_name = part_ptr;
	}
	dsk->partition_num = part_num;
	dsk->dynapro_blocks = dynamic_blocks;

	iwm_printf("Opening up disk image named: %s\n", name_ptr);

	if(ejected) {
		/* just get out of here */
		dsk->fd = -1;
		return;
	}

	dsk->fd = -1;
	dsk->raw_data = 0;
	dsk->image_type = 0;
	dsk->dimage_start = 0;
	dsk->dimage_size = 0;
	dsk->write_prot = 0;
	dsk->write_through_to_unix = 1;
	image_type = 0;
	locked = 0;

	if(dynamic_blocks) {
		ret = dynapro_mount(dsk, name_ptr, dynamic_blocks);
		if(ret < 0) {
			iwm_eject_disk(dsk);
			return;
		}
		image_type = DSK_TYPE_DYNAPRO;
		printf("After dynapro_mount, write_through:%d\n",
						dsk->write_through_to_unix);
	}
	if((partition_name != 0) || (part_num >= 0)) {
		ret = cfg_partition_find_by_name_or_num(dsk, partition_name,
								part_num);
		printf("partition %s (num %d) mounted, wr_prot: %d, ret:%d\n",
				partition_name, part_num, dsk->write_prot, ret);

		if(ret < 0) {
			iwm_eject_disk(dsk);
			return;
		}
		locked = dsk->write_prot;
		if(dsk->raw_data) {
			// .zip file or something similar.  Do name matching on
			//  partition name
			name_len = part_len;
			name_ptr = part_ptr;
			locked = 1;
		}
	}

	if((name_len > 3) && !image_type &&
				!cfgcasecmp(".gz", &name_ptr[name_len - 3])) {
		// it's gzip'ed, try to gunzip it to dsk->raw_data
		undeflate_gzip(dsk, name_ptr);

		locked = 1;
		dsk->dimage_start = 0;
		dsk->dimage_size = dsk->raw_dsize;
		name_len -= 3;		// So .dsk, .po look for correct chars
	}

	if((name_len > 4) && !image_type &&
				!cfgcasecmp(".bz2", &name_ptr[name_len - 4])) {
		// it's bzip2'ed, try to bunzip2 it to dsk->raw_data
		config_file_to_pipe(dsk, "bunzip2", name_ptr);
		locked = 1;

		// Reduce name_len by 4 so that subsequent compares for .po
		//  look at the correct chars
		name_len -= 4;
	}

	if((name_len > 4) && !image_type &&
				!cfgcasecmp(&name_ptr[name_len - 4], ".sdk")) {
		// it's a ShrinkIt archive with a disk image in it
		unshk(dsk, name_ptr);
		locked = 1;
		image_type = DSK_TYPE_PRODOS;
		printf("dsk->fd:%d dsk->raw_data:%p, raw_dsize:%lld\n", dsk->fd,
						dsk->raw_data, dsk->raw_dsize);
		dsk->dimage_start = 0;
		dsk->dimage_size = dsk->raw_dsize;
	}

	if((name_len > 4) && !image_type && dsk->disk_525 &&
				!cfgcasecmp(".nib", &name_ptr[name_len-4])) {
		// Old, obsolete .nib 5.25" nibblized format.  Support is
		//  read-only
		image_type = DSK_TYPE_NIB;
		locked = 1;
	}

	if((dsk->fd < 0) && !locked && !dynamic_blocks) {
		dsk->fd = open(name_ptr, O_RDWR | O_BINARY, 0x1b6);
	}

	if((dsk->fd < 0) && !dynamic_blocks) {
		printf("Trying to open %s read-only, errno: %d\n", name_ptr,
								errno);
		dsk->fd = open(name_ptr, O_RDONLY | O_BINARY, 0x1b6);
		locked = 2;
	}

	iwm_printf("open returned: %d\n", dsk->fd);

	if((dsk->fd < 0) && !dynamic_blocks) {
		fatal_printf("Disk image %s does not exist!\n", name_ptr);
		free(dsk->raw_data);
		return;
	}

	//printf("Checking if it's woz, name_ptr:%s. %d vs %d\n", name_ptr,
	//	name_len, (int)strlen(name_ptr));
	if((name_len > 4) && !image_type &&
				!cfgcasecmp(&name_ptr[name_len - 4], ".woz")) {
		// it's a WOZ applesauce disk image
		image_type = DSK_TYPE_WOZ;
		printf("It is woz!\n");
	}

	if(locked) {
		dsk->write_prot = locked;
	}

	save_ftrack = dsk->cur_frac_track;	/* save arm position */

	/* See if it is in 2IMG format */
	if(dsk->raw_data) {
		// Just do a copy from raw_data
		for(i = 0; i < 512; i++) {
			buf_2img[i] = dsk->raw_data[i];
		}
		dsize = dsk->raw_dsize;
		if(!dsk->dynapro_info_ptr) {
			dsk->write_through_to_unix = 0;
		}
	} else {
		ret = (int)read(dsk->fd, (char *)&buf_2img[0], 512);
		dsize = cfg_get_fd_size(dsk->fd);
	}

#if 0
	/* Try to guess that there is a Mac Binary header of 128 bytes */
	/* See if image size & 0xfff = 0x080 which indicates extra 128 bytes */
	if(((dsize & 0xfff) == 0x080) && (dsize < (801*1024)) && !image_type) {
		printf("Assuming Mac Binary header on %s\n", dsk->name_ptr);
		dsk->dimage_start += 0x80;
		dsize -= 0x80;
	}
#endif

	dsk->dimage_size = dsize;

	if(!image_type && (buf_2img[0] == '2') && (buf_2img[1] == 'I') &&
				(buf_2img[2] == 'M') && (buf_2img[3] == 'G')) {
		/* It's a 2IMG disk */
		printf("Image named %s is in 2IMG format\n", dsk->name_ptr);
		image_type = DSK_TYPE_PRODOS;

		if(buf_2img[12] == 0) {
			printf("2IMG is in DOS 3.3 sector order\n");
			image_type = DSK_TYPE_DOS33;
		}
		if(buf_2img[19] & 0x80) {
			/* disk is locked */
			printf("2IMG is write protected\n");
			if(dsk->write_prot == 0) {
				dsk->write_prot = 1;
			}
		}
		if((buf_2img[17] & 1) && (dsk->image_type == DSK_TYPE_DOS33)) {
			dsk->vol_num = buf_2img[16];
			printf("Setting DOS 3.3 vol num to %d\n", dsk->vol_num);
		}
		//	Some 2IMG archives have the size byte reversed
		dsize = (buf_2img[31] << 24) + (buf_2img[30] << 16) +
				(buf_2img[29] << 8) + buf_2img[28];
		dunix_pos = (buf_2img[27] << 24) + (buf_2img[26] << 16) +
				(buf_2img[25] << 8) + buf_2img[24];
		if(dsize == 0x800c00) {
			//	Byte reversed 0x0c8000
			dsize = 0x0c8000;
		}
		if(dsize == 0) {
			/* Sweet-16 makes some images with size == 0 */
			/* Example: Prosel from */
			/*  www.whatisthe2gs.apple2.org.za/the_ring/ */
			if(buf_2img[12] == 1) {
				/* then get the size from 0x14 in blocks */
				dsize = (buf_2img[23] << 24) +
					(buf_2img[22] << 16) +
					(buf_2img[21] << 8) + buf_2img[20];
				dsize = dsize * 512;	/* it was blocks */
			}
		}
		dsk->dimage_start = dunix_pos;
		dsk->dimage_size = dsize;
	}
	exp_dsize = 800*1024;
	dsk->fbit_mult = 256;
	if(dsk->disk_525) {
		exp_dsize = 140*1024;
		dsk->fbit_mult = 128;
	}
	if(!image_type) {
		/* See if it might be the Mac diskcopy format */
		dtmp = cfg_detect_dc42(&buf_2img[0], dsize, exp_dsize, slot);
		if(dtmp != 0) {
			// It's diskcopy 4.2
			printf("Image named %s is in Mac diskcopy format\n",
								dsk->name_ptr);
			dsk->dimage_start += 0x54;
			dsk->dimage_size = dtmp;
			image_type = DSK_TYPE_PRODOS;	/* ProDOS */
		}
	}
	if(!image_type) {
		/* Assume raw image */
		dsk->dimage_size = dsize;
		image_type = DSK_TYPE_PRODOS;
		is_po = (name_len > 3) &&
				!cfgcasecmp(".po", &name_ptr[name_len-3]);
		if(dsk->disk_525) {
			image_type = DSK_TYPE_DOS33;
			if(is_po) {
				image_type = DSK_TYPE_PRODOS;
			}
		}
	}

	dsk->image_type = image_type;
	dsk->disk_dirty = 0;
	dsk->cur_fbit_pos = 0;
	dsk->cur_track_bits = 0;
	dsk->cur_trk_ptr = 0;

	if(image_type == DSK_TYPE_WOZ) {
		// Special handling
		ret = woz_open(dsk, 0);
		if(!ret) {
			iwm_eject_disk(dsk);
			return;
		}
	} else if(dsk->smartport) {
		g_highest_smartport_unit = MY_MAX(dsk->drive,
						g_highest_smartport_unit);

		iwm_printf("adding smartport device[%d], img_sz:%08llx\n",
			dsk->drive, dsk->dimage_size);
	} else if(dsk->disk_525) {
		dunix_pos = dsk->dimage_start;
		dsize = dsk->dimage_size;
		disk_set_num_tracks(dsk, 4*35);
		len = 0x1000;
		if(dsk->image_type == DSK_TYPE_NIB) {
			// Weird .nib format, has no sync bits
			len = (int)(dsk->dimage_size / 35);
			for(i = 0; i < 35; i++) {
				disk_unix_to_nib(dsk, 4*i, dunix_pos, len,
								len * 8, 0);
				dunix_pos += len;
			}
		} else {
			for(i = 0; i < 35; i++) {
				len_bits = iwm_get_default_track_bits(dsk, 4*i);
				disk_unix_to_nib(dsk, 4*i, dunix_pos, len,
								len_bits, 0);
				dunix_pos += len;
			}
		}
		if(dsize != (dword64)35*len) {
			fatal_printf("Disk 5.25 error: size is %lld, not %d.  "
				"Will try to mount anyway\n", dsize, 35*len);
		}
	} else {
		/* disk_35 */
		dunix_pos = dsk->dimage_start;
		dsize = dsk->dimage_size;
		if(dsize != 800*1024) {
			printf("Disk 3.5 Drive %d (Image File: %s), Error: "
				"size is %lld, not 800K.  Will try to mount "
				"anyway\n", drive+1, name, dsize);
		}
		disk_set_num_tracks(dsk, 2*80);
		for(i = 0; i < 2*80; i++) {
			len = g_track_bytes_35[i >> 5];
			len_bits = iwm_get_default_track_bits(dsk, i);
			iwm_printf("Trk: %d.%d = unix: %08llx, %04x, %04x\n",
				i>>1, i & 1, dunix_pos, len, len_bits);
			disk_unix_to_nib(dsk, i, dunix_pos, len, len_bits, 0);
			dunix_pos += len;

			iwm_printf(" trk_bits:%05x\n", dsk->trks[i].track_bits);
		}
	}

	iwm_move_to_ftrack(dsk, save_ftrack, 0, 0);
}

dword64
cfg_detect_dc42(byte *bptr, dword64 dsize, dword64 exp_dsize, int slot)
{
	dword64	ddata_size, dtag_size;
	int	i;

	// Detect Mac DiskCopy4.2 disk image (often .dmg or .dc or .dc42)
	// bptr points to just the first 512 bytes of the file

	if((bptr[0x52] != 1) || (bptr[0x53] != 0)) {
		return 0;	// No "magic number 0x01, 0x00 for DiskCopy4.2
	}
	if(bptr[0] > 0x3f) {
		return 0;	// not a valid image name length (1-0x3f)
	}
	ddata_size = 0;
	dtag_size = 0;
	for(i = 0; i < 4; i++) {
		ddata_size = (ddata_size << 8) | bptr[0x40 + i];
		dtag_size = (dtag_size << 8) | bptr[0x44 + i];
	}
	if((dtag_size != 0) && (dtag_size != ((ddata_size >> 9) * 12))) {
		return 0;	// Tags are either 0, or 12 bytes per block
	}
	if(slot == 7) {
		if(ddata_size < 140*1024) {
			return 0;		// Allow any size >=140K slot 7
		}
	} else if(ddata_size != exp_dsize) {
		return 0;			// Must be 140K or 800K
	}

	if(ddata_size > (dsize - 0x54)) {
		return 0;	// data_size doesn't make sense
	}
	if((ddata_size & 0x1ff) || ((ddata_size >> 31) > 1)) {
		return 0;	// data_size not a multiple of 512 bytes
	}
	if((ddata_size + dtag_size + 0x54) != dsize) {
		return 0;	// File doesn't appear to be well-formed
	}

	return ddata_size;
}

dword64
cfg_get_fd_size(int fd)
{
	struct stat stat_buf;
	int	ret;

	ret = fstat(fd, &stat_buf);
	if(ret != 0) {
		fprintf(stderr,"fstat returned %d on fd %d, errno: %d\n",
			ret, fd, errno);
		stat_buf.st_size = 0;
	}

	return stat_buf.st_size;
}

dword64
cfg_read_from_fd(int fd, byte *bufptr, dword64 dpos, dword64 dsize)
{
	dword64	dret, doff;
	word32	this_len;

	dret = kegs_lseek(fd, dpos, SEEK_SET);
	if(dret != dpos) {
		printf("lseek failed: %lld\n", dret);
		return 0;
	}
	doff = 0;
	while(1) {
		if(doff >= dsize) {
			break;
		}
		this_len = 1UL << 30;
		if((dsize - doff) < this_len) {
			this_len = (word32)(dsize - doff);
		}
		dret = read(fd, bufptr + doff, this_len);
		if((dret + 1) == 0) {		// dret==-1
			printf("read failed\n");
			return 0;
		}
		if(dret == 0) {
			printf("Unexpected end of file, tried to read from "
				"dpos:%lld dsize:%lld\n", dpos, dsize);
			return 0;
		}
		doff += dret;
	}
	return doff;
}

dword64
cfg_write_to_fd(int fd, byte *bufptr, dword64 dpos, dword64 dsize)
{
	dword64	dret;

	dret = kegs_lseek(fd, dpos, SEEK_SET);
	if(dret != dpos) {
		printf("lseek failed: %lld\n", dret);
		return 0;
	}
	return must_write(fd, bufptr, dsize);
}

int
cfg_partition_maybe_add_dotdot()
{
	int	part_len;

	part_len = (int)strlen(&(g_cfg_part_path[0]));
	if(part_len > 0) {
		// Add .. entry here
		cfg_file_add_dirent(&g_cfg_partitionlist, "..", 1, 0, 0, 0, 0);
	}
	return part_len;
}

int
cfg_partition_name_check(const byte *name_ptr, int name_len)
{
	int	part_len;
	int	i;

	// Return 0 if name_ptr is not at the right path depth, 1 if OK

	part_len = (int)strlen(&g_cfg_part_path[0]);
	for(i = 0; i < part_len; i++) {
		if(i >= name_len) {
			return 0;
		}
		if(name_ptr[i] != g_cfg_part_path[i]) {
			return 0;
		}
	}
	return 1;
}

int
cfg_partition_read_block(int fd, void *buf, dword64 blk, int blk_size)
{
	if(!cfg_read_from_fd(fd, buf, blk * blk_size, blk_size)) {
		// Read failed
		return 0;
	}
	return blk_size;
}

int
cfg_partition_find_by_name_or_num(Disk *dsk, const char *in_partnamestr,
								int part_num)
{
	Cfg_dirent *direntptr;
	const char *partnamestr;
	int	match, num_parts, ret, fd, len, c;
	int	i;

#if 0
	printf("cfg_partition_find_by_name_or_num: %s, part_num:%d\n",
							partnamestr, part_num);
#endif

	// We need to copy partnamestr up to the last / to g_cfg_part_path[],
	//  and use just the end as the partition name.
	cfg_strncpy(&g_cfg_part_path[0], in_partnamestr, CFG_PATH_MAX);
	len = (int)strlen(in_partnamestr);
	partnamestr = in_partnamestr;
	for(i = len - 1; i >= 0; i--) {
		c = g_cfg_part_path[i];
		if(c == '/') {
			partnamestr = &(in_partnamestr[i+1]);
			break;
		}
		g_cfg_part_path[i] = 0;
	}

	fd = open(dsk->name_ptr, O_RDONLY | O_BINARY, 0x1b6);
	if(fd < 0) {
		fatal_printf("Cannot open disk image: %s\n", dsk->name_ptr);
		return -1;
	}

	num_parts = cfg_partition_make_list(fd);

	if(num_parts <= 0) {
		printf("num_parts: %d\n", num_parts);
		close(fd);
		return -1;
	}

	for(i = 0; i < g_cfg_partitionlist.last; i++) {
		direntptr = &(g_cfg_partitionlist.direntptr[i]);
		match = 0;
		if((strcmp(partnamestr, direntptr->name) == 0) &&
							(part_num < 0)) {
			//printf("partition, match1, name:%s %s, part_num:%d\n",
			//	partnamestr, direntptr->name, part_num);

			match = 1;
		}
		if((partnamestr == 0) && (direntptr->part_num == part_num)) {
			//printf("partition, match2, n:%s, part_num:%d == %d\n",
			//	direntptr->name, direntptr->part_num, part_num);
			match = 1;
		}
		if(match) {
			//printf("match with dimage_start:%08llx, dimage_size:"
			//	"%08llx\n", dsk->dimage_start,
			//	dsk->dimage_size);

			printf("match with dimage_start:%08llx, dimage_size:"
				"%08llx\n", direntptr->dimage_start,
				direntptr->dsize);
			ret = i;
			if(g_cfg_partition_is_zip) {
				ret = undeflate_zipfile(dsk, fd,
					direntptr->dimage_start,
					direntptr->dsize,
					direntptr->compr_dsize);
				close(fd);
			} else {
				dsk->fd = fd;
				dsk->dimage_start = direntptr->dimage_start;
				dsk->dimage_size = direntptr->dsize;
			}

			return ret;
		}
	}

	close(fd);
	// printf("No matches, ret -1\n");
	return -1;
}

int
cfg_partition_make_list_from_name(const char *namestr)
{
	int	fd, ret;

	fd = open(namestr, O_RDONLY | O_BINARY, 0x1b6);
	if(fd < 0) {
		fatal_printf("Cannot open part image: %s\n", namestr);
		return 0;
	}
	ret = cfg_partition_make_list(fd);

	close(fd);
	return ret;
}

int
cfg_partition_make_list(int fd)
{
	Driver_desc *driver_desc_ptr;
	Part_map *part_map_ptr;
	word32	*blk_bufptr;
	dword64	dimage_start, dimage_size, dsize;
	word32	start, len, data_off, data_len, sig, map_blk_cnt, cur_blk;
	word32	map_blks, block_size;
	int	is_dir, ret;

	block_size = 512;
	g_cfg_partition_is_zip = 0;

	cfg_free_alldirents(&g_cfg_partitionlist);

	blk_bufptr = (word32 *)malloc(MAX_PARTITION_BLK_SIZE);

	cfg_partition_read_block(fd, blk_bufptr, 0, block_size);

	driver_desc_ptr = (Driver_desc *)blk_bufptr;
	sig = cfg_get_be_word16(&(driver_desc_ptr->sig));
	block_size = cfg_get_be_word16(&(driver_desc_ptr->blk_size));
	if(block_size == 0) {
		block_size = 512;
	}
	if((sig != 0x4552) || (block_size < 0x200) ||
				(block_size > MAX_PARTITION_BLK_SIZE)) {
		printf("Partition error: No driver descriptor map found\n");
		free(blk_bufptr);
		ret = undeflate_zipfile_make_list(fd);
		if(ret > 0) {
			g_cfg_partition_is_zip = 1;
		}
		return ret;
	}

	map_blks = 1;
	cur_blk = 0;
	dsize = cfg_get_fd_size(fd);
	cfg_file_add_dirent(&g_cfg_partitionlist, "None - Whole image",
			is_dir=0, dsize, 0, 0, -1);

	while(cur_blk < map_blks) {
		cur_blk++;
		cfg_partition_read_block(fd, blk_bufptr, cur_blk, block_size);
		part_map_ptr = (Part_map *)blk_bufptr;
		sig = cfg_get_be_word16(&(part_map_ptr->sig));
		map_blk_cnt = cfg_get_be_word32(&(part_map_ptr->map_blk_cnt));
		if(cur_blk <= 1) {
			map_blks = MY_MIN(20, map_blk_cnt);
		}
		if(sig != 0x504d) {
			printf("Partition entry %d bad signature:%04x\n",
				cur_blk, sig);
			free(blk_bufptr);
			return g_cfg_partitionlist.last;
		}

		/* found it, check for consistency */
		start = cfg_get_be_word32(&(part_map_ptr->phys_part_start));
		len = cfg_get_be_word32(&(part_map_ptr->part_blk_cnt));
		data_off = cfg_get_be_word32(&(part_map_ptr->data_start));
		data_len = cfg_get_be_word32(&(part_map_ptr->data_cnt));
		if(data_off + data_len > len) {
			printf("Poorly formed entry\n");
			continue;
		}

		if(data_len < 10 || start < 1) {
			printf("Poorly formed entry %d, datalen:%d, "
				"start:%08x\n", cur_blk, data_len, start);
			continue;
		}

		dimage_size = (dword64)data_len * block_size;
		dimage_start = ((dword64)start + data_off) * block_size;
		is_dir = 2*(dimage_size < 800*1024);
#if 0
		printf(" partition add entry %d = %s %d %08llx %08llx\n",
			cur_blk, part_map_ptr->part_name, is_dir,
			dimage_size, dimage_start);
#endif

		cfg_file_add_dirent(&g_cfg_partitionlist,
			part_map_ptr->part_name, is_dir, dimage_size,
			dimage_start, 0, cur_blk);
	}

	free(blk_bufptr);
	return g_cfg_partitionlist.last;
}

int
cfg_maybe_insert_disk(int slot, int drive, const char *namestr)
{
	int	num_parts;

	g_cfg_part_path[0] = 0;
	num_parts = cfg_partition_make_list_from_name(namestr);

	if(num_parts > 0) {
		printf("Choose a partition\n");
		g_cfg_select_partition = 1;
		g_cfg_file_pathfield = 0;
	} else {
		insert_disk(slot, drive, namestr, 0, 0, -1, 0);
		return 1;
	}
	return 0;
}

void
cfg_insert_disk_dynapro(int slot, int drive, const char *name)
{
	int	dynapro_blocks;

	dynapro_blocks = 280;
	if(slot == 5) {
		dynapro_blocks = 1600;
	} else if(slot == 7) {
		dynapro_blocks = 65535;
	}
	if(g_cfg_newdisk_select && (g_cfg_newdisk_type == 3) &&
							g_cfg_newdisk_blocks) {
		dynapro_blocks = g_cfg_newdisk_blocks;
	}
	insert_disk(slot, drive, name, 0, 0, -1, dynapro_blocks);
}

int
cfg_stat(char *path, struct stat *sb, int do_lstat)
{
	int	ret, len, removed_slash;

	removed_slash = 0;
	len = 0;

	/* Windows doesn't like to stat paths ending in a /, so remove it */
	len = (int)strlen(path);
#ifdef _WIN32
	if((len > 1) && (path[len - 1] == '/') ) {
		path[len - 1] = 0;	/* remove the slash */
		removed_slash = 1;
	}
#endif

	if(do_lstat) {
		ret = lstat(path, sb);
	} else {
		ret = stat(path, sb);
	}

	/* put the slash back */
	if(removed_slash) {
		path[len - 1] = '/';
	}

	return ret;
}

word32
cfg_get_le16(byte *bptr)
{
	return bptr[0] | (bptr[1] << 8);
}

word32
cfg_get_le32(byte *bptr)
{
	return bptr[0] | (bptr[1] << 8) | (bptr[2] << 16) | (bptr[3] << 24);
}

dword64
cfg_get_le64(byte *bptr)
{
	dword64	dval;
	int	i;

	dval = 0;
	for(i = 7; i >= 0; i--) {
		dval = (dval << 8) | bptr[i];
	}
	return dval;
}

word32
cfg_get_be_word16(word16 *ptr)
{
	byte	*bptr;

	bptr = (byte *)ptr;
	return (bptr[0] << 8) | bptr[1];
}

word32
cfg_get_be_word32(word32 *ptr)
{
	byte	*bptr;

	bptr = (byte *)ptr;
	return (bptr[0] << 24) | (bptr[1] << 16) | (bptr[2] << 8) | bptr[3];
}

void
cfg_set_le32(byte *bptr, word32 val)
{
	*bptr++ = val;
	*bptr++ = val >> 8;
	*bptr++ = val >> 16;
	*bptr++ = val >> 24;
}

void
config_file_to_pipe(Disk *dsk, const char *cmd_ptr, const char *name_ptr)
{
#ifdef _WIN32
	printf("Cannot do pipe from cmd %s to %s\n", cmd_ptr, name_ptr);
	return;
#else
	int	output_pipe[2];
	byte	*bptr2;
	char	*bufptr;
	pid_t	pid;
	int	stat_loc, ret, bufsize, pos, fd;
	int	i;

	// Create a pipe to cmd_ptr, and send the contents of name_ptr to it
	//  Collect the output in a 32MB buffer.  Once complete, allocate
	//  a buffer of the correct size, copy to it, and free the giant buffer
	// Sample usage: "gunzip", "{filename}.gz" will run gunzip and collect
	//  uncompressed data
	ret = pipe(&output_pipe[0]);
	if(ret < 0) {
		return;
	}
	printf("output_pipe[0]=%d, [1]=%d\n", output_pipe[0], output_pipe[1]);
	bufsize = 32*1024*1024;
	bufptr = malloc(bufsize);
	if(bufptr == 0) {
		return;
	}
	pos = 0;
	pid = fork();
	if(pid == 0) {
		close(output_pipe[0]);
		ret = dup2(output_pipe[1], 1);
		if(ret < 0) {
			exit(1);
		}
		// The child.  Open 0 as the file, and then do system
		close(0);
		fd = open(name_ptr, O_RDONLY | O_BINARY, 0x1b6);
		if(fd != 0) {
			exit(1);
		}
		// Now just run the command.  Input is from name_ptr, output is
		//  to a pipe
		(void)!system(cmd_ptr);
		exit(0);
	} else if(pid > 0) {
		// Parent.  Collect output from output_pipe[0], and write it
		//  to bufptr.
		close(output_pipe[1]);
		while(1) {
			if(pos >= bufsize) {
				break;
			}
			ret = read(output_pipe[0], bufptr + pos, bufsize - pos);
			if(ret <= 0) {
				break;
			}
			pos += ret;
		}
		close(output_pipe[0]);
		waitpid(pid, &stat_loc, 0);
	} else {
		// Error case
		close(output_pipe[1]);
		close(output_pipe[0]);
	}

	// See what we got
	bptr2 = 0;
	printf("Read %d bytes from %s\n", pos, name_ptr);
	if(pos >= 140*1024) {
		// Looks like it could be an image
		bptr2 = malloc(pos);
		for(i = 0; i < pos; i++) {
			bptr2[i] = bufptr[i];
		}
		dsk->raw_data = bptr2;
		dsk->fd = 0;			// Indicates raw_data is valid
		dsk->raw_dsize = pos;
	}
	free(bufptr);
#endif
}

void
cfg_htab_vtab(int x, int y)
{
	if(x > 79) {
		x = 0;
	}
	if(y > 23) {
		y = 0;
	}
	g_cfg_curs_x = x;
	g_cfg_curs_y = y;
	g_cfg_curs_inv = 0;
	g_cfg_curs_mousetext = 0;
}

void
cfg_home()
{
	int	i;

	cfg_htab_vtab(0, 0);
	for(i = 0; i < 24; i++) {
		cfg_cleol();
	}
}

void
cfg_cleol()
{
	g_cfg_curs_inv = 0;
	g_cfg_curs_mousetext = 0;
	cfg_putchar(' ');
	while(g_cfg_curs_x != 0) {
		cfg_putchar(' ');
	}
}

void
cfg_putchar(int c)
{
	int	x, y;

	if(c == '\n') {
		cfg_cleol();
		return;
	}
	if(c == '\b') {
		g_cfg_curs_inv = !g_cfg_curs_inv;
		return;
	}
	if(c == '\t') {
		g_cfg_curs_mousetext = !g_cfg_curs_mousetext;
		return;
	}
	y = g_cfg_curs_y;
	x = g_cfg_curs_x;

	// Normal: 0xa0-0xff for space through lowercase
	// Inverse: 0x00-0x3f for upper case and numbers, 0x60-0x7f for lcase
	// Mousetext: 0x40-0x5f
	if(g_cfg_curs_inv) {
		if(c >= 0x40 && c < 0x60) {
			c = c & 0x1f;
		}
	} else {
		c = c | 0x80;
	}
	if(g_cfg_curs_mousetext) {
		c = (c & 0x1f) | 0x40;
	}
	g_cfg_screen[y][x] = c;
	x++;
	if(x >= 80) {
		x = 0;
		y++;
		if(y >= 24) {
			y = 0;
		}
	}
	g_cfg_curs_y = y;
	g_cfg_curs_x = x;
	g_cfg_screen_changed = 1;
}

void
cfg_printf(const char *fmt, ...)
{
	va_list ap;
	int	c;
	int	i;

	va_start(ap, fmt);
	(void)vsnprintf(g_cfg_printf_buf, CFG_PRINTF_BUFSIZE, fmt, ap);
	va_end(ap);

	for(i = 0; i < CFG_PRINTF_BUFSIZE; i++) {
		c = g_cfg_printf_buf[i];
		if(c == 0) {
			return;
		}
		cfg_putchar(c);
	}
}

void
cfg_print_dnum(dword64 dnum, int max_len)
{
	char	buf[64];
	char	buf2[64];
	int	len, cnt, c;
	int	i, j;

	/* Prints right-adjusted "num" in field "max_len" wide */
	snprintf(&buf[0], 64, "%lld", dnum);
	len = (int)strlen(buf);
	for(i = 0; i < 64; i++) {
		buf2[i] = ' ';
	}
	j = max_len + 1;
	buf2[j] = 0;
	j--;
	cnt = 0;
	for(i = len - 1; (i >= 0) && (j >= 1); i--) {
		c = buf[i];
		if(c >= '0' && c <= '9') {
			if(cnt >= 3) {
				buf2[j--] = ',';
				cnt = 0;
			}
			cnt++;
		}
		buf2[j--] = c;
	}
	cfg_printf(&buf2[1]);
}

int
cfg_get_disk_name(char *outstr, int maxlen, int type_ext, int with_extras)
{
	Disk	*dsk;
	int	slot, drive;

	slot = type_ext >> 8;
	drive = type_ext & 0xff;
	dsk = iwm_get_dsk_from_slot_drive(slot, drive);

	outstr[0] = 0;
	if(dsk->name_ptr == 0) {
		return 0;
	}

	config_generate_config_kegs_name(outstr, maxlen, dsk, with_extras);
	return dsk->dynapro_blocks;
}

int
cfg_get_disk_locked(int type_ext)
{
	Disk	*dsk;
	int	slot, drive;

	slot = type_ext >> 8;
	drive = type_ext & 0xff;
	dsk = iwm_get_dsk_from_slot_drive(slot, drive);
	if(dsk->fd < 0) {
		return 0;
	}

	if(dsk->write_prot) {
		return 1;
	} else if(!dsk->write_through_to_unix) {
		return 2;
	}

	return 0;
}

void
cfg_parse_menu(Cfg_menu *menuptr, int menu_pos, int highlight_pos, int change)
{
	char	valbuf[CFG_OPT_MAXSTR];
	char	*(*fn_ptr)(int);
	int	*iptr;
	char	**str_ptr;
	const char *menustr;
	char	*curstr, *defstr, *str, *outstr;
	void	*edit_ptr;
	int	val, num_opts, opt_num, bufpos, outpos, curval, defval, type;
	int	type_ext, opt_get_str, separator, len, c, locked;
	int	i;

	// For this menu_pos line, create output in g_cfg_opt_buf[] string
	//  Highlight it if menu_pos==highlight_pos
	// Allow arrows to modify the currently selected item of a list using
	//  change: -1 moves to a previous item, +1 moves to the next

	g_cfg_opt_buf[0] = 0;

	num_opts = 0;
	opt_get_str = 0;
	separator = ',';

	menuptr += menu_pos;		/* move forward to entry menu_pos */

	menustr = menuptr->str;
	type = menuptr->cfgtype;
	type_ext = (type >> 4);
	type = type & 0xf;
	len = (int)strlen(menustr) + 1;

	bufpos = 0;
	outstr = &(g_cfg_opt_buf[0]);

	outstr[bufpos++] = ' ';		// 0
	outstr[bufpos++] = ' ';		// 1
	outstr[bufpos++] = '\t';	// 2
	outstr[bufpos++] = '\t';	// 3
	outstr[bufpos++] = ' ';		// 4
	outstr[bufpos++] = ' ';		// 5

	// Figure out if we should get a checkmark
	curval = -1;
	defval = -1;
	curstr = 0;
	if(type == CFGTYPE_INT) {
		iptr = menuptr->ptr;
		curval = *iptr;
		iptr = menuptr->defptr;
		if(!iptr) {
			printf("BAD MENU, defptr is 0!\n");
		} else {
			defval = *iptr;
		}
		if(curval == defval) {
			g_cfg_opt_buf[3] = 'D';	/* checkmark */
			g_cfg_opt_buf[4] = '\t';
		}
	}

	if((type == CFGTYPE_FILE) || (type == CFGTYPE_STR)) {
		str_ptr = (char **)menuptr->ptr;
		curstr = *str_ptr;
		str_ptr = (char **)menuptr->defptr;
		if(!str_ptr) {
			printf("BAD MENU, defptr str is 0!\n");
			defstr = "";
		} else {
			defstr = *str_ptr;
		}
		if(strcmp(curstr, defstr) == 0) {
			g_cfg_opt_buf[3] = 'D';	/* checkmark */
			g_cfg_opt_buf[4] = '\t';
		}
	}

	// If it's a menu, give it a special menu indicator
	if(type == CFGTYPE_MENU) {
		g_cfg_opt_buf[1] = '\t';
		g_cfg_opt_buf[2] = 'M';		/* return-like symbol */
		g_cfg_opt_buf[3] = '\t';
		g_cfg_opt_buf[4] = ' ';
	}

	if(type == CFGTYPE_DISK) {
		locked = cfg_get_disk_locked(type_ext);
		if(locked) {
			g_cfg_opt_buf[4] = '*';
			if(locked == 2) {		// inverse
				g_cfg_opt_buf[2] = '\b';
				g_cfg_opt_buf[3] = '*';
				g_cfg_opt_buf[4] = '\b';
			}
		}
	}

	if(menu_pos == highlight_pos) {
		outstr[bufpos++] = '\b';
	}

	opt_get_str = 2;
	i = -1;
	outpos = bufpos;
#if 0
	printf("cfg menu_pos: %d str len: %d\n", menu_pos, len);
#endif
	while(++i < len) {
		c = menustr[i];
		if(c == separator) {		// ','?
			if(i == 0) {
				continue;
			}
			c = 0;
		}
		outstr[outpos++] = c;
		outstr[outpos] = 0;
		if(outpos >= CFG_OPT_MAXSTR) {
			fprintf(stderr, "CFG_OPT_MAXSTR exceeded\n");
			my_exit(1);
			return;
		}
		if(c == 0) {
			if(opt_get_str == 2) {
				outstr = &(valbuf[0]);
				bufpos = outpos - 1;
				opt_get_str = 0;
			} else if(opt_get_str) {
#if 0
				if(menu_pos == highlight_pos) {
					printf("menu_pos %d opt %d = %s=%d\n",
						menu_pos, num_opts,
						g_cfg_opts_str[0],
						g_cfg_opts_vals[num_opts]);
				}
#endif
				num_opts++;
				outstr = &(valbuf[0]);
				opt_get_str = 0;
				if(num_opts >= CFG_MAX_OPTS) {
					fprintf(stderr, "CFG_MAX_OPTS oflow\n");
					my_exit(1);
					return;
				}
			} else {
				val = (word32)strtoul(valbuf, 0, 0);
				g_cfg_opts_vals[num_opts] = val;
				outstr = &(g_cfg_opts_str[0]);
				if(val != curval) {
					outstr = valbuf;
				}
				opt_get_str = 1;
			}
			outpos = 0;
			outstr[0] = 0;
		}
	}

	if(menu_pos == highlight_pos) {
		g_cfg_opt_buf[bufpos++] = '\b';
	}

	g_cfg_opt_buf[bufpos] = 0;

	// Decide what to display on the "right" side
	str = 0;
	opt_num = -1;
	if((type == CFGTYPE_INT) || (type == CFGTYPE_FILE) ||
						(type == CFGTYPE_STR)) {
		g_cfg_opt_buf[bufpos++] = ' ';
		g_cfg_opt_buf[bufpos++] = '=';
		g_cfg_opt_buf[bufpos++] = ' ';
		g_cfg_opt_buf[bufpos] = 0;
		for(i = 0; i < num_opts; i++) {
			if(curval == g_cfg_opts_vals[i]) {
				opt_num = i;
				break;
			}
		}
	}

	if(change != 0) {
		if(type == CFGTYPE_INT) {
			if(num_opts > 0) {
				opt_num += change;
				if(opt_num >= num_opts) {
					opt_num = 0;
				}
				if(opt_num < 0) {
					opt_num = num_opts - 1;
				}
				curval = g_cfg_opts_vals[opt_num];
			} else {
				curval += change;
				/* HACK: min_val, max_val testing here */
			}
			iptr = (int *)menuptr->ptr;
			cfg_int_update(iptr, curval);
		}
		g_config_kegs_update_needed = 1;
	}

#if 0
	if(menu_pos == highlight_pos) {
		printf("menu_pos %d opt_num %d\n", menu_pos, opt_num);
	}
#endif

	edit_ptr = g_cfg_edit_ptr;
	if((edit_ptr == menuptr->ptr) && edit_ptr) {
		// Just show the current edit string
		str = cfg_shorten_filename(&(g_cfg_edit_buf[0]), 68 - 1);
		cfg_strlcat(str, "\b \b", CFG_PATH_MAX);
	} else if(opt_num >= 0) {
		str = &(g_cfg_opts_str[0]);
	} else {
		if(type == CFGTYPE_INT) {
			str = &(g_cfg_opts_str[0]);
			snprintf(str, CFG_OPT_MAXSTR, "%d", curval);
		} else if (type == CFGTYPE_DISK) {
			str = &(g_cfg_opts_str[0]);
			(void)cfg_get_disk_name(str, CFG_PATH_MAX, type_ext, 1);
			str = cfg_shorten_filename(str, 68);
		} else if ((type == CFGTYPE_FILE) || (type == CFGTYPE_STR)) {
			str = cfg_shorten_filename(curstr, 68);
		} else if(type == CFGTYPE_FUNC) {
			fn_ptr = (char *(*)(int))menuptr->ptr;
			str = "";
			curstr = (*fn_ptr)(1);
			if(curstr) {
				g_cfg_opt_buf[bufpos++] = ' ';
				g_cfg_opt_buf[bufpos++] = ':';
				g_cfg_opt_buf[bufpos++] = ' ';
				g_cfg_opt_buf[bufpos] = 0;
				str = cfg_shorten_filename(curstr, 68);
				free(curstr);
			}
		} else {
			str = "";
		}
	}

#if 0
	if(menu_pos == highlight_pos) {
		printf("menu_pos %d buf_pos %d, str is %s, %02x, %02x, "
			"%02x %02x\n",
			menu_pos, bufpos, str, g_cfg_opt_buf[bufpos-1],
			g_cfg_opt_buf[bufpos-2],
			g_cfg_opt_buf[bufpos-3],
			g_cfg_opt_buf[bufpos-4]);
	}
#endif

	g_cfg_opt_buf[bufpos] = 0;
	cfg_strncpy(&(g_cfg_opt_buf[bufpos]), str, CFG_OPT_MAXSTR - bufpos - 1);
	g_cfg_opt_buf[CFG_OPT_MAXSTR-1] = 0;
}

void
cfg_get_base_path(char *pathptr, const char *inptr, int go_up)
{
	const char *tmpptr;
	char	*slashptr;
	char	*outptr;
	int	add_dotdot, is_dotdot;
	int	len;
	int	c;

	// Take full filename, copy it to pathptr, and truncate at last slash
	//  inptr and pathptr can be the same.
	// If go_up is set, then replace a blank dir with ".."
	//  but first, see if path is currently just ../ over and over
	// if so, just tack .. onto the end and return

	//printf("cfg_get_base start with %s\n", inptr);

	g_cfg_file_match[0] = 0;
	tmpptr = inptr;
	is_dotdot = 1;
	while(1) {
		if(tmpptr[0] == 0) {
			break;
		}
		if((tmpptr[0] == '.') && (tmpptr[1] == '.') &&
							(tmpptr[2] == '/')) {
			tmpptr += 3;
		} else {
			is_dotdot = 0;
			break;
		}
	}
	slashptr = 0;
	outptr = pathptr;
	c = -1;
	while(c != 0) {
		c = *inptr++;
		if(c == '/') {
			if(*inptr != 0) {	/* if not a trailing slash... */
				slashptr = outptr;
			}
		}
		*outptr++ = c;
	}
	if(!go_up) {
		/* if not go_up, copy chopped part to g_cfg_file_match*/
		/* copy from slashptr+1 to end */
		tmpptr = slashptr+1;
		if(slashptr == 0) {
			tmpptr = pathptr;
		}
		cfg_strncpy(&g_cfg_file_match[0], tmpptr, CFG_PATH_MAX);
		/* remove trailing / from g_cfg_file_match */
		len = (int)strlen(&g_cfg_file_match[0]);
		if((len > 1) && (len < (CFG_PATH_MAX - 1)) &&
					g_cfg_file_match[len - 1] == '/') {
			g_cfg_file_match[len - 1] = 0;
		}
		//printf("set g_cfg_file_match to %s\n", &g_cfg_file_match[0]);
	}
	if(!is_dotdot && (slashptr != 0)) {
		slashptr[0] = '/';
		slashptr[1] = 0;
		outptr = slashptr + 2;
	}
	add_dotdot = 0;
	if(pathptr[0] == 0 || is_dotdot) {
		/* path was blank, or consisted of just ../ pattern */
		if(go_up) {
			add_dotdot = 1;
		}
	} else if(slashptr == 0) {
		/* no slashes found, but path was not blank--make it blank */
		if(pathptr[0] == '/') {
			pathptr[1] = 0;
		} else {
			pathptr[0] = 0;
		}
	}

	if(add_dotdot) {
		--outptr;
		outptr[0] = '.';
		outptr[1] = '.';
		outptr[2] = '/';
		outptr[3] = 0;
	}

	//printf("cfg_get_base end with %s, is_dotdot:%d, add_dotdot:%d\n",
	//		pathptr, is_dotdot, add_dotdot);
}

char *
cfg_name_new_image(int get_status)
{
	// Called from menu to create a new disk image, this will pop up the
	//  file selection dialog.  Once name is selected,
	//  cfg_create_new_image() is called.
	if(get_status) {
		return 0;
	}
	printf("cfg_name_new_image called!\n");
	g_cfg_slotdrive = g_cfg_newdisk_slotdrive;
	g_cfg_newdisk_select = 1;
	cfg_file_init();
	return 0;
}

void
cfg_dup_existing_image(word32 slotdrive)
{
	// Set g_cfg_newdisk_* to copy the slotdrive image
	g_cfg_slotdrive = slotdrive;
	g_cfg_newdisk_select = 2;			// Do DUP
	cfg_file_init();
}

void
cfg_dup_image_selected()
{
	Disk	*dsk;
	Woz_info *wozinfo_ptr;
	byte	*bufptr;
	char	*str;
	dword64	dsize, dret;
	word32	slotdrive;
	int	fd;

	// printf("cfg_dup_image_selected\n");

	slotdrive = g_cfg_slotdrive;
	g_cfg_slotdrive = 0;
	g_menuptr = &g_cfg_disk_menu[0];
	g_menu_redraw_needed = 1;
	g_cfg_newdisk_select = 0;
	g_cfg_newdisk_slotdrive = 0;
	printf("slotdrive:%04x\n", slotdrive);
	if(slotdrive < 0x500) {
		return;		// Invalid slot,drive: Do nothing
	}
	dsk = iwm_get_dsk_from_slot_drive((slotdrive >> 8) & 7,
							slotdrive & 0xff);
	if(dsk->fd < 0) {
		return;		// No disk
	}
	dsize = dsk->dimage_size;

	str = &g_cfg_file_path[0],
	printf("Create dup image %s, dsize:%lld\n", str, dsize);
	if((word32)dsize != dsize) {
		return;
	}
	fd = open(str, O_RDWR | O_CREAT | O_TRUNC, 0x1b6);
	if(fd < 0) {
		printf("Open %s failed, errno:%d\n", str, errno);
		return;
	}
	wozinfo_ptr = dsk->wozinfo_ptr;
	if(wozinfo_ptr && !dsk->write_through_to_unix) {
		// Just write out the WOZ image, and then fully enable this
		//  image
		printf("Writing out .WOZ image to %s, size:%d\n", str,
							wozinfo_ptr->woz_size);
		if((dsk->raw_data == 0) && (dsk->fd >= 0)) {
			close(dsk->fd);
		}
		dsk->fd = fd;
		woz_rewrite_crc(dsk, wozinfo_ptr->woz_size);
			// Above will recalc CRC and write out woz_size bytes
		dsk->raw_data = 0;
		dsk->write_through_to_unix = 1;
		free(dsk->name_ptr);
		dsk->name_ptr = kegs_malloc_str(str);
		free(dsk->partition_name);
		dsk->partition_name = 0;
		dsk->image_type = DSK_TYPE_WOZ;
		dsk->dimage_size = wozinfo_ptr->woz_size;
		dsk->dimage_start = 0;
		g_config_kegs_update_needed = 1;
		woz_check_file(dsk);
	} else if(dsk->raw_data) {
		cfg_write_to_fd(fd, dsk->raw_data, 0, dsize);
		close(fd);
	} else {
		bufptr = malloc((size_t)dsize);
		if((bufptr != 0) && ((size_t)dsize == dsize)) {
			dret = cfg_read_from_fd(dsk->fd, bufptr, 0, dsize);
			if(dret == dsize) {
				cfg_write_to_fd(fd, bufptr, 0, dsize);
			}
		}
		free(bufptr);
		close(fd);
	}
}

void
cfg_validate_image(word32 slotdrive)
{
	Disk	*dsk;

	dsk = iwm_get_dsk_from_slot_drive((slotdrive >> 8) & 7,
							slotdrive & 0xff);
	dynapro_validate_any_image(dsk);
}

void
cfg_toggle_lock_disk(word32 slotdrive)
{
	Disk	*dsk;

	dsk = iwm_get_dsk_from_slot_drive((slotdrive >> 8) & 7,
							slotdrive & 0xff);
	iwm_toggle_lock(dsk);
}

int
cfg_create_new_image_act(const char *str, int type, int size_blocks)
{
	byte	buf[512];
	dword64	dret;
	int	fd, ret;
	int	i;

	// Called after file dialog selects a new image name, this creates it

	if(size_blocks == 65536) {
		size_blocks--;		// Shrink to 65535 total blocks
	}
	printf("Create new image type:%d, size:%dKB\n", type, size_blocks/2);
	fd = open(str, O_RDWR | O_CREAT | O_TRUNC, 0x1b6);
	if(fd < 0) {
		printf("Open %s failed, errno:%d\n", str, errno);
		return fd;
	}
	for(i = 0; i < 512; i++) {
		buf[i] = 0;
	}

	ret = 0;
	if(type == 2) {		// WOZ
		(void)woz_new(fd, str, size_blocks/2);
	} else {
		for(i = 0; i < size_blocks; i++) {
			dret = cfg_write_to_fd(fd, &(buf[0]), i * 512U, 512);
			if(dret != 512) {
				ret = -1;
				break;
			}
		}
	}
	close(fd);

	return ret;		// 0=success, -1 is a failure
}

void
cfg_create_new_image()
{
	word32	dynamic_blocks;
	int	ret;

	// Type is in g_cfg_file_path.  Create this file and prepare it
	printf("Creating new image: %s\n", &g_cfg_file_path[0]);

	if(g_cfg_newdisk_select == 2) {
		cfg_dup_image_selected();
		return;
	}
	ret = 0;
	dynamic_blocks = 0;
	if(g_cfg_newdisk_type == 3) {
		dynamic_blocks = g_cfg_newdisk_blocks;
	} else {
		ret = cfg_create_new_image_act(&g_cfg_file_path[0],
				g_cfg_newdisk_type, g_cfg_newdisk_blocks);
	}
	if(ret < 0) {
		// Maybe open a dialog?  Oh well...do nothing
	} else {
		insert_disk((g_cfg_slotdrive >> 8) & 0xf,
			g_cfg_slotdrive & 0xff, &(g_cfg_file_path[0]),
			0, 0, -2, dynamic_blocks);
	}
	g_cfg_slotdrive = 0;
	g_menuptr = &g_cfg_disk_menu[0];
	g_menu_redraw_needed = 1;
	g_cfg_newdisk_select = 0;
	g_cfg_newdisk_slotdrive = 0;
}

void
cfg_file_init()
{
	int	slot, drive, is_dynapro;
	int	i;

	is_dynapro = 0;
	if((g_cfg_slotdrive & 0xfff) == 0xfff) {	// File selection
		// Just use g_cfg_file_def_name
		cfg_strncpy(&g_cfg_tmp_path[0], g_cfg_file_def_name,
								CFG_PATH_MAX);
	} else {
		is_dynapro = cfg_get_disk_name(&g_cfg_tmp_path[0], CFG_PATH_MAX,
							g_cfg_slotdrive, 0);

		slot = (g_cfg_slotdrive >> 8) & 7;
		drive = g_cfg_slotdrive & 1;
		for(i = 0; i < 6; i++) {
			if(g_cfg_tmp_path[0] != 0) {
				break;
			}
			/* try to get a starting path from some mounted drive */
			drive = !drive;
			if(i & 1) {
				slot++;
				if(slot >= 8) {
					slot = 5;
				}
			}
			is_dynapro = cfg_get_disk_name(&g_cfg_tmp_path[0],
					CFG_PATH_MAX, (slot << 8) + drive, 0);
		}
	}

	cfg_get_base_path(&g_cfg_file_curpath[0], &g_cfg_tmp_path[0], 0);
	if(is_dynapro) {
		// Use the full path to the dir (don't strip off last part)
		cfg_strncpy(&g_cfg_file_curpath[0], &g_cfg_tmp_path[0],
							CFG_PATH_MAX);
	}
	g_cfg_dirlist.invalid = 1;
}

void
cfg_free_alldirents(Cfg_listhdr *listhdrptr)
{
	int	i;

	if(listhdrptr->max > 0) {
		// Free the old directory listing
		for(i = 0; i < listhdrptr->last; i++) {
			free(listhdrptr->direntptr[i].name);
		}
		free(listhdrptr->direntptr);
	}

	listhdrptr->direntptr = 0;
	listhdrptr->last = 0;
	listhdrptr->max = 0;
	listhdrptr->invalid = 0;

	listhdrptr->topent = 0;
	listhdrptr->curent = 0;
}

void
cfg_file_add_dirent_unique(Cfg_listhdr *listhdrptr, const char *nameptr,
	int is_dir, dword64 dsize, dword64 dimage_start, dword64 compr_dsize,
	int part_num)
{
	Cfg_dirent *direntptr;
	int	num, namelen, this_len;
	int	i;

	// Loop through all entries, make sure name is unique
	num = listhdrptr->last;
	namelen = (int)strlen(nameptr);
	for(i = 0; i < num; i++) {
		direntptr = &(listhdrptr->direntptr[i]);
		this_len = (int)strlen(direntptr->name);
		if(cfg_str_match(direntptr->name, nameptr, namelen) == 0) {
			// It's a match...check len
			if(namelen == this_len) {
				return;
			}
		}
	}
	cfg_file_add_dirent(listhdrptr, nameptr, is_dir, dsize, dimage_start,
						compr_dsize, part_num);
}

void
cfg_file_add_dirent(Cfg_listhdr *listhdrptr, const char *nameptr, int is_dir,
	dword64 dsize, dword64 dimage_start, dword64 compr_dsize, int part_num)
{
	Cfg_dirent *direntptr;
	char	*ptr;
	int	inc_amt, namelen;

	namelen = (int)strlen(nameptr);
	if(listhdrptr->last >= listhdrptr->max) {
		// realloc
		inc_amt = MY_MAX(64, listhdrptr->max);
		inc_amt = MY_MIN(inc_amt, 1024);
		listhdrptr->max += inc_amt;
		listhdrptr->direntptr = realloc(listhdrptr->direntptr,
					listhdrptr->max * sizeof(Cfg_dirent));
	}
	ptr = malloc(namelen+1+is_dir);
	cfg_strncpy(ptr, nameptr, namelen+1);
	if(is_dir && (namelen >= 1) && (ptr[namelen - 1] != '/')) {
		// Add a trailing '/' to directories, unless already there
		cfg_strlcat(ptr, "/", namelen + 1 + is_dir);
	}
#if 0
	printf("...file entry %d is %s\n", listhdrptr->last, ptr);
#endif
	direntptr = &(listhdrptr->direntptr[listhdrptr->last]);
	direntptr->name = ptr;
	direntptr->is_dir = is_dir;
	direntptr->dsize = dsize;
	direntptr->dimage_start = dimage_start;
	direntptr->compr_dsize = compr_dsize;
	direntptr->part_num = part_num;
	listhdrptr->last++;
}

int
cfg_dirent_sortfn(const void *obj1, const void *obj2)
{
	const Cfg_dirent *direntptr1, *direntptr2;
	int	ret;

	/* Called by qsort to sort directory listings */
	direntptr1 = (const Cfg_dirent *)obj1;
	direntptr2 = (const Cfg_dirent *)obj2;
	ret = cfg_str_match(direntptr1->name, direntptr2->name, CFG_PATH_MAX);
	return ret;
}

int
cfg_str_match(const char *str1, const char *str2, int len)
{
	return cfg_str_match_maybecase(str1, str2, len, g_cfg_ignorecase);
}

int
cfg_str_match_maybecase(const char *str1, const char *str2, int len,
							int ignorecase)
{
	const byte *bptr1, *bptr2;
	int	c, c2;
	int	i;

	/* basically, work like strcmp or strcasecmp */

	bptr1 = (const byte *)str1;
	bptr2 = (const byte *)str2;
	for(i = 0; i < len; i++) {
		c = *bptr1++;
		c2 = *bptr2++;
		if(ignorecase) {
			c = tolower(c);
			c2 = tolower(c2);
		}
		if((c == 0) || (c2 == 0) || (c != c2)) {
			return c - c2;
		}
	}

	return 0;
}

int
cfgcasecmp(const char *str1, const char *str2)
{
	return cfg_str_match_maybecase(str1, str2, 32767, 1);
}

int
cfg_strlcat(char *dstptr, const char *srcptr, int dstsize)
{
	char	*ptr;
	int	destlen, srclen, ret, c;

	// Concat srcptr to the end of dstptr, ensuring a null within dstsize
	//  Return the total buffer size that would be needed, even if dstsize
	//  is too small.  Compat with strlcat()
	destlen = (int)strlen(dstptr);
	srclen = (int)strlen(srcptr);
	ret = destlen + srclen;
	dstsize--;
	if(destlen >= dstsize) {
		return ret;		// Do nothing, buf too small
	}
	ptr = dstptr + destlen;
	while(destlen < dstsize) {
		c = *srcptr++;
		*ptr++ = c;
		if(c == 0) {
			return ret;
		}
		destlen++;
	}
	dstptr[dstsize] = 0;
	return ret;
}

char *
cfg_strncpy(char *dstptr, const char *srcptr, int dstsize)
{
	char	*ptr;
	int	c;

	// Copy srcptr to dstptr, ensuring there is room for a null
	// Compatible with strncpy()--except dstptr is ALWAYS null terminated
	ptr = dstptr;
	while(--dstsize > 0) {
		c = *srcptr++;
		*ptr++ = c;
		if(c == 0) {
			return dstptr;
		}
	}
	*ptr = 0;
	return dstptr;
}

const char *
cfg_str_basename(const char *str)
{
	int	len;
	int	i;

	// If str is /aa/bb/cc, this routine returns cc
	len = (int)strlen(str);
	while(len && (str[len - 1] == '/')) {
		len--;		// Ignore trailing '/' if there are any
	}
	for(i = len - 1; i > 0; i--) {
		if(str[i] == '/') {
			return str + i + 1;
		}
	}

	return str;
}

char *
cfg_strncpy_dirname(char *dstptr, const char *srcptr, int dstsize)
{
	char	*ptr;
	int	c;

	// If srcptr is /aa/bb/cc, this routine returns /aa/bb/
	// Copy srcptr to dstptr, ensuring there is room for a null
	// Compatible with strncpy()--except dstptr is ALWAYS null terminated
	ptr = dstptr;
	while(--dstsize > 0) {
		c = *srcptr++;
		*ptr++ = c;
		if(c == 0) {
			// Remove any trailing /'s
			ptr--;
			while((ptr > dstptr) && (ptr[0] == '/')) {
				ptr[0] = 0;
				ptr--;
			}
			while(ptr > dstptr) {
				if(ptr[0] == '/') {
					ptr[1] = 0;
					break;
				}
				ptr--;
			}
			return dstptr;
		}
	}
	*ptr = 0;
	return dstptr;
}

void
cfg_file_readdir(const char *pathptr)
{
	struct dirent *direntptr;
	struct stat stat_buf;
	DIR	*dirptr;
	mode_t	fmt;
	char	*str;
	const char *tmppathptr;
	int	size, ret, is_dir, is_gz, len;
	int	i;

	if(!strncmp(pathptr, &g_cfg_file_cachedpath[0], CFG_PATH_MAX) &&
			(g_cfg_dirlist.last > 0) && (g_cfg_dirlist.invalid==0)){
		return;
	}
	// No match, must read new directory

	// Free all dirents that were cached previously
	cfg_free_alldirents(&g_cfg_dirlist);

	cfg_strncpy(&g_cfg_file_cachedpath[0], pathptr, CFG_PATH_MAX);
	cfg_strncpy(&g_cfg_file_cachedreal[0], pathptr, CFG_PATH_MAX);

	str = &g_cfg_file_cachedreal[0];

	for(i = 0; i < 200; i++) {
		len = (int)strlen(str);
		if(len <= 0) {
			break;
		} else if(len < CFG_PATH_MAX-2) {
			if(str[len-1] != '/') {
				// append / to make various routines work
				str[len] = '/';
				str[len+1] = 0;
			}
		}
		ret = cfg_stat(str, &stat_buf, 0);
		is_dir = 0;
		if(ret == 0) {
			fmt = stat_buf.st_mode & S_IFMT;
			if(fmt == S_IFDIR) {
				/* it's a directory */
				is_dir = 1;
			}
		}
		if(is_dir) {
			break;
		} else {
			// user is entering more path, use base for display
			cfg_get_base_path(str, str, 0);
		}
	}

	tmppathptr = str;
	if(str[0] == 0) {
		tmppathptr = ".";
	}
	cfg_file_add_dirent(&g_cfg_dirlist, "..", 1, 0, 0, 0, -1);

	dirptr = opendir(tmppathptr);
	if(dirptr == 0) {
		printf("Could not open %s as a directory\n", tmppathptr);
		return;
	}
	while(1) {
		direntptr = readdir(dirptr);
		if(direntptr == 0) {
			break;
		}
		if(!strcmp(".", direntptr->d_name)) {
			continue;
		}
		if(!strcmp("..", direntptr->d_name)) {
			continue;
		}
		/* Else, see if it is a directory or a file */
		cfg_strncpy(&(g_cfg_tmp_path[0]), &(g_cfg_file_cachedreal[0]),
								CFG_PATH_MAX);
		cfg_strlcat(&(g_cfg_tmp_path[0]), direntptr->d_name,
								CFG_PATH_MAX);
		ret = cfg_stat(&g_cfg_tmp_path[0], &stat_buf, 0);
		len = (int)strlen(g_cfg_tmp_path);
		is_dir = 0;
		is_gz = 0;
		if((len > 3) && !cfgcasecmp(&g_cfg_tmp_path[len - 3], ".gz")) {
			is_gz = 1;
		}
		if((len > 4) && !cfgcasecmp(&g_cfg_tmp_path[len - 4], ".bz2")) {
			is_gz = 1;
		}
		if((len > 4) && !cfgcasecmp(&g_cfg_tmp_path[len - 4], ".zip")) {
			is_gz = 1;
		}
		if((len > 4) && !cfgcasecmp(&g_cfg_tmp_path[len - 4], ".woz")) {
			is_gz = 1;
		}
		if((len > 4) && !cfgcasecmp(&g_cfg_tmp_path[len - 4], ".sdk")) {
			is_gz = 1;
		}
		if(ret != 0) {
			printf("stat %s ret %d, errno:%d\n", &g_cfg_tmp_path[0],
						ret, errno);
			stat_buf.st_size = 0;
			continue;	/* skip it */
		} else {
			fmt = stat_buf.st_mode & S_IFMT;
			size = (int)stat_buf.st_size;
			if(fmt == S_IFDIR) {
				/* it's a directory */
				is_dir = 1;
			} else if((fmt == S_IFREG) && (is_gz == 0)) {
				if((g_cfg_slotdrive & 0xfff) == 0xfff) {
					/* see if there are size limits */
					if((size < g_cfg_file_min_size) ||
						(size > g_cfg_file_max_size)) {
						continue;	/* skip it */
					}
				} else {
					if(size < 140*1024) {
						continue;	/* skip it */
					}
				}
			}
		}
		cfg_file_add_dirent(&g_cfg_dirlist, direntptr->d_name, is_dir,
					(dword64)stat_buf.st_size, 0, 0, -1);
	}

	closedir(dirptr);

	/* then sort the results (Mac's HFS+ is sorted, but other FS won't be)*/
	qsort(&(g_cfg_dirlist.direntptr[0]), g_cfg_dirlist.last,
					sizeof(Cfg_dirent), cfg_dirent_sortfn);

	g_cfg_dirlist.curent = g_cfg_dirlist.last - 1;
	for(i = g_cfg_dirlist.last - 1; i >= 0; i--) {
		ret = cfg_str_match(&(g_cfg_file_match[0]),
				g_cfg_dirlist.direntptr[i].name, CFG_PATH_MAX);
		if(ret <= 0) {
			/* set cur ent to closest filename to the match name */
			g_cfg_dirlist.curent = i;
		}
	}
}

char *
cfg_shorten_filename(const char *in_ptr, int maxlen)
{
	char	*out_ptr;
	int	len, c;
	int	i;

	/* Warning: uses a static string, not reentrant! */

	out_ptr = &(g_cfg_file_shortened[0]);
	len = (int)strlen(in_ptr);
	maxlen = MY_MIN(len, maxlen);
	for(i = 0; i < maxlen; i++) {
		c = in_ptr[i] & 0x7f;
		if(c < 0x20) {
			c = '*';
		}
		out_ptr[i] = c;
	}
	out_ptr[maxlen] = 0;
	if(len > maxlen) {
		for(i = 0; i < (maxlen/2); i++) {
			c = in_ptr[len-i-1] & 0x7f;
			if(c < 0x20) {
				c = '*';
			}
			out_ptr[maxlen-i-1] = c;
		}
		out_ptr[(maxlen/2) - 1] = '.';
		out_ptr[maxlen/2] = '.';
		out_ptr[(maxlen/2) + 1] = '.';
	}

	return out_ptr;
}

void
cfg_fix_topent(Cfg_listhdr *listhdrptr)
{
	int	num_to_show;

	num_to_show = listhdrptr->num_to_show;

	/* Force curent and topent to make sense */
	if(listhdrptr->curent >= listhdrptr->last) {
		listhdrptr->curent = listhdrptr->last - 1;
	}
	if(listhdrptr->curent < 0) {
		listhdrptr->curent = 0;
	}
	if(abs(listhdrptr->curent - listhdrptr->topent) >= num_to_show) {
		listhdrptr->topent = listhdrptr->curent - (num_to_show/2);
	}
	if(listhdrptr->topent > listhdrptr->curent) {
		listhdrptr->topent = listhdrptr->curent - (num_to_show/2);
	}
	if(listhdrptr->topent < 0) {
		listhdrptr->topent = 0;
	}
}

void
cfg_file_draw()
{
	Cfg_listhdr *listhdrptr;
	Cfg_dirent *direntptr;
	const char *tmp_str;
	char	*str, *fmt;
	int	num_to_show;
	int	yoffset;
	int	x, y;
	int	i;

	//printf("cfg_file_draw called\n");

	cfg_file_readdir(&g_cfg_file_curpath[0]);

	for(y = 0; y < 21; y++) {
		cfg_htab_vtab(0, y);
		cfg_printf("\tZ\t");
		for(x = 1; x < 79; x++) {
			cfg_htab_vtab(x, y);
			cfg_putchar(' ');
		}
		cfg_htab_vtab(79, y);
		cfg_printf("\t_\t");
	}

	cfg_htab_vtab(1, 0);
	cfg_putchar('\b');
	for(x = 1; x < 79; x++) {
		cfg_putchar(' ');
	}
	if((g_cfg_slotdrive & 0xfff) == 0xfff) {
		cfg_htab_vtab(5, 0);
		cfg_printf("\bSelect file to use as %-40s\b",
			cfg_shorten_filename(g_cfg_file_def_name, 40));
	} else {
		cfg_htab_vtab(30, 0);
		tmp_str = "Select";
		if(g_cfg_newdisk_select == 2) {
			tmp_str = "Create duplicate";
		} else if(g_cfg_newdisk_select) {
			tmp_str = "Create new";
		}
		cfg_printf("\b%s image for s%dd%d\b", tmp_str,
			(g_cfg_slotdrive >> 8) & 0xf,
			(g_cfg_slotdrive & 0xff) + 1);
	}

	cfg_htab_vtab(2, 1);
	cfg_printf("config.kegs path: %-56s",
			cfg_shorten_filename(&g_config_kegs_name[0], 56));

	cfg_htab_vtab(2, 2);
	cfg_printf("Current KEGS directory: %-50s",
			cfg_shorten_filename(&g_cfg_cwd_str[0], 50));

	cfg_htab_vtab(2, 3);

	str = "";
	if(g_cfg_file_pathfield) {
		str = "\b \b";
	}
	cfg_printf("Path: %s%s",
			cfg_shorten_filename(&g_cfg_file_curpath[0], 68), str);

	cfg_htab_vtab(0, 4);
	cfg_printf(" \t");
	for(x = 1; x < 79; x++) {
		cfg_putchar('\\');
	}
	cfg_printf("\t ");

	/* Force curent and topent to make sense */
	listhdrptr = &g_cfg_dirlist;
	num_to_show = CFG_NUM_SHOWENTS;
	yoffset = 5;
	if(g_cfg_select_partition > 0) {
		listhdrptr = &g_cfg_partitionlist;
		num_to_show -= 2;
		cfg_htab_vtab(2, yoffset);
		cfg_printf("Select partition of %-50s",
			cfg_shorten_filename(&g_cfg_file_path[0], 50), "");
		cfg_htab_vtab(2, yoffset + 1);
		cfg_printf("Current partition: %-50s",
			cfg_shorten_filename(&g_cfg_part_path[0], 50), "");
		yoffset += 2;
	}

	listhdrptr->num_to_show = num_to_show;
	cfg_fix_topent(listhdrptr);
	for(i = 0; i < num_to_show; i++) {
		y = i + yoffset;
		if(listhdrptr->last > (i + listhdrptr->topent)) {
			direntptr = &(listhdrptr->
					direntptr[i + listhdrptr->topent]);
			cfg_htab_vtab(2, y);
			if(direntptr->is_dir) {
				cfg_printf("\tXY\t ");
			} else {
				cfg_printf("   ");
			}
			if(direntptr->part_num >= 0) {
				cfg_printf("%3d: ", direntptr->part_num);
			}
			str = cfg_shorten_filename(direntptr->name, 50);
			fmt = "%-50s";
			if((i + listhdrptr->topent) == listhdrptr->curent) {
				if(g_cfg_file_pathfield == 0) {
					fmt = "\b%-50s\b";
				} else {
					fmt = "%-49s\b \b";
				}
				//printf("file highlight l %d top:%d cur:%d\n",
				//	i, listhdrptr->topent,
				//	listhdrptr->curent);
			}
			cfg_printf(fmt, str);
			if(!direntptr->is_dir) {
				cfg_print_dnum(direntptr->dsize, 18);
			}
			//printf(" :%s:%lld:\n", str, direntptr->dsize);
		}
	}

	cfg_htab_vtab(1, 5 + CFG_NUM_SHOWENTS);
	cfg_putchar('\t');
	for(x = 1; x < 79; x++) {
		cfg_putchar('L');
	}
	cfg_putchar('\t');

	//printf("cfg_file_draw done\n");
}

void
cfg_partition_select_all()
{
	word32	slot_drive;
	int	part_path_len, curent;

	slot_drive = g_cfg_slotdrive;
	part_path_len = (int)strlen(&g_cfg_part_path[0]);
	curent = g_cfg_partitionlist.curent;
	while(1) {
		g_cfg_slotdrive = slot_drive;
		g_cfg_partitionlist.curent = curent;
		cfg_partition_selected();
		if(g_cfg_slotdrive != 0) {
			// Something went wrong, get out
			return;
		}
		slot_drive++;
		curent++;
		g_cfg_part_path[part_path_len] = 0;
		if(curent >= g_cfg_partitionlist.last) {
			return;
		}
		if((slot_drive >> 8) == 7) {
			if((slot_drive & 0xff) >= MAX_C7_DISKS) {
				return;
			}
			if((slot_drive & 0xff) >= 12) {
				return;
			}
		} else if((slot_drive & 0xff) >= 2) {
			return;
		}
	}
}

void
cfg_partition_selected()
{
	char	*str;
	const char *part_str;
	char	*part_str2;
	int	pos;
	int	part_num;

	pos = g_cfg_partitionlist.curent;
	str = g_cfg_partitionlist.direntptr[pos].name;
	if(g_cfg_partitionlist.direntptr[pos].is_dir) {
		// Add this path to the partition path, and try again
		if(!strcmp(str, "../")) {
			/* go up one directory */
			cfg_get_base_path(&g_cfg_part_path[0],
						&g_cfg_part_path[0], 1);
		} else {
			cfg_strlcat(&(g_cfg_part_path[0]), str, CFG_PATH_MAX);
		}
		cfg_partition_make_list_from_name(&g_cfg_file_path[0]);
		return;
	}

	part_num = -2;
	part_str = 0;
	if(str[0] == 0 || (str[0] >= '0' && str[0] <= '9')) {
		part_num = g_cfg_partitionlist.direntptr[pos].part_num;
	} else {
		part_str = str;
	}
	part_str2 = 0;
	if(part_str != 0) {
		cfg_strlcat(&g_cfg_part_path[0], part_str, CFG_PATH_MAX);
		part_str2 = kegs_malloc_str(&g_cfg_part_path[0]);
		g_cfg_part_path[0] = 0;
	}
	printf("cfg_partition_selected, pos:%d, g_cfg_file_path[0]:%s, "
		"part:%s\n", pos, g_cfg_file_path, part_str2);

	insert_disk((g_cfg_slotdrive >> 8) & 0xf, g_cfg_slotdrive & 0xff,
			&(g_cfg_file_path[0]), 0, part_str2, part_num, 0);
	free(part_str2);
	g_cfg_slotdrive = 0;
	g_cfg_newdisk_select = 0;
	g_cfg_select_partition = -1;
}

void
cfg_file_selected()
{
	struct stat stat_buf;
	char	*str;
	int	fmt, stat_errno, is_cmd_key_down;
	int	ret;

	is_cmd_key_down = adb_is_cmd_key_down() &&
					((g_cfg_slotdrive & 0xfff) != 0xfff);
		// Cmd-Return means create DynaPro image when using slot/drive

	if(g_cfg_select_partition > 0) {
		cfg_partition_selected();
		return;
	}

	if(!is_cmd_key_down && (g_cfg_file_pathfield == 0)) {
		// in file section area of window
		str = g_cfg_dirlist.direntptr[g_cfg_dirlist.curent].name;
		if(!strcmp(str, "../")) {
			/* go up one directory */
			cfg_get_base_path(&g_cfg_file_curpath[0],
						&g_cfg_file_curpath[0], 1);
			return;
		}

		cfg_strncpy(&(g_cfg_file_path[0]), &(g_cfg_file_cachedreal[0]),
								CFG_PATH_MAX);
		cfg_strlcat(&(g_cfg_file_path[0]), str, CFG_PATH_MAX);
	} else {
		// just use cfg_file_curpath directly
		cfg_strncpy(&g_cfg_file_path[0], &g_cfg_file_curpath[0],
							CFG_PATH_MAX);
	}

	ret = cfg_stat(&g_cfg_file_path[0], &stat_buf, 0);
	stat_errno = errno;
	fmt = stat_buf.st_mode & S_IFMT;
	cfg_printf("Stat'ing %s, st_mode is: %08x\n", &g_cfg_file_path[0],
			(int)stat_buf.st_mode);

	if((ret == 0) && (fmt == S_IFDIR) && is_cmd_key_down &&
						(g_cfg_newdisk_select != 2)) {
		// Make a new DynaPro disk
		cfg_insert_disk_dynapro((g_cfg_slotdrive >> 8) & 0xf,
				g_cfg_slotdrive & 0xff, &g_cfg_file_path[0]);
		g_cfg_slotdrive = 0;			// End file selection
		g_cfg_newdisk_select = 0;
		g_menuptr = &g_cfg_disk_menu[0];
	} else if((g_cfg_newdisk_select == 1) && (g_cfg_newdisk_type == 3) &&
			g_cfg_file_pathfield && (fmt == S_IFDIR)) {
		// Special handling for Dynamic ProDOS directories.  User hit
		//  return in the Path field on a directory, use this directory
		cfg_create_new_image();
	} if(ret != 0) {
		if(g_cfg_newdisk_select && (g_cfg_newdisk_type != 3)) {
			// This looks good, a new file name was entered
			if(stat_errno == ENOENT) {
				cfg_create_new_image();
			} else {
				printf("Unknown errno:%d while checking %s\n",
					stat_errno, &g_cfg_file_path[0]);
			}
		} else {
			printf("stat %s returned %d, errno: %d\n",
					&g_cfg_file_path[0], ret, stat_errno);
		}
	} else if(fmt == S_IFDIR) {
		/* it's a directory */
		cfg_strncpy(&g_cfg_file_curpath[0], &g_cfg_file_path[0],
								CFG_PATH_MAX);
	} else if(g_cfg_newdisk_select) {
		// Do not allow selecting files, just ignore it
	} else if((g_cfg_slotdrive & 0xfff) < 0xfff) {
		/* select it */
		ret = cfg_maybe_insert_disk((g_cfg_slotdrive >> 8) & 0xf,
				g_cfg_slotdrive & 0xff, &g_cfg_file_path[0]);
		if(ret > 0) {
			g_cfg_slotdrive = 0;
			g_cfg_newdisk_select = 0;
		}
	} else {
		cfg_file_update_ptr(g_cfg_file_strptr, &g_cfg_file_path[0], 1);
		g_cfg_slotdrive = 0;
		g_cfg_newdisk_select = 0;
	}
}

void
cfg_file_handle_key(int key)
{
	Cfg_listhdr *listhdrptr;
	int	len, lowkey, got_match_key, is_cmd_key_down;

	// Modes: g_cfg_slotdrive: 1 to 0xfff: File selection dialog
	//		otherwise: normal menu being shown
	//	g_cfg_file_pathfield: File selection with cursor in Path: field
	//		otherwise: in scrolling file selection field
	//	g_cfg_select_partition: file selection for partition name
	if(g_cfg_file_pathfield) {
		if(key >= 0x20 && key < 0x7f) {
			len = (int)strlen(&g_cfg_file_curpath[0]);
			if(len < CFG_PATH_MAX-4) {
				g_cfg_file_curpath[len] = key;
				g_cfg_file_curpath[len+1] = 0;
			}
			return;
		}
	}

	listhdrptr = &g_cfg_dirlist;
	is_cmd_key_down = 0;
	if(g_cfg_select_partition > 0) {
		listhdrptr = &g_cfg_partitionlist;
		is_cmd_key_down = adb_is_cmd_key_down() &&
					((g_cfg_slotdrive & 0xfff) != 0xfff);
	}
	lowkey = tolower(key);
	got_match_key = 0;
	if((g_cfg_file_pathfield == 0) && (lowkey >= 'a') && (lowkey <= 'z') &&
						!is_cmd_key_down) {
		/* jump to file starting with this letter */
		g_cfg_file_match[0] = key;
		g_cfg_file_match[1] = 0;
		g_cfg_dirlist.invalid = 1;	/* re-read directory */
		got_match_key = 1;
	}

	switch(key) {
	case 0x1b:				// ESC
		if(((g_cfg_slotdrive & 0xfff) < 0xfff) &&
							!g_cfg_newdisk_select) {
			iwm_eject_disk_by_num((g_cfg_slotdrive >> 8) & 0xf,
						g_cfg_slotdrive & 0xff);
		}
		g_cfg_slotdrive = 0;
		g_cfg_select_partition = -1;
		g_cfg_dirlist.invalid = 1;
		g_cfg_newdisk_select = 0;
		break;
	case 0x0a:	/* down arrow */
		if(g_cfg_file_pathfield == 0) {
			listhdrptr->curent++;
			cfg_fix_topent(listhdrptr);
		}
		break;
	case 0x0b:	/* up arrow */
		if(g_cfg_file_pathfield == 0) {
			listhdrptr->curent--;
			cfg_fix_topent(listhdrptr);
		}
		break;
	case 0x0d:	/* return */
		//printf("handling return press\n");
		cfg_file_selected();
		break;
	case 0x61:	/* 'a' */
		if(is_cmd_key_down && (g_cfg_select_partition > 0)) {
			cfg_partition_select_all();
		}
		break;
	case 0x09:	/* tab */
		g_cfg_file_pathfield = !g_cfg_file_pathfield;
		if(g_cfg_select_partition > 0) {
			// If selecting file inside zip or partition, don't
			//  allow editing of the Path info
			g_cfg_file_pathfield = 0;
		}
		break;
	case 0x08:	/* left arrow */
	case 0x7f:	/* delete key */
		if(g_cfg_file_pathfield) {
			// printf("left arrow/delete\n");
			len = (int)strlen(&g_cfg_file_curpath[0]) - 1;
			if(len >= 0) {
				g_cfg_file_curpath[len] = 0;
			}
		}
		break;
	default:
		if(!got_match_key) {
			printf("key: %02x\n", key);
		}
	}
#if 0
	printf("curent: %d, topent: %d, last: %d\n",
		g_cfg_dirlist.curent, g_cfg_dirlist.topent, g_cfg_dirlist.last);
#endif
}

void
cfg_draw_menu()
{
	const char *str;
	Cfg_menu *menuptr;
	int	print_eject_help, line, type, match_found, menu_line, max_line;

	g_menu_redraw_needed = 0;

	menuptr = g_menuptr;
	if(menuptr == 0) {
		menuptr = g_cfg_main_menu;
	}
	if(g_rom_version < 0) {
		/* Must select ROM file */
		menuptr = g_cfg_rom_menu;
	}
	g_menuptr = menuptr;

	cfg_home();
	line = 1;
	max_line = 1;
	match_found = 0;
	print_eject_help = 0;
	menu_line = g_menu_line;
	cfg_printf("%s\n\n", menuptr[0].str);
	while(line < 24) {
		str = menuptr[line].str;
		type = menuptr[line].cfgtype;
		if(str == 0) {
			break;
		}
		if((type & 0xf) == CFGTYPE_DISK) {
			print_eject_help = 1;
		}
#if 0
		printf("Calling parse_menu line:%d, menu_line:%d, %p\n", line,
							menu_line, menuptr);
#endif
		cfg_parse_menu(menuptr, line, menu_line, 0);
		if(line == g_menu_line) {
			if(type != 0) {
				match_found = 1;
			} else if(g_menu_inc) {
				menu_line++;
			} else {
				menu_line--;
			}
		}
		if(line > max_line) {
			max_line = line;
		}

		cfg_printf("%s\n", g_cfg_opt_buf);
		line++;
	}
	if((menu_line < 1) && !match_found) {
		menu_line = 1;
	}
	if((menu_line >= max_line) && !match_found) {
		menu_line = max_line;
	}

	g_menu_line = menu_line;
	g_menu_max_line = max_line;
	if(!match_found) {
		g_menu_redraw_needed = 1;
	}
	if(g_rom_version < 0) {
		cfg_htab_vtab(0, 21);
		cfg_printf("\bYOU MUST SELECT A VALID ROM FILE\b\n");
	}

	cfg_htab_vtab(0, 23);
	cfg_printf("Move: \tJ\t \tK\t Change: \tH\t \tU\t \tM\t");
	if(print_eject_help) {
		cfg_printf("  Eject: ");
		if((g_cfg_slotdrive & 0xfff) > 0) {
			cfg_printf("\bESC\b");
		} else {
			cfg_printf("E");
			cfg_printf("  New image: N  Dup image: D  Verify: V");
		}
	}
	if((g_cfg_slotdrive & 0xfff) > 0) {
		cfg_printf("  Edit Path: \bTAB\b");
		if(g_cfg_select_partition > 0) {
			cfg_printf("  (\bCmd\b-\bA\b to mount all)");
		} else if((g_cfg_newdisk_type == 3) || !g_cfg_newdisk_select) {
			// Dynamic ProDOS, select a directory
			cfg_printf("  (\bCmd\b-\bEnter\b for DynaPro)");
		}
		if(g_cfg_newdisk_select && (g_cfg_newdisk_type != 3)) {
			cfg_printf("  (Enter new name on Path)");
		}
	}
#if 0
	cfg_htab_vtab(0, 22);
	cfg_printf("menu_line: %d line: %d, vbl:%d, adb:%d key_dn:%d\n",
			menu_line, line, g_cfg_vbl_count, g_adb_repeat_vbl,
			g_key_down);
#endif

	if((g_cfg_slotdrive & 0xfff) > 0) {
		cfg_file_draw();
	}
}

void
cfg_newdisk_pick_menu(word32 slotdrive)
{
	slotdrive = slotdrive & 0xfff;
	g_cfg_newdisk_slotdrive = slotdrive;	// 0x601: s6d2, 0x500: s5d1
	g_menu_line = 1;
	//printf("N key, g_menuptr=%p\n", g_menuptr);
	g_cfg_newdisk_type_default = 1;
	g_cfg_newdisk_type = 1;
	g_cfg_newdisk_blocks_default = 140*2;
	g_cfg_newdisk_blocks = 280;
	if((slotdrive >> 8) == 6) {
		g_menuptr = g_cfg_newslot6_menu;
	} else if((slotdrive >> 8) == 5) {
		g_menuptr = g_cfg_newslot5_menu;
		g_cfg_newdisk_blocks_default = 1600;
		g_cfg_newdisk_blocks = 1600;
	} else {
		g_menuptr = g_cfg_newslot7_menu;
		g_cfg_newdisk_blocks_default = 65535;
		g_cfg_newdisk_blocks = 65535;
	}
}

int
cfg_control_panel_update()
{
	int	ret;
	int	i;

	ret = cfg_control_panel_update1();
	if(g_cfg_screen_changed) {
		for(i = 0; i < 24; i++) {
			video_draw_a2_string(i, &g_cfg_screen[i][0]);
		}
	}
	g_cfg_screen_changed = 0;

	return ret;
}

void
cfg_edit_mode_key(int key)
{
	char	*new_str;
	int	*iptr;
	int	len, ival;

	len = (int)strlen(&g_cfg_edit_buf[0]);
	if(key == 0x0d) {		// Return
		// Try to accept the change
		new_str = kegs_malloc_str(&g_cfg_edit_buf[0]);
		if(g_cfg_edit_type == CFGTYPE_STR) {
			cfg_file_update_ptr(g_cfg_edit_ptr, new_str, 1);
		} else if(g_cfg_edit_type == CFGTYPE_INT) {
			ival = strtol(&g_cfg_edit_buf[0], 0, 0);
			iptr = (int *)g_cfg_edit_ptr;
			cfg_int_update(iptr, ival);
		}
		g_cfg_edit_ptr = 0;
		g_config_kegs_update_needed = 1;
	} else if(key == 0x1b) {	// ESC
		g_cfg_edit_ptr = 0;	// Abort out of edit mode, no changes
	} else if((key == 0x08) || (key == 0x7f)) {	// Left arrow or Delete
		len--;
		if(len >= 0) {
			g_cfg_edit_buf[len] = 0;
		}
	} else if((key >= 0x20) && (key < 0x7f)) {
		if(len < (CFG_OPT_MAXSTR - 3)) {
			g_cfg_edit_buf[len] = key;
			g_cfg_edit_buf[len+1] = 0;
		}
	}
}

int
cfg_control_panel_update1()
{
	char	*(*fn_ptr)(int);
	void	*ptr;
	char	**str_ptr;
	int	*iptr;
	int	type, key;

	while(g_config_control_panel) {
		if(g_menu_redraw_needed) {
			cfg_draw_menu();
		}
		if(g_menu_redraw_needed) {
			cfg_draw_menu();
		}
		key = adb_read_c000();
		if(key & 0x80) {
			key = key & 0x7f;
			(void)adb_access_c010();
		} else {
			return 0;		// No keys
		}
		g_menu_redraw_needed = 1;
		// If we get here, we got a key, figure out what to do with it
		if(g_cfg_slotdrive & 0xfff) {
			cfg_file_handle_key(key);
			continue;
		}
		if(g_cfg_edit_ptr) {
			cfg_edit_mode_key(key);
			continue;
		}

		// Normal menu system
		switch(key) {
		case 0x0a: /* down arrow */
			g_menu_line++;
			g_menu_inc = 1;
			break;
		case 0x0b: /* up arrow */
			g_menu_line--;
			g_menu_inc = 0;
			if(g_menu_line < 1) {
				g_menu_line = 1;
			}
			break;
		case 0x15: /* right arrow */
			cfg_parse_menu(g_menuptr, g_menu_line, g_menu_line, 1);
			break;
		case 0x08: /* left arrow */
			cfg_parse_menu(g_menuptr, g_menu_line, g_menu_line, -1);
			break;
		case 0x0d:
			type = g_menuptr[g_menu_line].cfgtype;
			ptr = g_menuptr[g_menu_line].ptr;
			switch(type & 0xf) {
			case CFGTYPE_MENU:
				g_menuptr = (Cfg_menu *)ptr;
				g_menu_line = 1;
				break;
			case CFGTYPE_DISK:
				g_cfg_slotdrive = (type >> 4) & 0xfff;
				cfg_file_init();
				break;
			case CFGTYPE_FUNC:
				fn_ptr = (char * (*)(int))ptr;
				(void)(*fn_ptr)(0);
				break;
			case CFGTYPE_FILE:
				g_cfg_slotdrive = 0xfff;
				g_cfg_file_def_name = *((char **)ptr);
				g_cfg_file_strptr = (char **)ptr;
				cfg_file_init();
				break;
			case CFGTYPE_STR:
				str_ptr = (char **)ptr;
				if(str_ptr) {
					g_cfg_edit_type = type & 0xf;
					g_cfg_edit_ptr = str_ptr;
					cfg_strncpy(&g_cfg_edit_buf[0],
						*str_ptr, CFG_OPT_MAXSTR);
				}
				break;
			case CFGTYPE_INT:
				// If there are no ',' in the menu str, then
				//  allow user to enter a manual number
				if(!strchr(g_menuptr[g_menu_line].str, ',')) {
					g_cfg_edit_type = type & 0xf;
					g_cfg_edit_ptr = ptr;
					iptr = (int *)ptr;
					snprintf(&g_cfg_edit_buf[0],
						CFG_OPT_MAXSTR, "%d", *iptr);
				}
			}
			break;
		case 0x1b:
			// Jump to last menu entry
			g_menu_line = g_menu_max_line;
			break;
		case 'd':
		case 'D':			// Duplicate an image
			type = g_menuptr[g_menu_line].cfgtype;
			if((type & 0xf) == CFGTYPE_DISK) {
				cfg_dup_existing_image(type >> 4);
			}
			break;
		case 'e':
		case 'E':
			type = g_menuptr[g_menu_line].cfgtype;
			if((type & 0xf) == CFGTYPE_DISK) {
				iwm_eject_disk_by_num(type >> 12,
							(type >> 4) & 0xff);
			}
			break;
		case 'l':
		case 'L':
			type = g_menuptr[g_menu_line].cfgtype;
			if((type & 0xf) == CFGTYPE_DISK) {
				cfg_toggle_lock_disk(type >> 4);
			}
			break;
		case 'n':
		case 'N':
			type = g_menuptr[g_menu_line].cfgtype;
			if((type & 0xf) == CFGTYPE_DISK) {
				cfg_newdisk_pick_menu(type >> 4);
			}
			break;
		case 'v':
		case 'V':
			type = g_menuptr[g_menu_line].cfgtype;
			if((type & 0xf) == CFGTYPE_DISK) {
				cfg_validate_image(type >> 4);
			}
			break;
		default:
			printf("key: %02x\n", key);
		}
	}

	return 0;
}

