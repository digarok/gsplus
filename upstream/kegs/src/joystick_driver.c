const char rcsid_joystick_driver_c[] = "@(#)$KmKId: joystick_driver.c,v 1.23 2023-09-26 02:59:00+00 kentd Exp $";

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

#ifdef __linux__
# include <linux/joystick.h>
#endif

#ifdef _WIN32
# include <windows.h>
# include <mmsystem.h>
#endif

extern int g_joystick_native_type1;		/* in paddles.c */
extern int g_joystick_native_type2;		/* in paddles.c */
extern int g_joystick_native_type;		/* in paddles.c */
extern int g_paddle_buttons;
extern int g_paddle_val[];


const char *g_joystick_dev = "/dev/js0";	/* default joystick dev file */
#define MAX_JOY_NAME	128

int	g_joystick_native_fd = -1;
int	g_joystick_num_axes = 0;
int	g_joystick_num_buttons = 0;

int	g_joystick_callback_buttons = 0;
int	g_joystick_callback_x = 32767;
int	g_joystick_callback_y = 32767;

#ifdef __linux__
# define JOYSTICK_DEFINED
void
joystick_init()
{
	char	joy_name[MAX_JOY_NAME];
	int	version, fd;
	int	i;

	fd = open(g_joystick_dev, O_RDONLY | O_NONBLOCK);
	if(fd < 0) {
		printf("Unable to open joystick dev file: %s, errno: %d\n",
			g_joystick_dev, errno);
		printf("Defaulting to mouse joystick\n");
		return;
	}

	strcpy(&joy_name[0], "Unknown Joystick");
	version = 0x800;

	ioctl(fd, JSIOCGNAME(MAX_JOY_NAME), &joy_name[0]);
	ioctl(fd, JSIOCGAXES, &g_joystick_num_axes);
	ioctl(fd, JSIOCGBUTTONS, &g_joystick_num_buttons);
	ioctl(fd, JSIOCGVERSION, &version);

	printf("Detected joystick: %s [%d axes, %d buttons vers: %08x]\n",
		joy_name, g_joystick_num_axes, g_joystick_num_buttons,
		version);

	g_joystick_native_type1 = 1;
	g_joystick_native_type2 = -1;
	g_joystick_native_fd = fd;
	for(i = 0; i < 4; i++) {
		g_paddle_val[i] = 32767;
	}
	g_paddle_buttons = 0xc;
}

/* joystick_update_linux() called from paddles.c.  Update g_paddle_val[] */
/*  and g_paddle_buttons with current information */
void
joystick_update(dword64 dfcyc)
{
	struct js_event js;	/* the linux joystick event record */
	int	mask, val, num, type, ret, len;
	int	i;

	/* suck up to 20 events, then give up */
	len = sizeof(struct js_event);
	for(i = 0; i < 20; i++) {
		ret = read(g_joystick_native_fd, &js, len);
		if(ret != len) {
			/* just get out */
			break;
		}
		type = js.type & ~JS_EVENT_INIT;
		val = js.value;
		num = js.number & 3;		/* clamp to 0-3 */
		switch(type) {
		case JS_EVENT_BUTTON:
			mask = 1 << num;
			if(val) {
				val = mask;
			}
			g_paddle_buttons = (g_paddle_buttons & ~mask) | val;
			break;
		case JS_EVENT_AXIS:
			/* val is -32767 to +32767 */
			g_paddle_val[num] = val;
			break;
		}
	}

	if(i > 0) {
		paddle_update_trigger_dcycs(dfcyc);
	}
}

void
joystick_update_buttons()
{
}
#endif /* LINUX */

#ifdef _WIN32
# define JOYSTICK_DEFINED
#undef JOYSTICK_DEFINED
	// HACK: remove
