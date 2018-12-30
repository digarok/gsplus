/* tun/tap support */
/* for Linux, *BSD */

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
const char *default_interface_name = "/dev/net/tun";
#endif

#if defined(__FreeBSD__)
const char *default_interface_name = "/dev/tap";
#endif

static int interface_fd = -1;


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


/* interface name may be a full path (/dev/tap0) or relative (tap0) */
int rawnet_arch_activate(const char *interface_name) {

	struct ifreq ifr;

	int ok;
	char *tmp = NULL;
	int one = 1;
	int fd;

	if (!interface_name || !*interface_name) {
		interface_name = default_interface_name;
	}
	if (interface_name[0] != '/') {
		ok = asprintf(&tmp, "/dev/%s", interface_name);
		if (ok < 0) {
			perror("rawnet_arch_activate: asprintf");
			return 0;
		}
		interface_name = tmp;
	}

	fd = open(interface_name, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "rawnet_arch_activate: open(%s): %s\n", interface_name, strerror(errno));
		free(tmp);
		return 0;
	}
	free(tmp);

	ok = ioctl(fd, FIONBIO, &one);
	if (ok < 0) {
		perror("ioctl(FIONBIO");
		close(fd);
		return 0;
	}

	#if defined(__linux__)
	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
	ok = ioctl(fd, TUNSETIFF, (void *) &ifr);
	if (ok < 0) {
		perror("ioctl(TUNSETIFF)");
		close(fd);
		return 0;
	}
	#endif

	interface_fd = fd;
	return 1;
}

void rawnet_arch_deactivate(void) {
	if (interface_fd >= 0)
		close(interface_fd);
	interface_fd = -1;
}


void rawnet_arch_set_mac(const uint8_t mac[6]) {

#ifdef RAWNET_DEBUG_ARCH
    log_message( rawnet_arch_log, "New MAC address set: %02X:%02X:%02X:%02X:%02X:%02X.", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] );
#endif
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
	return read(interface_fd, buffer, nbyte);
}

int rawnet_arch_write(const void *buffer, int nbyte) {
	return write(interface_fd, buffer, nbyte);
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
			if (flags == IFF_TAP | IFF_NO_PI) {

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
	return lib_stralloc(default_interface_name);
}

int rawnet_arch_get_mtu(void) {
	if (interface_fd < 0) return -1;

	#if defined(__linux__)

	struct ifreq ifr;
	if (ioctl(interface_fd, TUNGETIFF, (void *) &ifr) < 0) return -1; /* ? */
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

