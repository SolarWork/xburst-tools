/*
 * JzBoot: an USB bootloader for JZ series of Ingenic(R) microprocessors.
 * Copyright (C) 2010  Sergey Gridassov <grindars@gmail.com>
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

#include <stdarg.h>
#include <stdio.h>

#include "debug.h"

static int debug_level = LEVEL_ERROR;

void set_debug_level(int level) {
	debug_level = level;
}

int get_debug_level() {
	return debug_level;
}

void debug(int level, const char *fmt, ...) {
	va_list list;

	va_start(list, fmt);

	if(level <= debug_level) {
		if(level <= LEVEL_ERROR)
			vfprintf(stderr, fmt, list);
		else
			vprintf(fmt, list);
	}

	va_end(list);
}
