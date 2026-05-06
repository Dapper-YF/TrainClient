#include "LoginPage.h"
#include "../Config.h"
#include "../network/TrainApi.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>

// ============================================================
// 左侧面版（深色品牌区）
// ============================================================
class LeftPanel : public QWidget
{
public:
    explicit LeftPanel(QWidget* parent = nullptr) : QWidget(parent) {}

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        QLinearGradient grad(0, 0, width(), height());
        grad.setColorAt(0, QColor("#1a2a3a"));
        grad.setColorAt(1, QColor("#0d1821"));
        p.fillRect(rect(), grad);

        int W = width();
        int H = height();
        int CX = W / 2;

        // 上部分割线
        int sep1Y = int(H * 0.28);
        p.setPen(QPen(QColor(24, 144, 255, 100), 1));
        p.drawLine(CX - 60, sep1Y, CX + 60, sep1Y);

        // 主标题
        QFont f1("Microsoft YaHei", 36, QFont::Bold);
        p.setFont(f1);
        p.setPen(QColor("#ffffff"));
        p.drawText(QRect(0, sep1Y + 8, W, 50), Qt::AlignHCenter | Qt::AlignVCenter, "铁路货车装载状态");
        p.drawText(QRect(0, sep1Y + 58, W, 50), Qt::AlignHCenter | Qt::AlignVCenter, "监测系统");

        int sep2Y = sep1Y + 118;
        p.setPen(QPen(QColor(24, 144, 255, 100), 1));
        p.drawLine(CX - 60, sep2Y, CX + 60, sep2Y);

        // 副标题
        QFont f2("Segoe UI", 12);
        p.setFont(f2);
        p.setPen(QColor("#6b8cae"));
        p.drawText(QRect(0, sep2Y + 8, W, 30), Qt::AlignHCenter | Qt::AlignVCenter,
                   "Railway Freight Car Monitoring System");

        // 车厢图形（在文字下方）
        int carY = int(H * 0.56);
        int carW = 100;
        int carH = 48;
        QPen carPen(QColor("#3a8acc"), 2);
        p.setPen(carPen);
        p.drawRoundedRect(QRectF(CX - carW/2, carY - carH/2, carW, carH), 6, 6);
        p.setBrush(QColor(24, 144, 255, 30));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(QRectF(CX - carW/2 + 4, carY - carH/2 + 4, carW - 8, carH - 8), 3, 3);

        p.setPen(QPen(QColor(80, 170, 255, 200), 3));
        p.drawLine(CX - carW/2 - 16, carY + carH/2, CX - carW/2, carY + carH/2);
        p.drawLine(CX + carW/2, carY + carH/2, CX + carW/2 + 16, carY + carH/2);

        p.setBrush(QColor(255, 255, 255, 210));
        p.setPen(Qt::NoPen);
        int wR = 9;
        p.drawEllipse(QPointF(CX - carW/3, carY + carH), wR, wR);
        p.drawEllipse(QPointF(CX + carW/3, carY + carH), wR, wR);


        int railY1 = int(H * 0.88);
        int railY2 = int(H * 0.935);

        QLinearGradient rg(0, railY1 - 8, 0, railY2 + 8);
        rg.setColorAt(0, QColor(24, 144, 255, 0));
        rg.setColorAt(0.5, QColor(24, 144, 255, 20));
        rg.setColorAt(1, QColor(24, 144, 255, 0));
        p.fillRect(0, railY1 - 8, W, railY2 - railY1 + 16, rg);

        QPen railPen(QColor("#4a6a8a"), 2);
        p.setPen(railPen);
        p.drawLine(20, railY1, W - 20, railY1);
        p.drawLine(20, railY2, W - 20, railY2);

        QPen tiePen(QColor("#3d5570"), 3);
        p.setPen(tiePen);
        for (int x = 40; x < W - 20; x += 38) {
            p.drawLine(x, railY1 - 3, x, railY2 + 3);
        }

        p.setPen(QColor("#4a5a6a"));
        QFont f3("Segoe UI", 9);
        p.setFont(f3);
        p.drawText(QRect(0, H - 28, W, 20), Qt::AlignHCenter | Qt::AlignVCenter, "V1.0.0");
    }
};

