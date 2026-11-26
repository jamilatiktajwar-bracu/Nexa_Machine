#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>
namespace nexa {
class VirtualFileSystem {
public:
 void createFile(const std::string& path,const std::vector<std::uint8_t>& data={});
 void writeFile(const std::string& path,const std::vector<std::uint8_t>& data);
 [[nodiscard]] std::vector<std::uint8_t> readFile(const std::string& path)const;
 [[nodiscard]] bool exists(const std::string& path)const;
 void removeFile(const std::string& path);
 [[nodiscard]] std::vector<std::string> list(const std::string& directory="/")const;
 void createDirectory(const std::string& path);
private:
 std::map<std::string,std::vector<std::uint8_t>> files_;
 std::vector<std::string> directories_{"/"};
 static std::string normalize(const std::string&);
};
}
