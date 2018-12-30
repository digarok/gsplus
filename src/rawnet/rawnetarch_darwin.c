/*
 * OS X 10.10+
 * vmnet support (clang -framework vmnet -framework Foundation)
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <vmnet/vmnet.h>

#include "rawnetarch.h"
#include "rawnetsupp.h"


static const uint8_t broadcast_mac[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

static interface_ref interface;
static uint8_t interface_mac[6];
static uint8_t interface_fake_mac[6];
static uint64_t interface_mtu;
static uint64_t interface_packet_size;
static uint8_t *interface_buffer = NULL;
static vmnet_return_t interface_status;

int rawnet_arch_init(void) {
	interface = NULL;
	return 1;
}
void rawnet_arch_pre_reset(void) {
	/* NOP */
}

void rawnet_arch_post_reset(void) {
	/* NOP */
}

int rawnet_arch_activate(const char *interface_name) {

	xpc_object_t dict;
	dispatch_queue_t q;
	dispatch_semaphore_t sem;

	/*
	 * there's no way to set the MAC address directly.
	 * using vmnet_interface_id_key w/ the previous interface id
	 * *MIGHT* re-use the previous MAC address.
	 */

	memset(interface_mac, 0, sizeof(interface_mac));
	memset(interface_fake_mac, 0, sizeof(interface_fake_mac));
	interface_status = 0;
	interface_mtu = 0;
	interface_packet_size = 0;

	dict = xpc_dictionary_create(NULL, NULL, 0);
	xpc_dictionary_set_uint64(dict, vmnet_operation_mode_key, VMNET_SHARED_MODE);
	sem = dispatch_semaphore_create(0);
	q = dispatch_get_global_queue(QOS_CLASS_UTILITY, 0);

	interface = vmnet_start_interface(dict, q, ^(vmnet_return_t status, xpc_object_t params){
		interface_status = status;
		if (status == VMNET_SUCCESS) {
			const char *cp;
			cp = xpc_dictionary_get_string(params, vmnet_mac_address_key);
			fprintf(stderr, "vmnet mac: %s\n", cp);
			sscanf(cp, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
				&interface_mac[0],
				&interface_mac[1],
				&interface_mac[2],
				&interface_mac[3],
				&interface_mac[4],
				&interface_mac[5]
			);

			interface_mtu = xpc_dictionary_get_uint64(params, vmnet_mtu_key);
			interface_packet_size =  xpc_dictionary_get_uint64(params, vmnet_max_packet_size_key);

			fprintf(stderr, "vmnet mtu: %u\n", (unsigned)interface_mtu);

		}
		dispatch_semaphore_signal(sem);
	});
	dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);

	if (interface_status == VMNET_SUCCESS) {
		interface_buffer = (uint8_t *)malloc(interface_packet_size);
	} else {
		log_message(rawnet_arch_log, "vmnet_start_interface failed");
		if (interface) {
			vmnet_stop_interface(interface, q, ^(vmnet_return_t status){
				dispatch_semaphore_signal(sem);
			});
			dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
			interface = NULL;
		}
	}


	dispatch_release(sem);
	xpc_release(dict);
	return interface_status == VMNET_SUCCESS;
}

void rawnet_arch_deactivate(void) {

	dispatch_queue_t q;
	dispatch_semaphore_t sem;


	if (interface) {
		sem = dispatch_semaphore_create(0);
		q = dispatch_get_global_queue(QOS_CLASS_UTILITY, 0);

		vmnet_stop_interface(interface, q, ^(vmnet_return_t status){
			dispatch_semaphore_signal(sem);
		});
		dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
		dispatch_release(sem);

		interface = NULL;
		interface_status = 0;
	}
	free(interface_buffer);
	interface_buffer = NULL;

}

void rawnet_arch_set_mac(const uint8_t mac[6]) {

#ifdef RAWNET_DEBUG_ARCH
    log_message( rawnet_arch_log, "New MAC address set: %02X:%02X:%02X:%02X:%02X:%02X.", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] );
#endif
    memcpy(interface_fake_mac, mac, 6);
}
void rawnet_arch_set_hashfilter(const uint32_t hash_mask[2]) {
	/* NOP */
}

void rawnet_arch_recv_ctl(int bBroadcast, int bIA, int bMulticast, int bCorrect, int bPromiscuous, int bIAHash) {
	/* NOP */
}

void rawnet_arch_line_ctl(int bEnableTransmitter, int bEnableReceiver) {
	/* NOP */
}




enum {
	eth_dest	= 0,	// destination address
	eth_src		= 6,	// source address
	eth_type	= 12,	// packet type
	eth_data	= 14,	// packet data
};

