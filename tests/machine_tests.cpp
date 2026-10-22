#include <catch2/catch_test_macros.hpp>
#include <initializer_list>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "isa/isa.hpp"
#include "machine/machine.hpp"
#include "memory/memory.hpp"

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

    memory::MemoryImage kilburnBuilder(std::int32_t input){

        memory::MemoryImage image = imageBuilder({
            {1, isa::encode(LDN, 18)},      // A = 0 (clear)
            {2, isa::encode(LDN, 19)},      // A = input
            {3, isa::encode(SUB, 20)},      //
            {4, isa::encode(CMP, 0)},       //
            {5, isa::encode(JRP, 21)},      // back to line 3
            {6, isa::encode(SUB, 22)},      // A += b -> remainder r
            {7, isa::encode(STO, 24)},      //
            {8, isa::encode(LDN, 22)},      // A = b
            {9, isa::encode(SUB, 23)},      // A = b - 1
            {10, isa::encode(STO, 20)},     // b = b - 1
            {11, isa::encode(LDN, 20)},     //
            {12, isa::encode(STO, 22)},     // store -b
            {13, isa::encode(LDN, 24)},     // A = -r
            {14, isa::encode(CMP, 0)},      //
            {15, isa::encode(JMP, 25)},     // r == 0 -> line 17
            {16, isa::encode(JMP, 18)},     // r > 0 -> line 1
            {17, isa::encode(STOP, 0)},
            {18, 0},
            {19, toTwosComplement(-input)},
            {20, input - 1},
            {21, toTwosComplement(-3)},
            {22, toTwosComplement(-(input - 1))},
            {23, 1},
            {24, 0},
            {25, 16}
        });

        return image;
    }
}

// Basic Tests ----------------------------

TEST_CASE("Machine can be constructed"){
    memory::MemoryImage image{};
    machine::Machine m(image);

    CHECK(m.store() == image);
    CHECK(m.accumulator() == 0);
    CHECK(m.controlInstruction() == 0);
    CHECK(m.status() == machine::StepResult::Ok);
    CHECK(m.instructionCount() == 0);
}

TEST_CASE("Machine can be constructed with non-zero memory image"){
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

    image[0] = 0x12345678;

    CHECK(m.store() == original);
    CHECK(m.accumulator() == 0);
    CHECK(m.controlInstruction() == 0);
    CHECK(m.status() == machine::StepResult::Ok);
    CHECK(m.instructionCount() == 0);
}

TEST_CASE("Machine executes line 1 first and correctly STOPs"){
    memory::MemoryImage image = imageBuilder({
        {0, isa::encode(LDN, 5)},
        {1, isa::encode(STOP, 0)},
        {5, 42}
    });

    machine::Machine m(image);



    REQUIRE(runHelper(m) == machine::StepResult::Halted);
    CHECK(m.instructionCount() == 1);
    CHECK(m.accumulator() == 0);
    CHECK(m.controlInstruction() == 1);
    CHECK(m.store() == image);
}

// Arithmetic / Accumulator Tests ----------------------------
TEST_CASE("LDN 0x80000000 gives accumulator 0x80000000"){
    const std::uint32_t HEX_8E8 = 0x80000000;
    memory::MemoryImage image = imageBuilder({
        {1, isa::encode(LDN, 8)},
        {2, isa::encode(STOP, 0)},
        {8, HEX_8E8}
    });

    machine::Machine m(image);

    REQUIRE(runHelper(m) == machine::StepResult::Halted);
    CHECK(m.accumulator() == HEX_8E8);
}

TEST_CASE("LDN 5 gives accumulator -5"){
    memory::MemoryImage image = imageBuilder({
        {1, isa::encode(LDN, 8)},
        {2, isa::encode(STOP, 0)},
        {8, 5}
    });

    machine::Machine m(image);

    REQUIRE(runHelper(m) == machine::StepResult::Halted);
    CHECK(m.accumulator() == toTwosComplement(-5));
}

