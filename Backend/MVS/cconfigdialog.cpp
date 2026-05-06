#include "cconfigdialog.h"
#include "ui_cconfigdialog.h"
#include "MVS/MvCamera.h"
#include <QVBoxLayout>
#include <QMessageBox>
#include <QLineEdit>
#include <QRegularExpressionValidator>
#include <QFileDialog>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include "clinecameramanager.h"
#include "cpreviewdialog.h"
#include "logger.h"

CConfigDialog::CConfigDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::CConfigDialog)
{
    ui->setupUi(this);
    // 不允许比设计时更小
    // setMinimumSize(sizeHint());
    initUI();
}
void CConfigDialog::initUI()
{

    QLineEdit *ipEdit=ui->lineEditPLCAddress;
    // 严格 IPv4，每一段 0-255，共 4 段
    QRegularExpression re(R"(^((25[0-5]|2[0-4]\d|[01]?\d\d?)\.){3}(25[0-5]|2[0-4]\d|[01]?\d\d?)$)");
    QRegularExpressionValidator *val = new QRegularExpressionValidator(re, ipEdit);
    ipEdit->setValidator(val);
    // ipEdit->setInputMask("000.000.000.000;_");   // 可选：占位符 “___”
    // ipEdit->setPlaceholderText("192.168.0.1");
    // ipEdit->show();

    memset(&m_stDevList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));

    // 初始禁用控件
    enableControls(false);

    // 枚举设备
    onBtnEnumClicked();

    setConfiguration(*CSystemConfig::instance());
}

CConfigDialog::~CConfigDialog()
{
    delete ui;
}
// void CConfigDialog::onBtnEnumClicked()
// {
//     // 清空下拉框
//     ui->comboLineCameraLeft->clear();
//     ui->comboLineCameraRight->clear();
//     ui->comboLineCameraTop->clear();
//     ui->comboLineCameraLeft->addItem("无");
//     ui->comboLineCameraRight->addItem("无");
//     ui->comboLineCameraTop->addItem("无");
//     memset(&m_stDevList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));

//     int nRet = CMvCamera::EnumDevices(
//         MV_GIGE_DEVICE | MV_USB_DEVICE |
//             MV_GENTL_GIGE_DEVICE | MV_GENTL_CAMERALINK_DEVICE |
//             MV_GENTL_CXP_DEVICE | MV_GENTL_XOF_DEVICE,
//         &m_stDevList);
//     if (MV_OK != nRet)
//         return;

//     for (unsigned int i = 0; i < m_stDevList.nDeviceNum; ++i)
//     {
//         MV_CC_DEVICE_INFO *pDeviceInfo = m_stDevList.pDeviceInfo[i];
//         if (!pDeviceInfo)
//             continue;

//         QString strMsg;
//         QString strUserName;

//         if (pDeviceInfo->nTLayerType == MV_GIGE_DEVICE)
//         {
//             const MV_GIGE_DEVICE_INFO &gige = pDeviceInfo->SpecialInfo.stGigEInfo;
//             quint32 ip = gige.nCurrentIp;
//             int nIp1 = (ip >> 24) & 0xFF;
//             int nIp2 = (ip >> 16) & 0xFF;
//             int nIp3 = (ip >> 8)  & 0xFF;
//             int nIp4 =  ip        & 0xFF;

//             if (gige.chUserDefinedName[0] != '\0')
//                 strUserName = QString("%1 (%2)").arg(gige.chUserDefinedName).arg(gige.chSerialNumber);
//             else
//                 strUserName = QString("%1 (%2)").arg(gige.chModelName).arg(gige.chSerialNumber);

