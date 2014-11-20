ROOT = ../../..

include $(ROOT)/common/Makefile.common

ELASTICITY ?= 2

ifeq ($(STM),SEQUENTIAL)
  BINS = $(BINDIR)/sequential-rbtree
else
  BINS = $(BINDIR)/$(STM)-rbtree
endif

.PHONY:	all clean

all:	main

rbtree.o: interface.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/rbtree.o rbtree.c

intset.o: rbtree.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/intset.o intset.c

test.o: rbtree.o intset.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test.o test.c

main: intset.o test.o $(TMILB)
	$(CC) $(CFLAGS) $(BUILDIR)/intset.o $(BUILDIR)/test.o -o $(BINS) $(LDFLAGS)

clean:
	-rm -f $(BINS) *.o
