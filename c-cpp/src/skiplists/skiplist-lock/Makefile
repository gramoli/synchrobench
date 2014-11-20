ROOT = ../../..

include $(ROOT)/common/Makefile.common

BINS = $(BINDIR)/$(LOCK)-skiplist

.PHONY:	all clean

all:	main

skiplist-lock.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/skiplist-lock.o skiplist-lock.c

optimistic.o: skiplist-lock.h 
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/optimistic.o optimistic.c 

intset.o: skiplist-lock.h optimistic.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/intset.o intset.c

test.o: skiplist-lock.h optimistic.h intset.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test.o test.c

main: skiplist-lock.o optimistic.o intset.o test.o 
	$(CC) $(CFLAGS) $(BUILDIR)/skiplist-lock.o $(BUILDIR)/optimistic.o $(BUILDIR)/intset.o $(BUILDIR)/test.o -o $(BINS) $(LDFLAGS)

clean:
	-rm -f $(BINS) *.o
