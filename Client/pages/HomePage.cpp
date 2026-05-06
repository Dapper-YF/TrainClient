#include "HomePage.h"
#include "../network/TrainApi.h"
#include "../Config.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QComboBox>
#include <QFile>
#include <QTextStream>
#include <QFileDevice>
#include <QTableWidget>
#include <QHeaderView>
#include <QDate>
#include <QDateTime>
#include <QTimer>
#include <QStyle>

// ============================================================
// DateSelector 实现
// ============================================================
DateSelector::DateSelector(QWidget* parent)
    : QWidget(parent)
    , m_yearBox(new QComboBox(this))
    , m_monthBox(new QComboBox(this))
    , m_dayBox(new QComboBox(this))
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    m_yearBox->setObjectName("dateYearBox");
    m_monthBox->setObjectName("dateMonthBox");
    m_dayBox->setObjectName("dateDayBox");

    populateYearItems();
    populateMonthItems();
    populateDayItems();

    // 默认今天
    setDate(QDate::currentDate());

    layout->addWidget(m_yearBox);
    layout->addWidget(new QLabel("年", this));
    layout->addWidget(m_monthBox);
    layout->addWidget(new QLabel("月", this));
    layout->addWidget(m_dayBox);
    layout->addWidget(new QLabel("日", this));

    connect(m_yearBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DateSelector::onYearChanged);
    connect(m_monthBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DateSelector::onMonthChanged);
    connect(m_dayBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DateSelector::onDayChanged);

    setStyleSheet(R"(
        #dateYearBox, #dateMonthBox, #dateDayBox {
            border: 1px solid #dcdfe6;
            border-radius: 4px;
            padding: 4px 8px;
            font-size: 13px;
            color: #303133;
            background: #ffffff;
            min-width: 60px;
        }
        #dateYearBox { min-width: 80px; }
    )");
}

void DateSelector::populateYearItems()
{
    m_yearBox->clear();
    QDate today = QDate::currentDate();
    int minYear = 2020;
    int maxYear = today.year();
    for (int y = minYear; y <= maxYear; ++y)
        m_yearBox->addItem(QString::number(y), y);
}

void DateSelector::populateMonthItems()
{
    m_monthBox->clear();
    for (int m = 1; m <= 12; ++m)
        m_monthBox->addItem(QString::number(m).rightJustified(2, '0'), m);
}

void DateSelector::populateDayItems()
{
    int year = m_yearBox->currentData().toInt();
    int month = m_monthBox->currentData().toInt();
    int daysInMonth = QDate(year, month, 1).daysInMonth();

    m_dayBox->clear();
    for (int d = 1; d <= daysInMonth; ++d)
        m_dayBox->addItem(QString::number(d).rightJustified(2, '0'), d);
}

void DateSelector::refreshDayItems()
{
    int year = m_yearBox->currentData().toInt();
    int month = m_monthBox->currentData().toInt();
    int daysInMonth = QDate(year, month, 1).daysInMonth();

    m_updatingDays = true;
    m_dayBox->clear();
    for (int d = 1; d <= daysInMonth; ++d)
        m_dayBox->addItem(QString::number(d).rightJustified(2, '0'), d);

    // 保持在有效范围内
    int curDay = m_current.day();
    int di = m_dayBox->findData(qMin(curDay, daysInMonth));
    if (di >= 0) m_dayBox->setCurrentIndex(di);
    m_updatingDays = false;
}

void DateSelector::setDate(const QDate& date)
{
    if (!date.isValid()) return;
    m_current = date;
    m_currentDate = date.toString("yyyy-MM-dd");

    int yi = m_yearBox->findData(date.year());
    if (yi >= 0) m_yearBox->setCurrentIndex(yi);

    int mi = m_monthBox->findData(date.month());
    if (mi >= 0) m_monthBox->setCurrentIndex(mi);

    populateDayItems();
    int di = m_dayBox->findData(date.day());
    if (di >= 0) m_dayBox->setCurrentIndex(di);
}

