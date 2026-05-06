#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QString>
#include <QDataStream>

enum MessageType {
    MSG_REQUEST_IMAGE = 0x01,
    MSG_IMAGE_DATA = 0x02,
    MSG_IMAGE_LIST = 0x03,
    MSG_ERROR = 0xFF
};

struct ImageRequest {
    QString imageName;
    int requestId;

    // 添加序列化方法
    void serialize(QDataStream &out) const;
    void deserialize(QDataStream &in);
};

struct ImageData {
    QString imageName;
    int requestId;
    QByteArray imageData;
    bool isLastChunk;

    // 添加序列化方法
    void serialize(QDataStream &out) const;
    void deserialize(QDataStream &in);
};

struct ErrorMessage {
    int requestId;
    QString errorString;

    // 添加序列化方法
    void serialize(QDataStream &out) const;
    void deserialize(QDataStream &in);
};

#endif // PROTOCOL_H
