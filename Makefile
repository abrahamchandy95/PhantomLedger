CMAKE ?= cmake
BUILD_DIR ?= build
CONFIG ?= Debug
TESTS ?= ON
PYTHON ?= OFF

.PHONY: refresh build test clean rebuild

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

clean:
	$(CMAKE) -E rm -rf $(BUILD_DIR)

rebuild: clean build
