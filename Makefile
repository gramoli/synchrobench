.PHONY:	all

BENCHS = src/linkedlists/lockfree-list 
LBENCHS = src/linkedlists/lazy-list 
LFBENCHS = src/linkedlists/lockfree-list 

# Only compile C11/GNU11 algorithms with compatible compiler
GCC_GTEQ_490 := $(shell expr `gcc -dumpversion | sed -e 's/\.\([0-9][0-9]\)/\1/g' -e 's/\.\([0-9]\)/0\1/g' -e 's/^[0-9]\{3,4\}$$/&00/'` \>= 40900)
ifeq "$(GCC_GTEQ_490)" "1"
	BENCHS +=
endif

#MAKEFLAGS+=-j4

MALLOC=TC

.PHONY:	clean all $(BENCHS) $(LBENCHS)

all:	lock spinlock lockfree estm sequential

lock: clean-build
	$(MAKE) "LOCK=MUTEX" $(LBENCHS)

spinlock: clean-build
	$(MAKE) "LOCK=SPIN" $(LBENCHS)

sequential: clean-build
	$(MAKE) "STM=SEQUENTIAL" $(BENCHS)

lockfree: clean-build
	for dir in $(LFBENCHS); do \
	$(MAKE) "STM=LOCKFREE" -C $$dir; \
	done

estm: clean-build
	$(MAKE) -C src/utils/estm-0.3.0
	$(MAKE) "STM=ESTM" $(BENCHS)

clean-build: 
	rm -rf build

clean:
	$(MAKE) -C src/utils/estm-0.3.0 clean
	rm -rf build bin

$(BENCHS):
	$(MAKE) -C $@ $(TARGET)

$(LBENCHS):
	$(MAKE) -C $@ $(TARGET)
