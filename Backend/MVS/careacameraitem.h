#ifndef CAREACAMERAITEM_H
#define CAREACAMERAITEM_H
#include <QThread>
#include <QMutex>
#include <QByteArray>
#include "MvCamera.h"          // 你的 SDK 封装
class CAreaCameraItem:public QThread
{
    Q_OBJECT
public:
    explicit CAreaCameraItem(CMvCamera *camera, QObject *parent = nullptr);
    void stop();                 // 供外部调用，安全退出
    void setGrabbing(bool on);   // 开始/暂停采集

signals:
    void oneFrameReady(const QByteArray &imgBuf,
                       const MV_FRAME_OUT_INFO_EX &info);

protected:
    void run() override;

private:
    CMvCamera            *m_camera;
    std::atomic<bool>    m_grabbing {false};
    QByteArray           m_saveBuf;   // 缓存
    QMutex               m_bufMutex;  // 保护 m_saveBuf
};

#endif // CAREACAMERAITEM_H
