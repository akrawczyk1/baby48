#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include "isa/isa.hpp"
#include "memory/memory.hpp"

namespace isa = baby48::isa;
namespace memory = baby48::memory;

template <>
struct Catch::StringMaker<isa::Instruction> {
    static std::string convert(const isa::Instruction& i) {
        return std::string(isa::mnemonic(i.op)) + " " + std::to_string(i.line);
    }
};

template <>
struct Catch::StringMaker<isa::Opcode>{
    static std::string convert(const isa::Opcode op){
        return std::string(isa::mnemonic(op));
    }
};

TEST_CASE("Isa lineCount is correct", "[isa]"){
    CHECK(isa::lineCount == 32);
}

TEST_CASE("Encode outputs correct word (hardcode)", "[isa]"){
    CHECK(isa::encode(isa::Opcode::LDN, 18) == 16402);
    CHECK(isa::encode(isa::Opcode::STOP, 22) == 57366);
}

TEST_CASE("Decode works correctly for each opcode and line number", "[isa]"){
    for (auto op : isa::opcodeList){
        for (std::uint8_t s = 0; s < isa::lineCount; ++s){
            CAPTURE(op, static_cast<int>(s));
            CHECK(isa::decode(isa::encode(op, s)) == isa::Instruction{op, s});
        }
    }
}

TEST_CASE("Decode works correctly for the extreme words", "[isa]"){
    // Maximum possible encoding for "STOP 31" = 0xFFFFFFFF with all optional bits set
    CHECK(isa::decode(0xFFFFFFFF) == isa::Instruction{isa::Opcode::STOP, 31});

    // Normal "STOP 31" encoding = 0xE01F also works
    CHECK(isa::decode(0xE01F) == isa::Instruction{isa::Opcode::STOP, 31});

    // Machine code for "JMP 0" = 0x00000000
    CHECK(isa::decode(0x00000000) == isa::Instruction{isa::Opcode::JMP, 0});
}

TEST_CASE("Encode outputs correct word for each opcode and line number", "[isa]"){
    for (auto op : isa::opcodeList){
        for (std::uint8_t s = 0; s < isa::lineCount; ++s){
            CAPTURE(op, static_cast<int>(s));
            // Magic number 13 comes from the project specification, which says that the opcode is stored in bits 13-15 of the instruction word
            CHECK(isa::encode(op, s) == (static_cast<std::uint32_t>(op) << 13 | static_cast<std::uint32_t>(s)));
        }
    }
}

TEST_CASE("Decode works correctly with unused opcode 5", "[isa]"){
    std::uint32_t fiveOp = (5 << 13) | 18;
    CHECK(isa::decode(fiveOp) == isa::Instruction(isa::Opcode::SUB, 18));
}

TEST_CASE("Decode works when every unused bit is set", "[isa]"){
    CHECK(isa::decode(isa::encode(isa::Opcode::LDN, 18) | 0xFFFF1FE0) == isa::decode(isa::encode(isa::Opcode::LDN, 18)));
}

TEST_CASE("mnemonic returns the correct strings for every opcode", "[isa]"){
    CHECK(isa::mnemonic(isa::Opcode::JMP) == "JMP");
    CHECK(isa::mnemonic(isa::Opcode::JRP) == "JRP");
    CHECK(isa::mnemonic(isa::Opcode::LDN) == "LDN");
    CHECK(isa::mnemonic(isa::Opcode::STO) == "STO");
    CHECK(isa::mnemonic(isa::Opcode::SUB) == "SUB");
    CHECK(isa::mnemonic(isa::Opcode::CMP) == "CMP");
    CHECK(isa::mnemonic(isa::Opcode::STOP) == "STOP");
}

TEST_CASE("opcodeFromMnemonic returns correct opcode, or nullopt if invalid", "[isa]"){
    // Valid opcodes
    CHECK(isa::opcodeFromMnemonic("STOP") == isa::Opcode::STOP);
    CHECK(isa::opcodeFromMnemonic("STP") == isa::Opcode::STOP);

    // Close but invalid opcodes
    CHECK(isa::opcodeFromMnemonic("STOOP") == std::nullopt);
    CHECK(isa::opcodeFromMnemonic("STP1") == std::nullopt);
    CHECK(isa::opcodeFromMnemonic("LD") == std::nullopt);

    // Leading/trailing whitespace
    CHECK(isa::opcodeFromMnemonic("STOP ") == std::nullopt);
    CHECK(isa::opcodeFromMnemonic(" LDN") == std::nullopt);

    // Invalid opcodes
    CHECK(isa::opcodeFromMnemonic("JUNK") == std::nullopt);
    CHECK(isa::opcodeFromMnemonic("NUM") == std::nullopt);
    CHECK(isa::opcodeFromMnemonic("") == std::nullopt);
}

TEST_CASE("opcodeFromMnemonic is case-insensitive", "[isa]"){
    CHECK(isa::opcodeFromMnemonic("stop") == isa::Opcode::STOP);
    CHECK(isa::opcodeFromMnemonic("sToP") == isa::Opcode::STOP);
    CHECK(isa::opcodeFromMnemonic("stp") == isa::Opcode::STOP);
}

TEST_CASE("mnemonic and opcodeFromMnemonic round-trip correctly", "[isa]"){
    CHECK(isa::mnemonic(isa::opcodeFromMnemonic("JMP").value()) == "JMP"); //sanity test

    for (auto op : isa::opcodeList){
        CAPTURE(op);
        CHECK(isa::opcodeFromMnemonic(isa::mnemonic(op)) == op);
    }
}

TEST_CASE("usesOperand returns correctly", "[isa]"){
    for (auto op : isa::opcodeList){
        CAPTURE(op);
        if (op == isa::Opcode::CMP || op == isa::Opcode::STOP){
            CHECK(isa::usesOperand(op) == false);
        }
        else{
            CHECK(isa::usesOperand(op) == true);
        }
    }
}

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
