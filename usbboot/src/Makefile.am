AM_CFLAGS = -pedantic -Wall -W -DCFGDIR=\"$(cfgdir)\" -DDATADIR=\"$(pkgdatadir)\"

bin_PROGRAMS = usbboot
usbboot_SOURCES = cmd.c  command_line.c  ingenic_cfg.c  \
	ingenic_usb.c  main.c

pkgdatadir = $(datadir)/xburst-tools/

if COND_FIRMWARE
pkgdata_DATA = ../xburst_stage1/xburst_stage1.bin \
	../xburst_stage2/xburst_stage2.bin
endif

cfgdir = $(sysconfdir)/xburst-tools/
cfg_DATA = ../doc/usbboot.cfg

EXTRA_DIST = $(pkgdatadir) $(cfgdir)

../xburst_stage1/xburst_stage1.bin:
	$(MAKE) CROSS_COMPILE=$(CROSS_COMPILE) -C ../xburst_stage1

../xburst_stage2/xburst_stage2.bin:
	$(MAKE) CROSS_COMPILE=$(CROSS_COMPILE) -C ../xburst_stage2

clean-local:
	$(MAKE) CROSS_COMPILE=$(CROSS_COMPILE) -C ../xburst_stage1 clean
	$(MAKE) CROSS_COMPILE=$(CROSS_COMPILE) -C ../xburst_stage2 clean
