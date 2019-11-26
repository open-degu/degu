/*
 * Copyright (c) 2018 Nordic Semiconductor ASA.
 * Copyright (c) 2019 Atmark Techno, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <power.h>
#include "../degu_pm.h"

void device_power(bool enable)
{
	if (enable) {
		sys_pm_resume_devices();
		degu_ext_device_power(true);
	} else {
		degu_ext_device_power(false);
		sys_pm_suspend_devices();
	}
}

void sys_pm_notify_power_state_entry(enum power_states state)
{
	switch (state) {
	case SYS_POWER_STATE_SLEEP_2:
		device_power(false);
		k_cpu_idle();
		break;

	case SYS_POWER_STATE_SLEEP_3:
		device_power(false);
		k_cpu_idle();
		break;

	case SYS_POWER_STATE_DEEP_SLEEP_1:
		device_power(false);
		break;

	default:
		break;
	}
}

void sys_pm_notify_power_state_exit(enum power_states state)
{
	switch (state) {
	case SYS_POWER_STATE_SLEEP_2:
		device_power(true);
		break;

	case SYS_POWER_STATE_SLEEP_3:
		device_power(true);
		break;

	case SYS_POWER_STATE_DEEP_SLEEP_1:
		device_power(true);
		break;

	default:
		break;
	}
}
