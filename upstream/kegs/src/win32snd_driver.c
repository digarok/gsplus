const char rcsid_win32snd_driver_c[] = "@(#)$KmKId: win32snd_driver.c,v 1.12 2023-05-19 14:01:33+00 kentd Exp $";

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

// Audio is sent every 1/60th of a second to win32_send_audio().  Play it
//  using waveOutWrite() as long as we've got at least 2/60th of a second
//  buffered--otherwise waveOutPause().  If we get more than 10 buffers
//  queued, drop buffers until we get down to 6 buffers queued again.

// Track headers completed in g_wavehdr_rd_pos using callback function.

#include "defc.h"
#include "sound.h"

#include <windows.h>
#include <mmsystem.h>

extern int Verbose;

extern int g_audio_rate;

unsigned int __stdcall child_sound_loop_win32(void *param);
void check_wave_error(int res, char *str);

#define NUM_WAVE_HEADERS	32

HWAVEOUT g_wave_handle;
WAVEHDR g_wavehdr[NUM_WAVE_HEADERS];
// Each header is for 1/60th of a second of sound (generally).  Pause
//  until 2 headers are available, then unpause.  Experimentally it appears
//  we keep about 5 headers (5/60th of a second = 80msec) ahead, which is
//  excellent latency

extern int g_audio_enable;
extern word32 *g_sound_shm_addr;
extern int g_preferred_rate;

int	g_win32snd_buflen = 0x1000;
int	g_win32_snd_playing = 0;
int	g_win32_snd_to_drop = 0;
word32	g_win32_snd_dropped = 0;
volatile int g_wavehdr_rd_pos = 0;
volatile int g_wavehdr_wr_pos = 0;

void
win32snd_init(word32 *shmaddr)
{
	printf("win32snd_init\n");
	child_sound_init_win32();
}

void
win32snd_shutdown()
{
	/* hmm */

}

void CALLBACK
handle_wav_snd(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance,
					DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
	LPWAVEHDR lpwavehdr;
	int	pos;

	/* Only service "buffer done playing messages */
	if(uMsg == WOM_DONE) {
		lpwavehdr = (LPWAVEHDR)dwParam1;
		if(lpwavehdr->dwFlags == (WHDR_DONE | WHDR_PREPARED)) {
			lpwavehdr->dwUser = FALSE;
		}
		pos = (int)(lpwavehdr - &g_wavehdr[0]);
		// printf("At %.3f, pos %d is done\n", get_dtime(), pos);
		if(pos == g_wavehdr_rd_pos) {
			pos = (pos + 1) % NUM_WAVE_HEADERS;
			g_wavehdr_rd_pos = pos;
		} else {
			printf("wavehdr %d finished, exp %d\n", pos,
							g_wavehdr_rd_pos);
		}
	}

	return;
}


void
check_wave_error(int res, char *str)
{
	char	buf[256];

	if(res == MMSYSERR_NOERROR) {
		return;
	}

	waveOutGetErrorText(res, &buf[0], sizeof(buf));
	printf("%s: %s\n", str, buf);
	exit(1);
}

void
child_sound_init_win32()
{
	WAVEFORMATEX wavefmt;
	WAVEOUTCAPS caps;
	byte	*bptr;
	UINT	wave_id;
	int	bits_per_sample, channels, block_align, blen, res;
	int	i;

	memset(&wavefmt, 0, sizeof(WAVEFORMATEX));

	wavefmt.wFormatTag = WAVE_FORMAT_PCM;
	bits_per_sample = 16;
	channels = 2;
	wavefmt.wBitsPerSample = bits_per_sample;
	wavefmt.nChannels = channels;
	wavefmt.nSamplesPerSec = g_preferred_rate;
	block_align = channels * (bits_per_sample / 8);
	wavefmt.nBlockAlign = block_align;
	wavefmt.nAvgBytesPerSec = block_align * g_audio_rate;

	res = waveOutOpen(&g_wave_handle, WAVE_MAPPER, &wavefmt, 0, 0,
				WAVE_FORMAT_QUERY);

	if(res != MMSYSERR_NOERROR) {
		printf("Cannot open audio device, res:%d, g_audio_rate:%d\n",
						res, g_preferred_rate);
		g_audio_enable = 0;
		return;
	}

	res = waveOutOpen(&g_wave_handle, WAVE_MAPPER, &wavefmt,
		(DWORD_PTR)handle_wav_snd, 0,
		CALLBACK_FUNCTION | WAVE_ALLOWSYNC);

	if(res != MMSYSERR_NOERROR) {
		printf("Cannot register audio\n");
		g_audio_enable = 0;
		return;
	}

	g_audio_rate = wavefmt.nSamplesPerSec;
	blen = (((g_audio_rate * block_align) / 60) * 5) / 4;
		// Size buffer 25% larger than expected, to add some margin
	blen = (blen + 15) & -16L;

	g_win32snd_buflen = blen;
	bptr = malloc(blen * NUM_WAVE_HEADERS);
	if(bptr == NULL) {
		printf("Unabled to allocate sound buffer\n");
		exit(1);
	}

	for(i = 0; i < NUM_WAVE_HEADERS; i++) {
		memset(&g_wavehdr[i], 0, sizeof(WAVEHDR));
		g_wavehdr[i].dwUser = FALSE;
		g_wavehdr[i].lpData = (char *)&(bptr[i * blen]);
		g_wavehdr[i].dwBufferLength = blen;
		g_wavehdr[i].dwFlags = 0;
		g_wavehdr[i].dwLoops = 0;
		res = waveOutPrepareHeader(g_wave_handle, &g_wavehdr[i],
						sizeof(WAVEHDR));
		check_wave_error(res, "waveOutPrepareHeader");
	}

	res = waveOutGetID(g_wave_handle, &wave_id);
	res = waveOutGetDevCaps(wave_id, &caps, sizeof(caps));
	check_wave_error(res, "waveOutGetDevCaps");
	printf("Using %s, buflen:%d\n", caps.szPname, g_win32snd_buflen);
	printf(" Bits per Sample = %d.  Channels = %d\n",
		wavefmt.wBitsPerSample, wavefmt.nChannels);
	printf(" Sampling rate = %d, avg_bytes_per_sec = %d\n",
		(int)wavefmt.nSamplesPerSec, (int)wavefmt.nAvgBytesPerSec);

	sound_set_audio_rate(g_audio_rate);
}

