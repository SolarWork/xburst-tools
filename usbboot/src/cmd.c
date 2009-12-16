/*
 * Copyright(C) 2009  Qi Hardware Inc.,
 * Authors: Marek Lindner <lindner_marek@yahoo.de>
 *          Xiangfu Liu <xiangfu@qi-hardware.com>
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
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include "cmd.h"
#include "ingenic_cfg.h"
#include "ingenic_usb.h"
#include "usb_boot_defines.h"

struct hand hand;
struct sdram_in sdram_in;

unsigned int total_size;
static char code_buf[4 * 512 * 1024];
static char ret[8];


static int load_file(struct ingenic_dev *ingenic_dev, const char *file_path)
{
	struct stat fstat;
	int fd, status, res = -1;

	status = stat(file_path, &fstat);

	if (status < 0) {
		fprintf(stderr, "Error - can't get file size from '%s': %s\n",
			file_path, strerror(errno));
		goto out;
	}

	ingenic_dev->file_len = fstat.st_size;
	ingenic_dev->file_buff = code_buf;

	fd = open(file_path, O_RDONLY);

	if (fd < 0) {
		fprintf(stderr, "Error - can't open file '%s': %s\n", 
			file_path, strerror(errno));
		goto out;
	}

	status = read(fd, ingenic_dev->file_buff, ingenic_dev->file_len);

	if (status < (int)ingenic_dev->file_len) {
		fprintf(stderr, "Error - can't read file '%s': %s\n", 
			file_path, strerror(errno));
		goto close;
	}

	/* write args to code */
	memcpy(ingenic_dev->file_buff + 8, &hand.fw_args, 
	       sizeof(struct fw_args));

	res = 1;

close:
	close(fd);
out:
	return res;
}

/* after upload stage2. must init device */
void init_cfg(struct ingenic_dev *dev)
{
	if (usb_get_ingenic_cpu(dev) < 3) {
		printf(" XBurst CPU not booted yet, boot it first!\n");
		return;
	}

	if (usb_send_data_to_ingenic(dev, (char*)&hand, sizeof(hand)) != 1)
		goto xout;

	if (usb_ingenic_configration(dev, DS_hand) != 1)
		goto xout;

	if (usb_read_data_from_ingenic(dev, ret, 8) != 1)
		goto xout;

	printf(" Configuring XBurst CPU succeeded.\n");
	return;
xout:
	printf("Configuring XBurst CPU failed.\n");
}

int boot(struct ingenic_dev *dev, const char *stage1_path, const char *stage2_path)
{
	int status;

	status = usb_get_ingenic_cpu(dev);
	switch (status)	{
	case 1:            /* Jz4740v1 */
		status = 0;
		hand.fw_args.cpu_id = 0x4740;
		break;
	case 2:            /* Jz4750v1 */
		status = 0;
		hand.fw_args.cpu_id = 0x4750;
		break;
	case 3:            /* Boot4740 */
		status = 1;
		hand.fw_args.cpu_id = 0x4740;
		break;
	case 4:            /* Boot4750 */
		status = 1;
		hand.fw_args.cpu_id = 0x4750;
		break;
	default:
		return 1;
	}

	if (status) {
		printf(" Already booted.\n");
		return 1;
	} else {
		printf(" CPU not yet booted, now booting...\n");

		/* now we upload the boot stage1 */
		printf(" Loading stage1 from '%s'\n", stage1_path);
		if (load_file(dev, stage1_path) < 1)
			return -1;

		if (usb_ingenic_upload(dev, 1) < 1)
			return -1;

		/* now we upload the boot stage2 */
		usleep(100);
		printf(" Loading stage2 from '%s'\n", stage2_path);
		if (load_file(dev, stage2_path) < 1)
			return -1;

		if (usb_ingenic_upload(dev, 2) < 1)
			return -1;

		printf(" Booted successfully!\n");
	}
	usleep(100);
	init_cfg(dev);
	return 1;
}


