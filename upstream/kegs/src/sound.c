const char rcsid_sound_c[] = "@(#)$KmKId: sound.c,v 1.155 2023-08-19 17:45:33+00 kentd Exp $";

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

#include "defc.h"

#define INCLUDE_RCSID_C
#include "sound.h"
#undef INCLUDE_RCSID_C

#define DOC_LOG(a,b,c,d)

extern int Verbose;
extern int g_use_shmem;
extern word32 g_vbl_count;
extern int g_preferred_rate;

extern word32 g_c03ef_doc_ptr;

extern int g_doc_vol;

extern dword64 g_last_vbl_dfcyc;

int	g_queued_samps = 0;
int	g_queued_nonsamps = 0;

#if defined(HPUX) || defined(__linux__) || defined(_WIN32) || defined(MAC)
int	g_audio_enable = -1;
#else
int	g_audio_enable = 0;		/* Not supported: default to off */
#endif
int	g_sound_min_msecs = 32;			// 32 msecs
int	g_sound_min_msecs_pulse = 150;		// 150 msecs
int	g_sound_max_multiplier = 6;		// 6*32 = ~200 msecs
int	g_sound_min_samples = 48000 * 32/1000;	// 32 msecs

Mockingboard g_mockingboard;

// The AY8913 chip has non-linear amplitudes (it has 16 levels) and the
//  documentation does not match measured results.  But all the measurements
//  should really be done at the final speaker/jack since all the stuff in
//  the path affects it.  But: no one's done this for Mockingboard that I
//  have found, so I'm taking measurements from the AY8913 chip itself.
// AY8913 amplitudes from https://groups.google.com/forum/#!original/
//				comp.sys.sinclair/-zCR2kxMryY/XgvaDICaldUJ
// by Matthew Westcott on December 21, 2001.
double g_ay8913_ampl_factor_westcott[16] = {		// NOT USED
	0.000,	// level[0]
	0.010,	// level[1]
	0.015,	// level[2]
	0.022,	// level[3]
	0.031,	// level[4]
	0.046,	// level[5]
	0.064,	// level[6]
	0.106,	// level[7]
	0.132,	// level[8]
	0.216,	// level[9]
	0.297,	// level[10]
	0.391,	// level[11]
	0.513,	// level[12]
	0.637,	// level[13]
	0.819,	// level[14]
	1.000,	// level[15]
};
// https://sourceforge.net/p/fuse-emulator/mailman/message/34065660/
//  refers to some Russian-language measurements at:
//  http://forum.tslabs.info/viewtopic.php?f=6&t=539 (translate from
//  Russian), they give:
// 0000,028F,03B3,0564, 07DC,0BA9,1083,1B7C,
// 2068,347A,4ACE,5F72, 7E16,A2A4,CE3A,FFFF
double g_ay8913_ampl_factor[16] = {
	0.000,	// level[0]
	0.010,	// level[1]
	0.014,	// level[2]
	0.021,	// level[3]
	0.031,	// level[4]
	0.046,	// level[5]
	0.064,	// level[6]
	0.107,	// level[7]
	0.127,	// level[8]
	0.205,	// level[9]
	0.292,	// level[10]
	0.373,	// level[11]
	0.493,	// level[12]
	0.635,	// level[13]
	0.806,	// level[14]
	1.000,	// level[15]
};
// MAME also appears to try to figure out how the channels get "summed"
//  together.  KEGS code adds them in a completely independent way, and due
//  to the circuit used on the AY8913, this is certainly incorrect.
#define MAX_MOCK_ENV_SAMPLES	2000
int	g_mock_env_vol[MAX_MOCK_ENV_SAMPLES];
byte	g_mock_noise_bytes[MAX_MOCK_ENV_SAMPLES];
int	g_mock_volume[16];		// Sample for each of the 16 amplitudes
word32	g_last_mock_vbl_count = 0;

#define VAL_MOCK_RANGE		(39000)

int	g_audio_rate = 0;
double	g_daudio_rate = 0.0;
double	g_drecip_audio_rate = 0.0;
double	g_dsamps_per_dfcyc = 0.0;
double	g_fcyc_per_samp = 0.0;

double g_last_sound_play_dsamp = 0.0;

#define VAL_C030_POS_VAL	(20400*16/15)
		// C030_POS_VAL is multiplied by g_doc_vol (0-15) and then
		//  divided by 16.  So scale this value up by 16/15 so that
		//  g_doc_vol==15 gives the intended value (+/-20400)

