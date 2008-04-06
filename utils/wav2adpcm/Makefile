
# Makefile for the wav2adpcm program.

CFLAGS = -O2 -Wall -g
LDFLAGS = -g

install: all
	install -m 755 wav2adpcm $(KOS_PATH)/bin

all: wav2adpcm

clean:
	-rm -f wav2adpcm.o wav2adpcm
