#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <QObject>
#include <QHttpServer>
#include <QHttpServerRequest>
#include <QHttpServerResponse>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QMimeDatabase>
#include "cdatabasemanager.h"
#include "logger.h"
#include "csystemconfig.h"
#include <QTcpServer>

class WebServer : public QObject
{
    Q_OBJECT

public:
    explicit WebServer(QObject *parent = nullptr);
    ~WebServer();

    bool start(quint16 port = 8080);

// private slots:
//     void handleRequest(QHttpServerRequest &&request, QHttpServerResponder &&responder);

private:
    QHttpServer *m_server;
    QMimeDatabase m_mimeDb;
    std::unique_ptr<QTcpServer> m_tcpServer;

    // 路由处理函数
    QHttpServerResponse handleLogin(const QJsonObject &request);
    QHttpServerResponse handleGetTrains(const QMap<QString, QString> &queryParams, const QString &token);
    QHttpServerResponse handleUpdateTrainInspection(const QJsonObject &request, const QString &token, int trainId);
    QHttpServerResponse handleGetCarriages(const QMap<QString, QString> &queryParams, const QString &token);
    QHttpServerResponse handleUpdateCarriageInspection(const QJsonObject &request, const QString &token, int carriageId);
    // 按列车号+车厢号+到达时间定位车厢（复合键，非数字ID）
    QHttpServerResponse handleUpdateCarriageInspectionByKey(const QJsonObject &request, const QString &token, const QMap<QString, QString> &queryParams);
    // 车厢标注
    QHttpServerResponse handleGetCarriageAnnotation(const QString &trainNumber, qint64 reachDatetime, const QMap<QString, QString> &queryParams, const QString &token);
    QHttpServerResponse handleSaveCarriageAnnotation(const QString &trainNumber, qint64 reachDatetime, int order, const QJsonObject &data, const QString &token);
    QHttpServerResponse handleGetStats(const QMap<QString, QString> &queryParams, const QString &token);
    QHttpServerResponse handleGetUsers(const QMap<QString, QString> &queryParams, const QString &token);
    QHttpServerResponse handleGetUser(const QString &username, const QString &token);
    QHttpServerResponse handleCreateUser(const QJsonObject &request, const QString &token);
    QHttpServerResponse handleUpdateUser(const QJsonObject &request, const QString &token, const QString &username);
    QHttpServerResponse handleResetPassword(const QJsonObject &request, const QString &token, const QString &username);
    QHttpServerResponse handleSetUserStatus(const QJsonObject &request, const QString &token, const QString &username);
    QHttpServerResponse handleChangePassword(const QJsonObject &request, const QString &token);
    QHttpServerResponse handleStaticFile(const QString &filePath);
    QHttpServerResponse handleRangeRequest(const QString &filePath, const QString &rangeHeader);

    // 工具函数
    QMap<QString, QString> getQueryParams(const QUrl &url);
    bool validateToken(const QString &token);
    QString getTokenFromHeader(const QHttpServerRequest &request);
    QString convertToWebImagePath(QString &localImagePath);
    QString convertToWebVideoPath(QString &localVideoPath);
    QHttpServerResponse createJsonResponse(int statusCode, const QJsonObject &data);
    QHttpServerResponse createErrorResponse(int statusCode, const QString &message);

    // 路由注册
    void setupRoutes();
};

#endif // WEBSERVER_H
