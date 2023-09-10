#--------------------------------------
# Author: Mubeena Udyavar Kazi
# Course: ECEN 5713 - AESD
#--------------------------------------

CC := gcc

ifeq ($(CROSS_COMPILE),aarch64-none-linux-gnu-)
   CC := aarch64-none-linux-gnu-gcc
endif

all: echo_cc writer

echo_cc:
	@echo "Compiling with $(CC) ..."

writer: writer.o
	$(CC) -g writer.o -o writer 

writer.o: writer.c
	$(CC) -g -c writer.c

clean:
	rm *.o writer
