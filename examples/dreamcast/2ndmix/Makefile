# KallistiOS ##version##
#
# 2ndmix/Makefile
# Copyright (C)2003 Dan Potter
#   
# $Id: Makefile,v 1.4 2002/04/20 04:24:19 axlen Exp $

all: rm-elf 2ndmix.elf

KOS_ROMDISK_DIR = romdisk_boot
include ../../../Makefile.rules

OBJS = 2ndmix.o romdisk.o

clean:
	rm -f 2ndmix.elf $(OBJS)

rm-elf:
	rm -f 2ndmix.elf
	rm -f romdisk.img

2ndmix.elf: $(OBJS)
	kos-cc -o 2ndmix.elf $(OBJS)

run: 2ndmix.elf
	$(KOS_LOADER) 2ndmix.elf

dist:
	rm -f $(OBJS) romdisk.*
	$(KOS_STRIP) 2ndmix.elf

