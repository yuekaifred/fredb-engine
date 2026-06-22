cmake_minimum_required(VERSION 3.15)
project(fredb)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_COMPILER clang++)

find_program(CCACHE ccache)
if(CCACHE)
  set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE})
endif()

add_library(fredb_core STATIC fredb.cpp compaction.cpp wal.cpp memtable.cpp bloom.cpp)
target_include_directories(fredb_core PUBLIC .)
target_compile_options(fredb_core PUBLIC -pthread)
target_link_libraries(fredb_core PUBLIC pthread)

add_subdirectory(tests)
add_subdirectory(benchmark)
