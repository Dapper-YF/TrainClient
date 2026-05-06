#ifndef CNETDATAPOOL_H
#define CNETDATAPOOL_H

#include <QList>
#include <memory>
#include <QMutex>
#include <QThread>

class CNetDataPool : QThread
{
public:
    CNetDataPool();
    virtual ~CNetDataPool();

    //开始及停止线程
    void StartDealData();
    void StopDealData();

    //向数据缓存池中添加数据帧
    void AddDataFrame(int descriptor, char *pFrame, quint32 nLen);
    void AddDataFrame(int descriptor, QByteArray *pArray);

    static CNetDataPool *instance();

protected:
    void run() override;

private:
    //接收数据缓冲池,每一个QByteArray是一个完整的数据帧
    //QList<std::shared_ptr<QByteArray>> m_dataList;
    QList<QPair<int, std::shared_ptr<QByteArray>>> m_dataList;

    QMutex m_mutexDataList;

    static CNetDataPool s_instance;
};

#endif // CNETDATAPOOL_H
