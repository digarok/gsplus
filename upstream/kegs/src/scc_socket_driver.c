const char rcsid_scc_socket_driver_c[] = "@(#)$KmKId: scc_socket_driver.c,v 1.37 2025-04-29 22:17:38+00 kentd Exp $";

/************************************************************************/
/*			KEGS: Apple //gs Emulator			*/
/*			Copyright 2002-2025 by Kent Dickey		*/
/*									*/
/*	This code is covered by the GNU GPL v3				*/
/*	See the file COPYING.txt or https://www.gnu.org/licenses/	*/
/*	This program is provided with no warranty			*/
/*									*/
/*	The KEGS web page is kegs.sourceforge.net			*/
/*	You may contact the author at: kadickey@alumni.princeton.edu	*/
/************************************************************************/

// This file contains the socket calls for Win32 and Mac/Linux
// Win32: see: https://learn.microsoft.com/en-us/windows/win32/api/winsock2/

// Modem init string GBBS GSPORT.HST: ats0=1s2=128v0
// Modem init string Warp6: atm0e0v0s0=0s7=25   atx4h0  at&c1&d2

#include "defc.h"

#include "scc.h"
#include <signal.h>

extern int Verbose;
extern Scc g_scc[2];
extern int g_serial_cfg[2];
extern char *g_serial_remote_ip[2];
extern int g_serial_remote_port[2];
extern int g_serial_modem_response_code;
extern int g_serial_modem_allow_incoming;
extern int g_serial_modem_init_telnet;

#ifdef _WIN32
#include <winsock2.h>
typedef int socklen_t;
#endif

#ifndef _WIN32
extern int h_errno;
#endif

int g_wsastartup_called = 0;

#ifndef _WIN32
// SOCKET is defined in scc.h, and is an "int" for non-Windows systems
// INVALID_SOCKET is -1 for non-WINDOWS
# define closesocket(s)		close(s)
#endif

/* Usage: scc_socket_open() called to init socket mode */
/*  At all times, we try to have a listen running on the incoming socket */
/* If we want to dial out, we close the incoming socket and create a new */
/*  outgoing socket.  Any hang-up causes the socket to be closed and it will */
/*  then re-open on a subsequent call to scc_socket_open */

void
scc_socket_open(dword64 dfcyc, int port, int cfg)
{
	Scc	*scc_ptr;

	// printf("scc_socket_open p:%d cfg:%d\n", port, cfg);
#if defined(_WIN32) && defined(SCC_SOCKETS)
	WSADATA	wsadata;
	int	ret;

	if(g_wsastartup_called == 0) {
		ret = WSAStartup(MAKEWORD(2,0), &wsadata);
		printf("WSAStartup ret: %d\n", ret);
		g_wsastartup_called = 1;
	}
#endif
	scc_ptr = &(g_scc[port]);
	scc_ptr->cur_state = cfg;		// successful socket
	scc_ptr->sockfd = INVALID_SOCKET; /* Indicate no socket open yet */
	scc_ptr->rdwrfd = INVALID_SOCKET; /* Indicate no socket open yet */
	scc_ptr->modem_state = 0;	/* 0 means talk to socket */
					/* 1 means talk to modem */
	scc_ptr->socket_error = 0;
	scc_ptr->socket_num_rings = 0;
	scc_ptr->socket_last_ring_dfcyc = 0;
	scc_ptr->dcd = 1;		/* 1 means carrier */
	scc_ptr->modem_s0_val = 0;		// Number of rings before answer
	scc_ptr->sockaddr_size = sizeof(struct sockaddr_in);
	free(scc_ptr->sockaddr_ptr);
	scc_ptr->sockaddr_ptr = malloc(scc_ptr->sockaddr_size);

	// cfg==1: Virtual Modem.  All set up, wait for AT commands now
	// cfg==2: Outgoing connection to g_serial_remote_ip[], do it now
	// cfg==3: Incoming connection on 6501/6502.  Do it now
	if(cfg == 2) {
		scc_ptr->modem_state = 0;
		// Do not open it now, it will be opened when output is
		//  sent to the port
	}
	if(cfg == 1) {
		scc_ptr->modem_state = 1;
		scc_ptr->dcd = 0;
		scc_socket_open_incoming(dfcyc, port);
	} else if(cfg == 3) {
		scc_ptr->modem_state = 0;
		scc_socket_open_incoming(dfcyc, port);
	}
}

void
scc_socket_close(int port)
{
#ifdef SCC_SOCKETS
	Scc	*scc_ptr;
	SOCKET	rdwrfd;
	SOCKET	sockfd;
	int	do_init;
	int	i;

	scc_ptr = &(g_scc[port]);

	// printf("In scc_socket_close, %d\n", port);

	rdwrfd = scc_ptr->rdwrfd;
	do_init = 0;
	if(rdwrfd != INVALID_SOCKET) {
		printf("socket_close: rdwrfd=%llx, closing\n", (dword64)rdwrfd);
		closesocket(rdwrfd);
		do_init = 1;
	}
	sockfd = scc_ptr->sockfd;
	if((sockfd != INVALID_SOCKET) && (rdwrfd != sockfd)) {
		printf("socket_close: sockfd=%llx, closing\n", (dword64)sockfd);
		closesocket(sockfd);
		do_init = 1;
	}

	scc_ptr->modem_state = 0;
	scc_ptr->socket_num_rings = 0;
	if(scc_ptr->cur_state == 1) {			// Virtual modem
		scc_ptr->modem_state = 1;
		scc_ptr->dcd = 0;
	}
	if(do_init) {
		scc_ptr->modem_cmd_len = 0;
		scc_ptr->telnet_iac = 0;
		for(i = 0; i < 2; i++) {
			scc_ptr->telnet_local_mode[i] = 0;
			scc_ptr->telnet_remote_mode[i] = 0;
			scc_ptr->telnet_reqwill_mode[i] = 0;
			scc_ptr->telnet_reqdo_mode[i] = 0;
		}
		scc_ptr->rdwrfd = INVALID_SOCKET;
		scc_ptr->sockfd = INVALID_SOCKET;
	}
#endif
}

