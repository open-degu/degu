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
#include <misc/printk.h>
#include <net/net_ip.h>
#include <net/net_if.h>
#include <net/socket.h>
#include <net/udp.h>
#include <net/coap.h>
#include <gpio.h>
#include <stdio.h>
#include <shell/shell.h>
#include "zcoap.h"
#include "degu_utils.h"
#include "degu_routing.h"
#include <string.h>
#include <openthread/udp.h>
#include <openthread/thread.h>
#include <openthread/message.h>
#include <openthread/ip6.h>

#define I2C
#include "libA71CH_api.h"

extern char *net_byte_to_hex(char *ptr, u8_t byte, char base, bool pad);
extern char *net_sprint_addr(sa_family_t af, const void *addr);
char gw_addr[NET_IPV6_ADDR_LEN] = "ff03::1";

static int Swap16(int before_swap)
{
	int after_swap;
	after_swap = ( (((before_swap) >> 8) & 0x00FF) | (((before_swap) << 8) & 0xFF00) );
	return after_swap;
}

void UDPmessageHandler(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo)
{
	char buf_ipv6[NET_IPV6_ADDR_LEN];
	int equal_cmp, length;
	char buf_mesg[256];
	length = otMessageRead(aMessage, otMessageGetOffset(aMessage), buf_mesg, sizeof(buf_mesg) - 1);
	buf_mesg[length] = '\0';
	equal_cmp = strcmp(buf_mesg, "degu::mcast");
	if (equal_cmp == 0) {
	        sprintf(buf_ipv6, "%x:%x:%x:%x:%x:%x:%x:%x",
                   Swap16(aMessageInfo->mPeerAddr.mFields.m16[0]),
                   Swap16(aMessageInfo->mPeerAddr.mFields.m16[1]),
                   Swap16(aMessageInfo->mPeerAddr.mFields.m16[2]),
                   Swap16(aMessageInfo->mPeerAddr.mFields.m16[3]),
                   Swap16(aMessageInfo->mPeerAddr.mFields.m16[4]),
                   Swap16(aMessageInfo->mPeerAddr.mFields.m16[5]),
                   Swap16(aMessageInfo->mPeerAddr.mFields.m16[6]),
                   Swap16(aMessageInfo->mPeerAddr.mFields.m16[7]));
		strcpy(gw_addr, buf_ipv6);
		printk("Passed gateway address success: %s\n", gw_addr);
	}
	OT_UNUSED_VARIABLE(aContext);
	OT_UNUSED_VARIABLE(aMessage);
	OT_UNUSED_VARIABLE(aMessageInfo);
}

void get_eui64(char *eui64)
{
	struct net_if *iface;
	char *buf = eui64;
	char byte;
	int i;

	iface = net_if_get_by_index(1);

	for (i = 0; i < 8; i++) {
		byte = (char)net_if_get_link_addr(iface)->addr[i];
		buf = net_byte_to_hex(buf, byte, 'A', true);
		(*buf)++;
	}
}
#if 0
static char *get_gw_addr(unsigned int prefix)
{
	struct net_if *iface;
	struct net_if_ipv6 *ipv6;
	struct net_if_addr *unicast;
	struct in6_addr gw_in6_addr;
	int i, j;

	iface = net_if_get_by_index(1);
	ipv6 = iface->config.ip.ipv6;

	for (i = 0; ipv6 && i < NET_IF_MAX_IPV6_ADDR; i++) {
		unicast = &ipv6->unicast[i];

		if (!unicast->is_used) {
			continue;
		}

		if (unicast->address.in6_addr.s6_addr[0] == 0xfd) {
			for (j = 0; j < 16; j++) {
				gw_in6_addr.s6_addr[j] = unicast->address.in6_addr.s6_addr[j];
				if (j >= prefix / 8) {
					gw_in6_addr.s6_addr[j] = 0x00;
				}
			}
			gw_in6_addr.s6_addr[15] += 1;
			return net_sprint_addr(AF_INET6, &gw_in6_addr);
		}
	}

	return NULL;
}
#endif

int degu_send_asset(void);
int degu_connect(void);

