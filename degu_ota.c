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

#include <stdio.h>
#include <string.h>
#include <flash.h>
#include <json.h>
#include <fs.h>
#include <net/coap.h>
#include <sys/util.h>
#include <logging/log.h>
#include "mbedtls/md5.h"
#include "degu_utils.h"
#include "zcoap.h"
#include "degu_ota.h"
#include "version.h"

#define FIRWARE_SIZE_SLOT0	((uint32_t *)0x0001400CL)
#define BOOT_MAGIC_SZ		16
#define BOOT_MAGIC_OFFS		(DT_FLASH_AREA_IMAGE_1_SIZE - BOOT_MAGIC_SZ)

LOG_MODULE_REGISTER(degu_ota);

static u32_t boot_img_magic[4] = {
	0xf395c277,
	0x7fefd260,
	0x0f505235,
	0x8079b62c,
};

struct device *flash_dev;
struct fs_file_t file;
struct fs_dirent dirent;

u16_t payload_len;
u8_t *payload;
u32_t byte_written;

char script_user_ver[33];
char config_user_ver[33];
char firmware_system_ver[33];
char firmware_ver[33];

bool update_flag_script_user;
bool update_flag_config_user;
bool update_flag_firmware_system;

struct shadow_send {
	struct state_send {
		struct reported {
			char *script_user_ver;
			char *config_user_ver;
			char *firmware_system;
			char *firmware_system_ver;
			char *firmware_ver;
		} reported;
	} state;
} shadow_send;

struct shadow_recv {
	struct state_recv {
		struct desired {
			char *script_user;
			char *script_user_ver;
			char *config_user;
			char *config_user_ver;
			char *firmware_system;
			char *firmware_system_ver;
		} desired;
	} state;
} shadow_recv;

static const struct json_obj_descr reported_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct reported, script_user_ver, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct reported, config_user_ver, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct reported, firmware_system_ver, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct reported, firmware_ver, JSON_TOK_STRING),
};

static const struct json_obj_descr desired_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct desired, script_user, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct desired, script_user_ver, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct desired, config_user, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct desired, config_user_ver, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct desired, firmware_system, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct desired, firmware_system_ver, JSON_TOK_STRING),
};

static const struct json_obj_descr state_send_descr[] = {
	JSON_OBJ_DESCR_OBJECT(struct state_send, reported, reported_descr),
};

static const struct json_obj_descr state_recv_descr[] = {
	JSON_OBJ_DESCR_OBJECT(struct state_recv, desired, desired_descr),
};

static const struct json_obj_descr shadow_send_descr[] = {
	JSON_OBJ_DESCR_OBJECT(struct shadow_send, state, state_send_descr),
};

static const struct json_obj_descr shadow_recv_descr[] = {
	JSON_OBJ_DESCR_OBJECT(struct shadow_recv, state, state_recv_descr),
};

static char *md5sum(char *buf, int len)
{
	unsigned char output[16];
	static char outword[33];
	mbedtls_md5_ret(buf, len, output);
	for(int i = 0; i<16; i++){
		sprintf(outword+i*2, "%02x", output[i]);
	}
	outword[32] = '\0';
	return outword;
}

static int user_sum(char *path, char *md5)
{
	int err, offset = 0;
	char *file_data;
	int count = INT_MAX;
	int read = 0;

	err = fs_stat(path, &dirent);
	if(err) {
		LOG_ERR("Failed to stat file");
		strcpy(md5, "none");
		return 1;
	}

	err = fs_open(&file, path);
	if(err) {
		LOG_ERR("Can't open file");
		strcpy(md5, "none");
		return 1;
	}

	if (offset > 0) {
		fs_seek(&file, offset, FS_SEEK_SET);
	}

	file_data = k_malloc(dirent.size);
	if (!file_data) {
		LOG_ERR("Cannot malloc for a user file");
		return 1;
	}

	while(1){
		read = fs_read(&file, file_data + offset, MIN(count, sizeof(file_data)));
		if (read <= 0) {
			break;
		}
		offset += read;
		count -= read;
	}

	strcpy(md5, md5sum(file_data, dirent.size));
	LOG_INF("%s : %s\n", path, md5);

	k_free(file_data);
	fs_close(&file);

	return 0;
}

