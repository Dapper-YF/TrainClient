#include <QHostAddress>
#include "clinecameramanager.h"
#include "MvCamera.h"
#include "logger.h"
#include "csystemconfig.h"
#include "Utils/imageutil.h"
#include "Net/cdataprotocol.h"
#include "Utils/stringutil.h"

CLineCameraManager CLineCameraManager::s_instance;
CLineCameraManager::CLineCameraManager(QObject *parent)
    : QObject(parent)
{

}

CLineCameraManager::~CLineCameraManager()
{
    ClearDevices();
}

CLineCameraManager *CLineCameraManager::instance()
{
    return &s_instance;
}
QString CLineCameraManager::FindCameraItemPos(CLineCameraItem* item){
    for(auto iter=m_lineCameraItems.begin();iter!=m_lineCameraItems.end();iter++){
        if(iter.value().data()==item){
            return iter.key();
        }
    }
    return "";
}
QString CLineCameraManager::GetDeviceName(MV_CC_DEVICE_INFO *pDeviceInfo){
    QString strMsg;

    if (pDeviceInfo->nTLayerType == MV_GIGE_DEVICE)
    {
        const MV_GIGE_DEVICE_INFO &gige = pDeviceInfo->SpecialInfo.stGigEInfo;
        // unsigned int ip = pDeviceInfo->SpecialInfo.stGigEInfo.nCurrentIp;
        // 直接构造IP地址字符串
        strMsg = QString("%1.%2.%3.%4")
                     .arg((gige.nCurrentIp >> 24) & 0xFF)
                     .arg((gige.nCurrentIp >> 16) & 0xFF)
                     .arg((gige.nCurrentIp >> 8)  & 0xFF)
                     .arg( gige.nCurrentIp        & 0xFF);
        // 记录IP映射
    }
    else if (pDeviceInfo->nTLayerType == MV_USB_DEVICE)
    {
        strMsg = QString::fromUtf8(
            reinterpret_cast<const char*>(pDeviceInfo->SpecialInfo.stUsb3VInfo.chSerialNumber)
            );
    }
    else if (pDeviceInfo->nTLayerType == MV_GENTL_CAMERALINK_DEVICE)
    {
        strMsg = QString::fromUtf8(
            reinterpret_cast<const char*>(pDeviceInfo->SpecialInfo.stCMLInfo.chSerialNumber)
            );

    }
    else if (pDeviceInfo->nTLayerType == MV_GENTL_CXP_DEVICE)
    {
        strMsg = QString::fromUtf8(
            reinterpret_cast<const char*>(pDeviceInfo->SpecialInfo.stCXPInfo.chSerialNumber)
            );
    }
    else if (pDeviceInfo->nTLayerType == MV_GENTL_XOF_DEVICE)
    {
        strMsg = QString::fromUtf8(
            reinterpret_cast<const char*>(pDeviceInfo->SpecialInfo.stXoFInfo.chSerialNumber)
            );
    }
    else
    {
        return "";
    }
    return strMsg;
}
void CLineCameraManager::ClearDevices()
{
    QMap<QString, QSharedPointer<CLineCameraItem>>::iterator itor;
    for (itor = m_lineCameraItems.begin(); itor != m_lineCameraItems.end(); ++itor)
    {
        itor.value()->Close();
    }
    m_lineCameraItems.clear();
}

void CLineCameraManager::StartCameras()
{
    if(!inited)
        EnumDevices();
    QMap<QString, QSharedPointer<CLineCameraItem>>::iterator itor;
    for (itor = m_lineCameraItems.begin(); itor != m_lineCameraItems.end(); ++itor)
    {
        itor.value()->Open();
        itor.value()->StartGrabbing();
    }
}

