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
#include <shell/shell.h>
#include <init.h>
#include <misc/reboot.h>
#include <net/socket.h>
#include <net/coap.h>
#include <flash.h>
#include <json.h>
#include <stdio.h>

#include <fs.h>
#include <ff.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(main);

#include "../md5.h"
#include "../zcoap.h"
#include "../degu_utils.h"
int real_main(void);
int bg_main(char *src, size_t len);
static const char splash[] = "Start background micropython process.\r\n";
bool mp_running = 0;

FATFS fsdata;
static struct fs_mount_t fatfs_mnt = {
	.type = FS_FATFS,
	.fs_data = &fsdata,
};

struct device *flash_dev;

int sock;
struct sockaddr sockaddr;
u16_t payload_len;
u8_t *payload;

char gw_addr[NET_IPV6_ADDR_LEN];
char eui64[17];

char script_user_ver[33];
char config_user_ver[33];
char firmware_system_ver[33];

bool update_flag_script_user;
bool update_flag_config_user;
bool update_flag_firmware_system;

char coap_path_thing[23];
char coap_path_update_status[31];
char coap_path_update_script_user[36];
char coap_path_update_config_user[36];
char coap_path_update_firmware_system[40];

struct shadow_send {
	struct state_send {
		struct reported {
			char *script_user_ver;
			char *config_user_ver;
			char *firmware_system;
			char *firmware_system_ver;
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

#define BOOT_MAGIC_SZ  16
#define BOOT_MAGIC_OFFS	(FLASH_AREA_IMAGE_1_SIZE - BOOT_MAGIC_SZ)

static u32_t boot_img_magic[4] = {
	0xf395c277,
	0x7fefd260,
	0x0f505235,
	0x8079b62c,
};

static int mount_fat(void)
{
	int res;
	fatfs_mnt.mnt_point = "/NAND:";

	res = fs_mount(&fatfs_mnt);
	if (res != 0) {
		LOG_INF("Error mounting fat fs.Error Code [%d]", res);
		return -1;
	}
	return 0;
}

int run_user_script(char *path) {
	struct fs_file_t file;
	struct fs_dirent dirent;
	int err, offset = 0;
	static char *file_data;
	static size_t len;

	err = fs_stat(path, &dirent);
	if(err) {
		LOG_ERR("Failed to stat file");
		goto no_script;
	}

	err = fs_open(&file, path);
	if(err) {
		LOG_ERR("Can't open file");
		goto no_script;
	}

	if (offset > 0) {
		fs_seek(&file, offset, FS_SEEK_SET);
	}
	len = dirent.size;
	file_data = k_malloc(len);
	int count = INT_MAX;
	int read = 0;
	while(1){
		read = fs_read(&file, file_data + offset, min(count, len));
		if (read <= 0) {
			break;
		}
		offset += read;
		count -= read;
	}
	console_init();
	console_write(NULL, splash, sizeof(splash) - 1);

	bg_main(file_data, len);
	fs_close(&file);
	k_free(file_data);
	return 1;
no_script:
	return 0;
}

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
	struct fs_file_t file;
	struct fs_dirent dirent;
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
		read = fs_read(&file, file_data + offset, min(count, sizeof(file_data)));
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

#define FIRWARE_SIZE_SLOT0		((uint32_t *)0x0001400CL)
static int firmware_sum(char *md5)
{
	int size = *FIRWARE_SIZE_SLOT0 + 848;

	strcpy(md5, md5sum((char *)FLASH_AREA_IMAGE_0_OFFSET, size));
	LOG_INF("firmware size: %d, md5sum: %s", size, md5);

	return 0;
}

int update_init(void)
{
	int ret;
	char shadow_encoded[1024];
	struct sockaddr_in *sockaddr_in = (struct sockaddr_in*)&sockaddr;

	flash_dev = device_get_binding(DT_FLASH_DEV_NAME);

	payload = (u8_t *)k_malloc(MAX_COAP_MSG_LEN);
	if (!payload) {
		LOG_ERR("Cannot malloc for payload");
		goto init_err;
	}

	sock = zsock_socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0) {
		LOG_ERR("Cannot make a socket");
		goto init_err;
	}

	sockaddr_in->sin_family = AF_INET6;

	/* Get the address of gateway */
	strcpy(gw_addr, get_gw_addr(64));
	ret = zsock_inet_pton(AF_INET6, gw_addr, &sockaddr_in->sin_addr);
	if (ret <= 0) {
		LOG_ERR("Cannot get gateway address");
		goto init_err;
	}

