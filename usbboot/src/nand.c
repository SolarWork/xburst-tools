#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>

#include "nand.h"
#include "ingenic_usb.h"
#include "usb_boot_defines.h"

#define NAND_OP(idx, op, mode) (((mode << 12) & 0xf000) | ((idx << 4) & 0xff0) | op)

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))

static const char IMAGE_TYPE[][30] = {
	"with oob and ecc",
	"with oob and without ecc",
	"without oob",
};

static int error_check(const char *a, const char *b, unsigned int size)
{
	unsigned int i;
	const unsigned char *org = (const unsigned char *)a;
	const unsigned char *obj= (const unsigned char *)b;


	printf("Comparing %d bytes - ", size);
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
			return 0;
		}
	}
	printf("SUCCESS\n");
	return 1;
}

static int nand_read_pages(struct ingenic_dev *dev, unsigned int start_page, int num_pages, char *buf,
                          int length, uint16_t op, char *ret)
{
        usb_send_data_address_to_ingenic(dev, start_page);
        usb_send_data_length_to_ingenic(dev, num_pages);

        usb_ingenic_nand_ops(dev, op);

        usb_read_data_from_ingenic(dev, buf, length);

        usb_read_data_from_ingenic(dev, ret, 8);

        return 0;
}
int nand_markbad(struct ingenic_dev *dev, uint8_t nand_idx, uint32_t block)
{
    char ret[8];
    (void)nand_idx;

	if (usb_get_ingenic_cpu(dev) < 3) {
		fprintf(stderr, "Device unboot! Boot it first!\n");
		return -ENODEV;
	}
	printf("Mark bad block : %d\n", block);
	usb_send_data_address_to_ingenic(dev, block);
	usb_ingenic_nand_ops(dev, NAND_MARK_BAD);
	usb_read_data_from_ingenic(dev, ret, ARRAY_SIZE(ret));
	printf("Mark bad block at %d\n",((ret[3] << 24) |
					   (ret[2] << 16) |
					   (ret[1] << 8)  |
					   (ret[0] << 0)) / dev->config.nand_ppb);

	return 0;
}

int nand_program_check(struct ingenic_dev *dev, uint8_t nand_idx,
						unsigned int start_page, const char *data,
                        uint32_t length, unsigned int mode)
{
	unsigned int page_num, cur_page = -1;
	unsigned short op;
    static char read_back_buf[MAX_TRANSFER_SIZE];
    char ret[8];

	printf("Writing NAND page %u len %u...\n", start_page, length);
	if (length > (unsigned int)MAX_TRANSFER_SIZE) {
		fprintf(stderr, "Buffer size too long!\n");
		return -EINVAL;
	}

	if (usb_get_ingenic_cpu(dev) < 3) {
		fprintf(stderr, "Device unboot! Boot it first!\n");
		return -ENODEV;
	}
	usb_send_data_to_ingenic(dev, data, length);

    if (mode == NO_OOB)
        page_num = DIV_ROUND_UP(length, dev->config.nand_ps);
	else
		page_num = DIV_ROUND_UP(length, dev->config.nand_ps + dev->config.nand_os);

	op = NAND_OP(nand_idx, NAND_PROGRAM, mode);

	usb_send_data_address_to_ingenic(dev, start_page);
	usb_send_data_length_to_ingenic(dev, page_num);
	usb_ingenic_nand_ops(dev, op);

	usb_read_data_from_ingenic(dev, ret, 8);
	printf("Finish! (len %d start_page %d page_num %d)\n",
		       length, start_page, page_num);

    switch(mode) {
	case NO_OOB:
        op = NAND_OP(nand_idx, NAND_READ, NO_OOB);
		break;
	default:
        op = NAND_OP(nand_idx, NAND_READ_RAW, OOB_ECC);
		break;
    }

    nand_read_pages(dev, start_page, page_num, read_back_buf, length, op, ret);
    printf("Checking %d bytes...", length);

    cur_page = (ret[3] << 24) | (ret[2] << 16) |  (ret[1] << 8) |
        (ret[0] << 0);

    if (start_page < 1 &&
        dev->config.nand_ps == 4096 &&
        dev->config.fw_args.cpu_id == 0x4740) {
        printf("no check! End at Page: %d\n", cur_page);
    }

    if (!error_check(data, read_back_buf, length)) {
        // tbd: doesn't the other side skip bad blocks too? Can we just deduct 1 from cur_page?
        // tbd: why do we only mark a block as bad if the last page in the block was written?
        if (cur_page % dev->config.nand_ppb == 0)
            nand_markbad(dev, nand_idx, (cur_page - 1) / dev->config.nand_ppb);
    }

    printf("End at Page: %d\n", cur_page);

/*	*start_page = cur_page;*/
	return 0;
}

