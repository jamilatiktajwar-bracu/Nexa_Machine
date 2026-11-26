#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QString>
#include <sstream>
#include <iostream>
#include "nexa/assembler.hpp"
#include "nexa/vm.hpp"
class NexaWindow:public QMainWindow {
public:
 NexaWindow(){
  setWindowTitle("Nexa Machine 0.4");resize(900,650);
  auto*w=new QWidget;auto*l=new QVBoxLayout(w);auto*bar=new QHBoxLayout;
  auto*run=new QPushButton("Run");auto*reset=new QPushButton("Reset example");status=new QLabel("Ready");
  bar->addWidget(run);bar->addWidget(reset);bar->addWidget(status);l->addLayout(bar);
  editor=new QPlainTextEdit;editor->setPlainText("LOAD A, 19\nLOAD B, 23\nADD A, B\nPRINT A\nHALT\n");l->addWidget(editor,2);
  output=new QTextEdit;output->setReadOnly(true);l->addWidget(output,1);setCentralWidget(w);
  connect(run,&QPushButton::clicked,this,[this](){try{std::ostringstream capture;auto*old=std::cout.rdbuf(capture.rdbuf());nexa::VM vm;try{vm.loadProgram(nexa::parseAssembly(editor->toPlainText().toStdString()));vm.run();}catch(...){std::cout.rdbuf(old);throw;}std::cout.rdbuf(old);output->setPlainText(QString::fromStdString(capture.str())+QString("\nHalted. Instructions: %1").arg(vm.instructionsExecuted()));status->setText("Halted");}catch(const std::exception&e){output->setPlainText(e.what());status->setText("Error");}});
  connect(reset,&QPushButton::clicked,this,[this](){editor->setPlainText("LOAD A, 19\nLOAD B, 23\nADD A, B\nPRINT A\nHALT\n");output->clear();status->setText("Ready");});
 }
private:QPlainTextEdit*editor{};QTextEdit*output{};QLabel*status{};
};
int main(int argc,char**argv){QApplication app(argc,argv);NexaWindow w;w.show();return app.exec();}
