#include "nexa/snapshot_journal.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

static void write(const std::filesystem::path& p, const std::string& value) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out << value;
}
static std::string read(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
int main(int argc, char** argv) {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / "nexa-snapshot-journal-test";
    const auto target = root / "disk.img";
    const auto backup = root / "disk.rollback";
    const auto journal = root / "transaction.journal";
    fs::create_directories(root);
    if (argc == 3 && std::string(argv[1]) == "--crash-child") {
        write(target, "original-disk-state");
        auto entries = nexa::snapshot::prepare(journal, {{target, backup}});
        write(target, argv[2]);
        nexa::snapshot::syncFile(target);
        std::_Exit(73); // Simulate abrupt process death; no destructors or rollback.
    }
    for (const std::string phase : {"after-disk-replacement", "after-vm-load"}) {
        write(target, "pre-crash-placeholder");
        const std::string command = "\"" + std::string(argv[0]) + "\" --crash-child " + phase;
        const int status = std::system(command.c_str());
        if (status == 0) { std::cerr << "child unexpectedly exited normally\n"; return 1; }
        if (!fs::exists(journal)) { std::cerr << "journal missing after crash\n"; return 1; }
        nexa::snapshot::recover(journal);
        if (read(target) != "original-disk-state") {
            std::cerr << "rollback did not restore original bytes\n"; return 1;
        }
        if (fs::exists(journal) || fs::exists(backup)) {
            std::cerr << "recovery left transaction debris\n"; return 1;
        }
    }
    write(target, "committed-before-crash");
    auto entries = nexa::snapshot::prepare(journal, {{target, backup}});
    write(target, "new-committed-state");
    nexa::snapshot::syncFile(target);
    nexa::snapshot::writeJournal(journal, nexa::snapshot::JournalPhase::Committed, entries);
    nexa::snapshot::recover(journal);
    if (read(target) != "new-committed-state") {
        std::cerr << "committed transaction was incorrectly rolled back\n"; return 1;
    }
    // A single coordinator decision must recover every disk, even when
    // recovery is first triggered by attaching only the second disk.
    const auto diskA = root / "disk-a.img";
    const auto diskB = root / "disk-b.img";
    const auto backupA = root / "disk-a.rollback";
    const auto backupB = root / "disk-b.rollback";
    const auto sidecarA = fs::path(diskA.string() + ".nexa-journal");
    const auto sidecarB = fs::path(diskB.string() + ".nexa-journal");
    const auto coordinator = root / "coordinated.journal";
    write(diskA, "disk-a-original");
    write(diskB, "disk-b-original");
    nexa::snapshot::prepareGroup(coordinator, {
        {sidecarA, {diskA, backupA}},
        {sidecarB, {diskB, backupB}}
    });
    write(diskA, "disk-a-mutated");
    write(diskB, "disk-b-mutated");
    nexa::snapshot::syncFile(diskA);
    nexa::snapshot::syncFile(diskB);
    nexa::snapshot::recover(sidecarB);
    if (read(diskA) != "disk-a-original" || read(diskB) != "disk-b-original" ||
        fs::exists(sidecarA) || fs::exists(sidecarB) || fs::exists(coordinator) ||
        fs::exists(backupA) || fs::exists(backupB)) {
        std::cerr << "coordinated rollback did not restore and clean every disk\\n";
        return 1;
    }

    write(diskA, "disk-a-before-commit");
    write(diskB, "disk-b-before-commit");
    nexa::snapshot::prepareGroup(coordinator, {
        {sidecarA, {diskA, backupA}},
        {sidecarB, {diskB, backupB}}
    });
    write(diskA, "disk-a-committed");
    write(diskB, "disk-b-committed");
    nexa::snapshot::syncFile(diskA);
    nexa::snapshot::syncFile(diskB);
    nexa::snapshot::commitGroup(coordinator);
    // A crash during rollback-directory cleanup leaves the COMMITTED decision
    // for the next process to consume without rolling either disk back.
    nexa::snapshot::recoverCoordinator(coordinator);
    if (read(diskA) != "disk-a-committed" || read(diskB) != "disk-b-committed" ||
        fs::exists(sidecarA) || fs::exists(sidecarB) || fs::exists(coordinator) ||
        fs::exists(backupA) || fs::exists(backupB)) {
        std::cerr << "coordinated commit did not retain and clean every disk\\n";
        return 1;
    }

    fs::remove_all(root);
    std::cout << "Snapshot journal subprocess recovery tests passed\n";
    return 0;
}
