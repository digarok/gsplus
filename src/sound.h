/*
  GSPLUS - Advanced Apple IIGS Emulator Environment
  Based on the KEGS emulator written by Kent Dickey
  See COPYRIGHT.txt for Copyright information
	See LICENSE.txt for license (GPL v2)
*/

#if !defined(_WIN32) && !defined(__CYGWIN__)
# include <sys/ipc.h>
# include <sys/shm.h>
#endif

#define SOUND_SHM_SAMP_SIZE		(32*1024)

#define SAMPLE_SIZE		2
#define NUM_CHANNELS		2
#define SAMPLE_CHAN_SIZE	(SAMPLE_SIZE * NUM_CHANNELS)

STRUCT(Doc_reg) {
	double	dsamp_ev;
	double	dsamp_ev2;
	double	complete_dsamp;
	int	samps_left;
	word32	cur_acc;
	word32	cur_inc;
	word32	cur_start;
	word32	cur_end;
	word32	cur_mask;
	int	size_bytes;
	int	event;
	int	running;
	int	has_irq_pending;
	word32	freq;
	word32	vol;
	word32	waveptr;
	word32	ctl;
	word32	wavesize;
	word32	last_samp_val;
};

/* prototypes for win32snd_driver.c functions */
void win32snd_init(word32 *);
void win32snd_shutdown();
void win32snd_shutdown();
void child_sound_init_win32();
int win32_send_audio(byte *ptr, int size);

/* prototypes for sdl(2)snd_driver.c functions */
void sound_shutdown_sdl();
void sdlsnd_init(word32 *shmaddr);


/* Prototypes for macsnd_driver.c functions */
int mac_send_audio(byte *ptr, int in_size);
void child_sound_init_mac();
void macsnd_init(word32 *shmaddr);
