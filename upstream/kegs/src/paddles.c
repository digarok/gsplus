const char rcsid_paddles_c[] = "@(#)$KmKId: paddles.c,v 1.21 2023-05-19 13:52:54+00 kentd Exp $";

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

extern int g_mouse_raw_x;	/* from adb.c */
extern int g_mouse_raw_y;

dword64	g_paddle_trig_dfcyc = 0;
int	g_swap_paddles = 0;
int	g_invert_paddles = 0;
int	g_joystick_scale_factor_x = 0x100;
int	g_joystick_scale_factor_y = 0x100;
int	g_joystick_trim_amount_x = 0;
int	g_joystick_trim_amount_y = 0;

int	g_joystick_type = 0;	/* 0 = Keypad Joystick */
int	g_joystick_native_type1 = -1;
int	g_joystick_native_type2 = -1;
int	g_joystick_native_type = -1;

extern int g_paddle_buttons;

int	g_paddle_val[4] = { 0, 0, 0, 0 };
		/* g_paddle_val[0]: Joystick X coord, [1]:Y coord */

dword64	g_paddle_dfcyc[4] = { 0, 0, 0, 0 };
		/* g_paddle_dfcyc are the dfcyc the paddle goes to 0 */


void
paddle_fixup_joystick_type()
{
	/* If g_joystick_type points to an illegal value, change it */
	if(g_joystick_type == 2) {
		g_joystick_native_type = g_joystick_native_type1;
		if(g_joystick_native_type1 < 0) {
			g_joystick_type = 0;
		}
	}
	if(g_joystick_type == 3) {
		g_joystick_native_type = g_joystick_native_type2;
		if(g_joystick_native_type2 < 0) {
			g_joystick_type = 0;
		}
	}
}

void
paddle_trigger(dword64 dfcyc)
{
	/* Called by read/write to $c070 */
	g_paddle_trig_dfcyc = dfcyc;

	/* Determine what all the paddle values are right now */
	paddle_fixup_joystick_type();

	switch(g_joystick_type) {
	case 0:		/* Keypad Joystick */
		paddle_trigger_keypad(dfcyc);
		break;
	case 1:		/* Mouse Joystick */
		paddle_trigger_mouse(dfcyc);
		break;
	default:
		joystick_update(dfcyc);
	}
}

void
paddle_trigger_mouse(dword64 dfcyc)
{
	int	val_x, val_y;
	int	mouse_x, mouse_y;

	val_x = 0;

	mouse_x = g_mouse_raw_x;
	mouse_y = g_mouse_raw_y;
	/* mous_phys_x is 0->560, convert that to -32768 to + 32767 cyc */
	/*  So subtract 280 then mult by 117 */
	val_x = (mouse_x - 280) * 117;

	/* mous_phys_y is 0->384, convert that to -32768 to + 32767 cyc */
	/*  so subtract 192 then mult by 180 to overscale it a bit */
	val_y = (mouse_y - 192) * 180;

	g_paddle_val[0] = val_x;
	g_paddle_val[1] = val_y;
	g_paddle_val[2] = 32767;
	g_paddle_val[3] = 32767;
	g_paddle_buttons |= 0xc;
	paddle_update_trigger_dcycs(dfcyc);
}

void
paddle_trigger_keypad(dword64 dfcyc)
{
	int	get_y, val_x, val_y;

	val_x = adb_get_keypad_xy(get_y=0);
	val_y = adb_get_keypad_xy(get_y=1);
	/* val_x and val_y are already scale -32768 to +32768 */

	g_paddle_val[0] = val_x;
	g_paddle_val[1] = val_y;
	g_paddle_val[2] = 32767;
	g_paddle_val[3] = 32767;
	g_paddle_buttons |= 0xc;
	paddle_update_trigger_dcycs(dfcyc);
}

void
paddle_update_trigger_dcycs(dword64 dfcyc)
{
	dword64	trig_dfcyc;
	int	val, paddle_num, scale, trim;
	int	i;

	for(i = 0; i < 4; i++) {
		paddle_num = i;
		if(g_swap_paddles) {
			paddle_num = i ^ 1;
		}
		val = g_paddle_val[paddle_num];
		if(g_invert_paddles) {
			val = -val;
		}
		/* convert -32768 to +32768 into 0->2816.0 cycles (the */
		/* paddle delay const) */
		/* First multiply by the scale factor to adjust range */
		if(paddle_num & 1) {
			scale = g_joystick_scale_factor_y;
			trim = g_joystick_trim_amount_y;
		} else {
			scale = g_joystick_scale_factor_x;
			trim = g_joystick_trim_amount_x;
		}
#if 0
		if(i == 0) {
			printf("val was %04x(%d) * scale %03x = %d\n",
				val, val, scale, (val*scale)>>16);
		}
#endif
		val = (val * scale) >> 16;
		/* Val is now from -128 to + 128 since scale is */
		/*  256=1.0, 128 = 0.5 */
		val = val + 128 + trim;
		if(val >= 255) {
			val = 280;	/* increase range */
		}
		trig_dfcyc = dfcyc + (dword64)((val * (2816/255.0)) * 65536);
		g_paddle_dfcyc[i] = trig_dfcyc;
		if(i < 2) {
			dbg_log_info(dfcyc, (scale << 16) | (val & 0xffff),
					(trim << 16) | i, 0x70);
		}
	}
}

int
read_paddles(dword64 dfcyc, int paddle)
{
	dword64	trig_dfcyc;

	trig_dfcyc = g_paddle_dfcyc[paddle & 3];

	if(dfcyc < trig_dfcyc) {
		return 0x80;
	} else {
		return 0x00;
	}
}

void
paddle_update_buttons()
{
	paddle_fixup_joystick_type();
	joystick_update_buttons();
}
