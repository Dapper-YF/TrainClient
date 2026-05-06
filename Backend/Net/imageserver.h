#ifndef IMAGESERVER_H
#define IMAGESERVER_H

#include <QTcpServer>
#include <QMap>
#include <QSet>
#include "clienthandler.h"

class ImageServer : public QTcpServer
{
    Q_OBJECT

public:
    explicit ImageServer(QObject *parent = nullptr);
    ~ImageServer();

    bool startServer(quint16 port);
    void setImageDirectory(const QString &directory);

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private slots:
    void onClientDisconnected(ClientHandler *client);

private:
    QString m_imageDirectory;
    QSet<ClientHandler*> m_clients;
};

#endif // IMAGESERVER_H