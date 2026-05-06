#ifndef INSPECTIONWINDOW_H
#define INSPECTIONWINDOW_H

#include <QDialog>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QTableWidget>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QJsonArray>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QButtonGroup>
#include <QMap>
#include <QVector>
#include "VideoPlaybackDialog.h"

// 车厢数据结构（对应后端 GET /api/carriages 返回）
struct CarriageInfo3 {
    int id = 0;
    int nOrder = 0;
    QString strWagonNumber;
    QString strCarriageModel;
    double fConverredLength = 0;
    QString strCarriageModelCode;
    QString strFactoryAndDatetime;
    double frontWheelSpeed = 0;
    double rearWheelSpeed = 0;
    QString leftImagePath;
    QString topImagePath;
    QString rightImagePath;
    int leftImagePass = 0;
    int topImagePass = 0;
    int rightImagePass = 0;
    int inspectorState = 0;
};

// 列车简略信息（用于车次选择器）
struct TrainBriefInfo {
    QString number;
    quint64 reachDatetime = 0;
};

// 三视图图片缓存结构
struct TriViewPixmaps {
    QPixmap left;
    QPixmap top;
    QPixmap right;
    bool valid = false;
};

class InspectionWindow : public QDialog
{
    Q_OBJECT
public:
    InspectionWindow(const QString& trainNumber, quint64 reachDatetime,
                     const QString& direction, const QString& detectionStation,
                     int numCarriages, int numAxles,
                     const QString& inspectionStatus, quint64 inspectionDatetime,
                     const QString& leftVideoPath = QString(),
                     const QString& rightVideoPath = QString(),
                     const QString& topVideoPath = QString(),
                     QWidget* parent = nullptr);
    ~InspectionWindow();

    bool eventFilter(QObject* obj, QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

signals:
    void inspectionCompleted(const QString& trainNumber, int passCount, int failCount);

private slots:
    void onCloseClicked();
    void onPlayStopToggled();
    void onCarriageSelected(int row, int col);
    void onCarriageResultChanged(int row, bool passed);
    void onAutoInspectClicked();
    void onAutoInspectTimer();
    void onPlayTimerTick();
    void stopPlayAnimation();
    void onPrevFrame();
    void onNextFrame();
    void onFirstFrame();
    void onLastFrame();
    void onSpeedChanged(int index);
    void onTrainComboChanged(int index);
    void onVideoPlaybackClicked();

private:
    void setupUi();
    void loadCarriages();
    void loadTrainList();
    void populateCarriageTable();
    void updateCarriageRow(int row, const CarriageInfo3& c);
    void syncLeftToCarriage(int row);
    void preloadCarriagePixmaps(int row);
    void setPixmapOnLabel(QLabel* lbl, const QPixmap& pm);
    QString colorForCarriage(const CarriageInfo3& c) const;
    void updateCarriageApi(int row, bool passed);
    void updateProgressLabels();
    void animateCrossfadeCached(int fromRow, int toRow);

    // 左半部分
    QWidget* m_leftPanel;
    QLabel* m_trainInfoLabel;       // 第一行：车次 + 到达时间 + 方向 + 检测站
    QLabel* m_trainInfoSubLabel;    // 第二行：车厢数 + 轴数 + 检视状态
    QComboBox* m_trainCombo;        // 车次切换下拉框

    // 三视图——每个视图两个重叠标签（prev=老图/outgoing, cur=当前图/incoming）
    // 左视图
    QLabel* m_leftImageLabel;   // 当前/新图（上方）
    QLabel* m_prevLeftLabel;    // 旧图（下方，淡出用）
    QWidget* m_leftContainer;   // 使用QGridLayout同一单元格重叠
    QLabel* m_leftTitleLabel;
    // 顶视图
    QLabel* m_topImageLabel;
    QLabel* m_prevTopLabel;
    QWidget* m_topContainer;
    QLabel* m_topTitleLabel;
    // 右视图
    QLabel* m_rightImageLabel;
    QLabel* m_prevRightLabel;
    QWidget* m_rightContainer;
    QLabel* m_rightTitleLabel;

    // 播放控制
    QPushButton* m_playStopBtn;
    QPushButton* m_prevBtn;
    QPushButton* m_nextBtn;
    QComboBox* m_speedCombo;    // 播放速度选择

    // 检视状态标签
    QLabel* m_inspectStatusLabel = nullptr;
    QLabel* m_passCountLabel = nullptr;
    QLabel* m_failCountLabel = nullptr;
    QLabel* m_progressLabel = nullptr;

    // 右半部分
    QWidget* m_rightPanel;
    QPushButton* m_closeBtn;
    QTableWidget* m_carriageTable;

    // 状态
    QList<CarriageInfo3> m_carriages;
    QList<TrainBriefInfo> m_trainList;     // 当日列车列表
    int m_currentRow = 0;        // 当前选中的车厢（显示/编辑）
    int m_playRow = 0;           // 播放进度（车厢索引）
    int m_nextPlayRow = 0;       // 下一帧要播放的车厢（预加载用）
    bool m_isPlaying = false;
    QTimer* m_playTimer;
    QTimer* m_autoInspectTimer;
    bool m_autoInspecting = false;
    int m_autoInspectPassCount = 0;
    int m_autoInspectFailCount = 0;

    // 图片缓存（预加载用）
    QMap<int, TriViewPixmaps> m_pixmapCache;

    // 动画滤镜（淡入淡出）
    QGraphicsOpacityEffect* m_fadeOutEffect = nullptr;
    QGraphicsOpacityEffect* m_fadeInEffect = nullptr;

    // 网络
    QNetworkAccessManager* m_nam;
    QString m_token;
    QString m_apiBase;
    QString m_currentTrainNumber;
    quint64 m_currentReachDatetime;
    QString m_leftVideoPath;
    QString m_rightVideoPath;
    QString m_topVideoPath;
    QString m_direction;
    QString m_detectionStation;
    int m_numCarriages = 0;
    int m_numAxles = 0;
    QString m_inspectionStatus;
    quint64 m_inspectionDatetime = 0;

    // 行内按钮组（用于合格/不合格互斥）
    QMap<int, QButtonGroup*> m_rowButtonGroups;

    bool m_dragging = false;
    QPoint m_dragPos;
    QWidget* m_windowTitleBar = nullptr;
};

#endif // INSPECTIONWINDOW_H
