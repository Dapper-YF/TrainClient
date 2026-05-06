#include <QDir>
#include "clinecameraitem.h"
//#include "MvCamera.h"
#include "logger.h"
#include "csystemconfig.h"
#include <opencv2/opencv.hpp>
#include "clinecameramanager.h"
#include "Utils/stringutil.h"
#include "MvErrorDefine.h"
void ShowErrorMsg(QString csMessage,unsigned int nErrorNum)
{
    QString errorMsg;
    if (nErrorNum == 0)
    {
        errorMsg=QString("%1").arg(csMessage);
    }
    else
    {
        errorMsg=QString("%1: Error =%2").arg(csMessage).arg(nErrorNum);
    }

    switch(nErrorNum)
    {
    case MV_E_HANDLE:           errorMsg += "Error or invalid handle ";                                         break;
    case MV_E_SUPPORT:          errorMsg += "Not supported function ";                                          break;
    case MV_E_BUFOVER:          errorMsg += "Cache is full ";                                                   break;
    case MV_E_CALLORDER:        errorMsg += "Function calling order error ";                                    break;
    case MV_E_PARAMETER:        errorMsg += "Incorrect parameter ";                                             break;
    case MV_E_RESOURCE:         errorMsg += "Applying resource failed ";                                        break;
    case MV_E_NODATA:           errorMsg += "No data ";                                                         break;
    case MV_E_PRECONDITION:     errorMsg += "Precondition error, or running environment changed ";              break;
    case MV_E_VERSION:          errorMsg += "Version mismatches ";                                              break;
    case MV_E_NOENOUGH_BUF:     errorMsg += "Insufficient memory ";                                             break;
    case MV_E_ABNORMAL_IMAGE:   errorMsg += "Abnormal image, maybe incomplete image because of lost packet ";   break;
    case MV_E_UNKNOW:           errorMsg += "Unknown error ";                                                   break;
    case MV_E_GC_GENERIC:       errorMsg += "General error ";                                                   break;
    case MV_E_GC_ACCESS:        errorMsg += "Node accessing condition error ";                                  break;
    case MV_E_ACCESS_DENIED:	errorMsg += "No permission ";                                                   break;
    case MV_E_BUSY:             errorMsg += "Device is busy, or network disconnected ";                         break;
    case MV_E_NETER:            errorMsg += "Network error ";                                                   break;
    }

    LOG(errorMsg);
}
void equalizeColorYUV(cv::Mat& src, cv::Mat& dst)
{
    CV_Assert(src.type() == CV_8UC3);

    /* 1. 拿到 Y 统计量 */
    cv::Mat ycrcb;
    cv::cvtColor(src, ycrcb, cv::COLOR_BGR2YCrCb);
    std::vector<cv::Mat> ycc;
    cv::split(ycrcb, ycc);               // ycc[0]=Y
    double yMean = cv::mean(ycc[0])[0];  // 0~255

    /* 2. 拿到 S 统计量 */
    cv::Mat hsv;
    cv::cvtColor(src, hsv, cv::COLOR_BGR2HSV);
    std::vector<cv::Mat> ch;
    cv::split(hsv, ch);                  // ch[1]=S
    double satMean = cv::mean(ch[1])[0];

    /* 3. 自适应 gamma & satScale */
    double gamma = 1.0 - 0.3 * std::tanh((150.0 - yMean) / 50.0);
    double satScale = 1.1 + 0.4 * (1.0 - yMean / 255.0) * (1.0 - satMean / 255.0);
    satScale = std::min(satScale, 1.8);  // 封顶防溢出

    /* 4. V 通道 CLAHE */
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
    clahe->apply(ch[2], ch[2]);

    /* 5. 伽马提亮/压暗 V */
    cv::Mat lut(1, 256, CV_8U);
    uchar* p = lut.ptr();
    for (int i = 0; i < 256; ++i)
        p[i] = cv::saturate_cast<uchar>(pow(i / 255.0, gamma) * 255.0);
    cv::LUT(ch[2], lut, ch[2]);

    /* 6. 拉伸 S */
    cv::multiply(ch[1], satScale, ch[1], 1.0, CV_8U);
    ch[1].setTo(255, ch[1] > 255);

    /* 7. 合并回 BGR */
    cv::merge(ch, hsv);
    cv::cvtColor(hsv, dst, cv::COLOR_HSV2BGR);
}
void __stdcall ImageCallbackEx(unsigned char *pData, MV_FRAME_OUT_INFO_EX *pFrameInfo, void* pUser)
{
    // LOG(QString("imageHeight:%1").arg(pFrameInfo->nHeight));
    CLineCameraItem *pItem = (CLineCameraItem*)pUser;
    emit pItem->signalImageGrabed(pData,pFrameInfo);
    if (nullptr == pItem || nullptr == pData || 0 == pFrameInfo->enPixelType)
    {
        return;
    }
    if(pItem->m_convertBuffer==nullptr){
        pItem->m_convertBuffer=new unsigned char[pFrameInfo->nExtendWidth*pFrameInfo->nExtendHeight*4+1024];
    }
    MV_CC_PIXEL_CONVERT_PARAM_EX cvt_param;
    cvt_param.enSrcPixelType=pFrameInfo->enPixelType;
    cvt_param.enDstPixelType=PixelType_Gvsp_RGB8_Packed;
    cvt_param.nHeight=pFrameInfo->nHeight;
    cvt_param.nWidth=pFrameInfo->nWidth;
    cvt_param.nDstBufferSize=pFrameInfo->nWidth * pFrameInfo->nHeight *  4 + 2048;
    cvt_param.pSrcData=pData;
    cvt_param.pDstBuffer=pItem->m_convertBuffer;
    cvt_param.nSrcDataLen=pFrameInfo->nFrameLen;

    //保存图像片段
    if (pFrameInfo) {
        //转换图片格式
        MV_CC_ConvertPixelTypeEx(pItem->m_devHandle,&cvt_param);
        cv::Mat rawImage(pFrameInfo->nHeight, pFrameInfo->nWidth, CV_8UC3, pItem->m_convertBuffer);
        cvtColor(rawImage,rawImage,cv::COLOR_RGB2BGR);
        // 图像预处（根据列车特性优化）
        cv::Mat processed=rawImage;
        equalizeColorYUV(rawImage,processed);
//        cv::equalizeHist(rawImage, processed);  // 增强对比度

        // 存入待拼接队列（线程安全操作）
        // CLineCameraItem * item=(CLineCameraItem*)pUser;
        auto now=QDateTime::currentDateTime();
        FrameRecord record{now.toMSecsSinceEpoch(),processed.clone()};
        pItem->m_pQueue->push_back(record);

        // cv::imshow("test",processed);
        // cv::waitKey(1);
        if(CSystemConfig::instance()->m_saveLineCameraTempFile){
             QString savePath=StringUtil::ensurePathAndGet(CSystemConfig::instance()->m_strLineCameraGFrabbingFilePath,"temp");
            savePath+=QString("%1.jpg").arg(QDateTime::currentDateTime().toMSecsSinceEpoch());
            bool nRet=cv::imwrite(cv::String(savePath.toUtf8()),processed);
             if (nRet)
             {
                 LOG("保存图片成功：" +savePath);
             }
             else
             {
                 LOG("保存图片失败：" + savePath);
             }
        }
    }
}
void __stdcall ImageCallbackAreaEx(unsigned char *pData, MV_FRAME_OUT_INFO_EX *pFrameInfo, void* pUser)
{
    CLineCameraItem *pItem = (CLineCameraItem*)pUser;
    emit pItem->signalImageGrabed(pData,pFrameInfo);
    if (nullptr == pItem || nullptr == pData || 0 == pFrameInfo->enPixelType)
    {
        return;
    }
    //保存图像片段
    if(pItem->m_convertBuffer==nullptr){
        pItem->m_convertBuffer=new unsigned char[pFrameInfo->nExtendWidth*pFrameInfo->nExtendHeight*4+1024];
    }
    MV_CC_PIXEL_CONVERT_PARAM_EX cvt_param;
    cvt_param.enSrcPixelType=pFrameInfo->enPixelType;
    cvt_param.enDstPixelType=PixelType_Gvsp_RGB8_Packed;
    cvt_param.nHeight=pFrameInfo->nHeight;
    cvt_param.nWidth=pFrameInfo->nWidth;
    cvt_param.nDstBufferSize=pFrameInfo->nWidth * pFrameInfo->nHeight *  4 + 2048;
    cvt_param.pSrcData=pData;
    cvt_param.pDstBuffer=pItem->m_convertBuffer;
    cvt_param.nSrcDataLen=pFrameInfo->nFrameLen;
    if (pFrameInfo) {
        MV_CC_ConvertPixelTypeEx(pItem->m_devHandle,&cvt_param);
        cv::Mat rawImage(pFrameInfo->nHeight, pFrameInfo->nWidth, CV_8UC3, pItem->m_convertBuffer);
        cvtColor(rawImage,rawImage,cv::COLOR_RGB2BGR);
        // 图像预处（根据列车特性优化）
        cv::Mat processed;
        equalizeColorYUV(rawImage,processed);
        // pItem->m_curWriter->write(processed);
    }

}
CLineCameraItem::CLineCameraItem(QString strName,QString strCamPos):m_convertBuffer(nullptr),m_pQueue(nullptr),
    // : QThread(parent)
    m_strName(strName),m_strPos(strCamPos)
{
    m_devHandle = MV_NULL;
    m_pImageStitching = new CImageStitching(this);

    //采集文件路径
    QByteArray arrayUtf8Path = CSystemConfig::instance()->m_strLineCameraGFrabbingFilePath.toUtf8();

    // 安全版替代方案（立即验证）
    strcpy_s(m_szSaveImagePath, sizeof(m_szSaveImagePath), arrayUtf8Path.constData());
    // strcat_s(m_szSaveImagePath, sizeof(m_szSaveImagePath), "\\left");
    strcat_s(m_szSaveImagePath, sizeof(m_szSaveImagePath), "\\");
    QByteArray ba = strName.toLocal8Bit();
    const char* path = ba.constData();
    strcat_s(m_szSaveImagePath, sizeof(m_szSaveImagePath), path);
//    sprintf_s(m_szSaveImagePath, sizeof(m_szSaveImagePath), "%s\\left", arrayUtf8Path.constData());

    connect(this, &CLineCameraItem::signalSaveImageToFile, this, &CLineCameraItem::SaveImageToFile);
}