#define MAX_C030_TIMES		18000
float	g_c030_fsamps[MAX_C030_TIMES + 2];
int	g_num_c030_fsamps = 0;
int	g_c030_state = 0;
int	g_c030_val = (VAL_C030_POS_VAL / 2);
dword64	g_c030_dsamp_last_toggle = 0;

word32	*g_sound_shm_addr = 0;
int	g_sound_shm_pos = 0;

extern dword64 g_cur_dfcyc;

#define MAX_SND_BUF	65536

int	g_samp_buf[2*MAX_SND_BUF];
byte	g_zero_buf[4096];

double g_doc_dsamps_extra = 0.0;

int	g_num_snd_plays = 0;
int	g_num_recalc_snd_parms = 0;

char	*g_sound_file_str = 0;
int	g_sound_file_fd = -1;
int	g_sound_file_bytes = 0;

// WAV file information:
// From https://docs.fileformat.com/audio/wav/
// left channel is first: https://web.archive.org/web/20080113195252/
//			http://www.borg.com/~jglatt/tech/wave.htm

byte g_wav_hdr[44] = {
	'R', 'I', 'F', 'F', 0xff, 0xff, 0xff, 0xff,		// 0x00-0x07
	'W', 'A', 'V', 'E', 'f', 'm', 't', ' ',			// 0x08-0x0f
	16, 0, 0, 0, 1, 0, 2, 0,				// 0x10-0x17
		// 16=length of 'fmt ' chunk, 1=PCM, 2=stereo.
	0xff, 0xff, 0xff, 0xff,	0xff, 0xff, 0xff, 0xff,		// 0x18-0x1f
	4, 0, 16, 0, 'd', 'a', 't', 'a',			// 0x20-0x27
	0xff, 0xff, 0xff, 0xff					// 0x28-0x2b
};
	// Bytes 4-7 are the total file size-8, so [0x28:0x2b]+0x24
	// Bytes [0x18-0x1b]is the sample rate (so 44100 or so)
	// [0x1c-0x1f] is bytes-per-second: [0x18-0x1b]*[0x10]*[0x16]/8


void
sound_init()
{
	doc_init();
	snddrv_init();
}

void
sound_set_audio_rate(int rate)
{
	g_audio_rate = rate;
	g_daudio_rate = (rate)*1.0;
	g_drecip_audio_rate = 1.0/(rate);
	g_dsamps_per_dfcyc = ((rate*1.0) / (DCYCS_1_MHZ * 65536.0));
	g_fcyc_per_samp = (DCYCS_1_MHZ * 65536.0 / (rate*1.0));
	g_sound_min_samples = rate * g_sound_min_msecs / 1000;

	printf("Set g_audio_rate = %d in main KEGS process, min_samples:%d\n",
		rate, g_sound_min_samples);
}

void
sound_reset(dword64 dfcyc)
{
	doc_reset(dfcyc);
	mockingboard_reset(dfcyc);
}

void
sound_shutdown()
{
	snddrv_shutdown();
}

void
sound_update(dword64 dfcyc)
{
	/* Called every VBL time to update sound status */

	/* "play" sounds for this vbl */

	//DOC_LOG("do_snd_pl", -1, dsamps, 0);
	sound_play(dfcyc);
}

void
sound_file_start(char *filename)
{
	sound_file_close();

	g_sound_file_str = filename;	// Can be NULL, if so, do not start
	if(filename) {
		printf("Set audio save file to: %s\n", filename);
	}
}

void
sound_file_open()
{
	char	*filename;
	word32	exp_size;
	int	fd;

	filename = g_sound_file_str;
	if(!filename) {
		return;
	}

	fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0x1b6);
	if(fd < 0) {
		printf("open_sound_file open ret: %d, errno: %d\n", fd, errno);
		sound_file_close();
		return;
	}

	exp_size = 1024*1024;		// Default to 1MB, changed at close
	dynapro_set_word32(&g_wav_hdr[4], exp_size + 44 - 8);	// File size
	dynapro_set_word32(&g_wav_hdr[0x28], exp_size);		// data size
	dynapro_set_word32(&g_wav_hdr[0x18], g_audio_rate);	// Sample rate
	dynapro_set_word32(&g_wav_hdr[0x1c], g_audio_rate * 2 * 2);
								// bytes-per-sec

	(void)cfg_write_to_fd(fd, &g_wav_hdr[0], 0, 44);
	g_sound_file_fd = fd;
	g_sound_file_bytes = 0;
	printf("Opened file %s for sound\n", filename);
}