static int firmware_sum(char *md5)
{
	int size = *FIRWARE_SIZE_SLOT0 + 848;

	strcpy(md5, md5sum((char *)DT_FLASH_AREA_IMAGE_0_OFFSET, size));
	LOG_INF("firmware size: %d, md5sum: %s", size, md5);

	return 0;
}

int update_init(void)
{
	char shadow_encoded[1024];
	memset(shadow_encoded, 0, 1024);

	degu_get_asset();

	flash_dev = device_get_binding(DT_FLASH_DEV_NAME);

	payload = (u8_t *)k_malloc(MAX_COAP_MSG_LEN);
	if (!payload) {
		LOG_ERR("Cannot malloc for payload");
		return DEGU_OTA_ERR;
	}
	memset(payload, 0, MAX_COAP_MSG_LEN);

	update_flag_script_user = false;
	update_flag_config_user = false;
	update_flag_firmware_system = false;

	user_sum("/NAND:/main.py", script_user_ver);
	shadow_send.state.reported.script_user_ver = script_user_ver;

	user_sum("/NAND:/CONFIG", config_user_ver);
	shadow_send.state.reported.config_user_ver = config_user_ver;

	firmware_sum(firmware_system_ver);
	shadow_send.state.reported.firmware_system_ver = firmware_system_ver;

	sprintf(firmware_ver, "%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION);
	shadow_send.state.reported.firmware_ver = firmware_ver;

	json_obj_encode_buf(shadow_send_descr, ARRAY_SIZE(shadow_send_descr),
				&shadow_send, shadow_encoded, sizeof(shadow_encoded));

	return degu_coap_request("thing", COAP_METHOD_POST, shadow_encoded, NULL) < COAP_RESPONSE_CODE_OK ? DEGU_OTA_ERR : DEGU_OTA_OK;
}

int erase_flash_slot1(void)
{
	int err;

	LOG_INF("Erasing slot-1...");

	flash_write_protection_set(flash_dev, false);
	err = flash_erase(flash_dev, DT_FLASH_AREA_IMAGE_1_OFFSET, DT_FLASH_AREA_IMAGE_1_SIZE);
	flash_write_protection_set(flash_dev, true);
	if (err) {
		LOG_INF("Erasing slot-1 failed");
		return 1;
	}

	LOG_INF("Erased slot-1 ");

	return 0;
}

int write_flash_slot1(int written, void *data, int len)
{
	int err;

	flash_write_protection_set(flash_dev, false);
	err = flash_write(flash_dev, DT_FLASH_AREA_IMAGE_1_OFFSET + written, data, len);
	flash_write_protection_set(flash_dev, true);
	if (err) {
		return 1;
	}
/*
 * verify
*/

	return 0;
}

void write_file(u8_t *buf, u16_t len)
{
	fs_write(&file, buf, len);
	fs_sync(&file);
}

void write_firmware(u8_t *buf, u16_t len)
{
	write_flash_slot1(byte_written, buf, len);
	byte_written += len;
	LOG_INF("Wrote: %d", byte_written);
}

void write_img_magic() {
	write_flash_slot1(BOOT_MAGIC_OFFS, boot_img_magic, BOOT_MAGIC_SZ);
}

int do_update(void)
{
	char request_url[1024];
	int err;

	memset(request_url, 0, 1024);

	json_obj_encode_buf(desired_descr, ARRAY_SIZE(desired_descr),
		&shadow_recv.state.desired, request_url, sizeof(request_url));

	if (update_flag_script_user) {
		if (degu_coap_request("update/script_user", COAP_METHOD_PUT, "", NULL) < COAP_RESPONSE_CODE_OK) {
			goto error;
		}

		if (degu_coap_request("update/script_user", COAP_METHOD_POST, request_url, NULL) < COAP_RESPONSE_CODE_OK) {
			goto error;
		}

		err = fs_stat("/NAND:/main.py", &dirent);
		if (!err) {
			fs_unlink("/NAND:/main.py");
		}

		err = fs_open(&file, "/NAND:/main.py");
		if(err) {
			LOG_ERR("Can't open user script");
			goto error;
		}

		memset(payload, 0, 1024);
		if (degu_coap_request("update/script_user", COAP_METHOD_GET, payload, &write_file) < COAP_RESPONSE_CODE_OK) {
			fs_close(&file);
			goto error;
		}

		fs_close(&file);
	}

	if (update_flag_config_user) {
		if (degu_coap_request("update/config_user", COAP_METHOD_PUT, "", NULL) < COAP_RESPONSE_CODE_OK) {
			goto error;
		}

		if (degu_coap_request("update/config_user", COAP_METHOD_POST, request_url, NULL) < COAP_RESPONSE_CODE_OK) {
			goto error;
		}

		err = fs_stat("/NAND:/CONFIG", &dirent);
		if (!err) {
			fs_unlink("/NAND:/CONFIG");
		}

		err = fs_open(&file, "/NAND:/CONFIG");
		if(err) {
			LOG_ERR("Can't open config file");
			goto error;
		}

		memset(payload, 0, 1024);
		if (degu_coap_request("update/config_user", COAP_METHOD_GET, payload, &write_file) < COAP_RESPONSE_CODE_OK) {
			fs_close(&file);
			goto error;
		}

		fs_close(&file);
	}

	if (update_flag_firmware_system) {
		byte_written = 0;

		if (degu_coap_request("update/firmware_system", COAP_METHOD_PUT, "", NULL) < COAP_RESPONSE_CODE_OK) {
			goto error;
		}

		if (degu_coap_request("update/firmware_system", COAP_METHOD_POST, request_url, NULL) < COAP_RESPONSE_CODE_OK) {
			goto error;
		}

		memset(payload, 0, 1024);
		if (degu_coap_request("update/firmware_system", COAP_METHOD_GET, payload, &write_firmware) < COAP_RESPONSE_CODE_OK) {
			goto error;
		}

		write_img_magic();
	}

	return DEGU_OTA_OK;

error:
	return DEGU_OTA_ERR;
}

int check_update(void)
{
	int diff;
	int ret = DEGU_OTA_ERR;

	if (degu_coap_request("update/status", COAP_METHOD_PUT, "", NULL) < COAP_RESPONSE_CODE_OK) {
		goto end;
	}

	memset(payload, 0, 1024);
	if (degu_coap_request("update/status", COAP_METHOD_GET, payload, NULL) < COAP_RESPONSE_CODE_OK) {
		goto end;
	}

	if (payload != NULL) {
		json_obj_parse(payload, strlen(payload), shadow_recv_descr,
				ARRAY_SIZE(shadow_recv_descr), &shadow_recv);

		if (shadow_recv.state.desired.script_user_ver != NULL) {
			diff = strcmp(shadow_recv.state.desired.script_user_ver,
					shadow_send.state.reported.script_user_ver);
			if (diff) {
				update_flag_script_user = true;
				ret = DEGU_OTA_OK;
			}
		}
		if (shadow_recv.state.desired.config_user_ver != NULL) {
			diff = strcmp(shadow_recv.state.desired.config_user_ver,
					shadow_send.state.reported.config_user_ver);
			if (diff) {
				update_flag_config_user = true;
				ret = DEGU_OTA_OK;
			}
		}
		if (shadow_recv.state.desired.firmware_system_ver != NULL) {
			diff = strcmp(shadow_recv.state.desired.firmware_system_ver,
					shadow_send.state.reported.firmware_system_ver);
			if (diff) {
				update_flag_firmware_system = true;
				ret = DEGU_OTA_OK;
			}
		}
	}

end:
	return ret;
}
