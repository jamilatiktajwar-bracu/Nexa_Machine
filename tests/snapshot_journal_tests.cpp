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
    fs::remove_all(root);
    std::cout << "Snapshot journal subprocess recovery tests passed\n";
    return 0;
}
