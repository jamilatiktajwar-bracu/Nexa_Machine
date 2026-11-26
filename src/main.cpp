#include "nexa/assembler.hpp"
#include "nexa/config.hpp"
#include "nexa/vm.hpp"
#include "nexa/vm_manager.hpp"
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
namespace {
const char* demo="LOAD A, 19\nLOAD B, 23\nADD A, B\nPRINT A\nHALT\n";
std::string readFile(const std::string&p){std::ifstream f(p);if(!f)throw std::runtime_error("Cannot open file: "+p);return {std::istreambuf_iterator<char>(f),{}};}
void console(){nexa::MachineConfig config; nexa::VMManager manager(config);std::string current;std::cout<<"Nexa Console 0.7. Type 'help'.\n";std::string line;while(true){std::cout<<"nexa> "<<std::flush;if(!std::getline(std::cin,line))break;std::stringstream ss(line);std::string cmd;ss>>cmd;if(cmd=="quit"||cmd=="exit")break;else if(cmd=="help")std::cout<<"help | load FILE | keyboard TEXT | run/continue | step | break ADDRESS | reset | status | mem ADDRESS | disk-read OFFSET COUNT | disk-write OFFSET BYTE... | disk-save | state-save FILE | state-load FILE | quit\n";else if(cmd=="load"){std::string file;ss>>file;try{current=readFile(file);manager.loadAssembly(current);std::cout<<"Loaded "<<file<<"\n";}catch(const std::exception&e){std::cerr<<e.what()<<"\n";}}else if(cmd=="keyboard"){std::string text;std::getline(ss,text);if(!text.empty()&&text.front()==' ')text.erase(0,1);manager.machine().queueKeyboardInput(text);std::cout<<"Queued "<<text.size()<<" input bytes.\\n";}else if(cmd=="step"){try{manager.machine().stepInstruction();std::cout<<"PC="<<manager.machine().programCounter()<<" A="<<unsigned(manager.machine().accumulator())<<" B="<<unsigned(manager.machine().secondRegister())<<"\\n";}catch(const std::exception&e){std::cerr<<e.what()<<"\\n";}}else if(cmd=="break"){std::size_t a;ss>>a;if(ss)manager.machine().setBreakpoint(a);else std::cout<<"Usage: break ADDRESS\\n";}else if(cmd=="run"||cmd=="continue"){if(current.empty()){std::cout<<"Load a program first.\n";continue;}try{manager.start();std::cout<<"Halted. instructions="<<manager.machine().instructionsExecuted()<<" A="<<unsigned(manager.machine().accumulator())<<"\n";}catch(const std::exception&e){std::cerr<<e.what()<<"\n";}}else if(cmd=="reset"){manager.reset();std::cout<<"Reset.\n";}else if(cmd=="status"){auto st=manager.state();std::cout<<"State="<<(st==nexa::MachineState::Stopped?"Stopped":st==nexa::MachineState::Running?"Running":"Halted")<<" RAM="<<manager.machine().ramSize()<<" bytes PC="<<manager.machine().programCounter()<<" Stack="<<manager.machine().stackDepth()<<" Instructions="<<manager.machine().instructionsExecuted()<<"\n";}else if(cmd=="mem"){unsigned a;ss>>a;if(ss&&a<=65535)std::cout<<"["<<a<<"]="<<unsigned(manager.machine().readMemory(static_cast<std::uint16_t>(a)))<<"\n";else std::cout<<"Usage: mem ADDRESS (0..65535)\n";}else if(cmd=="disk-read"){std::size_t o,n;ss>>o>>n;try{auto b=manager.disk().read(o,n);for(auto x:b)std::cout<<unsigned(x)<<' ';std::cout<<'\n';}catch(const std::exception&e){std::cerr<<e.what()<<'\n';}}else if(cmd=="disk-write"){std::size_t o;ss>>o;std::vector<std::uint8_t>b;unsigned x;while(ss>>x){if(x>255){std::cerr<<"Byte values must be 0..255\n";b.clear();break;}b.push_back(static_cast<std::uint8_t>(x));}try{manager.disk().write(o,b);std::cout<<"Written "<<b.size()<<" bytes.\n";}catch(const std::exception&e){std::cerr<<e.what()<<'\n';}}else if(cmd=="state-save"||cmd=="state-load"){std::string file;ss>>file;if(file.empty()){std::cout<<"Usage: "<<cmd<<" FILE\\n";continue;}try{if(cmd=="state-save"){manager.saveSnapshot(file);std::cout<<"State saved: "<<file<<"\\n";}else{manager.loadSnapshot(file);std::cout<<"State loaded: "<<file<<"\\n";}}catch(const std::exception&e){std::cerr<<e.what()<<"\\n";}}else if(cmd=="disk-save"){try{manager.disk().flush();std::cout<<"Disk image saved: "<<config.diskPath<<'\n';}catch(const std::exception&e){std::cerr<<e.what()<<'\n';}}else if(!cmd.empty())std::cout<<"Unknown command. Type help.\n";}}
}
int main(int argc,char**argv){
 try{
  if(argc==2&&std::string(argv[1])=="console"){console();return 0;}
  if(argc==2&&std::string(argv[1])=="--help"){std::cout<<"Usage: nexa [program.nexa] | nexa console | nexa --config config.json program.nexa\n";return 0;}
  std::string program=demo;nexa::MachineConfig config;
  if(argc==2)program=readFile(argv[1]);
  else if(argc==4&&std::string(argv[1])=="--config"){config=nexa::MachineConfig::load(argv[2]);program=readFile(argv[3]);}
  else if(argc!=1){std::cerr<<"Usage: nexa [program.nexa] | nexa console | nexa --config config.json program.nexa\n";return 2;}
  nexa::VM vm(config.ramBytes);vm.loadProgram(nexa::parseAssembly(program));vm.run();
  std::cout<<"VM halted. Instructions="<<vm.instructionsExecuted()<<" RAM="<<vm.ramSize()<<"\n";
 }catch(const std::exception&e){std::cerr<<"Nexa error: "<<e.what()<<'\n';return 1;}
 return 0;
}
