const char rcsid_scc_macdriver_c[] = "@(#)$KmKId: scc_unixdriver.c,v 1.4 2025-01-07 16:45:35+00 kentd Exp $";

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

/* This file contains the Mac/Linux calls to a real serial port */

#include "defc.h"
#include "scc.h"

#ifndef _WIN32
# include <termios.h>
#endif

extern Scc g_scc[2];
extern word32 g_c025_val;
extern char *g_serial_device[2];

#ifndef _WIN32
void
scc_serial_unix_open(int port)
{
	Scc	*scc_ptr;
	int	fd;

	scc_ptr = &(g_scc[port]);

	fd = open(&g_serial_device[port][0], O_RDWR | O_NONBLOCK);
	scc_ptr->unix_dev_fd = fd;

	printf("scc_serial_unix_init %d called, fd: %d\n", port, fd);

	if(fd < 0) {
		scc_ptr->unix_dev_fd = -1;
		scc_ptr->cur_state = -1;	// Failed to open
		return;
	}

	scc_serial_unix_change_params(port);

	scc_ptr->cur_state = 0;		// Actual Serial device
}

void
scc_serial_unix_close(int port)
{
	Scc	*scc_ptr;
	int	fd;

	scc_ptr = &(g_scc[port]);

	fd = scc_ptr->unix_dev_fd;
	if(fd >= 0) {
		close(fd);
	}
	scc_ptr->unix_dev_fd = -1;
}

void
scc_serial_unix_change_params(int port)
{
	struct termios termios_buf;
	Scc	*scc_ptr;
	int	fd, csz, ret;

	scc_ptr = &(g_scc[port]);

	fd = scc_ptr->unix_dev_fd;
	printf("scc_serial_unix_change_parms port: %d, fd: %d\n", port, fd);
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
#ifdef CRTSCTS
	termios_buf.c_cflag |= CRTSCTS;			// Linux: CTS/RTS
#endif
#ifdef CRTS_IFLOW
	termios_buf.c_cflag |= CDTR_IFLOW | CRTS_IFLOW;		// Mac: CTS/RTS
#endif

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
scc_serial_unix_fill_readbuf(dword64 dfcyc, int port, int space_left)
{
	byte	tmp_buf[256];
	Scc	*scc_ptr;
	int	fd, ret, flags, dcd;
	int	i;

	scc_ptr = &(g_scc[port]);

	fd = scc_ptr->unix_dev_fd;
	if(fd <= 0) {
		return;
	}

	/* Try reading some bytes */
	space_left = MY_MIN(space_left, 256);
	ret = read(fd, tmp_buf, space_left);

	if(ret > 0) {
		for(i = 0; i < ret; i++) {
			scc_add_to_readbuf(dfcyc, port, tmp_buf[i]);
		}
	}
	flags = 0;
	dcd = 0;

#if defined(TIOCMGET) && defined(TIOCM_CAR)
	ret = ioctl(fd, TIOCMGET, &flags);
	if(ret == 0) {
		dcd = 0;
		if(flags & TIOCM_CAR) {		// DCD
			dcd = 1;
		}
		scc_ptr->dcd = dcd;
	}
#endif
}

void
scc_serial_unix_empty_writebuf(int port)
{
	Scc	*scc_ptr;
	int	fd, rdptr, wrptr, done, ret, len;

	scc_ptr = &(g_scc[port]);

	fd = scc_ptr->unix_dev_fd;
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
#endif	/* !_WIN32 */
