#ifdef INCLUDE_RCSID_C
const char rcsid_config_h[] = "@(#)$KmKId: config.h,v 1.12 2023-08-28 01:59:55+00 kentd Exp $";
#endif

/************************************************************************/
/*			KEGS: Apple //gs Emulator			*/
/*			Copyright 2002-2019 by Kent Dickey		*/
/*									*/
/*	This code is covered by the GNU GPL v3				*/
/*	See the file COPYING.txt or https://www.gnu.org/licenses/	*/
/*	This program is provided with no warranty			*/
/*									*/
/*	The KEGS web page is kegs.sourceforge.net			*/
/*	You may contact the author at: kadickey@alumni.princeton.edu	*/
/************************************************************************/

#define CONF_BUF_LEN		1024
#define COPY_BUF_SIZE		4096
#define CFG_PRINTF_BUFSIZE	2048

#define CFG_PATH_MAX		1024

#define CFG_NUM_SHOWENTS	16

#define CFGTYPE_MENU		1
#define CFGTYPE_INT		2
#define CFGTYPE_DISK		3
#define CFGTYPE_FUNC		4
#define CFGTYPE_FILE		5
#define CFGTYPE_STR		6
/* CFGTYPE limited to just 4 bits: 0-15 */

/* Cfg_menu, Cfg_dirent and Cfg_listhdr are defined in defc.h */

STRUCT(Cfg_defval) {
	Cfg_menu *menuptr;
	int	intval;
	char	*strval;
};
