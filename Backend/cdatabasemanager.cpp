#include <QSqlQuery>
#include <QStandardPaths>
#include <QDateTime>
#include <QCryptographicHash>
#include <QUuid>
#include <QJsonObject>
#include <QJsonArray>
#include "cdatabasemanager.h"
#include "logger.h"

CDatabaseManager *CDatabaseManager::m_instance = nullptr;

CDatabaseManager::CDatabaseManager() {
    // 连接数据库
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("appdata.db");

    if (!db.open()) {
        LOG("Error:" + db.lastError().text(), LogLevel::LOG_ERROR);
    }

    // 创建各表
    createTrainInformationTable();
    createLocomotiveTable();
    createCarriageTable();
    createUserTable();
    createTokenTable();
}

// 创建token表用于认证
bool CDatabaseManager::createTokenTable() {
    QSqlQuery checkQuery(db);
    if (!checkQuery.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='user_tokens'")) {
        LOG("Token表检查失败:" + checkQuery.lastError().text(), LogLevel::LOG_WARNING);
        return false;
    }

    if (checkQuery.next()) {
        return true;
    }

    QString createSQL = R"(
        CREATE TABLE user_tokens (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT NOT NULL,
            token TEXT UNIQUE NOT NULL,
            created_time INTEGER NOT NULL,
            expires_time INTEGER NOT NULL,
            FOREIGN KEY(username) REFERENCES users(username)
        )
    )";

    QSqlQuery createQuery(db);
    if (!createQuery.exec(createSQL)) {
        LOG("Token表创建失败:" + createQuery.lastError().text(), LogLevel::LOG_ERROR);
        return false;
    }

    LOG("Token表创建成功");
    return true;
}

// 用户认证
QMap<QString, QVariant> CDatabaseManager::authenticateUser(const QString &username, const QString &password) {
    QMap<QString, QVariant> result;

    if (!usernameExists(username)) {
        result["success"] = false;
        result["error"] = "用户不存在";
        return result;
    }

    QString encryptedPassword = encryptPassword(password);
    QSqlQuery query(db);
    query.prepare("SELECT username, name, worknumber, cellphone, unit, status, remark, role FROM users WHERE username = :username AND password = :password");
    query.bindValue(":username", username);
    query.bindValue(":password", encryptedPassword);

    if (!query.exec()) {
        LOG("认证查询失败: " + query.lastError().text(), LogLevel::LOG_ERROR);
        result["success"] = false;
        result["error"] = "数据库错误";
        return result;
    }

    if (query.next()) {
        // 检查用户状态
        int status = query.value("status").toInt();
        if (status == USER_DISABLED) {
            result["success"] = false;
            result["error"] = "用户已被禁用";
            return result;
        }

        QString token;
        quint64 createdTime = QDateTime::currentMSecsSinceEpoch();
        quint64 expiresTime = createdTime + 24 * 3600; // 24小时过期

        // 检查内存中是否有现有token
        if (m_userTokens.contains(username)) {
            QMap<QString, QVariant> tokenInfo = m_userTokens[username].toMap();
            quint64 existingExpiresTime = tokenInfo["expires_time"].toULongLong();

            // 检查token是否过期
            if (createdTime < existingExpiresTime) {
                // token未过期，返回现有token
                token = tokenInfo["token"].toString();
                LOG("使用现有token: " + token, LogLevel::LOG_INFO);
            } else {
                // token已过期，从数据库中删除
                LOG("Token已过期，删除旧token", LogLevel::LOG_INFO);
                deleteTokenFromDatabase(username);
                // 生成新token
                token = generateToken();
                saveTokenToMemory(username, token, createdTime, expiresTime);
                saveTokenToDatabase(username, token, createdTime, expiresTime);
            }
        } else {
            // 内存中没有token，检查数据库中是否有未过期的token
            QSqlQuery tokenQuery(db);
            tokenQuery.prepare("SELECT token, expires_time FROM user_tokens WHERE username = :username");
            tokenQuery.bindValue(":username", username);

            if (tokenQuery.exec() && tokenQuery.next()) {
                quint64 existingExpiresTime = tokenQuery.value("expires_time").toULongLong();

                if (createdTime < existingExpiresTime) {
                    // 数据库中的token未过期
                    token = tokenQuery.value("token").toString();
                    saveTokenToMemory(username, token, createdTime, existingExpiresTime);
                    LOG("从数据库加载未过期token: " + token, LogLevel::LOG_INFO);
                } else {
                    // 数据库中的token已过期，删除并生成新的
                    LOG("数据库中的token已过期，删除并生成新token", LogLevel::LOG_INFO);
                    deleteTokenFromDatabase(username);
                    token = generateToken();
                    saveTokenToMemory(username, token, createdTime, expiresTime);
                    saveTokenToDatabase(username, token, createdTime, expiresTime);
                }
            } else {
                // 数据库中没有token，生成新的
                token = generateToken();
                saveTokenToMemory(username, token, createdTime, expiresTime);
                saveTokenToDatabase(username, token, createdTime, expiresTime);
            }
        }

        // 构建响应
        result["success"] = true;
        result["token"] = token;

        QMap<QString, QVariant> userInfo;
        userInfo["username"] = query.value("username").toString();
        userInfo["name"] = query.value("name").toString();
        userInfo["worknumber"] = query.value("worknumber").toString();
        userInfo["cellphone"] = query.value("cellphone").toString();
        userInfo["unit"] = query.value("unit").toString();
        userInfo["isAdmin"] = (username == "admin"); // 简单判断管理员
        userInfo["role"] = query.value("role").toString();

        result["user"] = userInfo;
    } else {
        result["success"] = false;
        result["error"] = "用户名或密码错误";
    }

    return result;
}

// 辅助函数：从数据库删除token
void CDatabaseManager::deleteTokenFromDatabase(const QString &username) {
    QSqlQuery query(db);
    query.prepare("DELETE FROM user_tokens WHERE username = :username");
    query.bindValue(":username", username);

    if (!query.exec()) {
        LOG("删除token失败: " + query.lastError().text(), LogLevel::LOG_ERROR);
    }
}

// 辅助函数：保存token到数据库
bool CDatabaseManager::saveTokenToDatabase(const QString &username, const QString &token, quint64 createdTime, quint64 expiresTime) {
    QSqlQuery query(db);
    query.prepare("INSERT INTO user_tokens (username, token, created_time, expires_time) VALUES (:username, :token, :created, :expires)");
    query.bindValue(":username", username);
    query.bindValue(":token", token);
    query.bindValue(":created", createdTime);
    query.bindValue(":expires", expiresTime);

    if (!query.exec()) {
        LOG("Token保存失败: " + query.lastError().text(), LogLevel::LOG_ERROR);
        return false;
    }
    return true;
}

// 辅助函数：保存token到内存
void CDatabaseManager::saveTokenToMemory(const QString &username, const QString &token, quint64 createdTime, quint64 expiresTime) {
    QMap<QString, QVariant> tokenInfo;
    tokenInfo["token"] = token;
    tokenInfo["created_time"] = createdTime;
    tokenInfo["expires_time"] = expiresTime;

    m_userTokens[username] = tokenInfo;
}

// Token验证
bool CDatabaseManager::validateToken(const QString &token) {
    auto subToken=token.trimmed();
    QSqlQuery query(db);
    query.prepare("SELECT username, expires_time FROM user_tokens WHERE token = :token");
    query.bindValue(":token", subToken);
    if (!query.exec()) {
        LOG("验证TOKEN时sql执行失败");
        return false;
    }

    if (query.next()) {
        quint64 expiresTime = query.value("expires_time").toULongLong();
        quint64 currentTime = QDateTime::currentMSecsSinceEpoch();

        if (currentTime <= expiresTime) {
            LOG("token有效");
            return true;
        } else {
            LOG("token已过期");
            // 删除过期token
            QSqlQuery deleteQuery(db);
            deleteQuery.prepare("DELETE FROM user_tokens WHERE token = :token");
            deleteQuery.bindValue(":token", subToken);
            deleteQuery.exec();
        }
    }else{
        LOG("未找到token记录");
    }

    return false;
}

QString CDatabaseManager::getUsernameByToken(const QString &token) {
    QSqlQuery query(db);
    query.prepare("SELECT username FROM user_tokens WHERE token = :token");
    query.bindValue(":token", token.trimmed());
    if (query.exec() && query.next()) {
        return query.value("username").toString();
    }
    return QString();
}

