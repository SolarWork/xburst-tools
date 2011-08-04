/*
 * Copyright(C) 2009  Qi Hardware Inc.,
 * Authors: Marek Lindner <lindner_marek@yahoo.de>
 *          Xiangfu Liu <xiangfu@sharism.cc>
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
#include "ingenic_request.h"
#include "usb_boot_defines.h"

extern int com_argc;
extern char com_argv[MAX_ARGC][MAX_COMMAND_LENGTH];
extern char * stage1;

struct ingenic_dev ingenic_dev;
struct hand hand;
struct sdram_in sdram_in;
struct nand_in nand_in;

unsigned int total_size;
unsigned char code_buf[4 * 512 * 1024];
unsigned char check_buf[4 * 512 * 1024];
unsigned char cs[16];
unsigned char ret[8];

static const char IMAGE_TYPE[][30] = {
	"with oob and ecc",
	"with oob and without ecc",
	"without oob",
};

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

	if (status < ingenic_dev->file_len) {
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

int get_ingenic_cpu()
{
	int status;

	status = usb_get_ingenic_cpu(&ingenic_dev);

	switch (status)	{
	case JZ4740V1:
		hand.fw_args.cpu_id = 0x4740;
		break;
	case JZ4750V1:
		hand.fw_args.cpu_id = 0x4750;
		break;
	case JZ4760V1:
		hand.fw_args.cpu_id = 0x4760;
		break;
	case BOOT4740:
		hand.fw_args.cpu_id = 0x4740;
		break;
	case BOOT4750:
		hand.fw_args.cpu_id = 0x4750;
		break;
	case BOOT4760:
		hand.fw_args.cpu_id = 0x4760;
		break;
	default:
		hand.fw_args.cpu_id = 0;
	}

	return status;
}

/* after upload stage2. must init device */
void init_cfg()
{
	int cpu = get_ingenic_cpu();
	if (cpu != BOOT4740 && cpu != BOOT4750 && cpu != BOOT4760) {
		printf(" Device unboot! boot it first!\n");
		return;
	}

	ingenic_dev.file_buff = &hand;
	ingenic_dev.file_len = sizeof(hand);
	if (usb_send_data_to_ingenic(&ingenic_dev) != 1)
		goto xout;

	sleep(1);
	if (usb_ingenic_configration(&ingenic_dev, DS_hand) != 1)
		goto xout;

	if (usb_read_data_from_ingenic(&ingenic_dev, ret, 8) != 1)
		goto xout;

	printf(" Configuring XBurst CPU succeeded.\n");
	return;
xout:
	printf(" Configuring XBurst CPU failed.\n");
}

int boot(char *stage1_path, char *stage2_path){
	int status = get_ingenic_cpu();

	if (status == BOOT4740 || status == BOOT4750 || status == BOOT4760) {
		printf(" Already booted.\n");
		return 1;
	} else {
		printf(" CPU not yet booted, now booting...\n");

		/* now we upload the boot stage1 */
		printf(" Loading stage1 from '%s'\n", stage1_path);
		if (load_file(&ingenic_dev, stage1_path) < 1)
			return -1;

		if (usb_ingenic_upload(&ingenic_dev, 1) < 1)
			return -1;

		/* now we upload the boot stage2 */
		usleep(100);
		printf(" Loading stage2 from '%s'\n", stage2_path);
		if (load_file(&ingenic_dev, stage2_path) < 1)
			return -1;

		if (usb_ingenic_upload(&ingenic_dev, 2) < 1)
			return -1;

		printf(" Booted successfully!\n");
	}

	usleep(100);
	init_cfg();

	return 1;
}

