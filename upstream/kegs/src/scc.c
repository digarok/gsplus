const char rcsid_scc_c[] = "@(#)$KmKId: scc.c,v 1.74 2025-04-29 22:17:05+00 kentd Exp $";

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

// Driver for the Zilog SCC Z8530, which implements two channels (A,B) of
//  serial ports, controlled by $C038-$C03B

#include "defc.h"

extern int Verbose;
extern int g_code_yellow;
extern dword64 g_cur_dfcyc;
extern int g_serial_cfg[2];
extern int g_serial_mask[2];
extern char *g_serial_remote_ip[2];
extern int g_serial_remote_port[2];
extern char *g_serial_device[2];
extern int g_serial_win_device[2];
extern int g_irq_pending;

/* scc	port 0 == channel A = slot 1 = c039/c03b */
/*	port 1 == channel B = slot 2 = c038/c03a */

#include "scc.h"
#define SCC_R14_DPLL_SOURCE_BRG		0x100
#define SCC_R14_DPLL_SOURCE_RTXC	0x200

#define SCC_DCYCS_PER_PCLK	((DCYCS_1_MHZ) / ((DCYCS_28_MHZ) /8))
#define SCC_DCYCS_PER_XTAL	((DCYCS_1_MHZ) / 3686400.0)

// PCLK is 3.5795MHz

#define SCC_BR_EVENT			1
#define SCC_TX_EVENT			2
#define SCC_RX_EVENT			3
#define SCC_MAKE_EVENT(port, a)		(((a) << 1) + (port))

Scc	g_scc[2];

int g_baud_table[] = {
	110, 300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200
};

int g_scc_overflow = 0;
int	g_scc_init = 0;

// cur_state >= 0 and matches g_serial_cfg[port]: port is in that mode
// cur_state = -1: port is in no particular mode and should go to g_serial_cfg[]
// cur_state = -2: port failed to enter g_serial_cfg[], do not try again until
//		something changes

void
scc_init()
{
	Scc	*scc_ptr;
	int	i, j;

	for(i = 0; i < 2; i++) {
		scc_ptr = &(g_scc[i]);
		memset(scc_ptr, 0, sizeof(*scc_ptr));
		scc_ptr->cur_state = -1;
		scc_ptr->modem_state = 0;
		scc_ptr->sockfd = INVALID_SOCKET;
		scc_ptr->rdwrfd = INVALID_SOCKET;
		scc_ptr->sockaddr_ptr = 0;
		scc_ptr->sockaddr_size = 0;
		scc_ptr->unix_dev_fd = -1;
		scc_ptr->win_com_handle = 0;
		scc_ptr->win_dcb_ptr = 0;
		scc_ptr->br_event_pending = 0;
		scc_ptr->rx_event_pending = 0;
		scc_ptr->tx_event_pending = 0;
		scc_ptr->char_size = 8;
		scc_ptr->baud_rate = 9600;
		scc_ptr->telnet_mode = 0;
		scc_ptr->telnet_iac = 0;
		scc_ptr->out_char_dfcyc = 0;
		scc_ptr->socket_error = 0;
		scc_ptr->socket_num_rings = 0;
		scc_ptr->socket_last_ring_dfcyc = 0;
		scc_ptr->modem_mode = 0;
		scc_ptr->modem_plus_mode = 0;
		scc_ptr->modem_s0_val = 0;
		scc_ptr->modem_s2_val = '+';
		scc_ptr->modem_cmd_len = 0;
		scc_ptr->modem_out_portnum = 23;
		scc_ptr->modem_cmd_str[0] = 0;
		for(j = 0; j < 2; j++) {
			scc_ptr->telnet_local_mode[j] = 0;
			scc_ptr->telnet_remote_mode[j] = 0;
			scc_ptr->telnet_reqwill_mode[j] = 0;
			scc_ptr->telnet_reqdo_mode[j] = 0;
		}
	}

	g_scc_init = 1;
}

void
scc_reset()
{
	Scc	*scc_ptr;
	int	i;

	if(!g_scc_init) {
		halt_printf("scc_reset called before init\n");
		return;
	}
	for(i = 0; i < 2; i++) {
		scc_ptr = &(g_scc[i]);

		scc_ptr->mode = 0;
		scc_ptr->reg_ptr = 0;
		scc_ptr->in_rdptr = 0;
		scc_ptr->in_wrptr = 0;
		scc_ptr->out_rdptr = 0;
		scc_ptr->out_wrptr = 0;
		scc_ptr->dcd = 1 - i;		// 1 for slot 1, 0 for slot 2
		scc_ptr->wantint_rx = 0;
		scc_ptr->wantint_tx = 0;
		scc_ptr->wantint_zerocnt = 0;
		scc_ptr->read_called_this_vbl = 0;
		scc_ptr->write_called_this_vbl = 0;
		scc_evaluate_ints(i);
		scc_hard_reset_port(i);
	}
}

void
scc_hard_reset_port(int port)
{
	Scc	*scc_ptr;

	scc_reset_port(port);

	scc_ptr = &(g_scc[port]);
	scc_ptr->reg[14] = 0;		/* zero bottom two bits */
	scc_ptr->reg[13] = 0;
	scc_ptr->reg[12] = 0;
	scc_ptr->reg[11] = 0x08;
	scc_ptr->reg[10] = 0;
	scc_ptr->reg[7] = 0;
	scc_ptr->reg[6] = 0;
	scc_ptr->reg[5] = 0;
	scc_ptr->reg[4] = 0x04;
	scc_ptr->reg[3] = 0;
	scc_ptr->reg[2] = 0;
	scc_ptr->reg[1] = 0;

	/* HACK HACK: */
	g_scc[0].reg[9] = 0;		/* Clear all interrupts */

	scc_evaluate_ints(port);

	scc_regen_clocks(port);
}

