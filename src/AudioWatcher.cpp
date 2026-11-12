#include "AudioWatcher.h"
#include <QProcess>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTimer>
#include <algorithm>
#include <cstdlib>
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
        busy = false;
        seen = false;
        if(playing) { playing = false; emit audioChanged(false); }
    }
}

void AudioWatcher::setHoldMs(int ms) { holdMs = ms; }

void AudioWatcher::poll()
{
    if(busy) return;                        // controllo precedente ancora in corso
    // PipeWire/PulseAudio (copre anche Bluetooth e uscite di rete); senza pactl si usa ALSA
    if(QStandardPaths::findExecutable("pactl").isEmpty()) { result(alsaRunning()); return; }
    busy = true;
    proc = new QProcess(this);
    connect(proc, &QProcess::finished, this, [this]{
        QProcess* p = proc; proc = nullptr;
        // Output di "pactl list sinks" (con LC_ALL=C, quindi non tradotto): un blocco per uscita.
        // Candidata = uscita in RUNNING, non muta, con volume sopra lo zero.
        static const QRegularExpression pct("(\\d+)%");
        QStringList mons;
        QString monitor;
        bool isRunning = false, muted = false, loud = false, inSink = false;
        auto closeSink = [&]{ if(inSink && isRunning && !muted && loud && !monitor.isEmpty()) mons << monitor; };
        for(const QByteArray& raw : p->readAllStandardOutput().split('\n'))
        {
            const QString line = QString::fromUtf8(raw);
            if(line.startsWith("Sink #")) { closeSink(); inSink = true; isRunning = muted = loud = false; monitor.clear(); continue; }
            const QString t = line.trimmed();
            if(t.startsWith("State:"))               isRunning = t.contains("RUNNING");
            else if(t.startsWith("Mute:"))           muted = t.contains("yes");
            else if(t.startsWith("Monitor Source:")) monitor = t.mid(15).trimmed();
            else if(t.startsWith("Volume:"))
            {
                auto it = pct.globalMatch(t);
                while(it.hasNext()) if(it.next().captured(1).toInt() > 0) loud = true;
            }
        }
        closeSink();
        p->deleteLater();
        if(mons.isEmpty()) { busy = false; result(false); return; }
        // Un flusso aperto non basta (un video in pausa o un'app che manda silenzio): serve segnale vero.
        if(QStandardPaths::findExecutable("parec").isEmpty()) { busy = false; result(true); return; }
        probe(mons, 0);
    });
    connect(proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError){
        if(!proc) return;
        QProcess* p = proc; proc = nullptr; p->deleteLater();
        busy = false;
        result(alsaRunning());              // pactl non parte (server audio assente): ripiega su ALSA
    });
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("LC_ALL", "C");
    proc->setProcessEnvironment(env);
    proc->start("pactl", {"list", "sinks"});
}

// Ascolta mezzo secondo il "monitor" di un'uscita (quello che esce davvero dalle casse) e
// verifica che non sia silenzio. Se ci sono piu' uscite candidate le prova una dopo l'altra.
void AudioWatcher::probe(QStringList mons, int idx)
{
    if(idx >= mons.size()) { busy = false; result(false); return; }
    QProcess* p = new QProcess(this);
    connect(p, &QProcess::finished, this, [this, p, mons, idx]{
        const QByteArray data = p->readAllStandardOutput();
        p->deleteLater();
        // "Presenza" di suono, non volume: contano i campioni sopra il rumore di fondo (~ -54 dB).
        // Musica anche piano li supera di molto; il silenzio digitale e il rumore minuscolo no.
        // Servono diversi campioni, cosi' un singolo clic isolato non basta.
        int above = 0;
        const qint16* smp = reinterpret_cast<const qint16*>(data.constData());
        for(int i = 0; i < data.size() / 2; i++) if(std::abs((int)smp[i]) >= 64) above++;
        if(above >= 40) { busy = false; result(true); }
        else probe(mons, idx + 1);
    });
    connect(p, &QProcess::errorOccurred, this, [this, p](QProcess::ProcessError e){
        if(e != QProcess::FailedToStart) return;
        p->deleteLater();
        busy = false; result(true);          // parec non parte: ripiego sul solo stato dell'uscita
    });
    p->start("parec", {"-d", mons[idx], "--format=s16le", "--rate=8000", "--channels=1", "--latency-msec=50"});
    QTimer::singleShot(500, p, [p]{ if(p->state() != QProcess::NotRunning) p->terminate(); });
}

void AudioWatcher::result(bool running)
{
    if(!timer.isActive()) return;           // controllo disattivato nel frattempo
    if(running) { lastRunning.start(); seen = true; }
    bool now = running || (seen && lastRunning.elapsed() < holdMs);
    if(now != playing) { playing = now; emit audioChanged(playing); }
}
