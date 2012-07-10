CC=gcc
CXX=g++
CFLAGS=-std=c99 -Wall -pedantic
LDLIBS=-lsndfile

#DETEC=c1
#DETEC=c2
DETEC=ppp

# FreeBSD
CPPFLAGS=-I/usr/local/include/
LDFLAGS=-L/usr/local/lib/

# Debian Linux: default values are fine
#CPPFLAGS=
#LDFLAGS=

all: peakdetector

peakdetector: peakdetector.o detector_$(DETEC).o

clean:
	rm -f *.o *~

distclean: clean
	rm -f peakdetector

.PHONY: all clean distclean
