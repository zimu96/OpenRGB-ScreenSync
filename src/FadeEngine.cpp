#include "FadeEngine.h"
#include <algorithm>
#include <cmath>
#include <QElapsedTimer>
#define SLOG(...) api->LogEntry(__FILE__, __LINE__, 3, __VA_ARGS__)

FadeEngine::FadeEngine(OpenRGBPluginAPIInterface* a, QObject* parent) : QObject(parent), api(a)
{
    timer.setInterval(40);
    total.start();   // ~25 passi al secondo
    connect(&timer, &QTimer::timeout, this, &FadeEngine::tick);
}

QString FadeEngine::deviceKey(RGBControllerInterface* c)
{
    return QString::fromStdString(c->GetName()) + "|" + QString::fromStdString(c->GetLocation());
}

void FadeEngine::setSettings(const ScreenSyncSettings& s)
{
    cfg = s;
    if(!cfg.enabled && (snap || timer.isActive())) restoreNow();
}

bool FadeEngine::isSelected(RGBControllerInterface* c) const
{
    return cfg.allDevices || cfg.selected.contains(deviceKey(c));
}

bool FadeEngine::stillExists(RGBControllerInterface* c) const
{
    for(RGBControllerInterface* x : api->GetRGBControllers()) if(x == c) return true;
    return false;
}

void FadeEngine::setScreenOn(bool on)
{
    SLOG("[ScreenSync] evento schermo: %s (plugin %s)", on ? "ACCESO" : "SPENTO", cfg.enabled ? "attivo" : "disattivato");
    if(!cfg.enabled || shuttingDown) return;
    if(!on && !snap) takeSnapshot();
    if(!snap) return;                       // schermo acceso e niente da ripristinare
    target = on ? 1.0 : 0.0;
    if(!timer.isActive()) { clock.start(); timer.start(); }
}

void FadeEngine::testCycle()
{
    setScreenOn(false);
    int wait = cfg.fadeOutMs + 1500;
    QTimer::singleShot(wait, this, [this]{ setScreenOn(true); });
}

void FadeEngine::takeSnapshot()
{
    devs.clear();
    for(RGBControllerInterface* c : api->GetRGBControllers())
    {
        if(!isSelected(c) || c->GetModeCount() == 0 || c->GetActiveMode() < 0) continue;
        Dev d{};
        d.c = c;
        d.origMode = c->GetActiveMode();
        unsigned flags = c->GetModeFlags(d.origMode);
        if((flags & MODE_FLAG_HAS_PER_LED_COLOR) && c->GetModeColorMode(d.origMode) == MODE_COLORS_PER_LED) d.kind = 0;
        else if(flags & MODE_FLAG_HAS_BRIGHTNESS) d.kind = 1;
        else d.kind = 2;

        d.offMode = -1;
        for(unsigned m = 0; m < c->GetModeCount(); m++)
            if(c->GetModeFlags(m) & MODE_FLAG_HAS_PER_LED_COLOR)
            {
                if(d.offMode < 0) d.offMode = (int)m;
                if(c->GetModeName(m) == "Direct") { d.offMode = (int)m; break; }
            }

        unsigned n = c->GetLEDCount();
        d.colors.resize(n);
        for(unsigned i = 0; i < n; i++) d.colors[i] = c->GetColor(i);
        if(d.kind == 1)
        {
            d.origBright = c->GetModeBrightness(d.origMode);
            d.minBright  = c->GetModeBrightnessMin(d.origMode);
        }
        if(d.kind == 0)
        {
            bool allBlack = true;
            for(RGBColor col : d.colors) if(col != 0) { allBlack = false; break; }
            if(allBlack)
            {
                // Buffer tutto nero: lo stato reale del dispositivo non e' noto a OpenRGB
                // (es. profilo caricato prima che il dispositivo fosse registrato). Non lo tocco.
                SLOG("[ScreenSync] %s ignorato: colori noti tutti neri (carica un profilo e riprova)", c->GetName().c_str());
                continue;
            }
        }
        d.switched = false;
        d.lastMs = -100000;
        d.minGapMs = (n > 50) ? 125 : 40;   // tastiere con molti LED: ~8 invii/s
        SLOG("[ScreenSync] snapshot %s: modo=%d (%s) tipo=%d offMode=%d luci=%u bright=%u/min=%u", c->GetName().c_str(), d.origMode, c->GetModeName(d.origMode).c_str(), d.kind, d.offMode, n, d.origBright, d.minBright);
        devs.push_back(d);
    }
    snap = !devs.empty();
    level = 1.0;
    offApplied = false;
}

