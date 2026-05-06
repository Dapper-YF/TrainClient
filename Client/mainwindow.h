#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QLabel>
#include <QPushButton>
#include <QTimer>

class LoginPage;
class HomePage;
class ProfileDialog;
class InspectionWindow;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

    // 页面索引
    enum PageIndex {
        PAGE_LOGIN = 0,
        PAGE_HOME = 1
    };

signals:
    void userLoggedIn();
    void userLoggedOut();

public slots:
    void showLoginPage();
    void showMainFrame();
    void navigateTo(int pageIndex);
    void updateStatusBar();
    void onLogout();
    void onInspectTrain(const QString& trainNumber, quint64 reachDatetime,
                        const QString& direction, const QString& detectionStation,
                        int numCarriages, int numAxles,
                        const QString& inspectionStatus, quint64 inspectionDatetime,
                        const QString& leftVideoPath, const QString& rightVideoPath, const QString& topVideoPath);
    void onUserLabelClicked();
    bool eventFilter(QObject* obj, QEvent* event) override;
    void openInspectionWindow(const QString& trainNumber, quint64 reachDatetime,
                              const QString& direction, const QString& detectionStation,
                              int numCarriages, int numAxles,
                              const QString& inspectionStatus, quint64 inspectionDatetime,
                              const QString& leftVideoPath, const QString& rightVideoPath, const QString& topVideoPath);

private:
    void setupUi();
    void setupTopBar();
    void setupStatusBar();
    void setupStackedWidget();
    void applyStyle();

    // 登录相关
    QWidget* m_loginContainer;
    QStackedWidget* m_stackedWidget;   // 主切换：登录页 vs 主框架
    QStackedWidget* m_pageStack;       // 页面栈：各功能页面

    // 主框架
    QWidget* m_mainFrame;
    QWidget* m_topBar;
    QWidget* m_statusBar;
    QLabel* m_locationLabel;
    QLabel* m_statusTimeLabel;
    QLabel* m_userLabel;

    // 页面实例
    LoginPage* m_loginPage = nullptr;
    HomePage* m_homePage = nullptr;
    ProfileDialog* m_profileDialog = nullptr;

    QTimer* m_timer;

    // Frameless window
    QWidget* m_windowTitleBar = nullptr;
    bool m_dragging = false;
    QPoint m_dragPos;

#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray& eventType, void* message, long* result) override;
#endif
};

#endif // MAINWINDOW_H
