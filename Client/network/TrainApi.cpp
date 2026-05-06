#include "TrainApi.h"
#include "../Config.h"
#include "ImagePathMapper.h"
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QFile>
#include <QTextStream>
#include <QFileDevice>

TrainApiClient::TrainApiClient(QObject* parent)
    : QObject(parent)
{
    m_nam = new QNetworkAccessManager(this);
    m_apiBase = Config::instance()->apiBaseUrl();
    m_token = Config::instance()->token();
}

// ============================================================
// 登录 POST /api/auth/login
// ============================================================
void TrainApiClient::login(const QString& username, const QString& password)
{
    QString url = m_apiBase + "/api/auth/login";
    QNetworkRequest req;
    req.setUrl(QUrl(url));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject body;
    body["username"] = username;
    body["password"] = password;

    QJsonDocument doc(body);
    QNetworkReply* reply = m_nam->post(req, doc.toJson());

    connect(reply, &QNetworkReply::finished, this, [=]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit loginFailed(reply->errorString());
            reply->deleteLater();
            return;
        }

        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject root = doc.object();

        if (root["success"].toBool()) {
            // 后端返回格式: {"success":true, "token":"xxx", "user":{...}}
            QString token = root["token"].toString();
            QJsonObject userData = root["user"].toObject();
            emit loginSuccess(token, userData);
        } else {
            QString err = root["error"].toString("登录失败");
            emit loginFailed(err);
        }
        reply->deleteLater();
    });
}

// ============================================================
// 获取列车列表 GET /api/trains
// ============================================================
void TrainApiClient::getTrainsByDate(const QString& date)
{
    QUrl url(m_apiBase + "/api/trains");
    QUrlQuery query;
    query.addQueryItem("date", date);
    url.setQuery(query);
    doGetRequest(url.toString());
}

void TrainApiClient::getTrainsByRange(quint64 startTs, quint64 endTs)
{
    QUrl url(m_apiBase + "/api/trains");
    QUrlQuery query;
    query.addQueryItem("startDate", QString::number(startTs));
    query.addQueryItem("endDate", QString::number(endTs));
    // 使用大 pageSize 确保后端返回全量数据，由前端进行客户端分页
    query.addQueryItem("page", "1");
    query.addQueryItem("pageSize", "999999");
    url.setQuery(query);
    doGetRequest(url.toString());
}

void TrainApiClient::doGetRequest(const QString& url)
{
    // 调试日志
    {
        QFile logFile("E:/Study/AI_Workspace/TrainClient/Client/logs/api_debug.log");
        if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QTextStream ts(&logFile);
            ts << "[doGetRequest] URL: " << url << "\n";
            ts << "[doGetRequest] Config token: " << (Config::instance()->token().isEmpty() ? "EMPTY" : Config::instance()->token().left(20) + "...") << "\n";
        }
    }

    QNetworkRequest req = makeRequest(QUrl(url));
    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [=]() {
        handleHttpError(reply);
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QString response = QString::fromUtf8(data);

            // 调试日志
            {
                QFile logFile("E:/Study/AI_Workspace/TrainClient/Client/logs/api_debug.log");
                if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
                    QTextStream ts(&logFile);
                    ts << "[doGetRequest] Response: " << response.left(500) << "\n";
                }
            }

            QJsonDocument doc = QJsonDocument::fromJson(data);
            QJsonObject root = doc.object();

            QList<TrainInfo2> trains;
            if (root["success"].toBool()) {
                QJsonArray arr = root["data"].toObject()["trains"].toArray();
                for (const QJsonValue& v : arr) {
                    QJsonObject obj = v.toObject();
                    TrainInfo2 t;
                    t.id = obj["id"].toInt();
                    t.trainNumber = obj["strTrainNumber"].toString();
                    t.reachDatetime = obj["nReachDatetime"].toVariant().toULongLong();
                    t.direction = obj["strDirection"].toString();
                    t.numberOfAxles = obj["nNumberOfAxles"].toInt();
                    t.numberOfCarriage = obj["nNumberOfCarriage"].toInt();
                    t.inspectionPassNumber = obj["nInspectionPassNumber"].toInt();
                    t.inspectionDispassNumber = obj["nInspectionDispassNumber"].toInt();
                    t.uninspectionNumber = obj["nUnInspectionNumber"].toInt();
                    t.inspectionStatus = obj["strInspectionStatus"].toString();
                    t.strDetection = obj["strDetection"].toString();
                    t.inspectionDatetime = obj["nInspectionDatetime"].toVariant().toULongLong();
                    t.leftImagePath = obj["strLeftAreaCameraPath"].toString();
                    t.rightImagePath = obj["strRightAreaCameraPath"].toString();
                    t.topImagePath = obj["strTopAreaCameraPath"].toString();
                    trains.append(t);
                }
            }
            // 调试日志
            {
                QFile logFile("E:/Study/AI_Workspace/TrainClient/Client/logs/api_debug.log");
                if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
                    QTextStream ts(&logFile);
                    ts << "[doGetRequest] Parsed trains count: " << trains.count() << "\n";
                }
            }

            emit trainsReady(trains);
        } else {
            QFile logFile("E:/Study/AI_Workspace/TrainClient/Client/logs/api_debug.log");
            if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
                QTextStream ts(&logFile);
                ts << "[doGetRequest] HTTP Error: " << reply->errorString() << "\n";
            }
        }
        reply->deleteLater();
    });
}