#if 0
void
joystick_init()
{
	JOYINFO info;
	JOYCAPS joycap;
	MMRESULT ret1, ret2;
	int	i;

	//	Check that there is a joystick device
	if(joyGetNumDevs() <= 0) {
		printf("No joystick hardware detected\n");
		g_joystick_native_type1 = -1;
		g_joystick_native_type2 = -1;
		return;
	}

	g_joystick_native_type1 = -1;
	g_joystick_native_type2 = -1;

	//	Check that at least joystick 1 or joystick 2 is available
	ret1 = joyGetPos(JOYSTICKID1, &info);
	ret2 = joyGetDevCaps(JOYSTICKID1, &joycap, sizeof(joycap));
	if(ret1 == JOYERR_NOERROR && ret2 == JOYERR_NOERROR) {
		g_joystick_native_type1 = JOYSTICKID1;
		printf("Joystick #1 = %s\n", joycap.szPname);
		g_joystick_native_type = JOYSTICKID1;
	}
	ret1 = joyGetPos(JOYSTICKID2, &info);
	ret2 = joyGetDevCaps(JOYSTICKID2, &joycap, sizeof(joycap));
	if(ret1 == JOYERR_NOERROR && ret2 == JOYERR_NOERROR) {
		g_joystick_native_type2 = JOYSTICKID2;
		printf("Joystick #2 = %s\n", joycap.szPname);
		if(g_joystick_native_type < 0) {
			g_joystick_native_type = JOYSTICKID2;
		}
	}

	for(i = 0; i < 4; i++) {
		g_paddle_val[i] = 32767;
	}
	g_paddle_buttons = 0xc;
}

void
joystick_update(dword64 dfcyc)
{
	JOYCAPS joycap;
	JOYINFO info;
	UINT	id;
	MMRESULT ret1, ret2;

	id = g_joystick_native_type;

	ret1 = joyGetDevCaps(id, &joycap, sizeof(joycap));
	ret2 = joyGetPos(id, &info);
	if(ret1 == JOYERR_NOERROR && ret2 == JOYERR_NOERROR) {
		g_paddle_val[0] = (info.wXpos - joycap.wXmin) * 32768 /
						(joycap.wXmax - joycap.wXmin);
		g_paddle_val[1] = (info.wYpos - joycap.wYmin) * 32768 /
						(joycap.wYmax - joycap.wYmin);
		if(info.wButtons & JOY_BUTTON1) {
			g_paddle_buttons = g_paddle_buttons | 1;
		} else {
			g_paddle_buttons = g_paddle_buttons & (~1);
		}
		if(info.wButtons & JOY_BUTTON2) {
			g_paddle_buttons = g_paddle_buttons | 2;
		} else {
			g_paddle_buttons = g_paddle_buttons & (~2);
		}
		paddle_update_trigger_dcycs(dfcyc);
	}
}

void
joystick_update_buttons()
{
	JOYINFOEX info;
	UINT id;

	id = g_joystick_native_type;

	info.dwSize = sizeof(JOYINFOEX);
	info.dwFlags = JOY_RETURNBUTTONS;
	if(joyGetPosEx(id, &info) == JOYERR_NOERROR) {
		if(info.dwButtons & JOY_BUTTON1) {
			g_paddle_buttons = g_paddle_buttons | 1;
		} else {
			g_paddle_buttons = g_paddle_buttons & (~1);
		}
		if(info.dwButtons & JOY_BUTTON2) {
			g_paddle_buttons = g_paddle_buttons | 2;
		} else {
			g_paddle_buttons = g_paddle_buttons & (~2);
		}
	}
}
#endif
#endif

#ifdef MAC
# define JOYSTICK_DEFINED

#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/usb/USBSpec.h>
#include <CoreFoundation/CoreFoundation.h>
// Headers are at: /Applications/Xcode.app/Contents/Developer/Platforms/
//      MacOSX.platform/Developer/SDKs/MacOSX.sdk/System/Library/
//      Frameworks/IOKit.framework/Headers
// Thanks to VirtualC64 and hidapi library for coding example

CFIndex g_joystick_min = 0;
CFIndex g_joystick_range = 256;
int	g_joystick_minmax_valid = 0;
int	g_joystick_dummy = 0;