TEST_CASE("LDN 0 gives accumulator 0"){
    memory::MemoryImage image = imageBuilder({
        {1, isa::encode(LDN, 8)},
        {2, isa::encode(LDN, 9)},
        {3, isa::encode(STOP, 0)},
        {8, 5},
        {9, 0}
    });

    machine::Machine m(image);

    REQUIRE(runHelper(m) == machine::StepResult::Halted);
    CHECK(m.accumulator() == 0);
}

TEST_CASE("LDN negative number gives positive"){
    memory::MemoryImage image = imageBuilder({
        {1, isa::encode(LDN, 8)},
        {2, isa::encode(STOP, 0)},
        {8, toTwosComplement(-42)}
    });

    machine::Machine m(image);

    REQUIRE(runHelper(m) == machine::StepResult::Halted);
    CHECK(m.accumulator() == 42);
}

TEST_CASE("0 - 1 = 0xFFFFFFFF"){
    const std::uint32_t ALL_F = 0xFFFFFFFF;
    memory::MemoryImage image = imageBuilder({
        {1, isa::encode(SUB, 10)},
        {2, isa::encode(STOP, 0)},
        {10, 1}
    });

    machine::Machine m(image);

    REQUIRE(runHelper(m) == machine::StepResult::Halted);
    CHECK(m.accumulator() == ALL_F);
}

TEST_CASE("10 - 3 = 7"){
    memory::MemoryImage image = imageBuilder({
        {1, isa::encode(LDN, 8)},
        {2, isa::encode(SUB, 9)},
        {3, isa::encode(STOP, 0)},
        {8, toTwosComplement(-10)},
        {9, 3}
    });

    machine::Machine m(image);

    REQUIRE(runHelper(m) == machine::StepResult::Halted);
    CHECK(m.accumulator() == 7);
}

TEST_CASE("STO only writes to the appropriate line"){
    memory::MemoryImage image = imageBuilder({
        {1, isa::encode(LDN, 5)},
        {2, isa::encode(STO, 30)},
        {3, isa::encode(STOP, 0)},
        {5, toTwosComplement(-10)},
        {6, 30}
    });

    machine::Machine m(image);

    CHECK(m.store().at(30) == 0);
    REQUIRE(runHelper(m) == machine::StepResult::Halted);
    REQUIRE(m.accumulator() == 10);

    for (std::size_t i = 0; i < memory::storeSize; i++){
        switch (i){
            case 1:
            CHECK(m.store().at(i) == isa::encode(LDN, 5));
            break;

            case 2:
            CHECK(m.store().at(i) == isa::encode(STO, 30));
            break;

            case 3:
            CHECK(m.store().at(i) == isa::encode(STOP, 0));
            break;

            case 5:
            CHECK(m.store().at(i) == toTwosComplement(-10));
            break;

            case 6:
            CHECK(m.store().at(i) == 30);
            break;

            case 30:
            CHECK(m.store().at(i) == 10);
            break;

            default:
            CHECK(m.store().at(i) == 0);
            break;
        }
    }
}

TEST_CASE("Opcode 5 is read as SUB"){

    const std::uint32_t ALL_F = 0xFFFFFFFF;
    memory::MemoryImage image = imageBuilder({
        {1, (5u << 13) | 10},
        {2, isa::encode(STOP, 0)},
        {10, 1}
    });

    machine::Machine m(image);

    REQUIRE(runHelper(m) == machine::StepResult::Halted);
    CHECK(m.accumulator() == ALL_F);

}

// JMP / JRP / CMP Tests ----------------------------
TEST_CASE("JMP to line 5 executes line 6, not line 5"){

    memory::MemoryImage image = imageBuilder({
        {1, isa::encode(JMP, 10)},
        {2, isa::encode(STOP, 0)},
        {5, isa::encode(STOP, 0)},
        {6, isa::encode(STOP, 0)},
        {7, isa::encode(STOP, 0)},

        {10, 5}
    });

    machine::Machine m(image);

    REQUIRE(runHelper(m) == machine::StepResult::Halted);
    CHECK(m.controlInstruction() == 6);
}

