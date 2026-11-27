#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace nexa::snapshot {
struct JournalEntry {
    std::filesystem::path target;
    std::filesystem::path backup;
    std::uintmax_t size{};
    std::uint64_t checksum{};
};
enum class JournalPhase { Prepared, Committed };

inline std::uint64_t checksum(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot read snapshot journal file: " + path.string());
    std::uint64_t hash = 14695981039346656037ull;
    char buffer[8192];
    while (in) {
        in.read(buffer, sizeof(buffer));
        for (std::streamsize i = 0; i < in.gcount(); ++i) {
            hash ^= static_cast<unsigned char>(buffer[i]);
            hash *= 1099511628211ull;
        }
    }
    if (!in.eof()) throw std::runtime_error("Failed reading snapshot journal file");
    return hash;
}
inline void syncFile(const std::filesystem::path& path) {
#ifdef _WIN32
    const int fd = _open(path.string().c_str(), _O_RDWR | _O_BINARY);
    if (fd < 0) throw std::runtime_error("Cannot open file for durable flush: " + path.string());
    const int result = _commit(fd); _close(fd);
#else
    const int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) throw std::runtime_error("Cannot open file for durable flush: " + path.string());
    const int result = ::fsync(fd); ::close(fd);
#endif
    if (result != 0) throw std::runtime_error("Durable file flush failed: " + path.string());
}
inline void syncDirectory(const std::filesystem::path& directory) {
#ifndef _WIN32
    const auto dir = directory.empty() ? std::filesystem::path(".") : directory;
    const int fd = ::open(dir.c_str(), O_RDONLY | O_DIRECTORY);
    if (fd < 0) throw std::runtime_error("Cannot open journal directory for durable flush");
    const int result = ::fsync(fd); ::close(fd);
    if (result != 0) throw std::runtime_error("Journal directory flush failed");
#else
    (void)directory; // MoveFileExW(..., MOVEFILE_WRITE_THROUGH) flushes publication.
#endif
}
inline void writeJournal(const std::filesystem::path& journal, JournalPhase phase,
                         const std::vector<JournalEntry>& entries) {
    const auto temp = std::filesystem::path(journal.string() + ".tmp");
    {
        std::ofstream out(temp, std::ios::trunc);
        if (!out) throw std::runtime_error("Cannot create snapshot transaction journal");
        out << "NEXA_SNAPSHOT_JOURNAL 1\n"
            << (phase == JournalPhase::Prepared ? "PREPARED" : "COMMITTED") << '\n'
            << entries.size() << '\n';
        for (const auto& e : entries)
            out << std::quoted(e.target.string()) << ' '
                << std::quoted(e.backup.string()) << ' ' << e.size << ' ' << e.checksum << '\n';
        out.flush();
        if (!out) throw std::runtime_error("Failed writing snapshot transaction journal");
    }
    syncFile(temp);
#ifdef _WIN32
    if (!MoveFileExW(temp.c_str(), journal.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Cannot atomically publish snapshot transaction journal");
#else
    std::error_code ec;
    std::filesystem::rename(temp, journal, ec);
    if (ec) throw std::runtime_error("Cannot publish snapshot transaction journal: " + ec.message());
    syncDirectory(journal.parent_path());
#endif
}
inline std::vector<JournalEntry> readJournal(const std::filesystem::path& journal,
                                             JournalPhase& phase) {
    std::ifstream in(journal);
    std::string magic, state;
    unsigned version = 0;
    std::size_t count = 0;
    in >> magic >> version >> state >> count;
    if (!in || magic != "NEXA_SNAPSHOT_JOURNAL" || version != 1 ||
        (state != "PREPARED" && state != "COMMITTED") || count > 1024)
        throw std::runtime_error("Invalid snapshot transaction journal");
    phase = state == "PREPARED" ? JournalPhase::Prepared : JournalPhase::Committed;
    std::vector<JournalEntry> entries;
    entries.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        std::string target, backup;
        JournalEntry e;
        in >> std::quoted(target) >> std::quoted(backup) >> e.size >> e.checksum;
        if (!in || target.empty() || backup.empty())
            throw std::runtime_error("Invalid snapshot transaction journal entry");
        e.target = target; e.backup = backup;
        entries.push_back(std::move(e));
    }
    std::string extra;
    if (in >> extra) throw std::runtime_error("Unexpected trailing snapshot journal data");
    return entries;
}
inline void recoverCoordinator(const std::filesystem::path& journal) {
    if (!std::filesystem::exists(journal)) return;
    JournalPhase phase{};
    const auto entries = readJournal(journal, phase);
    if (phase == JournalPhase::Prepared) {
        for (const auto& e : entries) {
            if (!std::filesystem::exists(e.backup) ||
                std::filesystem::file_size(e.backup) != e.size ||
                checksum(e.backup) != e.checksum)
                throw std::runtime_error("Snapshot rollback backup is missing or corrupt: " + e.backup.string());
        }
        for (const auto& e : entries) {
            std::filesystem::copy_file(e.backup, e.target,
                std::filesystem::copy_options::overwrite_existing);
            syncFile(e.target);
            syncDirectory(e.target.parent_path());
        }
    }
    const auto linksFile = std::filesystem::path(journal.string() + ".links");
    if (std::filesystem::exists(linksFile)) {
        std::ifstream links(linksFile);
        std::string sidecar;
        while (std::getline(links, sidecar)) {
            if (!sidecar.empty()) {
                std::filesystem::remove(sidecar);
                syncDirectory(std::filesystem::path(sidecar).parent_path());
            }
        }
    }
    for (const auto& e : entries) {
        std::filesystem::remove(e.backup);
        syncDirectory(e.backup.parent_path());
    }
    std::filesystem::remove(journal);
    std::filesystem::remove(linksFile);
    syncDirectory(journal.parent_path());
}
inline void writeJournalLink(const std::filesystem::path& sidecar,
                             const std::filesystem::path& coordinator) {
    const auto temp = std::filesystem::path(sidecar.string() + ".tmp");
    {
        std::ofstream out(temp, std::ios::trunc);
        if (!out) throw std::runtime_error("Cannot create snapshot journal link");
        out << "NEXA_SNAPSHOT_LINK 1\n" << std::quoted(coordinator.string()) << '\n';
        out.flush();
        if (!out) throw std::runtime_error("Failed writing snapshot journal link");
    }
    syncFile(temp);
#ifdef _WIN32
    if (!MoveFileExW(temp.c_str(), sidecar.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Cannot publish snapshot journal link");
#else
    std::error_code ec;
    std::filesystem::rename(temp, sidecar, ec);
    if (ec) throw std::runtime_error("Cannot publish snapshot journal link: " + ec.message());
    syncDirectory(sidecar.parent_path());
#endif
}
inline void recover(const std::filesystem::path& journal) {
    if (!std::filesystem::exists(journal)) return;
    std::ifstream in(journal);
    std::string magic;
    in >> magic;
    if (magic == "NEXA_SNAPSHOT_LINK") {
        unsigned version = 0;
        std::string coordinator;
        in >> version >> std::quoted(coordinator);
        if (!in || version != 1 || coordinator.empty())
            throw std::runtime_error("Invalid snapshot journal link");
        recoverCoordinator(coordinator);
        return;
    }
    recoverCoordinator(journal);
}
inline std::vector<JournalEntry> prepareGroup(
    const std::filesystem::path& coordinator,
    const std::vector<std::pair<std::filesystem::path,
                                std::pair<std::filesystem::path, std::filesystem::path>>>& files) {
    std::vector<JournalEntry> entries;
    std::vector<std::filesystem::path> sidecars;
    entries.reserve(files.size());
    for (const auto& item : files) {
        const auto& sidecar = item.first;
        const auto& target = item.second.first;
        const auto& backup = item.second.second;
        if (!std::filesystem::exists(target))
            throw std::runtime_error("Snapshot transaction target is missing: " + target.string());
        std::filesystem::copy_file(target, backup, std::filesystem::copy_options::overwrite_existing);
        syncFile(backup);
        syncDirectory(backup.parent_path());
        entries.push_back({target, backup, std::filesystem::file_size(backup), checksum(backup)});
        sidecars.push_back(sidecar);
    }
    const auto linksFile = std::filesystem::path(coordinator.string() + ".links");
    {
        std::ofstream links(linksFile, std::ios::trunc);
        if (!links) throw std::runtime_error("Cannot create coordinated journal link list");
        for (const auto& sidecar : sidecars) links << sidecar.string() << '\n';
        links.flush();
        if (!links) throw std::runtime_error("Failed writing coordinated journal link list");
    }
    syncFile(linksFile);
    syncDirectory(linksFile.parent_path());
    writeJournal(coordinator, JournalPhase::Prepared, entries);
    for (const auto& sidecar : sidecars) writeJournalLink(sidecar, coordinator);
    return entries;
}
inline void commitGroup(const std::filesystem::path& coordinator) {
    if (!std::filesystem::exists(coordinator)) return;
    JournalPhase phase{};
    const auto entries = readJournal(coordinator, phase);
    writeJournal(coordinator, JournalPhase::Committed, entries);
    for (const auto& e : entries) {
        std::filesystem::remove(e.backup);
        syncDirectory(e.backup.parent_path());
    }
    const auto linksFile = std::filesystem::path(coordinator.string() + ".links");
    if (std::filesystem::exists(linksFile)) {
        std::ifstream links(linksFile);
        std::string sidecar;
        while (std::getline(links, sidecar)) if (!sidecar.empty()) {
            std::filesystem::remove(sidecar);
            syncDirectory(std::filesystem::path(sidecar).parent_path());
        }
    }
    std::filesystem::remove(coordinator);
    std::filesystem::remove(linksFile);
    syncDirectory(coordinator.parent_path());
}
inline std::vector<JournalEntry> prepare(const std::filesystem::path& journal,
    const std::vector<std::pair<std::filesystem::path, std::filesystem::path>>& files) {
    std::vector<JournalEntry> entries;
    entries.reserve(files.size());
    for (const auto& [target, backup] : files) {
        if (!std::filesystem::exists(target))
            throw std::runtime_error("Snapshot transaction target is missing: " + target.string());
        std::filesystem::copy_file(target, backup, std::filesystem::copy_options::overwrite_existing);
        syncFile(backup);
        syncDirectory(backup.parent_path());
        entries.push_back({target, backup, std::filesystem::file_size(backup), checksum(backup)});
    }
    writeJournal(journal, JournalPhase::Prepared, entries);
    return entries;
}
inline void commit(const std::filesystem::path& journal,
                   const std::vector<JournalEntry>& entries) {
    writeJournal(journal, JournalPhase::Committed, entries);
    for (const auto& e : entries) {
        std::filesystem::remove(e.backup);
        syncDirectory(e.backup.parent_path());
    }
    std::filesystem::remove(journal);
    syncDirectory(journal.parent_path());
}
} // namespace nexa::snapshot
