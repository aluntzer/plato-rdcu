CC               = sparc-elf-gcc
SOURCEDIR	 = lib
INCLUDEDIR       = include
BUILDDIR         = ./
PATH            +=
CFLAGS          := -O2 -mv8 -W -Wall -Wextra -std=gnu89  -Werror -Wno-missing-field-initializers # -pedantic -pedantic-errors #-Wconversion #-Wno-unused
CPPFLAGS        := -I$(INCLUDEDIR) -Iinclude/leon -I. -DNO_IASW
LDFLAGS         :=
SOURCES         := $(wildcard *.c)\
		   $(SOURCEDIR)/grspw2.c \
		   $(SOURCEDIR)/sysctl.c \
		   $(SOURCEDIR)/rmap.c \
		   $(SOURCEDIR)/rdcu_ctrl.c \
		   $(SOURCEDIR)/rdcu_cmd.c \
		   $(SOURCEDIR)/rdcu_rmap.c \
		   $(SOURCEDIR)/cmp_support.c \
		   $(SOURCEDIR)/cmp_data_types.c \
		   $(SOURCEDIR)/cmp_entity.c \
		   $(SOURCEDIR)/cmp_rdcu.c \
		   $(SOURCEDIR)/cmp_icu.c \
		   $(SOURCEDIR)/decmp.c \
		   $(SOURCEDIR)/gr718b_rmap.c \
		   $(SOURCEDIR)/leon3_grtimer.c \
		   $(SOURCEDIR)/leon3_grtimer_longcount.c \
		   $(SOURCEDIR)/irq_dispatch.c

OBJECTS         := $(patsubst %.c, $(BUILDDIR)/%.o, $(subst $(SOURCEDIR)/,, $(SOURCES)))
TARGET          := demo_plato_rdcu

DEBUG?=1
ifeq  "$(shell expr $(DEBUG) \> 1)" "1"
	    CFLAGS += -DDEBUGLEVEL=$(DEBUG)
else
	    CFLAGS += -DDEBUGLEVEL=1
endif


#all: builddir $(OBJECTS) $(BUILDDIR)/$(TARGET)
#	$(CC) $(CPPFLAGS) $(CFLAGS)  $< -o $@
#
#builddir:
#	mkdir -p $(BUILDDIR)
#
#clean:
#	 rm -f $(BUILDDIR)/{$(TARGET), $(OBJECTS)}
#	 rm -rf $(BUILDDIR)
#
#
#$(BUILDDIR)/$(TARGET): $(OBJECTS)
#	$(CC) $^ -o $@
#
#$(OBJECTS): $(SOURCES)
#	$(CC) $(CPPFLAGS) $(CFLAGS) -c $^ -o $@

all: $(SOURCES)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $(TARGET)


