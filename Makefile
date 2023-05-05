CC=musl-gcc-x86_32
CFLAGS=-Wall -D_GNU_SOURCE
all:	rundradio
rpm:	rundradio
	strip rundradio
	bar -c --license=GPLv3 --version 1.1 --release 1 --name rundradio --prefix=/usr/bin --fgroup=root --fuser=root rundradio-1.1-1.rpm rundradio
clean:
	rm -f *.o rundradio
