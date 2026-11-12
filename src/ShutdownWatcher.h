// Rileva lo spegnimento del PC (non il riavvio) tramite systemd-logind.
#pragma once
#include <QObject>
#include <QTimer>
#include <QVariantMap>
#include <QDBusUnixFileDescriptor>

class ShutdownWatcher : public QObject
{
    Q_OBJECT
public:
    explicit ShutdownWatcher(QObject* parent = nullptr);
    void setEnabled(bool on);       // prende/rilascia il "blocco di ritardo" di logind
signals:
    void powerOff();                // il PC si sta spegnendo (poweroff/halt): spegni i LED
    void cancelled();               // lo spegnimento e' stato annullato
private slots:
    void onPrepare(bool start);
    void onPrepareMeta(bool start, const QVariantMap& meta);
private:
    void takeLock();
    void releaseLock();
    void decide(const QString& type);
    void pollJobs();
    int  jobKind();                 // 1 = spegnimento, 2 = riavvio, 0 = non ancora noto

    bool enabled = false;
    bool decided = false;
    bool inShutdown = false;
    int  polls = 0;
    QTimer pollTimer;
    QDBusUnixFileDescriptor lock;
};
