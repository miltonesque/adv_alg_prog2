cmake_minimum_required(VERSION 3.20)

project(StringMatchingBenchmark LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# benchmarks are only meaningful with optimisation on, so default to Release
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(MSVC)
    add_compile_options(/W4 /EHsc)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ---- the three algorithms ----
add_library(string_matching
    src/kmp.cpp
    src/rabin_karp.cpp
    src/boyer_moore.cpp
)
target_include_directories(string_matching PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

# ---- input families (shared by the benchmark and the tests) ----
add_library(bench_inputs benchmark/input_generators.cpp)
target_include_directories(bench_inputs PUBLIC ${PROJECT_SOURCE_DIR}/benchmark)

# ---- benchmark ----
add_executable(benchmark_app
    benchmark/benchmark_main.cpp
    benchmark/benchmark_runner.cpp
)
target_link_libraries(benchmark_app PRIVATE string_matching bench_inputs)

# record the exact compiler and flags in results/environment.txt.
# $<CONFIG> is used so the right build type and flags are recorded with
# Visual Studio too, where CMAKE_BUILD_TYPE is empty
target_compile_definitions(benchmark_app PRIVATE
    BENCH_COMPILER="${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
    BENCH_FLAGS="${CMAKE_CXX_FLAGS} $<$<CONFIG:Release>:${CMAKE_CXX_FLAGS_RELEASE}>$<$<CONFIG:Debug>:${CMAKE_CXX_FLAGS_DEBUG}>"
    BENCH_BUILD_TYPE="$<CONFIG>"
)

# ---- explanation tests: controls that check our explanations ----
add_executable(explain_app benchmark/explanation_tests.cpp)
target_link_libraries(explain_app PRIVATE string_matching bench_inputs)

# ---- tests: each file has its own main, so each is its own executable ----
enable_testing()

add_executable(kmp_tests tests/kmp_tests.cpp)
target_link_libraries(kmp_tests PRIVATE string_matching)
add_test(NAME kmp COMMAND kmp_tests)

add_executable(boyer_moore_tests tests/boyer_moore_tests.cpp)
target_link_libraries(boyer_moore_tests PRIVATE string_matching)
add_test(NAME boyer_moore COMMAND boyer_moore_tests)

add_executable(stringmatching_tests tests/stringmatching_tests.cpp)
target_link_libraries(stringmatching_tests PRIVATE string_matching)
add_test(NAME stringmatching COMMAND stringmatching_tests)

add_executable(cross_tests tests/cross_tests.cpp)
target_link_libraries(cross_tests PRIVATE string_matching bench_inputs)
add_test(NAME cross COMMAND cross_tests)