void
sound_file_close()
{
	int	fd;

	fd = g_sound_file_fd;
	if(fd >= 0) {
		dynapro_set_word32(&g_wav_hdr[0x28], g_sound_file_bytes);
		dynapro_set_word32(&g_wav_hdr[4], g_sound_file_bytes + 44 - 8);
		cfg_write_to_fd(fd, &g_wav_hdr[0], 0, 44);
			// Rewrite first 44 bytes with WAV header
		printf("Close sound file %s, fd:%d\n", g_sound_file_str, fd);
		close(fd);
	}

	free(g_sound_file_str);
	g_sound_file_fd = -1;
	g_sound_file_str = 0;
}

void
send_sound_to_file(word32 *wptr, int shm_pos, int num_samps, int real_samps)
{
	int	size, this_size;

	if(!real_samps && g_sound_file_bytes) {
		// Don't do anything
		return;
	}
	if(g_sound_file_fd < 0) {
		sound_file_open();
	}

	if(!wptr) {
		// No real samps
		size = real_samps * 4;
		while(size) {
			this_size = size;
			if(this_size > 4096) {
				this_size = 4096;
			}
			must_write(g_sound_file_fd, &g_zero_buf[0], this_size);
			size -= this_size;
		}
		return;
	}

	size = 0;
	if((num_samps + shm_pos) > SOUND_SHM_SAMP_SIZE) {
		size = SOUND_SHM_SAMP_SIZE - shm_pos;
		g_sound_file_bytes += (size * 4);

		must_write(g_sound_file_fd, (byte *)&(wptr[shm_pos]), 4*size);
		shm_pos = 0;
		num_samps -= size;
	}

	g_sound_file_bytes += (num_samps * 4);

	must_write(g_sound_file_fd, (byte *)&(wptr[shm_pos]), 4*num_samps);
}

void
show_c030_state(dword64 dfcyc)
{
	show_c030_samps(dfcyc, &(g_samp_buf[0]), 100);
}

void
show_c030_samps(dword64 dfcyc, int *outptr, int num)
{
	int	last;
	int	i;

	if(!g_num_c030_fsamps) {
		return;

	}
	printf("c030_fsamps[]: %d, dfcyc:%015llx\n", g_num_c030_fsamps, dfcyc);

	for(i = 0; i < g_num_c030_fsamps+2; i++) {
		printf("%3d: %5.3f\n", i, g_c030_fsamps[i]);
	}

	printf("Samples[] = %d\n", num);

	last = 0x0dadbeef;
	for(i = 0; i < num; i++) {
		if((last != outptr[0]) || (i == (num - 1))) {
			printf("Samp[%4d]: %d\n", i, outptr[0]);
			last = outptr[0];
		}
		outptr += 2;
	}
}

