/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Atmark Techno, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <power.h>
#include <gpio.h>

#include <openthread/thread.h>
#include <openthread/platform/radio.h>

void degu_ext_device_power(bool enable)
{
	struct device *gpio0 = device_get_binding(DT_GPIO_P0_DEV_NAME);
	struct device *gpio1 = device_get_binding(DT_GPIO_P1_DEV_NAME);
	struct device *i2c0 = device_get_binding(DT_I2C_0_NAME);
	struct device *i2c1 = device_get_binding(DT_I2C_1_NAME);

	if (enable) {
		gpio_pin_configure(gpio1, 6, GPIO_DIR_OUT|GPIO_PUD_PULL_UP);
		gpio_pin_write(gpio1, 6, 1);
		gpio_pin_configure(gpio1, 2, GPIO_DIR_OUT|GPIO_PUD_PULL_UP);
		gpio_pin_write(gpio1, 2, 1);
		gpio_pin_configure(gpio0, 26, GPIO_DIR_OUT|GPIO_PUD_PULL_UP);
		gpio_pin_write(gpio0, 26, 1);

		device_set_power_state(i2c1, DEVICE_PM_ACTIVE_STATE, NULL, NULL);
		device_set_power_state(i2c0, DEVICE_PM_ACTIVE_STATE, NULL, NULL);
	} else {
		device_set_power_state(i2c0, DEVICE_PM_SUSPEND_STATE, NULL, NULL);
		device_set_power_state(i2c1, DEVICE_PM_SUSPEND_STATE, NULL, NULL);

		gpio_pin_configure(gpio0, 26, GPIO_DIR_OUT|GPIO_PUD_PULL_DOWN);
		gpio_pin_write(gpio0, 26, 0);
		gpio_pin_configure(gpio1, 2, GPIO_DIR_OUT|GPIO_PUD_PULL_DOWN);
		gpio_pin_write(gpio1, 2, 0);
		gpio_pin_configure(gpio1, 6, GPIO_DIR_OUT|GPIO_PUD_PULL_DOWN);
		gpio_pin_write(gpio1, 6, 0);

		gpio_pin_configure(gpio0, 24, GPIO_DIR_IN | GPIO_PUD_PULL_DOWN);
		gpio_pin_write(gpio0, 24, 0);
		gpio_pin_configure(gpio0, 25, GPIO_DIR_IN | GPIO_PUD_PULL_DOWN);
		gpio_pin_write(gpio0, 25, 0);
	}
}

void openthread_suspend(otInstance *aInstance)
{
    otThreadSetEnabled(aInstance, false);
    otIp6SetEnabled(aInstance, false);
    otPlatRadioSleep(aInstance);
    otPlatRadioDisable(aInstance);
}

void openthread_resume(otInstance *aInstance, uint8_t aChannel, otLinkModeConfig aConfig)
{
    otPlatRadioEnable(aInstance);
    otPlatRadioReceive(aInstance, aChannel);
    otThreadSetLinkMode(aInstance, aConfig);
    otIp6SetEnabled(aInstance, true);
    otThreadSetEnabled(aInstance, true);
    while(otThreadGetDeviceRole(aInstance) <= OT_DEVICE_ROLE_DETACHED)
    {}
}
