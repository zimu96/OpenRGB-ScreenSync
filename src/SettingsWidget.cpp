#include "SettingsWidget.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QRadioButton>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QTimer>

SettingsWidget::SettingsWidget(OpenRGBPluginAPIInterface* a, FadeEngine* e, QWidget* parent) :
    QWidget(parent), api(a), engine(e)
{
    QVBoxLayout* root = new QVBoxLayout(this);

    chkEnabled = new QCheckBox("Spegni i LED quando lo schermo va in standby");
    root->addWidget(chkEnabled);

    chkShutdown = new QCheckBox("Spegni subito i LED quando spengo il PC (non vale per il riavvio)");
    root->addWidget(chkShutdown);

    QGroupBox* gb = new QGroupBox("Come si spengono e si accendono i LED");
    QFormLayout* fl = new QFormLayout(gb);
    auto row = [&](QComboBox*& cmb, QDoubleSpinBox*& spn)
    {
        QWidget* w = new QWidget; QHBoxLayout* h = new QHBoxLayout(w); h->setContentsMargins(0,0,0,0);
        cmb = new QComboBox; cmb->addItems({"Dissolvenza graduale", "Di colpo"});
        spn = new QDoubleSpinBox; spn->setRange(0.1, 30.0); spn->setSingleStep(0.1); spn->setDecimals(1); spn->setSuffix(" secondi");
        h->addWidget(cmb); h->addWidget(spn); h->addStretch();
        return w;
    };
    fl->addRow("Quando lo schermo si SPEGNE:", row(cmbOut, spnOut));
    fl->addRow("Quando lo schermo si ACCENDE:", row(cmbIn, spnIn));
    root->addWidget(gb);

    QGroupBox* gd = new QGroupBox("Dispositivi (tutti si comportano allo stesso modo)");
    QVBoxLayout* vd = new QVBoxLayout(gd);
    rbAll = new QRadioButton("Tutti i dispositivi OpenRGB");
    rbSel = new QRadioButton("Solo quelli selezionati qui sotto:");
    list  = new QListWidget;
    QPushButton* btnRef = new QPushButton("Aggiorna elenco");
    btnRescan = new QPushButton("Riscansiona dispositivi (cerca quelli nuovi)");
    vd->addWidget(rbAll); vd->addWidget(rbSel); vd->addWidget(list);
    QHBoxLayout* hr = new QHBoxLayout; hr->addWidget(btnRef); hr->addWidget(btnRescan); hr->addStretch();
    vd->addLayout(hr);
    root->addWidget(gd, 1);

    QGroupBox* ga = new QGroupBox("Audio");
    QVBoxLayout* va = new QVBoxLayout(ga);
    chkAudio = new QCheckBox("Con lo schermo spento, tieni accesi i LED finche' dal PC esce dell'audio\n"
                             "(casse, uscita frontale/posteriore, HDMI, Bluetooth, audio dal telefono...)");
    rbAudAll = new QRadioButton("Tutti i dispositivi scelti sopra");
    rbAudSel = new QRadioButton("Solo questi:");
    listAudio = new QListWidget;
    listAudio->setMaximumHeight(130);
    spnHold = new QSpinBox; spnHold->setRange(1, 120); spnHold->setSuffix(" secondi");
    QWidget* hw = new QWidget; QHBoxLayout* hh = new QHBoxLayout(hw); hh->setContentsMargins(0,0,0,0);
    hh->addWidget(new QLabel("Dopo la fine dell'audio aspetta prima di spegnere (pausa tra due brani):")); hh->addWidget(spnHold); hh->addStretch();
    va->addWidget(chkAudio); va->addWidget(hw);
    va->addWidget(new QLabel("Se il volume e' a zero o l'uscita e' muta, l'audio non conta."));
    va->addWidget(rbAudAll); va->addWidget(rbAudSel); va->addWidget(listAudio);
    root->addWidget(ga);

    QHBoxLayout* hb = new QHBoxLayout;
    btnTest = new QPushButton("Prova (spegni e riaccendi i LED)");
    lblStatus = new QLabel("Stato: in avvio...");
    hb->addWidget(btnTest); hb->addWidget(lblStatus, 1);
    root->addLayout(hb);

    // valori iniziali
    const ScreenSyncSettings& s = engine->settings();
    chkEnabled->setChecked(s.enabled);
    chkShutdown->setChecked(s.offOnShutdown);
    cmbOut->setCurrentIndex(s.fadeOutMs > 0 ? 0 : 1); spnOut->setValue(s.fadeOutMs > 0 ? s.fadeOutMs / 1000.0 : 2.0);
    cmbIn->setCurrentIndex(s.fadeInMs > 0 ? 0 : 1);   spnIn->setValue(s.fadeInMs > 0 ? s.fadeInMs / 1000.0 : 1.0);
    rbAll->setChecked(s.allDevices); rbSel->setChecked(!s.allDevices);
    chkAudio->setChecked(s.keepOnAudio);
    spnHold->setValue(s.keepHoldMs / 1000);
    rbAudAll->setChecked(s.keepAll); rbAudSel->setChecked(!s.keepAll);
    refreshDevices();
    syncEnabledState();
    loading = false;

    auto chg = [this]{ collect(); };
    connect(chkEnabled, &QCheckBox::toggled, this, chg);
    connect(chkShutdown, &QCheckBox::toggled, this, chg);
    connect(cmbOut, &QComboBox::currentIndexChanged, this, chg);
    connect(cmbIn,  &QComboBox::currentIndexChanged, this, chg);
    connect(spnOut, &QDoubleSpinBox::valueChanged, this, chg);
    connect(spnIn,  &QDoubleSpinBox::valueChanged, this, chg);
    connect(rbAll,  &QRadioButton::toggled, this, chg);
    connect(list,   &QListWidget::itemChanged, this, chg);
    connect(chkAudio, &QCheckBox::toggled, this, chg);
    connect(spnHold, &QSpinBox::valueChanged, this, chg);
    connect(rbAudAll, &QRadioButton::toggled, this, chg);
    connect(listAudio, &QListWidget::itemChanged, this, chg);
    connect(btnRef, &QPushButton::clicked, this, [this]{ loading = true; refreshDevices(); loading = false; });
    connect(btnTest, &QPushButton::clicked, engine, &FadeEngine::testCycle);
    connect(btnRescan, &QPushButton::clicked, this, [this]{
        lblStatus->setText("Stato: riscansione in corso...");
        QTimer::singleShot(50, this, [this]{ this->api->RescanDevices(); });
    });
}

