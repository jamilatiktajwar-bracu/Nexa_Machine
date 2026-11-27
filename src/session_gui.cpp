#include "nexa/session_gui.hpp"
#include "nexa/session_manager.hpp"
#include "nexa/virtual_disk.hpp"
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QPushButton>
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
namespace nexa {
namespace {
struct DiskEntry { std::unique_ptr<VirtualDisk> disk; QString path; };
class SessionPage final : public QWidget {
public:
 explicit SessionPage(QString name,QWidget* parent=nullptr):QWidget(parent),name_(std::move(name)),id_(manager_.create(name_.toStdString(),1024)) {
  auto* root=new QVBoxLayout(this); auto* controls=new QHBoxLayout;
  auto* start=new QPushButton("Start");auto* pause=new QPushButton("Pause");auto* resume=new QPushButton("Resume");auto* step=new QPushButton("Step");auto* reset=new QPushButton("Reset");
  auto* attach=new QPushButton("Attach disk");auto* detach=new QPushButton("Detach disk");auto* flush=new QPushButton("Flush disk");auto* statusDisk=new QPushButton("Device status");
  auto* save=new QPushButton("Save VM + disk snapshot");auto* restore=new QPushButton("Restore VM + disk snapshot");
  for(auto*b:{start,pause,resume,step,reset,attach,detach,flush,statusDisk,save,restore})controls->addWidget(b);
  root->addLayout(controls);
  auto* split=new QHBoxLayout; editor_=new QPlainTextEdit;editor_->setPlainText("LOAD A, 19\nLOAD B, 23\nADD A, B\nPRINT A\nHALT\n");
  output_=new QTextEdit;output_->setReadOnly(true);memory_=new QTextEdit;memory_->setReadOnly(true);memory_->setMaximumWidth(260);
  auto* right=new QVBoxLayout;right->addWidget(new QLabel("Console"));right->addWidget(output_);right->addWidget(new QLabel("RAM preview"));right->addWidget(memory_);
  split->addWidget(editor_,2);split->addLayout(right,1);root->addLayout(split,1);
  diskTable_=new QTableWidget(0,4);diskTable_->setHorizontalHeaderLabels({"Device","Image","Capacity","State"});diskTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);diskTable_->setMaximumHeight(150);root->addWidget(diskTable_);
  status_=new QLabel("Ready");root->addWidget(status_);
  timer_.setInterval(15);connect(&timer_,&QTimer::timeout,this,[this]{tick();});
  connect(start,&QPushButton::clicked,this,[this]{try{if(!loaded_){manager_.loadAssembly(id_,editor_->toPlainText().toStdString());loaded_=true;}manager_.start(id_);timer_.start();status_->setText("Running");}catch(const std::exception&e){error(e.what());}});
  connect(pause,&QPushButton::clicked,this,[this]{try{timer_.stop();manager_.pause(id_);status_->setText("Paused");}catch(const std::exception&e){error(e.what());}});
  connect(resume,&QPushButton::clicked,this,[this]{try{manager_.resume(id_);timer_.start();status_->setText("Resumed");}catch(const std::exception&e){error(e.what());}});
  connect(step,&QPushButton::clicked,this,[this]{try{if(!loaded_){manager_.loadAssembly(id_,editor_->toPlainText().toStdString());loaded_=true;}manager_.step(id_);refresh();}catch(const std::exception&e){error(e.what());}});
  connect(reset,&QPushButton::clicked,this,[this]{try{timer_.stop();manager_.reset(id_);loaded_=true;output_->clear();refresh();status_->setText("Reset");}catch(const std::exception&e){error(e.what());}});
  connect(attach,&QPushButton::clicked,this,[this]{attachDisk();});connect(detach,&QPushButton::clicked,this,[this]{detachDisk();});connect(flush,&QPushButton::clicked,this,[this]{enqueue([this]{for(auto&[id,d]:disks_)d.disk->flush();status_->setText("All attached disks flushed");});});
  connect(statusDisk,&QPushButton::clicked,this,[this]{refreshDisks();QMessageBox::information(this,"Device status",QString("%1 disk device(s) attached. Queued I/O: %2").arg(disks_.size()).arg(ioQueue_.size()));});
  connect(save,&QPushButton::clicked,this,[this]{snapshot(true);});connect(restore,&QPushButton::clicked,this,[this]{snapshot(false);});
 }
 ~SessionPage() override {timer_.stop();try{for(auto&[id,d]:disks_)d.disk->flush();}catch(...){}}
private:
 void error(const std::string&s){status_->setText("Error");QMessageBox::warning(this,"Nexa session",QString::fromStdString(s));}
 void enqueue(std::function<void()> task){ioQueue_.push_back(std::move(task));if(!timer_.isActive())timer_.start();}
 void attachDisk(){QString path=QFileDialog::getSaveFileName(this,"Attach or create disk image",QString(),"Disk images (*.disk *.img);;All files (*)");if(path.isEmpty())return;bool ok=false;int mib=QInputDialog::getInt(this,"Disk capacity","Capacity (MiB):",16,1,128,1,&ok);if(!ok)return;uint32_t device=0;while(disks_.contains(device))++device;const auto capacity=static_cast<std::size_t>(mib)*1024*1024;enqueue([this,path,capacity,device]{auto disk=std::make_unique<VirtualDisk>(path.toStdString(),capacity);disk->load();disk->flush();disks_.emplace(device,DiskEntry{std::move(disk),path});refreshDisks();status_->setText(QString("Device %1 attached").arg(device));});}
 void detachDisk(){if(disks_.empty())return;QStringList options;for(const auto&[id,d]:disks_)options<<QString::number(id)+" — "+d.path;bool ok=false;QString choice=QInputDialog::getItem(this,"Detach disk","Device:",options,0,false,&ok);if(!ok)return;bool flushFirst=QMessageBox::question(this,"Detach disk","Flush pending image changes before detaching?")==QMessageBox::Yes;uint32_t id=choice.section(' ',0,0).toUInt();enqueue([this,id,flushFirst]{auto it=disks_.find(id);if(it==disks_.end())return;if(flushFirst)it->second.disk->flush();disks_.erase(it);refreshDisks();status_->setText(QString("Device %1 detached").arg(id));});}
 void refreshDisks(){diskTable_->setRowCount(static_cast<int>(disks_.size()));int row=0;for(const auto&[id,d]:disks_){diskTable_->setItem(row,0,new QTableWidgetItem(QString::number(id)));diskTable_->setItem(row,1,new QTableWidgetItem(QFileInfo(d.path).fileName()));diskTable_->setItem(row,2,new QTableWidgetItem(QString::number(d.disk->size())+" bytes"));diskTable_->setItem(row,3,new QTableWidgetItem("Attached"));++row;}}
 void snapshot(bool save){QString base=QFileDialog::getExistingDirectory(this,save?"Snapshot destination":"Snapshot directory");if(base.isEmpty())return;QString safeName=name_;safeName.replace('/','_');const QString stem=QDir(base).filePath(safeName);enqueue([this,stem,save]{try{if(save){for(const auto&[id,d]:disks_)d.disk->flush();manager_.saveSnapshot(id_,(stem+".vm").toStdString());QDir().mkpath(stem);for(const auto&[id,d]:disks_)std::filesystem::copy_file(d.path.toStdString(),(stem+"/device-"+QString::number(id)+".disk").toStdString(),std::filesystem::copy_options::overwrite_existing);}else{manager_.loadSnapshot(id_,(stem+".vm").toStdString());for(auto&[id,d]:disks_){auto src=(stem+"/device-"+QString::number(id)+".disk").toStdString();if(!std::filesystem::exists(src))throw std::runtime_error("Snapshot is missing disk device "+std::to_string(id));std::filesystem::copy_file(src,d.path.toStdString(),std::filesystem::copy_options::overwrite_existing);d.disk->load();}loaded_=true;refresh();}status_->setText(save?"VM + disk snapshot saved":"VM + disk snapshot restored");}catch(const std::exception&e){error(e.what());}});}
 void tick(){try{if(!ioQueue_.empty()){auto task=std::move(ioQueue_.front());ioQueue_.pop_front();task();}auto info=manager_.info(id_);if(info.state==SessionState::Running){manager_.step(id_);refresh();info=manager_.info(id_);if(info.state==SessionState::Halted){timer_.stop();status_->setText("Halted");}}if(ioQueue_.empty()&&info.state!=SessionState::Running)timer_.stop();}catch(const std::exception&e){timer_.stop();error(e.what());}}
 void refresh(){const auto&vm=manager_.machine(id_);QString text;for(std::size_t a=0;a<std::min<std::size_t>(128,vm.ramSize());a+=16){text+=QString("%1: ").arg(a,4,16,QChar('0'));for(std::size_t j=0;j<16&&a+j<vm.ramSize();++j)text+=QString("%1 ").arg(vm.readMemory(static_cast<uint16_t>(a+j)),2,16,QChar('0'));text+='\n';}memory_->setPlainText(text.toUpper());status_->setText(QString("PC=%1  A=%2  B=%3  Instructions=%4").arg(vm.programCounter()).arg(vm.accumulator()).arg(vm.secondRegister()).arg(vm.instructionsExecuted()));}
 SessionManager manager_;std::string id_;QString name_;QTimer timer_;QPlainTextEdit*editor_{};QTextEdit*output_{};QTextEdit*memory_{};QLabel*status_{};QTableWidget*diskTable_{};std::map<uint32_t,DiskEntry>disks_;std::deque<std::function<void()>>ioQueue_;bool loaded_{false};
};
}
SessionWorkspace::SessionWorkspace(QWidget*parent):QWidget(parent){auto*layout=new QVBoxLayout(this);auto*bar=new QHBoxLayout;auto*tabs=new QTabWidget;auto*add=new QPushButton("New VM session");auto*remove=new QPushButton("Close session");bar->addWidget(add);bar->addWidget(remove);bar->addStretch();layout->addLayout(bar);layout->addWidget(tabs);auto create=[tabs]{auto n=tabs->count()+1;tabs->addTab(new SessionPage(QString("VM %1").arg(n)),QString("VM %1").arg(n));tabs->setCurrentIndex(tabs->count()-1);};connect(add,&QPushButton::clicked,this,create);connect(remove,&QPushButton::clicked,this,[tabs]{if(tabs->count()){auto*w=tabs->currentWidget();tabs->removeTab(tabs->currentIndex());w->deleteLater();}});create();}
}
