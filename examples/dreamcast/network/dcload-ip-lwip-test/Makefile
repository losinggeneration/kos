TARGET = dcload-lwip.elf

OBJS = main.o

LWIPDIR := $(KOS_PATH)/include/addons/lwip
LWIPARCH := $(LWIPDIR)/kos
CFLAGS := $(KOS_CFLAGS) -DIPv4 -DLWIP_DEBUG \
	-I$(LWIPDIR) -I$(LWIPARCH) \
	-I$(LWIPDIR)/ipv4 

all: rm-elf $(TARGET)

include ../../../../Makefile.rules

clean:
	-rm -f $(TARGET) $(OBJS) romdisk.*

rm-elf:
	-rm -f $(TARGET) romdisk.*

$(TARGET): $(OBJS)
	kos-cc -o $(TARGET) $(OBJS) $(OBJEXTRA) -llwip4

dist:
	rm -f $(OBJS)
	$(KOS_STRIP) $(TARGET)