/* nand function  */
int error_check(unsigned char *org,unsigned char * obj,unsigned int size)
{
	unsigned int i;
	printf(" Comparing %d bytes - ", size);
	for (i = 0; i < size; i++) {
		if (org[i] != obj[i]) {
			unsigned int s = (i < 8) ? i : i - 8; // start_dump
			printf("FAIL at off %d, wrote 0x%x, read 0x%x\n", i, org[i], obj[i]);
			printf("  off %d write: %02x %02x %02x %02x %02x %02x %02x %02x"
			       " %02x %02x %02x %02x %02x %02x %02x %02x\n", s,
				org[s], org[s+1], org[s+2], org[s+3], org[s+4], org[s+5], org[s+6], org[s+7],
				org[s+8], org[s+9], org[s+10], org[s+11], org[s+12], org[s+13], org[s+14], org[s+15]);
			printf("  off %d read:  %02x %02x %02x %02x %02x %02x %02x %02x"
			       " %02x %02x %02x %02x %02x %02x %02x %02x\n", s,
				obj[s], obj[s+1], obj[s+2], obj[s+3], obj[s+4], obj[s+5], obj[s+6], obj[s+7],
				obj[s+8], obj[s+9], obj[s+10], obj[s+11], obj[s+12], obj[s+13], obj[s+14], obj[s+15]);
			fflush(NULL);
			return 0;
		}
	}
	printf("SUCCESS\n");
	fflush(NULL);
	return 1;
}

int nand_markbad(struct nand_in *nand_in)
{
	int cpu = get_ingenic_cpu();
	if (cpu != BOOT4740 && cpu != BOOT4750 && cpu != BOOT4760) {
		printf(" Device unboot! boot it first!\n");
		return -1;
	}
	printf(" Mark bad block : %d\n",nand_in->start);
	usb_send_data_address_to_ingenic(&ingenic_dev, nand_in->start);
	usb_ingenic_nand_ops(&ingenic_dev, NAND_MARK_BAD);
	usb_read_data_from_ingenic(&ingenic_dev, ret, 8);
	printf(" Mark bad block at %d\n",((ret[3] << 24) |
					   (ret[2] << 16) |
					   (ret[1] << 8)  |
					   (ret[0] << 0)) / hand.nand_ppb);
	return 0;
}

