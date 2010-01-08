/*
 * Copyright(C) 2009  Qi Hardware Inc.,
 * Authors: Xiangfu Liu <xiangfu@qi-hardware.com>
 *          Marek Lindner <lindner_marek@yahoo.de>
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
#include <usb.h>
#include "usb_boot_defines.h"
#include "ingenic_usb.h"

extern unsigned int total_size;

static int get_ingenic_device(struct ingenic_dev *ingenic_dev)
{
	struct usb_bus *usb_busses, *usb_bus;
	struct usb_device *usb_dev;
	int count = 0;

	usb_busses = usb_get_busses();

	for (usb_bus = usb_busses; usb_bus != NULL; usb_bus = usb_bus->next) {
		for (usb_dev = usb_bus->devices; usb_dev != NULL;
		     usb_dev = usb_dev->next) {

			if ((usb_dev->descriptor.idVendor == VENDOR_ID) &&
				(usb_dev->descriptor.idProduct == PRODUCT_ID)) {
				ingenic_dev->usb_dev = usb_dev;
				count++;
				break;
			}

		}
	}

	return count;
}

static int get_ingenic_interface(struct ingenic_dev *ingenic_dev)
{
	struct usb_config_descriptor *usb_config_desc;
	struct usb_interface_descriptor *usb_if_desc;
	struct usb_interface *usb_if;
	int config_index, if_index, alt_index;

	for (config_index = 0;
	     config_index < ingenic_dev->usb_dev->descriptor.bNumConfigurations;
	     config_index++) {
		usb_config_desc = &ingenic_dev->usb_dev->config[config_index];

		if (!usb_config_desc)
			return -1;

		for (if_index = 0; if_index < usb_config_desc->bNumInterfaces;
		     if_index++) {
			usb_if = &usb_config_desc->interface[if_index];

			if (!usb_if)
				return -1;

			for (alt_index = 0; alt_index < usb_if->num_altsetting;
			     alt_index++) {
				usb_if_desc = &usb_if->altsetting[alt_index];

				if (!usb_if_desc)
					return -1;

				if ((usb_if_desc->bInterfaceClass == 0xff) &&
					(usb_if_desc->bInterfaceSubClass == 0)) {
					ingenic_dev->interface =
						usb_if_desc->bInterfaceNumber;
					return 0;
				}
			}
		}
	}

	return 0;
}

int usb_ingenic_init(struct ingenic_dev *ingenic_dev)
{
	int num_ingenic;
	int ret = -1;

	memset(ingenic_dev, 0, sizeof(struct ingenic_dev));

	usb_init();
 	/* usb_set_debug(255); */
	usb_find_busses();
	usb_find_devices();

	num_ingenic = get_ingenic_device(ingenic_dev);

	if (num_ingenic == 0) {
		fprintf(stderr, "Error - no XBurst device found\n");
		goto err;
	}

	if (num_ingenic > 1) {
		fprintf(stderr, "Error - too many XBurst devices found: %d\n",
			num_ingenic);
		goto err;
	}

	ingenic_dev->usb_handle = usb_open(ingenic_dev->usb_dev);
	if (!ingenic_dev->usb_handle) {
		fprintf(stderr, "Error - can't open XBurst device: %s\n",
			usb_strerror());
		goto err;
	}

	ret = get_ingenic_interface(ingenic_dev);
	if (ret < 0) {
		fprintf(stderr, "Error - can't find XBurst interface\n");
		goto err;
	}

	ret = usb_claim_interface(ingenic_dev->usb_handle, ingenic_dev->interface);
	if (ret < 0) {
		fprintf(stderr, "Error - can't claim XBurst interface: %s\n",
			usb_strerror());
		goto err;
	}

	return 0;
err:
	return ret;
}

