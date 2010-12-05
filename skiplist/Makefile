ROOT = ..

include $(ROOT)/common/Makefile.common

BINS = lf-sl

.PHONY:	all clean

all:	main

skiplist.o:
	$(CC) $(CFLAGS) -c -o skiplist.o skiplist.c

fraser.o: skiplist.h 
	$(CC) $(CFLAGS) -c -o fraser.o fraser.c 

intset.o: skiplist.h fraser.h
	$(CC) $(CFLAGS) -c -o intset.o intset.c

test.o: skiplist.h fraser.h intset.h
	$(CC) $(CFLAGS) -c -o test.o test.c

main: skiplist.o fraser.o intset.o test.o 
	$(CC) $(CFLAGS) skiplist.o fraser.o intset.o test.o -o lf-sl $(LDFLAGS)

clean:
	-rm -f $(BINS) *.o
