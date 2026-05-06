#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE
class CTrainTcpServer;
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    CTrainTcpServer *m_pTcpServer{nullptr};
    QTimer *m_simulateTimer;
public slots:
    void Test();

    void Config();
    void onLogAppended(const QString &time,
                       const QString &level,
                       const QString &msg);
    // void on_pushButtonTest_clicked();
};
#endif // MAINWINDOW_H
