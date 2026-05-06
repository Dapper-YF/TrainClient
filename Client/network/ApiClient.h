#ifndef APICLIENT_H
#define APICLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>

class ApiClient : public QObject
{
    Q_OBJECT

public:
    explicit ApiClient(QObject* parent = nullptr);
    ~ApiClient();

    // 登录
    void login(const QString& username, const QString& password);

    // 通用GET请求
    void get(const QString& path);

    // 通用POST请求
    void post(const QString& path, const QJsonObject& body);

signals:
    void loginSuccess(const QJsonObject& userData, const QString& token);
    void loginFailed(const QString& error);
    void requestSuccess(const QJsonObject& data);
    void requestFailed(const QString& error);

private slots:
    void onLoginReply();
    void onGenericReply();

private:
    QNetworkAccessManager* m_nam;
    QString m_token;
};

#endif // APICLIENT_H
