#ifndef TRAINAPI_H
#define TRAINAPI_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>

// 车厢信息
struct CarriageInfo2 {
    int id = 0;
    int number = 0;         // 车厢序号 (1-based)
    int nOrder = 0;         // 车厢顺序号 (nOrder from API)
    QString trainNumber;
    quint64 reachDatetime = 0;
    bool inspected = false;
    QString inspectionStatus;
    bool leftPassed = false;
    bool rightPassed = false;
    bool topPassed = false;
    QString remarks;
    QStringList alarms;
    QString leftImagePath;       // 后端返回的路径
    QString rightImagePath;
    QString topImagePath;
    QString localLeftImage;      // ImagePathMapper 映射后的本地路径
    QString localRightImage;
    QString localTopImage;
};

// 列车信息
struct TrainInfo2 {
    int id = 0;
    QString trainNumber;
    quint64 reachDatetime = 0;
    QString direction;
    int numberOfAxles = 0;
    int numberOfCarriage = 0;
    QString inspectionStatus;
    quint64 inspectionDatetime = 0;
    QString strDetection;          // 检测站/站别
    QString leftImagePath;
    QString rightImagePath;
    QString topImagePath;
    // 检视进度
    int inspectionPassNumber = 0;    // 合格车厢数
    int inspectionDispassNumber = 0; // 不合格车厢数
    int uninspectionNumber = 0;     // 未检车厢数
};

// API 结果回调
class TrainApiClient : public QObject
{
    Q_OBJECT
public:
    explicit TrainApiClient(QObject* parent = nullptr);

    // 登录（独立于 token 验证）
    void login(const QString& username, const QString& password);

    // 获取列车列表（需 token）
    void getTrainsByDate(const QString& date);
    void getTrainsByRange(quint64 startTs, quint64 endTs);

    // 获取车厢列表（需 token）
    void getCarriages(const QString& trainNumber, quint64 reachDatetime);

    // 提交车厢检车结果（需 token）
    // 用 trainNumber + carriageNumber 定位车厢（复合键，非数字ID）
    void submitCarriageInspection(const QString& trainNumber, int carriageNumber, quint64 reachDatetime, const QJsonObject& data);

    // 获取统计信息（需 token）
    void getStats(const QString& date);

    // 个人账户（需 token）
    void getUserProfile(const QString& username);
    void updateUserProfile(const QString& username, const QJsonObject& data);
    void changePassword(const QString& currentPassword, const QString& newPassword);

    // 注册新用户（管理员）
    void createUser(const QString& username, const QString& name, const QString& worknumber,
                    const QString& password, const QString& unit = QString(),
                    const QString& cellphone = QString());

    // API 基础地址
    QString apiBase() const { return m_apiBase; }

signals:
    // 登录结果
    void loginSuccess(const QString& token, const QJsonObject& userData);
    void loginFailed(const QString& error);

    // 数据返回
    void trainsReady(const QList<TrainInfo2>& trains);
    void carriagesReady(const QList<CarriageInfo2>& carriages);
    void submitDone(bool success, const QString& msg);

    // 网络错误（包含 401 未授权）
    void error(const QString& err);
    void unauthorized();  // 401 时触发

    // 统计结果
    void statsReady(int todayTrainCount, int pendingCount, int alarmCount);

    // 账户结果
    void userProfileReady(const QJsonObject& profile);
    void userProfileUpdated(bool success, const QJsonObject& profile);
    void passwordChanged(bool success, const QString& error);
    void userCreated(bool success, const QString& error);

private slots:

private:
    void doGetRequest(const QString& url);
    void doPutRequest(const QString& url, const QJsonObject& body);

    QNetworkAccessManager* m_nam = nullptr;
    QString m_apiBase;
    QString m_token;

    QNetworkRequest makeRequest(const QUrl& url);
    void handleHttpError(QNetworkReply* reply);

};

#endif // TRAINAPI_H
