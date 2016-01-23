/*
 GSport - an Apple //gs Emulator
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

#include "defc.h"
#include "sound.h"
#include "SDL.h"

extern int Verbose;

extern int g_audio_rate;

unsigned int __stdcall child_sound_loop_win32(void *param);
void check_wave_error(int res, char *str);

#define NUM_WAVE_HEADERS	8

//HWAVEOUT g_wave_handle = NULL;	// OG Default value must be set
//WAVEHDR g_wavehdr[NUM_WAVE_HEADERS];

extern int g_audio_enable;
extern word32 *g_sound_shm_addr;
extern int g_preferred_rate;

int	g_win32snd_buflen = 0x1000;

SDL_AudioSpec want, have;
SDL_AudioDeviceID dev;

void sdlsnd_init(word32 *shmaddr)
{
  printf("sdlsnd_init\n");
  if (SDL_Init(SDL_INIT_AUDIO) < 0) {
		printf("Cannot initialize SDL audio\n");
		g_audio_enable = 0;
	}
  child_sound_loop(-1, -1, shmaddr);
  return;
}

void
win32snd_init(word32 *shmaddr)
{
	printf("win32snd_init\n");
	child_sound_loop(-1, -1, shmaddr);

	return;
}


// OG Added global to free the dedicated win32 sound memory
byte	*bptr = NULL;

// OG shut win32 sound resources
void
win32snd_shutdown()
{
/*
	//if (g_wave_handle)  //////
  if (false)
	{
		MMRESULT	res = waveOutReset(g_wave_handle);
		if (res!=MMSYSERR_NOERROR )
			printf("waveOutReset Failed");

		res = waveOutClose(g_wave_handle);
		if (res!=MMSYSERR_NOERROR )
			printf("waveOutClose Failed");
		g_wave_handle=NULL;
}
	// OG Free dedicated sound memory
	if (bptr)
	{
		free(bptr);
		bptr = NULL;
	}
*/
}

/*
void CALLBACK
handle_wav_snd(HWAVEOUT hwo, UINT uMsg, DWORD dwInstance, DWORD dwParam1,
		DWORD dwParam2)
{
	LPWAVEHDR	lpwavehdr;

	// Only service "buffer done playing messages
	if(uMsg == WOM_DONE) {
		lpwavehdr = (LPWAVEHDR)dwParam1;
		if(lpwavehdr->dwFlags == (WHDR_DONE | WHDR_PREPARED)) {
			lpwavehdr->dwUser = FALSE;
		}
	}

	return;
}
*/
void
check_wave_error(int res, char *str)
{
  /*
	TCHAR	buf[256];

	if(res == MMSYSERR_NOERROR) {
		return;
	}

	waveOutGetErrorText(res, &buf[0], sizeof(buf));
	printf("%s: %s\n", str, buf);
  */
	exit(1);
}
void handle_sdl_snd(/* arguments */) {
  /* code */
}
void
sdlsnd_shutdown() {
  //SDL_Delay(5000);  // let the audio callback play some sound for 5 seconds.
  SDL_CloseAudioDevice(dev);
  printf("sdlsnd_shutdown");
}


void
child_sound_init_sdl()
{
  printf("child_sound_init_sdl");

  SDL_memset(&want, 0, sizeof(want)); // or SDL_zero(want)
  want.freq = 48000;
  want.format = AUDIO_F32;
  want.channels = 2;
  want.samples = 4096;
  want.callback = handle_sdl_snd;  // you wrote this function elsewhere.

  dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
  if (dev == 0) {
      printf("Failed to open audio: %s\n", SDL_GetError());
  } else {
      if (have.format != want.format) { // we let this one thing change.
          printf("We didn't get Float32 audio format.\n");
      }
      SDL_PauseAudioDevice(dev, 0); // start audio playing.
  }

  // UNKNOWN?
	g_audio_rate = have.freq;
  printf("g_audio_rate: %d\n", g_audio_rate);
	set_audio_rate(g_audio_rate);
}



