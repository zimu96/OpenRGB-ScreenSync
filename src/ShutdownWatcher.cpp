#include "ShutdownWatcher.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusArgument>
#include <QDBusObjectPath>
#include <QDBusReply>

static const char* LOGIND = "org.freedesktop.login1";
static const char* LOGIND_PATH = "/org/freedesktop/login1";
static const char* LOGIND_IF = "org.freedesktop.login1.Manager";

ShutdownWatcher::ShutdownWatcher(QObject* parent) : QObject(parent)
{
    QDBusConnection bus = QDBusConnection::systemBus();
    bus.connect(LOGIND, LOGIND_PATH, LOGIND_IF, "PrepareForShutdownWithMetadata", this, SLOT(onPrepareMeta(bool,QVariantMap)));
    bus.connect(LOGIND, LOGIND_PATH, LOGIND_IF, "PrepareForShutdown", this, SLOT(onPrepare(bool)));
    pollTimer.setInterval(50);
    connect(&pollTimer, &QTimer::timeout, this, &ShutdownWatcher::pollJobs);
}

void ShutdownWatcher::setEnabled(bool on)
{
    enabled = on;
    if(on && !inShutdown) takeLock(); else if(!on) releaseLock();
}

void ShutdownWatcher::takeLock()
{
    if(lock.isValid()) return;
    QDBusMessage m = QDBusMessage::createMethodCall(LOGIND, LOGIND_PATH, LOGIND_IF, "Inhibit");
    m << "shutdown" << "OpenRGB Screen Sync" << "Spegnimento dei LED" << "delay";
    QDBusReply<QDBusUnixFileDescriptor> r = QDBusConnection::systemBus().call(m, QDBus::Block, 1000);
    if(r.isValid()) lock = r.value();
}

void ShutdownWatcher::releaseLock() { lock = QDBusUnixFileDescriptor(); }

void ShutdownWatcher::onPrepareMeta(bool start, const QVariantMap& meta)
{
    if(!start || !enabled) return;
    inShutdown = true;
    QString type = meta.value("type").toString();
    if(!type.isEmpty()) decide(type);
}

void ShutdownWatcher::onPrepare(bool start)
{
    if(!enabled) return;
    if(!start) {                              // annullato
        pollTimer.stop();
        bool was = decided;
        decided = inShutdown = false;
        if(was) emit cancelled();
        takeLock();
        return;
    }
    inShutdown = true;
    if(decided) return;
    // vecchi systemd senza metadati: guardo quale "target" viene messo in coda
    polls = 0;
    QTimer::singleShot(100, this, [this]{ if(inShutdown && !decided) pollTimer.start(); });
}

int ShutdownWatcher::jobKind()
{
    QDBusMessage m = QDBusMessage::createMethodCall("org.freedesktop.systemd1", "/org/freedesktop/systemd1",
                                                    "org.freedesktop.systemd1.Manager", "ListJobs");
    QDBusMessage r = QDBusConnection::systemBus().call(m, QDBus::Block, 300);
    if(r.type() != QDBusMessage::ReplyMessage || r.arguments().isEmpty()) return 0;
    const QDBusArgument arg = r.arguments().at(0).value<QDBusArgument>();
    int kind = 0;
    arg.beginArray();
    while(!arg.atEnd()) {
        uint id; QString unit, type, state; QDBusObjectPath o1, o2;
        arg.beginStructure(); arg >> id >> unit >> type >> state >> o1 >> o2; arg.endStructure();
        if(unit == "reboot.target" || unit == "kexec.target" || unit == "soft-reboot.target") return 2;
        if(unit == "poweroff.target" || unit == "halt.target") kind = 1;
    }
    arg.endArray();
    return kind;
}

void ShutdownWatcher::pollJobs()
{
    if(decided) { pollTimer.stop(); return; }
    int k = jobKind();
    if(k == 1) { pollTimer.stop(); decide("poweroff"); }
    else if(k == 2) { pollTimer.stop(); decide("reboot"); }
    else if(++polls > 40) { pollTimer.stop(); releaseLock(); }   // non riconosciuto: non faccio niente
}

void ShutdownWatcher::decide(const QString& type)
{
    if(decided) return;
    decided = true;
    pollTimer.stop();
    if(type == "reboot" || type == "kexec" || type == "soft-reboot") { releaseLock(); return; }   // riavvio: i LED restano
    emit powerOff();
    QTimer::singleShot(800, this, [this]{ releaseLock(); });   // il tempo di far arrivare lo spegnimento ai dispositivi
}
