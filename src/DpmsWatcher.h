// Rileva quando il sistema spegne/riaccende lo schermo, su vari desktop:
//  - Wayland KDE Plasma      (protocollo org_kde_kwin_dpms)
//  - Wayland wlroots         (sway, Hyprland, river, labwc: wlr-output-power-management)
//  - GNOME (Wayland e X11)   (D-Bus org.gnome.Mutter.DisplayConfig, PowerSaveMode)
//  - X11 (i3, XFCE, ...)     (estensione DPMS di X)
#pragma once
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <atomic>
#include <mutex>
#include <thread>

class DpmsWatcher : public QObject
{
    Q_OBJECT
public:
    explicit DpmsWatcher(QObject* parent = nullptr);
    ~DpmsWatcher() override;
    void start();
    void stop();
    void report(bool on);              // thread-safe: i backend la chiamano a ogni cambio

signals:
    void screensChanged(bool anyOn);   // true = schermo acceso
    void statusChanged(QString text);

private slots:
    void onMutterProps(QString iface, QVariantMap changed, QStringList);

private:
    void runWayland();
    void runX11();
    void startGnome();
    std::thread       thWayland, thX11;
    std::atomic<bool> stopFlag{false};
    std::mutex        mtx;
    int               last = -1;
    std::atomic<int>  backends{0};
};
