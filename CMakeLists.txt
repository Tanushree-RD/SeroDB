cmake_minimum_required(VERSION 3.16)

project(
    SeroDB
    VERSION 0.1.0
    DESCRIPTION "A SQLite-inspired database engine built from scratch for learning."
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

add_library(serodb_options INTERFACE)
target_compile_features(serodb_options INTERFACE cxx_std_17)

if(MSVC)
    target_compile_options(serodb_options INTERFACE /W4 /permissive-)
else()
    target_compile_options(serodb_options INTERFACE -Wall -Wextra -Wpedantic)
endif()

# -----------------------------------------------------------------------
# Core library — the storage engine (Pager, Table, Cursor, Executor, Parser)
# -----------------------------------------------------------------------

add_library(serodb_core
    src/row.cpp
    src/Pager.cpp
    src/Table.cpp
    src/Cursor.cpp
    src/Executor.cpp
    src/Parser.cpp
)

target_include_directories(serodb_core
    PUBLIC
        include
)

target_link_libraries(serodb_core
    PUBLIC
        serodb_options
)

# -----------------------------------------------------------------------
# REPL executable
# -----------------------------------------------------------------------

add_executable(serodb
    src/main.cpp
)

target_link_libraries(serodb
    PRIVATE
        serodb_core
        serodb_options
)

# -----------------------------------------------------------------------
# Tests
# -----------------------------------------------------------------------

add_executable(serodb_tests
    tests/tests.cpp
)

target_link_libraries(serodb_tests
    PRIVATE
        serodb_core
        serodb_options
)

add_test(NAME serodb_tests COMMAND serodb_tests)
