#include <stdint.h>

#include <cronus/device.h>
#include <gpio.h>
#include <imxrt1062.h>

struct gpio_imxrt_config {
  volatile uint32_t* ccgr;
  volatile uint32_t* gdir;
  volatile uint32_t* dr;
  volatile uint32_t* psr;
  uint32_t ccgr_mask;
};

struct gpio_imxrt_out_data {
  uint32_t saved_gdir;
  uint32_t saved_dr;
};

static int gpio_imxrt_init(const struct device* dev) {
  const struct gpio_imxrt_config* cfg = dev->config;
  if(cfg->dr == (volatile uint32_t*)0) {
    return -1;
  }
  if(cfg->ccgr) {
    *cfg->ccgr |= cfg->ccgr_mask;
  }
  return 0;
}

static int gpio_imxrt_configure(const struct device* dev, uint32_t pin, uint32_t flags) {
  const struct gpio_imxrt_config* cfg = dev->config;
  if(pin >= 32u) {
    return -1;
  }

  if((flags & GPIO_OUTPUT_INIT_LOW) && (flags & GPIO_OUTPUT_INIT_HIGH)) {
    return -1;
  }

  if(flags & GPIO_DIR_OUTPUT) {
    if(flags & GPIO_OUTPUT_INIT_HIGH) {
      *cfg->dr |= (1u << pin);
    } else if (flags & GPIO_OUTPUT_INIT_LOW){
      *cfg->dr &= ~(1u << pin);
    }

    *cfg->gdir |= (1u << pin);
  } else {
    *cfg->gdir &= ~(1u << pin);
  }

  return 0;
}

static int gpio_imxrt_set(const struct device* dev, uint32_t pin, int value) {
  const struct gpio_imxrt_config* cfg = dev->config;
  if(pin >= 32u) {
    return -1;
  }

  if(value) {
    *cfg->dr |= (1u << pin);
  } else {
    *cfg->dr &= ~(1u << pin);
  }

  return 0;
}

static int gpio_imxrt_get(const struct device* dev, uint32_t pin) {
  const struct gpio_imxrt_config* cfg = dev->config;
  if(pin >= 32u) {
    return -1;
  }

  return (int)((*cfg->psr >> pin) & 1u);
}

static int gpio_imxrt_toggle(const struct device* dev, uint32_t pin) {
  const struct gpio_imxrt_config* cfg = dev->config;
  if(pin >= 32u) {
    return -1;
  }

  *cfg->dr ^= (1u << pin);
  return 0;
}

static int gpio_imxrt_suspend(const struct device* dev) {
  const struct gpio_imxrt_config* cfg = dev->config;
  struct gpio_imxrt_out_data* data = dev->data;

  data->saved_dr = *cfg->dr;
  data->saved_gdir = *cfg->gdir;

  if(cfg->ccgr) {
    *cfg->ccgr &= ~cfg->ccgr_mask;
  }

  return 0;
}

static int gpio_imxrt_resume(const struct device* dev) {
  const struct gpio_imxrt_config* cfg = dev->config;
  struct gpio_imxrt_out_data* data = dev->data;

  if(cfg->ccgr) {
    *cfg->ccgr |= cfg->ccgr_mask;
  }

  *cfg->gdir = data->saved_gdir;
  *cfg->dr = data->saved_dr;

  return 0;
}

static const struct gpio_driver_api gpio_imxrt_api = {
  .gpio_configure = gpio_imxrt_configure,
  .gpio_set = gpio_imxrt_set,
  .gpio_get = gpio_imxrt_get,
  .gpio_toggle = gpio_imxrt_toggle,
};

static const struct gpio_imxrt_config gpio1_cfg = {
  .dr   = (volatile uint32_t*)0x401B8000u,
  .gdir = (volatile uint32_t*)0x401B8004u,
  .psr  = (volatile uint32_t*)0x401B8008u,
  .ccgr = &CCM_CCGR1,
  .ccgr_mask = (3u << 26),
};
static struct gpio_imxrt_out_data gpio1_data;

DEVICE_DEFINE(gpio1, "gpio1", &gpio1_data, &gpio1_cfg, &gpio_imxrt_api,
              gpio_imxrt_init, gpio_imxrt_suspend, gpio_imxrt_resume,
              0, DEVICE_LEVEL_PRE_KERNEL, 11);

static const struct gpio_imxrt_config gpio2_cfg = {
  .dr   = (volatile uint32_t*)0x401BC000u,
  .gdir = (volatile uint32_t*)0x401BC004u,
  .psr  = (volatile uint32_t*)0x401BC008u,
  .ccgr = &CCM_CCGR0,
  .ccgr_mask = (3u << 30),
};
static struct gpio_imxrt_out_data gpio2_data;

