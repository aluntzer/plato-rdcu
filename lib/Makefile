DEBUGLEVEL ?= 3 #default build is with debug info
CPPFLAGS += -DDEBUGLEVEL=$(DEBUGLEVEL)
DEBUGFLAGS= -Wall -Wextra -Wcast-qual -Wshadow \
            -Wstrict-aliasing=1 -Wdeclaration-after-statement \
            -Wstrict-prototypes -Wpointer-arith \
            -Wvla -Wformat=2 -Winit-self -Wfloat-equal -Wwrite-strings \
            # -Wcast-align -Wredundant-decls -Wmissing-prototypes -Wundef -Wswitch-enum
CFLAGS   += $(DEBUGFLAGS) $(MOREFLAGS)
LDFLAGS  += $(MOREFLAGS)
FLAGS     = $(CPPFLAGS) $(CFLAGS) $(LDFLAGS)

# default target (when runing `make` with no argument)
lib-release:

# define silent mode as default (verbose mode with V=1 or VERBOSE=1)
# Note : must be defined _after_ the default target
$(V)$(VERBOSE).SILENT:

COMMON_FILES := $(sort $(wildcard common/*.c))
ICU_COMPRESS_FILES := $(sort $(wildcard icu_compress/*.c))
RDCU_COMPRESS_FILES := $(sort $(wildcard rdcu_compress/*.c))
DECOMPRESS_FILES := $(sort $(wildcard decompress/*.c))

SUBDIR := ./common ./icu_compress ./rdcu_compress ./decompress
vpath %.c $(SUBDIR)
vpath %.S $(SUBDIR)


# Modules
LIB_ICU_COMPRESSION ?= 1
LIB_RDCU_COMPRESSION ?= 1
LIB_DECOMPRESSION ?= 1

SRC_FILES := $(COMMON_FILES)

ifneq ($(LIB_ICU_COMPRESSION), 0)
  SRC_FILES += $(ICU_COMPRESS_FILES)
endif

ifneq ($(LIB_RDCU_COMPRESSION), 0)
  SRC_FILES += $(RDCU_COMPRESS_FILES)
endif

ifneq ($(LIB_DECOMPRESSION), 0)
  SRC_FILES += $(DECOMPRESS_FILES)
endif

LOCAL_SRC := $(notdir $(SRC_FILES))
LOCAL_OBJ0 := $(LOCAL_SRC:.c=.o)
LOCAL_OBJ := $(LOCAL_OBJ0:.S=.o)


lib-release: DEBUGFLAGS :=
lib-release: CFLAGS := -O2
lib-release: lib

.PHONY: all
all: lib

.PHONY: libcmp.a  # must be run every time

ifndef BUILD_DIR
# determine BUILD_DIR from compilation flags
ifeq ($(UNAME), Darwin)
  ifeq ($(shell md5 < /dev/null > /dev/null; echo $$?), 0)
    HASH ?= md5
  endif
else ifeq ($(UNAME), FreeBSD)
  HASH ?= gmd5sum
else ifeq ($(UNAME), NetBSD)
  HASH ?= md5 -n
else ifeq ($(UNAME), OpenBSD)
  HASH ?= md5
endif
HASH ?= md5sum

HASH_DIR = conf_$(shell echo $(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(ZSTD_FILES) | $(HASH) | cut -f 1 -d " " )
HAVE_HASH :=$(shell echo 1 | $(HASH) > /dev/null && echo 1 || echo 0)
ifeq ($(HAVE_HASH),0)
  $(info warning : could not find HASH ($(HASH)), needed to differentiate builds using different flags)
  BUILD_DIR := obj/generic_noconf
endif

libcmp.a: $(STATLIB)
SET_CACHE_DIRECTORY = \
   +$(MAKE) --no-print-directory $@ \
    BUILD_DIR=obj/$(HASH_DIR) \
    CC="$(CC)" \
    CPPFLAGS="$(CPPFLAGS)" \
    CFLAGS="$(CFLAGS)" \
    LDFLAGS="$(LDFLAGS)"

libcmp.a: $(SRC_FILES)
	$(SET_CACHE_DIRECTORY)

else
# BUILD_DIR is defined

STATLIB_DIR := $(BUILD_DIR)/static
STATLIB := $(STATLIB_DIR)/libcmp.a
STATLIB_OBJ := $(addprefix $(STATLIB_DIR)/,$(LOCAL_OBJ))

$(STATLIB): | $(STATLIB_DIR)
$(STATLIB): $(STATLIB_OBJ)
	@echo compiling static library
	@echo $(STATLIB_DIR)
	$(AR) rcs $@ $^

libcmp.a: $(STATLIB)
	@echo cp -f $< $@
	cp -f $< $@
endif


.PHONY: lib
lib: libcmp.a

.PHONY: lib-release
lib-release : DEBUGFLAGS :=
lib-release : lib
	@echo release build completed

# Generate .h dependencies automatically

DEPFLAGS = -MT $@ -MMD -MP -MF

$(STATLIB_DIR)/%.o : %.c $(STATLIB_DIR)/%.d | $(STATLIB_DIR)
	@echo CC $@
	$(COMPILE.c) $(DEPFLAGS) $(STATLIB_DIR)/$*.d $(OUTPUT_OPTION) $<

$(STATLIB_DIR)/%.o : %.S | $(STATLIB_DIR)
	@echo AS $@
	$(COMPILE.S) $(OUTPUT_OPTION) $<

MKDIR ?= mkdir
$(BUILD_DIR) $(STATLIB_DIR):
	$(MKDIR) -p $@

DEPFILES :=  $(STATLIB_OBJ:.o=.d)
$(DEPFILES):

include $(wildcard $(DEPFILES))


.PHONY: clean
clean:
	$(RM) core *.o
	$(RM) *.a
	$(RM) -r $(BUILD_DIR)
	$(RM) -r obj
	@echo Cleaning library completed
