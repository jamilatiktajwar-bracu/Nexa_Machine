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
static bool createSession(NexaContext* context, uint64_t& session) {
    return nexa_session_create(context, "restore-crash-test", 1024, &session) == 0;
}
static bool attachDisk(NexaContext* context, uint64_t session, const fs::path& disk) {
    return nexa_disk_attach(context, session, disk.string().c_str(), 512) == 0;
}

int main(int argc, char** argv) {
    const auto root = rootPath();
    const auto disk = root / "guest.disk";
    const auto vmState = root / "snapshot.vm";
    const auto preCrashVm = root / "pre-crash.vm";
    const auto snapshotDir = root / "bundle";
    const auto journal = fs::path(disk.string() + ".nexa-journal");

    if (argc == 7 && std::string(argv[1]) == "--restore-crash-child") {
        NexaContext* context = nexa_context_create();
        if (!context) return 10;
        uint64_t session = 0;
        if (!createSession(context, session) || !attachDisk(context, session, fs::path(argv[5]))) {
            std::cerr << "child setup failed: " << nexa_last_error(context) << '\n';
            nexa_context_destroy(context);
            return 11;
        }
        if (nexa_snapshot_load(context, session, argv[6]) != 0) {
            std::cerr << "child could not load pre-crash VM image: "
                      << nexa_last_error(context) << '\n';
            nexa_context_destroy(context);
            return 13;
        }
        nexa_snapshot_test_set_failpoint(std::stoi(argv[2]));
        const int result = nexa_snapshot_restore_bundle(context, session, argv[3], argv[4]);
        std::cerr << "restore returned instead of crashing, result=" << result
                  << " error=" << nexa_last_error(context) << '\n';
        nexa_context_destroy(context);
        return 12;
    }

    fs::remove_all(root);
    fs::create_directories(root);
    fillFile(disk, 0x41);

    NexaContext* setup = nexa_context_create();
    if (!setup) return 1;
    uint64_t setupSession = 0;
    NexaSessionInfo expectedCpuState{};
    NexaCpuState expectedRegisters{};
    int guestAlive = 0;
    if (!createSession(setup, setupSession) || !attachDisk(setup, setupSession, disk) ||
        nexa_session_load_assembly(setup, setupSession, "LOAD A, 12\nLOAD B, 34\nHALT\n") != 0 ||
        nexa_session_start(setup, setupSession) != 0 ||
        nexa_session_step(setup, setupSession, &guestAlive) != 0 ||
        nexa_session_step(setup, setupSession, &guestAlive) != 0 ||
        nexa_session_info(setup, setupSession, &expectedCpuState) != 0 ||
        nexa_cpu_state_get(setup, setupSession, &expectedRegisters) != 0 ||
        expectedCpuState.instructions == 0 || expectedRegisters.accumulator != 12 ||
        expectedRegisters.second_register != 34 || expectedRegisters.halted != 0 ||
        nexa_memory_write(setup, setupSession, 17, 0x11) != 0 ||
        nexa_snapshot_save_bundle(setup, setupSession, vmState.string().c_str(),
                                  snapshotDir.string().c_str()) != 0) {
        std::cerr << "snapshot setup failed: " << nexa_last_error(setup) << '\n';
        nexa_context_destroy(setup);
        return 2;
    }

    std::array<uint8_t, 512> mutated{};
    mutated.fill(0x42);
    if (nexa_disk_write_sector(setup, setupSession, 0, mutated.data()) != 0 ||
        nexa_disk_flush(setup, setupSession) != 0 ||
        nexa_memory_write(setup, setupSession, 17, 0x22) != 0 ||
        nexa_snapshot_save(setup, setupSession, preCrashVm.string().c_str()) != 0) {
        std::cerr << "pre-crash state setup failed: " << nexa_last_error(setup) << '\n';
        nexa_context_destroy(setup);
        return 3;
    }
    nexa_context_destroy(setup);

    for (const int failpoint : {3, 4}) {
        const std::string command =
            quote(argv[0]) + " --restore-crash-child " + std::to_string(failpoint) +
            " " + quote(vmState.string()) + " " + quote(snapshotDir.string()) +
            " " + quote(disk.string()) + " " + quote(preCrashVm.string());
        const int childStatus = std::system(command.c_str());
#ifdef _WIN32
        const int expectedStatus = failpoint == 3 ? 83 : 84;
#else
        const int expectedStatus = (failpoint == 3 ? 83 : 84) << 8;
#endif
        if (childStatus != expectedStatus || !fs::exists(journal)) {
            std::cerr << "restore child missed crash failpoint " << failpoint
                      << " (status " << childStatus << ")\n";
            return 4;
        }

        // Recover the VM image and all disks before attaching devices in a fresh process.
        NexaContext* restarted = nexa_context_create();
        if (!restarted) return 5;
        uint64_t restartedSession = 0;
        NexaRecoveryReport recoveryReport{};
        if (nexa_session_create_recovering(restarted, "restarted-session", 1024,
                                               vmState.string().c_str(), snapshotDir.string().c_str(),
                                               &restartedSession) != 0 ||
            nexa_snapshot_recovery_report_get(restarted, &recoveryReport) != 0 ||
            recoveryReport.attempted != 1 || recoveryReport.success != 1 ||
            !attachDisk(restarted, restartedSession, disk)) {
            std::cerr << "restart session recovery failed: "
                      << nexa_last_error(restarted) << '\n';
            nexa_context_destroy(restarted);
            return 6;
        }
        std::array<uint8_t, 512> observed{};
        uint8_t restoredRam = 0;
        NexaSessionInfo restoredCpuState{};
        NexaCpuState restoredRegisters{};
        const int readStatus =
            nexa_disk_read_sector(restarted, restartedSession, 0, observed.data());
        const int memoryStatus = nexa_memory_read(restarted, restartedSession, 17, &restoredRam);
        const int cpuStatus = nexa_session_info(restarted, restartedSession, &restoredCpuState);
        const int registerStatus = nexa_cpu_state_get(restarted, restartedSession, &restoredRegisters);
        nexa_context_destroy(restarted);
        if (readStatus != 0 || observed[0] != 0x42 || memoryStatus != 0 ||
            restoredRam != 0x22 || cpuStatus != 0 ||
            restoredCpuState.pc != expectedCpuState.pc ||
            restoredCpuState.instructions != expectedCpuState.instructions ||
            registerStatus != 0 ||
            restoredRegisters.accumulator != expectedRegisters.accumulator ||
            restoredRegisters.second_register != expectedRegisters.second_register ||
            restoredRegisters.pc != expectedRegisters.pc ||
            restoredRegisters.instructions != expectedRegisters.instructions ||
            restoredRegisters.stack_depth != expectedRegisters.stack_depth ||
            restoredRegisters.halted != expectedRegisters.halted || fs::exists(journal)) {
            std::cerr << "new session did not restore pre-transaction RAM and disk at failpoint "
                      << failpoint << '\n';
            return 7;
        }
    }

    fs::remove_all(root);
    std::cout << "C API restore_bundle process-crash and session recovery tests passed\n";
    return 0;
}