CLineCameraItem::~CLineCameraItem()
{
    if (m_devHandle)
    {
        Close();
        // MV_CC_DestroyHandle(m_devHandle);
        // m_devHandle = MV_NULL;
    }

    if (m_pSaveImageBuf)
    {
        delete[] m_pSaveImageBuf;
        m_pSaveImageBuf = nullptr;
        m_nSaveImageBufSize = 0;
    }

    if (m_pImageStitching)
    {
        delete m_pImageStitching;
        m_pImageStitching = nullptr;
    }
    if(m_convertBuffer!=nullptr)
        delete [] m_convertBuffer;
}
int CLineCameraItem::Open(){

    if(m_devHandle!=nullptr){
        return 0;
    }
   return Open(&m_psDeviceInfo);
}
int CLineCameraItem::Open(MV_CC_DEVICE_INFO *psDeviceInfo)
{
    int nRet = CMvCamera::Open(psDeviceInfo, &m_devHandle);
    if (MV_OK != nRet)
    {
        ShowErrorMsg("打开设备失败",nRet);
        return nRet;
    }
    m_psDeviceInfo=*psDeviceInfo;
    //探测网络最佳包大小
    if (MV_GIGE_DEVICE == psDeviceInfo->nTLayerType)
    {
        unsigned int nPacketSize = 0;
        nRet = CMvCamera::GetOptimalPacketSize(m_devHandle, &nPacketSize);
        if (MV_OK == nRet)
        {
            nRet = CMvCamera::SetIntValue(m_devHandle, "GevSCPSPacketSize", nPacketSize);
            if (MV_OK != nRet)
            {
                LOG("设置网络最佳包大小失败:" + QString::number(nRet, 16));
            }
        }
        else
        {
            LOG("获取网络最佳包大小失败：" + QString::number(nRet, 16));
        }
    }

    //开始采集
    /*nRet = CMvCamera::StartGrabbing(m_devHandle);
    if (MV_OK != nRet)
    {
        LOG("线阵相机开始采集失败：" + QString::number(nRet));
        return nRet;
    }
    m_bStartGrabbing = true;

    //开启线程
    StartThread();*/

    return MV_OK;
}

