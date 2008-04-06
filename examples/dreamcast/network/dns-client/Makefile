# Put the filename of the output binary here
TARGET = dns-client.elf

# List all of your C files here, but change the extension to ".o"
OBJS = dnslookup.o

LWIPDIR = $(KOS_PATH)/include/addons/lwip
ARCHDIR = $(LWIPDIR)/kos

all: rm-elf $(TARGET)

include ../../../../Makefile.rules

CFLAGS += -DIPv4 \
	-I$(LWIPDIR) -I$(ARCHDIR) \
	-I$(LWIPDIR)/ipv4

clean:
	rm -f $(TARGET) $(OBJS)

rm-elf:
	rm -f $(TARGET)

$(TARGET): $(OBJS)
	kos-cc -o $(TARGET) $(OBJS) $(OBJEXTRA) -llwip4 -lkosutils

run: $(TARGET)
	dc-tool -n -x $(TARGET)
