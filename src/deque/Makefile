ROOT = ../..

include $(ROOT)/common/Makefile.common

BINS = $(BINDIR)/lf-dq

.PHONY:	all clean

all:	main

queue.o: 
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/queue.o queue.c

herlihy.o: queue.h queue.o
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/herlihy.o herlihy.c

deque.o: queue.h herlihy.h 
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/deque.o deque.c

test.o: queue.h herlihy.h deque.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test.o test.c

main: queue.o herlihy.o deque.o test.o $(TMILB)
	$(CC) $(CFLAGS) $(BUILDIR)/queue.o $(BUILDIR)/herlihy.o $(BUILDIR)/deque.o $(BUILDIR)/test.o -o $(BINS) $(LDFLAGS)

clean:
	rm -f $(BINS)
