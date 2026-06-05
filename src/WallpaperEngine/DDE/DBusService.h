#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <memory>

namespace WallpaperEngine::Application {
class WallpaperApplication;
}

namespace WallpaperEngine::DDE {

class WorkspaceManager;
class MonitorTracker;

class DBusService : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.wallpaperengine.Controller")
public:
    explicit DBusService(WallpaperEngine::Application::WallpaperApplication* app = nullptr,
                         QObject* parent = nullptr);
    ~DBusService() override;

    bool registerService();
    void unregisterService();

    void setWorkspaceManager(WorkspaceManager* mgr);
    void setMonitorTracker(MonitorTracker* tracker);

public slots:
    // 壁纸控制
    void SetWallpaper(const QString& monitor, const QString& wallpaperPath);
    QString GetWallpaper(const QString& monitor);
    void ClearWallpaper(const QString& monitor);

    // 工作区
    void SetWorkspaceWallpaper(int workspace, const QString& monitor, const QString& wallpaperPath);
    QString GetWorkspaceWallpaper(int workspace, const QString& monitor);
    void SwitchWorkspace(int workspace);

    // 播放控制
    void Pause();
    void Resume();
    void NextWallpaper(const QString& monitor);
    void SetVolume(int volume);
    void SetFPS(int fps);

    // 状态查询
    QString GetStatus();
    QVariantMap GetLoadedWallpapers();
    QStringList GetSupportedTypes();

    // 窗口几何信息
    QVariantMap GetWindowGeometry(const QString& monitor);

    // 生命周期
    void Reload();
    void Quit();

signals:
    void WallpaperChanged(const QString& monitor, const QString& wallpaperPath);
    void WallpaperLoaded(const QString& monitor, bool success, const QString& errorMsg);
    void WorkspaceChanged(int oldWorkspace, int newWorkspace);
    void RenderingError(const QString& monitor, const QString& error);

private:
    void connectWorkspaceSignals();

    WallpaperEngine::Application::WallpaperApplication* m_app = nullptr;
    WorkspaceManager* m_workspaceManager = nullptr;
    MonitorTracker* m_monitorTracker = nullptr;
    bool m_isPaused = false;
    int m_volume = 30;
    int m_fps = 30;
};

} // namespace WallpaperEngine::DDE
