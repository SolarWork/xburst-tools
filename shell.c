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
#ifdef WITH_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif
#include <stdlib.h>
#include <errno.h>

#include "shell.h"
#include "debug.h"
#include "ingenic.h"
#include "config.h"

static void *device = NULL;
static char linebuf[512];
char *strval = NULL, *line = NULL;

int yylex();
void yyrestart(FILE *new_file);

static int builtin_help(int argc, char *argv[]);
static int builtin_exit(int argc, char *argv[]);
static int builtin_source(int argc, char *argv[]);
static int builtin_echo(int argc, char *argv[]);
static int builtin_redetect(int argc, char *argv[]);
static int builtin_rebuildcfg(int argc, char *argv[]);
static int builtin_set(int argc, char *argv[]);

static const shell_command_t commands[] = {
	{ "help", "- Display this message", builtin_help },
	{ "exit", "- Batch: stop current script, interactive: end session", builtin_exit },
	{ "source", "<FILENAME> - run specified script", builtin_source },
	{ "echo", "<STRING> - output specified string", builtin_echo },
	{ "set", "[VARIABLE] [VALUE] - print or set configuraton variables", builtin_set },

	{ "redetect", " - Redetect CPU", builtin_redetect },
	{ "rebuildcfg", " - Rebuild firmware configuration data", builtin_rebuildcfg },

	{ NULL, NULL, NULL }
};

static void shell_update_cmdset(void *arg);

static const ingenic_callbacks_t shell_callbacks = {
	shell_update_cmdset,
};

static const shell_command_t *set_cmds = NULL;
static int shell_exit = 0;

int shell_init(void *ingenic) {
#ifdef WITH_READLINE
	rl_initialize();
#endif

	debug(LEVEL_DEBUG, "Initializing shell\n");

	device = ingenic;

	ingenic_set_callbacks(ingenic, &shell_callbacks, NULL);

	shell_update_cmdset(NULL);

	return 0;
}

#define STATE_WANTSTR	0
#define STATE_WANTSPACE	1

int shell_run(int argc, char *argv[]) {
	for(int i = 0; commands[i].cmd != NULL; i++)
		if(strcmp(commands[i].cmd, argv[0]) == 0)
			return commands[i].handler(argc, argv);

	if(set_cmds) {
		for(int i = 0; set_cmds[i].cmd != NULL; i++)
			if(strcmp(set_cmds[i].cmd, argv[0]) == 0)
				return set_cmds[i].handler(argc, argv);
	}

	debug(LEVEL_ERROR, "Bad command '%s'\n", argv[0]);

	errno = EINVAL;
	return -1;
}

int shell_execute(const char *cmd) {
	line = strdup(cmd);
	char *ptr = line;

	int token;
	int state = STATE_WANTSTR;
	int argc = 0;
	char **argv = NULL;

	yyrestart(NULL);

	do {
		int noway = 0;

		token = yylex();

		if((token == TOK_SEPARATOR || token == TOK_COMMENT || token == 0)) {
			if(argc > 0) {
				int ret = shell_run(argc, argv);

				for(int i = 0; i < argc; i++) {
					free(argv[i]);
				}

				free(argv);

				argv = NULL;
				argc = 0;

				if(ret == -1) {
					free(ptr);

					return -1;
				}
			}

			state = STATE_WANTSTR;
		} else {
			switch(state) {
			case STATE_WANTSTR:
				if(token == TOK_SPACE) {
					state = STATE_WANTSTR;
				} else if(token == TOK_STRING) {
					int oargc = argc++;

					argv = realloc(argv, sizeof(char *) * argc);

					argv[oargc] = strval;

					state = STATE_WANTSPACE;
				} else {
					noway = 1;
				}

				break;

			case STATE_WANTSPACE:
				if(token == TOK_STRING) {
					free(strval);

					noway = 1;
				} else if(token == TOK_SPACE) {
					state = STATE_WANTSTR;
				} else
					noway = 1;
			}

			if(noway) {
				debug(LEVEL_ERROR, "No way from state %d by token %d\n", state, token);

				for(int i = 0; i < argc; i++)
					free(argv[i]);

				free(argv);
				free(ptr);

				return -1;
			}
		}

	} while(token && token != TOK_COMMENT);

	free(ptr);

	return 0;
}

