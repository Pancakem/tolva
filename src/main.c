#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

#include "ble.h"

// Enable logs
LOG_MODULE_REGISTER(tolva_log);

// variable or buffer to hold info
struct wifi_info {
  char ssid[32];
  char password[32];
};

int main(void) {
  char buffer[64] = {0};

  if (bluetooth_business(buffer) != 0)
    sys_reboot(SYS_REBOOT_WARM);

  while (1) {
    k_sleep(K_FOREVER);
  }
  return 0;
}
