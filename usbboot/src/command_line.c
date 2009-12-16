/*
 * Copyright(C) 2009  Qi Hardware Inc.,
 * Authors: Xiangfu Liu <xiangfu@qi-hardware.com>
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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "usb_boot_defines.h"
#include "ingenic_usb.h"
#include "cmd.h"
#include "xburst-tools_version.h"
#include "nand.h"
#include "mem.h"

extern struct nand_in nand_in;
extern struct sdram_in sdram_in;
static char code_buf[4 * 512 * 1024];
extern struct ingenic_dev ingenic_dev;
typedef int (*command_callback_t)(size_t argc, char *argv[]);

struct command {
	const char *name;
	command_callback_t callback;
};

#define COMMAND(_name, _callback) {\
	.name = _name, \
	.callback = (command_callback_t)_callback, \
}

static const char COMMANDS[][COMMAND_NUM]=
{
	"go",
	"fconfig",
	"boot", /* index 20 */
	"list",
	"select",
	"unselect",
	"chip",
	"unchip",
	"nmark",
	"nmake",
	"load",
	"memtest",
	"run",
};

static unsigned long parse_number(const char *s, int *err)
{
	unsigned long val = 0;
	unsigned int base = 10;
	char *endptr;

	if (s == 0 || *s == 0) {
	    if (err)
	        *err = 1;
	    return 0;
	}

	if (*s == '0') {
	    ++s;
	    if (*s == 'x') {
	        base = 16;
	        ++s;
	    } else if (*s != 0) {
	        base = 8;
	    }
	} else if (*s == 'b') {
	    ++s;
	    base = 2;
	}

	val = strtoul(s, &endptr, base);

	if (*endptr) {
	    if (err)
	        *err = 1;
	    return 0;
	}

	if (err)
	    *err = 0;

	return val;
}

static unsigned long parse_number_print_error(const char *s, int *err)
{
	unsigned long value;
	int err2 = 0;
	value = parse_number(s, &err2);
	if (err2) {
	    fprintf(stderr, "Error: %s is not a number\n", s);
	    if (err)
	        ++err;
	}

	return value;
}

static int handle_exit()
{
	exit(0);
	return 0;
}

static int handle_help()
{
	printf(" command support in current version:\n"
	/* " query" */
	/* " querya" */
	/* " erase" */
	/* " read" */
	/* " prog" */
	" nquery        query NAND flash info\n"
	" nerase        erase NAND flash\n"
	" nread         read NAND flash data with checking bad block and ECC\n"
	" nreadraw      read NAND flash data without checking bad block and ECC\n"
	" nreadoo       read NAND flash oob without checking bad block and ECC\n" /* index 10 */
	" nprog         program NAND flash with data and ECC\n"
	" ndump         dump NAND flash data to file\n"
	" help          print this help\n"
	" version       show current USB Boot software version\n"
	" go            execute program in SDRAM\n"
	" fconfig       set USB Boot config file(not implement)\n"
	" exit          quit from current session\n"
	" readnand      read data from nand flash and store to SDRAM\n"
	" boot          boot device and make it in stage2\n" /* index 20 */
	" list          show current device number can connect(not implement)\n"
	/* " select" */
	/* " unselect" */
	/* " chip" */
	/* " unchip" */
	" nmark        mark a bad block in NAND flash\n"
	" nmake        read all data from nand flash and store to file(not implement)\n"
	" load         load file data to SDRAM\n"
	" memtest      do SDRAM test\n"
	" run          run command script in file(implement by -c args)\n"
	" sdprog       program SD card(not implement)\n"
	" sdread       read data from SD card(not implement)\n");

	return 0;
}

static int handle_version()
{
	printf("USB Boot Software current version: %s\n", XBURST_TOOLS_VERSION);

	return 0;
}

static int handle_boot()
{
	boot(&ingenic_dev, STAGE1_FILE_PATH, STAGE2_FILE_PATH);

	return 0;
}

static int handle_nand_erase(size_t argc, char *argv[])
{
	uint32_t start_block, num_blocks;
	unsigned int device_idx;
	uint8_t nand_idx;
	int err;

	if (argc < 5) {
		printf(" Usage: nerase (1) (2) (3) (4)\n"
			   " 1:start block number\n"
			   " 2:block length\n"
			   " 3:device index number\n"
			   " 4:flash chip index number\n");
		return -1;
	}

	start_block = parse_number_print_error(argv[1], &err);
	num_blocks =  parse_number_print_error(argv[2], &err);
	device_idx =  parse_number_print_error(argv[3], &err);
	nand_idx =    parse_number_print_error(argv[4], &err);
	if (err)
	    return err;

	if (nand_idx >= MAX_DEV_NUM) {
		printf(" Flash index number overflow!\n");
		return -1;
	}

	if (nand_erase(&ingenic_dev, nand_idx, start_block, num_blocks))
		return -1;

	return 0;
}

