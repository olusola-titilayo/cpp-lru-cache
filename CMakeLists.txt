cmake_minimum_required(VERSION 3.14)
project(cpp_lru_cache LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(lru_cache INTERFACE)
target_include_directories(lru_cache INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

option(LRU_SANITIZE "Build tests with address and undefined-behaviour sanitizers" OFF)

enable_testing()
add_executable(test_lru_cache tests/test_lru_cache.cpp)
target_link_libraries(test_lru_cache PRIVATE lru_cache)
target_compile_options(test_lru_cache PRIVATE -Wall -Wextra -Wpedantic -Werror)
if(LRU_SANITIZE)
  target_compile_options(test_lru_cache PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(test_lru_cache PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME lru_cache COMMAND test_lru_cache)

add_executable(memoize examples/memoize.cpp)
target_link_libraries(memoize PRIVATE lru_cache)