void CLineCameraManager::StopCameras()
{
    //ClearItems();
    QMap<QString, QSharedPointer<CLineCameraItem>>::iterator itor;
    for (itor = m_lineCameraItems.begin(); itor != m_lineCameraItems.end(); ++itor)
    {
        itor.value()->StopGrabbing();
    }
}
void CLineCameraManager::StartCamera(QString name){
    m_lineCameraItems[name]->StartGrabbing();
}
bool CLineCameraManager::OpenCamera(QString name){
    return m_lineCameraItems[name]->Open()==MV_OK;
}
QImage CLineCameraManager::GetLatestImage(QString name){
//待完成
    if( m_lineCameraItems[name]->m_pQueue->size()>0){
    return ImageUtil::cvMatToQImage(m_lineCameraItems[name]->m_pQueue->back().frame);
    }
    QImage image(QSize(1,1),QImage::Format_ARGB32);
    return image;
}
void CLineCameraManager::ConfigCameraParams(QString name,LineCameraParameter param){
    m_lineCameraItems[name]->setParameter(param);
}
LineCameraParameter CLineCameraManager::GetCameraPrams(QString name){
    return m_lineCameraItems[name]->getParameter();
}
void CLineCameraManager::OpenDevice(QString name){
    m_lineCameraItems[name]->Open();
}
void CLineCameraManager::OpenDevice(MV_CC_DEVICE_INFO *pDeviceInfo){
    if (nullptr == pDeviceInfo)
    {
        return;
    }
    // unsigned int ip = pDeviceInfo->SpecialInfo.stGigEInfo.nCurrentIp;
    // QString strIp = LongToIP4String(ip);
    QString strName = GetDeviceName(pDeviceInfo);
     QString camName= CSystemConfig::instance()->GetCameraName(strName);
    CLineCameraItem *pItem = new CLineCameraItem(strName,camName);
    int nRet = pItem->Open(pDeviceInfo);
    if (MV_OK != nRet)
    {
        delete pItem;
        LOG(strName + "打开失败:" + QString::number(nRet, 16));
        return;
    }
    else
    {
        LOG(strName + "打开成功");
    }

    m_lineCameraItems[strName] = QSharedPointer<CLineCameraItem>(pItem);
}
int CLineCameraManager::EnumDevices()
{
    ClearDevices();

    MV_CC_DEVICE_INFO_LIST stDevList;
    memset(&stDevList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));

    //枚举子网内所有设备
    int nRet = CMvCamera::EnumDevices(        MV_GIGE_DEVICE | MV_USB_DEVICE |
                                          MV_GENTL_GIGE_DEVICE | MV_GENTL_CAMERALINK_DEVICE |
                                          MV_GENTL_CXP_DEVICE | MV_GENTL_XOF_DEVICE, &stDevList);
    if (MV_OK != nRet)
    {
        LOG("枚举相机设备失败：" + QString::number(nRet, 16));
        return nRet;
    }

    //打开所有设备并添加相应的操作类
    for (unsigned int i = 0; i < stDevList.nDeviceNum; ++i)
    {
        MV_CC_DEVICE_INFO *pDeviceInfo = stDevList.pDeviceInfo[i];
        QString strName=GetDeviceName(pDeviceInfo);
        QString camName= CSystemConfig::instance()->GetCameraName(strName);
        CLineCameraItem *pItem = new CLineCameraItem(strName,camName);
        pItem->SetDeviceInfo(pDeviceInfo);

        // nRet = pItem->Open(pDeviceInfo);
        // if (MV_OK != nRet)
        // {
        //     delete pItem;
        //     LOG(strName + "打开失败:" + QString::number(nRet, 16));
        //     continue;
        // }
        // else
        // {
        //     LOG(strName + "打开成功");
        // }

        m_lineCameraItems[strName] = QSharedPointer<CLineCameraItem>(pItem);
    }
    if(!inited)
        inited=true;
    return nRet;
}
// QString ensurePathAndGet(const QString& savePath, const QString& subPath) {
//     // 获取当前日期（格式：yyyy-MM-dd）
//     const QString currentDate = QDate::currentDate().toString("yyyy-MM-dd");

//     // 构建完整路径：savePath/currentDate/subPath
//     QString fullPath = savePath;

//     // 规范化路径分隔符（适配不同操作系统）
//     if (!fullPath.endsWith('/') && !fullPath.endsWith('\\')) {
//         fullPath += '/';
//     }
//     fullPath +=  currentDate +'/' + subPath + '/';

//     // 创建QDir对象并确保路径存在
//     QDir dir;
//     if (!dir.exists(fullPath)) {
//         // mkpath会自动创建所有不存在的父级目录
//         if (!dir.mkpath(fullPath)) {
//             // 创建失败时返回空字符串（可改为抛异常或日志记录）
//             return QString();
//         }
//     }

//     return fullPath;
// }
///
/// \brief CLineCameraManager::stichImages 拼接图像
/// \param startTime    开始时间
/// \param endTime  结束时间
/// \param beForward 拼接顺序，从前到后还是从后到前
/// \return 各相机名（left\right\top）与对应的完整路径文件名
///
QMap<QString,QString> CLineCameraManager::stichImages(quint64 startTime,quint64 endTime){
    QMap<QString,QString> nameMap;
    //拼接机车图像
    for(const auto &item:CLineCameraManager::instance()->m_lineCameraItems)
    {
        cv::Mat locoImage=ImageUtil::stichImageInVector(item->m_pQueue,
                                                          startTime,
                                                          endTime,
                                                          m_currentTrain.strDirection=="正向"
                                                          );
        cv::rotate(locoImage,locoImage,cv::ROTATE_90_CLOCKWISE);
        if(locoImage.cols<=1){
            LOG("图像拼接失败");
            return nameMap;
        }
        QString savePath=StringUtil::ensurePathAndGet(
            CSystemConfig::instance()->m_strTrainImagesSavePath,
            item->getName());
        QString fullPath=QDir(savePath).filePath(QString::number(startTime)+"_"+ item->getName()+".jpg");
        // 将QString转换为本地8位字符串（在Windows下是GBK，Linux/macOS下是UTF-8）
        std::string pathStr = fullPath.toLocal8Bit().constData();

        // 保存图像
        bool success = cv::imwrite(pathStr, locoImage);
        if (!success) {
            // 处理保存失败的情况
            LOG("机车文件保存失败："+fullPath,LogLevel::LOG_ERROR);
        }
        nameMap[item->getName()]=fullPath;
    }
    return nameMap;
}
QString CLineCameraManager::LongToIP4String(unsigned int ip)
{
    QHostAddress addr(ip);
    return addr.toString();
}

unsigned int CLineCameraManager::IP4StringToLong(const QString &ipAddress)
{
    QHostAddress addr(ipAddress);
    return addr.toIPv4Address();
}