// 获取列车列表
QMap<QString, QVariant> CDatabaseManager::getTrainList(const QMap<QString, QString> &queryParams) {
    QMap<QString, QVariant> result;

    // 构建基础查询（含车厢检视进度 LEFT JOIN）
    QString baseQuery = QStringLiteral(
        "SELECT ti.*, "
        "COALESCE(ci.inspectionPassNumber, 0) as nInspectionPassNumber, "
        "COALESCE(ci.inspectionDispassNumber, 0) as nInspectionDispassNumber, "
        "COALESCE(ci.nUnInspectionNumber, 0) as nUnInspectionNumber "
        "FROM TrainInformation ti "
        "LEFT JOIN ("
        "  SELECT strTrainNumber, nReachDatetime,"
        "    SUM(CASE WHEN (inspectorState = 1) THEN 1 ELSE 0 END) as inspectionPassNumber,"
        "    SUM(CASE WHEN (inspectorState = 2 OR leftImagePass = 2 OR TopImagePass = 2 OR RightImagePass = 2) THEN 1 ELSE 0 END) as inspectionDispassNumber,"
        "    SUM(CASE WHEN (inspectorState = 0 AND leftImagePass = 0 AND TopImagePass = 0 AND RightImagePass = 0) THEN 1 ELSE 0 END) as nUnInspectionNumber"
        "  FROM carriage_information"
        "  GROUP BY strTrainNumber, nReachDatetime"
        ") ci ON ci.strTrainNumber = ti.strTrainNumber AND ci.nReachDatetime = ti.nReachDatetime "
        "WHERE 1=1"
    );
    QMap<QString, QVariant> bindValues;

    // 添加过滤条件
    if (queryParams.contains("trainNumber") && !queryParams["trainNumber"].isEmpty()) {
        baseQuery += " AND ti.strTrainNumber LIKE :trainNumber";
        bindValues[":trainNumber"] = "%" + queryParams["trainNumber"] + "%";
    }

    if (queryParams.contains("inspectionStatus") && !queryParams["inspectionStatus"].isEmpty()) {
        baseQuery += " AND ti.strInspectionStatus = :inspectionStatus";
        bindValues[":inspectionStatus"] = queryParams["inspectionStatus"];
    }

    // strDetection=某站: filter by detection station
    if (queryParams.contains("strDetection") && !queryParams["strDetection"].isEmpty()) {
        baseQuery += " AND ti.strDetection = :detection";
        bindValues[":detection"] = queryParams["strDetection"];
    }

    // year=YYYY: filter by full year
    if (queryParams.contains("year") && !queryParams["year"].isEmpty()) {
        int year = queryParams["year"].toInt();
        QDateTime startDt(QDate(year, 1, 1), QTime(0, 0, 0));
        quint64 startTs = startDt.toMSecsSinceEpoch() / 1000;
        QDateTime endDt(QDate(year, 12, 31), QTime(23, 59, 59));
        quint64 endTs = endDt.toMSecsSinceEpoch() / 1000;
        baseQuery += " AND ti.nReachDatetime >= :startDate AND ti.nReachDatetime <= :endDate";
        bindValues[":startDate"] = startTs;
        bindValues[":endDate"] = endTs;
    }

    // month=YYYY-MM: filter by full month
    if (queryParams.contains("month") && !queryParams["month"].isEmpty()) {
        QString monthStr = queryParams["month"];
        QStringList parts = monthStr.split("-");
        if (parts.size() == 2) {
            int year = parts[0].toInt();
            int month = parts[1].toInt();
            QDate startD(year, month, 1);
            QDate endD = startD.addMonths(1).addDays(-1);
            QDateTime startDt(startD, QTime(0, 0, 0));
            quint64 startTs = startDt.toMSecsSinceEpoch() / 1000;
            QDateTime endDt(endD, QTime(23, 59, 59));
            quint64 endTs = endDt.toMSecsSinceEpoch() / 1000;
            baseQuery += " AND ti.nReachDatetime >= :startDate AND ti.nReachDatetime <= :endDate";
            bindValues[":startDate"] = startTs;
            bindValues[":endDate"] = endTs;
        }
    }

    // date=YYYY-MM-DD: auto-convert to startDate (00:00) and endDate (23:59:59)
    if (queryParams.contains("date") && !queryParams["date"].isEmpty()) {
        QString dateStr = queryParams["date"];
        // Parse YYYY-MM-DD and convert to Unix timestamps
        QStringList parts = dateStr.split("-");
        if (parts.size() == 3) {
            int year = parts[0].toInt();
            int month = parts[1].toInt();
            int day = parts[2].toInt();
            // Start of day: YYYY-MM-DD 00:00:00
            QDateTime startDt(QDate(year, month, day), QTime(0, 0, 0));
            quint64 startTs = startDt.toMSecsSinceEpoch() / 1000;
            // End of day: YYYY-MM-DD 23:59:59
            QDateTime endDt(QDate(year, month, day), QTime(23, 59, 59));
            quint64 endTs = endDt.toMSecsSinceEpoch() / 1000;
            baseQuery += " AND ti.nReachDatetime >= :startDate AND ti.nReachDatetime <= :endDate";
            bindValues[":startDate"] = startTs;
            bindValues[":endDate"] = endTs;
        }
    } else {
        // Manual startDate/endDate
        if (queryParams.contains("startDate") && !queryParams["startDate"].isEmpty()) {
            baseQuery += " AND ti.nReachDatetime >= :startDate";
            bindValues[":startDate"] = queryParams["startDate"].toULongLong();
        }
        if (queryParams.contains("endDate") && !queryParams["endDate"].isEmpty()) {
            baseQuery += " AND ti.nReachDatetime <= :endDate";
            bindValues[":endDate"] = queryParams["endDate"].toULongLong();
        }
    }

    if (queryParams.contains("strDirection") && !queryParams["strDirection"].isEmpty()) {
        baseQuery += " AND ti.strDirection = :direction";
        bindValues[":direction"] = queryParams["strDirection"];
    }

    // 获取总数
    QString countQuery = "SELECT COUNT(*) FROM (" + baseQuery + ")";
    QSqlQuery countQ(db);
    countQ.prepare(countQuery);

    for (auto it = bindValues.constBegin(); it != bindValues.constEnd(); ++it) {
        countQ.bindValue(it.key(), it.value());
    }

    int total = 0;
    if (countQ.exec() && countQ.next()) {
        total = countQ.value(0).toInt();
    }
    LOG(countQ.executedQuery());
    // 分页
    int page = queryParams.value("page", "1").toInt();
    int pageSize = queryParams.value("pageSize", "20").toInt();
    int offset = (page - 1) * pageSize;

    baseQuery += " ORDER BY ti.nReachDatetime DESC LIMIT :limit OFFSET :offset";
    bindValues[":limit"] = pageSize;
    bindValues[":offset"] = offset;

    // 执行查询
    QSqlQuery query(db);
    query.prepare(baseQuery);

    for (auto it = bindValues.constBegin(); it != bindValues.constEnd(); ++it) {
        query.bindValue(it.key(), it.value());
    }

    QList<QVariant> trains;
    if (query.exec()) {
        while (query.next()) {
            QMap<QString, QVariant> train;
            train["id"] = query.value("id").toInt();
            train["strTrainNumber"] = query.value("strTrainNumber").toString();
            train["nReachDatetime"] = query.value("nReachDatetime").toULongLong();
            train["strDirection"] = query.value("strDirection").toString();
            train["nNumberOfAxles"] = query.value("nNumberOfAxles").toInt();
            train["nNumberOfCarriage"] = query.value("nNumberOfCarriage").toInt();
            train["strFilePath"] = query.value("strFilePath").toString();
            train["strDetection"] = query.value("strDetection").toString();
            train["strInspectionStatus"] = query.value("strInspectionStatus").toString();
            train["nInspectionDatetime"] = query.value("nInspectionDatetime").toULongLong();

            // 从 LEFT JOIN 结果读取车厢检视统计
            train["nUnInspectionNumber"] = query.value("nUnInspectionNumber").toInt();
            train["nInspectionPassNumber"] = query.value("nInspectionPassNumber").toInt();
            train["nInspectionDispassNumber"] = query.value("nInspectionDispassNumber").toInt();
            train["strLeftAreaCameraPath"] = query.value("strLeftAreaCameraPath").toString();
            train["strRightAreaCameraPath"] = query.value("strRightAreaCameraPath").toString();
            train["strTopAreaCameraPath"] = query.value("strTopAreaCameraPath").toString();
            trains.append(train);
        }
    }

    result["success"] = true;

    QMap<QString, QVariant> data;
    data["trains"] = trains;
    data["total"] = total;
    data["page"] = page;
    data["pageSize"] = pageSize;
    data["total"] = total;  // 确保 total 也返回

    result["data"] = data;
    return result;
}

// 更新列车检视状态
bool CDatabaseManager::updateTrainInspectionStatus(int trainId, const QString &status, quint64 inspectionTime) {
    QSqlQuery query(db);
    query.prepare("UPDATE TrainInformation SET strInspectionStatus = :status, nInspectionDatetime = :time WHERE id = :id");
    query.bindValue(":status", status);
    query.bindValue(":time", inspectionTime);
    query.bindValue(":id", trainId);

    if (!query.exec()) {
        LOG("更新列车检视状态失败: " + query.lastError().text(), LogLevel::LOG_ERROR);
        return false;
    }

    return query.numRowsAffected() > 0;
}

