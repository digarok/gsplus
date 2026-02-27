const char rcsid_pulseaudio_driver_c[] = "@(#)$KmKId: pulseaudio_driver.c,v 1.10 2021-12-16 22:41:59+00 kentd Exp $";

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

#ifdef PULSE_AUDIO
// Ignore entire file if PULSE_AUDIO is not defined!

// Some ideas from Sample code:
//  https://github.com/gavv/snippets/blob/master/pa/pa_play_async_poll.c

#include "defc.h"
#include "sound.h"
#include "protos_pulseaudio_driver.h"

#include <pulse/pulseaudio.h>
#include <unistd.h>

#define PULSE_REBUF_SIZE	(64*1024)

word32	g_pulseaudio_rebuf[PULSE_REBUF_SIZE];
volatile int g_pulseaudio_rd_pos;
volatile int g_pulseaudio_wr_pos;
volatile int g_pulseaudio_playing = 0;

extern int Verbose;

extern int g_preferred_rate;
extern int g_audio_enable;
extern word32 *g_sound_shm_addr;
extern int g_sound_min_samples;
extern int g_sound_max_multiplier;
extern int g_sound_size;
extern word32 g_vbl_count;

int g_call_num = 0;

pa_mainloop *g_pa_mainloop_ptr = 0;
pa_context *g_pa_context_ptr = 0;
pa_stream *g_pa_stream_ptr = 0;
pa_sample_spec g_pa_sample_spec = { 0 };
pa_buffer_attr g_pa_buffer_attr = { 0 };


int
pulse_audio_send_audio(byte *ptr, int in_size)
{
	word32	*wptr, *pa_wptr;
	int	samps, sample_num;
	int	i;

	samps = in_size / 4;
	wptr = (word32 *)ptr;
	sample_num = g_pulseaudio_wr_pos;
	pa_wptr = &(g_pulseaudio_rebuf[0]);
	for(i = 0; i < samps; i++) {
		pa_wptr[sample_num] = *wptr++;
		sample_num++;
		if(sample_num >= PULSE_REBUF_SIZE) {
			sample_num = 0;
		}
	}

	g_pulseaudio_wr_pos = sample_num;

	pulse_audio_main_events();

	return in_size;
}

void
pulse_audio_main_events()
{
	pa_stream_state_t stream_state;
	pa_context_state_t context_state;
	int	count, num, sz, do_write;

	count = 0;
	do_write = 1;
	while(1) {
		// Do a few mainloop cycles to see if samples are needed
		num = pa_mainloop_iterate(g_pa_mainloop_ptr, 0, 0);
		//printf("pa_mainloop_iterate ret:%d count:%d\n", num, count);
		if(num < 0) {
			return;
		}
		context_state = pa_context_get_state(g_pa_context_ptr);
		if((context_state == PA_CONTEXT_FAILED) ||
				(context_state == PA_CONTEXT_TERMINATED)) {
			printf("context_state is bad: %d\n", context_state);
			g_audio_enable = 0;
			return;
		}
		stream_state = pa_stream_get_state(g_pa_stream_ptr);
		if((stream_state == PA_STREAM_FAILED) ||
				(stream_state == PA_STREAM_TERMINATED)) {
			printf("stream state bad: %d\n", stream_state);
			g_audio_enable = 0;
			return;
		}
		if(do_write) {
			sz = pa_stream_writable_size(g_pa_stream_ptr);
			if(sz > 0) {
				pulse_audio_write_to_stream(count, sz);
				do_write = 0;
			}
		}
		count++;
		if(count > 50) {
			printf("pulse_audio_main_events() looped %d times\n",
								count);
			return;
		}
		if(num == 0) {			// Nothing else to do
			return;
		}
	}
}

