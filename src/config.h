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
#define CFGTYPE_STR			6
/* CFGTYPE limited to just 4 bits: 0-15 */

/* Cfg_menu, Cfg_dirent and Cfg_listhdr are defined in defc.h */

STRUCT(Cfg_defval) {
	Cfg_menu *menuptr;
	int	intval;
	char	*strval;
};