// 获取车厢信息
QMap<QString, QVariant> CDatabaseManager::getCarriageList(const QString &trainNumber, quint64 reachDatetime) {
    QMap<QString, QVariant> result;

    QSqlQuery query(db);
    query.prepare(R"(
        SELECT * FROM carriage_information
        WHERE strTrainNumber = :trainNumber AND nReachDatetime = :reachTime
        ORDER BY nOrder
    )");
    query.bindValue(":trainNumber", trainNumber);
    query.bindValue(":reachTime", reachDatetime);

    QList<QVariant> carriages;
    if (query.exec()) {
        while (query.next()) {
            QMap<QString, QVariant> carriage;
            carriage["id"] = query.value("id").toInt();
            carriage["strTrainNumber"] = query.value("strTrainNumber").toString();
            carriage["nReachDatetime"] = query.value("nReachDatetime").toULongLong();
            carriage["nOrder"] = query.value("nOrder").toInt();
            carriage["strWagonNumber"] = query.value("strWagonNumber").toString();
            carriage["strCarriageModel"] = query.value("strCarriageModel").toString();
            carriage["fConverredLength"] = query.value("fConverredLength").toDouble();
            carriage["strCarriageModelCode"] = query.value("strCarriageModelCode").toString();
            carriage["strFactoryAndDatetime"] = query.value("strFactoryAndDatetime").toString();
            carriage["nFrontWheelTime"] = query.value("nFrontWheelTime").toULongLong();
            carriage["frontWheelSpeed"] = query.value("frontWheelSpeed").toDouble();
            carriage["nRearWheelTime"] = query.value("nRearWheelTime").toULongLong();
            carriage["rearWheelSpeed"] = query.value("rearWheelSpeed").toDouble();
            carriage["leftImagePath"] = query.value("leftImagePath").toString();
            carriage["TopImagePath"] = query.value("TopImagePath").toString();
            carriage["RightImagePath"] = query.value("RightImagePath").toString();
            carriage["inspectorState"] = query.value("inspectorState").toInt();
            carriage["leftImagePass"] = query.value("leftImagePass").toInt();
            carriage["TopImagePass"] = query.value("TopImagePass").toInt();
            carriage["RightImagePass"] = query.value("RightImagePass").toInt();
            carriage["inspectorNote"] = query.value("inspectorNote").toString();
            // 图片路径映射对象
            QMap<QString, QVariant> imagePathMap;
            QString leftPath = query.value("leftImagePath").toString();
            QString topPath = query.value("TopImagePath").toString();
            QString rightPath = query.value("RightImagePath").toString();
            
            if (!leftPath.isEmpty()) {
                imagePathMap["left"] = leftPath;
            }
            if (!topPath.isEmpty()) {
                imagePathMap["top"] = topPath;
            }
            if (!rightPath.isEmpty()) {
                imagePathMap["right"] = rightPath;
            }
            
            carriage["imagePathMap"] = imagePathMap;
            

            carriages.append(carriage);
        }
    }

    result["success"] = true;

    QMap<QString, QVariant> data;
    data["carriages"] = carriages;

    result["data"] = data;
    return result;
}

// 更新车厢检视状态
bool CDatabaseManager::updateCarriageInspection(int carriageId, const QMap<QString, QVariant> &inspectionData) {
    QSqlQuery query(db);
    query.prepare(R"(
        UPDATE carriage_information
        SET inspectorState = :state,
            leftImagePass = :leftPass,
            TopImagePass = :topPass,
            RightImagePass = :rightPass,
            inspectorNote = :note
        WHERE id = :id
    )");

    query.bindValue(":state", inspectionData["inspectorState"].toInt());
    query.bindValue(":leftPass", inspectionData["leftImagePass"].toInt());
    query.bindValue(":topPass", inspectionData["TopImagePass"].toInt());
    query.bindValue(":rightPass", inspectionData["RightImagePass"].toInt());
    query.bindValue(":note", inspectionData["inspectorNote"].toString());
    query.bindValue(":id", carriageId);

    if (!query.exec()) {
        LOG("更新车厢检视状态失败: " + query.lastError().text(), LogLevel::LOG_ERROR);
        return false;
    }

    return query.numRowsAffected() > 0;
}

// 获取用户列表
QMap<QString, QVariant> CDatabaseManager::getUserList(const QMap<QString, QString> &queryParams) {
    QMap<QString, QVariant> result;

    QString baseQuery = "SELECT username, name, worknumber, cellphone, unit, status, remark FROM users WHERE 1=1";
    QMap<QString, QVariant> bindValues;

    // 添加过滤条件
    if (queryParams.contains("username") && !queryParams["username"].isEmpty()) {
        baseQuery += " AND username LIKE :username";
        bindValues[":username"] = "%" + queryParams["username"] + "%";
    }

    if (queryParams.contains("name") && !queryParams["name"].isEmpty()) {
        baseQuery += " AND name LIKE :name";
        bindValues[":name"] = "%" + queryParams["name"] + "%";
    }

    if (queryParams.contains("worknumber") && !queryParams["worknumber"].isEmpty()) {
        baseQuery += " AND worknumber LIKE :worknumber";
        bindValues[":worknumber"] = "%" + queryParams["worknumber"] + "%";
    }

    // 获取总数
    QString countQuery = "SELECT COUNT(*) FROM (" + baseQuery + ")";
    QSqlQuery countQ(db);
    countQ.prepare(countQuery);

    for (auto it = bindValues.constBegin(); it != bindValues.constEnd(); ++it) {
        countQ.bindValue(it.key(), it.value());
    }

    int total = 0;
    if (countQ.exec() && countQ.next()) {
        total = countQ.value(0).toInt();
    }

    // 分页
    int page = queryParams.value("page", "1").toInt();
    int pageSize = queryParams.value("pageSize", "20").toInt();
    int offset = (page - 1) * pageSize;

    baseQuery += " ORDER BY username LIMIT :limit OFFSET :offset";
    bindValues[":limit"] = pageSize;
    bindValues[":offset"] = offset;

    // 执行查询
    QSqlQuery query(db);
    query.prepare(baseQuery);

    for (auto it = bindValues.constBegin(); it != bindValues.constEnd(); ++it) {
        query.bindValue(it.key(), it.value());
    }

    QList<QVariant> users;
    if (query.exec()) {
        while (query.next()) {
            QMap<QString, QVariant> user;
            user["username"] = query.value("username").toString();
            user["name"] = query.value("name").toString();
            user["worknumber"] = query.value("worknumber").toString();
            user["cellphone"] = query.value("cellphone").toString();
            user["unit"] = query.value("unit").toString();
            user["status"] = query.value("status").toInt();
            user["remark"] = query.value("remark").toString();

            users.append(user);
        }
    }

    result["success"] = true;

    QMap<QString, QVariant> data;
    data["users"] = users;
    data["total"] = total;
    data["page"] = page;
    data["pageSize"] = pageSize;

    result["data"] = data;
    return result;
}

// 获取单个用户信息
QMap<QString, QVariant> CDatabaseManager::getUserByUsername(const QString &username) {
    QMap<QString, QVariant> result;

    QSqlQuery query(db);
    query.prepare("SELECT username, name, worknumber, cellphone, unit, status, remark, role FROM users WHERE username = :username");
    query.bindValue(":username", username);

    if (query.exec() && query.next()) {
        result["success"] = true;

        QMap<QString, QVariant> user;
        user["username"] = query.value("username").toString();
        user["name"] = query.value("name").toString();
        user["worknumber"] = query.value("worknumber").toString();
        user["cellphone"] = query.value("cellphone").toString();
        user["unit"] = query.value("unit").toString();
        user["status"] = query.value("status").toInt();
        user["remark"] = query.value("remark").toString();
        user["role"] = query.value("role").toString();

        result["data"] = user;
    } else {
        result["success"] = false;
        result["error"] = "用户不存在";
    }

    return result;
}

// 创建用户
bool CDatabaseManager::createUser(const QMap<QString, QVariant> &userData) {
    if (usernameExists(userData["username"].toString())) {
        LOG("用户名已存在: " + userData["username"].toString(), LogLevel::LOG_WARNING);
        return false;
    }

    QString encryptedPassword = encryptPassword(userData["password"].toString());

    QSqlQuery query(db);
    query.prepare(R"(
        INSERT INTO users (username, name, worknumber, password, cellphone, unit, status, remark)
        VALUES (:username, :name, :worknumber, :password, :cellphone, :unit, :status, :remark)
    )");

    query.bindValue(":username", userData["username"].toString());
    query.bindValue(":name", userData["name"].toString());
    query.bindValue(":worknumber", userData["worknumber"].toString());
    query.bindValue(":password", encryptedPassword);
    query.bindValue(":cellphone", userData["cellphone"].toString());
    query.bindValue(":unit", userData["unit"].toString());
    query.bindValue(":status", userData["status"].toInt());
    query.bindValue(":remark", userData["remark"].toString());

    if (!query.exec()) {
        LOG("创建用户失败: " + query.lastError().text(), LogLevel::LOG_ERROR);
        return false;
    }

    return true;
}

