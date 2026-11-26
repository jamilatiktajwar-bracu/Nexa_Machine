#include "nexa/memory_manager.hpp"
#include <algorithm>
#include <stdexcept>
namespace nexa {
VirtualMemoryManager::VirtualMemoryManager(std::size_t bytes) {
    if(bytes<PageSize||bytes%PageSize!=0||bytes/PageSize>65536)
        throw std::invalid_argument("Physical memory must be a page-aligned size of at least 256 bytes");
    physical_.assign(bytes,0); usedFrames_.assign(bytes/PageSize,false);
}
void VirtualMemoryManager::createAddressSpace(std::uint32_t pid) {
    if(!spaces_.emplace(pid,std::map<std::uint8_t,PageMapping>{}).second)
        throw std::invalid_argument("Address space already exists");
}
void VirtualMemoryManager::destroyAddressSpace(std::uint32_t pid) {
    auto it=spaces_.find(pid); if(it==spaces_.end()) throw std::out_of_range("Unknown address space");
    invalidateTlb(pid);
    for(const auto& [page,m]:it->second){(void)page;if(m.resident) releaseFrame(m.frame); else if(swappedPages_) --swappedPages_;}
    spaces_.erase(it);
}
std::uint16_t VirtualMemoryManager::allocateFrame() {
    for(std::size_t i=0;i<usedFrames_.size();++i) if(!usedFrames_[i]) {
        usedFrames_[i]=true; const auto start=i*PageSize;
        std::fill(physical_.begin()+static_cast<std::ptrdiff_t>(start),physical_.begin()+static_cast<std::ptrdiff_t>(start+PageSize),0);
        return static_cast<std::uint16_t>(i);
    }
    throw std::runtime_error("Out of physical memory");
}
void VirtualMemoryManager::releaseFrame(std::uint16_t frame) {
    if(frame>=usedFrames_.size()||!usedFrames_[frame]) throw std::runtime_error("Invalid physical frame release");
    usedFrames_[frame]=false;
}
void VirtualMemoryManager::mapPage(std::uint32_t pid,std::uint8_t page,bool writable,bool user,bool executable) {
    auto it=spaces_.find(pid); if(it==spaces_.end()) throw std::out_of_range("Unknown address space");
    if(it->second.contains(page)) throw std::invalid_argument("Virtual page already mapped");
    const auto frame=allocateFrame();
    it->second.emplace(page,PageMapping{frame,writable,user,executable,true,++accessClock_,{}});
    invalidateTlb(pid);
}
void VirtualMemoryManager::unmapPage(std::uint32_t pid,std::uint8_t page) {
    auto& table=spaces_.at(pid); auto it=table.find(page);
    if(it==table.end()) throw std::out_of_range("Virtual page is not mapped");
    if(it->second.resident) releaseFrame(it->second.frame); else if(swappedPages_) --swappedPages_;
    table.erase(it); invalidateTlb(pid);
}
void VirtualMemoryManager::protectPage(std::uint32_t pid,std::uint8_t page,bool writable,bool user,bool executable) {
    auto& m=mapping(pid,page); m.writable=writable; m.userAccessible=user; m.executable=executable; invalidateTlb(pid);
}
PageMapping& VirtualMemoryManager::mapping(std::uint32_t pid,std::uint8_t page) {
    auto s=spaces_.find(pid); if(s==spaces_.end()) throw std::out_of_range("Unknown address space");
    auto p=s->second.find(page); if(p==s->second.end()) throw std::runtime_error("Page fault: unmapped virtual page");
    return p->second;
}
const PageMapping& VirtualMemoryManager::mapping(std::uint32_t pid,std::uint8_t page) const {
    auto s=spaces_.find(pid); if(s==spaces_.end()) throw std::out_of_range("Unknown address space");
    auto p=s->second.find(page); if(p==s->second.end()) throw std::runtime_error("Page fault: unmapped virtual page");
    return p->second;
}
std::size_t VirtualMemoryManager::translate(std::uint32_t pid,std::uint16_t address,MemoryAccess access,bool writeAccess,bool executeAccess) const {
    const auto page=static_cast<std::uint8_t>(address/PageSize);
    const auto key=(static_cast<std::uint64_t>(pid)<<8)|page;
    auto hit=tlb_.find(key);
    if(hit==tlb_.end()) ++tlbMisses_; else ++tlbHits_;
    auto& self=*const_cast<VirtualMemoryManager*>(this);
    PageMapping* livePtr=nullptr;
    try { livePtr=&self.mapping(pid,page); }
    catch(const std::runtime_error&) {
        if(!pageFaultHandler_) throw;
        ++pageFaults_;
        pageFaultHandler_(pid,page);
        livePtr=&self.mapping(pid,page);
    }
    if(!livePtr->resident) {
        ++pageFaults_;
        self.pageIn(pid,page);
    }
    auto& live=*livePtr;
    live.lastUsed=++accessClock_;
    if(access==MemoryAccess::User&&!live.userAccessible) throw std::runtime_error("Page fault: supervisor-only page");
    if(writeAccess&&!live.writable) throw std::runtime_error("Page fault: write protection");
    if(executeAccess&&!live.executable) throw std::runtime_error("Page fault: execute protection");
    if(hit==tlb_.end()) {
        if(tlb_.size()>=TlbCapacity) {
            auto oldest=tlb_.begin();
            for(auto candidate=tlb_.begin();candidate!=tlb_.end();++candidate) {
                const auto candidatePid=static_cast<std::uint32_t>(candidate->first>>8);
                const auto candidatePage=static_cast<std::uint8_t>(candidate->first&255);
                const auto oldestPid=static_cast<std::uint32_t>(oldest->first>>8);
                const auto oldestPage=static_cast<std::uint8_t>(oldest->first&255);
                const auto candidateAge=self.mapping(candidatePid,candidatePage).lastUsed;
                const auto oldestAge=self.mapping(oldestPid,oldestPage).lastUsed;
                if(candidateAge<oldestAge) oldest=candidate;
            }
            tlb_.erase(oldest);
        }
        tlb_.emplace(key,live);
    } else hit->second=live;
    return static_cast<std::size_t>(live.frame)*PageSize+(address%PageSize);
}
void VirtualMemoryManager::flushTlb() const noexcept {tlb_.clear();}
void VirtualMemoryManager::invalidateTlb(std::uint32_t pid) const noexcept {
    const auto prefix=static_cast<std::uint64_t>(pid)<<8;
    for(auto it=tlb_.begin();it!=tlb_.end();) {
        if((it->first&0xFFFFFFFFFFFFFF00ULL)==prefix) it=tlb_.erase(it); else ++it;
    }
}
std::uint8_t VirtualMemoryManager::read(std::uint32_t pid,std::uint16_t address,MemoryAccess access) const {
    return physical_[translate(pid,address,access)];
}
void VirtualMemoryManager::write(std::uint32_t pid,std::uint16_t address,std::uint8_t value,MemoryAccess access) {
    physical_[translate(pid,address,access,true)]=value;
}
bool VirtualMemoryManager::canExecute(std::uint32_t pid,std::uint16_t address,MemoryAccess access) const {
    (void)translate(pid,address,access,false,true); return true;
}
bool VirtualMemoryManager::isMapped(std::uint32_t pid,std::uint8_t page) const {
    auto it=spaces_.find(pid); return it!=spaces_.end()&&it->second.contains(page);
}
std::size_t VirtualMemoryManager::mappedPages(std::uint32_t pid) const {return spaces_.at(pid).size();}
std::size_t VirtualMemoryManager::freeFrames() const noexcept {return static_cast<std::size_t>(std::count(usedFrames_.begin(),usedFrames_.end(),false));}
}
