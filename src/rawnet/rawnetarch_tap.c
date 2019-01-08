/* tun/tap support */
/* for Linux, *BSD */

/*
 * tap is a virtual ethernet devices.
 * open the device, configure, and read/write ethernet frames.
 *
 * Linux setup: (from Network Programmability and Automation, Appendix A)
 *
 * Notes:
 * - this assumes eth0 is your main interface device 
 * - may need to install the iproute/iproute2 package (ip command)
 * - do this stuff as root.
 *
 * 1. create a tap interface
 * $ ip tuntap add tap65816 mode tap user YOUR_USER_NAME
 * $ ip link set tap65816 up
 *
 * 2. create a network bridge
 * $ ip link add name br0 type bridge
 * $ ip link set br0 up
 *
 * 3. bridge the physical network and virtual network.
 * $ ip link set eth0 master br0
 * $ ip link set tap65816 master br0
 *
 * 4. remove ip address from physical device (This will kill networking)
 * ip address flush dev eth0
 *
 * 5. and add the ip address to the bridge 
 * dhclient br0 # if using dhcp
 * ip address add 192.168.1.1/24 dev eth0 # if using static ip address.
 *
 * *BSD:
 * - assumes eth0 is your main interface device.
 * $ ifconfig bridge0 create
 * $ ifconfig tap65816 create
 * $ ifconfig bridge0 addm eth0 addm tap65816 up
 * 
 * allow normal users to open tap devices?
 * $ sysctl net.link.tap.user_open=1
 * $ sysctl net.link.tap.up_on_open=1
 *
 * set permissions
 * $ chown YOUR_USER_NAME /dev/tap65816
 * $ chmod 660 /dev/tap65816
 */



#define _GNU_SOURCE

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <net/if.h>

#include <dirent.h>

#if defined(__linux__)
#include <linux/if.h>
#include <linux/if_tun.h>
#endif

#include "rawnetarch.h"
#include "rawnetsupp.h"

#if defined(__linux__)
#define TAP_DEVICE "/dev/net/tun"
#endif

#if defined(__FreeBSD__)
#define TAP_DEVICE "/dev/tap"
#endif

static int interface_fd = -1;
static char *interface_dev = NULL;

static uint8_t interface_mac[6];
static uint8_t interface_fake_mac[6];

static uint8_t *interface_buffer = NULL;
static int interface_buffer_size = 0;

int rawnet_arch_init(void) {
	interface_fd = -1;
	return 1;
}
void rawnet_arch_pre_reset(void) {
	/* NOP */
}

void rawnet_arch_post_reset(void) {
	/* NOP */
}

/* memoized buffer for ethernet packets, etc */
static int make_buffer(int size) {
	if (size <= interface_buffer_size) return 0;
	if (interface_buffer) free(interface_buffer);
	if (size < 1500) size = 1500; /* good mtu size */
	interface_buffer_size = 0;
	size *= 2;
	interface_buffer = malloc(size);
	if (!interface_buffer) return -1;
	interface_buffer_size = size;
	return 0;
}

#if defined(__linux__)
/* interface name. default is tap65816. */
int rawnet_arch_activate(const char *interface_name) {

	struct ifreq ifr;

	int ok;
	int one = 1;
	int fd;

	if (!interface_name || !*interface_name) {
		interface_name = "tap65816";
	}

	fd = open(TAP_DEVICE, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "rawnet_arch_activate: open(%s): %s\n", TAP_DEVICE, strerror(errno));
		return 0;
	}

	ok = ioctl(fd, FIONBIO, &one);
	if (ok < 0) {
		perror("ioctl(FIONBIO");
		close(fd);
		return 0;
	}

	memset(&ifr, 0, sizeof(ifr));
	strcpy(&if.ifr_name, interface_name);
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
	ok = ioctl(fd, TUNSETIFF, (void *) &ifr);
	if (ok < 0) {
		perror("ioctl(TUNSETIFF)");
		close(fd);
		return 0;
	}

	if (rawnet_arch_get_mac(interface_mac) < 0) {
		perror("rawnet_arch_get_mac");
		close(fd);
		return 0;
	}
	/* copy mac to fake mac */
	memcpy(interface_fake_mac, interface_mac, 6);


	interface_dev = strdup(interface_name);
	interface_fd = fd;
	return 1;
}
#endif

