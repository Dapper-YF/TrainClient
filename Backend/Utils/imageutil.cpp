#include "imageutil.h"
#include <filesystem>  // 需要C++17支持
// #include <opencv2/ximgproc.hpp>  // ximgproc not available in this OpenCV build
// #include <QDebug>
#include "logger.h"
#include "mvs/cimagestitching.h"
#include <QImage>

namespace fs = std::filesystem;
ImageUtil::ImageUtil() {}
void ImageUtil::StichImageInFolder(const QString folderPath, double velocity, int sampling_hz,int exposure_mm){
    // 1. 获取并排序图像文件
    QDir directory(folderPath);
    QStringList imageFiles = directory.entryList(
        {"*.jpg", "*.png", "*.bmp", "*.jpeg"}, // 支持的格式
        QDir::Files,                          // 仅文件
        QDir::Name                            // 按文件名排序
        );

    if (imageFiles.isEmpty()) {
        Logger::instance()->Log("未找到图像文件!");
    }
    //初始化图像列表
    std::vector<FrameRecord> images;
    // 2. 遍历处理图像
    for (const QString &filename : imageFiles) {
        QString fullPath = directory.filePath(filename);
        Logger::instance()->Log("处理文件:" + fullPath);
        cv::String fullName=fullPath.toStdString();
        // 使用OpenCV读取图像
        cv::Mat image = cv::imread(fullName,cv::IMREAD_COLOR);
        if (image.empty()) {
            LOG("  加载失败!");
            continue;
        }
        images.push_back({QDateTime::currentDateTime().toMSecsSinceEpoch(), image});
    }
    // StitchTrainImage(images,velocity,sampling_hz,exposure_mm);
    (new CImageStitching(nullptr))->StitchingImages(&images,"D:\\testresult.jpg");

}
/**
 * 垂直分割图像（按行分割），用于产生样本图像
 *
 * 修改说明：
 * 1. 将水平分割（按列）改为垂直分割（按行）
 * 2. ROI区域参数调整为垂直方向切割
 * 3. 高度计算逻辑调整（原宽度计算改为高度计算）
 * */
void ImageUtil::SplitImageV(String sourceFileName, String destFolder, String prefix, int splitNum) {
    // 1. 读取源图像
    cv::Mat sourceImage = cv::imread(sourceFileName, cv::IMREAD_COLOR);
    if (sourceImage.empty()) {
        std::cerr << "Error: Could not load image " << sourceFileName << std::endl;
        return;
    }

    // 2. 检查分割数量有效性（针对高度）
    if (splitNum <= 0 || splitNum > sourceImage.rows) {  // 修改：cols -> rows
        std::cerr << "Error: Invalid split number " << splitNum << std::endl;
        return;
    }

    // 3. 创建目标目录
    if (!fs::exists(destFolder) && !fs::create_directories(destFolder)) {
        std::cerr << "Error: Failed to create directory " << destFolder << std::endl;
        return;
    }

    // 4. 计算垂直分割参数
    int segmentHeight = sourceImage.rows / splitNum;  // 修改：宽度->高度
    int remainder = sourceImage.rows % splitNum;      // 修改：cols -> rows

    // 5. 按行分割并保存
    for (int i = 0, y = 0; i < splitNum; i++) {  // 修改：x->y（垂直方向起始点）
        // 计算当前分段高度（处理最后一行余数）
        int currentHeight = segmentHeight + (i == splitNum - 1 ? remainder : 0);

        // 提取垂直ROI区域（关键修改）
        cv::Rect roi(0, y, sourceImage.cols, currentHeight);  // 修改：垂直矩形区域
        cv::Mat segment = sourceImage(roi);

        // 生成文件名
        std::string order_num=std::to_string((i + 1)*0.000001).substr(8-3); //="042"
        std::string outputPath = destFolder + "/" + prefix + "_"
                                 + order_num + ".png";

        // 保存分段图像
        if (!cv::imwrite(outputPath, segment)) {
            std::cerr << "Error: Failed to save segment " << i + 1 << std::endl;
        }

        y += currentHeight;  // 修改：垂直方向移动
    }
    //旋转90度
    std::cout << "Successfully vertically split image into " << splitNum << " segments" << std::endl;
}

/***
 * 分割图像，用于产生样本图像，方便实验
 *
 * * */
