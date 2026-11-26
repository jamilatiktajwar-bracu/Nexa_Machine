#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace nexa {class VirtualDisk {public:VirtualDisk(std::string,std::size_t);void load();void flush()const;void write(std::size_t,const std::vector<std::uint8_t>&);std::vector<std::uint8_t> read(std::size_t,std::size_t)const;std::size_t size()const noexcept{return bytes_.size();}private:std::string path_;std::vector<std::uint8_t>bytes_;};}
