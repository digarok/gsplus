/*
 * This file is a consolidation of functions required for tfe
 * emulation taken from the following files
 *
 * lib.c   - Library functions.
 * util.c  - Miscellaneous utility functions.
 * crc32.c
 *
 * Written by
 *  Andreas Boose <viceteam@t-online.de>
 *  Ettore Perazzoli <ettore@comm2000.it>
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


#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "rawnetsupp.h"



#define CRC32_POLY  0xedb88320
static unsigned long crc32_table[256];
static int crc32_is_initialized = 0;

void lib_free(void *ptr) {
  free(ptr);
}

void *lib_malloc(size_t size) {

  void *ptr = malloc(size);

  if (ptr == NULL && size > 0) {
    perror("lib_malloc");
    exit(-1);
  }

  return ptr;
}

/*-----------------------------------------------------------------------*/

/* Malloc enough space for `str', copy `str' into it and return its
   address.  */
char *lib_stralloc(const char *str) {
  char *ptr;

  if (str == NULL) {
    fprintf(stderr, "lib_stralloc\n");
    exit(-1);
  }

  ptr = strdup(str);
  if (!ptr) {
    perror("lib_stralloc");
    exit(-1);
  }

  return ptr;
}



/* Like realloc, but abort if not enough memory is available.  */
void *lib_realloc(void *ptr, size_t size) {


  void *new_ptr = realloc(ptr, size);

  if (new_ptr == NULL) {
    perror("lib_realloc");
    exit(-1);
  }

  return new_ptr;
}

// Util Stuff

/* Set a new value to the dynamically allocated string *str.
   Returns `-1' if nothing has to be done.  */
int util_string_set(char **str, const char *new_value) {
  if (*str == NULL) {
    if (new_value != NULL)
      *str = lib_stralloc(new_value);
  } else {
    if (new_value == NULL) {
      lib_free(*str);
      *str = NULL;
    } else {
      /* Skip copy if src and dest are already the same.  */
      if (strcmp(*str, new_value) == 0)
        return -1;

      *str = (char *)lib_realloc(*str, strlen(new_value) + 1);
      strcpy(*str, new_value);
    }
  }
  return 0;
}


// crc32 Stuff

unsigned long crc32_buf(const char *buffer, unsigned int len) {
  int i, j;
  unsigned long crc, c;
  const char *p;

  if (!crc32_is_initialized) {
    for (i = 0; i < 256; i++) {
      c = (unsigned long) i;
      for (j = 0; j < 8; j++)
        c = c & 1 ? CRC32_POLY ^ (c >> 1) : c >> 1;
      crc32_table[i] = c;
    }
    crc32_is_initialized = 1;
  }

  crc = 0xffffffff;
  for (p = buffer; len > 0; ++p, --len)
    crc = (crc >> 8) ^ crc32_table[(crc ^ *p) & 0xff];

  return ~crc;
}

void rawnet_hexdump(const void *what, int count) {
  static const char hex[] = "0123456789abcdef";
  char buffer1[16 * 3 + 1];
  char buffer2[16 + 1];
  unsigned offset;
  unsigned char *cp = (unsigned char *)what;


  offset = 0;
  while (count > 0) {
    unsigned char x = *cp++;

    buffer1[offset * 3] = hex[x >> 4];
    buffer1[offset * 3 + 1] = hex[x & 0x0f];
    buffer1[offset * 3 + 2] = ' ';

    buffer2[offset] = (x < 0x80) && isprint(x) ? x : '.';


    --count;
    ++offset;
    if (offset == 16 || count == 0) {
      buffer1[offset * 3] = 0;
      buffer2[offset] = 0;
      fprintf(stderr, "%-50s %s\n", buffer1, buffer2);
      offset = 0;
    }
  }
}





enum {
  eth_dest  = 0,  // destination address
  eth_src   = 6,  // source address
  eth_type  = 12, // packet type
  eth_data  = 14, // packet data
};

enum {
  ip_ver_ihl  = 0,
  ip_tos    = 1,
  ip_len    = 2,
  ip_id   = 4,
  ip_frag   = 6,
  ip_ttl    = 8,
  ip_proto    = 9,
  ip_header_cksum = 10,
  ip_src    = 12,
  ip_dest   = 16,
  ip_data   = 20,
};

enum {
  udp_source = 0, // source port
  udp_dest = 2, // destination port
  udp_len = 4, // length
  udp_cksum = 6, // checksum
  udp_data = 8, // total length udp header
};

enum {
  bootp_op = 0, // operation
  bootp_hw = 1, // hardware type
  bootp_hlen = 2, // hardware len
  bootp_hp = 3, // hops
  bootp_transid = 4, // transaction id
  bootp_secs = 8, // seconds since start
  bootp_flags = 10, // flags
  bootp_ipaddr = 12, // ip address knwon by client
  bootp_ipclient = 16, // client ip from server
  bootp_ipserver = 20, // server ip
  bootp_ipgateway = 24, // gateway ip
  bootp_client_hrd = 28, // client mac address
  bootp_spare = 34,
  bootp_host = 44,
  bootp_fname = 108,
  bootp_data = 236, // total length bootp packet
};

