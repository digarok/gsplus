/*
  GSPLUS - Advanced Apple IIGS Emulator Environment
  Based on the KEGS emulator written by Kent Dickey
  See COPYRIGHT.txt for Copyright information
	See LICENSE.txt for license (GPL v2)
*/

/** ELAP port of the AppleTalk Bridge **/

bool elap_init();
void elap_shutdown();
void elap_process();

struct packet_t;

void elap_enqueue_out(struct packet_t* packet);
struct packet_t* elap_dequeue_in();

struct ether_addr_t;
struct snap_discriminator_t;

void elap_send(const struct ether_addr_t* dest, const struct snap_discriminator_t* discriminator, size_t size, byte data[]);

const struct ether_addr_t* elap_get_mac();
