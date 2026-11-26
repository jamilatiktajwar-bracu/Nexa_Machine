#include "nexa/vm.hpp"

#include <iostream>
#include <stdexcept>

namespace nexa {

void VM::loadProgram(std::vector<Instruction> program) {
    program_ = std::move(program);
    pc_ = 0;
    a_ = 0;
    b_ = 0;
    ram_.fill(0);
    halted_ = false;
}

void VM::run() {
    while (!halted_) {
        step();
    }
}

void VM::step() {
    if (pc_ >= program_.size()) {
        throw std::runtime_error("Program counter moved beyond program; missing HALT?");
    }

    const Instruction instruction = program_[pc_++];

    switch (instruction.opcode) {
        case Opcode::LoadImmediate:
            a_ = instruction.operand;
            break;
        case Opcode::Add:
            a_ = static_cast<std::uint8_t>(a_ + b_);
            break;
        case Opcode::Store:
            ram_[instruction.operand] = a_;
            break;
        case Opcode::Load:
            a_ = ram_[instruction.operand];
            break;
        case Opcode::Print:
            std::cout << "Output: " << static_cast<unsigned>(a_) << '\n';
            break;
        case Opcode::Halt:
            halted_ = true;
            break;
        default:
            throw std::runtime_error("Unknown opcode");
    }
}

} // namespace nexa
