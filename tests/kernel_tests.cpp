#include "nexa/kernel.hpp"
#include <cassert>
#include <stdexcept>
int main() {
    nexa::NexaKernel kernel(1024);
    nexa::Executable basic{0,{{nexa::Opcode::LoadImmediate,65},{nexa::Opcode::Output,nexa::VM::DisplayPort},{nexa::Opcode::Halt,0}}};
    const auto first=kernel.spawn("alpha",basic);
    const auto second=kernel.spawn("beta",basic);
    assert(first!=second);
    kernel.filesystem().createFile("/7",{7});
    assert(!kernel.hasFileAccess(first,"/7",nexa::FilePermission::Read));
    kernel.grantFileAccess(first,"/7",nexa::FilePermission::Read);
    assert(kernel.hasFileAccess(first,"/7",nexa::FilePermission::Read));
    assert(!kernel.hasFileAccess(first,"/7",nexa::FilePermission::Write));
    assert(!kernel.hasFileAccess(second,"/7",nexa::FilePermission::Read));
    bool badPermissionPath=false;
    try { kernel.grantFileAccess(first,"/../7",nexa::FilePermission::Read); }
    catch(const std::invalid_argument&) { badPermissionPath=true; }
    assert(badPermissionPath);
    kernel.writeProcessMemory(first,20,42);
    assert(kernel.readProcessMemory(first,20)==42);
    assert(kernel.readProcessMemory(second,20)==0);
    bool bounds=false;
    try { kernel.writeProcessMemory(first,1024,1); } catch(const std::out_of_range&) { bounds=true; }
    assert(bounds);
    assert(kernel.scheduleOne()); assert(kernel.scheduleOne()); kernel.run();
    auto processes=kernel.processes();
    assert(processes.size()==2);
    assert(processes[0].state==nexa::ProcessState::Halted);
    assert(processes[1].state==nexa::ProcessState::Halted);
    assert(kernel.takeConsoleOutput()=="AA");
    assert(kernel.syscall(first,nexa::Syscall::GetPid)==(first&255));
    assert(kernel.syscall(first,nexa::Syscall::Write,'!')==0);
    assert(kernel.takeConsoleOutput()=="!");
    nexa::Executable guest{0,{{nexa::Opcode::LoadImmediate,1},{nexa::Opcode::LoadBImmediate,'H'},{nexa::Opcode::Syscall,0},{nexa::Opcode::LoadImmediate,0},{nexa::Opcode::Syscall,0}}};
    const auto guestPid=kernel.spawn("guest-syscall",guest);
    kernel.run();
    assert(kernel.takeConsoleOutput()=="H");
    processes=kernel.processes();
    bool guestHalted=false;
    for(const auto& process:processes) if(process.pid==guestPid) guestHalted=process.state==nexa::ProcessState::Halted;
    assert(guestHalted);
    nexa::Executable invalid{0,{{nexa::Opcode::LoadImmediate,99},{nexa::Opcode::Syscall,0}}};
    const auto badPid=kernel.spawn("invalid-syscall",invalid);
    // A faulting guest is isolated; the scheduler continues and records its fault.
    const auto healthyPid=kernel.spawn("still-healthy",basic);
    kernel.run();
    processes=kernel.processes();
    bool badFaulted=false,healthyHalted=false;
    for(const auto& process:processes) {
        if(process.pid==badPid) badFaulted=process.state==nexa::ProcessState::Faulted&&!process.faultReason.empty();
        if(process.pid==healthyPid) healthyHalted=process.state==nexa::ProcessState::Halted;
    }
    assert(badFaulted&&healthyHalted);
    assert(kernel.takeConsoleOutput()=="A");
}
