/*
 * cs8900.h - CS8900 Ethernet Core
 *
 * Written by
 *  Spiro Trikaliotis <Spiro.Trikaliotis@gmx.de>
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

#include <stdint.h>

#ifndef HAVE_RAWNET
  #error CS8900.H should not be included if HAVE_RAWNET is not defined!
#endif /* #ifdef HAVE_RAWNET */

#ifndef VICE_CS8900_H
#define VICE_CS8900_H

//#include <"types.h">

typedef struct snapshot_s snapshot_t;
extern int cs8900_snapshot_read_module(snapshot_t *s);
extern int cs8900_snapshot_write_module(snapshot_t *s);

extern int cs8900_init(void);
extern void cs8900_reset(void);

extern int cs8900_activate(const char *net_interface);
extern int cs8900_deactivate(void);
extern void cs8900_shutdown(void);

extern uint8_t cs8900_read(uint16_t io_address);
extern uint8_t cs8900_peek(uint16_t io_address);
extern void cs8900_store(uint16_t io_address, uint8_t byte);
extern int cs8900_dump(void);

/*
 This is a helper for cs8900_receive() to determine if the received frame should be accepted
 according to the settings.

 This function is even allowed to be called (indirectly via rawnet_should_accept) in rawnetarch.c
 from rawnet_arch_receive() if necessary, and must be registered using rawnet_set_should_accept_func
 at init time.
*/
extern int cs8900_should_accept(unsigned char *buffer, int length, int *phashed, int *phash_index, int *pcorrect_mac, int *pbroadcast, int *pmulticast);

#endif
