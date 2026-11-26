#include "nexa/kernel.hpp"
#include "nexa/shell.hpp"
#include <cassert>
#include <cstdio>
#include <stdexcept>
int main() {
    nexa::NexaSecurity freshSecurity;
    nexa::NexaSecurity restoredFreshSecurity;
    restoredFreshSecurity.deserialize(freshSecurity.serialize());
    assert(restoredFreshSecurity.hasUser("root"));
    assert(!restoredFreshSecurity.authenticate("root", "not-configured"));


    nexa::NexaKernel schedulingKernel(512);
    const nexa::Executable looping{0,{{nexa::Opcode::Inc,0},{nexa::Opcode::Jump,0}}};
    const auto lowPid=schedulingKernel.spawn("low-priority",looping);
    const auto highPid=schedulingKernel.spawn("high-priority",looping);
    schedulingKernel.setPriority(lowPid,1);
    schedulingKernel.setPriority(highPid,7);
    schedulingKernel.setTimeQuantum(2);
    assert(schedulingKernel.scheduleOne());
    auto scheduled=schedulingKernel.processes();
    std::uint64_t lowTicks=0,highTicks=0;
    for(const auto& proc:scheduled){if(proc.pid==lowPid)lowTicks=proc.ticks;if(proc.pid==highPid)highTicks=proc.ticks;}
    assert(highTicks==2&&lowTicks==0);
    schedulingKernel.setTimerInterval(1);
    assert(schedulingKernel.scheduleOne());
    assert(schedulingKernel.systemTicks()==3&&schedulingKernel.timerInterrupts()==1);
    assert(schedulingKernel.pendingTimerInterrupts()==1);
    assert(schedulingKernel.acknowledgeTimerInterrupt());
    assert(!schedulingKernel.acknowledgeTimerInterrupt());
    const auto peerPid=schedulingKernel.spawn("equal-priority-peer",looping);
    schedulingKernel.setPriority(peerPid,7);
    assert(schedulingKernel.scheduleOne());
    scheduled=schedulingKernel.processes();
    bool peerRan=false;
    for(const auto& proc:scheduled)if(proc.pid==peerPid)peerRan=proc.ticks==1;
    assert(peerRan);
    bool invalidPriority=false;
    try{schedulingKernel.setPriority(peerPid,8);}catch(const std::invalid_argument&){invalidPriority=true;}
    assert(invalidPriority);

    nexa::NexaKernel agingKernel(512);
    const auto agedLowPid=agingKernel.spawn("aged-low",looping);
    const auto agingHighPid=agingKernel.spawn("always-ready-high",looping);
    agingKernel.setPriority(agedLowPid,0);
    agingKernel.setPriority(agingHighPid,7);
    agingKernel.setPriorityAgingThreshold(1);
    agingKernel.setTimeQuantum(1);
    for(int tick=0;tick<7;++tick) assert(agingKernel.scheduleOne());
    auto agedProcesses=agingKernel.processes();
    bool agingReachedMaximum=false;
    for(const auto& proc:agedProcesses)if(proc.pid==agedLowPid)
        agingReachedMaximum=proc.effectivePriority==7&&proc.readyWaitTicks==7;
    assert(agingReachedMaximum);
    assert(agingKernel.scheduleOne());
    agedProcesses=agingKernel.processes();
    bool agedProcessRan=false;
    for(const auto& proc:agedProcesses)if(proc.pid==agedLowPid)agedProcessRan=proc.ticks==1;
    assert(agedProcessRan);

    nexa::NexaShell schedulerShell(schedulingKernel);
    assert(schedulerShell.execute("nice "+std::to_string(lowPid)+" 2")=="priority updated");
    assert(schedulerShell.execute("aging 1")=="priority aging updated");
    assert(schedulerShell.execute("sched").find("policy=fixed-priority-round-robin")!=std::string::npos);
    assert(schedulerShell.execute("sched").find("aging_threshold=1")!=std::string::npos);
    assert(schedulerShell.execute("ps").find("priority=2")!=std::string::npos);
    assert(schedulerShell.execute("ps").find("effective_priority=")!=std::string::npos);
    assert(schedulerShell.execute("nice "+std::to_string(lowPid)+" 9")=="invalid PID or priority (0-7)");

    nexa::NexaKernel yieldKernel(256);
    const auto yieldingPid=yieldKernel.spawn("voluntary-yield",nexa::Executable{0,{
        {nexa::Opcode::LoadImmediate,static_cast<std::uint16_t>(nexa::Syscall::Yield)},
        {nexa::Opcode::Syscall,0},{nexa::Opcode::LoadImmediate,'Y'},{nexa::Opcode::Halt,0}}});
    yieldKernel.setTimeQuantum(4);
    assert(yieldKernel.scheduleOne());
    auto yieldProcesses=yieldKernel.processes();
    bool yieldedAfterSyscall=false;
    for(const auto& proc:yieldProcesses)if(proc.pid==yieldingPid)
        yieldedAfterSyscall=proc.state==nexa::ProcessState::Ready&&proc.ticks==2;
    assert(yieldedAfterSyscall);
    assert(yieldKernel.scheduleOne());
    yieldProcesses=yieldKernel.processes();
    bool yieldedProcessFinished=false;
    for(const auto& proc:yieldProcesses)if(proc.pid==yieldingPid)
        yieldedProcessFinished=proc.state==nexa::ProcessState::Halted&&proc.ticks==4;
    assert(yieldedProcessFinished);


    nexa::NexaKernel shellMemoryKernel(256);
    nexa::NexaShell memoryShell(shellMemoryKernel);
    assert(memoryShell.execute("memstat").find("frames_total=")!=std::string::npos);
    assert(memoryShell.execute("swapfile /tmp/nexa_shell_swap.img")=="disk swap configured");
    assert(shellMemoryKernel.memoryManager().swapFilePath()=="/tmp/nexa_shell_swap.img");
    assert(memoryShell.execute("memstat").find("swap_backend=disk")!=std::string::npos);
    std::remove("/tmp/nexa_shell_swap.img");
    nexa::NexaKernel processControlKernel(256);
    const auto framesBeforeManagedProcess=processControlKernel.memoryManager().freeFrames();
    const auto managedPid=processControlKernel.spawn("managed-loop",looping);
    nexa::NexaShell processShell(processControlKernel);
    assert(processShell.execute("kill "+std::to_string(managedPid))=="process terminated");
    auto managedProcesses=processControlKernel.processes();
    assert(managedProcesses.size()==1&&managedProcesses.front().state==nexa::ProcessState::Halted);
    assert(processShell.execute("reap "+std::to_string(managedPid))=="process reaped");
    assert(processControlKernel.processes().empty());
    assert(processControlKernel.memoryManager().freeFrames()==framesBeforeManagedProcess);
    assert(processShell.execute("reap "+std::to_string(managedPid))=="unknown or active process");

    nexa::NexaKernel ipcKernel(256);
    const auto ipcSender=ipcKernel.spawn("ipc-host-sender",nexa::Executable{0,{{nexa::Opcode::Halt,0}}});
    const auto ipcReceiver=ipcKernel.spawn("ipc-host-receiver",nexa::Executable{0,{{nexa::Opcode::Halt,0}}});
    assert(ipcKernel.sendMessage(ipcSender,ipcReceiver,{1,2,3}));
    auto ipcMessage=ipcKernel.receiveMessage(ipcReceiver);
    assert(ipcMessage&&ipcMessage->sender==ipcSender&&ipcMessage->payload==std::vector<std::uint8_t>({1,2,3}));
    assert(!ipcKernel.receiveMessage(ipcReceiver));
    std::vector<std::uint8_t> oversized(252,0);
    assert(!ipcKernel.sendMessage(ipcSender,ipcReceiver,std::move(oversized)));

    nexa::NexaKernel guestIpcKernel(256);
    const auto guestReceiver=guestIpcKernel.spawn("guest-receiver",nexa::Executable{0,{
        {nexa::Opcode::LoadImmediate,static_cast<std::uint16_t>(nexa::Syscall::ReceiveMessage)},
        {nexa::Opcode::LoadBImmediate,100},{nexa::Opcode::Syscall,0},
        {nexa::Opcode::Load,105},{nexa::Opcode::Output,nexa::VM::DisplayPort},
        {nexa::Opcode::Halt,0}}});
    const auto guestSender=guestIpcKernel.spawn("guest-sender",nexa::Executable{0,{
        {nexa::Opcode::LoadImmediate,static_cast<std::uint16_t>(guestReceiver&0xFF)},{nexa::Opcode::Store,100},
        {nexa::Opcode::LoadImmediate,static_cast<std::uint16_t>((guestReceiver>>8)&0xFF)},{nexa::Opcode::Store,101},
        {nexa::Opcode::LoadImmediate,1},{nexa::Opcode::Store,102},
        {nexa::Opcode::LoadImmediate,'Z'},{nexa::Opcode::Store,103},
        {nexa::Opcode::LoadImmediate,static_cast<std::uint16_t>(nexa::Syscall::SendMessage)},
        {nexa::Opcode::LoadBImmediate,100},{nexa::Opcode::Syscall,0},{nexa::Opcode::Halt,0}}});
    guestIpcKernel.setPriority(guestSender,1);
    guestIpcKernel.setPriority(guestReceiver,7);
    guestIpcKernel.setTimeQuantum(32);
    assert(guestIpcKernel.scheduleOne());
    auto guestIpcProcesses=guestIpcKernel.processes();
    bool receiverBlocked=false;
    for(const auto& proc:guestIpcProcesses)if(proc.pid==guestReceiver)receiverBlocked=proc.state==nexa::ProcessState::Blocked;
    assert(receiverBlocked&&!guestIpcKernel.hasDeadlock());
    assert(guestIpcKernel.scheduleOne());
    guestIpcProcesses=guestIpcKernel.processes();
    bool receiverWoken=false;
    for(const auto& proc:guestIpcProcesses)if(proc.pid==guestReceiver)receiverWoken=proc.state==nexa::ProcessState::Ready;
    assert(receiverWoken);
    bool guestMessageQueued=false;
    for(const auto& proc:guestIpcProcesses)if(proc.pid==guestReceiver)guestMessageQueued=proc.pendingMessages==1;
    assert(guestMessageQueued);
    guestIpcKernel.run();
    guestIpcProcesses=guestIpcKernel.processes();
    for(const auto& proc:guestIpcProcesses)
        if(proc.pid==guestReceiver&&proc.state==nexa::ProcessState::Faulted)
            throw std::runtime_error("guest IPC receiver fault: "+proc.faultReason);
    const auto guestIpcOutput=guestIpcKernel.takeConsoleOutput();
    if(guestIpcOutput!="Z") {
        std::string diagnostic="guest IPC output mismatch; bytes="+std::to_string(guestIpcOutput.size());
        for(const auto& proc:guestIpcProcesses)
            diagnostic+=" pid="+std::to_string(proc.pid)+" state="+std::to_string(static_cast<int>(proc.state))+
                " ticks="+std::to_string(proc.ticks)+" inbox="+std::to_string(proc.pendingMessages)+
                " fault="+proc.faultReason;
        throw std::runtime_error(diagnostic);
    }

    nexa::NexaKernel blockedKernel(256);
    const auto waitingPid=blockedKernel.spawn("waiting-reader",nexa::Executable{0,{
        {nexa::Opcode::LoadImmediate,static_cast<std::uint16_t>(nexa::Syscall::ReceiveMessage)},
        {nexa::Opcode::LoadBImmediate,100},{nexa::Opcode::Syscall,0},
        {nexa::Opcode::Load,105},{nexa::Opcode::Output,nexa::VM::DisplayPort},
        {nexa::Opcode::Halt,0}}});
    blockedKernel.setTimeQuantum(8);
    assert(blockedKernel.scheduleOne());
    assert(blockedKernel.hasDeadlock());
    assert(!blockedKernel.scheduleOne());
    const auto wakeSource=blockedKernel.spawn("wake-source",nexa::Executable{0,{{nexa::Opcode::Halt,0}}});
    assert(!blockedKernel.hasDeadlock());
    assert(blockedKernel.sendMessage(wakeSource,waitingPid,{'A'}));
    assert(blockedKernel.scheduleOne());
    blockedKernel.run();
    assert(blockedKernel.takeConsoleOutput()=="A");

    // Inbox capacity is bounded and FIFO; a full mailbox never overwrites messages.
    for(std::uint8_t value=0;value<64;++value)
        assert(ipcKernel.sendMessage(ipcSender,ipcReceiver,{value}));
    assert(!ipcKernel.sendMessage(ipcSender,ipcReceiver,{99}));
    auto ipcProcesses=ipcKernel.processes();
    bool inboxFull=false;
    for(const auto& proc:ipcProcesses)if(proc.pid==ipcReceiver)inboxFull=proc.pendingMessages==64;
    assert(inboxFull);
    for(std::uint8_t value=0;value<64;++value) {
        auto message=ipcKernel.receiveMessage(ipcReceiver);
        assert(message&&message->payload.size()==1&&message->payload[0]==value);
    }


    nexa::NexaKernel pagingKernel(1024);
    const auto pagingPid=pagingKernel.spawn("paged-load-store",nexa::Executable{0,{
        {nexa::Opcode::LoadImmediate,90},{nexa::Opcode::Store,200},
        {nexa::Opcode::Load,200},{nexa::Opcode::Output,nexa::VM::DisplayPort},
        {nexa::Opcode::Halt,0}}});
    assert(pagingKernel.memoryManager().mappedPages(pagingPid)==0);
    pagingKernel.run();
    assert(pagingKernel.memoryManager().read(pagingPid,200,nexa::MemoryAccess::Kernel)==90);
    assert(pagingKernel.memoryManager().mappedPages(pagingPid)==1);
    assert(pagingKernel.memoryManager().pageFaults()>=1);
    assert(pagingKernel.takeConsoleOutput()=="Z");
    const auto executeFaultPid=pagingKernel.spawn("execute-protection",nexa::Executable{0,{
        {nexa::Opcode::Halt,0}}});
    const auto executeSpace=executeFaultPid|0x80000000u;
    assert(pagingKernel.memoryManager().canExecute(executeSpace,0));
    assert(pagingKernel.memoryManager().read(executeSpace,0,nexa::MemoryAccess::Kernel)==static_cast<std::uint8_t>(nexa::Opcode::Halt));
    bool codeWriteDenied=false;
    try { pagingKernel.memoryManager().write(executeSpace,0,0xFF); } catch(const std::runtime_error&) { codeWriteDenied=true; }
    assert(codeWriteDenied);
    pagingKernel.memoryManager().protectPage(executeSpace,0,false,true,false);
    pagingKernel.run();
    bool executeFault=false;
    for(const auto& p:pagingKernel.processes())
        if(p.pid==executeFaultPid) executeFault=p.state==nexa::ProcessState::Faulted&&p.faultReason.find("execute protection")!=std::string::npos;
    assert(executeFault);
    const auto protectedPid=pagingKernel.spawn("protected-store",nexa::Executable{0,{
        {nexa::Opcode::LoadImmediate,1},{nexa::Opcode::Store,0},{nexa::Opcode::Halt,0}}});
    pagingKernel.memoryManager().mapPage(protectedPid,0,true,false,false);
    pagingKernel.memoryManager().protectPage(protectedPid,0,true,false,false);
    pagingKernel.run();
    bool protectedFault=false;
    for(const auto& p:pagingKernel.processes())
        if(p.pid==protectedPid) protectedFault=p.state==nexa::ProcessState::Faulted&&p.faultReason.find("supervisor-only")!=std::string::npos;
    assert(protectedFault);
    nexa::NexaKernel reclaimKernel(1024);
    const auto freeBefore=reclaimKernel.memoryManager().freeFrames();
    const auto reclaimPid=reclaimKernel.spawn("reclaim",nexa::Executable{0,{
        {nexa::Opcode::LoadImmediate,7},{nexa::Opcode::Store,9},{nexa::Opcode::Halt,0}}});
    reclaimKernel.run();
    assert(reclaimKernel.memoryManager().freeFrames()<freeBefore);
    assert(reclaimKernel.reapProcess(reclaimPid));
    assert(reclaimKernel.memoryManager().freeFrames()==freeBefore);
    assert(reclaimKernel.processes().empty());
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
