#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>

#include "ble.h"
#include "log.h"

struct wifi_info {
  char ssid[32];
  char password[32];
};

int main(void) {
  bt_init();
  while (1) {
    k_sleep(K_MSEC(100));
  }
  return 0;
}
