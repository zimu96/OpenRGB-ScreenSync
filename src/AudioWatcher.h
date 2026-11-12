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
    void setEnabled(bool on);       // attiva/disattiva il controllo (a riposo non consuma nulla)
    void setHoldMs(int ms);         // attesa dopo la fine dell'audio prima di dichiarare il silenzio
    bool active() const { return playing; }
signals:
    void audioChanged(bool playing);
private:
    void poll();
    void result(bool running);
    QTimer        timer;
    QProcess*     proc = nullptr;
    QElapsedTimer lastRunning;
    bool          seen = false;
    bool          playing = false;
    int           holdMs = 8000;
};