int nand_erase(struct ingenic_dev *dev, uint8_t nand_idx, uint32_t start_block,
				uint32_t num_blocks)
{
    uint32_t end_block;
    uint16_t op;
    static char ret[8];

	if (start_block > (unsigned int)NAND_MAX_BLK_NUM)  {
		fprintf(stderr, "Start block number overflow!\n");
		return -EINVAL;
	}
	if (num_blocks > (unsigned int)NAND_MAX_BLK_NUM) {
		fprintf(stderr, "Length block number overflow!\n");
		return -EINVAL;
	}

	if (usb_get_ingenic_cpu(dev) < 3) {
		fprintf(stderr, "Device unboot! Boot it first!\n");
		return -ENODEV;
	}

    printf("Erasing No.%d device No.%d flash (start_blk %u blk_num %u)......\n",
           0, nand_idx, start_block, num_blocks);

    usb_send_data_address_to_ingenic(dev, start_block);
    usb_send_data_length_to_ingenic(dev, num_blocks);

    op = NAND_OP(nand_idx, NAND_ERASE, 0);
    usb_ingenic_nand_ops(dev, op);

    usb_read_data_from_ingenic(dev, ret, 8);
    printf("Finish!");

	end_block = ((ret[3] << 24) | (ret[2] << 16) |
		     (ret[1] << 8) | (ret[0] << 0)) / dev->config.nand_ppb;
	printf("Return: %02x %02x %02x %02x %02x %02x %02x %02x (position %d)\n",
	       ret[0], ret[1], ret[2], ret[3], ret[4], ret[5], ret[6], ret[7], end_block);
	if (!dev->config.nand_force_erase) {
	/* not force erase, show bad block infomation */
		printf("There are marked bad blocks: %d\n",
		       end_block - start_block - num_blocks );
	} else {
	/* force erase, no bad block infomation can show */
		printf("Force erase, no bad block infomation!\n" );
	}

	return 0;
}

int nand_program_file(struct ingenic_dev *dev, uint8_t nand_idx,
					uint32_t start_page, const char *filename, int mode)
{
	uint32_t start_block, num_blocks;
	int flen, m, j, k;
	unsigned int page_num, code_len, offset, transfer_size;
	int fd, status;
	struct stat fstat;
    static char code_buf[MAX_TRANSFER_SIZE];

	status = stat(filename, &fstat);

	if (status < 0) {
		fprintf(stderr, "Error - can't get file size from '%s': %s\n",
			filename, strerror(errno));
		return -EEXIST;
	}
	flen = fstat.st_size;

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Error - can't open file '%s': %s\n",
			filename, strerror(errno));
		return errno;
	}

	printf("Programing No.%d device, flen %d, start page %d...\n", 0,
			flen, start_page);

	/* printf("length %d flen %d\n", n_in.length, flen); */
	if (mode == NO_OOB)
		transfer_size = (dev->config.nand_ppb * dev->config.nand_ps);
	else
		transfer_size = (dev->config.nand_ppb * (dev->config.nand_ps +
		dev->config.nand_os));

	start_block = start_page / dev->config.nand_ppb;
	num_blocks = flen / (transfer_size - 1) + 1;

	if (nand_erase(dev, nand_idx, start_block, num_blocks))
		return -1;

	m = flen / transfer_size;
	j = flen % transfer_size;

	printf("Size to send %d, transfer_size %d\n", flen, transfer_size);
/*	printf("Image type : %s\n", IMAGE_TYPE[mode]);*/
	printf("It will cause %d times buffer transfer.\n", j == 0 ? m : m + 1);

	if (mode == NO_OOB)
		page_num = transfer_size / dev->config.nand_ps;
	else
		page_num = transfer_size / (dev->config.nand_ps + dev->config.nand_os);


	offset = 0;
	for (k = 0; k < m; k++)	{
		code_len = transfer_size;
		status = read(fd, code_buf, code_len);
		if (status < (int)code_len) {
			fprintf(stderr, "Error - can't read file '%s': %s\n",
				filename, strerror(errno));
			goto close;
		}

		if (nand_program_check(dev, nand_idx, start_page, code_buf, code_len, mode) == -1)
            goto close;

/*		if (start_page - nand_in->start > dev->config.nand_ppb)
			printf("Skip a old bad block !\n");*/

		offset += code_len ;
	}

	if (j) {
		code_len = j;
		if (j % dev->config.nand_ps)
			j += dev->config.nand_ps - (j % dev->config.nand_ps);
		memset(code_buf, 0, j);		/* set all to null */

		status = read(fd, code_buf, code_len);

		if (status < (int)code_len) {
			fprintf(stderr, "Error - can't read file '%s': %s\n",
				filename, strerror(errno));
			goto close;
		}

		if (nand_program_check(dev, nand_idx, start_page, code_buf, j, mode) == -1)
			goto close;

/*
		if (start_page - nand_in->start > dev->config.nand_ppb)
			printf("Skip a old bad block !");
*/
	}
