#include "webserver.h"
#include <cstring>
#include <QDateTime>
#include <QUrlQuery>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QRegularExpression>
#include "logger.h"
#include <QTcpServer>
#include <QString>

// ==================== 鍥剧墖璺緞淇閫昏緫 ====================
// 鎵弿 D:/MVS/ 涓嬫墍鏈夋棩鏈熺洰褰曪紝鎸変慨鏀规椂闂存敹闆嗘墍鏈夊疄闄呭瓨鍦ㄧ殑鍥剧墖鏂囦欢
// 鐢ㄤ簬瑙ｅ喅鏁版嵁搴撲腑鏃у浘鐗囪矾寰勪笌瀹為檯鏂囦欢涓嶅尮閰嶇殑闂

static QStringList g_leftImages;
static QStringList g_rightImages;
static QStringList g_topImages;
static bool g_imageCacheInitialized = false;

static void ensureImageCache() {
    if (g_imageCacheInitialized) return;

    QString imgBase = CSystemConfig::instance()->m_strTrainImagesSavePath;
    QDir baseDir(imgBase);

    if (!baseDir.exists()) {
        LOG(QString("鍥剧墖鐩綍涓嶅瓨鍦? %1").arg(imgBase), LOG_WARNING);
        g_imageCacheInitialized = true;
        return;
    }

    // 鑾峰彇鎵€鏈夋棩鏈熷瓙鐩綍锛屾寜鍚嶇О鎺掑簭
    QFileInfoList dateDirs = baseDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    std::sort(dateDirs.begin(), dateDirs.end(),
              [](const QFileInfo &a, const QFileInfo &b) {
                  return a.fileName() < b.fileName();
              });

    for (const QFileInfo &dateDir : dateDirs) {
        QStringList viewTypes = {"left", "right", "top"}; for (const QString &viewType : viewTypes) {
            QDir viewDir(dateDir.filePath() + "/" + viewType);
            if (!viewDir.exists()) continue;

            // 鏀堕泦鎵€鏈?jpg 鏂囦欢锛屾寜淇敼鏃堕棿鎺掑簭
            QFileInfoList files = viewDir.entryInfoList(QStringList() << "*.jpg", QDir::Files);
            std::sort(files.begin(), files.end(),
                      [](const QFileInfo &a, const QFileInfo &b) {
                          return a.lastModified() < b.lastModified();
                      });

            QStringList &targetList = (viewType == "left") ? g_leftImages
                                      : (viewType == "right") ? g_rightImages
                                      : g_topImages;

            for (const QFileInfo &f : files) {
                targetList.append(f.absoluteFilePath().replace('\\', '/'));
            }
        }
    }

    LOG(QString("鍥剧墖缂撳瓨宸插姞杞? left=%1 right=%2 top=%3")
            .arg(g_leftImages.size()).arg(g_rightImages.size()).arg(g_topImages.size()),
        LogLevel::LOG_INFO);
    g_imageCacheInitialized = true;
}

static QString getFixedImagePath(const QString &, int imgIdx, const QStringList &available) {
    if (available.isEmpty()) return "";
    return available[imgIdx % available.size()];
}

WebServer::WebServer(QObject *parent)
    : QObject(parent), m_server(nullptr)
{
}

WebServer::~WebServer() {
    if (m_server) {
        delete m_server;
    }
}

bool WebServer::start(quint16 port) {
    m_tcpServer=std::make_unique<QTcpServer>();
    m_server = new QHttpServer(this);
    setupRoutes();
    if(!m_tcpServer->listen(QHostAddress::Any,port)|| !m_server->bind(m_tcpServer.get())){
            LOG("Web server failed to listen/bind", LogLevel::LOG_ERROR);

        return false;
    }
     m_tcpServer.release();
    LOG(QString("Web鏈嶅姟鍣ㄥ凡鍚姩锛岀洃鍚鍙? %1").arg(port), LogLevel::LOG_INFO);
    return true;
}

