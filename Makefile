SRC_DIR := src
TEST_DIR := tests
UNIT_TEST_DIR := $(TEST_DIR)/unit
E2E_TEST_DIR := $(TEST_DIR)/e2e
OBJ_DIR := obj

CC := gcc
STD := -std=c11
CFLAGS := $(STD) -Wall -Wextra -pedantic -O2 -I$(SRC_DIR)
LDFLAGS := -lm
TARGET := ifj26

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

LIB_OBJS := $(filter-out $(OBJ_DIR)/main.o, $(OBJS))
UNIT_TEST_SRCS := $(wildcard $(UNIT_TEST_DIR)/test_*.c)
UNIT_TEST_BINS := $(UNIT_TEST_SRCS:$(UNIT_TEST_DIR)/%.c=$(OBJ_DIR)/%)

DEPS := $(OBJS:.o=.d) $(UNIT_TEST_BINS:=.d)

DEV_CFLAGS := $(STD) -Wall -Wextra -pedantic -Werror -g -O0 -I$(SRC_DIR)
ASAN_FLAGS := -fsanitize=address,undefined -fno-omit-frame-pointer
TEST_FLAGS := --report-error

FORMAT_FILES := $(wildcard $(SRC_DIR)/*.c $(SRC_DIR)/*.h $(UNIT_TEST_DIR)/*.c $(UNIT_TEST_DIR)/*.h)

DOC_DIR     := docs
DOC_OUT     := dokumentace.pdf
DOC_META    := $(DOC_DIR)/metadata.yaml
DOC_SOURCES := $(sort $(wildcard $(DOC_DIR)/*.md))
DOC_IMAGES  := $(wildcard $(DOC_DIR)/*.pdf $(DOC_DIR)/img/*.pdf)

ZIP_NAME    := xpalacj00.zip
SUBMIT_DIRS := $(SRC_DIR) $(TEST_DIR)
SUBMIT_META := Makefile $(DOC_OUT) rozdeleni rozsireni

.PHONY: all build dev asan test-build test test-unit test-e2e test-e2e-update test-asan check format format-check tidy cppcheck build-check clean check-all doc zip

all: build

build: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

test-build: $(UNIT_TEST_BINS)

$(UNIT_TEST_BINS): $(OBJ_DIR)/%: $(OBJ_DIR)/%.o $(LIB_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

test-unit: test-build
	@fail=0; for t in $(UNIT_TEST_BINS); do ./$$t $(TEST_FLAGS) || fail=1; done; exit $$fail

test-e2e: $(TARGET)
	python3 $(E2E_TEST_DIR)/run.py

test-e2e-update: $(TARGET)
	python3 $(E2E_TEST_DIR)/run.py --update

test: test-unit test-e2e

test-asan:
	$(MAKE) clean
	$(MAKE) test-unit CFLAGS="$(DEV_CFLAGS) $(ASAN_FLAGS)" LDFLAGS="$(ASAN_FLAGS)"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/%.o: $(UNIT_TEST_DIR)/%.c | $(OBJ_DIR)
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

doc: $(DOC_OUT)

$(DOC_OUT): $(DOC_META) $(DOC_SOURCES) $(DOC_IMAGES)
	pandoc $(DOC_META) $(DOC_SOURCES) \
		--pdf-engine=xelatex \
		--resource-path=$(DOC_DIR):$(DOC_DIR)/img \
		-o $(DOC_OUT)

zip: $(DOC_OUT)
	rm -f $(ZIP_NAME)
	zip -r $(ZIP_NAME) $(SUBMIT_META) $(SUBMIT_DIRS) \
		-x "$(TEST_DIR)/bin/*" \
		-x "*.o" -x "*.d" -x "*.out" \
		-x "*/.*" -x ".*" -x "__MACOSX/*"

clean:
	rm -rf $(OBJ_DIR) $(TARGET)
