cmake_minimum_required(VERSION 3.16)
project(mini_hft CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Put built binaries in build/ (optional)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# You use pthread APIs in cpu.hpp → find & link Threads properly
find_package(Threads REQUIRED)

# Main engine executable
add_executable(engine_bin engine/main.cpp)
target_include_directories(engine_bin PRIVATE ${CMAKE_SOURCE_DIR})
target_compile_options(engine_bin PRIVATE -Wall -Wextra -Wpedantic -O3 -march=native)
target_link_libraries(engine_bin PRIVATE Threads::Threads)

# Benchmark executable
add_executable(bench_spsc_bench bench/spsc_bench.cpp)
target_include_directories(bench_spsc_bench PRIVATE ${CMAKE_SOURCE_DIR})
target_compile_options(bench_spsc_bench PRIVATE -Wall -Wextra -Wpedantic -O3 -march=native)
target_link_libraries(bench_spsc_bench PRIVATE Threads::Threads)

# Add tests
add_executable(spsc_test tests/test_spsc.cpp)
target_include_directories(spsc_test PRIVATE ${CMAKE_SOURCE_DIR})
target_compile_options(spsc_test PRIVATE -O2 -Wall -Wextra -Wpedantic)