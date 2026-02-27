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

const char rcsid_protos_macsnd_driver_h[] = "@(#)$KmKId: protos_macsnd_driver.h,v 1.9 2023-05-04 19:35:29+00 kentd Exp $";

/* END_HDR */

/* macsnd_driver.c */
int mac_send_audio(byte *ptr, int in_size);
void macsnd_init(void);


