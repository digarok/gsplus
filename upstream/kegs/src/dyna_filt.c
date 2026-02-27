const char rcsid_dynafilt_c[] = "@(#)$KmKId: dyna_filt.c,v 1.1 2021-08-09 03:13:55+00 kentd Exp $";

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

#include "defc.h"

// Provide filters for Dynapro to use for copying files from the host to
//  a ProDOS volume, and for writing changes to the ProDOS volume back to
//  host files.

