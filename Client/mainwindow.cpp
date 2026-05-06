#include "MainWindow.h"
#include "pages/LoginPage.h"
#include "pages/HomePage.h"
#include "dialogs/ProfileDialog.h"
#include "windows/InspectionWindow.h"
#include "Config.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QMouseEvent>
#include <QDateTime>
#include <QDebug>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUi();
    applyStyle();
    showLoginPage();

    // 状态栏时钟
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &MainWindow::updateStatusBar);
    m_timer->start(1000);
    updateStatusBar();
}

MainWindow::~MainWindow() {}

// ============================================================
// 整体布局：登录页 vs 主框架（StackedWidget）
// ============================================================
void MainWindow::setupUi()
{
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    resize(1400, 800);
    setMinimumSize(1200, 700);

    QWidget* root = new QWidget(this);
    setCentralWidget(root);
    QVBoxLayout* rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // === 自定义标题栏 (32px) ===
    QWidget* titleBar = new QWidget(root);
    titleBar->setFixedHeight(32);
    titleBar->setObjectName("windowTitleBar");
    titleBar->setCursor(Qt::ArrowCursor);
    m_windowTitleBar = titleBar;

    QHBoxLayout* tbLayout = new QHBoxLayout(titleBar);
    tbLayout->setContentsMargins(12, 0, 4, 0);
    tbLayout->setSpacing(0);

    QLabel* titleLabel = new QLabel(QString::fromUtf8("铁路货车装载状态监测系统"), titleBar);
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
    minBtn->setObjectName("titleMinBtn");
    connect(minBtn, &QPushButton::clicked, this, &MainWindow::showMinimized);
    btnLayout->addWidget(minBtn);

    QPushButton* maxBtn = new QPushButton(QString::fromUtf8("□"), btnContainer);
    maxBtn->setFixedSize(32, 28);
    maxBtn->setObjectName("titleMaxBtn");
    connect(maxBtn, &QPushButton::clicked, this, [this, maxBtn]() {
        if (isMaximized()) { showNormal(); maxBtn->setText(QString::fromUtf8("□")); }
        else { showMaximized(); maxBtn->setText(QString::fromUtf8("❐")); }
    });
    btnLayout->addWidget(maxBtn);

    QPushButton* clsBtn = new QPushButton(QString::fromUtf8("X"), btnContainer);
    clsBtn->setFixedSize(32, 28);
    clsBtn->setObjectName("titleCloseBtn");
    connect(clsBtn, &QPushButton::clicked, this, &MainWindow::close);
    btnLayout->addWidget(clsBtn);

    tbLayout->addWidget(btnContainer);

    titleBar->installEventFilter(this);
    rootLayout->addWidget(titleBar);

    // 主 Stack（登录页 + 主框架）
    m_stackedWidget = new QStackedWidget(root);
    rootLayout->addWidget(m_stackedWidget);

    // 1. 登录页
    m_loginPage = new LoginPage(this);
    connect(m_loginPage, &LoginPage::loginSuccess, this, &MainWindow::showMainFrame);
    m_stackedWidget->addWidget(m_loginPage);

    // 2. 主框架
    m_mainFrame = new QWidget(this);
    m_stackedWidget->addWidget(m_mainFrame);

    // 主框架内部布局
    QHBoxLayout* mainLayout = new QHBoxLayout(m_mainFrame);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    m_mainFrame->setLayout(mainLayout);

    // 右侧垂直布局（顶部栏 + 页面栈 + 状态栏）
    QVBoxLayout* rightLayout = new QVBoxLayout();
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);
    mainLayout->addLayout(rightLayout);

    setupTopBar();
    rightLayout->addWidget(m_topBar);

    setupStackedWidget();
    rightLayout->addWidget(m_pageStack);

    setupStatusBar();
    rightLayout->addWidget(m_statusBar);

    rightLayout->setStretch(1, 1); // 页面栈占满剩余空间
}

