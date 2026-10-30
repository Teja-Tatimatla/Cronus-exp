#include <stdint.h>
#include <sys/reent.h>

#include <cronus/mutex.h>
#include <cronus/critical.h>
#include <cronus/tasks.h>
#include <cronus/config.h>
#include <systick.h>
#include <cm7.h>

extern tcb_t* curr_task;

static __itcm void held_push(mutex_t* m, tcb_t* t) {
  m->next_held = t->held_mutexes;
  t->held_mutexes = m;
}

static __itcm void held_remove(tcb_t* t, mutex_t* m) {
  struct mutex** hm = &t->held_mutexes;
  while(*hm && *hm != m) {
    hm = &(*hm)->next_held;
  }
  if(*hm) {
    *hm = m->next_held;
    m->next_held = NULL;
  }
}

#if CONFIG_MUTEX_PRIORITY_INHERITANCE
static __itcm void inheritance_walk(tcb_t* owner, uint32_t priority) {
  uint32_t depth = 0;
  while(owner != NULL && depth++ < CONFIG_MUTEX_CHAIN_MAX) {
    if(owner->priority >= priority) {
      return;
    }

    task_set_priority(owner, priority);
    if(owner->state == TASK_BLOCKED && owner->blocking_mutex != NULL) {
      owner = owner->blocking_mutex->owner;
    } else {
      return;
    }
  }
}

static __itcm uint8_t top_waiter_priority(mutex_t* m) {
  return m->wait_list ? m->wait_list->priority : 0;
}

static __itcm uint8_t effective_floor(tcb_t* t) {
  uint8_t floor = t->base_priorty;
  for(struct mutex* m = t->held_mutexes; m != NULL; m = m->next_held) {
    uint8_t wp = top_waiter_priority(m);
    if(floor < wp) {
      floor = wp;
    }
  }

  return floor;
}

static __itcm void reevaluate_inheritance(tcb_t* t) {
  uint8_t floor = effective_floor(t);
  if(floor != t->priority) {
    task_set_priority(t, floor);
  }
}
#endif

void init_mutex(mutex_t* m) {
  if(m == NULL) {
    return;
  }

  m->owner = NULL;
  m->wait_list = NULL;
  m->recursion_count = 0;
  m->recursive = 0;
  m->next_held = NULL;
}

void init_mutex_recursive(mutex_t* m) {
  if(m == NULL) {
    return;
  }
  init_mutex(m);
  m->recursive = 1;
}

__itcm int mutex_lock(mutex_t* m, uint32_t time_out) {
  enter_critical();
    if(m == NULL) {
      exit_critical();
      return -1;
    }

    if(m->owner == NULL) {
      m->owner = curr_task;
      m->recursion_count = 1;
      held_push(m, curr_task);
      exit_critical();
      return 1;
    }

    if(m->owner == curr_task) {
      if(m->recursive) {
        m->recursion_count++;
        exit_critical();
        return 1;
      } else {
        exit_critical();
        return -1;
      }
    }

    if(time_out == 0) {
      exit_critical();
      return 0;
    }

#if CONFIG_MUTEX_PRIORITY_INHERITANCE
    inheritance_walk(m->owner, curr_task->priority);
#endif

    curr_task->blocking_mutex = m;

    unlink_task_from_list(curr_task);
    curr_task->state = TASK_BLOCKED;
    curr_task->wait_result = 1;
    wait_insert(&m->wait_list, curr_task);

    if(time_out != WAIT_FOREVER) {
      uint32_t now = get_tick_count();
      curr_task->wake = now + time_out;
      insert_delayed_task(curr_task, now);
    }
  exit_critical();
  yield();
  enter_critical();
    curr_task->blocking_mutex = NULL;
#if CONFIG_MUTEX_PRIORITY_INHERITANCE
    if(curr_task->wait_result == 0 && m->owner != NULL) {
      reevaluate_inheritance(curr_task);
    }
#endif

  exit_critical();

  return curr_task->wait_result;
}

__itcm int mutex_unlock(mutex_t* m) {
  enter_critical();
    if(m == NULL || m->owner != curr_task) {
      exit_critical();
      return -1;
    }

    if(m->recursion_count > 1) {
      m->recursion_count--;
      exit_critical();
      return 1;
    }

    held_remove(curr_task, m);

    tcb_t* w = wait_pop_highest(&m->wait_list);
    if(w) {
      unlink_task_from_list(w);
      w->blocking_mutex = NULL;
      m->owner = w;
      m->recursion_count = 1;
      held_push(m, w);
      w->state = TASK_READY;
      w->wait_result = 1;
      insert_ready_task(w);
#if CONFIG_MUTEX_PRIORITY_INHERITANCE
      reevaluate_inheritance(curr_task);
#endif
      int preempt = (w->priority > curr_task->priority);
      exit_critical();
      if(preempt) {
        yield();
      }

      return 1;
    }

    m->owner = NULL;
    m->recursion_count = 0;
#if CONFIG_MUTEX_PRIORITY_INHERITANCE
    reevaluate_inheritance(curr_task);
#endif

  exit_critical();
  return 1;
}
