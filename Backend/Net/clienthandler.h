#ifndef CLIENTHANDLER_H
#define CLIENTHANDLER_H

#include <QTcpSocket>
#include <QFile>
#include <QMap>

// 前向声明
struct ImageRequest;

class ClientHandler : public QObject
{
    Q_OBJECT

public:
    explicit ClientHandler(QObject *parent = nullptr);
    ~ClientHandler();

    bool setupSocket(qintptr socketDescriptor);
    void setImageDirectory(const QString &directory);

signals:
    void disconnected(ClientHandler *client);

private slots:
    void onReadyRead();
    void onDisconnected();

private:
    void processImageRequest(const ImageRequest &request);
    void sendImageData(const QString &imageName, int requestId,
                       const QByteArray &data, bool isLastChunk);
    void sendError(int requestId, const QString &error);

private:
    QTcpSocket *m_socket;
    QString m_imageDirectory;
};

#endif // CLIENTHANDLER_H