// ============================================================
// 顶部栏
// ============================================================
void MainWindow::setupTopBar()
{
    m_topBar = new QWidget(this);
    m_topBar->setFixedHeight(50);
    m_topBar->setObjectName("topbar");

    QHBoxLayout* layout = new QHBoxLayout(m_topBar);
    layout->setContentsMargins(16, 0, 16, 0);

    // 当前位置
    m_locationLabel = new QLabel("当前位置: 首页", m_topBar);
    m_locationLabel->setObjectName("locationLabel");
    layout->addWidget(m_locationLabel);

    layout->addStretch();

    // 用户信息（点击弹出个人账户弹窗）+ 退出
    m_userLabel = new QLabel(m_topBar);
    m_userLabel->setObjectName("userLabel");
    m_userLabel->setText("👤 用户: " + Config::instance()->userDisplayName());
    m_userLabel->setCursor(Qt::PointingHandCursor);
    m_userLabel->installEventFilter(this);
    layout->addWidget(m_userLabel);

    QPushButton* logoutBtn = new QPushButton("退出", m_topBar);
    logoutBtn->setObjectName("logoutBtn");
    logoutBtn->setFixedWidth(60);
    connect(logoutBtn, &QPushButton::clicked, this, &MainWindow::onLogout);
    layout->addWidget(logoutBtn);
}

// ============================================================
// 状态栏
// ============================================================
void MainWindow::setupStatusBar()
{
    m_statusBar = new QWidget(this);
    m_statusBar->setFixedHeight(30);
    m_statusBar->setObjectName("statusbar");

    QHBoxLayout* layout = new QHBoxLayout(m_statusBar);
    layout->setContentsMargins(16, 0, 16, 0);

    // 系统就绪
    QLabel* readyLabel = new QLabel("● 系统就绪", m_statusBar);
    readyLabel->setObjectName("statusReady");
    layout->addWidget(readyLabel);

    layout->addStretch();

    // 连接状态
    QLabel* connLabel = new QLabel("● 连接正常", m_statusBar);
    connLabel->setObjectName("statusConn");
    layout->addWidget(connLabel);

    // 时间
    m_statusTimeLabel = new QLabel(m_statusBar);
    m_statusTimeLabel->setObjectName("statusTime");
    layout->addWidget(m_statusTimeLabel);
}

void MainWindow::updateStatusBar()
{
    QString dt = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    m_statusTimeLabel->setText(dt);
}

// ============================================================
// 页面栈
// ============================================================
void MainWindow::setupStackedWidget()
{
    m_pageStack = new QStackedWidget(this);

    m_homePage = new HomePage(this);
    connect(m_homePage, &HomePage::inspectTrain,
            this, &MainWindow::onInspectTrain);

    m_pageStack->addWidget(m_homePage);
}

void MainWindow::navigateTo(int pageIndex)
{
    // 只保留首页（PAGE_HOME）
    if (pageIndex == PAGE_HOME) {
        m_pageStack->setCurrentIndex(pageIndex - 1);
        m_locationLabel->setText("当前位置: 首页");
    }
}

void MainWindow::showLoginPage()
{
    m_stackedWidget->setCurrentIndex(PAGE_LOGIN);
}

void MainWindow::showMainFrame()
{
    // 刷新用户信息
    m_userLabel->setText("👤 用户: " + Config::instance()->userDisplayName());

    // 默认进入首页
    navigateTo(PAGE_HOME);
    m_stackedWidget->setCurrentIndex(1);
}

void MainWindow::openInspectionWindow(const QString& trainNumber, quint64 reachDatetime,
                                        const QString& direction, const QString& detectionStation,
                                        int numCarriages, int numAxles,
                                        const QString& inspectionStatus, quint64 inspectionDatetime,
                                        const QString& leftVideoPath, const QString& rightVideoPath, const QString& topVideoPath)
{
    InspectionWindow* win = new InspectionWindow(trainNumber, reachDatetime,
                                                  direction, detectionStation,
                                                  numCarriages, numAxles,
                                                  inspectionStatus, inspectionDatetime,
                                                  leftVideoPath, rightVideoPath, topVideoPath,
                                                  this);
    connect(win, &InspectionWindow::inspectionCompleted, this, [=](const QString&, int, int) {
        // 检视完成后刷新首页
        qInfo() << "Inspection completed, refreshing home page...";
        if (m_homePage) {
            m_homePage->onQueryClicked();
        }
    });
    win->show();
}

void MainWindow::onInspectTrain(const QString& trainNumber, quint64 reachDatetime,
                                   const QString& direction, const QString& detectionStation,
                                   int numCarriages, int numAxles,
                                   const QString& inspectionStatus, quint64 inspectionDatetime,
                                   const QString& leftVideoPath, const QString& rightVideoPath, const QString& topVideoPath)
{
    // 打开检视窗口，传入完整列车信息
    openInspectionWindow(trainNumber, reachDatetime,
                         direction, detectionStation,
                         numCarriages, numAxles,
                         inspectionStatus, inspectionDatetime,
                         leftVideoPath, rightVideoPath, topVideoPath);
}

