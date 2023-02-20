CC=musl-gcc-x86_32
CFLAGS=-Wall
all:	rundradio
rpm:	rundradio
	strip rundradio
	bar -c --license=GPLv3 --version 1.0 --release 1 --name rundradio --prefix=/usr/bin --fgroup=root --fuser=root rundradio-1.0-1.rpm rundradio
clean:
	rm -f *.o rundradio
