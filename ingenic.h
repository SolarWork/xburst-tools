/* This file is based on code by Ingenic Semiconductor Co., Ltd. */

#ifndef __INGENIC__H__
#define __INGENIC__H__

#include <stdint.h>

#define VR_GET_CPU_INFO		0x00
#define VR_SET_DATA_ADDRESS	0x01
#define VR_SET_DATA_LENGTH	0x02
#define VR_FLUSH_CACHES		0x03
#define VR_PROGRAM_START1	0x04
#define VR_PROGRAM_START2	0x05
#define VR_NOR_OPS		0x06
#define VR_NAND_OPS		0x07
#define VR_SDRAM_OPS		0x08
#define VR_CONFIGRATION		0x09
#define VR_GET_NUM		0x0a

typedef struct {
	void (*cmdset_change)(void *arg);
} ingenic_callbacks_t;

void *ingenic_open(void *usb_hndl);
void ingenic_close(void *hndl);
void ingenic_set_callbacks(void *hndl, const ingenic_callbacks_t *callbacks, void *arg);

int ingenic_redetect(void *hndl);
int ingenic_cmdset(void *hndl);
int ingenic_type(void *hndl);
uint32_t ingenic_sdram_size(void *hndl);

int ingenic_rebuild(void *hndl);
int ingenic_loadstage(void *hndl, int id, const char *filename);
int ingenic_stage1_debugop(void *device, const char *filename, uint32_t op, uint32_t pin, uint32_t base, uint32_t size);
int ingenic_memtest(void *hndl, const char *filename, uint32_t base, uint32_t size, uint32_t *fail);

int ingenic_configure_stage2(void *hndl);

#define CMDSET_SPL	1
#define CMDSET_USBBOOT	2

#define INGENIC_STAGE1	1
#define INGENIC_STAGE2	2

#define STAGE1_DEBUG_BOOT		0
#define STAGE1_DEBUG_MEMTEST		1
#define STAGE1_DEBUG_GPIO_SET		2
#define STAGE1_DEBUG_GPIO_CLEAR		3

#define STAGE1_BASE	0x2000
#define STAGE2_CODESIZE	0x400000
#define SDRAM_BASE	0x80000000

#define DS_flash_info 0
#define DS_hand	1
typedef struct {
	/* debug args */
	uint8_t debug_ops;
	uint8_t pin_num;
	uint32_t start;
	uint32_t size;
} __attribute__((packed)) ingenic_stage1_debug_t;

typedef struct {
	/* CPU ID */
	uint32_t cpu_id;

	/* PLL args */
	uint8_t ext_clk;
	uint8_t cpu_speed;
	uint8_t phm_div;
	uint8_t use_uart;
	uint32_t baudrate;

	/* SDRAM args */
	uint8_t bus_width;
	uint8_t bank_num;
	uint8_t row_addr;
	uint8_t col_addr;
	uint8_t is_mobile;
	uint8_t is_busshare;

	ingenic_stage1_debug_t debug;
} __attribute__((packed)) firmware_config_t;

typedef struct {
	/* nand flash info */
	uint32_t cpuid; /* cpu type */
	uint32_t nand_bw;		/* bus width */
	uint32_t nand_rc;		/* row cycle */
	uint32_t nand_ps;		/* page size */
	uint32_t nand_ppb;		/* page number per block */
	uint32_t nand_force_erase;
	uint32_t nand_pn;		/* page number in total */
	uint32_t nand_os;		/* oob size */
	uint32_t nand_eccpos;
	uint32_t nand_bbpage;
	uint32_t nand_bbpos;
	uint32_t nand_plane;
	uint32_t nand_bchbit;
	uint32_t nand_wppin;
	uint32_t nand_bpc;		/* block number per chip */
} nand_config_t;

#endif
