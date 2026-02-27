/************************************************************************/
/*			KEGS: Apple //gs Emulator			*/
/*			Copyright 2002-2020 by Kent Dickey		*/
/*									*/
/*	This code is covered by the GNU GPL v3				*/
/*	See the file COPYING.txt or https://www.gnu.org/licenses/	*/
/*	This program is provided with no warranty			*/
/*									*/
/*	The KEGS web page is kegs.sourceforge.net			*/
/*	You may contact the author at: kadickey@alumni.princeton.edu	*/
/************************************************************************/

const char rcsid_protos_pulseaudio_driver_h[] = "@(#)$KmKId: protos_pulseaudio_driver.h,v 1.4 2020-12-02 22:35:39+00 kentd Exp $";

/* END_HDR */

/* pulseaudio_driver.c */
int pulse_audio_send_audio(byte *ptr, int in_size);
void pulse_audio_main_events(void);
void pulse_audio_write_to_stream(int dbg_count, int in_sz);
int pulse_audio_start_stream(void);
int pulse_audio_do_init(void);
int pulse_audio_init(void);
void pulse_audio_shutdown(void);