void
scc_reset_port(int port)
{
	Scc	*scc_ptr;

	scc_ptr = &(g_scc[port]);
	scc_ptr->reg[15] = 0xf8;
	scc_ptr->reg[14] &= 0x03;	/* 0 most (including >= 0x100) bits */
	scc_ptr->reg[10] = 0;
	scc_ptr->reg[5] &= 0x65;	/* leave tx bits and sdlc/crc bits */
	scc_ptr->reg[4] |= 0x04;	/* Set async mode */
	scc_ptr->reg[3] &= 0xfe;	/* clear receiver enable */
	scc_ptr->reg[1] &= 0xfe;	/* clear ext int enable */

	scc_ptr->br_is_zero = 0;
	scc_ptr->tx_buf_empty = 1;

	scc_ptr->wantint_rx = 0;
	scc_ptr->wantint_tx = 0;
	scc_ptr->wantint_zerocnt = 0;

	scc_ptr->rx_queue_depth = 0;

	scc_evaluate_ints(port);

	scc_regen_clocks(port);

	scc_clr_tx_int(port);
	scc_clr_rx_int(port);
}

void
scc_regen_clocks(int port)
{
	Scc	*scc_ptr;
	double	br_dcycs, tx_dcycs, rx_dcycs, rx_char_size, tx_char_size;
	double	clock_mult, dpll_dcycs;
	word32	reg4, reg11, reg14, br_const, max_diff, diff;
	int	baud, cur_state, baud_entries, pos;
	int	i;

	/*	Always do baud rate generator */
	scc_ptr = &(g_scc[port]);
	br_const = (scc_ptr->reg[13] << 8) + scc_ptr->reg[12];
	br_const += 2;	/* counts down past 0 */

	reg4 = scc_ptr->reg[4];		// Transmit/Receive misc params
	clock_mult = 1.0;
	switch((reg4 >> 6) & 3) {
	case 0:		/* x1 */
		clock_mult = 1.0;
		break;
	case 1:		/* x16 */
		clock_mult = 16.0;
		break;
	case 2:		/* x32 */
		clock_mult = 32.0;
		break;
	case 3:		/* x64 */
		clock_mult = 64.0;
		break;
	}

	br_dcycs = 0.01;
	reg14 = scc_ptr->reg[14];
	if(reg14 & 0x1) {
		br_dcycs = SCC_DCYCS_PER_XTAL;
		if(reg14 & 0x2) {
			br_dcycs = SCC_DCYCS_PER_PCLK;
		}
	}

	br_dcycs = br_dcycs * (double)br_const * 2.0;

	dpll_dcycs = 0.1;
	if(reg14 & SCC_R14_DPLL_SOURCE_BRG) {
		dpll_dcycs = br_dcycs;
	} else if(reg14 & SCC_R14_DPLL_SOURCE_RTXC) {
		dpll_dcycs = SCC_DCYCS_PER_XTAL;
	}

	tx_dcycs = 1;
	reg11 = scc_ptr->reg[11];
	switch((reg11 >> 3) & 3) {
	case 0:		// /RTxC pin
		tx_dcycs = SCC_DCYCS_PER_XTAL;
		break;
	case 2:		// BR generator output
		tx_dcycs = br_dcycs;
		break;
	case 3:		// DPLL output
		tx_dcycs = dpll_dcycs;
		break;
	}

	tx_dcycs = tx_dcycs * clock_mult;

	rx_dcycs = 1;
	switch((reg11 >> 5) & 3) {
	case 0:		// /RTxC pin
		rx_dcycs = SCC_DCYCS_PER_XTAL;
		break;
	case 2:		// BR generator output
		rx_dcycs = br_dcycs;
		break;
	case 3:		// DPLL output
		rx_dcycs = dpll_dcycs;
		break;
	}
	rx_dcycs = rx_dcycs * clock_mult;

	tx_char_size = 8.0;
	switch((scc_ptr->reg[5] >> 5) & 0x3) {
	case 0:	// 5 bits
		tx_char_size = 5.0;
		break;
	case 1:	// 7 bits
		tx_char_size = 7.0;
		break;
	case 2:	// 6 bits
		tx_char_size = 6.0;
		break;
	}

	scc_ptr->char_size = (int)tx_char_size;

	switch((scc_ptr->reg[4] >> 2) & 0x3) {
	case 1:	// 1 stop bit
		tx_char_size += 2.0;	// 1 stop + 1 start bit
		break;
	case 2:	// 1.5 stop bit
		tx_char_size += 2.5;	// 1.5 stop + 1 start bit
		break;
	case 3:	// 2 stop bits
		tx_char_size += 3.0;	// 2.0 stop + 1 start bit
		break;
	}

	if(scc_ptr->reg[4] & 1) {
		// parity enabled
		tx_char_size += 1.0;
	}

	if(scc_ptr->reg[14] & 0x10) {
		/* loopback mode, make it go faster...*/
		rx_char_size = 1.0;
		tx_char_size = 1.0;
	}

	rx_char_size = tx_char_size;	/* HACK */

	baud = (int)(DCYCS_1_MHZ / tx_dcycs);
	max_diff = 5000000;
	pos = 0;
	baud_entries = sizeof(g_baud_table)/sizeof(g_baud_table[0]);
	for(i = 0; i < baud_entries; i++) {
		diff = abs(g_baud_table[i] - baud);
		if(diff < max_diff) {
			pos = i;
			max_diff = diff;
		}
	}

	scc_ptr->baud_rate = g_baud_table[pos];

	scc_ptr->br_dcycs = br_dcycs;
	scc_ptr->tx_dcycs = tx_dcycs * tx_char_size;
	scc_ptr->rx_dcycs = rx_dcycs * rx_char_size;

	cur_state = scc_ptr->cur_state;
	if(cur_state == 0) {		// real serial ports
#ifdef _WIN32
		scc_serial_win_change_params(port);
#else
		scc_serial_unix_change_params(port);
#endif
	}
}

