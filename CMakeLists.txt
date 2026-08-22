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

add_executable(serodb
    src/main.cpp
)

target_link_libraries(serodb
    PRIVATE
        serodb_options
)
