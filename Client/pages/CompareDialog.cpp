#include "CompareDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QPixmap>
#include <QGridLayout>
#include <QHeaderView>
#include <QApplication>

CompareDialog::CompareDialog(const QList<CompareCarriageItem>& carriages,
                             QNetworkAccessManager* nam,
                             QWidget* parent)
    : QDialog(parent)
    , m_carriages(carriages)
    , m_nam(nam)
{
    setWindowTitle("多图对比");
    resize(1000, 700);
    setLayout(new QVBoxLayout);

    // 顶部控制栏
    QWidget* ctrlBar = new QWidget(this);
    ctrlBar->setLayout(new QHBoxLayout);
    layout()->addWidget(ctrlBar);

    QLabel* viewLabel = new QLabel("选择视角:", ctrlBar);
    ctrlBar->layout()->addWidget(viewLabel);

    m_viewCombo = new QComboBox(ctrlBar);
    m_viewCombo->addItems({"左侧视图", "右侧视图", "顶部视图"});
    m_viewCombo->setFixedWidth(120);
    ctrlBar->layout()->addWidget(m_viewCombo);

    QLabel* infoLabel = new QLabel(
        QString("共 %1 节车厢，显示前 4 节").arg(carriages.size()), ctrlBar);
    ctrlBar->layout()->addWidget(infoLabel);
    qobject_cast<QHBoxLayout*>(ctrlBar->layout())->addStretch();

    QPushButton* closeBtn = new QPushButton("关闭", ctrlBar);
    closeBtn->setFixedWidth(80);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    ctrlBar->layout()->addWidget(closeBtn);

    // 图片网格：2行 x 2列
    QWidget* gridWidget = new QWidget(this);
    m_gridLayout = new QGridLayout(gridWidget);
    m_gridLayout->setSpacing(8);
    m_gridLayout->setContentsMargins(10, 10, 10, 10);
    gridWidget->setLayout(m_gridLayout);
    qobject_cast<QVBoxLayout*>(layout())->addWidget(gridWidget, 1);

    // 创建4个QLabel占位
    const int MAX_SHOW = 4;
    for (int i = 0; i < MAX_SHOW; ++i) {
        QLabel* lbl = new QLabel(gridWidget);
        lbl->setMinimumSize(300, 200);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet("background-color: #1a1a1a; color: #4a4a4a; border: 1px solid #333;");
        lbl->setText(QString("车厢 %1").arg(i < carriages.size() ? carriages[i].number : 0));
        m_gridLayout->addWidget(lbl, i / 2, i % 2);
        m_imageLabels.append(lbl);
    }

    connect(m_viewCombo, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &CompareDialog::onViewChanged);

    setStyleSheet(R"(
        QDialog { background-color: #1e1e1e; }
        QLabel { color: #cccccc; font-size: 13px; }
        QComboBox { background-color: #2d2d2d; color: #cccccc; border: 1px solid #444; padding: 4px; }
    )");

    loadImages();
}

void CompareDialog::onViewChanged(int)
{
    loadImages();
}

void CompareDialog::loadImages()
{
    int viewIndex = m_viewCombo->currentIndex();  // 0=left, 1=right, 2=top
    const int MAX_SHOW = qMin(4, m_carriages.size());

    for (int i = 0; i < MAX_SHOW; ++i) {
        QString url;
        if (viewIndex == 0) url = m_carriages[i].leftImageUrl;
        else if (viewIndex == 1) url = m_carriages[i].rightImageUrl;
        else url = m_carriages[i].topImageUrl;

        QLabel* lbl = m_imageLabels[i];
        lbl->setText(QString("车厢 %1\n加载中...").arg(m_carriages[i].number));
        lbl->setStyleSheet("background-color: #1a1a1a; color: #888; border: 1px solid #333;");
        loadImageFromHttp(url, lbl);
    }

    // 清空未使用的槽
    for (int i = MAX_SHOW; i < m_imageLabels.size(); ++i) {
        m_imageLabels[i]->setText("—");
        m_imageLabels[i]->setStyleSheet("background-color: #1a1a1a; color: #4a4a4a; border: 1px solid #333;");
    }
}

void CompareDialog::loadImageFromHttp(const QString& url, QLabel* label)
{
    if (url.isEmpty()) {
        label->setText("无图片");
        return;
    }
    QString fullUrl = (url.startsWith("http")) ? url : "http://localhost:8080" + url;
    QNetworkRequest req{QUrl(fullUrl)};
    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [=]() {
        if (reply->error() != QNetworkReply::NoError) {
            label->setText("加载失败");
            label->setStyleSheet("background-color: #2a1a1a; color: #e75d55; border: 1px solid #e75d55;");
            reply->deleteLater();
            return;
        }
        QPixmap pix;
        pix.loadFromData(reply->readAll());
        if (!pix.isNull()) {
            label->setPixmap(pix.scaled(label->width(), label->height(),
                                        Qt::KeepAspectRatio, Qt::SmoothTransformation));
            label->setText("");
        } else {
            label->setText("图片损坏");
        }
        reply->deleteLater();
    });
}