void
scc_socket_close_extended(dword64 dfcyc, int port, int allow_retry)
{
#ifdef SCC_SOCKETS
	Scc	*scc_ptr;

	// printf("In scc_socket_close_extended, %d, %016llx\n", port, dfcyc);
	scc_socket_close(port);

	scc_ptr = &(g_scc[port]);
	if(scc_ptr->cur_state == 1) {		// Virtual modem mode
		scc_socket_send_modem_code(dfcyc, port, 3);
		scc_ptr->modem_state = 1;	// and go back to modem mode
	} else if((scc_ptr->cur_state == 2) && !allow_retry) {	// Remote IP
		scc_ptr->cur_state = -2;	// Error, give up
	}
#endif
}

void
scc_socket_maybe_open(dword64 dfcyc, int port, int must)
{
	Scc	*scc_ptr;

	scc_ptr = &(g_scc[port]);

	if(scc_ptr->sockfd != INVALID_SOCKET) {
		// Valid socket.  See if we should hang up
		if((scc_ptr->reg[5] & 0x80) && (scc_ptr->cur_state == 1)) {
			// Handle DTR forcing modem hang-up now
			printf("DTR is deasserted, hanging up\n");
			scc_socket_close(port);
		}
		return;
	}
	if(scc_ptr->cur_state == 2) {
		if(must) {
			scc_socket_open_outgoing(dfcyc, port,
					g_serial_remote_ip[port],
					g_serial_remote_port[port]);
		}
	} else {
		scc_socket_open_incoming(dfcyc, port);
	}
}

void
scc_socket_open_incoming(dword64 dfcyc, int port)
{
#ifdef SCC_SOCKETS
	SOCKET	sockfd;
	struct sockaddr_in sa_in;
	Scc	*scc_ptr;
	int	on, ret, inc;

	inc = 0;

	scc_ptr = &(g_scc[port]);

	if(scc_ptr->sockfd != INVALID_SOCKET) {
		/* it's already open, get out */
		return;
	}

	//printf("scc_socket_open_incoming: scc socket close being called\n");
	scc_socket_close(port);

	scc_ptr->socket_num_rings = 0;

	memset(scc_ptr->sockaddr_ptr, 0, scc_ptr->sockaddr_size);

	if(scc_ptr->cur_state == 2) {
		// Outgoing connection only, never accept an incoming connect
		return;
	}
	if(scc_ptr->cur_state == 1) {
		if(!g_serial_modem_allow_incoming) {
			// Virtual modem with incoming connections disallowed
			return;
		}
		if(scc_ptr->reg[5] & 0x80) {
			// DTR is forcing hang-up.  Don't open incoming socket
			return;
		}
	}

	while(1) {
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		//printf("sockfd ret: %llx\n", (dword64)sockfd);
		if(sockfd == INVALID_SOCKET) {
			printf("socket ret: -1, errno: %d\n", errno);
			scc_socket_close(port);
			scc_ptr->socket_error = -1;
			return;
		}
		/* printf("socket ret: %d\n", sockfd); */

		on = 1;
		ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
					(char *)&on, sizeof(on));
		if(ret < 0) {
			printf("setsockopt REUSEADDR ret: %d, err:%d\n",
				ret, errno);
			scc_socket_close(port);
			return;
		}

		memset(&sa_in, 0, sizeof(sa_in));
		sa_in.sin_family = AF_INET;
		sa_in.sin_port = htons(6501 + port + inc);
		sa_in.sin_addr.s_addr = htonl(INADDR_ANY);

		ret = bind(sockfd, (struct sockaddr *)&sa_in, sizeof(sa_in));
		printf("bind ret:%d\n", ret);

		if(ret >= 0) {
			ret = listen(sockfd, 1);
			break;
		}
		/* else ret to bind was < 0 */
		printf("bind or listen ret: %d, errno: %d\n", ret, errno);
		inc++;
		closesocket(sockfd);
		printf("Trying next port: %d\n", 6501 + port + inc);
		if(inc >= 10) {
			printf("Too many retries, quitting\n");
			scc_socket_close(port);
			scc_ptr->socket_error = -1;
			return;
		}
	}

	printf("SCC port %d is at unix port %d\n", port, 6501 + port + inc);

	scc_ptr->sockfd = sockfd;
	scc_ptr->socket_error = 0;

	scc_socket_make_nonblock(dfcyc, port);
#endif
}

