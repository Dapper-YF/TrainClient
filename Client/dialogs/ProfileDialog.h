#ifndef PROFILEDIALOG_H
#define PROFILEDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QNetworkAccessManager>
#include <QJsonObject>

class ProfileDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ProfileDialog(QWidget* parent = nullptr);
    ~ProfileDialog();

signals:
    void logoutRequested();

private slots:
    void onSaveClicked();
    void onLogoutClicked();
    void onProfileReady(const QJsonObject& profile);
    void onSaveDone(bool success, const QString& error);

private:
    void loadProfile();

    QLabel* m_usernameLabel = nullptr;
    QLabel* m_nameLabel = nullptr;
    QLabel* m_worknumberLabel = nullptr;
    QLabel* m_roleLabel = nullptr;
    QLineEdit* m_unitEdit = nullptr;
    QLineEdit* m_phoneEdit = nullptr;
    QPushButton* m_saveBtn = nullptr;

    QNetworkAccessManager* m_nam = nullptr;
    QJsonObject m_profileData;
};

#endif // PROFILEDIALOG_H
