#ifndef VIDEOPLAYBACKDIALOG_H
#define VIDEOPLAYBACKDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QAxWidget>
#include <QUrl>
#include <QTimer>

class VideoPlaybackDialog : public QDialog
{
    Q_OBJECT
public:
    explicit VideoPlaybackDialog(QWidget* parent = nullptr);
    ~VideoPlaybackDialog();

    bool eventFilter(QObject* obj, QEvent* event) override;

    void loadVideos(const QString& leftUrl, const QString& topUrl, const QString& rightUrl);

private slots:
    void onCloseClicked();
    void onPlayPauseClicked();

private:
    QAxWidget* m_leftPlayer = nullptr;
    QAxWidget* m_topPlayer = nullptr;
    QAxWidget* m_rightPlayer = nullptr;
    bool m_isPlaying = false;
    QTimer* m_loopTimer = nullptr;
    int m_lastLeftState = -1;
    QString m_leftUrl;
    QString m_topUrl;
    QString m_rightUrl;

    bool m_dragging = false;
    QPoint m_dragPos;
};

#endif // VIDEOPLAYBACKDIALOG_H