void
scc_socket_open_outgoing(dword64 dfcyc, int port, const char *remote_ip_str,
						int remote_port)
{
#ifdef SCC_SOCKETS
	Scc	*scc_ptr;
	struct sockaddr_in sa_in;
	struct hostent *hostentptr;
	int	on;
	int	ret;
	SOCKET	sockfd;

	scc_ptr = &(g_scc[port]);

	// printf("scc socket close being called from socket_open_out\n");
	scc_socket_close(port);

	memset(scc_ptr->sockaddr_ptr, 0, scc_ptr->sockaddr_size);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	// printf("outgoing sockfd ret: %llx\n", (dword64)sockfd);
	if(sockfd == INVALID_SOCKET) {
		printf("socket ret: %llx, errno: %d\n", (dword64)sockfd, errno);
		scc_socket_close_extended(dfcyc, port, 0);
		return;
	}
	/* printf("socket ret: %d\n", sockfd); */

	on = 1;
	ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
					(char *)&on, sizeof(on));
	if(ret < 0) {
		printf("setsockopt REUSEADDR ret: %d, err:%d\n",
			ret, errno);
		scc_socket_close_extended(dfcyc, port, 0);
		return;
	}

	memset(&sa_in, 0, sizeof(sa_in));
	sa_in.sin_family = AF_INET;
	sa_in.sin_port = htons(remote_port);
	while(*remote_ip_str == ' ') {
		remote_ip_str++;		// Skip leading blanks
	}
	hostentptr = gethostbyname(remote_ip_str);
	printf("Connecting to %s, port:%d\n", remote_ip_str, remote_port);
	if(hostentptr == 0) {
#ifdef _WIN32
		fatal_printf("Lookup host %s failed\n",
						&scc_ptr->modem_cmd_str[0]);
#else
		fatal_printf("Lookup host %s failed, herrno: %d\n",
					&scc_ptr->modem_cmd_str[0], h_errno);
#endif
		closesocket(sockfd);
		scc_socket_close_extended(dfcyc, port, 0);
		return;
	}
	memcpy(&sa_in.sin_addr.s_addr, hostentptr->h_addr,
							hostentptr->h_length);
	/* The above copies the 32-bit internet address into */
	/*  sin_addr.s_addr.  It's in correct network format */

	ret = connect(sockfd, (struct sockaddr *)&sa_in, sizeof(sa_in));
	if(ret < 0) {
		printf("connect ret: %d, errno: %d\n", ret, errno);
		closesocket(sockfd);
		scc_socket_close_extended(dfcyc, port, 0);
		return;
	}
	scc_socket_modem_connect(dfcyc, port);
	scc_ptr->dcd = 1;		/* carrier on */
	scc_ptr->modem_state = 0;	/* talk to socket */
	scc_ptr->socket_num_rings = 0;

	printf("SCC port %d is now outgoing to %s:%d\n", port, remote_ip_str,
							remote_port);

	scc_ptr->sockfd = sockfd;

	scc_socket_make_nonblock(dfcyc, port);
	scc_ptr->rdwrfd = scc_ptr->sockfd;
#endif
}

void
scc_socket_make_nonblock(dword64 dfcyc, int port)
{
#ifdef SCC_SOCKETS
	Scc	*scc_ptr;
	SOCKET	sockfd;
	int	ret;
# ifdef _WIN32
	u_long	flags;
# else
	int	flags;
# endif

	scc_ptr = &(g_scc[port]);
	sockfd = scc_ptr->sockfd;

# ifdef _WIN32
	flags = 1;
	ret = ioctlsocket(sockfd, FIONBIO, &flags);
	if(ret != 0) {
		printf("ioctlsocket ret: %d\n", ret);
	}
# else
	flags = fcntl(sockfd, F_GETFL, 0);
	if(flags == -1) {
		printf("fcntl GETFL ret: %d, errno: %d\n", flags, errno);
		scc_socket_close_extended(dfcyc, port, 0);
		return;
	}
	ret = fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
	if(ret == -1) {
		printf("fcntl SETFL ret: %d, errno: %d\n", ret, errno);
		scc_socket_close_extended(dfcyc, port, 0);
		return;
	}
# endif
#endif
}

