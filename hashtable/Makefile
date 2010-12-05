ROOT = ..

include $(ROOT)/common/Makefile.common

BINS = lf-ht

.PHONY:	all clean

all:	main

LLREP=../linkedlist/

$(LLREP)linkedlist.o:
	$(CC) $(CFLAGS) -c -o $(LLREP)linkedlist.o $(LLREP)linkedlist.c

$(LLREP)harris.o: $(LLREP)linkedlist.h $(LLREP)linkedlist.o
	$(CC) $(CFLAGS) -c -o $(LLREP)harris.o $(LLREP)harris.c

$(LLREP)intset.o: $(LLREP)linkedlist.h $(LLREP)linkedlist.o $(LLREP)harris.o
	$(CC) $(CFLAGS) -c -o $(LLREP)intset.o $(LLREP)intset.c

hashtable.o: $(LLREP)linkedlist.h $(LLREP)intset.o $(LLREP)harris.o $(LLREP)intset.o
	$(CC) $(CFLAGS) -c -o hashtable.o hashtable.c

intset.o: $(LLREP)linkedlist.h $(LLREP)intset.o $(LLREP)harris.o $(LLREP)intset.o hashtable.o
	$(CC) $(CFLAGS) -c -o intset.o intset.c

test.o: $(LLREP)linkedlist.o $(LLREP)harris.o $(LLREP)intset.o hashtable.o intset.o
	$(CC) $(CFLAGS) -c -o test.o test.c

main: $(LLREP)linkedlist.o $(LLREP)harris.o $(LLREP)intset.o hashtable.o intset.o test.o 
	$(CC) $(CFLAGS) $(LLREP)linkedlist.o $(LLREP)harris.o $(LLREP)intset.o hashtable.o intset.o test.o -o lf-ht $(LDFLAGS)

clean:
	-rm -f $(BINS) *.o
