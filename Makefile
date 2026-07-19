CMAKE     ?= cmake
BUILD_DIR ?= build
CONFIG    ?= Release
TESTS     ?= ON
BIN       ?= phantomledger
ARGS      ?=

# Topic filter for the diagnostics run targets below. Comma-separated:
#   sim,spending,routing,clearing,liquidity,entities,mem   (or: all)
TOPICS    ?= all

.PHONY: refresh build test clean rebuild run run-help run-fast \
        run-info run-debug run-trace run-mem

refresh:
	$(CMAKE) -S . -B $(BUILD_DIR) \
		-DCMAKE_BUILD_TYPE=$(CONFIG) \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		-DPL_BUILD_TESTS=$(TESTS)

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

# ---------------------------------------------------------------------
# Diagnostics runs. The generator is SILENT by default (the logger's
# runtime level defaults to warn); these targets are the supported way
# to switch diagnostics on, so nobody has to remember the PL_LOG_*
# environment variables:
#
#   make run-info  ARGS="--population 2000 --days 60"
#   make run-debug ARGS="..." TOPICS=spending,liquidity
#   make run-trace ARGS="..." TOPICS=routing
#   make run-mem   ARGS="..."             # per-stage peak-RSS lines
# ---------------------------------------------------------------------

run-info: build
	PL_LOG_LEVEL=info PL_LOG_TOPICS=$(TOPICS) ./$(BUILD_DIR)/$(BIN) $(ARGS)

run-debug: build
	PL_LOG_LEVEL=debug PL_LOG_TOPICS=$(TOPICS) ./$(BUILD_DIR)/$(BIN) $(ARGS)

run-trace: build
	PL_LOG_LEVEL=trace PL_LOG_TOPICS=$(TOPICS) ./$(BUILD_DIR)/$(BIN) $(ARGS)

run-mem: build
	PL_LOG_LEVEL=info PL_LOG_TOPICS=mem ./$(BUILD_DIR)/$(BIN) $(ARGS)

clean:
	$(CMAKE) -E rm -rf $(BUILD_DIR)

rebuild: clean build
