#
# KallistiGL test program in C++
# (c)2001 Dan Potter
#   

TARGET = gltest.elf
OBJS = gltest.o

all: rm-elf $(TARGET)

include ../../../../Makefile.rules

clean:
	-rm -f $(TARGET) $(OBJS) romdisk.*

rm-elf:
	-rm -f $(TARGET) romdisk.*

$(TARGET): $(OBJS) romdisk.o
	kos-c++ -o $(TARGET) $(OBJS) romdisk.o -lgl -lpcx -lkosutils

romdisk.img:
	$(KOS_GENROMFS) -f romdisk.img -d romdisk -v

romdisk.o: romdisk.img
	../../../../utils/bin2o/bin2o romdisk.img romdisk romdisk.o

run: $(TARGET)
	$(KOS_LOADER) $(TARGET)

dist:
	rm -f $(OBJS) romdisk.o romdisk.img
	$(KOS_STRIP) $(TARGET)

