const char rcsid_scc_windriver_c[] = "@(#)$KmKId: scc_windriver.c,v 1.13 2023-08-28 18:11:05+00 kentd Exp $";

/************************************************************************/
/*			KEGS: Apple //gs Emulator			*/
/*			Copyright 2002-2023 by Kent Dickey		*/
/*									*/
/*	This code is covered by the GNU GPL v3				*/
/*	See the file COPYING.txt or https://www.gnu.org/licenses/	*/
/*	This program is provided with no warranty			*/
/*									*/
/*	The KEGS web page is kegs.sourceforge.net			*/
/*	You may contact the author at: kadickey@alumni.princeton.edu	*/
/************************************************************************/

/* This file contains the Win32 COM1/COM2 calls */

#include "defc.h"
#include "scc.h"

extern Scc g_scc[2];
extern word32 g_c025_val;
extern int g_serial_win_device[2];

#ifdef _WIN32
void
scc_serial_win_open(int port)
{
	COMMTIMEOUTS commtimeouts;
	char	str_buf[32];
	Scc	*scc_ptr;
	HANDLE	com_handle;
	int	ret;

	scc_ptr = &(g_scc[port]);

	snprintf(&str_buf[0], sizeof(str_buf), "COM%d",
						g_serial_win_device[port]);

	com_handle = CreateFile(&str_buf[0], GENERIC_READ | GENERIC_WRITE,
			0, NULL, OPEN_EXISTING, 0, NULL);

	scc_ptr->win_com_handle = com_handle;

	printf("scc_serial_win_init %d called, com_handle: %p\n", port,
								com_handle);

	if(com_handle == INVALID_HANDLE_VALUE) {
		scc_ptr->cur_state = -1;		// Failed to open
		return;
	}
	scc_ptr->win_dcb_ptr = malloc(sizeof(DCB));

	scc_serial_win_change_params(port);

	commtimeouts.ReadIntervalTimeout = MAXDWORD;
	commtimeouts.ReadTotalTimeoutMultiplier = 0;
	commtimeouts.ReadTotalTimeoutConstant = 0;
	commtimeouts.WriteTotalTimeoutMultiplier = 0;
	commtimeouts.WriteTotalTimeoutConstant = 10;
	ret = SetCommTimeouts(com_handle, &commtimeouts);
	if(ret == 0) {
		printf("setcommtimeout ret: %d\n", ret);
	}
	scc_ptr->cur_state = 0;		// COM* is open
}

void
scc_serial_win_close(int port)
{
	Scc	*scc_ptr;
	HANDLE	com_handle;

	scc_ptr = &(g_scc[port]);
	com_handle = scc_ptr->win_com_handle;
	scc_ptr->win_com_handle = INVALID_HANDLE_VALUE;
	if(com_handle == INVALID_HANDLE_VALUE) {
		return;
	}
	CloseHandle(com_handle);
	free(scc_ptr->win_dcb_ptr);
	scc_ptr->win_dcb_ptr = 0;
}

