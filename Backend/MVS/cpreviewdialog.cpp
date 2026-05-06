#include "cpreviewdialog.h"
#include "clinecameramanager.h"
#include "csystemconfig.h"
#include<QException>
#include "logger.h"
CPreviewDialog::CPreviewDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::CPreviewDialog)
{
    ui->setupUi(this);
    connect(ui->btnStart, &QPushButton::clicked, this, &CPreviewDialog::onStart);
    connect(ui->btnStop,  &QPushButton::clicked, this, &CPreviewDialog::onStop);
    connect(&m_timer, &QTimer::timeout, this, &CPreviewDialog::grabFrame);

    ui->btnStop->setEnabled(false);

}

CPreviewDialog::~CPreviewDialog()
{
    stopPreview();
    delete ui;
}
void CPreviewDialog::setCameraList(const QStringList &ips)
{
    ui->comboCamera->clear();
    ui->comboCamera->addItems(ips);
    if (!ips.isEmpty())
        ui->comboCamera->setCurrentIndex(0);
}
// void CPreviewDialog::setIpMap(QMap<QString,unsigned int> ipMap){
//     this->m_ipMap=ipMap;
// }
void CPreviewDialog::onStart()
{
    QString ipStr = ui->comboCamera->currentText();
    if (ipStr.isEmpty())
        return;

    bool ok = false;
    // unsigned int ip = m_ipMap.value(ipStr, 0);           // 用你已有的 m_ipMap
    // if (ip == 0) return;

    stopPreview();   // 先关闭旧相机

    // 打开新相机并启动取流
    if (CLineCameraManager::instance()->OpenCamera(ipStr))
    {
        CLineCameraManager::instance()->StartCamera(ipStr);//开始拍摄
        m_currentCamera = ipStr;
        //使用配置的频率刷新
        m_timer.start(30);//CSystemConfig::instance()->m_camera_line_sampling_hz[CSystemConfig::instance()->GetCameraName(ipStr)]);;                 // 30ms 刷新
        ui->btnStart->setEnabled(false);
        ui->btnStop->setEnabled(true);
    }
}

void CPreviewDialog::onStop()
{
    stopPreview();
}

void CPreviewDialog::grabFrame()
{
    try{
    if (m_currentCamera.isEmpty()) return;

    QImage frame = CLineCameraManager::instance()->GetLatestImage(m_currentCamera);
    if (!frame.isNull())
    {
        // 按 label 大小等比缩放
        QPixmap pix = QPixmap::fromImage(frame);
        ui->labelImage->setPixmap(pix.scaled(ui->labelImage->size(),
                                             Qt::KeepAspectRatio,
                                             Qt::SmoothTransformation));
    }
    }catch(QException e){
        LOG(QString(e.what()));

    }
}

void CPreviewDialog::stopPreview()
{
    if (!m_currentCamera.isEmpty())
    {
        m_timer.stop();
        CLineCameraManager::instance()->CloseCamera(m_currentCamera);
        m_currentCamera = "";
    }
    ui->btnStart->setEnabled(true);
    ui->btnStop->setEnabled(false);
}

void CPreviewDialog::closeEvent(QCloseEvent *event)
{
    stopPreview();
    QDialog::closeEvent(event);
}