DEVICE_DEFINE(gpio2, "gpio2", &gpio2_data, &gpio2_cfg, &gpio_imxrt_api,
              gpio_imxrt_init, gpio_imxrt_suspend, gpio_imxrt_resume,
              0, DEVICE_LEVEL_PRE_KERNEL, 10);

static const struct gpio_imxrt_config gpio3_cfg = {
  .dr   = (volatile uint32_t*)0x401C0000u,
  .gdir = (volatile uint32_t*)0x401C0004u,
  .psr  = (volatile uint32_t*)0x401C0008u,
  .ccgr = &CCM_CCGR2,
  .ccgr_mask = (3u << 26),
};
static struct gpio_imxrt_out_data gpio3_data;

DEVICE_DEFINE(gpio3, "gpio3", &gpio3_data, &gpio3_cfg, &gpio_imxrt_api,
              gpio_imxrt_init, gpio_imxrt_suspend, gpio_imxrt_resume,
              0, DEVICE_LEVEL_PRE_KERNEL, 13);

static const struct gpio_imxrt_config gpio4_cfg = {
  .dr   = (volatile uint32_t*)0x401C4000u,
  .gdir = (volatile uint32_t*)0x401C4004u,
  .psr  = (volatile uint32_t*)0x401C4008u,
  .ccgr = &CCM_CCGR3,
  .ccgr_mask = (3u << 12),
};
static struct gpio_imxrt_out_data gpio4_data;

DEVICE_DEFINE(gpio4, "gpio4", &gpio4_data, &gpio4_cfg, &gpio_imxrt_api,
              gpio_imxrt_init, gpio_imxrt_suspend, gpio_imxrt_resume,
              0, DEVICE_LEVEL_PRE_KERNEL, 14);

static const struct gpio_imxrt_config gpio5_cfg = {
  .dr   = (volatile uint32_t*)0x400C0000u,
  .gdir = (volatile uint32_t*)0x400C0004u,
  .psr  = (volatile uint32_t*)0x400C0008u,
  .ccgr = &CCM_CCGR1,
  .ccgr_mask = (3u << 30),
};
static struct gpio_imxrt_out_data gpio5_data;

DEVICE_DEFINE(gpio5, "gpio5", &gpio5_data, &gpio5_cfg, &gpio_imxrt_api,
              gpio_imxrt_init, gpio_imxrt_suspend, gpio_imxrt_resume,
              0, DEVICE_LEVEL_PRE_KERNEL, 15);

static const struct gpio_imxrt_config gpio6_cfg = {
  .dr   = (volatile uint32_t*)0x42000000u,
  .gdir = (volatile uint32_t*)0x42000004u,
  .psr  = (volatile uint32_t*)0x42000008u,
};
static struct gpio_imxrt_out_data gpio6_data;

DEVICE_DEFINE(gpio6, "gpio6", &gpio6_data, &gpio6_cfg, &gpio_imxrt_api,
              gpio_imxrt_init, gpio_imxrt_suspend, gpio_imxrt_resume,
              0, DEVICE_LEVEL_PRE_KERNEL, 21);

static const struct gpio_imxrt_config gpio7_cfg = {
  .dr   = (volatile uint32_t*)0x42004000u,
  .gdir = (volatile uint32_t*)0x42004004u,
  .psr  = (volatile uint32_t*)0x42004008u,
};
static struct gpio_imxrt_out_data gpio7_data;

DEVICE_DEFINE(gpio7, "gpio7", &gpio7_data, &gpio7_cfg, &gpio_imxrt_api,
              gpio_imxrt_init, gpio_imxrt_suspend, gpio_imxrt_resume,
              0, DEVICE_LEVEL_PRE_KERNEL, 22);

static const struct gpio_imxrt_config gpio8_cfg = {
  .dr = (volatile uint32_t*)0x42008000u,
  .gdir = (volatile uint32_t*)0x42008004u,
  .psr  = (volatile uint32_t*)0x42008008u,
};
static struct gpio_imxrt_out_data gpio8_data;

DEVICE_DEFINE(gpio8, "gpio8", &gpio8_data, &gpio8_cfg, &gpio_imxrt_api,
              gpio_imxrt_init, gpio_imxrt_suspend, gpio_imxrt_resume,
              0, DEVICE_LEVEL_PRE_KERNEL, 23);

static const struct gpio_imxrt_config gpio9_cfg = {
  .dr   = (volatile uint32_t*)0x4200C000u,
  .gdir = (volatile uint32_t*)0x4200C004u,
  .psr  = (volatile uint32_t*)0x4200C008u,
};
static struct gpio_imxrt_out_data gpio9_data;

DEVICE_DEFINE(gpio9, "gpio9", &gpio9_data, &gpio9_cfg, &gpio_imxrt_api,
              gpio_imxrt_init, gpio_imxrt_suspend, gpio_imxrt_resume,
              0, DEVICE_LEVEL_PRE_KERNEL, 24);
