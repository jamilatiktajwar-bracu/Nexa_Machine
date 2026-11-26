#pragma once
#include "nexa/config.hpp"
#include "nexa/vm.hpp"
#include "nexa/virtual_disk.hpp"
namespace nexa {enum class MachineState{Stopped,Running,Halted};class VMManager{public:explicit VMManager(MachineConfig);void loadAssembly(const std::string&);void start();void stop();void reset();MachineState state()const noexcept{return state_;}const VM& machine()const noexcept{return vm_;}VirtualDisk& disk()noexcept{return disk_;}const MachineConfig&config()const noexcept{return config_;}private:MachineConfig config_;VM vm_;VirtualDisk disk_;MachineState state_{MachineState::Stopped};};}
