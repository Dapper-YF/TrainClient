#include "ApiClient.h"
#include "../Config.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrl>

ApiClient::ApiClient(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

ApiClient::~ApiClient() {}

void ApiClient::login(const QString& username, const QString& password)
{
    QString url = Config::instance()->apiBaseUrl() + "/api/auth/login";

    QNetworkRequest request;
    request.setUrl(QUrl(url));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject body;
    body["username"] = username;
    body["password"] = password;

    QNetworkReply* reply = m_nam->post(request, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, &ApiClient::onLoginReply);
}

void ApiClient::get(const QString& path)
{
    QString url = Config::instance()->apiBaseUrl() + path;

    QNetworkRequest request;
    request.setUrl(QUrl(url));
    if (!m_token.isEmpty()) {
        request.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());
    }

    QNetworkReply* reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, &ApiClient::onGenericReply);
}

void ApiClient::post(const QString& path, const QJsonObject& body)
{
    QString url = Config::instance()->apiBaseUrl() + path;

    QNetworkRequest request;
    request.setUrl(QUrl(url));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!m_token.isEmpty()) {
        request.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());
    }

    QNetworkReply* reply = m_nam->post(request, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, &ApiClient::onGenericReply);
}

void ApiClient::onLoginReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    QByteArray data = reply->readAll();
    QJsonObject json = QJsonDocument::fromJson(data).object();

    if (reply->error() == QNetworkReply::NoError && json["success"].toBool()) {
        QJsonObject userObj = json["user"].toObject();
        QString token = json["token"].toString();
        m_token = token;
        Config::instance()->setToken(token);
        Config::instance()->setUser(userObj["username"].toString(), userObj["name"].toString());
        Config::instance()->setAdmin(userObj["isAdmin"].toBool());
        emit loginSuccess(userObj, token);
    } else {
        QString error = json["error"].toString("登录失败");
        emit loginFailed(error);
    }

    reply->deleteLater();
}

void ApiClient::onGenericReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    QByteArray data = reply->readAll();
    QJsonObject json = QJsonDocument::fromJson(data).object();

    if (reply->error() == QNetworkReply::NoError && json["success"].toBool()) {
        emit requestSuccess(json["data"].toObject());
    } else {
        QString error = json["error"].toString("请求失败");
        emit requestFailed(error);
    }

    reply->deleteLater();
}
