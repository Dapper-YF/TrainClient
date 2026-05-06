#ifndef CDATAPROTOCOL_H
#define CDATAPROTOCOL_H

#include <QtGlobal>
#include <QDateTime>
#include "../include/define.h"
#include <QThread>
#include <QMap>
#include <QFile>
#include <QMutex>
using RequestProcess=std::function<void(QMap<QString,QString>,int descriptor)> ;
// 时间结构体定义
struct TimeStruct {
    quint16 year;    // 年份 (完整年份，如2023)
    quint8 month;    // 月份 (1-12)
    quint8 day;      // 日期 (1-31)
    quint8 weekday;  // 星期 (1=周一, 7=周日)
    quint8 hour;     // 小时 (0-23)
    quint8 minute;   // 分钟 (0-59)
    quint8 second;   // 秒 (0-59)
};

QDateTime timeStructToQDateTime(const TimeStruct &ts);
TimeStruct qDateTimeToTimeStruct(const QDateTime &dt);
QString getCurrentTrainImagePath();
template<typename T>
float readBigEndianT(const QByteArray &bigEndian4Bytes)
{
    Q_ASSERT(bigEndian4Bytes.size() == sizeof(T));

    QDataStream ds(bigEndian4Bytes);
    ds.setByteOrder(QDataStream::BigEndian);              // 1. 大端
    ds.setFloatingPointPrecision(QDataStream::SinglePrecision); // 2. 单精度

    T f = 0;
    ds >> f;                                              // 3. 直接读出
    return f;
}
// 修改线程处理类，添加最大包大小参数
class CarriagePictureProcessor : public QThread
{
    Q_OBJECT

public:
    CarriagePictureProcessor(int descriptor, const QString &filePath, qint64 maxPacketSize, QObject *parent = nullptr);

protected:
    void run() override;

private:
    int m_descriptor;
    QString m_filePath;
    qint64 m_maxPacketSize;
};
class CDataProtocol
{
public:
    CDataProtocol();
    static int m_curFileID;
    static QMutex m_mutex;  // 互斥锁成员变量
    static TimeStruct ExtractDateTime(char *pFrame);
    static bool AnalyzeFrame(int descriptor, char *pFrame, quint32 nLen);

    static bool ResponseTimeVerify(int descriptor, char *pFrame, quint32 nLen);

    static bool AnalyzeCommonReq(int descriptor, const char *pData, quint32 nLen,RequestProcess process);
    //解析用户注册请求指令，pData不包括报文长度及指令代码
    static bool AnalyzeRegisterReq(int descriptor, const char *pData, quint32 nLen);
    //解析用户登录请求
    static bool AnalyzeLoginReq(int descriptor, const char *pData, quint32 nLen);
    //解析用户注销请求
    static bool AnalyzeLogoutReq(int descriptor, const char *pData, quint32 nLen);
    //解析车箱查询请求报文
    static bool AnalyzeTrainSearchReq(int descriptor, const char *pData, quint32 nLen);
    //解析车箱查询请求报文
    static bool AnalyzeCarriageSearchReq(int descriptor, const char *pData, quint32 nLen);
    //解析车次信息查询请求报文
    static bool AnalyzeTrainNumberSearchReq(int descriptor, const char *pData, quint32 nLen);
    //解析车辆三视图文件请求
    static bool AnalyzeCarriagePictureReq(int descirptor, const char *pData, quint32 nLen);

