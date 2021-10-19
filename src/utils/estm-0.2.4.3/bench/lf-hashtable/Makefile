ROOT = ../..

include $(ROOT)/Makefile.in

BINS = lf-ht

.PHONY:	all clean

all:	main

LLREP=../lf-linkedlist/

$(LLREP)intset.o:
	$(CC) $(CFLAGS) -c -o $(LLREP)intset.o $(LLREP)intset.c

$(LLREP)harris.o: $(LLREP)intset.h $(LLREP)intset.o
	$(CC) $(CFLAGS) -c -o $(LLREP)harris.o $(LLREP)harris.c

$(LLREP)linkedlist.o: $(LLREP)intset.h $(LLREP)intset.o $(LLREP)harris.o
	$(CC) $(CFLAGS) -c -o $(LLREP)linkedlist.o $(LLREP)linkedlist.c

#$(LLREP)recursive.o: $(LLREP)intset.o $(LLREP)harris.o 
#	$(CC) $(CFLAGS) -c -o $(LLREP)recursive.o $(LLREP)recursive.c

lf-ht.o: $(LLREP)intset.o $(LLREP)harris.o $(LLREP)linkedlist.o
	$(CC) $(CFLAGS) -c -o lf-ht.o lf-ht.c

main: $(LLREP)intset.o $(LLREP)harris.o $(LLREP)linkedlist.o lf-ht.o 
	$(CC) $(CFLAGS) $(LLREP)intset.o $(LLREP)harris.o $(LLREP)linkedlist.o lf-ht.o -o lf-ht $(LDFLAGS)

clean:
	rm -f $(BINS) *.o
