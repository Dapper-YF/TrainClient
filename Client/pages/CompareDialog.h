#ifndef COMPAREDIALOG_H
#define COMPAREDIALOG_H

#include <QDialog>
#include <QList>
#include <QMap>
#include <QNetworkAccessManager>

class QLabel;
class QComboBox;
class QPushButton;
class QGridLayout;

// 车厢信息（简化版，只含图片URL）
struct CompareCarriageItem {
    int number = 0;
    QString leftImageUrl;
    QString rightImageUrl;
    QString topImageUrl;
};

// 多图对比对话框
class CompareDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CompareDialog(const QList<CompareCarriageItem>& carriages,
                          QNetworkAccessManager* nam,
                          QWidget* parent = nullptr);

private slots:
    void onViewChanged(int index);
    void loadImages();

private:
    void loadImageFromHttp(const QString& url, QLabel* label);

    QList<CompareCarriageItem> m_carriages;
    QNetworkAccessManager* m_nam;
    QComboBox* m_viewCombo = nullptr;
    QGridLayout* m_gridLayout = nullptr;
    QList<QLabel*> m_imageLabels;  // 最多显示前4个
};

#endif  // COMPAREDIALOG_H