void
scc_accept_socket(dword64 dfcyc, int port)
{
#ifdef SCC_SOCKETS
	Scc	*scc_ptr;
	SOCKET	rdwrfd;
	socklen_t address_len;
	int	flags, num_rings, ret;

	scc_ptr = &(g_scc[port]);

	if(scc_ptr->sockfd == INVALID_SOCKET) {
		// printf("in accept_socket, call socket_open\n");
		scc_socket_open_incoming(dfcyc, port);
	}
	if(scc_ptr->sockfd == INVALID_SOCKET) {
		return;		/* just give up */
	}
	if(scc_ptr->rdwrfd == INVALID_SOCKET) {
		address_len = scc_ptr->sockaddr_size;
		rdwrfd = accept(scc_ptr->sockfd, scc_ptr->sockaddr_ptr,
								&address_len);
		scc_ptr->sockaddr_size = (int)address_len;
		if(rdwrfd == INVALID_SOCKET) {
			return;
		}

		flags = 0;
		ret = 0;
#ifndef _WIN32
		/* For Linux, we need to set O_NONBLOCK on the rdwrfd */
		flags = fcntl(rdwrfd, F_GETFL, 0);
		if(flags == -1) {
			printf("fcntl GETFL ret: %d, errno: %d\n", flags,errno);
			return;
		}
		ret = fcntl(rdwrfd, F_SETFL, flags | O_NONBLOCK);
		if(ret == -1) {
			printf("fcntl SETFL ret: %d, errno: %d\n", ret, errno);
			return;
		}
#endif
		scc_ptr->rdwrfd = rdwrfd;
		printf("Set port[%d].rdwrfd = %llx\n", port, (dword64)rdwrfd);

		num_rings = 4;
		if(scc_ptr->modem_s0_val > 0) {
			num_rings = scc_ptr->modem_s0_val;
		}
		scc_ptr->socket_num_rings = num_rings;
		scc_ptr->socket_last_ring_dfcyc = 0;	/* do ring now*/

		/* and send some telnet codes */
		if(g_serial_modem_init_telnet) {
			scc_ptr->telnet_reqwill_mode[0] = 0xa;
			scc_ptr->telnet_reqdo_mode[0] = 0xa;
				/* 3=GO_AH and 1=ECHO */
			scc_ptr->telnet_reqdo_mode[1] = 0x4;	// 34=LINEMODE
		}
		printf("Telnet reqwill and reqdo's initialized: %08x_%08x,"
				"%08x_%08x\n",
				scc_ptr->telnet_reqwill_mode[1],
				scc_ptr->telnet_reqwill_mode[0],
				scc_ptr->telnet_reqdo_mode[1],
				scc_ptr->telnet_reqdo_mode[0]);

		scc_socket_modem_do_ring(dfcyc, port);
	}
#endif
}

void
scc_socket_telnet_reqs(dword64 dfcyc, int port)
{
	Scc	*scc_ptr;
	word32	mask, willmask, domask;
	int	i, j;

	scc_ptr = &(g_scc[port]);
	for(i = 0; i < 64; i++) {
		j = i >> 5;
		mask = 1 << (i & 31);
		willmask = scc_ptr->telnet_reqwill_mode[j];
		if(willmask & mask) {
			scc_printf("Telnet reqwill %d\n", i);
			scc_add_to_writebuf(dfcyc, port, 0xff);
			scc_add_to_writebuf(dfcyc, port, 0xfb);	/* WILL */
			scc_add_to_writebuf(dfcyc, port, i);
		}
		domask = scc_ptr->telnet_reqdo_mode[j];
		if(domask & mask) {
			scc_printf("Telnet reqdo %d\n", i);
			scc_add_to_writebuf(dfcyc, port, 0xff);
			scc_add_to_writebuf(dfcyc, port, 0xfd);	/* DO */
			scc_add_to_writebuf(dfcyc, port, i);
		}
	}
}

void
scc_socket_fill_readbuf(dword64 dfcyc, int port, int space_left)
{
#ifdef SCC_SOCKETS
	byte	tmp_buf[256];
	Scc	*scc_ptr;
	SOCKET	rdwrfd;
	int	ret;
	int	i;

	scc_ptr = &(g_scc[port]);

	scc_accept_socket(dfcyc, port);
	scc_socket_modem_do_ring(dfcyc, port);

	if(scc_ptr->modem_state) {
		/* Just get out, this is modem mode */
		/*  The transmit function stuffs any bytes in receive buf */
		return;
	}
	rdwrfd = scc_ptr->rdwrfd;
	if(rdwrfd == INVALID_SOCKET) {
		return;		/* just get out */
	}

	/* Try reading some bytes */
	space_left = MY_MIN(space_left, 256);
	ret = (int)recv(rdwrfd, tmp_buf, space_left, 0);
	if(ret > 0) {
		for(i = 0; i < ret; i++) {
			if(tmp_buf[i] == 0) {
				/* Skip null chars */
				continue;
			}
			scc_socket_recvd_char(dfcyc, port, tmp_buf[i]);
		}
	} else if(ret == 0) {
		/* assume socket close */
		printf("recv got 0 from rdwrfd=%llx, closing\n",
							(dword64)rdwrfd);
		scc_socket_close_extended(dfcyc, port, 1);
	}
#endif
}

