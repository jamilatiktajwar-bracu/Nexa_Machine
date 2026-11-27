#include "nexa/assembler.hpp"
#include "nexa/executable.hpp"
#include "nexa/config.hpp"
#include "nexa/vm.hpp"
#include "nexa/vm_manager.hpp"
#include "nexa/shell.hpp"
#include "nexa/platform.hpp"
#include "nexa/workspace.hpp"
#include <memory>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <algorithm>
#include <cctype>
#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif
namespace {
const char* demo="LOAD A, 19\nLOAD B, 23\nADD A, B\nPRINT A\nHALT\n";
bool safeProfileName(const std::string& name){return !name.empty()&&name.size()<=48&&std::all_of(name.begin(),name.end(),[](unsigned char c){return std::isalnum(c)||c=='-'||c=='_';});}
std::string readFile(const std::string&p){std::ifstream f(p);if(!f)throw std::runtime_error("Cannot open file: "+p);return {std::istreambuf_iterator<char>(f),{}};}
void miniOs(nexa::VMManager& manager,std::string& current){std::cout<<"NexaOS shell prototype. Type help.\n";std::string line;while(true){std::cout<<"nexa-os$ "<<std::flush;if(!std::getline(std::cin,line))return;std::stringstream ss(line);std::string cmd;ss>>cmd;if(cmd=="exit"||cmd=="quit")return;else if(cmd=="help")std::cout<<"help | echo TEXT | clear | run FILE | mem ADDRESS | exit\n";else if(cmd=="echo"){std::string rest;std::getline(ss,rest);if(!rest.empty()&&rest.front()==' ')rest.erase(0,1);std::cout<<rest<<"\n";}else if(cmd=="clear")std::cout<<"\033[2J\033[H";else if(cmd=="mem"){unsigned a;ss>>a;if(ss&&a<manager.machine().ramSize())std::cout<<unsigned(manager.machine().readMemory(a))<<"\n";else std::cout<<"address invalid\n";}else if(cmd=="run"){std::string file;ss>>file;try{current=readFile(file);manager.loadAssembly(current);manager.start();}catch(const std::exception&e){std::cerr<<e.what()<<"\n";}}else if(!cmd.empty())std::cout<<"Unknown command. Type help.\n";}}
std::string readSecret(const std::string& prompt) {
 std::cout<<prompt<<std::flush; std::string value;
#ifdef _WIN32
 for(;;) { int ch=_getch(); if(ch=='\r'||ch=='\n') break; if(ch==8) { if(!value.empty()) value.pop_back(); continue; } if(ch>=32&&ch<=126) value.push_back(static_cast<char>(ch)); }
#else
 termios oldState{},newState{};
 if(tcgetattr(STDIN_FILENO,&oldState)==0) {
  newState=oldState; newState.c_lflag&=static_cast<tcflag_t>(~ECHO);
  if(tcsetattr(STDIN_FILENO,TCSAFLUSH,&newState)==0) { std::getline(std::cin,value); tcsetattr(STDIN_FILENO,TCSAFLUSH,&oldState); }
  else std::getline(std::cin,value);
 } else std::getline(std::cin,value);
#endif
 std::cout<<"\n"; return value;
}
void osConsole(){nexa::NexaKernel kernel; nexa::NexaShell shell(kernel); std::cout<<"Nexa OS 2.3 user-space shell. Type help.\n"; std::string line; while(true){std::cout<<"nexa-os$ "<<std::flush;if(!std::getline(std::cin,line))break;if(line=="exit"||line=="quit")break;std::stringstream commandStream(line); std::string command,user,extra; commandStream>>command;
 if(command=="login"||command=="useradd") { commandStream>>user>>extra; if(!user.empty()&&extra.empty()) line+=" "+readSecret(command=="login"?"Password: ":"New password: "); }
 else if(command=="passwd") { commandStream>>user; if(user.empty()) line+=" "+readSecret("New password: "); }
 const auto result=shell.execute(line);if(!result.empty())std::cout<<result<<(result.back()=='\n'?"":"\n");}}
void console(){nexa::MachineConfig config; nexa::VMManager manager(config);std::string current;std::cout<<"Nexa Console 0.9. Type 'help'.\n";std::string line;while(true){std::cout<<"nexa> "<<std::flush;if(!std::getline(std::cin,line))break;std::stringstream ss(line);std::string cmd;ss>>cmd;if(cmd=="quit"||cmd=="exit")break;else if(cmd=="os"){miniOs(manager,current);}else if(cmd=="help")std::cout<<"help | os | load FILE | keyboard TEXT | run/continue | step | break ADDRESS | watch ADDRESS | unwatch ADDRESS | mem ADDRESS | mem-write ADDRESS BYTE | reset | status | disk-read OFFSET COUNT | disk-write OFFSET BYTE... | disk-save | state-save FILE | state-load FILE | quit\n";else if(cmd=="load"){std::string file;ss>>file;try{current=readFile(file);manager.loadAssembly(current);std::cout<<"Loaded "<<file<<"\n";}catch(const std::exception&e){std::cerr<<e.what()<<"\n";}}else if(cmd=="keyboard"){std::string text;std::getline(ss,text);if(!text.empty()&&text.front()==' ')text.erase(0,1);manager.machine().queueKeyboardInput(text);std::cout<<"Queued "<<text.size()<<" input bytes.\n";}else if(cmd=="step"){try{manager.machine().stepInstruction();std::cout<<"PC="<<manager.machine().programCounter()<<" A="<<unsigned(manager.machine().accumulator())<<" B="<<unsigned(manager.machine().secondRegister())<<"\n";}catch(const std::exception&e){std::cerr<<e.what()<<"\n";}}else if(cmd=="break"){std::size_t a;ss>>a;if(ss)manager.machine().setBreakpoint(a);else std::cout<<"Usage: break ADDRESS\n";}else if(cmd=="run"||cmd=="continue"){if(current.empty()){std::cout<<"Load a program first.\n";continue;}try{manager.start();std::cout<<(manager.machine().halted()?"Halted. ":"Paused. ")<<"instructions="<<manager.machine().instructionsExecuted()<<" A="<<unsigned(manager.machine().accumulator())<<"\n";}catch(const std::exception&e){std::cerr<<e.what()<<"\n";}}else if(cmd=="reset"){manager.reset();std::cout<<"Reset.\n";}else if(cmd=="status"){auto st=manager.state();std::cout<<"State="<<(st==nexa::MachineState::Stopped?"Stopped":st==nexa::MachineState::Running?"Running":"Halted")<<" RAM="<<manager.machine().ramSize()<<" bytes PC="<<manager.machine().programCounter()<<" Stack="<<manager.machine().stackDepth()<<" Instructions="<<manager.machine().instructionsExecuted()<<"\n";}else if(cmd=="mem"){unsigned a;ss>>a;if(ss&&a<manager.machine().ramSize())std::cout<<"["<<a<<"]="<<unsigned(manager.machine().readMemory(static_cast<std::uint16_t>(a)))<<"\n";else std::cout<<"Usage: mem ADDRESS (within RAM)\n";}else if(cmd=="mem-write"){unsigned a,v;ss>>a>>v;try{if(!ss||a>=manager.machine().ramSize()||v>255)throw std::invalid_argument("Usage: mem-write ADDRESS BYTE");manager.machine().writeMemory(static_cast<std::uint16_t>(a),static_cast<std::uint8_t>(v));std::cout<<"Memory updated\n";}catch(const std::exception&e){std::cerr<<e.what()<<"\n";}}else if(cmd=="watch"){unsigned a;ss>>a;try{if(!ss||a>=manager.machine().ramSize())throw std::invalid_argument("Usage: watch ADDRESS");manager.machine().setWatchpoint(static_cast<std::uint16_t>(a));std::cout<<"Watchpoint set\n";}catch(const std::exception&e){std::cerr<<e.what()<<"\n";}}else if(cmd=="unwatch"){unsigned a;ss>>a;if(ss)manager.machine().clearWatchpoint(static_cast<std::uint16_t>(a));}else if(cmd=="disk-read"){std::size_t o,n;ss>>o>>n;try{auto b=manager.disk().read(o,n);for(auto x:b)std::cout<<unsigned(x)<<' ';std::cout<<'\n';}catch(const std::exception&e){std::cerr<<e.what()<<'\n';}}else if(cmd=="disk-write"){std::size_t o;ss>>o;std::vector<std::uint8_t>b;unsigned x;while(ss>>x){if(x>255){std::cerr<<"Byte values must be 0..255\n";b.clear();break;}b.push_back(static_cast<std::uint8_t>(x));}try{manager.disk().write(o,b);std::cout<<"Written "<<b.size()<<" bytes.\n";}catch(const std::exception&e){std::cerr<<e.what()<<'\n';}}else if(cmd=="state-save"||cmd=="state-load"){std::string file;ss>>file;if(file.empty()){std::cout<<"Usage: "<<cmd<<" FILE\n";continue;}try{if(cmd=="state-save"){manager.saveSnapshot(file);std::cout<<"State saved: "<<file<<"\n";}else{manager.loadSnapshot(file);std::cout<<"State loaded: "<<file<<"\n";}}catch(const std::exception&e){std::cerr<<e.what()<<"\n";}}else if(cmd=="disk-save"){try{manager.disk().flush();std::cout<<"Disk image saved: "<<config.diskPath<<'\n';}catch(const std::exception&e){std::cerr<<e.what()<<'\n';}}else if(!cmd.empty())std::cout<<"Unknown command. Type help.\n";}}
}
int main(int argc,char**argv){
 try{
  if(argc==2&&std::string(argv[1])=="console"){console();return 0;}
  if(argc==2&&std::string(argv[1])=="--os"){osConsole();return 0;}
  if(argc==4&&std::string(argv[1])=="--compile"){auto source=readFile(argv[2]);nexa::Executable e{0,nexa::parseAssembly(source)};nexa::saveExecutable(e,argv[3]);std::cout<<"Compiled "<<e.code.size()<<" instructions to "<<argv[3]<<"\n";return 0;}
  if(argc==3&&std::string(argv[1])=="--exe"){auto e=nexa::loadExecutable(argv[2]);nexa::VM vm;vm.loadProgram(e.code);vm.setProgramCounter(e.entryPoint);vm.run();std::cout<<"Executable halted. Instructions="<<vm.instructionsExecuted()<<"\n";return 0;}
  if((argc==3||argc==5)&&std::string(argv[1])=="--profile-create"){std::size_t ram=256,disk=1048576;if(argc==5){ram=std::stoull(argv[3]);disk=std::stoull(argv[4]);}auto path=nexa::WorkspacePaths::discover().createProfile(argv[2],ram,disk);std::cout<<"Profile created: "<<path.string()<<"\n";return 0;}
  if(argc==4&&std::string(argv[1])=="--profile-run"){auto paths=nexa::WorkspacePaths::discover();if(!safeProfileName(argv[2]))throw std::invalid_argument("Invalid profile name");auto profilePath=paths.profiles/(std::string(argv[2])+".json");if(!std::filesystem::exists(profilePath))throw std::runtime_error("Profile not found: "+std::string(argv[2]));auto config=nexa::MachineConfig::load(profilePath.string());auto source=readFile(argv[3]);nexa::VMManager manager(config);manager.loadAssembly(source);manager.start();std::cout<<"Profile "<<config.name<<" halted. Instructions="<<manager.machine().instructionsExecuted()<<" RAM="<<manager.machine().ramSize()<<" disk="<<config.diskPath<<"\n";return 0;}
  if(argc==3&&std::string(argv[1])=="--profile-show"){auto paths=nexa::WorkspacePaths::discover();if(!safeProfileName(argv[2]))throw std::invalid_argument("Invalid profile name");auto profilePath=paths.profiles/(std::string(argv[2])+".json");if(!std::filesystem::exists(profilePath))throw std::runtime_error("Profile not found: "+std::string(argv[2]));auto config=nexa::MachineConfig::load(profilePath.string());std::cout<<"Name: "<<config.name<<"\nRAM: "<<config.ramBytes<<" bytes\nDisk: "<<config.diskPath<<"\nDisk size: "<<config.diskBytes<<" bytes\n";return 0;}
  if(argc==2&&std::string(argv[1])=="--profile-list"){auto names=nexa::WorkspacePaths::discover().listProfiles();if(names.empty())std::cout<<"No profiles yet. Create one with --profile-create NAME.\n";else for(const auto& name:names)std::cout<<name<<"\n";return 0;}
  if(argc==2&&std::string(argv[1])=="--workspace"){std::cout<<nexa::WorkspacePaths::discover().summary()<<"\n";return 0;}
  if(argc==2&&std::string(argv[1])=="--init-workspace"){auto paths=nexa::WorkspacePaths::discover();paths.initialize();std::cout<<"Workspace initialized.\n"<<paths.summary()<<"\n";return 0;}
  if(argc==2&&std::string(argv[1])=="--platform"){std::cout<<nexa::platformSummary()<<"\n";return 0;}
  if(argc==2&&std::string(argv[1])=="--help"){std::cout<<"Usage: nexa --profile-create NAME [RAM_BYTES DISK_BYTES] | nexa --profile-list | nexa --profile-show NAME | nexa --profile-run NAME SOURCE | nexa --workspace | nexa --init-workspace | nexa --platform | nexa [program.nexa] | nexa --compile source.nexa output.nexe | nexa --exe program.nexe | nexa console | nexa --os | nexa --config config.json program.nexa\n";return 0;}
  std::string program=demo;nexa::MachineConfig config;
  if(argc==2)program=readFile(argv[1]);
  else if(argc==4&&std::string(argv[1])=="--config"){config=nexa::MachineConfig::load(argv[2]);program=readFile(argv[3]);}
  else if(argc!=1){std::cerr<<"Usage: nexa [program.nexa] | nexa --compile source.nexa output.nexe | nexa --exe program.nexe | nexa console | nexa --os | nexa --config config.json program.nexa\n";return 2;}
  nexa::VM vm(config.ramBytes);vm.loadProgram(nexa::parseAssembly(program));vm.run();
  std::cout<<"VM halted. Instructions="<<vm.instructionsExecuted()<<" RAM="<<vm.ramSize()<<"\n";
 }catch(const std::exception&e){std::cerr<<"Nexa error: "<<e.what()<<'\n';return 1;}
 return 0;
}
