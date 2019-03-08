/** \file   rawnetarch_unix.c
 * \brief   Raw ethernet interface, architecture-dependent stuff
 *
 * \author  Spiro Trikaliotis <Spiro.Trikaliotis@gmx.de>
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 *
 * These functions let the UI enumerate the available interfaces.
 *
 * First, rawnet_arch_enumadapter_open() is used to start enumeration.
 *
 * rawnet_arch_enumadapter() is then used to gather information for each adapter
 * present on the system, where:
 *
 * ppname points to a pointer which will hold the name of the interface
 * ppdescription points to a pointer which will hold the description of the
 * interface
 *
 * For each of these parameters, new memory is allocated, so it has to be
 * freed with lib_free(), except ppdescription, which can be `NULL`, though
 * calling lib_free() on `NULL` is safe.
 *
 * rawnet_arch_enumadapter_close() must be used to stop processing.
 *
 * Each function returns 1 on success, and 0 on failure.
 * rawnet_arch_enumadapter() only fails if there is no more adpater; in this
 * case, *ppname and *ppdescription are not altered.
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

#include <stdint.h>

// #include "vice.h"

#ifdef HAVE_RAWNET

#include <pcap.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #include "lib.h"
// #include "log.h"
#include "rawnetarch.h"
#include "rawnetsupp.h"

/*
 *  FIXME:  rename all remaining tfe_ stuff to rawnet_
 */

#define RAWNET_DEBUG_WARN 1 /* this should not be deactivated
                             * If this should not be deactived, why is this
                             * here at all? --compyx
                             */


/** \brief  Only select devices that are PCAP_IF_UP
 *
 * Since on Linux pcap_findalldevs() returns all interfaces, including special
 * kernal devices such as nfqueue, filtering the list returned by pcap makes
 * sense. Should this filtering cause trouble on other Unices, this define can
 * be guarded with #ifdef SOME_UNIX_VERSION to disable the filtering.
 */
#ifdef PCAP_IF_UP
#define RAWNET_ONLY_IF_UP
#endif


/** #define RAWNET_DEBUG_ARCH 1 **/
/** #define RAWNET_DEBUG_PKTDUMP 1 **/

/* ------------------------------------------------------------------------- */
/*    variables needed                                                       */

// static log_t rawnet_arch_log = LOG_ERR;


/** \brief  Iterator for the list returned by pcap_findalldevs()
 */
static pcap_if_t *rawnet_pcap_dev_iter = NULL;


/** \brief  Device list returned by pcap_findalldevs()
 *
 * Can be `NULL` since pcap_findalldevs() considers not finding any devices a
 * succesful outcome.
 */
static pcap_if_t *rawnet_pcap_dev_list = NULL;


static pcap_t *rawnet_pcap_fp = NULL;
static char *rawnet_device_name = NULL;


/** \brief  Buffer for pcap error messages
 */
static char rawnet_pcap_errbuf[PCAP_ERRBUF_SIZE];


#ifdef RAWNET_DEBUG_PKTDUMP

static void debug_output( const char *text, uint8_t *what, int count )
{
    char buffer[256];
    char *p = buffer;
    char *pbuffer1 = what;
    int len1 = count;
    int i;

    sprintf(buffer, "\n%s: length = %u\n", text, len1);
    fprintf(stderr, "%s", buffer);
    do {
        p = buffer;
        for (i=0; (i<8) && len1>0; len1--, i++) {
            sprintf(p, "%02x ", (unsigned int)(unsigned char)*pbuffer1++);
            p += 3;
        }
        *(p-1) = '\n'; *p = 0;
        fprintf(stderr, "%s", buffer);
    } while (len1>0);
}
#endif /* #ifdef RAWNET_DEBUG_PKTDUMP */


