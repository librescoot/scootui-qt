BUILD_DIR := build
CMAKE_FLAGS := -DCMAKE_BUILD_TYPE=Debug
JOBS := $(shell sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)

.PHONY: all configure build run clean rebuild

all: build

configure:
	@cmake -B $(BUILD_DIR) -S . $(CMAKE_FLAGS)

build: configure
	@cmake --build $(BUILD_DIR) -j$(JOBS)

run: build
	@$(BUILD_DIR)/bin/scootui

clean:
	@rm -rf $(BUILD_DIR)

rebuild: clean build
