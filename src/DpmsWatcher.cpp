#include "DpmsWatcher.h"
#include <wayland-client.h>
#include "dpms-client.h"
#include "wlr-output-power-client.h"
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <cstdlib>
#include <poll.h>
#include <unistd.h>
#include <vector>
#include <string>
#ifdef HAVE_X11
#include <X11/Xlib.h>
#include <X11/extensions/dpms.h>
#endif

/*---------------------------------------------------------*\
| Wayland: KDE (org_kde_kwin_dpms) e wlroots (zwlr_output_power)
\*---------------------------------------------------------*/
namespace {

struct Ctx;
struct Out
{
    Ctx* ctx; uint32_t name; wl_output* out;
    org_kde_kwin_dpms* kde; zwlr_output_power_v1* wlr;
    bool on; bool known;
};

struct Ctx
{
    DpmsWatcher*                owner   = nullptr;
    org_kde_kwin_dpms_manager*  kdeMgr  = nullptr;
    zwlr_output_power_manager_v1* wlrMgr = nullptr;
    std::vector<Out*>           outs;

    void evaluate()
    {
        if(outs.empty()) return;
        bool any = false;
        for(Out* o : outs) { if(!o->known) return; if(o->on) any = true; }
        owner->report(any);
    }
    void ensure();
};

// --- KDE
void kSupported(void*, org_kde_kwin_dpms*, uint32_t) {}
void kMode(void* d, org_kde_kwin_dpms*, uint32_t m) { ((Out*)d)->on = (m == 0); }
void kDone(void* d, org_kde_kwin_dpms*) { Out* o = (Out*)d; o->known = true; o->ctx->evaluate(); }
const org_kde_kwin_dpms_listener kListener = { kSupported, kMode, kDone };

// --- wlroots
void wMode(void* d, zwlr_output_power_v1*, uint32_t m) { Out* o = (Out*)d; o->on = (m == 1); o->known = true; o->ctx->evaluate(); }
void wFailed(void*, zwlr_output_power_v1*) {}
const zwlr_output_power_v1_listener wListener = { wMode, wFailed };

void Ctx::ensure()
{
    for(Out* o : outs)
    {
        if(kdeMgr && !o->kde)
        {
            o->kde = org_kde_kwin_dpms_manager_get(kdeMgr, o->out);
            org_kde_kwin_dpms_add_listener(o->kde, &kListener, o);
        }
        else if(wlrMgr && !kdeMgr && !o->wlr)
        {
            o->wlr = zwlr_output_power_manager_v1_get_output_power(wlrMgr, o->out);
            zwlr_output_power_v1_add_listener(o->wlr, &wListener, o);
        }
    }
}

void onGlobal(void* d, wl_registry* reg, uint32_t name, const char* iface, uint32_t)
{
    Ctx* c = (Ctx*)d;
    std::string i = iface;
    if(i == "wl_output")
    {
        c->outs.push_back(new Out{c, name, (wl_output*)wl_registry_bind(reg, name, &wl_output_interface, 1), nullptr, nullptr, true, false});
        c->ensure();
    }
    else if(i == "org_kde_kwin_dpms_manager")
    {
        c->kdeMgr = (org_kde_kwin_dpms_manager*)wl_registry_bind(reg, name, &org_kde_kwin_dpms_manager_interface, 1);
        c->ensure();
    }
    else if(i == "zwlr_output_power_manager_v1")
    {
        c->wlrMgr = (zwlr_output_power_manager_v1*)wl_registry_bind(reg, name, &zwlr_output_power_manager_v1_interface, 1);
        c->ensure();
    }
}
void onGlobalRemove(void* d, wl_registry*, uint32_t name)
{
    Ctx* c = (Ctx*)d;
    for(size_t k = 0; k < c->outs.size(); k++)
        if(c->outs[k]->name == name)
        {
            Out* o = c->outs[k];
            if(o->kde) org_kde_kwin_dpms_destroy(o->kde);
            if(o->wlr) zwlr_output_power_v1_destroy(o->wlr);
            wl_output_destroy(o->out);
            delete o;
            c->outs.erase(c->outs.begin() + k);
            c->evaluate();
            return;
        }
}
const wl_registry_listener regListener = { onGlobal, onGlobalRemove };

} // namespace

DpmsWatcher::DpmsWatcher(QObject* parent) : QObject(parent) {}
DpmsWatcher::~DpmsWatcher() { stop(); }

