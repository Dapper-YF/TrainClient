#include <QSettings>
#include <QDebug>
#include <QCoreApplication>
#include <QFileInfo>
// #include <QTextCodec>
#include "csystemconfig.h"

CSystemConfig CSystemConfig::s_instance;
CSystemConfig::CSystemConfig(QString strFileName)
{
    if (strFileName.size() > 0)
    {
        m_strFileNmae = strFileName;
    }
}

CSystemConfig *CSystemConfig::instance()
{
    return &s_instance;
}

bool CSystemConfig::Read()
{
    QString msFilepath = QCoreApplication::applicationDirPath() + "/" + m_strFileNmae;
    QSettings settings(msFilepath, QSettings::IniFormat);

    /* 网络 / 服务 */
    m_nListenPort   = static_cast<quint16>(settings.value("net/listenport", 9090).toUInt());
    m_nWebServerPort=static_cast<quint16>(settings.value("net/webServerPort",8080).toUInt());
    m_nRecPort= static_cast<quint16>(settings.value("net/imagerecport", 5000).toUInt());
    m_strPLCAddress = settings.value("net/plcaddress", "").toString();
    m_strImageRecAddress= settings.value("net/imagerecaddress", "").toString();
    // m_nPLCPort      = static_cast<quint16>(settings.value("net/plcport", 8989).toUInt());

    /* 线阵相机 IP */
    m_mapCamerasIP["left"]  = settings.value("cameras/line_left").toString();
    m_mapCamerasIP["top"]   = settings.value("cameras/line_top").toString();
    m_mapCamerasIP["right"] = settings.value("cameras/line_right").toString();

    /* 面阵相机 IP */
    m_mapAreaCamerasIP["area_left"]  = settings.value("cameras/area_left").toString();
    m_mapAreaCamerasIP["area_top"]   = settings.value("cameras/area_top").toString();
    m_mapAreaCamerasIP["area_right"] = settings.value("cameras/area_right").toString();

    /* 线阵相机参数 */
    m_camera_line_sampling_hz["left"]  = settings.value("camera/line_sampling_hz_left",  10000).toInt();
    m_camera_line_sampling_hz["top"]   = settings.value("camera/line_sampling_hz_top",   10000).toInt();
    m_camera_line_sampling_hz["right"] = settings.value("camera/line_sampling_hz_right", 10000).toInt();

    m_camera_exporsure_time["left"]  = settings.value("camera/exposure_left",  0.1).toFloat();
    m_camera_exporsure_time["top"]   = settings.value("camera/exposure_top",   0.1).toFloat();
    m_camera_exporsure_time["right"] = settings.value("camera/exposure_right", 0.1).toFloat();

    m_camera_image_height["left"]  = settings.value("camera/height_left",  2048).toInt();
    m_camera_image_height["top"]   = settings.value("camera/height_top",   2048).toInt();
    m_camera_image_height["right"] = settings.value("camera/height_right", 2048).toInt();
    m_camera_image_width["left"]  = settings.value("camera/width_left",  2048).toInt();
    m_camera_image_width["top"]   = settings.value("camera/width_top",   2048).toInt();
    m_camera_image_width["right"] = settings.value("camera/width_right", 2048).toInt();


    m_camera_image_height["area_left"]  = settings.value("camera/area_height_left",  2048).toInt();
    m_camera_image_height["area_top"]   = settings.value("camera/area_height_top",   2048).toInt();
    m_camera_image_height["area_right"] = settings.value("camera/area_height_right", 2048).toInt();
    m_camera_image_width["area_left"]  = settings.value("camera/area_width_left",  2048).toInt();
    m_camera_image_width["area_top"]   = settings.value("camera/area_width_top",   2048).toInt();
    m_camera_image_width["area_right"] = settings.value("camera/area_width_right", 2048).toInt();

    m_camera_line_sampling_hz["area_left"]  = settings.value("camera/area_sampling_hz_left",  10000).toInt();
    m_camera_line_sampling_hz["area_top"]   = settings.value("camera/area_sampling_hz_top",   10000).toInt();
    m_camera_line_sampling_hz["area_right"] = settings.value("camera/area_sampling_hz_right", 10000).toInt();


    m_camera_image_compensate["left"]  = settings.value("camera/gain_left",  1.0).toDouble();
    m_camera_image_compensate["top"]   = settings.value("camera/gain_top",   1.0).toDouble();
    m_camera_image_compensate["right"] = settings.value("camera/gain_right", 1.0).toDouble();

    m_camera_capture_delay = settings.value("camera/capture_delay", 0).toInt();

    /* 文件保存 */
    m_strTrainImagesSavePath=settings.value("save/train_image_path", "../../mvsimages").toString();
    m_saveLineCameraTempFile = settings.value("save/save_line_temp", false).toBool();
    m_strLineCameraGFrabbingFilePath = settings.value("save/line_temp_path", "c:\\grabbing").toString();
    m_saveAreaCameraVideoFile = settings.value("save/save_area_video", false).toBool();
    m_strAreaCameraVideoFilePath = settings.value("save/area_video_path", "c:\\video").toString();

    return true;
}