int nand_program_check(struct nand_in *nand_in, unsigned int *start_page)
{
	unsigned int i, page_num, cur_page = -1;
	unsigned int start_addr;
	unsigned short temp;
	int status = -1;

	printf(" Writing NAND page %d len %d...\n", nand_in->start, nand_in->length);
	if (nand_in->length > (unsigned int)MAX_TRANSFER_SIZE) {
		printf(" Buffer size too long!\n");
		goto err;
	}

	int cpu = get_ingenic_cpu();
	if (cpu != BOOT4740 && cpu != BOOT4750 && cpu != BOOT4760) {
		printf(" Device unboot! boot it first!\n");
		goto err;
	}

	ingenic_dev.file_buff = nand_in->buf;
	ingenic_dev.file_len = nand_in->length;
	usb_send_data_to_ingenic(&ingenic_dev);
	for (i = 0; i < nand_in->max_chip; i++) {
		if ((nand_in->cs_map)[i] == 0)
			continue;
		if (nand_in->option == NO_OOB) {
			page_num = nand_in->length / hand.nand_ps;
			if ((nand_in->length % hand.nand_ps) !=0)
				page_num++;
		} else {
			page_num = nand_in->length /
				(hand.nand_ps + hand.nand_os);
			if ((nand_in->length% (hand.nand_ps + hand.nand_os)) !=0)
				page_num++;
		}
		temp = ((nand_in->option << 12) & 0xf000)  +
			((i<<4) & 0xff0) + NAND_PROGRAM;
		if (usb_send_data_address_to_ingenic(&ingenic_dev, nand_in->start) != 1)
			goto err;
		if (usb_send_data_length_to_ingenic(&ingenic_dev, page_num) != 1)
			goto err;
		if (usb_ingenic_nand_ops(&ingenic_dev, temp) != 1)
			goto err;
		if (usb_read_data_from_ingenic(&ingenic_dev, ret, 8) != 1)
			goto err;

		printf(" Finish! (len %d start_page %d page_num %d)\n",
		       nand_in->length, nand_in->start, page_num);

		/* Read back to check! */
		usb_send_data_address_to_ingenic(&ingenic_dev, nand_in->start);
		usb_send_data_length_to_ingenic(&ingenic_dev, page_num);

		switch (nand_in->option) {
		case OOB_ECC:
			temp = ((OOB_ECC << 12) & 0xf000) +
				((i << 4) & 0xff0) + NAND_READ;
			start_addr = page_num * (hand.nand_ps + hand.nand_os);
			break;
		case OOB_NO_ECC:	/* do not support data verify */
			temp = ((OOB_NO_ECC << 12) & 0xf000) +
				((i << 4) & 0xff0) + NAND_READ;
			start_addr = page_num * (hand.nand_ps + hand.nand_os);
			break;
		case NO_OOB:
			temp = ((NO_OOB << 12) & 0xf000) +
				((i << 4) & 0xff0) + NAND_READ;
			start_addr = page_num * hand.nand_ps;
			break;
		default:
			;
		}

		printf(" Checking %d bytes...", nand_in->length);
		usb_ingenic_nand_ops(&ingenic_dev, temp);
		usb_read_data_from_ingenic(&ingenic_dev, check_buf, start_addr);
		usb_read_data_from_ingenic(&ingenic_dev, ret, 8);

		cur_page = (ret[3] << 24) | (ret[2] << 16) |  (ret[1] << 8) |
			(ret[0] << 0);

		if (nand_in->start == 0 && hand.nand_ps == 4096 &&
		    hand.fw_args.cpu_id == 0x4740) {
			printf(" No check! end at page: %d\n", cur_page);
			fflush(NULL);
			continue;
		}

		if (!nand_in->check(nand_in->buf, check_buf, nand_in->length)) {
			struct nand_in bad;
			// tbd: doesn't the other side skip bad blocks too? Can we just deduct 1 from cur_page?
			// tbd: why do we only mark a block as bad if the last page in the block was written?
			bad.start = (cur_page - 1) / hand.nand_ppb;
			if (cur_page % hand.nand_ppb == 0)
				nand_markbad(&bad);
		}

		printf(" End at page: %d\n", cur_page);
		fflush(NULL);
	}

	*start_page = cur_page;

	status = 1;
err:
	return status;
}

int nand_erase(struct nand_in *nand_in)
{
	unsigned int start_blk, blk_num, end_block;
	int i;

	start_blk = nand_in->start;
	blk_num = nand_in->length;
	if (start_blk > (unsigned int)NAND_MAX_BLK_NUM)  {
		printf(" Start block number overflow!\n");
		return -1;
	}
	if (blk_num > (unsigned int)NAND_MAX_BLK_NUM) {
		printf(" Length block number overflow!\n");
		return -1;
	}

	int cpu = get_ingenic_cpu();
	if (cpu != BOOT4740 && cpu != BOOT4750 && cpu != BOOT4760) {
		printf(" Device unboot! boot it first!\n");
		return -1;
	}

	for (i = 0; i < nand_in->max_chip; i++) {
		if ((nand_in->cs_map)[i]==0)
			continue;
		printf(" Erasing No.%d device No.%d flash (start_blk %u blk_num %u)......\n",
		       nand_in->dev, i, start_blk, blk_num);

		usb_send_data_address_to_ingenic(&ingenic_dev, start_blk);
		usb_send_data_length_to_ingenic(&ingenic_dev, blk_num);

		unsigned short temp = ((i << 4) & 0xff0) + NAND_ERASE;
		usb_ingenic_nand_ops(&ingenic_dev, temp);

		usb_read_data_from_ingenic(&ingenic_dev, ret, 8);
		printf(" Finish!");
	}
	end_block = ((ret[3] << 24) |
		     (ret[2] << 16) |
		     (ret[1] << 8)  |
		     (ret[0] << 0)) / hand.nand_ppb;
	printf(" Return: %02x %02x %02x %02x %02x %02x %02x %02x (position %d)\n",
	       ret[0], ret[1], ret[2], ret[3], ret[4], ret[5], ret[6], ret[7], end_block);
	if (!hand.nand_force_erase) {
	/* not force erase, show bad block infomation */
		printf(" There are marked bad blocks: %d\n",
		       end_block - start_blk - blk_num );
	} else {
	/* force erase, no bad block infomation can show */
		printf(" Force erase, no bad block infomation!\n" );
	}

	return 1;
}

