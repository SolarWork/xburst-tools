#include <stdint.h>
#include <stdio.h>
#include "ingenic_usb.h"

#define MEM_WRITE 0
#define MEM_READ 1

#define MEM_8BIT (0 << 1)
#define MEM_16BIT (1 << 1)
#define MEM_32BIT (2 << 1)

static void mem_write(struct ingenic_dev *dev, uint32_t addr, uint32_t val, unsigned int width)
{
    char ret[8];
	usb_send_data_address_to_ingenic(dev, addr);
	usb_send_data_length_to_ingenic(dev, val);
	usb_ingenic_mem_ops(dev, MEM_WRITE | width);
	usb_read_data_from_ingenic(dev, ret, ARRAY_SIZE(ret));
}

static uint32_t mem_read(struct ingenic_dev *dev, uint32_t addr, unsigned int width)
{
    char ret[8];
	usb_send_data_address_to_ingenic(dev, addr);
	usb_ingenic_mem_ops(dev, MEM_READ | width);
	usb_read_data_from_ingenic(dev, ret, ARRAY_SIZE(ret));

    return (((unsigned char)ret[3]) << 24) | (((unsigned char)ret[2]) << 16) |
            (((unsigned char)ret[1]) << 8) | ((unsigned char)ret[0]);
}

void mem_write8(struct ingenic_dev *dev, uint32_t addr, uint8_t val)
{
    mem_write(dev, addr, val, MEM_8BIT);
}

void mem_write16(struct ingenic_dev *dev, uint32_t addr, uint16_t val)
{
    mem_write(dev, addr, val, MEM_16BIT);
}

void mem_write32(struct ingenic_dev *dev, uint32_t addr, uint32_t val)
{
    mem_write(dev, addr, val, MEM_32BIT);
}

uint8_t mem_read8(struct ingenic_dev *dev, uint32_t addr)
{
    uint8_t val = mem_read(dev, addr, MEM_8BIT) & 0xff;
    return val;
}

uint16_t mem_read16(struct ingenic_dev *dev, uint32_t addr)
{
    return mem_read(dev, addr, MEM_16BIT) & 0xffff;
}

uint32_t mem_read32(struct ingenic_dev *dev, uint32_t addr)
{
    return mem_read(dev, addr, MEM_32BIT);
}
