#include "nexa/config.hpp"
#include "nexa/virtual_disk.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <vector>

int main() {
    const char* configPath = "nexa_test_config.json";
    const char* diskPath = "nexa_test.disk";
    const char* malformedPath = "nexa_test_malformed.disk";

    nexa::MachineConfig config;
    config.name = "TestVM";
    config.ramBytes = 512;
    config.diskPath = diskPath;
    config.diskBytes = 128;
    config.save(configPath);

    const auto loaded = nexa::MachineConfig::load(configPath);
    assert(loaded.name == "TestVM");
    assert(loaded.ramBytes == 512);
    assert(loaded.diskBytes == 128);

    nexa::VirtualDisk disk(diskPath, 128);
    disk.write(5, {1, 2, 3, 4});
    assert((disk.read(5, 4) == std::vector<unsigned char>{1, 2, 3, 4}));
    disk.flush();

    nexa::VirtualDisk reloaded(diskPath, 128);
    reloaded.load();
    assert((reloaded.read(5, 4) == std::vector<unsigned char>{1, 2, 3, 4}));

    bool outOfRange = false;
    try {
        reloaded.read(127, 2);
    } catch (const std::out_of_range&) {
        outOfRange = true;
    }
    assert(outOfRange);

    // A missing image represents a new, zero-filled disk.
    std::remove(malformedPath);
    nexa::VirtualDisk blank(malformedPath, 512);
    blank.load();
    assert((blank.read(0, 4) == std::vector<unsigned char>{0, 0, 0, 0}));

    // Existing images must match the configured capacity exactly.
    {
        std::ofstream malformed(malformedPath, std::ios::binary);
        malformed.put('x');
    }
    bool invalidSize = false;
    try {
        blank.load();
    } catch (const std::runtime_error&) {
        invalidSize = true;
    }
    assert(invalidSize);

    bool invalidCapacity = false;
    try {
        nexa::VirtualDisk invalid("", 0);
    } catch (const std::invalid_argument&) {
        invalidCapacity = true;
    }
    assert(invalidCapacity);

    std::remove(configPath);
    std::remove(diskPath);
    std::remove(malformedPath);
}
