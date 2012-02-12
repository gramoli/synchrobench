.PHONY:	all


BENCHS = src/sftree src/linkedlist src/hashtable src/skiplist src/rbtree src/deque
LBENCHS = src/linkedlist-lock src/hashtable-lock src/skiplist-lock
LFBENCHS = src/linkedlist src/hashtable src/skiplist 

.PHONY:	clean all $(BENCHS) $(LBENCHS)

all:	lock

lock:
	$(MAKE) "LOCK=MUTEX" $(LBENCHS)

spinlock:
	$(MAKE) "LOCK=SPIN" $(LBENCHS)

sequential:
	$(MAKE) "STM=SEQUENTIAL" $(BENCHS)

lockfree:
	$(MAKE) "STM=LOCKFREE" $(LFBENCHS)

estm:
	$(MAKE) "STM=ESTM" $(BENCHS)

tinystm:
	$(MAKE) "STM=TINY100" $(BENCHS)

tiny100:
	$(MAKE) "STM=TINY100" $(BENCHS)

tiny10B:
	$(MAKE) "STM=TINY10B" $(BENCHS)

tiny099:
	$(MAKE) "STM=TINY099" $(BENCHS)

tiny098:
	$(MAKE) "STM=TINY098" $(BENCHS)

tl2:
	$(MAKE) "STM=TL2" $(BENCHS)

herlihy:
	$(MAKE) "STM=LOCKFREE" -C src/deque

herlihytiny:
	$(MAKE) "STM=TINYSTM" -C src/deque

herlihyestm:
	$(MAKE) "STM=ESTM" -C src/deque

herlihywlpdstm:
	$(MAKE) "STM=WLPDSTM" -C src/deque

herlihyseq:
	$(MAKE) "STM=SEQUENTIAL" -C src/deque

# transactional boosting (xb), aggressive (axb), with work stealing (axbs)

xb:
	$(MAKE) "STM=XB" -C src/rbtree-boosted

axb:
	$(MAKE) "STM=AXB" -C src/rbtree-boosted

axbs:
	$(MAKE) "STM=AXBS" -C src/rbtree-boosted

wlpdstm:
	$(MAKE) "STM=WLPDSTM" $(BENCHS)

wlpdstm_qui:
	$(MAKE) "STM=WLPDSTM" $(BENCHS)

icc:
	$(MAKE) "STM=ICC" $(BENCHS)

tanger: 
	$(MAKE) "STM=TANGER" $(BENCHS)

clean:
	$(MAKE) -C src/linkedlist clean	
	$(MAKE) -C src/skiplist clean
	$(MAKE) -C src/hashtable clean
	$(MAKE) -C src/rbtree clean
	$(MAKE) -C src/linkedlist-lock clean
	$(MAKE) -C src/hashtable-lock clean
	$(MAKE) -C src/skiplist-lock clean
	$(MAKE) -C src/sftree clean
	$(MAKE) -C src/deque clean
	rm -rf build
#	$(MAKE) -C rbtree-boosted clean

$(BENCHS):
	$(MAKE) -C $@ $(TARGET)

$(LBENCHS):
	$(MAKE) -C $@ $(TARGET)
