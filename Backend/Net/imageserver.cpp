#include "imageserver.h"
#include "common/constants.h"
#include <QDir>

ImageServer::ImageServer(QObject *parent) 
    : QTcpServer(parent)
{
}

ImageServer::~ImageServer()
{
    qDeleteAll(m_clients);
}

bool ImageServer::startServer(quint16 port)
{
    if (!listen(QHostAddress::Any, port)) {
        qWarning() << "无法启动服务器:" << errorString();
        return false;
    }
    
    qDebug() << "服务器已启动，监听端口:" << port;
    return true;
}

void ImageServer::setImageDirectory(const QString &directory)
{
    m_imageDirectory = directory;
    QDir dir(directory);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    qDebug() << "图片目录设置为:" << directory;
}

void ImageServer::incomingConnection(qintptr socketDescriptor)
{
    ClientHandler *client = new ClientHandler(this);
    client->setImageDirectory(m_imageDirectory);
    
    if (client->setupSocket(socketDescriptor)) {
        m_clients.insert(client);
        connect(client, &ClientHandler::disconnected, 
                this, &ImageServer::onClientDisconnected);
        qDebug() << "新客户端连接，当前客户端数:" << m_clients.size();
    } else {
        delete client;
    }
}

void ImageServer::onClientDisconnected(ClientHandler *client)
{
    m_clients.remove(client);
    client->deleteLater();
    qDebug() << "客户端断开连接，剩余客户端数:" << m_clients.size();
}
