ROOT = ../../..

include $(ROOT)/common/Makefile.common

#atomic loads instead of unit loads (comment out for unit loads)
#CFLAGS += -DATOMIC_LOADS
#read/write set size tracking
#CFLAGS += -DRW_SIZE
#sequential TFAVL (comment out for normal sequential AVL)
CFLAGS += -std=gnu89
CFLAGS += -DTFAVLSEQ
#2 mainteance threds (comment out for 1 mainteance thread)
#CFLAGS += -DTWO_MAINTENANCE
#bias (comment out next line for no bias)
#CFLAGS += -DBIAS_RANGE
#no maintenance (comment out for maintenance)
CFLAGS += -DNO_MAINTENANCE
CFLAGS += -DMICROBENCH

BINS = $(BINDIR)/$(STM)-specfriendly-tree 
#$(BINDIR)/lf-st-move $(BINDIR)/lf-st-nest

ifeq ($(STM),SEQUENTIAL)
  BINS = $(BINDIR)/sequential-specfriendly-tree 
else
  #sequential TFAVL (comment out for normal sequential AVL)
  CFLAGS += -DTFAVLSEQ
  BINS = $(BINDIR)/$(STM)-specfriendly-tree
endif

CFLAGS += -DNO_UNITLOADS

.PHONY:	all clean

all:	main #main_nest

sftree.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/sftree.o sftree.c

intset.o: sftree.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/intset.o intset.c

test.o: sftree.h intset.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test.o test.c

main: sftree.o intset.o test.o 
	$(CC) $(CFLAGS) $(BUILDIR)/sftree.o $(BUILDIR)/intset.o $(BUILDIR)/test.o -o $(BINS) $(LDFLAGS)

#test_move.o: sftree.h intset.h
#	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test_move.o test_move.c

#main_move: sftree.o intset.o test_move.o 
#	$(CC) $(CFLAGS) $(BUILDIR)/sftree.o $(BUILDIR)/intset.o $(BUILDIR)/test_move.o -o $(BINDIR)/lf-st-move $(LDFLAGS)

#test_nested.o: sftree.h intset.h
#	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test_nested.o test_nested.c

#main_nest: sftree.o intset.o test_nested.o 
#	$(CC) $(CFLAGS) $(BUILDIR)/sftree.o $(BUILDIR)/intset.o $(BUILDIR)/test_nested.o -o $(BINDIR)/lf-st-nest $(LDFLAGS)


clean:
	-rm -f $(BINS)
