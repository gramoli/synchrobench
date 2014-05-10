ROOT = ../../..

include $(ROOT)/common/Makefile.common

BINS = $(BINDIR)/lockfree-skiplist-fraser

ARCH      := INTEL
DEBUGGING := -DNDEBUG
INCLUDE   := -I../../include/

#CC          := gcc $(INCLUDE)
CFLAGS      += -DINTEL -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -Wno-format #-fomit-frame-pointer
#LDFLAGS     := -lpthread

#CFLAGS      += $(DEBUGGING)
COMMON_DEPS += Makefile $(wildcard *.h)

GC_HARNESS_TARGETS := skip_lock_perlist skip_lock_pernode skip_lock_perpointer
GC_HARNESS_TARGETS += skip_cas skip_mcas

GC_HARNESS_TARGETS += bst_lock_fraser bst_lock_manber bst_lock_kung
GC_HARNESS_TARGETS += bst_mcas

GC_HARNESS_TARGETS += rb_lock_concurrentwriters rb_lock_serialisedwriters
GC_HARNESS_TARGETS += rb_lock_mutex

TARGETS    := $(GC_HARNESS_TARGETS)
TARGETS    += rb_stm_fraser rb_stm_herlihy rb_stm_lock
TARGETS    += skip_stm_fraser skip_stm_herlihy skip_stm_lock

all: main cleanbuild

main: intset.o ptst.h set.h skip_cas.o gc.o ptst.o portable_defns.h intel_defns.h intset.h
	$(CC) $(CFLAGS) intset.o gc.o ptst.o skip_cas.o test.c -o $(BINS) $(LDFLAGS)

cleanbuild:
	rm -f *~ core *.o *.a

clean:
	rm -f *~ core *.o *.a
	rm -f $(BINS)

%.o: %.c $(COMMON_DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

$(GC_HARNESS_TARGETS): %: %.o set_harness.o ptst.o gc.o
	$(CC) -o $@ $^ $(LDFLAGS)