void FadeEngine::tick()
{
    double dt = clock.restart();
    double dur = (target < level) ? cfg.fadeOutMs : cfg.fadeInMs;
    double step = (dur <= 0) ? 1.0 : dt / dur;
    if(target < level) level = std::max(target, level - step);
    else               level = std::min(target, level + step);

    apply(level);

    if(level <= 0.0 && target <= 0.0) { timer.stop(); finalizeOff(); }
    else if(level >= 1.0 && target >= 1.0) { timer.stop(); restoreExact(); }
}

void FadeEngine::apply(double lv)
{
    for(Dev& d : devs)
    {
        if(!stillExists(d.c)) continue;
        bool endpoint = (lv <= 0.0 || lv >= 1.0);
        if(!endpoint && total.elapsed() - d.lastMs < d.minGapMs) continue;
        d.lastMs = total.elapsed();

        // all'inizio della risalita, tornare al modo originale
        if(lv > 0.0 && d.switched)
        {
            d.c->SetActiveMode(d.origMode);
            d.switched = false;
        }
        if(d.kind == 0)
        {
            for(unsigned i = 0; i < d.colors.size(); i++)
            {
                RGBColor o = d.colors[i];
                d.c->SetColor(i, ToRGBColor((unsigned)std::lround(RGBGetRValue(o) * lv),
                                            (unsigned)std::lround(RGBGetGValue(o) * lv),
                                            (unsigned)std::lround(RGBGetBValue(o) * lv)));
            }
            d.c->UpdateLEDs();
        }
        else if(d.kind == 1 && !d.switched)
        {
            unsigned b = d.minBright + (unsigned)std::lround((double)(d.origBright - d.minBright) * lv);
            d.c->SetModeBrightness(d.origMode, b);
            d.c->UpdateMode();
        }
    }
}

void FadeEngine::finalizeOff()
{
    SLOG("[ScreenSync] finalizeOff");
    QTimer::singleShot(500, this, [this]{ if(snap && level <= 0.0 && !timer.isActive()) { apply(0.0); } });
    offApplied = true;
    for(Dev& d : devs)
    {
        if(!stillExists(d.c)) continue;
        bool needSwitch = (d.kind == 2) || (d.kind == 1 && d.minBright > 0);
        if(needSwitch && d.offMode >= 0 && d.offMode != d.origMode)
        {
            d.c->SetActiveMode(d.offMode);
            d.c->SetAllColors(0);
            d.c->UpdateLEDs();
            d.switched = true;
        }
    }
}

void FadeEngine::writeOriginal(Dev& d)
{
    if(!stillExists(d.c)) return;
    if(d.switched) { d.c->SetActiveMode(d.origMode); d.switched = false; }
    if(d.kind == 0)
    {
        for(unsigned i = 0; i < d.colors.size(); i++) d.c->SetColor(i, d.colors[i]);
        d.c->UpdateLEDs();
    }
    else if(d.kind == 1)
    {
        d.c->SetModeBrightness(d.origMode, d.origBright);
        d.c->UpdateMode();
    }
}

void FadeEngine::restoreExact()
{
    SLOG("[ScreenSync] restoreExact");
    for(Dev& d : devs) writeOriginal(d);
    // secondo invio dopo mezzo secondo: alcune tastiere perdono il comando finale
    std::vector<Dev> copy = devs;
    QTimer::singleShot(500, this, [this, copy]() mutable {
        if(snap || level < 1.0) return;     // nel frattempo e' ripartita una sfumatura
        for(Dev& d : copy) writeOriginal(d);
        SLOG("[ScreenSync] restoreExact (secondo invio)");
    });
    devs.clear();
    snap = false;
    level = target = 1.0;
}

void FadeEngine::restoreNow()
{
    if(shuttingDown) return;        // durante lo spegnimento del PC i LED devono restare spenti
    timer.stop();
    if(snap) { level = 1.0; target = 1.0; restoreExact(); }
}

void FadeEngine::shutdownOff()
{
    if(!cfg.offOnShutdown || shuttingDown) return;
    SLOG("[ScreenSync] spegnimento del PC: LED spenti subito");
    shuttingDown = true;
    timer.stop();
    if(!snap) takeSnapshot();
    if(!snap) return;
    level = target = 0.0;
    for(Dev& d : devs) d.lastMs = -100000;
    apply(0.0);
    finalizeOff();
    // secondo invio: se un comando si perde, i LED si spengono lo stesso
    QTimer::singleShot(250, this, [this]{ if(snap) { for(Dev& d : devs) d.lastMs = -100000; apply(0.0); } });
}

void FadeEngine::cancelShutdown()
{
    if(!shuttingDown) return;
    shuttingDown = false;
    if(snap) { timer.stop(); level = 1.0; target = 1.0; restoreExact(); }
}
