#pragma once
#include <filesystem>
#include <string>
namespace nexa {
struct WorkspacePaths {
 std::filesystem::path root, config, profiles, disks, snapshots, logs;
 static WorkspacePaths discover();
 void initialize() const;
 std::filesystem::path createProfile(const std::string& name) const;
 std::vector<std::string> listProfiles() const;
 std::string summary() const;
};
}