void
scc_port_close(int port)
{
	Scc	*scc_ptr;

	scc_ptr = &(g_scc[port]);

#ifdef _WIN32
	scc_serial_win_close(port);
#else
	scc_serial_unix_close(port);
#endif
	scc_socket_close(port);

	scc_ptr->cur_state = -1;		// Nothing open
}

void
scc_port_open(dword64 dfcyc, int port)
{
	int	cfg;

	cfg = g_serial_cfg[port];
	printf("scc_port_open port:%d cfg:%d\n", port, cfg);

	if(cfg == 0) {		// Real host serial port
#ifdef _WIN32
		scc_serial_win_open(port);
#else
		scc_serial_unix_open(port);
#endif
	} else if(cfg >= 1) {
		scc_socket_open(dfcyc, port, cfg);
	}
	printf(" open socketfd:%ld\n", (long)g_scc[port].sockfd);
}

int
scc_is_port_closed(dword64 dfcyc, int port)
{
	Scc	*scc_ptr;

	// Returns 1 is the port is closed (not working).  Returns 0
	//  if the port is open.  Tries to open the port if it is not in error
	scc_ptr = &(g_scc[port]);
	if(scc_ptr->cur_state == -1) {
		scc_port_open(dfcyc, port);
	}
	if(scc_ptr->cur_state < 0) {
		scc_ptr->cur_state = -2;
		//printf("scc_is_port_closed p:%d returning 0\n", port);
		return 1;		// Not open
	}
	return 0;			// Port is open!
}

char *
scc_get_serial_status(int get_status, int port)
{
	char	buf[80];
	char	*str;
	int	cur_state;

	if(get_status == 0) {
		return 0;
	}
	cur_state = g_scc[port].cur_state;
	str = "";
	switch(cur_state) {
	case -1:
		str = "Not initialized yet";
		break;
	case 0:
		snprintf(buf, 80, "Opened %s OK", g_serial_device[port]);
		str = buf;
		break;
	case 1:
		snprintf(buf, 80, "Virtual modem, sockfd:%d",
			(int)g_scc[port].sockfd);
		str = buf;
		break;
	case 2:
		snprintf(buf, 80, "Outgoing to %s:%d",
			g_serial_remote_ip[port], g_serial_remote_port[port]);
		str = buf;
		break;
	case 3:
		snprintf(buf, 80, "Opened %d, sockfd:%d", 6501 + port,
					(int)g_scc[port].sockfd);
		str = buf;
		break;
	default:
		str = "Open failed, port is closed";
	}
	return kegs_malloc_str(str);
}


void
scc_config_changed(int port, int cfg_changed, int remote_changed,
						int serial_dev_changed)
{
	Scc	*scc_ptr;
	int	must_change;

	// Check if scc_init() was called, if not get out
	if(!g_scc_init) {
		return;
	}

	// F4 may have changed the serial port config.  If so, close old
	//  port, open new one.

	scc_ptr = &(g_scc[port]);
	must_change = cfg_changed;
	switch(scc_ptr->cur_state) {
	case 0:		// Using serial port
		must_change |= serial_dev_changed;
		break;
	case 2:		// Using remote connection
		must_change |= remote_changed;
		break;
	}
	if(must_change) {
		scc_port_close(port);
	}
}

void
scc_update(dword64 dfcyc)
{
	int	i;

	// called each VBL update
	for(i = 0; i < 2; i++) {
		g_scc[i].write_called_this_vbl = 0;
		g_scc[i].read_called_this_vbl = 0;

		// These calls will try to open the port if it's closed
		scc_try_to_empty_writebuf(dfcyc, i);
		scc_try_fill_readbuf(dfcyc, i);

		g_scc[i].write_called_this_vbl = 0;
		g_scc[i].read_called_this_vbl = 0;
	}
}

void
scc_try_to_empty_writebuf(dword64 dfcyc, int port)
{
	Scc	*scc_ptr;
	int	cur_state;

	scc_ptr = &(g_scc[port]);
	cur_state = scc_ptr->cur_state;
	if(scc_ptr->write_called_this_vbl) {
		return;
	}
	if(scc_is_port_closed(dfcyc, port)) {
		return;				// Port is not open
	}

	scc_ptr->write_called_this_vbl = 1;

	if(cur_state == 0) {
#if defined(_WIN32)
		scc_serial_win_empty_writebuf(port);
#else
		scc_serial_unix_empty_writebuf(port);
#endif
	} else if(cur_state >= 1) {
		scc_socket_empty_writebuf(dfcyc, port);
	}
}

void
scc_try_fill_readbuf(dword64 dfcyc, int port)
{
	Scc	*scc_ptr;
	int	space_used, space_left, cur_state;

	scc_ptr = &(g_scc[port]);

	space_used = scc_ptr->in_wrptr - scc_ptr->in_rdptr;
	if(space_used < 0) {
		space_used += SCC_INBUF_SIZE;
	}
	space_left = (7*SCC_INBUF_SIZE/8) - space_used;
	if(space_left < 1) {
		/* Buffer is pretty full, don't try to get more */
		return;
	}

	if(scc_is_port_closed(dfcyc, port)) {
		return;				// Port is not open
	}

#if 0
	if(scc_ptr->read_called_this_vbl) {
		return;
	}
#endif

	scc_ptr->read_called_this_vbl = 1;

	cur_state = scc_ptr->cur_state;
	if(cur_state == 0) {
#if defined(_WIN32)
		scc_serial_win_fill_readbuf(dfcyc, port, space_left);
#else
		scc_serial_unix_fill_readbuf(dfcyc, port, space_left);
#endif
	} else if(cur_state >= 1) {
		scc_socket_fill_readbuf(dfcyc, port, space_left);
	}
}

