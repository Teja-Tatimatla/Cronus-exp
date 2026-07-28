#include <reent.h>
#include <stdint.h>

#include <cronus/timer.h>
#include <cronus/config.h>
#include <cronus/queue.h>
#include <cronus/tasks.h>
#include <systick.h>
#include <uart.h>

typedef enum {
  TCMD_START = 0,
  TCMD_RESET,
  TCMD_STOP,
  TCMD_CHANGE_PERIOD,
  TCMD_DELETE,
  TCMD_PEND_FUNC,
  TCMD_REPORT
} timer_cmd_e;

typedef struct {
  timer_t*       timer;
  timer_cmd_e    cmd;
  timer_pendfn_t pend_func;
  void*          parg;
  uint32_t       arg;
  uint32_t       issued_at;
  uint16_t       generation;
} timer_cmd_t;

static timer_cmd_t cmd_store[CONFIG_TIMER_CMD_QUEUE_LEN];
static timer_t* timer_list;
static uint32_t stale_drops;
static queue_t  cmd_q;

void init_timer(timer_t* t, const char* name, uint32_t period, int auto_reload, int catch_up, timer_cb_t cb, void* user_data) {
  if(t == NULL) {
    return;
  }

  t->name = name;
  t->period = period;
  t->expiry = 0;
  t->callback = cb;
  t->user_data = user_data;
  t->active = 0;
  t->auto_reload = auto_reload;
  t->catch_up = catch_up;
  t->generation = 0;
  t->fire_count = 0;
  t->max_late = 0;
  t->next = NULL;
}

static void list_remove(timer_t* t) {
  timer_t** pp = &timer_list;
  while(*pp && *pp != t) {
    pp = &(*pp)->next;
  }
  if(*pp){
    *pp = t->next;
    t->next = NULL;
  }
}

static void list_insert(timer_t* t) {
  timer_t** pp = &timer_list;
  while(*pp && (int32_t)(t->expiry - (*pp)->expiry) >= 0) {
    pp = &(*pp)->next;
  }

  t->next = *pp;
  *pp = t;
}

static uint32_t ticks_until_next(uint32_t now) {
  if(timer_list == NULL) {
    return WAIT_FOREVER;
  }

  int32_t d = (int32_t)(timer_list->expiry - now);
  return (d > 0) ? (uint32_t)d : 0u;
}

static void apply_cmd(timer_cmd_t* c) {
  if(c->cmd == TCMD_PEND_FUNC) {
    if(c->pend_func) {
      c->pend_func(c->parg, c->arg);
    }
    return;
  }

  if(c->cmd == TCMD_REPORT) {
    uint32_t now = get_tick_count();
    uart_printf("timers @ %u (stale_drops=%u):\r\n", (unsigned)now, (unsigned)stale_drops);
    for(timer_t* t = timer_list; t; t = t->next) {
      uart_printf(" %s: due in %d, period: %u, fires: %u, max_late: %u\r\n", t->name, (int)(uint32_t)(t->expiry - now),
                  (unsigned)t->period, (unsigned)t->fire_count, (unsigned)t->max_late);
    }
    return;
  }

  timer_t* t = c->timer;
  if(t == NULL) {
    return;
  }

  if(t->generation != c->generation) {
    stale_drops++;
    return;
  }

  switch(c->cmd) {
    case TCMD_START:
    case TCMD_RESET:
      if(t->active) {
        list_remove(t);
      }

      t->expiry = c->issued_at + t->period;
      t->active = 1;
      list_insert(t);
      break;

    case TCMD_CHANGE_PERIOD:
      t->period = c->arg;
      if(t->active) {
        list_remove(t);
      }

      t->expiry = c->issued_at + t->period;
      t->active = 1;
      list_insert(t);
      break;

    case TCMD_STOP:
      if(t->active) {
        list_remove(t);
        t->active = 0;
      }
      break;

    case TCMD_DELETE:
      if(t->active) {
        list_remove(t);
        t->active = 0;
      }
      t->generation++;
      break;

    default:
      break;
  }
}