int rawnet_arch_enumadapter_open(void)
{
    if (pcap_findalldevs(&rawnet_pcap_dev_list, rawnet_pcap_errbuf) == -1) {
        log_message(rawnet_arch_log,
                "ERROR in rawnet_arch_enumadapter_open: pcap_findalldevs: '%s'",
                rawnet_pcap_errbuf);
        return 0;
    }

    if (!rawnet_pcap_dev_list) {
        log_message(rawnet_arch_log,
                "ERROR in rawnet_arch_enumadapter_open, finding all pcap "
                "devices - Do we have the necessary privilege rights?");
        return 0;
    }

    rawnet_pcap_dev_iter = rawnet_pcap_dev_list;
    return 1;
}


/** \brief  Get current pcap device iterator values
 *
 * The \a ppname and \a ppdescription are heap-allocated via lib_stralloc()
 * and should thus be freed after use with lib_free(). Please not that
 * \a ppdescription can be `NULL` due to pcap_if_t->description being `NULL`,
 * so check against `NULL` before using it. Calling lib_free() on it is safe
 * though, free(`NULL`) is guaranteed to just do nothing.
 *
 * \param[out]  ppname          device name
 * \param[out]  ppdescription   device description
 *
 * \return  bool (1 on success, 0 on failure)
 */
int rawnet_arch_enumadapter(char **ppname, char **ppdescription)
{
#ifdef RAWNET_ONLY_IF_UP
    /* only select devices that are up */
    while (rawnet_pcap_dev_iter != NULL
            && !(rawnet_pcap_dev_iter->flags & PCAP_IF_UP)) {
        rawnet_pcap_dev_iter = rawnet_pcap_dev_iter->next;
    }
#endif

    if (rawnet_pcap_dev_iter == NULL) {
        return 0;
    }

    *ppname = lib_stralloc(rawnet_pcap_dev_iter->name);
    /* carefull: pcap_if_t->description can be NULL and lib_stralloc() fails on
     * passing `NULL` */
    if (rawnet_pcap_dev_iter->description != NULL) {
        *ppdescription = lib_stralloc(rawnet_pcap_dev_iter->description);
    } else {
        *ppdescription = NULL;
    }

    rawnet_pcap_dev_iter = rawnet_pcap_dev_iter->next;

    return 1;
}

int rawnet_arch_enumadapter_close(void)
{
    if (rawnet_pcap_dev_list) {
        pcap_freealldevs(rawnet_pcap_dev_list);
        rawnet_pcap_dev_list = NULL;
    }
    return 1;
}

static int rawnet_pcap_open_adapter(const char *interface_name) 
{
    rawnet_pcap_fp = pcap_open_live((char*)interface_name, 1700, 1, 20, rawnet_pcap_errbuf);
    if ( rawnet_pcap_fp == NULL) {
        log_message(rawnet_arch_log, "ERROR opening adapter: '%s'", rawnet_pcap_errbuf);
        return 0;
    }

    if (pcap_setnonblock(rawnet_pcap_fp, 1, rawnet_pcap_errbuf) < 0) {
        log_message(rawnet_arch_log, "WARNING: Setting PCAP to non-blocking failed: '%s'", rawnet_pcap_errbuf);
    }

    /* Check the link layer. We support only Ethernet for simplicity. */
    if (pcap_datalink(rawnet_pcap_fp) != DLT_EN10MB) {
        log_message(rawnet_arch_log, "ERROR: TFE works only on Ethernet networks.");
        pcap_close(rawnet_pcap_fp);
        rawnet_pcap_fp = NULL;
        return 0;
    }
    rawnet_device_name = strdup(interface_name);

    return 1;
}

/* ------------------------------------------------------------------------- */
/*    the architecture-dependend functions                                   */

int rawnet_arch_init(void)
{
    //rawnet_arch_log = log_open("TFEARCH");

    return 1;
}

void rawnet_arch_pre_reset(void)
{
#ifdef RAWNET_DEBUG_ARCH
    log_message( rawnet_arch_log, "rawnet_arch_pre_reset()." );
#endif
}

void rawnet_arch_post_reset(void)
{
#ifdef RAWNET_DEBUG_ARCH
    log_message( rawnet_arch_log, "rawnet_arch_post_reset()." );
#endif
}