#if defined(__FreeBSD__)
/* man tap(4) */
/* interface name. default is tap65816. */
int rawnet_arch_activate(const char *interface_name) {

	struct ifreq ifr;

	int ok;
	int one = 1;
	int fd;
	char *path[64];

	if (!interface_name || !*interface_name) {
		interface_name = "tap65816";
	}

	ok = snprintf(path, sizeof(path), "/dev/%s", interface_name);
	if (ok >= sizeof(path)) return 0;

	fd = open(path, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "rawnet_arch_activate: open(%s): %s\n", path, strerror(errno));
		return 0;
	}

	ok = ioctl(fd, FIONBIO, &one);
	if (ok < 0) {
		perror("ioctl(FIONBIO");
		close(fd);
		return 0;
	}

	if (rawnet_arch_get_mac(interface_mac) < 0) {
		perror("rawnet_arch_get_mac");
		close(fd);
		return 0;
	}
	/* copy mac to fake mac */
	memcpy(interface_fake_mac, interface_mac, 6);



	interface_dev = strdup(interface_name);
	interface_fd = fd;
	return 1;
}
#endif




void rawnet_arch_deactivate(void) {
	if (interface_fd >= 0)
		close(interface_fd);
	free(interface_dev);

	free(interface_buffer);
	interface_buffer = 0;
	interface_buffer_size = 0;

	interface_dev = NULL;
	interface_fd = -1;
}


void rawnet_arch_set_mac(const uint8_t mac[6]) {

#ifdef RAWNET_DEBUG_ARCH
    log_message( rawnet_arch_log, "New MAC address set: %02X:%02X:%02X:%02X:%02X:%02X.", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] );
#endif

    memcpy(interface_fake_mac, mac, 6);

    /* in theory,  SIOCSIFHWADDR (linux) or SIOCSIFADDR (bsd) will set the mac address. */
    /* in testing, (linux) I get EBUSY or EOPNOTSUPP */


#if 0
    if (interface_fd < 0) return;


    #if defined(__linux__)
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    memcpy(ifr.ifr_hwaddr.sa_data, mac, 6);
    if (ioctl(interface_fd, SIOCSIFHWADDR, (void *)&ifr) < 0)
    	perror("ioctl(SIOCSIFHWADDR)");
    #endif

    #if defined(__FreeBSD__)
    if (ioctl(interface_fd, SIOCSIFADDR, mac) < 0)
    	perror("ioctl(SIOCSIFADDR)");
    #endif
#endif
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

int rawnet_arch_read(void *buffer, int nbyte) {
	int ok;

	if (make_buffer(nbyte) < 0) return -1;

	ok = read(interface_fd, interface_buffer, nbyte);
	if (ok <= 0) return -1;

	rawnet_fix_incoming_packet(interface_buffer, ok, interface_mac, interface_fake_mac);
	memcpy(buffer, interface_buffer, ok);
	return ok;
}

int rawnet_arch_write(const void *buffer, int nbyte) {
	int ok;

	if (make_buffer(nbyte) < 0) return -1;


	memcpy(interface_buffer, buffer, nbyte);
	rawnet_fix_outgoing_packet(interface_buffer, nbyte, interface_mac, interface_fake_mac);

	ok = write(interface_fd, interface_buffer, nbyte);
	return ok;
}


static unsigned adapter_index = 0;
static unsigned adapter_count = 0;
static char **devices = NULL;

static int cmp(const void *a, const void *b) {
	return strcasecmp(*(const char **)a, *(const char **)b);
}

