#pragma once
#include <cstddef>
#include <string>
namespace nexa { struct MachineConfig {std::string name{"Nexa-VM"};std::size_t ramBytes{256};std::string diskPath{"nexa.disk"};std::size_t diskBytes{1048576};static MachineConfig load(const std::string&);void save(const std::string&)const;};}
