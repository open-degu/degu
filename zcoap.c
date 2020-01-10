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

#include <logging/log.h>

#include "zcoap.h"

LOG_MODULE_REGISTER(zcoap);

static struct coap_block_context blk_ctx;

static u8_t token[8];

static const u16_t COAP_BLOCK_THRESHOLD = 1024;

static int zcoap_request(int sock, u8_t *path, u8_t method, u8_t *payload, u16_t *payload_len, bool *last_block)
{
	int r;
	int rcvd;
	fd_set fds;
	struct coap_packet request;
	struct coap_packet reply;
	u8_t *data;
	const u8_t *payload_buf;
	int code;
	struct timeval tv;
	u8_t retry;

	code = 0;
	retry = 0;

	if (blk_ctx.total_size == 0) {
		if (method == COAP_METHOD_GET) {
			coap_block_transfer_init(&blk_ctx, COAP_BLOCK_1024,
						BLOCK_WISE_TRANSFER_SIZE_GET);
		}
		else if ((method == COAP_METHOD_POST || method == COAP_METHOD_PUT) &&
			  *payload_len > COAP_BLOCK_THRESHOLD) {
			r = coap_block_transfer_init(&blk_ctx, COAP_BLOCK_1024, *payload_len);
			if (r != 0) {
				LOG_ERR("failed to coap_block_transfer_init(%d)\n", r);
				return code;
			}
		}
		memcpy(token, coap_next_token(), sizeof(token));
	}

	data = (u8_t *)k_malloc(MAX_COAP_MSG_LEN);
	if (!data) {
		LOG_ERR("can't malloc\n");
		memset(&blk_ctx, 0, sizeof(blk_ctx));
		return code;
	}
	memset(data, 0, MAX_COAP_MSG_LEN);

	r = coap_packet_init(&request, data, MAX_COAP_MSG_LEN,
			     1, COAP_TYPE_CON, sizeof(token), token,
			     method, coap_next_id());
	if (r < 0) {
		LOG_ERR("Unable to init CoAP packet\n");
		goto errorend;
	}

	r = coap_packet_append_option(&request, COAP_OPTION_URI_PATH, path, strlen(path));
	if (r < 0) {
		LOG_ERR("Unable to append URI to request\n");
		goto errorend;
	}

	if (method == COAP_METHOD_GET) {
		r = coap_append_block2_option(&request, &blk_ctx);
		if (r < 0) {
			LOG_ERR("Unable to append block2 option to request\n");
			goto errorend;
		}
	}
	else if (method == COAP_METHOD_POST || method == COAP_METHOD_PUT) {
		if (*payload_len > COAP_BLOCK_THRESHOLD || blk_ctx.total_size != 0) {
			r = coap_append_block1_option(&request, &blk_ctx);
			if (r < 0) {
				LOG_ERR("Unable to append block1 option to request\n");
				goto errorend;
			}
		}
		r = coap_packet_append_payload_marker(&request);
		if (r < 0) {
			LOG_ERR("Unable to append payload maker\n");
			goto errorend;
		}

		if (blk_ctx.total_size > COAP_BLOCK_THRESHOLD) {
			if ((blk_ctx.total_size - blk_ctx.current) < COAP_BLOCK_THRESHOLD) {
				r = coap_packet_append_payload(&request, payload,
						blk_ctx.total_size - blk_ctx.current);
			} else {
				r = coap_packet_append_payload(&request, payload, COAP_BLOCK_THRESHOLD);
			}
		}
		else {
			r = coap_packet_append_payload(&request, payload, *payload_len);
		}
		if (r < 0) {
			LOG_ERR("Unable to append payload\n");
			goto errorend;
		}
	}

	tv.tv_sec = COAP_ACK_TIMEOUT;
	tv.tv_usec = 0;
	while (1) {
		r = send(sock, request.data, request.offset, 0);
		if (r < 0) {
			LOG_ERR("Unable to send request\n");
			goto errorend;
		}

		FD_ZERO(&fds);
		FD_SET(sock, &fds);
		r = select(sock + 1, &fds, NULL, NULL, &tv);
		if (!r) {
			LOG_ERR("Receiving response timeout\n");
			tv.tv_sec *= 2;
			retry ++;
		}
		else{
			break;
		}

		if (retry > COAP_MAX_RETRANSMIT) {
			code = COAP_FAILED_TO_RECEIVE_RESPONSE;
			goto errorend;
		}
	}

	rcvd = recv(sock, data, MAX_COAP_MSG_LEN, MSG_DONTWAIT);
	if (rcvd == 0) {
		LOG_ERR("Unable to receive response\n");
		goto errorend;
	}
	if (rcvd < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			r = 0;
		}
		else {
			LOG_ERR("Unable to receive response\n");
			r = -errno;
		}

		goto errorend;
	}

	r = coap_packet_parse(&reply, data, rcvd, NULL, 0);
	if (r < 0) {
		LOG_ERR("Unable to parse recieved packet\n");
		goto errorend;
	}

	code = coap_header_get_code(&reply);

	if (method == COAP_METHOD_GET) {
		payload_buf = coap_packet_get_payload(&reply, payload_len);
		memcpy(payload, payload_buf, *payload_len);

		if (!coap_next_block(&reply, &blk_ctx) || code != COAP_RESPONSE_CODE_CONTENT) {
			memset(&blk_ctx, 0, sizeof(blk_ctx));
			*last_block = true;
		}
		else {
			*last_block = false;
		}
	}
	else if (method == COAP_METHOD_POST || method == COAP_METHOD_PUT) {
		if (blk_ctx.total_size > COAP_BLOCK_THRESHOLD) {
			r = coap_next_block(&request, &blk_ctx);
			if (r >= *payload_len || r == 0) {
				memset(&blk_ctx, 0, sizeof(blk_ctx));
				*last_block = true;
			}
			else {
				*last_block = false;
			}
		}
		else {
			memset(&blk_ctx, 0, sizeof(blk_ctx));
			*last_block = true;
		}
	}

	k_free(data);
	return code;

errorend:
	memset(&blk_ctx, 0, sizeof(blk_ctx));
	k_free(data);
	return code;
}

int zcoap_request_post(int sock, u8_t *path, u8_t *payload, u16_t *payload_len, bool *last_block)
{
	return zcoap_request(sock, path, COAP_METHOD_POST, payload, payload_len, last_block);
}

int zcoap_request_put(int sock, u8_t *path, u8_t *payload, u16_t *payload_len, bool *last_block)
{
	return zcoap_request(sock, path, COAP_METHOD_PUT, payload, payload_len, last_block);
}

int zcoap_request_get(int sock, u8_t *path, u8_t *payload, u16_t *payload_len, bool *last_block)
{
	return zcoap_request(sock, path, COAP_METHOD_GET, payload, payload_len, last_block);
}

int zcoap_request_delete(int sock, u8_t *path)
{
	return zcoap_request(sock, path, COAP_METHOD_DELETE, NULL, NULL, NULL);
}
