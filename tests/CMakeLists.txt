include(FetchContent)

FetchContent_Declare(Catch2 GIT_REPOSITORY https://github.com/catchorg/Catch2.git GIT_TAG v3.8.1 GIT_SHALLOW TRUE SYSTEM)
FetchContent_MakeAvailable(Catch2)

add_executable(baby48_tests tests.cpp)

target_link_libraries(baby48_tests PRIVATE Catch2::Catch2WithMain baby48_isa baby48_options baby48_memory)

include(Catch)
catch_discover_tests(baby48_tests)
