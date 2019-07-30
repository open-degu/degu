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
#include <errno.h>
#include <zephyr.h>
#include "py/nlr.h"
#include "py/obj.h"
#include "py/runtime.h"
#include "py/binary.h"

#include <stdio.h>
#include <string.h>

#include <net/socket.h>
#include <net/net_mgmt.h>
#include <net/net_ip.h>
#include <net/udp.h>
#include <net/coap.h>

#define MAX_COAP_MSG_LEN 1152
#define COAP_TYPE_CON 0 //Confirmable
#define COAP_TYPE_NCON 1 //non-Confirmable
#define COAP_TYPE_ACK 2 //Acknowledgement
#define COAP_TYPE_RST 3 //Reset

static u8_t *zcoap_request(int sock, u8_t *path, u8_t method, u8_t *payload_send, u16_t *payload_len)
{
	int r;
	int rcvd;
	struct coap_packet request;
	struct coap_packet reply;
	static u8_t str_code[5];
	const u8_t *payload_recv;
	u8_t *data;
	u8_t code;

	data = (u8_t *)k_malloc(MAX_COAP_MSG_LEN);
	if (!data) {
		printf("can't malloc\n");
		return NULL;
	}

	r = coap_packet_init(&request, data, MAX_COAP_MSG_LEN,
			     1, COAP_TYPE_CON, 8, coap_next_token(),
			     method, coap_next_id());
	if (r < 0) {
		printf("Unable to init CoAP packet\n");
		goto end;
	}

	r = coap_packet_append_option(&request, COAP_OPTION_URI_PATH, path, strlen(path));
	if (r < 0) {
		printf("Unable to add option to request\n");
		goto end;
	}

	if (method != COAP_METHOD_GET) {
		r = coap_packet_append_payload_marker(&request);
		if (r < 0) {
			printf("Unable to append payload maker\n");
			goto end;
		}
		r = coap_packet_append_payload(&request, payload_send, strlen(payload_send));
		if (r < 0) {
			printf("Unable to append payload\n");
			goto end;
		}
	}

	r = zsock_send(sock, request.data, request.offset, 0);
	if (r < 0) {
		printf("Unable to send request\n");
		goto end;
	}

	rcvd = zsock_recv(sock, data, MAX_COAP_MSG_LEN, 0);
	if (rcvd <= 0) {
		printf("Unable to receive response\n");
		goto end;
	}

	r = coap_packet_parse(&reply, data, rcvd, NULL, 0);
	if (r < 0) {
		printf("Unable to parse recieved packet\n");
		goto end;
	}

	if (method == COAP_METHOD_GET) {
		payload_recv = coap_packet_get_payload(&reply, payload_len);
		k_free(data);
		return (u8_t *)payload_recv;
	}
	else {
		code = coap_header_get_code(&reply);
		sprintf(str_code, "%d.%02d", code/32, code%32);
		k_free(data);
		return str_code;
	}

end:
	k_free(data);
	return NULL;
}

u8_t *zcoap_request_post(int sock, u8_t *path, u8_t *payload)
{
	return zcoap_request(sock, path, COAP_METHOD_POST, payload, NULL);
}

u8_t *zcoap_request_put(int sock, u8_t *path, u8_t *payload)
{
	return zcoap_request(sock, path, COAP_METHOD_PUT, payload, NULL);
}

u8_t *zcoap_request_get(int sock, u8_t *path, u16_t *payload_len)
{
	return zcoap_request(sock, path, COAP_METHOD_GET, NULL, payload_len);
}