int usb_get_ingenic_cpu(struct ingenic_dev *ingenic_dev)
{
	int ret;
	char buf[9];

	sleep(1);
	ret = usb_control_msg(ingenic_dev->usb_handle,
	  /* bmRequestType */ USB_ENDPOINT_IN | USB_TYPE_VENDOR |USB_RECIP_DEVICE,
	  /* bRequest      */ VR_GET_CPU_INFO,
	  /* wValue        */ 0,
	  /* wIndex        */ 0,
	  /* Data          */ buf,
	  /* wLength       */ 8,
			      USB_TIMEOUT);

	if (ret != 8) {
		fprintf(stderr, "Error - "
			"can't retrieve XBurst CPU information: %d\n", ret);
		return ret;
	}

	buf[8] = '\0';
	printf(" CPU data: %s\n", buf);

	if (!strcmp(buf, "JZ4740V1"))
		return INGENIC_CPU_JZ4740V1;
	if (!strcmp(buf, "JZ4750V1"))
		return INGENIC_CPU_JZ4750V1;
	if (!strcmp(buf, "Boot4740"))
		return INGENIC_CPU_JZ4740;
	if (!strcmp(buf, "Boot4750"))
		return INGENIC_CPU_JZ4750;

	return INGENIC_CPU_UNKOWN;
}

int usb_send_data_to_ingenic(struct ingenic_dev *ingenic_dev, const char *data,
                             int size)
{
	int ret;
	ret = usb_bulk_write(ingenic_dev->usb_handle,
	/* endpoint         */	INGENIC_OUT_ENDPOINT,
	/* bulk data        */	data,
	/* bulk data length */	size,
				USB_TIMEOUT);

	if (ret < 0) {
		fprintf(stderr, "Error - "
			"Can't send bulk data to Ingenic CPU: %d\n", ret);
		return ret;
	}

	return size;
}

int usb_read_data_from_ingenic(struct ingenic_dev *ingenic_dev,
			       char *data, int size)
{
	int ret;
	ret = usb_bulk_read(ingenic_dev->usb_handle,
	/* endpoint         */	INGENIC_IN_ENDPOINT,
	/* bulk data        */	data,
	/* bulk data length */	size,
				USB_TIMEOUT);

	if (ret < 0) {
		fprintf(stderr, "Error - "
			"Can't read bulk data from Ingenic device: %d\n", ret);
		return ret;
	}

	return size;
}

int usb_ingenic_start(struct ingenic_dev *ingenic_dev, int rqst, int stage_addr)
{
	int ret;

	/* tell the device to start the uploaded device */
	ret = usb_control_msg(ingenic_dev->usb_handle,
	  /* bmRequestType */ USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	  /* bRequest      */ rqst,
	  /* wValue        */ STAGE_ADDR_MSB(stage_addr),
	  /* wIndex        */ STAGE_ADDR_LSB(stage_addr),
	  /* Data          */ 0,
	  /* wLength       */ 0,
	                      USB_TIMEOUT);

	if (ret != 0) {
		fprintf(stderr, "Error - can't start the uploaded binary "
			"on the Ingenic device: %d\n", ret);
	}
	return ret;
}

int usb_send_data_length_to_ingenic(struct ingenic_dev *ingenic_dev, unsigned int len)
{
	int ret;

	/* tell the device the length of the file to be uploaded */
	ret = usb_control_msg(ingenic_dev->usb_handle,
	  /* bmRequestType */ USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	  /* bRequest      */ VR_SET_DATA_LENGTH,
	  /* wValue        */ STAGE_ADDR_MSB(len),
	  /* wIndex        */ STAGE_ADDR_LSB(len),
	  /* Data          */ 0,
	  /* wLength       */ 0,
			      USB_TIMEOUT);

	if (ret != 0) {
		fprintf(stderr, "Error - "
			"can't set data length on Ingenic device: %d\n", ret);
	}

	return ret;
}

int usb_send_data_address_to_ingenic(struct ingenic_dev *ingenic_dev, uint32_t addr)
{
	int ret;

	/* tell the device the RAM address to store the file */
	ret = usb_control_msg(ingenic_dev->usb_handle,
	  /* bmRequestType */ USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	  /* bRequest      */ VR_SET_DATA_ADDRESS,
	  /* wValue        */ STAGE_ADDR_MSB(addr),
	  /* wIndex        */ STAGE_ADDR_LSB(addr),
	  /* Data          */ 0,
	  /* wLength       */ 0,
			      USB_TIMEOUT);

	if (ret != 0) {
		fprintf(stderr, "Error - "
			"Can't set the address on Ingenic device: %d\n", ret);
	}

	return ret;
}


