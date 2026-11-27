#pragma once
#include "nexa/vm.hpp"
#include <cstdint>
#include <deque>
#include <optional>
#include <stdexcept>

namespace nexa {
struct GuestDiskRequest {
    uint64_t requestId{};
    uint64_t session{};
    uint32_t device{};
    uint32_t sector{};
    uint16_t buffer{};
    bool write{};
    uint8_t interruptVector{};
    int status{};
    std::string error;
};

// Shared descriptor parser and FIFO used by both the C ABI and desktop GUI.
// The caller owns execution and disk-device resolution; pop() never performs I/O.
class GuestDiskQueue {
public:
    GuestDiskRequest submit(const VM& vm, uint64_t session, uint16_t descriptor, uint8_t vector) {
        if (static_cast<size_t>(descriptor) + 11 > vm.ramSize())
            throw std::out_of_range("Disk descriptor exceeds guest RAM");
        auto byte = [&](size_t n) { return vm.readMemory(static_cast<uint16_t>(descriptor + n)); };
        GuestDiskRequest request;
        request.requestId = nextRequest_++;
        request.session = session;
        request.write = byte(0) != 0;
        request.device = static_cast<uint32_t>(byte(1)) |
            (static_cast<uint32_t>(byte(2)) << 8) |
            (static_cast<uint32_t>(byte(3)) << 16) |
            (static_cast<uint32_t>(byte(4)) << 24);
        request.sector = static_cast<uint32_t>(byte(5)) |
            (static_cast<uint32_t>(byte(6)) << 8) |
            (static_cast<uint32_t>(byte(7)) << 16) |
            (static_cast<uint32_t>(byte(8)) << 24);
        request.buffer = static_cast<uint16_t>(byte(9) | (static_cast<uint16_t>(byte(10)) << 8));
        request.interruptVector = vector;
        if (static_cast<size_t>(request.buffer) + 512 > vm.ramSize())
            throw std::out_of_range("Disk request buffer exceeds guest RAM");
        pending_.push_back(request);
        return request;
    }

    std::optional<GuestDiskRequest> pop() {
        if (pending_.empty()) return std::nullopt;
        auto request = pending_.front();
        pending_.pop_front();
        return request;
    }
    bool empty() const noexcept { return pending_.empty(); }
    size_t size() const noexcept { return pending_.size(); }
    void clearSession(uint64_t session) {
        std::deque<GuestDiskRequest> kept;
        while (!pending_.empty()) {
            auto request = pending_.front();
            pending_.pop_front();
            if (request.session != session) kept.push_back(std::move(request));
        }
        pending_.swap(kept);
    }
private:
    uint64_t nextRequest_{1};
    std::deque<GuestDiskRequest> pending_;
};
}