close:
	close(fd);
	return 0;
}

int nand_prog(struct ingenic_dev *dev, uint8_t nand_idx, uint32_t start_page,
			const char *filename, int mode)
{
	if (dev->config.nand_plane > 1)
		printf("ERROR");
	else
		nand_program_file(dev, nand_idx, start_page, filename, mode);

	return 0;
}

int nand_query(struct ingenic_dev *dev, uint8_t nand_idx)
{
    uint16_t op;
    char ret[8] = {0,0,0,0,0,0,0,0};
	int ret2;

	if (usb_get_ingenic_cpu(dev) < 3) {
		fprintf(stderr, "Device unboot! Boot it first!\n");
		return -ENODEV;
	}

	printf("ID of No.%u device No.%u flash: \n", 0, nand_idx);

	op = NAND_OP(nand_idx, NAND_QUERY, 0);

	usb_ingenic_nand_ops(dev, op);
	ret2 = usb_read_data_from_ingenic(dev, ret, ARRAY_SIZE(ret));

	if (ret2 < 0)
		return ret2;

	printf("Vendor ID    :0x%x \n", (unsigned char)ret[0]);
	printf("Product ID   :0x%x \n", (unsigned char)ret[1]);
	printf("Chip ID      :0x%x \n", (unsigned char)ret[2]);
	printf("Page ID      :0x%x \n", (unsigned char)ret[3]);
	printf("Plane ID     :0x%x \n", (unsigned char)ret[4]);

	usb_read_data_from_ingenic(dev, ret, ARRAY_SIZE(ret));
	printf("Operation status: Success!\n");

	return 0;
}

int nand_read(struct ingenic_dev *dev, uint8_t nand_idx, int mode,
				uint32_t start_page, uint32_t length, uint32_t ram_addr,
				nand_read_cb_t callback, void *userdata)
{
	uint16_t op;
    uint32_t page;
    uint32_t request_length;
    uint32_t pages_per_request;
    char ret[8];
    char *buf;
	int ret2;

	if (start_page > NAND_MAX_PAGE_NUM) {
		fprintf(stderr, "Page number overflow!\n");
		return -EINVAL;
	}
	if (usb_get_ingenic_cpu(dev) < 3) {
		fprintf(stderr, "Device unboot! Boot it first!\n");
		return -EINVAL;
	}
	if (nand_idx >= 16)
        return -EINVAL;

	printf("Reading from No.%u device No.%u flash....\n", 0, nand_idx);


	switch(mode) {
	case NAND_READ:
        op = NAND_OP(nand_idx, NAND_READ, NO_OOB);
		break;
	case NAND_READ_OOB:
        op = NAND_OP(nand_idx, NAND_READ_OOB, 0);
		break;
	case NAND_READ_RAW:
        op = NAND_OP(nand_idx, NAND_READ_RAW, OOB_ECC);
		break;
	case NAND_READ_TO_RAM:
        op = NAND_OP(nand_idx, NAND_READ_TO_RAM, NO_OOB);
		usb_ingenic_start(dev, VR_PROGRAM_START1, ram_addr);
		break;
	default:
		return -EINVAL;
	}

    pages_per_request = 1;
	if (mode == NAND_READ_OOB || mode == NAND_READ_RAW)
		request_length = (dev->config.nand_ps + dev->config.nand_os) * pages_per_request;
	else
		request_length = dev->config.nand_ps * pages_per_request;

    page = start_page;

    buf = malloc(request_length);

    while (length > 0) {
        if (request_length > length)
            request_length = length;

        nand_read_pages(dev, page, pages_per_request, buf, request_length, op, ret);

		ret2 = callback(userdata, buf, request_length);
		if (ret2)
			return ret2;

        length -= request_length;
        page += pages_per_request;
    }

	printf("Operation end position : %u \n",
	       (ret[3]<<24)|(ret[2]<<16)|(ret[1]<<8)|(ret[0]<<0));
    free(buf);

	return 0;
}
