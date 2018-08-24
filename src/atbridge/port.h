/*
  GSPLUS - Advanced Apple IIGS Emulator Environment
  Based on the KEGS emulator written by Kent Dickey
  See COPYRIGHT.txt for Copyright information
	See LICENSE.txt for license (GPL v2)
*/

struct packet_t
{
	struct at_addr_t dest;
	struct at_addr_t source;
	byte type;
	size_t size;
	byte* data;
	struct packet_t* next;
};

struct packet_queue_t
{
	struct packet_t* head;
	struct packet_t* tail;
};

void queue_init(struct packet_queue_t* queue);
void queue_shutdown(struct packet_queue_t* queue);

void enqueue(struct packet_queue_t* queue, struct at_addr_t dest, struct at_addr_t source, byte type, size_t size, byte data[]);
void enqueue_packet(struct packet_queue_t* queue, struct packet_t* packet);

struct packet_t* dequeue(struct packet_queue_t* queue);
struct packet_t* queue_peek(struct packet_queue_t* queue);

// Insert the packet at the head of the queue, contrary to normal FIFO operation.
void insert(struct packet_queue_t* queue, struct at_addr_t dest, struct at_addr_t source, byte type, size_t size, byte data[]);
void insert_packet(struct packet_queue_t* queue, struct packet_t* packet);



struct packet_port_t
{
	struct packet_queue_t in;
	struct packet_queue_t out;
};

void port_init(struct packet_port_t* port);
void port_shutdown(struct packet_port_t* port);
