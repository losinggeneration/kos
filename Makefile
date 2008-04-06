# KallistiOS ##version##
#
# Root Makefile
# Copyright (C)2003 Dan Potter
#   
# $Id: Makefile,v 1.5 2002/04/20 17:23:31 bardtx Exp $

# Add stuff to DIRS to auto-compile it with the big tree.
DIRS = utils
ifdef KOS_CCPLUS
	DIRS += libk++
endif
DIRS += kernel addons # examples

export TOPDIR=${PWD}

# Detect a non-working or missing environ.sh file.
ifndef KOS_PATH
error:
	@echo You don\'t seem to have a working environ.sh file. Please take a look at
	@echo doc/README for more info.
	@echo
	@echo Perhaps you need to run kos-make instead.
	@echo Usually you should first run
	@echo utils/gnu_wrappers/kos-make install-tools
	@exit 0
else

all: pre-make
	for i in $(DIRS); do $(MAKE) -C $$i || exit -1; done

clean:
	for i in $(DIRS); do $(MAKE) -C $$i clean || exit -1; done

distclean: clean
	-rm -f lib/$(KOS_ARCH)/*.a
	-rm -f lib/$(KOS_ARCH)/addons/*.a

pre-make: install-headers install-${KOS_ARCH}-headers
	install -m 755 utils/bin2o/bin2o ${KOS_PATH}/bin/bin2o

install-tools:
	@echo Installing tools
	install -m 660 environ.sh ${KOS_PATH}/bin/environ.sh
	install -m 660 environ_base.sh ${KOS_PATH}/bin/environ_base.sh
	install -m 660 environ_${KOS_ARCH}.sh ${KOS_PATH}/bin/environ_${KOS_ARCH}.sh
	install -m 755 utils/gnu_wrappers/kos-ar ${KOS_PATH}/bin/kos-ar
	install -m 755 utils/gnu_wrappers/kos-as ${KOS_PATH}/bin/kos-as
	install -m 755 utils/gnu_wrappers/kos-c++ ${KOS_PATH}/bin/kos-c++
	install -m 755 utils/gnu_wrappers/kos-cc ${KOS_PATH}/bin/kos-cc
	install -m 755 utils/gnu_wrappers/kos-ld ${KOS_PATH}/bin/kos-ld
	install -m 755 utils/gnu_wrappers/kos-make ${KOS_PATH}/bin/kos-make
	install -m 755 utils/gnu_wrappers/kos-objcopy ${KOS_PATH}/bin/kos-objcopy
	install -m 755 utils/gnu_wrappers/kos-ranlib ${KOS_PATH}/bin/kos-ranlib
	install -m 755 utils/gnu_wrappers/kos-strip ${KOS_PATH}/bin/kos-strip
	@echo Installing Makefile.rules
	install -d ${KOS_PATH}/scripts
	install -m 660 Makefile.rules ${KOS_PATH}/scripts/Makefile.rules

install-libs:
	@echo Installing libraries
	install -m 660 lib/${KOS_ARCH}/libkallisti.a ${KOS_PATH}/lib/libkallisti.a
	install -m 660 lib/${KOS_ARCH}/libkallisti_exports.a ${KOS_PATH}/lib/libkallisti_exports.a
	install -m 770 -d ${KOS_PATH}/lib/addons
	install -m 660 lib/${KOS_ARCH}/addons/libkosutils.a ${KOS_PATH}/lib/addons/libkosutils.a
	install -m 660 kernel/arch/${KOS_ARCH}/kernel/startup.o ${KOS_PATH}/lib/startup.o
ifdef KOS_CCPLUS
	install -m 660 lib/$(KOS_ARCH)/libk++.a ${KOS_PATH}/lib/libk++.a
endif

install: all install-tools install-libs

install-headers:
	install -d ${KOS_PATH}/include/kos
	install -d ${KOS_PATH}/include/kos
	install -d ${KOS_PATH}/include/sys
	install -d ${KOS_PATH}/include/arch
	install -d ${KOS_PATH}/include/arpa
	install -d ${KOS_PATH}/include/addons
	install -d ${KOS_PATH}/include/addons/kos
	install -d ${KOS_PATH}/include/netinet
	install -m 660 include/*.h ${KOS_PATH}/include/
	install -m 660 include/kos/*.h ${KOS_PATH}/include/kos/
	install -m 660 include/arpa/*.h ${KOS_PATH}/include/arpa
	install -m 660 include/netinet/*.h ${KOS_PATH}/include/netinet
	install -m 660 include/sys/*.h ${KOS_PATH}/include/sys/
	install -m 660 include/addons/kos/*.h ${KOS_PATH}/include/addons/kos


install-gba-headers:
	install -d ${KOS_PATH}/include/arch/gba
	install -d ${KOS_PATH}/include/arch/gba/gba
	install -d ${KOS_PATH}/include/arch/gba/arch
	install -m 660 include/arch/gba/gba/*.h ${KOS_PATH}/include/arch/gba/gba/
	install -m 660 include/arch/gba/arch/*.h ${KOS_PATH}/include/arch/gba/arch/

install-ps2-headers:
	install -d ${KOS_PATH}/include/arch/ps2
	install -d ${KOS_PATH}/include/arch/ps2/ps2
	install -d ${KOS_PATH}/include/arch/ps2/arch
	install -m 660 include/arch/ps2/ps2/*.h ${KOS_PATH}/include/arch/ps2/ps2
	install -m 660 include/arch/ps2/arch/*.h ${KOS_PATH}/include/arch/ps2/arch

install-ia32-headers:
	install -d ${KOS_PATH}/include/arch/ia32
	install -d ${KOS_PATH}/include/arch/ia32/arch
	install -d ${KOS_PATH}/include/arch/ia32/ia32
	install -m 660 include/arch/ia32/arch/*.h ${KOS_PATH}/include/arch/ia32/arch
	install -m 660 include/arch/ia32/ia32/*.h ${KOS_PATH}/include/arch/ia32/ia32

install-dreamcast-headers:
	install -d ${KOS_PATH}/include/arch/dreamcast
	install -d ${KOS_PATH}/include/arch/dreamcast/dc
	install -d ${KOS_PATH}/include/arch/dreamcast/dc/net
	install -d ${KOS_PATH}/include/arch/dreamcast/dc/maple
	install -d ${KOS_PATH}/include/arch/dreamcast/dc/modem
	install -d ${KOS_PATH}/include/arch/dreamcast/dc/sound
	install -d ${KOS_PATH}/include/arch/dreamcast/arch
	install -d ${KOS_PATH}/include/arch/dreamcast/navi
	install -m 660 include/arch/dreamcast/dc/*.h ${KOS_PATH}/include/arch/dreamcast/dc
	install -m 660 include/arch/dreamcast/dc/net/*.h ${KOS_PATH}/include/arch/dreamcast/dc/net
	install -m 660 include/arch/dreamcast/dc/maple/*.h ${KOS_PATH}/include/arch/dreamcast/dc/maple
	install -m 660 include/arch/dreamcast/dc/modem/*.h ${KOS_PATH}/include/arch/dreamcast/dc/modem
	install -m 660 include/arch/dreamcast/dc/sound/*.h ${KOS_PATH}/include/arch/dreamcast/dc/sound
	install -m 660 include/arch/dreamcast/arch/*.h ${KOS_PATH}/include/arch/dreamcast/arch
	install -m 660 include/arch/dreamcast/navi/*.h ${KOS_PATH}/include/arch/dreamcast/navi

examples:
	$(MAKE) -C examples/${KOS_ARCH} all

endif

