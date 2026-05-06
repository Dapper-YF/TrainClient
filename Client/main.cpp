#include <QApplication>
#include "MainWindow.h"
#include "Config.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("铁路货车装载状态监测系统");
    a.setApplicationDisplayName("铁路货车装载状态监测系统");
    a.setApplicationVersion("1.0.0");

    Config::instance(); // 初始化单例

    MainWindow w;
    w.show();

    return a.exec();
}
