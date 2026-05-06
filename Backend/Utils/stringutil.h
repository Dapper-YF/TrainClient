#ifndef STRINGUTIL_H
#define STRINGUTIL_H
#include <QString>
#include <qbytearray.h>

class StringUtil
{
public:
    StringUtil();
    static QByteArray hexStringToByteArray(const QString &hexStr);
    static QString ensurePathAndGet(const QString& savePath, const QString& subPath);
};

#endif // STRINGUTIL_H