static int handle_nand_mark(size_t argc, char *argv[])
{
	uint32_t block;
	unsigned int device_idx;
	uint8_t nand_idx;
	int err = 0;

	if (argc < 4) {
		printf("Usage: nerase (1) (2) (3)\n"
			   "1: bad block number\n"
			   "2: device index number\n"
			   "3: flash chip index number\n");
		return -1;
	}

	block =      parse_number_print_error(argv[1], &err);
	device_idx = parse_number_print_error(argv[2], &err);
	nand_idx =   parse_number_print_error(argv[3], &err);
	if (err)
	    return err;

	if (nand_idx >= MAX_DEV_NUM) {
		printf("Flash index number overflow!\n");
		return -1;
	}

	nand_markbad(&ingenic_dev, nand_idx, block);

	return 0;
}

static int handle_memtest(size_t argc, char *argv[])
{
	unsigned int device_idx;
	unsigned int start, size;
	int err = 0;

	if (argc != 2 && argc != 4)
	{
		printf(" Usage: memtest (1) [2] [3]\n"
			   " 1: device index number\n"
			   " 2: SDRAM start address\n"
			   " 3: test size\n");
		return -1;
	}

	if (argc == 4) {
	    start = parse_number_print_error(argv[2], &err);
	    size =  parse_number_print_error(argv[3], &err);
	} else {
		start = 0;
		size = 0;
	}
	device_idx = parse_number_print_error(argv[1], &err);
	if (err)
	    return err;

	debug_memory(&ingenic_dev, device_idx, start, size);
	return 0;
}

static int handle_load(size_t argc, char *argv[])
{
	if (argc != 4) {
		printf(" Usage:"
			   " load (1) (2) (3) \n"
			   " 1:SDRAM start address\n"
			   " 2:image file name\n"
			   " 3:device index number\n");

		return -1;
	}

	sdram_in.start=strtoul(argv[1], NULL, 0);
	printf(" start:::::: 0x%x\n", sdram_in.start);

	sdram_in.dev = atoi(argv[3]);
	sdram_in.buf = code_buf;
	sdram_load_file(&ingenic_dev, &sdram_in, argv[2]);
	return 0;
}

static size_t command_parse(char *cmd, char *argv[])
{
	size_t argc = 0;

	if (cmd == 0 || *cmd == 0)
	    return 0;


	while (isspace(*cmd)) {
	    ++cmd;
	}

	argv[0] = cmd;
	argc = 1;

	while (*cmd) {
	    if (isspace(*cmd)) {
	        *cmd = 0;

	        do {
	            ++cmd;
	        } while (isspace(*cmd));

	        if (*cmd == 0 || argc >= MAX_ARGC)
	            break;

	        argv[argc] = cmd;
	        ++argc;
	    }

	    ++cmd;
	}

	return argc;
}

static int handle_nand_read(size_t argc, char *argv[])
{
	int mode;
	uint32_t start_page, num_pages;
	unsigned int device_idx;
	uint8_t nand_idx;
	int err = 0;

	if (argc != 5) {
	    printf("Usage: %s <start page> <length> <device index> "
	                "<nand chip index>\n", argv[0]);
	    return -1;
	}

	if (strcmp(argv[0], "nread") == 0) {
	    mode = NAND_READ;
	} else if (strcmp(argv[0], "nreadraw") == 0) {
	    mode = NAND_READ_RAW;
	} else if (strcmp(argv[0], "nreadoob") == 0) {
	    mode = NAND_READ_OOB;
	} else {
	    return -1;
	}

	start_page =  parse_number_print_error(argv[1], &err);
	num_pages =   parse_number_print_error(argv[2], &err);
	device_idx =  parse_number_print_error(argv[3], &err);
	nand_idx =    parse_number_print_error(argv[4], &err);
	if (err)
	    return err;

	return nand_read(&ingenic_dev, nand_idx, mode, start_page, num_pages, 0);
}

static int handle_nand_dump(size_t argc, char *argv[])
{
	int mode;
	uint32_t start_page, num_pages;
	unsigned int device_idx;
	uint8_t nand_idx;
	int err = 0;

	if (argc != 5) {
	    printf("Usage: %s <start page> <length> <filename> <mode>\n", argv[0]);
	    return -1;
	}

	if (strcmp(argv[5], "-n") == 0) {
	    mode = NAND_READ;
	} else if (strcmp(argv[5], "-e") == 0) {
	    mode = NAND_READ_RAW;
	} else if (strcmp(argv[5], "-o") == 0) {
	    mode = NAND_READ_OOB;
	} else {
	    return -1;
	}

	start_page =  parse_number_print_error(argv[1], &err);
	num_pages =   parse_number_print_error(argv[2], &err);
	device_idx =  parse_number_print_error(argv[3], &err);
	nand_idx =    parse_number_print_error(argv[4], &err);
	if (err)
	    return err;

	return nand_read(&ingenic_dev, nand_idx, mode, start_page, num_pages, 0);

}

