#include "nexa/session_gui.hpp"
#include "nexa/session_manager.hpp"
#include "nexa/virtual_disk.hpp"
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QTimer>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDateTime>
#include <QGroupBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QDir>
#include <filesystem>
#include <map>
#include <memory>
#include <deque>
#include <functional>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <cstdint>
#include <QStringList>
namespace nexa {
namespace {
struct DiskEntry { std::unique_ptr<VirtualDisk> disk; QString path; };
class SessionPage final : public QWidget {
public:
 explicit SessionPage(QString name,QWidget* parent=nullptr):QWidget(parent),name_(std::move(name)),id_(manager_.create(name_.toStdString(),1024)) {
  auto* root=new QVBoxLayout(this); auto* controls=new QHBoxLayout;
  auto* start=new QPushButton("Start");auto* pause=new QPushButton("Pause");auto* resume=new QPushButton("Resume");auto* step=new QPushButton("Step");auto* reset=new QPushButton("Reset");auto* stop=new QPushButton("Stop");
  auto* attach=new QPushButton("Attach disk");auto* detach=new QPushButton("Detach disk");auto* flush=new QPushButton("Flush disk");auto* statusDisk=new QPushButton("Device status");
  auto* save=new QPushButton("Save VM + disk snapshot");auto* restore=new QPushButton("Restore VM + disk snapshot");auto* reload=new QPushButton("Reload source");auto* setBreakpoint=new QPushButton("Set breakpoint");auto* clearBreakpoints=new QPushButton("Clear breakpoints");auto* breakpointAddress=new QLineEdit;breakpointAddress->setPlaceholderText("PC address");breakpointAddress->setMaximumWidth(90);auto* guestInput=new QLineEdit;guestInput->setPlaceholderText("Guest keyboard input");guestInput->setMaximumWidth(150);auto* sendInput=new QPushButton("Send input");
  for(auto*b:{start,pause,resume,step,reset,stop,reload,breakpointAddress,setBreakpoint,clearBreakpoints,guestInput,sendInput,submitDisk,attach,detach,flush,statusDisk,save,restore})controls->addWidget(b);
  root->addLayout(controls);
  auto* split=new QHBoxLayout; editor_=new QPlainTextEdit;editor_->setPlainText("LOAD A, 19\nLOAD B, 23\nADD A, B\nPRINT A\nHALT\n");
  output_=new QTextEdit;output_->setReadOnly(true);memory_=new QTextEdit;memory_->setReadOnly(true);memory_->setMaximumWidth(260);
  auto* right=new QVBoxLayout;right->addWidget(new QLabel("Console"));right->addWidget(output_);right->addWidget(new QLabel("RAM preview"));right->addWidget(memory_);
  split->addWidget(editor_,2);split->addLayout(right,1);root->addLayout(split,1);
  diskTable_=new QTableWidget(0,4);diskTable_->setHorizontalHeaderLabels({"Device","Image","Capacity","State"});diskTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);diskTable_->setMaximumHeight(150);root->addWidget(diskTable_);
  status_=new QLabel("Ready");root->addWidget(status_);
  timer_.setInterval(15);connect(&timer_,&QTimer::timeout,this,[this]{tick();});
  connect(start,&QPushButton::clicked,this,[this]{try{if(!loaded_){manager_.loadAssembly(id_,editor_->toPlainText().toStdString());loaded_=true;}manager_.start(id_);autoRun_=true;timer_.start();status_->setText("Running");}catch(const std::exception&e){error(e.what());}});
  connect(pause,&QPushButton::clicked,this,[this]{try{autoRun_=false;timer_.stop();manager_.pause(id_);status_->setText("Paused");}catch(const std::exception&e){error(e.what());}});
  connect(resume,&QPushButton::clicked,this,[this]{try{manager_.resume(id_);autoRun_=true;timer_.start();status_->setText("Resumed");}catch(const std::exception&e){error(e.what());}});
  connect(step,&QPushButton::clicked,this,[this]{try{if(!loaded_){manager_.loadAssembly(id_,editor_->toPlainText().toStdString());loaded_=true;}std::ostringstream capture;auto*old=std::cout.rdbuf(capture.rdbuf());try{manager_.step(id_);}catch(...){std::cout.rdbuf(old);throw;}std::cout.rdbuf(old);if(!capture.str().empty())output_->append(QString::fromStdString(capture.str()));refresh();}catch(const std::exception&e){error(e.what());}});
  connect(reset,&QPushButton::clicked,this,[this]{try{autoRun_=false;timer_.stop();manager_.reset(id_);loaded_=true;output_->clear();refresh();status_->setText("Reset");}catch(const std::exception&e){error(e.what());}});
  connect(stop,&QPushButton::clicked,this,[this]{try{autoRun_=false;timer_.stop();manager_.stop(id_);refresh();status_->setText("Stopped — reset before restarting");}catch(const std::exception&e){error(e.what());}});
  connect(attach,&QPushButton::clicked,this,[this]{attachDisk();});connect(detach,&QPushButton::clicked,this,[this]{detachDisk();});connect(flush,&QPushButton::clicked,this,[this]{enqueue([this]{for(auto&[id,d]:disks_)d.disk->flush();status_->setText("All attached disks flushed");});});
  connect(statusDisk,&QPushButton::clicked,this,[this]{refreshDisks();QMessageBox::information(this,"Device status",QString("%1 disk device(s) attached. Queued I/O: %2").arg(disks_.size()).arg(ioQueue_.size()));});
  connect(save,&QPushButton::clicked,this,[this]{snapshot(true);});connect(restore,&QPushButton::clicked,this,[this]{snapshot(false);});
  connect(reload,&QPushButton::clicked,this,[this]{try{autoRun_=false;timer_.stop();if(manager_.info(id_).state==SessionState::Running)manager_.pause(id_);manager_.loadAssembly(id_,editor_->toPlainText().toStdString());loaded_=true;output_->clear();refresh();status_->setText("Source reloaded");}catch(const std::exception&e){error(e.what());}});
  connect(setBreakpoint,&QPushButton::clicked,this,[this,breakpointAddress]{bool ok=false;auto address=breakpointAddress->text().toULongLong(&ok);if(!ok){error("Invalid breakpoint address");return;}try{manager_.machine(id_).setBreakpoint(static_cast<std::size_t>(address));status_->setText(QString("Breakpoint set at %1").arg(address));}catch(const std::exception&e){error(e.what());}});
  connect(clearBreakpoints,&QPushButton::clicked,this,[this]{manager_.machine(id_).clearBreakpoints();status_->setText("Breakpoints cleared");});
  auto submitInput=[this,guestInput]{manager_.machine(id_).queueKeyboardInput(guestInput->text().toStdString()+"\n");guestInput->clear();status_->setText("Guest input queued");};connect(sendInput,&QPushButton::clicked,this,submitInput);connect(guestInput,&QLineEdit::returnPressed,this,submitInput);\n  connect(submitDisk,&QPushButton::clicked,this,[this]{submitGuestDiskRequest();});
 }
 ~SessionPage() override {timer_.stop();try{for(auto&[id,d]:disks_)d.disk->flush();}catch(...){}}
private:
 void error(const std::string&s){status_->setText("Error");QMessageBox::warning(this,"Nexa session",QString::fromStdString(s));}
 void enqueue(std::function<void()> task){ioQueue_.push_back(std::move(task));if(!timer_.isActive())timer_.start();}
 void submitGuestDiskRequest(){bool ok=false;int descriptor=QInputDialog::getInt(this,"Guest disk request","11-byte descriptor address:",0,0,65525,1,&ok);if(!ok)return;int vector=QInputDialog::getInt(this,"Guest disk request","Completion interrupt vector:",9,0,255,1,&ok);if(!ok)return;try{auto&vm=manager_.machine(id_);auto byte=[&](int offset){return vm.readMemory(static_cast<uint16_t>(descriptor+offset));};const bool write=byte(0)!=0;const uint32_t device=static_cast<uint32_t>(byte(1))|(static_cast<uint32_t>(byte(2))<<8)|(static_cast<uint32_t>(byte(3))<<16)|(static_cast<uint32_t>(byte(4))<<24);const uint32_t sector=static_cast<uint32_t>(byte(5))|(static_cast<uint32_t>(byte(6))<<8)|(static_cast<uint32_t>(byte(7))<<16)|(static_cast<uint32_t>(byte(8))<<24);const uint16_t buffer=static_cast<uint16_t>(byte(9)|(static_cast<uint16_t>(byte(10))<<8));if(static_cast<std::size_t>(buffer)+512>vm.ramSize())throw std::out_of_range("Guest disk buffer exceeds RAM");const auto request=nextDiskRequest_++;enqueue([this,write,device,sector,buffer,vector,request]{auto&guest=manager_.machine(id_);auto it=disks_.find(device);if(it==disks_.end())throw std::runtime_error("Guest request references an unattached disk");auto&disk=*it->second.disk;if(sector>=disk.size()/512)throw std::out_of_range("Guest disk sector outside image");if(write){std::vector<uint8_t>data(512);for(std::size_t i=0;i<512;++i)data[i]=guest.readMemory(static_cast<uint16_t>(buffer+i));disk.write(static_cast<std::size_t>(sector)*512,data);}else{auto data=disk.read(static_cast<std::size_t>(sector)*512,512);for(std::size_t i=0;i<512;++i)guest.writeMemory(static_cast<uint16_t>(buffer+i),data[i]);}guest.triggerInterrupt(static_cast<uint8_t>(vector));status_->setText(QString("Guest disk request %1 completed").arg(request));});status_->setText(QString("Guest disk request %1 queued").arg(request));}catch(const std::exception&e){error(e.what());}}
 void attachDisk(){QString path=QFileDialog::getSaveFileName(this,"Attach or create disk image",QString(),"Disk images (*.disk *.img);;All files (*)");if(path.isEmpty())return;bool ok=false;int mib=QInputDialog::getInt(this,"Disk capacity","Capacity (MiB):",16,1,128,1,&ok);if(!ok)return;uint32_t device=0;while(disks_.contains(device))++device;const auto capacity=static_cast<std::size_t>(mib)*1024*1024;enqueue([this,path,capacity,device]{auto disk=std::make_unique<VirtualDisk>(path.toStdString(),capacity);disk->load();disk->flush();disks_.emplace(device,DiskEntry{std::move(disk),path});refreshDisks();status_->setText(QString("Device %1 attached").arg(device));});}
 void detachDisk(){if(disks_.empty())return;QStringList options;for(const auto&[id,d]:disks_)options<<QString::number(id)+" — "+d.path;bool ok=false;QString choice=QInputDialog::getItem(this,"Detach disk","Device:",options,0,false,&ok);if(!ok)return;bool flushFirst=QMessageBox::question(this,"Detach disk","Flush pending image changes before detaching?")==QMessageBox::Yes;uint32_t id=choice.section(' ',0,0).toUInt();enqueue([this,id,flushFirst]{auto it=disks_.find(id);if(it==disks_.end())return;if(flushFirst)it->second.disk->flush();disks_.erase(it);refreshDisks();status_->setText(QString("Device %1 detached").arg(id));});}
 void refreshDisks(){diskTable_->setRowCount(static_cast<int>(disks_.size()));int row=0;for(const auto&[id,d]:disks_){diskTable_->setItem(row,0,new QTableWidgetItem(QString::number(id)));diskTable_->setItem(row,1,new QTableWidgetItem(QFileInfo(d.path).fileName()));diskTable_->setItem(row,2,new QTableWidgetItem(QString::number(d.disk->size())+" bytes"));diskTable_->setItem(row,3,new QTableWidgetItem("Attached"));++row;}}
 void snapshot(bool save){QString base=QFileDialog::getExistingDirectory(this,save?"Snapshot destination":"Snapshot directory");if(base.isEmpty())return;QString safeName=name_;safeName.replace('/','_');const QString stem=QDir(base).filePath(safeName);enqueue([this,stem,save]{try{if(save){for(const auto&[id,d]:disks_)d.disk->flush();manager_.saveSnapshot(id_,(stem+".vm").toStdString());QDir().mkpath(stem);for(const auto&[id,d]:disks_)std::filesystem::copy_file(d.path.toStdString(),(stem+"/device-"+QString::number(id)+".disk").toStdString(),std::filesystem::copy_options::overwrite_existing);}else{manager_.loadSnapshot(id_,(stem+".vm").toStdString());for(auto&[id,d]:disks_){auto src=(stem+"/device-"+QString::number(id)+".disk").toStdString();if(!std::filesystem::exists(src))throw std::runtime_error("Snapshot is missing disk device "+std::to_string(id));std::filesystem::copy_file(src,d.path.toStdString(),std::filesystem::copy_options::overwrite_existing);d.disk->load();}loaded_=true;refresh();}status_->setText(save?"VM + disk snapshot saved":"VM + disk snapshot restored");}catch(const std::exception&e){error(e.what());}});}
 void tick(){try{if(!ioQueue_.empty()){auto task=std::move(ioQueue_.front());ioQueue_.pop_front();task();}if(autoRun_){auto info=manager_.info(id_);if(info.state==SessionState::Paused)manager_.resume(id_);std::ostringstream capture;auto*old=std::cout.rdbuf(capture.rdbuf());try{manager_.run(id_,1);}catch(...){std::cout.rdbuf(old);throw;}std::cout.rdbuf(old);auto produced=capture.str();if(!produced.empty())output_->append(QString::fromStdString(produced));refresh();if(manager_.info(id_).state==SessionState::Halted){autoRun_=false;status_->setText("Halted");}else if(manager_.machine(id_).hasBreakpoint(manager_.info(id_).pc)){autoRun_=false;status_->setText(QString("Breakpoint at %1").arg(manager_.info(id_).pc));}}if(ioQueue_.empty()&&!autoRun_)timer_.stop();}catch(const std::exception&e){autoRun_=false;timer_.stop();error(e.what());}}
 void refresh(){const auto&vm=manager_.machine(id_);QString text;for(std::size_t a=0;a<std::min<std::size_t>(128,vm.ramSize());a+=16){text+=QString("%1: ").arg(a,4,16,QChar('0'));for(std::size_t j=0;j<16&&a+j<vm.ramSize();++j)text+=QString("%1 ").arg(vm.readMemory(static_cast<uint16_t>(a+j)),2,16,QChar('0'));text+='\n';}memory_->setPlainText(text.toUpper());status_->setText(QString("PC=%1  A=%2  B=%3  Instructions=%4").arg(vm.programCounter()).arg(vm.accumulator()).arg(vm.secondRegister()).arg(vm.instructionsExecuted()));}
 SessionManager manager_;std::string id_;QString name_;QTimer timer_;QPlainTextEdit*editor_{};QTextEdit*output_{};QTextEdit*memory_{};QLabel*status_{};QTableWidget*diskTable_{};std::map<uint32_t,DiskEntry>disks_;std::deque<std::function<void()>>ioQueue_;uint64_t nextDiskRequest_{1};bool loaded_{false},autoRun_{false};
};
}
SessionWorkspace::SessionWorkspace(QWidget*parent):QWidget(parent){auto*layout=new QVBoxLayout(this);auto*bar=new QHBoxLayout;auto*tabs=new QTabWidget;auto*add=new QPushButton("New VM session");auto*remove=new QPushButton("Close session");bar->addWidget(add);bar->addWidget(remove);bar->addStretch();layout->addLayout(bar);layout->addWidget(tabs);auto create=[tabs]{auto n=tabs->count()+1;tabs->addTab(new SessionPage(QString("VM %1").arg(n)),QString("VM %1").arg(n));tabs->setCurrentIndex(tabs->count()-1);};connect(add,&QPushButton::clicked,this,create);connect(remove,&QPushButton::clicked,this,[tabs]{if(tabs->count()){auto*w=tabs->currentWidget();tabs->removeTab(tabs->currentIndex());w->deleteLater();}});create();}
}