void
scc_do_event(dword64 dfcyc, int type)
{
	Scc	*scc_ptr;
	int	port;

	port = type & 1;
	type = (type >> 1);

	scc_ptr = &(g_scc[port]);
	if(type == SCC_BR_EVENT) {
		/* baud rate generator counted down to 0 */
		scc_ptr->br_event_pending = 0;
		scc_set_zerocnt_int(port);
		scc_maybe_br_event(dfcyc, port);
	} else if(type == SCC_TX_EVENT) {
		scc_ptr->tx_event_pending = 0;
		scc_ptr->tx_buf_empty = 1;
		scc_handle_tx_event(port);
	} else if(type == SCC_RX_EVENT) {
		scc_ptr->rx_event_pending = 0;
		scc_maybe_rx_event(dfcyc, port);
		scc_maybe_rx_int(port);
	} else {
		halt_printf("scc_do_event: %08x!\n", type);
	}
	return;
}

void
show_scc_state()
{
	Scc	*scc_ptr;
	int	i, j;

	for(i = 0; i < 2; i++) {
		scc_ptr = &(g_scc[i]);
		printf("SCC port: %d\n", i);
		for(j = 0; j < 16; j += 4) {
			printf("Reg %2d-%2d: %02x %02x %02x %02x\n", j, j+3,
				scc_ptr->reg[j], scc_ptr->reg[j+1],
				scc_ptr->reg[j+2], scc_ptr->reg[j+3]);
		}
		printf("state: %d, sockfd:%llx rdwrfd:%llx, win_com:%p, "
			"win_dcb:%p\n", scc_ptr->cur_state,
			(dword64)scc_ptr->sockfd, (dword64)scc_ptr->rdwrfd,
			scc_ptr->win_com_handle, scc_ptr->win_dcb_ptr);
		printf("in_rdptr: %04x, in_wr:%04x, out_rd:%04x, out_wr:%04x\n",
			scc_ptr->in_rdptr, scc_ptr->in_wrptr,
			scc_ptr->out_rdptr, scc_ptr->out_wrptr);
		printf("rx_queue_depth: %d, queue: %02x, %02x, %02x, %02x\n",
			scc_ptr->rx_queue_depth, scc_ptr->rx_queue[0],
			scc_ptr->rx_queue[1], scc_ptr->rx_queue[2],
			scc_ptr->rx_queue[3]);
		printf("want_ints: rx:%d, tx:%d, zc:%d\n",
			scc_ptr->wantint_rx, scc_ptr->wantint_tx,
			scc_ptr->wantint_zerocnt);
		printf("ev_pendings: rx:%d, tx:%d, br:%d\n",
			scc_ptr->rx_event_pending,
			scc_ptr->tx_event_pending,
			scc_ptr->br_event_pending);
		printf("br_dcycs: %f, tx_dcycs: %f, rx_dcycs: %f\n",
			scc_ptr->br_dcycs, scc_ptr->tx_dcycs,
			scc_ptr->rx_dcycs);
		printf("char_size: %d, baud_rate: %d, mode: %d\n",
			scc_ptr->char_size, scc_ptr->baud_rate,
			scc_ptr->mode);
		printf("modem_state: %dtelnet_mode:%d iac:%d, "
			"modem_cmd_len:%d\n", scc_ptr->modem_state,
			scc_ptr->telnet_mode, scc_ptr->telnet_iac,
			scc_ptr->modem_cmd_len);
		printf("telnet_loc_modes:%08x %08x, telnet_rem_motes:"
			"%08x %08x\n", scc_ptr->telnet_local_mode[0],
			scc_ptr->telnet_local_mode[1],
			scc_ptr->telnet_remote_mode[0],
			scc_ptr->telnet_remote_mode[1]);
		printf("modem_mode:%08x plus_mode:%d, out_char_dfcyc:%016llx\n",
			scc_ptr->modem_mode, scc_ptr->modem_plus_mode,
			scc_ptr->out_char_dfcyc);
	}

}

word32
scc_read_reg(dword64 dfcyc, int port)
{
	Scc	*scc_ptr;
	word32	ret;
	int	regnum;

	scc_ptr = &(g_scc[port]);
	scc_ptr->mode = 0;
	regnum = scc_ptr->reg_ptr;

	/* port 0 is channel A, port 1 is channel B */
	switch(regnum) {
	case 0:
	case 4:
		ret = 0x60;	/* 0x44 = no dcd, no cts,0x6c = dcd ok, cts ok*/
		if(scc_ptr->dcd) {
			ret |= 0x08;
		}
		//ret |= 0x8;	/* HACK HACK */
		if(scc_ptr->rx_queue_depth) {
			ret |= 0x01;
		}
		if(scc_ptr->tx_buf_empty) {
			ret |= 0x04;
		}
		if(scc_ptr->br_is_zero) {
			ret |= 0x02;
		}
		//printf("Read scc[%d] stat: %016llx : %02x dcd:%d\n", port,
		//	dfcyc, ret, scc_ptr->dcd);
		break;
	case 1:
	case 5:
		/* HACK: residue codes not right */
		ret = 0x07;	/* all sent */
		break;
	case 2:
	case 6:
		if(port == 0) {
			ret = scc_ptr->reg[2];
		} else {			// Port B, read RR2B int stat
			// The TELNET.SYSTEM by Colin Leroy-Mira uses RR2B
			ret = scc_do_read_rr2b() << 1;
			if(g_scc[0].reg[9] & 0x10) {	// wr9 status high
				// Map bit 3->4, 2->5, 1->6
				ret = ((ret << 1) & 0x10) |
					((ret << 3) & 0x20) |
					((ret << 5) & 0x40);
			}
		}
		break;
	case 3:
	case 7:
		if(port == 0) {
			ret = (g_irq_pending & 0x3f);
		} else {
			ret = 0;
		}
		break;
	case 8:
		ret = scc_read_data(dfcyc, port);
		break;
	case 9:
	case 13:
		ret = scc_ptr->reg[13];
		break;
	case 10:
	case 14:
		ret = 0;
		break;
	case 11:
	case 15:
		ret = scc_ptr->reg[15];
		break;
	case 12:
		ret = scc_ptr->reg[12];
		break;
	default:
		halt_printf("Tried reading c03%x with regnum: %d!\n", 8+port,
			regnum);
		ret = 0;
	}

	scc_ptr->reg_ptr = 0;
	scc_printf("Read c03%x, rr%d, ret: %02x\n", 8+port, regnum, ret);
	dbg_log_info(dfcyc, 0, ret, (regnum << 20) | (0xc039 - port));

	return ret;
}

