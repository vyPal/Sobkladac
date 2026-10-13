CC := gcc
STD := -std=c11
CFLAGS := $(STD) -Wall -Wextra -pedantic -O2
LDFLAGS :=
TARGET := ifj26

SRC_DIR := src
OBJ_DIR := obj
SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

DEV_CFLAGS := $(STD) -Wall -Wextra -pedantic -Werror -g -O0
ASAN_FLAGS := -fsanitize=address,undefined -fno-omit-frame-pointer

FORMAT_FILES := $(wildcard $(SRC_DIR)/*.c $(SRC_DIR)/*.h)

.PHONY: all build dev asan check format format-check tidy cppcheck build-check clean

all: build

build: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
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