// 更新用户信息
bool CDatabaseManager::updateUser(const QString &username, const QMap<QString, QVariant> &userData) {
    QSqlQuery query(db);
    query.prepare(R"(
        UPDATE users
        SET name = :name, worknumber = :worknumber, cellphone = :cellphone, unit = :unit, remark = :remark
        WHERE username = :username
    )");

    query.bindValue(":name", userData["name"].toString());
    query.bindValue(":worknumber", userData["worknumber"].toString());
    query.bindValue(":cellphone", userData["cellphone"].toString());
    query.bindValue(":unit", userData["unit"].toString());
    query.bindValue(":remark", userData["remark"].toString());
    query.bindValue(":username", username);

    if (!query.exec()) {
        LOG("更新用户失败: " + query.lastError().text(), LogLevel::LOG_ERROR);
        return false;
    }

    int affected = query.numRowsAffected();
    LOG("更新用户 Affected Rows: " + QString::number(affected), LogLevel::LOG_INFO);
    return affected > 0;
}

// 重置用户密码
bool CDatabaseManager::resetUserPassword(const QString &username, const QString &newPassword) {
    QString encryptedPassword = encryptPassword(newPassword);

    QSqlQuery query(db);
    query.prepare("UPDATE users SET password = :password WHERE username = :username");
    query.bindValue(":password", encryptedPassword);
    query.bindValue(":username", username);

    if (!query.exec()) {
        LOG("重置密码失败: " + query.lastError().text(), LogLevel::LOG_ERROR);
        return false;
    }

    // 显式结束上一个查询，释放数据库连接资源
    query.finish();

    // 重新构造查询对象，验证密码是否已更新
    QSqlQuery verifyQuery(db);
    verifyQuery.prepare("SELECT password FROM users WHERE username = :username");
    verifyQuery.bindValue(":username", username);
    if (!verifyQuery.exec()) {
        LOG("resetUserPassword: SELECT失败: " + verifyQuery.lastError().text(), LogLevel::LOG_ERROR);
        return false;
    }
    if (!verifyQuery.next()) {
        LOG("resetUserPassword: 用户不存在: " + username, LogLevel::LOG_ERROR);
        return false;
    }
    // 用字段索引获取值
    QString actualPassword = verifyQuery.value(0).toString();
    LOG("resetUserPassword: 验证 expected(len)=" + QString::number(encryptedPassword.length())
         + " actual(len)=" + QString::number(actualPassword.length())
         + " match=" + QString(actualPassword == encryptedPassword ? "YES" : "NO"),
         LogLevel::LOG_INFO);
    return actualPassword == encryptedPassword;
}

// 设置用户状态
bool CDatabaseManager::setUserStatus(const QString &username, int status) {
    QSqlQuery query(db);
    query.prepare("UPDATE users SET status = :status WHERE username = :username");
    query.bindValue(":status", status);
    query.bindValue(":username", username);

    if (!query.exec()) {
        LOG("设置用户状态失败: " + query.lastError().text(), LogLevel::LOG_ERROR);
        return false;
    }

    return query.numRowsAffected() > 0;
}

// 修改当前用户密码
bool CDatabaseManager::changeUserPassword(const QString &username, const QString &currentPassword, const QString &newPassword) {
    QString encryptedCurrent = encryptPassword(currentPassword);

    QSqlQuery checkQuery(db);
    checkQuery.prepare("SELECT COUNT(*) FROM users WHERE username = :username AND password = :password");
    checkQuery.bindValue(":username", username);
    checkQuery.bindValue(":password", encryptedCurrent);

    if (!checkQuery.exec()) {
        LOG("changeUserPassword: exec failed " + checkQuery.lastError().text(), LogLevel::LOG_ERROR);
        return false;
    }
    if (!checkQuery.next()) {
        LOG("changeUserPassword: next() failed, error=" + checkQuery.lastError().text(), LogLevel::LOG_ERROR);
        return false;
    }
    int count = checkQuery.value(0).toInt();
    if (count == 0) {
        LOG("changeUserPassword: password mismatch for user=" + username + " encryptedCurrent=" + encryptedCurrent, LogLevel::LOG_INFO);
        return false;
    }
    LOG("changeUserPassword: password verified OK, calling resetUserPassword", LogLevel::LOG_INFO);

    return resetUserPassword(username, newPassword);
}

// 生成Token
QString CDatabaseManager::generateToken() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

// 密码加密
QString CDatabaseManager::encryptPassword(const QString &password) {
    QByteArray hash = QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256);
    return QString(hash.toHex());
}

// 修改用户表结构以支持新字段
bool CDatabaseManager::createUserTable() {
    QSqlQuery checkQuery(db);
    checkQuery.prepare("SELECT name FROM sqlite_master WHERE type='table' AND name='users'");
    if (!checkQuery.exec()) {
        LOG("检查用户表失败: " + checkQuery.lastError().text(), LogLevel::LOG_ERROR);
        return false;
    }

    if (checkQuery.next()) {
        // 表已存在，检查是否需要添加新字段
        QSqlQuery alterQuery(db);

        // 检查并添加unit字段
        QSqlQuery checkUnit(db);
        checkUnit.prepare("PRAGMA table_info(users)");
        bool hasUnit = false;
        if (checkUnit.exec()) {
            while (checkUnit.next()) {
                if (checkUnit.value("name").toString() == "unit") {
                    hasUnit = true;
                    break;
                }
            }
        }

        if (!hasUnit) {
            if (!alterQuery.exec("ALTER TABLE users ADD COLUMN unit VARCHAR(255)")) {
                LOG("添加unit字段失败: " + alterQuery.lastError().text(), LogLevel::LOG_WARNING);
            }
        }

        // 类似检查其他字段：status, remark
        return true;
    }

    // 创建新表
    QSqlQuery createQuery(db);
    QString createTableSQL =
        "CREATE TABLE users ("
        "username VARCHAR(255) PRIMARY KEY,"
        "name VARCHAR(255) NOT NULL,"
        "worknumber VARCHAR(255),"
        "password VARCHAR(255) NOT NULL,"
        "cellphone VARCHAR(20),"
        "unit VARCHAR(255),"
        "status INTEGER DEFAULT 0,"
        "remark TEXT)";

    if (!createQuery.exec(createTableSQL)) {
        LOG("创建用户表失败: " + createQuery.lastError().text(), LogLevel::LOG_ERROR);
        return false;
    }
    return true;
}
bool CDatabaseManager::createTrainInformationTable() {

    // 3. 检查表是否存在
    bool tableExists = false;
    QSqlQuery checkQuery("SELECT name FROM sqlite_master WHERE type='table' AND name='TrainInformation'");
    if(checkQuery.exec() && checkQuery.next()) {
        qDebug() << "TrainInformation表已存在";
        tableExists = true;
    }

    // 4. 创建表（如果不存在）
    if(!tableExists) {
        QSqlQuery createQuery;
        QString createTableSQL = R"(
            CREATE TABLE TrainInformation (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                strTrainNumber TEXT NOT NULL,        -- 车次
                nReachDatetime INTEGER NOT NULL,     -- 到达时间
                strDirection TEXT,                   -- 列车方向
                nNumberOfAxles INTEGER,              -- 总轴数
                nNumberOfCarriage INTEGER,           -- 辆数
                strFilePath TEXT,                    -- 图像文件路径
                strDetection TEXT,                   -- 监测点
                strInspectionStatus TEXT DEFAULT '未检视', -- 检视状态
                nInspectionDatetime INTEGER,         -- 检测时间
                strLeftAreaCameraPath  TEXT,
                strRightAreaCameraPath TEXT,
                strTopAreaCameraPath   TEXT,
                UNIQUE(strTrainNumber, nReachDatetime) -- 防止重复记录
            )
        )";

        if(!createQuery.exec(createTableSQL)) {
            LOG( "表TrainInformation创建失败：" + createQuery.lastError().text(),LogLevel::LOG_ERROR);
            return false;
        }

        // 创建索引提高查询效率
        createQuery.exec("CREATE INDEX idx_train_number ON TrainInformation(strTrainNumber)");
        createQuery.exec("CREATE INDEX idx_detection ON TrainInformation(strDetection)");

        LOG( "TrainInformation表创建成功");
    }

    return true;
}

