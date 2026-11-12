#include "AudioWatcher.h"
#include <QProcess>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QProcessEnvironment>
#include <QRegularExpression>

// Metodo universale (nessun programma richiesto): il kernel ALSA espone lo stato di ogni uscita
// in /proc/asound/card*/pcm*p/sub*/status ("state: RUNNING" = sta riproducendo).
static bool alsaRunning()
{
    QDir cards("/proc/asound");
    for(const QString& card : cards.entryList({"card*"}, QDir::Dirs | QDir::NoDotAndDotDot))
    {
        QDir c(cards.filePath(card));
        for(const QString& pcm : c.entryList({"pcm*p"}, QDir::Dirs | QDir::NoDotAndDotDot))
        {
            QDir p(c.filePath(pcm));
            for(const QString& sub : p.entryList({"sub*"}, QDir::Dirs | QDir::NoDotAndDotDot))
            {
                QFile f(p.filePath(sub) + "/status");
                if(f.open(QIODevice::ReadOnly) && f.readAll().contains("state: RUNNING")) return true;
            }
        }
    }
    return false;
}


AudioWatcher::AudioWatcher(QObject* parent) : QObject(parent)
{
    timer.setInterval(1500);
    connect(&timer, &QTimer::timeout, this, &AudioWatcher::poll);
}

void AudioWatcher::setEnabled(bool on)
{
    if(on) { if(!timer.isActive()) { timer.start(); poll(); } }
    else
    {
        timer.stop();
        if(proc) { proc->disconnect(this); proc->kill(); proc->deleteLater(); proc = nullptr; }
        seen = false;
        if(playing) { playing = false; emit audioChanged(false); }
    }
}

void AudioWatcher::setHoldMs(int ms) { holdMs = ms; }

void AudioWatcher::poll()
{
    if(proc) return;                        // controllo precedente ancora in corso
    // PipeWire/PulseAudio (copre anche Bluetooth e uscite di rete); senza pactl si usa ALSA
    if(QStandardPaths::findExecutable("pactl").isEmpty()) { result(alsaRunning()); return; }
    proc = new QProcess(this);
    connect(proc, &QProcess::finished, this, [this]{
        QProcess* p = proc; proc = nullptr;
        bool running = false;
        // Output di "pactl list sinks" (con LC_ALL=C, quindi non tradotto): un blocco per uscita.
        // Conta solo l'uscita in RUNNING che non e' muta e ha il volume sopra lo zero.
        static const QRegularExpression pct("(\\d+)%");
        bool isRunning = false, muted = false, loud = false, inSink = false;
        auto closeSink = [&]{ if(inSink && isRunning && !muted && loud) running = true; };
        for(const QByteArray& raw : p->readAllStandardOutput().split('\n'))
        {
            const QString line = QString::fromUtf8(raw);
            if(line.startsWith("Sink #") ) { closeSink(); inSink = true; isRunning = muted = loud = false; continue; }
            const QString t = line.trimmed();
            if(t.startsWith("State:"))      isRunning = t.contains("RUNNING");
            else if(t.startsWith("Mute:"))  muted = t.contains("yes");
            else if(t.startsWith("Volume:"))
            {
                auto it = pct.globalMatch(t);
                while(it.hasNext()) if(it.next().captured(1).toInt() > 0) loud = true;
            }
        }
        closeSink();
        p->deleteLater();
        result(running);
    });
    connect(proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError){
        if(!proc) return;
        QProcess* p = proc; proc = nullptr; p->deleteLater();
        result(alsaRunning());              // pactl non parte (server audio assente): ripiega su ALSA
    });
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("LC_ALL", "C");
    proc->setProcessEnvironment(env);
    proc->start("pactl", {"list", "sinks"});
}

void AudioWatcher::result(bool running)
{
    if(running) { lastRunning.start(); seen = true; }
    bool now = running || (seen && lastRunning.elapsed() < holdMs);
    if(now != playing) { playing = now; emit audioChanged(playing); }
}
