#include "ScreenSyncPlugin.h"
#include "DpmsWatcher.h"
#include "SettingsWidget.h"

static const char* KEY = "ScreenSyncPlugin";

OpenRGBPluginInfo ScreenSyncPlugin::GetPluginInfo()
{
    OpenRGBPluginInfo i;
    i.Name        = "Screen Sync";
    i.Description = "Spegne (con dissolvenza) i LED di tutti i dispositivi OpenRGB quando il sistema spegne lo schermo, e li riaccende quando lo schermo si riaccende.";
    i.Version     = "1.1.0";
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
}

QWidget* ScreenSyncPlugin::GetWidget()
{
    widget = new SettingsWidget(api, engine);
    widget->setStatus(lastStatus);
    connect(widget, &SettingsWidget::settingsChanged, this, [this](const ScreenSyncSettings& s){
        engine->setSettings(s);
        saveSettings(s);
    });
    return widget;
}

void ScreenSyncPlugin::Unload()
{
    if(watcher) { watcher->stop(); }
    if(engine)  { engine->restoreNow(); }
    widget = nullptr;
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
    j["selected"] = nlohmann::json::array();
    for(const QString& k : s.selected) j["selected"].push_back(k.toStdString());
    api->SetSettings(KEY, j);
    api->SaveSettings();
}