bool CDatabaseManager::updateTrainInformationEx(const QMap<QString, QString> &updateParams, const QMap<QString, QString> &conditionParams)
{
    // 检查数据库连接
    if (!db.isOpen()) {
        LOG("数据库未连接", LogLevel::LOG_ERROR);
        return false;
    }

    // 构建更新语句
    QString updateQuery = "UPDATE TrainInformation SET ";

    // 添加要更新的字段
    QList<QString> setClauses;
    if (updateParams.contains("strInspectionStatus")) {
        setClauses.append("strInspectionStatus = :strInspectionStatus");
    }
    if (updateParams.contains("nInspectionDatetime")) {
        setClauses.append("nInspectionDatetime = :nInspectionDatetime");
    }

    // 如果没有要更新的字段，直接返回
    if (setClauses.isEmpty()) {
        LOG("没有需要更新的字段", LogLevel::LOG_WARNING);
        return false;
    }

    updateQuery += setClauses.join(", ");

    // 添加WHERE条件
    QList<QString> whereClauses;
    for (auto it = conditionParams.constBegin(); it != conditionParams.constEnd(); ++it) {
        whereClauses.append(it.key() + " = :" + it.key());
    }

    if (!whereClauses.isEmpty()) {
        updateQuery += " WHERE " + whereClauses.join(" AND ");
    }

    // 准备查询
    QSqlQuery query(db);
    if (!query.prepare(updateQuery)) {
        LOG("准备更新语句失败: " + query.lastError().text(), LogLevel::LOG_ERROR);
        return false;
    }

    // 绑定更新值
    for (auto it = updateParams.constBegin(); it != updateParams.constEnd(); ++it) {
        if (it.key() == "nInspectionDatetime") {
            query.bindValue(":" + it.key(), it.value().toLongLong());
        } else {
            query.bindValue(":" + it.key(), it.value());
        }
    }

    // 绑定条件值
    for (auto it = conditionParams.constBegin(); it != conditionParams.constEnd(); ++it) {
        if (it.key() == "nReachDatetime") {
            query.bindValue(":" + it.key(), it.value().toLongLong());
        } else {
            query.bindValue(":" + it.key(), it.value());
        }
    }

    // 执行更新
    if (!query.exec()) {
        LOG("更新失败: " + query.lastError().text(), LogLevel::LOG_ERROR);
        return false;
    }

    // 检查是否成功更新了记录
    if (query.numRowsAffected() == 0) {
        LOG("未找到匹配的记录，更新失败", LogLevel::LOG_WARNING);
        return false;
    }

    LOG("成功更新TrainInformation记录", LogLevel::LOG_INFO);
    return true;
}
// 插入数据的示例函数
bool CDatabaseManager::insertTrainRecord(const TrainInformation &train) {
    QSqlQuery query(db);
    query.prepare("INSERT INTO TrainInformation "
                  "(strTrainNumber, nReachDatetime, strDirection, "
                  "nNumberOfAxles, nNumberOfCarriage, strFilePath, "
                  "strDetection, nInspectionDatetime,"
    "strLeftAreaCameraPath,"
   " strRightAreaCameraPath,"
    "strTopAreaCameraPath) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?,?)");

    query.addBindValue(train.strTrainNumber);
    query.addBindValue(train.nReachDatetime);
    query.addBindValue(train.strDirection);
    query.addBindValue(train.nNumberOfAxles);
    query.addBindValue(train.nNumberOfCarriage);
    query.addBindValue(train.strFilePath);
    query.addBindValue(train.strDetection);
    query.addBindValue(train.nInspectionDatetime);
    query.addBindValue(train.strLeftVideoPath);
    query.addBindValue(train.strRightVideoPath);
    query.addBindValue(train.strTopVideoPath);
    return query.exec();
}
bool CDatabaseManager::createCarriageTable(){
    // 创建表
    QSqlQuery checkQuery(db);
    if (!checkQuery.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='carriage_information'")) {
        LOG( "表检查失败:" + checkQuery.lastError().text(),LogLevel::LOG_WARNING);
        return false;
    }

    // 表已存在则跳过创建
    if (checkQuery.next()) {
        qDebug() << "表已存在，跳过创建";
        return true;
    }
    // 在创建表语句中添加inspectorNote字段
    QString createTable = "CREATE TABLE IF NOT EXISTS carriage_information ("
                          "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                          "strTrainNumber TEXT, "
                          "nReachDatetime INTEGER, "
                          "nOrder INTEGER, "
                          "strWagonNumber TEXT, "
                          "strCarriageModel TEXT, "
                          "fConverredLength REAL, "
                          "strCarriageModelCode TEXT, "
                          "strFactoryAndDatetime TEXT, "
                          "nFrontWheelTime INTEGER, "
                          "frontWheelSpeed REAL, "
                          "nRearWheelTime INTEGER, "
                          "rearWheelSpeed REAL,"
                          "leftImagePath TEXT,"
                          "TopImagePath TEXT,"
                          "RightImagePath TEXT,"
                          "inspectorState INTEGER DEFAULT 0,"
                          "leftImagePass INTEGER DEFAULT 0,"
                          "TopImagePass INTEGER DEFAULT 0,"
                          "RightImagePass INTEGER DEFAULT 0,"
                          "inspectorNote TEXT)";

    if (!checkQuery.exec(createTable)) {
        LOG("创建表失败: " + checkQuery.lastError().text(),LogLevel::LOG_ERROR);
        return false;
    }
    return true;
}
bool CDatabaseManager::insertCarriage(const CarriageInformation &info)
{
    QSqlDatabase subdb=db;
    if(!db.isValid()||!db.isOpen()){
        // 连接数据库
        subdb = QSqlDatabase::addDatabase("QSQLITE");
        subdb.setDatabaseName("appdata.db");

        if (!subdb.open()) {
            LOG("Error:" + subdb.lastError().text(), LogLevel::LOG_ERROR);
        }
    }
    QSqlQuery query(subdb);
    query.prepare(
        "INSERT INTO carriage_information("
        "strTrainNumber, nReachDatetime, nOrder, strWagonNumber, "
        "strCarriageModel, fConverredLength, strCarriageModelCode, strFactoryAndDatetime, "
        "nFrontWheelTime, frontWheelSpeed, nRearWheelTime, rearWheelSpeed, "
        "leftImagePath, TopImagePath, RightImagePath, "
        "inspectorState, leftImagePass, TopImagePass, RightImagePass) "
        "VALUES (?, ?, ?, ?,   ?, ?, ?, ?,   ?, ?, ?, ?,   ?, ?, ?,   ?, ?, ?, ?)");

    /* 基本字段 */
    query.addBindValue(info.strTrainNumber);
    query.addBindValue(info.nReachDatetime);
    query.addBindValue(info.nOrder);
    query.addBindValue(info.strWagonNumber);
    query.addBindValue(info.strCarriageModel);
    query.addBindValue(info.fConverredLength);
    query.addBindValue(info.strCarriageModelCode);
    query.addBindValue(info.strFactoryAndDatetime);
    query.addBindValue(info.nFrontWheelTime);
    query.addBindValue(info.frontWheelSpeed);
    query.addBindValue(info.nRearWheelTime);
    query.addBindValue(info.rearWheelSpeed);

    /* 图像路径 */
    auto getPath = [&info](const QString &key) -> QString {
        auto it = info.imagePathMap.find(key);
        return (it != info.imagePathMap.end()) ? it->second : QString();
    };
    query.addBindValue(getPath("left"));
    query.addBindValue(getPath("top"));
    query.addBindValue(getPath("right"));

    /* 检索状态 */
    query.addBindValue(info.inspectorState ? 1 : 0);

    /* 三幅图判定结果 */
    auto getResult = [&info](const QString &key) -> int {
        auto it = info.inspectorResult.find(key);
        return (it != info.inspectorResult.end() && it->second) ? 1 : 0;
    };
    query.addBindValue(getResult("left"));
    query.addBindValue(getResult("top"));
    query.addBindValue(getResult("right"));

    if (!query.exec()) {
        LOG(query.lastError().text());
        return false;
    }
    return true;
}

// ==================== 车厢标注 ====================

QMap<QString, QVariant> CDatabaseManager::getCarriageAnnotation(
    const QString &trainNumber, qint64 reachDatetime, int order)
{
    QMap<QString, QVariant> result;

    QSqlQuery query(db);
    query.prepare("SELECT nOrder, strCarriageModel, strDefectTypes, strDefectRects, strAnnotator, nAnnotateTime "
                  "FROM carriage_annotation WHERE strTrainNumber = :trainNumber AND nReachDatetime = :reachDatetime AND nOrder = :order");
    query.bindValue(":trainNumber", trainNumber);
    query.bindValue(":reachDatetime", reachDatetime);
    query.bindValue(":order", order);

    if (!query.exec()) {
        LOG("获取车厢标注失败: " + query.lastError().text(), LogLevel::LOG_ERROR);
        result["success"] = false;
        result["error"] = "数据库查询失败";
        return result;
    }

    if (!query.next()) {
        result["success"] = true;
        result["found"] = false;  // 标记为未找到（正常情况）
        return result;
    }

    result["success"] = true;
    result["found"] = true;
    result["nOrder"] = query.value("nOrder").toInt();
    result["strCarriageModel"] = query.value("strCarriageModel").toString();
    result["strDefectTypes"] = query.value("strDefectTypes").toString();
    result["strDefectRects"] = query.value("strDefectRects").toString();
    result["strAnnotator"] = query.value("strAnnotator").toString();
    result["nAnnotateTime"] = query.value("nAnnotateTime").toLongLong();
    return result;
}

