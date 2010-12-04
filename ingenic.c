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

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>

#include "ingenic.h"
#include "usbdev.h"
#include "debug.h"
#include "config.h"

#define HANDLE ingenic_handle_t *handle = hndl
#define BUILDTYPE(cmdset, id) (((cmdset) << 16) | (0x##id & 0xFFFF))
#define CPUID(id) ((id) & 0xFFFF)
#define CMDSET(id) (((id) & 0xFFFF0000) >> 16)

typedef struct {
	void *usb;
	uint32_t type;
	uint32_t total_sdram_size;

	const ingenic_callbacks_t *callbacks;
	void *callbacks_data;

	firmware_config_t cfg;
} ingenic_handle_t;

static const struct {
	const char * const magic;
	uint32_t id;
} magic_list[] = {
	{ "JZ4740V1", BUILDTYPE(CMDSET_SPL, 4740) },
	{ "JZ4750V1", BUILDTYPE(CMDSET_SPL, 4750) },
	{ "JZ4760V1", BUILDTYPE(CMDSET_SPL, 4760) },

	{ "Boot4740", BUILDTYPE(CMDSET_USBBOOT, 4740) },
	{ "Boot4750", BUILDTYPE(CMDSET_USBBOOT, 4750) },
	{ "Boot4760", BUILDTYPE(CMDSET_USBBOOT, 4760) },

	{ NULL, 0 }
};

static void hexdump(const void *data, size_t size) {
        const unsigned char *bytes = data;

        for(int i = 0; i < size; i+= 16) {
                debug(LEVEL_DEBUG, "%04X  ", i);

                int chunk_size = size - i;
		if(chunk_size > 16)
			chunk_size = 16;

                for(int j = 0; j < chunk_size; j++) {
                        debug(LEVEL_DEBUG, "%02X ", bytes[i + j]);

                        if(j == 7)
                                debug(LEVEL_DEBUG, " ");
                }

                for(int j = 0; j < 16 - chunk_size; j++) {
                        debug(LEVEL_DEBUG, "   ");

                        if(j == 8)
                                debug(LEVEL_DEBUG, " ");
                }

                debug(LEVEL_DEBUG, "|");

                for(int j = 0; j < chunk_size; j++) {
                        debug(LEVEL_DEBUG, "%c", isprint(bytes[i + j]) ? bytes[i + j] : '.');
                }

                debug(LEVEL_DEBUG, "|\n");
        }
}

static uint32_t ingenic_probe(void *usb_hndl) {
	char magic[9];

	if(usbdev_vendor(usb_hndl, USBDEV_FROMDEV, VR_GET_CPU_INFO, 0, 0, magic, 8) == -1) {
		if(errno == EFAULT) {
			debug(LEVEL_DEBUG, "Stage detected\n");
		} else
			return 0;
	}

	magic[8] = 0;

	for(int i = 0; magic_list[i].magic != NULL; i++)
		if(strcmp(magic_list[i].magic, magic) == 0) {
			debug(LEVEL_DEBUG, "Magic: '%s', type: %08X\n", magic, magic_list[i].id);

			return magic_list[i].id;
		}

	debug(LEVEL_ERROR, "Unknown CPU: '%s'\n", magic);
	errno = EINVAL;
	return 0;
}

void *ingenic_open(void *usb_hndl) {
	uint32_t type = ingenic_probe(usb_hndl);

	if(type == 0)
		return NULL;

	ingenic_handle_t *ret  = malloc(sizeof(ingenic_handle_t));
	ret->usb = usb_hndl;
	ret->type = type;
	ret->callbacks = NULL;

	return ret;
}

int ingenic_redetect(void *hndl) {
	HANDLE;

	uint32_t type = ingenic_probe(handle->usb);

	if(type == 0)
		return -1;

	uint32_t prev = handle->type;

	handle->type = type;

	if(CMDSET(prev) != CMDSET(type) && handle->callbacks && handle->callbacks->cmdset_change)
		handle->callbacks->cmdset_change(handle->callbacks_data);

	return 0;
}

void ingenic_set_callbacks(void *hndl, const ingenic_callbacks_t *callbacks, void *arg) {
	HANDLE;

	handle->callbacks = callbacks;
	handle->callbacks_data = arg;
}

int ingenic_cmdset(void *hndl) {
	HANDLE;

	return CMDSET(handle->type);
}

int ingenic_type(void *hndl) {
	HANDLE;

	return CPUID(handle->type);
}

void ingenic_close(void *hndl) {
	HANDLE;

	free(handle);
}

