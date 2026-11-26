#include "nexa/vm_manager.hpp"
#include "nexa/assembler.hpp"
#include <stdexcept>
#include <utility>
namespace nexa {VMManager::VMManager(MachineConfig c):config_(std::move(c)),vm_(config_.ramBytes),disk_(config_.diskPath,config_.diskBytes){disk_.load();}void VMManager::loadAssembly(const std::string&s){if(state_==MachineState::Running)throw std::logic_error("Cannot load while running");vm_.loadProgram(parseAssembly(s));state_=MachineState::Stopped;}void VMManager::start(){if(state_==MachineState::Running)throw std::logic_error("Already running");state_=MachineState::Running;vm_.run();state_=MachineState::Halted;disk_.flush();}void VMManager::stop(){state_=MachineState::Stopped;}void VMManager::reset(){vm_.reset();state_=MachineState::Stopped;}}
