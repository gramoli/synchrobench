.PHONY:	all

BENCHS = lf-linkedlist lf-skiplist lf-hashtable lf-rbtree

.PHONY:	all $(BENCHS)

all:	$(BENCHS)

clean:
	$(MAKE) -C lf-linkedlist clean	
	$(MAKE) -C lf-skiplist clean
	$(MAKE) -C lf-hashtable clean

$(BENCHS):
	$(MAKE) -C $@ $(TARGET)
