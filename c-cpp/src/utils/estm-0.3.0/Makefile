include Makefile.in

CFLAGS += -std=gnu89 -Wno-sequence-point
CFLAGS += -I$(SRCDIR) 
CFLAGS += $(DEFINES) 
MODULES := $(patsubst %.c,%.o,$(wildcard $(SRCDIR)/mod_*.c))

.PHONY:	all clean

all:	$(TMLIB)

$(TMLIB):	$(SRCDIR)/$(TM).o $(SRCDIR)/wrappers.o $(SRCDIR)/gc.o $(MODULES)
	@mkdir -p lib
	$(AR) cru $@ $^

clean:
	rm -f $(TMLIB) $(SRCDIR)/*.o
