#include "nexa/kernel.hpp"
#include <algorithm>
#include <exception>
#include <stdexcept>
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
        if(service>static_cast<std::uint8_t>(Syscall::CloseFile)) throw std::runtime_error("Unknown kernel syscall");
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
bool NexaKernel::scheduleOne() {
    if(processes_.empty()) return false;
    std::vector<std::uint32_t> ids;
    for(const auto& [id,p]:processes_) if(p.info.state==ProcessState::Ready) ids.push_back(id);
    if(ids.empty()) return false;
    auto it=std::find_if(ids.begin(),ids.end(),[&](auto id){return id>cursor_;});
    const auto pid=it==ids.end()?ids.front():*it;
    cursor_=pid; currentPid_=pid;
    auto& proc=processes_.at(pid);
    proc.info.state=ProcessState::Running;
    try {
        proc.machine.stepInstruction();
        ++proc.info.ticks;
        output_+=proc.machine.deviceOutput();
        proc.machine.clearDeviceOutput();
        proc.info.state=proc.machine.halted()?ProcessState::Halted:ProcessState::Ready;
    } catch(const std::exception& e) {
        proc.info.state=ProcessState::Faulted;
        proc.info.faultReason=e.what();
        output_+=proc.machine.deviceOutput();
        proc.machine.clearDeviceOutput();
    } catch(...) {
        proc.info.state=ProcessState::Faulted;
        proc.info.faultReason="Unknown process fault";
    }
    return true;
}
void NexaKernel::run(std::size_t maxTicks) { for(std::size_t n=0;n<maxTicks&&scheduleOne();++n) {} }
std::vector<ProcessInfo> NexaKernel::processes() const {
    std::vector<ProcessInfo> result;
    for(const auto& [id,p]:processes_) result.push_back(p.info);
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
    case Syscall::Exit: it->second.info.state=ProcessState::Halted; return 0;
    case Syscall::Write: output_.push_back(static_cast<char>(arg)); return 0;
    case Syscall::Read: return it->second.machine.readMemory(arg);
    case Syscall::GetPid: return static_cast<std::uint8_t>(pid&255);
    case Syscall::Yield: return 0;
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