bool CDatabaseManager::saveCarriageAnnotation(
    const QString &trainNumber, qint64 reachDatetime, int order,
    const QJsonObject &data)
{
    QString carriageModel = data["strCarriageModel"].toString();
    QString defectTypes = data["strDefectTypes"].toString();
    QString defectRects = data["strDefectRects"].toString();
    QString annotator = data["strAnnotator"].toString();
    qint64 annotateTime = QDateTime::currentSecsSinceEpoch();

    QSqlQuery query(db);
    // 使用 REPLACE INTO：若记录存在则更新，不存在则插入
    query.prepare("REPLACE INTO carriage_annotation "
                  "(strTrainNumber, nReachDatetime, nOrder, strCarriageModel, strDefectTypes, strDefectRects, strAnnotator, nAnnotateTime) "
                  "VALUES (:trainNumber, :reachDatetime, :order, :carriageModel, :defectTypes, :defectRects, :annotator, :annotateTime)");
    query.bindValue(":trainNumber", trainNumber);
    query.bindValue(":reachDatetime", reachDatetime);
    query.bindValue(":order", order);
    query.bindValue(":carriageModel", carriageModel);
    query.bindValue(":defectTypes", defectTypes);
    query.bindValue(":defectRects", defectRects);
    query.bindValue(":annotator", annotator);
    query.bindValue(":annotateTime", annotateTime);

    if (!query.exec()) {
        LOG("保存车厢标注失败: " + query.lastError().text(), LogLevel::LOG_ERROR);
        return false;
    }
    return true;
}

// 获取统计信息
QMap<QString, QVariant> CDatabaseManager::getStats(const QString &date) {
    QMap<QString, QVariant> result;

    if (date.isEmpty()) {
        result["success"] = false;
        result["error"] = "日期参数不能为空";
        return result;
    }

    // 解析日期
    QStringList parts = date.split("-");
    if (parts.size() != 3) {
        result["success"] = false;
        result["error"] = "日期格式错误，应为YYYY-MM-DD";
        return result;
    }

    int year = parts[0].toInt();
    int month = parts[1].toInt();
    int day = parts[2].toInt();

    // 计算日期范围的时间戳
    QDateTime startDt(QDate(year, month, day), QTime(0, 0, 0));
    QDateTime endDt(QDate(year, month, day), QTime(23, 59, 59));
    quint64 startTs = startDt.toMSecsSinceEpoch() / 1000;
    quint64 endTs = endDt.toMSecsSinceEpoch() / 1000;

    QSqlQuery query(db);

    // 1. 今日过车数
    query.prepare("SELECT COUNT(*) FROM TrainInformation WHERE nReachDatetime >= :start AND nReachDatetime <= :end");
    query.bindValue(":start", startTs);
    query.bindValue(":end", endTs);
    int todayTrainCount = 0;
    if (query.exec() && query.next()) {
        todayTrainCount = query.value(0).toInt();
    }

    // 2. 待检车辆数（未检视的车厢）
    query.prepare("SELECT COUNT(*) FROM carriage_information WHERE nReachDatetime >= :start AND nReachDatetime <= :end AND inspectorState = 0");
    query.bindValue(":start", startTs);
    query.bindValue(":end", endTs);
    int pendingCount = 0;
    if (query.exec() && query.next()) {
        pendingCount = query.value(0).toInt();
    }

    // 3. 今日告警数（inspectorState = 2 表示不合格/告警）
    query.prepare("SELECT COUNT(*) FROM carriage_information WHERE nReachDatetime >= :start AND nReachDatetime <= :end AND inspectorState = 2");
    query.bindValue(":start", startTs);
    query.bindValue(":end", endTs);
    int alarmCount = 0;
    if (query.exec() && query.next()) {
        alarmCount = query.value(0).toInt();
    }

    result["success"] = true;
    QJsonObject dataObj;
    dataObj["todayTrainCount"] = todayTrainCount;
    dataObj["pendingCount"] = pendingCount;
    dataObj["alarmCount"] = alarmCount;
    result["data"] = dataObj;

    return result;
}

