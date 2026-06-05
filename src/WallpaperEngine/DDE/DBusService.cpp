#include "DBusService.h"
#include "WorkspaceManager.h"
#include "MonitorTracker.h"
#include "WallpaperEngine/Application/WallpaperApplication.h"
#include "WallpaperEngine/Logging/Log.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <algorithm>

namespace WallpaperEngine::DDE {

static const QString DBUS_SERVICE = "org.deepin.wallpaperengine";
static const QString DBUS_PATH = "/org/deepin/wallpaperengine";

DBusService::DBusService(WallpaperEngine::Application::WallpaperApplication& app,
                         QObject* parent)
    : QObject(parent), m_app(app) {}

DBusService::~DBusService() {
    unregisterService();
}

bool DBusService::registerService() {
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerService(DBUS_SERVICE)) {
        sLog.error("Failed to register DBus service: ", bus.lastError().message().toStdString());
        return false;
    }
    if (!bus.registerObject(DBUS_PATH, this, QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals)) {
        sLog.error("Failed to register DBus object");
        return false;
    }
    sLog.out("DBus service registered: ", DBUS_SERVICE.toStdString());
    return true;
}

void DBusService::unregisterService() {
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.unregisterObject(DBUS_PATH);
    bus.unregisterService(DBUS_SERVICE);
}

void DBusService::setWorkspaceManager(WorkspaceManager* mgr) {
    m_workspaceManager = mgr;
    connectWorkspaceSignals();
}
void DBusService::setMonitorTracker(MonitorTracker* tracker) { m_monitorTracker = tracker; }

void DBusService::connectWorkspaceSignals() {
    if (!m_workspaceManager) return;
    connect(m_workspaceManager, &WorkspaceManager::workspaceSwitched,
            this, [this](int from, int to) {
        emit WorkspaceChanged(from, to);
    });
    connect(m_workspaceManager, &WorkspaceManager::wallpaperChanged,
            this, [this](int workspace, const std::string& monitor) {
        auto path = m_workspaceManager->getWallpaper(workspace, monitor);
        emit WallpaperChanged(QString::fromStdString(monitor),
                              QString::fromStdString(path.string()));
    });
}

void DBusService::SetWallpaper(const QString& monitor, const QString& wallpaperPath) {
    if (m_workspaceManager) {
        m_workspaceManager->setWallpaper(m_workspaceManager->currentWorkspace(),
                                         monitor.toStdString(),
                                         wallpaperPath.toStdString());
    }
    emit WallpaperChanged(monitor, wallpaperPath);
}

QString DBusService::GetWallpaper(const QString& monitor) {
    if (m_workspaceManager) {
        auto path = m_workspaceManager->getWallpaper(m_workspaceManager->currentWorkspace(),
                                                      monitor.toStdString());
        return QString::fromStdString(path.string());
    }
    return {};
}

void DBusService::ClearWallpaper(const QString& monitor) {
    if (m_workspaceManager) {
        m_workspaceManager->clearWallpaper(m_workspaceManager->currentWorkspace(),
                                           monitor.toStdString());
    }
}

void DBusService::SetWorkspaceWallpaper(int workspace, const QString& monitor, const QString& wallpaperPath) {
    if (m_workspaceManager) {
        m_workspaceManager->setWallpaper(workspace, monitor.toStdString(), wallpaperPath.toStdString());
    }
}

QString DBusService::GetWorkspaceWallpaper(int workspace, const QString& monitor) {
    if (m_workspaceManager) {
        auto path = m_workspaceManager->getWallpaper(workspace, monitor.toStdString());
        return QString::fromStdString(path.string());
    }
    return {};
}

void DBusService::SwitchWorkspace(int workspace) {
    int oldWs = m_workspaceManager ? m_workspaceManager->currentWorkspace() : 0;
    if (m_workspaceManager) {
        m_workspaceManager->switchTo(workspace);
    }
    emit WorkspaceChanged(oldWs, workspace);
}

void DBusService::Pause() {
    m_isPaused = true;
}

void DBusService::Resume() {
    m_isPaused = false;
}

void DBusService::NextWallpaper(const QString& monitor) {
    // TODO: implement playlist next
}

void DBusService::SetVolume(int volume) {
    m_volume = std::clamp(volume, 0, 100);
    // TODO: apply to audio context when available
}

void DBusService::SetFPS(int fps) {
    m_fps = std::clamp(fps, 1, 60);
    // TODO: apply to render context when available
}

QString DBusService::GetStatus() {
    if (m_isPaused) return "paused";
    return "running";
}

QVariantMap DBusService::GetLoadedWallpapers() {
    QVariantMap result;
    if (m_workspaceManager) {
        int ws = m_workspaceManager->currentWorkspace();
        auto wallpapers = m_workspaceManager->getWorkspaceWallpapers(ws);
        for (const auto& [monitor, path] : wallpapers) {
            result[QString::fromStdString(monitor)] = QString::fromStdString(path.string());
        }
    }
    return result;
}

QStringList DBusService::GetSupportedTypes() {
    return {"scene", "video", "web", "image"};
}

QVariantMap DBusService::GetWindowGeometry(const QString& monitor) {
    QVariantMap result;
    // TODO: get window geometry from render engine
    result["x"] = 0;
    result["y"] = 0;
    result["width"] = 1920;
    result["height"] = 1080;
    return result;
}

void DBusService::Reload() {
    // TODO: reload config
}

void DBusService::Quit() {
    QCoreApplication::quit();
}

} // namespace WallpaperEngine::DDE
