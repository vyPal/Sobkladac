SRC_DIR := src
TEST_DIR := tests
OBJ_DIR := obj

CC := gcc
STD := -std=c11
CFLAGS := $(STD) -Wall -Wextra -pedantic -O2 -I$(SRC_DIR)
LDFLAGS := -lm
TARGET := ifj26

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

LIB_OBJS := $(filter-out $(OBJ_DIR)/main.o, $(OBJS))
TEST_SRCS := $(wildcard $(TEST_DIR)/test_*.c)
TEST_BINS := $(TEST_SRCS:$(TEST_DIR)/%.c=$(OBJ_DIR)/%)

DEPS := $(OBJS:.o=.d) $(TEST_BINS:=.d)

DEV_CFLAGS := $(STD) -Wall -Wextra -pedantic -Werror -g -O0 -I$(SRC_DIR)
ASAN_FLAGS := -fsanitize=address,undefined -fno-omit-frame-pointer
TEST_FLAGS := --report-error

FORMAT_FILES := $(wildcard $(SRC_DIR)/*.c $(SRC_DIR)/*.h $(TEST_DIR)/*.c $(TEST_DIR)/*.h)

.PHONY: all build dev asan test-build test test-asan check format format-check tidy cppcheck build-check clean check-all

all: build

build: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

test-build: $(TEST_BINS)

$(TEST_BINS): $(OBJ_DIR)/%: $(OBJ_DIR)/%.o $(LIB_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

test: test-build
	@fail=0; for t in $(TEST_BINS); do ./$$t $(TEST_FLAGS) || fail=1; done; exit $$fail

test-asan:
	$(MAKE) clean
	$(MAKE) test CFLAGS="$(DEV_CFLAGS) $(ASAN_FLAGS)" LDFLAGS="$(ASAN_FLAGS)"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/%.o: $(TEST_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR):
	mkdir -p $@

-include $(DEPS)

dev:
	$(MAKE) clean
	$(MAKE) build CFLAGS="$(DEV_CFLAGS)"

asan:
	$(MAKE) clean
	$(MAKE) build CFLAGS="$(DEV_CFLAGS) $(ASAN_FLAGS)" LDFLAGS="$(ASAN_FLAGS)"

format:
	clang-format -i $(FORMAT_FILES)

format-check:
	clang-format --dry-run --Werror $(FORMAT_FILES)

cppcheck:
	cppcheck --enable=warning,style,performance,portability --error-exitcode=1 --inline-suppr --quiet $(SRC_DIR)

tidy:
	clang-tidy --quiet $(SRCS) -- $(STD) -I$(SRC_DIR)

check: format-check cppcheck

build-check: check dev

check-all: build-check tidy asan

clean:
	rm -rf $(OBJ_DIR) $(TARGET)