int CLineCameraItem::Close()
{
    //结束线程
    // StopThread();

    //停止采集
    if (m_bStartGrabbing)
    {
        int nRet = CMvCamera::StopGrabbing(m_devHandle);
        if (MV_OK != nRet)
        {
            LOG(m_strName + "停止采集失败！" + QString::number(nRet, 16));
            return nRet;
        }
        else
        {
            LOG(m_strName + "停止采集！");
        }
        m_bStartGrabbing = false;
    }
    int nRet=CMvCamera::Close(&m_devHandle);
    if(nRet==0){
        m_devHandle=nullptr;
    }
    return nRet;
}

int CLineCameraItem::StartGrabbing()
{
    if(m_pQueue==nullptr){
        m_pQueue=new std::vector<FrameRecord>();
    }/*else{
        m_pQueue->clear();
    }*/
    if(isLine){

        int nResult = CMvCamera::RegisterImageCallBack(m_devHandle, ImageCallbackEx, this);
        if (MV_OK != nResult)
        {
            ShowErrorMsg("注册图片反馈函数失败：",nResult);
            // LOG("注册图片反馈函数失败：" + QString::number(nResult, 16));
            return nResult;
        }
    }else{
        int nResult = CMvCamera::RegisterImageCallBack(m_devHandle, ImageCallbackAreaEx, this);
        if (MV_OK != nResult)
        {
            ShowErrorMsg("注册视频处理函数失败：",nResult);
            // LOG("注册视频处理函数失败：" + QString::number(nResult, 16));
            return nResult;
        }
        // m_currentVideoPath=StringUtil::ensurePathAndGet(
        //     CSystemConfig::instance()->m_strAreaCameraVideoFilePath,m_strPos);
        // m_currentVideoPath+=QString("%1.mp4").arg(QDateTime::currentDateTime().toMSecsSinceEpoch());
        // int fourcc = //cv::VideoWriter::fourcc('M','J','P','G'); // Motion-JPEG编码
        // // 其他常用编码器:
        // // VideoWriter::fourcc('X','2','6','4'); // H.264
        // cv::VideoWriter::fourcc('M','P','4','V'); // MPEG-4
        // // VideoWriter::fourcc('X','V','I','D'); // XVID
        // m_curWriter=new cv::VideoWriter(cv::String(m_currentVideoPath.toUtf8()),
        //                                   fourcc,  (double)CSystemConfig::instance()->m_camera_line_sampling_hz[m_strPos],
        //                                       cv::Size(CSystemConfig::instance()->m_camera_image_width[m_strPos]
        //                                            ,CSystemConfig::instance()->m_camera_image_height[m_strPos])
        //                                   );
    }
    //开始采集
    int nRet = CMvCamera::StartGrabbing(m_devHandle);
    if (MV_OK != nRet)
    {
        ShowErrorMsg(m_strPos + "开始采集失败：",nRet);
        // LOG(m_strName + "开始采集失败：" + QString::number(nRet, 16));
        return nRet;
    }
    else
    {
        LOG(m_strName + "开始采集");
    }
    m_bStartGrabbing = true;

    //开启线程
    //StartThread();

    return MV_OK;
}