void
win32snd_set_playing(int snd_playing)
{
	g_win32_snd_playing = snd_playing;
	if(snd_playing) {
		waveOutRestart(g_wave_handle);
#if 0
		printf("win32 restarted sound wr:%d rd:%d\n", g_wavehdr_wr_pos,
							g_wavehdr_rd_pos);
#endif
	} else {
		waveOutPause(g_wave_handle);
#if 0
		printf("win32 paused sound wr:%d rd:%d\n", g_wavehdr_wr_pos,
							g_wavehdr_rd_pos);
#endif
	}
}

void
win32_send_audio2(byte *ptr, int size)
{
	int	res, wr_pos, rd_pos, new_pos, bufs_in_use;

	wr_pos = g_wavehdr_wr_pos;
	rd_pos = g_wavehdr_rd_pos;
#if 0
	if(wr_pos == 0) {
		printf("send_audio2 wr:%d rd:%d sz:%d at %.3f\n", wr_pos,
				rd_pos, size, get_dtime());
	}
#endif
	if(g_wavehdr[wr_pos].dwUser != FALSE) {
		// Audio buffer busy...should not happen!
		printf("Audio buffer %d is busy!\n", wr_pos);
		return;
	}

	bufs_in_use = (NUM_WAVE_HEADERS + wr_pos - rd_pos) % NUM_WAVE_HEADERS;
	if(g_win32_snd_to_drop) {
		g_win32_snd_to_drop--;
		g_win32_snd_dropped += size;
		if((bufs_in_use < 4) && (g_win32_snd_to_drop != 0)) {
#if 0
			printf("bufs_in_use:%d, snd_to_drop:%d\n", bufs_in_use,
							g_win32_snd_to_drop);
#endif
			g_win32_snd_to_drop = 0;
		}
		if(g_win32_snd_to_drop == 0) {
			printf("Dropped %d bytes of sound\n",
						g_win32_snd_dropped);
		}
		return;
	}
#if 0
	if(g_win32_snd_playing && (bufs_in_use <= 2)) {
		printf("bufs_in_use:%d, wr:%d, rd:%d\n", bufs_in_use, wr_pos,
								rd_pos);
	}
#endif
	if(bufs_in_use == 0) {
#if 0
		printf("bufs_in_use:%d, wr_pos:%d rd_pos:%d\n", bufs_in_use,
			wr_pos, rd_pos);
#endif
		// We've underflowed, so pause sound until we get some buffered
		win32snd_set_playing(0);
	} else if(g_win32_snd_playing == 0) {
		if(bufs_in_use >= 2) {
			//printf("bufs_in_use:%d, will start\n", bufs_in_use);
			win32snd_set_playing(1);
		}
	} else {
		if(bufs_in_use >= 14) {		// About 230msec
			// Drop 6 buffers to get us back down to 100msec delay
			printf("bufs_in_use:%d, wr:%d will drop 6\n",
							bufs_in_use, wr_pos);
			g_win32_snd_to_drop = 6;
			g_win32_snd_dropped = 0;
		}
	}
	memcpy(g_wavehdr[wr_pos].lpData, ptr, size);
	g_wavehdr[wr_pos].dwBufferLength = size;
	g_wavehdr[wr_pos].dwUser = TRUE;

	new_pos = (wr_pos + 1) % NUM_WAVE_HEADERS;
	g_wavehdr_wr_pos = new_pos;
	res = waveOutWrite(g_wave_handle, &g_wavehdr[wr_pos],
							sizeof(g_wavehdr));
	check_wave_error(res, "waveOutWrite");

	return;
}

int
win32_send_audio(byte *ptr, int in_size)
{
	int	size;
	int	tmpsize;

	// printf("send_audio %d bytes at %.3f\n", in_size, get_dtime());
	size = in_size;
	while(size > 0) {
		tmpsize = size;
		if(size > g_win32snd_buflen) {
			tmpsize = g_win32snd_buflen;
		}
		win32_send_audio2(ptr, tmpsize);
		ptr += tmpsize;
		if(size != tmpsize) {
#if 0
			printf("Orig size:%d, reduced to %d\n", in_size,
								tmpsize);
#endif
		}
		size = size - tmpsize;
	}

	return in_size;
}

