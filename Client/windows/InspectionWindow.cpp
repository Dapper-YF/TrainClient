#include "InspectionWindow.h"
#include "../Config.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QTableWidget>
#include <QMessageBox>
#include <QHeaderView>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QTimer>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QGraphicsOpacityEffect>
#include <QDateTime>
#include <QDebug>
#include <QMouseEvent>

// =====================================================================
// 帮助函数
// =====================================================================
static QString formatReachDatetime(quint64 ts)
{
    if (ts == 0) return "未知";
    return QDateTime::fromSecsSinceEpoch(ts).toString("yyyy-MM-dd hh:mm:ss");
}

static QString formatDateOnly(quint64 ts)
{
    if (ts == 0) return "未知";
    return QDateTime::fromSecsSinceEpoch(ts).toString("yyyy-MM-dd");
}

static QString resolveImageHttpPath(const QString& backendPath)
{
    static const QString prefix = "../../mvsimages/";
    if (!backendPath.startsWith(prefix))
        return backendPath;
    QString relPath = backendPath.mid(prefix.length());
    return QStringLiteral("/trainimages/") + relPath;
}

// =====================================================================
// 异步加载图片数据到 QPixmap（callback 模式）
// =====================================================================
static void loadImageData(const QString& imagePath, const QString& apiBase,
                          const QString& token,
                          std::function<void(const QPixmap&)> callback)
{
    if (imagePath.isEmpty()) {
        callback(QPixmap());
        return;
    }

    QString httpPath = resolveImageHttpPath(imagePath);
    QString fullUrl = httpPath.startsWith("http") ? httpPath : apiBase + httpPath;

    QNetworkAccessManager* nam = new QNetworkAccessManager();
    QNetworkRequest req{ QUrl(fullUrl) };
    req.setHeader(QNetworkRequest::ContentTypeHeader, "image/jpeg");
    if (!token.isEmpty())
        req.setRawHeader("Authorization", ("Bearer " + token).toUtf8());

    QNetworkReply* reply = nam->get(req);
    QObject::connect(reply, &QNetworkReply::finished, [=]() {
        QPixmap pm;
        if (reply->error() == QNetworkReply::NoError) {
            pm.loadFromData(reply->readAll());
        }
        callback(pm);
        reply->deleteLater();
        nam->deleteLater();
    });
}

// =====================================================================
// 设置 Pixmap 到 QLabel（自动缩放适应标签尺寸）
// =====================================================================
void InspectionWindow::setPixmapOnLabel(QLabel* lbl, const QPixmap& pm)
{
    if (!lbl) return;
    if (pm.isNull()) {
        lbl->setText("无图片");
        lbl->setPixmap(QPixmap());
        return;
    }
    lbl->setPixmap(pm);
}

// =====================================================================
// 直接加载图片到 QLabel（异步）
// =====================================================================
static void loadImageToLabel(QLabel* lbl, const QString& backendPath,
                              const QString& apiBase, const QString& token)
{
    loadImageData(backendPath, apiBase, token, [lbl](const QPixmap& pm) {
        if (!lbl) return;
        if (!pm.isNull()) {
            lbl->setPixmap(pm);
            lbl->setScaledContents(true);
        } else {
            lbl->setText("无图片");
            lbl->setPixmap(QPixmap());
        }
    });
}

// =====================================================================
// 按速度索引取间隔（毫秒）
// =====================================================================
static int speedIntervalFromIndex(int idx)
{
    // 0=0.5x, 1=1x, 2=2x, 3=3x, 4=5x
    const int intervals[] = { 3000, 1500, 750, 500, 300 };
    if (idx < 0 || idx >= 5) return 1500;
    return intervals[idx];
}

// =====================================================================
// 构造函数
// =====================================================================
InspectionWindow::InspectionWindow(const QString& trainNumber, quint64 reachDatetime,
                                   const QString& direction, const QString& detectionStation,
                                   int numCarriages, int numAxles,
                                   const QString& inspectionStatus, quint64 inspectionDatetime,
                                   const QString& leftVideoPath,
                                   const QString& rightVideoPath,
                                   const QString& topVideoPath,
                                   QWidget* parent)
    : QDialog(parent)
    , m_playTimer(new QTimer(this))
    , m_autoInspectTimer(new QTimer(this))
    , m_nam(new QNetworkAccessManager(this))
    , m_token(Config::instance()->token())
    , m_apiBase(Config::instance()->apiBaseUrl())
    , m_currentTrainNumber(trainNumber)
    , m_currentReachDatetime(reachDatetime)
    , m_direction(direction)
    , m_detectionStation(detectionStation)
    , m_numCarriages(numCarriages)
    , m_numAxles(numAxles)
    , m_inspectionStatus(inspectionStatus)
    , m_inspectionDatetime(inspectionDatetime)
    , m_leftVideoPath(leftVideoPath)
    , m_rightVideoPath(rightVideoPath)
    , m_topVideoPath(topVideoPath)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setMinimumSize(1200, 700);
    resize(1400, 800);

    setupUi();
    QTimer::singleShot(200, this, [=]() {
        loadTrainList();
        loadCarriages();
    });

    connect(m_playTimer, &QTimer::timeout, this, &InspectionWindow::onPlayTimerTick);
    connect(m_autoInspectTimer, &QTimer::timeout, this, &InspectionWindow::onAutoInspectTimer);
}

InspectionWindow::~InspectionWindow()
{
    m_playTimer->stop();
    m_autoInspectTimer->stop();
}

bool InspectionWindow::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            m_dragging = true;
            m_dragPos = me->globalPos() - frameGeometry().topLeft();
            return true;
        }
    } else if (event->type() == QEvent::MouseMove && m_dragging) {
        move(static_cast<QMouseEvent*>(event)->globalPos() - m_dragPos);
        return true;
    } else if (event->type() == QEvent::MouseButtonRelease && m_dragging) {
        m_dragging = false;
        return true;
    } else if (event->type() == QEvent::MouseButtonDblClick) {
        if (isMaximized()) showNormal(); else showMaximized();
        return true;
    }
    return QDialog::eventFilter(obj, event);
}

void InspectionWindow::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    if (m_windowTitleBar) {
        m_windowTitleBar->setFixedWidth(width());
        m_windowTitleBar->raise();
    }
}

void InspectionWindow::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    if (m_windowTitleBar) {
        m_windowTitleBar->setFixedWidth(width());
    }
}

