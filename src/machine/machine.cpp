
#include "machine/machine.hpp"

#include "isa/isa.hpp"
#include "memory/memory.hpp"


// Helpers
namespace{
    bool isNegative(std::uint32_t input){
        return static_cast<std::int32_t>(input) < 0;
    }
}

namespace baby48::machine{

    // Constructor
    Machine::Machine(const memory::MemoryImage& image):
        store_(image){}

    StepResult Machine::step(){

        if (status_ != StepResult::Ok){
            return status_;
        }

        ++controlInstruction_;

        if (((controlInstruction_ >> isa::functionShift) & isa::functionMask) != 0){
            status_ = StepResult::Fault;
            return status_;
        }

        isa::Instruction instr = isa::decode(store_[controlInstruction_ & isa::lineMask]);

        ++instructionCount_;

        switch (instr.op){
            case isa::Opcode::JMP:
                controlInstruction_ = store_[instr.lineOperand];
                break;

            case isa::Opcode::JRP:
                controlInstruction_ += store_[instr.lineOperand];
                break;

            case isa::Opcode::LDN:
                accumulator_ = -store_[instr.lineOperand];
                break;

            case isa::Opcode::STO:
                store_[instr.lineOperand] = accumulator_;
                break;

            case isa::Opcode::SUB:
                accumulator_ -= store_[instr.lineOperand];
                break;

            case isa::Opcode::CMP:
                if (isNegative(accumulator_)){
                    controlInstruction_ += 1;
                }
                break;

            case isa::Opcode::STOP:
                status_ = StepResult::Halted;
                break;

        }

        return status_;
    }

}
