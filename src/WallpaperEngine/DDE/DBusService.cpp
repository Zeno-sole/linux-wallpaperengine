#include "DBusService.h"
#include "WorkspaceManager.h"
#include "MonitorTracker.h"
#include "WallpaperEngine/Application/WallpaperApplication.h"
#include "WallpaperEngine/Logging/Log.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QFile>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <algorithm>
#include <csignal>
#include <unistd.h>

namespace WallpaperEngine::DDE {

static const QString DBUS_SERVICE = "org.deepin.wallpaperengine";
static const QString DBUS_PATH = "/org/deepin/wallpaperengine";

DBusService::DBusService(WallpaperEngine::Application::WallpaperApplication* app,
                         QObject* parent)
    : QObject(parent), m_app(app) {}

DBusService::~DBusService() {
    stopAllEngineProcesses();
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

QString DBusService::findEngineBinary() {
    // Try same directory as current binary first
    QString appDir = QCoreApplication::applicationDirPath();
    QString localPath = appDir + "/linux-wallpaperengine";
    if (QFile::exists(localPath)) {
        return localPath;
    }
    // Fall back to PATH
    QString path = QStandardPaths::findExecutable("linux-wallpaperengine");
    if (!path.isEmpty()) {
        return path;
    }
    return {};
}

void DBusService::stopEngineProcess(const QString& monitor) {
    auto it = m_engineProcesses.find(monitor);
    if (it != m_engineProcesses.end()) {
        QProcess* proc = it.value();
        if (proc->state() != QProcess::NotRunning) {
            sLog.out("Stopping engine for monitor: ", monitor.toStdString());
            proc->terminate();
            if (!proc->waitForFinished(3000)) {
                proc->kill();
                proc->waitForFinished(1000);
            }
        }
        proc->deleteLater();
        m_engineProcesses.erase(it);
    }
}

void DBusService::stopAllEngineProcesses() {
    for (auto it = m_engineProcesses.begin(); it != m_engineProcesses.end(); ++it) {
        QProcess* proc = it.value();
        if (proc->state() != QProcess::NotRunning) {
            proc->terminate();
            if (!proc->waitForFinished(3000)) {
                proc->kill();
            }
        }
        proc->deleteLater();
    }
    m_engineProcesses.clear();
}

void DBusService::SetWallpaper(const QString& monitor, const QString& wallpaperPath) {
    // Save to config
    if (m_workspaceManager) {
        m_workspaceManager->setWallpaper(m_workspaceManager->currentWorkspace(),
                                         monitor.toStdString(),
                                         wallpaperPath.toStdString());
    }

    // Stop existing engine for this monitor
    stopEngineProcess(monitor);

    // Find engine binary
    QString engineBin = findEngineBinary();
    if (engineBin.isEmpty()) {
        sLog.error("Cannot find linux-wallpaperengine binary");
        emit RenderingError(monitor, "Engine binary not found");
        return;
    }

    // Spawn engine subprocess
    QProcess* proc = new QProcess(this);

    // Ensure X11 environment for the engine
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (!env.contains("DISPLAY")) {
        env.insert("DISPLAY", ":0");
    }
    if (!env.contains("XDG_SESSION_TYPE") || env.value("XDG_SESSION_TYPE") == "tty") {
        env.insert("XDG_SESSION_TYPE", "x11");
    }
    proc->setProcessEnvironment(env);

    connect(proc, &QProcess::readyReadStandardOutput, this, [this, monitor, proc]() {
        QString output = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        if (!output.isEmpty()) {
            sLog.out("[engine:", monitor.toStdString(), "] ", output.toStdString());
        }
    });
    connect(proc, &QProcess::readyReadStandardError, this, [this, monitor, proc]() {
        QString output = QString::fromUtf8(proc->readAllStandardError()).trimmed();
        if (!output.isEmpty()) {
            sLog.error("[engine:", monitor.toStdString(), "] ", output.toStdString());
        }
    });
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, monitor](int exitCode, QProcess::ExitStatus status) {
        if (status == QProcess::CrashExit) {
            sLog.error("Engine crashed for monitor: ", monitor.toStdString());
            emit RenderingError(monitor, "Engine process crashed");
        } else if (exitCode != 0) {
            sLog.error("Engine exited with code ", exitCode, " for monitor: ", monitor.toStdString());
        }
    });

    QStringList args;
    args << "--screen-root" << monitor
         << "--bg" << wallpaperPath
         << "--fps" << QString::number(m_fps)
         << "--volume" << QString::number(m_volume);

    sLog.out("Starting engine: ", engineBin.toStdString(), " ", args.join(" ").toStdString());
    proc->start(engineBin, args);

    m_engineProcesses[monitor] = proc;
    emit WallpaperChanged(monitor, wallpaperPath);
    emit WallpaperLoaded(monitor, true, {});
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
    stopEngineProcess(monitor);
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

    // Restart engines for the new workspace
    if (m_workspaceManager) {
        auto wallpapers = m_workspaceManager->getWorkspaceWallpapers(workspace);
        // Stop all current engines
        stopAllEngineProcesses();
        // Start engines for new workspace wallpapers
        for (const auto& [monitor, path] : wallpapers) {
            if (!path.empty()) {
                SetWallpaper(QString::fromStdString(monitor),
                             QString::fromStdString(path.string()));
            }
        }
    }

    emit WorkspaceChanged(oldWs, workspace);
}

void DBusService::Pause() {
    m_isPaused = true;
    for (auto* proc : m_engineProcesses) {
        if (proc->state() == QProcess::Running && proc->processId() > 0) {
            ::kill(static_cast<pid_t>(proc->processId()), SIGSTOP);
        }
    }
    sLog.out("Paused all engines");
}

void DBusService::Resume() {
    m_isPaused = false;
    for (auto* proc : m_engineProcesses) {
        if (proc->state() == QProcess::Running && proc->processId() > 0) {
            ::kill(static_cast<pid_t>(proc->processId()), SIGCONT);
        }
    }
    sLog.out("Resumed all engines");
}

void DBusService::NextWallpaper(const QString& monitor) {
    // TODO: implement playlist next
}

void DBusService::SetVolume(int volume) {
    m_volume = std::clamp(volume, 0, 100);
}

void DBusService::SetFPS(int fps) {
    m_fps = std::clamp(fps, 1, 60);
    sLog.out("FPS set to ", m_fps, " (will apply to new wallpapers)");
}

QString DBusService::GetStatus() {
    if (m_isPaused) return "paused";
    if (!m_engineProcesses.isEmpty()) return "running";
    return "idle";
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
    // Reload config and restart all engines
    if (m_workspaceManager) {
        m_workspaceManager->loadFromConfig();
        int ws = m_workspaceManager->currentWorkspace();
        auto wallpapers = m_workspaceManager->getWorkspaceWallpapers(ws);
        stopAllEngineProcesses();
        for (const auto& [monitor, path] : wallpapers) {
            if (!path.empty()) {
                SetWallpaper(QString::fromStdString(monitor),
                             QString::fromStdString(path.string()));
            }
        }
    }
}

void DBusService::Quit() {
    stopAllEngineProcesses();
    QCoreApplication::quit();
}

} // namespace WallpaperEngine::DDE
