/**********************************************************************/
/*                    GSplus - Apple //gs Emulator                    */
/*                    Based on KEGS by Kent Dickey                    */
/*      This code is covered by the GNU GPL v3                        */
/**********************************************************************/

/* SDL3 audio backend.
 *
 * The core PUSHES audio: each frame sound_driver.c's child_sound_playit() ->
 * reliable_buf_write() -> child_send_samples() hands us interleaved 16-bit
 * stereo samples (4 bytes per frame) at g_preferred_rate. We feed those straight
 * into an SDL3 SDL_AudioStream, which buffers (and resamples if needed) and
 * plays them -- so we don't need the manual ring buffer the old CoreAudio /
 * SDL2 backends maintained.
 */

#include <SDL3/SDL.h>

#include "defc.h"
#include "protos_sdl.h"

extern int g_preferred_rate;		/* desired sample rate, set by the core */

static SDL_AudioStream *g_sdl_audio_stream = NULL;

/* Push `bytes` of silence into the stream, to rebuild a latency cushion when
 * we're about to underrun. Done in chunks so we don't need a huge buffer. */
static void
sdl_put_silence(int bytes)
{
	byte	zerobuf[4096];
	int	chunk;

	memset(zerobuf, 0, sizeof(zerobuf));
	while(bytes > 0) {
		chunk = bytes;
		if(chunk > (int)sizeof(zerobuf)) {
			chunk = (int)sizeof(zerobuf);
		}
		SDL_PutAudioStreamData(g_sdl_audio_stream, zerobuf, chunk);
		bytes -= chunk;
	}
}

void
sdl_snd_init(word32 *shmaddr)
{
	SDL_AudioSpec spec;

	(void)shmaddr;			/* push model: we get samples via sdl_send_audio */

	if(!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
		printf("SDL_InitSubSystem(AUDIO) failed: %s\n", SDL_GetError());
		sound_set_audio_rate(g_preferred_rate);
		return;
	}

	spec.format = SDL_AUDIO_S16;	/* native-endian 16-bit, matches the core */
	spec.channels = 2;
	spec.freq = g_preferred_rate;

	/* NULL callback => we drive it by pushing data with SDL_PutAudioStreamData. */
	g_sdl_audio_stream = SDL_OpenAudioDeviceStream(
				SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
	if(!g_sdl_audio_stream) {
		printf("SDL_OpenAudioDeviceStream failed: %s\n", SDL_GetError());
		sound_set_audio_rate(g_preferred_rate);
		return;
	}

	/* Streams open paused; start playback. */
	SDL_ResumeAudioStreamDevice(g_sdl_audio_stream);
	sound_set_audio_rate(g_preferred_rate);
	printf("sdl_snd_init: SDL3 audio stream open, rate=%d\n", g_preferred_rate);
}

int
sdl_send_audio(byte *ptr, int size)
{
	int	bytes_per_sec, target, high, queued;

	if(g_sdl_audio_stream) {
		/* SDL's audio thread drains this queue at a steady rate on its own
		 * thread, decoupled from our main-loop pushes. The emulator feeds us
		 * once per frame from run_16ms(), paced to realtime by micro_sleep().
		 * With the Windows timer resolution raised to 1ms (see clock.c) that
		 * pacing is smooth, so under normal play the queue neither empties nor
		 * grows. The two clamps below are just backstops. (stereo S16 = 4
		 * bytes/frame) */
		bytes_per_sec = g_preferred_rate * 4;
		target = bytes_per_sec / 8;	/* ~125ms cushion to rebuild after a dropout */
		high   = bytes_per_sec / 2;	/* ~500ms: too far ahead, cap latency */

		queued = (int)SDL_GetAudioStreamQueued(g_sdl_audio_stream);

		/* True underrun only: the queue has fully drained, so SDL is already
		 * outputting silence and a gap exists regardless. Append a cushion of
		 * silence *before* the next real samples to avoid immediately
		 * re-underrunning on the next bit of jitter. We deliberately do NOT do
		 * this while queued > 0: SDL_PutAudioStreamData appends, so silence
		 * inserted then would land between real audio already buffered and the
		 * new samples -- a mid-stream discontinuity, i.e. an audible click. */
		if(queued == 0) {
			sdl_put_silence(target);
		}

		/* Don't let latency grow without bound when we're running ahead: drop
		 * these new samples rather than SDL_ClearAudioStream()ing the whole
		 * backlog (clearing empties the buffer and causes a gap). */
		if(queued <= high) {
			SDL_PutAudioStreamData(g_sdl_audio_stream, ptr, size);
		}
	}

	/* MUST be >= 0: the core's reliable_buf_write() calls exit(1) otherwise. */
	return size;
}

void
sdl_snd_shutdown(void)
{
	if(g_sdl_audio_stream) {
		SDL_DestroyAudioStream(g_sdl_audio_stream);
		g_sdl_audio_stream = NULL;
	}
}
