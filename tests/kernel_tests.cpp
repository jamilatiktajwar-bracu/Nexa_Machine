#include "nexa/kernel.hpp"
#include "nexa/shell.hpp"
#include "nexa/virtual_memory.hpp"
#include <cassert>
#include <cstdio>
#include <stdexcept>
int main() {
    // Virtual memory: independent page tables, access flags, page faults and frame reuse.
    nexa::VirtualMemoryManager vmm(3*nexa::VirtualMemoryManager::PageSize);
    assert(vmm.physicalBytes()==3*nexa::VirtualMemoryManager::PageSize);
    assert(vmm.freeFrames()==3);
    vmm.createAddressSpace(10);
    vmm.createAddressSpace(11);
    vmm.mapPage(10,0x0000,nexa::PageAccess::Read|nexa::PageAccess::Write);
    vmm.mapPage(11,0x0000,nexa::PageAccess::Read|nexa::PageAccess::Execute);
    assert(vmm.mappedPages(10)==1&&vmm.mappedPages(11)==1);
    vmm.write8(10,0x0012,0xA5);
    assert(vmm.read8(10,0x0012)==0xA5);
    assert(vmm.read8(11,0x0012)==0);
    bool denied=false;
    try { vmm.write8(11,0x0012,0xFF); } catch(const std::runtime_error&) { denied=true; }
    assert(denied);
    bool unmapped=false;
    try { (void)vmm.read8(10,0x1000); } catch(const std::out_of_range&) { unmapped=true; }
    assert(unmapped);
    bool unaligned=false;
    try { vmm.mapPage(10,1,nexa::PageAccess::Read); } catch(const std::invalid_argument&) { unaligned=true; }
    assert(unaligned);
    vmm.mapPage(10,0x1000,nexa::PageAccess::Read);
    bool exhausted=false;
    try { vmm.mapPage(10,0x2000,nexa::PageAccess::Read); } catch(const std::runtime_error&) { exhausted=true; }
    assert(exhausted);
    vmm.unmapPage(10,0x1000);
    assert(vmm.freeFrames()==1);
    vmm.mapPage(10,0x2000,nexa::PageAccess::Read|nexa::PageAccess::Write);
    vmm.destroyAddressSpace(10);
    assert(!vmm.hasAddressSpace(10));
    assert(vmm.freeFrames()==2);
    vmm.destroyAddressSpace(11);
    assert(vmm.freeFrames()==3);

    nexa::NexaSecurity freshSecurity;
    nexa::NexaSecurity restoredFreshSecurity;
    restoredFreshSecurity.deserialize(freshSecurity.serialize());
    assert(restoredFreshSecurity.hasUser("root"));
    assert(!restoredFreshSecurity.authenticate("root", "not-configured"));

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
    kernel.writeProcessMemory(first,100,'/');
    kernel.writeProcessMemory(first,101,'7');
    kernel.writeProcessMemory(first,102,0);
    const auto readHandle=kernel.syscall(first,nexa::Syscall::OpenRead,100);
    assert(readHandle!=0);
    assert(kernel.syscall(first,nexa::Syscall::ReadByte,readHandle)==7);
    assert(kernel.syscall(first,nexa::Syscall::ReadByte,readHandle)==0);
    assert(kernel.syscall(first,nexa::Syscall::CloseFile,readHandle)==0);
    kernel.grantFileAccess(first,"/7",nexa::FilePermission::Write);
    const auto writeHandle=kernel.syscall(first,nexa::Syscall::OpenWrite,100);
    kernel.writeProcessMemory(first,110,writeHandle);
    kernel.writeProcessMemory(first,111,88);
    assert(kernel.syscall(first,nexa::Syscall::WriteByte,110)==0);
    assert(kernel.filesystem().readFile("/7")==std::vector<std::uint8_t>{88});
    assert(kernel.syscall(first,nexa::Syscall::CloseFile,writeHandle)==0);
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
    nexa::NexaShell shell(kernel);
    assert(shell.execute("pwd")=="/");
    assert(shell.execute("whoami")=="root");
    assert(shell.execute("passwd root-secret")=="password updated");
    assert(shell.execute("useradd guest guest-secret")=="created user guest");
    assert(shell.execute("login guest wrong-secret")=="authentication failed");
    assert(shell.execute("write /shared.txt hello")=="wrote 5 bytes");
    assert(shell.execute("login guest guest-secret")=="logged in as guest");
    assert(shell.execute("cat /shared.txt")=="hello");
    assert(shell.execute("write /shared.txt blocked")=="permission denied");
    assert(shell.execute("login root root-secret")=="logged in as root");
    assert(shell.execute("chmod 600 /shared.txt")=="mode updated");
    assert(shell.execute("login guest guest-secret")=="logged in as guest");
    assert(shell.execute("cat /shared.txt")=="permission denied");
    assert(shell.execute("login root root-secret")=="logged in as root");
    assert(shell.execute("chown guest /shared.txt")=="owner updated");
    assert(shell.execute("login guest guest-secret")=="logged in as guest");
    assert(shell.execute("write /shared.txt allowed")=="wrote 7 bytes");
    assert(shell.execute("login root root-secret")=="logged in as root");
    for(int attempt=0;attempt<4;++attempt) assert(shell.execute("login guest bad-password")=="authentication failed");
    assert(shell.execute("login guest bad-password")=="account locked; contact root");
    assert(shell.execute("login guest guest-secret")=="account locked; contact root");
    assert(shell.execute("passwd guest renewed-secret")=="password reset");
    assert(shell.execute("login guest renewed-secret")=="logged in as guest");
    assert(shell.execute("login root root-secret")=="logged in as root");
    assert(shell.execute("groupadd developers")=="created group developers");
    assert(shell.execute("groupadd qa")=="created group qa");
    assert(shell.execute("usermod -aG qa guest")=="supplementary group added");
    assert(shell.execute("groups guest")=="guest qa");
    assert(shell.execute("usermod -aG qa guest")=="unknown user/group or membership exists");
    assert(shell.execute("usermod -rG qa guest")=="supplementary group removed");
    assert(shell.execute("groups guest")=="guest");
    assert(shell.execute("usermod -aG qa guest")=="supplementary group added");
    assert(shell.execute("usermod -g developers guest")=="primary group updated");
    assert(shell.execute("groups guest")=="developers qa");
    assert(shell.execute("write /team.txt team-data")=="wrote 9 bytes");
    assert(shell.execute("chgrp developers /team.txt")=="group updated");
    assert(shell.execute("chmod 640 /team.txt")=="mode updated");
    assert(shell.execute("login guest renewed-secret")=="logged in as guest");
    assert(shell.execute("cat /team.txt")=="team-data");
    assert(shell.execute("write /team.txt denied")=="permission denied");
    assert(shell.execute("login root root-secret")=="logged in as root");
    assert(shell.execute("chmod 660 /team.txt")=="mode updated");
    assert(shell.execute("login guest renewed-secret")=="logged in as guest");
    assert(shell.execute("write /team.txt shared")=="wrote 6 bytes");
    assert(shell.execute("login root root-secret")=="logged in as root");
    assert(shell.execute("savefs /tmp/nexa_security_test.img")=="filesystem image saved");
    nexa::NexaKernel restoredKernel(1024);
    nexa::NexaShell restoredShell(restoredKernel);
    assert(restoredShell.execute("loadfs /tmp/nexa_security_test.img")=="filesystem image loaded");
    assert(restoredShell.execute("whoami")=="root");
    assert(restoredShell.execute("login guest renewed-secret")=="logged in as guest");
    assert(restoredShell.execute("cat /shared.txt")=="allowed");
    assert(restoredShell.execute("write /shared.txt persisted")=="wrote 9 bytes");
    std::remove("/tmp/nexa_security_test.img");
    assert(shell.execute("login root root-secret")=="logged in as root");
    assert(shell.execute("mkdir /apps")=="created directory /apps");
    assert(shell.execute("cd /apps")=="/apps");
    assert(shell.execute("pwd")=="/apps");
    assert(shell.execute("write hello.nexa HALT")=="wrote 4 bytes");
    assert(shell.execute("cat hello.nexa").find("HALT")==0);
    assert(shell.execute("ls").find("hello.nexa")!=std::string::npos);
    assert(shell.execute("run hello.nexa")=="");
    assert(shell.execute("ps").find("hello.nexa halted")!=std::string::npos);
    assert(shell.execute("rm hello.nexa")=="removed /apps/hello.nexa");
    assert(shell.execute("cd ..")=="/");
    assert(shell.execute("chmod 700 /apps")=="mode updated");
    assert(shell.execute("login guest renewed-secret")=="logged in as guest");
    assert(shell.execute("ls /apps")=="permission denied");
    assert(shell.execute("cd /apps")=="permission denied");
    assert(shell.execute("touch /apps/blocked.txt")=="permission denied");
    assert(shell.execute("login root root-secret")=="logged in as root");
    assert(shell.execute("logout")=="session closed");
    assert(shell.execute("whoami")=="(not logged in)");
    assert(shell.execute("pwd")=="session locked; login first");
    assert(shell.execute("ls")=="session locked; login first");
    assert(shell.execute("login guest renewed-secret")=="logged in as guest");
    assert(shell.execute("whoami")=="guest");
    assert(shell.execute("pwd")=="/");
}
