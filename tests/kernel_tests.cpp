#include "nexa/kernel.hpp"
#include <cassert>
#include <stdexcept>
int main() {
    nexa::NexaKernel kernel(1024);
    nexa::Executable basic{0, {
        {nexa::Opcode::LoadImmediate, 65},
        {nexa::Opcode::Output, nexa::VM::DisplayPort},
        {nexa::Opcode::Halt, 0}
    }};
    const auto first = kernel.spawn("alpha", basic);
    const auto second = kernel.spawn("beta", basic);
    assert(first != second);
    assert(kernel.scheduleOne());
    assert(kernel.scheduleOne());
    kernel.run();
    auto processes = kernel.processes();
    assert(processes.size() == 2);
    assert(processes[0].state == nexa::ProcessState::Halted);
    assert(processes[1].state == nexa::ProcessState::Halted);
    assert(kernel.takeConsoleOutput() == "AA");
    assert(kernel.syscall(first, nexa::Syscall::GetPid) == (first & 255));
    assert(kernel.syscall(first, nexa::Syscall::Write, '!') == 0);
    assert(kernel.takeConsoleOutput() == "!");

    // A guest program writes 'H' through SYS, then exits through SYS.
    nexa::Executable guest{0, {
        {nexa::Opcode::LoadImmediate, 1},
        {nexa::Opcode::LoadBImmediate, 'H'},
        {nexa::Opcode::Syscall, 0},
        {nexa::Opcode::LoadImmediate, 0},
        {nexa::Opcode::Syscall, 0}
    }};
    const auto guestPid = kernel.spawn("guest-syscall", guest);
    kernel.run();
    assert(kernel.takeConsoleOutput() == "H");
    processes = kernel.processes();
    bool guestHalted = false;
    for (const auto& process : processes)
        if (process.pid == guestPid)
            guestHalted = process.state == nexa::ProcessState::Halted;
    assert(guestHalted);

    bool rejected = false;
    try {
        nexa::Executable invalid{0, {{nexa::Opcode::LoadImmediate, 99}, {nexa::Opcode::Syscall, 0}}};
        kernel.spawn("invalid-syscall", invalid);
        kernel.run();
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);
}
