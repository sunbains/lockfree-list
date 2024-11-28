cmake_minimum_required(VERSION 3.14)
project(LockFreeList CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin)

include(cmake/CompilerOptions.cmake)

# Enable testing
enable_testing()

# Add Google Test directly
include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.12.1
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

include(cmake/FetchBenchmark.cmake)

# Find other required packages
find_package(Threads REQUIRED)

# Create header-only library target (INTERFACE)
add_library(lockfreelist INTERFACE)

target_include_directories(lockfreelist
  INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(lockfreelist
  INTERFACE
    Threads::Threads
)

# Tests executable
add_executable(lockfreelist_test
  tests/lockfreelist_test.cc
)

target_include_directories(lockfreelist_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${gtest_SOURCE_DIR}/include
)

target_link_libraries(lockfreelist_test
  PRIVATE
    lockfreelist
    gtest
    gtest_main
)

add_test(NAME lockfreelist_test COMMAND lockfreelist_test)

# Benchmarks executable
add_executable(lockfreelist_bench
  bench/lockfreelist_bench.cc
)

target_include_directories(lockfreelist_bench
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(lockfreelist_bench
  PRIVATE
    lockfreelist
    benchmark::benchmark
)

add_executable(iterator_bench
  bench/iterator_bench.cc
)

target_include_directories(iterator_bench
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(iterator_bench
  PRIVATE
    lockfreelist
    benchmark::benchmark
)

target_include_directories(iterator_bench
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${gtest_SOURCE_DIR}/include
)

add_executable(mt examples/mt.cc)

target_include_directories(mt
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
