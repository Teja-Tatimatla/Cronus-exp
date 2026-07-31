#include <stdint.h>
#include <reent.h>

#include <cronus/device.h>
#include <uart.h>
#include <cm7.h>
#include <cronus/cstrings.h>

extern const struct device __device_pre_start[], __device_pre_end[];
extern const struct device __device_post_start[], __device_post_end[];

static int deps_ready(const struct device* d) {
  if(d->deps == NULL) {
    return 1;
  }

  for(const struct device* const* dep_d = d->deps; *dep_d; dep_d++) {
    if(!(*dep_d)->state->ready) {
      uart_printf("devinit: %s SKIPPED, dep %s not ready\r\n", d->name, (*dep_d)->name);
      return 0;
    }
  }

  return 1;
}

static void init(const struct device* s, const struct device* e, int* failed) {
  for(const struct device* d = s; d < e; d++) {
    if(!deps_ready(d)) {
      d->state->ready = 0;
      d->state->init_res = -2;
      (*failed)++;
      continue;
    }

    uint32_t t0 = DWT_CYCCNT;
    int rc = d->init ? d->init(d) : 0;
    d->state->init_cycles = DWT_CYCCNT - t0;
    d->state->ready = (rc == 0) ? 1 : 0;
    d->state->init_res = (int8_t)rc;
    d->state->suspended = 0;
    if(rc != 0) {
      (*failed)++;
    }
    uart_printf("devinit: %s rc=%d\r\n", d->name, rc);
  }
}

int init_device(uint8_t level) {
  int failed = 0;
  if(level == DEVICE_LEVEL_PRE_KERNEL) {
    init(__device_pre_start, __device_pre_end, &failed);
  } else {
    init(__device_post_start, __device_post_end, &failed);
  }
  return failed;
}

int device_is_ready(const struct device* d) {
  return (d != (const struct device*)0) && d->state->ready;
}

const struct device* get_device(const char* name) {
  for(const struct device* d = __device_pre_start; d < __device_pre_end; d++) {
    if(cr_strcmp(d->name, name) == 0) {
      return d;
    }
  }

  for(const struct device* d = __device_post_start; d < __device_post_end; d++) {
    if(cr_strcmp(d->name, name) == 0) {
      return d;
    }
  }

  return (const struct device*)0;
}

static int power(const struct device* s, const struct device* e, int suspend, int reverse) {
  int acted = 0;
  if(reverse) {
    for(const struct device* d = e - 1; d >= s; d--) {
      if(!d->state->ready) {
        continue;
      }

      device_pm_fn fn = suspend ? d->suspend : d->resume;
      if(fn) {
        fn(d);
      }
      d->state->suspended = suspend ? 1 : 0;
      acted++;
    }
  } else {
    for(const struct device* d = s; d < e; d++) {
      if(!d->state->ready) {
        continue;
      }

      device_pm_fn fn = suspend ? d->suspend : d->resume;
      if(fn) {
        fn(d);
      }
      d->state->suspended = suspend ? 1 : 0;
      acted++;
    }
  }

  return acted;
}

int device_power_all(int suspend) {
  int acted = 0;
  if(suspend) {
    acted += power(__device_pre_start, __device_pre_end, 1, 1);
    acted += power(__device_post_start, __device_post_end, 1, 1);
  } else {
    acted += power(__device_pre_start, __device_pre_end, 0, 0);
    acted += power(__device_post_start, __device_post_end, 0, 0);
  }

  return acted;
}

static void reporter(const struct device* s, const struct device* e) {
  for(const struct device* d = s; d < e; d++) {
    uart_printf(" %s: level %u, ready %u, rc %d, init %u us, api %u\r\n",
      d->name, (unsigned)d->level, (unsigned)d->state->ready,
      (int)d->state->init_res, (unsigned)d->state->init_cycles,
      (unsigned)(uintptr_t)d->api);
  }
}

void device_reporter(void) {
  uart_printf("device (%u pre, %u post)",
  (unsigned)(__device_pre_end - __device_pre_start),
  (unsigned)(__device_post_end - __device_post_start));

  reporter(__device_pre_start, __device_pre_end);
  reporter(__device_post_start, __device_post_end);
}
