#include "mainwindow.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include "csystemconfig.h"
#include "net/webserver.h"
#include "logger.h"
#include <QHttpServer>
#include <QFile>
#include <QHttpServerResponse>
#include <QMimeDatabase>
#include "net/imageserver.h"

// Windows entry point - bypass Qt entry point wrapper
#ifdef _WIN32
#include <windows.h>

int realMain(int argc, char *argv[]);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    return realMain(__argc, __argv);
}
#endif

int realMain(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
    QApplication a(argc, argv);

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "testqt6_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }
    Logger::instance()->init("", LOG_DEBUG, true);
    CSystemConfig::instance()->Read();
    WebServer server;
    server.start(CSystemConfig::instance()->m_nWebServerPort);
    ImageServer image_server;
    image_server.setImageDirectory(CSystemConfig::instance()->m_strTrainImagesSavePath);

    if (!image_server.startServer(CSystemConfig::instance()->m_nListenPort+1)) {
        LOG("图像服务启动失败");
        return -1;
    }
    MainWindow w;
    w.show();
    return a.exec();
}
