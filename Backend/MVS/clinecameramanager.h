#ifndef CLINECAMERAMANAGER_H
#define CLINECAMERAMANAGER_H

#include <QObject>
#include <QMap>
#include <QSharedPointer>
#include "clinecameraitem.h"
#include <QImage>
#include "../include/define.h"
//线阵相机管理类
class CLineCameraManager : public QObject
{
    Q_OBJECT
public:
    explicit CLineCameraManager(QObject *parent = nullptr);
    virtual ~CLineCameraManager();

    //开始或停止相机采集
    void StartCamera(QString name);
    void StartCameras();
    void StopCameras();
    QMap<QString,QString> stichImages(quint64 startTime,quint64 endTime);
    QSharedPointer<CLineCameraItem> GetItem(QString name){
        return m_lineCameraItems[name];
    }
    void CloseCamera(QString name){
        m_lineCameraItems[name]->Close();
    }
    //枚举并打开相应的相机
    int EnumDevices();
    void OpenDevice(MV_CC_DEVICE_INFO *pDeviceInfo);
    void OpenDevice(QString name);
    //关闭所有相机
    void ClearDevices();

    static CLineCameraManager *instance();
    static QString GetDeviceName(MV_CC_DEVICE_INFO *pDeviceInfo);
    void ConfigCameraParams(QString name,LineCameraParameter param);
    LineCameraParameter GetCameraPrams(QString name);
    bool OpenCamera(QString name);
    QImage GetLatestImage(QString name);
    TrainInformation m_currentTrain;
    //线阵相机操作对象列表<camera ip, CLineCameraItem>
    QMap<QString, QSharedPointer<CLineCameraItem>> m_lineCameraItems;
    QString FindCameraItemPos(CLineCameraItem* item);
signals:

private:

    //ip地址转换
    QString LongToIP4String(unsigned int ip);
    unsigned int IP4StringToLong(const QString& ipAddress);



    static CLineCameraManager s_instance;
    bool inited{false};
};

#endif // CLINECAMERAMANAGER_H
