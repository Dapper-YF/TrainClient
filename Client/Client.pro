QT += core gui widgets network multimedia multimediawidgets axcontainer

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

TARGET = TrainClient
TEMPLATE = app

# Plan A: exe 输出到源码 Client 目录，这样运行时相对路径 ..\mvsimages 可用
# 源码在 $$_PRO_FILE_PWD_，构建在 $$OUT_PWD
DESTDIR = $$_PRO_FILE_PWD_

DEFINES += QT_DEPRECATED_WARNINGS MVSIMAGES_PATH=..\\mvsimages

SOURCES += \
    main.cpp \
    MainWindow.cpp \
    pages/LoginPage.cpp \
    pages/HomePage.cpp \
    pages/CompareDialog.cpp \
    dialogs/ProfileDialog.cpp \
    windows/InspectionWindow.cpp \
    windows/VideoPlaybackDialog.cpp \
    network/ApiClient.cpp \
    network/TrainApi.cpp \
    network/ImagePathMapper.cpp \
    Config.cpp

HEADERS += \
    MainWindow.h \
    pages/LoginPage.h \
    pages/HomePage.h \
    pages/CompareDialog.h \
    dialogs/ProfileDialog.h \
    windows/InspectionWindow.h \
    windows/VideoPlaybackDialog.h \
    network/ApiClient.h \
    network/TrainApi.h \
    network/ImagePathMapper.h \
    Config.h

INCLUDEPATH += $$PWD/windows

# OpenCV (如果后续需要图片处理)
# INCLUDEPATH += D:/opencv-4.12.0-build/install/include
# LIBS += -LD:/opencv-4.12.0-build/install/x64/mingw/lib \
#     -lopencv_core4120 -lopencv_imgproc4120 -lopencv_imgcodecs4120
