/*
 GSPLUS - Advanced Apple IIGS Emulator Environment
 Copyright (C) 2016 - Dagen Brock
 
 Copyright (C) 2010 by GSport contributors
 
 Based on the KEGS emulator written by and Copyright (C) 2003 Kent Dickey

 This program is free software; you can redistribute it and/or modify it 
 under the terms of the GNU General Public License as published by the 
 Free Software Foundation; either version 2 of the License, or (at your 
 option) any later version.

 This program is distributed in the hope that it will be useful, but 
 WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
 or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License 
 for more details.

 You should have received a copy of the GNU General Public License along 
 with this program; if not, write to the Free Software Foundation, Inc., 
 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

/* This file contains the Mac serial calls */

#include "defc.h"
#include "scc.h"

#ifndef _WIN32
# include <termios.h>
#endif

extern Scc scc_stat[2];
extern word32 g_c025_val;

#ifdef MAC
int
scc_serial_mac_init(int port)
{
	char	str_buf[1024];
	Scc	*scc_ptr;
	int	state;
	int	fd;

	scc_ptr = &(scc_stat[port]);

	scc_ptr->state = 0;		/* mark as uninitialized */

	/*sprintf(&str_buf[0], "/dev/tty.USA19QW11P1.1"); */
	sprintf(&str_buf[0], "/dev/tty.USA19H181P1.1");
	/* HACK: fix this... */

	fd = open(&str_buf[0], O_RDWR | O_NONBLOCK);

	scc_ptr->host_handle = (void *)fd;
	scc_ptr->host_handle2 = 0;

	printf("scc_serial_mac_init %d called, fd: %d\n", port, fd);

	if(fd < 0) {
		scc_ptr->host_handle = (void *)-1;
		return -1;
	}

	scc_serial_mac_change_params(port);

	state = 2;		/* raw serial */
	scc_ptr->state = state;

	return state;
}

void
scc_serial_mac_change_params(int port)
{
	struct termios termios_buf;
	Scc	*scc_ptr;
	int	fd;
	int	csz;
	int	ret;

	scc_ptr = &(scc_stat[port]);

	fd = (intptr_t)scc_ptr->host_handle;
	printf("scc_serial_mac_change_parms port: %d, fd: %d\n", port, fd);
	if(fd <= 0) {
		return;
	}

	ret = tcgetattr(fd, &termios_buf);
	if(ret != 0) {
		printf("tcgetattr port%d ret: %d\n", port, ret);
	}

#if 1
	printf("baudrate: %d, iflag:%x, oflag:%x, cflag:%x, lflag:%x\n",
		(int)termios_buf.c_ispeed, (int)termios_buf.c_iflag,
		(int)termios_buf.c_oflag, (int)termios_buf.c_cflag,
		(int)termios_buf.c_lflag);
#endif

	memset(&termios_buf, 0, sizeof(struct termios));
	cfmakeraw(&termios_buf);
	cfsetspeed(&termios_buf, scc_ptr->baud_rate);

	csz = scc_ptr->char_size;
	termios_buf.c_cflag = CREAD | CLOCAL;
	termios_buf.c_cflag |= (csz == 5) ? CS5 :
				(csz == 6) ? CS6 :
				(csz == 7) ? CS7 :
					CS8;
	switch((scc_ptr->reg[4] >> 2) & 0x3) {
	case 2: // 1.5 stop bits
		termios_buf.c_cflag |= CSTOPB;	/* no 1.5 stop bit setting.*/
		break;
	case 3: // 2 stop bits
		termios_buf.c_cflag |= CSTOPB;
		break;
	}

	switch((scc_ptr->reg[4]) & 0x3) {
	case 1:	// Odd parity
		termios_buf.c_cflag |= (PARENB | PARODD);
		break;
	case 3:	// Even parity
		termios_buf.c_cflag |= PARENB;
		break;
	}

	/* always enabled DTR and RTS control */
	termios_buf.c_cflag |= CDTR_IFLOW | CRTS_IFLOW;

	printf("fd: %d, baudrate: %d, iflag:%x, oflag:%x, cflag:%x, lflag:%x\n",
		fd, (int)termios_buf.c_ispeed, (int)termios_buf.c_iflag,
		(int)termios_buf.c_oflag, (int)termios_buf.c_cflag,
		(int)termios_buf.c_lflag);
	ret = tcsetattr(fd, TCSANOW, &termios_buf);
	if(ret != 0) {
		printf("tcsetattr ret: %d\n", ret);
	}
}

void
scc_serial_mac_fill_readbuf(int port, int space_left, double dcycs)
{
	byte	tmp_buf[256];
	Scc	*scc_ptr;
	int	fd;
	int	i;
	int	ret;

	scc_ptr = &(scc_stat[port]);

	fd = (intptr_t)scc_ptr->host_handle;
	if(fd <= 0) {
		return;
	}

	/* Try reading some bytes */
	space_left = MIN(space_left, 256);
	ret = read(fd, tmp_buf, space_left);

	if(ret > 0) {
		for(i = 0; i < ret; i++) {
			scc_add_to_readbuf(port, tmp_buf[i], dcycs);
		}
	}
	
}

void
scc_serial_mac_empty_writebuf(int port)
{
	Scc	*scc_ptr;
	int	fd;
	int	rdptr;
	int	wrptr;
	int	done;
	int	ret;
	int	len;

	scc_ptr = &(scc_stat[port]);

	fd = (intptr_t)scc_ptr->host_handle;
	if(fd <= 0) {
		return;
	}

	/* Try writing some bytes */
	done = 0;
	while(!done) {
		rdptr = scc_ptr->out_rdptr;
		wrptr = scc_ptr->out_wrptr;
		if(rdptr == wrptr) {
			//printf("...rdptr == wrptr\n");
			done = 1;
			break;
		}
		len = wrptr - rdptr;
		if(len < 0) {
			len = SCC_OUTBUF_SIZE - rdptr;
		}
		if(len > 32) {
			len = 32;
		}
		if(len <= 0) {
			done = 1;
			break;
		}
		ret = write(fd, &(scc_ptr->out_buf[rdptr]), len);

		if(ret <= 0) {
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
}
#endif	/* MAC */