int
sound_play_c030(dword64 dfcyc, dword64 dsamp, int *outptr_start, int num_samps)
{
	int	*outptr;
	dword64	dsamp_min;
	float	ftmp, fsampnum, next_fsampnum, fpercent;
	int	val, num, c030_state, c030_val, pos, sampnum, next_sampnum;
	int	doc_vol, min_i, mul;
	int	i, j;

	// Handle $C030 speaker clicks.  Clicks for the past num_samps are
	//  in g_c030_fsamps[] giving the sample position when the click
	//  occurred.  Turn this into samples, tracking multiple clicks per
	//  sample into an intermediate value.  After 500ms of no clicks,
	//  transition the speakers from +/-20400 to 0, so it's idle.
	// The speaker is affected by the DOC volume in g_doc_vol, like a real
	//  IIgs (this is used during the system beep).  This code reacts
	//  to DOC volume changes when they happen by causing sound_play()
	//  to be called, so all samples with the old volume are played then
	//  new clicks are collected.

	num = g_num_c030_fsamps;		// Number of clicks
	if(!num) {
		if(g_c030_val == 0) {
			return 0;		// Speaker is at rest
		}
	}

	pos = 0;
	outptr = outptr_start;
	c030_state = g_c030_state;
	c030_val = g_c030_val;
		// c030_val may be less than max due decay after 500ms.
		// Always use it first, until speaker toggles, which should
		//  restore the full speaker range
	doc_vol = g_doc_vol;

	if(!num) {
		// No clicks.  See if we should begin transitioning the
		//  speaker output to 0.  I tried multiplying by .9999 but
		//  that seemed to take too long at the end, so just use a
		//  linear ramp down.  Do this ramp based on the last click
		//  time, not VBL, since this is more consistent

		dsamp_min = g_c030_dsamp_last_toggle + (g_audio_rate >> 4);
		if(dsamp >= dsamp_min) {
			min_i = 0;
		} else {
			min_i = (int)(dsamp_min - dsamp);
		}
		mul = (2 * c030_state - 1) * doc_vol;
		val = (c030_val * mul) >> 4;
		for(i = 0; i < num_samps; i++) {
			if(i >= min_i) {
				if(c030_val > 4) {
					c030_val -= 4;
				} else {
					c030_val = 0;
				}
				val = (c030_val * mul) >> 4;
			}
			outptr[0] = val;
			outptr[1] = val;
			outptr += 2;
		}
#if 0
		printf("at %015llx, num_samps:%d val at start:%d, at end:%d, "
			"min_i:%d\n", dfcyc, num_samps, g_c030_val, c030_val,
			min_i);
#endif
		g_c030_val = c030_val;
		if(c030_val == 0) {
			//printf("Speaker at rest\n");
		}

		return 1;
	}

	g_c030_fsamps[num] = (float)(num_samps);

	num++;
	fsampnum = g_c030_fsamps[0];
	sampnum = (int)fsampnum;
	fpercent = (float)0.0;
	i = 0;

	while(i < num) {
		if(sampnum < 0 || sampnum > num_samps) {
			halt_printf("play c030: [%d]:%f is %d, > %d\n",
					i, fsampnum, sampnum, num_samps);
			break;
		}

		/* write in samples to all samps < me */
		val = ((2 * c030_state) - 1) * ((c030_val * doc_vol) >> 4);
		if(num <= 1) {
			printf("num:%d i:%d pos:%d, sampnum:%d c030_state:%d "
				" at %015llx\n", num, i, pos, sampnum,
				c030_state, dfcyc);
		}
		for(j = pos; j < sampnum; j++) {
			outptr[0] = val;
			outptr[1] = val;
			outptr += 2;
			pos++;
		}

		if((sampnum >= num_samps) || ((i + 1) >= num)) {
			break;		// All done
		}

		/* now, calculate me */
		fpercent = (float)0.0;
		if(c030_state) {
			fpercent = (fsampnum - (float)sampnum);
		}

		c030_state = !c030_state;
		c030_val = VAL_C030_POS_VAL;
		g_c030_dsamp_last_toggle = dsamp + i;

		next_fsampnum = g_c030_fsamps[i+1];
		next_sampnum = (int)next_fsampnum;
		// Handle all the changes during this one sample
		while(next_sampnum == sampnum) {
			if(c030_state) {
				fpercent += (next_fsampnum - fsampnum);
			}
			i++;
			fsampnum = next_fsampnum;

			if(i > num) {
				break;		// This should not happen!
			}
			next_fsampnum = g_c030_fsamps[i+1];
			next_sampnum = (int)next_fsampnum;
			c030_state = !c030_state;
		}

		if(c030_state) {
			// add in time until next sample
			ftmp = (float)(int)(fsampnum + (float)1.0);
			fpercent += (ftmp - fsampnum);
		}

		if((fpercent < (float)0.0) || (fpercent > (float)1.0)) {
			halt_printf("fpercent: %d = %f\n", i, fpercent);
			show_c030_samps(dfcyc, outptr_start, num_samps);
			break;
		}

		val = (int)((2*fpercent - 1) * ((c030_val * doc_vol) >> 4));
		outptr[0] = val;
		outptr[1] = val;
		outptr += 2;
		pos++;
		i++;

		sampnum = next_sampnum;
		fsampnum = next_fsampnum;
	}

	g_c030_state = c030_state;
	g_c030_val = c030_val;

#if 0
	if(g_sound_file_str) {
		show_c030_samps(dfcyc, outptr_start, num_samps);
	}
#endif

	// See if there are any entries >= fsampnum, copy them back down
	//  to the beginning of the array
	pos = 0;
	num--;
	fsampnum = (float)num_samps;
	while(i < num) {
		g_c030_fsamps[pos] = g_c030_fsamps[i] - fsampnum;
#if 0
		printf("Copied [%d] %f to [%d] as %f\n", i, g_c030_fsamps[i],
			pos, g_c030_fsamps[pos]);
#endif
		i++;
		pos++;
	}
	g_num_c030_fsamps = pos;

	return 1;
}