int debug_memory(struct ingenic_dev *dev, int obj, unsigned int start, unsigned int size)
{
	char buffer[8], tmp;

    (void)obj;

	tmp = usb_get_ingenic_cpu(dev);
	if (tmp  > 2) {
		printf(" This command only run under UNBOOT state!\n");
		return -1;
	}

	switch (tmp) {
	case 1:
		tmp = 0;
		hand.fw_args.cpu_id = 0x4740;
		break;
	case 2:
		tmp = 0;
		hand.fw_args.cpu_id = 0x4750;
		break;
	}

	hand.fw_args.debug_ops = 1;/* tell device it's memory debug */
	hand.fw_args.start = start;

	if (size == 0)
		hand.fw_args.size = total_size;
	else
		hand.fw_args.size = size;

	printf(" Now test memory from 0x%x to 0x%x: \n",
	       start, start + hand.fw_args.size);

	if (load_file(dev, STAGE1_FILE_PATH) < 1)
		return -1;
	if (usb_ingenic_upload(dev, 1) < 1)
		return -1;

	usleep(100);
	usb_read_data_from_ingenic(dev, buffer, 8);
	if (buffer[0] != 0)
		printf(" Test memory fail! Last error address is 0x%x !\n",
		       buffer[0]);
	else
		printf(" Test memory pass!\n");

	return 1;
}

int debug_go(struct ingenic_dev *dev, size_t argc, char *argv[])
{
	unsigned int addr,obj;
	if (argc < 3) {
		printf(" Usage: go (1) (2) \n"
		       " 1:start SDRAM address\n"
		       " 2:device index number\n");
		return 0;
	}

	addr = strtoul(argv[1], NULL, 0);
	obj = atoi(argv[2]);

	printf(" Executing No.%d device at address 0x%x\n", obj, addr);

	if (usb_ingenic_start(dev, VR_PROGRAM_START2, addr) < 1)
		return -1;

	return 1;
}

int sdram_load(struct ingenic_dev *dev, struct sdram_in *sdram_in)
{
	if (usb_get_ingenic_cpu(dev) < 3) {
		printf(" Device unboot! Boot it first!\n");
		return -1;
	}

	if (sdram_in->length > (unsigned int) MAX_LOAD_SIZE) {
		printf(" Image length too long!\n");
		return -1;
	}

	usb_send_data_to_ingenic(dev, sdram_in->buf, sdram_in->length);
	usb_send_data_address_to_ingenic(dev, sdram_in->start);
	usb_send_data_length_to_ingenic(dev, sdram_in->length);
/*	usb_ingenic_sdram_ops(dev, sdram_in);*/

	usb_read_data_from_ingenic(dev, ret, 8);
	printf(" Load last address at 0x%x\n",
	       ((ret[3]<<24)|(ret[2]<<16)|(ret[1]<<8)|(ret[0]<<0)));

	return 1;
}

int sdram_load_file(struct ingenic_dev *dev, struct sdram_in *sdram_in, char *file_path)
{
	struct stat fstat;
	unsigned int flen,m,j,offset,k;
	int fd, status, res = -1;

	status = stat(file_path, &fstat);
	if (status < 0) {
		fprintf(stderr, "Error - can't get file size from '%s': %s\n",
			file_path, strerror(errno));
		goto out;
	}
	flen = fstat.st_size;

	fd = open(file_path, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Error - can't open file '%s': %s\n",
			file_path, strerror(errno));
		goto out;
	}

	m = flen / MAX_LOAD_SIZE;
	j = flen % MAX_LOAD_SIZE;
	offset = 0;

	printf(" Total size to send in byte is :%d\n", flen);
	printf(" Loading data to SDRAM :\n");

	for (k = 0; k < m; k++) {
		status = read(fd, sdram_in->buf, MAX_LOAD_SIZE);
		if (status < MAX_LOAD_SIZE) {
			fprintf(stderr, "Error - can't read file '%s': %s\n",
				file_path, strerror(errno));
			goto close;
		}

		sdram_in->length = MAX_LOAD_SIZE;
		if (sdram_load(dev, sdram_in) < 1)
			goto close;

		sdram_in->start += MAX_LOAD_SIZE;
		if ( k % 60 == 0) 
			printf(" 0x%x \n", sdram_in->start);
	}

	if (j) {
		if (j % 4 !=0)
			j += 4 - (j % 4);
		status = read(fd, sdram_in->buf, j);
		if (status < (int)j) {
			fprintf(stderr, "Error - can't read file '%s': %s\n",
				file_path, strerror(errno));
			goto close;
		}

		sdram_in->length = j;
		if (sdram_load(dev, sdram_in) < 1)
			goto close;
	}

	res = 1;

close:
	close(fd);
out:
	return res;
}
