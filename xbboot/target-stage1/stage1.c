//
// Authors: Wolfgang Spraul <wolfgang@sharism.cc>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version
// 3 of the License, or (at your option) any later version.
//

#include "common.h"
#include "serial.h"
#include "jz4740.h"

extern void gpio_init_4740();
extern void pll_init_4740();
extern void serial_init_4740();
extern void sdram_init_4740();
extern void nand_init_4740();

void load_args()
{
	ARG_CPU_ID = 0x4740;
	ARG_EXTAL = 12 * 1000000;
	ARG_CPU_SPEED = 21 * ARG_EXTAL;
	ARG_PHM_DIV = 3;
	ARG_UART_BASE = UART0_BASE + 0 * UART_OFF;
	UART_BASE = ARG_UART_BASE; // for ../target-common/serial.c
	ARG_UART_BAUD = 57600;
	ARG_BUS_WIDTH_16 = * (int *)0x80002014;
	ARG_BANK_ADDR_2BIT = 1;
	ARG_ROW_ADDR = 13;
	ARG_COL_ADDR = 9;
}

void c_main(void)
{
	load_args();

	switch (ARG_CPU_ID)	{
	case 0x4740:
		gpio_init_4740();
		serial_init_4740();
		pll_init_4740();
		sdram_init_4740();
		nand_init_4740();
		break;
	case 0x4760:	
		break;
	default:
		return;
	}

	serial_puts("stage 1 finished: GPIO, clocks, SDRAM, UART setup\n"
		    "now jump back to BOOT ROM...\n");

}

