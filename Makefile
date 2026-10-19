SRC_DIR := src
TEST_DIR := tests
OBJ_DIR := obj

CC := gcc
STD := -std=c11
CFLAGS := $(STD) -Wall -Wextra -pedantic -O2 -I$(SRC_DIR)
LDFLAGS :=
TARGET := ifj26
TEST_TARGET := test_ifj26

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

TEST_SRCS := $(filter-out $(SRC_DIR)/main.c, $(SRCS))
TEST_OBJS := $(TEST_SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o) $(OBJ_DIR)/unit.o

DEPS := $(OBJS:.o=.d) $(OBJ_DIR)/unit.d

DEV_CFLAGS := $(STD) -Wall -Wextra -pedantic -Werror -g -O0 -I$(SRC_DIR)
ASAN_FLAGS := -fsanitize=address,undefined -fno-omit-frame-pointer

FORMAT_FILES := $(wildcard $(SRC_DIR)/*.c $(SRC_DIR)/*.h $(TEST_DIR)/*.c $(TEST_DIR)/*.h)

.PHONY: all build dev asan test-build test test-asan check format format-check tidy cppcheck build-check clean

all: build

build: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

test-build: $(TEST_OBJS)
	$(CC) $(CFLAGS) $^ -o $(TEST_TARGET) $(LDFLAGS)

test: test-build
	./$(TEST_TARGET)

test-asan:
	$(MAKE) clean
	$(MAKE) test-build CFLAGS="$(DEV_CFLAGS) $(ASAN_FLAGS)" LDFLAGS="$(ASAN_FLAGS)"
	./$(TEST_TARGET)

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

clean:
	rm -rf $(OBJ_DIR) $(TARGET)
