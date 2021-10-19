ROOT = ../../..

include $(ROOT)/common/Makefile.common

BINS = $(BINDIR)/lockfree-rotating-skiplist

.PHONY:	all clean

all:	main

$(BUILDIR)/ptst.o: ptst.h common.h 
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/ptst.o ptst.c 

$(BUILDIR)/garbagecoll.o: garbagecoll.h ptst.h common.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/garbagecoll.o garbagecoll.c 

$(BUILDIR)/nohotspot_ops.o: skiplist.h background.h nohotspot_ops.h common.h 
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/nohotspot_ops.o nohotspot_ops.c 

$(BUILDIR)/skiplist.o: skiplist.h background.h common.h 
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/skiplist.o skiplist.c 

$(BUILDIR)/background.o: background.h skiplist.h common.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/background.o background.c 

intset.o: intset.h nohotspot_ops.h skiplist.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/intset.o intset.c 

test.o: intset.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test.o test.c

main: intset.o $(BUILDIR)/background.o $(BUILDIR)/skiplist.o $(BUILDIR)/nohotspot_ops.o test.o $(BUILDIR)/ptst.o $(BUILDIR)/garbagecoll.o
	$(CC) $(CFLAGS) $(BUILDIR)/garbagecoll.o $(BUILDIR)/ptst.o $(BUILDIR)/skiplist.o $(BUILDIR)/nohotspot_ops.o $(BUILDIR)/intset.o $(BUILDIR)/background.o $(BUILDIR)/test.o -o $(BINS) $(LDFLAGS)

clean:
	-rm -rf ${BUILDIR}
	-rm -f $(BINS)