int usb_ingenic_upload(struct ingenic_dev *ingenic_dev, int stage,
                       const char *buf, int size)
{
	uint32_t stage_addr;
	int request;
	int ret;

	if (stage == 1) {
		stage_addr = 0x80002000;
		request = VR_PROGRAM_START1;
	} else {
		stage_addr = 0x80000000 + total_size - CODE_SIZE;
		request = VR_PROGRAM_START2;
	}


	usb_send_data_address_to_ingenic(ingenic_dev, stage_addr);
	printf("Download stage %d program and execute at 0x%08x\n",
	       stage, stage_addr);
	usb_send_data_to_ingenic(ingenic_dev, buf, size);

	if (stage == 2) {
		ret = usb_get_ingenic_cpu(ingenic_dev);
		if (ret < 0)
			return ret;
		usb_ingenic_flush_cache(ingenic_dev);
	}

	ret = usb_ingenic_start(ingenic_dev, request, stage_addr);
	if (ret)
		return ret;
	ret = usb_get_ingenic_cpu(ingenic_dev);
	if (ret)
		return ret;

	return 0;
}

void usb_ingenic_cleanup(struct ingenic_dev *ingenic_dev)
{
	if (ingenic_dev->usb_handle) {
		if (ingenic_dev->interface) {
			usb_release_interface(ingenic_dev->usb_handle,
			                      ingenic_dev->interface);
		}

		usb_close(ingenic_dev->usb_handle);
	}

}

static int usb_ingenic_ops(struct ingenic_dev *ingenic_dev, uint32_t type,
                           uint32_t ops)
{
	int ret;
	ret = usb_control_msg(ingenic_dev->usb_handle,
	  /* bmRequestType */ USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	  /* bRequest      */ type,
	  /* wValue        */ ops,
	  /* wIndex        */ 0,
	  /* Data          */ 0,
	  /* wLength       */ 0,
			      USB_TIMEOUT);

	return ret;
}

int usb_ingenic_flush_cache(struct ingenic_dev *ingenic_dev)
{
	int ret;
	ret = usb_ingenic_ops(ingenic_dev, VR_FLUSH_CACHES, 0);

	if (ret != 0) {
		fprintf(stderr, "Error - can't flush cache: %d\n", ret);
	}

	return ret;
}


int usb_ingenic_nand_ops(struct ingenic_dev *ingenic_dev, int ops)
{
	int ret;
	ret = usb_ingenic_ops(ingenic_dev, VR_NAND_OPS, ops);

	if (ret != 0) {
		fprintf(stderr, "Error - "
			"can't set Ingenic device nand ops: %d\n", ret);
	}

	return ret;
}

int usb_ingenic_mem_ops(struct ingenic_dev *ingenic_dev, int ops)
{
	int ret;
	ret = usb_ingenic_ops(ingenic_dev, VR_MEM_OPS, ops);

	if (ret != 0) {
		fprintf(stderr, "Error - "
			"can't set Ingenic device nand ops: %d\n", ret);
	}

	return ret;
}


int usb_ingenic_configration(struct ingenic_dev *ingenic_dev, int ops)
{
	int ret;
	ret = usb_ingenic_ops(ingenic_dev, VR_CONFIGURATION, ops);

	if (ret != 0) {
		fprintf(stderr, "Error - "
			"can't init Ingenic configration: %d\n", ret);
	}

	return ret;
}

int usb_ingenic_sdram_ops(struct ingenic_dev *ingenic_dev, int ops)
{
	int ret;

	ret = usb_ingenic_ops(ingenic_dev, VR_SDRAM_OPS, ops);

	if (ret != 0) {
		fprintf(stderr, "Error - "
			"Device can't load file to sdram: %d\n", ret);
	}

	return ret;
}
