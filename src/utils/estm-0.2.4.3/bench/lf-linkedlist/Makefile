ROOT = ../..

include $(ROOT)/Makefile.in

BINS = lf-ll

.PHONY:	all clean

all:	main

intset.o: 
	$(CC) $(CFLAGS) -c -o intset.o intset.c

harris.o: intset.h intset.o
	$(CC) $(CFLAGS) -c -o harris.o harris.c

linkedlist.o: intset.h harris.h
	$(CC) $(CFLAGS) -c -o linkedlist.o linkedlist.c

lf-ll.o: intset.h harris.h linkedlist.h
	$(CC) $(CFLAGS) -c -o lf-ll.o lf-ll.c

main: intset.o harris.o linkedlist.o lf-ll.o $(TMILB)
	$(CC) $(CFLAGS) intset.o harris.o linkedlist.o lf-ll.o -o lf-ll $(LDFLAGS)

clean:
	rm -f $(BINS) *.o
