#include "display.h"

#include <zephyr/drivers/display.h>

int init_display(void) {
  const struct device *display_dev = DEVICE_DT_GET(TOLVA_DISPLAY);
}
