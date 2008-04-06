#
# KallistiOS lwIP test program
# (c)2002 Dan Potter
#   

# Put the filename of the output binary here
TARGET = httpd.elf

# List all of your C files here, but change the extension to ".o"
OBJS = simhost.o httpd.o romdisk.o

all: rm-elf $(TARGET)

include $(KOS_PATH)/scripts/Makefile.rules

LWIPDIR = $(KOS_PATH)/include/addons/lwip
ARCHDIR = $(LWIPDIR)/kos

CFLAGS += -DIPv4 \
	-I$(LWIPDIR) -I$(ARCHDIR) \
	-I$(LWIPDIR)/ipv4

clean:
	rm -f $(TARGET) $(OBJS) romdisk.img

rm-elf:
	rm -f $(TARGET) romdisk.*

$(TARGET): $(OBJS)
	kos-cc -o $(TARGET) $(OBJS) $(OBJEXTRA) -llwip4 -lkosutils

romdisk.img:
	$(KOS_GENROMFS) -f romdisk.img -d romdisk -v

romdisk.o: romdisk.img
	$(KOS_PATH)/bin/bin2o romdisk.img romdisk romdisk.o

run: $(TARGET)
	dc-tool -n -x $(TARGET)
