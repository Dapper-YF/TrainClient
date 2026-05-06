#include "clienthandler.h"
#include "common/protocol.h"
#include "common/constants.h"
#include <QDataStream>
#include <QFileInfo>
#include <QDebug>
#include <QThread>
#include "logger.h"

ClientHandler::ClientHandler(QObject *parent)
    : QObject(parent)
    , m_socket(nullptr)
{
}

ClientHandler::~ClientHandler()
{
    if (m_socket) {
        m_socket->disconnectFromHost();
        m_socket->deleteLater();
    }
}

bool ClientHandler::setupSocket(qintptr socketDescriptor)
{
    m_socket = new QTcpSocket(this);
    if (!m_socket->setSocketDescriptor(socketDescriptor)) {
        qDebug() << "设置套接字描述符失败";
        delete m_socket;
        m_socket = nullptr;
        return false;
    }

    connect(m_socket, &QTcpSocket::readyRead, this, &ClientHandler::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &ClientHandler::onDisconnected);

    qDebug() << "客户端连接成功";
    return true;
}

void ClientHandler::setImageDirectory(const QString &directory)
{
    m_imageDirectory = directory;
}

void ClientHandler::onReadyRead()
{
    QDataStream in(m_socket);
    in.setVersion(QDataStream::Qt_5_9);

    while (m_socket->bytesAvailable() > 0) {
        // 读取消息类型
        quint8 msgType;
        in >> msgType;

        qDebug() << "收到消息类型:" << msgType;

        switch (msgType) {
        case MSG_REQUEST_IMAGE: {
            ImageRequest request;
            request.deserialize(in);
            qDebug() << "处理图片请求:" << request.imageName << "请求ID:" << request.requestId;
            processImageRequest(request);
            break;
        }
        default:
            qWarning() << "未知消息类型:" << msgType;
            break;
        }
    }
}

void ClientHandler::onDisconnected()
{
    qDebug() << "客户端断开连接";
    emit disconnected(this);
}

void ClientHandler::processImageRequest(const ImageRequest &request)
{
    QString filePath = m_imageDirectory + "/" + request.imageName;
    QFile file(filePath);

    if (!file.exists()) {
        qDebug() << "图片不存在:" << filePath;
        sendError(request.requestId, "图片不存在: " + request.imageName);
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "无法打开图片:" << filePath;
        sendError(request.requestId, "无法打开图片: " + request.imageName);
        return;
    }

    qDebug() << "开始传输图片:" << request.imageName << "大小:" << file.size();

    // 读取文件数据
    QByteArray imageData = file.readAll();
    file.close();

    // 分块传输
    const int chunkSize = Constants::CHUNK_SIZE;
    int totalChunks = (imageData.size() + chunkSize - 1) / chunkSize;

    for (int i = 0; i < totalChunks; i++) {
        int start = i * chunkSize;
        int end = qMin(start + chunkSize, imageData.size());
        QByteArray chunk = imageData.mid(start, end - start);
        bool isLastChunk = (i == totalChunks - 1);

        sendImageData(request.imageName, request.requestId, chunk, isLastChunk);
        LOG("第"+QString::number(i)+"块传输完成");
        // 小延迟，避免过快发送
        if (!isLastChunk) {
            QThread::msleep(1);
        }
    }

    qDebug() << "图片传输完成:" << request.imageName;
}

void ClientHandler::sendImageData(const QString &imageName, int requestId,
                                  const QByteArray &data, bool isLastChunk)
{
    ImageData imageData;
    imageData.imageName = imageName;
    imageData.requestId = requestId;
    imageData.imageData = data;
    imageData.isLastChunk = isLastChunk;

    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_9);

    out << quint8(MSG_IMAGE_DATA);
    imageData.serialize(out);

    m_socket->write(block);
    m_socket->flush();
}

void ClientHandler::sendError(int requestId, const QString &error)
{
    ErrorMessage errorMsg;
    errorMsg.requestId = requestId;
    errorMsg.errorString = error;

    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_9);

    out << quint8(MSG_ERROR);
    errorMsg.serialize(out);

    m_socket->write(block);
    m_socket->flush();

    qDebug() << "发送错误消息:" << error;
}
