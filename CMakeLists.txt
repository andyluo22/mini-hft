cmake_minimum_required(VERSION 3.16)
project(mini_hft CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

find_package(Threads REQUIRED)

# -------- Optional: ThreadSanitizer (global toggle) --------
option(ENABLE_TSAN "Build with ThreadSanitizer" OFF)
if(ENABLE_TSAN)
  add_compile_options(-fsanitize=thread -g -O1 -fno-omit-frame-pointer)
  add_link_options(-fsanitize=thread)
endif()

# -------- GoogleTest (for unit/property tests) -------------
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# -------- Common flags -------------------------------------
set(COMMON_WARN_FLAGS -Wall -Wextra -Wpedantic)
set(COMMON_OPT_FLAGS  -O3 -march=native)

# -------- Engine / Bench / Tests ---------------------------
# Main engine executable
add_executable(engine_bin engine/main.cpp)
target_include_directories(engine_bin PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/engine)
target_compile_options(engine_bin PRIVATE ${COMMON_WARN_FLAGS} ${COMMON_OPT_FLAGS})
target_link_libraries(engine_bin PRIVATE Threads::Threads)

# SPSC micro-benchmark
add_executable(bench_spsc_bench bench/spsc_bench.cpp)
target_include_directories(bench_spsc_bench PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/engine)
target_compile_options(bench_spsc_bench PRIVATE ${COMMON_WARN_FLAGS} ${COMMON_OPT_FLAGS})
target_link_libraries(bench_spsc_bench PRIVATE Threads::Threads)

# SPSC soak (TSAN)
add_executable(spsc_soak_tsan tests/spsc_soak_tsan.cpp)
target_include_directories(spsc_soak_tsan PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/engine)
target_compile_options(spsc_soak_tsan PRIVATE -O2 -g -fno-omit-frame-pointer ${COMMON_WARN_FLAGS})
target_link_libraries(spsc_soak_tsan PRIVATE Threads::Threads)

# Backpressure benchmark
add_executable(bench_spsc_backpressure bench/spsc_backpressure_bench.cpp)
target_include_directories(bench_spsc_backpressure PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/engine)
target_compile_options(bench_spsc_backpressure PRIVATE ${COMMON_OPT_FLAGS} -fno-omit-frame-pointer ${COMMON_WARN_FLAGS})
target_link_libraries(bench_spsc_backpressure PRIVATE Threads::Threads)

# LOB property/invariants demo (if you have it)
add_executable(lob_props tests/lob_props.cpp)
target_include_directories(lob_props PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/engine)
target_compile_options(lob_props PRIVATE -O2 -g ${COMMON_WARN_FLAGS})
target_link_libraries(lob_props PRIVATE Threads::Threads)

# Day 5: match latency bench & TSAN soak
add_executable(match_bench bench/match_bench.cpp)
target_include_directories(match_bench PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/engine)
target_compile_options(match_bench PRIVATE ${COMMON_OPT_FLAGS} ${COMMON_WARN_FLAGS})
target_link_libraries(match_bench PRIVATE Threads::Threads)

add_executable(tsan_soak bench/tsan_soak.cpp)
target_include_directories(tsan_soak PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/engine)
target_compile_options(tsan_soak PRIVATE -O1 -g -fsanitize=thread ${COMMON_WARN_FLAGS})
target_link_options(tsan_soak PRIVATE -fsanitize=thread)
target_link_libraries(tsan_soak PRIVATE Threads::Threads)

# -------- Tests (with GoogleTest) --------------------------
add_executable(spsc_test tests/test_spsc.cpp)
target_include_directories(spsc_test PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/engine)
target_compile_options(spsc_test PRIVATE -O2 ${COMMON_WARN_FLAGS})
target_link_libraries(spsc_test PRIVATE Threads::Threads)

add_executable(test_match tests/test_match.cpp)
target_include_directories(test_match PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/engine)
target_compile_options(test_match PRIVATE -O2 ${COMMON_WARN_FLAGS})
target_link_libraries(test_match PRIVATE gtest_main Threads::Threads)

add_executable(test_volume_property tests/test_volume_property.cpp)
target_include_directories(test_volume_property PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/engine)
target_compile_options(test_volume_property PRIVATE -O2 ${COMMON_WARN_FLAGS})
target_link_libraries(test_volume_property PRIVATE gtest_main Threads::Threads)

# ----- Day 6 new tests -----
add_executable(test_ioc_fok tests/test_ioc_fok.cpp)
target_include_directories(test_ioc_fok PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/engine)
target_compile_options(test_ioc_fok PRIVATE -O2 ${COMMON_WARN_FLAGS})
target_link_libraries(test_ioc_fok PRIVATE gtest_main Threads::Threads)

add_executable(test_replace tests/test_replace.cpp)
target_include_directories(test_replace PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/engine)
target_compile_options(test_replace PRIVATE -O2 ${COMMON_WARN_FLAGS})
target_link_libraries(test_replace PRIVATE gtest_main Threads::Threads)

add_executable(test_stp tests/test_stp.cpp)
target_include_directories(test_stp PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/engine)
target_compile_options(test_stp PRIVATE -O2 ${COMMON_WARN_FLAGS})
target_link_libraries(test_stp PRIVATE gtest_main Threads::Threads)

add_executable(test_no_ghost_orders tests/test_no_ghost_orders.cpp)
target_include_directories(test_no_ghost_orders PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/engine)
target_compile_options(test_no_ghost_orders PRIVATE -O2 ${COMMON_WARN_FLAGS})
target_link_libraries(test_no_ghost_orders PRIVATE gtest_main Threads::Threads)

# Optional: enable CTest integration
include(CTest)
add_test(NAME test_match            COMMAND test_match)
add_test(NAME test_volume_property  COMMAND test_volume_property)
add_test(NAME test_ioc_fok          COMMAND test_ioc_fok)
add_test(NAME test_replace          COMMAND test_replace)
add_test(NAME test_stp              COMMAND test_stp)
add_test(NAME test_no_ghost_orders  COMMAND test_no_ghost_orders)
