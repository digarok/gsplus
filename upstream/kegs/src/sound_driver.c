const char rcsid_sound_driver_c[] = "@(#)$KmKId: sound_driver.c,v 1.32 2023-06-05 18:41:09+00 kentd Exp $";

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

// Routines for managing sending sound samples to the hardware.  The
//  primary routines are snddrv_init() for initializing the sound hardware,
//  and snddrv_send_sound() which calls the driver for the sound hardware
//  in use to play samples.
// Linux forks a child process to manage /dev/dsp (so KEGS will not block)
//  so the lowerlevel routines for all sound hardware start with child_().

#include "defc.h"
#include "sound.h"

#if defined(__linux__) || defined(OSS)
# include <sys/soundcard.h>
#endif

#ifndef _WIN32
# include <sys/socket.h>
# include <netinet/in.h>
#endif
#include <errno.h>

#if defined(_WIN32) || defined(__CYGWIN__) || defined(MAC)
# define KEGS_CAN_FORK	0
#else
	// Linux, or other Unix, we may fork and run sound in the child
# define KEGS_CAN_FORK	1
#endif

extern int Verbose;

extern int g_audio_rate;
extern int g_audio_enable;
extern double g_last_sound_play_dsamp;
extern word32 *g_sound_shm_addr;

int	g_preferred_rate = 48000;
int	g_audio_socket = -1;
int	g_bytes_written = 0;
int	g_pulse_audio = 0;
int	g_pipe_fd[2] = { -1, -1 };
int	g_pipe2_fd[2] = { -1, -1 };

#define ZERO_BUF_SIZE		2048

word32 g_snd_zero_buf[ZERO_BUF_SIZE];

#define ZERO_PAUSE_SAFETY_SAMPS		(g_audio_rate >> 5)
#define ZERO_PAUSE_NUM_SAMPS		(4*g_audio_rate)

int	g_zeroes_buffered = 0;
int	g_zeroes_seen = 0;
int	g_sound_paused = 0;
int	g_childsnd_vbl = 0;
int	g_childsnd_pos = 0;

int child_sound_init_linux(void);

void
snddrv_init()
{
	word32	*shmaddr;
	int	size, ret, use_shm;

	ret = 0;
	if(ret) {			// Avoid unused var warning
	}

	g_zeroes_buffered = 0;
	g_zeroes_seen = 0;
	g_sound_paused = 0;

	g_childsnd_pos = 0;
	g_childsnd_vbl = 0;

	if(g_audio_enable == 0) {
		sound_set_audio_rate(g_preferred_rate);
		return;
	}
	printf("snddrv_init, g_audio_enable:%d\n", g_audio_enable);

	size = SOUND_SHM_SAMP_SIZE * SAMPLE_CHAN_SIZE;

	use_shm = KEGS_CAN_FORK;
#ifdef PULSE_AUDIO
	use_shm = 0;
#endif
	if(!use_shm) {
		/* windows and mac, and Linux Pulse Audio */
		shmaddr = malloc(size);
		memset(shmaddr, 0, size);
		g_sound_shm_addr = shmaddr;
#ifdef MAC
		macsnd_init();
		return;
#endif
#ifdef _WIN32
		win32snd_init(shmaddr);
		return;
#endif
#ifdef PULSE_AUDIO
		ret = pulse_audio_init(shmaddr);
		if(ret == 0) {
			g_pulse_audio = 1;
			return;		// Success!
		}
		free(shmaddr);
		g_sound_shm_addr = 0;
		use_shm = 1;
		// Otherwise, fall back on /dev/dsp
#endif
	}

	if(use_shm) {
		sound_child_fork(size);
	}
}