int CLineCameraItem::StopGrabbing()
{
    //结束线程
    //StopThread();
    //图像拼接
    // if (m_pImageStitching)
    // {
    //     //拼接并保存图像
    //     m_pImageStitching->StitchingImages(this->m_pQueue,this->m_currentSaveDir+".jpg");
    //     //保存数据，待完成………………
    // }

    //停止回调
    // int nResult = CMvCamera::RegisterImageCallBack(m_devHandle, nullptr, nullptr);
    // if (MV_OK != nResult)
    // {
    //     LOG("注销图片反馈函数失败：" + QString::number(nResult, 16));
    //     //return nResult;
    // }

    //停止采集
    if (m_bStartGrabbing)
    {
        int nRet = CMvCamera::StopGrabbing(m_devHandle);
        if (MV_OK != nRet)
        {
            ShowErrorMsg(m_strName + "停止采集失败！",nRet);
            // LOG(m_strName + "停止采集失败！" + QString::number(nRet, 16));
            return nRet;
        }
        else
        {
            LOG(m_strName + "停止采集");
        }
        // if(this->isLine&&m_curWriter!=nullptr){
        //     if(m_curWriter->isOpened()){
        //         m_curWriter->release();
        //         m_curWriter=nullptr;
        //     }
        // }
        m_bStartGrabbing = false;
    }


    return MV_OK;
}

