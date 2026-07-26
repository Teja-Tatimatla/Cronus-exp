#pragma once

#include <stddef.h>
#include <stdint.h>
#include <cronus/tasks.h>

typedef struct queue {
  uint8_t*      buffer;
  uint32_t      head;
  uint32_t      tail;
  uint32_t      count;
  uint32_t      capacity;
  size_t        data_size;
  tcb_t*        send_waiters;
  tcb_t*        receive_waiters;
  struct queue* set;
} queue_t;

void init_queue(queue_t* q, void* data_container, uint32_t capacity, size_t data_size);
int queue_send(queue_t* q, const void* data, uint32_t timeout);
int queue_send_to_front(queue_t* q, const void* data, uint32_t timeout);
int queue_recv(queue_t* q, void* out, uint32_t timeout);
int queue_send_from_isr(queue_t* q, const void* data);
int queue_peek(queue_t* q, void* out);
uint32_t queue_count(queue_t* q);
int queue_set_add(queue_t* member, queue_t* set);
queue_t* queue_select(queue_t* q, uint32_t timeout);
