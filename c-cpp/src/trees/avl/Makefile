ROOT = ../../..

include $(ROOT)/common/Makefile.common

BINS = $(BINDIR)/ESTM0243-avlnounit

CFLAGS += -DNO_UNITLOADS -DTFAVLSEQ -DMICROBENCH -I./include


.PHONY:	all clean

all:	main

avlnounit.o:
	$(CC) $(CFLAGS) -c -o avlnounit.o avlnounit.c

intset.o: avlnounit.h
	$(CC) $(CFLAGS) -c -o intset.o intset.c

test.o: avlnounit.h intset.h
	$(CC) $(CFLAGS) -c -o test.o test.c

main: avlnounit.o intset.o test.o 
	$(CC) $(CFLAGS) avlnounit.o intset.o test.o -o $(BINS) $(LDFLAGS)

clean:
	-rm -f $(BINS) *.o
