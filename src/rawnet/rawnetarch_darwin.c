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
static uint64_t interface_mtu;
static uint64_t interface_packet_size;
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


	if (interface_status != VMNET_SUCCESS) {
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

}

void rawnet_arch_set_mac(const uint8_t mac[6]) {
	/* NOP */
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

void rawnet_arch_transmit(int force, int onecoll, int inhibit_crc, int tx_pad_dis, int txlength, uint8_t *txframe) {

	int count = 1;
	vmnet_return_t st;
	struct vmpktdesc v;
	struct iovec iov;

	if (txlength == 0) return;
	if (txlength > interface_packet_size) {
		log_message(rawnet_arch_log, "packet is too big: %d", txlength);
		return;
	}


	iov.iov_base = txframe;
	iov.iov_len = txlength;

	v.vm_pkt_size = txlength;
	v.vm_pkt_iov = &iov;
	v.vm_pkt_iovcnt = 1;
	v.vm_flags = 0;


	fprintf(stderr, "\rawnet_arch_transmit: %u\n", (unsigned)iov.iov_len);
	rawnet_hexdump(iov.iov_base, iov.iov_len);

	
	st = vmnet_write(interface, &v, &count);
	if (st != VMNET_SUCCESS) {
		log_message(rawnet_arch_log, "vmnet_write failed!");
	}
	return;
}

int rawnet_arch_receive(uint8_t *pbuffer, int *plen, int *phashed, int *phash_index, int *prx_ok, int *pcorrect_mac, int *pbroadcast, int *pcrc_error) {

	char *buffer;
	//unsigned hashreg;

	int count = 1;
	int xfer;
	vmnet_return_t st;
	struct vmpktdesc v;
	struct iovec iov;


	buffer = calloc(interface_packet_size, 1);

	iov.iov_base = buffer;
	iov.iov_len = interface_packet_size;

	v.vm_pkt_size = interface_packet_size;
	v.vm_pkt_iov = &iov;
	v.vm_pkt_iovcnt = 1;
	v.vm_flags = 0;

	st = vmnet_read(interface, &v, &count);
	if (st != VMNET_SUCCESS) {
		log_message(rawnet_arch_log, "vmnet_write failed!");
		free(buffer);
		return 0;
	}

	if (count < 1) {
		free(buffer);
		return 0;
	}

	// iov.iov_len is not updated with the read count, apparently. 
	fprintf(stderr, "\nrawnet_arch_receive: %u\n", (unsigned)v.vm_pkt_size);
	rawnet_hexdump(iov.iov_base, v.vm_pkt_size);

	xfer = v.vm_pkt_size;
	if (xfer > *plen) xfer = *plen;
	memcpy(pbuffer, buffer, xfer);

	xfer = v.vm_pkt_size;
	if (xfer & 0x01) ++xfer; /* ??? */
	*plen = xfer; /* actual frame size */

	*phashed =
	*phash_index =
	*pbroadcast = 
	*pcorrect_mac =
	*pcrc_error = 0;

	*prx_ok = 1;
	*pcorrect_mac = memcmp(buffer, interface_mac, 6) == 0;
	*pbroadcast = memcmp(buffer, broadcast_mac, 6) == 0;
	
	/* vmnet won't send promiscuous packets */
	#if 0
	hashreg = (~crc32_buf(buffer, 6) >> 26) & 0x3f;
	if (hash_mask[hashreg >= 32] & (1 << (hashreg & 0x1F))) {
		*phashed = 1;
		*phash_index = hashreg;
	} else {
		*phashed = 0;
		*phash_index = 0;
	}
	#endif

	free(buffer);
	return 1;
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
