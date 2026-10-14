#pragma once

#include <cstdint>

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
        STOP = 7    //   Stop   |   Light "Stop" neon and halt the machine
    };



}