void
scc_write_reg(dword64 dfcyc, int port, word32 val)
{
	Scc	*scc_ptr;
	word32	old_val, changed_bits, irq_mask;
	int	regnum, mode, tmp1;

	scc_ptr = &(g_scc[port]);
	regnum = scc_ptr->reg_ptr & 0xf;
	mode = scc_ptr->mode;

	if(mode == 0) {
		if((val & 0xf0) == 0) {
			/* Set reg_ptr */
			scc_ptr->reg_ptr = val & 0xf;
			regnum = 0;
			scc_ptr->mode = 1;
		}
	} else {
		scc_ptr->reg_ptr = 0;
		scc_ptr->mode = 0;
	}
	old_val = scc_ptr->reg[regnum];
	changed_bits = (old_val ^ val) & 0xff;

	dbg_log_info(dfcyc, (mode << 16) | scc_ptr->reg_ptr,
			(changed_bits << 16) | val,
			(regnum << 20) | (0x1c039 - port));

	/* Set reg reg */
	switch(regnum) {
	case 0: /* wr0 */
		tmp1 = (val >> 3) & 0x7;
		switch(tmp1) {
		case 0x0:
		case 0x1:
			break;
		case 0x2:	/* reset ext/status ints */
			/* should clear other ext ints */
			scc_clr_zerocnt_int(port);
			break;
		case 0x5:	/* reset tx int pending */
			scc_clr_tx_int(port);
			break;
		case 0x6:	/* reset rr1 bits */
			break;
		case 0x7:	/* reset highest pri int pending */
			irq_mask = g_irq_pending;
			if(port == 0) {
				/* Move SCC0 ints into SCC1 positions */
				irq_mask = irq_mask >> 3;
			}
			if(irq_mask & IRQ_PENDING_SCC1_RX) {
				scc_clr_rx_int(port);
			} else if(irq_mask & IRQ_PENDING_SCC1_TX) {
				scc_clr_tx_int(port);
			} else if(irq_mask & IRQ_PENDING_SCC1_ZEROCNT) {
				scc_clr_zerocnt_int(port);
			}
			break;
		case 0x4:	/* enable int on next rx char */
		default:
			halt_printf("Wr c03%x to wr0 of %02x, bad cmd cd:%x!\n",
				9-port, val, tmp1);
		}
		tmp1 = (val >> 6) & 0x3;
		switch(tmp1) {
		case 0x0:	/* null code */
			break;
		case 0x1:	/* reset rx crc */
		case 0x2:	/* reset tx crc */
			printf("Wr c03%x to wr0 of %02x!\n", 9-port, val);
			break;
		case 0x3:	/* reset tx underrun/eom latch */
			/* if no extern status pending, or being reset now */
			/*  and tx disabled, ext int with tx underrun */
			/* ah, just do nothing */
			break;
		}
		return;
	case 1: /* wr1 */
		/* proterm sets this == 0x10, which is int on all rx */
		scc_ptr->reg[regnum] = val;
		return;
	case 2: /* wr2 */
		/* All values do nothing, let 'em all through! */
		scc_ptr->reg[regnum] = val;
		return;
	case 3: /* wr3 */
		if((val & 0x1e) != 0x0) {
			halt_printf("Wr c03%x to wr3 of %02x!\n", 8+port, val);
		}
		scc_ptr->reg[regnum] = val;
		return;
	case 4: /* wr4 */
		if((val & 0x30) != 0x00 || (val & 0x0c) == 0) {
			halt_printf("Wr c03%x to wr4 of %02x!\n", 8+port, val);
		}
		scc_ptr->reg[regnum] = val;
		if(changed_bits) {
			scc_regen_clocks(port);
		}
		return;
	case 5: /* wr5 */
		if((val & 0x15) != 0x0) {
			halt_printf("Wr c03%x to wr5 of %02x!\n", 8+port, val);
		}
		scc_ptr->reg[regnum] = val;
		if(changed_bits & 0x60) {
			scc_regen_clocks(port);
		}
		if(changed_bits & 0x80) {
			scc_printf("SCC port %d DTR:%d\n", port, val & 0x80);
		}
		return;
	case 6: /* wr6 */
		if(val != 0) {
			halt_printf("Wr c03%x to wr6 of %02x!\n", 8+port, val);
		}
		scc_ptr->reg[regnum] = val;
		return;
	case 7: /* wr7 */
		if(val != 0) {
			halt_printf("Wr c03%x to wr7 of %02x!\n", 8+port, val);
		}
		scc_ptr->reg[regnum] = val;
		return;
	case 8: /* wr8 */
		scc_write_data(dfcyc, port, val);
		return;
	case 9: /* wr9 */
		if((val & 0xc0)) {
			if(val & 0x80) {
				scc_reset_port(0);
			}
			if(val & 0x40) {
				scc_reset_port(1);
			}
			if((val & 0xc0) == 0xc0) {
				scc_hard_reset_port(0);
				scc_hard_reset_port(1);
			}
		}
		// Bit 5 is software interrupt ack, which does not exist on NMOS
		// Bit 2 sets IEO pin low, which doesn't exist either
		old_val = g_scc[0].reg[9];
		g_scc[0].reg[9] = val;
		scc_evaluate_ints(0);
		scc_evaluate_ints(1);
		return;
	case 10: /* wr10 */
		if((val & 0xff) != 0x00) {
			printf("Wr c03%x to wr10 of %02x!\n", 8+port, val);
		}
		scc_ptr->reg[regnum] = val;
		return;
	case 11: /* wr11 */
		scc_ptr->reg[regnum] = val;
		if(changed_bits) {
			scc_regen_clocks(port);
		}
		return;
	case 12: /* wr12 */
		scc_ptr->reg[regnum] = val;
		if(changed_bits) {
			scc_regen_clocks(port);
		}
		return;
	case 13: /* wr13 */
		scc_ptr->reg[regnum] = val;
		if(changed_bits) {
			scc_regen_clocks(port);
		}
		return;
	case 14: /* wr14 */
		val = val | (old_val & (~0xffU));
		switch((val >> 5) & 0x7) {
		case 0x0:	// Null command (change nothing)
			break;
		case 0x1:	// Enter search mode
		case 0x2:	// Reset missing clock
		case 0x3:	// Disable PLL
		case 0x6:	// Set FM mode
		case 0x7:	// Set NRZI mode
			// Disable the PLL effectively
			val = val & 0xff;	// Clear all upper bits
			break;
		case 0x4:	// DPLL source is BR gen
			val = (val & 0xff) | SCC_R14_DPLL_SOURCE_BRG;
			break;
		case 0x5:	// DPLL source is RTxC
			val = (val & 0xff) | SCC_R14_DPLL_SOURCE_RTXC;
			break;
		default:
			halt_printf("Wr c03%x to wr14 of %02x, bad dpll cd!\n",
				8+port, val);
		}
		if((val & 0x0c) != 0x0) {
			halt_printf("Wr c03%x to wr14 of %02x!\n", 8+port, val);
		}
		scc_ptr->reg[regnum] = val;
		if(changed_bits || (val != old_val)) {
			scc_regen_clocks(port);
		}
		scc_maybe_br_event(dfcyc, port);
		return;
	case 15: /* wr15 */
		/* ignore all accesses since IIgs self test messes with it */
		if((val & 0xff) != 0x0) {
			scc_printf("Write c03%x to wr15 of %02x!\n", 8+port,
				val);
		}
		if((g_scc[0].reg[9] & 0x8) && (val != 0)) {
			printf("Write wr15:%02x and master int en = 1!\n",val);
			/* set_halt(1); */
		}
		scc_ptr->reg[regnum] = val;
		scc_maybe_br_event(dfcyc, port);
		scc_evaluate_ints(port);
		return;
	default:
		halt_printf("Wr c03%x to wr%d of %02x!\n", 8+port, regnum, val);
		return;
	}
}

