ROOT = ../..

include $(ROOT)/Makefile.in

BINS = lf-sl

.PHONY:	all clean

all:	main

intset.o:
	$(CC) $(CFLAGS) -c -o intset.o intset.c

lf-sl.o: intset.o
	$(CC) $(CFLAGS) -c -o lf-sl.o lf-sl.c 

main: intset.o lf-sl.o $(TMLIB)
	$(CC) $(CFLAGS) lf-sl.o -o lf-sl $(LDFLAGS)

clean:
	rm -f $(BINS) *.o
