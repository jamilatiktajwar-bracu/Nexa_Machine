#pragma once
#include "nexa/vm.hpp"
#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <vector>
namespace nexa {
enum class SessionState { Ready, Running, Paused, Stopped, Halted, Faulted };
struct SessionInfo {
 std::string id, name;
 SessionState state{SessionState::Ready};
 std::size_t pc{0}, instructions{0}, ramBytes{0};
 bool halted{false};
};
class SessionManager {
public:
 std::string create(std::string name="Nexa session",std::size_t ramBytes=VM::DefaultRamSize);
 bool remove(const std::string& id);
 std::vector<SessionInfo> list()const;
 SessionInfo info(const std::string& id)const;
 void loadAssembly(const std::string& id,const std::string& source);
 void start(const std::string& id);
 void pause(const std::string& id);
 void resume(const std::string& id);
 void stop(const std::string& id);
 void reset(const std::string& id);
 bool step(const std::string& id);
 SessionState run(const std::string& id,std::size_t instructionBudget=100000);
 void saveSnapshot(const std::string& id,const std::string& path)const;
 void loadSnapshot(const std::string& id,const std::string& path);
 VM& machine(const std::string& id);
 const VM& machine(const std::string& id)const;
private:
 struct Session {std::string id,name;std::unique_ptr<VM>vm;SessionState state{SessionState::Ready};bool programLoaded{false};};
 std::map<std::string,Session>sessions_;
 std::size_t nextId_{1};
 Session& require(const std::string&);
 const Session& require(const std::string&)const;
};
}