void ImageUtil::SplitImageH(String sourceFileName,String destFolder,String prefix,int splitNum){
    // 1. 读取源图像
    cv::Mat sourceImage = cv::imread(sourceFileName, cv::IMREAD_COLOR);
    if (sourceImage.empty()) {
        std::cerr << "Error: Could not load image " << sourceFileName << std::endl;
        return;
    }

    // 2. 检查分割数量有效性
    if (splitNum <= 0 || splitNum > sourceImage.cols) {
        std::cerr << "Error: Invalid split number " << splitNum << std::endl;
        return;
    }

    // 3. 创建目标目录
    if (!fs::exists(destFolder) && !fs::create_directories(destFolder)) {
        std::cerr << "Error: Failed to create directory " << destFolder << std::endl;
        return;
    }

    // 4. 计算分割参数
    int segmentWidth = sourceImage.cols/ splitNum;
    int remainder = sourceImage.cols % splitNum;  // 处理余数

    // 5. 分割并保存图像
    for (int i = 0, x = 0; i < splitNum; i++) {
        // 计算当前分段的宽度（处理最后一列余数）
        int currentWidth = segmentWidth + (i == splitNum - 1 ? remainder : 0);

        // 提取ROI区域
        if(x+currentWidth*2>=sourceImage.cols){
            currentWidth=(sourceImage.cols-x)/2;
            if(currentWidth<=0){
                break;
            }
        }
        cv::Rect roi(x, 0, currentWidth*2, sourceImage.rows);
        cv::Mat segment = sourceImage(roi);

        std::string order_num=std::to_string((i + 1)*0.000001).substr(8-3); //="042"
        // 生成文件名
        std::string outputPath = destFolder + "/" + prefix + "_"
                                 + order_num + ".png";

        // 保存分段图像
        if (!cv::imwrite(outputPath, segment)) {
            std::cerr << "Error: Failed to save segment " << i + 1 << std::endl;
        }

        x += currentWidth;  // 移动截取位置
    }

    std::cout << "Successfully split image into " << splitNum << " segments" << std::endl;

}
// 运动补偿去模糊
std::vector<cv::Mat> motionDeblur(const std::vector<cv::Mat>& frames,
                                  double v, double k,int exposure_mm) {
    const double line_spacing = v / k; // 每行像素的实际物理距离
    const int kernel_size = std::max(3, static_cast<int>(exposure_mm / line_spacing));

    std::vector<cv::Mat> result;
    cv::Mat kernel = cv::Mat::ones(1, kernel_size, CV_32F) / kernel_size;

#pragma omp parallel for
    for(int i = 0; i < frames.size(); ++i) {
        cv::Mat frame_float;
        frames[i].convertTo(frame_float, CV_32F, 1.0/255.0);

        // 维纳滤波去模糊
        cv::Mat deblurred;
        cv::filter2D(frame_float, deblurred, -1, kernel, cv::Point(-1,-1), 0, cv::BORDER_REFLECT);
        // cv::ximgproc::wienerFilter(deblurred, frame_float, deblurred, Size(15,15));

        // 对比度受限自适应直方图均衡
        // cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, cv::Size(8,8));
        // clahe->apply(deblurred, deblurred);

        result.push_back(deblurred);
    }
    return result;
}

// 计算总高度（考虑亚像素精度）
int calculateTotalHeight(const std::vector<cv::Mat>& frames,double velocity,int sampling_hz) {
    double total_pixels = 0;
    for(const auto& frame : frames) {
        // 考虑线阵相机的行间距补偿
        double row_compensation = 1.0 + (velocity / (sampling_hz * frame.rows));
        total_pixels += frame.rows * row_compensation;
    }
    return static_cast<int>(std::ceil(total_pixels));
}

// 多频带融合拼接
void multiBandBlend(const std::vector<cv::Mat>& frames, cv::Mat& panorama) {
    int current_y = 0;
    int i=0;

    for(; i <frames.size(); i++) {
        cv::Rect roi(0, current_y, frames[i].cols, frames[i].rows);

        // 创建权重图（中心重两侧轻）
        cv::Mat weights = cv::Mat::zeros(frames[i].size(), CV_32F);
        cv::Mat grad = cv::Mat::zeros(frames[i].size(), CV_32F);
        for(int y = 0; y < weights.rows; ++y) {
            float w = 0.5f - 0.5f * std::cos(2 * CV_PI * y / weights.rows);

            weights.row(y).setTo(w);
            grad.row(y).setTo(1.0f - w);
        }

    //     // 混合当前帧与全景图
        cv::Mat src_roi = panorama(roi);
        for(int y = 0; y < frames[i].rows; ++y) {
            float* pano = src_roi.ptr<float>(y* frames[i].channels());
            const float* frame = frames[i].ptr<float>(y* frames[i].channels());
            const float* w = weights.ptr<float>(y);
            const float* g = grad.ptr<float>(y);

            for(int x = 0; x < frames[i].cols*frames[i].channels(); ++x) {
                pano[x] = w[x] * frame[x] + g[x] * pano[x];
            }
        }

        // // 无缝缝合处理
        // if(i > 0) {
        //     cv::Rect prev_roi(0, current_y - 10, frames[i].cols, 20);
        //     cv::detail::GraphCutSeamFinder seam_finder;
        //     std::vector<cv::UMat> images, masks;
        //     images.push_back(panorama(prev_roi).getUMat(cv::ACCESS_RW));
        //     images.push_back(frames[i](cv::Rect(0,0,frames[i].cols,10)).getUMat(cv::ACCESS_RW));
        //     masks.push_back(cv::UMat(prev_roi.size(), CV_8U, cv::Scalar(255)));
        //     masks.push_back(cv::UMat(cv::Size(frames[i].cols,10), CV_8U, cv::Scalar(255)));
        //     auto p=cv::Point(0,0);
        //     seam_finder.find(images, {p}, masks);
        // }

        current_y += frames[i].rows;
    }
}

