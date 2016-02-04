/*
 GSPLUS - Advanced Apple IIGS Emulator Environment
 Copyright (C) 2016 - Dagen Brock

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

#include <assert.h>
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

int	g_sdlsnd_buflen = 0x1000;
word32 *bptr = NULL;
int g_sdlsnd_write_idx;
int g_sdlsnd_read_idx;

// just to makeit stand out
#ifdef HAVE_SDL
static Uint8 *audio_chunk;
static Uint8 *audio_pos;
static Uint32 audio_len;

static byte *sdlsnd_buf = 0;
SDL_AudioSpec want, have;
SDL_AudioDeviceID dev;

static byte *playbuf = 0;
static int g_playbuf_buffered = 0;
static SDL_AudioSpec spec;
static int snd_buf;
static int snd_write;           /* write position into playbuf */
static /* volatile */ int snd_read = 0;

static void _snd_callback(void*, Uint8 *stream, int len);
static void sound_write_sdl(int real_samps, int size);


#endif

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


void sdl_send_audio(word32	*ptr, int size, int real_samps) {

  /* code */
  //printf(" sdl_s_a %d\t 0x%08x ",size, &ptr);
  int i;
  for (i=0; i<size; i++) {
    if (real_samps) {
      bptr[g_sdlsnd_write_idx++] = ptr[i];
    } else {
      bptr[g_sdlsnd_write_idx++] = 0;
    }
    if (g_sdlsnd_write_idx>g_sdlsnd_buflen) {
      g_sdlsnd_write_idx=0;
    }
  }
  g_playbuf_buffered += size;

}

void handle_sdl_snd(void *userdata, Uint8 *stream, int len) {
  /* Only play if we have data left */
/*  if ( g_playbuf_buffered == 0) {
    return;
  }
*/
  for(int i = 0; i < len; ++i) {
    if(g_playbuf_buffered <= 0) {
        stream[i] = 0;
    } else {
        stream[i] = bptr[g_sdlsnd_read_idx++];
        if(g_sdlsnd_read_idx == g_sdlsnd_buflen)
            g_sdlsnd_read_idx = 0;
        g_playbuf_buffered--;
    }
  }
  return;


  /* Mix as much data as possible */
  len = ( len > g_playbuf_buffered ? g_playbuf_buffered : len );
  //len = ( g_sdlsnd_read_idx+len < g_sdlsnd_buflen ? len : g_sdlsnd_read_idx+len);
  SDL_memset(stream, 0, len);
  if (g_sdlsnd_read_idx+len < g_sdlsnd_buflen) {
    SDL_memcpy (stream, &bptr[g_sdlsnd_read_idx], len);
    g_sdlsnd_read_idx += len;
    g_playbuf_buffered -= len;
  } else {
    /*
    int top_len = 0;
    top_len = g_sdlsnd_buflen - g_sdlsnd_read_idx;
    SDL_memcpy (stream, &bptr[g_sdlsnd_read_idx], top_len);
    g_sdlsnd_read_idx = 0;
    g_playbuf_buffered -= top_len;
  //  SDL_memcpy (stream+top_len, bptr[g_sdlsnd_read_idx], len-top_len);
    g_sdlsnd_read_idx += len-top_len;
    g_playbuf_buffered -= len-top_len;
    */
  }

  //SDL_MixAudio(stream, pointer, len, SDL_MIX_MAXVOLUME);

}


void
child_sound_init_sdl()
{
  printf("child_sound_init_sdl");

  SDL_memset(&want, 0, sizeof(want)); // or SDL_zero(want)
  want.freq = g_preferred_rate;       // 48000 ?
  want.format = AUDIO_S16SYS;         // AUDIO_F32
  want.channels = NUM_CHANNELS;       //2
  want.samples = 512;                 //4096
  want.callback = handle_sdl_snd;  // you wrote this function elsewhere.

  dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
  if (dev == 0) {
      printf("Failed to open audio: %s\n", SDL_GetError());
  } else {
      if (have.format != want.format) { // we let this one thing change.
          printf("We didn't get Float32 audio format.\n");
      }

  }

  // super experimental unknown

  g_playbuf_buffered = 0;  // init buffered state

  int blen;
  blen = (SOUND_SHM_SAMP_SIZE * SAMPLE_CHAN_SIZE * 2);  // *2 unnecessary?
  g_sdlsnd_buflen = blen;
  bptr = (byte*)malloc(blen);
  if(bptr == NULL) {
    printf("Unabled to allocate sound buffer\n");
    exit(1);
  }
  memset(bptr, 0, SOUND_SHM_SAMP_SIZE*SAMPLE_CHAN_SIZE *2);  // zero out the buffer

  g_sdlsnd_write_idx = 0; // initialize
  g_sdlsnd_read_idx = 0;

  SDL_PauseAudioDevice(dev, 0);   // start audio playing.

	g_audio_rate = have.freq;
  printf("g_audio_rate: %d\n", g_audio_rate);
	set_audio_rate(g_audio_rate);   // let kegs simulator know the rate
}




void
sdlsnd_shutdown() {
  //SDL_Delay(5000);  // let the audio callback play some sound for 5 seconds.
  SDL_CloseAudioDevice(dev);
  printf("sdlsnd_shutdown");
}
