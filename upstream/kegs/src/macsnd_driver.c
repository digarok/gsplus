const char rcsid_macsnd_driver_c[] = "@(#)$KmKId: macsnd_driver.c,v 1.20 2022-04-03 13:38:47+00 kentd Exp $";

/************************************************************************/
/*			KEGS: Apple //gs Emulator			*/
/*			Copyright 2002-2022 by Kent Dickey		*/
/*									*/
/*	This code is covered by the GNU GPL v3				*/
/*	See the file COPYING.txt or https://www.gnu.org/licenses/	*/
/*	This program is provided with no warranty			*/
/*									*/
/*	The KEGS web page is kegs.sourceforge.net			*/
/*	You may contact the author at: kadickey@alumni.princeton.edu	*/
/************************************************************************/

// Headers at: /Applications/Xcode.app/Contents/Developer/Platforms/
//      MacOSX.platform/Developer/SDKs/MacOSX.sdk/System/Library/
//      Frameworks/AudioToolbox.framework/Headers

// Some ideas from SDL code:
//  http://www-personal.umich.edu/~bazald/l/api/_s_d_l__coreaudio_8c_source.html

#include "defc.h"
#include "sound.h"

#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <unistd.h>

// Mac OS X 12.0 deprecates kAudioObjectPropertyElementMaster.  So now we
//  need to use kAudioObjectPropertyElementMain, and set it to ...Master if it
//  isn't already set.  This is beyond dumb
#if defined(__MAC_OS_X_VERSION_MAX_ALLOWED)
# if __MAC_OS_X_VERSION_MAX_ALLOWED < 120000
#  define kAudioObjectPropertyElementMain kAudioObjectPropertyElementMaster
# endif
#endif

#define MACSND_REBUF_SIZE	(64*1024)

word32	g_macsnd_rebuf[MACSND_REBUF_SIZE];
volatile int g_macsnd_rd_pos;
volatile int g_macsnd_wr_pos;
volatile int g_macsnd_playing = 0;
int g_macsnd_channel_warn = 0;
extern int g_sound_min_samples;			// About 33ms
extern int g_sound_max_multiplier;		// About 6, so 6*33 ~= 200ms.

extern int Verbose;

extern int g_preferred_rate;
extern word32 *g_sound_shm_addr;
extern int g_sound_size;

int g_call_num = 0;

AURenderCallbackStruct g_callback_struct = { 0 };

static OSStatus
audio_callback(void *in_ref_ptr, AudioUnitRenderActionFlags *aura_flags_ptr,
		const AudioTimeStamp *a_timestamp_ptr, UInt32 bus_number,
		UInt32 in_num_frame, AudioBufferList *abuflist_ptr)
{
	word32	*wptr;
	int	num_buffers, num_channels, num_samps, sample_num, samps_avail;
	int	max_samples, min_samples;
	int	i, j;

	if(in_ref_ptr || aura_flags_ptr || a_timestamp_ptr || bus_number) {
		// Avoid unused parameter warning
	}
#if 0
	printf("CB: audio_callback called, in_ref:%p, bus:%d, in_frame:%d\n",
		in_ref_ptr, bus_number, in_num_frame);
#endif
	num_buffers = abuflist_ptr->mNumberBuffers;
	min_samples = g_sound_min_samples;
	max_samples = min_samples * g_sound_max_multiplier;
#if 0
	printf("CB: num_buffers: %d. sample_time:%lf, host_time:%016llx "
		"rate_scalar:%lf, word_clock_time:%016llx, flags:%04x\n",
		num_buffers, a_timestamp_ptr->mSampleTime,
		a_timestamp_ptr->mHostTime, a_timestamp_ptr->mRateScalar,
		a_timestamp_ptr->mWordClockTime, a_timestamp_ptr->mFlags);
#endif

	sample_num = g_macsnd_rd_pos;
	samps_avail = (g_macsnd_wr_pos - sample_num) & (MACSND_REBUF_SIZE - 1);
	if((int)in_num_frame > samps_avail) {
		// We don't have enough samples, must pause
		g_macsnd_playing = 0;
		sample_num = g_macsnd_wr_pos;		// Eat remaining samps
	}
	if((g_macsnd_playing == 0) &&
			(samps_avail > (min_samples + (int)in_num_frame))) {
		// We can unpause
		g_macsnd_playing = 1;
	}
	if(g_macsnd_playing && (samps_avail > max_samples)) {
		printf("JUMP SAMPLE_NUM by %d samples!\n", max_samples / 2);
		sample_num += (max_samples / 2);
		sample_num = sample_num & (MACSND_REBUF_SIZE - 1);
	}
#if 0
	printf("CB: in_frame:%d, samps_avail:%d, playing:%d\n", in_num_frame,
			samps_avail, g_macsnd_playing);
#endif

	for(i = 0; i < num_buffers; i++) {
		num_channels = abuflist_ptr->mBuffers[i].mNumberChannels;
		if((num_channels != 2) && !g_macsnd_channel_warn) {
			printf("mNumberChannels:%d\n", num_channels);
			g_macsnd_channel_warn = 1;
		}
		num_samps = abuflist_ptr->mBuffers[i].mDataByteSize / 4;
		wptr = (word32 *)abuflist_ptr->mBuffers[i].mData;
#if 0
		printf("CB buf[%d]: num_ch:%d, num_samps:%d, wptr:%p\n", i,
			num_channels, num_samps, wptr);
#endif
#if 0
		if((g_call_num & 31) == 0) {
			printf("%d play %d samples, samps_avail:%d\n",
				g_call_num, num_samps, samps_avail);
		}
#endif
		if(g_macsnd_playing) {
			for(j = 0; j < num_samps; j++) {
				wptr[j] = g_macsnd_rebuf[sample_num];
				sample_num++;
				if(sample_num >= MACSND_REBUF_SIZE) {
					sample_num = 0;
				}
			}
		} else {
			for(j = 0; j < num_samps; j++) {
				wptr[j] = 0;
			}
		}
	}

	g_call_num++;
	g_macsnd_rd_pos = sample_num;

	return 0;
}