TEST_CASE("JMP -1 jumps to and executes line 0"){
    memory::MemoryImage image = imageBuilder({
        {0, isa::encode(STOP, 0)},
        {1, isa::encode(JMP, 10)},
        {2, isa::encode(STOP, 0)},

        {10, toTwosComplement(-1)}
    });

    machine::Machine m(image);

    REQUIRE(runHelper(m) == machine::StepResult::Halted);
    CHECK(m.controlInstruction() == 0);
}

TEST_CASE("JRP Forward by 2 -- [1] = JRP 20 | [4] = STOP | [20] = 2"){
    memory::MemoryImage image = imageBuilder({
        {1, isa::encode(JRP, 20)},
        {3, isa::encode(STOP, 0)},
        {4, isa::encode(STOP, 0)},
        {5, isa::encode(STOP, 0)},

        {20, 2}
    });

    machine::Machine m(image);

    REQUIRE(runHelper(m) == machine::StepResult::Halted);
    CHECK(m.controlInstruction() == 4);
}

TEST_CASE("JRP Backward by 3 -- [1] = JMP 20 | [3] = STOP | [5] = JRP 25 | [20] = 4 | [25] = -3"){
    memory::MemoryImage image = imageBuilder({
        {1, isa::encode(JMP, 20)},
        {2, isa::encode(STOP, 0)},
        {3, isa::encode(STOP, 0)},
        {4, isa::encode(STOP, 0)},
        {5, isa::encode(JRP, 25)},
        {20, 4},
        {25, toTwosComplement(-3)}
    });

    machine::Machine m(image);

    REQUIRE(runHelper(m) == machine::StepResult::Halted);
    CHECK(m.controlInstruction() == 3);
}

TEST_CASE("CMP A = -5 (skip)"){
    memory::MemoryImage image = imageBuilder({
        {1, isa::encode(LDN, 20)},
        {2, isa::encode(CMP, 0)},
        {3, isa::encode(STOP, 0)},
        {4, isa::encode(STOP, 0)},

        {20, 5}
    });

    machine::Machine m(image);

    REQUIRE(runHelper(m) == machine::StepResult::Halted);
    CHECK(m.controlInstruction() == 4);
}

TEST_CASE("CMP A = 0 (don't skip)"){
    memory::MemoryImage image = imageBuilder({
        {1, isa::encode(LDN, 20)},
        {2, isa::encode(CMP, 0)},
        {3, isa::encode(STOP, 0)},
        {4, isa::encode(STOP, 0)},

        {20, 0}
    });

    machine::Machine m(image);

    REQUIRE(runHelper(m) == machine::StepResult::Halted);
    CHECK(m.controlInstruction() == 3);
}

TEST_CASE("CMP A = 5 (don't skip)"){
    memory::MemoryImage image = imageBuilder({
        {1, isa::encode(LDN, 20)},
        {2, isa::encode(CMP, 0)},
        {3, isa::encode(STOP, 0)},
        {4, isa::encode(STOP, 0)},

        {20, toTwosComplement(-5)}
    });

    machine::Machine m(image);

    REQUIRE(runHelper(m) == machine::StepResult::Halted);
    CHECK(m.controlInstruction() == 3);
}

TEST_CASE("CMP A = 0x80000000 (skip)"){
    const std::uint32_t HEX_8E8 = 0x80000000;
    memory::MemoryImage image = imageBuilder({
        {1, isa::encode(LDN, 20)},
        {2, isa::encode(CMP, 0)},
        {3, isa::encode(STOP, 0)},
        {4, isa::encode(STOP, 0)},

        {20, HEX_8E8}
    });

    machine::Machine m(image);

    REQUIRE(runHelper(m) == machine::StepResult::Halted);
    CHECK(m.controlInstruction() == 4);
}

// Status Tests ----------------------------

