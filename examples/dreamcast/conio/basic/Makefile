# KallistiOS ##version##
#
# conio/basic/Makefile
# (c)2002 Dan Potter
#   
# $Id: Makefile,v 1.1 2002/04/20 19:21:49 bardtx Exp $

all: rm-elf basic.elf

include ../../../../Makefile.rules

OBJS = basic.o

clean:
	-rm -f basic.elf $(OBJS)

rm-elf:
	-rm -f basic.elf

basic.elf: $(OBJS) 
	$(KOS_CC) $(KOS_CFLAGS) $(KOS_LDFLAGS) -o basic.elf $(KOS_START) $(OBJS) $(DATAOBJS) $(OBJEXTRA) -lconio $(KOS_LIBS)

run: basic.elf
	$(KOS_LOADER) basic.elf

dist:
	-rm -f $(OBJS)
	$(KOS_STRIP) basic.elf
