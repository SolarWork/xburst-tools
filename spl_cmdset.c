/*
 * JzBoot: an USB bootloader for JZ series of Ingenic(R) microprocessors.
 * Copyright (C) 2010  Sergey Gridassov <grindars@gmail.com>,
 *                     Peter Zotov <whitequark@whitequark.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "shell.h"
#include "config.h"
#include "ingenic.h"

static int spl_memtest(int argc, char *argv[]);
static int spl_gpio(int argc, char *argv[]);
static int spl_boot(int argc, char *argv[]);

const shell_command_t spl_cmdset[] = {
	{ "memtest", "[BASE <SIZE>] - SDRAM test", spl_memtest },
	{ "gpio", "<PIN> <STATE> - Set GPIO #PIN to STATE 0 or 1", spl_gpio },
	{ "boot", " - Load stage2 USB bootloader", spl_boot },
	{ NULL, NULL, NULL }
};

static int spl_stage1_op(uint32_t op, uint32_t pin, uint32_t base, uint32_t size) {
	if(cfg_getenv("STAGE1_FILE") == NULL) {
		printf("Variable STAGE1_FILE is not set\n");

		return -1;
	}

	int ret = ingenic_stage1_debugop(shell_device(), cfg_getenv("STAGE1_FILE"), op, pin, base, size);

	if(ret == -1)
		perror("ingenic_stage1_debugop");

	return ret;
}

static int spl_memtest(int argc, char *argv[]) {
	if(argc != 1 && argc != 3) {
		printf("Usage: %s [BASE <SIZE>]\n", argv[0]);

		return -1;
	}

	uint32_t start, size;

	if(argc == 3) {
		start = strtoul(argv[1], NULL, 0);
		size = strtoul(argv[2], NULL, 0);
	} else {
		start = SDRAM_BASE;
		size = ingenic_sdram_size(shell_device());
	}

	if(cfg_getenv("STAGE1_FILE") == NULL) {
		printf("Variable STAGE1_FILE is not set\n");

		return -1;
	}

	uint32_t fail;

	int ret = ingenic_memtest(shell_device(), cfg_getenv("STAGE1_FILE"), start, size, &fail);

	if(ret == -1) {
		if(errno == EFAULT) {
			printf("Memory test failed at address 0x%08X\n", fail);
		} else {
			perror("ingenic_memtest");
		}

	} else {
		printf("Memory test passed\n");
	}

	return ret;
}

static int spl_gpio(int argc, char *argv[]) {
	if(argc != 3 || (strcmp(argv[2], "0") && strcmp(argv[2], "1"))) {
		printf("Usage: %s <PIN> <STATE>\n", argv[0]);
		printf("  STATE := 0 | 1\n");

		return -1;
	}

	return spl_stage1_op(!strcmp(argv[2], "1") ? STAGE1_DEBUG_GPIO_SET : STAGE1_DEBUG_GPIO_CLEAR, atoi(argv[1]), 0, 0);
}

static int spl_boot(int argc, char *argv[]) {
	if(argc != 1) {
		printf("Usage: %s\n", argv[0]);
	}

	int ret = spl_stage1_op(STAGE1_DEBUG_BOOT, 0, 0, 0);

	if(ret == -1)
		return -1;

	if(cfg_getenv("STAGE2_FILE") == NULL) {
		printf("Variable STAGE2_FILE is not set\n");

		return -1;
	}

	ret = ingenic_loadstage(shell_device(), INGENIC_STAGE2, cfg_getenv("STAGE2_FILE"));

	if(ret == -1)
		perror("ingenic_loadstage");

	return ret;
}

