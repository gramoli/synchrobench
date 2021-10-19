#VERSION = TCMALLOC_NO_UPDATE

ROOT = ../../..

include $(ROOT)/common/Makefile.common

.PHONY:	all clean
all:	main

BINS = $(BINDIR)/lockfree-bst

CC = g++
CFLAGS += -std=gnu++0x

main: test.o
	$(CC) $(CFLAGS) ${BUILDIR}/test.o -o $(BINS) $(LDFLAGS)

test.o: wfrbt.h test.c
	$(CC) $(CFLAGS) -c -o ${BUILDIR}/test.o test.c
clean:
	-rm -f $(BINS) *.o
