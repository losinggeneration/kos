
# Makefile for the genromfs program.

CFLAGS = -O2 -Wall #-g#
LDFLAGS = -s#-g

all: genromfs install-bin

genromfs: genromfs.o

clean:
	rm -f genromfs *.o

install: all install-bin install-man

install-bin:
	install -m 755 genromfs $(KOS_PATH)/bin

install-man:
	install -m 644 genromfs.8 $(KOS_PATH)/man8