// ============================================================
// 登录页主体
// ============================================================
LoginPage::LoginPage(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint);
    setupUi();
    applyStyle();

    // 自动填入记住的用户名/密码
    if (Config::instance()->isRememberMe()) {
        m_usernameEdit->setText(Config::instance()->savedUsername());
        m_passwordEdit->setText(Config::instance()->savedPassword());
        m_rememberCheck->setChecked(true);
    }

    // Enter 键登录
    connect(m_passwordEdit, &QLineEdit::returnPressed, this, &LoginPage::onLoginClicked);
    connect(m_usernameEdit, &QLineEdit::returnPressed, this, &LoginPage::onLoginClicked);
}

LoginPage::~LoginPage() {}

// ============================================================
// 构建界面
// ============================================================
void LoginPage::setupUi()
{
    setObjectName("loginBg");

    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ----- 左侧固定品牌区 -----
    LeftPanel* leftPanel = new LeftPanel(this);
    leftPanel->setObjectName("leftPanel");
    mainLayout->addWidget(leftPanel, 6);

    // ----- 右侧表单区（滚动） -----
    QScrollArea* scrollArea = new QScrollArea(this);
    scrollArea->setObjectName("rightScroll");
    scrollArea->setStyleSheet("QScrollArea { border: none; background: #f5f7fa; }");
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mainLayout->addWidget(scrollArea, 4);

    // 堆叠窗口：登录页 + 注册页
    QStackedWidget* stacked = new QStackedWidget(scrollArea);
    stacked->setObjectName("stackedWidget");

    // ----- 登录页 -----
    m_loginWidget = new QWidget(stacked);
    QVBoxLayout* loginLayout = new QVBoxLayout(m_loginWidget);
    loginLayout->setContentsMargins(60, 40, 60, 40);
    loginLayout->setSpacing(0);

    QLabel* welcomeLabel = new QLabel("欢迎回来", m_loginWidget);
    welcomeLabel->setObjectName("welcomeLabel");
    loginLayout->addWidget(welcomeLabel);

    loginLayout->addSpacing(8);

    QLabel* subLabel = new QLabel("请登录您的账户", m_loginWidget);
    subLabel->setObjectName("subLabel");
    loginLayout->addWidget(subLabel);

    loginLayout->addSpacing(32);

    QLabel* userLabel = new QLabel("用户名", m_loginWidget);
    userLabel->setObjectName("formLabel");
    loginLayout->addWidget(userLabel);
    loginLayout->addSpacing(6);

    m_usernameEdit = new QLineEdit(m_loginWidget);
    m_usernameEdit->setObjectName("formEdit");
    m_usernameEdit->setPlaceholderText("请输入用户名");
    m_usernameEdit->setFixedHeight(42);
    m_usernameEdit->setTextMargins(14, 0, 14, 0);
    loginLayout->addWidget(m_usernameEdit);

    loginLayout->addSpacing(18);

    QLabel* passLabel = new QLabel("密码", m_loginWidget);
    passLabel->setObjectName("formLabel");
    loginLayout->addWidget(passLabel);
    loginLayout->addSpacing(6);

    m_passwordEdit = new QLineEdit(m_loginWidget);
    m_passwordEdit->setObjectName("formEdit");
    m_passwordEdit->setPlaceholderText("请输入密码");
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setFixedHeight(42);
    m_passwordEdit->setTextMargins(14, 0, 14, 0);
    loginLayout->addWidget(m_passwordEdit);

    loginLayout->addSpacing(14);

    QHBoxLayout* row2 = new QHBoxLayout();
    row2->setContentsMargins(0, 0, 0, 0);

    m_rememberCheck = new QCheckBox("记住登录", m_loginWidget);
    m_rememberCheck->setObjectName("rememberCheck");
    m_rememberCheck->setMinimumSize(100, 24);
    m_rememberCheck->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    row2->addWidget(m_rememberCheck);
    row2->addStretch();

    QPushButton* registerLink = new QPushButton("注册 →", m_loginWidget);
    registerLink->setObjectName("registerLink");
    registerLink->setCursor(Qt::PointingHandCursor);
    connect(registerLink, &QPushButton::clicked, stacked, [=](){ stacked->setCurrentWidget(m_registerWidget); });
    row2->addWidget(registerLink);

    loginLayout->addLayout(row2);
    loginLayout->addSpacing(24);

    m_errorLabel = new QLabel(m_loginWidget);
    m_errorLabel->setObjectName("errorLabel");
    m_errorLabel->setVisible(false);
    m_errorLabel->setAlignment(Qt::AlignCenter);
    loginLayout->addWidget(m_errorLabel);

    m_loginBtn = new QPushButton("登 录", m_loginWidget);
    m_loginBtn->setObjectName("loginBtn");
    m_loginBtn->setFixedHeight(46);
    connect(m_loginBtn, &QPushButton::clicked, this, &LoginPage::onLoginClicked);
    loginLayout->addWidget(m_loginBtn);

    loginLayout->addStretch();   // 登录表单比注册表单短，底部用 stretch 垫起
    stacked->addWidget(m_loginWidget);

    // ----- 注册页 -----
    m_registerWidget = new QWidget(stacked);
    QVBoxLayout* regLayout = new QVBoxLayout(m_registerWidget);
    regLayout->setContentsMargins(60, 40, 60, 40);
    regLayout->setSpacing(0);

    // 顶部栏：标题 + 返回按钮
    QHBoxLayout* regHeader = new QHBoxLayout();
    regHeader->setContentsMargins(0, 0, 0, 0);
    QLabel* regTitle = new QLabel("新用户注册", m_registerWidget);
    regTitle->setObjectName("welcomeLabel");
    regHeader->addWidget(regTitle);
    regHeader->addStretch();
    QPushButton* backBtn = new QPushButton("← 返回登录", m_registerWidget);
    backBtn->setObjectName("backBtn");
    backBtn->setCursor(Qt::PointingHandCursor);
    connect(backBtn, &QPushButton::clicked, stacked, [=](){ stacked->setCurrentWidget(m_loginWidget); });
    regHeader->addWidget(backBtn);
    regLayout->addLayout(regHeader);

    regLayout->addSpacing(8);

    QLabel* regSub = new QLabel("请填写以下信息创建账户", m_registerWidget);
    regSub->setObjectName("subLabel");
    regLayout->addWidget(regSub);

    regLayout->addSpacing(24);

    // 辅助函数：添加一行
    auto addRegRow = [&](const QString& label, QLineEdit*& edit, const QString& placeholder,
                         bool isPassword = false) {
        QLabel* lbl = new QLabel(label, m_registerWidget);
        lbl->setObjectName("formLabel");
        regLayout->addWidget(lbl);
        edit = new QLineEdit(m_registerWidget);
        edit->setObjectName("formEdit");
        edit->setPlaceholderText(placeholder);
        edit->setFixedHeight(38);
        edit->setTextMargins(12, 0, 12, 0);
        if (isPassword) edit->setEchoMode(QLineEdit::Password);
        regLayout->addWidget(edit);
        regLayout->addSpacing(12);
    };

    addRegRow("用户名 *",     m_regUsernameEdit,   "3-20位字母、数字或下划线");
    addRegRow("姓名 *",       m_regNameEdit,        "请输入真实姓名");
    addRegRow("工号 *",       m_regWorknumberEdit,  "请输入工号");
    addRegRow("密码 *",       m_regPasswordEdit,    "至少6位", true);
    addRegRow("确认密码 *",   m_regConfirmEdit,     "再次输入密码", true);
    addRegRow("单位",         m_regUnitEdit,         "选填，如：上海铁路局");
    addRegRow("联系电话",     m_regCellphoneEdit,   "选填，如：13800138000");

    // 错误提示
    m_regErrorLabel = new QLabel(m_registerWidget);
    m_regErrorLabel->setObjectName("errorLabel");
    m_regErrorLabel->setVisible(false);
    m_regErrorLabel->setStyleSheet("color: #e75d55; font-size: 13px; padding: 4px 0;");
    regLayout->addWidget(m_regErrorLabel);

    // 注册按钮（绿色）
    m_regSubmitBtn = new QPushButton("注 册", m_registerWidget);
    m_regSubmitBtn->setObjectName("regBtn");
    m_regSubmitBtn->setFixedHeight(46);
    m_regSubmitBtn->setCursor(Qt::PointingHandCursor);
    connect(m_regSubmitBtn, &QPushButton::clicked, this, &LoginPage::onRegisterClicked);
    regLayout->addWidget(m_regSubmitBtn);

    regLayout->addStretch();   // 注册表单较长，stretch 确保底部也有填充
    stacked->addWidget(m_registerWidget);

    // ScrollArea 的 widget 设为 stacked
    scrollArea->setWidget(stacked);
}