// void CLineCameraItem::StartThread()
// {
//     if (!isRunning())
//     {
//         start();
//     }
// }

// void CLineCameraItem::StopThread()
// {
//     if (isRunning())
//     {
//         requestInterruption();
//         wait(1000);
//     }
// }

int CLineCameraItem::SaveImageToFile()
{
    //创建文件目录
    // auto path=MakeFilePath();
    // if (path.isNull()||path.isEmpty())
    // {
    //     LOG("创建文件目录失败：" + QString(m_szSaveImagePath));
    //     return MV_E_NODATA;
    // }

    MV_CC_IMAGE stImage;
    memset(&stImage, 0, sizeof(MV_CC_IMAGE));
    MV_CC_SAVE_IMAGE_PARAM  stSaveImageParam;
    memset(&stSaveImageParam, 0, sizeof(MV_CC_SAVE_IMAGE_PARAM));

    QMutexLocker locker(&m_mutexSaveImage);
    if (nullptr == m_pSaveImageBuf || 0 == m_stImageInfo.enPixelType)
    {
        return MV_E_NODATA;
    }

    stImage.nWidth = m_stImageInfo.nExtendWidth;
    stImage.nHeight = m_stImageInfo.nExtendHeight;
    stImage.enPixelType = m_stImageInfo.enPixelType;
    stImage.pImageBuf = m_pSaveImageBuf;
    stImage.nImageLen = m_stImageInfo.nFrameLenEx;

    stSaveImageParam.enImageType = MV_Image_Jpeg;
    stSaveImageParam.iMethodValue = 1;
    stSaveImageParam.nQuality = 99;
    // qint64 nSDateTime = QDateTime::currentDateTime().toMSecsSinceEpoch();
    // 使用QDir构建路径，确保路径分隔符正确
    QString filePath =StringUtil::ensurePathAndGet(CSystemConfig::instance()->m_strLineCameraGFrabbingFilePath,m_strName);
    //     QDir( m_currentSaveDir).absoluteFilePath(
    //     QString("%1.%2").arg(nSDateTime).arg(it.value())
    //     );

    // // 确保目录存在
    // QDir().mkpath(m_currentSaveDir);

    // 转换为所需的字符编码
    QByteArray utf8Path = filePath.toUtf8();
    const char* szImagePath = utf8Path.constData();

    // 保存图像
    int nRet = CMvCamera::SaveImageToFile(m_devHandle, &stImage, &stSaveImageParam, szImagePath);
    if (MV_OK == nRet) {
        LOG(QString("保存图片成功：%1").arg(filePath));
    } else {
        LOG(QString("保存图片失败：%1 | 错误代码：0x%2")
                .arg(filePath)
                .arg(QString::number(nRet, 16).toUpper()));
    }

    return nRet;
}