QString DateSelector::makeDateStr() const
{
    return m_current.toString("yyyy-MM-dd");
}

void DateSelector::onYearChanged(int)
{
    int year = m_yearBox->currentData().toInt();
    int month = m_monthBox->currentData().toInt();
    if (!m_current.isValid() || m_current.year() != year || m_current.month() != month) {
        m_current = QDate(year, month, 1);
    }
    m_currentDate = makeDateStr();
    refreshDayItems();
    emit dateChanged(m_currentDate);
}

void DateSelector::onMonthChanged(int)
{
    int year = m_yearBox->currentData().toInt();
    int month = m_monthBox->currentData().toInt();
    if (!m_current.isValid() || m_current.year() != year || m_current.month() != month) {
        m_current = QDate(year, month, 1);
    }
    m_currentDate = makeDateStr();
    refreshDayItems();
    emit dateChanged(m_currentDate);
}

void DateSelector::onDayChanged(int)
{
    if (m_updatingDays) return;
    int day = m_dayBox->currentData().toInt();
    if (day > 0) {
        m_current = QDate(m_current.year(), m_current.month(), day);
        m_currentDate = makeDateStr();
        emit dateChanged(m_currentDate);
    }
}

// ============================================================
// HomePage 实现
// ============================================================
HomePage::HomePage(QWidget* parent)
    : QWidget(parent)
    , m_api(new TrainApiClient(this))
{
    setupUi();

    connect(m_api, &TrainApiClient::trainsReady,
            this, &HomePage::onApiTrainsReady);
    connect(m_api, &TrainApiClient::error,
            this, &HomePage::onApiError);
    connect(m_api, &TrainApiClient::statsReady,
            this, &HomePage::onStatsReady);

    // 默认查询当天
    QTimer::singleShot(100, this, &HomePage::onQueryClicked);
}

HomePage::~HomePage() {}

