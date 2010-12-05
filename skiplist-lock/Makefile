ROOT = ..

include $(ROOT)/common/Makefile.common

BINS = lb-sl

.PHONY:	all clean

all:	main

skiplist-lock.o:
	$(CC) $(CFLAGS) -c -o skiplist-lock.o skiplist-lock.c

optimistic.o: skiplist-lock.h 
	$(CC) $(CFLAGS) -c -o optimistic.o optimistic.c 

intset.o: skiplist-lock.h optimistic.h
	$(CC) $(CFLAGS) -c -o intset.o intset.c

test.o: skiplist-lock.h optimistic.h intset.h
	$(CC) $(CFLAGS) -c -o test.o test.c

main: skiplist-lock.o optimistic.o intset.o test.o 
	$(CC) $(CFLAGS) skiplist-lock.o optimistic.o intset.o test.o -o lb-sl $(LDFLAGS)

clean:
	-rm -f $(BINS) *.o
