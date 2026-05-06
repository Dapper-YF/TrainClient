#include <QDebug>
#include "ctraintcpserver.h"
#include "logger.h"
#include "cnetdatapool.h"
#include "cdataprotocol.h"

CTrainTcpServer CTrainTcpServer::s_instance;
CTrainTcpServer::CTrainTcpServer(QObject *parent)
    : QTcpServer(parent)
    //, m_nListenPort(nListenPort)
{
    connect(this, &CTrainTcpServer::sendDatasToClient, this, &CTrainTcpServer::SendData);
}

CTrainTcpServer::~CTrainTcpServer()
{
    StopServer();
}

CTrainTcpServer *CTrainTcpServer::instance()
{
    return &s_instance;
}

void CTrainTcpServer::StartServer(quint16 nListenPort)
{
    if (this->isListening())
    {
        return;
    }

    m_nListenPort = nListenPort;
    if (this->listen(QHostAddress::Any, m_nListenPort))
    {
        //开始数据缓冲处理线程
        CNetDataPool::instance()->StartDealData();

        Logger::instance()->Log("socket开启监听端口：" + QString::number(m_nListenPort));
    }
    else
    {
        Logger::instance()->Log("socket开启监听失败！");
    }
}

void CTrainTcpServer::StopServer()
{
    if (this->isListening())
    {
        this->close();
        //停止数据缓冲处理线程
        CNetDataPool::instance()->StopDealData();

        Logger::instance()->Log("socket停止监听！");
    }
}

void CTrainTcpServer::incomingConnection(qintptr socketDescriptor)
{
    CTcpClientSocket *pSocket = new CTcpClientSocket(this);
    pSocket->setSocketDescriptor(socketDescriptor);
    Logger::instance()->Log("客户端socket建立连接：" + QString::number(pSocket->socketDescriptor()));

    m_TcpClientSocketList.append(pSocket);

    //发送对时指令
    QByteArray array;
    CDataProtocol::PackTimeSynCom(array);
    SendDatasToClient(pSocket->socketDescriptor(), array);
    //pSocket->SendDatas(array);

    connect(pSocket, &CTcpClientSocket::updateserver, this, &CTrainTcpServer::slotUpdateServer);
    connect(pSocket, &CTcpClientSocket::clientDisconnected, this, &CTrainTcpServer::slotClientDisconnected);
}

void CTrainTcpServer::slotUpdateServer(QString msg)
{
    Logger::instance()->Log(msg);
}

void CTrainTcpServer::slotClientDisconnected(int descriptor)
{
    for (int i = 0; i < m_TcpClientSocketList.size(); ++i)
    {
        QTcpSocket *item = m_TcpClientSocketList.at(i);
        if (item->socketDescriptor() == descriptor)
        {
            Logger::instance()->Log("客户端socket断开连接：" + QString::number(item->socketDescriptor()));
            m_TcpClientSocketList.removeAt(i);
            break;
        }
    }
    //网络断开注销登录
    CDataProtocol::removeUser(descriptor);
    return;
}

void CTrainTcpServer::slotSendDatasToClient(int descriptor, const QByteArray& array)
{
    SendDatasToClient(descriptor, array);
}

bool CTrainTcpServer::SetClientIdentity(int descriptor, bool isClient)
{
    // for (int i = 0; i < m_TcpClientSocketList.size(); ++i)
    // {
    //     CTcpClientSocket *item = m_TcpClientSocketList.at(i);
    //     if (item->socketDescriptor() == descriptor)
    //     {
    //         item->SetClient(isClient);
    //         return true;
    //     }
    // }
    for(const auto item:m_TcpClientSocketList){
        if (item->socketDescriptor() == descriptor)
        {
            item->SetClient(isClient);
            return true;
        }
    }
    return false;
}

void CTrainTcpServer::SendDatasToClient(int descriptor, const char *pDatas, quint32 nLen)
{
    /*if (nullptr == pDatas || nLen <= 0)
    {
        return false;
    }

    for (int i = 0; i < m_TcpClientSocketList.size(); ++i)
    {
        CTcpClientSocket *item = m_TcpClientSocketList.at(i);
        if (item->socketDescriptor() == descriptor)
        {
            return item->SendDatas(pDatas, nLen);
        }
    }

    return false;*/
    QByteArray array(pDatas, nLen);
    SendDatasToClient(descriptor, array);
}

void CTrainTcpServer::SendDatasToClient(int descriptor, const QByteArray &array)
{
    /*if (array.size() <= 0)
    {
        return false;
    }

    for (int i = 0; i < m_TcpClientSocketList.size(); ++i)
    {
        CTcpClientSocket *item = m_TcpClientSocketList.at(i);
        if (item->socketDescriptor() == descriptor)
        {
            return item->SendDatas(array);
        }
    }

    return false;*/
    emit sendDatasToClient(descriptor, array);
}
void CTrainTcpServer::sendImage(int descriptor, const QString &imagePath)
{
    //查找客户端SOCKET
    CTcpClientSocket * client=nullptr;
    for(const auto item:m_TcpClientSocketList){
        if (item->socketDescriptor() == descriptor)
        {
            client=item;
            break;
        }
    }
    if(client==nullptr){
        LOG(QString("客户端ID[%1]不存在").arg(descriptor),LogLevel::LOG_ERROR);
        return;
    }
    QFile file(imagePath);
    if (!file.exists()) {
        LOG("文件不存在: " + imagePath);

        // 发送错误响应
        QByteArray response;
        QDataStream stream(&response, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::BigEndian);

        // 文件大小设为0表示错误
        stream << (qint32)0;
        client->write(response);
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        LOG("无法打开文件: " + imagePath);

        // 发送错误响应
        QByteArray response;
        QDataStream stream(&response, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::BigEndian);

        // 文件大小设为0表示错误
        stream << (qint32)0;
        client->write(response);
        return;
    }

    // 读取文件内容
    QByteArray imageData = file.readAll();
    file.close();

    // 准备响应数据
    QByteArray response;
    QDataStream stream(&response, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    // 先发送文件大小
    stream << (qint32)imageData.size();

    // 再发送文件内容
    response.append(imageData);

    // 发送响应
    client->write(response);
    LOG("已发送图片: " + imagePath + " (" +
               QString::number(imageData.size()) + " 字节)");
}
bool CTrainTcpServer::SendData(int descriptor, const QByteArray &array)
{
    if (array.size() <= 0)
    {
        return false;
    }

    // for (int i = 0; i < m_TcpClientSocketList.size(); ++i)
    // {
    //     CTcpClientSocket *item = m_TcpClientSocketList.at(i);
    //     if (item->socketDescriptor() == descriptor)
    //     {
    //         return item->SendDatas(array);
    //     }
    // }
    //使用新的循环语法，提高程序运行效率
    for(const auto item:m_TcpClientSocketList){
        if (item->socketDescriptor() == descriptor)
        {
            return item->SendDatas(array);
        }
    }
    return false;
}
