#ifdef INCLUDE_RCSID_C
const char rcsid_scc_h[] = "@(#)$KmKId: scc.h,v 1.27 2025-01-11 18:45:06+00 kentd Exp $";
#endif

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

#include <ctype.h>

#ifdef _WIN32
# include <winsock2.h>
#else
# include <sys/socket.h>
# include <netinet/in.h>
# include <netdb.h>
#endif

#if defined(HPUX) || defined(__linux__) || defined(SOLARIS)
# define SCC_SOCKETS
#endif
#if defined(MAC) || defined(__MACH__) || defined(_WIN32)
# define SCC_SOCKETS
#endif


/* my scc port 0 == channel A, port 1 = channel B */

#define	SCC_INBUF_SIZE		512		/* must be a power of 2 */
#define	SCC_OUTBUF_SIZE		512		/* must be a power of 2 */

#define SCC_MODEM_MAX_CMD_STR	128

#ifndef _WIN32
# define SOCKET			int		/* For non-Windows */
# define INVALID_SOCKET		(-1)		/* For non-Windows */
#endif

STRUCT(Scc) {
	int	cur_state;
	int	modem_state;
	SOCKET	sockfd;
	SOCKET	rdwrfd;
	void	*sockaddr_ptr;		// Socket: pointer to sockaddr struct
	int	sockaddr_size;		// Socket: sizeof(sockaddr_in)
	int	unix_dev_fd;		// Unix fd to real serial device
	void	*win_com_handle;	// Win32 handle to COMx port
	void	*win_dcb_ptr;		// Win32 ptr to COMx DCB
	int	read_called_this_vbl;
	int	write_called_this_vbl;

	byte	dcd;
	byte	reg_ptr;
	byte	br_is_zero;
	byte	tx_buf_empty;
	word32	mode;
	byte	reg[16];

	int	rx_queue_depth;
	byte	rx_queue[4];

	int	in_rdptr;
	int	in_wrptr;
	byte	in_buf[SCC_INBUF_SIZE];

	int	out_rdptr;
	int	out_wrptr;
	byte	out_buf[SCC_OUTBUF_SIZE];

	int	wantint_rx;
	int	wantint_tx;
	int	wantint_zerocnt;

	double	br_dcycs;
	double	tx_dcycs;
	double	rx_dcycs;

	int	br_event_pending;
	int	rx_event_pending;
	int	tx_event_pending;

	int	char_size;
	int	baud_rate;
	dword64	out_char_dfcyc;

	int	socket_error;
	int	socket_num_rings;
	dword64	socket_last_ring_dfcyc;
	word32	modem_mode;
	int	modem_plus_mode;
	int	modem_s0_val;
	int	modem_s2_val;
	int	telnet_mode;
	int	telnet_iac;
	word32	telnet_local_mode[2];
	word32	telnet_remote_mode[2];
	word32	telnet_reqwill_mode[2];
	word32	telnet_reqdo_mode[2];
	word32	modem_out_portnum;
	int	modem_cmd_len;
	byte	modem_cmd_str[SCC_MODEM_MAX_CMD_STR + 5];
};

#define SCCMODEM_NOECHO		0x0001
#define SCCMODEM_NOVERBOSE	0x0002

