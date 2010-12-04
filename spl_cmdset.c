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

static int spl_load_stage1() {
	if(cfg_getenv("STAGE1_FILE") == NULL) {
		printf("Variable STAGE1_FILE is not set\n");

		return -1;
	}

	return ingenic_loadstage(shell_device(), INGENIC_STAGE1, cfg_getenv("STAGE1_FILE"));
}

static int spl_memtest(int argc, char *argv[]) {
	if(argc != 1 && argc != 3) {
		printf("Usage: %s [BASE <SIZE>]\n", argv[0]);
	}

	return spl_load_stage1(); // TODO
}

static int spl_gpio(int argc, char *argv[]) {
	if(argc != 3 || (strcmp(argv[2], "0") && strcmp(argv[2], "1"))) {
		printf("Usage: %s <PIN> <STATE>\n", argv[0]);
		printf("  STATE := 0 | 1\n");

		return -1;
	}

	char *old_debugops = strdup(cfg_getenv("DEBUGOPS")),
	     *old_pinnum   = strdup(cfg_getenv("PINNUM"));

	cfg_setenv("DEBUGOPS", (!strcmp(argv[2], "1")) ?
			SPL_DEBUG_GPIO_SET : SPL_DEBUG_GPIO_CLEAR);
	cfg_setenv("PINNUM", argv[1]);

	int ret = 0;

	ret = shell_execute("rebuildcfg");

	if(ret == -1)
		goto finally;

	ret = spl_load_stage1();

finally:
	cfg_setenv("DEBUGOPS", old_debugops);
	cfg_setenv("PINNUM", old_pinnum);

	free(old_debugops);
	free(old_pinnum);

	return ret;
}

static int spl_boot(int argc, char *argv[]) {
	if(argc != 1) {
		printf("Usage: %s\n", argv[0]);
	}

	int ret;

	ret = spl_load_stage1();

	if(ret == -1)
		return -1;

	if(cfg_getenv("STAGE2_FILE") == NULL) {
		printf("Variable STAGE2_FILE is not set\n");

		return -1;
	}

	return ingenic_loadstage(shell_device(), INGENIC_STAGE2, cfg_getenv("STAGE2_FILE"));
}