void
hid_device_callback(void *ptr, IOReturn result, void *sender,
						IOHIDValueRef value)
{
	IOHIDElementRef element;
	word32	mask;
	int	usage_page, usage, ival, button;

	// This is a callback routine, and it's unclear to me what the state is
	//  For safety, do no printfs() (other than for debug).
	if((ptr || result || sender || 1) == 0) {
		printf("Bad\n");		// Avoid unused var warning
	}

	element = IOHIDValueGetElement(value);
	usage_page = IOHIDElementGetUsagePage(element);
	usage = IOHIDElementGetUsage(element);
	ival = IOHIDValueGetIntegerValue(value);
#if 0
	printf(" usage_page:%d, usage:%d, value:%d\n", usage_page, usage, ival);
#endif
	if((usage_page == kHIDPage_GenericDesktop) &&
				((usage >= kHIDUsage_GD_X) &&
				(usage <= kHIDUsage_GD_Y)) &&
				!g_joystick_minmax_valid) {
		g_joystick_min = IOHIDElementGetLogicalMin(element);
		g_joystick_range = IOHIDElementGetLogicalMax(element) + 1 -
							g_joystick_min;
		// printf("min:%lld range:%lld\n", (dword64)g_joystick_min,
		//				(dword64)g_joystick_range);
		if(g_joystick_range == 0) {
			g_joystick_range = 1;
		}
		g_joystick_minmax_valid = 1;
	}
	if((usage_page == kHIDPage_GenericDesktop) &&
						(usage == kHIDUsage_GD_X)) {
		g_joystick_callback_x = ((ival * 65536) / g_joystick_range) -
									32768;
		//printf("g_joystick_callback_x = %d\n", g_joystick_callback_x);
	}
	if((usage_page == kHIDPage_GenericDesktop) &&
						(usage == kHIDUsage_GD_Y)) {
		g_joystick_callback_y = ((ival * 65536) / g_joystick_range) -
									32768;
		//printf("g_joystick_callback_y = %d\n", g_joystick_callback_y);
	}
	if((usage_page == kHIDPage_Button) && (usage >= 1) && (usage <= 10)) {
		// Buttons: usage=1 is button 0, usage=2 is button 1, etc.
		button = (~usage) & 1;
		mask = 1 << button;
		//printf("Button %d (%d) pressed:%d\n", button, usage, ival);
		if(ival == 0) {		// Button released
			g_joystick_callback_buttons &= (~mask);
		} else {		// Button pressed
			g_joystick_callback_buttons |= mask;
		}
	}
}

int
hid_get_int_property(IOHIDDeviceRef device, CFStringRef key_cfstr)
{
	CFTypeRef ref;
	Boolean	bret;
	int	int_val;

	ref = IOHIDDeviceGetProperty(device, key_cfstr);
	if(ref) {
		bret = CFNumberGetValue((CFNumberRef)ref, kCFNumberIntType,
								&int_val);
		if(bret) {
			return int_val;
		}
	}
	return 0;
}