QString CLineCameraItem::MakeFilePath(QString rootPath)
{
    // 1. 确保根目录存在
    QDir rootDir;
    if(rootPath.isEmpty()||rootPath.isNull()){
        rootDir=QDir(m_szSaveImagePath);
    }else{
        rootDir=QDir(rootPath);
    }
    if (!rootDir.exists()) {
        if (!rootDir.mkpath(".")) {
            qWarning() << "Failed to create root directory:" << m_szSaveImagePath;
            return QString();
        }
    }

    // 2. 创建当前日期目录 (格式: yyyyMMdd)
    QString dateDir = QDate::currentDate().toString("yyyyMMdd");
    QString fullDatePath = rootDir.filePath(dateDir);

    QDir datePath(fullDatePath);
    if (!datePath.exists()) {
        if (!datePath.mkpath(".")) {
            qWarning() << "Failed to create date directory:" << fullDatePath;
            return QString();
        }
    }

    // 3. 查找最大序数并创建新序数目录
    int maxIndex = 0;
    QStringList existingDirs = datePath.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QString &dir : existingDirs) {
        bool ok;
        int num = dir.toInt(&ok);
        if (ok && num > maxIndex) maxIndex = num;
    }

    QString newDirName = QString::number(maxIndex + 1);
    QString fullPath = datePath.filePath(newDirName);

    if (!QDir().mkpath(fullPath)) {
        qWarning() << "Failed to create sequence directory:" << fullPath;
        return QString();
    }

    // 返回完整路径 (格式: /root/20250828/5/)
    LOG( "Created new directory:" + fullPath);
    m_currentSaveDir=fullDatePath+"/";
    return fullPath + "/";  // 确保以斜杠结尾
}
LineCameraParameter CLineCameraItem::getParameter(){
    LineCameraParameter params;
    MVCC_INTVALUE_EX stIntValue = { 0,66535,0,0,{0} };
    int nRet = GetIntValue("Height", &stIntValue);
    if (MV_OK != nRet)
    {
        ShowErrorMsg(m_strName + "获取图像高度失败：",nRet);
        // LOG("获取图像高度失败：" + QString::number(nRet, 16));
        // return params;
    }
    params.imageHeight=stIntValue.nCurValue;
    nRet = GetIntValue("Width", &stIntValue);
    if (MV_OK != nRet)
    {
        ShowErrorMsg(m_strName + "获取图像宽度失败：",nRet);
        // LOG("获取图像高度失败：" + QString::number(nRet, 16));
        // return params;
    }
    params.imageWidth=stIntValue.nCurValue;
    // ch:获取行频  | en:Get Acquisition LineRate
    nRet = GetIntValue("AcquisitionLineRate", &stIntValue);
    if (MV_OK != nRet)
    {
        ShowErrorMsg(m_strName + "获取行频失败：",nRet);
        // LOG("获取行频失败：" + QString::number(nRet, 16));
        // return params;
        int nRet = GetIntValue("AcquisitioFrameRate", &stIntValue);
        if (MV_OK != nRet){
            ShowErrorMsg(m_strName + "获取频率失败：",nRet);
        }
    }
    params.lineRate=stIntValue.nCurValue;
    // ch:获取增益 | en:Get Gain
    MVCC_FLOATVALUE stFloatValue = {0,std::numeric_limits<float>::max(),0,{0}};
    nRet =GetFloatValue("DigitalShift", &stFloatValue);
    if (MV_OK != nRet)
    {
        ShowErrorMsg(m_strName + "获取增益失败：",nRet);
        // LOG("获取增益失败：" + QString::number(nRet, 16));
        // return params;
    }
    params.imageCompensate=stFloatValue.fCurValue;
    nRet =GetFloatValue("ExposureTime", &stFloatValue);
    if (MV_OK != nRet)
    {
        ShowErrorMsg(m_strName + "获取曝光时间失败：",nRet);
        // LOG("获取曝光时间失败：" + QString::number(nRet, 16));
        return params;
    }
    params.ExposureTime=stFloatValue.fCurValue;

    return params;
}
void CLineCameraItem::setParameter(LineCameraParameter &params){
    int nRet =setFloatValue("DigitalShift", params.imageCompensate);
    if (MV_OK != nRet)
    {
        LOG("图像增益设置失败");
        // return;
    }
    nRet =setFloatValue("ExposureTime", params.ExposureTime);
    if (MV_OK != nRet)
    {
        LOG("曝光时间设置失败");
        // return;
    }
    nRet =setIntValue("Height", params.imageHeight);
    if (MV_OK != nRet)
    {
        LOG("图像高度设置失败");
        // return;
    }
    nRet =setIntValue("Width", params.imageWidth);
    if (MV_OK != nRet)
    {
        LOG("图像宽度设置失败");
        // return;
    }
    nRet =setIntValue("AcquisitionLineRate", params.lineRate);
    if (MV_OK != nRet)
    {
        LOG("行帧设置失败");
        // return;
    }
    if(params.frameRate>0){
    nRet =setIntValue("AcquisitionFrameRate", params.frameRate);
    if (MV_OK != nRet)
    {
        LOG("帧率设置失败");
        // return;
    }
    }
}
// void CLineCameraItem::run()
// {
//     MV_FRAME_OUT stImageInfo;
//     memset(&stImageInfo, 0, sizeof(MV_FRAME_OUT));
//     //MV_CC_IMAGE stImage;
//     //memset(&stImage, 0, sizeof(MV_CC_IMAGE));
//     int nRet = MV_OK;