void
scc_socket_recvd_char(dword64 dfcyc, int port, int c)
{
	Scc	*scc_ptr;
	word32	locmask, remmask, mask, reqwillmask, reqdomask;
	int	telnet_mode, telnet_iac, eff_c, cpos, reply;

	scc_ptr = &(g_scc[port]);

	telnet_mode = scc_ptr->telnet_mode;
	telnet_iac = scc_ptr->telnet_iac;

	eff_c = c;
	if(scc_ptr->cur_state == 2) {
		// Outgoing only connection, support 8-bit transfers with
		//  no telnet command support
		telnet_mode = 0;
		telnet_iac = 0;
	} else if(telnet_iac == 0) {
		if(c == 0xff) {
			scc_ptr->telnet_iac = 0xff;
			return;			/* and just get out */
		}
	} else {
		/* last char was 0xff, see if this is 0xff */
		if(c != 0xff) {
			/* this is some kind of command */
			eff_c = eff_c | 0x100;	/* indicate prev char IAC */
		}
	}
	scc_ptr->telnet_iac = 0;

	mask = 1 << (c & 31);
	cpos = (c >> 5) & 1;
	locmask = scc_ptr->telnet_local_mode[cpos] & mask;
	remmask = scc_ptr->telnet_remote_mode[cpos] & mask;
	reqwillmask = scc_ptr->telnet_reqwill_mode[cpos] & mask;
	reqdomask = scc_ptr->telnet_reqdo_mode[cpos] & mask;
	switch(telnet_mode) {
	case 0:	/* just passing through bytes */
		switch(eff_c) {
		case 0x1fe:	/* DON'T */
		case 0x1fd:	/* DO */
		case 0x1fc:	/* WON'T */
		case 0x1fb:	/* WILL */
		case 0x1fa:	/* SB */
			telnet_mode = c;
			break;
		default:
			if(eff_c < 0x100) {
				scc_add_to_readbuf(dfcyc, port, c);
			}
			break;
		}
		break;
	case 3: /* LINEMODE SB SLC, octet 0 */
		if(eff_c == 0x1f0) {
			/* SE, the end */
			telnet_mode = 0;
		}
		scc_printf("LINEMODE SLC octet 0: %02x\n", c);
		telnet_mode = 4;
		break;
	case 4: /* LINEMODE SB SLC, octet 1 */
		scc_printf("LINEMODE SLC octet 1: %02x\n", c);
		telnet_mode = 5;
		if(eff_c == 0x1f0) {
			/* SE, the end */
			scc_printf("Got SE at octet 1...\n");
			telnet_mode = 0;
		}
		break;
	case 5: /* LINEMODE SB SLC, octet 2 */
		scc_printf("LINEMODE SLC octet 2: %02x\n", c);
		telnet_mode = 3;
		if(eff_c == 0x1f0) {
			/* SE, the end */
			printf("Got Telnet SE at octet 2...\n");
			telnet_mode = 0;
		}
		break;
	case 34: /* LINEMODE SB beginning */
		switch(c) {
		case 3:	/* SLC */
			telnet_mode = 3;
			break;
		default:
			telnet_mode = 0xee;	/* go to SB eat */
			break;
		}
		break;
	case 0xfa: /* in 0xfa = SB mode, eat up subcommands */
		switch(c) {
		case 34:	/* LINEMODE */
			telnet_mode = 34;
			break;
		default:
			telnet_mode = 0xee;	/* SB eat mode */
			break;
		}
		break;
	case 0xee: /* in SB eat mode */
		if(eff_c == 0x1f0) {		/* SE, end of sub-command */
			telnet_mode = 0;
		} else {
			/* just stay in eat mode */
		}
		break;
	case 0xfe:	/* previous char was "DON'T" */
		if(locmask && (reqwillmask == 0)) {
			/* it's a mode change */
			/* always OK to turn off a mode that we had on */
			scc_add_to_writebuf(dfcyc, port, 0xff);
			scc_add_to_writebuf(dfcyc, port, 0xfc);	/* WON'T */
			scc_add_to_writebuf(dfcyc, port, c);
		}
		scc_ptr->telnet_local_mode[cpos] &= ~mask;
		scc_ptr->telnet_reqwill_mode[cpos] &= ~mask;
		telnet_mode = 0;
		break;
	case 0xfd:	/* previous char was "DO" */
		reply = 0xfc;
		if(locmask == 0 && (reqwillmask == 0)) {
			/* it's a mode change, send some response  */
			reply = 0xfc;	/* nack it with WON'T */
			if(c == 0x03 || c == 0x01) {
				reply = 0xfb;	/* Ack with WILL */
			}
			scc_add_to_writebuf(dfcyc, port, 0xff);
			scc_add_to_writebuf(dfcyc, port, reply);
			scc_add_to_writebuf(dfcyc, port, c);
		}
		if(reqwillmask || (reply == 0xfb)) {
			scc_ptr->telnet_local_mode[cpos] |= mask;
		}
		scc_ptr->telnet_reqwill_mode[cpos] &= ~mask;
		telnet_mode = 0;
		break;
	case 0xfc:	/* previous char was "WON'T" */
		if(remmask && (reqdomask == 0)) {
			/* it's a mode change, ack with DON'T */
			scc_add_to_writebuf(dfcyc, port, 0xff);
			scc_add_to_writebuf(dfcyc, port, 0xfe);	/* DON'T */
			scc_add_to_writebuf(dfcyc, port, c);
		}
		scc_ptr->telnet_remote_mode[cpos] &= ~mask;
		scc_ptr->telnet_reqdo_mode[cpos] &= ~mask;
		telnet_mode = 0;
		break;
	case 0xfb:	/* previous char was "WILL" */
		reply = 0xfe;	/* nack it with DON'T */
		if(remmask == 0 && (reqdomask == 0)) {
			/* it's a mode change, send some response  */
			if(c == 0x03 || c == 0x01) {
				reply = 0xfd;	/* Ack with DO */
			}
			scc_add_to_writebuf(dfcyc, port, 0xff);
			scc_add_to_writebuf(dfcyc, port, reply);
			scc_add_to_writebuf(dfcyc, port, c);
		}
		if(reqdomask || (reply == 0xfd)) {
			scc_ptr->telnet_remote_mode[cpos] |= mask;
		}
		scc_ptr->telnet_reqdo_mode[cpos] &= ~mask;
		telnet_mode = 0;
		break;
	default:
		telnet_mode = 0;
		break;
	}
	scc_ptr->telnet_mode = telnet_mode;
}

