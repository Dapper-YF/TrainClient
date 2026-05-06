#ifndef IMAGEPATHMAPPER_H
#define IMAGEPATHMAPPER_H

#include <QString>
#include <QStringList>

// ============================================================
// 图片路径映射器
//
// 职责：将后端返回的 imagePathMap 路径，映射到本地历史图片目录
//
// 路径转换规则：
//   后端: D:/MVS/2025-11-13/2026-04-23/left/1776911221.jpg
//   本地: E:/Study/AI_Workspace/TrainClient/mvsimages/2025-11-13/...
//
// 如果本地找不到精确路径，按以下顺序尝试：
//   1. 直接替换 D:/MVS/ -> 本地 mvsimages 根目录
//   2. 用日期文件夹 (如 2025-11-13) + 车厢顺序号 (01.jpg) 构造路径
//   3. 返回空字符串（由调用方显示占位图）
// ============================================================
class ImagePathMapper
{
public:
    // 初始化本地 mvsimages 根目录
    static void setLocalRoot(const QString& path);

    // 将后端返回的 imagePathMap 中的一条路径，映射到本地路径
    static QString map(const QString& backendPath, int carriageOrder, int totalCarriages);

    // 检查给定路径是否指向有效的本地文件
    static bool exists(const QString& localPath);

    // 获取 mvsimages 根目录
    static QString localRoot();

private:
    // 从路径中提取第一个日期格式的文件夹 (YYYY-MM-DD)
    static QString extractDateFolder(const QString& backendPath);

    // 用日期 + 车厢序号构造顺序文件名 (e.g. "2025-11-13", 1, 8 -> "01.jpg")
    static QString buildSequentialPath(const QString& dateFolder, int carriageOrder, int totalCarriages);

    static QString s_localRoot;
};

#endif // IMAGEPATHMAPPER_H