// ============================================================
// 样式表
// ============================================================
void LoginPage::applyStyle()
{
    setStyleSheet(R"(
        #loginBg { background-color: #f5f7fa; }
        #leftPanel { background-color: #125491; }
        #welcomeLabel {
            font-size: 26px;
            font-weight: bold;
            color: #125491;
        }
        #subLabel { font-size: 14px; color: #909399; }
        #formLabel { font-size: 13px; color: #606266; font-weight: 500; margin-bottom: 4px; }
        #formEdit {
            background-color: #ffffff;
            border: 1px solid #dcdfe6;
            border-radius: 4px;
            padding: 0 14px;
            font-size: 14px;
            color: #303133;
        }
        #formEdit:focus {
            border-color: #1890ff;
            background-color: #f5f8ff;
        }
        #formEdit::placeholder { color: #c0c4cc; }
        #rememberCheck { font-size: 13px; color: #606266; }
        #registerLink {
            background-color: transparent;
            color: #1890ff;
            border: none;
            padding: 0;
            font-size: 13px;
        }
        #registerLink:hover { color: #40a9ff; text-decoration: underline; }
        #backBtn {
            background-color: transparent;
            color: #909399;
            border: none;
            padding: 0;
            font-size: 13px;
        }
        #backBtn:hover { color: #606266; text-decoration: underline; }
        #errorLabel {
            color: #e75d55;
            font-size: 13px;
            padding: 6px 0;
        }
        #loginBtn {
            background-color: #1890ff;
            color: #ffffff;
            border: none;
            border-radius: 4px;
            font-size: 15px;
            font-weight: bold;
            letter-spacing: 6px;
        }
        #loginBtn:hover { background-color: #40a9ff; }
        #loginBtn:pressed { background-color: #096dd9; }
        #loginBtn:disabled { background-color: #a0cfff; color: #ffffff; }
        #regBtn {
            background-color: #52c41a;
            color: #ffffff;
            border: none;
            border-radius: 4px;
            font-size: 15px;
            font-weight: bold;
            letter-spacing: 4px;
        }
        #regBtn:hover { background-color: #73d13d; }
        #regBtn:pressed { background-color: #389e0d; }
        #regBtn:disabled { background-color: #b7eb8f; color: #ffffff; }
    )");
}

