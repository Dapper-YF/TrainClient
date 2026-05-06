#ifndef CCONFIGDIALOG_H
#define CCONFIGDIALOG_H

#include <QDialog>
#include <QComboBox>
#include "MVS/MvCamera.h"
#include "csystemconfig.h"
#include <QMap>
namespace Ui {
class CConfigDialog;
}

class CConfigDialog : public QDialog
{
    Q_OBJECT

public:

    explicit CConfigDialog(QWidget *parent = nullptr);
    ~CConfigDialog();

    // Sets the UI controls from a configuration struct
    void setConfiguration(const CSystemConfig& config);

    // Gets the current settings from UI controls into a configuration struct
    CSystemConfig getConfiguration() const;

public slots:
    void onBtnEnumClicked();
    void accept();
    void selectImageFolder();
    void selectVideoFolder();
    void SaveLineImageChecked(int state);
    void SaveAreaVideoChecked(int);
    void getParameters();
    void setParameters();
    void on_pushButtonPreview_clicked();
    void selectTrainImagesSavePath();
    void camera_select_area(QString curText);
    void camera_select_line(QString curText);
private:
    Ui::CConfigDialog *ui;

    MV_CC_DEVICE_INFO_LIST m_stDevList{};
    void enableControls(bool enable);
    void showErrorMsg(const QString &msg);
    void initUI(); // Helper to initialize UI elements
    // QMap<QString,unsigned int> m_ipMap;
};

#endif // CCONFIGDIALOG_H
