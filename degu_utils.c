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
#include <openthread/ip6.h>

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

#define I2C
#include "libA71CH_api.h"

extern char *net_byte_to_hex(char *ptr, u8_t byte, char base, bool pad);
extern char *net_sprint_addr(sa_family_t af, const void *addr);

static const unsigned int CERT_SIZE = 2048;

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
int degu_send_asset(u8_t *zcoap_buf);
int degu_connect(u8_t *zcoap_buf);

int degu_coap_request(u8_t *path, u8_t method, u8_t *payload,
			void (*callback)(u8_t *, u16_t), u8_t *zcoap_buf)
{
	int sock;
	struct sockaddr_in6 sockaddr;
	u16_t payload_len;
	bool last_block = false;
	char eui64[17];
	char coap_path[40];
	int ret;
	int code = 0;
	u8_t *zcoap_buf_tmp = NULL;

	while (!net_if_is_up(net_if_get_by_index(1)));

	sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_DTLS_1_2);
	if (sock == -1) {
		goto end;
	}

	sockaddr.sin6_family = AF_INET6;

	//strcpy(gw_addr, get_gw_addr(64));
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

	if (!zcoap_buf) {
		zcoap_buf_tmp = degu_utils_k_malloc(MAX_COAP_MSG_LEN);
		if (!zcoap_buf_tmp) {
			goto end;
		}
		zcoap_buf = zcoap_buf_tmp;
	}

	while (1) {
		switch (method) {
		case COAP_METHOD_POST:
			payload_len = strlen((char*)payload);
			code = zcoap_request_post(sock, coap_path, payload,
					&payload_len, &last_block, zcoap_buf);
			break;
		case COAP_METHOD_PUT:
			payload_len = strlen((char*)payload);
			code = zcoap_request_put(sock, coap_path, payload,
					&payload_len, &last_block, zcoap_buf);
			break;
		case COAP_METHOD_GET:
			code = zcoap_request_get(sock, coap_path, payload,
					&payload_len, &last_block, zcoap_buf);
			break;
		case COAP_METHOD_DELETE:
			code = zcoap_request_delete(sock, coap_path, zcoap_buf);
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
			degu_get_asset(true, zcoap_buf);
			/* Need to send the asset again */
			code = degu_send_asset(zcoap_buf);
			if (code < COAP_RESPONSE_CODE_OK) {
				goto end;
			}
			/* Need to make connection again */
			degu_connect(zcoap_buf);
			code = COAP_FAILED_TO_RECEIVE_RESPONSE;
			goto end;
			break;

		case COAP_RESPONSE_CODE_BAD_REQUEST:
		case COAP_RESPONSE_CODE_INTERNAL_ERROR:
			degu_get_asset(true, zcoap_buf);
			/* Need to send the asset again */
			code = degu_send_asset(zcoap_buf);
			if (code < COAP_RESPONSE_CODE_OK) {
				goto end;
			}

		case COAP_RESPONSE_CODE_GATEWAY_TIMEOUT:
			/* Need to make connection again */
			code = degu_connect(zcoap_buf);
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
			code = degu_get_asset(false, zcoap_buf);
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
	if (zcoap_buf_tmp) {
		degu_utils_k_free(zcoap_buf_tmp);
	}
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
static int a71ch_update_asset(const char *key, const char *cert,
							u8_t *zcoap_buf)
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
	code_delete = degu_coap_request("x509/key", COAP_METHOD_DELETE,
							NULL, NULL, zcoap_buf);
	if (code_delete < COAP_RESPONSE_CODE_OK) {
		result = -1;
		goto a71ch_end;
	}

a71ch_end:
	LIBA71CH_finalize();
	return result;
}

static char *_key = NULL;
static char *_cert = NULL;

void free_asset_buf()
{
	if (_key)
		degu_utils_k_free(_key);
	if (_cert)
		degu_utils_k_free(_cert);

	_key = NULL;
	_cert = NULL;
}

bool malloc_asset_buf()
{
	if (!_key)
		_key = degu_utils_k_malloc(CERT_SIZE);

	if (!_cert)
		_cert = degu_utils_k_malloc(CERT_SIZE);

	if (!_key || !_cert) {
		free_asset_buf();
		return false;
	}

	memset(_key, 0, CERT_SIZE);
	memset(_cert, 0, CERT_SIZE);

	return true;
}

int degu_get_asset(bool nextSetAsset, u8_t *zcoap_buf)
{
	int code = 0;
	u8_t *zcoap_buf_tmp = NULL;

	if (!zcoap_buf) {
		zcoap_buf_tmp = degu_utils_k_malloc(MAX_COAP_MSG_LEN);
		if (!zcoap_buf_tmp)
			goto end;

		zcoap_buf = zcoap_buf_tmp;
	}

	if (!malloc_asset_buf())
		goto end;

	code = degu_coap_request("x509/key", COAP_METHOD_GET, _key, NULL,
								zcoap_buf);
	if (code == COAP_RESPONSE_CODE_NOT_FOUND) {
		if (a71ch_has_asset())
			code = COAP_RESPONSE_CODE_CONTENT;
		goto end;
	} else if (code < COAP_RESPONSE_CODE_OK)
		goto end;

	code = degu_coap_request("x509/cert", COAP_METHOD_GET, _cert, NULL,
								zcoap_buf);
	if (code == COAP_RESPONSE_CODE_NOT_FOUND) {
		if (a71ch_has_asset())
			code = COAP_RESPONSE_CODE_CONTENT;
		goto end;
	} else if (code < COAP_RESPONSE_CODE_OK)
		goto end;

	if (a71ch_update_asset(_key, _cert, zcoap_buf) != 0)
		code = 0;

end:
	if (!nextSetAsset )
		free_asset_buf();

	if (zcoap_buf_tmp)
		degu_utils_k_free(zcoap_buf_tmp);

	return code;
}

int degu_connect(u8_t *zcoap_buf)
{
	return degu_coap_request("con/connection", COAP_METHOD_PUT, "",
							NULL, zcoap_buf);
}

int degu_send_asset(u8_t *zcoap_buf)
{
	/* char timeout[4]; */
	int code = 0;

	if (!malloc_asset_buf())
		return code;

	if (LIBA71CH_open() != 0)
		goto end;

	if (LIBA71CH_getKeyAndCert(_key, _cert) != 0) {
		LIBA71CH_finalize();
		goto end;
	}

	LIBA71CH_finalize();

	/* strcpy(timeout, DEGU_TEST_TIMEOUT_SEC); */

	code = degu_coap_request("con/key", COAP_METHOD_PUT, _key,
							NULL, zcoap_buf);
	if (code < COAP_RESPONSE_CODE_OK)
		goto end;

	code = degu_coap_request("con/cert", COAP_METHOD_PUT, _cert,
							NULL, zcoap_buf);
	if (code < COAP_RESPONSE_CODE_OK)
		goto end;

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
	free_asset_buf();
	return code;
}

void *degu_utils_k_malloc(size_t size)
{
	if (size < (CONFIG_HEAP_MEM_POOL_SIZE / 64) ) {
		size = (CONFIG_HEAP_MEM_POOL_SIZE / 64);
		size += (4 - size % 4);
	}
	return k_malloc(size);
}

void degu_utils_k_free(void *ptr)
{
	k_free(ptr);
}

void openthread_join_success_handler(struct k_work *work)
{
	struct device *gpio1 = device_get_binding(DT_GPIO_P1_DEV_NAME);

	gpio_pin_configure(gpio1, 7, GPIO_DIR_OUT);
	gpio_pin_write(gpio1, 7, 0);
}