void
scc_socket_empty_writebuf(dword64 dfcyc, int port)
{
#ifdef SCC_SOCKETS
# ifndef _WIN32
	struct sigaction newact, oldact;
# endif
	Scc	*scc_ptr;
	dword64	diff_dusec;
	SOCKET	rdwrfd;
	int	plus_mode, plus_char, rdptr, wrptr, done, ret, len, c;
	int	i;

	scc_socket_maybe_open(dfcyc, port, 0);

	scc_ptr = &(g_scc[port]);

	/* See if +++ done and we should go to command mode */
	diff_dusec = (dfcyc - scc_ptr->out_char_dfcyc) >> 16;
	if((diff_dusec > 900LL*1000) && (scc_ptr->modem_plus_mode == 3) &&
				(scc_ptr->modem_state == 0) &&
				(scc_ptr->cur_state == 1)) {
		scc_ptr->modem_state = 1;	/* go modem mode, stay connect*/
		scc_ptr->modem_plus_mode = 0;
		scc_socket_send_modem_code(dfcyc, port, 0);	// "OK"
	}

	/* Try writing some bytes */
	done = 0;
	while(!done) {
		rdptr = scc_ptr->out_rdptr;
		wrptr = scc_ptr->out_wrptr;
		if(rdptr == wrptr) {
			done = 1;
			break;
		}
		rdwrfd = scc_ptr->rdwrfd;
		len = wrptr - rdptr;
		if(len < 0) {
			len = SCC_OUTBUF_SIZE - rdptr;
		}
		if(len > 32) {
			len = 32;
		}
		//printf("Writing data, %d bytes, modem_state:%d\n", len,
		//					scc_ptr->modem_state);
		if(len <= 0) {
			done = 1;
			break;
		}
		scc_socket_maybe_open(dfcyc, port, 1);

		if(scc_ptr->modem_state) {
			len = 1;
			scc_socket_modem_write(dfcyc, port,
						scc_ptr->out_buf[rdptr]);
			scc_ptr->write_called_this_vbl = 0;
			ret = 1;
		} else {
			if(rdwrfd == INVALID_SOCKET) {
				return;
			}
			plus_char = scc_ptr->modem_s2_val;
			if((plus_char == 0) || (plus_char >= 128)) {
				plus_char = 0xfff;		// Invalid
				scc_ptr->modem_plus_mode = 0;
			}
			// Look for '+++' within .8 seconds
			for(i = 0; i < len; i++) {
				c = scc_ptr->out_buf[rdptr + i];
				plus_mode = scc_ptr->modem_plus_mode;
				diff_dusec =
					(dfcyc - scc_ptr->out_char_dfcyc) >> 16;
				if(c == plus_char && plus_mode == 0) {
					if(diff_dusec > 500LL*1000) {
						scc_ptr->modem_plus_mode = 1;
					}
				} else if(c == plus_char) {
					if(diff_dusec < 800LL*1000) {
						plus_mode++;
						scc_ptr->modem_plus_mode =
								plus_mode;
					}
				} else {
					scc_ptr->modem_plus_mode = 0;
				}
				scc_ptr->out_char_dfcyc = dfcyc;
			}
# ifndef _WIN32
			// ignore SIGPIPE around writes to the socket, so we
			//  can catch a closed socket and prepare to accept
			//  a new connection.  Otherwise, SIGPIPE kills KEGS
			sigemptyset(&newact.sa_mask);
			newact.sa_handler = SIG_IGN;
			newact.sa_flags = 0;
			sigaction(SIGPIPE, &newact, &oldact);
# endif

			ret = (int)send(rdwrfd, &(scc_ptr->out_buf[rdptr]),
									len, 0);

# ifndef _WIN32
			sigaction(SIGPIPE, &oldact, 0);
			/* restore previous SIGPIPE behavior */
# endif

#if 0
			printf("sock output: %02x, len:%d\n",
					scc_ptr->out_buf[rdptr], len);
#endif

		}

		if(ret == 0) {
			done = 1;	/* give up for now */
			break;
		} else if(ret < 0) {
			/* assume socket is dead */
			printf("socket write failed, resuming modem mode\n");
			scc_socket_close_extended(dfcyc, port, 1);
			done = 1;
			break;
		} else {
			rdptr = rdptr + ret;
			if(rdptr >= SCC_OUTBUF_SIZE) {
				rdptr = rdptr - SCC_OUTBUF_SIZE;
			}
			scc_ptr->out_rdptr = rdptr;
		}
	}
#endif
}

