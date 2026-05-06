#include <QFileInfo>
#include "cdataprotocol.h"
#include <cstring>
#include "logger.h"
#include "ctraintcpserver.h"
#include "MVS/clinecameramanager.h"
#include <QDataStream>
#include "cdatabasemanager.h"
#include "csystemconfig.h"
#include "Utils/imageutil.h"
#include <QJsonArray>
#include <QJsonObject>
#include "imageuploader.h"
#include <QtConcurrent/QtConcurrent>
std::map<int,QString> CDataProtocol::loginedUsers;
// 修改1：使用动态计算的最大传输值，并添加协议头来避免粘包
#define DEFAULT_PACKET_SIZE 65536  // 默认包大小，但会根据实际情况调整
int CDataProtocol::m_curFileID=0;
 QMutex CDataProtocol::m_mutex;
bool CDataProtocol::AnalyzeFrame(int descriptor, char *pFrame, quint32 nLen)
{
    QByteArray logArray(pFrame,nLen);
    LOG("收到指令信息："+logArray.toHex('-'));
    if (nullptr == pFrame || nLen <= 4)
    {
        Logger::instance()->Log("数据帧报文长度错误！");
        return false;
    }

    //data length
    quint32 dataLen;
    memcpy(&dataLen, pFrame, 4);
    if (nLen < dataLen + 4)
    {
        Logger::instance()->Log("数据帧报文长度错误！");
        return false;
    }

    //校验
    //quint8 nCalcCheck = CalcCheckCode(pFrame, dataLen + 4 - 1);
    //quint8 nFrameCheck = (quint8)pFrame[dataLen + 4 - 1];
    quint16 nCalcCheck = CalcCheckCode16(pFrame, dataLen + 4 - 2);
    quint16 nFrameCheck;
    memcpy(&nFrameCheck, pFrame + dataLen + 4 - 2, 2);
    if (nCalcCheck != nFrameCheck)
    {
        Logger::instance()->Log("校验码不匹配！正确校验码："+QString::number(nCalcCheck,16)+"收到校验码:"+QString::number(nFrameCheck,16));
        // return false;
    }

    //指令码为2个字节
    quint8 nCommandCode = pFrame[4];
    switch (nCommandCode)
    {
    case 0x01://对时指令
        ResponseTimeVerify(descriptor,pFrame,nLen);
        LOG("对时指令处理成功");
        break;
    case 0x04: //车来信息
        AnalyzeTrainComeInfo(pFrame+6, nLen - 8);
        LOG("车来信息处理成功");
        break;
    case 0x08: //车离开信息
        AnalyzeTrainLeaveInfo(pFrame+6, nLen - 8);
        LOG("车离开信息处理成功");
        break;
    case 0x16: //机车数据
        AnalyzeLocomotiveDatasInfo(pFrame+6, nLen - 8);
        LOG("机车数据处理成功");
        break;
    case 0x32: //车箱数据
        AnalyzeCarriageDatasInfo(pFrame+6, nLen - 8);
        LOG("车箱数据处理成功");
        break;
    case 0x50: //身份信息
        CTrainTcpServer::instance()->SetClientIdentity(descriptor, true);
        CTrainTcpServer::instance()->SendDatasToClient(descriptor, pFrame, nLen);
        LOG("身份信息接收和回应成功");
        break;
    case 0x51: //用户注册
        AnalyzeRegisterReq(descriptor, pFrame+6, nLen - 8);
        break;
    case 0x53: //用户登录
        AnalyzeLoginReq(descriptor, pFrame + 6, nLen - 8);
        break;
    case 0x55: //用户注销
        AnalyzeLogoutReq(descriptor, pFrame + 6, nLen - 8);
        break;
    case 0x61: //货车查询
        AnalyzeTrainSearchReq(descriptor, pFrame + 6, nLen - 8);
        break;
    case 0x63: //车次查询
        AnalyzeCarriageSearchReq(descriptor, pFrame + 6, nLen - 8);
        break;
    case 0x65: //车辆三视图文件请求
        AnalyzeCarriagePictureReq(descriptor, pFrame + 6, nLen - 8);
        break;
    case 0x67://火车列表查询
    {
        // QByteArray byteArray = QByteArray::fromRawData(pFrame, nLen);
        // LOG(byteArray.toHex('-'));
        AnalyzeTrainListReq(descriptor,pFrame+5,nLen-8);
        break;
    }
    case 0x69://列车待检车箱列表查询
        AnalyzeCarriageUnInspectorListReq(descriptor,pFrame+5,nLen-8);
        break;
    case 0x6B://车辆检视请求
        // LOG("收到车辆检视请求");
        AnalyzeCommonReq(descriptor,pFrame+6,nLen-8,
                         [](QMap<QString,QString> infos,int descriptor){
                             bool updateResult=CDatabaseManager::Instance()->inspectCarriageInspection(infos);
                             infos.remove("leftImagePass");
                             infos.remove("TopImagePass");
                             infos.remove("RightImagePass");
                             auto response=MapToString(infos).toLocal8Bit();
                             QByteArray array;
                             PackCommonRes(0x6C,response.data(),response.size(),updateResult?0:1,array);
                             CTrainTcpServer::instance()->SendDatasToClient(descriptor, array);
                         });
        break;
    case 0x6D://列车检视修改
        AnalyzeCommonReq(descriptor,pFrame+6,nLen-8,[](QMap<QString,QString> infos,int descriptor){
            QMap<QString,QString> controlParams{{"strTrainNumber",infos.value("strTrainNumber")},{"nReachDatetime",QString::number(QDateTime::fromString(infos.value("nReachDatetime"),"yyyy-MM-dd hh:mm:ss").toMSecsSinceEpoch())}};
            QMap<QString,QString> valueParams{{"strInspectionStatus",infos.value("strInspectionStatus")},{"nInspectionDatetime",infos.value("nInspectionDatetime")}};
            bool updateResult=CDatabaseManager::Instance()->updateTrainInformationEx(valueParams,controlParams);
            auto response=MapToString(controlParams).toLocal8Bit();
            QByteArray array;
            PackCommonRes(0x6E,response.data(),response.size(),updateResult?0:1,array);
            CTrainTcpServer::instance()->SendDatasToClient(descriptor, array);
            LOG("列车检视修改:"+infos.value("strTrainNumber")+","+infos.value("nReachDatetime")+","+(updateResult?"成功":"失败"));
        });
        break;
    case 0x6F://列车待检车箱列表查询
        LOG("收到车箱列表查询请求");
        AnalyzeCarriageListReq(descriptor,pFrame+5,nLen-8);
        break;
    default:
        LOG("指令解析错误，收到无法处理的指令代码："+ QString::number(nCommandCode,16));
        break;
    }

    return true;
}
quint32 CDataProtocol::PackCarriagePictureRes(const QString &filePath, std::vector<QByteArray> &array, qint64 maxPacketSize)
{
    // 如果未指定最大包大小，使用默认值
    if (maxPacketSize <= 0) {
        maxPacketSize = DEFAULT_PACKET_SIZE;
    }

    // 读取文件数据
    QFile file(filePath);
    if (!file.exists()) {
        LOG("File not found: " + filePath, LogLevel::LOG_WARNING);
        return 0;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        LOG("Cannot open file: " + filePath, LogLevel::LOG_WARNING);
        return 0;
    }

    // 获取文件大小但不一次性读取所有数据
    quint64 fileSize = file.size();
    file.close();

    QByteArray filePathData = filePath.toUtf8();

    // 计算固定头部长度（不包括报文长度、协议头和校验码）
    quint32 fixedHeaderLen = 1 + 2 + filePathData.size() + 2 + 2 + 4; // 指令代码+文件路径长度+文件路径+总包数+序号+图像长度

    // 计算每个包的最大数据长度，预留空间给协议头
    quint32 headerAndFooterSize = 4 + 8 + 2; // 报文长度(4) + 协议头(8) + 校验码(2)
    quint32 maxDataLen = maxPacketSize - headerAndFooterSize - fixedHeaderLen;

    if (maxDataLen <= 0) {
        LOG("File path too long for packet structure: " + filePath, LogLevel::LOG_WARNING);
        return 0;
    }

    quint16 totalCount = (fileSize + maxDataLen - 1) / maxDataLen;
    quint16 order = 0;
    quint64 bytesRemaining = fileSize;
    quint64 bytesSent = 0;

    // 重新打开文件进行流式读取
    if (!file.open(QIODevice::ReadOnly)) {
        LOG("Cannot reopen file for streaming: " + filePath, LogLevel::LOG_WARNING);
        return 0;
    }

    // 生成文件传输的唯一标识，用于接收端识别数据包属于同一个文件
    quint32 fileTransferId =m_curFileID++; //QDateTime::currentDateTime().toMSecsSinceEpoch();

    while (bytesRemaining > 0) {
        QByteArray packet;
        quint32 packetLen=0;
        // 计算当前包的数据长度
        quint32 currentDataLen = bytesRemaining > maxDataLen ? maxDataLen : bytesRemaining;

        // 读取当前包的数据
        QByteArray fileData = file.read(currentDataLen);
        if (fileData.size() != currentDataLen) {
            LOG("File read error: " + filePath, LogLevel::LOG_WARNING);
            file.close();
            return array.size();
        }

        // 计算报文长度（不包括报文长度和校验码字段）
        // quint32 packetLength = fixedHeaderLen + currentDataLen + 8; // +8 为协议头

        // // 构建报文 - 添加防粘包协议头
        // packet.append((char*)&packetLength, 4);                    // 报文长度

        // 协议头：文件传输ID(4) + 总包数(2) + 当前包序号(2)
        packet.append((char*)&fileTransferId, 4);                  // 文件传输唯一标识
        packetLen+=4;
        packet.append((char*)&totalCount, 2);                      // 总包数
        packetLen+=2;
        packet.append((char*)&order, 2);                           // 当前包序号
        packetLen+=2;

        packet.append(static_cast<char>(0x66));                    // 指令代码
        packetLen+=1;

        quint16 filePathLen = filePathData.length();
        packet.append((char*)&filePathLen, 2);                     // 文件路径长度
        packetLen+=2;
        packet.append(filePathData);                               // 文件路径
        packetLen+=filePathLen;

        // 添加当前包数据长度
        packet.append((char*)&currentDataLen, 4);                  // 当前包数据长度
        packetLen+=4;
        packet.append(fileData);                                   // 文件数据
        packetLen+=currentDataLen;
        packet.insert(0, (char *)&packetLen+2, 4);
        // 计算校验码（不包括报文长度字段）
        quint16 nCheckCode = CalcCheckCode16(packet.data() , packet.length());
        packet.append((char*)&nCheckCode, 2);                      // 校验码
        packetLen += 2;

        array.push_back(packet);


        bytesRemaining -= currentDataLen;
        order++;
    }

    file.close();

    LOG(QString("File %1 packed into %2 packets, transfer ID: %3, max packet size: %4")
            .arg(filePath).arg(array.size()).arg(fileTransferId).arg(maxPacketSize),
        LogLevel::LOG_INFO);

    return array.size();
}
quint32 CDataProtocol::PackCarriagePictureResAndSend(const QString &filePath,  qint64 maxPacketSize,int descriptor)
{
    // 使用互斥锁保护整个函数，确保线程安全
    QMutexLocker locker(&m_mutex); // 假设类中有 QMutex m_mutex 成员变量
    // 如果未指定最大包大小，使用默认值
    if (maxPacketSize <= 0) {
        maxPacketSize = DEFAULT_PACKET_SIZE;
    }

    // 读取文件数据
    QFile file(filePath);
    if (!file.exists()) {
        LOG("File not found: " + filePath, LogLevel::LOG_WARNING);
        return 0;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        LOG("Cannot open file: " + filePath, LogLevel::LOG_WARNING);
        return 0;
    }

    // 获取文件大小但不一次性读取所有数据
    quint64 fileSize = file.size();
    file.close();

    QByteArray filePathData = filePath.toUtf8();

    // 计算固定头部长度（不包括报文长度、协议头和校验码）
    quint32 fixedHeaderLen = 1 + 2 + filePathData.size() + 2 + 2 + 4; // 指令代码+文件路径长度+文件路径+总包数+序号+图像长度

    // 计算每个包的最大数据长度，预留空间给协议头
    quint32 headerAndFooterSize = 4 + 8 + 2; // 报文长度(4) + 协议头(8) + 校验码(2)
    quint32 maxDataLen = maxPacketSize - headerAndFooterSize - fixedHeaderLen;

    if (maxDataLen <= 0) {
        LOG("File path too long for packet structure: " + filePath, LogLevel::LOG_WARNING);
        return 0;
    }

    quint16 totalCount = (fileSize + maxDataLen - 1) / maxDataLen;
    quint16 order = 0;
    quint64 bytesRemaining = fileSize;
    quint64 bytesSent = 0;

    // 重新打开文件进行流式读取
    if (!file.open(QIODevice::ReadOnly)) {
        LOG("Cannot reopen file for streaming: " + filePath, LogLevel::LOG_WARNING);
        return 0;
    }

    // 生成文件传输的唯一标识，用于接收端识别数据包属于同一个文件
    quint32 fileTransferId =m_curFileID++; //QDateTime::currentDateTime().toMSecsSinceEpoch();
    int count=0;
    while (bytesRemaining > 0) {
        QByteArray packet;
        packet.append(0x66);
        // 计算当前包的数据长度
        quint32 currentDataLen = bytesRemaining > maxDataLen ? maxDataLen : bytesRemaining;

        // 读取当前包的数据
        QByteArray fileData = file.read(currentDataLen);
        if (fileData.size() != currentDataLen) {
            LOG("File read error: " + filePath, LogLevel::LOG_WARNING);
            file.close();
            return count;
        }

        // // 计算报文长度（不包括报文长度和校验码字段）
        // quint32 packetLength = fixedHeaderLen + currentDataLen + 8; // +8 为协议头

        // // 构建报文 - 添加防粘包协议头
        // packet.append((char*)&packetLength, 4);                    // 报文长度

        // 协议头：文件传输ID(4) + 总包数(2) + 当前包序号(2)
        packet.append((char*)&fileTransferId, 4);                  // 文件传输唯一标识
        packet.append((char*)&totalCount, 2);                      // 总包数
        packet.append((char*)&order, 2);                           // 当前包序号

        // packet.append(static_cast<char>(0x66));                    // 指令代码

        quint16 filePathLen = filePathData.length();
        packet.append((char*)&filePathLen, 2);                     // 文件路径长度
        packet.append(filePathData);                               // 文件路径

        // 添加当前包数据长度
        packet.append((char*)&currentDataLen, 4);                  // 当前包数据长度
        packet.append(fileData);                                   // 文件数据
        quint32 nPacketlen = packet.size()+2;
        packet.insert(0, (char *)&nPacketlen, 4);
        // 计算校验码（不包括报文长度字段）
        quint16 nCheckCode = CalcCheckCode16(packet.data(), packet.length());
        packet.append((char*)&nCheckCode, 2);                      // 校验码
        LOG("nCheckCode on server:"+QString::number(nCheckCode));


        // array.push_back(packet);
        CTrainTcpServer::instance()->SendDatasToClient(descriptor, packet);
        bytesSent += currentDataLen;
        bytesRemaining -= currentDataLen;
        order++;
        count++;
        // 根据包大小动态调整延时，小包延时短，大包延时长
        int delay = qMax(10, qMin(100, packet.size() / 1000));
        QThread::msleep(delay);
    }

    file.close();

    LOG(QString("File %1 packed into %2 packets, transfer ID: %3, max packet size: %4")
            .arg(filePath).arg(count).arg(fileTransferId).arg(maxPacketSize),
        LogLevel::LOG_INFO);

    return count;
}
CarriagePictureProcessor::CarriagePictureProcessor(int descriptor, const QString &filePath, qint64 maxPacketSize, QObject *parent)
    :QThread(parent),m_descriptor(descriptor),m_filePath(filePath),m_maxPacketSize(maxPacketSize)
    {

}
// 修改接收端解析函数，添加防粘包处理
bool CDataProtocol::AnalyzeCarriagePictureReq(int descriptor, const char *pData, quint32 nLen)
{
    if (nullptr == pData || nLen <= 0)
    {
        return false;
    }

    int nIndex = 0;

    // 文件路径数据长度
    quint16 nFileDataLen;
    memcpy(&nFileDataLen, pData + nIndex, 2);
    nIndex += 2;

    if (nLen < nFileDataLen + 2)
    {
        return false;
    }

    QString strFileKeyValue = QString::fromLocal8Bit(pData + nIndex + 1, nFileDataLen - 2);
    nIndex += nFileDataLen;
    QMap<QString, QString> mapFileKeyValue;
    AnalyzeStringtoKeyValue(strFileKeyValue, mapFileKeyValue);

    QString strFilePath = mapFileKeyValue["filepath"];
    if (strFilePath.size() <= 0)
    {
        return false;
    }

    // 车辆信息
    QString strWagonNumber = mapFileKeyValue["wagonNumber"];
    if (strWagonNumber.size() <= 0)
    {
        return false;
    }

    // 获取socket的实际最大传输大小
    qint64 maxWriteSize = DEFAULT_PACKET_SIZE;
    // if (socket) {
    //     // QTcpSocket没有直接提供最大写入大小的方法，但我们可以使用一些经验值
    //     // 通常TCP最大段大小(MSS)是1460字节，但我们可以设置更大的值让系统自动分段
    //     maxWriteSize = 64 * 1024; // 64KB，这是一个合理的值
    // }


    // 为每个文件请求创建新线程处理
    CarriagePictureProcessor *processor = new CarriagePictureProcessor(descriptor, strFilePath, maxWriteSize);

    // 连接线程结束信号，用于自动清理内存
    // connect(processor, &CarriagePictureProcessor::finished, processor, &CarriagePictureProcessor::deleteLater);

    // 启动线程
    processor->start();

    LOG(QString("Started carriage picture processing thread for descriptor %1, file: %2, max packet size: %3")
            .arg(descriptor).arg(strFilePath).arg(maxWriteSize), LogLevel::LOG_INFO);

    return true;
}




