#
# Basic KallistiOS skeleton / test program
# (c)2001 Dan Potter
#   

# Put the filename of the output binary here
TARGET = basic.elf

# List all of your C files here, but change the extension to ".o"
OBJS = basic.o

all: rm-elf $(TARGET)

include ../../../../Makefile.rules

clean:
	-rm -f $(TARGET) $(OBJS)

rm-elf:
	-rm -f $(TARGET)

$(TARGET): $(OBJS)
	kos-cc -o $(TARGET) $(OBJS) $(OBJEXTRA)

run: $(TARGET)
	$(KOS_LOADER) $(TARGET) -n

dist:
	rm -f $(OBJS)
	$(KOS_STRIP) $(TARGET)