//             strMsg = QString("[%1]GigE:    %2  (%3.%4.%5.%6)")
//                          .arg(i).arg(strUserName).arg(nIp1).arg(nIp2).arg(nIp3).arg(nIp4);
//             m_ipMap[strMsg]=ip;//暂时仅记录gige设备的IP地址
//         }
//         else if (pDeviceInfo->nTLayerType == MV_USB_DEVICE)
//         {
//             const MV_USB3_DEVICE_INFO &usb = pDeviceInfo->SpecialInfo.stUsb3VInfo;
//             if (usb.chUserDefinedName[0] != '\0')
//                 strUserName = QString("%1 (%2)").arg(usb.chUserDefinedName).arg(usb.chSerialNumber);
//             else
//                 strUserName = QString("%1 (%2)").arg(usb.chModelName).arg(usb.chSerialNumber);

//             strMsg = QString("[%1]UsbV3:  %2").arg(i).arg(strUserName);
//         }
//         else if (pDeviceInfo->nTLayerType == MV_GENTL_CAMERALINK_DEVICE)
//         {
//             const MV_CML_DEVICE_INFO &cml = pDeviceInfo->SpecialInfo.stCMLInfo;
//             if (cml.chUserDefinedName[0] != '\0')
//                 strUserName = QString("%1 (%2)").arg(cml.chUserDefinedName).arg(cml.chSerialNumber);
//             else
//                 strUserName = QString("%1 (%2)").arg(cml.chModelName).arg(cml.chSerialNumber);

//             strMsg = QString("[%1]CML:  %2").arg(i).arg(strUserName);
//         }
//         else if (pDeviceInfo->nTLayerType == MV_GENTL_CXP_DEVICE)
//         {
//             const MV_CXP_DEVICE_INFO &cxp = pDeviceInfo->SpecialInfo.stCXPInfo;
//             if (cxp.chUserDefinedName[0] != '\0')
//                 strUserName = QString("%1 (%2)").arg(cxp.chUserDefinedName).arg(cxp.chSerialNumber);
//             else
//                 strUserName = QString("%1 (%2)").arg(cxp.chModelName).arg(cxp.chSerialNumber);

//             strMsg = QString("[%1]CXP:  %2").arg(i).arg(strUserName);
//         }
//         else if (pDeviceInfo->nTLayerType == MV_GENTL_XOF_DEVICE)
//         {
//             const MV_XOF_DEVICE_INFO &xof = pDeviceInfo->SpecialInfo.stXoFInfo;
//             if (xof.chUserDefinedName[0] != '\0')
//                 strUserName = QString("%1 (%2)").arg(xof.chUserDefinedName).arg(xof.chSerialNumber);
//             else
//                 strUserName = QString("%1 (%2)").arg(xof.chModelName).arg(xof.chSerialNumber);

//             strMsg = QString("[%1]XoF:  %2").arg(i).arg(strUserName);
//         }
//         else
//         {
//             showErrorMsg("Unknown device enumerated");
//             continue;
//         }

//         ui->comboLineCameraLeft->addItem(strMsg);
//         ui->comboLineCameraRight->addItem(strMsg);
//         ui->comboLineCameraTop->addItem(strMsg);
//     }