// 统计指定ID的未检车箱数量并更新状态
bool CDatabaseManager::updateInspectionStatus(int carriageId) {
    QSqlQuery query;

    // 1. 获取指定ID的车次信息和到达时间
    QString getTrainInfo =
        "SELECT strTrainNumber, nReachDatetime FROM carriage_information WHERE id = :id";

    query.prepare(getTrainInfo);
    query.bindValue(":id", carriageId);

    if (!query.exec() || !query.next()) {
        qDebug() << "Error getting train information:" << query.lastError().text();
        return false;
    }

    QString trainNumber = query.value("strTrainNumber").toString();
    qint64 reachDatetime = query.value("nReachDatetime").toLongLong();

    // 2. 统计该车次未检车箱数量
    QString countUninspected =
        "SELECT COUNT(*) as uninspected_count FROM carriage_information "
        "WHERE strTrainNumber = :trainNumber AND nReachDatetime = :reachDatetime "
        "AND inspectorState = 0";

    query.prepare(countUninspected);
    query.bindValue(":trainNumber", trainNumber);
    query.bindValue(":reachDatetime", reachDatetime);

    if (!query.exec() || !query.next()) {
        qDebug() << "Error counting uninspected carriages:" << query.lastError().text();
        return false;
    }

    int uninspectedCount = query.value("uninspected_count").toInt();
    qDebug() << "Train" << trainNumber << "has" << uninspectedCount << "uninspected carriages";

    // 3. 如果未检数为0，更新列车状态
    if (uninspectedCount == 0) {
        QString updateTrainStatus =
            "UPDATE TrainInformation "
            "SET strInspectionStatus = '已检视', nInspectionDatetime = :inspectionTime "
            "WHERE strTrainNumber = :trainNumber AND nReachDatetime = :reachDatetime";

        query.prepare(updateTrainStatus);
        query.bindValue(":trainNumber", trainNumber);
        query.bindValue(":reachDatetime", reachDatetime);
        query.bindValue(":inspectionTime", QDateTime::currentDateTime().toMSecsSinceEpoch());

        if (!query.exec()) {
            qDebug() << "Error updating train status:" << query.lastError().text();
            return false;
        }

        if (query.numRowsAffected() > 0) {
            qDebug() << "Successfully updated train" << trainNumber << "status to '已检视'";
            return true;
        } else {
            qDebug() << "No rows affected - train record may not exist";
            return false;
        }
    } else {
        qDebug() << "Train" << trainNumber << "still has uninspected carriages, status not updated";
        return true;
    }
}
QString generateDebugSQL(const QString& queryStr, const QSqlQuery& query) {
    QString debugSQL = queryStr;
    for (int i = 0; i < query.boundValues().size(); ++i) {
        QVariant value = query.boundValue(i);
        QString placeholder = /*QString(":") + */query.boundValueName(i);
        QString replacement;

        if (value.isNull()) {
            replacement = "NULL";
        } else if (value.typeId() == QMetaType::QString) {
            replacement = QString("'%1'").arg(value.toString().replace("'", "''"));
        } else {
            replacement = value.toString();
        }

        debugSQL=debugSQL.replace(placeholder, replacement);
    }
    return debugSQL;
}
bool CDatabaseManager::inspectCarriageInspection(const QMap<QString, QString> &updateParams)
{
    // 检查数据库连接
    if (!db.isOpen()) {
        LOG("数据库未连接", LogLevel::LOG_ERROR);
        return false;
    }

    // 构建更新语句
    QString updateQuery = "UPDATE carriage_information SET inspectorState=1, ";

    // 添加要更新的字段
    QList<QString> setClauses;
    if (updateParams.contains("leftImagePass")) {
        setClauses.append("leftImagePass = :leftImagePass");
    }
    if (updateParams.contains("TopImagePass")) {
        setClauses.append("TopImagePass = :TopImagePass");
    }
    if (updateParams.contains("RightImagePass")) {
        setClauses.append("RightImagePass = :RightImagePass");
    }
    setClauses.append("inspectorState = 1");
    // 如果没有要更新的字段，直接返回
    if (setClauses.isEmpty()) {
        LOG("没有需要更新的字段", LogLevel::LOG_WARNING);
        return false;
    }

    updateQuery += setClauses.join(", ");

    // 添加WHERE条件
    updateQuery += " WHERE strTrainNumber = :strTrainNumber AND strWagonNumber = :strWagonNumber AND nReachDatetime = :nReachDatetime";

    // 准备查询
    QSqlQuery query(db);
    if (!query.prepare(updateQuery)) {
        LOG("准备更新语句失败: " + query.lastError().text(), LogLevel::LOG_ERROR);
        LOG(updateQuery);
        return false;
    }

    // 绑定更新值
    if (updateParams.contains("leftImagePass")) {
        query.bindValue(":leftImagePass", updateParams.value("leftImagePass").toInt());
    }
    if (updateParams.contains("TopImagePass")) {
        query.bindValue(":TopImagePass", updateParams.value("TopImagePass").toInt());
    }
    if (updateParams.contains("RightImagePass")) {
        query.bindValue(":RightImagePass", updateParams.value("RightImagePass").toInt());
    }

    // 绑定条件值
    query.bindValue(":strTrainNumber", updateParams.value("strTrainNumber"));
    query.bindValue(":strWagonNumber", updateParams.value("strWagonNumber"));

    query.bindValue(":nReachDatetime", QDateTime::fromString( updateParams.value("nReachDatetime"),"yyyy-MM-dd hh:mm:ss").toMSecsSinceEpoch());

    // 执行更新
    if (!query.exec()) {
        LOG("更新失败: " + query.lastError().text(), LogLevel::LOG_ERROR);
        return false;
    }

    // 检查是否成功更新了记录
    if (query.numRowsAffected() == 0) {
        QString sql=generateDebugSQL(query.lastQuery(),query);
        LOG("更新失败:"+sql, LogLevel::LOG_WARNING);
        return false;
    }

    LOG("成功更新记录", LogLevel::LOG_INFO);
    return true;
}
// 创建数据库表（自动检查存在性）
bool CDatabaseManager::createLocomotiveTable() {
    // 检查表是否存在
    QSqlQuery checkQuery(db);
    if (!checkQuery.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='LocomotiveData'")) {
        LOG( "表检查失败:" + checkQuery.lastError().text(),LogLevel::LOG_WARNING);
        return false;
    }

    // 表已存在则跳过创建
    if (checkQuery.next()) {
        qDebug() << "表已存在，跳过创建";
        return true;
    }

    // 创建新表
    QSqlQuery createQuery(db);
    QString createSQL = R"(
        CREATE TABLE LocomotiveData (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            sectionNumber INTEGER NOT NULL,
            nFrontWheelTime INTEGER NOT NULL,
            frontWheelSpeed REAL NOT NULL,
            nRearWheelTime INTEGER NOT NULL,
            rearWheelSpeed REAL NOT NULL,
            model INTEGER NOT NULL,
            department INTEGER NOT NULL,
            trainNumber TEXT NOT NULL,
            state TEXT CHECK(LENGTH(state) = 1) NOT NULL,
            leftImagePath TEXT,
            TopImagePath TEXT,
            RightImagePath TEXT
        )
    )";

    if (!createQuery.exec(createSQL)) {
        LOG( "表创建失败:" + checkQuery.lastError().text(),LogLevel::LOG_ERROR);
        return false;
    }

    qDebug() << "机车数据表创建成功";
    return true;
}
// 插入机车数据
bool CDatabaseManager::insertLocomotiveData(const LocomotiveData &data) {
    QSqlQuery query(db);
    query.prepare(R"(
        INSERT INTO LocomotiveData (
            sectionNumber, nFrontWheelTime, frontWheelSpeed,
            nRearWheelTime, rearWheelSpeed, model,
            department, trainNumber, state,leftImagePath,
                               TopImagePath,
                               RightImagePath
        ) VALUES (
            :section, :frontTime, :frontSpeed,
            :rearTime, :rearSpeed, :model,
            :dept, :trainNum, :state,:leftImagePath,:TopImagePath,:RightImagePath
        )
    )");

    // 绑定参数值
    query.bindValue(":section", data.sectionNumber);
    query.bindValue(":frontTime", QVariant::fromValue(data.nFrontWheelTime));
    query.bindValue(":frontSpeed", data.frontWheelSpeed);
    query.bindValue(":rearTime", QVariant::fromValue(data.nRearWheelTime));
    query.bindValue(":rearSpeed", data.rearWheelSpeed);
    query.bindValue(":model", QVariant::fromValue(data.model));
    query.bindValue(":dept", data.department);
    query.bindValue(":trainNum", QVariant::fromValue(data.strTrainNumber));
    query.bindValue(":state", QChar(data.state));  // 将char转为QChar
    auto leftIt = data.imagePathMap.find("left");
    QString leftImagePath = (leftIt != data.imagePathMap.end()) ? leftIt->second : QString();
    query.bindValue(":leftImagePath",leftImagePath);
    auto topIt = data.imagePathMap.find("top");
    QString topImagePath = (topIt != data.imagePathMap.end()) ? topIt->second : QString();

    query.bindValue(":topImagePath",topImagePath);
    auto rightIt = data.imagePathMap.find("right");
    QString rightImagePath = (rightIt != data.imagePathMap.end()) ? rightIt->second : QString();
    query.bindValue(":rightImagePath",rightImagePath);


    if (!query.exec()) {
        LOG ("数据插入失败:" + query.lastError().text(),LogLevel::LOG_ERROR);
        return false;
    }

    qDebug() << "机车数据插入成功，ID:" << query.lastInsertId();
    return true;
}
// 检查用户名是否存在
bool CDatabaseManager::usernameExists(const QString &username) {
    QSqlQuery query(db);
    query.prepare("SELECT COUNT(*) FROM users WHERE username = :username");
    query.bindValue(":username", username);

    if (!query.exec()) {
        qDebug() << "Check username error:" << query.lastError();
        return false;
    }

    if (query.next()) {
        return query.value(0).toInt() > 0;
    }

    return false;
}
// 密码加密函数（使用SHA-256加密）
QString encryptPassword(const QString &password) {
    QByteArray hash = QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256);
    return QString(hash.toHex());
}

// 添加用户函数（检查用户名是否存在）
bool CDatabaseManager::addUser(const UserInfo &userInfo) {
    // 检查用户名是否存在
    if (usernameExists(userInfo.username)) {
        qDebug() << "Username already exists:" << userInfo.username;
        return false;
    }

    // 加密密码
    QString encryptedPassword = encryptPassword(userInfo.password);

    // 插入新用户
    QSqlQuery query(db);
    query.prepare(
        "INSERT INTO users (username, name, worknumber, password, cellphone) "
        "VALUES (:username, :name, :worknumber, :password, :cellphone)"
        );

    query.bindValue(":username", userInfo.username);
    query.bindValue(":name", userInfo.name);
    query.bindValue(":worknumber", userInfo.workNumber);
    query.bindValue(":password", encryptedPassword);
    query.bindValue(":cellphone", userInfo.cellphone);

    if (!query.exec()) {
        qDebug() << "Insert user error:" << query.lastError();
        return false;
    }

    qDebug() << "User added successfully:" << userInfo.username;
    return true;
}
// 用户登录函数
int CDatabaseManager::userLogin(const QString &username, const QString &password) {
    // 检查用户名是否存在
    if (!usernameExists(username)) {
        qDebug() << "Username does not exist:" << username;
        return 1;
    }

    // 加密输入的密码
    QString encryptedPassword = encryptPassword(password);

    // 验证用户名和密码
    QSqlQuery query(db);
    query.prepare("SELECT COUNT(*) FROM users WHERE username = :username AND password = :password");
    query.bindValue(":username", username);
    query.bindValue(":password", encryptedPassword);

    if (!query.exec()) {
        LOG("Login query error:" + query.lastError().text(),LogLevel::LOG_ERROR);
        return 4;
    }

    if (query.next()) {
        bool loginSuccess = query.value(0).toInt() > 0;
        if (loginSuccess) {
            qDebug() << "User login successful:" << username;

            // 可以在这里更新最后登录时间等操作
            QSqlQuery updateQuery(db);
            updateQuery.prepare("UPDATE users SET last_login = datetime('now') WHERE username = :username");
            updateQuery.bindValue(":username", username);
            updateQuery.exec();

            return 0;
        }
    }

    qDebug() << "Login failed: invalid password for user:" << username;
    return 3;
}

