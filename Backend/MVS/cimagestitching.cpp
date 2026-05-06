#include "cimagestitching.h"
#include "clinecameraitem.h"
#include <QDir>
#include <QImage>
#include <QPainter>
#include <QTransform>
#include <opencv2/opencv.hpp>
#include "clinecameraitem.h"

CImageStitching::CImageStitching(QObject *parent)
    : QThread(parent)
{
    connect((CLineCameraItem*)parent, &CLineCameraItem::singalStitching, this, &CImageStitching::Stitching);
}
cv::Mat stitchImages(std::vector<FrameRecord>& frames) {
    if (frames.empty()) return cv::Mat();
    std::vector<cv::Mat> images;
    foreach (auto frame, frames) {
        images.push_back(frame.frame);
    }
    // 初始化拼接器和参数
    cv::Ptr<cv::Stitcher> stitcher = cv::Stitcher::create(cv::Stitcher::PANORAMA);
    stitcher->setPanoConfidenceThresh(0.5);  // 降低匹配阈值适应低重叠场景

    // 尝试标准拼接（处理有重叠的情况）
    cv::Mat result;
    cv::Stitcher::Status status = stitcher->stitch(images, result);

    // 标准拼接失败时处理无重叠/低重叠场景
    if (status != cv::Stitcher::OK) {
        result = images[0].clone();
        cv::Rect roi(0, 0, images[0].cols, images[0].rows);

        for (size_t i = 1; i < images.size(); ++i) {
            // 特征匹配检测重叠区域
            cv::Ptr<cv::ORB> orb = cv::ORB::create();
            std::vector<cv::KeyPoint> kp1, kp2;
            cv::Mat desc1, desc2;
            orb->detectAndCompute(result(roi), cv::noArray(), kp1, desc1);
            orb->detectAndCompute(images[i], cv::noArray(), kp2, desc2);

            // 无重叠时直接连接
            if (kp1.empty() || kp2.empty()) {
                cv::Mat newResult(result.rows, result.cols + images[i].cols, result.type());
                result.copyTo(newResult(cv::Rect(0, 0, result.cols, result.rows)));
                images[i].copyTo(newResult(cv::Rect(result.cols, 0, images[i].cols, result.rows)));
                result = newResult;
                roi = cv::Rect(0, 0, result.cols, result.rows);
                continue;
            }

            // 有重叠时计算最优变换
            cv::BFMatcher matcher(cv::NORM_HAMMING);
            std::vector<cv::DMatch> matches;
            matcher.match(desc1, desc2, matches);

            // 提取匹配点并计算偏移量
            std::vector<cv::Point2f> pts1, pts2;
            for (const auto& m : matches) {
                pts1.push_back(kp1[m.queryIdx].pt);
                pts2.push_back(kp2[m.trainIdx].pt);
            }

            // 计算最优垂直对齐（最小化接缝）
            double y_offset = 0;
            if (!pts1.empty() && !pts2.empty()) {
                std::vector<float> diffs;
                for (size_t j = 0; j < pts1.size(); ++j) {
                    diffs.push_back(pts1[j].y - pts2[j].y);
                }
                std::sort(diffs.begin(), diffs.end());
                y_offset = diffs[diffs.size()/2];  // 取中位数减少误差
            }

            // 创建新画布并混合图像
            int newWidth = result.cols + images[i].cols;
            int newHeight = std::max(result.rows, static_cast<int>(images[i].rows + std::abs(y_offset)));
            cv::Mat newResult = cv::Mat::zeros(newHeight, newWidth, result.type());

            // 放置已拼接部分
            result.copyTo(newResult(cv::Rect(0, 0, result.cols, result.rows)));

            // 放置新图像（带偏移优化）
            cv::Mat targetROI = newResult(cv::Rect(result.cols, y_offset, images[i].cols, images[i].rows));
            images[i].copyTo(targetROI);

            // 更新ROI用于下一帧
            result = newResult;
            roi = cv::Rect(0, 0, result.cols, result.rows);
        }
    }
    return result;
}
void CImageStitching::StitchingImages(std::vector<FrameRecord>* pQueue,QString fullSaveName){
    // cv::Ptr<cv::Stitcher> stitcher = cv::Stitcher::create(cv::Stitcher::SCANS);
    // // 针对金属表面优化拼接参数
    // stitcher->setRegistrationResol(0.9);
    // stitcher->setPanoConfidenceThresh(0.3);

    // cv::Mat panorama;
    // //
    // std::vector<cv::Mat> tempQueue{(*pQueue)[0],(*pQueue)[1]};
    // cv::Stitcher::Status status = stitcher->stitch(tempQueue, panorama);
    // int i=2;
    // while(status== cv::Stitcher::OK&&i<pQueue->size()){
    //     tempQueue.clear();
    //     tempQueue.push_back(panorama);
    //     tempQueue.push_back((*pQueue)[i]);
    //     status = stitcher->stitch(tempQueue, panorama);
    //     cv::String title;
    //     title="queue";
    //     title.append(std::to_string(i));
    //     cv::imshow(title,(*pQueue)[i]);
    //     i++;
    // }
    // cv::imshow("panorma",panorama);
    // if (status == cv::Stitcher::OK) {
    //     cv::imwrite(fullSaveName.toStdString(), panorama);
    // }else{

    //     cv::imshow("imgi",(*pQueue)[i]);
    //     cv::waitKey(0);
    // }
    // pQueue->clear(); // 清空队列
    cv::Mat panorama=stitchImages(*pQueue);
    cv::imshow("panorma",panorama);
    cv::imwrite(fullSaveName.toStdString(), panorama);
}
QString CImageStitching::Stitching(const QString &strFilePath)
{
    cv::Ptr<cv::Stitcher> stitcher = cv::Stitcher::create(cv::Stitcher::PANORAMA);

    // 针对金属表面优化拼接参数
    stitcher->setRegistrationResol(0.6);
    stitcher->setPanoConfidenceThresh(0.8);

    // cv::Mat panorama;
    // cv::Stitcher::Status status = stitcher->stitch(imageQueue, panorama);

    // if (status == cv::Stitcher::OK) {
    //     cv::imwrite("train_panorama.jpg", panorama);
    // }
    // imageQueue.clear(); // 清空队列



    ///////////////////////////////////////////////////////////////////
    m_strFilePath = strFilePath;
    //合并后的图片路径
    m_strStitchingFilaPath = m_strFilePath + "\\stitch.png";

    if (!isRunning())
    {
        start();
    }

    return m_strStitchingFilaPath;
}
/*
QImage CImageStitching::GetImageByTime(quint64 unStartMSec, quint64 unEndMSec)
{

}

QImage CImageStitching::GetImageByLength(quint64 unStartMSec, int nLength)
{

}
*/

