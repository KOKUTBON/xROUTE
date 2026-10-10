# Makefile for xROUTE v2.0
CC ?= clang
CFLAGS ?= -std=c99 -Wall -Wextra -pedantic -O3 -fPIC
INCLUDES = -Iinclude
LDFLAGS ?=

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    SHLIB_EXT = dylib
    SHLIB_FLAGS = -dynamiclib
else
    SHLIB_EXT = so
    SHLIB_FLAGS = -shared
endif

BUILD_DIR = build
BIN_DIR = bin

SRC_CORE = src/map.c src/router.c src/parsers.c src/visualizer.c
OBJ_CORE = $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRC_CORE))

STATIC_LIB = $(BUILD_DIR)/libxroute.a
SHARED_LIB = $(BUILD_DIR)/libxroute.$(SHLIB_EXT)
CLI_BIN = $(BIN_DIR)/xroute
TEST_BIN = $(BIN_DIR)/xroute_tests

.PHONY: all clean test benchmark sanitizer format-check run_tests dirs

all: dirs $(STATIC_LIB) $(SHARED_LIB) $(CLI_BIN) $(TEST_BIN)

dirs:
	@mkdir -p $(BUILD_DIR)/src $(BIN_DIR)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(STATIC_LIB): $(OBJ_CORE)
	ar rcs $@ $^

$(SHARED_LIB): $(OBJ_CORE)
	$(CC) $(CFLAGS) $(SHLIB_FLAGS) $^ -o $@ $(LDFLAGS)

$(CLI_BIN): dirs cli/main.c $(STATIC_LIB)
	$(CC) $(CFLAGS) $(INCLUDES) cli/main.c $(STATIC_LIB) -o $@ $(LDFLAGS) -lm

$(TEST_BIN): dirs tests/test_main.c $(STATIC_LIB)
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_main.c $(STATIC_LIB) -o $@ $(LDFLAGS) -lm

example: dirs examples/basic_usage.c $(STATIC_LIB)
	$(CC) $(CFLAGS) $(INCLUDES) examples/basic_usage.c $(STATIC_LIB) -o $(BIN_DIR)/example_basic $(LDFLAGS) -lm
	./$(BIN_DIR)/example_basic

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR) test_roundtrip.xml test_temp_map.csv

# Build and run Studio without manually configuring the Python import path.
PYTHON ?= python3
.PHONY: studio test-python check test-native
studio: all
	PYTHONPATH=python $(PYTHON) -m xroute studio

test-python: $(SHARED_LIB)
	PYTHONPATH=python $(PYTHON) -m unittest discover -s tests/python -v

test-native: test

benchmark: $(CLI_BIN)
	@echo "Hardware/OS: $$(uname -a)"
	@echo "Compiler:    $$($(CC) --version | head -1)"
	@echo "CFLAGS:      $(CFLAGS)"
	./$(CLI_BIN) bench --map examples/maps/warehouse.xml 5000

sanitizer:
	$(MAKE) clean
	$(MAKE) test CC=clang CFLAGS='-std=c99 -Wall -Wextra -pedantic -g -fsanitize=address,undefined' LDFLAGS='-fsanitize=address,undefined'

format-check:
	@if command -v clang-format >/dev/null 2>&1; then clang-format --dry-run --Werror $$(find src cli tests include \( -name '*.c' -o -name '*.h' \)); else echo 'clang-format not installed; skipping format check'; fi

check: test test-python

# Static GitHub Pages demo; no native backend required.
.PHONY: demo demo-check demo-serve
demo:
	$(PYTHON) scripts/build_demo.py

demo-check:
	node --test tests/demo/api.test.cjs

demo-serve: demo
	$(PYTHON) -m http.server 8080 --bind 127.0.0.1 --directory build/demo
