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

#ifndef __SHELL__H__
#define __SHELL__H__

typedef struct {
	const char *cmd;
	const char *description;
	int (*handler)(int argc, char *argv[]);
} shell_command_t;

int shell_init(void *ingenic);
void shell_fini();

void shell_interactive();
int shell_source(const char *filename);
int shell_execute(const char *input);

void *shell_device();

// lexer interface
extern char *strval;

#define TOK_SEPARATOR	1
#define TOK_STRING	2
#define TOK_SPACE	3
#define TOK_COMMENT	4

int shell_pull(char *buf, int maxlen);

extern const shell_command_t spl_cmdset[];

#endif
