add_library(baby48_isa INTERFACE)
add_library(baby48_memory INTERFACE)
target_include_directories(baby48_isa INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(baby48_memory INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(hello main.cpp)

target_link_libraries(hello PRIVATE baby48_options baby48_isa)

