# KallistiOS ##version##
# 
# tunnel Makefile
#
# $Id: Makefile,v 1.4 2002/09/05 07:40:48 bardtx Exp $
#   

TARGET = tunnel.elf
OBJS = tunnel.o plprint.o menu.o tunneldat.o

KOS_ROMDISK_DIR=romdisk

all: rm-elf $(TARGET)

include $(KOS_PATH)/scripts/Makefile.rules

clean:
	-rm -f $(TARGET) $(BIN) $(OBJS) romdisk.*

rm-elf:
	-rm -f $(TARGET) $(BIN) romdisk.*

$(TARGET): $(OBJS) romdisk.o
	kos-c++ -o $(TARGET) $(OBJS) romdisk.o $(OBJEXTRA) -ldcplib -lk++ -lgl -lpcx -lkosutils -lm

run: $(TARGET)
	$(KOS_LOADER) $(TARGET)

dist: 
	rm -f $(OBJS) romdisk.*
	$(KOS_STRIP) $(TARGET)