void WebServer::setupRoutes() {
    m_server->route("/api/auth/login", QHttpServerRequest::Method::Post,
                    [this](const QHttpServerRequest &request) {
                        QJsonDocument doc = QJsonDocument::fromJson(request.body());
                        if (!doc.isObject()) {
                            return createErrorResponse(400, "鏃犳晥鐨凧SON鏁版嵁");
                        }
                        auto json=handleLogin(doc.object());
                        return json;
                    });

    auto authMiddleware = [this](const QHttpServerRequest &request, auto next) {
        QString token = getTokenFromHeader(request);
        if (!validateToken(token)) {
            return createErrorResponse(401, "鏈巿鏉冩垨token鏃犳晥");
        }
        return next(request, token);
    };

    // 视频/图片静态文件服务（用绝对路径，避免 CWD 问题）
    m_server->route("/trainvideos/<arg>", QHttpServerRequest::Method::Get,
                    [this](const QString &filename, const QHttpServerRequest &) {
        QString videoDir = CSystemConfig::instance()->m_strAreaCameraVideoFilePath;
        QString fullPath = videoDir + "/" + filename;
        // 相对路径 → 以 exe 所在目录为基准解析
        if (!QDir::isAbsolutePath(fullPath)) {
            fullPath = QCoreApplication::applicationDirPath() + "/" + fullPath;
        }
        fullPath = QDir::cleanPath(fullPath);
        LOG("Serving video: " + fullPath, LogLevel::LOG_INFO);
        return handleStaticFile(fullPath);
    });

    m_server->route("/trainimages/<arg>", QHttpServerRequest::Method::Get,
                    [this](const QString &filename, const QHttpServerRequest &) {
        QString imgDir = CSystemConfig::instance()->m_strTrainImagesSavePath;
        QString fullPath = imgDir + "/" + filename;
        if (!QDir::isAbsolutePath(fullPath)) {
            fullPath = QCoreApplication::applicationDirPath() + "/" + fullPath;
        }
        fullPath = QDir::cleanPath(fullPath);
        return handleStaticFile(fullPath);
    });

    m_server->route("/api/trains", QHttpServerRequest::Method::Get,
                    [this, authMiddleware](const QHttpServerRequest &request) {
                        return authMiddleware(request, [this](const QHttpServerRequest &request, const QString &token) {
                            auto params = getQueryParams(request.url());
                            return handleGetTrains(params, token);
                        });
                    });

    m_server->route("/api/trains/<arg>/inspection", QHttpServerRequest::Method::Put,
                    [this, authMiddleware](int trainId, const QHttpServerRequest &request) {
                        return authMiddleware(request, [this, trainId](const QHttpServerRequest &request, const QString &token) {
                            QJsonDocument doc = QJsonDocument::fromJson(request.body());
                            if (!doc.isObject()) {
                                return createErrorResponse(400, "鏃犳晥鐨凧SON鏁版嵁");
                            }
                            return handleUpdateTrainInspection(doc.object(), token, trainId);
                        });
                    });

    m_server->route("/api/carriages", QHttpServerRequest::Method::Get,
                    [this, authMiddleware](const QHttpServerRequest &request) {
                        return authMiddleware(request, [this](const QHttpServerRequest &request, const QString &token) {
                            auto params = getQueryParams(request.url());
                            return handleGetCarriages(params, token);
                        });
                    });

    m_server->route("/api/carriages/<arg>/inspection", QHttpServerRequest::Method::Put,
                    [this, authMiddleware](int carriageId, const QHttpServerRequest &request) {
                        return authMiddleware(request, [this, carriageId](const QHttpServerRequest &request, const QString &token) {
                            QJsonDocument doc = QJsonDocument::fromJson(request.body());
                            if (!doc.isObject()) {
                                return createErrorResponse(400, "鏃犳晥鐨凧SON鏁版嵁");
                            }
                            return handleUpdateCarriageInspection(doc.object(), token, carriageId);
                        });
                    });

    // 提交车厢检视结果（按复合键定位：trainNumber + carriageNumber + reachDatetime）
    m_server->route("/api/carriages/inspection", QHttpServerRequest::Method::Put,
                    [this, authMiddleware](const QHttpServerRequest &request) {
                        return authMiddleware(request, [this](const QHttpServerRequest &request, const QString &token) {
                            auto params = getQueryParams(request.url());
                            QJsonDocument doc = QJsonDocument::fromJson(request.body());
                            if (!doc.isObject()) {
                                return createErrorResponse(400, "无效的JSON数据");
                            }
                            return handleUpdateCarriageInspectionByKey(doc.object(), token, params);
                        });
                    });

    // 获取车厢标注 GET /api/carriages/<trainNumber>/<reachDatetime>/annotation?order=N
    m_server->route("/api/carriages/<arg>/<arg>/annotation", QHttpServerRequest::Method::Get,
                    [this, authMiddleware](const QString &trainNumber, const QString &reachDatetimeStr,
                                           const QHttpServerRequest &request) {
                        return authMiddleware(request, [this, trainNumber, reachDatetimeStr](const QHttpServerRequest &request, const QString &token) {
                            auto params = getQueryParams(request.url());
                            return handleGetCarriageAnnotation(trainNumber, reachDatetimeStr.toLongLong(), params, token);
                        });
                    });

    // 保存车厢标注 PUT /api/carriages/<trainNumber>/<reachDatetime>/<arg>/annotation
    m_server->route("/api/carriages/<arg>/<arg>/<arg>/annotation", QHttpServerRequest::Method::Put,
                    [this, authMiddleware](const QString &trainNumber, const QString &reachDatetimeStr,
                                           const QString &orderStr, const QHttpServerRequest &request) {
                        return authMiddleware(request, [this, trainNumber, reachDatetimeStr, orderStr](const QHttpServerRequest &request, const QString &token) {
                            QJsonDocument doc = QJsonDocument::fromJson(request.body());
                            if (!doc.isObject()) {
                                return createErrorResponse(400, "无效的JSON数据");
                            }
                            return handleSaveCarriageAnnotation(trainNumber, reachDatetimeStr.toLongLong(),
                                                                orderStr.toInt(), doc.object(), token);
                        });
                    });

    m_server->route("/api/stats", QHttpServerRequest::Method::Get,
                    [this, authMiddleware](const QHttpServerRequest &request) {
                        return authMiddleware(request, [this](const QHttpServerRequest &request, const QString &token) {
                            auto params = getQueryParams(request.url());
                            return handleGetStats(params, token);
                        });
                    });

    m_server->route("/api/users/list", QHttpServerRequest::Method::Get,
                    [this, authMiddleware](const QHttpServerRequest &request) {
                        return authMiddleware(request, [this](const QHttpServerRequest &request, const QString &token) {
                            auto params = getQueryParams(request.url());
                            return handleGetUsers(params, token);
                        });
                    });

    m_server->route("/api/users/info/<arg>", QHttpServerRequest::Method::Get,
                    [this, authMiddleware](const QString &username, const QHttpServerRequest &request) {
                        return authMiddleware(request, [this, username](const QHttpServerRequest &request, const QString &token) {
                            return handleGetUser(username, token);
                        });
                    });

    m_server->route("/api/users/create", QHttpServerRequest::Method::Post,
                    [this, authMiddleware](const QHttpServerRequest &request) {
                        return authMiddleware(request, [this](const QHttpServerRequest &request, const QString &token) {
                            QJsonDocument doc = QJsonDocument::fromJson(request.body());
                            if (!doc.isObject()) {
                                return createErrorResponse(400, "鏃犳晥鐨凧SON鏁版嵁");
                            }
                            return handleCreateUser(doc.object(), token);
                        });
                    });

    m_server->route("/api/users/<arg>", QHttpServerRequest::Method::Put,
                    [this, authMiddleware](const QString &username, const QHttpServerRequest &request) {
                        return authMiddleware(request, [this, username](const QHttpServerRequest &request, const QString &token) {
                            QJsonDocument doc = QJsonDocument::fromJson(request.body());
                            if (!doc.isObject()) {
                                return createErrorResponse(400, "鏃犳晥鐨凧SON鏁版嵁");
                            }
                            return handleUpdateUser(doc.object(), token, username);
                        });
                    });

    // 改当前用户密码（具体路由必须注册在 <arg>/password 参数路由之前）
    m_server->route("/api/users/me/password", QHttpServerRequest::Method::Put,
                    [this, authMiddleware](const QHttpServerRequest &request) {
                        return authMiddleware(request, [this](const QHttpServerRequest &request, const QString &token) {
                            QJsonDocument doc = QJsonDocument::fromJson(request.body());
                            if (!doc.isObject()) {
                                return createErrorResponse(400, "鏃犳晥鐨凧SON鏁版嵁");
                            }
                            return handleChangePassword(doc.object(), token);
                        });
                    });

    // 重置任意用户密码（管理员）
    m_server->route("/api/users/<arg>/password", QHttpServerRequest::Method::Put,
                    [this, authMiddleware](const QString &username, const QHttpServerRequest &request) {
                        return authMiddleware(request, [this, username](const QHttpServerRequest &request, const QString &token) {
                            QJsonDocument doc = QJsonDocument::fromJson(request.body());
                            if (!doc.isObject()) {
                                return createErrorResponse(400, "鏃犳晥鐨凧SON鏁版嵁");
                            }
                            return handleResetPassword(doc.object(), token, username);
                        });
                    });

    m_server->route("/api/users/forbiden/<arg>", QHttpServerRequest::Method::Put,
                    [this, authMiddleware](const QString &username, const QHttpServerRequest &request) {
                        return authMiddleware(request, [this, username](const QHttpServerRequest &request, const QString &token) {
                            QJsonDocument doc = QJsonDocument::fromJson(request.body());
                            if (!doc.isObject()) {
                                return createErrorResponse(400, "鏃犳晥鐨凧SON鏁版嵁");
                            }
                            return handleSetUserStatus(doc.object(), token, username);
                        });
                    });

    m_server->route("/trainimages/<arg>", [] (const QUrl &url) {
        return QHttpServerResponse::fromFile(CSystemConfig::instance()->m_strTrainImagesSavePath+"/" + url.path());
    });
    m_server->route("/trainvideos/<arg>", [] (const QUrl &url) {
        QString path = url.path();
        if (path.startsWith("/trainvideos/"))
            path = path.mid(14);
        // Resolve relative path from config against application directory
        QString configPath = CSystemConfig::instance()->m_strAreaCameraVideoFilePath;
        if (!QFileInfo(configPath).isAbsolute())
            configPath = QCoreApplication::applicationDirPath() + "/" + configPath;
        QString fullPath = QDir::toNativeSeparators(QDir::cleanPath(configPath + "/" + path));
        return QHttpServerResponse::fromFile(fullPath);
    });
}

