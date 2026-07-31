#pragma once

#include <stdint.h>
#include <cronus/device.h>

struct sensor_driver_api {
  int (*sample_fetch)(const struct device* dev);
  int (*channel_get)(const struct device* dev, int32_t* val);
};

static inline int sensor_sample_fetch(const struct device* dev) {
  if(!device_is_ready(dev) || dev->state->suspended) {
    return -1;
  }

  return ((const struct sensor_driver_api*)dev->api)->sample_fetch(dev);
}

static inline int sensor_channel_get(const struct device* dev, int32_t* val) {
  if(!device_is_ready(dev) || dev->state->suspended) {
    return -1;
  }

  return ((const struct sensor_driver_api*)dev->api)->channel_get(dev, val);
}
