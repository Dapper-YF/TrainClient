#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <opencv2/opencv.hpp>
#include "csystemconfig.h"
#include "Net/ctraintcpserver.h"
#include "logger.h"
#include "MVS/MvCamera.h"
#include "MVS/clinecameramanager.h"
#include "QMessageBox"
// #include "Net/cdataprotocol.h"
#include "Utils/imageutil.h"
#include "MVS/cconfigdialog.h"
#include "cdatabasemanager.h"
#include <QSqlQuery>
#include <QUuid>
#include <QImage>
#include <QPainter>
#include <QDir>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    m_simulateTimer=new QTimer(this);
    /* 2. 把日志信号绑到界面槽 */
    connect(Logger::instance(), &Logger::logAppended,
            this,               &MainWindow::onLogAppended);
    /* 3. 初始化表格 */
    ui->tableWidgetMessage->setColumnCount(3);
    ui->tableWidgetMessage->setHorizontalHeaderLabels(
        QStringList() << "时间" << "级别" << "内容");
    ui->tableWidgetMessage->horizontalHeader()->setStretchLastSection(true);
    ui->pushButtonTest->setVisible(false);


    //start tcp server listen
    CTrainTcpServer::instance()->StartServer(CSystemConfig::instance()->m_nListenPort);

    //camera initial
    CMvCamera::InitSDK();
    //open cameras
    CLineCameraManager::instance()->EnumDevices();
    //初始化数据库
    CDatabaseManager::Instance()->addUser({
        "admin",
        "管理员",
        "00001",
        "admin",
        "13800110011"

    });
    // 创建目录并生成带文件名的图片
    auto createImageWithFilename = [](const QString& filePath) {
        // 确保目录存在
        QDir dir = QFileInfo(filePath).absoluteDir();
        if (!dir.exists()) {
            dir.mkpath(".");
        }

        // 使用样本图像
        QImage image("D:\\MVS\\sample.jpg");
        // image.fill(Qt::white);

        // 在图像上绘制文件名
        QPainter painter(&image);
        painter.setPen(Qt::red);
        painter.setFont(QFont("Arial", 50));
        painter.drawText(image.rect(), Qt::AlignCenter, QFileInfo(filePath).fileName());

        // 保存图像
        return image.save(filePath);
    };

    quint64 reachTime = QDateTime::currentDateTime().toSecsSinceEpoch() - 100;
    QString strTrainNumber = "G4457";
    try{
    // 创建列车记录（保持不变）
    CDatabaseManager::Instance()->insertTrainRecord(
        {
            strTrainNumber,
            reachTime,
            "正向",
            18,
            8,
            "D:\\MVS",
            CSystemConfig::instance()->m_detectionName,
            (quint64)QDateTime::currentDateTime().toSecsSinceEpoch(),
         "未检视",
                CSystemConfig::instance()->m_strAreaCameraVideoFilePath+"\\left.mp4",
            CSystemConfig::instance()->m_strAreaCameraVideoFilePath+"\\right.mp4",
            CSystemConfig::instance()->m_strAreaCameraVideoFilePath+"\\top.mp4"
        }
        );

    for (quint16 i = 0; i < 8; i++) {
        // 生成图片路径
        QString basePath = CSystemConfig::instance()->m_strTrainImagesSavePath+"/"+QDateTime::currentDateTime().toString("yyyy-MM-dd")+"/";
        QDir dir(basePath);
        if (!dir.exists()) {
            if (!dir.mkpath(".")) {
                // 目录创建失败，这里可以添加错误处理
                qDebug() << "Failed to create directory:" << basePath;
            } else {
                dir.mkpath("left");
                dir.mkpath("right");
                dir.mkpath("top");
                qDebug() << "Directory created successfully:" << basePath;
            }
        }
        QString timestamp = QString::number(reachTime + i * 60);

        std::map<QString, QString> imagePaths = {
            {"left", basePath + "left/" + timestamp + ".jpg"},
            {"right", basePath + "right/" + timestamp + ".jpg"},
            {"top", basePath + "top/" + timestamp + ".jpg"}
        };

        // 创建图片文件
        for (const auto& path : imagePaths) {
            createImageWithFilename(path.second);
        }

        // 构建车厢信息
        CarriageInformation carriage;
        carriage.strGUID = QUuid::createUuid().toString(QUuid::WithoutBraces);
        carriage.strTrainNumber = strTrainNumber;
        carriage.nReachDatetime = reachTime;
        carriage.nOrder = (quint16)(i + 1);
        carriage.strWagonNumber = "162323" + QString::number(i);
        carriage.strCarriageModel = "敞口";
        carriage.strCarriageModelCode = "changkou";
        carriage.strFactoryAndDatetime = "华南造车厂2010年11月2日";
        carriage.nFrontWheelTime = (quint64)(reachTime + i * 60);
        carriage.frontWheelTime = carriage.nFrontWheelTime;
        carriage.frontWheelSpeed = 52.6;
        carriage.nRearWheelTime = (quint64)(reachTime + i * 160);
        carriage.rearWheelTime = carriage.nRearWheelTime;
        carriage.rearWheelSpeed = 50.5;
        carriage.fConverredLength = 15.6;
        carriage.inspectorState = false;
        carriage.imagePathMap = imagePaths;
        carriage.inspectorResult = {{"left", 0}, {"right", 0}, {"top", 0}};
        // 插入数据库
        bool insertResult = CDatabaseManager::Instance()->insertCarriage(carriage);
        if (!insertResult) {
            LOG("QSqlQuery::lastError()");
        }
    }
    }catch(const std::exception &e){
        LOG(e.what());
    }

}

