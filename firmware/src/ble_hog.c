#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "ble_hog.h"

LOG_MODULE_REGISTER(ble_hog, LOG_LEVEL_INF);

static struct hids_info info = {
	.version = 0x0000,
	.code = 0x00,
	.flags = HIDS_NORMALLY_CONNECTABLE,
};

static struct hids_report input_mouse = {
	.id = 0x01,
	.type = HIDS_INPUT,
};

static bool notify_enabled;
static uint8_t ctrl_point;
static uint8_t report_map[] = {
	0x05, 0x01,                    // Usage Page (Generic Desktop)
	0x09, 0x02,                    // Usage (Mouse)
	0xa1, 0x01,                    // Collection (Application)
	0x85, 0x01,                    //  Report ID (1)
	0x09, 0x01,                    //  Usage (Pointer)
	0xa1, 0x00,                    //  Collection (Physical)

	0x05, 0x09,                    //   Usage Page (Button)
	0x19, 0x01,                    //   Usage Minimum (1)
	0x29, 0x03,                    //   Usage Maximum (3)
	0x15, 0x00,                    //   Logical Minimum (0)
	0x25, 0x01,                    //   Logical Maximum (1)
	0x95, 0x03,                    //   Report Count (3)
	0x75, 0x01,                    //   Report Size (1)
	0x81, 0x02,                    //   Input (Data,Var,Abs)

	0x95, 0x01,                    //   Report Count (1)
	0x75, 0x05,                    //   Report Size (5)
	0x81, 0x03,                    //   Input (Cnst,Var,Abs)

	0x05, 0x01,                    //   Usage Page (Generic Desktop)
	0x16, 0x01, 0x80,              //   Logical Minimum (-32767)
	0x26, 0xff, 0x7f,              //   Logical Maximum (32767)
	0x75, 0x10,                    //   Report Size (16)
	0x95, 0x02,                    //   Report Count (2)
	0x09, 0x30,                    //   Usage (X)
	0x09, 0x31,                    //   Usage (Y)
	0x81, 0x06,                    //   Input (Data,Var,Rel)

	0x15, 0x81,                    //   Logical Minimum (-127)
	0x25, 0x7f,                    //   Logical Maximum (127)
	0x75, 0x08,                    //   Report Size (8)
	0x95, 0x01,                    //   Report Count (1)
	0x09, 0x38,                    //   Usage (Wheel)
	0x81, 0x06,                    //   Input (Data,Var,Rel)

	0xc0,                          //  End Collection
	0xc0,                          // End Collection
};

static ssize_t read_info(struct bt_conn *conn,
			  const struct bt_gatt_attr *attr, void *buf,
			  uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
				 sizeof(struct hids_info));
}

static ssize_t read_report_map(struct bt_conn *conn,
			       const struct bt_gatt_attr *attr, void *buf,
			       uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, report_map,
				 sizeof(report_map));
}

static ssize_t read_report(struct bt_conn *conn,
			   const struct bt_gatt_attr *attr, void *buf,
			   uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
				 sizeof(struct hids_report));
}

static void input_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	ARG_UNUSED(attr);

	notify_enabled = (value == BT_GATT_CCC_NOTIFY);

	LOG_INF("notify_enabled: %d", notify_enabled);
}

static ssize_t read_input_report(struct bt_conn *conn,
				 const struct bt_gatt_attr *attr, void *buf,
				 uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, NULL, 0);
}

static ssize_t write_ctrl_point(struct bt_conn *conn,
				const struct bt_gatt_attr *attr,
				const void *buf, uint16_t len, uint16_t offset,
				uint8_t flags)
{
	uint8_t *value = attr->user_data;

	if (offset + len > sizeof(ctrl_point)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	memcpy(value + offset, buf, len);

	return len;
}

/* HID Service Declaration */
BT_GATT_SERVICE_DEFINE(hog_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_HIDS),
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_INFO, BT_GATT_CHRC_READ,
			       BT_GATT_PERM_READ, read_info, NULL, &info),
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT_MAP, BT_GATT_CHRC_READ,
			       BT_GATT_PERM_READ, read_report_map, NULL, NULL),

	/* Report 1: mouse */
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ_ENCRYPT,
			       read_input_report, NULL, NULL),
	BT_GATT_CCC(input_ccc_changed,
		    BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),
	BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ,
			   read_report, NULL, &input_mouse),

	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_CTRL_POINT,
			       BT_GATT_CHRC_WRITE_WITHOUT_RESP,
			       BT_GATT_PERM_WRITE,
			       NULL, write_ctrl_point, &ctrl_point),
);

static struct {
	uint8_t buttons;
	int16_t x;
	int16_t y;
	int8_t wheel;
} __packed report_mouse, report_mouse_last;

static void ble_hog_set_key_mouse(uint16_t code, uint32_t value)
{
	uint8_t bit;

	switch (code) {
	case INPUT_BTN_LEFT:
		bit = 0;
		break;
	case INPUT_BTN_RIGHT:
		bit = 1;
		break;
	case INPUT_BTN_MIDDLE:
		bit = 2;
		break;
	default:
		return;
	}

	WRITE_BIT(report_mouse.buttons, bit, value);
}

static void ble_hog_set_rel(uint16_t code, int32_t value)
{
	switch (code) {
	case INPUT_REL_X:
		report_mouse.x = CLAMP(value, INT16_MIN, INT16_MAX);
		break;
	case INPUT_REL_Y:
		report_mouse.y = CLAMP(value, INT16_MIN, INT16_MAX);
		break;
	case INPUT_REL_WHEEL:
		report_mouse.wheel = CLAMP(value, INT8_MIN, INT8_MAX);;
		break;
	default:
		return;
	}
}

static void hog_input_cb(struct input_event *evt)
{
	if (evt->type == INPUT_EV_KEY) {
		ble_hog_set_key_mouse(evt->code, evt->value);
	} else if (evt->type == INPUT_EV_REL) {
		ble_hog_set_rel(evt->code, evt->value);
	} else {
		LOG_ERR("unrecognized event type: %x", evt->type);
	}

	if (!notify_enabled) {
		return;
	}

	if (!input_queue_empty()) {
		return;
	}

	if (memcmp(&report_mouse_last, &report_mouse, sizeof(report_mouse))) {
		bt_gatt_notify(NULL, &hog_svc.attrs[5],
			       &report_mouse, sizeof(report_mouse));
		report_mouse.x = 0;
		report_mouse.y = 0;
		report_mouse.wheel = 0;
		memcpy(&report_mouse_last, &report_mouse, sizeof(report_mouse));
	}
}
INPUT_CALLBACK_DEFINE(NULL, hog_input_cb);