//     if (m_stDevList.nDeviceNum == 0)
//     {
//         showErrorMsg("没有发现相机，请通过网线或串口连接相机");
//         return;
//     }
//     if(!CSystemConfig::instance()->m_mapCamerasIP["left"].isNull())
//     ui->comboLineCameraLeft->setCurrentText(CSystemConfig::instance()->m_mapCamerasIP["left"]);
//     else
//         ui->comboLineCameraLeft->setCurrentIndex(0);
//     if(!CSystemConfig::instance()->m_mapCamerasIP["right"].isNull())
//         ui->comboLineCameraRight->setCurrentText(CSystemConfig::instance()->m_mapCamerasIP["right"]);
//     else
//         ui->comboLineCameraRight->setCurrentIndex(0);
//     if(!CSystemConfig::instance()->m_mapCamerasIP["top"].isNull())
//         ui->comboLineCameraTop->setCurrentText(CSystemConfig::instance()->m_mapCamerasIP["top"]);
//     else
//         ui->comboLineCameraTop->setCurrentIndex(0);
//     enableControls(true);
// }
void CConfigDialog::onBtnEnumClicked()
{
    // 清空下拉框
    ui->comboLineCameraLeft->clear();
    ui->comboLineCameraRight->clear();
    ui->comboLineCameraTop->clear();
    ui->comboAreaCameraRight->clear();
    ui->comboAreaCameraLeft->clear();
    ui->comboAreaCameraTop->clear();
    ui->comboLineCameraLeft->addItem("无");
    ui->comboLineCameraRight->addItem("无");
    ui->comboLineCameraTop->addItem("无");
    ui->comboAreaCameraTop->addItem("无");
    ui->comboAreaCameraLeft->addItem("无");
    ui->comboAreaCameraRight->addItem("无");
    ui->comboAreaCameraTop->addItem("无");
    memset(&m_stDevList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));

    int nRet = CMvCamera::EnumDevices(
        MV_GIGE_DEVICE | MV_USB_DEVICE |
            MV_GENTL_GIGE_DEVICE | MV_GENTL_CAMERALINK_DEVICE |
            MV_GENTL_CXP_DEVICE | MV_GENTL_XOF_DEVICE,
        &m_stDevList);
    if (MV_OK != nRet)
        return;

    for (unsigned int i = 0; i < m_stDevList.nDeviceNum; ++i)
    {
        MV_CC_DEVICE_INFO *pDeviceInfo = m_stDevList.pDeviceInfo[i];
        if (!pDeviceInfo)
            continue;

        QString strMsg=CLineCameraManager::GetDeviceName(pDeviceInfo);

        // 添加到下拉框
        ui->comboLineCameraLeft->addItem(strMsg);
        ui->comboLineCameraRight->addItem(strMsg);
        ui->comboLineCameraTop->addItem(strMsg);
        ui->comboAreaCameraRight->addItem(strMsg);
        ui->comboAreaCameraLeft->addItem(strMsg);
        ui->comboAreaCameraTop->addItem(strMsg);
    }

    if (m_stDevList.nDeviceNum == 0)
    {
        showErrorMsg("没有发现相机，请通过网线或串口连接相机");
        return;
    }

    // 设置当前选中项（需确保配置类已适配新格式）
    auto setCurrent = [](QComboBox* combo, const QString& configValue) {
        if (!configValue.isNull()) {
            int idx = combo->findText(configValue);
            if (idx >= 0) combo->setCurrentIndex(idx);
        }
    };

    setCurrent(ui->comboLineCameraLeft,  CSystemConfig::instance()->m_mapCamerasIP["left"]);
    setCurrent(ui->comboLineCameraRight, CSystemConfig::instance()->m_mapCamerasIP["right"]);
    setCurrent(ui->comboLineCameraTop,   CSystemConfig::instance()->m_mapCamerasIP["top"]);
    setCurrent(ui->comboAreaCameraTop,   CSystemConfig::instance()->m_mapAreaCamerasIP["area_top"]);
    setCurrent(ui->comboAreaCameraRight,   CSystemConfig::instance()->m_mapAreaCamerasIP["area_right"]);
    setCurrent(ui->comboAreaCameraLeft,   CSystemConfig::instance()->m_mapAreaCamerasIP["area_left"]);
    enableControls(true);
}
void CConfigDialog::enableControls(bool enable)
{
    // 这里把需要一次性启/禁用的控件全部 setEnabled(enable);
    ui->comboLineCameraLeft->setEnabled(enable);
    ui->comboLineCameraRight->setEnabled(enable);
    ui->comboLineCameraTop->setEnabled(enable);
    ui->comboAreaCameraRight->setEnabled(enable);
    ui->comboAreaCameraLeft->setEnabled(enable);
    ui->comboAreaCameraTop->setEnabled(enable);
}

void CConfigDialog::showErrorMsg(const QString &msg)
{
    QMessageBox::critical(this, "Error", msg);
}
void CConfigDialog::accept()
{
    *CSystemConfig::instance()=getConfiguration();
    CSystemConfig::instance()->Save();
    QDialog::accept();
}

