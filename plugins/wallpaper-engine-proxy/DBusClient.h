#pragma once

#include <QObject>
#include <QDBusInterface>
#include <QDBusPendingReply>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class DBusClient : public QObject {
    Q_OBJECT
public:
    explicit DBusClient(QObject* parent = nullptr);

    bool isEngineRunning();

    // Wallpaper control
    void setWallpaper(const QString& monitor, const QString& path);
    QString getWallpaper(const QString& monitor);
    void clearWallpaper(const QString& monitor);

    // Workspace
    void setWorkspaceWallpaper(int workspace, const QString& monitor, const QString& path);
    QString getWorkspaceWallpaper(int workspace, const QString& monitor);
    void switchWorkspace(int workspace);

    // Playback control
    void pause();
    void resume();
    void nextWallpaper(const QString& monitor);
    void setVolume(int volume);
    void setFPS(int fps);

    // Status
    QString getStatus();
    QVariantMap getLoadedWallpapers();
    QStringList getSupportedTypes();
    QVariantMap getWindowGeometry(const QString& monitor);

    // Lifecycle
    void reload();
    void quit();

signals:
    void wallpaperChanged(const QString& monitor, const QString& path);
    void wallpaperLoaded(const QString& monitor, bool success, const QString& errorMsg);
    void workspaceChanged(int oldWorkspace, int newWorkspace);
    void renderingError(const QString& monitor, const QString& error);
    void engineDisconnected();

private:
    QDBusInterface* m_iface = nullptr;
    void connectToEngine();
    void onServiceRegistered(const QString& serviceName);
    void onServiceUnregistered(const QString& serviceName);
};
