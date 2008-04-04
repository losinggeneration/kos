# KallistiOS ##version##
#
# examples/dreamcast/basic/Makefile
# (c)2002 Dan Potter
#   
# $Id: Makefile,v 1.4 2002/10/08 07:51:20 bardtx Exp $

all:
	$(MAKE) -C exec
	$(MAKE) -C threading
	$(MAKE) -C fpu
	$(MAKE) -C asserthnd
	$(MAKE) -C stacktrace
	$(MAKE) -C mmu

clean:
	$(MAKE) -C exec clean
	$(MAKE) -C threading clean
	$(MAKE) -C fpu clean
	$(MAKE) -C asserthnd clean
	$(MAKE) -C stacktrace clean
	$(MAKE) -C mmu clean
		
dist:
	$(MAKE) -C exec dist
	$(MAKE) -C threading dist
	$(MAKE) -C fpu dist
	$(MAKE) -C asserthnd dist
	$(MAKE) -C stacktrace dist
	$(MAKE) -C mmu dist


