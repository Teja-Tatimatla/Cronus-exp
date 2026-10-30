#include <stddef.h>
#include <stdint.h>
#include <reent.h>

#include <cronus/queue.h>
#include <cronus/critical.h>
#include <cronus/tasks.h>
#include <cronus/config.h>
#include <systick.h>
#include <cronus/cstrings.h>
#include <cm7.h>

extern tcb_t* curr_task;

void init_queue(queue_t* q, void* data_container, uint32_t capacity, size_t data_size) {
  if(q == NULL) {
    return;
  }

  q->buffer = (uint8_t *)data_container;
  q->head = 0;
  q->tail = 0;
  q->count = 0;
  q->capacity = capacity;
  q->data_size = data_size;
  q->send_waiters = NULL;
  q->receive_waiters = NULL;
  q->set = NULL;
}

static __itcm int wake_waiter(tcb_t* t) {
  unlink_task_from_list(t);
  t->wait_head = NULL;
  t->q_xfer = NULL;
  t->state = TASK_READY;
  t->wait_result = 1;
  insert_ready_task(t);
  return (t->priority > curr_task->priority);
}

static __itcm void copy_in_front(queue_t* q, const void* data) {
  q->tail = (q->tail + q->capacity - 1u) % q->capacity;
  cr_memcpy(q->buffer + (q->tail * q->data_size), data, q->data_size);
  q->count++;
}

static __itcm void copy_in(queue_t* q, const void* data) {
  cr_memcpy(q->buffer + (q->head * q->data_size), data, q->data_size);
  q->head = (q->head + 1u) % q->capacity;
  q->count++;
}

static __itcm void copy_out(queue_t* q, void* out) {
  cr_memcpy(out, q->buffer + (q->tail * q->data_size), q->data_size);
  q->tail = (q->tail + 1u) % q->capacity;
  q->count--;
}

static __itcm int queue_notify_set(queue_t* q) {
  queue_t* set = q->set;
  if(set == NULL) {
    return 0;
  }

  if(set->receive_waiters) {
    tcb_t* t = wait_pop_highest(&set->receive_waiters);
    cr_memcpy(t->q_xfer, &q, set->data_size);
    return wake_waiter(t);
  }

  if(set->count < set->capacity) {
    copy_in(set, &q);
  }

  return 0;
}

static __itcm int queue_send_common(queue_t* q, const void* data, uint32_t timeout, uint32_t to_front) {
  enter_critical();
    if(q == NULL || data == NULL) {
      exit_critical();
      return -1;
    }

    if(q->receive_waiters) {
      tcb_t* t = wait_pop_highest(&q->receive_waiters);
      cr_memcpy(t->q_xfer, data, q->data_size);
      int preempt = wake_waiter(t);

      exit_critical();
      if(preempt) {
        yield();
      }

      return 1;
    }

    if(q->count < q->capacity) {
      if(to_front) {
        copy_in_front(q, data);
      } else {
        copy_in(q, data);
      }

      int preempt = queue_notify_set(q);
      exit_critical();

      if(preempt) {
        yield();
      }

      return 1;
    }

    if(timeout == 0) {
      exit_critical();
      return 0;
    }

    unlink_task_from_list(curr_task);
    curr_task->q_xfer = (void *)data;
    curr_task->q_to_front = to_front;
    curr_task->state = TASK_BLOCKED;
    curr_task->wait_result = 0;
    wait_insert(&q->send_waiters, curr_task);

    if(timeout != WAIT_FOREVER) {
      uint32_t now = get_tick_count();
      curr_task->wake = now + timeout;
      insert_delayed_task(curr_task, now);
    }

    exit_critical();
    yield();
    enter_critical();

    curr_task->q_xfer = NULL;
    curr_task->q_to_front = 0;

  exit_critical();
  return curr_task->wait_result;
}

__itcm int queue_send(queue_t* q, const void* data, uint32_t timeout) {
  return queue_send_common(q, data, timeout, 0);
}

__itcm int queue_send_to_front(queue_t* q, const void* data, uint32_t timeout) {
  return queue_send_common(q, data, timeout, 1);
}

__itcm int queue_recv(queue_t* q, void* out, uint32_t timeout) {
  enter_critical();
    if(q == NULL || out == NULL) {
      exit_critical();
      return -1;
    }

    if(q->count > 0) {
      copy_out(q, out);

      int preempt = 0;
      if(q->send_waiters) {
        tcb_t* t = wait_pop_highest(&q->send_waiters);
        if(t->q_to_front) {
          copy_in_front(q, t->q_xfer);
        } else {
          copy_in(q, t->q_xfer);
        }

        preempt = wake_waiter(t);
        preempt |= queue_notify_set(q);
      }

      exit_critical();

      if(preempt) {
        yield();
      }

      return 1;
    }

    if(timeout == 0) {
      exit_critical();
      return 0;
    }

    unlink_task_from_list(curr_task);
    curr_task->q_xfer = out;
    curr_task->wait_result = 0;
    curr_task->state = TASK_BLOCKED;
    wait_insert(&q->receive_waiters, curr_task);

    if(timeout != WAIT_FOREVER) {
      uint32_t now = get_tick_count();
      curr_task->wake = now + timeout;
      insert_delayed_task(curr_task, now);
    }

    exit_critical();
    yield();
    enter_critical();

    curr_task->q_xfer = NULL;
  exit_critical();

  return curr_task->wait_result;
}

__itcm int queue_send_from_isr(queue_t* q, const void* data) {
  uint32_t bpr = enter_critical_isr();
    if(q == NULL || data == NULL) {
      exit_critical_isr(bpr);
      return -1;
    }

    int preempt = 0;
    if(q->receive_waiters) {
      tcb_t* t = wait_pop_highest(&q->receive_waiters);
      cr_memcpy(t->q_xfer, data, q->data_size);
      preempt = wake_waiter(t);
    } else if(q->count < q->capacity){
      copy_in(q, data);
      preempt = queue_notify_set(q);
    }
  exit_critical_isr(bpr);

  return preempt;
}

__itcm int queue_peek(queue_t* q, void* out) {
  enter_critical();
    if(q == NULL || out == NULL) {
      exit_critical();
      return -1;
    }

    if(q->count == 0) {
      exit_critical();
      return 0;
    }

    cr_memcpy(out, q->buffer + (q->tail * q->data_size), q->data_size);

  exit_critical();
  return 1;
}

__itcm uint32_t queue_count(queue_t* q) {
  enter_critical();
    if(q == NULL) {
      exit_critical();
      return 0;
    }

    uint32_t n = q->count;

  exit_critical();
  return n;
}

int queue_set_add(queue_t* member, queue_t* set) {
  enter_critical();
    if(member == NULL || set == NULL || member == set ||
      member->count != 0 || set->data_size != sizeof(queue_t*) ||
      member->set != NULL) {
        exit_critical();
        return -1;
    }

    member->set = set;

  exit_critical();
  return 1;
}

queue_t* queue_select(queue_t* q, uint32_t timeout) {
  queue_t* member = NULL;
  queue_recv(q, &member, timeout);
  return member;
}
