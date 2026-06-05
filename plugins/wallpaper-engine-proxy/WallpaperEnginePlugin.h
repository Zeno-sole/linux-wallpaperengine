#pragma once

#include <DApplet>
#include <QTimer>

class DBusClient;
class DDEEventBridge;

class WallpaperEnginePlugin : public DTK_NAMESPACE::DApplet {
    Q_OBJECT
public:
    explicit WallpaperEnginePlugin(QObject* parent = nullptr);
    ~WallpaperEnginePlugin() override;

    bool load() override;
    bool init() override;
    void quit() override;

private:
    void ensureEngineRunning();
    void onEngineDisconnected();
    void checkBatteryStatus();

    DBusClient* m_dbusClient = nullptr;
    DDEEventBridge* m_eventBridge = nullptr;

    // Crash recovery
    int m_crashCount = 0;
    static constexpr int MAX_CRASH_RESTARTS = 3;
    QTimer m_crashRecoveryTimer;

    // Battery monitoring
    QTimer m_batteryTimer;
    bool m_onBattery = false;
};
