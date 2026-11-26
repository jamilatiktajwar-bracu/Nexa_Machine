#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

namespace nexa {
enum class PageAccess : std::uint8_t { None=0, Read=1, Write=2, Execute=4 };
constexpr PageAccess operator|(PageAccess a,PageAccess b) noexcept {
    return static_cast<PageAccess>(static_cast<std::uint8_t>(a)|static_cast<std::uint8_t>(b));
}
constexpr bool hasAccess(PageAccess flags,PageAccess requested) noexcept {
    const auto value=static_cast<std::uint8_t>(requested);
    return value!=0&&(static_cast<std::uint8_t>(flags)&value)==value;
}

// Software-managed, per-address-space paging model for the Nexa educational VM.
class VirtualMemoryManager {
public:
    static constexpr std::size_t PageSize=4096;
    explicit VirtualMemoryManager(std::size_t physicalBytes=1024*1024)
        : physical_(physicalBytes,0), frameCount_(physicalBytes/PageSize) {
        if(physicalBytes<PageSize||physicalBytes%PageSize!=0)
            throw std::invalid_argument("Physical memory must contain whole 4 KiB frames");
        for(std::size_t i=0;i<frameCount_;++i) freeFrames_.insert(i);
    }
    void createAddressSpace(std::uint32_t id) {
        if(!spaces_.emplace(id,std::map<std::uint32_t,Mapping>{}).second)
            throw std::invalid_argument("Address space already exists");
    }
    void destroyAddressSpace(std::uint32_t id) {
        auto it=spaces_.find(id);
        if(it==spaces_.end()) throw std::out_of_range("Unknown address space");
        for(const auto& [vpn,mapping]:it->second) { (void)vpn; releaseFrame(mapping.frame); }
        spaces_.erase(it);
    }
    void mapPage(std::uint32_t id,std::uint32_t virtualAddress,PageAccess access) {
        auto& space=addressSpace(id);
        if(virtualAddress%PageSize!=0) throw std::invalid_argument("Virtual page address must be 4 KiB aligned");
        if(access==PageAccess::None) throw std::invalid_argument("Page must have at least one access permission");
        const auto vpn=virtualAddress/static_cast<std::uint32_t>(PageSize);
        if(space.count(vpn)) throw std::invalid_argument("Virtual page is already mapped");
        if(freeFrames_.empty()) throw std::runtime_error("Physical memory exhausted");
        const auto frame=*freeFrames_.begin(); freeFrames_.erase(freeFrames_.begin());
        const auto begin=frame*PageSize;
        for(std::size_t i=0;i<PageSize;++i) physical_[begin+i]=0;
        space.emplace(vpn,Mapping{frame,access});
    }
    void unmapPage(std::uint32_t id,std::uint32_t virtualAddress) {
        auto& space=addressSpace(id);
        if(virtualAddress%PageSize!=0) throw std::invalid_argument("Virtual page address must be 4 KiB aligned");
        const auto vpn=virtualAddress/static_cast<std::uint32_t>(PageSize);
        auto it=space.find(vpn);
        if(it==space.end()) throw std::out_of_range("Virtual page is not mapped");
        releaseFrame(it->second.frame); space.erase(it);
    }
    [[nodiscard]] std::size_t translate(std::uint32_t id,std::uint32_t address,PageAccess requested) const {
        const auto& space=addressSpace(id);
        const auto vpn=address/static_cast<std::uint32_t>(PageSize);
        auto it=space.find(vpn);
        if(it==space.end()) throw std::out_of_range("Page fault: unmapped virtual address");
        if(!hasAccess(it->second.access,requested)) throw std::runtime_error("Page fault: access denied");
        return it->second.frame*PageSize+(address%PageSize);
    }
    [[nodiscard]] std::uint8_t read8(std::uint32_t id,std::uint32_t address) const {
        return physical_[translate(id,address,PageAccess::Read)];
    }
    void write8(std::uint32_t id,std::uint32_t address,std::uint8_t value) {
        physical_[translate(id,address,PageAccess::Write)]=value;
    }
    [[nodiscard]] std::size_t mappedPages(std::uint32_t id) const { return addressSpace(id).size(); }
    [[nodiscard]] std::size_t freeFrames() const noexcept { return freeFrames_.size(); }
    [[nodiscard]] std::size_t physicalBytes() const noexcept { return physical_.size(); }
    [[nodiscard]] bool hasAddressSpace(std::uint32_t id) const noexcept { return spaces_.count(id)!=0; }
private:
    struct Mapping { std::size_t frame; PageAccess access; };
    std::vector<std::uint8_t> physical_;
    std::size_t frameCount_{};
    std::set<std::size_t> freeFrames_;
    std::map<std::uint32_t,std::map<std::uint32_t,Mapping>> spaces_;
    std::map<std::uint32_t,Mapping>& addressSpace(std::uint32_t id) {
        auto it=spaces_.find(id); if(it==spaces_.end()) throw std::out_of_range("Unknown address space"); return it->second;
    }
    const std::map<std::uint32_t,Mapping>& addressSpace(std::uint32_t id) const {
        auto it=spaces_.find(id); if(it==spaces_.end()) throw std::out_of_range("Unknown address space"); return it->second;
    }
    void releaseFrame(std::size_t frame) {
        const auto begin=frame*PageSize;
        for(std::size_t i=0;i<PageSize;++i) physical_[begin+i]=0;
        freeFrames_.insert(frame);
    }
};
}
