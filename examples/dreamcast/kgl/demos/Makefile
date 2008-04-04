# KallistiOS ##version##
#
# examples/dreamcast/kgl/demos/Makefile
# (c)2001-2002 Dan Potter
#   
# $Id: Makefile,v 1.2 2002/03/04 05:00:45 bardtx Exp $

all:
ifdef KOS_CCPLUS
	$(MAKE) -C tunnel
endif
	$(MAKE) -C bubbles

clean:
ifdef KOS_CCPLUS
	$(MAKE) -C tunnel clean
endif
	$(MAKE) -C bubbles clean
		
dist:
ifdef KOS_CCPLUS
	$(MAKE) -C tunnel dist
endif
	$(MAKE) -C bubbles dist


