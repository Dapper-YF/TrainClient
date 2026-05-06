#ifndef CTCPCLIENTSOCKET_H
#define CTCPCLIENTSOCKET_H

#include <QTcpSocket>

class CTcpClientSocket : public QTcpSocket
{
    Q_OBJECT
public:
    CTcpClientSocket(QObject *parent = nullptr);

    void SetClient(bool bIsClient);
    bool GetCLient() const{return m_bIsClient;};

    //发送数据
    bool SendDatas(const char *pDatas, quint32 nLen);
    bool SendDatas(const QByteArray& array);

protected slots:
    void ReceiveData();
    void slotClientDisconnected();

signals:
    void updateserver(QString msg);
    void clientDisconnected(int);

private:
    //是否客户端
    bool m_bIsClient{false};
};

#endif // CTCPCLIENTSOCKET_H
