#include "protocol.h"
#include <QDataStream>

// ImageRequest 序列化方法
void ImageRequest::serialize(QDataStream &out) const
{
    out << imageName;
    out << requestId;
}

void ImageRequest::deserialize(QDataStream &in)
{
    in >> imageName;
    in >> requestId;
}

// ImageData 序列化方法
void ImageData::serialize(QDataStream &out) const
{
    out << imageName;
    out << requestId;
    out << imageData;
    out << isLastChunk;
}

void ImageData::deserialize(QDataStream &in)
{
    in >> imageName;
    in >> requestId;
    in >> imageData;
    in >> isLastChunk;
}

// ErrorMessage 序列化方法
void ErrorMessage::serialize(QDataStream &out) const
{
    out << requestId;
    out << errorString;
}

void ErrorMessage::deserialize(QDataStream &in)
{
    in >> requestId;
    in >> errorString;
}
