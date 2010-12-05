ROOT = ..

include $(ROOT)/common/Makefile.common

BINS = lf-rt

.PHONY:	all clean

all:	main

rbtree.o: interface.h
	$(CC) $(CFLAGS) -c -o rbtree.o rbtree.c

intset.o: rbtree.h
	$(CC) $(CFLAGS) -c -o intset.o intset.c

test.o: rbtree.o intset.h
	$(CC) $(CFLAGS) -c -o test.o test.c

main: intset.o test.o $(TMILB)
	$(CC) $(CFLAGS) intset.o test.o -o lf-rt $(LDFLAGS)

clean:
	-rm -f $(BINS) *.o