// 将TimeStruct转换为QDateTime
QDateTime timeStructToQDateTime(const TimeStruct &ts) {
    // 1. 创建QDate对象
    QDate date;
    if (ts.year >= 1970 && ts.year <= 2200) {
        date.setDate(ts.year, ts.month, ts.day);
    } else {
        qWarning() << QString("无效年份: %1").arg(ts.year);
        return QDateTime(); // 返回无效日期时间
    }

    // 验证日期有效性
    if (!date.isValid()) {
        qWarning() << QString("无效日期: %1-%2-%3")
                          .arg(ts.year)
                          .arg(ts.month)
                          .arg(ts.day);
        return QDateTime();
    }

    // 2. 创建QTime对象
    QTime time(ts.hour, ts.minute, ts.second);

    // 验证时间有效性
    if (!time.isValid()) {
        qWarning() << QString("无效时间: %1:%2:%3")
                          .arg(ts.hour)
                          .arg(ts.minute)
                          .arg(ts.second);
        return QDateTime();
    }

    // 3. 组合成QDateTime (假设为本地时间)
    QDateTime dateTime(date, time, Qt::LocalTime);

    // 4. 验证星期信息 (可选)
    if (date.dayOfWeek() != ts.weekday) {
        qWarning() << QString("星期不匹配: 计算%1 vs 报文%2")
                          .arg(date.dayOfWeek())
                          .arg(ts.weekday);
    }

    return dateTime;
}

// 将QDateTime转换为TimeStruct
TimeStruct qDateTimeToTimeStruct(const QDateTime &dt) {
    if (!dt.isValid()) {
        qWarning() << "无法转换无效的QDateTime";
        return TimeStruct{0};
    }

    QDate date = dt.date();
    QTime time = dt.time();

    return TimeStruct {
        .year = static_cast<quint16>(date.year()),
        .month = static_cast<quint8>(date.month()),
        .day = static_cast<quint8>(date.day()),
        .weekday = static_cast<quint8>(date.dayOfWeek()), // Qt:1=周一,7=周日
        .hour = static_cast<quint8>(time.hour()),
        .minute = static_cast<quint8>(time.minute()),
        .second = static_cast<quint8>(time.second())
    };
}
CDataProtocol::CDataProtocol()
{
}
TimeStruct CDataProtocol::ExtractDateTime(char *pFrame){
    QByteArray array(pFrame);
    QDataStream stream(array);
    stream.setByteOrder(QDataStream::LittleEndian); // 小端序
    TimeStruct t;
    // 读取时间字段 (8字节)
    stream >> t.year
        >> t.month
        >> t.day
        >> t.weekday
        >> t.hour
        >> t.minute
        >> t.second;
    return t;
}
QString CDataProtocol::MapToString(QMap<QString,QString> maps){
    QStringList list;
    for (auto it = maps.cbegin(); it != maps.cend(); ++it)
        list.append(it.key() + '=' + it.value());
    return '[' + list.join(',') + ']';
}

bool CDataProtocol::ResponseTimeVerify(int descriptor, char *pFrame, quint32 nLen){
    // 1. 基本长度检查 (最小16字节)
    if (nLen < 16) {
        qWarning("报文长度不足: %u bytes", nLen);
        return false;
    }

    // 2. 转换为QByteArray以便处理
    QByteArray receivedData = QByteArray::fromRawData(pFrame, static_cast<int>(nLen));
    QDataStream stream(receivedData);
    stream.setByteOrder(QDataStream::LittleEndian); // 小端序

    // 3. 读取并验证报文长度字段 (DINT类型)
    qint32 lengthField;
    stream >> lengthField;

    // 验证长度字段值 (应为12)
    if (lengthField != 12) {
        qWarning("无效长度字段: %d (应为12)", lengthField);
        return false;
    }

    // 4. 读取并验证指令代码 (应为0x01)
    quint16 commandCode;
    stream >> commandCode;
    if (commandCode != 0x01) {
        qWarning("非对时指令: 0x%04X (应为0x0001)", commandCode);
        return false;
    }

    // 5. 计算并验证校验和
    quint16 receivedChecksum = 0;
    quint16 calculatedChecksum = 0;

    // 移动到最后2字节读取校验码
    stream.device()->seek(nLen - 2);
    stream >> receivedChecksum;

    // 计算校验和 (从第5字节开始到倒数第3字节)
    for (int i = 4; i < static_cast<int>(nLen) - 2; i++) {
        calculatedChecksum += static_cast<quint8>(receivedData.at(i));
    }

    if (calculatedChecksum != receivedChecksum) {
        qWarning("校验失败: 接收0x%04X vs 计算0x%04X", receivedChecksum, calculatedChecksum);
        return false;
    }

    // 6. 构造响应报文
    QByteArray responseArray;
    quint32 packResult = PackTimeSynCom(responseArray); // 调用已有封装函数

    if (packResult == 0 || responseArray.isEmpty()) {
        qCritical("PackTimeSynCom失败: 返回%u, 长度%d", packResult, responseArray.size());
        return false;
    }

    // 7. 发送响应 (根据descriptor类型处理)
    if (descriptor >= 0) { // 假设是文件描述符
        CTrainTcpServer::instance()->SendDatasToClient(descriptor, responseArray);
    } else if (descriptor == -1) { // 特殊值表示返回给上层处理
        // 需要由上层提取responseArray发送
        qDebug("响应报文已构造, 需由上层发送");
    } else { // 假设是QTcpSocket指针
        QTcpSocket* socket = reinterpret_cast<QTcpSocket*>(descriptor);
        if (socket && socket->isWritable()) {
            qint64 bytesWritten = socket->write(responseArray);
            if (bytesWritten != responseArray.size()) {
                QString errorMsg = QString("套接字发送失败: 预期%1字节, 实际%2字节").arg(responseArray.size()).arg(bytesWritten);
                LOG(errorMsg,LogLevel::LOG_ERROR);
                return false;
            }
            socket->flush(); // 确保立即发送
        } else {
            qWarning("无效的套接字描述符或不可写");
            return false;
        }
    }

    LOG("成功响应对时指令");
    return true;
}
bool CDataProtocol::AnalyzeCommonReq(int descriptor, const char *pData, quint32 nLen,RequestProcess process){
    if (nullptr == pData || nLen <= 0)
    {
        return false;
    }

    int nIndex = 0;

    //数据长度
    quint16 nDataLen;

    memcpy(&nDataLen, pData, 2);
    nIndex += 2;

    if (nLen < nDataLen + 2)
    {
        return false;
    }

    //QByteArray arraykeyValue = QByteArray(pData+nIndex+1, nDataLen - 2);
    //QString strKeyValue = QString(arraykeyValue);
    QString strKeyValue = QString::fromLocal8Bit(pData+nIndex+1, nDataLen - 2);
    QMap<QString, QString> mapKeyValue;
    AnalyzeStringtoKeyValue(strKeyValue, mapKeyValue);
    process(mapKeyValue,descriptor);
    return true;

}
bool CDataProtocol::AnalyzeRegisterReq(int descriptor, const char *pData, quint32 nLen)
{
    if (nullptr == pData || nLen <= 0)
    {
        return false;
    }

    int nIndex = 0;

    //数据长度
    quint16 nDataLen;

    memcpy(&nDataLen, pData, 2);
    nIndex += 2;

    if (nLen < nDataLen + 2)
    {
        return false;
    }

    //QByteArray arraykeyValue = QByteArray(pData+nIndex+1, nDataLen - 2);
    //QString strKeyValue = QString(arraykeyValue);
    QString strKeyValue = QString::fromLocal8Bit(pData+nIndex+1, nDataLen - 2);
    QMap<QString, QString> mapKeyValue;
    AnalyzeStringtoKeyValue(strKeyValue, mapKeyValue);
    UserInfo user;
    user.username = mapKeyValue["username"];
    user.name = mapKeyValue["name"];
    user.workNumber = mapKeyValue["worknumber"];
    user.password = mapKeyValue["password"];
    user.cellphone= mapKeyValue["cellphone"];

    int nErrorCode = 0;
    if (user.username.size() <= 0)
    {
        nErrorCode=2;
    }else if(user.password .size() <= 0){
        nErrorCode=3;
    }else{
        if(!CDatabaseManager::Instance()->addUser(user)){
            nErrorCode=1;
        }else{
            Logger::instance()->Log("成功注册新用户：" + user.username + "+" + user.name + "+" + user.workNumber + "+" + user.password + "+" + user.cellphone);
        }
    }
    //填写并发送响应报文
    QByteArray resArray;
    PackRegisterRes(pData, nLen, nErrorCode, resArray);
    CTrainTcpServer::instance()->SendDatasToClient(descriptor, resArray);
    return true;
}