int rawnet_arch_activate(const char *interface_name)
{
#ifdef RAWNET_DEBUG_ARCH
    log_message( rawnet_arch_log, "rawnet_arch_activate()." );
#endif
    if (!rawnet_pcap_open_adapter(interface_name)) {
        return 0;
    }
    return 1;
}

void rawnet_arch_deactivate( void )
{
#ifdef RAWNET_DEBUG_ARCH
    log_message( rawnet_arch_log, "rawnet_arch_deactivate()." );
#endif
}

void rawnet_arch_set_mac( const uint8_t mac[6] )
{
#ifdef RAWNET_DEBUG_ARCH
    log_message( rawnet_arch_log, "New MAC address set: %02X:%02X:%02X:%02X:%02X:%02X.", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] );
#endif
}

void rawnet_arch_set_hashfilter(const uint32_t hash_mask[2])
{
#ifdef RAWNET_DEBUG_ARCH
    log_message( rawnet_arch_log, "New hash filter set: %08X:%08X.", hash_mask[1], hash_mask[0]);
#endif
}

/* int bBroadcast   - broadcast */
/* int bIA          - individual address (IA) */
/* int bMulticast   - multicast if address passes the hash filter */
/* int bCorrect     - accept correct frames */
/* int bPromiscuous - promiscuous mode */
/* int bIAHash      - accept if IA passes the hash filter */

void rawnet_arch_recv_ctl(int bBroadcast, int bIA, int bMulticast, int bCorrect, int bPromiscuous, int bIAHash)
{
#ifdef RAWNET_DEBUG_ARCH
    log_message(rawnet_arch_log, "rawnet_arch_recv_ctl() called with the following parameters:" );
    log_message(rawnet_arch_log, "\tbBroadcast   = %s", bBroadcast ? "TRUE" : "FALSE");
    log_message(rawnet_arch_log, "\tbIA          = %s", bIA ? "TRUE" : "FALSE");
    log_message(rawnet_arch_log, "\tbMulticast   = %s", bMulticast ? "TRUE" : "FALSE");
    log_message(rawnet_arch_log, "\tbCorrect     = %s", bCorrect ? "TRUE" : "FALSE");
    log_message(rawnet_arch_log, "\tbPromiscuous = %s", bPromiscuous ? "TRUE" : "FALSE");
    log_message(rawnet_arch_log, "\tbIAHash      = %s", bIAHash ? "TRUE" : "FALSE");
#endif
}

void rawnet_arch_line_ctl(int bEnableTransmitter, int bEnableReceiver )
{
#ifdef RAWNET_DEBUG_ARCH
    log_message(rawnet_arch_log,
            "rawnet_arch_line_ctl() called with the following parameters:");
    log_message(rawnet_arch_log,
            "\tbEnableTransmitter = %s", bEnableTransmitter ? "TRUE" : "FALSE");
    log_message(rawnet_arch_log, 
            "\tbEnableReceiver    = %s", bEnableReceiver ? "TRUE" : "FALSE");
#endif
}


/** \brief  Raw pcap packet
 */
typedef struct rawnet_pcap_internal_s {
    unsigned int len;   /**< length of packet data */
    uint8_t *buffer;    /**< packet data */
} rawnet_pcap_internal_t;


/** \brief  Callback function invoked by libpcap for every incoming packet
 *
 * \param[in,out]   param       reference to internal VICE packet struct
 * \param[in]       header      pcap header
 * \param[in]       pkt_data    packet data
 */
static void rawnet_pcap_packet_handler(u_char *param,
        const struct pcap_pkthdr *header, const u_char *pkt_data)
{
    rawnet_pcap_internal_t *pinternal = (void*)param;

    /* determine the count of bytes which has been returned,
     * but make sure not to overrun the buffer
     */
    if (header->caplen < pinternal->len) {
        pinternal->len = header->caplen;
    }

    memcpy(pinternal->buffer, pkt_data, pinternal->len);
}


