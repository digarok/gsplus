/*
  GSPLUS - Advanced Apple IIGS Emulator Environment
  Based on the KEGS emulator written by Kent Dickey
  See COPYRIGHT.txt for Copyright information
	See LICENSE.txt for license (GPL v2)
*/

/**
ATBridge is a limited function AppleTalk bridge that allows GSPort to connect to an
EtherTalk network.  The bridge has two ports, one the emulated LocalTalk port and the
other an EtherTalk Phase II port.  

Due to timing requirements of LocalTalk, it is not reasonable to transparently bridge
LLAP traffic to ELAP.  For example, implementing the lapENQ/lapACK LLAP control packets
requires AARP queries on the ELAP port, which can't be reasonably done within the 200us
LLAP interframe gap.  So, we must implement the local bridge functionality detailed
in "Inside AppleTalk", including AARP, RTMP, ZIP, and DDP routing.
**/
#include "atalk.h"

bool atbridge_init();
void atbridge_shutdown();
void atbridge_process();

void atbridge_set_diagnostics(bool enabled);
bool atbridge_get_diagnostics();
void atbridge_printf(const char *fmt, ...);

const struct at_addr_t* atbridge_get_addr();
const at_network_t atbridge_get_net();
const at_node_t atbridge_get_node();
void atbridge_set_net(at_network_t net);
void atbridge_set_node(at_node_t node);
bool atbridge_address_used(const struct at_addr_t* addr);
