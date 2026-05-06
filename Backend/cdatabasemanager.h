#ifndef CDATABASEMANAGER_H
#define CDATABASEMANAGER_H

#include <QSqlDatabase>
#include <QSqlError>
#include <QMap>
#include <QList>
#include <QVariant>
#include "../include/define.h"

// 用户状态枚举
enum UserStatus {
    USER_ACTIVE = 0,
    USER_DISABLED = 1
};

// 图像检视状态枚举
enum ImageInspectionStatus {
    IMAGE_NOT_INSPECTED = 0,
    IMAGE_PASS = 1,
    IMAGE_FAIL = 2
};

class CDatabaseManager
{
public:
    static CDatabaseManager* Instance() {
        if(m_instance == nullptr) {
            m_instance = new CDatabaseManager();
        }
        return m_instance;
    }

    // 现有函数
    bool insertTrainRecord(const TrainInformation &train);
    bool insertLocomotiveData(const LocomotiveData &data);
    bool insertCarriage(const CarriageInformation &info);
    bool addUser(const UserInfo &userInfo);
    int userLogin(const QString &username, const QString &password);
    UserInfo getUserInfo(const QString &username);
    CarriageInformation queryCarriageInformation(const QMap<QString, QString>& mapKeyValue);
    std::vector<CarriageInformation> queryCarriageList(const QString &trainNo, quint64 reachTime, int state);
    bool getTrainInfoByNumberAndTime(const QString& strTrainNumber, quint64 nReachDatetime, TrainInformation& trainInfo);
    QList<TrainInformation> getTrainInfoByReachTimeRange(quint64 startTime, quint64 endTime);
    bool inspectCarriageInspection(const QMap<QString, QString> &updateParams);
    bool updateTrainInformationEx(const QMap<QString, QString> &updateParams, const QMap<QString, QString> &conditionParams);

    // Web服务新增函数
    // 用户认证相关
    QMap<QString, QVariant> authenticateUser(const QString &username, const QString &password);
    bool validateToken(const QString &token);
    QString getUsernameByToken(const QString &token);
    bool changeUserPassword(const QString &username, const QString &currentPassword, const QString &newPassword);

    // 列车信息相关
    QMap<QString, QVariant> getTrainList(const QMap<QString, QString> &queryParams);
    bool updateTrainInspectionStatus(int trainId, const QString &status, quint64 inspectionTime);

    // 车厢信息相关
    QMap<QString, QVariant> getCarriageList(const QString &trainNumber, quint64 reachDatetime);
    bool updateCarriageInspection(int carriageId, const QMap<QString, QVariant> &inspectionData);

    // 统计相关
    QMap<QString, QVariant> getStats(const QString &date);  // date: YYYY-MM-DD格式

    // 车厢标注相关
    QMap<QString, QVariant> getCarriageAnnotation(const QString &trainNumber, qint64 reachDatetime, int order);
    bool saveCarriageAnnotation(const QString &trainNumber, qint64 reachDatetime, int order, const QJsonObject &data);

    // 用户管理相关
    QMap<QString, QVariant> getUserList(const QMap<QString, QString> &queryParams);
    QMap<QString, QVariant> getUserByUsername(const QString &username);
    bool createUser(const QMap<QString, QVariant> &userData);
    bool updateUser(const QString &username, const QMap<QString, QVariant> &userData);
    bool resetUserPassword(const QString &username, const QString &newPassword);
    bool setUserStatus(const QString &username, int status);
    bool updateInspectionStatus(int carriageId);

private:
    CDatabaseManager();
    QSqlDatabase db;

    bool createTrainInformationTable();
    bool createLocomotiveTable();
    bool createCarriageTable();
    bool createUserTable();
    bool createTokenTable();

    bool usernameExists(const QString &username);
    QString generateToken();
    QString encryptPassword(const QString &password);
    // 内存中保存用户token的映射表
    QMap<QString, QVariant> m_userTokens;

    // 辅助函数
    void deleteTokenFromDatabase(const QString &username);
    bool saveTokenToDatabase(const QString &username, const QString &token, quint64 createdTime, quint64 expiresTime);
    void saveTokenToMemory(const QString &username, const QString &token, quint64 createdTime, quint64 expiresTime);

    static CDatabaseManager *m_instance;
};

#endif // CDATABASEMANAGER_H
