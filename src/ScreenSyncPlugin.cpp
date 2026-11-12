#include "ScreenSyncPlugin.h"
#include "DpmsWatcher.h"
#include "SettingsWidget.h"
#include "ShutdownWatcher.h"
#include "AudioWatcher.h"
#include <QMetaObject>

static const char* KEY = "ScreenSyncPlugin";

OpenRGBPluginInfo ScreenSyncPlugin::GetPluginInfo()
{
    OpenRGBPluginInfo i;
    i.Name        = "Screen Sync";
    i.Description = "Spegne (con dissolvenza) i LED di tutti i dispositivi OpenRGB quando il sistema spegne lo schermo, e li riaccende quando lo schermo si riaccende.";
    i.Version     = "1.0.1";
    i.Commit      = "";
    i.URL         = "";
    i.Location    = OPENRGB_PLUGIN_LOCATION_TOP;
    i.Label       = "Schermo";
    i.TabIconString = "";
    i.ProtocolVersion = 5;
    return i;
}

unsigned int ScreenSyncPlugin::GetPluginAPIVersion() { return OPENRGB_PLUGIN_API_VERSION; }

void ScreenSyncPlugin::Load(OpenRGBPluginAPIInterface* a)
{
    api = a;
    engine = new FadeEngine(api, this);
    loadSettings();

    watcher = new DpmsWatcher(this);
    connect(watcher, &DpmsWatcher::screensChanged, engine, &FadeEngine::setScreenOn);
    connect(watcher, &DpmsWatcher::statusChanged, this, [this](QString s){
        lastStatus = s;
        if(widget) widget->setStatus(s);
    });
    watcher->start();

    shutdown = new ShutdownWatcher(this);
    connect(shutdown, &ShutdownWatcher::powerOff, engine, &FadeEngine::shutdownOff);
    connect(shutdown, &ShutdownWatcher::cancelled, engine, &FadeEngine::cancelShutdown);
    shutdown->setEnabled(engine->settings().offOnShutdown);

    audioW = new AudioWatcher(this);
    connect(audioW, &AudioWatcher::audioChanged, engine, &FadeEngine::setAudioActive);
    connect(watcher, &DpmsWatcher::screensChanged, audioW, &AudioWatcher::setScreenOn);
    audioW->setHoldMs(engine->settings().keepHoldMs);
    audioW->setEnabled(engine->settings().enabled && engine->settings().keepOnAudio);
}

// OpenRGB ha cambiato l'elenco dei dispositivi (nuovo dispositivo, riscansione...): aggiorna la scheda
void ScreenSyncPlugin::ResourceManagerUpdated(unsigned int)
{
    if(widget) QMetaObject::invokeMethod(widget.data(), "refreshDevicesAuto", Qt::QueuedConnection);
}

QWidget* ScreenSyncPlugin::GetWidget()
{
    widget = new SettingsWidget(api, engine);
    widget->setStatus(lastStatus);
    connect(widget, &SettingsWidget::settingsChanged, this, [this](const ScreenSyncSettings& s){
        engine->setSettings(s);
        if(shutdown) shutdown->setEnabled(s.offOnShutdown);
        if(audioW) { audioW->setHoldMs(s.keepHoldMs); audioW->setEnabled(s.enabled && s.keepOnAudio); }
        saveSettings(s);
    });
    return widget;
}

void ScreenSyncPlugin::Unload()
{
    if(audioW) audioW->setEnabled(false);
    if(watcher) { watcher->stop(); }
    if(engine)  { engine->restoreNow(); }
    widget.clear();
}

void ScreenSyncPlugin::loadSettings()
{
    ScreenSyncSettings s;
    nlohmann::json j = api->GetSettings(KEY);
    if(j.is_object())
    {
        s.enabled    = j.value("enabled", s.enabled);
        s.fadeOutMs  = j.value("fade_out_ms", s.fadeOutMs);
        s.fadeInMs   = j.value("fade_in_ms", s.fadeInMs);
        s.allDevices = j.value("all_devices", s.allDevices);
        s.offOnShutdown = j.value("off_on_shutdown", s.offOnShutdown);
        s.keepOnAudio = j.value("keep_on_audio", s.keepOnAudio);
        s.keepAll     = j.value("keep_all", s.keepAll);
        s.keepHoldMs  = j.value("keep_hold_ms", s.keepHoldMs);
        if(j.contains("keep_selected") && j["keep_selected"].is_array())
            for(auto& k : j["keep_selected"]) s.keepSelected << QString::fromStdString(k.get<std::string>());
        if(j.contains("selected") && j["selected"].is_array())
            for(auto& k : j["selected"]) s.selected << QString::fromStdString(k.get<std::string>());
    }
    engine->setSettings(s);
}

void ScreenSyncPlugin::saveSettings(const ScreenSyncSettings& s)
{
    nlohmann::json j;
    j["enabled"] = s.enabled; j["fade_out_ms"] = s.fadeOutMs; j["fade_in_ms"] = s.fadeInMs;
    j["all_devices"] = s.allDevices;
    j["off_on_shutdown"] = s.offOnShutdown;
    j["keep_on_audio"] = s.keepOnAudio; j["keep_all"] = s.keepAll; j["keep_hold_ms"] = s.keepHoldMs;
    j["keep_selected"] = nlohmann::json::array();
    for(const QString& k : s.keepSelected) j["keep_selected"].push_back(k.toStdString());
    j["selected"] = nlohmann::json::array();
    for(const QString& k : s.selected) j["selected"].push_back(k.toStdString());
    api->SetSettings(KEY, j);
    api->SaveSettings();
}