void HomePage::setupUi()
{
    QVBoxLayout* main = new QVBoxLayout(this);
    main->setContentsMargins(16, 16, 16, 16);
    main->setSpacing(12);

    // ===== 统计卡片行 =====
    QHBoxLayout* cardRow = new QHBoxLayout();
    cardRow->setSpacing(12);

    struct CardDef { const char* title; QLabel*& valueLbl; };
    CardDef cards[] = {
        {"今日过车", m_todayTrainCount},
        {"待检车辆", m_pendingCount},
        {"今日告警", m_alarmCount},
    };

    for (auto& c : cards) {
        QWidget* card = new QWidget(this);
        card->setObjectName("statCard");
        QVBoxLayout* vl = new QVBoxLayout(card);
        vl->setContentsMargins(16, 12, 16, 12);
        QLabel* title = new QLabel(c.title, card);
        title->setObjectName("cardTitle");
        c.valueLbl = new QLabel("0", card);
        c.valueLbl->setObjectName("cardValue");
        vl->addWidget(title);
        vl->addWidget(c.valueLbl);
        cardRow->addWidget(card, 1);
    }

    cardRow->addStretch();
    main->addLayout(cardRow);

    // ===== 日期查询栏 =====
    QWidget* queryBar = new QWidget(this);
    queryBar->setObjectName("queryBar");
    QHBoxLayout* queryLayout = new QHBoxLayout(queryBar);
    queryLayout->setContentsMargins(12, 10, 12, 10);
    queryLayout->setSpacing(12);

    m_singleDateRadio = new QRadioButton("单日期查询", queryBar);
    m_singleDateRadio->setObjectName("modeRadio");
    m_singleDateRadio->setChecked(true);
    queryLayout->addWidget(m_singleDateRadio);

    m_rangeRadio = new QRadioButton("区间查询", queryBar);
    m_rangeRadio->setObjectName("modeRadio");
    queryLayout->addWidget(m_rangeRadio);

    queryLayout->addSpacing(10);

    // 起始日期（单日期和区间都显示）
    QLabel* startLbl = new QLabel("日期:", queryBar);
    queryLayout->addWidget(startLbl);
    m_startDateSelector = new DateSelector(queryBar);
    queryLayout->addWidget(m_startDateSelector);

    // 区间模式：截止日期（初始隐藏）
    m_rangePanel = new QWidget(queryBar);
    m_rangePanel->setVisible(false);
    QHBoxLayout* rangeLayout = new QHBoxLayout(m_rangePanel);
    rangeLayout->setContentsMargins(0, 0, 0, 0);
    rangeLayout->setSpacing(6);
    QLabel* toLbl = new QLabel("至", m_rangePanel);
    rangeLayout->addWidget(toLbl);
    m_endDateSelector = new DateSelector(m_rangePanel);
    m_endDateSelector->setDate(QDate::currentDate());
    rangeLayout->addWidget(m_endDateSelector);
    queryLayout->addWidget(m_rangePanel);

    // 查询按钮
    m_queryBtn = new QPushButton("查询", queryBar);
    m_queryBtn->setObjectName("queryBtn");
    m_queryBtn->setFixedWidth(80);
    connect(m_queryBtn, &QPushButton::clicked, this, &HomePage::onQueryClicked);
    queryLayout->addWidget(m_queryBtn);

    queryLayout->addStretch();
    main->addWidget(queryBar);

    // ===== 状态筛选栏 =====
    m_filterBar = new QWidget(this);
    m_filterBar->setObjectName("filterBar");
    QHBoxLayout* filterLayout = new QHBoxLayout(m_filterBar);
    filterLayout->setContentsMargins(12, 6, 12, 6);
    filterLayout->setSpacing(8);
    filterLayout->addWidget(new QLabel("状态筛选:", m_filterBar));

    QStringList filterLabels = {"全部", "已检视", "未检视", "部分检视"};
    for (int i = 0; i < filterLabels.size(); ++i) {
        QPushButton* btn = new QPushButton(filterLabels[i], m_filterBar);
        btn->setObjectName("filterBtn");
        btn->setCheckable(true);
        btn->setChecked(i == 0);
        btn->setFocusPolicy(Qt::NoFocus);
        connect(btn, &QPushButton::clicked, this, [=]() { onFilterChanged(i); });
        filterLayout->addWidget(btn);
        m_filterBtns.append(btn);
    }
    filterLayout->addStretch();
    main->addWidget(m_filterBar);

    // 区间切换
    connect(m_rangeRadio, &QRadioButton::toggled,
            this, &HomePage::onRangeModeToggled);

    // 选择日期自动查询（日期改变时）
    connect(m_startDateSelector, &DateSelector::dateChanged,
            this, [=](const QString& date) { queryTrains(); loadStats(date); });
    connect(m_endDateSelector, &DateSelector::dateChanged,
            this, [=](const QString& date) { queryTrains(); loadStats(date); });

    // ===== 列车列表 =====
    QWidget* tablePanel = new QWidget(this);
    tablePanel->setObjectName("tablePanel");
    QVBoxLayout* tableLayout = new QVBoxLayout(tablePanel);
    tableLayout->setContentsMargins(12, 12, 12, 12);
    tableLayout->setSpacing(8);

    m_resultLabel = new QLabel("查询结果: 共 0 条记录", tablePanel);
    m_resultLabel->setObjectName("resultLabel");
    tableLayout->addWidget(m_resultLabel);

    m_trainTable = new QTableWidget(tablePanel);
    m_trainTable->setObjectName("trainTable");
    m_trainTable->setColumnCount(8);
    m_trainTable->setHorizontalHeaderLabels({
        "列车号", "方向", "车厢数", "轴数", "检车状态", "检视进度", "过车时间", "操作"
    });
    m_trainTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_trainTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_trainTable->setAlternatingRowColors(true);
    m_trainTable->horizontalHeader()->setStretchLastSection(true);
    m_trainTable->setColumnWidth(0, 100);
    m_trainTable->setColumnWidth(1, 60);
    m_trainTable->setColumnWidth(2, 70);
    m_trainTable->setColumnWidth(3, 60);
    m_trainTable->setColumnWidth(4, 90);
    m_trainTable->setColumnWidth(5, 100);
    m_trainTable->setColumnWidth(6, 160);
    m_trainTable->setColumnWidth(7, 70);
    // 单击选行，双击跳转（由 onTrainDoubleClicked 处理）
    connect(m_trainTable, &QTableWidget::cellDoubleClicked,
            this, &HomePage::onTrainDoubleClicked);
    tableLayout->addWidget(m_trainTable);

    main->addWidget(tablePanel, 1);

    setLayout(main);

    setStyleSheet(R"(
        #statCard {
            background-color: #ffffff;
            border-radius: 4px;
            border: 1px solid #dcdfe6;
        }
        #cardTitle {
            color: #909399;
            font-size: 13px;
        }
        #cardValue {
            color: #303133;
            font-size: 28px;
            font-weight: bold;
        }
        #queryBar {
            background-color: #ffffff;
            border-radius: 4px;
            border: 1px solid #dcdfe6;
        }
        #modeRadio {
            font-size: 13px;
            color: #606266;
        }
        #queryBtn {
            background-color: #1890ff;
            color: #ffffff;
            border: none;
            border-radius: 4px;
            font-size: 13px;
            padding: 6px 16px;
        }
        #queryBtn:hover {
            background-color: #40a9ff;
        }
        #filterBar {
            background-color: #f5f7fa;
            border-radius: 4px;
            border: 1px solid #dcdfe6;
        }
        #filterBtn {
            background-color: #ffffff;
            color: #606266;
            border: 1px solid #dcdfe6;
            border-radius: 4px;
            font-size: 12px;
            padding: 4px 14px;
        }
        #filterBtn:hover {
            border-color: #1890ff;
            color: #1890ff;
        }
        #filterBtn:checked {
            background-color: #1890ff;
            color: #ffffff;
            border-color: #1890ff;
        }
        #tablePanel {
            background-color: #ffffff;
            border-radius: 4px;
            border: 1px solid #dcdfe6;
        }
        #resultLabel {
            font-size: 13px;
            color: #606266;
        }
        #trainTable {
            border: none;
            font-size: 13px;
            gridline-color: #ebeef5;
        }
        #trainTable::item {
            padding: 6px 4px;
        }
        #trainTable::item:selected {
            background-color: #e6f0ff;
            color: #303133;
        }
        QHeaderView::section {
            background-color: #f5f7fa;
            color: #606266;
            font-size: 13px;
            font-weight: bold;
            padding: 8px 4px;
            border-bottom: 1px solid #ebeef5;
        }
    )");
}

