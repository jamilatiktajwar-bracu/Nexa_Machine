#include "nexa/disk_scheduler.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>

int main() {
    nexa::VM vm(2048);
    // Descriptor: read, device 7, sector 3, buffer 512.
    vm.writeMemory(0, 0);
    vm.writeMemory(1, 7);
    vm.writeMemory(2, 0);
    vm.writeMemory(3, 0);
    vm.writeMemory(4, 0);
    vm.writeMemory(5, 3);
    vm.writeMemory(6, 0);
    vm.writeMemory(7, 0);
    vm.writeMemory(8, 0);
    vm.writeMemory(9, 0);
    vm.writeMemory(10, 2);

    nexa::GuestDiskQueue queue;
    auto first = queue.submit(vm, 42, 0, 9);
    assert(first.device == 7 && first.sector == 3 && first.buffer == 512);
    assert(first.session == 42 && first.interruptVector == 9);
    assert(queue.hasDevice(42, 7));
    assert(!queue.hasDevice(41, 7));

    for (size_t i = 1; i < nexa::GuestDiskQueue::MaxPendingRequests; ++i)
        queue.submit(vm, 42, 0, 9);
    bool full = false;
    try { queue.submit(vm, 42, 0, 9); }
    catch (const std::runtime_error&) { full = true; }
    assert(full);
    assert(queue.size() == nexa::GuestDiskQueue::MaxPendingRequests);

    queue.clearSession(42);
    assert(queue.empty());
    assert(!queue.hasDevice(42, 7));
    std::cout << "Disk scheduler tests passed\\n";
}
