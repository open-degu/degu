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

#define MAX_COAP_MSG_LEN 1152
#define BLOCK_WISE_TRANSFER_SIZE_GET 0x6e000
#define COAP_TYPE_CON 0 //Confirmable
#define COAP_TYPE_NCON 1 //non-Confirmable
#define COAP_TYPE_ACK 2 //Acknowledgement
#define COAP_TYPE_RST 3 //Reset

u8_t zcoap_request_post(int sock, u8_t *path, u8_t *payload);
u8_t zcoap_request_put(int sock, u8_t *path, u8_t *payload);
u8_t zcoap_request_get(int sock, u8_t *path, u8_t *payload, u16_t *payload_len, bool *last_block);
