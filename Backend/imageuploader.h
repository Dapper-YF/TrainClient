#ifndef NETWORKIMAGEUPLOADER_H
#define NETWORKIMAGEUPLOADER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QHttpMultiPart>
#include <QFile>
#include <QUrl>
#include <QTimer>
#include <QSslConfiguration>
#include "singleton.h"

class NetworkImageUploader : public QObject,public Singleton<NetworkImageUploader>
{
    friend class Singleton<NetworkImageUploader>;
    Q_OBJECT

public:
    enum class UploadError {
        NoError = 0,
        FileNotFound,
        FileOpenFailed,
        NetworkError,
        TimeoutError,
        SSLError,
        ServerError
    };

    explicit NetworkImageUploader(QObject *parent = nullptr);
    ~NetworkImageUploader();

    // 设置上传参数
    void setUrl(const QUrl &url);
    void setFile(const QString &filePath);
    void setFieldName(const QString &fieldName);
    void setFormFields(const QMap<QString, QString> &fields);
    void setTimeout(int milliseconds);
    void setRetryCount(int count);

    // SSL/TLS配置
    void setVerifySSL(bool verify);
    void setSSLConfiguration(const QSslConfiguration &config);

    // 上传控制
    void startUpload();
    void cancelUpload();

    // 状态查询
    bool isUploading() const;
    UploadError lastError() const;
    QString lastErrorString() const;

signals:
    // 进度信号 (bytesSent, bytesTotal, percentage)
    void uploadProgress(qint64 bytesSent, qint64 bytesTotal, double percentage);

    // 状态信号
    void uploadStarted();
    void uploadFinished(const QByteArray &responseData);
    void uploadFailed(NetworkImageUploader::UploadError error, const QString &errorString);

    // 详细信号（包含更多信息）
    void uploadDetailedFinished(int statusCode,
                                const QByteArray &responseData,
                                const QList<QNetworkReply::RawHeaderPair> &headers);

private slots:
    void onUploadFinished(QNetworkReply *reply);
    void onUploadProgress(qint64 bytesSent, qint64 bytesTotal);
    void onTimeout();
    void onSslErrors(const QList<QSslError> &errors);

private:
    void cleanup();
    void handleError(UploadError error, const QString &errorString = QString());
    QHttpMultiPart* createMultipartForm();
    bool validateParameters();
    void retryUpload();

    QNetworkAccessManager *m_networkManager;
    QNetworkReply *m_currentReply;
    QTimer *m_timeoutTimer;

    // 上传参数
    QUrl m_url;
    QString m_filePath;
    QString m_fieldName;
    QMap<QString, QString> m_formFields;

    // 配置
    int m_timeoutMs;
    int m_retryCount;
    int m_currentRetry;
    bool m_verifySSL;
    QSslConfiguration m_sslConfig;

    // 状态
    bool m_isUploading;
    UploadError m_lastError;
    QString m_lastErrorString;
};

#endif // NETWORKIMAGEUPLOADER_H
