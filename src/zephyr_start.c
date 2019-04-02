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

#include <fs.h>
#include <ff.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(main);

int real_main(void);
int bg_main(char *src, size_t len);
static const char splash[] = "Start background micropython process.\r\n";
bool mp_running = 0;

FATFS fsdata;
static struct fs_mount_t fatfs_mnt = {
	.type = FS_FATFS,
	.fs_data = &fsdata,
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

	err = mount_fat();
	if(err) {
		LOG_ERR("Failed to mount user region");
		goto no_script;
	}
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

void main(void) {
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

