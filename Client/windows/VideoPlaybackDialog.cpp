#include "VideoPlaybackDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDebug>
#include <QMouseEvent>
#include <QAxWidget>
#include <QTimer>

VideoPlaybackDialog::VideoPlaybackDialog(QWidget* parent)
    : QDialog(parent)
    , m_isPlaying(false)
    , m_loopTimer(nullptr)
    , m_lastLeftState(-1)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setWindowTitle(QString::fromUtf8("▶ 过车视频"));
    setMinimumSize(700, 600);
    resize(700, 600);
    setStyleSheet("QDialog { background-color: #1a1a2e; }");

    QVBoxLayout* main = new QVBoxLayout(this);
    main->setContentsMargins(0, 0, 0, 0);
    main->setSpacing(0);

    // 标题栏 (32px, 可拖拽)
    QWidget* titleBar = new QWidget(this);
    titleBar->setFixedHeight(32);
    titleBar->setObjectName("windowTitleBar");
    titleBar->setCursor(Qt::ArrowCursor);
    titleBar->setStyleSheet("#windowTitleBar { background-color: #092845; }");
    QHBoxLayout* titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(12, 0, 4, 0);
    titleLayout->setSpacing(0);

    QLabel* titleLabel = new QLabel(QString::fromUtf8("铁路货车装载状态监测系统"), titleBar);
    titleLabel->setStyleSheet("color: #ffffff; font-size: 13px; background: transparent;");
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();

    // 按钮容器（固定宽度，防止被标题挤压）
    QWidget* btnContainer = new QWidget(titleBar);
    btnContainer->setFixedWidth(104);
    QHBoxLayout* btnLayout = new QHBoxLayout(btnContainer);
    btnLayout->setContentsMargins(0, 0, 0, 0);
    btnLayout->setSpacing(0);

    QPushButton* minBtn = new QPushButton(QString::fromUtf8("_"), btnContainer);
    minBtn->setFixedSize(32, 28);
    minBtn->setStyleSheet("QPushButton { background: transparent; color: #ffffff; border: none; font-size: 16px; font-weight: bold; } QPushButton:hover { background-color: rgba(255,255,255,0.15); }");
    connect(minBtn, &QPushButton::clicked, this, &VideoPlaybackDialog::showMinimized);
    btnLayout->addWidget(minBtn);

    QPushButton* maxBtn = new QPushButton(QString::fromUtf8("□"), btnContainer);
    maxBtn->setFixedSize(32, 28);
    maxBtn->setStyleSheet("QPushButton { background: transparent; color: #ffffff; border: none; font-size: 16px; font-weight: bold; } QPushButton:hover { background-color: rgba(255,255,255,0.15); }");
    connect(maxBtn, &QPushButton::clicked, this, [this, maxBtn]() {
        if (isMaximized()) { showNormal(); maxBtn->setText(QString::fromUtf8("□")); }
        else { showMaximized(); maxBtn->setText(QString::fromUtf8("❐")); }
    });
    btnLayout->addWidget(maxBtn);

    QPushButton* closeBtn = new QPushButton(QString::fromUtf8("X"), btnContainer);
    closeBtn->setFixedSize(32, 28);
    closeBtn->setStyleSheet("QPushButton { background: transparent; color: #ffffff; border: none; font-size: 18px; font-weight: bold; } QPushButton:hover { background-color: #e75d55; color: #ffffff; }");
    connect(closeBtn, &QPushButton::clicked, this, &VideoPlaybackDialog::onCloseClicked);
    btnLayout->addWidget(closeBtn);

    titleLayout->addWidget(btnContainer);

    titleBar->installEventFilter(this);
    main->addWidget(titleBar);

    // 三路视频区域
    QWidget* videoArea = new QWidget(this);
    videoArea->setStyleSheet("background-color: #092845;");
    QVBoxLayout* videoLayout = new QVBoxLayout(videoArea);
    videoLayout->setContentsMargins(8, 8, 8, 8);
    videoLayout->setSpacing(6);

    // 左侧视图
    QWidget* leftPanel = new QWidget(videoArea);
    leftPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QVBoxLayout* leftV = new QVBoxLayout(leftPanel);
    leftV->setContentsMargins(0, 0, 0, 0);
    leftV->setSpacing(4);
    QLabel* leftTitle = new QLabel(QString::fromUtf8("左侧视图"), leftPanel);
    leftTitle->setFixedHeight(24);
    leftTitle->setAlignment(Qt::AlignCenter);
    leftTitle->setStyleSheet("color: #a0aec0; font-size: 12px; background: #092845; border-top-left-radius: 4px; border-top-right-radius: 4px;");
    m_leftPlayer = new QAxWidget(leftPanel);
    m_leftPlayer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_leftPlayer->setMinimumHeight(160);
    m_leftPlayer->setStyleSheet("background: #000000; border: 1px solid #30363d; border-bottom-left-radius: 4px; border-bottom-right-radius: 4px;");
    if (m_leftPlayer->setControl("{6BF52A52-394A-11D3-B153-00C04F79FAA6}")) {
        m_leftPlayer->setProperty("uiMode", "none");
        m_leftPlayer->dynamicCall("settings.set_autoStart(bool)", false);
    }
    leftV->addWidget(leftTitle);
    leftV->addWidget(m_leftPlayer, 1);
    videoLayout->addWidget(leftPanel, 1);

    // 顶部视图
    QWidget* topPanel = new QWidget(videoArea);
    topPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QVBoxLayout* topV = new QVBoxLayout(topPanel);
    topV->setContentsMargins(0, 0, 0, 0);
    topV->setSpacing(4);
    QLabel* topTitle = new QLabel(QString::fromUtf8("顶部视图"), topPanel);
    topTitle->setFixedHeight(24);
    topTitle->setAlignment(Qt::AlignCenter);
    topTitle->setStyleSheet("color: #a0aec0; font-size: 12px; background: #092845; border-top-left-radius: 4px; border-top-right-radius: 4px;");
    m_topPlayer = new QAxWidget(topPanel);
    m_topPlayer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_topPlayer->setMinimumHeight(160);
    m_topPlayer->setStyleSheet("background: #000000; border: 1px solid #30363d; border-bottom-left-radius: 4px; border-bottom-right-radius: 4px;");
    if (m_topPlayer->setControl("{6BF52A52-394A-11D3-B153-00C04F79FAA6}")) {
        m_topPlayer->setProperty("uiMode", "none");
        m_topPlayer->dynamicCall("settings.set_autoStart(bool)", false);
    }
    topV->addWidget(topTitle);
    topV->addWidget(m_topPlayer, 1);
    videoLayout->addWidget(topPanel, 1);

    // 右侧视图
    QWidget* rightPanel = new QWidget(videoArea);
    rightPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QVBoxLayout* rightV = new QVBoxLayout(rightPanel);
    rightV->setContentsMargins(0, 0, 0, 0);
    rightV->setSpacing(4);
    QLabel* rightTitle = new QLabel(QString::fromUtf8("右侧视图"), rightPanel);
    rightTitle->setFixedHeight(24);
    rightTitle->setAlignment(Qt::AlignCenter);
    rightTitle->setStyleSheet("color: #a0aec0; font-size: 12px; background: #092845; border-top-left-radius: 4px; border-top-right-radius: 4px;");
    m_rightPlayer = new QAxWidget(rightPanel);
    m_rightPlayer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_rightPlayer->setMinimumHeight(160);
    m_rightPlayer->setStyleSheet("background: #000000; border: 1px solid #30363d; border-bottom-left-radius: 4px; border-bottom-right-radius: 4px;");
    if (m_rightPlayer->setControl("{6BF52A52-394A-11D3-B153-00C04F79FAA6}")) {
        m_rightPlayer->setProperty("uiMode", "none");
        m_rightPlayer->dynamicCall("settings.set_autoStart(bool)", false);
    }
    rightV->addWidget(rightTitle);
    rightV->addWidget(m_rightPlayer, 1);
    videoLayout->addWidget(rightPanel, 1);

    main->addWidget(videoArea, 1);

    // 底部控制栏
    QWidget* bottomBar = new QWidget(this);
    bottomBar->setFixedHeight(40);
    bottomBar->setStyleSheet("background-color: #092845;");
    QHBoxLayout* bottomLayout = new QHBoxLayout(bottomBar);
    bottomLayout->setContentsMargins(12, 0, 12, 0);

    QPushButton* playBtn = new QPushButton(QString::fromUtf8("■ 暂停"), bottomBar);
    playBtn->setFixedSize(70, 28);
    playBtn->setObjectName("playBtn");
    playBtn->setStyleSheet(R"(
        QPushButton { background-color: #e6a23c; color: white; border: none; border-radius: 4px; font-size: 12px; font-weight: bold; }
        QPushButton:hover { background-color: #ebb563; }
    )");
    connect(playBtn, &QPushButton::clicked, this, &VideoPlaybackDialog::onPlayPauseClicked);

    QLabel* infoLabel = new QLabel(QString::fromUtf8("三路同步 | 自动循环播放"), bottomBar);
    infoLabel->setStyleSheet("color: #606266; font-size: 11px; background: transparent;");
    bottomLayout->addWidget(playBtn);
    bottomLayout->addWidget(infoLabel);
    bottomLayout->addStretch();
    main->addWidget(bottomBar);
}

VideoPlaybackDialog::~VideoPlaybackDialog()
{
    m_isPlaying = false;
    if (m_loopTimer) {
        m_loopTimer->stop();
    }
}

bool VideoPlaybackDialog::eventFilter(QObject* obj, QEvent* event)
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

// 辅助：启动一个播放器
static void startPlayer(QAxWidget* player, const QString& url)
{
    if (!player || player->control().isEmpty()) return;
    if (!url.isEmpty()) player->setProperty("URL", url);
    player->dynamicCall("controls.play()");
}

void VideoPlaybackDialog::loadVideos(const QString& leftUrl, const QString& topUrl, const QString& rightUrl)
{
    m_isPlaying = false;
    m_lastLeftState = -1;
    if (m_loopTimer) { m_loopTimer->stop(); delete m_loopTimer; m_loopTimer = nullptr; }

    QPushButton* playBtn = findChild<QPushButton*>("playBtn");
    if (playBtn) {
        playBtn->setText(QString::fromUtf8("■ 暂停"));
        playBtn->setStyleSheet(R"(
            QPushButton { background-color: #e6a23c; color: white; border: none; border-radius: 4px; font-size: 12px; font-weight: bold; }
            QPushButton:hover { background-color: #ebb563; }
        )");
    }

    // 设置 URL
    if (m_leftPlayer && !m_leftPlayer->control().isEmpty() && !leftUrl.isEmpty())
        m_leftPlayer->setProperty("URL", leftUrl);
    if (m_topPlayer && !m_topPlayer->control().isEmpty() && !topUrl.isEmpty())
        m_topPlayer->setProperty("URL", topUrl);
    if (m_rightPlayer && !m_rightPlayer->control().isEmpty() && !rightUrl.isEmpty())
        m_rightPlayer->setProperty("URL", rightUrl);

    // 保存 URL 用于循环重启
    m_leftUrl = leftUrl;
    m_topUrl = topUrl;
    m_rightUrl = rightUrl;

    // 延迟后自动播放
    QTimer::singleShot(300, this, [this]() {
        startPlayer(m_leftPlayer, QString());
        startPlayer(m_topPlayer, QString());
        startPlayer(m_rightPlayer, QString());
        m_isPlaying = true;
        qDebug() << "[Video] Auto-play started";
    });

    // 循环检测定时器：监控 playState，播放结束后重启
    m_loopTimer = new QTimer(this);
    m_loopTimer->setInterval(1000);
    connect(m_loopTimer, &QTimer::timeout, this, [this]() {
        if (!m_isPlaying) return;
        if (!m_leftPlayer || m_leftPlayer->control().isEmpty()) return;

        // WMP playState: 0=Undefined, 1=Stopped, 2=Paused, 3=Playing, 8=MediaEnded
        int state = m_leftPlayer->property("playState").toInt();
        if (state == 8 || (m_lastLeftState == 3 && state == 1)) {
            // MediaEnded 或从 Playing 变为 Stopped：播放结束，重启全部
            qDebug() << "[Video] Play ended (state=" << state << "), restarting all";
            QTimer::singleShot(0, this, [this]() {
                startPlayer(m_leftPlayer, m_leftUrl);
                startPlayer(m_topPlayer, m_topUrl);
                startPlayer(m_rightPlayer, m_rightUrl);
                m_lastLeftState = -1;
            });
        }
        m_lastLeftState = state;
    });
    m_loopTimer->start();
}

void VideoPlaybackDialog::onPlayPauseClicked()
{
    QPushButton* playBtn = findChild<QPushButton*>("playBtn");

    if (!m_isPlaying) {
        startPlayer(m_leftPlayer, QString());
        startPlayer(m_topPlayer, QString());
        startPlayer(m_rightPlayer, QString());
        m_isPlaying = true;
        if (playBtn) {
            playBtn->setText(QString::fromUtf8("■ 暂停"));
            playBtn->setStyleSheet(R"(
                QPushButton { background-color: #e6a23c; color: white; border: none; border-radius: 4px; font-size: 12px; font-weight: bold; }
                QPushButton:hover { background-color: #ebb563; }
            )");
        }
    } else {
        if (m_leftPlayer && !m_leftPlayer->control().isEmpty())
            m_leftPlayer->dynamicCall("controls.pause()");
        if (m_topPlayer && !m_topPlayer->control().isEmpty())
            m_topPlayer->dynamicCall("controls.pause()");
        if (m_rightPlayer && !m_rightPlayer->control().isEmpty())
            m_rightPlayer->dynamicCall("controls.pause()");
        m_isPlaying = false;
        if (playBtn) {
            playBtn->setText(QString::fromUtf8("▶ 播放"));
            playBtn->setStyleSheet(R"(
                QPushButton { background-color: #13ce66; color: white; border: none; border-radius: 4px; font-size: 12px; font-weight: bold; }
                QPushButton:hover { background-color: #3dbd5a; }
            )");
        }
    }
}

void VideoPlaybackDialog::onCloseClicked()
{
    m_isPlaying = false;
    if (m_loopTimer) m_loopTimer->stop();
    if (m_leftPlayer && !m_leftPlayer->control().isEmpty())
        m_leftPlayer->dynamicCall("controls.stop()");
    if (m_topPlayer && !m_topPlayer->control().isEmpty())
        m_topPlayer->dynamicCall("controls.stop()");
    if (m_rightPlayer && !m_rightPlayer->control().isEmpty())
        m_rightPlayer->dynamicCall("controls.stop()");
    close();
}
