/*
 GSPLUS - Advanced Apple IIGS Emulator Environment
 Copyright (C) 2016 - Dagen Brock
 
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

#include "SDL.h"
#include "defc.h"

#include "sound.h"
#include <assert.h>
extern word32	*g_sound_shm_addr;
extern int	g_sound_shm_pos;
extern int	g_audio_enable;
extern int g_preferred_rate;

static byte *playbuf = 0;
static int g_playbuf_buffered = 0;
static SDL_AudioSpec spec;
static int snd_buf;
static int snd_write;           /* write position into playbuf */
static /* volatile */ int snd_read = 0;
static int g_sound_paused;
static int g_zeroes_buffered;
static int g_zeroes_seen;

void sdlsnd_init(word32 *shmaddr)
{
  printf("sdlsnd_init\n");
  if (SDL_Init(SDL_INIT_AUDIO) < 0) {
		printf("Cannot initialize SDL audio\n");
		g_audio_enable = 0;
	} else {
		printf("SDL AUDIO INITIALIZED\n");
	}

  child_sound_loop(-1, -1, shmaddr);
  return;
}


void
sound_write_sdl(int real_samps, int size)
{

#ifdef HAVE_SDL
    int shm_read;

    if (real_samps) {
        shm_read = (g_sound_shm_pos - size + SOUND_SHM_SAMP_SIZE)%SOUND_SHM_SAMP_SIZE;
        SDL_LockAudio();
        while(size > 0) {
            if(g_playbuf_buffered >= snd_buf) {
                printf("sound_write_sdl failed @%d, %d buffered, %d samples skipped\n",snd_write,g_playbuf_buffered, size);
                shm_read += size;
                shm_read %= SOUND_SHM_SAMP_SIZE;
                size = 0;
            } else {
                ((word32*)playbuf)[snd_write/SAMPLE_CHAN_SIZE] = g_sound_shm_addr[shm_read];
                shm_read++;
                if (shm_read >= SOUND_SHM_SAMP_SIZE)
                    shm_read = 0;
                snd_write += SAMPLE_CHAN_SIZE;
                if (snd_write >= snd_buf)
                    snd_write = 0;
                size--;
                g_playbuf_buffered += SAMPLE_CHAN_SIZE;
            }
        }

        assert((snd_buf+snd_write - snd_read)%snd_buf == g_playbuf_buffered%snd_buf);
        assert(g_sound_shm_pos == shm_read);
        SDL_UnlockAudio();
    }
    if(g_sound_paused && (g_playbuf_buffered > 0)) {
        printf("Unpausing sound, %d buffered\n",g_playbuf_buffered);
        g_sound_paused = 0;
        SDL_PauseAudio(0);
    }
    if(!g_sound_paused && (g_playbuf_buffered <= 0)) {
        printf("Pausing sound\n");
        g_sound_paused = 1;
        SDL_PauseAudio(1);
    }
#endif
}

#ifdef HAVE_SDL
/* Callback for sound */
static void _snd_callback(void* userdata, Uint8 *stream, int len)
{
    int i;
    /* Slurp off the play buffer */
    assert((snd_buf+snd_write - snd_read)%snd_buf == g_playbuf_buffered%snd_buf);
    /*printf("slurp %d, %d buffered\n",len, g_playbuf_buffered);*/
    for(i = 0; i < len; ++i) {
        if(g_playbuf_buffered <= 0) {
            stream[i] = 0;
        } else {
            stream[i] = playbuf[snd_read++];
            if(snd_read == snd_buf)
                snd_read = 0;
            g_playbuf_buffered--;
        }
    }
#if 0
    if (g_playbuf_buffered <= 0) {
        printf("snd_callback: buffer empty, Pausing sound\n");
        g_sound_paused = 1;
        SDL_PauseAudio(1);
    }
#endif
    //printf("end slurp %d, %d buffered\n",len, g_playbuf_buffered);
}
#endif




long
sound_init_device_sdl()
{
#ifdef HAVE_SDL
    long rate;
    SDL_AudioSpec wanted;

    //if(SDL_InitSubSystem(SDL_INIT_AUDIO)) {
	  if (SDL_Init(SDL_INIT_AUDIO) < 0) {
      fprintf(stderr, "sdl: Couldn't init SDL_Audio: %s!\n", SDL_GetError());
      return 0;
    }

    /* Set the desired format */
    wanted.freq = g_preferred_rate;
    wanted.format = AUDIO_S16SYS;
    wanted.channels = NUM_CHANNELS;
    wanted.samples = 512;
    wanted.callback = _snd_callback;
    wanted.userdata = NULL;

    /* Open audio, and get the real spec */
    if(SDL_OpenAudio(&wanted, &spec) < 0) {
        fprintf(stderr, "sdl: Couldn't open audio: %s!\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return 0;
    }
    /* Check everything */
    if(spec.channels != wanted.channels) {
        fprintf(stderr, "sdl: Couldn't get stereo audio format!\n");
        goto snd_error;
    }
    if(spec.format != wanted.format) {
        fprintf(stderr, "sdl: Couldn't get a supported audio format!\n");
        fprintf(stderr, "sdl: wanted %X, got %X\n",wanted.format,spec.format);
        goto snd_error;
    }
    if(spec.freq != wanted.freq) {
        fprintf(stderr, "sdl: wanted rate = %d, got rate = %d\n", wanted.freq, spec.freq);
    }
    /* Set things as they really are */
    rate = spec.freq;

    snd_buf = SOUND_SHM_SAMP_SIZE*SAMPLE_CHAN_SIZE;
    playbuf = (byte*) malloc(snd_buf);
    if (!playbuf)
        goto snd_error;
    g_playbuf_buffered = 0;

    printf ("Sound shared memory size=%d\n",
            SOUND_SHM_SAMP_SIZE * SAMPLE_CHAN_SIZE);

    g_sound_shm_addr = malloc(SOUND_SHM_SAMP_SIZE * SAMPLE_CHAN_SIZE);
    memset(g_sound_shm_addr,0,SOUND_SHM_SAMP_SIZE * SAMPLE_CHAN_SIZE);

    /* It's all good! */
    g_zeroes_buffered = 0;
    g_zeroes_seen = 0;
    /* Let's start playing sound */
    g_sound_paused = 0;
    SDL_PauseAudio(0);

		set_audio_rate(rate);
		return rate;

  snd_error:
    /* Oops! Something bad happened, cleanup. */
    SDL_CloseAudio();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    if(playbuf)
        free((void*)playbuf);
    playbuf = 0;
    snd_buf = 0;
    return 0;
#else
    return 0;
#endif
}

void
sound_shutdown_sdl()
{
#ifdef HAVE_SDL
    SDL_CloseAudio();
    if(playbuf)
        free((void*)playbuf);
    playbuf = 0;
#endif
}

void
sound_shutdown2()
{
	sound_shutdown_sdl();
	if (g_sound_shm_addr)
	{
		free(g_sound_shm_addr);
		g_sound_shm_addr=NULL;
	}
}
