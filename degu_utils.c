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
#include <net/net_ip.h>
#include <net/net_if.h>
#include <net/socket.h>
#include <net/udp.h>
#include <net/coap.h>
#include <gpio.h>
#include <stdio.h>
#include <shell/shell.h>
#include "zcoap.h"
#include "degu_test.h"

extern char *net_byte_to_hex(char *ptr, u8_t byte, char base, bool pad);
extern char *net_sprint_addr(sa_family_t af, const void *addr);

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

char *get_gw_addr(unsigned int prefix)
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

void degu_get_asset(void);
void degu_send_asset(void);
void degu_connect(void);

int degu_coap_request(u8_t *path, u8_t method, u8_t *payload, void (*callback)(u8_t *buf, u16_t size))
{
	int sock;
	struct sockaddr_in6 sockaddr;
	u16_t payload_len;
	bool last_block = false;
	char gw_addr[NET_IPV6_ADDR_LEN];
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

	strcpy(gw_addr, get_gw_addr(64));
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
			payload_len = strlen(payload);
			code = zcoap_request_post(sock, coap_path, payload, &payload_len, &last_block);
			break;
		case COAP_METHOD_PUT:
			payload_len = strlen(payload);
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
			/* In progress of GET/PUT/POST */
			if (callback != NULL) {
				/* Process a block of payload*/
				callback(payload, payload_len);
			}
		case COAP_RESPONSE_CODE_CONTINUE:
			if (last_block) {
				goto end;
			}
			else {
				payload += 1024;
			}
			break;

		case COAP_RESPONSE_CODE_BAD_REQUEST:
		case COAP_RESPONSE_CODE_INTERNAL_ERROR:
			/* Need to send the asset again */
			degu_send_asset();
		case COAP_RESPONSE_CODE_GATEWAY_TIMEOUT:
			/* Need to make connection again */
			degu_connect();
			break;

		case COAP_RESPONSE_CODE_NOT_FOUND:
			/* Need to get the asset from GW again */
			degu_get_asset();
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

void degu_get_asset(void)
{
	char *key;
	char *cert;

	key = k_malloc(2048);
	cert = k_malloc(2048);

	/* At first, we must erase A71CH in here*/

	degu_coap_request("x509/key", COAP_METHOD_GET, key, NULL);
	/* Write the key to A71CH in here */

	degu_coap_request("x509/cert", COAP_METHOD_GET, cert, NULL);
	/* Write the cert to A71CH in here */

	k_free(key);
	k_free(cert);
}

void degu_connect(void)
{
	degu_coap_request("con/connection", COAP_METHOD_PUT, "", NULL);
}

void degu_send_asset(void)
{
	char *key;
	char *cert;
	char timeout[4];

	key = k_malloc(4096);
	cert = k_malloc(4096);

	strcpy(key, DEGU_TEST_KEY);
	strcpy(cert, DEGU_TEST_CERT);
	strcpy(timeout, DEGU_TEST_TIMEOUT_SEC);

	degu_coap_request("con/key", COAP_METHOD_PUT, key, NULL);
	degu_coap_request("con/cert", COAP_METHOD_PUT, cert, NULL);
	degu_coap_request("con/timeout", COAP_METHOD_PUT, timeout, NULL);

	k_free(key);
	k_free(cert);
}

void openthread_join_success_handler(struct k_work *work)
{
	struct device *gpio1 = device_get_binding(DT_GPIO_P1_DEV_NAME);

	degu_get_asset();

	gpio_pin_configure(gpio1, 7, GPIO_DIR_OUT);
	gpio_pin_write(gpio1, 7, 0);
}
