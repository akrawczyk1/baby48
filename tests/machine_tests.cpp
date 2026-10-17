#include <catch2/catch_test_macros.hpp>
#include <initializer_list>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "machine/machine.hpp"

namespace machine = baby48::machine;
namespace memory = baby48::memory;
namespace isa = baby48::isa;

using enum isa::Opcode;

// Helper functions for tests
namespace{

    // Takes a list of line number, encoded operation pairs.
    memory::MemoryImage imageBuilder(const std::initializer_list<std::pair<std::size_t, std::uint32_t>> words){
        memory::MemoryImage image{};
        for (auto [lineNumber, machCode] : words){
            image.at(lineNumber) = machCode;
        }

        return image;
    }

    std::uint32_t toTwosComplement(std::int32_t value){
        return static_cast<std::uint32_t>(value);
    }

    // Run the machine for a number of steps, or until it halts or faults. Returns the final status.
    machine::StepResult runHelper(machine::Machine& m, std::size_t steps = 32){
        machine::StepResult result = machine::StepResult::Ok;
        for (std::size_t i = 0; i < steps; ++i){
            result = m.step();
            if (result != machine::StepResult::Ok){
                break;
            }
        }
        return result;
    }
}


TEST_CASE("Machine can be constructed", "[machine]"){
    memory::MemoryImage image{};
    machine::Machine m(image);

    CHECK(m.store() == image);
    CHECK(m.accumulator() == 0);
    CHECK(m.controlInstruction() == 0);
    CHECK(m.status() == machine::StepResult::Ok);
    CHECK(m.instructionCount() == 0);
}

TEST_CASE("Machine can be constructed with non-zero memory image", "[machine]"){
    memory::MemoryImage image = imageBuilder({
        {0, isa::encode(JMP, 0)},
        {1, isa::encode(JRP, 1)},
        {2, isa::encode(LDN, 2)},
        {3, isa::encode(STO, 3)},
        {4, isa::encode(SUB, 4)},
        {5, isa::encode(CMP, 5)},
        {6, isa::encode(STOP, 6)}

    });

    auto original = image;
    machine::Machine m(image);

    CHECK(m.store() == original);
    CHECK(m.accumulator() == 0);
    CHECK(m.controlInstruction() == 0);
    CHECK(m.status() == machine::StepResult::Ok);
    CHECK(m.instructionCount() == 0);
}
