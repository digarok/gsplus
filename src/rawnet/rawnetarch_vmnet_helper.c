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
#include <errno.h>
#include <spawn.h>
#include <err.h>
#include <signal.h>
#include <mach-o/dyld.h>

#include "rawnetarch.h"
#include "rawnetsupp.h"


enum {
	MSG_QUIT,
	MSG_STATUS,
	MSG_READ,
	MSG_WRITE
};
#define MAKE_MSG(msg, extra) (msg | ((extra) << 8))

static uint8_t interface_mac[6];
static uint8_t interface_fake_mac[6];
static uint32_t interface_mtu;
static uint32_t interface_packet_size;
static uint8_t *interface_buffer = NULL;

static pid_t interface_pid = 0;
static int interface_pipe[2];


static pid_t safe_waitpid(pid_t pid, int *stat_loc, int options) {
	for(;;) {
		pid_t rv = waitpid(pid, stat_loc,options);
		if (rv < 0 && errno == EINTR) continue;
		return rv;
	}
}

static ssize_t safe_read(void *data, size_t nbytes) {
	for (;;) {
		ssize_t rv = read(interface_pipe[0], data, nbytes);
		if (rv < 0 && errno == EINTR) continue;
		return rv;
	}
}

static ssize_t safe_readv(const struct iovec *iov, int iovcnt) {

	for(;;) {
		ssize_t rv = readv(interface_pipe[0], iov, iovcnt);
		if (rv < 0 && errno == EINTR) continue;
		return rv;
	}
}


static ssize_t safe_write(const void *data, size_t nbytes) {
	for (;;) {
		ssize_t rv = write(interface_pipe[1], data, nbytes);
		if (rv < 0 && errno == EINTR) continue;
		return rv;
	}
}

static ssize_t safe_writev(const struct iovec *iov, int iovcnt) {

	for(;;) {
		ssize_t rv = writev(interface_pipe[1], iov, iovcnt);
		if (rv < 0 && errno == EINTR) continue;
		return rv;
	}
}

/* block the sigpipe signal */
static int block_pipe(struct sigaction *oact) {
	struct sigaction act;
	memset(&act, 0, sizeof(act));
	act.sa_handler = SIG_IGN;
	act.sa_flags = SA_RESTART;

	return sigaction(SIGPIPE, &act, oact);
}
static int restore_pipe(const struct sigaction *oact) {
	return sigaction(SIGPIPE, oact, NULL);
}

#if 0
static int block_pipe(sigset_t *oldset) {
	sigset_t set;

	sigemptyset(&set);
	sigaddset(&set, SIGPIPE);
	sigaddset(&set, SIGCHLD);

	return sigprocmask(SIG_BLOCK, &set, oldset);
}
#endif

static int check_child_status(void) {
	pid_t pid;
	int stat;
	pid = safe_waitpid(interface_pid, &stat, WNOHANG);

	if (pid < 0 && errno == ECHILD) {
		fprintf(stderr, "child process does not exist.\n");
		close(interface_pipe[0]);
		close(interface_pipe[1]);
		interface_pid = 0;
		return 0;
	}
	if (pid == interface_pid) {
		if (WIFEXITED(stat)) fprintf(stderr, "child process exited.\n");
		if (WIFSIGNALED(stat)) fprintf(stderr, "child process signalled.\n");

		close(interface_pipe[0]);
		close(interface_pipe[1]);
		interface_pid = 0;
		return 0;
	}
	return 1;
}

static char *get_relative_path(const char *leaf) {

	uint32_t size = 0;
	char *buffer = 0;
	int ok;
	char *cp;
	int l;

	l = strlen(leaf);
	ok = _NSGetExecutablePath(NULL, &size);
	size += l + 1;
	buffer = malloc(size);
	if (buffer) {
		ok = _NSGetExecutablePath(buffer, &size);
		if (ok < 0) {
			free(buffer);
			return NULL;
		}
		cp = strrchr(buffer, '/');
		if (cp)
			strcpy(cp + 1 , leaf);
		else {
			free(buffer);
			buffer = NULL;
		}
	}
	return buffer;	
}