void
sound_child_fork(int size)
{
#if KEGS_CAN_FORK
	word32	*shmaddr;
	int	shmid, pid, tmp, ret;
	int	i;

	doc_printf("In sound_child_fork, size:%d\n", size);
	shmid = shmget(IPC_PRIVATE, size, IPC_CREAT | 0777);
	if(shmid < 0) {
		printf("sound_init: shmget ret: %d, errno: %d\n", shmid,
			errno);
		exit(2);
	}

	shmaddr = shmat(shmid, 0, 0);
	tmp = (int)PTR2WORD(shmaddr);
	if(tmp == -1) {
		printf("sound_init: shmat ret: %p, errno: %d\n", shmaddr,
			errno);
		exit(3);
	}

	ret = shmctl(shmid, IPC_RMID, 0);
	if(ret < 0) {
		printf("sound_init: shmctl ret: %d, errno: %d\n", ret, errno);
		exit(4);
	}

	g_sound_shm_addr = shmaddr;
	printf("shmaddr: %p\n", shmaddr);

	fflush(stdout);

	/* prepare pipe so parent can signal child each other */
	/*  pipe[0] = read side, pipe[1] = write end */
	ret = pipe(&g_pipe_fd[0]);
	if(ret < 0) {
		printf("sound_init: pipe ret: %d, errno: %d\n", ret, errno);
		exit(5);
	}
	ret = pipe(&g_pipe2_fd[0]);
	if(ret < 0) {
		printf("sound_init: pipe ret: %d, errno: %d\n", ret, errno);
		exit(5);
	}


	doc_printf("pipes: pipe_fd = %d, %d  pipe2_fd: %d,%d\n",
		g_pipe_fd[0], g_pipe_fd[1], g_pipe2_fd[0], g_pipe2_fd[1]);
	fflush(stdout);

	pid = fork();
	switch(pid) {
	case 0:
		/* child */
		/* close stdin and write-side of pipe */
		close(0);
		/* Close other fds to make sure X window fd is closed */
		for(i = 3; i < 100; i++) {
			if((i != g_pipe_fd[0]) && (i != g_pipe2_fd[1])) {
				close(i);
			}
		}
		close(g_pipe_fd[1]);		/*make sure write pipe closed*/
		close(g_pipe2_fd[0]);		/*make sure read pipe closed*/
		child_sound_loop(g_pipe_fd[0], g_pipe2_fd[1], g_sound_shm_addr);
		printf("Child sound loop returned\n");
		exit(0);
	case -1:
		/* error */
		printf("sound_init: fork ret: -1, errno: %d\n", errno);
		exit(6);
	default:
		/* parent */
		/* close read-side of pipe1, and the write side of pipe2 */
		close(g_pipe_fd[0]);
		close(g_pipe2_fd[1]);
		doc_printf("Child is pid: %d\n", pid);
	}

	parent_sound_get_sample_rate(g_pipe2_fd[0]);
#endif
	if(size) {
		// Avoid unused param warning
	}
}

void
parent_sound_get_sample_rate(int read_fd)
{
	word32	audio_rate, tmp;
	int	ret;

	ret = (int)read(read_fd, &audio_rate, 4);
	if(ret != 4) {
		printf("parent dying, could not get sample rate from child\n");
		printf("ret: %d, fd: %d errno:%d\n", ret, read_fd, errno);
		exit(1);
	}
	ret = (int)read(read_fd, &tmp, 4);
	if(ret != 4) {
		printf("parent dying, could not get audio status from child\n");
		printf("ret: %d, fd: %d errno:%d\n", ret, read_fd, errno);
		exit(1);
	}
	if(tmp == 0) {
		g_audio_enable = 0;
		printf("Failed to init Sound, turning off audio\n");
	}
	close(read_fd);

	sound_set_audio_rate(audio_rate);
}

void
snddrv_shutdown()
{
#ifdef _WIN32
	win32snd_shutdown();
#else
	if((g_audio_enable != 0) && (g_pipe_fd[1] >= 0)) {
		close(g_pipe_fd[1]);
	}
#endif
#ifdef PULSE_AUDIO
	if(g_pulse_audio) {
		pulse_audio_shutdown();
	}
#endif
}