void HomePage::loadStats(const QString& date)
{
    // 从API获取统计数据
    m_api->getStats(date);
}

void HomePage::onStatsReady(int todayCount, int pendingCount, int alarmCount)
{
    // 调试日志
    QFile logFile("E:/Study/AI_Workspace/TrainClient/Client/logs/api_debug.log");
    logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    QTextStream ts(&logFile);
    ts << "[onStatsReady] Received: todayCount=" << todayCount << ", pendingCount=" << pendingCount << ", alarmCount=" << alarmCount << "\n";
    logFile.close();

    m_todayTrainCount->setText(QString::number(todayCount));
    m_pendingCount->setText(QString::number(pendingCount));
    m_alarmCount->setText(QString::number(alarmCount));
}

void HomePage::queryTrains()
{
    if (m_singleDateRadio->isChecked()) {
        QString date = m_startDateSelector->currentDate();
        m_api->getTrainsByDate(date);
    } else {
        QString startDate = m_startDateSelector->currentDate();
        QString endDate = m_endDateSelector->currentDate();

        QDate start = QDate::fromString(startDate, "yyyy-MM-dd");
        QDate end = QDate::fromString(endDate, "yyyy-MM-dd");
        quint64 startTs = QDateTime(start, QTime(0,0,0)).toMSecsSinceEpoch() / 1000;
        quint64 endTs = QDateTime(end, QTime(23,59,59)).toMSecsSinceEpoch() / 1000;

        m_api->getTrainsByRange(startTs, endTs);
    }
}

