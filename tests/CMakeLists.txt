include(FetchContent)

FetchContent_Declare(Catch2 GIT_REPOSITORY https://github.com/catchorg/Catch2.git GIT_TAG v3.8.1 GIT_SHALLOW TRUE SYSTEM)
FetchContent_MakeAvailable(Catch2)

add_executable(baby48_isa_tests isa_tests.cpp)
add_executable(baby48_memory_tests memory_tests.cpp)

target_link_libraries(baby48_isa_tests PRIVATE Catch2::Catch2WithMain baby48_options baby48_isa)
target_link_libraries(baby48_memory_tests PRIVATE Catch2::Catch2WithMain  baby48_options baby48_memory)

include(Catch)
catch_discover_tests(baby48_isa_tests)
catch_discover_tests(baby48_memory_tests)
