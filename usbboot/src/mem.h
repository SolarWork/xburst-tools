#ifndef __MEM_H
#define __MEM_H

struct ingenic_dev;

void mem_write32(struct ingenic_dev *dev, uint32_t addr, uint32_t val);
void mem_write16(struct ingenic_dev *dev, uint32_t addr, uint16_t val);
void mem_write8(struct ingenic_dev *dev, uint32_t addr, uint8_t val);
uint32_t mem_read32(struct ingenic_dev *dev, uint32_t addr);
uint16_t mem_read16(struct ingenic_dev *dev, uint32_t addr);
uint8_t mem_read8(struct ingenic_dev *dev, uint32_t addr);

#endif
