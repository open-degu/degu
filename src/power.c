/*
 * Copyright (c) 2018 Nordic Semiconductor ASA.
 * Copyright (c) 2019 Atmark Techno, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */



#include <clock_control.h>
#include <gpio.h>

void device_power(bool enable)
{
	struct device *gpio0 = device_get_binding(DT_GPIO_P0_DEV_NAME);
	struct device *gpio1 = device_get_binding(DT_GPIO_P1_DEV_NAME);

	if (enable) {
		gpio_pin_configure(gpio0, 26, GPIO_DIR_OUT|GPIO_PUD_PULL_DOWN);
		gpio_pin_write(gpio0, 26, 0);
		gpio_pin_configure(gpio1, 2, GPIO_DIR_OUT|GPIO_PUD_PULL_DOWN);
		gpio_pin_write(gpio1, 2, 0);
		gpio_pin_configure(gpio1, 6, GPIO_DIR_OUT|GPIO_PUD_PULL_DOWN);
		gpio_pin_write(gpio1, 6, 0);
	} else {
		gpio_pin_configure(gpio0, 26, GPIO_DIR_OUT|GPIO_PUD_PULL_UP);
		gpio_pin_write(gpio0, 26, 1);
		gpio_pin_configure(gpio1, 2, GPIO_DIR_OUT|GPIO_PUD_PULL_UP);
		gpio_pin_write(gpio1, 2, 1);
		gpio_pin_configure(gpio1, 6, GPIO_DIR_OUT|GPIO_PUD_PULL_UP);
		gpio_pin_write(gpio1, 6, 1);
	}
}

void sys_pm_notify_lps_entry(enum power_states state)
{
	switch (state) {
	case SYS_POWER_STATE_CPU_LPS_1:
		device_power(false);
		k_cpu_idle();
		break;

	case SYS_POWER_STATE_CPU_LPS_2:
		device_power(false);
		k_cpu_idle();
		break;

	case SYS_POWER_STATE_DEEP_SLEEP:
		device_power(false);
		break;

	default:
		break;
	}
}

void sys_pm_notify_lps_exit(enum power_states state)
{
	switch (state) {
	case SYS_POWER_STATE_CPU_LPS_1:
		device_power(true);
		break;

	case SYS_POWER_STATE_CPU_LPS_2:
		device_power(true);
		break;

	case SYS_POWER_STATE_DEEP_SLEEP:
		device_power(true);
		break;

	default:
		break;
	}
}