void HomePage::onRangeModeToggled(bool checked)
{
    m_rangePanel->setVisible(checked);
    if (!checked) {
        // 切换回单日期模式，立即查询
        queryTrains();
    }
}

void HomePage::onQueryClicked()
{
    QString date = m_startDateSelector->currentDate();
    queryTrains();
    loadStats(date);
}

void HomePage::onApiTrainsReady(const QList<TrainInfo2>& trains)
{
    // 调试日志
    QFile logFile("E:/Study/AI_Workspace/TrainClient/Client/logs/api_debug.log");
    logFile.open(QIODevice::Append | QIODevice::Text);
    QTextStream ts(&logFile);
    ts << "[onApiTrainsReady] Received trains count: " << trains.count() << "\n";
    logFile.close();

    m_allTrains = trains;
    applyFilter();
}

void HomePage::onApiError(const QString& err)
{
    m_resultLabel->setText(QString("查询失败: %1").arg(err));
    if (m_trainTable->rowCount() == 0) {
        m_trainTable->insertRow(0);
        m_trainTable->setItem(0, 0, new QTableWidgetItem("（无数据，请确认后端服务已启动）"));
        m_trainTable->item(0, 0)->setForeground(QBrush(QColor("#c0c4cc")));
        m_trainTable->setSpan(0, 0, 1, 8);
    }
}

// ============================================================
// 根据当前筛选条件填充表格
// ============================================================
void HomePage::applyFilter()
{
    m_trainTable->setRowCount(0);

    for (const TrainInfo2& t : m_allTrains) {
        int inspected = t.inspectionPassNumber + t.inspectionDispassNumber;
        int total = t.numberOfCarriage;

        // 根据检视进度计算状态（优先使用计算值，而非 DB 原始字符串）
        QString derivedStatus;
        if (total > 0 && inspected == total) {
            derivedStatus = "已检视";
        } else if (inspected == 0) {
            derivedStatus = "未检视";
        } else {
            derivedStatus = "部分检视";
        }

        // 筛选逻辑（按派生状态）
        if (m_currentFilter == 1) {  // 已检视
            if (derivedStatus != "已检视")
                continue;
        } else if (m_currentFilter == 2) {  // 未检视
            if (derivedStatus != "未检视")
                continue;
        } else if (m_currentFilter == 3) {  // 部分检视
            if (derivedStatus != "部分检视")
                continue;
        }

        int row = m_trainTable->rowCount();
        m_trainTable->insertRow(row);

        QTableWidgetItem* numItem = new QTableWidgetItem(t.trainNumber);
        numItem->setForeground(QBrush(QColor("#1890ff")));
        numItem->setFont(QFont("Microsoft YaHei", 13, QFont::Bold));
        m_trainTable->setItem(row, 0, numItem);

        m_trainTable->setItem(row, 1, new QTableWidgetItem(t.direction));
        m_trainTable->setItem(row, 2, new QTableWidgetItem(QString::number(t.numberOfCarriage)));
        m_trainTable->setItem(row, 3, new QTableWidgetItem(
            t.numberOfAxles > 0 ? QString::number(t.numberOfAxles) : "-"));

        // 检车状态：绿色=已检视，红色=未检视，黄色=部分检视
        QTableWidgetItem* statusItem = new QTableWidgetItem(derivedStatus);
        if (derivedStatus == "已检视") {
            statusItem->setForeground(QBrush(QColor("#13ce66")));
        } else if (derivedStatus == "未检视") {
            statusItem->setForeground(QBrush(QColor("#e75d55")));
        } else {
            statusItem->setForeground(QBrush(QColor("#e6a23c")));
        }
        m_trainTable->setItem(row, 4, statusItem);

        // 检视进度
        QString progressText = total > 0
            ? QString("已检 %1/%2").arg(inspected).arg(total)
            : "-";
        QTableWidgetItem* progressItem = new QTableWidgetItem(progressText);
        if (inspected == total && total > 0) {
            progressItem->setForeground(QBrush(QColor("#67c23a")));
        } else if (inspected > 0) {
            progressItem->setForeground(QBrush(QColor("#e6a23c")));
        } else {
            progressItem->setForeground(QBrush(QColor("#909399")));
        }
        m_trainTable->setItem(row, 5, progressItem);

        QString timeStr;
        if (t.reachDatetime > 0) {
            QDateTime dt = QDateTime::fromMSecsSinceEpoch(t.reachDatetime * 1000);
            timeStr = dt.toString("yyyy-MM-dd hh:mm:ss");
        } else {
            timeStr = "-";
        }
        m_trainTable->setItem(row, 6, new QTableWidgetItem(timeStr));
        m_trainTable->item(row, 0)->setData(Qt::UserRole, t.reachDatetime);

        // 操作列：检视按钮
        addInspectButton(row, t);
    }

    int totalCount = m_allTrains.size();
    int filteredCount = m_trainTable->rowCount();
    QString label = (m_currentFilter == 0)
        ? QString("查询结果: 共 %1 条记录").arg(totalCount)
        : QString("查询结果: 筛选 %1/%2 条记录").arg(filteredCount).arg(totalCount);
    m_resultLabel->setText(label);
}