bool CDataProtocol::AnalyzeLoginReq(int descriptor, const char *pData, quint32 nLen)
{
    if (nullptr == pData || nLen <= 0)
    {
        return false;
    }

    int nIndex = 0;

    //数据长度
    quint16 nDataLen;
    memcpy(&nDataLen, pData, 2);
    nIndex += 2;

    if (nLen < nDataLen + 2)
    {
        return false;
    }

    //QByteArray arraykeyValue = QByteArray(pData+nIndex+1, nDataLen - 2);
    //QString strKeyValue = QString(arraykeyValue);
    QString strKeyValue = QString::fromLocal8Bit(pData+nIndex+1, nDataLen - 2);
    QMap<QString, QString> mapKeyValue;
    AnalyzeStringtoKeyValue(strKeyValue, mapKeyValue);

    QString strUserName = mapKeyValue["username"];
    QString strPassword = mapKeyValue["password"];
    int nErrorCode =0;
    //填写并发送响应报文
    QByteArray resArray;
    UserInfo *pUser=nullptr,user;
    if(isUserLogined(strUserName)){
        nErrorCode=2;
    }else{
        //查询数据库，判断注册结果
        nErrorCode = CDatabaseManager::Instance()->userLogin(strUserName,strPassword);
        Logger::instance()->Log("接收到用户登录请求：" + strUserName + "+" + strPassword);
        if(nErrorCode==0){
            //登录成功，获取用户信息
            user=CDatabaseManager::Instance()->getUserInfo(strUserName);
            pUser=&user;
            //保存登录信息
            loginedUsers[descriptor]=strUserName;
        }
    }
    PackLoginRes(/*pData, nLen, */nErrorCode, resArray,pUser);
    CTrainTcpServer::instance()->SendDatasToClient(descriptor, resArray);

    return true;
}

bool CDataProtocol::AnalyzeLogoutReq(int descriptor, const char *pData, quint32 nLen)
{
    if (nullptr == pData || nLen <= 0)
    {
        return false;
    }

    int nIndex = 0;

    //数据长度
    quint16 nDataLen;
    memcpy(&nDataLen, pData, 2);
    nIndex += 2;

    if (nLen < nDataLen + 2)
    {
        return false;
    }

    //QByteArray arraykeyValue = QByteArray(pData+nIndex+1, nDataLen - 2);
    //QString strKeyValue = QString(arraykeyValue);
    QString strKeyValue = QString::fromLocal8Bit(pData+nIndex+1, nDataLen - 2);
    QMap<QString, QString> mapKeyValue;
    AnalyzeStringtoKeyValue(strKeyValue, mapKeyValue);

    QString strUserName = mapKeyValue["username"];
    if (strUserName.size() <= 0)
    {
        return false;
    }

    Logger::instance()->Log("接收到用户注销请求：" + strUserName);

    //查询数据库，判断注册结果
    int nErrorCode = 0;
    if(!isUserLogined(strUserName)){
        nErrorCode=2;
    }else{
        removeUserByUsername(strUserName);
    }
    //填写并发送响应报文
    QByteArray resArray;
    PackLogoutRes(pData, nLen, nErrorCode, resArray);
    CTrainTcpServer::instance()->SendDatasToClient(descriptor, resArray);

    return true;
}
QByteArray packageCarriageResponse(const CarriageInformation& info) {
    QByteArray responseData;
    QDataStream stream(&responseData, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian); // 协议通常使用大端序

    // 临时保存数据，用于计算校验码
    QByteArray tempData;
    QDataStream tempStream(&tempData, QIODevice::WriteOnly);
    tempStream.setByteOrder(QDataStream::BigEndian);

    // 信息/指令代码 (1字节)
    quint8 commandCode = 0x62;
    tempStream << commandCode;

    // 构建Record内容
    QString recordStr;
    recordStr += "trainnumber=" + info.strTrainNumber + ",";
    recordStr += "reachdatetime=" + QString::number(info.nReachDatetime) + ",";
    recordStr += "direction=,"; // 方向信息可能不在结构体中，留空
    recordStr += "numberofaxles=,"; // 总轴数可能不在结构体中，留空
    recordStr += "numberofcarriage=1,"; // 只有一辆车
    recordStr += "filepath=";

    // 添加所有图像路径
    for (const auto& pair : info.imagePathMap) {
        recordStr += pair.second + ";";
    }
    if (!info.imagePathMap.empty()) {
        recordStr.chop(1); // 移除最后一个分号
    }
    recordStr += ",";

    recordStr += "detection=,"; // 检测点信息可能不在结构体中，留空
    recordStr += "inspectiondatetime=" + QString::number(info.nReachDatetime); // 使用到达时间作为检车时间

    // 将Record字符串转换为UTF-8字节数组
    QByteArray recordBytes = recordStr.toUtf8();

    // 过车数量 (2字节) - 只有一辆车
    quint16 carriageCount = 1;
    tempStream << carriageCount;

    // Record数据长度 (2字节)
    quint16 recordLength = static_cast<quint16>(recordBytes.size());
    tempStream << recordLength;

    // Record内容
    tempStream.writeRawData(recordBytes.constData(), recordBytes.size());

    // 计算报文长度 (4字节) - 不包括报文长度本身
    quint32 messageLength = tempData.size();
    stream << messageLength;

    // 写入临时数据
    stream.writeRawData(tempData.constData(), tempData.size());

    // 计算校验码 (1字节) - 前面所有字节的累加和
    quint8 checksum = 0;
    for (int i = 0; i < responseData.size(); i++) {
        checksum += static_cast<quint8>(responseData.at(i));
    }
    stream << checksum;

    return responseData;
}
// 将 TrainInformation 打包成通信字节流
QByteArray packTrainInformation(const std::vector<TrainInformation>& trainInfos) {
    QByteArray packet;
    QDataStream stream(&packet, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian); // 设置字节序为大端

    // 1. 先预留4字节给报文长度
    quint32 packetLength = 0;
    stream << packetLength;

    // 2. 信息/指令代码 (62H)
    quint8 commandCode = 0x62;
    stream << commandCode;

    // 3. 过车信息
    // 3.1 过车数量 (2字节)
    quint16 trainCount = static_cast<quint16>(trainInfos.size());
    stream << trainCount;

    // 3.2 每条记录
    for (const auto& info : trainInfos) {
        // 构建键值对字符串
        QString recordStr;
        recordStr += "trainnumber=" + info.strTrainNumber + ",";
        recordStr += "reachdatetime=" + QString::number(info.nReachDatetime) + ",";
        recordStr += "direction=" + info.strDirection + ",";
        recordStr += "numberofaxles=" + QString::number(info.nNumberOfAxles) + ",";
        recordStr += "numberofcarriage=" + QString::number(info.nNumberOfCarriage) + ",";
        recordStr += "filepath=" + info.strFilePath + ",";
        recordStr += "detection=" + info.strDetection + ",";
        recordStr += "inspectiondatetime=" + QString::number(info.nInspectionDatetime);

        // 转换为UTF-8字节数组
        QByteArray recordData = recordStr.toUtf8();

        // 数据长度 (2字节)
        quint16 dataLength = static_cast<quint16>(recordData.size());
        stream << dataLength;

        // 写入记录数据
        stream.writeRawData(recordData.constData(), recordData.size());
    }

    // 4. 计算校验码 (前面所有字节的累加和)
    quint8 checksum = 0;
    // 跳过前4字节的报文长度
    for (int i = 4; i < packet.size(); i++) {
        checksum += static_cast<quint8>(packet.at(i));
    }
    stream << checksum;

    // 5. 更新报文长度 (不包括报文长度字段本身)
    packetLength = packet.size() - 4;
    QDataStream lengthStream(&packet, QIODevice::WriteOnly);
    lengthStream.setByteOrder(QDataStream::BigEndian);
    lengthStream << packetLength;

    return packet;
}
bool CDataProtocol::AnalyzeTrainSearchReq(int descriptor, const char *pData, quint32 nLen)
{
    if (nullptr == pData || nLen <= 0)
    {
        return false;
    }

    int nIndex = 0;

    //数据长度
    quint16 nDataLen;
    memcpy(&nDataLen, pData, 2);
    nIndex += 2;

    if (nLen < nDataLen + 2)
    {
        return false;
    }

    //QByteArray arraykeyValue = QByteArray(pData+nIndex+1, nDataLen - 2);
    //QString strKeyValue = QString(arraykeyValue);
    QString strKeyValue = QString::fromLocal8Bit(pData+nIndex+1, nDataLen - 2);
    QMap<QString, QString> mapKeyValue;
    AnalyzeStringtoKeyValue(strKeyValue, mapKeyValue);

    QString strTrainNumber = mapKeyValue["trainnumber"];
    quint64 nReachDatetime = mapKeyValue["reachdatetime "].toULong();
    //读取车箱数据
    TrainInformation info;
    info.Init();
    bool hasRecord=CDatabaseManager::Instance()->getTrainInfoByNumberAndTime(strTrainNumber,nReachDatetime,info);
    Logger::instance()->Log("接收到用户货车查询请求：");
    auto responseBytes=packTrainInformation({info});

    CTrainTcpServer::instance()->SendDatasToClient(descriptor, responseBytes);
    return true;
}
QString generateTrainListData(const QList<TrainInformation> &trainList)
{
    // 将TrainInformation列表转换为JSON格式
    QJsonArray jsonArray;

    for (const TrainInformation &info : trainList) {
        QJsonObject jsonObj;
        jsonObj["trainNumber"] = info.strTrainNumber;
        jsonObj["reachDatetime"] = QString::number(info.nReachDatetime);
        jsonObj["direction"] = info.strDirection;
        jsonObj["axleCount"] = info.nNumberOfAxles;
        jsonObj["carriageCount"] = info.nNumberOfCarriage;
        jsonObj["filePath"] = info.strFilePath;
        jsonObj["detection"] = info.strDetection;
        jsonObj["inspectionDatetime"] = QString::number(info.nInspectionDatetime);
        jsonObj["inspectionState"] = info.strInspectionState;

        jsonArray.append(jsonObj);
    }

    QJsonDocument doc(jsonArray);
    return doc.toJson(QJsonDocument::Compact);
}
QByteArray CDataProtocol::buildTrainListResponsePacket(const QString &trainListData)
{
    QByteArray packet;

    // memcpy(&nDataLen, pData, 2);

    // 更新数据长度
    QByteArray rawData = trainListData.toLocal8Bit();   // 或 toUtf8()
    quint16 newDataLen = static_cast<quint16>(rawData.size());   // 真正的字节长度
    // quint16 newDataLen = trainListData.length();

    // 报文长度 = 指令代码(1) + 数据长度(2) + 数据(m) + 校验码(2)
    quint32 nPacketLen = 1 + 2 + newDataLen  + 2;
    char *pPacket = new char[nPacketLen + 4]; // +4 用于存储报文长度本身

    int nIndex = 0;
    // 报文长度
    memcpy(pPacket + nIndex, &nPacketLen, 4);
    nIndex += 4;

    // 信息/指令代码
    pPacket[nIndex++] = 0x68;

    // 数据长度
    memcpy(pPacket + nIndex, &newDataLen, 2);
    nIndex += 2;

    // 数据
    if (newDataLen > 0) {
        memcpy(pPacket + nIndex,rawData, newDataLen);
        nIndex += newDataLen;
    }

    quint16 nCheckCode = CalcCheckCode16(pPacket, nIndex);
    memcpy(pPacket + nIndex, &nCheckCode, 2);
    nIndex += 2;

    packet = QByteArray(pPacket, nIndex);
    delete[] pPacket;

    return packet; // 返回整个报文
}
bool CDataProtocol::AnalyzeTrainListReq(int descriptor, const char *pData, quint32 nLen)
{
    if (nullptr == pData || nLen <= 0)
    {
        return false;
    }

    int nIndex = 0;

    // 开始时间\结束时间
    quint64 nStartTime,nEndTime;
    memcpy(&nStartTime, pData, 8);
    memcpy(&nEndTime, pData+8, 8);
    //查询火车列表
    auto trainList=CDatabaseManager::Instance()->getTrainInfoByReachTimeRange(nStartTime,nEndTime);
    // 生成火车列表数据
    QString trainListData = generateTrainListData(trainList);
    auto responseBytes=buildTrainListResponsePacket(trainListData);
    //发送响应
    CTrainTcpServer::instance()->SendDatasToClient(descriptor, responseBytes);
    return true;
}
QByteArray CDataProtocol::buildCarriageListResponsePacket(const QString &trainListData,quint8 cmd)
{
    QByteArray packet;

    // memcpy(&nDataLen, pData, 2);

    // 更新数据长度
    QByteArray rawData = trainListData.toLocal8Bit();   // 或 toUtf8()
    quint16 newDataLen = static_cast<quint16>(rawData.size());   // 真正的字节长度
    // quint16 newDataLen = trainListData.length();

    // 报文长度 = 指令代码(1) + 数据长度(2) + 数据(m) + 校验码(2)
    quint32 nPacketLen = 1 + 2 + newDataLen  + 2;
    char *pPacket = new char[nPacketLen + 4]; // +4 用于存储报文长度本身

    int nIndex = 0;
    // 报文长度
    memcpy(pPacket + nIndex, &nPacketLen, 4);
    nIndex += 4;

    // 信息/指令代码
    pPacket[nIndex++] =cmd; //0x6A;

    // 数据长度
    memcpy(pPacket + nIndex, &newDataLen, 2);
    nIndex += 2;

    // 数据
    if (newDataLen > 0) {
        memcpy(pPacket + nIndex,rawData, newDataLen);
        nIndex += newDataLen;
    }

    quint16 nCheckCode = CalcCheckCode16(pPacket, nIndex);
    memcpy(pPacket + nIndex, &nCheckCode, 2);
    nIndex += 2;

    packet = QByteArray(pPacket, nIndex);
    delete[] pPacket;

    return packet; // 返回整个报文
}
QString generateCarriageListData(const std::vector<CarriageInformation> &trainList)
{
    // 将TrainInformation列表转换为JSON格式
    QJsonArray jsonArray;

    for (const auto &info : trainList) {
        QJsonObject jsonObj;
        jsonObj["strTrainNumber"] = info.strTrainNumber;
        jsonObj["nReachDatetime"] = QString::number(info.nReachDatetime);
        jsonObj["nOrder"] = info.nOrder;
        jsonObj["strWagonNumber"] = info.strWagonNumber;
        jsonObj["strCarriageModel"] = info.strCarriageModel;
        jsonObj["fConverredLength"] = info.fConverredLength;
        jsonObj["strCarriageModelCode"] = info.strCarriageModelCode;
        jsonObj["strFactoryAndDatetime"] =info.strFactoryAndDatetime;
        jsonObj["nFrontWheelTime"] = QString::number(info.nFrontWheelTime);
        jsonObj["frontWheelSpeed"] = QString::number(info.frontWheelSpeed);
        jsonObj["nRearWheelTime"] = QString::number(info.nRearWheelTime);
        jsonObj["rearWheelSpeed"] = QString::number(info.rearWheelSpeed);
        jsonObj["leftImagePath"] =info.imagePathMap.at("left");
        jsonObj["rightImagePath"] =info.imagePathMap.at("right");
        jsonObj["topImagePath"] =info.imagePathMap.at("top");
        jsonObj["inspectorState"] =info.inspectorState?"未检视":"已检视";
        jsonObj["strFactoryAndDatetime"] =info.strFactoryAndDatetime;
        jsonObj["leftInspectorResult"] =info.inspectorResult.at("left")?"通过":"不通过";
        jsonObj["rightInspectorResult"] =info.inspectorResult.at("right")?"通过":"不通过";
        jsonObj["topInspectorResult"] =info.inspectorResult.at("top")?"通过":"不通过";

        jsonArray.append(jsonObj);
    }
    QJsonDocument doc(jsonArray);
    return doc.toJson(QJsonDocument::Compact);
}
bool CDataProtocol::AnalyzeCarriageUnInspectorListReq(int descriptor, const char *pData, quint32 nLen){
    if (nullptr == pData || nLen <= 0)
    {
        return false;
    }

    int nIndex = 0;
    //车次数据长度
    quint16 nTrainNumLen;
    memcpy(&nTrainNumLen, pData, 2);
    nIndex+=2;
    QString strKeyValue = QString::fromLocal8Bit(pData+nIndex+1, nTrainNumLen-2);//去掉方括号

    QMap<QString, QString> mapKeyValue;
    AnalyzeStringtoKeyValue(strKeyValue, mapKeyValue);
    QString strTrainNumber=mapKeyValue.value("trainnumber");
    // 过车时间
    quint64 nStartTime=mapKeyValue.value("trainreachtime").toLongLong();
    //查询车箱列表
    auto carriageList=CDatabaseManager::Instance()->queryCarriageList(strTrainNumber,nStartTime,0);
    // 生成火车列表数据
    QString trainListData = generateCarriageListData(carriageList);
    auto responseBytes=buildCarriageListResponsePacket(trainListData);
    //发送响应
    CTrainTcpServer::instance()->SendDatasToClient(descriptor, responseBytes);
    return true;
}
bool CDataProtocol::AnalyzeCarriageListReq(int descriptor, const char *pData, quint32 nLen){
    if (nullptr == pData || nLen <= 0)
    {
        return false;
    }

    int nIndex = 0;
    //车次数据长度
    quint16 nTrainNumLen;
    memcpy(&nTrainNumLen, pData, 2);
    nIndex+=2;
    QString strKeyValue = QString::fromLocal8Bit(pData+nIndex+1, nTrainNumLen-2);//去掉方括号

    QMap<QString, QString> mapKeyValue;
    AnalyzeStringtoKeyValue(strKeyValue, mapKeyValue);
    QString strTrainNumber=mapKeyValue.value("trainnumber");
    // 过车时间
    quint64 nStartTime=mapKeyValue.value("trainreachtime").toLongLong();
    //查询所有车箱列表
    auto carriageList=CDatabaseManager::Instance()->queryCarriageList(strTrainNumber,nStartTime,-1);
    // 生成火车列表数据
    QString trainListData = generateCarriageListData(carriageList);
    auto responseBytes=buildCarriageListResponsePacket(trainListData,0x70);
    //发送响应
    CTrainTcpServer::instance()->SendDatasToClient(descriptor, responseBytes);
    return true;
}
bool CDataProtocol::AnalyzeCarriageSearchReq(int descriptor, const char *pData, quint32 nLen)
{
    if (nullptr == pData || nLen <= 0)
    {
        return false;
    }

    int nIndex = 0;

    //数据长度
    quint16 nDataLen;
    memcpy(&nDataLen, pData, 2);
    nIndex += 2;

    if (nLen < nDataLen + 2)
    {
        return false;
    }

    //QByteArray arraykeyValue = QByteArray(pData+nIndex+1, nDataLen - 2);
    //QString strKeyValue = QString(arraykeyValue);
    QString strKeyValue = QString::fromLocal8Bit(pData+nIndex+1, nDataLen - 2);
    QMap<QString, QString> mapKeyValue;
    AnalyzeStringtoKeyValue(strKeyValue, mapKeyValue);

    // QString strTrainNumber = mapKeyValue["trainnumber"];
    // QString strWagonNumber = mapKeyValue["wagonnumber"];
    // quint64 nStartDatetime = mapKeyValue["startdatetime"].toULong();
    // quint64 nEndDatetime = mapKeyValue["enddatetime"].toULong();
    //读取车箱数据
    CarriageInformation carriage=CDatabaseManager::Instance()->queryCarriageInformation(mapKeyValue);
    Logger::instance()->Log("接收到用户货车查询请求：");
    auto responseBytes=packageCarriageResponse(carriage);

    CTrainTcpServer::instance()->SendDatasToClient(descriptor, responseBytes);
    return true;
}

