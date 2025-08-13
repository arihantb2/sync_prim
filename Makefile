# Makefile for orchestrating the CMake build of the UpgradeableMutexProject

# --- Variables ---
# The directory where build artifacts will be stored.
BUILD_DIR := build

# The installation path prefix. Can be overridden from the command line,
# e.g., `make install INSTALL_PREFIX=~/.local`
INSTALL_PREFIX ?= install

# --- Phony Targets ---
# These targets do not represent files and should always be executed.
.PHONY: all build run_tests clean install

# The default target when running `make`.
all: build

# --- Build Orchestration ---

# Configures the project with CMake and compiles all targets.
build:
	@echo "--- Configuring and Building Project in '$(BUILD_DIR)' ---"
	@cmake -B $(BUILD_DIR) -S . -DCMAKE_INSTALL_PREFIX=$(INSTALL_PREFIX)
	@cmake --build $(BUILD_DIR)

# Runs the test suite using CTest. Depends on the 'build' target.
run_tests: build
	@echo "--- Running Tests ---"
	@cd $(BUILD_DIR) && ctest --verbose

# Removes the build directory and all its contents.
clean:
	@echo "--- Cleaning Build Directory ---"
	@rm -rf $(BUILD_DIR)

# Installs the library to the specified INSTALL_PREFIX. Depends on the 'build' target.
# Requires an 'install' rule to be defined in CMakeLists.txt.
install: build
	@echo "--- Installing header library to $(INSTALL_PREFIX)/include ---"
	@cmake --install $(BUILD_DIR)