int rawnet_arch_init(void) {
	//interface = NULL;
	return 1;
}
void rawnet_arch_pre_reset(void) {
	/* NOP */
}

void rawnet_arch_post_reset(void) {
	/* NOP */
}




int rawnet_arch_activate(const char *interface_name) {


	int ok;
	posix_spawn_file_actions_t actions;
	posix_spawnattr_t attr;
	char *argv[] = { "vmnet_helper", NULL };
	char *path = NULL;

	int pipe_stdin[2];
	int pipe_stdout[2];

	struct sigaction oldaction;


	if (interface_pid > 0) return 1;

	/* fd[0] = read, fd[1] = write */
	ok = pipe(pipe_stdin);
	if (ok < 0) { warn("pipe"); return 0; }

	ok = pipe(pipe_stdout);
	if (ok < 0) { warn("pipe"); return 0; }

	block_pipe(&oldaction);

	posix_spawn_file_actions_init(&actions);
	posix_spawnattr_init(&attr);


	posix_spawn_file_actions_adddup2(&actions, pipe_stdin[0], STDIN_FILENO);
	posix_spawn_file_actions_adddup2(&actions, pipe_stdout[1], STDOUT_FILENO);

	posix_spawn_file_actions_addclose(&actions, pipe_stdin[0]);
	posix_spawn_file_actions_addclose(&actions, pipe_stdin[1]);

	posix_spawn_file_actions_addclose(&actions, pipe_stdout[0]);
	posix_spawn_file_actions_addclose(&actions, pipe_stdout[1]);


	path = get_relative_path("vmnet_helper");
	ok = posix_spawn(&interface_pid, path, &actions, &attr, argv, NULL);
	free(path);
	posix_spawn_file_actions_destroy(&actions);
	posix_spawnattr_destroy(&attr);

	close(pipe_stdin[0]);
	close(pipe_stdout[1]);
	/* posix spawn returns 0 on success, error code on failure. */

	if (ok != 0) {
		fprintf(stderr, "posix_spawn vmnet_helper failed: %d\n", ok);
		close(pipe_stdin[1]);
		close(pipe_stdout[0]);
		interface_pid = -1;
		return 0;
	}

	interface_pipe[0] = pipe_stdout[0];
	interface_pipe[1] = pipe_stdin[1];

	/* now get the mac/mtu/etc */

	uint32_t msg = MAKE_MSG(MSG_STATUS, 0);
	ok = safe_write(&msg, 4);
	if (ok != 4) goto fail;

	ok = safe_read(&msg, 4);
	if (ok != 4) goto fail;

	if (msg != MAKE_MSG(MSG_STATUS, 6 + 4 + 4)) goto fail;

	struct iovec iov[3];
	iov[0].iov_len = 6;
	iov[0].iov_base = interface_mac;
	iov[1].iov_len = 4;
	iov[1].iov_base = &interface_mtu;
	iov[2].iov_len = 4;
	iov[2].iov_base = &interface_packet_size;

	ok = safe_readv(iov, 3);
	if (ok != 6 + 4 + 4) goto fail;

	/* copy mac to fake mac */
	memcpy(interface_fake_mac, interface_mac, 6);

	/* sanity check */
	/* expect MTU = 1500, packet_size = 1518 */
	if (interface_packet_size < 256) {
		interface_packet_size = 1518;
	}
	interface_buffer = malloc(interface_packet_size);
	if (!interface_buffer) goto fail;

	restore_pipe(&oldaction);
	return 1;

fail:
	close(interface_pipe[0]);
	close(interface_pipe[1]);
	safe_waitpid(interface_pid, NULL, 0);
	interface_pid = 0;

	restore_pipe(&oldaction);
	return 0;
}

