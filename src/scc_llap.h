/*
  GSPLUS - Advanced Apple IIGS Emulator Environment
  Based on the KEGS emulator written by Kent Dickey
  See COPYRIGHT.txt for Copyright information
	See COPYING.txt for license (GPL v2)
*/

void scc_llap_init();
void scc_llap_shutdown();
void scc_llap_update();
void scc_llap_fill_readbuf(int port, int space_left, double dcycs);
void scc_llap_empty_writebuf(int port, double dcycs);
void scc_llap_set_node(byte val);