int degu_coap_request(u8_t *path, u8_t method, u8_t *payload, void (*callback)(u8_t *, u16_t))
{
	int sock;
	struct sockaddr_in6 sockaddr;
	u16_t payload_len;
	bool last_block = false;
	char eui64[17];
	char coap_path[40];
	int ret;
	int code = 0;

	while (!net_if_is_up(net_if_get_by_index(1)));

	sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_DTLS_1_2);
	if (sock == -1) {
		goto end;
	}

	sockaddr.sin6_family = AF_INET6;

	ret = zsock_inet_pton(AF_INET6, gw_addr, &sockaddr.sin6_addr);
	if (ret <= 0) {
		goto end;
	}

	sockaddr.sin6_port = htons(COAPS_PORT);
	ret = zsock_connect(sock, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
	if (ret < 0) {
		goto end;
	}

	get_eui64(eui64);
	eui64[16] = '\0';

	snprintf(coap_path, sizeof(coap_path), "%s/%s", path, eui64);

	while (1) {
		switch (method) {
		case COAP_METHOD_POST:
			payload_len = strlen((char*)payload);
			code = zcoap_request_post(sock, coap_path, payload, &payload_len, &last_block);
			break;
		case COAP_METHOD_PUT:
			payload_len = strlen((char*)payload);
			code = zcoap_request_put(sock, coap_path, payload, &payload_len, &last_block);
			break;
		case COAP_METHOD_GET:
			code = zcoap_request_get(sock, coap_path, payload, &payload_len, &last_block);
			break;
		case COAP_METHOD_DELETE:
			code = zcoap_request_delete(sock, coap_path);
		default:
			goto end;
		}

		/* Process by response code */
		switch (code) {
		case COAP_RESPONSE_CODE_VALID:
			/* GW is in progress */
			break;

		case COAP_RESPONSE_CODE_CONTENT:
			/* In progress of GET */
			if (callback != NULL) {
				/* Process a block of payload*/
				callback(payload, payload_len);
			}
			else {
				payload += 1024;
			}
			if (last_block) {
				goto end;
			}
			break;

		case COAP_RESPONSE_CODE_CONTINUE:
			/* In progress of PUT/POST */
			if (last_block) {
				goto end;
			}
			payload += 1024;
			break;

		case COAP_RESPONSE_CODE_UNAUTHORIZED:
			/* illigal or expired certificate */
		case COAP_RESPONSE_CODE_FORBIDDEN:
			/* invalid or duplex certificate */
			degu_get_asset();
			/* Need to send the asset again */
			code = degu_send_asset();
			if (code < COAP_RESPONSE_CODE_OK) {
				goto end;
			}
			/* Need to make connection again */
			degu_connect();
			code = COAP_FAILED_TO_RECEIVE_RESPONSE;
			goto end;
			break;

		case COAP_RESPONSE_CODE_BAD_REQUEST:
		case COAP_RESPONSE_CODE_INTERNAL_ERROR:
			degu_get_asset();
			/* Need to send the asset again */
			code = degu_send_asset();
			if (code < COAP_RESPONSE_CODE_OK) {
				goto end;
			}

		case COAP_RESPONSE_CODE_GATEWAY_TIMEOUT:
			/* Need to make connection again */
			code = degu_connect();
			if (code < COAP_RESPONSE_CODE_OK) {
				goto end;
			}
			break;

		case COAP_RESPONSE_CODE_NOT_FOUND:
			if (method == COAP_METHOD_GET) {
				if (strstr(path, "x509") != NULL) {
					/* end procedure, if gw has no degu asset. */
					goto end;
				} else if (strstr(path, "update") != NULL) {
					/* ota, bad url. */
					code = COAP_FAILED_TO_RECEIVE_RESPONSE;
					goto end;
				}
			}

			/* Need to get the asset from GW again */
			code = degu_get_asset();
			if (code < COAP_RESPONSE_CODE_OK) {
				goto end;
			}
			break;

		default:
			/* Complete or failed the operation */
			goto end;
		}
	}

end:
	close(sock);

	return code;
}

/**
 * check A71CH has asset.
 * @return	1:has asset, 0:no asset
 */