/** \brief  Receives a frame
 *
 * If there's none, it returns a -1 in \a pinternal->len, if there is one,
 * it returns the length of the frame in bytes in \a pinternal->len.
 *
 * It copies the frame to \a buffer and returns the number of copied bytes as
 * the return value.
 *
 * \param[in,out]   pinternal   internal VICE packet struct
 *
 * \note    At most 'len' bytes are copied.
 *
 * \return  number of bytes copied or -1 on failure
 */
static int rawnet_arch_receive_frame(rawnet_pcap_internal_t *pinternal)
{
    int ret = -1;

    /* check if there is something to receive */
    if (pcap_dispatch(rawnet_pcap_fp, 1, rawnet_pcap_packet_handler,
                (void*)pinternal) != 0) {
        /* Something has been received */
        ret = pinternal->len;
    }

#ifdef RAWNET_DEBUG_ARCH
    log_message(rawnet_arch_log,
            "rawnet_arch_receive_frame() called, returns %d.", ret);
#endif

    return ret;
}

int rawnet_arch_read(void *buffer, int nbyte) {

    int len;

    rawnet_pcap_internal_t internal = { nbyte, (uint8_t *)buffer };

    len = rawnet_arch_receive_frame(&internal);

    if (len <= 0) return len;

#ifdef RAWNET_DEBUG_PKTDUMP
    debug_output("Received frame: ", internal.buffer, internal.len);
#endif /* #ifdef RAWNET_DEBUG_PKTDUMP */
    return len;

}


int rawnet_arch_write(const void *buffer, int nbyte) {

#ifdef RAWNET_DEBUG_PKTDUMP
    debug_output("Transmit frame: ", buffer, nbyte);
#endif /* #ifdef RAWNET_DEBUG_PKTDUMP */

    if (pcap_sendpacket(rawnet_pcap_fp, buffer, nbyte) < 0) {
        log_message(rawnet_arch_log, "WARNING! Could not send packet!");
        return -1;
    }
    return nbyte;
}



/** \brief  Find default device on which to capture
 *
 * \return  name of standard interface
 *
 * \note    pcap_lookupdev() has been deprecated, so the correct way to get
 *          the default device is to use the first entry returned by
 *          pcap_findalldevs().
 *          See http://www.tcpdump.org/manpages/pcap_lookupdev.3pcap.html
 *
 * \return  default interface name or `NULL` when not found
 *
 * \note    free the returned value with lib_free() if not `NULL`
 */
char *rawnet_arch_get_standard_interface(void)
{
    char *dev = NULL;
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_if_t *list;

    if (pcap_findalldevs(&list, errbuf) == 0 && list != NULL) {
        dev = lib_stralloc(list[0].name);
        pcap_freealldevs(list);
    }
    return dev;
}

extern int rawnet_arch_get_mtu(void) {

    #if defined(__linux__)
    int fd;
    int ok;
    struct ifreq ifr;

    if (!rawnet_device_name) return -1;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return -1;

    memset(&ifr, 0, sizeof(ifr));
    strcpy(ifr.ifr_name, rawnet_device_name);
    ok = ioctl(fd, SIOCGIFMTU, (void *)&ifr);
    close(fd);
    if (ok < 0) return -1;
    return ifr.ifr_mtu;
    #endif

    return -1;
}

extern int rawnet_arch_get_mac(uint8_t mac[6]) {

    #if defined(__linux__)

    int fd;
    struct ifreq ifr;
    int ok;

    if (!rawnet_device_name) return -1;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return -1;

    memset(&ifr, 0, sizeof(ifr));
    strcpy(ifr.ifr_name, rawnet_device_name);
    ok = ioctl(fd, SIOCGIFHWADDR, &ifr);
    close(fd);
    if (ok < 0) return -1;
    memcpy(mac, &ifr.ifr_hwaddr.sa_data, 6);
    return 0;
    #endif

    return -1;
}

int rawnet_arch_status(void) {
    return rawnet_pcap_fp ? 1 : 0;
}


#endif /* #ifdef HAVE_RAWNET */
