#ifndef CSYSTEMCONFIG_H
#define CSYSTEMCONFIG_H

#include <QString>
#include <QMap>

//系统配置
class CSystemConfig
{
public:
    explicit CSystemConfig(QString strFileName = "");
    bool Read();
    bool Save();
    static CSystemConfig *instance();

    /*---------------- 网络/服务 ----------------*/
    quint16 m_nListenPort{9090};          // 主服务监听端口
    quint16 m_nWebServerPort{8080};
    QString m_strPLCAddress;              // PLC 地址，如 "192.168.1.100"
    // quint16 m_nPLCPort{8989};             // PLC 监听端口
    QString m_strImageRecAddress;    //文字识别服务器IP
    quint16 m_nRecPort{5000};
    QString m_detectionName{"某市城南货运站"};              //检测站名称

    /*---------------- 线阵相机 ----------------*/
    // 相机 IP 映射：<位置(left/top/right), IP>
    QMap<QString, QString> m_mapCamerasIP;

    // 各位置相机参数
    QMap<QString, int>    m_camera_line_sampling_hz;  // 行帧(Hz)
    QMap<QString, float>  m_camera_exporsure_time;    // 曝光时间(ms)
    QMap<QString, int>    m_camera_image_height;      // 图像高度(px)
    QMap<QString, int>    m_camera_image_width;      // 图像高度(px)
    QMap<QString, double> m_camera_image_compensate;  // 图像增益
    int m_camera_capture_delay{0};                    // 拍照延迟(ms)

    /*---------------- 面阵相机 ----------------*/
    // 相机 IP 映射：<位置(left/top/right), IP>
    QMap<QString, QString> m_mapAreaCamerasIP;

    /*---------------- 文件保存 ----------------*/
    QString m_strTrainImagesSavePath{"c:\\trainImage"};
    bool   m_saveLineCameraTempFile{false};
    QString m_strLineCameraGFrabbingFilePath{"c:\\grabbing"};
    bool   m_saveAreaCameraVideoFile{false};
    QString m_strAreaCameraVideoFilePath{"c:\\video"};

    /*---------------- 工具函数 ----------------*/
    QString GetCameraName(const QString& strCameraIp);


private:
    QString m_strFileNmae{"config.ini"};
    static CSystemConfig s_instance;
};

#endif // CSYSTEMCONFIG_H