int rawnet_arch_enumadapter_open(void) {
	adapter_index = 0;
	adapter_count = 0;
	devices = NULL;

	char buffer[MAXPATHLEN];

	unsigned capacity = 0;
	unsigned count = 0;
	DIR *dp = NULL;

	int i;


	capacity = 20;
	devices = (char **)malloc(sizeof(char *) * capacity);
	if (!devices) return 0;


#if defined(__linux__)

	dp = opendir("/sys/class/net/");
	if (!dp) goto fail;

	for(;;) {
		char *cp;
		FILE *fp;
		struct dirent *d = readdir(dp);
		if (!d) break;

		if (d->d_name[0] == '.') continue;

		sprintf(buffer, "/sys/class/net/%s/tun_flags", d->d_name);
		fp = fopen(buffer, "r");
		if (!fp) continue;
		cp = fgets(buffer, sizeof(buffer), fp);
		fclose(fp);
		if (cp) {
			/* expect 0x1002 */
			int flags = strtol(cp, (char **)NULL, 16);
			if ((flags & TUN_TYPE_MASK) == IFF_TAP) {

				devices[count++] = strdup(d->d_name);
				if (count == capacity) {
					char **tmp;
					capacity *= 2;
					tmp = (char **)realloc(devices, sizeof(char *) * capacity);
					if (tmp) devices = tmp;
					else break; /* no mem? */
				}

			}
		}

	}
	closedir(dp);
#endif

#if defined(__FreeBSD__)
	/* dev/tapxxxx */
	dp = opendir("/dev/");
	if (!dp) goto fail;
	for(;;) {
		struct dirent *d = readdir(dp);
		if (!d) break;

		if (d->d_name[0] == '.') continue;
		if (strncmp(d->d_name, "tap", 3) == 0 && isdigit(d->d_name[3])) {

			devices[count++] = strdup(d->d_name);
			if (count == capacity) {
				char **tmp;
				capacity *= 2;
				tmp = (char **)realloc(devices, sizeof(char *) * capacity);
				if (tmp) devices = tmp;
				else break; /* no mem? */
			}
		}

	}
	closedir(dp);
#endif

	/* sort them ... */
	qsort(devices, count, sizeof(char *), cmp);

	adapter_count = count;
	return 1;

fail:
	if (dp) closedir(dp);
	if (devices) for(i = 0; i <count; ++i) free(devices[i]);
	free(devices);
	return 0;
}

int rawnet_arch_enumadapter_close(void) {

	if (devices) {
		int i;
		for (i = 0; i < adapter_count; ++i)
			free(devices[i]);
		free(devices);
	}
	adapter_count = 0;
	adapter_index = 0;
	devices = NULL;
	return 1;
}


int rawnet_arch_enumadapter(char **ppname, char **ppdescription) {

	if (adapter_index >= adapter_count) return 0;

	if (ppdescription) *ppdescription = NULL;
	if (ppname) *ppname = strdup(devices[adapter_index]);
	++adapter_index;
	return 1;
}


char *rawnet_arch_get_standard_interface(void) {
	return lib_stralloc("tap65816");
}

int rawnet_arch_get_mtu(void) {
	if (interface_fd < 0) return -1;

	#if defined(__linux__)
	/* n.b. linux tap driver doesn't actually support SIOCGIFMTU */
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	if (ioctl(interface_fd, SIOCGIFMTU, (void *) &ifr) < 0) return -1; /* ? */
	return ifr.ifr_mtu;
	#endif

	#if defined(__FreeBSD__)
	struct tapinfo ti;
	if (ioctl(interface_fd, TAPSIFINFO, &ti) < 0) return -1;
	return ti.mtu;
	#endif

	return -1;
}

int rawnet_arch_get_mac(uint8_t mac[6]) {


	if (interface_fd < 0) return -1;

    #if defined(__linux__)
    struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
    if (ioctl(interface_fd, SIOCGIFHWADDR, (void *)&ifr) < 0) return -1;
    memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
    return 0;
    #endif

    #if defined(__FreeBSD__)
    if (ioctl(interface_fd, SIOCSIFADDR, mac) < 0) return -1;
    return 0;
    #endif

	return -1;

}

int rawnet_arch_status(void) {
	return interface_fd >= 0 ? 1 : 0;
}