// ============================================================
// 获取车厢列表 GET /api/carriages
// ============================================================
void TrainApiClient::getCarriages(const QString& trainNumber, quint64 reachDatetime)
{
    QUrl url(m_apiBase + "/api/carriages");
    QUrlQuery query;
    query.addQueryItem("trainNumber", trainNumber);
    query.addQueryItem("reachDatetime", QString::number(reachDatetime));
    url.setQuery(query);

    QNetworkRequest req = makeRequest(QUrl(url));
    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [=]() {
        handleHttpError(reply);
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            QJsonObject root = doc.object();

            QList<CarriageInfo2> carriages;
            if (root["success"].toBool()) {
                // 后端返回格式: {"success":true,"carriages":[...]}
                QJsonArray arr;
                if (root.contains("carriages")) {
                    arr = root["carriages"].toArray();
                } else if (root["data"].toObject().contains("carriages")) {
                    arr = root["data"].toObject()["carriages"].toArray();
                }
                for (const QJsonValue& v : arr) {
                    QJsonObject obj = v.toObject();
                    CarriageInfo2 c;
                    c.id = obj["id"].toInt();
                    c.number = obj["nCarriageNumber"].toInt();
                    c.nOrder = obj["nOrder"].toInt();
                    c.trainNumber = obj["strTrainNumber"].toString();
                    c.reachDatetime = obj["nReachDatetime"].toVariant().toULongLong();
                    c.inspected = (obj["strInspectionStatus"].toString() == "已检视");
                    c.inspectionStatus = obj["strInspectionStatus"].toString();
                    c.leftImagePath = obj["leftImagePath"].toString();
                    c.rightImagePath = obj["rightImagePath"].toString();
                    c.topImagePath = obj["topImagePath"].toString();
                    QJsonObject pathMap = obj["imagePathMap"].toObject();
                    QString rawLeft = pathMap["left"].toString();
                    QString rawRight = pathMap["right"].toString();
                    QString rawTop = pathMap["top"].toString();

                    // 直接使用 imagePathMap 中的 HTTP 路径（来自后端 /trainimages/... 路由）
                    c.localLeftImage = rawLeft;
                    c.localRightImage = rawRight;
                    c.localTopImage = rawTop;

                    carriages.append(c);
                }
            }
            emit carriagesReady(carriages);
        }
        reply->deleteLater();
    });
}

