/*
 * This file is a consolidation of functions required for tfe
 * emulation taken from the following files
 *
 * lib.h   - Library functions.
 * util.h  - Miscellaneous utility functions.
 * crc32.h
 *
 * Written by
 *  Andreas Boose <viceteam@t-online.de>
 *  Ettore Perazzoli <ettore@comm2000.it>
 *  Manfred Spraul <manfreds@colorfullife.com>
 *  Andreas Matthies <andreas.matthies@gmx.net>
 *  Tibor Biczo <crown@mail.matav.hu>
 *  Spiro Trikaliotis <Spiro.Trikaliotis@gmx.de>*
 *
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */



#ifndef _TFESUPP_H
#define _TFESUPP_H

#include <stdio.h>
#include <stdint.h>

extern FILE*      g_fh;				// Filehandle for log file

extern void *lib_malloc(size_t size);
extern void *lib_realloc(void *p, size_t size);
extern void lib_free(void *ptr);
extern char *lib_stralloc(const char *str);

extern int util_string_set(char **str, const char *new_value);

extern unsigned long crc32_buf(const char *buffer, unsigned int len);

extern void rawnet_hexdump(const void *what, int count);
extern void rawnet_fix_incoming_packet(uint8_t *packet, unsigned size, const uint8_t real_mac[6], const uint8_t fake_mac[6]);
extern void rawnet_fix_outgoing_packet(uint8_t *packet, unsigned size, const uint8_t real_mac[6], const uint8_t fake_mac[6]);


#define log_message(level,...) do { fprintf(stderr,__VA_ARGS__); fputs("\n", stderr); } while (0)
#endif