void
child_sound_init_win32()
{
  /*
	WAVEFORMATEX wavefmt;
	WAVEOUTCAPS caps;

// OG Moved as global variable (to rename)
//	byte	*bptr;
	int	bits_per_sample, channels, block_align;
	int	blen;
	int	res;
	int	i;

	memset(&wavefmt, 0, sizeof(WAVEFORMATEX));

	wavefmt.wFormatTag = WAVE_FORMAT_PCM;
#ifndef UNDER_CE
	bits_per_sample = 16;
	wavefmt.nSamplesPerSec = g_audio_rate;
#else
	bits_per_sample = 16;
	wavefmt.nSamplesPerSec = 12000;
#endif

	channels = 2;
	wavefmt.wBitsPerSample = bits_per_sample;
	wavefmt.nChannels = channels;
	block_align = channels * (bits_per_sample / 8);
	wavefmt.nBlockAlign = block_align;
	wavefmt.nAvgBytesPerSec = block_align * g_audio_rate;

	res = waveOutOpen(&g_wave_handle, WAVE_MAPPER, &wavefmt, 0, 0,
				WAVE_FORMAT_QUERY);

	if(res != MMSYSERR_NOERROR) {
		printf("Cannot open audio device\n");
		g_audio_enable = 0;
		return;
	}

	res = waveOutOpen(&g_wave_handle, WAVE_MAPPER, &wavefmt,
		(DWORD)handle_wav_snd, 0, CALLBACK_FUNCTION | WAVE_ALLOWSYNC);

	if(res != MMSYSERR_NOERROR) {
		printf("Cannot register audio\n");
		g_audio_enable = 0;
		return;
	}

	g_audio_rate = wavefmt.nSamplesPerSec;

	blen = (SOUND_SHM_SAMP_SIZE * 4 * 2) / NUM_WAVE_HEADERS;
	g_win32snd_buflen = blen;
	bptr = (byte*)malloc(blen * NUM_WAVE_HEADERS); // OG Added cast
	if(bptr == NULL) {
		printf("Unabled to allocate sound buffer\n");
		exit(1);
	}

	for(i = 0; i < NUM_WAVE_HEADERS; i++) {
		memset(&g_wavehdr[i], 0, sizeof(WAVEHDR));
		g_wavehdr[i].dwUser = FALSE;
		g_wavehdr[i].lpData = (LPSTR)&(bptr[i*blen]);	// OG Added cast
		g_wavehdr[i].dwBufferLength = blen;
		g_wavehdr[i].dwFlags = 0;
		g_wavehdr[i].dwLoops = 0;
		res = waveOutPrepareHeader(g_wave_handle, &g_wavehdr[i],
						sizeof(WAVEHDR));
		check_wave_error(res, "waveOutPrepareHeader");
	}

	res = waveOutGetDevCaps((UINT)g_wave_handle, &caps, sizeof(caps));
	check_wave_error(res, "waveOutGetDevCaps");
	printf("Using %s\n", caps.szPname);
	printf(" Bits per Sample = %d.  Channels = %d\n",
		wavefmt.wBitsPerSample, wavefmt.nChannels);
	printf(" Sampling rate = %d, avg_bytes_per_sec = %d\n",
		(int)wavefmt.nSamplesPerSec, (int)wavefmt.nAvgBytesPerSec);

	set_audio_rate(g_audio_rate);
*/
}

void sdl_send_audio(byte *ptr, int size) {
  /* code */
  printf("  sdl_send_audio  ");
}

void
win32_send_audio2(byte *ptr, int size)
{
  /*
	int	found;
	int	res;
	int	i;

	found = 0;
	for(i = 0; i < NUM_WAVE_HEADERS; i++) {
		if(g_wavehdr[i].dwUser == FALSE) {
			found = 1;
			break;
		}
	}

	if(!found) {
		//  all audio buffers busy, just get out
		return;
	}

	memcpy(g_wavehdr[i].lpData, ptr, size);
	g_wavehdr[i].dwBufferLength = size;
	g_wavehdr[i].dwUser = TRUE;

	res = waveOutWrite(g_wave_handle, &g_wavehdr[i], sizeof(g_wavehdr));
	check_wave_error(res, "waveOutWrite");
  */
	return;
}

int
win32_send_audio(byte *ptr, int in_size)
{
	int	size;
	int	tmpsize;

	size = in_size;
	while(size > 0) {
		tmpsize = size;
		if(size > g_win32snd_buflen) {
			tmpsize = g_win32snd_buflen;
		}
		win32_send_audio2(ptr, tmpsize);
		ptr += tmpsize;
		size = size - tmpsize;
	}

	return in_size;
}