static void service_expired(uint32_t now) {
  while(timer_list && (int32_t)(timer_list->expiry - now) <= 0) {
    timer_t* t = timer_list;
    list_remove(timer_list);

    uint32_t late = now - t->expiry;
    if(late > t->max_late) {
      t->max_late = late;
    }
    t->fire_count++;

    if(t->auto_reload && t->period > 0u) {
      t->expiry += t->period;
      if(!t->catch_up) {
        while((int32_t)(t->expiry - now) <= 0) {
          t->expiry += t->period;
        }
      }
      list_insert(t);
    } else {
      t->active = 0;  /* one-shot, or a period-0 reload degraded to one */
    }

    if(t->callback) {
      t->callback(t);
    }
  }
}

static void timer_daemon(void) {
  timer_cmd_t c;
  while(1) {
    uint32_t block = ticks_until_next(get_tick_count());
    if(queue_recv(&cmd_q, &c, block) == 1) {
      apply_cmd(&c);
    }
    service_expired(get_tick_count());
  }
}

void init_timer_daemon(uint32_t* stack) {
  init_queue(&cmd_q, cmd_store, CONFIG_TIMER_CMD_QUEUE_LEN, sizeof(timer_cmd_t));
  timer_list = NULL;
  stale_drops = 0;
  create_task(stack, timer_daemon, CONFIG_TIMER_TASK_STACK, CONFIG_TIMER_TASK_PRIORITY, "timerd");
}

static int post(timer_t* t, uint8_t cmd, uint32_t arg, uint32_t timeout) {
  timer_cmd_t c;
  c.timer = t;
  c.pend_func = NULL;
  c.parg = 0;
  c.arg = arg;
  c.issued_at = get_tick_count();
  c.generation = t ? t->generation : 0u;
  c.cmd = cmd;
  return (queue_send(&cmd_q, &c, timeout) == 1) ? 1 : 0;
}

int timer_start(timer_t* t, uint32_t timeout) {
  return post(t, TCMD_START, 0, timeout);
}

int timer_stop(timer_t* t, uint32_t timeout) {
  return post(t, TCMD_STOP, 0, timeout);
}

int timer_reset(timer_t* t, uint32_t timeout) {
  return post(t, TCMD_RESET, 0, timeout);
}

int timer_delete(timer_t* t, uint32_t timeout) {
  return post(t, TCMD_DELETE, 0, timeout);
}

int timer_reporter(uint32_t timeout) {
  return post((timer_t *)0, TCMD_REPORT, 0, timeout);
}

int timer_change_period(timer_t* t, uint32_t new_period, uint32_t timeout) {
  return post(t, TCMD_CHANGE_PERIOD, new_period, timeout);
}

int timer_pend_function(timer_pendfn_t fn, void* parg, uint32_t arg, uint32_t timeout) {
  timer_cmd_t c;
  c.timer = (timer_t*)0;
  c.pend_func = fn;
  c.parg = parg;
  c.arg = arg;
  c.issued_at = get_tick_count();
  c.generation = 0;
  c.cmd = TCMD_PEND_FUNC;
  return (queue_send(&cmd_q, &c, timeout) == 1) ? 1 : 0;
}

int timer_start_from_isr(timer_t* t) {
  timer_cmd_t c;
  c.timer = t;
  c.pend_func = NULL;
  c.parg = 0;
  c.arg = 0;
  c.issued_at = get_tick_count();
  c.generation = t ? t->generation : 0;
  c.cmd = TCMD_START;
  return queue_send_from_isr(&cmd_q, &c);
}

int timer_pend_function_from_isr(timer_pendfn_t fn, void* parg, uint32_t arg) {
  timer_cmd_t c;
  c.timer = (timer_t *)0;
  c.pend_func = fn;
  c.parg = parg;
  c.arg = arg;
  c.issued_at = get_tick_count();
  c.generation = 0;
  c.cmd = TCMD_PEND_FUNC;
  return queue_send_from_isr(&cmd_q, &c);
}
