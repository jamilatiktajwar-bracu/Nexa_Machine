#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

namespace nexa {
enum class MemoryAccess { User, Kernel };
struct PageMapping {
    std::uint16_t frame{};
    bool writable{true};
    bool userAccessible{true};
    bool executable{true};
};

class VirtualMemoryManager {
public:
    static constexpr std::size_t PageSize=256;
    explicit VirtualMemoryManager(std::size_t physicalBytes);
    void createAddressSpace(std::uint32_t pid);
    void destroyAddressSpace(std::uint32_t pid);
    void mapPage(std::uint32_t pid,std::uint8_t virtualPage,bool writable=true,bool userAccessible=true,bool executable=true);
    void unmapPage(std::uint32_t pid,std::uint8_t virtualPage);
    void protectPage(std::uint32_t pid,std::uint8_t virtualPage,bool writable,bool userAccessible,bool executable=true);
    [[nodiscard]] std::uint8_t read(std::uint32_t pid,std::uint16_t virtualAddress,MemoryAccess access=MemoryAccess::User) const;
    void write(std::uint32_t pid,std::uint16_t virtualAddress,std::uint8_t value,MemoryAccess access=MemoryAccess::User);
    [[nodiscard]] bool canExecute(std::uint32_t pid,std::uint16_t virtualAddress,MemoryAccess access=MemoryAccess::User) const;\n    [[nodiscard]] bool isMapped(std::uint32_t pid,std::uint8_t virtualPage) const;
    [[nodiscard]] std::size_t mappedPages(std::uint32_t pid) const;
    [[nodiscard]] std::size_t freeFrames() const noexcept;
    [[nodiscard]] std::size_t totalFrames() const noexcept{return physical_.size()/PageSize;}
private:
    std::vector<std::uint8_t> physical_;
    std::vector<bool> usedFrames_;
    std::map<std::uint32_t,std::map<std::uint8_t,PageMapping>> spaces_;
    [[nodiscard]] PageMapping& mapping(std::uint32_t pid,std::uint8_t page);
    [[nodiscard]] const PageMapping& mapping(std::uint32_t pid,std::uint8_t page) const;
    [[nodiscard]] std::uint16_t allocateFrame();
    void releaseFrame(std::uint16_t frame);
};
}
