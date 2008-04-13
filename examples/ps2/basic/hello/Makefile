#
# Basic KallistiOS skeleton / test program
# (c)2001 Dan Potter
#   

# Put the filename of the output binary here
TARGET = hello.elf

# List all of your C files here, but change the extension to ".o"
OBJS = hello.o

all: rm-elf $(TARGET)

include $(KOS_PATH)/scripts/Makefile.rules

clean:
	-rm -f $(TARGET) $(OBJS) romdisk.*

rm-elf:
	-rm -f $(TARGET) romdisk.*

KOS_START=

# If you don't need a ROMDISK, then remove "romdisk.o" from the next few
# lines. Also change the -l arguments to include everything you need,
# such as -lmp3, etc.. these will need to go _before_ $(KOS_LIBS)
$(TARGET): $(OBJS) romdisk.o
	kos-cc -o $(TARGET) $(OBJS) $(OBJEXTRA)

# You can safely remove the next two targets if you don't use a ROMDISK
romdisk.img:
	$(KOS_GENROMFS) -f romdisk.img -d romdisk -v

romdisk.o: romdisk.img
	$(KOS_PATH)/bin/bin2o romdisk.img romdisk romdisk.o

run: $(TARGET)
	$(KOS_LOADER) $(TARGET)

dist:
	rm -f $(OBJS) romdisk.o romdisk.img
	$(KOS_STRIP) $(TARGET)