// 璁よ瘉鎺ュ彛
QHttpServerResponse WebServer::handleLogin(const QJsonObject &request) {
    QString username = request["username"].toString();
    QString password = request["password"].toString();

    if (username.isEmpty() || password.isEmpty()) {
        return createErrorResponse(400, "鐢ㄦ埛鍚嶅拰瀵嗙爜涓嶈兘涓虹┖");
    }

    QMap<QString, QVariant> result = CDatabaseManager::Instance()->authenticateUser(username, password);
    auto json= QJsonObject::fromVariantMap(result);
    return createJsonResponse(result["success"].toBool() ? 200 : 401,json);
}

// 鑾峰彇鍒楄溅鍒楄〃
QHttpServerResponse WebServer::handleGetTrains(const QMap<QString, QString> &queryParams, const QString &token) {
    Q_UNUSED(token)

    QMap<QString, QVariant> result = CDatabaseManager::Instance()->getTrainList(queryParams);
    auto dataObj = result["data"].toJsonObject();
    QList<QVariant> trains = dataObj["trains"].toArray().toVariantList();
    QList<QVariant> modifiedTrains;

    for (const QVariant &trainVar : trains) {
        QMap<QString, QVariant> train = trainVar.toMap();

        QString leftImagePath = train["strLeftAreaCameraPath"].toString();
        QString topImagePath = train["strTopAreaCameraPath"].toString();
        QString rightImagePath = train["strRightAreaCameraPath"].toString();

        if (!leftImagePath.isEmpty()) {
            train["strLeftAreaCameraPath"] = convertToWebVideoPath(leftImagePath);
        }
        if (!topImagePath.isEmpty()) {
            train["strTopAreaCameraPath"] = convertToWebVideoPath(topImagePath);
        }
        if (!rightImagePath.isEmpty()) {
            train["strRightAreaCameraPath"] = convertToWebVideoPath(rightImagePath);
        }

        modifiedTrains.append(train);
    }

    dataObj["trains"] = QJsonArray::fromVariantList(modifiedTrains);
    result["data"] = dataObj;
    return createJsonResponse(result["success"].toBool() ? 200 : 500, QJsonObject::fromVariantMap(result));
}

