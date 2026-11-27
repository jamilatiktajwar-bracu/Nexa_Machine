#pragma once
#include <filesystem>
#include <string>
namespace nexa {
struct WorkspacePaths {
 std::filesystem::path root, config, profiles, disks, snapshots, logs;
 static WorkspacePaths discover();
 void initialize() const;
 std::string summary() const;
};
}