int nand_program_file(struct nand_in *nand_in, char *fname)
{

	int flen, m, j, k;
	unsigned int start_page = 0, page_num, code_len, offset, transfer_size;
	int fd, status;
	struct stat fstat;
	struct nand_in n_in;

	status = stat(fname, &fstat);

	if (status < 0) {
		fprintf(stderr, "Error - can't get file size from '%s': %s\n",
			fname, strerror(errno));
		return -1;
	}
	flen = fstat.st_size;

	fd = open(fname, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Error - can't open file '%s': %s\n",
			fname, strerror(errno));
		return -1;
	}

	printf(" Programing No.%d device, flen %d, start page %d...\n",nand_in->dev, flen, nand_in->start);
	n_in.start = nand_in->start / hand.nand_ppb;
	if (nand_in->option == NO_OOB) {
		if (flen % (hand.nand_ppb * hand.nand_ps) == 0)
			n_in.length = flen / (hand.nand_ps * hand.nand_ppb);
		else
			n_in.length = flen / (hand.nand_ps * hand.nand_ppb) + 1;
	} else {
		if (flen % (hand.nand_ppb * (hand.nand_ps + hand.nand_os)) == 0)
			n_in.length = flen /
				((hand.nand_ps + hand.nand_os) * hand.nand_ppb);
		else
			n_in.length = flen /
				((hand.nand_ps + hand.nand_os) * hand.nand_ppb)
				+ 1;
	}
	n_in.cs_map = nand_in->cs_map;
	n_in.dev = nand_in->dev;
	n_in.max_chip = nand_in->max_chip;
	if (nand_erase(&n_in) != 1)
		return -1;
	if (nand_in->option == NO_OOB)
		transfer_size = (hand.nand_ppb * hand.nand_ps);
	else
		transfer_size = (hand.nand_ppb * (hand.nand_ps + hand.nand_os));

	m = flen / transfer_size;
	j = flen % transfer_size;
	printf(" Size to send %d, transfer_size %d\n", flen, transfer_size);
	printf(" Image type : %s\n", IMAGE_TYPE[nand_in->option]);
	printf(" It will cause %d times buffer transfer.\n", j == 0 ? m : m + 1);
	fflush(NULL);

	offset = 0;
	for (k = 0; k < m; k++)	{
		if (nand_in->option == NO_OOB)
			page_num = transfer_size / hand.nand_ps;
		else
			page_num = transfer_size / (hand.nand_ps + hand.nand_os);

		code_len = transfer_size;
		status = read(fd, code_buf, code_len);
		if (status < code_len) {
			fprintf(stderr, "Error - can't read file '%s': %s\n",
				fname, strerror(errno));
			return -1;
		}

		nand_in->length = code_len; /* code length,not page number! */
		nand_in->buf = code_buf;
		if (nand_program_check(nand_in, &start_page) == -1)
			return -1;

		if (start_page - nand_in->start > hand.nand_ppb)
			printf(" Info - skip bad block!\n");
		nand_in->start = start_page;

		offset += code_len ;
	}

	if (j) {
		code_len = j;
		if (j % hand.nand_ps)
			j += hand.nand_ps - (j % hand.nand_ps);
		memset(code_buf, 0, j);		/* set all to null */

		status = read(fd, code_buf, code_len);

		if (status < code_len) {
			fprintf(stderr, "Error - can't read file '%s': %s\n",
				fname, strerror(errno));
			return -1;
		}

		nand_in->length = j;
		nand_in->buf = code_buf;
		if (nand_program_check(nand_in, &start_page) == -1)
			return -1;

		if (start_page - nand_in->start > hand.nand_ppb)
			printf(" Info - skip bad block!");

	}

	close(fd);
	return 1;
}