	sockaddr_in->sin_port = htons(5683);
	ret = zsock_connect(sock, &sockaddr, sizeof(sockaddr));
	if (ret < 0) {
		LOG_ERR("Cannot connect to gateway");
		goto init_err;
	}

	update_flag_script_user = false;
	update_flag_config_user = false;
	update_flag_firmware_system = false;

	get_eui64(eui64);
	eui64[16] = '\0';
	snprintf(coap_path_thing, sizeof(coap_path_thing), "thing/%s", eui64);
	snprintf(coap_path_update_status, sizeof(coap_path_update_status), "update/status/%s", eui64);
	snprintf(coap_path_update_script_user, sizeof(coap_path_update_script_user), "update/script_user/%s", eui64);
	snprintf(coap_path_update_config_user, sizeof(coap_path_update_config_user), "update/config_user/%s", eui64);
	snprintf(coap_path_update_firmware_system, sizeof(coap_path_update_firmware_system), "update/firmware_system/%s", eui64);

	user_sum("/NAND:/main.py", script_user_ver);
	shadow_send.state.reported.script_user_ver = script_user_ver;
	user_sum("/NAND:/config", config_user_ver);
	shadow_send.state.reported.config_user_ver = config_user_ver;
	firmware_sum(firmware_system_ver);
	shadow_send.state.reported.firmware_system_ver = firmware_system_ver;

	ret = json_obj_encode_buf(shadow_send_descr, ARRAY_SIZE(shadow_send_descr),
				&shadow_send, shadow_encoded, sizeof(shadow_encoded));

	zcoap_request_post(sock, coap_path_thing, shadow_encoded);