void
snddrv_send_sound(int real_samps, int size)
{
	word32	tmp;
	int	ret, call_playit;

	if(g_audio_enable == 0) {
		printf("Entered send_sound but audio off!\n");
		exit(2);
	}

	if(real_samps) {
		tmp = size + 0xa2000000;
	} else {
		tmp = size + 0xa1000000;
	}
	//doc_log_rout("send_sound", -1, g_last_sound_play_dsamp,
	//					(real_samps << 30) + size);

	call_playit = 0;
#if defined(MAC) || defined(_WIN32)
	call_playit = 1;			// Never fork child mac/windows
#endif
	if(call_playit || g_pulse_audio) {
		child_sound_playit(tmp);
		return;
	}

	/* Although this looks like a big/little-endian issue, since the */
	/*  child is also reading an int, it just works with no byte swap */
	ret = (int)write(g_pipe_fd[1], &tmp, 4);
	if(ret != 4) {
		halt_printf("send_sound, wr ret: %d, errno: %d\n", ret, errno);
	}
}

void
child_sound_playit(word32 tmp)
{
	int	size;

	size = tmp & 0xffffff;

	//printf("child_sound_playit: %08x\n", tmp);

	if((tmp >> 24) == 0xa2) {			// play sound
		if(g_zeroes_buffered) {
			reliable_zero_write(g_zeroes_buffered);
		}

		g_zeroes_buffered = 0;
		g_zeroes_seen = 0;

		if((size + g_childsnd_pos) > SOUND_SHM_SAMP_SIZE) {
			reliable_buf_write(g_sound_shm_addr, g_childsnd_pos,
					SOUND_SHM_SAMP_SIZE - g_childsnd_pos);
			size = (g_childsnd_pos + size) - SOUND_SHM_SAMP_SIZE;
			g_childsnd_pos = 0;
		}

		reliable_buf_write(g_sound_shm_addr, g_childsnd_pos, size);

		if(g_sound_paused) {
			printf("Unpausing sound, zb: %d\n", g_zeroes_buffered);
			g_sound_paused = 0;
		}

	} else if((tmp >> 24) == 0xa1) {		// play zeroes
		if(g_sound_paused) {
			if(g_zeroes_buffered < ZERO_PAUSE_SAFETY_SAMPS) {
				g_zeroes_buffered += size;
			}
		} else {
			/* not paused, send it through */
			g_zeroes_seen += size;

			reliable_zero_write(size);

			if(g_zeroes_seen >= ZERO_PAUSE_NUM_SAMPS) {
				printf("Pausing sound\n");
				g_sound_paused = 1;
			}
		}
	} else {
		printf("tmp received bad: %08x\n", tmp);
		exit(3);
	}

	g_childsnd_pos += size;
	while(g_childsnd_pos >= SOUND_SHM_SAMP_SIZE) {
		g_childsnd_pos -= SOUND_SHM_SAMP_SIZE;
	}

	g_childsnd_vbl++;
	if(g_childsnd_vbl >= 60) {
		g_childsnd_vbl = 0;
#if 0
		printf("sound bytes written: %06x\n", g_bytes_written);
#endif
		g_bytes_written = 0;
	}
}

void
reliable_buf_write(word32 *shm_addr, int pos, int size)
{
	byte	*ptr;
	int	ret;

	if(size < 1 || pos < 0 || pos > SOUND_SHM_SAMP_SIZE ||
				size > SOUND_SHM_SAMP_SIZE ||
				(pos + size) > SOUND_SHM_SAMP_SIZE) {
		printf("reliable_buf_write: pos: %04x, size: %04x\n",
			pos, size);
		exit(1);
	}

	ptr = (byte *)&(shm_addr[pos]);
	size = size * 4;

	while(size > 0) {
		ret = child_send_samples(ptr, size);

		if(ret < 0) {
			printf("audio write, errno: %d %s\n", errno,
							strerror(errno));
			exit(1);
		}
		size = size - ret;
		ptr += ret;
		g_bytes_written += ret;
	}

}

void
reliable_zero_write(int amt)
{
	int	len;

	while(amt > 0) {
		len = MY_MIN(amt, ZERO_BUF_SIZE);
		reliable_buf_write(g_snd_zero_buf, 0, len);
		amt -= len;
	}
}


