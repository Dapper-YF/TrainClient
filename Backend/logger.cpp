#include "logger.h"
#include <QDir>                    // 目录操作
#include <QStandardPaths>          // 标准路径获取
#include <QDebug>                  // Qt调试输出
#include<QCoreApplication>
#include<QApplication>
#include<QMessageBox>
#include<QTextStream>
// 初始化静态单例实例
Logger* Logger::s_instance = new Logger();

// 构造函数
Logger::Logger(QObject *parent)
    : QObject(parent),
      m_logLevel(LOG_DEBUG),          // 默认日志级别为DEBUG
      m_currentDate(QDate::currentDate()) // 记录当前日期用于日志轮转
{

}

// 析构函数
Logger::~Logger()
{
    if(m_logFile!=nullptr)
    {
        // 确保日志文件关闭
        if(m_logFile->isOpen()) {
            m_logFile->close();
        }

        delete m_logFile;
        m_logFile=nullptr;
    }


}

// 获取单例实例
Logger* Logger::instance()
{
    return s_instance;
}

// 初始化日志系统
void Logger::init(const QString &path, LogLevel level, bool redirectQtMessages)
{
    // 使用互斥锁确保线程安全
    QMutexLocker locker(&instance()->m_mutex);

    // 设置日志级别
    instance()->m_logLevel = level;

    // 设置日志路径（如果未指定则使用默认路径）
    // 设置日志文件路径
        if (path.isEmpty()) {
            // 默认路径：exe生成目录下的logs 子目录
            QString defaultDir =QCoreApplication::applicationDirPath()  + "/logs";

            // 创建目录（如果不存在）
            if (!QDir().mkpath(defaultDir)) {
                qCritical() << "Failed to create default log directory:" << defaultDir;
                QMessageBox::information(nullptr,"提示","日志系统初始化过程创建目录失败!_1");
                QApplication::quit();//结束程序
            }

            instance()->m_logPath = defaultDir;
        } else {
            // 使用用户指定的完整文件路径
            instance()->m_logPath = path;

            // 提取纯目录路径
            QFileInfo fileInfo(path);
            QString dirPath = fileInfo.absolutePath();

            // 创建目录（如果不存在）
            if (!QDir().exists(dirPath)) {
                if (!QDir().mkpath(dirPath)) {
                    qCritical() << "Failed to create log directory:" << dirPath;
                    QMessageBox::information(nullptr,"提示","日志系统初始化过程创建目录失败!_2");
                    QApplication::quit();//结束程序
                }
            }
        }
        m_logFile= new QFile(this->m_logPath);
//        if (!m_logFile->open(QIODevice::WriteOnly | QIODevice::Text|QIODevice::Append))
//        {
//            QMessageBox::information(nullptr,"提示","日志系统初始化过程打开日志文件失败!_3");
//            QApplication::quit();//结束程序
//        }


    // 重定向Qt系统消息
    if(redirectQtMessages) {
        // 安装Qt消息处理函数
        qInstallMessageHandler(messageHandler);
    }
}

// 清理日志系统资源
void Logger::cleanup()
{
    // 删除单例实例
    delete s_instance;
    s_instance = nullptr;

    // 恢复默认的Qt消息处理
    qInstallMessageHandler(nullptr);
}

QString Logger::getLogPath() const
{
    return this->m_logPath;
}
// 日志级别枚举转字符串
QString Logger::levelToString(LogLevel level)
{
    switch(level) {
    case LOG_DEBUG:   return "DEBUG";
    case LOG_INFO:    return "INFO";
    case LOG_WARNING: return "WARN";
    case LOG_ERROR:   return "ERROR";
    case LOG_FATAL:   return "FATAL";
    default:      return "UNKNOWN";
    }
}
// 核心日志记录函数
void Logger::Log( const QString &message,LogLevel level)
{
    // 检查日志级别是否满足记录条件
    if(level < instance()->m_logLevel) return;

    // 构建日志条目
    QString logEntry = QString("[%1][%2] ")
        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz")) // 时间戳
        .arg(levelToString(level)) ;// 日志级别字符串
        //.arg(QString("Location:%1:%2-\"%3\")").arg(file).arg(line).arg(function)); // 位置信息

    // 添加日志消息内容
    if(!message.isEmpty()) {
        logEntry += " " + message;
    }
    // 线程安全锁定
    QMutexLocker locker(&instance()->m_mutex);

    // 检查并执行日志文件轮转
    instance()->rotateLogFile();

    // 确保文件已打开
       if (!instance()->ensureFileOpen()) {
           QMessageBox::information(nullptr,"提示","日志系统Log接口打开文件失败!");
           return;
       }

    // 将日志写入文件
    QTextStream stream(instance()->m_logFile);
    stream << logEntry << Qt::endl;

    /* 新增：把本条日志发给 UI */
    emit instance()->logAppended(
        QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"),
        levelToString(level),
        message);

}

