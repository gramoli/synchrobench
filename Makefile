.PHONY:	all

BENCHS = src/sftree src/linkedlist src/hashtable src/rbtree src/deque src/skiplist/sequential
LBENCHS = src/linkedlist-lock src/hashtable-lock src/skiplist-lock 
LFBENCHS = src/linkedlist src/hashtable src/skiplist/rotating src/skiplist/fraser

.PHONY:	clean all $(BENCHS) $(LBENCHS)

all:	lock lockfree sequential

lock:
	for dir in $(LBENCHS); do \
	$(MAKE) "LOCK=MUTEX" -C $$dir; \
	done

spinlock:
	for dir in $(LBENCHS); do \
	$(MAKE) "LOCK=SPIN" -C $$dir; \
	done

sequential:
	for dir in $(BENCHS); do \
	$(MAKE) "STM=SEQUENTIAL" -C $$dir; \
	done

lockfree:
	for dir in $(LFBENCHS); do \
	$(MAKE) "STM=LOCKFREE" -C $$dir; \
	done

estm:
	for dir in $(BENCHS); do \
	$(MAKE) "STM=ESTM" -C $$dir; \
	done

clean:
	rm -rf build

$(BENCHS):
	$(MAKE) -C $@ $(TARGET)

$(LBENCHS):
	$(MAKE) -C $@ $(TARGET)
