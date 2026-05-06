#ifndef HOMEPAGE_H
#define HOMEPAGE_H

#include <QWidget>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QComboBox>
#include <QDate>
#include "../network/TrainApi.h"

class TrainApiClient;

// ============================================================
// 日期选择器（年月日三级联动下拉）
// ============================================================
class DateSelector : public QWidget
{
    Q_OBJECT
public:
    explicit DateSelector(QWidget* parent = nullptr);
    // 返回选中的日期字符串 YYYY-MM-DD
    QString currentDate() const { return m_currentDate; }

signals:
    void dateChanged(const QString& date);  // YYYY-MM-DD 格式

public slots:
    void setDate(const QDate& date);   // 设为指定日期

private slots:
    void onYearChanged(int year);
    void onMonthChanged(int month);
    void onDayChanged(int day);

private:
    void populateYearItems();
    void populateMonthItems();
    void populateDayItems();
    void refreshDayItems();
    QString makeDateStr() const;

    QComboBox* m_yearBox;
    QComboBox* m_monthBox;
    QComboBox* m_dayBox;

    QDate m_current;        // 当前选中日期
    QString m_currentDate;  // "YYYY-MM-DD" 格式

    bool m_updatingDays = false;
};

// ============================================================
// 首页
// ============================================================
class HomePage : public QWidget
{
    Q_OBJECT
public:
    explicit HomePage(QWidget* parent = nullptr);
    ~HomePage() override;

signals:
    void inspectTrain(const QString& trainNumber, quint64 reachDatetime,
                      const QString& direction, const QString& detectionStation,
                      int numCarriages, int numAxles,
                      const QString& inspectionStatus, quint64 inspectionDatetime,
                      const QString& leftVideoPath, const QString& rightVideoPath, const QString& topVideoPath);

public slots:
    void onQueryClicked();

private slots:
    void onTrainDoubleClicked(int row, int col);
    void onApiTrainsReady(const QList<TrainInfo2>& trains);
    void onApiError(const QString& err);
    void onRangeModeToggled(bool checked);
    void onFilterChanged(int filter);
    void onStatsReady(int todayCount, int pendingCount, int alarmCount);

private:
    void setupUi();
    void loadStats(const QString& date);
    void queryTrains();
    void applyFilter();

    // 日期选择
    QRadioButton* m_singleDateRadio;
    QRadioButton* m_rangeRadio;
    QWidget* m_rangePanel;        // 区间选择面板（截止日期）
    DateSelector* m_startDateSelector;
    DateSelector* m_endDateSelector;
    QPushButton* m_queryBtn;

    // 统计卡片
    QLabel* m_todayTrainCount;
    QLabel* m_pendingCount;
    QLabel* m_alarmCount;

    // 状态筛选
    QList<TrainInfo2> m_allTrains;
    int m_currentFilter = 0; // 0=全部, 1=已检视, 2=未检视, 3=部分检视
    QWidget* m_filterBar = nullptr;
    QList<QPushButton*> m_filterBtns;

    // 列车表格
    QTableWidget* m_trainTable;
    QLabel* m_resultLabel;

    // 检视按钮（每行一个）
    void addInspectButton(int row, const TrainInfo2& train);

    TrainApiClient* m_api;
};

#endif // HOMEPAGE_H
