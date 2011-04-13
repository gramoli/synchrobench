.PHONY:	all

BENCHS = linkedlist hashtable skiplist rbtree #deque
LBENCHS = linkedlist-lock hashtable-lock skiplist-lock

.PHONY:	clean all $(BENCHS) $(LBENCHS)

all:	lock

lock:
	$(MAKE) "LOCK=MUTEX" $(LBENCHS)

spinlock:
	$(MAKE) "LOCK=SPIN" $(LBENCHS)

sequential:
	$(MAKE) "STM=SEQUENTIAL" $(BENCHS)

lockfree:
	$(MAKE) "STM=LOCKFREE" $(BENCHS)

estm: 
	$(MAKE) "STM=ESTM" $(BENCHS)

tinystm:
	$(MAKE) "STM=TINYSTM" $(BENCHS)

tiny098:
	$(MAKE) "STM=TINY098" $(BENCHS)

tl2:
	$(MAKE) "STM=TL2" $(BENCHS)

herlihy:
	$(MAKE) "STM=LOCKFREE" -C deque

herlihytiny:
	$(MAKE) "STM=TINYSTM" -C deque

herlihyestm:
	$(MAKE) "STM=ESTM" -C deque

herlihywlpdstm:
	$(MAKE) "STM=WLPDSTM" -C deque

herlihyseq:
	$(MAKE) "STM=SEQUENTIAL" -C deque

# transactional boosting (xb), aggressive (axb), with work stealing (axbs)

xb:
	$(MAKE) "STM=XB" -C rbtree-boosted

axb:
	$(MAKE) "STM=AXB" -C rbtree-boosted

axbs:
	$(MAKE) "STM=AXBS" -C rbtree-boosted

wlpdstm:
	$(MAKE) "STM=WLPDSTM" $(BENCHS)

wlpdstm_qui:
	$(MAKE) "STM=WLPDSTM" $(BENCHS)

icc:
	$(MAKE) "STM=ICC" $(BENCHS)

tanger: 
	$(MAKE) "STM=TANGER" $(BENCHS)

clean:
	$(MAKE) -C linkedlist clean	
	$(MAKE) -C skiplist clean
	$(MAKE) -C hashtable clean
	$(MAKE) -C rbtree clean
	$(MAKE) -C linkedlist-lock clean
	$(MAKE) -C hashtable-lock clean
	$(MAKE) -C skiplist-lock clean
	$(MAKE) -C deque clean
#	$(MAKE) -C rbtree-boosted clean

$(BENCHS):
	$(MAKE) -C $@ $(TARGET)

$(LBENCHS):
	$(MAKE) -C $@ $(TARGET)
