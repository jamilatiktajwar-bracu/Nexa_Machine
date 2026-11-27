#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QLineEdit>
#include <QString>
#include <QImage>
#include <QPixmap>
#include <QMouseEvent>
#include <QComboBox>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QFile>
#include <QStringList>
#include <QTimer>
#include <QFileInfo>
#include <functional>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <memory>
#include <vector>
#include <stdexcept>
#include <filesystem>
#include "nexa/assembler.hpp"
#include "nexa/vm.hpp"
#include "nexa/config.hpp"
#include "nexa/workspace.hpp"
#include "nexa/virtual_disk.hpp"
class PixelCanvas:public QLabel {
public: std::function<void(int,int,bool)> paintPixel;
protected:
 void mousePressEvent(QMouseEvent*e)override{paint(e);}
 void mouseMoveEvent(QMouseEvent*e)override{if(e->buttons()&(Qt::LeftButton|Qt::RightButton))paint(e);}
private:
 void paint(QMouseEvent*e){double scale=std::min(width()/64.0,height()/32.0);if(scale<=0)return;double ox=(width()-64*scale)/2,oy=(height()-32*scale)/2;int x=int((e->position().x()-ox)/scale),y=int((e->position().y()-oy)/scale);if(x>=0&&x<64&&y>=0&&y<32&&paintPixel){bool erase=e->button()==Qt::RightButton||(e->buttons()&Qt::RightButton);paintPixel(x,y,!erase);}}
};
class NexaWindow:public QMainWindow {
public:
 NexaWindow(){timer.setInterval(15);connect(&timer,&QTimer::timeout,this,[this](){tick();});
  paths=nexa::WorkspacePaths::discover();paths.initialize();setWindowTitle("Nexa Machine — VM Manager");resize(1180,800);
  auto*w=new QWidget;auto*l=new QVBoxLayout(w);auto*bar=new QHBoxLayout;
  profiles=new QComboBox;profiles->setMinimumWidth(180);auto*refresh=new QPushButton("Refresh");auto*create=new QPushButton("New profile");auto*clone=new QPushButton("Clone");auto*del=new QPushButton("Delete");auto*exportProfile=new QPushButton("Export");auto*importProfile=new QPushButton("Import");auto*loadFile=new QPushButton("Open .nexa");
  auto*saveSnap=new QPushButton("Save snapshot");auto*restoreSnap=new QPushButton("Restore snapshot");auto*run=new QPushButton("Start");auto*pause=new QPushButton("Pause");auto*resume=new QPushButton("Resume");auto*stop=new QPushButton("Stop");auto*step=new QPushButton("Step");auto*reset=new QPushButton("Reset");auto*diskInfo=new QPushButton("Disk info");auto*flushDisk=new QPushButton("Flush disk");auto*importDisk=new QPushButton("Import disk");auto*exportDisk=new QPushButton("Export disk");
  auto*bp=new QLineEdit;bp->setPlaceholderText("Breakpoint address");bp->setMaximumWidth(150);auto*setBp=new QPushButton("Set breakpoint");auto*guestInput=new QLineEdit;guestInput->setPlaceholderText("Keyboard input to guest");guestInput->setMaximumWidth(170);auto*sendInput=new QPushButton("Send input");auto*exportLog=new QPushButton("Export log");status=new QLabel("Ready");
  bar->addWidget(profiles);bar->addWidget(refresh);bar->addWidget(create);bar->addWidget(clone);bar->addWidget(del);bar->addWidget(exportProfile);bar->addWidget(importProfile);bar->addWidget(loadFile);bar->addWidget(saveSnap);bar->addWidget(restoreSnap);bar->addWidget(run);bar->addWidget(pause);bar->addWidget(resume);bar->addWidget(stop);bar->addWidget(step);bar->addWidget(reset);bar->addWidget(diskInfo);bar->addWidget(flushDisk);bar->addWidget(importDisk);bar->addWidget(exportDisk);bar->addWidget(bp);bar->addWidget(setBp);bar->addWidget(guestInput);bar->addWidget(sendInput);bar->addWidget(exportLog);bar->addWidget(status);l->addLayout(bar);
  editor=new QPlainTextEdit;editor->setPlainText("LOAD A, 19\nLOAD B, 23\nADD A, B\nPRINT A\nHALT\n");l->addWidget(editor,2);
  auto*panels=new QHBoxLayout;output=new QTextEdit;output->setReadOnly(true);memory=new QTextEdit;memory->setReadOnly(true);memory->setMaximumWidth(330);graphics=new PixelCanvas;graphics->setMinimumSize(320,160);graphics->setAlignment(Qt::AlignCenter);panels->addWidget(output,2);panels->addWidget(memory,1);panels->addWidget(graphics,1);l->addLayout(panels,1);setCentralWidget(w);
  connect(refresh,&QPushButton::clicked,this,[this](){refreshProfiles();});
  connect(create,&QPushButton::clicked,this,[this](){createProfile();});
  connect(clone,&QPushButton::clicked,this,[this](){cloneProfile();});
  connect(del,&QPushButton::clicked,this,[this](){deleteProfile();});
  connect(exportProfile,&QPushButton::clicked,this,[this](){exportProfileFile();});
  connect(importProfile,&QPushButton::clicked,this,[this](){importProfileFile();});
  connect(profiles,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int){selectProfile();});
  connect(loadFile,&QPushButton::clicked,this,[this](){auto file=QFileDialog::getOpenFileName(this,"Open assembly",QString(),"Nexa Assembly (*.nexa *.asm *.txt);;All files (*)");if(!file.isEmpty()){QFile f(file);if(f.open(QIODevice::ReadOnly|QIODevice::Text))editor->setPlainText(QString::fromUtf8(f.readAll()));loaded=false;status->setText("Source loaded");}});
  connect(saveSnap,&QPushButton::clicked,this,[this](){saveSnapshot();});
  connect(restoreSnap,&QPushButton::clicked,this,[this](){restoreSnapshot();});
  connect(run,&QPushButton::clicked,this,[this](){startExecution();});
  connect(pause,&QPushButton::clicked,this,[this](){timer.stop();status->setText("Paused");});
  connect(resume,&QPushButton::clicked,this,[this](){resumeExecution();});
  connect(stop,&QPushButton::clicked,this,[this](){timer.stop();if(vm)vm->requestHalt();status->setText("Stopped");refreshMemory();});
  connect(diskInfo,&QPushButton::clicked,this,[this](){showDiskInfo();});
  connect(flushDisk,&QPushButton::clicked,this,[this](){flushDiskImage();});
  connect(importDisk,&QPushButton::clicked,this,[this](){importDiskImage();});
  connect(exportDisk,&QPushButton::clicked,this,[this](){exportDiskImage();});
  connect(step,&QPushButton::clicked,this,[this](){execute(true);});
  connect(reset,&QPushButton::clicked,this,[this](){timer.stop();vm=std::make_unique<nexa::VM>(ramBytes);loaded=false;output->clear();status->setText("Reset");refreshMemory();});
  connect(setBp,&QPushButton::clicked,this,[this,bp](){bool ok=false;auto address=bp->text().toULong(&ok);if(ok){vm->setBreakpoint(address);status->setText(QString("Breakpoint at %1").arg(address));}else status->setText("Invalid breakpoint");});\n  connect(sendInput,&QPushButton::clicked,this,[this,guestInput](){vm->queueKeyboardInput(guestInput->text().toStdString()+"\\n");guestInput->clear();status->setText("Input queued");});\n  connect(guestInput,&QLineEdit::returnPressed,this,[this,guestInput](){vm->queueKeyboardInput(guestInput->text().toStdString()+"\\n");guestInput->clear();status->setText("Input queued");});\n  connect(exportLog,&QPushButton::clicked,this,[this](){QString path=QFileDialog::getSaveFileName(this,"Export console log","nexa-console.txt","Text (*.txt)");if(path.isEmpty())return;QFile file(path);if(file.open(QIODevice::WriteOnly|QIODevice::Text)){file.write(output->toPlainText().toUtf8());status->setText("Console log exported");}else QMessageBox::warning(this,"Log export","Could not write log file.");});
  refreshProfiles();
 }
