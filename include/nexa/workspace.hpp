#pragma once
#include <filesystem>
#include <cstddef>
#include <string>
#include <vector>
namespace nexa {
struct WorkspacePaths {
 std::filesystem::path root, config, profiles, disks, snapshots, logs;
 static WorkspacePaths discover();
 void initialize() const;
 std::filesystem::path createProfile(const std::string& name, std::size_t ramBytes=256, std::size_t diskBytes=1048576) const;
 std::vector<std::string> listProfiles() const;
 std::filesystem::path snapshotPath(const std::string& profile, const std::string& snapshot) const;
 std::vector<std::string> listSnapshots(const std::string& profile) const;
 void deleteSnapshot(const std::string& profile, const std::string& snapshot) const;
 std::string summary() const;
};
}