// ============================================================
// 提交车厢检车 PUT /api/carriages/<id>/inspection
// ============================================================
void TrainApiClient::submitCarriageInspection(const QString& trainNumber, int carriageNumber, quint64 reachDatetime, const QJsonObject& data)
{
    // 用 Query 参数传递复合键（trainNumber + carriageNumber + reachDatetime 定位车厢）
    QUrl url(m_apiBase + "/api/carriages/inspection");
    QUrlQuery query;
    query.addQueryItem("trainNumber", trainNumber);
    query.addQueryItem("carriageNumber", QString::number(carriageNumber));
    query.addQueryItem("reachDatetime", QString::number(reachDatetime));
    url.setQuery(query);
    doPutRequest(url.toString(), data);
}

void TrainApiClient::doPutRequest(const QString& url, const QJsonObject& body)
{
    QNetworkRequest req = makeRequest(QUrl(url));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonDocument doc(body);
    QNetworkReply* reply = m_nam->put(req, doc.toJson());

    connect(reply, &QNetworkReply::finished, this, [=]() {
        handleHttpError(reply);
        bool ok = (reply->error() == QNetworkReply::NoError);
        QString msg = ok ? "提交成功" : reply->errorString();
        emit submitDone(ok, msg);
        reply->deleteLater();
    });
}

// ============================================================
// 获取统计信息 GET /api/stats
// ============================================================
// ============================================================
// 获取个人账户信息 GET /api/users/info/<username>
// ============================================================
void TrainApiClient::getUserProfile(const QString& username)
{
    QUrl url(m_apiBase + "/api/users/info/" + username);
    QNetworkRequest req = makeRequest(url);
    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [=]() {
        handleHttpError(reply);
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QJsonObject root = doc.object();
            if (root["success"].toBool()) {
                emit userProfileReady(root["data"].toObject());
            } else {
                emit error(root["error"].toString());
            }
        }
        reply->deleteLater();
    });
}

// ============================================================
// 更新个人账户信息 PUT /api/users/<username>
// ============================================================
void TrainApiClient::updateUserProfile(const QString& username, const QJsonObject& data)
{
    QUrl url(m_apiBase + "/api/users/" + username);
    QNetworkRequest req = makeRequest(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply* reply = m_nam->put(req, QJsonDocument(data).toJson());
    connect(reply, &QNetworkReply::finished, this, [=]() {
        handleHttpError(reply);
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QJsonObject root = doc.object();
            emit userProfileUpdated(root["success"].toBool(), root["data"].toObject());
        } else {
            QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
            emit userProfileUpdated(false, root);
        }
        reply->deleteLater();
    });
}

// ============================================================
// 修改密码 PUT /api/users/me/password
// ============================================================
void TrainApiClient::changePassword(const QString& currentPassword, const QString& newPassword)
{
    QUrl url(m_apiBase + "/api/users/me/password");
    QNetworkRequest req = makeRequest(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject body;
    body["currentPassword"] = currentPassword;
    body["newPassword"] = newPassword;
    QNetworkReply* reply = m_nam->put(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [=]() {
        handleHttpError(reply);
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QJsonObject root = doc.object();
            emit passwordChanged(root["success"].toBool(), root["error"].toString());
        } else {
            QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
            emit passwordChanged(false, root["error"].toString());
        }
        reply->deleteLater();
    });
}

// ============================================================
// 注册新用户 POST /api/users/create
// ============================================================
void TrainApiClient::createUser(const QString& username, const QString& name,
                                const QString& worknumber, const QString& password,
                                const QString& unit, const QString& cellphone)
{
    QUrl url(m_apiBase + "/api/users/create");
    QNetworkRequest req = makeRequest(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject body;
    body["username"] = username;
    body["name"] = name;
    body["worknumber"] = worknumber;
    body["password"] = password;
    body["unit"] = unit;
    body["cellphone"] = cellphone;
    QNetworkReply* reply = m_nam->post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [=]() {
        handleHttpError(reply);
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QJsonObject root = doc.object();
            emit userCreated(root["success"].toBool(), root["error"].toString());
        } else {
            QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
            emit userCreated(false, root["error"].toString());
        }
        reply->deleteLater();
    });
}

