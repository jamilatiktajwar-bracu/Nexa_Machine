#include "nexa/kernel.hpp"
#include <algorithm>
#include <exception>
#include <stdexcept>
#include <limits>
namespace nexa {
NexaKernel::NexaKernel(std::size_t n):ramBytes_(n),memory_(std::max<std::size_t>(VirtualMemoryManager::PageSize,((n+VirtualMemoryManager::PageSize-1)/VirtualMemoryManager::PageSize)*VirtualMemoryManager::PageSize*16)){
    VM check(n); (void)check;
    memory_.setPageFaultHandler([this](std::uint32_t pid,std::uint8_t page){
        if((pid&0x80000000u)!=0) throw std::runtime_error("Execute address space page fault");
        memory_.mapPage(pid,page,true,true,false);
    });
}
std::uint32_t NexaKernel::spawn(const std::string& name,const Executable& image) {
    if(name.empty()) throw std::invalid_argument("Process name cannot be empty");
    if(image.code.empty()||image.entryPoint>=image.code.size()||image.code.size()>65536/3) throw std::invalid_argument("Invalid process executable");
    const auto pid=nextPid_++;
    Process p{ProcessInfo{pid,name,ProcessState::Ready,0,{}},VM(ramBytes_)};
    p.machine.loadProgram(image.code);
    p.machine.setProgramCounter(image.entryPoint);
    p.machine.setSyscallHandler([this,pid](std::uint8_t service,std::uint8_t argument) {
        if(service>static_cast<std::uint8_t>(Syscall::ReceiveMessage)) throw std::runtime_error("Unknown kernel syscall");
        return syscall(pid,static_cast<Syscall>(service),argument);
    });
    const auto codeSpace=pid|0x80000000u;
    memory_.createAddressSpace(pid);
    bool codeSpaceCreated=false;
    try {
        memory_.createAddressSpace(codeSpace);
        codeSpaceCreated=true;
        const auto codePages=((image.code.size()*3)+VirtualMemoryManager::PageSize-1)/VirtualMemoryManager::PageSize;
        for(std::size_t page=0;page<codePages;++page)
            memory_.mapPage(codeSpace,static_cast<std::uint8_t>(page),true,true,true);
        for(std::size_t index=0;index<image.code.size();++index) {
            const auto address=index*3;
            const auto& instruction=image.code[index];
            memory_.write(codeSpace,static_cast<std::uint16_t>(address),static_cast<std::uint8_t>(instruction.opcode),MemoryAccess::Kernel);
            memory_.write(codeSpace,static_cast<std::uint16_t>(address+1),static_cast<std::uint8_t>(instruction.operand&0xFF),MemoryAccess::Kernel);
            memory_.write(codeSpace,static_cast<std::uint16_t>(address+2),static_cast<std::uint8_t>(instruction.operand>>8),MemoryAccess::Kernel);
        }
        for(std::size_t page=0;page<codePages;++page)
            memory_.protectPage(codeSpace,static_cast<std::uint8_t>(page),false,true,true);
    } catch(...) {
        if(codeSpaceCreated) memory_.destroyAddressSpace(codeSpace);
        memory_.destroyAddressSpace(pid);
        throw;
    }
    p.machine.setMemoryAccessors(
        [this,pid](std::uint16_t address){return memory_.read(pid,address,MemoryAccess::User);},
        [this,pid](std::uint16_t address,std::uint8_t value){memory_.write(pid,address,value,MemoryAccess::User);});
    p.machine.setInstructionFetchGuard([this,codeSpace](std::uint16_t instructionIndex){
        const auto byteAddress=static_cast<std::size_t>(instructionIndex)*3;
        if(byteAddress>65533) throw std::runtime_error("Instruction address exceeds code memory");
        (void)memory_.canExecute(codeSpace,static_cast<std::uint16_t>(byteAddress),MemoryAccess::User);
    });
    p.machine.setInstructionReader([this,codeSpace](std::uint16_t instructionIndex){
        const auto address=static_cast<std::size_t>(instructionIndex)*3;
        if(address>65533) throw std::runtime_error("Instruction address exceeds code memory");
        std::uint8_t bytes[3]{};
        for(std::size_t offset=0;offset<3;++offset) {
            const auto virtualAddress=static_cast<std::uint16_t>(address+offset);
            (void)memory_.canExecute(codeSpace,virtualAddress,MemoryAccess::User);
            bytes[offset]=memory_.read(codeSpace,virtualAddress,MemoryAccess::Kernel);
        }
        if(bytes[0]>static_cast<std::uint8_t>(Opcode::Halt)) throw std::runtime_error("Invalid instruction opcode in guest memory");
        const auto operand=static_cast<std::uint16_t>(bytes[1]|(static_cast<std::uint16_t>(bytes[2])<<8));
        return Instruction{static_cast<Opcode>(bytes[0]),operand};
    });
    processes_.emplace(pid,std::move(p));
    filePermissions_[pid] = {};
    return pid;
}
bool NexaKernel::hasDeadlock() const noexcept {
    bool blocked=false;
    for(const auto& [pid,proc]:processes_) {
        (void)pid;
        if(proc.info.state==ProcessState::Ready||proc.info.state==ProcessState::Running) return false;
        if(proc.info.state==ProcessState::Blocked) blocked=true;
    }
    return blocked;
}
bool NexaKernel::terminateProcess(std::uint32_t pid) {
    auto it=processes_.find(pid);
    if(it==processes_.end()) return false;
    if(it->second.info.state==ProcessState::Halted||it->second.info.state==ProcessState::Faulted) return false;
    it->second.machine.requestHalt();
    it->second.info.state=ProcessState::Halted;
    it->second.info.exitCode=137;
    it->second.info.terminationReason="killed";
    it->second.openFiles.clear();
    it->second.inbox.clear();
    if(currentPid_==pid) currentPid_=0;
    return true;
}
bool NexaKernel::reapProcess(std::uint32_t pid) {
    auto it=processes_.find(pid);
    if(it==processes_.end()) return false;
    if(it->second.info.state!=ProcessState::Halted&&it->second.info.state!=ProcessState::Faulted) return false;
    memory_.destroyAddressSpace(pid);
    const auto codeSpace=pid|0x80000000u;
    memory_.destroyAddressSpace(codeSpace);
    filePermissions_.erase(pid);
    processes_.erase(it);
    if(currentPid_==pid) currentPid_=0;
    return true;
}
void NexaKernel::setPriority(std::uint32_t pid,std::uint8_t priority) {
    if(priority>7) throw std::invalid_argument("Process priority must be between 0 and 7");
    auto it=processes_.find(pid);
    if(it==processes_.end()) throw std::out_of_range("Unknown process id");
    it->second.info.priority=priority;
    it->second.readyWaitTicks=0;
}
void NexaKernel::setTimeQuantum(std::size_t instructions) {
    if(instructions==0||instructions>1000000) throw std::invalid_argument("Time quantum must be between 1 and 1000000 instructions");
    timeQuantum_=instructions;
}
void NexaKernel::setPriorityAgingThreshold(std::uint64_t ticks) {
    priorityAgingThreshold_=ticks;
    for(auto& [id,proc]:processes_) { (void)id; proc.readyWaitTicks=0; }
}
void NexaKernel::setTimerInterval(std::uint64_t ticks) {
    timerInterval_=ticks;
}
bool NexaKernel::acknowledgeTimerInterrupt() noexcept {
    if(pendingTimerInterrupts_==0) return false;
    --pendingTimerInterrupts_;
    return true;
}
bool NexaKernel::sendMessage(std::uint32_t from,std::uint32_t to,std::vector<std::uint8_t> payload) {
    auto sender=processes_.find(from),receiver=processes_.find(to);
    if(sender==processes_.end()||receiver==processes_.end()) throw std::out_of_range("Unknown IPC process id");
    if(sender->second.info.state==ProcessState::Halted||sender->second.info.state==ProcessState::Faulted) return false;
    if(receiver->second.info.state==ProcessState::Halted||receiver->second.info.state==ProcessState::Faulted) return false;
    if(payload.size()>MaxIpcPayload||receiver->second.inbox.size()>=MaxInboxMessages) return false;
    receiver->second.inbox.push_back(IpcMessage{from,std::move(payload)});
    if(receiver->second.info.state==ProcessState::Blocked) receiver->second.info.state=ProcessState::Ready;
    return true;
}
std::optional<IpcMessage> NexaKernel::receiveMessage(std::uint32_t pid) {
    auto it=processes_.find(pid);
    if(it==processes_.end()) throw std::out_of_range("Unknown IPC process id");
    if(it->second.inbox.empty()) return std::nullopt;
    auto message=std::move(it->second.inbox.front());
    it->second.inbox.pop_front();
    return message;
}
bool NexaKernel::scheduleOne() {
    if(processes_.empty()) return false;
    std::vector<std::uint32_t> ids;
    std::uint8_t highestPriority=0;
    bool haveReady=false;
    const auto effectivePriority=[this](const Process& candidate) {
        if(priorityAgingThreshold_==0) return candidate.info.priority;
        const auto boost=candidate.readyWaitTicks/priorityAgingThreshold_;
        const auto base=candidate.info.priority;
        if(boost>=static_cast<std::uint64_t>(7-base)) return std::uint8_t{7};
        return static_cast<std::uint8_t>(base+boost);
    };
    for(const auto& [id,proc]:processes_) {
        if(proc.info.state!=ProcessState::Ready) continue;
        const auto effective=effectivePriority(proc);
        if(!haveReady||effective>highestPriority) {
            ids.clear(); highestPriority=effective; haveReady=true;
        }
        if(effective==highestPriority) ids.push_back(id);
    }
    if(ids.empty()) return false;
    auto it=std::find_if(ids.begin(),ids.end(),[&](auto id){return id>cursor_;});
    const auto pid=it==ids.end()?ids.front():*it;
    cursor_=pid; currentPid_=pid;
    auto& proc=processes_.at(pid);
    proc.info.state=ProcessState::Running;
    proc.readyWaitTicks=0;
    proc.yieldRequested=false;
    for(std::size_t instruction=0;instruction<timeQuantum_;++instruction) {
        bool timerFired=false;
        try {
            proc.machine.stepInstruction();
            ++proc.info.ticks;
            ++systemTicks_;
            for(auto& [waitingPid,waiting]:processes_) {
                if(waitingPid!=pid&&waiting.info.state==ProcessState::Ready&&waiting.readyWaitTicks<std::numeric_limits<std::uint64_t>::max())
                    ++waiting.readyWaitTicks;
            }
            if(timerInterval_!=0&&systemTicks_%timerInterval_==0) {
                ++timerInterrupts_;
                ++pendingTimerInterrupts_;
                timerFired=true;
            }
            output_+=proc.machine.deviceOutput();
            proc.machine.clearDeviceOutput();
            if(proc.machine.halted()||proc.yieldRequested||proc.info.state==ProcessState::Blocked) break;
            if(timerFired) break;
        } catch(const std::exception& e) {
            proc.info.state=ProcessState::Faulted;
            proc.info.terminationReason="fault";
            proc.info.faultReason=e.what();
            output_+=proc.machine.deviceOutput();
            proc.machine.clearDeviceOutput();
            break;
        } catch(...) {
            proc.info.state=ProcessState::Faulted;
            proc.info.terminationReason="fault";
            proc.info.faultReason="Unknown process fault";
            break;
        }
    }
    if(proc.info.state!=ProcessState::Faulted&&proc.info.state!=ProcessState::Blocked) {
        proc.info.state=proc.machine.halted()?ProcessState::Halted:ProcessState::Ready;
        if(proc.info.state==ProcessState::Halted&&proc.info.terminationReason.empty())
            proc.info.terminationReason="halt";
    }
    if(proc.info.state==ProcessState::Faulted||proc.info.state==ProcessState::Halted) {
        proc.openFiles.clear();
        proc.inbox.clear();
    }
    proc.yieldRequested=false;
    return true;
}
void NexaKernel::run(std::size_t maxSlices) { for(std::size_t n=0;n<maxSlices&&scheduleOne();++n) {} }
std::vector<ProcessInfo> NexaKernel::processes() const {
    std::vector<ProcessInfo> result;
    for(const auto& [id,p]:processes_) {
        (void)id;
        auto info=p.info;
        info.pendingMessages=p.inbox.size();
        info.readyWaitTicks=p.readyWaitTicks;
        const auto boost=priorityAgingThreshold_==0?0:p.readyWaitTicks/priorityAgingThreshold_;
        if(boost>=static_cast<std::uint64_t>(7-p.info.priority)) info.effectivePriority=7;
        else info.effectivePriority=static_cast<std::uint8_t>(p.info.priority+boost);
        result.push_back(std::move(info));
    }
    return result;
}
std::string NexaKernel::takeConsoleOutput() {
    // Drain any buffered device output as well as kernel-mediated writes.
    for (auto& [id, process] : processes_) {
        (void)id;
        output_ += process.machine.deviceOutput();
        process.machine.clearDeviceOutput();
    }
    auto result = std::move(output_);
    output_.clear();
    return result;
}
std::uint8_t NexaKernel::syscall(std::uint32_t pid,Syscall call,std::uint8_t arg) {
    auto it=processes_.find(pid);
    if(it==processes_.end()) throw std::out_of_range("Unknown process id");
    switch(call) {
    case Syscall::Exit: it->second.info.state=ProcessState::Halted; it->second.info.exitCode=arg; it->second.info.terminationReason="exit"; return 0;
    case Syscall::Write: output_.push_back(static_cast<char>(arg)); return 0;
    case Syscall::Read: return it->second.machine.readMemory(arg);
    case Syscall::GetPid: return static_cast<std::uint8_t>(pid&255);
    case Syscall::Yield: it->second.yieldRequested=true; return 0;
    case Syscall::FileExists: { const auto path="/"+std::to_string(arg); return hasFileAccess(pid,path,FilePermission::Read)&&fs_.exists(path)?1:0; }
    case Syscall::OpenRead:
    case Syscall::OpenWrite: {
        const auto path=readGuestPath(it->second.machine,arg);
        const auto permission=call==Syscall::OpenRead?FilePermission::Read:FilePermission::Write;
        if(!hasFileAccess(pid,path,permission)) throw std::runtime_error("VFS access denied");
        if(!fs_.exists(path)) throw std::runtime_error("VFS file not found");
        if(it->second.nextHandle>255) throw std::runtime_error("Process file handle table exhausted");
        const auto handle=static_cast<std::uint8_t>(it->second.nextHandle++);
        it->second.openFiles.emplace(handle,OpenFile{path,0,call==Syscall::OpenWrite});
        return handle;
    }
    case Syscall::ReadByte: {
        auto file=it->second.openFiles.find(arg);
        if(file==it->second.openFiles.end()||file->second.writable) throw std::runtime_error("Invalid readable file handle");
        const auto data=fs_.readFile(file->second.path);
        if(file->second.position>=data.size()) return 0;
        return data[file->second.position++];
    }
    case Syscall::WriteByte: {
        if(static_cast<std::size_t>(arg)+1>=it->second.machine.ramSize()) throw std::runtime_error("Write descriptor outside process RAM");
        const auto handle=it->second.machine.readMemory(arg);
        const auto value=it->second.machine.readMemory(static_cast<std::uint16_t>(arg+1));
        auto file=it->second.openFiles.find(handle);
        if(file==it->second.openFiles.end()||!file->second.writable) throw std::runtime_error("Invalid writable file handle");
        auto data=fs_.readFile(file->second.path);
        if(file->second.position>data.size()) data.resize(file->second.position,0);
        if(file->second.position==data.size()) data.push_back(value); else data[file->second.position]=value;
        fs_.writeFile(file->second.path,data); ++file->second.position; return 0;
    }
    case Syscall::CloseFile:
        if(!it->second.openFiles.erase(arg)) throw std::runtime_error("Invalid file handle");
        return 0;
    case Syscall::SendMessage: {
        if(static_cast<std::size_t>(arg)+3>it->second.machine.ramSize()) throw std::runtime_error("IPC send descriptor outside process RAM");
        const auto low=it->second.machine.readMemory(arg);
        const auto high=it->second.machine.readMemory(static_cast<std::uint16_t>(arg+1));
        const auto length=it->second.machine.readMemory(static_cast<std::uint16_t>(arg+2));
        const auto target=static_cast<std::uint32_t>(low|(static_cast<std::uint16_t>(high)<<8));
        if(static_cast<std::size_t>(arg)+3+length>it->second.machine.ramSize()) throw std::runtime_error("IPC payload outside process RAM");
        std::vector<std::uint8_t> payload; payload.reserve(length);
        for(std::size_t index=0;index<length;++index) payload.push_back(it->second.machine.readMemory(static_cast<std::uint16_t>(arg+3+index)));
        const auto destination=processes_.find(target);
        if(destination==processes_.end()||destination->second.info.state==ProcessState::Halted||
           destination->second.info.state==ProcessState::Faulted||destination->second.inbox.size()>=MaxInboxMessages)
            return 0;
        return sendMessage(pid,target,std::move(payload))?1:0;
    }
    case Syscall::ReceiveMessage: {
        if(static_cast<std::size_t>(arg)+5>it->second.machine.ramSize()) throw std::runtime_error("IPC receive descriptor outside process RAM");
        if(it->second.inbox.empty()) {
            it->second.info.state=ProcessState::Blocked;
            it->second.machine.retryCurrentInstruction();
            return 0;
        }
        const auto& message=it->second.inbox.front();
        if(static_cast<std::size_t>(arg)+5+message.payload.size()>it->second.machine.ramSize()) throw std::runtime_error("IPC receive buffer too small");
        const auto sender=message.sender;
        for(std::size_t byte=0;byte<4;++byte)
            it->second.machine.writeMemory(static_cast<std::uint16_t>(arg+byte),static_cast<std::uint8_t>((sender>>(8*byte))&0xFF));
        it->second.machine.writeMemory(static_cast<std::uint16_t>(arg+4),static_cast<std::uint8_t>(message.payload.size()));
        for(std::size_t index=0;index<message.payload.size();++index)
            it->second.machine.writeMemory(static_cast<std::uint16_t>(arg+5+index),message.payload[index]);
        it->second.inbox.pop_front();
        return 1;
    }
    }
    return 255;
}
std::uint8_t NexaKernel::readProcessMemory(std::uint32_t pid,std::uint16_t address) const {
    auto it=processes_.find(pid);
    if(it==processes_.end()) throw std::out_of_range("Unknown process id");
    if(address>=it->second.machine.ramSize()) throw std::out_of_range("Process memory address out of range");
    return memory_.read(pid,address,MemoryAccess::Kernel);
}
void NexaKernel::writeProcessMemory(std::uint32_t pid,std::uint16_t address,std::uint8_t value) {
    auto it=processes_.find(pid);
    if(it==processes_.end()) throw std::out_of_range("Unknown process id");
    if(address>=it->second.machine.ramSize()) throw std::out_of_range("Process memory address out of range");
    memory_.write(pid,address,value,MemoryAccess::Kernel);
}
std::string NexaKernel::readGuestPath(const VM& machine,std::uint8_t address) const {
    std::string path;
    for(std::size_t pos=address;pos<machine.ramSize();++pos) {
        const auto ch=machine.readMemory(static_cast<std::uint16_t>(pos));
        if(ch==0) { if(path.empty()) throw std::runtime_error("Empty guest VFS path"); return path; }
        path.push_back(static_cast<char>(ch));
        if(path.size()>255) throw std::runtime_error("Guest VFS path too long");
    }
    throw std::runtime_error("Unterminated guest VFS path");
}
void NexaKernel::grantFileAccess(std::uint32_t pid,const std::string& path,FilePermission permissions) {
    if(!processes_.contains(pid)) throw std::out_of_range("Unknown process id");
    if(path.empty() || path.front()!='/' || path.find("..")!=std::string::npos)
        throw std::invalid_argument("File permission path must be an absolute normalized VFS path");
    if(!fs_.exists(path)) throw std::out_of_range("VFS path does not exist");
    filePermissions_[pid][path]=static_cast<std::uint8_t>(permissions);
}
bool NexaKernel::hasFileAccess(std::uint32_t pid,const std::string& path,FilePermission permission) const {
    if(!processes_.contains(pid)) throw std::out_of_range("Unknown process id");
    if(path.empty() || path.front()!='/' || path.find("..")!=std::string::npos) return false;
    const auto processIt=filePermissions_.find(pid);
    if(processIt==filePermissions_.end()) return false;
    const auto accessIt=processIt->second.find(path);
    if(accessIt==processIt->second.end()) return false;
    const auto requested=static_cast<std::uint8_t>(permission);
    return requested!=0 && (accessIt->second & requested)==requested;
}
}