int g_sound_play_depth = 0;

// sound_play(): forms the samples from the last sample time to the current
//  time.  Can be called anytime from anywhere.  This is how KEGS handles
//  dynamic sound changes (say, disabling an Ensoniq oscillator manually):
//  when it's turned off, call sound_play() to play up to this moment, then
//  the next time sound_play() is called, it will just know this osc is off
// So, on any sound-related state change, call sound_play().

void
sound_play(dword64 dfcyc)
{
	Ay8913	*ay8913ptr;
	int	*outptr, *outptr_start;
	word32	*sndptr;
	double	last_dsamp, dsamp_now, dvolume, dsamps;
	word32	uval1, uval0;
	int	val, val0, val1, pos, snd_buf_init, num_samps, num_pairs;
	int	sound_mask, ivol;
	int	i, j;

	g_num_snd_plays++;
	if(g_sound_play_depth) {
		halt_printf("Nested sound_play!\n");
	}

	g_sound_play_depth++;

	/* calc sample num */

	dsamps = dfcyc * g_dsamps_per_dfcyc;
	last_dsamp = g_last_sound_play_dsamp;
	num_samps = (int)(dsamps - g_last_sound_play_dsamp);

	dsamp_now = last_dsamp + (double)num_samps;

	if(num_samps < 1) {
		/* just say no */
		g_sound_play_depth--;
		return;
	}

	dbg_log_info(dfcyc, (word32)(dword64)dsamp_now, num_samps, 0x200);

	if(num_samps > MAX_SND_BUF) {
		printf("num_samps: %d, too big!\n", num_samps);
		g_sound_play_depth--;
		return;
	}

	outptr_start = &(g_samp_buf[0]);
	outptr = outptr_start;

	snd_buf_init = sound_play_c030(dfcyc, (dword64)dsamp_now, outptr_start,
								num_samps);

	snd_buf_init = doc_play(dfcyc, last_dsamp, dsamp_now, num_samps,
						snd_buf_init, outptr_start);

	num_pairs = 0;
	// Do Mockinboard channels
	for(i = 0; i < 2; i++) {			// Pair: 0 or 1
		ay8913ptr = &(g_mockingboard.pair[i].ay8913);
		for(j = 0; j < 3; j++) {		// Channels: A, B, or C
			if((ay8913ptr->regs[8 + j] & 0x1f) == 0) {
				continue;
			}
			num_pairs = 2;
			g_last_mock_vbl_count = g_vbl_count;
			break;
		}
	}
	if((g_vbl_count - g_last_mock_vbl_count) < 120) {
		// Keep playing for 2 seconds, to avoid some static issues
		num_pairs = 2;
	}
	if(num_pairs) {
		sound_mask = -1;
		if(snd_buf_init == 0) {
			sound_mask = 0;
			snd_buf_init++;
		}
		outptr = outptr_start;
		ivol = -((VAL_MOCK_RANGE * 3 / (8 * 15)) * g_doc_vol);
			// Do 3/8 of range below 0, leaving 5/8 above 0
		for(i = 0; i < num_samps; i++) {
			outptr[0] = (outptr[0] & sound_mask) + ivol;
			outptr[1] = (outptr[1] & sound_mask) + ivol;
			outptr += 2;
		}
		for(i = 0; i < 16; i++) {
			dvolume = (g_doc_vol * VAL_MOCK_RANGE) / (15.0 * 3.0);
			ivol = (int)(g_ay8913_ampl_factor[i] * dvolume);
			g_mock_volume[i] = ivol;
		}
	}
	for(i = 0; i < num_pairs; i++) {
		if(g_mockingboard.disable_mask) {
			printf("dsamp:%lf\n", dsamps);
		}

		sound_mock_envelope(i, &(g_mock_env_vol[0]), num_samps,
							&(g_mock_volume[0]));
		sound_mock_noise(i, &(g_mock_noise_bytes[0]), num_samps);
		for(j = 0; j < 3; j++) {
			sound_mock_play(i, j, outptr_start,
				&(g_mock_env_vol[0]), &(g_mock_noise_bytes[0]),
				&(g_mock_volume[0]), num_samps);
		}
	}

	g_last_sound_play_dsamp = dsamp_now;

	outptr = outptr_start;
	pos = g_sound_shm_pos;
	sndptr = g_sound_shm_addr;

#if 0
	printf("samps_left: %d, num_samps: %d\n", samps_left, num_samps);
#endif

	if(g_audio_enable != 0) {
		if(snd_buf_init) {
			/* convert sound buf */
			for(i = 0; i < num_samps; i++) {
				val0 = outptr[0];
				val1 = outptr[1];
				val = val0;
				if(val0 > 32767) {
					val = 32767;
				}
				if(val0 < -32768) {
					val = -32768;
				}
				uval0 = val & 0xffffU;
				val = val1;
				if(val1 > 32767) {
					val = 32767;
				}
				if(val1 < -32768) {
					val = -32768;
				}
				uval1 = val & 0xffffU;
				outptr += 2;

#if defined(__linux__) || defined(OSS)
				/* Linux seems to expect little-endian */
				/*  samples always, even on PowerPC */
# ifdef KEGS_BIG_ENDIAN
				sndptr[pos] = ((uval1 & 0xff) << 24) +
						((uval1 & 0xff00) << 8) +
						((uval0 & 0xff) << 8) +
						((uval0 >> 8) & 0xff);
# else
				sndptr[pos] = (uval1 << 16) + (uval0 & 0xffff);
# endif
#else
# ifdef KEGS_BIG_ENDIAN
				sndptr[pos] = (uval0 << 16) + uval1;
# else
				sndptr[pos] = (uval1 << 16) + uval0;
# endif
#endif
				pos++;
				if(pos >= SOUND_SHM_SAMP_SIZE) {
					pos = 0;
				}
			}
			if(g_queued_nonsamps) {
				/* force out old 0 samps */
				snddrv_send_sound(0, g_queued_nonsamps);
				g_queued_nonsamps = 0;
			}
			if(g_sound_file_str) {
				send_sound_to_file(g_sound_shm_addr,
						g_sound_shm_pos, num_samps, 1);
			}
			g_queued_samps += num_samps;
		} else {
			/* move pos */
			pos += num_samps;
			while(pos >= SOUND_SHM_SAMP_SIZE) {
				pos -= SOUND_SHM_SAMP_SIZE;
			}
			if(g_sound_file_str) {
				send_sound_to_file(0, g_sound_shm_pos,
								num_samps, 0);
			}
			if(g_queued_samps) {
				/* force out old non-0 samps */
				snddrv_send_sound(1, g_queued_samps);
				g_queued_samps = 0;
			}
			g_queued_nonsamps += num_samps;
		}
	}

	g_sound_shm_pos = pos;

	if(g_audio_enable != 0) {
		if(g_queued_samps >= (g_audio_rate/60)) {
			snddrv_send_sound(1, g_queued_samps);
			g_queued_samps = 0;
		}

		if(g_queued_nonsamps >= (g_audio_rate/60)) {
			snddrv_send_sound(0, g_queued_nonsamps);
			g_queued_nonsamps = 0;
		}
	}

	g_last_sound_play_dsamp = dsamp_now;

	g_sound_play_depth--;
}