	return 0;

init_err:
	return 1;
}

int erase_flash_slot1(void)
{
	int err;

	LOG_INF("Erasing slot-1...");

	flash_write_protection_set(flash_dev, false);
	err = flash_erase(flash_dev, FLASH_AREA_IMAGE_1_OFFSET, FLASH_AREA_IMAGE_1_SIZE);
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
	err = flash_write(flash_dev, FLASH_AREA_IMAGE_1_OFFSET + written, data, len);
	flash_write_protection_set(flash_dev, true);
	if (err) {
		return 1;
	}
/*
 * verify
*/

	return 0;
}

int do_update(void)
{
	struct fs_file_t file;
	struct fs_dirent dirent;
	int err;
	char request_url[1024];
	bool done = false;
	int code;
	int byte_written = 0;
	bool last_block;

	json_obj_encode_buf(desired_descr, ARRAY_SIZE(desired_descr),
			&shadow_recv.state.desired, request_url, sizeof(request_url));

	if (update_flag_script_user) {
		err = fs_stat("/NAND:/main.py", &dirent);
		if (!err) {
			fs_unlink("/NAND:/main.py");
		}

		err = fs_open(&file, "/NAND:/main.py");
		if(err) {
			LOG_ERR("Can't open file");
			goto err_update;
		}

		code = zcoap_request_put(sock, coap_path_update_script_user, "");
		if (code != COAP_RESPONSE_CODE_CREATED)
			goto err_update;

		code = zcoap_request_post(sock, coap_path_update_script_user, request_url);
		if (code != COAP_RESPONSE_CODE_CHANGED)
			goto err_update;

		while (!done) {
			code = zcoap_request_get(sock, coap_path_update_script_user, payload, &payload_len, &last_block);
			switch (code) {
			case COAP_RESPONSE_CODE_CONTENT:
				err = fs_write(&file, payload, payload_len);
				if (err < 0) {
					LOG_ERR("Can't write");
					goto err_update;
				}
				fs_sync(&file);
				if (last_block) {
					done = true;
				}
				break;
			case COAP_RESPONSE_CODE_VALID:
				break;
			default:
				done = true;
				break;
			}
		}
		fs_close(&file);
	}
	done = false;

	if (update_flag_config_user) {
		err = fs_stat("/NAND:/config", &dirent);
		if (!err) {
			fs_unlink("/NAND:/config");
		}

		err = fs_open(&file, "/NAND:/config");
		if(err) {
			LOG_ERR("Can't open file");
			goto err_update;
		}

		code = zcoap_request_put(sock, coap_path_update_config_user, "");
		if (code != COAP_RESPONSE_CODE_CREATED)
			goto err_update;

		code = zcoap_request_post(sock, coap_path_update_config_user, request_url);
		if (code != COAP_RESPONSE_CODE_CHANGED)
			goto err_update;

		while (!done) {
			code = zcoap_request_get(sock, coap_path_update_config_user, payload, &payload_len, &last_block);
			switch (code) {
			case COAP_RESPONSE_CODE_CONTENT:
				err = fs_write(&file, payload, payload_len);
				if (err < 0) {
					LOG_ERR("Can't write");
					goto err_update;
				}
				fs_sync(&file);
				if (last_block) {
					done = true;
				}
				break;
			case COAP_RESPONSE_CODE_VALID:
				break;
			default:
				done = true;
				break;
			}
		}
		fs_close(&file);
	}
	done = false;

	if (update_flag_firmware_system) {
		code = zcoap_request_put(sock, coap_path_update_firmware_system, "");
		if (code != COAP_RESPONSE_CODE_CREATED)
			goto err_update;

		code = zcoap_request_post(sock, coap_path_update_firmware_system, request_url);
		if (code != COAP_RESPONSE_CODE_CHANGED)
			goto err_update;

		erase_flash_slot1();

		while (!done) {
			code = zcoap_request_get(sock, coap_path_update_firmware_system, payload, &payload_len, &last_block);
			switch (code) {
			case COAP_RESPONSE_CODE_CONTENT:
				err = write_flash_slot1(byte_written, payload, payload_len);
				if (err) {
					LOG_ERR("Can't write");
					goto err_update;
				}
				byte_written += payload_len;

				LOG_INF("Download total: %d bytes", byte_written);
				if (last_block) {
					write_flash_slot1(BOOT_MAGIC_OFFS, boot_img_magic, BOOT_MAGIC_SZ);
					LOG_INF("Download completed!");
					done = true;
				}
				break;
			case COAP_RESPONSE_CODE_VALID:
				break;
			default:
				done = true;
				break;
			}
		}
	}

	return 0;

err_update:

	return 1;
}

int check_update(void)
{
	int ret = 0;
	int code;
	bool last_block;

	code = zcoap_request_put(sock, coap_path_update_status, "");
	if (code != COAP_RESPONSE_CODE_CREATED)
		goto err_update;

	do {
		code = zcoap_request_get(sock, coap_path_update_status, payload, &payload_len, &last_block);
		if(code == COAP_RESPONSE_CODE_NOT_FOUND)
			goto err_update;
	} while (code != COAP_RESPONSE_CODE_CONTENT);

	if (payload != NULL) {
		json_obj_parse(payload, payload_len, shadow_recv_descr, ARRAY_SIZE(shadow_recv_descr), &shadow_recv);

		if (shadow_recv.state.desired.script_user_ver != NULL) {
			ret = strcmp(shadow_recv.state.desired.script_user_ver, shadow_send.state.reported.script_user_ver);
			if (ret) {
				update_flag_script_user = true;
			}
		}
		if (shadow_recv.state.desired.config_user_ver != NULL) {
			ret = strcmp(shadow_recv.state.desired.config_user_ver, shadow_send.state.reported.config_user_ver);
			if (ret) {
				update_flag_config_user = true;
			}
		}
		if (shadow_recv.state.desired.firmware_system_ver != NULL) {
			ret = strcmp(shadow_recv.state.desired.firmware_system_ver, shadow_send.state.reported.firmware_system_ver);
			if (ret) {
				update_flag_firmware_system = true;
			}
		}
	}

err_update:
	return ret;
}

void main(void) {
	int err = 0;

	err = mount_fat();
	if (err) {
		LOG_ERR("Failed to mount user region");
	}

	err = update_init();
	if (err) {
		LOG_ERR("Failed to init for updating");
	}
	else {
		/* Check update status */
		if (check_update()) {
			LOG_INF("Trying to update...");
			if (!do_update()) {
				sys_reboot(SYS_REBOOT_COLD);
			}
		}
	}

	sys_pm_ctrl_disable_state(SYS_POWER_STATE_CPU_LPS_1);
	sys_pm_ctrl_disable_state(SYS_POWER_STATE_CPU_LPS_2);
	mp_running = run_user_script("/NAND:/main.py");
}

static int cmd_upython(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	if (mp_running){
		shell_print(shell, "WARNING: micropython already started background.");
		return 0;
	}

	console_init();
	real_main();
	return 0;
}
SHELL_CMD_REGISTER(upython, NULL, "micropython interpreterx`", cmd_upython);
