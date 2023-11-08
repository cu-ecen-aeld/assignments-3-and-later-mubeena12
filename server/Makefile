#--------------------------------------
# Author: Mubeena Udyavar Kazi
# Course: ECEN 5713 - AESD
#--------------------------------------

CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -g -Wall -Werror
TARGET ?= aesdsocket
LDFLAGS ?= -pthread -lrt
AESD_IOCTL_INCLUDE_DIR ?= ../aesd-char-driver/

EXTRA_CFLAGS = -DUSE_AESD_CHAR_DEVICE=1

all: aesdsocket

aesdsocket : aesdsocket.o
	$(CC) $(CFLAGS) aesdsocket.o -o $(TARGET) $(LDFLAGS)

aesdsocket.o: aesdsocket.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -c aesdsocket.c -I$(AESD_IOCTL_INCLUDE_DIR)

clean:
	-rm -f *.o aesdsocket