void
scc_socket_modem_write(dword64 dfcyc, int port, int c)
{
	Scc	*scc_ptr;
	char	*str;
	word32	modem_mode;
	int	do_echo, got_at, len;

	scc_ptr = &(g_scc[port]);

	if(scc_ptr->sockfd == INVALID_SOCKET) {
		scc_socket_open_incoming(dfcyc, port);
	}

	modem_mode = scc_ptr->modem_mode;
	str = (char *)&(scc_ptr->modem_cmd_str[0]);

	do_echo = ((modem_mode & SCCMODEM_NOECHO) == 0);
	len = scc_ptr->modem_cmd_len;
	got_at = 0;
#if 0
	printf("T:%016llx M[%d][%d]: %02x\n", dfcyc, port, len, c);
#endif
	if(len >= 2 && str[0] == 'a' && str[1] == 't') {
		/* we've got an 'at', do not back up past it */
		got_at = 1;
	}
	if(c == 0x0d) {
		if(do_echo) {
			scc_add_to_readbuf(dfcyc, port, c);	/* echo cr */
			scc_add_to_readbuf(dfcyc, port, 0x0a);	/* echo lf */
		}
		do_echo = 0;	/* already did the echo */
		scc_socket_do_cmd_str(dfcyc, port);
		scc_ptr->modem_cmd_len = 0;
		len = 0;
		str[0] = 0;
	} else if(c == 0x08) {
		if(len <= 0) {
			do_echo = 0;	/* do not go past left margin */
		} else if(len == 2 && got_at) {
			do_echo = 0;	/* do not erase "AT" */
		} else {
			/* erase a character */
			len--;
			str[len] = 0;
		}
	} else if(c < 0x20) {
		/* ignore all control characters, don't echo */
		/* includes line feeds and nulls */
		do_echo = 0;
	} else {
		/* other characters */
		if(len < SCC_MODEM_MAX_CMD_STR) {
			str[len] = tolower(c);
			str[len+1] = 0;
			len++;
		}
	}
	scc_ptr->modem_cmd_len = len;
	if(do_echo) {
		scc_add_to_readbuf(dfcyc, port, c);	/* echo */
	}
}

void
scc_socket_do_cmd_str(dword64 dfcyc, int port)
{
	Scc	*scc_ptr;
	char	*str;
	int	pos, len, ret_val, reg, reg_val, was_amp, out_port, c;
	int	i;

	scc_ptr = &(g_scc[port]);

	str = (char *)&(scc_ptr->modem_cmd_str[0]);
	printf("Got modem string :%s:=%02x %02x %02x\n", str, str[0], str[1],
						str[2]);

	len = scc_ptr->modem_cmd_len;
	str[len] = 0;
	str[len+1] = 0;
	str[len+2] = 0;
	pos = -1;
	if(len < 2) {
		/* just ignore it */
		return;
	}
	if(str[0] != 'a' || str[1] != 't') {
		return;
	}

	/* Some AT command received--make sure socket 6501/6502 is open */
	printf("Some AT command received, sockfd=%llx\n",
						(dword64)scc_ptr->sockfd);

	pos = 2 - 1;
	ret_val = 0;		/* "OK" */
	was_amp = 0;
	while(++pos < len) {
		c = str[pos] + was_amp;
		was_amp = 0;
		switch(c) {
		case '&':	/* at& */
			was_amp = 0x100;
			break;
		case 'z':	/* atz */
			scc_ptr->modem_mode = 0;
			scc_ptr->modem_s0_val = 0;
			pos = len;		/* ignore any other commands */
			break;
		case 'e':	/* ate = echo */
			c = str[pos+1];
			if(c == '1') {
				scc_ptr->modem_mode &= ~SCCMODEM_NOECHO;
				pos++;
			} else {
				scc_ptr->modem_mode |= SCCMODEM_NOECHO;
				pos++;
			}
			break;
		case 'v':	/* atv = verbose */
			c = str[pos+1];
			if(c == '1') {
				scc_ptr->modem_mode &= ~SCCMODEM_NOVERBOSE;
				pos++;
			} else {
				scc_ptr->modem_mode |= SCCMODEM_NOVERBOSE;
				pos++;
			}
			break;
		case 'o':	/* ato = go online */
			printf("ato\n");
			if(scc_ptr->dcd && scc_ptr->modem_state &&
					(scc_ptr->rdwrfd != INVALID_SOCKET)) {
				printf("Going back online\n");
				scc_socket_modem_connect(dfcyc, port);
				scc_ptr->modem_state = 0;
						// talk to socket
				ret_val = -1;
			}
			break;
		case 'h':	/* ath = hang up */
			printf("ath, hanging up\n");
			scc_socket_close(port);
			if(scc_ptr->rdwrfd != INVALID_SOCKET) {
				scc_socket_close_extended(dfcyc, port, 0);
			}
			/* scc_socket_maybe_open_incoming(dfcyc, port); */
							/* reopen listen */
			break;
		case 'a':	/* ata */
			printf("Doing ATA\n");
			scc_socket_do_answer(dfcyc, port);
			ret_val = -1;
			break;
		case 'd':	/* atd */
			scc_ptr->modem_out_portnum = 23;
			pos++;
			c = str[pos];
			if(c == 't' || c == 'p') {
				/* skip tone or pulse */
				pos++;
			}
			/* see if it is 111 */
			if(strcmp(&str[pos], "111") == 0) {
				/* Do PPP! */
			} else {
				/* get string to connect to */
				/* Shift string so hostname moves to str[0] */
				for(i = 0; i < len; i++) {
					c = str[pos];
					if(c == ':') {
						/* get port number now */
						out_port = (int)strtol(
							&str[pos+1], 0, 10);
						if(out_port <= 1) {
							out_port = 23;
						}
						scc_ptr->modem_out_portnum =
								out_port;
						c = 0;
					}
					str[i] = c;
					if((pos >= len) || (c == 0)) {
						break;
					}
					pos++;
				}
			}
			scc_socket_open_outgoing(dfcyc, port,
				(char *)&scc_ptr->modem_cmd_str[0],
				scc_ptr->modem_out_portnum);
			ret_val = -1;
			pos = len;	/* always eat rest of the line */
			break;
		case 's':	/* atsnn=yy */
			pos++;
			reg = 0;
			while(1) {
				c = str[pos];
				if(c < '0' || c > '9') {
					break;
				}
				reg = (reg * 10) + c - '0';
				pos++;
			}
			if(c == '?') {
				/* display S-register */
				if(reg == 0) {
					scc_add_to_readbufv(dfcyc, port,
						"S0=%d\n",
						scc_ptr->modem_s0_val);
				}
				break;
			}
			if(c != '=') {
				break;
			}
			pos++;
			reg_val = 0;
			while(1) {
				c = str[pos];
				if(c < '0' || c >'9') {
					break;
				}
				reg_val = (reg_val * 10) + c - '0';
				pos++;
			}
			printf("ats%d = %d\n", reg, reg_val);
			if(reg == 0) {
				scc_ptr->modem_s0_val = reg_val;
			}
			if(reg == 2) {
				scc_ptr->modem_s2_val = reg_val;
			}
			pos--;
			break;
		default:
			/* some command--peek into next chars to finish it */
			while(1) {
				c = str[pos+1];
				if(c >= '0' && c <= '9') {
					/* eat numbers */
					pos++;
					continue;
				}
				if(c == '=') {
					/* eat this as well */
					pos++;
					continue;
				}
				/* else get out */
				break;
			}
		}
	}

	if(ret_val >= 0) {
		scc_socket_send_modem_code(dfcyc, port, ret_val);
	}
}

