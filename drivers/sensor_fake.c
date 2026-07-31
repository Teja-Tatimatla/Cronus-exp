#include "cronus/device.h"
#include <stdint.h>
#include <reent.h>

#include <sensor.h>
#include <cronus/mutex.h>
#include <cronus/config.h>
#include <cronus/tasks.h>
#include <uart.h>

struct sensor_fake_config {
  int32_t step;
  int32_t max;
};

struct sensor_fake_data {
  int32_t value;
};

static mutex_t sensor_lock;

static int sensor_fake_init(const struct device* dev) {
  struct sensor_fake_data* data = dev->data;
  init_mutex(&sensor_lock);
  mutex_lock(&sensor_lock, WAIT_FOREVER);
  task_delay(5);
  mutex_unlock(&sensor_lock);
  data->value = 0;
  return 0;
}

static int sensor_fake_fetch(const struct device* dev) {
  const struct sensor_fake_config* cfg = dev->config;
  struct sensor_fake_data* data = dev->data;
  data->value += cfg->step;
  if(data->value >= cfg->max) {
    data->value = 0;
  }
  return 0;
}

static int sensor_fake_get(const struct device* dev, int32_t* val) {
  const struct sensor_fake_data* data = dev->data;
  if(val == NULL) {
    return -1;
  }
  *val = data->value;
  return 0;
}

static int sensor_bad_init(const struct device* dev) {
  (void)dev;
  return -1;
}

static const struct sensor_driver_api sensor_fake_api = {
  .sample_fetch = sensor_fake_fetch,
  .channel_get  = sensor_fake_get,
};

DEVICE_DECLARE(gpio2);

static const struct sensor_fake_config fake0_cfg = {
  .step = 7,
  .max = 100
};
static struct sensor_fake_data fake0_data;
static const struct device* const fake0_deps[] = {
  DEVICE_REF(gpio2),
  0,
};

DEVICE_DEFINE(fake0, "sensor0", &fake0_data, &fake0_cfg, &sensor_fake_api,
              sensor_fake_init, 0, 0, fake0_deps, DEVICE_LEVEL_POST_KERNEL,
              10);



static struct sensor_fake_data bad0_data;

DEVICE_DEFINE(bad0, "sensor_bad", &bad0_data, &fake0_cfg, &sensor_fake_api,
              sensor_bad_init, 0, 0, 0, DEVICE_LEVEL_POST_KERNEL,
              05);

static struct sensor_fake_data dep0_data;
static const struct device* const dep0_deps[] = {
  DEVICE_REF(bad0),
  0,
};

DEVICE_DEFINE(dep0, "depchk", &dep0_data, &fake0_cfg, &sensor_fake_api,
              0, 0, 0, dep0_deps, DEVICE_LEVEL_POST_KERNEL,
              30);
