ROOT = ../../..

include $(ROOT)/common/Makefile.common

ifeq ($(STM),SEQUENTIAL)
  BINS = $(BINDIR)/sequential-linkedlist
else ifeq ($(STM),LOCKFREE)
  BINS = $(BINDIR)/lockfree-linkedlist
else
  BINS = $(BINDIR)/$(STM)-linkedlist
endif

CFLAGS += -Wall -pedantic -std=gnu11
#LDFLAGS += -ltcmalloc

.PHONY:	all clean

all: selfish fomitchev

selfish: selfish.o test.c
	$(CC) $(CFLAGS) -DSELFISH selfish.o test.c -o $(BINDIR)/lockfree-selfishlist $(LDFLAGS)

fomitchev: fomitchev.o test.c
	$(CC) $(CFLAGS) -DFOMITCHEV fomitchev.o test.c -o $(BINDIR)/lockfree-fomitchevlist $(LDFLAGS)

clean:	
	-rm -f *.o $(BINDIR)/selfish-list $(BINDIR)/fomitchev-list 
