ROOT = ../..

include $(ROOT)/Makefile.in

BINS = lf-ll

.PHONY:	all clean

all:	main

intset.o: 
	$(CC) $(CFLAGS) -c -o intset.o intset.c

herlihy.o: intset.h intset.o
	$(CC) $(CFLAGS) -c -o herlihy.o herlihy.c

linkedlist.o: intset.h herlihy.h
	$(CC) $(CFLAGS) -c -o linkedlist.o linkedlist.c

lf-ll.o: intset.h herlihy.h linkedlist.h
	$(CC) $(CFLAGS) -c -o lf-ll.o lf-ll.c

main: intset.o herlihy.o linkedlist.o lf-ll.o $(TMILB)
	$(CC) $(CFLAGS) intset.o herlihy.o linkedlist.o lf-ll.o -o lf-ll $(LDFLAGS)

clean:
	rm -f $(BINS) *.o