//从硬件获取参数
void CConfigDialog::getParameters(){

    // 左线阵相机
    int leftIndex = ui->comboLineCameraLeft->currentIndex();
    if (leftIndex >0 && leftIndex < ui->comboLineCameraLeft->count()) {
        QString leftText = ui->comboLineCameraLeft->currentText();
            LineCameraParameter param =
                CLineCameraManager::instance()->GetCameraPrams(leftText);
            ui->spinBoxLineRateLeft->setValue(static_cast<int>(param.lineRate));
            ui->spinBoxImageHeightLeft->setValue(static_cast<int>(param.imageHeight));
            ui->spinBoxImageWidthLeft->setValue(static_cast<int>(param.imageWidth));
            ui->spinBoxExposureLeft->setValue(static_cast<double>(param.ExposureTime));
    }

    // 右线阵相机
    int rightIndex = ui->comboLineCameraRight->currentIndex();
    if (rightIndex >0 && rightIndex < ui->comboLineCameraRight->count()) {
        QString rightText = ui->comboLineCameraRight->currentText();
            LineCameraParameter param =
                CLineCameraManager::instance()->GetCameraPrams(rightText);
            ui->spinBoxLineRateRight->setValue(static_cast<int>(param.lineRate));
            ui->spinBoxImageHeightRight->setValue(static_cast<int>(param.imageHeight));
            ui->spinBoxImageWidthRight->setValue(static_cast<int>(param.imageWidth));
            ui->spinBoxExposureRight->setValue(static_cast<double>(param.ExposureTime));
    }

    // 顶线阵相机
    int topIndex = ui->comboLineCameraTop->currentIndex();
    if (topIndex >0 && topIndex < ui->comboLineCameraTop->count()) {
        QString topText = ui->comboLineCameraTop->currentText();
            LineCameraParameter param =
                CLineCameraManager::instance()->GetCameraPrams(topText);
            ui->spinBoxLineRateTop->setValue(static_cast<int>(param.lineRate));
            ui->spinBoxImageHeightTop->setValue(static_cast<int>(param.imageHeight));
            ui->spinBoxImageWidthTop->setValue(static_cast<int>(param.imageWidth));
            ui->spinBoxExposureTop->setValue(static_cast<double>(param.ExposureTime));
    }


    // 左面阵相机
    int leftAreaIndex = ui->comboAreaCameraLeft->currentIndex();
    if (leftAreaIndex >0 && leftAreaIndex < ui->comboAreaCameraLeft->count()) {
        QString leftText = ui->comboAreaCameraLeft->currentText();
        LineCameraParameter param =
            CLineCameraManager::instance()->GetCameraPrams(leftText);
        ui->spinBoxFrameRateLeft->setValue(static_cast<int>(param.lineRate));
        ui->spinBoxImageHeightLeft_Area->setValue(static_cast<int>(param.imageHeight));
        ui->spinBoxImageWidthLeft_Area->setValue(static_cast<int>(param.imageWidth));
        // ui->doubleSpinBoxImageCompensateLeft->setValue(param.imageCompensate);
        // ui->spinBoxExposureLeft->setValue(static_cast<double>(param.ExposureTime));
    }

    // 右线阵相机
    int rightAreaIndex = ui->comboLineCameraRight->currentIndex();
    if (rightAreaIndex >0 && rightAreaIndex < ui->comboLineCameraRight->count()) {
        QString rightText = ui->comboLineCameraRight->currentText();
        LineCameraParameter param =
            CLineCameraManager::instance()->GetCameraPrams(rightText);
        ui->spinBoxFrameRateRight->setValue(static_cast<int>(param.lineRate));
        ui->spinBoxImageHeightRight_Area->setValue(static_cast<int>(param.imageHeight));
        ui->spinBoxImageWidthRight_Area->setValue(static_cast<int>(param.imageWidth));
        // ui->doubleSpinBoxImageCompensateRight->setValue(param.imageCompensate);
        // ui->spinBoxExposureRight->setValue(static_cast<double>(param.ExposureTime));
    }

    // 顶线阵相机
    int top_AreaIndex = ui->comboLineCameraTop->currentIndex();
    if (top_AreaIndex >0 && top_AreaIndex < ui->comboLineCameraTop->count()) {
        QString topText = ui->comboLineCameraTop->currentText();
        LineCameraParameter param =
            CLineCameraManager::instance()->GetCameraPrams(topText);
        ui->spinBoxFrameRateTop->setValue(static_cast<int>(param.lineRate));
        ui->spinBoxImageHeightTop_Area->setValue(static_cast<int>(param.imageHeight));
        ui->spinBoxImageWidthTop_Area->setValue(static_cast<int>(param.imageWidth));
        // ui->doubleSpinBoxImageCompensateTop->setValue(param.imageCompensate);
        // ui->spinBoxExposureTop->setValue(static_cast<double>(param.ExposureTime));
    }

    // QMessageBox::information(this, tr("提示"), tr("已从硬件读取参数并更新到界面"));
}
//保存参数到硬件
void CConfigDialog::setParameters(){
    *CSystemConfig::instance()=getConfiguration();
    CSystemConfig::instance()->Save();
    // 左线阵相机
    int leftIndex = ui->comboLineCameraLeft->currentIndex();
    if (leftIndex >0 && leftIndex < ui->comboLineCameraLeft->count()) {
        QString leftText = ui->comboLineCameraLeft->currentText();
            LineCameraParameter leftParam;
            leftParam.lineRate = ui->spinBoxLineRateLeft->value();
            leftParam.frameRate = 1; // 未在UI中定义，设为默认值1
            leftParam.imageHeight = ui->spinBoxImageHeightLeft->value();
            leftParam.ExposureTime = static_cast<float>(ui->spinBoxExposureLeft->value());
            CLineCameraManager::instance()->ConfigCameraParams(leftText, leftParam);
    }

    // 右线阵相机
    int rightIndex = ui->comboLineCameraRight->currentIndex();
    if (rightIndex > 0 && rightIndex < ui->comboLineCameraRight->count()) {
        QString rightText = ui->comboLineCameraRight->currentText();
            LineCameraParameter rightParam;
            rightParam.lineRate = ui->spinBoxLineRateRight->value();
            rightParam.frameRate = 1;
            rightParam.imageHeight = ui->spinBoxImageHeightRight->value();
            rightParam.ExposureTime = static_cast<float>(ui->spinBoxExposureRight->value());
            CLineCameraManager::instance()->ConfigCameraParams(rightText, rightParam);
    }

    // 顶线阵相机
    int topIndex = ui->comboLineCameraTop->currentIndex();
    if (topIndex > 0 && topIndex < ui->comboLineCameraTop->count()) {
        QString topText = ui->comboLineCameraTop->currentText();
            LineCameraParameter topParam;
            topParam.lineRate = ui->spinBoxLineRateTop->value();
            topParam.frameRate = 1;
            topParam.imageHeight = ui->spinBoxImageHeightTop->value();
            topParam.ExposureTime = static_cast<float>(ui->spinBoxExposureTop->value());
            CLineCameraManager::instance()->ConfigCameraParams(topText, topParam);
    }

    QMessageBox::information(this, tr("提示"), tr("线阵相机参数设置完成"));
}


