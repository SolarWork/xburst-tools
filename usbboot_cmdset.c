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

#include "shell.h"
#include "config.h"
#include "ingenic.h"

static int usbboot_boot(int argc, char *argv[]);

const shell_command_t usbboot_cmdset[] = {
	{ "boot", "- Reconfigure stage2", usbboot_boot },

	{ NULL, NULL, NULL }
};

static int usbboot_boot(int argc, char *argv[]) {
	int ret = ingenic_configure_stage2(shell_device());

	if(ret == -1)
		perror("ingenic_configure_stage2");

	return ret;
}

