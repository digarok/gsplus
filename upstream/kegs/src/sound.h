#ifdef INCLUDE_RCSID_C
const char rcsid_sound_h[] = "@(#)$KmKId: sound.h,v 1.31 2023-05-04 19:35:29+00 kentd Exp $";
#endif

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

// Mockingboard contains two pairs.  Each pair is a 6522 interfacing
//  to an AY-8913 to generate sounds.  Eacho AY-8913 contains 3 channels of
//  sound.  Model each pair separately.

STRUCT(Mos6522) {
	byte	orb;
	byte	ora;
	byte	ddrb;
	byte	ddra;
	word32	timer1_latch;
	word32	timer1_counter;
	word32	timer2_latch;
	word32	timer2_counter;
	byte	sr;
	byte	acr;
	byte	pcr;
	byte	ifr;
	byte	ier;
};

STRUCT(Ay8913) {
	byte	regs[16];
	byte	reg_addr_latch;
	byte	toggle_tone[3];		// Channel A,B,C: 0 = low, 1 = high
	word32	tone_samp[3];
	word32	noise_val;
	word32	noise_samp;
	dword64	env_dsamp;
};

STRUCT(Mock_pair) {
	Mos6522	mos6522;
	Ay8913	ay8913;
};

STRUCT(Mockingboard) {
	Mock_pair pair[2];
	word32	disable_mask;
};

/* prototypes for win32snd_driver.c functions */
void win32snd_init(word32 *);
void win32snd_shutdown(void);
void child_sound_init_win32(void);
int win32_send_audio(byte *ptr, int size);

/* Prototypes for macsnd_driver.c functions */
int mac_send_audio(byte *ptr, int in_size);
void macsnd_init();

/* Prototypes for pulseaudio_driver.c functions */
int pulse_audio_init();
int pulse_audio_send_audio(byte *ptr, int in_size);
void pulse_audio_shutdown(void);

