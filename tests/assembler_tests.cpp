#include "nexa/assembler.hpp"

#include <cassert>
#include <stdexcept>

int main() {
    const auto program = nexa::parseAssembly(
        "LOAD A, 0x10\n"
        "LOAD B, 2\n"
        "ADD A, B\n"
        "STORE [5]\n"
        "LOAD A, [5]\n"
        "PRINT A\n"
        "HALT\n");
    assert(program.size() == 7);
    assert(program[0].opcode == nexa::Opcode::LoadImmediate && program[0].operand == 16);
    assert(program[1].opcode == nexa::Opcode::LoadBImmediate && program[1].operand == 2);
    assert(program[2].opcode == nexa::Opcode::Add);
    assert(program[3].opcode == nexa::Opcode::Store && program[3].operand == 5);
    assert(program[4].opcode == nexa::Opcode::Load && program[4].operand == 5);
    assert(program[5].opcode == nexa::Opcode::Print);
    assert(program[6].opcode == nexa::Opcode::Halt);

    bool threw = false;
    try { (void)nexa::parseAssembly("BOGUS\n"); }
    catch (const std::runtime_error&) { threw = true; }
    assert(threw);

    threw = false;
    try { (void)nexa::parseAssembly("LOAD A, 999\n"); }
    catch (const std::runtime_error&) { threw = true; }
    assert(threw);
    return 0;
}