void rawnet_arch_deactivate(void) {

	if (interface_pid) {
		close(interface_pipe[0]);
		close(interface_pipe[1]);
		for(;;) {
			int ok = waitpid(interface_pid, NULL, 0);
			if (ok < 0 && errno == EINTR) continue;
			break;
		}
		interface_pid = 0;
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



int rawnet_arch_read(void *buffer, int nbyte) {


	uint32_t msg;
	int ok;
	int xfer;
	struct sigaction oldaction;

	if (interface_pid <= 0) return -1;

	block_pipe(&oldaction);

	msg = MAKE_MSG(MSG_READ, 0);

	ok = safe_write(&msg, 4);
	if (ok != 4) goto fail;

	ok = safe_read(&msg, 4);
	if (ok != 4) goto fail;

	if ((msg & 0xff) != MSG_READ) goto fail;

	xfer = msg >> 8;
	if (xfer > interface_packet_size) {

		fprintf(stderr, "packet size too big: %d\n", xfer);

		/* drain the message ... */
		while (xfer) {
			int count = interface_packet_size;
			if (count > xfer) count = xfer;
			ok = safe_read(interface_buffer, count);
			if (ok < 0) goto fail;
			xfer -= ok;
		}
		return -1;
	}

	if (xfer == 0) return -1;

	ok = safe_read(interface_buffer, xfer);
	if (ok != xfer) goto fail;


	rawnet_fix_incoming_packet(interface_buffer, xfer, interface_mac, interface_fake_mac);

	/* don't sebug multicast packets */
	if (interface_buffer[0] == 0xff || (interface_buffer[0] & 0x01) == 0 ) {
		fprintf(stderr, "\nrawnet_arch_receive: %u\n", (unsigned)xfer);
		rawnet_hexdump(interface_buffer, xfer);
	}

	if (xfer > nbyte) xfer = nbyte;
	memcpy(buffer, interface_buffer, xfer);

	restore_pipe(&oldaction);
	return xfer;

fail:
	/* check if process still ok? */
	check_child_status();
	restore_pipe(&oldaction);
	return -1;
}

int rawnet_arch_write(const void *buffer, int nbyte) {

	int ok;
	uint32_t msg;
	struct iovec iov[2];
	struct sigaction oldaction;

	if (interface_pid <= 0) return -1;



	if (nbyte <= 0) return 0;

	if (nbyte > interface_packet_size) {
		log_message(rawnet_arch_log, "packet is too big: %d", nbyte);
		return -1;
	}


	/* copy the buffer and fix the source mac address. */
	memcpy(interface_buffer, buffer, nbyte);
	rawnet_fix_outgoing_packet(interface_buffer, nbyte, interface_mac, interface_fake_mac);


	fprintf(stderr, "\nrawnet_arch_transmit: %u\n", (unsigned)nbyte);
	rawnet_hexdump(interface_buffer, nbyte);


	block_pipe(&oldaction);

	msg = MAKE_MSG(MSG_WRITE, nbyte);

	iov[0].iov_base = &msg;
	iov[0].iov_len = 4;
	iov[1].iov_base = interface_buffer;
	iov[1].iov_len = nbyte;


	ok = safe_writev(iov, 2);
	if (ok != 4 + nbyte) goto fail;

	ok = safe_read(&msg, 4);
	if (ok != 4) goto fail;

	if (msg != MAKE_MSG(MSG_WRITE, nbyte)) goto fail;

	restore_pipe(&oldaction);
	return nbyte;

fail:
	check_child_status();
	restore_pipe(&oldaction);
	return -1;
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
	return interface_pid > 0 ? interface_mtu : -1;
}

int rawnet_arch_get_mac(uint8_t mac[6]) {
	if (interface_pid > 0) {
		memcpy(mac, interface_mac, 6);
		return 1;
	}
	return -1;
}


int rawnet_arch_status(void) {
	return interface_pid > 0 ? 1 : 0;
}

