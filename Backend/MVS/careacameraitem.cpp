#include "careacameraitem.h"
#include <cstring>

CAreaCameraItem::CAreaCameraItem(CMvCamera *camera, QObject *parent)
    : QThread(parent), m_camera(camera)
{
}

void CAreaCameraItem::stop()
{
    requestInterruption();
    wait();
}

void CAreaCameraItem::setGrabbing(bool on)
{
    m_grabbing.store(on);
}

void CAreaCameraItem::run()
{
    MV_FRAME_OUT stImageInfo = {};
    int nRet = MV_OK;

    while (!isInterruptionRequested())
    {
        if (!m_grabbing.load())
        {
            QThread::msleep(10);
            continue;
        }

        stImageInfo = {};   // 每次清零
        nRet = m_camera->GetImageBuffer(&stImageInfo, 1000);
        if (nRet == MV_OK)
        {
            // 准备缓存
            {
                QMutexLocker locker(&m_bufMutex);

                quint32 frameLen = stImageInfo.stFrameInfo.nFrameLenEx;
                if (m_saveBuf.size() < static_cast<int>(frameLen))
                    m_saveBuf.resize(static_cast<int>(frameLen));

                memcpy(m_saveBuf.data(), stImageInfo.pBufAddr, frameLen);
            }

            // 向外发信号（Qt::QueuedConnection 自动跨线程）
            emit oneFrameReady(m_saveBuf, stImageInfo.stFrameInfo);

            m_camera->FreeImageBuffer(&stImageInfo);
        }
    }
}
