DEBUGLEVEL ?= 3
CPPFLAGS  := -DDEBUGLEVEL=$(DEBUGLEVEL)
DEBUGFLAGS= -Wall -Wextra -pedantic -Wcast-qual -Wshadow \
            -Wstrict-aliasing=1 -Wdeclaration-after-statement \
            -Wstrict-prototypes -Wpointer-arith \
            -Wformat=2 -Winit-self -Wfloat-equal -Wwrite-strings \
            -Wno-missing-field-initializers -Wno-missing-braces \
            -Wcast-align -Wredundant-decls
CFLAGS   += -std=gnu99 $(DEBUGFLAGS) $(MOREFLAGS)
LDFLAGS  += $(MOREFLAGS)
FLAGS     = $(CPPFLAGS) $(CFLAGS) $(LDFLAGS)
LDLIBS=-lm


ROOT_DIR  := ./..
UNITY_DIR := unity/src
TEST_SUB_DIRS = cmp_data_types cmp_entity cmp_decmp cmp_icu cmp_max_used_bits \
                cmp_rdcu_cfg decmp test_common
LIB_DIR =../lib
LIB = $(LIB_DIR)/libcmp.a

vpath %.c $(TEST_SUB_DIRS) $(UNITY_DIR)

UNITY_OBJS = unity.o
TEST_COMMON_OBJ = test_common.o pcg_basic.o chunk_round_trip.o

CPPFLAGS  += -I$(LIB_DIR) -I$(LIB_DIR)/common -I$(LIB_DIR)/icu_compress \
             -I$(LIB_DIR)/rdcu_compress -I$(LIB_DIR)/decompress -I$(UNITY_DIR)

.PHONY: all
all: test_cmp_data_types_Runner test_cmp_entity_Runner test_cmp_decmp_Runner \
     test_cmp_icu_Runner test_cmp_rdcu_cfg_Runner \
     test_decmp_Runner

# define silent mode as default (verbose mode with V=1 or VERBOSE=1)
# Note : must be defined _after_ the default target
$(V)$(VERBOSE).SILENT:

.PHONY: test
test: test_cmp_data_types test_cmp_entity test_cmp_decmp test_cmp_icu \
      test_cmp_rdcu_cfg test_decmp


$(LIB) :
	CC=$(CC) MOREFLAGS="$(MOREFLAGS)" $(MAKE) -C $(LIB_DIR) libcmp.a


%.txt: %_Runner
	./$< > $@ 2>&1 || echo "\n\n===== TEST FAILD ====="
	@echo -e "-----------------------\n$@\n-----------------------"
	@echo -e "-----------------------\nIGNORES:\n-----------------------"
	@echo -e "`grep -s IGNORE $@`"
	@echo -e "-----------------------\nFAILURES:\n-----------------------"
	@echo -e "`grep -s FAIL $@`"
	@echo -e "-----------------------\nPASSES:\n-----------------------"
	@echo -e "`grep -s PASS $@`"


.PHONY: test_cmp_data_types
test_cmp_data_types: test_cmp_data_types.txt
test_cmp_data_types_Runner: test_cmp_data_types.o $(UNITY_OBJS) $(LIB)

.PHONY: test_cmp_entity
test_cmp_entity: test_cmp_entity.txt
test_cmp_entity_Runner: test_cmp_entity.o $(UNITY_OBJS) $(LIB)

.PHONY: test_cmp_decmp
test_cmp_decmp: test_cmp_decmp.txt
test_cmp_decmp_Runner: test_cmp_decmp.o $(UNITY_OBJS) $(TEST_COMMON_OBJ) $(LIB)

.PHONY: test_cmp_icu
test_cmp_icu: test_cmp_icu.txt
test_cmp_icu_Runner: test_cmp_icu.o $(UNITY_OBJS) $(TEST_COMMON_OBJ) $(LIB)

.PHONY: test_cmp_rdcu_cfg
test_cmp_rdcu_cfg: test_cmp_rdcu_cfg.txt
test_cmp_rdcu_cfg_Runner: test_cmp_rdcu_cfg.o $(UNITY_OBJS) $(LIB)

.PHONY: test_decmp
test_decmp: test_decmp.txt
test_decmp_Runner: test_decmp.o $(UNITY_OBJS) $(LIB)



LCOV      := lcov
GENHTML   := genhtml

.PHONY: coverage
coverage: coverage_test

coverage_%:
	$(LCOV) --zerocounters --directory $(ROOT_DIR)
	MOREFLAGS="-O0 -fprofile-arcs -ftest-coverage --coverage" LDFLAGS="--coverage" $(MAKE) $*
	$(LCOV) --capture --directory $(ROOT_DIR) --output-file $*.info --test-name $* --rc lcov_branch_coverage=1
	@echo
	@echo '*'
	@echo '* Generating HTML output'
	@echo '*'
	@echo
	$(GENHTML) $*.info --output-directory coverage --show-details\
		        --title "Compression Unit Tests" --branch-coverage --legend
	@echo
	@echo '*'
	@echo '* See: '`pwd`/coverage/index.html
	@echo '*'
	@echo


.PHONY: clean
clean:
	@$(RM) *.o *.gcda *.gcno *.info *.txt test_*_Runner
	$(MAKE) -C $(LIB_DIR) clean
	@$(RM) -r coverage
	@echo Cleaning completed
