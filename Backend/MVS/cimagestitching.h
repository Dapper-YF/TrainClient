#ifndef CIMAGESTITCHING_H
#define CIMAGESTITCHING_H

#include <QThread>
#include <QMutex>
#include <QMap>
#include <opencv2/opencv.hpp>
#include <QDateTime>
#include "../include/define.h"

class CImageStitching : public QThread
{
    Q_OBJECT
public:
    explicit CImageStitching(QObject *parent = nullptr);

    //获取车厢图片
    //通过开始与结束时间获取图片
    QImage GetImageByTime(quint64 unStartMSec, quint64 unEndMSec);
    //通过开始时间与图片长度获取图片
    QImage GetImageByLength(quint64 unStartMSec, int nLength);

    void StitchingImages(std::vector<FrameRecord>* pQueue,QString fullSaveName);
protected:
    void run() override;

signals:

private slots:
    QString Stitching(const QString& strFilePath);

private:
    QMutex m_mutexStitching;
    QString m_strFilePath;  //相机采集图片路径
    QString m_strStitchingFilaPath; //合并后的图片保存路径

    //获得采集目录下所有PNG文件，并按时间排序
    QMap<quint64, QString> GetAllPngFile();
};

#endif // CIMAGESTITCHING_H
