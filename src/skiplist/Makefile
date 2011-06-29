ROOT = ../..

include $(ROOT)/common/Makefile.common

BINS = $(BINDIR)/lf-sl

.PHONY:	all clean

all:	main

skiplist.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/skiplist.o skiplist.c

fraser.o: skiplist.h 
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/fraser.o fraser.c 

intset.o: skiplist.h fraser.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/intset.o intset.c

test.o: skiplist.h fraser.h intset.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test.o test.c

main: skiplist.o fraser.o intset.o test.o 
	$(CC) $(CFLAGS) $(BUILDIR)/skiplist.o $(BUILDIR)/fraser.o $(BUILDIR)/intset.o $(BUILDIR)/test.o -o $(BINS) $(LDFLAGS)

clean:
	-rm -f $(BINS)
