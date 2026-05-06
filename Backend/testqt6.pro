QT       += core gui
QT       += network
QT       += sql
QT       += httpserver
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++20

SOURCES += \
    MVS/MvCamera.cpp \
    MVS/careacameraitem.cpp \
    MVS/cconfigdialog.cpp \
    MVS/cimagestitching.cpp \
    MVS/clinecameraitem.cpp \
    MVS/clinecameramanager.cpp \
    MVS/cmvscamera.cpp \
    MVS/cpreviewdialog.cpp \
    Net/cdataprotocol.cpp \
    Net/clienthandler.cpp \
    Net/cnetdatapool.cpp \
    Net/ctcpclientsocket.cpp \
    Net/ctraintcpserver.cpp \
    Net/imageserver.cpp \
    Net/webserver.cpp \
    Utils/imageutil.cpp \
    Utils/stringutil.cpp \
    cdatabasemanager.cpp \
    common/protocol.cpp \
    csystemconfig.cpp \
    imageuploader.cpp \
    logger.cpp \
    main.cpp \
    mainwindow.cpp \
    singleton.cpp

HEADERS += \
    E:/Study/Qtcreator/screw/include/define.h \
    MVS/MvCamera.h \
    MVS/careacameraitem.h \
    MVS/cconfigdialog.h \
    MVS/cimagestitching.h \
    MVS/clinecameraitem.h \
    MVS/clinecameramanager.h \
    MVS/cmvscamera.h \
    MVS/cpreviewdialog.h \
    Net/cdataprotocol.h \
    Net/clienthandler.h \
    Net/cnetdatapool.h \
    Net/ctcpclientsocket.h \
    Net/ctraintcpserver.h \
    Net/imageserver.h \
    Net/webserver.h \
    Utils/imageutil.h \
    Utils/stringutil.h \
    cdatabasemanager.h \
    common/constants.h \
    common/protocol.h \
    csystemconfig.h \
    imageuploader.h \
    logger.h \
    mainwindow.h \
    singleton.h

FORMS += \
    MVS/cconfigdialog.ui \
    MVS/cpreviewdialog.ui \
    mainwindow.ui

# MVS SDK
INCLUDEPATH += E:/Study/Qtcreator/screw/MVS/Development/Includes
INCLUDEPATH += E:/Study/Qtcreator/screw/include
DEPENDPATH += E:/Study/Qtcreator/screw/MVS/Development/Includes
LIBS += -LE:/Study/Qtcreator/screw/MVS/Development/Libraries/win64 -lMvCameraControl
PRE_TARGETDEPS += E:/Study/Qtcreator/screw/MVS/Development/Libraries/win64/MvCameraControl.lib

# OpenCV 4.12 (MinGW编译版 - install目录)
INCLUDEPATH += D:/opencv-4.12.0-build/install/include
LIBS += -LD:/opencv-4.12.0-build/install/x64/mingw/lib \
    -lopencv_stitching4120 \
    -lopencv_features2d4120 \
    -lopencv_calib3d4120 \
    -lopencv_flann4120 \
    -lopencv_highgui4120 \
    -lopencv_core4120 \
    -lopencv_imgproc4120 \
    -lopencv_imgcodecs4120 \
    -lopencv_videoio4120
