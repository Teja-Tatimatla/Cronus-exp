#include <stdint.h>
#include <sys/reent.h>

#include <cronus/sem.h>
#include <cronus/tasks.h>
#include <cronus/critical.h>
#include <systick.h>
#include <cm7.h>

extern tcb_t* curr_task;

void sem_init(semaphore_t* s, int32_t initial, int32_t max_count) {
  if(s == NULL || max_count <= 0 || initial < 0) {
    return;
  }

  s->count = initial;
  s->max_count = max_count;
  s->wait_list = NULL;
}

__itcm int sem_take(semaphore_t* s, uint32_t timeout_ms) {
  enter_critical();
    if(s == NULL) {
      exit_critical();
      return -1;
    }

    if(s->count > 0) {
      s->count--;
      exit_critical();
      return 1;
    }

    if(timeout_ms == 0) {
      exit_critical();
      return 0;
    }

    unlink_task_from_list(curr_task);
    curr_task->wait_result = 0;

    curr_task->state = TASK_BLOCKED;
    wait_insert(&s->wait_list, curr_task);

    if(timeout_ms != SEM_WAIT_FOREVER) {
      uint32_t now = get_tick_count();
      curr_task->wake = now + timeout_ms;
      insert_delayed_task(curr_task, now);
    }

  exit_critical();

  yield();
  return curr_task->wait_result;
}

__itcm void sem_give(semaphore_t* s) {
  enter_critical();

    if(s == NULL) {
      exit_critical();
      return;
    }

    tcb_t* t = wait_pop_highest(&s->wait_list);
    if(t) {
      unlink_task_from_list(t);

      t->wait_head = NULL;
      t->state = TASK_READY;
      t->wait_result = 1;

      insert_ready_task(t);
      int preempt = (t->priority > curr_task->priority);
      exit_critical();

      if(preempt) {
        yield();
      }
      return;
    }

    if (s->count < s->max_count) {
      s->count++;
    }

  exit_critical();
}

__itcm int sem_give_from_isr(semaphore_t* s) {
  uint32_t bpr = enter_critical_isr();
    if(s == NULL) {
      exit_critical_isr(bpr);
      return -1;
    }

    int preempt = 0;
    tcb_t* t = wait_pop_highest(&s->wait_list);
    if(t) {
      unlink_task_from_list(t);

      t->wait_head = NULL;
      t->state = TASK_READY;
      t->wait_result = 1;

      insert_ready_task(t);
      if(t->priority > curr_task->priority) {
        preempt = 1;
      }
    } else if(s->count < s->max_count) {
      s->count++;
    }

  exit_critical_isr(bpr);
  return preempt;
}
