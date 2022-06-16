#include "mainwindow.h"

#include <QApplication>

extern "C" {
// 引入设备头文件
#include <libavdevice/avdevice.h>
}

int main(int argc, char *argv[])
{
    // 1.初始化libavdevice并注册所有输入和输出设备
    avdevice_register_all();

    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