void
pulse_audio_write_to_stream(int dbg_count, int in_sz)
{
	word32	*wptr;
	void	*vptr;
	// const pa_timing_info *pa_timing_info_ptr;
	// pa_usec_t	pa_latency;
	size_t	sz;
	int	num_samps, sample_num, samps_avail, err, samps_needed;
	int	min_samples, max_samples;
	int	i;

	samps_needed = in_sz / 4;
	sample_num = g_pulseaudio_rd_pos;
	samps_avail = (g_pulseaudio_wr_pos - sample_num) &
							(PULSE_REBUF_SIZE - 1);
	min_samples = g_sound_min_samples;
	max_samples = min_samples * g_sound_max_multiplier;
	if(samps_needed > min_samples) {
		min_samples = samps_needed;
	}
#if 0
	if(samps_needed > samps_avail) {
		// We don't have enough samples, must pause
		g_pulseaudio_playing = 0;
		sample_num = g_pulseaudio_wr_pos;	// Eat remaining samps
	}
#endif
	if((g_pulseaudio_playing == 0) && (samps_avail > min_samples)) {
		// We can unpause
		g_pulseaudio_playing = 1;
	}
	if(g_pulseaudio_playing && (samps_avail > max_samples)) {
		printf("JUMP SAMPLE_NUM by %d samples!\n", max_samples / 2);
		sample_num += (max_samples / 2);
		sample_num = sample_num & (PULSE_REBUF_SIZE - 1);
	}

#if 0
	if(g_call_num < 100) {
		printf("call_num:%d playing:%d g_vbl_count:%d samps_needed:%d, "
			"samps_avail:%d\n", g_call_num, g_pulseaudio_playing,
			g_vbl_count, samps_needed, samps_avail);
	}
#endif
	g_call_num++;
	num_samps = MIN(samps_avail, samps_needed);
	if(g_pulseaudio_playing) {
		vptr = 0;			// Let it allocate for us
		sz = num_samps * 4;
		err = pa_stream_begin_write(g_pa_stream_ptr, &vptr, &sz);
		wptr = vptr;
		if(err) {
			g_audio_enable = 0;
			printf("pa_stream_begin_write failed: %s\n",
							pa_strerror(err));
			return;
		}
		num_samps = sz / 4;
		for(i = 0; i < num_samps; i++) {
			wptr[i] = g_pulseaudio_rebuf[sample_num];
			sample_num++;
			if(sample_num >= PULSE_REBUF_SIZE) {
				sample_num = 0;
			}
		}
	} else {
		err = pa_stream_cancel_write(g_pa_stream_ptr);
		// Just get out...don't let us get further behind by sending
		//  silence frames.
		return;
	}

	g_pulseaudio_rd_pos = sample_num;

#if 0
	pa_timing_info_ptr = pa_stream_get_timing_info(g_pa_stream_ptr);
	err = pa_stream_get_latency(g_pa_stream_ptr, &pa_latency, 0);
	printf(" will send %d samples to the stream, write_index:%lld, "
		"latency:%lld\n", num_samps * 4,
		(word64)pa_timing_info_ptr->write_index, (word64)pa_latency);
#endif
	err = pa_stream_write(g_pa_stream_ptr, wptr, num_samps * 4, 0, 0,
							PA_SEEK_RELATIVE);
	if(err) {
		printf("pa_stream_write: %s\n", pa_strerror(err));
		g_audio_enable = 0;
	}
}

int
pulse_audio_start_stream()
{
	int	flags, ret;

	g_pa_sample_spec.format = PA_SAMPLE_S16LE;
	g_pa_sample_spec.rate = g_preferred_rate;
	g_pa_sample_spec.channels = 2;
	printf("Set requested rate=%d\n", g_pa_sample_spec.rate);
	g_pa_stream_ptr = pa_stream_new(g_pa_context_ptr, "KEGS",
						&g_pa_sample_spec, 0);
	if(!g_pa_stream_ptr) {
		printf("pa_stream_new failed\n");
		return 1;
	}

	g_pa_buffer_attr.maxlength = -1;		// Maximum server buffer
	g_pa_buffer_attr.tlength = 4*g_preferred_rate/10;	// 1/10th sec
	//g_pa_buffer_attr.prebuf = 4*g_preferred_rate/100;	// 1/100th sec
	g_pa_buffer_attr.prebuf = -1;
	g_pa_buffer_attr.minreq = 4*g_preferred_rate/60;	// 1/60th sec

	flags = PA_STREAM_ADJUST_LATENCY;
	//flags = PA_STREAM_AUTO_TIMING_UPDATE | PA_STREAM_INTERPOLATE_TIMING |
	//		PA_STREAM_ADJUST_LATENCY;
	// PA_STREAM_AUTO_TIMING_UPDATE and PA_STREAM_INTERPOLATE_TIMING are
	//  to get latency info from the server.  PA_STREAM_ADJUST_LATENCY
	//  means the total latency including the server output sink buffer
	//  tries to be tlength

	ret = pa_stream_connect_playback(g_pa_stream_ptr, 0, &g_pa_buffer_attr,
							flags, 0, 0);
	if(ret) {
		printf("pa_stream_connect_playback failed: %d\n", ret);
		return ret;
	}

	return 0;		// Success!
}

