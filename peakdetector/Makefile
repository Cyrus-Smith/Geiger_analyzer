CC=gcc
CXX=g++
CFLAGS=-std=c99 -Wall -pedantic
LDLIBS=-lsndfile

# FreeBSD
CPPFLAGS=-I/usr/local/include/
LDFLAGS=-L/usr/local/lib/

# Debian Linux: default values are fine
#CPPFLAGS=
#LDFLAGS=

all: peakdetector

peakdetector: peakdetector.o detector_c1.o detector_ppp.o

clean:
	rm -f *.o *~

distclean: clean
	rm -f peakdetector

.PHONY: all clean distclean
