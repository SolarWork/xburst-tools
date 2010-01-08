
#ifndef __NAND_H
#define __NAND_H

#include <stdint.h>

struct ingenic_dev;

typedef int (*nand_read_cb_t)(void *userdata, const char *buf, size_t size);

int nand_query(struct ingenic_dev *dev, uint8_t nand_idx);
int nand_read(struct ingenic_dev *dev, uint8_t nand_idx, int mode,
				uint32_t start_page, uint32_t length, uint32_t ram_addr,
				nand_read_cb_t callbcallback, void *userdata);
int nand_erase(struct ingenic_dev *dev, uint8_t nand_idx,
				uint32_t start_block, uint32_t num_blocks);
int nand_prog(struct ingenic_dev *dev, uint8_t nand_idx,
				uint32_t start_page, const char *filename, int mode);
int nand_markbad(struct ingenic_dev *dev, uint8_t nand_idx, uint32_t block);

#endif
