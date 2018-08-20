/*
   GSPLUS - Advanced Apple IIGS Emulator Environment
   Based on the KEGS emulator written by Kent Dickey
   See COPYRIGHT.txt for Copyright information
   See LICENSE.txt for license (GPL v2)
 */

/** This module implements queues for storing and transferring packets within the bridge. **/

#include "../defc.h"
#include "atalk.h"
#include "port.h"

void port_init(struct packet_port_t* port) {
  if (port)
  {
    queue_init(&port->in);
    queue_init(&port->out);
  }
}

void port_shutdown(struct packet_port_t* port) {
  if (port)
  {
    queue_shutdown(&port->in);
    queue_shutdown(&port->out);
  }
}

void queue_init(struct packet_queue_t* queue) {
  if (queue)
  {
    queue->head = queue->tail = 0;
  }
}

void queue_shutdown(struct packet_queue_t* queue) {
  if (queue)
  {
    struct packet_t* packet = dequeue(queue);
    while (packet)
    {
      if (packet->data)
        free(packet->data);
      free(packet);
      packet = dequeue(queue);
    }
  }
}

void enqueue(struct packet_queue_t* queue, struct at_addr_t dest, struct at_addr_t source, byte type, size_t size, byte data[]) {
  if (!queue)
    return;

  struct packet_t* packet = (struct packet_t*)malloc(sizeof(struct packet_t));
  packet->dest.network = dest.network;
  packet->dest.node = dest.node;
  packet->source.network = source.network;
  packet->source.node = source.node;
  packet->type = type;
  packet->size = size;
  packet->data = data;
  enqueue_packet(queue, packet);
}

void enqueue_packet(struct packet_queue_t* queue, struct packet_t* packet) {
  packet->next = 0;

  if (queue->tail)
    queue->tail->next = packet;
  else
    queue->head = packet;
  queue->tail = packet;
}

void insert(struct packet_queue_t* queue, struct at_addr_t dest, struct at_addr_t source, byte type, size_t size, byte data[]) {
  if (!queue)
    return;

  struct packet_t* packet = (struct packet_t*)malloc(sizeof(struct packet_t));
  packet->dest.network = dest.network;
  packet->dest.node = dest.node;
  packet->source.network = source.network;
  packet->source.node = source.node;
  packet->type = type;
  packet->size = size;
  packet->data = data;
  insert_packet(queue, packet);
}

void insert_packet(struct packet_queue_t* queue, struct packet_t* packet) {
  packet->next = queue->head;
  queue->head = packet;
  if (!queue->tail)
    queue->tail = queue->head;
}

struct packet_t* dequeue(struct packet_queue_t* queue) {
  if (queue && queue->head)
  {
    struct packet_t* packet = queue->head;
    if (queue->tail == queue->head)
      queue->tail = queue->head = 0;
    else
      queue->head = packet->next;
    return packet;
  }
  else
    return 0;
}

struct packet_t* queue_peek(struct packet_queue_t* queue) {
  if (queue)
    return queue->head;
  else
    return 0;
}
