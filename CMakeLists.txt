cmake_minimum_required(VERSION 3.25)
project(baby48 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(BABY48_SANITIZE "Enable ASan and UBSan" ON)

add_library(baby48_options INTERFACE)
target_compile_options(baby48_options INTERFACE -Wall -Wextra -Wpedantic -Wconversion)
target_compile_definitions(baby48_options INTERFACE $<$<CONFIG:Debug>:_GLIBCXX_ASSERTIONS>)

if(BABY48_SANITIZE)
    target_compile_options(baby48_options INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(baby48_options INTERFACE -fsanitize=address,undefined)
endif()

add_subdirectory(src)
