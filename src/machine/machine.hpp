#pragma once

#include <cstdint>

#include "isa/isa.hpp"
#include "memory/memory.hpp"

namespace baby48::machine{

    enum class StepResult{
        Ok,
        Halted,
        Fault
    };

    class Machine{

    public:

        explicit Machine(const memory::MemoryImage& image);
        ~Machine() = default;

        StepResult step();

        const memory::MemoryImage& store() const{
            return store_;
        }

        std::uint32_t accumulator() const{
            return accumulator_;
        }

        std::uint32_t controlInstruction() const{
            return controlInstruction_;
        }

        StepResult status() const{
            return status_;
        }

        std::uint64_t instructionCount() const{
            return instructionCount_;
        }

    private:

        memory::MemoryImage store_;
        std::uint32_t accumulator_ = 0;
        std::uint32_t controlInstruction_ = 0;
        StepResult status_ = StepResult::Ok;
        std::uint64_t instructionCount_ = 0;



    };

    static_assert(memory::storeSize == isa::lineCount, "Memory store size must match ISA line count");

}
