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

#include "zcoap.h"

static struct coap_block_context blk_ctx;

static u8_t token[8];

static u8_t zcoap_request(int sock, u8_t *path, u8_t method, u8_t *payload, u16_t *payload_len, bool *last_block)
{
	int r;
	int rcvd;
	fd_set fds;
	struct coap_packet request;
	struct coap_packet reply;
	u8_t *data;
	const u8_t *payload_buf;
	u8_t code;
	struct timeval tv;

	if (blk_ctx.total_size == 0) {
		coap_block_transfer_init(&blk_ctx, COAP_BLOCK_1024,
					BLOCK_WISE_TRANSFER_SIZE_GET);
		memcpy(token, coap_next_token(), sizeof(token));
	}

	data = (u8_t *)k_malloc(MAX_COAP_MSG_LEN);
	if (!data) {
		printf("can't malloc\n");
		return 0;
	}

	r = coap_packet_init(&request, data, MAX_COAP_MSG_LEN,
			     1, COAP_TYPE_CON, sizeof(token), token,
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

	if (method == COAP_METHOD_GET) {
		r = coap_append_block2_option(&request, &blk_ctx);
		if (r < 0) {
			printf("Unable to add option to request\n");
			goto end;
		}
	}
	else {
		r = coap_packet_append_payload_marker(&request);
		if (r < 0) {
			printf("Unable to append payload maker\n");
			goto end;
		}
		r = coap_packet_append_payload(&request, payload, strlen(payload));
		if (r < 0) {
			printf("Unable to append payload\n");
			goto end;
		}
	}

	r = send(sock, request.data, request.offset, 0);
	if (r < 0) {
		printf("Unable to send request\n");
		goto end;
	}

	FD_ZERO(&fds);
	FD_SET(sock, &fds);
	tv.tv_sec = 3;
	tv.tv_usec = 0;

	r = select(sock + 1, &fds, NULL, NULL, &tv);
	if (!r) {
		printf("Receiving response timeout\n");
		goto end;
	}

	rcvd = recv(sock, data, MAX_COAP_MSG_LEN, MSG_DONTWAIT);
	if (rcvd == 0) {
		printf("Unable to receive response\n");
		goto end;
	}
	if (rcvd < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			r = 0;
		}
		else {
			printf("Unable to receive response\n");
			r = -errno;
		}

		goto end;
	}

	r = coap_packet_parse(&reply, data, rcvd, NULL, 0);
	if (r < 0) {
		printf("Unable to parse recieved packet\n");
		goto end;
	}

	code = coap_header_get_code(&reply);

	if (method == COAP_METHOD_GET) {
		payload_buf = coap_packet_get_payload(&reply, payload_len);
		memcpy(payload, payload_buf, *payload_len);
		if (r < 0) {
			goto end;
		}

		if (!coap_next_block(&reply, &blk_ctx) || code != COAP_RESPONSE_CODE_CONTENT) {
			memset(&blk_ctx, 0, sizeof(blk_ctx));
			*last_block = true;
		}
		else{
			*last_block = false;
		}
	}

	k_free(data);
	return code;

end:
	k_free(data);
	return 0;
}

u8_t zcoap_request_post(int sock, u8_t *path, u8_t *payload)
{
	return zcoap_request(sock, path, COAP_METHOD_POST, payload, NULL, NULL);
}

u8_t zcoap_request_put(int sock, u8_t *path, u8_t *payload)
{
	return zcoap_request(sock, path, COAP_METHOD_PUT, payload, NULL, NULL);
}

u8_t zcoap_request_get(int sock, u8_t *path, u8_t *payload, u16_t *payload_len, bool *last_block)
{
	return zcoap_request(sock, path, COAP_METHOD_GET, payload, payload_len, last_block);
}
