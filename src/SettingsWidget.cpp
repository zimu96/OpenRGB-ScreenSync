#include "SettingsWidget.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QRadioButton>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>

SettingsWidget::SettingsWidget(OpenRGBPluginAPIInterface* a, FadeEngine* e, QWidget* parent) :
    QWidget(parent), api(a), engine(e)
{
    QVBoxLayout* root = new QVBoxLayout(this);

    chkEnabled = new QCheckBox("Spegni i LED quando lo schermo va in standby");
    root->addWidget(chkEnabled);

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
    vd->addWidget(rbAll); vd->addWidget(rbSel); vd->addWidget(list); vd->addWidget(btnRef, 0, Qt::AlignLeft);
    root->addWidget(gd, 1);

    QHBoxLayout* hb = new QHBoxLayout;
    btnTest = new QPushButton("Prova (spegni e riaccendi i LED)");
    lblStatus = new QLabel("Stato: in avvio...");
    hb->addWidget(btnTest); hb->addWidget(lblStatus, 1);
    root->addLayout(hb);

    // valori iniziali
    const ScreenSyncSettings& s = engine->settings();
    chkEnabled->setChecked(s.enabled);
    cmbOut->setCurrentIndex(s.fadeOutMs > 0 ? 0 : 1); spnOut->setValue(s.fadeOutMs > 0 ? s.fadeOutMs / 1000.0 : 2.0);
    cmbIn->setCurrentIndex(s.fadeInMs > 0 ? 0 : 1);   spnIn->setValue(s.fadeInMs > 0 ? s.fadeInMs / 1000.0 : 1.0);
    rbAll->setChecked(s.allDevices); rbSel->setChecked(!s.allDevices);
    refreshDevices();
    syncEnabledState();
    loading = false;

    auto chg = [this]{ collect(); };
    connect(chkEnabled, &QCheckBox::toggled, this, chg);
    connect(cmbOut, &QComboBox::currentIndexChanged, this, chg);
    connect(cmbIn,  &QComboBox::currentIndexChanged, this, chg);
    connect(spnOut, &QDoubleSpinBox::valueChanged, this, chg);
    connect(spnIn,  &QDoubleSpinBox::valueChanged, this, chg);
    connect(rbAll,  &QRadioButton::toggled, this, chg);
    connect(list,   &QListWidget::itemChanged, this, chg);
    connect(btnRef, &QPushButton::clicked, this, [this]{ loading = true; refreshDevices(); loading = false; });
    connect(btnTest, &QPushButton::clicked, engine, &FadeEngine::testCycle);
}

void SettingsWidget::setStatus(const QString& s) { lblStatus->setText("Stato: " + s); }

void SettingsWidget::refreshDevices()
{
    list->clear();
    const ScreenSyncSettings& s = engine->settings();
    for(RGBControllerInterface* c : api->GetRGBControllers())
    {
        QListWidgetItem* it = new QListWidgetItem(QString::fromStdString(c->GetName()), list);
        QString key = FadeEngine::deviceKey(c);
        it->setData(Qt::UserRole, key);
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState(s.selected.contains(key) ? Qt::Checked : Qt::Unchecked);
    }
}

void SettingsWidget::syncEnabledState()
{
    bool on = chkEnabled->isChecked();
    for(QWidget* w : std::initializer_list<QWidget*>{cmbOut, cmbIn, rbAll, rbSel, btnTest}) w->setEnabled(on);
    spnOut->setEnabled(on && cmbOut->currentIndex() == 0);
    spnIn->setEnabled(on && cmbIn->currentIndex() == 0);
    list->setEnabled(on && rbSel->isChecked());
}

void SettingsWidget::collect()
{
    if(loading) return;
    syncEnabledState();
    ScreenSyncSettings s;
    s.enabled    = chkEnabled->isChecked();
    s.fadeOutMs  = cmbOut->currentIndex() == 0 ? (int)(spnOut->value() * 1000) : 0;
    s.fadeInMs   = cmbIn->currentIndex()  == 0 ? (int)(spnIn->value()  * 1000) : 0;
    s.allDevices = rbAll->isChecked();
    for(int i = 0; i < list->count(); i++)
        if(list->item(i)->checkState() == Qt::Checked) s.selected << list->item(i)->data(Qt::UserRole).toString();
    // attenzione: se l'elenco e' stato aggiornato, mantieni anche le scelte di dispositivi non presenti
    for(const QString& k : engine->settings().selected)
    {
        bool present = false;
        for(int i = 0; i < list->count(); i++) if(list->item(i)->data(Qt::UserRole).toString() == k) present = true;
        if(!present) s.selected << k;
    }
    emit settingsChanged(s);
}
