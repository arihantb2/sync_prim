# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.22)

# Define the project name, version, and language
project(UpgradeableMutexProject VERSION 1.0 LANGUAGES CXX)

# Set the C++ standard to C++17 and enforce it
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Find Required Packages ---
# Find the built-in Threads package to enable multi-threading support.
# This defines the `Threads::Threads` target used for linking.
find_package(Threads REQUIRED)

# --- Library Definition ---
# For a header-only library, we just need to specify the include directory.
# This makes sure that any target can find the headers using #include "sync_prim/..."
include_directories(include)

# --- Executable Targets ---

# 1. Bank Account Example
add_executable(bank_example src/bank_account_example.cpp)
# Link against the pthreads library, which is required for std::thread
target_link_libraries(bank_example PRIVATE Threads::Threads)

# 2. Benchmark Runner
add_executable(run_benchmarks src/benchmark_upgrade_mutex.cpp)
# Link against pthreads
target_link_libraries(run_benchmarks PRIVATE Threads::Threads)

# 3. Unit Tests
add_executable(run_tests tests/test_upgrade_mutex.cpp)
# Link against pthreads
target_link_libraries(run_tests PRIVATE Threads::Threads)


# --- Testing Integration ---
# Enable the CTest testing framework
enable_testing()

# Add the test executable to CTest
# This allows you to run all your tests by simply calling `ctest` in your build directory
add_test(NAME SyncPrimTests COMMAND run_tests)

# --- Installation ---
# Define an install rule for the header-only library.
# This will install the 'sync_prim' directory into the <prefix>/include directory.
install(
    DIRECTORY include/sync_prim
    DESTINATION include
)