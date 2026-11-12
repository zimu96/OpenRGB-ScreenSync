#pragma once
#include <QWidget>
#include "FadeEngine.h"

class QCheckBox; class QComboBox; class QDoubleSpinBox; class QRadioButton; class QListWidget; class QLabel; class QPushButton; class QSpinBox;

class SettingsWidget : public QWidget
{
    Q_OBJECT
public:
    SettingsWidget(OpenRGBPluginAPIInterface* api, FadeEngine* engine, QWidget* parent = nullptr);
public slots:
    void setStatus(const QString& s);
    void refreshDevicesAuto();      // chiamata quando cambia l'elenco dei dispositivi
signals:
    void settingsChanged(const ScreenSyncSettings& s);
private:
    void refreshDevices();
    void collect();
    void syncEnabledState();
    OpenRGBPluginAPIInterface* api;
    FadeEngine*     engine;
    bool            loading = true;
    QCheckBox*      chkEnabled;
    QCheckBox*      chkShutdown;
    QCheckBox*      chkAudio;
    QRadioButton   *rbAudAll, *rbAudSel;
    QListWidget*    listAudio;
    QSpinBox*       spnHold;
    QPushButton*    btnRescan;
    QComboBox       *cmbOut, *cmbIn;
    QDoubleSpinBox  *spnOut, *spnIn;
    QRadioButton    *rbAll, *rbSel;
    QListWidget*    list;
    QLabel*         lblStatus;
    QPushButton*    btnTest;
};