MainWindow::~MainWindow()
{
    m_simulateTimer->stop();
    delete m_simulateTimer;
    delete ui;
}
void MainWindow::onLogAppended(const QString &time,
                               const QString &level,
                               const QString &msg)
{
    auto *tbl = ui->tableWidgetMessage;
    int row = tbl->rowCount();
    tbl->insertRow(row);

    tbl->setItem(row, 0, new QTableWidgetItem(time));
    tbl->setItem(row, 1, new QTableWidgetItem(level));
    tbl->setItem(row, 2, new QTableWidgetItem(msg));

    /* 自动滚动到最新一行 */
    tbl->scrollToBottom();
}
#include "imageuploader.h"
// 创建上传器实例
NetworkImageUploader uploader;
void MainWindow::Test(){


    // 配置上传参数
    uploader.setUrl(QUrl("http://127.0.0.1:5000/ocr")); // 测试用的免费API
    uploader.setFile("D:\\train6.jpg");
    uploader.setFieldName("image");
    uploader.setTimeout(60000); // 60秒超时
    uploader.setRetryCount(3);  // 重试3次
    uploader.setVerifySSL(false);

    // 添加表单字段
    QMap<QString, QString> fields;
    fields["description"] = "Test image upload";
    fields["timestamp"] = QDateTime::currentDateTime().toString();
    uploader.setFormFields(fields);

    // 连接信号
    QObject::connect(&uploader, &NetworkImageUploader::uploadProgress,
                     [](qint64 sent, qint64 total, double percent) {
                         qDebug() << "Progress:" << sent << "/" << total
                                  << "(" << QString::number(percent, 'f', 1) << "%)";
                     });

    QObject::connect(&uploader, &NetworkImageUploader::uploadStarted,
                     []() {
                         qDebug() << "=== Upload started ===";
                     });

    QObject::connect(&uploader, &NetworkImageUploader::uploadFinished,
                     [](const QByteArray &response) {
                         qDebug() << "=== Upload successful ===";
                         qDebug() << "Response:" << QString::fromUtf8(response.left(500)) << "...";

                         // 可选：解析JSON响应
                         // QJsonDocument doc = QJsonDocument::fromJson(response);
                         // qDebug() << "Parsed JSON:" << doc.toJson(QJsonDocument::Indented);

                         // QTimer::singleShot(0,QApplication::instance(), &QCoreApplication::quit);
                     });

    QObject::connect(&uploader, &NetworkImageUploader::uploadFailed,
                     [](NetworkImageUploader::UploadError error, const QString &errorString) {
                         qDebug() << "=== Upload failed ===";
                         qDebug() << "Error:" << static_cast<int>(error);
                         qDebug() << "Message:" << errorString;

                         // QTimer::singleShot(0, ,QApplication::instance(), &QCoreApplication::quit);
                     });

    // 开始上传
    uploader.startUpload();
}
void MainWindow::Config(){
    CConfigDialog dlg;
    dlg.exec();
}
