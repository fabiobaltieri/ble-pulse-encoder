#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(charging_led, LOG_LEVEL_INF);

static const struct device *leds = DEVICE_DT_GET_ONE(gpio_leds);

const struct gpio_dt_spec chrg = GPIO_DT_SPEC_GET( DT_PATH(zephyr_user), chrg_gpios);

#define LED_CHARGING DT_NODE_CHILD_IDX(DT_NODELABEL(charging))

static void charging_led_handler(struct k_work *work);

K_WORK_DELAYABLE_DEFINE(charging_led_dwork, charging_led_handler);

static void charging_led_handler(struct k_work *work)
{
	if (gpio_pin_get_dt(&chrg)) {
		led_on(leds, LED_CHARGING);
	} else {
		led_off(leds, LED_CHARGING);
	}

	k_work_schedule(&charging_led_dwork, K_MSEC(500));
}

static int charging_led_init(void)
{
	if (!gpio_is_ready_dt(&chrg)) {
		LOG_ERR("chrg device is not ready");
		return -ENODEV;
	}

	if (!device_is_ready(leds)) {
		LOG_ERR("leds device is not ready");
		return -ENODEV;
	}

	gpio_pin_configure_dt(&chrg, GPIO_INPUT);

	charging_led_handler(&charging_led_dwork.work);

	return 0;
}
SYS_INIT(charging_led_init, APPLICATION, 50);
