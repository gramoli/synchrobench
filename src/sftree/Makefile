ROOT = ../..

include $(ROOT)/common/Makefile.common

#atomic loads instead of unit loads (comment out for unit loads)
#CFLAGS += -DATOMIC_LOADS
#read/write set size tracking
#CFLAGS += -DRW_SIZE
#2 mainteance threads (comment out for 1 mainteance thread)
#CFLAGS += -DTWO_MAINTENANCE
#bias (comment out next line for no bias)
#CFLAGS += -DBIAS_RANGE
#no maintenance (comment out for maintenance)
#CFLAGS += -DNO_MAINTENANCE
CFLAGS += -DMICROBENCH
CFLASG +=  -Wno-uninitialized -Wno-unused-but-set-variable

ifeq ($(STM),SEQUENTIAL)
  BINS = $(BINDIR)/sequential-sftree  
else
  #sequential TFAVL (comment out for normal sequential AVL)
  CFLAGS += -DTFAVLSEQ
  BINS = $(BINDIR)/$(STM)-linkedlist
endif

#BINS = $(BINDIR)/$(STM)-sftree $(BINDIR)/$(STM)-sftree-move $(BINDIR)/$(STM)-sftree-nest

CFLAGS += -DNO_UNITLOADS

.PHONY:	all clean

all:	main

sftree.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/sftree.o sftree.c

intset.o: sftree.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/intset.o intset.c

test.o: sftree.h intset.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test.o test.c

main: sftree.o intset.o test.o 
	$(CC) $(CFLAGS) $(BUILDIR)/sftree.o $(BUILDIR)/intset.o $(BUILDIR)/test.o -o $(BINS) $(LDFLAGS)

test_move.o: sftree.h intset.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test_move.o test_move.c

main_move: sftree.o intset.o test_move.o 
	$(CC) $(CFLAGS) $(BUILDIR)/sftree.o $(BUILDIR)/intset.o $(BUILDIR)/test_move.o -o $(BINS)-move $(LDFLAGS)

test_nested.o: sftree.h intset.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test_nested.o test_nested.c

main_nest: sftree.o intset.o test_nested.o 
	$(CC) $(CFLAGS) $(BUILDIR)/sftree.o $(BUILDIR)/intset.o $(BUILDIR)/test_nested.o -o $(BINS)-sftree-nest $(LDFLAGS)


clean:
	-rm -f $(BINS)
