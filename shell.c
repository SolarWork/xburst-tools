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
#include <unistd.h>
#include <errno.h>

#include "shell_internal.h"
#include "debug.h"
#include "ingenic.h"

static void shell_update_cmdset(void *arg);

static const ingenic_callbacks_t shell_callbacks = {
	shell_update_cmdset,
};

static const struct {
	int set;
	const char *name;
	const shell_command_t *commands;
} cmdsets[] = {
	{ CMDSET_SPL,     "SPL",     spl_cmdset },
	{ CMDSET_USBBOOT, "USBBoot", usbboot_cmdset },
	{ 0,          NULL,  NULL }
};

shell_context_t *shell_init(void *ingenic) {
#ifdef WITH_READLINE
	rl_initialize();
#endif

	debug(LEVEL_DEBUG, "Initializing shell\n");

	shell_context_t *ctx = malloc(sizeof(shell_context_t));
	memset(ctx, 0, sizeof(shell_context_t));
	ctx->device = ingenic;

	ingenic_set_callbacks(ingenic, &shell_callbacks, ctx);

	shell_update_cmdset(ctx);

	return ctx;
}

int shell_enumerate_commands(shell_context_t *ctx, int (*callback)(shell_context_t *ctx, const shell_command_t *cmd, void *arg), void *arg) {
	for(int i = 0; builtin_cmdset[i].cmd != NULL; i++) {
		int ret = callback(ctx, builtin_cmdset + i, arg);

		if(ret != 0)
			return ret;
	}

	if(ctx->set_cmds)
		for(int i = 0; ctx->set_cmds[i].cmd != NULL; i++) {
			int ret = callback(ctx, ctx->set_cmds + i, arg);

			if(ret != 0)
				return ret;
		}

	return 0;
}

static int shell_run_function(shell_context_t *ctx, const shell_command_t *cmd, void *arg) {
	shell_run_data_t *data = arg;

	if(strcmp(cmd->cmd, data->argv[0]) == 0) {
		int ret = cmd->handler(ctx, data->argc, data->argv);

		if(ret == 0)
			return 1;
		else
			return ret;
	} else
		return 0;
}

int shell_run(shell_context_t *ctx, int argc, char *argv[]) {
	shell_run_data_t data = { argc, argv };

	int ret = shell_enumerate_commands(ctx, shell_run_function, &data);

	if(ret != 0) {
		debug(LEVEL_ERROR, "Bad command '%s'\n", argv[0]);

		errno = EINVAL;
		return -1;

	} else if(ret == 1) {
		return 0;
	} else
		return ret;
}

int shell_execute(shell_context_t *ctx, const char *cmd) {
	yyscan_t scanner;
	if(yylex_init_extra(ctx, &scanner) == -1)
		return -1;

	ctx->line = strdup(cmd);
	char *ptr = ctx->line;

	int token;
	int state = STATE_WANTSTR;
	int argc = 0;
	char **argv = NULL;
	int fret = -1;

	do {
		int noway = 0;

		token = yylex(&scanner);

		if((token == TOK_SEPARATOR || token == TOK_COMMENT || token == 0)) {
			if(argc > 0) {
				int ret = shell_run(ctx, argc, argv);

				for(int i = 0; i < argc; i++) {
					free(argv[i]);
				}

				free(argv);

				argv = NULL;
				argc = 0;

				if(ret == -1) {
					fret = -1;

					break;
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

					argv[oargc] = ctx->strval;

					state = STATE_WANTSPACE;
				} else {
					noway = 1;
				}

				break;

			case STATE_WANTSPACE:
				if(token == TOK_STRING) {
					free(ctx->strval);

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

				fret = -1;

				break;
			}
		}

	} while(token && token != TOK_COMMENT);

	free(ptr);

	yylex_destroy(&scanner);

	return fret;
}

int shell_pull(shell_context_t *ctx, char *buf, int maxlen) {
	size_t len = strlen(ctx->line);

	if(len < maxlen)
		maxlen = len;

	memcpy(buf, ctx->line, maxlen);

	ctx->line += maxlen;

	return maxlen;
}

void shell_fini(shell_context_t *ctx) {
	free(ctx);
}

int shell_source(shell_context_t *ctx, const char *filename) {
	ctx->shell_exit = 0;

	FILE *file = fopen(filename, "r");

	if(file == NULL) {
		return -1;
	}

	char *line;

	while((line = fgets(ctx->linebuf, sizeof(ctx->linebuf), file)) && !ctx->shell_exit) {
		if(shell_execute(ctx, line) == -1) {
			fclose(file);

			return -1;
		}
	}

	fclose(file);

	return 0;
}

void shell_interactive(shell_context_t *ctx) {
	ctx->shell_exit = 0;

#ifndef WITH_READLINE
	char *line;

	while(!ctx->shell_exit) {
		fputs("jzboot> ", stdout);
		fflush(stdout);

		line = fgets(ctx->linebuf, sizeof(ctx->linebuf), stdin);

		if(line == NULL)
			break;

		shell_execute(ctx, line);
	}
#else

	rl_set_signals();

	while(!ctx->shell_exit) {
		char *line = readline("jzboot> ");

		if(line == NULL) {
			break;
		}

		add_history(line);

		shell_execute(ctx, line);

		free(line);
	}

	rl_clear_signals();
#endif
}

static void shell_update_cmdset(void *arg) {
	shell_context_t *ctx = arg;

	ctx->set_cmds = NULL;

	int set = ingenic_cmdset(ctx->device);

	for(int i = 0; cmdsets[i].name != NULL; i++) {
		if(cmdsets[i].set == set) {
			printf("Shell: using command set '%s', run 'help' for command list. CPU: %04X\n", cmdsets[i].name, ingenic_type(ctx->device));

			ctx->set_cmds = cmdsets[i].commands;

			return;
		}
	}

	debug(LEVEL_ERROR, "Shell: unknown cmdset %d\n", set);
}

void *shell_device(shell_context_t *ctx) {
	return ctx->device;
}

void shell_exit(shell_context_t *ctx, int val) {
	ctx->shell_exit = val;
}

