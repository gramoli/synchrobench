ROOT = ../../..

include $(ROOT)/common/Makefile.common

BINS = $(BINDIR)/versioned-linkedlist

CFLAGS += -std=gnu11 -g -Wall -Wextra -pedantic

.PHONY:	all clean

all:	main

versioned-lock.o: ../../utils/versioned-lock/versioned-lock.h ../../utils/versioned-lock/versioned-lock.c
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/versioned-lock.o ../../utils/versioned-lock/versioned-lock.c

versioned-linkedlist.o: versioned-linkedlist.h versioned-linkedlist.c intset.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/versioned-linkedlist.o versioned-linkedlist.c

test.o: test.c intset.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test.o test.c

main: versioned-linkedlist.o versioned-lock.o test.o
	$(CC) $(CFLAGS) $(BUILDIR)/versioned-linkedlist.o $(BUILDIR)/versioned-lock.o $(BUILDIR)/test.o -o $(BINS) $(LDFLAGS)

clean:
	-rm -f $(BINS) *.o
