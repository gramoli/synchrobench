ROOT = ../../..

include $(ROOT)/common/Makefile.common

BINS = $(BINDIR)/$(LOCK)-skiplist
CFLAGS += -std=gnu11

.PHONY:	all clean

all:	main

skiplist-lock.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/skiplist-lock.o skiplist-lock.c

lazylock.o: skiplist-lock.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/lazylock.o lazylock.c

intset.o: skiplist-lock.h lazylock.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/intset.o intset.c

test.o: skiplist-lock.h lazylock.h intset.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test.o test.c

main: skiplist-lock.o lazylock.o intset.o test.o
	$(CC) $(CFLAGS) $(BUILDIR)/skiplist-lock.o $(BUILDIR)/lazylock.o $(BUILDIR)/intset.o $(BUILDIR)/test.o -o $(BINS) $(LDFLAGS)

clean:
	-rm -f $(BINS) *.o