bool CDataProtocol::AnalyzeTrainNumberSearchReq(int descriptor, const char *pData, quint32 nLen)
{
    if (nullptr == pData || nLen <= 0)
    {
        return false;
    }

    int nIndex = 0;

    //数据长度
    quint16 nDataLen;
    memcpy(&nDataLen, pData, 2);
    nIndex += 2;

    if (nLen < nDataLen + 2)
    {
        return false;
    }

    //QByteArray arraykeyValue = QByteArray(pData+nIndex+1, nDataLen - 2);
    //QString strKeyValue = QString(arraykeyValue);
    QString strKeyValue = QString::fromLocal8Bit(pData+nIndex+1, nDataLen - 2);
    QMap<QString, QString> mapKeyValue;
    AnalyzeStringtoKeyValue(strKeyValue, mapKeyValue);

    QString strTrainnumber = mapKeyValue["trainnumber"];
    quint32 nReachDatetime = mapKeyValue["reachdatetime"].toULong();

    Logger::instance()->Log("接收到用户车次信息查询请求：");

    //查询数据库，并填写响应报文

    return true;
}

// bool CDataProtocol::AnalyzeCarriagePictureReq(int descirptor, const char *pData, quint32 nLen)
// {
//     if (nullptr == pData || nLen <= 0)
//     {
//         return false;
//     }

//     int nIndex = 0;

//     //文件路径数据长度
//     quint16 nFileDataLen;
//     memcpy(&nFileDataLen, pData+nIndex, 2);
//     nIndex += 2;

//     if (nLen < nFileDataLen + 2)
//     {
//         return false;
//     }

//     //QByteArray arrayFileKeyValue = QByteArray(pData+nIndex+1, nFileDataLen - 2);
//     //nIndex += nFileDataLen;
//     //QString strFileKeyValue = QString(arrayFileKeyValue);
//     QString strFileKeyValue = QString::fromLocal8Bit(pData+nIndex+1, nFileDataLen - 2);
//     nIndex += nFileDataLen;
//     QMap<QString, QString> mapFileKeyValue;
//     AnalyzeStringtoKeyValue(strFileKeyValue, mapFileKeyValue);

//     QString strFilePath = mapFileKeyValue["filepath"];
//     if (strFilePath.size() <= 0)
//     {
//         return false;
//     }

//     //车辆信息
//     QString strWagonNumber=mapFileKeyValue["wagonNumber"];
//     if (strWagonNumber.size() <= 0)
//     {
//         return false;
//     }
//     //填写并发送响应报文
//     std::vector<QByteArray> arrayRes;
//     PackCarriagePictureRes(strFilePath, arrayRes);
//     for(auto & array:arrayRes){
//         CTrainTcpServer::instance()->SendDatasToClient(descirptor, array);
//         QThread::msleep(50);
//     }
//     return true;
// }
//来车信息校验函数
bool CDataProtocol::validateTrainArrivalData(const char *pData, quint32 nLen) {
    // 1. 检查最小长度
    if (nLen < 18) {
        qWarning() << QString("报文长度不足: %1字节 (需≥18字节)").arg(nLen);
        return false;
    }

    // 2. 读取并验证报文长度字段 (小端序)
    qint32 lengthField = 0;
    for (int i = 0; i < 4; i++) {
        lengthField |= (static_cast<quint8>(pData[i]) << (i * 8));
    }

    if (lengthField != 14) {
        qWarning() << QString("无效长度字段: %1 (应为14)").arg(lengthField);
        return false;
    }

    // 3. 读取并验证指令代码 (小端序)
    quint16 commandCode = static_cast<quint8>(pData[4]) |
                          (static_cast<quint8>(pData[5]) << 8);

    if (commandCode != 0x04) {
        qWarning() << QString("非车来信息指令: 0x%1 (应为0x0004)")
                          .arg(commandCode, 4, 16, QLatin1Char('0'));
        return false;
    }

    // 4. 读取并验证列车方向 (小端序)
    quint16 direction = static_cast<quint8>(pData[6]) |
                        (static_cast<quint8>(pData[7]) << 8);

    if (direction != 0 && direction != 1) {
        qWarning() << QString("无效列车方向: %1 (应为0或1)").arg(direction);
        return false;
    }

    // 5. 读取时间字段
    TimeStruct timeInfo;
    timeInfo.year = static_cast<quint8>(pData[8]) |
                    (static_cast<quint8>(pData[9]) << 8);
    timeInfo.month = static_cast<quint8>(pData[10]);
    timeInfo.day = static_cast<quint8>(pData[11]);
    timeInfo.weekday = static_cast<quint8>(pData[12]);
    timeInfo.hour = static_cast<quint8>(pData[13]);
    timeInfo.minute = static_cast<quint8>(pData[14]);
    timeInfo.second = static_cast<quint8>(pData[15]);

    // 6. 验证时间有效性
    // 年份范围检查
    if (timeInfo.year < 1970 || timeInfo.year > 2200) {
        qWarning() << QString("无效年份: %1 (范围1970-2200)").arg(timeInfo.year);
        return false;
    }

    // 月份检查
    if (timeInfo.month < 1 || timeInfo.month > 12) {
        qWarning() << QString("无效月份: %1 (范围1-12)").arg(timeInfo.month);
        return false;
    }

    // 日期检查
    int maxDay = 31;
    if (timeInfo.month == 4 || timeInfo.month == 6 ||
        timeInfo.month == 9 || timeInfo.month == 11) {
        maxDay = 30;
    } else if (timeInfo.month == 2) {
        // 闰年判断
        bool isLeap = (timeInfo.year % 4 == 0 && timeInfo.year % 100 != 0) ||
                      (timeInfo.year % 400 == 0);
        maxDay = isLeap ? 29 : 28;
    }

    if (timeInfo.day < 1 || timeInfo.day > maxDay) {
        qWarning() << QString("无效日期: %1年%2月没有%3日")
                          .arg(timeInfo.year)
                          .arg(timeInfo.month)
                          .arg(timeInfo.day);
        return false;
    }

    // 星期检查
    if (timeInfo.weekday < 1 || timeInfo.weekday > 7) {
        qWarning() << QString("无效星期: %1 (范围1-7)").arg(timeInfo.weekday);
        return false;
    }

    // 时间检查
    if (timeInfo.hour > 23) {
        qWarning() << QString("无效小时: %1 (范围0-23)").arg(timeInfo.hour);
        return false;
    }
    if (timeInfo.minute > 59) {
        qWarning() << QString("无效分钟: %1 (范围0-59)").arg(timeInfo.minute);
        return false;
    }
    if (timeInfo.second > 59) {
        qWarning() << QString("无效秒数: %1 (范围0-59)").arg(timeInfo.second);
        return false;
    }

    // 7. 计算并验证校验和 (小端序)
    quint16 receivedChecksum = static_cast<quint8>(pData[16]) |
                               (static_cast<quint8>(pData[17]) << 8);

    quint16 calcChecksum = 0;
    // 从指令代码开始到时间结束 (字节4到15)
    for (int i = 4; i < 16; i++) {
        calcChecksum += static_cast<quint8>(pData[i]);
    }

    if (calcChecksum != receivedChecksum) {
        qWarning() << QString("校验失败: 接收0x%1 vs 计算0x%2")
                          .arg(receivedChecksum, 4, 16, QLatin1Char('0'))
                          .arg(calcChecksum, 4, 16, QLatin1Char('0'));
        return false;
    }

    // 所有检查通过
    QString directionStr = (direction == 1) ? "正向" : "反向";
    qDebug() << QString("车来信息验证通过: %1方向 时间:%2-%3-%4 %5:%6:%7")
                    .arg(directionStr)
                    .arg(timeInfo.year)
                    .arg(timeInfo.month, 2, 10, QLatin1Char('0'))
                    .arg(timeInfo.day, 2, 10, QLatin1Char('0'))
                    .arg(timeInfo.hour, 2, 10, QLatin1Char('0'))
                    .arg(timeInfo.minute, 2, 10, QLatin1Char('0'))
                    .arg(timeInfo.second, 2, 10, QLatin1Char('0'));

    return true;
}