// Update train inspection status
QHttpServerResponse WebServer::handleUpdateTrainInspection(const QJsonObject &request, const QString &token, int trainId) {
    Q_UNUSED(token)

    QString status = request["strInspectionStatus"].toString();
    quint64 inspectionTime = request["nInspectionDatetime"].toVariant().toULongLong();

    bool success = CDatabaseManager::Instance()->updateTrainInspectionStatus(trainId, status, inspectionTime);

    QJsonObject response;
    response["success"] = success;
    return createJsonResponse(success ? 200 : 500, response);
}

// 鑾峰彇杞﹀帰淇℃伅
QHttpServerResponse WebServer::handleGetCarriages(const QMap<QString, QString> &queryParams, const QString &token) {
    Q_UNUSED(token)

    QString trainNumber = queryParams.value("trainNumber");
    QString reachDatetimeStr = queryParams.value("reachDatetime");

    if (trainNumber.isEmpty() || reachDatetimeStr.isEmpty()) {
        return createErrorResponse(400, "缂哄皯蹇呰鍙傛暟: trainNumber 鍜?reachDatetime");
    }

    bool ok;
    quint64 reachDatetime = reachDatetimeStr.toULongLong(&ok);
    if (!ok || reachDatetime == 0) {
        return createErrorResponse(400, "鍒拌揪鏃堕棿鏍煎紡閿欒");
    }

    // 纭繚鍥剧墖缂撳瓨宸插垵濮嬪寲
    ensureImageCache();

    // Resolve relative path to absolute for correct stripping
    QString imgBase = CSystemConfig::instance()->m_strTrainImagesSavePath;
    if (!QDir::isAbsolutePath(imgBase)) {
        imgBase = QDir::cleanPath(QCoreApplication::applicationDirPath() + "/" + imgBase);
    }
    qDebug() << "[handleGetCarriages] imgBase:" << imgBase;


    QMap<QString, QVariant> result = CDatabaseManager::Instance()->getCarriageList(trainNumber, reachDatetime);

    if (result["success"].toBool()) {
        QList<QVariant> carriages = (result["data"].toJsonObject())["carriages"].toArray().toVariantList();
        QList<QVariant> modifiedCarriages;

        int imgIdx = 0;
        for (const QVariant &carriageVar : carriages) {
            QMap<QString, QVariant> carriage = carriageVar.toMap();

            // Replace DB paths with scanned real image paths.
            QString fixedLeft  = getFixedImagePath("", imgIdx, g_leftImages);
            QString fixedRight = getFixedImagePath("", imgIdx, g_rightImages);
            QString fixedTop   = getFixedImagePath("", imgIdx, g_topImages);

            // imagePathMap stores HTTP paths (same as leftImagePath) for Qt desktop client.
            QMap<QString, QVariant> imagePathMap;
            if (!fixedLeft.isEmpty()) {
                QString rp = fixedLeft.mid(imgBase.length());
                if (rp.startsWith('/') || rp.startsWith('\\')) rp = rp.mid(1);
                imagePathMap["left"] = "/trainimages/" + rp;
            }
            if (!fixedTop.isEmpty()) {
                QString rp = fixedTop.mid(imgBase.length());
                if (rp.startsWith('/') || rp.startsWith('\\')) rp = rp.mid(1);
                imagePathMap["top"] = "/trainimages/" + rp;
            }
            if (!fixedRight.isEmpty()) {
                QString rp = fixedRight.mid(imgBase.length());
                if (rp.startsWith('/') || rp.startsWith('\\')) rp = rp.mid(1);
                imagePathMap["right"] = "/trainimages/" + rp;
            }
            carriage["imagePathMap"] = imagePathMap;

            // Web paths for HTTP access (must use imagePathMap values for Qt client compatibility).
            carriage["leftImagePath"]  = imagePathMap.value("left").toString();
            carriage["topImagePath"]    = imagePathMap.value("top").toString();
            carriage["rightImagePath"]  = imagePathMap.value("right").toString();

            // Ensure lowercase keys for Top/Right PASS fields to match client expectations
            if (carriage.contains("TopImagePass"))
                carriage["topImagePass"] = carriage["TopImagePass"];
            if (carriage.contains("RightImagePass"))
                carriage["rightImagePass"] = carriage["RightImagePass"];

            modifiedCarriages.append(carriage);
            imgIdx++;
        }

        result["carriages"] = modifiedCarriages;
    }

    return createJsonResponse(result["success"].toBool() ? 200 : 500, QJsonObject::fromVariantMap(result));
}

