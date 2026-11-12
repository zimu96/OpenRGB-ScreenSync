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
    bool        offOnShutdown = false;   // spegni subito i LED quando il PC si spegne (non al riavvio)
    bool        allDevices = true;
    QStringList selected;           // chiavi "nome|posizione"
    bool        keepOnAudio = false;     // con lo schermo spento, tieni accesi i LED finche' c'e' audio
    bool        keepAll = true;          // ...tutti i dispositivi scelti sopra
    int         keepHoldMs = 8000;      // attesa dopo la fine dell'audio prima di spegnere
    QStringList keepSelected;           // ...oppure solo questi (chiavi "nome|posizione")
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
    void shutdownOff();      // il PC si spegne: LED spenti di colpo e niente ripristino
    void cancelShutdown();   // spegnimento annullato: ripristina
    void setAudioActive(bool on);   // c'e' audio in uscita dal PC

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
        bool keep;           // resta acceso se c'e' audio
        double lvl;          // luminosita' attuale (0..1)
        double tgt;          // luminosita' da raggiungere
        bool offDone;        // spegnimento completato
    };
    bool  isKeep(RGBControllerInterface* c) const;
    void  updateTargets();
    void  applyDev(Dev& d, double lv);
    void  finalizeDev(Dev& d);
    bool  isSelected(RGBControllerInterface* c) const;
    void  takeSnapshot();
    void  tick();
    void  apply(double level);
    void  restoreExact();
    void  writeOriginal(Dev& d);
    bool  stillExists(RGBControllerInterface* c) const;

    OpenRGBPluginAPIInterface* api;
    ScreenSyncSettings cfg;
    std::vector<Dev>   devs;
    bool               snap = false;
    QTimer             timer;
    QElapsedTimer      clock;
    bool               screenOn = true;
    bool               audio = false;
    bool               shuttingDown = false;
    QElapsedTimer      total;
};