// =====================================================================
// UI 布局
// =====================================================================
void InspectionWindow::setupUi()
{
    // === 内容容器（顶部留 32px 给标题栏）===
    QWidget* contentWidget = new QWidget(this);
    contentWidget->setObjectName("contentWidget");
    QVBoxLayout* contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);
    contentLayout->addSpacing(32);

    // 主布局加入 contentLayout
    QHBoxLayout* mainLayout = new QHBoxLayout();
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    contentLayout->addLayout(mainLayout);

    // 将 contentWidget 放入 dialog 主布局
    QVBoxLayout* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(contentWidget);

    // === 标题栏：完全独立，浮在内容之上（不在任何 layout 中）===
    QWidget* titleBar = new QWidget(this);
    titleBar->setFixedHeight(32);
    titleBar->setObjectName("windowTitleBar");
    titleBar->setCursor(Qt::ArrowCursor);
    titleBar->setStyleSheet("#windowTitleBar { background-color: #092845; }");
    titleBar->setGeometry(0, 0, this->width(), 32);
    titleBar->raise();

    QHBoxLayout* tbLayout = new QHBoxLayout(titleBar);
    tbLayout->setContentsMargins(12, 0, 4, 0);
    tbLayout->setSpacing(0);

    QLabel* titleLabel = new QLabel(QString::fromUtf8("铁路货车装载状态监测系统"), titleBar);
    titleLabel->setFixedWidth(260);
    titleLabel->setStyleSheet("color: #ffffff; font-size: 13px; background: transparent;");
    tbLayout->addWidget(titleLabel);
    tbLayout->addStretch();

    // 按钮容器（固定宽度，防止被标题挤压）
    QWidget* btnContainer = new QWidget(titleBar);
    btnContainer->setFixedWidth(104);
    QHBoxLayout* btnLayout = new QHBoxLayout(btnContainer);
    btnLayout->setContentsMargins(0, 0, 0, 0);
    btnLayout->setSpacing(0);

    QPushButton* minBtn = new QPushButton(QString::fromUtf8("_"), btnContainer);
    minBtn->setFixedSize(32, 28);
    minBtn->setStyleSheet("QPushButton { background: transparent; color: #ffffff; border: none; font-size: 16px; font-weight: bold; } QPushButton:hover { background-color: rgba(255,255,255,0.15); }");
    connect(minBtn, &QPushButton::clicked, this, &InspectionWindow::showMinimized);
    btnLayout->addWidget(minBtn);

    QPushButton* maxBtn = new QPushButton(QString::fromUtf8("□"), btnContainer);
    maxBtn->setFixedSize(32, 28);
    maxBtn->setStyleSheet("QPushButton { background: transparent; color: #ffffff; border: none; font-size: 16px; font-weight: bold; } QPushButton:hover { background-color: rgba(255,255,255,0.15); }");
    connect(maxBtn, &QPushButton::clicked, this, [this, maxBtn]() {
        if (isMaximized()) { showNormal(); maxBtn->setText(QString::fromUtf8("□")); }
        else { showMaximized(); maxBtn->setText(QString::fromUtf8("❐")); }
    });
    btnLayout->addWidget(maxBtn);

    QPushButton* clsBtn = new QPushButton(QString::fromUtf8("X"), btnContainer);
    clsBtn->setFixedSize(32, 28);
    clsBtn->setStyleSheet("QPushButton { background: transparent; color: #ffffff; border: none; font-size: 18px; font-weight: bold; } QPushButton:hover { background-color: #e75d55; color: #ffffff; }");
    connect(clsBtn, &QPushButton::clicked, this, &InspectionWindow::onCloseClicked);
    btnLayout->addWidget(clsBtn);

    tbLayout->addWidget(btnContainer);

    m_windowTitleBar = titleBar;
    titleBar->installEventFilter(this);

    // ================================================================
    // 左半部分
    // ================================================================
    m_leftPanel = new QWidget(this);
    m_leftPanel->setMinimumWidth(600);
    m_leftPanel->setObjectName("leftPanel");
    m_leftPanel->setStyleSheet("#leftPanel { background-color: #1a1a2e; }");

    QVBoxLayout* leftLayout = new QVBoxLayout(m_leftPanel);
    leftLayout->setContentsMargins(10, 8, 10, 8);
    leftLayout->setSpacing(6);

    // --- 顶部：列车选择 + 基本信息（两行） ---
    QWidget* topBar = new QWidget(m_leftPanel);
    topBar->setObjectName("trainInfoBar");
    topBar->setStyleSheet(R"(
        #trainInfoBar {
            background-color: #092845;
            border-radius: 6px;
            padding: 8px 14px;
        }
    )");
    QVBoxLayout* infoLayout = new QVBoxLayout(topBar);
    infoLayout->setContentsMargins(0, 0, 0, 0);
    infoLayout->setSpacing(4);

    // 第一行：车次下拉选择框
    QHBoxLayout* comboRow = new QHBoxLayout();
    comboRow->setContentsMargins(0, 0, 0, 0);

    QLabel* trainComboLabel = new QLabel("选择车次:", topBar);
    trainComboLabel->setStyleSheet("color: #a0aec0; font-size: 12px; background: transparent;");

    m_trainCombo = new QComboBox(topBar);
    m_trainCombo->setMinimumWidth(200);
    m_trainCombo->setStyleSheet(R"(
        QComboBox {
            background-color: #1e2a3a; color: white; border: 1px solid #4a5568;
            border-radius: 4px; padding: 4px 8px; font-size: 13px;
        }
        QComboBox:hover { border-color: #718096; }
        QComboBox::drop-down { border: none; width: 20px; }
        QComboBox::down-arrow { image: none; border-left: 5px solid transparent;
            border-right: 5px solid transparent; border-top: 6px solid white; }
        QComboBox QAbstractItemView { background-color: #1e2a3a; color: white;
            selection-background-color: #1890ff; font-size: 12px; }
    )");
    connect(m_trainCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &InspectionWindow::onTrainComboChanged);

    comboRow->addWidget(trainComboLabel);
    comboRow->addWidget(m_trainCombo, 1);
    infoLayout->addLayout(comboRow);

    // 第二行：车次 + 到达 + 方向 + 检测站
    m_trainInfoLabel = new QLabel(topBar);
    m_trainInfoLabel->setStyleSheet(
        "color: #ffffff; font-size: 15px; font-weight: bold; background: transparent;");

    QString directionDisplay = m_direction;
    if (directionDisplay == "上行" || directionDisplay == "1")
        directionDisplay = "↑上行";
    else if (directionDisplay == "下行" || directionDisplay == "2" || directionDisplay == "0")
        directionDisplay = "↓下行";

    m_trainInfoLabel->setText(QString("车次: %1  |  到达: %2  |  方向: %3  |  检测站: %4")
        .arg(m_currentTrainNumber)
        .arg(formatReachDatetime(m_currentReachDatetime))
        .arg(directionDisplay)
        .arg(m_detectionStation.isEmpty() ? "-" : m_detectionStation));

    infoLayout->addWidget(m_trainInfoLabel);

    // 第三行：车厢数 + 轴数 + 检视状态 + 检视时间
    m_trainInfoSubLabel = new QLabel(topBar);
    m_trainInfoSubLabel->setStyleSheet(
        "color: #a0aec0; font-size: 13px; background: transparent;");

    QString statusColor = "#a0aec0";
    QString statusText = m_inspectionStatus;
    if (m_inspectionStatus == "已检视") {
        statusColor = "#13ce66";
    } else if (m_inspectionStatus == "未检视") {
        statusColor = "#e75d55";
    } else if (m_inspectionStatus.contains("部分") || m_inspectionStatus.contains("未完成")) {
        statusColor = "#e6a23c";
    }

    QString inspectTimeStr = m_inspectionDatetime > 0
        ? formatReachDatetime(m_inspectionDatetime) : "-";

    m_trainInfoSubLabel->setText(QString("车厢: %1节  |  轴数: %2  |  "
        "<span style='color:%3;font-weight:bold;'>检视状态: %4</span>  |  检视时间: %5")
        .arg(m_numCarriages > 0 ? QString::number(m_numCarriages) : "-")
        .arg(m_numAxles > 0 ? QString::number(m_numAxles) : "-")
        .arg(statusColor)
        .arg(statusText)
        .arg(inspectTimeStr));
    m_trainInfoSubLabel->setTextFormat(Qt::RichText);

    infoLayout->addWidget(m_trainInfoSubLabel);
    leftLayout->addWidget(topBar);

    // --- 中部：三侧视图（垂直排列，占满剩余空间） ---
    QWidget* imageArea = new QWidget(m_leftPanel);
    imageArea->setObjectName("imageArea");
    QVBoxLayout* imageLayout = new QVBoxLayout(imageArea);
    imageLayout->setContentsMargins(4, 4, 4, 4);
    imageLayout->setSpacing(6);

    auto createViewSection = [&](QLabel*& titleLabel, QWidget*& container,
                                 QLabel*& prevLabel, QLabel*& curLabel,
                                 QWidget* parent, const QString& title,
                                 int stretch) -> void
    {
        // 标题行
        QWidget* titleRow = new QWidget(parent);
        titleRow->setFixedHeight(24);
        titleRow->setStyleSheet("background-color: #092845; border-top-left-radius: 4px; border-top-right-radius: 4px;");
        QHBoxLayout* titleRowLayout = new QHBoxLayout(titleRow);
        titleRowLayout->setContentsMargins(8, 0, 8, 0);
        titleLabel = new QLabel(title, titleRow);
        titleLabel->setStyleSheet("color: #ffffff; font-size: 12px; font-weight: bold; background: transparent;");
        titleRowLayout->addWidget(titleLabel);
        titleRowLayout->addStretch();
        imageLayout->addWidget(titleRow);

        // 图片容器
        container = new QWidget(parent);
        container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        container->setMinimumHeight(100);
        container->setStyleSheet("background-color: #0d1117; border: 1px solid #30363d; border-bottom-left-radius: 4px; border-bottom-right-radius: 4px;");

        QGridLayout* grid = new QGridLayout(container);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setSpacing(0);

        // 旧图标签（outgoing，下方）
        prevLabel = new QLabel(container);
        prevLabel->setAlignment(Qt::AlignCenter);
        prevLabel->setStyleSheet("background: transparent; color: #666; font-size: 13px;");
        prevLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        prevLabel->setScaledContents(true);
        prevLabel->hide();

        // 当前图标签（incoming，上方）
        curLabel = new QLabel(container);
        curLabel->setAlignment(Qt::AlignCenter);
        curLabel->setStyleSheet("background: transparent; color: #ffffff; font-size: 13px; font-weight: bold;");
        curLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        curLabel->setScaledContents(true);
        curLabel->setText("加载中...");

        // 重叠放在 (0,0) 同一单元格
        grid->addWidget(prevLabel, 0, 0);
        grid->addWidget(curLabel, 0, 0);

        imageLayout->addWidget(container, stretch);
    };

    // 左视图
    createViewSection(m_leftTitleLabel, m_leftContainer,
                      m_prevLeftLabel, m_leftImageLabel,
                      imageArea, "左侧视图", 3);
    // 顶视图
    createViewSection(m_topTitleLabel, m_topContainer,
                      m_prevTopLabel, m_topImageLabel,
                      imageArea, "顶部视图", 3);
    // 右视图
    createViewSection(m_rightTitleLabel, m_rightContainer,
                      m_prevRightLabel, m_rightImageLabel,
                      imageArea, "右侧视图", 3);

    leftLayout->addWidget(imageArea, 1);

    // --- 底部：播放控件 ---
    QWidget* playBar = new QWidget(m_leftPanel);
    playBar->setObjectName("playBar");
    playBar->setFixedHeight(42);
    QHBoxLayout* playLayout = new QHBoxLayout(playBar);
    playLayout->setContentsMargins(8, 4, 8, 4);
    playLayout->setSpacing(4);

    m_prevBtn = new QPushButton("◀ 上一节", playBar);
    m_prevBtn->setFixedSize(70, 30);
    m_prevBtn->setStyleSheet(R"(
        QPushButton { background-color: #606266; color: white; border: none; border-radius: 4px; font-size: 11px; font-weight: bold; }
        QPushButton:hover { background-color: #787878; }
    )");
    connect(m_prevBtn, &QPushButton::clicked, this, &InspectionWindow::onPrevFrame);

    m_playStopBtn = new QPushButton("▶ 播放", playBar);
    m_playStopBtn->setFixedSize(68, 30);
    m_playStopBtn->setObjectName("playStopBtn");
    m_playStopBtn->setStyleSheet(R"(
        QPushButton { background-color: #13ce66; color: white; border: none; border-radius: 4px; font-size: 12px; font-weight: bold; }
        QPushButton:hover { background-color: #3dbd5a; }
    )");
    connect(m_playStopBtn, &QPushButton::clicked, this, &InspectionWindow::onPlayStopToggled);

    m_nextBtn = new QPushButton("下一节 ▶", playBar);
    m_nextBtn->setFixedSize(70, 30);
    m_nextBtn->setStyleSheet(R"(
        QPushButton { background-color: #606266; color: white; border: none; border-radius: 4px; font-size: 11px; font-weight: bold; }
        QPushButton:hover { background-color: #787878; }
    )");
    connect(m_nextBtn, &QPushButton::clicked, this, &InspectionWindow::onNextFrame);

    m_speedCombo = new QComboBox(playBar);
    m_speedCombo->addItems({"0.5x", "1x", "2x", "3x", "5x"});
    m_speedCombo->setCurrentIndex(1);
    m_speedCombo->setFixedSize(58, 30);
    m_speedCombo->setStyleSheet(R"(
        QComboBox {
            background-color: #606266; color: white; border: none; border-radius: 4px;
            padding: 2px 6px; font-size: 11px; font-weight: bold;
        }
        QComboBox:hover { background-color: #787878; }
        QComboBox::drop-down { border: none; width: 16px; }
        QComboBox::down-arrow { image: none; border-left: 4px solid transparent; border-right: 4px solid transparent; border-top: 5px solid white; }
        QComboBox QAbstractItemView { background-color: #909399; color: white; selection-background-color: #1890ff; font-size: 11px; }
    )");
    connect(m_speedCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &InspectionWindow::onSpeedChanged);

    playLayout->addWidget(m_prevBtn);
    playLayout->addWidget(m_playStopBtn);
    playLayout->addWidget(m_nextBtn);
    playLayout->addSpacing(8);
    playLayout->addWidget(new QLabel("速度:", playBar));
    playLayout->addWidget(m_speedCombo);

    // ▶ 视频回放按钮
    QPushButton* videoBtn = new QPushButton(QString::fromUtf8("▶ 视频回放"), playBar);
    videoBtn->setFixedSize(80, 30);
    videoBtn->setObjectName("videoPlaybackBtn");
    videoBtn->setStyleSheet(R"(
        QPushButton { background-color: #1890ff; color: white; border: none; border-radius: 4px; font-size: 11px; font-weight: bold; }
        QPushButton:hover { background-color: #40a9ff; }
    )");
    playLayout->addWidget(videoBtn);
    connect(videoBtn, &QPushButton::clicked, this, &InspectionWindow::onVideoPlaybackClicked);

    playLayout->addStretch();

    leftLayout->addWidget(playBar);

    mainLayout->addWidget(m_leftPanel, 5);

    // ================================================================
    // 右半部分
    // ================================================================
    m_rightPanel = new QWidget(this);
    m_rightPanel->setMinimumWidth(420);
    m_rightPanel->setObjectName("rightPanel");
    m_rightPanel->setStyleSheet("#rightPanel { background-color: #f5f7fa; }");

    QVBoxLayout* rightLayout = new QVBoxLayout(m_rightPanel);
    rightLayout->setContentsMargins(8, 8, 8, 8);
    rightLayout->setSpacing(6);

    // --- 标题栏 ---
    QWidget* rightTitleBar = new QWidget(m_rightPanel);
    rightTitleBar->setStyleSheet("background: transparent;");
    QHBoxLayout* titleBarLayout = new QHBoxLayout(rightTitleBar);
    titleBarLayout->setContentsMargins(0, 0, 0, 0);

    QLabel* rightTitle = new QLabel("车厢列表", rightTitleBar);
    rightTitle->setStyleSheet("font-size: 16px; font-weight: bold; color: #222;");

    m_closeBtn = new QPushButton("关闭", rightTitleBar);
    m_closeBtn->setFixedSize(72, 32);
    m_closeBtn->setStyleSheet(R"(
        QPushButton { background-color: #444; color: white; border: none; border-radius: 4px; font-size: 13px; font-weight: bold; }
        QPushButton:hover { background-color: #666; }
    )");
    connect(m_closeBtn, &QPushButton::clicked, this, &InspectionWindow::onCloseClicked);

    titleBarLayout->addWidget(rightTitle);
    titleBarLayout->addStretch();
    titleBarLayout->addWidget(m_closeBtn);
    rightLayout->addWidget(rightTitleBar);

    // --- 车厢列表 ---
    m_carriageTable = new QTableWidget(m_rightPanel);
    m_carriageTable->setObjectName("carriageTable");
    m_carriageTable->setColumnCount(8);
    m_carriageTable->setHorizontalHeaderLabels({
        "车厢号", "车型", "换算长度", "车型代码",
        "厂方时间", "前轮速度", "后轮速度", "检视结果"
    });
    m_carriageTable->horizontalHeader()->setStretchLastSection(true);
    m_carriageTable->setColumnWidth(0, 70);
    m_carriageTable->setColumnWidth(1, 60);
    m_carriageTable->setColumnWidth(2, 70);
    m_carriageTable->setColumnWidth(3, 70);
    m_carriageTable->setColumnWidth(4, 90);
    m_carriageTable->setColumnWidth(5, 70);
    m_carriageTable->setColumnWidth(6, 70);
    m_carriageTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_carriageTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_carriageTable->setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
    m_carriageTable->setStyleSheet(R"(
        QTableWidget {
            background-color: white;
            border: 1px solid #dcdfe6;
            border-radius: 4px;
            font-size: 12px;
            gridline-color: #ebeef5;
        }
        QHeaderView::section {
            background-color: #f5f7fa;
            color: #606266;
            font-weight: bold;
            font-size: 11px;
            padding: 6px 4px;
            border-bottom: 1px solid #ebeef5;
        }
        QTableWidget::item:selected {
            background-color: #e6f0ff;
            color: #303133;
        }
        QTableWidget::item { padding: 3px; }
    )");
    connect(m_carriageTable, &QTableWidget::cellClicked,
            this, &InspectionWindow::onCarriageSelected);
    rightLayout->addWidget(m_carriageTable, 1);

    // --- 底部：检视操作区 ---
    QWidget* bottomBar = new QWidget(m_rightPanel);
    bottomBar->setObjectName("bottomBar");
    bottomBar->setStyleSheet(R"(
        #bottomBar {
            background-color: white;
            border-top: 1px solid #dcdfe6;
            border-radius: 6px;
            padding: 8px;
        }
    )");
    QVBoxLayout* bottomMainLayout = new QVBoxLayout(bottomBar);
    bottomMainLayout->setContentsMargins(0, 0, 0, 0);
    bottomMainLayout->setSpacing(6);

    // 第一行：操作按钮
    QHBoxLayout* topRowLayout = new QHBoxLayout();
    topRowLayout->setContentsMargins(0, 0, 0, 0);

    // 审核按钮
    QPushButton* reviewBtn = new QPushButton("✓ 审核", bottomBar);
    reviewBtn->setFixedSize(70, 30);
    reviewBtn->setStyleSheet(R"(
        QPushButton { background-color: #1890ff; color: white; border: none; border-radius: 4px; font-size: 12px; font-weight: bold; }
        QPushButton:hover { background-color: #40a9ff; }
    )");
    connect(reviewBtn, &QPushButton::clicked, this, [=]() {
        if (m_currentRow >= 0 && m_currentRow < m_carriages.size()) {
            onCarriageResultChanged(m_currentRow, true);
        }
    });

    // 跳过按钮
    QPushButton* skipBtn = new QPushButton("跳过", bottomBar);
    skipBtn->setFixedSize(60, 30);
    skipBtn->setStyleSheet(R"(
        QPushButton { background-color: #909399; color: white; border: none; border-radius: 4px; font-size: 12px; font-weight: bold; }
        QPushButton:hover { background-color: #a6a9ad; }
    )");
    connect(skipBtn, &QPushButton::clicked, this, [=]() {
        if (m_carriages.isEmpty()) return;
        int nextRow = qMin(m_carriages.size() - 1, m_currentRow + 1);
        m_carriageTable->selectRow(nextRow);
        syncLeftToCarriage(nextRow);
        m_currentRow = nextRow;
    });

    // 自动检视按钮
    QPushButton* autoBtn = new QPushButton("▶ 自动检视", bottomBar);
    autoBtn->setObjectName("autoInspectBtn");
    autoBtn->setFixedSize(100, 30);
    autoBtn->setStyleSheet(R"(
        QPushButton { background-color: #e6a23c; color: white; border: none; border-radius: 4px; font-size: 12px; font-weight: bold; }
        QPushButton:hover { background-color: #ebb563; }
    )");
    connect(autoBtn, &QPushButton::clicked, this, &InspectionWindow::onAutoInspectClicked);

    topRowLayout->addWidget(reviewBtn);
    topRowLayout->addSpacing(4);
    topRowLayout->addWidget(skipBtn);
    topRowLayout->addSpacing(4);
    topRowLayout->addWidget(autoBtn);
    topRowLayout->addStretch();

    // 第二行：检视状态显示
    QHBoxLayout* statusRowLayout = new QHBoxLayout();
    statusRowLayout->setContentsMargins(0, 0, 0, 0);

    QLabel* statusLabel = new QLabel("检视状态:", bottomBar);
    statusLabel->setStyleSheet("color: #606266; font-size: 11px; background: transparent;");
    m_inspectStatusLabel = new QLabel("待检", bottomBar);
    m_inspectStatusLabel->setStyleSheet("color: #909399; font-size: 11px; font-weight: bold; background: transparent;");

    QLabel* passCountLabel = new QLabel("合格: ", bottomBar);
    passCountLabel->setStyleSheet("color: #606266; font-size: 11px; background: transparent;");
    m_passCountLabel = new QLabel("0", bottomBar);
    m_passCountLabel->setStyleSheet("color: #13ce66; font-size: 11px; font-weight: bold; background: transparent;");

    QLabel* failCountLabel = new QLabel("不合格: ", bottomBar);
    failCountLabel->setStyleSheet("color: #606266; font-size: 11px; background: transparent;");
    m_failCountLabel = new QLabel("0", bottomBar);
    m_failCountLabel->setStyleSheet("color: #e75d55; font-size: 11px; font-weight: bold; background: transparent;");

    QLabel* progressLabel = new QLabel("进度: ", bottomBar);
    progressLabel->setStyleSheet("color: #606266; font-size: 11px; background: transparent;");
    m_progressLabel = new QLabel("0/0", bottomBar);
    m_progressLabel->setStyleSheet("color: #1890ff; font-size: 11px; font-weight: bold; background: transparent;");

    statusRowLayout->addWidget(statusLabel);
    statusRowLayout->addWidget(m_inspectStatusLabel);
    statusRowLayout->addSpacing(16);
    statusRowLayout->addWidget(passCountLabel);
    statusRowLayout->addWidget(m_passCountLabel);
    statusRowLayout->addSpacing(12);
    statusRowLayout->addWidget(failCountLabel);
    statusRowLayout->addWidget(m_failCountLabel);
    statusRowLayout->addSpacing(12);
    statusRowLayout->addWidget(progressLabel);
    statusRowLayout->addWidget(m_progressLabel);
    statusRowLayout->addStretch();

    bottomMainLayout->addLayout(topRowLayout);
    bottomMainLayout->addLayout(statusRowLayout);
    rightLayout->addWidget(bottomBar);

    mainLayout->addWidget(m_rightPanel, 5);
}

// =====================================================================
// 获取当日列车列表（填充车次选择下拉框）
// =====================================================================
void InspectionWindow::loadTrainList()
{
    // 使用当前列车的到达时间提取日期，获取同日所有列车
    QString dateStr = formatDateOnly(m_currentReachDatetime);

    // 同一天 00:00 到 23:59
    QDateTime startDt = QDateTime::fromString(dateStr + " 00:00:00", "yyyy-MM-dd hh:mm:ss");
    QDateTime endDt = QDateTime::fromString(dateStr + " 23:59:59", "yyyy-MM-dd hh:mm:ss");
    quint64 startTs = startDt.isValid() ? startDt.toSecsSinceEpoch() : m_currentReachDatetime;
    quint64 endTs = endDt.isValid() ? endDt.toSecsSinceEpoch() : m_currentReachDatetime;

    QString url = m_apiBase + "/api/trains?startDate=" + QString::number(startTs)
                + "&endDate=" + QString::number(endTs);
    if (!m_detectionStation.isEmpty()) {
        url += "&strDetection=" + m_detectionStation;
    }

    QNetworkRequest req{ QUrl(url) };
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!m_token.isEmpty())
        req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [=]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "loadTrainList error:" << reply->errorString();
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject root = doc.object();
        if (!root["success"].toBool()) return;

        QJsonArray arr;
        if (root.contains("data") && root["data"].toObject().contains("trains")) {
            arr = root["data"].toObject()["trains"].toArray();
        } else if (root.contains("trains")) {
            arr = root["trains"].toArray();
        }

        // 阻止 onTrainComboChanged 的触发
        m_trainCombo->blockSignals(true);
        m_trainCombo->clear();

        m_trainList.clear();

        for (const QJsonValue& v : arr) {
            QJsonObject obj = v.toObject();
            TrainBriefInfo info;
            info.number = obj["strTrainNumber"].toString();
            info.reachDatetime = obj["nReachDatetime"].toVariant().toULongLong();

            QString displayName = info.number + "  "
                + QDateTime::fromSecsSinceEpoch(info.reachDatetime).toString("HH:mm:ss");

            m_trainCombo->addItem(displayName, QVariant::fromValue(m_trainList.size()));
            m_trainList.append(info);

            // 选中当前列车
            if (info.number == m_currentTrainNumber && info.reachDatetime == m_currentReachDatetime) {
                m_trainCombo->setCurrentIndex(m_trainCombo->count() - 1);
            }
        }

        m_trainCombo->blockSignals(false);
    });
}

// =====================================================================
// 车次选择下拉框切换
// =====================================================================
void InspectionWindow::onTrainComboChanged(int index)
{
    if (index < 0 || index >= m_trainList.size()) return;
    const TrainBriefInfo& selected = m_trainList[index];

    // 如果和当前列车相同，不重复加载
    if (selected.number == m_currentTrainNumber && selected.reachDatetime == m_currentReachDatetime)
        return;

    // 切换列车：停止播放和自动检视
    if (m_isPlaying) stopPlayAnimation();
    if (m_autoInspecting) {
        m_autoInspectTimer->stop();
        m_autoInspecting = false;
    }

    // 清除旧数据
    m_carriages.clear();
    m_pixmapCache.clear();
    m_playRow = 0;
    m_currentRow = 0;
    m_carriageTable->setRowCount(0);
    m_leftImageLabel->setText("加载中...");
    m_topImageLabel->setText("加载中...");
    m_rightImageLabel->setText("加载中...");

    // 更新当前列车信息（仅有限字段，全部字段需要从 API 获取）
    // 先更新基础的
    m_currentTrainNumber = selected.number;
    m_currentReachDatetime = selected.reachDatetime;

    // 更新顶部信息栏
    m_trainInfoLabel->setText(QString("车次: %1  |  到达: %2")
        .arg(m_currentTrainNumber)
        .arg(formatReachDatetime(m_currentReachDatetime)));

    // 加载新列车数据
    loadCarriages();
}

// =====================================================================
// 数据加载 — 车厢列表
// =====================================================================
void InspectionWindow::loadCarriages()
{
    QString url = m_apiBase + "/api/carriages?trainNumber=" + m_currentTrainNumber
                + "&reachDatetime=" + QString::number(m_currentReachDatetime);

    QNetworkRequest req{ QUrl(url) };
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!m_token.isEmpty())
        req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [=]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "loadCarriages error:" << reply->errorString();
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject root = doc.object();
        if (!root["success"].toBool()) return;

        QList<CarriageInfo3> list;
        QJsonArray arr = root["carriages"].toArray();
        if (arr.isEmpty())
            arr = root["data"].toObject()["carriages"].toArray();
        for (const QJsonValue& v : arr) {
            QJsonObject obj = v.toObject();
            CarriageInfo3 c;
            c.id = obj["id"].toInt();
            c.nOrder = obj["nOrder"].toInt();
            c.strWagonNumber = obj["strWagonNumber"].toString();
            c.strCarriageModel = obj["strCarriageModel"].toString();
            c.fConverredLength = obj["fConverredLength"].toDouble();
            c.strCarriageModelCode = obj["strCarriageModelCode"].toString();
            c.strFactoryAndDatetime = obj["strFactoryAndDatetime"].toString();
            c.frontWheelSpeed = obj["frontWheelSpeed"].toDouble();
            c.rearWheelSpeed = obj["rearWheelSpeed"].toDouble();
            c.leftImagePath = obj["leftImagePath"].toString();
            c.topImagePath = obj["topImagePath"].toString();
            c.rightImagePath = obj["rightImagePath"].toString();
            c.leftImagePass = obj["leftImagePass"].toInt();
            c.topImagePass = obj["topImagePass"].toInt();
            c.rightImagePass = obj["rightImagePass"].toInt();
            c.inspectorState = obj["inspectorState"].toInt();
            list.append(c);
        }

        m_carriages = list;
        populateCarriageTable();
        updateProgressLabels();

        if (!m_carriages.isEmpty()) {
            m_carriageTable->selectRow(0);
            syncLeftToCarriage(0);
        }
    });
}

// =====================================================================
// 填充车厢列表（表格）
// =====================================================================
void InspectionWindow::populateCarriageTable()
{
    m_carriageTable->setRowCount(m_carriages.size());
    for (int i = 0; i < m_carriages.size(); ++i) {
        updateCarriageRow(i, m_carriages[i]);
    }
}

// =====================================================================
// 更新单行车厢显示（含互斥单选按钮组）
// =====================================================================
void InspectionWindow::updateCarriageRow(int row, const CarriageInfo3& c)
{
    // 列0: 车厢号
    QTableWidgetItem* numItem = new QTableWidgetItem(
        c.strWagonNumber.isEmpty() ? QString::number(c.nOrder) : c.strWagonNumber);
    numItem->setForeground(QBrush(QColor("#1890ff")));
    numItem->setFont(QFont("Microsoft YaHei", 12, QFont::Bold));
    numItem->setTextAlignment(Qt::AlignCenter);
    m_carriageTable->setItem(row, 0, numItem);

    // 列1-6: 基本信息
    auto setTextItem = [&](int col, const QString& text) {
        QTableWidgetItem* item = new QTableWidgetItem(text);
        item->setTextAlignment(Qt::AlignCenter);
        item->setForeground(QBrush(QColor("#606266")));
        m_carriageTable->setItem(row, col, item);
    };
    setTextItem(1, c.strCarriageModel);
    setTextItem(2, QString::number(c.fConverredLength));
    setTextItem(3, c.strCarriageModelCode);
    setTextItem(4, c.strFactoryAndDatetime);
    setTextItem(5, QString::number(c.frontWheelSpeed));
    setTextItem(6, QString::number(c.rearWheelSpeed));

    // 列7: 检视结果 — 使用互斥单选按钮组
    int resultState = c.inspectorState;
    if (resultState < 0 || resultState > 2) resultState = 0;

    QWidget* resultWidget = new QWidget(m_carriageTable);
    resultWidget->setStyleSheet("background: transparent;");
    QHBoxLayout* resultLayout = new QHBoxLayout(resultWidget);
    resultLayout->setContentsMargins(2, 1, 2, 1);
    resultLayout->setSpacing(2);

    // 清除旧按钮组
    if (m_rowButtonGroups.contains(row)) {
        delete m_rowButtonGroups.take(row);
    }

    QButtonGroup* btnGroup = new QButtonGroup(resultWidget);
    btnGroup->setExclusive(true);

    // 合格按钮（单选，checkable）
    QPushButton* passBtn = new QPushButton("✓合格", resultWidget);
    passBtn->setFixedSize(52, 24);
    passBtn->setCheckable(true);
    passBtn->setChecked(resultState == 1);
    passBtn->setStyleSheet(resultState == 1 ? R"(
        QPushButton { background-color: #13ce66; color: white; border: 2px solid #13ce66; border-radius: 4px; font-size: 11px; font-weight: bold; }
        QPushButton:hover { background-color: #3dbd5a; }
    )" : R"(
        QPushButton { background-color: transparent; color: #13ce66; border: 1px solid #13ce66; border-radius: 4px; font-size: 11px; }
        QPushButton:hover { background-color: #13ce66; color: white; font-weight: bold; }
        QPushButton:checked { background-color: #13ce66; color: white; border: 2px solid #13ce66; font-weight: bold; }
    )");
    btnGroup->addButton(passBtn, 1);

    // 不合格按钮（单选，checkable）
    QPushButton* failBtn = new QPushButton("✗不合格", resultWidget);
    failBtn->setFixedSize(62, 24);
    failBtn->setCheckable(true);
    failBtn->setChecked(resultState == 2);
    failBtn->setStyleSheet(resultState == 2 ? R"(
        QPushButton { background-color: #e75d55; color: white; border: 2px solid #e75d55; border-radius: 4px; font-size: 11px; font-weight: bold; }
        QPushButton:hover { background-color: #e8706a; }
    )" : R"(
        QPushButton { background-color: transparent; color: #e75d55; border: 1px solid #e75d55; border-radius: 4px; font-size: 11px; }
        QPushButton:hover { background-color: #e75d55; color: white; font-weight: bold; }
        QPushButton:checked { background-color: #e75d55; color: white; border: 2px solid #e75d55; font-weight: bold; }
    )");
    btnGroup->addButton(failBtn, 2);

    // 点击检测（监听按钮组 ID 变化）
    connect(btnGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, [this, row](int id) {
        // id=1 合格, id=2 不合格
        bool passed = (id == 1);
        onCarriageResultChanged(row, passed);
    });

    m_rowButtonGroups[row] = btnGroup;

    resultLayout->addWidget(passBtn);
    resultLayout->addWidget(failBtn);
    m_carriageTable->setCellWidget(row, 7, resultWidget);
}

QString InspectionWindow::colorForCarriage(const CarriageInfo3& c) const
{
    if (c.inspectorState == 0) {
        return "#e75d55";
    } else if (c.inspectorState == 1) {
        return "#13ce66";
    } else if (c.inspectorState == 2) {
        return "#e6a23c";
    }
    return "#e75d55";
}

// =====================================================================
// 左侧图片同步
// =====================================================================
void InspectionWindow::syncLeftToCarriage(int row)
{
    if (row < 0 || row >= m_carriages.size()) return;
    m_currentRow = row;
    const CarriageInfo3& c = m_carriages[row];

    if (m_pixmapCache.contains(row) && m_pixmapCache[row].valid) {
        const TriViewPixmaps& cached = m_pixmapCache[row];
        setPixmapOnLabel(m_leftImageLabel, cached.left);
        setPixmapOnLabel(m_topImageLabel, cached.top);
        setPixmapOnLabel(m_rightImageLabel, cached.right);
    } else {
        loadImageToLabel(m_leftImageLabel, c.leftImagePath, m_apiBase, m_token);
        loadImageToLabel(m_topImageLabel, c.topImagePath, m_apiBase, m_token);
        loadImageToLabel(m_rightImageLabel, c.rightImagePath, m_apiBase, m_token);
        preloadCarriagePixmaps(row);
    }

    m_prevLeftLabel->hide();
    m_prevTopLabel->hide();
    m_prevRightLabel->hide();
    m_prevLeftLabel->setGraphicsEffect(nullptr);
    m_prevTopLabel->setGraphicsEffect(nullptr);
    m_prevRightLabel->setGraphicsEffect(nullptr);
    m_leftImageLabel->setGraphicsEffect(nullptr);
    m_topImageLabel->setGraphicsEffect(nullptr);
    m_rightImageLabel->setGraphicsEffect(nullptr);
}

// =====================================================================
// 预加载车厢图片到缓存
// =====================================================================
void InspectionWindow::preloadCarriagePixmaps(int row)
{
    if (row < 0 || row >= m_carriages.size()) return;
    if (m_pixmapCache.contains(row)) return;

    const CarriageInfo3& c = m_carriages[row];
    TriViewPixmaps& cache = m_pixmapCache[row];
    Q_UNUSED(cache)

    struct LoadState {
        int loaded = 0;
    };
    LoadState* state = new LoadState;

    auto onOneLoaded = [state]() {
        state->loaded++;
        if (state->loaded >= 3) {
            delete state;
        }
    };

    loadImageData(c.leftImagePath, m_apiBase, m_token,
        [this, row, onOneLoaded](const QPixmap& pm) {
            if (m_pixmapCache.contains(row)) {
                m_pixmapCache[row].left = pm;
                m_pixmapCache[row].valid = !pm.isNull();
            }
            onOneLoaded();
        });
    loadImageData(c.topImagePath, m_apiBase, m_token,
        [this, row, onOneLoaded](const QPixmap& pm) {
            if (m_pixmapCache.contains(row))
                m_pixmapCache[row].top = pm;
            onOneLoaded();
        });
    loadImageData(c.rightImagePath, m_apiBase, m_token,
        [this, row, onOneLoaded](const QPixmap& pm) {
            if (m_pixmapCache.contains(row))
                m_pixmapCache[row].right = pm;
            onOneLoaded();
        });
}

// =====================================================================
// 交叉淡入淡出动画
// =====================================================================
void InspectionWindow::animateCrossfadeCached(int fromRow, int toRow)
{
    if (fromRow < 0 || fromRow >= m_carriages.size()) return;
    if (toRow < 0 || toRow >= m_carriages.size()) return;

    auto copyToPrev = [](QLabel* prev, QLabel* cur) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        QPixmap pm = cur->pixmap();
#else
        const QPixmap* pmPtr = cur->pixmap();
        QPixmap pm = pmPtr ? *pmPtr : QPixmap();
#endif
        if (!pm.isNull()) {
            prev->setPixmap(pm);
            prev->setScaledContents(true);
        }
    };
    copyToPrev(m_prevLeftLabel, m_leftImageLabel);
    copyToPrev(m_prevTopLabel, m_topImageLabel);
    copyToPrev(m_prevRightLabel, m_rightImageLabel);

    const TriViewPixmaps& cache = m_pixmapCache.value(toRow);
    setPixmapOnLabel(m_leftImageLabel, cache.left);
    setPixmapOnLabel(m_topImageLabel, cache.top);
    setPixmapOnLabel(m_rightImageLabel, cache.right);

    m_prevLeftLabel->show();
    m_prevTopLabel->show();
    m_prevRightLabel->show();

    auto createFade = [](QLabel* label) -> QGraphicsOpacityEffect* {
        QGraphicsOpacityEffect* effect = new QGraphicsOpacityEffect(label);
        effect->setOpacity(1.0);
        label->setGraphicsEffect(effect);
        return effect;
    };

    QGraphicsOpacityEffect* fadeOutLeft = createFade(m_prevLeftLabel);
    QGraphicsOpacityEffect* fadeOutTop = createFade(m_prevTopLabel);
    QGraphicsOpacityEffect* fadeOutRight = createFade(m_prevRightLabel);

    QGraphicsOpacityEffect* fadeInLeft = createFade(m_leftImageLabel);
    QGraphicsOpacityEffect* fadeInTop = createFade(m_topImageLabel);
    QGraphicsOpacityEffect* fadeInRight = createFade(m_rightImageLabel);
    fadeInLeft->setOpacity(0.0);
    fadeInTop->setOpacity(0.0);
    fadeInRight->setOpacity(0.0);

    auto animOpacity = [](QGraphicsOpacityEffect* effect,
                          qreal from, qreal to, int duration) -> QPropertyAnimation*
    {
        QPropertyAnimation* anim = new QPropertyAnimation(effect, "opacity");
        anim->setDuration(duration);
        anim->setStartValue(from);
        anim->setEndValue(to);
        anim->setEasingCurve(QEasingCurve::InOutQuad);
        return anim;
    };

    QParallelAnimationGroup* group = new QParallelAnimationGroup(this);
    group->addAnimation(animOpacity(fadeOutLeft, 1.0, 0.0, 350));
    group->addAnimation(animOpacity(fadeOutTop, 1.0, 0.0, 350));
    group->addAnimation(animOpacity(fadeOutRight, 1.0, 0.0, 350));
    group->addAnimation(animOpacity(fadeInLeft, 0.0, 1.0, 350));
    group->addAnimation(animOpacity(fadeInTop, 0.0, 1.0, 350));
    group->addAnimation(animOpacity(fadeInRight, 0.0, 1.0, 350));

    group->start(QAbstractAnimation::DeleteWhenStopped);
    connect(group, &QParallelAnimationGroup::finished, this, [=]() {
        m_prevLeftLabel->hide();
        m_prevTopLabel->hide();
        m_prevRightLabel->hide();
        m_prevLeftLabel->setGraphicsEffect(nullptr);
        m_prevTopLabel->setGraphicsEffect(nullptr);
        m_prevRightLabel->setGraphicsEffect(nullptr);
        m_leftImageLabel->setGraphicsEffect(nullptr);
        m_topImageLabel->setGraphicsEffect(nullptr);
        m_rightImageLabel->setGraphicsEffect(nullptr);
    });
}

// =====================================================================
// 播放定时器
// =====================================================================
void InspectionWindow::onPlayTimerTick()
{
    if (!m_isPlaying || m_carriages.isEmpty()) return;

    int nextRow = m_playRow + 1;
    if (nextRow >= m_carriages.size()) nextRow = 0;

    int preloadRow = nextRow + 1;
    if (preloadRow >= m_carriages.size()) preloadRow = 0;
    preloadCarriagePixmaps(preloadRow);

    if (m_pixmapCache.contains(nextRow) && m_pixmapCache[nextRow].valid) {
        animateCrossfadeCached(m_playRow, nextRow);
    } else {
        preloadCarriagePixmaps(nextRow);
        syncLeftToCarriage(nextRow);
    }

    m_playRow = nextRow;
    m_carriageTable->selectRow(m_playRow);
}

void InspectionWindow::stopPlayAnimation()
{
    m_playTimer->stop();
    m_isPlaying = false;
    m_playRow = 0;
    m_playStopBtn->setText("▶ 播放");
    m_playStopBtn->setStyleSheet(R"(
        QPushButton { background-color: #13ce66; color: white; border: none; border-radius: 4px; font-size: 12px; font-weight: bold; }
        QPushButton:hover { background-color: #3dbd5a; }
    )");
    m_prevLeftLabel->hide();
    m_prevTopLabel->hide();
    m_prevRightLabel->hide();

    if (m_currentRow >= 0 && m_currentRow < m_carriages.size())
        syncLeftToCarriage(m_currentRow);
}

// =====================================================================
// 播放/停止切换
// =====================================================================
void InspectionWindow::onPlayStopToggled()
{
    if (m_carriages.isEmpty()) return;
    if (!m_isPlaying) {
        m_isPlaying = true;
        m_playRow = m_currentRow;
        m_playStopBtn->setText("■ 停止");
        m_playStopBtn->setStyleSheet(R"(
            QPushButton { background-color: #e75d55; color: white; border: none; border-radius: 4px; font-size: 12px; font-weight: bold; }
            QPushButton:hover { background-color: #e8706a; }
        )");

        int nextRow = m_playRow + 1;
        if (nextRow >= m_carriages.size()) nextRow = 0;
        preloadCarriagePixmaps(nextRow);

        int speedIdx = m_speedCombo->currentIndex();
        m_playTimer->start(speedIntervalFromIndex(speedIdx));
    } else {
        stopPlayAnimation();
    }
}

// =====================================================================
// 导航控制
// =====================================================================
void InspectionWindow::onPrevFrame()
{
    if (m_carriages.isEmpty()) return;
    int prevRow = qMax(0, m_currentRow - 1);
    m_carriageTable->selectRow(prevRow);
    syncLeftToCarriage(prevRow);
    m_currentRow = prevRow;
    m_playRow = prevRow;
}

void InspectionWindow::onNextFrame()
{
    if (m_carriages.isEmpty()) return;
    int nextRow = qMin(m_carriages.size() - 1, m_currentRow + 1);
    m_carriageTable->selectRow(nextRow);
    syncLeftToCarriage(nextRow);
    m_currentRow = nextRow;
    m_playRow = nextRow;
}

void InspectionWindow::onFirstFrame()
{
    if (m_carriages.isEmpty()) return;
    m_carriageTable->selectRow(0);
    syncLeftToCarriage(0);
    m_currentRow = 0;
    m_playRow = 0;
}

void InspectionWindow::onLastFrame()
{
    if (m_carriages.isEmpty()) return;
    int lastRow = m_carriages.size() - 1;
    m_carriageTable->selectRow(lastRow);
    syncLeftToCarriage(lastRow);
    m_currentRow = lastRow;
    m_playRow = lastRow;
}

void InspectionWindow::onSpeedChanged(int index)
{
    if (m_isPlaying) {
        m_playTimer->setInterval(speedIntervalFromIndex(index));
    }
}

// =====================================================================
// 车厢选中
// =====================================================================
void InspectionWindow::onCarriageSelected(int row, int)
{
    if (row < 0) return;
    m_currentRow = row;
    m_playRow = row;
    syncLeftToCarriage(row);
}

// =====================================================================
// 检视结果改变 — 合格(true) / 不合格(false)
// =====================================================================
void InspectionWindow::onCarriageResultChanged(int row, bool passed)
{
    if (row < 0 || row >= m_carriages.size()) return;
    updateCarriageApi(row, passed);
}

// =====================================================================
// 更新车厢检视结果
// =====================================================================
void InspectionWindow::updateCarriageApi(int row, bool passed)
{
    if (row < 0 || row >= m_carriages.size()) return;
    const CarriageInfo3& c = m_carriages[row];

    QString url = m_apiBase + "/api/carriages/inspection"
                + "?trainNumber=" + m_currentTrainNumber
                + "&carriageNumber=" + QString::number(c.nOrder)
                + "&reachDatetime=" + QString::number(m_currentReachDatetime);

    QJsonObject body;
    body["leftPassed"] = true;
    body["topPassed"] = true;
    body["rightPassed"] = true;
    if (!passed) {
        body["inspectorState"] = 2;
    }

    QNetworkRequest req{ QUrl(url) };
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!m_token.isEmpty())
        req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());

    QNetworkReply* reply = m_nam->put(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [=]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "updateCarriageApi error:" << reply->errorString();
            return;
        }
        // 本地同步
        CarriageInfo3& local = m_carriages[row];
        local.leftImagePass = 1;
        local.topImagePass = 1;
        local.rightImagePass = 1;
        local.inspectorState = passed ? 1 : 2;
        updateCarriageRow(row, local);
        updateProgressLabels();
    });
}

void InspectionWindow::updateProgressLabels()
{
    int inspected = 0, passed = 0, failed = 0;
    for (const CarriageInfo3& cc : m_carriages) {
        if (cc.inspectorState == 1) {
            inspected++;
            passed++;
        } else if (cc.inspectorState == 2) {
            inspected++;
            failed++;
        }
    }
    if (m_passCountLabel) m_passCountLabel->setText(QString::number(passed));
    if (m_failCountLabel) m_failCountLabel->setText(QString::number(failed));
    if (m_progressLabel) m_progressLabel->setText(QString::number(inspected) + "/"
                                                   + QString::number(m_carriages.size()));
    if (m_inspectStatusLabel) {
        if (inspected == 0) {
            m_inspectStatusLabel->setText("待检");
            m_inspectStatusLabel->setStyleSheet("color: #909399; font-size: 11px; font-weight: bold; background: transparent;");
        } else if (inspected == m_carriages.size()) {
            m_inspectStatusLabel->setText("全部完成");
            m_inspectStatusLabel->setStyleSheet("color: #13ce66; font-size: 11px; font-weight: bold; background: transparent;");
        } else {
            m_inspectStatusLabel->setText("检视中");
            m_inspectStatusLabel->setStyleSheet("color: #1890ff; font-size: 11px; font-weight: bold; background: transparent;");
        }
    }
}

// =====================================================================
// 自动检视
// =====================================================================
void InspectionWindow::onAutoInspectClicked()
{
    if (m_carriages.isEmpty()) return;

    QPushButton* autoBtn = findChild<QPushButton*>("autoInspectBtn");
    if (!autoBtn) return;

    if (m_autoInspecting) {
        m_autoInspectTimer->stop();
        m_autoInspecting = false;
        stopPlayAnimation();
        autoBtn->setText("▶ 自动检视");
        autoBtn->setStyleSheet(R"(
            QPushButton { background-color: #e6a23c; color: white; border: none; border-radius: 4px; font-size: 12px; font-weight: bold; }
            QPushButton:hover { background-color: #ebb563; }
        )");
        return;
    }

    m_autoInspecting = true;
    m_autoInspectPassCount = 0;
    m_autoInspectFailCount = 0;
    m_currentRow = 0;
    m_playRow = 0;

    m_carriageTable->selectRow(0);
    syncLeftToCarriage(0);
    m_isPlaying = true;
    m_playStopBtn->setText("■ 停止");
    m_playStopBtn->setStyleSheet(R"(
        QPushButton { background-color: #e75d55; color: white; border: none; border-radius: 4px; font-size: 12px; font-weight: bold; }
        QPushButton:hover { background-color: #e8706a; }
    )");

    int speedIdx = m_speedCombo->currentIndex();
    m_playTimer->start(speedIntervalFromIndex(speedIdx));
    m_autoInspectTimer->start(1000);

    autoBtn->setText("停止检视");
    autoBtn->setStyleSheet(R"(
        QPushButton { background-color: #e75d55; color: white; border: none; border-radius: 4px; font-size: 12px; font-weight: bold; }
        QPushButton:hover { background-color: #e8706a; }
    )");
}

void InspectionWindow::onAutoInspectTimer()
{
    if (!m_autoInspecting || m_carriages.isEmpty()) return;

    int row = m_playRow;

    const CarriageInfo3& c = m_carriages[row];
    if (c.inspectorState == 0) {
        // 自动检视默认标记为合格
        updateCarriageApi(row, true);
        m_autoInspectPassCount++;
    }
}

// =====================================================================
// 关闭
// =====================================================================
void InspectionWindow::onCloseClicked()
{
    stopPlayAnimation();
    if (m_autoInspecting) {
        m_autoInspectTimer->stop();
        m_autoInspecting = false;
    }

    int passCount = 0, failCount = 0;
    for (const CarriageInfo3& c : m_carriages) {
        if (c.inspectorState == 1) passCount++;
        else if (c.inspectorState == 2) failCount++;
    }
    emit inspectionCompleted(m_currentTrainNumber, passCount, failCount);
    accept();
}

// =====================================================================
// 视频回放
// =====================================================================
void InspectionWindow::onVideoPlaybackClicked()
{
    // 使用 reachDatetime 动态构造视频 URL（不走数据库视频路径）
    QString dateStr = QDateTime::fromSecsSinceEpoch(m_currentReachDatetime).toString("yyyy-MM-dd");
    QString base = m_apiBase;  // e.g. "http://localhost:8080"
    QString leftUrl  = base + "/trainvideos/" + dateStr + "/left.mp4";
    QString topUrl   = base + "/trainvideos/" + dateStr + "/top.mp4";
    QString rightUrl = base + "/trainvideos/" + dateStr + "/right.mp4";

    static VideoPlaybackDialog* dialog = nullptr;
    if (!dialog) {
        dialog = new VideoPlaybackDialog(this);
    }
    dialog->loadVideos(leftUrl, topUrl, rightUrl);
    dialog->show();
    dialog->activateWindow();
}