// Update carriage inspection status
QHttpServerResponse WebServer::handleUpdateCarriageInspection(const QJsonObject &request, const QString &token, int carriageId) {
    Q_UNUSED(token)

    QMap<QString, QVariant> inspectionData;
    inspectionData["inspectorState"] = request["inspectorState"].toInt();
    inspectionData["leftImagePass"] = request["leftImagePass"].toInt();
    inspectionData["TopImagePass"] = request["TopImagePass"].toInt();
    inspectionData["RightImagePass"] = request["RightImagePass"].toInt();
    inspectionData["inspectorNote"] = request["inspectorNote"].toString();

    bool success = CDatabaseManager::Instance()->updateCarriageInspection(carriageId, inspectionData);
    CDatabaseManager::Instance()->updateInspectionStatus(carriageId);

    QJsonObject response;
    response["success"] = success;
    return createJsonResponse(success ? 200 : 500, response);
}

// find carriage id by composite key using CDatabaseManager public API
static int getCarriageIdByKey(const QString &trainNumber, quint64 reachDatetime, int nOrder)
{
    QMap<QString, QVariant> result = CDatabaseManager::Instance()->getCarriageList(trainNumber, reachDatetime);
    QMap<QString, QVariant> data = result["data"].toMap();
    QList<QVariant> carriages = data["carriages"].toList();
    for (const QVariant &v : carriages) {
        QMap<QString, QVariant> c = v.toMap();
        if (c["nOrder"].toInt() == nOrder) {
            return c["id"].toInt();
        }
    }
    return -1;
}

// submit carriage inspection by composite key (trainNumber + carriageNumber + reachDatetime)
QHttpServerResponse WebServer::handleUpdateCarriageInspectionByKey(
    const QJsonObject &request, const QString &token, const QMap<QString, QString> &queryParams)
{
    Q_UNUSED(token);

    QString trainNumber = queryParams.value("trainNumber");
    QString carriageNumberStr = queryParams.value("carriageNumber");
    QString reachDatetimeStr = queryParams.value("reachDatetime");

    if (trainNumber.isEmpty() || carriageNumberStr.isEmpty() || reachDatetimeStr.isEmpty()) {
        QJsonObject err;
        err["success"] = false;
        err["error"] = "missing parameters: trainNumber, carriageNumber, reachDatetime required";
        return createJsonResponse(400, err);
    }

    bool ok1, ok2;
    int carriageNumber = carriageNumberStr.toInt(&ok1);
    quint64 reachDatetime = reachDatetimeStr.toULongLong(&ok2);
    if (!ok1 || !ok2) {
        QJsonObject err;
        err["success"] = false;
        err["error"] = "invalid parameter format";
        return createJsonResponse(400, err);
    }

    int carriageId = getCarriageIdByKey(trainNumber, reachDatetime, carriageNumber);
    if (carriageId < 0) {
        QJsonObject err;
        err["success"] = false;
        err["error"] = QString("carriage not found: %1 reachDatetime=%2 nOrder=%3").arg(trainNumber).arg(reachDatetime).arg(carriageNumber);
        return createJsonResponse(404, err);
    }

    // field mapping: frontend sends leftPassed/rightPassed/topPassed/remarks/alarms
    bool leftPassed = request["leftPassed"].toBool();
    bool rightPassed = request["rightPassed"].toBool();
    bool topPassed = request["topPassed"].toBool();
    bool allPassed = leftPassed && rightPassed && topPassed;

    QMap<QString, QVariant> inspectionData;

    // 如果客户端明确传了 inspectorState，使用它（支持值2=不合格已检）
    // 否则根据三视图通过情况自动计算（0=未检, 1=合格）
    if (request.contains("inspectorState")) {
        inspectionData["inspectorState"] = request["inspectorState"].toInt();
    } else {
        inspectionData["inspectorState"] = allPassed ? 1 : 0;
    }

    // 三视图通过状态
    inspectionData["leftImagePass"] = leftPassed ? 1 : 0;
    inspectionData["RightImagePass"] = rightPassed ? 1 : 0;
    inspectionData["TopImagePass"] = topPassed ? 1 : 0;

    QString remarks = request["remarks"].toString();
    QJsonArray alarms = request["alarms"].toArray();
    QStringList alarmList;
    for (const QJsonValue &v : alarms) alarmList.append(v.toString());
    if (!alarmList.isEmpty()) {
        remarks = (remarks.isEmpty() ? "" : remarks + "; ") + "alarms: " + alarmList.join("; ");
    }
    inspectionData["inspectorNote"] = remarks;

    bool success = CDatabaseManager::Instance()->updateCarriageInspection(carriageId, inspectionData);
    CDatabaseManager::Instance()->updateInspectionStatus(carriageId);

    QJsonObject response;
    response["success"] = success;
    return createJsonResponse(success ? 200 : 500, response);
}

