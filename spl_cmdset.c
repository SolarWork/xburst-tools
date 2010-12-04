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

#include "shell.h"
#include "config.h"
#include "ingenic.h"

static int spl_memtest(int argc, char *argv[]);
static int spl_boot(int argc, char *argv[]);

const shell_command_t spl_cmdset[] = {
	{ "memtest", "[BASE <SIZE>] - SDRAM test", spl_memtest },
	{ "boot", " - Load stage2 USB bootloader", spl_boot },
	{ NULL, NULL, NULL }
};

static int spl_load_stage1() {
	return ingenic_loadstage(shell_device(), INGENIC_STAGE1, cfg_getenv("STAGE1_FILE"));
}

static int spl_memtest(int argc, char *argv[]) {
	if(argc != 1 && argc != 3) {
		printf("Usage: %s [BASE <SIZE>]\n", argv[0]);
	}

	return spl_load_stage1(); // TODO
}

static int spl_boot(int argc, char *argv[]) {
	if(argc != 1) {
		printf("Usage: %s\n", argv[0]);
	}

	int ret;

	ret = spl_load_stage1();

	if(ret == -1)
		return -1;

	return ingenic_loadstage(shell_device(), INGENIC_STAGE2, cfg_getenv("STAGE2_FILE"));
}
