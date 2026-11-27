#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <vector>
#include <unordered_map>
#include <functional>
#include <string>

namespace nexa {
enum class MemoryAccess { User, Kernel };
struct PageMapping {
    std::uint16_t frame{};
    bool writable{true};
    bool userAccessible{true};
    bool executable{true};
    bool resident{true};
    std::uint64_t lastUsed{};
    std::vector<std::uint8_t> swapImage;
    std::uint64_t swapOffset{};
    bool diskBacked{};
};

class VirtualMemoryManager {
public:
    static constexpr std::size_t PageSize=256;
    explicit VirtualMemoryManager(std::size_t physicalBytes);
    void setSwapFile(const std::string& hostPath);
    [[nodiscard]] const std::string& swapFilePath() const noexcept{return swapFilePath_;}
    void createAddressSpace(std::uint32_t pid);
    void destroyAddressSpace(std::uint32_t pid);
    void mapPage(std::uint32_t pid,std::uint8_t virtualPage,bool writable=true,bool userAccessible=true,bool executable=true);
    [[nodiscard]] std::uint16_t createSharedFrame();
    void releaseSharedFrame(std::uint16_t frame);
    void mapSharedFrame(std::uint32_t pid,std::uint8_t virtualPage,std::uint16_t frame,bool writable=true,bool userAccessible=true);
    [[nodiscard]] std::uint8_t readSharedFrame(std::uint16_t frame,std::size_t offset) const;
    void writeSharedFrame(std::uint16_t frame,std::size_t offset,std::uint8_t value);
    void unmapPage(std::uint32_t pid,std::uint8_t virtualPage);
    void protectPage(std::uint32_t pid,std::uint8_t virtualPage,bool writable,bool userAccessible,bool executable=true);
    [[nodiscard]] std::uint8_t read(std::uint32_t pid,std::uint16_t virtualAddress,MemoryAccess access=MemoryAccess::User) const;
    void write(std::uint32_t pid,std::uint16_t virtualAddress,std::uint8_t value,MemoryAccess access=MemoryAccess::User);
    [[nodiscard]] bool canExecute(std::uint32_t pid,std::uint16_t virtualAddress,MemoryAccess access=MemoryAccess::User) const;
    [[nodiscard]] std::size_t translate(std::uint32_t pid,std::uint16_t virtualAddress,MemoryAccess access=MemoryAccess::User,bool writeAccess=false,bool executeAccess=false) const;
    void setPageFaultHandler(std::function<void(std::uint32_t,std::uint8_t)> handler){pageFaultHandler_=std::move(handler);}
    [[nodiscard]] std::size_t pageFaults() const noexcept{return pageFaults_;}
    void flushTlb() const noexcept;
    void invalidateTlb(std::uint32_t pid) const noexcept;
    [[nodiscard]] std::size_t tlbHits() const noexcept{return tlbHits_;}
    [[nodiscard]] std::size_t tlbMisses() const noexcept{return tlbMisses_;}
    [[nodiscard]] bool isMapped(std::uint32_t pid,std::uint8_t virtualPage) const;
    [[nodiscard]] std::size_t mappedPages(std::uint32_t pid) const;
    [[nodiscard]] std::size_t residentPages(std::uint32_t pid) const;
    [[nodiscard]] std::size_t swappedPages(std::uint32_t pid) const;
    [[nodiscard]] std::size_t freeFrames() const noexcept;
    [[nodiscard]] std::size_t swappedPages() const noexcept{return swappedPages_;}
    [[nodiscard]] std::size_t swapIns() const noexcept{return swapIns_;}
    [[nodiscard]] std::size_t swapOuts() const noexcept{return swapOuts_;}
    [[nodiscard]] std::size_t residentPages() const noexcept{return totalFrames()-freeFrames();}
    [[nodiscard]] std::size_t totalFrames() const noexcept{return physical_.size()/PageSize;}
private:
    std::vector<std::uint8_t> physical_;
    std::vector<bool> usedFrames_;
    std::map<std::uint32_t,std::map<std::uint8_t,PageMapping>> spaces_;
    mutable std::unordered_map<std::uint64_t,PageMapping> tlb_;
    mutable std::size_t tlbHits_{0},tlbMisses_{0};
    mutable std::size_t pageFaults_{0};
    mutable std::uint64_t accessClock_{0};
    std::size_t swappedPages_{0},swapIns_{0},swapOuts_{0};
    std::string swapFilePath_;
    std::map<std::uint16_t,std::size_t> sharedFrameMappings_;
    std::set<std::uint16_t> ownedSharedFrames_;
    std::function<void(std::uint32_t,std::uint8_t)> pageFaultHandler_;
    static constexpr std::size_t TlbCapacity=16;
    [[nodiscard]] PageMapping& mapping(std::uint32_t pid,std::uint8_t page);
    [[nodiscard]] const PageMapping& mapping(std::uint32_t pid,std::uint8_t page) const;
    [[nodiscard]] std::uint16_t allocateFrame();
    void releaseFrame(std::uint16_t frame);
    [[nodiscard]] std::uint16_t evictOrAllocateFrame();
    void pageIn(std::uint32_t pid,std::uint8_t page);
};
}
