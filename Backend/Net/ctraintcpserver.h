#ifndef CTRAINTCPSERVER_H
#define CTRAINTCPSERVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include "ctcpclientsocket.h"

class CTrainTcpServer : public QTcpServer
{
    Q_OBJECT
public:
    CTrainTcpServer(QObject *parent = nullptr);
    virtual ~CTrainTcpServer();

    void StartServer(quint16 nListenPort);
    void StopServer();

    //设置客户端身份
    bool SetClientIdentity(int descriptor, bool isClient);

    //发送数据
    void SendDatasToClient(int descriptor, const char *pDatas, quint32 nLen);
    void SendDatasToClient(int descriptor, const QByteArray& array);
    //发送图片文件
    void sendImage(int descriptor, const QString &imagePath);

    static CTrainTcpServer *instance();

protected:
    virtual void incomingConnection(qintptr socketDescriptor);

signals:
    void sendDatasToClient(int descriptor, const QByteArray& array);
private slots:
    void slotUpdateServer(QString msg);
    void slotClientDisconnected(int);
    void slotSendDatasToClient(int descriptor, const QByteArray& array);

private:
    bool SendData(int descriptor, const QByteArray& array);

    //监听端口
    quint16 m_nListenPort{8989};

    //客户端socket列表
    QList<CTcpClientSocket *> m_TcpClientSocketList;

    static CTrainTcpServer s_instance;
};

#endif // CTRAINTCPSERVER_H
