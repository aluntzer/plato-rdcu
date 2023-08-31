CC_SPARK  = sparc-elf-gcc
CC_SPARK_AVAILABLE := $(shell command -v $(CC_SPARK) 2> /dev/null)
MOREFLAGS_SPARC = -O2 -mv8 -Wno-long-long -Wno-missing-field-initializers
CPPFLAGS += -DNO_IASW -I$(LIBDIR)
DEBUGFLAGS= -Wall -Wextra -pedantic -Wcast-qual -Wshadow \
            -Wstrict-aliasing=1 -Wdeclaration-after-statement \
            -Wstrict-prototypes -Wpointer-arith \
            -Wvla -Wformat=2 -Winit-self -Wfloat-equal -Wwrite-strings \
            #-Wcast-align -Wredundant-decls -Wmissing-prototypes -Wundef -Wswitch-enum
CFLAGS   += $(DEBUGFLAGS) $(MOREFLAGS)
LIBDIR    = ../lib
LIB       = $(LIBDIR)/libcmp.a

SUBDIR   := ./leon
vpath %.c $(SUBDIR)

LEON_SRC := $(sort $(wildcard leon/*.c))
LEON_SRC_LOCAL := $(notdir $(LEON_SRC))
LEON_OBJ := $(LEON_SRC_LOCAL:.c=.o)

.PHONY: default
default: all

.PHONY: all
all: demo_plato_rdcu_sparc

.PHONY: lib
lib:
	CC=$(CC) MOREFLAGS="$(MOREFLAGS)" $(MAKE) -C $(LIBDIR) $@


.PHONY: $(LIB) # must be run every time
$(LIB) :
	$(MAKE) -C $(LIBDIR) libcmp.a


demo_plato_rdcu.o: demo.h cfg.h
demo_plato_rdcu: demo_plato_rdcu.o $(LEON_OBJ) $(LIB)



.PHONY: demo_plato_rdcu_sparc
demo_plato_rdcu_sparc:
ifndef CC_SPARK_AVAILABLE
	$(error $(CC_SPARK) is not available)
endif
	CC=$(CC_SPARK) MOREFLAGS="$(MOREFLAGS_SPARC)" $(MAKE) demo_plato_rdcu


.PHONY: clean
clean:
	@$(RM) *.o demo_plato_rdcu
	$(MAKE) -C $(LIBDIR) clean
	@echo Cleaning completed
