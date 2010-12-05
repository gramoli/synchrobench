ROOT = ..

include $(ROOT)/common/Makefile.common

BINS = lf-dq

.PHONY:	all clean

all:	main

queue.o: 
	$(CC) $(CFLAGS) -c -o queue.o queue.c

herlihy.o: queue.h queue.o
	$(CC) $(CFLAGS) -c -o herlihy.o herlihy.c

deque.o: queue.h herlihy.h 
	$(CC) $(CFLAGS) -c -o deque.o deque.c

test.o: queue.h herlihy.h deque.h
	$(CC) $(CFLAGS) -c -o test.o test.c

main: queue.o herlihy.o deque.o test.o $(TMILB)
	$(CC) $(CFLAGS) queue.o herlihy.o deque.o test.o -o lf-dq $(LDFLAGS)

clean:
	rm -f $(BINS) *.o
