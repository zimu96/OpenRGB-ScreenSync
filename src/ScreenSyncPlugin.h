#pragma once
#include <QObject>
#include "OpenRGBPluginInterface.h"
#include "FadeEngine.h"

#include <QPointer>
class DpmsWatcher; class SettingsWidget; class ShutdownWatcher; class AudioWatcher;

class ScreenSyncPlugin : public QObject, public OpenRGBPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OpenRGBPluginInterface_IID FILE "metadata.json")
    Q_INTERFACES(OpenRGBPluginInterface)
public:
    ScreenSyncPlugin() = default;
    ~ScreenSyncPlugin() override = default;

    OpenRGBPluginInfo   GetPluginInfo() override;
    unsigned int        GetPluginAPIVersion() override;
    void                Load(OpenRGBPluginAPIInterface* plugin_api_ptr) override;
    QWidget*            GetWidget() override;
    QMenu*              GetTrayMenu() override { return nullptr; }
    void                Unload() override;
    void                OnProfileAboutToLoad() override {}
    void                OnProfileLoad(nlohmann::json) override {}
    nlohmann::json      OnProfileSave() override { return nlohmann::json(); }
    unsigned char*      OnSDKCommand(unsigned int, unsigned char*, unsigned int*) override { return nullptr; }
    void                ProfileManagerUpdated(unsigned int) override {}
    void                ResourceManagerUpdated(unsigned int) override;
    void                SettingsManagerUpdated(unsigned int) override {}

private:
    void loadSettings();
    void saveSettings(const ScreenSyncSettings& s);

    OpenRGBPluginAPIInterface* api = nullptr;
    FadeEngine*                engine = nullptr;
    DpmsWatcher*               watcher = nullptr;
    ShutdownWatcher*           shutdown = nullptr;
    AudioWatcher*              audioW = nullptr;
    QPointer<SettingsWidget>   widget;
    QString                    lastStatus = "in avvio";
};