void DpmsWatcher::report(bool on)
{
    {
        std::lock_guard<std::mutex> g(mtx);
        if(last == (int)on) return;
        last = on;
    }
    emit statusChanged(on ? "Schermo acceso" : "Schermo spento (standby)");
    emit screensChanged(on);
}

void DpmsWatcher::start()
{
    stopFlag = false;
    bool wayland = getenv("WAYLAND_DISPLAY") != nullptr;
    bool x11     = getenv("DISPLAY") != nullptr;
    if(wayland) thWayland = std::thread([this]{ runWayland(); });
    else if(x11) thX11 = std::thread([this]{ runX11(); });
    startGnome();
    if(!wayland && !x11 && backends == 0)
        emit statusChanged("Nessun desktop grafico rilevato");
}

void DpmsWatcher::stop()
{
    stopFlag = true;
    if(thWayland.joinable()) thWayland.join();
    if(thX11.joinable())     thX11.join();
}

void DpmsWatcher::runWayland()
{
    wl_display* d = wl_display_connect(nullptr);
    if(!d) { emit statusChanged("Wayland: connessione non riuscita"); return; }

    Ctx ctx; ctx.owner = this;
    wl_registry* reg = wl_display_get_registry(d);
    wl_registry_add_listener(reg, &regListener, &ctx);
    wl_display_roundtrip(d);
    wl_display_roundtrip(d);

    if(!ctx.kdeMgr && !ctx.wlrMgr)
    {
        if(backends == 0)
            emit statusChanged("Wayland: il compositor non offre un protocollo per lo stato dello schermo (provo con GNOME/D-Bus)");
        wl_display_disconnect(d);
        return;
    }
    backends++;
    emit statusChanged(ctx.kdeMgr ? "In ascolto: Wayland (KDE Plasma)" : "In ascolto: Wayland (wlroots: sway/Hyprland/...)");

    int fd = wl_display_get_fd(d);
    while(!stopFlag)
    {
        while(wl_display_prepare_read(d) != 0) wl_display_dispatch_pending(d);
        wl_display_flush(d);
        pollfd pfd{fd, POLLIN, 0};
        int r = poll(&pfd, 1, 200);
        if(r > 0) wl_display_read_events(d); else wl_display_cancel_read(d);
        if(wl_display_dispatch_pending(d) < 0) break;
    }
    wl_display_disconnect(d);
}

/*---------------------------------------------------------*\
| X11: estensione DPMS, interrogata 4 volte al secondo
\*---------------------------------------------------------*/
void DpmsWatcher::runX11()
{
#ifdef HAVE_X11
    Display* dpy = XOpenDisplay(nullptr);
    if(!dpy) { emit statusChanged("X11: connessione non riuscita"); return; }
    int ev, er;
    if(!DPMSQueryExtension(dpy, &ev, &er) || !DPMSCapable(dpy))
    {
        emit statusChanged("X11: estensione DPMS non disponibile");
        XCloseDisplay(dpy);
        return;
    }
    backends++;
    emit statusChanged("In ascolto: X11 (DPMS)");
    while(!stopFlag)
    {
        CARD16 level; BOOL enabled;
        if(DPMSInfo(dpy, &level, &enabled)) report(!enabled || level == DPMSModeOn);
        usleep(250000);
    }
    XCloseDisplay(dpy);
#else
    emit statusChanged("Compilato senza supporto X11");
#endif
}

/*---------------------------------------------------------*\
| GNOME: Mutter PowerSaveMode su D-Bus
\*---------------------------------------------------------*/
void DpmsWatcher::startGnome()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if(!bus.isConnected() || !bus.interface() ||
       !bus.interface()->isServiceRegistered("org.gnome.Mutter.DisplayConfig")) return;
    bus.connect("org.gnome.Mutter.DisplayConfig", "/org/gnome/Mutter/DisplayConfig",
                "org.freedesktop.DBus.Properties", "PropertiesChanged",
                this, SLOT(onMutterProps(QString, QVariantMap, QStringList)));
    backends++;
    emit statusChanged("In ascolto: GNOME (D-Bus)");
}

void DpmsWatcher::onMutterProps(QString iface, QVariantMap changed, QStringList)
{
    if(iface != "org.gnome.Mutter.DisplayConfig" || !changed.contains("PowerSaveMode")) return;
    int m = changed.value("PowerSaveMode").toInt();
    if(m >= 0) report(m == 0);
}