int
mac_send_audio(byte *ptr, int in_size)
{
	word32	*wptr, *macptr;
	int	samps, sample_num;
	int	i;

	samps = in_size / 4;
	wptr = (word32 *)ptr;
	sample_num = g_macsnd_wr_pos;
	macptr = &(g_macsnd_rebuf[0]);
	for(i = 0; i < samps; i++) {
		macptr[sample_num] = *wptr++;
		sample_num++;
		if(sample_num >= MACSND_REBUF_SIZE) {
			sample_num = 0;
		}
	}

	g_macsnd_wr_pos = sample_num;

	return in_size;
}

AudioObjectPropertyAddress g_aopa = { 0 };

void
macsnd_init()
{
	AudioComponentInstance ac_inst;
	AudioStreamBasicDescription *str_desc_ptr;
	AudioComponentDescription *ac_descr_ptr;
	AudioComponent	ac;
	AudioDeviceID	device;
	OSStatus	result;
	UInt32		size;

	g_macsnd_rd_pos = 0;
	g_macsnd_wr_pos = 0;
	mac_printf("macsnd_init called\n");

	g_aopa.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
	g_aopa.mScope = kAudioObjectPropertyScopeGlobal;
	g_aopa.mElement = kAudioObjectPropertyElementMain;

	size = 4;
	result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &g_aopa,
					0, 0, &size, &device);
	if(result != 0) {
		printf("AudioObjectGetPropertData on DefaultOutputDevice:%d\n",
									result);
		return;
	}
	mac_printf("Audio Device number: %d\n", device);

	str_desc_ptr = calloc(1, sizeof(AudioStreamBasicDescription));
	str_desc_ptr->mFormatID = kAudioFormatLinearPCM;
	str_desc_ptr->mFormatFlags = kLinearPCMFormatFlagIsPacked |
					kLinearPCMFormatFlagIsSignedInteger;
	str_desc_ptr->mChannelsPerFrame = 2;
	str_desc_ptr->mSampleRate = g_preferred_rate;
	str_desc_ptr->mFramesPerPacket = 1;
	str_desc_ptr->mBitsPerChannel = 16;		// 16-bit samples
	str_desc_ptr->mBytesPerFrame = (16 * 2) / 8;
	str_desc_ptr->mBytesPerPacket = str_desc_ptr->mBytesPerFrame;

	ac_descr_ptr = calloc(1, sizeof(AudioComponentDescription));
	ac_descr_ptr->componentType = kAudioUnitType_Output;
	ac_descr_ptr->componentManufacturer = kAudioUnitManufacturer_Apple;
	ac_descr_ptr->componentSubType = kAudioUnitSubType_DefaultOutput;
	ac = AudioComponentFindNext(0, ac_descr_ptr);
	mac_printf("AudioComponentFindNext ret: %p\n", ac);
	if(ac == 0) {
		exit(1);
	}

	result = AudioComponentInstanceNew(ac, &ac_inst);
	mac_printf("AudioComponentInstanceNew ret:%d\n", result);
	if(result != 0) {
		exit(1);
	}

	result = AudioUnitSetProperty(ac_inst,
			kAudioOutputUnitProperty_CurrentDevice,
			kAudioUnitScope_Global, 0, &device,
			sizeof(device));
	mac_printf("AudioUnitSetProperty CurrentDevice ret: %d\n", result);
	if(result != 0) {
		exit(1);
	}

	result = AudioUnitSetProperty(ac_inst,
			kAudioUnitProperty_StreamFormat,
			kAudioUnitScope_Input, 0, str_desc_ptr,
			sizeof(*str_desc_ptr));
	mac_printf("AudioUnitSetProperty StreamFormat ret: %d\n", result);
	if(result != 0) {
		exit(1);
	}

	g_callback_struct.inputProc = audio_callback;
	g_callback_struct.inputProcRefCon = (void *)1;
	result = AudioUnitSetProperty(ac_inst,
			kAudioUnitProperty_SetRenderCallback,
			kAudioUnitScope_Input, 0, &g_callback_struct,
			sizeof(g_callback_struct));

	mac_printf("AudioUnitSetProperty SetRenderCallback ret: %d\n", result);
	if(result != 0) {
		exit(1);
	}

	result = AudioUnitInitialize(ac_inst);
	mac_printf("AudioUnitInitialize ret: %d\n", result);
	if(result != 0) {
		exit(1);
	}

	result = AudioOutputUnitStart(ac_inst);
	mac_printf("AudioOutputUnitStart ret: %d\n", result);
	if(result != 0) {
		exit(1);
	}

	sound_set_audio_rate(g_preferred_rate);

	mac_printf("End of child_sound_init_mac\n");
	fflush(stdout);
}