void SettingsWidget::refreshDevicesAuto()
{
    // elenco aggiornato da solo quando OpenRGB trova o perde dispositivi
    bool was = loading;
    loading = true; refreshDevices(); loading = was;
}

void SettingsWidget::setStatus(const QString& s) { lblStatus->setText("Stato: " + s); }

void SettingsWidget::refreshDevices()
{
    list->clear(); listAudio->clear();
    const ScreenSyncSettings& s = engine->settings();
    for(RGBControllerInterface* c : api->GetRGBControllers())
    {
        QString key = FadeEngine::deviceKey(c);
        QListWidgetItem* it = new QListWidgetItem(QString::fromStdString(c->GetName()), list);
        it->setData(Qt::UserRole, key);
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState(s.selected.contains(key) ? Qt::Checked : Qt::Unchecked);
        QListWidgetItem* ia = new QListWidgetItem(QString::fromStdString(c->GetName()), listAudio);
        ia->setData(Qt::UserRole, key);
        ia->setFlags(ia->flags() | Qt::ItemIsUserCheckable);
        ia->setCheckState(s.keepSelected.contains(key) ? Qt::Checked : Qt::Unchecked);
    }
}

void SettingsWidget::syncEnabledState()
{
    bool on = chkEnabled->isChecked();
    for(QWidget* w : std::initializer_list<QWidget*>{cmbOut, cmbIn, rbAll, rbSel, btnTest}) w->setEnabled(on);
    spnOut->setEnabled(on && cmbOut->currentIndex() == 0);
    spnIn->setEnabled(on && cmbIn->currentIndex() == 0);
    list->setEnabled(on && rbSel->isChecked());
    chkAudio->setEnabled(on);
    bool au = on && chkAudio->isChecked();
    spnHold->setEnabled(au);
    rbAudAll->setEnabled(au); rbAudSel->setEnabled(au);
    listAudio->setEnabled(au && rbAudSel->isChecked());
}

void SettingsWidget::collect()
{
    if(loading) return;
    syncEnabledState();
    ScreenSyncSettings s;
    s.enabled    = chkEnabled->isChecked();
    s.offOnShutdown = chkShutdown->isChecked();
    s.fadeOutMs  = cmbOut->currentIndex() == 0 ? (int)(spnOut->value() * 1000) : 0;
    s.fadeInMs   = cmbIn->currentIndex()  == 0 ? (int)(spnIn->value()  * 1000) : 0;
    s.allDevices = rbAll->isChecked();
    for(int i = 0; i < list->count(); i++)
        if(list->item(i)->checkState() == Qt::Checked) s.selected << list->item(i)->data(Qt::UserRole).toString();
    s.keepOnAudio = chkAudio->isChecked();
    s.keepAll     = rbAudAll->isChecked();
    s.keepHoldMs  = spnHold->value() * 1000;
    for(int i = 0; i < listAudio->count(); i++)
        if(listAudio->item(i)->checkState() == Qt::Checked) s.keepSelected << listAudio->item(i)->data(Qt::UserRole).toString();
    for(const QString& k : engine->settings().keepSelected)
    {
        bool present = false;
        for(int i = 0; i < listAudio->count(); i++) if(listAudio->item(i)->data(Qt::UserRole).toString() == k) present = true;
        if(!present) s.keepSelected << k;
    }
    // attenzione: se l'elenco e' stato aggiornato, mantieni anche le scelte di dispositivi non presenti
    for(const QString& k : engine->settings().selected)
    {
        bool present = false;
        for(int i = 0; i < list->count(); i++) if(list->item(i)->data(Qt::UserRole).toString() == k) present = true;
        if(!present) s.selected << k;
    }
    emit settingsChanged(s);
}
