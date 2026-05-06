#ifndef CPREVIEWDIALOG_H
#define CPREVIEWDIALOG_H

#include <QDialog>
#include <QTimer>
#include "ui_cpreviewdialog.h"
// namespace Ui {
// class CPreviewDialog;
// }

class CPreviewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CPreviewDialog(QWidget *parent = nullptr);
    ~CPreviewDialog();
    void setCameraList(const QStringList &ips);
    // void setIpMap(QMap<QString,unsigned int> ipMap);

private slots:
    void onStart();
    void onStop();
    void grabFrame();          // 定时取帧

private:
    void closeEvent(QCloseEvent *event) override;
    void stopPreview();        // 内部统一停止

    Ui::CPreviewDialog *ui;
    QString      m_currentCamera{""};  // 当前打开的相机 IP
    QTimer   m_timer;
    // QMap<QString,unsigned int> m_ipMap;
};

#endif // CPREVIEWDIALOG_H