bool CDataProtocol::AnalyzeTrainComeInfo(const char *pData, quint32 nLen)
{
    if (nullptr == pData || nLen < 8)
    {
        return false;
    }
    // //校验不通过，不进行响应
    // if(!validateTrainArrivalData(pData,nLen)){
    //     LOG("来车信息校验失败"+QDateTime::currentDateTime().toString(),LogLevel::LOG_ERROR);
    //     return false;
    // }
    quint32 nIndex = 0;
    //列车方向
    quint16 nDirection=readBigEndianT<quint16>(QByteArray(pData+nIndex,2)); //(quint16 *)pData;

    nIndex += 2;

    //时间
    //qint64 ms;
    //memcpy(&ms, pData+nIndex, 8);
    //nIndex += 8;
    //QDateTime datetime = QDateTime::fromMSecsSinceEpoch(ms);
    //获得来车时间
    QDateTime dt;
    quint32 len = AnalyzeDatetime(pData+nIndex, dt);
    LOG(QString("来车信息解析结果：来车时间：%1,方向：%2").arg(dt.toString("yyyy-MM-dd hh:mm:ss")).arg(nDirection));
    nIndex += len;
    //记录来车过程
    CLineCameraManager::instance()->m_currentTrain.Init();
    //列车到达时间
    CLineCameraManager::instance()->m_currentTrain.nReachDatetime=static_cast<quint64>(dt.toSecsSinceEpoch());;
    //列车方向
    CLineCameraManager::instance()->m_currentTrain.strDirection=nDirection==0?"反向":"正向";
    //图像保存路径
    CLineCameraManager::instance()->m_currentTrain.strFilePath=getCurrentTrainImagePath();
    //检测时间
    CLineCameraManager::instance()->m_currentTrain.nInspectionDatetime=QDateTime::currentMSecsSinceEpoch();
    //检测点
    CLineCameraManager::instance()->m_currentTrain.strDetection=CSystemConfig::instance()->m_detectionName;

    //通知相机开始采集
    CLineCameraManager::instance()->StartCameras();
    return true;
}

bool CDataProtocol::AnalyzeTrainLeaveInfo(const char *pData, quint32 nLen)
{
    if (nullptr == pData || nLen < 8)
    {
        return false;
    }
    //qint64 ms;
    //memcpy(&ms, pData, 8);
    //QDateTime datetime = QDateTime::fromMSecsSinceEpoch(ms);

    //时间
    QDateTime dt;
    AnalyzeDatetime(pData, dt);
    LOG(QString("车离开信息解析结果：离开时间：%1").arg(dt.toString("yyyy-MM-dd hh:mm:ss")));
    CLineCameraManager::instance()->m_currentTrain.nInspectionDatetime=static_cast<quint64>(dt.toSecsSinceEpoch());
    //通知相机停止采集
    CLineCameraManager::instance()->StopCameras();
    //记录来车过程到数据库
    CDatabaseManager::Instance()->insertTrainRecord(CLineCameraManager::instance()->m_currentTrain);

    return true;
}
QString getCurrentTrainImagePath(){
    // 获取当前日期（格式：yyyy-MM-dd）
    const QString currentDate = QDate::currentDate().toString("yyyy-MM-dd");
    // 构建完整路径：savePath/currentDate/subPath
    QString fullPath = CSystemConfig::instance()->m_strTrainImagesSavePath;
    // 规范化路径分隔符（适配不同操作系统）
    if (!fullPath.endsWith('/') && !fullPath.endsWith('\\')) {
        fullPath += '/';
    }
    fullPath +=  currentDate +'/';
    return fullPath;
}

float readBigEndianFloat(const QByteArray &bigEndian4Bytes)
{
    Q_ASSERT(bigEndian4Bytes.size() == 4);

    QDataStream ds(bigEndian4Bytes);
    ds.setByteOrder(QDataStream::BigEndian);              // 1. 大端
    ds.setFloatingPointPrecision(QDataStream::SinglePrecision); // 2. 单精度

    float f = 0;
    ds >> f;                                              // 3. 直接读出
    return f;
}

// 修改后的机车解析函数
bool CDataProtocol::AnalyzeLocomotiveDatasInfo(const char *pData, quint32 nLen) {
    if (nullptr == pData || nLen < 3) return false;
    QString m="";
    for(int i=0;i<nLen;i++){
        m+=QString::number(i)+":"+QString::number(pData[i])+";";
    }
    LOG(m);
    quint32 nIndex = 0;
    quint16 nCount;
    // qToBigEndian(pData + nIndex,&nCount);
    // memcpy(&nCount, pData + nIndex, 2);
    nCount=readBigEndianT<quint16>(QByteArray(pData+nIndex,2));

    nIndex += 2;
    LOG("机车数量："+QString::number(nCount));
    // 计算单条机车数据长度 (包含时间解析长度)
    const quint16 baseLen = 2 + 8 + 4 + 8 + 4 + 20; // 基础字段长度
    if (nLen - nIndex < baseLen * nCount) return false;

    for (quint16 i = 0; i < nCount; ++i) {
        LocomotiveData locoData;

        // 解析序号
        // memcpy(&locoData.sectionNumber, pData + nIndex, 2);
        locoData.sectionNumber=readBigEndianT<quint16>(QByteArray(pData+nIndex,2));
        nIndex += 2;
        LOG("机车序号："+QString::number(locoData.sectionNumber));
        // 解析前轮时间（转换为毫秒时间戳）
        QDateTime frontDt;
        quint32 dtLen = AnalyzeDatetime(pData + nIndex, frontDt);
        locoData.nFrontWheelTime = frontDt.toMSecsSinceEpoch();
        nIndex += dtLen;
        LOG("前轮时间："+QString::number(locoData.nFrontWheelTime)+"，转换结果："+frontDt.toString("yyyy-MM-dd hh:mm:ss"));
        // 解析前轮速度
        locoData.frontWheelSpeed=readBigEndianFloat(QByteArray(pData+nIndex,4));;
        nIndex += 4;
        LOG("前轮速度："+QString::number(locoData.frontWheelSpeed));
        // 解析后轮时间
        QDateTime rearDt;
        dtLen = AnalyzeDatetime(pData + nIndex, rearDt);
        locoData.nRearWheelTime = rearDt.toMSecsSinceEpoch();
        nIndex += dtLen;
        LOG("后轮时间："+QString::number(locoData.nRearWheelTime)+"，转换结果："+rearDt.toString("yyyy-MM-dd hh:mm:ss"));
        // 解析后轮速度
        // memcpy(&locoData.rearWheelSpeed, pData + nIndex, 4);
        locoData.rearWheelSpeed=readBigEndianFloat(QByteArray(pData+nIndex,4));
        nIndex += 4;
        LOG("后轮速度："+QString::number(locoData.rearWheelSpeed));
        // 解析机车信息块（20字节）
        char infoBlock[20];
        memcpy(infoBlock, pData + nIndex, 20);
        nIndex += 20;

        // 转换信息块（每个字节+0x20）
        for (int j = 0; j < 20; ++j) {
            infoBlock[j] += 0x20;
        }
        // QString test(infoBlock);
        // LOG(test);
        QByteArray temp(infoBlock,8);
        locoData.model=temp;
        locoData.department=QByteArray(infoBlock+8,4);
        locoData.state=infoBlock[12];
        locoData.strTrainNumber=QByteArray(infoBlock+13,6);

        LOG(QString("机车信息解析结果：车型:%3,配属段：%2,状态：%9,车次：%1,前轮时间：%4，前轮速度：%5，后轮时间：%6,后轮速度：%7,序号：%8")
                .arg(locoData.strTrainNumber).arg(locoData.department).arg(locoData.model).
            arg(locoData.nFrontWheelTime).arg(locoData.frontWheelSpeed)
            .arg(locoData.nRearWheelTime).arg(locoData.rearWheelSpeed).arg(locoData.sectionNumber).arg(locoData.state));
        //记录列车信息
        CLineCameraManager::instance()->m_currentTrain.strTrainNumber=locoData.strTrainNumber;
        CLineCameraManager::instance()->m_currentTrain.nNumberOfAxles+=2;

        //拼接机车图像
        auto timeRange=calculateVehicleInCameraRange(locoData.nFrontWheelTime,
                                                       locoData.nRearWheelTime,
                                                       locoData.frontWheelSpeed,
                                                       locoData.rearWheelSpeed,
                                                       CSystemConfig::instance()->m_camera_capture_delay
                                                       );
        auto nameMap=CLineCameraManager::instance()->stichImages(timeRange.first,timeRange.second);
        for (auto it = nameMap.begin(); it != nameMap.end(); ++it) {
            locoData.imagePathMap[it.key()] = it.value();
        }
        // 存入数据库
        CDatabaseManager::Instance()->insertLocomotiveData(locoData);
        LOG(QString("机车信息处理和记录成功，车次：%1;车型：%2").arg(QString::fromLatin1(locoData.strTrainNumber)).arg(QString::fromLatin1(locoData.model)));
    }
    return true;
}
//解析机车数据信息
// bool CDataProtocol::AnalyzeLocomotiveDatasInfo(const char *pData, quint32 nLen)
// {
//     if (nullptr == pData || nLen < 3)
//     {
//         return false;
//     }

//     quint32 nIndex = 0;

//     //列车方向
//     //quint8 nDirection = pData[nIndex++];

//     //机车数量
//     quint16 nCount;
//     memcpy(&nCount, pData+nIndex, 2);
//     nIndex += 2;

//     //机车数据长度:序号（2）+ 前轮时间（8）+前轮速度（4）+后轮时间（8）+后轮速度（4）+ 车型（3）+配属段（4）+机车编号（4）+车次（5）
//     quint16 nDataLen = 2 + 8 + 4 + 8 + 4 + 3 + 4 + 4 + 5;
//     if (nLen - nIndex < nDataLen * nCount)
//     {
//         return false;
//     }

//     for (quint16 i = 0; i < nCount; ++i)
//     {
//         //序号
//         quint16 nOrder;
//         memcpy(&nOrder, pData+nIndex, 2);
//         nIndex += 2;

//         //前轮时间
//         //qint64 ms1;
//         //memcpy(&ms1, pData+nIndex, 8);
//         //nIndex += 8;
//         //时间
//         QDateTime dt1;
//         quint32 len = AnalyzeDatetime(pData+nIndex, dt1);
//         nIndex += len;


//         //前轮速度
//         float speed1;
//         memcpy(&speed1, pData+nIndex, 4);
//         nIndex += 4;

//         //后轮时间
//         //qint64 ms2;
//         //memcpy(&ms2, pData+nIndex, 8);
//         //nIndex += 8;
//         QDateTime dt2;
//         len = AnalyzeDatetime(pData+nIndex, dt2);
//         nIndex += len;

//         //后轮速度
//         float speed2;
//         memcpy(&speed2, pData+nIndex, 4);
//         nIndex += 4;

//         //提取并转换机车信息
//         char *pLocomotiveInfo = new char[20];
//         for (int i = 0; i < 20; ++i)
//         {
//             pLocomotiveInfo[i] = pData[nIndex+i] + 0x20;
//         }
//         nIndex += 20;

//         QByteArray arrayTemp;
//         //车型
//         arrayTemp = QByteArray(pLocomotiveInfo, 8);
//         QString strModel = arrayTemp;

//         //配属段
//         arrayTemp = QByteArray(pLocomotiveInfo+8, 4);
//         QString strAllocationSection = arrayTemp;

//         //机车状态
//         arrayTemp = QByteArray(pLocomotiveInfo+12, 1);
//         QString strStatus = arrayTemp;

//         //车次
//         arrayTemp = QByteArray(pLocomotiveInfo+13, 6);
//         QString strTrainNumber = arrayTemp;

//         delete[] pLocomotiveInfo;
//         /*QByteArray arrayTemp;
//         //车型
//         arrayTemp = QByteArray(pData+nIndex, 3);
//         QString strModel = QString(arrayTemp);
//         nIndex += 3;

//         //配属段
//         arrayTemp = QByteArray(pData+nIndex, 4);
//         QString strAllocationSection = arrayTemp;
//         nIndex += 4;