// ============================================================
// 获取统计信息 GET /api/stats
// ============================================================
void TrainApiClient::getStats(const QString& date)
{
    QUrl url(m_apiBase + "/api/stats");
    QUrlQuery query;
    if (!date.isEmpty()) {
        query.addQueryItem("date", date);
    }
    url.setQuery(query);

    // 调试日志
    {
        QFile logFile("E:/Study/AI_Workspace/TrainClient/Client/logs/api_debug.log");
        if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QTextStream ts(&logFile);
            ts << "[getStats] URL: " << url.toString() << ", date param: " << date << "\n";
            ts << "[getStats] Config token: " << (Config::instance()->token().isEmpty() ? "EMPTY" : Config::instance()->token().left(20) + "...") << "\n";
        }
    }

    QNetworkRequest req = makeRequest(QUrl(url));
    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [=]() {
        handleHttpError(reply);
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QString response = QString::fromUtf8(data);

            // 调试日志
            {
                QFile logFile("E:/Study/AI_Workspace/TrainClient/Client/logs/api_debug.log");
                if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
                    QTextStream ts(&logFile);
                    ts << "[getStats] Response: " << response.left(500) << "\n";
                }
            }

            QJsonDocument doc = QJsonDocument::fromJson(data);
            QJsonObject root = doc.object();

            if (root["success"].toBool()) {
                QJsonObject dataObj = root["data"].toObject();
                int todayCount = dataObj["todayTrainCount"].toInt();
                int pendingCount = dataObj["pendingCount"].toInt();
                int alarmCount = dataObj["alarmCount"].toInt();

                // 调试日志
                {
                    QFile logFile("E:/Study/AI_Workspace/TrainClient/Client/logs/api_debug.log");
                    if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
                        QTextStream ts(&logFile);
                        ts << "[getStats] Parsed: todayCount=" << todayCount << ", pendingCount=" << pendingCount << ", alarmCount=" << alarmCount << "\n";
                    }
                }

                emit statsReady(todayCount, pendingCount, alarmCount);
            } else {
                QFile logFile("E:/Study/AI_Workspace/TrainClient/Client/logs/api_debug.log");
                if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
                    QTextStream ts(&logFile);
                    ts << "[getStats] API returned success=false\n";
                }
            }
        } else {
            QFile logFile("E:/Study/AI_Workspace/TrainClient/Client/logs/api_debug.log");
            if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
                QTextStream ts(&logFile);
                ts << "[getStats] HTTP Error: " << reply->errorString() << "\n";
            }
        }
        reply->deleteLater();
    });
}

// ============================================================
// 网络错误处理（含 401 检测）
// ============================================================
void TrainApiClient::handleHttpError(QNetworkReply* reply)
{
    if (!reply) {
        emit error("网络错误");
        return;
    }

    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (statusCode == 401) {
        emit unauthorized();
        emit error("登录已过期，请重新登录");
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        emit error(reply->errorString());
    }
}

QNetworkRequest TrainApiClient::makeRequest(const QUrl& url)
{
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    // 每次发送请求时从 Config 读取最新 token
    QString token = Config::instance()->token();
    
    // 调试日志
    {
        QFile logFile("E:/Study/AI_Workspace/TrainClient/Client/logs/api_debug.log");
        if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QTextStream ts(&logFile);
            ts << "[makeRequest] Config token: " << (token.isEmpty() ? "EMPTY" : token.left(20) + "...") << "\n";
        }
    }
    
    if (!token.isEmpty()) {
        req.setRawHeader("Authorization", ("Bearer " + token).toUtf8());
    }
    return req;
}