static int handle_nand_query(size_t argc, char *argv[])
{
	unsigned int device_idx;
	uint8_t nand_idx;
	int err = 0;

	if (argc != 3) {
	    printf("Usage: %s <device index> <nand chip index>\n", argv[0]);
	    return -1;
	}

	device_idx =  parse_number_print_error(argv[1], &err);
	nand_idx =    parse_number_print_error(argv[2], &err);
	if (err)
	    return err;

	return nand_query(&ingenic_dev, nand_idx);
}

static int handle_nand_prog(size_t argc, char *argv[])
{
	uint32_t start_page;
	unsigned int device_idx;
	uint8_t nand_idx, mode = -1;
	int err = 0;

	if (argc != 5) {
		printf("Usage: %s <start page> <filename> <device index> <nand chip index> <mode>\n", argv[0]);
		return -1;
	}

	start_page = parse_number_print_error(argv[1], &err);
	device_idx = parse_number_print_error(argv[3], &err);
	nand_idx =   parse_number_print_error(argv[4], &err);

	if (argv[5][0] == '-') {
		switch (argv[5][1]) {
		case 'e':
			mode = NO_OOB;
			break;
		case 'o':
			mode = OOB_NO_ECC;
			break;
		case 'r':
			mode = OOB_ECC;
			break;
		default:
			break;
		}
	}
	if (mode == -1) {
		printf("%s: Invalid mode '%s'\n", argv[0], argv[5]);
		err = -1;
	}

	if (err)
		return err;


	nand_prog(&ingenic_dev, nand_idx, start_page, argv[2], mode);

	return 0;
}

static int handle_mem_read(size_t argc, char *argv[])
{
	uint32_t addr;
	uint32_t val;
	int err = 0;
	if (argc != 2)
	    printf("Usage: %s <addr>\n", argv[0]);

	addr = parse_number_print_error(argv[1], &err);
	if (err)
	    return err;

	switch (argv[0][7]) {
	case '8':
		val = mem_read8(&ingenic_dev, addr);
		break;
	case '1':
		val = mem_read16(&ingenic_dev, addr);
		break;
	default:
		val = mem_read32(&ingenic_dev, addr);
		break;
	}

	printf("0x%x = 0x%x\n", addr, val);

	return 0;
}

static int handle_mem_write(size_t argc, char *argv[])
{
	uint32_t addr;
	uint32_t val;
	int err = 0;

	if (argc != 3)
	    printf("Usage: %s <addr> <value>\n", argv[0]);

	addr = parse_number_print_error(argv[1], &err);
	val = parse_number_print_error(argv[2], &err);
	if (err)
	    return err;

	switch (argv[0][8]) {
	case '8':
		mem_write8(&ingenic_dev, addr, val);
		break;
	case '1':
		mem_write16(&ingenic_dev, addr, val);
		break;
	default:
		mem_write32(&ingenic_dev, addr, val);
		break;
	}

	printf("0x%x = 0x%x\n", addr, val);

	return 0;
}


static const struct command commands[] = {
	COMMAND("version",	handle_version),
	COMMAND("help",		handle_help),
	COMMAND("nquery",	handle_nand_query),
	COMMAND("nerase",	handle_nand_erase),
	COMMAND("nread",	handle_nand_read),
	COMMAND("nreadraw",	handle_nand_read),
	COMMAND("nreadoo",	handle_nand_read),
	COMMAND("nprog",	handle_nand_prog),
	COMMAND("nwrite",	handle_nand_prog),
	COMMAND("nmark",	handle_nand_mark),
	COMMAND("ndump",	handle_nand_dump),
	COMMAND("exit",		handle_exit),
	COMMAND("boot",		handle_boot),
	COMMAND("memread",	handle_mem_read),
	COMMAND("memwrite",	handle_mem_write),
	COMMAND("memread16",	handle_mem_read),
	COMMAND("memwrite16",	handle_mem_write),
	COMMAND("memread8",	handle_mem_read),
	COMMAND("memwrite8",	handle_mem_write),
	COMMAND("memtest",		handle_memtest),
	COMMAND("load",		handle_load),
};

int command_handle(char *buf)
{
	size_t argc;
	char *argv[MAX_ARGC];
	size_t i;

	argc = command_parse(buf, argv);

	if (argc == 0)
	    return 0;

	for (i = 0; i < ARRAY_SIZE(commands); ++i) {
	    if (strcmp(commands[i].name, argv[0]) == 0)
	        return commands[i].callback(argc, argv);
	}

	printf("Unknow command \"%s\"\n", argv[0]);

	return -1;
}
