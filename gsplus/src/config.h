#ifdef INCLUDE_RCSID_C
#endif

/**********************************************************************/
/*                    GSplus - Apple //gs Emulator                    */
/*                    Based on KEGS by Kent Dickey                    */
/*                    Copyright 2002-2019 Kent Dickey                 */
/*                    Copyright 2025-2026 GSplus Contributors         */
/*                                                                    */
/*      This code is covered by the GNU GPL v3                        */
/*      See the file COPYING.txt or https://www.gnu.org/licenses/     */
/**********************************************************************/

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
