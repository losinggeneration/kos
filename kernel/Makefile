# KallistiOS ##version##
#
# kernel Makefile
# (c)2001 Dan Potter
#   
# $Id: Makefile,v 1.5 2002/02/27 05:43:11 bardtx Exp $

OBJS =
SUBDIRS = arch debug fs thread net libc exports
STUBS = stubs/kernel_export_stubs.o stubs/arch_export_stubs.o

all: subdirs $(STUBS)
	rm -f $(TOPDIR)/lib/$(KOS_ARCH)/libkallisti.a
	kos-ar rcs $(TOPDIR)/lib/$(KOS_ARCH)/libkallisti.a build/*.o
	kos-ar rcs $(TOPDIR)/lib/$(KOS_ARCH)/libkallisti_exports.a stubs/*.o

stubs/kernel_export_stubs.c: exports.txt
	$(TOPDIR)/utils/genexports/genexportstubs.sh $< stubs/kernel_export_stubs.c

stubs/arch_export_stubs.c: arch/$(KOS_ARCH)/exports.txt
	$(TOPDIR)/utils/genexports/genexportstubs.sh $< stubs/arch_export_stubs.c

include $(TOPDIR)/Makefile.prefab

clean: clean_subdirs
	rm -f build/*.o
	rm -f build/libc/*.o

run:


