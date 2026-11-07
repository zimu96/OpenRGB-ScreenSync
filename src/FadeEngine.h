// Dissolvenza dei LED di tutti i dispositivi OpenRGB (o di quelli scelti).
#pragma once
#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QStringList>
#include <vector>
#include "OpenRGBPluginInterface.h"

struct ScreenSyncSettings
{
    bool        enabled   = true;
    int         fadeOutMs = 2000;   // 0 = di colpo
    int         fadeInMs  = 1000;   // 0 = di colpo
    bool        allDevices = true;
    QStringList selected;           // chiavi "nome|posizione"
};

class FadeEngine : public QObject
{
    Q_OBJECT
public:
    FadeEngine(OpenRGBPluginAPIInterface* api, QObject* parent = nullptr);
    void setSettings(const ScreenSyncSettings& s);
    const ScreenSyncSettings& settings() const { return cfg; }
    static QString deviceKey(RGBControllerInterface* c);

public slots:
    void setScreenOn(bool on);
    void testCycle();        // spegne e riaccende (prova)
    void restoreNow();       // ripristina subito tutto (chiusura plugin)

private:
    struct Dev
    {
        RGBControllerInterface* c;
        int  kind;           // 0 = colori per LED, 1 = luminosita' del modo, 2 = solo cambio modo
        int  origMode;
        int  offMode;        // modo per-LED per il nero (-1 se non esiste)
        std::vector<RGBColor> colors;
        unsigned origBright, minBright;
        bool switched;
        qint64 lastMs;       // ultimo invio (per limitare la frequenza)
        int  minGapMs;       // intervallo minimo tra due invii
    };
    bool  isSelected(RGBControllerInterface* c) const;
    void  takeSnapshot();
    void  tick();
    void  apply(double level);
    void  finalizeOff();
    void  restoreExact();
    void  writeOriginal(Dev& d);
    bool  stillExists(RGBControllerInterface* c) const;

    OpenRGBPluginAPIInterface* api;
    ScreenSyncSettings cfg;
    std::vector<Dev>   devs;
    bool               snap = false;
    double             level = 1.0;
    double             target = 1.0;
    QTimer             timer;
    QElapsedTimer      clock;
    bool               offApplied = false;
    QElapsedTimer      total;
};
