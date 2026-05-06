#include "ImagePathMapper.h"
#include <QFile>
#include <QDir>
#include <QDebug>

// relative path: run from Client dir, use ..\mvsimages
#define MVS_STR(x) #x
#ifdef MVSIMAGES_PATH
QString ImagePathMapper::s_localRoot = MVS_STR(MVSIMAGES_PATH);
#else
QString ImagePathMapper::s_localRoot = R"(..\mvsimages)";
#endif

void ImagePathMapper::setLocalRoot(const QString& path)
{
    s_localRoot = path;
}

QString ImagePathMapper::localRoot()
{
    return s_localRoot;
}

bool ImagePathMapper::exists(const QString& localPath)
{
    return QFile::exists(localPath);
}

QString ImagePathMapper::extractDateFolder(const QString& backendPath)
{
    // 后端路径如: D:/MVS/2025-11-13/2026-04-23/left/1776911221.jpg
    // 提取第一个日期格式的文件夹 (YYYY-MM-DD)
    QStringList parts = backendPath.split('/');
    for (const QString& part : parts) {
        if (part.size() == 10 && part[4] == '-' && part[7] == '-') {
            return part;
        }
    }
    return QString();
}

QString ImagePathMapper::buildSequentialPath(const QString& dateFolder, int carriageOrder, int totalCarriages)
{
    Q_UNUSED(dateFolder);
    // 根据车厢总数决定零填充位数：8车厢用01，12车厢用001
    int width = (totalCarriages >= 100) ? 3 : 2;
    return QString::number(carriageOrder).rightJustified(width, '0') + ".jpg";
}

QString ImagePathMapper::map(const QString& backendPath, int carriageOrder, int totalCarriages)
{
    if (backendPath.isEmpty())
        return QString();

    // Step 0: 处理 /trainimages/ 前缀 (来自后端 imagePathMap)
    // 后端 imagePathMap["left"] = "/trainimages/2026-04-22/01.jpg"
    // 需要去掉 /trainimages/ 前缀，再映射到本地 mvsimages 路径
    QString pathToMap = backendPath;
    if (pathToMap.startsWith("/trainimages/")) {
        pathToMap = pathToMap.mid(13); // 去掉 "/trainimages/" 得到 "2026-04-22/01.jpg"
        // 提取日期目录
        QString dateFolder = extractDateFolder(pathToMap);
        if (!dateFolder.isEmpty()) {
            QString localDatePath = s_localRoot + "/" + dateFolder;
            if (QDir(localDatePath).exists()) {
                QString filename = pathToMap.mid(pathToMap.lastIndexOf('/') + 1);
                // 尝试 left/ right/ top/ 三个子目录
                static const char* views[] = { "left", "right", "top" };
                for (const char* view : views) {
                    QString tryPath = localDatePath + "/" + QString(view) + "/" + filename;
                    tryPath.replace('/', '\\');
                    if (exists(tryPath)) {
                        qDebug() << "[ImagePathMapper] /trainimages/ match:" << tryPath;
                        return tryPath;
                    }
                }
                // 文件名可能是序号 (01.jpg)，不是时间戳，尝试构建顺序文件名
                // 判断是否需要构建顺序文件名 (文件名纯数字或带零填充序号)
                bool isSequential = (filename.size() <= 4 && filename.endsWith(".jpg"));
                if (isSequential) {
                    QString seqName = buildSequentialPath(dateFolder, carriageOrder, totalCarriages);
                    static const char* views2[] = { "left", "right", "top" };
                    for (const char* view : views2) {
                        QString tryPath = localDatePath + "/" + QString(view) + "/" + seqName;
                        tryPath.replace('/', '\\');
                        if (exists(tryPath)) {
                            qDebug() << "[ImagePathMapper] /trainimages/ sequential match:" << tryPath;
                            return tryPath;
                        }
                    }
                }
            }
        }
        // /trainimages/ 路径无法映射，返回空
        qDebug() << "[ImagePathMapper] /trainimages/ path not found:" << backendPath;
        return QString();
    }

    // Step 1: 尝试直接替换 D:/MVS/ -> 本地根目录
    // "D:/MVS/2025-11-13/2026-04-23/left/1776911221.jpg"
    // -> "E:/Study/AI_Workspace/TrainClient/mvsimages/2025-11-13/..."
    QString localPath = backendPath;
    // Step 1: D:/MVS/  ->  localRoot
    if (localPath.startsWith("D:/MVS/", Qt::CaseInsensitive) ||
        localPath.startsWith("D:\\MVS\\", Qt::CaseInsensitive)) {
        localPath = s_localRoot + "/" + localPath.mid(7);
    } else if (localPath.startsWith("D:/MVS", Qt::CaseInsensitive)) {
        // handle D:/MVS without trailing slash
        localPath = s_localRoot + "/" + localPath.mid(7);
    }
    // 统一路径分隔符 (for Windows file exists check)
    localPath.replace('/', '\\');

    if (exists(localPath)) {
        qDebug() << "[ImagePathMapper] Exact match:" << localPath;
        return localPath;
    }

    // Step 2: 提取日期文件夹，映射到本地对应日期目录
    QString dateFolder = extractDateFolder(backendPath);
    if (!dateFolder.isEmpty()) {
        // 构建本地日期目录路径
        QString localDatePath = s_localRoot + "/" + dateFolder;

        // 检查日期目录是否存在
        if (QDir(localDatePath).exists()) {
            // 从原路径提取文件名
            QString filename = backendPath.mid(backendPath.lastIndexOf('/') + 1);
            QString localFilePath;

            // 尝试：本地日期目录 + 原文件名
            localFilePath = localDatePath + "/" + filename;
            localFilePath.replace('/', '\\');
            if (exists(localFilePath)) {
                qDebug() << "[ImagePathMapper] Date folder match:" << localFilePath;
                return localFilePath;
            }

            // 如果原文件名是 Unix 时间戳，尝试用顺序文件名
            // 时间戳格式：纯数字，长度 >= 10 位
            if (filename.size() >= 10 && filename.left(3).toInt() >= 100) {
                QString seqName = buildSequentialPath(dateFolder, carriageOrder, totalCarriages);

                // 尝试 left/right/top + 顺序文件名
                QString viewFolder;
                if (backendPath.contains("/left/")) viewFolder = "left";
                else if (backendPath.contains("/right/")) viewFolder = "right";
                else if (backendPath.contains("/top/")) viewFolder = "top";

                if (!viewFolder.isEmpty()) {
                    localFilePath = localDatePath + "/" + viewFolder + "/" + seqName;
                    localFilePath.replace('/', '\\');
                    if (exists(localFilePath)) {
                        qDebug() << "[ImagePathMapper] Sequential match:" << localFilePath;
                        return localFilePath;
                    }
                }
            }

            // 日期目录存在但文件不匹配，返回日期目录路径供调用方进一步处理
            qDebug() << "[ImagePathMapper] Date dir exists but file not found, returning:" << localDatePath;
            return localDatePath;
        } else {
            qDebug() << "[ImagePathMapper] Date folder not found locally:" << localDatePath;
        }
    }

    // Step 3: 找不到，返回空
    qDebug() << "[ImagePathMapper] No mapping found for:" << backendPath;
    return QString();
}
