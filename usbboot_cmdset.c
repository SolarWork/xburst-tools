/*
 * JzBoot: an USB bootloader for JZ series of Ingenic(R) microprocessors.
 * Copyright (C) 2010  Peter Zotov <whitequark@whitequark.org>
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

static int usbboot_boot(int argc, char *argv[]);
static int usbboot_load(int argc, char *argv[]);
static int usbboot_go(int argc, char *argv[]);

const shell_command_t usbboot_cmdset[] = {
	{ "boot", "- Reconfigure stage2", usbboot_boot },
	{ "load", "<FILE> <BASE> - Load file to SDRAM", usbboot_load },
	{ "go", "<ADDRESS> - Jump to <ADDRESS>", usbboot_go },

	{ NULL, NULL, NULL }
};

static int usbboot_boot(int argc, char *argv[]) {
	int ret = ingenic_configure_stage2(shell_device());

	if(ret == -1)
		perror("ingenic_configure_stage2");

	return ret;
}

static int usbboot_load(int argc, char *argv[]) {
	if(argc != 3) {
		printf("Usage: %s <FILE> <BASE>\n", argv[0]);

		return -1;
	}

	int ret = ingenic_load_sdram_file(shell_device(), strtoul(argv[2], NULL, 0), argv[1]);

	if(ret == -1)
		perror("ingenic_load_sdram_file");

	return ret;
}

static int usbboot_go(int argc, char *argv[]) {
	if(argc != 2) {
		printf("Usage: %s <ADDRESS>\n", argv[0]);

		return -1;
	}

	int ret = ingenic_go(shell_device(), strtoul(argv[1], NULL, 0));

	if(ret == -1)
		perror("ingenic_go");

	return ret;
}

