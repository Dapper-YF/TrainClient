#include "imageuploader.h"
#include <QFileInfo>
#include <QDebug>
#include <QSslSocket>

NetworkImageUploader::NetworkImageUploader(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_currentReply(nullptr)
    , m_timeoutTimer(new QTimer(this))
    , m_fieldName("image")
    , m_timeoutMs(30000) // 30秒默认超�?
    , m_retryCount(0)
    , m_currentRetry(0)
    , m_verifySSL(true)
    , m_isUploading(false)
    , m_lastError(UploadError::NoError)
{
    // 设置超时定时�?
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, &NetworkImageUploader::onTimeout);

    // 连接网络管理器信�?
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &NetworkImageUploader::onUploadFinished);
}

NetworkImageUploader::~NetworkImageUploader()
{
    cancelUpload();
}

void NetworkImageUploader::setUrl(const QUrl &url)
{
    m_url = url;
}

void NetworkImageUploader::setFile(const QString &filePath)
{
    m_filePath = filePath;
}

void NetworkImageUploader::setFieldName(const QString &fieldName)
{
    m_fieldName = fieldName;
}

void NetworkImageUploader::setFormFields(const QMap<QString, QString> &fields)
{
    m_formFields = fields;
}

void NetworkImageUploader::setTimeout(int milliseconds)
{
    m_timeoutMs = milliseconds;
}

void NetworkImageUploader::setRetryCount(int count)
{
    m_retryCount = qMax(0, count);
}

void NetworkImageUploader::setVerifySSL(bool verify)
{
    m_verifySSL = verify;
}

void NetworkImageUploader::setSSLConfiguration(const QSslConfiguration &config)
{
    m_sslConfig = config;
}

void NetworkImageUploader::startUpload()
{
    if (m_isUploading) {
        qWarning() << "Upload is already in progress";
        return;
    }

    if (!validateParameters()) {
        return;
    }

    m_currentRetry = 0;
    retryUpload();
}

void NetworkImageUploader::cancelUpload()
{
    if (m_currentReply) {
        m_currentReply->abort();
        // m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }

    m_timeoutTimer->stop();
    m_isUploading = false;
}

bool NetworkImageUploader::isUploading() const
{
    return m_isUploading;
}

NetworkImageUploader::UploadError NetworkImageUploader::lastError() const
{
    return m_lastError;
}

QString NetworkImageUploader::lastErrorString() const
{
    return m_lastErrorString;
}

void NetworkImageUploader::onUploadFinished(QNetworkReply *reply)
{
    m_timeoutTimer->stop();

    // 确保是我们当前的回复
    if (reply != m_currentReply) {
        reply->deleteLater();
        return;
    }

    m_isUploading = false;

    // 检查错�?
    if (reply->error() != QNetworkReply::NoError) {
        QString errorString = reply->errorString();
        int errorCode = reply->error();

        qDebug() << "Network error:" << errorCode << errorString;

        // 判断错误类型
        UploadError errorType = UploadError::NetworkError;
        if (errorCode == QNetworkReply::OperationCanceledError) {
            // 用户取消，不重试
            cleanup();
            return;
        } else if (errorCode == QNetworkReply::TimeoutError) {
            errorType = UploadError::TimeoutError;
        } else if (errorCode >= QNetworkReply::SslHandshakeFailedError) {
            errorType = UploadError::SSLError;
        }

        // 重试逻辑
        if (m_currentRetry < m_retryCount) {
            m_currentRetry++;
            qDebug() << "Retrying upload..." << m_currentRetry << "/" << m_retryCount;
            QTimer::singleShot(1000, this, &NetworkImageUploader::retryUpload);
            reply->deleteLater();
            m_currentReply = nullptr;
            return;
        }

        handleError(errorType, errorString);
        reply->deleteLater();
        m_currentReply = nullptr;
        return;
    }

    // 获取HTTP状态码
    QVariant statusCodeVariant = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    int statusCode = statusCodeVariant.isValid() ? statusCodeVariant.toInt() : 0;

    // 检查服务器错误 (4xx, 5xx)
    if (statusCode >= 400) {
        qDebug() << "Server error:" << statusCode;

        if (m_currentRetry < m_retryCount) {
            m_currentRetry++;
            qDebug() << "Retrying upload due to server error..." << m_currentRetry << "/" << m_retryCount;
            QTimer::singleShot(1000, this, &NetworkImageUploader::retryUpload);
            reply->deleteLater();
            m_currentReply = nullptr;
            return;
        }

        handleError(UploadError::ServerError,
                    QString("Server returned error code: %1").arg(statusCode));
        reply->deleteLater();
        m_currentReply = nullptr;
        return;
    }

    // 上传成功
    QByteArray responseData = reply->readAll();
    QList<QNetworkReply::RawHeaderPair> headers = reply->rawHeaderPairs();

    qDebug() << "Upload successful! Status code:" << statusCode;
    qDebug() << "Response size:" << responseData.size() << "bytes";

    emit uploadFinished(responseData);
    emit uploadDetailedFinished(statusCode, responseData, headers);

    reply->deleteLater();
    m_currentReply = nullptr;
    m_lastError = UploadError::NoError;
    m_lastErrorString.clear();
}

void NetworkImageUploader::onUploadProgress(qint64 bytesSent, qint64 bytesTotal)
{
    if (bytesTotal > 0) {
        double percentage = (double(bytesSent) / double(bytesTotal)) * 100.0;
        emit uploadProgress(bytesSent, bytesTotal, percentage);
    }
}

void NetworkImageUploader::onTimeout()
{
    if (m_currentReply && m_currentReply->isRunning()) {
        qDebug() << "Upload timeout after" << m_timeoutMs << "ms";
        m_currentReply->abort();
        handleError(UploadError::TimeoutError, "Upload operation timed out");
    }
}

void NetworkImageUploader::onSslErrors(const QList<QSslError> &errors)
{
    QString errorString;
    for (const QSslError &error : errors) {
        errorString += error.errorString() + "; ";
    }

    qWarning() << "SSL errors occurred:" << errorString;

    if (!m_verifySSL) {
        // 忽略SSL错误
        m_currentReply->ignoreSslErrors();
    } else {
        handleError(UploadError::SSLError, errorString);
    }
}

void NetworkImageUploader::cleanup()
{
    if (m_currentReply) {
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
    m_timeoutTimer->stop();
    m_isUploading = false;
}

void NetworkImageUploader::handleError(UploadError error, const QString &errorString)
{
    m_lastError = error;
    m_lastErrorString = errorString.isEmpty() ?
                            QString("Error code: %1").arg(static_cast<int>(error)) : errorString;

    qDebug() << "Upload failed:" << m_lastErrorString;
    emit uploadFailed(error, m_lastErrorString);

    cleanup();
}

QHttpMultiPart* NetworkImageUploader::createMultipartForm()
{
    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    // 添加文件部分
    QFile *file = new QFile(m_filePath);
    if (!file->open(QIODevice::ReadOnly)) {
        delete file;
        delete multiPart;
        return nullptr;
    }

    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("image/jpeg"));

    QString contentDisposition = QString("form-data; name=\"%1\"; filename=\"%2\"")
                                     .arg(m_fieldName, QFileInfo(m_filePath).fileName());
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant(contentDisposition));

    filePart.setBodyDevice(file);
    file->setParent(multiPart);
    multiPart->append(filePart);

    // 添加其他表单字段
    for (auto it = m_formFields.constBegin(); it != m_formFields.constEnd(); ++it) {
        QHttpPart textPart;
        textPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                           QVariant(QString("form-data; name=\"%1\"").arg(it.key())));
        textPart.setBody(it.value().toUtf8());
        multiPart->append(textPart);
    }

    return multiPart;
}

