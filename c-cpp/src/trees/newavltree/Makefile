ROOT = ../../..

include $(ROOT)/common/Makefile.common

BINS = newlf-at newlf-atmove newlf-atnest

.PHONY:	all clean

all:	main main_move

newavltree.o:
	$(CC) $(CFLAGS) -c -o newavltree.o newavltree.c

intset.o: newavltree.h
	$(CC) $(CFLAGS) -c -o intset.o intset.c

test.o: newavltree.h intset.h
	$(CC) $(CFLAGS) -c -o test.o test.c

main: newavltree.o intset.o test.o 
	$(CC) $(CFLAGS) newavltree.o intset.o test.o -o newlf-at $(LDFLAGS)

move_test.o: newavltree.h intset.h
	$(CC) $(CFLAGS) -c -o move_test.o move_test.c

main_move: newavltree.o intset.o move_test.o 
	$(CC) $(CFLAGS) newavltree.o intset.o move_test.o -o newlf-atmove $(LDFLAGS)

clean:
	-rm -f $(BINS) *.o