bool CSystemConfig::Save()
{
    QString msFilepath = QCoreApplication::applicationDirPath() + "/" + m_strFileNmae;
    QSettings settings(msFilepath, QSettings::IniFormat);

    /* 网络 / 服务 */
    settings.setValue("net/listenport",   m_nListenPort);
    settings.setValue("net/webServerPort",m_nWebServerPort);
    settings.setValue("net/imagerecport",   m_nRecPort);
    settings.setValue("net/imagerecaddress",   m_strImageRecAddress);
    settings.setValue("net/plcaddress",   m_strPLCAddress);
    // settings.setValue("net/plcport",      m_nPLCPort);

    /* 线阵相机 IP */
    settings.setValue("cameras/line_left",  m_mapCamerasIP.value("left"));
    settings.setValue("cameras/line_top",   m_mapCamerasIP.value("top"));
    settings.setValue("cameras/line_right", m_mapCamerasIP.value("right"));

    /* 面阵相机 IP */
    settings.setValue("cameras/area_left",  m_mapAreaCamerasIP.value("area_left"));
    settings.setValue("cameras/area_top",   m_mapAreaCamerasIP.value("area_top"));
    settings.setValue("cameras/area_right", m_mapAreaCamerasIP.value("area_right"));

    /* 线阵相机参数 */
    settings.setValue("camera/line_sampling_hz_left",  m_camera_line_sampling_hz.value("left"));
    settings.setValue("camera/line_sampling_hz_top",   m_camera_line_sampling_hz.value("top"));
    settings.setValue("camera/line_sampling_hz_right", m_camera_line_sampling_hz.value("right"));

    settings.setValue("camera/exposure_left",  m_camera_exporsure_time.value("left"));
    settings.setValue("camera/exposure_top",   m_camera_exporsure_time.value("top"));
    settings.setValue("camera/exposure_right", m_camera_exporsure_time.value("right"));

    settings.setValue("camera/height_left",  m_camera_image_height.value("left"));
    settings.setValue("camera/height_top",   m_camera_image_height.value("top"));
    settings.setValue("camera/height_right", m_camera_image_height.value("right"));
    settings.setValue("camera/width_left",  m_camera_image_width.value("left"));
    settings.setValue("camera/width_top",   m_camera_image_width.value("top"));
    settings.setValue("camera/width_right", m_camera_image_width.value("right"));

    settings.setValue("camera/area_height_left",  m_camera_image_height.value("area_left"));
    settings.setValue("camera/area_height_top",   m_camera_image_height.value("area_top"));
    settings.setValue("camera/area_height_right", m_camera_image_height.value("area_right"));
    settings.setValue("camera/area_width_left",  m_camera_image_width.value("area_left"));
    settings.setValue("camera/area_width_top",   m_camera_image_width.value("area_top"));
    settings.setValue("camera/area_width_right", m_camera_image_width.value("area_right"));

    settings.setValue("camera/area_sampling_hz_left",  m_camera_line_sampling_hz.value("area_left"));
    settings.setValue("camera/area_sampling_hz_top",   m_camera_line_sampling_hz.value("area_top"));
    settings.setValue("camera/area_sampling_hz_right", m_camera_line_sampling_hz.value("area_right"));

    settings.setValue("camera/gain_left",  m_camera_image_compensate.value("left"));
    settings.setValue("camera/gain_top",   m_camera_image_compensate.value("top"));
    settings.setValue("camera/gain_right", m_camera_image_compensate.value("right"));

    settings.setValue("camera/capture_delay", m_camera_capture_delay);

    /* 文件保存 */
    settings.setValue("save/train_image_path",m_strTrainImagesSavePath);
    settings.setValue("save/save_line_temp",   m_saveLineCameraTempFile);
    settings.setValue("save/line_temp_path",   m_strLineCameraGFrabbingFilePath);
    settings.setValue("save/save_area_video",  m_saveAreaCameraVideoFile);
    settings.setValue("save/area_video_path",  m_strAreaCameraVideoFilePath);

    return true;
}

QString CSystemConfig::GetCameraName(const QString &strCameraIp)
{
    QString strName = "";

    QString strKey = "";
    QMap<QString, QString>::iterator itor;
    //查找线阵相机
    for (itor = m_mapCamerasIP.begin(); itor != m_mapCamerasIP.end(); ++itor)
    {
        if (itor.value() == strCameraIp)
        {
            strKey = itor.key();
            break;
        }
    }
    //查找线阵相机
    for (itor = m_mapAreaCamerasIP.begin(); itor != m_mapAreaCamerasIP.end(); ++itor)
    {
        if (itor.value() == strCameraIp)
        {
            strKey = itor.key();
            break;
        }
    }

    return strKey;
}
