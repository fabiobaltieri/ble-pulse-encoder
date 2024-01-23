#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>

#include "blinker.h"

LOG_MODULE_REGISTER(ble_state, LOG_LEVEL_INF);

static void connected(struct bt_conn *conn, uint8_t err)
{
	blink(BLINK_CONNECTED);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	blink(BLINK_DISCONNECTED);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};
