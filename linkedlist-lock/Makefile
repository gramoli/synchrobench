ROOT = ..

include $(ROOT)/common/Makefile.common

BINS = lb-ll

.PHONY:	all clean

all:	main

linkedlist-lock.o: 
	$(CC) $(CFLAGS) -c -o linkedlist-lock.o linkedlist-lock.c

lazy.o: linkedlist-lock.h linkedlist-lock.o
	$(CC) $(CFLAGS) -c -o lazy.o lazy.c	 

coupling.o: linkedlist-lock.h linkedlist-lock.o
	$(CC) $(CFLAGS) -c -o coupling.o coupling.c

intset.o: linkedlist-lock.h coupling.h lazy.h
	$(CC) $(CFLAGS) -c -o intset.o intset.c

test.o: linkedlist-lock.h coupling.h lazy.h intset.h
	$(CC) $(CFLAGS) -c -o test.o test.c

main: linkedlist-lock.o coupling.o lazy.o intset.o test.o
	$(CC) $(CFLAGS) linkedlist-lock.o lazy.o coupling.o intset.o test.o -o lb-ll $(LDFLAGS)

clean:
	rm -f $(BINS) *.o