#define CFGOPT(name, var, exp) { char *str = cfg_getenv(name); if(str == NULL) { debug(LEVEL_ERROR, "%s is not set\n", name); errno = EINVAL; return -1; }; int v = atoi(str); if(!(exp)) { debug(LEVEL_ERROR, "%s must be in %s\n", name, #exp); return -1; }; handle->cfg.var = v; }

int ingenic_rebuild(void *hndl) {
	HANDLE;

	handle->cfg.cpu_id = 0x4750; //CPUID(handle->type);

	CFGOPT("EXTCLK", ext_clk, v <= 27 && v >= 12);
	CFGOPT("CPUSPEED", cpu_speed, (v % 12) == 0);
	handle->cfg.cpu_speed /= handle->cfg.ext_clk;
	CFGOPT("PHMDIV", phm_div, v <= 32 && v >= 2);
	CFGOPT("USEUART", use_uart, 1);
	CFGOPT("BAUDRATE", baudrate, 1);

	CFGOPT("SDRAM_BUSWIDTH", bus_width, (v == 16) || (v == 32));
	handle->cfg.bus_width = handle->cfg.bus_width == 16 ? 1 : 0;
	CFGOPT("SDRAM_BANKS", bank_num, (v >= 4) && ((v % 4) == 0));
	handle->cfg.bank_num /= 4;
	CFGOPT("SDRAM_ROWADDR", row_addr, 1);
	CFGOPT("SDRAM_COLADDR", col_addr, 1);
	CFGOPT("SDRAM_ISMOBILE", is_mobile, v == 0 || v == 1);
	CFGOPT("SDRAM_ISBUSSHARE", is_busshare, v == 0 || v == 1);
	CFGOPT("DEBUGOPS", debug_ops, 1);
	CFGOPT("PINNUM", pin_num, 1);
	CFGOPT("START", start, 1);
	CFGOPT("SIZE", size, 1);

	handle->total_sdram_size = (uint32_t)
		(2 << (handle->cfg.row_addr + handle->cfg.col_addr - 1)) * 2
		* (handle->cfg.bank_num + 1) * 2
		* (2 - handle->cfg.bus_width);

	debug(LEVEL_DEBUG, "Firmware configuration dump:\n");

	hexdump(&handle->cfg, sizeof(firmware_config_t));

	return 0;
}

static int ingenic_address(void *usb, uint32_t base) {
	debug(LEVEL_DEBUG, "Ingenic: address 0x%08X\n", base);

	return usbdev_vendor(usb, USBDEV_TODEV, VR_SET_DATA_ADDRESS, (base >> 16), base & 0xFFFF, 0, 0);
}

int ingenic_loadstage(void *hndl, int id, const char *file) {
	HANDLE;

	if(file == NULL) {
		errno = EINVAL;

		return -1;
	}

	uint32_t base;
	int cmd;

	switch(id) {
	case INGENIC_STAGE1:
		base = SDRAM_BASE + STAGE1_BASE;
		cmd = VR_PROGRAM_START1;

		break;

	case INGENIC_STAGE2:
		base = SDRAM_BASE + handle->total_sdram_size - STAGE2_CODESIZE;
		cmd = VR_PROGRAM_START2;

		break;

	default:
		errno = EINVAL;

		return -1;
	}

	FILE *fd = fopen(file, "rb");

	if(fd == NULL) {
		debug(LEVEL_ERROR, "Ingenic: cannot load file `%s'\n", file);

		return -1;
	}

	fseek(fd, 0, SEEK_END);
	int size = ftell(fd);
	fseek(fd, 0, SEEK_SET);

	void *data = malloc(size);
	fread(data, size, 1, fd);

	fclose(fd);

	memcpy(data + 8, &handle->cfg, sizeof(firmware_config_t));

	debug(LEVEL_DEBUG, "Ingenic: loading stage%d to 0x%08X, %d bytes\n", id, base, size);

	if(ingenic_address(handle->usb, base) == -1) {
		free(data);

		return -1;
	}

	hexdump(data, size);

	int ret = usbdev_sendbulk(handle->usb, data, size);

	free(data);

	if(ret == -1)
		return -1;

	debug(LEVEL_DEBUG, "Ingenic: stage written\n");

	if(id == INGENIC_STAGE2) {
		debug(LEVEL_DEBUG, "Ingenic: flushing cache\n");

		ret = usbdev_vendor(handle->usb, USBDEV_TODEV, VR_FLUSH_CACHES, 0, 0, 0, 0);

		if(ret == -1)
			return -1;
	}

	debug(LEVEL_DEBUG, "Starting stage!\n");

	ret = usbdev_vendor(handle->usb, USBDEV_TODEV, cmd, (base >> 16), base & 0xFFFF, 0, 0);

	if(ret == -1)
		return -1;

	return ingenic_redetect(hndl);
}
