/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
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
#define DEBUG_TOOL
#define MAX_LCD_ONELINE 17
#define MAX_DATA_INDEX 6

#ifdef DEBUG_TOOL

#include <zephyr.h>
#include <console.h>
#include <gpio.h>
#include "zephyr_getchar.h"
#include "../degu_utils.h"
#include "../zephyr/include/display/grove_lcd.h"
#include <shell/shell.h>

#include <net/openthread.h>

#include <openthread/cli.h>
#include <openthread/ip6.h>
#include <openthread/link.h>
#include <openthread/message.h>
#include <openthread/tasklet.h>
#include <openthread/thread.h>
#include <openthread/dataset.h>
#include <openthread/joiner.h>
#include <openthread-system.h>
#include <init.h>

#include <logging/log.h>
#include <openthread/instance.h>

otInstance *mOtInstance;
bool is_pressed = false;
bool is_start = false;
struct device *gpio1;
struct device *gpio0;

char gw_addr[NET_IPV6_ADDR_LEN] = {0};
char print_information[MAX_DATA_INDEX][MAX_LCD_ONELINE] = {0};
struct device *glcd;

int shell_cmds_init(otInstance *aInstance)
{
	mOtInstance = aInstance;
}

static void lcd_print_string(char *str1 , char *str2)
{
	glcd_clear(glcd);
	glcd_cursor_pos_set(glcd, 0, 0);
	if (str1 != NULL) {
		glcd_print(glcd, str1, strlen(str1));
	}
	glcd_cursor_pos_set(glcd, 0, 1);
	if (str1 != NULL) {
		glcd_print(glcd, str2, strlen(str2));
	}
}

static void display_lcd(int8_t index)
{
	glcd_clear(glcd);
	glcd_cursor_pos_set(glcd, 0, 0);
	glcd_print(glcd, print_information[index], strlen(print_information[index]));
	glcd_cursor_pos_set(glcd, 0, 1);
	if(MAX_DATA_INDEX > (index + 1)) {
		glcd_print(glcd, print_information[index + 1], strlen(print_information[index + 1]));
	} else {
		// glcd_print(glcd, print_information[0], strlen(print_information[0]));
	}

	return;
}

static void display_shell_print(const struct shell *shell)
{
	for(int i = 0 ; i < MAX_DATA_INDEX ; i++){
		shell_print(shell, "%s", print_information[i]);
	}
	return;
}

static int debug_tool(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	int8_t tx_power, average_rssi, last_rssi;
	otRouterInfo parent_info;
	otExtAddress ext_address;
	otError error;

	// Get RSSI value
	error = otThreadGetParentInfo(mOtInstance, &parent_info);
	if (error == OT_ERROR_NONE)
	{
		otThreadGetParentAverageRssi(mOtInstance, &average_rssi);
		snprintk(print_information[0], MAX_LCD_ONELINE, "AvgRSSI%6ddBm", average_rssi);
		error = otThreadGetParentLastRssi(mOtInstance, &last_rssi);
		if (error == OT_ERROR_NONE) {
			snprintk(print_information[1], MAX_LCD_ONELINE, "LastRSSI%5ddBm", last_rssi);
		}
	}
	// Get Ping value
	strcpy(gw_addr, get_gw_addr(64));

	// Get TX Power value
	error = otPlatRadioGetTransmitPower(mOtInstance, &tx_power);
	if (error == OT_ERROR_NONE) {
		snprintk(print_information[2], MAX_LCD_ONELINE, "txpower%6ddBm", tx_power);
	}

	// Get MAC address
	snprintk(print_information[3], MAX_LCD_ONELINE, "rloc16      %04x", otThreadGetRloc16(mOtInstance));

	// Get Channel
	uint8_t channel = otLinkGetChannel(mOtInstance);
	snprintk(print_information[4], MAX_LCD_ONELINE, "channel%9d", channel);

	if (shell != NULL) {
		display_shell_print(shell);
	}

	// Get State
	switch (otThreadGetDeviceRole(mOtInstance))
	{
		case OT_DEVICE_ROLE_DISABLED:
			snprintk(print_information[5], MAX_LCD_ONELINE, "state   disabled" );
			break;
		case OT_DEVICE_ROLE_DETACHED:
			snprintk(print_information[5], MAX_LCD_ONELINE, "state   detached");
			break;
		case OT_DEVICE_ROLE_CHILD:
			snprintk(print_information[5], MAX_LCD_ONELINE, "state      child");
			break;
#if OPENTHREAD_FTD
		case OT_DEVICE_ROLE_ROUTER:
			snprintk(print_information[5], MAX_LCD_ONELINE, "state     router");
			break;
		case OT_DEVICE_ROLE_LEADER:
			snprintk(print_information[5], MAX_LCD_ONELINE, "state     leader");
			break;
#endif // OPENTHREAD_FTD
		default:
			snprintk(print_information[5], MAX_LCD_ONELINE, "state%11s", "invalid");
			snprintk(print_information[5], MAX_LCD_ONELINE, "state    invalid");
			break;
	}

	return 0;
}