void
sound_mock_envelope(int pair, int *env_ptr, int num_samps, int *vol_ptr)
{
	Ay8913	*ay8913ptr;
	double	dmul, denv_period, dusecs_per_samp;
	dword64	env_dsamp, dsamp_inc;
	word32	ampl, eff_ampl, reg13, env_val, env_period;
	int	i;

	// This routine calculates a fixed-point increment to apply
	//  to env_dsamp, where the envelope value is in bits 44:40 (bit
	//  44 is to track the alternating waveform, 43:40 is the env_ampl).
	// This algorithm does not properly handle dynamically changing the
	//  envelope period in the middle of a step.  In the AY8913, the
	//  part counts up to the env_period, and if the period is changed
	//  to a value smaller than the current count, it steps immediately
	//  to the next step.  This routine will wait for enough fraction
	//  to accumulate before stepping.  At most, this can delay the step
	//  by almost the new count time (if the new period is smaller), but
	//  no more.  I suspect this is not noticeable.
	if(num_samps > MAX_MOCK_ENV_SAMPLES) {
		halt_printf("envelope overflow!: %d\n", num_samps);
		return;
	}

	ay8913ptr = &(g_mockingboard.pair[pair].ay8913);
	ampl = ay8913ptr->regs[8] | ay8913ptr->regs[9] | ay8913ptr->regs[10];
	if((ampl & 0x10) == 0) {
		// No one uses the envelope
		return;
	}

	env_dsamp = ay8913ptr->env_dsamp;
	env_period = ay8913ptr->regs[11] + (256 * ay8913ptr->regs[12]);
	if(env_period == 0) {
		denv_period = 0.5;		// To match MAME
	} else {
		denv_period = (double)env_period;
	}
	dmul = (1.0 / 16.0) * (1 << 20) * (1 << 20);	// (1ULL << 40) / 16.0
	// Calculate amount counter will count in one sample.
	// inc_per_tick 62.5KHz tick: (1/env_period)
	// inc_per_dfcyc: (1/(16*env_period))
	// inc_per_samp = inc_per_dfcyc * g_fcyc_per_samp
	dusecs_per_samp = g_fcyc_per_samp / 65536.0;
	dsamp_inc = (dword64)((dmul * dusecs_per_samp / denv_period));
			// Amount to inc per sample, fixed point, 40 bit frac

	reg13 = ay8913ptr->regs[13];			// "reg15", env ctrl
	eff_ampl = 0;
	for(i = 0; i < num_samps; i++) {
		env_dsamp = (env_dsamp + dsamp_inc) & 0x9fffffffffffULL;
		env_val = (env_dsamp >> 40) & 0xff;
		eff_ampl = env_val & 0xf;
		if((reg13 & 4) == 0) {
			eff_ampl = 15 - eff_ampl;	// not attack
		}
		if((reg13 & 8) && (reg13 & 2)) {
			// continue and alternate
			if(env_val & 0x10) {
				eff_ampl = 15 - eff_ampl;
			}
		}
		if(((reg13 & 8) == 0) && (env_val >= 0x10)) {
			eff_ampl = 0;
			ampl = 0;		// Turn off envelope
			env_dsamp |= (0x80ULL << 40);
		} else if((reg13 & 1) && (env_val >= 0x10)) {
			eff_ampl = ((reg13 >> 1) ^ (reg13 >> 2)) & 1;
			eff_ampl = eff_ampl * 15;
			ampl = eff_ampl;	// Turn off envelope
			env_dsamp |= (0x80ULL << 40);
		}
		*env_ptr++ = vol_ptr[eff_ampl & 0xf];
	}
	ay8913ptr->env_dsamp = env_dsamp;
}