private:
 void refreshProfiles(){QString current=profiles->currentText();profiles->blockSignals(true);profiles->clear();for(const auto&name:paths.listProfiles())profiles->addItem(QString::fromStdString(name));int idx=profiles->findText(current);if(idx>=0)profiles->setCurrentIndex(idx);profiles->blockSignals(false);if(profiles->count()==0){vm=std::make_unique<nexa::VM>();ramBytes=256;activeProfile.clear();status->setText("No profile selected — default VM");refreshMemory();}else{if(profiles->currentIndex()<0)profiles->setCurrentIndex(0);selectProfile();}}
 void createProfile(){bool ok=false;QString name=QInputDialog::getText(this,"Create profile","Profile name:",QLineEdit::Normal,"Nexa",&ok);if(!ok||name.trimmed().isEmpty())return;int ram=QInputDialog::getInt(this,"Profile RAM","RAM bytes (1-65536):",256,1,65536,1,&ok);if(!ok)return;int disk=QInputDialog::getInt(this,"Profile disk","Disk bytes (1-134217728):",1048576,1,134217728,4096,&ok);if(!ok)return;try{paths.createProfile(name.trimmed().toStdString(),static_cast<std::size_t>(ram),static_cast<std::size_t>(disk));refreshProfiles();int idx=profiles->findText(name.trimmed());if(idx>=0)profiles->setCurrentIndex(idx);}catch(const std::exception&e){QMessageBox::warning(this,"Profile error",e.what());}}
 void cloneProfile(){if(activeProfile.empty())return;bool ok=false;QString name=QInputDialog::getText(this,"Clone profile","New profile name:",QLineEdit::Normal,QString::fromStdString(activeProfile)+"-copy",&ok);if(!ok||name.trimmed().isEmpty())return;try{auto cfg=nexa::MachineConfig::load((paths.profiles/(activeProfile+".json")).string());paths.createProfile(name.trimmed().toStdString(),cfg.ramBytes,cfg.diskBytes);refreshProfiles();profiles->setCurrentIndex(profiles->findText(name.trimmed()));QMessageBox::information(this,"Clone","Cloned settings. A new empty disk image is assigned.");}catch(const std::exception&e){QMessageBox::warning(this,"Clone error",e.what());}}
 void deleteProfile(){if(activeProfile.empty())return;if(QMessageBox::question(this,"Delete profile","Delete profile definition? The virtual disk is kept.")!=QMessageBox::Yes)return;try{paths.deleteProfile(activeProfile);activeProfile.clear();refreshProfiles();}catch(const std::exception&e){QMessageBox::warning(this,"Delete error",e.what());}}
 void exportProfileFile(){if(activeProfile.empty())return;QString path=QFileDialog::getSaveFileName(this,"Export profile",QString::fromStdString(activeProfile)+".json","JSON (*.json)");if(path.isEmpty())return;try{std::filesystem::copy_file(paths.profiles/(activeProfile+".json"),path.toStdString(),std::filesystem::copy_options::overwrite_existing);status->setText("Profile exported");}catch(const std::exception&e){QMessageBox::warning(this,"Export error",e.what());}}
 void importProfileFile(){QString path=QFileDialog::getOpenFileName(this,"Import profile",QString(),"JSON (*.json)");if(path.isEmpty())return;try{auto cfg=nexa::MachineConfig::load(path.toStdString());paths.createProfile(cfg.name,cfg.ramBytes,cfg.diskBytes);refreshProfiles();profiles->setCurrentIndex(profiles->findText(QString::fromStdString(cfg.name)));QMessageBox::information(this,"Import","Profile settings imported with a fresh disk image.");}catch(const std::exception&e){QMessageBox::warning(this,"Import error",e.what());}}
 void selectProfile(){timer.stop();QString name=profiles->currentText();if(name.isEmpty())return;try{auto cfg=nexa::MachineConfig::load((paths.profiles/(name.toStdString()+".json")).string());ramBytes=cfg.ramBytes;vm=std::make_unique<nexa::VM>(ramBytes);activeProfile=name.toStdString();loaded=false;output->clear();status->setText(QString("Profile %1 | RAM %2 bytes").arg(name).arg(ramBytes));refreshMemory();}catch(const std::exception&e){QMessageBox::warning(this,"Profile load error",e.what());}}
 void saveSnapshot(){if(activeProfile.empty()){QMessageBox::information(this,"Snapshot","Create or select a profile first.");return;}bool ok=false;QString name=QInputDialog::getText(this,"Save snapshot","Snapshot name:",QLineEdit::Normal,"checkpoint",&ok);if(!ok||name.trimmed().isEmpty())return;try{auto path=paths.snapshotPath(activeProfile,name.trimmed().toStdString());vm->saveState(path.string());status->setText("Snapshot saved: "+name);}catch(const std::exception&e){QMessageBox::warning(this,"Snapshot error",e.what());}}
 void restoreSnapshot(){timer.stop();if(activeProfile.empty())return;auto names=paths.listSnapshots(activeProfile);if(names.empty()){QMessageBox::information(this,"Snapshots","No snapshots found for this profile.");return;}QStringList list;for(auto&n:names)list<<QString::fromStdString(n);bool ok=false;QString name=QInputDialog::getItem(this,"Restore snapshot","Snapshot:",list,0,false,&ok);if(!ok)return;try{vm->loadState(paths.snapshotPath(activeProfile,name.toStdString()).string());loaded=true;status->setText("Snapshot restored: "+name);refreshMemory();}catch(const std::exception&e){QMessageBox::warning(this,"Restore error",e.what());}}
 void startExecution(){try{if(vm->halted()){QMessageBox::information(this,"VM","VM is halted. Reset before starting again.");return;}if(!loaded){vm->loadProgram(nexa::parseAssembly(editor->toPlainText().toStdString()));loaded=true;}timer.start(15);status->setText("Running");}catch(const std::exception&e){output->setPlainText(e.what());status->setText("Start error");}}
 void resumeExecution(){if(!loaded||vm->halted()){QMessageBox::information(this,"VM","Load or reset a program before resuming.");return;}timer.start(15);status->setText("Resumed");}
 void tick(){if(!vm||vm->halted()){timer.stop();status->setText("Halted");return;}if(vm->hasBreakpoint(vm->programCounter())){timer.stop();status->setText(QString("Breakpoint at %1").arg(vm->programCounter()));return;}try{std::ostringstream capture;auto*old=std::cout.rdbuf(capture.rdbuf());try{vm->stepInstruction();}catch(...){std::cout.rdbuf(old);throw;}std::cout.rdbuf(old);auto produced=capture.str();if(!produced.empty())output->append(QString::fromStdString(produced));refreshMemory();status->setText(QString("Running | PC=%1 A=%2 B=%3 | %4 instructions").arg(vm->programCounter()).arg(vm->accumulator()).arg(vm->secondRegister()).arg(vm->instructionsExecuted()));if(vm->halted()){timer.stop();status->setText(QString("Halted | %1 instructions").arg(vm->instructionsExecuted()));}}catch(const std::exception&e){timer.stop();output->append(QString::fromStdString(e.what()));status->setText("Execution fault");}}
 void showDiskInfo(){if(activeProfile.empty()){QMessageBox::information(this,"Disk","Select a profile to inspect its disk.");return;}try{auto cfg=nexa::MachineConfig::load((paths.profiles/(activeProfile+".json")).string());QFileInfo info(QString::fromStdString(cfg.diskPath));QMessageBox::information(this,"Virtual disk",QString("Image: %1\\nConfigured capacity: %2 bytes\\nFile exists: %3\\nFile size: %4 bytes").arg(info.absoluteFilePath()).arg(cfg.diskBytes).arg(info.exists()?"Yes":"No").arg(info.exists()?info.size():0));}catch(const std::exception&e){QMessageBox::warning(this,"Disk info",e.what());}}
 void flushDiskImage(){if(activeProfile.empty())return;try{auto cfg=nexa::MachineConfig::load((paths.profiles/(activeProfile+".json")).string());nexa::VirtualDisk disk(cfg.diskPath,cfg.diskBytes);disk.load();disk.flush();status->setText("Disk image initialized/flushed");}catch(const std::exception&e){QMessageBox::warning(this,"Disk error",e.what());}}
 void importDiskImage(){if(activeProfile.empty())return;QString source=QFileDialog::getOpenFileName(this,"Import disk image",QString(),"Disk images (*.disk *.img);;All files (*)");if(source.isEmpty())return;try{auto cfg=nexa::MachineConfig::load((paths.profiles/(activeProfile+".json")).string());QFile input(source);if(!input.open(QIODevice::ReadOnly))throw std::runtime_error("Cannot read selected image");if(input.size()>static_cast<qint64>(cfg.diskBytes))throw std::runtime_error("Image exceeds profile disk capacity");auto bytes=input.readAll();nexa::VirtualDisk disk(cfg.diskPath,cfg.diskBytes);disk.write(0,std::vector<std::uint8_t>(bytes.begin(),bytes.end()));disk.flush();status->setText("Disk image imported");}catch(const std::exception&e){QMessageBox::warning(this,"Disk import",e.what());}}
 void exportDiskImage(){if(activeProfile.empty())return;QString target=QFileDialog::getSaveFileName(this,"Export disk image",QString::fromStdString(activeProfile)+".disk","Disk images (*.disk *.img);;All files (*)");if(target.isEmpty())return;try{auto cfg=nexa::MachineConfig::load((paths.profiles/(activeProfile+".json")).string());nexa::VirtualDisk disk(cfg.diskPath,cfg.diskBytes);disk.load();disk.flush();std::filesystem::copy_file(cfg.diskPath,target.toStdString(),std::filesystem::copy_options::overwrite_existing);status->setText("Disk image exported");}catch(const std::exception&e){QMessageBox::warning(this,"Disk export",e.what());}}
 void execute(bool singleStep){try{if(!loaded){vm->loadProgram(nexa::parseAssembly(editor->toPlainText().toStdString()));loaded=true;}std::ostringstream capture;auto*old=std::cout.rdbuf(capture.rdbuf());try{if(singleStep)vm->stepInstruction();else vm->run();}catch(...){std::cout.rdbuf(old);throw;}std::cout.rdbuf(old);output->append(QString::fromStdString(capture.str()));status->setText(QString("PC=%1 A=%2 B=%3 | %4 | %5 instructions").arg(vm->programCounter()).arg(vm->accumulator()).arg(vm->secondRegister()).arg(vm->halted()?"Halted":vm->hasBreakpoint(vm->programCounter())?"Breakpoint":"Paused").arg(vm->instructionsExecuted()));refreshMemory();}catch(const std::exception&e){output->setPlainText(e.what());status->setText("Error");}}
 void refreshGraphics(){QImage img(nexa::VM::ScreenWidth,nexa::VM::ScreenHeight,QImage::Format_RGB32);for(int y=0;y<int(nexa::VM::ScreenHeight);++y)for(int x=0;x<int(nexa::VM::ScreenWidth);++x)img.setPixel(x,y,vm->pixel(x,y)?qRgb(255,255,255):qRgb(0,0,0));graphics->setPixmap(QPixmap::fromImage(img).scaled(graphics->size(),Qt::KeepAspectRatio));}
 void refreshMemory(){if(!vm)return;refreshGraphics();QString text;for(unsigned row=0;row<64&&row<ramBytes;row+=16){text+=QString("%1: ").arg(row,4,16,QChar('0'));for(unsigned col=0;col<16&&row+col<ramBytes;++col)text+=QString("%1 ").arg(vm->readMemory(row+col),2,16,QChar('0'));text+="\n";}memory->setPlainText(text.toUpper());}
 nexa::WorkspacePaths paths;QTimer timer;std::string activeProfile;std::size_t ramBytes{256};std::unique_ptr<nexa::VM>vm{std::make_unique<nexa::VM>()};QComboBox*profiles{};QPlainTextEdit*editor{};QTextEdit*output{};QTextEdit*memory{};QLabel*status{};PixelCanvas*graphics{};bool loaded=false;
};
int main(int argc,char**argv){QApplication app(argc,argv);NexaWindow w;w.show();return app.exec();}
