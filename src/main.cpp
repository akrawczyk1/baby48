#include <string>
#include <array>
#include <iostream>
#include <expected>

#include "isa/isa.hpp"

int main(){
    // std::cout << "Hello, world!" << std::endl;

    // std::expected<int, std::string> test1 = 42;
    // std::expected<int, std::string> test2 = std::unexpected("error");

    // auto arr = std::to_array({test1, test2});

    // for (const auto& r : arr){
    //     if (r.has_value()){
    //         std::cout << r.value() << std::endl;
    //     }
    //     else{
    //         std::cout << r.error() << std::endl;
    //     }
    // }

    // std::cout << std::endl;

    // static_assert(baby48::isa::encode(baby48::isa::Opcode::LDN, 18) == 16402);
    // static_assert(baby48::isa::decode(baby48::isa::encode(baby48::isa::Opcode::LDN, 18)) == baby48::isa::Instruction{baby48::isa::Opcode::LDN, 18});

    return 0;
}