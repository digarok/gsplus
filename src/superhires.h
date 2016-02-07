/*
 GSPLUS - Advanced Apple IIGS Emulator Environment
 Copyright (C) 2016 - Dagen Brock
 
 Copyright (C) 2010 by GSport contributors

 Based on the KEGS emulator written by and Copyright (C) 2003 Kent Dickey

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

/* This file is included by video.c */

void
SUPER_TYPE(byte *screen_data, int pixels_per_line, int y, int scan,
	word32 ch_mask, int use_a2vid_palette, int mode_640)
{
	word32	*palptr;
	word32	mem_ptr;
	byte	val0;
	int	x1, x2;
	byte	*b_ptr;
	word32	*img_ptr, *img_ptr2;
	word32	tmp, tmp2;
	word32	ch_tmp;
	byte	*slow_mem_ptr;
	int	shift_per;
	word32	pal;
	word32	pal_word;
	word32	pix0, pix1, pix2, pix3;
	word32	save_pix;
	int	offset, next_line_offset;
	int	dopr;

	mem_ptr = 0xa0 * y + 0x12000;
	tmp2 = 0;
	tmp = 0;

	shift_per = (1 << SHIFT_PER_CHANGE);
	if(use_a2vid_palette) {
		pal = (g_a2vid_palette & 0xf);
	} else {
		pal = (scan & 0xf);
	}

	if(SUPER_FILL) {
		ch_mask = -1;
		save_pix = 0;
	}

	if(use_a2vid_palette) {
		palptr = &(g_a2vid_palette_remap[0]);
	} else {
		palptr = &(g_palette_8to1624[pal * 16]);
	}

	dopr = 0;
#if 0
	if(y == 1) {
		dopr = 1;
		printf("superhires line %d has ch_mask: %08x\n", y, ch_mask);
	}
#endif
	for(x1 = 0; x1 < 0xa0; x1 += shift_per) {

		CH_LOOP_A2_VID(ch_mask, ch_tmp);

		pal_word = (pal << 28) + (pal << 20) + (pal << 12) +
			(pal << 4);

		if(mode_640 && !use_a2vid_palette) {
#ifdef GSPORT_LITTLE_ENDIAN
			pal_word += 0x04000c08;
#else
			pal_word += 0x080c0004;
#endif
		}


		slow_mem_ptr = &(g_slow_memory_ptr[mem_ptr + x1]);
		offset = y*2*pixels_per_line + x1*4;
		next_line_offset = pixels_per_line;
#if SUPER_PIXEL_SIZE == 16
		offset *= 2;
		next_line_offset *= 2;
#elif SUPER_PIXEL_SIZE == 32
		offset *= 4;
		next_line_offset *= 4;
#endif
		b_ptr = &screen_data[offset];
		img_ptr = (word32 *)b_ptr;
		img_ptr2 = (word32 *)(b_ptr + next_line_offset);

		if(mode_640) {
			for(x2 = 0; x2 < shift_per; x2++) {
				val0 = *slow_mem_ptr++;

				pix0 = (val0 >> 6) & 0x3;
				pix1 = (val0 >> 4) & 0x3;
				pix2 = (val0 >> 2) & 0x3;
				pix3 = val0 & 0x3;
				if(use_a2vid_palette || (SUPER_PIXEL_SIZE > 8)){
					pix0 = palptr[pix0+8];
					pix1 = palptr[pix1+12];
					pix2 = palptr[pix2+0];
					pix3 = palptr[pix3+4];
				}
#if SUPER_PIXEL_SIZE == 8
# ifdef GSPORT_LITTLE_ENDIAN
				tmp = (pix3 << 24) + (pix2 << 16) +
					(pix1 << 8) + pix0 + pal_word;
# else
				tmp = (pix0 << 24) + (pix1 << 16) +
					(pix2 << 8) + pix3 + pal_word;
# endif
				*img_ptr++ = tmp; *img_ptr2++ = tmp;
#elif SUPER_PIXEL_SIZE == 16
# ifdef GSPORT_LITTLE_ENDIAN
				tmp = (pix1 << 16) + pix0;
				tmp2 = (pix3 << 16) + pix2;
# else
				tmp = (pix0 << 16) + pix1;
				tmp2 = (pix2 << 16) + pix3;
# endif
				*img_ptr++ = tmp;
				*img_ptr++ = tmp2;
				*img_ptr2++ = tmp;
				*img_ptr2++ = tmp2;
#else /* SUPER_PIXEL_SIZE == 32 */
				*img_ptr++ = pix0;
				*img_ptr++ = pix1;
				*img_ptr++ = pix2;
				*img_ptr++ = pix3;
				*img_ptr2++ = pix0;
				*img_ptr2++ = pix1;
				*img_ptr2++ = pix2;
				*img_ptr2++ = pix3;
#endif

#if 0
				if(y == 1 && x1 == 0 && x2 == 0) {
					printf("y==1,x1,x2=0, %02x = %08x %08x "
						"%08x %08x, pal: %04x\n", val0,
						pix0, pix1, pix2, pix3, pal);
					printf("offset: %04x, nlo:%04x, ppl:"
						"%04x, %d\n", offset,
						next_line_offset,
						pixels_per_line, SUPER_PIXEL_SIZE);
				}
#endif
			}

		} else { /* 320 mode */
			for(x2 = 0; x2 < shift_per; x2++) {
				val0 = *slow_mem_ptr++;
				pix0 = (val0 >> 4);
				if(SUPER_FILL) {
					if(pix0) {
						save_pix = pix0;
					} else {
						pix0 = save_pix;
					}
				}
				pix1 = (val0 & 0xf);
				if(SUPER_FILL) {
					if(pix1) {
						save_pix = pix1;
					} else {
						pix1 = save_pix;
					}
				}
				if(use_a2vid_palette || (SUPER_PIXEL_SIZE > 8)){
					pix0 = palptr[pix0];
					pix1 = palptr[pix1];
				}
				if(dopr && x1 == 0) {
					printf("y:%d, x2:%d, val:%02x = %08x %08x\n", y, x2, val0, pix0, pix1);
				}
#if SUPER_PIXEL_SIZE == 8
# ifdef GSPORT_LITTLE_ENDIAN
				tmp = (pix1 << 24) + (pix1 << 16) +
					(pix0 << 8) + pix0 + pal_word;
# else
				tmp = (pix0 << 24) + (pix0 << 16) +
					(pix1 << 8) + pix1 + pal_word;
# endif
				*img_ptr++ = tmp; *img_ptr2++ = tmp;
#elif SUPER_PIXEL_SIZE == 16
				tmp = (pix0 << 16) + pix0;
				tmp2 = (pix1 << 16) + pix1;
				*img_ptr++ = tmp;
				*img_ptr++ = tmp2;
				*img_ptr2++ = tmp;
				*img_ptr2++ = tmp2;
#else /* SUPER_PIXEL_SIZE == 32 */
				*img_ptr++ = pix0;
				*img_ptr++ = pix0;
				*img_ptr++ = pix1;
				*img_ptr++ = pix1;
				*img_ptr2++ = pix0;
				*img_ptr2++ = pix0;
				*img_ptr2++ = pix1;
				*img_ptr2++ = pix1;
#endif
			}
		}
	}
}
