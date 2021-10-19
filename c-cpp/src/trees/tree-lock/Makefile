ROOT = ../../..

include $(ROOT)/common/Makefile.common

BINS = $(BINDIR)/$(LOCK)-RCU-tree
CFLAGS += -std=gnu89

.PHONY:	all clean

all:	main

new_urcu.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/new_urcu.o new_urcu.c

citrus.o: urcu.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/citrus.o citrus.c

test.o: citrus.h urcu.h
	$(CC) $(CFLAGS) -L. -c -o $(BUILDIR)/test.o test.c

main: new_urcu.o citrus.o test.o urcu.h
	$(CC) $(CFLAGS) $(BUILDIR)/new_urcu.o $(BUILDIR)/citrus.o $(BUILDIR)/test.o -o $(BINS) $(LDFLAGS)

clean:
	-rm -f $(BINS)
