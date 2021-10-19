ROOT = ../../..

include $(ROOT)/common/Makefile.common

BINS = $(BINDIR)/lockfree-numask-skiplist
CXX = g++

.PHONY:	all clean

all:	main

nohotspot_ops.o: skiplist.h nohotspot_ops.h search.h
	$(CXX) $(CFLAGS) -c -o $(BUILDIR)/nohotspot_ops.o nohotspot_ops.cpp -std=c++11 -I.

skiplist.o: skiplist.h queue.h allocator.h
	$(CXX) $(CFLAGS) -c -o $(BUILDIR)/skiplist.o skiplist.cpp -std=c++11 -I.
	
queue.o: queue.h
	$(CXX) $(CFLAGS) -c -o $(BUILDIR)/queue.o queue.cpp -std=c++11 -I.

background.o: background.h skiplist.h search.h
	$(CXX) $(CFLAGS) -c -o $(BUILDIR)/background.o background.cpp -std=c++11 -I.

intset.o: intset.h nohotspot_ops.h search.h
	$(CXX) $(CFLAGS) -c -o $(BUILDIR)/intset.o intset.cpp -std=c++11 -I.
	
search.o: search.h skiplist.h queue.h 
	$(CXX) $(CFLAGS) -c -o $(BUILDIR)/search.o search.cpp -std=c++11 -I.
	
allocator.o: allocator.h
	$(CXX) $(CFLAGS) -c -o $(BUILDIR)/allocator.o allocator.cpp -std=c++11 -I -lnuma.
	
test.o: intset.h skiplist.h search.h nohotspot_ops.h background.h allocator.h
	$(CXX) $(CFLAGS) -c -o $(BUILDIR)/test.o test.cpp -std=c++11 -I.
	
main: intset.o skiplist.o search.o nohotspot_ops.o test.o background.o allocator.o queue.o
	$(CXX) $(CFLAGS) $(BUILDIR)/background.o $(BUILDIR)/queue.o $(BUILDIR)/skiplist.o $(BUILDIR)/intset.o $(BUILDIR)/nohotspot_ops.o $(BUILDIR)/search.o $(BUILDIR)/allocator.o $(BUILDIR)/test.o -o $(BINS) -std=c++11 $(LDFLAGS) -I. -lnuma
	
clean:
	-rm -f $(BINS)