void
joystick_init()
{
	IOHIDManagerRef hid_mgr;
	CFSetRef devices_set;
	CFIndex	num_devices;
	IOHIDDeviceRef *devices_array, device;
	int	vendor, usage_page, usage;
	int	i;

	g_joystick_native_type1 = -1;
	g_joystick_native_type2 = -1;

	hid_mgr = IOHIDManagerCreate(kCFAllocatorDefault,
							kIOHIDOptionsTypeNone);
	IOHIDManagerSetDeviceMatching(hid_mgr, 0);
	IOHIDManagerOpen(hid_mgr, kIOHIDOptionsTypeNone);

	devices_set = IOHIDManagerCopyDevices(hid_mgr);
	num_devices = CFSetGetCount(devices_set);

	// Sets are hashtables, so we cannot directly access it.  The only way
	//  to iterate over all values is to use CFSetGetValues to get a simple
	//  array of the values, and iterate over that
	devices_array = calloc(num_devices, sizeof(IOHIDDeviceRef));
	CFSetGetValues(devices_set, (const void **)devices_array);

	for(i = 0; i < num_devices; i++) {
		device = devices_array[i];
		vendor = hid_get_int_property(device, CFSTR(kIOHIDVendorIDKey));
		// printf(" vendor: %d\n", vendor);
		usage_page = hid_get_int_property(device,
					CFSTR(kIOHIDDeviceUsagePageKey));
		usage = hid_get_int_property(device,
					CFSTR(kIOHIDDeviceUsageKey));
		// printf(" usage_page:%d, usage:%d\n", usage_page, usage);
		usage_page = hid_get_int_property(device,
					CFSTR(kIOHIDPrimaryUsagePageKey));
		usage = hid_get_int_property(device,
					CFSTR(kIOHIDPrimaryUsageKey));
		// printf(" primary_usage_page:%d, usage:%d\n", usage_page,
		//						usage);
		if(usage_page != kHIDPage_GenericDesktop) {
			continue;
		}
		if((usage != kHIDUsage_GD_Joystick) &&
				(usage != kHIDUsage_GD_GamePad) &&
				(usage != kHIDUsage_GD_MultiAxisController)) {
			continue;
		}
		printf(" JOYSTICK FOUND, vendor:%08x!\n", vendor);
		IOHIDDeviceOpen(device, kIOHIDOptionsTypeNone);
		IOHIDDeviceScheduleWithRunLoop(device, CFRunLoopGetCurrent(),
							kCFRunLoopCommonModes);
		IOHIDDeviceRegisterInputValueCallback(device,
					hid_device_callback, 0);
		g_joystick_native_type1 = 1;
		return;
		// Now, hid_device_callback will be called whenever a joystick
		//  value changes.  Only set global variables for joystick.
	}
}

void
joystick_update(dword64 dfcyc)
{
	int	i;

	if(dfcyc) {
		// Avoid unused parameter warnings
	}
	for(i = 0; i < 4; i++) {
		g_paddle_val[i] = 32767;
	}
	g_paddle_buttons = 0xc;
	if(g_joystick_native_type1 >= 0) {
		g_paddle_buttons = 0xc | (g_joystick_callback_buttons & 3);
		g_paddle_val[0] = g_joystick_callback_x;
		g_paddle_val[1] = g_joystick_callback_y;
		paddle_update_trigger_dcycs(dfcyc);
	}
}

void
joystick_update_buttons()
{
	if(g_joystick_native_type1 >= 0) {
		g_paddle_buttons = 0xc | (g_joystick_callback_buttons & 3);
	}
}
#endif

#ifndef JOYSTICK_DEFINED
/* stubs for the routines */
void
joystick_init()
{
	g_joystick_native_type1 = -1;
	g_joystick_native_type2 = -1;
	g_joystick_native_type = -1;
}

void
joystick_update(dword64 dfcyc)
{
	int	i;

	if(dfcyc) {
		// Avoid unused parameter warnings
	}
	for(i = 0; i < 4; i++) {
		g_paddle_val[i] = 32767;
	}
	g_paddle_buttons = 0xc;
	if(g_joystick_native_type1 >= 0) {
		g_paddle_buttons = 0xc | (g_joystick_callback_buttons & 3);
		g_paddle_val[0] = g_joystick_callback_x;
		g_paddle_val[1] = g_joystick_callback_y;
		paddle_update_trigger_dcycs(dfcyc);
	}
}

void
joystick_update_buttons()
{
	if(g_joystick_native_type1 >= 0) {
		g_paddle_buttons = 0xc | (g_joystick_callback_buttons & 3);
	}
}
#endif

void
joystick_callback_init(int native_type)
{
	g_joystick_native_type1 = native_type;
}

void
joystick_callback_update(word32 buttons, int paddle_x, int paddle_y)
{
	g_joystick_callback_buttons = (g_paddle_buttons & (~3)) | (buttons & 3);
	g_joystick_callback_x = paddle_x;
	g_joystick_callback_y = paddle_y;
}