// 获取统计信息
QHttpServerResponse WebServer::handleGetStats(const QMap<QString, QString> &queryParams, const QString &token) {
    Q_UNUSED(token)

    QString date = queryParams.value("date");
    if (date.isEmpty()) {
        // 默认当天
        date = QDate::currentDate().toString("yyyy-MM-dd");
    }

    QMap<QString, QVariant> result = CDatabaseManager::Instance()->getStats(date);
    return createJsonResponse(result["success"].toBool() ? 200 : 500, QJsonObject::fromVariantMap(result));
}

// 获取车厢标注 GET /api/carriages/<trainNumber>/<reachDatetime>/annotation?order=N
QHttpServerResponse WebServer::handleGetCarriageAnnotation(
    const QString &trainNumber, qint64 reachDatetime,
    const QMap<QString, QString> &queryParams, const QString &token)
{
    Q_UNUSED(token)
    Q_UNUSED(reachDatetime)

    int order = queryParams.value("order").toInt();
    if (order <= 0) {
        return createErrorResponse(400, "Invalid order parameter");
    }

    QMap<QString, QVariant> result = CDatabaseManager::Instance()->getCarriageAnnotation(
        trainNumber, reachDatetime, order);

    if (!result["success"].toBool() && result["found"].toBool() == false) {
        // 没有找到标注，返回空（正常情况）
        QJsonObject emptyData;
        emptyData["nOrder"] = order;
        emptyData["strCarriageModel"] = "";
        emptyData["strDefectTypes"] = "[]";
        emptyData["strDefectRects"] = "[]";
        emptyData["strAnnotator"] = "";
        return createJsonResponse(200, emptyData);
    }

    return createJsonResponse(result["success"].toBool() ? 200 : 500,
                              QJsonObject::fromVariantMap(result));
}

// 保存车厢标注 PUT /api/carriages/<trainNumber>/<reachDatetime>/<order>/annotation
QHttpServerResponse WebServer::handleSaveCarriageAnnotation(
    const QString &trainNumber, qint64 reachDatetime, int order,
    const QJsonObject &data, const QString &token)
{
    Q_UNUSED(token)

    bool success = CDatabaseManager::Instance()->saveCarriageAnnotation(
        trainNumber, reachDatetime, order, data);

    QJsonObject response;
    response["success"] = success;
    response["message"] = success ? "Annotation saved" : "Failed to save annotation";
    return createJsonResponse(success ? 200 : 500, response);
}

// 鑾峰彇鐢ㄦ埛鍒楄〃
QHttpServerResponse WebServer::handleGetUsers(const QMap<QString, QString> &queryParams, const QString &token) {
    Q_UNUSED(token)

    QMap<QString, QVariant> result = CDatabaseManager::Instance()->getUserList(queryParams);
    return createJsonResponse(result["success"].toBool() ? 200 : 500, QJsonObject::fromVariantMap(result));
}

// 鑾峰彇鍗曚釜鐢ㄦ埛淇℃伅
QHttpServerResponse WebServer::handleGetUser(const QString &username, const QString &token) {
    Q_UNUSED(token)

    QMap<QString, QVariant> result = CDatabaseManager::Instance()->getUserByUsername(username);
    return createJsonResponse(result["success"].toBool() ? 200 : 404, QJsonObject::fromVariantMap(result));
}

// 鍒涘缓鐢ㄦ埛
QHttpServerResponse WebServer::handleCreateUser(const QJsonObject &request, const QString &token) {
    Q_UNUSED(token)

    QMap<QString, QVariant> userData;
    userData["username"] = request["username"].toString();
    userData["name"] = request["name"].toString();
    userData["worknumber"] = request["worknumber"].toString();
    userData["password"] = request["password"].toString();
    userData["cellphone"] = request["cellphone"].toString();
    userData["unit"] = request["unit"].toString();
    userData["status"] = request.value("status").toInt();
    userData["remark"] = request["remark"].toString();

    bool success = CDatabaseManager::Instance()->createUser(userData);

    QJsonObject response;
    response["success"] = success;
    if (success) {
        response["data"] = request;
    } else {
        response["error"] = "鍒涘缓鐢ㄦ埛澶辫触";
    }
    return createJsonResponse(success ? 200 : 500, response);
}