enum {
	ip_ver_ihl	= 0,
	ip_tos		= 1,
	ip_len		= 2,
	ip_id		= 4,
	ip_frag		= 6,
	ip_ttl		= 8,
	ip_proto		= 9,
	ip_header_cksum = 10,
	ip_src		= 12,
	ip_dest		= 16,
	ip_data		= 20,
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
	arp_hw = 14,		// hw type (eth = 0001)
	arp_proto = 16,		// protocol (ip = 0800)
	arp_hwlen = 18,		// hw addr len (eth = 06)
	arp_protolen = 19,	// proto addr len (ip = 04)
	arp_op = 20,		// request = 0001, reply = 0002
	arp_shw = 22,		// sender hw addr
	arp_sp = 28,		// sender proto addr
	arp_thw = 32,		// target hw addr
	arp_tp = 38,		// target protoaddr
	arp_data = 42,	// total length of packet
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

static void fix_incoming_packet(uint8_t *packet, unsigned size, const uint8_t real_mac[6], const uint8_t fake_mac[6]) {

	if (memcmp(packet + 0, real_mac, 6) == 0)
		memcpy(packet + 0, fake_mac, 6);

	/* dhcp request - fix the hardware address */
	if (is_unicast(packet, size) && is_dhcp_in(packet, size)) {
		if (!memcmp(packet + 70, real_mac, 6))
			memcpy(packet + 70, fake_mac, 6);
		return;
	}

}

static void fix_outgoing_packet(uint8_t *packet, unsigned size, const uint8_t real_mac[6], const uint8_t fake_mac[6]) {



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
		/* set source ip to 0 - LL bug? */
		/* fixed in link layer as of 2018-12-17 */
#if 0
		unsigned ck;
		memset(packet + 26, 0, 4);
		ck = ip_checksum(packet);
		packet[eth_data + ip_header_cksum + 0] = ck >> 8;
		packet[eth_data + ip_header_cksum + 1] = ck;
#endif

		if (!memcmp(packet + 70, fake_mac, 6))
			memcpy(packet + 70, real_mac, 6);
		return;
	}

}

int rawnet_arch_read(void *buffer, int nbyte) {

	int count = 1;
	int xfer;
	vmnet_return_t st;
	struct vmpktdesc v;
	struct iovec iov;

	iov.iov_base = interface_buffer;
	iov.iov_len = interface_packet_size;

	v.vm_pkt_size = interface_packet_size;
	v.vm_pkt_iov = &iov;
	v.vm_pkt_iovcnt = 1;
	v.vm_flags = 0;

	st = vmnet_read(interface, &v, &count);
	if (st != VMNET_SUCCESS) {
		log_message(rawnet_arch_log, "vmnet_read failed!");
		return -1;
	}

	if (count < 1) {
		return 0;
	}

	fix_incoming_packet(interface_buffer, v.vm_pkt_size, interface_mac, interface_fake_mac);

	// iov.iov_len is not updated with the read count, apparently. 
	if (!is_multicast(interface_buffer, v.vm_pkt_size)) { /* multicast crap */
		fprintf(stderr, "\nrawnet_arch_receive: %u\n", (unsigned)v.vm_pkt_size);
		rawnet_hexdump(interface_buffer, v.vm_pkt_size);
	}

	xfer = v.vm_pkt_size;
	memcpy(buffer, interface_buffer, xfer);

	return xfer;
}

int rawnet_arch_write(const void *buffer, int nbyte) {

	int count = 1;
	vmnet_return_t st;
	struct vmpktdesc v;
	struct iovec iov;

	if (nbyte <= 0) return 0;

	if (nbyte > interface_packet_size) {
		log_message(rawnet_arch_log, "packet is too big: %d", nbyte);
		return -1;
	}

	/* copy the buffer and fix the source mac address. */
	memcpy(interface_buffer, buffer, nbyte);
	fix_outgoing_packet(interface_buffer, nbyte, interface_mac, interface_fake_mac);

	iov.iov_base = interface_buffer;
	iov.iov_len = nbyte;

	v.vm_pkt_size = nbyte;
	v.vm_pkt_iov = &iov;
	v.vm_pkt_iovcnt = 1;
	v.vm_flags = 0;


	fprintf(stderr, "\nrawnet_arch_transmit: %u\n", (unsigned)iov.iov_len);
	rawnet_hexdump(interface_buffer, v.vm_pkt_size);

	
	st = vmnet_write(interface, &v, &count);

	if (st != VMNET_SUCCESS) {
		log_message(rawnet_arch_log, "vmnet_write failed!");
		return -1;
	}
	return nbyte;
}




static unsigned adapter_index = 0;
int rawnet_arch_enumadapter_open(void) {
	adapter_index = 0;
	return 1;
}

int rawnet_arch_enumadapter(char **ppname, char **ppdescription) {

	if (adapter_index == 0) {
		++adapter_index;
		if (ppname) *ppname = lib_stralloc("vmnet");
		if (ppdescription) *ppdescription = lib_stralloc("vmnet");
		return 1;
	}
	return 0;
}

int rawnet_arch_enumadapter_close(void) {
	return 1;
}

char *rawnet_arch_get_standard_interface(void) {
	return lib_stralloc("vmnet");
}

int rawnet_arch_get_mtu(void) {
	return interface ? interface_mtu : -1;
}

int rawnet_arch_get_mac(uint8_t mac[6]) {
	if (interface) {
		memcpy(mac, interface_mac, 6);
		return 1;
	}
	return -1;
}


int rawnet_arch_status(void) {
	return interface ? 1 : 0;
}

