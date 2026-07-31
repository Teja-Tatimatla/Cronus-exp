#pragma once

#include <stdint.h>
#include <cronus/device.h>

#define GPIO_DIR_INPOT        0x0u
#define GPIO_DIR_OUTPUT       0x1u
#define GPIO_OUTPUT_INIT_LOW  0x2u
#define GPIO_OUTPUT_INIT_HIGH 0x4u

struct gpio_driver_api {
  int (*gpio_configure)(const struct device* dev, uint32_t pin, uint32_t flags);
  int (*gpio_set)(const struct device* dev, uint32_t pin, int value);
  int (*gpio_get)(const struct device* dev, uint32_t pin);
  int (*gpio_toggle)(const struct device* dev, uint32_t pin);
};

static inline int gpio_pin_configure(const struct device* dev, uint32_t pin, uint32_t flags) {
  if(!device_is_ready(dev) || dev->state->suspended) {
    return -1;
  }

  return ((const struct gpio_driver_api*)dev->api)->gpio_configure(dev, pin, flags);
}

static inline int gpio_pin_set(const struct device* dev, uint32_t pin, int value) {
  if(!device_is_ready(dev) || dev->state->suspended) {
    return -1;
  }

  return ((const struct gpio_driver_api*)dev->api)->gpio_set(dev, pin, value);
}

static inline int gpio_pin_get(const struct device* dev, uint32_t pin) {
  if(!device_is_ready(dev) || dev->state->suspended) {
    return -1;
  }

  return ((const struct gpio_driver_api*)dev->api)->gpio_get(dev, pin);
}

static inline int gpio_pin_toggle(const struct device* dev, uint32_t pin) {
  if(!device_is_ready(dev) || dev->state->suspended) {
    return -1;
  }

  return ((const struct gpio_driver_api*)dev->api)->gpio_toggle(dev, pin);
}