void
sound_mock_noise(int pair, byte *noise_ptr, int num_samps)
{
	Ay8913	*ay8913ptr;
	word32	ampl, mix, noise_val, noise_samp, noise_period, xor, samp_inc;
	int	doit;
	int	i;

	if(num_samps > MAX_MOCK_ENV_SAMPLES) {
		halt_printf("noise overflow!: %d\n", num_samps);
		return;
	}

	ay8913ptr = &(g_mockingboard.pair[pair].ay8913);
	doit = 0;
	for(i = 0; i < 3; i++) {
		ampl = ay8913ptr->regs[8 + i];
		mix = ay8913ptr->regs[7] >> i;
		if((ampl != 0) && ((mix & 8) == 0)) {
			doit = 1;
			break;
		}
	}
	if(!doit) {
		// No channel looks at noise, don't bother
		return;
	}

	noise_val = ay8913ptr->noise_val;
	noise_samp = ay8913ptr->noise_samp;
	noise_period = (ay8913ptr->regs[6] & 0x1f);
	noise_period = noise_period << 16;
	samp_inc = (word32)(g_fcyc_per_samp / 16.0);
			// Amount to inc per sample
	if(noise_samp >= noise_period) {
		// Period changed during sound, reset
		noise_samp = noise_period;
	}
	for(i = 0; i < num_samps; i++) {
		noise_samp += samp_inc;
		if(noise_samp >= noise_period) {
			// HACK: handle fraction
			// 17-bit LFSR, algorithm from MAME:sound/ay8910.cpp
			// val = val ^ (((val & 1) ^ ((val >> 3) & 1)) << 17)
			xor = 0;
			xor = (noise_val ^ (noise_val >> 3)) & 1;
			noise_val = (noise_val ^ (xor << 17)) >> 1;
			noise_samp -= noise_period;
		}
		noise_ptr[i] = noise_val & 1;
	}
	ay8913ptr->noise_samp = noise_samp;
	ay8913ptr->noise_val = noise_val;
}