int nand_program_file_planes(struct nand_in *nand_in, char *fname)
{
	printf("  not implement yet !\n");
	return -1;
}

int init_nand_in(void)
{
	nand_in.buf = code_buf;
	nand_in.check = error_check;
	nand_in.dev = 0;
	nand_in.cs_map = cs;
	memset(nand_in.cs_map, 0, MAX_DEV_NUM);

	nand_in.max_chip = 16;
	return 0;
}

int nand_prog(void)
{
	int status = -1;
	char *image_file;
	char *help = " Usage: nprog (1) (2) (3) (4) (5)\n"
		" (1)\tstart page number\n"
		" (2)\timage file name\n"
		" (3)\tdevice index number\n"
		" (4)\tflash index number\n"
		" (5) image type must be:\n"
		" \t-n:\tno oob\n"
		" \t-o:\twith oob no ecc\n"
		" \t-e:\twith oob and ecc\n";

	if (com_argc != 6) {
		printf("%s", help);
		return 0;
	}

	init_nand_in();

	nand_in.start = atoi(com_argv[1]);
	image_file = com_argv[2];
	nand_in.dev = atoi(com_argv[3]);

	(nand_in.cs_map)[atoi(com_argv[4])] = 1;
	if (!strcmp(com_argv[5], "-e"))
		nand_in.option = OOB_ECC;
	else if (!strcmp(com_argv[5], "-o"))
		nand_in.option = OOB_NO_ECC;
	else if (!strcmp(com_argv[5], "-n"))
		nand_in.option = NO_OOB;
	else
		printf("%s", help);

	if (hand.nand_plane > 1)
		nand_program_file_planes(&nand_in, image_file);
	else
		nand_program_file(&nand_in, image_file);

	status = 1;
err:
	return status;
}

int nand_query(void)
{
	int i;
	unsigned char csn;

	if (com_argc < 3) {
		printf(" Usage: nquery (1) (2)\n"
		       " (1):device index number\n"
		       " (2):flash index number\n");
		return -1;
	}
	init_nand_in();

	nand_in.dev = atoi(com_argv[1]);
	(nand_in.cs_map)[atoi(com_argv[2])] = 1;

	for (i = 0; i < nand_in.max_chip; i++) {
		if ((nand_in.cs_map)[i] != 0)
			break;
	}
	if (i >= nand_in.max_chip)
		return -1;

	int cpu = get_ingenic_cpu();
	if (cpu != BOOT4740 && cpu != BOOT4750 && cpu != BOOT4760) {
		printf(" Device unboot! boot it first!\n");
		return -1;
	}

	csn = i;
	printf(" ID of No.%d device No.%d flash: \n", nand_in.dev, csn);

	unsigned short ops = ((csn << 4) & 0xff0) + NAND_QUERY;
	usb_ingenic_nand_ops(&ingenic_dev, ops);
	usb_read_data_from_ingenic(&ingenic_dev, ret, 8);
	printf(" Vendor ID    :0x%x \n",(unsigned char)ret[0]);
	printf(" Product ID   :0x%x \n",(unsigned char)ret[1]);
	printf(" Chip ID      :0x%x \n",(unsigned char)ret[2]);
	printf(" Page ID      :0x%x \n",(unsigned char)ret[3]);
	printf(" Plane ID     :0x%x \n",(unsigned char)ret[4]);

	usb_read_data_from_ingenic(&ingenic_dev, ret, 8);

	return 1;
}

