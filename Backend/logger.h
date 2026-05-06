#ifndef LOGGER_H
#define LOGGER_H



#include <QObject>
#include <QFile>        // 文件操作
#include <QMutex>       // 线程互斥锁
#include <QTextStream>  // 文本流操作
#include <QDateTime>    // 日期时间处理

// 日志级别枚举
enum LogLevel {
    LOG_DEBUG = 0,   // 调试信息，最详细级别
    LOG_INFO,        // 一般信息
    LOG_WARNING,     // 警告信息
    LOG_ERROR,       // 错误信息
    LOG_FATAL        // 致命错误，最高级别
};

// 日志记录器类
class Logger : public QObject
{
    Q_OBJECT
public:
    Logger(QObject *parent = nullptr);  // 构造函数
    ~Logger();                                   // 析构函数

public:

    // 获取单例实例
    static Logger *instance();
    // 初始化日志系统（单例模式）
    // @param path 日志文件路径，空表示不记录到文件
    // @param level 日志记录级别
    // @param redirectQtMessages 是否重定向Qt系统消息
    void init(const QString &path = "",
                    LogLevel level = LOG_DEBUG,
                    bool redirectQtMessages = true);
    static void Log(const QString &message,LogLevel level=LOG_DEBUG);
    // 清理日志系统资源
    void cleanup();
    QString getLogPath()const;

    // 日志级别转字符串
     static QString levelToString(LogLevel level);
    // 核心日志记录函数
    // @param level 日志级别
    // @param message 日志内容
    // @param file 源文件名（自动获取）
    // @param line 源代码行号（自动获取）
    // @param function 函数名（自动获取）


    // 便捷日志接口（不同级别）
//    static void debug(const QString &message,
//                     const char *file = nullptr,
//                     int line = 0,
//                     const char *function = nullptr);

//    static void info(const QString &message,
//                    const char *file = nullptr,
//                    int line = 0,
//                    const char *function = nullptr);

//    static void warning(const QString &message,
//                       const char *file = nullptr,
//                       int line = 0,
//                       const char *function = nullptr);

//    static void error(const QString &message,
//                     const char *file = nullptr,
//                     int line = 0,
//                     const char *function = nullptr);
signals:
     /* 每次写日志时发出，三个字段直接给界面用 */
     void logAppended(const QString &time,
                      const QString &level,
                      const QString &msg);
private:
    // 日志文件轮转（按日期分割）
    void rotateLogFile();
    bool ensureFileOpen();



    // Qt消息处理函数（用于捕获Qt系统消息）
    static void messageHandler(QtMsgType type,
                              const QMessageLogContext &context,
                              const QString &msg);

    // 成员变量
    QFile *m_logFile;        // 日志文件对象
    QMutex m_mutex;         // 互斥锁（保证线程安全）
    LogLevel m_logLevel;    // 当前日志级别
    QString m_logPath;      // 日志文件路径
    QDate m_currentDate;    // 当前日志日期（用于轮转）

    static Logger* s_instance; // 单例实例指针
};

// 宏定义简化日志调用（自动填充文件、行号、函数信息）
#define LOG Logger::instance()->Log

//#define LOG_DEBUG(message)   Logger::debug(message, __FILE__, __LINE__, Q_FUNC_INFO)
//#define LOG_INFO(message)    Logger::info(message, __FILE__, __LINE__, Q_FUNC_INFO)
//#define LOG_WARNING(message) Logger::warning(message, __FILE__, __LINE__, Q_FUNC_INFO)
//#define LOG_ERROR(message)   Logger::error(message, __FILE__, __LINE__, Q_FUNC_INFO)
//#define LOG_FATAL(message)   Logger::error(message, __FILE__, __LINE__, Q_FUNC_INFO)  // 注意：实际使用error级别


#endif // LOGGER_H
