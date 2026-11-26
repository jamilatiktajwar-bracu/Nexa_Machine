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
#include <sstream>
#include <iostream>
#include "nexa/assembler.hpp"
#include "nexa/vm.hpp"
class NexaWindow:public QMainWindow {
public:
 NexaWindow(){
  setWindowTitle("Nexa Machine 0.7");resize(1050,760);
  auto*w=new QWidget;auto*l=new QVBoxLayout(w);auto*bar=new QHBoxLayout;
  auto*run=new QPushButton("Run");auto*step=new QPushButton("Step");auto*reset=new QPushButton("Reset");
  auto*bp=new QLineEdit;bp->setPlaceholderText("Breakpoint instruction address");auto*setBp=new QPushButton("Set breakpoint");status=new QLabel("Ready");
  bar->addWidget(run);bar->addWidget(step);bar->addWidget(reset);bar->addWidget(bp);bar->addWidget(setBp);bar->addWidget(status);l->addLayout(bar);
  editor=new QPlainTextEdit;editor->setPlainText("LOAD A, 19\nLOAD B, 23\nADD A, B\nPRINT A\nHALT\n");l->addWidget(editor,2);
  auto*panels=new QHBoxLayout;output=new QTextEdit;output->setReadOnly(true);memory=new QTextEdit;memory->setReadOnly(true);memory->setMaximumWidth(330);graphics=new QLabel;graphics->setMinimumSize(320,160);graphics->setAlignment(Qt::AlignCenter);panels->addWidget(output,2);panels->addWidget(memory,1);panels->addWidget(graphics,1);l->addLayout(panels,1);setCentralWidget(w);
  connect(run,&QPushButton::clicked,this,[this](){execute(false);});
  connect(step,&QPushButton::clicked,this,[this](){execute(true);});
  connect(reset,&QPushButton::clicked,this,[this](){vm.reset();loaded=false;editor->setPlainText("LOAD A, 19\nLOAD B, 23\nADD A, B\nPRINT A\nHALT\n");output->clear();status->setText("Ready");refreshMemory();});
  connect(setBp,&QPushButton::clicked,this,[this,bp](){bool ok=false;auto address=bp->text().toULong(&ok);if(ok){vm.setBreakpoint(address);status->setText(QString("Breakpoint at %1").arg(address));}else status->setText("Invalid breakpoint address");});
 }
private:
 void execute(bool singleStep){try{if(!loaded){vm.loadProgram(nexa::parseAssembly(editor->toPlainText().toStdString()));loaded=true;}std::ostringstream capture;auto*old=std::cout.rdbuf(capture.rdbuf());try{if(singleStep)vm.stepInstruction();else vm.run();}catch(...){std::cout.rdbuf(old);throw;}std::cout.rdbuf(old);output->append(QString::fromStdString(capture.str()));status->setText(QString("PC=%1 A=%2 B=%3 | %4 | %5 instructions").arg(vm.programCounter()).arg(vm.accumulator()).arg(vm.secondRegister()).arg(vm.halted()?"Halted":vm.hasBreakpoint(vm.programCounter())?"Breakpoint":"Paused").arg(vm.instructionsExecuted()));refreshMemory();}catch(const std::exception&e){output->setPlainText(e.what());status->setText("Error");}}
 void refreshMemory(){QImage img(nexa::VM::ScreenWidth,nexa::VM::ScreenHeight,QImage::Format_RGB32);for(int y=0;y<int(nexa::VM::ScreenHeight);++y)for(int x=0;x<int(nexa::VM::ScreenWidth);++x)img.setPixel(x,y,vm.pixel(x,y)?qRgb(255,255,255):qRgb(0,0,0));graphics->setPixmap(QPixmap::fromImage(img).scaled(graphics->size(),Qt::KeepAspectRatio));QString text;for(unsigned row=0;row<64;row+=16){text+=QString("%1: ").arg(row,4,16,QChar('0'));for(unsigned col=0;col<16;++col)text+=QString("%1 ").arg(vm.readMemory(row+col),2,16,QChar('0'));text+="\n";}memory->setPlainText(text.toUpper());}
 QPlainTextEdit*editor{};QTextEdit*output{};QTextEdit*memory{};QLabel*status{};QLabel*graphics{};nexa::VM vm;bool loaded=false;
};
int main(int argc,char**argv){QApplication app(argc,argv);NexaWindow w;w.show();return app.exec();}
