

#include "machine/machine.hpp"

namespace baby48::machine{

    // Constructor / Destructor
    Machine::Machine(const memory::MemoryImage& image):
        store_(image){}

    StepResult Machine::step(){
        //placeholder implementation
        return status_;
    }

}
