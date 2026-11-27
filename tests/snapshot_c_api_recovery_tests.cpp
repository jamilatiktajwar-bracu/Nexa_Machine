#include "nexa/c_api.h"
#include "nexa/snapshot_journal.hpp"
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
static fs::path rootPath() { return fs::temp_directory_path() / "nexa-c-api-crash-recovery"; }
static void fillFile(const fs::path& path, unsigned char value) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    std::array<unsigned char, 512> bytes{}; bytes.fill(value);
    out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    out.flush();
    if (!out) throw std::runtime_error("failed to write disk fixture");
}
int main(int argc, char** argv) {
    const auto root = rootPath();
    const auto disk = root / "guest.disk";
    const auto backup = root / "guest.rollback";
    const auto journal = fs::path(disk.string() + ".nexa-journal");
    fs::create_directories(root);
    if (argc > 1 && std::string(argv[1]) == "--crash-child") {
        fillFile(disk, 0x41);
        nexa::snapshot::prepare(journal, {{disk, backup}});
        fillFile(disk, 0x42);
        nexa::snapshot::syncFile(disk);
        std::_Exit(91);
    }
    const std::string command = "\"" + std::string(argv[0]) + "\" --crash-child";
    const int childStatus = std::system(command.c_str());
    if (childStatus == 0 || !fs::exists(journal)) {
        std::cerr << "child did not leave an interrupted transaction\n"; return 1;
    }
    NexaContext* context = nexa_context_create();
    if (!context) return 2;
    uint64_t session = 0;
    if (nexa_session_create(context, "recovery-test", 1024, &session) != 0 ||
        nexa_disk_attach(context, session, disk.string().c_str(), 512) != 0) {
        std::cerr << "disk attachment recovery failed: " << nexa_last_error(context) << '\n';
        nexa_context_destroy(context); return 3;
    }
    std::array<uint8_t, 512> sector{};
    const int readStatus = nexa_disk_read_sector(context, session, 0, sector.data());
    nexa_context_destroy(context);
    if (readStatus != 0 || sector[0] != 0x41 || fs::exists(journal) || fs::exists(backup)) {
        std::cerr << "C API attachment did not recover the original disk\n";
        return 4;
    }
    fs::remove_all(root);
    std::cout << "C API crash recovery on disk attachment passed\n";
    return 0;
}
