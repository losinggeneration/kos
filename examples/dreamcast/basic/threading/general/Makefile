# KallistiOS ##version##
#
# basic/threading/Makefile
# (c)2001 Dan Potter
#   
# $Id: Makefile,v 1.3 2002/04/20 06:25:41 bardtx Exp $

all: rm-elf general_threading_test.elf

include ../../../../../Makefile.rules

OBJS = general_threading_test.o

clean:
	-rm -f general_threading_test.elf $(OBJS)

rm-elf:
	-rm -f general_threading_test.elf

general_threading_test.elf: $(OBJS) 
	$(KOS_CC) $(KOS_CFLAGS) $(KOS_LDFLAGS) -o general_threading_test.elf $(KOS_START) $(OBJS) $(DATAOBJS) $(OBJEXTRA) $(KOS_LIBS)

run: general_threading_test.elf
	$(KOS_LOADER) general_threading_test.elf

dist:
	-rm -f $(OBJS)
	$(KOS_STRIP) general_threading_test.elf