void repeat_debug_tool(void)
{
	const struct shell *shell;
	size_t argc = 0;
	char **argv = NULL;
	u32_t sw1,sw2;
	u32_t start = 0;
	u8_t set_config;
	int8_t count = 0;
	bool is_first = true;

	glcd = device_get_binding(GROVE_LCD_NAME);
	if (!glcd) {
		printk("Grove LCD: Device not found.\n");
		return;
	}

	/* Now configure the LCD the way we want it */
	set_config = GLCD_FS_ROWS_2
		     | GLCD_FS_DOT_SIZE_LITTLE
		     | GLCD_FS_8BIT_MODE;
	glcd_function_set(glcd, set_config);
	set_config = GLCD_DS_DISPLAY_ON | GLCD_DS_CURSOR_OFF | GLCD_DS_BLINK_OFF;
	glcd_display_state_set(glcd, set_config);
	lcd_print_string("Please push the", " Start button");

	gpio0 = device_get_binding(DT_GPIO_P0_DEV_NAME);
	gpio1 = device_get_binding(DT_GPIO_P1_DEV_NAME);
	// gpio_pin_configure(gpio1, 14, GPIO_DIR_IN | GPIO_PUD_PULL_UP);
	gpio_pin_configure(gpio0, 6, GPIO_DIR_IN | GPIO_PUD_PULL_DOWN);
	gpio_pin_configure(gpio0, 8, GPIO_DIR_IN | GPIO_PUD_PULL_DOWN);
	gpio_pin_configure(gpio1, 7, GPIO_DIR_OUT);
	gpio_pin_write(gpio1, 7, 1);

	while (1) {
		// check start button
		gpio_pin_read(gpio0, 8, &sw1);
		if ( sw1 != 0 ) {
			if(!is_pressed && is_start){
				is_start = false;
				lcd_print_string("  Stop Analys", " ");
			} else {
				is_start = true;
				start = k_cycle_get_32();
				lcd_print_string("  Start Analys"," ");
			}
			is_first = true;
			is_pressed = true;
			k_sleep(K_MSEC(400));
		} else {
			is_pressed = false;
		}
		// check change display button
		gpio_pin_read(gpio0, 6, &sw2);
		if ( sw2 != 0 && !is_first) {
			display_lcd(count);
			if (count < MAX_DATA_INDEX - 1){
				count = count + 2;
			} else {
				count = 0;
			}
			k_sleep(K_MSEC(400));
		}
		k_sleep(K_MSEC(100));
		// run debug tool
		if ( ( start + (65000 / 2) ) <= k_cycle_get_32() && is_start ) {
			gpio_pin_write(gpio1, 7, 0); // LED1 ON
		}
		if ( ( start + 65000 ) <= k_cycle_get_32() ) {
			if ( is_start ){
				debug_tool(shell, argc, argv);
			}
			if ( is_first ) {
				display_lcd(0);
				is_first = false;
			}
			gpio_pin_write(gpio1, 7, 1); // LED1 OFF
			start = k_cycle_get_32();
		}
	}
}

K_THREAD_DEFINE(repeat_debug, 1024, repeat_debug_tool, NULL, NULL, NULL, 7, 0, K_NO_WAIT);
SHELL_CMD_REGISTER(debug_tool, NULL, "Debug tool to display Thread network information`", debug_tool);
#endif
