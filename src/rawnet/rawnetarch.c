/** \file   rawnetarch.c
 * \brief   raw ethernet interface, architecture-dependant stuff
 *
 * \author  Bas Wassink <b.wassink@ziggo.nl
 */

/*
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

#include "rawnetarch.h"
#include "rawnetsupp.h"
#include <assert.h>

#ifdef HAVE_RAWNET

/* backward compatibility junk */


/** \brief  Transmit a frame
 *
 * \param[in]   force       Delete waiting frames in transmit buffer
 * \param[in]   onecoll     Terminate after just one collision
 * \param[in]   inhibit_crc Do not append CRC to the transmission
 * \param[in]   tx_pad_dis  Disable padding to 60 Bytes
 * \param[in]   txlength    Frame length
 * \param[in]   txframe     Pointer to the frame to be transmitted
 */
void rawnet_arch_transmit(int force, int onecoll, int inhibit_crc,
        int tx_pad_dis, int txlength, uint8_t *txframe)
{

	int ok;

#ifdef RAWNET_DEBUG_ARCH
    log_message(rawnet_arch_log,
            "rawnet_arch_transmit() called, with: force = %s, onecoll = %s, "
            "inhibit_crc=%s, tx_pad_dis=%s, txlength=%u",
            force ? "TRUE" : "FALSE", 
            onecoll ? "TRUE" : "FALSE",
            inhibit_crc ? "TRUE" : "FALSE",
            tx_pad_dis ? "TRUE" : "FALSE",
            txlength);
#endif

	ok = rawnet_arch_write(txframe, txlength);
	if (ok < 0) {
        log_message(rawnet_arch_log, "WARNING! Could not send packet!");		
	}
}

/**
 * \brief   Check if a frame was received
 *
 * This function checks if there was a frame received. If so, it returns 1,
 * else 0.
 *
 * If there was no frame, none of the parameters is changed!
 *
 * If there was a frame, the following actions are done:
 *
 * - at maximum \a plen byte are transferred into the buffer given by \a pbuffer
 * - \a plen gets the length of the received frame, EVEN if this is more
 *   than has been copied to \a pbuffer!
 * - if the dest. address was accepted by the hash filter, \a phashed is set,
 *   else cleared.
 * - if the dest. address was accepted by the hash filter, \a phash_index is
 *   set to the number of the rule leading to the acceptance
 * - if the receive was ok (good CRC and valid length), \a *prx_ok is set, else
 *   cleared.
 * - if the dest. address was accepted because it's exactly our MAC address
 *   (set by rawnet_arch_set_mac()), \a pcorrect_mac is set, else cleared.
 * - if the dest. address was accepted since it was a broadcast address,
 *   \a pbroadcast is set, else cleared.
 * - if the received frame had a crc error, \a pcrc_error is set, else cleared
 *
 * \param[out]      buffer          where to store a frame
 * \param[in,out]   plen            IN: maximum length of frame to copy;
 *                                  OUT: length of received frame OUT
 *                                  can be bigger than IN if received frame was
 *                                  longer than supplied buffer
 * \param[out]      phashed         set if the dest. address is accepted by the
 *                                  hash filter
 * \param[out]      phash_index     hash table index if hashed == TRUE
 * \param[out]      prx_ok          set if good CRC and valid length
 * \param[out]      pcorrect_mac    set if dest. address is exactly our IA
 * \param[out[      pbroadcast      set if dest. address is a broadcast address
 * \param[out]      pcrc_error      set if received frame had a CRC error
*/
int rawnet_arch_receive(uint8_t *pbuffer, int *plen, int  *phashed,
        int *phash_index, int *prx_ok, int *pcorrect_mac, int *pbroadcast,
        int *pcrc_error)
{
	int ok;

#ifdef RAWNET_DEBUG_ARCH
    log_message(rawnet_arch_log,
            "rawnet_arch_receive() called, with *plen=%u.",
            *plen);
#endif



    assert((*plen & 1) == 0);

    ok = rawnet_arch_read(pbuffer, *plen);
    if (ok <= 0) return 0;

    if (ok & 1) ++ok;
    *plen = ok;

    *phashed =
    *phash_index =
    *pbroadcast = 
    *pcorrect_mac =
    *pcrc_error = 0;

    /* this frame has been received correctly */
    *prx_ok = 1;
    return 1;

}




#endif  /* ifdef HAVE_RAWNET */
