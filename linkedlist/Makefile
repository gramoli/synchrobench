ROOT = ..

include $(ROOT)/common/Makefile.common

BINS = lf-ll

.PHONY:	all clean

all:	main

linkedlist.o: 
	$(CC) $(CFLAGS) -c -o linkedlist.o linkedlist.c

harris.o: linkedlist.h linkedlist.o
	$(CC) $(CFLAGS) -c -o harris.o harris.c

intset.o: linkedlist.h harris.h
	$(CC) $(CFLAGS) -c -o intset.o intset.c

test.o: linkedlist.h harris.h intset.h
	$(CC) $(CFLAGS) -c -o test.o test.c

main: linkedlist.o harris.o intset.o test.o $(TMILB)
	$(CC) $(CFLAGS) linkedlist.o harris.o intset.o test.o -o lf-ll $(LDFLAGS)

clean:
	-rm -f $(BINS) *.o
