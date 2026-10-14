#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include "isa/isa.hpp"

namespace isa = baby48::isa;

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


TEST_CASE("Sanity Check"){
    REQUIRE(1 == 1);
}

TEST_CASE("isa tests"){

    SECTION("Encode outputs correct word"){
        CHECK(isa::encode(isa::Opcode::LDN, 18) == 16402);
        CHECK(isa::encode(isa::Opcode::STOP, 22) == 57366);
    }

    SECTION("Decoder works correctly for each opcode and line number"){
        for (auto op : isa::opcodeList){
            for (std::uint8_t s = 0; s < isa::storeSize; ++s){
                CAPTURE(op, static_cast<int>(s));
                CHECK(isa::decode(isa::encode(op, s)) == isa::Instruction{op, s});
            }
        }

    }

    SECTION("Decoder works correctly with unused opcode 5"){
        std::uint32_t fiveOp = (5 << 13) | 18;
        CHECK(isa::decode(fiveOp) == isa::Instruction(isa::Opcode::SUB, 18));
    }

    SECTION("Decoder works when every unused bit is set"){
        CHECK(isa::decode(isa::encode(isa::Opcode::LDN, 18) | 0xFFFF1FE0) == isa::decode(isa::encode(isa::Opcode::LDN, 18)));
    }

    SECTION("mnemonic returns the correct strings"){
        CHECK(isa::mnemonic(isa::Opcode::JMP) == "JMP");
        CHECK(isa::mnemonic(isa::Opcode::STOP) == "STOP");
    }

    SECTION("opcodeFromMnemonic returns correct opcode, or nullopt if invalid"){
        CHECK(isa::opcodeFromMnemonic("STOP") == isa::Opcode::STOP);
        CHECK(isa::opcodeFromMnemonic("STP") == isa::Opcode::STOP);
        CHECK(isa::opcodeFromMnemonic("JUNK") == std::nullopt);
        CHECK(isa::opcodeFromMnemonic("") == std::nullopt);
    }

    SECTION("opcodeFromMnemonic is case-insensitive"){
        CHECK(isa::opcodeFromMnemonic("stop") == isa::Opcode::STOP);
        CHECK(isa::opcodeFromMnemonic("sToP") == isa::Opcode::STOP);
    }

    SECTION("mnemonic and opcodeFromMnemonic round-trip correctly"){
        CHECK(isa::mnemonic(isa::opcodeFromMnemonic("JMP").value()) == "JMP");
        CHECK(isa::opcodeFromMnemonic(isa::mnemonic(isa::Opcode::JMP)) == isa::Opcode::JMP);
    }

    SECTION("usesOperand returns correctly"){
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
}