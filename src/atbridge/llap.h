/*
  GSPLUS - Advanced Apple IIGS Emulator Environment
  Based on the KEGS emulator written by Kent Dickey
  See COPYRIGHT.txt for Copyright information
	See LICENSE.txt for license (GPL v2)
*/

struct packet_t;

/** LLAP port of the AppleTalk Bridge **/

void llap_init();
void llap_shutdown();

/** Send one LLAP packet from the GS
 */
void llap_enqueue_in(double dcycs, size_t size, byte data[]);

/** Receive one LLAP packet from the world to the GS and return the size
 */
void llap_dequeue_out(double dcycs, size_t* size, byte* data[]);


void llap_enqueue_out(struct packet_t* packet);
struct packet_t* llap_dequeue_in();
