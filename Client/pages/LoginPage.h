#ifndef LOGINPAGE_H
#define LOGINPAGE_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QStackedWidget>
#include <QJsonObject>

class LoginPage : public QWidget
{
    Q_OBJECT

public:
    explicit LoginPage(QWidget* parent = nullptr);
    ~LoginPage();

signals:
    void loginSuccess();

private slots:
    void onLoginClicked();
    void onLoginSuccess(const QString& token, const QJsonObject& userData);
    void onLoginFailed(const QString& error);
    void onRegisterClicked();

private:
    void setupUi();
    void applyStyle();
    void showError(const QString& msg);
    QString validateRegisterFields();

    // 登录表单
    QLineEdit* m_usernameEdit = nullptr;
    QLineEdit* m_passwordEdit = nullptr;
    QCheckBox*  m_rememberCheck = nullptr;
    QPushButton* m_loginBtn = nullptr;
    QLabel*     m_errorLabel = nullptr;

    // 注册表单
    QLineEdit*  m_regUsernameEdit = nullptr;
    QLineEdit*  m_regNameEdit = nullptr;
    QLineEdit*  m_regWorknumberEdit = nullptr;
    QLineEdit*  m_regPasswordEdit = nullptr;
    QLineEdit*  m_regConfirmEdit = nullptr;
    QLineEdit*  m_regUnitEdit = nullptr;
    QLineEdit*  m_regCellphoneEdit = nullptr;
    QLabel*     m_regErrorLabel = nullptr;
    QPushButton* m_regSubmitBtn = nullptr;

    // 两个表单页面
    QWidget*     m_loginWidget = nullptr;
    QWidget*     m_registerWidget = nullptr;
};

#endif  // LOGINPAGE_H