void MainWindow::onUserLabelClicked()
{
    // 弹出个人账户弹窗
    if (!m_profileDialog) {
        m_profileDialog = new ProfileDialog(this);
        connect(m_profileDialog, &ProfileDialog::logoutRequested, this, &MainWindow::onLogout);
    }

    m_profileDialog->exec();
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_windowTitleBar) {
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
    }
    if (obj == m_userLabel && event->type() == QEvent::MouseButtonPress) {
        onUserLabelClicked();
        return true;
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::onLogout()
{
    Config::instance()->clearToken();
    showLoginPage();
}

// ============================================================
// QSS 样式表
// ============================================================
void MainWindow::applyStyle()
{
    setStyleSheet(R"(
        /* 自定义标题栏 */
        #windowTitleBar {
            background-color: #092845;
        }
        #titleMinBtn, #titleMaxBtn {
            background: transparent;
            color: #ffffff;
            border: none;
            font-size: 16px;
            font-weight: bold;
        }
        #titleMinBtn:hover, #titleMaxBtn:hover {
            background-color: rgba(255,255,255,0.15);
        }
        #titleCloseBtn {
            background: transparent;
            color: #ffffff;
            border: none;
            font-size: 18px;
            font-weight: bold;
        }
        #titleCloseBtn:hover {
            background-color: #e75d55;
            color: #ffffff;
        }

        * {
            font-family: "Microsoft YaHei", "Segoe UI", Arial, sans-serif;
            font-size: 14px;
            color: #303133;
        }

        /* 顶部栏 */
        #topbar {
            background-color: #092845;
        }
        #locationLabel {
            color: #d1d5db;
            font-size: 14px;
        }
        #userLabel {
            color: #d1d5db;
            font-size: 14px;
        }
        #logoutBtn {
            background-color: transparent;
            color: #9ca3af;
            border: 1px solid #4b5563;
            border-radius: 3px;
            font-size: 13px;
        }
        #logoutBtn:hover {
            background-color: #e75d55;
            color: #ffffff;
            border-color: #e75d55;
        }

        /* 状态栏 */
        #statusbar {
            background-color: #092845;
        }
        #statusReady, #statusConn, #statusTime {
            color: #9ca3af;
            font-size: 12px;
        }
        #statusConn {
            color: #13ce66;
        }

        /* 通用 */
        QWidget {
            background-color: #f5f7fa;
        }
        QLabel {
            background-color: transparent;
        }
        QPushButton {
            background-color: #1890ff;
            color: #ffffff;
            border: none;
            border-radius: 3px;
            padding: 6px 16px;
            font-size: 14px;
        }
        QPushButton:hover {
            background-color: #40a9ff;
        }
        QPushButton:pressed {
            background-color: #096dd9;
        }
        QLineEdit, QTextEdit, QPlainTextEdit {
            background-color: #ffffff;
            border: 1px solid #dcdfe6;
            border-radius: 3px;
            padding: 6px 10px;
            color: #303133;
        }
        QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus {
            border-color: #1890ff;
        }
    )");
}

#ifdef Q_OS_WIN
bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, long* result)
{
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_NCHITTEST) {
        const int border = 8;
        int x = int(short(msg->lParam & 0xFFFF));
        int y = int(short((msg->lParam >> 16) & 0xFFFF));
        QRect r = frameGeometry();
        bool left   = x < r.left()   + border;
        bool right  = x > r.right()  - border;
        bool top    = y < r.top()    + border;
        bool bottom = y > r.bottom() - border;
        if (top && left)       *result = HTTOPLEFT;
        else if (top && right)  *result = HTTOPRIGHT;
        else if (bottom && left) *result = HTBOTTOMLEFT;
        else if (bottom && right) *result = HTBOTTOMRIGHT;
        else if (top)           *result = HTTOP;
        else if (bottom)        *result = HTBOTTOM;
        else if (left)          *result = HTLEFT;
        else if (right)         *result = HTRIGHT;
        else return false;
        return true;
    }
    return QMainWindow::nativeEvent(eventType, message, result);
}
#endif
