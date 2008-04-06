
TARGET = vq-example.elf
OBJS = vq-example.o fruit.o 

all: rm-elf $(TARGET)

include $(KOS_PATH)/scripts/Makefile.rules

clean:
	-rm -f $(TARGET) $(OBJS) fruit.vq

rm-elf:
	-rm -f $(TARGET) 

$(TARGET): $(OBJS) 
	$(KOS_CC) $(KOS_CFLAGS) $(KOS_LDFLAGS) -o $(TARGET) $(KOS_START) \
		$(OBJS) $(OBJEXTRA) -L$(TOPDIR)/lib -lgl $(KOS_LIBS) -lm

fruit.o: fruit.vq
	$(KOS_PATH)/bin/bin2o fruit.vq fruit fruit.o

fruit.vq: fruit.jpg
	$(KOS_PATH)/bin/vqenc -t -v fruit.jpg

run: $(TARGET)
	$(KOS_LOADER) $(TARGET)

dist:
	rm -f $(OBJS) 
	$(KOS_STRIP) $(TARGET)

