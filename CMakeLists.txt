cmake_minimum_required(VERSION 3.25)
project(baby48 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(baby48_options INTERFACE)
target_compile_options(baby48_options INTERFACE -Wall -Wextra -Wpedantic -Wconversion)

add_executable(hello src/main.cpp)

target_link_libraries(hello PRIVATE baby48_options)