//         //机车编号
//         arrayTemp = QByteArray(pData+nIndex, 4);
//         QString strNumber = arrayTemp;
//         nIndex += 4;

//         //车次
//         arrayTemp = QByteArray(pData+nIndex, 5);
//         QString strTrainNumber = arrayTemp;
//         nIndex += 5;*/
//     }

//     return true;
// }

// bool CDataProtocol::AnalyzeCarriageDatasInfo(const char *pData, quint32 nLen)
// {
//     if (nullptr == pData || nLen < 2)
//     {
//         return false;
//     }
//     CarriageInformation info;
//     info.strTrainNumber=CLineCameraManager::instance()->m_currentTrain.strTrainNumber;
//     info.nRearWheelTime=CLineCameraManager::instance()->m_currentTrain.nReachDatetime;
//     quint32 nIndex = 0;
//     //车辆数量
//     quint16 nCount=readBigEndianT<quint16>(QByteArray(pData+nIndex,2));
//     // memcpy(&nCount, pData, 2);
//     nIndex += 2;

//     //车辆数据长度:序号（2）+前轮时间（8）+前轮速度（4）+后轮时间（8）+后轮速度（4）+车型（7）+车号（7）+厂家和日期（6）+换长（2）
//     quint32 nDataLen = 2 + 8 + 4 + 8 + 4 +7 + 7 + 6 + 2;
//     if (nLen - nIndex < nDataLen * nCount)
//     {
//         return false;
//     }

//     for (quint16 i = 0; i < nCount; ++i)
//     {
//         //序号
//         quint16 nOrder=readBigEndianT<quint16>(QByteArray(pData+nIndex,2));

//         nIndex += 2;
//         info.nOrder=nOrder;

//         //前轮时间
//         //qint64 ms1;
//         //memcpy(&ms1, pData+nIndex, 8);
//         //nIndex += 8;
//         QDateTime dt1;
//         quint32 len = AnalyzeDatetime(pData+nIndex, dt1);
//         nIndex += len;
//         info.nFrontWheelTime=dt1.toMSecsSinceEpoch();

//         //前轮速度
//         float speed1=readBigEndianT<float>(QByteArray(pData+nIndex,4));
//         nIndex += 4;
//         info.frontWheelSpeed=speed1;

//         //后轮时间
//         //qint64 ms2;
//         //memcpy(&ms2, pData+nIndex, 8);
//         //nIndex += 8;
//         QDateTime dt2;
//         len = AnalyzeDatetime(pData+nIndex, dt2);
//         nIndex += len;
//         info.nRearWheelTime=dt2.toMSecsSinceEpoch();

//         //后轮速度
//         float speed2=readBigEndianT<float>(QByteArray(pData+nIndex,4));
//         nIndex += 4;
//         info.rearWheelSpeed=speed2;

//         //提取并转换机车信息
//         char *pCarriageInfo = new char[20];
//         for (int i = 0; i < 20; ++i)
//         {
//             pCarriageInfo[i] = pData[nIndex+i] + 0x20;
//         }
//         nIndex += 20;


//         QByteArray arrayTemp;
//         //车型
//         arrayTemp = QByteArray(pCarriageInfo, 7);
//         // QString strModel = arrayTemp;
//         info.strCarriageModel=arrayTemp;

//         //车号
//         arrayTemp = QByteArray(pCarriageInfo+7, 7);
//         // QString strNumber = arrayTemp;
//         info.strWagonNumber=arrayTemp;

//         //换长
//         arrayTemp = QByteArray(pCarriageInfo+14, 2);
//         // float fConverredLength = QString(arrayTemp).toUInt() * 0.1;
//         info.fConverredLength=QString(arrayTemp).toUInt() * 0.1;
//         //厂家和日期
//         arrayTemp = QByteArray(pCarriageInfo+16, 4);
//         // QString strFactoryAndDatetime = arrayTemp;
//         info.strFactoryAndDatetime=arrayTemp;

//         LOG(QString("车厢信息解析结果：车次：%1,车号：%2,车型:%3,前轮时间：%4，前轮速度：%5，后轮时间：%6,后轮速度：%7,序号：%8,厂家和日期：%9")
//                 .arg(info.strTrainNumber).arg(info.strWagonNumber).arg(info.strCarriageModel).arg(info.nFrontWheelTime).arg(info.frontWheelSpeed)
//                 .arg(info.nRearWheelTime).arg(info.rearWheelSpeed).arg(info.nOrder).arg(info.strFactoryAndDatetime));
//         delete[] pCarriageInfo;

//         //拼接车箱图像
//         auto timeRange=calculateVehicleInCameraRange(info.nFrontWheelTime,
//                                                        info.nRearWheelTime,
//                                                        info.frontWheelSpeed,
//                                                        info.rearWheelSpeed,
//                                                        CSystemConfig::instance()->m_camera_capture_delay
//                                                        );
//         auto nameMap=CLineCameraManager::instance()->stichImages(timeRange.first,timeRange.second);
//         for (auto it = nameMap.begin(); it != nameMap.end(); ++it) {
//             info.imagePathMap[it.key()] = it.value();
//         }
//         //识别车厢号
//         if(info.strWagonNumber.isEmpty()){
//             auto uploader=NetworkImageUploader::getInstance();
//             // 配置上传参数
//             uploader->setUrl(QUrl(QString("http://%1:%2/ocr").arg(CSystemConfig::instance()->m_strImageRecAddress).arg(CSystemConfig::instance()->m_nRecPort))); // 配置的服务器地址
//             uploader->setFile(info.imagePathMap["left"]);
//             uploader->setFieldName("image");
//             uploader->setTimeout(60000); // 60秒超时
//             uploader->setRetryCount(1);  // 重试1次
//             uploader->setVerifySSL(false);

//             // 添加表单字段
//             QMap<QString, QString> fields;
//             fields["description"] = "Test image upload";
//             fields["timestamp"] = QDateTime::currentDateTime().toString();
//             uploader->setFormFields(fields);

//             // 连接信号
//             QObject::connect(uploader, &NetworkImageUploader::uploadProgress,
//                              [](qint64 sent, qint64 total, double percent) {
//                                  qDebug() << "Progress:" << sent << "/" << total
//                                           << "(" << QString::number(percent, 'f', 1) << "%)";
//                              });

//             QObject::connect(uploader, &NetworkImageUploader::uploadStarted,
//                              []() {
//                                  qDebug() << "=== Upload started ===";
//                              });

//             QObject::connect(uploader, &NetworkImageUploader::uploadFinished,
//                              [&](const QByteArray &response) {
//                                  QJsonParseError err;
//                                  QJsonDocument doc = QJsonDocument::fromJson(response, &err);
//                                  if (err.error != QJsonParseError::NoError) {
//                                      qWarning() << "parse error:" << err.errorString();
//                                      return 1;
//                                  }
//                                  QString result=doc["result"].toString();
//                                  doc=QJsonDocument::fromJson(result.toLocal8Bit());
//                                  info.strWagonNumber=doc["best_text"].toString();
//                                  CDatabaseManager::Instance()->insertCarriage(info);
//                                  //记录列车信息
//                                  CLineCameraManager::instance()->m_currentTrain.nNumberOfCarriage++;
//                                  CLineCameraManager::instance()->m_currentTrain.nNumberOfAxles+=2;
//                                  LOG("车箱信息解析和记录成功，车箱号："+info.strWagonNumber+";车次："+info.strTrainNumber);
//                              });

//             QObject::connect(uploader, &NetworkImageUploader::uploadFailed,
//                              [](NetworkImageUploader::UploadError error, const QString &errorString) {
//                                  qDebug() << "=== Upload failed ===";
//                                  qDebug() << "Error:" << static_cast<int>(error);
//                                  qDebug() << "Message:" << errorString;

//                                  // QTimer::singleShot(0, ,QApplication::instance(), &QCoreApplication::quit);
//                              });

//             // 开始上传
//             uploader->startUpload();

//         }else{

//             CDatabaseManager::Instance()->insertCarriage(info);
//             //记录列车信息
//             CLineCameraManager::instance()->m_currentTrain.nNumberOfCarriage++;
//             CLineCameraManager::instance()->m_currentTrain.nNumberOfAxles+=2;
//             LOG("车箱信息解析和记录成功，车箱号："+info.strWagonNumber+";车次："+info.strTrainNumber);
//         }
//     }

//     return true;
// }
bool CDataProtocol::AnalyzeCarriageDatasInfo(const char *pData, quint32 nLen)
{
    if (nullptr == pData || nLen < 3) return false;
    QString m="";
    for(int i=0;i<nLen;i++){
        m+=QString::number(i)+":"+QString::number(pData[i])+";";
    }
    LOG(m);
    quint32 nIndex = 0;
    quint16 nCount;
    // qToBigEndian(pData + nIndex,&nCount);
    // memcpy(&nCount, pData + nIndex, 2);
    nCount=readBigEndianT<quint16>(QByteArray(pData+nIndex,2));

    nIndex += 2;
    LOG("机车数量："+QString::number(nCount));
    // 计算单条机车数据长度 (包含时间解析长度)
    const quint16 baseLen = 2 + 8 + 4 + 8 + 4 + 20; // 基础字段长度
    if (nLen - nIndex < baseLen * nCount) return false;

    for (quint16 i = 0; i < nCount; ++i) {
        CarriageInformation info;

        // 解析序号
        // memcpy(&locoData.sectionNumber, pData + nIndex, 2);
        quint16 order=readBigEndianT<quint16>(QByteArray(pData+nIndex,2));
        nIndex += 2;
        LOG("机车序号："+QString::number(order));
        // 解析前轮时间（转换为毫秒时间戳）
        QDateTime frontDt;
        quint32 dtLen = AnalyzeDatetime(pData + nIndex, frontDt);
        info.nFrontWheelTime = frontDt.toMSecsSinceEpoch();
        nIndex += dtLen;
        LOG("前轮时间："+QString::number(info.nFrontWheelTime)+"，转换结果："+frontDt.toString("yyyy-MM-dd hh:mm:ss"));
        // 解析前轮速度
        info.frontWheelSpeed=readBigEndianFloat(QByteArray(pData+nIndex,4));;
        nIndex += 4;
        LOG("前轮速度："+QString::number(info.frontWheelSpeed));
        // 解析后轮时间
        QDateTime rearDt;
        dtLen = AnalyzeDatetime(pData + nIndex, rearDt);
        info.nRearWheelTime = rearDt.toMSecsSinceEpoch();
        nIndex += dtLen;
        LOG("后轮时间："+QString::number(info.nRearWheelTime)+"，转换结果："+rearDt.toString("yyyy-MM-dd hh:mm:ss"));
        // 解析后轮速度
        // memcpy(&locoData.rearWheelSpeed, pData + nIndex, 4);
        info.rearWheelSpeed=readBigEndianFloat(QByteArray(pData+nIndex,4));
        nIndex += 4;
        LOG("后轮速度："+QString::number(info.rearWheelSpeed));

        // 解析机车信息块（20字节）
        char infoBlock[20];
        memcpy(infoBlock, pData + nIndex, 20);
        nIndex += 20;

        // 转换信息块（每个字节+0x20）
        for (int j = 0; j < 20; ++j) {
            infoBlock[j] += 0x20;
        }
        // QString test(infoBlock);
        // LOG(test);
        //车型
        QByteArray temp(infoBlock,7);

        info.strCarriageModel=temp;
         //车号
        info.strWagonNumber=QByteArray(infoBlock+7,7);
        //厂家和日期
        info.strFactoryAndDatetime=QByteArray(infoBlock+14,4);
        //换长
        // qint16 len;
        // memcpy(&len,infoBlock+18,2);
        auto arrayTemp = QByteArray(infoBlock + 18, 2);
        info.fConverredLength =QString(arrayTemp).toUInt() * 0.1;// len*0.1; //
        info.strTrainNumber=CLineCameraManager::instance()->m_currentTrain.strTrainNumber;
        info.nReachDatetime=CLineCameraManager::instance()->m_currentTrain.nReachDatetime;
        LOG(QString("车厢信息解析结果：车次：%1,车号：%2,车型:%3,前轮时间：%4，前轮速度：%5，后轮时间：%6,后轮速度：%7,序号：%8,厂家和日期：%9,换长：%10")
                .arg(info.strTrainNumber).arg(info.strWagonNumber).arg(info.strCarriageModel).arg(info.nFrontWheelTime).arg(info.frontWheelSpeed)
                .arg(info.nRearWheelTime).arg(info.rearWheelSpeed).arg(info.nOrder).arg(info.strFactoryAndDatetime).arg(info.fConverredLength));

        // 在线程中执行拼接车厢和识别车厢号、存储数据的过程
        processCarriageData(info);
    }

    return true;
}

void CDataProtocol::processCarriageData(CarriageInformation info)
{
    qint64 startTime=info.nFrontWheelTime,endTime=info.nRearWheelTime;
    if(QString::number(startTime).length()<13){
        QDateTime firstTime=QDateTime::fromSecsSinceEpoch(startTime);
        startTime=firstTime.toMSecsSinceEpoch();
        firstTime=QDateTime::fromSecsSinceEpoch(endTime);
        endTime=firstTime.toMSecsSinceEpoch();
    }
    //拼接车箱图像
    auto timeRange = calculateVehicleInCameraRange(startTime,
                                                   endTime,
                                                   info.frontWheelSpeed,
                                                   info.rearWheelSpeed,
                                                   CSystemConfig::instance()->m_camera_capture_delay);
    auto nameMap = CLineCameraManager::instance()->stichImages(timeRange.first, timeRange.second);
    for (auto it = nameMap.begin(); it != nameMap.end(); ++it) {
        info.imagePathMap[it.key()] = it.value();
    }

    //识别车厢号
    if (info.strWagonNumber.isEmpty()) {
        processOCRCarriageNumber(info);
    } else {
        saveCarriageData(info);
    }
}

