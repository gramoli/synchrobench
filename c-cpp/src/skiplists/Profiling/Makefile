ROOT = ../../..

include $(ROOT)/common/Makefile.common

BINS = $(BINDIR)/lazy-numa
EXTRA_PATHS = -I./Architecture -I../../atomic_ops
EXTRA_FLAGS = -lpthread -g
.PHONY:	all clean

all:	main

Allocator.o: ./Architecture/Allocator.h
	$(CC) $(CFLAGS) $(EXTRA_PATHS) -c -o $(BUILDIR)/Allocator.o ./Architecture/Allocator.c

Nodes.o: ./Architecture/Nodes.h ./Architecture/Allocator.h
	$(CC) $(CFLAGS) $(EXTRA_PATHS) $(EXTRA_FLAGS) -c -o $(BUILDIR)/Nodes.o ./Architecture/Nodes.c

JobQueue.o: ./Architecture/JobQueue.h ./Architecture/Nodes.h ./Architecture/Allocator.h
	$(CC) $(CFLAGS) $(EXTRA_PATHS) $(EXTRA_FLAGS) -c -o $(BUILDIR)/JobQueue.o ./Architecture/JobQueue.c

SkipListLazyLock.o: SkipListLazyLock.h ./Architecture/Nodes.h
	$(CC) $(CFLAGS) $(EXTRA_PATHS) $(EXTRA_FLAGS) -c -o $(BUILDIR)/SkipListLazyLock.o SkipListLazyLock.c

SearchLayer.o: SearchLayer.h ./Architecture/Nodes.h ./Architecture/JobQueue.h
	$(CC) $(CFLAGS) $(EXTRA_PATHS) $(EXTRA_FLAGS) -c -o $(BUILDIR)/SearchLayer.o SearchLayer.c

DataLayer.o: DataLayer.h SearchLayer.h ./Architecture/Nodes.h ./Architecture/JobQueue.h
	$(CC) $(CFLAGS) $(EXTRA_PATHS) $(EXTRA_FLAGS) -c -o $(BUILDIR)/DataLayer.o DataLayer.c

Set.o: Set.h DataLayer.h ./Architecture/Allocator.h
	$(CC) $(CFLAGS) $(EXTRA_PATHS) $(EXTRA_FLAGS) -c -o $(BUILDIR)/Set.o Set.c

test.o: test.c Set.h
	$(CC) $(CFLAGS) $(EXTRA_PATHS) $(EXTRA_FLAGS) -c -o $(BUILDIR)/test.o test.c
	
main: Allocator.o Nodes.o JobQueue.o SkipListLazyLock.o SearchLayer.o DataLayer.o Set.o test.o
	$(CC) $(CFLAGS) $(EXTRA_PATHS) $(EXTRA_FLAGS) $(BUILDIR)/Allocator.o $(BUILDIR)/Nodes.o $(BUILDIR)/JobQueue.o $(BUILDIR)/SkipListLazyLock.o $(BUILDIR)/SearchLayer.o $(BUILDIR)/DataLayer.o $(BUILDIR)/Set.o $(BUILDIR)/test.o -o $(BINS) $(LDFLAGS) -lnuma
	
clean:
	-rm -f $(BINS)
