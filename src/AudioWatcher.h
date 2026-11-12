// Rileva se dal PC esce audio (casse, HDMI, Bluetooth... qualunque uscita di PipeWire/PulseAudio).
// Controlla lo stato delle uscite audio ("RUNNING" = c'e' un flusso attivo).
#pragma once
#include <QObject>
#include <QTimer>
#include <QElapsedTimer>

class QProcess;

class AudioWatcher : public QObject
{
    Q_OBJECT
public:
    explicit AudioWatcher(QObject* parent = nullptr);
    void setEnabled(bool on);       // opzione "tieni accesi con audio" attiva/disattiva
    void setScreenOn(bool on);      // il controllo gira solo con lo schermo spento (unico caso in cui serve)
    void setHoldMs(int ms);         // attesa dopo la fine dell'audio prima di dichiarare il silenzio
    bool active() const { return playing; }
signals:
    void audioChanged(bool playing);
private:
    void poll();
    void apply();                   // avvia/ferma il controllo in base a opzione e stato dello schermo
    void result(bool running);
    void probe(QStringList mons, int idx);
    QTimer        timer;
    QProcess*     proc = nullptr;
    QElapsedTimer lastRunning;
    bool          seen = false;
    bool          busy = false;
    bool          playing = false;
    bool          wanted = false;   // opzione attiva
    bool          screenOn = true;  // schermo acceso: nessun controllo
    int           holdMs = 8000;
};