// scc_read_data: Read from 0xc03b or 0xc03a
word32
scc_read_data(dword64 dfcyc, int port)
{
	Scc	*scc_ptr;
	word32	ret;
	int	depth;
	int	i;

	scc_ptr = &(g_scc[port]);

	scc_try_fill_readbuf(dfcyc, port);

	depth = scc_ptr->rx_queue_depth;

	ret = 0;
	if(depth != 0) {
		ret = scc_ptr->rx_queue[0];
		for(i = 1; i < depth; i++) {
			scc_ptr->rx_queue[i-1] = scc_ptr->rx_queue[i];
		}
		scc_ptr->rx_queue_depth = depth - 1;
		scc_maybe_rx_event(dfcyc, port);
		scc_maybe_rx_int(port);
	}

	scc_printf("SCC read %04x: ret %02x, depth:%d\n", 0xc03b-port, ret,
			depth);

	dbg_log_info(dfcyc, 0, ret, 0xc03b - port);

	return ret;
}

void
scc_write_data(dword64 dfcyc, int port, word32 val)
{
	Scc	*scc_ptr;

	scc_printf("SCC write %04x: %02x\n", 0xc03b-port, val);
	dbg_log_info(dfcyc, val, 0, 0x1c03b - port);

	scc_ptr = &(g_scc[port]);
	if(scc_ptr->reg[14] & 0x10) {
		/* local loopback! */
		scc_add_to_readbuf(dfcyc, port, val);
	} else {
		scc_transmit(dfcyc, port, val);
	}
	scc_try_to_empty_writebuf(dfcyc, port);

	scc_maybe_tx_event(dfcyc, port);
}

word32
scc_do_read_rr2b()
{
	word32	val;

	val = g_irq_pending & 0x3f;
	if(val == 0) {
		return 3;	// 011 if no interrupts pending
	}
	// Do Channel A first.  Priority order from SCC documentation
	if(val & IRQ_PENDING_SCC0_RX) {
		return 6;	// 110 Ch A Rx char available
	}
	if(val & IRQ_PENDING_SCC0_TX) {
		return 4;	// 100 Ch A Tx buffer empty
	}
	if(val & IRQ_PENDING_SCC0_ZEROCNT) {
		return 5;	// 101 Ch A External/Status change
	}
	if(val & IRQ_PENDING_SCC1_RX) {
		return 2;	// 010 Ch B Rx char available
	}
	if(val & IRQ_PENDING_SCC1_TX) {
		return 0;	// 000 Ch B Tx buffer empty
	}
	if(val & IRQ_PENDING_SCC1_ZEROCNT) {
		return 1;	// 001 Ch B External/Status change
	}

	return 3;
}