int
child_send_samples(byte *ptr, int size)
{
#ifdef _WIN32
	return win32_send_audio(ptr, size);
#else
# ifdef MAC
	return mac_send_audio(ptr, size);
# else
#  ifdef PULSE_AUDIO
	if(g_pulse_audio) {
		return pulse_audio_send_audio(ptr, size);
	}
#  endif
	return (int)write(g_audio_socket, ptr, size);
# endif
#endif
}

// child_sound_loop(): used by Linux child process as the main loop, to read
//  from pipe to get sample info every VBL, and use shm_addr to get samples
void
child_sound_loop(int read_fd, int write_fd, word32 *shm_addr)
{
	word32	tmp, did_init;
	int	ret, ret1, ret2;

	doc_printf("Child pipe fd: %d, shm_addr:%p\n", read_fd, shm_addr);

	g_audio_rate = g_preferred_rate;

	did_init = 0;
#if defined(__linux__) || defined(OSS)
	did_init = child_sound_init_linux();
#endif

	tmp = g_audio_rate;
	ret1 = (int)write(write_fd, &tmp, 4);
	tmp = did_init;
	ret2 = (int)write(write_fd, &tmp, 4);
	if((ret1) != 4 || (ret2 != 4)) {
		printf("Unable to send back audio rate to parent\n");
		printf("ret1: %d,%d fd: %d, errno:%d\n", ret1, ret2, write_fd,
								errno);
		exit(1);
	}
	doc_printf("Wrote to fd %d the audio rate\n", write_fd);

	close(write_fd);

	while(1) {
		errno = 0;
		ret = (int)read(read_fd, &tmp, 4);
		if(ret <= 0) {
			printf("child dying from ret: %d, errno: %d\n",
				ret, errno);
			break;
		}

		child_sound_playit(tmp);
	}

	close(g_audio_socket);

	exit(0);
}


#if defined(__linux__) || defined(OSS)
int
child_sound_init_linux()
{
	int	stereo, sample_size, rate, fmt, ret;

	g_audio_socket = open("/dev/dsp", O_WRONLY, 0);
	if(g_audio_socket < 0) {
		printf("open /dev/dsp failed, ret: %d, errno:%d\n",
			g_audio_socket, errno);
		return 0;
	}

#if 0
	fragment = 0x00200009;
	ret = ioctl(g_audio_socket, SNDCTL_DSP_SETFRAGMENT, &fragment);
	if(ret < 0) {
		printf("ioctl SETFRAGEMNT failed, ret:%d, errno:%d\n",
			ret, errno);
		return 0;
	}
#endif

	sample_size = 16;
	ret = ioctl(g_audio_socket, SNDCTL_DSP_SAMPLESIZE, &sample_size);
	if(ret < 0) {
		printf("ioctl SNDCTL_DSP_SAMPLESIZE failed, ret:%d, errno:%d\n",
			ret, errno);
		return 0;
	}

#ifdef KEGS_BIG_ENDIAN
	fmt = AFMT_S16_BE;
#else
	fmt = AFMT_S16_LE;
#endif
	ret = ioctl(g_audio_socket, SNDCTL_DSP_SETFMT, &fmt);
	if(ret < 0) {
		printf("ioctl SNDCTL_DSP_SETFMT failed, ret:%d, errno:%d\n",
			ret, errno);
		return 0;
	}

	stereo = 1;
	ret = ioctl(g_audio_socket, SNDCTL_DSP_STEREO, &stereo);
	if(ret < 0) {
		printf("ioctl SNDCTL_DSP_STEREO failed, ret:%d, errno:%d\n",
			ret, errno);
		return 0;
	}

	rate = g_audio_rate;
	ret = ioctl(g_audio_socket, SNDCTL_DSP_SPEED, &rate);
	if(ret < 0) {
		printf("ioctl SNDCTL_DSP_SPEED failed, ret:%d, errno:%d\n",
			ret, errno);
		return 0;
	}
	if(ret > 0) {
		rate = ret;	/* rate is returned value */
	}
	if(rate < 8000) {
		printf("Audio rate of %d which is < 8000!\n", rate);
		return 0;
	}
	g_audio_rate = rate;

	printf("Sound initialized\n");
	return 1;
}
#endif
