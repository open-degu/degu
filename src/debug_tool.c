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
#include <zephyr.h>
#include <console.h>
#include "zephyr_getchar.h"
#include "../degu_utils.h"
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

static int debug_tool(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	int8_t tx_pwr, prt_avrg_rssi, prt_last_rssi;
	otRouterInfo parent_info;
	otExtAddress ext_address;
	otError ret;

	char rloc16[5] = {0};
	char tx_power[5] = {0};
	char average_rssi[5] = {0};
	char last_rssi[5] = {0};
	char mac_addr[(OT_EXT_ADDRESS_SIZE * 2) + 1] = {0};

	// RSSI
	otThreadGetParentAverageRssi(mOtInstance, &prt_avrg_rssi);
	if (prt_avrg_rssi != NULL) {
		snprintk(average_rssi, 5, "%d", prt_avrg_rssi);
		shell_print(shell, "Acerage Rssi = %s dBm", average_rssi);
	}
	ret = otThreadGetParentLastRssi(mOtInstance, &prt_last_rssi);
	if (ret == OT_ERROR_NONE) {
		snprintk(last_rssi, 5, "%d", prt_last_rssi);
		shell_print(shell, "Last Rssi = %s dBm", last_rssi);
	} else {
		shell_print(shell, "Last Rssi = error : \"Unable to get RSSI data.\" or"
		" \"aLastRssi is NULL.\" ret = %d",ret);
	}
	// Ping
	char gw_addr[NET_IPV6_ADDR_LEN];
	strcpy(gw_addr, get_gw_addr(64));
	shell_print(shell, "gateway address %s", gw_addr);

	// TX Power
	ret = otPlatRadioGetTransmitPower(mOtInstance, &tx_pwr);
	if (ret == OT_ERROR_NONE) {
		snprintk(tx_power, 5, "%d", tx_pwr);
		shell_print(shell, "tx power = %s dBm", tx_power);
	} else {
		shell_print(shell, "rx power = error : \"aPower was NULL.\" or"
		" \"Transmit power configuration via dBm is not implemented.\" ret = %d",ret);
	}
	// MAC
	otLinkGetFactoryAssignedIeeeEui64(mOtInstance, &ext_address);
	shell_print(shell, "EUI64 = ");
	// for (int i = 0; i < OT_EXT_ADDRESS_SIZE; i++) {
	//  shell_print(shell, "%02x", ext_address.m8[i]);
	// }
	// snprintk(mac_addr, 17, "%016x", ext_address.m8);

	snprintk(rloc16, 5, "%04x", otThreadGetRloc16(mOtInstance));
	shell_print(shell, "rloc16 = %s", rloc16);
	// TODO : I2C LCD

	return 0;
}

SHELL_CMD_REGISTER(debug_tool, NULL, "Debug tool to display Thread network information`", debug_tool);