int
pulse_audio_do_init()
{
	pa_stream_state_t stream_state;
	pa_context_state_t context_state;
	int	ret, count, num;

	g_pa_mainloop_ptr = pa_mainloop_new();
	g_pa_context_ptr = pa_context_new(
			pa_mainloop_get_api(g_pa_mainloop_ptr), "KEGS");
	if(!g_pa_context_ptr) {
		printf("Pulse Audio pa_context_new() failed\n");
		return 1;
	}

	ret = pa_context_connect(g_pa_context_ptr, 0, 0, 0);
	if(ret != 0) {
		printf("pa_context_connect failed: %d\n", ret);
		return 1;
	}

	count = 0;
	while(1) {
		// Do a few mainloop cycles to get stream initialized
		num = pa_mainloop_iterate(g_pa_mainloop_ptr, 0, 0);
#if 0
		printf("pa_mainloop_iterate ret: %d, count:%d g_pa_stream_ptr:"
				"%p\n", num, count, g_pa_stream_ptr);
#endif
		if(num < 0) {
			return 1;
		}
		if(num == 0) {
			usleep(10*1000);
			if(count++ > 50) {
				// Waited more than 500ms, just give up
				printf("Timed out waiting for Pulse Audio to "
								"start\n");
				return 1;
			}
		}
		// See if context is ready
		context_state = pa_context_get_state(g_pa_context_ptr);
		if((context_state == PA_CONTEXT_FAILED) ||
				(context_state == PA_CONTEXT_TERMINATED)) {
			printf("context_state is bad: %d\n", context_state);
			return 1;
		}
		if((context_state == PA_CONTEXT_READY) && !g_pa_stream_ptr) {
			ret = pulse_audio_start_stream();
			if(ret) {
				return ret;
			}
		}
		if(g_pa_stream_ptr) {
			stream_state = pa_stream_get_state(g_pa_stream_ptr);
			if((stream_state == PA_STREAM_FAILED) ||
					(stream_state == PA_STREAM_TERMINATED)){
				printf("stream state bad: %d\n", stream_state);
				return 1;
			}
			if(stream_state == PA_STREAM_READY) {
				printf("Pulse Audio stream is now ready!\n");
				return 0;
			}
		}
	}
}

int
pulse_audio_init()
{
	int	ret;

	g_pulseaudio_rd_pos = 0;
	g_pulseaudio_wr_pos = 0;

	ret = pulse_audio_do_init();
	// printf("pulse_audio_init ret:%d\n", ret);
	if(ret != 0) {
		// Free structures, disable sound
		if(g_pa_stream_ptr) {
			pa_stream_disconnect(g_pa_stream_ptr);
			pa_stream_unref(g_pa_stream_ptr);
			g_pa_stream_ptr = 0;
		}
		if(g_pa_context_ptr) {
			pa_context_disconnect(g_pa_context_ptr);
			pa_context_unref(g_pa_context_ptr);
			g_pa_context_ptr = 0;
		}
		if(g_pa_mainloop_ptr) {
			pa_mainloop_free(g_pa_mainloop_ptr);
			g_pa_mainloop_ptr = 0;
		}
		return ret;
	}

	sound_set_audio_rate(g_preferred_rate);

	return 0;
}

void
pulse_audio_shutdown()
{
	printf("pulse_audio_shutdown\n");
}

#endif		/* PULSE_AUDIO */
