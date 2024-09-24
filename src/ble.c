#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/settings/settings.h>
#include <zephyr/types.h>

#ifdef CONFIG_MCUMGR
#include <mgmt/smp_bt.h>
#endif

#include "log.h"
#include "version.h"

static void connected(struct bt_conn *conn, uint8_t err);
static void disconnected(struct bt_conn *conn, uint8_t reason);
static bool le_param_req(struct bt_conn *conn, struct bt_le_conn_param *param);
static void le_param_updated(struct bt_conn *conn, uint16_t interval,
                             uint16_t latency, uint16_t timeout);

static struct k_work advertise_work;

/* Custom Service Variables */

#define BT_UUID_CUSTOM_SERVICE_VAL                                             \
  BT_UUID_128_ENCODE(0x49696277, 0xf2f0, 0x47c6, 0x8854, 0xe2dc31396481)

static const struct bt_uuid_128 primary_service_uuid =
    BT_UUID_INIT_128(BT_UUID_CUSTOM_SERVICE_VAL);

static const struct bt_uuid_128 read_characteristic_uuid = BT_UUID_INIT_128(
    BT_UUID_128_ENCODE(0x49696277, 0xf2f0, 0x47c6, 0x8854, 0xe2dc31396482));

static const struct bt_uuid_128 write_characteristic_uuid = BT_UUID_INIT_128(
    BT_UUID_128_ENCODE(0x49696277, 0xf2f0, 0x47c6, 0x8854, 0xe2dc31396483));

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    /* Device information */
    BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0x0a, 0x18),
    /* Current time */
    BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0x05, 0x18),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_CUSTOM_SERVICE_VAL),
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
            sizeof(CONFIG_BT_DEVICE_NAME) - 1),
#ifdef CONFIG_MCUMGR
    /* SMP */
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, 0x84, 0xaa, 0x60, 0x74, 0x52, 0x8a, 0x8b,
                  0x86, 0xd3, 0x4c, 0xb7, 0x1d, 0x1d, 0xdc, 0x53, 0x8d),
#endif
};

static struct bt_conn_cb m_conn_callbacks = {.connected = connected,
                                             .disconnected = disconnected,
                                             .le_param_req = le_param_req,
                                             .le_param_updated =
                                                 le_param_updated};

static struct bt_le_adv_param param = BT_LE_ADV_PARAM_INIT(
    BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_USE_NAME, BT_GAP_ADV_SLOW_INT_MIN,
    BT_GAP_ADV_SLOW_INT_MAX, NULL);

static int settings_runtime_load(void) {
  settings_runtime_set("bt/dis/sw", CONFIG_BT_DEVICE_NAME,
                       sizeof(CONFIG_BT_DEVICE_NAME));
  settings_runtime_set("bt/dis/fw", FW_VERSION, sizeof(FW_VERSION));
  return 0;
}

static void advertise(struct k_work *work) {
  int rc;

  bt_le_adv_stop();

  rc = bt_le_adv_start(&param, ad, ARRAY_SIZE(ad), NULL, 0);
  if (rc) {
    LOG_ERR("Advertising failed to start (rc %d)", rc);
    return;
  }

  LOG_INF("Advertising successfully started");
}

static void connected(struct bt_conn *conn, uint8_t err) {
  printk("test\n");
  if (err) {
    return;
  }
  LOG_INF("connected");
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
  LOG_INF("disconnected (reason: %u)", reason);
}

static bool le_param_req(struct bt_conn *conn, struct bt_le_conn_param *param) {
  return true;
}

static void le_param_updated(struct bt_conn *conn, uint16_t interval,
                             uint16_t latency, uint16_t timeout) {}

/*notify parameters from watch to outside world,
these can be real time measurements*/

static uint8_t vnd_value[] = {'t', 'o', 'l', 'v', 'a'};

static ssize_t read_vnd(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                        void *buf, uint16_t len, uint16_t offset) {
  const char *value = attr->user_data;

  return bt_gatt_attr_read(conn, attr, buf, len, offset, value, strlen(value));
}

static ssize_t write_vnd(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                         const void *buf, uint16_t len, uint16_t offset,
                         uint8_t flags) {
  uint8_t *value = attr->user_data;

  if (offset + len > sizeof(vnd_value)) {
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
  }

  memcpy(value + offset, buf, len);

  return len;
}

static uint8_t simulate_vnd;

static void vnd_ccc_cfg_changed(const struct bt_gatt_attr *attr,
                                uint16_t value) {
  simulate_vnd = (value == BT_GATT_CCC_INDICATE) ? 1 : 0;
}

/* Vendor Primary Service Declaration */
BT_GATT_SERVICE_DEFINE(
    primary_service, BT_GATT_PRIMARY_SERVICE(&primary_service_uuid),
    BT_GATT_CHARACTERISTIC(&read_characteristic_uuid.uuid, BT_GATT_CHRC_READ,
                           BT_GATT_PERM_READ, read_vnd, NULL, NULL),
    BT_GATT_CHARACTERISTIC(&write_characteristic_uuid.uuid, BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_WRITE_ENCRYPT, NULL, write_vnd,
                           NULL), );

int bt_vendor_notify(uint16_t sequence, uint16_t heartrate) {
  int rc;
  static uint8_t hrm[2];

  hrm[0] =
      sequence; /* value to describe the order, in case something gets lost */
  hrm[1] = heartrate; /* this is the real parameter value */

  rc = bt_gatt_notify(NULL, &primary_service.attrs[1], &hrm, sizeof(hrm));

  return rc == -ENOTCONN ? 0 : rc;
}

void param_notify(int nummer) {

  // instead of 240-nummer a measurement value can be used

  bt_vendor_notify(nummer, 240 - nummer);
}

void bt_init(void) {
  int err = bt_enable(NULL);
  if (err) {
    LOG_ERR("Bluetooth init failed (err %d)", err);
    return;
  }

  settings_load();
  settings_runtime_load();

  k_work_init(&advertise_work, advertise);
  bt_conn_cb_register(&m_conn_callbacks);
  LOG_INF("bt init callback started\n");

  k_work_submit(&advertise_work);
#ifdef CONFIG_MCUMGR
  /* Initialize the Bluetooth mcumgr transport. */
  smp_bt_register();
#endif

  // notify vendor specific to export values
  err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), NULL, 0);
  if (err) {
    LOG_ERR("problem starting vendor specific ...(err %d)", err);
  }

  LOG_INF("Bluetooth initialized");
}

void bt_adv_stop(void) {
  k_sleep(K_MSEC(400));

  int err = bt_le_adv_stop();
  if (err) {
    LOG_ERR("Advertising failed to stop (err %d)", err);
    return;
  }
}