void CConfigDialog::setConfiguration(const CSystemConfig& config)
{
    /*---------------- 线阵相机 ----------------*/
    ui->comboLineCameraLeft ->setCurrentText(config.m_mapCamerasIP.value("left"));
    ui->comboLineCameraTop  ->setCurrentText(config.m_mapCamerasIP.value("top"));
    ui->comboLineCameraRight->setCurrentText(config.m_mapCamerasIP.value("right"));

    ui->spinBoxLineRateLeft ->setValue(config.m_camera_line_sampling_hz.value("left"));
    ui->spinBoxLineRateTop  ->setValue(config.m_camera_line_sampling_hz.value("top"));
    ui->spinBoxLineRateRight->setValue(config.m_camera_line_sampling_hz.value("right"));

    ui->spinBoxExposureLeft ->setValue(static_cast<double>(config.m_camera_exporsure_time.value("left")));
    ui->spinBoxExposureTop  ->setValue(static_cast<double>(config.m_camera_exporsure_time.value("top")));
    ui->spinBoxExposureRight->setValue(static_cast<double>(config.m_camera_exporsure_time.value("right")));

    ui->spinBoxImageHeightLeft ->setValue(config.m_camera_image_height.value("left"));
    ui->spinBoxImageHeightTop  ->setValue(config.m_camera_image_height.value("top"));
    ui->spinBoxImageHeightRight->setValue(config.m_camera_image_height.value("right"));

    ui->spinBoxImageWidthLeft ->setValue(config.m_camera_image_width.value("left"));
    ui->spinBoxImageWidthTop  ->setValue(config.m_camera_image_width.value("top"));
    ui->spinBoxImageWidthRight->setValue(config.m_camera_image_width.value("right"));


    ui->spinBoxCaptureDelay->setValue(config.m_camera_capture_delay);

    /*---------------- 面阵相机 ----------------*/
    ui->comboAreaCameraLeft ->setCurrentText(config.m_mapAreaCamerasIP.value("area_left"));
    ui->comboAreaCameraTop  ->setCurrentText(config.m_mapAreaCamerasIP.value("area_top"));
    ui->comboAreaCameraRight->setCurrentText(config.m_mapAreaCamerasIP.value("area_right"));
    ui->spinBoxFrameRateRight->setValue(config.m_camera_line_sampling_hz.value("area_right"));
    ui->spinBoxFrameRateLeft->setValue(config.m_camera_line_sampling_hz.value("area_left"));
    ui->spinBoxFrameRateTop->setValue(config.m_camera_line_sampling_hz.value("area_top"));

    ui->spinBoxImageHeightLeft_Area ->setValue(config.m_camera_image_height.value("area_left"));
    ui->spinBoxImageHeightTop_Area  ->setValue(config.m_camera_image_height.value("area_top"));
    ui->spinBoxImageHeightRight_Area->setValue(config.m_camera_image_height.value("area_right"));

    ui->spinBoxImageWidthLeft_Area ->setValue(config.m_camera_image_width.value("area_left"));
    ui->spinBoxImageWidthTop_Area  ->setValue(config.m_camera_image_width.value("area_top"));
    ui->spinBoxImageWidthRight_Area->setValue(config.m_camera_image_width.value("area_right"));

    /*---------------- 网络 / PLC ----------------*/
    ui->spinListenPort   ->setValue(config.m_nListenPort);
    ui->spinWebServerPort->setValue(config.m_nWebServerPort);
    ui->lineEditPLCAddress->setText(config.m_strPLCAddress);
    // ui->spinListenPort_2 ->setValue(config.m_nPLCPort);

    ui->spinRecPort->setValue(config.m_nRecPort);
    ui->lineEditRecAddress->setText(config.m_strImageRecAddress);

    ui->lineEditDetection->setText(config.m_detectionName);

    /*---------------- 文件保存 ----------------*/
    ui->checkBoxSaveLineImage->setChecked(config.m_saveLineCameraTempFile);
    ui->checkBoxSaveAreaVideo->setChecked(config.m_saveAreaCameraVideoFile);

    ui->lineEditImageSavePath->setText(config.m_strLineCameraGFrabbingFilePath);
    ui->lineEditVideoSavePath->setText(config.m_strAreaCameraVideoFilePath);
    ui->lineEditTrainImagesSavePath->setText(config.m_strTrainImagesSavePath);

    /* 触发槽函数，更新启用状态 */
    SaveLineImageChecked(config.m_saveLineCameraTempFile ? Qt::Checked : Qt::Unchecked);
    SaveAreaVideoChecked(config.m_saveAreaCameraVideoFile ? Qt::Checked : Qt::Unchecked);
}

