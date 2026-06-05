#include "DDEEventBridge.h"
#include "DBusClient.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDebug>

DDEEventBridge::DDEEventBridge(DBusClient* client, QObject* parent)
    : QObject(parent), m_client(client) {
    connect(&m_fullscreenTimer, &QTimer::timeout, this, &DDEEventBridge::checkFullscreen);
}

void DDEEventBridge::connectDDESignals() {
    // Monitor com.deepin.wm workspace switch signal
    QDBusConnection::sessionBus().connect(
        "com.deepin.wm", "/com/deepin/wm", "com.deepin.wm",
        "WorkspaceSwitched",
        this, SLOT(onWorkspaceSwitched(int, int)));

    // Monitor workspace background change
    QDBusConnection::sessionBus().connect(
        "com.deepin.wm", "/com/deepin/wm", "com.deepin.wm",
        "WorkspaceBackgroundChanged",
        this, SLOT(onWorkspaceBackgroundChanged(int, QString)));

    qDebug() << "DDE event signals connected";
}

void DDEEventBridge::connectLockScreen() {
    // Monitor DDE session manager lock screen signal
    QDBusConnection::sessionBus().connect(
        "org.deepin.dde.SessionManager",
        "/org/deepin/dde/SessionManager",
        "org.deepin.dde.SessionManager",
        "Locked",
        this, [this]() {
        m_isLocked = true;
        if (m_lockscreenMode == "static") {
            m_client->pause();
        }
        emit sessionLocked();
    });

    QDBusConnection::sessionBus().connect(
        "org.deepin.dde.SessionManager",
        "/org/deepin/dde/SessionManager",
        "org.deepin.dde.SessionManager",
        "Unlocked",
        this, [this]() {
        m_isLocked = false;
        m_client->resume();
        emit sessionUnlocked();
    });

    qDebug() << "Lock screen signals connected";
}

void DDEEventBridge::connectFullscreen() {
    // Poll for fullscreen windows every 2 seconds
    m_fullscreenTimer.start(2000);
    qDebug() << "Fullscreen detection started (polling)";
}

void DDEEventBridge::setLockscreenMode(const QString& mode) {
    if (m_lockscreenMode != mode) {
        m_lockscreenMode = mode;
        emit lockscreenModeChanged(mode);
    }
}

void DDEEventBridge::setFullscreenIgnoreList(const QStringList& list) {
    m_fullscreenIgnoreList = list;
}

void DDEEventBridge::onWorkspaceSwitched(int from, int to) {
    m_client->switchWorkspace(to);
    emit workspaceSwitched(from, to);
}

void DDEEventBridge::onWorkspaceBackgroundChanged(int workspace, const QString& path) {
    // Background changed via DDE settings, sync to engine
    m_client->setWorkspaceWallpaper(workspace, "DP-1", path);
}

void DDEEventBridge::checkFullscreen() {
    // Use xdotool or X11 properties to detect fullscreen windows
    // This is a simplified implementation using QDBus to query DDE
    QDBusInterface iface("org.deepin.dde.SessionManager",
                         "/org/deepin/dde/SessionManager",
                         "org.deepin.dde.SessionManager");
    if (!iface.isValid()) return;

    // Check if there's a fullscreen application
    // For now, we rely on the engine's built-in fullscreen detection
    // A more complete implementation would use X11 _NET_WM_STATE_FULLSCREEN
}