void
scc_maybe_br_event(dword64 dfcyc, int port)
{
	Scc	*scc_ptr;
	double	br_dcycs;

	scc_ptr = &(g_scc[port]);

	if(((scc_ptr->reg[14] & 0x01) == 0) || scc_ptr->br_event_pending) {
		return;
	}
	/* also, if ext ints not enabled, don't do baud rate ints */
	if(((scc_ptr->reg[15] & 0x02) == 0) || ((scc_ptr->reg[9] & 8) == 0)) {
		return;
	}

	br_dcycs = scc_ptr->br_dcycs;
	if(br_dcycs < 1.0) {
		halt_printf("br_dcycs: %f!\n", br_dcycs);
#if 0
		printf("br_dcycs: %f!\n", br_dcycs);
		dbg_log_info(dfcyc, (word32)(br_dcycs * 65536),
			(scc_ptr->reg[15] << 24) | (scc_ptr->reg[14] << 16) |
			(scc_ptr->reg[13] << 8) | scc_ptr->reg[12], 0xdc1c);
		dbg_log_info(dfcyc,
			(scc_ptr->reg[11] << 24) | (scc_ptr->reg[10] << 16) |
			(scc_ptr->reg[9] << 8) | scc_ptr->reg[5],
			(scc_ptr->reg[4] << 24) | (scc_ptr->reg[3] << 16) |
			(scc_ptr->reg[1] << 8) | scc_ptr->reg[0], 0xdc1b);
#endif
	}

	scc_ptr->br_event_pending = 1;
	add_event_scc(dfcyc + (dword64)(br_dcycs * 65536.0),
					SCC_MAKE_EVENT(port, SCC_BR_EVENT));
}

void
scc_evaluate_ints(int port)
{
	Scc	*scc_ptr;
	word32	irq_add_mask, irq_remove_mask;
	int	mie;

	scc_ptr = &(g_scc[port]);
	mie = g_scc[0].reg[9] & 0x8;			/* Master int en */

	if(!mie) {
		/* There can be no interrupts if MIE=0 */
		remove_irq(IRQ_PENDING_SCC1_RX | IRQ_PENDING_SCC1_TX |
						IRQ_PENDING_SCC1_ZEROCNT |
			IRQ_PENDING_SCC0_RX | IRQ_PENDING_SCC0_TX |
						IRQ_PENDING_SCC0_ZEROCNT);
		return;
	}

	irq_add_mask = 0;
	irq_remove_mask = 0;
	if(scc_ptr->wantint_rx) {
		irq_add_mask |= IRQ_PENDING_SCC1_RX;
	} else {
		irq_remove_mask |= IRQ_PENDING_SCC1_RX;
	}
	if(scc_ptr->wantint_tx) {
		irq_add_mask |= IRQ_PENDING_SCC1_TX;
	} else {
		irq_remove_mask |= IRQ_PENDING_SCC1_TX;
	}
	if(scc_ptr->wantint_zerocnt) {
		irq_add_mask |= IRQ_PENDING_SCC1_ZEROCNT;
	} else {
		irq_remove_mask |= IRQ_PENDING_SCC1_ZEROCNT;
	}
	if(port == 0) {
		/* Port 1 is in bits 0-2 and port 0 is in bits 3-5 */
		irq_add_mask = irq_add_mask << 3;
		irq_remove_mask = irq_remove_mask << 3;
	}
	if(irq_add_mask) {
		add_irq(irq_add_mask);
	}
	if(irq_remove_mask) {
		remove_irq(irq_remove_mask);
	}
}

void
scc_maybe_rx_event(dword64 dfcyc, int port)
{
	Scc	*scc_ptr;
	double	rx_dcycs;
	int	in_rdptr, in_wrptr, depth;

	scc_ptr = &(g_scc[port]);

	if(scc_ptr->rx_event_pending) {
		/* one pending already, wait for the event to arrive */
		return;
	}

	in_rdptr = scc_ptr->in_rdptr;
	in_wrptr = scc_ptr->in_wrptr;
	depth = scc_ptr->rx_queue_depth;
	if((in_rdptr == in_wrptr) || (depth >= 3)) {
		/* no more chars or no more space, just get out */
		return;
	}

	if(depth < 0) {
		depth = 0;
	}

	/* pull char from in_rdptr into queue */
	scc_ptr->rx_queue[depth] = scc_ptr->in_buf[in_rdptr];
	scc_ptr->in_rdptr = (in_rdptr + 1) & (SCC_INBUF_SIZE - 1);
	scc_ptr->rx_queue_depth = depth + 1;
	scc_maybe_rx_int(port);
	rx_dcycs = scc_ptr->rx_dcycs;
	scc_ptr->rx_event_pending = 1;
	add_event_scc(dfcyc + (dword64)(rx_dcycs*65536.0),
					SCC_MAKE_EVENT(port, SCC_RX_EVENT));
}

void
scc_maybe_rx_int(int port)
{
	Scc	*scc_ptr;
	int	depth;
	int	rx_int_mode;

	scc_ptr = &(g_scc[port]);

	depth = scc_ptr->rx_queue_depth;
	if(depth <= 0) {
		/* no more chars, just get out */
		scc_clr_rx_int(port);
		return;
	}
	rx_int_mode = (scc_ptr->reg[1] >> 3) & 0x3;
	if((rx_int_mode == 1) || (rx_int_mode == 2)) {
		scc_ptr->wantint_rx = 1;
	}
	scc_evaluate_ints(port);
}

void
scc_clr_rx_int(int port)
{
	g_scc[port].wantint_rx = 0;
	scc_evaluate_ints(port);
}

void
scc_handle_tx_event(int port)
{
	Scc	*scc_ptr;
	int	tx_int_mode;

	scc_ptr = &(g_scc[port]);

	/* nothing pending, see if ints on */
	tx_int_mode = (scc_ptr->reg[1] & 0x2);
	if(tx_int_mode) {
		scc_ptr->wantint_tx = 1;
	}
	scc_evaluate_ints(port);
}