static int a71ch_has_asset(void)
{
	int hasAsset = 0;

	if (LIBA71CH_open() != 0) {
		return hasAsset;
	}

	if (LIBA71CH_hasKeyAndCert()) {
		hasAsset = 1;
	}

	LIBA71CH_finalize();

	return hasAsset;

}

/**
 * update asset on A71CH.
 * @return	0:success, -1:fail
 */
static int a71ch_update_asset(const char *key, const char *cert)
{
	int result = 0;
	int code_delete;
	if (LIBA71CH_open() != 0) {
		return -1;
	}

	if (LIBA71CH_eraseKeyAndCert() != 0) {
		result = -1;
		goto a71ch_end;
	}

	if (LIBA71CH_setKeyAndCert(key, cert)) {
		result = -1;
		goto a71ch_end;
	}

	/* send DELETE x509/key command, Degu GW delete key and cert both. */
	code_delete = degu_coap_request("x509/key", COAP_METHOD_DELETE, NULL, NULL);
	if (code_delete < COAP_RESPONSE_CODE_OK) {
		result = -1;
		goto a71ch_end;
	}

a71ch_end:
	LIBA71CH_finalize();
	return result;
}

int degu_get_asset(void)
{
	char *key;
	char *cert;
	int  code = 0;

	key = k_malloc(2048);
	cert = k_malloc(2048);

	memset(key, 0, 2048);
	memset(cert, 0, 2048);

	code = degu_coap_request("x509/key", COAP_METHOD_GET, key, NULL);
	if (code == COAP_RESPONSE_CODE_NOT_FOUND) {
		if (a71ch_has_asset()) {
			code = COAP_RESPONSE_CODE_CONTENT;
		}
		goto end;
	} else if (code < COAP_RESPONSE_CODE_OK) {
		goto end;
	}

	code = degu_coap_request("x509/cert", COAP_METHOD_GET, cert, NULL);
	if (code == COAP_RESPONSE_CODE_NOT_FOUND) {
		if (a71ch_has_asset()) {
			code = COAP_RESPONSE_CODE_CONTENT;
		}
		goto end;
	} else if (code < COAP_RESPONSE_CODE_OK) {
		goto end;
	}

	if (a71ch_update_asset(key, cert) != 0) {
		code = 0;
	}

end:
	k_free(key);
	k_free(cert);
	return code;
}

int degu_connect(void)
{
	return degu_coap_request("con/connection", COAP_METHOD_PUT, "", NULL);
}

int degu_send_asset(void)
{
	char *key;
	char *cert;
	/* char timeout[4]; */
	int  code = 0;

	key = k_malloc(4096);
	cert = k_malloc(4096);

	memset(key, 0, 4096);
	memset(cert, 0, 4096);

	if (LIBA71CH_open() != 0) {
		goto end;
	}

	if (LIBA71CH_getKeyAndCert(key, cert) != 0) {
		LIBA71CH_finalize();
		goto end;
	}

	LIBA71CH_finalize();

	/* strcpy(timeout, DEGU_TEST_TIMEOUT_SEC); */

	code = degu_coap_request("con/key", COAP_METHOD_PUT, key, NULL);
	if (code < COAP_RESPONSE_CODE_OK) {
		goto end;
	}
	code = degu_coap_request("con/cert", COAP_METHOD_PUT, cert, NULL);
	if (code < COAP_RESPONSE_CODE_OK) {
		goto end;
	}
	/* do not send timeout.
	   If the shadow update interval is longer than this value,
	   TLS communication between the Degu gateway and AWS is disconnected.
	   It takes about 4 seconds to reconnect the connection.
	   In the future, it will be possible to set the timeout value variably.
	code = degu_coap_request("con/timeout", COAP_METHOD_PUT, timeout, NULL);
	if (code < COAP_RESPONSE_CODE_OK){
		goto end;
	}
	*/

end:
	k_free(key);
	k_free(cert);
	return code;
}

void openthread_join_success_handler(struct k_work *work)
{
	struct device *gpio1 = device_get_binding(DT_GPIO_P1_DEV_NAME);

	degu_get_asset();

	gpio_pin_configure(gpio1, 7, GPIO_DIR_OUT);
	gpio_pin_write(gpio1, 7, 0);
}
