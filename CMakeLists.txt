cmake_minimum_required(VERSION 3.16)
project(NexDB VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(nexdb_core
    nexdb/Database.cpp
    nexdb/Parser.cpp
    nexdb/Storage.cpp
)
target_include_directories(nexdb_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(nexdb_core PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)

add_executable(nexdb nexdb/main.cpp)
target_link_libraries(nexdb PRIVATE nexdb_core)

include(CTest)
if(BUILD_TESTING)
    add_executable(nexdb_tests tests/test_nexdb.cpp)
    target_link_libraries(nexdb_tests PRIVATE nexdb_core)
    add_test(NAME NexDBTests COMMAND nexdb_tests)
endif()
