#include "nexa/c_api.h"
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <initializer_list>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

static fs::path rootPath() {
    return fs::temp_directory_path() / "nexa-c-api-crash-recovery";
}
static void fillFile(const fs::path& path, unsigned char value) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    std::array<unsigned char, 512> bytes{};
    bytes.fill(value);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    out.flush();
    if (!out) throw std::runtime_error("failed to write disk fixture");
}
static std::string quote(const std::string& value) {
    return "\"" + value + "\"";
}
static bool attachDisk(NexaContext* context, uint64_t& session,
                       const fs::path& disk) {
    return nexa_session_create(context, "restore-crash-test", 1024, &session) == 0 &&
           nexa_disk_attach(context, session, disk.string().c_str(), 512) == 0;
}

int main(int argc, char** argv) {
    const auto root = rootPath();
    const auto disk = root / "guest.disk";
    const auto vmState = root / "snapshot.vm";
    const auto snapshotDir = root / "bundle";
    const auto journal = fs::path(disk.string() + ".nexa-journal");

    if (argc == 6 && std::string(argv[1]) == "--restore-crash-child") {
        NexaContext* context = nexa_context_create();
        if (!context) return 10;
        uint64_t session = 0;
        if (!attachDisk(context, session, disk)) {
            std::cerr << "child attach failed: " << nexa_last_error(context) << '\\n';
            nexa_context_destroy(context);
            return 11;
        }
        nexa_snapshot_test_set_failpoint(std::stoi(argv[2]));
        const int result = nexa_snapshot_restore_bundle(
            context, session, argv[3], argv[4]);
        std::cerr << "restore returned instead of crashing, result=" << result
                  << " error=" << nexa_last_error(context) << '\\n';
        nexa_context_destroy(context);
        return 12;
    }

    fs::remove_all(root);
    fs::create_directories(root);
    fillFile(disk, 0x41);

    // Save a real bundle through the public C API, then mutate the live disk.
    NexaContext* setup = nexa_context_create();
    if (!setup) return 1;
    uint64_t setupSession = 0;
    if (!attachDisk(setup, setupSession, disk) ||
        nexa_snapshot_save_bundle(setup, setupSession, vmState.string().c_str(),
                                  snapshotDir.string().c_str()) != 0) {
        std::cerr << "snapshot setup failed: " << nexa_last_error(setup) << '\\n';
        nexa_context_destroy(setup);
        return 2;
    }
    std::array<uint8_t, 512> mutated{};
    mutated.fill(0x42);
    if (nexa_disk_write_sector(setup, setupSession, 0, mutated.data()) != 0 ||
        nexa_disk_flush(setup, setupSession) != 0) {
        std::cerr << "disk mutation failed: " << nexa_last_error(setup) << '\\n';
        nexa_context_destroy(setup);
        return 3;
    }
    nexa_context_destroy(setup);

    for (const int failpoint : {3, 4}) {
        const std::string command =
            quote(argv[0]) + " --restore-crash-child " + std::to_string(failpoint) +
            " " + quote(vmState.string()) + " " + quote(snapshotDir.string()) +
            " " + quote(disk.string());
        const int childStatus = std::system(command.c_str());
#ifdef _WIN32
        const int expectedStatus = failpoint == 3 ? 83 : 84;
#else
        const int expectedStatus = (failpoint == 3 ? 83 : 84) << 8;
#endif
        if (childStatus != expectedStatus || !fs::exists(journal)) {
            std::cerr << "restore child did not hit the intended abrupt-exit failpoint "
                      << failpoint << " (status " << childStatus << ")\\n";
            return 4;
        }

        // A fresh context models a new host process attaching the guest disk.
        NexaContext* restarted = nexa_context_create();
        if (!restarted) return 5;
        uint64_t restartedSession = 0;
        if (!attachDisk(restarted, restartedSession, disk)) {
            std::cerr << "restart attachment recovery failed: "
                      << nexa_last_error(restarted) << '\\n';
            nexa_context_destroy(restarted);
            return 6;
        }
        std::array<uint8_t, 512> observed{};
        const int readStatus =
            nexa_disk_read_sector(restarted, restartedSession, 0, observed.data());
        nexa_context_destroy(restarted);
        if (readStatus != 0 || observed[0] != 0x42 || fs::exists(journal)) {
            std::cerr << "new context did not recover pre-restore disk state at failpoint "
                      << failpoint << '\\n';
            return 7;
        }
    }

    fs::remove_all(root);
    std::cout << "C API restore_bundle process-crash recovery tests passed\\n";
    return 0;
}
