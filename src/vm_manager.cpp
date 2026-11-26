#include "nexa/vm_manager.hpp"
#include "nexa/assembler.hpp"
#include <stdexcept>
#include <utility>
namespace nexa {
VMManager::VMManager(MachineConfig c):config_(std::move(c)),vm_(config_.ramBytes),disk_(config_.diskPath,config_.diskBytes){disk_.load();}
void VMManager::loadAssembly(const std::string&s){if(state_==MachineState::Running)throw std::logic_error("Cannot load while running");vm_.loadProgram(parseAssembly(s));state_=MachineState::Stopped;}
void VMManager::start(){if(state_==MachineState::Running)throw std::logic_error("Already running");state_=MachineState::Running;try{vm_.run();state_=vm_.halted()?MachineState::Halted:MachineState::Stopped;disk_.flush();}catch(...){state_=MachineState::Stopped;throw;}}
void VMManager::stop(){state_=MachineState::Stopped;}
void VMManager::reset(){vm_.reset();state_=MachineState::Stopped;}
void VMManager::saveSnapshot(const std::string&path)const{vm_.saveState(path);}
void VMManager::loadSnapshot(const std::string&path){if(state_==MachineState::Running)throw std::logic_error("Cannot restore while running");vm_.loadState(path);state_=vm_.halted()?MachineState::Halted:MachineState::Stopped;}
}
