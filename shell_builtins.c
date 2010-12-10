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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "shell.h"
#include "config.h"
#include "ingenic.h"

static int builtin_help(shell_context_t *ctx, int argc, char *argv[]);
static int builtin_exit(shell_context_t *ctx, int argc, char *argv[]);
static int builtin_source(shell_context_t *ctx, int argc, char *argv[]);
static int builtin_echo(shell_context_t *ctx, int argc, char *argv[]);
static int builtin_sleep(shell_context_t *ctx, int argc, char *argv[]);
static int builtin_redetect(shell_context_t *ctx, int argc, char *argv[]);
static int builtin_rebuildcfg(shell_context_t *ctx, int argc, char *argv[]);
static int builtin_set(shell_context_t *ctx, int argc, char *argv[]);
static int builtin_safe(shell_context_t *ctx, int argc, char *argv[]);

const shell_command_t builtin_cmdset[] = {
	{ "help", "- Display this message", builtin_help },
	{ "exit", "- Batch: stop current script, interactive: end session", builtin_exit },
	{ "source", "<FILENAME> - run specified script", builtin_source },
	{ "echo", "<STRING> - output specified string", builtin_echo },
	{ "sleep", "<MILLISECONDS> - sleep a specified amount of time", builtin_sleep },
	{ "set", "[VARIABLE] [VALUE] - print or set configuraton variables", builtin_set },
	{ "safe", "<COMMAND> [ARG]... - run command ignoring errors", builtin_safe },

	{ "redetect", " - Redetect CPU", builtin_redetect },
	{ "rebuildcfg", " - Rebuild firmware configuration data", builtin_rebuildcfg },

	{ NULL, NULL, NULL }
};

static int builtin_help(shell_context_t *ctx, int argc, char *argv[]) {
/*	for(int i = 0; commands[i].cmd != NULL; i++) {
		printf("%s %s\n", commands[i].cmd, commands[i].description);
	}

	if(set_cmds) {
		for(int i = 0; set_cmds[i].cmd != NULL; i++)
			printf("%s %s\n", set_cmds[i].cmd, set_cmds[i].description);
	}*/

	return 0;
}

static int builtin_exit(shell_context_t *ctx, int argc, char *argv[]) {
	shell_exit(ctx, 1);

	return 0;
}

static int builtin_source(shell_context_t *ctx, int argc, char *argv[]) {
	if(argc != 2) {
		printf("Usage: %s <FILENAME>\n", argv[0]);

		return -1;
	}

	int ret = shell_source(ctx, argv[1]);

	if(ret == -1) {
		fprintf(stderr, "Error while sourcing file %s: %s\n", argv[1], strerror(errno));
	}

	shell_exit(ctx, 0);

	return ret;
}

static int builtin_echo(shell_context_t *ctx, int argc, char *argv[]) {
	if(argc < 2) {
		printf("Usage: %s <STRING>\n", argv[0]);

		return -1;
	}

	for(int i = 1; i < argc; i++) {
		fputs(argv[i], stdout);

		putchar((i < argc - 1) ? ' ' : '\n');
	}

	return 0;
}

static int builtin_sleep(shell_context_t *ctx, int argc, char *argv[]) {
	if(argc != 2) {
		printf("Usage: %s <MILLISECONDS>\n", argv[0]);

		return -1;
	}

	uint32_t ms = atoi(argv[1]);

	usleep(ms * 1000);

	return 0;
}

static int builtin_redetect(shell_context_t *ctx, int argc, char *argv[]) {
	if(argc != 1) {
		printf("Usage: %s\n", argv[0]);

		return -1;
	}

	if(ingenic_redetect(shell_device(ctx)) == -1) {
		perror("ingenic_redetect");

		return -1;
	} else
		return 0;
}


static int builtin_set(shell_context_t *ctx, int argc, char *argv[]) {
	if(argc == 1 && cfg_environ) {
		for(int i = 0; cfg_environ[i] != NULL; i++)
			printf("%s\n", cfg_environ[i]);

	} else if(argc == 2) {
		cfg_unsetenv(argv[1]);

	} else if(argc == 3) {
		cfg_setenv(argv[1], argv[2]);

	} else {
		printf("Usage: %s [VARIABLE] [VALUE]\n", argv[0]);

		return -1;
	}

	return 0;
}


static int builtin_rebuildcfg(shell_context_t *ctx, int argc, char *argv[]) {
	if(argc != 1) {
		printf("Usage: %s\n", argv[0]);

		return -1;
	}

	return ingenic_rebuild(shell_device(ctx));
}

static int builtin_safe(shell_context_t *ctx, int argc, char *argv[]) {
	if(argc < 2) {
		printf("Usage: %s <COMMAND> [ARG]...\n", argv[0]);

		return -1;
	}

	if(shell_run(ctx, argc - 1, argv + 1) == -1)
		perror("shell_run");

	return 0;
}

