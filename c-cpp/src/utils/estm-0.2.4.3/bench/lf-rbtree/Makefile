ROOT = ../..

include $(ROOT)/Makefile.in

BINS = lf-rt

.PHONY:	all clean

all: main

lf-rt.o: intset.c
	$(CC) $(CFLAGS) $(DEFINES) -c -o $@ $<

$(BINS):	%:	%.o $(TMLIB)
	$(CC) -o intset-rb-normal $< $(LDFLAGS)

main: lf-rt.o
	$(CC) lf-rt.o -o lf-rt $(LDFLAGS)

clean:
	rm -f $(BINS) *.o