CSystemConfig CConfigDialog::getConfiguration() const
{
    CSystemConfig cfg;

    /*------------ 线阵相机 ------------*/
    cfg.m_mapCamerasIP["left"]  = ui->comboLineCameraLeft->currentText();
    cfg.m_mapCamerasIP["top"]   = ui->comboLineCameraTop->currentText();
    cfg.m_mapCamerasIP["right"] = ui->comboLineCameraRight->currentText();

    cfg.m_camera_line_sampling_hz["left"]  = ui->spinBoxLineRateLeft->value();
    cfg.m_camera_line_sampling_hz["top"]   = ui->spinBoxLineRateTop->value();
    cfg.m_camera_line_sampling_hz["right"] = ui->spinBoxLineRateRight->value();

    cfg.m_camera_exporsure_time["left"]  = ui->spinBoxExposureLeft->value();
    cfg.m_camera_exporsure_time["top"]   = ui->spinBoxExposureTop->value();
    cfg.m_camera_exporsure_time["right"] = ui->spinBoxExposureRight->value();

    cfg.m_camera_image_height["left"]  = ui->spinBoxImageHeightLeft->value();
    cfg.m_camera_image_height["top"]   = ui->spinBoxImageHeightTop->value();
    cfg.m_camera_image_height["right"] = ui->spinBoxImageHeightRight->value();

    cfg.m_camera_image_width["left"]  = ui->spinBoxImageWidthLeft->value();
    cfg.m_camera_image_width["top"]   = ui->spinBoxImageWidthTop->value();
    cfg.m_camera_image_width["right"] = ui->spinBoxImageWidthRight->value();


    /*------------ 面阵相机 ------------*/
    cfg.m_mapAreaCamerasIP["area_left"]  = ui->comboAreaCameraLeft->currentText();
    cfg.m_mapAreaCamerasIP["area_left"] = ui->comboAreaCameraRight->currentText();
    cfg.m_mapAreaCamerasIP["area_left"]   = ui->comboAreaCameraTop->currentText();
    cfg.m_camera_line_sampling_hz["area_left"]  = ui->spinBoxFrameRateLeft->value();
    cfg.m_camera_line_sampling_hz["area_top"]   = ui->spinBoxFrameRateTop->value();
    cfg.m_camera_line_sampling_hz["area_right"] = ui->spinBoxFrameRateRight->value();

    cfg.m_camera_image_width["area_left"]  = ui->spinBoxImageWidthLeft_Area->value();
    cfg.m_camera_image_width["area_top"]   = ui->spinBoxImageWidthTop_Area->value();
    cfg.m_camera_image_width["area_right"] = ui->spinBoxImageWidthRight_Area->value();

    cfg.m_camera_image_height["area_left"]  = ui->spinBoxImageHeightLeft_Area->value();
    cfg.m_camera_image_height["area_top"]   = ui->spinBoxImageHeightTop_Area->value();
    cfg.m_camera_image_height["area_right"] = ui->spinBoxImageHeightRight_Area->value();

    /*------------ 服务 / 网络 ------------*/
    cfg.m_nListenPort      = static_cast<quint16>(ui->spinListenPort->value());
    cfg.m_nWebServerPort=static_cast<quint16>(ui->spinWebServerPort->value());
    cfg.m_strPLCAddress    = ui->lineEditPLCAddress->text();
    // cfg.m_nPLCPort         = static_cast<quint16>(ui->spinListenPort_2->value());
    cfg.m_camera_capture_delay = ui->spinBoxCaptureDelay->value();
    cfg.m_detectionName=ui->lineEditDetection->text();
    cfg.m_strImageRecAddress=ui->lineEditRecAddress->text();
    cfg.m_nRecPort= static_cast<quint16>(ui->spinRecPort->value());

    /*------------ 文件保存 ------------*/
    cfg.m_strTrainImagesSavePath=ui->lineEditTrainImagesSavePath->text();
    cfg.m_saveLineCameraTempFile  = ui->checkBoxSaveLineImage->isChecked();
    cfg.m_saveAreaCameraVideoFile = ui->checkBoxSaveAreaVideo->isChecked();
    cfg.m_strLineCameraGFrabbingFilePath = ui->lineEditImageSavePath->text();
    cfg.m_strAreaCameraVideoFilePath     = ui->lineEditVideoSavePath->text();

    return cfg;
}
void CConfigDialog::selectImageFolder(){
    QString dir = QFileDialog::getExistingDirectory(
        this,
        tr("选择图像保存位置"),
        QString(),               // 起始目录（空=上一次）
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!dir.isEmpty())
        ui->lineEditImageSavePath->setText(dir);
}
void CConfigDialog::selectVideoFolder(){
    QString dir = QFileDialog::getExistingDirectory(
        this,
        tr("选择视频保存位置"),
        QString(),               // 起始目录（空=上一次）
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!dir.isEmpty())
        ui->lineEditVideoSavePath->setText(dir);
}
void CConfigDialog::SaveAreaVideoChecked(int state){
    if(state==2){
        ui->lineEditVideoSavePath->setEnabled(true);
        ui->pushButtonSelectVideoFolder->setEnabled(true);
    }else{
        ui->lineEditVideoSavePath->setEnabled(false);
        ui->pushButtonSelectVideoFolder->setEnabled(false);
    }
}
void CConfigDialog::SaveLineImageChecked(int state){
    if(state==2){
        ui->lineEditImageSavePath->setEnabled(true);
        ui->pushButtonSelectImageFolder->setEnabled(true);
    }else{
        ui->lineEditImageSavePath->setEnabled(false);
        ui->pushButtonSelectImageFolder->setEnabled(false);
    }
}
void CConfigDialog::camera_select_line(QString text){
    if(text!="无"&&!text.isEmpty()){
        if(CLineCameraManager::instance()->m_lineCameraItems.contains(text))
            CLineCameraManager::instance()->m_lineCameraItems[text]->isLine=true;
    }
    else{
        LOG("选中未知的设备:"+text);
    }
}
void CConfigDialog::camera_select_area(QString text){
    if(text!="无"&&!text.isEmpty()){
        if(CLineCameraManager::instance()->m_lineCameraItems.contains(text))
            CLineCameraManager::instance()->m_lineCameraItems[text]->isLine=false;
    }
    else{
        LOG("选中未知的设备:"+text);
    }
}
void CConfigDialog::on_pushButtonPreview_clicked()
{
    *CSystemConfig::instance()=getConfiguration();
    static CPreviewDialog *dlg = nullptr;
    if (!dlg)
        dlg = new CPreviewDialog(this);

    // 填充相机列表
    QStringList ips;
    for (int i = 0; i < ui->comboLineCameraLeft->count(); ++i)
        ips << ui->comboLineCameraLeft->itemText(i);
    // for (int i = 0; i < ui->comboLineCameraTop->count(); ++i)
    //     ips << ui->comboLineCameraTop->itemText(i);
    // for (int i = 0; i < ui->comboLineCameraRight->count(); ++i)
    //     ips << ui->comboLineCameraRight->itemText(i);

    dlg->setCameraList(ips);
    //配置IP映射
    // dlg->setIpMap(m_ipMap);

    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}
void CConfigDialog::selectTrainImagesSavePath(){
    QString dir = QFileDialog::getExistingDirectory(
        this,
        tr("选择列车记录图像保存位置"),
        QString(),               // 起始目录（空=上一次）
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!dir.isEmpty())
        ui->lineEditTrainImagesSavePath->setText(dir);
}
