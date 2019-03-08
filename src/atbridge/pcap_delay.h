/*
  GSPLUS - Advanced Apple IIGS Emulator Environment
  Based on the KEGS emulator written by Kent Dickey
  See COPYRIGHT.txt for Copyright information
	See LICENSE.txt for license (GPL v2)
*/

/*
This interface provides a thin, delay-loaded wrapper around the PCAP library so that
you may start GSport without intalling PCAP.  Of course, some features that require 
PCAP won't be available.  

This wrapper provides a subset of the available PCAP APIs necessary for ATBridge.
Feel free to extend the wrapper.
*/

#include <pcap.h>

bool pcapdelay_load();
bool pcapdelay_is_loaded();
void pcapdelay_unload();

void pcapdelay_freealldevs(pcap_if_t *);
pcap_t* pcapdelay_open_live(const char *, int, int, int, char *);
void pcapdelay_close(pcap_t *);
int	pcapdelay_findalldevs(pcap_if_t **, char *);
int	pcapdelay_datalink(pcap_t *);
int	pcapdelay_setnonblock(pcap_t *, int, char *);
int pcapdelay_sendpacket(pcap_t *p, u_char *buf, int size);
const u_char* pcapdelay_next(pcap_t *, struct pcap_pkthdr *);
int	pcapdelay_dispatch(pcap_t *, int, pcap_handler, u_char *);
