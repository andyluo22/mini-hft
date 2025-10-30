cmake_minimum_required(VERSION 3.16)
project(mini_hft CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Put built binaries in build/ (optional)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# You use pthread APIs in cpu.hpp → find & link Threads properly
find_package(Threads REQUIRED)

# ---- TSAN toggle (move BEFORE targets so it applies globally if enabled) ----
option(ENABLE_TSAN "Build with ThreadSanitizer" OFF)
if(ENABLE_TSAN)
  add_compile_options(-fsanitize=thread -g -O1 -fno-omit-frame-pointer)
  add_link_options(-fsanitize=thread)
endif()

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
target_link_libraries(spsc_test PRIVATE Threads::Threads)

# Backpressure benchmark
add_executable(bench_spsc_backpressure bench/spsc_backpressure_bench.cpp)
target_include_directories(bench_spsc_backpressure PRIVATE ${CMAKE_SOURCE_DIR})
target_compile_options(bench_spsc_backpressure PRIVATE -O3 -march=native -fno-omit-frame-pointer)
target_link_libraries(bench_spsc_backpressure PRIVATE Threads::Threads)

# TSAN soak test
add_executable(spsc_soak_tsan tests/spsc_soak_tsan.cpp)
target_include_directories(spsc_soak_tsan PRIVATE ${CMAKE_SOURCE_DIR})
target_compile_options(spsc_soak_tsan PRIVATE -O2 -g -fno-omit-frame-pointer)
target_link_libraries(spsc_soak_tsan PRIVATE Threads::Threads)

add_executable(lob_props tests/lob_props.cpp)
target_include_directories(lob_props PRIVATE ${CMAKE_SOURCE_DIR})
target_compile_options(lob_props PRIVATE -O2 -g -Wall -Wextra -Wpedantic)

add_executable(match_bench bench/match_bench.cpp)
target_link_libraries(match_bench PRIVATE pthread)

add_executable(tsan_soak bench/tsan_soak.cpp)
target_compile_options(tsan_soak PRIVATE -O1 -g -fsanitize=thread)
target_link_options(tsan_soak PRIVATE -fsanitize=thread)
target_link_libraries(tsan_soak PRIVATE pthread)

add_executable(test_match tests/test_match.cpp)
target_link_libraries(test_match PRIVATE pthread gtest)

add_executable(test_volume_property tests/test_volume_property.cpp)
target_link_libraries(test_volume_property PRIVATE pthread gtest)

# (ensure your existing engine/common/timebase.hpp & cpu.hpp are in include paths)