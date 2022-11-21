.SECONDEXPANSION:
# tools
CC        := gcc
LCOV      := lcov
GENHTML   := genhtml
# directories
SRCS_DIR  := lib
INC_DIR   := include
TEST_DIR  := test
BUILD_DIR := build
UNITY_DIR := $(TEST_DIR)/Unity/src
# flags
CPPFLAGS  := -DNO_IASW -DDEBUGLEVEL=1 -I. -I$(INC_DIR) -I$(INC_DIR)/leon -I$(UNITY_DIR)
CFLAGS    := -O2 -Wall -Wextra -std=gnu99 -Werror -pedantic \
             -pedantic-errors -Wno-long-long# -Wconversion
LDFLAGS   :=


TESTS := cmp_data_types cmp_icu cmp_entity cmp_decmp cmp_rdcu_cfg
# units under test
UUT_SRCS := $(SRCS_DIR)/cmp_icu.c \
            $(SRCS_DIR)/cmp_support.c \
            $(SRCS_DIR)/cmp_entity.c \
            $(SRCS_DIR)/cmp_data_types.c \
            $(SRCS_DIR)/cmp_rdcu_cfg.c \
            $(TEST_DIR)/cmp_decmp/decmp.c

TARGET = demo_plato_rdcu



TESTS_SUB_DIRS := $(addprefix $(TEST_DIR)/, $(TESTS))
TESTS_SUB_DIRS += $(UNITY_DIR)
DIRS := $(SRCS_DIR) $(TESTS_SUB_DIRS)
BUILD_SUB_DIRS := $(addprefix $(BUILD_DIR)/, $(DIRS))
BUILD_SUB_DIRS += $(BUILD_DIR)


LIB_SRCS := $(wildcard $(SRCS_DIR)/*.c)
LIB_OBJS := $(addprefix $(BUILD_DIR)/, $(LIB_SRCS:.c=.o))

HEADERS  := $(wildcard $(INC_DIR)/*.h)

UNITY_SRCS := $(wildcard $(UNITY_DIR)/*.c)
UNITY_OBJS = $(addprefix $(BUILD_DIR)/, $(UNITY_SRCS:.c=.o))

UUT_OBJS := $(addprefix $(BUILD_DIR)/, $(UUT_SRCS:.c=.o))

TESTS_SRCS := $(shell find $(addsuffix /*.c, $(TESTS_SUB_DIRS)))
TEST_OBJS := $(addprefix $(BUILD_DIR)/, $(TESTS_SRCS:.c=.o))


.PHONY: all
all: $(TARGET)

.PHONY: $(TARGET)
$(TARGET): CC = sparc-elf-gcc
$(TARGET): CFLAGS += -mv8
$(TARGET): $(BUILD_DIR)/$(TARGET)

$(BUILD_DIR)/$(TARGET): $(BUILD_DIR)/$(TARGET).o $(LIB_OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

.PHONY: coverage
coverage: $(BUILD_DIR)/coverage

TESTS_RESULTS := $(addprefix $(BUILD_DIR)/test_, $(addsuffix .txt, $(TESTS)))
$(BUILD_DIR)/coverage: CFLAGS  += -O0 -fprofile-arcs -ftest-coverage --coverage
$(BUILD_DIR)/coverage: LDFLAGS += --coverage
$(BUILD_DIR)/coverage: $(TESTS_RESULTS)

	@echo
	@echo '*'
	@echo '* Generating HTML output'
	@echo '*'
	@echo
	$(GENHTML) $(BUILD_DIR)/*.info --output-directory $@ --show-details\
		        --title "Compression Unit Tests" --branch-coverage --legend
	@echo
	@echo '*'
	@echo '* See: '$(CURDIR)/$(BUILD_DIR)/coverage/index.html
	@echo '*'
	@echo

PASSED = `grep -s PASS $(BUILD_DIR)/*.txt`
FAIL = `grep -s FAIL $(BUILD_DIR)/*.txt`
IGNORE = `grep -s IGNORE $(BUILD_DIR)/*.txt`

.PHONY: test
test: $(TESTS_RESULTS)
	@echo "-----------------------\nIGNORES:\n-----------------------"
	@echo "$(IGNORE)"
	@echo "-----------------------\nFAILURES:\n-----------------------"
	@echo "$(FAIL)"
	@echo "-----------------------\nPASSED:\n-----------------------"
	@echo "$(PASSED)"
	@echo "DONE"

$(BUILD_DIR)/%.txt: $(BUILD_DIR)/%
	$(LCOV) --zerocounters --directory .
	./$< > $@ 2>&1
	$(LCOV) --capture --directory . --output-file $^.info --test-name $* --rc lcov_branch_coverage=1 --no-external

# test_cmp_icu.c includes the cmp_icu.c file therefore the cmp_icu object file is excluded
$(BUILD_DIR)/test_cmp_icu: $(BUILD_DIR)/$(TEST_DIR)/cmp_icu/test_cmp_icu_Runner.o $(BUILD_DIR)/$(TEST_DIR)/cmp_icu/test_cmp_icu.o \
                           $(filter-out %cmp_icu.o,$(UUT_OBJS)) $(UNITY_OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

$(BUILD_DIR)/test_%: $(BUILD_DIR)/test/$$*/test_$$*_Runner.o $(BUILD_DIR)/test/$$*/test_$$*.o \
                     $(UUT_OBJS) $(UNITY_OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

TEST_EXECUTABLES := $(addprefix $(BUILD_DIR)/test_, $(TESTS))
.SECONDARY: $(TEST_EXECUTABLES)

# create build directories for the object files
$(LIB_OBJS): | $(BUILD_DIR)/$(SRCS_DIR)
$(BUILD_DIR)/$(TARGET).o: | $(BUILD_DIR)
$(TEST_OBJS): | $(addprefix $(BUILD_DIR)/, $(TESTS_SUB_DIRS))

$(BUILD_DIR)/%.o : %.c $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_SUB_DIRS):
	mkdir -p $@

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
