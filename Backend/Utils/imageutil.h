#ifndef IMAGEUTIL_H
#define IMAGEUTIL_H
#include <opencv2/opencv.hpp>
#include <QDir>
#include "../include/define.h"
using namespace cv;
class ImageUtil
{
public:
    ImageUtil();
    static void SplitImageV(String sourceFileName,String destFolder,String prefx,int splitNum);
    static void SplitImageH(String sourceFileName,String destFolder,String prefx,int splitNum);
    static cv::Mat StitchTrainImage(const std::vector<cv::Mat>& frames, double velocity, int sampling_hz,int exposure_mm);
    static void StichImageInFolder(const QString  destPath, double velocity, int sampling_hz,int exposure_mm);
    static QImage cvMatToQImage(const cv::Mat &mat);
    static cv::Mat stichImageInVector(std::vector<FrameRecord>* pVector,quint64 fromTime,quint64 toTime, bool up2down=true);
};

#endif // IMAGEUTIL_H
