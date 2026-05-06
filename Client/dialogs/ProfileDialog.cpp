#include "ProfileDialog.h"
#include "../Config.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QNetworkReply>

ProfileDialog::ProfileDialog(QWidget* parent)
    : QDialog(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    setWindowTitle(QString::fromUtf8("个人账户"));
    setFixedSize(420, 380);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 20, 24, 16);
    mainLayout->setSpacing(12);

    // ========== 标题 ==========
    QLabel* title = new QLabel(QString::fromUtf8("👤 个人账户"), this);
    title->setStyleSheet("font-size: 16px; font-weight: bold; color: #222;");
    mainLayout->addWidget(title);

    // ========== 基本信息 ==========
    QGridLayout* grid = new QGridLayout();
    grid->setSpacing(10);
    grid->setColumnMinimumWidth(0, 70);
    grid->setColumnStretch(1, 1);

    int row = 0;
    grid->addWidget(new QLabel(QString::fromUtf8("账户名:")), row, 0);
    m_usernameLabel = new QLabel(QString::fromUtf8("-"), this);
    m_usernameLabel->setStyleSheet("color: #333; font-weight: bold;");
    grid->addWidget(m_usernameLabel, row++, 1);

    grid->addWidget(new QLabel(QString::fromUtf8("姓名:")), row, 0);
    m_nameLabel = new QLabel(QString::fromUtf8("-"), this);
    m_nameLabel->setStyleSheet("color: #333; font-weight: bold;");
    grid->addWidget(m_nameLabel, row++, 1);

    grid->addWidget(new QLabel(QString::fromUtf8("工号:")), row, 0);
    m_worknumberLabel = new QLabel(QString::fromUtf8("-"), this);
    m_worknumberLabel->setStyleSheet("color: #333;");
    grid->addWidget(m_worknumberLabel, row++, 1);

    grid->addWidget(new QLabel(QString::fromUtf8("角色:")), row, 0);
    m_roleLabel = new QLabel(QString::fromUtf8("-"), this);
    m_roleLabel->setStyleSheet("color: #333;");
    grid->addWidget(m_roleLabel, row++, 1);

    grid->addWidget(new QLabel(QString::fromUtf8("单位:")), row, 0);
    m_unitEdit = new QLineEdit(this);
    m_unitEdit->setPlaceholderText(QString::fromUtf8("请输入单位"));
    grid->addWidget(m_unitEdit, row++, 1);

    grid->addWidget(new QLabel(QString::fromUtf8("电话:")), row, 0);
    m_phoneEdit = new QLineEdit(this);
    m_phoneEdit->setPlaceholderText(QString::fromUtf8("请输入手机号"));
    grid->addWidget(m_phoneEdit, row++, 1);

    mainLayout->addLayout(grid);

    // ========== 按钮行 ==========
    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->addStretch();

    m_saveBtn = new QPushButton(QString::fromUtf8("保存修改"), this);
    m_saveBtn->setFixedWidth(100);
    m_saveBtn->setStyleSheet("background-color: #2d7d2d; color: white; border: none; border-radius: 4px; padding: 6px;");
    btnRow->addWidget(m_saveBtn);

    QPushButton* logoutBtn = new QPushButton(QString::fromUtf8("退出登录"), this);
    logoutBtn->setFixedWidth(80);
    logoutBtn->setStyleSheet("background-color: #d93636; color: white; border: none; border-radius: 4px; padding: 6px;");
    btnRow->addWidget(logoutBtn);

    mainLayout->addLayout(btnRow);

    connect(m_saveBtn, &QPushButton::clicked, this, &ProfileDialog::onSaveClicked);
    connect(logoutBtn, &QPushButton::clicked, this, &ProfileDialog::onLogoutClicked);

    loadProfile();
}

ProfileDialog::~ProfileDialog() {}

void ProfileDialog::loadProfile() {
    QString username = Config::instance()->username();
    if (username.isEmpty()) return;

    QString baseUrl = Config::instance()->apiBaseUrl();
    QString token = Config::instance()->token();

    QUrl url(baseUrl + "/api/users/info/" + username);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!token.isEmpty()) {
        req.setRawHeader("Authorization", ("Bearer " + token).toUtf8());
    }

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            return;
        }
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject root = doc.object();
        if (root["success"].toBool()) {
            onProfileReady(root["data"].toObject());
        }
    });
}

void ProfileDialog::onProfileReady(const QJsonObject& profile) {
    m_profileData = profile;
    m_usernameLabel->setText(profile["username"].toString(QString::fromUtf8("-")));
    m_nameLabel->setText(profile["name"].toString(QString::fromUtf8("-")));
    m_worknumberLabel->setText(profile["worknumber"].toString(QString::fromUtf8("-")));
    QString role = profile["role"].toString();
    if (role == "admin") role = QString::fromUtf8("管理员");
    else if (role == "annotator") role = QString::fromUtf8("标注员");
    else if (role == "inspector") role = QString::fromUtf8("检车员");
    m_roleLabel->setText(role.isEmpty() ? QString::fromUtf8("-") : role);
    m_unitEdit->setText(profile["unit"].toString());
    m_phoneEdit->setText(profile["cellphone"].toString());
}

void ProfileDialog::onSaveClicked() {
    QString username = Config::instance()->username();
    if (username.isEmpty()) return;

    QString unit = m_unitEdit->text().trimmed();
    QString phone = m_phoneEdit->text().trimmed();

    if (!phone.isEmpty() && phone.length() < 7) {
        QMessageBox::warning(this, QString::fromUtf8("输入错误"),
            QString::fromUtf8("电话号码格式不正确"));
        return;
    }

    m_saveBtn->setEnabled(false);
    m_saveBtn->setText(QString::fromUtf8("保存中..."));

    QString baseUrl = Config::instance()->apiBaseUrl();
    QString token = Config::instance()->token();

    QUrl url(baseUrl + "/api/users/" + username);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!token.isEmpty()) {
        req.setRawHeader("Authorization", ("Bearer " + token).toUtf8());
    }

    QJsonObject data;
    data["unit"] = unit;
    data["cellphone"] = phone;

    QNetworkReply* reply = m_nam->put(req, QJsonDocument(data).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        m_saveBtn->setEnabled(true);
        m_saveBtn->setText(QString::fromUtf8("保存修改"));

        if (reply->error() != QNetworkReply::NoError) {
            QMessageBox::warning(this, QString::fromUtf8("保存失败"),
                QString::fromUtf8("保存失败: %1").arg(reply->errorString()));
            return;
        }
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject root = doc.object();
        QString err = root["error"].toString();
        onSaveDone(root["success"].toBool(), err);
    });
}

void ProfileDialog::onSaveDone(bool success, const QString& error) {
    if (success) {
        QMessageBox::information(this, QString::fromUtf8("保存成功"),
            QString::fromUtf8("个人信息已保存。"));
        accept();
    } else {
        QMessageBox::warning(this, QString::fromUtf8("保存失败"),
            QString::fromUtf8("保存失败: %1").arg(error.isEmpty() ? QString::fromUtf8("未知错误") : error));
    }
}

void ProfileDialog::onLogoutClicked() {
    int ret = QMessageBox::question(this, QString::fromUtf8("退出登录"),
        QString::fromUtf8("确定要退出登录吗？"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    Config::instance()->clearToken();
    Config::instance()->clearSavedCredentials();
    Config::instance()->setRememberMe(false);

    accept();
    emit logoutRequested();
}