// ============================================================
// 登录
// ============================================================
void LoginPage::onLoginClicked()
{
    QString username = m_usernameEdit->text().trimmed();
    QString password = m_passwordEdit->text();

    if (username.isEmpty()) {
        showError("请输入用户名");
        m_usernameEdit->setFocus();
        return;
    }
    if (password.isEmpty()) {
        showError("请输入密码");
        m_passwordEdit->setFocus();
        return;
    }

    showError("");
    m_loginBtn->setText("登录中...");
    m_loginBtn->setEnabled(false);

    TrainApiClient* api = new TrainApiClient(this);
    connect(api, &TrainApiClient::loginSuccess,
            this, &LoginPage::onLoginSuccess);
    connect(api, &TrainApiClient::loginFailed,
            this, &LoginPage::onLoginFailed);
    connect(api, &TrainApiClient::error,
            this, [=](const QString& err) {
        showError("网络错误：" + err);
        m_loginBtn->setText("登 录");
        m_loginBtn->setEnabled(true);
    });
    api->login(username, password);
}

void LoginPage::onLoginSuccess(const QString& token, const QJsonObject& userData)
{
    Config::instance()->setToken(token);
    Config::instance()->setUser(
        userData["username"].toString(),
        userData["name"].toString(),
        userData["role"].toString()
    );

    if (m_rememberCheck->isChecked()) {
        Config::instance()->setSavedCredentials(
            m_usernameEdit->text().trimmed(),
            m_passwordEdit->text()
        );
        Config::instance()->setRememberMe(true);
    } else {
        Config::instance()->setRememberMe(false);
        Config::instance()->clearSavedCredentials();
    }

    emit loginSuccess();
}