int nand_read(int mode)
{
	unsigned int i, j, cpu;
	unsigned int start_addr, length, page_num;
	unsigned char csn;
	unsigned short temp = 0;

	if (com_argc < 5) {
		printf(" Usage: nread (1) (2) (3) (4)\n"
		       " 1:start page number\n"
		       " 2:length in byte\n"
		       " 3:device index number\n"
		       " 4:flash index number\n");
		return -1;
	}

	init_nand_in();

	nand_in.start  = atoi(com_argv[1]);
	nand_in.length = atoi(com_argv[2]);
	nand_in.dev    = atoi(com_argv[3]);

	csn        = atoi(com_argv[4]);
	start_addr = nand_in.start;
	length     = nand_in.length;

	if (csn >= MAX_DEV_NUM) {
		printf(" Flash index number overflow!\n");
		return -1;
	}
	nand_in.cs_map[csn] = 1;

	if (start_addr > NAND_MAX_PAGE_NUM || length > NAND_MAX_PAGE_NUM ) {
		printf(" Page number overflow!\n");
		return -1;
	}

	cpu = get_ingenic_cpu();
	if (cpu != BOOT4740 && cpu != BOOT4750 && cpu != BOOT4760) {
		printf(" Device unboot! boot it first!\n");
		return -1;
	}

	printf(" Reading from No.%d device No.%d flash....\n", nand_in.dev, csn);

	page_num = length / hand.nand_ps +1;

	switch(mode) {
	case NAND_READ:
		temp = ((NO_OOB<<12) & 0xf000) + ((csn<<4) & 0xff0) + NAND_READ;
		break;
	case NAND_READ_OOB:
		temp = ((csn<<4) & 0xff0) + NAND_READ_OOB;
		break;
	case NAND_READ_RAW:
		temp = ((NO_OOB<<12) & 0xf000) + ((csn<<4) & 0xff0) +
			NAND_READ_RAW;
		break;
	default:
		printf(" Unknow mode!\n");
		return -1;
	}

	usb_send_data_address_to_ingenic(&ingenic_dev, start_addr);
	usb_send_data_length_to_ingenic(&ingenic_dev, page_num);

	usb_ingenic_nand_ops(&ingenic_dev, temp);

	usb_read_data_from_ingenic(&ingenic_dev, nand_in.buf, page_num * hand.nand_ps);

	for (j = 0; j < length; j++) {
		if (j % 16 == 0)
			printf("\n 0x%08x : ",j);
		printf("%02x ",(nand_in.buf)[j]);
	}
	printf("\n");

	usb_read_data_from_ingenic(&ingenic_dev, ret, 8);
	printf(" Operation end position : %d \n",
	       (ret[3]<<24)|(ret[2]<<16)|(ret[1]<<8)|(ret[0]<<0));

	return 1;
}