QMap<quint64, QString> CImageStitching::GetAllPngFile()
{
    QMap<quint64, QString> mapFile;

    QDir pathDir(m_strFilePath);
    if (!pathDir.exists())
    {
        return mapFile;
    }

    QStringList strFilter;
    strFilter << "*.png";

    QFileInfoList fileInfoList = pathDir.entryInfoList(strFilter, QDir::Files | QDir::NoSymLinks);
    foreach (QFileInfo fileInfo, fileInfoList)
    {
        QString strFilePath = fileInfo.absoluteFilePath();
        QString strFileName = fileInfo.baseName();
        quint64 unFileNameMSec = strFileName.toULongLong();

        mapFile[unFileNameMSec] = strFilePath;
    }

    return mapFile;
}

void CImageStitching::run()
{
    QMutexLocker locker(&m_mutexStitching);

    QMap<quint64, QString> mapFile = GetAllPngFile();
    if (mapFile.size() <= 0)
    {
        return;
    }

    int nSrcWidth = 0, nDesHeight = 0, nSrcHeight;
    QImage stitchingImage;
    int nOffY = 0;

    for (QMap<quint64, QString>::iterator itor = mapFile.begin();
         itor != mapFile.end(); ++itor)
    {
        QString strFilePath = itor.value();

        QImage image(strFilePath);
        //获得每张图片的宽度和高度
        if (0 == nSrcWidth)
        {
            nSrcWidth = image.size().width();
            nSrcHeight = image.size().height();
            nDesHeight = nSrcHeight * mapFile.size();

            stitchingImage = QImage(nSrcWidth, nDesHeight, QImage::Format_ARGB32);
        }
        else if (image.size().width() != nSrcWidth || image.size().height() != nSrcHeight)//图片尺寸不对
        {
            continue;
        }

        QPainter painter(&stitchingImage);
        painter.drawImage(0, nOffY, image);
        nOffY += nSrcHeight;
    }

    //图片旋转90
    QTransform transform;
    transform.rotate(90);
    QImage rotateImage = stitchingImage.transformed(transform);

    //保存文件
    rotateImage.save(m_strStitchingFilaPath);
}
