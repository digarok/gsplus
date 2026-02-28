/**********************************************************************/
/*                    GSplus - Apple //gs Emulator                    */
/*                    Based on KEGS by Kent Dickey                    */
/*                    Copyright 2002-2020 Kent Dickey                 */
/*                    Copyright 2025-2026 GSplus Contributors         */
/*                                                                    */
/*      This code is covered by the GNU GPL v3                        */
/*      See the file COPYING.txt or https://www.gnu.org/licenses/     */
/**********************************************************************/


/* END_HDR */

/* pulseaudio_driver.c */
int pulse_audio_send_audio(byte *ptr, int in_size);
void pulse_audio_main_events(void);
void pulse_audio_write_to_stream(int dbg_count, int in_sz);
int pulse_audio_start_stream(void);
int pulse_audio_do_init(void);
int pulse_audio_init(void);
void pulse_audio_shutdown(void);

