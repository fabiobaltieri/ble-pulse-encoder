#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>

#include "blinker.h"

LOG_MODULE_REGISTER(ble_state, LOG_LEVEL_INF);

static const struct device *qdec = DEVICE_DT_GET(DT_NODELABEL(gpio_qdec));

static void connected(struct bt_conn *conn, uint8_t err)
{
	pm_device_action_run(qdec, PM_DEVICE_ACTION_RESUME);

	blink(BLINK_CONNECTED);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	pm_device_action_run(qdec, PM_DEVICE_ACTION_SUSPEND);

	blink(BLINK_DISCONNECTED);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};
