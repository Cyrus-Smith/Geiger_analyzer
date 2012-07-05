CC=gcc
CFLAGS=-std=c99 -Wall -pedantic
LDLIBS=-lportaudio

# FreeBSD
CPPFLAGS=-I/usr/local/include/portaudio2
LDFLAGS=-L/usr/local/lib/portaudio2

# Debian Linux: default values are fine
#CPPFLAGS=
#LDFLAGS=

all: geiger

