#
# Plasma PVR example
# (c)2002 Dan Potter
#   

TARGET = plasma.elf
OBJS = plasma.o

all: rm-elf $(TARGET)

include ../../../../Makefile.rules

clean:
	-rm -f $(TARGET) $(OBJS)

rm-elf:
	-rm -f $(TARGET)

$(TARGET): $(OBJS)
	$(KOS_CC) $(KOS_CFLAGS) $(KOS_LDFLAGS) -o $(TARGET) $(KOS_START) \
		$(OBJS) $(OBJEXTRA) $(KOS_LIBS)

run: $(TARGET)
	$(KOS_LOADER) $(TARGET) -n

dist:
	rm -f $(OBJS)
	$(KOS_STRIP) $(TARGET)

