#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow) {
    ui->setupUi(this);
}

MainWindow::~MainWindow() {
    delete ui;
}


void MainWindow::on_audioButton_clicked() {
    _audioThread = new AudioThread(this);
    // 执行这句就会调用 AudioThread 中的 run 方法
    _audioThread->start();
}

