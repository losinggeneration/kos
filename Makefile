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
	@exit 0
else

all: pre-make
	for i in $(DIRS); do $(MAKE) -C $$i || exit -1; done

clean:
	for i in $(DIRS); do $(MAKE) -C $$i clean || exit -1; done

distclean: clean
	-rm -f lib/$(KOS_ARCH)/*.a
	-rm -f lib/$(KOS_ARCH)/addons/*.a

pre-make:
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

install: install-tools install-libs

kos-ports_all:
	$(MAKE) -C ../kos-ports all KOS_PATH=$(CURDIR)

kos-ports_clean:
	$(MAKE) -C ../kos-ports clean KOS_PATH=$(CURDIR)

all_auto_kos_base:
	$(MAKE) all KOS_PATH=$(CURDIR)

clean_auto_kos_base:
	$(MAKE) clean KOS_PATH=$(CURDIR)
endif