int shell_pull(char *buf, int maxlen) {
	size_t len = strlen(line);

	if(len < maxlen)
		maxlen = len;

	memcpy(buf, line, maxlen);

	line += maxlen;

	return maxlen;
}

void shell_fini() {
	device = NULL;
}

int shell_source(const char *filename) {
	shell_exit = 0;

	FILE *file = fopen(filename, "r");

	if(file == NULL) {
		return -1;
	}

	char *line;

	while((line = fgets(linebuf, sizeof(linebuf), file)) && !shell_exit) {
		if(shell_execute(line) == -1) {
			fclose(file);

			return -1;
		}
	}

	fclose(file);

	return 0;
}

void shell_interactive() {
	shell_exit = 0;

#ifndef WITH_READLINE
	char *line;

	while(!shell_exit) {
		fputs("jzboot> ", stdout);
		fflush(stdout);

		line = fgets(linebuf, sizeof(linebuf), stdin);

		if(line == NULL)
			break;

		shell_execute(line);
	}
#else

	rl_set_signals();

	while(!shell_exit) {
		char *line = readline("jzboot> ");

		if(line == NULL) {
			printf("line null, EOP\n");
			break;
		}

		add_history(line);

		shell_execute(line);

		free(line);
	}

	rl_clear_signals();
#endif
}

static int builtin_help(int argc, char *argv[]) {
	for(int i = 0; commands[i].cmd != NULL; i++) {
		printf("%s %s\n", commands[i].cmd, commands[i].description);
	}

	if(set_cmds) {
		for(int i = 0; set_cmds[i].cmd != NULL; i++)
			printf("%s %s\n", set_cmds[i].cmd, set_cmds[i].description);
	}

	return 0;
}

static int builtin_exit(int argc, char *argv[]) {
	shell_exit = 1;

	return 0;
}

static int builtin_source(int argc, char *argv[]) {
	if(argc != 2) {
		printf("Usage: %s <FILENAME>\n", argv[0]);

		return -1;
	}

	int ret = shell_source(argv[1]);

	if(ret == -1) {
		printf("Error while sourcing file %s\n", argv[1]);
	}

	shell_exit = 0;

	return ret;
}

static int builtin_echo(int argc, char *argv[]) {
	if(argc < 2) {
		printf("Usage: %s <STRING>\n", argv[0]);

		return -1;
	}

	puts(argv[1]);

	return 0;
}

static int builtin_redetect(int argc, char *argv[]) {
	if(argc != 1) {
		printf("Usage: %s\n", argv[0]);

		return -1;
	}

	if(ingenic_redetect(device) == -1) {
		perror("ingenic_redetect");

		return -1;
	} else
		return 0;
}


static int builtin_set(int argc, char *argv[]) {
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


static int builtin_rebuildcfg(int argc, char *argv[]) {
	if(argc != 1) {
		printf("Usage: %s\n", argv[0]);

		return -1;
	}

	return ingenic_rebuild(device);
}

static const struct {
	int set;
	const char *name;
	const shell_command_t *commands;
} cmdsets[] = {
	{ CMDSET_SPL, "SPL", spl_cmdset },
	{ 0,          NULL,  NULL }
};

static void shell_update_cmdset(void *arg) {
	set_cmds = NULL;

	int set = ingenic_cmdset(device);

	for(int i = 0; cmdsets[i].name != NULL; i++) {
		if(cmdsets[i].set == set) {
			printf("Shell: using command set '%s', run 'help' for command list. CPU: %04X\n", cmdsets[i].name, ingenic_type(device));

			set_cmds = cmdsets[i].commands;

			return;
		}
	}

	debug(LEVEL_ERROR, "Shell: unknown cmdset %d\n", set);
}

void *shell_device() {
	return device;
}
