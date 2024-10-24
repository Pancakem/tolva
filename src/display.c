#include "display.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>

#define TOLVA_DISPLAY DT_CHOSEN(zephyr_display)

LOG_MODULE_REGISTER(tolva_display);

int init_display(void) {
#if DT_NODE_HAS_STATUS(TOLVA_DISPLAY, okay)
  const struct device *const disp_dev = DEVICE_DT_GET(TOLVA_DISPLAY);
#else
#error "Node is disabled"
#endif

  if (!device_is_ready(disp_dev)) {
    LOG_ERR("display not ready");
    return 1;
  }
  LOG_INF("display is ready");
  return 0;
}