void HomePage::addInspectButton(int row, const TrainInfo2& train)
{
    QWidget* widget = new QWidget(m_trainTable);
    QHBoxLayout* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(4);

    QPushButton* btn = new QPushButton("检视", widget);
    btn->setStyleSheet(R"(
        QPushButton {
            background-color: #1890ff;
            color: white;
            border: none;
            border-radius: 3px;
            padding: 3px 10px;
            font-size: 12px;
        }
        QPushButton:hover {
            background-color: #40a9ff;
        }
    )"
    );
    connect(btn, &QPushButton::clicked, this, [=]() {
        emit inspectTrain(train.trainNumber, train.reachDatetime,
                          train.direction, train.strDetection,
                          train.numberOfCarriage, train.numberOfAxles,
                          train.inspectionStatus, train.inspectionDatetime,
                          train.leftImagePath, train.rightImagePath, train.topImagePath);
    });
    layout->addWidget(btn);
    layout->addStretch();
    m_trainTable->setCellWidget(row, 7, widget);
}

void HomePage::onFilterChanged(int filter)
{
    m_currentFilter = filter;
    // 更新按钮选中状态
    for (int i = 0; i < m_filterBtns.size(); ++i)
        m_filterBtns[i]->setChecked(i == filter);
    applyFilter();
}

void HomePage::onTrainDoubleClicked(int row, int)
{
    if (row < 0) return;
    if (!m_trainTable->item(row, 0)) return;

    QString trainNumber = m_trainTable->item(row, 0)->text();
    quint64 reachDatetime = m_trainTable->item(row, 0)->data(Qt::UserRole).toULongLong();

    // 在完整列表中查找对应列车（因过滤可能导致行索引不匹配）
    for (const TrainInfo2& t : m_allTrains) {
        if (t.trainNumber == trainNumber && t.reachDatetime == reachDatetime) {
            emit inspectTrain(t.trainNumber, t.reachDatetime,
                              t.direction, t.strDetection,
                              t.numberOfCarriage, t.numberOfAxles,
                              t.inspectionStatus, t.inspectionDatetime,
                              t.leftImagePath, t.rightImagePath, t.topImagePath);
            return;
        }
    }
}