    //校验来车信息
    static bool validateTrainArrivalData(const char *pData, quint32 nLen);
    //解析车来信息
    static bool AnalyzeTrainComeInfo(const char *pData, quint32 nLen);
    //解析车离开信息
    static bool AnalyzeTrainLeaveInfo(const char *pData, quint32 nLen);
    //解析机车数据信息
    static bool AnalyzeLocomotiveDatasInfo(const char *pData, quint32 nLen);
    //解析车辆数据信息
    static bool AnalyzeCarriageDatasInfo(const char *pData, quint32 nLen);
    //解析火车列表请求数据信息
    static bool AnalyzeTrainListReq(int descriptor, const char *pData, quint32 nLen);
    //解析火车待检车箱列表请求数据信息
    static bool AnalyzeCarriageUnInspectorListReq(int descriptor, const char *pData, quint32 nLen);
    //解析火车所有车箱列表请求数据信息
    static bool AnalyzeCarriageListReq(int descriptor, const char *pData, quint32 nLen);
    static quint32 PackCommonRes(quint8 cmd,const char *pData, quint32 nLen, int nErrorCode, QByteArray& array);
    //填写用户注册响应报文
    static quint32 PackRegisterRes(const char *pData, quint32 nLen, int nErrorCode, QByteArray& array);
    //填写用户登录响应报文
    static quint32 PackLoginRes(/*const char *pData, quint32 nLen,*/ int nErrorCode, QByteArray& array,UserInfo *user=nullptr);
    //填写用户注销响应报文
    static quint32 PackLogoutRes(const char *pData, quint32 nLen, int nErrorCode, QByteArray& array);
    //填写货车查询响应报文
    static quint32 PackTrainSearchRes(const QList<TrainInformation>& trainInfoList, QByteArray& array);
    //填写车次信息查询响应报文
    static quint32 PackTrainNumberSearchRes(const QList<CarriageInformation>& carriageInfoList, const QString& strTrainNumber, quint32 nReachDatetime, QByteArray& array);
    //填写车辆三视图文件响应报文
    static quint32 PackCarriagePictureRes(const QString& filePath, std::vector<QByteArray>& array, qint64 maxPacketSize);
    static quint32 PackCarriagePictureResAndSend(const QString& filePath, qint64 maxPacketSize,int descriptor);
    //填写对时报文
    static quint32 PackTimeSynCom(QByteArray& array);
    // 移除登录用户，网络断开时需要
    static void removeUser(int id) {
        auto it = loginedUsers.find(id);
        if (it != loginedUsers.end()) {
            loginedUsers.erase(it);
        }
    }
private:
    // static quint8 CalcCheckCode(char *pFrame, quint32 nLen);
    static quint16 CalcCheckCode16(char *pFrame, quint32 nLen);

    //解析字符串key=value,key=value...
    static void AnalyzeStringtoKeyValue(const QString& str, QMap<QString, QString>& mapKeyValue);

    //解析时间
    static quint32 AnalyzeDatetime(const char *pData, QDateTime& dt);
    //计算机车和车箱拍摄时间范围
    static QPair<quint64, quint64> calculateVehicleInCameraRange(
        quint64 frontWheelTime,
        quint64 rearWheelTime,
        double frontSpeed,
        double rearSpeed,
        quint64 standardDelay);
    static std::map<int,QString> loginedUsers;
    //判断用户是否登录
    static bool isUserLogined(const QString &username) {
        return std::any_of(loginedUsers.begin(), loginedUsers.end(),
                           [&username](const std::pair<int, QString> &pair) {
                               return pair.second == username;
                           });
    }

    // 移除指定用户名的所有记录
    static void removeUserByUsername(const QString &username) {
        // 使用迭代器遍历map
        for (auto it = loginedUsers.begin(); it != loginedUsers.end(); ) {
            if (it->second == username) {
                // 删除匹配的元素，并获取下一个元素的迭代器
                it = loginedUsers.erase(it);
            } else {
                // 继续下一个元素
                ++it;
            }
        }
    }
    static QByteArray buildTrainListResponsePacket(const QString &trainListData);
    static QByteArray buildCarriageListResponsePacket(const QString &trainListData,quint8 cmd=0x6A);
    static void saveCarriageData(const CarriageInformation &info);
    static void processOCRCarriageNumber(CarriageInformation info);
    static void processCarriageData(CarriageInformation info);
    static QString MapToString(QMap<QString,QString> maps);
};

#endif // CDATAPROTOCOL_H
