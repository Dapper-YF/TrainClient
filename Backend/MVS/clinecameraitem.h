#ifndef CLINECAMERAITEM_H
#define CLINECAMERAITEM_H

#include <QThread>
#include <QMutex>
// #include "MvCamera.h"
#include "cimagestitching.h"
#include <opencv2/opencv.hpp>
#include <cstring>
#include <MVS/MvCamera.h>
#include "../include/define.h"


struct LineCameraParameter{
    int64 lineRate;//行帧
    int64 frameRate;//帧率
    int64 imageHeight;//图像高度
    int64 imageWidth;//图像宽度
    double imageCompensate;//图像增益
    float ExposureTime;//曝光时长
};

//线阵相机操作类
class CLineCameraItem : public QObject //QThread
{
    friend void __stdcall ImageCallbackAreaEx(unsigned char *pData, MV_FRAME_OUT_INFO_EX *pFrameInfo, void* pUser);
    Q_OBJECT
public:
    explicit CLineCameraItem(QString strName,QString strPos);
    virtual ~CLineCameraItem();
    enum CameraType
    {
        CTUNKNOWN = 0,
        CTLEFT,
        CTCENTER,
        CTCENATERLEFT,
        CTCENTERRIGHT,
        CTRIGHT
    };
    //打开及关闭相机
    void SetDeviceInfo(MV_CC_DEVICE_INFO *psDeviceInfo){
        memcpy(&m_psDeviceInfo,psDeviceInfo,sizeof(MV_CC_DEVICE_INFO));
    }
    int Open(MV_CC_DEVICE_INFO *psDeviceInfo);
    int Open();
    int Close();

    //开始或停止采集
    int StartGrabbing();
    int StopGrabbing();

    //开始和停止线程
    // void StartThread();
    // void StopThread();
    //获取名称
    QString getName(){
        return m_strName;
    }
    //获取参数
    LineCameraParameter getParameter();
    //设置参数
    void setParameter(LineCameraParameter &params);
    //创建文件目录
    QString MakeFilePath(QString rootDir="");
    bool isLine{true};
    //当前保存路径
    QString m_currentVideoPath;
    QString m_currentSaveDir;
    void *m_devHandle;
    unsigned char *m_convertBuffer;
    //图片保存格式
    // MV_SAVE_IAMGE_TYPE m_enSaveImageType{MV_Image_Jpeg};
    //文件保存路径
    char m_szSaveImagePath[1024]={0};


    std::vector<FrameRecord>* m_pQueue;

// protected:
//     void run() override;

signals:
    int signalSaveImageToFile();
    void singalStitching(const QString& strFilePath);
    void signalImageGrabed(unsigned char *pData, MV_FRAME_OUT_INFO_EX *pFrameInfo);
private slots:


private:
    QString m_strName; //相机名称
    QString m_strPos;//相机位置
    //是否开始抓图
    bool m_bStartGrabbing{false};

    // cv::VideoWriter *m_curWriter;
    //保存图片
    QMutex              m_mutexSaveImage;
    unsigned char*      m_pSaveImageBuf{nullptr};
    quint64             m_nSaveImageBufSize{0};
    MV_FRAME_OUT_INFO_EX m_stImageInfo;
    MV_CC_DEVICE_INFO m_psDeviceInfo;
    //保存图片文件
    int SaveImageToFile();

    //图片拼接类
    CImageStitching *m_pImageStitching{nullptr};
    int GetIntValue(IN const char* strKey, OUT MVCC_INTVALUE_EX *pIntValue)
    {
        if(m_devHandle==nullptr){
            Open();
        }
        return MV_CC_GetIntValueEx(m_devHandle, strKey, pIntValue);
    }
    int setIntValue(IN const char* strKey,IN int value){
        if(m_devHandle==nullptr){
            Open();
        }
        return MV_CC_SetIntValue(m_devHandle,strKey,value);
    }
    int GetFloatValue(IN const char* strKey, OUT MVCC_FLOATVALUE *pFloatValue)
    {
        if(m_devHandle==nullptr){
            Open();
        }
        if(m_devHandle==nullptr){
            Open();
        }
        return MV_CC_GetFloatValue(m_devHandle, strKey, pFloatValue);
    }
    int setFloatValue(IN const char* strKey,IN float value){
        if(m_devHandle==nullptr){
            Open();
        }
        return MV_CC_SetFloatValue(m_devHandle, strKey, value);
    }
};

#endif // CLINECAMERAITEM_H