int g_did_mock_print = 100;

void
sound_mock_play(int pair, int channel, int *outptr, int *env_ptr,
				byte *noise_ptr, int *vol_ptr, int num_samps)
{
	Ay8913	*ay8913ptr;
	word32	ampl, mix, tone_samp, tone_period, toggle_tone;
	word32	samp_inc, noise_val;
	int	out, ival, do_print;
	int	i;

	if((g_mockingboard.disable_mask >> ((pair * 3) + channel)) & 1) {
		// This channel is disabled
		return;
	}

	ay8913ptr = &(g_mockingboard.pair[pair].ay8913);
	ampl = ay8913ptr->regs[8 + channel] & 0x1f;
	if(ampl == 0) {
		return;
	}
	toggle_tone = ay8913ptr->toggle_tone[channel];		// 0 or 1
	mix = (ay8913ptr->regs[7] >> channel) & 9;
	if(mix == 9) {
		// constant tone: output will be ampl for this channel.
		if(ampl & 0x10) {		// Envelope!
			// The envelope can make the tone, so must calculate it
		} else {
			// HACK: do nothing for now
			return;
		}
	}
	outptr += pair;			// pair[1] is right
	tone_samp = ay8913ptr->tone_samp[channel];
	tone_period = ay8913ptr->regs[2*channel] +
					(256 * ay8913ptr->regs[2*channel + 1]);
	tone_period = tone_period << 16;
	samp_inc = (word32)(g_fcyc_per_samp / 8.0);
			// Amount to inc per sample
	do_print = 0;
	if(g_mockingboard.disable_mask) {
		printf("Doing %d samps, mix:%d, ampl:%02x\n", num_samps, mix,
									ampl);
		do_print = 1;
		g_did_mock_print = 0;
	}
	if((num_samps > 500) && (g_did_mock_print == 0)) {
		do_print = 1;
		g_did_mock_print = 1;
		printf("Start of %d sample, channel %d mix:%02x ampl:%02x "
			"toggle_tone:%02x\n", num_samps, channel, mix, ampl,
			toggle_tone);
		printf(" tone_period:%08x, tone_samp:%08x, samp_inc:%08x\n",
			tone_period, tone_samp, samp_inc);
	}
	if(tone_samp >= tone_period) {
		// Period changed during sound, reset it
		tone_samp = tone_period;
	}
	for(i = 0; i < num_samps; i++) {
		tone_samp += samp_inc;
		if(tone_samp >= tone_period) {
			// HACK: handle toggling mid-sample...
			toggle_tone ^= 1;
			if(do_print) {
				printf("i:%d tone_toggled to %d, tone_period:"
					"%04x, pre tone_samp:%08x\n", i,
					toggle_tone, tone_period, tone_samp);
			}
			tone_samp -= tone_period;
			if(do_print) {
				printf("post tone_samp:%08x\n", tone_samp);
			}
		}
		noise_val = noise_ptr[i] & 1;
		out = (toggle_tone || (mix & 1)) &
						((noise_val & 1) || (mix & 8));
			// Careful mix of || and & above...
		ival = vol_ptr[ampl & 0xf];
		if(ampl & 0x10) {			// Envelope
			ival = env_ptr[i];
		}
		*outptr += ival*out;
		outptr += 2;
	}
	ay8913ptr->tone_samp[channel] = tone_samp;
	ay8913ptr->toggle_tone[channel] = toggle_tone;
}

word32
sound_read_c030(dword64 dfcyc)
{
	sound_write_c030(dfcyc);
	return float_bus(dfcyc);
}

void
sound_write_c030(dword64 dfcyc)
{
	int	num;

	num = g_num_c030_fsamps;
	if(num >= MAX_C030_TIMES) {
		halt_printf("Too many clicks per vbl: %d\n", num);
		return;
	}

	g_c030_fsamps[num] = (float)(dfcyc * g_dsamps_per_dfcyc -
						g_last_sound_play_dsamp);
	g_num_c030_fsamps = num + 1;
	dbg_log_info(dfcyc, num, 0, 0xc030);

	doc_printf("touch c030, num this vbl: %04x\n", num);
}