// 获取用户信息函数（登录成功后使用）
UserInfo CDatabaseManager::getUserInfo(const QString &username) {
    UserInfo userInfo;

    QSqlQuery query(db);
    query.prepare("SELECT username, name, worknumber, cellphone FROM users WHERE username = :username");
    query.bindValue(":username", username);

    if (query.exec() && query.next()) {
        userInfo.username = query.value("username").toString();
        userInfo.name = query.value("name").toString();
        userInfo.workNumber = query.value("worknumber").toString();
        userInfo.cellphone = query.value("cellphone").toString();
    } else {
        qDebug() << "Get user info error:" << query.lastError();
    }

    return userInfo;
}
CarriageInformation CDatabaseManager::queryCarriageInformation(const QMap<QString, QString>& mapKeyValue) {
    CarriageInformation info;
    QString strTrainNumber = mapKeyValue["trainnumber"];
    QString strWagonNumber = mapKeyValue["wagonnumber"];
    quint64 nStartDatetime = mapKeyValue["startdatetime"].toULong();
    quint64 nEndDatetime = mapKeyValue["enddatetime"].toULong();

    QSqlQuery query(db);
    QString sql = "SELECT * FROM carriage_information "
                  "WHERE strTrainNumber = :trainnumber "
                  "AND strWagonNumber = :wagonnumber "
                  "AND nReachDatetime BETWEEN :startdatetime AND :enddatetime "
                  "LIMIT 1";

    query.prepare(sql);
    query.bindValue(":trainnumber", strTrainNumber);
    query.bindValue(":wagonnumber", strWagonNumber);
    query.bindValue(":startdatetime", nStartDatetime);
    query.bindValue(":enddatetime", nEndDatetime);

    if (!query.exec()) {
        qDebug() << "Query failed:" << query.lastError();
        return info;
    }

    if (query.next()) {
        info.strTrainNumber = query.value("strTrainNumber").toString();
        info.nReachDatetime = query.value("nReachDatetime").toULongLong();
        info.nOrder = query.value("nOrder").toUInt();
        info.strWagonNumber = query.value("strWagonNumber").toString();
        info.strCarriageModel = query.value("strCarriageModel").toString();
        info.fConverredLength = query.value("fConverredLength").toFloat();
        info.strCarriageModelCode = query.value("strCarriageModelCode").toString();
        info.strFactoryAndDatetime = query.value("strFactoryAndDatetime").toString();
        info.nFrontWheelTime = query.value("nFrontWheelTime").toULongLong();
        info.frontWheelSpeed = query.value("frontWheelSpeed").toFloat();
        info.nRearWheelTime = query.value("nRearWheelTime").toULongLong();
        info.rearWheelSpeed = query.value("rearWheelSpeed").toFloat();

        info.imagePathMap["left"] = query.value("leftImagePath").toString();
        info.imagePathMap["top"] = query.value("TopImagePath").toString();
        info.imagePathMap["right"] = query.value("RightImagePath").toString();
    }

    return info;
}
/**
 * @brief 通过车次和到达时间查询列车信息
 * @param strTrainNumber 车次号
 * @param nReachDatetime 到达时间（时间戳）
 * @param[out] trainInfo 查询到的列车信息
 * @return bool 查询成功返回true，失败返回false
 */
bool CDatabaseManager::getTrainInfoByNumberAndTime(const QString& strTrainNumber,
                                                   quint64 nReachDatetime,
                                                   TrainInformation& trainInfo)
{
    // 初始化输出参数
    trainInfo.Init();

    // 准备SQL查询语句
    QString querySQL = R"(
        SELECT
            strTrainNumber,
            nReachDatetime,
            strDirection,
            nNumberOfAxles,
            nNumberOfCarriage,
            strFilePath,
            strDetection,
            nInspectionDatetime
        FROM TrainInformation
        WHERE strTrainNumber = :trainNumber
        AND nReachDatetime = :reachTime
        LIMIT 1
    )";

    QSqlQuery query;
    query.prepare(querySQL);
    query.bindValue(":trainNumber", strTrainNumber);
    query.bindValue(":reachTime", nReachDatetime);

    // 执行查询
    if (!query.exec()) {
        LOG("查询列车信息失败: " + query.lastError().text(), LogLevel::LOG_ERROR);
        return false;
    }

    // 处理查询结果
    if (query.next()) {
        // 填充列车信息结构体
        trainInfo.strTrainNumber = query.value("strTrainNumber").toString();
        trainInfo.nReachDatetime = query.value("nReachDatetime").toULongLong();
        trainInfo.strDirection = query.value("strDirection").toString();
        trainInfo.nNumberOfAxles = query.value("nNumberOfAxles").toInt();
        trainInfo.nNumberOfCarriage = query.value("nNumberOfCarriage").toInt();
        trainInfo.strFilePath = query.value("strFilePath").toString();
        trainInfo.strDetection = query.value("strDetection").toString();
        trainInfo.nInspectionDatetime = query.value("nInspectionDatetime").toULongLong();

        return true;
    } else {
        LOG(QString("未找到车次[%1]在时间[%2]的列车信息")
                .arg(strTrainNumber)
                .arg(QDateTime::fromSecsSinceEpoch(nReachDatetime).toString("yyyy-MM-dd hh:mm:ss")),
            LogLevel::LOG_INFO);
        return false;
    }
}
// 函数：根据到达时间范围检索TrainInformation
QList<TrainInformation> CDatabaseManager::getTrainInfoByReachTimeRange(quint64 startTime, quint64 endTime) {
    QList<TrainInformation> resultList;


    if (!db.isOpen()) {
        qWarning() << "Database is not open!";
        return resultList;
    }

    // 准备SQL查询
    QSqlQuery query(db);
    query.prepare(R"(
        SELECT
            strTrainNumber,
            nReachDatetime,
            strDirection,
            nNumberOfAxles,
            nNumberOfCarriage,
            strFilePath,
            strDetection,
            nInspectionDatetime,
            strInspectionStatus
        FROM TrainInformation
        WHERE nReachDatetime BETWEEN :startTime AND :endTime
        ORDER BY nReachDatetime ASC
    )");

    // 绑定参数
    query.bindValue(":startTime", startTime);
    query.bindValue(":endTime", endTime);

    // 执行查询
    if (!query.exec()) {
        qWarning() << "Query failed:" << query.lastError().text();
        return resultList;
    }

    // 处理查询结果
    while (query.next()) {
        TrainInformation info;
        info.Init(); // 初始化结构体

        // 从查询结果中提取数据
        info.strTrainNumber = query.value("strTrainNumber").toString();
        info.nReachDatetime = query.value("nReachDatetime").toULongLong();
        info.strDirection = query.value("strDirection").toString();
        info.nNumberOfAxles = query.value("nNumberOfAxles").toInt();
        info.nNumberOfCarriage = query.value("nNumberOfCarriage").toInt();
        info.strFilePath = query.value("strFilePath").toString();
        info.strDetection = query.value("strDetection").toString();
        info.nInspectionDatetime = query.value("nInspectionDatetime").toULongLong();
        info.strInspectionState = query.value("strInspectionStatus").toString();

        // 添加到结果列表
        resultList.append(info);
    }

    return resultList;
}
/*
 * 根据车次、到达时间、检视状态查询车厢信息
 * @param trainNo   车次
 * @param reachTime 到达时间（Unix 时间戳，秒）
 * @param state     检视状态，-1 表示忽略该条件
 * @return          查询到的列表
 */
std::vector<CarriageInformation>
CDatabaseManager::queryCarriageList(const QString &trainNo,
                                    quint64 reachTime,
                                    int state)
{
    std::vector<CarriageInformation> list;

    QSqlQuery q(db); // 默认连接
    q.prepare(
        "SELECT "
        "strTrainNumber, "
        "nReachDatetime, "
        "nOrder, "
        "strWagonNumber, "
        "strCarriageModel, "
        "fConverredLength, "
        "strCarriageModelCode, "
        "strFactoryAndDatetime, "
        "nFrontWheelTime, "
        "frontWheelSpeed, "
        "nRearWheelTime, "
        "rearWheelSpeed, "
        "leftImagePath, "
        "TopImagePath, "
        "RightImagePath, "
        "inspectorState, "
        "leftImagePass, "
        "TopImagePass, "
        "RightImagePass "
        "FROM carriage_information "
        "WHERE strTrainNumber = :trainNo "
        "  AND nReachDatetime = :reachTime "
        "  AND (:state = -1 OR inspectorState = :state) "
        "ORDER BY nOrder");

    q.bindValue(":trainNo",   trainNo);
    q.bindValue(":reachTime", QVariant::fromValue<qint64>(reachTime));
    q.bindValue(":state",     state);

    if (!q.exec())
    {
        LOG( "queryCarriageList failed:" + q.lastError().text(), LOG_ERROR);
        return list;
    }

    while (q.next())
    {
        CarriageInformation ci;
        ci.strTrainNumber      = q.value("strTrainNumber").toString();
        ci.nReachDatetime      = q.value("nReachDatetime").toULongLong();
        ci.nOrder              = q.value("nOrder").toUInt();
        ci.strWagonNumber      = q.value("strWagonNumber").toString();
        ci.strCarriageModel    = q.value("strCarriageModel").toString();
        ci.fConverredLength    = q.value("fConverredLength").toFloat();
        ci.strCarriageModelCode= q.value("strCarriageModelCode").toString();
        ci.strFactoryAndDatetime = q.value("strFactoryAndDatetime").toString();
        ci.nFrontWheelTime     = q.value("nFrontWheelTime").toULongLong();
        ci.frontWheelSpeed     = q.value("frontWheelSpeed").toFloat();
        ci.nRearWheelTime      = q.value("nRearWheelTime").toULongLong();
        ci.rearWheelSpeed      = q.value("rearWheelSpeed").toFloat();

        ci.imagePathMap["left"]  = q.value("leftImagePath").toString();
        ci.imagePathMap["top"]   = q.value("TopImagePath").toString();
        ci.imagePathMap["right"] = q.value("RightImagePath").toString();

        ci.inspectorState        = q.value("inspectorState").toBool();

        ci.inspectorResult["left"]  = q.value("leftImagePass").toBool();
        ci.inspectorResult["top"]   = q.value("TopImagePass").toBool();
        ci.inspectorResult["right"] = q.value("RightImagePass").toBool();

        list.emplace_back(std::move(ci));
    }
    return list;
}