// 调试级别日志记录
//void Logger::debug(const QString &message, const char *file, int line, const char *function)
//{
//    log(DEBUG, message, file, line, function);
//}

// 信息级别日志记录
//void Logger::info(const QString &message, const char *file, int line, const char *function)
//{
//    log(INFO, message, file, line, function);
//}

// 警告级别日志记录
//void Logger::warning(const QString &message, const char *file, int line, const char *function)
//{
//    log(WARNING, message, file, line, function);
//}

// 错误级别日志记录
//void Logger::error(const QString &message, const char *file, int line, const char *function)
//{
//    log(ERROR, message, file, line, function);
//}

// 日志文件轮转函数（按日期分割）
void Logger::rotateLogFile()
{
    // 获取当前日期
    QDate today = QDate::currentDate();

    // 检查是否需要创建新的日志文件（日期变化或文件未打开）
    if(m_currentDate != today || !m_logFile->isOpen()) {
        m_currentDate = today;

        // 构建带日期的日志文件名（格式：DiagnosisSys_20250601.log）
        QString filename = QString("%1/DiagnosisSys_%2.log")
            .arg(m_logPath)
            .arg(m_currentDate.toString("yyyyMMdd"));

        // 关闭当前日志文件（如果已打开）
        if(m_logFile->isOpen()) {
            m_logFile->close();
        }

        // 打开新的日志文件
        m_logFile->setFileName(filename);
        if(!m_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            // 文件打开失败时输出警告
            //qWarning() << "Failed to open log file:" << filename;
            QMessageBox::information(nullptr,"提示","日志系统文件轮转过程中打开文件失败!");


        }

    }
}

bool Logger::ensureFileOpen()
{
    // 如果文件已经打开，直接返回成功
        if (m_logFile->isOpen()) {
            return true;
        }

        // 生成带日期的文件名
        QString fileName = QString("DiagnosisSys_%1.log").arg(m_currentDate.toString("yyyyMMdd"));
        QString filePath = this->m_logPath + "/" + fileName;

        // 设置文件名
        m_logFile->setFileName(filePath);

        // 尝试打开文件
        if (m_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            //qDebug() << "Opened log file:" << filePath;
            return true;
        }

        // 打开失败，记录错误
        QString error = m_logFile->errorString();
        QString originalPath = filePath;

        // 尝试使用临时文件作为后备
        QString fallbackPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                             + "/"+ fileName;  //C:/Users/HP/AppData/Local/Temp/fileName

//        qWarning() << "Failed to open log file:" << originalPath
//                   << "Error:" << error
//                   << "Trying fallback:" << fallbackPath;

        // 尝试后备路径
        m_logFile->setFileName(fallbackPath);
        if (m_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            // 更新路径为后备路径
//            qWarning() << "Using fallback log file:" << fallbackPath;
            QMessageBox::information(nullptr,"提示","使用了后备路径输出日志!");
            return true;
        }

        // 后备路径也失败
        QMessageBox::information(nullptr,"提示",QString("后备路径日志打开失败!-%1").arg(m_logFile->errorString()));
        return false;
}

// Qt消息处理函数（捕获Qt系统消息）
void Logger::messageHandler(QtMsgType type,
                            const QMessageLogContext &context,
                            const QString &msg)
{
    // 将Qt消息类型转换为日志级别
    LogLevel level = LOG_DEBUG;
    switch(type) {
    case QtDebugMsg:    level = LOG_DEBUG; break;
    case QtInfoMsg:     level = LOG_INFO; break;
    case QtWarningMsg:  level = LOG_WARNING; break;
    case QtCriticalMsg: level = LOG_ERROR; break;
    case QtFatalMsg:    level = LOG_FATAL; break;
    }

    // 将Qt系统消息记录到日志系统
    Log( msg,level);
}
