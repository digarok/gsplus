/*
  GSPLUS - Advanced Apple IIGS Emulator Environment
  Based on the KEGS emulator written by Kent Dickey
  See COPYRIGHT.txt for Copyright information
	See LICENSE.txt for license (GPL v2)
*/

struct at_addr_t;
struct aarp_header_t;
struct ether_addr_t;

void aarp_init();
void aarp_shutdown();

const struct ether_addr_t* aarp_request_hardware(const struct at_addr_t* protocol);
const struct at_addr_t* aarp_request_protocol(const struct ether_addr_t* hardware);

bool aarp_retry();
void aarp_retry_reset();

void aarp_glean(const struct at_addr_t* protocol, const struct ether_addr_t* hardware);

bool aarp_address_used(const struct at_addr_t* protocol);

void aarp_handle_packet(const struct aarp_header_t* aarp);
