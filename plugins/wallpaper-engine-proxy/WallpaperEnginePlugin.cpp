#include "WallpaperEnginePlugin.h"
#include "DBusClient.h"
#include "DDEEventBridge.h"

#include <QProcess>
#include <QTimer>
#include <QFile>
#include <QDebug>

WallpaperEnginePlugin::WallpaperEnginePlugin(QObject* parent)
    : DApplet(parent) {
    // Setup crash recovery timer
    m_crashRecoveryTimer.setSingleShot(true);
    connect(&m_crashRecoveryTimer, &QTimer::timeout, this, &WallpaperEnginePlugin::ensureEngineRunning);

    // Setup battery monitoring (check every 30 seconds)
    connect(&m_batteryTimer, &QTimer::timeout, this, &WallpaperEnginePlugin::checkBatteryStatus);
}

WallpaperEnginePlugin::~WallpaperEnginePlugin() = default;

bool WallpaperEnginePlugin::load() {
    // Check if wallpaperengine binary is available
    QProcess which;
    which.start("which", {"linux-wallpaperengine"});
    which.waitForFinished();
    if (which.exitCode() != 0) {
        qWarning() << "linux-wallpaperengine not found in PATH";
        return false;
    }
    return true;
}

bool WallpaperEnginePlugin::init() {
    m_dbusClient = new DBusClient(this);
    m_eventBridge = new DDEEventBridge(m_dbusClient, this);

    // Connect engine disconnect signal for crash recovery
    connect(m_dbusClient, &DBusClient::engineDisconnected,
            this, &WallpaperEnginePlugin::onEngineDisconnected);

    ensureEngineRunning();

    // Connect DDE events
    m_eventBridge->connectDDESignals();
    m_eventBridge->connectLockScreen();
    m_eventBridge->connectFullscreen();

    // Start battery monitoring
    m_batteryTimer.start(30000);
    checkBatteryStatus();

    return true;
}

void WallpaperEnginePlugin::quit() {
    m_crashRecoveryTimer.stop();
    m_batteryTimer.stop();
    if (m_dbusClient) {
        m_dbusClient->quit();
    }
}

void WallpaperEnginePlugin::ensureEngineRunning() {
    if (m_dbusClient->isEngineRunning()) {
        qDebug() << "Wallpaper engine already running";
        m_crashCount = 0; // Reset crash count on successful start
        return;
    }

    if (m_crashCount >= MAX_CRASH_RESTARTS) {
        qWarning() << "Wallpaper engine crashed" << m_crashCount
                    << "times, stopping restart attempts";
        return;
    }

    qDebug() << "Starting wallpaper engine process...";
    QProcess::startDetached("linux-wallpaperengine", {"--dde-plugin"});

    // Wait for DBus service to be ready
    QTimer* timer = new QTimer(this);
    int attempts = 0;
    connect(timer, &QTimer::timeout, this, [this, timer, &attempts]() {
        if (m_dbusClient->isEngineRunning()) {
            timer->stop();
            timer->deleteLater();
            qDebug() << "Wallpaper engine started successfully";
        } else if (++attempts > 30) {  // 15 second timeout
            timer->stop();
            timer->deleteLater();
            qWarning() << "Failed to start wallpaper engine after 15 seconds";
        }
    });
    timer->start(500);
}

void WallpaperEnginePlugin::onEngineDisconnected() {
    m_crashCount++;
    qWarning() << "Wallpaper engine disconnected (crash" << m_crashCount
                << "of" << MAX_CRASH_RESTARTS << ")";

    if (m_crashCount < MAX_CRASH_RESTARTS) {
        qDebug() << "Restarting in 3 seconds...";
        m_crashRecoveryTimer.start(3000);
    } else {
        qWarning() << "Max crash count reached, not restarting";
    }
}

void WallpaperEnginePlugin::checkBatteryStatus() {
    // Check if running on battery via /sys/class/power_supply
    QFile acOnline("/sys/class/power_supply/AC/online");
    bool onBattery = false;

    if (acOnline.open(QIODevice::ReadOnly)) {
        char val;
        if (acOnline.read(&val, 1) == 1) {
            onBattery = (val == '0');
        }
    } else {
        // Try alternative paths
        QFile batStatus("/sys/class/power_supply/BAT0/status");
        if (batStatus.open(QIODevice::ReadOnly)) {
            QString status = QString::fromUtf8(batStatus.readAll()).trimmed();
            onBattery = (status == "Discharging");
        }
    }

    if (onBattery != m_onBattery) {
        m_onBattery = onBattery;
        if (onBattery) {
            qDebug() << "On battery, reducing wallpaper FPS";
            m_dbusClient->setFPS(15);
        } else {
            qDebug() << "On AC power, restoring wallpaper FPS";
            m_dbusClient->setFPS(30);
        }
    }
}