// 鏇存柊鐢ㄦ埛淇℃伅
QHttpServerResponse WebServer::handleUpdateUser(const QJsonObject &request, const QString &token, const QString &username) {
    LOG("handleUpdateUser called with username=" + username, LogLevel::LOG_INFO);
    Q_UNUSED(token)

    // 先查当前用户完整信息，保留不变字段避免 NOT NULL 约束冲突
    QMap<QString, QVariant> existingResult = CDatabaseManager::Instance()->getUserByUsername(username);
    if (!existingResult["success"].toBool()) {
        QJsonObject response;
        response["success"] = false;
        response["error"] = "用户不存在";
        return createJsonResponse(404, response);
    }

    QMap<QString, QVariant> existingData = existingResult["data"].toMap();
    QMap<QString, QVariant> userData;
    // 只读字段保留原值，可写字段用请求值覆盖
    userData["name"] = existingData["name"];
    userData["worknumber"] = existingData["worknumber"];
    userData["cellphone"] = request["cellphone"].toString();
    userData["unit"] = request["unit"].toString();
    userData["remark"] = existingData["remark"];

    bool success = CDatabaseManager::Instance()->updateUser(username, userData);

    QJsonObject response;
    response["success"] = success;
    if (success) {
        QMap<QString, QVariant> userInfo = CDatabaseManager::Instance()->getUserByUsername(username)["data"].toMap();
        response["data"] = QJsonObject::fromVariantMap(userInfo);
    }
    return createJsonResponse(success ? 200 : 500, response);
}

// 閲嶇疆鐢ㄦ埛瀵嗙爜
QHttpServerResponse WebServer::handleResetPassword(const QJsonObject &request, const QString &token, const QString &username) {
    Q_UNUSED(token)

    QString newPassword = request["newPassword"].toString();

    bool success = CDatabaseManager::Instance()->resetUserPassword(username, newPassword);

    QJsonObject response;
    response["success"] = success;
    if (success) {
        response["message"] = "瀵嗙爜閲嶇疆鎴愬姛";
    }
    return createJsonResponse(success ? 200 : 500, response);
}

// Set user status
QHttpServerResponse WebServer::handleSetUserStatus(const QJsonObject &request, const QString &token, const QString &username) {
    Q_UNUSED(token)

    int status = request["status"].toInt();

    bool success = CDatabaseManager::Instance()->setUserStatus(username, status);

    QJsonObject response;
    response["success"] = success;
    if (success) {
        response["message"] = status == USER_DISABLED ? "鐢ㄦ埛绂佺敤鎴愬姛" : "鐢ㄦ埛鍚敤鎴愬姛";
    }
    return createJsonResponse(success ? 200 : 500, response);
}

// 淇敼褰撳墠鐢ㄦ埛瀵嗙爜
QHttpServerResponse WebServer::handleChangePassword(const QJsonObject &request, const QString &token) {
    LOG("handleChangePassword called, token len=" + QString::number(token.length()), LogLevel::LOG_INFO);
    QString currentPassword = request["currentPassword"].toString();
    QString newPassword = request["newPassword"].toString();

    QString username = CDatabaseManager::Instance()->getUsernameByToken(token);
    if (username.isEmpty()) {
        QJsonObject response;
        response["success"] = false;
        response["error"] = "无效的会话，请重新登录";
        return createJsonResponse(401, response);
    }

    bool success = CDatabaseManager::Instance()->changeUserPassword(username, currentPassword, newPassword);

    QJsonObject response;
    response["success"] = success;
    if (success) {
        response["message"] = "瀵嗙爜淇敼鎴愬姛";
    } else {
        response["error"] = "褰撳墠瀵嗙爜閿欒";
    }
    return createJsonResponse(success ? 200 : 400, response);
}

// Handle static files
QHttpServerResponse WebServer::handleStaticFile(const QString &filePath) {
    QFile file(filePath);
    if (!file.exists()) {
        return createErrorResponse(404, "File not found");
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return createErrorResponse(500, "鏃犳硶璇诲彇鏂囦欢");
    }

    QFileInfo fileInfo(filePath);
    QMimeType mimeType = m_mimeDb.mimeTypeForFile(filePath);
    QString contentType = mimeType.name();

    if (contentType == "application/octet-stream") {
        QString extension = fileInfo.suffix().toLower();
        if (extension == "mp4" || extension == "avi" || extension == "mkv") {
            contentType = "video/mp4";
        } else if (extension == "jpg" || extension == "jpeg") {
            contentType = "image/jpeg";
        } else if (extension == "png") {
            contentType = "image/png";
        }
    }

    QHttpServerResponse response(contentType.toUtf8(), file.readAll());
    file.close();

    LOG(filePath + " sent", LogLevel::LOG_INFO);
    return response;
}