//     while (!isInterruptionRequested())
//     {
//         if (!m_bStartGrabbing)
//         {
//             QThread::msleep(10);
//             continue;
//         }

//         //需要根据帧率调整此函数的调用次数
//         nRet = CMvCamera::GetImageBuffer(m_devHandle, &stImageInfo, 1000);
//         if (MV_OK == nRet)
//         {
//             QMutexLocker locker(&m_mutexSaveImage);

//             //采集图片信息
//             //分配图片缓存
//             if (nullptr == m_pSaveImageBuf || stImageInfo.stFrameInfo.nFrameLenEx > m_nSaveImageBufSize)
//             {
//                 if (m_pSaveImageBuf)
//                 {
//                     delete[] m_pSaveImageBuf;
//                     m_pSaveImageBuf = nullptr;
//                 }

//                 m_pSaveImageBuf = new unsigned char[stImageInfo.stFrameInfo.nFrameLenEx];
//                 if (nullptr == m_pSaveImageBuf)
//                 {
//                     LOG("系统内存不够！");
//                     CMvCamera::FreeImageBuffer(m_devHandle, &stImageInfo);
//                     break;
//                 }
//                 m_nSaveImageBufSize = stImageInfo.stFrameInfo.nFrameLenEx;
//             }

//             memcpy(m_pSaveImageBuf, stImageInfo.pBufAddr, stImageInfo.stFrameInfo.nFrameLenEx);
//             memcpy(&m_stImageInfo, &(stImageInfo.stFrameInfo), sizeof(MV_FRAME_OUT_INFO_EX));

//             CMvCamera::FreeImageBuffer(m_devHandle, &stImageInfo);
//             // emit signalSaveImageToFile();//由于回调函数中有存储文件过程，此处应去除

//             //根据帧率调整
//             QThread::msleep(33);
//         }//if (MV_OK == nRet)
//         else
//         {
//             LOG("主动读取图像帧失败：" + QString::number(nRet, 16));
//             QThread::msleep(10);
//         }
//     }//while

// }