// 后处理增强
cv::Mat postProcessing(cv::Mat& panorama) {
    // 锐化处理
    cv::Mat sharpened;
    cv::GaussianBlur(panorama, sharpened, cv::Size(0,0), 3);
    cv::addWeighted(panorama, 1.5, sharpened, -0.5, 0, sharpened);

    // 消除带状噪声
    cv::Mat denoised;
    // cv::ximgproc::guidedFilter(  // ximgproc not availablesharpened, sharpened, denoised, 10, 0.1);

    // 转换为8位并增强对比度
    cv::Mat result;
    denoised.convertTo(result, CV_8U, 255.0);

    // 边缘增强
    cv::Mat edges;
    cv::Sobel(result, edges, CV_16S, 0, 1);
    cv::convertScaleAbs(edges, edges);
    cv::addWeighted(result, 0.8, edges, 0.2, 0, result);

    return result;
}

/***
 * 拼接列车图片
 * frames--捕捉到的图片片断
 * velocity--列车速度
 * samping_hz--图像帧率
 * ***/
cv::Mat ImageUtil::StitchTrainImage(const std::vector<cv::Mat>& frames, double velocity, int sampling_hz,int exposure_mm){
    if(frames.empty()) return cv::Mat();

    // 1. 预处理：运动补偿去模糊
    std::vector<cv::Mat> processed = motionDeblur(frames, velocity, sampling_hz,exposure_mm);

    // 2. 创建全景图画布
    cv::Mat panorama = cv::Mat::zeros(calculateTotalHeight(frames,velocity,sampling_hz),
                                      frames[0].cols, CV_32FC3);

    // 3. 多频带融合拼接
     multiBandBlend(processed, panorama);
    cv::imshow("panorama",panorama);
    cv::waitKey(0);
    // 4. 后处理增强
    return postProcessing(panorama);
}
QImage ImageUtil::cvMatToQImage(const cv::Mat &mat) {
    if (mat.empty()) {
        return QImage();
    }

    switch (mat.type()) {
    case CV_8UC1: // 灰度图
    {
        QImage image(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step), QImage::Format_Grayscale8);
        return image.copy(); // 深拷贝
    }
    case CV_8UC3: // 三通道BGR
    {
        cv::Mat rgbMat;
        cv::cvtColor(mat, rgbMat, cv::COLOR_BGR2RGB); // 将BGR转换为RGB
        QImage image(rgbMat.data, rgbMat.cols, rgbMat.rows, static_cast<int>(rgbMat.step), QImage::Format_RGB888);
        return image.copy(); // 深拷贝
    }
    case CV_8UC4: // 四通道BGRA
    {
        cv::Mat rgbaMat;
        cv::cvtColor(mat, rgbaMat, cv::COLOR_BGRA2RGBA); // 将BGRA转换为RGBA
        QImage image(rgbaMat.data, rgbaMat.cols, rgbaMat.rows, static_cast<int>(rgbaMat.step), QImage::Format_RGBA8888);
        return image.copy();
    }
    default:
        qWarning() << "cvMatToQImage: Mat type not supported:" << mat.type();
        return QImage();
    }
}
cv::Mat ImageUtil::stichImageInVector(std::vector<FrameRecord>* pVector, quint64 fromTime, quint64 toTime, bool up2down) {
    // 检查输入有效性
    if (!pVector || pVector->empty()) return cv::Mat();

    // 筛选时间区间内的有效帧
    std::vector<FrameRecord> validFrames;
    for (const auto& record : *pVector) {
        if (record.recordTime >= fromTime && record.recordTime <= toTime) {
            validFrames.push_back(record);
        }
    }
    if (validFrames.empty()) return cv::Mat(1,1,CV_8UC1);

    // 按时间排序（确保拼接顺序）
    std::sort(validFrames.begin(), validFrames.end(),
              [](const FrameRecord& a, const FrameRecord& b) {
                  return a.recordTime < b.recordTime;
              });

    // 反转顺序（当需要从下到上拼接时）
    if (!up2down) {
        std::reverse(validFrames.begin(), validFrames.end());
    }

    // 计算拼接后图像总高度
    int totalHeight = 0;
    const int width = validFrames[0].frame.cols;
    for (const auto& record : validFrames) {
        totalHeight += record.frame.rows;
    }

    // 创建结果矩阵并拼接
    cv::Mat result(totalHeight, width, validFrames[0].frame.type());
    int currentY = 0;
    for (const auto& record : validFrames) {
        record.frame.copyTo(result(cv::Rect(0, currentY, width, record.frame.rows)));
        currentY += record.frame.rows;
    }

    return result;
}