void LoginPage::onLoginFailed(const QString& error)
{
    showError(error);
    m_loginBtn->setText("登 录");
    m_loginBtn->setEnabled(true);
}

void LoginPage::showError(const QString& msg)
{
    m_errorLabel->setText(msg);
    m_errorLabel->setVisible(!msg.isEmpty());
}

// ============================================================
// 注册字段校验
// ============================================================
QString LoginPage::validateRegisterFields()
{
    QString username   = m_regUsernameEdit->text().trimmed();
    QString name       = m_regNameEdit->text().trimmed();
    QString worknumber = m_regWorknumberEdit->text().trimmed();
    QString password   = m_regPasswordEdit->text();
    QString confirm   = m_regConfirmEdit->text();
    QString cellphone  = m_regCellphoneEdit->text().trimmed();

    if (username.isEmpty() || username.length() < 3 || username.length() > 20)
        return "用户名必填，长度3-20位";
    for (const QChar& c : username) {
        if (!c.isLetterOrNumber() && c != '_')
            return "用户名只能包含字母、数字和下划线";
    }
    if (name.isEmpty()) return "姓名不能为空";
    if (worknumber.isEmpty()) return "工号不能为空";
    if (password.isEmpty() || password.length() < 6)
        return "密码至少6位";
    if (password != confirm)
        return "两次输入的密码不一致";
    if (!cellphone.isEmpty() && cellphone.length() < 7)
        return "联系电话格式不正确";

    return QString();
}

// ============================================================
// 注册提交
// ============================================================
void LoginPage::onRegisterClicked()
{
    QString err = validateRegisterFields();
    if (!err.isEmpty()) {
        m_regErrorLabel->setText(err);
        m_regErrorLabel->setVisible(true);
        return;
    }

    m_regErrorLabel->setVisible(false);
    m_regSubmitBtn->setText("注册中...");
    m_regSubmitBtn->setEnabled(false);

    TrainApiClient* api = new TrainApiClient(this);
    connect(api, &TrainApiClient::userCreated,
            this, [=](bool success, const QString& error) {
        m_regSubmitBtn->setText("注 册");
        m_regSubmitBtn->setEnabled(true);

        if (success) {
            QString username = m_regUsernameEdit->text().trimmed();
            // 清空注册表单，切回登录并预填用户名
            m_regUsernameEdit->clear();
            m_regNameEdit->clear();
            m_regWorknumberEdit->clear();
            m_regPasswordEdit->clear();
            m_regConfirmEdit->clear();
            m_regUnitEdit->clear();
            m_regCellphoneEdit->clear();
            m_regErrorLabel->setVisible(false);

            // 切换到登录页
            QStackedWidget* stacked = m_loginWidget->parentWidget()->findChild<QStackedWidget*>("stackedWidget");
            if (stacked) stacked->setCurrentWidget(m_loginWidget);
            m_usernameEdit->setText(username);

            QMessageBox::information(this, "注册成功",
                "账户创建成功！\n\n用户名：「" + username + "」\n请使用该账户登录。");
        } else {
            m_regErrorLabel->setText(error.isEmpty() ? "注册失败，请稍后重试" : error);
            m_regErrorLabel->setVisible(true);
        }
        api->deleteLater();
    });

    api->createUser(
        m_regUsernameEdit->text().trimmed(),
        m_regNameEdit->text().trimmed(),
        m_regWorknumberEdit->text().trimmed(),
        m_regPasswordEdit->text(),
        m_regUnitEdit->text().trimmed(),
        m_regCellphoneEdit->text().trimmed()
    );
}
