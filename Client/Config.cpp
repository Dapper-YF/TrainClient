#include "Config.h"
#include <QSettings>
#include <QCoreApplication>

Config* Config::s_instance = nullptr;

Config::Config() {}

Config* Config::instance()
{
    if (s_instance == nullptr) {
        s_instance = new Config();
    }
    return s_instance;
}

QString Config::apiBaseUrl() const
{
    return QString("http://%1:%2").arg(m_apiHost).arg(m_apiPort);
}

void Config::setToken(const QString& token)
{
    m_token = token;
}

void Config::clearToken()
{
    m_token.clear();
    m_username.clear();
    m_userDisplayName.clear();
    m_userRole.clear();
    m_isAdmin = false;
}

void Config::setUser(const QString& username, const QString& displayName, const QString& role)
{
    m_username = username;
    m_userDisplayName = displayName;
    m_userRole = role;
}

void Config::setAdmin(bool admin)
{
    m_isAdmin = admin;
}

bool Config::isLoggedIn() const
{
    return !m_token.isEmpty();
}

bool Config::isRememberMe() const
{
    QSettings s("MVS", "TrainClient");
    return s.value("rememberMe", false).toBool();
}

void Config::setRememberMe(bool on)
{
    QSettings s("MVS", "TrainClient");
    s.setValue("rememberMe", on);
    if (!on) {
        s.remove("savedUsername");
        s.remove("savedPassword");
    }
}

QString Config::savedUsername() const
{
    QSettings s("MVS", "TrainClient");
    return s.value("savedUsername", "").toString();
}

QString Config::savedPassword() const
{
    QSettings s("MVS", "TrainClient");
    return s.value("savedPassword", "").toString();
}

void Config::setSavedCredentials(const QString& user, const QString& pass)
{
    QSettings s("MVS", "TrainClient");
    s.setValue("savedUsername", user);
    s.setValue("savedPassword", pass);
}

void Config::clearSavedCredentials()
{
    QSettings s("MVS", "TrainClient");
    s.remove("savedUsername");
    s.remove("savedPassword");
    s.setValue("rememberMe", false);
}
