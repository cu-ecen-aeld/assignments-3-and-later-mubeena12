#--------------------------------------
# Author: Mubeena Udyavar Kazi
# Course: ECEN 5713 - AESD
#--------------------------------------

CC := $(CROSS_COMPILE)gcc

CFLAGS := -g -Wall -Werror

all: aesdsocket

aesdsocket : aesdsocket.o
	$(CC) $(CFLAGS) aesdsocket.o -o aesdsocket -lpthread

aesdsocket.o: aesdsocket.c
	$(CC) $(CFLAGS) -c aesdsocket.c

clean:
	-rm -f *.o aesdsocket
