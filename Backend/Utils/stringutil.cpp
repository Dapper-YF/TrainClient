#include <QString>
#include <qbytearray.h>
#include "stringutil.h"
#include "logger.h"
#include <QDir>

StringUtil::StringUtil() {}
/**
 * @brief 将十六进制字符串转换为QByteArray，用于测试PLC信号
 * @param hexStr 空格分隔的十六进制字符串（如"0e 00 00 00"）
 * @return 对应的字节数组，转换失败返回空数组
 */
QByteArray StringUtil::hexStringToByteArray(const QString &hexStr) {
    // 清理输入：移除多余空格和不可见字符
    QString cleaned = hexStr.simplified();
    cleaned.replace(" ", "");

    // 验证长度（必须为偶数个字符）
    if(cleaned.length() % 2 != 0) {
        qWarning() << "错误：十六进制字符串长度应为偶数，当前长度" << cleaned.length();
        return QByteArray();
    }

    QByteArray byteArray;
    bool ok;

    // 每2个字符转换为一个字节
    for(int i = 0; i < cleaned.length(); i += 2) {
        QString byteStr = cleaned.mid(i, 2);
        char byte = static_cast<char>(byteStr.toUInt(&ok, 16));

        if(!ok) {
            qWarning() << "转换失败：" << byteStr;
            return QByteArray();
        }
        byteArray.append(byte);
    }

    return byteArray;
}
QString StringUtil::ensurePathAndGet(const QString& savePath, const QString& subPath) {
    // 获取当前日期（格式：yyyy-MM-dd）
    const QString currentDate = QDate::currentDate().toString("yyyy-MM-dd");

    // 构建完整路径：savePath/currentDate/subPath
    QString fullPath = savePath;

    // 规范化路径分隔符（适配不同操作系统）
    if (!fullPath.endsWith('/') && !fullPath.endsWith('\\')) {
        fullPath += '/';
    }
    fullPath +=  currentDate +'/' + subPath + '/';

    // 创建QDir对象并确保路径存在
    QDir dir;
    if (!dir.exists(fullPath)) {
        // mkpath会自动创建所有不存在的父级目录
        if (!dir.mkpath(fullPath)) {
            // 创建失败时返回空字符串（可改为抛异常或日志记录）
            return QString();
        }
    }

    return fullPath;
}