void
scc_maybe_tx_event(dword64 dfcyc, int port)
{
	Scc	*scc_ptr;
	double	tx_dcycs;

	scc_ptr = &(g_scc[port]);

	if(scc_ptr->tx_event_pending) {
		/* one pending already, tx_buf is full */
		scc_ptr->tx_buf_empty = 0;
	} else {
		/* nothing pending, see ints on */
		scc_ptr->tx_buf_empty = 0;
		scc_evaluate_ints(port);
		tx_dcycs = scc_ptr->tx_dcycs;
		scc_ptr->tx_event_pending = 1;
		add_event_scc(dfcyc + (dword64)(tx_dcycs * 65536.0),
				SCC_MAKE_EVENT(port, SCC_TX_EVENT));
	}
}

void
scc_clr_tx_int(int port)
{
	g_scc[port].wantint_tx = 0;
	scc_evaluate_ints(port);
}

void
scc_set_zerocnt_int(int port)
{
	Scc	*scc_ptr;

	scc_ptr = &(g_scc[port]);

	if(scc_ptr->reg[15] & 0x2) {
		scc_ptr->wantint_zerocnt = 1;
	}
	scc_evaluate_ints(port);
}

void
scc_clr_zerocnt_int(int port)
{
	g_scc[port].wantint_zerocnt = 0;
	scc_evaluate_ints(port);
}

void
scc_add_to_readbuf(dword64 dfcyc, int port, word32 val)
{
	Scc	*scc_ptr;
	int	in_wrptr, in_wrptr_next, in_rdptr, safe_val;

	scc_ptr = &(g_scc[port]);

	if((scc_ptr->reg[5] & 0x60) != 0x60) {	// HACK: this is tx char size!
		val = val & 0x7f;
	}
	in_wrptr = scc_ptr->in_wrptr;
	in_rdptr = scc_ptr->in_rdptr;
	in_wrptr_next = (in_wrptr + 1) & (SCC_INBUF_SIZE - 1);
	if(in_wrptr_next != in_rdptr) {
		scc_ptr->in_buf[in_wrptr] = val;
		scc_ptr->in_wrptr = in_wrptr_next;
		safe_val = val & 0x7f;
		if((safe_val < 0x20) || (safe_val >= 0x7f)) {
			safe_val = '.';
		}
		scc_printf("scc in port[%d] add 0x%02x (%c), %d,%d != %d\n",
			port, val, safe_val, in_wrptr, in_wrptr_next, in_rdptr);
		g_scc_overflow = 0;
	} else {
		if(g_scc_overflow == 0) {
			g_code_yellow++;
			printf("scc inbuf overflow port %d\n", port);
		}
		g_scc_overflow = 1;
	}

	scc_maybe_rx_event(dfcyc, port);
	scc_maybe_rx_int(port);
}

void
scc_add_to_readbufv(dword64 dfcyc, int port, const char *fmt, ...)
{
	va_list	ap;
	char	*bufptr;
	int	len, c;
	int	i;

	va_start(ap, fmt);
	bufptr = malloc(4096);
	bufptr[0] = 0;
	vsnprintf(bufptr, 4090, fmt, ap);
	len = (int)strlen(bufptr);
	for(i = 0; i < len; i++) {
		c = bufptr[i];
		if(c == 0x0a) {
			scc_add_to_readbuf(dfcyc, port, 0x0d);
		}
		scc_add_to_readbuf(dfcyc, port, c);
	}
	va_end(ap);
}

void
scc_transmit(dword64 dfcyc, int port, word32 val)
{
	Scc	*scc_ptr;
	int	out_wrptr, out_rdptr;

	scc_ptr = &(g_scc[port]);

	// printf("scc_transmit port:%d val:%02x \n", port, val);
	/* See if port initialized, if not, do so now */
	if(scc_is_port_closed(dfcyc, port)) {
		printf("  port %d is closed, cur_state:%d\n", port,
				scc_ptr->cur_state);
		return;		// No working serial port, just toss it and go
	}
	if(!scc_ptr->tx_buf_empty) {
		/* toss character! */
		printf("Tossing char\n");
		return;
	}

	out_wrptr = scc_ptr->out_wrptr;
	out_rdptr = scc_ptr->out_rdptr;
	if(scc_ptr->tx_dcycs < 1.0) {
		if(out_wrptr != out_rdptr) {
			/* do just one char, then get out */
			printf("tx_dcycs < 1\n");
			return;
		}
	}
	if(g_serial_mask[port] || (scc_ptr->reg[5] & 0x60) != 0x60) {
		val = val & 0x7f;
	}

	scc_add_to_writebuf(dfcyc, port, val);
}

void
scc_add_to_writebuf(dword64 dfcyc, int port, word32 val)
{
	Scc	*scc_ptr;
	int	out_wrptr, out_wrptr_next, out_rdptr;

	// printf("scc_add_to_writebuf p:%d, val:%02x\n", port, val);
	if(scc_is_port_closed(dfcyc, port)) {
		return;			// Port is closed
	}
	scc_ptr = &(g_scc[port]);

	out_wrptr = scc_ptr->out_wrptr;
	out_rdptr = scc_ptr->out_rdptr;

	out_wrptr_next = (out_wrptr + 1) & (SCC_OUTBUF_SIZE - 1);
	if(out_wrptr_next != out_rdptr) {
		scc_ptr->out_buf[out_wrptr] = val;
		scc_ptr->out_wrptr = out_wrptr_next;
		scc_printf("scc wrbuf port %d had char 0x%02x added\n",
			port, val);
		g_scc_overflow = 0;
	} else {
		if(g_scc_overflow == 0) {
			g_code_yellow++;
			printf("scc outbuf overflow port %d\n", port);
		}
		g_scc_overflow = 1;
	}
}

