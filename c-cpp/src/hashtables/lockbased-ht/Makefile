ROOT = ../../..

include $(ROOT)/common/Makefile.common

BINS = $(BINDIR)/$(LOCK)-hashtable 
LLREP = $(ROOT)/src/linkedlists/lazy-list
CFLAGS += -std=gnu89

.PHONY:	all clean

all:	main

ll-intset.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/intset.o $(LLREP)/intset.c

coupling.o: ll-intset.o
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/coupling.o $(LLREP)/coupling.c

lazy.o: ll-intset.o
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/lazy.o $(LLREP)/lazy.c      

linkedlist-lock.o: ll-intset.o coupling.o lazy.o
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/linkedlist-lock.o $(LLREP)/linkedlist-lock.c

hashtable-lock.o: ll-intset.o coupling.o lazy.o linkedlist-lock.o
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/hashtable-lock.o hashtable-lock.c

test.o: ll-intset.o coupling.o lazy.o linkedlist-lock.o hashtable-lock.o
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test.o test.c

main: ll-intset.o coupling.o lazy.o linkedlist-lock.o hashtable-lock.o test.o
	$(CC) $(CFLAGS) $(BUILDIR)/intset.o $(BUILDIR)/coupling.o $(BUILDIR)/lazy.o $(BUILDIR)/linkedlist-lock.o $(BUILDIR)/hashtable-lock.o $(BUILDIR)/test.o -o $(BINS) $(LDFLAGS)

clean:
	rm -f $(BINS)
