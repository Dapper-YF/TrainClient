#include "cnetdatapool.h"
#include "cdataprotocol.h"

CNetDataPool CNetDataPool::s_instance;
CNetDataPool::CNetDataPool()
{

}

CNetDataPool::~CNetDataPool()
{
    StopDealData();

    //数据锁
    QMutexLocker locker(&m_mutexDataList);
    m_dataList.clear();
}

CNetDataPool *CNetDataPool::instance()
{
    return &CNetDataPool::s_instance;
}

void CNetDataPool::StartDealData()
{
    if (!isRunning())
    {
        start();
    }
}

void CNetDataPool::StopDealData()
{
    if (isRunning())
    {
        //强制终止线程？
        //terminate();

        requestInterruption();
        wait(3000);
    }
}

void CNetDataPool::AddDataFrame(int descriptor, char *pFrame, quint32 nLen)
{
    if (nullptr == pFrame || nLen <= 0)
    {
        return;
    }

    //数据锁
    QMutexLocker locker(&m_mutexDataList);

    QByteArray *pArray = new QByteArray(pFrame, nLen);
    std::shared_ptr<QByteArray> ptr(pArray);
    //m_dataList.append(ptr);
    m_dataList.append(qMakePair(descriptor, ptr));
    return;
}

void CNetDataPool::AddDataFrame(int descriptor, QByteArray *pArray)
{
    if (nullptr == pArray)
    {
        return;
    }

    //数据锁
    QMutexLocker locker(&m_mutexDataList);

    std::shared_ptr<QByteArray> ptr(pArray);
    //m_dataList.append(ptr);
    m_dataList.append(qMakePair(descriptor, ptr));
    return;
}

void CNetDataPool::run()
{
    while (!isInterruptionRequested())
    {
        {
            //数据锁
            QMutexLocker locker(&m_mutexDataList);

            while (m_dataList.size() > 0)
            {
                //QByteArray *pArray = m_dataList.first().get();
                QByteArray *pArray = m_dataList.first().second.get();

                //处理数据
                CDataProtocol::AnalyzeFrame(m_dataList.first().first, pArray->data(), pArray->size());

                //从列表中删除
                m_dataList.erase(m_dataList.begin());
            }
        }

        QThread::msleep(1);
    }
}
