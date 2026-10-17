#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include "memory/memory.hpp"

namespace memory = baby48::memory;

TEST_CASE("MemoryImage has correct size", "[memory]"){
    CHECK(memory::storeSize == 32);
    CHECK(memory::MemoryImage{}.size() == memory::storeSize);
}

TEST_CASE("MemoryImage is initialized to zero", "[memory]"){
    memory::MemoryImage mem{};
    for (auto word : mem){
        CHECK(word == 0);
    }
}