enum {
  arp_hw = 14,    // hw type (eth = 0001)
  arp_proto = 16,   // protocol (ip = 0800)
  arp_hwlen = 18,   // hw addr len (eth = 06)
  arp_protolen = 19,  // proto addr len (ip = 04)
  arp_op = 20,    // request = 0001, reply = 0002
  arp_shw = 22,   // sender hw addr
  arp_sp = 28,    // sender proto addr
  arp_thw = 32,   // target hw addr
  arp_tp = 38,    // target protoaddr
  arp_data = 42,  // total length of packet
};

enum {
  dhcp_discover = 1,
  dhcp_offer = 2,
  dhcp_request = 3,
  dhcp_decline = 4,
  dhcp_pack = 5,
  dhcp_nack = 6,
  dhcp_release = 7,
  dhcp_inform = 8,
};

static uint8_t oo[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static uint8_t ff[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

static int is_arp(const uint8_t *packet, unsigned size) {
  return size == arp_data
    && packet[12] == 0x08 && packet[13] == 0x06 /* ARP */
    && packet[14] == 0x00 && packet[15] == 0x01 /* ethernet */
    && packet[16] == 0x08 && packet[17] == 0x00 /* ipv4 */
    && packet[18] == 0x06 /* hardware size */
    && packet[19] == 0x04 /* protocol size */
  ;
}

static int is_broadcast(const uint8_t *packet, unsigned size) {
  return !memcmp(packet + 0, ff, 6);
}

static int is_unicast(const uint8_t *packet, unsigned size) {
  return (*packet & 0x01) == 0;
}

static int is_multicast(const uint8_t *packet, unsigned size) {
  return (*packet & 0x01) == 0x01 && !is_broadcast(packet, size);
}

static int is_dhcp_out(const uint8_t *packet, unsigned size) {
  static uint8_t cookie[] = { 0x63, 0x82, 0x53, 0x63 };
  return size >= 282
    //&& !memcmp(&packet[0], ff, 6) /* broadcast */
    && packet[12] == 0x08 && packet[13] == 0x00
    && packet[14] == 0x45 /* version 4 */
    && packet[23] == 0x11 /* UDP */
    //&& !memcmp(&packet[26], oo, 4)  /* source ip */
    //&& !memcmp(&packet[30], ff, 4)  /* dest ip */
    && packet[34] == 0x00 && packet[35] == 0x44 /* source port */
    && packet[36] == 0x00 && packet[37] == 0x43 /* dest port */
    //&& packet[44] == 0x01 /* dhcp boot req */
    && packet[43] == 0x01 /* ethernet */
    && packet[44] == 0x06 /* 6 byte mac */
    && !memcmp(&packet[278], cookie, 4)
  ;
}


static int is_dhcp_in(const uint8_t *packet, unsigned size) {
  static uint8_t cookie[] = { 0x63, 0x82, 0x53, 0x63 };
  return size >= 282
    //&& !memcmp(&packet[0], ff, 6) /* broadcast */
    && packet[12] == 0x08 && packet[13] == 0x00
    && packet[14] == 0x45 /* version 4 */
    && packet[23] == 0x11 /* UDP */
    //&& !memcmp(&packet[26], oo, 4)  /* source ip */
    //&& !memcmp(&packet[30], ff, 4)  /* dest ip */
    && packet[34] == 0x00 && packet[35] == 0x43 /* source port */
    && packet[36] == 0x00 && packet[37] == 0x44 /* dest port */
    //&& packet[44] == 0x01 /* dhcp boot req */
    && packet[43] == 0x01 /* ethernet */
    && packet[44] == 0x06 /* 6 byte mac */
    && !memcmp(&packet[278], cookie, 4)
  ;
}

static unsigned ip_checksum(const uint8_t *packet) {
  unsigned x = 0;
  unsigned i;
  for (i = 0; i < ip_data; i += 2) {
    if (i == ip_header_cksum) continue;
    x += packet[eth_data + i + 0 ] << 8;
    x += packet[eth_data + i + 1];
  }

  /* add the carry */
  x += x >> 16;
  x &= 0xffff;
  return ~x & 0xffff;
}

void rawnet_fix_incoming_packet(uint8_t *packet, unsigned size, const uint8_t real_mac[6], const uint8_t fake_mac[6]) {

  if (memcmp(packet + 0, real_mac, 6) == 0)
    memcpy(packet + 0, fake_mac, 6);

  /* dhcp request - fix the hardware address */
  if (is_unicast(packet, size) && is_dhcp_in(packet, size)) {
    if (!memcmp(packet + 70, real_mac, 6))
      memcpy(packet + 70, fake_mac, 6);
    return;
  }

}

void rawnet_fix_outgoing_packet(uint8_t *packet, unsigned size, const uint8_t real_mac[6], const uint8_t fake_mac[6]) {



  if (memcmp(packet + 6, fake_mac, 6) == 0)
    memcpy(packet + 6, real_mac, 6);

  if (is_arp(packet, size)) {
    /* sender mac address */
    if (!memcmp(packet + 22, fake_mac, 6))
      memcpy(packet + 22, real_mac, 6);
    return;
  }

  /* dhcp request - fix the hardware address */
  if (is_broadcast(packet, size) && is_dhcp_out(packet, size)) {

    if (!memcmp(packet + 70, fake_mac, 6))
      memcpy(packet + 70, real_mac, 6);
    return;
  }

}
