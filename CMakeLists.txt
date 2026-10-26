# CMakeLists.txt - builds the benchmark and all tests on Windows, macOS and Linux.
#
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --config Release
#   cd build && ctest -C Release --output-on-failure
#

cmake_minimum_required(VERSION 3.16)
project(string_matching CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(MSVC)
  add_compile_options(/W4 /EHsc /utf-8)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# the three algorithms, shared by every program below
add_library(algorithms STATIC kmp.cpp bmp.cpp rabin_karp.cpp)
target_include_directories(algorithms PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(benchmark benchmark.cpp)
target_link_libraries(benchmark PRIVATE algorithms)

#may need rabin_karp_tests
enable_testing()
foreach(test_name kmp_tests bmp_tests cross_tests)
  add_executable(${test_name} ${test_name}.cpp)
  target_link_libraries(${test_name} PRIVATE algorithms)
  add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