TEST_CASE("Machine halts properly on step() when m.status() == Halted"){
    memory::MemoryImage image = imageBuilder({
        {1, isa::encode(LDN, 5)},
        {2, isa::encode(STOP, 0)},
        {5, toTwosComplement(-42)}
    });

    machine::Machine m(image);

    REQUIRE(runHelper(m) == machine::StepResult::Halted);

    CHECK(m.controlInstruction() == 2);
    CHECK(m.instructionCount() == 2);
    CHECK(m.accumulator() == 42);

    CHECK(m.step() == machine::StepResult::Halted);

    CHECK(m.controlInstruction() == 2);
    CHECK(m.instructionCount() == 2);
    CHECK(m.accumulator() == 42);
}

TEST_CASE("Fault on bit 13 set in CI"){
    const std::uint32_t BIT_13 = 0x2000;
    memory::MemoryImage image = imageBuilder({
        {1, isa::encode(JMP, 20)},
        {2, isa::encode(STOP, 0)},
        {20, BIT_13}
    });

    machine::Machine m(image);

    REQUIRE(runHelper(m) == machine::StepResult::Fault);
    CHECK(m.instructionCount() == 1);
    CHECK(m.controlInstruction() == BIT_13 + 1);
    CHECK(m.step() == machine::StepResult::Fault);
    CHECK(m.instructionCount() == 1);
    CHECK(m.controlInstruction() == BIT_13 + 1);
}

TEST_CASE("Fault on jumping back through instruction 0"){
    memory::MemoryImage image = imageBuilder({
        {1, isa::encode(JRP, 20)},
        {2, isa::encode(STOP, 0)},
        {20, toTwosComplement(-5)}
    });

    machine::Machine m(image);

    REQUIRE(runHelper(m) == machine::StepResult::Fault);
    CHECK(m.controlInstruction() == 0xFFFFFFFD);
    CHECK(m.instructionCount() == 1);
}

TEST_CASE("CI wraps through line 31 to line 0"){
    memory::MemoryImage image = imageBuilder({
        {0, isa::encode(STOP, 0)},
        {1, isa::encode(JMP, 20)},
        {2, isa::encode(STOP, 0)},
        {20, 30},
        {21, 42},
        {31, isa::encode(LDN, 21)}
    });

    machine::Machine m(image);

    REQUIRE(runHelper(m) == machine::StepResult::Halted);
    CHECK(m.accumulator() == toTwosComplement(-42));
    CHECK(m.controlInstruction() == 32);
}

TEST_CASE("Kilburn Program (input = 989)", "[kilburn]"){
    memory::MemoryImage kilburn = kilburnBuilder(989);

    machine::Machine m(kilburn);

    REQUIRE(runHelper(m, 3'000'000) == machine::StepResult::Halted);
    CHECK(m.store().at(20) == 42);
    CHECK(m.instructionCount() == 21'387);
    CHECK(m.controlInstruction() == 17);
}

TEST_CASE("Kilburn Program (input = 3141)", "[kilburn]"){
    memory::MemoryImage kilburn = kilburnBuilder(3141);

    machine::Machine m(kilburn);

    REQUIRE(runHelper(m, 3'000'000) == machine::StepResult::Halted);
    CHECK(m.store().at(20) == 1046);
    CHECK(m.instructionCount() == 37'174);
    CHECK(m.controlInstruction() == 17);
}
TEST_CASE("Kilburn Program (input = 4537)", "[kilburn]"){
    memory::MemoryImage kilburn = kilburnBuilder(4537);

    machine::Machine m(kilburn);

    REQUIRE(runHelper(m, 3'000'000) == machine::StepResult::Halted);
    CHECK(m.store().at(20) == 348);
    CHECK(m.instructionCount() == 88'333);
    CHECK(m.controlInstruction() == 17);
}

TEST_CASE("Kilburn Program (input = 262'144)", "[kilburn] [slow]"){
    memory::MemoryImage kilburn = kilburnBuilder(262'144);

    machine::Machine m(kilburn);

    REQUIRE(runHelper(m, 3'000'000) == machine::StepResult::Halted);
    CHECK(m.store().at(20) == 131'071);
    CHECK(m.instructionCount() == 2'228'228);
    CHECK(m.controlInstruction() == 17);
}