int debug_memory(int obj, unsigned int start, unsigned int size)
{
	unsigned int buffer[8],tmp;

	tmp = get_ingenic_cpu();
	if (tmp == BOOT4740 || tmp == BOOT4750 || tmp == BOOT4760) {
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

	if (load_file(&ingenic_dev, stage1) < 1)
		return -1;
	if (usb_ingenic_upload(&ingenic_dev, 1) < 1)
		return -1;

	usleep(100);
	usb_read_data_from_ingenic(&ingenic_dev, buffer, 8);
	if (buffer[0] != 0)
		printf(" Test memory fail! Last error address is 0x%x !\n",
		       buffer[0]);
	else
		printf(" Test memory pass!\n");

	return 1;
}

int debug_gpio(int obj, unsigned char ops, unsigned char pin)
{
	unsigned int tmp;

	tmp = get_ingenic_cpu();
	if (tmp == BOOT4740 || tmp == BOOT4750 || tmp == BOOT4760) {
		printf(" This command only run under UNBOOT state!\n");
		return -1;
	}

	switch (tmp) {
	case 1:
		tmp = 0;
		hand.fw_args.cpu_id = 0x4740;
		if (pin > 124) {
			printf(" Jz4740 has 124 GPIO pin in all!\n");
			return -1;
		}
		break;
	case 2:
		tmp = 0;
		hand.fw_args.cpu_id = 0x4750;
		if (pin > 178) {
			printf(" Jz4750 has 178 GPIO pin in all!\n");
			return -1;
		}
		break;
	}

	hand.fw_args.debug_ops = ops;/* tell device it's memory debug */
	hand.fw_args.pin_num = pin;

	if (ops == 2)
		printf(" GPIO %d set!\n",pin);
	else
		printf(" GPIO %d clear!\n",pin);

	if (load_file(&ingenic_dev, stage1) < 1)
		return -1;
	if (usb_ingenic_upload(&ingenic_dev, 1) < 1)
		return -1;

	return 0;
}

int debug_go(void)
{
	unsigned int addr,obj;
	if (com_argc<3) {
		printf(" Usage: go (1) (2) \n"
		       " 1:start SDRAM address\n"
		       " 2:device index number\n");
		return 0;
	}

	addr = strtoul(com_argv[1], NULL, 0);
	obj = atoi(com_argv[2]);

	printf(" Executing No.%d device at address 0x%x\n", obj, addr);

	if (usb_ingenic_start(&ingenic_dev, VR_PROGRAM_START2, addr) < 1)
		return -1;

	return 1;
}

int sdram_load(struct sdram_in *sdram_in)
{
	int cpu = get_ingenic_cpu();
	if (cpu != BOOT4740 && cpu != BOOT4750 && cpu != BOOT4760) {
		printf(" Device unboot! boot it first!\n");
		return -1;
	}

	if (sdram_in->length > (unsigned int) MAX_LOAD_SIZE) {
		printf(" Image length too long!\n");
		return -1;
	}

	ingenic_dev.file_buff = sdram_in->buf;
	ingenic_dev.file_len = sdram_in->length;
	usb_send_data_to_ingenic(&ingenic_dev);
	usb_send_data_address_to_ingenic(&ingenic_dev, sdram_in->start);
	usb_send_data_length_to_ingenic(&ingenic_dev, sdram_in->length);
	usb_ingenic_sdram_ops(&ingenic_dev, sdram_in);

	usb_read_data_from_ingenic(&ingenic_dev, ret, 8);
	printf(" Load last address at 0x%x\n",
	       ((ret[3]<<24)|(ret[2]<<16)|(ret[1]<<8)|(ret[0]<<0)));

	return 1;
}

int sdram_load_file(struct sdram_in *sdram_in, char *file_path)
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
		if (sdram_load(sdram_in) < 1)
			goto close;

		sdram_in->start += MAX_LOAD_SIZE;
		if ( k % 60 == 0)
			printf(" 0x%x \n", sdram_in->start);
	}

	if (j) {
		if (j % 4 !=0)
			j += 4 - (j % 4);
		status = read(fd, sdram_in->buf, j);
		if (status < j) {
			fprintf(stderr, "Error - can't read file '%s': %s\n",
				file_path, strerror(errno));
			goto close;
		}

		sdram_in->length = j;
		if (sdram_load(sdram_in) < 1)
			goto close;
	}

	res = 1;

close:
	close(fd);
out:
	return res;
}

int device_reset(int ops)
{
	int cpu = get_ingenic_cpu();
	if (cpu != BOOT4740 && cpu != BOOT4750 && cpu != BOOT4760) {
		printf(" Device unboot! boot it first!\n");
		return -1;
	}

	if (usb_ingenic_reset(&ingenic_dev, ops) < 1)
		return -1;

	return 1;
}
