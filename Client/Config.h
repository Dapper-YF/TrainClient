#ifndef CONFIG_H
#define CONFIG_H

#include <QString>

class Config
{
public:
    static Config* instance();

    // API 服务器地址
    QString apiHost() const { return m_apiHost; }
    int apiPort() const { return m_apiPort; }
    QString apiBaseUrl() const;

    // 认证
    QString token() const { return m_token; }
    void setToken(const QString& token);
    void clearToken();

    // 当前用户
    QString username() const { return m_username; }
    QString userDisplayName() const { return m_userDisplayName; }
    QString userRole() const { return m_userRole; }
    void setUser(const QString& username, const QString& displayName, const QString& role = QString());

    bool isLoggedIn() const;
    bool isAdmin() const { return m_isAdmin; }
    void setAdmin(bool admin);

    // 记住登录
    bool isRememberMe() const;
    void setRememberMe(bool on);
    QString savedUsername() const;
    QString savedPassword() const;
    void setSavedCredentials(const QString& user, const QString& pass);
    void clearSavedCredentials();

private:
    Config();
    static Config* s_instance;

    QString m_apiHost = "127.0.0.1";
    int     m_apiPort = 8080;
    QString m_token;
    QString m_username;
    QString m_userDisplayName;
    QString m_userRole;
    bool    m_isAdmin = false;
};

#endif // CONFIG_H
