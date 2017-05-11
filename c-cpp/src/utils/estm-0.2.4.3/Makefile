include Makefile.in

# The only tested configuration share the parameter of 
# the default configuration of TinySTM-0.9.9.
# Changing these parameters may produce unexpected errors.
#DEFINES += -DDESIGN=WRITE_THROUGH
DEFINES += -DDESIGN=WRITE_BACK_ETL
DEFINES += -DCM=CM_SUICIDE
#DEFINES += -DINTERNAL_STATS
#DEFINES += -DROLLOVER_CLOCK
#DEFINES += -DCLOCK_IN_CACHE_LINE
#DEFINES += -UNO_DUPLICATES_IN_RW_SETS
#DEFINES += -UWAIT_YIELD
#DEFINES += -UUSE_BLOOM_FILTER
DEFINES += -DEPOCH_GC
#DEFINES += -UCONFLICT_TRACKING
#DEFINES += -UREAD_LOCKED_DATA
#DEFINES += -ULOCK_IDX_SWAP
#DEFINES += -UDEBUG
#DEFINES += -UDEBUG2

# Replace textual values by constants for unifdef...
#D := $(DEFINES)
#D := $(D:WRITE_BACK_ETL=0)
#D := $(D:WRITE_BACK_CTL=1)
#D := $(D:WRITE_THROUGH=2)
#D += -DWRITE_BACK_ETL=0 -DWRITE_BACK_CTL=1 -DWRITE_THROUGH=2
#D := $(D:CM_SUICIDE=0)
#D := $(D:CM_DELAY=1)
#D := $(D:CM_BACKOFF=2)
#D := $(D:CM_PRIORITY=3)
#D += -DCM_SUICIDE=0 -DCM_DELAY=1 -DCM_BACKOFF=2 -DCM_PRIORITY=3

ifneq (,$(findstring -DEPOCH_GC,$(DEFINES)))
 GC := $(SRCDIR)/gc.o
else
 GC :=
endif

#GC := $(SRCDIR)/gc.o
CFLAGS += -I$(SRCDIR)
CFLAGS += $(DEFINES)

MODULES := $(patsubst %.c,%.o,$(wildcard $(SRCDIR)/mod_*.c))

.PHONY:	all doc bench clean

all:	$(TMLIB)

%.o:	%.c
	$(CC) $(CFLAGS) -DCOMPILE_FLAGS="$(CFLAGS)" -c -o $@ $<

%.s:	%.c
	$(CC) $(CFLAGS) -DCOMPILE_FLAGS="$(CFLAGS)" -fverbose-asm -S -o $@ $<

%.o.c:	%.c
	unifdef $(D) $< > $@ || true

$(TMLIB):	$(SRCDIR)/$(TM).o $(SRCDIR)/wrappers.o $(GC) $(MODULES)
	@mkdir -p lib
	$(AR) cru $@ $^

bench:	$(TMLIB)
	$(MAKE) -C bench

doc:
	$(DOXYGEN)

clean:
	rm -f $(TMLIB) $(SRCDIR)/*.o
	TARGET=clean $(MAKE) -C bench
