CMAKE     ?= cmake
BUILD_DIR ?= build
CONFIG    ?= Release
TESTS     ?= ON
PYTHON    ?= OFF
BIN       ?= phantomledger
ARGS      ?=

.PHONY: refresh build test clean rebuild run run-help run-fast

refresh:
	$(CMAKE) -S . -B $(BUILD_DIR) \
		-DCMAKE_BUILD_TYPE=$(CONFIG) \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		-DPL_BUILD_TESTS=$(TESTS) \
		-DPL_BUILD_PYTHON=$(PYTHON)

build: refresh
	$(CMAKE) --build $(BUILD_DIR) -j

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

run: build
	./$(BUILD_DIR)/$(BIN) $(ARGS)

run-help: build
	./$(BUILD_DIR)/$(BIN) --help

run-fast:
	$(CMAKE) --build $(BUILD_DIR) --target $(BIN) -j
	./$(BUILD_DIR)/$(BIN) $(ARGS)

clean:
	$(CMAKE) -E rm -rf $(BUILD_DIR)

rebuild: clean build
