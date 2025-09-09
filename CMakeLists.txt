cmake_minimum_required(VERSION 3.16)
project(mini_hft)   # <- use underscore, dash is invalid in target names

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# typo: add_executable (not add_exectuable)
add_executable(engine_bin engine/main.cpp)

# typo: -march=native (not -march=active)
target_compile_options(engine_bin PRIVATE -Wall -Wpedantic -Wextra -O3 -march=native)
