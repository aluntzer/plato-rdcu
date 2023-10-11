# verbose mode (print commands) on V=1 or VERBOSE=1
Q = $(if $(filter 1,$(V) $(VERBOSE)),,@)

LIBDIR      = lib
TESTDIR     = test
EXAMPLESDIR = examples
TESTDIR     = test

# Define nul output
VOID = /dev/null

# default target (when runing `make` with no argument)
lib-release:

.PHONY: all
all: lib examples testsbuild

.PHONY: lib lib-release
lib lib-release:
	$(Q)$(MAKE) -C $(LIBDIR) $@

## examples: build all examples in `examples/` directory
.PHONY: examples
examples:
	$(Q)$(MAKE) -C $(EXAMPLESDIR) all

## tests: build all tests in `test/` directory
.PHONY: testsbuild
testsbuild:
	$(Q)$(MAKE) -C $(TESTDIR) all

.PHONY: test
test:
	$(Q)$(MAKE) -C $(TESTDIR) test

.PHONY: coverage
coverage:
	$(Q)$(MAKE) -C $(TESTDIR) coverage


.PHONY: sparcbuild
sparcbuild: clean
	sparc-elf-gcc -v
	CC=sparc-elf-gcc CFLAGS="-mv8 -Wno-long-long \
		 -Wno-missing-field-initializers" $(MAKE) all


.PHONY: clean
clean:
	$(Q)$(MAKE) -C $(EXAMPLESDIR) $@ > $(VOID)
	$(Q)$(MAKE) -C $(TESTDIR) $@ > $(VOID)
	@echo Cleaning completed