// 鑼冨洿璇锋眰澶勭悊
QHttpServerResponse WebServer::handleRangeRequest(const QString &filePath, const QString &rangeHeader) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return createErrorResponse(500, "Cannot open file");
    }

    qint64 fileSize = file.size();
    qint64 start = 0;
    qint64 end = fileSize - 1;
    qint64 chunkSize = end - start + 1;

    if (!rangeHeader.isEmpty() && rangeHeader.startsWith("bytes=")) {
        QString range = rangeHeader.mid(6);
        QStringList ranges = range.split("-");
        if (ranges.size() == 2) {
            start = ranges[0].toLongLong();
            if (!ranges[1].isEmpty()) {
                end = ranges[1].toLongLong();
            }
            chunkSize = end - start + 1;
        }
    }

    file.seek(start);

    QMimeType mimeType = m_mimeDb.mimeTypeForFile(filePath);
    QString contentType = mimeType.name();

    QByteArray fileData = file.read(chunkSize);
    file.close();

    QHttpServerResponse response(contentType.toUtf8(), fileData);
    auto headers=response.headers();
    headers.append("Content-Range",
                   QString("bytes %1-%2/%3").arg(start).arg(end).arg(fileSize).toUtf8());
    headers.append("Accept-Ranges", "bytes");
    response.setHeaders(std::move(headers));
    LOG(QString("鍒嗗潡鍙戦€佸畬姣曪細寮€濮嬩綅缃細%1").arg(start), LogLevel::LOG_INFO);
    return response;
}

// 宸ュ叿鍑芥暟
QMap<QString, QString> WebServer::getQueryParams(const QUrl &url) {
    QMap<QString, QString> params;
    QUrlQuery query(url);

    for (const auto &item : query.queryItems()) {
        params.insert(item.first, item.second);
    }

    return params;
}

bool WebServer::validateToken(const QString &token) {
    return CDatabaseManager::Instance()->validateToken(token);
}

QString WebServer::getTokenFromHeader(const QHttpServerRequest &request) {
    QString authHeader = request.value("Authorization");
    if (authHeader.isEmpty()) {
        authHeader = request.value("authorization");
    }

    if (authHeader.startsWith("Bearer ")) {
        return authHeader.mid(7);
    }
    return QString();
}

QString WebServer::convertToWebImagePath(QString &localImagePath) {
    if (localImagePath.isEmpty()) {
        return localImagePath;
    }

    QString basePath = CSystemConfig::instance()->m_strTrainImagesSavePath;
    QString modifyBasePath = basePath.replace('\\', '/');
    QString modifyLocalPath = localImagePath.replace('\\', '/');

    if (modifyLocalPath.startsWith(modifyBasePath)) {
        QString relativePath = modifyLocalPath.mid(modifyBasePath.length());
        if (relativePath.startsWith('/') || relativePath.startsWith('\\')) {
            relativePath = relativePath.mid(1);
        }
        return "/trainimages/" + relativePath;
    }

    return localImagePath;
}

QString WebServer::convertToWebVideoPath(QString &localVideoPath) {
    if (localVideoPath.isEmpty()) {
        return localVideoPath;
    }

    QString basePath = CSystemConfig::instance()->m_strAreaCameraVideoFilePath;
    // Resolve relative config path to absolute (relative to exe dir)
    if (!QDir::isAbsolutePath(basePath)) {
        basePath = QDir::cleanPath(QCoreApplication::applicationDirPath() + "/" + basePath);
    }

    // Also resolve DB path if it's relative (relative to exe dir)
    QString resolvedPath = QDir::fromNativeSeparators(localVideoPath);
    if (!QDir::isAbsolutePath(resolvedPath)) {
        resolvedPath = QDir::cleanPath(QCoreApplication::applicationDirPath() + "/" + localVideoPath);
    }

    QString modifyBasePath = QDir::fromNativeSeparators(basePath).toLower();
    QString modifyLocalPath = resolvedPath.toLower();

    // Case 1: DB path starts with basePath -> strip prefix to get web path
    if (modifyLocalPath.startsWith(modifyBasePath)) {
        QString relativePath = modifyLocalPath.mid(modifyBasePath.length());
        if (relativePath.startsWith('/') || relativePath.startsWith('\\')) {
            relativePath = relativePath.mid(1);
        }
        return "/trainvideos/" + relativePath;
    }

    // Case 2: path mismatch, but file may exist in video dir by filename alone
    QFileInfo fi(localVideoPath);
    QString fileName = fi.fileName();
    if (!fileName.isEmpty()) {
        QString candidatePath = basePath + "/" + fileName;
        if (QFile::exists(candidatePath)) {
            return "/trainvideos/" + fileName;
        }
    }

    // Case 3: no match at all - return original (frontend will get broken URL)
    return localVideoPath;
}

QHttpServerResponse WebServer::createJsonResponse(int statusCode, const QJsonObject &data) {
    QHttpServerResponse response(data);
    return response;
}

QHttpServerResponse WebServer::createErrorResponse(int statusCode, const QString &message) {
    QJsonObject obj;
    obj["success"] = false;
    obj["error"] = message;
    QHttpServerResponse response(obj);
    return response;
}