void
scc_socket_send_modem_code(dword64 dfcyc, int port, int code)
{
	Scc	*scc_ptr;
	char	*str;
	word32	modem_mode;

	scc_ptr = &(g_scc[port]);

	switch(code) {
	case 0: str = "OK"; break;
	case 1: str = "CONNECT"; break;
	case 2: str = "RING"; break;
	case 3: str = "NO CARRIER"; break;
	case 4: str = "ERROR"; break;
	case 5: str = "CONNECT 1200"; break;
	case 10: str = "CONNECT 2400"; break;
	case 12: str = "CONNECT 9600"; break;	// Generic AT docs/Warp6 BBS
	case 13: str = "CONNECT 9600"; break;	// US Robotics Sportster/HST
	case 14: str = "CONNECT 19200"; break;	// Warp6 BBS
	case 16: str = "CONNECT 19200"; break;	// Generic AT docs
	case 25: str = "CONNECT 14400"; break;	// US Robotics Sportster
	case 85: str = "CONNECT 19200"; break;	// US Robotics Sportster
	case 18: str = "CONNECT 57600"; break;	// Generic AT docs/Warp6 BBS
	case 28: str = "CONNECT 38400"; break;	// Warp6 BBS/Hayes
	default:
		str = "ERROR";
	}

	printf("Sending modem code %d = %s\n", code, str);

	modem_mode = scc_ptr->modem_mode;
	if(modem_mode & SCCMODEM_NOVERBOSE) {
		/* just the number */
		scc_add_to_readbufv(dfcyc, port, "%d", code);
		scc_add_to_readbuf(dfcyc, port, 0x0d);
	} else {
		scc_add_to_readbufv(dfcyc, port, "%s\n", str);
	}
}

void
scc_socket_modem_connect(dword64 dfcyc, int port)
{
	// Send code telling Term program the connect speed
	if(g_scc[port].cur_state == 1) {		// Virtual modem
		scc_socket_send_modem_code(dfcyc, port,
			g_serial_modem_response_code);	/*28=38400*/
	}
}

void
scc_socket_modem_do_ring(dword64 dfcyc, int port)
{
	Scc	*scc_ptr;
	dword64	diff_dusecs;
	int	num_rings;

	scc_ptr = &(g_scc[port]);
	num_rings = scc_ptr->socket_num_rings;
	if((num_rings > 0) && scc_ptr->modem_state) {
		num_rings--;
		diff_dusecs = (dfcyc - scc_ptr->socket_last_ring_dfcyc) >> 16;
		if(diff_dusecs < 2LL*1000*1000) {
			return;		/* nothing more to do */
		}

		scc_ptr->socket_num_rings = num_rings;
		scc_ptr->socket_last_ring_dfcyc = dfcyc;
		if(num_rings <= 0) {
			/* decide on answering */
			if(scc_ptr->modem_s0_val) {
				scc_socket_do_answer(dfcyc, port);
			} else {
				printf("No answer, closing socket\n");
				scc_socket_close(port);
			}
		} else {
			scc_socket_send_modem_code(dfcyc, port, 2); /* RING */
		}
	}
}

void
scc_socket_do_answer(dword64 dfcyc, int port)
{
	Scc	*scc_ptr;

	scc_ptr = &(g_scc[port]);
	scc_accept_socket(dfcyc, port);
	if(scc_ptr->rdwrfd == INVALID_SOCKET) {
		printf("Answer when rdwrfd=-1, closing\n");
		scc_socket_close_extended(dfcyc, port, 0);
		/* send NO CARRIER message */
	} else {
		scc_socket_telnet_reqs(dfcyc, port);
		printf("Send telnet reqs\n");
		if(scc_ptr->modem_state) {
			scc_socket_modem_connect(dfcyc, port);
		}
		scc_ptr->modem_state = 0;		// Talk to socket
		scc_ptr->dcd = 1;			// carrier on
		scc_ptr->socket_num_rings = 0;
	}
}

