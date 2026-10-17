#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>


namespace baby48::isa{

    // Opcodes and descriptions taken from:
    // https://www.icsa.inf.ed.ac.uk/research/groups/hase/models/ssem/index.html
    enum class Opcode : std::uint8_t{
    //  Mnemonic        Ref not.    Description
    //-----------------------------------------------------------------------------------------
        JMP = 0,    //  s, C    |   Copy content of Store line to CI
        JRP = 1,    //  c+s, C  |   Add content of Store line to CI
        LDN = 2,    //  -s, A   |   Copy content of Store line, negated, to Accumulator
        STO = 3,    //  a, S    |   Copy content of Accumulator to Store line
        SUB = 4,    //  a-s, A  |   Subtract content of Store line from Accumulator
    //  --- = 5     //  -       |   Unused, same as SUB (function 4)
        CMP = 6,    //  Test    |   Skip next instruction if content of Accumulator is negative
        STOP = 7    //  Stop    |   Light "Stop" neon and halt the machine
    };

    inline constexpr std::uint32_t lineMask = 0x1F;
    inline constexpr std::uint32_t lineCount = lineMask + 1;
    inline constexpr std::uint32_t functionShift = 13;
    inline constexpr std::uint32_t functionMask = 0x7;

    inline constexpr std::array<Opcode, 7> opcodeList = {
        Opcode::JMP,
        Opcode::JRP,
        Opcode::LDN,
        Opcode::STO,
        Opcode::SUB,
        Opcode::CMP,
        Opcode::STOP
    };

    struct Instruction{
        Opcode op;
        std::uint8_t line;

        bool operator==(const Instruction&) const = default;
    };

    constexpr std::uint32_t encode(Opcode op, std::uint8_t line){
        assert(line < lineCount);

        std::uint32_t lineBits = static_cast<std::uint32_t>(line);

        std::uint32_t opBits = static_cast<std::uint32_t>(op);
        opBits = opBits << functionShift;

        return (lineBits | opBits);
    }

    constexpr Instruction decode(std::uint32_t word){
        std::uint8_t opBits = static_cast<std::uint8_t>((word >> functionShift) & functionMask);
        Opcode op = (opBits == 5) ? Opcode::SUB : static_cast<Opcode>(opBits);

        std::uint8_t line = static_cast<std::uint8_t>(word & lineMask);

        return Instruction{op, line};
    }

    constexpr std::string_view mnemonic(Opcode op){
        using enum Opcode;

        switch(op){
            case JMP:
                return "JMP";
            case JRP:
                return "JRP";
            case LDN:
                return "LDN";
            case STO:
                return "STO";
            case SUB:
                return "SUB";
            case CMP:
                return "CMP";
            case STOP:
                return "STOP";
        }
        std::unreachable();
    }

    namespace detail{
        constexpr char toUpperChar(char c){
            if (c >= 'a' && c <= 'z'){
                return static_cast<char>(c - ('a' - 'A'));
            }
            return c;
        }
    }

    constexpr std::optional<Opcode> opcodeFromMnemonic(std::string_view m){

        std::string mUpper;
        mUpper.resize(m.size());
        std::transform(m.begin(), m.end(), mUpper.begin(), detail::toUpperChar);

        if (mUpper == "STP"){        // For historical accuracy, accept STP as STOP opcode
            return Opcode::STOP;
        }

        for (auto op : opcodeList){
            if (mnemonic(op) == mUpper){
                return op;
            }
        }

        return std::nullopt;
    }

    constexpr bool usesOperand(Opcode op){
        using enum Opcode;

        switch(op){
            case CMP:
            case STOP:
                return false;

            case JMP:
            case JRP:
            case LDN:
            case STO:
            case SUB:
                return true;
        }

        std::unreachable();
    }

}


