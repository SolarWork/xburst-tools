/*
 * Copyright(C) 2009  Qi Hardware Inc.,
 * Authors: Xiangfu Liu <xiangfu@qi-hardware.com>

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

#ifndef __CMD_H__
#define __CMD_H__

#include "usb_boot_defines.h"
#include <stdint.h>

#define COMMAND_NUM 31
#define MAX_ARGC	10
#define MAX_COMMAND_LENGTH	100

struct ingenic_dev;

int boot(struct ingenic_dev *dev, const char *stage1_path, const char *stage2_path);
int debug_memory(struct ingenic_dev *dev, int obj, unsigned int start, unsigned int size);
int debug_go(struct ingenic_dev *dev, size_t argc, char *argv[]);
int sdram_load_file(struct ingenic_dev *dev, struct sdram_in *sdram_in, char *file_path);

#endif  /* __CMD_H__ */