void CDataProtocol::processOCRCarriageNumber(CarriageInformation info)
{
    auto uploader = NetworkImageUploader::getInstance();

    // 配置上传参数
    uploader->setUrl(QUrl(QString("http://%1:%2/ocr").arg(CSystemConfig::instance()->m_strImageRecAddress).arg(CSystemConfig::instance()->m_nRecPort)));
    uploader->setFile(info.imagePathMap["left"]);
    uploader->setFieldName("image");
    uploader->setTimeout(60000); // 60秒超时
    uploader->setRetryCount(1);  // 重试1次
    uploader->setVerifySSL(false);

    // 添加表单字段
    QMap<QString, QString> fields;
    fields["description"] = "Test image upload";
    fields["timestamp"] = QDateTime::currentDateTime().toString();
    uploader->setFormFields(fields);

    // 使用QEventLoop等待异步操作完成
    QEventLoop loop;
    QObject::connect(uploader, &NetworkImageUploader::uploadFinished, &loop, &QEventLoop::quit);
    QObject::connect(uploader, &NetworkImageUploader::uploadFailed, &loop, &QEventLoop::quit);

    // 连接信号处理结果
    QObject::connect(uploader, &NetworkImageUploader::uploadFinished,
                     [&](const QByteArray &response) {
                         QJsonParseError err;
                         QJsonDocument doc = QJsonDocument::fromJson(response, &err);
                         if (err.error != QJsonParseError::NoError) {
                             qWarning() << "parse error:" << err.errorString();
                             return;
                         }
                         QString result = doc["result"].toString();
                         doc = QJsonDocument::fromJson(result.toLocal8Bit());
                         info.strWagonNumber = doc["best_text"].toString();
                         saveCarriageData(info);
                     });

    QObject::connect(uploader, &NetworkImageUploader::uploadFailed,
                     [&](NetworkImageUploader::UploadError error, const QString &errorString) {
                         qDebug() << "=== Upload failed ===";
                         qDebug() << "Error:" << static_cast<int>(error);
                         qDebug() << "Message:" << errorString;
                         // 即使失败也保存数据
                         saveCarriageData(info);
                     });

    // 开始上传并等待完成
    uploader->startUpload();
    loop.exec();
}

void CDataProtocol::saveCarriageData(const CarriageInformation &info)
{
    CDatabaseManager::Instance()->insertCarriage(info);

    //记录列车信息
    CLineCameraManager::instance()->m_currentTrain.nNumberOfCarriage++;
    CLineCameraManager::instance()->m_currentTrain.nNumberOfAxles += 2;

    LOG("车箱信息解析和记录成功，车箱号：" + info.strWagonNumber + ";车次：" + info.strTrainNumber);
}
quint32 CDataProtocol::PackCommonRes(quint8 cmd,const char *pData, quint32 nLen, int nErrorCode, QByteArray& array){
    if (nullptr == pData || nLen <= 0)
    {
        return 0;
    }

    array.clear();

    //数据长度
    // quint16 nDataLen = 0;
    // memcpy(&nDataLen, pData, 2);

    //报文长度,指令代码+数据长度+数据+错误码+校验码
    quint32 nPacketLen = 2 + 4 + nLen + 1 + 2;
    char *pPacket = new char[nPacketLen + 4];

    int nIndex = 0;
    //报文长度
    memcpy(pPacket+nIndex, &nPacketLen, 4);
    nIndex += 4;

    //信息/指令代码
    pPacket[nIndex++] =cmd;// 0x52;
    pPacket[nIndex++] = 0x00;

    memcpy(pPacket+nIndex, &nLen, 4);
    nIndex += 4;
    //数据长度及数据
    memcpy(pPacket+nIndex, pData, nLen);
    nIndex += nLen;

    //错误码
    pPacket[nIndex++] = (char)nErrorCode;

    //校验码
    // pPacket[nIndex] = CalcCheckCode(pPacket, nIndex);
    quint16 nCheckCode = CalcCheckCode16(pPacket, nIndex);
    memcpy(pPacket+nIndex, &nCheckCode, 2);
    nIndex += 2;

    array = QByteArray(pPacket, nIndex);
    delete[] pPacket;

    return nPacketLen + 4;
}
quint32 CDataProtocol::PackRegisterRes(const char *pData, quint32 nLen, int nErrorCode, QByteArray& array)
{
    // return PackCommonRes(0x52,pData,nLen,nErrorCode,array);
    if (nullptr == pData || nLen <= 0)
    {
        return 0;
    }

    array.clear();

    //数据长度
    quint16 nDataLen = 0;
    memcpy(&nDataLen, pData, 2);

    //报文长度,指令代码+数据长度+数据+错误码+校验码
    quint32 nPacketLen = 2 + 2 + nDataLen + 1 + 2;
    char *pPacket = new char[nPacketLen + 4];

    int nIndex = 0;
    //报文长度
    memcpy(pPacket+nIndex, &nPacketLen, 4);
    nIndex += 4;

    //信息/指令代码
    pPacket[nIndex++] = 0x52;
    pPacket[nIndex++] = 0x00;

    //数据长度及数据
    memcpy(pPacket+nIndex, pData, nDataLen+2);
    nIndex += nDataLen + 2;

    //错误码
    pPacket[nIndex++] = (char)nErrorCode;

    //校验码
    // pPacket[nIndex] = CalcCheckCode(pPacket, nIndex);
    quint16 nCheckCode = CalcCheckCode16(pPacket, nIndex);
    memcpy(pPacket+nIndex, &nCheckCode, 2);
    nIndex += 2;

    array = QByteArray(pPacket, nIndex);
    delete[] pPacket;

    return nPacketLen + 4;
}
quint32 CDataProtocol::PackLoginRes( /*const char *pData,quint32 nLen, */int nErrorCode, QByteArray &array, UserInfo *user)
{
    // if (nullptr == pData || nLen <= 0)
    // {
    //     return 0;
    // }

    array.clear();

    // 原始数据长度
    quint16 nDataLen = 0;
    // memcpy(&nDataLen, pData, 2);

    // 构建新的数据部分
    QByteArray newData;

    // // 复制原始数据（不包括长度字段）
    // if (nDataLen > 0 && nLen >= 2 + nDataLen) {
    //     newData.append(pData + 2, nDataLen);
    // }

    // 如果user不为空且登录成功，添加用户信息
    if (user != nullptr && nErrorCode == 0) {
        // 添加分隔符（如果原始数据不为空）
        if (!newData.isEmpty() && !newData.endsWith(',')) {
            newData.append(',');
        }

        // 添加用户信息
        if (!user->name.isEmpty()) {
            newData.append("name=" + user->name.toUtf8());
            newData.append(',');
        }

        if (!user->workNumber.isEmpty()) {
            newData.append("worknumber=" + user->workNumber.toUtf8());
            newData.append(',');
        }

        // 移除最后一个多余的逗号
        if (newData.endsWith(',')) {
            newData.chop(1);
        }
    }

    // 更新数据长度
    quint16 newDataLen = static_cast<quint16>(newData.size());

    // 报文长度 = 指令代码(2) + 数据长度(2) + 数据(n) + 错误码(1) + 校验码(2)
    quint32 nPacketLen = 2 + 2 + newDataLen + 1 + 2;
    char *pPacket = new char[nPacketLen + 4]; // +4 用于存储报文长度本身

    int nIndex = 0;
    // 报文长度
    memcpy(pPacket + nIndex, &nPacketLen, 4);
    nIndex += 4;

    // 信息/指令代码
    pPacket[nIndex++] = 0x54;
    pPacket[nIndex++] = 0x00;

    // 数据长度
    memcpy(pPacket + nIndex, &newDataLen, 2);
    nIndex += 2;

    // 数据
    if (newDataLen > 0) {
        memcpy(pPacket + nIndex, newData.constData(), newDataLen);
        nIndex += newDataLen;
    }

    // 错误码
    pPacket[nIndex++] = static_cast<char>(nErrorCode);

    // 计算校验码（从报文长度开始到错误码结束的所有字节的累加和）
    // quint16 nCheckCode = 0;
    // for (int i = 0; i < nIndex; i++) {
    //     nCheckCode += static_cast<quint8>(pPacket[i]);
    // }
    quint16 nCheckCode = CalcCheckCode16(pPacket, nIndex);
    memcpy(pPacket + nIndex, &nCheckCode, 2);
    nIndex += 2;

    array = QByteArray(pPacket, nIndex);
    delete[] pPacket;

    return nIndex; // 返回整个报文的长度
}
//原始的登录响应报文函数
// quint32 CDataProtocol::PackLoginRes(const char *pData, quint32 nLen, int nErrorCode, QByteArray &array,UserInfo *user)
// {
//     if (nullptr == pData || nLen <= 0)
//     {
//         return 0;
//     }

//     array.clear();

//     //数据长度
//     quint16 nDataLen = 0;
//     memcpy(&nDataLen, pData, 2);

//     //报文长度,指令代码+数据长度+数据+错误码+校验码
//     quint32 nPacketLen = 2 + 2 + nDataLen + 1 + 2;
//     char *pPacket = new char[nPacketLen + 4];

//     int nIndex = 0;
//     //报文长度
//     memcpy(pPacket+nIndex, &nPacketLen, 4);
//     nIndex += 4;

//     //信息/指令代码
//     pPacket[nIndex++] = 0x54;
//     pPacket[nIndex++] = 0x00;

//     //数据长度及数据
//     memcpy(pPacket+nIndex, pData, nDataLen+2);
//     nIndex += nDataLen + 2;

//     //错误码
//     pPacket[nIndex++] = (char)nErrorCode;

//     //校验码
//     //pPacket[nIndex] = CalcCheckCode(pPacket, nIndex);
//     quint16 nChechCode = CalcCheckCode16(pPacket, nIndex);
//     memcpy(pPacket+nIndex, &nChechCode, 2);
//     nIndex += 2;

//     array = QByteArray(pPacket, nIndex);
//     delete[] pPacket;

//     return nPacketLen + 4;
// }

quint32 CDataProtocol::PackLogoutRes(const char *pData, quint32 nLen, int nErrorCode, QByteArray &array)
{
    if (nullptr == pData || nLen <= 0)
    {
        return 0;
    }

    array.clear();

    //数据长度
    quint16 nDataLen = 0;
    memcpy(&nDataLen, pData, 2);

    //报文长度,指令代码+数据长度+数据+错误码+校验码
    quint32 nPacketLen = 2 + 2 + nDataLen + 1 + 2;
    char *pPacket = new char[nPacketLen + 4];

    int nIndex = 0;
    //报文长度
    memcpy(pPacket+nIndex, &nPacketLen, 4);
    nIndex += 4;

    //信息/指令代码
    pPacket[nIndex++] = 0x56;
    pPacket[nIndex++] = 0x00;

    //数据长度及数据
    memcpy(pPacket+nIndex, pData, nDataLen+2);
    nIndex += nDataLen + 2;

    //错误码
    pPacket[nIndex++] = (char)nErrorCode;

    //校验码
    //pPacket[nIndex] = CalcCheckCode(pPacket, nIndex);
    quint16 nCheckCode = CalcCheckCode16(pPacket, nIndex);
    memcpy(pPacket+nIndex, &nCheckCode, 2);
    nIndex += 2;

    array = QByteArray(pPacket, nIndex);
    delete[] pPacket;

    return nPacketLen + 4;
}

quint32 CDataProtocol::PackTrainSearchRes(const QList<TrainInformation> &trainInfoList, QByteArray &array)
{
    array.clear();

    //指令代码
    array.append(0x62);

    //过车数量
    quint16 nTrainCount = trainInfoList.size();
    array.append((char *)&nTrainCount, 2);

    for (int i = 0; i < trainInfoList.size(); ++i)
    {
        TrainInformation info = trainInfoList.at(i);

        QString strKeyValue = QString("[trainnumber=%1,reachdatetime=%2,direction=%3,numberofaxles=%4,"
                                      "numberofcarriage=%5,filepath=%6,detection=%7,inspectiondatetime=%8]")
                                  .arg(info.strTrainNumber).arg(QString::number(info.nReachDatetime)).arg(info.strDirection)
                                  .arg(QString::number(info.nNumberOfAxles)).arg(QString::number(info.nNumberOfCarriage)).arg(info.strFilePath)
                                  .arg(info.strDetection).arg(QString::number(info.nInspectionDatetime));
        QByteArray arrayKeyValue = strKeyValue.toLocal8Bit();
        quint16 nDataLen = arrayKeyValue.size();

        //数据长度
        array.append((char *)&nDataLen, 2);
        //数据
        array.append(arrayKeyValue);
    }

    //报文长度
    quint32 nPacketLen = array.size();
    array.insert(0, (char *)&nPacketLen, 4);

    return array.size();
}

