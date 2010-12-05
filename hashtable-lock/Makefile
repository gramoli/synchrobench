ROOT = ..

include $(ROOT)/common/Makefile.common

BINS = lb-ht 

.PHONY:	all clean

all:	main

LLREP=../linkedlist-lock/

$(LLREP)intset.o:
	$(CC) $(CFLAGS) -c -o $(LLREP)intset.o $(LLREP)intset.c

$(LLREP)coupling.o: $(LLREP)intset.o
	$(CC) $(CFLAGS) -c -o $(LLREP)coupling.o $(LLREP)coupling.c

$(LLREP)lazy.o: $(LLREP)intset.o
	$(CC) $(CFLAGS) -c -o $(LLREP)lazy.o $(LLREP)lazy.c      

$(LLREP)linkedlist-lock.o: $(LLREP)intset.o $(LLREP)coupling.o $(LLREP)lazy.o
	$(CC) $(CFLAGS) -c -o $(LLREP)linkedlist-lock.o $(LLREP)linkedlist-lock.c

hashtable-lock.o: $(LLREP)intset.o $(LLREP)coupling.o $(LLREP)lazy.o $(LLREP)linkedlist-lock.o
	$(CC) $(CFLAGS) -c -o hashtable-lock.o hashtable-lock.c

test.o: $(LLREP)intset.o $(LLREP)coupling.o $(LLREP)lazy.o $(LLREP)linkedlist-lock.o hashtable-lock.o
	$(CC) $(CFLAGS) -c -o test.o test.c

main: $(LLREP)intset.o $(LLREP)coupling.o $(LLREP)lazy.o $(LLREP)linkedlist-lock.o hashtable-lock.o test.o
	$(CC) $(CFLAGS) $(LLREP)intset.o $(LLREP)coupling.o $(LLREP)lazy.o $(LLREP)linkedlist-lock.o hashtable-lock.o test.o -o lb-ht $(LDFLAGS)

clean:
	rm -f $(BINS) *.o
