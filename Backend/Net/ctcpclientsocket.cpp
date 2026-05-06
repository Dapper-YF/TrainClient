#include "ctcpclientsocket.h"
#include <cstring>
#include "logger.h"
#include "cnetdatapool.h"

CTcpClientSocket::CTcpClientSocket(QObject *parent)
    : QTcpSocket(parent)
{
    connect(this, &CTcpClientSocket::readyRead, this, &CTcpClientSocket::ReceiveData);
    connect(this, &CTcpClientSocket::disconnected, this, &CTcpClientSocket::slotClientDisconnected);
    this->setSocketOption(SocketOption::SendBufferSizeSocketOption,100*1024*1024);
    this->setSocketOption(SocketOption::ReceiveBufferSizeSocketOption,100*1024*1024);
}

QByteArray *pFrameArray = new QByteArray();
void CTcpClientSocket::ReceiveData()
{
    //接收数据，如果正常接收到一帧数据，在传给CNetDataPool处理后，不要删除指针
    int nFlag = 0;
    while (true)
    {
        QByteArray arrayRec;
        arrayRec = this->readAll();
        if (arrayRec.size() <= 0)
        {
            LOG("收到0字节的消息",LOG_WARNING);
            break;
        }
        LOG(QString("Receive datas len: %1").arg(pFrameArray->size()));
        pFrameArray->append(arrayRec);
        //pArray->append(m_pStationTcpSocket->readAll());
        if ((quint32)pFrameArray->size() < sizeof(quint32))
        {
            if (nFlag >= 10)//防止意外情况下的死循环
            {
                //delete pArray;
                break;
            }
            else
            {
                nFlag++;
                continue;
            }
        }

        quint32 nDataLen;
        memcpy(&nDataLen, pFrameArray->data(), 4);
        qDebug() << "Packet len:" << QString::number(nDataLen);
        if ((quint32)pFrameArray->size() < nDataLen + 4)
        {
            continue;
            /*if (nFlag >= 10)//防止意外情况下的死循环
            {
                delete pArray;
                break;
            }
            else
            {
                nFlag++;
                continue;
            }*/
        }

        while (pFrameArray->size() > 0)
        {
            QByteArray *array = new QByteArray(pFrameArray->data(), nDataLen + 4);

            CNetDataPool::instance()->AddDataFrame(this->socketDescriptor(), array);
            pFrameArray->remove(0, nDataLen + 4);

            if (pFrameArray->size() < 4)
            {
                break;
            }

            memcpy(&nDataLen, pFrameArray->data(), 4);
            if ((quint32)pFrameArray->size() < nDataLen + 4)
            {
                break;
            }
        }
        continue;
    }
}

void CTcpClientSocket::slotClientDisconnected()
{
    emit clientDisconnected(this->socketDescriptor());
}

void CTcpClientSocket::SetClient(bool bIsClient)
{
    LOG("客户端设置身份：" + QString::number(this->socketDescriptor()));
    m_bIsClient = bIsClient;
}

bool CTcpClientSocket::SendDatas(const char *pDatas, quint32 nLen)
{
    if (nullptr == pDatas || nLen <= 0)
    {
        return false;
    }

    QByteArray array(pDatas, nLen);
    return SendDatas(array);
}

bool CTcpClientSocket::SendDatas(const QByteArray& array)
{
    if (array.size() <= 0)
    {
        return false;
    }
    // QByteArray lenArray;
    // quint64 len=array.size();
    // lenArray.append((char*)&len,8);
    // this->write(lenArray);
    // while(!flush()){
    //     QThread::sleep(20);
    // }
    // QThread::sleep(10);
    qint64 bytesWrite = this->write(array);
    if (-1 == bytesWrite)
    {
        LOG("发送数据失败！");
        return false;
    }
    return this->flush();
}
