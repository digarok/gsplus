/*
  GSPLUS - Advanced Apple IIGS Emulator Environment
  Based on the KEGS emulator written by Kent Dickey
  See COPYRIGHT.txt for Copyright information
	See COPYING.txt for license (GPL v2)
*/

/* END_HDR */

/* macsnd_driver.c */
void mac_snd_callback(SndChannelPtr snd_chan_ptr, SndCommand *in_sndcmd);
int mac_send_audio(byte *ptr, int in_size);
void child_sound_init_mac(void);
void macsnd_init(word32 *shmaddr);