quint32 CDataProtocol::PackTrainNumberSearchRes(
    const QList<CarriageInformation> &carriageInfoList,
    const QString &strTrainNumber,
    quint32 nReachDatetime,
    QByteArray &array)
{
    array.clear();

    //指令代码
    array.append(0x64);
    array.append('\0');

    //车次数据
    QString strTrainKeyValue = QString("[trainnumber=%1,reachdatetime=%2]")
                                   .arg(strTrainNumber).arg(QString::number(nReachDatetime));
    QByteArray arrayTrainKeyValue = strTrainKeyValue.toLocal8Bit();
    quint16 nTrainDataLen = arrayTrainKeyValue.size();

    //数据长度
    array.append((char *)&nTrainDataLen, 2);
    //数据
    array.append(arrayTrainKeyValue);

    //车辆信息
    //车厢数量
    quint16 nCarriageCount = carriageInfoList.size();
    array.append((char *)&nCarriageCount, 2);

    //车厢数据
    for (int i = 0; i < nCarriageCount; ++i)
    {
        CarriageInformation info = carriageInfoList.at(i);

        QString strKeyValue = QString("[wagonnumber=%1,carriagemodel=%2,convertedlength=%3,carriagemodelcode=%4,factoryanddatetime=%5]")
                                  .arg(info.strWagonNumber).arg(info.strCarriageModel).arg(QString::number(info.fConverredLength))
                                  .arg(info.strCarriageModelCode).arg(info.strFactoryAndDatetime);
        QByteArray arrayKeyValue = strKeyValue.toLocal8Bit();
        quint16 nDataLen = arrayKeyValue.size();

        //数据长度
        array.append((char *)&nDataLen, 2);
        //数据
        array.append(arrayKeyValue);
    }

    //报文长度
    quint32 nPacketlen = array.size();
    array.insert(0, (char *)&nPacketlen, 4);

    return array.size();
}
// quint16 calculateChecksum(const QByteArray &data)
// {
//     quint16 checksum = 0;
//     for (char byte : data) {
//         checksum += static_cast<quint16>(byte);
//     }
//     return checksum;
// }
// quint32 CDataProtocol::PackCarriagePictureRes(const QString &filePath, std::vector<QByteArray> &array)
// {
//     // 读取文件数据
//     QFile file(filePath);
//     if (!file.exists()) {
//         LOG("File not found: " + filePath,LogLevel::LOG_WARNING);
//         return 0;
//     }

//     if (!file.open(QIODevice::ReadOnly)) {
//         LOG("Cannot open file: " + filePath,LogLevel::LOG_WARNING);
//         return 0;
//     }

//     QByteArray fileData = file.readAll();
//     file.close();
//     quint64 remainLen=fileData.size();
//     quint16 order=0;
//     quint64 dataPos=0;
//     quint16 totalCount=(remainLen+59999)/60000;
//     while(remainLen>0){
//         QByteArray packet;
//         // QDataStream stream(&array, QIODevice::WriteOnly);
//         // stream.setByteOrder(QDataStream::BigEndian);

//         QByteArray filePathData = filePath.toUtf8();
//         quint32 imageLength =remainLen>60000?60000:remainLen;

//         // 计算报文长度（不包括报文长度和校验码字段）
//         quint32 packetLength = 1 + 2 + filePathData.size()+4 + 4 + imageLength+2;

//         // 构建报文
//         // stream << packetLength; // 报文长度
//         packet.append((char*)&packetLength,4);
//         quint32 testi;
//         memcpy(&testi,packet.data(),4);
//         // stream << static_cast<quint8>(0x66); // 指令代码
//         packet.append(0x66);
//         quint16 filePathLen=filePathData.length();
//         packet.append((char*)&filePathLen,2);
//         packet.append(filePathData);
//         //添加总包数
//         packet.append((char*)&totalCount,2);
//         //附加序号
//         packet.append((char*)&order,2);
//         order++;
//         packet.append((char*)&imageLength,4);
//         remainLen-=imageLength;
//         auto subData=fileData.mid(dataPos,imageLength);
//         packet.append(subData);
//         dataPos+=imageLength;
//         // stream << static_cast<quint16>(filePathData.size()); // 文件名长度
//         // stream.writeRawData(filePathData.constData(), filePathData.size()); // 文件路径
//         // stream << imageLength; // 图像长度
//         // stream.writeRawData(fileData.constData(), fileData.size()); // 图像文件数据

//         // 计算校验码（不包括报文长度字段）
//         quint16 nCheckCode = CalcCheckCode16(packet.data(), packet.length());
//         packet.append((char*)&nCheckCode,2);
//         array.push_back(packet);
//     }
//     return array.size();
// }

quint32 CDataProtocol::PackTimeSynCom(QByteArray &array)
{
    array.clear();

    char *pPacket = new char[16];
    int nIndex = 0;

    //报文长度
    quint32 nPacketLen = 12;
    memcpy(pPacket+nIndex, &nPacketLen, 4);
    nIndex += 4;

    //指令代码
    pPacket[nIndex++] = 0x01;
    pPacket[nIndex++] = 0x00;

    //时间
    //qint64 ms = QDateTime::currentDateTime().toMSecsSinceEpoch();
    //memcpy(pPacket+nIndex, &ms, 8);
    //nIndex += 8;

    //时间 year
    QDateTime curDateTime = QDateTime::currentDateTime();
    quint16 year = curDateTime.date().year();
    memcpy(pPacket+nIndex, &year, 2);
    nIndex += 2;

    //momth
    quint8 month = curDateTime.date().month();
    memcpy(pPacket+nIndex, &month, 1);
    nIndex++;

    //day
    quint8 day = curDateTime.date().day();
    memcpy(pPacket+nIndex, &day, 1);
    nIndex++;

    //weekday
    quint8 weekday = curDateTime.date().dayOfWeek();
    memcpy(pPacket+nIndex, &weekday, 1);
    nIndex++;

    //hour
    quint8 hour = curDateTime.time().hour();
    memcpy(pPacket+nIndex, &hour, 1);
    nIndex++;

    //minute
    quint8 minute = curDateTime.time().minute();
    memcpy(pPacket+nIndex, &minute, 1);
    nIndex++;

    //second
    quint8 second = curDateTime.time().second();
    memcpy(pPacket+nIndex, &second, 1);
    nIndex++;

    //校验码
    //pPacket[nIndex] = CalcCheckCode(pPacket, nIndex);
    quint16 nChechCode =CalcCheckCode16(pPacket, nIndex);
    memcpy(pPacket+nIndex, &nChechCode, 2);
    nIndex += 2;

    array = QByteArray(pPacket, nIndex);
    delete[] pPacket;

    return nPacketLen + 4;
}

// quint8 CDataProtocol::CalcCheckCode(char *pFrame, quint32 nLen)
// {
//     if (nullptr == pFrame || nLen <= 0)
//     {
//         return 0;
//     }

//     quint8 nCheck = 0;
//     for (quint32 i = 0; i < nLen; ++i)
//     {
//         nCheck += (quint8)pFrame[i];
//     }

//     return nCheck;
// }

quint16 CDataProtocol::CalcCheckCode16(char *pFrame, quint32 nLen)
{
    if (nullptr == pFrame || nLen <= 0)
    {
        return 0;
    }

    quint16 nCheck = 0;
    quint8 * pData= (quint8*)pFrame;
    for (quint32 i = 0; i < nLen; ++i)
    {
        nCheck +=*pData++;
    }

    return nCheck;
}

void CDataProtocol::AnalyzeStringtoKeyValue(const QString &str, QMap<QString, QString> &mapKeyValue)
{
    QStringList list= str.split(',');

    for (int i = 0; i < list.size(); ++i)
    {
        QString strkeyValue = list.at(i);
        QStringList listKeyValue = strkeyValue.split('=');
        if (listKeyValue.size() != 2)
        {
            continue;
        }

        QString strKey = listKeyValue.at(0).trimmed();
        QString strValue = listKeyValue.at(1).trimmed();
        mapKeyValue[strKey] = strValue;
    }
}

quint32 CDataProtocol::AnalyzeDatetime(const char *pData, QDateTime &dt)
{
    if (nullptr == pData)
    {
        return 0;
    }

    quint32 nIndex = 0;

    //year
    quint16 year;
    memcpy(&year, pData+nIndex, 2);
    nIndex += 2;

    int month = (int)pData[nIndex++];
    int day = (int)pData[nIndex++];
    nIndex++;//weekday
    int hour = (int)pData[nIndex++];
    int minute = (int)pData[nIndex++];
    int second = (int)pData[nIndex++];

    //dt.date().setDate(year, month, day);
    //dt.time().setHMS(hour, minute, second);
    dt.setDate(QDate(year, month, day));
    dt.setTime(QTime(hour, minute, second));

    return nIndex;
}
/**
 * @brief 计算列车运动的开始/结束时间
 * @param bStart true: 已知结束时间t1求开始时间t0; false: 已知开始时间t0求结束时间t1
 * @param a 加速度 (m/s²)，正值加速/负值减速
 * @param v 速度 (m/s)，始终大于0
 * @param s 经过距离 (m)，始终大于0
 * @param knownTime 已知的时间值 (s)
 *               - 当 bStart=true 时表示结束时间 t1
 *               - 当 bStart=false 时表示开始时间 t0
 * @return std::optional<double> 待求的时间值，无效时返回-1
 */
double calculateTrainTime(bool bStart, double a, double v, double s, double knownTime)
{
    constexpr double EPSILON = 1e-9; // 浮点精度阈值
    constexpr double MAX_TIME = 1e6; // 最大允许时间(100万秒≈11.5天)

    // 验证基础约束（速度&距离>0）
    if (v <= 0 || s <= 0) {
        qWarning() << "⚠️ 速度或距离必须大于0";
        return -1;
    }

    // 处理匀速运动 (a ≈ 0)
    if (std::fabs(a) < EPSILON) {
        double deltaT = s / v;
        return bStart ?
                   knownTime - deltaT : // t0 = t1 - Δt
                   knownTime + deltaT;  // t1 = t0 + Δt
    }

    // 匀加速运动核心计算
    double discriminant = v * v - 2 * a * s;

    // 验证物理可行性
    if (discriminant < -EPSILON) {
        qWarning() << "⛔ 参数违反物理定律：判别式" << discriminant << "<0";
        return -1;
    }

    // 处理边界情况 (discriminant ≈ 0)
    discriminant = (discriminant < 0) ? 0 : discriminant;

    double sqrtD = std::sqrt(discriminant);
    double deltaT1 = (v + sqrtD) / a;
    double deltaT2 = (v - sqrtD) / a;

    // 选择有效时间解（正值且在合理范围内）
    auto validateDeltaT = [](double t) {
        return (t > EPSILON) && (t < MAX_TIME);
    };

    std::optional<double> validDeltaT;
    if (validateDeltaT(deltaT1)) validDeltaT = deltaT1;
    if (validateDeltaT(deltaT2)) {
        // 优先选择较小的时间解
        if (!validDeltaT || deltaT2 < *validDeltaT) {
            validDeltaT = deltaT2;
        }
    }

    // 计算结果
    if (!validDeltaT) {
        qWarning() << "⏱️ 无有效时间解";
        return -1;
    }

    return bStart ?
               knownTime - *validDeltaT : // 已知结束时间求开始时间
               knownTime + *validDeltaT;  // 已知开始时间求结束时间
}
/**
 * @brief 计算车辆在摄像机视野内的完整时间范围
 * @param frontWheelTime 前轮经过参考点的时间（毫秒）
 * @param rearWheelTime 后轮经过参考点的时间（毫秒）
 * @param frontSpeed 前轮经过参考点时的速度（米/秒）
 * @param rearSpeed 后轮经过参考点时的速度（米/秒）
 * @param standardDelay 单位速度时的拍照延迟，由系统设置(毫秒)
 * @return QPair<quint64, quint64> 车辆在摄像机视野内的开始和结束时间（毫秒）
 */
QPair<quint64, quint64> CDataProtocol::calculateVehicleInCameraRange(
    quint64 frontWheelTime,
    quint64 rearWheelTime,
    double frontSpeed,
    double rearSpeed,
    quint64 standardDelay)
{
    // 计算前后轮经过参考点的时间差（秒）
    double deltaT = static_cast<double>(rearWheelTime - frontWheelTime) / 1000.0;

    // 计算加速度（米/秒²）
    double acceleration = (rearSpeed - frontSpeed) / deltaT;

    qint64 startTime=frontWheelTime+ (-frontSpeed+sqrt(frontSpeed*frontSpeed+2*acceleration*standardDelay))/acceleration;
    acceleration=-acceleration;
    qint64 endTime=rearWheelTime+(-rearSpeed+sqrt(rearSpeed*rearSpeed+2*acceleration*standardDelay))/acceleration;

    return qMakePair(startTime, endTime);
}
void CarriagePictureProcessor::run() {
    // CDataProtocol::PackCarriagePictureRes(m_filePath, arrayRes, m_maxPacketSize);
    LOG(QString("Sending file: %1").arg(m_filePath), LogLevel::LOG_INFO);
    qint32 packageLen=CDataProtocol::PackCarriagePictureResAndSend(m_filePath,m_maxPacketSize,m_descriptor);

    LOG(QString("Carriage picture processing completed for descriptor %1, file: %2, packets: %3")
            .arg(m_descriptor).arg(m_filePath).arg(packageLen), LogLevel::LOG_INFO);
}