bool NetworkImageUploader::validateParameters()
{
    // 检查URL
    if (!m_url.isValid() || m_url.scheme().isEmpty()) {
        handleError(UploadError::NetworkError, "Invalid URL");
        return false;
    }

    // 检查文�?
    QFileInfo fileInfo(m_filePath);
    if (!fileInfo.exists()) {
        handleError(UploadError::FileNotFound, "File does not exist: " + m_filePath);
        return false;
    }

    if (!fileInfo.isReadable()) {
        handleError(UploadError::FileOpenFailed, "File is not readable: " + m_filePath);
        return false;
    }

    // 检查文件大小（可选限制）
    if (fileInfo.size() > 100 * 1024 * 1024) { // 100MB限制
        handleError(UploadError::FileOpenFailed, "File is too large: " + m_filePath);
        return false;
    }

    return true;
}

void NetworkImageUploader::retryUpload()
{
    cleanup();

    // 创建多部分表单数�?
    QHttpMultiPart *multiPart = createMultipartForm();
    if (!multiPart) {
        handleError(UploadError::FileOpenFailed, "Failed to create multipart form data");
        return;
    }

    // 创建网络请求
    QNetworkRequest request(m_url);
    request.setRawHeader("User-Agent", "QT6-ImageUploader/1.0");
    request.setRawHeader("Accept", "application/json");

    // SSL配置
    if (m_sslConfig != QSslConfiguration::defaultConfiguration()) {
        request.setSslConfiguration(m_sslConfig);
    }

    // 发送请�?
    m_currentReply = m_networkManager->post(request, multiPart);
    multiPart->setParent(m_currentReply);

    // 连接信号
    connect(m_currentReply, &QNetworkReply::uploadProgress,
            this, &NetworkImageUploader::onUploadProgress);
    connect(m_currentReply, &QNetworkReply::sslErrors,
            this, &NetworkImageUploader::onSslErrors);

    // 启动超时定时�?
    m_timeoutTimer->start(m_timeoutMs);
    m_isUploading = true;

    emit uploadStarted();
    qDebug() << "Upload started to:" << m_url.toString();
    qDebug() << "File:" << m_filePath;
    qDebug() << "Timeout:" << m_timeoutMs << "ms";
    qDebug() << "Retry:" << m_currentRetry << "/" << m_retryCount;
}
