CC_SPARK  = sparc-elf-gcc
CC_SPARK_AVAILABLE := $(shell command -v $(CC_SPARK) 2> /dev/null)
MOREFLAGS_SPARC = -O2
LIBDIR    = ../lib
CPPFLAGS += -std=gnu99 -DNO_IASW -I$(LIBDIR) -I$(LIBDIR)/common
DEBUGFLAGS= -Wall -Wextra -pedantic -Wcast-qual -Wshadow \
            -Wstrict-aliasing=1 -Wdeclaration-after-statement \
            -Wstrict-prototypes -Wpointer-arith \
            -Wformat=2 -Winit-self -Wfloat-equal -Wwrite-strings \
            #-Wcast-align -Wredundant-decls -Wmissing-prototypes -Wundef -Wswitch-enum
CFLAGS   += $(DEBUGFLAGS) $(MOREFLAGS)
LIB       = $(LIBDIR)/libcmp.a

SUBDIR   := ./leon
vpath %.c $(SUBDIR)

LEON_SRC := $(sort $(wildcard leon/*.c))
LEON_SRC_LOCAL := $(notdir $(LEON_SRC))
LEON_OBJ := $(LEON_SRC_LOCAL:.c=.o)


.PHONY: all
all: example_cmp_icu demo_plato_rdcu_sparc example_compress_chunk


.PHONY: $(LIB) # must be run every time
$(LIB) :
	$(MAKE) -C $(LIBDIR) libcmp.a


example_cmp_icu: example_cmp_icu.o $(LIB)

example_compress_chunk: example_compress_chunk.o $(LIB)


ifndef CC_SPARK_AVAILABLE
demo_plato_rdcu_sparc:
	$(warning --- $(CC_SPARK) is not available ---)
	$(warning --- Skip building $@ ---)

else
demo_plato_rdcu.o: demo.h cfg.h

demo_plato_rdcu: demo_plato_rdcu.o $(LEON_OBJ) $(LIB)

.PHONY: demo_plato_rdcu_sparc
demo_plato_rdcu_sparc:
	CC=$(CC_SPARK) MOREFLAGS="$(MOREFLAGS_SPARC)" $(MAKE) demo_plato_rdcu

endif


.PHONY: clean
clean:
	@$(RM) *.o example_cmp_icu demo_plato_rdcu example_compress_chunk
	$(MAKE) -C $(LIBDIR) clean
	@echo Cleaning completed
