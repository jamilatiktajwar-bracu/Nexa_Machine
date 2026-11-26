#include "nexa/vm.hpp"

#include <exception>
#include <iostream>
#include <vector>

int main() {
    std::cout << "Nexa Machine VM starting...\n";

    // Demo program: 19 + 23, print the result, and halt.
    const std::vector<nexa::Instruction> program{
        {nexa::Opcode::LoadImmediate, 19},
        {nexa::Opcode::LoadBImmediate, 23},
        {nexa::Opcode::Add, 0},
        {nexa::Opcode::Print, 0},
        {nexa::Opcode::Halt, 0},
    };

    nexa::VM vm;
    vm.loadProgram(program);

    try {
        vm.run();
    } catch (const std::exception& error) {
        std::cerr << "VM error: " << error.what() << '\n';
        return 1;
    }

    std::cout << "VM halted.\n";
    return 0;
}