void
scc_serial_win_change_params(int port)
{
	DCB	*dcbptr;
	HANDLE	com_handle;
	Scc	*scc_ptr;
	int	ret;

	scc_ptr = &(g_scc[port]);

	com_handle = scc_ptr->win_com_handle;
	dcbptr = scc_ptr->win_dcb_ptr;
	if((com_handle == INVALID_HANDLE_VALUE) || (scc_ptr->cur_state != 0)) {
		return;
	}

	ret = GetCommState(com_handle, dcbptr);
	if(ret == 0) {
		printf("getcomm port%d ret: %d\n", port, ret);
	}

#if 1
	printf("dcb.baudrate: %d, bytesize:%d, stops:%d, parity:%d\n",
		(int)dcbptr->BaudRate, (int)dcbptr->ByteSize,
		(int)dcbptr->StopBits, (int)dcbptr->Parity);
	printf("dcb.binary: %d, ctsflow: %d, dsrflow: %d, dtr: %d, dsr: %d\n",
		(int)dcbptr->fBinary,
		(int)dcbptr->fOutxCtsFlow,
		(int)dcbptr->fOutxDsrFlow,
		(int)dcbptr->fDtrControl,
		(int)dcbptr->fDsrSensitivity);
	printf("dcb.txonxoff:%d, outx:%d, inx: %d, null: %d, rts: %d\n",
		(int)dcbptr->fTXContinueOnXoff,
		(int)dcbptr->fOutX,
		(int)dcbptr->fInX,
		(int)dcbptr->fNull,
		(int)dcbptr->fRtsControl);
	printf("dcb.fAbortOnErr:%d, fParity:%d\n", (int)dcbptr->fAbortOnError,
		(int)dcbptr->fParity);
#endif

	dcbptr->fAbortOnError = 0;

	dcbptr->BaudRate = scc_ptr->baud_rate;
	dcbptr->ByteSize = scc_ptr->char_size;
	dcbptr->StopBits = ONESTOPBIT;
	switch((scc_ptr->reg[4] >> 2) & 0x3) {
	case 2: // 1.5 stop bits
		dcbptr->StopBits = ONE5STOPBITS;
		break;
	case 3: // 2 stop bits
		dcbptr->StopBits = TWOSTOPBITS;
		break;
	}

	dcbptr->Parity = NOPARITY;
	switch((scc_ptr->reg[4]) & 0x3) {
	case 1:	// Odd parity
		dcbptr->Parity = ODDPARITY;
		break;
	case 3:	// Even parity
		dcbptr->Parity = EVENPARITY;
		break;
	}

	dcbptr->fNull = 0;
	dcbptr->fDtrControl = DTR_CONTROL_ENABLE;
	dcbptr->fDsrSensitivity = 0;
	dcbptr->fOutxCtsFlow = 0;
	dcbptr->fOutxDsrFlow = 0;
	dcbptr->fParity = 0;
	dcbptr->fInX = 0;
	dcbptr->fOutX = 0;
	dcbptr->fRtsControl = RTS_CONTROL_ENABLE;

	ret = SetCommState(com_handle, dcbptr);
	if(ret == 0) {
		printf("SetCommState ret: %d, new baud: %d\n", ret,
			(int)dcbptr->BaudRate);
	}
}

void
scc_serial_win_fill_readbuf(dword64 dfcyc, int port, int space_left)
{
	byte	tmp_buf[256];
	Scc	*scc_ptr;
	HANDLE	com_handle;
	DWORD	bytes_read;
	int	ret;
	int	i;

	scc_ptr = &(g_scc[port]);

	com_handle = scc_ptr->win_com_handle;
	if(com_handle == INVALID_HANDLE_VALUE) {
		return;
	}

	/* Try reading some bytes */
	space_left = MY_MIN(256, space_left);
	ret = ReadFile(com_handle, tmp_buf, space_left, &bytes_read, NULL);

	if(ret == 0) {
		printf("ReadFile ret 0\n");
	}

	if(ret && (bytes_read > 0)) {
		for(i = 0; i < (int)bytes_read; i++) {
			scc_add_to_readbuf(dfcyc, port, tmp_buf[i]);
		}
	}
}

void
scc_serial_win_empty_writebuf(int port)
{
	Scc	*scc_ptr;
	HANDLE	com_handle;
	DWORD	bytes_written;
	word32	err_code;
	int	rdptr, wrptr, done, ret, len;

	scc_ptr = &(g_scc[port]);

	//printf("win_empty_writebuf, com_h: %d\n", scc_ptr->win_com_handle);
	com_handle = scc_ptr->win_com_handle;
	if(com_handle == INVALID_HANDLE_VALUE) {
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
		bytes_written = 1;
		ret = WriteFile(com_handle, &(scc_ptr->out_buf[rdptr]), len,
				&bytes_written, NULL);
		printf("WriteFile ret: %d, bytes_written:%d, len:%d\n", ret,
			(int)bytes_written, len);

		err_code = (word32)-1;
		if(ret == 0) {
			err_code = (word32)GetLastError();
			printf("WriteFile ret:0, err_code: %08x\n", err_code);
		}

		if(ret == 0 || (bytes_written == 0)) {
			done = 1;
			break;
		} else {
			rdptr = rdptr + bytes_written;
			if(rdptr >= SCC_OUTBUF_SIZE) {
				rdptr = rdptr - SCC_OUTBUF_SIZE;
			}
			scc_ptr->out_rdptr = rdptr;
		}
	}
}

#endif
