#pragma once

#include <stdint.h>

struct device;
typedef int (*device_init_fn)(const struct device* dev);
typedef int (*device_pm_fn)(const struct device* dev);

struct device_state {
  uint8_t  ready;
  uint8_t  suspended;
  int8_t   init_res;
};

struct device {
  const char*                 name;
  void*                       data;
  const void*                 config;
  const void*                 api;
  struct device_state*        state;
  device_init_fn              init;
  device_pm_fn                suspend;
  device_pm_fn                resume;
  const struct device* const* deps;
  uint8_t                     level;
};

#define DEVICE_LEVEL_PRE_KERNEL   0
#define DEVICE_LEVEL_POST_KERNEL  1

#define _DEV_STR2(x) #x
#define _DEV_STR(x) _DEV_STR2(x)

#define DEVICE_DEFINE(id, drv_name, data_p, config_p, api_p, init_fn, suspend_fn, resume_fn, deps_p, lvl, priority)    \
                      static struct device_state _devstate_##id;                                                         \
                      const struct device _dev_##id                                                                      \
                      __attribute__((used, section(".device_init." _DEV_STR(lvl) "_" _DEV_STR(priority) "." #id))) = { \
                      .name = (drv_name), .data = (data_p), .config = (config_p), .api = (api_p),                        \
                      .state = &_devstate_##id, .init = (init_fn), .suspend = (suspend_fn), .resume = (resume_fn),         \
                      .deps = (deps_p), .level = (lvl) }

#define DEVICE_DECLARE(id) extern const struct device _dev_##id
#define DEVICE_REF(id)  (&_dev_##id)

int init_device(uint8_t level);
int device_is_ready(const struct device*);
const struct device* get_device(const char* name);
int device_power_all(int suspend);
void device_reporter(void);
