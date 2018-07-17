/*
  GSPLUS - Advanced Apple IIGS Emulator Environment
  Based on the KEGS emulator written by Kent Dickey
  See COPYRIGHT.txt for Copyright information
	See LICENSE.txt for license (GPL v2)
*/

/*
parallel.c

This file handles the Apple II Parallel Card emulation in slot 1. Its very 
basic, but allows for future support of redirecting the output to a real
parallel port, files, and additional types of emulated printers.
*/

#include "defc.h"
#include "printer.h"
extern int g_parallel_out_masking;
extern word32 g_vbl_count;
extern int g_printer_timeout;
word32 printer_vbl_count = 0;
int port_block = 0;

byte parallel_read(word16 io_address)
{
	//printf("parallel card status called at %x\n", io_address);
	//since we only have a virtual printer, always return state as "Ready"
	return 0xff;
}
void parallel_write(word16 io_address, byte val)
{	
	//Mask MSB if user has it set.
	if(g_parallel_out_masking) {
		val = val & 0x7f;
	}
	//printf("parallel card called at %x\n", io_address);
	//send a byte to the virtual printer
	//By default all output to $C090 gets sent to the printer
	if (io_address == 0x00)
	{
		port_block = 1;
		printer_loop(val);
		printer_vbl_count = g_vbl_count+(g_printer_timeout*60);
		port_block = 0;
	}
	return;
}

//This function handles the automatic timeout of the virtual printer if an
//application doesn't send a form feed at the end of the page. It also
//allows multipage mode Postscript and native printer documents to
//print somewhat how a regular application would.
void printer_update()
{
	if (port_block != 1 && printer_vbl_count != 0 && g_vbl_count >= printer_vbl_count)
	{
		printf("Calling printer_update and flushing!\n");
		printer_feed();
		printer_vbl_count = 0;
	}
	return;
}
